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
// A change goes to the provider through Data::act(); the row shows the value
// that was asked for while it is on its way, the provider's when it answered,
// and returns to what it had when the provider refused.

#include "afterglow/screens/settings.hpp"

#include "ui/components/keyboard.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>

namespace afterglow
{

namespace
{

using namespace settings;

constexpr float kView = kFormBottom - kFormTop;
constexpr std::string_view kTilePrefix = "tile:";
constexpr std::size_t kWaiting = 12; // changes that may wait for the provider at once
constexpr int kPagesAtMost = 16;

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::uint64_t mixed(std::uint64_t hash, std::string_view text)
{
    for (const char c : text)
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    hash ^= 0xffu;
    hash *= 1099511628211ull;
    return hash;
}

// A number that changes when anything a page shows of these rows changes.
std::uint64_t mark_of(const std::vector<model::Row> &rows, std::uint64_t hash = 1469598103934665603ull)
{
    for (const model::Row &row : rows)
    {
        hash = mixed(hash, row.id);
        hash = mixed(hash, row.title);
        hash = mixed(hash, row.subtitle);
        hash = mixed(hash, row.overview);
        for (const auto &fact : row.facts)
        {
            hash = mixed(hash, fact.first);
            hash = mixed(hash, fact.second);
        }
    }
    return hash;
}

void set_fact(model::Row &row, std::string_view name, std::string value)
{
    for (auto &fact : row.facts)
    {
        if (fact.first == name)
        {
            fact.second = std::move(value);
            return;
        }
    }
    row.facts.emplace_back(std::string(name), std::move(value));
}

// True when an answer is the list these rows are of, as it now stands.
bool same_list(const std::vector<model::Row> &rows, const std::vector<model::Row> &answer)
{
    if (rows.empty() || answer.empty())
        return false;
    std::size_t shared = 0;
    for (const model::Row &row : rows)
    {
        shared += std::any_of(answer.begin(), answer.end(), [&](const model::Row &other) { return other.id == row.id; }) ? 1u : 0u;
    }
    return shared * 10 >= rows.size() * 6 && shared * 10 >= answer.size() * 6;
}

std::string filled(const char *pattern, std::string_view value)
{
    std::string text = pattern;
    const std::size_t at = text.find("{0}");
    if (at != std::string::npos)
        text.replace(at, 3, value);
    return text;
}

std::vector<std::string> split(std::string_view text, char separator)
{
    std::vector<std::string> parts;
    while (!text.empty())
    {
        const std::size_t at = text.find(separator);
        if (at != 0)
            parts.emplace_back(text.substr(0, at));
        if (at == std::string_view::npos)
            break;
        text.remove_prefix(at + 1);
    }
    return parts;
}

void drop_last_character(std::string &text)
{
    while (!text.empty())
    {
        const unsigned char c = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((c & 0xC0u) != 0x80u)
            break;
    }
}

// Draws what follows at a share of its strength.
struct Faded
{
    gfx::DrawList &l;
    Faded(G &g, float amount) : l(g.l)
    {
        l.push_opacity(tween::clamp01(amount));
    }
    ~Faded()
    {
        l.pop_opacity();
    }
    Faded(const Faded &) = delete;
    Faded &operator=(const Faded &) = delete;
};

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

class Settings final : public Screen
{
  public:
    Settings() : alive_(std::make_shared<int>(0)) {}

    ScreenId id() const override
    {
        return ScreenId::settings;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
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
            sources_.push_back({kMain});
        }
        else if (tile_ >= 0)
        {
            const Tile &tile = tiles()[static_cast<std::size_t>(tile_)];
            sources_.push_back({kMain});
            for (const char *catalog : tile.reads)
            {
                if (catalog != nullptr)
                    sources_.push_back({catalog});
            }
            path_.emplace_back(tr(tile.name));
        }
        else
        {
            sources_.push_back({route.argument});
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
        scroll_.position.snap(0.0f);
        tiles_scroll_.snap(0.0f);
        plate_.snap(0.0f);
        ring_.snap(grown(tile_rect(0), 0.03f, 4.0f));
        sticky_ = PreviewKind::none;
        preview_ = PreviewState{};
        preview_placed_ = false;
        sheet_.open = false;
        sheet_.amount.snap(0.0f);
        applied_ = refused_ = 0;
        last_applied_.clear();
        last_refused_.clear();
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        read(context.services.data);
        settle(context);
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

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        // The hand-over: a page deeper arrives from the right while this one slides off to the left, and back.
        const bool reduced = context.reduced_motion;
        float opacity = 1.0f;
        float scale = 1.0f;
        float dx = 0.0f;
        if (phase.out > 0.0f)
        {
            opacity = 1.0f - tween::smoothstep(phase.out * 2.4f);
            if (!reduced && phase.move == Move::fade)
                scale = 1.0f - 0.025f * tween::cubic_out(phase.out);
            if (!reduced && phase.move == Move::push)
                dx = -90.0f * tween::cubic_in(phase.out);
            if (!reduced && phase.move == Move::pop)
                dx = 90.0f * tween::cubic_in(phase.out);
        }
        else if (phase.in < 1.0f)
        {
            opacity = tween::smoothstep((phase.in - 0.12f) / 0.6f);
            if (!reduced && phase.move == Move::push)
                dx = 110.0f * (1.0f - tween::quint_out(phase.in));
            if (!reduced && phase.move == Move::pop)
                dx = -110.0f * (1.0f - tween::quint_out(phase.in));
        }
        if (opacity <= 0.003f)
            return;
        g.l.push_opacity(opacity);
        g.l.push_transform(scale, 960.0f, 540.0f, dx, 0.0f);
        if (home_)
            draw_home(g);
        else
            draw_page(g, context);
        g.l.pop_transform();
        if (sheet_.amount.value > 0.01f)
            draw_sheet(g, context);
        g.l.pop_opacity();
    }

    std::string title() const override
    {
        // REQUEST: a path over the title in the frame's header (requests/e.md, 3). Until the frame draws
        // "SETTINGS > CAPTIONS" and sets the name lower, a page draws both itself and keeps the frame's title blank.
        return home_ ? std::string(tr("Settings")) : std::string(" ");
    }

    std::span<const ui::Hint> hints() const override
    {
        return {hints_.data(), hint_count_};
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        if (sheet_.open)
        {
            info.id = "sheet/keys";
            info.text = sheet_.item.label;
            info.rect = sheet_.keys.key_rect(sheet_.keys.focus_row(), sheet_.keys.focus_key());
            info.total = 1;
            return info;
        }
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

    std::vector<std::pair<std::string, std::string>> focusable() const override
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

    void snapshot(Snapshot &out) const override
    {
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
        out.set("typing", sheet_.open ? 1 : 0);
        out.set("typed", sheet_.text);
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

  private:
    // A catalog the page shows rows of.
    struct Source
    {
        std::string catalog;
        model::Load load = model::Load::idle;
        std::string error;
        std::vector<model::Row> rows; // as shown: the data layer's, or newer ones a change brought back
        std::uint64_t mark = 0;       // of the data layer's rows last taken over
        int pages = 1;                // how many pages are asked for
        bool more = false;            // a further page exists beyond those shown
    };

    struct Line
    {
        int item = -1; // -1: a heading
        std::string heading;
        float y = 0.0f; // from the top of the page's content
        float h = 0.0f;
    };

    // One change on its way to the provider.
    struct Change
    {
        std::string item;    // the row it is shown on ("" for a page's defaults)
        std::string catalog; // ... and the list that row is of
        std::string action;  // the row the provider is asked to carry out
        std::string value;   // what the row shows meanwhile
        std::string done;    // what to say when it worked, "" for nothing
        bool sets_value = false;
        bool on = false;
        bool sets_on = false;
        int index = -1;
        model::Row choice; // the choice that was taken: the previews read its facts
        bool has_choice = false;
        bool absolute = false; // names its result: a later one for the same row replaces it while it waits
        bool leave = false;    // a choice on a list page: the page closes once it is confirmed
        bool moved = false;    // the row changed its place
        std::vector<model::Row> before; // moved: the list as it was
        unsigned ticket = 0;
    };

    struct Shown
    {
        std::string value;
        bool on = false;
        int index = -1;
        bool waiting = false;
        const model::Row *choice = nullptr;
    };

    struct Motion
    {
        tween::Bounce on;
        tween::Spring fill;
        tween::Spring waiting;
        ui::Pulse applied;
        ui::Pulse refused;
        ui::Pulse nudge;
        float direction = 0.0f;
        bool placed = false;
    };

    struct Choices
    {
        bool ready = false;
        std::vector<Item> rows;
    };

    // REQUEST: text entry as a service of the frame and Data::act(row, text) (requests/e.md, 1 and 2).
    // Until then a typed value is taken here, on the kit's keyboard, and the provider is only told that
    // the row was chosen.
    struct TextSheet
    {
        bool open = false;
        Item item;
        std::string text;
        ui::Keyboard keys;
        tween::Spring amount;
    };

    static constexpr Rect kNoticeButton{kLeft + 136.0f, 470.0f, 300.0f, 60.0f};

    // ---- reading --------------------------------------------------------------------

    void read(Data &data)
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
    void read_choices(Data &data)
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

    Source *source_of(std::string_view catalog)
    {
        for (Source &source : sources_)
        {
            if (source.catalog == catalog)
                return &source;
        }
        return nullptr;
    }

    // The page as it is shown, from what was read.
    void rebuild()
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
                items_.insert(items_.begin(), std::move(link));
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
    void place_focus()
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

    static std::string key_of(const Item &item)
    {
        return item.catalog + '\n' + item.row.id;
    }

    // "row/captions", "row/ui:scheme:1": the row's id without what every id here starts with.
    static std::string name_of(const Item &item)
    {
        constexpr std::string_view kPrefix = "settings:";
        const std::string_view id = item.row.id;
        return "row/" + std::string(starts_with(id, kPrefix) ? id.substr(kPrefix.size()) : id);
    }

    const Item *focused() const
    {
        if (focus_ < 0 || focus_ >= static_cast<int>(lines_.size()))
            return nullptr;
        const int index = lines_[static_cast<std::size_t>(focus_)].item;
        return index < 0 ? nullptr : &items_[static_cast<std::size_t>(index)];
    }

    // Loading until a first answer is here; then ready while there is anything to show; else the state of the page's own list.
    model::Load page_load() const
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

    const std::string &page_error() const
    {
        static const std::string kNone;
        for (auto it = sources_.rbegin(); it != sources_.rend(); ++it)
        {
            if (it->load != model::Load::ready && !it->error.empty())
                return it->error;
        }
        return kNone;
    }

    // ---- changes --------------------------------------------------------------------

    // What a row shows now: its confirmed value, or the newest one asked for.
    Shown shown(const Item &item) const
    {
        Shown now{item.value, item.on, item.index, false, nullptr};
        for (const Change &change : changes_)
        {
            if (change.item != item.row.id || change.catalog != item.catalog)
                continue;
            now.waiting = true;
            if (change.sets_value)
                now.value = change.value;
            if (change.sets_on)
                now.on = change.on;
            if (change.index >= 0)
                now.index = change.index;
            if (change.has_choice)
                now.choice = &change.choice;
        }
        return now;
    }

    bool ask(Change change, UpdateContext &context)
    {
        if (!change.absolute || changes_.empty() || changes_.back().ticket != 0 || !changes_.back().absolute ||
            changes_.back().item != change.item || changes_.back().catalog != change.catalog)
        {
            if (changes_.size() >= kWaiting)
            {
                context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
                return false;
            }
            changes_.push_back(std::move(change));
        }
        else
        {
            changes_.back() = std::move(change); // the step before it was never sent: this one stands for both
        }
        return true;
    }

    // Asks first, with the frame's dialog; the change is made when the answer is yes.
    void ask_first(Change change, std::string_view title, std::string_view body, std::string_view yes, UpdateContext &context)
    {
        asking_ = std::move(change);
        const std::weak_ptr<int> alive = alive_;
        context.nav.confirm(title, body, yes, [this, alive](bool agreed)
        {
            if (alive.expired())
                return;
            if (agreed && changes_.size() < kWaiting)
                changes_.push_back(std::move(asking_));
            asking_ = Change{};
        });
    }

    // One change at a time goes to the provider; its answer settles the row.
    void settle(UpdateContext &context)
    {
        if (changes_.empty())
            return;
        Data &data = context.services.data;
        Change &front = changes_.front();
        if (front.ticket == 0)
            front.ticket = data.act(front.action);
        const Page &state = data.action(front.ticket);
        if (state.load == model::Load::loading)
            return;
        const Change done = std::move(front);
        changes_.pop_front();
        if (state.load == model::Load::ready)
        {
            const std::vector<model::Row> answer = state.rows;
            confirmed(done, answer, context);
        }
        else
        {
            const std::string error = state.load == model::Load::idle ? std::string() : state.error;
            refused(done, error, context);
        }
    }

    void confirmed(const Change &done, const std::vector<model::Row> &answer, UpdateContext &context)
    {
        // The provider answers with a list. Where that is a list of this page, it is that list as it now stands.
        bool refreshed = false;
        for (Source &source : sources_)
        {
            if (source.pages == 1 && same_list(source.rows, answer))
            {
                source.rows = answer;
                refreshed = true;
            }
        }
        const auto listed = choices_.find(done.item);
        std::vector<model::Row> before;
        if (listed != choices_.end())
        {
            for (const Item &choice : listed->second.rows)
                before.push_back(choice.row);
            if (same_list(before, answer))
            {
                listed->second.rows.clear();
                for (const model::Row &row : answer)
                    listed->second.rows.push_back(item_of(row, done.item));
            }
        }
        if (!refreshed)
            patch(done, answer);
        // The data layer's copies are older than the provider now: the next read asks again.
        Data &data = context.services.data;
        for (const Source &source : sources_)
            data.forget(source.catalog);
        data.forget(kMain);
        if (!done.item.empty())
            data.forget(done.item);
        rebuild();
        if (!done.item.empty() && !done.moved)
            motion_[done.catalog + '\n' + done.item].applied.trigger(1.0f);
        ++applied_;
        last_applied_ = done.action;
        if (!done.done.empty())
            context.nav.toast(done.done, ToastKind::success);
        if (done.leave)
            leave_ = 0.3f;
    }

    // The answer was another list than the row's: the row takes what the answer says of it, or what was asked.
    void patch(const Change &done, const std::vector<model::Row> &answer)
    {
        Source *source = source_of(done.catalog);
        if (source == nullptr || done.item.empty())
            return;
        const auto row = std::find_if(source->rows.begin(), source->rows.end(), [&](const model::Row &r) { return r.id == done.item; });
        if (row == source->rows.end())
            return;
        const Item item = item_of(*row, done.catalog);
        if (item.kind == Kind::choice)
        {
            for (model::Row &other : source->rows)
                set_fact(other, "selected", other.id == done.item ? "1" : "0");
            return;
        }
        if (item.kind == Kind::list || item.kind == Kind::slider)
        {
            // The choice the answer marks is the one that stands.
            int position = 0;
            for (const model::Row &other : answer)
            {
                const Item choice = item_of(other, done.item);
                if (choice.kind != Kind::choice)
                    continue;
                if (choice.on)
                {
                    set_fact(*row, "value", choice.label);
                    set_fact(*row, "index", std::to_string(position));
                    for (const char *name : {"number", "look", "background", "accent", "text_rgb", "background_alpha", "edge"})
                    {
                        if (!other.fact(name).empty())
                            set_fact(*row, name, other.fact(name));
                    }
                    return;
                }
                ++position;
            }
        }
        if (done.sets_value)
            set_fact(*row, "value", done.value);
        if (done.sets_on)
            set_fact(*row, "on", done.on ? "1" : "0");
        if (done.index >= 0)
            set_fact(*row, "index", std::to_string(done.index));
        if (done.has_choice)
        {
            for (const char *name : {"number", "look", "background", "accent", "text_rgb", "background_alpha", "edge"})
            {
                if (!done.choice.fact(name).empty())
                    set_fact(*row, name, done.choice.fact(name));
            }
        }
    }

    void refused(const Change &done, const std::string &error, UpdateContext &context)
    {
        // What was asked of the same row after it falls with it: the row is back at what the provider holds.
        changes_.erase(std::remove_if(changes_.begin(), changes_.end(),
                                      [&](const Change &other) { return other.ticket == 0 && other.item == done.item && other.catalog == done.catalog; }),
                       changes_.end());
        if (done.moved)
        {
            if (Source *source = source_of(done.catalog))
                source->rows = done.before;
            rebuild();
        }
        if (!done.item.empty())
            motion_[done.catalog + '\n' + done.item].refused.trigger(1.0f);
        ++refused_;
        last_refused_ = done.action;
        const std::string words = error.empty() ? std::string(tr("That could not be saved. Nothing was changed.")) : error;
        said_row_ = done.catalog + '\n' + done.item; // the row's help says it in full for as long as the focus stays
        said_ = words;
        context.nav.toast(words, ToastKind::error);
        context.feedback.play(audio::Cue::error);
        context.feedback.rumble(0.25f, 0.05f);
    }

    // ---- input: the tiles -----------------------------------------------------------

    void input_home(const InputFrame &input, UpdateContext &context)
    {
        const int count = static_cast<int>(shown_.size());
        const Source &main = sources_.front();
        int next = focus_;
        bool edge = false;
        switch (input.nav)
        {
        case Direction::left:
            if (focus_ % kTileColumns > 0)
                next = focus_ - 1;
            else
                context.nav.open_rail();
            break;
        case Direction::right:
            if (focus_ % kTileColumns < kTileColumns - 1 && focus_ + 1 < count)
                next = focus_ + 1;
            else
                edge = true;
            break;
        case Direction::up:
            if (focus_ >= kTileColumns)
                next = focus_ - kTileColumns;
            else
                edge = true;
            break;
        case Direction::down:
            if (focus_ / kTileColumns < (count - 1) / kTileColumns)
                next = std::min(focus_ + kTileColumns, count - 1); // a last row that is not full takes its last tile
            else
                edge = true;
            break;
        case Direction::none:
            break;
        }
        if (next != focus_)
        {
            focus_ = next;
            context.feedback.play(audio::Cue::focus, 1.08f - 0.04f * static_cast<float>(next / kTileColumns), ui::pan_for_x(tile_rect(next).cx()));
        }
        else if (edge)
        {
            bump(input, context);
        }
        if (count == 0)
            return;
        const int index = shown_[static_cast<std::size_t>(focus_)];
        const Tile &tile = tiles()[static_cast<std::size_t>(index)];
        if (pressed(input, Action::confirm))
        {
            // A tile that is only the door to another package's screen opens that; every other one its page.
            if (tile.link != ScreenId::settings && !tile_has_page(tile, main.rows))
                context.nav.push(Route{tile.link, {}, tr(tile.name)}, Move::push);
            else
                context.nav.push(Route{ScreenId::settings, std::string(kTilePrefix) + tile.key, {}}, Move::push);
        }
        else if (pressed(input, Action::back))
        {
            context.nav.back();
        }
        else if (pressed(input, Action::menu))
        {
            // Options: this part of the settings back to its defaults.
            if (const model::Row *reset = tile_reset(tile, main.rows))
                ask_reset(item_of(*reset, kMain), tr(tile.name), context);
            else if (!input.nav_repeat)
                context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        }
        else if (pressed(input, Action::north) && main.load != model::Load::ready && main.load != model::Load::loading)
        {
            again(context);
        }
    }

    void ask_reset(const Item &reset, std::string_view name, UpdateContext &context)
    {
        Change change;
        change.catalog = reset.catalog;
        change.action = reset.row.id;
        change.done = filled(tr("{0} is back at its defaults"), name);
        const std::string &question = reset.row.fact("confirm");
        ask_first(std::move(change), filled(tr("Reset {0}?"), name),
                  question.empty() ? std::string(tr("Every setting of this part returns to its default for the profile in use.")) : question, tr("Reset"), context);
    }

    // Reads the page again after it could not be read.
    void again(UpdateContext &context)
    {
        for (Source &source : sources_)
        {
            context.services.data.forget(source.catalog);
            if (source.load != model::Load::ready)
            {
                source.load = model::Load::idle;
                source.mark = 0;
            }
        }
        context.feedback.play(audio::Cue::select);
    }

    // The end of a row or a list: one soft answer, none while the direction is held.
    void bump(const InputFrame &input, UpdateContext &context)
    {
        if (input.nav_repeat)
            return;
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        edge_.trigger(1.0f);
    }

    // ---- input: a page --------------------------------------------------------------

    void input_page(const InputFrame &input, UpdateContext &context)
    {
        if (sheet_.open)
        {
            input_sheet(input, context);
            return;
        }
        if (leave_ > 0.0f)
        {
            // The choice was confirmed: the list closes once the mark has been seen on it.
            leave_ -= context.dt;
            if (leave_ <= 0.0f || pressed(input, Action::back))
            {
                leave_ = 0.0f;
                step_back(context);
            }
            return;
        }
        const model::Load state = page_load();
        const Item *item = focused();
        if (state != model::Load::ready || item == nullptr)
        {
            // Nothing to move through: the one button, the rail and the way back.
            if (pressed(input, Action::back))
                context.nav.back();
            else if (input.nav == Direction::left)
                context.nav.open_rail();
            else if (pressed(input, Action::confirm) && state != model::Load::loading)
            {
                if (state == model::Load::ready)
                    context.nav.back();
                else
                    again(context);
            }
            else if (input.nav != Direction::none)
                bump(input, context);
            return;
        }
        read_choices(context.services.data);
        if (steps_ != 0 && choices_[item->row.id].ready)
        {
            // Presses made before the choices were here.
            const int owed = steps_;
            steps_ = 0;
            step(*item, owed, input, context);
        }
        // More rows of a long list arrive as the focus nears its end.
        Source &last = sources_.back();
        if (last.more && last.pages < kPagesAtMost && focus_ + 6 >= static_cast<int>(lines_.size()))
        {
            const Page &next = context.services.data.catalog(last.catalog, last.pages);
            if (next.load != model::Load::loading && next.load != model::Load::idle)
                ++last.pages;
        }

        if (grabbed_)
        {
            input_move(*item, input, context);
            return;
        }
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            go(input.nav == Direction::down ? 1 : -1, 1, input, context);
        }
        else if (pressed(input, Action::jump_next) || pressed(input, Action::jump_prev))
        {
            go(pressed(input, Action::jump_next) ? 1 : -1, 8, input, context);
        }
        else if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int direction = input.nav == Direction::right ? 1 : -1;
            switch (item->kind)
            {
            case Kind::toggle:
                if (shown(*item).on != (direction > 0))
                    flip(*item, context);
                else
                    bump(input, context);
                break;
            case Kind::list:
            case Kind::slider:
                step(*item, direction, input, context);
                break;
            case Kind::cycle:
                if (direction > 0)
                    activate(*item, input, context);
                else
                    bump(input, context);
                break;
            default:
                if (direction < 0)
                    context.nav.open_rail();
                else
                    bump(input, context);
                break;
            }
        }
        else if (pressed(input, Action::confirm))
        {
            activate(*item, input, context);
        }
        else if (pressed(input, Action::back))
        {
            step_back(context);
        }
        else if (pressed(input, Action::west) && item->kind == Kind::order)
        {
            show_or_hide(*item, input, context);
        }
        else if (pressed(input, Action::north))
        {
            if (item->kind == Kind::order)
                context.nav.push(Route{ScreenId::settings, item->row.id, below(item->label)}, Move::push);
            else if (has_reset_)
                ask_reset(reset_, path_.empty() ? std::string_view() : std::string_view(path_.back()), context);
            else if (!input.nav_repeat)
                context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        }
    }

    // Back to the page before. Leaving a page on which something was saved says so once.
    void step_back(UpdateContext &context)
    {
        if (applied_ > 0)
            context.feedback.play(audio::Cue::saved, 1.0f, 0.0f, 0.7f);
        context.nav.back();
    }

    // Moves the focus by rows; a held direction moves one row at a time and stops quietly at the end.
    void go(int direction, int rows, const InputFrame &input, UpdateContext &context)
    {
        int at = focus_;
        int reached = focus_;
        for (int left = rows; left > 0;)
        {
            at += direction;
            if (at < 0 || at >= static_cast<int>(lines_.size()))
                break;
            if (lines_[static_cast<std::size_t>(at)].item < 0)
                continue;
            reached = at;
            --left;
        }
        if (reached == focus_)
        {
            bump(input, context);
            return;
        }
        focus_ = reached;
        steps_ = 0;
        said_row_.clear();
        const Item &item = items_[static_cast<std::size_t>(lines_[static_cast<std::size_t>(focus_)].item)];
        focus_id_ = key_of(item);
        reveal();
        const float through = content_ > 0.0f ? lines_[static_cast<std::size_t>(focus_)].y / content_ : 0.0f;
        context.feedback.play(audio::Cue::focus, 1.08f - 0.14f * through, ui::pan_for_x(kFormX + kFormW * 0.5f));
    }

    void reveal()
    {
        if (focus_ < 0 || focus_ >= static_cast<int>(lines_.size()))
            return;
        const Line &line = lines_[static_cast<std::size_t>(focus_)];
        scroll_.reveal(line.y, line.y + line.h, kView, kRowH + kRowGap + 8.0f, content_);
    }

    std::string below(std::string_view name) const
    {
        std::string title;
        for (const std::string &part : path_)
        {
            title += part;
            title += kPathSeparator;
        }
        title += name;
        return title;
    }

    // A row that cannot be changed now: its help beside the form says why.
    void refuse(const Item &item, const InputFrame &input, UpdateContext &context)
    {
        (void)item;
        bump(input, context);
    }

    void flip(const Item &item, UpdateContext &context)
    {
        Change change;
        change.item = item.row.id;
        change.catalog = item.catalog;
        change.action = item.row.id;
        change.on = !shown(item).on;
        change.sets_on = true;
        if (ask(std::move(change), context))
            context.feedback.play(audio::Cue::toggle, shown(item).on ? 1.08f : 0.94f, ui::pan_for_x(kFormX + kFormW));
    }

    // Left or Right on a row that has choices: the one before or after the one it shows.
    void step(const Item &item, int direction, const InputFrame &input, UpdateContext &context)
    {
        if (item.disabled)
        {
            refuse(item, input, context);
            return;
        }
        const Choices &choices = choices_[item.row.id];
        if (!choices.ready)
        {
            steps_ = std::clamp(steps_ + direction, -24, 24); // taken when its choices are here
            return;
        }
        const int count = static_cast<int>(choices.rows.size());
        if (count < 2)
        {
            bump(input, context);
            return;
        }
        int now = shown(item).index;
        if (now < 0 || now >= count)
        {
            now = 0;
            for (int i = 0; i < count; ++i)
            {
                if (choices.rows[static_cast<std::size_t>(i)].on)
                    now = i;
            }
        }
        int next = now + direction;
        if (item.kind == Kind::slider)
            next = std::clamp(next, 0, count - 1); // a quantity has two ends
        else
            next = ((next % count) + count) % count; // a list goes round
        Motion &motion = motion_[key_of(item)];
        if (next == now)
        {
            bump(input, context);
            return;
        }
        const Item &choice = choices.rows[static_cast<std::size_t>(next)];
        Change change;
        change.item = item.row.id;
        change.catalog = item.catalog;
        change.action = choice.row.id;
        change.value = choice.label;
        change.sets_value = true;
        change.index = next;
        change.choice = choice.row;
        change.has_choice = true;
        change.absolute = true;
        if (!ask(std::move(change), context))
            return;
        motion.nudge.trigger(1.0f);
        motion.direction = direction > 0 ? 1.0f : -1.0f;
        if (item.kind == Kind::slider)
            context.feedback.play(audio::Cue::slider, 0.9f + 0.3f * static_cast<float>(next) / static_cast<float>(count - 1), ui::pan_for_x(kFormX + kFormW));
        else
            context.feedback.play(audio::Cue::toggle, direction > 0 ? 1.04f : 0.96f, ui::pan_for_x(kFormX + kFormW));
    }

    void activate(const Item &item, const InputFrame &input, UpdateContext &context)
    {
        if (item.disabled)
        {
            refuse(item, input, context);
            return;
        }
        switch (item.kind)
        {
        case Kind::toggle:
            flip(item, context);
            break;
        case Kind::list:
        case Kind::slider:
        case Kind::page:
            // A row's choices, or a further page: the list that has the row's id.
            context.nav.push(Route{ScreenId::settings, item.row.id, below(item.label)}, Move::push);
            break;
        case Kind::cycle:
        {
            Change change;
            change.item = item.row.id;
            change.catalog = item.catalog;
            change.action = item.row.id;
            if (ask(std::move(change), context))
            {
                Motion &motion = motion_[key_of(item)];
                motion.nudge.trigger(1.0f);
                motion.direction = 1.0f;
                context.feedback.play(audio::Cue::toggle, 1.04f, ui::pan_for_x(kFormX + kFormW));
            }
            break;
        }
        case Kind::choice:
        {
            if (shown(item).on)
            {
                step_back(context); // it already stands
                break;
            }
            Change change;
            change.item = item.row.id;
            change.catalog = item.catalog;
            change.action = item.row.id;
            change.on = true;
            change.sets_on = true;
            change.leave = true;
            if (ask(std::move(change), context))
                context.feedback.play(audio::Cue::select);
            break;
        }
        case Kind::action:
        {
            Change change;
            change.item = item.row.id;
            change.catalog = item.catalog;
            change.action = item.row.id;
            change.done = item.row.fact("done");
            const std::string &question = item.row.fact("confirm");
            if (!question.empty())
            {
                const std::string &yes = item.row.fact("confirm_label");
                ask_first(std::move(change), item.label, question, yes.empty() ? std::string(tr("Confirm")) : yes, context);
            }
            else if (ask(std::move(change), context))
            {
                context.feedback.play(audio::Cue::select);
            }
            break;
        }
        case Kind::text:
            sheet_.open = true;
            sheet_.item = item;
            sheet_.text = item.row.has("secret") ? std::string() : item.row.fact("text").empty() ? item.value : item.row.fact("text");
            sheet_.keys.style.bindings = ui::KeyboardBindings::standard();
            sheet_.keys.style.panel = false;
            sheet_.keys.style.reduced_motion = context.reduced_motion;
            sheet_.keys.set_bounds(kSheetKeys);
            sheet_.keys.set_length(-1);
            sheet_.keys.enter();
            context.feedback.play(audio::Cue::modal_open);
            break;
        case Kind::link:
        {
            ScreenId target = ScreenId::reader;
            const std::string &argument = item.row.fact("argument");
            if (!screen_from_name(item.row.fact("screen"), &target))
                target = ScreenId::reader; // a text to read: credits, changes, release notes, an address
            if (target == ScreenId::settings)
                context.nav.push(Route{target, argument.empty() ? item.row.id : argument, below(item.label)}, Move::push);
            else
                context.nav.push(Route{target, !argument.empty() ? argument : target == ScreenId::reader ? item.row.id : std::string(), item.label}, Move::push);
            break;
        }
        case Kind::order:
            grabbed_ = true;
            context.feedback.play(audio::Cue::pickup);
            break;
        case Kind::info:
        case Kind::reset:
            if (!input.nav_repeat)
                context.feedback.play(audio::Cue::tick, 1.0f, 0.0f, 0.6f);
            break;
        }
    }

    // ---- input: a row held in the move mode -----------------------------------------

    void input_move(const Item &item, const InputFrame &input, UpdateContext &context)
    {
        if (item.kind != Kind::order)
        {
            grabbed_ = false;
            return;
        }
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            move(item, input.nav == Direction::down ? 1 : -1, input, context);
        }
        else if (pressed(input, Action::confirm) || pressed(input, Action::back))
        {
            grabbed_ = false;
            context.feedback.play(audio::Cue::drop);
        }
        else if (pressed(input, Action::west))
        {
            show_or_hide(item, input, context);
        }
        else if (input.nav != Direction::none)
        {
            bump(input, context);
        }
    }

    static bool can_move(const Item &item, int direction)
    {
        return !item.row.fact(direction < 0 ? "up" : "down").empty() && !item.row.has(direction < 0 ? "first" : "last");
    }

    void move(const Item &item, int direction, const InputFrame &input, UpdateContext &context)
    {
        const int at = lines_[static_cast<std::size_t>(focus_)].item;
        const int other = at + direction;
        Source *source = source_of(item.catalog);
        if (!can_move(item, direction) || source == nullptr || other < 0 || other >= static_cast<int>(items_.size()) ||
            items_[static_cast<std::size_t>(other)].kind != Kind::order || items_[static_cast<std::size_t>(other)].section != item.section)
        {
            bump(input, context);
            return;
        }
        const std::string mine = item.row.id;
        const std::string theirs = items_[static_cast<std::size_t>(other)].row.id;
        const auto a = std::find_if(source->rows.begin(), source->rows.end(), [&](const model::Row &row) { return row.id == mine; });
        const auto b = std::find_if(source->rows.begin(), source->rows.end(), [&](const model::Row &row) { return row.id == theirs; });
        if (a == source->rows.end() || b == source->rows.end())
            return;
        Change change;
        change.item = mine;
        change.catalog = item.catalog;
        change.action = item.row.fact(direction < 0 ? "up" : "down");
        change.moved = true;
        change.before = source->rows;
        if (!ask(std::move(change), context))
            return;
        // The row is where it was put at once; which ends it now stands at follows from the row it passed.
        const bool was_end = !can_move(items_[static_cast<std::size_t>(other)], direction);
        set_fact(*a, direction < 0 ? "first" : "last", was_end ? "1" : "0");
        set_fact(*a, direction < 0 ? "last" : "first", "0");
        set_fact(*b, direction < 0 ? "first" : "last", "0");
        set_fact(*b, direction < 0 ? "last" : "first", can_move(item, -direction) ? "0" : "1");
        std::iter_swap(a, b);
        rebuild();
        reveal();
        context.feedback.play(audio::Cue::slide, direction > 0 ? 0.96f : 1.04f, ui::pan_for_x(kFormX + kFormW * 0.5f));
    }

    void show_or_hide(const Item &item, const InputFrame &input, UpdateContext &context)
    {
        const std::string &action = item.row.fact("toggle");
        if (action.empty())
        {
            bump(input, context);
            return;
        }
        Change change;
        change.item = item.row.id;
        change.catalog = item.catalog;
        change.action = action;
        change.on = !shown(item).on;
        change.sets_on = true;
        if (ask(std::move(change), context))
            context.feedback.play(audio::Cue::toggle, shown(item).on ? 1.08f : 0.94f, ui::pan_for_x(kFormX + kFormW));
    }

    // ---- input: typing --------------------------------------------------------------

    void input_sheet(const InputFrame &input, UpdateContext &context)
    {
        if (pressed(input, Action::back))
        {
            sheet_.open = false;
            context.feedback.play(audio::Cue::modal_close);
            return;
        }
        ui::Event event = ui::Event::none;
        if (pressed(input, Action::menu))
            event = ui::Event::activated; // Options saves from anywhere on the keyboard
        else
            event = sheet_.keys.handle(input, context.feedback);
        for (int i = 0; i < sheet_.keys.erased(); ++i)
            drop_last_character(sheet_.text);
        sheet_.text += sheet_.keys.typed();
        if (event != ui::Event::activated)
            return;
        sheet_.open = false;
        Change change;
        change.item = sheet_.item.row.id;
        change.catalog = sheet_.item.catalog;
        change.action = sheet_.item.row.id;
        if (!sheet_.item.row.has("secret"))
        {
            change.value = sheet_.text;
            change.sets_value = true;
        }
        if (ask(std::move(change), context))
            context.feedback.play(audio::Cue::select);
    }

    // ---- what moves -----------------------------------------------------------------

    void advance(UpdateContext &context)
    {
        const float dt = context.dt;
        const bool reduced = context.reduced_motion;
        const float quick = reduced ? 60.0f : 20.0f;
        edge_.update(dt, 10.0f);
        if (home_)
        {
            // A fourth row of tiles scrolls the grid; three rows never move.
            const int row = focus_ / kTileColumns;
            tiles_scroll_.target = shown_.size() > 12 ? static_cast<float>(std::max(0, row - 2)) * kTilePitchY : 0.0f;
            tiles_scroll_.update(dt, reduced ? 60.0f : 12.0f);
            const Rect target = grown(tile_rect(focus_, tiles_scroll_.target), 0.03f, 4.0f);
            if (age_ < 0.1f)
                ring_.snap(target);
            else
                ring_.target(target);
            ring_.update(dt, quick);
            return;
        }
        scroll_.update(dt, reduced ? 60.0f : 14.0f);
        if (focus_ >= 0 && focus_ < static_cast<int>(lines_.size()))
            plate_.target = lines_[static_cast<std::size_t>(focus_)].y;
        plate_.update(dt, quick);
        grab_.target = grabbed_ ? 1.0f : 0.0f;
        grab_.update(dt, reduced ? 60.0f : 16.0f);
        sheet_.amount.target = sheet_.open ? 1.0f : 0.0f;
        sheet_.amount.update(dt, reduced ? 60.0f : 13.0f);
        if (sheet_.amount.value > 0.01f)
            sheet_.keys.update(dt);
        for (const Item &item : items_)
        {
            Motion &motion = motion_[key_of(item)];
            const Shown now = shown(item);
            motion.on.target = now.on ? 1.0f : 0.0f;
            float fill = number_of(item.row.fact("fill"), 0.0f);
            if (item.count > 1 && now.index >= 0)
                fill = static_cast<float>(now.index) / static_cast<float>(item.count - 1);
            motion.fill.target = fill;
            motion.waiting.target = now.waiting ? 1.0f : 0.0f;
            if (!motion.placed)
            {
                motion.on.snap(motion.on.target);
                motion.fill.snap(fill);
                motion.placed = true;
            }
            if (reduced)
                motion.on.snap(motion.on.target);
            else
                motion.on.update(dt, 18.0f, 0.62f);
            motion.fill.update(dt, quick);
            motion.waiting.update(dt, 9.0f);
            motion.applied.update(dt, 2.6f);
            motion.refused.update(dt, 7.0f);
            motion.nudge.update(dt, 9.0f);
        }
        preview(reduced, dt);
    }

    // What the right-hand picture shows: every row that feeds it says so with its fact "preview".
    void preview(bool reduced, float dt)
    {
        PreviewState next;
        const Item *under = focused();
        const auto kind_of = [](const std::string &what)
        {
            return starts_with(what, "caption") ? PreviewKind::captions : what == "theme" ? PreviewKind::theme : starts_with(what, "card") ? PreviewKind::card : PreviewKind::none;
        };
        PreviewKind first = PreviewKind::none;
        for (const Item &item : items_)
        {
            const std::string &what = item.row.fact("preview");
            if (!item.row.fact("preview_art_url").empty())
                next.picture = item.row.fact("preview_art_url");
            if (what.empty())
                continue;
            if (first == PreviewKind::none)
                first = kind_of(what);
            // On a list of choices only the one under the focus speaks: moving through the list tries each on.
            if (item.kind == Kind::choice && &item != under)
                continue;
            const Shown now = shown(item);
            const model::Row &row = now.choice != nullptr ? *now.choice : item.row;
            if (what == "caption.on")
                next.captions = now.on;
            else if (what == "caption.size")
                next.scale = number_of(row.fact("number"), 100.0f) / 100.0f;
            else if (what == "caption.position")
                next.position = number_of(row.fact("number"), -1.0f) < 0.0f ? -1.0f : number_of(row.fact("number"), 0.0f) / 1000.0f;
            else if (what == "caption.style")
            {
                next.text = colour_of(row.fact("text_rgb"), kWhite);
                next.plate = number_of(row.fact("background_alpha"), 140.0f) / 255.0f;
                next.edge = row.fact("edge") == "outline" ? 1 : row.fact("edge") == "shadow" ? 2 : 0;
            }
            else if (what == "theme")
            {
                next.look = row.fact("look");
                next.name = item.kind == Kind::choice ? item.label : now.value;
                next.ground = colour_of(row.fact("background"), kBlack);
                next.accent = colour_of(row.fact("accent"), kWhite);
            }
            else if (what == "card.scale")
                next.interface_scale = number_of(row.fact("number"), 100.0f) / 100.0f;
            else if (what == "card.grid")
                next.grid_scale = number_of(row.fact("number"), 100.0f) / 100.0f;
            else if (what == "card.title_lines")
                next.title_lines = static_cast<int>(number_of(row.fact("number"), 1.0f));
            else if (what == "card.subtitle_lines")
                next.subtitle_lines = item.kind == Kind::toggle ? (now.on ? 2 : 1) : static_cast<int>(number_of(row.fact("number"), 1.0f));
            else if (what == "card.corners")
                next.rounded = now.on;
        }
        // The picture follows the row under the focus, and stays when the focus is on a row that feeds none.
        if (under != nullptr && kind_of(under->row.fact("preview")) != PreviewKind::none)
            sticky_ = kind_of(under->row.fact("preview"));
        else if (sticky_ == PreviewKind::none)
            sticky_ = first;
        next.kind = first == PreviewKind::none ? PreviewKind::none : sticky_;
        // Sizes and places glide to what was chosen.
        look_scale_.target = next.scale;
        look_position_.target = next.position;
        look_plate_.target = next.plate;
        look_interface_.target = next.interface_scale;
        look_grid_.target = next.grid_scale;
        if (!preview_placed_ || reduced || (preview_.position < 0.0f) != (next.position < 0.0f))
        {
            look_scale_.snap(next.scale);
            look_position_.snap(next.position);
            look_plate_.snap(next.plate);
            look_interface_.snap(next.interface_scale);
            look_grid_.snap(next.grid_scale);
            preview_placed_ = next.kind != PreviewKind::none;
        }
        for (tween::Spring *spring : {&look_scale_, &look_position_, &look_plate_, &look_interface_, &look_grid_})
            spring->update(dt, 16.0f);
        preview_ = std::move(next);
        preview_.scale = look_scale_.value;
        preview_.position = look_position_.value;
        preview_.plate = look_plate_.value;
        preview_.interface_scale = look_interface_.value;
        preview_.grid_scale = look_grid_.value;
    }

    void make_hints()
    {
        hint_count_ = 0;
        const auto add = [&](ui::Button button, const char *label, ui::Button second = ui::Button::none)
        {
            if (hint_count_ < hints_.size())
                hints_[hint_count_++] = {button, label, second};
        };
        if (sheet_.open)
        {
            add(ui::Button::cross, tr("Type"));
            add(ui::Button::square, tr("Delete"));
            add(ui::Button::options, tr("Save"));
            add(ui::Button::circle, tr("Cancel"));
            return;
        }
        if (home_)
        {
            const Source &main = sources_.front();
            add(ui::Button::cross, tr("Open"));
            add(ui::Button::circle, tr("Back"));
            if (main.load != model::Load::ready && main.load != model::Load::loading && main.load != model::Load::idle)
                add(ui::Button::triangle, tr("Try again"));
            else if (!shown_.empty() && tile_reset(tiles()[static_cast<std::size_t>(shown_[static_cast<std::size_t>(focus_)])], main.rows) != nullptr)
                add(ui::Button::options, tr("Reset a section"));
            return;
        }
        const Item *item = page_load() == model::Load::ready ? focused() : nullptr;
        if (item == nullptr)
        {
            if (page_load() != model::Load::loading)
                add(ui::Button::cross, page_load() == model::Load::ready ? tr("Back to Settings") : tr("Try again"));
            add(ui::Button::circle, tr("Back"));
            return;
        }
        if (grabbed_)
        {
            add(ui::Button::dpad, tr("Move"));
            add(ui::Button::square, tr("Show or hide"));
            add(ui::Button::cross, tr("Done"));
            return;
        }
        switch (item->kind)
        {
        case Kind::toggle:
            add(ui::Button::dpad, tr("Change"));
            add(ui::Button::cross, tr("Switch"));
            break;
        case Kind::list:
        case Kind::slider:
            add(ui::Button::dpad, tr("Change"));
            add(ui::Button::cross, tr("All choices"));
            break;
        case Kind::cycle:
            add(ui::Button::cross, tr("Next"));
            break;
        case Kind::choice:
            add(ui::Button::cross, tr("Choose"));
            break;
        case Kind::action:
            add(ui::Button::cross, tr("Select"));
            break;
        case Kind::text:
            add(ui::Button::cross, tr("Edit"));
            break;
        case Kind::page:
        case Kind::link:
            add(ui::Button::cross, tr("Open"));
            break;
        case Kind::order:
            add(ui::Button::cross, tr("Move"));
            add(ui::Button::square, tr("Show or hide"));
            add(ui::Button::triangle, tr("More"));
            break;
        case Kind::info:
        case Kind::reset:
            break;
        }
        if (has_reset_ && item->kind != Kind::order)
            add(ui::Button::triangle, tr("Defaults"));
        add(ui::Button::circle, tr("Back"));
    }

    // ---- drawing --------------------------------------------------------------------

    float arrive(int index, float step = 0.05f, float duration = 0.45f) const
    {
        return tween::stagger(age_, index, step, duration);
    }

    void draw_home(G &g) const
    {
        const Look &k = g.k;
        const Source &main = sources_.front();
        const bool loading = main.load == model::Load::idle || main.load == model::Load::loading;
        {
            // REQUEST: a line under the title in the frame's header (requests/e.md, 3).
            Enter enter(g, arrive(0, 0.05f, 0.5f), -14.0f);
            const std::string who = profile_.guest || profile_.name.empty() ? std::string(tr("Guest")) : profile_.name;
            text(g, g.f.regular, fit(g.f.regular, filled(tr("For {0} \xC2\xB7 saved on this PS5"), who), 22.0f, 1200.0f), kLeft, 166.0f, 22.0f, k.ink2);
        }
        const bool scrolls = shown_.size() > 12;
        const float scroll = tiles_scroll_.value;
        if (scrolls)
            g.l.push_clip({kRail + 8.0f, 196.0f, gfx::kVirtualWidth - kRail - 8.0f, 772.0f});
        const Rect ring = ring_.value();
        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::size_t i = 0; i < shown_.size(); ++i)
            {
                const bool has_focus = static_cast<int>(i) == focus_;
                if ((pass == 1) != has_focus)
                    continue;
                const Tile &tile = tiles()[static_cast<std::size_t>(shown_[i])];
                const Rect base = tile_rect(static_cast<int>(i), scroll);
                const float focus = tween::clamp01(1.0f - std::hypot(ring.cx() - base.cx(), ring.cy() - base.cy() + 4.0f) / 220.0f);
                Enter enter(g, arrive(1 + static_cast<int>(i) % kTileColumns + static_cast<int>(i) / kTileColumns, 0.055f, 0.45f), 28.0f);
                if (loading && main.rows.empty())
                    draw_tile_bones(g, base, 0.11f * static_cast<float>(i));
                else
                    draw_tile(g, base, tile, tr(tile.name),
                              shown_[i] + 1 == static_cast<int>(tiles().size()) ? filled(tr("{0} further settings"), std::to_string(unknown_)) : tile_summary(tile, main.rows),
                              focus, tile_attention(tile, main.rows), false);
            }
        }
        const float nudge = g.reduced ? 0.0f : ui::shake(edge_.value, g.time);
        focus_ring(g, {ring.x + nudge, ring.y, ring.w, ring.h}, kRadiusPanel, k.accent, arrive(4, 0.05f, 0.4f));
        {
            Enter enter(g, arrive(6, 0.06f, 0.5f), 16.0f);
            const float y = kTilesTop + static_cast<float>((shown_.size() + kTileColumns - 1) / kTileColumns) * kTilePitchY - scroll + 40.0f;
            if (main.load == model::Load::failed || main.load == model::Load::offline)
            {
                // The tiles stay: what is another screen's still opens. This line says what is missing.
                icon(g, main.load == model::Load::offline ? Icon::offline : Icon::alert, kLeft + 14.0f, y - 8.0f, 28.0f, k.ink2);
                const std::string words = main.error.empty() ? std::string(tr("The settings could not be read. Triangle reads them again.")) : main.error;
                text(g, g.f.regular, fit(g.f.regular, words, 21.0f, kRight - kLeft - 44.0f), kLeft + 44.0f, y, 21.0f, k.ink2);
            }
            else if (!loading && main.rows.empty())
            {
                text(g, g.f.regular, fit(g.f.regular, tr("ProsperoTube listed no settings for this profile. Accounts and Phone still open."), 21.0f, kRight - kLeft), kLeft, y, 21.0f,
                     k.ink2);
            }
            else
            {
                text(g, g.f.regular, fit(g.f.regular, tr("Settings follow the profile in use. Interface and updates apply to the console."), 21.0f, kRight - kLeft), kLeft, y,
                     21.0f, k.ink3);
            }
        }
        if (scrolls)
            g.l.pop_clip();
    }

    void draw_page(G &g, const DrawContext &context) const
    {
        const Look &k = g.k;
        draw_path(g, path_, arrive(0, 0.05f, 0.5f));
        const model::Load state = page_load();
        if (state == model::Load::loading)
        {
            draw_form_bones(g, arrive(1, 0.06f, 0.4f));
            return;
        }
        if (state != model::Load::ready || items_.empty())
        {
            Enter enter(g, arrive(1, 0.06f, 0.5f), 22.0f);
            if (state == model::Load::ready)
                draw_notice(g, Icon::info, tr("Nothing to set here"), tr("This page has no settings for the profile in use. Other pages of Settings are not affected."),
                            tr("Back to Settings"), 1.0f, kNoticeButton);
            else
                draw_notice(g, state == model::Load::offline ? Icon::offline : Icon::alert,
                            state == model::Load::offline ? tr("These settings cannot be reached") : tr("These settings could not be read"),
                            page_error().empty() ? std::string(tr("Nothing was changed. What is saved on this PS5 stays as it is.")) : page_error(), tr("Try again"), 1.0f,
                            kNoticeButton);
            return;
        }

        const float scroll = scroll_.offset();
        const Item *under = focused();
        {
            Enter enter(g, arrive(1, 0.06f, 0.5f), 22.0f);
            g.l.push_clip({kFormX - 14.0f, kFormTop - 8.0f, kFormW + 28.0f, kView + 16.0f});
            // The plate glides with the focus; a row held in the move mode lifts on it.
            const float nudge = g.reduced ? 0.0f : ui::shake(edge_.value, g.time);
            const float refusal = under != nullptr ? shake_of(*under, g) : 0.0f;
            draw_plate(g, {kFormX + nudge + refusal, kFormTop + plate_.value - scroll, kFormW, kRowH}, 1.0f, grab_.value);
            int seen = 0;
            for (const Line &line : lines_)
            {
                const float y = kFormTop + line.y - scroll;
                if (y + line.h < kFormTop - 30.0f || y > kFormBottom + 30.0f)
                    continue;
                // A row the edge of the form cuts fades: gone when half of it is cut. Never the row under the
                // focus: that one is on its way into view.
                const float cut = std::max(kFormTop - 2.0f - y, y + line.h - kFormBottom - 2.0f);
                const bool is_under = line.item >= 0 && under == &items_[static_cast<std::size_t>(line.item)];
                const float whole = is_under ? 1.0f : tween::clamp01((0.5f - cut / line.h) / 0.4f);
                if (whole <= 0.01f)
                    continue;
                Faded faded(g, whole);
                const float amount = tween::stagger(age_, 2 + seen++, 0.03f, 0.35f);
                Enter row_enter(g, amount, 12.0f);
                if (line.item < 0)
                {
                    draw_heading(g, {kFormX, y, kFormW, line.h}, line.heading);
                    continue;
                }
                const Item &item = items_[static_cast<std::size_t>(line.item)];
                const Shown now = shown(item);
                const auto found = motion_.find(key_of(item));
                RowLook look;
                look.value = now.value;
                if (item.kind == Kind::order)
                    look.value = now.on ? tr("Shown") : tr("Hidden");
                else if (item.kind == Kind::text && item.row.has("secret") && !now.value.empty())
                    look.value = "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2";
                look.focus = tween::clamp01(1.0f - std::fabs(plate_.value - line.y) / 60.0f);
                look.on = now.on ? 1.0f : 0.0f;
                if (found != motion_.end())
                {
                    const Motion &motion = found->second;
                    look.on = motion.on.value;
                    look.fill = motion.fill.value;
                    look.pending = tween::clamp01(motion.waiting.value * 1.6f - 0.6f); // a change the provider answers at once never shows as waiting
                    look.applied = motion.applied.value;
                    look.nudge = motion.nudge.value * motion.direction;
                }
                const bool held = grabbed_ && &item == under;
                look.grabbed = held ? grab_.value : 0.0f;
                look.can_up = can_move(item, -1);
                look.can_down = can_move(item, 1);
                const float shift = &item == under ? nudge + refusal : shake_of(item, g);
                draw_row(g, {kFormX + shift, y, kFormW, kRowH}, item, look);
            }
            // More of a long list is on its way: its shape under the last row.
            if (sources_.back().more)
            {
                const float y = kFormTop + content_ + kRowGap - scroll;
                if (y < kFormBottom)
                {
                    const float pulse = g.reduced ? 0.8f : 0.6f + 0.4f * ui::breathe(g.time, 1.7f);
                    g.l.rounded_rect({kLeft, y + 24.0f, 260.0f, 22.0f}, 11.0f, k.raised.with_alpha(k.raised.a * pulse));
                }
            }
            g.l.pop_clip();
            if (content_ > kView + 1.0f)
            {
                // How far down the page the view is.
                const Rect track{1039.0f, kFormTop + 8.0f, 3.0f, kView - 16.0f};
                g.l.rounded_rect(track, 1.5f, k.hairline);
                const float thumb = std::max(64.0f, track.h * kView / content_);
                g.l.rounded_rect({track.x - 1.0f, track.y + (track.h - thumb) * tween::clamp01(scroll / (content_ - kView)), 5.0f, thumb}, 2.5f, k.ink3);
            }
        }
        {
            Enter enter(g, arrive(2, 0.07f, 0.55f), 0.0f, 44.0f);
            const bool pictured = preview_.kind != PreviewKind::none;
            if (pictured)
                draw_preview(g, kPreviewBox, preview_, context.images.get(preview_.picture, PictureSize::hero), 1.0f);
            // The focused row explains itself here, in full sentences.
            const Rect help = pictured ? Rect{kPreviewBox.x, kPreviewBox.y + kPreviewBox.h + 28.0f, kPreviewBox.w, 148.0f}
                                       : Rect{kPreviewBox.x, kPreviewBox.y, kPreviewBox.w, under != nullptr && !under->row.overview.empty() ? 330.0f : 148.0f};
            if (under != nullptr && !said_row_.empty() && said_row_ == key_of(*under))
            {
                draw_help(g, help, said_, {}, true);
            }
            else
            {
                const std::string words = under != nullptr && !under->detail.empty() ? under->detail : std::string(tr("Changes are saved as you make them, for this profile."));
                draw_help(g, help, words, under != nullptr ? std::string_view(under->row.overview) : std::string_view());
            }
        }
    }

    // A refused change shakes its row.
    float shake_of(const Item &item, const G &g) const
    {
        const auto found = motion_.find(key_of(item));
        return found == motion_.end() || g.reduced ? 0.0f : ui::shake(found->second.refused.value, g.time, 12.0f);
    }

    void draw_sheet(G &g, const DrawContext &context) const
    {
        const Look &k = g.k;
        const float amount = tween::clamp01(sheet_.amount.value);
        g.l.push_opacity(amount);
        g.l.rounded_rect({kRail, 0.0f, gfx::kVirtualWidth - kRail, gfx::kVirtualHeight}, 0.0f, kBlack.with_alpha(k.dark ? 0.62f : 0.3f));
        g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, g.reduced ? 0.0f : 40.0f * (1.0f - amount));
        lift(g, kSheet, 36.0f, 1.0f);
        glass_panel(g, kSheet, 36.0f, 1.5f);
        text(g, g.f.display, fit(g.f.display, sheet_.item.label, 34.0f, kSheet.w - 88.0f), kSheet.x + 44.0f, kSheet.y + 66.0f, 34.0f, k.ink);
        text(g, g.f.regular, fit(g.f.regular, sheet_.item.detail, kTypeMeta, kSheet.w - 88.0f), kSheet.x + 44.0f, kSheet.y + 100.0f, kTypeMeta, k.ink2);
        const Rect field{kSheet.x + 44.0f, kSheet.y + 124.0f, kSheet.w - 88.0f, 64.0f};
        g.l.rounded_rect(field, 16.0f, k.raised);
        g.l.bordered_rect(field, 16.0f, clear(), 2.0f, k.focus.with_alpha(0.6f));
        std::string typed = sheet_.text;
        if (sheet_.item.row.has("secret"))
        {
            typed.clear();
            for (std::size_t i = 0; i < sheet_.text.size(); ++i)
            {
                if ((static_cast<unsigned char>(sheet_.text[i]) & 0xC0u) != 0x80u)
                    typed += "\xE2\x80\xA2";
            }
        }
        // The end of a long text stays in view: what does not fit leaves on the left.
        g.l.push_clip(field.inset(14.0f));
        const float width = measure(g.f.mono, typed, kTypeBody);
        const float room = field.w - 44.0f;
        const float x = field.x + 20.0f - std::max(0.0f, width - room);
        text(g, g.f.mono, typed, x, field.cy() + 8.5f, kTypeBody, k.ink);
        if (g.reduced || std::fmod(g.time, 1.0f) < 0.55f)
            g.l.rounded_rect({x + width + 3.0f, field.cy() - 16.0f, 2.5f, 32.0f}, 1.0f, k.accent);
        g.l.pop_clip();
        // The kit's keyboard, in the look's theme.
        ui::Keyboard keys = sheet_.keys;
        keys.style.theme = context.look.theme;
        keys.style.sounds.set = context.look.theme.sounds;
        ui::Canvas canvas{g.l, context.fonts, context.glass, context.time};
        keys.draw(canvas);
        g.l.pop_transform();
        g.l.pop_opacity();
    }

    static constexpr Rect kSheet{510.0f, 150.0f, 900.0f, 720.0f};
    static constexpr Rect kSheetKeys{554.0f, 372.0f, 812.0f, 460.0f};

    Route route_;
    model::Profile profile_;
    bool home_ = true;
    int tile_ = -1;                 // a tile's page: its place among tiles()
    std::vector<std::string> path_; // the names of the pages from Settings down to this one
    std::vector<Source> sources_;   // the first is the main list on the tiles and on a tile's page
    std::vector<int> shown_;        // the tiles on the page, as places among tiles()
    std::vector<Item> items_;
    std::vector<Line> lines_;
    Item reset_; // the row that puts this page back to its defaults
    bool has_reset_ = false;
    float content_ = 0.0f;
    int unknown_ = 0;      // rows of the main list no rule knows: the "More" tile
    int focus_ = 0;        // the tiles: a place among shown_; a page: a place among lines_
    std::string focus_id_; // the row under the focus, so it is found again when the rows change
    bool placed_ = false;

    std::deque<Change> changes_;
    Change asking_; // the change the dialog is asking about
    std::map<std::string, Choices> choices_;
    std::map<std::string, Motion> motion_;
    int steps_ = 0;
    bool grabbed_ = false;
    float leave_ = 0.0f;
    int applied_ = 0;
    int refused_ = 0;
    std::string last_applied_;
    std::string last_refused_;
    std::string said_row_; // the row whose change the provider refused ...
    std::string said_;     // ... and what it said

    float age_ = 0.0f;
    ui::Scroller scroll_;
    tween::Spring tiles_scroll_;
    tween::Spring plate_; // the focus plate, down the page's content
    tween::Spring grab_;
    ui::SpringRect ring_;
    ui::Pulse edge_;
    PreviewKind sticky_ = PreviewKind::none;
    PreviewState preview_;
    bool preview_placed_ = false;
    tween::Spring look_scale_, look_position_, look_plate_, look_interface_, look_grid_;
    TextSheet sheet_;
    std::array<ui::Hint, 6> hints_{};
    std::size_t hint_count_ = 0;
    std::shared_ptr<int> alive_;
};

} // namespace

std::unique_ptr<Screen> make_settings()
{
    return std::make_unique<Settings>();
}

} // namespace afterglow
