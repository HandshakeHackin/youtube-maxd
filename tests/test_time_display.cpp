/* Real formatter plus shipping renderer/picker blocks. */
#include "tube_time_display.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
static int mode=1;
static double rate=1;
static time_t wall=1704067200;
static int tube_player_time_mode(){return mode;}
static int tube_player_set_time_mode(int value){if(value<0 || value>2)return 0;mode=value;return 1;}
static double evo_audio_rate(){return rate;}
static time_t test_wall(time_t*){return wall;}
#define time test_wall
struct Element {
    std::string text;int writes=0;
    std::string GetInnerRML(){return text;}
    void SetInnerRML(const char* value){text=value;++writes;}
} element;
struct Document {Element* GetElementById(const char* id){assert(!strcmp(id,"time-duration"));return &element;}} doc;
static Document* m_playback_doc=&doc;
static bool m_frame_dirty;
struct State {bool scrub_active=false,is_live=false;double scrub_target=0,position_sec=40,duration_sec=100;} state,raw_;
static void rml(){
#include "rml.inc"
}
struct Color {Color with_alpha(float)const{return {};}}kWhite;
struct Fonts {int mono=1;}fonts;
struct Track {float x=0,w=100;}track;
struct List {std::string text;}list;
enum class Align {right};
namespace ui {static void text(List& target,int,const char* text,float,int,int,Color,Align){target.text=text;}}
static void hui(){
#include "hui.inc"
}
enum {KindTime};
struct Entry {int kind=0,streamIndex=0;std::string label,detail,badge;};
static std::vector<Entry> m_tracks;
static std::string message;
static void toast(const char*,const char* text){message=text;}
static void rows(){m_tracks.clear();
#include "rows.inc"
}
static void select(const Entry& e){
#include "selection.inc"
}
static void expect(int value,double position,double duration,double speed,int live,time_t now,const char* expected){
    char text[96];tube_time_display(text,sizeof text,value,position,duration,speed,live,now);assert(!strcmp(text,expected));
}
int main(){
    setenv("TZ","UTC",1);tzset();
    expect(0,40,100,2,0,wall,"01:40");
    expect(1,40,100,2,0,wall,"-00:30 / 01:40");
    expect(1,40,100,.25,0,wall,"-04:00 / 01:40");
    expect(2,40,100,1,0,wall,"Ends 00:01 / 01:40");
    expect(2,40,100,2,0,wall+86390,"Ends 00:00 / 01:40");
    expect(1,99.9,100,1,0,wall,"-00:01 / 01:40");
    expect(1,101,100,1,0,wall,"-00:00 / 01:40");
    expect(1,-10,100,0,0,wall,"-01:40 / 01:40");
    expect(1,NAN,100,NAN,0,wall,"-01:40 / 01:40");
    expect(2,40,100,1,0,-1,"-01:00 / 01:40");
    expect(2,40,100,1,1,wall,"LIVE");
    expect(2,40,0,1,0,wall,"--:--");
    expect(1,40,INFINITY,1,0,wall,"--:--");
    expect(0,0,3661,1,0,wall,"1:01:01");
    char small[3]={'x','x','x'};tube_time_display(small,sizeof small,2,40,100,1,0,wall);assert(small[2]==0);
    tube_time_display(nullptr,0,0,0,0,1,0,wall);
    for(mode=0;mode<3;++mode){rate=2;rml();hui();assert(element.text==list.text);}
    mode=2;rml();int writes=element.writes;m_frame_dirty=false;rml();assert(element.writes==writes && !m_frame_dirty);
    wall+=60;rml();assert(element.writes==writes+1 && m_frame_dirty); // Paused state does not change.
    mode=1;rate=2;state.scrub_active=raw_.scrub_active=true;state.scrub_target=raw_.scrub_target=80;
    rml();hui();assert(element.text=="-00:10 / 01:40" && list.text==element.text);
    state.is_live=raw_.is_live=true;rml();hui();assert(element.text=="LIVE" && list.text=="LIVE");
    mode=1;rows();assert(m_tracks.size()==3 && m_tracks[1].badge=="SELECTED");
    select(m_tracks[2]);assert(mode==2 && message==m_tracks[2].label);
    puts("PASS: shipping speed-corrected time labels, paused refresh, live/invalid bounds and picker");
}
