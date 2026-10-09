"""Cancellable, bounded yt-dlp subprocesses keep the HTTP loop responsive."""
import asyncio
import json
import os
import sys
import time
from .models import ServiceError


class Extractor:
    def __init__(self, *, timeout=45, workers=2, cookies=None, extractor_args=None, js_runtimes=None):
        self.timeout = timeout
        self.semaphore = asyncio.Semaphore(workers)
        self.cookies = cookies
        self.extractor_args = extractor_args or {}
        self.js_runtimes = js_runtimes
        self.cache = {}

    async def extract(self, target: str, *, flat=False, start=1, end=24):
        key = (target, flat, start, end)
        cached = self.cache.get(key)
        if cached and cached[0] > time.monotonic():
            return cached[1]
        async with self.semaphore:
            process = await asyncio.create_subprocess_exec(
                sys.executable, "-m", "prosperotube.worker",
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL)
            payload = json.dumps({"target": target, "flat": flat, "start": start, "end": end,
                                  "cookies": self.cookies, "extractor_args": self.extractor_args,
                                  "js_runtimes": self.js_runtimes}).encode()
            try:
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
                if process.returncode:
                    raise ServiceError("extraction_failed", "YouTube could not be resolved. Check the companion's yt-dlp/runtime configuration and retry.")
                result = json.loads(b"".join(chunks))
                if not isinstance(result, dict):
                    raise ValueError("expected object")
            except TimeoutError:
                raise ServiceError("timeout", "YouTube took too long to respond. Retry from the video screen.", 504) from None
            except (ValueError, UnicodeError):
                raise ServiceError("invalid_response", "The resolver returned an invalid response.") from None
            finally:
                if process.returncode is None:
                    process.kill()
                await process.wait()
            if len(self.cache) >= 128:
                self.cache.pop(next(iter(self.cache)))
            self.cache[key] = (time.monotonic() + (180 if flat else 30), result)
            return result
