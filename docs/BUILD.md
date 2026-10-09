# Build and offline validation

Use Linux/WSL. Keep the native cache on its Linux filesystem; scripts default
to `$HOME/.cache/prosperotube`. Sources may be on `/mnt/c`.

```sh
git submodule update --init --recursive
sudo apt-get install clang-18 lld-18 llvm-18 cmake ninja-build make git \
  python3 python3-venv pkg-config ccache wget curl unzip zip nasm yasm \
  libssl-dev libfreetype-dev libzstd-dev libbz2-dev liblzma-dev ffmpeg
export PATH=/usr/lib/llvm-18/bin:$PATH
bash scripts/bootstrap_sdk.sh
bash scripts/build_ps5.sh --ffpfsc
```

LLVM 18.1.8 was used. Package names vary by distribution. The bootstrap fetches
release assets into an isolated user prefix. `PS5_PAYLOAD_SDK` selects an existing
SDK, but the build installs libraries into it; use a dedicated copy.
`PROSPEROTUBE_NATIVE_WORK` selects a dedicated generated work tree.

Pins: EVO `9715f6ee9bf9b53461d6fdf9a2e11b4c48357783`, UI
`4bd942579dd981b3df9c740438489ca6a614ddc0`, SDK v0.42, Pacbrew v0.39,
FFmpeg 7.1.1 minimal, dav1d 1.5.1, cJSON 1.7.19, MkPFS
`6cb8313dfe0c988ac52617794553f343243d3a56`. Upstream build scripts check
source archive checksums. SDK/Pacbrew assets are release-tag pinned, not
independently checksum-pinned by this wrapper.

`prepare_native.py` checks the EVO commit, extracts pinned sources, restores
every overlaid file before applying checked patches, and copies the original
provider/UI and complete cJSON. It never edits the submodule checkout.
`build_ps5.sh` uses EVO's native packaging pipeline, builds PS5 FFmpeg,
signs/inspects containers and copies artifacts to `dist/`. MkPFS verifies its
image. Network access is needed only for uncached dependencies. Scripts never
deploy or launch on PS5.

The pinned EVO app pipeline also includes its PS5 Videodec2 hardware backend;
the secondary high-resolution decoder is enabled by its packaging recipe.
FFmpeg supplies demuxing and the software fallback. Presence in a binary is
not hardware acceptance: the selected decoder, presentation, sound and timing
must be checked on the console. The current companion format policy selects
SDR AVC/VP9 and has not yet enabled the backend's wider HDR/HEVC capabilities.
See [reference candidates](REFERENCE_CANDIDATES.md) for source evidence.

Set `PROSPEROTUBE_PS5_DIST` to an isolated candidate directory when checking
new changes, preserving existing release files and their recorded manifests.
`PROSPEROTUBE_NATIVE_WORK` separately isolates the generated build tree.
These options do not deploy a candidate or change the console target.
Optional account library setup is documented in [library service](LIBRARY_SERVICE.md).

For bounded console acceptance, `build_ps5.sh --usb-remote` enables EVO's
existing development command/status/UI instrumentation. The overlay scopes its
files to `dev_command`, `dev_status` and `dev_ui.json`, and the diagnostic log
to `console.log`, under `/data/prosperotube`. Startup requests filesystem access
through Lapy and proves writable app storage before initializing services.
The packaged one-shot helper targets only `PPSA99142`; a resident Lapy service
gets the first bounded opportunity. Failed elevation stops startup rather than
loading or saving transient state. Lapy's handshake markers still use
`/download0`, as required by its protocol. The normal build leaves
development commands disabled. Use the title-aware protocol tooling for
deployment/launch/close; upstream EVO helper scripts still target EVO's own
title and shared USB filenames and must not be invoked against ProsperoTube.
Add EVO's `--virtual-keyboard` option for scripted app text-entry cases; the
development input commands cannot type into the PS5 system IME.

`package_release.py` packages companion/documentation bytes from the committed
Git revision and only named generated media fixtures. Local account files,
dependency directories and generated library bundles are excluded. Install the
pinned library dependencies and rebuild its bundle on the companion host.
Packaging records artifact hashes; test evidence belongs in the validation
record and is not inferred from running the packager.

## Checks

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r companion/requirements.lock
PYTHONPATH=companion .venv/bin/python -m pytest -q
bash scripts/test_native.sh
bash scripts/test_elevation.sh
bash scripts/test_subtitles.sh
python3 scripts/generate_fixtures.py
PYTHONPATH=companion .venv/bin/python scripts/probe_companion.py
bash scripts/preview_native.sh
```

The native provider runs with AddressSanitizer/UndefinedBehaviorSanitizer and
mocked HTTP. Tests cover isolation, persistence, stable identities, pairing,
jobs/cancellation, origin checks, chapters and SponsorBlock.
The media probe runs the real HTTP API/relay against generated AVC/AAC, VP9/Opus
and HLS, then decodes and seeks with host FFmpeg and checks caption timing.
This tests host FFmpeg, not the PS5 build. The UI preview builds pinned RmlUi 6.2
and renders the shipping interface with EVO's CPU renderer, checking card bounds,
focus and selection. PNGs: `output/preview`; media evidence: `output/validation`.
Fixtures and outputs are generated, not tracked.

## Offline console fixtures

After generating media:

```sh
PYTHONPATH=companion .venv/bin/python -m prosperotube \
  --host 0.0.0.0 --public-base http://YOUR-PC-LAN-IP:8765 \
  --fixtures fixtures/generated
```

Pair and search any term. Three labelled synthetic videos exercise AVC/AAC DASH,
VP9/Opus DASH and finite HLS. Thumbnails/media are local. This mode does not
contact YouTube or SponsorBlock and does not establish service compatibility.
