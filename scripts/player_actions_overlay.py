"""In-player actions: the video actions menu and its lists in the options
picker, player-button filtering, video presets, screen dimming and screen off,
the chapter notification and the error fixer. Applied by the player extras
overlay, whose picker and player blocks this extends."""


def _picker(replace, player):
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    header = player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp'
    replace(header, 'KindExtraGroup, KindExtraChoice, KindChapterList, KindChapter };',
            'KindExtraGroup, KindExtraChoice, KindChapterList, KindChapter,\n'
            '                KindTubeAction, KindTubeMenuRow, KindTubeNotice, KindSearch, KindScreenOff, KindPresetList, KindPreset };')
    replace(header, '    int m_tubeExtraGroup = -1;      /* -1 = all options, -2 = chapters, else one preference group */',
            '    int m_tubeExtraGroup = -1;      /* -1 = all options, -2 = chapters, -3 = a video actions list, -4 = presets, else one preference group */\n'
            '    int m_tubeMenuAction = -1;      /* the action row a list was opened from */\n'
            '    int m_tubeMenuSeen = -2;        /* list status and size the rows were built from */\n'
            '    int m_tubeMenuRows = 0;\n'
            '    int m_tubeMenuKeep = 0;         /* row to select once the list is ready */\n'
            '    bool m_tubeMenuRoot = false;    /* the list was reached through "Video actions" */\n'
            '    bool m_tubeMenuDirect = false;  /* the picker opened on the list: Circle returns to the video */')
    replace(picker, '#include "tube_player_extra.h"\n#include <cctype>',
            '#include "tube_player_extra.h"\n#include "evo_subtitle.h"\n#include "evo_net.h"\n#include <cctype>')
    replace(picker, '    StatefulScreen::onEnter();\n    m_tubeExtraGroup = -1;\n    refreshTracks();',
            '''    StatefulScreen::onEnter();
    m_tubeExtraGroup = -1;
    // ProsperoTube picker entry (extracted by host tests).
    m_tubeMenuAction = -1; m_tubeMenuSeen = -2; m_tubeMenuRows = 0; m_tubeMenuKeep = 0;
    m_tubeMenuRoot = m_tubeMenuDirect = false;
    if (tube_extra_take_picker_request() == TUBE_PICKER_SUGGESTIONS &&
        tube_player_menu_begin(TUBE_PLAYER_SUGGESTIONS) == TUBE_MENU_OPENED) {
        m_tubeExtraGroup = -3; m_tubeMenuAction = TUBE_PLAYER_SUGGESTIONS; m_tubeMenuDirect = true;
    }
    // End ProsperoTube picker entry.
    refreshTracks();''')
    replace(picker, 'void AudioTrackPickerScreen::onExit() {\n    StatefulScreen::onExit();',
            'void AudioTrackPickerScreen::onExit() {\n    tube_player_menu_close();   /* the browser\'s own list state returns */\n    StatefulScreen::onExit();')
    replace(picker, '    // End ProsperoTube extra option lists.', '''    // End ProsperoTube extra option lists.
    // ProsperoTube video action lists (extracted by host tests).
    if (m_tubeExtraGroup == -3) {
        const int status = tube_player_menu_status();
        m_tubeMenuSeen = status; m_tubeMenuRows = tube_player_menu_count();
        if (status != 1) {
            Entry e; e.kind = KindTubeNotice;
            char why[256] = "";
            if (status < 0 && !*tube_player_menu_message()) evo_failure_reason(why, sizeof why);
            e.label = status == 0 ? "Loading..." : *tube_player_menu_message() ? tube_player_menu_message()
                    : *why ? why : "This action is unavailable right now";
            e.detail = status == 0 ? "The video keeps playing; Circle cancels" : "Circle returns";
            m_tracks.push_back(std::move(e));
            return;
        }
        for (int i = 0; i < m_tubeMenuRows; ++i) {
            const tube_player_menu_row_t* row = tube_player_menu_row(i);
            Entry e; e.kind = KindTubeMenuRow; e.streamIndex = i;
            e.label = row->title; e.detail = row->detail;
            if (!row->folder) e.badge = "VIDEO";
            m_tracks.push_back(std::move(e));
        }
        return;
    }
    if (m_tubeExtraGroup == -4) {
        int height = 0, fps = 0, codec = 0;
        tube_player_quality(&height, &fps, &codec);
        const int selected = tube_preset_find(height, fps, codec);
        for (int i = 0; i < tube_preset_count(); ++i) {
            Entry e; e.kind = KindPreset; e.streamIndex = i;
            char label[48]; tube_preset_label(i, label, sizeof label);
            e.label = label;
            e.detail = i ? "Resolution, frame rate and codec for this profile" : "Any frame rate and codec up to the maximum quality";
            if (i == selected) { e.badge = "SELECTED"; m_activeIndex = i; }
            m_tracks.push_back(std::move(e));
        }
        return;
    }
    // End ProsperoTube video action lists.
    // ProsperoTube video action rows (extracted by host tests).
    if (playback->isActive() && !tube_phone_owns_media()) {
        static const struct { int action, button; } actions[] = {
            {TUBE_PLAYER_ACTIONS, -1}, {TUBE_PLAYER_LIKE, TUBE_BUTTON_LIKE}, {TUBE_PLAYER_DISLIKE, TUBE_BUTTON_DISLIKE},
            {TUBE_PLAYER_SUBSCRIBE, TUBE_BUTTON_SUBSCRIBE}, {TUBE_PLAYER_PLAYLIST_ADD, TUBE_BUTTON_PLAYLIST_ADD},
            {TUBE_PLAYER_QUEUE, TUBE_BUTTON_QUEUE}, {TUBE_PLAYER_SUGGESTIONS, -2}, {TUBE_PLAYER_PLAYLIST, -2},
            {TUBE_PLAYER_DESCRIPTION, TUBE_BUTTON_VIDEO_INFO}, {TUBE_PLAYER_SHARE, TUBE_BUTTON_SHARE},
            {TUBE_PLAYER_SPONSOR, TUBE_BUTTON_CONTENT_BLOCK}, {TUBE_PLAYER_CHANNEL, TUBE_BUTTON_CHANNEL}};
        for (const auto& action : actions) {
            /* -2: the lists SmartTube shows under the controls follow the suggestions switch. */
            if (action.button == -2 ? !tube_extra_suggestions() : action.button >= 0 && !tube_extra_button(action.button)) continue;
            char label[160], detail[160];
            if (!tube_player_action(action.action, label, sizeof label, detail, sizeof detail)) continue;
            Entry e; e.kind = KindTubeAction; e.streamIndex = action.action;
            e.label = label; e.detail = detail;
            m_tracks.push_back(std::move(e));
        }
        if (tube_extra_button(TUBE_BUTTON_SEARCH)) {
            Entry e; e.kind = KindSearch; e.label = "Search";
            e.detail = "Stop this video and search YouTube";
            m_tracks.push_back(std::move(e));
        }
        if (tube_extra_button(TUBE_BUTTON_SCREEN_OFF)) {
            Entry e; e.kind = KindScreenOff; e.label = "Screen off";
            e.detail = "Keep the sound; any button except Left and Right brings the picture back";
            m_tracks.push_back(std::move(e));
        }
    }
    {
        int height = 0, fps = 0, codec = 0;
        tube_player_quality(&height, &fps, &codec);
        char preset[48] = "Custom limits";
        tube_preset_label(tube_preset_find(height, fps, codec), preset, sizeof preset);
        Entry e; e.kind = KindPresetList; e.label = std::string("Video presets: ") + (*preset ? preset : "Custom limits");
        e.detail = "One resolution, frame rate and codec for this profile";
        m_tracks.push_back(std::move(e));
    }
    // End ProsperoTube video action rows.''')
    # Rows of switched-off buttons leave the list; the cursor follows its row.
    replace(picker, '''        if (toggleIndex >= 0) m_activeIndex = toggleIndex;
    }
}''', '''        if (toggleIndex >= 0) m_activeIndex = toggleIndex;
    }
    // ProsperoTube player buttons filter (extracted by host tests).
    {
        std::vector<Entry> shown;
        int active = 0;
        for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i) {
            const Entry& e = m_tracks[i];
            const int button = e.kind == KindSpeed ? TUBE_BUTTON_SPEED
                : (e.kind == KindResize || e.kind == KindZoom || e.kind == KindAspect || e.kind == KindPixel) ? TUBE_BUTTON_ZOOM
                : e.kind == KindPlaybackMode ? TUBE_BUTTON_REPEAT
                : e.kind == KindVideoNavigation ? (e.streamIndex > 0 ? TUBE_BUTTON_NEXT : TUBE_BUTTON_PREVIOUS)
                : e.kind == KindSeekInterval ? TUBE_BUTTON_SEEK_INTERVAL
                : e.kind == KindRotate ? TUBE_BUTTON_ROTATE : e.kind == KindMirror ? TUBE_BUTTON_FLIP
                : e.kind == KindChat ? TUBE_BUTTON_CHAT : -1;
            if (button >= 0 && !tube_extra_button(button)) continue;
            if (i == m_activeIndex) active = static_cast<int>(shown.size());
            shown.push_back(e);
        }
        if (shown.size() != m_tracks.size()) { m_tracks = std::move(shown); m_activeIndex = active; }
    }
    // End ProsperoTube player buttons filter.
}''')
    replace(picker, '    // ProsperoTube extra option selection (extracted by host tests).', '''    // ProsperoTube video action selection (extracted by host tests).
    if (e.kind == KindTubeNotice) return;
    if (e.kind == KindTubeAction || e.kind == KindTubeMenuRow || e.kind == KindSearch) {
        const bool action = e.kind == KindTubeAction;
        const tube_player_menu_row_t* row = e.kind == KindTubeMenuRow ? tube_player_menu_row(e.streamIndex) : nullptr;
        const bool more = row && !std::strcmp(row->id, "more:");
        int result = TUBE_MENU_LEAVE;
        if (e.kind == KindSearch) tube_player_request_search();
        else result = action ? tube_player_menu_begin(e.streamIndex) : tube_player_menu_open(e.streamIndex);
        if (result == TUBE_MENU_LEAVE) {
            /* A video or a place in the browser was chosen: the host starts it once playback has stopped. */
            playback->stopPlayback();
            if (sm) sm->returnFromPlayback();
            return;
        }
        if (result == TUBE_MENU_OPENED) {
            if (action) { m_tubeMenuAction = e.streamIndex; m_tubeMenuRoot = e.streamIndex == TUBE_PLAYER_ACTIONS; }
            m_tubeExtraGroup = -3;
            m_tubeMenuKeep = more ? e.streamIndex : 0;
            refreshTracks();
            m_selectedIndex = std::min(m_tubeMenuKeep, std::max(0, static_cast<int>(m_tracks.size()) - 1));
            m_scrollOffset = m_selectedIndex >= EVO_RMLUI_LIST_ROWS ? m_selectedIndex - EVO_RMLUI_LIST_ROWS + 1 : 0;
        } else if (action) toast("VIDEO ACTIONS", "This action is unavailable for this video");
        return;
    }
    if (e.kind == KindPresetList) {
        m_tubeExtraGroup = -4;
        refreshTracks();
        m_selectedIndex = m_activeIndex;
        m_scrollOffset = m_selectedIndex >= EVO_RMLUI_LIST_ROWS ? m_selectedIndex - EVO_RMLUI_LIST_ROWS + 1 : 0;
        return;
    }
    if (e.kind == KindScreenOff || e.kind == KindPreset) {
        if (e.kind == KindScreenOff) tube_extra_screen_off();
        else {
            int height = 0, fps = 0, codec = 0, limit = 0;
            tube_preset_at(e.streamIndex, &height, &fps, &codec);
            tube_player_quality(&limit, nullptr, nullptr);
            if (!tube_player_set_quality(height ? height : limit, fps, codec)) toast("VIDEO PRESETS", "Unable to save this preset");
            else if (playback->isActive() && !tube_phone_owns_media() &&
                     tube_player_reload(playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled)) {
                /* As in SmartTube the preset applies at once: the video reopens at its position. */
                toast("VIDEO PRESETS", (e.label + " - reloading this video").c_str());
                playback->stopPlayback();
                if (sm) sm->returnFromPlayback();
                return;
            } else toast("VIDEO PRESETS", (e.label + " - used from the next video").c_str());
        }
        if (sm && !sm->navigateBack()) sm->navigateTo(ScreenId::Player);
        return;
    }
    // End ProsperoTube video action selection.
    // ProsperoTube extra option selection (extracted by host tests).''')
    replace(picker, '''        if (playback->switchVideoVariant(e.streamIndex)) {
            toast("QUALITY", e.label.c_str());''', '''        if (playback->switchVideoVariant(e.streamIndex)) {
            toast("QUALITY", e.label.c_str());
            // ProsperoTube remembered quality: SmartTube keeps a chosen format for the next videos.
            if (tube_extra_remember_quality())
                for (const auto& v : playback->getVideoVariants())
                    if (v.streamIndex == e.streamIndex) tube_player_remember_height(v.height);''')
    replace(picker, '        // ProsperoTube extra option back (extracted by host tests).', '''        // ProsperoTube video action back (extracted by host tests).
        if (m_tubeExtraGroup == -3) {
            if (m_tubeMenuRoot && !tube_player_menu_root() && tube_player_menu_begin(TUBE_PLAYER_ACTIONS) == TUBE_MENU_OPENED) {
                /* From a page of the actions back to the actions themselves. */
                m_tubeMenuKeep = 0;
                refreshTracks();
                m_selectedIndex = m_scrollOffset = 0;
                return true;
            }
            tube_player_menu_close();
            m_tubeExtraGroup = -1;
            if (!m_tubeMenuDirect) {
                refreshTracks();
                m_selectedIndex = m_activeIndex;
                for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i)
                    if (m_tracks[i].kind == KindTubeAction && m_tracks[i].streamIndex == m_tubeMenuAction) m_selectedIndex = i;
                m_scrollOffset = m_selectedIndex >= EVO_RMLUI_LIST_ROWS ? m_selectedIndex - EVO_RMLUI_LIST_ROWS + 1 : 0;
                return true;
            }
        } else if (m_tubeExtraGroup == -4) {
            m_tubeExtraGroup = -1;
            refreshTracks();
            m_selectedIndex = m_activeIndex;
            for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i)
                if (m_tracks[i].kind == KindPresetList) m_selectedIndex = i;
            m_scrollOffset = m_selectedIndex >= EVO_RMLUI_LIST_ROWS ? m_selectedIndex - EVO_RMLUI_LIST_ROWS + 1 : 0;
            return true;
        }
        // End ProsperoTube video action back.
        // ProsperoTube extra option back (extracted by host tests).''')
    replace(picker, 'void AudioTrackPickerScreen::update(double deltaMs) {\n    StatefulScreen::update(deltaMs);',
            '''void AudioTrackPickerScreen::update(double deltaMs) {
    StatefulScreen::update(deltaMs);
    // ProsperoTube video action list refresh (extracted by host tests).
    if (m_tubeExtraGroup == -3 &&
        (tube_player_menu_status() != m_tubeMenuSeen || tube_player_menu_count() != m_tubeMenuRows)) {
        /* The answer arrived, or the list was retired: show it and keep the cursor inside. */
        refreshTracks();
        m_selectedIndex = std::min(m_tubeMenuKeep, std::max(0, static_cast<int>(m_tracks.size()) - 1));
        m_scrollOffset = m_selectedIndex >= EVO_RMLUI_LIST_ROWS ? m_selectedIndex - EVO_RMLUI_LIST_ROWS + 1 : 0;
    }
    // End ProsperoTube video action list refresh.''')
    replace(picker, '    params.title = m_tubeExtraGroup == -2 ? "CHAPTERS" : m_tubeExtraGroup >= 0 ? tube_extra_group_title(m_tubeExtraGroup) : "PLAYBACK OPTIONS";',
            '    params.title = m_tubeExtraGroup == -2 ? "CHAPTERS" : m_tubeExtraGroup == -3 ? tube_player_menu_title()\n'
            '        : m_tubeExtraGroup == -4 ? "VIDEO PRESETS" : m_tubeExtraGroup >= 0 ? tube_extra_group_title(m_tubeExtraGroup) : "PLAYBACK OPTIONS";')


def _player_screen(replace, player):
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    header = player + 'core/include/evo/screens/PlayerScreen.hpp'
    replace(header, '#include "evo/screens/StatefulScreen.hpp"', '#include "evo/screens/StatefulScreen.hpp"\n#include "tube_player_extra.h"')
    replace(header, '    bool m_tubeFailureShown = false;', '    bool m_tubeFailureShown = false;\n    tube_buffering_t m_tubeBuffering = {};   /* error fixer: long buffering */')
    replace(screen, '    m_osdVisibilityAlpha = 255;\n    m_tubeSuppressControls = false;',
            '    m_osdVisibilityAlpha = 255;\n    tube_buffering_reset(&m_tubeBuffering);\n    m_tubeSuppressControls = false;')
    # Dimming draws through the HUD, so a dimmed picture always has one.
    replace(screen, '    if (tube_player_chat_mode()) return true;\n    auto reviewPlayback',
            '    if (tube_player_chat_mode() || tube_extra_dim_percent() > 0) return true;\n    auto reviewPlayback')
    replace(screen, 'tube_player_chat_mode() || tube_sponsor_prompt(playback->getPositionSeconds())) && !playback->isMusicMode() &&',
            'tube_player_chat_mode() || tube_extra_dim_percent() > 0 || tube_sponsor_prompt(playback->getPositionSeconds())) && !playback->isMusicMode() &&')
    replace(screen, '    // End ProsperoTube player gain and audio delay.', '''    // End ProsperoTube player gain and audio delay.
    // ProsperoTube chapter notification (extracted by host tests).
    {
        const bool chapterBusy = playback->isPaused() || playback->isScrubbing() || playback->isLiveSource() ||
            evo_demux_is_seeking() || g_pp_pb.seek_discarding || pb_prebuffer_hold;
        const int chapter = tube_extra_chapter_notice(playback->getPositionSeconds(), chapterBusy, m_osdVisibilityAlpha > 0);
        if (chapter >= 0) {
            char label[512];
            tube_extra_chapter_label(chapter, label, sizeof label);
            const char* title = tube_extra_chapter_title(chapter);
            toast("CHAPTER - R1 SKIPS IT", *title ? title : label);
        }
    }
    // End ProsperoTube chapter notification.''')
    replace(screen, '''    if (evo_pb_decode_fatal()) {
        if (evo_audio_has_failed())''', '''    if (evo_pb_decode_fatal()) {
        // ProsperoTube error fixer: decoder failure (extracted by host tests).
        if (!tube_phone_owns_media()) {
            /* The stop below returns to the browser, which reopens the video in the format the fixer asked for. */
            const int error = evo_audio_has_failed() ? TUBE_ERROR_AUDIO_DECODER : TUBE_ERROR_VIDEO_DECODER;
            const int fix = tube_error_fix_begin(error, playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled);
            if (fix) toast("PLAYBACK", tube_error_fix_message(error, fix));
        }
        // End ProsperoTube error fixer: decoder failure.
        if (evo_audio_has_failed())''')
    replace(screen, '''    if (isBuffering) {
        m_controlsLastUsedMs = now;
    }''', '''    if (isBuffering) {
        m_controlsLastUsedMs = now;
    }
    // ProsperoTube error fixer: long buffering (extracted by host tests).
    if (playback->isScrubbing() || evo_demux_is_seeking() || g_pp_pb.seek_discarding) tube_buffering_reset(&m_tubeBuffering);
    else if (tube_buffering_tick(&m_tubeBuffering, now, !playback->isMusicMode() && !reallyPaused && !ended &&
                                 (pb_prebuffer_hold || stalled)) && !tube_phone_owns_media()) {
        const int fix = tube_error_fix_begin(TUBE_ERROR_BUFFERING, playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled);
        if (fix || tube_extra_error_fixer()) toast("PLAYBACK", tube_error_fix_message(TUBE_ERROR_BUFFERING, fix));
        if (fix) {
            playback->stopPlayback();
            if (auto sm = Application::getInstance().getScreenManager()) sm->returnFromPlayback();
            return;
        }
    }
    // End ProsperoTube error fixer: long buffering.''')


def _host(replace, player):
    host = player + 'core/src/screens/ProviderHostScreen.cpp'
    # A reload that asked for another quality takes the stream the companion lists first.
    replace(host, '        int selected = ok ? tube_recovery_choice(choices, count) : -1;',
            '        int selected = !ok ? -1 : tube_error_fix_relaxed() ? tube_error_fix_choice(choices, count)\n'
            '                                                         : tube_recovery_choice(choices, count);')
    replace(host, '''    } else if (tube_take_next(&queued)) {
        g_tube_recovery_launch = false;''', '''    } else if (tube_player_take_launch(&queued) || tube_take_next(&queued)) {
        g_tube_recovery_launch = false;''')
    replace(host, '    // End ProsperoTube unavailable phone wait.', '''    // End ProsperoTube unavailable phone wait.
    // ProsperoTube player browse request: Open channel and Search left the player for this.
    if (m_providerId == "youtube") {
        char target[160], crumb[192];
        const int browse = tube_player_take_browse(target, sizeof target, crumb, sizeof crumb);
        if (browse == 1) { evo_rmlui_provider_push_folder(target, crumb); return; }
        if (browse == 2) { openSearch(); return; }
    }
    // End ProsperoTube player browse request.''')
    ui = player + 'ui_rml/src/evo_rmlui_provider.cpp'
    ui_header = player + 'ui_rml/include/evo_rmlui_provider.h'
    replace(ui_header, '    void ReplaceFolder(const char* parent, const char* title);',
            '    void ReplaceFolder(const char* parent, const char* title);\n    void PushFolder(const char* parent, const char* title);')
    replace(ui_header, 'void evo_rmlui_provider_replace_folder(const char* parent, const char* title);',
            'void evo_rmlui_provider_replace_folder(const char* parent, const char* title);\n'
            '/* Open a folder one level above the list on screen; Back returns to that list. */\n'
            'void evo_rmlui_provider_push_folder(const char* parent, const char* title);')
    replace(ui, 'void EvoRmlProviderHost::ReplaceFolder(const char* parent, const char* title)\n{',
            '''void EvoRmlProviderHost::PushFolder(const char* parent, const char* title)
{
    // ProsperoTube pushed folder
    if (!parent || !*parent || m_provider_id != "youtube") return;
    const auto* row = GetFocusedRow();
    const int slot = row ? row->index : 0;
    if (!m_model.query.empty() && m_stack.size() == m_search_depth && *tube_search_folder()) {
        /* Typed results stay a level of their own under the new folder. */
        m_pos_stack.push_back({0, 0});
        m_stack.push_back(tube_search_folder());
        m_crumbs.push_back(std::string("Search: ") + m_model.query.c_str());
        m_model.query = "";
        if (m_model_handle) m_model_handle.DirtyVariable("query");
    }
    m_pos_stack.push_back({m_row_offset, slot});
    m_stack.push_back(parent); m_crumbs.push_back(title && *title ? title : "Channel");
    m_page = 0; RequestPage(m_stack.back().c_str(), 0);
    // End ProsperoTube pushed folder
}

void EvoRmlProviderHost::ReplaceFolder(const char* parent, const char* title)
{''')
    replace(ui, 'void evo_rmlui_provider_replace_folder(const char* parent, const char* title)\n{',
            'void evo_rmlui_provider_push_folder(const char* parent, const char* title)\n{\n'
            '    EvoRmlProviderHost::Instance().PushFolder(parent, title);\n}\n\n'
            'void evo_rmlui_provider_replace_folder(const char* parent, const char* title)\n{')


def _dimming(replace, player):
    app = player + 'core/src/Application.cpp'
    replace(app, '#include "evo/screens/ProviderHostScreen.hpp"\n#include "tube_provider.h"',
            '#include "evo/screens/ProviderHostScreen.hpp"\n#include "tube_provider.h"\n#include "tube_player_extra.h"')
    # The press that brings the picture back does nothing else.
    replace(app, '            hasInput = (pressed != 0 || released != 0 || evo_input_any(&evo_pad_state));\n',
            '''            hasInput = (pressed != 0 || released != 0 || evo_input_any(&evo_pad_state));
            // ProsperoTube screen dimming input.
            if (tube_extra_dim_input(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()), pressed != 0,
                    pressed != 0 && !(pressed & ~(PadButtons::Left | PadButtons::Right)))) {
                pressed = 0; released = 0; held = 0;
            }
            // End ProsperoTube screen dimming input.
''')
    # A phone command is input too: it brings the picture back and is still carried out.
    replace(app, '            phone.activity = [](void*) { tube_sleep_activity(); };', '''            phone.activity = [](void*) {
                tube_sleep_activity();
                tube_extra_dim_input(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()), 1, 0);
            };''')
    replace(app, '''                toast("SLEEP TIMER", "Playback paused after inactivity");
            }
        }''', '''                toast("SLEEP TIMER", "Playback paused after inactivity");
            }
        }
        // ProsperoTube screen dimming: one idle clock for the browser and the player.
        bool tubeDimChanged = false;
        {
            int context = TUBE_DIM_NONE;
            const ScreenId current = m_screenManager->getCurrentScreenId();
            if (m_softClose || evo_keyboard_is_open() || ProviderHostScreen::playbackStartupBusy() || tube_player_dim_blocked()) context = TUBE_DIM_NONE;
            else if (current == ScreenId::Player) {
                auto playback = getPlaybackController();
                context = playback && playback->isActive() && !playback->isPaused() && !evo_pb_is_paused() && !evo_pb_is_complete()
                    ? TUBE_DIM_PLAYING : TUBE_DIM_PAUSED;
            } else if (current == ScreenId::EmbyBrowse) context = TUBE_DIM_BROWSING;
            const int before = tube_extra_dim_percent();
            tubeDimChanged = tube_extra_dim_tick(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()), context) != before;
        }
        // End ProsperoTube screen dimming.''')
    replace(app, '                       evo_rmlui_provider_needs_frame() || (jb_repaint > 0);',
            '                       evo_rmlui_provider_needs_frame() || (jb_repaint > 0) || tubeDimChanged;')
    ui = player + 'ui_rml/src/evo_rmlui_provider.cpp'
    replace(ui, '#include "evo_net.h"   /* evo_failure_reason */', '#include "evo_net.h"   /* evo_failure_reason */\n#include "tube_player_extra.h"')
    replace(ui, 'void EvoRmlProviderHost::Tick()\n{\n    if (!IsOpen()) return;\n', '''void EvoRmlProviderHost::Tick()
{
    if (!IsOpen()) return;
    // ProsperoTube screen dimming
    if (m_provider_id == "youtube" && m_doc) {
        static int dim_shown = -1;
        static const void* dim_document = nullptr;
        const int percent = tube_extra_dim_percent();
        if (percent != dim_shown || dim_document != m_doc) {
            if (Rml::Element* dim = m_doc->GetElementById("tube-dim")) {
                char value[16];
                std::snprintf(value, sizeof value, "#000000%02x", percent * 255 / 100);
                dim->SetProperty("background-color", value);
                dim->SetClass("shown", percent > 0);
            }
            dim_shown = percent; dim_document = m_doc; m_dirty = true;
        }
    }
    // End ProsperoTube screen dimming
''')
    replace(player + 'assets/rml/youtube.rml', '</body>',
            '    <!-- Screen dimming while idle: black at the chosen strength over the whole screen -->\n    <div id="tube-dim"></div>\n</body>')
    replace(player + 'assets/rml/youtube.rcss', 'body { margin: 0;',
            '#tube-dim { display: none; position: absolute; left: 0; top: 0; width: 1920dp; height: 1080dp; z-index: 100; background-color: #000000; }\n'
            '#tube-dim.shown { display: block; }\nbody { margin: 0;')


def apply_player_actions_overlay(replace, player):
    _picker(replace, player)
    _player_screen(replace, player)
    _host(replace, player)
    _dimming(replace, player)
