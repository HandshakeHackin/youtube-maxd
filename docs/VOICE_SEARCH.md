# Local voice search

**This page describes voice search with the external development companion.
The release, whose service runs inside the app, has no voice search:** the L3
shortcut and its help line are hidden, `/v1/voice` answers `voice_unavailable`
and `voice` is not in the service's feature list. The reasons and the ways
forward are in [Without a companion](#without-a-companion-assessment-of-2026-10-13).

The companion speech-recognition service is implemented and tested with real
whisper.cpp inference. The native AudioIn capture worker and scoped upload/job
pipeline pass host fault tests and PS5 SDK compilation. Local/phone controls and
native search navigation are connected and compile with the actual Application
and provider screen. Candidate `578ad430` built and froze with native AudioIn
imports (216 objects, 126 files). Physical microphone acceptance remains open.

`native/tube_microphone.c` records 0.5–10 seconds of 16 kHz signed-16 mono PCM
through the PS5 VoiceRecognition input route. AudioIn open/input/close stay on an
owned thread. The main thread polls completion without joining an active capture;
early finish supports push-to-talk, while cancellation discards even completed
audio. Silence, input/open/user/close failures and a recording deadline reject the
result. Samples stay in memory and are wiped on retirement. Shutdown joins the
worker before freeing its storage; cancellation takes effect after the current
AudioIn grain returns.
Run its deterministic lifecycle and fault tests with `bash scripts/test_microphone.sh`.

`native/tube_voice.c` connects capture to authenticated `/v1/voice` uploads and
job polling. It encodes PCM explicitly as little-endian base64, accepts a bounded
valid UTF-8 transcript, and preserves complete queries up to 200 code points.
Source/token/profile changes, phone-owner retirement, cancellation, shutdown and
a 75-second overall deadline retire the operation. Completed and abandoned jobs
are deleted with their original credentials, including a late create response
or a late poll when cancellation found all request slots occupied. A transport
failure can prevent deletion; the service's 120-second job expiry remains the
cleanup backstop. Run `bash scripts/test_voice_driver.sh` for the network/capture
substitute tests.

On the YouTube browser, hold L3 while speaking and release it to search; Circle
cancels recording or recognition. Recording stops automatically at six seconds.
Phone `voiceCommand` INITIATED/CANCELED events use the same pipeline and retain
receiver ownership. Starting voice search pauses active playback. Leaving the
browser or changing source/profile retires the recording and any pending result.
Recognized text enters the actual provider search handler, preserving the full
UTF-8 query. If speech recognition is unavailable, the UI reports failure.
Native service availability is now connected to receiver capability negotiation.
The client probes `/v1/health` before recording, checks again every minute while
idle and retries failed checks after ten seconds. Scope changes retire stale
health results. The receiver advertises `mic` only when native capture/search is
connected and the companion has local ASR configured. Capability changes retire
and rebind the listening receiver with the same saved identity; an already failed
receiver still requires an explicit retry. Older clients omit this capability,
and companions without ASR keep it disabled. The availability-gated package
`prosperotube-0b06bc66_1011-0111` built successfully (216 objects, 126 frozen
files; eboot SHA-256 prefix `4b3fd861c59b9048`). Physical microphone capture and
recognized-text search on PS5 still require hardware acceptance.

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

## Without a companion: assessment of 2026-10-13

Nothing was implemented; this records what a recogniser on the console would
take.

**What the native side needs.** Nothing new. Capture (`native/tube_microphone.c`)
is already on the console, and `native/tube_voice.c` only needs a service that
lists `voice` in `GET /v1/health` and answers `POST /v1/voice` (bearer, `profile`,
`pcm_s16le`: base64 of 0.5 to 10 seconds of 16 kHz signed-16 mono PCM, at most
about 427 KiB of text) with a job whose result is `query`, at most 200 characters.
The missing part is the recogniser alone.

**Options.**

1. **whisper.cpp and a model inside the package.** The model the companion
   used, `ggml-tiny.bin` (multilingual), is 75 MiB; upstream also publishes a
   5-bit quantized tiny of about 31 MiB and gives roughly 273 MB of memory in
   use for tiny. This repository holds no timing: the host probe recorded that
   recognition succeeded, not how long it took. Whisper encodes a fixed
   30-second window for every request, however short the recording, so the
   cost per query is constant; on desktop x86 with AVX2, which the console's
   processor has, tiny is usually reported at one to a few seconds on four
   threads. That is an expectation, not a measurement. The work: whisper.cpp
   and ggml (MIT, C and C++) as new third-party code built with the PS5
   toolchain; 31 to 75 MiB more in the package; a recogniser thread of its own
   (the engine thread must not stop for seconds) loaded on demand and freed,
   because 273 MB next to a playing 4K video is not free; the larger request
   body on one more route (the mechanism the subscriptions import uses); and a
   hardware session, since microphone capture itself has never been accepted
   on a console.
2. **YouTube's own voice search.** YouTube.js 18.1.0 exposes no speech
   endpoint: its only "voice" and "transcript" classes are voice-reply comment
   views and caption transcripts. YouTube's applications use the platform's
   speech recogniser, which is a Google cloud service outside InnerTube, with
   its own credentials. Using it would also send recordings to a cloud
   service, which this feature was designed not to do. Not available.
3. **Leave it out.** Chosen for this release. With a phone linked
   ([phone receiver](REMOTE_SERVICE.md#built-in-receiver)), the YouTube app's
   own voice search runs on the phone and casts the result, with nothing
   needed on the console. Using the phone as the TV's microphone
   (`voiceCommand`) stays unsupported: the receiver does not advertise `mic`.

**State of the tree.** The built-in service does not list `voice` and answers
`/v1/voice` with `voice_unavailable` (503). In built-in mode the provider keeps
the voice scope closed, L3 does not start a recording, the help line is not in
the package, and the list of experimental features names no voice search. With
the `external-companion` switch everything on this page works as before.
