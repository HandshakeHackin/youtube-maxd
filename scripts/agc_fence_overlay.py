"""Keep AGC command and descriptor memory mapped until its GPU fence retires."""


def apply_agc_fence_overlay(replace, player):
    path = player + "media/src/evo_agc_runtime.c"
    replace(path, "static int agc_wait_gpu_idle(unsigned timeout_ms)", """/* A missing/late marker is not proof that command memory can be reused. */
static int agc_wait_slot_fence(uint32_t slot, uint32_t marker,
                               unsigned attempts, unsigned step_us,
                               unsigned *waited)
{
    if (waited) *waited = 0;
    if (!marker || !g_agc_dev.fences[slot]) return 0;
    for (unsigned i = 0; i < attempts; ++i) {
        evo_agc_runtime_cache_flush((const void *)g_agc_dev.fences[slot], 4);
        if (*g_agc_dev.fences[slot] == marker) {
            if (waited) *waited = i;
            return 1;
        }
        sceKernelUsleep(step_us);
    }
    if (waited) *waited = attempts;
    return 0;
}

static int agc_emit_frame_fence(SceAgcCommandBuffer *cb, uint32_t slot,
                                uint32_t marker)
{
    if (evo_agc_writer_flush_color_target(cb) != 0) return 0;
    return evo_agc_writer_release_mem(cb,
        (uint64_t)(uintptr_t)g_agc_dev.fences[slot], marker) == 0;
}

static int agc_wait_gpu_idle(unsigned timeout_ms)""")
    replace(path, """        unsigned waited = 0;
        for (; waited < timeout_ms; ++waited) {
            evo_agc_runtime_cache_flush((const void *)g_agc_dev.fences[slot], 4);
            if (*g_agc_dev.fences[slot] == g_agc_dev.fence_expect[slot])
                break;
            sceKernelUsleep(1000);
        }

        evo_boot_log("agc shutdown: slot=%d %s after %ums (fence=%u expect=%u)",
                     slot, waited >= timeout_ms ? "STILL BUSY"
                                                : "idle",
                     waited,
                     (unsigned)*g_agc_dev.fences[slot],
                     (unsigned)g_agc_dev.fence_expect[slot]);
        if (waited >= timeout_ms) idle = 0;
        else g_agc_dev.fence_expect[slot] = 0;""",
            """        unsigned waited = 0;
        const int retired = agc_wait_slot_fence((uint32_t)slot,
            g_agc_dev.fence_expect[slot], timeout_ms, 1000, &waited);
        evo_boot_log("agc shutdown: slot=%d %s after %ums (fence=%u expect=%u)",
                     slot, retired ? "idle" : "STILL BUSY", waited,
                     (unsigned)*g_agc_dev.fences[slot],
                     (unsigned)g_agc_dev.fence_expect[slot]);
        if (!retired) idle = 0;
        else g_agc_dev.fence_expect[slot] = 0;""")
    replace(path, "    agc_wait_gpu_idle(500);",
            """    if (!agc_wait_gpu_idle(500)) {
        evo_boot_log("agc shutdown: GPU fence busy; retaining mapped resources");
        evo_boot_log_flush();
        return;
    }""")
    replace(path, """        unsigned waits = 0;
        for (; waits < 2000; ++waits) {
            evo_agc_runtime_cache_flush((const void *)g_agc_dev.fences[slot], 4);
            if (*g_agc_dev.fences[slot] == g_agc_dev.fence_expect[slot])
                break;
            sceKernelUsleep(1000);
        }
        if (waits >= 2000) {""",
            """        unsigned waits = 0;
        if (!agc_wait_slot_fence(slot, g_agc_dev.fence_expect[slot],
                                 2000, 1000, &waits)) {""")
    replace(path, """                evo_boot_log("agc frame_begin slot=%u FENCE TIMEOUT fence=%u expect=%u",
                             slot, (unsigned)*g_agc_dev.fences[slot],
                             (unsigned)g_agc_dev.fence_expect[slot]);
            }
        } else if (tracing) {
            evo_boot_log("agc frame_begin slot=%u retired after %u waits", slot, waits);
        }
    }""",
            """                evo_boot_log("agc frame_begin slot=%u FENCE TIMEOUT fence=%u expect=%u",
                             slot, (unsigned)*g_agc_dev.fences[slot],
                             (unsigned)g_agc_dev.fence_expect[slot]);
            }
            return; /* preserve this sealed slot and its live GPU mappings */
        } else {
            g_agc_dev.fence_expect[slot] = 0;
            if (tracing)
                evo_boot_log("agc frame_begin slot=%u retired after %u waits", slot, waits);
        }
    }""")
    replace(path, """    evo_agc_writer_flush_color_target(&g_agc_dev.current_cb);
    evo_agc_writer_release_mem(&g_agc_dev.current_cb, fence_addr, marker);

    /* 3. Seal transient ring */""",
            """    if (!agc_emit_frame_fence(&g_agc_dev.current_cb, slot, marker)) {
        evo_boot_log("agc frame_end slot=%u cannot emit GPU fence; dropping unsubmitted frame", slot);
        g_agc_dev.fence_expect[slot] = 0;
        evo_agc_transient_ring_abort(&g_agc_dev.transient_ring, slot);
        g_agc_dev.ring_token[slot] = 0;
        g_agc_dev.frame_active = 0;
        return;
    }

    /* 3. Seal transient ring */""")
    replace(path, """    const uint64_t fence_addr = (uint64_t)(uintptr_t)g_agc_dev.fences[slot];
    g_agc_dev.flip_arg++;""",
            """    g_agc_dev.flip_arg++;""")
    replace(path, """    if (submit_rc != 0) {
        printf(EVO_AGC_LOG_PREFIX "sceAgcDriverSubmitDcb failed: %d\\n", submit_rc);
    } else {""",
            """    if (submit_rc != 0) {
        printf(EVO_AGC_LOG_PREFIX "sceAgcDriverSubmitDcb failed: %d\\n", submit_rc);
        g_agc_dev.fence_expect[slot] = 0;
        evo_agc_transient_ring_abort(&g_agc_dev.transient_ring, slot);
        g_agc_dev.ring_token[slot] = 0;
        g_agc_dev.frame_active = 0;
        return;
    } else {""")
    replace(path, """            unsigned waits = 0;
            for (; waits < 500000u / step_us; ++waits) {
                evo_agc_runtime_cache_flush((const void *)g_agc_dev.fences[slot], 4);
                if (*g_agc_dev.fences[slot] == marker)
                    break;
                sceKernelUsleep(step_us);
            }""",
            """            unsigned waits = 0;
            const int retired = agc_wait_slot_fence(slot, marker,
                500000u / step_us, step_us, &waits);""")
    replace(path, """            int32_t fliprc = sceVideoOutSubmitFlip(g_agc_dev.video_handle,
                                                   g_agc_dev.active_backbuffer,
                                                   1 /* VSYNC */,
                                                   (int64_t)g_agc_dev.flip_arg);
            g_agc_dev.fence_expect[slot] = 0; /* already waited; don't re-wait */""",
            """            if (!retired) {
                evo_boot_log("agc frame_end slot=%u GPU fence timeout marker=%u; retaining slot",
                             slot, marker);
                g_agc_dev.frame_active = 0;
                return; /* no flip, slot advance, source retirement or command reuse */
            }
            g_agc_dev.fence_expect[slot] = 0;
            int32_t fliprc = sceVideoOutSubmitFlip(g_agc_dev.video_handle,
                                                   g_agc_dev.active_backbuffer,
                                                   1 /* VSYNC */,
                                                   (int64_t)g_agc_dev.flip_arg);""")
    replace(path, """    evo_agc_runtime_frame_begin();

    const uint32_t slot = g_agc_dev.current_slot;""",
            """    evo_agc_runtime_frame_begin();
    if (!g_agc_dev.frame_active) return -1;

    const uint32_t slot = g_agc_dev.current_slot;""")
    replace(path, "    agc_wait_gpu_idle(200);", "    if (!agc_wait_gpu_idle(200)) return -1;", count=2)
