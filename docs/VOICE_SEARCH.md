# Voice search

The standalone PS5 build includes CPU-only whisper.cpp 1.9.5 and the multilingual
Whisper tiny model (about 74 MiB). No PC, cloud account, API key, subscription or
speech service is required. Audio stays in memory on the console; recognized
text is submitted to normal YouTube search and follows the app's search-history
settings. This build has not yet been tested on PS5 hardware.

## Use

1. Open the YouTube browser in Classic or Afterglow. Connect and unmute your
   DualSense microphone. Voice search is unavailable inside account/PIN dialogs,
   native keyboards, settings sheets, or while opening a video.
2. Hold **L3** until **Listening** appears (350 ms), then speak your search.
3. Release L3 to search, or keep holding: capture ends after six seconds.
4. **Circle** cancels recording or recognition. Results open in the normal
   search page. Square opens typed search when you need to correct a query.

The controller light bar uses Max’d's red-to-white listening effect and loading effect
while recognizing. Playing media is paused before recording. A short L3 tap
keeps Afterglow's background-playback return action; Classic comments retain
their L3 refresh shortcut. Voice cannot be activated by remote clients.

Speak after the indicator appears. Very short, quiet, muted or unclear input
shows a retry message. Recognition is asynchronous, supports automatic language
detection, and can make mistakes. Capture stops at six seconds, with an overall
75-second timeout. Speed and microphone routing on firmware 11.00 remain part
of console acceptance; headset microphones and reconnect behavior are unverified.

## Build and checks

`scripts/build_ps5.sh` fetches and checks the pinned engine/model inputs, builds
CPU-only static PS5 libraries, and packages the model at
`PPSA99991/assets/voice/ggml-tiny.bin`. Keep this file with the app. Its SHA-256
is checked by the recognition worker before loading. MIT notices and the model
manifest are included beside it; the source ZIP includes the engine source.
The PS5 build excludes GGML's desktop debugger/process hooks and dynamic backend
discovery through `scripts/patch_local_voice.py`; only the static CPU backend
is used. Its build checks reject process-creation/discovery imports.

```sh
python3 scripts/bootstrap_local_voice.py
python3 scripts/build_local_voice.py
ASAN_OPTIONS=detect_leaks=0 bash scripts/test_local_voice.sh
ASAN_OPTIONS=detect_leaks=0 bash scripts/test_voice_driver.sh
```

The real-model host test recognizes speech, rejects silence and a corrupt
model, exercises cancellation/retry, and checks hold-versus-tap handling.
The driver tests check local ownership/profile retirement and the existing
optional companion flow. A host pass does not establish console functionality.

For console acceptance, check built-in mic mute/unmute, recording/release,
auto-stop, cancellation at every stage, quiet input, language detection,
search/back navigation in both interfaces, background playback, profile switch,
controller reconnect, shutdown while recognizing, and repeat searches. Record
capture success, recognition time and memory use before approving launch.
