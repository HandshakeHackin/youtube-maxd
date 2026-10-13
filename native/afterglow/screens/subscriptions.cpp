// ProsperoTube Afterglow - new videos from the channels a profile subscribes to.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Board 26 of the proposal: a strip of faces, a filter, and the feed by day.
// The feed is "library:subscriptions" (a video's fact published_days says how
// many days old it is: that is what groups it), the faces are
// "library:channels" (facts unseen and live). Moving along the strip keeps the
// feed to that channel; the filter (All, Today, Live, Unwatched) is turned
// with L1 and R1. Without an account, and when the account cannot be reached,
// the strip is "subscriptions", the channels followed on this console.

#include "afterglow/screens/channel_family.hpp"

namespace afterglow
{

namespace
{

using namespace family;

constexpr float kStripY = 236.0f;
constexpr float kStripX = kLeft + 44.0f;
constexpr float kStripPitch = 138.0f;
constexpr int kStripShown = 12;
constexpr float kFeedY = 396.0f;       // the first row of cards
constexpr float kHeadAbove = 24.0f;    // a day's heading stands this far above its first row
constexpr float kSectionGap = 40.0f;   // more room before the next day than between two rows
constexpr float kClipTop = 176.0f;     // a scrolled feed passes under the header here
constexpr float kRowRest = 204.0f;     // where a focused row rests once the feed is scrolled
constexpr float kHeadRest = 236.0f;    // ... when it is the first of its day, so that the heading shows
constexpr int kMaxPages = 8;

enum class Mode : std::uint8_t
{
    loading,
    feed,
    kept,    // the account cannot be reached: the channels followed on this console, and why
    invite,  // no account
    empty,   // an account that subscribes to nothing
    trouble, // nothing arrived and nothing is kept
};

const char *mode_name(Mode mode)
{
    switch (mode)
    {
    case Mode::loading:
        return "loading";
    case Mode::feed:
        return "feed";
    case Mode::kept:
        return "kept";
    case Mode::invite:
        return "invite";
    case Mode::empty:
        return "empty";
    case Mode::trouble:
        return "trouble";
    }
    return "";
}

enum class Zone : std::uint8_t
{
    strip,
    feed,
    notice,
};

const char *filter_name(int filter)
{
    return filter == 1 ? "today" : filter == 2 ? "live" : filter == 3 ? "unwatched" : "all";
}

// The day a video belongs to: 0 today, 1 this week, 2 this month, 3 earlier.
int day_of(int days)
{
    return days <= 0 ? 0 : days < 7 ? 1 : days < 30 ? 2 : 3;
}

class Subscriptions final : public Screen
{
  public:
    Subscriptions()
    {
        bar_.style.kind = ui::TabKind::pill;
        bar_.style.text_size = 22.0f;
        bar_.style.height = 50.0f;
        bar_.style.theme.omega = 18.0f; // REQUEST: see Channel::Channel
        bar_.style.theme.damping = 0.9f;
        bar_.set_bounds({1150.0f, 84.0f, 540.0f, 50.0f});
        bar_.set_focused(false);
    }

    ScreenId id() const override
    {
        return ScreenId::subscriptions;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        profile_ = context.services.data.profile();
        if (restored)
        {
            feed_.restart();
            faces_.restart();
            local_.restart();
            read(context);
            return;
        }
        route_ = route;
        feed_ = Feed{};
        faces_ = Feed{};
        local_ = Feed{};
        local_.start("subscriptions");
        if (profile_.signed_in)
        {
            feed_.start("library:subscriptions");
            faces_.start("library:channels");
        }
        bar_.set_tabs({{tr("All")}, {tr("Today")}, {tr("Live")}, {tr("Unwatched")}});
        bar_.set_active(0, true);
        mode_ = Mode::loading;
        zone_ = Zone::feed;
        settled_ = false;
        filter_ = 0;
        pick_ = 0;
        focus_ = 0;
        choice_ = 0;
        seen_ = 0;
        strip_.clear();
        order_.clear();
        lines_.clear();
        age_ = 0.0f;
        scroll_.snap(0.0f);
        strip_scroll_.snap(0.0f);
        ring_set_ = false;
        read(context);
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        read(context);
        sheet_.update(context);
        if (sheet_.take_changed())
        {
            context.services.data.forget("library:subscriptions");
            context.services.data.forget("library:channels");
        }
        lay_out(context.services.fonts);
        if (context.top)
        {
            handle(input, context);
            lay_out(context.services.fonts);
            const model::Video *video = zone_ == Zone::feed ? focused_video() : nullptr;
            context.nav.ambient(video != nullptr ? video->thumb : std::string());
        }
        advance(context);
        ask_pictures(context);
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        Travel travel(g, phase);
        if (mode_ == Mode::loading)
        {
            draw_bones(g);
            return;
        }
        const float scroll = scroll_.value;
        const bool clipped = scroll > 0.5f;
        if (clipped)
            g.l.push_clip({0.0f, kClipTop, gfx::kVirtualWidth, gfx::kVirtualHeight - kClipTop});
        g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, -scroll);
        draw_strip(g, context);
        if (mode_ == Mode::feed)
            draw_feed(g, context);
        else
        {
            Enter enter(g, arrive(age_, 4, 0.07f, 0.55f), 26.0f);
            notice_.draw(g, notice_icon(), notice_title(), notice_body(), choices_, choice_pos_.value, zone_ == Zone::notice && !sheet_.open());
        }
        g.l.pop_transform();
        if (clipped)
            g.l.pop_clip();
        foot_scrim(g);
        if (mode_ == Mode::feed)
        {
            Enter enter(g, arrive(age_, 1, 0.05f, 0.5f), -14.0f);
            ui::TabBar bar = bar_;
            const float omega = bar.style.theme.omega;
            const float damping = bar.style.theme.damping;
            bar.style.theme = context.look.theme;
            bar.style.theme.omega = omega;
            bar.style.theme.damping = damping;
            ui::Canvas canvas{g.l, g.f, g.glass, g.time};
            bar.draw(canvas);
        }
        sheet_.draw(g);
    }

    std::string title() const override
    {
        return tr("Subscriptions");
    }

    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[4];
        std::size_t count = 0;
        if (sheet_.open())
            return sheet_.hints();
        if (zone_ == Zone::notice)
            items[count++] = {ui::Button::cross, tr("Choose")};
        else if (zone_ == Zone::feed)
            items[count++] = {ui::Button::cross, tr("Play")};
        else
            items[count++] = {ui::Button::cross, tr("Open")};
        if (mode_ == Mode::feed)
            items[count++] = {ui::Button::l1, tr("Filter"), ui::Button::r1};
        if (mode_ != Mode::loading)
            items[count++] = {ui::Button::triangle, tr("Channels")};
        if (zone_ == Zone::feed || (zone_ == Zone::strip && strip_channel(strip_focus()) != nullptr))
            items[count++] = {ui::Button::touchpad, tr("Actions")};
        else if (zone_ == Zone::notice)
            items[count++] = {ui::Button::circle, tr("Back")};
        return {items, count};
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        if (sheet_.open())
            return sheet_.focus();
        if (zone_ == Zone::notice && notice_.count > 0)
        {
            info.id = std::string("state/") + choice_names_[choice_];
            info.text = choices_[choice_].label;
            info.rect = notice_.buttons[static_cast<std::size_t>(choice_)];
            info.index = choice_;
            info.total = notice_.count;
        }
        else if (zone_ == Zone::feed && !order_.empty())
        {
            info.id = "feed/" + std::to_string(focus_);
            info.text = feed_.videos[static_cast<std::size_t>(order_[static_cast<std::size_t>(focus_)])].title;
            Rect r = cell_rect(focus_);
            r.y -= scroll_.target;
            info.rect = r;
            info.index = focus_;
            info.total = static_cast<int>(order_.size());
        }
        else if (zone_ == Zone::strip && !strip_.empty())
        {
            const int at = strip_focus();
            const model::Channel *channel = strip_channel(at);
            info.id = channel == nullptr ? "strip/all" : "strip/" + std::to_string(at + (has_all() ? 0 : 1));
            info.text = channel == nullptr ? tr("All") : channel->name.c_str();
            const float cx = strip_x(at, strip_scroll_.target);
            info.rect = {cx - 48.0f, kStripY - 48.0f, 96.0f, 96.0f};
            info.index = at;
            info.total = static_cast<int>(strip_.size());
        }
        else
        {
            info.id = "state/loading";
            info.text = tr("Subscriptions");
            info.rect = {kLeft, kFeedY, kCellW, kCellH};
            info.total = 1;
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        for (std::size_t i = 0; i < strip_.size() && i < 24; ++i)
        {
            const model::Channel *channel = strip_channel(static_cast<int>(i));
            out.emplace_back(channel == nullptr ? "strip/all" : "strip/" + std::to_string(i + (has_all() ? 0 : 1)), channel == nullptr ? tr("All") : channel->name.c_str());
        }
        for (std::size_t i = 0; i < order_.size() && i < 24; ++i)
            out.emplace_back("feed/" + std::to_string(i), feed_.videos[static_cast<std::size_t>(order_[i])].title);
        if (zone_ == Zone::notice)
            for (int i = 0; i < notice_.count; ++i)
                out.emplace_back(std::string("state/") + choice_names_[i], choices_[i].label);
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("mode", mode_name(mode_));
        out.set("load", load_name(profile_.signed_in ? feed_.load : local_.load));
        out.set("guest", profile_.signed_in ? 0 : 1);
        out.set("zone", zone_ == Zone::strip ? "strip" : zone_ == Zone::feed ? "feed" : "notice");
        out.set("filter", filter_name(filter_));
        const model::Channel *picked = strip_channel(pick_);
        out.set("channel", picked != nullptr && mode_ == Mode::feed ? picked->id : std::string("all"));
        out.set("videos", static_cast<int>(order_.size()));
        out.set("strip", static_cast<int>(strip_.size()));
        out.set("pages", feed_.taken);
        out.set("scroll", static_cast<int>(scroll_.target));
        sheet_.snapshot(out);
        std::vector<std::string> titles;
        for (std::size_t i = 0; i < order_.size() && i < 12; ++i)
            titles.push_back(feed_.videos[static_cast<std::size_t>(order_[i])].title);
        out.list("videos", std::move(titles));
        std::vector<std::string> heads;
        for (const Line &line : lines_)
        {
            if (line.heads)
                heads.push_back(day_name(line.day));
        }
        out.list("days", std::move(heads));
    }

  private:
    // One face of the strip: a channel, or the tile that stands for all of them (channel -1).
    struct Entry
    {
        int channel = -1; // an index into the strip's feed
        bool live = false;
        bool unseen = false;
    };
    // One row of cards in the feed.
    struct Line
    {
        int day = 0;
        int first = 0; // an index into order_
        int count = 0;
        float y = 0.0f;     // of its cards, before the page's scroll
        bool heads = false; // the first row of its day: the heading stands above it
    };

    bool has_all() const
    {
        return mode_ == Mode::feed;
    }
    // The centre of a face. Without the "All" tile the first name starts at the content's left edge.
    float strip_x(int index, float scroll) const
    {
        return (has_all() ? kStripX : kLeft + 64.0f) + kStripPitch * static_cast<float>(index) - scroll;
    }
    const Feed &strip_feed() const
    {
        return mode_ == Mode::feed ? faces_ : local_;
    }
    // The strip entry the focus is on while it is in the strip: the picked one.
    int strip_focus() const
    {
        return std::clamp(pick_, 0, std::max(static_cast<int>(strip_.size()) - 1, 0));
    }
    const model::Channel *strip_channel(int at) const
    {
        if (at < 0 || at >= static_cast<int>(strip_.size()) || strip_[static_cast<std::size_t>(at)].channel < 0)
            return nullptr;
        return &strip_feed().channels[static_cast<std::size_t>(strip_[static_cast<std::size_t>(at)].channel)];
    }
    const model::Video *focused_video() const
    {
        return order_.empty() ? nullptr : &feed_.videos[static_cast<std::size_t>(order_[static_cast<std::size_t>(focus_)])];
    }
    const char *day_name(int day) const
    {
        if (!dated_)
            return tr("Latest");
        return day == 0 ? tr("Today") : day == 1 ? tr("This week") : day == 2 ? tr("This month") : tr("Earlier");
    }
    int line_of(int index) const
    {
        for (std::size_t i = 0; i < lines_.size(); ++i)
        {
            if (index >= lines_[i].first && index < lines_[i].first + lines_[i].count)
                return static_cast<int>(i);
        }
        return 0;
    }
    Rect cell_rect(int index) const
    {
        if (lines_.empty())
            return {kLeft, kFeedY, kCellW, kCellH};
        const Line &line = lines_[static_cast<std::size_t>(line_of(index))];
        return {kLeft + static_cast<float>(index - line.first) * kCellPitchX, line.y, kCellW, kCellH};
    }

    // ---- data ---------------------------------------------------------------------------

    static bool waiting(const Feed &feed)
    {
        return feed.load == model::Load::loading || feed.load == model::Load::idle;
    }
    static bool failed(const Feed &feed)
    {
        return feed.load == model::Load::failed || feed.load == model::Load::offline;
    }

    void read(UpdateContext &context)
    {
        Data &data = context.services.data;
        local_.read(data);
        if (profile_.signed_in)
        {
            feed_.read(data);
            faces_.read(data);
        }
        Mode mode = Mode::loading;
        if (!profile_.signed_in)
            mode = waiting(local_) ? Mode::loading : Mode::invite;
        else if (waiting(feed_))
            mode = Mode::loading;
        else if (failed(feed_))
            mode = waiting(local_) ? Mode::loading : local_.channels.empty() ? Mode::trouble : Mode::kept;
        else if (feed_.videos.empty() && !waiting(faces_) && faces_.channels.empty())
            mode = Mode::empty;
        else
            mode = Mode::feed;
        const unsigned seen = feed_.version * 31u + faces_.version * 7u + local_.version;
        if (mode == mode_ && seen == seen_)
            return;
        const bool arrived = mode_ == Mode::loading && mode != Mode::loading;
        mode_ = mode;
        seen_ = seen;
        if (arrived)
            age_ = std::min(age_, 0.1f);
        read_strip();
        arrange();
        if (mode_ == Mode::feed)
        {
            if (!settled_ || zone_ == Zone::notice)
                zone_ = order_.empty() ? Zone::strip : Zone::feed; // the page opens on its newest video
        }
        else if (mode_ != Mode::loading)
            zone_ = zone_ == Zone::strip && !strip_.empty() ? Zone::strip : Zone::notice;
        settled_ = mode_ != Mode::loading;
    }

    void read_strip()
    {
        const model::Channel *was = strip_channel(pick_);
        const std::string kept = was != nullptr ? was->id : std::string();
        strip_.clear();
        if (mode_ == Mode::loading)
            return;
        const Feed &source = strip_feed();
        if (has_all())
            strip_.push_back(Entry{});
        for (std::size_t i = 0; i < source.channels.size(); ++i)
        {
            const model::Row &row = *source.row_of_channel(i);
            strip_.push_back(Entry{static_cast<int>(i), row.has("live"), row.has("unseen")});
        }
        // The channel that was picked stays picked wherever the list has put it now.
        int pick = has_all() || kept.empty() ? std::min(pick_, 0) : 0;
        for (std::size_t i = 0; i < strip_.size(); ++i)
        {
            const model::Channel *channel = strip_channel(static_cast<int>(i));
            if (channel != nullptr && channel->id == kept)
                pick = static_cast<int>(i);
        }
        pick_ = std::clamp(pick, 0, std::max(static_cast<int>(strip_.size()) - 1, 0));
    }

    // Works out which videos show (the filter, the picked channel) and where: by day, four to a row.
    void arrange()
    {
        const std::string was = order_.empty() ? std::string() : feed_.videos[static_cast<std::size_t>(order_[static_cast<std::size_t>(focus_)])].id;
        order_.clear();
        lines_.clear();
        if (mode_ != Mode::feed)
        {
            focus_ = 0;
            return;
        }
        const model::Channel *picked = strip_channel(pick_);
        std::vector<int> day(feed_.videos.size(), 3);
        int live = 0;
        dated_ = false;
        for (std::size_t i = 0; i < feed_.videos.size(); ++i)
        {
            const std::string &days = feed_.row_of_video(i)->fact("published_days");
            if (!days.empty())
            {
                dated_ = true;
                day[i] = day_of(number_of(days));
            }
            live += (feed_.videos[i].badges & model::kBadgeLive) != 0 ? 1 : 0;
        }
        bar_.tab(2).badge = live;
        std::vector<int> by_day[4];
        for (std::size_t i = 0; i < feed_.videos.size(); ++i)
        {
            const model::Video &video = feed_.videos[i];
            if (picked != nullptr && video.channel_id != picked->id)
                continue;
            if (filter_ == 1 && (!dated_ || day[i] != 0))
                continue;
            if (filter_ == 2 && (video.badges & model::kBadgeLive) == 0)
                continue;
            if (filter_ == 3 && (video.watched || video.progress > 0.9f))
                continue;
            by_day[dated_ ? day[i] : 0].push_back(static_cast<int>(i));
        }
        float y = kFeedY;
        for (int d = 0; d < 4; ++d)
        {
            const std::vector<int> &videos = by_day[d];
            for (std::size_t at = 0; at < videos.size(); at += kColumns)
            {
                Line line;
                line.day = d;
                line.first = static_cast<int>(order_.size());
                line.count = static_cast<int>(std::min<std::size_t>(kColumns, videos.size() - at));
                line.heads = at == 0;
                if (line.heads && !lines_.empty())
                    y += kSectionGap;
                line.y = y;
                y += kCellPitchY;
                for (int i = 0; i < line.count; ++i)
                    order_.push_back(videos[at + static_cast<std::size_t>(i)]);
                lines_.push_back(line);
            }
        }
        // The focus stays on the video it was on, or on the nearest one that still shows.
        const int count = static_cast<int>(order_.size());
        for (int i = 0; i < count && !was.empty(); ++i)
        {
            if (feed_.videos[static_cast<std::size_t>(order_[static_cast<std::size_t>(i)])].id == was)
                focus_ = i;
        }
        focus_ = std::clamp(focus_, 0, std::max(count - 1, 0));
        if (count == 0 && zone_ == Zone::feed)
            zone_ = Zone::strip;
        // A filter that leaves little of what has been read looks further back, a few pages at most.
        if (lines_.size() < 3 && feed_.wanted < kMaxPages)
            feed_.want_more();
    }

    void ask_pictures(UpdateContext &context)
    {
        ImageCache &images = context.services.images;
        const int from = std::max(static_cast<int>(strip_scroll_.target / kStripPitch) - 1, 0);
        for (int i = from; i < std::min(static_cast<int>(strip_.size()), from + kStripShown + 3); ++i)
        {
            if (const model::Channel *channel = strip_channel(i))
                images.get(channel->avatar, PictureSize::avatar);
        }
        if (order_.empty())
            return;
        const int line = line_of(focus_);
        const int first = lines_[static_cast<std::size_t>(line)].first;
        const Line &last = lines_[static_cast<std::size_t>(std::min(line + 2, static_cast<int>(lines_.size()) - 1))];
        for (int i = first; i < last.first + last.count; ++i)
            images.get(feed_.videos[static_cast<std::size_t>(order_[static_cast<std::size_t>(i)])].thumb, PictureSize::card);
        if (zone_ == Zone::feed)
            images.get(focused_video()->thumb, PictureSize::ambient);
    }

    // ---- input ------------------------------------------------------------------------------

    void press_choice(UpdateContext &context)
    {
        const std::string name = choice_names_[choice_];
        if (name == "sign_in")
            context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
        else if (name == "discover")
            context.nav.root(Route{ScreenId::home, {}, {}});
        else if (name == "search")
            context.nav.root(Route{ScreenId::search, {}, {}});
        else
        {
            context.feedback.play(audio::Cue::select);
            context.services.data.refresh();
            age_ = 0.0f;
        }
    }

    void go(Zone zone, UpdateContext &context)
    {
        zone_ = zone;
        context.feedback.play(audio::Cue::tab, zone == Zone::strip ? 1.10f : 1.03f);
    }

    void handle(const InputFrame &input, UpdateContext &context)
    {
        if (sheet_.open())
        {
            Route route;
            if (sheet_.handle(input, context, &route))
                context.nav.push(route, Move::push);
            return;
        }
        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (mode_ == Mode::loading)
        {
            if (input.nav == Direction::left)
                context.nav.open_rail();
            return;
        }
        if (pressed(input, Action::north))
        {
            context.nav.push(Route{ScreenId::channels, {}, {}}, Move::push);
            return;
        }
        if (mode_ == Mode::feed && (pressed(input, Action::page_next) || pressed(input, Action::page_prev)))
        {
            if (bar_.step(pressed(input, Action::page_next) ? 1 : -1, input, context.feedback) == ui::Event::changed)
            {
                filter_ = bar_.active();
                focus_ = 0;
                order_.clear(); // another list: nothing to stay on
                arrange();
            }
            return;
        }
        switch (zone_)
        {
        case Zone::strip:
            handle_strip(input, context);
            break;
        case Zone::feed:
            handle_feed(input, context);
            break;
        case Zone::notice:
            handle_notice(input, context);
            break;
        }
    }

    void handle_strip(const InputFrame &input, UpdateContext &context)
    {
        const int count = static_cast<int>(strip_.size());
        if (count == 0)
        {
            zone_ = mode_ == Mode::feed ? Zone::feed : Zone::notice;
            return;
        }
        const float x = strip_x(pick_, strip_scroll_.target);
        if (input.nav == Direction::left && pick_ == 0)
            context.nav.open_rail();
        else if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = pick_ + (input.nav == Direction::right ? 1 : -1);
            if (next < count)
            {
                // Moving along the faces keeps the feed to the one in focus.
                pick_ = next;
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(x + (input.nav == Direction::right ? kStripPitch : -kStripPitch)));
                if (mode_ == Mode::feed)
                {
                    focus_ = 0;
                    order_.clear();
                    arrange();
                }
            }
            else
                refuse(context, input, nudge_, x);
        }
        else if (input.nav == Direction::down)
        {
            if (mode_ == Mode::feed && !order_.empty())
                go(Zone::feed, context);
            else if (mode_ != Mode::feed && notice_.count > 0)
                go(Zone::notice, context);
            else
                refuse(context, input, nudge_, x);
        }
        else if (input.nav == Direction::up)
            refuse(context, input, nudge_, x);
        else if (pressed(input, Action::confirm))
        {
            if (const model::Channel *channel = strip_channel(pick_))
                context.nav.push(Route{ScreenId::channel, channel->id, channel->name}, Move::push);
            else
                context.nav.push(Route{ScreenId::channels, {}, {}}, Move::push);
        }
        else if (pressed(input, Action::touch))
        {
            if (const model::Channel *channel = strip_channel(pick_))
                sheet_.show(channel->row, channel->name, context);
        }
    }

    void handle_feed(const InputFrame &input, UpdateContext &context)
    {
        if (order_.empty())
        {
            zone_ = Zone::strip;
            return;
        }
        const int at = line_of(focus_);
        const Line &line = lines_[static_cast<std::size_t>(at)];
        const int column = focus_ - line.first;
        int next = focus_;
        bool edge = false;
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
            if (column + 1 < line.count)
                next = focus_ + 1;
            else
                edge = true;
            break;
        case Direction::up:
            if (at == 0)
            {
                go(Zone::strip, context);
                return;
            }
            next = lines_[static_cast<std::size_t>(at - 1)].first + std::min(column, lines_[static_cast<std::size_t>(at - 1)].count - 1);
            break;
        case Direction::down:
            if (at + 1 < static_cast<int>(lines_.size()))
                next = lines_[static_cast<std::size_t>(at + 1)].first + std::min(column, lines_[static_cast<std::size_t>(at + 1)].count - 1);
            else
                edge = true;
            break;
        case Direction::none:
            break;
        }
        const float x = cell_rect(focus_).cx();
        if (edge)
            refuse(context, input, nudge_, x);
        else if (next != focus_)
        {
            focus_ = next;
            context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(cell_rect(next).cx()));
        }
        // The next page is asked for two rows before the end.
        if (static_cast<int>(lines_.size()) - 1 - line_of(focus_) <= 1 && feed_.wanted < kMaxPages)
            feed_.want_more();
        const model::Video &video = *focused_video();
        if (pressed(input, Action::confirm))
            family::play(video, x, context);
        else if (pressed(input, Action::touch))
            sheet_.show(video.id, video.title, context);
    }

    void handle_notice(const InputFrame &input, UpdateContext &context)
    {
        if (input.nav == Direction::left && choice_ == 0)
            context.nav.open_rail();
        else if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = choice_ + (input.nav == Direction::right ? 1 : -1);
            if (next < notice_.count)
            {
                choice_ = next;
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(notice_.buttons[static_cast<std::size_t>(next)].cx()));
            }
            else
                refuse(context, input, nudge_);
        }
        else if (input.nav == Direction::up && !strip_.empty())
            go(Zone::strip, context);
        else if (input.nav != Direction::none)
            refuse(context, input, nudge_);
        else if (pressed(input, Action::confirm) && notice_.count > 0)
            press_choice(context);
    }

    // ---- where things are, and how they move ---------------------------------------------------

    Icon notice_icon() const
    {
        return mode_ == Mode::kept || mode_ == Mode::trouble ? trouble_icon(feed_.load) : Icon::subs;
    }
    const char *notice_title() const
    {
        if (mode_ == Mode::kept || mode_ == Mode::trouble)
            return trouble_title(feed_.load);
        return mode_ == Mode::empty ? tr("No subscriptions yet") : tr("Sign in to see your subscriptions");
    }
    std::string notice_body() const
    {
        if (mode_ == Mode::kept)
            return (feed_.error.empty() ? std::string() : feed_.error + " ") + tr("The channels you follow on this PS5 are still here.");
        if (mode_ == Mode::trouble)
            return feed_.error.empty() ? std::string(tr("Your subscriptions did not arrive. Check the connection and try again.")) : feed_.error;
        if (mode_ == Mode::empty)
            return tr("Subscribe to a channel and its new videos arrive here, the newest first.");
        return tr("New videos from the channels you subscribe to on YouTube arrive here. Without an account, Discover has what people watch now.");
    }

    void lay_out(const ui::Fonts &fonts)
    {
        notice_.count = 0;
        const float top = strip_.empty() ? 300.0f : 400.0f;
        const float cx = (kLeft + kRight) * 0.5f;
        if (mode_ == Mode::invite)
        {
            choices_[0] = {tr("Sign in"), Icon::user};
            choices_[1] = {tr("Discover"), Icon::trending};
            choice_names_[0] = "sign_in";
            choice_names_[1] = "discover";
            notice_.lay_out(fonts, cx, top, notice_body(), choices_, 2);
        }
        else if (mode_ == Mode::empty)
        {
            choices_[0] = {tr("Search"), Icon::search};
            choice_names_[0] = "search";
            notice_.lay_out(fonts, cx, top, notice_body(), choices_, 1);
        }
        else if (mode_ == Mode::kept || mode_ == Mode::trouble)
        {
            choices_[0] = {tr("Try again"), Icon::update};
            choice_names_[0] = "retry";
            notice_.lay_out(fonts, cx, top, notice_body(), choices_, 1);
        }
        choice_ = std::clamp(choice_, 0, std::max(notice_.count - 1, 0));
    }

    // The page's scroll: the strip shows while the focus is in it or on the first row.
    float wanted_scroll() const
    {
        if (zone_ != Zone::feed || lines_.empty())
            return 0.0f;
        const int at = line_of(focus_);
        if (at == 0)
            return 0.0f;
        const Line &line = lines_[static_cast<std::size_t>(at)];
        return line.y - (line.heads ? kHeadRest : kRowRest);
    }

    void advance(UpdateContext &context)
    {
        const float dt = context.dt;
        const bool reduced = context.reduced_motion;
        const float quick = reduced ? 60.0f : 20.0f;
        scroll_.target = wanted_scroll();
        scroll_.update(dt, reduced ? 60.0f : 12.0f);
        // The strip keeps the picked face among the twelve that show.
        float target = strip_scroll_.target;
        target = std::min(target, kStripPitch * static_cast<float>(pick_));
        target = std::max(target, kStripPitch * static_cast<float>(pick_ - (kStripShown - 1)));
        strip_scroll_.target = std::max(target, 0.0f);
        strip_scroll_.update(dt, reduced ? 60.0f : 14.0f);
        feed_focus_.target = zone_ == Zone::feed && !sheet_.open() ? 1.0f : 0.0f;
        feed_focus_.update(dt, quick);
        strip_focus_.target = zone_ == Zone::strip && !sheet_.open() ? 1.0f : 0.0f;
        strip_focus_.update(dt, quick);
        pick_pos_.target = static_cast<float>(pick_);
        pick_pos_.update(dt, quick);
        choice_pos_.target = static_cast<float>(choice_);
        choice_pos_.update(dt, quick);
        nudge_.update(dt, 10.0f);
        bar_.style.reduced_motion = reduced;
        bar_.update(dt);
        if (!order_.empty())
        {
            const Rect goal = grown(cell_rect(focus_), 0.05f, 5.0f);
            if (!ring_set_ || feed_focus_.value < 0.02f)
                ring_.snap(goal);
            else
                ring_.target(goal);
            ring_set_ = true;
        }
        ring_.update(dt, quick);
    }

    // ---- drawing ----------------------------------------------------------------------------

    void draw_bones(G &g) const
    {
        Enter enter(g, arrive(age_, 1, 0.05f, 0.4f), 18.0f);
        for (int i = 0; i < kStripShown; ++i)
        {
            const float cx = kStripX + kStripPitch * static_cast<float>(i);
            bone(g, {cx - 40.0f, kStripY - 40.0f, 80.0f, 80.0f}, 40.0f);
            bone(g, {cx - 44.0f, 296.0f, 88.0f, 16.0f}, 8.0f);
        }
        bone(g, {kLeft, 352.0f, 120.0f, 22.0f}, 11.0f);
        for (int i = 0; i < kColumns * 2; ++i)
            card_bone(g, {kLeft + static_cast<float>(i % kColumns) * kCellPitchX, kFeedY + static_cast<float>(i / kColumns) * (kCellPitchY + kSectionGap), kCellW, kCellH},
                      i < kColumns);
    }

    // Who the profile follows: a strip of faces, the one that is picked marked.
    void draw_strip(G &g, const DrawContext &context) const
    {
        if (strip_.empty())
            return;
        if (!has_all())
        {
            Enter enter(g, arrive(age_, 1, 0.05f, 0.5f), 14.0f);
            overline(g, tr("Followed on this PS5"), kLeft, 172.0f, g.k.ink3);
        }
        g.l.push_clip({kRail + 8.0f, kStripY - 90.0f, gfx::kVirtualWidth - kRail - 8.0f, 190.0f});
        const float scroll = strip_scroll_.value;
        const int count = static_cast<int>(strip_.size());
        for (int i = 0; i < count; ++i)
        {
            const float cx = strip_x(i, scroll);
            if (cx < kRail - 80.0f || cx > gfx::kVirtualWidth + 80.0f)
                continue;
            const Entry &face = strip_[static_cast<std::size_t>(i)];
            const model::Channel *channel = strip_channel(i);
            const float near = tween::clamp01(1.0f - std::fabs(pick_pos_.value - static_cast<float>(i)));
            const float focus = near * strip_focus_.value;
            const bool picked = i == pick_ && has_all();
            // A face at the edge of the screen fades: there are more, and none is cut in half.
            const float edge = tween::clamp01((gfx::kVirtualWidth - 16.0f - cx) / 90.0f) * tween::clamp01((cx - kRail - 20.0f) / 60.0f);
            Enter enter(g, arrive(age_, 2 + std::min(i, kStripShown), 0.03f, 0.4f) * edge, 18.0f);
            g.l.push_transform(1.0f + 0.1f * focus, cx, kStripY, 0.0f, 0.0f);
            Color glow = g.k.focus;
            if (channel == nullptr)
            {
                // All of them: a plate while it is the pick, a quiet disc otherwise.
                const float on = picked ? 1.0f : 0.0f;
                g.l.circle(cx, kStripY, 44.0f, gfx::mix(g.k.raised, g.k.plate, on));
                icon(g, Icon::grid, cx, kStripY, 34.0f, gfx::mix(g.k.ink, g.k.on_plate, on));
            }
            else
            {
                const Picture picture = context.images.get(channel->avatar, PictureSize::avatar);
                if (picture.ready())
                    glow = picture.glow;
                avatar(g, cx, kStripY, 40.0f, channel->name, channel->id, picture);
                if (face.live)
                {
                    // Live: a ring in the signature colour and the word.
                    g.l.ring(cx, kStripY, 47.0f, 3.5f, g.k.accent);
                    const char *label = tr("LIVE");
                    tag(g, cx - tag_width(g, label, 24.0f) * 0.5f, kStripY + 40.0f, label, g.k.accent, g.k.on_accent, 24.0f);
                }
                else if (picked && focus < 0.5f)
                    g.l.ring(cx, kStripY, 47.0f, 3.0f, g.k.ink); // the pick keeps its mark when the focus is elsewhere
                if (face.unseen && !face.live)
                {
                    g.l.circle(cx + 30.0f, kStripY - 30.0f, 9.0f, g.k.base);
                    g.l.circle(cx + 30.0f, kStripY - 30.0f, 6.5f, g.k.accent);
                }
            }
            g.l.pop_transform();
            if (focus > 0.01f)
            {
                Rect lit{cx - 48.0f, kStripY - 48.0f, 96.0f, 96.0f};
                lit.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
                focus_ring(g, grown(lit, 0.1f * focus), 48.0f * (1.0f + 0.1f * focus), glow, focus);
            }
            const bool strong = channel == nullptr || picked || focus > 0.5f;
            const std::string name = channel == nullptr ? std::string(tr("All")) : channel->name;
            const Face &face_of = strong ? g.f.semibold : g.f.regular;
            const float size = channel == nullptr ? 19.0f : 18.0f;
            text(g, face_of, fit(face_of, name, size, 128.0f), cx, 310.0f, size, strong ? g.k.ink : g.k.ink2, gfx::Align::center);
        }
        g.l.pop_clip();
    }

    void draw_feed(G &g, const DrawContext &context) const
    {
        const float scroll = scroll_.value;
        const int count = static_cast<int>(order_.size());
        const float zone_focus = feed_focus_.value;
        if (count == 0)
        {
            draw_nothing(g);
            return;
        }
        const int focused_line = line_of(focus_);
        for (std::size_t i = 0; i < lines_.size(); ++i)
        {
            const Line &line = lines_[i];
            if (!line.heads || line.y - kHeadAbove - scroll < kClipTop - 40.0f || line.y - scroll > gfx::kVirtualHeight)
                continue;
            Enter enter(g, arrive(age_, 4, 0.06f, 0.5f), 18.0f);
            const bool here = lines_[static_cast<std::size_t>(focused_line)].day == line.day && zone_focus > 0.5f;
            text(g, g.f.semibold, day_name(line.day), kLeft, line.y - kHeadAbove, kTypeShelf, here || i == 0 ? g.k.ink : g.k.ink2);
        }
        const Rect ring = ring_.value();
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < count; ++i)
            {
                if ((pass == 1) != (i == focus_ && zone_focus > 0.01f))
                    continue;
                const Rect r = cell_rect(i);
                const float y = r.y - scroll;
                if (y > gfx::kVirtualHeight || y + r.h + 120.0f < 0.0f)
                    continue;
                const model::Video &video = feed_.videos[static_cast<std::size_t>(order_[static_cast<std::size_t>(i)])];
                const float focus = under_ring(ring, r, 260.0f) * zone_focus;
                const bool whole = y + r.h + 104.0f <= kPageBottom;
                const Picture picture = context.images.get(video.thumb, PictureSize::card);
                Enter enter(g, arrive(age_, 6 + (i - lines_[static_cast<std::size_t>(line_of(i))].first), 0.05f, 0.45f), 26.0f);
                video_card(g, grown(r, 0.05f * focus, 5.0f * focus), video, picture, focus, whole ? 1.0f : 0.8f, whole ? 2 : 0);
                if (i == focus_ && focus > 0.01f)
                {
                    Rect lit = ring;
                    lit.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
                    focus_ring(g, lit, kRadiusThumb, picture.ready() ? picture.glow : g.k.focus, zone_focus);
                }
            }
        }
        if (feed_.fetching_more())
        {
            // The next page is on its way: its row waits as bones under the last one.
            const float y = lines_.back().y + kCellPitchY;
            for (int i = 0; i < kColumns; ++i)
                card_bone(g, {kLeft + static_cast<float>(i) * kCellPitchX, y, kCellW, kCellH}, y - scroll + kCellH + 104.0f <= kPageBottom);
        }
    }

    // The filter or the picked channel leaves nothing of what has been read.
    void draw_nothing(G &g) const
    {
        if (feed_.fetching_more())
        {
            for (int i = 0; i < kColumns; ++i)
                card_bone(g, {kLeft + static_cast<float>(i) * kCellPitchX, kFeedY, kCellW, kCellH});
            return;
        }
        Enter enter(g, arrive(age_, 4, 0.06f, 0.5f), 18.0f);
        const model::Channel *picked = strip_channel(pick_);
        std::string title;
        if (picked != nullptr)
            title = fill(filter_ == 0 ? tr("Nothing from {0} in your feed yet") : tr("Nothing from {0} under this filter"), picked->name);
        else
            title = filter_ == 1 ? tr("Nothing new today") : filter_ == 2 ? tr("Nobody is live right now") : filter_ == 3 ? tr("You are all caught up") : tr("Nothing in your feed yet");
        icon(g, Icon::info, kLeft + 24.0f, kFeedY + 2.0f, 44.0f, g.k.ink2);
        text(g, g.f.semibold, fit(g.f.semibold, title, kTypeShelf, kRight - kLeft - 80.0f), kLeft + 64.0f, kFeedY + 11.0f, kTypeShelf, g.k.ink);
        const char *way = picked != nullptr ? tr("Cross opens the channel with everything it has.") : tr("L1 and R1 turn the filter.");
        paragraph(g, g.f.regular, way, kLeft + 64.0f, kFeedY + 48.0f, kTypeBody, 1100.0f, 34.0f, g.k.ink2, 1);
    }

    Route route_;
    model::Profile profile_;
    Feed feed_;  // "library:subscriptions"
    Feed faces_; // "library:channels"
    Feed local_; // "subscriptions": followed on this console
    unsigned seen_ = 0;
    Mode mode_ = Mode::loading;
    Zone zone_ = Zone::feed;
    bool settled_ = false;
    bool dated_ = false; // the feed says how old its videos are
    ui::TabBar bar_;
    int filter_ = 0;
    int pick_ = 0; // the strip entry the feed is kept to; with an "All" tile that is entry 0
    int focus_ = 0;
    int choice_ = 0;
    std::vector<Entry> strip_;
    std::vector<int> order_; // the videos that show, as they stand: indices into the feed
    std::vector<Line> lines_;
    Choice choices_[2];
    const char *choice_names_[2] = {"", ""};
    Notice notice_;
    Sheet sheet_;

    float age_ = 0.0f;
    tween::Spring scroll_;
    tween::Spring strip_scroll_;
    tween::Spring feed_focus_;
    tween::Spring strip_focus_;
    tween::Spring pick_pos_;
    tween::Spring choice_pos_;
    ui::SpringRect ring_;
    bool ring_set_ = false;
    ui::Pulse nudge_;
};

} // namespace

std::unique_ptr<Screen> make_subscriptions()
{
    return std::make_unique<Subscriptions>();
}

} // namespace afterglow
