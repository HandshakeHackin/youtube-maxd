# Third-party notices and corresponding source

ProsperoTube: GPL-3.0-or-later. Original provider/service/UI/overlay copyright
(C) 2026 BlackBearReloaded. Complete GPL text: LICENSE.

* EVO `9715f6ee9bf9b53461d6fdf9a2e11b4c48357783` derives from GPL-3.0-or-later
  ProsperoPlayer. Full source/notices are preserved in `third_party/evo`, including
  COPYRIGHT.md/LICENSE. Recursive UI pin:
  `4bd942579dd981b3df9c740438489ca6a614ddc0`. Initialize recursive submodules.
* cJSON 1.7.19: MIT, complete vendored source/license/provenance in
  `native/vendor/cJSON`. Replaces EVO's unsafe slim JSON printer in the build.
* Lapy exact-title helper: PS5-Lapy-JB-Daemon
  `c3bdfe3a399366d8eacfc580f20b19fd03b16ca3` (MIT), built from verified pinned
  source by `scripts/build_lapy_helper.py`; the helper's manifest and complete
  MIT license ship with the app. Its GPL-3.0-or-later cooperative client and
  build recipe are adapted from ProsperoTV
  `a33031e9e50b4942ca0e2287cc757271345d874f`; existing/resident/helper regression
  coverage derives from ProsperoLight. Source: `native/elevation`.
* FFmpeg 7.1.1 minimal software profile, dav1d 1.5.1, RmlUi 6.2, SDL2,
  FreeType, OpenSSL, zlib and Pacbrew libraries retain upstream licenses.
  Source recipes/checksums are in EVO scripts and Pacbrew v0.39 source.
  FFmpeg emits its configuration/license text. No new PS5 hardware codec is added.
* PS5 Payload SDK v0.42 / Pacbrew v0.39 provide toolchain/runtime. MkPFS
  `6cb8313dfe0c988ac52617794553f343243d3a56` packages the image.
* Python uses yt-dlp/EJS, FastAPI, Uvicorn, HTTPX and dependencies. Exact versions:
  `companion/requirements.lock`; licenses remain in upstream package metadata.
* Optional companion library: YouTube.js 18.1.0 (MIT), inspected source pin
  `bad89d2657e88f907011655f199fba9fb615c339`; complete package/dependency pins
  and integrity hashes are in `companion/innertube/package-lock.json`.
  esbuild 0.25.10 (MIT) builds its host-side worker bundle. Dependency license
  files remain in the installed packages; keep them with binary distributions.

Matching source consists of this repository, recursive submodules, pinned
dependencies and build overlay. Keep these notices with binary distributions
and make the matching source revision available. Launcher art/fixtures are
original code-drawn/synthetic assets. SmartTube/MediaServiceCore are architectural
references only; their code/assets are not distributed here.
