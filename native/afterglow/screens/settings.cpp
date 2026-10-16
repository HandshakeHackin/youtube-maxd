// ProsperoTube Afterglow - Settings: twelve tiles and the pages behind them.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One screen, three shapes, chosen by Route::argument:
//   ""                the tiles (board 31): what each part is set to, in one line
//   "tile:<key>"      a tile's page (board 32): its rows of the main list and of
//                     the catalogs it reads, under their headings
//   "<catalog id>"    any list under "settings" as a page: the choices of a row,
//                     a further page, a list that can be arranged
// A row is drawn and driven by what its facts say it is (settings.hpp, Kind).
// This file is what the frame asks of the screen, and how the page is read and
// laid out; changes are in settings_changes.cpp, the controller in
// settings_input.cpp, what moves and what is drawn in settings_view.cpp.

#include "afterglow/screens/settings_screen.hpp"

namespace afterglow::settings
{

ScreenId Settings::id() const
{
    return ScreenId::settings;
}

void Settings::enter(const Route &route, UpdateContext &context, bool restored)
{
    profile_ = context.services.data.profile();
    if (restored)
    {
        // Back from a page above: the focus and the scroll stay; what that page changed arrives with the next read.
        choices_.clear();
        steps_ = 0;
        grabbed_ = false;
        leave_ = 0.0f;
        return;
    }
    route_ = route;
    home_ = route.argument.empty();
    tile_ = -1;
    sources_.clear();
    path_.clear();
    if (!home_ && starts_with(route.argument, kTilePrefix))
        tile_ = tile_index(std::string_view(route.argument).substr(kTilePrefix.size()));
    if (home_)
    {
        add_source(kMain);
    }
    else if (tile_ >= 0)
    {
        const Tile &tile = tiles()[static_cast<std::size_t>(tile_)];
        add_source(kMain);
        for (const char *catalog : tile.reads)
        {
            if (catalog != nullptr)
                add_source(catalog);
        }
        path_.emplace_back(tr(tile.name));
    }
    else
    {
        add_source(route.argument);
        path_ = split(route.title, kPathSeparator);
        if (path_.empty())
            path_.emplace_back(tr("Settings"));
    }
    items_.clear();
    lines_.clear();
    shown_.clear();
    for (int i = 0; i + 1 < static_cast<int>(tiles().size()); ++i)
        shown_.push_back(i);
    reset_ = Item{};
    has_reset_ = false;
    changes_.clear();
    choices_.clear();
    motion_.clear();
    focus_ = 0;
    focus_id_.clear();
    placed_ = false;
    content_ = 0.0f;
    steps_ = 0;
    grabbed_ = false;
    grab_.snap(0.0f);
    leave_ = 0.0f;
    age_ = 0.0f;
    shown_age_ = 0.0f;
    scroll_.position.snap(0.0f);
    tiles_scroll_.snap(0.0f);
    plate_.snap(0.0f);
    ring_.snap(grown(tile_rect(0), 0.03f, 4.0f));
    sticky_ = PreviewKind::none;
    preview_ = PreviewState{};
    preview_placed_ = false;
    applied_ = refused_ = 0;
    last_applied_.clear();
    last_refused_.clear();
}

void Settings::update(const InputFrame &input, UpdateContext &context)
{
    age_ += context.dt;
    read(context.services.data);
    settle(context);
    const model::Load state = home_ ? sources_.front().load : page_load();
    shown_age_ = state == model::Load::idle || state == model::Load::loading ? 0.0f : shown_age_ + context.dt;
    if (context.top)
    {
        if (home_)
            input_home(input, context);
        else
            input_page(input, context);
    }
    advance(context);
    // The picture under the preview is asked for before it is first drawn.
    if (!home_ && !preview_.picture.empty())
        context.services.images.get(preview_.picture, PictureSize::hero);
    make_hints();
}

void Settings::draw(const DrawContext &context, const Phase &phase) const
{
    G g = context.g();
    HandOver hand(g, phase);
    if (home_)
        draw_home(g);
    else
        draw_page(g, context);
}

std::string Settings::title() const
{
    return home_ ? std::string(tr("Settings")) : std::string();
}

std::span<const ui::Hint> Settings::hints() const
{
    return {hints_.data(), hint_count_};
}

FocusInfo Settings::focus() const
{
    FocusInfo info;
    if (home_)
    {
        const int index = shown_.empty() ? 0 : shown_[static_cast<std::size_t>(std::clamp(focus_, 0, static_cast<int>(shown_.size()) - 1))];
        const Tile &tile = tiles()[static_cast<std::size_t>(index)];
        info.id = std::string("tile/") + tile.key;
        info.text = tr(tile.name);
        info.rect = tile_rect(focus_, tiles_scroll_.target);
        info.index = focus_;
        info.total = static_cast<int>(shown_.size());
        return info;
    }
    const model::Load state = page_load();
    if (state != model::Load::ready)
    {
        info.id = state == model::Load::loading ? "loading" : "action/retry";
        info.text = path_.empty() ? std::string() : path_.back();
        info.rect = state == model::Load::loading ? Rect{kFormX, kFormTop, kFormW, kView} : kNoticeButton;
        info.total = 1;
        return info;
    }
    const Item *item = focused();
    if (item == nullptr)
    {
        info.id = "action/back";
        info.text = tr("Back to Settings");
        info.rect = kNoticeButton;
        info.total = 1;
        return info;
    }
    const Line &line = lines_[static_cast<std::size_t>(focus_)];
    info.id = name_of(*item);
    info.text = item->label;
    const float y = std::clamp(kFormTop + line.y - scroll_.position.target, kFormTop, kFormBottom - kRowH);
    info.rect = {kFormX, y, kFormW, kRowH};
    info.total = static_cast<int>(items_.size());
    info.index = line.item;
    return info;
}

std::vector<std::pair<std::string, std::string>> Settings::focusable() const
{
    std::vector<std::pair<std::string, std::string>> all;
    if (home_)
    {
        for (const int index : shown_)
            all.emplace_back(std::string("tile/") + tiles()[static_cast<std::size_t>(index)].key, tr(tiles()[static_cast<std::size_t>(index)].name));
    }
    else
    {
        for (const Item &item : items_)
            all.emplace_back(name_of(item), item.label);
    }
    return all;
}

void Settings::snapshot(Snapshot &out) const
{
    out.set("typing", typing_ ? 1 : 0);
    out.set("page", home_ ? std::string("home") : route_.argument);
    out.set("load", load_name(home_ ? sources_.front().load : page_load()));
    out.set("tiles", static_cast<int>(shown_.size()));
    out.set("rows", static_cast<int>(items_.size()));
    out.set("waiting", static_cast<int>(changes_.size()));
    out.set("applied", applied_);
    out.set("refused", refused_);
    out.set("last_applied", last_applied_);
    out.set("last_refused", last_refused_);
    out.set("moving", grabbed_ ? 1 : 0);
    out.set("scroll", static_cast<int>(home_ ? tiles_scroll_.target : scroll_.position.target));
    out.set("defaults", has_reset_ ? 1 : 0);
    static constexpr const char *kPreviews[] = {"none", "captions", "theme", "card"};
    out.set("preview", kPreviews[static_cast<std::size_t>(preview_.kind)]);
    if (const Item *item = home_ ? nullptr : focused())
    {
        const Shown now = shown(*item);
        static constexpr const char *kKinds[] = {"switch", "list", "slider", "cycle", "choice", "action", "text", "page", "link", "info", "order", "reset"};
        out.set("kind", kKinds[static_cast<std::size_t>(item->kind)]);
        out.set("value", item->kind == Kind::toggle || item->kind == Kind::choice || item->kind == Kind::order ? std::string(now.on ? "on" : "off") : now.value);
    }
    if (home_)
    {
        const Source &main = sources_.front();
        std::vector<std::string> lines;
        for (const int index : shown_)
        {
            const Tile &tile = tiles()[static_cast<std::size_t>(index)];
            lines.push_back(std::string(tile.key) + ": " + tile_summary(tile, main.rows));
        }
        out.list("summaries", std::move(lines));
    }
    else
    {
        std::vector<std::string> labels;
        for (const Item &item : items_)
            labels.push_back(item.label);
        out.list("rows", std::move(labels));
    }
}

void Settings::read(Data &data)
{
    bool changed = false;
    for (Source &source : sources_)
    {
        const Page &first = data.catalog(source.catalog, 0);
        if (first.load == model::Load::loading || first.load == model::Load::idle)
        {
            if (source.load == model::Load::idle)
                source.load = model::Load::loading;
            continue; // what is shown stays until the newer answer is here
        }
        if (first.load != model::Load::ready)
        {
            if (source.load != first.load || source.error != first.error)
            {
                source.load = first.load;
                source.error = first.error;
                source.rows.clear();
                source.mark = 0;
                changed = true;
            }
            continue;
        }
        // Every page asked for that has arrived, in order.
        std::uint64_t mark = mark_of(first.rows);
        bool more = first.more;
        int arrived = 1;
        for (; arrived < source.pages && more; ++arrived)
        {
            const Page &next = data.catalog(source.catalog, arrived);
            if (next.load != model::Load::ready)
            {
                more = next.load == model::Load::loading || next.load == model::Load::idle;
                break;
            }
            mark = mark_of(next.rows, mark);
            more = next.more;
        }
        // A list read again (after a change) keeps what it shows until every part asked for is here again:
        // a row on a later part keeps the focus and the page its scroll, instead of the first part alone standing in.
        if (arrived < source.pages && more && source.load == model::Load::ready && !source.rows.empty())
            continue;
        if (mark == source.mark && source.load == model::Load::ready)
        {
            source.more = more;
            continue;
        }
        source.rows = first.rows;
        for (int page = 1; page < arrived; ++page)
        {
            const Page &next = data.catalog(source.catalog, page);
            source.rows.insert(source.rows.end(), next.rows.begin(), next.rows.end());
        }
        source.mark = mark;
        source.more = more;
        source.load = model::Load::ready;
        source.error.clear();
        changed = true;
    }
    if (changed)
        rebuild();
}

// The focused row's choices, for Left and Right on it: its own catalog.
void Settings::read_choices(Data &data)
{
    const Item *item = home_ ? nullptr : focused();
    if (item == nullptr || (item->kind != Kind::list && item->kind != Kind::slider))
        return;
    Choices &choices = choices_[item->row.id];
    if (choices.ready)
        return;
    std::vector<Item> rows;
    for (int page = 0; page < 8; ++page)
    {
        const Page &part = data.catalog(item->row.id, page);
        if (part.load == model::Load::loading || part.load == model::Load::idle)
            return; // not here yet
        if (part.load != model::Load::ready)
            break; // not to be had: Left and Right then answer as the end of a list does
        for (const model::Row &row : part.rows)
        {
            Item choice = item_of(row, item->row.id);
            if (choice.kind == Kind::choice)
                rows.push_back(std::move(choice));
        }
        if (!part.more)
            break;
    }
    choices.rows = std::move(rows);
    choices.ready = true;
}

void Settings::add_source(std::string_view catalog)
{
    sources_.emplace_back();
    sources_.back().catalog = std::string(catalog);
}

Settings::Source *Settings::source_of(std::string_view catalog)
{
    for (Source &source : sources_)
    {
        if (source.catalog == catalog)
            return &source;
    }
    return nullptr;
}

// The page as it is shown, from what was read.
void Settings::rebuild()
{
    const std::vector<model::Row> &main = sources_.front().rows;
    if (home_)
    {
        const std::string key = shown_.empty() ? std::string() : std::string(tiles()[static_cast<std::size_t>(shown_[static_cast<std::size_t>(std::min<int>(focus_, static_cast<int>(shown_.size()) - 1))])].key);
        const int more = static_cast<int>(tiles().size()) - 1;
        shown_.clear();
        for (int i = 0; i < more; ++i)
            shown_.push_back(i);
        // "More" exists only while the main list holds a row the table does not know.
        unknown_ = static_cast<int>(std::count_if(main.begin(), main.end(), [](const model::Row &row) { return !is_container(row.id) && place_of(kMain, row.id).tile < 0; }));
        if (unknown_ > 0)
            shown_.push_back(more);
        focus_ = std::clamp(focus_, 0, static_cast<int>(shown_.size()) - 1);
        for (std::size_t i = 0; i < shown_.size(); ++i)
        {
            if (key == tiles()[static_cast<std::size_t>(shown_[i])].key)
                focus_ = static_cast<int>(i);
        }
        return;
    }
    items_.clear();
    has_reset_ = false;
    if (tile_ >= 0)
    {
        const Tile &tile = tiles()[static_cast<std::size_t>(tile_)];
        for (const Source &source : sources_)
        {
            const bool is_main = source.catalog == kMain;
            for (const model::Row &row : source.rows)
            {
                if (is_main && is_container(row.id))
                    continue; // its rows are on some tile's page themselves
                const Place place = place_of(source.catalog, row.id);
                if ((place.tile >= 0 ? place.tile : fallback_tile(source.catalog)) != tile_)
                    continue;
                Item item = item_of(row, source.catalog);
                item.rank = place.rank;
                if (item.kind == Kind::reset)
                {
                    reset_ = std::move(item);
                    has_reset_ = true;
                    continue;
                }
                // A row the table does not know keeps what it says itself, under "More" on the page it arrived for.
                if (place.tile >= 0)
                    item.section = tr(place.section);
                else if (item.section.empty() && !is_main)
                    item.section = tr("More");
                items_.push_back(std::move(item));
            }
        }
        std::stable_sort(items_.begin(), items_.end(), [](const Item &a, const Item &b) { return a.rank < b.rank; });
        if (tile.link != ScreenId::settings && !items_.empty())
        {
            // The tile's own screen is another package's: the page leads there first.
            Item link;
            link.row.id = "link";
            link.row.facts = {{"screen", screen_name(tile.link)}};
            link.catalog = kMain;
            link.label = tr(tile.link_label);
            link.detail = tr(tile.link_detail);
            link.value = tile_summary(tile, main);
            link.kind = Kind::link;
            // A Guest's "Sign in to YouTube" stays first; the tile's own screen follows it.
            const bool sign_in_first = items_.front().row.id == "auth:start";
            items_.insert(items_.begin() + (sign_in_first ? 1 : 0), std::move(link));
        }
    }
    else
    {
        for (const model::Row &row : main)
        {
            Item item = item_of(row, sources_.front().catalog);
            if (item.kind == Kind::reset)
            {
                reset_ = std::move(item);
                has_reset_ = true;
                continue;
            }
            items_.push_back(std::move(item));
        }
    }
    lines_.clear();
    float y = 0.0f;
    std::string section;
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        const Item &item = items_[i];
        if (item.section != section || (i == 0 && !item.section.empty()))
        {
            if (!item.section.empty())
            {
                lines_.push_back({-1, item.section, y, kHeadingH});
                y += kHeadingH + kRowGap;
            }
            else if (i != 0)
            {
                y += 22.0f; // rows without a heading after rows that had one stand apart
            }
            section = item.section;
        }
        lines_.push_back({static_cast<int>(i), {}, y, kRowH});
        y += kRowH + kRowGap;
    }
    content_ = std::max(0.0f, y - kRowGap);
    place_focus();
}

// The focus stays on its row when the rows change; when that row is gone, on the nearest one.
void Settings::place_focus()
{
    int found = -1;
    int first = -1;
    int chosen = -1;
    for (std::size_t i = 0; i < lines_.size(); ++i)
    {
        if (lines_[i].item < 0)
            continue;
        const Item &item = items_[static_cast<std::size_t>(lines_[i].item)];
        if (first < 0)
            first = static_cast<int>(i);
        if (chosen < 0 && item.kind == Kind::choice && item.on)
            chosen = static_cast<int>(i);
        if (!focus_id_.empty() && key_of(item) == focus_id_)
            found = static_cast<int>(i);
    }
    if (first < 0 || page_load() == model::Load::loading)
    {
        focus_ = std::max(first, 0);
        return; // the focus takes its place when the whole page is here
    }
    if (found >= 0)
    {
        focus_ = found;
    }
    else if (!placed_)
    {
        focus_ = chosen >= 0 ? chosen : first; // a list opens on the choice that stands
    }
    else
    {
        focus_ = std::clamp(focus_, first, static_cast<int>(lines_.size()) - 1);
        while (focus_ > first && lines_[static_cast<std::size_t>(focus_)].item < 0)
            --focus_;
    }
    focus_id_ = key_of(items_[static_cast<std::size_t>(lines_[static_cast<std::size_t>(focus_)].item)]);
    if (!placed_)
    {
        placed_ = true;
        reveal();
        scroll_.position.snap(scroll_.position.target);
        plate_.snap(lines_[static_cast<std::size_t>(focus_)].y);
    }
}

std::string Settings::key_of(const Item &item)
{
    return item.catalog + '\n' + item.row.id;
}

// "row/captions", "row/ui:scheme:1": the row's id without what every id here starts with.
std::string Settings::name_of(const Item &item)
{
    constexpr std::string_view kPrefix = "settings:";
    const std::string_view id = item.row.id;
    return "row/" + std::string(starts_with(id, kPrefix) ? id.substr(kPrefix.size()) : id);
}

const Item *Settings::focused() const
{
    if (focus_ < 0 || focus_ >= static_cast<int>(lines_.size()))
        return nullptr;
    const int index = lines_[static_cast<std::size_t>(focus_)].item;
    return index < 0 ? nullptr : &items_[static_cast<std::size_t>(index)];
}

// Loading until a first answer is here; then ready while there is anything to show; else the state of the page's own list.
model::Load Settings::page_load() const
{
    // A page made of two lists waits for both: rows that arrived first would jump when the others came.
    for (const Source &source : sources_)
    {
        if (source.load == model::Load::idle || source.load == model::Load::loading)
            return model::Load::loading;
    }
    if (!items_.empty())
        return model::Load::ready;
    for (auto it = sources_.rbegin(); it != sources_.rend(); ++it)
    {
        if (it->load != model::Load::ready)
            return it->load;
    }
    return model::Load::ready;
}

const std::string &Settings::page_error() const
{
    static const std::string kNone;
    for (auto it = sources_.rbegin(); it != sources_.rend(); ++it)
    {
        if (it->load != model::Load::ready && !it->error.empty())
            return it->error;
    }
    return kNone;
}

} // namespace afterglow::settings

namespace afterglow
{

std::unique_ptr<Screen> make_settings()
{
    return std::make_unique<settings::Settings>();
}

} // namespace afterglow
