"""No raw yt-dlp messages, cookies, or signed URLs are written to diagnostics."""
import contextlib
import json
import sys


class Quiet:
    def debug(self, *_): pass
    def warning(self, *_): pass
    def error(self, *_): pass


def main():
    from yt_dlp import YoutubeDL
    request = json.load(sys.stdin)
    opts = {"quiet": True, "no_warnings": True, "logger": Quiet(), "skip_download": True,
            "extract_flat": "in_playlist" if request["flat"] else False,
            "playliststart": request["start"], "playlistend": request["end"],
            "socket_timeout": 12, "retries": 1, "extractor_retries": 1,
            "noplaylist": not request["flat"], "ignoreerrors": False,
            "extractor_args": request.get("extractor_args", {})}
    if request.get("cookies"):
        opts["cookiefile"] = request["cookies"]
    if request.get("js_runtimes") is not None:
        opts["js_runtimes"] = request["js_runtimes"]
    try:
        # The Python API's stdout is not a protocol. Keep its messages away
        # from the single structured result, even when an extractor prints.
        with contextlib.redirect_stdout(sys.stderr):
            with YoutubeDL(opts) as ydl:
                info = ydl.extract_info(request["target"], download=False)
                result = ydl.sanitize_info(info)
        json.dump(result, sys.stdout, ensure_ascii=False)
    except Exception:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
