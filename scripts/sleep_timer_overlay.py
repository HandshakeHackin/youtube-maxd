"""Inactivity sleep timer connected to actual playback and controller/phone input."""


def apply_sleep_timer_overlay(replace, player):
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp',
            'KindAudio, KindSpeed, KindChat };', 'KindAudio, KindSpeed, KindChat, KindSleep };')
    replace(picker, '    // End ProsperoTube chat option rows.', '''    // End ProsperoTube chat option rows.
    if (playback->isActive()) {
        for (int minutes = 0; minutes <= 600; minutes += 30) {
            Entry e; e.kind = KindSleep; e.streamIndex = minutes;
            e.label = minutes ? "Sleep after " + std::to_string(minutes) + " min without input" : "Sleep timer: disabled";
            e.detail = "Controller input restarts inactivity; expiry pauses playback";
            e.badge = tube_sleep_minutes() == minutes ? "SELECTED" : "";
            m_tracks.push_back(std::move(e));
        }
    }''')
    replace(picker, '    if (e.kind == KindChat) {', '''    if (e.kind == KindSleep) {
        if (tube_sleep_set_minutes(e.streamIndex)) toast("SLEEP TIMER", e.label.c_str());
        else toast("SLEEP TIMER", "Choose a valid inactivity duration");
    } else if (e.kind == KindChat) {''')
    app = player + 'core/src/Application.cpp'
    replace(app, '        g_pace.mark(1);   /* input */', '''        g_pace.mark(1);   /* input */
        // ProsperoTube sleep timer: pause once, preserving the current media/queue.
        if (!m_softClose && !ProviderHostScreen::playbackStartupBusy()) {
            auto playback = getPlaybackController();
            if (tube_sleep_tick(playback && playback->isActive())) {
                playback->setPaused(true);
                toast("SLEEP TIMER", "Playback paused after inactivity");
            }
        }''')
    replace(app, '            phone.context = this;', '''            phone.context = this;
            phone.activity = [](void*) { tube_sleep_activity(); };''')
    replace(app, '            hasInput = (pressed != 0 || released != 0 || evo_input_any(&evo_pad_state));', '''            hasInput = (pressed != 0 || released != 0 || evo_input_any(&evo_pad_state));
            if (hasInput) {
                auto playback = getPlaybackController();
                if (playback && playback->isActive()) tube_sleep_activity();
            }''')
