// ProsperoTube Afterglow - Results: what a search found, and the filters that narrow it.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// route.argument is the query. The page is the query with the number of
// results, a row of filter chips, and a grid of four columns that mixes
// videos, channels and playlists in the order the provider lists them. The
// grid reads "search:<query>", or "search-with:<options>:<query>" once a
// filter is set (screens/search.hpp), page after page as the focus nears the
// end. A video plays or opens its page, a channel opens the channel screen, a
// playlist the grid.

#include "afterglow/screens/search.hpp"

#include "ui/components.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace afterglow
{

namespace
{

using search::fill;
using search::starts_with;
using search::trimmed;

constexpr int kColumns = 4;
constexpr float kCellW = 394.0f;
constexpr float kCellH = 222.0f;
constexpr float kCellPitchX = kCellW + 24.0f;
constexpr float kCellPitchY = 352.0f;
constexpr float kGridTop = 268.0f;
constexpr float kGridBottom = 968.0f;
constexpr float kGridClip = 230.0f;     // rows that scroll up are gone by here, under the chips
constexpr float kChannelExtra = 82.0f;  // a channel's card is taller than a picture
constexpr Rect kField{kLeft, 80.0f, 620.0f, 64.0f};
constexpr float kChipsY = 196.0f;
constexpr float kChipsLeft = kLeft + 48.0f;
constexpr float kChipHeight = 44.0f;
constexpr int kBones = 8;
constexpr int kMaxPages = 24;
constexpr Rect kStateBounds{kLeft + 400.0f, 290.0f, 848.0f, 460.0f};

enum Chip : int
{
    kChipDate,
    kChipDuration,
    kChipType,
    kChipUhd,
    kChipCaptions,
    kChipLive,
    kChipOrder,
    kChipCount,
};
constexpr const char *kChipIds[kChipCount] = {"date", "duration", "type", "4k", "captions", "live", "sort"};

bool chip_opens_list(int chip)
{
    return chip == kChipDate || chip == kChipDuration || chip == kChipType || chip == kChipOrder;
}

// The choices of the lists, in the provider's order (screens/search.hpp, Filters).
const char *date_label(int index)
{
    switch (index)
    {
    case 1:
        return tr("Today");
    case 2:
        return tr("This week");
    case 3:
        return tr("This month");
    case 4:
        return tr("This year");
    default:
        return tr("Any time");
    }
}
const char *duration_label(int index)
{
    switch (index)
    {
    case 1:
        return tr("Under 3 minutes");
    case 2:
        return tr("3 to 20 minutes");
    case 3:
        return tr("Over 20 minutes");
    default:
        return tr("Any length");
    }
}
const char *type_label(int index)
{
    switch (index)
    {
    case 1:
        return tr("Videos");
    case 2:
        return tr("Channels");
    case 3:
        return tr("Playlists");
    case 4:
        return tr("Movies");
    case 5:
        return tr("Shorts");
    default:
        return tr("All types");
    }
}
const char *order_label(int index)
{
    return index == 1 ? tr("View count") : tr("Relevance");
}
int choices_of(int chip)
{
    return chip == kChipDate ? 5 : chip == kChipDuration ? 4 : chip == kChipType ? 6 : chip == kChipOrder ? 2 : 0;
}
const char *choice_label(int chip, int index)
{
    return chip == kChipDate ? date_label(index) : chip == kChipDuration ? duration_label(index) : chip == kChipType ? type_label(index) : order_label(index);
}

Rect cell_rect(int index, float scroll)
{
    return {kLeft + static_cast<float>(index % kColumns) * kCellPitchX, kGridTop + static_cast<float>(index / kColumns) * kCellPitchY - scroll,
            kCellW, kCellH};
}

const char *load_name(model::Load load)
{
    switch (load)
    {
    case model::Load::ready:
        return "ready";
    case model::Load::failed:
        return "failed";
    case model::Load::offline:
        return "offline";
    default:
        return "loading";
    }
}

class Results final : public Screen
{
  public:
    Results()
    {
        for (ui::Skeleton &bone : bones_)
        {
            bone.style.kind = ui::SkeletonKind::card;
            bone.style.picture = 0.7f;
            bone.style.lines = 2;
            bone.style.radius = kRadiusThumb;
        }
        menu_.style.close_on_check = true;
        menu_.style.text_size = 24.0f;
        menu_.set_bounds({kLeft, 150.0f, kRight - kLeft, kGridBottom - 150.0f});
        state_.style.title_size = 36.0f;
        state_.style.body_size = 24.0f;
        state_.style.hint_size = 24.0f;
        state_.style.max_text_width = 660.0f;
        state_.set_bounds(kStateBounds);
    }

    ScreenId id() const override
    {
        return ScreenId::results;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        (void)context;
        if (restored)
            return; // back from a video, a channel or a playlist: the focus and the row stay
        route_ = route;
        query_ = trimmed(route.argument);
        filters_ = search::Filters{};
        age_ = 0.0f;
        zone_ = Zone::grid;
        chip_ = 0;
        menu_.dismiss();
        list_again();
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        Data &data = context.services.data;
        age_ += context.dt;
        ready_age_ += context.dt;
        localise(context.reduced_motion);
        lay_out_chips(context.services.fonts);
        read(data);
        settle();
        if (context.top)
            handle(input, context);

        // The next page is asked for while the focus is still a row away from the end.
        const int count = static_cast<int>(cells_.size());
        const int row = cell_ / kColumns;
        const int last_row = count > 0 ? (count - 1) / kColumns : 0;
        if (count > 0 && more_ && ready_pages_ == pages_ && pages_ < kMaxPages && row >= last_row - 1)
            ++pages_;
        // Two rows are in view; the grid moves by whole rows and as little as it can.
        if (row < top_row_)
            top_row_ = row;
        else if (row > top_row_ + 1)
            top_row_ = row - 1;
        if (more_loading_ && row == last_row && top_row_ < row)
            top_row_ = row; // the page on its way shows as bones under the last row
        if (count == 0)
            top_row_ = 0;
        scroll_.target = static_cast<float>(top_row_) * kCellPitchY;
        scroll_.update(context.dt, context.reduced_motion ? 60.0f : 12.0f);

        const float quick = context.reduced_motion ? 60.0f : 20.0f;
        const Rect target = grown(cell_rect(cell_, scroll_.value), 0.05f, 5.0f);
        if (snap_ring_)
            ring_.snap(target);
        else
            ring_.target(target);
        snap_ring_ = false;
        ring_.update(context.dt, quick);
        ring_amount_.target = zone_ == Zone::grid && count > 0 ? 1.0f : 0.0f;
        ring_amount_.update(context.dt, quick);
        nudge_.update(context.dt, 9.0f);
        press_.update(context.dt, 9.0f);
        for (int i = 0; i < kChipCount; ++i)
        {
            chip_focus_[static_cast<std::size_t>(i)].target = zone_ == Zone::chips && chip_ == i && !menu_.is_open() ? 1.0f : 0.0f;
            chip_focus_[static_cast<std::size_t>(i)].update(context.dt, quick);
        }
        // A row of chips that a language makes longer than the page follows its focus.
        const Rect &chip = chips_[static_cast<std::size_t>(chip_)];
        chips_scroll_.reveal(chip.x - kChipsLeft, chip.x + chip.w - kChipsLeft, kRight - kChipsLeft, 24.0f, chips_width_);
        chips_scroll_.update(context.dt, context.reduced_motion ? 60.0f : 14.0f);
        menu_.update(context.dt);
        state_.update(context.dt);
        for (ui::Skeleton &bone : bones_)
            bone.update(context.dt);

        // Pictures of the rows in view and of the one below are asked for here, a
        // frame before they are drawn; the page takes the colour of the one in focus.
        for (int i = top_row_ * kColumns; i < count && i < (top_row_ + 3) * kColumns; ++i)
        {
            const Cell &cell = cells_[static_cast<std::size_t>(i)];
            context.services.images.get(cell.thumb, cell.kind == Kind::channel ? PictureSize::avatar : PictureSize::card);
        }
        if (context.top && count > 0)
        {
            const Cell &lit = cells_[static_cast<std::size_t>(zone_ == Zone::grid ? cell_ : first_picture_)];
            if (lit.kind != Kind::channel && !lit.thumb.empty())
            {
                context.services.images.get(lit.thumb, PictureSize::ambient);
                context.nav.ambient(lit.thumb);
            }
        }
        set_hints();
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        search::Handover hand(g, phase);
        wear(context.look);
        ui::Canvas canvas{g.l, context.fonts, context.glass, context.time};
        draw_header(g);
        draw_chips(g);
        draw_grid(context, g, canvas);
        draw_state(g, canvas);
        menu_.draw(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        return {hints_.data(), hint_count_};
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        if (menu_.is_open())
        {
            info.id = "menu/" + std::to_string(menu_.focus());
            info.text = menu_.items()[static_cast<std::size_t>(menu_.focus())].label;
            info.rect = menu_.panel_rect();
            info.index = menu_.focus();
            info.total = static_cast<int>(menu_.items().size());
        }
        else if (zone_ == Zone::chips)
        {
            info.id = std::string("chip/") + kChipIds[chip_];
            info.text = chip_label(chip_);
            info.rect = chips_[static_cast<std::size_t>(chip_)];
            info.rect.x -= chips_scroll_.position.target;
            info.index = chip_;
            info.total = kChipCount;
        }
        else if (zone_ == Zone::state)
        {
            info.id = std::string("action/") + state_action_id();
            info.text = state_.action;
            info.rect = kStateBounds;
            info.total = 1;
        }
        else if (cells_.empty())
        {
            info.id = "grid/loading";
            info.rect = cell_rect(0, 0.0f);
        }
        else
        {
            const Cell &cell = cells_[static_cast<std::size_t>(cell_)];
            info.id = "cell/" + std::to_string(cell_);
            info.text = cell.title;
            info.rect = grown(cell_rect(cell_, static_cast<float>(top_row_) * kCellPitchY), 0.05f, 5.0f);
            if (cell.kind == Kind::channel)
                info.rect.h += kChannelExtra;
            info.index = cell_;
            info.total = static_cast<int>(cells_.size());
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        if (menu_.is_open())
        {
            for (std::size_t i = 0; i < menu_.items().size(); ++i)
                out.emplace_back("menu/" + std::to_string(i), menu_.items()[i].label);
            return out;
        }
        for (int i = 0; i < kChipCount; ++i)
            out.emplace_back(std::string("chip/") + kChipIds[i], chip_label(i));
        if (state_kind() != State::none)
            out.emplace_back(std::string("action/") + state_action_id(), state_.action);
        for (std::size_t i = 0; i < cells_.size(); ++i)
            out.emplace_back("cell/" + std::to_string(i), cells_[i].title);
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("query", query_);
        out.set("catalog", catalog_);
        out.set("load", load_name(load_));
        out.set("zone", menu_.is_open() ? "menu" : zone_ == Zone::chips ? "chips" : zone_ == Zone::grid ? "grid" : "state");
        out.set("filters", filters_.any() ? filters_.options() : std::string("none"));
        out.set("cells", static_cast<int>(cells_.size()));
        out.set("pages", ready_pages_);
        out.set("more", more_ ? 1 : 0);
        out.set("row", top_row_);
        out.set("count", count_);
        std::vector<std::string> cells;
        for (const Cell &cell : cells_)
            cells.push_back(std::string(cell.kind == Kind::video ? "video:" : cell.kind == Kind::channel ? "channel:" : "playlist:") + cell.title);
        out.list("cells", std::move(cells));
    }

  private:
    enum class Zone : std::uint8_t
    {
        chips,
        grid,
        state, // nothing to list: the page says why and offers one way on
    };
    enum class Kind : std::uint8_t
    {
        video,
        channel,
        playlist,
    };
    enum class State : std::uint8_t
    {
        none,
        empty,
        failed,
        offline,
    };

    struct Cell
    {
        Kind kind = Kind::video;
        std::string id;    // the provider's row
        std::string title; // a video's title, a channel's name, a playlist's title
        std::string thumb; // the picture: a channel's is its avatar
        model::Video video;
        std::string channel_id;  // channel: "UC..."
        std::string handle;      // channel
        std::string subscribers; // channel
        bool subscribed = false; // channel
        std::string owner;       // playlist
        std::string count;       // playlist: how many videos
    };

    // ---- the kit's components in the look and the language -----------------------

    // REQUEST: as in the search screen, the look reaches the kit's components
    // only when they are drawn (requests/A.md, "The look in update").
    void wear(const Look &look) const
    {
        menu_.style.theme = look.theme;
        state_.style.theme = look.theme;
        for (ui::Skeleton &bone : bones_)
            bone.style.theme = look.theme;
    }

    void localise(bool reduced)
    {
        menu_.style.reduced_motion = reduced;
        state_.style.reduced_motion = reduced;
        for (ui::Skeleton &bone : bones_)
            bone.style.reduced_motion = reduced;
    }

    // ---- filters -------------------------------------------------------------------

    std::string chip_label(int chip) const
    {
        switch (chip)
        {
        case kChipDate:
            return date_label(filters_.date);
        case kChipDuration:
            return duration_label(filters_.duration);
        case kChipType:
            return type_label(filters_.type);
        case kChipUhd:
            return "4K";
        case kChipCaptions:
            return tr("Captions");
        case kChipLive:
            return tr("Live");
        default:
            return filters_.order == 1 ? tr("Sort: view count") : tr("Sort: relevance");
        }
    }

    bool chip_selected(int chip) const
    {
        switch (chip)
        {
        case kChipDate:
            return filters_.date != 0;
        case kChipDuration:
            return filters_.duration != 0;
        case kChipType:
            return filters_.type != 0;
        case kChipUhd:
            return filters_.uhd;
        case kChipCaptions:
            return filters_.captions;
        case kChipLive:
            return filters_.live;
        default:
            return filters_.order != 0;
        }
    }

    int chip_choice(int chip) const
    {
        return chip == kChipDate ? filters_.date : chip == kChipDuration ? filters_.duration : chip == kChipType ? filters_.type : filters_.order;
    }

    // Chips are as wide as their labels: measured here, with the design
    // language's own arithmetic, so the focus knows where each one is.
    void lay_out_chips(const ui::Fonts &fonts)
    {
        gfx::DrawList nothing;
        G g{nothing, fonts, look_afterglow(), 0, 0.0f, false};
        float x = kChipsLeft;
        for (int i = 0; i < kChipCount; ++i)
        {
            const float width = chip_width(g, chip_label(i), kChipHeight, chip_opens_list(i));
            chips_[static_cast<std::size_t>(i)] = {x, kChipsY - kChipHeight * 0.5f, width, kChipHeight};
            x += width + 12.0f;
        }
        chips_width_ = x - 12.0f - kChipsLeft;
        menu_width_ = 340.0f;
        if (menu_.is_open())
        {
            for (const ui::MenuItem &item : menu_.items())
                menu_width_ = std::max(menu_width_, measure(fonts.semibold, item.label, menu_.style.text_size) + 104.0f);
            menu_.style.width = std::min(menu_width_, 620.0f);
        }
    }

    // Everything the list holds is forgotten: another catalog is about to be read.
    void list_again()
    {
        catalog_ = search::results_catalog(query_, filters_);
        pages_ = 1;
        ready_pages_ = 0;
        built_.clear();
        cells_.clear();
        count_.clear();
        more_ = false;
        more_loading_ = false;
        placed_ = false;
        cell_ = 0;
        first_picture_ = 0;
        top_row_ = 0;
        load_ = model::Load::loading;
    }

    void open_menu(int chip, UpdateContext &context)
    {
        std::vector<ui::MenuItem> items;
        for (int i = 0; i < choices_of(chip); ++i)
        {
            ui::MenuItem item;
            item.label = choice_label(chip, i);
            item.checkable = true;
            item.checked = i == chip_choice(chip);
            items.push_back(std::move(item));
        }
        menu_.set_items(std::move(items));
        menu_chip_ = chip;
        Rect anchor = chips_[static_cast<std::size_t>(chip)];
        anchor.x -= chips_scroll_.position.target;
        menu_.open(anchor, context.feedback);
        menu_.set_focus(chip_choice(chip));
    }

    void choose(int chip, int choice)
    {
        search::Filters next = filters_;
        (chip == kChipDate ? next.date : chip == kChipDuration ? next.duration : chip == kChipType ? next.type : next.order) = choice;
        apply(next);
    }

    void apply(const search::Filters &next)
    {
        if (next.options() == filters_.options())
            return;
        filters_ = next;
        list_again(); // the grid goes back to its bones; the focus stays on the chip
    }

    // ---- data --------------------------------------------------------------------

    void add_cells(const Page &page)
    {
        // The rows in the provider's order, each with what the data layer made of
        // it (data_rows.cpp, add_row: a channel, a folder, a notice, else a video).
        std::size_t video = 0;
        std::size_t channel = 0;
        for (const model::Row &row : page.rows)
        {
            Cell cell;
            cell.id = row.id;
            cell.title = row.title;
            cell.thumb = row.thumb;
            if (starts_with(row.id, "channel:"))
            {
                if (channel >= page.channels.size())
                    continue;
                const model::Channel &found = page.channels[channel++];
                cell.kind = Kind::channel;
                cell.channel_id = found.id;
                // REQUEST: the data layer fills a channel's handle, subscribers and
                // subscribed from the row's facts (requests/A.md); read here until it does.
                cell.handle = found.handle.empty() ? row.fact("handle") : found.handle;
                cell.subscribers = found.subscribers.empty() ? row.fact("subscribers") : found.subscribers;
                cell.subscribed = found.subscribed || row.has("subscribed");
                cells_.push_back(std::move(cell));
            }
            else if (row.folder)
            {
                if (!starts_with(row.id, "playlist:") && !starts_with(row.id, "library:playlist:"))
                    continue; // the provider's own rows in front of a typed search, a notice
                cell.kind = Kind::playlist;
                cell.owner = row.fact("owner").empty() ? row.subtitle : row.fact("owner");
                cell.count = row.fact("count");
                cells_.push_back(std::move(cell));
            }
            else if (!row.interface_text)
            {
                if (video >= page.videos.size())
                    continue;
                cell.kind = Kind::video;
                cell.video = page.videos[video++];
                cells_.push_back(std::move(cell));
            }
        }
    }

    void read(Data &data)
    {
        const Page &first = data.catalog(catalog_, 0);
        load_ = first.load == model::Load::idle ? model::Load::loading : first.load;
        error_ = first.error;
        if (load_ != model::Load::ready)
        {
            if (!cells_.empty() || ready_pages_ != 0)
            {
                const search::Filters kept = filters_;
                list_again(); // the data layer forgot the list (another profile, a retry)
                filters_ = kept;
                load_ = first.load == model::Load::idle ? model::Load::loading : first.load;
            }
            return;
        }
        int ready = 1;
        std::size_t rows = first.rows.size();
        bool more = first.more;
        more_loading_ = false;
        for (int page = 1; page < pages_; ++page)
        {
            const Page &next = data.catalog(catalog_, page);
            if (next.load != model::Load::ready)
            {
                // A later page that fails ends the list where it is; the rows above stay.
                more_loading_ = next.load == model::Load::loading || next.load == model::Load::idle;
                more = more_loading_;
                break;
            }
            ready = page + 1;
            rows += next.rows.size();
            more = next.more;
        }
        more_ = more;
        const std::string built = std::to_string(ready) + ':' + std::to_string(rows);
        if (built == built_)
            return;
        // Rows arrived: the list is put together again and the focus stays on its row.
        const std::string held = static_cast<std::size_t>(cell_) < cells_.size() ? cells_[static_cast<std::size_t>(cell_)].id : std::string();
        const bool was_empty = cells_.empty();
        built_ = built;
        ready_pages_ = ready;
        cells_.clear();
        count_.clear();
        for (int page = 0; page < ready; ++page)
            add_cells(data.catalog(catalog_, page));
        for (const model::Row &row : first.rows)
        {
            if (!row.fact("results").empty())
            {
                count_ = row.fact("results");
                break;
            }
        }
        first_picture_ = 0;
        for (std::size_t i = 0; i < cells_.size(); ++i)
        {
            if (cells_[i].kind != Kind::channel && !cells_[i].thumb.empty())
            {
                first_picture_ = static_cast<int>(i);
                break;
            }
        }
        if (was_empty)
            ready_age_ = 0.0f;
        for (std::size_t i = 0; !held.empty() && i < cells_.size(); ++i)
        {
            if (cells_[i].id == held)
                cell_ = static_cast<int>(i);
        }
    }

    State state_kind() const
    {
        if (load_ == model::Load::offline)
            return State::offline;
        if (load_ == model::Load::failed)
            return State::failed;
        return load_ == model::Load::ready && cells_.empty() ? State::empty : State::none;
    }

    const char *state_action_id() const
    {
        switch (state_kind())
        {
        case State::empty:
            return filters_.any() ? "clear-filters" : "search-again";
        default:
            return "retry";
        }
    }

    // The focus is always on something that exists.
    void settle()
    {
        const int count = static_cast<int>(cells_.size());
        const State state = state_kind();
        if (zone_ == Zone::grid && state != State::none)
            zone_ = Zone::state;
        else if (zone_ == Zone::state && state == State::none)
            zone_ = Zone::grid;
        if (count > 0 && !placed_)
        {
            // A search lands on its first video where the first row has one: Cross plays it.
            cell_ = 0;
            for (int i = 0; i < count && i < kColumns; ++i)
            {
                if (cells_[static_cast<std::size_t>(i)].kind == Kind::video)
                {
                    cell_ = i;
                    break;
                }
            }
            placed_ = true;
            snap_ring_ = true;
        }
        cell_ = std::clamp(cell_, 0, std::max(count - 1, 0));

        if (state != shown_state_)
        {
            shown_state_ = state;
            state_.enter();
        }
        // The words of the state: what is missing and the one way on.
        state_.title.clear();
        state_.body.clear();
        state_.action.clear();
        if (state == State::empty)
        {
            state_.body = filters_.any() ? tr("Some filters are set. Clear them to look through everything.")
                                         : tr("Check the spelling, or try fewer or other words.");
            state_.action = filters_.any() ? tr("Clear filters") : tr("Search again");
        }
        else if (state == State::failed)
        {
            state_.title = tr("This search did not come through");
            state_.body = error_.empty() ? std::string(tr("YouTube did not answer. Try again in a moment.")) : error_;
            state_.action = tr("Try again");
        }
        else if (state == State::offline)
        {
            state_.title = tr("No connection to YouTube");
            state_.body = tr("The PS5 is not reaching the internet. Your history, favourites and play queue on this console are still here.");
            state_.action = tr("Try again");
        }
    }

    // ---- input -------------------------------------------------------------------

    void refuse(UpdateContext &context, const InputFrame &input, bool shake = false)
    {
        if (input.nav_repeat)
            return; // a held direction stays silent at an edge
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        if (shake)
            nudge_.trigger();
    }

    void search_again(UpdateContext &context)
    {
        // REQUEST: with a way to know that the search screen is the one below,
        // this would be nav.back() and keep that screen as it was (requests/A.md).
        context.nav.root(Route{ScreenId::search, query_, {}});
    }

    void open_cell(const Cell &cell, UpdateContext &context, bool details)
    {
        const Rect from = grown(cell_rect(cell_, scroll_.value), 0.05f, 5.0f);
        if (cell.kind == Kind::channel)
            context.nav.push(Route{ScreenId::channel, cell.channel_id, cell.title}, Move::push);
        else if (cell.kind == Kind::playlist)
            context.nav.push(Route{ScreenId::grid, cell.id, cell.title}, Move::push);
        else if (details)
            context.nav.push(Route{ScreenId::video, cell.video.id, {}}, Move::zoom, from);
        else if (context.services.data.play(cell.video))
            context.feedback.play(audio::Cue::launch);
        else
            context.nav.toast(tr("This video cannot be played right now."), ToastKind::error);
    }

    void handle(const InputFrame &input, UpdateContext &context)
    {
        if (menu_.is_open())
        {
            // The list of a chip has the input until a choice or Circle closes it.
            if (menu_.handle(input, context.feedback) == ui::Event::changed)
                choose(menu_chip_, menu_.focus());
            return;
        }
        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (pressed(input, Action::west))
        {
            search_again(context);
            return;
        }

        if (zone_ == Zone::chips)
        {
            if (input.nav == Direction::left && chip_ == 0)
                context.nav.open_rail();
            else if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int next = chip_ + (input.nav == Direction::right ? 1 : -1);
                if (next < kChipCount)
                {
                    chip_ = next;
                    context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(chips_[static_cast<std::size_t>(next)].cx()));
                }
                else
                {
                    refuse(context, input);
                }
            }
            else if (input.nav == Direction::down)
            {
                if (state_kind() != State::none || !cells_.empty())
                {
                    zone_ = state_kind() != State::none ? Zone::state : Zone::grid;
                    snap_ring_ = true;
                    context.feedback.play(audio::Cue::tab, 1.0f);
                }
                else
                {
                    refuse(context, input); // the list is still on its way
                }
            }
            else if (input.nav == Direction::up)
            {
                refuse(context, input);
            }
            else if (pressed(input, Action::confirm))
            {
                press_.trigger();
                if (chip_opens_list(chip_))
                {
                    open_menu(chip_, context);
                }
                else
                {
                    search::Filters next = filters_;
                    bool &flag = chip_ == kChipUhd ? next.uhd : chip_ == kChipCaptions ? next.captions : next.live;
                    flag = !flag;
                    context.feedback.play(audio::Cue::toggle, flag ? 1.08f : 0.94f, ui::pan_for_x(chips_[static_cast<std::size_t>(chip_)].cx()));
                    apply(next);
                }
            }
            return;
        }

        if (zone_ == Zone::state)
        {
            const State state = state_kind();
            if (input.nav == Direction::up)
            {
                zone_ = Zone::chips;
                context.feedback.play(audio::Cue::tab, 1.08f);
            }
            else if (input.nav == Direction::left)
                context.nav.open_rail();
            else if (input.nav != Direction::none)
                refuse(context, input);
            else if (pressed(input, Action::confirm))
            {
                context.feedback.play(audio::Cue::select);
                if (state == State::empty && filters_.any())
                {
                    apply(search::Filters{});
                    zone_ = Zone::grid;
                }
                else if (state == State::empty)
                {
                    search_again(context);
                }
                else
                {
                    // The data layer keeps a failed answer: forgetting it asks again.
                    context.services.data.forget(catalog_);
                    const search::Filters kept = filters_;
                    list_again();
                    filters_ = kept;
                    zone_ = Zone::grid;
                }
            }
            else if (state == State::offline && pressed(input, Action::north))
            {
                context.nav.root(Route{ScreenId::library, {}, {}}); // what is kept on the console
            }
            return;
        }

        const int count = static_cast<int>(cells_.size());
        if (count == 0)
        {
            // Bones: the filters are within reach, nothing else is.
            if (input.nav == Direction::up)
            {
                zone_ = Zone::chips;
                context.feedback.play(audio::Cue::tab, 1.08f);
            }
            else if (input.nav == Direction::left)
                context.nav.open_rail();
            else if (input.nav != Direction::none || pressed(input, Action::confirm))
                refuse(context, input);
            return;
        }
        const int column = cell_ % kColumns;
        const int row = cell_ / kColumns;
        const int last_row = (count - 1) / kColumns;
        int next = cell_;
        switch (input.nav)
        {
        case Direction::left:
            if (column == 0)
            {
                context.nav.open_rail();
                return;
            }
            next = cell_ - 1;
            break;
        case Direction::right:
            next = column < kColumns - 1 && cell_ + 1 < count ? cell_ + 1 : -1;
            break;
        case Direction::up:
            if (row == 0)
            {
                zone_ = Zone::chips;
                context.feedback.play(audio::Cue::tab, 1.08f);
                return;
            }
            next = cell_ - kColumns;
            break;
        case Direction::down:
            // A last row that is shorter takes the focus on its last card.
            next = row < last_row ? std::min(cell_ + kColumns, count - 1) : -1;
            break;
        case Direction::none:
            break;
        }
        if (next < 0)
        {
            refuse(context, input, true);
        }
        else if (next != cell_)
        {
            cell_ = next;
            context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(cell_rect(next, 0.0f).cx()));
        }
        const Cell &cell = cells_[static_cast<std::size_t>(cell_)];
        if (pressed(input, Action::confirm))
            open_cell(cell, context, false);
        else if (pressed(input, Action::north) && cell.kind == Kind::video)
            open_cell(cell, context, true);
        else if (pressed(input, Action::touch) && cell.kind == Kind::video)
        {
            // REQUEST: the actions sheet of a video is one piece for every screen
            // that lists videos (requests/A.md). Until it exists Touchpad opens
            // the video page, where the actions are.
            open_cell(cell, context, true);
        }
    }

    void set_hints()
    {
        hint_count_ = 0;
        const auto add = [&](ui::Button button, const char *label)
        {
            if (hint_count_ < hints_.size())
                hints_[hint_count_++] = {button, label};
        };
        if (menu_.is_open())
        {
            add(ui::Button::cross, tr("Choose"));
            add(ui::Button::circle, tr("Close"));
            return;
        }
        if (zone_ == Zone::chips)
        {
            add(ui::Button::cross, chip_opens_list(chip_) ? tr("Choose") : chip_selected(chip_) ? tr("Turn off") : tr("Turn on"));
            add(ui::Button::square, tr("Search again"));
            add(ui::Button::circle, tr("Back"));
        }
        else if (zone_ == Zone::state)
        {
            const State state = state_kind();
            add(ui::Button::cross, state_.action.c_str());
            if (state == State::offline)
                add(ui::Button::triangle, tr("Library"));
            if (!(state == State::empty && !filters_.any()))
                add(ui::Button::square, tr("Search again"));
            add(ui::Button::circle, tr("Back"));
        }
        else if (cells_.empty())
        {
            add(ui::Button::square, tr("Search again"));
            add(ui::Button::circle, tr("Back"));
        }
        else if (cells_[static_cast<std::size_t>(cell_)].kind == Kind::video)
        {
            add(ui::Button::cross, tr("Play"));
            add(ui::Button::triangle, tr("Details"));
            add(ui::Button::square, tr("Search again"));
            add(ui::Button::touchpad, tr("Actions"));
        }
        else
        {
            add(ui::Button::cross, tr("Open"));
            add(ui::Button::square, tr("Search again"));
            add(ui::Button::circle, tr("Back"));
        }
    }

    // ---- drawing -----------------------------------------------------------------

    float arrive(int index, float step, float duration) const
    {
        return tween::stagger(age_, index, step, duration);
    }

    // What was asked, and how much was found.
    void draw_header(G &g) const
    {
        Enter enter(g, arrive(0, 0.05f, 0.5f), -14.0f);
        glass_panel(g, kField, kField.h * 0.5f);
        icon(g, Icon::search, kField.x + 36.0f, kField.cy(), 28.0f, g.k.ink2);
        text(g, g.f.semibold, fit(g.f.semibold, query_, 27.0f, kField.w - 68.0f - 28.0f), kField.x + 68.0f, kField.cy() + 9.5f, 27.0f, g.k.ink);
        if (load_ == model::Load::ready && !count_.empty() && !cells_.empty())
        {
            const float x = kField.x + kField.w + 28.0f;
            text(g, g.f.regular, fit(g.f.regular, fill(tr("About {0} results"), count_), 22.0f, kRight - 140.0f - x), x, kField.cy() + 8.0f, 22.0f,
                 g.k.ink3);
        }
    }

    void draw_chips(G &g) const
    {
        Enter enter(g, arrive(1, 0.05f, 0.5f), 16.0f);
        icon(g, Icon::sort, kLeft + 16.0f, kChipsY, 26.0f, g.k.ink3);
        const bool scrolls = kChipsLeft + chips_width_ > kRight;
        if (scrolls)
            g.l.push_clip({kChipsLeft - 8.0f, kChipsY - 40.0f, kRight - kChipsLeft + 16.0f, 80.0f});
        for (int i = 0; i < kChipCount; ++i)
        {
            const Rect &r = chips_[static_cast<std::size_t>(i)];
            const float x = r.x - chips_scroll_.position.value;
            // A chip that is pressed dips; one that opens a list carries its chevron inside.
            const float dip = i == chip_ ? press_.value : 0.0f;
            g.l.push_transform(g.reduced ? 1.0f : 1.0f - 0.05f * dip, x + r.w * 0.5f, kChipsY, 0.0f, 0.0f);
            chip(g, x, kChipsY, chip_label(i), chip_selected(i), chip_focus_[static_cast<std::size_t>(i)].value, kChipHeight, chip_opens_list(i));
            g.l.pop_transform();
        }
        if (scrolls)
            g.l.pop_clip();
    }

    void draw_channel(const DrawContext &context, G &g, const Cell &cell, const Rect &big, float focus) const
    {
        const Rect card{big.x, big.y, big.w, big.h + kChannelExtra};
        if (focus > 0.01f)
            g.l.shadow({card.x, card.y + 20.0f, card.w, card.h}, kRadiusPanel, 40.0f, kBlack.with_alpha(0.5f * focus));
        glass_panel(g, card, kRadiusPanel);
        // Without the mark of a subscription the rest sits in the middle of the card.
        const float drop = cell.subscribed ? 0.0f : 26.0f;
        const float room = card.w - 48.0f;
        avatar(g, card.cx(), card.y + 86.0f + drop, 60.0f, cell.title, cell.channel_id, context.images.get(cell.thumb, PictureSize::avatar));
        text(g, g.f.semibold, fit(g.f.semibold, cell.title, 26.0f, room), card.cx(), card.y + 190.0f + drop, 26.0f, g.k.ink, gfx::Align::center);
        // "@handle · 2.41M subscribers"; where both do not fit, the handle is the one that is cut.
        const std::string tail = cell.subscribers.empty() ? std::string() : (cell.handle.empty() ? "" : " \xC2\xB7 ") + cell.subscribers;
        const float tail_w = measure(g.f.regular, tail, kTypeMeta);
        const std::string line = tail_w < room - 120.0f ? fit(g.f.regular, cell.handle, kTypeMeta, room - tail_w) + tail
                                                         : fit(g.f.regular, cell.subscribers.empty() ? cell.handle : cell.subscribers, kTypeMeta, room);
        text(g, g.f.regular, line, card.cx(), card.y + 222.0f + drop, kTypeMeta, g.k.ink2, gfx::Align::center);
        if (cell.subscribed)
        {
            const std::string label = fit(g.f.semibold, tr("Subscribed"), 21.0f, room - 76.0f);
            const float label_w = measure(g.f.semibold, label, 21.0f);
            const float width = std::max(192.0f, label_w + 76.0f);
            const Rect mark{card.cx() - width * 0.5f, card.y + 244.0f, width, 44.0f};
            g.l.rounded_rect(mark, 22.0f, g.k.raised);
            g.l.bordered_rect(mark, 22.0f, clear(), 1.5f, g.k.hairline);
            icon(g, Icon::check, mark.cx() - label_w * 0.5f - 4.0f, mark.cy(), 20.0f, g.k.accent);
            text(g, g.f.semibold, label, mark.cx() - label_w * 0.5f + 16.0f, mark.cy() + 7.5f, 21.0f, g.k.ink);
        }
    }

    // A playlist: its first picture with the others stacked behind it.
    void draw_playlist(const DrawContext &context, G &g, const Cell &cell, const Rect &big, float focus) const
    {
        const Picture art = context.images.get(cell.thumb, PictureSize::card);
        const Color tone = art.ready() ? art.tone : flatten(g.k.raised, g.k.base);
        g.l.rounded_rect({big.x + 28.0f, big.y - 16.0f, big.w - 56.0f, 30.0f}, 12.0f, tone.with_alpha(0.5f));
        g.l.rounded_rect({big.x + 14.0f, big.y - 8.0f, big.w - 28.0f, 30.0f}, 14.0f, tone.with_alpha(0.85f));
        if (focus > 0.01f)
            g.l.shadow({big.x, big.y + 20.0f, big.w, big.h}, kRadiusThumb, 40.0f, kBlack.with_alpha(0.55f * focus));
        const float shown = art.ready() ? (g.reduced ? 1.0f : tween::clamp01(art.age / 0.25f)) : 0.0f;
        if (shown < 1.0f)
            picture_placeholder(g, big, kRadiusThumb);
        if (shown > 0.0f)
        {
            const float dim = 0.86f + 0.14f * focus;
            g.l.image(art.texture, big, art.cover(big.w / big.h), Color{dim, dim, dim, shown}, kRadiusThumb);
        }
        g.l.bordered_rect(big, kRadiusThumb, clear(), 1.5f, kWhite.with_alpha(0.1f)); // the edge every picture card has
        const Rect side{big.x + big.w - 132.0f, big.y, 132.0f, big.h};
        g.l.rounded_rect(side, kRadiusThumb, kBlack.with_alpha(0.66f));
        icon(g, Icon::queue, side.cx(), side.cy() - 18.0f, 34.0f, kWhite);
        if (!cell.count.empty())
        {
            // "24 videos"; a count too long for the panel stands alone under the mark of a list.
            const std::string worded = fill(tr("{0} videos"), cell.count);
            const bool fits = measure(g.f.semibold, worded, 20.0f) <= side.w - 16.0f;
            text(g, g.f.semibold, fits ? worded : fit(g.f.semibold, cell.count, 20.0f, side.w - 16.0f), side.cx(), side.cy() + 32.0f, 20.0f, kWhite,
                 gfx::Align::center);
        }
        const float after = paragraph(g, g.f.semibold, cell.title, big.x + 2.0f, big.y + big.h + 36.0f, 23.0f, big.w - 4.0f, 30.0f,
                                      g.k.ink.with_alpha(0.82f + 0.18f * focus), 2);
        const std::string line = std::string(tr("Playlist")) + (cell.owner.empty() ? "" : " \xC2\xB7 ") + cell.owner;
        text(g, g.f.regular, fit(g.f.regular, line, kTypeMeta, big.w - 4.0f), big.x + 2.0f, after, kTypeMeta, g.k.ink2);
    }

    void draw_bone(ui::Canvas &canvas, int slot, const Rect &r) const
    {
        ui::Skeleton bone = bones_[static_cast<std::size_t>(slot % kBones)];
        bone.set_bounds({r.x, r.y, r.w, r.h + 96.0f});
        bone.draw(canvas);
    }

    // The grid: bones first, then what loaded, in the same places.
    void draw_grid(const DrawContext &context, G &g, ui::Canvas &canvas) const
    {
        if (state_kind() != State::none)
            return;
        const int count = static_cast<int>(cells_.size());
        const float loaded = count > 0 ? (g.reduced ? tween::clamp01(ready_age_ / 0.12f) : tween::smoothstep(ready_age_ / 0.3f)) : 0.0f;
        const float scroll = scroll_.value;
        g.l.push_clip({kRail, kGridClip, gfx::kVirtualWidth - kRail, kGridBottom - kGridClip});
        if (loaded < 1.0f)
        {
            g.l.push_opacity(1.0f - loaded);
            for (int i = 0; i < kBones; ++i)
            {
                Enter enter(g, arrive(2 + i, 0.045f, 0.45f), 26.0f);
                draw_bone(canvas, i, cell_rect(i, 0.0f));
            }
            g.l.pop_opacity();
        }
        const Rect ring = ring_.value();
        const float lit = tween::clamp01(ring_amount_.value);
        for (int i = 0; i < count; ++i)
        {
            const Rect r = cell_rect(i, scroll);
            if (r.y > kGridBottom || r.y + r.h + 140.0f < kGridClip)
                continue;
            // A row that scrolls up leaves under the chips and is gone before it reaches them.
            const float shown = loaded * tween::clamp01(1.0f - (kGridTop - r.y) / 150.0f);
            if (shown <= 0.004f)
                continue;
            const Cell &cell = cells_[static_cast<std::size_t>(i)];
            const float focus = lit * tween::clamp01(1.0f - std::hypot(ring.cx() - r.cx(), ring.cy() - r.cy() + 5.0f) / 260.0f);
            const Rect big = grown(r, 0.05f * focus, 5.0f * focus);
            const int slot = i - top_row_ * kColumns;
            Enter enter(g, arrive(2 + std::clamp(slot, 0, kBones - 1), 0.045f, 0.45f), 26.0f);
            if (cell.kind == Kind::video)
            {
                video_card(g, big, cell.video, context.images.get(cell.thumb, PictureSize::card), focus, shown, 2);
            }
            else
            {
                g.l.push_opacity(shown);
                if (cell.kind == Kind::channel)
                    draw_channel(context, g, cell, big, focus);
                else
                    draw_playlist(context, g, cell, big, focus);
                g.l.pop_opacity();
            }
        }
        // The page on its way: bones where its cards will be.
        if (more_loading_ && count > 0)
        {
            for (int i = count; i < count + kColumns; ++i)
            {
                const Rect r = cell_rect(i, scroll);
                if (r.y < kGridBottom)
                    draw_bone(canvas, i, r);
            }
        }
        if (lit > 0.01f && count > 0 && loaded > 0.5f)
        {
            const Cell &cell = cells_[static_cast<std::size_t>(cell_)];
            Rect at = ring;
            if (cell.kind == Kind::channel)
                at.h += kChannelExtra;
            if (!g.reduced)
                at.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
            const Picture picture = context.images.get(cells_[static_cast<std::size_t>(cell.kind == Kind::channel ? first_picture_ : cell_)].thumb,
                                                       PictureSize::card);
            focus_ring(g, at, cell.kind == Kind::channel ? kRadiusPanel : kRadiusThumb, picture.ready() ? picture.glow : g.k.focus,
                       lit * arrive(6, 0.05f, 0.4f));
        }
        g.l.pop_clip();
    }

    void draw_state(G &g, ui::Canvas &canvas) const
    {
        const State state = state_kind();
        if (state == State::none)
            return;
        ui::EmptyState panel = state_;
        if (state == State::empty)
        {
            const char *quoted = tr("No results for \xE2\x80\x9C{0}\xE2\x80\x9D");
            panel.title = fit(g.f.display, fill(covers(g.f.display, quoted) ? quoted : tr("No results for \"{0}\""), query_), panel.style.title_size,
                              kStateBounds.w - 48.0f);
        }
        const Icon which = state == State::offline ? Icon::offline : state == State::failed ? Icon::alert : Icon::search;
        panel.icon = [&g, which](ui::Canvas &, const Rect &area)
        {
            g.l.circle(area.cx(), area.cy(), area.w * 0.5f, g.k.raised);
            icon(g, which, area.cx(), area.cy(), area.w * 0.52f, g.k.ink);
        };
        panel.draw(canvas);
    }

    Route route_;
    std::string query_;
    search::Filters filters_;
    std::string catalog_;
    float age_ = 0.0f;

    // The list.
    int pages_ = 1;       // pages asked for so far
    int ready_pages_ = 0; // ... and answered, from the first one on
    std::string built_;   // what the cells were put together from
    std::vector<Cell> cells_;
    std::string count_;   // the provider's count of results, as it words it
    model::Load load_ = model::Load::loading;
    std::string error_;
    bool more_ = false;
    bool more_loading_ = false;
    float ready_age_ = 0.0f; // seconds since the first cards arrived
    int first_picture_ = 0;  // the first card with a picture: the page's colour

    // The focus.
    Zone zone_ = Zone::grid;
    int chip_ = 0;
    int cell_ = 0;
    bool placed_ = false; // the focus was put on this list's first video
    int top_row_ = 0;
    tween::Spring scroll_;
    ui::SpringRect ring_;
    bool snap_ring_ = true;
    tween::Spring ring_amount_;
    ui::Pulse nudge_;
    ui::Pulse press_;

    // The filters.
    std::array<Rect, kChipCount> chips_{};
    std::array<tween::Spring, kChipCount> chip_focus_{};
    float chips_width_ = 0.0f;
    ui::Scroller chips_scroll_;
    mutable ui::Menu menu_;
    int menu_chip_ = 0;
    float menu_width_ = 340.0f;

    mutable std::array<ui::Skeleton, kBones> bones_;
    mutable ui::EmptyState state_;
    State shown_state_ = State::none;

    std::array<ui::Hint, 5> hints_{};
    std::size_t hint_count_ = 0;
};

} // namespace

std::unique_ptr<Screen> make_results()
{
    return std::make_unique<Results>();
}

} // namespace afterglow
