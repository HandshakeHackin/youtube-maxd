"""Compile actual GPU drain and parked self-exit dispatch with hostile fences."""
from pathlib import Path
import os
import subprocess
import tempfile

work=Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK',Path.home()/'.cache/prosperotube/native'))
player=work/'projects/evoplayer'
gpu=(player/'media/src/evo_agc_runtime.c').read_text()
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
static void evo_agc_runtime_cache_flush(const void*,int) { ++flushes; }
static void sceKernelUsleep(unsigned us) {
    assert(us==1000);++sleeps;
    if(complete_on_sleep)for(int i=0;i<3;i++)values[i]=g_agc_dev.fence_expect[i];
}
static int sceSystemServiceLoadExec(const char* path,const char** args) {
    assert(!strcmp(path,"exit") && !args);
    for(int i=0;i<3;i++)assert(!g_agc_dev.fence_expect[i]);
    ++exits;return -7;
}
'''+drain+'\nstatic void markParked() { for (int i=0;i<1;i++) {\n'+mark+'\n} }\nstatic void parked() {\n'+close+r'''
}
static void reset(bool completed) {
    flushes=sleeps=exits=markers=0;complete_on_sleep=false;m_exitAfterPark=true;m_softCloseFrames=150;
    for(int i=0;i<3;i++){values[i]=completed?i+1:0;g_agc_dev.fences[i]=&values[i];g_agc_dev.fence_expect[i]=i+1;}
}
int main() {
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
