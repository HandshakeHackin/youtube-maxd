"""TV-only yt-dlp adapter. OAuth bearer is confined to bounded InnerTube POSTs."""
import json
import re
from urllib.parse import urlsplit, urljoin

import httpx
from yt_dlp.extractor.youtube import YoutubeIE
from yt_dlp.utils import ExtractorError
from .live_hls import LiveHLSMixin
from .paid_content import PaidContentMixin


class TVOAuthYoutubeIE(PaidContentMixin, LiveHLSMixin, YoutubeIE):
    def __init__(self, token):
        super().__init__()
        if not isinstance(token, str) or not 1 <= len(token) <= 8192 or re.search(r"[^\x21-\x7e]", token):
            raise ValueError("invalid OAuth access token")
        self._tv_access_token = token

    @classmethod
    def ie_key(cls):
        return "Youtube"  # Replace this process's video extractor; no global plugin.

    @property
    def is_authenticated(self):
        return True

    def _get_requested_clients(self, url, smuggled_data, is_premium_subscriber):
        return ["tv", "tv_downgraded"]

    def _construct_player_url(self, *, player_id=None, player_url=None):
        if player_url and re.search(r"/s/player/[0-9a-fA-F]+/tv-player[^?]+\.js$", player_url):
            parsed = urlsplit(urljoin("https://www.youtube.com", player_url))
            if parsed.scheme != "https" or parsed.netloc != "www.youtube.com":
                raise ExtractorError("Invalid TV player script origin", expected=True)
            return parsed.geturl()
        player_id = player_id or self._extract_player_info(player_url)
        if not re.fullmatch(r"[0-9a-fA-F]{8,}", player_id or ""):
            raise ExtractorError("Invalid TV player identity", expected=True)
        return f"https://www.youtube.com/s/player/{player_id}/tv-player-ias.vflset/tv-player-ias.js"

    def _extract_player_url(self, *ytcfgs, webpage=None):
        tv = [cfg for cfg in ytcfgs if isinstance(cfg, dict) and
              (cfg.get("INNERTUBE_CONTEXT", {}).get("client", {}).get("clientName") == "TVHTML5"
               or "tv-player" in str(cfg.get("PLAYER_JS_URL", "")))]
        return super()._extract_player_url(*(tv or ytcfgs), webpage=webpage)

    def _extract_signature_timestamp(self, video_id, player_url, ytcfg=None, fatal=False):
        # The pinned loop can supply WEB STS when TV config retrieval fails.
        # Even a TV config can refer to a different variant than player_url.
        # Obtain STS from the exact TV script used by the signature solver.
        if player_url:
            parsed = urlsplit(player_url)
            if (parsed.scheme != "https" or parsed.netloc != "www.youtube.com"
                    or not re.fullmatch(r"/s/player/[0-9a-fA-F]{8,}/tv-player[^?]+\.js", parsed.path)
                    or parsed.query or parsed.fragment):
                raise ExtractorError("Invalid TV player script origin", expected=True)
        return super()._extract_signature_timestamp(video_id, player_url, None, fatal=fatal)

    def _get_player_js_version(self):
        # Caller-supplied WEB STS@HASH overrides cannot define TV identity.
        return None, None

    def generate_api_headers(self, *, default_client="web", **kwargs):
        headers = super().generate_api_headers(default_client=default_client, **kwargs)
        if default_client in ("tv", "tv_downgraded"):
            if headers.get("X-YouTube-Client-Name") != "7" or headers.get("Origin") != "https://www.youtube.com":
                raise ExtractorError("Invalid TV account context", expected=True)
            for name in list(headers):
                if name.lower() in ("cookie", "authorization", "x-goog-pageid", "x-goog-authuser"):
                    del headers[name]
            headers["Authorization"] = "Bearer " + self._tv_access_token
        return headers

    def _extract_player_response(self, client, *args, **kwargs):
        if client not in ("tv", "tv_downgraded"):
            raise ExtractorError("Unsupported TV player fallback", expected=True)
        return super()._extract_player_response(client, *args, **kwargs)

    def _download_json(self, url_or_request, video_id, *args, **kwargs):
        headers = kwargs.get("headers") or {}
        if not any(k.lower() == "authorization" for k in headers):
            return super()._download_json(url_or_request, video_id, *args, **kwargs)
        # Use a separate transport so yt-dlp redirect handlers cannot forward
        # the bearer beyond this precise TV endpoint, even within YouTube.
        url = urlsplit(url_or_request) if isinstance(url_or_request, str) else None
        data = kwargs.get("data")
        if (url is None or url.scheme != "https" or url.netloc != "www.youtube.com"
                or url.path not in ("/youtubei/v1/player", "/youtubei/v1/next", "/youtubei/v1/browse", "/youtubei/v1/search")
                or not isinstance(data, bytes) or len(data) > 65536
                or any(k.lower() == "cookie" for k in headers)):
            raise ExtractorError("Invalid TV request origin", expected=True)
        try:
            payload = json.loads(data)
            if payload.get("context", {}).get("client", {}).get("clientName") != "TVHTML5":
                raise ValueError()
        except (ValueError, AttributeError, TypeError):
            raise ExtractorError("Invalid TV account context", expected=True) from None
        try:
            with httpx.Client(timeout=12, follow_redirects=False, trust_env=False) as client:
                with client.stream("POST", url_or_request, headers=headers, content=data, params=kwargs.get("query")) as response:
                    if response.status_code in (401, 403):
                        raise ExtractorError("TV account login required", expected=True)
                    if response.status_code == 429:
                        raise ExtractorError("HTTP error 429", expected=True)
                    if response.status_code != 200:
                        raise ExtractorError("TV API request failed", expected=True)
                    chunks, size = [], 0
                    for chunk in response.iter_bytes():
                        size += len(chunk)
                        if size > 8 * 1024 * 1024:
                            raise ExtractorError("TV API response exceeded its size limit", expected=True)
                        chunks.append(chunk)
                    result = json.loads(b"".join(chunks))
                    if not isinstance(result, dict):
                        raise ValueError()
                    return result
        except httpx.HTTPError:
            raise ExtractorError("Unable to download API page", expected=True) from None
        except (ValueError, UnicodeError):
            raise ExtractorError("Invalid TV API response", expected=True) from None


def strip_oauth_headers(value):
    """Bearer headers are never media/caption credentials."""
    if isinstance(value, dict):
        for key in list(value):
            if key.lower() in ("authorization", "proxy-authorization", "access_token", "refresh_token"):
                del value[key]
            else:
                strip_oauth_headers(value[key])
    elif isinstance(value, list):
        for record in value:
            strip_oauth_headers(record)
