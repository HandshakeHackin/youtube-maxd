"""Recover split live-HLS codec/pairing metadata from validated master playlists."""
import re
from urllib.parse import urljoin

from yt_dlp.extractor.youtube import YoutubeIE

from .models import ServiceError, safe_remote
from .paid_content import PaidContentMixin


def attributes(line):
    return {match[1]: match[2] if match[2] is not None else match[3]
            for match in re.finditer(r'(?:^|,)([A-Z0-9-]+)=(?:"([^"]*)"|([^,]*))', line.partition(':')[2])}


def apply_master(info, master, content):
    """Only explicit CODECS + AUDIO group + exact format URLs establish a pair."""
    formats = info.get('formats') or []
    audios, variants = {}, []
    pending = None
    for line in content.splitlines():
        line = line.strip()
        if line.startswith('#EXT-X-MEDIA:'):
            attrs = attributes(line)
            if attrs.get('TYPE') == 'AUDIO' and attrs.get('URI') and attrs.get('GROUP-ID'):
                audios.setdefault(attrs['GROUP-ID'], []).append(safe_remote(urljoin(master, attrs['URI'])))
        elif line.startswith('#EXT-X-STREAM-INF:'):
            pending = attributes(line)
        elif line and not line.startswith('#') and pending is not None:
            variants.append((safe_remote(urljoin(master, line)), pending))
            pending = None
    for url, attrs in variants:
        codecs = attrs.get('CODECS', '').split(',')
        audio_codec = next((c for c in codecs if c.startswith(('mp4a.', 'opus'))), None)
        video_codec = next((c for c in codecs if c.startswith(('avc1.', 'vp09.', 'vp9', 'av01.'))), None)
        if not audio_codec or not video_codec:
            continue
        paired = []
        for audio_url in audios.get(attrs.get('AUDIO'), []):
            for audio in formats:
                if (audio.get('url') == audio_url and audio.get('manifest_url') == master
                        and audio.get('vcodec') == 'none' and not audio.get('has_drm')
                        and audio.get('protocol') in ('m3u8', 'm3u8_native')):
                    previous = audio.get('acodec')
                    if previous not in (None, '', 'none', audio_codec):
                        continue
                    audio['acodec'] = audio_codec
                    paired.append(audio_url)
        for video in formats:
            if (paired and video.get('url') == url and video.get('manifest_url') == master
                    and video.get('vcodec') == video_codec and video.get('acodec') == 'none'):
                video['hls_audio_urls'] = list(dict.fromkeys([*video.get('hls_audio_urls', []), *paired]))


class LiveHLSMixin:
    def _parse_m3u8_formats_and_subtitles(self, m3u8_doc, m3u8_url=None, *args, **kwargs):
        formats, subtitles = super()._parse_m3u8_formats_and_subtitles(m3u8_doc, m3u8_url, *args, **kwargs)
        if m3u8_url and len(m3u8_doc.encode('utf-8')) <= 2 * 1024 * 1024:
            try:
                safe_remote(m3u8_url)
                apply_master({'formats': formats}, m3u8_url, m3u8_doc)
            except (ServiceError, ValueError):
                pass
        return formats, subtitles


class LiveHLSYoutubeIE(PaidContentMixin, LiveHLSMixin, YoutubeIE):
    @classmethod
    def ie_key(cls):
        return 'Youtube'
