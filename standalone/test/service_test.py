#!/usr/bin/env python3
"""Offline test of `tube_js serve`: HTTP requests against the service on
127.0.0.1 while every upstream request is answered from standalone/fixtures.

    python3 standalone/test/service_test.py [path/to/tube_js]

Never touches the network: a request without a canned answer fails inside the
service. SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
TOKEN = "test-token-0123456789abcdef"
CHANNEL = "UC" + "f" * 22
PLAYLIST = "PLfixture0001"
VIDEO, LIVE, GONE, SHORT = "fixtureVID1", "fixtureLIV1", "fixtureGONE", "fixtureSHT1"
MEDIA = "https://rr1---sn-fixture.googlevideo.com/videoplayback?expire=1999999999&c=VISIONOS&sig=FIXTURE&x=a%26b"
failures, passed = [], 0


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


OPENER = urllib.request.build_opener(NoRedirect)


def check(name, condition, detail=""):
    global passed
    if condition:
        passed += 1
    else:
        failures.append(f"{name}: {detail}"[:600])


def same(name, actual, expected):
    got, wanted = repr(actual), repr(expected)
    at = next((i for i, (a, b) in enumerate(zip(got, wanted)) if a != b), min(len(got), len(wanted)))
    check(name, actual == expected, f"differs at {at}: got …{got[max(0, at - 60):at + 200]} wanted …{wanted[max(0, at - 60):at + 120]}")


class Service:
    def __init__(self, binary, *options):
        self.state = tempfile.mkdtemp(prefix="tube_js-test-")
        environment = {**os.environ, "TUBE_JS_FIXTURES": str(ROOT / "fixtures"), "ASAN_OPTIONS": "detect_leaks=1:abort_on_error=0",
                       "UBSAN_OPTIONS": "print_stacktrace=1"}
        self.process = subprocess.Popen([binary, "serve", "--host", "127.0.0.1", "--port", "0", "--state-dir", self.state,
                                         *options], stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment, text=True)
        line = self.process.stdout.readline()
        if not line:
            raise SystemExit("the service did not start: " + self.process.stderr.read()[-2000:])
        self.base = json.loads(line)["listening"]
        self.port = int(self.base.rsplit(":", 1)[1])

    def call(self, method, path, body=None, *, token=TOKEN, headers=None, raw=False):
        url = path if path.startswith("http") else self.base + path
        data = body if isinstance(body, bytes) else None if body is None else json.dumps(body).encode()
        request = urllib.request.Request(url, method=method, data=data)
        if data is not None:
            request.add_header("Content-Type", "application/json")
        if token:
            request.add_header("Authorization", "Bearer " + token)
        for name, value in (headers or {}).items():
            request.add_header(name, value)
        try:
            with OPENER.open(request, timeout=30) as reply:
                payload, status, got = reply.read(), reply.status, dict(reply.headers.items())
        except urllib.error.HTTPError as error:
            payload, status, got = error.read(), error.code, dict(error.headers.items())
        got = {name.lower(): value for name, value in got.items()}
        if raw:
            return status, got, payload
        try:
            return status, got, json.loads(payload) if payload else None
        except ValueError:
            return status, got, payload

    def job(self, body, expect="done"):
        status, _, created = self.call("POST", "/v1/jobs", body)
        if status != 200:
            return status, created
        for _ in range(400):
            status, _, result = self.call("GET", "/v1/jobs/" + created["job"])
            if status != 200 or result.get("state") != "pending":
                break
            time.sleep(0.01)
        if status == 200 and result.get("state") == expect == "done":
            return 200, result["result"]
        return status, result

    def library(self, **request):
        return self.job({"action": "library", "library": request})

    def stop(self):
        self.process.send_signal(signal.SIGTERM)
        try:
            _, errors = self.process.communicate(timeout=20)
        except subprocess.TimeoutExpired:
            self.process.kill()
            _, errors = self.process.communicate()
            errors += "\n(did not stop on SIGTERM)"
        return self.process.returncode, errors


def raw_exchange(port, payload, wait=5):
    """One connection, bytes out, everything that comes back."""
    with socket.create_connection(("127.0.0.1", port), timeout=wait) as connection:
        connection.sendall(payload)
        chunks = []
        try:
            while chunk := connection.recv(65536):
                chunks.append(chunk)
        except (socket.timeout, ConnectionError):
            pass
        return b"".join(chunks)


def row(identifier, title, subtitle="Fixture channel", channel=CHANNEL, duration=60, art=None, context=None):
    return {"id": identifier, "title": title, "subtitle": subtitle, "overview": "",
            "art_url": art or f"https://i.ytimg.com/vi/{identifier}/hq720.jpg", "channel_id": channel, "duration_sec": duration,
            "is_folder": False, "is_live": False, "is_short": False, "resume_pos_sec": 0, "played": False, "watch_percent": -1,
            "playback_context": context}


def basics(s):
    status, headers, health = s.call("GET", "/v1/health", token=None)
    same("health status", (status, headers.get("content-type")), (200, "application/json"))
    same("health fields", {k: v for k, v in health.items() if k != "features"},
         {"api_version": 1, "version": "1.000.000", "service": "ProsperoTube", "mode": "youtube", "sponsorblock": True,
          "sponsorblock_alt": False, "proxy": False, "embedded": True, "account_playback": False})
    features = health["features"]
    check("health offers what exists", {"search", "library", "dash", "captions", "storyboards", "channel_pages", "content_filters",
                                          "video_details", "translated_captions", "locale", "thumbnail_source", "dearrow", "device_auth"} <= set(features), features)
    check("health hides what does not", not {"relay", "voice", "dislikes", "proxy"} & set(features), features)

    same("no token", s.call("GET", "/v1/jobs/x", token=None)[::2],
         (401, {"error": {"code": "unauthorized", "message": "Pair the PS5 with this companion first."}}))
    same("wrong token", s.call("GET", "/v1/jobs/x", token=TOKEN + "x")[0], 401)
    same("token prefix", s.call("POST", "/v1/library", {"action": "home"}, token=TOKEN[:-1])[0], 401)
    same("unknown route", s.call("GET", "/v1/nothing")[::2], (404, {"detail": "Not Found"}))
    same("unknown route outside the API", s.call("GET", "/favicon.ico", token=None)[::2], (404, {"detail": "Not Found"}))
    same("wrong method", s.call("DELETE", "/v1/health")[::2], (405, {"detail": "Method Not Allowed"}))
    same("pairing is not needed", s.call("POST", "/v1/pair", {"code": "123456"}, token=None)[::2],
         (410, {"api_version": 1, "error": {"code": "pair_expired", "message": "The pairing code expired. Restart the companion to get a new code."}}))
    same("pairing code shape", s.call("POST", "/v1/pair", {"code": "12"}, token=None)[0], 422)
    same("dislikes are off", s.call("GET", f"/v1/dislikes/{VIDEO}")[::2], (200, {"api_version": 1, "enabled": False}))
    same("dislikes video id", s.call("GET", "/v1/dislikes/short")[2]["error"]["code"], "invalid_video")
    same("dislikes off: nothing is asked of Return YouTube Dislike",
         [r for r in s.call("GET", "/v1/_upstream")[2]["requests"] if "dislike" in r["key"]], [])
    # Sign-in is offered (standalone/test/account_test.py); here Google has no canned answer, so a start fails cleanly.
    for path, code, status in (("/v1/voice", "voice_unavailable", 503), ("/v1/auth/start", "auth_failed", 502)):
        got = s.call("POST", path, {})
        same(f"not offered {path}", (got[0], got[2]["error"]["code"]), (status, code))
    # The phone receiver is offered (standalone/test/remote_test.py); nothing exists before a start asks for it.
    got = s.call("POST", "/v1/remote", {"action": "poll", "session": "0" * 32})
    same("no phone receiver before a start", (got[0], got[2]["error"]["code"], "remote" in features), (404, "remote_session", True))


def bounds(s):
    reply = raw_exchange(s.port, b"POST /v1/jobs HTTP/1.1\r\nHost: x\r\nAuthorization: Bearer " + TOKEN.encode() +
                         b"\r\nContent-Length: 8193\r\n\r\n" + b"x" * 8193)
    check("body limit", reply.startswith(b"HTTP/1.1 413 "), reply[:60])
    reply = raw_exchange(s.port, b"GET /v1/health HTTP/1.1\r\nX-Padding: " + b"a" * 9000 + b"\r\n\r\n")
    check("header limit", reply.startswith(b"HTTP/1.1 431 "), reply[:60])
    reply = raw_exchange(s.port, b"POST /v1/jobs HTTP/1.1\r\nContent-Length: nine\r\n\r\n")
    check("bad length", reply.startswith(b"HTTP/1.1 400 "), reply[:60])
    reply = raw_exchange(s.port, b"POST /v1/jobs HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n")
    check("no chunked bodies", reply.startswith(b"HTTP/1.1 400 "), reply[:60])
    reply = raw_exchange(s.port, b"PUT /v1/health HTTP/1.1\r\n\r\n")
    check("unknown method", reply.startswith(b"HTTP/1.1 405 "), reply[:60])
    reply = raw_exchange(s.port, b"garbage\r\n\r\n")
    check("not HTTP", reply.startswith(b"HTTP/1.1 400 "), reply[:60])
    reply = raw_exchange(s.port, b"GET /v1/health HTTP/1.1\r\nHost: x\r\n\r\n")
    check("closes after one answer", b"Connection: close\r\n" in reply and reply.endswith(b"}"), reply[-80:])
    # Many clients at once: thumbnails and metadata are asked for in parallel.
    sockets = [socket.create_connection(("127.0.0.1", s.port), timeout=5) for _ in range(24)]
    for connection in sockets:
        connection.sendall(b"GET /v1/health HTTP/1.1\r\n\r\n")
    answers = [connection.recv(65536) for connection in sockets]
    for connection in sockets:
        connection.close()
    check("24 parallel connections", all(answer.startswith(b"HTTP/1.1 200 ") for answer in answers), answers[0][:40])
    same("invalid JSON", s.call("POST", "/v1/jobs", b"{not json")[0], 422)
    same("invalid job action", s.call("POST", "/v1/jobs", {"action": "delete"})[0], 422)
    same("invalid library field", s.call("POST", "/v1/library", {"action": "home", "nonsense": 1})[0], 422)
    detail = s.call("POST", "/v1/library", {"action": "search"})
    check("422 shape", detail[0] == 422 and isinstance(detail[2].get("detail"), list) and "msg" in detail[2]["detail"][0], detail[2])
    same("auth job without an operation", s.call("POST", "/v1/jobs", {"action": "auth"})[::2],
         (400, {"api_version": 1, "error": {"code": "invalid_job", "message": "An enabled sign-in service and operation are required."}}))
    same("resolve job needs playback", s.call("POST", "/v1/jobs", {"action": "resolve"})[2]["error"]["code"], "invalid_job")
    same("library job needs an operation", s.call("POST", "/v1/jobs", {"action": "library"})[2]["error"]["code"], "invalid_job")


def jobs_and_catalog(s):
    status, _, created = s.call("POST", "/v1/jobs", {"action": "search", "query": "canned"})
    same("job created", (status, sorted(created), created["state"], created["api_version"]), (200, ["api_version", "job", "state"], "pending", 1))
    check("job id shape", re.fullmatch(r"[A-Za-z0-9_-]{24}", created["job"]) is not None, created["job"])
    result = None
    for _ in range(400):
        status, _, result = s.call("GET", "/v1/jobs/" + created["job"])
        if result.get("state") != "pending":
            break
        time.sleep(0.01)
    same("job done", (status, result["api_version"], result["state"], sorted(result)), (200, 1, "done", ["api_version", "result", "state"]))
    page = result["result"]
    same("search page shape", (page["api_version"], page["page"], page["has_more"], len(page["items"]), list(page)),
         (1, 0, True, 24, ["api_version", "items", "has_more", "page"]))
    # Search rows carry the retained group the native side navigates within.
    group = page["items"][0]["playback_context"]["group"]
    check("search group", re.fullmatch(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}", group) is not None, group)
    same("search row 0", page["items"][0], row("jNQXAC9IVRw", "Me at the zoo", "jawed", "UC4QobU6STFB0P71PMvOGN5A", 19, context={"group": group, "index": 0}))
    same("search row 1 keeps a safe image", page["items"][1],
         row("AAAAAAAAAAA", "Second canned row", "Canned channel", "UC" + "a" * 22, 3723, "https://i.ytimg.com/vi/AAAAAAAAAAA/hqdefault.jpg",
             context={"group": group, "index": 1}))
    same("repeat poll", s.call("GET", "/v1/jobs/" + created["job"])[2], result)
    same("cancel", s.call("DELETE", "/v1/jobs/" + created["job"])[::2], (200, {"state": "cancelled"}))
    same("poll after cancel", s.call("GET", "/v1/jobs/" + created["job"])[::2],
         (410, {"api_version": 1, "error": {"code": "expired_job", "message": "This request expired. Retry from the current screen."}}))
    same("cancel unknown", s.call("DELETE", "/v1/jobs/unknown")[::2], (200, {"state": "cancelled"}))
    same("poll unknown", s.call("GET", "/v1/jobs/unknown")[0], 410)

    status, second = s.job({"action": "search", "query": "canned", "page": 1})
    same("search page 2", (status, second["page"], second["has_more"], [r["id"] for r in second["items"]]),
         (200, 1, False, [f"fixture{n:04d}" for n in range(24, 30)]))
    status, third = s.job({"action": "search", "query": "canned", "page": 2})
    same("search page 3 is empty", (status, third["items"], third["has_more"]), (200, [], False))
    status, _, direct = s.call("GET", "/v1/search?q=canned&page=1")
    same("direct search route", (status, direct), (200, second))
    same("direct search needs q", s.call("GET", "/v1/search")[0], 422)
    same("search by video id", s.job({"action": "search", "query": f"video:{VIDEO}"})[1]["items"],
         [row(VIDEO, "Fixture video", duration=635, art=f"https://i.ytimg.com/vi/{VIDEO}/hqdefault.jpg") | {
             "overview": "A fixture.\n0:00 Intro\n1:30 Middle part\n5:00 The end\n"}])
    same("search by link", s.job({"action": "search", "query": f"https://youtu.be/{VIDEO}"})[1]["items"][0]["id"], VIDEO)
    same("search by bad id", s.job({"action": "search", "query": "video:short"})[1],
         {"api_version": 1, "state": "failed", "error": {"code": "invalid_video", "message": "The YouTube video ID is invalid."}})
    same("blank query", s.job({"action": "search", "query": "  "})[1]["error"]["code"], "invalid_query")
    same("no query", s.job({"action": "search"})[1]["error"]["code"], "invalid_catalog")

    status, channel = s.job({"action": "catalog", "parent": f"channel:{CHANNEL}"})
    same("channel catalog", (status, channel["has_more"], channel["items"]),
         (200, False, [row(f"fixture{100 + n:04d}", f"Channel video {n}", context={"channel": CHANNEL, "index": n}) for n in range(3)]))
    status, playlist = s.job({"action": "catalog", "parent": f"playlist:{PLAYLIST}"})
    same("playlist catalog", (status, playlist["items"]),
         (200, [row(f"fixture{200 + n:04d}", f"Playlist video {n}", art=f"https://i.ytimg.com/vi/fixture{200 + n:04d}/hqdefault.jpg",
                    context={"playlist": PLAYLIST, "index": n}) for n in range(3)]))
    same("direct catalog route", s.call("GET", f"/v1/catalog?parent=playlist:{PLAYLIST}")[2], playlist)
    same("search by playlist link", s.job({"action": "search", "query": f"https://www.youtube.com/playlist?list={PLAYLIST}"})[1], playlist)
    same("invalid parent", s.job({"action": "catalog", "parent": "nonsense"})[1]["error"],
         {"code": "invalid_catalog", "message": "Choose search, a channel, or a YouTube playlist."})
    same("other profile", s.job({"action": "search", "query": "canned", "profile": "someone"})[1]["error"],
         {"code": "unknown_profile", "message": "Select an available profile."})
    middle = s.job({"action": "search", "query": "canned", "thumbnails": "middle"})[1]["items"]
    same("thumbnail source", [middle[0]["art_url"], middle[1]["art_url"]],
         ["https://i.ytimg.com/vi/jNQXAC9IVRw/hq2.jpg", "https://i.ytimg.com/vi/AAAAAAAAAAA/hq2.jpg"])
    branded = s.job({"action": "search", "query": "canned", "dearrow_titles": True, "dearrow_thumbnails": True})[1]["items"]
    same("DeArrow title", (branded[0].get("dearrow_title"), "dearrow_title" in branded[1]), ("The first video", False))
    art = branded[0].get("dearrow_art_url", "")
    check("DeArrow image address", re.fullmatch(re.escape(s.base) + r"/art/dearrow/[A-Za-z0-9_-]{32}", art) is not None, art)
    status, headers, image = s.call("GET", art, token=None, raw=True)
    same("DeArrow image", (status, headers.get("content-type"), headers.get("cache-control"), image[:3]),
         (200, "image/jpeg", "private, max-age=300", b"\xff\xd8\xff"))
    same("DeArrow unknown image", s.call("GET", "/art/dearrow/" + "a" * 32, token=None)[0], 404)


def library(s):
    status, page = s.library(action="search", query="canned")
    same("library search", (status, page["api_version"], page["kind"], len(page["items"]), page["has_more"], page["items"][0]["kind"],
                            page["items"][0]["id"]), (200, 1, "catalog", 24, True, "video", "jNQXAC9IVRw"))
    check("library cursor", re.fullmatch(r"[0-9a-f-]{36}", page["cursor"]) is not None, page["cursor"])
    status, more = s.library(action="continue", cursor=page["cursor"])
    same("library continuation", (status, len(more["items"]), more["has_more"], more["cursor"]), (200, 6, False, ""))
    same("continuation of nothing", s.library(action="continue", cursor="unknown")[1]["error"]["code"], "cursor_expired")
    status, direct = s.call("POST", "/v1/library", {"action": "search", "query": "canned"})[::2]
    same("direct library route", (status, direct["kind"], len(direct["items"])), (200, "catalog", 24))
    status, typed = s.library(action="search_suggestions", query="canned")
    same("completions of a typed text", (status, typed), (200, {"api_version": 1, "kind": "search_suggestions", "query": "canned",
         "suggestions": ["canned heat", "canned music for a quiet room", "canned laughter"]}))
    same("completions YouTube does not answer", s.library(action="search_suggestions", query="offline")[1]["error"]["code"], "upstream_format")
    status, shelf = s.library(action="topic_videos", section="music")
    same("a topic as one list of videos", (status, shelf["kind"], [r["id"] for r in shelf["items"]], shelf["has_more"], shelf["items"][0]["title"]),
         (200, "catalog", [f"fixture{200 + n:04d}" for n in range(6)], False, "Topic video 0"))
    same("a topic that is no shelf of Home", s.library(action="topic_videos", section="movies")[1]["error"]["code"], "invalid_catalog")
    status, opened = s.library(action="channel", channel=CHANNEL)
    same("channel page", (status, opened["channel"]["id"], opened["channel"]["title"], opened["channel"]["tab"],
                          [r["id"] for r in opened["items"]], {r["subtitle"] for r in opened["items"]}, {r["channel_id"] for r in opened["items"]},
                          opened["items"][0]["playback_context"]),
         (200, CHANNEL, "Fixture channel", "videos", [f"fixture{100 + n:04d}" for n in range(3)], {"Fixture channel"}, {CHANNEL},
          {"channel": CHANNEL, "index": 0}))
    status, listed = s.library(action="playlist", id=PLAYLIST)
    same("playlist page", (status, [r["playback_context"] for r in listed["items"]]),
         (200, [{"playlist": PLAYLIST, "index": n} for n in range(3)]))
    same("profiles", s.library(action="profiles")[1], {"api_version": 1, "profiles": [{"id": "guest", "name": "Guest", "account_configured": False}]})
    # Account-only actions answer exactly as the companion does without an account.
    same("account action from the adapter", s.call("POST", "/v1/library", {"action": "subscriptions"})[::2],
         (401, {"api_version": 1, "error": {"code": "account_required", "message": "Configure an account profile on the companion first."}}))
    same("account change", s.call("POST", "/v1/library", {"action": "like", "id": VIDEO})[2]["error"]["code"], "account_required")
    same("guarded account change", s.call("POST", "/v1/library", {"action": "watch_progress", "id": VIDEO, "session": "s1", "position_sec": 3})[::2],
         (401, {"api_version": 1, "error": {"code": "account_required", "message": "Select an account profile first."}}))
    same("guarded change, bad input", s.call("POST", "/v1/library", {"action": "watch_progress", "id": "short", "session": "s1"})[::2],
         (400, {"api_version": 1, "error": {"code": "invalid_progress", "message": "The playback report is invalid."}}))
    same("profile removal", s.call("POST", "/v1/library", {"action": "profile_remove"})[::2],
         (400, {"api_version": 1, "error": {"code": "account_required", "message": "Select an account profile first."}}))
    same("unknown profile", s.call("POST", "/v1/library", {"action": "home", "profile": "someone"})[::2],
         (404, {"api_version": 1, "error": {"code": "unknown_profile", "message": "Select an available profile."}}))
    same("failed library job", s.library(action="subscriptions")[1],
         {"api_version": 1, "state": "failed", "error": {"code": "account_required", "message": "Configure an account profile on the companion first."}})


def playback(s):
    status, details = s.job({"action": "details", "id": VIDEO})
    # Structured chapters of the watch page win over the timestamps in the description.
    chapters = [{"start_time": 0, "title": "Opening", "end_time": 120}, {"start_time": 120, "title": "Main part", "end_time": 500},
                {"start_time": 500, "title": "Credits", "end_time": 635}]
    same("details job", (status, sorted(details), details["item"]["id"], details["item"]["title"], details["item"]["duration_sec"], details["chapters"]),
         (200, ["chapters", "item"], VIDEO, "Fixture video", 635, chapters))
    status, _, video = s.call("GET", f"/v1/video/{VIDEO}")
    same("video route", (status, video["api_version"], video["item"], video["chapters"], video["caption_languages"]),
         (200, 1, details["item"], chapters, ["en"]))
    same("video route, bad id", s.call("GET", "/v1/video/short")[::2],
         (400, {"api_version": 1, "error": {"code": "invalid_video", "message": "Enter an eleven-character YouTube video ID."}}))

    status, play = s.job({"action": "resolve", "playback": {"id": VIDEO, "codec": "avc", "max_height": 1080}})
    same("resolve fields", (status, list(play)), (200, ["api_version", "id", "title", "channel", "channel_id", "channel_art_url", "views", "published",
                                                        "duration_sec", "expires_in", "choices", "captions", "chapters",
                                                        "paid_content_text", "loudness_db", "is_short", "storyboards",
                                                        "audio_tracks", "audio_track"]))
    token = re.fullmatch(re.escape(s.base) + r"/media/([A-Za-z0-9_-]{32})/d0\.mpd", play["choices"][0]["url"])
    check("manifest address", token is not None, play["choices"][0]["url"])
    prefix = f"{s.base}/media/{token[1]}"
    choice = lambda n, label, codec, width, height, fps, rate: {
        "url": f"{prefix}/d{n}.mpd", "label": label, "container": "dash", "video_codec": codec, "audio_codec": "aac", "width": width,
        "height": height, "fps": fps, "bitrate_bps": rate, "is_live": False, "video_range": "SDR", "audio_channels": 2}
    same("resolve result", play, {
        "api_version": 1, "id": VIDEO, "title": "Fixture video", "channel": "Fixture channel", "channel_id": CHANNEL,
        "channel_art_url": "", "views": "12345", "published": "", "duration_sec": 635, "expires_in": 1800,
        "choices": [choice(0, "1080p 60 fps H264 / AAC", "h264", 1920, 1080, 60, 3248000), choice(1, "720p 30 fps H264 / AAC", "h264", 1280, 720, 30, 1100000)],
        # Every resolve lists the automatic track and the machine translations of the human one, whatever the preferences.
        "captions": [{"language": "en", "name": "English", "url": f"{prefix}/c0.srt", "automatic": False, "translated": False},
                     {"language": "de", "name": "German (auto-generated)", "url": f"{prefix}/c1.srt", "automatic": True, "translated": False},
                     {"language": "fr-en", "name": "French", "url": f"{prefix}/c2.srt", "automatic": True, "translated": True, "target": "fr"},
                     {"language": "es-en", "name": "Spanish", "url": f"{prefix}/c3.srt", "automatic": True, "translated": True, "target": "es"}],
        "chapters": [{"title": "Opening", "start": 0, "end": 120}, {"title": "Main part", "start": 120, "end": 500}, {"title": "Credits", "start": 500, "end": 635}],
        "paid_content_text": "", "loudness_db": -3.5, "is_short": False,
        "storyboards": [{"width": 48, "height": 27, "columns": 10, "rows": 10, "count": 100, "sheets": 1, "interval_ms": 6350, "url": f"{prefix}/sb0-$M.jpg"},
                        {"width": 80, "height": 45, "columns": 10, "rows": 10, "count": 128, "sheets": 2, "interval_ms": 4961, "url": f"{prefix}/sb1-$M.jpg"},
                        {"width": 160, "height": 90, "columns": 5, "rows": 5, "count": 128, "sheets": 6, "interval_ms": 4961, "url": f"{prefix}/sb2-$M.jpg"}],
        "audio_tracks": [], "audio_track": ""})

    status, headers, manifest = s.call("GET", play["choices"][0]["url"], token=None, raw=True)
    escaped = MEDIA.replace("&", "&amp;")
    # The audio language is the spoken one YouTube names through the automatic caption track.
    track = lambda kind, mime, codecs, attributes, itag: (
        f'<AdaptationSet contentType="{kind}" mimeType="{mime}" codecs="{codecs}"{" lang=" + chr(34) + "de" + chr(34) if kind == "audio" else ""}>'
        f'<Representation id="{kind}" {attributes}>'
        f'<BaseURL>{escaped}&amp;itag={itag}</BaseURL><SegmentList duration="635000" timescale="1000">'
        f'<SegmentURL media="{escaped}&amp;itag={itag}" /></SegmentList></Representation></AdaptationSet>')
    same("manifest headers", (status, headers.get("content-type"), headers.get("cache-control")), (200, "application/dash+xml", "no-store"))
    same("manifest text", manifest.decode(),
         "<?xml version='1.0' encoding='utf-8'?>\n<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"static\" "
         "mediaPresentationDuration=\"PT635.000S\" minBufferTime=\"PT1S\" profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\">"
         "<Period duration=\"PT635.000S\">" + track("video", "video/mp4", "avc1.64002a", 'bandwidth="3248000" width="1920" height="1080"', 299)
         + track("audio", "audio/mp4", "mp4a.40.2", 'bandwidth="129500" audioSamplingRate="44100"', 140) + "</Period></MPD>")
    status, headers, empty = s.call("HEAD", play["choices"][0]["url"], token=None, raw=True)
    same("manifest HEAD", (status, headers.get("content-type"), empty), (200, "application/dash+xml", b""))
    status, headers, subtitles = s.call("GET", play["captions"][0]["url"], token=None, raw=True)
    same("caption file", (status, headers.get("content-type"), subtitles.decode()),
         (200, "application/x-subrip", "1\n00:00:01,200 --> 00:00:03,360\nAll right, so here we are\n\n2\n00:00:03,360 --> 00:00:05,000\nin front of the & fixture\n"))
    # An automatic track as YouTube writes it: a blank-looking first line that rolls up, word timings,
    # and a ten-millisecond cue repeating the finished line. Its translation is asked for with tlang.
    status, headers, spoken = s.call("GET", play["captions"][1]["url"], token=None, raw=True)
    same("automatic caption file", (status, spoken.decode()),
         (200, "1\n00:00:00,080 --> 00:00:02,550\nhallo und willkommen\n\n2\n00:00:02,560 --> 00:00:04,390\n"
               "hallo und willkommen\nzu diesem Video\n"))
    status, headers, french = s.call("GET", play["captions"][2]["url"], token=None, raw=True)
    same("translated caption file", (status, french.decode().splitlines()[2]), (200, "Bonjour et bienvenue"))
    status, headers, sheet = s.call("GET", play["storyboards"][1]["url"].replace("$M", "0"), token=None, raw=True)
    same("storyboard sheet", (status, headers.get("content-type"), sheet[:3]), (200, "image/jpeg", b"\xff\xd8\xff"))
    same("storyboard sheet upstream failure", s.call("GET", f"{prefix}/sb1-1.jpg", token=None)[2]["error"]["code"], "media_network")
    same("missing media", s.call("GET", f"{prefix}/nothing", token=None)[::2],
         (404, {"api_version": 1, "error": {"code": "missing_media", "message": "The playback resource does not exist."}}))
    same("bad range", s.call("GET", f"{prefix}/c0.srt", token=None, headers={"Range": "bytes=1-2,4-5"})[2]["error"]["code"], "invalid_range")
    same("unknown session", s.call("GET", f"{s.base}/media/{'x' * 32}/d0.mpd", token=None)[::2],
         (410, {"api_version": 1, "error": {"code": "expired_session", "message": "Playback information expired. Select the video again to refresh it."}}))

    status, _, direct = s.call("POST", "/v1/resolve", {"id": VIDEO, "codec": "vp9", "max_height": 2160, "automatic_captions": True, "caption_language": "de"})
    same("direct resolve, VP9 to 2160p", (status, [(c["label"], c["container"], c["bitrate_bps"], c["video_range"]) for c in direct["choices"]]),
         (200, [("2160p 60 fps VP9 / AAC", "dash", 17174000, "SDR"),
                ("2160p 30 fps VP9 / AAC HDR10", "dash", 18500000, "HDR10"),
                ("1080p 60 fps VP9 / AAC", "dash", 2127000, "SDR")]))
    same("captions, human then automatic then translations", [(c["language"], c["name"], c["automatic"], c["translated"]) for c in direct["captions"]],
         [("en", "English", False, False), ("de", "German (auto-generated)", True, False), ("fr-en", "French", True, True), ("es-en", "Spanish", True, True)])
    webm = s.call("GET", direct["choices"][0]["url"], token=None, raw=True)[2].decode()
    check("WebM video with MP4 audio", 'mimeType="video/webm" codecs="vp9"' in webm and 'mimeType="audio/mp4" codecs="mp4a.40.2"' in webm and "itag=315" in webm, webm[:300])
    translated = s.job({"action": "resolve", "playback": {"id": VIDEO, "translated_captions": True}})[1]["captions"]
    same("translated captions", [(c["language"], c["translated"]) for c in translated],
         [("en", False), ("de", False), ("fr-en", True), ("es-en", True)])
    # Several audio tracks and more streams than one resolve offers: every resolution keeps a stream for the
    # player's quality choice, best first; YouTube's default audio track plays unless another is asked for.
    status, multi = s.job({"action": "resolve", "playback": {"id": "fixtureMLT1", "max_height": 2160}})
    same("every resolution in the choices", (status, [(c["height"], c["video_codec"]) for c in multi["choices"]]),
         (200, [(2160, "vp9"), (2160, "av1"), (1440, "vp9"), (1440, "av1"), (1080, "h264"), (1080, "vp9"), (1080, "av1"),
                (720, "h264"), (720, "vp9"), (720, "av1"), (480, "h264"), (480, "vp9"), (480, "av1"), (360, "h264"),
                (240, "h264"), (144, "h264")]))
    same("audio tracks, the original first", (multi["audio_track"], multi["audio_tracks"]), ("en-US.4", [
        {"id": "en-US.4", "language": "en-US", "name": "English (US) original", "original": True, "default": True, "descriptive": False, "dubbed": False},
        {"id": "en-US.2", "language": "en-US", "name": "English (US) audio description", "original": False, "default": False, "descriptive": True, "dubbed": False},
        {"id": "de.3", "language": "de", "name": "German", "original": False, "default": False, "descriptive": False, "dubbed": True}]))
    same("automatic captions and their translations without any preference", [(c["language"], c["automatic"], c["translated"]) for c in multi["captions"]],
         [("en", True, False), ("fr-en", True, True), ("de-en", True, True)])
    status, dub = s.job({"action": "resolve", "playback": {"id": "fixtureMLT1", "max_height": 2160, "audio_track": "de.3"}})
    dubbed = s.call("GET", dub["choices"][0]["url"], token=None, raw=True)[2].decode()
    same("the audio track asked for", (status, dub["audio_track"], len(dub["audio_tracks"]), 'lang="de"' in dubbed), (200, "de.3", 3, True))
    same("the content language picks its dub", s.job({"action": "resolve", "playback": {"id": "fixtureMLT1", "hl": "de"}})[1]["audio_track"], "de.3")
    same("never described audio by language", s.job({"action": "resolve", "playback": {"id": "fixtureMLT1", "hl": "en"}})[1]["audio_track"], "en-US.4")
    same("an unknown audio track keeps the default", s.job({"action": "resolve", "playback": {"id": "fixtureMLT1", "audio_track": "xx.9"}})[1]["audio_track"], "en-US.4")
    same("bad audio track", s.call("POST", "/v1/resolve", {"id": "fixtureMLT1", "audio_track": "a b"})[0], 422)
    same("frame-rate limit", s.job({"action": "resolve", "playback": {"id": VIDEO, "max_fps": 30}})[1]["choices"][0]["label"], "720p 30 fps H264 / AAC")
    same("codec that is not offered", s.job({"action": "resolve", "playback": {"id": VIDEO, "codec": "hevc"}})[1]["error"]["code"], "unsupported_media")
    same("unsupported media status", s.call("POST", "/v1/resolve", {"id": VIDEO, "codec": "hevc"})[0], 422)
    same("removed video", s.call("POST", "/v1/resolve", {"id": GONE})[::2],
         (404, {"api_version": 1, "error": {"code": "video_unavailable", "message": "This YouTube video is unavailable or has been removed. Choose another video."}}))
    # Links that the media server refuses are found at resolve time (three tries), not by the player.
    began = time.time()
    same("refused links", s.call("POST", "/v1/resolve", {"id": "fixtureDENY"})[::2],
         (502, {"api_version": 1, "error": {"code": "upstream_media", "message": "The media server could not provide this resource."}}))
    check("refused links are retried with pauses", 3 < time.time() - began < 15, time.time() - began)
    # Every deep range of the first session's links refused, a new session's served.
    s.call("GET", "/v1/_upstream?clear=1")
    status, _, renewed = s.call("POST", "/v1/resolve", {"id": "fxtSESSION1", "codec": "vp9", "max_height": 2160})
    same("refused media plays from a new session", (status, renewed.get("choices", [{}])[0].get("label")), (200, "2160p 60 fps VP9 / AAC"))
    tags = [row["tag"] for row in s.call("GET", "/v1/_upstream?clear=1")[2]["requests"] if row["tag"].startswith(("session-", "visitor"))]
    same("a new visitor identity is fetched before the second round", tags,
         ["session-bad", "session-start", "session-deep"] * 3 + ["visitor", "session-good"])
    s.call("GET", "/v1/_upstream?clear=1")
    status, _, avc = s.call("POST", "/v1/resolve", {"id": "fxtRETRY001", "codec": "avc", "max_height": 1080})
    same("unselected 4K refusal does not affect AVC", (status, avc["choices"][0]["label"]),
         (200, "1080p 60 fps H264 / AAC"))
    status, _, recovered = s.call("POST", "/v1/resolve", {"id": "fxtRETRY001", "codec": "vp9", "max_height": 2160})
    same("deep-range refusal recovers with same format", (status, recovered["choices"][0]["label"]),
         (200, "2160p 60 fps VP9 / AAC"))
    tags = [row["tag"] for row in s.call("GET", "/v1/_upstream?clear=1")[2]["requests"] if row["tag"].startswith("retry-")]
    same("playback policies keep separate extraction cache and retry selected link", tags,
         ["retry-bad", "retry-bad", "retry-start", "retry-deep", "retry-good"])
    same("resolve, other profile", s.call("POST", "/v1/resolve", {"id": VIDEO, "profile": "someone"})[0], 404)
    same("resolve, bad request", s.call("POST", "/v1/resolve", {"id": VIDEO, "max_height": 99})[0], 422)

    # A manifest asked for again with ?refresh=1 is rebuilt from a new player answer, under the same names.
    status, headers, again = s.call("GET", play["choices"][0]["url"] + "?refresh=1", token=None, raw=True)
    same("refreshed manifest", (status, headers.get("content-type"), again == manifest), (200, "application/dash+xml", True))
    same("captions after a refresh", s.call("GET", play["captions"][0]["url"], token=None, raw=True)[0], 200)
    status, short = s.job({"action": "resolve", "playback": {"id": SHORT, "max_height": 2160}})
    same("short, paid promotion, description chapters out of range", (status, short["is_short"], short["paid_content_text"], short["chapters"], short["choices"][0]["label"]),
         (200, True, "Includes paid promotion", [], "1920p 30 fps H264 / AAC"))
    same("an ordinary video is no short", (play["is_short"], play["paid_content_text"]), (False, ""))
    status, live = s.job({"action": "resolve", "playback": {"id": LIVE, "max_height": 1080}})
    same("live choices", (status, [(c["label"], c["container"], c["is_live"], c["video_codec"], c["audio_codec"], c["bitrate_bps"]) for c in live["choices"]],
                          live["storyboards"], live["duration_sec"]),
         (200, [("1080p 60 fps H264 / AAC", "hls", True, "h264", "aac", 5000000), ("1080p 60 fps VP9 / AAC", "hls", True, "vp9", "aac", 4000000),
                ("720p 30 fps H264 / AAC", "hls", True, "h264", "aac", 2500000)], [], 0))
    status, headers, master = s.call("GET", live["choices"][0]["url"], token=None, raw=True)
    same("live playlist", (status, headers.get("content-type"), master.decode()), (200, "application/vnd.apple.mpegurl", "\n".join([
        "#EXTM3U", "#EXT-X-INDEPENDENT-SEGMENTS",
        '#EXT-X-MEDIA:URI="https://manifest.googlevideo.com/api/manifest/hls_playlist/itag/234/playlist/index.m3u8",TYPE=AUDIO,GROUP-ID="234",NAME="Default",DEFAULT=YES,AUTOSELECT=YES',
        '#EXT-X-STREAM-INF:BANDWIDTH=5000000,CODECS="avc1.640028,mp4a.40.2",RESOLUTION=1920x1080,FRAME-RATE=60,AUDIO="234",CLOSED-CAPTIONS=NONE',
        "https://manifest.googlevideo.com/api/manifest/hls_playlist/itag/270/playlist/index.m3u8", ""])))


def sponsor(s):
    same("sponsor segments", s.call("GET", f"/v1/sponsor/{VIDEO}")[::2], (200, {"segments": [{"start": 12.5, "end": 40, "category": "sponsor"}], "enabled": True}))
    same("sponsor categories", s.call("GET", f"/v1/sponsor/{VIDEO}?categories=intro,poi_highlight")[2]["segments"],
         [{"start": 0, "end": 5, "category": "intro"}, {"start": 60, "end": 60, "category": "poi_highlight"}])
    same("sponsor without an answer", s.call("GET", "/v1/sponsor/jNQXAC9IVRw")[::2], (200, {"segments": [], "enabled": True}))
    same("sponsor category refused", s.call("GET", f"/v1/sponsor/{VIDEO}?categories=sponsor,nonsense")[::2],
         (400, {"api_version": 1, "error": {"code": "invalid_categories", "message": "An unsupported SponsorBlock category was requested."}}))
    same("sponsor video id", s.call("GET", "/v1/sponsor/short")[0], 400)
    # No host named a server and the request names none: `alt` alone stays with the default one.
    same("sponsor alt without a server", s.call("GET", f"/v1/sponsor/{VIDEO}?alt=true")[2]["segments"], [{"start": 12.5, "end": 40, "category": "sponsor"}])
    named = "server=https%3A%2F%2Fsb.example.org"
    same("sponsor, the alternative server a request names", s.call("GET", f"/v1/sponsor/{VIDEO}?alt=true&{named}")[::2],
         (200, {"segments": [{"start": 3, "end": 9, "category": "sponsor"}], "enabled": True}))
    same("a named server waits for alt", s.call("GET", f"/v1/sponsor/{VIDEO}?{named}")[2]["segments"], [{"start": 12.5, "end": 40, "category": "sponsor"}])
    for bad in ("http%3A%2F%2Fsb.example.org", "https%3A%2F%2Fsb.example.org%3A8443", "https%3A%2F%2Fuser%40sb.example.org",
                "https%3A%2F%2Fsb.example.org%2Fpath", "https%3A%2F%2Fsb.example.org%3Fq%3D1", "sb.example.org", ""):
        same(f"sponsor server refused: {bad}", s.call("GET", f"/v1/sponsor/{VIDEO}?alt=true&server={bad}")[::2],
             (400, {"api_version": 1, "error": {"code": "invalid_server", "message": "The SponsorBlock server must be an https:// origin."}}))
    same("the alternative server was asked once", [r["key"] for r in s.call("GET", "/v1/_upstream")[2]["requests"] if r["tag"] == "sponsor-alt"],
         ["GET sb.example.org/api/skipSegments/d2e4"])


def dislikes(binary):
    """Return YouTube Dislike behind its host option: strict answers, one upstream request per video, silence on failure."""
    s = Service(binary, "--token", TOKEN, "--return-youtube-dislike")
    try:
        asked = lambda: [r for r in s.call("GET", "/v1/_upstream")[2]["requests"] if r["key"] == "GET returnyoutubedislikeapi.com/votes"]
        features = s.call("GET", "/v1/health", token=None)[2]["features"]
        check("health lists dislikes once enabled", "dislikes" in features, features)
        same("dislikes need the token", s.call("GET", f"/v1/dislikes/{VIDEO}", token=None)[0], 401)
        same("dislikes video id, nothing asked", (s.call("GET", "/v1/dislikes/short")[0], asked()), (400, []))
        expected = {"api_version": 1, "enabled": True, "id": VIDEO, "available": True, "likes": 120, "dislikes": 7}
        same("dislike counts", s.call("GET", f"/v1/dislikes/{VIDEO}")[::2], (200, expected))
        same("dislike counts, asked again", s.call("GET", f"/v1/dislikes/{VIDEO}")[2], expected)
        same("one upstream request, without the bearer", asked(), [{"key": "GET returnyoutubedislikeapi.com/votes", "bearer": False, "tag": "dislikes"}])
        for name, other in (("another video's answer", LIVE), ("a count that is not written as an integer", SHORT), ("an answer over 64 KiB", "fixtureDENY"),
                            ("an upstream error", GONE), ("an answer that is not JSON", "jNQXAC9IVRw"), ("no answer", "AAAAAAAAAAA")):
            got = s.call("GET", f"/v1/dislikes/{other}", raw=True)
            same(f"dislikes unavailable: {name}", (got[0], json.loads(got[2])), (200, {"api_version": 1, "enabled": True, "id": other, "available": False}))
            check(f"nothing of the upstream answer is passed on: {name}", b"WEBVTT" not in got[2] and len(got[2]) < 200, got[2][:80])
        same("an unavailable answer is kept for a while too", (s.call("GET", f"/v1/dislikes/{GONE}")[2]["available"], len(asked())), (False, 7))
    finally:
        code, errors = s.stop()
    same("clean shutdown (dislikes)", (code, errors.strip()), (0, ""))


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build" / "tube_js")
    service = Service(binary, "--token", TOKEN, "--sponsorblock")
    try:
        for part in (basics, bounds, jobs_and_catalog, library, playback, sponsor):
            try:
                part(service)
            except Exception as error:  # a broken answer shape is a failure, not a crash of the test
                failures.append(f"{part.__name__}: {type(error).__name__}: {error}")
    finally:
        code, errors = service.stop()
    probe_lines = [line for line in errors.splitlines() if line.startswith("YouTube media probe: ")]
    # A refused video is asked again once with a new session: three tries each.
    same("refused media diagnostics: phases, retries and statuses", probe_lines, [
        *["YouTube media probe: phase=header attempt=1 status=403",
          "YouTube media probe: phase=header attempt=2 status=403",
          "YouTube media probe: phase=header attempt=3 status=403"] * 2,
        "YouTube media probe: phase=midpoint attempt=1 status=403",
        "YouTube media probe: phase=midpoint attempt=2 status=403",
        "YouTube media probe: phase=midpoint attempt=3 status=403",
        "YouTube media probe: phase=midpoint attempt=1 status=403",
    ])
    same("a new session after refused media, once per refused resolve", errors.count("YouTube playback session renewed after refused media"), 2)
    check("media probe diagnostics contain no identifiers or signed URLs", all(re.fullmatch(
        r"YouTube media probe: phase=(header|midpoint) attempt=[1-3] status=(401|403|410)", line)
        for line in probe_lines))
    expected_library_errors = [
        "YouTube operation failed: stage=device action=start category=TypeError",
        *["YouTube operation failed: stage=execute action=other category=LibraryError"] * 3,
        "YouTube operation failed: stage=execute action=subscriptions category=LibraryError",
        "YouTube operation failed: stage=execute action=other category=LibraryError",
        "YouTube operation failed: stage=execute action=home category=LibraryError",
        "YouTube operation failed: stage=execute action=subscriptions category=LibraryError",
    ]
    library_errors = [line for line in errors.splitlines() if line.startswith("YouTube operation failed: ")]
    same("expected fixture library diagnostics", library_errors, expected_library_errors)
    other_errors = "\n".join(line for line in errors.splitlines()
                             if line not in probe_lines and line not in expected_library_errors and
                             line != "YouTube playback session renewed after refused media")
    same("clean shutdown", (code, other_errors.strip()), (0, ""))

    # Defaults: a generated token kept private, SponsorBlock off, and a pairing code when the host asks for one.
    plain = Service(binary, "--pairing-code", "424242")
    try:
        path = Path(plain.state) / "token"
        token = path.read_text().strip()
        check("generated token", re.fullmatch(r"[0-9a-f]{64}", token) is not None and (path.stat().st_mode & 0o777) == 0o600, oct(path.stat().st_mode))
        check("port range", 20000 <= plain.port < 40000, plain.port)
        same("sponsor off", plain.call("GET", f"/v1/sponsor/{VIDEO}", token=token)[::2], (200, {"segments": [], "enabled": False}))
        same("health says so", plain.call("GET", "/v1/health", token=None)[2]["sponsorblock"], False)
        same("pair denied", plain.call("POST", "/v1/pair", {"code": "000000"}, token=None)[2]["error"]["code"], "pair_denied")
        same("pair accepted", plain.call("POST", "/v1/pair", {"code": "424242"}, token=None)[::2], (200, {"api_version": 1, "token": token}))
        for _ in range(3):
            plain.call("POST", "/v1/pair", {"code": "000000"}, token=None)
        same("pair rate limit", plain.call("POST", "/v1/pair", {"code": "424242"}, token=None)[0], 429)
    finally:
        code, errors = plain.stop()
    same("clean shutdown (defaults)", (code, errors.strip()), (0, ""))

    # Manifests with one byte-range segment per group of fragments, read from the file's index.
    ranged = Service(binary, "--token", TOKEN, "--ranged-manifest")
    try:
        play = ranged.call("POST", "/v1/resolve", {"id": VIDEO, "codec": "avc"})[2]
        same("ranged resolve keeps the record", (play["choices"][0]["label"], play["choices"][0]["container"]), ("1080p 60 fps H264 / AAC", "dash"))
        status, headers, manifest = ranged.call("GET", play["choices"][0]["url"], token=None, raw=True)
        manifest = manifest.decode()
        escaped = MEDIA.replace("&", "&amp;") + "&amp;itag=299"
        same("ranged manifest", (status, headers.get("content-type"), manifest.count("<SegmentURL "), manifest.count("<Initialization ")),
             (200, "application/dash+xml", 6, 2))
        check("ranged video track", f'<BaseURL>{escaped}</BaseURL><SegmentList timescale="1000" duration="6000">'
              f'<Initialization sourceURL="{escaped}" range="0-740" /><SegmentURL media="{escaped}" mediaRange="809-1808" />'
              f'<SegmentURL media="{escaped}" mediaRange="1809-3808" /><SegmentURL media="{escaped}" mediaRange="3809-5308" /></SegmentList>' in manifest, manifest[:900])
        same("ranged manifest is built once", ranged.call("GET", play["choices"][0]["url"], token=None, raw=True)[2].decode(), manifest)
    finally:
        code, errors = ranged.stop()
    same("clean shutdown (ranged)", (code, errors.strip()), (0, ""))
    dislikes(binary)

    for failure in failures:
        print("FAIL", failure)
    print(f"service test: {passed} checks passed, {len(failures)} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
