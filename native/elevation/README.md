# Lapy integration

The cooperative client is adapted from ProsperoTV revision
`a33031e9e50b4942ca0e2287cc757271345d874f`,
`opengl-ui/ps5/src/elevation`. GPL-3.0-or-later source retains its notices.
ProsperoLight's existing/resident/helper regression and ProsperoTV's mounted
`/data` listing regression are retained and extended in `tests/test_elevation*`.

`scripts/build_lapy_helper.py` builds the upstream MIT helper at
`c3bdfe3a399366d8eacfc580f20b19fd03b16ca3`, for `PPSA99142` only. Source,
SDK archive, logging header and protocol are pinned and verified. The helper
allows one request, installs no service, and ships with its manifest/license.
Its own manifest explicitly does not claim console validation.

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
