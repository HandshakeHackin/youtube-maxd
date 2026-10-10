"""Profile-scoped OAuth resolution and TV catalog continuations."""
import asyncio
import json
from datetime import datetime, timezone
import re
import time
from urllib.parse import parse_qs, urlsplit

from .extractor import Extractor
from .models import ServiceError


class OAuthExtractor(Extractor):
    uses_explicit_catalog_pages = True
    def __init__(self, profile, library, guest):
        super().__init__(timeout=guest.timeout, extractor_args=guest.extractor_args, js_runtimes=guest.js_runtimes)
        self.profile, self.library = profile, library
        self.semaphore = guest.semaphore
        self.catalogs = {}
        self.catalog_lock = asyncio.Lock()

    def invalidate(self):
        super().invalidate()
        self.catalogs.clear()

    async def _private(self, request):
        self.ensure_active()
        result = await self.library.execute({**request, "profile": self.profile}, operation="playback")
        self.ensure_active()
        return result

    async def extract(self, target, *, flat=False, start=1, end=24, fresh=False):
        self.ensure_active()
        try:
            async with asyncio.timeout(self.timeout):
                return await self._extract_authorized(target, flat=flat, start=start, end=end, fresh=fresh)
        except TimeoutError:
            raise ServiceError("timeout", "YouTube took too long to respond. Retry from the video screen.", 504) from None

    async def _extract_authorized(self, target, *, flat, start, end, fresh=False):
        if flat:
            return await self._catalog(target, start, end)
        if not re.fullmatch(r"https://www\.youtube\.com/watch\?v=[A-Za-z0-9_-]{11}", target):
            raise ServiceError("invalid_video", "Select a YouTube video.", 400)
        credentials = await self._private({"action": "credentials"})
        token = credentials.get("access_token")
        try:
            expiry = datetime.fromisoformat(credentials["expiry_date"].replace("Z", "+00:00"))
            if expiry.tzinfo is None or expiry <= datetime.now(timezone.utc):
                raise ValueError()
        except (ValueError, TypeError, KeyError, OverflowError):
            raise ServiceError("authentication_required", "Sign in to the YouTube account again.", 403) from None
        if not isinstance(token, str) or not 1 <= len(token) <= 8192 or re.search(r"[^\x21-\x7e]", token):
            raise ServiceError("authentication_required", "Sign in to the YouTube account again.", 403)
        return await super().extract(target, fresh=fresh, _oauth_access_token=token)

    async def _catalog(self, target, start, end):
        if type(start) is not int or type(end) is not int or start < 1 or (start - 1) % 24 or end != start + 24 or end > 24025:
            raise ServiceError("invalid_page", "Select a valid catalog page.", 400)
        parsed = urlsplit(target)
        search = re.fullmatch(r"ytsearch([0-9]{1,5}):(.+)", target, re.DOTALL)
        if search and int(search[1]) == end and len(search[2]) <= 200:
            operation = {"action": "catalog", "kind": "search", "query": search[2]}
        elif parsed.scheme == "https" and parsed.netloc == "www.youtube.com" and re.fullmatch(r"/channel/UC[A-Za-z0-9_-]{22}/videos", parsed.path) and not parsed.query:
            operation = {"action": "catalog", "kind": "channel", "id": parsed.path.split("/")[2]}
        elif parsed.scheme == "https" and parsed.netloc == "www.youtube.com" and parsed.path == "/playlist":
            ids = parse_qs(parsed.query)
            if set(ids) != {"list"} or len(ids["list"]) != 1 or not re.fullmatch(r"[A-Za-z0-9_-]{10,100}", ids["list"][0]):
                raise ServiceError("invalid_parent", "Select a valid YouTube playlist.", 400)
            operation = {"action": "catalog", "kind": "playlist", "id": ids["list"][0]}
        else:
            raise ServiceError("invalid_parent", "Select a YouTube catalog.", 400)
        page = (start - 1) // 24
        key = json.dumps(operation, sort_keys=True)
        async with self.catalog_lock:
            now = time.monotonic()
            self.catalogs = {k: v for k, v in self.catalogs.items() if v["deadline"] > now}
            entry = self.catalogs.get(key)
            if entry and page < entry["next"] and page not in entry["pages"]:
                entry = None
            if not entry:
                if len(self.catalogs) >= 32:
                    self.catalogs.pop(next(iter(self.catalogs)))
                entry = {"deadline": now + 110, "pages": {}, "next": 0, "cursor": ""}
                self.catalogs[key] = entry
            # Opaque continuations are followed in order; never re-run a query
            # to guess an offset in a changing account feed.
            while entry["next"] <= page:
                if entry["next"] and not entry["cursor"]:
                    return {"entries": [], "_has_more": False}
                request = {"action": "catalog", "cursor": entry["cursor"]} if entry["next"] else operation
                try:
                    result = await self._private(request)
                except ServiceError:
                    self.catalogs.pop(key, None)
                    raise
                records = result.get("items")
                cursor = result.get("cursor", "")
                if (not isinstance(records, list) or len(records) > 24 or type(result.get("has_more")) is not bool
                        or (result["has_more"] and (not isinstance(cursor, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", cursor)))):
                    self.catalogs.pop(key, None)
                    raise ServiceError("library_transport", "The TV catalog response was invalid.", 502)
                entry["pages"][entry["next"]] = result
                entry["next"] += 1
                entry["cursor"] = cursor if result["has_more"] else ""
                if len(entry["pages"]) > 16:
                    entry["pages"].pop(next(iter(entry["pages"])))
            records = entry["pages"][page]["items"]
            values = []
            for record in records:
                identity = record.get("id", "") if isinstance(record, dict) else ""
                if not re.fullmatch(r"[A-Za-z0-9_-]{11}", identity):
                    continue
                values.append({"id": identity, "title": record.get("title", ""),
                    "channel": record.get("subtitle", ""), "channel_id": record.get("channel_id", ""),
                    "description": record.get("overview", ""), "thumbnail": record.get("art_url", ""),
                    "duration": record.get("duration_sec", 0), "is_live": record.get("is_live", False)})
            return {"entries": values, "_has_more": entry["pages"][page]["has_more"]}
