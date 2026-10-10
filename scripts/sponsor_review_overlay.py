"""Transparent SponsorBlock review in both shipping HUD renderers."""


def apply_sponsor_review_overlay(replace, player):
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, '    if (tube_sponsor_prompt(playback->getPositionSeconds()) &&',
            '    if (!m_showStatsForNerds && !playback->isScrubbing() &&\n        tube_sponsor_prompt(playback->getPositionSeconds()) &&')
    replace(screen, 'bool PlayerScreen::hasActiveOverlay() const {', '''bool PlayerScreen::hasActiveOverlay() const {
    auto reviewPlayback = Application::getInstance().getPlaybackController();
    if (reviewPlayback && !reviewPlayback->isMusicMode() && !reviewPlayback->isLiveSource() &&
        tube_sponsor_prompt(reviewPlayback->getPositionSeconds())) return true;''')
    replace(screen, '        p.chrome_hidden = 0;', '''        p.chrome_hidden = !m_showStatsForNerds && !playback->isScrubbing() && !playback->isLiveSource() &&
            tube_sponsor_prompt(playback->getPositionSeconds()) ? 1 : 0;''')
    replace(screen, '    } else if ((activeSubText[0] || secondarySubText[0]) && !playback->isMusicMode() &&',
            '    } else if ((activeSubText[0] || secondarySubText[0] || tube_sponsor_prompt(playback->getPositionSeconds())) && !playback->isMusicMode() &&')
    replace(screen, '        p.title = "Video Playback";', '''        p.title = "Video Playback";
        p.position_sec = playback->getPositionSeconds();
        p.duration_sec = playback->getDurationSeconds();
        p.paused = playback->isPaused() ? 1 : 0;
        p.is_live = playback->isLiveSource() ? 1 : 0;''')
    kit = player + 'ui_kit/src/evo_hui_osd.cpp'
    replace(kit, '    if (hud_on_)\n        draw_stats(list, ctx);', '''    // ProsperoTube SponsorBlock review draw (extracted by host tests).
    const char *review = !raw_.show_stats && !raw_.scrub_active && !raw_.music_mode && !raw_.is_live
        ? tube_sponsor_prompt(raw_.position_sec) : nullptr;
    if (review) {
        const Rect panel{720, 156, 1104, 136};
        list.rounded_rect(panel, 18, kBlack.with_alpha(0.72f));
        list.bordered_rect(panel, 18, kClear, 2, ctx.palette.accent.with_alpha(0.85f));
        ui::text(list, fonts.semibold, "SPONSORBLOCK", panel.x + 24, panel.y + 38, 18, ctx.palette.accent);
        ui::text(list, fonts.regular, fit(fonts.regular, review, 24, panel.w - 48),
                 panel.x + 24, panel.y + 88, 24, kWhite);
    }
    // End ProsperoTube SponsorBlock review draw.
    if (hud_on_)
        draw_stats(list, ctx);''')
    replace(kit, '    /* The player has no cursor; report the state the remote tests look for. */', '''    // ProsperoTube SponsorBlock review focus (extracted by host tests).
    const char *review = !raw_.show_stats && !raw_.scrub_active && !raw_.music_mode && !raw_.is_live
        ? tube_sponsor_prompt(raw_.position_sec) : nullptr;
    if (review) {
        out->focusable.clear(); out->id = "sponsor-review"; out->text = review;
        out->index = out->total = 1; out->rect = {720, 156, 1104, 136}; return true;
    }
    // End ProsperoTube SponsorBlock review focus.
    /* The player has no cursor; report the state the remote tests look for. */''')
    app = player + 'ui_rml/src/evo_rmlui_app.cpp'
    replace(app, '    // End ProsperoTube SponsorBlock marker RML.', '''    // End ProsperoTube SponsorBlock marker RML.
    // ProsperoTube SponsorBlock review RML (extracted by host tests).
    if (auto* el = m_playback_doc->GetElementById("sponsor-review")) {
        const char *review = !m_last_state.show_stats && !m_last_state.scrub_active && !m_last_state.music_mode && !m_last_state.is_live
            ? tube_sponsor_prompt(m_last_state.position_sec) : nullptr;
        el->SetProperty("display", review ? "block" : "none");
        if (auto* text = m_playback_doc->GetElementById("sponsor-review-text")) {
            std::string escaped;
            for (const char *p = review; p && *p; ++p) {
                if (*p == '&') escaped += "&amp;";
                else if (*p == '<') escaped += "&lt;";
                else if (*p == '>') escaped += "&gt;";
                else escaped += *p;
            }
            if (text->GetInnerRML() != escaped) text->SetInnerRML(escaped);
        }
    }
    // End ProsperoTube SponsorBlock review RML.''')
    replace(player + 'assets/rml/playback.rml', '</body>', '''    <div id="sponsor-review">
        <div id="sponsor-review-title">SPONSORBLOCK</div>
        <div id="sponsor-review-text"></div>
    </div>
</body>''')
    replace(player + 'assets/rml/playback.rcss', '/* Top Gradient Scrim */', '''#sponsor-review {
    display: none;
    position: absolute;
    left: 720dp;
    top: 156dp;
    width: 1104dp;
    height: 136dp;
    padding: 20dp 24dp;
    border: 2dp #4dd8ff;
    border-radius: 18dp;
    background-color: rgba(0, 0, 0, 184);
}
#sponsor-review-title { font-size: 18dp; color: #4dd8ff; margin-bottom: 22dp; }
#sponsor-review-text { font-size: 24dp; color: #ffffff; }

/* Top Gradient Scrim */''')
