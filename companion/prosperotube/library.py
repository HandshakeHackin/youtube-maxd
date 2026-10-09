"""Bounded InnerTube worker; account credentials remain on the companion."""
import asyncio
import http.cookiejar
import json
from pathlib import Path
import re
import secrets

from .models import ServiceError


def load_profiles(config):
    if config is None:
        return []
    path = Path(config)
    if path.stat().st_size > 65536:
        raise ValueError("The profile configuration exceeds 64 KiB.")
    profiles = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(profiles, list) or len(profiles) > 8:
        raise ValueError("Configure at most eight profiles.")
    output, identities = [], {"guest"}
    for profile in profiles:
        if not isinstance(profile, dict):
            raise ValueError("Each profile must be an object.")
        identity = profile.get("id", "")
        if not isinstance(identity, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", identity) or identity in identities:
            raise ValueError("Profile identities must be unique short identifiers.")
        identities.add(identity)
        index = profile.get("account_index", 0)
        if type(index) is not int or not 0 <= index <= 7:
            raise ValueError("Account indexes must be between zero and seven.")
        channel = profile.get("channel", "")
        if not isinstance(channel, str) or (channel and not re.fullmatch(r"[A-Za-z0-9_-]{1,100}", channel)):
            raise ValueError("The profile channel identifier is invalid.")
        cookie_path = Path(profile["cookie_file"])
        if not cookie_path.is_absolute():
            cookie_path = path.parent / cookie_path
        if cookie_path.stat().st_size > 65536:
            raise ValueError("Cookie files must be at most 64 KiB.")
        contents = cookie_path.read_text(encoding="utf-8")
        if contents.startswith("# Netscape HTTP Cookie File") or contents.startswith("# HTTP Cookie File"):
            jar = http.cookiejar.MozillaCookieJar(str(cookie_path))
            jar.load(ignore_discard=True, ignore_expires=False)
            cookie = "; ".join(f"{item.name}={item.value}" for item in jar
                               if item.domain.lstrip(".") == "youtube.com" and not item.is_expired())
        else:
            cookie = contents.strip()
        if not cookie or len(cookie) > 65536 or "\r" in cookie or "\n" in cookie:
            raise ValueError("Provide a valid YouTube cookie file for each account profile.")
        value = {"id": identity, "name": str(profile.get("name", identity))[:192],
                 "cookie": cookie, "account_index": index}
        if channel:
            value["channel"] = channel
        output.append(value)
    return output


class LibraryService:
    def __init__(self, node="node", *, profile_config=None, worker=None, timeout=30):
        self.node = node
        self.worker = Path(worker) if worker else Path(__file__).resolve().parents[1] / "innertube" / "worker.bundle.mjs"
        self.profiles = load_profiles(profile_config)
        self.timeout = timeout
        self.process = None
        self.lock = asyncio.Lock()
        self.pending = 0
        self.closed = False

    async def _stop(self):
        process, self.process = self.process, None
        if process is not None:
            if process.returncode is None:
                try:
                    process.kill()
                except ProcessLookupError:
                    pass
            await process.wait()

    async def _exchange(self, payload):
        identity = secrets.token_hex(16)
        body = json.dumps({"id": identity, **payload}, ensure_ascii=False).encode("utf-8") + b"\n"
        self.process.stdin.write(body)
        await self.process.stdin.drain()
        response = await self.process.stdout.readline()
        if not response or len(response) > 1048576:
            raise ServiceError("library_transport", "The library worker returned an invalid response.")
        value = json.loads(response)
        if not isinstance(value, dict) or value.get("id") != identity:
            raise ServiceError("library_transport", "The library worker returned an invalid response identity.")
        if "error" in value:
            error = value["error"]
            # Only the original worker's bounded public errors cross this boundary.
            code = error.get("code", "library_failure")
            status = error.get("status", 502)
            if not isinstance(code, str) or not re.fullmatch(r"[a-z_]{1,64}", code):
                code = "library_failure"
            if type(status) is not int or not 400 <= status <= 599:
                status = 502
            message = error.get("message", "The library operation failed.")
            if not isinstance(message, str) or len(message) > 256:
                message = "The library operation failed."
            raise ServiceError(code, message, status)
        result = value.get("result")
        if not isinstance(result, dict):
            raise ServiceError("library_transport", "The library worker returned invalid operation data.")
        return result

    async def _start(self):
        if self.process is not None and self.process.returncode is None:
            return
        self.process = await asyncio.create_subprocess_exec(
            str(self.node), str(self.worker), stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
            limit=1048577)
        result = await self._exchange({"operation": "init", "profiles": self.profiles})
        if result.get("ready") is not True:
            raise ServiceError("library_transport", "The library worker did not initialize.")

    async def execute(self, request):
        if self.closed:
            raise ServiceError("library_closed", "The library service has stopped.", 503)
        if not isinstance(request, dict) or len(json.dumps(request).encode("utf-8")) > 8192:
            raise ServiceError("library_request_limit", "The library operation exceeds its request limit.", 400)
        if self.pending >= 16:
            raise ServiceError("library_busy", "The library is busy. Retry shortly.", 429)
        self.pending += 1
        owned = False
        try:
            async with asyncio.timeout(self.timeout):
                async with self.lock:
                    if self.closed:
                        raise ServiceError("library_closed", "The library service has stopped.", 503)
                    owned = True
                    await self._start()
                    return await self._exchange({"operation": "execute", "request": request})
        except asyncio.CancelledError:
            if owned:
                await self._stop()
            raise
        except TimeoutError:
            if owned:
                await self._stop()
            raise ServiceError("library_timeout", "The operation timed out. Check the account before repeating a change.", 504) from None
        except ServiceError as error:
            if owned and error.code == "library_transport":
                await self._stop()
            raise
        except (OSError, ValueError, ConnectionError):
            if owned:
                await self._stop()
            raise ServiceError("library_transport", "The library worker is unavailable. Check its installation.", 503) from None
        finally:
            self.pending -= 1

    async def close(self):
        self.closed = True
        async with self.lock:
            await self._stop()
