#!/usr/bin/env bash
# Native Linux/WSL build of EVO's pinned PS5 shader compiler; no console access.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${PROSPEROTUBE_SHADER_TOOLS:-${HOME}/.cache/prosperotube/shader-tools}"
mkdir -p "$cache"
exec 9>"$cache/build.lock"
flock -n 9 || { echo "Shader-toolchain build already running" >&2; exit 1; }
jobs="${PROSPEROTUBE_SHADER_JOBS:-2}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo "Invalid job count" >&2; exit 1; }
# AMDVLK v-2025.Q2.1 default.xml + build_with_tools.xml.
while read -r path repo revision; do
  checkout="$cache/src/$path"
  if [[ ! -d "$checkout/.git" ]]; then
    mkdir -p "$checkout"
    git -C "$checkout" init -q
    git -C "$checkout" remote add origin "https://github.com/$repo.git"
  fi
  if ! git -C "$checkout" cat-file -e "$revision^{commit}" 2>/dev/null; then
    git -C "$checkout" fetch --depth=1 origin "$revision"
  fi
  current="$(git -C "$checkout" rev-parse --verify HEAD 2>/dev/null || true)"
  if [[ "$current" != "$revision" ]]; then
    git -C "$checkout" checkout --detach "$revision"
  fi
  if [[ -f "$checkout/.gitmodules" ]]; then
    git -C "$checkout" submodule update --init --recursive --depth=1
  fi
done <<'REPOS'
drivers/xgl GPUOpen-Drivers/xgl e9782eb33ce5e5e4ed2e339542a28c1b933624b4
drivers/pal GPUOpen-Drivers/pal c5e800072a32f68b6ccc4422936d96167c6e0728
drivers/llpc GPUOpen-Drivers/llpc 40cb8d95ad8d6f7f1652e3fd47d39667594cce08
drivers/gpurt GPUOpen-Drivers/gpurt 7b226d48b46b7e92fec3b9ecc5712e5bf2bf3dd9
drivers/llvm-project GPUOpen-Drivers/llvm-project 8fd93e26cf9b1235fc9573b68b96233818be0ed4
drivers/third_party/metrohash GPUOpen-Drivers/MetroHash 6ab6ee5d31d001ba73feb5e7f13dbc75da96b620
drivers/third_party/cwpack GPUOpen-Drivers/CWPack 73d612971b3a73341f241b021e5cbda220eef7f2
drivers/spvgen GPUOpen-Drivers/spvgen ba3bda984defc22aadb20f3e1fdfaf23972cf636
drivers/third_party/glslang KhronosGroup/glslang 1b65bd602b23d401d1c4c86dfa90a36a52c66294
drivers/third_party/SPIRV-tools KhronosGroup/SPIRV-Tools ce37fd67f83cd1e8793b988d2e4126bbf72b19dd
drivers/third_party/SPIRV-tools/external/spirv-headers KhronosGroup/SPIRV-Headers e7294a8ebed84f8c5bd3686c68dbe12a4e65b644
drivers/third_party/SPIRV-cross KhronosGroup/SPIRV-Cross 6173e24b31f09a0c3217103a130e74c4ddec14a6
REPOS
python3 "$root/third_party/evo/tools/patches/llpc-add-gfx1013.py" "$cache/src/drivers/llpc/lgc/state/TargetInfo.cpp"
dxc="$cache/linux_dxc_2026_07_29.x86_x64.tar.gz"
if [[ ! -f "$dxc" ]]; then
  curl -fL --retry 3 -o "$dxc.part" https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/linux_dxc_2026_07_29.x86_x64.tar.gz
  mv "$dxc.part" "$dxc"
fi
printf '%s  %s\n' 55665c87824051ed4774ff3280a79ccbbb7d39243b9736ca5e98222134112d54 "$dxc" | sha256sum -c -
mkdir -p "$cache/dxc"
dxc_root="$cache/dxc/linux_dxc_2026_07_29.x86_x64"
if [[ ! -x "$dxc_root/bin/dxc" ]]; then tar -xzf "$dxc" -C "$cache/dxc"; fi
export PATH="$dxc_root/bin:$PATH"
export LD_LIBRARY_PATH="$dxc_root/lib:${LD_LIBRARY_PATH:-}"
cmake -G Ninja -S "$cache/src/drivers/xgl" -B "$cache/build" \
  -DCMAKE_BUILD_TYPE=Release -DDXC_PATH="$dxc_root/bin/dxc" -DVKI_BUILD_TOOLS=ON -DVKI_RAY_TRACING=OFF -DLLPC_RAY_TRACING=OFF \
  -DLLVM_PARALLEL_COMPILE_JOBS="$jobs" -DLLVM_PARALLEL_LINK_JOBS=1
cmake --build "$cache/build" --target amdllpc -j"$jobs"
compiler="$(find "$cache/build" -type f -name amdllpc -executable -print -quit)"
[[ -n "$compiler" ]] || { echo "No compiled amdllpc" >&2; exit 1; }
spvgen="$(find "$cache/build" -type f -name 'libspvgen.so*' -print -quit)"
[[ -n "$spvgen" ]] || { echo "No compiled spvgen" >&2; exit 1; }
mkdir -p "$cache/bin" "$cache/lib"
install -m755 "$compiler" "$cache/bin/amdllpc"
install -m755 "$spvgen" "$cache/lib/libspvgen.so"
export LD_LIBRARY_PATH="$cache/lib:$LD_LIBRARY_PATH"
"$cache/bin/amdllpc" --version
# Exercise the actual PS5 target and metadata/header packaging, not --version alone.
mkdir -p "$cache/probe-source/tools" "$cache/probe-source/projects/evoplayer/shaders/agc"
cp "$root/third_party/evo/tools/build_agc_pipes.py" "$cache/probe-source/tools/"
cp "$root/third_party/evo/projects/evoplayer/shaders/agc/video_yuv_nv12.pipe" "$cache/probe-source/projects/evoplayer/shaders/agc/"
python3 "$cache/probe-source/tools/build_agc_pipes.py" \
  --amdllpc "$cache/bin/amdllpc" --output-dir "$cache/probe" \
  "$root/third_party/evo/projects/evoplayer/shaders/agc/video_yuv_nv12.pipe"
printf 'PS5 shader compiler validated: %s\n' "$cache/bin/amdllpc"
