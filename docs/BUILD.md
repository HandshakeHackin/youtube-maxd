# Building YouTube Max’d

Use x86-64 Ubuntu 24.04 or WSL, keeping build caches on the Linux filesystem.
The full native media build needs several GiB of RAM and disk space. No
console connection is used by the build or packaging commands.

```sh
make host-deps  # Explicit Ubuntu apt/sudo prerequisites, once.
# Source ZIP only: restore dependency Git metadata without replacing files.
python3 scripts/setup_source_checkout.py
make deps
make zip
```

A Git clone with its recursive submodules already initialized can skip
`setup_source_checkout.py`. Network access is needed for uncached dependencies.
Dependencies are pinned in the build scripts and `UPSTREAM.json`. Source
archives carry matching native-library inputs in `runtime-sources/`.

The default build uses EVO’s existing pinned video shaders, verified against
81 source headers. Rotation/mirroring controls are hidden and imported
transform bits are ignored. `MAXD_ENABLE_DISPLAY_TRANSFORMS=1 make zip` opts
into the separate shader compiler and artifact path, which may take hours to
build and requires its own console validation.

The Makefile defaults to project-local caches:

| Variable | Default |
| --- | --- |
| `PS5_PAYLOAD_SDK` | `.deps/sdk` |
| `PROSPEROTUBE_NATIVE_WORK` | `.deps/native-work` |
| `PROSPEROTUBE_NODE_HOME` | `.deps/node/node-v24.10.0-linux-x64` |
| `PROSPEROTUBE_SHADER_TOOLS` | `.deps/shader-tools` |
| `TUBE_TEXT_CACHE` | `.deps/text-deps` |
| `PROSPEROTUBE_PS5_DIST` | `dist` |

Use a dedicated SDK: the build installs its patched FFmpeg/dav1d into that
sysroot. SDK v0.42 and PacBrew v0.39 use the public PS5 homebrew toolchain.
The integrated service uses QuickJS-NG 0.17.0 and YouTube.js 18.1.0. Its HTTP
endpoint is authenticated and bound to loopback inside the PS5 process.

`make zip` produces `dist/PPSA99991/` and `dist/PPSA99991.zip`, checks app
identity and executable containers, and sets the homebrew ZIP modes to 0777.
The packaged Lapy helper is compiled and verified for PPSA99991. Native
startup requires a compatible console-side ELF loader on TCP 9021 or an
already running compatible Lapy service. No PC companion is required.

To create the named binary and corresponding source distribution:

```sh
python3 scripts/collect_maxd_sources.py
python3 scripts/package_maxd_release.py
```

The packager checks metadata, artwork, shader provenance and source hashes,
excludes credentials/build caches, and writes `YouTube-Maxd-PS5.zip`,
`YouTube-Maxd-source.zip`, `YouTube-Maxd-icon.png` and `SHA256SUMS` in `dist`.
Do not distribute the executable without its corresponding source and notices.

Useful offline checks (some fixture suites need local loopback sockets):

```sh
bash scripts/test_maxd.sh
npm test --prefix companion/innertube
bash standalone/test_host.sh
bash scripts/test_native.sh maxd_quality stream_choice profile_settings codecs sponsor
bash scripts/test_device_auth_native.sh
bash scripts/test_sponsor_player.sh
bash scripts/test_afterglow.sh
```

`make test` runs the broader inherited suite after preparing its host Python
virtual environment. Restricted runners may set `ASAN_OPTIONS=detect_leaks=0`
while keeping address/undefined-behavior sanitizers enabled. See
[MAXD_VALIDATION.md](MAXD_VALIDATION.md) for completed checks and console gaps.

The detailed original upstream build notes are preserved in
[UPSTREAM_BUILD.md](UPSTREAM_BUILD.md). Their title IDs, release automation,
cache defaults and shader policy describe the upstream project.

## First-preview distribution

The existing `v1.2.0-preview.1` app is already compiled. To assemble its
Nuvio-style distribution from the verified files in `dist/`, run:

```sh
python3 scripts/assemble_maxd_preview.py
```

The output is `dist/release-v1.2.0-preview.1/`: `PPSA99991.zip`, matching full
source, icon, release/testing notes, the static catalog scan, validation JSON
and `SHA256SUMS`. The title-ID ZIP is byte-identical to `YouTube-Maxd-PS5.zip`.
The assembler verifies the original app hash and each source-manifest entry;
it does not rebuild the app or rerun the recorded development tests.

The manually invoked **Publish Max'd preview assets** GitHub Actions workflow
can attach these files to the existing preview. It checks out the original app
source, restores exact dependency revisions from an accessible public mirror,
packages matching source and reuses the pinned app ZIP from `downloads`. It
does not change repository visibility or submit a store listing. Existing
assets with conflicting digests are never replaced.

The original UI-kit URL is currently unavailable. A fork containing both
exact pinned revisions is `https://github.com/LedyBacer/ps5-homebrew-ui.git`.
The README's clone command applies a URL override for recursive initialization;
set `PROSPEROTUBE_UI_KIT` to that mirror or a local clone containing
`bbe19054ee2e7ec069254775273aeb8846c4bd3e`. Pins and file-hash checks remain
unchanged. A fresh rebuild still needs every other pinned input or its cache;
the complete source distribution retains the matching source inputs.

See [store preparation](../store/README.md) for catalog requirements and the
prepared reviewer description. GitHub branch downloads do not substitute for
an attached release asset in a store submission.

## YouTube Max’d local voice inputs

The standalone `scripts/build_ps5.sh` also fetches checksum-pinned whisper.cpp
1.9.5 and the multilingual Whisper tiny model, builds the CPU-only static
recognizer with the public SDK, and includes the model as a regular asset.
This adds about 74 MiB of model data. Voice setup and host tests are documented
in [VOICE_SEARCH.md](VOICE_SEARCH.md). `TUBE_EMBEDDED_SERVICE=0` keeps the optional
companion-only build and omits the local recognizer/model.
