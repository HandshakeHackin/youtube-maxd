#!/usr/bin/env bash
# Installs into an isolated user prefix; never overwrites /opt or another SDK.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-${HOME}/.cache/prosperotube/sdk}"
for command in git python3 llvm-config clang ld.lld wget unzip cmake make; do
    command -v "$command" >/dev/null || { echo "Missing host dependency: $command" >&2; exit 2; }
done
git -C "$root" submodule update --init --recursive
if [[ ! -f "$PS5_PAYLOAD_SDK/bin/prospero-clang" ]]; then
    PS5_SDK_VERSION=v0.42 bash "$root/third_party/evo/scripts/install-sdk-image.sh"
fi
if [[ ! -f "$PS5_PAYLOAD_SDK/EVO_PACBREW_VERSION" ]]; then
    PACBREW_VERSION=v0.39 bash "$root/third_party/evo/scripts/install-pacbrew-image.sh"
fi
echo "SDK ready at $PS5_PAYLOAD_SDK. Run bash scripts/build_ps5.sh."
