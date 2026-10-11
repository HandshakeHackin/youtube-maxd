"""Extract the actual PS5 AGC sizing function and HUD/picker integration."""
from pathlib import Path
import sys
root,output=map(Path,sys.argv[1:3]);output.mkdir(parents=True,exist_ok=True)
source=(root/'media/src/evo_agc_runtime.c').read_text()
if len(sys.argv)>3 and sys.argv[3]=="staged" and "g_tube_frame_geometry" not in source:
    from video_transform_overlay import apply_video_transform_overlay
    def replace(path,old,new,count=1):
        global source
        assert path=="media/src/evo_agc_runtime.c"
        assert source.count(old)==count,old
        source=source.replace(old,new)
    apply_video_transform_overlay(replace,"")
active="g_tube_frame_geometry" in source
state="#define TUBE_TEST_TRANSFORM_ACTIVE "+str(int(active))+"\n"
snapshot=""
if active:
    begin=source.index("static unsigned g_tube_frame_geometry")
    end=source.index(";",source.index("static int g_tube_canvas_w",begin))+1
    state+=source[begin:end]+"\n"
    begin=source.index("    g_tube_frame_geometry=tube_player_video_geometry();")
    end=source.index("    float sx, sy;",begin)
    snapshot=source[begin:end]
output.joinpath("snapshot-state.inc").write_text(state)
output.joinpath("snapshot.inc").write_text(snapshot)
start=source.index('static void agc_video_scale(')
end=source.index('\nint evo_agc_blit_yuv(',start)
output.joinpath('scale.inc').write_text(source[start:end])
for relative,tag,name in [('ui_rml/src/evo_rmlui_app.cpp','RML','rml.inc'),
 ('core/src/screens/AudioTrackPickerScreen.cpp','rows','rows.inc'),
 ('core/src/screens/AudioTrackPickerScreen.cpp','selection','selection.inc')]:
    source=(root/relative).read_text();start=source.index('// ProsperoTube video geometry '+tag)
    end=source.index('// End ProsperoTube video geometry '+tag,start)
    if tag=='RML':assert end<source.index('if (state == m_last_state &&')
    output.joinpath(name).write_text(source[start:end])
controller=(root/'core/src/services/PlaybackController.cpp').read_text()
assert 'm_source = PlaybackSource{};\n    tube_video_geometry_enable(0);' in controller
assert 'tube_video_geometry_enable(m_source.provider == "youtube");\n    applyViewMode();' in controller
assert 'tube_video_geometry_enable(tube_player_set_resize_mode(static_cast<int>(mode)))' in controller

functions=[]
for signature in ['void PlaybackController::cycleViewMode()', 'void PlaybackController::setViewMode(ViewMode mode)']:
    start=controller.index(signature);end=controller.index('{',start)+1;depth=1
    while depth:
        if controller[end]=='{':depth+=1
        elif controller[end]=='}':depth-=1
        end+=1
    functions.append(controller[start:end])
output.joinpath('controller.inc').write_text('\n'.join(functions))
