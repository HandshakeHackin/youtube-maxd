// ProsperoTube Afterglow - the grid: any catalog of videos on one page.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// "See all" of a shelf, a topic of Discover, a section (Trending, Shorts,
// Kids), a playlist, the videos related to one: route.argument is the catalog,
// route.title the page's title. The catalog is read page by page as the focus
// nears the end of what has arrived. Its rows may be videos, sections inside
// the section, playlists and channels; where the provider offers filters they
// stand in a row above the grid, and a playlist has "Play all" there. When the
// provider answers with a notice instead of a list ("not available for this
// profile"), the page says so and offers a way on.

#include "afterglow/screen.hpp"

#include "ui/components/skeleton.hpp"
#include "ui/components/stat.hpp"

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
constexpr float kCellText = 96.0f;    // the title's two lines and the line under them
constexpr float kChannelExtra = 82.0f; // a channel's card is taller than a picture
constexpr float kTopPlain = 220.0f;    // the first row, and ...
constexpr float kTopUnderBar = 280.0f; // ... where it starts under a row of filters
constexpr float kBarCy = 206.0f;
constexpr float kBarButtonH = 52.0f;
constexpr float kBarChipH = 44.0f;
constexpr Rect kMessage{kLeft + 400.0f, 280.0f, 848.0f, 460.0f};
constexpr int kPageLimit = 80; // pages of one catalog read at most
// REQUEST: theme.hpp has no size for the line under a page title, nor the sizes
// of the message block the approved empty and offline boards use.
constexpr float kTypeSub = 22.0f;
constexpr float kTypeCard = 23.0f;
constexpr float kTypeMessage = 36.0f;

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

// "24" -> "24 videos"; a count the provider already worded stays as it is.
std::string videos_text(const std::string &count)
{
    if (!all_digits(count))
        return count;
    return count == "1" ? std::string(tr("1 video")) : fill(tr("{0} videos"), count);
}

// A list that plays through in its order: a playlist, Watch later, the queue.
bool plays_in_order(std::string_view catalog)
{
    return starts_with(catalog, "library:playlist:") || starts_with(catalog, "playlist:") || catalog == "library:watch_later" ||
           catalog == "queue";
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
    folder,  // a section inside the section
    channel,
};

struct Cell
{
    Kind kind = Kind::video;
    std::string id;     // the row
    std::string title;
    std::string line;   // under the title: a playlist's owner, a folder's detail, a channel's subscribers
    std::string count;  // a playlist's "24 videos"
    std::string thumb;
    Icon icon = Icon::grid; // a folder's
    bool subscribed = false; // a channel's
    model::Video video;
};

// The rectangle a cell's card takes at rest: a channel's card is taller.
Rect card_rect(const Rect &cell, Kind kind)
{
    return kind == Kind::channel ? Rect{cell.x, cell.y, cell.w, cell.h + kChannelExtra} : cell;
}
float card_radius(Kind kind)
{
    return kind == Kind::channel || kind == Kind::folder ? kRadiusPanel : kRadiusThumb;
}
// How far the card in focus grows and lifts: pictures 5 %, tiles 4 %.
Rect focused_rect(const Rect &cell, Kind kind, float focus = 1.0f)
{
    const bool tile = kind == Kind::channel || kind == Kind::folder;
    return grown(card_rect(cell, kind), (tile ? 0.04f : 0.05f) * focus, (tile ? 4.0f : 5.0f) * focus);
}

// REQUEST: design.hpp has the video card only; results, the library and this
// page show playlists, sections and channels among videos.
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
        text(g, g.f.semibold, fit(g.f.semibold, cell.count, kTypeMeta, side.w - 8.0f), side.cx(), side.cy() + 32.0f, kTypeMeta,
             kWhite, gfx::Align::center);
    const float after = paragraph(g, g.f.semibold, cell.title, r.x + 2.0f, r.y + r.h + 36.0f, kTypeCard, r.w - 4.0f, 30.0f,
                                  g.k.ink.with_alpha(0.82f + 0.18f * f), 2);
    text(g, g.f.regular, fit(g.f.regular, cell.line, kTypeMeta, r.w - 4.0f), r.x + 2.0f, after, kTypeMeta, g.k.ink2);
    g.l.pop_opacity();
}

// A section inside the section: a tile that opens, with its picture when it has one.
void folder_card(G &g, const Rect &r, const Cell &cell, const Picture &picture, float focus, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    g.l.push_opacity(tween::clamp01(alpha));
    if (f > 0.01f)
        lift(g, r, kRadiusPanel, f);
    const float shown = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
    Color ink = g.k.ink;
    if (shown < 1.0f)
        glass_panel(g, r, kRadiusPanel, 1.0f + 0.2f * f);
    if (shown > 0.0f)
    {
        // Over a picture the words are white on a shade, whatever the look.
        const float dim = 0.5f + 0.12f * f;
        g.l.image(picture.texture, r, picture.cover(r.w / r.h), Color{dim, dim, dim, shown}, kRadiusPanel);
        g.l.gradient_rect(r, kRadiusPanel, kBlack.with_alpha(0.0f), kBlack.with_alpha(0.55f * shown));
        g.l.bordered_rect(r, kRadiusPanel, clear(), 1.5f, kWhite.with_alpha(0.12f * shown));
        ink = gfx::mix(ink, kWhite, shown);
    }
    icon(g, cell.icon, r.x + 46.0f, r.y + 48.0f, 36.0f, ink);
    icon(g, Icon::chevron_right, r.x + r.w - 40.0f, r.y + 48.0f, 26.0f, ink.with_alpha(0.6f + 0.4f * f));
    // The title sits at the tile's foot: two lines grow upward.
    const TextBlock block = measure_block(g.f.semibold, cell.title, kTypeShelf, r.w - 64.0f, 34.0f, 2);
    paragraph(g, g.f.semibold, cell.title, r.x + 32.0f, r.y + r.h - 30.0f - 34.0f * static_cast<float>(std::max(1, block.lines) - 1),
              kTypeShelf, r.w - 64.0f, 34.0f, ink, 2);
    if (!cell.line.empty())
        text(g, g.f.regular, fit(g.f.regular, cell.line, kTypeMeta, r.w - 4.0f), r.x + 2.0f, r.y + r.h + 36.0f, kTypeMeta, g.k.ink2);
    g.l.pop_opacity();
}

// A channel among the videos: its face, its name and how many follow it.
void channel_card(G &g, const Rect &r, const Cell &cell, const Picture &picture, float focus, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    g.l.push_opacity(tween::clamp01(alpha));
    if (f > 0.01f)
        g.l.shadow({r.x, r.y + 20.0f, r.w, r.h}, kRadiusPanel, 40.0f, kBlack.with_alpha(0.5f * f));
    glass_panel(g, r, kRadiusPanel, 1.0f + 0.2f * f);
    avatar(g, r.cx(), r.y + 86.0f, 60.0f, cell.title, cell.id, picture);
    text(g, g.f.semibold, fit(g.f.semibold, cell.title, kTypeShelf, r.w - 40.0f), r.cx(), r.y + 190.0f, kTypeShelf, g.k.ink, gfx::Align::center);
    text(g, g.f.regular, fit(g.f.regular, cell.line, kTypeMeta, r.w - 40.0f), r.cx(), r.y + 222.0f, kTypeMeta, g.k.ink2, gfx::Align::center);
    if (cell.subscribed)
    {
        // Selected is said with a mark, not with the focus.
        const char *label = tr("Subscribed");
        const float label_w = std::min(measure(g.f.semibold, label, kTypeMeta), r.w - 120.0f);
        const Rect pill{r.cx() - label_w * 0.5f - 34.0f, r.y + 244.0f, label_w + 68.0f, 44.0f};
        g.l.rounded_rect(pill, 22.0f, g.k.raised);
        g.l.bordered_rect(pill, 22.0f, clear(), 1.5f, g.k.hairline);
        icon(g, Icon::check, pill.x + 26.0f, pill.cy(), 20.0f, g.k.accent);
        text(g, g.f.semibold, fit(g.f.semibold, label, kTypeMeta, label_w + 2.0f), pill.x + 44.0f, pill.cy() + 7.0f, kTypeMeta, g.k.ink);
    }
    g.l.pop_opacity();
}

enum class State : std::uint8_t
{
    loading,
    ready,
    empty,
    notice, // the provider answered with a notice instead of the list
    failed,
    offline,
};
const char *state_name(State state)
{
    static constexpr const char *kNames[] = {"loading", "ready", "empty", "notice", "failed", "offline"};
    return kNames[static_cast<std::size_t>(state)];
}

enum class Zone : std::uint8_t
{
    bar,
    cells,
};

// One thing in the row above the grid.
struct BarItem
{
    bool play_all = false;
    std::string id;    // a filter's catalog
    std::string label;
    bool selected = false;
    float x = 0.0f;    // from the row's left end
    float width = 0.0f;
};

struct Message
{
    Icon icon = Icon::info;
    std::string title;
    std::string body;
    std::string action;
    ui::Button button = ui::Button::cross;
};

class Grid final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::grid;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        data_ = &context.services.data;
        if (restored)
            return; // back from a video, a playlist, a channel: the focus and the scroll stay
        route_ = route;
        age_ = 0.0f;
        content_age_ = 0.0f;
        open_catalog(route.argument);
        bar_.clear();
        bar_pos_.snap(0.0f);
        bar_amount_.snap(0.0f);
        bar_scroll_.position.snap(0.0f);
        bar_age_ = 0.0f;
        // The first row starts where it will be once the list is there: a playlist and the
        // recommendations are known to have a row above them.
        top_.snap(expects_bar(route.argument) ? kTopUnderBar : kTopPlain);
        zone_ = Zone::cells;
        bar_focus_ = 0;
        profile_ = context.services.data.profile();
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
        shown_state_ = State::loading;
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        const float dt = context.dt;
        Data &data = context.services.data;
        data_ = &data;
        age_ += dt;
        content_age_ += dt;
        nudge_.update(dt, 10.0f);
        const model::Profile browsing = data.profile();
        if (browsing.id != profile_.id || browsing.signed_in != profile_.signed_in)
        {
            cells_.clear(); // another profile is browsing: nothing of the one before stays while its list loads
            feed_.stamp = 0;
        }
        profile_ = browsing;
        if (language_ != context.services.platform.language_revision())
        {
            language_ = context.services.platform.language_revision();
            feed_.stamp = 0; // the row above the grid holds interface texts: build it again
        }
        if (feed_.poll(data))
            rebuild(context.services.fonts);

        const State state = state_now();
        if (state != shown_state_)
        {
            shown_state_ = state;
            block_.enter();
        }
        if (state == State::loading)
        {
            reveal_ = 0.0f;
            was_loading_ = true;
        }
        else
        {
            reveal_ = was_loading_ ? std::min(1.0f, reveal_ + dt / 0.3f) : 1.0f;
        }

        // The focus stays on something that exists; the row it is in stays on the page.
        const int count = static_cast<int>(cells_.size());
        focus_ = std::clamp(focus_, 0, std::max(0, count - 1));
        bar_focus_ = std::clamp(bar_focus_, 0, std::max(0, static_cast<int>(bar_.size()) - 1));
        const int rows = (count + kColumns - 1) / kColumns;
        const int row = focus_ / kColumns;
        // On the last row that has arrived while the next page is on its way, the list waits with that page's bones in view.
        const bool waiting = feed_.growing && rows > 0 && row == rows - 1;
        first_row_ = std::clamp(first_row_, row - (kVisibleRows - 1), row);
        first_row_ = waiting ? row : std::clamp(first_row_, 0, std::max(0, rows - kVisibleRows));
        scroll_.target = static_cast<float>(first_row_) * kPitchY;
        scroll_.update(dt, context.reduced_motion ? 60.0f : 12.0f);
        if (state == State::ready && rows - 1 - row <= 2)
            feed_.grow(); // the end of what has arrived is near: ask for the next page
        if (feed_.stuck && !told_stuck_)
        {
            told_stuck_ = true;
            context.nav.toast(feed_.stuck_error.empty() ? std::string(tr("The rest of this list could not be loaded.")) : feed_.stuck_error,
                              ToastKind::warning);
        }
        if (state != State::loading)
            top_.target = bar_.empty() ? kTopPlain : kTopUnderBar;
        top_.update(dt, context.reduced_motion ? 60.0f : 13.0f);
        if (!bar_.empty())
        {
            const BarItem &item = bar_[static_cast<std::size_t>(bar_focus_)];
            bar_scroll_.reveal(item.x, item.x + item.width, kRight - kLeft, 48.0f, bar_.back().x + bar_.back().width);
        }
        bar_scroll_.update(dt, context.reduced_motion ? 60.0f : 14.0f);

        // Pictures are asked for as soon as the list is known, a row ahead of what
        // is on the page, so that a row which scrolls in is already there.
        for (int i = std::max(0, (first_row_ - 1) * kColumns); i < std::min(count, (first_row_ + kVisibleRows + 1) * kColumns); ++i)
        {
            const Cell &cell = cells_[static_cast<std::size_t>(i)];
            (void)context.services.images.get(cell.thumb, cell.kind == Kind::channel ? PictureSize::avatar : PictureSize::card);
        }
        if (state == State::ready && count > 0)
            (void)context.services.images.get(cells_[static_cast<std::size_t>(focus_)].thumb, PictureSize::ambient);
        bone_.style.reduced_motion = context.reduced_motion;
        bone_.update(dt);
        block_.style.reduced_motion = context.reduced_motion;
        block_.update(dt);

        const Zone zone = zone_now(state);
        const Rect target = ring_target();
        if (ring_snap_ || age_ < 0.1f)
            ring_.snap(target);
        else
            ring_.target(target);
        ring_snap_ = false;
        ring_.update(dt, context.reduced_motion ? 60.0f : 20.0f);
        bar_pos_.target = static_cast<float>(bar_focus_);
        bar_pos_.update(dt, context.reduced_motion ? 60.0f : 20.0f);
        bar_amount_.target = zone == Zone::bar ? 1.0f : 0.0f;
        bar_amount_.update(dt, context.reduced_motion ? 60.0f : 18.0f);
        bar_age_ = bar_.empty() ? 0.0f : bar_age_ + dt;

        if (context.top)
        {
            act(input, context, state, zone);
            // The page takes its colour from the picture in focus.
            const bool on_cell = state_now() == State::ready && zone_now(state_now()) == Zone::cells && !cells_.empty();
            context.nav.ambient(on_cell ? std::string_view(cells_[static_cast<std::size_t>(std::clamp(focus_, 0, static_cast<int>(cells_.size()) - 1))].thumb)
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
        const State state = state_now();
        const float shown = state == State::loading ? 0.0f : tween::smoothstep(reveal_);
        if (!subtitle_.empty())
        {
            // REQUEST: the frame draws a page's title but not the line under it.
            Enter enter(g, tween::stagger(age_, 0, 0.05f, 0.5f) * shown, -14.0f);
            text(g, g.f.regular, fit(g.f.regular, subtitle_, kTypeSub, 1400.0f), kLeft, 166.0f, kTypeSub, g.k.ink2);
        }
        if (shown < 1.0f)
        {
            // Bones in the shape of the list, then the list in the same places.
            g.l.push_opacity(1.0f - shown);
            ui::Skeleton bone = bone_;
            bone.style.theme = g.k.theme;
            for (int i = 0; i < kColumns * kVisibleRows; ++i)
            {
                Enter enter(g, tween::stagger(content_age_, 2 + i, 0.045f, 0.45f), 26.0f);
                const Rect r = cell_rect(i, 0.0f);
                bone.set_bounds({r.x, r.y, r.w, r.h + kCellText});
                bone.draw(canvas);
            }
            g.l.pop_opacity();
        }
        if (state == State::ready)
            draw_cells(context, g, canvas, shown);
        else if (state != State::loading)
            draw_message(g, canvas, state, shown);
        draw_bar(g, shown);
        g.l.pop_transform();
        g.l.pop_opacity();
    }

    std::string title() const override
    {
        return route_.title;
    }

    std::span<const ui::Hint> hints() const override
    {
        return {hints_.data(), hint_count_};
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        const State state = state_now();
        if (state != State::ready && state != State::loading)
        {
            info.id = std::string("action/") + action_name(state);
            info.text = message_of(state).action;
            info.rect = kMessage;
            info.total = 1;
            return info;
        }
        if (zone_now(state) == Zone::bar)
        {
            const BarItem &item = bar_[static_cast<std::size_t>(bar_focus_)];
            info.id = "bar/" + std::to_string(bar_focus_);
            info.text = item.label;
            info.rect = bar_rect(item, bar_scroll_.position.target);
            info.index = bar_focus_;
            info.total = static_cast<int>(bar_.size());
            return info;
        }
        info.id = "grid/" + std::to_string(focus_);
        info.index = focus_;
        info.total = static_cast<int>(cells_.size());
        const Kind kind = focus_ < static_cast<int>(cells_.size()) ? cells_[static_cast<std::size_t>(focus_)].kind : Kind::video;
        info.rect = card_rect(cell_rect(focus_, scroll_.target), kind);
        if (focus_ < static_cast<int>(cells_.size()))
            info.text = cells_[static_cast<std::size_t>(focus_)].title;
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        const State state = state_now();
        if (state != State::ready)
        {
            if (state != State::loading)
                out.emplace_back(std::string("action/") + action_name(state), message_of(state).action);
            return out;
        }
        for (std::size_t i = 0; i < bar_.size(); ++i)
            out.emplace_back("bar/" + std::to_string(i), bar_[i].label);
        for (std::size_t i = 0; i < cells_.size() && i < 48; ++i)
            out.emplace_back("grid/" + std::to_string(i), cells_[i].title);
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        const State state = state_now();
        out.set("catalog", feed_.catalog);
        out.set("state", state_name(state));
        out.set("zone", zone_now(state) == Zone::bar ? "bar" : "cells");
        out.set("items", static_cast<int>(cells_.size()));
        out.set("index", focus_);
        out.set("row", first_row_);
        out.set("pages", feed_.pages);
        out.set("more", feed_.more || feed_.growing ? 1 : 0);
        out.set("bar", static_cast<int>(bar_.size()));
        out.set("subtitle", subtitle_);
        std::string chosen;
        for (const BarItem &item : bar_)
        {
            if (item.selected)
                chosen = item.label;
        }
        out.set("filter", chosen);
        std::vector<std::string> titles;
        for (std::size_t i = 0; i < cells_.size() && i < 12; ++i)
            titles.push_back(cells_[i].title);
        out.list("titles", std::move(titles));
    }

  private:
    // ---- what the page holds ---------------------------------------------------------

    static bool expects_bar(std::string_view catalog)
    {
        return plays_in_order(catalog) || starts_with(catalog, "library:home") || starts_with(catalog, "library:filter:");
    }

    // Another catalog on the same page (the first one, or a filter of it): the list starts over.
    void open_catalog(const std::string &catalog)
    {
        feed_.reset(catalog);
        cells_.clear();
        notice_.clear();
        subtitle_.clear();
        focus_ = 0;
        first_row_ = 0;
        scroll_.snap(0.0f);
        reveal_ = 0.0f;
        was_loading_ = false;
        told_stuck_ = false;
        ring_snap_ = true;
        content_age_ = 0.0f;
    }

    State state_now() const
    {
        if (!cells_.empty())
            return State::ready; // also while a newer answer is on its way
        if (feed_.ready > 0)
            return notice_.empty() ? State::empty : State::notice;
        if (feed_.load == model::Load::failed)
            return State::failed;
        if (feed_.load == model::Load::offline)
            return State::offline;
        return State::loading;
    }

    Zone zone_now(State state) const
    {
        // The row above the grid keeps the focus while the list of a filter chosen there loads.
        return zone_ == Zone::bar && !bar_.empty() && (state == State::ready || state == State::loading) ? Zone::bar : Zone::cells;
    }

    static const char *action_name(State state)
    {
        switch (state)
        {
        case State::failed:
            return "retry";
        case State::offline:
            return "library";
        case State::notice:
            return "notice";
        default:
            return "browse";
        }
    }

    // Turns the pages that have arrived into cells, the row above them and the line under the title.
    void rebuild(const ui::Fonts &fonts)
    {
        if (feed_.ready == 0)
        {
            // Asked again and not answered yet: what was there stays until the answer replaces it.
            if (feed_.load != model::Load::loading && feed_.load != model::Load::idle)
                cells_.clear();
            return;
        }
        if (data_ == nullptr)
            return;
        const std::string focused = focus_ < static_cast<int>(cells_.size()) ? cells_[static_cast<std::size_t>(focus_)].id : std::string();
        cells_.clear();
        notice_.clear();
        std::vector<BarItem> chips;
        std::string count, owner, updated, first_notice;
        int videos = 0;
        for (int number = 0; number < feed_.ready; ++number)
        {
            const Page &page = data_->catalog(feed_.catalog, number);
            if (number == 0 && !page.rows.empty() && all_digits(page.rows.front().fact("total")))
                count = page.rows.front().fact("total");
            std::size_t next_video = 0, next_channel = 0, next_topic = 0;
            for (const model::Row &row : page.rows)
            {
                const model::Video *video = next_video < page.videos.size() && page.videos[next_video].id == row.id ? &page.videos[next_video++] : nullptr;
                const model::Channel *channel = next_channel < page.channels.size() && page.channels[next_channel].row == row.id ? &page.channels[next_channel++] : nullptr;
                const model::Topic *topic = next_topic < page.topics.size() && page.topics[next_topic].id == row.id ? &page.topics[next_topic++] : nullptr;
                if (row.folder && (!row.fact("chip").empty() || starts_with(row.id, "library:filter:")))
                {
                    // A filter the provider offers for this list (the one that is on names this very catalog).
                    BarItem chip;
                    chip.id = row.id;
                    chip.label = row.title;
                    chip.selected = row.has("selected") || row.id == feed_.catalog;
                    chips.push_back(std::move(chip));
                    continue;
                }
                if (row.id == feed_.catalog)
                {
                    // The list itself, first in its own catalog: what the line under the title says.
                    if (!row.fact("count").empty())
                        count = shown_fact(row, "count");
                    owner = row.fact("owner").empty() ? row.subtitle : shown_fact(row, "owner");
                    updated = shown_fact(row, "updated");
                    continue;
                }
                if (row.id == "account:notice")
                {
                    if (first_notice.empty())
                        first_notice = row.subtitle.empty() ? row.title : row.title + ". " + row.subtitle;
                    continue;
                }
                Cell cell;
                cell.id = row.id;
                cell.title = row.title;
                cell.thumb = row.thumb;
                if (video != nullptr)
                {
                    cell.video = *video;
                    ++videos;
                }
                else if (channel != nullptr)
                {
                    cell.kind = Kind::channel;
                    cell.id = channel->id;
                    cell.title = channel->name;
                    cell.thumb = channel->avatar;
                    const std::string handle = shown_fact(row, "handle");
                    const std::string followers = row.fact("subscribers").empty() ? channel->subscribers : shown_fact(row, "subscribers");
                    cell.line = handle.empty() || followers.empty() ? handle + followers : handle + " \xC2\xB7 " + followers;
                    cell.subscribed = row.has("subscribed");
                }
                else if (row.folder && (starts_with(row.id, "library:playlist:") || starts_with(row.id, "playlist:")))
                {
                    cell.kind = Kind::playlist;
                    const std::string by = row.fact("owner").empty() ? row.subtitle : shown_fact(row, "owner");
                    cell.line = by.empty() ? std::string(tr("Playlist")) : std::string(tr("Playlist")) + " \xC2\xB7 " + by;
                    cell.count = videos_text(shown_fact(row, "count"));
                }
                else if (row.folder)
                {
                    cell.kind = Kind::folder;
                    cell.line = row.subtitle;
                    cell.icon = topic != nullptr ? icon_by_name(topic->icon) : Icon::grid;
                }
                else
                {
                    continue; // a notice or an action of the provider's own: this page has no cell for it
                }
                cells_.push_back(std::move(cell));
            }
        }
        if (cells_.empty())
            notice_ = first_notice;
        for (std::size_t i = 0; i < cells_.size(); ++i)
        {
            if (!focused.empty() && cells_[i].id == focused)
                focus_ = static_cast<int>(i);
        }

        // The row above the grid: "Play all" for a list that plays in its order, then the provider's filters.
        // A filtered list that offers no filters of its own keeps the ones it was reached from.
        if (!chips.empty() || feed_.catalog == route_.argument)
        {
            for (auto it = bar_.begin(); it != bar_.end();)
                it = it->play_all ? std::next(it) : bar_.erase(it);
            for (BarItem &chip : chips)
                bar_.push_back(std::move(chip));
        }
        else
        {
            for (BarItem &item : bar_)
                item.selected = !item.play_all && item.id == feed_.catalog;
        }
        const bool play_all = plays_in_order(feed_.catalog) && videos > 0;
        const bool has_play_all = !bar_.empty() && bar_.front().play_all;
        if (play_all && !has_play_all)
        {
            BarItem item;
            item.play_all = true;
            item.label = tr("Play all");
            bar_.insert(bar_.begin(), std::move(item));
        }
        else if (!play_all && has_play_all)
        {
            bar_.erase(bar_.begin());
        }
        // Measured with the design language's own pieces; the look does not change a width.
        gfx::DrawList scratch;
        G g{scratch, fonts, look_afterglow(), 0, 0.0f, false};
        float x = 0.0f;
        for (BarItem &item : bar_)
        {
            item.x = x;
            item.width = item.play_all ? button_width(g, item.label, true) : chip_width(g, item.label, kBarChipH);
            x += item.width + (item.play_all ? 20.0f : 12.0f);
        }

        // The line under the title: how long the list is, whose it is, when it changed.
        subtitle_.clear();
        const auto add = [this](const std::string &part)
        {
            if (!part.empty())
                subtitle_ += (subtitle_.empty() ? "" : " \xC2\xB7 ") + part;
        };
        if (!count.empty())
            add(videos_text(count));
        else if (videos > 0 && videos == static_cast<int>(cells_.size()) && !feed_.more && !feed_.growing && !feed_.stuck)
            add(videos_text(std::to_string(videos)));
        add(owner);
        add(updated);
        if (!cells_.empty())
            add(first_notice);
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

    void act(const InputFrame &input, UpdateContext &context, State state, Zone zone)
    {
        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (zone == Zone::bar)
        {
            const int last = static_cast<int>(bar_.size()) - 1;
            switch (input.nav)
            {
            case Direction::left:
                if (bar_focus_ == 0)
                    context.nav.open_rail();
                else
                    move_in_bar(context, -1);
                break;
            case Direction::right:
                if (bar_focus_ >= last)
                    refuse(input, context, 1.0f, 0.0f);
                else
                    move_in_bar(context, 1);
                break;
            case Direction::down:
                zone_ = Zone::cells;
                context.feedback.play(audio::Cue::tab, 1.03f);
                break;
            case Direction::up:
                refuse(input, context, 0.0f, -1.0f);
                break;
            case Direction::none:
                break;
            }
            if (pressed(input, Action::confirm))
                choose_in_bar(context);
            return;
        }
        if (state == State::ready)
        {
            move_in_grid(input, context);
            const Cell &cell = cells_[static_cast<std::size_t>(focus_)];
            if (pressed(input, Action::confirm))
                open(context, cell);
            else if ((pressed(input, Action::north) || pressed(input, Action::touch)) && cell.kind == Kind::video)
                // REQUEST: Touchpad opens the Actions sheet of a video; until the frame has one, the video page stands in.
                context.nav.push(Route{ScreenId::video, cell.video.id, cell.video.title}, Move::zoom, focused_rect(cell_rect(focus_, scroll_.target), cell.kind));
            return;
        }
        if (input.nav == Direction::left)
            context.nav.open_rail();
        else if (input.nav != Direction::none && state != State::loading)
            refuse(input, context, 0.0f, 0.0f);
        if (pressed(input, Action::confirm))
        {
            switch (state)
            {
            case State::empty:
                context.nav.root(Route{ScreenId::home, {}, {}});
                break;
            case State::notice:
                if (profile_.signed_in)
                    context.nav.root(Route{ScreenId::home, {}, {}});
                else
                    context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
                break;
            case State::failed:
                try_again(context);
                break;
            case State::offline:
                context.nav.root(Route{ScreenId::library, {}, {}}); // what the console keeps is there
                break;
            default:
                break;
            }
        }
        else if (pressed(input, Action::north) && state == State::offline)
        {
            try_again(context);
        }
    }

    void try_again(UpdateContext &context)
    {
        context.services.data.forget(feed_.catalog); // the next read asks again
        context.feedback.play(audio::Cue::select);
    }

    void move_in_bar(UpdateContext &context, int step)
    {
        bar_focus_ += step;
        const BarItem &item = bar_[static_cast<std::size_t>(bar_focus_)];
        context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(kLeft + item.x + item.width * 0.5f));
    }

    void choose_in_bar(UpdateContext &context)
    {
        const BarItem item = bar_[static_cast<std::size_t>(bar_focus_)];
        if (item.play_all)
        {
            // The first video of the list: the app's player goes on through it in the profile's playback order.
            for (const Cell &cell : cells_)
            {
                if (cell.kind == Kind::video)
                {
                    play(context, cell);
                    return;
                }
            }
            return;
        }
        if (item.selected)
        {
            context.feedback.play(audio::Cue::select, 1.0f, 0.0f, 0.6f);
            return;
        }
        // Another filter: the same page, the provider's catalog of that filter.
        for (BarItem &each : bar_)
            each.selected = !each.play_all && each.id == item.id;
        open_catalog(item.id);
        context.feedback.play(audio::Cue::toggle, 1.08f);
    }

    void move_in_grid(const InputFrame &input, UpdateContext &context)
    {
        const int count = static_cast<int>(cells_.size());
        const int column = focus_ % kColumns;
        int next = focus_;
        switch (input.nav)
        {
        case Direction::left:
            if (column == 0)
            {
                context.nav.open_rail();
                return;
            }
            next = focus_ - 1;
            break;
        case Direction::right:
            if (column == kColumns - 1 || focus_ + 1 >= count)
            {
                refuse(input, context, 1.0f, 0.0f);
                return;
            }
            next = focus_ + 1;
            break;
        case Direction::up:
            if (focus_ < kColumns)
            {
                if (bar_.empty())
                {
                    refuse(input, context, 0.0f, -1.0f);
                }
                else
                {
                    zone_ = Zone::bar;
                    context.feedback.play(audio::Cue::tab, 1.1f);
                }
                return;
            }
            next = focus_ - kColumns;
            break;
        case Direction::down:
            if (focus_ / kColumns >= (count - 1) / kColumns)
            {
                if (feed_.stuck && !input.nav_repeat)
                {
                    // The page after this one was refused: ask for the list again; what is here stays meanwhile.
                    told_stuck_ = false;
                    context.services.data.forget(feed_.catalog);
                }
                else if (!feed_.more && !feed_.growing)
                {
                    refuse(input, context, 0.0f, 1.0f); // the end of the list; while more is arriving the press just waits
                }
                return;
            }
            next = std::min(focus_ + kColumns, count - 1);
            break;
        case Direction::none:
            return;
        }
        focus_ = next;
        context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(cell_rect(next, 0.0f).cx()));
    }

    void play(UpdateContext &context, const Cell &cell)
    {
        if (context.services.data.play(cell.video))
        {
            context.feedback.play(audio::Cue::launch);
            context.feedback.rumble(0.6f, 0.16f);
        }
        else
        {
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
            context.feedback.rumble(0.25f, 0.05f);
        }
    }

    void open(UpdateContext &context, const Cell &cell)
    {
        switch (cell.kind)
        {
        case Kind::video:
            play(context, cell);
            break;
        case Kind::playlist:
        case Kind::folder:
            context.nav.push(Route{ScreenId::grid, cell.id, cell.title}, Move::push);
            break;
        case Kind::channel:
            context.nav.push(Route{ScreenId::channel, cell.id, cell.title}, Move::push);
            break;
        }
    }

    void write_hints()
    {
        hint_count_ = 0;
        const auto add = [this](ui::Button button, const char *label, ui::Button second = ui::Button::none)
        {
            if (hint_count_ < hints_.size())
                hints_[hint_count_++] = ui::Hint{button, label, second};
        };
        const State state = state_now();
        switch (state)
        {
        case State::ready:
            if (zone_now(state) == Zone::bar)
            {
                const BarItem &item = bar_[static_cast<std::size_t>(std::clamp(bar_focus_, 0, static_cast<int>(bar_.size()) - 1))];
                add(ui::Button::cross, item.play_all ? tr("Play all") : tr("Show"));
            }
            else if (cells_[static_cast<std::size_t>(std::clamp(focus_, 0, static_cast<int>(cells_.size()) - 1))].kind == Kind::video)
            {
                add(ui::Button::cross, tr("Play"));
                add(ui::Button::triangle, tr("Details"));
                add(ui::Button::touchpad, tr("Actions"));
            }
            else
            {
                add(ui::Button::cross, tr("Open"));
            }
            break;
        case State::empty:
            add(ui::Button::cross, tr("Browse Home"));
            break;
        case State::notice:
            add(ui::Button::cross, profile_.signed_in ? tr("Browse Home") : tr("Sign in"));
            break;
        case State::failed:
            add(ui::Button::cross, tr("Try again"));
            break;
        case State::offline:
            add(ui::Button::cross, tr("Open Library"));
            add(ui::Button::triangle, tr("Try again"));
            break;
        case State::loading:
            break;
        }
        add(ui::Button::circle, tr("Back"));
    }

    Message message_of(State state) const
    {
        Message m;
        switch (state)
        {
        case State::empty:
            m.icon = Icon::grid;
            m.title = tr("Nothing here yet");
            m.body = tr("This list has no videos at the moment. It fills as soon as YouTube or this PS5 has something for it.");
            m.action = tr("Browse Home");
            break;
        case State::notice:
            m.icon = Icon::lock;
            m.title = tr("Not available for this profile");
            m.body = notice_;
            m.action = profile_.signed_in ? tr("Browse Home") : tr("Sign in with a code");
            break;
        case State::failed:
            m.icon = Icon::alert;
            m.title = tr("This list could not be loaded");
            m.body = feed_.error.empty() ? std::string(tr("YouTube did not answer as expected.")) : feed_.error;
            m.action = tr("Try again");
            break;
        case State::offline:
            m.icon = Icon::offline;
            m.title = tr("No connection to YouTube");
            m.body = tr("The PS5 is not reaching the internet. Your history, favourites and play queue on this console are still here.");
            m.action = tr("Open Library");
            break;
        case State::loading:
        case State::ready:
            break;
        }
        return m;
    }

    // ---- drawing -------------------------------------------------------------------------

    Rect cell_rect(int index, float scroll) const
    {
        return {kLeft + static_cast<float>(index % kColumns) * kPitchX,
                top_.value + static_cast<float>(index / kColumns) * kPitchY - scroll, kCellW, kCellH};
    }
    // The same cell with the first row where it will settle.
    Rect cell_at_rest(int index) const
    {
        return {kLeft + static_cast<float>(index % kColumns) * kPitchX, static_cast<float>(index / kColumns) * kPitchY, kCellW, kCellH};
    }

    Rect bar_rect(const BarItem &item, float scroll) const
    {
        const float height = item.play_all ? kBarButtonH : kBarChipH;
        return {kLeft + item.x - scroll, kBarCy - height * 0.5f, item.width, height};
    }

    // The ring's place among the cells, kept without the first row's offset and
    // without the scroll: both are applied where it is drawn.
    Rect ring_target() const
    {
        const Kind kind = focus_ < static_cast<int>(cells_.size()) ? cells_[static_cast<std::size_t>(focus_)].kind : Kind::video;
        return focused_rect(cell_at_rest(focus_), kind);
    }

    void draw_cells(const DrawContext &context, G &g, ui::Canvas &canvas, float alpha) const
    {
        const int count = static_cast<int>(cells_.size());
        const int rows = (count + kColumns - 1) / kColumns;
        const float scroll = scroll_.value;
        const float top = top_.value;
        const bool focused = zone_now(State::ready) == Zone::cells;
        const float on_cells = 1.0f - tween::clamp01(bar_amount_.value);
        const Rect ring_at = ring_.value();
        const float clip_top = top - 50.0f;
        g.l.push_opacity(alpha);
        g.l.push_clip({kRail, clip_top, gfx::kVirtualWidth - kRail, gfx::kVirtualHeight - clip_top});
        const int first_row = std::max(0, static_cast<int>(std::floor((scroll + clip_top - top - kCellH - kCellText) / kPitchY)));
        const int last_row = std::min(rows - 1, static_cast<int>(std::floor((scroll + gfx::kVirtualHeight - top) / kPitchY)));
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = first_row * kColumns; i < std::min(count, (last_row + 1) * kColumns); ++i)
            {
                // The card in focus is drawn last: it stands above its neighbours.
                if ((pass == 1) != (focused && i == focus_))
                    continue;
                const Cell &cell = cells_[static_cast<std::size_t>(i)];
                const Rect at_rest = card_rect(cell_at_rest(i), cell.kind);
                const Rect r = cell_rect(i, scroll);
                const float reach = cell.kind == Kind::channel || cell.kind == Kind::folder ? 220.0f : 260.0f;
                const float focus = on_cells * tween::clamp01(1.0f - std::hypot(ring_at.cx() - at_rest.cx(), ring_at.cy() - at_rest.cy() + 5.0f) / reach);
                Enter enter(g, tween::stagger(content_age_, 2 + i - first_row_ * kColumns, 0.045f, 0.45f), 26.0f);
                // A row that scrolled off the top fades as it goes; one that has not reached the page stays out of the hint row.
                const float above = (top - r.y) / kPitchY;
                const float below = tween::clamp01((kHintY - 36.0f - r.y) / 70.0f);
                const float card_alpha = (1.0f - tween::clamp01(above * 1.5f)) * below;
                const Picture picture = context.images.get(cell.thumb, cell.kind == Kind::channel ? PictureSize::avatar : PictureSize::card);
                const Rect big = focused_rect(r, cell.kind, focus);
                switch (cell.kind)
                {
                case Kind::video:
                    video_card(g, big, cell.video, picture, focus, card_alpha, 2);
                    break;
                case Kind::playlist:
                    playlist_card(g, big, cell, picture, focus, card_alpha);
                    break;
                case Kind::folder:
                    folder_card(g, big, cell, picture, focus, card_alpha);
                    break;
                case Kind::channel:
                    channel_card(g, big, cell, picture, focus, card_alpha);
                    break;
                }
            }
        }
        if (feed_.growing)
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
        if (on_cells > 0.01f && focus_ < count)
        {
            const Cell &cell = cells_[static_cast<std::size_t>(focus_)];
            Rect ring = ring_at;
            ring.y += top - scroll;
            const float nudge = g.reduced ? 0.0f : ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
            ring.x += nudge * nudge_x_;
            ring.y += nudge * nudge_y_;
            const Picture picture = context.images.get(cell.thumb, cell.kind == Kind::channel ? PictureSize::avatar : PictureSize::card);
            focus_ring(g, ring, card_radius(cell.kind), picture.ready() ? picture.glow : g.k.focus, tween::stagger(content_age_, 5, 0.05f, 0.4f) * on_cells);
        }
        g.l.pop_clip();
        if (rows > kVisibleRows)
        {
            // The hint row always sits on a clean ground; what peeks under it shows pictures only.
            const Color ground = gfx::mix(g.k.base, g.k.base2, 0.6f);
            g.l.gradient_rect({kRail, 944.0f, gfx::kVirtualWidth - kRail, 60.0f}, 0.0f, ground.with_alpha(0.0f), ground.with_alpha(0.95f));
            g.l.rounded_rect({kRail, 1004.0f, gfx::kVirtualWidth - kRail, 76.0f}, 0.0f, ground.with_alpha(0.95f));
            // How far down the list the page is.
            const Rect track{kRight + 40.0f, top, 6.0f, 944.0f - top - 24.0f};
            const float limit = static_cast<float>(rows - kVisibleRows) * kPitchY;
            const float thumb = std::max(48.0f, track.h * static_cast<float>(kVisibleRows) / static_cast<float>(rows));
            g.l.rounded_rect(track, 3.0f, g.k.raised);
            g.l.rounded_rect({track.x, track.y + (track.h - thumb) * tween::clamp01(scroll / limit), track.w, thumb}, 3.0f, g.k.ink3);
        }
        g.l.pop_opacity();
    }

    // "Play all" and the filters the provider offers for this list.
    void draw_bar(G &g, float alpha) const
    {
        if (bar_.empty())
            return;
        const float scroll = bar_scroll_.offset();
        // The row stays while a filter's list loads; it arrives once, with the first list.
        Enter enter(g, tween::cubic_out(tween::clamp01(bar_age_ / 0.4f)), 16.0f);
        g.l.push_clip({kLeft - 14.0f, kBarCy - 46.0f, kRight - kLeft + 28.0f, 92.0f});
        for (std::size_t i = 0; i < bar_.size(); ++i)
        {
            const BarItem &item = bar_[i];
            const Rect r = bar_rect(item, scroll);
            if (r.x > kRight + 20.0f || r.x + r.w < kLeft - 20.0f)
                continue;
            // One highlight glides along the row: an item reads how near it is.
            const float focus = tween::clamp01(1.0f - std::fabs(bar_pos_.value - static_cast<float>(i))) * tween::clamp01(bar_amount_.value);
            if (item.play_all)
            {
                button(g, r, item.label, Icon::play, focus, true);
                if (focus > 0.01f) // a filled button says "focused" with the ring, not with its colour
                    g.l.bordered_rect(r.inset(-6.0f), r.h * 0.5f + 6.0f, clear(), g.k.theme.focus_width, g.k.focus.with_alpha(focus));
            }
            else
            {
                (void)chip(g, r.x, kBarCy, item.label, item.selected, focus, kBarChipH);
            }
        }
        g.l.pop_clip();
        (void)alpha;
    }

    void draw_message(G &g, ui::Canvas &canvas, State state, float alpha) const
    {
        const Message m = message_of(state);
        ui::EmptyState block = block_;
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
        block.set_bounds(kMessage);
        g.l.push_opacity(alpha);
        block.draw(canvas);
        g.l.pop_opacity();
    }

    Route route_;
    model::Profile profile_;
    Data *data_ = nullptr; // the frame's, which outlives every screen
    Feed feed_;
    std::vector<Cell> cells_;
    std::vector<BarItem> bar_;
    std::string notice_;   // the provider's words, when it answered with a notice
    std::string subtitle_; // the line under the title
    int focus_ = 0;
    int first_row_ = 0;
    int bar_focus_ = 0;
    Zone zone_ = Zone::cells;
    State shown_state_ = State::loading;
    tween::Spring scroll_;
    tween::Spring top_; // where the first row starts
    ui::Scroller bar_scroll_;
    tween::Spring bar_pos_;    // the highlight's place in the row above the grid
    tween::Spring bar_amount_; // ... and how much of the focus that row has
    float bar_age_ = 0.0f;
    unsigned language_ = 0;
    ui::SpringRect ring_;
    bool ring_snap_ = true;
    ui::Pulse nudge_;
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
    float age_ = 0.0f;
    float content_age_ = 0.0f;
    float reveal_ = 0.0f;      // how far the answer has taken over from the bones
    bool was_loading_ = false; // bones were shown: the answer cross-fades in
    bool told_stuck_ = false;  // a toast has said that the list ends early
    ui::Skeleton bone_;
    ui::EmptyState block_;
    std::array<ui::Hint, 5> hints_{};
    std::size_t hint_count_ = 0;
};

} // namespace

std::unique_ptr<Screen> make_grid()
{
    return std::make_unique<Grid>();
}

} // namespace afterglow
