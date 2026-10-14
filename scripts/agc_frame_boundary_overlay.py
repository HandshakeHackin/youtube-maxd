"""Close an AGC UI frame even when RmlUi did not request a swap."""


def apply(replace, player):
    replace(
        player + "core/src/Application.cpp",
        "        evo_perf_monitor_tick(0.0);\n        if (!swap) {",
        """        evo_perf_monitor_tick(0.0);
        if (!swap && uiActive && !isPlayer) {
            /* frame_begin ran before the UI pass. An unchanged/provider page may
             * consume no draw and leave swap false for arbitrarily many loops.
             * Close that frame now: the runtime aborts an empty frame without
             * flipping, while a draw that escaped consume_drew still submits.
             * Otherwise later passes append to the same DCB until it fills. */
            evo_agc_runtime_frame_end();
            evo_rmlui_end_frame();
        }
        if (!swap) {""",
    )
