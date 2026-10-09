"""Convert bounded WebVTT text into the native player's SRT sidecar format."""
import html
import re
from .models import ServiceError

STAMP = r"(?:[0-9]{2,}:)?[0-9]{2}:[0-9]{2}\.[0-9]{3}"
TIMING = re.compile(rf"^({STAMP})\s+-->\s+({STAMP})(?:\s+.*)?$")


def vtt_to_srt(raw: bytes) -> str:
    if len(raw) > 2 * 1024 * 1024:
        raise ServiceError("captions_too_large", "The caption file exceeded its size limit.")
    try:
        source = raw.decode("utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
    except UnicodeError:
        raise ServiceError("invalid_captions", "The caption file is not valid UTF-8.") from None
    lines = source.split("\n")
    result, i = [], 0
    while i < len(lines):
        match = TIMING.fullmatch(lines[i].strip())
        i += 1
        if not match:
            continue
        body = []
        while i < len(lines) and lines[i].strip():
            # WebVTT inline word timestamps and style tags are not visible text.
            body.append(html.unescape(re.sub(r"<[^>]*>", "", lines[i])).strip())
            i += 1
        if not any(body):
            continue
        def stamp(s):
            if s.count(":") == 1:
                s = "00:" + s
            return s.replace(".", ",")
        result.append(f"{len(result) + 1}\n{stamp(match[1])} --> {stamp(match[2])}\n" + "\n".join(body))
        if len(result) > 50000:
            raise ServiceError("captions_limit", "The caption file contains too many cues.")
    return "\n\n".join(result) + ("\n" if result else "")
