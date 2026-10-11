import asyncio
import base64
from functools import wraps
import json
import os
from pathlib import Path
import struct
import time

from fastapi.testclient import TestClient
import httpx
import pytest

from prosperotube.app import create_app
from prosperotube.models import ServiceError
from prosperotube.voice import MAX_PCM, MAX_REQUEST, WhisperCLI, decode_pcm

TOKEN = "synthetic-voice-token"
AUTH = {"Authorization": "Bearer " + TOKEN}
PCM = struct.pack("<h", 2000) * 24000
PAYLOAD = {"profile": "guest", "pcm_s16le": base64.b64encode(PCM).decode()}


def asynchronous(function):
    @wraps(function)
    def execute(*args, **kwargs):
        return asyncio.run(function(*args, **kwargs))
    return execute


class Voice:
    def __init__(self, block=False):
        self.block, self.calls, self.cancelled = block, 0, False

    async def transcribe(self, pcm):
        assert pcm == PCM
        self.calls += 1
        try:
            if self.block:
                await asyncio.Future()
            return "kittens and puppies"
        finally:
            self.cancelled = True


def client(voice=None):
    return TestClient(create_app(token=TOKEN, voice=voice))


def result(c, identifier):
    for _ in range(50):
        response = c.get("/v1/jobs/" + identifier, headers=AUTH)
        if response.json().get("state") != "pending":
            return response
        time.sleep(.01)
    raise AssertionError("voice job did not finish")


def test_voice_authenticated_upload_poll_repeat_and_delete():
    backend = Voice()
    with client(backend) as c:
        assert "voice" in c.get("/v1/health").json()["features"]
        assert c.post("/v1/voice", json=PAYLOAD).status_code == 401
        response = c.post("/v1/voice", headers=AUTH, json=PAYLOAD)
        assert response.status_code == 200
        identifier = response.json()["job"]
        completed = result(c, identifier).json()
        assert completed["state"] == "done" and completed["result"]["query"] == "kittens and puppies"
        assert result(c, identifier).json() == completed and backend.calls == 1
        assert c.delete("/v1/jobs/" + identifier, headers=AUTH).status_code == 200
        assert c.get("/v1/jobs/" + identifier, headers=AUTH).status_code == 410
        assert c.post("/v1/jobs", headers=AUTH, json={"action": "search", "query": "x" * 9000}).status_code == 413


def test_unavailable_profile_scope_and_invalid_audio():
    with client() as c:
        assert "voice" not in c.get("/v1/health").json()["features"]
        assert c.post("/v1/voice", headers=AUTH, json=PAYLOAD).status_code == 503
    with client(Voice()) as c:
        assert c.post("/v1/voice", headers=AUTH, json={**PAYLOAD, "profile": "other"}).status_code == 404
        for encoded in ("?", "eA==", base64.b64encode(b"\0" * 32000).decode()):
            assert c.post("/v1/voice", headers=AUTH, json={**PAYLOAD, "pcm_s16le": encoded}).status_code == 400
        invalid = c.post("/v1/voice", headers=AUTH, json={**PAYLOAD, "execute": "anything"})
        assert invalid.status_code == 422 and len(invalid.content) < 300 and "pcm_s16le" not in invalid.text
        assert c.post("/v1/voice", headers=AUTH, content=b"x" * (MAX_REQUEST + 1)).status_code == 413


@pytest.mark.parametrize("finish", ["delete", "expiry", "shutdown"])
def test_single_voice_job_and_owned_cancellation(finish):
    backend = Voice(block=True)
    with client(backend) as c:
        identifier = c.post("/v1/voice", headers=AUTH, json=PAYLOAD).json()["job"]
        assert c.post("/v1/voice", headers=AUTH, json=PAYLOAD).json()["error"]["code"] == "voice_busy"
        if finish == "delete":
            c.delete("/v1/jobs/" + identifier, headers=AUTH)
        elif finish == "expiry":
            _, task = c.app.state.jobs[identifier]
            c.app.state.jobs[identifier] = (0, task)
            assert c.get("/v1/jobs/" + identifier, headers=AUTH).status_code == 410
    assert backend.cancelled and backend.calls == 1


@asynchronous
async def test_chunked_audio_stops_reading_at_limit():
    app = create_app(token=TOKEN, voice=Voice())
    count = 0
    async def chunks():
        nonlocal count
        for _ in range(100):
            count += 1
            yield b"x" * 65536
    async with app.router.lifespan_context(app):
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as c:
            response = await c.post("/v1/voice", headers=AUTH, content=chunks())
            assert response.status_code == 413 and count <= 8


@pytest.mark.parametrize("pcm", [b"", b"x" * 15998, b"x" * 16001, b"x" * (MAX_PCM + 2), b"\0" * 32000])
def test_pcm_boundaries(pcm):
    with pytest.raises(ServiceError):
        decode_pcm(base64.b64encode(pcm).decode())


def fake_cli(tmp_path, mode="success"):
    executable = tmp_path / "whisper-cli"
    executable.write_text("""#!/usr/bin/env python3
import json, os, sys, time, wave
from pathlib import Path
args=sys.argv
source=Path(args[args.index('-f')+1])
output=Path(args[args.index('-of')+1]+'.txt')
root=Path(__file__).resolve().parent
with wave.open(str(source)) as wav:
    assert (wav.getnchannels(),wav.getsampwidth(),wav.getframerate(),wav.getnframes())==(1,2,16000,24000)
(root/'marker.json').write_text(json.dumps(dict(pid=os.getpid(),source=str(source))))
mode=(root/'mode').read_text()
if mode=='hang':time.sleep(60)
if mode=='fail':sys.exit(1)
output.write_text('[BLANK_AUDIO]' if mode=='blank' else 'x'*17000 if mode=='oversize' else '  kittens \\n and puppies  ')
""")
    executable.chmod(0o700)
    (tmp_path / "mode").write_text(mode)
    model = tmp_path / "model.bin"
    model.write_bytes(b"synthetic-model")
    return WhisperCLI(executable, model)


@pytest.mark.parametrize("mode", ["success", "fail", "blank", "oversize", "hang"])
@asynchronous
async def test_cli_real_process_output_timeout_and_file_cleanup(tmp_path, mode):
    backend = fake_cli(tmp_path, mode)
    if mode == "hang":
        backend.timeout = .5
    if mode == "success":
        assert await backend.transcribe(PCM) == "kittens and puppies"
    else:
        with pytest.raises(ServiceError):
            await backend.transcribe(PCM)
    marker = json.loads((tmp_path / "marker.json").read_text())
    assert not Path(marker["source"]).exists() and not backend.busy
    with pytest.raises(ProcessLookupError):
        os.kill(marker["pid"], 0)


@asynchronous
async def test_cli_cancellation_kills_owned_process_and_releases_slot(tmp_path):
    backend = fake_cli(tmp_path, "hang")
    task = asyncio.create_task(backend.transcribe(PCM))
    for _ in range(100):
        if (tmp_path / "marker.json").exists():
            break
        await asyncio.sleep(.01)
    marker = json.loads((tmp_path / "marker.json").read_text())
    with pytest.raises(ServiceError) as error:
        await backend.transcribe(PCM)
    assert error.value.code == "voice_busy"
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task
    assert not Path(marker["source"]).exists() and not backend.busy
    with pytest.raises(ProcessLookupError):
        os.kill(marker["pid"], 0)
    (tmp_path / "mode").write_text("success")
    assert await backend.transcribe(PCM) == "kittens and puppies"
