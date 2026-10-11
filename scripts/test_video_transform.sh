#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$root/build/tests/video-transform"
mkdir -p "$work"
python3 "$root/scripts/prepare_video_transform_shaders.py" "$work/pipes" > "$work/generate.log"
PYTHONPATH="$root/scripts" python3 - "$work" <<'PY'
from pathlib import Path
import sys
from prepare_video_transform_shaders import TRANSFORM, EVO
out=Path(sys.argv[1])
(out/'transform.inc').write_text(TRANSFORM)
# Each family has one shared VS. Verify all generated pipes use exactly these
# three variants, and validate their actual emitted GLSL and descriptor offsets.
sources={}
for pipe in (out/'pipes').glob('*.pipe'):
    text=pipe.read_text()
    original=(EVO/'projects/evoplayer/shaders/agc'/pipe.name).read_text()
    assert text.split('[FsGlsl]',1)[1]==original.split('[FsGlsl]',1)[1], pipe.name
    vs=text.split('[VsGlsl]\n',1)[1].split('[VsInfo]',1)[0].strip()
    family=pipe.name.split('_',1)[0]
    if family in sources: assert sources[family]==vs
    sources[family]=vs
assert set(sources)=={'video','upscale','interp'}
for family,vs in sources.items(): (out/(family+'.vert')).write_text(vs)
PY
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$root/native" -I"$work" "$root/tests/test_video_transform.cpp" -o "$work/test"
ASAN_OPTIONS=detect_leaks=1 "$work/test"
for family in video upscale interp; do
  glslangValidator -V --target-env vulkan1.2 -o "$work/$family.spv" "$work/$family.vert"
  spirv-val --target-env vulkan1.2 "$work/$family.spv"
  spirv-dis "$work/$family.spv" -o "$work/$family.spvasm"
done
python3 - "$work" <<'PY'
from pathlib import Path
import re,sys
work=Path(sys.argv[1])
for family,uniform,offsets in [('video','VideoConstants',[0,8,16,20,24,28]),
                              ('upscale','PassConstants',[0,16,32]),
                              ('interp','PassConstants',[0,16,32])]:
    assembly=(work/(family+'.spvasm')).read_text()
    for member,offset in enumerate(offsets):
        assert re.search(rf'OpMemberDecorate %{uniform} {member} Offset {offset}\b',assembly)
print('PASS: all staged vertex families compile; Vulkan validation and uniform ABI offsets match')
PY
