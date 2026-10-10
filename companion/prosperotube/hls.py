"""Atomic HLS capability rewriting with bounded rolling-playlist retention."""
import hashlib
from pathlib import Path
import re
from urllib.parse import urljoin, urlsplit

from .models import ServiceError, safe_remote
from .sessions import Remote


def rewrite(content, origin_url, session, request_base, playlist_key, source, *, limit=4096):
    tracks = dict(session.tracks)
    resources = dict(session.hls_resources)  # insertion order is last playlist use
    playlists = dict(session.hls_playlists)
    references = set()
    # A concurrent refresh can retire an in-flight child playlist. Restore only
    # this known fetched capability, keeping it classified as a dynamic resource.
    # A successful old root response may arrive after atomic link renewal.
    # Rewrite that fetched playlist, but never put its expired root URL back.
    current = tracks.get(playlist_key)
    if not (source.live_binding is not None and current is not None and current is not source):
        tracks[playlist_key] = source
    if playlist_key.startswith("hls-"):
        resources.pop(playlist_key, None)
        resources[playlist_key] = True

    def register(raw):
        target = safe_remote(urljoin(origin_url, raw))
        suffix = Path(urlsplit(target).path).suffix.lower()
        if suffix not in {".m3u8", ".ts", ".m4s", ".mp4", ".aac", ".vtt", ".key"}:
            suffix = ".ts"
        key = "hls-" + hashlib.sha256(target.encode()).hexdigest()[:24] + suffix
        headers = source.headers if urlsplit(source.url).hostname == urlsplit(target).hostname else {
            k: v for k, v in source.headers.items() if k.lower() not in {"cookie", "authorization"}}
        tracks[key] = Remote(target, dict(headers), "application/octet-stream")
        resources.pop(key, None)
        resources[key] = True
        references.add(key)
        if len(references) > limit:
            raise ServiceError("playlist_limit", "The current live playlist exceeds its resource limit.")
        return f"{request_base}/media/{session.token}/{key}"

    lines = []
    for line in content.splitlines():
        if line and not line.startswith("#"):
            line = register(line.strip())
        elif 'URI="' in line:
            line = re.sub(r'URI="([^"]+)"', lambda m: 'URI="' + register(m.group(1)) + '"', line)
        lines.append(line)
    playlists[playlist_key] = references

    # Keep every resource referenced by the current root/variant graph, including
    # keys, init segments and rendition playlists. Older windows stay available
    # until capacity is needed; retire the least recently listed unreferenced
    # resources rather than allowing an active live stream to grow without bound.
    reachable, pending = set(), [*set(tracks).difference(resources), playlist_key]
    while pending:
        key = pending.pop()
        if key in reachable:
            continue
        reachable.add(key)
        pending.extend(playlists.get(key, ()))
    playlists = {key: value for key, value in playlists.items() if key in reachable}
    for key in list(resources):
        if len(tracks) <= limit:
            break
        if key not in reachable:
            resources.pop(key)
            tracks.pop(key, None)
    if len(tracks) > limit:
        raise ServiceError("playlist_limit", "The current live playlist exceeds its resource limit.")
    # Failed validation/overflow leaves the previously usable registry intact.
    session.tracks, session.hls_resources, session.hls_playlists = tracks, resources, playlists
    return "\n".join(lines) + "\n"
