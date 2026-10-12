#!/usr/bin/env python3
"""Bounded Guest-only live check of a running companion.

Read-only: search, sections, channel, playlist, video details, comments,
resolution and the first bytes of media through the relay. It signs in to
nothing, changes nothing on YouTube, never contacts a console and never prints
the pairing token, relay capabilities or signed addresses. For an embedded
service (health says `embedded`) the media reads go to YouTube's own servers. About fifty
sequential requests with a pause between them.

  PYTHONPATH=companion .venv/bin/python scripts/probe_live_guest.py \
      --base http://127.0.0.1:8765 --code 123456 --token-file ~/.cache/prosperotube/probe-token
"""
import argparse
import html
import json
import os
from pathlib import Path
import re
import sys
import time
import urllib.error
import urllib.request
from urllib.parse import urlsplit

SECTIONS = ("music", "gaming", "news", "sports", "live", "movies", "kids")


class Client:
    def __init__(self, base, pause):
        self.base, self.pause, self.token, self.requests = base.rstrip("/"), pause, "", 0
        # An embedded service says so in its health answer: its manifests and playlists name
        # YouTube's media servers directly, so those addresses are read from there.
        self.embedded = False

    def call(self, method, path, body=None, *, headers=None, raw=False, limit=1 << 20):
        url = path if path.startswith("http") else self.base + path
        target = urlsplit(url)
        direct = (self.embedded and target.scheme == "https" and target.port is None and
                  (target.hostname or "").endswith(".googlevideo.com"))
        if target.netloc != urlsplit(self.base).netloc and not direct:
            raise ValueError("the probe only talks to the companion")
        request = urllib.request.Request(url, method=method, data=None if body is None else json.dumps(body).encode())
        if body is not None:
            request.add_header("Content-Type", "application/json")
        if self.token and urlsplit(url).path.startswith("/v1/"):
            request.add_header("Authorization", "Bearer " + self.token)
        for name, value in (headers or {}).items():
            request.add_header(name, value)
        self.requests += 1
        time.sleep(self.pause)
        try:
            with urllib.request.urlopen(request, timeout=60) as reply:
                data = reply.read(limit)
                return reply.status, {k.lower(): v for k, v in reply.headers.items()}, data if raw else json.loads(data)
        except urllib.error.HTTPError as error:
            data = error.read(4096)
            try:
                return error.code, {}, data if raw else json.loads(data)
            except ValueError:
                return error.code, {}, {}

    def job(self, body):
        status, _, created = self.call("POST", "/v1/jobs", body)
        if status != 200:
            raise Failure(f"job refused with HTTP {status}")
        for _ in range(120):
            time.sleep(0.5)
            self.requests += 1
            request = urllib.request.Request(f"{self.base}/v1/jobs/{created['job']}",
                                             headers={"Authorization": "Bearer " + self.token})
            with urllib.request.urlopen(request, timeout=30) as reply:
                result = json.loads(reply.read(4 << 20))
            if result.get("state") != "pending":
                if result.get("state") != "done":
                    raise Failure(str((result.get("error") or {}).get("code", result.get("state"))))
                return result["result"]
        raise Failure("job timeout")

    def library(self, **request):
        return self.job({"action": "library", "library": request})


class Failure(Exception):
    pass


def need(condition, note):
    if not condition:
        raise Failure(note)


def pair(client, args):
    path = Path(args.token_file).expanduser() if args.token_file else None
    if path and path.exists():
        client.token = path.read_text().strip()
        return "stored token"
    need(args.code, "give --code or a --token-file from an earlier run")
    status, _, reply = client.call("POST", "/v1/pair", {"code": args.code})
    need(status == 200 and isinstance(reply.get("token"), str), f"pairing refused with HTTP {status}")
    client.token = reply["token"]
    if path:
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(descriptor, "w") as handle:
            handle.write(client.token + "\n")
    return "paired"


def checks(client, args):
    state = {}

    def health():
        status, _, reply = client.call("GET", "/v1/health")
        need(status == 200 and reply.get("mode") == "youtube", "the companion is not in YouTube mode")
        need("library" in reply.get("features", []), "start the companion with --library-node")
        client.embedded = reply.get("embedded") is True
        return f"{len(reply['features'])} features"
    yield "health", health
    yield "pairing", lambda: pair(client, args)

    def search_job():
        page = client.job({"action": "search", "query": args.query})
        need(len(page["items"]) >= 5, "fewer than five results")
        more = client.job({"action": "search", "query": args.query, "page": 1})
        need(more["items"] and more["items"][0]["id"] != page["items"][0]["id"], "page 2 repeats page 1")
        return f"{len(page['items'])} + {len(more['items'])} rows"
    yield "search job, two pages", search_job

    def search_library():
        page = client.library(action="search", query=args.query)
        kinds = {row.get("kind") for row in page["items"]}
        need("video" in kinds and page["has_more"], "no video rows or no continuation")
        more = client.library(action="continue", cursor=page["cursor"])
        need(more["items"], "empty continuation")
        return f"{len(page['items'])} + {len(more['items'])} rows, kinds {sorted(kinds)}"
    yield "library search, continuation", search_library

    def search_filters():
        notes = []
        for name, options, test in (
                ("channel", {"type": "channel"}, lambda row: row.get("kind") == "channel"),
                ("playlist", {"type": "playlist"}, lambda row: row.get("kind") == "playlist"),
                ("shorts", {"type": "shorts"}, lambda row: row.get("is_short") is True),
                ("live", {"features": ["live"]}, lambda row: row.get("is_live") is True),
                ("long", {"duration": "long"}, lambda row: row.get("duration_sec", 0) > 1200),
                ("today+views", {"upload_date": "today", "order": "view_count"}, lambda row: row.get("kind") == "video")):
            rows = client.library(action="search", query=args.query, **options)["items"]
            good = sum(1 for row in rows if test(row))
            need(rows and good * 2 >= len(rows), f"{name}: {good} of {len(rows)} rows match")
            notes.append(f"{name} {good}/{len(rows)}")
            if name == "live":
                state["live"] = next(row["id"] for row in rows if test(row))
        return ", ".join(notes)
    yield "search filters", search_filters

    def suggestions():
        reply = client.library(action="search_suggestions", query=args.query[:6])
        need(reply["suggestions"], "no completions")
        first = client.library(action="search", query=args.query, first_playable=True)
        need(first["found"] and re.fullmatch(r"[A-Za-z0-9_-]{11}", first["item"]["id"]), "no first playable video")
        return f"{len(reply['suggestions'])} completions, first playable found"
    yield "search suggestions, first playable", suggestions

    def home():
        page = client.library(action="home")
        return f"{len(page['items'])} rows (YouTube gives a signed-out session without history an empty Home)"
    yield "home", home

    def section(name):
        def run():
            root = client.library(action="topics", section=name)
            need(root["items"] and all(row["is_folder"] for row in root["items"]), "no shelves")
            for shelf in root["items"][:3]:
                rows = client.library(action="topic_open", section=name, id=shelf["id"].split(":")[2])["items"]
                if rows and all(row.get("title") for row in rows):
                    return f"{len(root['items'])} shelves, first usable shelf has {len(rows)} titled rows"
            raise Failure("no shelf with titled rows among the first three")
        return run
    for name in SECTIONS:
        yield f"section {name}", section(name)

    def retired():
        notes = []
        for name in ("trending", "shorts"):
            try:
                notes.append(f"{name}: {len(client.library(action='topics', section=name)['items'])} rows")
            except Failure as error:
                need(str(error) == "catalog_unavailable", f"{name}: {error}")
                notes.append(f"{name}: unavailable upstream")
        return ", ".join(notes)
    yield "sections trending, shorts", retired

    def channel():
        page = client.library(action="channel", channel=args.channel)
        info = page["channel"]
        need(page["items"] and info["sorts"] == ["newest", "popular", "oldest"], "no videos or no order chips")
        need(all(row["channel_id"] == args.channel and row["subtitle"] == info["title"] for row in page["items"]),
             "rows do not name the channel")
        popular = client.library(action="channel", channel=args.channel, tab="videos", channel_sort="popular")
        need(popular["items"] and popular["items"][0]["id"] != page["items"][0]["id"], "popular order equals newest")
        notes = [f"videos {len(page['items'])}", "popular differs"]
        for tab in ("home", "shorts", "live", "playlists"):
            if tab in info["tabs"]:
                rows = client.library(action="channel", channel=args.channel, tab=tab)["items"]
                need(rows, f"{tab}: no rows")
                if tab == "shorts":
                    need(all(row.get("is_short") and row["art_url"] for row in rows), "shorts without flag or image")
                if tab == "playlists":
                    state["playlist"] = rows[0]["id"].split(":")[2]
                notes.append(f"{tab} {len(rows)}")
        found = client.library(action="channel_search", channel=args.channel, query=args.channel_query)["items"]
        need(found, "channel search found nothing")
        return ", ".join(notes + [f"search {len(found)}"])
    yield "channel tabs, order, search", channel

    def playlist():
        identity = args.playlist or state.get("playlist")
        need(identity, "no playlist to open")
        page = client.library(action="playlist", id=identity)
        need(page["items"] and page["items"][0]["playback_context"] == {"playlist": identity, "index": 0}, "no positions")
        window = client.library(action="playback_queue", playlist=identity, id=page["items"][0]["id"], index=0)
        need(window["video_ids"][0] == page["items"][0]["id"], "the window starts elsewhere")
        return f"{len(page['items'])} rows, window of {len(window['video_ids'])}"
    yield "playlist, playback window", playlist

    def video():
        details = client.library(action="video_details", id=args.video)
        need(details["title"] and details["channel"]["id"].startswith("UC"), "details without title or channel")
        description = client.library(action="description", id=args.video)
        need(description["title"] == details["title"], "description names another video")
        related = client.library(action="suggestions", id=args.video)["items"]
        following = client.library(action="playback_next", id=args.video)
        need(related and re.fullmatch(r"[A-Za-z0-9_-]{11}", following["next_video_id"]), "no related videos or no next video")
        return f"{len(related)} related rows, next video found"
    yield "video details, description, related", video

    def comments():
        top = client.library(action="comments", id=args.comments_video)
        newest = client.library(action="comments", id=args.comments_video, sort="newest")
        need(top["items"] and newest["items"], "no comments")
        thread = next((row for row in top["items"] if row["reply_cursor"]), None)
        need(thread, "no thread with replies on the first page")
        replies = client.library(action="replies", cursor=thread["reply_cursor"])["items"]
        need(replies and all(row["content"] for row in replies), "no replies")
        return f"top {len(top['items'])}, newest {len(newest['items'])}, replies {len(replies)}"
    yield "comments top, newest, replies", comments

    def resolve():
        notes = []
        for codec in ("avc", "vp9", "av1", "hevc"):
            try:
                choices = client.job({"action": "resolve", "playback": {"id": args.video, "codec": codec, "max_height": 2160}})["choices"]
                notes.append(f"{codec} {choices[0]['height']}p{choices[0]['fps']:g}")
            except Failure as error:
                notes.append(f"{codec} none ({error})")
        need(notes[0].startswith("avc ") and "none" not in notes[0], "no AVC stream")
        return ", ".join(notes)
    yield "resolve per codec", resolve

    def media():
        playback = client.job({"action": "resolve", "playback": {"id": args.video, "codec": "avc", "max_height": 1080}})
        state["playback"] = playback
        choice = playback["choices"][0]
        need(choice["container"] == "dash", f"first choice is {choice['container']}")
        status, headers, manifest = client.call("GET", choice["url"], raw=True)
        need(status == 200 and "dash+xml" in headers.get("content-type", ""), f"manifest HTTP {status}")
        tracks = [html.unescape(track) for track in re.findall(r"<BaseURL>([^<]+)</BaseURL>", manifest.decode())]
        need(len(tracks) == 2, "the manifest does not name one video and one audio track")
        for track in tracks:
            status, headers, data = client.call("GET", track, headers={"Range": "bytes=0-65535"}, raw=True)
            need(status == 206 and len(data) == 65536 and data[4:8] == b"ftyp", f"ranged track read HTTP {status}")
        return f"{choice['label']}: manifest and 64 KiB of each track"
    yield "DASH manifest and ranged media", media

    def extras():
        playback = state.get("playback") or client.job({"action": "resolve", "playback": {"id": args.video}})
        notes = [f"loudness {playback['loudness_db']}", f"{len(playback['chapters'])} chapters"]
        if playback["storyboards"]:
            board = playback["storyboards"][0]
            status, headers, data = client.call("GET", board["url"].replace("$M", "0"), raw=True)
            need(status == 200 and headers.get("content-type", "").startswith("image/"), f"storyboard HTTP {status}")
            notes.append(f"storyboard sheet {headers['content-type']} {len(data)} bytes")
        captions = client.job({"action": "resolve", "playback": {"id": args.captions_video, "codec": "avc"}})
        need(captions["captions"], "no caption track")
        status, _, text = client.call("GET", captions["captions"][0]["url"], raw=True)
        need(status == 200 and b"-->" in text, f"caption HTTP {status}")
        notes.append(f"{len(captions['captions'])} caption tracks, SubRip read")
        return ", ".join(notes)
    yield "loudness, chapters, storyboard, captions", extras

    def live():
        identity = args.live_video or state.get("live")
        need(identity, "no live stream found by the search filter")
        playback = client.job({"action": "resolve", "playback": {"id": identity, "max_height": 1080}})
        choice = playback["choices"][0]
        need(choice["container"] == "hls" and choice["is_live"], "the live stream did not resolve to HLS")
        status, _, master = client.call("GET", choice["url"], raw=True)
        lines = [line for line in master.decode().splitlines() if line.startswith("http")]
        need(status == 200 and lines, "no variant playlist")
        status, _, variant = client.call("GET", lines[0], raw=True)
        segments = [line for line in variant.decode().splitlines() if line.startswith("http")]
        need(status == 200 and segments, "no segments")
        status, _, data = client.call("GET", segments[-1], raw=True, limit=262144)
        need(status == 200 and data, f"segment HTTP {status}")
        chat = "no chat"
        try:
            opened = client.library(action="live_chat", id=identity, chat_mode="live")
            chat = f"chat {len(opened['items'])} messages"
            client.library(action="chat_close", cursor=opened["cursor"])
        except Failure as error:
            chat = f"chat unavailable ({error})"
        return f"{choice['label']}, {len(segments)} segments, {chat}"
    yield "live HLS and chat", live

    def locale():
        english = client.library(action="topics", section="news")["items"]
        german = client.library(action="topics", section="news", hl="de", gl="DE")["items"]
        need(english and german and english[0]["title"] != german[0]["title"], "section titles did not change language")
        hidden = client.library(action="search", query=args.query, hide=["shorts_search", "mixes"])["items"]
        need(not any(row.get("is_short") for row in hidden), "a Short survived the filter")
        middle = client.library(action="search", query=args.query, thumbnails="middle")["items"]
        need(any("/hq2.jpg" in row.get("art_url", "") for row in middle), "no middle-frame thumbnail")
        return "German section titles, Shorts hidden, middle-frame thumbnails"
    yield "locale, content filters, thumbnail source", locale

    def third_party():
        status, _, sponsor = client.call("GET", f"/v1/sponsor/{args.video}")
        need(status == 200, f"sponsor HTTP {status}")
        page = client.job({"action": "search", "query": args.query, "dearrow_titles": True, "dearrow_thumbnails": True})
        branded = sum(1 for row in page["items"] if row.get("dearrow_title") or row.get("dearrow_art_url"))
        return (f"SponsorBlock {'on, ' + str(len(sponsor['segments'])) + ' segments' if sponsor['enabled'] else 'off'}, "
                f"DeArrow on {branded} of {len(page['items'])} rows")
    yield "SponsorBlock, DeArrow", third_party


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", default="http://127.0.0.1:8765", help="companion address")
    parser.add_argument("--code", help="six-digit pairing code printed by the companion")
    parser.add_argument("--token-file", help="private file that keeps the pairing token between runs")
    parser.add_argument("--pause", type=float, default=1.0, help="seconds between requests")
    parser.add_argument("--only", help="comma-separated words; run the checks whose name contains one")
    parser.add_argument("--query", default="lofi hip hop")
    parser.add_argument("--video", default="aqz-KE-bpKQ", help="public video with 1080p60")
    parser.add_argument("--captions-video", default="jNQXAC9IVRw", help="public video with human captions")
    parser.add_argument("--comments-video", default="aqz-KE-bpKQ")
    parser.add_argument("--channel", default="UCLA_DiR1FfKNvjuUpBHmylQ")
    parser.add_argument("--channel-query", default="moon")
    parser.add_argument("--playlist", help="default: the channel's first playlist")
    parser.add_argument("--live-video", help="default: the first live search result")
    args = parser.parse_args()
    if urlsplit(args.base).scheme not in ("http", "https") or args.pause < 0.2:
        parser.error("Give an http(s) companion address and a pause of at least 0.2 seconds.")
    client = Client(args.base, args.pause)
    wanted = [word for word in (args.only or "").split(",") if word]
    rows, failed = [], 0
    for name, run in checks(client, args):
        if wanted and name not in ("health", "pairing") and not any(word in name for word in wanted):
            continue
        try:
            rows.append((name, "pass", run()))
        except Failure as error:
            rows.append((name, "FAIL", str(error)))
            failed += 1
        except (KeyError, IndexError, TypeError, ValueError, OSError, StopIteration) as error:
            rows.append((name, "FAIL", f"unexpected response ({type(error).__name__})"))
            failed += 1
        if not client.token and name == "pairing":
            break
    width = max(len(name) for name, _, _ in rows)
    for name, result, note in rows:
        print(f"{name:<{width}}  {result}  {note}")
    print(f"{len(rows) - failed} of {len(rows)} checks passed, {client.requests} requests to the companion")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
