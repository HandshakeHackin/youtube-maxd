"""The worker's `description` contract served from one yt-dlp video extraction."""
import re

from .models import CHANNEL_ID, ServiceError, text

LIMIT = 20000  # UTF-16 code units, as in the worker


def bounded(value):
    """Cut on a code-point boundary so the text never ends inside a surrogate pair."""
    value = value.replace("\x00", "") if isinstance(value, str) else ""
    units, end = 0, len(value)
    for index, character in enumerate(value):
        units += 2 if ord(character) > 0xFFFF else 1
        if units > LIMIT:
            end = index
            break
    return value[:end], end < len(value)


def describe(info, identifier):
    if not isinstance(info, dict) or info.get("id") != identifier:
        raise ServiceError("upstream_format", "YouTube returned a description for a different video.", 502)
    description, truncated = bounded(info.get("description"))
    uploaded = info.get("upload_date")
    views = info.get("view_count")
    channel = info.get("channel_id")
    return {"api_version": 1, "kind": "description", "id": identifier, "title": text(info.get("title")),
            "channel": text(info.get("channel") or info.get("uploader")),
            "channel_id": channel if isinstance(channel, str) and CHANNEL_ID.fullmatch(channel) else "",
            "published": (f"{uploaded[:4]}-{uploaded[4:6]}-{uploaded[6:]}"
                          if isinstance(uploaded, str) and re.fullmatch(r"[0-9]{8}", uploaded) else ""),
            "views": str(views) if type(views) is int and 0 <= views <= 2**53 else "",
            "description": description, "truncated": truncated}
