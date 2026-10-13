// ProsperoTube Afterglow - the library: what a profile keeps, on this PS5 and on its account.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Six lists behind one row of tabs. Three live on the console and are there
// for every profile, also without an account and without a connection: History
// (with the place each video was left at), Queue and Favourites. Three live on
// the YouTube account: Watch later, Playlists and Liked; a profile that is not
// signed in is invited to sign in there, and when YouTube cannot be reached
// they point at what the console keeps. route.argument names the tab to open
// ("history", "watch_later", "playlists", "liked", "queue", "favorites").

#include "afterglow/screen.hpp"

#include "ui/components/skeleton.hpp"
#include "ui/components/stat.hpp"
#include "ui/components/tabs.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace afterglow
{

namespace
{

// ---- the page ------------------------------------------------------------------
constexpr int kColumns = 4;
constexpr int kVisibleRows = 2;
constexpr float kCellW = 394.0f;
constexpr float kCellH = 222.0f;
constexpr float kPitchX = kCellW + 24.0f;
constexpr float kPitchY = 352.0f;
constexpr float kCellText = 96.0f; // the title's two lines and the line under them
constexpr float kGridTop = 292.0f;
constexpr float kClipTop = 246.0f;
constexpr Rect kTabsBounds{kLeft, 184.0f, 1200.0f, 56.0f};
constexpr Rect kMessage{kLeft + 400.0f, 300.0f, 848.0f, 460.0f};
constexpr Rect kMessageOverTiles{kLeft + 400.0f, 262.0f, 848.0f, 420.0f};
constexpr float kTilesTitle = 736.0f;
constexpr float kTilesTop = 764.0f;
constexpr float kTileH = 124.0f;
constexpr int kTiles = 4;
constexpr int kPageLimit = 80; // pages of one list read at most
// REQUEST: theme.hpp has no size for the line under a page title, nor the sizes
// of the message block the approved empty and offline boards use.
constexpr float kTypeSub = 22.0f;
constexpr float kTypeCard = 23.0f;
constexpr float kTypeMessage = 36.0f;

struct Tab
{
    const char *id;      // focus ids, snapshots, the route's argument
    const char *catalog; // the provider's
    bool account;        // lives on the YouTube account, not on this PS5
    bool playlists;      // its rows are playlists
    Icon icon;
};
constexpr Tab kTabs[] = {
    {"history", "history", false, false, Icon::clock},
    {"watch_later", "library:watch_later", true, false, Icon::clock},
    {"playlists", "library:playlists", true, true, Icon::queue},
    {"liked", "library:playlist:LL", true, false, Icon::heart},
    {"queue", "queue", false, false, Icon::queue},
    {"favorites", "favorites", false, false, Icon::heart},
};
constexpr int kTabCount = static_cast<int>(sizeof kTabs / sizeof kTabs[0]);
enum : int
{
    kHistory = 0,
    kWatchLater = 1,
    kPlaylists = 2,
    kLiked = 3,
    kQueue = 4,
    kFavorites = 5,
};

const char *tab_label(int index)
{
    switch (index)
    {
    case kHistory:
        return tr("History");
    case kWatchLater:
        return tr("Watch later");
    case kPlaylists:
        return tr("Playlists");
    case kLiked:
        return tr("Liked");
    case kQueue:
        return tr("Queue");
    default:
        return tr("Favourites");
    }
}

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool all_digits(std::string_view text)
{
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// A translated pattern with its number in place: "{0} videos" -> "12 videos".
std::string fill(const char *pattern, std::string_view value)
{
    std::string out(pattern);
    const std::size_t at = out.find("{0}");
    if (at != std::string::npos)
        out.replace(at, 3, value);
    return out;
}

// REQUEST: the data layer cleans a row's title and subtitle of what no face
// draws, but not its facts; a fact that is shown is cleaned here meanwhile.
std::string shown_fact(const model::Row &row, std::string_view name)
{
    return clean(row.fact(name));
}

std::string videos_text(std::size_t count)
{
    return count == 1 ? std::string(tr("1 video")) : fill(tr("{0} videos"), std::to_string(count));
}

// ---- stand-ins for pieces more than one screen needs ---------------------------

// REQUEST: Data has no way to see that a page changed and no list across pages.
// A catalog read page by page. poll() runs every frame the list is on screen:
// the first call of a page asks for it, later calls see what has arrived.
struct Feed
{
    std::string catalog;
    int pages = 1;       // asked for so far
    int ready = 0;       // of them, how many have answered, counted from the first
    model::Load load = model::Load::idle; // of the first page
    std::string error;
    bool more = false;    // a page beyond those asked for exists
    bool growing = false; // the last page asked for is on its way
    bool stuck = false;   // ... or was refused: the list ends early
    std::string stuck_error;
    std::uint64_t stamp = 0;

    void reset(std::string id)
    {
        *this = Feed{};
        catalog = std::move(id);
    }
    // True when what the pages hold changed since the last call.
    bool poll(Data &data)
    {
        std::uint64_t next = 1469598103934665603ull;
        const auto mix = [&next](std::uint64_t value) { next = (next ^ value) * 1099511628211ull; };
        ready = 0;
        more = false;
        growing = false;
        stuck = false;
        for (int index = 0; index < pages; ++index)
        {
            const Page &page = data.catalog(catalog, index);
            if (index == 0)
            {
                load = page.load;
                error = page.error;
            }
            mix(static_cast<std::uint64_t>(page.load) + 11u);
            if (page.load != model::Load::ready)
            {
                growing = index > 0 && page.load == model::Load::loading;
                stuck = index > 0 && !growing;
                if (stuck)
                    stuck_error = page.error;
                break;
            }
            ++ready;
            mix(page.rows.size());
            for (const model::Row &row : page.rows)
                mix(std::hash<std::string>{}(row.id));
            more = page.more && index + 1 == pages;
        }
        const bool changed = next != stamp;
        stamp = next;
        return changed;
    }
    // One more page, when there is one and none is on its way.
    void grow()
    {
        if (more && !growing && pages < kPageLimit)
            ++pages;
    }
};

// REQUEST: the motion table's hand-overs belong to the design language; every
// screen answers its Phase the same way.
struct Hand
{
    float alpha = 1.0f;
    float dx = 0.0f;
    float scale = 1.0f;
};
Hand hand_over(const Phase &phase, bool reduced)
{
    Hand hand;
    const float in = tween::clamp01(phase.in);
    const float out = tween::clamp01(phase.out);
    switch (phase.move)
    {
    case Move::push:
    case Move::pop:
    {
        // Deeper and back out: the old page slides 90 pixels and fades, the new one comes 110 from the other side.
        const float side = phase.move == Move::push ? 1.0f : -1.0f;
        if (out > 0.0f)
        {
            hand.alpha = 1.0f - tween::cubic_in(out);
            hand.dx = -90.0f * side * tween::cubic_in(out);
        }
        else
        {
            hand.alpha = tween::quint_out(in);
            hand.dx = 110.0f * side * (1.0f - tween::quint_out(in));
        }
        break;
    }
    case Move::zoom:
    case Move::shrink:
        // A picture travels: this page's furniture is gone in the first 150 ms and returns under the picture.
        hand.alpha = out > 0.0f ? 1.0f - tween::clamp01(out / 0.29f) : tween::clamp01(in / 0.6f);
        break;
    case Move::fade:
        if (out > 0.0f)
        {
            hand.alpha = 1.0f - tween::clamp01(out / 0.4f);
            hand.scale = 1.0f - 0.025f * out;
        }
        else
        {
            hand.alpha = tween::clamp01((in - 0.12f) / 0.4f);
        }
        break;
    case Move::cut:
        hand.alpha = in * (1.0f - out);
        break;
    }
    if (reduced)
    {
        hand.dx = 0.0f;
        hand.scale = 1.0f;
    }
    return hand;
}

enum class Kind : std::uint8_t
{
    video,
    playlist,
};

struct Cell
{
    Kind kind = Kind::video;
    std::string id;     // the row
    std::string title;
    std::string line;   // a playlist's second line
    std::string count;  // a playlist's "24 videos"
    std::string thumb;
    model::Video video; // as the provider gave it: what is played
    model::Video card;  // as the card shows it (History says when it was watched)
};

// REQUEST: design.hpp has the video card only; results and the grid show playlists too.
// A playlist: its first picture with the others stacked behind it.
void playlist_card(G &g, const Rect &r, const Cell &cell, const Picture &picture, float focus, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    g.l.push_opacity(tween::clamp01(alpha));
    const Color tone = picture.ready() ? picture.tone : gfx::mix(g.k.base2, g.k.ink, 0.22f);
    g.l.rounded_rect({r.x + 28.0f, r.y - 16.0f, r.w - 56.0f, 30.0f}, 12.0f, tone.with_alpha(0.5f));
    g.l.rounded_rect({r.x + 14.0f, r.y - 8.0f, r.w - 28.0f, 30.0f}, 14.0f, tone.with_alpha(0.85f));
    if (f > 0.01f)
        g.l.shadow({r.x, r.y + 20.0f, r.w, r.h}, kRadiusThumb, 40.0f, kBlack.with_alpha(0.55f * f));
    const float shown = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
    if (shown < 1.0f)
        picture_placeholder(g, r, kRadiusThumb);
    if (shown > 0.0f)
    {
        const float dim = 0.86f + 0.14f * f;
        g.l.image(picture.texture, r, picture.cover(r.w / r.h), Color{dim, dim, dim, shown}, kRadiusThumb);
    }
    g.l.bordered_rect(r, kRadiusThumb, clear(), 1.5f, kWhite.with_alpha(0.1f));
    const Rect side{r.x + r.w - 132.0f, r.y, 132.0f, r.h};
    g.l.rounded_rect(side, kRadiusThumb, kBlack.with_alpha(0.66f));
    icon(g, Icon::queue, side.cx(), side.cy() - (cell.count.empty() ? 0.0f : 18.0f), 34.0f, kWhite);
    if (!cell.count.empty())
        text(g, g.f.semibold, fit(g.f.semibold, cell.count, kTypeMeta, side.w - 16.0f), side.cx(), side.cy() + 32.0f, kTypeMeta,
             kWhite, gfx::Align::center);
    const float after = paragraph(g, g.f.semibold, cell.title, r.x + 2.0f, r.y + r.h + 36.0f, kTypeCard, r.w - 4.0f, 30.0f,
                                  g.k.ink.with_alpha(0.82f + 0.18f * f), 2);
    text(g, g.f.regular, fit(g.f.regular, cell.line, kTypeMeta, r.w - 4.0f), r.x + 2.0f, after, kTypeMeta, g.k.ink2);
    g.l.pop_opacity();
}

Rect cell_rect(int index, float scroll = 0.0f)
{
    return {kLeft + static_cast<float>(index % kColumns) * kPitchX,
            kGridTop + static_cast<float>(index / kColumns) * kPitchY - scroll, kCellW, kCellH};
}

Rect tile_rect(int index)
{
    return {kLeft + static_cast<float>(index) * kPitchX, kTilesTop, kCellW, kTileH};
}

enum class State : std::uint8_t
{
    loading,
    ready,
    empty,
    invite,  // the list lives on an account and the profile has none
    notice,  // the provider answered with a notice instead of the list
    failed,
    offline,
};
const char *state_name(State state)
{
    static constexpr const char *kNames[] = {"loading", "ready", "empty", "invite", "notice", "failed", "offline"};
    return kNames[static_cast<std::size_t>(state)];
}

enum class Zone : std::uint8_t
{
    tabs,
    content,
};

// What a tab says when it has no list to show.
struct Message
{
    Icon icon = Icon::info;
    std::string title;
    std::string body;
    std::string action;
    ui::Button button = ui::Button::cross;
};

// A video on its way out of a list. Removing is the provider's own action: a
// row of the video's actions menu that acts when it is listed (section 5.6).
struct Removal
{
    int tab = -1;          // the list it leaves; -1 while nothing is being removed
    std::string row;       // the video
    std::string menu;      // its actions menu, where the acting row is looked up
    unsigned ticket = 0;   // the action, once it runs
    bool applying = false; // an account change was prepared and is now being applied
};

// The row of a video's actions menu that takes the video out of a tab's list.
bool removes(int tab, std::string_view id)
{
    const auto has = [id](std::string_view part) { return id.find(part) != std::string_view::npos; };
    switch (tab)
    {
    case kHistory:
        return starts_with(id, "local:video:") && has(":forget:");
    case kWatchLater:
        return starts_with(id, "account:confirm:watch_later_remove:");
    case kLiked:
        return starts_with(id, "account:confirm:unrate:");
    // REQUEST: the provider changes the queue and the favourites through
    // tube_toggle_queue() and tube_toggle_favorite() only; these two acting
    // rows of the video's menu are asked of it.
    case kQueue:
        return starts_with(id, "local:video:") && has(":queue:");
    case kFavorites:
        return starts_with(id, "local:video:") && has(":favorite:");
    default:
        return false;
    }
}

struct Pane
{
    Feed feed;
    std::vector<Cell> cells;
    // Rows the provider has removed and whose list has not answered again yet:
    // they stay off the page meanwhile.
    std::vector<std::string> hidden;
    std::string notice; // the provider's words, when it answered with a notice
    int total = -1;     // how long the whole list is, where its first row says so (the fact "total")
    int focus = 0;
    int first_row = 0;
    tween::Spring scroll;
    float reveal = 0.0f;      // how far the answer has taken over from the bones
    bool was_loading = false; // bones were shown: the answer cross-fades in
    bool told_stuck = false;  // a toast has said that the list ends early
    int tile = 0;             // the focused tile of "Kept on this PS5"
};

class Library final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::library;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        data_ = &context.services.data;
        fonts_ = &context.services.fonts;
        if (restored)
            return; // back from a playlist or a video: the tab, the focus and the scroll stay
        route_ = route;
        removal_ = Removal{};
        age_ = 0.0f;
        content_age_ = 0.0f;
        tab_ = 0;
        for (int i = 0; i < kTabCount; ++i)
        {
            if (route.argument == kTabs[i].id)
                tab_ = i;
            panes_[static_cast<std::size_t>(i)] = Pane{};
            panes_[static_cast<std::size_t>(i)].feed.reset(kTabs[i].catalog);
        }
        prev_tab_ = tab_;
        follows_.reset("subscriptions");
        follows_count_ = -1;
        zone_ = Zone::content;
        ring_snap_ = true;
        rippled_from_ = -1;
        swap_.running = false;
        profile_ = context.services.data.profile();
        shown_state_ = state_of(tab_);

        // The kit's components measure with the faces every look shares; they take the look itself when drawn.
        tabs_.style.theme = look_afterglow().theme;
        tabs_.style.kind = ui::TabKind::pill;
        tabs_.style.text_size = kTypeBody;
        tabs_.style.height = kTabsBounds.h;
        tabs_.set_bounds(kTabsBounds);
        std::vector<ui::TabItem> items;
        for (int i = 0; i < kTabCount; ++i)
            items.push_back({tab_label(i), 0, false, i});
        tabs_.set_tabs(std::move(items));
        tabs_.set_active(tab_, true);
        language_ = context.services.platform.language_revision();

        bone_.style.kind = ui::SkeletonKind::card;
        bone_.style.picture = 0.7f;
        bone_.style.lines = 2;
        bone_.style.radius = kRadiusThumb;
        block_.style.title_size = kTypeMessage;
        block_.style.body_size = kTypeBody;
        block_.style.hint_size = kTypeBody;
        block_.style.max_text_width = 640.0f;
        block_.style.max_lines = 4;
        block_.enter();
        block_out_ = block_;
        spinner_.style.kind = ui::SpinnerKind::arc;
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        const float dt = context.dt;
        Data &data = context.services.data;
        data_ = &data;
        fonts_ = &context.services.fonts;
        age_ += dt;
        content_age_ += dt;
        ripple_age_ += dt;
        swap_.update(dt);
        nudge_.update(dt, 10.0f);
        profile_ = data.profile();
        if (language_ != context.services.platform.language_revision())
        {
            language_ = context.services.platform.language_revision();
            for (int i = 0; i < kTabCount; ++i)
                tabs_.tab(i).label = tab_label(i);
        }

        // What is read: the tab on screen, the two lists whose tabs carry a
        // count, and what the console keeps while its tiles are shown.
        for (int i = 0; i < kTabCount; ++i)
        {
            const bool counted = i == kQueue || i == kWatchLater;
            if (kTabs[i].account && !profile_.signed_in)
                continue;
            if (i != tab_ && !counted && !(tiles_ && !kTabs[i].account))
                continue;
            if (panes_[static_cast<std::size_t>(i)].feed.poll(data))
                rebuild(i);
        }
        if (tiles_ && follows_.poll(data))
        {
            follows_count_ = -1;
            if (follows_.ready > 0 && !follows_.more)
            {
                follows_count_ = 0;
                for (int index = 0; index < follows_.ready; ++index)
                    follows_count_ += static_cast<int>(data.catalog(follows_.catalog, index).channels.size());
            }
        }
        advance_removal(context);
        tabs_.tab(kWatchLater).badge = profile_.signed_in ? counted(kWatchLater) : 0;
        tabs_.tab(kQueue).badge = counted(kQueue);

        Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        const State state = state_of(tab_);
        tiles_ = state == State::offline && kTabs[tab_].account;
        if (state != shown_state_)
        {
            shown_state_ = state;
            block_.enter();
        }
        if (state == State::loading)
        {
            pane.reveal = 0.0f;
            pane.was_loading = true;
        }
        else
        {
            pane.reveal = pane.was_loading ? std::min(1.0f, pane.reveal + dt / 0.3f) : 1.0f;
        }

        // The focus stays on something that exists; the row it is in stays on the page.
        const int count = static_cast<int>(pane.cells.size());
        pane.focus = std::clamp(pane.focus, 0, std::max(0, count - 1));
        const int rows = (count + kColumns - 1) / kColumns;
        const int row = pane.focus / kColumns;
        // On the last row that has arrived while the next page is on its way, the list waits with that page's bones in view.
        const bool waiting = pane.feed.growing && rows > 0 && row == rows - 1;
        pane.first_row = std::clamp(pane.first_row, row - (kVisibleRows - 1), row);
        pane.first_row = waiting ? row : std::clamp(pane.first_row, 0, std::max(0, rows - kVisibleRows));
        pane.scroll.target = static_cast<float>(pane.first_row) * kPitchY;
        if (state == State::ready && rows - 1 - row <= 2)
            pane.feed.grow(); // the end of what has arrived is near: ask for the next page
        if (pane.feed.stuck && !pane.told_stuck)
        {
            pane.told_stuck = true;
            context.nav.toast(pane.feed.stuck_error.empty() ? std::string(tr("The rest of this list could not be loaded.")) : pane.feed.stuck_error,
                              ToastKind::warning);
        }
        for (Pane &each : panes_)
            each.scroll.update(dt, context.reduced_motion ? 60.0f : 12.0f);
        // Pictures are asked for as soon as the list is known, a row ahead of what
        // is on the page, so that a row which scrolls in is already there.
        for (int i = std::max(0, (pane.first_row - 1) * kColumns); i < std::min(count, (pane.first_row + kVisibleRows + 1) * kColumns); ++i)
            (void)context.services.images.get(pane.cells[static_cast<std::size_t>(i)].thumb, PictureSize::card);
        if (state == State::ready && count > 0)
            (void)context.services.images.get(pane.cells[static_cast<std::size_t>(pane.focus)].thumb, PictureSize::ambient);

        const Zone zone = zone_now(state);
        tabs_.set_focused(zone == Zone::tabs);
        tabs_.style.reduced_motion = context.reduced_motion;
        tabs_.update(dt);
        bone_.style.reduced_motion = context.reduced_motion;
        bone_.update(dt);
        block_.style.reduced_motion = context.reduced_motion;
        block_.update(dt);
        block_out_.update(dt);
        spinner_.style.reduced_motion = context.reduced_motion;
        spinner_.update(dt);

        const Rect target = ring_target(state);
        if (ring_snap_ || age_ < 0.1f)
            ring_.snap(target);
        else
            ring_.target(target);
        ring_snap_ = false;
        ring_.update(dt, context.reduced_motion ? 60.0f : 20.0f);

        if (context.top)
        {
            act(input, context, state, zone);
            // The page takes its colour from the picture in focus.
            const State now = state_of(tab_);
            const Pane &shown = panes_[static_cast<std::size_t>(tab_)];
            context.nav.ambient(now == State::ready && !shown.cells.empty()
                                    ? std::string_view(shown.cells[static_cast<std::size_t>(std::clamp(
                                                                       shown.focus, 0, static_cast<int>(shown.cells.size()) - 1))]
                                                           .thumb)
                                    : std::string_view());
        }
        write_hints();
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        const Hand hand = hand_over(phase, g.reduced);
        if (hand.alpha <= 0.004f)
            return;
        g.l.push_opacity(hand.alpha);
        g.l.push_transform(hand.scale, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, hand.dx, 0.0f);
        ui::Canvas canvas{g.l, g.f, g.glass, g.time};
        {
            // REQUEST: the frame draws a page's title but not the line under it.
            Enter enter(g, tween::stagger(age_, 0, 0.05f, 0.5f), -14.0f);
            text(g, g.f.regular, fit(g.f.regular, subtitle(), kTypeSub, 1100.0f), kLeft, 166.0f, kTypeSub, g.k.ink2);
        }
        {
            Enter enter(g, tween::stagger(age_, 1, 0.05f, 0.5f), 18.0f);
            ui::TabBar tabs = tabs_;
            tabs.style.theme = g.k.theme;
            tabs.draw(canvas);
            // Where this list lives and how long it is, in what room the tabs leave.
            const Rect last = tabs.tab_rect(g.f, kTabCount - 1);
            const float from = std::min(last.x + last.w, kTabsBounds.x + kTabsBounds.w) + 40.0f;
            if (kRight - from >= 180.0f)
                text(g, g.f.regular, fit(g.f.regular, status(tab_), kTypeMeta, kRight - from), kRight, 220.0f, kTypeMeta,
                     g.k.ink3, gfx::Align::right);
        }
        if (swap_.running)
        {
            // Another tab: the old list leaves the way the plate went, the new one arrives from the other side.
            const float t = swap_.progress();
            const float travel = g.reduced ? 0.0f : 64.0f * static_cast<float>(swap_dir_);
            draw_pane(context, g, canvas, prev_tab_, 1.0f - tween::clamp01(t / 0.55f), -travel * tween::cubic_in(t), false);
            draw_pane(context, g, canvas, tab_, tween::clamp01((t - 0.2f) / 0.6f), travel * (1.0f - tween::cubic_out(t)), true);
        }
        else
        {
            draw_pane(context, g, canvas, tab_, 1.0f, 0.0f, true);
        }
        g.l.pop_transform();
        g.l.pop_opacity();
    }

    std::string title() const override
    {
        return tr("Library");
    }

    std::span<const ui::Hint> hints() const override
    {
        return {hints_.data(), hint_count_};
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        const State state = state_of(tab_);
        const Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        if (zone_now(state) == Zone::tabs)
        {
            info.id = std::string("tab/") + kTabs[tab_].id;
            info.text = tab_label(tab_);
            info.rect = fonts_ != nullptr ? tabs_.tab_rect(*fonts_, tab_) : kTabsBounds;
            info.index = tab_;
            info.total = kTabCount;
            return info;
        }
        if (state == State::offline)
        {
            info.id = "kept/" + std::to_string(pane.tile);
            info.text = tile_name(pane.tile);
            info.rect = tile_rect(pane.tile);
            info.index = pane.tile;
            info.total = kTiles;
            return info;
        }
        info.id = std::string(kTabs[tab_].id) + "/" + std::to_string(pane.focus);
        info.rect = cell_rect(pane.focus, pane.scroll.target);
        info.index = pane.focus;
        info.total = static_cast<int>(pane.cells.size());
        if (pane.focus < static_cast<int>(pane.cells.size()))
            info.text = pane.cells[static_cast<std::size_t>(pane.focus)].title;
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        for (int i = 0; i < kTabCount; ++i)
            out.emplace_back(std::string("tab/") + kTabs[i].id, tab_label(i));
        const State state = state_of(tab_);
        const Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        if (state == State::ready)
        {
            for (std::size_t i = 0; i < pane.cells.size() && i < 48; ++i)
                out.emplace_back(std::string(kTabs[tab_].id) + "/" + std::to_string(i), pane.cells[i].title);
        }
        else if (state == State::offline && kTabs[tab_].account)
        {
            for (int i = 0; i < kTiles; ++i)
                out.emplace_back("kept/" + std::to_string(i), tile_name(i));
        }
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        const State state = state_of(tab_);
        const Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        out.set("tab", kTabs[tab_].id);
        out.set("catalog", kTabs[tab_].catalog);
        out.set("state", state_name(state));
        out.set("zone", zone_now(state) == Zone::tabs ? "tabs" : "content");
        out.set("items", static_cast<int>(pane.cells.size()));
        out.set("index", pane.focus);
        out.set("row", pane.first_row);
        out.set("pages", pane.feed.pages);
        out.set("more", pane.feed.more || pane.feed.growing ? 1 : 0);
        out.set("signed_in", profile_.signed_in ? 1 : 0);
        out.set("watch_later", tabs_.tabs()[kWatchLater].badge);
        out.set("queue", tabs_.tabs()[kQueue].badge);
        out.set("status", status(tab_));
        out.set("removing", removal_.tab >= 0 ? removal_.row : std::string());
        std::vector<std::string> titles;
        for (std::size_t i = 0; i < pane.cells.size() && i < 12; ++i)
            titles.push_back(pane.cells[i].title);
        out.list("titles", std::move(titles));
    }

  private:
    // ---- what a tab holds ------------------------------------------------------------

    State state_of(int index) const
    {
        const Pane &pane = panes_[static_cast<std::size_t>(index)];
        if (kTabs[index].account && !profile_.signed_in)
            return State::invite;
        if (!pane.cells.empty())
            return State::ready; // also while a newer answer is on its way
        if (pane.feed.ready > 0)
            return pane.notice.empty() ? State::empty : State::notice;
        if (pane.feed.load == model::Load::failed)
            return State::failed;
        if (pane.feed.load == model::Load::offline)
            return State::offline;
        return State::loading;
    }

    // Where the focus is: on the list when there is one (or one is coming), else on the tabs.
    Zone zone_now(State state) const
    {
        if (zone_ == Zone::tabs)
            return Zone::tabs;
        const bool tiles = state == State::offline && kTabs[tab_].account;
        return state == State::ready || state == State::loading || tiles ? Zone::content : Zone::tabs;
    }

    // How long a list is: what its first row says, else what was counted once
    // every page has been read; -1 while that is not known.
    int length(int index) const
    {
        const Pane &pane = panes_[static_cast<std::size_t>(index)];
        if (pane.feed.ready == 0 && pane.cells.empty())
            return -1;
        if (pane.total >= 0)
            return std::max(0, pane.total - static_cast<int>(pane.hidden.size()));
        return pane.feed.more || pane.feed.growing ? -1 : static_cast<int>(pane.cells.size());
    }
    // The number a tab shows.
    int counted(int index) const
    {
        return std::max(0, length(index));
    }

    // Turns the pages that have arrived into cards. The focus stays on the same row where that row still exists.
    void rebuild(int index)
    {
        Pane &pane = panes_[static_cast<std::size_t>(index)];
        if (pane.feed.ready == 0)
        {
            // Asked again and not answered yet: what was there stays until the answer replaces it.
            if (pane.feed.load != model::Load::loading && pane.feed.load != model::Load::idle)
                pane.cells.clear();
            return;
        }
        const std::string focused = pane.focus < static_cast<int>(pane.cells.size()) ? pane.cells[static_cast<std::size_t>(pane.focus)].id : std::string();
        pane.cells.clear();
        pane.notice.clear();
        pane.total = -1;
        std::vector<std::string> still_hidden;
        if (data_ == nullptr)
            return;
        Data &data = *data_;
        for (int number = 0; number < pane.feed.ready; ++number)
        {
            const Page &page = data.catalog(pane.feed.catalog, number);
            if (number == 0 && !page.rows.empty() && all_digits(page.rows.front().fact("total")))
                pane.total = std::atoi(page.rows.front().fact("total").c_str());
            std::size_t next_video = 0;
            for (const model::Row &row : page.rows)
            {
                const bool is_video = next_video < page.videos.size() && page.videos[next_video].id == row.id;
                const model::Video *video = is_video ? &page.videos[next_video++] : nullptr;
                if (row.id == "account:notice")
                {
                    if (pane.notice.empty())
                        pane.notice = row.subtitle.empty() ? row.title : row.title + ". " + row.subtitle;
                    continue;
                }
                if (std::find(pane.hidden.begin(), pane.hidden.end(), row.id) != pane.hidden.end())
                {
                    still_hidden.push_back(row.id);
                    continue;
                }
                Cell cell;
                cell.id = row.id;
                cell.title = row.title;
                cell.thumb = row.thumb;
                if (video != nullptr)
                {
                    cell.video = *video;
                    cell.card = *video;
                    // History says when the video was watched instead of how many watched it.
                    const std::string watched = shown_fact(row, "watched");
                    if (index == kHistory && !watched.empty())
                    {
                        cell.card.views.clear();
                        cell.card.age = watched;
                    }
                }
                else if (row.folder && (starts_with(row.id, "library:playlist:") || starts_with(row.id, "playlist:")))
                {
                    cell.kind = Kind::playlist;
                    const std::string owner = row.fact("owner").empty() ? row.subtitle : shown_fact(row, "owner");
                    cell.line = owner.empty() ? std::string(tr("Playlist")) : std::string(tr("Playlist")) + " \xC2\xB7 " + owner;
                    const std::string amount = shown_fact(row, "count");
                    cell.count = all_digits(amount) ? (amount == "1" ? std::string(tr("1 video")) : fill(tr("{0} videos"), amount)) : amount;
                }
                else
                {
                    continue; // a row this page has no card for
                }
                pane.cells.push_back(std::move(cell));
            }
        }
        pane.hidden = std::move(still_hidden);
        for (std::size_t i = 0; i < pane.cells.size(); ++i)
        {
            if (!focused.empty() && pane.cells[i].id == focused)
                pane.focus = static_cast<int>(i);
        }
    }

    // ---- input ---------------------------------------------------------------------------

    void refuse(const InputFrame &input, UpdateContext &context, float dx, float dy)
    {
        if (input.nav_repeat)
            return; // a held direction against an edge stays silent
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        if (!context.reduced_motion)
        {
            nudge_.trigger();
            nudge_x_ = dx;
            nudge_y_ = dy;
        }
    }

    // Another tab comes up: its own focus and scroll position return with it.
    void show(int index, int direction, bool reduced)
    {
        if (index == tab_)
            return;
        prev_tab_ = tab_;
        tab_ = index;
        swap_dir_ = direction;
        swap_.start(reduced ? 0.14f : 0.3f);
        content_age_ = 0.0f;
        rippled_from_ = -1;
        ring_snap_ = true;
        block_out_ = block_;
        block_.enter();
        shown_state_ = state_of(tab_);
    }

    void turn(int direction, const InputFrame &input, UpdateContext &context)
    {
        if (tabs_.step(direction, input, context.feedback) == ui::Event::changed)
            show(tabs_.active(), direction, context.reduced_motion);
    }

    void act(const InputFrame &input, UpdateContext &context, State state, Zone zone)
    {
        Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        const bool tiles = state == State::offline && kTabs[tab_].account;
        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (pressed(input, Action::page_next) || pressed(input, Action::page_prev))
        {
            turn(pressed(input, Action::page_next) ? 1 : -1, input, context);
            return;
        }
        if (zone == Zone::tabs)
        {
            const bool below = state == State::ready || state == State::loading || tiles;
            switch (input.nav)
            {
            case Direction::left:
                if (tab_ == 0)
                    context.nav.open_rail();
                else
                    turn(-1, input, context);
                break;
            case Direction::right:
                turn(1, input, context);
                break;
            case Direction::down:
                if (below)
                    enter_content(context);
                else
                    refuse(input, context, 0.0f, 1.0f);
                break;
            case Direction::up:
                refuse(input, context, 0.0f, -1.0f);
                break;
            case Direction::none:
                break;
            }
        }
        else if (state == State::ready)
        {
            move_in_grid(input, context, pane);
        }
        else if (tiles)
        {
            if (input.nav == Direction::left && pane.tile == 0)
                context.nav.open_rail();
            else if (input.nav == Direction::up)
                leave_content(context);
            else if (input.nav == Direction::down || (input.nav == Direction::right && pane.tile == kTiles - 1))
                refuse(input, context, input.nav == Direction::right ? 1.0f : 0.0f, input.nav == Direction::down ? 1.0f : 0.0f);
            else if (input.nav == Direction::left || input.nav == Direction::right)
            {
                pane.tile += input.nav == Direction::right ? 1 : -1;
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(tile_rect(pane.tile).cx()));
            }
        }
        else if (input.nav == Direction::left)
        {
            context.nav.open_rail(); // the list is still on its way
        }
        else if (input.nav == Direction::up)
        {
            leave_content(context);
        }

        if (pressed(input, Action::confirm))
            confirm(context, state, zone, tiles);
        else if (pressed(input, Action::west) && state == State::ready && zone == Zone::content)
            remove(context, pane);
        else if (pressed(input, Action::touch) && state == State::ready && zone == Zone::content)
            open_details(context, pane);
        else if (pressed(input, Action::north))
        {
            if (state == State::offline)
                try_again(context);
            else if (tab_ == kHistory && profile_.signed_in)
                context.nav.push(Route{ScreenId::grid, "library:history", tr("YouTube history")}, Move::push);
        }
        else if (pressed(input, Action::menu) && state == State::invite)
            context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
    }

    void enter_content(UpdateContext &context)
    {
        zone_ = Zone::content;
        ring_snap_ = true;
        context.feedback.play(audio::Cue::tab, 1.03f);
    }

    void leave_content(UpdateContext &context)
    {
        zone_ = Zone::tabs;
        context.feedback.play(audio::Cue::tab, 1.1f);
    }

    void move_in_grid(const InputFrame &input, UpdateContext &context, Pane &pane)
    {
        const int count = static_cast<int>(pane.cells.size());
        const int column = pane.focus % kColumns;
        int next = pane.focus;
        switch (input.nav)
        {
        case Direction::left:
            if (column == 0)
            {
                context.nav.open_rail();
                return;
            }
            next = pane.focus - 1;
            break;
        case Direction::right:
            if (column == kColumns - 1 || pane.focus + 1 >= count)
            {
                refuse(input, context, 1.0f, 0.0f);
                return;
            }
            next = pane.focus + 1;
            break;
        case Direction::up:
            if (pane.focus < kColumns)
            {
                leave_content(context);
                return;
            }
            next = pane.focus - kColumns;
            break;
        case Direction::down:
            if (pane.focus / kColumns >= (count - 1) / kColumns)
            {
                if (pane.feed.stuck && !input.nav_repeat)
                {
                    // The page after this one was refused: ask for the list again; what is here stays meanwhile.
                    pane.told_stuck = false;
                    context.services.data.forget(pane.feed.catalog);
                }
                else if (!pane.feed.more && !pane.feed.growing)
                {
                    refuse(input, context, 0.0f, 1.0f); // the end of the list; while more is arriving the press just waits
                }
                return;
            }
            next = std::min(pane.focus + kColumns, count - 1);
            break;
        case Direction::none:
            return;
        }
        pane.focus = next;
        context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(cell_rect(next).cx()));
    }

    void confirm(UpdateContext &context, State state, Zone zone, bool tiles)
    {
        Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        switch (state)
        {
        case State::ready:
            if (zone == Zone::tabs)
            {
                enter_content(context);
                break;
            }
            if (const Cell &cell = pane.cells[static_cast<std::size_t>(pane.focus)]; cell.kind == Kind::playlist)
            {
                context.nav.push(Route{ScreenId::grid, cell.id, cell.title}, Move::push);
            }
            else if (context.services.data.play(cell.video))
            {
                context.feedback.play(audio::Cue::launch);
                context.feedback.rumble(0.6f, 0.16f);
            }
            else
            {
                context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                context.feedback.rumble(0.25f, 0.05f);
            }
            break;
        case State::empty:
        case State::notice:
            context.nav.root(Route{ScreenId::home, {}, {}});
            break;
        case State::invite:
            context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
            break;
        case State::failed:
            try_again(context);
            break;
        case State::offline:
            if (!tiles)
                try_again(context);
            else if (zone == Zone::tabs)
                enter_content(context);
            else if (pane.tile == 3)
                // What the console follows is the channels page's; "subscriptions" is the provider's list of them.
                context.nav.push(Route{ScreenId::channels, "subscriptions", tr("Followed channels")}, Move::push);
            else
            {
                static constexpr int kOpens[] = {kHistory, kFavorites, kQueue};
                const int to = kOpens[pane.tile];
                tabs_.set_active(to);
                context.feedback.play(audio::Cue::tab);
                show(to, to > tab_ ? 1 : -1, context.reduced_motion);
            }
            break;
        case State::loading:
            break;
        }
    }

    void try_again(UpdateContext &context)
    {
        context.services.data.forget(kTabs[tab_].catalog); // the next read asks again
        context.feedback.play(audio::Cue::select);
    }

    // The video page of the card in focus; its picture grows out of the card.
    void open_details(UpdateContext &context, const Pane &pane)
    {
        const Cell &cell = pane.cells[static_cast<std::size_t>(pane.focus)];
        if (cell.kind != Kind::video)
            return;
        // REQUEST: Touchpad opens the Actions sheet of a video; until the frame has one, the video page stands in.
        context.nav.push(Route{ScreenId::video, cell.video.id, cell.video.title}, Move::zoom,
                         grown(cell_rect(pane.focus, pane.scroll.target), 0.05f, 5.0f));
    }

    // Square on a video: its actions menu is read for the row that takes it out of this list.
    void remove(UpdateContext &context, Pane &pane)
    {
        const Cell &cell = pane.cells[static_cast<std::size_t>(pane.focus)];
        if (cell.kind != Kind::video || removal_.tab >= 0)
        {
            refuse(InputFrame{}, context, 0.0f, 0.0f); // a playlist is not removed from here; one removal runs at a time
            return;
        }
        removal_ = Removal{};
        removal_.tab = tab_;
        removal_.row = cell.id;
        removal_.menu = context.services.data.menu(cell.id, cell.title);
        if (removal_.menu.empty())
        {
            fail_removal(context, {});
            return;
        }
        context.services.data.forget(removal_.menu); // the menu as it is now, not as it was listed before
        context.feedback.play(audio::Cue::select);
    }

    void advance_removal(UpdateContext &context)
    {
        if (removal_.tab < 0)
            return;
        Data &data = context.services.data;
        if (removal_.ticket == 0)
        {
            const Page &menu = data.catalog(removal_.menu);
            if (menu.load == model::Load::loading || menu.load == model::Load::idle)
                return;
            const model::Row *acting = nullptr;
            for (const model::Row &row : menu.rows)
            {
                if (menu.load == model::Load::ready && acting == nullptr && removes(removal_.tab, row.id))
                    acting = &row;
            }
            if (acting == nullptr)
                fail_removal(context, menu.load == model::Load::ready ? std::string() : menu.error);
            else
                removal_.ticket = data.act(acting->id);
            return;
        }
        const Page &answer = data.action(removal_.ticket);
        if (answer.load == model::Load::loading)
            return;
        if (answer.load != model::Load::ready)
        {
            fail_removal(context, answer.error);
            return;
        }
        if (!removal_.applying && !answer.rows.empty() && starts_with(answer.rows.front().id, "account:apply:"))
        {
            // The provider prepares a change to the account and applies it with a second row; Square was the word for both.
            removal_.applying = true;
            removal_.ticket = data.act(answer.rows.front().id);
            return;
        }
        // Removed: the card leaves at once, the list is asked again, and the cards after it close the gap.
        Pane &pane = panes_[static_cast<std::size_t>(removal_.tab)];
        const auto gone = std::find_if(pane.cells.begin(), pane.cells.end(), [this](const Cell &cell) { return cell.id == removal_.row; });
        if (gone != pane.cells.end())
        {
            if (removal_.tab == tab_)
            {
                rippled_from_ = static_cast<int>(gone - pane.cells.begin());
                ripple_age_ = 0.0f;
            }
            pane.cells.erase(gone);
        }
        pane.hidden.push_back(removal_.row);
        data.forget(pane.feed.catalog);
        data.forget(removal_.menu);
        const char *said = removal_.tab == kHistory      ? tr("Removed from history")
                           : removal_.tab == kWatchLater ? tr("Removed from Watch later")
                           : removal_.tab == kLiked      ? tr("Removed from liked videos")
                           : removal_.tab == kQueue      ? tr("Removed from the queue")
                                                         : tr("Removed from favourites");
        context.feedback.play(audio::Cue::favorite_off);
        context.feedback.rumble(0.3f, 0.06f);
        context.nav.toast(said, ToastKind::success);
        removal_ = Removal{};
    }

    // The provider's own words when it refused; ours when the menu has no such action.
    void fail_removal(UpdateContext &context, const std::string &why)
    {
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        context.nav.toast(why.empty() ? std::string(tr("This video cannot be removed from here.")) : why, ToastKind::error);
        if (!removal_.menu.empty())
            context.services.data.forget(removal_.menu);
        removal_ = Removal{};
    }

    void write_hints()
    {
        hint_count_ = 0;
        const auto add = [this](ui::Button button, const char *label, ui::Button second = ui::Button::none)
        {
            if (hint_count_ < hints_.size())
                hints_[hint_count_++] = ui::Hint{button, label, second};
        };
        const State state = state_of(tab_);
        const Zone zone = zone_now(state);
        const Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        const bool on_card = state == State::ready && zone == Zone::content && pane.focus < static_cast<int>(pane.cells.size());
        const bool video = on_card && pane.cells[static_cast<std::size_t>(pane.focus)].kind == Kind::video;
        switch (state)
        {
        case State::ready:
            if (video)
                add(ui::Button::cross, tr("Play"));
            else if (on_card)
                add(ui::Button::cross, tr("Open"));
            break;
        case State::empty:
        case State::notice:
            add(ui::Button::cross, tr("Browse Home"));
            break;
        case State::invite:
            add(ui::Button::cross, tr("Sign in"));
            break;
        case State::failed:
            add(ui::Button::cross, tr("Try again"));
            break;
        case State::offline:
            if (kTabs[tab_].account && zone == Zone::content)
                add(ui::Button::cross, tr("Open"));
            add(kTabs[tab_].account ? ui::Button::triangle : ui::Button::cross, tr("Try again"));
            break;
        case State::loading:
            break;
        }
        add(ui::Button::l1, tr("Section"), ui::Button::r1);
        if (video)
        {
            add(ui::Button::square, tr("Remove"));
            add(ui::Button::touchpad, tr("Actions"));
        }
        if (tab_ == kHistory && profile_.signed_in && state != State::offline)
            add(ui::Button::triangle, tr("YouTube history"));
        if (!video)
            add(ui::Button::circle, tr("Back"));
    }

    // ---- words ---------------------------------------------------------------------------

    std::string subtitle() const
    {
        const std::string name = profile_.guest || profile_.name.empty() ? std::string(tr("Guest")) : profile_.name;
        return name + " \xC2\xB7 " + (profile_.signed_in ? tr("on this PS5 and on YouTube") : tr("on this PS5"));
    }

    // Where the list lives, after how long it is once every page of it has been read.
    std::string status(int index) const
    {
        const Pane &pane = panes_[static_cast<std::size_t>(index)];
        const std::string place = kTabs[index].account ? tr("on YouTube") : tr("on this PS5");
        (void)pane;
        if (state_of(index) != State::ready || length(index) < 0)
            return place;
        const std::size_t count = static_cast<std::size_t>(length(index));
        const std::string amount = !kTabs[index].playlists ? videos_text(count)
                                   : count == 1           ? std::string(tr("1 playlist"))
                                                          : fill(tr("{0} playlists"), std::to_string(count));
        return amount + " \xC2\xB7 " + place;
    }

    Message message_of(int index, State state) const
    {
        const Pane &pane = panes_[static_cast<std::size_t>(index)];
        Message m;
        m.icon = kTabs[index].icon;
        switch (state)
        {
        case State::invite:
            m.icon = Icon::user;
            m.title = index == kWatchLater ? tr("Sign in to see Watch later")
                      : index == kPlaylists ? tr("Sign in to see your playlists")
                                            : tr("Sign in to see liked videos");
            m.body = tr("These lists live on a YouTube account. History, the play queue and favourites stay on this PS5 and need none.");
            m.action = tr("Sign in with a code");
            break;
        case State::empty:
            switch (index)
            {
            case kHistory:
                m.title = tr("Nothing watched yet");
                m.body = tr("Videos you start are kept here on this PS5, each with the place you left it at.");
                break;
            case kWatchLater:
                m.title = tr("Nothing saved for later yet");
                m.body = tr("Press Square on any video to keep it here. Watch later follows your account, so it is the same list on your phone.");
                break;
            case kPlaylists:
                m.title = tr("No playlists yet");
                m.body = tr("Playlists you make or save on YouTube appear here. Add a video to one from its Actions.");
                break;
            case kLiked:
                m.title = tr("No liked videos yet");
                m.body = tr("Like a video on its page and YouTube collects it here, on every device of your account.");
                break;
            case kQueue:
                m.title = tr("The play queue is empty");
                m.body = tr("Add videos from their Actions and they play one after another. The queue stays on this PS5.");
                break;
            default:
                m.title = tr("No favourites yet");
                m.body = tr("Keep a video as a favourite from its Actions. Favourites stay on this PS5 and need no account.");
                break;
            }
            m.action = tr("Browse Home");
            break;
        case State::notice:
            m.icon = Icon::info;
            m.title = tr("Not available for this profile");
            m.body = pane.notice;
            m.action = tr("Browse Home");
            break;
        case State::failed:
            m.icon = Icon::alert;
            m.title = tr("This list could not be loaded");
            m.body = pane.feed.error.empty() ? std::string(tr("YouTube did not answer as expected.")) : pane.feed.error;
            m.action = tr("Try again");
            break;
        case State::offline:
            m.icon = Icon::offline;
            m.title = tr("No connection to YouTube");
            if (kTabs[index].account)
            {
                m.body = tr("The PS5 is not reaching the internet. Your history, favourites and play queue on this console are still here.");
                m.button = ui::Button::triangle;
            }
            else
            {
                m.body = pane.feed.error;
            }
            m.action = tr("Try again");
            break;
        case State::loading:
        case State::ready:
            break;
        }
        return m;
    }

    static const char *tile_name(int index)
    {
        switch (index)
        {
        case 0:
            return tr("Continue watching");
        case 1:
            return tr("Favourites");
        case 2:
            return tr("Play queue");
        default:
            return tr("Followed channels");
        }
    }

    // How much of it the console holds; "" until that is known.
    std::string tile_count(int index) const
    {
        if (index == 3)
            return follows_count_ < 0 ? std::string()
                   : follows_count_ == 1 ? std::string(tr("1 channel"))
                                         : fill(tr("{0} channels"), std::to_string(follows_count_));
        static constexpr int kPanes[] = {kHistory, kFavorites, kQueue};
        const int count = length(kPanes[index]);
        if (count < 0)
            return {};
        if (index == 0) // the tile opens History, where every video waits at the place it was left at
            return count == 1 ? std::string(tr("1 video, resume point kept"))
                              : fill(tr("{0} videos, resume points kept"), std::to_string(count));
        if (index == 2)
            return fill(tr("{0} waiting"), std::to_string(count));
        return videos_text(static_cast<std::size_t>(count));
    }

    // ---- drawing -------------------------------------------------------------------------

    // The ring's place on the page with the list at rest (the list's scroll is applied when it is drawn).
    Rect ring_target(State state) const
    {
        const Pane &pane = panes_[static_cast<std::size_t>(tab_)];
        if (state == State::offline)
            return tile_rect(pane.tile);
        return grown(cell_rect(pane.focus), 0.05f, 5.0f);
    }

    void draw_pane(const DrawContext &context, G &g, ui::Canvas &canvas, int index, float alpha, float dx, bool active) const
    {
        if (alpha <= 0.01f)
            return;
        const Pane &pane = panes_[static_cast<std::size_t>(index)];
        const State state = state_of(index);
        const float shown = state == State::loading ? 0.0f : tween::smoothstep(pane.reveal);
        g.l.push_opacity(alpha);
        g.l.push_transform(1.0f, 0.0f, 0.0f, dx, 0.0f);
        if (shown < 1.0f)
        {
            // Bones in the shape of the list, then the list in the same places.
            g.l.push_opacity(1.0f - shown);
            ui::Skeleton bone = bone_;
            bone.style.theme = g.k.theme;
            for (int i = 0; i < kColumns * kVisibleRows; ++i)
            {
                Enter enter(g, active ? tween::stagger(content_age_, 2 + i, 0.045f, 0.45f) : 1.0f, 26.0f);
                const Rect r = cell_rect(i);
                bone.set_bounds({r.x, r.y, r.w, r.h + kCellText});
                bone.draw(canvas);
            }
            g.l.pop_opacity();
        }
        if (state == State::ready)
            draw_cells(context, g, canvas, index, shown, active);
        else if (state != State::loading)
            draw_message(g, canvas, index, state, shown, active);
        g.l.pop_transform();
        g.l.pop_opacity();
    }

    void draw_cells(const DrawContext &context, G &g, ui::Canvas &canvas, int index, float alpha, bool active) const
    {
        const Pane &pane = panes_[static_cast<std::size_t>(index)];
        const int count = static_cast<int>(pane.cells.size());
        const int rows = (count + kColumns - 1) / kColumns;
        const float scroll = pane.scroll.value;
        const bool focused = active && zone_ == Zone::content;
        const Rect ring_at = ring_.value(); // on the page at rest
        g.l.push_opacity(alpha);
        g.l.push_clip({kRail, kClipTop, gfx::kVirtualWidth - kRail, gfx::kVirtualHeight - kClipTop});
        const int first_row = std::max(0, static_cast<int>(std::floor((scroll + kClipTop - kGridTop - kCellH - kCellText) / kPitchY)));
        const int last_row = std::min(rows - 1, static_cast<int>(std::floor((scroll + gfx::kVirtualHeight - kGridTop) / kPitchY)));
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = first_row * kColumns; i < std::min(count, (last_row + 1) * kColumns); ++i)
            {
                // The card in focus is drawn last: it stands above its neighbours.
                if ((pass == 1) != (focused && i == pane.focus))
                    continue;
                const Cell &cell = pane.cells[static_cast<std::size_t>(i)];
                const Rect at_rest = cell_rect(i);
                const Rect r = cell_rect(i, scroll);
                const float focus = focused ? tween::clamp01(1.0f - std::hypot(ring_at.cx() - at_rest.cx(), ring_at.cy() - at_rest.cy() + 5.0f) / 260.0f) : 0.0f;
                // A row that scrolled off the top fades as it goes.
                const float above = (kGridTop - r.y) / kPitchY;
                float arrive = active ? tween::stagger(content_age_, 2 + i - pane.first_row * kColumns, 0.045f, 0.45f) : 1.0f;
                if (active && rippled_from_ >= 0 && i >= rippled_from_)
                    arrive *= tween::stagger(ripple_age_, i - rippled_from_, 0.02f, 0.25f); // the cards after a removed one close the gap
                Enter enter(g, arrive, 26.0f);
                const Picture picture = context.images.get(cell.thumb, PictureSize::card);
                const Rect big = grown(r, 0.05f * focus, 5.0f * focus);
                // The card whose removal is on its way waits dimmed under a spinner.
                const bool leaving = removal_.tab == index && cell.id == removal_.row;
                // A row that has not reached the page yet stays out of the hint row's ground.
                const float below = tween::clamp01((kHintY - 36.0f - r.y) / 70.0f);
                const float card_alpha = (1.0f - tween::clamp01(above * 1.5f)) * below * (leaving ? 0.4f : 1.0f);
                if (cell.kind == Kind::playlist)
                    playlist_card(g, big, cell, picture, focus, card_alpha);
                else
                    video_card(g, big, cell.card, picture, focus, card_alpha, 2);
                if (leaving)
                {
                    ui::Spinner spinner = spinner_;
                    spinner.style.theme = g.k.theme;
                    spinner.style.color = g.k.ink;
                    spinner.set_bounds({big.cx() - 24.0f, big.cy() - 24.0f, 48.0f, 48.0f});
                    spinner.draw(canvas);
                }
            }
        }
        if (pane.feed.growing)
        {
            // More of the list is on its way: its bones stand where it will land.
            ui::Skeleton bone = bone_;
            bone.style.theme = g.k.theme;
            for (int i = count; i < (count / kColumns + 2) * kColumns && i / kColumns <= last_row + 1; ++i)
            {
                const Rect r = cell_rect(i, scroll);
                bone.set_bounds({r.x, r.y, r.w, r.h + kCellText});
                bone.draw(canvas);
            }
        }
        if (focused && pane.focus < count)
        {
            Rect ring = ring_at;
            ring.y -= scroll;
            const float nudge = g.reduced ? 0.0f : ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
            ring.x += nudge * nudge_x_;
            ring.y += nudge * nudge_y_;
            const Picture picture = context.images.get(pane.cells[static_cast<std::size_t>(pane.focus)].thumb, PictureSize::card);
            focus_ring(g, ring, kRadiusThumb, picture.ready() ? picture.glow : g.k.focus, tween::stagger(content_age_, 5, 0.05f, 0.4f));
        }
        g.l.pop_clip();
        if (rows > kVisibleRows)
        {
            // The hint row always sits on a clean ground; what peeks under it shows pictures only.
            const Color ground = gfx::mix(g.k.base, g.k.base2, 0.6f);
            g.l.gradient_rect({kRail, 944.0f, gfx::kVirtualWidth - kRail, 60.0f}, 0.0f, ground.with_alpha(0.0f), ground.with_alpha(0.95f));
            g.l.rounded_rect({kRail, 1004.0f, gfx::kVirtualWidth - kRail, 76.0f}, 0.0f, ground.with_alpha(0.95f));
            // How far down the list the page is.
            const Rect track{kRight + 40.0f, kGridTop, 6.0f, 944.0f - kGridTop - 24.0f};
            const float limit = static_cast<float>(rows - kVisibleRows) * kPitchY;
            const float thumb = std::max(48.0f, track.h * static_cast<float>(kVisibleRows) / static_cast<float>(rows));
            g.l.rounded_rect(track, 3.0f, g.k.raised);
            g.l.rounded_rect({track.x, track.y + (track.h - thumb) * tween::clamp01(scroll / limit), track.w, thumb}, 3.0f, g.k.ink3);
        }
        g.l.pop_opacity();
    }

    void draw_message(G &g, ui::Canvas &canvas, int index, State state, float alpha, bool active) const
    {
        const Message m = message_of(index, state);
        const bool tiles = state == State::offline && kTabs[index].account;
        ui::EmptyState block = active ? block_ : block_out_;
        block.style.theme = g.k.theme;
        block.title = m.title;
        block.body = m.body;
        block.action = m.action;
        block.action_button = m.button;
        const Icon which = m.icon;
        block.icon = [&g, which](ui::Canvas &, const Rect &area)
        {
            g.l.circle(area.cx(), area.cy(), area.w * 0.5f, g.k.raised);
            icon(g, which, area.cx(), area.cy(), area.w * 0.52f, g.k.ink);
        };
        block.set_bounds(tiles ? kMessageOverTiles : kMessage);
        g.l.push_opacity(alpha);
        block.draw(canvas);
        if (tiles)
            draw_tiles(g, index, active);
        g.l.pop_opacity();
    }

    // What still works is offered, not hidden behind the error.
    void draw_tiles(G &g, int index, bool active) const
    {
        const Pane &pane = panes_[static_cast<std::size_t>(index)];
        const bool focused = active && zone_ == Zone::content;
        Enter enter(g, active ? tween::stagger(content_age_, 4, 0.07f, 0.5f) : 1.0f, 26.0f);
        text(g, g.f.semibold, tr("Kept on this PS5"), kLeft, kTilesTitle, kTypeShelf, g.k.ink);
        static constexpr Icon kIcons[kTiles] = {Icon::clock, Icon::heart, Icon::queue, Icon::users};
        const Rect ring = ring_.value();
        for (int i = 0; i < kTiles; ++i)
        {
            const Rect r = tile_rect(i);
            const float focus = focused ? tween::clamp01(1.0f - std::fabs(ring.cx() - r.cx()) / 300.0f) : 0.0f;
            if (focus > 0.01f)
                lift(g, r, 24.0f, 0.7f * focus);
            glass_panel(g, r, 24.0f, 1.0f + 0.2f * focus);
            g.l.circle(r.x + 52.0f, r.cy(), 28.0f, gfx::mix(g.k.raised, g.k.plate, focus));
            icon(g, kIcons[i], r.x + 52.0f, r.cy(), 28.0f, gfx::mix(g.k.ink, g.k.on_plate, focus));
            const std::string amount = tile_count(i);
            const float width = r.w - 100.0f - 10.0f;
            text(g, g.f.semibold, fit(g.f.semibold, tile_name(i), kTypeBody, width), r.x + 100.0f, r.cy() + (amount.empty() ? 8.5f : -2.0f),
                 kTypeBody, g.k.ink);
            if (!amount.empty())
                text(g, g.f.regular, fit(g.f.regular, amount, kTypeMeta, width), r.x + 100.0f, r.cy() + 26.0f, kTypeMeta, g.k.ink3);
        }
        if (focused)
        {
            Rect at = ring;
            at.x += g.reduced ? 0.0f : ui::shake(nudge_.value, g.time, 14.0f, 8.0f) * nudge_x_;
            focus_ring(g, at, 24.0f, g.k.focus, tween::stagger(content_age_, 6, 0.07f, 0.4f));
        }
        (void)pane;
    }

    Route route_;
    model::Profile profile_;
    Data *data_ = nullptr;             // the frame's, which outlives every screen
    const ui::Fonts *fonts_ = nullptr; // ... and its fonts, for measuring the tabs
    Removal removal_;
    ui::Spinner spinner_; // on the card that is being removed
    ui::TabBar tabs_;
    ui::Skeleton bone_;
    ui::EmptyState block_;     // the message of the tab on screen
    ui::EmptyState block_out_; // ... and of the one that is leaving
    std::array<Pane, kTabCount> panes_;
    Feed follows_; // the channels followed on this PS5, for their tile
    int follows_count_ = -1;
    bool tiles_ = false; // "Kept on this PS5" is on screen
    int tab_ = 0;
    int prev_tab_ = 0;
    int swap_dir_ = 1;
    tween::Timer swap_;
    Zone zone_ = Zone::content;
    State shown_state_ = State::loading;
    ui::SpringRect ring_;
    bool ring_snap_ = true;
    ui::Pulse nudge_;
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
    float age_ = 0.0f;
    float content_age_ = 0.0f;
    int rippled_from_ = -1;
    float ripple_age_ = 10.0f;
    unsigned language_ = 0;
    std::array<ui::Hint, 6> hints_{};
    std::size_t hint_count_ = 0;
};

} // namespace

std::unique_ptr<Screen> make_library()
{
    return std::make_unique<Library>();
}

} // namespace afterglow
