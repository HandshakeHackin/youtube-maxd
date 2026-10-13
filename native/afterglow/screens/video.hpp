// ProsperoTube Afterglow - the video page: what the class holds (video.cpp, video_draw.cpp).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One video before it plays: what it is, who made it, what can be done with it
// and what to watch after. route.argument is the video's row id (the 11
// characters of a YouTube video). The page reads, through the data layer:
//
//   details:<id>              the first row is the video (title, excerpt, picture
//                             and its facts); the rows after it name the channel,
//                             the description and the related list
//   library:suggestions:<id>  the related shelf, page after page
//   account:menu:<id>         the actions the profile has for this video, in the
//                             profile's own order: the More actions sheet, and
//                             the rows the Queue and From start buttons open
//   link:video:<id>           the addresses the Share panel offers
//
// Private to the video page: no other screen includes this file.

#pragma once

#include "afterglow/screen.hpp"

#include "ui/components/list.hpp"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace afterglow::video_page
{

// ---- the approved board, in virtual pixels ---------------------------------------
constexpr Rect kHero{992.0f, 132.0f, 832.0f, 468.0f}; // the picture
constexpr float kColumn = 760.0f;                     // the text column, from kLeft
constexpr float kOverlineY = 168.0f;
constexpr float kTitleY = 226.0f; // first baseline
constexpr float kTitleLine = 52.0f;
constexpr float kActionY = 620.0f;
constexpr float kActionH = 64.0f;
constexpr float kActionGap = 14.0f;
constexpr float kRelatedY = 764.0f;
constexpr float kRelatedW = 284.0f;
constexpr float kRelatedH = 160.0f;
constexpr float kRelatedPitch = 304.0f;
constexpr Rect kSheet{1296.0f, -40.0f, 720.0f, 1160.0f};     // More actions
constexpr Rect kSheetList{1336.0f, 196.0f, 536.0f, 720.0f};
constexpr Rect kSharePanel{450.0f, 214.0f, 1020.0f, 652.0f}; // Share
// REQUEST: theme.hpp has kRadiusThumb, kRadiusPanel and kRadiusChip only; the
// boards also use a picture that has become a page's (24) and sheets (36 to 44).
constexpr float kRadiusHero = 24.0f;
constexpr float kRadiusSheet = 40.0f;
constexpr float kRadiusDialog = 36.0f;
constexpr float kRadiusField = 16.0f;
// REQUEST: theme.hpp's type scale stops at kTypeMeta (20) and kTypeBody (24);
// the boards set secondary lines at 19 to 23 and a name at 25.
constexpr float kTypeFine = 19.0f;  // a count under a name
constexpr float kTypeNote = 21.0f;  // a line under a panel title
constexpr float kTypeRead = 22.0f;  // running text beside a picture
constexpr float kTypeLine = 23.0f;  // the numbers under a title
constexpr float kTypeName = 25.0f;  // a channel's name
constexpr float kTypePanel = 34.0f; // a sheet's title
constexpr float kTypeDialog = 38.0f; // a panel's title

// ---- small helpers -----------------------------------------------------------------

inline bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

// REQUEST: base.hpp has tr() and nothing that puts a value into a translated
// pattern. "{0}" and "{1}" are replaced; anything else stays.
inline std::string fill(const char *pattern, std::string_view first, std::string_view second = {})
{
    std::string out;
    for (const char *at = pattern; *at != '\0'; ++at)
    {
        if (at[0] == '{' && (at[1] == '0' || at[1] == '1') && at[2] == '}')
        {
            out += at[1] == '0' ? first : second;
            at += 2;
        }
        else
        {
            out.push_back(*at);
        }
    }
    return out;
}

// REQUEST: tr() has no plural forms (the catalogs do: "one", "other"). Until a
// tr_n(pattern, count) exists the two English forms are separate texts.
inline std::string counted(int count, const char *one, const char *other)
{
    return fill(count == 1 ? one : other, std::to_string(count));
}

// What a hand-over does to a whole screen. REQUEST: every screen needs the
// motion table's "deeper / back out" and "section change" rows; the frame hands
// a Phase and moves nothing, so this is a copy each screen carries today.
struct Motion
{
    float opacity = 1.0f;   // of everything the screen draws
    float scale = 1.0f;     // about the middle of the screen
    float dx = 0.0f;
    float furniture = 1.0f; // of everything but a picture that travels
};
inline Motion hand_over(const Phase &phase, bool reduced)
{
    Motion m;
    const bool carried = phase.move == Move::zoom || phase.move == Move::shrink;
    if (phase.out > 0.0f)
    {
        m.opacity = 1.0f - tween::smoothstep(phase.out * (carried ? 3.4f : 2.4f));
        if (!reduced && phase.move == Move::fade)
            m.scale = 1.0f - 0.025f * tween::cubic_out(phase.out);
        if (!reduced && phase.move == Move::push)
            m.dx = -90.0f * tween::cubic_in(phase.out);
        if (!reduced && phase.move == Move::pop)
            m.dx = 90.0f * tween::cubic_in(phase.out);
        if (carried)
        {
            m.furniture = m.opacity; // the screen fades its furniture and keeps the picture
            m.opacity = 1.0f;
        }
    }
    else if (phase.in < 1.0f)
    {
        if (phase.move == Move::fade || phase.move == Move::push || phase.move == Move::pop)
            m.opacity = tween::smoothstep((phase.in - 0.12f) / 0.6f);
        if (!reduced && phase.move == Move::push)
            m.dx = 110.0f * (1.0f - tween::quint_out(phase.in));
        if (!reduced && phase.move == Move::pop)
            m.dx = -110.0f * (1.0f - tween::quint_out(phase.in));
    }
    return m;
}

// REQUEST: design.hpp has no bone: the resting shape of something on its way.
inline void bone(G &g, const Rect &r, float radius, float offset = 0.0f)
{
    const float light = g.reduced ? 0.5f : ui::breathe(g.time + offset, 1.7f);
    g.l.rounded_rect(r, radius, g.k.raised.with_alpha(g.k.raised.a * (0.55f + 0.45f * light)));
}

// REQUEST: design.hpp has no mark for "sent, waiting for the answer".
inline void spinner(G &g, float cx, float cy, float radius, Color color)
{
    const float turn = g.reduced ? 0.0f : g.time * 5.2f;
    g.l.ring(cx, cy, radius, 2.5f, color.with_alpha(color.a * 0.22f));
    g.l.arc(cx, cy, radius, 2.5f, turn, 1.7f, color, true);
}

// ---- what the page knows about its video ---------------------------------------------

struct Details
{
    std::string title;
    std::string overview; // the description's first lines
    std::string thumb;
    std::string views; // as YouTube words them
    std::string age;
    std::string likes;
    std::string channel;
    std::string channel_id;
    std::string avatar;
    std::string subscribers;
    std::string rating; // "like", "dislike", ""
    int duration_s = 0;
    int resume_s = 0; // the profile's own position, 0 for none
    int chapters = 0;
    int segments = 0; // SponsorBlock segments the profile skips
    int captions = 0; // languages
    int fps = 0;
    bool live = false;
    bool upcoming = false;
    bool uhd = false;
    bool hdr = false;
    bool subscribed = false;
    bool saved = false;       // in Watch later
    bool description = false; // details: names a description to read
    std::string related;      // the catalog of the related list, "" for none
};

enum class Zone : std::uint8_t
{
    channel, // the Subscribe button
    actions,
    related,
};

enum class Act : std::uint8_t
{
    play,
    restart,
    queue,
    later,
    like,
    share,
    more,
    retry, // the one button of a page that could not be loaded
};

enum class Layer : std::uint8_t
{
    none,
    menu,  // More actions, from the right
    share, // the link to take away
};

struct ActionButton
{
    Act act = Act::play;
    std::string label;
    Icon icon = Icon::play;
    bool primary = false;
    bool selected = false;
    bool waiting = false; // sent, not answered yet
    float x = 0.0f;
    float w = 0.0f;
};

// What opening a row of the actions menu does.
enum class RowKind : std::uint8_t
{
    hidden,    // the page shows it already, or this stage cannot do it
    account,   // an account change: asked, then applied once
    local,     // done on the console by listing the row
    restart,   // play from the start
    incognito, // play without history
    channel,
    reader,
    grid,
    sublist, // a list of choices inside the sheet
    note,    // the provider's own words, nothing to open
};

struct MenuRow
{
    std::string id;
    std::string title;
    RowKind kind = RowKind::hidden;
    Icon icon = Icon::more;
};

struct ShareLink
{
    std::string id;    // "video", "time", "channel", "playlist"
    std::string label; // the chip
    std::string url;
    Rect chip{};
};

// One thing asked of YouTube or of the console on the viewer's behalf.
// REQUEST: Data has no action requests (data.hpp: catalog, home, play). Until
// Data::submit exists this walks the provider's own two catalogs through
// Data::catalog: "account:confirm:<action>:<id>" answers with the row
// "account:apply:<serial>", and listing that row sends the change, once.
struct Request
{
    enum class Kind : std::uint8_t
    {
        none,
        like,
        unrate,
        save,
        unsave,
        subscribe,
        unsubscribe,
        queue,
        restart,
        incognito,
        menu, // a row of the sheet
    };
    Kind kind = Kind::none;
    std::string first;  // the catalog asked first
    std::string second; // the apply row, once the first has answered
    std::string label;  // what the toast says when it is confirmed
    bool prepared = false; // the cache was cleared for a repeated action; ask from the next frame on
};

class VideoPage final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::video;
    }
    void enter(const Route &route, UpdateContext &context, bool restored) override;
    void update(const InputFrame &input, UpdateContext &context) override;
    void draw(const DrawContext &context, const Phase &phase) const override;
    std::span<const ui::Hint> hints() const override;
    FocusInfo focus() const override;
    std::vector<std::pair<std::string, std::string>> focusable() const override;
    void snapshot(Snapshot &out) const override;

  private:
    // ---- video.cpp: reading, input, requests ----
    void read(UpdateContext &context);
    void read_details(const Page &page, const ui::Fonts &fonts);
    void read_related(UpdateContext &context);
    void read_menu(UpdateContext &context);
    void read_links(UpdateContext &context);
    void lay_out(const ui::Fonts &fonts);
    void build_buttons(const ui::Fonts &fonts);
    void fill_sheet(UpdateContext &context);
    void animate(UpdateContext &context);
    void input_page(const InputFrame &input, UpdateContext &context);
    void input_menu(const InputFrame &input, UpdateContext &context);
    void input_share(const InputFrame &input, UpdateContext &context);
    void activate(Act act, UpdateContext &context);
    void open_row(const MenuRow &row, UpdateContext &context);
    void open_layer(Layer layer, UpdateContext &context);
    void close_layer(UpdateContext &context);
    void start(Request request, UpdateContext &context);
    void advance_request(UpdateContext &context);
    void confirmed(const Page &answer, UpdateContext &context);
    void failed(const std::string &message, UpdateContext &context);
    void launch(const model::Video &video, UpdateContext &context);
    void edge(const InputFrame &input, UpdateContext &context, float direction);
    bool needs_account(UpdateContext &context);
    model::Video own() const;
    std::string menu_row(std::string_view verb) const;
    int button_index(Act act) const;
    Rect button_rect(int index) const;
    Rect subscribe_rect() const;
    Rect related_rect(int index, float scroll) const;
    bool liked() const
    {
        return liked_.value_or(details_.rating == "like");
    }
    bool saved() const
    {
        return saved_.value_or(details_.saved);
    }
    bool subscribed() const
    {
        return subscribed_.value_or(details_.subscribed);
    }
    bool can_subscribe() const
    {
        return have_ && !details_.channel_id.empty();
    }
    std::string like_label() const;

    // ---- video_draw.cpp ----
    float arrive(int index) const;
    void draw_details(G &g, const DrawContext &context) const;
    void draw_bones(G &g) const;
    void draw_failure(G &g) const;
    void draw_actions(G &g) const;
    void draw_related(G &g, const DrawContext &context) const;
    void draw_hero(G &g, const DrawContext &context, const Phase &phase, const Motion &motion) const;
    void draw_layers(const DrawContext &context, float fade) const;
    void draw_sheet(G &g, const DrawContext &context, float amount) const;
    void draw_share(G &g, float amount) const;

    Route route_;
    std::string id_; // the video's row id
    model::Profile profile_;

    // What the catalogs said.
    Details details_;
    bool have_ = false; // details_ holds an answer (kept while a newer one is on its way)
    model::Load load_ = model::Load::idle;
    std::string error_;
    float since_ready_ = 0.0f; // seconds: bones give way to the content
    std::string measured_title_;
    int title_lines_ = 2;
    int description_lines_ = 3;
    float meta_y_ = 0.0f;
    float channel_y_ = 0.0f;
    float description_y_ = 0.0f;
    float chips_y_ = 0.0f;
    std::vector<model::Video> related_;
    int related_pages_ = 1; // pages asked for so far
    model::Load related_load_ = model::Load::idle;
    bool related_more_ = false; // a further page is on its way
    std::vector<MenuRow> menu_all_; // every row of the actions menu
    model::Load menu_load_ = model::Load::idle;
    std::string menu_error_;
    std::vector<ShareLink> links_;
    model::Load links_load_ = model::Load::idle;
    std::string links_error_;

    // What the viewer changed here and YouTube confirmed, over what details: said.
    std::optional<bool> liked_;
    std::optional<bool> saved_;
    std::optional<bool> subscribed_;

    // Focus.
    Zone zone_ = Zone::actions;
    Act action_ = Act::play;
    int related_col_ = 0;
    std::vector<ActionButton> buttons_;
    tween::Spring action_pos_;
    tween::Spring channel_focus_;
    tween::Spring subscribe_w_;
    ui::Scroller related_scroll_;
    ui::SpringRect ring_;
    bool ring_placed_ = false;
    ui::Pulse nudge_;
    float nudge_dir_ = 0.0f;

    // Confirmations.
    ui::Pulse like_pop_;
    ui::Pulse saved_pop_;
    ui::Pulse subscribe_pop_;
    tween::Spring like_count_;
    bool like_count_placed_ = false;

    // The layer over the page.
    Layer layer_ = Layer::none;
    Layer shown_layer_ = Layer::menu; // the one on screen, also while it leaves
    tween::Spring layer_amount_;
    ui::ListView list_;
    std::vector<MenuRow> sheet_rows_; // the rows of the level on screen
    std::string sheet_signature_;
    std::string sheet_catalog_; // "" for the menu itself, else the list of choices opened from it
    std::string sheet_title_;
    model::Load sheet_load_ = model::Load::idle;
    std::string sheet_error_;
    int link_ = 0;
    bool link_chosen_ = false;

    // Requests.
    Request request_;
    std::set<std::string> asked_; // catalogs already used for a change: the cache must not answer them again
    std::optional<Request> after_dialog_;
    std::shared_ptr<int> answer_ = std::make_shared<int>(0); // the dialog's: 0 none, 1 yes, 2 sign in
    gfx::DrawList nowhere_; // measuring with the design language's own functions

    float age_ = 0.0f;
    float clock_ = 0.0f;
    bool reduced_ = false;
};

} // namespace afterglow::video_page
