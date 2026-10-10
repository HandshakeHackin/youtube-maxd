/* Actual generated marker rendering, mocked draw list and DOM.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_sponsor_draw.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
struct Rect { float x, y, w, h; };
struct Color { unsigned rgb_value; static Color rgb(unsigned value) { return {value}; } };
struct DrawList {
    std::vector<Rect> rectangles;
    void rounded_rect(Rect r, int, Color) { rectangles.push_back(r); }
};
static tube_sponsor_marker_t source_markers[128];
static int source_count;
extern "C" int tube_sponsor_markers(double duration, tube_sponsor_marker_t* markers, int capacity) {
    assert(duration == 100 && capacity == 128);
    std::memcpy(markers, source_markers, source_count * sizeof *markers); return source_count;
}
static struct { double duration_sec = 100; } raw_, m_last_state;
struct Element {
    std::string markup; int writes = 0;
    std::string GetInnerRML() { return markup; }
    void SetInnerRML(const std::string& text) { markup = text; ++writes; }
} element;
struct Document { Element* GetElementById(const char* id) { assert(!std::strcmp(id, "sponsor-markers")); return &element; } } document;
static Document* m_playback_doc = &document;
static void kit(DrawList& list, const Rect& track) {
#include "kit.inc"
}
static void rml() {
#include "rml.inc"
}
int main() {
    source_markers[0] = {.1, .2, 0x00ff00, 0};
    source_markers[1] = {1, 1, 0xff1684, 1};
    source_markers[2] = {.5, .5, 0xff1684, 1};
    source_count = 3;
    DrawList list; Rect track{96, 920, 1728, 8}; kit(list, track);
    assert(list.rectangles.size() == 3);
    for (auto r : list.rectangles) assert(r.x >= track.x && r.x + r.w <= track.x + track.w && r.w >= 1 && r.h == 8);
    assert(list.rectangles[1].w == 4 && list.rectangles[1].x + 4 == track.x + track.w);
    rml(); assert(element.writes == 1 && element.markup.find("left:10.000000%;width:10.000000%;background-color:#00ff00") != std::string::npos);
    assert(element.markup.find("left:99.777778%;width:0.222222%;background-color:#ff1684") != std::string::npos);
    rml(); assert(element.writes == 1); // Paused/unchanged frames do not rewrite the DOM.
    source_count = 0; rml(); assert(element.markup.empty() && element.writes == 2);
    double left, width;
    assert(!tube_sponsor_geometry(nullptr, 1800, &left, &width));
    tube_sponsor_marker_t invalid{NAN, .3, 0, 0};
    assert(!tube_sponsor_geometry(&invalid, 1800, &left, &width));
    invalid = {.3, .3, 0, 0}; assert(!tube_sponsor_geometry(&invalid, 1800, &left, &width));
    invalid = {.3, 1.1, 0, 0}; assert(!tube_sponsor_geometry(&invalid, 1800, &left, &width));
    invalid = {.3, .2, 0, 0}; assert(!tube_sponsor_geometry(&invalid, 1800, &left, &width));
    invalid = {.5, .5, 0, 1};
    assert(tube_sponsor_geometry(&invalid, 2, &left, &width) && left == 0 && width == 2);
    assert(!tube_sponsor_geometry(&invalid, INFINITY, &left, &width));
    puts("PASS: actual PS5 UI-kit/fallback marker rendering, point/interval geometry, end clipping, invalid geometry and DOM lifecycle");
}
