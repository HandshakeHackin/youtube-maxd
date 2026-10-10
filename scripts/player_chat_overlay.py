"""Request-driven live chat beside video in both shipping HUD renderers."""


def apply_player_chat_overlay(replace, player):
    interface = player + 'core/include/evo/interfaces/IPlaybackController.hpp'
    header = player + 'core/include/evo/services/PlaybackController.hpp'
    controller = player + 'core/src/services/PlaybackController.cpp'
    replace(interface, '    virtual bool canChangePlaybackSpeed() const = 0;',
            '    virtual bool canChangePlaybackSpeed() const = 0;\n    virtual bool canShowLiveChat() const = 0;')
    replace(header, '    bool canChangePlaybackSpeed() const override;',
            '    bool canChangePlaybackSpeed() const override;\n    bool canShowLiveChat() const override;')
    replace(controller, 'bool PlaybackController::canChangePlaybackSpeed() const {', '''bool PlaybackController::canShowLiveChat() const {
    return m_source.provider == "youtube" && !m_source.item_id.empty() &&
        !m_source.url.empty() && !isMusicMode();
}
bool PlaybackController::canChangePlaybackSpeed() const {''')
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp',
            'KindAudio, KindSpeed };', 'KindAudio, KindSpeed, KindChat };')
    replace(picker, '    if (playback->canChangePlaybackSpeed()) {', '''
    // ProsperoTube chat option rows (extracted by host tests).
    if (playback->canShowLiveChat()) {
        const char* labels[] = {"Hide live chat", "Live chat: left", "Live chat: right"};
        for (int mode = 0; mode < 3; ++mode) {
            Entry e; e.kind = KindChat; e.streamIndex = mode;
            e.label = labels[mode];
            e.detail = "Show top chat beside video; select again to retry unavailable chat";
            e.badge = tube_player_chat_preference() == mode ? "SELECTED" : "";
            m_tracks.push_back(std::move(e));
        }
    }
    // End ProsperoTube chat option rows.
    if (playback->canChangePlaybackSpeed()) {''')
    replace(picker, '    if (e.kind == KindSpeed) {', '''    if (e.kind == KindChat) {
        // ProsperoTube chat option selection (extracted by host tests).
        if (playback->canShowLiveChat() && tube_player_chat_set_mode(e.streamIndex))
            toast("CHAT", e.label.c_str());
        else toast("CHAT", "Chat is unavailable for this playback");
        // End ProsperoTube chat option selection.
    } else if (e.kind == KindSpeed) {''')
    replace(picker, 'Select quality, audio or playback speed',
            'Select quality, audio, playback speed or live chat')
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, 'void PlayerScreen::onExit() {',
            'void PlayerScreen::onExit() {\n    tube_player_chat_tick(0);')
    replace(screen, '    playback->tickScrubAutoCommit();', '''    const auto chatState = playback->getPlaybackState();
    tube_player_chat_tick(!playback->isMusicMode() &&
        (chatState == PlaybackState::Playing || chatState == PlaybackState::Paused ||
         chatState == PlaybackState::Scrubbing || chatState == PlaybackState::Seeking));
    playback->tickScrubAutoCommit();''')
    replace(screen, 'bool PlayerScreen::hasActiveOverlay() const {',
            'bool PlayerScreen::hasActiveOverlay() const {\n    if (tube_player_chat_mode()) return true;')
    replace(screen, 'activeSubText[0] || secondarySubText[0] || tube_sponsor_prompt(playback->getPositionSeconds())',
            'activeSubText[0] || secondarySubText[0] || tube_player_chat_mode() || tube_sponsor_prompt(playback->getPositionSeconds())')
    kit = player + 'ui_kit/src/evo_hui_osd.cpp'
    replace(kit, '    // End ProsperoTube SponsorBlock review draw.', '''    // End ProsperoTube SponsorBlock review draw.
    // ProsperoTube player chat draw (extracted by host tests).
    const int chatMode = !raw_.show_stats && !raw_.scrub_active && !raw_.music_mode ? tube_player_chat_mode() : 0;
    if (chatMode) {
        const Rect panel{chatMode == 1 ? 24.0f : 1456.0f, 180, 440, 560};
        list.rounded_rect(panel, 18, kBlack.with_alpha(0.72f));
        ui::text(list, fonts.semibold, "TOP CHAT", panel.x + 16, panel.y + 34, 18, ctx.palette.accent);
        const tube_player_chat_row_t *rows = nullptr;
        const int count = tube_player_chat_rows(&rows);
        if (!count) ui::text(list, fonts.regular, fit(fonts.regular, tube_player_chat_status(), 18, panel.w - 32),
            panel.x + 16, panel.y + 72, 18, kWhite);
        for (int i = 0; rows && i < count && i < 6; ++i) {
            const float y = panel.y + 76 + i * 76;
            ui::text(list, fonts.semibold, fit(fonts.semibold, rows[i].author, 16, panel.w - 32), panel.x + 16, y, 16, ctx.palette.accent);
            ui::text(list, fonts.regular, fit(fonts.regular, rows[i].message, 18, panel.w - 32), panel.x + 16, y + 28, 18, kWhite);
        }
    }
    // End ProsperoTube player chat draw.''')
    app = player + 'ui_rml/src/evo_rmlui_app.cpp'
    replace(app, '    // End ProsperoTube SponsorBlock review RML.', '''    // End ProsperoTube SponsorBlock review RML.
    // ProsperoTube player chat RML (extracted by host tests).
    if (auto* panel = m_playback_doc->GetElementById("player-chat")) {
        const int mode = !m_last_state.show_stats && !m_last_state.scrub_active && !m_last_state.music_mode ? tube_player_chat_mode() : 0;
        panel->SetProperty("display", mode ? "block" : "none");
        panel->SetProperty("left", mode == 1 ? "24dp" : "1456dp");
        const auto escape = [](const char *value) {
            std::string escaped;
            for (const char *p = value; p && *p; ++p) {
                if (*p == '&') escaped += "&amp;";
                else if (*p == '<') escaped += "&lt;";
                else if (*p == '>') escaped += "&gt;";
                else escaped += *p;
            }
            return escaped;
        };
        const tube_player_chat_row_t *rows = nullptr;
        const int count = mode ? tube_player_chat_rows(&rows) : 0;
        if (auto* status = m_playback_doc->GetElementById("player-chat-status")) {
            const std::string text = mode && !count ? escape(tube_player_chat_status()) : "";
            if (status->GetInnerRML() != text) status->SetInnerRML(text);
        }
        for (int i = 0; i < 6; ++i) {
            if (auto* row = m_playback_doc->GetElementById("player-chat-row-" + std::to_string(i))) {
                const std::string text = rows && i < count ? "<b>" + escape(rows[i].author) + "</b><br/>" + escape(rows[i].message) : "";
                row->SetProperty("display", text.empty() ? "none" : "block");
                if (row->GetInnerRML() != text) row->SetInnerRML(text);
            }
        }
    }
    // End ProsperoTube player chat RML.''')
    replace(player + 'assets/rml/playback.rml', '</body>', '''    <div id="player-chat">
        <div id="player-chat-title">TOP CHAT</div><div id="player-chat-status"></div>
        <div id="player-chat-row-0" class="player-chat-row"></div><div id="player-chat-row-1" class="player-chat-row"></div>
        <div id="player-chat-row-2" class="player-chat-row"></div><div id="player-chat-row-3" class="player-chat-row"></div>
        <div id="player-chat-row-4" class="player-chat-row"></div><div id="player-chat-row-5" class="player-chat-row"></div>
    </div>
</body>''')
    replace(player + 'assets/rml/playback.rcss', '/* Top Gradient Scrim */', '''#player-chat {
    display: none; position: absolute; left: 1456dp; top: 180dp; width: 408dp; height: 528dp;
    padding: 16dp; border-radius: 18dp; background-color: rgba(0, 0, 0, 184);
}
#player-chat-title { font-size: 18dp; color: #4dd8ff; margin-bottom: 18dp; }
#player-chat-status { font-size: 18dp; color: #ffffff; }
.player-chat-row { font-size: 18dp; color: #ffffff; height: 68dp; margin-bottom: 8dp; overflow: hidden; }
.player-chat-row b { color: #4dd8ff; font-size: 16dp; }

/* Top Gradient Scrim */''')
