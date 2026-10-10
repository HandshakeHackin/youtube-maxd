"""Explicit public or configured OAuth YouTube API/relay probe; no console traffic."""
import argparse
import asyncio
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
import httpx
from prosperotube.app import create_app
from prosperotube.extractor import Extractor
from prosperotube.models import ServiceError, video_id


class ExpireOnceTransport(httpx.AsyncBaseTransport):
    """Inject one local 403 after a real successful probe; never alter YouTube."""
    def __init__(self):
        self.inner = httpx.AsyncHTTPTransport()
        self.fail_url = None
        self.injections = 0
        self.matching_requests = 0
        self.inject_on_request = 1

    async def handle_async_request(self, request):
        if self.fail_url is not None and str(request.url) == self.fail_url:
            self.matching_requests += 1
            if self.matching_requests == self.inject_on_request:
                self.fail_url = None
                self.injections += 1
                return httpx.Response(403, request=request)
        return await self.inner.handle_async_request(request)

    async def aclose(self):
        await self.inner.aclose()


def decoded_seconds(progress, required):
    """Reject successful early exits; persist only the final decoded timestamp."""
    text = progress.decode('ascii', errors='strict')
    records = text.strip().split('progress=')
    if len(records) < 2 or records[-1].strip() != 'end':
        raise RuntimeError('decode_progress_missing')
    times = re.findall(r'^out_time_us=([0-9]{1,16})$', records[-2], re.MULTILINE)
    if len(times) != 1:
        raise RuntimeError('decode_progress_invalid')
    seconds = int(times[0]) / 1_000_000
    if seconds < required - 0.25:
        raise RuntimeError('decode_ended_early')
    return round(seconds, 3)


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
    simulation = parser.add_mutually_exclusive_group()
    simulation.add_argument("--simulate-vod-expiry", action="store_true", help="Inject one local expired-link response after real ffprobe, then require fresh resolution and decode")
    simulation.add_argument("--simulate-hls-expiry", action="store_true", help="Inject one local live-root expiry after real ffprobe, then require fresh paired roots and live-edge decode")
    parser.add_argument("--hls-expiry-after-requests", type=int, choices=range(1, 101), default=1,
                        metavar="1..100", help="Inject on this root request during decode; requires --simulate-hls-expiry")
    parser.add_argument("--decode-seconds", type=int, choices=range(2, 3601), default=2,
                        metavar="2..3600", help="Required decoded duration for each decode/seek case")
    parser.add_argument("--js-runtime", choices=("deno", "node", "bun", "quickjs"), required=True)
    parser.add_argument("--js-runtime-path")
    parser.add_argument("--profile", help="Explicit authorized OAuth profile ID; never falls back to Guest")
    parser.add_argument("--library-node", help="Node.js executable for account preflight/refresh")
    parser.add_argument("--account-state", type=Path, help="Explicit companion private OAuth state directory")
    parser.add_argument("--output", type=Path, default=Path("output/validation/public-youtube.json"))
    args = parser.parse_args()
    if args.hls_expiry_after_requests != 1 and not args.simulate_hls_expiry:
        parser.error('--hls-expiry-after-requests requires --simulate-hls-expiry')
    try:
        identifier = video_id(args.video)
    except ServiceError as error:
        parser.error(error.message)
    try:
        app = probe_app(args)
    except ValueError as error:
        parser.error(str(error))
    expiry = None
    fresh_resolutions = [0]
    if args.simulate_vod_expiry or args.simulate_hls_expiry:
        asyncio.run(app.state.http.aclose())  # Unused client; no transport was opened.
        expiry = ExpireOnceTransport()
        app.state.http = httpx.AsyncClient(transport=expiry, timeout=httpx.Timeout(20, connect=5), follow_redirects=False)
        extractor = app.state.extractor
        scoped = extractor.for_profile(args.profile or 'guest') if hasattr(extractor, 'for_profile') else extractor
        original_extract = scoped.extract
        async def tracked_extract(target, **kwargs):
            if kwargs.get('fresh'):
                fresh_resolutions[0] += 1
            return await original_extract(target, **kwargs)
        scoped.extract = tracked_extract
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    base = f"http://127.0.0.1:{listener.getsockname()[1]}"
    server = uvicorn.Server(uvicorn.Config(app, log_level="critical", access_log=False))
    thread = threading.Thread(target=server.run, kwargs={"sockets": [listener]}, daemon=True)
    result = {"id": identifier, "codec": args.codec, "height_limit": args.height,
              "scope": ("OAuth TV YouTube" if args.profile else "public YouTube") +
                       " -> authenticated companion -> relay -> host FFmpeg",
              "account_profile_selected": bool(args.profile), "console_traffic": False,
              "simulated_media_expiry": args.simulate_vod_expiry or args.simulate_hls_expiry,
              "simulated_expiry_kind": 'live_hls_root' if args.simulate_hls_expiry else 'vod' if args.simulate_vod_expiry else None,
              "required_decode_seconds": args.decode_seconds,
              "decoded_seconds": [],
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
        if expiry is not None:
            stage = 'simulate_expiry'
            token_part = choice['url'].split('/media/', 1)[1].split('/', 1)[0]
            session = app.state.sessions.get(token_part)
            if args.simulate_hls_expiry:
                if not live or choice['container'] != 'hls':
                    raise RuntimeError('live_hls_required_for_expiry_probe')
                root = choice['url'].split('/media/', 1)[1].split('/', 1)[1]
                paired_video = root.removesuffix('.m3u8') + 'v.m3u8'
                if paired_video in session.hls_pairs:
                    root = paired_video
                resources = [session.tracks[root]] if root in session.tracks and session.tracks[root].live_binding else []
                if not resources:
                    raise RuntimeError('live_root_identity_missing')
            else:
                if live:
                    raise RuntimeError('vod_required_for_expiry_probe')
                resources = [r for r in session.tracks.values() if r.binding and r.binding[3] != 'none'
                             and r.representation.observed]
                if not resources or resources[0].representation.validator is None:
                    result['code'] = 'media_identity_missing'
                    raise RuntimeError('media_identity_missing')
            expiry.fail_url = resources[0].url
            expiry.inject_on_request = args.hls_expiry_after_requests
        offsets = (0,) if live else (0, min(4, max(0, playback.get("duration_sec", 0) - args.decode_seconds)))
        for offset in offsets:
            stage = "decode" if offset == 0 else "seek_decode"
            result.setdefault("decode_offsets_seconds", []).append(offset)
            command = ["ffmpeg", "-v", "error"] + (["-ss", str(offset)] if offset else [])
            command += ["-i", choice["url"], "-t", str(args.decode_seconds), "-map", "0:v", "-map", "0:a",
                        "-progress", "pipe:1", "-nostats", "-f", "null", "-"]
            decoded = subprocess.run(command, capture_output=True, timeout=max(45, args.decode_seconds * 2 + 60), check=True)
            result['decoded_seconds'].append(decoded_seconds(decoded.stdout, args.decode_seconds))
        if expiry is not None and (expiry.injections != 1 or fresh_resolutions[0] != 1):
            stage = 'renewal_assertion'
            raise RuntimeError('renewal_not_exercised_once')
        result["result"] = "pass"
        result["checks"] = ["pair", "resolve", "relay_probe",
                            "two_second_audio_video_decode" if args.decode_seconds == 2 else "sustained_audio_video_decode"]
        if live:
            result["checks"].append("live_edge_decode")
        elif any(offset > 0 for offset in offsets):
            result["checks"].append("seek_decode")
        else:
            result["checks"].append("vod_decode_without_seek")
        if expiry is not None:
            result['checks'].append('injected_expiry_fresh_resolution_same_relay_decode')
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
        result.update(stage=stage, code=result.get('code', 'probe_failed'))
    finally:
        server.should_exit = True
        thread.join(timeout=10)
        listener.close()
        if expiry is not None:
            result.update(expiry_injections=expiry.injections, fresh_resolutions=fresh_resolutions[0],
                          expiry_root_requests=expiry.matching_requests,
                          expiry_inject_on_request=expiry.inject_on_request)
        if thread.is_alive():
            result.update(result="failed", stage="cleanup", code="server_shutdown_timeout")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["result"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
