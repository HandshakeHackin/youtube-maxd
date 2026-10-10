"""Separate native library warmup from resource creation before Lapy."""

PRELOAD = r'''
static int g_preload_tried;
static unsigned g_preload_sm;

static void preload_codec(nat_codec codec)
{
    const nat_codec_desc *d = &g_codec[codec];
    SceVideodec2DecoderConfigInfo config;
    SceVideodec2DecoderMemoryInfo memory;
    memset(&config, 0, sizeof config);
    memset(&memory, 0, sizeof memory);
    config.size = sizeof config;
    config.resource_type = SCE_VIDEODEC2_RESOURCE_COMPUTE;
    config.codec_type = d->codec_type;
    config.profile = d->profile_cfg;
    config.max_level = d->level_1080;
    config.max_width = EVO_VDEC_NATIVE_BOOT_W;
    config.max_height = EVO_VDEC_NATIVE_BOOT_H;
    config.max_dpb_frames = SCE_VIDEODEC2_AUTO_FRAMES;
    config.pipeline_depth = (uint32_t)d->pipeline_depth;
    config.cpu_affinity = 0x3f;
    config.cpu_priority = 700;
    config.optimize_progressive = 1;
    memory.size = sizeof memory;
    int rc = sceVideodec2QueryDecoderMemoryInfo(&config, &memory);
    note("ProsperoTube vdec: pre-storage warmup %s rc=0x%08x", d->tag, (unsigned)rc);
}

int evo_vdec_native_preload(void)
{
    if (g_preload_tried) return (int)g_preload_sm >= 0;
    g_preload_tried = 1;
    g_preload_sm = (unsigned)sceSysmoduleLoadModule(SCE_SYSMODULE_VIDEODEC2_NUM);
    if ((int)g_preload_sm < 0) return 0;
    SceVideodec2ComputeMemoryInfo compute;
    memset(&compute, 0, sizeof compute);
    compute.size = sizeof compute;
    int rc = sceVideodec2QueryComputeMemoryInfo(&compute);
    note("ProsperoTube vdec: pre-storage compute warmup rc=0x%08x", (unsigned)rc);
    preload_codec(NAT_H264);
#if EVO_VDEC_NATIVE_SECONDARY
    preload_codec(NAT_HEVC);
    preload_codec(NAT_VP9);
#if EVO_VDEC_NATIVE_10BIT
    preload_codec(NAT_HEVC10);
#endif
#endif
    return 1;
}

'''


def apply(work, replace, player):
    native = player + "media/src/evo_vdec_native.c"
    replace(native, "int evo_vdec_native_probe(void)\n{", PRELOAD + "int evo_vdec_native_probe(void)\n{")
    replace(native, "    unsigned sm = (unsigned)sceSysmoduleLoadModule(SCE_SYSMODULE_VIDEODEC2_NUM);",
            "    (void)evo_vdec_native_preload();\n    unsigned sm = g_preload_sm;")
    replace(native, "int evo_vdec_native_probe(void) { return 0; }",
            "int evo_vdec_native_preload(void) { return 0; }\nint evo_vdec_native_probe(void) { return 0; }")
    replace(player + "media/src/evo_vdec_native.h", "int evo_vdec_native_probe(void);",
            "int evo_vdec_native_preload(void);\nint evo_vdec_native_probe(void);")
    replace(player + "media/include/evo_vdec.h", "int evo_vdec_probe(void);",
            "/* Warm libraries without creating decoder/compute resources. */\nint evo_vdec_preload(void);\nint evo_vdec_probe(void);")
    replace(player + "media/src/evo_vdec_ffmpeg.c", "int evo_vdec_probe(void)\n{",
            "int evo_vdec_preload(void)\n{\n    return evo_vdec_native_preload();\n}\n\nint evo_vdec_probe(void)\n{")
    public_header = player + "media/include/evo_vdec.h"
    public_source = (work / public_header).read_text()
    begin = public_source.index("/* Preload the native backend")
    end = public_source.index("int evo_vdec_probe(void);", begin) + len("int evo_vdec_probe(void);")
    replace(public_header, public_source[begin:end], """/* Load and warm native libraries before Lapy without creating compute queues,
 * decoder instances or playback workers. Idempotent; returns 0 on host/payload.
 * Probe creates resident decoders after storage proof, using the cached module.
 * Open retains FFmpeg fallback if native bring-up fails. */
int evo_vdec_preload(void);
int evo_vdec_probe(void);""")
    private_header = player + "media/src/evo_vdec_native.h"
    private_source = (work / private_header).read_text()
    begin = private_source.index("/*\n * Preload libSceVideodec2")
    end = private_source.index("int evo_vdec_native_probe(void);", begin) + len("int evo_vdec_native_probe(void);")
    replace(private_header, private_source[begin:end], """/* Query-only library preload must precede Lapy. Resident probe follows storage
 * proof; it reuses the cached module result and preserves software fallback. */
int evo_vdec_native_preload(void);
int evo_vdec_native_probe(void);""")
    app = player + "core/src/Application.cpp"
    replace(app, "    evo_vdec_probe();", "    evo_vdec_preload();")
    source = (work / app).read_text()
    start = source.index('#if defined(EVO_APP_MODULE)\n    evo_bt("AGC: bare-metal AGC device context");')
    end = source.index("#endif", start) + len("#endif")
    hardware = source[start:end]
    split = hardware.index("    evo_keyboard_ime_probe();")
    display = hardware[:split] + "#endif"
    preloads = "#if defined(EVO_APP_MODULE)\n" + hardware[split:]
    replace(app, hardware, preloads)
    replace(app, "    evo_av_log_init();", display + "\n\n    evo_vdec_probe();\n    evo_av_log_init();")
