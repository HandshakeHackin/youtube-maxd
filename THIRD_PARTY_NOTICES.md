# YouTube Max’d fork notices

This release is YouTube Max’d, based on the ProsperoTube revision in UPSTREAM.json.
Original copyrights, credits and licenses below are retained. The local changes
are listed in MAXD_CHANGES.md. Corresponding native-library source and notices
are provided in runtime-sources/, and pinned EVO/UI source is included in the
source distribution. PacBrew v0.39 SDL identifies itself as SDL-2.30.12-g0baf4ac49;
that exact SDL source is included, with the PacBrew recipes and patches.

Max’d's light-bar effect functions and native feedback bridge are GPL-3.0-or-later.
The original icon master and Nuvio Max’d reference provenance are in branding/;
Nuvio Max’d is at https://github.com/HandshakeHackin/Nuvio-maxd-releases (GPL-3.0).
No Nuvio executable is included. The native header M/play mark is drawn by this
fork in the declared coral/pink/violet palette. Upstream splash/music assets
are omitted from the application package.

The embedded JavaScript runtime dependencies include @bufbuild/protobuf 2.16.0
(Apache-2.0 and Google BSD-3-Clause), fflate 0.8.3 (MIT) and meriyah 7.4.0
(ISC). Their notices and matching inputs accompany the distribution in
runtime-sources/. The protobuf-es source supplies its Apache license; Google's
BSD notice is retained from the distributed varint implementation.

# Third-party notices and corresponding source

ProsperoTube: GPL-3.0-or-later. Original provider/service/UI/overlay copyright
(C) 2026 BlackBearReloaded. Complete GPL text: LICENSE. ProsperoTube is an
independent client: it is not affiliated with or endorsed by YouTube or Google.
YouTube is a trademark of Google LLC.

* EVO `9715f6ee9bf9b53461d6fdf9a2e11b4c48357783` derives from GPL-3.0-or-later
  ProsperoPlayer. Full source/notices are preserved in `third_party/evo`, including
  COPYRIGHT.md/LICENSE. Recursive UI pin:
  `4bd942579dd981b3df9c740438489ca6a614ddc0`. Initialize recursive submodules.
* cJSON 1.7.19: MIT, complete vendored source/license/provenance in
  `native/vendor/cJSON`. Replaces EVO's unsafe slim JSON printer in the build.
* Lapy exact-title helper:
  [PS5-Lapy-JB-Daemon](https://github.com/ArkSama/PS5-Lapy-JB-Daemon) by
  ArkSama, with the cooperative owned-root design from
  [mpereiraesaa's fork](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon) and
  contributors, pinned at
  [`57fcc19b7f57e46c823d6f2a6c6233ebf3288079`](https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon/tree/57fcc19b7f57e46c823d6f2a6c6233ebf3288079)
  of the [BlackBearReloaded fork](https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon)
  (upstream PR #48, #49 and
  [#50](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/50)). Lapy is MIT
  licensed; its shared protocol header `source/lapy_elevation_protocol.h`
  (SHA-256 `bb02c4aa814eaba7a7a423a31b29ff41f786212c2953678cf29434e85fa0f869`)
  is LGPL-2.1-or-later. `scripts/build_lapy_helper.py` fetches that commit,
  invokes upstream's unmodified `owned-helper` build for `PPSA99012`, verifies
  the generated manifest and ships `lapy.elf`, `lapy-manifest.json` and the MIT
  licence (`Lapy-MIT.txt`) with the app, as
  [ProsperoEden](https://github.com/blackbearreloaded/ProsperoEden) does. The
  helper is built with the official
  [PS5 Payload SDK v0.42](https://github.com/ps5-payload-dev/sdk/releases/tag/v0.42)
  (archive SHA-256 `8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`)
  and includes the single-header
  [ps5log](https://github.com/mpereiraesaa/ps5-agc-gears/tree/1ae1f9182abd2770c131b97419034fb85173c2dc/native/ps5log)
  logging client, GPL-3.0-or-later (SHA-256
  `394af67d0f8b60b3335deb53396e52855ea2daa50ca914a456ea7663f48900c6`).
  The GPL-3.0-or-later cooperative client in `native/elevation` is the same as
  ProsperoEden's `headless/elevation` (compared at
  `971ad0a095b178cd00f4e751a0a1d46998d61d70`), first adapted here from
  [ProsperoTV](https://github.com/blackbearreloaded/ProsperoTV)
  `a33031e9e50b4942ca0e2287cc757271345d874f`; its regression tests follow
  ProsperoEden's and ProsperoLight's.
* The update check and self-update helper (`native/update/`) are copied from
  [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
  at `b8dc4a3fe2e05e826b037bd68824751c7468a6d7`, GPL-3.0-or-later,
  Copyright (C) 2026 BlackBearReloaded. Original source notices are retained;
  `native/update/SOURCE.json` records the pin and exact imported file hashes.
  The helper uses miniz 3.0.2 under the MIT license; its original license and
  source provenance are in `native/update/third_party/miniz/`.
  `scripts/zip_open_modes.py` is adapted from that repository's
  `tools/zip-open-modes.py`; the release workflow follows its release workflow.

* PS5 UUID entropy adapts the checked `kern.rng_pseudo` reader from
  [ProsperoLight](https://github.com/blackbearreloaded/ProsperoLight/blob/b0149a941617c9e92a3be475b78304052c718d7c/platform/ps5/ps5_entropy.c)
  (GPL-3.0-or-later). Adapted source is in `native/tube_random.h`.
* The native self-exit API usage follows
  [ProsperoLight's PS5 system service](https://github.com/blackbearreloaded/ProsperoLight/blob/b0149a941617c9e92a3be475b78304052c718d7c/third_party/ps5-homebrew-ui/platform/ps5/system.cpp)
  (GPL-3.0-or-later). ProsperoTube's overlay adds media retirement, checked GPU
  fence drain and a single self-exit attempt for the explicit disconnect choice.
* FFmpeg 7.1.1 minimal software profile, dav1d 1.5.1, RmlUi 6.2, SDL2,
  FreeType, OpenSSL, zlib and Pacbrew libraries retain upstream licenses
  (FFmpeg: LGPL-2.1-or-later, with GPL parts as configured, as EVO's
  COPYRIGHT.md records; dav1d: BSD-2-Clause; RmlUi: MIT; SDL2 and zlib: zlib
  license; OpenSSL: Apache-2.0; the PS5 Payload SDK: GPL-3.0-or-later).
  Source recipes/checksums are in EVO scripts and Pacbrew v0.39 source.
  FFmpeg emits its configuration/license text. The ProsperoTube DASH overlay
  enables indexed byte seeking only for complete single-file VOD representations;
  fragmented/live/initialization-section streams keep their existing behavior. No new PS5 hardware codec is added.
* PS5 Payload SDK v0.42 / Pacbrew v0.39 provide toolchain/runtime. MkPFS
  `6cb8313dfe0c988ac52617794553f343243d3a56` packages the image.
* Python uses yt-dlp/EJS, FastAPI, Uvicorn, HTTPX and dependencies. Exact versions:
  `companion/requirements.lock`; licenses remain in upstream package metadata.
* Optional companion library: YouTube.js 18.1.0 (MIT), inspected source pin
  `bad89d2657e88f907011655f199fba9fb615c339`; complete package/dependency pins
  and integrity hashes are in `companion/innertube/package-lock.json`.
  esbuild 0.25.10 (MIT) builds its host-side worker bundle. Dependency license
  files remain in the installed packages; keep them with binary distributions.
* The built-in YouTube service (packages built with `TUBE_EMBEDDED_SERVICE=1`,
  the default of `scripts/build_ps5.sh`) adds, all linked or embedded in
  `eboot.bin`:
  QuickJS-NG v0.17.0 (MIT), the release's `quickjs-amalgam.zip`, SHA-256
  `a0955463c74809173a253ff87365095e6972e8b171cfb6aa8a1e781adf35cfeb`, fetched
  by `standalone/ps5/fetch_quickjs.sh` and compiled unmodified;
  libcurl 8.18.0 (curl license) and libpsl 0.21.5 (MIT), the static archives of
  Pacbrew v0.39, on the OpenSSL 3.5.2 already linked;
  the Mozilla root certificates as extracted by curl.se on 2026-09-25
  (`cacert-2026-09-25.pem`, SHA-256
  `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`, fetched by
  `standalone/ps5/fetch_cacert.sh`); the certificate data is under MPL-2.0 and
  is embedded unchanged, so that file is its source form;
  YouTube.js 18.1.0 (MIT) and its dependencies inside the service bundle, with
  the pins of `companion/innertube/package-lock.json`.
  The license texts are in `native/licenses` and ship under
  `assets/prosperotube-notices`. The console support for libcurl follows
  ps5-native-app-boilerplate `examples/update-check/console_curl.c`
  (GPL-3.0-or-later, same author); adapted source: `standalone/ps5/tube_js_console.c`,
  `standalone/ps5/tube_js_libc.c`.
* Optional segment information comes from the SponsorBlock community database
  and API by Ajay Ramachandran and contributors: https://sponsor.ajay.app/.
  The API/database license is CC BY-NC-SA 4.0:
  https://github.com/ajayyy/SponsorBlock/wiki/Database-and-API-License.
  No database snapshot is bundled. The client filters fetched segment metadata
  into playback intervals; no automated submissions or votes are implemented.
* Optional DeArrow title/thumbnail metadata comes from Ajay Ramachandran and
  community contributors: https://dearrow.ajay.app/ and
  https://wiki.sponsor.ajay.app/w/API_Docs/DeArrow. The same CC BY-NC-SA 4.0
  API/database license applies. Titles are normalized for display and thumbnail
  timestamps request the community image service; originals remain available.
  No database snapshot, automated submissions or votes are bundled.

Matching source consists of this repository, recursive submodules, pinned
dependencies and build overlay. Keep these notices with binary distributions
and make the matching source revision available. Launcher art/fixtures are
original code-drawn/synthetic assets. SmartTube (MIT at the inspected revision
`2c60453007dfc3775ff525c3837c2d1d4e95aab9`) and MediaServiceCore are behaviour and
architectural references only, and FreeTube (AGPL-3.0-or-later) is the reference
for reading YouTube without an account through YouTube.js; their code/assets
are not distributed here, with one exception: the list of blocked live-chat
author names in `companion/innertube/live_chat.mjs` is taken from SmartTube's
`ChatController` at that revision (MIT, Copyright (c) 2020-present yuliskov).

Native microphone AudioIn ABI and routing are based on the GPL-3.0-or-later
[ps5-audio-decoding-research microphone proof of concept](https://github.com/blackbearreloaded/ps5-audio-decoding-research/tree/26be5a12f92b2cdc67f8a9e082fd69ae0e9a35a2/examples/native-audio-poc/microphone).
Implementation: `native/tube_microphone.c`.

Optional local speech recognition invokes whisper.cpp (MIT), v1.9.5 at
`d1be6fde11ac6e0407606b4e42fe72d34add8037`:
https://github.com/ggml-org/whisper.cpp. Its executable and model are installed
separately on the companion host; neither is bundled in the PS5 application.
The setup script obtains the multilingual tiny GGML model through upstream's
download script and checks SHA-256
`be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21`.
Keep upstream license files with any redistributed tool or model.

Channel-name comparison uses a generated simple-case mapping derived from
[UnicodeData 15.0.0](https://www.unicode.org/Public/15.0.0/ucd/UnicodeData.txt),
SHA-256 `806e9aed65037197f1ec85e12be6e8cd870fc5608b4de0fffd990f689f376a73`.
The table is generated by `scripts/generate_channel_name_case.py`; it applies
simple uppercase then simple lowercase per code point, without locale-specific
or multi-character expansions. Unicode data is covered by the Unicode License
V3, preserved at `native/licenses/Unicode-3.0.txt` and copied into native assets.

* The text engine (`native/text`) links HarfBuzz 9.0.0 ("Old MIT" licence) and
  GNU FriBidi 1.0.15 (LGPL-2.1-or-later), the static archives of Pacbrew v0.39,
  beside the FreeType 2.13.2 (FreeType Project License) already linked. The
  licence texts are in `native/text/licenses` and ship under
  `assets/prosperotube-notices`. FriBidi's source is Pacbrew's package source;
  this repository's sources and build recipe are what relinking the app
  against another build of it takes. The console font names and their order,
  and the way glyphs become distance fields, follow ProsperoEden's launcher
  and ProsperoRadio (`gfx/system_fonts.cpp`,
  GPL-3.0-or-later, same author); the code is written for this project. The
  sub-pixel placement of an edge from its coverage follows Gustavson and
  Strand, "Anti-aliased Euclidean distance transform" (2011); the distance
  transform is Felzenszwalb and Huttenlocher's. Host tests build the three
  libraries from the release archives pinned in `scripts/text_deps.sh`.
* The Afterglow interface (`native/afterglow`) is drawn with ps5-homebrew-ui
  (GPL-3.0-or-later, same author), fetched at the commit pinned in
  `scripts/ui_kit.py`; EVO's own kit screens are built from the same copy.
  It carries four font files from that kit's `third_party/fonts`: Inter
  Regular and SemiBold and Montserrat Medium (SIL Open Font License 1.1) and
  DejaVu Sans Mono (Bitstream Vera and DejaVu licences). Their licence texts
  ship under `assets/prosperotube-notices`.
* The Afterglow interface's sounds (`native/afterglow/sound/recordings`) are
  original procedural synthesis by this project, Copyright (C) 2026
  BlackBearReloaded, GPL-3.0-or-later. They contain no recordings, samples,
  downloaded audio or audio generated by an external service. Reproduce them
  with `scripts/generate_afterglow_sounds.py`; the manifest lists each file's
  hash, duration and levels. The package ships these assets under
  `assets/hui/afterglow/sfx/prosperotube`, with provenance in
  `assets/prosperotube-notices/afterglow-sounds.txt`. The bank and mixer remain
  from the pinned UI kit; its stock audio is not used by the Afterglow bank.
* Local speech recognition uses whisper.cpp 1.9.5 (commit
  `d1be6fde11ac6e0407606b4e42fe72d34add8037`) and its bundled GGML CPU code,
  under the MIT license. The multilingual OpenAI Whisper tiny model is also
  MIT licensed. Exact source/model URLs and hashes are in
  `native/maxd/voice/manifest.json`; both MIT notices are retained there and
  installed under `assets/voice`. The source archive retains the engine source,
  model, build recipe and app integration needed to rebuild this combination.
