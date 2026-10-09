"""Explicit public or configured OAuth YouTube API/relay probe; no console traffic."""
import argparse
import json
import re
from pathlib import Path
import socket
import subprocess
import threading
import time
import urllib.error
import urllib.request

import uvicorn
from prosperotube.app import create_app
from prosperotube.extractor import Extractor
from prosperotube.models import ServiceError, video_id


def probe_app(args):
    """Never discover browser credentials or fall back from a selected account."""
    runtimes = {args.js_runtime: {"path": args.js_runtime_path} if args.js_runtime_path else {}}
    guest = Extractor(timeout=60, js_runtimes=runtimes)
    selected = (args.profile, args.library_node, args.account_state)
    if not any(selected):
        return create_app(extractor=guest)
    if not all(selected) or args.profile == "guest" or not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", args.profile):
        raise ValueError("Provide --profile, --library-node and --account-state together for an OAuth account.")
    # A typo must not create an empty private store and run an anonymous probe.
    if not (args.account_state / "oauth-profiles.json").is_file():
        raise ValueError("The explicitly selected companion OAuth store is unavailable.")
    from prosperotube.device_auth import OAuthProfiles
    from prosperotube.extractor import ProfileExtractors
    from prosperotube.library import LibraryService
    try:
        store = OAuthProfiles(args.account_state)
        if not any(p["id"] == args.profile for p in store.profiles):
            raise ValueError("Select an OAuth profile in the explicitly supplied companion store.")
        library = LibraryService(args.library_node, oauth_store=store)
    except (OSError, ValueError, KeyError):
        raise ValueError("The selected companion OAuth account configuration is unavailable or invalid.") from None
    return create_app(extractor=ProfileExtractors(guest, lambda: library.profiles, library), library=library)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--video", required=True, help="Public eleven-character YouTube video ID")
    parser.add_argument("--codec", choices=("avc", "vp9"), default="avc")
    parser.add_argument("--height", type=int, choices=(144, 240, 360, 480, 720, 1080), default=720)
    parser.add_argument("--transport", choices=("any", "hls", "dash"), default="any", help="Require a specific returned transport")
    parser.add_argument("--expect-live", action="store_true", help="Fail unless the selected stream is currently live")
    parser.add_argument("--js-runtime", choices=("deno", "node", "bun", "quickjs"), required=True)
    parser.add_argument("--js-runtime-path")
    parser.add_argument("--profile", help="Explicit authorized OAuth profile ID; never falls back to Guest")
    parser.add_argument("--library-node", help="Node.js executable for account preflight/refresh")
    parser.add_argument("--account-state", type=Path, help="Explicit companion private OAuth state directory")
    parser.add_argument("--output", type=Path, default=Path("output/validation/public-youtube.json"))
    args = parser.parse_args()
    try:
        identifier = video_id(args.video)
    except ServiceError as error:
        parser.error(error.message)
    try:
        app = probe_app(args)
    except ValueError as error:
        parser.error(str(error))
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    base = f"http://127.0.0.1:{listener.getsockname()[1]}"
    server = uvicorn.Server(uvicorn.Config(app, log_level="critical", access_log=False))
    thread = threading.Thread(target=server.run, kwargs={"sockets": [listener]}, daemon=True)
    result = {"id": identifier, "codec": args.codec, "height_limit": args.height,
              "scope": ("OAuth TV YouTube" if args.profile else "public YouTube") +
                       " -> authenticated companion -> relay -> host FFmpeg",
              "account_profile_selected": bool(args.profile), "console_traffic": False,
              "result": "failed"}
    stage = "startup"
    token = ""

    def request(path, data):
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = "Bearer " + token
        req = urllib.request.Request(base + path, data=json.dumps(data).encode(), headers=headers)
        with urllib.request.urlopen(req, timeout=70) as response:
            return json.load(response)

    try:
        thread.start()
        deadline = time.monotonic() + 10
        while not server.started:
            if not thread.is_alive() or time.monotonic() >= deadline:
                raise RuntimeError("server_start_failed")
            time.sleep(.01)
        stage = "pairing"
        token = request("/v1/pair", {"code": app.state.pairing_code})["token"]
        stage = "resolve"
        playback = request("/v1/resolve", {"id": identifier, "codec": args.codec, "max_height": args.height,
                                            "profile": args.profile or "guest"})
        choices = [choice for choice in playback["choices"] if args.transport == "any" or choice["container"] == args.transport]
        if not choices:
            raise RuntimeError("requested_transport_missing")
        choice = choices[0]
        live = choice.get("is_live") is True
        result["is_live"] = live
        if args.expect_live and not live:
            raise RuntimeError("expected_live_stream_missing")
        result["transport"] = choice["container"]
        stage = "relay_probe"
        probe = subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-of", "json", choice["url"]],
                               capture_output=True, text=True, timeout=45, check=True)
        streams = json.loads(probe.stdout)["streams"]
        if not {"audio", "video"}.issubset({stream["codec_type"] for stream in streams}):
            raise RuntimeError("audio_video_streams_missing")
        result["streams"] = [{"type": stream["codec_type"], "codec": stream["codec_name"]}
                             for stream in streams]
        offsets = (0,) if live else (0, min(4, max(0, playback.get("duration_sec", 0) - 2)))
        for offset in offsets:
            stage = "decode" if offset == 0 else "seek_decode"
            command = ["ffmpeg", "-v", "error"] + (["-ss", str(offset)] if offset else [])
            command += ["-i", choice["url"], "-t", "2", "-map", "0:v", "-map", "0:a", "-f", "null", "-"]
            subprocess.run(command, capture_output=True, timeout=45, check=True)
        result["result"] = "pass"
        result["checks"] = ["pair", "resolve", "relay_probe", "two_second_audio_video_decode"]
        result["checks"] += ["live_edge_decode"] if live else ["seek_decode"]
    except urllib.error.HTTPError as error:
        result.update(stage=stage, status=error.code)
        # Only our API's stable machine code is persisted. Never copy error
        # text, upstream URLs, request headers or raw FFmpeg diagnostics.
        try:
            code = json.loads(error.read(8192)).get("error", {}).get("code", "http_error")
            result["code"] = code if isinstance(code, str) and code.replace("_", "").isalnum() else "http_error"
        except (ValueError, AttributeError):
            result["code"] = "http_error"
    except subprocess.CalledProcessError as error:
        result.update(stage=stage, code="ffmpeg_failed", exit_code=error.returncode)
    except subprocess.TimeoutExpired:
        result.update(stage=stage, code="ffmpeg_timeout")
    except (RuntimeError, KeyError, ValueError, OSError):
        result.update(stage=stage, code="probe_failed")
    finally:
        server.should_exit = True
        thread.join(timeout=10)
        listener.close()
        if thread.is_alive():
            result.update(result="failed", stage="cleanup", code="server_shutdown_timeout")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["result"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
