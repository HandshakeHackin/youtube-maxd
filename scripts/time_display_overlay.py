"""SmartTube time-display preferences on both shipping HUDs and player options."""


def apply_time_display_overlay(replace, player):
    for path, anchor in [('ui_rml/src/evo_rmlui_app.cpp', '#include "evo_rmlui_app.h"'),
                         ('ui_kit/src/evo_hui_osd.cpp', '#include "evo_hui_osd.hpp"')]:
        replace(player + path, anchor, anchor + '\n#include "tube_time_display.h"\n#include "tube_provider.h"\n#include "evo_audio_out.h"')
    rml=player+'ui_rml/src/evo_rmlui_app.cpp'
    replace(rml, '    if (state == m_last_state && m_theme_generation == m_theme_gen_playback) return;', '''    // ProsperoTube time display refresh.
    char tube_time_label[96];
    tube_time_display(tube_time_label,sizeof tube_time_label,tube_player_time_mode(),
        state.scrub_active ? state.scrub_target : state.position_sec,state.duration_sec,
        evo_audio_rate(),state.is_live,time(NULL));
    if(auto* time_element=m_playback_doc->GetElementById("time-duration")) {
        if(time_element->GetInnerRML()!=tube_time_label) {
            time_element->SetInnerRML(tube_time_label);m_frame_dirty=true;
        }
    }
    // End ProsperoTube time display refresh.
    if (state == m_last_state && m_theme_generation == m_theme_gen_playback) return;''')
    replace(rml, '''    std::string dur_str = format_time(state.duration_sec);
    double remaining = state.duration_sec - cur_pos;
    if (remaining < 0) remaining = 0;
    std::string rem_str = "-" + format_time(remaining);
    std::string time_display = rem_str + " / " + dur_str;''', '    std::string time_display = tube_time_label;')
    replace(player+'ui_kit/src/evo_hui_osd.cpp', '''        ui::text(list, fonts.mono, clock_text(raw_.duration_sec), track.x + track.w, 970, 24,
                 kWhite.with_alpha(0.7f), Align::right);''', '''        // ProsperoTube time display HUI.
        char tube_time_label[96];
        tube_time_display(tube_time_label,sizeof tube_time_label,tube_player_time_mode(),
            raw_.scrub_active ? raw_.scrub_target : raw_.position_sec,raw_.duration_sec,
            evo_audio_rate(),raw_.is_live,time(NULL));
        ui::text(list, fonts.mono, tube_time_label, track.x + track.w, 970, 24,
                 kWhite.with_alpha(0.7f), Align::right);
        // End ProsperoTube time display HUI.''')
    replace(player+'ui_rml/src/evo_rmlui_provider.cpp','id != "settings:seek-interval"',
            'id != "settings:seek-interval" && id != "settings:time-display"')
    replace(player+'core/include/evo/screens/AudioTrackPickerScreen.hpp',
            'KindSeek, KindSeekInterval };','KindSeek, KindSeekInterval, KindTime };')
    picker=player+'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(picker,'    // End ProsperoTube chat option rows.','''    // End ProsperoTube chat option rows.
    // ProsperoTube time display rows.
    const char *time_labels[]={"Time display: total duration","Time display: remaining","Time display: estimated finish"};
    for(int mode=0;mode<3;++mode) {
        Entry e;e.kind=KindTime;e.streamIndex=mode;e.label=time_labels[mode];
        e.detail="Remaining and finish times account for playback speed";
        e.badge=tube_player_time_mode()==mode ? "SELECTED" : "";
        m_tracks.push_back(std::move(e));
    }
    // End ProsperoTube time display rows.''')
    replace(picker,'    if (e.kind == KindSeekInterval) {','''    // ProsperoTube time display selection.
    if(e.kind==KindTime) {
        toast("TIME DISPLAY",tube_player_set_time_mode(e.streamIndex) ? e.label.c_str() : "Unable to save time display");
    }
    // End ProsperoTube time display selection.
    else if (e.kind == KindSeekInterval) {''')
