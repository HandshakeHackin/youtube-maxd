"""Exercise the real companion voice job with a local PCM WAV and whisper.cpp."""
import argparse
import asyncio
import base64
import json
from pathlib import Path
import sys
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "companion"))
import httpx
from prosperotube.app import create_app
from prosperotube.voice import WhisperCLI


async def probe(args):
    with wave.open(str(args.audio), "rb") as source:
        if (source.getnchannels(), source.getsampwidth(), source.getframerate()) != (1, 2, 16000):
            raise ValueError("Use a 16 kHz signed-16 mono WAV.")
        pcm = source.readframes(16000 * 8)
    backend = WhisperCLI(args.cli, args.model, language=args.language)
    app = create_app(token="synthetic-local-voice-probe", voice=backend)
    headers = {"Authorization": "Bearer synthetic-local-voice-probe"}
    async with app.router.lifespan_context(app):
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
            response = await client.post("/v1/voice", headers=headers,
                json={"profile": "guest", "pcm_s16le": base64.b64encode(pcm).decode()})
            response.raise_for_status()
            job = response.json()["job"]
            for _ in range(600):
                response = await client.get("/v1/jobs/" + job, headers=headers)
                response.raise_for_status()
                result = response.json()
                if result["state"] != "pending":
                    break
                await asyncio.sleep(.1)
            else:
                raise RuntimeError("Voice job did not finish within one minute.")
            if result["state"] != "done":
                raise RuntimeError("Speech recognition failed: " + result.get("error", {}).get("code", "unknown"))
            repeated = (await client.get("/v1/jobs/" + job, headers=headers)).json()
            if repeated != result:
                raise RuntimeError("Repeated polling changed the recognized query.")
            await client.delete("/v1/jobs/" + job, headers=headers)
            result = {"voice_job": "passed", "seconds": len(pcm) / 32000, "query": result["result"]["query"]}
    if args.output:
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    else:
        print(json.dumps(result, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--audio", type=Path, required=True, help="Uses the first eight seconds of a local WAV")
    parser.add_argument("--language", default="auto")
    parser.add_argument("--output", type=Path)
    asyncio.run(probe(parser.parse_args()))


if __name__ == "__main__":
    main()
