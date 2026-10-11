"""Card thumbnails taken from the start, middle or end of the video (SmartTube hq1/hq2/hq3)."""
from urllib.parse import urlsplit

from .models import VIDEO_ID

FRAMES = {"start": "hq1", "middle": "hq2", "end": "hq3"}


def frame_url(identifier, source):
    # A signed size query belongs to the original file name, so it is not kept.
    return f"https://i.ytimg.com/vi/{identifier}/{FRAMES[source]}.jpg"


def apply(result, source):
    """Rewrite video `art_url`s on a copy; live, upcoming and non-YouTube images keep theirs."""
    if source not in FRAMES:
        return result
    data = result.model_dump() if hasattr(result, "model_dump") else dict(result)
    if data.get("kind") not in (None, "catalog"):
        return data
    rows = data.get("items")
    single = not isinstance(rows, list)
    if single:
        rows = [data["item"]] if isinstance(data.get("item"), dict) else []
    copies = []
    for row in rows[:4096]:
        if (isinstance(row, dict) and not row.get("is_folder") and not row.get("is_live")
                and not row.get("is_upcoming") and VIDEO_ID.fullmatch(str(row.get("id", "")))):
            try:
                host = (urlsplit(str(row.get("art_url", ""))).hostname or "").lower()
            except ValueError:
                host = ""
            if host == "ytimg.com" or host.endswith(".ytimg.com"):
                row = {**row, "art_url": frame_url(row["id"], source)}
        copies.append(row)
    if single:
        if copies:
            data["item"] = copies[0]
    else:
        data["items"] = copies
    return data
