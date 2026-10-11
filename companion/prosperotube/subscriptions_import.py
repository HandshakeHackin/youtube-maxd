"""Subscription exports of NewPipe, PocketTube and GrayJay as a bounded channel list. Pure parsing."""
import json
import re
from urllib.parse import urlsplit

from .models import CHANNEL_ID, ServiceError, text

MAX_BYTES = 1024 * 1024
MAX_CHANNELS = 2048


def invalid():
    return ServiceError("invalid_import", "This is not a NewPipe, PocketTube or GrayJay subscriptions export.", 400)


def channel_id(value):
    """A channel ID, or the ID inside a youtube.com/channel/UC... address; handles and user names are not resolved."""
    if not isinstance(value, str) or len(value) > 2048:
        return ""
    if CHANNEL_ID.fullmatch(value):
        return value
    try:
        parts = urlsplit(value.strip())
        host = (parts.hostname or "").lower()
    except ValueError:
        return ""
    match = re.fullmatch(r"/channel/(UC[A-Za-z0-9_-]{22})/?", parts.path)
    if parts.scheme in ("http", "https") and (host == "youtube.com" or host.endswith(".youtube.com")) and match:
        return match[1]
    return ""


def newpipe(data):
    """{"subscriptions": [{"service_id": 0, "url": ".../channel/UC...", "name": "..."}]}; service 0 is YouTube."""
    rows = data.get("subscriptions") if isinstance(data, dict) else None
    if not isinstance(rows, list):
        return None
    return [(row.get("url") if isinstance(row, dict) and row.get("service_id", 0) == 0 else None,
             row.get("name") if isinstance(row, dict) else "", "") for row in rows]


def grayjay(data):
    """A list of channel addresses, or of subscription records (objects or JSON strings) with a channel address."""
    if not isinstance(data, list):
        return None
    rows = []
    for row in data:
        if isinstance(row, str) and row.lstrip().startswith("{") and len(row) <= 65536:
            try:
                row = json.loads(row)
            except (ValueError, RecursionError):
                row = None
        if isinstance(row, dict):
            channel = row.get("channel") if isinstance(row.get("channel"), dict) else row
            rows.append((channel.get("url"), channel.get("name"), ""))
        else:
            rows.append((row, "", ""))
    return rows


def pockettube(data):
    """{"Group name": ["UC...", ...], ...}; `ysc_*` keys hold the extension's own settings."""
    if not isinstance(data, dict):
        return None
    groups = {key: value for key, value in data.items() if not key.startswith("ysc_")}
    if not groups or not all(isinstance(value, list) for value in groups.values()):
        return None
    return [(entry, "", group) for group, entries in groups.items() for entry in entries]


def parse(raw):
    """Return {"format", "channels": [{"channel_id", "title", "group"?}], "skipped", "truncated"}."""
    if isinstance(raw, bytes):
        if len(raw) > MAX_BYTES:
            raise ServiceError("import_too_large", "The subscriptions file exceeds one mebibyte.", 413)
        try:
            raw = raw.decode("utf-8-sig")
        except UnicodeError:
            raise invalid() from None
    if not isinstance(raw, str) or len(raw) > MAX_BYTES:
        raise invalid()
    try:
        data = json.loads(raw)
    except (ValueError, RecursionError):
        raise invalid() from None
    for name, reader in (("newpipe", newpipe), ("grayjay", grayjay), ("pockettube", pockettube)):
        rows = reader(data)
        if rows is not None:
            break
    else:
        raise invalid()
    channels, seen, skipped = [], set(), 0
    for address, title, group in rows:
        identifier = channel_id(address)
        if not identifier:
            skipped += 1
        elif identifier not in seen:
            seen.add(identifier)
            channels.append({"channel_id": identifier, "title": text(title).strip() if isinstance(title, str) else "",
                             **({"group": text(group, 64)} if group else {})})
    truncated = len(channels) > MAX_CHANNELS
    if not channels and name == "pockettube":  # structural detection alone is too weak
        raise invalid()
    return {"api_version": 1, "kind": "subscriptions_import", "format": name,
            "channels": channels[:MAX_CHANNELS], "skipped": skipped, "truncated": truncated}
