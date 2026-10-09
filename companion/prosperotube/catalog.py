"""Search, channels, and playlists; pagination keeps opaque stable identities."""
from .models import CHANNEL_ID, PLAYLIST_ID, VIDEO_ID, Item, Page, ServiceError, number, safe_remote, text
from urllib.parse import urlsplit, parse_qs

PAGE_SIZE = 24


def item(info):
    identifier = str(info.get("id", ""))
    if not VIDEO_ID.fullmatch(identifier):
        return None
    art = info.get("thumbnail", "")
    if not art:
        thumbs = info.get("thumbnails") or []
        art = next((t.get("url", "") for t in reversed(thumbs) if isinstance(t, dict)), "")
    try:
        art = safe_remote(art) if art else f"https://i.ytimg.com/vi/{identifier}/hqdefault.jpg"
    except ServiceError:
        art = f"https://i.ytimg.com/vi/{identifier}/hqdefault.jpg"
    return Item(id=identifier, title=text(info.get("title")) or "Unavailable video",
                subtitle=text(info.get("channel") or info.get("uploader"), 160),
                overview=text(info.get("description"), 320), art_url=art,
                channel_id=text(info.get("channel_id"), 64),
                duration_sec=int(number(info.get("duration"))),
                is_live=info.get("live_status") == "is_live" or info.get("is_live") is True)


async def page_for(extractor, *, query="", parent="", page=0):
    if page < 0 or page > 1000:
        raise ServiceError("invalid_page", "The requested page is outside the supported range.", 400)
    if query:
        if len(query) > 200 or not query.strip():
            raise ServiceError("invalid_query", "Search text must contain at most 200 characters.", 400)
        query = query.strip()
        if query.startswith(("channel:", "playlist:")):
            return await page_for(extractor, parent=query, page=page)
        identifier = query[6:] if query.startswith("video:") else ""
        if query.startswith(("https://", "http://")):
            try:
                link = urlsplit(query)
                params = parse_qs(link.query)
            except ValueError:
                raise ServiceError("invalid_query", "The YouTube link is invalid.", 400) from None
            if link.hostname in {"youtube.com", "www.youtube.com", "m.youtube.com", "youtu.be"}:
                playlist = params.get("list", [""])[0]
                if playlist and PLAYLIST_ID.fullmatch(playlist):
                    return await page_for(extractor, parent="playlist:" + playlist, page=page)
                if link.path.startswith("/channel/"):
                    return await page_for(extractor, parent="channel:" + link.path.split("/")[2], page=page)
                identifier = link.path.strip("/") if link.hostname == "youtu.be" else params.get("v", [""])[0]
                if not identifier and link.path.startswith(("/shorts/", "/live/")):
                    identifier = link.path.split("/")[2]
        if identifier:
            if not VIDEO_ID.fullmatch(identifier):
                raise ServiceError("invalid_video", "The YouTube video ID is invalid.", 400)
            if page: return Page(items=[], page=page)
            raw = await extractor.extract(f"https://www.youtube.com/watch?v={identifier}")
            row = item(raw)
            return Page(items=[row] if row else [], page=0)
        target = f"ytsearch{(page + 1) * PAGE_SIZE + 1}:{query}"
    elif parent.startswith("channel:") and CHANNEL_ID.fullmatch(parent[8:]):
        target = f"https://www.youtube.com/channel/{parent[8:]}/videos"
    elif parent.startswith("playlist:") and PLAYLIST_ID.fullmatch(parent[9:]):
        target = f"https://www.youtube.com/playlist?list={parent[9:]}"
    else:
        raise ServiceError("invalid_catalog", "Choose search, a channel, or a YouTube playlist.", 400)
    start = page * PAGE_SIZE + 1
    result = await extractor.extract(target, flat=True, start=start, end=start + PAGE_SIZE)
    entries = result.get("entries") or []
    rows, seen = [], set()
    for raw in entries[:PAGE_SIZE]:
        row = item(raw) if isinstance(raw, dict) else None
        if row and getattr(extractor, "fixture_mode", False):
            row.art_url = extractor.base + "/fixtures/thumb.png"
        if row and row.id not in seen:
            rows.append(row)
            seen.add(row.id)
    more = result.get("_has_more") if getattr(extractor, "uses_explicit_catalog_pages", False) else len(entries) > PAGE_SIZE
    return Page(items=rows, has_more=more is True, page=page)
