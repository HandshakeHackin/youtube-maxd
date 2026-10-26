# Issues #1 and #2: follow-up fixes and verification

This work uses the latest public reports, including feedback after 1.4.1 and
PR #5. It does not treat earlier host tests as console acceptance.

## Issue #1: Cyrillic and the new L3-release crash

The reporter confirms that Russian playback titles, status/log text and chapter
labels are now fixed in the PR #5 test build. Its font and model-path corrections
are retained here. The new report is a crash after speaking and releasing L3,
with SIGILL/page-fault information; it is not another model-availability failure.

The production model verifier allocated a 65,536-byte local buffer. GCC reports
its frame as 65,712 bytes. Running that exact verifier on a guarded 64 KiB pthread
stack with the actual pinned model crashes with SIGSEGV (-11), before inference.
The existing host recognition tests used a much larger default stack, so they
could not reveal this failure.

The verifier now uses a heap buffer while retaining full-file size/SHA checking.
The recognition worker requests a 4 MiB pthread stack explicitly, as the native
player opener already does for its own work. Failed stack setup/thread creation
returns an error instead of falling back to an unknown default. Cancellation,
completion and shutdown still join the worker before clearing its input.

`scripts/test_voice_worker_stack.py` runs the production verifier on the same
64 KiB guarded stack and rejects oversized frames at compile time. It passes
with the correction. The real-model test also checks the explicit worker stack,
injected stack-setup failure, actual speech recognition, silence, corrupt-model
rejection, cancellation/retry and complete UTF-8 results under ASan/UBSan.

This proves and corrects a stack-overflow defect consistent with the reported
write fault. It does **not** establish the console's default stack size or
symbolicate the reporter's exact test executable. A host SIGSEGV reproduction is
not proof that every console SIGILL has the same cause. L3 must still be tested
on the reporter's PS5 before declaring their remaining crash resolved.

## Issue #2: Home pages full of category cards

The reporter's screenshots show actual Classic Home page 2–4 entries named for
categories, with initials instead of video artwork. The 1.4.1 TV Home adapter
intentionally returned video previews only for the first group and registered
all later groups as `library:filter:` folders. Its outer continuations returned
no video previews at all. This reproduces the reported behavior.

The TV account adapter now returns playable video cards from all supported
recommendation shelves in source order. Category/channel/playlist navigation
cards do not replace videos in the Home grid. Other account/channel/playlist
browsers retain their existing navigation. Each shelf's independent horizontal
continuation and the outer Home continuation remain reachable lazily, with
bounded page/item counts and deduplication across the feed.

Structured ad rejection, Shorts/mixes/upcoming hide selections, the primary
watched preference and profile-owned continuation cursors remain enforced.
Shorts hiding is controlled by the existing Home hide setting; this does not
claim to detect unmarked ads or make YouTube's personalization identical to its
phone app.

The new regression follows the real Library OAuth route: 24 videos on page 1,
12 on page 2, all playable with artwork/context, no category placeholders,
duplicate videos, explicitly marked ads or Shorts when Home Shorts hiding is on.
It also checks wrong-profile cursors, horizontal and outer continuations,
filtered-empty pages, malformed feeds and item limits. The pre-fix TV adapter
fails these new regressions; the corrected implementation passes.

## Recorded host results

- Bundled account/library worker suite: **223 passed, 0 failed**.
- Exact account-feed CI command: **41 passed, 0 failed**, including the TV
  account shape/cursor fixtures updated for videos-only Home continuations.
- Embedded service: **792 plain-host checks passed** (21 engine, 186 HTTP,
  232 account, 51 import, 302 casting).
- Focused runner: **6 passed, 0 failed** (local/companion voice drivers,
  model preflight, Cyrillic font/UTF-8 paths, guarded verifier stack).
- Real pinned Whisper/tiny-model recognition: passed; the public JFK fixture
  produced “And so my fellow Americans ask not what you are coming”.
- Worker stack size and injected setup failure: passed in the real-model test.
- Complete model integrity, cancellation and ownership protections are retained.

LeakSanitizer is disabled because this sandbox cannot inspect process tasks;
address/undefined checks run on the application test code. Third-party Whisper
archives are built in Release mode. No microphone, private account or physical
PS5 was used for these checks.

## Console acceptance

Install the complete new test build, including all voice assets. Check:

1. Hold L3, speak and release repeatedly in both interfaces: no crash, Listening
   transitions to recognition, and a search/command is returned. Test cancellation,
   silence, profile switches and playback ownership as well.
2. Open the signed-in Home feed, move through pages 2–4 and an outer continuation:
   playable video cards and thumbnails must remain present. Check queue/playback,
   Home Shorts hiding and any remaining promoted content separately.
3. Confirm Russian video/status/chapter text remains readable.

Keep the issues open for this hardware acceptance. Build/static-package results
and the downloadable test artifact are recorded separately after compilation.
