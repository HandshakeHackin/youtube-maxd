"""Check ProsperoTube's default and EVO's supported video cadences."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLAYER = ROOT / "third_party/evo/projects/evoplayer"


def main():
    sources = {
        "core/include/evo/services/SettingsService.hpp": (PLAYER / "core/include/evo/services/SettingsService.hpp").read_text(),
        "media/src/evo_agc_runtime.c": (PLAYER / "media/src/evo_agc_runtime.c").read_text(),
    }

    def replace(path, old, new, count=1):
        source = sources[path.removeprefix("projects/evoplayer/")]
        assert source.count(old) == count, (path, old)
        sources[path.removeprefix("projects/evoplayer/")] = source.replace(old, new)

    import sys
    sys.path.insert(0, str(ROOT / "scripts"))
    from auto_frame_rate_overlay import apply_auto_frame_rate_overlay
    apply_auto_frame_rate_overlay(replace, "projects/evoplayer/")

    settings = sources["core/include/evo/services/SettingsService.hpp"]
    runtime = sources["media/src/evo_agc_runtime.c"]
    assert "m_refreshRateMode = RefreshRateMode::PlaybackOnly;" in settings
    assert "g_agc_dev.supports_120hz = (sup120 > 0) ? 1 : 0;" in runtime

    app = (PLAYER / "core/src/Application.cpp").read_text()
    start = app.index("const double k = 119.88 / fps;")
    end = app.index("\n", app.index("want = kr >= 1.0", start))
    decision = app[start:end]
    program = f"""
#include <cassert>
#include <cmath>
#include <initializer_list>
bool wants_120(double fps) {{
    bool want = false;
    if (fps > 1.0) {{ {decision} }}
    return want;
}}
int main() {{
    for (double fps : {{23.976, 24.0, 29.97, 30.0, 59.94, 60.0}})
        assert(wants_120(fps));
    for (double fps : {{0.0, 1.0, 25.0, 50.0}})
        assert(!wants_120(fps));
}}
"""
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        source.write_text(program)
        subprocess.run(["g++", "-std=c++17", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("automatic frame-rate policy: pass")


if __name__ == "__main__":
    main()
