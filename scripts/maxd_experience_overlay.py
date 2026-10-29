"""Next Max'd experience; apply to generated inputs after the existing overlays."""
import shutil


def apply(root, work, player):
    app = work / player
    for name in ("brand.h", "experience.h", "experience_store.h", "experience_provider.h", "session.h", "family.h", "family_catalog.h", "voice_commands.h", "voice_playback.hpp", "party_server.h", "party_page.h", "party_catalog.h", "phone_remote.h", "phone_remote_page.h", "phone_remote_catalog.h", "phone_remote_playback.hpp", "diagnostics.h"):
        shutil.copyfile(root / "native/maxd" / name, app / "addons/include/maxd" / name)
    shutil.copyfile(root / "native/maxd/party_server.c", app / "addons/src/maxd_party_server.c")
    for source in ("phone_remote", "diagnostics"):
        shutil.copyfile(root / "native/maxd" / (source + ".c"), app / ("addons/src/maxd_" + source + ".c"))
    def change(path, old, new):
        p = app / path
        text = p.read_text()
        assert text.count(old) == 1, (path, old[:100], text.count(old))
        p.write_text(text.replace(old, new))
    change("Makefile", "    addons/src/maxd_lightbar.c", '    addons/src/maxd_lightbar.c \\\n    addons/src/maxd_party_server.c \\\n    addons/src/maxd_phone_remote.c \\\n    addons/src/maxd_diagnostics.c')
    picker = "core/src/screens/AudioTrackPickerScreen.cpp"
    header = "core/include/evo/screens/AudioTrackPickerScreen.hpp"
    change(header, "KindBackgroundBrowse,", "KindBackgroundBrowse, KindMaxdPip, KindMaxdTools, KindMaxdHealth, KindMaxdHealthLower, KindMaxdHealthSafe, KindMaxdHealthConfirm, KindMaxdSearch, KindMaxdSave, KindMaxdCue, KindMaxdBookmark,")
    change(header, "    void refreshTracks();", """    void refreshTracks();
    void maxdTools();
    void maxdHealth();
    bool m_maxdRescueSafe = false;
    static void maxdText(const char*, void*);
    std::string m_maxdQuery;
    unsigned m_maxdOwner = 0;
    double m_maxdPosition = 0;
    bool m_maxdNaming = false;
    int m_maxdSeen = -2;
""")
    change(picker, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "maxd/experience.h"\n#include "maxd/smart_rules.h"\n#include "evo_keyboard.h"')
    change(picker, "    tube_player_menu_close();   /* the browser's own list state returns */", "    tube_player_menu_close();   /* the browser's own list state returns */\n    m_maxdOwner = 0; tube_maxd_transcript_clear();")
    change(picker, '    if (m_tubeExtraGroup >= 0) {', '    if (m_tubeExtraGroup == -6 || m_tubeExtraGroup == -7) { maxdHealth(); return; }\n    if (m_tubeExtraGroup == -5) { maxdTools(); return; }\n    if (m_tubeExtraGroup >= 0) {')
    change(picker, '    groupAfterglowTracks();', '''    {
        Entry e; e.kind=KindMaxdHealth; e.label="Playback health & rescue";
        e.detail="See recent playback health; review a lower-quality or AVC rescue"; m_tracks.push_back(e);
        e.kind=KindMaxdTools; e.label="Transcript & bookmarks";
        e.detail="Search captions, jump to a line, or save a named timestamp"; m_tracks.push_back(e);
        if (tube_afterglow_picker_active()) {
            e.kind=KindMaxdPip; e.label="Picture-in-picture";
            e.detail="Keep this video visible while browsing; tap L3 to return"; m_tracks.push_back(e);
        }
    }
    groupAfterglowTracks();''')
    change(picker, '    } else if (e.kind == KindBackgroundBrowse) {', '''    } else if (e.kind == KindMaxdHealth) {
        m_maxdOwner=tube_phone_selection(); m_tubeExtraGroup=-6;
        refreshTracks(); m_selectedIndex=m_scrollOffset=0;
    } else if (e.kind == KindMaxdHealthLower || e.kind == KindMaxdHealthSafe) {
        m_maxdRescueSafe=e.kind==KindMaxdHealthSafe; m_tubeExtraGroup=-7;
        refreshTracks(); m_selectedIndex=m_tracks.size()>1?1:0; m_scrollOffset=0;
    } else if (e.kind == KindMaxdHealthConfirm) {
        if(tube_maxd_health_repair(m_maxdOwner,m_maxdRescueSafe,playback->getPositionSeconds(),playback->isLiveSource(),prospero_subtitle_enabled)) {
            toast("PLAYBACK RESCUE","Reopening at this position with the selected format");
            playback->stopPlayback(); if(sm)sm->returnFromPlayback();return;
        }
        toast("PLAYBACK RESCUE","No suitable alternative is available for this video");
        m_tubeExtraGroup=-6;refreshTracks();m_selectedIndex=m_scrollOffset=0;
    } else if (e.kind == KindMaxdTools) {
        m_maxdQuery.clear(); m_maxdOwner=tube_caption_selection(); m_maxdSeen=-2;
        tube_maxd_transcript_begin(); m_tubeExtraGroup=-5;
        refreshTracks(); m_selectedIndex=m_scrollOffset=0;
    } else if (e.kind == KindMaxdSearch || e.kind == KindMaxdSave) {
        if (!playback->isLiveSource() || e.kind == KindMaxdSearch) {
            m_maxdOwner=tube_caption_selection(); m_maxdNaming=e.kind==KindMaxdSave;
            m_maxdPosition=playback->getPositionSeconds();
            evo_keyboard_open(m_maxdNaming?"Name this bookmark":"Find words in the transcript",
                m_maxdNaming?"":m_maxdQuery.c_str(),m_maxdNaming?120:128,maxdText,this);
        } else toast("BOOKMARKS","Timestamp bookmarks are available for videos on demand");
    } else if (e.kind == KindMaxdCue || e.kind == KindMaxdBookmark) {
        double position=e.kind==KindMaxdCue?tube_maxd_transcript_seconds(e.streamIndex):tube_maxd_bookmark_seconds(e.streamIndex);
        if (m_maxdOwner==tube_caption_selection() && position>=0 && !playback->isLiveSource()) {
            playback->seekTo(position); if(sm)sm->navigateTo(ScreenId::Player);
        }
    } else if (e.kind == KindBackgroundBrowse || e.kind == KindMaxdPip) {''')
    change(picker, '            tube_background_play_status(playback->getDisplayTitle().c_str(),playback->isPaused());', '            tube_background_play_picture(e.kind == KindMaxdPip);\n            tube_background_play_status(playback->getDisplayTitle().c_str(),playback->isPaused());')
    change(picker, '    if (pressed & PadButtons::Up)    { navigate(-1); return true; }', '''    if (m_tubeExtraGroup == -5 && (pressed & PadButtons::Square)) {
        if(m_selectedIndex>=0 && m_selectedIndex<static_cast<int>(m_tracks.size()) &&
           m_tracks[m_selectedIndex].kind==KindMaxdBookmark && m_maxdOwner==tube_caption_selection()) {
            toast("BOOKMARKS",tube_maxd_bookmark_remove(m_tracks[m_selectedIndex].streamIndex)?"Bookmark removed":"This bookmark is no longer available");refreshTracks();
            m_selectedIndex=std::max(0,std::min(m_selectedIndex,static_cast<int>(m_tracks.size())-1));
            m_scrollOffset=std::max(0,m_selectedIndex-EVO_RMLUI_LIST_ROWS+1);
        }
        return true;
    }
    if (pressed & PadButtons::Up)    { navigate(-1); return true; }''')
    change(picker, '        if (m_tubeExtraGroup != -1) {', '''        if (m_tubeExtraGroup == -7) {
            m_tubeExtraGroup=-6;refreshTracks();m_selectedIndex=std::max(0,std::min(m_maxdRescueSafe?2:1,static_cast<int>(m_tracks.size())-1));m_scrollOffset=0;return true;
        }
        if (m_tubeExtraGroup != -1) {''')
    change(picker, 'if (group == -2 ? m_tracks[i].kind == KindChapterList',
        'if (group == -5 ? m_tracks[i].kind == KindMaxdTools : group == -6 ? m_tracks[i].kind == KindMaxdHealth : group == -2 ? m_tracks[i].kind == KindChapterList')
    change(picker, 'void AudioTrackPickerScreen::update(double deltaMs) {', '''void AudioTrackPickerScreen::update(double deltaMs) {
    if(m_tubeExtraGroup==-5 && m_maxdSeen!=tube_maxd_transcript_state())refreshTracks();''')
    with (app / picker).open('a') as f:
        f.write('\nnamespace evo {\n' + (root / 'native/maxd/picker_tools.cpp.inc').read_text() + '\n}\n')
    appcpp = "core/src/Application.cpp"
    change(appcpp, "int uiActive = tube_voice_state()", "int uiActive = tube_background_play_picture_active() || tube_voice_state()")
    change(appcpp, '        // End YouTube Max’d video destination lease.', '''        if (tube_background_play_picture_active()) tube_video_destination_set(1392,730,480,270);
        // End YouTube Max’d video destination lease.''')
    change(appcpp, '        /*\n         * Draw the toast, over whatever the screen just drew.', '''        // PiP shares the existing decoder and the same source/fence lease.
        // Draw after the browser composite so menus cannot cover the video.
        if (!isPlayer && tube_background_play_picture_active() && tube_present_source.ready && uiActive) {
            auto &f=tube_present_source;
            evo_agc_motion_smoothing_set_phase(1.0f);
            const int direct=(evo_pb_active_backend()==EVO_VDEC_BACKEND_NATIVE && !f.held && f.uv && f.tube_lease)?1:0;
            evo_agc_blit_yuv(f.y,f.y_pitch,f.uv,f.uv_pitch,f.u,f.u_pitch,f.v,f.v_pitch,
                static_cast<int>(f.coded_w),static_cast<int>(f.coded_h),static_cast<int>(f.disp_w),static_cast<int>(f.disp_h),
                0,f.ten_bit,f.color_trc,direct,f.pts_us,f.dovi);
            swap=true;
        }
        /*
         * Draw the toast, over whatever the screen just drew.''')

    change(appcpp, '#include "tube_phone.h"', '#include "tube_phone.h"\n#include "maxd/experience.h"')
    change(appcpp, '        m_screenManager->update(frameDeltaMs);', """        if(tube_maxd_family_tick(static_cast<long long>(now_ms()),m_playbackController &&
            m_playbackController->isActive() && !m_playbackController->isPaused()) && m_playbackController && m_playbackController->isActive()) {
            m_playbackController->stopPlayback();tube_background_play_clear();
            toast("FAMILY LIMIT","This profile has reached its daily viewing limit");
            m_screenManager->returnFromPlayback();
        }
        m_screenManager->update(frameDeltaMs);""")

    host = "core/src/screens/ProviderHostScreen.cpp"
    change(host, '#include "tube_voice.h"', '#include "tube_voice.h"\n#include "maxd/experience.h"\n#include "maxd/voice_playback.hpp"')
    change(host, '                if (auto pb = Application::getInstance().getPlaybackController())\n                    if (pb->isActive()) pb->setPaused(true);', '                maxd_voice_capture();')
    change(host, 'm_voiceHeld = false; tube_voice_cancel();', 'm_voiceHeld = false; maxd_voice_restore(); tube_voice_cancel();')
    change(host, '    m_voiceHeld = false;\n    if (tube_afterglow_shown())', '    m_voiceHeld = false;\n    if(maxd_voice_apply(query))return true;\n    maxd_voice_restore();\n    if (tube_afterglow_shown())')
    change(appcpp, '#include "maxd/experience.h"', '#include "maxd/experience.h"\n#include "maxd/voice_playback.hpp"')
    change(appcpp, 'm_screenManager->getCurrentScreenId() == ScreenId::EmbyBrowse))', '(m_screenManager->getCurrentScreenId() == ScreenId::EmbyBrowse || m_screenManager->getCurrentScreenId() == ScreenId::Player)))')
    change(appcpp, '            toast("VOICE SEARCH", tube_voice_error());', '            maxd_voice_restore();\n            toast("VOICE SEARCH", tube_voice_error());')
    change(appcpp, '        } else if (tube_voice_state() == TUBE_VOICE_READY && m_screenManager &&', """        } else if (tube_voice_state()==TUBE_VOICE_READY && m_screenManager && m_screenManager->getCurrentScreenId()==ScreenId::Player) {
            if(!maxd_voice_apply(tube_voice_result())){maxd_voice_restore();toast("VOICE CONTROL","Try pause, resume, captions on, quality 1080p, forward 30 seconds, or add to queue");}
            tube_voice_cancel();
        } else if (tube_voice_state() == TUBE_VOICE_READY && m_screenManager &&""")
    playercpp="core/src/screens/PlayerScreen.cpp"
    playerh="core/include/evo/screens/PlayerScreen.hpp"
    change(playerh, '#include "tube_player_extra.h"', '#include "tube_player_extra.h"\n#include "voice_gesture.h"')
    change(playerh, 'private:', 'private:\n    MaxdVoiceGesture m_maxdVoiceGesture{};\n    bool m_maxdVoiceHeld=false;',)
    change(playercpp, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_voice.h"\n#include "maxd/experience.h"\n#include "maxd/voice_playback.hpp"\n#include "evo_keyboard.h"')
    change(playercpp, 'bool PlayerScreen::handleInput(uint32_t pressed, uint32_t held, uint32_t released) {', """bool PlayerScreen::handleInput(uint32_t pressed, uint32_t held, uint32_t released) {
        if(tube_voice_state()==TUBE_VOICE_RECORDING || tube_voice_state()==TUBE_VOICE_TRANSCRIBING) {
            if(pressed & PadButtons::Circle){maxd_voice_restore();tube_voice_cancel();m_maxdVoiceHeld=false;return true;}
            if(m_maxdVoiceHeld && (released & PadButtons::L3)){tube_voice_finish();m_maxdVoiceHeld=false;}
            return true;
        }
        if(!tube_phone_owns_media() && !evo_keyboard_is_open()) {
            const auto time=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            int gesture=maxd_voice_gesture(&m_maxdVoiceGesture,!!(pressed&PadButtons::L3),!!(held&PadButtons::L3),!!(released&PadButtons::L3),!!(pressed&PadButtons::Circle),time);
            if(gesture==MAXD_VOICE_START){if(tube_voice_begin(0,time)){maxd_voice_capture();m_maxdVoiceHeld=true;toast("VOICE CONTROL","Listening: hold L3, speak, release. Circle cancels.");}else toast("VOICE CONTROL",tube_voice_error());return true;}
            if(gesture==MAXD_VOICE_WAIT || gesture==MAXD_VOICE_CANCEL)return true;
        }
""")

    change(picker, 'params.title = m_tubeExtraGroup == -2 ?', 'params.title = m_tubeExtraGroup == -6 || m_tubeExtraGroup == -7 ? "PLAYBACK HEALTH & RESCUE" : m_tubeExtraGroup == -5 ? "TRANSCRIPT & BOOKMARKS" : m_tubeExtraGroup == -2 ?')

    change("core/include/evo/screens/ProviderHostScreen.hpp", "    bool submitVoiceQuery(const char* query);", "    bool submitVoiceQuery(const char* query);\n    bool submitPhoneSearch(const char* query);")
    change(host, "void ProviderHostScreen::openSearch()", """bool ProviderHostScreen::submitPhoneSearch(const char* query)
{
    // Phone text is always a query; never interpret it as a voice command.
    if (m_providerId != "youtube" || !m_opened || m_picking || m_web || m_discovering ||
        m_resolving || m_streamPick || !query || !*query || evo_keyboard_is_open() ||
        evo_rmlui_provider_panel_open()) return false;
    if(tube_afterglow_shown())return tube_afterglow_submit_voice_query(query)!=0;
    OnSearchSubmitted(query,this);return true;
}

void ProviderHostScreen::openSearch()""")
    change(appcpp, 'long long now_ms(void);\n}', 'long long now_ms(void);\n}\n#include "maxd/phone_remote_playback.hpp"')
    change(appcpp, '        if (!m_softClose) tube_poll();', '        if (!m_softClose) tube_poll();\n        if(!m_softClose)maxd_phone_frame(*this,static_cast<int64_t>(now_ms()));\n        else maxd_remote_stop();')
    change(appcpp, '    evo_boot_log("shutdown: begin");', '    maxd_remote_stop();\n    evo_boot_log("shutdown: begin");')
