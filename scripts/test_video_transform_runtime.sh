#!/usr/bin/env bash
# Compile a staged copy only. This does not activate new shaders or controls.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
evo="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}/projects/evoplayer"
work="$root/build/tests/video-transform/runtime"
mkdir -p "$work/media/src"
PYTHONPATH="$root/scripts" python3 - "$evo" "$work" <<'PY'
from pathlib import Path
import shutil,sys
from video_transform_overlay import apply_video_transform_overlay
evo,work=map(Path,sys.argv[1:])
relative='media/src/evo_agc_runtime.c'
shutil.copyfile(evo/relative,work/relative)
shutil.copyfile(evo/'SDL_ps5tilemap.inc',work/'SDL_ps5tilemap.inc')
def replace(path,old,new,count=1):
    target=work/path
    text=target.read_text()
    assert text.count(old)==count,(path,old)
    target.write_text(text.replace(old,new))
apply_video_transform_overlay(replace,'')
PY
sdk="${PS5_PAYLOAD_SDK:-${HOME}/.cache/prosperotube/sdk}"
export PS5_PAYLOAD_SDK="$sdk"
cd "$evo"
"$sdk/bin/prospero-clang" -Wall -g -O2 -DEVO_TARGET_PS5=1 \
  -I"$root/native" -Icore/include -Iinclude -Ipp/include -Iui/include -Iui_rml/include \
  -Imedia/include -Iaddons/include -I"$sdk/target/user/homebrew/include/SDL2" \
  -I"$sdk/target/user/homebrew/include" -DPP_4K_PRODUCT_STAGE=4 -DEVO_AGC_DEVICE=1 \
  -DEVO_AGC_HAVE_VIDEO_PIPES=1 -Ishaders/agc -c \
  -o "$work/runtime.o" "$work/media/src/evo_agc_runtime.c"
echo 'PASS: staged rotation/mirror AGC runtime compiles with the PS5 SDK'
