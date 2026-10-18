"""Connect Auto Max to the existing player's measured counters and reopen path."""


def apply(root, work, player):
    app = work / player
    pp = app / "pp/src/pp_playback.c"
    text = pp.read_text()
    for name in ("frames_in", "frames_late_dropped"):
        old = f"    pb->stats.{name} = 0;"
        assert text.count(old) == 1
        text = text.replace(old, f"    __atomic_store_n(&pb->stats.{name}, 0, __ATOMIC_RELAXED);")
        old = f"pb->stats.{name}++;"
        assert text.count(old) == 1
        text = text.replace(old, f"__atomic_fetch_add(&pb->stats.{name}, 1, __ATOMIC_RELAXED);")
    pp.write_text(text)
    screen = app / "core/src/screens/PlayerScreen.cpp"
    text = screen.read_text()
    anchor = "    // End YouTube Max’d error fixer: long buffering."
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '''
    // Auto Max uses measured samples, then the existing scoped quality reopen.
    const int maxdAdapt = tube_maxd_auto_sample(now,
        __atomic_load_n(&g_pp_pb.stats.frames_in, __ATOMIC_RELAXED),
        __atomic_load_n(&g_pp_pb.stats.frames_late_dropped, __ATOMIC_RELAXED),
        reallyPaused || ended || playback->isMusicMode() || playback->isScrubbing() ||
        evo_demux_is_seeking() || g_pp_pb.seek_discarding || tube_phone_owns_media() || tube_background_play_active(),
        pb_prebuffer_hold || stalled, playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled);
    if (maxdAdapt) {
        toast("AUTO MAX", maxdAdapt == 3 ? "Trying a higher quality after stable playback" :
            maxdAdapt == 2 ? "Switching format to reduce dropped frames" : "Lowering quality to reduce buffering");
        playback->stopPlayback();
        if (auto sm = Application::getInstance().getScreenManager()) sm->returnFromPlayback();
        return;
    }
''')
    screen.write_text(text)
    picker = app / "core/src/screens/AudioTrackPickerScreen.cpp"
    text = picker.read_text()
    anchor = 'automatic.streamIndex = -1; automatic.label = "AUTO";'
    assert text.count(anchor) == 1
    text = text.replace(anchor, 'automatic.streamIndex = -1; automatic.label = tube_maxd_auto_enabled() ? "AUTO MAX" : "AUTO";')
    text = text.replace('automatic.detail = "YouTube Max’d picks the best quality available";',
                        'automatic.detail = tube_maxd_auto_enabled() ? "Adjusts quality from buffering and dropped frames" : "YouTube Max’d picks the best quality available";')
    picker.write_text(text)
