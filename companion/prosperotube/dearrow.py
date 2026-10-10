"""Opt-in DeArrow metadata and bounded thumbnail capabilities; originals survive."""
import asyncio
from collections import OrderedDict
import hashlib
import json
import math
import secrets
import time

import httpx

from .models import VIDEO_ID, ServiceError, safe_remote


def trusted(rows):
    if not isinstance(rows, list):
        return None
    # SmartTube selects the first non-original submission. Keep API trust checks.
    row = next((r for r in rows[:64] if isinstance(r, dict) and r.get("original") is False), None)
    if row is None:
        return None
    votes = row.get("votes")
    return row if row.get("locked") is True or (
        type(votes) in (int, float) and math.isfinite(votes) and votes >= 0) else None


def branding(data):
    if not isinstance(data, dict):
        return "", None
    title, stamp = "", None
    row = trusted(data.get("titles"))
    if row and row.get("original") is False and isinstance(row.get("title"), str):
        title = " ".join(row["title"][:2048].replace(">", "").replace("\x00", "").split())
        title = "".join(c for c in title if ord(c) >= 32 and ord(c) != 127)
        title = title.encode("utf-8", errors="replace")[:191].decode("utf-8", errors="ignore")
    row = trusted(data.get("thumbnails"))
    if row and row.get("original") is False:
        value = row.get("timestamp")
        if type(value) in (int, float) and math.isfinite(value) and 0 <= value < 604800:
            stamp = value
    return title, stamp


class DeArrow:
    def __init__(self, client):
        self.client = client
        self.cache = OrderedDict()
        self.images = OrderedDict()
        self.limit = asyncio.Semaphore(4)

    async def read(self, url, maximum, image=False):
        async with self.client.stream("GET", url, timeout=3, follow_redirects=False) as response:
            if response.status_code != 200:
                return None
            content = bytearray()
            async for part in response.aiter_bytes():
                content.extend(part)
                if len(content) > maximum:
                    return None
            if image:
                data = bytes(content)
                if data.startswith(b"\xff\xd8\xff"):
                    return data, "image/jpeg"
                if data.startswith(b"\x89PNG\r\n\x1a\n"):
                    return data, "image/png"
                if data.startswith(b"RIFF") and data[8:12] == b"WEBP":
                    return data, "image/webp"
                return None
            return json.loads(content)

    async def lookup(self, identifier):
        prefix = hashlib.sha256(identifier.encode()).hexdigest()[:4]
        async with self.limit:
            cached = self.cache.get(prefix)
            if cached and cached[0] > time.monotonic():
                self.cache.move_to_end(prefix)
                return branding(cached[1].get(identifier))
            data = {}
            try:
                async with asyncio.timeout(4):
                    fetched = await self.read("https://sponsor.ajay.app/api/branding/" + prefix, 1024 * 1024)
                if isinstance(fetched, dict):
                    data = fetched
            except (httpx.HTTPError, ValueError, TypeError, RecursionError, TimeoutError):
                pass
            self.cache[prefix] = (time.monotonic() + (3600 if data else 60), data)
            self.cache.move_to_end(prefix)
            while len(self.cache) > 128:
                self.cache.popitem(last=False)
            return branding(data.get(identifier))

    def image_url(self, identifier, stamp, fallback, base):
        try:
            fallback = safe_remote(fallback)
        except (ServiceError, TypeError, ValueError):
            fallback = f"https://i.ytimg.com/vi/{identifier}/hqdefault.jpg"
        now = time.monotonic()
        for key, (expires, video, at, original) in list(self.images.items()):
            if (video, at, original) == (identifier, stamp, fallback):
                self.images[key] = (now + 3600, video, at, original)
                self.images.move_to_end(key)
                return f"{base}/art/dearrow/{key}"
        key = secrets.token_urlsafe(24)
        self.images[key] = (now + 3600, identifier, stamp, fallback)
        while len(self.images) > 512:
            self.images.popitem(last=False)
        return f"{base}/art/dearrow/{key}"

    async def decorate(self, result, titles, thumbnails, base):
        if not titles and not thumbnails:
            return result
        # Copy records so cached library/extractor data never acquires replacements.
        data = result.model_dump() if hasattr(result, "model_dump") else dict(result)
        if data.get("kind") not in (None, "catalog"):
            return data
        rows = data.get("items")
        if not isinstance(rows, list):
            rows = [data["item"]] if isinstance(data.get("item"), dict) else []
        copies = [dict(row) if isinstance(row, dict) else row for row in rows]
        if "items" in data:
            data["items"] = copies
        elif copies:
            data["item"] = copies[0]

        async def one(row):
            if not isinstance(row, dict) or row.get("is_folder") or not VIDEO_ID.fullmatch(str(row.get("id", ""))):
                return
            title, stamp = await self.lookup(row["id"])
            if titles and title:
                row["dearrow_title"] = title
            if thumbnails and stamp is not None:
                row["dearrow_art_url"] = self.image_url(row["id"], stamp, row.get("art_url", ""), base)

        try:
            async with asyncio.timeout(6):
                await asyncio.gather(*(one(row) for row in copies[:64]))
        except TimeoutError:
            pass  # A delayed optional service cannot fail the catalog.
        return data

    async def thumbnail(self, key):
        record = self.images.get(key)
        if not record:
            self.images.pop(key, None)
            return None
        _, identifier, stamp, fallback = record
        generated = f"https://dearrow-thumb.ajay.app/api/v1/getThumbnail?videoID={identifier}&time={stamp}"
        async with self.limit:
            for url in ((generated, fallback) if record[0] > time.monotonic() else (fallback,)):
                try:
                    async with asyncio.timeout(4):
                        image = await self.read(url, 2 * 1024 * 1024, image=True)
                    if image:
                        return image
                except (httpx.HTTPError, ValueError, TypeError, RecursionError, TimeoutError):
                    pass
        return None
