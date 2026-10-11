"""Exercise real native preparation without installing synthetic GPU code."""
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
WORK=Path.home()/".cache/prosperotube/native-transform-prepare-test"
env=dict(os.environ,PROSPEROTUBE_NATIVE_WORK=str(WORK))
env.pop("PROSPEROTUBE_TRANSFORM_ARTIFACT",None)

def prepare(environment):
    result=subprocess.run(["python3",str(ROOT/"scripts/prepare_native.py")],
                          env=environment,text=True,capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr)
    assert result.stdout.strip()==str(WORK)

prepare(env)
header=WORK/"projects/evoplayer/shaders/agc/video_yuv_nv12_pipe.h"
stamp=WORK/".prosperotube-video-transform.json"
header.write_text("invalid layout fixture")
stamp.write_text("old activation fixture")
rejected=subprocess.run(["python3",str(ROOT/"scripts/prepare_native.py")],
    env=dict(env,PROSPEROTUBE_TRANSFORM_ARTIFACT=str(WORK/"missing-artifact")),
    text=True,capture_output=True)
assert rejected.returncode and "missing-artifact" in rejected.stderr
assert header.read_text()=="invalid layout fixture"
assert stamp.read_text()=="old activation fixture"
prepare(env)
pin=(WORK/".prosperotube-source").read_text().strip()
original=subprocess.check_output(["git","-C",str(ROOT/"third_party/evo"),"show",
    pin+":projects/evoplayer/shaders/agc/video_yuv_nv12_pipe.h"])
assert header.read_bytes()==original
assert not stamp.exists()
runtime=(WORK/"projects/evoplayer/media/src/evo_agc_runtime.c").read_text()
assert "g_tube_frame_transform" not in runtime
assert "const size_t cons_bytes = params ? 32u : 16u;" in runtime
print("PASS: real preparation rejects missing artifacts before mutation and restores the original shader/runtime ABI")
