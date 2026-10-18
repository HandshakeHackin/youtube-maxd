#!/usr/bin/env bash
# Fetch project dependencies without installing system packages or touching a console.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="/usr/lib/llvm-18/bin:$PATH"
for command in git python3 clang llvm-config ld.lld llvm-readelf-18 llvm-objcopy-18 curl wget tar xz unzip cmake ninja make meson nasm pkg-config flock; do
    command -v "$command" >/dev/null || { echo "Missing $command. On Ubuntu 24.04 run: make host-deps" >&2; exit 2; }
done
python3 -c 'import sys; assert sys.version_info >= (3, 11), "Python 3.11 or newer is required"'
[[ "$(uname -s)/$(uname -m)" == Linux/x86_64 ]] || { echo 'The PS5 toolchain requires x86_64 Linux (or WSL).' >&2; exit 2; }
mkdir -p "$root/.deps"
exec 8>"$root/.deps/bootstrap.lock"
flock -n 8 || { echo 'Another dependency setup is running.' >&2; exit 2; }
node_home="${PROSPEROTUBE_NODE_HOME:-$HOME/.cache/prosperotube/node/node-v24.10.0-linux-x64}"
if [[ ! -x "$node_home/bin/node" ]] || [[ "$("$node_home/bin/node" --version)" != v24.10.0 ]]; then
    mkdir -p "$(dirname "$node_home")"
    temporary=$(mktemp -d "$(dirname "$node_home")/.node-XXXXXX")
    trap 'rm -rf -- "$temporary"' EXIT
    curl -fL --retry 3 https://nodejs.org/dist/v24.10.0/node-v24.10.0-linux-x64.tar.xz -o "$temporary/node.tar.xz"
    printf '%s  %s\n' 2642f4428869aca32443660fd71b3918e2be1277a899bdcaeb64c93b54b5af17 "$temporary/node.tar.xz" | sha256sum -c -
    tar -xJf "$temporary/node.tar.xz" -C "$temporary"
    mkdir -p "$node_home"
    cp -a "$temporary/node-v24.10.0-linux-x64/." "$node_home/"
    rm -rf -- "$temporary"
    trap - EXIT
fi
export PATH="$node_home/bin:$PATH"
bash "$root/scripts/bootstrap_sdk.sh"
# npm ci reconciles the lockfile even when node_modules already exists.
(cd "$root/companion/innertube" && npm ci --ignore-scripts)
# Rebuild the bundle after dependency installation, even when source mtimes match.
bash "$root/standalone/build_bundle.sh"
tools="${PROSPEROTUBE_SHADER_TOOLS:-$HOME/.cache/prosperotube/shader-tools}"
recipe=$(sha256sum "$root/scripts/build_ps5_shader_compiler.sh" | cut -d ' ' -f 1)
if [[ "${MAXD_ENABLE_DISPLAY_TRANSFORMS:-0}" != 1 ]]; then
    printf 'Using pinned EVO video shaders; optional rotation/mirroring is disabled.\n'
elif [[ -f "$root/native/maxd/shader-artifact/manifest.json" && "${MAXD_REBUILD_SHADERS:-0}" != 1 ]]; then
    python3 - "$root/scripts" "$root/native/maxd/shader-artifact" <<'PY'
import sys
sys.path.insert(0, sys.argv[1])
from install_video_transform_shaders import validate
validate(sys.argv[2])
PY
elif [[ ! -x "$tools/bin/amdllpc" || ! -f "$tools/lib/libspvgen.so" || ! -f "$tools/.prosperotube-recipe" ]] ||
   [[ "$(cat "$tools/.prosperotube-recipe" 2>/dev/null)" != "$recipe" ]]; then
    bash "$root/scripts/build_ps5_shader_compiler.sh"
fi
printf 'Build dependencies ready.\n'
