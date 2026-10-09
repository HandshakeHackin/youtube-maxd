"""Short-lived relay capabilities and standard DASH packaging, without transcoding."""
from dataclasses import dataclass, field
import secrets
import time
import xml.etree.ElementTree as ET
from .models import Caption, Choice, Playback, ResolveRequest, ServiceError, number, safe_remote, text

DASH = "urn:mpeg:dash:schema:mpd:2011"
ET.register_namespace("", DASH)


@dataclass
class Remote:
    url: str
    headers: dict
    mime: str


@dataclass
class Session:
    token: str
    video_id: str
    deadline: float
    tracks: dict[str, Remote] = field(default_factory=dict)
    manifests: dict[str, str] = field(default_factory=dict)


def codec_name(raw):
    if raw.startswith(("avc1", "h264")): return "h264"
    if raw.startswith(("vp9", "vp09")): return "vp9"
    if raw.startswith(("mp4a", "aac")): return "aac"
    if raw.startswith("opus"): return "opus"
    return ""


def dash_manifest(duration, video, audio, video_url, audio_url):
    duration = max(number(duration), 0.001)
    def el(parent, tag, **attrs):
        return ET.SubElement(parent, f"{{{DASH}}}{tag}", {k: str(v) for k, v in attrs.items()})
    root = ET.Element(f"{{{DASH}}}MPD", {
        "type": "static", "mediaPresentationDuration": f"PT{duration:.3f}S",
        "minBufferTime": "PT1S", "profiles": "urn:mpeg:dash:profile:isoff-on-demand:2011"})
    period = el(root, "Period", duration=f"PT{duration:.3f}S")
    for kind, fmt, url in (("video", video, video_url), ("audio", audio, audio_url)):
        webm = fmt.get("ext") == "webm"
        mime = f"{kind}/{'webm' if webm else 'mp4'}"
        codec = fmt.get("vcodec" if kind == "video" else "acodec", "")
        adapt = el(period, "AdaptationSet", contentType=kind, mimeType=mime, codecs=codec)
        attrs = {"id": kind, "bandwidth": max(1, int(number(fmt.get("tbr"), 128) * 1000))}
        if kind == "video":
            attrs.update(width=int(number(fmt.get("width"))), height=int(number(fmt.get("height"))))
        else:
            attrs["audioSamplingRate"] = int(number(fmt.get("asr"), 48000))
            if fmt.get("language"):
                adapt.set("lang", text(fmt["language"], 16))
        rep = el(adapt, "Representation", **attrs)
        el(rep, "BaseURL").text = url
        # Each representation is one seekable complete MP4/WebM segment.
        # Host decoding and seeking must be tested; this is not SABR.
        segments = el(rep, "SegmentList", duration=max(1, int(duration * 1000)), timescale=1000)
        el(segments, "SegmentURL", media=url)
    return ET.tostring(root, encoding="unicode", xml_declaration=True)


class Sessions:
    def __init__(self, max_sessions=32, ttl=1800):
        self.sessions = {}
        self.max_sessions, self.ttl = max_sessions, ttl

    def get(self, token):
        session = self.sessions.get(token)
        if not session or session.deadline <= time.monotonic():
            self.sessions.pop(token, None)
            raise ServiceError("expired_session", "Playback information expired. Select the video again to refresh it.", 410)
        return session

    def create(self, info, request: ResolveRequest, base):
        now = time.monotonic()
        for key in list(self.sessions):
            if self.sessions[key].deadline <= now:
                del self.sessions[key]
        if len(self.sessions) >= self.max_sessions:
            raise ServiceError("busy", "The companion has too many active playback sessions. Retry shortly.", 503)
        session = Session(secrets.token_urlsafe(24), request.id, now + self.ttl)
        prefix = f"{base.rstrip('/')}/media/{session.token}"
        formats = info.get("formats") or []
        live = info.get("is_live") is True
        duration = number(info.get("duration"))
        choices = []
        headers = info.get("http_headers") or {}

        def remote(key, fmt, mime):
            url = safe_remote(fmt["url"])
            supplied = {**headers, **(fmt.get("http_headers") or {})}
            # Authentication is sent to the resolver's validated origin only.
            allowed = {k: str(v) for k, v in supplied.items() if k.lower() in
                       {"user-agent", "referer", "origin", "cookie", "accept", "accept-language"}
                       and "\r" not in str(v) and "\n" not in str(v)}
            session.tracks[key] = Remote(url, allowed, mime)
            return f"{prefix}/{key}"

        def eligible(fmt):
            raw = str(fmt.get("vcodec", ""))
            codec = codec_name(raw)
            return (fmt.get("url") and codec in {"h264", "vp9"}
                    and (request.codec == "auto" or codec == {"avc": "h264", "vp9": "vp9"}.get(request.codec))
                    and 0 < number(fmt.get("height")) <= request.max_height
                    and fmt.get("dynamic_range", "SDR") in (None, "SDR")
                    and not fmt.get("has_drm"))

        videos = sorted((f for f in formats if eligible(f)),
                        key=lambda f: (number(f.get("height")), codec_name(str(f.get("vcodec", ""))) == "h264",
                                       number(f.get("fps")), number(f.get("tbr"))), reverse=True)
        audios = [f for f in formats if f.get("url") and f.get("vcodec") == "none"
                  and codec_name(str(f.get("acodec", ""))) in {"aac", "opus"}
                  and f.get("protocol") in ("http", "https") and not f.get("has_drm")]
        audios.sort(key=lambda f: (bool(request.audio_language) and f.get("language") == request.audio_language,
                                   codec_name(str(f.get("acodec", ""))) == "aac", number(f.get("abr"))), reverse=True)
        seen = set()
        for fmt in videos:
            vcodec = codec_name(str(fmt.get("vcodec", "")))
            acodec = codec_name(str(fmt.get("acodec", "")))
            height = int(number(fmt.get("height")))
            protocol = fmt.get("protocol")
            key = (height, vcodec, bool(acodec), protocol)
            if key in seen or len(choices) >= 8:
                continue
            if protocol in ("m3u8", "m3u8_native") and live and acodec:
                url = remote(f"h{len(choices)}.m3u8", fmt, "application/vnd.apple.mpegurl")
                container = "hls"
            elif protocol in ("http", "https") and acodec:
                url = remote(f"p{len(choices)}", fmt, "video/mp4" if fmt.get("ext") == "mp4" else "video/webm")
                container = text(fmt.get("ext"), 16)
            elif protocol in ("http", "https") and not live and audios and duration:
                audio = audios[0]
                acodec = codec_name(str(audio.get("acodec", "")))
                stem = f"d{len(choices)}"
                vu = remote(stem + "v", fmt, "video/webm" if fmt.get("ext") == "webm" else "video/mp4")
                au = remote(stem + "a", audio, "audio/webm" if audio.get("ext") == "webm" else "audio/mp4")
                session.manifests[stem + ".mpd"] = dash_manifest(duration, fmt, audio, vu, au)
                url, container = f"{prefix}/{stem}.mpd", "dash"
            else:
                continue
            seen.add(key)
            choices.append(Choice(url=url, label=f"{height}p {vcodec.upper()} / {acodec.upper()}",
                                  container=container, video_codec=vcodec, audio_codec=acodec,
                                  width=int(number(fmt.get("width"))), height=height,
                                  bitrate_bps=int(number(fmt.get("tbr")) * 1000), is_live=live,
                                  audio_channels=2))
        if not choices:
            raise ServiceError("unsupported_media", "No compatible AVC/VP9 SDR stream was returned. This video may require a different runtime, token, transport, or quality setting.", 422)
        captions = []
        subtitles = dict(info.get("subtitles") or {})
        automatic = info.get("automatic_captions") or {}
        if request.automatic_captions:
            for lang, tracks in automatic.items():
                subtitles.setdefault(lang, tracks)
        languages = sorted(subtitles, key=lambda lang: (lang != request.caption_language, lang))
        for lang in languages[:32]:
            track = next((t for t in subtitles[lang] if t.get("ext") == "vtt" and t.get("url")), None)
            if not track:
                continue
            try:
                url = remote(f"c{len(captions)}.srt", track, "text/vtt; charset=utf-8")
            except ServiceError:
                continue
            captions.append(Caption(language=text(lang, 16), name=text(track.get("name") or lang, 64),
                                    url=url, automatic=lang not in (info.get("subtitles") or {})))
        self.sessions[session.token] = session
        chapters = [{"title": text(c.get("title"), 96), "start": number(c.get("start_time")),
                     "end": number(c.get("end_time"))} for c in (info.get("chapters") or [])[:128]]
        return Playback(id=request.id, title=text(info.get("title")), duration_sec=duration,
                        expires_in=self.ttl, choices=choices, captions=captions, chapters=chapters)
