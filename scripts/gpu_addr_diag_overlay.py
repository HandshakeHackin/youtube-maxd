"""Bounded GPU address trace for the first 4K native video frame.

This is diagnostic only. Keep the log small enough to survive a GPU fault.
"""


def apply_gpu_addr_diag_overlay(replace, player):
    runtime = player + "media/src/evo_agc_runtime.c"
    direct = player + "media/src/evo_direct_mem.c"
    rml = player + "ui_rml/src/evo_rmlui_render_agc.cpp"

    replace(runtime, "static int tube_present_retired;", """static int tube_diag_4k_active;
int evo_agc_runtime_diag_4k_active(void) { return tube_diag_4k_active; }
static int tube_present_retired;""")
    replace(runtime,
            "    /* PS user SGPR (PS stage: compact register 0x0c) - same derivation. */",
            """    if (coded_w >= 2880 && coded_h >= 2160) {
        static int trace_4k_descriptors = 2;
        tube_diag_4k_active = 1;
        if (trace_4k_descriptors > 0) {
            --trace_4k_descriptors;
            evo_boot_log("gpuaddr video slot=%u y=%p uv=%p u=%p v=%p coded=%dx%d pitch=%d/%d direct=%d desc=%llx scanout=%p/%p fence=%p expect=%u",
                         slot, y, uv, u, v, coded_w, coded_h, y_pitch, uv_pitch,
                         is_direct, (unsigned long long)desc_slice.gpu_addr,
                         g_agc_dev.scanout_buffers[0], g_agc_dev.scanout_buffers[1],
                         (void *)g_agc_dev.fences[slot], g_agc_dev.fence_expect[slot]);
            evo_boot_log("gpuaddr tsharp Y=%08x:%08x:%08x:%08x:%08x UV=%08x:%08x:%08x:%08x:%08x",
                         ((uint32_t *)desc_slice.cpu)[0], ((uint32_t *)desc_slice.cpu)[1],
                         ((uint32_t *)desc_slice.cpu)[2], ((uint32_t *)desc_slice.cpu)[3],
                         ((uint32_t *)desc_slice.cpu)[4], ((uint32_t *)desc_slice.cpu)[12],
                         ((uint32_t *)desc_slice.cpu)[13], ((uint32_t *)desc_slice.cpu)[14],
                         ((uint32_t *)desc_slice.cpu)[15], ((uint32_t *)desc_slice.cpu)[16]);
        }
    }
    /* PS user SGPR (PS stage: compact register 0x0c) - same derivation. */""")
    replace(direct,
            "    g_direct_pool.initialized = 1;\n    pthread_mutex_unlock(&g_direct_pool.lock);\n    return 0;",
            """    g_direct_pool.initialized = 1;
    pthread_mutex_unlock(&g_direct_pool.lock);
    evo_boot_log("gpuaddr direct_pool=%p+%zu hw=%d", base, pool_size_bytes, is_hw);
    return 0;""")
    replace(direct,
            "    /* Direct pool exhausted; graceful fallback to system allocator */\n    return malloc(bytes);",
            """    /* Direct pool exhausted; graceful fallback to system allocator */
    void *fallback = malloc(bytes);
    evo_boot_log("gpuaddr direct_fallback_alloc=%p+%zu", fallback, bytes);
    return fallback;""")
    replace(direct,
            "        pthread_mutex_unlock(&g_direct_pool.lock);\n        free(ptr);\n        return;",
            """        pthread_mutex_unlock(&g_direct_pool.lock);
        evo_boot_log("gpuaddr direct_fallback_free=%p", ptr);
        free(ptr);
        return;""")
    replace(rml, '#include "evo_boot_log.h"',
            '#include "evo_boot_log.h"\nextern "C" int evo_agc_runtime_diag_4k_active(void);')
    replace(rml,
            "    memcpy(tex_slice.cpu, tex->descriptor, sizeof(tex->descriptor));",
            """    memcpy(tex_slice.cpu, tex->descriptor, sizeof(tex->descriptor));
    if (evo_agc_runtime_diag_4k_active()) {
        static int trace_4k_ui_draws = 12;
        if (trace_4k_ui_draws > 0) {
            --trace_4k_ui_draws;
            evo_boot_log("gpuaddr ui_draw tex=%p+%zu descriptor=%llx vertex=%p+%zu index=%p+%zu",
                         tex->pixels, tex->alloc_bytes,
                         (unsigned long long)tex_slice.gpu_addr,
                         geom->vertex_data, geom->vertex_bytes,
                         geom->index_data, geom->index_bytes);
        }
    }""")
    replace(rml,
            "    if (geom->vertex_data) {\n        evo_direct_mem_free(geom->vertex_data);",
            """    if (evo_agc_runtime_diag_4k_active()) {
        static int trace_4k_geom_frees = 12;
        if (trace_4k_geom_frees > 0) {
            --trace_4k_geom_frees;
            evo_boot_log("gpuaddr ui_geom_free vertex=%p+%zu index=%p+%zu",
                         geom->vertex_data, geom->vertex_bytes,
                         geom->index_data, geom->index_bytes);
        }
    }
    if (geom->vertex_data) {
        evo_direct_mem_free(geom->vertex_data);""")
    replace(rml,
            "    if (tex->pixels) {\n        evo_direct_mem_free(tex->alloc_base);",
            """    if (evo_agc_runtime_diag_4k_active()) {
        static int trace_4k_tex_frees = 12;
        if (trace_4k_tex_frees > 0) {
            --trace_4k_tex_frees;
            evo_boot_log("gpuaddr ui_tex_free pixels=%p+%zu base=%p",
                         tex->pixels, tex->alloc_bytes, tex->alloc_base);
        }
    }
    if (tex->pixels) {
        evo_direct_mem_free(tex->alloc_base);""")
