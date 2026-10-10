"""Exercise generated native warmup without allocating decoder resources."""
import os
from pathlib import Path
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
player = work / "projects/evoplayer"
source = (player / "media/src/evo_vdec_native.c").read_text()
begin = source.index("static int g_preload_tried;")
end = source.index("int evo_vdec_native_probe(void)\n{", begin)
body = source[begin:end]
probe = source[end:source.index("/*\n * The largest", end)]
assert "unsigned sm = g_preload_sm;" in probe
assert "sceSysmoduleLoadModule" not in probe

app = (player / "core/src/Application.cpp").read_text()
initialize = app[app.index("bool Application::initialize("):app.index("bool Application::initHardware()")]
hardware = app[app.index("bool Application::initHardware()"):app.index("    evo_av_log_init();")]
assert "evo_vdec_preload();" in initialize and "evo_vdec_probe();" not in initialize
assert hardware.index("evo_keyboard_ime_probe();") < hardware.index("evo_jailbreak_self()")
assert hardware.index("evo_webui_preload();") < hardware.index("evo_jailbreak_self()")
assert hardware.index("evo_jailbreak_self()") < hardware.index("evo_agc_runtime_init(") < hardware.index("evo_vdec_probe();")
assert "startup stopped" in hardware

scaffold = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SCE_SYSMODULE_VIDEODEC2_NUM 207
#define SCE_VIDEODEC2_RESOURCE_COMPUTE 1
#define SCE_VIDEODEC2_AUTO_FRAMES 0
#define EVO_VDEC_NATIVE_BOOT_W 1920
#define EVO_VDEC_NATIVE_BOOT_H 1088
typedef enum { NAT_H264, NAT_HEVC, NAT_VP9, NAT_HEVC10 } nat_codec;
typedef struct { const char *tag; unsigned codec_type, profile_cfg; int level_1080, pipeline_depth; } nat_codec_desc;
static nat_codec_desc g_codec[] = {{"AVC",1,2,3,4},{"HEVC",5,6,7,8},{"VP9",9,10,11,12},{"HEVC10",13,14,15,16}};
typedef struct { unsigned size, resource_type, codec_type, profile; int max_level; unsigned max_width, max_height, max_dpb_frames, pipeline_depth, cpu_affinity, cpu_priority, optimize_progressive; } SceVideodec2DecoderConfigInfo;
typedef struct { unsigned size; } SceVideodec2DecoderMemoryInfo;
typedef struct { unsigned size; } SceVideodec2ComputeMemoryInfo;
static int modules, queries, computes, module_result, query_result;
static int sceSysmoduleLoadModule(unsigned module) { assert(module == 207); ++modules; return module_result; }
static int sceVideodec2QueryComputeMemoryInfo(SceVideodec2ComputeMemoryInfo *m) { assert(m->size == sizeof *m); ++computes; return query_result; }
static int sceVideodec2QueryDecoderMemoryInfo(SceVideodec2DecoderConfigInfo *c, SceVideodec2DecoderMemoryInfo *m) {
    assert(c->size == sizeof *c && m->size == sizeof *m);
    assert(c->max_width == 1920 && c->max_height == 1088);
    assert(c->resource_type == 1 && c->max_dpb_frames == 0);
    assert(c->cpu_affinity == 0x3f && c->cpu_priority == 700 && c->optimize_progressive == 1);
    const nat_codec_desc *d = &g_codec[queries++];
    assert(c->codec_type == d->codec_type && c->profile == d->profile_cfg);
    assert(c->max_level == d->level_1080 && c->pipeline_depth == (unsigned)d->pipeline_depth);
    return query_result;
}
static void note(const char *format, ...) { (void)format; }
'''
checks = r'''
static void reset(void) { g_preload_tried = 0; g_preload_sm = 0; modules = queries = computes = 0; }
int main(void) {
    module_result = -1;
    assert(evo_vdec_native_preload() == 0);
    assert(evo_vdec_native_preload() == 0);
    assert(modules == 1 && !queries && !computes);
    reset(); module_result = 0;
    for (int i = 0; i < 4; ++i) assert(evo_vdec_native_preload() == 1);
    assert(modules == 1 && computes == 1 && queries == EXPECTED_CODECS);
    reset(); query_result = -1;
    assert(evo_vdec_native_preload() == 1);
    assert(evo_vdec_native_preload() == 1);
    assert(modules == 1 && computes == 1 && queries == EXPECTED_CODECS);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="tube-native-preload-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    # Reject undeclared allocation/creation/thread calls in shipping warmup.
    test.write_text(scaffold + body + checks)
    for secondary, tenbit, count in ((0, 0, 1), (1, 0, 3), (1, 1, 4)):
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined",
                        f"-DEVO_VDEC_NATIVE_SECONDARY={secondary}",
                        f"-DEVO_VDEC_NATIVE_10BIT={tenbit}", f"-DEXPECTED_CODECS={count}",
                        str(test), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print("PASS: shipping warmup is query-only/idempotent across codec profiles; Lapy precedes display/resident creation")
