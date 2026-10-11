/* Record actual HUI/RML hints so gesture labels match the shipping input path. */
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
struct State { bool scrub_active=false, paused=false; } raw_, state;
namespace ui {
enum class Button { cross, circle };
struct Hint { Button button; const char *label; };
}
namespace Rml {
struct Element { std::string text; void SetInnerRML(const std::string& value) { text=value; } };
}
struct Document {
    std::map<std::string,Rml::Element> elements;
    Rml::Element* GetElementById(const char *id) { return &elements[id]; }
} document;
static Document *m_playback_doc=&document;
static void hui() {
    ui::Hint hints[2]; int n=0;
#include "hui.inc"
    assert(n==2 && hints[0].button==ui::Button::cross && hints[1].button==ui::Button::circle);
    assert(std::string(hints[0].label)==(raw_.scrub_active ? "Confirm seek" : raw_.paused ? "Play" : "Pause"));
    assert(std::string(hints[1].label)==(raw_.scrub_active ? "Cancel seek" : "Back"));
}
static void rml() {
#include "rml.inc"
    std::string h="OPUS"; auto *e=m_playback_doc->GetElementById("music-hint");
#include "music.inc"
}
int main() {
    for (bool scrub:{false,true}) for (bool paused:{false,true}) {
        raw_=state={scrub,paused}; hui(); rml();
        assert(document.elements["label-playpause"].text==(scrub ? "CONFIRM SEEK" : paused ? "PLAY" : "PAUSE"));
        assert(document.elements["label-back"].text==(scrub ? "CANCEL SEEK" : "BACK"));
        const auto& music=document.elements["music-hint"].text;
        assert(music.find(scrub ? "CROSS CONFIRM" : paused ? "CROSS PLAY" : "CROSS PAUSE")!=std::string::npos);
        assert((music.find("CIRCLE CANCEL")!=std::string::npos)==scrub);
    }
    puts("PASS: shipping video and music seek-confirm/cancel labels on HUI and RML");
}
