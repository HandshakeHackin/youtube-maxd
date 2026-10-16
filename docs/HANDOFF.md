# Handoff notes

For the next person or agent who picks up ProsperoTube. Read this first, then
[Status](STATUS.md), the [README](../README.md) roadmap and
[Afterglow's architecture](afterglow/ARCHITECTURE.md). Written 2026-10-14.

## Owner repeated-cast replay (2026-10-16)

Owner reports the repeated-cast test worked perfectly on `047e3fc`. Read-only
log collection while the app remained open confirms that build. The same
session contains an earlier receiver `scope_changed`, immediately followed by
launch rejection `reason=3 after=3 selection=1 bound=0 remote=1` (stale owner),
then a later acknowledged setPlaylist. A subsequent `command_rejected` is also
recorded. Thus the owner replay passes, but the intermittent issue is not yet
considered fixed. Next investigation: identify which remote configuration field
changes during launch; do not remove the owner guard. Private evidence:
`owner-cast-switch-047e3fc/app.log`. No launch, close or Chiaki-ng was used.

## Settings follow-up and repeated casting (2026-10-15)

Removed Check for updates and Notify about updates from About in every UI mode.
The Classic updater fallback now checks the actual renderer, including override
files, instead of relying only on the saved interface preference. Automatic
startup update offers remain available. Removed the main-menu Notifications
entry and its background badge polling; direct Library routing is preserved.
About, updater and 24 UI boards pass (321 UI checks).

Owner confirms the prior thumbnail fix on both Home and Library > History.
Repeated casting remains under investigation: the live log reaches successful
media open and decoder startup, then the replacement player is interrupted with
`AVERROR_EXIT` before producing frames. No new refused-media probe is recorded
for those attempts. Private evidence: `owner-cast-switch-38d2840/app.log`.

No playback fix is claimed yet. The real native phone/receiver fixture now
covers a second setPlaylist while the old engine stops and the replacement
open is held across eight poll cycles; bind, acknowledgement and ownership all
pass under ASan/UBSan. New capped diagnostics record rejected launch reason,
bind result and receiver failure category/status without video IDs, URLs or
credentials. This will distinguish stale launch state from receiver rejection
on the next console replay. Native receiver tests check diagnostic bounds and
privacy grammar. Logs can be collected while the app stays open.

Candidate `047e3fc` builds successfully. ZIP CRC and all 185 packaged files match
the output directory; the candidate is frozen. Installed on .30 after an idle
check: all 185 files still match after 120 seconds, error records unchanged,
and final idle. No launch or Chiaki-ng; owner repeated-cast replay remains pending.
Private install evidence: `settings-cast-047e3fc-installed/result.json`. Package evidence: `settings-cast-047e3fc-package.json`
and `build-settings-cast-047e3fc.log`.
Eboot SHA-256: `ae97814639dcfd858fefe2e8be416948968d149c3c60816fda621a996320c10a`.
ZIP SHA-256: `150ff03586b08cb8d57e3c3e0270211e302589ad96afcc096031fcfd90157243`.

## Settings and thumbnail candidate installed (2026-10-15)

`38d2840` is installed on .30. Idle-first deployment, all 185 stored files after
120 seconds, unchanged error records, and final idle pass. Private evidence:
`settings-38d2840-installed/result.json`. No launch, close or Chiaki-ng was used.
Owner confirms thumbnails now appear on Home Continue watching and Library > History.
The remaining About controls and repeated-cast failure are addressed in the follow-up below.

## Simpler settings and local thumbnails (2026-10-15)

Removed the Updates tile and manual update rows, plus About's Releases/source,
Feedback and installed-version changes entries. About keeps version, author and
credits. Classic retains its existing updater fallback in About; modern Settings
uses the simplified layout. The updater follows ProsperoEden's startup offer: Update now, What's
new or Skip outside Settings; accepting shows cancellable progress and uses the
existing validated helper to apply and close when ready. It does not silently
install an update during playback. Development/blocked builds retain protections.

New playback has a fixed 2160p ceiling, including older global/profile settings.
The editable cap and remember-quality setting are removed, and video presets
retain only full-cap choices. Explicit per-video stream selection and existing
error recovery may still use lower resolutions. Actual source availability,
codec and frame-rate constraints still determine which stream is selected.

Local History and Continue watching share the native local-record catalog.
Phone-origin records could have no artwork. Missing thumbnails now fall back to
the standard YouTube image for the validated video ID; existing artwork and
DeArrow handling remain intact. Old blank saved records work without migration.
The regression fails without the fix and passes with ASan/UBSan, including restart.
Profile settings, player menu/extras, provider extras and simplified About tests
pass. Updater sanitizer tests cover acceptance, cancellation, blocked builds,
staged-on-start consent, apply failure and one-shot close. Settings home passes
164 UI checks and About/Credits passes 50. Candidate `38d2840` builds successfully
with the real Afterglow mount integration. ZIP CRC and all 185 file contents
match; packaged-helper checks pass and the candidate is frozen.
Private evidence: `settings-38d2840-package.json` and `build-settings-38d2840.log`.
Eboot SHA-256: `c2b432000b719108e2e95cb7e046f733a7b3e66074abc084d58efe3e0e80ddd8`.
ZIP SHA-256: `7dd077aa99bd595047c1abb809fcaa412e6b2e288cbb6a90ada03c2cf8960889`.
No console installation or visual hardware acceptance is claimed for these changes.

## Media preflight refusal diagnostics (2026-10-15)

The resolver now logs `YouTube media probe: phase=header|midpoint attempt=N status=S`
only when the existing media preflight receives HTTP 401, 403 or 410. Values
are fixed phase names and checked numbers; URLs, video/account identifiers,
response bodies and credentials are excluded. The existing retry loop bounds
output to three attempts per client. Selection and retry behavior are unchanged.
The embedded stderr allowlist persists this prefix in the app log.

The real service fixture passes 173 checks, including the three exhausted
header attempts and one recoverable midpoint refusal with exact diagnostic
content and privacy grammar. The actual embedded log callback test passes,
including persistence and rejection of unrelated/stdout/oversized output.
Candidate `47d007a` is built, ZIP CRC/content verified and frozen. Idle-first
deployment to .30 passes: all 185 stored files match after 120 seconds, error
records unchanged and final idle. No launch, close or Chiaki-ng was used.
Private evidence: `media-probe-47d007a-package.json` and
`media-probe-47d007a-installed/result.json`.
Eboot SHA-256: `61f15fc5dddb2091cc930a84889206bd1b7d53445d366217a74c5a01480d02d7`.
ZIP SHA-256: `bb63b3e98ac01eab16798f248721e4eea8ae5385ee8e0f233acb78e0335a5f4b`.
The source of the real media refusal and earlier DASH startup-seek failure
remains unknown. Next: owner cast from 0:00, collect the new probe lines live.

## Diagnostic replay blocked before DASH (2026-10-15)

Read-only collection while ProsperoTube remained running confirms installed
`08ce71c`. The phone connection and setPlaylist were delivered, but native
resolution failed with "The media server could not provide this resource."
No decoder or DASH probe/seek record occurred in this run. Private evidence:
`owner-dash-seek-08ce71c/app.log`. The resolver produces this error after media
probes receive 401/403/410 and its bounded retries are exhausted; the current
log does not reveal which status or whether the header/deep-range probe failed.
This attempt does not test the previous startup-seek failure. The owner subsequently reports the same error on a different video and on
a video started from the beginning. This media-access failure is therefore
not confined to the original mid-video seek. No close, deployment or
Chiaki-ng was used for collection.

## DASH diagnostic candidate installed (2026-10-15)

Diagnostic `08ce71c` is installed on .30. Idle-first check, all 185 stored files
after 120 seconds, unchanged error records, and final idle all pass. Private
evidence: `dash-seek-08ce71c-installed/result.json`. No launch, close or Chiaki-ng
was used. Owner mid-video cast replay and diagnostic capture remain pending.

## Bounded DASH seek diagnostics (2026-10-15)

The next candidate adds numeric-only `ProsperoTube DASH` records to the pinned
FFmpeg patch. `phase=0` is a failed fragment probe; `phase=1` is the seek result.
Records include return code, stream, target milliseconds, selected sequence and
fragment count, range offset/size, bytes read, initialization read/length,
underlying and virtual IO errors, EOF and whether the return is AVERROR_EXIT.
The exit field is derived from the return code; no interrupt callback is invoked
for logging. Limits are four failed probes and eight seek results per
representation. Signed media URLs, identifiers and tokens are never included.

The actual extracted helper passes ASan/UBSan null-state, maximum numeric length,
privacy, severity and limit checks. The pinned patched host FFmpeg 7.1.1 passes
real loopback service-generated ranged AVC/AAC seek and forced-fragment-refusal
checks; both emit the expected records. Patch idempotence passes. Playback
policy is unchanged. Production-format diagnostic ZIP `08ce71c` is built and
frozen. ZIP CRC and all 185 file contents match; the linked ELF contains the
new numeric diagnostic strings, and packaged-helper validation passes.
Private evidence: `dash-seek-08ce71c-package.json` and `build-dash-seek-08ce71c.log`.
Eboot SHA-256: `17f48e6035544b8da09edbfeff408b5481694027dbae6ac631fca6cdbdd67f66`.
ZIP SHA-256: `0ef5b72fafa9e0d62b8edc32db86c33a010994ed12ef061521e46033afe10da5`.
No console deployment, launch or Chiaki-ng was used. These diagnostics do not
fix the seek failure. Next case: repeat the same mid-video cast, collect
`ProsperoTube DASH` records live without closing the app, then compare a cast
from the beginning if needed.

## Live phone replay after refresh repair (2026-10-15)

A requested redeploy of `00f4345` passed idle-first installation, all 185 stored
files after 120 seconds, unchanged error records, and final idle. Live read-only
log capture then confirmed this exact executable; the app remained open.
Private evidence: `live-casting-00f4345/result.json` and `live-app.log`.

The owner saw the received-video notification but no playback. The cast now
resolves and opens AVC 1080p59.94 plus audio, then performs the requested
6999.16-second startup seek in a 11510-second video. The owner confirms casting
from the middle. DASH first-fragment probing fails after that seek, no pictures
are decoded, and teardown clears the connection. This is beyond the repaired
status-refresh cancellation; the log does not distinguish transport failure
from DASH seek-state failure. A start-from-beginning comparison remains pending.
Home and notification-count errors also persist independently. No Chiaki-ng,
forced close, or credential replay was used.
The capture ended after a read-only check found another title running; that title
was left untouched. A service-generated AVC/AAC ranged-DASH fixture passes
start/middle/near-end seek with both host FFmpeg 8.0.1 and rebuilt, patched
7.1.1. This does not reproduce the owner failure. A future diagnostic needs
URL-free reopen error/representation/range/initialization state before changing
the player. No speculative playback patch was made.


## Cast refresh repair installed (2026-10-15)

Production `00f4345` is installed on .30. ZIP CRC/content and helper checks pass.
Install began idle; all 185 stored files match after 120 seconds and error
records are unchanged. The final-idle assertion briefly failed; a subsequent
read-only title check found idle. Classify installation partial-pass because
that assertion failed; no launch or close was sent and real-phone replay is
pending. Private evidence: `phone-pairing-00f4345-installed-production/post-install-state.json`.
Eboot SHA-256: `483fa244947e120a3854b360681d93b860bf27669fddcc47a22bdd1d36ad52d6`.
ZIP SHA-256: `276c69e4b0f1f60303ba34ed65a8606cda1d924628e68ae89adf5ff88614f1c4`.

## Owner feed acceptance and cast-resolution failure (2026-10-15)

Owner replay of `f8617d2` confirms Library loads and the pairing heading overlap
is fixed. History remains empty: the main History tab reads device-local history;
Triangle opens signed-in YouTube history. The exact account-history outcome and
personalized Home are not confirmed.

After owner Close, read-only collection found the console idle. Private evidence:
`owner-phone-f8617d2/app.log`. Fixed milestones show remoteConnected received,
delivered and acknowledged. The final setPlaylist was received and delivered,
then native provider resolution failed before acknowledgment. The existing
failed-launch path invalidates the receiver and clears connected-device state,
so the missing linked-phone display can follow the resolution failure. Do not
claim a new upstream pairing or metadata defect from this replay. No console
launch, installation, account request or Chiaki-ng was used for collection.

The real Afterglow/provider regression reproduces a DELETE of the pending cast
job during the page's two-second status refresh. The exact read-only
`settings:remote` catalog now returns before cancellation and browsing-context
mutations. Pairing actions retain their existing cancellation behavior. The
regression holds a real resolve across repeated refreshes, completes playback
resolution, checks launch acknowledgment/linked state, and checks explicit
connection changes still cancel a pending request. The regression fails before
the fix and passes afterward. Full Afterglow data/provider/picture tests and
native phone provider, player and connection suites pass under ASan/UBSan.


## TV account feeds and overlapping headers (2026-10-15)

Production `f8617d2` is installed on .30. ZIP CRC/content and packaged-helper
checks passed. The console was idle before upload; all 185 stored files
matched after 120 seconds and error records were unchanged. Another title
started during the wait, so the final-idle assertion failed: classify this
installation as partial-pass, not a completed idle handoff. No launch/close
or Chiaki-ng was used; the running title was left untouched. Functional
account-feed and casting acceptance remains pending.
Eboot SHA-256: `713d4df78251ce4e389d6ff4f928926ca377233f8495afa6598c6f5da5fda450`.
ZIP SHA-256: `46a4c3557baa58588998fd61030c0bfd7d834de20ee3214b8d894618e17c2ae0`.

Owner `a9ba0c1` replay confirms the real account name and profile switching,
but Library stays empty, Home recommendations do not appear personalized,
and casting still fails at an unspecified stage. Screenshot confirms two
headings overlap on Phone & casting. After owner Close, read-only log
collection left another running title untouched; no launch or writes occurred.
The durable service diagnostics show repeated notification_count InnertubeError,
not a recorded sign-in or Library exception.

The LinkPhone screen draws its own heading while the shared frame also draws
the route title. Its header policy now suppresses the duplicate; the same
pattern is fixed for SignIn. A titled-route host render reproduces the old
clash and confirms the repaired layout. The account-feed investigation
reproduces zero Home items from a public SmartTube TV response containing
Tile renderers, which the pinned web-oriented Feed getter does not enumerate.
Phone state diagnostics are fixed milestones and bounded event names only;
no device/video identifiers, phone names, codes or tokens are logged.

OAuth account feeds now use the existing bounded TV-card mapper and TV Home
browse ID, retaining profile-owned pagination and legacy web-shaped handling.
Validation: 232 real QuickJS account checks, seven authenticated TV/legacy
shape and ownership regressions, 302 receiver checks in plain and sanitizer
builds, 263 LinkPhone board checks and the actual embedded log-sink check pass.
Public SmartTube Home, history and playlist fixtures map to nonempty pages.
An older undated TV-history wrapper remains unsupported by the legacy parser;
the owner's exact live response still needs replay. Notification-count errors
are separate and remain unresolved. Casting has instrumentation, not a newly
proven protocol fix in this revision.

## Phone connection and account-list follow-up (2026-10-15)

Production `a9ba0c1` is installed on .30 for owner testing. The console was
idle before install; all 185 stored files matched after 120 seconds, error
records unchanged and still idle. No launch or Chiaki-ng was used. ZIP CRC,
archive bytes and packaged helper checks passed. Source and milestone pushed.
Eboot SHA-256: `493206c276904316238e90eff8bcc0acd9087062d498f83adbcc57cffc31459f`.
ZIP SHA-256: `686e23dad3f49fc6c5400306c7ea8c817ae9581e743b540d6488a6e53d4db168`.

The owner confirms `139f8c1` now displays a TV code and the phone lists the TV.
The console still reports no linked phone; casting was not confirmed. Account
approval shows the generic label, but returning to Switch profile omits the
account and Library remains empty. After owner Close, read-only inspection
found two saved OAuth profiles with credentials and neither selected. No
credential values were printed or saved in the diagnostic summary.

Implemented fixes: refresh the Accounts data revision after
approval so Back cannot restore a stale profile list; refresh the Lounge token
and rebind its channel after code registration, matching cached SmartTube
source while preserving native command sequence/ack. A best-effort TV account
name lookup preserves credentials when unavailable. Existing generic labels
are retained; no account is deleted or requires another sign-in to be listed.

Production JS diagnostic messages previously went to stderr rather than the
app log. The embedded callback now persists only bounded operation/category
lines. Real Library access and phone casting remain unaccepted; protocol
parity and offline fixtures are not substitutes for owner replay.

Validation passes: real embedded-service startup with two synthetic saved
accounts, native approval and Afterglow provider/picture integration under
ASan/UBSan; 301 real QuickJS receiver checks in both plain and sanitizer
builds; 76 Python remote/playlist checks; 26 Python account checks; worker
name lookup/fallback checks; actual embedded diagnostic callback and privacy
checks. No console launch or live-account requests were used for these tests.

## Owner acceptance failures and offline repair (2026-10-15)

The owner reports no TV code on entering Phone & casting, a Signed out account
label after QR approval with missing Library content, and an app-only crash
while moving playback forward. Read-only collection found .30 idle. The
installed build was `4d2fcbc`; its new CE-108255-1 record and app log are
preserved privately. No relaunch or Chiaki-ng was used.

Opening LinkPhone required another confirm press before requesting a code;
it now requests once on entry and reuses an existing code. Accounts consumed
profile facts that the provider did not emit; the provider now supplies them.
The saved state confirms one OAuth account with access/refresh credentials,
and that account is selected. Thus approval/storage succeeded; the wrong label
is confirmed, but the missing account Library content is not yet explained.

The crash address resolves against the exact installed ELF to FFmpeg HLS
read_data/read_from_url dereferencing a null current segment. A retained
input could bypass the segment-opening checks. The reader now returns EOF
when that segment is absent; an extracted real-callback sanitizer regression
reproduces the old fault and verifies the guard and normal/ranged reads.
The transition that produced this state is not established; do not attribute
it to seeking beyond duration or to VP9/GPU ownership. The combined provider/picture suite, HLS callback regression and overlay
checks pass ASan/UBSan. The actual sign-in handler and bounded diagnostic
privacy checks also pass. Safe operation/error-category logging is added for
the unresolved Library failure. Production `139f8c1` built successfully; ZIP
CRC and all 185 archived file bytes match. The initial installation stopped
because another title was running. After the owner closed it, a fresh idle
check passed and `139f8c1` was installed: all 185 stored files matched after
120 seconds, error records unchanged, console still idle. No launch or
Chiaki-ng was used. The owner will test manually.
Production eboot SHA-256:
`66a89a6cca013060165b5fe1bc3d12e231c5a098e144d4e8feb9e9f95b284905`.
ZIP SHA-256:
`057f9d7fdd80aa2dba3c7d9037b540658ef30c6da8f3a476016b59a5fcf8f40a`.
Installation verification passed; functional hardware validation is pending.

## Phone-pairing fixes installed for manual testing (2026-10-15)

Fixed the provider/Afterglow state contract, enabled-state refresh, linked-phone
feedback, and bounded transient reconnect without old command replay. Added
real provider-to-LinkPhone integration coverage through cast admission while
the pairing page stays mounted. Account sign-in now preserves credential-save
errors and supplies actual state/expiry facts. See [receiver details](REMOTE_SERVICE.md).

Production `4d2fcbc` builds and passes signed-container/helper/ZIP checks;
all 185 archived files match. Source and validation are pushed to GitHub.

At the owner's subsequent request, installed production `4d2fcbc` on .30 after
checking that the console was idle. All 185 files matched after the 120-second
stored-byte verification; the console remained idle and error records unchanged.
No launch or Chiaki-ng was used. This includes the `c1b67a5` guards below,
superseding their earlier uninstalled status. Real-phone acceptance is pending.
Production eboot SHA-256:
`8f7387103428658d5b6a86bd5f91fcfd64ac6607a366526495b8019ff3384943`.
Next: owner launches Link a phone, links using the TV code in YouTube's phone
app, and casts a video. Google account sign-in is a separate flow.

## Owner replay: no noticeable hitching via RDP (2026-10-15)

After the installed `6fc6d58` VP9 identity fix, the owner reports no noticeable
hitching during the manual replay. They watched via RDP, not directly on the
TV. Audio seemed in sync, but they explicitly note that remote-viewing delay
makes sync difficult to judge. Record this as a positive subjective remote
replay, not direct-TV motion or precise A/V-sync acceptance. Playback duration
and app closure were not confirmed. The prepared sustained/reuse tests remain
unrun; the follow-up `c1b67a5` guards remain uninstalled.

## Release failure-path audit (2026-10-15)

A subsequent current-code audit found three engineering gaps beyond the
confirmed VP9 identity fix: seek renewal ignored failed decoder deletion;
automatic refresh cached a refused switch as applied; and HDR disable could
retry a refused output change every frame.

The renewal patch retains uncertain handles and mappings and marks the slot
unavailable on Delete/Create/Reset errors or watchdog timeouts. Close also
retains that slot if the failure happened before the first decoded AU. The
mode policies read actual runtime output state and bound each requested mode
to three attempts, with one- and two-second retry delays. Existing playback
menu mode retention and HDR-before-refresh ordering remain intact.

Generated-code frame-lease, renewal, close and VP9 identity tests pass
ASan/UBSan; output-mode refusal/recovery tests also pass. The VP9 regression
now uses the actual 16-entry pending capacity. The full 36-second console
trace peaked at five pending entries; this is a measured sample bound, not
a universal stream guarantee. DVR restore and HDR format-selection review
found no additional concrete defect.

Both production and diagnostic builds of `c1b67a5` pass signed-container,
helper and ZIP checks; all 185 archived files match their frozen candidates.
They have not been tested on the console. The
installed production candidate remains `6fc6d58` for the owner's TV replay;
do not describe the failure-path guards as installed or hardware-verified.
A private repeat-open/EOF/control case is prepared but not run. Its eight
36-second replays cover reuse and control transitions, not uninterrupted
five-minute streaming or audible/physical acceptance. A separate uninterrupted
300-second public-network Afterglow case is prepared, also not run. Both
private case host suites pass (nine tests), including size/read/delete
exceptions during trace cleanup. Both drivers attempt saved-state restoration
even when private evidence cleanup throws; such a run still fails. No console
operations were made.

Follow-up production ZIP SHA-256:
`af78031002fa5ca1bad2117a2f6136d12a3349ce10f1a16c6c9d21c321d8d6bc`.
Production eboot SHA-256:
`198778f9bac5da6a0d176ffc5670a7f02ac2ff53ab68ec2b450d659969de4be9`.
Diagnostic ZIP SHA-256:
`41d3f5d78ad9d603acdfe306fb17182f7b3cda3bd35f6d91c2239ae9c786ea09`.
These are build-validation results, not hardware acceptance.

## Identity fix passes full-sample console replay (2026-10-15)

Committed and pushed `6fc6d58` replaces coded-AU FIFO association with exact
returned input tags from public `GetPictureInfo`. The same-input native 4K24
Afterglow case reached natural EOF and published all 864 pictures. All 864
packet identities and all 864 sparse luma fingerprints match the software
reference at their original timestamps. No missing, shifted or backward
pictures remain. All 929 coded-AU completion records are present, including
65 hidden completions. The public API executes successfully on hardware.
After five seconds, the trace has 733 five-refresh intervals and four
four-refresh corrections, with no six-or-longer gap. All 1,475 adjacent
source/render hash-change comparisons agree. Startup has brief disturbances
within roughly 1.5 seconds. Coarse status snapshots undercounted one polling
window; actual new-picture progression was 35.744 seconds wall versus 35.750
seconds of media, so this is not sustained playback slowdown.

The case restored saved state, deleted its private sample, closed cleanly and
left error records unchanged. Production `6fc6d58` is installed: all 185 files matched stored bytes after
120 seconds, with the console idle and error records unchanged. No launch was
sent after installation. It has no timing probes or development controls.
Production eboot SHA-256:
`2b478f478cf71c5b8171336827f66c5650a5e338a4cba71b90f566de15e58b20`.
ZIP SHA-256:
`22ab2639785456d7670d965e3e7afbee50bc2e399c113b20f1893650b36d1792`.
ZIP CRC, permission checks and all 185 archived files match the frozen folder.
Next owner replay: same clip directly on the TV, then separately via Chiaki;
report picture smoothness and audio sync.
Physical TV motion/A/V sync and longer-run reliability are not established by
this 36-second, video-only, sampled comparison. The earlier GPU fault ownership
limits below still apply. See [validation](VALIDATION.md) for host coverage.

## Returned tags verify completion identity (2026-10-15)

Diagnostic `3892445` passed a bounded identical-input Afterglow replay and
clean teardown. All 813 accepted compressed packets match the software
reference. All 871 returned tags identify the correct submitted AU and its
presentation flag, with no timestamp or tag discrepancies. The 811 shown
completions follow the exact software display order. Shown completions arrive
3-4 decode calls after submission; hidden completions arrive 15-16 calls later.
The coded-AU FIFO therefore discarded 61 shown completions and published 60
hidden completions containing stale pixels. Hidden completion omission/delay
must not consume the visible-picture queue.

The production fix uses public `GetPictureInfo` and unique process-wide input
tags, preserving original timestamps even when missing or equal. Exact pending
identity lookup retains delayed hidden entries while enforcing visible order.
Generated-code ASan/UBSan regressions pass the observed delayed-hidden sequence,
EOF, seek/object reuse, missing/equal timestamps, invalid metadata, queue bounds
and unchanged backpressure/frame leases. Production and diagnostic preparation
pass; console build and full-sample validation remain next. The diagnostic restored saved state, removed
its private sample, closed cleanly and left error records unchanged. Production
build `f801bae` was restored: all 185 files matched after 120 seconds,
the console stayed idle, and error records were unchanged.

## Output metadata isolates the VP9 association error (2026-10-15)

Diagnostic `77b77ee` passed a bounded local-input Afterglow replay and clean
teardown. All 814 accepted packets match the reference. All 872 returned
metadata records passed slot/trailer/cache bounds, held a lease, and reported
type 5 / metadata-valid 1. The raw field at trailer +0x10 tracks input PTS:
750 published pictures with a real value match the software picture at that
value exactly. Of those, 642 differ from the app FIFO's assigned timestamp.
All 61 stale published pictures instead carry the AV_NOPTS_VALUE bit pattern.
Conversely, 61 completions discarded by the FIFO as hidden carry real picture
timestamps. Excluding the sentinel leaves 811 metadata timestamps in exactly
the software reference's display order, without sorting or deduplication.

This identifies incorrect association/filtering in the app's coded-AU FIFO.
A count gate or fixed timestamp shift cannot fix it. Do not filter solely by
NOPTS in production: visible inputs can have missing or duplicate timestamps.
The next bounded diagnostic verifies unique input `attached` tags in trailer
+0x08, so a production change can classify hidden completions independently and
associate shown/show-existing outputs with their own queued metadata by tag.

IDA confirms the same trailer layout in the reopened library. Decode may
return success without output retrieval after pipeline fill; Core supplies
completion counts. Thus successful coded-AU count is not a completion identity.
No further library is currently needed for the tag probe. The observed +0x08
values remain zero, matching existing input attached=0; +0x18 is consistently
UINT64_MAX, matching the input DTS sentinel. Nonzero tag propagation is not yet
verified.

Saved state and the production `f801bae` build were restored after the run;
all 185 installed files matched after 120 seconds, the console stayed idle and
error records were unchanged. No private sample remains. The bug is isolated,
not fixed or visually accepted.

## Historical GPU-fault ownership review (2026-10-15)

A targeted extract from the original teardown, drain and no-reset runs confirms
two distinct families. Faults at `0x247cf4000` and `0x248002000` belong to the
same run's retired 1080p input and frame allocations respectively; both follow
retirement and replacement creation. The drain case recorded four drained
outputs before retirement. Resident decoding removes that resize/retirement
path, but those records do not establish which outstanding GPU command kept
the old address alive.

The later no-reset fault at `0x2a5256000` is outside every decoder allocation
logged in that run, with no retirement and before a valid 4K output. There is
no address-level bound-descriptor/origin ledger in its saved logs. A generic
direct-memory slab from another run cannot establish ownership here. The
strict GPU-pool guard closes a plausible allocation hazard; it is not a proven
causal explanation for this fault. The current stale-picture defect concerns
contents in mapped caller buffers and must not be treated as attribution of
these unmapped reads without additional completion/lifetime evidence.

## Identical-input VP9 defect confirmed (2026-10-15)

A complete 36-second, video-only local WebM preserves all 864 compressed
packets, PTS/DTS and software luma hashes from the downloaded reference. This
removes the failed YouTube resolve from the diagnostic, not the production
playback path. Diagnostic `dee3753` passed a bounded 32-second native 2160p24
Afterglow replay, captured all three traces, removed its private media file,
restored saved state and closed with unchanged error records.

All 813 accepted console packets exactly match the reference prefix by
SHA-256, PTS and size. Of 810 sampled output pictures, 115 match their assigned
PTS, 635 match the next software picture, and 59 uniquely match seven software
pictures earlier. Each of those 59 stale samples equals the prior contents of
the same source pointer/slot eight published pictures earlier. One additional
backward mapping is ambiguous repeated content. There are no geometry, missing
PTS or invalid-sample comparisons. This confirms an app-side decode/output
association defect independent of Remote Play capture.

Every recorded output nevertheless reports valid=1 and picture_count=1.
Associated coded-AU metadata agrees with submitted PTS, but the triggering
submission is four calls later for 808 rows and three for the first two. A
count-only filter cannot fix this; do not hard-code a one-frame timestamp shift
or discard repeated hashes. Investigate true completion identity and hidden-AU
handling against the local decoder research before changing the FIFO.
The local test has no audio and does not establish TV or A/V acceptance.

Production `f801bae` was restored afterward; all 185 files matched again after
120 seconds, the console remained idle, and error records were unchanged.
No diagnostic build or private sample remains installed.

The repository comparator can reproduce the compressed-input check with the
paired `--reference-packets REFERENCE.csv --native-packets NATIVE.csv` options.
It verifies recorded ordered prefixes, not unreported tails or whole-stream
completeness; invalid hashes, sequence gaps, capacity saturation and geometry
mismatches cannot establish input/content equivalence. Self-tests and the
813-packet / 695-picture-mismatch result pass.

Bounded offline disassembly and independent review established the VP9
metadata trailer read by `sceVideodec2GetPictureInfo` in firmware 12.70. The
next diagnostic reads that trailer directly, avoiding a new API import or
resolver. Its location is `output.buffer + output.buffer_size - 0xf8`; the
reviewed parser reads a type at +0 and validity at +0x20, then copies raw
fields including +0x08/+0x10/+0x18. The diagnostic validates the entire
cache-line envelope inside the leased caller slot before reading. These raw
fields remain unnamed until compared with actual input.

The probe records every validated VP9 output before hidden-frame filtering,
with fixed capacity and close-only file output. The initial probe left input
tags zero; its follow-up encodes a unique submission ordinal and display bit
in diagnostic-only attached tags, without changing PTS/DTS, the production
FIFO or frame fences. IDA remains useful for producer
semantics but is no longer required to prepare this bounded diagnostic. The
firmware-specific offsets are investigative evidence, not a portable decoder
contract. No new ABI call is introduced.


## Software-reference investigation (2026-10-15)

A bounded software decode of the same resolved VP9 Profile 0 / 2160p24
rendition provides packet SHA-256 and sparse luma fingerprints without raw
frame dumps. Host golden tests validate packet bytes, sampled pixels, equal-PTS
occurrences and duration bounds. The occurrence-preserving comparator finds
115 matching same-PTS hashes in the 842-picture historical trace, 664 matching
the following software picture, and 63 matching earlier pictures. Most backward
jumps occur 11?12 pictures apart, and repeated caller-buffer contents are a
specific lead. The reference has no show-existing frames; its hidden+shown
superframes align with missing software pictures in the native trace. This is
not yet decoder attribution: historical traces lack
compressed-byte identity. The next diagnostic adds accepted pre-split packet
SHA-256 and output metadata, without changing production decode behavior.

Reproduce the reference with `scripts/test_vp9_reference.py` and
`build/tests/vp9_reference INPUT FRAMES.csv PACKETS.csv 36`; compare with
`tools/diagnostics/compare_vp9_reference.py REFERENCE.csv NATIVE.csv`.
Media, signed URLs and raw evidence remain private. The packet trace shares
the existing diagnostic build flag and writes only at decoder close.

Diagnostic `dee3753` built, packaged and passed the 120-second installed-byte
check. Its bounded Afterglow replay stopped at the provider's media-resource
resolve failure, before decoder creation; no packet/output metadata was
collected. Teardown restored saved files, left the console idle and found no
new error records. Do not count this as a decoder test or retry identical
bytes automatically. Production `f801bae` was restored and all 185 files
matched again after 120 seconds; no launch followed, and errors were unchanged.

The hidden-frame hypothesis is narrower than a missing picture-count guard:
`tube_native_surface_valid` already requires one picture for VP9. The next
input-verified trace must explain why stale caller-slot content passes that
validation before replacing the coded-AU FIFO with visible-picture metadata.
No production decoder behavior changed in this milestone.


## Completed-buffer trace result (2026-10-15)

Diagnostic `b9d3583` passed one bounded 32-second same-clip Afterglow replay.
All 1,558 pre-blit samples matched decoder samples, and all completed-buffer
samples were valid. After five seconds, 1,324 presentations contained 656
adjacent source/output hash changes and 667 unchanged pairs, with no discordant
pair. All 660 same-serial repeated pairs switched buffers and retained the same
output hash. No older output hash recurred with a different sampled source hash.
This argues against steady stale rendered-buffer content in this instrumented
run; it does not prove complete frame correctness, HDMI output or Chiaki capture.

The 664 steady new pictures had 660 five-refresh and three four-refresh gaps,
with no six-refresh gap. One pacing miss occurred at startup (50 ms), not in
the steady window. Completed-buffer sampling cost 38 microseconds median,
44 p95 and 110 maximum. The existing seven repeated decoded/output sample pairs
at clip PTS 15.625?15.917 seconds recur, consistent with the earlier black-sample
cluster; they do not establish a decoder fault. Startup output differences on
repeated source pictures were confined to the first 1.38 seconds, when overlays
were active.

Saved files were restored; the title closed with healthy services and unchanged
error records. The next physical comparison remains the same clip on the TV
with Chiaki disconnected, then the remote stream separately. Do not change
output mode or declare the hitch fixed from this trace. Source and test changes
also correct an incremental-build header-reset omission found while packaging.

After collecting the trace, production `f801bae` was restored: all 185 files
matched again after 120 seconds, the console remained idle and errors were
unchanged. No diagnostic build is left installed. Physical keyboard and
post-playback glyph acceptance remain pending.

Next offline content check: compare native luma samples with software decoding
of the same VP9 rendition. The current run records clip, codec/profile, size
and rate but not the rendition itag or compressed-byte identity; the service's
resolved manifests are memory-only. A fresh resolve and bounded media prefix
can provide a reference, but a mismatch would remain inconclusive without
matching input bytes. Capture bounded input identity on any new comparison
run before attributing a difference to the native decoder.

## Latest owner observation (2026-10-15)

After installation of `f801bae`, the owner replayed the 4K VP9 clip and saw
hitching in the Chiaki-ng stream. This observation is not direct TV acceptance
or a new direct TV failure; previous TV reports remain unresolved. Chiaki-ng
keyboard input did not type in Search, but a physical PS5 keyboard was not
tested. Missing-letter recovery after playback was not confirmed either.
The owner cannot perform physical checks now. No console-idle confirmation
was given after this replay, so do not assume it is safe to replace files.

The next offline diagnostic extends sampled decoded/render-source identity
to the completed tiled backbuffer before submission. It must remain opt-in,
bounded, and preserve existing GPU fence/frame-lease ordering. A scanout
fingerprint cannot itself prove HDMI or Remote Play capture smoothness.

## Keyboard and missing text follow-up (2026-10-15)

The owner accidentally opened a different video during the `6599c48` visual
comparison, then reported missing letters on Afterglow Search. The screenshot
shows absent glyphs in the search field, keyboard and video titles while
key outlines, numbers, icons and thumbnails remain visible. This is a shared
font-upload investigation; the visual result for the same-clip cache-read
diagnostic remains unknown. Keep Afterglow enabled.

The requested keyboard forwarding means a physical keyboard connected to
PS5. Local native-keyboard research provides a device-tested 96-byte report
ABI; no IDA library needs reopening for this implementation. Host mapping and
UI routing tests cannot replace a physical-keyboard check on the console.

The fix retains pending atlas uploads across frame changes, retries allocation
failures and replaces texture storage without changing the handle already used
by glyph quads. Old GPU allocations retain the existing fenced retirement.
Browser toast/dialog parts now upload their newly rasterized glyphs before
drawing. The targeted ASan/UBSan regression uses the real text engine and
extracted AGC texture methods to cover late uploads, slot reuse, allocation
failure recovery and deferred frees. It passes. The real Search/TextSheet host
regression also passes typing, live-result debounce, submit, input limits,
UTF-8 backspace and hidden-page isolation. Console visual acceptance remains
pending; this change does not claim to fix the 4K hitch.

Candidate `f801bae` is the production keyboard/font build, with no timing
diagnostics or development controls. PS5 compilation, signed-container integrity,
packaged helper verification, ZIP CRC/permissions and comparison of all 185
frozen files passed. Executable SHA-256:
`efdd76df16f0530a2838546913cba6934f63ee85395503f0d62f4dded2368ec9`.
ZIP SHA-256:
`cdc3519d9505b34d5f40f222c37d11880ffb3dffad67b88bf6b6afa127fdb988`.

Installed on the authorized test console: all 185 files matched after 120
seconds, error records were unchanged and the title was idle. No launch was
sent. Next owner check: physical typing/edit/submit in Search, then return to
Search after playback and confirm keyboard labels and video titles remain
complete. US layout is supported; international composition remains through
the existing native IME.

## Final offline release audit (2026-10-15)

Candidate `6599c48` completed a bounded same-clip replay with Afterglow
retained. All 1,576 sampled render inputs matched their corresponding decoded
output samples (848 decoded records, no unmatched/mismatched comparisons).
Sampling cost was about 53 microseconds median at each hook. Seven adjacent
equal decoded hashes form one 0.3-second cluster, which is not itself proof
of a decoder error without same-bitstream reference content. Steady timing
had 672 five-refresh and three expected four-refresh intervals, no six-refresh
intervals and no pacing misses. Saved state was restored; .30 was idle and
healthy with unchanged error records. The diagnostic remains installed.
The owner was asked to check whether its cache reads change the visible
symptom; this run does not establish visual acceptance or full pixel identity.

The owner's `30aba46` Afterglow replay still visibly hitches. The complete
1,484-row trace has no omissions, failed presents, source/buffer PTS mismatch
or backward PTS transition. Smoothing is off and the scheduler is active at
120 Hz. Six steady-state six-refresh intervals cluster at 24.75?27.74 seconds,
with GPU completion about 0.65?1.09 ms late as UI cost rises. From 5 to 24.75
seconds, presentation metadata follows expected four/five-refresh cadence.
This does not explain continuous visible hitching; decoded pixel identity is
still unmeasured. Native playback reported 677 published pictures, no late
loss or decoder fatal. The console was idle and error records unchanged.
Next investigation compares sampled decoded luma with the leased source at
presentation; metadata cadence is not treated as visual acceptance.

The opt-in all-present Afterglow trace is built and frozen from `30aba46`.
Generated regression and independent review pass; it records repeated/UI
presentations plus pacing and submitted-buffer PTS without playback-time file
I/O. The first .30 attempt stopped at the running-title check: ProsperoTube was
already running, so no install or launch was sent. After the owner closed it,
the candidate installed and all 185 files matched after 120 seconds. The
Afterglow driver launched and selected the exact clip, but media resolution
failed before playback with a media-server resource error. Teardown restored
saved files, left the console idle and healthy, and found no new error record.
The diagnostic build remains installed; a manual same-clip replay was requested
to collect the all-present trace. This is diagnostic work, not a claimed fix.

The owner replayed the installed `5a65588` production candidate and reports
that the same 4K VP9 video still hitches. **Visual acceptance failed.** The
regular retirement intervals measured on the diagnostic build do not establish
regular visible content or explain this failure. The owner confirms smooth audio. An idle-only log read confirms
the production build, Afterglow interface, native 3840x2160 VP9/24, 608 published frames over
26.2 seconds, zero late drops and no decoder fatal. Earlier automated cadence
cases forced Classic and restored the original settings, so their workload
does not match this replay. The owner requires fixing forward with Afterglow;
no Classic comparison or interface change is requested. The next trace must retain Afterglow and record
all presentations, including repeated pictures omitted by the previous trace.
Reopen decode-to-display investigation;
do not release this candidate as a fix for the visible hitch.

Background EOF return now resumes only its automatic pause; explicit controls
cancel that state, including worker-side stop through an atomic flag. Generated
regressions and sanitizer/thread checks pass. The release-wide host audit also
repaired stale fixtures, restored pinned dependencies and refreshed localization.
See [Validation](VALIDATION.md) for exact counts and limits. Production candidate `5a65588` builds and packages successfully. Its ZIP
passes CRC, all 200 archive permission checks and comparison against all 185
frozen files. Signed-container integrity and the packaged helper pass.
Executable SHA-256:
`596def85881b976e793484ecd70fec1dfe087c20ecc868ae881e5d0a3e2b11f8`.
ZIP SHA-256:
`4de3afaaffa31a6edf1f574eb8e31ad619d259d43ba4c9b19b240811f31c026a`.
Local release artifact:
`C:\Users\denis\Documents\PS5\output\ProsperoTube-alpha-5a65588\PPSA99142.zip`.
Installed on .30: all 185 files matched after the 120-second persistence
check, error records were unchanged and the console remained idle. This
production binary was not launched; no public release was published.

The timing/endurance results below remain evidence for the unchanged d26bfd0
scheduler; the background EOF change has host regression evidence.

## Latest 4K hitch investigation (2026-10-15)

Candidate `d26bfd0` adds bounded presentation scheduling after GPU completion,
only for low-rate video on the known 120 Hz output mode. Two .30 replays of the
same 4K/24 clip independently measured 596 five-refresh and three four-refresh
intervals, with no six-refresh intervals, in each steady 25-second window.
The baseline was 440 / 81 / 78 respectively. The four-refresh correction about
every 8.34 seconds is required for 24 fps on 119.88 Hz. CPU retirement and raw
VideoOut status agree; the status timestamp ABI is still unnamed.

The measured cadence defect is corrected, but **TV smoothness and physical
A/V sync remain unaccepted**. Median fetch-to-retirement latency increased
about 17 ms. Frame leases/fences are unchanged. Host tests cover 119.88/120 Hz,
GPU stalls, clock drift, pause and seek. The separate `d26bfd0c` .30 control case
passed pause/resume and +10-second seek, with 29 confirmed pictures in the new
epoch. It restored saved state and closed with healthy services and unchanged
error records. Two preceding case failures were private control-harness bugs
(OSD auto-hide and wrong UI snapshot nesting), not product pause failures;
both restored state and closed cleanly. A later extended case watched 30.06
seconds after recovery, advancing 30.08
seconds with 744 confirmed post-seek pictures and clean teardown. Seek preroll
and queue refill initially took about seven seconds. Local ZIP packaging now
normalizes
archive permissions like CI, and local-save failures produce a visible notice.

A final control run gave resume its own 16-second watch, then watched another
30 seconds after seek. Position advanced 16.04 and 30.04 seconds respectively;
1,458 pictures were published with zero reported late drops. Transient long
refresh gaps occurred only in the first three seconds after each transition.
After five seconds, resume had 297 five-refresh / two four-refresh intervals,
and post-seek had 617 / three, with no six-refresh intervals or retirement
failures. This establishes bounded recovery, not an instantaneous transition
or physical A/V acceptance. The console again closed cleanly and restored state.

The same candidate then passed a 300-second public-network replay of that
4K/24 clip: position advanced 299.77 seconds in 300.4 seconds, with 7,250
published frames and zero reported late drops. Saved files were restored,
services remained healthy, the title closed and error records were unchanged.
The bounded timing trace covers only the first 2,048 pictures (about 85 seconds),
while playback progress and final health checks cover the full run. Across the
steady trace there were 1,916 five-refresh intervals, ten expected four-refresh
corrections and no six-refresh intervals. This is bounded endurance evidence,
not attribution of the historical GPU faults.

The production ZIP from `d26bfd0` is now installed on .30. All 185 files
matched after the 120-second stored-byte verification; the console is idle and
error records unchanged. It has no timing diagnostics or development controls.
Production eboot SHA-256:
`6f72ce284272c1c661a1379c63a1cc640981233a5b830e058925c045a11f9e6c`.
ZIP SHA-256:
`1c988284c8133d7b5e5c4cd5d8fc86034ac53b90228176e3efd53f0d71cfabdb`.
Local release artifact:
`C:\Users\denis\Documents\PS5\output\ProsperoTube-alpha-d26bfd0\PPSA99142.zip`.
The timing and endurance runs above used the matching diagnostic build;
the final production binary was verified in place but not launched. Later background EOF and localization changes require a new production build. No public release or catalog entry was published.

The bounded `de96f13` timing run on .30 completed one 32-second Guest replay
of the same 4K/24 clip. All 790 traced pictures had consecutive serials and
regular 41/42 ms PTS. In 668 steady first-picture intervals, CPU-observed
flip retirement occupied four refresh ticks 94 times, five ticks 483 times,
and six ticks 91 times at approximately 119.88 Hz. Raw VideoOut status word 1
followed the same refresh lattice; its offset from the CPU observation varied
by only 92 microseconds across the trace. Its ABI remains unnamed, so this is
corroborating timing evidence, not a verified scanout timestamp or proof of TV
smoothness. The candidate above addresses the measured scheduling defect.

The run restored saved state, left .30 idle with healthy services and unchanged
error records, and released its lock. An earlier attempt failed only because
the test selector expected a full title while the UI truncated it; its teardown
also completed cleanly. Neither run establishes that the visible hitch is fixed.

Previous visual result: the owner replayed the verified `289e1c2` production candidate
and still saw hitches with smooth audio. Native VP9/24 at 3840x2160 published
590 frames over 25.3 seconds, with zero late drops and no decoder fatal. The
console was idle afterward; error records were unchanged from the pre-install
baseline and all 185 installed files matched. Do not mark the hitch fixed.
Opt-in `PROSPEROTUBE_FLIP_TIMING_DIAG=1` collects first-picture timing and raw
VideoOut status in bounded memory, dumping only after playback stops. Raw
status fields other than the known flip marker have no verified timestamp ABI.


The owner initially requested offline analysis, then authorized .30 testing. Keep .30 as
the only test console and build ZIPs only. Audio, TV HDR indication and stable
playback-menu transitions have owner confirmation on `b8d57c0`; smooth 4K is
still unaccepted.

A verified audio-clock interpolation candidate (`90ed17f247d5bbd8` eboot prefix)
still visibly hitched on native 3840x2160 VP9/24, despite 807 published frames
and zero late drops in 34.8 seconds. Evidence:
`/home/denis/pt-console/clock-smooth-production-verified-30/`.
An earlier response arrived with an unchanged log. A raw FTP fetch returned
the executable ELF view, which was incorrectly compared to the signed SELF;
that hash difference did not prove a different installed revision. The later
stored-byte-verified replay supplies the result used here.

Offline review then found a specific delayed-output association bug:
`decode_one` used the current input's VP9 show flag for a picture from an
older input. Local decoder research establishes that hidden coded frames also
produce outputs. The new `vp9_output_order_overlay.py` queues each coded
input's PTS and display flag and consumes them together on decode and drain.
`scripts/test_vp9_output_order.py` compiles the generated functions with a
bounded delayed-output oracle. The old implementation fails picture identity
(expected picture 9 / PTS 9000, got picture 10 / PTS 9000); the fix passes,
including hidden/show-existing, drain, seek, errors and queue bounds. Existing
native frame-lease tests also pass. The later `289e1c2` console replay includes
this correction and still visibly hitches. Retain the picture-order fix, but
do not infer smoothness from counters alone.

**Do not deploy the `f4aaba6` ZIP.** A follow-up integration review found that
the five-slot reorder queue retains four pictures between calls, while one
split VP9 packet can return two delayed visible pictures. The second picture
then overflows the queue and makes the corrected decoder fail. The initial
mock test emptied the queue after every coded AU and missed this boundary.
The replacement processes split packets incrementally and resumes after
receive frees a slot. It also retains VP9 FIFO order when visible subframes
share a timestamp; the old slot-index tie break could return a newer picture
first. The new integration test executes generated send/receive code with the
real five-slot limit, including eight-AU bursts, retries, EOF and seek. The
previous candidate fails overflow; capacity-only processing fails picture
order; the combined correction passes both. The withdrawn `f4aaba6` candidate
was never deployed; the corrected `289e1c2` replacement was tested as below.

The production ZIP was built offline from clean commit `f4aaba6` with no
remote-control or timing-trace build flags:
`/home/denis/pt-candidates/vp9-order-f4aaba6/PPSA99142.zip`.
Eboot SHA-256: `1f7ab0076f5bbbb1b17021bd05f4337d1f260bbcd2679b445b1146be7d72a1a1`.
ZIP SHA-256: `2a9c34edd1627c785b352840f989849ad12502793e714d171b7023838a68303a`.
Full PS5 build, signed-container integrity, packaged Lapy helper, ZIP CRC and
embedded-eboot equality passed. This withdrawn artifact was never installed
or launched and is retained only as historical build evidence.

The earlier CPU-only flip-timing probe was archived outside product source at
`/home/denis/pt-console/offline-hitch-diagnostics/`. It measures CPU-observed
retirement, not authenticated scanout time, and is not part of the release.
The generated renderer already uses a locked frame/PTS/serial snapshot; the
unpatched upstream shared-PTS race does not exist in the built application.

The replacement production candidate was built from `289e1c2`. All 185 frozen
files, ZIP CRC, embedded executable equality, signed-container integrity and
packaged Lapy helper passed. Executable SHA-256:
`5107572b04f49e7a366a57e5d4860f55a0f52bd3d33b2fa91ccdd736b4201fc5`.
ZIP SHA-256:
`7a2881f8c4f6f5a5381bfeb3dbe6c6223a8eb9c1911f6bb0b94d77eaace8cb52`.
After the owner closed the prior title, the replacement was installed and
verified again after 120 seconds: all 185 packaged files matched, executable
hash matched, and the console remained idle. No launch was sent. The owner
was asked to replay the same 4K clip, report smoothness/audio sync, then Close.
That replay still hitched, as recorded above; installation did not establish smoothness.

## Playback continuation on 2026-10-14

A later 2026-10-15 public live DVR candidate (`964bb5c`, eboot SHA-256
`4eba00b13cacdf11ceb9e4f17889780dfdc99d0fd7bf6032ca4048574ffab927`)
passed on .30 as Guest with Bloomberg Business News Live (`QB5BNdBFujE`).
Native 1280x720/29.97 AVC playback rewound 3591.58 to 3472.61 seconds
after a -120-second command and resumed frames. After stop and an 18.4-second
playlist-window advance, reopen restored the saved UTC point within 2.61
seconds and resumed frames. The candidate includes HLS fixes for timezone
offsets, discontinuity metadata, AAC DTS with absent PTS, and wrap reference
reset on seek (`e1d0806`, `2fdf356`, `964bb5c`). One launch, saved-state
restoration, unchanged error records, idle/healthy services and ShadowMount
release passed. Private evidence:
`/home/denis/pt-console/public-dvr-bloomberg-964bb5c-30-run/result.json`.
Long or high-resolution public live DVR and physically audible sound remain
unverified.

The detailed 4K fault chronology below is historical. The current source adds
DVR read interruption and UTC restore, automatic frame-rate switching from
actual VideoOut state, PQ/HLG/SDR selection, pending-start cancellation, strict
GPU-mapped UI allocations and separate image/control HTTP lanes. Bounded console
evidence now includes synthetic live DVR rewind/restore on .30; PQ/HLG/SDR
scanout and first-playback 60-to-120-to-60 Hz switching on .40; direct
30-to-25 fps active switching from 120 to 60 Hz on .40; a 304-second direct-LAN
4K VP9/Opus run with seeks on both consoles; and a held-artwork control
request completing in 51 ms on .30. See [Status](STATUS.md) and
[Hardware acceptance](HARDWARE_ACCEPTANCE.md) for the exact build and limits.

Three later GPU unmapped texture reads at 0x2a5248000, 0x2a50fb000 and
0x2a5256000 occurred before the strict GPU-pool guard. They lie outside
logged scanout and decoder direct mappings; the diagnostic generic direct
pool occupied 0x272c00000 through 0x276c00000. The old allocator could
fall back to ordinary malloc for HUI/RmlUi GPU descriptors when its 64 MB slab had no
fitting chunk; its usage counters did not include fallback allocations.
That is an address-compatible hypothesis, not an identified owner.
Decoder-internal mapping ownership remains possible. If a fault recurs, log
each bound AGC texture/vertex address and allocator origin at submission to
correlate it with the reported fault address. Current source also retains HUI
textures until GPU fences retire, including at shutdown. Generated host checks
and a signed PS5 build pass. The revised build completed 304-second 4K
playback with rewind, forward seek and natural EOF, followed by 4K Afterglow
texture/navigation churn and 12 seconds of idle, on .30 without a new fault.

One later public BBC 4K/25 attempt on .30 got HTTP 403 at DASH segment 85
before any frame; exact state restoration passed. Host probes reproduced
intermittent signed links that returned 206 for the first 64 KiB but 403 for
the midpoint and segment 85, alongside links that returned 206 for all three.
Current source probes the selected video's midpoint in the existing bounded
same-client retry loop. It does not strip signed query parameters. A .30
retest of this link-selection fix reached native 3840x2160 VP9/25 playback
without an HTTP 403. The first 150-second interval advanced 150.14 seconds;
a -60-second seek landed within 0.5 seconds of target. The second 150-second
interval advanced only 24.07 seconds, with output falling to about 3.5
frames/s. Video packets stayed queued while audio packets ran dry. FFmpeg's
premature-file-end notices also occurred during the healthy interval, and
there was no GPU fault. Audio starvation is plausible but unproven. A focused
.30 diagnostic retest advanced 149.93 seconds in the first 150-second interval,
landed a -60-second seek, then advanced 53.38 seconds in the next 60 seconds.
Token-safe counters showed about 6–7 seconds of seek preroll before audio
packets and PCM output resumed; thereafter audio and video clocks tracked
closely and PCM underruns stayed flat. The earlier sustained 3–4 frames/s
collapse did not reproduce in this one run. There was no HTTP 403, GPU fault
or new error. Stage 6 restored all nine saved-state files, verified idle and
services, and released the console lock. Evidence is at
`/home/denis/pt-console/seek-audio-diag-30-run`. Temporary diagnostic
instrumentation was removed from product source.

The selected 4K DASH/WebM track advertised a bogus 2 kbit/s rate and its
video read-ahead was capped at 8 MB. Commit `cd45171` keeps 4K read-ahead at
48 MB for implausibly low rates without changing the unrelated core item's
8 MB cap. The signed `prosperotube-cd45171b_1014-2355` build (eboot SHA-256
`9697e4ecb8d60a267848138b34d2f13efd3db818bdefc719195d499662dad01f`)
passed two independent .30 public BBC 3840x2160 VP9/25 sessions, each with
150 seconds of playback, one -60-second seek and another 150 seconds. First
intervals advanced 149.97 and 149.84 seconds; post-seek intervals advanced
138.88 and 144.85 seconds. Both captured different BBC footage in initial and
post-seek 4K scanouts. Neither logged HTTP 403, `DEMUX_OVERSHOOT`, a GPU fault
or a new error record. Both used one launch, restored all nine saved-state
files, left the title idle and services healthy, and released the .30 lock.
Evidence: `/home/denis/pt-console/read-ahead-cd45171-30-run` and
`/home/denis/pt-console/read-ahead-cd45171-30-repeat`. The earlier bounded
test reproduced a severe slowdown before the cap fix; it did not recur in
these two passes. Continue treating other public CDN/timing conditions as unproven;
the historical GPU fault address owner is still unknown.

The current ZIP-only production package was built from `964bb5c` without
the USB remote: `/home/denis/pt-candidates/playback-20261015-final-964bb5c/`.
Its signed `PPSA99142/eboot.bin` has SHA-256
`dd9cf60618b3a1a9c0125e098644891620f57201879896e1334bb53228ec02b2`;
`PPSA99142.zip` has SHA-256
`ca5777c67b1f33e8b5f378e2109c0f7842f27ddf03f86b2d2bf0a75672100822`.
Signed-container and packaged Lapy helper checks passed. The ZIP passed
full CRC verification and its embedded eboot hash matched the folder.
The 185-file production folder was installed on .30 solely to restore the
title after the public DVR test: 185/185 files matched after 130 seconds of
settling, then the title was idle, error records unchanged and the lock
released. It was not launched or playback-tested as a production build; the
separately signed USB-remote candidate above supplied the DVR evidence.

The same instrumented `964bb5c` candidate later passed a 600-second direct-LAN
4K VP9/Opus watch on .30 with two -170-second rewinds, three distinct nonblank
4K captures, no stall or GPU fault signature, and unchanged error records.
Teardown restored all ten saved-state files, including the owner's
`afterglow.on`, then released the title and lock. Evidence:
`/home/denis/pt-console/lan4k-964bb5c-30-run2/result.json`. A separate
application acquired the .30 lock immediately afterwards, so the instrumented
candidate remains installed there; the production ZIP and folder above remain
verified locally but have not been launch-tested. Do not replace the installed
title while that other application owns .30.

## Where things stand

- `main` holds everything. The repository is **private**; it goes public only
  when the owner says so after reviewing the README. The history is not to be
  squashed (owner's decision).
- No version has been released. 0.1.0 (`01.000.000`) waits until every
  Afterglow screen is built and validated on a console
  ([decision](DECISIONS.md)). No tag, version bump or release without the
  owner's word.
- The app is self-sufficient on the console: a built-in YouTube service
  (QuickJS-NG running YouTube.js, `standalone/`) answers the provider on
  `127.0.0.1`; playback is EVO Player patched at build time by
  `scripts/prepare_native.py` and the `scripts/*_overlay.py` files. The PC
  companion in `companion/` is a development tool only.
- Two interfaces: Classic (RmlUi, the default) and Afterglow (the owner's UI
  kit, `native/afterglow/`), chosen in YouTube preferences > Interface, or
  forced with an empty file `afterglow.on` / `afterglow.off` in
  `/data/prosperotube`.
- `GOAL.md` describes the scope of 2026-10-11 (companion era) and is out of
  date; the README roadmap and this file supersede it.

### What the owner checked by hand on a PS5 (2026-10-14)

Afterglow drew; Guest Home with real video shelves, opening a topic (with
thumbnails), search with suggestions while typing, About, the player screen
with a spinner while a video starts, several videos played with seeking, the
new icon, backgrounds and selection music. No EVO Player notice at start.

### Fixed in the last session, worth knowing

- **YouTube refuses open-ended ranges since 2026-10-14.** A request for a
  whole media file (`Range: bytes=0-`) returns 403. The provider now asks the
  service for ranged manifests: each MP4 track's fragments as small byte ranges
  read from its index (`native/tube_provider.c`, `standalone/js/resolve.js`).
  The continuation now also indexes WebM and fixes FFmpeg's range-boundary
  EOF handling. VP9 1080p with AAC passed on PS5; VP9/Opus generated fixtures
  pass on the host. Bounded offline 4K/30 and live YouTube 4K/25 VP9 console
  runs now pass; two five-minute public-network 4K VOD sessions with rewind
  also pass. Longer public-network endurance, audible Opus and physical HDR
  indication remain pending.
- **Lapy elevation follows ProsperoEden's current method** (helper built with
  upstream's `make owned-helper`, manifest shipped unchanged, packaged copy
  checked, every outcome logged). ProsperoTube deliberately differs in two
  ways: it needs `/data` to be listable, and it has no sandbox fallback (it
  stops at startup without access). See `native/elevation/README.md`.
- **EVO's packager copies a fixed list of `sce_sys` pictures**; the overlay in
  `prepare_native.py` adds the DDS backgrounds to it. The owner's art and music
  are in `native/branding/` (`icon0.png`, `pic0.dds`, `pic1.dds`, `snd0.at9`).
- Code patched into EVO's `ui_rml/src/evo_rmlui_provider.cpp` is also compiled
  by PC tests where `EVO_AGC_DEVICE` is not defined: guard calls into the
  Afterglow header with it.

## What to do next

In the owner's order of interest (he asked what is missing; the list is the
README's **Roadmap**). The two recommended first:

1. **Complete VP9 and Opus validation**: ranged manifests for WebM tracks
   (Cues give the byte ranges), so 4K VP9 and HDR come back. Implementation now enables WebM
   indexing and fixes FFmpeg's zero-byte range-boundary read, which caused
   the seek hang. Generated MP4/WebM seeks pass on the host and VP9 1080p
   passed on PS5 (ec4660ac; HARDWARE_ACCEPTANCE.md). The offline 4K/30
   VP9/Opus path now passes on .40; a bounded live 4K/25 path passes on .30.
   Long public live playback, audible Opus and physical HDR indication remain
   to validate; synthetic PQ/HLG/SDR selection is recorded in hardware acceptance.
   The combined candidate 93b2e0c7 passed native VP9 1080p again, then faulted
   on the GPU after the first valid 4K frame of a second video. Local hardware
   decoding research exposed a frame-pool reuse race; explicit leases now hold
   native surfaces through GPU retirement, with generated ownership and package
   checks passing. The 8c8d7b15, 0184b32d and 88c154a6 retests passed 1080p,
   then faulted on retired 1080p decoder allocations after a first 2160p
   surface; bounded close draining alone was insufficient. The ca173808
   candidate instead kept one VP9 decoder at a 4K-capable maximum: 1080p and
   full 3840x2160 shared its frame pool with no resize. The full-width frame
   reached the presenter, then a different GPU unmapped read outside the
   decoder pools stopped playback. A later diagnostic build with address logs
   but unchanged playback logic opened full 3840x2160 and published 52 frames
   with zero late drops and no GPU fault. That short observation does not
   explain the earlier first-display fault or accept sustained 4K; sound was
   unjudged and Afterglow stop/return checks failed. Every case restored state
   and left the console idle. The AGC display path is the current investigation.
   Later .30 cases reproduced an unmapped GPU read near `0x2a5` even with
   AGC fence retirement and with VP9 close/open Reset omitted. The latest
   fault occurred before a valid 4K output frame and outside every logged
   decoder pool. The driver restored saved state but retained the .30 lock
   after a new console error record. The owner subsequently authorized
   clearing it for another bounded test. A strict GPU-pool guard and a fix
   for an AGC UI frame left open on no-swap loops now pass host checks. On .40,
   the frame fix eliminated
   command-buffer and ring exhaustion, but live-video testing stopped at
   DNS failure for YouTube and homebrew.page. An offline generated 8-second
   3840x2160 VP9 fixture on .40 then opened the native decoder, presented
   full-width frames and published all 240 with zero late drops or GPU faults.
   At 7.99/8.0 s, natural EOF, audio drain, video decode and display retirement
   gates were true and the player was paused. The source pauses after observing
   completion when no next item exists, but one-second status polling never
   sampled `complete=1`; the strict completion observer therefore timed out.
   This is not evidence of stalled decoding. Clean teardown restored state and
   released the .40 lock. A subsequent 64-second offline 3840x2160 VP9/Opus
   case on .40 passed 15/30/45/55-second checkpoints and reached 63.99/64 s
   with natural EOF, audio drain, video decode and display retirement gates
   true. The native decoder published all 1,920 frames with no late/seek drops,
   decode fatal or GPU fault. Stage 6 parked and closed the app, restored state,
   left services healthy and released the .40 lock. This accepts one bounded
   offline 4K/30 path, but does not isolate the strict GPU-pool guard or prove
   HDR, physical audio, 4K seek/repeat, very long sessions or a directly
   sampled completion transition. The same frozen 57efd5b2 candidate then
   ran a bounded live YouTube 4K/25 case on .30: Classic native VP9 1080p and
   a +10 s seek passed, followed by the public BBC Earth item at native
   3840x2160. Position advanced by 11 s; 315 frames were published with four
   late drops (1.3%), no decode fatal or GPU fault. It stopped through the
   confirmation and returned to Afterglow. Stage 6 parked and closed the app,
   restored state byte for byte, found services healthy and error records
   unchanged, and released the .30 lock. The 4K file discarded 253 frames
   as seek preroll at the saved 600 s position, separate from the four late
   presentation drops. FFmpeg logged premature-end and earliest-resync
   warnings in the WebM media stream; playback recovered, but transport and demux
   robustness remain to validate. No playback scanout image was captured, so
   visual output rests on presenter counters, not a picture review. This is
   one bounded live 4K result, not long playback, audible output, HDR or 4K
   seeking. The earlier network first-frame faults remain unexplained. See
   HARDWARE_ACCEPTANCE.md for the exact cases.

2. **Afterglow player as on its boards**: next-video card, Undo after a
   skipped segment, grouped options, tabbed captions, chat beside the video.
   What the existing player cannot tell yet is listed in
   `docs/afterglow/requests/H.md`.
   The continuation wires the next-video card's five-second Play now/Cancel
   offer, automatic-skip Undo and category colours. Host provider/controller
   checks pass; console acceptance remains pending. Grouped options and
   caption tabs now use the real picker actions and pass generated controller
   and board checks. Chat beside the picture is implemented using the existing
   video quad and Player layout, with generated geometry and lifecycle checks;
   console calibration remains pending.

Then: the Afterglow translations (all 21 catalogs and integer-count forms are
complete; native-speaker and console review remain;
`python3 scripts/i18n_extract.py`, catalogs in `native/i18n/`), live DVR,
keep-playing-while-browsing (implemented and host-tested, console pending), the interface's
own sound assets (`docs/afterglow/sound-design.md`), and the smaller items
of the roadmap. The Live now shelf is implemented for Guest and account Home,
loaded when reached and filtered to live videos. Provider checks and 62 Home
boards in four looks pass; console acceptance remains pending.
The bell now uses the validated account-wide unseen count, refreshed every
minute and after loading notifications; unknown counts hide the badge.
Notification rows preserve upstream read flags. Unsupported remote mark-read
actions are unavailable. Host service/provider/board checks pass; real-account
console acceptance remains pending.

The interface now packages 34 original procedural WAVs covering all 28 cues,
with reproducible source and provenance. Format, level, mixer and packaging
checks pass. Human listening and console acceptance remain pending.
The Afterglow controller map, verified Keep watching stop default, and Library
queue Move up/down actions have generated input, native persistence and four-look
board coverage. All still need console acceptance.

Live DVR includes the production SDK extension and passed bounded .30
synthetic split-HLS and public Bloomberg 720p AVC/AAC rewind/UTC restore.
Longer-window and high-resolution public console validation remain pending.

### Open decisions for the owner (do not decide them yourself)

- Update decision resolved on 2026-10-14: use the homebrew.page catalog and
  helper, like the owner's other apps. Integration and host checks are done;
  a complete PS5 package also builds. Console acceptance and a published signed
  catalog entry and release are required for an end-to-end update.
- The README footer entry for ProsperoTube in the protocol repository's
  `scripts/readme-footer/repos.toml` (prepared, not committed).
- Making the repository public, and the first release.
- Storage notice decision resolved on 2026-10-14: show an explanation before
  closing when Lapy access is missing. The startup overlay now sends a system
  notification; console acceptance remains pending.

### Loose ends

- The saved sound-last console-driver edits are reviewed and integrated:
  earlier Afterglow checks silence sound/vibration, the last step checks
  audio-port initialization and frame progress, and teardown restores all
  overrides. Offline driver tests pass; the original stash is retained as
  a backup. The revised sound step was reached after the 4K stall and could
  establish no sound result. A regression now requires frame progress before
  enabling sound; all 132 driver tests pass.
- The six Afterglow screen packages now reuse the existing shared pieces
  (ARCHITECTURE.md section 12d). The migration passes 611 boards in four looks
  and targeted checks of the final cleanup. Typed Settings/comment/reply
  submissions now reach the provider. Console acceptance remains pending;
  remaining `// REQUEST:` comments name backend or design limitations.
- Pull request 1 (`codex/first-version`) is already merged; verified on GitHub
  during the 2026-10-14 continuation. No closure is needed.

## How to work on it

- **Build and test from Linux or WSL.** Write commands into script files and
  run those; long jobs as background tasks. Never kill processes by script
  name (`pkill -f`): other worktrees run the same scripts.
- **Checks.** The full offline suite `bash scripts/test_all.sh` takes 15 to 25
  minutes; the owner asked (2026-10-14) not to run it before each console
  install for now. Run what the change touches, for example:
  `bash scripts/test_package_prepare.sh`, `bash scripts/test_native.sh`,
  `bash scripts/test_embedded_service.sh`, `bash scripts/test_embedded_e2e.sh`,
  `bash scripts/test_afterglow.sh` (Afterglow boards in four looks, about three
  minutes), `bash scripts/test_i18n.sh`, `python3 -m pytest -q`. Ask before a
  release whether the full suite should run.
- **PS5 package.**
  `PROSPEROTUBE_NATIVE_WORK=~/.cache/prosperotube/native-ps5 PROSPEROTUBE_PS5_DIST=<dist> bash scripts/build_ps5.sh`
  writes `<dist>/PPSA99142/` and `PPSA99142.zip`. An
  instrumented build for console sessions adds `--usb-remote --virtual-keyboard`.
  Freeze a candidate with the protocol repository's
  `scripts/freeze-candidate.sh <dist>/PPSA99142 <frozen>` and check that
  `sce_sys` holds the four branding files.
- **Install on a console.** Files only, with the protocol repository's
  `scripts/ps5-console.py install PPSA99142 <frozen>/PPSA99142` (`PS5_HOST`
  set). It refuses while ProsperoTube runs; an install needs no console lock.
  It does not delete files a build no longer has: remove stale ones by FTP.
  App logs are in `/data/prosperotube/console.log`.
- **Console sessions** (launching the app) follow the shared-console rules:
  take the console's lock file first, check that nothing is running, never
  close another session's title, one launch per session, no blind retries,
  ask the owner before each session. The driver is `tools/console/session.py`
  ([README](../tools/console/README.md)); `--afterglow` adds the Afterglow
  stage.
- **YouTube from the PC**: anonymous, read-only, a handful of requests at a
  time. Never sign in with a real account; the owner tests accounts himself.
- **Do not edit** `third_party/evo` (change an overlay instead) or the UI kit
  (pinned by `scripts/ui_kit.py`, fetched at build time).
- **Commits**: one-line imperative subject, no body, no trailers; author
  BlackBearReloaded. Push to `main` once the owner has confirmed a change on
  the console, or when he asks.
- **Public-facing text** (README, docs, notices) names no private repository
  and no personal paths or LAN addresses.
