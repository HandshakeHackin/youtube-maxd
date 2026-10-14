#!/usr/bin/env python3
"""Exercise generated UI direct-buffer deferral across late GPU retirement."""
from pathlib import Path
import os
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))
player = work / "projects/evoplayer"
runtime = (player / "media/src/evo_agc_runtime.c").read_text()
rml = (player / "ui_rml/src/evo_rmlui_render_agc.cpp").read_text()
hui = (player / "ui_kit/src/hui_agc_batch.cpp").read_text()
hui_app = (player / "ui_kit/src/evo_hui_app.cpp").read_text()


def function(source, signature):
    start = source.index(signature)
    pos = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]


idle = function(runtime, "int evo_agc_runtime_gpu_data_idle(void)")
helper_start = rml.index("static std::vector<void *> s_agc_pending_frees;")
helper_end = rml.index("EvoRenderInterfaceAGC::EvoRenderInterfaceAGC", helper_start)
helpers = rml[helper_start:helper_end]
hui_end = function(hui, "void AgcBatch::end_frame()")
hui_release = function(hui, "void AgcBatch::release()")
hui_shutdown = function(hui_app, "void evo_hui_shutdown(void)")
assert "evo_agc_runtime_gpu_data_idle" not in hui_release  # init failure can clean up
assert "if (evo_agc_runtime_gpu_data_idle()) k.batch.release();" in hui_shutdown
assert rml.count("agc_alloc_gpu(") == 4
assert rml.count("agc_release_buffer(") == 6  # helper plus five release sites
assert "agc_collect_released_buffers();" in function(rml, "void EvoRenderInterfaceAGC::FrameEnd()")
assert "m_white_texture.release()" in function(rml, "EvoRenderInterfaceAGC::~EvoRenderInterfaceAGC()")

c_harness = r'''#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#define EVO_AGC_FRAME_SLOTS 2
static uint32_t fences[2];
static struct {
    int initialized, frame_active;
    uint32_t *fences[2];
    uint32_t fence_expect[2];
} g_agc_dev = { .fences = {&fences[0], &fences[1]} };
static void evo_agc_runtime_cache_flush(const void *ptr, size_t bytes)
{ assert(ptr && bytes == 4); }
''' + idle + r'''
int main(void) {
    assert(evo_agc_runtime_gpu_data_idle());
    g_agc_dev.initialized = 1;
    g_agc_dev.frame_active = 1;
    assert(!evo_agc_runtime_gpu_data_idle());
    g_agc_dev.frame_active = 0;
    g_agc_dev.fence_expect[0] = 7;
    assert(!evo_agc_runtime_gpu_data_idle());
    fences[0] = 7;
    assert(evo_agc_runtime_gpu_data_idle());
    g_agc_dev.fence_expect[1] = 9;
    assert(!evo_agc_runtime_gpu_data_idle());
    fences[1] = 9;
    assert(evo_agc_runtime_gpu_data_idle());
}
'''

cpp_harness = r'''#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <vector>
static bool idle;
static int freed;
static int evo_agc_runtime_gpu_data_idle() { return idle; }
static void *evo_direct_mem_alloc_gpu(size_t bytes) { return malloc(bytes); }
static void evo_direct_mem_free(void *ptr) { assert(ptr); ++freed; free(ptr); }
''' + helpers + r'''
int main() {
    idle = false;
    void *first = agc_alloc_gpu(64);
    void *second = agc_alloc_gpu(64);
    agc_release_buffer(first);
    agc_release_buffer(second);
    agc_release_buffer(nullptr);
    assert(freed == 0 && s_agc_pending_frees.size() == 2);
    idle = true;
    void *third = agc_alloc_gpu(64); // late fence retires before reuse
    assert(freed == 2 && s_agc_pending_frees.empty());
    agc_release_buffer(third);
    assert(freed == 3 && s_agc_pending_frees.empty());
    idle = false;
    void *fourth = agc_alloc_gpu(64);
    agc_release_buffer(fourth);
    assert(freed == 3 && s_agc_pending_frees.size() == 1);
    idle = true;
    agc_collect_released_buffers();
    agc_collect_released_buffers(); // no double free
    assert(freed == 4 && s_agc_pending_frees.empty());
}
'''

hui_cpp_harness = r'''#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <vector>
static bool idle;
static int freed, freed_index;
static int evo_agc_runtime_gpu_data_idle() { return idle; }
static void evo_direct_mem_free(void *p) { free(p); ++freed_index; }
namespace evo::hui_agc {
struct AgcBatch {
    struct Texture {};
    struct Pending { Texture *texture; int frames_left; };
    std::vector<Pending> pending_free_;
    std::vector<Texture *> textures_;
    Texture *fonts_[2] = {};
    unsigned font_count_ = 0;
    Texture *white_ = nullptr;
    void *indices_base_ = nullptr;
    std::uint16_t *indices_ = nullptr;
    bool ok_ = true;
    void destroy(Texture *p) { if (p) { delete p; ++freed; } }
    void end_frame();
    void release();
};
''' + hui_end + '\n' + hui_release + r'''
}
int main() {
    using evo::hui_agc::AgcBatch;
    AgcBatch batch;
    batch.pending_free_.push_back({new AgcBatch::Texture, 4});
    for (int i = 0; i < 100; ++i) batch.end_frame(); // unswapped loops, fence pending
    assert(freed == 0 && batch.pending_free_[0].frames_left == 4);
    idle = true;
    for (int i = 0; i < 4; ++i) batch.end_frame();
    assert(freed == 1 && batch.pending_free_.empty());
    batch.end_frame();
    assert(freed == 1);

    AgcBatch init_failed;
    init_failed.indices_base_ = malloc(8);
    init_failed.white_ = new AgcBatch::Texture;
    init_failed.fonts_[0] = new AgcBatch::Texture;
    init_failed.font_count_ = 1;
    init_failed.textures_.push_back(new AgcBatch::Texture);
    init_failed.pending_free_.push_back({new AgcBatch::Texture, 4});
    idle = false; // initialization runs inside an active AGC frame
    init_failed.release();
    assert(freed == 5 && freed_index == 1);
    assert(!init_failed.ok_ && !init_failed.white_ && !init_failed.indices_base_);
    assert(init_failed.pending_free_.empty() && init_failed.textures_.empty());
}
'''

with tempfile.TemporaryDirectory(prefix="agc-ui-retirement-") as directory:
    directory = Path(directory)
    for name, source, compiler, flags in (
        ("idle", c_harness, "cc", ["-std=c11"]),
        ("ui", cpp_harness, "c++", ["-std=c++17"]),
        ("hui", hui_cpp_harness, "c++", ["-std=c++17"]),
    ):
        source_path = directory / (name + (".c" if name == "idle" else ".cpp"))
        binary = directory / name
        source_path.write_text(source)
        subprocess.run([compiler, *flags, "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", str(source_path), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print("generated HUI init cleanup and GPU-gated release: PASS")
