#!/usr/bin/env python3
"""Check the generated fault trace covers the GPU resources it diagnoses."""
from pathlib import Path
import os

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))
player = work / "projects/evoplayer"
runtime = (player / "media/src/evo_agc_runtime.c").read_text()
direct = (player / "media/src/evo_direct_mem.c").read_text()
rml = (player / "ui_rml/src/evo_rmlui_render_agc.cpp").read_text()

assert 'if (coded_w >= 2880 && coded_h >= 2160)' in runtime
assert 'static int trace_4k_descriptors = 2;' in runtime
assert 'gpuaddr video slot=' in runtime and 'gpuaddr tsharp Y=' in runtime
assert 'g_agc_dev.scanout_buffers[0], g_agc_dev.scanout_buffers[1]' in runtime
assert 'gpuaddr direct_pool=' in direct
assert 'gpuaddr direct_fallback_alloc=' in direct
assert 'gpuaddr direct_fallback_free=' in direct
assert 'gpuaddr ui_draw tex=' in rml
assert 'gpuaddr ui_geom_free' in rml and 'gpuaddr ui_tex_free' in rml
assert 'static int trace_4k_ui_draws = 12;' in rml
print("generated 4K GPU address trace: PASS")
