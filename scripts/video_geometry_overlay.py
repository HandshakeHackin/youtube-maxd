"""Profile sizing on the shared PS5 AGC quad, independent of decoder choice."""


def apply_video_geometry_overlay(replace,player):
    agc=player+'media/src/evo_agc_runtime.c'
    replace(agc,'#include "evo_agc_runtime.h"','#include "evo_agc_runtime.h"\n#include "tube_video_geometry.h"\n#include "tube_provider.h"')
    replace(agc,'''    }
}

int evo_agc_blit_yuv(''','''    }
    // ProsperoTube video geometry apply.
    tube_video_geometry_scale(tube_player_video_geometry(),disp_w,disp_h,
        g_agc_dev.width,g_agc_dev.height,sx,sy);
    // End ProsperoTube video geometry apply.
}

int evo_agc_blit_yuv(''')
    controller=player+'core/src/services/PlaybackController.cpp'
    replace(controller,'    int nextMode = (static_cast<int>(m_viewMode) + 1) % 3;',
            '''    // Percentage zoom and fixed-axis sizing return to Fit on Triangle.
    const int current=m_source.provider=="youtube" ? tube_player_resize_mode() : static_cast<int>(m_viewMode);
    const bool custom=m_source.provider=="youtube" && (tube_player_zoom_percent() || current>2);
    int nextMode=custom ? 0 : (current+1)%3;''')
    replace(controller,'    m_source = PlaybackSource{};','    m_source = PlaybackSource{};\n    tube_video_geometry_enable(0);')
    replace(controller,'    applyViewMode();\n\n    /*','    tube_video_geometry_enable(m_source.provider == "youtube");\n    applyViewMode();\n\n    /*')
    replace(controller,'''    m_viewMode = mode;
    applyViewMode();''','''    m_viewMode = mode;
    if(m_source.provider=="youtube")
        tube_video_geometry_enable(tube_player_set_resize_mode(static_cast<int>(mode)));
    applyViewMode();''')
    for path,anchor in [('ui_rml/src/evo_rmlui_app.cpp','#include "tube_time_display.h"'),
                        ('ui_kit/src/evo_hui_osd.cpp','#include "tube_time_display.h"')]:
        replace(player+path,anchor,anchor+'\n#include "tube_video_geometry.h"')
    rml=player+'ui_rml/src/evo_rmlui_app.cpp'
    replace(rml,'    // ProsperoTube time display refresh.','''    // ProsperoTube video geometry RML.
    char geometry_name[64];
    tube_video_geometry_label(tube_player_video_geometry(),state.view_mode,geometry_name,sizeof geometry_name);
    if(auto* aspect=m_playback_doc->GetElementById("label-aspect")) {
        const std::string label=std::string("ASPECT: ")+geometry_name;
        if(aspect->GetInnerRML()!=label){aspect->SetInnerRML(label);m_frame_dirty=true;}
    }
    // End ProsperoTube video geometry RML.
    // ProsperoTube time display refresh.''')
    replace(rml,'        const char* vm = (state.view_mode == 0) ? "FIT" : ((state.view_mode == 1) ? "FILL" : "STRETCH");',
            '        char vm[64];tube_video_geometry_label(tube_player_video_geometry(),state.view_mode,vm,sizeof vm);')
    replace(player+'ui_kit/src/evo_hui_osd.cpp','char aspect[32]','char aspect[80]')
    replace(player+'ui_kit/src/evo_hui_osd.cpp','''    std::snprintf(aspect, sizeof aspect, "Aspect: %s",
                  raw_.view_mode == 0 ? "Fit" : (raw_.view_mode == 1 ? "Fill" : "Stretch"));''','''    char geometry_name[64];
    tube_video_geometry_label(tube_player_video_geometry(),raw_.view_mode,geometry_name,sizeof geometry_name);
    std::snprintf(aspect,sizeof aspect,"Aspect: %.63s",geometry_name);''')
    replace(player+'ui_rml/src/evo_rmlui_provider.cpp','id != "settings:time-display"',
            'id != "settings:time-display" && id != "settings:video-zoom" && id != "settings:video-aspect" && id != "settings:pixel-ratio" && id != "settings:video-rotate"')
    replace(player+'core/include/evo/screens/AudioTrackPickerScreen.hpp',
            'KindSeekInterval, KindTime };','KindSeekInterval, KindTime, KindResize, KindZoom, KindAspect, KindPixel, KindRotate, KindMirror };')
    picker=player+'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(picker,'    // End ProsperoTube chat option rows.','''    // End ProsperoTube chat option rows.
    // ProsperoTube video geometry rows.
    const int resize_modes[]={0,3,4,1,2};const char* resize_labels[]={"Fit","Fit width","Fit height","Fill","Stretch"};
    for(int i=0;i<5;++i) {
        Entry e;e.kind=KindResize;e.streamIndex=resize_modes[i];e.label=resize_labels[i];e.detail="Video sizing; saved for this profile";
        e.badge=!tube_player_zoom_percent() && tube_player_resize_mode()==e.streamIndex?"SELECTED":"";m_tracks.push_back(std::move(e));
    }
    for(int percent=50;percent<=300;++percent)if(tube_zoom_valid(percent)) {
        Entry e;e.kind=KindZoom;e.streamIndex=percent;e.label="Zoom "+std::to_string(percent)+"%";
        e.detail="Centered zoom relative to Fit; saved for this profile";e.badge=tube_player_zoom_percent()==percent?"SELECTED":"";
        m_tracks.push_back(std::move(e));
    }
    for(int i=0;i<11;++i) {
        Entry e;e.kind=KindAspect;e.streamIndex=i;e.label=tube_aspect_labels[i];e.detail="Video aspect ratio; saved for this profile";
        e.badge=tube_player_aspect_mode()==i?"SELECTED":"";m_tracks.push_back(std::move(e));
    }
    for(int i=0;i<6;++i) {
        Entry e;e.kind=KindPixel;e.streamIndex=i;e.label=tube_pixel_labels[i];e.detail="Display correction; applies to all profiles";
        e.badge=tube_player_pixel_mode()==i?"SELECTED":"";m_tracks.push_back(std::move(e));
    }
    for(int degrees=0;degrees<=270;degrees+=90) {
        Entry e;e.kind=KindRotate;e.streamIndex=degrees;e.label="Rotate "+std::to_string(degrees)+" degrees";
        e.detail="Clockwise rotation; saved for this profile";e.badge=tube_player_rotation_degrees()==degrees?"SELECTED":"";
        m_tracks.push_back(std::move(e));
    }
    {
        Entry e;e.kind=KindMirror;e.label=tube_player_mirror_enabled()?"Mirror video: on":"Mirror video: off";
        e.detail="Toggle horizontal mirror before rotation; saved for this profile";
        m_tracks.push_back(std::move(e));
    }
    // End ProsperoTube video geometry rows.''')
    replace(picker,'#include "tube_provider.h"','#include "tube_provider.h"\n#include "tube_video_geometry.h"')
    replace(picker,'    // ProsperoTube time display selection.','''    // ProsperoTube video geometry selection.
    if(e.kind==KindResize || e.kind==KindZoom || e.kind==KindAspect || e.kind==KindPixel || e.kind==KindRotate || e.kind==KindMirror) {
        bool saved=e.kind==KindResize?tube_player_set_resize_mode(e.streamIndex):
            e.kind==KindZoom?tube_player_set_zoom_percent(e.streamIndex):
            e.kind==KindAspect?tube_player_set_aspect_mode(e.streamIndex):
            e.kind==KindPixel?tube_player_set_pixel_mode(e.streamIndex):
            e.kind==KindRotate?tube_player_set_rotation_degrees(e.streamIndex):
            tube_player_set_mirror_enabled(!tube_player_mirror_enabled());
        toast("VIDEO SIZING",saved?(e.kind==KindMirror?(tube_player_mirror_enabled()?"Mirror video: on":"Mirror video: off"):e.label.c_str()):"Unable to save video sizing");
    }
    // End ProsperoTube video geometry selection.
    else
    // ProsperoTube time display selection.''')
