/* Record the shipping HUI/RML blocks, including text escaping and stable updates. */
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct Rect { float x,y,w,h; };
struct Color { Color with_alpha(float) const { return {}; } };
static Color kBlack,kWhite;
struct DrawList { std::vector<Rect> panels;std::vector<std::string> texts;
    void rounded_rect(Rect r,int,Color) {panels.push_back(r);} };
struct Fonts {int semibold=1,regular=2;} fonts;
struct Context {struct {Color accent;} palette;} ctx;
namespace ui {static void text(DrawList& l,int,const std::string& s,float,float,int,Color){l.texts.push_back(s);}}
static std::string fit(int,const char* s,int,float){return s;}
struct State {bool show_stats=false,scrub_active=false,music_mode=false;} raw_,m_last_state;
struct tube_player_chat_row_t {char author[128],message[1024];};
static tube_player_chat_row_t rows[8];static int mode=1,count=1;
static int tube_player_chat_mode(){return mode;}
static int tube_player_chat_rows(const tube_player_chat_row_t** value){*value=rows;return count;}
static const char* tube_player_chat_status(){return "Loading <chat> & waiting";}
struct Element {std::string display,left,markup;int writes=0;
    void SetProperty(const char* key,const char* value){if(!std::strcmp(key,"display"))display=value;else left=value;}
    std::string GetInnerRML(){return markup;}
    void SetInnerRML(const std::string& value){markup=value;++writes;} };
struct Document {std::map<std::string,Element> elements;
    Element* GetElementById(const std::string& id){return &elements[id];}} document;
static Document* m_playback_doc=&document;
static void draw(DrawList& list){
#include "draw.inc"
}
static void rml(){
#include "rml.inc"
}
int main(){
    std::strcpy(rows[0].author,"<Viewer>&");std::strcpy(rows[0].message,"<script>alert(1)</script> & text");
    DrawList list;draw(list);rml();
    assert(list.panels.size()==1 && list.panels[0].x==24 && list.texts.back()==rows[0].message);
    auto& panel=document.elements["player-chat"];auto& row=document.elements["player-chat-row-0"];
    assert(panel.display=="block" && panel.left=="24dp");
    assert(row.markup=="<b>&lt;Viewer&gt;&amp;</b><br/>&lt;script&gt;alert(1)&lt;/script&gt; &amp; text");
    int writes=row.writes;rml();assert(row.writes==writes);
    mode=2;list=DrawList{};draw(list);rml();assert(list.panels[0].x==1456 && panel.left=="1456dp");
    count=8;list=DrawList{};draw(list);rml();assert(list.texts.size()==13); // six bounded rows, plus title
    count=0;rml();assert(row.display=="none" && row.markup.empty());
    assert(document.elements["player-chat-status"].markup=="Loading &lt;chat&gt; &amp; waiting");
    for(int gate=0;gate<4;++gate){
        raw_=m_last_state=State{};mode=1;
        if(gate==0)raw_.show_stats=true;
        if(gate==1)raw_.scrub_active=true;
        if(gate==2)raw_.music_mode=true;
        if(gate==3)mode=0;
        m_last_state=raw_;list=DrawList{};draw(list);rml();
        assert(list.panels.empty() && panel.display=="none");
    }
    puts("PASS: shipping chat HUD placement, row bound, escaping, stable updates and suppression");
}
