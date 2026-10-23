# Issue #1 follow-up: local voice model unavailable

This is separate from the Cyrillic playback-title defect. The reporter's
follow-up supplies the message, but no console logs or installation inventory.

## Evidence and correction

The public v1.4.0 `PPSA99991.zip` contains
`PPSA99991/assets/voice/ggml-tiny.bin`, size 77,691,713 bytes, SHA-256
`be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21`.
These match the pinned model manifest. The executable contains the local
recognizer's verification message and `/app0/assets/voice/ggml-tiny.bin` path.
Thus the published archive is not simply missing its model. This does not
establish whether the reporter's installation contains the complete file.

`tube_voice_configure_local()` originally probed model availability once and
returned early on every subsequent call with the same profile.
`tube_voice_begin()` rejected L3 whenever that cached availability was not 1.
An initially inaccessible model therefore left voice unavailable throughout
that profile session, even after access recovered. A new test reproduced that
failure: availability changed from false to true within one profile, but the
old implementation still rejected L3.

The correction refreshes the inexpensive availability probe at each local L3
start, before opening the microphone. Recording/transcribing requests still
return immediately; there is no new per-frame file I/O. This also detects a
model removed since initial configuration.

`maxd_asr_available()` now opens the model and checks its length with
`fseek`/`ftell`, instead of requiring a successful `stat` plus mode check.
This aligns preflight with the stdio file access used by verification and
Whisper. The repository's elevation code documents launcher situations where
file access works but metadata/listing may be refused. A host test simulates
denied metadata with a readable model. That scenario is **not confirmed as
the reporter's console cause**; the stale availability latch is independently
reproduced.

Model path, expected size, complete worker SHA verification, inference,
microphone ownership and network policy are unchanged. A sparse file with the
right size can pass preflight, as before, but cannot pass worker verification.
Wrong-size, missing and unopenable models remain unavailable.

## Validation

The added local-driver recovery test fails against the original implementation
and passes with the correction. Host commands (LeakSanitizer is disabled
because this environment cannot inspect process tasks):

```sh
ASAN_OPTIONS=detect_leaks=0 bash scripts/test_local_voice_driver.sh
ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_voice_model_probe.py
ASAN_OPTIONS=detect_leaks=0 bash scripts/test_voice_driver.sh
```

These pass with address/undefined-behavior instrumentation on the driver and
production preflight code. They cover same-profile recovery, model removal,
unavailable files, empty/truncated/oversized models, cancellation, profile
retirement, physical ownership and the optional companion flow.

`scripts/test_local_voice.sh` also passed with the pinned Whisper libraries,
actual multilingual tiny model, and public JFK speech sample. The recognizer
returned `And so my fellow Americans ask not what you are coming` from the
six-second fixture. Silence rejection, same-size corrupt-model SHA rejection,
cancellation/retry and complete-result copying passed. The application wrapper
and tests used address/undefined-behavior instrumentation; the script builds
the third-party Whisper libraries in Release mode without sanitizers.

The standard test runner reported **3 passed, 0 failed** for
`test_local_voice_driver`, `test_voice_driver`, and `test_voice_model_probe`.
The real-model script passed separately. These are host results only.

## Console acceptance still required

No PS5 execution or new installable app has been verified here. Build and
verify the app package, install it with the complete model, and check L3 from
both Classic and Afterglow. Confirm `/app0/assets/voice/ggml-tiny.bin` can be
opened and seeked in the actual launcher environment, then test capture,
unmute, transcription, cancel, repeat search and profile changes. If the model
remains unavailable, collect the file-open/seek outcome and installed model
size; a different mount path or incomplete installation would need a separate
correction. A host retry test cannot establish microphone routing or Whisper
performance on the console.
