"""Optional Return YouTube Dislike relay: off unless the host enables it, strictly validated."""
import time

import httpx

ORIGIN = "https://returnyoutubedislikeapi.com"
LIMIT = 10 ** 12


def votes(payload, identifier):
    """Accept only an object for this video with plain non-negative integer counts."""
    if not isinstance(payload, dict) or payload.get("id") != identifier:
        return None
    likes, dislikes = payload.get("likes"), payload.get("dislikes")
    if type(likes) is not int or type(dislikes) is not int or not 0 <= likes <= LIMIT or not 0 <= dislikes <= LIMIT:
        return None
    return {"likes": likes, "dislikes": dislikes}


class Dislikes:
    def __init__(self, http, enabled=False, *, clock=time.monotonic):
        self.http, self.enabled, self.clock = http, bool(enabled), clock
        self.cache = {}

    async def get(self, identifier):
        if not self.enabled:
            return {"api_version": 1, "enabled": False}
        cached = self.cache.get(identifier)
        if cached and cached[0] > self.clock():
            counts = cached[1]
        else:
            counts = None
            try:
                reply = await self.http.get(ORIGIN + "/votes", params={"videoId": identifier}, timeout=5)
                if reply.status_code == 200 and len(reply.content) <= 65536:
                    counts = votes(reply.json(), identifier)
            except (httpx.HTTPError, ValueError, TypeError):
                pass  # An optional service never fails playback or details.
            if len(self.cache) >= 128:
                self.cache.pop(next(iter(self.cache)))
            # Unavailable answers are retried sooner than known counts.
            self.cache[identifier] = (self.clock() + (3600 if counts else 300), counts)
        return {"api_version": 1, "enabled": True, "id": identifier, "available": counts is not None, **(counts or {})}
