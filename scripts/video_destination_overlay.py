"""Map only final video passes into Afterglow's destination; scratch surfaces stay local."""
import shutil

def apply(root,work,replace,player):
    for name in ("tube_video_destination.c","tube_video_destination.h"):
        shutil.copyfile(root/"native"/name,work/player/("addons/src" if name.endswith(".c") else "addons/include")/name)
    replace(player+"Makefile","    addons/src/tube_backup.c","    addons/src/tube_backup.c \\\n    addons/src/tube_video_destination.c")
    app=player+"core/src/Application.cpp"
    replace(app,'#include "tube_provider.h"','#include "tube_provider.h"\n#include "tube_video_destination.h"')
    replace(app,'        bool isSurround = (m_screenManager->getCurrentScreenId() == ScreenId::SurroundTest);',r"""        bool isSurround = (m_screenManager->getCurrentScreenId() == ScreenId::SurroundTest);
        // ProsperoTube video destination lease: decide before any quad draw, including ownership changes.
        auto layoutPlayer = isPlayer ? static_cast<PlayerScreen*>(m_screenManager->getCurrentScreen()) : nullptr;
        tube_afterglow_video_prepare(!m_softClose && layoutPlayer && m_playbackController &&
            m_playbackController->isActive() && !m_playbackController->isMusicMode() &&
            !m_playbackController->isScrubbing() && !layoutPlayer->isStatsForNerdsVisible() &&
            !tube_phone_owns_media() && s_upcmp.state==0,static_cast<float>(frameDeltaMs/1000.0));
        // End ProsperoTube video destination lease.""")
    path=player+"media/src/evo_agc_runtime.c"
    transformed="g_tube_frame_geometry" in (work/path).read_text()
    replace(path,'#include "tube_video_geometry.h"','#include "tube_video_geometry.h"\n#include "tube_video_destination.h"\n#include "tube_video_transform.h"\nstatic tube_video_destination g_tube_frame_destination;')
    anchor='    g_tube_frame_geometry=tube_player_video_geometry();' if transformed else '    float sx, sy;\n    agc_video_scale(disp_w, disp_h, view_mode, &sx, &sy);'
    replace(path,anchor,'    tube_video_destination_get(g_agc_dev.width,g_agc_dev.height,&g_tube_frame_destination);\n'+anchor)
    if transformed:
        replace(path,'tube_video_transform_canvas(g_tube_frame_transform,g_agc_dev.width,g_agc_dev.height,',
                'tube_video_transform_canvas(g_tube_frame_transform,g_tube_frame_destination.w,g_tube_frame_destination.h,')
        replace(path,'tube_video_transform_rect(g_tube_frame_transform,g_agc_dev.width,g_agc_dev.height,',
                'tube_video_transform_rect(g_tube_frame_transform,g_tube_frame_destination.w,g_tube_frame_destination.h,')
    else:
        # Keep the transform overlay's original anchors intact for its staged host regression.
        replace(path,'    // End ProsperoTube video geometry apply.',r"""    // End ProsperoTube video geometry apply.
    tube_video_destination destination;
    tube_video_destination_get(g_agc_dev.width,g_agc_dev.height,&destination);
    int logical_w,logical_h;
    tube_video_transform_canvas(tube_video_transform_code(tube_player_video_geometry()),destination.w,destination.h,&logical_w,&logical_h);
    tube_video_geometry_scale(tube_player_video_geometry(),disp_w,disp_h,logical_w,logical_h,sx,sy);""")
        replace(path,'const float W = (float)g_agc_dev.width, H = (float)g_agc_dev.height;',
                'const float W = (float)g_tube_frame_destination.w, H = (float)g_tube_frame_destination.h;',count=2)
        replace(path,'if (x1 > g_agc_dev.width) x1 = g_agc_dev.width;',
                'if (x1 > g_tube_frame_destination.w) x1 = g_tube_frame_destination.w;',count=2)
        replace(path,'if (y1 > g_agc_dev.height) y1 = g_agc_dev.height;',
                'if (y1 > g_tube_frame_destination.h) y1 = g_tube_frame_destination.h;',count=2)
    replace(path,'    if (agc_up_bind_target(dst, x, y, w, h) != 0)',r"""    // ProsperoTube final-pass destination; scratch targets never inherit its offset.
    if(dst->scanout) { x+=g_tube_frame_destination.x;y+=g_tube_frame_destination.y; }
    // End ProsperoTube final-pass destination.
    if (agc_up_bind_target(dst, x, y, w, h) != 0)""")
    source=(work/path).read_text();a=source.index('    /* 2. Fullscreen viewport and scissor */');b=source.index('\n    /* 3.',a)
    replace(path,source[a:b],r"""    /* 2. Destination viewport and scissor; Fit/Fill/zoom still use the same quad. */
    if (target_slot < 0)
        up_viewport_scissor(g_tube_frame_destination.x,g_tube_frame_destination.y,
            g_tube_frame_destination.w,g_tube_frame_destination.h,g_agc_dev.width,g_agc_dev.height);
""")
