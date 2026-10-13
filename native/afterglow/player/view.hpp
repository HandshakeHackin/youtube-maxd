// ProsperoTube Afterglow - the player's view: everything its controls draw, as plain values.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player stays EVO's PlayerScreen with ProsperoTube's overlays: it owns
// playback, the pad and every menu. This is what it shows, written down once.
// In the app, app/mount.cpp fills a PlayerView every frame from what EVO hands
// its kit screens (evo_playback_osd_params_t, the picker's rows, the dialog,
// the toast) and from the accessors the overlays already use; in the host
// harness a fixture file is one (view_json.cpp). Player (player.hpp) draws it
// and owns nothing but motion.
//
// Where each member comes from in the app is said beside it; "fixture" marks
// what the app cannot fill yet (docs/afterglow/requests/H.md says what is
// missing for each).

#pragma once

#include "afterglow/base.hpp"

#include <string>
#include <vector>

namespace afterglow::player
{

// One entry of a hint row. In the app these are the buttons the player really
// answers to, never the proposal's map where the two differ.
struct Hint
{
    ui::Button button = ui::Button::none;
    ui::Button second = ui::Button::none; // pairs such as L1 / R1
    std::string label;
};

// tube_extra_chapter_start(), tube_extra_chapter_title() or its label.
struct Chapter
{
    double start = 0.0; // seconds
    std::string title;
};

// tube_sponsor_markers(): a SponsorBlock segment, in the category's own colour.
struct Segment
{
    double from = 0.0; // share of the video, 0..1
    double to = 0.0;
    std::uint32_t rgb = 0;
    bool point = false; // a highlight: a mark, not a stretch
};

// tube_seek_preview_frame(): one storyboard tile for the seek target.
struct Tile
{
    std::uint32_t texture = 0; // the sheet it is cut from
    Rect uv{0.0f, 0.0f, 1.0f, 1.0f};
    float aspect = 16.0f / 9.0f;
    int offset = 0;  // 0 the target's, -1 and 1 its neighbours, -2 and 2 theirs
    std::string art; // fixture: a picture address instead of a texture
};

// The caption on the picture: evo_playback_osd_params_t (subtitle_text,
// subtitle_text2, subtitle_face, subtitle2_*), tube_extra_subtitle_style(),
// _scale() and _position().
struct Captions
{
    std::vector<std::string> lines;
    std::vector<std::string> second; // the second language
    float size = 50.0f;              // of the first language, virtual pixels
    float baseline = -1.0f;          // of the last line, from tube_subtitle_baseline(); negative = the player's own place
    bool raised = true;              // lifted above the timeline while the controls show
    bool custom = false;             // a caption style of the profile
    std::uint32_t text_rgb = 0xffffff;
    std::uint32_t plate_rgb = 0x000000;
    float plate_alpha = 0.55f;
    int edge = 0;            // 0 none, 1 outline, 2 shadow
    bool second_top = false; // the second language alone at the top of the picture
    std::uint32_t second_rgb = 0xffe14d;
};

// tube_player_chat_rows(): author and message. badge and avatar are fixture.
struct ChatLine
{
    std::string author;
    std::string text;
    int badge = 0; // 0 none, 1 member, 2 moderator, 3 the channel itself
    std::string avatar;
};

struct Chat
{
    int side = 0;         // tube_player_chat_mode(): 0 off, 1 left, 2 right
    bool theatre = false; // the video steps aside for the chat (fixture: needs the video's rectangle)
    std::string title;    // tube_player_chat_title()
    std::string filter;   // fixture: "Top chat"
    std::string status;   // tube_player_chat_status(), shown while there are no lines
    std::vector<ChatLine> lines; // oldest first
    std::string compose;  // fixture: the prompt of the field to write in, "" for none
    std::vector<Hint> hints;
};

// A row of a sheet: a caption track, an option, a related video without a picture.
struct Row
{
    std::string label;
    std::string detail; // what it does; shown beside the list for the focused row
    std::string value;  // the current choice, at the right ("Auto", "1x")
    std::string tag;    // a small tag ("Translated", "5.1", "12")
    std::string icon;   // a name of design.hpp's Icon, "" for none
    bool current = false;  // the track playing, the choice in effect: keeps a check mark
    bool focused = false;
    bool action = false;   // does something instead of being chosen
    bool disabled = false;
    bool chevron = false;  // opens a further list
};

// One value of the focused option, in the column beside the list (fixture).
struct Choice
{
    std::string label;
    std::string note;
    bool current = false;
};

// A sheet from the right over the video: the caption picker
// (evo_rmlui_subtitles_params_t) and the playback options and their lists
// (evo_rmlui_list_params_t of the player's own picker).
struct Sheet
{
    bool open = false;
    std::string kind = "options"; // "captions", "options"
    std::string title;
    std::vector<std::string> tabs; // fixture: "Captions", "Second language"
    int tab = 0;
    std::vector<std::string> pills; // size and sync of the captions
    std::vector<Row> rows;          // the rows on show
    int first = 0;                  // rows[0] is this row of the whole list
    int total = 0;                  // rows of the whole list; 0 = rows.size()
    std::string choices_title;      // fixture
    std::vector<Choice> choices;    // fixture
    std::string note;               // a line of help under the list
    std::string preview;            // captions: a line in the size that is chosen
    float preview_size = 50.0f;
    std::string empty_title; // a list with nothing in it says why
    std::string empty_hint;
    std::vector<Hint> hints;
};

// A related video as a card: tube_player_menu_row() of the suggestions list.
struct Card
{
    std::string id;
    std::string title;
    std::string channel;
    std::string meta;  // "2.2M views · 8 months ago"
    std::string thumb; // picture address, "" for none
    int duration_s = 0;
    unsigned badges = 0; // model::Badge bits
};

// The shelf that rises over the foot of the video.
struct UpNext
{
    bool open = false;
    std::string title;
    std::string chip;    // "Autoplay is on": tube_autoplay_enabled()
    bool marks_next = true; // the first video of the list is the one that plays next
    std::string note;    // where the list comes from
    std::string message; // while there are no cards: loading, or why there are none
    bool loading = false;
    std::vector<Card> cards;
    int first = 0; // cards[0] is this card of the whole list
    int total = 0;
    int focus = 0; // index into cards
    std::vector<Hint> hints;
};

// A notice above the timeline: a segment that was skipped or asks to be
// (tube_sponsor_prompt()), the next video on its way, and every message EVO
// shows as a toast while the player is up (paid promotion, chapter, speed).
struct Notice
{
    bool shown = false;
    std::string kind = "info"; // "skip", "ask", "next", "info", "error"
    std::string label;         // a small line above the title ("SponsorBlock")
    std::string title;
    std::string detail;
    std::uint32_t rgb = 0; // the segment's colour; 0 = the look's accent
    Hint action;           // "Undo", "Skip": drawn at the right
    Hint second;           // "Keep"
    float left = -1.0f;    // share of its time that is left, 0..1; negative = no line
};

// The end of a video: the next one offers itself (fixture: the provider does
// not say which video is next, nor how long the offer stands).
struct NextVideo
{
    bool shown = false;
    std::string watched; // the title that just ended
    Card card;
    float seconds_left = 0.0f;
    float seconds_total = 5.0f;
    int focus = 0; // 0 play now, 1 cancel
};

struct DialogButton
{
    std::string label;
    bool primary = false;
    bool destructive = false;
};

// evo_rmlui_dialog_params_t: the stop confirmation, the resume question.
struct Dialog
{
    bool shown = false;
    std::string kind = "question"; // "question", "info", "warning", "danger", "success"
    std::string title;
    std::string body;
    std::vector<DialogButton> buttons;
    int focus = 0;
    float progress = -1.0f; // 0..1 under the text, negative for none
};

// evo_rmlui_toast_params_t, outside the player (inside it a toast is a Notice).
struct Toast
{
    bool shown = false;
    std::string title;
    std::string message;
    int kind = 0; // 0 info, 1 technical, 2 error, 3 success
};

// evo_perf_hud_t: the playback details.
struct Graph
{
    std::string label;
    std::string value;
    std::vector<float> samples; // 0..1, oldest first
};

struct Stats
{
    bool shown = false;
    std::vector<std::string> lines;
    std::vector<Graph> graphs;
};

struct PlayerView
{
    // ---- what is playing ----
    std::string title;      // evo_playback_osd_params_t::title
    std::string channel;    // fixture
    std::string channel_id; // fixture
    std::string avatar;     // fixture: the channel's picture address
    std::string meta;       // views and age (fixture); in the app the player's own word ("Buffering...")
    std::vector<std::string> tags; // "4K", "HDR", "CC", "Ends 22:04": the badges and the time preference
    std::string quality;    // "2160p60 VP9": tube_extra_controls_info() without its clock
    std::string clock;      // the hour, "" when the profile hides it
    bool live = false;
    std::string live_status; // fixture: "2,184 watching · started 1 h 42 min ago"

    // ---- where it is ----
    bool paused = false;
    bool loading = false;   // the first frames, or a seek, are on their way
    bool scrubbing = false; // a seek target is being chosen
    double position = 0.0;  // seconds
    double duration = 0.0;
    double scrub_target = 0.0;
    double buffered = -1.0; // seconds read ahead; negative = unknown
    double rate = 1.0;      // playback speed: the time left is divided by it
    bool show_remaining = true; // tube_player_time_mode(): the time left beside the length

    // ---- the controls ----
    float chrome = 1.0f;        // how much of the controls EVO shows, 0..1 (its own fade)
    bool chrome_hidden = false; // only captions, notices and dimming are drawn
    std::vector<Chapter> chapters;
    std::vector<Segment> segments;
    std::vector<std::string> controls; // the row of marks, by icon name; the first is what Cross does
    std::vector<Hint> hints;
    std::vector<Hint> seek_hints;
    std::vector<Tile> preview; // while scrubbing: the target's tile first

    // ---- what is over the picture ----
    Captions captions;
    Chat chat;
    Sheet sheet;
    UpNext up_next;
    Notice notice;
    NextVideo next;
    Dialog dialog;
    Toast toast;
    Stats stats;
    int fps = -1;     // the development counter; negative hides it
    float dim = 0.0f; // black over everything, 0..1: tube_extra_dim_percent()

    // ---- how it is drawn ----
    bool reduced_motion = false;
    std::string frame; // fixture: the painted picture that stands in for the video
};

// Reads a view from a JSON object (a fixture file, a step of a board). Members
// the object does not name keep their values, and so do the members of an
// object inside it; a list is replaced whole. False, with *error saying why,
// for text that is not an object.
bool read_view(std::string_view json, PlayerView *view, std::string *error);

// "cross", "circle", "square", "triangle", "l1", "r1", "l2", "r2", "options",
// "dpad", "touchpad", "left_stick", "right_stick"; none for anything else.
ui::Button button_by_name(std::string_view name);
const char *button_name(ui::Button button);

} // namespace afterglow::player
