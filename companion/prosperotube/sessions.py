"""Short-lived relay capabilities and standard DASH packaging, without transcoding."""
from dataclasses import dataclass, field
import secrets
import time
import xml.etree.ElementTree as ET
import math
from urllib.parse import parse_qs, urlsplit
from .models import Caption, Choice, Playback, ResolveRequest, ServiceError, Storyboard, number, safe_remote, text
from .media_refresh import Representation, binding, media_headers
from .description import describe

DASH = "urn:mpeg:dash:schema:mpd:2011"
ET.register_namespace("", DASH)


@dataclass
class Remote:
    url: str
    headers: dict
    mime: str
    binding: tuple | None = None
    representation: Representation = field(default_factory=Representation)
    live_binding: tuple | None = None


@dataclass
class Session:
    token: str
    video_id: str
    deadline: float
    profile: str = "guest"
    tracks: dict[str, Remote] = field(default_factory=dict)
    manifests: dict[str, str] = field(default_factory=dict)
    hls_manifests: dict[str, str] = field(default_factory=dict)
    hls_resources: dict[str, bool] = field(default_factory=dict)
    hls_playlists: dict[str, set[str]] = field(default_factory=dict)
    duration: float = 0
    is_live: bool = False
    refresh_lock: object = None
    refresh_after: float = 0
    hls_pairs: dict[str, str] = field(default_factory=dict)


def codec_name(raw):
    if raw.startswith(("avc1", "h264")): return "h264"
    if raw.startswith(("vp9", "vp09")): return "vp9"
    if raw == "av1" or raw.startswith("av01."): return "av1"
    if raw in ("hevc", "h265") or raw.startswith(("hvc1.", "hev1.")): return "hevc"
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

    def invalidate_profile(self, identity):
        self.sessions = {k: v for k, v in self.sessions.items() if v.profile != identity}

    def get(self, token):
        session = self.sessions.get(token)
        if not session or session.deadline <= time.monotonic():
            self.sessions.pop(token, None)
            raise ServiceError("expired_session", "Playback information expired. Select the video again to refresh it.", 410)
        return session

    def touch(self, session):
        # Activity renews the idle lease, but never restores an invalidated token.
        if self.sessions.get(session.token) is session:
            session.deadline = time.monotonic() + self.ttl

    def create(self, info, request: ResolveRequest, base):
        now = time.monotonic()
        for key in list(self.sessions):
            if self.sessions[key].deadline <= now:
                del self.sessions[key]
        if len(self.sessions) >= self.max_sessions:
            raise ServiceError("busy", "The companion has too many active playback sessions. Retry shortly.", 503)
        session = Session(secrets.token_urlsafe(24), request.id, now + self.ttl, profile=request.profile)
        prefix = f"{base.rstrip('/')}/media/{session.token}"
        facts = describe(info, request.id, 0)
        formats = info.get("formats") or []
        live = info.get("is_live") is True
        duration = number(info.get("duration"))
        session.duration, session.is_live = duration, live
        choices = []
        headers = info.get("http_headers") or {}

        def remote(key, fmt, mime):
            url = safe_remote(fmt["url"])
            allowed = media_headers(headers, fmt)
            session.tracks[key] = Remote(url, allowed, mime, binding=None if live else binding(fmt),
                                        live_binding=binding(fmt, live=True) if live else None)
            return f"{prefix}/{key}"

        def eligible(fmt):
            raw = str(fmt.get("vcodec", ""))
            codec = codec_name(raw)
            return (fmt.get("url") and codec in {"h264", "vp9", "av1", "hevc"}
                    and (request.codec == "auto" or codec == {"avc": "h264", "vp9": "vp9", "av1": "av1", "hevc": "hevc"}.get(request.codec))
                    and 0 < number(fmt.get("height")) <= request.max_height
                    and (not request.max_fps or 0 < number(fmt.get("fps")) <= request.max_fps)
                    and fmt.get("dynamic_range", "SDR") in (None, "SDR")
                    and not fmt.get("has_drm"))

        videos = sorted((f for f in formats if eligible(f)),
                        key=lambda f: (number(f.get("height")), {"h264": 3, "vp9": 2, "hevc": 1, "av1": 0}.get(codec_name(str(f.get("vcodec", ""))), -1),
                                       number(f.get("fps")), number(f.get("tbr"))), reverse=True)
        audios = [f for f in formats if f.get("url") and f.get("vcodec") == "none"
                  and codec_name(str(f.get("acodec", ""))) in {"aac", "opus"}
                  and f.get("protocol") in ("http", "https") and not f.get("has_drm")]
        audios.sort(key=lambda f: (bool(request.audio_language) and f.get("language") == request.audio_language,
                                   codec_name(str(f.get("acodec", ""))) == "aac", number(f.get("abr"))), reverse=True)
        hls_audios = [f for f in formats if f.get('url') and f.get('vcodec') == 'none'
                      and codec_name(str(f.get('acodec', ''))) in {'aac', 'opus'}
                      and f.get('protocol') in ('m3u8', 'm3u8_native') and not f.get('has_drm')]
        hls_audios.sort(key=lambda f: (bool(request.audio_language) and f.get('language') == request.audio_language,
                                      codec_name(str(f.get('acodec', ''))) == 'aac', number(f.get('abr'))), reverse=True)
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
            elif protocol in ('m3u8', 'm3u8_native') and live and not acodec:
                audio = next((a for a in hls_audios if a['url'] in fmt.get('hls_audio_urls', [])
                              and a.get('manifest_url') == fmt.get('manifest_url')), None)
                if audio is None:
                    continue
                acodec = codec_name(str(audio['acodec']))
                stem = f'h{len(choices)}'
                vu = remote(stem + 'v.m3u8', fmt, 'application/vnd.apple.mpegurl')
                au = remote(stem + 'a.m3u8', audio, 'application/vnd.apple.mpegurl')
                session.hls_pairs[stem + 'v.m3u8'] = stem + 'a.m3u8'
                bandwidth = max(1, int((number(fmt.get('tbr')) + number(audio.get('tbr'))) * 1000))
                session.hls_manifests[stem + '.m3u8'] = (
                    '#EXTM3U\n#EXT-X-VERSION:3\n'
                    f'#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",NAME="Audio",DEFAULT=YES,AUTOSELECT=YES,URI="{au}"\n'
                    f'#EXT-X-STREAM-INF:BANDWIDTH={bandwidth},AUDIO="audio"\n{vu}\n')
                url, container = f'{prefix}/{stem}.m3u8', 'hls'
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
            fps = max(0, number(fmt.get("fps")))
            rate_label = f" {fps:g} fps" if fps else ""
            choices.append(Choice(url=url, label=f"{height}p{rate_label} {vcodec.upper()} / {acodec.upper()}",
                                  container=container, video_codec=vcodec, audio_codec=acodec,
                                  width=int(number(fmt.get("width"))), height=height, fps=fps,
                                  bitrate_bps=int(number(fmt.get("tbr")) * 1000), is_live=live,
                                  audio_channels=2))
        if not choices:
            raise ServiceError("unsupported_media", "No compatible AVC/VP9/HEVC/AV1 SDR stream was returned. This video may require a different runtime, token, transport, or quality setting.", 422)
        captions = []
        subtitles = dict(info.get("subtitles") or {})
        automatic = info.get("automatic_captions") or {}

        def vtt(tracks):
            return next((t for t in tracks if isinstance(t, dict) and t.get("ext") == "vtt" and t.get("url")), None)

        def translated(track):
            # A machine translation is the same caption address with a target language.
            try:
                return bool(track) and "tlang" in parse_qs(urlsplit(str(track.get("url"))).query)
            except ValueError:
                return False

        def wanted_last(lang):
            # A translation of a human-made track is named `<target>-<source>`.
            wanted = request.caption_language
            return (lang != wanted, not (wanted and lang.startswith(wanted + "-")), lang)
        if request.automatic_captions:
            for lang, tracks in automatic.items():
                subtitles.setdefault(lang, tracks)
        elif request.translated_captions:
            # "More subtitles": machine translations only, at most 128 of them.
            extra = sorted((lang for lang, tracks in automatic.items()
                            if lang not in subtitles and isinstance(tracks, list) and translated(vtt(tracks))),
                           key=wanted_last)
            for lang in extra[:128]:
                subtitles[lang] = automatic[lang]
        languages = sorted(subtitles, key=wanted_last)
        for lang in languages[:256]:
            track = vtt(subtitles[lang]) if isinstance(subtitles[lang], list) else None
            if not track:
                continue
            try:
                url = remote(f"c{len(captions)}.srt", track, "text/vtt; charset=utf-8")
            except ServiceError:
                continue
            captions.append(Caption(language=text(lang, 32), name=text(track.get("name") or lang, 64),
                                    url=url, automatic=lang not in (info.get("subtitles") or {}),
                                    translated=translated(track)))
        # Seek-preview sheets are signed image URLs, so each one becomes a relay
        # capability; `$M` in the returned template is the sheet number.
        storyboards = []
        boards = [] if live or duration <= 0 else [f for f in formats if isinstance(f, dict)
                                                  and f.get("format_note") == "storyboard"]
        boards.sort(key=lambda f: number(f.get("width")) * number(f.get("height")))  # level 0 is the smallest
        for fmt in boards[:8]:
            width, height = int(number(fmt.get("width"))), int(number(fmt.get("height")))
            columns, rows = int(number(fmt.get("columns"))), int(number(fmt.get("rows")))
            fragments = fmt.get("fragments")
            count = round(number(fmt.get("fps")) * duration)
            if (len(storyboards) >= 4 or not isinstance(fragments, list) or not 1 <= len(fragments) <= 512
                    or not 1 <= width <= 1024 or not 1 <= height <= 1024 or not 1 <= columns <= 32
                    or not 1 <= rows <= 32 or count < 1
                    or math.ceil(count / (columns * rows)) != len(fragments)):
                continue
            level = len(storyboards)
            try:
                urls = [safe_remote(part["url"]) for part in fragments]
            except (ServiceError, TypeError, KeyError):
                continue
            # Signed sheet addresses need no account cookie.
            plain = {k: v for k, v in media_headers(headers, fmt).items() if k.lower() != "cookie"}
            for index, url in enumerate(urls):
                session.tracks[f"sb{level}-{index}.jpg"] = Remote(url, plain, "image/jpeg")
            storyboards.append(Storyboard(width=width, height=height, columns=columns, rows=rows, count=count,
                                          sheets=len(urls), interval_ms=max(1, round(duration * 1000 / count)),
                                          url=f"{prefix}/sb{level}-$M.jpg"))
        self.sessions[session.token] = session
        chapters = [{"title": text(c.get("title"), 96), "start": number(c.get("start_time")),
                     "end": number(c.get("end_time"))} for c in (info.get("chapters") or [])[:128]]
        from .paid_content import disclosure
        from .player_metadata import loudness
        return Playback(id=request.id, title=text(info.get("title")), duration_sec=duration,
                        channel=facts["channel"], channel_id=facts["channel_id"],
                        views=facts["views"], published=facts["published"],
                        expires_in=self.ttl, choices=choices, captions=captions, chapters=chapters,
                        paid_content_text=disclosure(info.get("paid_content_text")),
                        loudness_db=loudness(info.get("loudness_db")), is_short=info.get("media_type") == "short",
                        storyboards=storyboards)
