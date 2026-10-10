"""Authenticated LAN API, origin-scoped streaming relay, and optional SponsorBlock."""
import asyncio
from contextlib import asynccontextmanager
from collections import defaultdict
import hashlib
import hmac
import json
import os
import re
import secrets
import time
from urllib.parse import urljoin, urlsplit
from typing import Literal

from fastapi import FastAPI, Header, Query, Request
from fastapi.responses import JSONResponse, Response, StreamingResponse
import httpx
from pydantic import BaseModel, ConfigDict, Field

from . import __version__
from .catalog import item, page_for
from .captions import vtt_to_srt
from .extractor import Extractor
from .hls import rewrite as hls_rewrite
from .models import ResolveRequest, ServiceError, safe_remote, video_id
from .sessions import Sessions
from .media_refresh import expired as expired_media, observe as observe_media, renew as renew_media


class PairRequest(BaseModel):
    code: str = Field(pattern="^[0-9]{6}$")


class DeviceStartRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    name: str = Field(default="YouTube account", min_length=1, max_length=150)


class DeviceSessionRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    session: str = Field(pattern="^[0-9a-f]{32}$")


class LibraryRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    action: Literal["profiles", "home", "subscriptions", "history", "library", "playlists",
                    "watch_later", "playlist", "comments", "replies", "continue", "like", "dislike",
                    "unrate", "subscribe", "unsubscribe", "playlist_create", "playlist_add",
                    "playlist_remove", "playlist_rename", "playlist_delete", "watch_later_add",
                    "watch_later_remove", "watch_progress", "profile_remove", "comment_rate",
                    "live_chat", "chat_poll", "chat_close"]
    profile: str = Field(default="guest", pattern="^[A-Za-z0-9_-]{1,32}$")
    id: str = Field(default="", max_length=100)
    playlist: str = Field(default="", max_length=100)
    title: str = Field(default="", max_length=150)
    sort: Literal["top", "newest"] = "top"
    rating: Literal["like", "dislike", "none"] = "none"
    chat_mode: Literal["top", "live"] = "top"
    cursor: str = Field(default="", max_length=64)
    session: str = Field(default="", max_length=64, pattern="^[A-Za-z0-9_-]*$")
    position_sec: float = Field(default=0, ge=0, le=1_000_000, allow_inf_nan=False)
    state: Literal["playing", "stopped"] = "playing"


class DeviceJobRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    action: Literal["start", "status", "cancel"]
    name: str = Field(default="YouTube account", min_length=1, max_length=150)
    session: str = Field(default="", pattern="^(|[0-9a-f]{32})$")


class JobRequest(BaseModel):
    action: str = Field(pattern="^(search|catalog|resolve|details|library|auth)$")
    query: str = Field(default="", max_length=200)
    parent: str = Field(default="", max_length=160)
    page: int = Field(default=0, ge=0, le=1000)
    playback: ResolveRequest | None = None
    library: LibraryRequest | None = None
    auth: DeviceJobRequest | None = None
    id: str = Field(default="", max_length=11)
    profile: str = Field(default="guest", pattern="^[A-Za-z0-9_-]{1,32}$")


def create_app(*, extractor=None, token=None, pairing_code=None, public_base=None,
               http_client=None, sponsorblock=False, library=None, device_auth=None):
    @asynccontextmanager
    async def lifespan(app):
        try:
            yield
        finally:
            if device_auth is not None:
                device_auth.close()
            tasks = list({job[1] for job in app.state.jobs.values()} | app.state.retired_tasks)
            for task in tasks:
                task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
            if library is not None:
                await library.close()
            await app.state.http.aclose()
    app = FastAPI(title="ProsperoTube Companion", version=__version__, docs_url=None, redoc_url=None, lifespan=lifespan)
    app.state.extractor = extractor or Extractor()
    app.state.token = token or secrets.token_urlsafe(32)
    app.state.pairing_code = pairing_code or f"{secrets.randbelow(1000000):06d}"
    app.state.pairing_deadline = time.monotonic() + 600
    app.state.sessions = Sessions()
    app.state.http = http_client or httpx.AsyncClient(timeout=httpx.Timeout(20, connect=5), follow_redirects=False)
    app.state.public_base = public_base.rstrip("/") if public_base else None
    app.state.pair_attempts = defaultdict(list)
    app.state.sponsorblock = sponsorblock
    app.state.sponsor_cache = {}
    app.state.jobs = {}
    app.state.retired_tasks = set()
    app.state.library = library
    app.state.device_auth = device_auth

    def profile_removed(identity):
        if hasattr(app.state.extractor, "invalidate_profile"):
            app.state.extractor.invalidate_profile(identity)
        app.state.sessions.invalidate_profile(identity)
        if app.state.device_auth is not None:
            app.state.device_auth.forget_profile(identity)
        current = asyncio.current_task()
        for key, (_, task) in list(app.state.jobs.items()):
            if task is not current and getattr(task, "profile", None) == identity:
                task.cancel()
                if not task.done():
                    app.state.retired_tasks.add(task)
                    task.add_done_callback(app.state.retired_tasks.discard)
                del app.state.jobs[key]
    if library is not None and hasattr(library, "on_profile_removed"):
        library.on_profile_removed = profile_removed

    @app.exception_handler(ServiceError)
    async def service_error(_request, exc):
        return JSONResponse({"api_version": 1, "error": {"code": exc.code, "message": exc.message}}, status_code=exc.status)

    @app.middleware("http")
    async def authorize(request: Request, call_next):
        path = request.url.path
        if path.startswith("/v1/") and path not in ("/v1/health", "/v1/pair"):
            expected = "Bearer " + app.state.token
            if not hmac.compare_digest(request.headers.get("authorization", ""), expected):
                return JSONResponse({"error": {"code": "unauthorized", "message": "Pair the PS5 with this companion first."}}, status_code=401)
        # Request lengths are bounded even before JSON/Pydantic parsing.
        if request.method == "POST":
            try:
                if int(request.headers.get("content-length", "0")) > 8192:
                    return Response(status_code=413)
            except ValueError:
                return Response(status_code=400)
            body = await request.body()
            if len(body) > 8192:
                return Response(status_code=413)
        return await call_next(request)

    def base(request):
        # Host is only used to construct session links for the requesting
        # client, never to connect upstream. An explicit LAN base is preferred.
        return app.state.public_base or str(request.base_url).rstrip("/")

    def resolver(profile):
        if library is not None and hasattr(library, "has_profile") and not library.has_profile(profile):
            raise ServiceError("unknown_profile", "Select an available profile.", 404)
        extractor = app.state.extractor
        if hasattr(extractor, "for_profile"):
            return extractor.for_profile(profile)
        if profile != "guest":
            raise ServiceError("unknown_profile", "Select an available playback profile.", 404)
        return extractor

    @app.get("/v1/health")
    async def health():
        return {"api_version": 1, "version": __version__, "service": "ProsperoTube",
                "mode": "fixtures" if getattr(app.state.extractor, "fixture_mode", False) else "youtube",
                "features": ["search", "channels", "playlists", "dash", "relay", "captions"] + (["library"] if library is not None else []) + (["device_auth"] if device_auth is not None else []),
                "sponsorblock": app.state.sponsorblock}

    @app.post("/v1/library")
    async def library_operation(payload: LibraryRequest):
        if app.state.library is None:
            raise ServiceError("library_unavailable", "Enable the library worker on the companion first.", 503)
        return await app.state.library.execute(payload.model_dump())

    def signin():
        if app.state.device_auth is None:
            raise ServiceError("auth_unavailable", "Enable the library worker on the companion first.", 503)
        return app.state.device_auth

    @app.post("/v1/auth/start")
    async def device_start(payload: DeviceStartRequest):
        return await signin().start(payload.name)

    @app.post("/v1/auth/status")
    async def device_status(payload: DeviceSessionRequest):
        return await signin().status(payload.session)

    @app.post("/v1/auth/cancel")
    async def device_cancel(payload: DeviceSessionRequest):
        return signin().cancel(payload.session)

    @app.post("/v1/pair")
    async def pair(payload: PairRequest, request: Request):
        address = request.client.host if request.client else "unknown"
        now = time.monotonic()
        attempts = [t for t in app.state.pair_attempts[address] if t > now - 60]
        if len(attempts) >= 5:
            raise ServiceError("pair_rate_limit", "Too many pairing attempts. Wait one minute and retry.", 429)
        if len(app.state.pair_attempts) > 256:
            app.state.pair_attempts.clear()
        attempts.append(now)
        app.state.pair_attempts[address] = attempts
        if now > app.state.pairing_deadline:
            raise ServiceError("pair_expired", "The pairing code expired. Restart the companion to get a new code.", 410)
        if not hmac.compare_digest(payload.code, app.state.pairing_code):
            raise ServiceError("pair_denied", "The pairing code did not match.", 403)
        return {"api_version": 1, "token": app.state.token}

    @app.get("/v1/search")
    async def search(q: str = Query(min_length=1, max_length=200), page: int = Query(default=0, ge=0, le=1000),
                     profile: str = Query(default="guest", pattern="^[A-Za-z0-9_-]{1,32}$")):
        return await page_for(resolver(profile), query=q, page=page)

    @app.get("/v1/catalog")
    async def catalog(parent: str = Query(min_length=1, max_length=160), page: int = Query(default=0, ge=0, le=1000),
                      profile: str = Query(default="guest", pattern="^[A-Za-z0-9_-]{1,32}$")):
        return await page_for(resolver(profile), parent=parent, page=page)

    @app.get("/v1/video/{identifier}")
    async def details(identifier: str, profile: str = Query(default="guest", pattern="^[A-Za-z0-9_-]{1,32}$")):
        video_id(identifier)
        info = await resolver(profile).extract(f"https://www.youtube.com/watch?v={identifier}")
        return {"api_version": 1, "item": item(info), "chapters": info.get("chapters") or [],
                "caption_languages": sorted((info.get("subtitles") or {}).keys())[:32]}

    @app.post("/v1/resolve")
    async def resolve(payload: ResolveRequest, request: Request):
        video_id(payload.id)
        info = await resolver(payload.profile).extract(f"https://www.youtube.com/watch?v={payload.id}", fresh=payload.fresh)
        resolver(payload.profile)  # profile may have been removed while awaiting extraction
        return app.state.sessions.create(info, payload, base(request))

    def cancel_auth_result(task):
        # An abandoned start may already have completed before DELETE arrives.
        # Once its result was delivered, the caller owns the device session.
        if app.state.device_auth is not None and not task.done():
            app.state.device_auth.cancel_task(task)
        if (getattr(task, "auth_start", False) and not getattr(task, "auth_delivered", False)
                and task.done() and not task.cancelled()):
            result = task.result().get("result", {})
            if result.get("state") == "pending":
                try:
                    signin().cancel(result["session"])
                except ServiceError:
                    pass
        if (library is not None and getattr(task, "chat_read", False) and not getattr(task, "auth_delivered", False)
                and not getattr(task, "chat_cleanup_started", False) and task.done() and not task.cancelled()):
            result = task.result().get("result", {})
            cursor = result.get("cursor", "")
            if result.get("kind") == "live_chat" and isinstance(cursor, str) and re.fullmatch(r"[A-Za-z0-9_-]{1,64}", cursor):
                task.chat_cleanup_started = True
                async def cleanup_chat():
                    try:
                        await library.execute({"action": "chat_close", "profile": task.profile, "cursor": cursor})
                    except ServiceError:
                        pass
                cleanup = asyncio.create_task(cleanup_chat())
                app.state.retired_tasks.add(cleanup)
                cleanup.add_done_callback(app.state.retired_tasks.discard)

    @app.post("/v1/jobs")
    async def create_job(payload: JobRequest, request: Request):
        now = time.monotonic()
        for key, (deadline, task) in list(app.state.jobs.items()):
            if deadline <= now:
                cancel_auth_result(task)
                task.cancel()
                del app.state.jobs[key]
        if len(app.state.jobs) >= 16:
            raise ServiceError("job_limit", "The companion is busy. Wait for the current request or cancel it.", 503)
        if payload.action == "resolve" and payload.playback is None:
            raise ServiceError("invalid_job", "A playback request is required.", 400)
        if payload.action == "library" and (payload.library is None or app.state.library is None):
            raise ServiceError("invalid_job", "An enabled library and operation are required.", 400)
        if payload.action == "auth":
            if payload.auth is None or app.state.device_auth is None:
                raise ServiceError("invalid_job", "An enabled sign-in service and operation are required.", 400)
            if payload.auth.action != "start" and not payload.auth.session:
                raise ServiceError("invalid_job", "A sign-in session is required.", 400)
        async def work():
            try:
                if payload.action == "search":
                    result = await page_for(resolver(payload.profile), query=payload.query, page=payload.page)
                elif payload.action == "catalog":
                    result = await page_for(resolver(payload.profile), parent=payload.parent, page=payload.page)
                elif payload.action == "resolve":
                    identifier = video_id(payload.playback.id)
                    info = await resolver(payload.playback.profile).extract(f"https://www.youtube.com/watch?v={identifier}", fresh=payload.playback.fresh)
                    resolver(payload.playback.profile)
                    result = app.state.sessions.create(info, payload.playback, base(request))
                elif payload.action == "library":
                    result = await app.state.library.execute(payload.library.model_dump())
                elif payload.action == "auth":
                    if payload.auth.action == "start":
                        result = await signin().start(payload.auth.name)
                    elif payload.auth.action == "status":
                        result = await signin().status(payload.auth.session)
                    else:
                        result = signin().cancel(payload.auth.session)
                else:
                    identifier = video_id(payload.id)
                    info = await resolver(payload.profile).extract(f"https://www.youtube.com/watch?v={identifier}")
                    result = {"item": item(info).model_dump(), "chapters": info.get("chapters") or []}
                if payload.action == "auth" and isinstance(result, dict) and result.get("state") == "authorized":
                    asyncio.current_task().profile = result.get("profile", {}).get("id", "guest")
                return {"state": "done", "result": result.model_dump() if hasattr(result, "model_dump") else result}
            except ServiceError as exc:
                return {"state": "failed", "error": {"code": exc.code, "message": exc.message}}
            except Exception:
                return {"state": "failed", "error": {"code": "service_failure", "message": "The video service could not complete this request. Retry."}}
        identifier = secrets.token_urlsafe(18)
        task = asyncio.create_task(work())
        task.auth_start = payload.action == "auth" and payload.auth.action == "start"
        task.chat_read = payload.action == "library" and payload.library.action in {"live_chat", "chat_poll"}
        task.auth_delivered = False
        task.profile = (payload.playback.profile if payload.action == "resolve" else
                        payload.library.profile if payload.action == "library" else payload.profile)
        app.state.jobs[identifier] = (now + 120, task)
        return {"api_version": 1, "job": identifier, "state": "pending"}

    @app.get("/v1/jobs/{identifier}")
    async def poll_job(identifier: str):
        job = app.state.jobs.get(identifier)
        if not job or job[0] <= time.monotonic():
            if job:
                cancel_auth_result(job[1])
                job[1].cancel()
            raise ServiceError("expired_job", "This request expired. Retry from the current screen.", 410)
        if not job[1].done():
            return {"api_version": 1, "state": "pending"}
        if job[1].cancelled():
            return {"api_version": 1, "state": "cancelled"}
        job[1].auth_delivered = True
        return {"api_version": 1, **job[1].result()}

    @app.delete("/v1/jobs/{identifier}")
    async def cancel_job(identifier: str):
        job = app.state.jobs.pop(identifier, None)
        if job:
            cancel_auth_result(job[1])
            job[1].cancel()
        return {"state": "cancelled"}

    async def upstream(url, headers, method="GET"):
        safe_remote(url)
        origin = urlsplit(url).hostname
        for _ in range(6):
            try:
                request = app.state.http.build_request(method, url, headers=headers)
                reply = await app.state.http.send(request, stream=True)
            except httpx.HTTPError:
                raise ServiceError("media_network", "The upstream media connection failed. Retry playback.", 502) from None
            if reply.status_code in (301, 302, 303, 307, 308):
                target = urljoin(url, reply.headers.get("location", ""))
                await reply.aclose()
                safe_remote(target)
                if urlsplit(target).hostname != origin:
                    headers = {k: v for k, v in headers.items() if k.lower() not in {"cookie", "authorization"}}
                url = target
                continue
            return reply, url
        raise ServiceError("media_redirects", "The upstream media redirected too many times.")

    async def bounded(reply, limit):
        data = bytearray()
        try:
            async for chunk in reply.aiter_bytes():
                data.extend(chunk)
                if len(data) > limit:
                    raise ServiceError("media_too_large", "The media metadata exceeded its size limit.")
            return bytes(data)
        finally:
            await reply.aclose()

    def relay_profile(session):
        # Account removal stops even a long stream whose capability has since
        # expired/pruned. Do not turn the existing start-request TTL into a
        # cutoff for a healthy, already-open media response.
        if library is not None and hasattr(library, "has_profile") and not library.has_profile(session.profile):
            raise ServiceError("expired_session", "Playback information expired. Select the video again to refresh it.", 410)

    @app.api_route("/media/{session_token}/{key}", methods=["GET", "HEAD"])
    async def media(session_token: str, key: str, request: Request):
        session = app.state.sessions.get(session_token)
        if key in session.manifests or key in session.hls_manifests:
            app.state.sessions.touch(session)
            manifest = session.hls_manifests.get(key, session.manifests.get(key, ''))
            mime = 'application/vnd.apple.mpegurl' if key in session.hls_manifests else 'application/dash+xml'
            return Response(manifest if request.method == "GET" else "", media_type=mime,
                            headers={"Cache-Control": "no-store"})
        remote = session.tracks.get(key)
        if not remote:
            raise ServiceError("missing_media", "The playback resource does not exist.", 404)
        byte_range = request.headers.get("range")
        if byte_range:
            if len(byte_range) > 100 or not re.fullmatch(r"bytes=(?:[0-9]+-[0-9]*|-[0-9]+)", byte_range):
                raise ServiceError("invalid_range", "Only a single byte range is supported.", 416)
        async def fetch(resource, renewed=False):
            headers = {**resource.headers, "Accept-Encoding": "identity"}
            if byte_range:
                headers["Range"] = byte_range
                if renewed and resource.representation.validator:
                    headers["If-Range"] = resource.representation.validator[1].encode('latin-1')
            return await upstream(resource.url, headers, request.method)
        reply, final_url = await fetch(remote)
        try:
            relay_profile(session)
        except ServiceError:
            await reply.aclose()
            raise
        if reply.status_code in (401, 403, 410):
            await reply.aclose()
            try:
                remote = await renew_media(app.state.sessions, session, key, remote, resolver)
                reply, final_url = await fetch(remote, True)
                relay_profile(session)
                if reply.status_code not in (200, 206):
                    raise expired_media()
                observe_media(remote, reply, byte_range, renewed=True)
            except (ServiceError, httpx.HTTPError, TimeoutError):
                await reply.aclose()
                app.state.sessions.sessions.pop(session_token, None)
                raise expired_media() from None
        if reply.status_code not in (200, 206):
            status = reply.status_code
            await reply.aclose()
            raise ServiceError("upstream_media", "The media server could not provide this resource.", 416 if status == 416 else 502)
        content_type = reply.headers.get("content-type", remote.mime)
        if remote.binding is not None:
            try:
                observe_media(remote, reply, byte_range)
            except ServiceError:
                await reply.aclose()
                app.state.sessions.sessions.pop(session_token, None)
                raise
        if key.endswith(".srt") and request.method == "GET":
            raw = await bounded(reply, 2 * 1024 * 1024)
            relay_profile(session)
            app.state.sessions.touch(session)
            return Response(vtt_to_srt(raw), media_type="application/x-subrip", headers={"Cache-Control": "no-store"})
        if request.method == "GET" and ("mpegurl" in content_type or final_url.split("?")[0].endswith(".m3u8")):
            raw = await bounded(reply, 2 * 1024 * 1024)
            relay_profile(session)
            try:
                rewritten = hls_rewrite(raw.decode("utf-8-sig"), final_url, session, base(request), key, remote)
            except UnicodeError:
                raise ServiceError("invalid_playlist", "The live playlist could not be read.") from None
            app.state.sessions.touch(session)
            return Response(rewritten, media_type="application/vnd.apple.mpegurl", headers={"Cache-Control": "no-store"})
        outgoing = {k: reply.headers[k] for k in ("content-length", "content-range", "accept-ranges") if k in reply.headers}
        outgoing["Cache-Control"] = "no-store"
        if request.method == "HEAD":
            await reply.aclose()
            return Response(status_code=reply.status_code, headers=outgoing, media_type=content_type)
        async def body():
            try:
                async for chunk in reply.aiter_raw():
                    try:
                        relay_profile(session)
                    except ServiceError:
                        return
                    app.state.sessions.touch(session)
                    remote.representation.delivered = True
                    yield chunk
            finally:
                await reply.aclose()
        return StreamingResponse(body(), status_code=reply.status_code, headers=outgoing, media_type=content_type)

    @app.get("/v1/sponsor/{identifier}")
    async def sponsor(identifier: str, categories: str = Query(default="sponsor,selfpromo", max_length=160)):
        video_id(identifier)
        allowed = {"sponsor", "selfpromo", "interaction", "intro", "outro", "preview", "music_offtopic", "filler"}
        chosen = set(categories.split(","))
        if not chosen <= allowed:
            raise ServiceError("invalid_categories", "An unsupported SponsorBlock category was requested.", 400)
        if not app.state.sponsorblock:
            return {"segments": [], "enabled": False}
        cache_key = (identifier, tuple(sorted(chosen)))
        cached = app.state.sponsor_cache.get(cache_key)
        if cached and cached[0] > time.monotonic():
            return {"segments": cached[1], "enabled": True}
        prefix = hashlib.sha256(identifier.encode()).hexdigest()[:4]
        segments = []
        try:
            reply = await app.state.http.get(f"https://sponsor.ajay.app/api/skipSegments/{prefix}",
                                            params={"categories": json.dumps(sorted(chosen))}, timeout=5)
            if reply.status_code == 200 and len(reply.content) <= 1024 * 1024:
                for result in reply.json():
                    if result.get("videoID") == identifier:
                        for s in result.get("segments", []):
                            pair = s.get("segment", [])
                            if (s.get("category") in chosen and s.get("actionType", "skip") == "skip"
                                and len(pair) == 2 and all(isinstance(n, (int, float)) for n in pair)
                                and 0 <= pair[0] < pair[1] < 604800):
                                segments.append({"start": pair[0], "end": pair[1], "category": s["category"]})
        except (httpx.HTTPError, ValueError, TypeError, KeyError):
            pass  # Optional service failures never become playback failures.
        if len(app.state.sponsor_cache) >= 128:
            app.state.sponsor_cache.pop(next(iter(app.state.sponsor_cache)))
        app.state.sponsor_cache[cache_key] = (time.monotonic() + 3600, segments[:128])
        return {"segments": segments[:128], "enabled": True}

    return app
