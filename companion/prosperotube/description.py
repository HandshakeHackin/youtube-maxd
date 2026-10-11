"""The worker's `description` contract served from one yt-dlp video extraction."""
import re

from .models import CHANNEL_ID, ServiceError, number, text

LIMIT = 20000  # UTF-16 code units, as in the worker
EXCERPT = 600  # the details-screen excerpt


def bounded(value, limit=LIMIT):
    """Cut on a code-point boundary so the text never ends inside a surrogate pair."""
    value = value.replace("\x00", "") if isinstance(value, str) else ""
    units, end = 0, len(value)
    for index, character in enumerate(value):
        units += 2 if ord(character) > 0xFFFF else 1
        if units > limit:
            end = index
            break
    return value[:end], end < len(value)


def count(value):
    return str(value) if type(value) is int and 0 <= value <= 2**53 else ""


def describe(info, identifier, limit=LIMIT):
    if not isinstance(info, dict) or info.get("id") != identifier:
        raise ServiceError("upstream_format", "YouTube returned a description for a different video.", 502)
    description, truncated = bounded(info.get("description"), limit)
    uploaded = info.get("upload_date")
    channel = info.get("channel_id")
    return {"api_version": 1, "kind": "description", "id": identifier, "title": text(info.get("title")),
            "channel": text(info.get("channel") or info.get("uploader")),
            "channel_id": channel if isinstance(channel, str) and CHANNEL_ID.fullmatch(channel) else "",
            "published": (f"{uploaded[:4]}-{uploaded[4:6]}-{uploaded[6:]}"
                          if isinstance(uploaded, str) and re.fullmatch(r"[0-9]{8}", uploaded) else ""),
            "views": count(info.get("view_count")), "like_count": count(info.get("like_count")),
            "description": description, "truncated": truncated}


def details(info, identifier):
    """The worker's `video_details` contract from the same extraction (no avatar, no related list)."""
    base = describe(info, identifier, EXCERPT)
    formats = info.get("formats") if isinstance(info.get("formats"), list) else []
    badges = [name for name, present in (
        ("live", info.get("is_live") is True or info.get("live_status") == "is_live"),
        ("upcoming", info.get("live_status") == "is_upcoming"),
        ("4k", any(isinstance(f, dict) and number(f.get("height")) >= 2160 for f in formats[:4096]))) if present]
    return {"api_version": 1, "kind": "video_details", "id": identifier, "title": base["title"],
            "channel": {"id": base["channel_id"], "name": base["channel"],
                        "subscribers": count(info.get("channel_follower_count")), "art_url": ""},
            "published": base["published"], "views": base["views"], "like_count": base["like_count"],
            "description": base["description"], "description_truncated": base["truncated"],
            "duration_sec": int(number(info.get("duration"))) if number(info.get("duration")) <= 1_000_000 else 0,
            "badges": badges, "has_related": False}
