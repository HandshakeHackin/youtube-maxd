/* Execute the shipping picker rows and selection without reopening playback. */
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
enum Kind { KindAudio, KindChat };
struct Entry { Kind kind=KindAudio; int streamIndex=-1; std::string label,detail,badge; };
struct Playback { bool available=true; bool canShowLiveChat() const {return available;} };
static int preference=0, changes=0;
static std::string notice;
static int tube_player_chat_preference() {return preference;}
static int tube_player_chat_set_mode(int mode) {
    if(mode<0 || mode>2)return 0;
    preference=mode;++changes;return 1;
}
static void toast(const char*,const char* text) {notice=text;}
struct Picker {
    Playback* playback;
    std::vector<Entry> m_tracks;
    int m_activeIndex=7;
    void append() {
#include "options.inc"
    }
    void select(const Entry& e) {
#include "selection.inc"
    }
};
int main() {
    Playback playback;
    for(int mode=0;mode<3;++mode) {
        preference=mode;Picker p{&playback,{},7};p.append();
        assert(p.m_tracks.size()==3 && p.m_activeIndex==7);
        for(int i=0;i<3;++i) {
            const auto& e=p.m_tracks[i];
            assert(e.kind==KindChat && e.streamIndex==i);
            assert((e.badge=="SELECTED")== (i==mode));
            p.select(e);assert(preference==i && notice==e.label);
        }
        assert(p.m_tracks[0].label=="Hide live chat");
    }
    playback.available=false;Picker hidden{&playback,{},0};hidden.append();
    assert(hidden.m_tracks.empty());
    int before=changes;Entry stale;stale.streamIndex=1;hidden.select(stale);
    assert(changes==before && notice=="Chat is unavailable for this playback");
    playback.available=true;stale.streamIndex=99;hidden.select(stale);
    assert(changes==before);
    puts("PASS: chat options, saved badge, preserved speed focus and stale selection guard");
}
