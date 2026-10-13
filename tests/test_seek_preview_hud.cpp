// The seek-preview block of the generated player controls, compiled as it
// ships against a recording draw list: the tile under the seek target is cut
// out of its sheet and drawn above the seek bar with the target time, the
// carousel's neighbours beside it, and nothing while no sheet is held.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "tube_seek_sheets.h"
extern "C" {
#include "tube_player_extra.h"
#include "cJSON.h"
}

struct Color {
    unsigned rgb; float alpha;
    Color with_alpha(float value) const { return Color{rgb, value}; }
};
static const Color kWhite{0xffffff, 1.0f}, kBlack{0x000000, 1.0f};
struct Rect {
    float x, y, w, h;
    float cx() const { return x + w * 0.5f; }
};
enum class Align { left, center, right };
struct Call { std::string kind, text; Rect rect, uv; float radius, opacity; Color color; std::uint32_t texture; };
struct DrawList {
    std::vector<Call> calls; std::vector<float> opacity{1.0f};
    void push_opacity(float value) { opacity.push_back(opacity.back() * value); }
    void pop_opacity() { assert(opacity.size() > 1); opacity.pop_back(); }
    void rounded_rect(const Rect& r, float radius, Color color) { calls.push_back({"rect", "", r, {}, radius, opacity.back(), color, 0}); }
    void image(std::uint32_t texture, const Rect& r, const Rect& uv, Color tint, float radius = 0.0f) {
        calls.push_back({"image", "", r, uv, radius, opacity.back(), tint, texture});
    }
};
struct FontRef { float measure(const std::string& text, float size) const { return static_cast<float>(text.size()) * size * 0.5f; } };
struct Fonts { FontRef mono; };
namespace ui {
static void text(DrawList& list, const FontRef&, const std::string& text, float x, float baseline, float size, Color color, Align align) {
    assert(align == Align::center);
    list.calls.push_back({"text", text, {x, baseline, size, 0}, {}, 0, list.opacity.back(), color, 0});
}
}
struct Upload { const std::uint32_t* data; int width, height; std::uint64_t tag; };
struct Textures {
    std::vector<Upload> uploads; bool fail = false;
    std::uint32_t pixels(const std::uint32_t* data, int width, int height, std::uint64_t tag) {
        uploads.push_back({data, width, height, tag});
        return fail ? 0 : 100 + static_cast<std::uint32_t>(uploads.size());
    }
};
struct Context { Textures& textures; };
static std::string clock_text(double seconds) {
    const long s = static_cast<long>(seconds); char b[32];
    if (s >= 3600) std::snprintf(b, sizeof b, "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
    else std::snprintf(b, sizeof b, "%ld:%02ld", s / 60, s % 60);
    return b;
}
static struct { double scrub_target, duration_sec; } raw_;
static struct { float value; } scrub_;

/* The scrub branch of OsdScreen::draw_bottom around the generated block. */
static void draw(DrawList& list, Context ctx, const Fonts& fonts) {
    const Rect track{96, 920, 1728, 8};
    const float t = static_cast<float>(raw_.scrub_target / raw_.duration_sec);
    const float sx = track.x + track.w * t;
#include "seek_preview.inc"
}

static std::vector<std::pair<int, unsigned>> requests;
static std::uint64_t now_ms;
static int fetch(const char*, int slot, unsigned ticket) { requests.push_back({slot, ticket}); return 0; }
static std::uint64_t now() { return now_ms; }
static int decode(const char*, size_t, std::uint32_t** pixels, int* width, int* height, long) {
    *width = 800; *height = 450;
    *pixels = static_cast<std::uint32_t*>(std::calloc(800 * 450, 4));
    return *pixels != nullptr;
}
static void rest(double seconds, int mode) {
    raw_.scrub_target = seconds;
    tube_seek_view view;
    now_ms += 1000; tube_seek_preview_frame(seconds, raw_.duration_sec, 960, 868, &view);
    now_ms += tube_sheets_debounce_ms(mode); tube_seek_preview_frame(seconds, raw_.duration_sec, 960, 868, &view);
    for (auto& request : requests) tube_seek_preview_deliver(request.first, request.second, 1, "sheet", 5);
    requests.clear();
}
static bool same(const Rect& a, float x, float y, float w, float h) {
    return std::fabs(a.x - x) < 0.01f && std::fabs(a.y - y) < 0.01f && std::fabs(a.w - w) < 0.01f && std::fabs(a.h - h) < 0.01f;
}

int main() {
    TubePlayerExtra prefs; tube_player_extra_defaults(&prefs); tube_player_extra_select(&prefs);
    tube_seek_preview_hooks(fetch, decode, now);
    cJSON* levels = cJSON_Parse("[{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":250,\"sheets\":10,\"interval_ms\":1000,"
                                "\"url\":\"http://127.0.0.1:8765/media/cap/sb2-$M.jpg\"}]");
    assert(levels && tube_extra_set_storyboards(levels) == 1); cJSON_Delete(levels);
    raw_.duration_sec = 250; scrub_.value = 0.8f;
    Fonts fonts; Textures textures; Context ctx{textures};

    /* Previews are a choice of the profile: off draws nothing and asks for nothing, however long the target rests. */
    if (!tube_extra_seek_preview()) {
        rest(125, 1);
        DrawList list; raw_.scrub_target = 125; draw(list, ctx, fonts); assert(list.calls.empty() && textures.uploads.empty());
    }
    /* Single frame: nothing is drawn until the sheet is held. */
    assert(tube_extra_set_seek_preview(1)); now_ms += 5000;
    { DrawList list; raw_.scrub_target = 125; draw(list, ctx, fonts); assert(list.calls.empty() && textures.uploads.empty() && list.opacity.size() == 1); }
    rest(125, 1);
    { DrawList list; draw(list, ctx, fonts);
      /* Second 125 of 250 sits in the middle of the bar: tile 125 is cell 0 of sheet 5. */
      assert(list.calls.size() == 4 && list.opacity.size() == 1 && textures.uploads.size() == 1);
      assert(textures.uploads[0].width == 800 && textures.uploads[0].height == 450 && textures.uploads[0].data);
      const Call& frame = list.calls[0]; const Call& image = list.calls[1]; const Call& plate = list.calls[2]; const Call& time = list.calls[3];
      assert(frame.kind == "rect" && same(frame.rect, 797, 685, 326, 186) && frame.color.rgb == 0xffffff && frame.color.alpha == 1.0f);
      assert(image.kind == "image" && same(image.rect, 800, 688, 320, 180) && image.texture == 101 && image.radius == 8);
      assert(same(image.uv, 0.0f, 0.0f, 0.2f, 0.2f) && image.color.alpha == 1.0f && std::fabs(image.opacity - 0.8f) < 1e-6f);
      assert(image.rect.y + image.rect.h < 920 - 14);                      /* above the seek bar and its mark */
      assert(plate.kind == "rect" && plate.color.rgb == 0 && std::fabs(plate.rect.x + plate.rect.w * 0.5f - 960) < 0.01f && plate.rect.y > 688 && plate.rect.y + plate.rect.h < 868);
      assert(time.kind == "text" && time.text == "2:05" && time.rect.x == 960 && time.rect.y < 868 && time.rect.y > plate.rect.y); }
    /* Another second of the same sheet: another cell, the same texture pixels. */
    { DrawList list; raw_.scrub_target = 137; draw(list, ctx, fonts);
      assert(list.calls.size() == 4 && same(list.calls[1].uv, 0.4f, 0.4f, 0.2f, 0.2f) && list.calls[3].text == "2:17");
      assert(textures.uploads.back().data == textures.uploads[0].data);
      assert(list.calls[1].rect.x > 800 && list.calls[1].rect.x + list.calls[1].rect.w <= 1824); }
    /* Near the ends of the bar the tile stays inside the margins. */
    rest(1, 1);
    { DrawList list; draw(list, ctx, fonts); assert(list.calls.size() == 4 && same(list.calls[1].rect, 96, 688, 320, 180) && same(list.calls[1].uv, 0.2f, 0.0f, 0.2f, 0.2f)); }
    /* A texture that cannot be made draws nothing for that tile. */
    textures.fail = true;
    { DrawList list; draw(list, ctx, fonts); assert(list.calls.empty() && list.opacity.size() == 1); }
    textures.fail = false;

    /* The carousel: two smaller neighbours on each side, drawn first, the target's tile last. */
    assert(tube_extra_set_seek_preview(2));
    rest(137, 2);
    { DrawList list; draw(list, ctx, fonts);
      int images = 0; float target_x = 0;
      for (size_t i = 0; i < list.calls.size(); ++i) if (list.calls[i].kind == "image") {
          ++images;
          const bool target = list.calls[i].rect.w == 320;
          assert(target == (images == 5) && list.calls[i].color.alpha == (target ? 1.0f : 0.75f));
          assert(list.calls[i - 1].kind == "rect" && list.calls[i - 1].color.rgb == (target ? 0xffffffu : 0u));
          assert(std::fabs(list.calls[i].uv.w - 0.2f) < 1e-6f && list.calls[i].rect.y + list.calls[i].rect.h == 868);
          if (target) target_x = list.calls[i].rect.x; else assert(list.calls[i].rect.w == 192 && list.calls[i].rect.h == 108);
      }
      assert(images == 5 && list.calls.size() == 12 && list.calls.back().kind == "text" && target_x > 96); }
    /* Previews switched off: nothing, at once. */
    assert(tube_extra_set_seek_preview(0));
    { DrawList list; draw(list, ctx, fonts); assert(list.calls.empty()); }
    /* Hours on the clock. */
    assert(tube_extra_set_seek_preview(1));
    levels = cJSON_Parse("[{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":100,\"sheets\":4,\"interval_ms\":0,"
                         "\"url\":\"http://127.0.0.1:8765/media/long/sb2-$M.jpg\"}]");
    assert(levels && tube_extra_set_storyboards(levels) == 1); cJSON_Delete(levels);
    raw_.duration_sec = 7200; rest(3725, 1);
    { DrawList list; draw(list, ctx, fonts); assert(list.calls.size() == 4 && list.calls[3].text == "1:02:05"); }
    tube_seek_preview_drop(); assert(!tube_seek_preview_bytes());
    std::puts("PASS: seek-preview tiles as the player controls draw them");
    return 0;
}
