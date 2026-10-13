#!/usr/bin/env python3
"""Offline test of `POST /v1/subscriptions/import` in `tube_js serve`: the
parser cases of tests/test_third_batch.py (NewPipe, PocketTube, GrayJay,
addresses, refusals) and the route's own bounds, through the real service.

    python3 standalone/test/import_test.py [path/to/tube_js]

Nothing upstream is involved: the body is the export file and the answer is
its channel list. SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
from pathlib import Path
import socket
import sys
import time

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import service_test as base  # noqa: E402

check, same, TOKEN, CHANNEL = base.check, base.same, base.TOKEN, base.CHANNEL
ROOT = HERE.parent
PATH = "/v1/subscriptions/import"
INVALID = {"api_version": 1, "error": {"code": "invalid_import",
                                       "message": "This is not a NewPipe, PocketTube or GrayJay subscriptions export."}}
MAX_CHANNELS = 2048


def channel(number):
    return "UC" + f"{number:022d}"


def address(number):
    return f"https://www.youtube.com/channel/{channel(number)}"


def answer(name, channels, skipped=0, truncated=False):
    return {"api_version": 1, "kind": "subscriptions_import", "format": name, "channels": channels, "skipped": skipped, "truncated": truncated}


def formats(s, parse):
    newpipe = json.dumps({"app_version": "0.27.2", "app_version_int": 999, "subscriptions": [
        {"service_id": 0, "url": address(1), "name": " First \x00channel "},
        {"service_id": 0, "url": address(1), "name": "Duplicate"},
        {"service_id": 1, "url": address(2), "name": "Another service"},
        {"service_id": 0, "url": "https://www.youtube.com/user/legacy", "name": "No identity"},
        {"service_id": 0, "url": address(3), "name": 7}, "junk"]})
    same("NewPipe", parse(newpipe), (200, answer("newpipe", [{"channel_id": channel(1), "title": "First channel"},
                                                             {"channel_id": channel(3), "title": ""}], 3)))
    same("NewPipe without a service number", parse(json.dumps({"subscriptions": [{"url": address(4), "name": "x" * 300}]}))[1]["channels"],
         [{"channel_id": channel(4), "title": "x" * 192}])
    groups = {"Music": [channel(1), channel(2)], "News 日本": [channel(2), channel(3), "not-a-channel"],
              "ysc_collection": {"Music": {"order": 0}}, "ysc_settings": "x"}
    expected = answer("pockettube", [{"channel_id": channel(1), "title": "", "group": "Music"}, {"channel_id": channel(2), "title": "", "group": "Music"},
                                     {"channel_id": channel(3), "title": "", "group": "News 日本"}], 1)
    same("PocketTube", parse(json.dumps(groups)), (200, expected))
    same("PocketTube, not escaped", parse(json.dumps(groups, ensure_ascii=False)), (200, expected))
    same("PocketTube group name bound", parse(json.dumps({"g" * 80: [channel(1)]}))[1]["channels"][0]["group"], "g" * 64)
    grayjay = json.dumps([address(1),
                          json.dumps({"channel": {"url": address(2), "name": "Serialized"}}),
                          {"channel": {"url": address(3), "name": "Record"}},
                          {"url": address(4), "name": "Flat"},
                          "https://rumble.com/c/other", "{broken", 5])
    same("GrayJay behind a byte-order mark", parse(b"\xef\xbb\xbf" + grayjay.encode()), (200, answer("grayjay", [
        {"channel_id": channel(1), "title": ""}, {"channel_id": channel(2), "title": "Serialized"},
        {"channel_id": channel(3), "title": "Record"}, {"channel_id": channel(4), "title": "Flat"}], 3)))
    many = parse(json.dumps([channel(n) for n in range(MAX_CHANNELS + 5)]))[1]
    same("bounded list", (len(many["channels"]), many["truncated"], many["channels"][-1]["channel_id"], many["skipped"]),
         (MAX_CHANNELS, True, channel(MAX_CHANNELS - 1), 0))
    same("empty exports", (parse("[]"), parse('{"subscriptions": []}')), ((200, answer("grayjay", [])), (200, answer("newpipe", []))))


def addresses(s, parse):
    """test_channel_addresses_are_recognized_only_by_identity, and a few more."""
    accepted = [channel(7), address(7), f"http://youtube.com/channel/{channel(7)}/", f" https://m.youtube.com/channel/{channel(7)}?view=0 ",
                f"HTTPS://WWW.YouTube.com:443/channel/{channel(7)}#x", f"https://user@www.youtube.com/channel/{channel(7)}"]
    for value in accepted:
        same(f"accepts {value.strip()[:44]}", parse(json.dumps([value])), (200, answer("grayjay", [{"channel_id": channel(7), "title": ""}])))
    refused = ["", None, 7, "https://www.youtube.com/@handle", "https://www.youtube.com/user/name", "https://www.youtube.com/c/name",
               f"https://example.com/channel/{channel(7)}", f"https://notyoutube.com/channel/{channel(7)}",
               f"ftp://www.youtube.com/channel/{channel(7)}", f"https://www.youtube.com/channel/{channel(7)}/videos",
               "https://www.youtube.com/channel/UCshort", "https://[bad/channel/x", "UC" + "c" * 23, "x" * 3000,
               f"https:www.youtube.com/channel/{channel(7)}", f"https://youtube.com.example.org/channel/{channel(7)}",
               f"https://www.youtube.com@example.org/channel/{channel(7)}", f"https://www.youtube.com/channel/{channel(7)}//",
               channel(7) + "\n", [channel(7)], {"id": channel(7)}]
    same("refuses everything else", parse(json.dumps(refused)), (200, answer("grayjay", [], len(refused))))


def refusals(s, parse):
    """test_unrecognized_exports_are_refused, plus bytes that are not UTF-8."""
    for raw in ("", "not json", "7", '"text"', "null", "{}", '{"a": 1}', '{"Group": "UC"}', '{"Group": ["x"]}', '{"ysc_collection": {}}',
                "[" * 100000, b"\xff\xfe", b"\xef\xbb\xbf\xef\xbb\xbf[]", b'["\xed\xa0\x80"]', b'["\xc0\xaf"]', b'["\xe6\x97"]', b"[]\xf5", "oops"):
        same(f"refuses {raw[:16]!r}", parse(raw), (400, INVALID))


def route(s, parse):
    check("health lists the import", "subscriptions_import" in s.call("GET", "/v1/health", token=None)[2]["features"])
    body = json.dumps({"subscriptions": [{"service_id": 0, "url": f"https://www.youtube.com/channel/{CHANNEL}", "name": "Imported"}]}).encode()
    same("needs the bearer", s.call("POST", PATH, body, token=None)[0], 401)
    same("answer", s.call("POST", PATH, body)[::2], (200, answer("newpipe", [{"channel_id": CHANNEL, "title": "Imported"}])))
    same("a query changes nothing", s.call("POST", PATH + "?x=1", body)[0], 200)
    same("only POST", s.call("GET", PATH)[0], 405)
    large = json.dumps([address(n) for n in range(9000)]).encode()
    check("the large file is large", 8192 < len(large) < 1024 * 1024, len(large))
    same("a file far over the other routes' 8 KiB", len(parse(large)[1]["channels"]), MAX_CHANNELS)
    same("exactly one mebibyte", parse(b"[" + b" " * (1024 * 1024 - 2) + b"]"), (200, answer("grayjay", [])))
    head = b"POST " + PATH.encode() + b" HTTP/1.1\r\nAuthorization: Bearer " + TOKEN.encode() + b"\r\nContent-Length: "
    check("one byte more", base.raw_exchange(s.port, head + b"1048577\r\n\r\n").startswith(b"HTTP/1.1 413 "))
    check("the other routes keep their limit", base.raw_exchange(s.port, head.replace(PATH.encode(), b"/v1/library") + b"8193\r\n\r\n").startswith(b"HTTP/1.1 413 "))
    check("a longer path is another route", base.raw_exchange(s.port, head.replace(PATH.encode(), PATH.encode() + b"x") + b"8193\r\n\r\n").startswith(b"HTTP/1.1 413 "))
    check("GET takes no large body", base.raw_exchange(s.port, head.replace(b"POST", b"GET") + b"8193\r\n\r\n").startswith(b"HTTP/1.1 413 "))
    # A body that arrives in pieces, a second large one meanwhile, and one that is abandoned.
    slow = socket.create_connection(("127.0.0.1", s.port), timeout=10)
    slow.sendall(head + str(len(large)).encode() + b"\r\n\r\n" + large[:5000])
    time.sleep(0.2)
    check("one large body at a time", base.raw_exchange(s.port, head + b"20000\r\n\r\n" + b" " * 100).startswith(b"HTTP/1.1 503 "))
    same("small requests are served meanwhile", s.call("GET", "/v1/health", token=None)[0], 200)
    for start in range(5000, len(large), 60000):
        slow.sendall(large[start:start + 60000])
        time.sleep(0.01)
    reply = b""
    while chunk := slow.recv(65536):
        reply += chunk
    slow.close()
    check("a body in pieces", reply.startswith(b"HTTP/1.1 200 ") and len(json.loads(reply.split(b"\r\n\r\n", 1)[1])["channels"]) == MAX_CHANNELS, reply[:80])
    abandoned = socket.create_connection(("127.0.0.1", s.port), timeout=10)
    abandoned.sendall(head + b"500000\r\n\r\n" + b"[" * 9000)
    time.sleep(0.2)
    abandoned.close()
    time.sleep(0.5)
    same("an abandoned body frees its place", len(parse(large)[1]["channels"]), MAX_CHANNELS)
    same("nothing went upstream", s.call("GET", "/v1/_upstream")[2]["requests"], [])


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build" / "tube_js")
    service = base.Service(binary, "--token", TOKEN)

    def parse(raw):
        return service.call("POST", PATH, raw if isinstance(raw, bytes) else raw.encode())[::2]

    try:
        for part in (formats, addresses, refusals, route):
            try:
                part(service, parse)
            except Exception as error:  # a broken answer shape is a failure, not a crash of the test
                base.failures.append(f"{part.__name__}: {type(error).__name__}: {error}")
    finally:
        code, errors = service.stop()
    same("clean shutdown", (code, errors.strip()), (0, ""))
    for failure in base.failures:
        print("FAIL", failure)
    print(f"import test: {base.passed} checks passed, {len(base.failures)} failed")
    return 1 if base.failures else 0


if __name__ == "__main__":
    sys.exit(main())
