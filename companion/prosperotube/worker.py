"""No raw yt-dlp messages, cookies, or signed URLs are written to diagnostics."""
import contextlib
import json
import sys


def install_profile_cookies(ydl, header):
    """Header exports are scoped to YouTube; never use a global Cookie header."""
    import http.cookiejar
    from http.cookies import SimpleCookie
    if not isinstance(header, str) or len(header) > 65536 or "\r" in header or "\n" in header:
        raise ValueError("invalid profile cookies")
    parsed = SimpleCookie()
    parsed.load(header)
    if not parsed:
        raise ValueError("empty profile cookies")
    for name, morsel in parsed.items():
        ydl.cookiejar.set_cookie(http.cookiejar.Cookie(
            version=0, name=name, value=morsel.value, port=None, port_specified=False,
            domain=".youtube.com", domain_specified=True, domain_initial_dot=True,
            path="/", path_specified=True, secure=True, expires=None, discard=True,
            comment=None, comment_url=None, rest={}))


def attach_media_cookies(ydl, info):
    """Retain only URL-scoped media cookies on the private worker boundary."""
    from .models import ServiceError, safe_remote
    # A top-level webpage URL must not inject its cookies into every format.
    records = list(info.get("formats") or [])
    for tracks in (info.get("subtitles") or {}, info.get("automatic_captions") or {}):
        records.extend(record for values in tracks.values() for record in values)
    for record in records:
        if not isinstance(record, dict) or not record.get("url"):
            continue
        try:
            url = safe_remote(record["url"])
        except ServiceError:
            continue
        cookie = ydl.cookiejar.get_cookie_header(url)
        if cookie:
            record["http_headers"] = {**(record.get("http_headers") or {}), "Cookie": cookie}


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
                if request.get("oauth_access_token") is not None:
                    if request.get("cookies") or request.get("cookie_header") or request["flat"]:
                        raise ValueError("invalid OAuth extraction context")
                    from .oauth_video import TVOAuthYoutubeIE
                    ydl.add_info_extractor(TVOAuthYoutubeIE(request["oauth_access_token"]))
                if request.get("cookie_header") is not None:
                    install_profile_cookies(ydl, request["cookie_header"])
                info = ydl.extract_info(request["target"], download=False)
                attach_media_cookies(ydl, info)
                result = ydl.sanitize_info(info)
                if request.get("oauth_access_token") is not None:
                    from .oauth_video import strip_oauth_headers
                    strip_oauth_headers(result)
        json.dump(result, sys.stdout, ensure_ascii=False)
    except Exception as error:
        from .resolver_errors import classify_error
        json.dump({"worker_error": {"version": 1, "code": classify_error(error)}}, sys.stdout)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
