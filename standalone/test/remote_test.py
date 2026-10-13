#!/usr/bin/env python3
"""Offline test of the phone receiver in `tube_js serve` (POST /v1/remote): the
scenarios of tests/test_remote_lounge.py and tests/test_remote_playlist.py,
through the real service and its real HTTP client.

    python3 standalone/test/remote_test.py [path/to/tube_js] [scenario ...]

YouTube's Lounge service is played by a small server on 127.0.0.1 (the
service's `--lounge-base`, a test-only option): the same paths, forms and
frames, and a back channel that stays open and delivers its frames in small
pieces. No phone, account or network is involved; everything else upstream is
answered from canned files. SPDX-License-Identifier: GPL-3.0-or-later
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import queue
import re
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import parse_qs, urlsplit

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import service_test as base  # noqa: E402

check, same, TOKEN = base.check, base.same, base.TOKEN
ROOT = HERE.parent
DEVICE = "01234567-89ab-cdef-0123-456789abcdef"
VIDEO, OTHER = "abcdefghijk", "zyxwvutsrqp"
MAX_QUEUE = 128
MESSAGE = "YouTube phone connection stopped. Enable it again to reconnect."
CLOCK = 40   # the lease and retry clock of the one service that tests them runs this much faster


def wire(messages):
    """One frame: its length in UTF-16 code units, a newline, the JSON."""
    text = json.dumps(messages, ensure_ascii=False, separators=(",", ":"))
    return (str(len(text.encode("utf-16-le")) // 2) + "\n" + text).encode()


class Stream:
    def __init__(self, query):
        self.query, self.items, self.closed = query, queue.Queue(), threading.Event()


class Lounge:
    """The stand-in for https://www.youtube.com/api/lounge."""
    def __init__(self):
        self.calls, self.streams, self.errors = [], [], []
        self.initial = [[0, ["c", "private-SID", "", 8]], [1, ["S", "private-gsession"]], [2, ["getNowPlaying"]]]
        self.incoming, self.fragment = b"", 0
        self.fail_pair, self.update, self.stream_status, self.instant_close = False, "ok", 200, False
        self.update_entered, self.update_release = threading.Event(), threading.Event()
        self.screens = 0
        owner = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *args):
                pass

            def do_GET(self):
                owner.serve(self, "GET")

            def do_POST(self):
                owner.serve(self, "POST")

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.httpd.daemon_threads = True
        self.base = f"http://127.0.0.1:{self.httpd.server_address[1]}/api/lounge"
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def expect(self, condition, what):
        if not condition:
            self.errors.append(what)

    def answer(self, handler, status, body=b"", kind="application/json"):
        handler.send_response(status)
        handler.send_header("Content-Type", kind)
        handler.send_header("Content-Length", str(len(body)))
        handler.send_header("Connection", "close")
        handler.end_headers()
        handler.wfile.write(body)
        handler.close_connection = True

    def serve(self, handler, method):
        url = urlsplit(handler.path)
        query = {key: values[0] for key, values in parse_qs(url.query, keep_blank_values=True).items()}
        body = handler.rfile.read(int(handler.headers.get("Content-Length") or 0))
        form = {key: values[0] for key, values in parse_qs(body.decode(), keep_blank_values=True).items()}
        if url.path == "/_calls":   # for a test in another process (tests/test_embedded_e2e.c): what arrived so far
            return self.answer(handler, 200, json.dumps({"calls": self.calls, "errors": self.errors}).encode())
        self.expect(url.path.startswith("/api/lounge/"), f"path {url.path}")
        self.expect("authorization" not in {name.lower() for name in handler.headers}, "a bearer went upstream")
        path = url.path.removeprefix("/api/lounge")
        self.calls.append((method, path, query, form))
        if path == "/pairing/generate_screen_id":
            self.expect(method == "GET", "screen id method")
            self.screens += 1
            return self.answer(handler, 200, f"screen-{self.screens}\n".encode(), "text/plain")
        if path == "/pairing/get_lounge_token_batch":
            self.expect(method == "POST" and list(form) == ["screen_ids"], f"token form {form}")
            return self.answer(handler, 200, json.dumps({"screens": [{"screenId": form.get("screen_ids"), "loungeToken": "private-token"}]}).encode())
        if path == "/pairing/get_pairing_code":
            self.expect(query == {"ctx": "pair"}, f"pair query {query}")
            if self.fail_pair:
                return self.answer(handler, 503, b"private-token", "text/plain")
            return self.answer(handler, 200, b'{"code":"123456789012"}')
        self.expect(path == "/bc/bind", f"unknown path {path}")
        self.expect(query.get("loungeIdToken") == "private-token" and query.get("device") == "LOUNGE_SCREEN" and query.get("VER") == "8" and
                    re.fullmatch(r"[0-9a-f]{16}", query.get("zx", "")) is not None, f"bind query {query}")
        if query.get("TYPE") == "terminate":
            self.expect(method == "GET" and query.get("SID") == "private-SID", "terminate")
            return self.answer(handler, 200)
        if method == "GET":
            self.expect(query.get("RID") == "rpc" and query.get("SID") == "private-SID" and query.get("gsessionid") == "private-gsession" and
                        query.get("CI") == "0" and query.get("TYPE") == "xmlhttp", f"back channel query {query}")
            if self.stream_status != 200:
                return self.answer(handler, self.stream_status, b"private-token", "text/plain")
            return self.stream(handler, query)
        if form == {"count": "0"}:
            self.expect("SID" not in query and query.get("RID", "").isdigit(), f"first bind query {query}")
            return self.answer(handler, 200, wire(self.initial), "text/plain")
        self.expect(query.get("SID") == "private-SID" and query.get("AID", "").isdigit(), f"report query {query}")
        if self.update == "hold":
            self.update_entered.set()
            self.update_release.wait(10)
        if self.update == "reset":   # the connection ends without an answer
            handler.close_connection = True
            handler.connection.shutdown(socket.SHUT_RDWR)
            return None
        return self.answer(handler, 200, wire([1, 2, 3]), "text/plain")

    def stream(self, handler, query):
        """The back channel: chunked, open until the test ends it or the client leaves."""
        stream = Stream(query)
        handler.send_response(200)
        handler.send_header("Content-Type", "text/plain")
        handler.send_header("Transfer-Encoding", "chunked")
        handler.send_header("Connection", "close")
        handler.end_headers()
        handler.close_connection = True

        def write(data):
            step = self.fragment or len(data) or 1
            for start in range(0, len(data), step):
                piece = data[start:start + step]
                handler.wfile.write(b"%x\r\n" % len(piece) + piece + b"\r\n")
                handler.wfile.flush()
                if self.fragment:
                    time.sleep(0.001)
        try:
            if self.incoming:
                write(self.incoming)
            self.streams.append(stream)
            complete = False
            while not complete:
                if self.instant_close:
                    complete = True
                    break
                try:
                    item = stream.items.get(timeout=0.02)
                except queue.Empty:
                    if select.select([handler.connection], [], [], 0)[0] and handler.connection.recv(1, socket.MSG_PEEK) == b"":
                        break   # the client closed its side
                    continue
                if item == "cut":   # the connection ends in the middle of the answer
                    break
                if item is None:
                    complete = True
                else:
                    write(item)
            if complete:
                handler.wfile.write(b"0\r\n\r\n")
        except OSError:
            pass
        stream.closed.set()

    def push(self, *frames, stream=-1):
        for frame in frames:
            self.streams[stream].items.put(frame if isinstance(frame, bytes) else wire(frame))

    def end(self, stream=-1):
        self.streams[stream].items.put(None)

    def count(self, path, **query):
        return sum(1 for call in self.calls if call[1] == path and all(call[2].get(key) == value for key, value in query.items()))

    def close(self):
        for stream in self.streams:
            stream.items.put(None)
        self.httpd.shutdown()
        self.httpd.server_close()


class Service(base.Service):
    def __init__(self, binary, lounge, fixtures=None, clock=None):
        self.state = tempfile.mkdtemp(prefix="tube_js-remote-")
        environment = {**os.environ, "TUBE_JS_FIXTURES": str(fixtures or ROOT / "fixtures"), "ASAN_OPTIONS": "detect_leaks=1:abort_on_error=0",
                       "UBSAN_OPTIONS": "print_stacktrace=1", **({"TUBE_JS_REMOTE_CLOCK": str(clock)} if clock else {})}
        options = ["--lounge-base", lounge.base] if lounge else []
        self.process = subprocess.Popen([binary, "serve", "--host", "127.0.0.1", "--port", "0", "--state-dir", self.state, "--token", TOKEN, *options],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment, text=True)
        line = self.process.stdout.readline()
        if not line:
            raise SystemExit("the service did not start: " + self.process.stderr.read()[-2000:])
        self.base = json.loads(line)["listening"]
        self.port = int(self.base.rsplit(":", 1)[1])
        self.lounge, self.seen = lounge, []

    def remote(self, **body):
        status, _, answer = self.call("POST", "/v1/remote", body)
        self.seen.append(json.dumps(answer) if not isinstance(answer, bytes) else answer.decode("utf-8", "replace"))
        return status, answer

    def start(self, device=DEVICE, **more):
        status, answer = self.remote(action="start", device_id=device, **more)
        if status != 200:
            raise AssertionError(f"start answered {status} {answer}")
        return answer

    def poll(self, session, ack=0, want=0, limit=5.0):
        """Poll until at least `want` commands wait, or the receiver stops answering 200."""
        end = time.monotonic() + limit
        while True:
            status, answer = self.remote(action="poll", session=session, ack=ack)
            if status != 200 or len(answer["commands"]) >= want or time.monotonic() > end:
                return status, answer
            time.sleep(0.02)

    def failed(self, session, limit=5.0):
        """The code a receiver stopped with, once polling says so."""
        end = time.monotonic() + limit
        while time.monotonic() < end:
            status, answer = self.remote(action="poll", session=session)
            if status != 200:
                return status, answer["error"]["code"], answer["error"]["message"]
            time.sleep(0.02)
        return 200, "", ""

    def finish(self, name):
        code, errors = self.stop()
        same(f"clean shutdown ({name})", (code, errors.strip()), (0, ""))
        same(f"no Lounge secret in any answer ({name})", [body for body in self.seen if "private" in body], [])
        if self.lounge:
            same(f"the wire was as expected ({name})", self.lounge.errors, [])
            self.lounge.close()
        shutil.rmtree(self.state, ignore_errors=True)


def opened(lounge, count):
    return until(lambda: len(lounge.streams) >= count and not lounge.streams[count - 1].closed.is_set())


def until(condition, limit=5.0):
    end = time.monotonic() + limit
    while time.monotonic() < end:
        if condition():
            return True
        time.sleep(0.01)
    return bool(condition())


def error(answer):
    return answer[0], answer[1]["error"]["code"]


def registration(binary):
    """Registration, pairing, acknowledgment, retry, state feedback and stop (test_registration_...)."""
    lounge = Lounge()
    lounge.fragment = 7   # every frame arrives in seven-byte pieces
    lounge.incoming = wire([[3, ["setPlaylist", {"videoId": VIDEO, "videoIds": VIDEO + "," + OTHER, "currentTime": "13.25", "currentIndex": "0",
                                                  "listId": "RDabcdefghijk", "ctt": "opaque"}]],
                            [4, ["pause"]], [5, ["seekTo", {"newTime": "27.5"}]], [6, ["noop"]]])
    s = Service(binary, lounge)
    try:
        same("no token", s.call("POST", "/v1/remote", {"action": "start"}, token=None)[0], 401)
        same("before a start there is no receiver", error(s.remote(action="poll", session="f" * 32)), (404, "remote_session"))
        for extra in ({"origin": "http://localhost"}, {"device_id": "bad"}, {"ack": True}, {"name": "bad\n"}, {"player": {"position_sec": -1}},
                      {"player": {"position_sec": True}}, {"voice_enabled": "true"}, {"player": {"video_id": VIDEO, "extra": 1}}, {"session": "F" * 32},
                      {"update_id": -1}, {"profile": "no spaces"}, {"screen_id": "a b"}):
            same(f"refused {extra}", s.remote(action="start", **extra)[0], 422)
        same("no action", s.call("POST", "/v1/remote", {})[0], 422)
        same("nothing went upstream before a start", lounge.calls, [])
        started = s.start()
        session = started["session"]
        same("start", {**started, "session": ""}, {"api_version": 1, "session": "", "device_id": DEVICE, "screen_id": "screen-1",
                                                     "name": "ProsperoTube PS5", "state": "listening", "error": ""})
        check("opaque session", re.fullmatch(r"[0-9a-f]{32}", session) is not None, session)
        same("registration requests", [call[:2] for call in lounge.calls[:3]],
             [("GET", "/pairing/generate_screen_id"), ("POST", "/pairing/get_lounge_token_batch"), ("POST", "/bc/bind")])
        first = lounge.calls[2][2]
        same("first bind", ({key: first.get(key) for key in ("device", "theme", "capabilities", "mdxVersion", "VER", "CVER", "v", "t", "app", "name", "id", "RID")},
                            lounge.calls[1][3], lounge.calls[2][3]),
             ({"device": "LOUNGE_SCREEN", "theme": "cl", "capabilities": "dsp,dpa", "mdxVersion": "2", "VER": "8", "CVER": "1", "v": "2", "t": "1",
               "app": "lb-v4", "name": "ProsperoTube PS5", "id": DEVICE, "RID": "1"}, {"screen_ids": "screen-1"}, {"count": "0"}))
        check("the back channel opens", until(lambda: len(lounge.streams) == 1))
        same("back channel continues after the registration answer", lounge.streams[0].query.get("AID"), "2")
        same("the same device never opens a second receiver", (s.remote(action="start", device_id=DEVICE), lounge.count("/pairing/get_lounge_token_batch")),
             ((200, started), 1))
        same("pairing code", s.remote(action="pair", session=session), (200, {**started, "pairing_code": "123456789012"}))
        same("pairing form", lounge.calls[-1][3], {"lounge_token": "private-token", "screen_id": "screen-1", "screen_name": "ProsperoTube PS5",
                                                    "access_type": "permanent", "app": "lb-v4", "device_id": DEVICE, "qr": "1"})
        status, poll = s.poll(session, want=4)
        same("commands", (status, poll), (200, {**started, "ack": 0, "commands": [
            {"sequence": 1, "action": "getNowPlaying"},
            {"sequence": 2, "action": "setPlaylist", "video_id": VIDEO, "video_ids": [VIDEO, OTHER], "playlist": "RDabcdefghijk", "index": 0,
             "position_sec": 13.25, "ctt": "opaque"},
            {"sequence": 3, "action": "pause"}, {"sequence": 4, "action": "seekTo", "position_sec": 27.5}]}))
        same("a lost poll consumes nothing", s.remote(action="poll", session=session), (200, poll))
        same("an acknowledgment beyond what was delivered", error(s.remote(action="poll", session=session, ack=99)), (409, "remote_ack"))
        same("partial acknowledgment", [c["sequence"] for c in s.remote(action="poll", session=session, ack=2)[1]["commands"]], [3, 4])
        same("an acknowledgment cannot move back", error(s.remote(action="poll", session=session, ack=1)), (409, "remote_ack"))
        same("all applied", s.remote(action="poll", session=session, ack=4), (200, {**started, "ack": 4, "commands": []}))
        player = {"video_id": VIDEO, "position_sec": 27.5, "duration_sec": 300, "state": 2, "playlist": "RDabcdefghijk", "ctt": "opaque",
                  "volume": 40, "muted": True, "has_next": True, "language": "en", "vss_id": "a.en"}
        time.sleep(0.3)   # the last frame of the first answer has arrived
        same("update needs a player and an identifier", (error(s.remote(action="update", session=session, update_id=1)),
                                                        error(s.remote(action="update", session=session, player=player))), ((400, "remote_state"),) * 2)
        same("state report", s.remote(action="update", session=session, update_id=1, player=player), (200, started))
        clock = {"currentTime": "27.5", "duration": "300.0", "loadedTime": "0", "seekableStartTime": "0", "seekableEndTime": "300.0"}
        packets = [("nowPlaying", {**clock, "videoId": VIDEO, "state": "2", "listId": "RDabcdefghijk", "currentIndex": "0", "ctt": "opaque"}),
                   ("onStateChange", {**clock, "state": "2"}), ("onVolumeChanged", {"volume": "40", "muted": "true"}),
                   ("onHasPreviousNextChanged", {"hasPrevious": "false", "hasNext": "true"}),
                   ("onSubtitlesTrackChanged", {"videoId": VIDEO, "vss_id": "a.en", "languageCode": "en", "sourceLanguageCode": "en"})]
        form = {"count": "5", "ofs": "0"}
        for index, (name, values) in enumerate(packets):
            form[f"req{index}__sc"] = name
            form.update({f"req{index}_{key}": value for key, value in values.items()})
        report = [call for call in lounge.calls if call[3].get("count") == "5"]
        same("state report form", (len(report), report[0][3]), (1, form))
        same("state report channel", {key: report[0][2].get(key) for key in ("SID", "gsessionid", "AID", "RID")},
             {"SID": "private-SID", "gsessionid": "private-gsession", "AID": "6", "RID": "2"})
        before = len(lounge.calls)
        same("the same report again sends nothing", (s.remote(action="update", session=session, update_id=1, player=player)[0], len(lounge.calls)), (200, before))
        same("other data under a used identifier", error(s.remote(action="update", session=session, update_id=1, player={**player, "position_sec": 28})),
             (409, "remote_state_order"))
        same("next report", s.remote(action="update", session=session, update_id=2, player={**player, "position_sec": 28})[0], 200)
        latest = [call for call in lounge.calls if call[3].get("count") == "5"][-1]
        same("next report continues the numbering", (latest[3]["ofs"], latest[3]["req0_currentTime"], latest[2]["RID"]), ("5", "28.0", "3"))
        same("stop", s.remote(action="stop", session=session), (200, {"api_version": 1, "state": "stopped"}))
        check("the back channel is closed", lounge.streams[0].closed.wait(3))
        terminate = [call for call in lounge.calls if call[2].get("TYPE") == "terminate"]
        same("the channel is ended upstream", (len(terminate), terminate[0][0], terminate[0][2].get("SID")), (1, "GET", "private-SID"))
        same("a stopped receiver is gone", error(s.remote(action="poll", session=session)), (404, "remote_session"))
        same("stopping twice", error(s.remote(action="stop", session=session)), (404, "remote_session"))
        same("one back channel in all", len(lounge.streams), 1)
    finally:
        s.finish("registration")


def commands(binary):
    """Every typed command as it arrives while the channel is open, the reconnect after a
    completed long poll, duplicates, and windows of large queues."""
    lounge = Lounge()
    lounge.fragment = 3
    s = Service(binary, lounge)
    try:
        session = s.start(screen_id="screen-123")["session"]
        same("a saved screen is not asked for again", lounge.count("/pairing/generate_screen_id"), 0)
        check("the back channel opens", until(lambda: len(lounge.streams) == 1))
        devices = json.dumps([{"type": "LOUNGE_SCREEN", "id": "tv"}, {"type": "REMOTE_CONTROL", "id": "phone", "name": "Phone", "token": "secret"}])
        lounge.push([[3, ["remoteConnected", {"id": "phone", "name": "日本語 😀"}]]], [[4, ["play"]], [5, ["next"]], [6, ["previous"]]],
                    [[7, ["setVolume", {"volume": "75", "delta": "-5"}]], [8, ["setVolume", {"delta": 5}]], [9, ["dpadCommand", {"key": "ENTER"}]],
                     [10, ["voiceCommand", {"status": "INITIATED"}]], [11, ["setSubtitlesTrack", {"videoId": VIDEO, "languageCode": "pt-BR"}]],
                     [12, ["loungeStatus", {"devices": devices}]], [13, ["futureExtension", {"anything": 1}]], [14, ["noop"]]],
                    [[4, ["pause"]], [14, ["pause"]]],   # wire numbers already seen are not applied again
                    [[15, ["setPlaylist", {"videoIds": VIDEO + "," + OTHER, "currentIndex": "1"}]], [16, ["updatePlaylist", {"videoIds": VIDEO}]],
                     [17, ["stopVideo"]], [18, ["remoteDisconnected", {"id": "phone", "name": "Phone"}]]])
        status, poll = s.poll(session, want=14)
        same("typed commands", (status, poll["commands"]), (200, [
            {"sequence": 1, "action": "getNowPlaying"}, {"sequence": 2, "action": "remoteConnected", "device_id": "phone", "name": "日本語 😀"},
            {"sequence": 3, "action": "play"}, {"sequence": 4, "action": "next"}, {"sequence": 5, "action": "previous"},
            {"sequence": 6, "action": "setVolume", "volume": 75, "delta": -5}, {"sequence": 7, "action": "setVolume", "delta": 5},
            {"sequence": 8, "action": "dpadCommand", "key": "ENTER"}, {"sequence": 9, "action": "voiceCommand", "status": "INITIATED"},
            {"sequence": 10, "action": "setSubtitlesTrack", "video_id": VIDEO, "language": "pt-BR"},
            {"sequence": 11, "action": "loungeStatus", "devices": [{"device_id": "phone", "name": "Phone"}]},
            {"sequence": 12, "action": "setPlaylist", "video_id": OTHER, "video_ids": [VIDEO, OTHER], "playlist": "", "index": 1, "position_sec": 0, "ctt": ""},
            {"sequence": 13, "action": "updatePlaylist", "video_id": "", "video_ids": [VIDEO], "playlist": "", "index": 0, "position_sec": 0, "ctt": ""},
            {"sequence": 14, "action": "stopVideo"}, {"sequence": 15, "action": "remoteDisconnected", "device_id": "phone", "name": "Phone"}][:len(poll["commands"])]))
        status, poll = s.poll(session, ack=14, want=1)
        same("the last one", poll["commands"], [{"sequence": 15, "action": "remoteDisconnected", "device_id": "phone", "name": "Phone"}])
        same("applied", s.poll(session, ack=15)[1]["commands"], [])
        # More than one poll's worth: 32 at a time, in order, none lost.
        lounge.push([[19 + n, ["seekTo", {"newTime": n}]] for n in range(40)])
        status, poll = s.poll(session, ack=15, want=32)
        same("32 commands per poll", ([c["sequence"] for c in poll["commands"]], poll["commands"][31]["position_sec"]), (list(range(16, 48)), 31))
        status, poll = s.poll(session, ack=47, want=8)
        same("the rest follows", [c["sequence"] for c in poll["commands"]], list(range(48, 56)))
        s.poll(session, ack=55)
        # A long poll that completes is followed by another one, from where the last ended, after a pause.
        ended = time.monotonic()
        lounge.end()
        check("a new back channel", until(lambda: len(lounge.streams) == 2))
        same("it continues after the last frame", (lounge.streams[1].query.get("AID"), time.monotonic() - ended > 0.2), ("58", True))
        lounge.push([[59, ["pause"]]])
        same("commands arrive on the new channel", s.poll(session, ack=55, want=1)[1]["commands"], [{"sequence": 56, "action": "pause"}])
        lounge.instant_close = True
        lounge.end()
        time.sleep(1.5)
        lounge.instant_close = False
        check("empty answers are not hammered", 3 <= len(lounge.streams) <= 10, len(lounge.streams))
        check("and the channel comes back", until(lambda: not lounge.streams[-1].closed.is_set() and s.poll(session, ack=56)[0] == 200))
        # Queues longer than one native window: 128 around the current video, the rest kept for refills.
        ids = [f"v{i:010d}" for i in range(500)]
        ids[250] = ids[10]
        lounge.fragment = 4096
        lounge.push([[60, ["setPlaylist", {"videoIds": ",".join(ids), "videoId": ids[250], "currentIndex": 250, "listId": "PLtestlarge", "ctt": "click"}]]])
        window = s.poll(session, ack=56, want=1)[1]["commands"][0]
        same("window around a repeated video", (window["sequence"], window["index"], window["window_start"], window["video_ids"]), (57, 250, 218, ids[218:346]))
        lounge.push([[61, ["setPlaylist", {"videoIds": ",".join([VIDEO] * 10000), "currentIndex": 9999}]]])   # a 120 KB frame
        largest = s.poll(session, ack=57, want=1)[1]["commands"][0]
        same("the largest queue", (len(largest["video_ids"]), largest["window_start"], largest["index"], largest["video_id"]), (128, 9872, 9999, VIDEO))
        # Refill after confirmed feedback (test_phone_queue_windows_refresh_after_owned_feedback_and_retire_on_replacement).
        ids = [f"w{i:010d}" for i in range(500)]
        lounge.push([[62, ["setPlaylist", {"videoId": ids[0], "videoIds": ",".join(ids), "listId": "PLtestlarge", "currentIndex": 0, "ctt": "click"}]]])
        same("first window", s.poll(session, ack=58, want=1)[1]["commands"][0]["window_start"], 0)
        s.poll(session, ack=59)
        state = {"video_id": ids[112], "playlist": "PLtestlarge", "ctt": "click", "index": 112, "position_sec": 4.5, "has_previous": True, "has_next": True}
        for _ in range(2):
            s.remote(action="update", session=session, update_id=1, player=state)
        refill = s.poll(session, ack=59)[1]["commands"]
        same("one refill near the window's end", [(c["action"], c["sequence"], c["index"], c["window_start"], c["video_ids"] == ids[80:208], c["position_sec"]) for c in refill],
             [("updatePlaylist", 60, 112, 80, True, 4.5)])
        state = {**state, "video_id": ids[200], "index": 200}
        s.remote(action="update", session=session, update_id=2, player=state)
        same("no second refill while one waits", len(s.poll(session, ack=59)[1]["commands"]), 1)
        s.poll(session, ack=60)
        s.remote(action="update", session=session, update_id=3, player={**state, "position_sec": 5})
        same("the next refill", [c["window_start"] for c in s.poll(session, ack=60)[1]["commands"]], [168])
        s.poll(session, ack=61)
        local = {**state, "video_id": ids[480], "index": 480, "has_previous": False, "has_next": False}
        s.remote(action="update", session=session, update_id=4, player=local)
        same("local playback takes no phone queue", s.poll(session, ack=61)[1]["commands"], [])
        ended_state = {"video_id": ids[295], "playlist": "PLtestlarge", "ctt": "click", "index": 295, "state": 0}
        s.remote(action="update", session=session, update_id=5, player=ended_state)
        same("an ended video refills without the playing flags", [c["window_start"] for c in s.poll(session, ack=61)[1]["commands"]], [263])
        s.poll(session, ack=62)
        lounge.push([[63, ["stopVideo", {}]]])
        s.poll(session, ack=62, want=1)
        s.poll(session, ack=63)
        s.remote(action="update", session=session, update_id=6, player={**ended_state, "video_id": ids[400], "index": 400})
        same("a stop retires the kept queue", s.poll(session, ack=63)[1]["commands"], [])
        same("stop", s.remote(action="stop", session=session)[0], 200)
    finally:
        s.finish("commands")


def refusals(binary):
    """Malformed frames and invalid commands end the channel without applying or leaking anything."""
    lounge = Lounge()
    s = Service(binary, lounge)
    many = ",".join([VIDEO] * 10001)
    cases = [(name, "remote_protocol", wire([[3, [name, args]]])) for name, args in (
        ("setPlaylist", {}), ("setPlaylist", {"videoId": "file:///bad"}), ("setPlaylist", {"videoId": VIDEO, "currentTime": "NaN"}),
        ("setPlaylist", {"videoId": VIDEO, "currentIndex": "0.5"}), ("setPlaylist", {"videoId": VIDEO, "videoIds": many}),
        ("setPlaylist", {"videoIds": ",".join([f"v{i:010d}" for i in range(500)]), "videoId": "xxxxxxxxxxx", "currentIndex": 250}),
        ("seekTo", {"newTime": "-1"}), ("seekTo", {"newTime": True}), ("seekTo", {"newTime": "nan"}), ("seekTo", {}), ("setVolume", {}),
        ("setVolume", {"volume": "101"}), ("dpadCommand", {"key": "exec"}), ("voiceCommand", {"status": "EXEC"}), ("loungeStatus", {"devices": "{}"}),
        ("setSubtitlesTrack", {"languageCode": "\n"}), ("play", []), ("remoteConnected", {"id": "x" * 129}))]
    cases += [(repr(data[:12]), "remote_protocol", data) for data in (b"-1\n", b"0000000\n", b"262145\n", b"4\nnull", b"3\nNaN", b"1\n\xff", b"2\n{}",
                                                                      b"5\n[[3]]", b'13\n[["3",["a"]]]', b"11\n[[3,[7,1]]]", b"3\n[1]\xc0\xaf")]
    cases += [("a gap in the wire numbers", "remote_gap", wire([[4, ["pause"]]])), ("stop", "remote_disconnected", wire([[3, ["stop"]]])),
              ("close", "remote_disconnected", wire([[3, ["play"]], [4, ["close"]]])),
              ("overflow", "remote_overflow", wire([[index, ["play"]] for index in range(3, 3 + MAX_QUEUE)]))]
    try:
        for count, (name, code, data) in enumerate(cases, 1):
            session = s.start()["session"]
            check(f"channel {count}", until(lambda: len(lounge.streams) == count))
            lounge.push(data)
            same(f"refused: {name}", s.failed(session), (409, code, MESSAGE))
            check(f"its channel is closed: {name}", lounge.streams[-1].closed.wait(3))
            before = len(lounge.calls)
            same(f"a failed receiver sends nothing: {name}", (error(s.remote(action="update", session=session, update_id=1, player={})), len(lounge.calls)),
                 ((409, code), before))
        # A truncated frame is found when the long poll ends.
        session = s.start()["session"]
        check("channel for the truncated frame", until(lambda: len(lounge.streams) == len(cases) + 1))
        lounge.push(b"20\n[[3,[\"pa")
        lounge.end()
        same("refused: truncated", s.failed(session), (409, "remote_protocol", MESSAGE))
        same("stop after a failure", s.remote(action="stop", session=session)[0], 200)
    finally:
        s.finish("refusals")


def failures(binary):
    """Upstream failures leak nothing, are not replayed, and a failed receiver stays failed."""
    lounge = Lounge()
    s = Service(binary, lounge)
    player = {"video_id": VIDEO}
    try:
        # The pairing request is refused.
        session = s.start()["session"]
        lounge.fail_pair = True
        same("pairing refused upstream", s.remote(action="pair", session=session), (502, {"api_version": 1, "error": {"code": "remote_upstream", "message": MESSAGE}}))
        lounge.fail_pair = False
        before = len(lounge.calls)
        same("the receiver stays failed", (error(s.remote(action="pair", session=session)), error(s.remote(action="poll", session=session)), len(lounge.calls)),
             ((409, "remote_upstream"), (409, "remote_upstream"), before))
        check("its channel is closed", until(lambda: lounge.streams and lounge.streams[-1].closed.is_set()))
        # A state report whose answer never comes: uncertain, so never sent again.
        session = s.start()["session"]
        lounge.update = "reset"
        same("report without an answer", error(s.remote(action="update", session=session, update_id=1, player=player)), (502, "remote_upstream"))
        lounge.update = "ok"
        same("not replayed", (sum(1 for call in lounge.calls if call[3].get("count") == "5"), error(s.remote(action="update", session=session, update_id=1, player=player))),
             (1, (409, "remote_upstream")))
        # The back channel fails while a report is in flight: no success is published.
        session = s.start()["session"]
        check("channel for the in-flight report", opened(lounge, 3))
        lounge.update, answer = "hold", {}
        thread = threading.Thread(target=lambda: answer.update(result=s.remote(action="update", session=session, update_id=1, player=player)))
        thread.start()
        check("the report is in flight", lounge.update_entered.wait(5))
        lounge.push(wire([[3, ["seekTo", {"newTime": "nan"}]]]))
        check("the channel failed meanwhile", lounge.streams[-1].closed.wait(5))
        lounge.update_release.set()
        thread.join(10)
        lounge.update = "ok"
        same("the report is not confirmed", error(answer["result"]), (502, "remote_protocol"))
        same("and the receiver stays failed", error(s.remote(action="update", session=session, update_id=2, player=player)), (409, "remote_protocol"))
        # The back channel is refused upstream.
        session = s.start()["session"]
        check("channel before the refusal", opened(lounge, 4))
        lounge.stream_status = 503
        lounge.end()
        same("back channel refused", s.failed(session), (409, "remote_upstream", MESSAGE))
        lounge.stream_status = 200
        # The back channel breaks off in the middle of the answer.
        session = s.start()["session"]
        check("channel before the break", opened(lounge, 5))
        lounge.push(b"30\n[[3,[\"pause\"]]")
        lounge.streams[-1].items.put("cut")
        same("back channel cut off", s.failed(session), (409, "remote_upstream", MESSAGE))
        # Registration that does not complete leaves nothing behind.
        lounge.initial = [[0, ["c", "private-SID"]]]   # no global session
        streams = len(lounge.streams)
        same("incomplete registration", error(s.remote(action="start", device_id=DEVICE)), (502, "remote_protocol"))
        same("it is ended upstream and opens no channel", (lounge.calls[-1][2].get("TYPE"), len(lounge.streams)), ("terminate", streams))
        lounge.initial = [[0, ["c", "private-SID", "", 8]], [1, ["S", "private-gsession"]]]
        same("the next one works", s.remote(action="start", device_id=DEVICE)[0], 200)
    finally:
        s.finish("failures")


def identity(binary):
    """Identity reuse, conflict, capacity, the microphone capability and the lease."""
    lounge = Lounge()
    s = Service(binary, lounge, clock=CLOCK)
    try:
        started = s.start(screen_id="screen-123", voice_enabled=True)
        session = started["session"]
        same("the saved identity is used as given", (started["device_id"], started["screen_id"], lounge.count("/pairing/generate_screen_id"), lounge.calls[0][3]),
             (DEVICE, "screen-123", 0, {"screen_ids": "screen-123"}))
        same("no microphone without a speech recogniser", lounge.calls[1][2].get("capabilities"), "dsp,dpa")
        same("asking again without it is the same receiver", s.remote(action="start", device_id=DEVICE, screen_id="screen-123"), (200, started))
        same("another name", error(s.remote(action="start", device_id=DEVICE, name="Other name")), (409, "remote_identity_conflict"))
        same("another screen", error(s.remote(action="start", device_id=DEVICE, screen_id="screen-9")), (409, "remote_identity_conflict"))
        same("another profile", error(s.remote(action="start", device_id=DEVICE, profile="other")), (409, "remote_identity_conflict"))
        # Four receivers at most; a fifth contacts nobody.
        others = [f"{n:08d}-89ab-cdef-0123-456789abcdef" for n in range(1, 5)]
        same("three more", [s.remote(action="start", device_id=device)[0] for device in others[:3]], [200] * 3)
        check("four back channels", opened(lounge, 4))
        before = len(lounge.calls)
        same("capacity", (s.remote(action="start", device_id=others[3]), len(lounge.calls)),
             ((429, {"api_version": 1, "error": {"code": "remote_capacity", "message": MESSAGE}}), before))
        same("capacity, also without an identity", error(s.remote(action="start")), (429, "remote_capacity"))
        # The lease: a receiver nobody polls for 120 s (here 3 s) is expired and replaced.
        time.sleep(120 / CLOCK + 0.4)
        same("expired", error(s.remote(action="poll", session=session)), (409, "remote_expired"))
        fresh = s.start(screen_id="screen-123")
        check("a new receiver", fresh["session"] != session and fresh["screen_id"] == "screen-123")
        same("the old one is gone", error(s.remote(action="poll", session=session)), (404, "remote_session"))
        check("every expired channel is closed", until(lambda: all(stream.closed.is_set() for stream in lounge.streams[:4])))
        same("each was ended upstream", lounge.count("/bc/bind", TYPE="terminate"), 4)
        same("polling renews the lease", [s.remote(action="poll", session=fresh["session"])[0] for _ in range(8) if not time.sleep(0.5)], [200] * 8)
        generated = s.remote(action="start")[1]
        check("a receiver without a saved identity", re.fullmatch(r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}", generated["device_id"]) is not None and
              re.fullmatch(r"screen-[0-9]+", generated["screen_id"]) is not None, generated)
    finally:
        s.finish("identity")


def panel(playlist, rows, current, **more):
    """A WatchNext answer with a playlist panel: rows are (index, video or None for an unavailable one)."""
    contents = []
    for index, video in rows:
        endpoint = {"watchEndpoint": {"playlistId": playlist, "index": index, **({"videoId": video} if video else {})}}
        contents.append({"playlistPanelVideoRenderer": {"navigationEndpoint": endpoint, "selected": index == current,
                                                        **({"videoId": video} if video else {"isPlayable": False})}})
    return {"contents": {"twoColumnWatchNextResults": {"playlist": {"playlist": {"playlistId": playlist, "currentIndex": current, "contents": contents, **more}}}}}


def expansion(binary):
    """A partial phone queue grows through the library's WatchNext read after confirmed playback
    (tests/test_remote_playlist.py), with YouTube's answer canned."""
    video = [f"0000000{n:04d}" for n in range(200)]
    directory = Path(tempfile.mkdtemp(prefix="tube_js-remote-fixtures-"))
    shutil.copytree(ROOT / "fixtures", directory, dirs_exist_ok=True)
    index = json.loads((directory / "index.json").read_text())
    answers = {"PLsyntheticqueue": panel("PLsyntheticqueue", [(100, video[100]), (101, video[101]), (102, video[102])], 101, totalVideos=500),
               "PLsparsequeue00": panel("PLsparsequeue00", [(100, video[100]), (101, video[101]), (102, None), (103, None), (105, video[105])], 101),
               "PLbrokenqueue00": {"contents": {}},
               "PLendedqueue000": panel("PLendedqueue000", [(101, video[101])], 101)}
    entries = []
    for playlist, answer in answers.items():
        (directory / f"r_{playlist}.json").write_text(json.dumps(answer))
        entries.append({"body_contains": f'"playlistId":"{playlist}"', "tag": playlist, "file": f"r_{playlist}.json"})
    index["POST www.youtube.com/youtubei/v1/next"] = entries + index["POST www.youtube.com/youtubei/v1/next"]
    (directory / "index.json").write_text(json.dumps(index))
    lounge = Lounge()
    s = Service(binary, lounge, fixtures=directory, clock=CLOCK)

    def reads(tag):
        return sum(1 for request in s.call("GET", "/v1/_upstream")[2]["requests"] if request["tag"] == tag)

    try:
        session = s.start()["session"]
        check("the back channel opens", until(lambda: len(lounge.streams) == 1))
        # The phone names one video of a playlist; the queue around it is read once playback is confirmed.
        lounge.push([[3, ["setPlaylist", {"videoId": video[101], "listId": "PLsyntheticqueue", "currentIndex": 101, "ctt": "click"}]]])
        same("the phone's own queue", s.poll(session, want=2)[1]["commands"][1], {
            "sequence": 2, "action": "setPlaylist", "video_id": video[101], "video_ids": [], "playlist": "PLsyntheticqueue", "index": 101,
            "position_sec": 0, "ctt": "click"})
        state = {"video_id": video[101], "playlist": "PLsyntheticqueue", "index": 101, "ctt": "click", "position_sec": 3.5, "state": 1}
        s.remote(action="update", session=session, update_id=1, player=state)
        time.sleep(0.3)
        same("nothing is read before the launch is acknowledged", (reads("PLsyntheticqueue"), s.poll(session, ack=1)[1]["commands"][0]["sequence"]), (0, 2))
        s.poll(session, ack=2)
        s.remote(action="update", session=session, update_id=2, player={**state, "position_sec": 8.75})
        grown = s.poll(session, ack=2, want=1)[1]["commands"]
        same("the queue grows without reopening playback", grown, [{
            "sequence": 3, "action": "updatePlaylist", "video_id": video[101], "playlist": "PLsyntheticqueue", "ctt": "click", "index": 101,
            "position_sec": 8.75, "video_ids": [video[100], video[101], video[102]], "window_start": 100}])
        s.poll(session, ack=3)
        for update in (3, 4):
            s.remote(action="update", session=session, update_id=update, player={**state, "position_sec": 9 + update})
        time.sleep(12 / CLOCK)
        s.remote(action="update", session=session, update_id=5, player={**state, "position_sec": 20})
        time.sleep(0.3)
        same("the same confirmed position is not read again", (reads("PLsyntheticqueue"), s.poll(session, ack=3)[1]["commands"]), (1, []))
        # Unavailable rows are left out without renumbering the others.
        lounge.push([[4, ["setPlaylist", {"videoId": video[101], "listId": "PLsparsequeue00", "currentIndex": 101, "ctt": "tap"}]]])
        s.poll(session, ack=3, want=1)
        s.poll(session, ack=4)
        sparse = {"video_id": video[101], "playlist": "PLsparsequeue00", "index": 101, "ctt": "tap", "position_sec": 1, "state": 1}
        s.remote(action="update", session=session, update_id=6, player=sparse)
        same("a sparse window keeps the original positions", s.poll(session, ack=4, want=1)[1]["commands"], [{
            "sequence": 5, "action": "updatePlaylist", "video_id": video[101], "playlist": "PLsparsequeue00", "ctt": "tap", "index": 101, "position_sec": 1,
            "video_ids": [video[100], video[101], video[105]], "window_start": 100, "video_indices": [100, 101, 105]}])
        s.poll(session, ack=5)
        # An answer without a usable queue keeps the phone's own, backs off, and never harms the receiver.
        lounge.push([[5, ["setPlaylist", {"videoId": video[7], "listId": "PLbrokenqueue00", "currentIndex": 7, "ctt": ""}]]])
        s.poll(session, ack=5, want=1)
        s.poll(session, ack=6)
        broken = {"video_id": video[7], "playlist": "PLbrokenqueue00", "index": 7, "position_sec": 1, "state": 1}
        for update in (7, 8, 9):
            s.remote(action="update", session=session, update_id=update, player={**broken, "position_sec": update})
            time.sleep(0.05)
        time.sleep(0.2)
        same("an unusable answer is not asked for again at once", reads("PLbrokenqueue00"), 1)
        same("the receiver is healthy", s.poll(session, ack=6)[1]["commands"], [])
        time.sleep(12 / CLOCK)
        s.remote(action="update", session=session, update_id=10, player={**broken, "position_sec": 30})
        check("it is asked again after the pause", until(lambda: reads("PLbrokenqueue00") == 2, 3))
        # A queue replaced while its read is out, and an ended last video with nothing after it.
        lounge.push([[6, ["setPlaylist", {"videoId": video[101], "listId": "PLendedqueue000", "currentIndex": 101, "ctt": ""}]]])
        s.poll(session, ack=6, want=1)
        s.poll(session, ack=7)
        ended = {"video_id": video[101], "playlist": "PLendedqueue000", "index": 101, "state": 0}
        s.remote(action="update", session=session, update_id=11, player=ended)
        same("an ended last video gets its explicit one-item window", s.poll(session, ack=7, want=1)[1]["commands"], [{
            "sequence": 8, "action": "updatePlaylist", "video_id": video[101], "playlist": "PLendedqueue000", "ctt": "", "index": 101, "position_sec": 0,
            "video_ids": [video[101]], "window_start": 101}])
        s.poll(session, ack=8)
        time.sleep(12 / CLOCK)
        s.remote(action="update", session=session, update_id=12, player={**ended, "duration_sec": 5})
        time.sleep(0.3)
        same("and that confirmation does not loop", (reads("PLendedqueue000"), s.poll(session, ack=8)[1]["commands"]), (1, []))
        same("stop", s.remote(action="stop", session=session)[0], 200)
    finally:
        s.finish("expansion")
        shutil.rmtree(directory, ignore_errors=True)


def offline(binary):
    """Without the stand-in the receiver would ask YouTube; in the offline tests that request has no
    canned answer and fails inside the service, and health lists the feature."""
    s = Service(binary, None)
    try:
        check("health lists the phone connection", "remote" in s.call("GET", "/v1/health", token=None)[2]["features"])
        same("no receiver without a start", error(s.remote(action="stop", session="0" * 32)), (404, "remote_session"))
        same("start cannot reach YouTube here", s.remote(action="start", device_id=DEVICE),
             (502, {"api_version": 1, "error": {"code": "remote_upstream", "message": MESSAGE}}))
        requests = [request["key"] for request in s.call("GET", "/v1/_upstream")[2]["requests"]]
        same("and it asked YouTube's own address", requests, ["GET www.youtube.com/api/lounge/pairing/generate_screen_id"])
    finally:
        s.finish("offline")


def serve():
    """The stand-in alone, for scripts/test_embedded_e2e.sh: prints its address, serves until its input closes."""
    lounge = Lounge()
    lounge.fragment = 5
    lounge.incoming = wire([[3, ["setPlaylist", {"videoId": "fixtureVID1", "videoIds": "fixtureVID1," + OTHER, "currentTime": "12.5",
                                                  "currentIndex": "0", "listId": "", "ctt": "opaque"}]], [4, ["pause"]]])
    print(lounge.base, flush=True)
    sys.stdin.read()


def main():
    if sys.argv[1:] == ["--serve"]:
        return serve()
    binary = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build" / "tube_js")
    only = sys.argv[2:] or ["offline", "registration", "commands", "refusals", "failures", "identity", "expansion"]
    for name in only:
        try:
            globals()[name](binary)
        except Exception as problem:   # a broken answer shape is a failure, not a crash of the test
            base.failures.append(f"{name}: {type(problem).__name__}: {problem}")
    for failure in base.failures:
        print("FAIL", failure)
    print(f"remote test: {base.passed} checks passed, {len(base.failures)} failed")
    return 1 if base.failures else 0


if __name__ == "__main__":
    sys.exit(main())
