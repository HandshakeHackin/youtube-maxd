# Lapy integration

The cooperative client is adapted from ProsperoTV revision
`a33031e9e50b4942ca0e2287cc757271345d874f`,
`opengl-ui/ps5/src/elevation`. GPL-3.0-or-later source retains its notices.
ProsperoLight's existing/resident/helper regression and ProsperoTV's mounted
`/data` listing regression are retained and extended in `tests/test_elevation*`.

`scripts/build_lapy_helper.py` builds the upstream MIT helper at
`57fcc19b7f57e46c823d6f2a6c6233ebf3288079`, the inspected head of
[upstream PR #50](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/50),
for `PPSA99142` only. Source,
SDK archive, logging header and protocol are pinned and verified. The helper
allows one request, installs no service, and ships with its manifest/license.
Its own manifest explicitly does not claim console validation.

This revision stops the target before raw target snapshots, binds its title
before and after the stop, and requires the post-donor two-reference balance
before detach. The wire protocol remains the previously pinned ELV1 protocol.
The prior helper revision remains in its separate ignored cache and frozen
candidates; it is not overwritten by this update.

Diagnostic builds may set
`PROSPEROTUBE_LAPY_RESULT_FILE=/data/prosperotube/lapy-helper.log`.
The upstream helper then mirrors its stage records through an owned file without
changing shared ps5log configuration. The test installer must create
`/data/prosperotube` before launch; otherwise the helper refuses to start. Normal
builds omit this option and require no pre-existing data directory. The packaged
manifest records the source revision and optional result path; the full upstream
manifest is checked to reject any test-only snapshot delay. This update has not
yet established ProsperoTube storage or playback acceptance on a console.

The client checks actual write/read and directory access, gives a resident
service a bounded chance, cancels an unclaimed marker before helper fallback,
and refuses fallback when a resident service has claimed the request. Wire
responses must match PID, capability, version and phase. Protocol markers use
`/download0`; application data uses only `/data/prosperotube`.

`tube_jailbreak.cpp` bridges the client to EVO's C interface, makes one startup
attempt, creates the app directory and verifies write/read/close/delete there.
Startup stops before app services if that proof fails; no render-loop retry or
transient persistence fallback remains. Decoder, IME and browser modules retain
EVO's preload ordering before the credential transition.

Old sandbox fixture data is not automatically copied or merged into durable
state. Console acceptance must verify a clean launch, exact app storage path,
save/relaunch persistence, playback and clean teardown. It is pending.

ProsperoTube startup keeps the boot log buffered until
`evo_jailbreak_is_open()` confirms both Lapy elevation and the application
storage proof. A sandbox-mounted `/data` may accept file writes without
directory access; opening a log there must not start the writer or log-server
threads before the helper's single-threaded credential precondition is met.
The generated flush regression is `scripts/test_startup_log_gate.py` (run after
`scripts/prepare_native.py`). Kernel boot breadcrumbs remain available while
the durable logger is gated.

Diagnostic helper builds can additionally set
`PROSPEROTUBE_LAPY_STOP_DIAGNOSTICS=1` with the result-file option. This copies
verified upstream sources into a separate build directory and adds only a
record of already-captured target-stop snapshot fields. The upstream acceptance
predicate, ptrace lifecycle and mutation paths are unchanged. The packaged
manifest records both upstream and compiled daemon SHA-256 values. This option
exists to distinguish the target-stop predicates rejected on the reserved
console; it has not established successful app storage.
