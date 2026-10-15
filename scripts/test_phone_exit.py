"""Compile actual GPU drain and parked self-exit dispatch with hostile fences."""
from pathlib import Path
import os
import subprocess
import tempfile

work=Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK',Path.home()/'.cache/prosperotube/native'))
player=work/'projects/evoplayer'
gpu=(player/'media/src/evo_agc_runtime.c').read_text()
start=gpu.index('static int agc_wait_slot_fence(')
end=gpu.index('static int agc_emit_frame_fence(',start)
wait_slot=gpu[start:end]
start=gpu.index('static int agc_wait_gpu_idle(unsigned timeout_ms)')
end=gpu.index('void evo_agc_runtime_wait_idle(unsigned timeout_ms)',start)
drain=gpu[start:end]
app=(player/'core/src/Application.cpp').read_text()
start=app.index('        if (m_softClosed) {')
end=app.index('            usleep(200000);',start)
close=app[start:end]+'        }\n'
start=app.index('        if (m_softClose) {')
park=app.index('                m_softClosed = true;',start)
mark_start=app.index('            if (++m_softCloseFrames > 150)',start)
mark_end=app.index('\n            }\n        }',mark_start)+len('\n            }')
mark=app[mark_start:mark_end]
for operation in ('stopPlayback();','tube_voice_shutdown();','tube_caption_switch_cancel();','tube_cancel();','tube_remote_shutdown();'):
    assert start<app.index(operation,start)<park,operation

source=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
constexpr int EVO_AGC_FRAME_SLOTS=3;
static struct { int initialized=1; uint32_t* fences[3]; uint32_t fence_expect[3]; } g_agc_dev;
static uint32_t values[3];
static unsigned flushes, sleeps, exits;
static unsigned markers;
static int m_softCloseFrames=150;
static bool complete_on_sleep, m_softClosed=true, m_exitAfterPark;
static void evo_boot_log(const char*,...) {}
static void evo_boot_log_flush() {}
static void evo_usb_remote_mark_parked() { ++markers; }
static void evo_agc_runtime_cache_flush(const void* address,int size) {
    assert(size==4);
    assert(address==&values[0] || address==&values[1] || address==&values[2]);
    ++flushes;
}
static void sceKernelUsleep(unsigned us) {
    assert(us==1000);++sleeps;
    if(complete_on_sleep)for(int i=0;i<3;i++)values[i]=g_agc_dev.fence_expect[i];
}
static int sceSystemServiceLoadExec(const char* path,const char** args) {
    assert(!strcmp(path,"exit") && !args);
    for(int i=0;i<3;i++)assert(!g_agc_dev.fence_expect[i]);
    ++exits;return -7;
}
'''+wait_slot+drain+'\nstatic void markParked() { for (int i=0;i<1;i++) {\n'+mark+'\n} }\nstatic void parked() {\n'+close+r'''
}
static void reset(bool completed) {
    flushes=sleeps=exits=markers=0;complete_on_sleep=false;m_exitAfterPark=true;m_softCloseFrames=150;
    g_agc_dev.initialized=1;m_softClosed=true;
    for(int i=0;i<3;i++){values[i]=completed?i+1:0;g_agc_dev.fences[i]=&values[i];g_agc_dev.fence_expect[i]=i+1;}
}
static void test_slot_wait() {
    unsigned waited=99;
    reset(false);
    // Zero memory cannot prove retirement of a zero marker.
    assert(!agc_wait_slot_fence(0,0,3,1000,&waited));
    assert(waited==0 && !flushes && !sleeps);
    g_agc_dev.fences[0]=nullptr;waited=99;
    assert(!agc_wait_slot_fence(0,1,3,1000,&waited));
    assert(waited==0 && !flushes && !sleeps);

    reset(true);
    assert(agc_wait_slot_fence(0,1,3,1000,&waited));
    assert(waited==0 && flushes==1 && !sleeps);
    assert(g_agc_dev.fence_expect[0]==1); // The caller retires ownership.
    assert(!agc_wait_slot_fence(0,1,0,1000,&waited));
    assert(waited==0 && flushes==1 && !sleeps);

    reset(false);values[0]=2; // A different completed marker is insufficient.
    assert(!agc_wait_slot_fence(0,1,3,1000,&waited));
    assert(waited==3 && flushes==3 && sleeps==3);
    assert(g_agc_dev.fence_expect[0]==1);

    reset(false);complete_on_sleep=true;
    assert(agc_wait_slot_fence(0,1,3,1000,&waited));
    assert(waited==1 && flushes==2 && sleeps==1);

    reset(false);complete_on_sleep=true;
    // Retirement during the last sleep is recognized only by a later read.
    assert(!agc_wait_slot_fence(0,1,1,1000,&waited));
    assert(waited==1 && flushes==1 && sleeps==1);
    assert(g_agc_dev.fence_expect[0]==1);
    assert(agc_wait_slot_fence(0,1,1,1000,nullptr));
    assert(flushes==2 && sleeps==1);
}
static void test_drain() {
    reset(false);g_agc_dev.initialized=0;
    assert(evo_agc_runtime_exit_ready(3) && !flushes && !sleeps);
    reset(false);
    for(int i=0;i<3;i++){g_agc_dev.fence_expect[i]=0;g_agc_dev.fences[i]=nullptr;}
    assert(evo_agc_runtime_exit_ready(3) && !flushes && !sleeps);

    reset(false);values[1]=2;
    assert(!evo_agc_runtime_exit_ready(3));
    assert(sleeps==6 && flushes==7);
    assert(g_agc_dev.fence_expect[0]==1 && !g_agc_dev.fence_expect[1] && g_agc_dev.fence_expect[2]==3);
    values[0]=1;values[2]=3;
    assert(evo_agc_runtime_exit_ready(3));
    assert(sleeps==6 && flushes==9);
    for(int i=0;i<3;i++)assert(!g_agc_dev.fence_expect[i]);
}
int main() {
    test_slot_wait();test_drain();
    reset(true);m_exitAfterPark=false;parked();assert(!exits && !flushes);
    reset(true);parked();assert(exits==1 && !m_exitAfterPark);parked();assert(exits==1);
    reset(false);parked();assert(!exits && !m_exitAfterPark && g_agc_dev.fence_expect[0]==1);
    for(int i=0;i<3;i++)values[i]=i+1;
    parked();assert(!exits); // A late retirement never retries the refused attempt.
    reset(true);g_agc_dev.fences[1]=nullptr;parked();assert(!exits && g_agc_dev.fence_expect[1]==2);
    reset(false);complete_on_sleep=true;parked();assert(exits==1 && sleeps==1 && flushes>=4);
    reset(true);markParked();assert(markers==1 && m_softClosed);
    reset(false);markParked();assert(!markers && m_softClosed);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='tube-phone-exit-') as directory:
    path=Path(directory)/'test.cpp';binary=Path(directory)/'test'
    path.write_text(source)
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: shipping self-exit occurs once after confirmed fence retirement; busy/missing fences remain parked, and media cleanup precedes park')
