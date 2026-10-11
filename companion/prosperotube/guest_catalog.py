"""Guest worker catalogs share retained groups with native navigation."""
from .oauth_extractor import OAuthExtractor
from .models import ServiceError


class GuestCatalogExtractor(OAuthExtractor):
    """Reuse bounded catalog pagination; video resolution stays with Guest yt-dlp."""
    def __init__(self, library, guest):
        super().__init__("guest", library, guest)
        self.guest = guest

    async def _private(self, request):
        self.ensure_active()
        if request.get("action") != "catalog":
            raise ServiceError("invalid_request", "Guest catalogs do not request account credentials.", 400)
        result = await self.library.execute({**request, "profile": "guest"}, operation="playback")
        self.ensure_active()
        return result

    async def _extract_authorized(self, target, *, flat, start, end, fresh=False):
        if flat:
            return await self._catalog(target, start, end)
        return await self.guest.extract(target, fresh=fresh)
