"""Single-flight renewal of VOD representations and paired live HLS roots."""
import asyncio
from dataclasses import dataclass
from email.utils import parsedate_to_datetime
import re
import math
import time

from .models import ServiceError, number, safe_remote


@dataclass
class Representation:
    validator: tuple | None = None
    total: int | None = None
    observed: bool = False
    delivered: bool = False


def expired():
    return ServiceError('expired_media', 'YouTube media access expired. Select the video again to refresh it.', 410)


def media_headers(headers, fmt):
    return {k: str(v) for k, v in {**headers, **(fmt.get('http_headers') or {})}.items()
            if k.lower() in {'user-agent', 'referer', 'origin', 'cookie', 'accept', 'accept-language'}
            and '\r' not in str(v) and '\n' not in str(v)}


def binding(fmt, *, live=False):
    if not isinstance(fmt, dict) or not isinstance(fmt.get('format_id'), str) or not 1 <= len(fmt['format_id']) <= 128:
        return None
    protocols = ('m3u8', 'm3u8_native') if live else ('http', 'https')
    if fmt.get('protocol') not in protocols or fmt.get('has_drm'):
        return None
    value = tuple(fmt.get(k) for k in ('format_id', 'protocol', 'ext', 'vcodec', 'acodec',
                                     'width', 'height', 'fps', 'language', 'asr', 'audio_channels', 'dynamic_range'))
    if any(type(v) not in (str, int, float, type(None)) or isinstance(v, str) and len(v) > 512 or
           isinstance(v, float) and not math.isfinite(v) for v in value):
        return None
    return value


def entity(headers):
    """RFC 9110 8.8.2.2/8.8.3: strong ETag, or a sufficiently old Last-Modified."""
    tag = headers.get('etag', '')
    if re.fullmatch(r'"[\x21\x23-\x7e\x80-\xff]{0,510}"', tag):
        return ('ETag', tag)
    if tag:  # If-Range dates require no entity tag, including a weak one.
        return None
    modified, date = headers.get('last-modified'), headers.get('date')
    if modified and date and len(modified) <= 64 and len(date) <= 64:
        try:
            left, right = parsedate_to_datetime(modified), parsedate_to_datetime(date)
            if left.tzinfo and right.tzinfo and (right - left).total_seconds() >= 60:
                return ('Last-Modified', modified)
        except (ValueError, TypeError, OverflowError):
            pass
    return None


def total_size(reply):
    raw = reply.headers.get('content-range', '') if reply.status_code == 206 else ''
    if reply.status_code == 206:
        if len(raw) > 100:
            raise expired()
        match = re.fullmatch(r'bytes ([0-9]+)-([0-9]+)/([0-9]+)', raw)
        if not match or not 0 <= int(match[1]) <= int(match[2]) < int(match[3]) <= 2**53:
            raise expired()
        return int(match[3])
    length = reply.headers.get('content-length', '')
    return int(length) if len(length) <= 16 and length.isascii() and length.isdigit() and int(length) <= 2**53 else None


def observe(remote, reply, byte_range=None, *, renewed=False):
    identity, size = entity(reply.headers), total_size(reply)
    state = remote.representation
    if (state.validator is not None and identity != state.validator or
            state.total is not None and size != state.total):
        raise expired()
    if renewed and state.observed and state.validator is None:
        raise expired()
    if renewed and reply.headers.get('content-encoding', 'identity').lower() != 'identity':
        raise expired()
    if renewed and byte_range:
        match = re.fullmatch(r'bytes ([0-9]+)-([0-9]+)/([0-9]+)', reply.headers.get('content-range', ''))
        wanted = re.fullmatch(r'bytes=([0-9]*)-([0-9]*)', byte_range)
        if reply.status_code != 206 or not match or not wanted:
            raise expired()
        first, last, count = map(int, match.groups())
        start, end = wanted.groups()
        expected = int(start) if start else max(0, count - int(end))
        limit = min(int(end), count - 1) if start and end else count - 1
        if first != expected or last > limit:
            raise expired()
    state.validator = state.validator or identity
    state.total = state.total if state.total is not None else size
    state.observed = True


async def renew(sessions, session, key, failed, resolver):
    if session.refresh_lock is None:
        session.refresh_lock = asyncio.Lock()
    async with session.refresh_lock:
        if sessions.get(session.token) is not session:
            raise expired()
        current = session.tracks.get(key)
        if current is not failed:
            if current is None:
                raise expired()
            return current
        state = failed.representation
        live = session.is_live and failed.live_binding is not None
        if (session.is_live and not live or not live and
                (failed.binding is None or state.observed and state.validator is None)):
            raise expired()
        if time.monotonic() < session.refresh_after:
            raise expired()
        session.refresh_after = time.monotonic() + 30
        async with asyncio.timeout(60):
            info = await resolver(session.profile).extract(
                f'https://www.youtube.com/watch?v={session.video_id}', fresh=True)
        resolver(session.profile)  # Removal may have occurred during extraction.
        if sessions.get(session.token) is not session or info.get('id') != session.video_id or (info.get('is_live') is True) != live:
            raise expired()
        if not live and abs(number(info.get('duration')) - session.duration) > 0.1:
            raise expired()
        replacements = {}
        formats = info.get('formats')
        if not isinstance(formats, list) or len(formats) > 4096:
            raise expired()
        mapped = {}
        for name, old in session.tracks.items():
            identity = old.live_binding if live else old.binding
            if identity is None:
                continue
            matches = [f for f in formats if binding(f, live=live) == identity]
            if len(matches) != 1:
                raise expired()
            fmt = matches[0]
            if not isinstance(fmt.get('url'), str) or len(fmt['url']) > 8192:
                raise expired()
            mapped[name] = fmt
            replacements[name] = type(old)(safe_remote(fmt.get('url', '')),
                media_headers(info.get('http_headers') or {}, fmt), old.mime,
                old.binding, old.representation, old.live_binding)
        if live:
            for video_key, audio_key in session.hls_pairs.items():
                video, audio = mapped.get(video_key), mapped.get(audio_key)
                if (not video or not audio or not isinstance(video.get('hls_audio_urls'), list) or
                    audio['url'] not in video['hls_audio_urls'] or
                    not isinstance(video.get('manifest_url'), str) or not video['manifest_url'] or
                    video['manifest_url'] != audio.get('manifest_url')):
                    raise expired()
                safe_remote(video['manifest_url'])
        session.tracks.update(replacements)  # Atomic: no partially renewed audio/video pair.
        return session.tracks[key]
