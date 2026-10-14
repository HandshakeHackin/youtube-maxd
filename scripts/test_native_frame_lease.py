"""Compile actual generated native claim/idle and pp handoff with real lease metadata."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
work=Path(os.environ['PROSPEROTUBE_NATIVE_WORK'])/'projects/evoplayer'
source=(work/'media/src/evo_vdec_native.c').read_text()
def function(source,signature):
    a=source.index(signature);b=source.index('{',a)+1;depth=1;i=b
    while depth:
        depth+=(source[i]=='{')-(source[i]=='}');i+=1
    return source[a:i]
# The original cyclic selection fails the retained baseline pool-wrap reproduction.
assert 'probe_slot(NAT_VP9, EVO_VDEC_NATIVE_VP9_MAX_W, EVO_VDEC_NATIVE_VP9_MAX_H, 0, sm);' in source
assert 'if (rc != 0 && d->idx != NAT_VP9 && (w > 1920 || h > 1088))' in source
assert 'int fslot = tube_native_claim_frame(' in source
with tempfile.TemporaryDirectory(prefix='tube-native-lease-') as temp:
    temp=Path(temp)
    (temp/'native_claim.inc').write_text(function(source,'static int tube_native_claim_frame('))
    (temp/'native_close_drain.inc').write_text(function(source,'static int tube_native_drain_for_close('))
    (temp/'native_idle.inc').write_text(function(source,'static int tube_native_pool_idle('))
    for name,signature in [('surface','static int tube_native_surface_valid('),('harvest','static void ro_harvest('),('receive','int evo_vdec_native_receive('),('reset','static void ro_reset(')]:
        (temp/f'native_{name}.inc').write_text(function(source,signature))
    import re
    (temp/'native_constants.inc').write_text('\n'.join(re.findall(r'^#define (?:RO_SLOTS|EVO_VDEC_REORDER_DEPTH)\s+[^\n]+',source,re.M))+'\n')

    flags=['-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    includes=['-I'+str(path) for path in (temp,root/'native',work/'pp/include',work/'addons/include',work/'media/include',work/'include')]
    subprocess.run(['cc',*flags,*includes,str(root/'tests/test_native_frame_lease.c'),str(root/'native/tube_frame_lease.c'),str(work/'pp/src/pp_playback.c'),str(work/'pp/src/pp_clock.c'),'-lm','-pthread','-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
    subprocess.run(['cc',*flags,'-I'+str(temp),str(root/'tests/test_native_close_drain.c'),'-o',str(temp/'test_close')],check=True)
    subprocess.run([str(temp/'test_close')],check=True)

app=(work/'core/src/Application.cpp').read_text()
retirement=function(app,'            if (tube_present_source.tube_lease)')
assert app.index(retirement)>app.index('            evo_agc_runtime_present();')
assert 'if (!(swap && uiActive)) pp_playback_release_video_frame(&tube_present_source);' in app
assert 'tube_present_source=f;f.tube_lease=nullptr;' in app
assert 'tube_present_source=hiddenFrame;hiddenFrame.tube_lease=nullptr;' in app
teardown=function(source,'static int slot_teardown(')
assert teardown.index('!tube_frame_pool_unregister(s->frame_mem)')<teardown.index('sceVideodec2DeleteDecoder(s->decoder)')
assert teardown.index('sceVideodec2DeleteDecoder(s->decoder)')<teardown.index('sceVideodec2ReleaseComputeQueue(s->compute_queue)')
assert teardown.index('sceVideodec2ReleaseComputeQueue(s->compute_queue)')<teardown.index('free_direct(s->frame_mem')
assert teardown.index('free_direct(s->compute_mem')>teardown.index('sceVideodec2ReleaseComputeQueue(s->compute_queue)')
assert teardown.count('retaining mapped pools')==2 and teardown.count('s->owned=1;s->ready=0;return -1;')==3
assert 'if(slot_teardown(s)!=0)return;\n        rc = slot_bringup(s, d, 1920, 1088' in source
assert 'if(slot_teardown(slot)!=0){s_depth_override=0;return NULL;}' in source
assert 'if(slot_teardown(slot)!=0)return NULL;\n            stage = "?"' in source


close=function(source,'void evo_vdec_native_close(evo_vdec_native *v)\n{')
assert close.index('ro_reset(v);')<close.index('tube_native_pool_idle(v->slot)')<close.index('tube_native_drain_for_close(v)')<close.index('sceVideodec2Reset(v->dec)')
assert 'if(rc!=0 || nwd_hung)' in close and 'slot->owned=1;slot->ready=0;return;' in close
controller=(work/'core/src/services/PlaybackController.cpp').read_text()
stop=function(controller,'void PlaybackController::stopPlayback()')
assert stop.index('pthread_join(video_thread')<stop.index('pp_playback_on_file_close(')<stop.index('evo_vdec_close(')
scaffold=r"""
#include "tube_frame_lease.h"
#include <cassert>
struct{const void*tube_lease=nullptr;}tube_present_source;
static bool completed=false;
static int evo_agc_runtime_last_present_retired(){return completed;}
static void evo_boot_log(const char*){}
static void pp_playback_release_video_frame(decltype(tube_present_source)*frame){tube_frame_lease_release(frame->tube_lease);frame->tube_lease=nullptr;}
static void retire(){
"""+retirement+r"""
}
int main(){void*base=(void*)0x10000000;assert(tube_frame_pool_register(base,16384,8));assert(tube_frame_pool_claim(base,0)==0);tube_present_source.tube_lease=base;
 retire();assert(tube_frame_pool_busy(base)==1&&!tube_frame_pool_unregister(base)&&tube_present_source.tube_lease==base);
 completed=true;retire();assert(!tube_frame_pool_busy(base)&&!tube_present_source.tube_lease);assert(tube_frame_pool_unregister(base));}
"""
with tempfile.TemporaryDirectory(prefix='tube-native-present-') as temp:
    temp=Path(temp);(temp/'test.cpp').write_text(scaffold)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'native'),'-c',str(root/'native/tube_frame_lease.c'),'-o',str(temp/'lease.o')],check=True)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'native'),str(temp/'test.cpp'),str(temp/'lease.o'),'-pthread','-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
print('PASS actual Application failed-present retention, successful retirement, snapshot transfer and stop-order checks')
