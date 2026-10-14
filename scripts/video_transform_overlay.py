"""PS5 display-transform wiring; activate only with verified transform shaders."""
def apply_video_transform_overlay(replace, player, destination=False):
    path=player+'media/src/evo_agc_runtime.c'
    width = 'g_tube_frame_destination.w' if destination else 'g_agc_dev.width'
    height = 'g_tube_frame_destination.h' if destination else 'g_agc_dev.height'
    replace(path,'#include "tube_video_geometry.h"','''#include "tube_video_geometry.h"
#include "tube_video_transform.h"
/* One geometry snapshot per presented frame; scratch passes remain unrotated. */
static unsigned g_tube_frame_geometry, g_tube_frame_transform;
static int g_tube_canvas_w, g_tube_canvas_h;''')
    replace(path,'''    float sx, sy;
    agc_video_scale(disp_w, disp_h, view_mode, &sx, &sy);''',f'''    g_tube_frame_geometry=tube_player_video_geometry();
    g_tube_frame_transform=tube_video_transform_code(g_tube_frame_geometry);
    tube_video_transform_canvas(g_tube_frame_transform,{width},{height},
                                &g_tube_canvas_w,&g_tube_canvas_h);
    float sx, sy;
    agc_video_scale(disp_w, disp_h, view_mode, &sx, &sy);''')
    replace(path,'''tube_video_geometry_scale(tube_player_video_geometry(),disp_w,disp_h,
        g_agc_dev.width,g_agc_dev.height,sx,sy);''','''tube_video_geometry_scale(g_tube_frame_geometry,disp_w,disp_h,
        g_tube_canvas_w,g_tube_canvas_h,sx,sy);''')
    if destination:
        replace(path,'''    tube_video_destination destination;
    tube_video_destination_get(g_agc_dev.width,g_agc_dev.height,&destination);
    int logical_w,logical_h;
    tube_video_transform_canvas(tube_video_transform_code(tube_player_video_geometry()),destination.w,destination.h,&logical_w,&logical_h);
    tube_video_geometry_scale(tube_player_video_geometry(),disp_w,disp_h,logical_w,logical_h,sx,sy);''', '')
    # Both postprocessing planners clip against the unrotated logical canvas.
    replace(path,f'const float W = (float){width}, H = (float){height};',
            'const float W = (float)g_tube_canvas_w, H = (float)g_tube_canvas_h;',count=2)
    replace(path,f'if (x1 > {width}) x1 = {width};',
            'if (x1 > g_tube_canvas_w) x1 = g_tube_canvas_w;',count=2)
    replace(path,f'if (y1 > {height}) y1 = {height};',
            'if (y1 > g_tube_canvas_h) y1 = g_tube_canvas_h;',count=2)
    replace(path,'const size_t cons_bytes = params ? 32u : 16u;','const size_t cons_bytes = 48u;')
    replace(path,'''    float cbuf[8] = {0};
    memcpy(cbuf, uv, 16);
    if (params)
        memcpy(cbuf + 4, params, 16);''','''    float cbuf[12];
    tube_video_transform_uniforms(g_tube_frame_transform,dst->scanout,uv,params,cbuf);''')
    anchor = '    // ProsperoTube final-pass destination; scratch targets never inherit its offset.' if destination else '    if (agc_up_bind_target(dst, x, y, w, h) != 0)'
    replace(path,anchor,'''    if(dst->scanout)
        tube_video_transform_rect(g_tube_frame_transform,g_agc_dev.width,g_agc_dev.height,
                                  &x,&y,&w,&h);
    if (agc_up_bind_target(dst, x, y, w, h) != 0)''' if not destination else f'''    if(dst->scanout)
        tube_video_transform_rect(g_tube_frame_transform,{width},{height},
                                  &x,&y,&w,&h);
''' + anchor)
    replace(path,'    constants.pad   = 0.0f;',
            '    constants.pad   = target_slot >= 0 ? 0.0f : (float)g_tube_frame_transform;')
