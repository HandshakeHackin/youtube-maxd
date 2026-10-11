"""Loudness for volume normalisation, read from the matching player response only."""
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


class LoudnessMixin:
    def _extract_player_responses(self, clients, video_id, *args, **kwargs):
        responses, player_url = super()._extract_player_responses(clients, video_id, *args, **kwargs)
        self._tube_loudness = next((value for response in responses
            if (value := player_loudness(response, video_id)) is not None), None)
        return responses, player_url

    def _real_extract(self, url):
        self._tube_loudness = None
        result = super()._real_extract(url)
        if self._tube_loudness is not None:
            result["loudness_db"] = self._tube_loudness
        return result
