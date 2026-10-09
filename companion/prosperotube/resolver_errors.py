"""Allowlisted resolver failures; upstream diagnostic text never leaves the worker."""
from .models import ServiceError

ERRORS = {
    "video_unavailable": ("This YouTube video is unavailable or has been removed. Choose another video.", 404),
    "authentication_required": ("This video requires an authorized YouTube account. Select a configured account profile on the companion.", 403),
    "youtube_challenge": ("YouTube requires account verification for this connection. Check the companion's account configuration before retrying.", 403),
    "youtube_rate_limited": ("YouTube is limiting this connection. Wait before retrying.", 429),
    "resolver_network": ("The companion could not reach YouTube. Check its Internet connection and retry.", 502),
    "extraction_failed": ("YouTube could not be resolved. Check the companion's yt-dlp/runtime configuration and retry.", 502),
}


def classify_error(error):
    # yt-dlp wraps expected extractor/network errors in DownloadError. Inspect
    # them only inside this private process, then discard all raw text.
    text = str(error)[:8192].casefold()
    if "confirm you're not a bot" in text or "confirm you’re not a bot" in text:
        return "youtube_challenge"
    if "http error 429" in text or "too many requests" in text:
        return "youtube_rate_limited"
    if any(phrase in text for phrase in ("private video", "this video is private", "sign in to confirm your age",
                                        "members-only", "join this channel", "login required")):
        return "authentication_required"
    if any(phrase in text for phrase in ("this video is unavailable", "video unavailable", "video has been removed",
                                        "video is not available", "video has been deleted")):
        return "video_unavailable"
    if isinstance(error, (TimeoutError, ConnectionError)) or any(phrase in text for phrase in (
            "timed out", "connection refused", "temporary failure in name resolution",
            "name or service not known", "unable to download webpage", "unable to download api page")):
        return "resolver_network"
    return "extraction_failed"


def service_error(code):
    code = code if isinstance(code, str) and code in ERRORS else "extraction_failed"
    message, status = ERRORS[code]
    return ServiceError(code, message, status)
