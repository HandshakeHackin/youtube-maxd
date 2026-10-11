"""Bounded local speech recognition; temporary recordings are removed after use."""
import asyncio
import base64
import binascii
import os
from pathlib import Path
import re
import struct
import tempfile
import wave

from .models import ServiceError

RATE = 16000
MAX_PCM = RATE * 2 * 10
MAX_BASE64 = 4 * ((MAX_PCM + 2) // 3)
MAX_REQUEST = MAX_BASE64 + 1024


def decode_pcm(encoded):
    try:
        pcm = base64.b64decode(encoded, validate=True)
    except (binascii.Error, ValueError, TypeError):
        raise ServiceError("invalid_audio", "Record a short voice search and retry.", 400) from None
    if not RATE <= len(pcm) <= MAX_PCM or len(pcm) % 2:
        raise ServiceError("invalid_audio", "Record between half a second and ten seconds of speech.", 400)
    if not any(abs(sample[0]) >= 64 for sample in struct.iter_unpack("<h", pcm)):
        raise ServiceError("silent_audio", "No speech was captured. Unmute the controller microphone and retry.", 400)
    return pcm


class WhisperCLI:
    """An explicitly configured whisper.cpp executable and local GGML model."""
    def __init__(self, executable, model, *, language="auto", timeout=45):
        self.executable = Path(executable).expanduser().resolve(strict=True)
        self.model = Path(model).expanduser().resolve(strict=True)
        if not self.executable.is_file() or not self.model.is_file() or not os.access(self.executable, os.X_OK):
            raise ValueError("Voice recognition needs an executable and a local model file.")
        if not re.fullmatch(r"auto|[a-z]{2,3}", language):
            raise ValueError("Invalid voice recognition language.")
        self.language, self.timeout = language, timeout
        self.busy = False

    async def transcribe(self, pcm):
        if self.busy:
            raise ServiceError("voice_busy", "A voice search is already running. Wait or cancel it.", 503)
        self.busy = True
        process = None
        try:
            with tempfile.TemporaryDirectory(prefix="prosperotube-voice-") as directory:
                source = Path(directory) / "speech.wav"
                output = Path(directory) / "result"
                with wave.open(str(source), "wb") as wav:
                    wav.setnchannels(1)
                    wav.setsampwidth(2)
                    wav.setframerate(RATE)
                    wav.writeframes(pcm)
                os.chmod(source, 0o600)
                try:
                    process = await asyncio.create_subprocess_exec(
                        str(self.executable), "-m", str(self.model), "-f", str(source),
                        "-l", self.language, "-t", str(min(4, os.cpu_count() or 1)),
                        "-ng", "-nt", "-np", "-otxt", "-of", str(output),
                        stdin=asyncio.subprocess.DEVNULL, stdout=asyncio.subprocess.DEVNULL,
                        stderr=asyncio.subprocess.DEVNULL)
                    await asyncio.wait_for(process.wait(), self.timeout)
                    transcript = output.with_suffix(".txt")
                    if process.returncode or not transcript.is_file() or transcript.stat().st_size > 16384:
                        raise ServiceError("voice_failed", "Speech recognition failed. Retry voice search.", 502)
                    text = " ".join(transcript.read_text(encoding="utf-8").split())
                    if not text or text.startswith("[") or any(ord(char) < 32 for char in text):
                        raise ServiceError("no_speech", "No speech was recognized. Speak clearly and retry.", 422)
                    return text[:200]
                except asyncio.TimeoutError:
                    raise ServiceError("voice_timeout", "Speech recognition timed out. Retry voice search.", 504) from None
                except (OSError, UnicodeError):
                    raise ServiceError("voice_failed", "Speech recognition failed. Check the companion voice configuration.", 502) from None
                finally:
                    if process is not None and process.returncode is None:
                        try:
                            process.kill()
                        except ProcessLookupError:
                            pass
                        await process.wait()
        finally:
            self.busy = False
