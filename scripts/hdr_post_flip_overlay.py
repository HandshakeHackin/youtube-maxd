"""Add a one-time VideoOut status readback after each AGC mode switch reaches scanout."""


def apply_hdr_post_flip_overlay(replace, player):
    path = player + "media/src/evo_agc_runtime.c"
    replace(path, '''    evo_boot_log("agc mode switch: %s -> first frame on screen +%d ms (blocked %d ms)",
                 g_agc_dev.mode_switch_what,
                 (int)((agc_now_us() - g_agc_dev.mode_switch_t0_us) / 1000),
                 g_agc_dev.mode_switch_block_ms);
    g_agc_dev.mode_switch_what = NULL;''',
            '''    evo_boot_log("agc mode switch: %s -> first frame on screen +%d ms (blocked %d ms)",
                 g_agc_dev.mode_switch_what,
                 (int)((agc_now_us() - g_agc_dev.mode_switch_t0_us) / 1000),
                 g_agc_dev.mode_switch_block_ms);
    /* This call follows the flip-completion fence, so it observes the mode
     * that reached scanout rather than the prior mode still reported by the
     * immediate status query in set_hdr_output(). Log once per switch. */
    evo_vo_output_status output = {0};
    int32_t rc = sceVideoOutGetOutputStatus(g_agc_dev.video_handle, &output);
    evo_boot_log("agc post-flip: %s out_rc=%d dynamic_range=%u refresh=%llu",
                 g_agc_dev.mode_switch_what, rc, output.dynamic_range,
                 (unsigned long long)output.refresh_rate);
    g_agc_dev.mode_switch_what = NULL;''')
