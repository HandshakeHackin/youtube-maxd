#include "tube_video_geometry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
static unsigned config;
static unsigned tube_player_video_geometry(){return config;}
struct {int width=1920,height=1080;}g_agc_dev;
#include "scale.inc"
static void expect(unsigned value,int width,int height,float x,float y){config=value;float sx,sy;agc_video_scale(width,height,0,&sx,&sy);assert(std::fabs(sx-x)<.0001 && std::fabs(sy-y)<.0001);}
struct Element {std::string text;int writes=0;std::string GetInnerRML(){return text;}void SetInnerRML(const std::string& value){text=value;++writes;}}element;
struct Doc {Element* GetElementById(const char* id){assert(!strcmp(id,"label-aspect"));return &element;}}doc;
static Doc* m_playback_doc=&doc;
static bool m_frame_dirty;
struct {int view_mode=0;}state;
static void rml(){
#include "rml.inc"
}
static int mode,zoom;
static int tube_player_resize_mode(){return mode;}
static int tube_player_zoom_percent(){return zoom;}
static int tube_player_set_resize_mode(int value){mode=value;zoom=0;return 1;}
static int tube_player_set_zoom_percent(int value){zoom=value;mode=0;return 1;}
static void tube_video_geometry_enable(int enabled){config=enabled?((unsigned)mode<<1|(unsigned)zoom<<4|1):0;}
enum class ViewMode {Fit,Fill,Stretch};
struct PlaybackController {
    ViewMode m_viewMode=ViewMode::Fit;struct {std::string provider="youtube";}m_source;int applies=0;
    void cycleViewMode();void setViewMode(ViewMode value);void applyViewMode(){++applies;}
};
#include "controller.inc"
enum {KindResize,KindZoom};
struct Entry {int kind=0,streamIndex=0;std::string label,detail,badge;};
static std::vector<Entry> m_tracks;
static std::string message;
static void toast(const char*,const char* value){message=value;}
static void rows(){m_tracks.clear();
#include "rows.inc"
}
static void select(const Entry& e){
#include "selection.inc"
}
int main(){
    expect(0,640,480,.75f,1);expect(1,640,480,.75f,1);expect(3,640,480,1,4.f/3);
    expect(5,640,480,1,1);expect(7,640,480,1,4.f/3);expect(9,640,480,.75f,1);
    expect((150u<<4)|1,640,480,1.125f,1.5f);expect((50u<<4)|1,1920,1080,.5f,.5f);
    expect((300u<<4)|1,1920,1080,3,3);
    for(int p=50;p<=300;++p)if(tube_zoom_valid(p))for(int w:{480,1920,3840}) {
        float fitx,fity,sx,sy;config=1;agc_video_scale(w,1080,0,&fitx,&fity);
        config=(unsigned)p<<4|1;agc_video_scale(w,1080,0,&sx,&sy);
        assert(std::fabs(sx/fitx-p/100.f)<.0001 && std::fabs(sy/fity-p/100.f)<.0001);
    }
    float sx=.5f,sy=.75f;tube_video_geometry_scale((101u<<4)|1,640,480,1920,1080,&sx,&sy);assert(sx==.5f && sy==.75f);
    tube_video_geometry_scale(1,0,480,1920,1080,&sx,&sy);assert(sx==.5f && sy==.75f);
    tube_video_geometry_scale(1,640,480,1920,1080,nullptr,&sy);
    config=150u<<4|1;rml();assert(element.text=="ASPECT: Zoom 150%");int writes=element.writes;m_frame_dirty=false;
    rml();assert(writes==element.writes && !m_frame_dirty);config=7;rml();assert(element.text=="ASPECT: Fit width" && m_frame_dirty);
    config=0;state.view_mode=2;rml();assert(element.text=="ASPECT: Stretch");
    rows();assert(m_tracks.size()==60 && m_tracks[0].badge=="SELECTED" && m_tracks.back().streamIndex==300);
    select(m_tracks.back());assert(zoom==300 && mode==0);rows();assert(m_tracks.back().badge=="SELECTED");
    select(m_tracks[1]);assert(mode==3 && !zoom && message=="Fit width");
    PlaybackController playback;mode=1;zoom=0;playback.cycleViewMode();
    assert(playback.m_viewMode==ViewMode::Stretch && mode==2 && config==5);
    zoom=150;mode=0;playback.cycleViewMode();assert(playback.m_viewMode==ViewMode::Fit && mode==0 && !zoom && config==1);
    mode=4;playback.cycleViewMode();assert(mode==0 && config==1);
    playback.m_source.provider="file";zoom=150;config=0;playback.cycleViewMode();
    assert(playback.m_viewMode==ViewMode::Fill && zoom==150 && !config);
    puts("PASS: shipping AGC geometry, all zoom levels, paused sizing labels, picker and Triangle controller");
}
