"""Compile-time AGC texture-binding trace; opt in with EXTRA_CFLAGS=-DTUBE_GPU_BIND_TRACE.

Diagnostic logs include frame/slot, binding origin, sampled address range and
descriptor address. Normal builds compile out both call sites and logger.
"""


def apply(replace, player):
    header = player + "media/include/evo_agc_runtime.h"
    runtime = player + "media/src/evo_agc_runtime.c"
    rml = player + "ui_rml/src/evo_rmlui_render_agc.cpp"
    hui = player + "ui_kit/src/hui_agc_batch.cpp"

    replace(header, "int evo_agc_runtime_exit_ready(unsigned timeout_ms);", """int evo_agc_runtime_exit_ready(unsigned timeout_ms);
#ifdef TUBE_GPU_BIND_TRACE
void evo_agc_runtime_trace_binding(const char *origin, uint64_t address,
                                   size_t bytes, uint64_t descriptor);
#define EVO_AGC_TRACE_BIND(origin, address, bytes, descriptor) \\
    evo_agc_runtime_trace_binding((origin), (uint64_t)(address), (size_t)(bytes), \\
                                  (uint64_t)(descriptor))
#else
#define EVO_AGC_TRACE_BIND(origin, address, bytes, descriptor) ((void)0)
#endif""")
    replace(runtime, "static int agc_wait_gpu_idle(unsigned timeout_ms)", """#ifdef TUBE_GPU_BIND_TRACE
void evo_agc_runtime_trace_binding(const char *origin, uint64_t address,
                                   size_t bytes, uint64_t descriptor)
{
    if (!g_agc_dev.initialized || !g_agc_dev.frame_active || !address || !bytes)
        return;
    evo_boot_log("gpu_bind frame=%llu slot=%u origin=%s addr=0x%llx bytes=%zu desc=0x%llx",
                 (unsigned long long)g_agc_dev.frame_counter, g_agc_dev.current_slot,
                 origin, (unsigned long long)address, bytes,
                 (unsigned long long)descriptor);
}
#endif

static int agc_wait_gpu_idle(unsigned timeout_ms)""")
    replace(runtime, "    ps_user[ud.ps_texture_table_dword] = (uint32_t)tex_d.gpu_addr;", """    EVO_AGC_TRACE_BIND("agc-composite", g_agc_dev.composite_pixels,
                       (size_t)pitch * (size_t)h, tex_d.gpu_addr);
    ps_user[ud.ps_texture_table_dword] = (uint32_t)tex_d.gpu_addr;""")
    replace(runtime, "        if (r0 != 0 || r1 != 0 || r2 != 0) {", """#ifdef TUBE_GPU_BIND_TRACE
        if (r0 == 0 && r1 == 0 && r2 == 0) {
            EVO_AGC_TRACE_BIND(is_direct ? "video-direct-y" : "video-staged-y",
                               y_gpu, (size_t)y_pitch_gpu * (size_t)coded_h, desc_slice.gpu_addr);
            EVO_AGC_TRACE_BIND(is_direct ? "video-direct-u" : "video-staged-u",
                               u_gpu, (size_t)u_pitch_gpu * ch2, desc_slice.gpu_addr + 48u);
            EVO_AGC_TRACE_BIND(is_direct ? "video-direct-v" : "video-staged-v",
                               v_gpu, (size_t)v_pitch_gpu * ch2, desc_slice.gpu_addr + 96u);
        }
#endif
        if (r0 != 0 || r1 != 0 || r2 != 0) {""")
    replace(runtime, "        if (r0 != 0 || r1 != 0) {", """#ifdef TUBE_GPU_BIND_TRACE
        if (r0 == 0 && r1 == 0) {
            EVO_AGC_TRACE_BIND(is_direct ? "video-direct-y" : "video-staged-y",
                               y_gpu, (size_t)y_pitch_gpu * (size_t)coded_h, desc_slice.gpu_addr);
            EVO_AGC_TRACE_BIND(is_direct && !planar ? "video-direct-uv" : "video-staged-uv",
                               uv_gpu, (size_t)uv_pitch_gpu * ch2, desc_slice.gpu_addr + 48u);
        }
#endif
        if (r0 != 0 || r1 != 0) {""")

    replace(rml, "        ps_user_data[ud.ps_texture_table_dword] = (uint32_t)tex_slice.gpu_addr;", """        ps_user_data[ud.ps_texture_table_dword] = (uint32_t)tex_slice.gpu_addr;
#ifdef TUBE_GPU_BIND_TRACE
        if (tex->pixels)
            EVO_AGC_TRACE_BIND("rml-gpu-pool", tex->pixels,
                               (size_t)tex->pitch * (size_t)tex->height, tex_slice.gpu_addr);
#endif""")
    replace(rml, "    ps_user[ud.ps_texture_table_dword] = (uint32_t)tex_desc.gpu_addr;", """    EVO_AGC_TRACE_BIND(src_is_base ? "rml-scanout-blur" : "rml-layer-blur",
                       src.gpu_addr, (size_t)src.pitch_bytes * src.height, tex_desc.gpu_addr);
    ps_user[ud.ps_texture_table_dword] = (uint32_t)tex_desc.gpu_addr;""")
    replace(rml, "    RenderGeometry(m_fullscreen_quad, Rml::Vector2f(0, 0), (Rml::TextureHandle)&src_tex);", """    EVO_AGC_TRACE_BIND(src_is_base ? "rml-scanout-copy" : "rml-layer-copy",
                       src.gpu_addr, (size_t)src.pitch_bytes * src.height, 0);
    RenderGeometry(m_fullscreen_quad, Rml::Vector2f(0, 0), (Rml::TextureHandle)&src_tex);""")

    replace(hui, "            texture_table = table.gpu_addr;", """            texture_table = table.gpu_addr;
#ifdef TUBE_GPU_BIND_TRACE
            Texture *own_tex = own && own <= textures_.size() && textures_[own - 1]
                                   ? textures_[own - 1] : white_;
            EVO_AGC_TRACE_BIND("hui-gpu-pool", own_tex->pixels,
                               (size_t)own_tex->pitch * own_tex->height, texture_table);
            for (std::uint32_t f = 0; f < hui::gfx::kFontSlots; ++f) {
                Texture *font_tex = f < font_count_ && fonts_[f] ? fonts_[f] : white_;
                EVO_AGC_TRACE_BIND("hui-font-gpu-pool", font_tex->pixels,
                                   (size_t)font_tex->pitch * font_tex->height,
                                   texture_table + (uint64_t)(f + 1u) * sizeof(textures[0]));
            }
#endif""")
