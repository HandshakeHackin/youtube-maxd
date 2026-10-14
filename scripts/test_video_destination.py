"""Compile the generated application lease and AGC destination mapping with real geometry."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
work=Path(os.environ['PROSPEROTUBE_NATIVE_WORK'])/'projects/evoplayer'
app=(work/'core/src/Application.cpp').read_text()
agc=(work/'media/src/evo_agc_runtime.c').read_text()
mount=(root/'native/afterglow/player/app/mount.cpp').read_text()
def section(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
lease=section(app,'        auto layoutPlayer','        // End ProsperoTube video destination lease.')
body=section(mount,'extern "C" void tube_afterglow_video_prepare','\n}',)+ '\n}'
if 'g_tube_frame_geometry' not in agc:
    import sys
    sys.path.insert(0,str(root/'scripts'))
    from video_transform_overlay import apply_video_transform_overlay
    def replace(path,old,new,count=1):
        global agc
        assert agc.count(old)==count,old
        agc=agc.replace(old,new)
    apply_video_transform_overlay(replace,'',destination=True)
snapshot=section(agc,'    tube_video_destination_get(g_agc_dev.width','    float sx, sy;')
final=section(agc,'    if(dst->scanout)\n        tube_video_transform_rect','    if (agc_up_bind_target')
direct=section(agc,'    /* 2. Destination viewport','    /* 3.')
scale=section(agc,'static void agc_video_scale(','\nint evo_agc_blit_yuv(')
source=r"""
#include "tube_video_destination.h"
#include "tube_video_transform.h"
#include "tube_video_geometry.h"
#include <cassert>
#include <cmath>
struct Rect{float x,y,w,h;};
struct FakePlayer {int side=0;void prepare_video(int s,float){side=s;}Rect video_rect(){return side==1?Rect{552,164,1312,738}:Rect{56,164,1312,738};}} player;
namespace afterglow {struct Mount{bool video_eligible=false;FakePlayer*player=&::player;};static Mount instance;static Mount&mount(){return instance;}}
static bool afterglow_on=true;static int chat=2;
static int tube_afterglow_enabled(){return afterglow_on;}static int tube_player_chat_mode(){return chat;}
"""+body+r"""
struct PlayerScreen{bool stats=false;bool isStatsForNerdsVisible(){return stats;}} screen;
struct Manager{void*current=&screen;void*getCurrentScreen(){return current;}} manager;
struct Controller{bool active=true,music=false,scrub=false;bool isActive(){return active;}bool isMusicMode(){return music;}bool isScrubbing(){return scrub;}} controller;
static bool phone=false;static bool tube_phone_owns_media(){return phone;}
static struct{int state=0;}s_upcmp;
static bool isPlayer=true,m_softClose=false;static Manager*m_screenManager=&manager;static Controller*m_playbackController=&controller;static double frameDeltaMs=16;
static void lease(){
"""+lease+r"""
}
static struct{int width=1920,height=1080;}g_agc_dev;
static unsigned config=1,g_tube_frame_geometry,g_tube_frame_transform;
static int g_tube_canvas_w,g_tube_canvas_h;
static tube_video_destination g_tube_frame_destination;
static unsigned tube_player_video_geometry(){return config;}
static void snapshot(){
"""+snapshot+r"""
}
"""+scale+r"""
struct Surface{int scanout;};
static void finalpass(Surface*dst,int&x,int&y,int&w,int&h){
"""+final+r"""
}
static tube_video_destination viewport;static int calls;
static void up_viewport_scissor(int x,int y,int w,int h,int,int){viewport={x,y,w,h};++calls;}
static void directpass(int target_slot){
"""+direct+r"""
}
static void full(){tube_video_destination d;tube_video_destination_get(1920,1080,&d);assert(d.x==0&&d.y==0&&d.w==1920&&d.h==1080);}
static void active(){lease();tube_video_destination d;tube_video_destination_get(1920,1080,&d);assert(d.x==56&&d.w==1312);}
int main(){
    active();isPlayer=false;lease();full();isPlayer=true;
    active();m_softClose=true;lease();full();m_softClose=false;
    active();manager.current=nullptr;lease();full();manager.current=&screen;
    active();m_playbackController=nullptr;lease();full();m_playbackController=&controller;
    active();controller.active=false;lease();full();controller.active=true;
    active();controller.music=true;lease();full();controller.music=false;
    active();controller.scrub=true;lease();full();controller.scrub=false;
    active();screen.stats=true;lease();full();screen.stats=false;
    active();phone=true;lease();full();phone=false;
    active();s_upcmp.state=1;lease();full();s_upcmp.state=0;
    active();afterglow_on=false;lease();full();afterglow_on=true;
    active();chat=0;lease();full();chat=2;
    active();afterglow::instance.player=nullptr;lease();full();afterglow::instance.player=&player;
    for(float bad:{NAN,INFINITY,-1.f}){tube_video_destination_set(bad,0,1312,738);full();tube_video_destination_set(56,0,bad,738);full();}
    tube_video_destination_set(1900,0,1312,738);full();tube_video_destination_set(0,0,0,0);full();
    for(int resolution:{1,2})for(int side:{1,2})for(unsigned turn=0;turn<8;++turn){
        chat=side;lease();g_agc_dev.width=1920*resolution;g_agc_dev.height=1080*resolution;
        config=1u|(turn<<20);snapshot();assert(g_tube_frame_transform==turn);
        tube_video_destination d;tube_video_destination_get(g_agc_dev.width,g_agc_dev.height,&d);
        assert(d.x==(side==1?552:56)*resolution&&d.y==164*resolution&&d.w==1312*resolution&&d.h==738*resolution);
        int x=0,y=0,w=g_tube_canvas_w,h=g_tube_canvas_h;Surface out{1};finalpass(&out,x,y,w,h);
        assert(x==d.x&&y==d.y&&w==d.w&&h==d.h);
        float sx,sy;agc_video_scale(640,480,0,&sx,&sy);
        assert(std::fabs(sx/sy*g_tube_canvas_w/g_tube_canvas_h-4.f/3)<.0001);
        float fitx=sx,fity=sy;config|=150u<<4;snapshot();agc_video_scale(640,480,0,&sx,&sy);
        assert(std::fabs(sx/fitx-1.5f)<.0001&&std::fabs(sy/fity-1.5f)<.0001);
        x=10;y=20;w=100;h=200;int px=(turn&4)?g_tube_canvas_w-110:10;
        finalpass(&out,x,y,w,h);
        int ex=px,ey=20,ew=100,eh=200;
        switch(turn&3){case 1:ex=d.w-220;ey=px;ew=200;eh=100;break;
        case 2:ex=d.w-px-100;ey=d.h-220;break;
        case 3:ex=20;ey=d.h-px-100;ew=200;eh=100;break;}
        assert(x==d.x+ex&&y==d.y+ey&&w==ew&&h==eh);
        int lx=7,ly=11,lw=129,lh=87;Surface scratch{0};finalpass(&scratch,lx,ly,lw,lh);assert(lx==7&&ly==11&&lw==129&&lh==87);
        directpass(-1);assert(viewport.x==d.x&&viewport.y==d.y&&viewport.w==d.w&&viewport.h==d.h);int before=calls;directpass(0);assert(calls==before);
    }
    tube_video_destination_reset();full();
}
"""
# Both rotation and mirror bits come from the public geometry representation.
source=source.replace('#include <cmath>','#include <cmath>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='tube-destination-') as temp:
    temp=Path(temp);(temp/'test.cpp').write_text(source)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'native'),str(temp/'test.cpp'),'-x','c++',str(root/'native/tube_video_destination.c'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
print('PASS generated application destination ownership, direct/final/scratch mapping, sides, resolutions and eight transforms')
