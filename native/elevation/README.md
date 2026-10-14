# Filesystem access with upstream Lapy

ProsperoTube gets its data folder, `/data/prosperotube`, the way
[ProsperoEden](https://github.com/blackbearreloaded/ProsperoEden) does
(`headless/elevation`, compared at `971ad0a`, 2026-10-13). It uses
[PS5-Lapy-JB-Daemon](https://github.com/ArkSama/PS5-Lapy-JB-Daemon), created by
ArkSama, and pins the cooperative owned-root implementation from
[the BlackBearReloaded Lapy fork](https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon),
based on [mpereiraesaa's cooperative helper](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon).

## The helper

If access or the app's write/readback proof fails, startup posts a system
notification explaining how to enable the ELF loader or Lapy service, then
stops before starting the player or YouTube service. The notification uses the
existing kernel notification path, so it does not require the app's renderer
or writable storage.

`scripts/build_lapy_helper.py` builds commit
`57fcc19b7f57e46c823d6f2a6c6233ebf3288079` (the donor-release and firmware
13.60 fixes of upstream PR #48 and #49, and the guarded target-lifetime and
fail-closed accounting of
[PR #50](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/50)) with
upstream's own `owned-helper` make target for `PPSA99142`, the official Payload
SDK v0.42 and the pinned ps5log header. Lapy's privileged source is neither
copied nor changed. Before packaging, the build checks the generated manifest's
title, one-shot mode, single request, no service, `root_layout_probe_retry`
feature and its ELF and protocol hashes; `build_ps5.sh` checks the packaged copy
again. The package carries `lapy.elf`, the upstream `lapy-manifest.json` as
generated, and Lapy's MIT licence (`assets/prosperotube-notices/Lapy-MIT.txt`).
It contains no locally implemented kernel mutation code. The manifest does not
claim console validation.

## At start

`tube_jailbreak.cpp` makes one request, before any worker thread starts (upstream
Lapy accepts only a single-threaded target): library preloads come first, the
boot log stays in memory and the kernel log until storage is proven
(`scripts/test_startup_log_gate.py`), and video decoder and display resources
are created after it. `elevation.cpp` then, as in ProsperoEden:

1. checks whether `/data` already works (write, read back, delete a probe file):
   route `existing`;
2. otherwise publishes the cooperative resident-service request through
   `/download0/elevate_proc` and waits 1.5 seconds for verified access: route
   `resident`. If a resident service claimed the marker, the one-shot helper is
   never started, and the claimed request gets one more second;
3. otherwise cancels the unclaimed marker, waits one more second in case a
   resident read it at that moment, and sends `/app0/lapy.elf` to the local ELF
   loader on TCP port 9021: route `helper`. The same connection carries
   upstream's fixed-width ELV1 version 1 request/prepare/prepared/response
   exchange (24 bytes, 5-second socket timeouts); replies must match PID,
   capability, version and phase. After a successful response, `/data` must
   prove itself again within one second.

Only a proven `/data` counts. `tube_jailbreak.cpp` then creates
`/data/prosperotube` and writes, reads back, closes and deletes a file there.
The boot log (kernel log and `console.log`) gets `Lapy: requesting filesystem
access` and then one line with the route, the status, the storage verdict and
the user and group ids the process was left with.

## Where ProsperoTube differs, and why

- **`/data` must also list.** ShadowMountPlus 1.7 mounts `/data` into a sandboxed
  app; with some versions files open and write but listing is refused (EPERM).
  ProsperoEden accepts such a mount as `existing`. ProsperoTube lists its
  folders (backups, imports, updates, fonts), so it treats a mount that does not
  list as no access and goes on to Lapy. Where the mount lists, the behaviour is
  the same as ProsperoEden's. `tests/test_elevation_listing.cpp` covers it.
- **No access means no start.** Without access ProsperoEden keeps running with
  no data, keeps only its logs in `/download0` and says so in its launcher.
  ProsperoTube stops before any service starts (`startup stopped` in the boot
  log), because its settings, profiles, history and signed-in accounts would
  otherwise be lost at the next start. A visible notice before closing would be
  the ProsperoEden-like improvement.
- **No group fix-up.** ProsperoEden sets the effective group back to the real one
  after elevation because Mesa turns RADV's disk shader cache off when they
  differ. ProsperoTube does not use RADV; the ids are logged so a console run
  shows whether they differ.

Old sandbox data is not copied into `/data/prosperotube`. Protocol markers use
`/download0`; application data uses only `/data/prosperotube`.

## Tests

`scripts/test_elevation.sh` builds `tests/test_elevation.cpp` (ProsperoEden's
cases with short reads and writes, the exact request and result files and the
wire bytes, plus wrong PID, refused loader, corrupt data and failed prepare)
and `tests/test_elevation_listing.cpp` under AddressSanitizer and UBSan, then
runs `tests/test_lapy_helper.py` (pins equal to ProsperoEden's, upstream make
target, manifest and packaged-copy refusals). `scripts/test_native_preload.py`
checks that nothing starts a thread before the request and that video and
display resources follow it.

## Credits

Lapy was created by [ArkSama](https://github.com/ArkSama), and the cooperative
helper comes from
[mpereiraesaa and contributors](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/graphs/contributors).
The client in this folder is GPL-3.0-or-later, the same client as ProsperoEden's
(first adapted here from ProsperoTV).
