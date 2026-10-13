// ProsperoTube Afterglow - Settings: what the parts of the screen share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settings is twelve tiles and, behind each, a page of rows. Nothing about a
// preference is written here: every row comes from the provider's catalog
// "settings" and the catalogs under it, and says what it is through its facts
// (docs/afterglow/requests/e.md lists them row by row). What this screen adds
// is where a row is shown: the table in settings_model.cpp sorts the rows into
// the tiles and their headings by the start of their ids, and a row it does
// not know still gets a place, so a new preference never disappears.
//
//   settings.cpp        the screen: reading, focus, changes on their way, input
//   settings_model.cpp  tiles, the grouping table, a row as a form row
//   settings_draw.cpp   tiles, form rows, bones, the previews

#pragma once

#include "afterglow/screen.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace afterglow::settings
{

inline constexpr const char *kMain = "settings"; // the list every tile is cut from
constexpr char kPathSeparator = '\x1f';          // between the page names in Route::title

// What a row is, from its fact "kind".
enum class Kind : std::uint8_t
{
    toggle, // "switch": on or off
    list,   // "list": one of the rows of the catalog that has its id
    slider, // "slider": such a list whose rows are the steps of one quantity
    cycle,  // "cycle": choosing it takes the next value; there is no list
    choice, // "choice": one row of a list; the current one is selected
    action, // "action": does something
    text,   // "text": a typed value
    page,   // "page": a further page of rows
    link,   // "link": leaves for another screen
    info,   // "info": says something
    order,  // "order": a row of a list that can be arranged
    reset,  // "reset": puts its page back to the defaults; it is Triangle, not a row
};

// One row of a page.
struct Item
{
    model::Row row;      // as the provider listed it: the previews read its further facts
    std::string catalog; // the list it arrived in
    std::string label;
    std::string value;   // the confirmed value, as text
    std::string detail;  // the sentence that explains it
    std::string section; // the heading it stands under, "" for none
    Kind kind = Kind::page;
    bool on = false;     // toggle: on; choice: selected; order: shown
    bool danger = false;
    bool attention = false;
    bool disabled = false;
    int index = -1; // list and slider: the place of the current choice ...
    int count = 0;  // ... among this many
    int rank = 0;   // its place on its page
};

struct Tile
{
    const char *key;  // "captions": the page is Route::argument "tile:captions", the focus "tile/captions"
    const char *name; // English; shown through tr()
    Icon icon;
    ScreenId link;          // the screen the tile leads to; ScreenId::settings for a page of its own
    const char *link_label; // English: the row that leads there when the tile has rows as well
    const char *reads[2];   // catalogs whose rows the tile's page lists beside its rows of the main list
    const char *summary[3]; // rows of the main list whose values make the line under the name
    const char *fact;       // the fact of those rows the line is made of
    const char *attention;  // the row of the main list whose "attention" lights the tile's dot
};

// The twelve tiles, then "more": the tile of rows the table does not know.
std::span<const Tile> tiles();
int tile_index(std::string_view key);
// True for a row of the main list that only stands for a catalog some tile reads.
bool is_container(std::string_view id);

// Where a row belongs.
struct Place
{
    int tile = -1;            // -1: no rule knows the row
    const char *section = ""; // English; shown through tr()
    int rank = 1 << 20;
};
Place place_of(std::string_view catalog, std::string_view id);
// The tile unknown rows of a catalog fall to.
int fallback_tile(std::string_view catalog);

Item item_of(const model::Row &row, std::string_view catalog);
// The line under a tile's name; "" when the main list says nothing for it.
std::string tile_summary(const Tile &tile, const std::vector<model::Row> &main);
bool tile_attention(const Tile &tile, const std::vector<model::Row> &main);
// True when the tile has rows to show (a tile that is only a link has none).
bool tile_has_page(const Tile &tile, const std::vector<model::Row> &main);
// The row of the main list that puts a tile back to its defaults, or null.
const model::Row *tile_reset(const Tile &tile, const std::vector<model::Row> &main);

// A number written in a fact; fallback when there is none.
float number_of(const std::string &text, float fallback);
// "ffe9a8" as a colour; fallback when it is no colour.
Color colour_of(const std::string &hex, Color fallback);

// ---- geometry -------------------------------------------------------------------

constexpr float kTileW = 394.0f;
constexpr float kTileH = 200.0f;
constexpr float kTilePitchX = 418.0f;
constexpr float kTilePitchY = 224.0f;
constexpr float kTilesTop = 216.0f;
constexpr int kTileColumns = 4;

constexpr float kFormX = 150.0f;   // a row's plate; its text starts at kLeft
constexpr float kFormW = 880.0f;
constexpr float kFormTop = 196.0f;
constexpr float kFormBottom = 966.0f;
constexpr float kRowH = 70.0f;
constexpr float kHeadingH = 58.0f;
constexpr float kRowGap = 6.0f;
constexpr float kRowPad = 26.0f;
constexpr float kControlW = 300.0f;
constexpr Rect kPreviewBox{1080.0f, 232.0f, 744.0f, 418.0f};

inline Rect tile_rect(int index, float scroll = 0.0f)
{
    return {kLeft + static_cast<float>(index % kTileColumns) * kTilePitchX,
            kTilesTop + static_cast<float>(index / kTileColumns) * kTilePitchY - scroll, kTileW, kTileH};
}

// ---- drawing (settings_draw.cpp) ------------------------------------------------

// How a row is shown this frame: its value may still be on its way.
struct RowLook
{
    std::string value; // what it shows: the value asked for, or the confirmed one
    float focus = 0.0f;
    float on = 0.0f;      // the switch's thumb, 0..1 (it may overshoot)
    float fill = 0.0f;    // the slider's thumb, 0..1
    float pending = 0.0f; // 0..1: the value is on its way to the provider
    float applied = 0.0f; // flashes to 1 when the provider confirmed it
    float nudge = 0.0f;   // a press this frame: the arrows answer
    float grabbed = 0.0f; // 0..1: the row is held in the move mode
    bool can_up = false;
    bool can_down = false;
};
void draw_row(G &g, const Rect &r, const Item &item, const RowLook &look);
void draw_heading(G &g, const Rect &r, std::string_view title);
// The plate under the focused row.
void draw_plate(G &g, const Rect &r, float amount, float lifted);
void draw_tile(G &g, const Rect &base, const Tile &tile, std::string_view name, std::string_view summary, float focus,
               bool attention, bool summary_loading);
void draw_tile_bones(G &g, const Rect &r, float phase);
void draw_form_bones(G &g, float amount);
// A page that has nothing to show, or could not be read: the words, and the one way on.
void draw_notice(G &g, Icon glyph, std::string_view title, std::string_view body, std::string_view button, float focus,
                 const Rect &button_rect);
// "SETTINGS > CAPTIONS" over the page's name.
void draw_path(G &g, const std::vector<std::string> &path, float amount);
// The panel in which the focused row explains itself.
void draw_help(G &g, const Rect &r, std::string_view words, std::string_view more);

enum class PreviewKind : std::uint8_t
{
    none,
    captions,
    theme,
    card,
};
// What the previews show: every value is the one on the page, confirmed or on its way.
struct PreviewState
{
    PreviewKind kind = PreviewKind::none;
    std::string picture; // an address for the picture under the captions and on the card, "" for none
    // captions
    bool captions = true;
    float scale = 1.0f;     // 1 is the player's own size
    float position = -1.0f; // 0..1 up from the bottom; below 0: where the player puts them
    Color text = kWhite;
    float plate = 0.55f;    // how dark the box behind the text is, 0..1
    int edge = 0;           // 0 none, 1 outline, 2 shadow
    // theme
    std::string look;       // a look's id; "" for a classic colour scheme, which has only two colours
    std::string name;
    Color ground = kBlack;
    Color accent = kWhite;
    // card
    float interface_scale = 1.0f;
    float grid_scale = 1.0f;
    int title_lines = 1;
    int subtitle_lines = 1;
    bool rounded = true;
};
void draw_preview(G &g, const Rect &r, const PreviewState &state, const Picture &picture, float amount);

} // namespace afterglow::settings
