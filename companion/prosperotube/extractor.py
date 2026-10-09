"""Cancellable, bounded yt-dlp subprocesses keep the HTTP loop responsive."""
import asyncio
import json
import os
import signal
import sys
import time
from .models import ServiceError
from .resolver_errors import service_error


class Extractor:
    def __init__(self, *, timeout=45, workers=2, cookies=None, cookie_header=None, extractor_args=None, js_runtimes=None):
        self.timeout = timeout
        self.semaphore = asyncio.Semaphore(workers)
        self.cookies = cookies
        self.cookie_header = cookie_header
        self.extractor_args = extractor_args or {}
        self.js_runtimes = js_runtimes
        self.cache = {}
        self.revoked = False
        self.processes = set()

    def ensure_active(self):
        if self.revoked:
            raise ServiceError("unknown_profile", "Select an available profile.", 404)

    @staticmethod
    def _kill_worker(process):
        identity = getattr(process, "pid", None)
        if os.name == "posix" and type(identity) is int and identity > 0:
            try:
                os.killpg(identity, signal.SIGKILL)
            except ProcessLookupError:
                pass
        elif process.returncode is None:
            process.kill()

    def invalidate(self):
        self.revoked = True
        self.cache.clear()
        for process in tuple(self.processes):
            self._kill_worker(process)

    async def extract(self, target: str, *, flat=False, start=1, end=24, _oauth_access_token=None):
        self.ensure_active()
        key = (target, flat, start, end)
        cached = self.cache.get(key)
        if cached and cached[0] > time.monotonic():
            return cached[1]
        async with self.semaphore:
            self.ensure_active()
            process = await asyncio.create_subprocess_exec(
                sys.executable, "-m", "prosperotube.worker",
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL,
                start_new_session=os.name == "posix")
            self.processes.add(process)
            try:
                self.ensure_active()
                payload = json.dumps({"target": target, "flat": flat, "start": start, "end": end,
                                  "cookies": self.cookies, "extractor_args": self.extractor_args,
                                  "cookie_header": self.cookie_header,
                                  "js_runtimes": self.js_runtimes,
                                  **({"oauth_access_token": _oauth_access_token} if _oauth_access_token else {})}).encode()
                async with asyncio.timeout(self.timeout):
                    process.stdin.write(payload)
                    await process.stdin.drain()
                    process.stdin.close()
                    chunks = []
                    size = 0
                    while chunk := await process.stdout.read(65536):
                        size += len(chunk)
                        if size > 8 * 1024 * 1024:
                            raise ServiceError("response_too_large", "The video service response exceeded its size limit.")
                        chunks.append(chunk)
                    await process.wait()
                self.ensure_active()
                if process.returncode:
                    code = None
                    try:
                        failure = json.loads(b"".join(chunks))
                        if isinstance(failure, dict) and set(failure) == {"worker_error"}:
                            diagnostic = failure["worker_error"]
                            if isinstance(diagnostic, dict) and set(diagnostic) == {"version", "code"} and type(diagnostic["version"]) is int and diagnostic["version"] == 1:
                                code = diagnostic["code"]
                    except (ValueError, UnicodeError):
                        pass
                    raise service_error(code)
                result = json.loads(b"".join(chunks))
                if not isinstance(result, dict):
                    raise ValueError("expected object")
            except TimeoutError:
                raise ServiceError("timeout", "YouTube took too long to respond. Retry from the video screen.", 504) from None
            except (ValueError, UnicodeError):
                raise ServiceError("invalid_response", "The resolver returned an invalid response.") from None
            finally:
                # yt-dlp starts a Node evaluator for signature/n challenges.
                # Canceling only Python can leave that child evaluating forever.
                # Retire our isolated WSL/Linux group even after Python exits.
                self.processes.discard(process)
                self._kill_worker(process)
                await process.wait()
            self.ensure_active()
            if len(self.cache) >= 128:
                self.cache.pop(next(iter(self.cache)))
            self.cache[key] = (time.monotonic() + (180 if flat else 30), result)
            return result


class ProfileExtractors:
    """Immutable credential/cache partitions sharing the subprocess limit."""
    def __init__(self, guest, profiles, library=None):
        self.guest = guest
        self.profile_source = profiles if callable(profiles) else lambda: profiles
        self.extractors = {"guest": guest}
        self.library = library

    def invalidate_profile(self, identity):
        resolver = self.extractors.pop(identity, None)
        if resolver is not None and identity != "guest":
            resolver.invalidate()

    def for_profile(self, profile):
        if profile == "guest":
            return self.guest
        configured = next((p for p in self.profile_source() if p["id"] == profile), None)
        if configured is None:
            raise ServiceError("unknown_profile", "Select an available profile.", 404)
        if "oauth" in configured:
            from .device_auth import credentials_valid
            if not credentials_valid(configured["oauth"]):
                raise ServiceError("authentication_required", "Sign in to the YouTube account again.", 403)
            if self.library is None:
                raise ServiceError("playback_oauth_context", "Enable the account library for OAuth playback.", 503)
            if profile not in self.extractors:
                from .oauth_extractor import OAuthExtractor
                self.extractors[profile] = OAuthExtractor(profile, self.library, self.guest)
            return self.extractors[profile]
        if configured.get("account_index", 0) or configured.get("channel"):
            raise ServiceError("playback_account_context", "Playback for secondary accounts or brand channels is not connected yet.", 503)
        if profile not in self.extractors:
            resolver = Extractor(timeout=self.guest.timeout, cookie_header=configured["cookie"],
                                 extractor_args=self.guest.extractor_args, js_runtimes=self.guest.js_runtimes)
            resolver.semaphore = self.guest.semaphore
            self.extractors[profile] = resolver
        return self.extractors[profile]

    async def extract(self, target, **kwargs):
        return await self.guest.extract(target, **kwargs)
