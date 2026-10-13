// ProsperoTube Afterglow - notifications: what the account was told about.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The provider's catalog "library:notifications" as a list: who, what happened
// and when, the video it is about, and a dot while it is unread. Cross plays the
// video, Triangle opens the channel, Square marks everything as read; opening a
// row marks it read. Notifications belong to a YouTube account: a profile
// without one is invited to sign in.

#include "afterglow/screen.hpp"

#include "ui/components/skeleton.hpp"
#include "ui/components/stat.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace afterglow
{

namespace
{

// ---- the page ------------------------------------------------------------------
constexpr int kVisibleRows = 6;
constexpr float kRowsTop = 200.0f;
constexpr float kRowH = 116.0f;
constexpr float kRowPitch = 128.0f;
constexpr float kRowX = kLeft - 16.0f; // the plate reaches a little past the text on both sides
constexpr float kRowW = kRight - kLeft + 32.0f;
constexpr float kClipTop = 176.0f;
constexpr float kTextX = 140.0f;  // from the row's left edge
constexpr float kThumbW = 170.6f; // 16:9 at the row's height
constexpr float kThumbH = 96.0f;
constexpr Rect kMessage{kLeft + 400.0f, 280.0f, 848.0f, 460.0f};
constexpr const char *kCatalog = "library:notifications";
constexpr int kPageLimit = 80; // pages read at most
// REQUEST: theme.hpp has no size for the line under a page title, nor the sizes
// of the message block the approved empty and offline boards use.
constexpr float kTypeSub = 22.0f;
constexpr float kTypeMessage = 36.0f;

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

// A translated pattern with its number in place: "{0} new" -> "3 new".
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
                mix(std::hash<std::string>{}(row.id) ^ (row.has("unread") ? 0x9e3779b97f4a7c15ull : 0ull));
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

// One notification as its row shows it.
struct Notice
{
    std::string key;        // tells it apart from another notification about the same video
    std::string channel;    // who; "" when the provider does not say
    std::string channel_id; // "UC...", "" when unknown
    std::string what;       // what happened ("uploaded"), "" when YouTube's sentence could not be taken apart
    std::string title;      // the video's title, or the whole sentence
    std::string when;
    std::string thumb;
    std::string avatar;
    bool unread = false;
    bool live = false;
    model::Video video; // what Cross plays
};

Rect row_rect(int index, float scroll = 0.0f)
{
    return {kRowX, kRowsTop + kRowPitch * static_cast<float>(index) - scroll, kRowW, kRowH};
}

enum class State : std::uint8_t
{
    loading,
    ready,
    empty,
    invite, // notifications belong to an account and the profile has none
    notice, // the provider answered with a notice instead of the list
    failed,
    offline,
};
const char *state_name(State state)
{
    static constexpr const char *kNames[] = {"loading", "ready", "empty", "invite", "notice", "failed", "offline"};
    return kNames[static_cast<std::size_t>(state)];
}

struct Message
{
    Icon icon = Icon::bell;
    std::string title;
    std::string body;
    std::string action;
};

class Notifications final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::notifications;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        data_ = &context.services.data;
        if (restored)
            return; // back from the channel or the video: the row in focus and the scroll stay
        route_ = route;
        age_ = 0.0f;
        feed_.reset(kCatalog);
        rows_.clear();
        read_.clear();
        notice_.clear();
        focus_ = 0;
        first_row_ = 0;
        scroll_.snap(0.0f);
        reveal_ = 0.0f;
        was_loading_ = false;
        told_stuck_ = false;
        ring_snap_ = true;
        profile_ = context.services.data.profile();
        bone_.style.line_height = 18.0f;
        bone_.style.line_gap = 16.0f;
        bone_.style.row_height = 64.0f;
        bone_.style.rows = 1;
        block_.style.title_size = kTypeMessage;
        block_.style.body_size = kTypeBody;
        block_.style.hint_size = kTypeBody;
        block_.style.max_text_width = 640.0f;
        block_.style.max_lines = 4;
        block_.enter();
        shown_state_ = state_now();
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        const float dt = context.dt;
        Data &data = context.services.data;
        data_ = &data;
        age_ += dt;
        nudge_.update(dt, 10.0f);
        swept_.update(dt, 5.0f);
        const model::Profile browsing = data.profile();
        if (browsing.id != profile_.id || browsing.signed_in != profile_.signed_in)
        {
            rows_.clear(); // another profile is browsing: nothing of the one before stays while its list loads
            read_.clear();
            feed_.stamp = 0;
        }
        profile_ = browsing;
        if (profile_.signed_in && feed_.poll(data))
            rebuild();

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

        // The focus stays on a row that exists, and that row stays on the page.
        const int count = static_cast<int>(rows_.size());
        focus_ = std::clamp(focus_, 0, std::max(0, count - 1));
        const bool waiting = feed_.growing && count > 0 && focus_ == count - 1; // the next page's bones come into view under the last row
        first_row_ = std::clamp(first_row_, focus_ - (kVisibleRows - 1), focus_);
        first_row_ = waiting ? std::max(0, count - kVisibleRows + 1) : std::clamp(first_row_, 0, std::max(0, count - kVisibleRows));
        scroll_.target = static_cast<float>(first_row_) * kRowPitch;
        scroll_.update(dt, context.reduced_motion ? 60.0f : 13.0f);
        if (state == State::ready && count - 1 - focus_ <= kVisibleRows)
            feed_.grow(); // the end of what has arrived is near: ask for the next page
        if (feed_.stuck && !told_stuck_)
        {
            told_stuck_ = true;
            context.nav.toast(feed_.stuck_error.empty() ? std::string(tr("The rest of this list could not be loaded.")) : feed_.stuck_error,
                              ToastKind::warning);
        }

        // Pictures are asked for as soon as the list is known, a little ahead of what is on the page.
        for (int i = std::max(0, first_row_ - 2); i < std::min(count, first_row_ + kVisibleRows + 2); ++i)
        {
            const Notice &row = rows_[static_cast<std::size_t>(i)];
            (void)context.services.images.get(row.thumb, PictureSize::card);
            (void)context.services.images.get(row.avatar, PictureSize::avatar);
        }
        if (state == State::ready && count > 0)
            (void)context.services.images.get(rows_[static_cast<std::size_t>(focus_)].thumb, PictureSize::ambient);
        bone_.style.reduced_motion = context.reduced_motion;
        bone_.update(dt);
        block_.style.reduced_motion = context.reduced_motion;
        block_.update(dt);

        const Rect target = row_rect(focus_);
        if (ring_snap_ || age_ < 0.1f)
            ring_.snap(target);
        else
            ring_.target(target);
        ring_snap_ = false;
        ring_.update(dt, context.reduced_motion ? 60.0f : 20.0f);

        if (context.top)
        {
            act(input, context, state);
            context.nav.ambient(state_now() == State::ready && !rows_.empty()
                                    ? std::string_view(rows_[static_cast<std::size_t>(std::clamp(focus_, 0, static_cast<int>(rows_.size()) - 1))].thumb)
                                    : std::string_view());
        }
        write_hints();
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        Motion hand = hand_over(phase, g.reduced);
        hand.alpha *= hand.furniture; // no picture of this page travels: all of it gives way
        if (hand.alpha <= 0.004f)
            return;
        g.l.push_opacity(hand.alpha);
        g.l.push_transform(hand.scale, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, hand.dx, 0.0f);
        ui::Canvas canvas{g.l, g.f, g.glass, g.time};
        const State state = state_now();
        const float shown = state == State::loading ? 0.0f : tween::smoothstep(reveal_);
        const std::string under = subtitle(state);
        if (!under.empty())
        {
            // REQUEST: the frame draws a page's title but not the line under it.
            Enter enter(g, tween::stagger(age_, 0, 0.05f, 0.5f) * shown, -14.0f);
            text(g, g.f.regular, fit(g.f.regular, under, kTypeSub, 1200.0f), kLeft, 166.0f, kTypeSub, g.k.ink2);
        }
        if (shown < 1.0f)
        {
            // Bones in the shape of the rows, then the rows in the same places.
            g.l.push_opacity(1.0f - shown);
            for (int i = 0; i < kVisibleRows; ++i)
            {
                Enter enter(g, tween::stagger(age_, 1 + i, 0.05f, 0.45f), 22.0f);
                draw_bones(g, canvas, row_rect(i));
            }
            g.l.pop_opacity();
        }
        if (state == State::ready)
            draw_rows(context, g, canvas, shown);
        else if (state != State::loading)
            draw_message(g, canvas, state, shown);
        g.l.pop_transform();
        g.l.pop_opacity();
    }

    std::string title() const override
    {
        return tr("Notifications");
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
        info.id = "row/" + std::to_string(focus_);
        info.rect = row_rect(focus_, scroll_.target);
        info.index = focus_;
        info.total = static_cast<int>(rows_.size());
        if (focus_ < static_cast<int>(rows_.size()))
        {
            const Notice &row = rows_[static_cast<std::size_t>(focus_)];
            info.text = row.channel.empty() ? row.title : row.channel + " " + row.what + ": " + row.title;
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        const State state = state_now();
        if (state == State::ready)
        {
            for (std::size_t i = 0; i < rows_.size() && i < 48; ++i)
                out.emplace_back("row/" + std::to_string(i), rows_[i].title);
        }
        else if (state != State::loading)
        {
            out.emplace_back(std::string("action/") + action_name(state), message_of(state).action);
        }
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        const State state = state_now();
        out.set("catalog", kCatalog);
        out.set("state", state_name(state));
        out.set("items", static_cast<int>(rows_.size()));
        out.set("index", focus_);
        out.set("row", first_row_);
        out.set("unread", unread());
        out.set("pages", feed_.pages);
        out.set("more", feed_.more || feed_.growing ? 1 : 0);
        out.set("signed_in", profile_.signed_in ? 1 : 0);
        out.set("subtitle", subtitle(state));
        std::vector<std::string> titles;
        for (std::size_t i = 0; i < rows_.size() && i < 12; ++i)
            titles.push_back((is_unread(rows_[i]) ? "* " : "") + rows_[i].title);
        out.list("titles", std::move(titles));
    }

  private:
    // ---- what the page holds ---------------------------------------------------------

    State state_now() const
    {
        if (!profile_.signed_in)
            return State::invite;
        if (!rows_.empty())
            return State::ready; // also while a newer answer is on its way
        if (feed_.ready > 0)
            return notice_.empty() ? State::empty : State::notice;
        if (feed_.load == model::Load::failed)
            return State::failed;
        if (feed_.load == model::Load::offline)
            return State::offline;
        return State::loading;
    }

    static const char *action_name(State state)
    {
        switch (state)
        {
        case State::invite:
            return "sign_in";
        case State::failed:
            return "retry";
        case State::offline:
            return "library";
        default:
            return "browse";
        }
    }

    bool is_unread(const Notice &row) const
    {
        return row.unread && std::find(read_.begin(), read_.end(), row.key) == read_.end();
    }

    int unread() const
    {
        return static_cast<int>(std::count_if(rows_.begin(), rows_.end(), [this](const Notice &row) { return is_unread(row); }));
    }

    // Turns the pages that have arrived into rows. The focus stays on the same notification where it still exists.
    void rebuild()
    {
        if (feed_.ready == 0)
        {
            // Asked again and not answered yet: what was there stays until the answer replaces it.
            if (feed_.load != model::Load::loading && feed_.load != model::Load::idle)
                rows_.clear();
            return;
        }
        if (data_ == nullptr)
            return;
        const std::string focused = focus_ < static_cast<int>(rows_.size()) ? rows_[static_cast<std::size_t>(focus_)].key : std::string();
        rows_.clear();
        notice_.clear();
        for (int number = 0; number < feed_.ready; ++number)
        {
            const Page &page = data_->catalog(feed_.catalog, number);
            std::size_t next_video = 0;
            for (const model::Row &row : page.rows)
            {
                const model::Video *video = next_video < page.videos.size() && page.videos[next_video].id == row.id ? &page.videos[next_video++] : nullptr;
                if (row.id == "account:notice")
                {
                    if (notice_.empty())
                        notice_ = row.subtitle.empty() ? row.title : row.title + ". " + row.subtitle;
                    continue;
                }
                if (video == nullptr)
                    continue; // only notifications that open a video are rows
                Notice item;
                item.video = *video;
                item.channel = shown_fact(row, "channel");
                item.channel_id = video->channel_id;
                item.when = row.fact("published").empty() ? row.subtitle : shown_fact(row, "published");
                item.thumb = row.thumb;
                item.avatar = row.fact("channel_art_url");
                item.unread = row.has("unread");
                item.live = (video->badges & model::kBadgeLive) != 0;
                // YouTube words a notification as one sentence, "<channel> <what happened>: <title>".
                // Where the provider names the channel, the sentence is taken apart along it; else it is shown whole.
                item.title = row.title;
                if (!item.channel.empty() && starts_with(row.title, item.channel) && row.title.size() > item.channel.size() + 1 &&
                    row.title[item.channel.size()] == ' ')
                {
                    const std::string rest = row.title.substr(item.channel.size() + 1);
                    const std::size_t colon = rest.find(": ");
                    if (colon != std::string::npos && colon + 2 < rest.size())
                    {
                        item.what = rest.substr(0, colon);
                        item.title = rest.substr(colon + 2);
                    }
                    else
                    {
                        item.what = rest;
                        item.title.clear();
                    }
                }
                item.key = row.id + "|" + item.when + "|" + std::to_string(std::count_if(rows_.begin(), rows_.end(), [&row](const Notice &other)
                                                                              { return other.video.id == row.id; }));
                rows_.push_back(std::move(item));
            }
        }
        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            if (!focused.empty() && rows_[i].key == focused)
                focus_ = static_cast<int>(i);
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

    // REQUEST: the provider has no row that tells YouTube a notification was read
    // (the service keeps YouTube's own "clicked" call to itself). Until it has,
    // read is remembered by this page only.
    void mark_read(const Notice &row)
    {
        if (is_unread(row))
            read_.push_back(row.key);
    }

    void act(const InputFrame &input, UpdateContext &context, State state)
    {
        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (state == State::ready)
        {
            const int count = static_cast<int>(rows_.size());
            switch (input.nav)
            {
            case Direction::left:
                context.nav.open_rail();
                return;
            case Direction::right:
                refuse(input, context, 1.0f, 0.0f);
                break;
            case Direction::up:
                if (focus_ == 0)
                    refuse(input, context, 0.0f, -1.0f);
                else
                    move(context, -1);
                break;
            case Direction::down:
                if (focus_ + 1 < count)
                {
                    move(context, 1);
                }
                else if (feed_.stuck && !input.nav_repeat)
                {
                    // The page after this one was refused: ask for the list again; what is here stays meanwhile.
                    told_stuck_ = false;
                    context.services.data.forget(feed_.catalog);
                }
                else if (!feed_.more && !feed_.growing)
                {
                    refuse(input, context, 0.0f, 1.0f); // the last notification; while more are arriving the press just waits
                }
                break;
            case Direction::none:
                break;
            }
            const Notice &row = rows_[static_cast<std::size_t>(focus_)];
            if (pressed(input, Action::confirm))
            {
                mark_read(row);
                if (context.services.data.play(row.video))
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
            else if (pressed(input, Action::north))
            {
                if (row.channel_id.empty())
                {
                    refuse(InputFrame{}, context, 0.0f, 0.0f); // the provider did not say whose video it is
                }
                else
                {
                    mark_read(row);
                    context.nav.push(Route{ScreenId::channel, row.channel_id, row.channel}, Move::push);
                }
            }
            else if (pressed(input, Action::west))
            {
                if (unread() == 0)
                {
                    context.feedback.play(audio::Cue::select, 1.0f, 0.0f, 0.6f);
                    context.nav.toast(tr("Nothing is unread"), ToastKind::info);
                }
                else
                {
                    for (const Notice &each : rows_)
                        mark_read(each);
                    if (!context.reduced_motion)
                        swept_.trigger(); // the dots leave together
                    context.feedback.play(audio::Cue::toggle, 0.94f);
                    context.feedback.rumble(0.3f, 0.06f);
                    context.nav.toast(tr("All notifications marked as read"), ToastKind::success);
                }
            }
            return;
        }
        if (input.nav == Direction::left)
            context.nav.open_rail();
        else if (input.nav != Direction::none && state != State::loading)
            refuse(input, context, 0.0f, 0.0f);
        if (pressed(input, Action::confirm) || (pressed(input, Action::menu) && state == State::invite))
        {
            switch (state)
            {
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

    void move(UpdateContext &context, int step)
    {
        focus_ += step;
        // Lower rows sound a little lower.
        context.feedback.play(audio::Cue::focus, 1.06f - 0.02f * static_cast<float>(std::min(focus_ - first_row_, kVisibleRows)));
    }

    void write_hints()
    {
        hint_count_ = 0;
        const auto add = [this](ui::Button button, const char *label)
        {
            if (hint_count_ < hints_.size())
                hints_[hint_count_++] = ui::Hint{button, label, ui::Button::none};
        };
        switch (state_now())
        {
        case State::ready:
            add(ui::Button::cross, tr("Play"));
            add(ui::Button::triangle, tr("Open channel"));
            add(ui::Button::square, tr("Mark all as read"));
            return;
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
            add(ui::Button::cross, tr("Open Library"));
            add(ui::Button::triangle, tr("Try again"));
            break;
        case State::loading:
            break;
        }
        add(ui::Button::circle, tr("Back"));
    }

    // ---- words ---------------------------------------------------------------------------

    // The line under the title: how many are unread.
    std::string subtitle(State state) const
    {
        if (state != State::ready)
            return {};
        const int count = unread();
        return count == 0 ? std::string(tr("Nothing unread")) : count == 1 ? std::string(tr("1 new")) : fill(tr("{0} new"), std::to_string(count));
    }

    Message message_of(State state) const
    {
        Message m;
        switch (state)
        {
        case State::invite:
            m.icon = Icon::user;
            m.title = tr("Sign in to get notifications");
            m.body = tr("With a YouTube account, the channels you subscribe to tell you here about new videos and live streams.");
            m.action = tr("Sign in with a code");
            break;
        case State::empty:
            m.title = tr("No notifications yet");
            m.body = tr("When a channel you subscribe to uploads or goes live, YouTube says so here. Set what a channel may tell you on its page.");
            m.action = tr("Browse Home");
            break;
        case State::notice:
            m.icon = Icon::info;
            m.title = tr("Not available for this profile");
            m.body = notice_;
            m.action = tr("Browse Home");
            break;
        case State::failed:
            m.icon = Icon::alert;
            m.title = tr("Notifications could not be loaded");
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

    static Rect thumb_rect(const Rect &row)
    {
        return {row.x + row.w - kThumbW - 16.0f, row.y + (row.h - kThumbH) * 0.5f, kThumbW, kThumbH};
    }

    void draw_bones(G &g, ui::Canvas &canvas, const Rect &r) const
    {
        ui::Skeleton bone = bone_;
        bone.style.theme = g.k.theme;
        bone.style.kind = ui::SkeletonKind::list_row;
        bone.set_bounds({r.x + 54.0f, r.cy() - 32.0f, 900.0f, 64.0f});
        bone.draw(canvas);
        bone.style.kind = ui::SkeletonKind::block;
        bone.style.radius = kRadiusThumb;
        bone.set_bounds(thumb_rect(r));
        bone.draw(canvas);
    }

    void draw_rows(const DrawContext &context, G &g, ui::Canvas &canvas, float alpha) const
    {
        const int count = static_cast<int>(rows_.size());
        const float scroll = scroll_.value;
        g.l.push_opacity(alpha);
        g.l.push_clip({kRail, kClipTop, gfx::kVirtualWidth - kRail, gfx::kVirtualHeight - kClipTop});
        // One plate glides under the row in focus.
        Rect plate = ring_.value();
        plate.y -= scroll;
        const float nudge = g.reduced ? 0.0f : ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
        plate.x += nudge * nudge_x_;
        plate.y += nudge * nudge_y_;
        const float plate_in = tween::stagger(age_, 3, 0.05f, 0.4f);
        if (count > 0)
        {
            g.l.push_opacity(plate_in);
            lift(g, plate, kRadiusPanel, 0.5f);
            glass_panel(g, plate, kRadiusPanel);
            g.l.pop_opacity();
        }
        const int first = std::max(0, static_cast<int>(std::floor((scroll + kClipTop - kRowsTop - kRowH) / kRowPitch)));
        const int last = std::min(count - 1, static_cast<int>(std::floor((scroll + gfx::kVirtualHeight - kRowsTop) / kRowPitch)));
        for (int i = first; i <= last; ++i)
        {
            const Notice &row = rows_[static_cast<std::size_t>(i)];
            const Rect r = row_rect(i, scroll);
            const float focus = tween::clamp01(1.0f - std::fabs(plate.cy() - r.cy()) / 110.0f);
            // A row that scrolled off the top fades as it goes; one that has not reached the page stays out of the hint row.
            const float above = (kRowsTop - r.y) / kRowPitch;
            const float below = tween::clamp01((1010.0f - (r.y + r.h)) / 54.0f);
            const float row_alpha = (1.0f - tween::clamp01(above * 1.6f)) * below;
            if (row_alpha <= 0.01f)
                continue;
            Enter enter(g, tween::stagger(age_, 1 + i - first_row_, 0.05f, 0.45f) * row_alpha, 22.0f);
            // Unread is a dot beside the row, never the colour of its text alone; marking all as read lets the dots go together.
            const bool fresh = is_unread(row);
            const float dot = fresh ? 1.0f : row.unread ? tween::clamp01(swept_.value) : 0.0f;
            if (dot > 0.01f)
                g.l.circle(r.x + 30.0f, r.cy(), 6.0f * dot, g.k.accent.with_alpha(dot));
            avatar(g, r.x + 86.0f, r.cy(), 32.0f, row.channel.empty() ? row.title : row.channel, row.channel_id.empty() ? row.channel : row.channel_id,
                   context.images.get(row.avatar, PictureSize::avatar));
            const Rect thumb = thumb_rect(r);
            const float x = r.x + kTextX;
            const float room = thumb.x - 28.0f - x;
            const float first_line = row.title.empty() || (row.channel.empty() && row.when.empty()) ? r.cy() + 8.5f : r.y + 50.0f;
            if (row.channel.empty())
            {
                // The sentence as YouTube wrote it, and when.
                text(g, g.f.semibold, fit(g.f.semibold, row.title, kTypeBody, room), x, first_line, kTypeBody, g.k.ink);
                if (!row.when.empty())
                    text(g, g.f.regular, fit(g.f.regular, row.when, kTypeMeta, room), x, r.y + 86.0f, kTypeMeta, g.k.ink3);
            }
            else
            {
                // Who, what happened, when: the name keeps its room first, then the time, and the words between give way.
                const std::string when = fit(g.f.regular, row.when, kTypeMeta, 320.0f);
                const float when_w = when.empty() ? 0.0f : measure(g.f.regular, when, kTypeMeta) + 16.0f;
                const float what_w = measure(g.f.regular, row.what, kTypeBody);
                const std::string who = fit(g.f.semibold, row.channel, kTypeBody, std::max((room - when_w) * 0.56f, room - when_w - what_w - 24.0f));
                float cursor = x + text(g, g.f.semibold, who, x, first_line, kTypeBody, g.k.ink) + 8.0f;
                const std::string what = fit(g.f.regular, row.what, kTypeBody, std::max(0.0f, x + room - when_w - cursor));
                if (!what.empty())
                    cursor += text(g, g.f.regular, what, cursor, first_line, kTypeBody, g.k.ink2) + 16.0f;
                if (!when.empty())
                    text(g, g.f.regular, when, cursor, first_line, kTypeMeta, g.k.ink3);
                if (!row.title.empty())
                    text(g, g.f.regular, fit(g.f.regular, row.title, kTypeBody, room), x, r.y + 86.0f, kTypeBody, g.k.ink.with_alpha(0.74f + 0.26f * focus));
            }
            const Picture picture = context.images.get(row.thumb, PictureSize::card);
            const float pictured = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
            if (pictured < 1.0f)
                picture_placeholder(g, thumb, kRadiusThumb);
            if (pictured > 0.0f)
                g.l.image(picture.texture, thumb, picture.cover(thumb.w / thumb.h), Color{1.0f, 1.0f, 1.0f, pictured}, kRadiusThumb);
            if (row.live)
                tag(g, thumb.x + 8.0f, thumb.y + 20.0f, tr("LIVE"), g.k.accent, g.k.on_accent, 24.0f);
        }
        if (feed_.growing)
        {
            // More notifications are on their way: their bones stand where they will land.
            for (int i = count; i < count + 2; ++i)
            {
                const Rect r = row_rect(i, scroll);
                if (r.y + r.h <= 980.0f) // nothing below the page's foot but the hint row
                    draw_bones(g, canvas, r);
            }
        }
        if (count > 0)
        {
            const Picture picture = context.images.get(rows_[static_cast<std::size_t>(std::min(focus_, count - 1))].thumb, PictureSize::card);
            focus_ring(g, plate, kRadiusPanel, picture.ready() ? picture.glow : g.k.focus, plate_in * 0.8f);
        }
        g.l.pop_clip();
        if (count > kVisibleRows)
        {
            // How far down the list the page is.
            const Rect track{kRight + 40.0f, kRowsTop, 6.0f, kRowPitch * static_cast<float>(kVisibleRows) - 12.0f};
            const float limit = static_cast<float>(count - kVisibleRows) * kRowPitch;
            const float thumb = std::max(48.0f, track.h * static_cast<float>(kVisibleRows) / static_cast<float>(count));
            g.l.rounded_rect(track, 3.0f, g.k.raised);
            g.l.rounded_rect({track.x, track.y + (track.h - thumb) * tween::clamp01(scroll / limit), track.w, thumb}, 3.0f, g.k.ink3);
        }
        g.l.pop_opacity();
    }

    void draw_message(G &g, ui::Canvas &canvas, State state, float alpha) const
    {
        const Message m = message_of(state);
        ui::EmptyState block = block_;
        block.style.theme = g.k.theme;
        block.title = m.title;
        block.body = m.body;
        block.action = m.action;
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
    std::vector<Notice> rows_;
    std::vector<std::string> read_; // notifications read on this page (see mark_read)
    std::string notice_;            // the provider's words, when it answered with a notice
    int focus_ = 0;
    int first_row_ = 0;
    State shown_state_ = State::loading;
    tween::Spring scroll_;
    ui::SpringRect ring_; // the plate, where the list is at rest
    bool ring_snap_ = true;
    ui::Pulse nudge_;
    ui::Pulse swept_; // the unread dots leaving after "Mark all as read"
    float nudge_x_ = 0.0f;
    float nudge_y_ = 0.0f;
    float age_ = 0.0f;
    float reveal_ = 0.0f;      // how far the answer has taken over from the bones
    bool was_loading_ = false; // bones were shown: the answer cross-fades in
    bool told_stuck_ = false;  // a toast has said that the list ends early
    ui::Skeleton bone_;
    ui::EmptyState block_;
    std::array<ui::Hint, 4> hints_{};
    std::size_t hint_count_ = 0;
};

} // namespace

std::unique_ptr<Screen> make_notifications()
{
    return std::make_unique<Notifications>();
}

} // namespace afterglow
