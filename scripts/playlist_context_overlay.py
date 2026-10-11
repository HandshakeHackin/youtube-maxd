"""Carry playlist occurrence metadata through actual provider rows and launch staging."""
def apply_playlist_context_overlay(replace, player, host):
    replace(player+'core/src/services/PlaybackController.cpp',
        '    /* The demux thread owns seek begin/end. Announcing begin here, after',
        '''    if (m_source.provider=="youtube") tube_completion_seek_started(targetSeconds);
    /* The demux thread owns seek begin/end. Announcing begin here, after''')
    replace(player+'core/src/services/PlaybackController.cpp',
        '        .addTransition(PlaybackState::Finished, PlaybackEvent::Open, PlaybackState::Opening)',
        '''        .addTransition(PlaybackState::Finished, PlaybackEvent::Pause, PlaybackState::Paused,
                       [this]() { return isActive(); })
        .addTransition(PlaybackState::Finished, PlaybackEvent::Open, PlaybackState::Opening)''')
    picker=player+'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(player+'core/include/evo/screens/AudioTrackPickerScreen.hpp',
        'KindRotate, KindMirror };','KindRotate, KindMirror, KindPlaybackMode };')
    replace(picker,'    // End ProsperoTube chat option rows.','''    // End ProsperoTube chat option rows.
    // ProsperoTube playback mode rows.
    const int playbackModes[]={2,3,4,5,6,0,1};
    const char* playbackLabels[]={"Continuous playback","Repeat one","Shuffle playlist","Playlist only","Reverse playlist","Stop at end","Close at end"};
    for(int i=0;i<7;++i) {
        Entry e;e.kind=KindPlaybackMode;e.streamIndex=playbackModes[i];
        e.label=playbackLabels[i];e.detail="Playback mode; saved for this profile";
        e.badge=tube_playback_mode()==playbackModes[i]?"SELECTED":"";
        m_tracks.push_back(std::move(e));
    }
    // End ProsperoTube playback mode rows.''')
    replace(picker,'    } else if (e.kind == KindSleep) {','''    } else if (e.kind == KindPlaybackMode) {
        if (tube_set_playback_mode(e.streamIndex)) toast("PLAYBACK MODE",e.label.c_str());
    } else if (e.kind == KindSleep) {''')
    screen=player+'core/src/screens/PlayerScreen.cpp'
    replace(screen, '    const bool ended = evo_pb_is_complete() != 0;', '''    tube_completion_seek_observe(playback->getPositionSeconds(), playback->isScrubbing() ||
        evo_demux_is_seeking() || g_pp_pb.seek_discarding || pb_prebuffer_hold);
    const bool ended = evo_pb_is_complete() != 0 || tube_sponsor_completed();
    const bool completionBusy=playback->isScrubbing() || evo_demux_is_seeking() ||
        g_pp_pb.seek_discarding || pb_prebuffer_hold;
    if (!tube_phone_owns_media()) tube_completion_playback_state(ended,completionBusy);''')
    replace(screen, '    if (!ended && !evo_pb_is_paused() && !playback->isScrubbing() && !playback->isLiveSource() &&', '''    if (ended && !completionBusy && !tube_phone_owns_media()) {
        const int action=tube_take_completion_action();
        if (action==TUBE_COMPLETION_REPEAT) {
            if (!playback->seekTo(0.0,false)) {
                playback->setPaused(true);
                toast("PLAYBACK", "Could not restart this video");
            } else playback->setPaused(false);
        } else if (action==TUBE_COMPLETION_PAUSE) playback->setPaused(true);
    }
    if (!ended && !evo_pb_is_paused() && !playback->isScrubbing() && !playback->isLiveSource() &&''')
    replace(player+'ui_rml/src/evo_rmlui_provider.cpp', 'id != "settings:video-rotate"',
        'id != "settings:video-rotate" && id != "settings:playback-mode"')
    replace(host, 'if (!tube_recovery_pending() && rp &&',
        'if (!tube_recovery_pending() && !tube_playlist_eof_pending() && rp &&')
    replace(host, '        sel.is_live = queued.is_live; sel.resume_sec = queued.resume_pos_sec;', '''        sel.is_live = queued.is_live; sel.resume_sec = queued.resume_pos_sec;
        std::snprintf(sel.playback_playlist,sizeof sel.playback_playlist,"%s",queued.playback_playlist);
        sel.playback_index=queued.playback_index; sel.playback_scope=queued.playback_scope;''')
    replace(player+'core/src/screens/PlayerScreen.cpp',
        'bool PlayerScreen::handleInput(uint32_t pressed, uint32_t held, uint32_t released) {',
        '''bool PlayerScreen::handleInput(uint32_t pressed, uint32_t held, uint32_t released) {
    if (pressed && tube_playlist_eof_pending()) tube_playlist_eof_cancel();''')
    fields='''    char playback_playlist[101];
    int playback_index;
    unsigned playback_scope;
'''
    replace(player+'addons/include/evo_provider.h', '    char    now_title[128];', fields+'    char    now_title[128];')
    header=player+'ui_rml/include/evo_rmlui_provider.h'
    replace(header, '    long long resume_sec = 0;', '''    long long resume_sec = 0;
    Rml::String playback_playlist;
    int playback_index = 0;
    unsigned playback_scope = 0;''')
    replace(header, '    long long resume_sec;   /* where the service says playback stopped, 0 = start */',
        '    long long resume_sec;   /* where the service says playback stopped, 0 = start */\n'+fields)
    ui=player+'ui_rml/src/evo_rmlui_provider.cpp'
    replace(ui, '        r.resume_sec = s.resume_pos_sec > 0 ? (long long)s.resume_pos_sec : 0;', '''        r.resume_sec = s.resume_pos_sec > 0 ? (long long)s.resume_pos_sec : 0;
        r.playback_playlist=s.playback_playlist;
        r.playback_index=s.playback_index; r.playback_scope=s.playback_scope;''')
    replace(ui, '    g_selection.resume_sec = hit->resume_sec;', '''    g_selection.resume_sec = hit->resume_sec;
    snprintf(g_selection.playback_playlist,sizeof g_selection.playback_playlist,"%s",hit->playback_playlist.c_str());
    g_selection.playback_index=hit->playback_index; g_selection.playback_scope=hit->playback_scope;''')
    replace(header, '    Rml::String playback_playlist;', '    Rml::String playback_playlist;\n    Rml::String selection_key;')
    replace(ui, '        row.RegisterMember("id",        &EvoProviderRow::id);',
        '        row.RegisterMember("id",        &EvoProviderRow::id);\n        row.RegisterMember("selection_key", &EvoProviderRow::selection_key);')
    replace(ui, '        r.playback_index=s.playback_index; r.playback_scope=s.playback_scope;', '''        r.playback_index=s.playback_index; r.playback_scope=s.playback_scope;
        r.selection_key=r.id;
        if (!r.playback_playlist.empty() && r.playback_scope)
            r.selection_key=r.id + "|" + std::to_string(r.playback_scope) + "|" + r.playback_playlist + "|" + std::to_string(r.playback_index);''')
    replace(ui, '''            if (Rml::Element* el = ev.GetCurrentElement())
                id = el->GetAttribute<Rml::String>("rowid", Rml::String());''', '''            if (Rml::Element* el = ev.GetCurrentElement()) {
                id = el->GetAttribute<Rml::String>("rowkey", Rml::String());
                if (id.empty()) id=el->GetAttribute<Rml::String>("rowid", Rml::String());
            }''')
    replace(ui, '        const Rml::String id = e->GetAttribute<Rml::String>("rowid", Rml::String());',
        '''        Rml::String id=e->GetAttribute<Rml::String>("rowkey", Rml::String());
        if (id.empty()) id=e->GetAttribute<Rml::String>("rowid", Rml::String());''')
    replace(ui, 'if (m_model.rows[i].id == id) return (int)i;',
        'if ((m_model.rows[i].selection_key.empty() ? m_model.rows[i].id : m_model.rows[i].selection_key) == id) return (int)i;')
    replace(ui, '        m_model.selected_id = row->id;',
        '        m_model.selected_id = row->selection_key.empty() ? row->id : row->selection_key;')
    replace(ui, 'if (id == r.id.c_str()) { hit = &r; break; }',
        'if (id == (r.selection_key.empty() ? r.id : r.selection_key).c_str()) { hit = &r; break; }')
    replace(host,'    long long resume_sec;   /* the service\'s resume point, 0 = from the start */',
        '    long long resume_sec;   /* the service\'s resume point, 0 = from the start */\n'+fields)
    replace(host,'    g_pending.resume_sec = sel.resume_sec;', '''    g_pending.resume_sec = sel.resume_sec;
    std::snprintf(g_pending.playback_playlist,sizeof g_pending.playback_playlist,"%s",sel.playback_playlist);
    g_pending.playback_index=sel.playback_index; g_pending.playback_scope=sel.playback_scope;''')
    replace(host,'            if (evo_provider_resolve_chain(g_pending.provider, g_pending.item_id,', '''            // Bind the selected occurrence immediately before the asynchronous request.
            if (!std::strcmp(g_pending.provider,"youtube"))
                tube_select_playlist_context(g_pending.item_id,g_pending.playback_playlist,
                    g_pending.playback_index,g_pending.playback_scope);
            if (evo_provider_resolve_chain(g_pending.provider, g_pending.item_id,''')
