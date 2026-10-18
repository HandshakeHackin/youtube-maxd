#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
voice="$root/.deps/voice"
python3 "$root/scripts/bootstrap_local_voice.py"
python3 "$root/scripts/build_local_voice.py"
revision=d1be6fde11ac6e0407606b4e42fe72d34add8037
mkdir -p "$root/build/tests"
python3 - "$voice/bad-model.bin" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])
with p.open('wb') as f: f.truncate(77691713)
PY
archives=()
for name in libwhisper.a libggml.a libggml-cpu.a libggml-base.a; do
    mapfile -t found < <(find "$voice/host" -name "$name")
    [[ ${#found[@]} == 1 ]]
    archives+=("${found[0]}")
done
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$root/native/maxd" -I"$root/native" -I"$voice/include" -I"$voice/whisper.cpp-$revision/include" \
    -I"$voice/whisper.cpp-$revision/ggml/include" \
    "$root/native/maxd/local_asr.cpp" "$root/tests/test_local_asr.cpp" \
    -Wl,--start-group "${archives[@]}" -Wl,--end-group -lcrypto -pthread -lm -ldl \
    -o "$root/build/tests/test-local-asr"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$root/build/tests/test-local-asr" \
    "$root/native/maxd/voice/ggml-tiny.bin" "$voice/whisper.cpp-$revision/samples/jfk.wav" "$voice/bad-model.bin"
rm -f "$voice/bad-model.bin"
