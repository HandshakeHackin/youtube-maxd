#include "afterglow/player/player.hpp"
#include <cassert>
#include <cmath>
using namespace afterglow;
static void same(Rect a,Rect b){assert(a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h);}
int main(){
    ui::Fonts fonts{};player::Player p(fonts,nullptr);player::PlayerView v;
    v.chat.side=2;v.chat.theatre=true;
    p.prepare_video(0,0);p.update(v,1.f/60);same(p.video_rect(),{0,0,1920,1080});
    for(int side:{2,1}){
        v.chat.side=side;
        for(int frame=0;frame<180;++frame){
            p.prepare_video(side,1.f/60);auto before=p.video_rect();float radius=p.video_radius();
            p.update(v,1.f/60);same(before,p.video_rect());assert(radius==p.video_radius());
        }
        auto r=p.video_rect();assert(std::fabs(r.x-(side==1?552:56))<.001&&std::fabs(r.y-164)<.001&&std::fabs(r.w-1312)<.001&&std::fabs(r.h-738)<.001);assert(std::fabs(p.video_radius()-22)<.001);
    }
    p.prepare_video(0,1.f/60);same(p.video_rect(),{0,0,1920,1080});assert(p.video_radius()==0);
    v.chat.theatre=false;p.update(v,1.f/60);same(p.video_rect(),{0,0,1920,1080});
}
