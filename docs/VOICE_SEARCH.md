# Local voice search

The companion speech-recognition service is implemented and tested with real
whisper.cpp inference. The native AudioIn capture worker passes host lifecycle
tests and PS5 SDK compilation. Upload/polling, search-screen integration and actual
microphone acceptance remain open; voice search is not yet enabled on the console.
Lounge microphone capability remains withheld.

`native/tube_microphone.c` records 0.5–10 seconds of 16 kHz signed-16 mono PCM
through the PS5 VoiceRecognition input route. AudioIn open/input/close stay on an
owned thread. The main thread polls completion without joining an active capture;
early finish supports push-to-talk, while cancellation discards even completed
audio. Silence, input/open/user/close failures and a recording deadline reject the
result. Samples stay in memory and are wiped on retirement. Shutdown joins the
worker before freeing its storage; cancellation takes effect after the current
AudioIn grain returns. Native capture code is not yet included in the app package.
Run its deterministic lifecycle and fault tests with `bash scripts/test_microphone.sh`.

The service uses a locally configured executable and model. It does not send
recordings to a cloud speech service. Temporary WAV and transcript files are
deleted on success, failure, timeout and cancellation. Completed query results
remain available in the short-lived job cache for repeat polling; clients should
delete the job after accepting the query.

On a Linux/WSL companion host with Git, CMake and a C++ compiler:

```sh
bash scripts/setup_voice.sh
python -m prosperotube --host 0.0.0.0 --public-base http://YOUR-LAN-IP:8765 \
  --voice-cli "$HOME/.cache/prosperotube/whisper-cpp/build/bin/whisper-cli" \
  --voice-model "$HOME/.cache/prosperotube/whisper-cpp/models/ggml-tiny.bin"
```

The setup script pins whisper.cpp v1.9.5 source and checks the multilingual tiny
model's SHA-256. It builds a companion-host executable. The PS5 capture path will
use the resident PS5 AudioIn library demonstrated in the existing
ps5-audio-decoding-research project. A Windows companion can use a local Windows
whisper-cli executable and GGML model with the same arguments. Supply
`--voice-language en` (or another supported language code) to override detection.
Larger compatible models can be selected explicitly for improved recognition.

`POST /v1/voice` requires the normal paired bearer token. Its JSON contains a
selected `profile` and `pcm_s16le`: standard base64 of 16 kHz signed-16 mono PCM,
between 0.5 and 10 seconds. Only this endpoint admits the larger bounded body;
ordinary API requests retain their 8 KiB limit. Chunked uploads stop at the bound.
Invalid requests never echo audio into validation errors.

The response supplies a pending job ID. Poll `/v1/jobs/{job}` until it returns
`result.query`, or cancel with `DELETE /v1/jobs/{job}`. Queries are normalized and
bounded to 200 characters. Only one transcription runs at once; recognition has
a 45-second deadline. Cancellation, profile retirement, expiry and server shutdown
retire owned subprocesses. Unavailable recognition, silence and busy/failed jobs
return explicit errors.

`scripts/probe_voice.py` exercises the real authenticated upload/job/poll/delete
path using the first eight seconds of an explicitly supplied 16 kHz mono WAV.
Keep recordings and probe output outside Git. Example with whisper.cpp's public
sample:

```sh
python scripts/probe_voice.py \
  --cli "$HOME/.cache/prosperotube/whisper-cpp/build/bin/whisper-cli" \
  --model "$HOME/.cache/prosperotube/whisper-cpp/models/ggml-tiny.bin" \
  --audio "$HOME/.cache/prosperotube/whisper-cpp/samples/jfk.wav"
```
