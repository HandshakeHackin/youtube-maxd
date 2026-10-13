// ProsperoTube Afterglow - the player's measures and the small pieces its two drawing files share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Private to native/afterglow/player. The numbers are the approved boards'
// (prosperotube_player.cpp of the proposal): the player keeps a margin of 96
// on both sides, and the title, the timeline, the marks and the hints share
// those two edges.

#pragma once

#include "afterglow/player/player.hpp"

namespace afterglow::player
{

constexpr Rect kFull{0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight};
constexpr Rect kTheatre{56.0f, 164.0f, 1312.0f, 738.0f}; // the video beside the live chat
constexpr float kEdge = 96.0f;                            // the player's left margin; kRight is its right one
constexpr float kBarWidth = kRight - kEdge;
constexpr float kBarY = 888.0f;
constexpr float kMarksY = 966.0f;  // the row of marks under the timeline
constexpr float kMark = 58.0f;
constexpr float kFootHintY = 995.0f;
constexpr Rect kCaptionSheet{1316.0f, -40.0f, 700.0f, 1160.0f};
constexpr Rect kOptionSheet{976.0f, -40.0f, 1040.0f, 1160.0f};
constexpr Rect kUpNextSheet{-40.0f, 640.0f, 2000.0f, 520.0f};
constexpr Rect kChatColumn{1408.0f, 48.0f, 464.0f, 984.0f};
constexpr Rect kNextCard{kRight - 720.0f, 560.0f, 720.0f, 400.0f};
constexpr float kNoticeY = 712.0f;
constexpr float kNoticeH = 92.0f;
constexpr float kUpNextCardW = 324.0f;
constexpr float kUpNextCardH = 182.0f;
constexpr float kUpNextPitch = 348.0f;
constexpr int kUpNextShown = 5;

// REQUEST: the sizes of design-notes section 3 that theme.hpp does not name
// yet (docs/afterglow/requests/H.md). They are the boards' own.
constexpr float kTypePlayerTitle = 38.0f; // the title over the video
constexpr float kTypePanelTitle = 34.0f;  // "Up next", "Captions", "Playback"
constexpr float kTypeChapter = 27.0f;     // the chapter's name over the timeline
constexpr float kTypeRow = 25.0f;         // rows of a sheet, a notice's first line
constexpr float kTypeClock = 23.0f;       // times of the video, in the mono face
constexpr float kTypeHint = 22.0f;        // channel line, hint labels
constexpr float kTypeNote = 21.0f;        // chat text, notes, card titles of the shelf
constexpr float kTypeSmall = 19.0f;       // the quietest line

// REQUEST: looks have no roles for what is drawn on the video itself, which is
// dark under every look: its ink is white, its shade black, and the lit mark
// carries the ink design.cpp puts on a white tag.
inline const Color kOnVideo = kWhite;
inline const Color kOnLit = Color::rgb(0x0b0c12);

// How far each part has arrived this frame.
struct Player::Motion
{
    float hud = 0.0f;     // the controls over the whole picture
    float theatre = 0.0f; // the video has stepped aside for the chat
    float chat = 0.0f;
    float sheet = 0.0f;
    float up_next = 0.0f;
    float next = 0.0f;
    float scrub = 0.0f;
    float foot = 0.0f;   // timeline, marks and hints: the shelf pushes them out
    float rise = 0.0f;   // pixels the foot still has to travel
    float notice = 0.0f; // 0..1 shown
    float notice_slide = 0.0f;
    float dialog = 0.0f;
};

// A hint row from the view's hints, cut from the end until it fits max_width.
// on_video draws it for the picture, otherwise for the look's panels. Returns
// the width drawn.
float hint_row(G &g, const std::vector<Hint> &hints, float x, float cy, bool right_align, bool on_video,
               float max_width, float size = 34.0f, float text_size = kTypeHint, float gap = 36.0f);
// A tracked line (an overline's capitals) cut to width, ending in an ellipsis
// when something was cut: fit() measures without the tracking.
std::string fit_tracked(const Face &face, std::string_view value, float size, float width, float tracking);
// A model::Video of a card, for design.hpp's video_card().
model::Video video_of(const Card &card);
// The mark of a live stream: the accent, a dot with a breathing halo, the word. Returns its width.
float live_tag(G &g, float x, float cy);

} // namespace afterglow::player
