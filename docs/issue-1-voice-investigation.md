# Issue #1 follow-up: local voice model unavailable

This is separate from the Cyrillic playback-title defect. The reporter's
follow-up supplies the message, but no console logs or installation inventory.

## Feedback after 1.4.1

On October 23 the reporter confirmed that Russian playback titles now display
correctly, but L3 still reports model unavailability. The retry and stdio probe
therefore did not resolve this console's voice failure. Its cause remains
unconfirmed; no microphone or transcription failure can be inferred from a
preflight rejection.

The next diagnostic change distinguishes invalid paths, failed opens, failed
seeks, failed size queries and wrong-size files. Open/seek/size-query errors
include the captured errno; size mismatches show actual and expected byte
counts. The voice UI forwards this result instead of hiding it behind a
generic unavailable message or an earlier timeout. It exposes no account data.
Host tests inject seek/size-query failures and cover missing/truncated files,
recovery and driver propagation. Full model SHA verification is unchanged.
This is diagnostic work, not a confirmed fix for the remaining console issue.

## Installation-path correction

Voice preflight and recognition still passed a single fixed path,
`/app0/assets/voice/ggml-tiny.bin`. The app's existing updater already supports
three installation roots: `/app0`, `/system_ex/app/<titleid>` and
`/data/homebrew/<titleid>`. A launcher exposing the model only under an install
root could therefore reject L3 even with a complete, readable model.

The local driver now probes those same fixed roots in that order. Both
title-specific paths are generated from the release's title ID. It retains
the selected file path for recording and passes that exact path to the
recognizer, whose full size/SHA verification and Whisper loading are
unchanged. It re-probes on each new L3 request; it does not search arbitrary
folders, download a model or perform model hashing on the UI thread.

The expanded driver regression reproduces the old failure with `/app0`
unavailable and each alternate location readable. The original driver fails
at `tube_voice_begin`; the corrected driver reaches capture/transcription,
passes the alternate path to the recognizer, and returns a UTF-8 query.
It also checks `/app0` priority, same-profile recovery, missing models,
removal before capture, profile changes, cancellation and zero network calls.
The production preflight's file/error fixtures and companion driver pass.

This establishes and corrects an installation-path defect; the reporter has
not supplied evidence identifying their console's mount layout. A successful
PS5 build and a test of the resulting app on their console are still needed
before calling their voice-search report resolved.

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
