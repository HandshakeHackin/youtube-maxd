"""Keep AGC resources on the GPU-mapped direct pool when it is exhausted."""


def apply_agc_gpu_pool_overlay(replace, player):
    header = player + "media/include/evo_direct_mem.h"
    direct = player + "media/src/evo_direct_mem.c"
    rml_agc = player + "ui_rml/src/evo_rmlui_render_agc.cpp"
    hui = player + "ui_kit/src/hui_agc_batch.cpp"
    agc = player + "media/src/evo_agc_runtime.c"
    agc_header = player + "media/include/evo_agc_runtime.h"

    replace(header, "void *evo_direct_mem_alloc(size_t bytes);",
            """void *evo_direct_mem_alloc(size_t bytes);
/* AGC descriptors may only reference the GPU-mapped hardware pool. Unlike
 * evo_direct_mem_alloc, this returns NULL instead of a malloc fallback. */
void *evo_direct_mem_alloc_gpu(size_t bytes);""")
    replace(direct, "void *evo_direct_mem_calloc(size_t count, size_t size)",
            """void *evo_direct_mem_alloc_gpu(size_t bytes)
{
    void *ptr = evo_direct_mem_alloc(bytes);
    if (!ptr) return NULL;

    pthread_mutex_lock(&g_direct_pool.lock);
    uintptr_t base = (uintptr_t)g_direct_pool.base_ptr;
    uintptr_t addr = (uintptr_t)ptr;
    int in_pool = g_direct_pool.initialized && addr >= base &&
                  addr - base < g_direct_pool.total_size;
#if defined(EVO_TARGET_PS5)
    in_pool = in_pool && g_direct_pool.is_direct_hw;
#endif
    pthread_mutex_unlock(&g_direct_pool.lock);
    if (!in_pool) {
        evo_direct_mem_free(ptr);
        return NULL;
    }
    return ptr;
}

void *evo_direct_mem_calloc(size_t count, size_t size)""")
    replace(agc_header, "int evo_agc_runtime_exit_ready(unsigned timeout_ms);",
            "int evo_agc_runtime_exit_ready(unsigned timeout_ms);\nint evo_agc_runtime_gpu_data_idle(void);")
    replace(agc, "static int agc_wait_gpu_idle(unsigned timeout_ms)",
            """/* RmlUi can release a texture after a timed-out submit. Its direct-pool
 * storage cannot be recycled until that submit's fence retires. */
int evo_agc_runtime_gpu_data_idle(void)
{
    if (!g_agc_dev.initialized) return 1;
    if (g_agc_dev.frame_active) return 0;
    for (uint32_t slot = 0; slot < EVO_AGC_FRAME_SLOTS; ++slot) {
        const uint32_t marker = g_agc_dev.fence_expect[slot];
        if (!marker) continue;
        if (!g_agc_dev.fences[slot]) return 0;
        evo_agc_runtime_cache_flush((const void *)g_agc_dev.fences[slot], 4);
        if (*g_agc_dev.fences[slot] != marker) return 0;
    }
    return 1;
}

static int agc_wait_gpu_idle(unsigned timeout_ms)""")
    replace(rml_agc, "#pragma clang diagnostic pop",
            """#pragma clang diagnostic pop

static std::vector<void *> s_agc_pending_frees;

static void agc_collect_released_buffers()
{
    if (s_agc_pending_frees.empty() || !evo_agc_runtime_gpu_data_idle()) return;
    for (void *ptr : s_agc_pending_frees) evo_direct_mem_free(ptr);
    s_agc_pending_frees.clear();
}

static void *agc_alloc_gpu(size_t bytes)
{
    agc_collect_released_buffers();
    return evo_direct_mem_alloc_gpu(bytes);
}

static void agc_release_buffer(void *ptr)
{
    if (!ptr) return;
    if (evo_agc_runtime_gpu_data_idle()) {
        agc_collect_released_buffers();
        evo_direct_mem_free(ptr);
    } else {
        s_agc_pending_frees.push_back(ptr);
    }
}""")
    replace(rml_agc, "    m_layer_stack.clear();\n    m_filters.clear();",
            "    m_layer_stack.clear();\n    m_filters.clear();\n    if (m_white_texture) ReleaseTexture(reinterpret_cast<Rml::TextureHandle>(m_white_texture.release()));")
    replace(rml_agc, "    /* Handled by evo_agc_runtime_present() */",
            "    /* Handled by evo_agc_runtime_present() */\n    agc_collect_released_buffers();")
    replace(rml_agc, 'evo_direct_mem_alloc(geom->vertex_bytes)',
            'agc_alloc_gpu(geom->vertex_bytes)')
    replace(rml_agc, 'evo_direct_mem_alloc(geom->index_bytes)',
            'agc_alloc_gpu(geom->index_bytes)')
    replace(rml_agc, 'evo_direct_mem_alloc(tex->alloc_bytes + 255u)',
            'agc_alloc_gpu(tex->alloc_bytes + 255u)')
    replace(rml_agc, 'evo_direct_mem_free(geom->vertex_data)',
            'agc_release_buffer(geom->vertex_data)', count=2)
    replace(rml_agc, 'evo_direct_mem_free(geom->index_data)',
            'agc_release_buffer(geom->index_data)')
    replace(rml_agc, 'evo_direct_mem_free(tex->alloc_base)',
            'agc_release_buffer(tex->alloc_base)', count=2)
    replace(hui, 'evo_direct_mem_alloc(index_bytes + 255u)',
            'evo_direct_mem_alloc_gpu(index_bytes + 255u)')
    replace(hui, 'evo_direct_mem_alloc(bytes + 255u)',
            'evo_direct_mem_alloc_gpu(bytes + 255u)')
