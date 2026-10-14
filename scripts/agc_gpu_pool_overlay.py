"""Keep AGC resources on the GPU-mapped direct pool when it is exhausted."""


def apply_agc_gpu_pool_overlay(replace, player):
    header = player + "media/include/evo_direct_mem.h"
    direct = player + "media/src/evo_direct_mem.c"
    rml = player + "ui_rml/src/evo_rmlui_render_agc.cpp"
    hui = player + "ui_kit/src/hui_agc_batch.cpp"

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
    replace(rml, "evo_direct_mem_alloc(geom->vertex_bytes)",
            "evo_direct_mem_alloc_gpu(geom->vertex_bytes)")
    replace(rml, "evo_direct_mem_alloc(geom->index_bytes)",
            "evo_direct_mem_alloc_gpu(geom->index_bytes)")
    replace(rml, "evo_direct_mem_alloc(tex->alloc_bytes + 255u)",
            "evo_direct_mem_alloc_gpu(tex->alloc_bytes + 255u)")
    replace(hui, "evo_direct_mem_alloc(index_bytes + 255u)",
            "evo_direct_mem_alloc_gpu(index_bytes + 255u)")
    replace(hui, "evo_direct_mem_alloc(bytes + 255u)",
            "evo_direct_mem_alloc_gpu(bytes + 255u)")
