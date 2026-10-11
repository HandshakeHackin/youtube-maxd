"""Worker catalogs share retained groups with native navigation."""
from .oauth_extractor import OAuthExtractor
from .models import ServiceError


class CatalogExtractor(OAuthExtractor):
    """Reuse catalog pagination while keeping video resolution profile-owned."""
    def __init__(self, profile, library, video):
        super().__init__(profile, library, video)
        self.video = video

    def invalidate(self):
        super().invalidate()
        self.video.invalidate()

    async def _private(self, request):
        self.ensure_active()
        if request.get("action") != "catalog":
            raise ServiceError("invalid_request", "Catalog adapters do not request account credentials.", 400)
        result = await self.library.execute({**request, "profile": self.profile}, operation="playback")
        self.ensure_active()
        return result

    async def _extract_authorized(self, target, *, flat, start, end, fresh=False, hl="", gl="", hide=None):
        if flat:
            return await self._catalog(target, start, end, hl=hl, gl=gl, hide=hide)
        return await self.video.extract(target, fresh=fresh, **({"hl": hl} if hl else {}))


class GuestCatalogExtractor(CatalogExtractor):
    def __init__(self, library, guest):
        super().__init__("guest", library, guest)
