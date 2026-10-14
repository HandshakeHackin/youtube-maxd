"""Playback metadata read from the matching YouTube player response only."""
import math


def player_loudness(response, video_id):
    if not isinstance(response, dict):
        return None
    details = response.get("videoDetails")
    if not isinstance(details, dict) or details.get("videoId") != video_id:
        return None
    config = response.get("playerConfig")
    audio = config.get("audioConfig") if isinstance(config, dict) else None
    return loudness(audio.get("loudnessDb")) if isinstance(audio, dict) else None


def loudness(value):
    """A finite decibel offset in a plausible range, otherwise unknown."""
    if type(value) not in (int, float) or not math.isfinite(value) or not -60 <= value <= 60:
        return None
    return float(value)


_TRANSFER_RANGE = {
    'COLOR_TRANSFER_CHARACTERISTICS_BT709': 'SDR',
    'COLOR_TRANSFER_CHARACTERISTICS_SMPTEST2084': 'HDR10',
    'COLOR_TRANSFER_CHARACTERISTICS_SMPTE2084': 'HDR10',
    'COLOR_TRANSFER_CHARACTERISTICS_ARIB_STD_B67': 'HLG',
    '01': 'SDR', '16': 'HDR10', '18': 'HLG',
}


def player_ranges(response, video_id):
    if not isinstance(response, dict):
        return {}
    details = response.get('videoDetails')
    if not isinstance(details, dict) or details.get('videoId') != video_id:
        return {}
    streaming = response.get('streamingData') or {}
    if not isinstance(streaming, dict):
        return {}
    ranges = {}
    formats = streaming.get('formats') if isinstance(streaming.get('formats'), list) else []
    adaptive = streaming.get('adaptiveFormats') if isinstance(streaming.get('adaptiveFormats'), list) else []
    for fmt in (*formats, *adaptive):
        if not isinstance(fmt, dict) or not isinstance(fmt.get('colorInfo'), dict):
            continue
        transfer = fmt['colorInfo'].get('transferCharacteristics')
        value = _TRANSFER_RANGE.get(transfer) if isinstance(transfer, str) else None
        if value is not None and isinstance(fmt.get('itag'), int):
            ranges[str(fmt['itag'])] = value
    return ranges


def codec_range(codec):
    parts = str(codec).split('.')
    # Only fully specified RFC 6381 codec strings carry a transfer code.
    transfer = parts[6] if parts[0] == 'vp09' and len(parts) >= 7 else (
        parts[7] if parts[0] == 'av01' and len(parts) >= 8 else None)
    return _TRANSFER_RANGE.get(transfer)


class PlayerMetadataMixin:
    def _extract_player_responses(self, clients, video_id, *args, **kwargs):
        responses, player_url = super()._extract_player_responses(clients, video_id, *args, **kwargs)
        self._tube_loudness = next((value for response in responses
            if (value := player_loudness(response, video_id)) is not None), None)
        self._tube_range_video_id = video_id
        self._tube_ranges = {}
        conflicts = set()
        for response in responses:
            for itag, value in player_ranges(response, video_id).items():
                if itag in self._tube_ranges and self._tube_ranges[itag] != value:
                    conflicts.add(itag)
                else:
                    self._tube_ranges[itag] = value
        for itag in conflicts:
            del self._tube_ranges[itag]
        return responses, player_url

    def _real_extract(self, url):
        self._tube_loudness = None
        self._tube_range_video_id = None
        self._tube_ranges = {}
        result = super()._real_extract(url)
        if self._tube_loudness is not None:
            result["loudness_db"] = self._tube_loudness
        for fmt in result.get('formats') or []:
            if not isinstance(fmt, dict):
                continue
            itag = str(fmt.get('format_id', '')).split('-', 1)[0]
            hls_range = fmt.pop('_tube_hls_video_range', None)
            value = (self._tube_ranges.get(itag) if result.get('id') == self._tube_range_video_id else None)
            value = value or codec_range(fmt.get('vcodec')) or hls_range
            if value:
                fmt['dynamic_range'] = value
            elif fmt.get('protocol') in ('http', 'https', 'm3u8', 'm3u8_native') and fmt.get('dynamic_range') == 'HDR10':
                # yt-dlp infers HDR10 from VP9 profile 2 / AV1 bit depth. Those
                # alone cannot distinguish PQ from HLG, so do not offer a guess.
                fmt['dynamic_range'] = 'UNKNOWN'
        return result
