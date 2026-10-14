// ProsperoTube Afterglow - the player's mount: EVO's player state, drawn in the Afterglow design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fills a PlayerView (view.hpp) from what EVO hands its kit screens and from
// the accessors ProsperoTube's overlays already read, and lets Player draw it
// into the list EVO's kit application submits. It takes no input and changes
// nothing of the player: where the proposal's boards show something the
// player cannot do or does not know yet, the view stays empty there and
// docs/afterglow/requests/H.md says what is missing.
//
// Console only: nothing here is compiled for the host.

#include "afterglow/player/app/mount.hpp"
#include "afterglow/player/app/picker.h"

#include "afterglow/app/afterglow.h"
#include "afterglow/player/player.hpp"
#include "afterglow/text_engine.hpp"

#include "evo_hui_screens.hpp"

extern "C"
{
#include "tube_player_extra.h"
#include "tube_provider.h"
#include "tube_seek_sheets.h"
#include "tube_time_display.h"
#include "tube_ui_style.h"
#include "tube_video_destination.h"
// media/include/evo_audio_out.h, without the decoder's headers it brings along.
double evo_audio_rate(void);
}

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace afterglow
{

namespace
{

using player::Hint;
using player::PlayerView;

std::string str(const char *text)
{
    return text != nullptr ? text : "";
}

std::vector<std::string> split_lines(const std::string &text)
{
    std::vector<std::string> out;
    std::string line;
    for (const char c : text)
    {
        if (c == '\n')
        {
            out.push_back(line);
            line.clear();
        }
        else if (c != '\r')
        {
            line.push_back(c);
        }
    }
    if (!line.empty())
        out.push_back(line);
    return out;
}

bool same(const std::string &a, const char *b)
{
    return a == b;
}

// EVO writes its labels in capitals ("PLAYBACK OPTIONS", "KEEP WATCHING"); the
// design sets them as sentences. Texts that hold a lower-case letter already
// are left as they are.
std::string sentence(const std::string &text)
{
    bool any_lower = false;
    for (const char c : text)
        any_lower = any_lower || (c >= 'a' && c <= 'z');
    if (any_lower || text.empty())
        return text;
    std::string out = text;
    for (std::size_t i = 1; i < out.size(); ++i)
    {
        if (out[i] >= 'A' && out[i] <= 'Z')
            out[i] = static_cast<char>(out[i] - 'A' + 'a');
    }
    return out;
}

// Native picker identities stay in English; the mounted group labels follow the interface language.
std::string picker_label(const std::string& source) {
    const struct { const char *source, *translated; } labels[] = {
        {"Quality",tr("Quality")},
        {"Audio",tr("Audio")},
        {"Speed",tr("Speed")},
        {"Live chat",tr("Live chat")},
        {"Playback mode",tr("Playback mode")},
        {"Video size",tr("Video size")},
        {"Zoom",tr("Zoom")},
        {"Aspect ratio",tr("Aspect ratio")},
        {"Display correction",tr("Display correction")},
        {"Rotation",tr("Rotation")},
        {"Time display",tr("Time display")},
        {"Seeking",tr("Seeking")},
        {"Seek interval",tr("Seek interval")},
        {"Cross button",tr("Cross button")},
        {"Hide controls",tr("Hide controls")},
        {"Sleep timer",tr("Sleep timer")},
        {"Video presets",tr("Video presets")},
        {"Chapters",tr("Chapters")},
    };
    for (const auto& label : labels) if (source==label.source) return label.translated;
    return source;
}

// What EVO said last, with its strings copied.
struct Playback
{
    evo_playback_osd_params_t raw{};
    std::string title, meta, res, hdr, upscale, smooth, deepblack, sub_track, caption, caption2;
    bool known = false;
};

struct Details
{
    std::vector<std::string> lines;
    std::vector<player::Graph> graphs;
};

struct ListRow
{
    std::string title, detail, badge;
    bool focused = false;
    bool chevron = false;
};

struct List
{
    std::string title, empty_title, empty_hint;
    std::vector<ListRow> rows;
    int section = 0;
    int total = 0;
    int cursor = -1;
    bool empty = false;
    std::string choices_title;
    std::vector<player::Choice> choices;
};

struct Mount
{
    std::unique_ptr<ImageCache> images;
    std::unique_ptr<player::Player> player;
    const ui::Fonts *fonts = nullptr;
    PlayerView view;
    Playback playback;
    Details details;
    player::Sheet picker; // the caption picker, as EVO last described it
    List list;
    int caption_tab = 0, caption_first = 0, caption_total = 0;
    player::Dialog dialog;
    player::Toast toast;
    std::string toast_key;
    double toast_since = 0.0;
    // The presented frame each part last drew in; far in the past to begin with.
    unsigned stepped = 0x80000000u;
    unsigned controls = 0x80000000u;
    unsigned picked = 0x80000000u;
    unsigned listed = 0x80000000u;
    unsigned asked = 0x80000000u;
    unsigned toasted = 0x80000000u;
    bool text_begun = false;
    bool video_eligible = false;
    std::string snapshot;
};

Mount &mount()
{
    static Mount *m = new Mount(); // never destroyed, like EVO's kit application
    return *m;
}

double seconds_now()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
}

bool recent(unsigned stamp, unsigned frame)
{
    return frame - stamp <= 1u;
}

// The text engine behind the kit's fonts belongs to whoever runs the frame.
// While the player is up that is this mount: one begin per presented frame,
// and an end after every part, so glyphs first asked for in a part reach
// their texture before the list is submitted.
TextEngine *engine()
{
    return static_cast<TextEngine *>(gfx::Font::shaper);
}

float caption_size(int face)
{
    return face == 1 ? 40.0f : face == 3 ? 60.0f : 50.0f;
}

// "4K UHD" -> "4K", "1080p FHD" -> "1080p".
std::string first_word(const std::string &text)
{
    return text.substr(0, text.find(' '));
}

void fill_controls(Mount &m)
{
    PlayerView &v = m.view;
    const Playback &pb = m.playback;
    const evo_playback_osd_params_t &raw = pb.raw;
    const std::time_t now = std::time(nullptr);
    if (raw.chrome_hidden == 0 || !same(pb.title, "Video Playback") || v.title.empty())
        v.title = pb.title; // with its controls hidden EVO hands a stand-in title: the video is still the same one
    v.channel.clear(); // REQUEST: the playing video's channel, views and age (requests/H.md)
    v.avatar.clear();
    v.live = raw.is_live != 0;
    v.live_status.clear();
    // The player's own word about the stream, where it has one.
    v.meta = same(pb.meta, "LIVE")                        ? std::string()
             : same(pb.meta, "BUFFERING...")              ? std::string(tr("Buffering..."))
             : same(pb.meta, "WAITING FOR THE SERVER...") ? std::string(tr("Waiting for the server..."))
                                                          : pb.meta;
    v.paused = raw.paused != 0;
    v.loading = raw.loading != 0;
    v.scrubbing = raw.scrub_active != 0;
    v.position = raw.position_sec;
    v.duration = raw.duration_sec;
    v.scrub_target = raw.scrub_target;
    v.buffered = -1.0;
    v.rate = evo_audio_rate();
    const int time_mode = tube_player_time_mode(); // 0 the length, 1 the time left, 2 when it ends
    v.show_remaining = time_mode != 0;
    v.chrome = static_cast<float>(std::clamp(raw.alpha, 0, 255)) / 255.0f;
    v.chrome_hidden = raw.chrome_hidden != 0;

    // ---- the hour, what is being decoded, and the tags beside it ----
    char clock[24] = "";
    if (tube_extra_clock())
        tube_clock_text(clock, sizeof clock, now, tube_extra_clock_12h());
    v.clock = clock;
    char info[96] = "";
    v.quality.clear();
    if (tube_extra_quality_info() && tube_extra_controls_info(info, sizeof info, now))
    {
        // "MUTED   1080p60 VP9   21:47": the quality is what is left between the two.
        std::string text = info;
        if (text.rfind("MUTED", 0) == 0)
            text.erase(0, 5);
        if (clock[0] != '\0' && text.size() >= std::strlen(clock) && text.compare(text.size() - std::strlen(clock), std::string::npos, clock) == 0)
            text.erase(text.size() - std::strlen(clock));
        while (!text.empty() && text.back() == ' ')
            text.pop_back();
        while (!text.empty() && text.front() == ' ')
            text.erase(text.begin());
        v.quality = text;
    }
    v.tags.clear();
    if (v.quality.empty() && !pb.res.empty())
        v.tags.push_back(first_word(pb.res));
    if (!pb.hdr.empty())
        v.tags.push_back(same(pb.hdr, "DOLBY VISION") ? "Dolby Vision" : pb.hdr);
    if (!pb.sub_track.empty())
        v.tags.push_back(pb.sub_track.size() <= 14 ? "CC " + pb.sub_track : std::string("CC")); // which captions are on, where that is short
    if (raw.sub_delay_ms != 0)
    {
        char sync[40];
        std::snprintf(sync, sizeof sync, "%+d ms", raw.sub_delay_ms);
        v.tags.push_back(fill(tr("Captions {0}"), sync));
    }
    if (v.rate > 0.0 && (v.rate < 0.99 || v.rate > 1.01))
    {
        char speed[24];
        std::snprintf(speed, sizeof speed, "%.3g\xC3\x97", v.rate);
        v.tags.emplace_back(speed);
    }
    if (tube_extra_muted())
        v.tags.emplace_back(tr("Muted"));
    if (raw.upscale_active != 0 && !pb.upscale.empty())
        v.tags.push_back(sentence(pb.upscale));
    if (raw.smooth_active != 0 && !pb.smooth.empty())
        v.tags.push_back(sentence(pb.smooth));
    if (!pb.deepblack.empty())
        v.tags.push_back(sentence(pb.deepblack));
    if (time_mode == 2 && !v.live && v.duration > 0.0)
    {
        const double rate = v.rate >= 0.25 && v.rate <= 4.0 ? v.rate : 1.0;
        const double left = std::max(0.0, v.duration - (v.scrubbing ? v.scrub_target : v.position)) / rate;
        char ends[24] = "";
        if (tube_clock_text(ends, sizeof ends, now + static_cast<std::time_t>(left + 0.5), tube_extra_clock_12h()))
            v.tags.push_back(fill(tr("Ends {0}"), ends));
    }

    // ---- chapters and SponsorBlock's segments ----
    v.chapters.clear();
    const int chapters = v.live ? 0 : tube_extra_chapter_count();
    for (int i = 0; i < chapters; ++i)
    {
        player::Chapter chapter;
        chapter.start = tube_extra_chapter_start(i);
        chapter.title = str(tube_extra_chapter_title(i));
        v.chapters.push_back(std::move(chapter));
    }
    v.segments.clear();
    if (!v.live && v.duration > 0.0)
    {
        tube_sponsor_marker_t markers[128];
        const int count = tube_sponsor_markers(v.duration, markers, 128);
        for (int i = 0; i < count; ++i)
            v.segments.push_back({markers[i].start, markers[i].end, markers[i].rgb, markers[i].point != 0});
    }

    // ---- the marks, and the buttons the player answers to ----
    v.controls.clear();
    v.controls.emplace_back("play_pause");
    if (tube_extra_button(TUBE_BUTTON_PREVIOUS))
        v.controls.emplace_back("prev");
    if (tube_extra_button(TUBE_BUTTON_NEXT))
        v.controls.emplace_back("next");
    v.controls.emplace_back("captions");
    v.controls.emplace_back("quality");
    if (!v.live && tube_extra_button(TUBE_BUTTON_SPEED))
        v.controls.emplace_back("speed");
    if (tube_extra_button(TUBE_BUTTON_CHAT) && (v.live || tube_player_chat_preference() != 0))
        v.controls.emplace_back("chat");
    if (tube_extra_suggestions())
        v.controls.emplace_back("queue");
    v.controls.emplace_back("more");
    v.hints.clear();
    if (!v.live)
        v.hints.push_back({ui::Button::dpad, ui::Button::none, tr("Seek")});
    if (chapters > 1)
        v.hints.push_back({ui::Button::l1, ui::Button::r1, tr("Chapter")});
    v.hints.push_back({ui::Button::r2, ui::Button::none, tr("Options")});
    v.hints.push_back({ui::Button::square, ui::Button::none, tr("Details")});
    v.hints.push_back({ui::Button::circle, ui::Button::none, tr("Back")});
    v.seek_hints.clear();
    v.seek_hints.push_back({ui::Button::dpad, ui::Button::none, tr("Hold to scrub")});
    v.seek_hints.push_back({ui::Button::cross, ui::Button::none, tr("Play from here")});
    v.seek_hints.push_back({ui::Button::circle, ui::Button::none, tr("Cancel")});

    // ---- captions, in the profile's style ----
    player::Captions &c = v.captions;
    c.lines = split_lines(pb.caption);
    c.second = split_lines(pb.caption2);
    c.size = caption_size(raw.subtitle_face) * static_cast<float>(tube_extra_subtitle_scale()) / 100.0f;
    const int position = tube_extra_subtitle_position();
    const float line = c.size * 1.25f;
    c.baseline = position >= 0
                     ? static_cast<float>(tube_subtitle_baseline(position, 1080.0, static_cast<double>(std::max<std::size_t>(1, c.lines.size())) * line, line))
                     : -1.0f;
    c.raised = raw.subtitle_raised != 0;
    const tube_subtitle_style_t style = tube_extra_subtitle_style();
    c.custom = style.custom != 0;
    c.text_rgb = style.text_rgb;
    c.plate_rgb = style.background_rgb;
    c.plate_alpha = static_cast<float>(style.background_alpha) / 255.0f;
    c.edge = style.edge == TUBE_SUBTITLE_EDGE_OUTLINE ? 1 : style.edge == TUBE_SUBTITLE_EDGE_SHADOW ? 2 : 0;
    c.second_top = raw.subtitle2_position == 1;
    static const std::uint32_t kSecond[3] = {0xffe14d, 0x66e3ff, 0xffffff};
    c.second_rgb = kSecond[std::clamp(raw.subtitle2_color, 0, 2)];

    // ---- the chat column ----
    player::Chat &chat = v.chat;
    chat = player::Chat{};
    const bool plain = raw.show_stats == 0 && raw.scrub_active == 0 && raw.music_mode == 0;
    chat.side = plain ? tube_player_chat_mode() : 0;
    if (chat.side != 0)
    {
        chat.theatre = m.video_eligible;
        const std::string title = str(tube_player_chat_title());
        chat.title = same(title, "TOP CHAT") ? tr("Top chat") : same(title, "CHAT REPLAY") ? tr("Chat replay") : sentence(title);
        const tube_player_chat_row_t *rows = nullptr;
        const int count = tube_player_chat_rows(&rows);
        for (int i = 0; rows != nullptr && i < count; ++i)
            chat.lines.push_back({rows[i].author, rows[i].message, 0, {}});
        if (chat.lines.empty())
            chat.status = str(tube_player_chat_status());
    }

    v.next = player::NextVideo{};
    tube_next_offer_t next;
    if (plain && tube_next_offer(&next))
    {
        v.next.shown = true;
        v.next.watched = v.title;
        v.next.card.id = next.item.id;
        v.next.card.title = next.item.title;
        v.next.card.channel = next.item.subtitle;
        v.next.card.thumb = next.item.art_url;
        v.next.card.duration_s = static_cast<int>(next.item.duration_sec);
        v.next.seconds_left = static_cast<float>(next.left_ms) / 1000.0f;
        v.next.seconds_total = static_cast<float>(TUBE_NEXT_OFFER_MS) / 1000.0f;
        v.next.focus = next.focus;
    }

    // ---- a segment that asks, or the next video on its way ----
    player::Notice &notice = v.notice;
    notice = player::Notice{};
    const char *prompt = raw.show_stats == 0 && raw.scrub_active == 0 && raw.music_mode == 0 && raw.is_live == 0 ? tube_sponsor_prompt(raw.position_sec) : nullptr;
    if (prompt != nullptr && prompt[0] != '\0')
    {
        notice.shown = true;
        notice.kind = "ask";
        notice.label = "SponsorBlock";
        notice.title = prompt;
        notice.action = {ui::Button::cross, ui::Button::none, tr("Skip")};
        notice.second = {ui::Button::circle, ui::Button::none, tr("Keep")};
        notice.rgb = tube_sponsor_prompt_rgb(raw.position_sec);
    }
    else if (!v.next.shown && tube_playlist_eof_pending())
    {
        notice.shown = true;
        notice.kind = "next";
        notice.title = tr("The next video is on its way");
        notice.detail = tr("Any button cancels");
    }
    else if (plain && !v.live)
    {
        tube_sponsor_skip_t skipped;
        if (tube_sponsor_last_skip(&skipped))
        {
            notice.shown = true;
            notice.kind = "skip";
            notice.label = "SponsorBlock";
            notice.title = std::string(tr("Skipped")) + " " + sentence(str(skipped.category));
            notice.rgb = skipped.rgb;
            notice.action = {ui::Button::cross, ui::Button::none, tr("Undo")};
            notice.left = 1.0f - static_cast<float>(skipped.age_ms) / TUBE_SPONSOR_UNDO_MS;
        }
    }

    // ---- the playback details, the development counter, dimming ----
    v.stats.shown = raw.show_stats != 0;
    v.stats.lines = m.details.lines;
    v.stats.graphs = m.details.graphs;
    v.fps = raw.debug_overlay != 0 ? raw.fps : -1;
    v.dim = static_cast<float>(std::clamp(tube_extra_dim_percent(), 0, 100)) / 100.0f;
}

// The storyboard tiles of the seek target, as textures of EVO's cache.
void fill_preview(Mount &m, evo::kit::TextureCache &textures)
{
    PlayerView &v = m.view;
    v.preview.clear();
    if (!v.scrubbing || v.live || v.duration <= 0.0)
        return;
    tube_seek_view preview;
    if (!tube_seek_preview_frame(v.scrub_target, v.duration, 960.0, 800.0, &preview))
        return;
    for (int i = 0; i < preview.count && i < TUBE_SEEK_TILES; ++i)
    {
        const tube_seek_tile_view &tile = preview.tiles[i];
        if (tile.sheet_width <= 0 || tile.sheet_height <= 0 || tile.height <= 0)
            continue;
        player::Tile out;
        out.texture = textures.pixels(tile.pixels, tile.sheet_width, tile.sheet_height, 0x7365656bull);
        if (out.texture == 0)
            continue;
        out.uv = {static_cast<float>(tile.x) / static_cast<float>(tile.sheet_width), static_cast<float>(tile.y) / static_cast<float>(tile.sheet_height),
                  static_cast<float>(tile.width) / static_cast<float>(tile.sheet_width), static_cast<float>(tile.height) / static_cast<float>(tile.sheet_height)};
        out.aspect = static_cast<float>(tile.width) / static_cast<float>(tile.height);
        out.offset = tile.box.offset;
        v.preview.push_back(std::move(out));
    }
}

bool plain_video_id(const char *id)
{
    if (id == nullptr || std::strlen(id) != 11)
        return false;
    return std::strspn(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == 11;
}

// The player's own list: the videos that are related become the shelf, every
// other list a sheet.
void fill_list(Mount &m)
{
    PlayerView &v = m.view;
    const List &list = m.list;
    int focus = 0;
    bool videos = !list.rows.empty();
    for (std::size_t i = 0; i < list.rows.size(); ++i)
    {
        if (list.rows[i].focused)
            focus = static_cast<int>(i);
        videos = videos && list.rows[i].badge == "VIDEO";
    }
    const int first = std::max(0, list.cursor - focus);
    v.up_next = player::UpNext{};
    v.sheet = player::Sheet{};
    if (videos)
    {
        player::UpNext &shelf = v.up_next;
        shelf.open = true;
        shelf.title = sentence(list.title);
        shelf.chip = tube_autoplay_enabled() ? tr("Autoplay is on") : tr("Autoplay is off");
        shelf.marks_next = tube_autoplay_enabled() != 0;
        shelf.first = first;
        shelf.total = list.total;
        shelf.focus = focus;
        const bool rows_known = tube_player_menu_status() == 1;
        for (std::size_t i = 0; i < list.rows.size(); ++i)
        {
            player::Card card;
            card.title = list.rows[i].title;
            card.channel = list.rows[i].detail;
            // The picture of a video is found from its id; the row of the menu behind the list names it.
            const int index = first + static_cast<int>(i);
            const tube_player_menu_row_t *row = rows_known && index < tube_player_menu_count() ? tube_player_menu_row(index) : nullptr;
            if (row != nullptr && card.title == row->title && plain_video_id(row->id))
            {
                card.id = row->id;
                card.thumb = std::string("https://i.ytimg.com/vi/") + row->id + "/mqdefault.jpg";
            }
            shelf.cards.push_back(std::move(card));
        }
        shelf.hints = {{ui::Button::cross, ui::Button::none, tr("Play")}, {ui::Button::circle, ui::Button::none, tr("Back")}};
        return;
    }
    player::Sheet &sheet = v.sheet;
    sheet.open = true;
    sheet.kind = "options";
    sheet.title = same(list.title, "PLAYBACK OPTIONS") ? tr("Playback") : picker_label(sentence(list.title));
    sheet.first = first;
    sheet.total = list.total;
    sheet.choices_title = picker_label(list.choices_title);
    sheet.choices_total = static_cast<int>(list.choices.size());
    int current = 0;
    for (int i=0; i<sheet.choices_total; ++i) if (list.choices[i].current) current=i;
    sheet.choices_first = std::clamp(current-4,0,std::max(0,sheet.choices_total-9));
    for (int i=sheet.choices_first; i<sheet.choices_total && i<sheet.choices_first+9; ++i)
        sheet.choices.push_back(list.choices[i]);
    for (const ListRow &from : list.rows)
    {
        player::Row row;
        // "Speed: 1.25x", "Time display: remaining": the name, and its value at the right.
        const std::size_t colon = from.title.find(": ");
        if (colon != std::string::npos && colon > 0 && from.title.size() - colon <= 42)
        {
            row.label = from.title.substr(0, colon);
            row.value = from.title.substr(colon + 2);
        }
        else
        {
            row.label = sentence(from.title);
        }
        row.detail = from.detail;
        if (row.label == "Keep playing while browsing") row.label = tr("Keep playing while browsing");
        if (row.detail == "L3: return to video") row.detail = std::string("L3: ") + tr("return to video");
        row.current = from.badge == "SELECTED" || from.badge == "PLAYING";
        if (!row.current && from.badge != "VIDEO")
            row.tag = sentence(from.badge);
        row.focused = from.focused;
        row.chevron = from.chevron;
        if (row.chevron) row.label = picker_label(row.label);
        sheet.rows.push_back(std::move(row));
    }
    if (list.empty || sheet.rows.empty())
    {
        sheet.empty_title = list.empty_title;
        sheet.empty_hint = list.empty_hint;
    }
    sheet.hints = {{ui::Button::cross, ui::Button::none, tr("Choose")}, {ui::Button::circle, ui::Button::none, tr("Back")}};
}

// EVO's toast, said from the notice's place while the player is up.
void fill_toast_notice(Mount &m)
{
    player::Notice &notice = m.view.notice;
    notice = player::Notice{};
    notice.shown = true;
    notice.kind = m.toast.kind == 2 ? "error" : "info";
    notice.label = sentence(m.toast.title);
    notice.title = m.toast.message;
    if (notice.title.empty())
    {
        notice.title = notice.label;
        notice.label.clear();
    }
    // EVO keeps a toast for 2.2 seconds and then fades it (src/evo_toast.c).
    notice.left = static_cast<float>(std::clamp(1.0 - (seconds_now() - m.toast_since) / 2.2, 0.0, 1.0));
}

// One step of the player per presented frame, at the first part that draws in it.
player::Player &step(Mount &m, const ui::Fonts &fonts, float dt, unsigned frame, bool own_text)
{
    if (!m.player || m.fonts != &fonts)
    {
        m.fonts = &fonts;
        // Pictures (a related video's, a channel's) come through the frame's platform, where it has run.
        m.images.reset();
        if (platform() != nullptr)
            m.images = std::make_unique<ImageCache>(*platform());
        m.player = std::make_unique<player::Player>(fonts, m.images.get());
        m.player->prepare_video(0,0); // no earlier quad could have used a newly-created layout
    }
    if (m.stepped == frame)
        return *m.player;
    m.stepped = frame;
    m.text_begun = false;
    if (own_text && engine() != nullptr)
    {
        engine()->begin_frame();
        m.text_begun = true;
    }
    const TubeUiPrefs *prefs = tube_ui_prefs();
    m.player->set_look(look_for_scheme(prefs != nullptr ? prefs->scheme : 0));

    PlayerView &v = m.view;
    const bool controls = recent(m.controls, frame);
    const bool toast = m.toast.shown && recent(m.toasted, frame);
    if (!controls)
    {
        // A picker or the dialog has the screen: the player hands nothing new, and nothing stale is shown.
        v.chrome = 0.0f;
        v.dim = 0.0f;
        v.captions.lines.clear();
        v.captions.second.clear();
        v.chat.side = 0;
        v.notice.shown = false;
        v.next.shown = false;
        v.stats.shown = false;
        v.preview.clear();
        v.scrubbing = false;
    }
    v.sheet.open = false;
    v.up_next.open = false;
    if (recent(m.listed, frame))
        fill_list(m);
    else if (recent(m.picked, frame))
        v.sheet = m.picker;
    const bool in_player = controls || recent(m.listed, frame) || recent(m.picked, frame);
    if (toast && in_player)
        fill_toast_notice(m);
    v.toast = toast && !in_player ? m.toast : player::Toast{};
    v.dialog = recent(m.asked, frame) ? m.dialog : player::Dialog{};
    if (m.images)
        m.images->pump(dt);
    m.player->update(v, dt);
    return *m.player;
}

// After a part has laid its text out: what the engine rasterised reaches its texture.
void finish(Mount &m)
{
    if (m.text_begun && engine() != nullptr)
        engine()->end_frame();
}

} // namespace

void player_set_playback(const evo_playback_osd_params_t &params)
{
    Playback &pb = mount().playback;
    pb.raw = params;
    pb.title = str(params.title);
    pb.meta = str(params.metadata);
    pb.res = str(params.res_badge);
    pb.hdr = str(params.hdr_badge);
    pb.upscale = str(params.upscale_badge);
    pb.smooth = str(params.smooth_badge);
    pb.deepblack = str(params.deepblack_badge);
    pb.sub_track = str(params.sub_track);
    pb.caption = str(params.subtitle_text);
    pb.caption2 = str(params.subtitle_text2);
    // The copied pointers are never read.
    pb.raw.title = pb.raw.metadata = pb.raw.res_badge = pb.raw.hdr_badge = pb.raw.codec_badge = pb.raw.fps_badge = nullptr;
    pb.raw.audio_badge = pb.raw.decoder_badge = pb.raw.upscale_badge = pb.raw.smooth_badge = pb.raw.deepblack_badge = nullptr;
    pb.raw.audio_track = pb.raw.sub_track = pb.raw.subtitle_text = pb.raw.subtitle_text2 = pb.raw.music_codec = nullptr;
    pb.known = true;
}

void player_set_details(const evo_perf_hud_t &hud)
{
    Details &details = mount().details;
    details.lines.clear();
    for (const char *line : {hud.line_video, hud.line_audio, hud.line_subs, hud.line_perf, hud.line_queues, hud.line_clocks})
    {
        if (line != nullptr && line[0] != '\0')
            details.lines.emplace_back(line);
    }
    details.graphs.clear();
    const auto graph = [&](const char *label, const float *samples, const char *format, double value)
    {
        player::Graph out;
        out.label = label;
        char text[32];
        std::snprintf(text, sizeof text, format, value);
        out.value = text;
        for (int i = 0; samples != nullptr && i < hud.hist_len; ++i)
            out.samples.push_back(samples[i]);
        details.graphs.push_back(std::move(out));
    };
    graph("GPU", hud.gpu_hist, "%.0f%%", static_cast<double>(hud.gpu_pct));
    graph("RAM", hud.ram_hist, "%.0f MB", static_cast<double>(hud.ram_mb));
    graph("CPU", hud.cpu_hist, "%.0f%%", static_cast<double>(hud.cpu_pct));
}

void player_set_captions_picker(const evo_rmlui_subtitles_params_t &params)
{
    player::Sheet &sheet = mount().picker;
    sheet = player::Sheet{};
    sheet.open = true;
    sheet.kind = "captions";
    sheet.title = tr("Captions");
    sheet.tabs = {tr("Captions"),tr("Second language")};
    sheet.tab = mount().caption_tab;
    sheet.first = mount().caption_first;
    sheet.total = mount().caption_total;
    if (params.size_str != nullptr && params.size_str[0] != '\0')
        sheet.pills.push_back(fill(tr("Size: {0}"), sentence(params.size_str)));
    const char *sync = sheet.tab == 1 ? params.sync2_str : params.sync_str;
    if (sync != nullptr && sync[0] != '\0') sheet.pills.push_back(sentence(sync));
    for (int i = 0; i < params.track_count && i < 8; ++i)
    {
        const evo_rmlui_subtitles_track_t &track = params.tracks[i];
        player::Row row;
        row.label = str(track.label);
        row.detail = str(track.detail);
        const std::string tag = str(track.tag);
        if (tag != "PRIMARY" && tag != "SECONDARY") row.tag = sentence(tag);
        row.current = track.is_current != 0;
        row.focused = track.is_focused != 0;
        row.action = track.is_action != 0;
        row.disabled = track.is_disabled != 0;
        sheet.rows.push_back(std::move(row));
    }
    sheet.preview = str(params.preview_text);
    sheet.preview_size = (params.preview_face == 0 ? 40.0f : params.preview_face == 2 ? 60.0f : 50.0f) * static_cast<float>(tube_extra_subtitle_scale()) / 100.0f;
    sheet.hints = {{ui::Button::cross, ui::Button::none, tr("Choose")},
                   {ui::Button::circle, ui::Button::none, tr("Back")},
                   {ui::Button::l1, ui::Button::r1, tr("Language")},
                   {ui::Button::square, ui::Button::none, tr("Size")},
                   {ui::Button::dpad, ui::Button::none, tr("Sync")}};
}

void player_set_list(const evo_rmlui_list_params_t &params)
{
    List &list = mount().list;
    list.title = str(params.title);
    list.section = params.section;
    list.total = params.total_count;
    list.cursor = params.cursor_index;
    list.empty = params.is_empty != 0;
    list.empty_title = str(params.empty_title);
    list.empty_hint = str(params.empty_hint);
    list.rows.clear();
    for (int i = 0; i < params.row_count && i < EVO_RMLUI_LIST_ROWS; ++i)
        list.rows.push_back({str(params.rows[i].title), str(params.rows[i].detail), str(params.rows[i].badge), params.rows[i].is_focused != 0, params.rows[i].has_chevron != 0});
}

void player_set_dialog(const evo_rmlui_dialog_params_t &params)
{
    Mount &m = mount();
    player::Dialog &dialog = m.dialog;
    dialog = player::Dialog{};
    dialog.shown = true;
    dialog.kind = "question";
    dialog.title = str(params.title);
    dialog.body = str(params.detail);
    dialog.focus = std::clamp(params.focused_action, 0, std::max(0, params.action_count - 1));
    dialog.progress = params.progress_pct >= 0.0 ? static_cast<float>(params.progress_pct > 1.0 ? params.progress_pct / 100.0 : params.progress_pct) : -1.0f;
    for (int i = 0; i < params.action_count && i < 3; ++i)
        dialog.buttons.push_back({sentence(str(params.actions[i].label)), i == 0, false});
    // The player's own two questions, in the words of the design.
    if (same(dialog.title, "Stop playback?"))
    {
        const Playback &pb = m.playback;
        dialog.title = tr("Stop watching?");
        dialog.body = pb.known && pb.raw.is_live == 0 && pb.raw.duration_sec > 0.0
                          ? fill(tr("Your place is kept: {0} of {1}. The video is waiting under Continue watching."), player::clock_text(pb.raw.position_sec),
                                 player::clock_text(pb.raw.duration_sec))
                          : std::string();
        if (dialog.buttons.size() == 2)
        {
            dialog.buttons[0].label = tr("Keep watching");
            dialog.buttons[1].label = tr("Stop");
        }
    }
    else if (same(dialog.title, "Continue from previous position?"))
    {
        dialog.title = tr("Continue where you stopped?");
        dialog.body = sentence(dialog.body);
        if (dialog.buttons.size() == 2)
        {
            dialog.buttons[0].label = tr("Resume");
            dialog.buttons[1].label = tr("Start over");
        }
    }
}

void player_set_toast(const evo_rmlui_toast_params_t &params)
{
    Mount &m = mount();
    m.toast.shown = params.visible != 0 && params.alpha >= 250; // on show; EVO's own fade is the notice's leaving
    if (!m.toast.shown)
    {
        m.toast_key.clear();
        return;
    }
    m.toast.title = str(params.title);
    m.toast.message = str(params.message);
    m.toast.kind = params.kind;
    const std::string key = m.toast.title + '\n' + m.toast.message;
    if (key != m.toast_key)
    {
        m.toast_key = key;
        m.toast_since = seconds_now();
    }
}

bool player_list_is_sheet()
{
    // The player's picker is the one list that is not a destination of EVO's rail.
    return mount().list.section == -1;
}

bool player_draw_controls(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, evo::kit::TextureCache &textures, float dt, unsigned frame)
{
    Mount &m = mount();
    if (!m.playback.known || m.playback.raw.music_mode != 0)
        return false; // sound alone keeps EVO's own page
    m.controls = frame;
    fill_controls(m);
    fill_preview(m, textures);
    player::Player &player = step(m, fonts, dt, frame, true);
    // A toast draws after the controls: while EVO draws one, the notice is that part's to draw.
    const bool toast = recent(m.toasted, frame);
    player.draw(list, list, 0, player::Player::kControls | (toast ? 0u : static_cast<unsigned>(player::Player::kNotice)));
    finish(m);
    return true;
}

bool player_draw_captions_picker(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame)
{
    Mount &m = mount();
    m.picked = frame;
    player::Player &player = step(m, fonts, dt, frame, true);
    player.draw(list, list, 0, player::Player::kControls);
    finish(m);
    return true;
}

bool player_draw_list(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame)
{
    Mount &m = mount();
    m.listed = frame;
    player::Player &player = step(m, fonts, dt, frame, true);
    player.draw(list, list, 0, player::Player::kControls);
    finish(m);
    return true;
}

bool player_draw_dialog(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame, bool page)
{
    Mount &m = mount();
    if (!m.dialog.shown)
        return false;
    m.asked = frame;
    player::Player &player = step(m, fonts, dt, frame, !page);
    player.draw(list, list, 0, player::Player::kDialog);
    finish(m);
    return true;
}

bool player_draw_toast(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame, bool page)
{
    Mount &m = mount();
    // A video that did not start: the interface has said why in a toast of its own, over the page it
    // returned to (Frame, Source::play_state). EVO's notice of the same failure is not drawn beside it.
    if (page && tube_afterglow_play_failed_recently())
        return true;
    m.toasted = frame;
    player::Player &player = step(m, fonts, dt, frame, !page);
    const bool in_player = recent(m.controls, frame) || recent(m.listed, frame) || recent(m.picked, frame);
    player.draw(list, list, 0, in_player ? player::Player::kNotice : player::Player::kToast);
    finish(m);
    return true;
}

const char *player_snapshot(unsigned frame)
{
    Mount &m = mount();
    if (!m.player || !(recent(m.controls, frame) || recent(m.listed, frame) || recent(m.picked, frame)))
        return "";
    m.snapshot = m.player->snapshot_json();
    return m.snapshot.c_str();
}

} // namespace afterglow

extern "C" void tube_afterglow_options_begin(const char *title) {
    auto &list = afterglow::mount().list;
    list.choices_title = title ? title : "";
    list.choices.clear();
}
extern "C" void tube_afterglow_options_choice(const char *label, int current) {
    afterglow::player::Choice choice;
    choice.label = label ? label : ""; choice.current = current != 0;
    afterglow::mount().list.choices.push_back(std::move(choice));
}
extern "C" void tube_afterglow_caption_tab(int tab, int first, int total) {
    auto &mount = afterglow::mount();
    mount.caption_tab = tab == 1 ? 1 : 0;
    mount.caption_first = std::max(0,first); mount.caption_total = std::max(0,total);
}

extern "C" void tube_afterglow_video_prepare(int eligible,float dt) {
    auto &mount=afterglow::mount();
    tube_video_destination_reset();
    mount.video_eligible = eligible && tube_afterglow_enabled();
    if (!mount.player) return;
    const int side=mount.video_eligible?tube_player_chat_mode():0;
    mount.player->prepare_video(side,dt);
    if (side) {
        const auto frame=mount.player->video_rect();
        tube_video_destination_set(frame.x,frame.y,frame.w,frame.h);
    }
}
