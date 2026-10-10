"""YouTube's localized paid-promotion disclosure, scoped to one extraction."""

def disclosure(value):
    if not isinstance(value, str):
        return ""
    value = " ".join(value[:2048].replace("\x00", "").split())
    value = "".join(c for c in value if ord(c) >= 32 and ord(c) != 127)
    return value.encode("utf-8", errors="replace")[:240].decode("utf-8", errors="ignore")


def player_disclosure(response, video_id):
    if not isinstance(response, dict):
        return ""
    details = response.get("videoDetails")
    if not isinstance(details, dict) or details.get("videoId") != video_id:
        return ""
    overlay = response.get("paidContentOverlay") or {}
    if not isinstance(overlay, dict):
        return ""
    renderer = overlay.get("paidContentOverlayRenderer") or {}
    if not isinstance(renderer, dict):
        return ""
    value = renderer.get("text") or {}
    if not isinstance(value, dict):
        return ""
    if isinstance(value.get("simpleText"), str):
        return disclosure(value["simpleText"])
    runs = value.get("runs")
    if not isinstance(runs, list):
        return ""
    return disclosure("".join(run["text"][:512] for run in runs[:64]
                              if isinstance(run, dict) and isinstance(run.get("text"), str)))


class PaidContentMixin:
    def _extract_player_responses(self, clients, video_id, *args, **kwargs):
        responses, player_url = super()._extract_player_responses(clients, video_id, *args, **kwargs)
        self._tube_paid_disclosure = next((text for response in responses
            if (text := player_disclosure(response, video_id))), "")
        return responses, player_url

    def _real_extract(self, url):
        self._tube_paid_disclosure = ""
        result = super()._real_extract(url)
        result["paid_content_text"] = self._tube_paid_disclosure
        return result
