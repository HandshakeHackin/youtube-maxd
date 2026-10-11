"""SmartTube seek confirmation, preserving playback state and demux ownership."""


def apply_seek_controls_overlay(replace, player):
    controller = player + 'core/src/services/PlaybackController.cpp'
    replace(player + 'core/include/evo/interfaces/IPlaybackController.hpp',
            '    virtual void moveScrub(double deltaSeconds) = 0;',
            '    virtual void moveScrub(double deltaSeconds) = 0;\n    virtual double getSeekIntervalSeconds() const = 0;')
    replace(player + 'core/include/evo/services/PlaybackController.hpp',
            '    void moveScrub(double deltaSeconds) override;',
            '    void moveScrub(double deltaSeconds) override;\n    double getSeekIntervalSeconds() const override;')
    replace(controller, 'void PlaybackController::moveScrub(double deltaSeconds) {', '''double PlaybackController::getSeekIntervalSeconds() const {
    return m_source.provider == "youtube" ? tube_player_seek_seconds() : 10.0;
}

void PlaybackController::moveScrub(double deltaSeconds) {''')
    replace(controller, 'moveScrub(static_cast<double>(dir) * speed);',
            'moveScrub(static_cast<double>(dir) * speed * getSeekIntervalSeconds() / 10.0);')
    replace(player + 'core/include/evo/interfaces/IPlaybackController.hpp',
            '    virtual double getPositionSeconds() const = 0;',
            '    virtual double getPositionSeconds() const = 0;\n    virtual double getPlaybackPositionSeconds() const = 0;')
    replace(player + 'core/include/evo/services/PlaybackController.hpp',
            '    double getPositionSeconds() const override;',
            '    double getPositionSeconds() const override;\n    double getPlaybackPositionSeconds() const override;')
    replace(controller, '''double PlaybackController::getPositionSeconds() const {
    if (isScrubbing()) {
        return m_scrubTargetSeconds;
    }''', '''double PlaybackController::getPositionSeconds() const {
    if (isScrubbing()) return m_scrubTargetSeconds;
    return getPlaybackPositionSeconds();
}

double PlaybackController::getPlaybackPositionSeconds() const {''')
    replace(controller, '''void PlaybackController::saveResumePosition() {
    if (m_currentFilePath.empty()) return;

    double pos = getPositionSeconds();''', '''void PlaybackController::saveResumePosition() {
    if (m_currentFilePath.empty()) return;

    double pos = getPlaybackPositionSeconds();''')
    replace(controller, 'static_cast<int64_t>(getPositionSeconds()),',
            'static_cast<int64_t>(getPlaybackPositionSeconds()),')
    replace(controller, '''bool PlaybackController::isPaused() const {
    return m_playbackFsm.getCurrentState() == PlaybackState::Paused;
}''', '''bool PlaybackController::isPaused() const {
    return m_playbackFsm.getCurrentState() == PlaybackState::Paused ||
        (isScrubbing() && (m_tubeScrubMode != 2 || m_tubeScrubWasPaused));
}''')
    replace(player + 'core/src/Application.cpp',
            'p->position = pb->getPositionSeconds(); p->duration = pb->getDurationSeconds();',
            'p->position = pb->getPlaybackPositionSeconds(); p->duration = pb->getDurationSeconds();')
    replace(player + 'core/include/evo/interfaces/IPlaybackController.hpp',
            '    virtual void tickScrubAutoCommit() = 0;',
            '    virtual void tickScrubAutoCommit() = 0;\n    virtual bool scrubNeedsConfirmation() const = 0;')
    replace(player + 'core/include/evo/services/PlaybackController.hpp',
            '    void tickScrubAutoCommit() override;',
            '    void tickScrubAutoCommit() override;\n    bool scrubNeedsConfirmation() const override;')
    replace(controller, 'void PlaybackController::tickScrubAutoCommit() {', '''bool PlaybackController::scrubNeedsConfirmation() const {
    return m_tubeScrubMode != 0;
}

void PlaybackController::tickScrubAutoCommit() {''')
    replace(player + 'core/include/evo/services/PlaybackController.hpp',
            '    // Scrub state', '''    // Scrub state
    bool m_tubeScrubWasPaused = false;
    int m_tubeScrubMode = 0;''')
    replace(controller,
            '.addTransition(PlaybackState::Scrubbing, PlaybackEvent::CancelScrub, PlaybackState::Playing)',
            '''.addTransition(PlaybackState::Scrubbing, PlaybackEvent::CancelScrub, PlaybackState::Paused,
                       [this]() { return m_tubeScrubWasPaused; })
        .addTransition(PlaybackState::Scrubbing, PlaybackEvent::CancelScrub, PlaybackState::Playing,
                       [this]() { return !m_tubeScrubWasPaused; })''')
    replace(controller, '    // From Scrubbing', '''    // Active provider engines can outlive the FSM's opening/seek/EOF state.
    // Enter preview directly: a synthetic Play would momentarily unpause them.
    m_playbackFsm
        .addTransition(PlaybackState::Opening, PlaybackEvent::StartScrub, PlaybackState::Scrubbing)
        .addTransition(PlaybackState::Stopped, PlaybackEvent::StartScrub, PlaybackState::Scrubbing)
        .addTransition(PlaybackState::Seeking, PlaybackEvent::StartScrub, PlaybackState::Scrubbing)
        .addTransition(PlaybackState::Finished, PlaybackEvent::StartScrub, PlaybackState::Scrubbing);

    // From Scrubbing''')
    replace(controller, '''void PlaybackController::setPaused(bool paused) {
    if (paused) {''', '''void PlaybackController::setPaused(bool paused) {
    if (isScrubbing()) {
        // Phone, voice or the sleep timer retires the preview at the current media position.
        // Use the guarded cancel transition directly, avoiding a transient resume on pause.
        m_tubeScrubWasPaused = paused;
        resetScrubHold(); pb_scrub_hold = 0;
        m_playbackFsm.postEvent(PlaybackEvent::CancelScrub);
        return;
    }
    if (paused) {''')
    replace(controller, '''void PlaybackController::beginScrub() {
    if (!isActive()) return;''', '''void PlaybackController::beginScrub() {
    if (!isActive() || isLiveSource() || isScrubbing() || evo_demux_is_seeking() ||
        g_pp_pb.seek_discarding || pb_prebuffer_hold) return;
    m_tubeScrubWasPaused = isPaused() || evo_pb_is_paused() != 0;
    m_tubeScrubMode = m_source.provider == "youtube" ? tube_player_seek_mode() : 0;''')
    replace(controller, '     * Only Playing and Paused may start a scrub.',
            '     * Preview normally starts from Playing or Paused; active provider engines also admit stale FSM states.')
    replace(controller, '''    pp_playback_pause(&g_pp_pb);
    /* Park demux's two consumers for the drag.''', '''    // A stale provider FSM may have needed the Play repair above; retain real pause ownership.
    if (m_tubeScrubWasPaused) player_paused = 1;
    if (m_tubeScrubMode != 2 || m_tubeScrubWasPaused) pp_playback_pause(&g_pp_pb);
    /* Park demux's two consumers for the drag.''')
    replace(controller, '''    pb_scrub_hold = 1;
    resetScrubHold();''', '''    pb_scrub_hold = m_tubeScrubMode == 2 ? 0 : 1;
    resetScrubHold();''')
    replace(controller, '''void PlaybackController::moveScrub(double deltaSeconds) {
    if (!isScrubbing()) {''', '''void PlaybackController::moveScrub(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || !isActive() || isLiveSource() ||
        evo_demux_is_seeking() || g_pp_pb.seek_discarding || pb_prebuffer_hold) return;
    if (!isScrubbing()) {''')
    replace(controller, '            seekTo(clampScrubTarget(getPositionSeconds() + deltaSeconds));',
            '            seekTo(clampScrubTarget(getPositionSeconds() + deltaSeconds), isPaused() || evo_pb_is_paused() != 0);')
    replace(controller, '    m_scrubAutoCommitDeadlineMs = GetCurrentTimeMs() + 600ULL; // auto-commit after 600ms idle',
            '    m_scrubAutoCommitDeadlineMs = m_tubeScrubMode == 0 ? GetCurrentTimeMs() + 600ULL : 0;')
    replace(controller, '''    pb_scrub_hold = 0;
    seekTo(target);

    m_playbackFsm.postEvent(PlaybackEvent::Play);
    return true;''', '''    pb_scrub_hold = 0;
    const bool accepted = seekTo(target, m_tubeScrubWasPaused);
    m_playbackFsm.postEvent(m_tubeScrubWasPaused ? PlaybackEvent::Pause : PlaybackEvent::Play);
    return accepted;''')
    replace(controller, '''    m_playbackFsm.postEvent(PlaybackEvent::CancelScrub);
    resetScrubHold();
    pb_scrub_hold = 0;
    pp_playback_resume(&g_pp_pb);''', '''    resetScrubHold();
    pb_scrub_hold = 0;
    m_playbackFsm.postEvent(PlaybackEvent::CancelScrub);''')
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, 'playback->moveScrub(-10.0);', 'playback->moveScrub(-playback->getSeekIntervalSeconds());')
    replace(screen, 'playback->moveScrub(10.0);', 'playback->moveScrub(playback->getSeekIntervalSeconds());')
    replace(screen, '), playback->getPositionSeconds());', '), playback->getPlaybackPositionSeconds());')
    replace(screen, 'if (ended && (tube_phone_finished()',
            'if (ended && !playback->isScrubbing() && !evo_demux_is_seeking() && !g_pp_pb.seek_discarding && (tube_phone_finished()')
    replace(screen, 'const bool hiddenCross = m_osdVisibilityAlpha <= 0 && (pressed & PadButtons::Cross);',
            'const bool hiddenCross = m_osdVisibilityAlpha <= 0 && !playback->isScrubbing() && (pressed & PadButtons::Cross);')
    replace(screen, '''    // Scrub hold update
    playback->updateScrubHold(held);''', '''    // Scrub hold update
    playback->updateScrubHold(held);
    // ProsperoTube seek input owns Cross/Circle before their normal actions.
    if (playback->isScrubbing() && (pressed & (PadButtons::Cross | PadButtons::Circle))) {
        m_lastPlayPauseToggleMs = NowMs();
        if (pressed & PadButtons::Circle) {
            playback->cancelScrub(); toast("SEEK", "Cancelled");
        } else if (playback->confirmScrub()) toast("SEEK", "Position selected");
        else toast("SEEK", "Unable to seek; playback state restored");
        return true;
    }
    if (playback->isScrubbing() && !playback->scrubNeedsConfirmation() &&
        (released & (PadButtons::Left | PadButtons::Right)) &&
        !(held & (PadButtons::Left | PadButtons::Right))) {
        if (!playback->confirmScrub()) toast("SEEK", "Unable to seek; playback state restored");
        return true;
    }
    // End ProsperoTube seek input.''')
    replace(player + 'ui_rml/src/evo_rmlui_provider.cpp', 'id != "settings:player-hide"',
            'id != "settings:player-hide" && id != "settings:player-seek" && id != "settings:seek-interval"')
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp',
            'KindCross, KindHide };', 'KindCross, KindHide, KindSeek, KindSeekInterval };')
    replace(picker, '    // End ProsperoTube chat option rows.', '''    // End ProsperoTube chat option rows.
    const char *seekLabels[] = {"Seeking: regular", "Seeking: confirm while paused", "Seeking: confirm while playing"};
    for (int mode = 0; mode < 3; ++mode) {
        Entry e; e.kind = KindSeek; e.streamIndex = mode; e.label = seekLabels[mode];
        e.detail = "Cross commits, Circle cancels; retain the previous pause state";
        e.badge = tube_player_seek_mode() == mode ? "SELECTED" : "";
        m_tracks.push_back(std::move(e));
    }
    for (int seconds : {1,2,3,5,7,10,15,20,30,60}) {
        Entry e; e.kind = KindSeekInterval; e.streamIndex = seconds;
        e.label = "Seek interval: " + std::to_string(seconds) + " seconds";
        e.detail = "Left / Right step; held seeking accelerates from this interval";
        e.badge = tube_player_seek_seconds() == seconds ? "SELECTED" : "";
        m_tracks.push_back(std::move(e));
    }''')
    replace(picker, '    if (e.kind == KindCross || e.kind == KindHide) {', '''    if (e.kind == KindSeekInterval) {
        toast("SEEK",tube_player_set_seek_seconds(e.streamIndex) ? e.label.c_str() : "Unable to save seek interval");
    } else if (e.kind == KindSeek) {
        toast("SEEK",tube_player_set_seek_mode(e.streamIndex) ? e.label.c_str() : "Unable to save seeking mode");
    } else if (e.kind == KindCross || e.kind == KindHide) {''')
    hui = player + 'ui_kit/src/evo_hui_osd.cpp'
    replace(hui, '    hints[n++] = {ui::Button::cross, raw_.paused ? "Play" : "Pause"};', '''    // ProsperoTube seek Cross hint.
    hints[n++] = {ui::Button::cross, raw_.scrub_active ? "Confirm seek" : raw_.paused ? "Play" : "Pause"};
    // End ProsperoTube seek Cross hint.''')
    replace(hui, '    hints[n++] = {ui::Button::circle, "Back"};', '''    // ProsperoTube seek Circle hint.
    hints[n++] = {ui::Button::circle, raw_.scrub_active ? "Cancel seek" : "Back"};
    // End ProsperoTube seek Circle hint.''')
    replace(player + 'assets/rml/playback.rml', '<span class="control-label">BACK</span>',
            '<span id="label-back" class="control-label">BACK</span>')
    rml = player + 'ui_rml/src/evo_rmlui_app.cpp'
    replace(rml, '''    Rml::Element* el_pp_label = m_playback_doc->GetElementById("label-playpause");
    if (el_pp_label) {
        el_pp_label->SetInnerRML(state.paused ? "PLAY" : "PAUSE");
    }''', '''    // ProsperoTube seek RML hints.
    Rml::Element* el_pp_label = m_playback_doc->GetElementById("label-playpause");
    if (el_pp_label) el_pp_label->SetInnerRML(state.scrub_active ? "CONFIRM SEEK" : state.paused ? "PLAY" : "PAUSE");
    if (auto* back = m_playback_doc->GetElementById("label-back"))
        back->SetInnerRML(state.scrub_active ? "CANCEL SEEK" : "BACK");
    // End ProsperoTube seek RML hints.''')
    replace(rml, '                    e->SetInnerRML(h + " \\xC2\\xB7 CROSS PAUSE \\xC2\\xB7 L/R SEEK");', '''                    // ProsperoTube seek music hint.
                    e->SetInnerRML(h + (state.scrub_active ? " · CROSS CONFIRM · CIRCLE CANCEL" :
                        state.paused ? " · CROSS PLAY · L/R SEEK" : " · CROSS PAUSE · L/R SEEK"));
                    // End ProsperoTube seek music hint.''')
