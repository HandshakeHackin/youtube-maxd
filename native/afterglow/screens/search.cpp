// ProsperoTube Afterglow - Search: the field, the keyboard and what they find while you type.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Three zones share the page. The keyboard (the kit's own) feeds the field;
// left of it the field with its list, which holds the profile's recent
// searches while nothing is typed and completions afterwards; under both a
// strip of the best matches so far. route.argument is the text the field
// starts with. Everything the page shows is read through the data layer:
// "search-history", "search-suggest:<text>" and "search-preview:<text>"
// (screens/search.hpp); a submitted query opens the results screen.

#include "afterglow/screens/search.hpp"

#include "ui/components.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace afterglow
{

namespace
{

using search::fill;
using search::starts_with;
using search::trimmed;

constexpr Rect kKeys{928.0f, 196.0f, 896.0f, 452.0f};
constexpr float kFieldTop = 196.0f;
constexpr float kFieldWidth = 704.0f;
constexpr float kStripTitle = 716.0f; // baseline of "Best matches"
constexpr float kStripTop = 740.0f;
constexpr int kStripCards = 10;
constexpr int kStripBones = 5;
constexpr float kDebounce = 0.12f; // seconds after the last key before anything is asked
constexpr int kRecentRows = 5;

char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// The provider's own test of two queries being one (ASCII case apart).
bool same_query(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return lower(x) == lower(y); });
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

class Search final : public Screen
{
  public:
    Search()
    {
        keys_.style.bindings = ui::KeyboardBindings::standard();
        keys_.style.gap = 10.0f;
        keys_.style.panel_padding = 20.0f;
        // Left of the first column is the list, under the last row the best matches.
        keys_.style.exits.left = true;
        keys_.style.exits.down = true;
        // Opening the results is the frame's sound; Done adds none of its own.
        keys_.style.sounds.activate = audio::Cue::count;
        keys_.set_bounds(kKeys);
        keys_.on_text = [this](std::string_view utf8)
        {
            if (field_.insert(utf8))
                edited();
        };
        keys_.on_backspace = [this]
        {
            if (field_.backspace())
                edited();
        };

        field_.style.pill = true;
        field_.style.keep_open = true;
        field_.style.field_height = 68.0f;
        field_.style.row_height = 58.0f;
        field_.style.text_size = 27.0f;
        field_.style.row_size = 24.0f;
        field_.style.detail_size = 20.0f;
        field_.style.max_rows = kRecentRows;
        field_.style.max_recent = kRecentRows;
        field_.style.max_length = 120;
        field_.style.debounce = kDebounce;
        field_.style.remember = false; // the provider keeps the history, not the field
        field_.style.exits.left = true;
        field_.style.exits.right = true;
        field_.style.exits.down = true;
        field_.style.sounds.activate = audio::Cue::count;
        field_.provider = [this](std::string_view query, std::vector<ui::Suggestion> &out) { provide(query, out); };
        field_.set_bounds({kLeft, kFieldTop, kFieldWidth, field_.preferred_height()});
        field_.set_active(true);

        for (ui::Skeleton &bone : bones_)
        {
            bone.style.kind = ui::SkeletonKind::block;
            bone.style.radius = kRadiusThumb;
        }
        ring_.snap(card_rect(0, 0.0f));
    }

    ScreenId id() const override
    {
        return ScreenId::search;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        data_ = &context.services.data;
        if (restored)
        {
            // Back from the results or a video: the text, the zone and the focus stay.
            if (!field_.text().empty() && field_.suggestions().empty())
                field_.refresh(); // a picked completion closed the list: open it again
            return;
        }
        route_ = route;
        age_ = 0.0f;
        zone_ = Zone::keys;
        return_zone_ = Zone::keys;
        match_index_ = 0;
        scroll_.position.snap(0.0f);
        shown_.clear();
        waiting_ = false;
        field_.set_text(trimmed(route.argument));
        field_.refresh(); // an opening text asks at once, without the typing pause
        since_edit_ = kDebounce;
        keys_.set_length(field_.length());
        keys_.set_layout(0);
        keys_.set_shift(ui::KeyboardShift::off);
        keys_.set_focus(0, 0);
        keys_.enter();
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        Data &data = context.services.data;
        data_ = &data;
        age_ += context.dt;
        since_edit_ += context.dt;
        match_age_ += context.dt;
        localise(context.reduced_motion);

        read_history(data, context);
        read_suggestions(data);
        read_matches(data);

        field_.set_active(zone_ != Zone::matches);
        keys_.set_active(zone_ == Zone::keys);
        keys_.set_length(field_.length());
        if (context.top)
            handle(input, context);

        field_.update(context.dt);
        keys_.update(context.dt);
        for (ui::Skeleton &bone : bones_)
            bone.update(context.dt);
        const float start = static_cast<float>(match_index_) * kCardPitch;
        scroll_.reveal(start, start + kCardW, kRight - kLeft, kCardPitch * 0.5f);
        scroll_.update(context.dt, context.reduced_motion ? 60.0f : 12.0f);
        ring_.target(grown(card_rect(match_index_, scroll_.position.value), 0.06f, 6.0f));
        ring_.update(context.dt, context.reduced_motion ? 60.0f : 20.0f);
        ring_amount_.target = zone_ == Zone::matches ? 1.0f : 0.0f;
        ring_amount_.update(context.dt, context.reduced_motion ? 60.0f : 20.0f);
        nudge_.update(context.dt, 9.0f);
        if (context.top)
        {
            // The page takes the colour of the best match, or of the card in focus.
            const std::size_t lit = zone_ == Zone::matches ? static_cast<std::size_t>(match_index_) : 0u;
            context.nav.ambient(lit < matches_.size() ? matches_[lit].thumb : std::string());
        }
        set_hints();
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        search::Handover hand(g, phase);
        wear(context.look);
        ui::Canvas canvas{g.l, context.fonts, context.glass, context.time};
        {
            Enter enter(g, arrive(1, 0.06f, 0.5f), 20.0f);
            field_.draw(canvas);
            draw_list_note(g);
        }
        {
            // The keyboard rises into place.
            Enter enter(g, arrive(2, 0.06f, 0.55f), 70.0f);
            keys_.draw(canvas);
        }
        draw_strip(context, g, canvas);
    }

    std::string title() const override
    {
        return tr("Search");
    }

    std::span<const ui::Hint> hints() const override
    {
        return {hints_.data(), hint_count_};
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        if (zone_ == Zone::keys)
        {
            const ui::KeyboardKey &key = keys_.focused();
            info.id = "key/" + key_name(key);
            info.text = key_name(key);
            info.rect = keys_.key_rect(keys_.focus_row(), keys_.focus_key());
            info.index = keys_.focus_key();
            info.total = static_cast<int>(keys_.layouts()[static_cast<std::size_t>(keys_.layout())].rows[static_cast<std::size_t>(keys_.focus_row())].size());
        }
        else if (zone_ == Zone::list)
        {
            const int at = field_.focus();
            if (at >= 0)
            {
                info.id = std::string(field_.showing_recent() ? "recent/" : "suggestion/") + std::to_string(at);
                info.text = field_.suggestions()[static_cast<std::size_t>(at)].text;
                info.rect = row_rect(at);
                info.index = at;
                info.total = shown_rows();
            }
            else
            {
                const Rect field = field_.field_rect();
                const bool clear = at == ui::SearchField::kFocusClear;
                info.id = clear ? "field/clear" : "field";
                info.text = clear ? std::string(tr("Clear")) : field_.text();
                info.rect = clear ? Rect{field.x + field.w - 46.0f, field.cy() - 15.0f, 30.0f, 30.0f} : field;
                info.total = 1;
            }
        }
        else
        {
            info.id = "matches/" + std::to_string(match_index_);
            if (static_cast<std::size_t>(match_index_) < matches_.size())
                info.text = matches_[static_cast<std::size_t>(match_index_)].title;
            info.rect = card_rect(match_index_, scroll_.position.target);
            info.index = match_index_;
            info.total = static_cast<int>(matches_.size());
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        out.emplace_back("field", field_.text());
        if (!field_.text().empty())
            out.emplace_back("field/clear", tr("Clear"));
        const char *group = field_.showing_recent() ? "recent/" : "suggestion/";
        for (int i = 0; i < shown_rows(); ++i)
            out.emplace_back(group + std::to_string(i), field_.suggestions()[static_cast<std::size_t>(i)].text);
        for (const auto &row : keys_.layouts()[static_cast<std::size_t>(keys_.layout())].rows)
        {
            for (const ui::KeyboardKey &key : row)
                out.emplace_back("key/" + key_name(key), key_name(key));
        }
        for (std::size_t i = 0; i < matches_.size(); ++i)
            out.emplace_back("matches/" + std::to_string(i), matches_[i].title);
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("text", field_.text());
        out.set("zone", zone_ == Zone::keys ? "keys" : zone_ == Zone::list ? "list" : "matches");
        out.set("list", shown_rows() == 0 ? "none" : field_.showing_recent() ? "recent" : "suggestions");
        out.set("rows", shown_rows());
        out.set("recent", static_cast<int>(recents_.size()));
        out.set("history", load_name(history_load_));
        out.set("suggest", field_.text().empty() ? "idle" : waiting_ || field_.searching() ? "loading" : load_name(suggest_load_));
        out.set("matches", static_cast<int>(matches_.size()));
        out.set("matches_for", match_shown_);
        out.set("matches_load", load_name(match_load_));
        out.set("layout", keys_.layout() == 0 ? "letters" : "symbols");
        out.set("shift", keys_.shift() == ui::KeyboardShift::off ? "off" : keys_.shift() == ui::KeyboardShift::once ? "once" : "lock");
        std::vector<std::string> rows;
        for (int i = 0; i < shown_rows(); ++i)
            rows.push_back(field_.suggestions()[static_cast<std::size_t>(i)].text);
        out.list("rows", std::move(rows));
        std::vector<std::string> titles;
        for (const model::Video &video : matches_)
            titles.push_back(video.title);
        out.list("matches", std::move(titles));
    }

  private:
    enum class Zone : std::uint8_t
    {
        keys,
        list,
        matches,
    };

    struct Recent
    {
        std::string serial; // the provider's name of the row; "" for a search of this visit
        std::string query;
    };

    // ---- layout ----------------------------------------------------------------

    static Rect card_rect(int index, float scroll)
    {
        return {kLeft + static_cast<float>(index) * kCardPitch - scroll, kStripTop, kCardW, kCardH};
    }

    int shown_rows() const
    {
        return std::min(static_cast<int>(field_.suggestions().size()), kRecentRows);
    }

    // Where the kit's field draws a row of its list.
    Rect row_rect(int index) const
    {
        const Rect list = field_.list_rect();
        const float pad = field_.style.list_padding;
        const float header = field_.showing_recent() && shown_rows() > 0 ? field_.style.header_height : 0.0f;
        return {list.x + pad, list.y + pad + header + static_cast<float>(index) * field_.style.row_height, list.w - 2.0f * pad,
                field_.style.row_height};
    }

    float arrive(int index, float step, float duration) const
    {
        return tween::stagger(age_, index, step, duration);
    }

    static std::string key_name(const ui::KeyboardKey &key)
    {
        switch (key.kind)
        {
        case ui::KeyKind::space:
            return "space";
        case ui::KeyKind::shift:
            return "shift";
        case ui::KeyKind::backspace:
            return "delete";
        case ui::KeyKind::layout:
            return "layout";
        case ui::KeyKind::done:
            return "done";
        case ui::KeyKind::character:
            break;
        }
        return key.text;
    }

    // ---- the kit's components in the look and the language -----------------------

    // REQUEST: update() has no Look, and a kit component draws in its style's
    // theme; until UpdateContext carries the look the two components are
    // mutable and put the theme on when they are drawn (requests/A.md).
    void wear(const Look &look) const
    {
        keys_.style.theme = look.theme;
        field_.style.theme = look.theme;
        for (ui::Skeleton &bone : bones_)
            bone.style.theme = look.theme;
    }

    void localise(bool reduced)
    {
        keys_.style.reduced_motion = reduced;
        keys_.style.space_label = tr("Space");
        keys_.style.done_label = tr("Done");
        field_.style.reduced_motion = reduced;
        field_.style.recent_title = tr("Recent searches");
        field_.set_placeholder(tr("Search YouTube"));
        for (ui::Skeleton &bone : bones_)
            bone.style.reduced_motion = reduced;
        // While an answer is on its way the list says nothing rather than "nothing found".
        field_.style.empty_text = waiting_ ? std::string()
                                  : suggest_load_ == model::Load::ready ? std::string(tr("No suggestions"))
                                                                        : std::string(tr("Suggestions are not available"));
    }

    // ---- data --------------------------------------------------------------------

    // The field asks here when the typing pause is over (and on refresh()).
    // An answer that has not arrived leaves the rows of the text before in place.
    void provide(std::string_view query, std::vector<ui::Suggestion> &out)
    {
        asked_ = trimmed(query);
        if (data_ == nullptr || asked_.empty())
        {
            waiting_ = false;
            shown_.clear();
            suggest_load_ = model::Load::ready;
            return;
        }
        const Page &page = data_->catalog(search::suggest_catalog(asked_));
        if (page.load == model::Load::loading || page.load == model::Load::idle)
        {
            waiting_ = true;
            out = shown_;
            return;
        }
        waiting_ = false;
        suggest_load_ = page.load;
        shown_.clear();
        if (page.load == model::Load::ready)
        {
            for (const model::Row &row : page.rows)
            {
                if (row.interface_text || row.title.empty())
                    continue; // the provider's own "No suggestions" row
                shown_.push_back({row.title, row.fact("detail"), 0});
            }
        }
        out = shown_;
    }

    void read_suggestions(Data &data)
    {
        // Only the answer to the text that is in the field now is ever read: one
        // that arrives for an earlier text is left in the data layer, unseen.
        if (waiting_ && since_edit_ > kDebounce &&
            data.catalog(search::suggest_catalog(asked_)).load != model::Load::loading)
            field_.refresh();
        field_.set_busy(waiting_);
    }

    void read_matches(Data &data)
    {
        const std::string wanted = trimmed(field_.text());
        if (wanted != match_text_ && since_edit_ >= kDebounce)
            match_text_ = wanted;
        const Page &page = data.catalog(search::preview_catalog(match_text_));
        if (page.load == model::Load::loading || page.load == model::Load::idle)
        {
            // What the text before found stays until this one has answered.
            match_load_ = matches_.empty() ? model::Load::loading : model::Load::ready;
        }
        else if (page.load == model::Load::ready)
        {
            const std::size_t count = std::min<std::size_t>(page.videos.size(), kStripCards);
            bool same = count == matches_.size() && match_shown_ == match_text_;
            for (std::size_t i = 0; same && i < count; ++i)
                same = matches_[i].id == page.videos[i].id;
            if (!same)
            {
                matches_.assign(page.videos.begin(), page.videos.begin() + static_cast<std::ptrdiff_t>(count));
                match_shown_ = match_text_;
                match_age_ = 0.0f;
            }
            match_load_ = model::Load::ready;
        }
        else
        {
            matches_.clear();
            match_shown_ = match_text_;
            match_load_ = page.load;
            match_error_ = page.error;
        }
        // Cards that went away under the focus hand it back to where it came from.
        if (zone_ == Zone::matches && matches_.empty())
            zone_ = return_zone_;
        match_index_ = std::clamp(match_index_, 0, std::max(static_cast<int>(matches_.size()) - 1, 0));
    }

    static std::string signature(const Page &page)
    {
        std::string out;
        for (const model::Row &row : page.rows)
            out += row.id + '\n' + row.title + '\n';
        return out;
    }

    // The list of a "search-history" answer: its queries, the row that clears
    // them, and the provider's notice when there is none.
    void adopt(const Page &page)
    {
        recents_.clear();
        note_title_.clear();
        note_state_.clear();
        for (const model::Row &row : page.rows)
        {
            if (starts_with(row.id, search::kHistoryRun))
                recents_.push_back({row.id.substr(search::kHistoryRun.size()), row.title});
            else if (starts_with(row.id, search::kHistoryForget))
                recents_.push_back({row.id.substr(search::kHistoryForget.size()), row.title});
            else if (starts_with(row.id, search::kHistoryClear))
                clear_row_ = row.id;
            else if (starts_with(row.id, "account:notice"))
            {
                note_title_ = row.title;
                note_state_ = row.fact("state");
            }
        }
        sync_recent();
    }

    void sync_recent()
    {
        std::vector<std::string> queries;
        for (const Recent &entry : recents_)
            queries.push_back(entry.query);
        if (queries != field_.recent())
            field_.set_recent(std::move(queries));
    }

    void read_history(Data &data, UpdateContext &context)
    {
        const Page &list = data.catalog(search::kHistory);
        history_load_ = list.load;
        if (list.load == model::Load::ready)
        {
            const std::string seen = signature(list);
            if (seen != history_seen_)
            {
                history_seen_ = seen;
                if (actions_.empty())
                    adopt(list);
            }
        }
        if (actions_.empty())
            return;
        // REQUEST: forgetting is asked for as the provider's own rows through
        // catalog(); the data layer needs a door for actions (requests/A.md).
        const Page &answer = data.catalog(actions_.front());
        if (answer.load == model::Load::ready)
        {
            actions_.erase(actions_.begin());
            if (actions_.empty())
                adopt(answer); // what the provider says is left
        }
        else if (answer.load == model::Load::failed || answer.load == model::Load::offline)
        {
            actions_.clear();
            context.nav.toast(answer.error.empty() ? std::string(tr("The recent searches could not be changed.")) : answer.error,
                              ToastKind::error);
            if (list.load == model::Load::ready)
                adopt(list); // nothing was forgotten: the list is what it was
        }
    }

    // ---- input -------------------------------------------------------------------

    void edited()
    {
        since_edit_ = 0.0f;
        if (field_.text().empty())
        {
            waiting_ = false;
            shown_.clear();
        }
    }

    void refuse(UpdateContext &context, const InputFrame &input, bool shake = false)
    {
        if (input.nav_repeat)
            return; // a held direction stays silent at an edge
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        if (shake)
            nudge_.trigger();
    }

    // The kit's field keeps its own focus and offers no way to set it, so it is
    // walked there with silent steps: kFocusField, or a row of the list.
    void walk_field(int target)
    {
        ui::Feedback silent;
        for (int guard = 0; guard < 16 && field_.focus() != target; ++guard)
        {
            const int at = field_.focus();
            InputFrame step;
            if (at == ui::SearchField::kFocusClear)
                step.nav = Direction::left;
            else if (target == ui::SearchField::kFocusField || at > target)
                step.nav = Direction::up;
            else
                step.nav = Direction::down;
            field_.handle(step, silent);
            if (field_.focus() == at)
                break;
        }
    }

    // The middle of what the list zone has in focus, for moving sideways.
    float list_focus_y() const
    {
        return field_.focus() >= 0 ? row_rect(field_.focus()).cy() : field_.field_rect().cy();
    }

    void to_list(UpdateContext &context)
    {
        // The field or the row beside the key the focus leaves.
        const float y = keys_.key_rect(keys_.focus_row(), keys_.focus_key()).cy();
        int target = ui::SearchField::kFocusField;
        float best = std::fabs(field_.field_rect().cy() - y);
        for (int i = 0; i < shown_rows(); ++i)
        {
            const float away = std::fabs(row_rect(i).cy() - y);
            if (away < best)
            {
                best = away;
                target = i;
            }
        }
        walk_field(target);
        zone_ = Zone::list;
        context.feedback.play(audio::Cue::tab, 1.0f, ui::pan_for_x(field_.field_rect().cx()));
    }

    void to_keys(UpdateContext &context, bool beside)
    {
        if (beside)
        {
            // The first key of the row beside what the focus leaves.
            const float y = list_focus_y();
            const int rows = static_cast<int>(keys_.layouts()[static_cast<std::size_t>(keys_.layout())].rows.size());
            int row = 0;
            for (int i = 1; i < rows; ++i)
            {
                if (std::fabs(keys_.key_rect(i, 0).cy() - y) < std::fabs(keys_.key_rect(row, 0).cy() - y))
                    row = i;
            }
            keys_.set_focus(row, 0, false);
        }
        walk_field(ui::SearchField::kFocusField);
        zone_ = Zone::keys;
        context.feedback.play(audio::Cue::tab, 1.0f, ui::pan_for_x(kKeys.cx()));
    }

    void to_matches(UpdateContext &context, const InputFrame &input, float from_x)
    {
        if (matches_.empty())
        {
            refuse(context, input);
            return;
        }
        return_zone_ = zone_;
        const float along = (from_x - kLeft + scroll_.position.target - kCardW * 0.5f) / kCardPitch;
        match_index_ = std::clamp(static_cast<int>(std::lround(along)), 0, static_cast<int>(matches_.size()) - 1);
        if (zone_ == Zone::list)
            walk_field(ui::SearchField::kFocusField);
        zone_ = Zone::matches;
        ring_.snap(grown(card_rect(match_index_, scroll_.position.value), 0.06f, 6.0f));
        context.feedback.play(audio::Cue::tab, 0.96f, ui::pan_for_x(from_x));
    }

    void submit(std::string_view text, UpdateContext &context)
    {
        const std::string query = trimmed(text);
        if (query.empty())
        {
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
            context.feedback.rumble(0.25f, 0.05f);
            return;
        }
        // The provider puts a search it runs at the head of the history; the
        // list here does the same until it is read again.
        Recent entry{std::string(), query};
        for (auto it = recents_.begin(); it != recents_.end();)
        {
            if (same_query(it->query, query))
            {
                entry.serial = it->serial;
                it = recents_.erase(it);
            }
            else
                ++it;
        }
        recents_.insert(recents_.begin(), std::move(entry));
        sync_recent();
        context.nav.push(Route{ScreenId::results, query, {}}, Move::push);
    }

    void forget(int index, UpdateContext &context)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= recents_.size())
            return;
        const Recent entry = recents_[static_cast<std::size_t>(index)];
        recents_.erase(recents_.begin() + index);
        sync_recent();
        if (!entry.serial.empty())
            actions_.push_back(std::string(search::kHistoryForget) + entry.serial);
        context.feedback.play(audio::Cue::erase, 1.0f, ui::pan_for_x(field_.field_rect().cx()));
    }

    void handle(const InputFrame &input, UpdateContext &context)
    {
        // The answer of "Clear recent searches?".
        if (*clear_answer_ != 0)
        {
            if (*clear_answer_ > 0 && !clear_row_.empty())
            {
                recents_.clear();
                sync_recent();
                actions_.push_back(clear_row_);
                context.nav.toast(tr("Recent searches cleared"), ToastKind::success);
            }
            *clear_answer_ = 0;
        }
        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (pressed(input, Action::menu))
        {
            submit(field_.text(), context);
            return;
        }
        // The components never see Circle: leaving is the frame's, with its sound.
        InputFrame in = input;
        in.pressed &= ~hui::action_bit(Action::back);

        if (zone_ == Zone::keys)
        {
            const ui::Event event = keys_.handle(in, context.feedback);
            if (event == ui::Event::activated)
                submit(field_.text(), context);
            else if (keys_.exit() == Direction::left)
                to_list(context);
            else if (keys_.exit() == Direction::down)
                to_matches(context, input, keys_.key_rect(keys_.focus_row(), keys_.focus_key()).cx());
        }
        else if (zone_ == Zone::list)
        {
            const bool on_recent = field_.showing_recent() && field_.focus() >= 0;
            if (on_recent && pressed(input, Action::west))
            {
                forget(field_.focus(), context);
                return;
            }
            if (on_recent && pressed(input, Action::north) && !clear_row_.empty())
            {
                const std::shared_ptr<int> answer = clear_answer_;
                context.nav.confirm(tr("Clear recent searches?"), tr("Every search kept for this profile is forgotten."), tr("Clear all"),
                                    [answer](bool yes) { *answer = yes ? 1 : -1; });
                return;
            }
            const float from_y = list_focus_y();
            const ui::Event event = field_.handle(in, context.feedback);
            if (event == ui::Event::activated)
            {
                if (field_.has_pick())
                {
                    // A completion or a recent search: it is the text now, and it is searched for.
                    edited();
                    since_edit_ = kDebounce;
                    keys_.set_length(field_.length());
                    submit(field_.picked().text, context);
                }
                else
                {
                    to_keys(context, false); // Cross on the field: go and type
                }
            }
            else if (event == ui::Event::changed)
            {
                edited(); // the clear button emptied the field
                keys_.set_length(field_.length());
            }
            else if (field_.exit() == Direction::right)
                to_keys(context, true);
            else if (field_.exit() == Direction::left)
                context.nav.open_rail();
            else if (field_.exit() == Direction::down)
                to_matches(context, input, kLeft + kCardW * 0.5f);
            (void)from_y;
        }
        else
        {
            const int count = static_cast<int>(matches_.size());
            if (input.nav == Direction::left && match_index_ == 0)
                context.nav.open_rail();
            else if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int next = match_index_ + (input.nav == Direction::right ? 1 : -1);
                if (next < count)
                {
                    match_index_ = next;
                    context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(card_rect(next, scroll_.position.target).cx()));
                }
                else
                {
                    refuse(context, input, true);
                }
            }
            else if (input.nav == Direction::up)
            {
                zone_ = return_zone_ == Zone::matches ? Zone::keys : return_zone_;
                context.feedback.play(audio::Cue::tab, 1.04f);
            }
            else if (input.nav == Direction::down)
            {
                refuse(context, input);
            }
            else if (count > 0 && pressed(input, Action::confirm))
            {
                const model::Video &video = matches_[static_cast<std::size_t>(match_index_)];
                if (context.services.data.play(video))
                    context.feedback.play(audio::Cue::launch);
                else
                    context.nav.toast(tr("This video cannot be played right now."), ToastKind::error);
            }
            else if (count > 0 && pressed(input, Action::north))
            {
                const model::Video &video = matches_[static_cast<std::size_t>(match_index_)];
                context.nav.push(Route{ScreenId::video, video.id, {}}, Move::zoom,
                                 grown(card_rect(match_index_, scroll_.position.value), 0.06f, 6.0f));
            }
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
        if (zone_ == Zone::keys)
        {
            add(ui::Button::square, tr("Delete"));
            add(ui::Button::triangle, tr("Space"));
            add(ui::Button::l2, tr("Shift"));
            add(ui::Button::r2, keys_.layout() == 0 ? tr("Symbols") : tr("Letters"));
            add(ui::Button::options, tr("Search"));
        }
        else if (zone_ == Zone::list)
        {
            const int at = field_.focus();
            if (at >= 0)
            {
                add(ui::Button::cross, tr("Search"));
                if (field_.showing_recent())
                {
                    add(ui::Button::square, tr("Remove"));
                    if (!clear_row_.empty())
                        add(ui::Button::triangle, tr("Clear all"));
                }
            }
            else
            {
                add(ui::Button::cross, at == ui::SearchField::kFocusClear ? tr("Clear") : tr("Type"));
                if (!trimmed(field_.text()).empty())
                    add(ui::Button::options, tr("Search"));
            }
            add(ui::Button::circle, tr("Back"));
        }
        else
        {
            add(ui::Button::cross, tr("Play"));
            add(ui::Button::triangle, tr("Details"));
            if (!trimmed(field_.text()).empty())
                add(ui::Button::options, tr("Search"));
            add(ui::Button::circle, tr("Back"));
        }
    }

    // ---- drawing -----------------------------------------------------------------

    // What stands under the field while its list has nothing to show.
    void draw_list_note(G &g) const
    {
        if (!field_.text().empty() || shown_rows() > 0)
            return;
        const Rect field = field_.field_rect();
        const float x = field.x + 30.0f;
        const float top = field.y + field.h + 10.0f;
        const float width = field.w - 60.0f;
        if (history_load_ == model::Load::loading || history_load_ == model::Load::idle)
        {
            // The recent searches are on their way: their rows as bones.
            for (int i = 0; i < 4; ++i)
            {
                const float pulse = g.reduced ? 0.8f : 0.6f + 0.4f * ui::breathe(g.time + 0.12f * static_cast<float>(i), 1.7f);
                g.l.rounded_rect({x, top + 26.0f + 58.0f * static_cast<float>(i), width * (i == 3 ? 0.4f : 0.62f - 0.08f * static_cast<float>(i)), 22.0f},
                                 11.0f, g.k.raised.with_alpha(g.k.raised.a * pulse));
            }
            return;
        }
        if (history_load_ != model::Load::ready)
            return; // the strip says what is wrong with the service
        const bool off = note_state_ == "off";
        const bool worded = off || note_state_ == "empty" || note_title_.empty();
        const std::string heading = off ? std::string(tr("Search history is off")) : worded ? std::string(tr("No recent searches")) : note_title_;
        icon(g, Icon::clock, x + 12.0f, top + 38.0f, 30.0f, g.k.ink3);
        text(g, g.f.semibold, fit(g.f.semibold, heading, kTypeBody, width - 44.0f), x + 44.0f, top + 46.0f, kTypeBody, g.k.ink2);
        if (worded)
            paragraph(g, g.f.regular,
                      off ? tr("Turn it on in the preferences to keep searches for this profile.")
                          : tr("Searches you run are kept here for this profile."),
                      x + 44.0f, top + 82.0f, kTypeMeta, width - 44.0f, 28.0f, g.k.ink3, 3);
    }

    void draw_strip(const DrawContext &context, G &g, ui::Canvas &canvas) const
    {
        const float in = arrive(4, 0.06f, 0.5f);
        {
            Enter enter(g, in, 26.0f);
            const std::string typed = trimmed(field_.text());
            const std::string heading = typed.empty() ? std::string(tr("Popular right now")) : fill(tr("Best matches for \xE2\x80\x9C{0}\xE2\x80\x9D"), typed);
            text(g, g.f.semibold, fit(g.f.semibold, heading, kTypeShelf, kRight - kLeft), kLeft, kStripTitle, kTypeShelf, g.k.ink);

            if (match_load_ == model::Load::loading)
            {
                for (int i = 0; i < kStripBones; ++i)
                {
                    Enter card(g, arrive(5 + i, 0.05f, 0.45f), 24.0f);
                    ui::Skeleton bone = bones_[static_cast<std::size_t>(i)];
                    bone.set_bounds(card_rect(i, 0.0f));
                    bone.draw(canvas);
                }
            }
            else if (match_load_ != model::Load::ready)
            {
                const bool offline = match_load_ == model::Load::offline;
                icon(g, offline ? Icon::offline : Icon::alert, kLeft + 22.0f, kStripTop + 34.0f, 44.0f, g.k.ink2);
                text(g, g.f.semibold,
                     fit(g.f.semibold, offline ? tr("The YouTube service cannot be reached") : tr("The best matches could not be loaded"), kTypeBody, 1100.0f),
                     kLeft + 64.0f, kStripTop + 30.0f, kTypeBody, g.k.ink);
                const std::string detail = offline ? std::string(tr("You can still type a search. It runs when the service is back; recent searches stay on this PS5."))
                                           : match_error_.empty() ? std::string(tr("Press Options to run the search anyway.")) : match_error_;
                paragraph(g, g.f.regular, detail, kLeft + 64.0f, kStripTop + 68.0f, kTypeMeta, 1100.0f, 30.0f, g.k.ink2, 3);
            }
            else if (matches_.empty())
            {
                icon(g, Icon::search, kLeft + 22.0f, kStripTop + 34.0f, 40.0f, g.k.ink3);
                text(g, g.f.semibold, fit(g.f.semibold, tr("Nothing matches yet"), kTypeBody, 1100.0f), kLeft + 64.0f, kStripTop + 30.0f,
                     kTypeBody, g.k.ink2);
                paragraph(g, g.f.regular, tr("Keep typing, or press Options to search for exactly this."), kLeft + 64.0f, kStripTop + 68.0f,
                          kTypeMeta, 1100.0f, 30.0f, g.k.ink3, 2);
            }
            else
            {
                const Rect ring = ring_.value();
                const float lit = tween::clamp01(ring_amount_.value);
                for (std::size_t i = 0; i < matches_.size(); ++i)
                {
                    const Rect r = card_rect(static_cast<int>(i), scroll_.position.value);
                    if (r.x > gfx::kVirtualWidth || r.x + r.w < kRail)
                        continue;
                    // A card that scrolled past the first column goes under the rail's edge and fades.
                    const float shown = tween::clamp01(1.0f - (kLeft - r.x) / (kCardPitch * 0.6f));
                    const float focus = lit * tween::clamp01(1.0f - std::fabs(ring.cx() - r.cx()) / 240.0f);
                    const int slot = static_cast<int>(i) - static_cast<int>(scroll_.position.target / kCardPitch);
                    const float card_in = std::min(arrive(5 + std::clamp(slot, 0, 5), 0.05f, 0.45f),
                                                   tween::stagger(match_age_, std::clamp(slot, 0, 5), 0.04f, 0.3f));
                    Enter card(g, card_in, 24.0f);
                    video_card(g, grown(r, 0.06f * focus, 6.0f * focus), matches_[i], context.images.get(matches_[i].thumb, PictureSize::card), focus,
                               shown, 3);
                }
                if (lit > 0.01f && static_cast<std::size_t>(match_index_) < matches_.size())
                {
                    Rect at = ring;
                    if (!g.reduced)
                        at.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
                    focus_ring(g, at, kRadiusThumb, context.images.get(matches_[static_cast<std::size_t>(match_index_)].thumb, PictureSize::card).glow, lit);
                }
            }
        }
        // The foot of the page stays clean for the hints, whatever is behind them.
        g.l.gradient_rect({kRail, 960.0f, gfx::kVirtualWidth - kRail, 44.0f}, 0.0f, g.k.base.with_alpha(0.0f), g.k.base.with_alpha(0.9f));
        g.l.rounded_rect({kRail, 1004.0f, gfx::kVirtualWidth - kRail, 76.0f}, 0.0f, g.k.base.with_alpha(0.9f));
    }

    Route route_;
    Data *data_ = nullptr; // for the field's provider, which the kit calls without a context
    mutable ui::Keyboard keys_;
    mutable ui::SearchField field_;
    mutable std::array<ui::Skeleton, kStripBones> bones_;
    Zone zone_ = Zone::keys;
    Zone return_zone_ = Zone::keys; // where Up from the strip leads back to
    float age_ = 0.0f;
    float since_edit_ = 0.0f;

    // Completions.
    std::string asked_;                 // the text the list was last asked for
    std::vector<ui::Suggestion> shown_; // its answer, or the one before while it loads
    bool waiting_ = false;
    model::Load suggest_load_ = model::Load::ready;

    // Recent searches.
    std::vector<Recent> recents_;
    std::string clear_row_;            // the provider's row that forgets them all
    std::string note_title_;           // the provider's notice when there is none
    std::string note_state_;           // its fact "state": "off", "empty" or ""
    std::string history_seen_;         // the "search-history" answer the list was taken from
    std::vector<std::string> actions_; // forgetting that has been asked and not answered
    model::Load history_load_ = model::Load::idle;
    std::shared_ptr<int> clear_answer_ = std::make_shared<int>(0); // 1 yes, -1 no

    // Best matches.
    std::string match_text_;  // the text the strip asks for
    std::string match_shown_; // the text its cards answer
    std::vector<model::Video> matches_;
    model::Load match_load_ = model::Load::loading;
    std::string match_error_;
    float match_age_ = 10.0f;
    int match_index_ = 0;
    ui::Scroller scroll_;
    ui::SpringRect ring_;
    tween::Spring ring_amount_;
    ui::Pulse nudge_;

    std::array<ui::Hint, 6> hints_{};
    std::size_t hint_count_ = 0;
};

} // namespace

std::unique_ptr<Screen> make_search()
{
    return std::make_unique<Search>();
}

} // namespace afterglow
