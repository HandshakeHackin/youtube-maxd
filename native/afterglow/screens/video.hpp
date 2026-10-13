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
//   Data::menu(<id>)          the actions the profile has for this video, in the
//                             profile's own order: the More actions sheet, and
//                             the rows the Queue and From start buttons act on
//   link:video:<id>           the addresses the Share panel offers
//
// Private to the video page: no other screen includes this file.

#pragma once

#include "afterglow/screen.hpp"

#include "ui/components/list.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/skeleton.hpp"

#include <memory>
#include <optional>
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

// "local:video:<scope>:<verb>:<video>": the verb.
inline std::string verb_of(std::string_view id)
{
    if (!starts_with(id, "local:video:"))
        return {};
    const std::size_t scope_end = id.find(':', 12);
    if (scope_end == std::string_view::npos)
        return {};
    const std::size_t verb_end = id.find(':', scope_end + 1);
    return std::string(id.substr(scope_end + 1, verb_end == std::string_view::npos ? verb_end : verb_end - scope_end - 1));
}

// "account:confirm:<action>:<target>": the action.
inline std::string account_action(std::string_view id)
{
    if (!starts_with(id, "account:confirm:"))
        return {};
    const std::size_t end = id.find(':', 16);
    return std::string(id.substr(16, end == std::string_view::npos ? end : end - 16));
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
    std::string detail; // the row's second line: a playlist's size
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

// One thing asked of YouTube or of the console on the viewer's behalf, through
// Data::act(). An account change takes two rows of the provider: listing
// "account:confirm:<action>:<id>" prepares it and answers with the row
// "account:apply:<serial>"; listing that row sends it, once. Everything else is
// one row.
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
    std::string row;       // the row to act on
    unsigned ticket = 0;   // Data::act()'s
    bool applying = false; // the apply row is the one in flight
    std::string label;     // what the toast says when it is confirmed
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
    void read_details(const Page &page);
    void read_related(UpdateContext &context);
    void read_menu(UpdateContext &context);
    void read_links(UpdateContext &context);
    void lay_out(const ui::Fonts &fonts);
    void build_buttons(const ui::Fonts &fonts);
    void fill_sheet(UpdateContext &context);
    void ask_pictures(UpdateContext &context);
    void animate(UpdateContext &context);
    void input_page(const InputFrame &input, UpdateContext &context);
    void input_menu(const InputFrame &input, UpdateContext &context);
    void input_share(const InputFrame &input, UpdateContext &context);
    void activate(Act act, UpdateContext &context);
    void open_row(const MenuRow &row, UpdateContext &context);
    void open_layer(Layer layer, UpdateContext &context);
    void close_layer(UpdateContext &context);
    void start(Request request, UpdateContext &context);
    void ask_first(Request request, std::string_view title, std::string_view body, std::string_view confirm,
                   UpdateContext &context);
    void refuse(const InputFrame &input, UpdateContext &context);
    void lay_out_links(const ui::Fonts &fonts);
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
    void bone(G &g, const Rect &r, float radius) const;
    void bone_disc(G &g, float cx, float cy, float radius) const;
    void draw_failure(G &g) const;
    void draw_actions(G &g, const DrawContext &context) const;
    void draw_related(G &g, const DrawContext &context) const;
    void draw_hero(G &g, const DrawContext &context, const Phase &phase, const Motion &motion) const;
    void draw_layers(const DrawContext &context, float fade) const;
    void draw_sheet(G &g, const DrawContext &context, float amount) const;
    void draw_share(G &g, float amount) const;
    void draw_spinner(const DrawContext &context, float cx, float cy, float size, Color color) const;

    Route route_;
    std::string id_; // the video's row id
    std::string details_id_;
    std::string links_id_;
    model::Profile profile_;

    // What the catalogs said.
    Details details_;
    bool have_ = false; // details_ holds an answer (kept while a newer one is on its way)
    model::Load load_ = model::Load::idle;
    std::string error_;
    float since_ready_ = 0.0f; // seconds: bones give way to the content
    std::string measured_title_;
    std::string measured_overview_;
    int measured_limit_ = 0;
    int title_lines_ = 2;
    int description_lines_ = 3; // lines the excerpt takes (at most what the title leaves)
    long long likes_number_ = -1; // the like count when YouTube wrote it out in digits
    std::string likes_separator_;
    float meta_y_ = 0.0f;
    float channel_y_ = 0.0f;
    float description_y_ = 0.0f;
    float chips_y_ = 0.0f;
    std::vector<model::Video> related_;
    int related_pages_ = 1; // pages asked for so far
    model::Load related_load_ = model::Load::idle;
    bool related_more_ = false; // a further page is on its way
    std::string menu_id_;           // Data::menu() of the video
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
    bool share_asked_ = false;

    // Requests.
    Request request_;
    std::optional<Request> after_dialog_;
    std::shared_ptr<int> answer_ = std::make_shared<int>(0); // the dialog's: 0 none, 1 yes, 2 sign in
    gfx::DrawList nowhere_; // measuring with the design language's own functions
    ui::Spinner spinner_;   // on the control whose request is in flight
    ui::Skeleton bones_;    // the shimmer every bone of the page shares

    float age_ = 0.0f;
    float clock_ = 0.0f;
    bool reduced_ = false;
};

} // namespace afterglow::video_page
