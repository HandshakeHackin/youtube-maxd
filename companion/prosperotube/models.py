"""Bounded, stable service records; media URLs never become catalog identities."""
import math
import re
from urllib.parse import urlsplit
from pydantic import BaseModel, Field

VIDEO_ID = re.compile(r"^[A-Za-z0-9_-]{11}$")
CHANNEL_ID = re.compile(r"^UC[A-Za-z0-9_-]{22}$")
PLAYLIST_ID = re.compile(r"^[A-Za-z0-9_-]{10,100}$")


class ServiceError(Exception):
    def __init__(self, code: str, message: str, status: int = 502):
        self.code, self.message, self.status = code, message, status


def video_id(value: str) -> str:
    if not VIDEO_ID.fullmatch(value):
        raise ServiceError("invalid_video", "Enter an eleven-character YouTube video ID.", 400)
    return value


def safe_remote(url: str) -> str:
    """Resolver output is not permission to read arbitrary hosts or local files."""
    try:
        p = urlsplit(url)
        port = p.port
    except ValueError:
        raise ServiceError("unsupported_url", "The resolver returned an unsupported media URL.") from None
    host = (p.hostname or "").lower()
    domains = ("googlevideo.com", "youtube.com", "ytimg.com", "ggpht.com", "googleusercontent.com")
    if p.scheme != "https" or p.username or p.password or port not in (None, 443):
        raise ServiceError("unsupported_url", "The resolver returned an unsupported media URL.")
    if not any(host == d or host.endswith("." + d) for d in domains):
        raise ServiceError("unsupported_origin", "The resolver returned an unsupported media origin.")
    return url


def number(value, default=0.0):
    try:
        n = float(value)
        return n if math.isfinite(n) and 0 <= n <= 1_000_000_000 else default
    except (TypeError, ValueError, OverflowError):
        return default


def text(value, limit=192):
    return str(value or "").replace("\x00", "")[:limit]


class PlaylistContext(BaseModel):
    playlist: str = Field(pattern=r"^(WL|LL|[A-Za-z0-9_-]{10,100})$")
    index: int = Field(ge=0, le=100000, strict=True)


class ChannelContext(BaseModel):
    channel: str = Field(pattern=r"^UC[A-Za-z0-9_-]{22}$")
    index: int = Field(ge=0, le=100000, strict=True)


class GroupContext(BaseModel):
    group: str = Field(pattern=r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$")
    index: int = Field(ge=0, le=4095, strict=True)


class Item(BaseModel):
    id: str
    title: str
    subtitle: str = ""
    overview: str = ""
    art_url: str = ""
    channel_id: str = ""
    duration_sec: int = 0
    is_folder: bool = False
    is_live: bool = False
    resume_pos_sec: int = 0
    played: bool = False
    playback_context: PlaylistContext | ChannelContext | GroupContext | None = None


class Page(BaseModel):
    api_version: int = 1
    items: list[Item]
    has_more: bool = False
    page: int = 0


class ResolveRequest(BaseModel):
    id: str = Field(min_length=11, max_length=11)
    profile: str = Field(default="guest", pattern="^[A-Za-z0-9_-]{1,32}$")
    max_height: int = Field(default=1080, ge=144, le=2160)
    max_fps: int = Field(default=0, ge=0, le=60, strict=True)
    codec: str = Field(default="auto", pattern="^(auto|avc|vp9|av1|hevc)$")
    caption_language: str = Field(default="en", max_length=16, pattern="^[A-Za-z0-9_-]*$")
    audio_language: str = Field(default="", max_length=16, pattern="^[A-Za-z0-9_-]*$")
    automatic_captions: bool = False
    fresh: bool = Field(default=False, strict=True)


class Choice(BaseModel):
    url: str
    label: str
    container: str
    video_codec: str = ""
    audio_codec: str = ""
    width: int = 0
    height: int = 0
    fps: float = 0
    bitrate_bps: int = 0
    is_live: bool = False
    video_range: str = "SDR"
    audio_channels: int = 0


class Caption(BaseModel):
    language: str
    name: str
    url: str
    automatic: bool = False


class Playback(BaseModel):
    api_version: int = 1
    id: str
    title: str
    duration_sec: float
    expires_in: int
    choices: list[Choice]
    captions: list[Caption] = Field(default_factory=list)
    chapters: list[dict] = Field(default_factory=list)
    paid_content_text: str = Field(default="", max_length=240)
