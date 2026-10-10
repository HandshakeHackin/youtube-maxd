/* Shipping review blocks with a recording renderer and DOM. */
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
struct Rect { float x, y, w, h; };
struct Color { float alpha = 1; Color with_alpha(float a) const { return {a}; } };
static Color kBlack, kClear, kWhite;
struct DrawList {
    int panels = 0; std::vector<std::string> texts;
    void rounded_rect(Rect r, int, Color c) { assert(r.w > 0 && c.alpha < 1); ++panels; }
    void bordered_rect(Rect, int, Color, int, Color) {}
};
struct Fonts { int semibold = 1, regular = 2; } fonts;
struct Context { struct { Color accent; } palette; } ctx;
namespace ui { static void text(DrawList& l, int, const std::string& s, float, float, int, Color) { l.texts.push_back(s); } }
static std::string fit(int, const char* s, int, float) { return s; }
struct State { bool show_stats=false, scrub_active=false, music_mode=false, is_live=false; double position_sec=15; } raw_, m_last_state;
static const char* message = "Skip sponsor? Cross: skip   Circle: keep";
static const char* tube_sponsor_prompt(double position) { return position >= 10 && position < 20 ? message : nullptr; }
struct Focus { std::vector<int> focusable; std::string id, text; int index=0, total=0; Rect rect{}; };
struct Element {
    std::string display, markup; int writes=0;
    void SetProperty(const char*, const char* value) { display=value; }
    std::string GetInnerRML() { return markup; }
    void SetInnerRML(const std::string& value) { markup=value; ++writes; }
} panel, text;
struct Document {
    Element* GetElementById(const std::string& id) { return id == "sponsor-review" ? &panel : &text; }
} document;
static Document* m_playback_doc=&document;
static void draw(DrawList& list) {
#include "draw.inc"
}
static bool focus(Focus* out) {
#include "focus.inc"
    return false;
}
static void rml() {
#include "rml.inc"
}
int main() {
    DrawList list; Focus f; draw(list); rml();
    assert(list.panels==1 && list.texts.back()==message && panel.display=="block");
    assert(focus(&f) && f.id=="sponsor-review" && f.text==message && f.total==1);
    int writes=text.writes; rml(); assert(text.writes==writes);
    for (int gate=0; gate<6; ++gate) {
        raw_=State{};
        if (gate==0) raw_.show_stats=true;
        if (gate==1) raw_.scrub_active=true;
        if (gate==2) raw_.music_mode=true;
        if (gate==3) raw_.is_live=true;
        if (gate==4) raw_.position_sec=9;
        if (gate==5) raw_.position_sec=20;
        m_last_state=raw_; list=DrawList{}; draw(list); rml();
        assert(list.panels==0 && !focus(&f) && panel.display=="none" && text.markup.empty());
    }
    raw_=m_last_state=State{}; message="<Skip> & keep"; rml();
    assert(text.markup=="&lt;Skip&gt; &amp; keep");
    message=nullptr; rml(); assert(panel.display=="none" && text.markup.empty());
    puts("PASS: shipping SponsorBlock review HUD, focus, fallback DOM, suppression and expiry");
}
