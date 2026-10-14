"""Exercise the generated application's no-swap AGC frame boundary on host."""
from pathlib import Path
import os
import subprocess
import tempfile

from agc_frame_boundary_overlay import apply

root = Path(__file__).resolve().parents[1]
source = (root / "third_party/evo/projects/evoplayer/core/src/Application.cpp").read_text()


def replace(path, old, new, count=1):
    global source
    assert path == "projects/evoplayer/core/src/Application.cpp"
    assert source.count(old) == count
    source = source.replace(old, new)


apply(replace, "projects/evoplayer/")
start = source.index("        if (!swap && uiActive && !isPlayer) {")
end = source.index("        if (!swap) {", start)
boundary = source[start:end]

code = r'''
#include <cassert>
static int opens, closes, flips, frame_active, draws;
void evo_agc_runtime_frame_begin() { if (!frame_active) { ++opens; frame_active = 1; } }
void evo_agc_runtime_frame_end() {
    if (!frame_active) return;
    ++closes;
    if (draws) ++flips;
    frame_active = 0;
    draws = 0;
}
void evo_rmlui_end_frame() {}
void finish(bool swap, bool uiActive, bool isPlayer) {
''' + boundary + r'''
}
int main() {
    // An active provider page can keep the render loop awake without drawing.
    for (int i = 0; i < 1000; ++i) {
        evo_agc_runtime_frame_begin();
        finish(false, true, false);
    }
    assert(opens == 1000 && closes == 1000 && flips == 0 && !frame_active);
    // A draw that escapes the swap signal must still submit at this boundary.
    evo_agc_runtime_frame_begin(); draws = 1;
    finish(false, true, false);
    assert(opens == 1001 && closes == 1001 && flips == 1 && !frame_active);
    // The ordinary present path owns swapped frames; video owns player frames.
    evo_agc_runtime_frame_begin();
    finish(true, true, false);
    assert(frame_active && closes == 1001);
    evo_agc_runtime_frame_end();
    evo_agc_runtime_frame_begin();
    finish(false, true, true);
    assert(frame_active && closes == 1002);
    evo_agc_runtime_frame_end();
}
'''
with tempfile.TemporaryDirectory(prefix="tube-agc-boundary-") as folder:
    binary = Path(folder) / "test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-x", "c++", "-",
                    "-o", str(binary)], input=code, text=True, check=True)
    subprocess.run([str(binary)], check=True)
print("AGC no-swap frame boundary: 1000 empty frames abort; missed draw submits; player/swapped paths untouched")
