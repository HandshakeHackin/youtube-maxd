#!/usr/bin/env bash
# Optional local speech recognition for the companion, never a PS4 build.
set -euo pipefail
destination="${1:-${HOME}/.cache/prosperotube/whisper-cpp}"
revision=d1be6fde11ac6e0407606b4e42fe72d34add8037
model_hash=be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21
if [[ ! -d "${destination}/.git" ]]; then
    git clone --depth 1 --branch v1.9.5 https://github.com/ggml-org/whisper.cpp.git "${destination}"
fi
if [[ "$(git -C "${destination}" rev-parse HEAD)" != "${revision}" ]]; then
    echo 'Voice source revision differs. Use an empty destination or the pinned source.' >&2
    exit 2
fi
cmake -S "${destination}" -B "${destination}/build" -DCMAKE_BUILD_TYPE=Release \
    -DGGML_CUDA=OFF -DGGML_VULKAN=OFF -DWHISPER_BUILD_TESTS=OFF
cmake --build "${destination}/build" --target whisper-cli -j4
if [[ ! -f "${destination}/models/ggml-tiny.bin" ]]; then
    (cd "${destination}" && bash models/download-ggml-model.sh tiny)
fi
printf '%s  %s\n' "${model_hash}" "${destination}/models/ggml-tiny.bin" | sha256sum --check --quiet
printf '\nCompanion voice arguments:\n  --voice-cli "%s/build/bin/whisper-cli" --voice-model "%s/models/ggml-tiny.bin"\n' "${destination}" "${destination}"
