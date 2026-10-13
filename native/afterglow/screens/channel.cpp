// ProsperoTube Afterglow - the channel page.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A channel: its banner and name, Subscribe with the notification level and
// search in the channel, and its sections as tabs. route.argument is the
// channel ("UC..." or "channel:UC..."), route.title its name until the page
// has arrived.
//
// The page is the catalog "channel:<id>": its first row is the channel itself
// with the facts of ARCHITECTURE.md 5.5 (handle, subscribers, banner_url,
// subscribed, tabs, tab, sorts, sort; this screen also reads videos, notify
// and search), the rows after it are the section that catalog is. Every other
// section is "channel-tab:<id>:<section>" and, in another order,
// "channel-tab:<id>:<section>:<order>". A section is read when it is first
// shown and keeps its focus and its place for as long as the page lives.
//
// Subscribe, Unsubscribe and the notification level are the provider's acting
// rows "account:confirm:subscribe:<id>", "account:confirm:unsubscribe:<id>"
// and "notify:confirm:level:<level>:<id>" (ARCHITECTURE.md 5.6).

#include "afterglow/screens/channel.hpp"

namespace afterglow
{

namespace channel_page
{

namespace
{

// The level after this one: Cross on the bell goes round.
const char *next_level(std::string_view level)
{
    if (level == "all")
        return "personalized";
    if (level == "personalized")
        return "none";
    return "all";
}

std::vector<std::string> split(std::string_view list)
{
    std::vector<std::string> out;
    std::size_t from = 0;
    while (from <= list.size())
    {
        const std::size_t comma = std::min(list.find(',', from), list.size());
        std::size_t a = from;
        std::size_t b = comma;
        while (a < b && list[a] == ' ')
            ++a;
        while (b > a && list[b - 1] == ' ')
            --b;
        if (b > a)
            out.emplace_back(list.substr(a, b - a));
        from = comma + 1;
    }
    return out;
}

const char *zone_name(Zone zone)
{
    switch (zone)
    {
    case Zone::actions:
        return "actions";
    case Zone::tabs:
        return "tabs";
    case Zone::sort:
        return "sort";
    case Zone::content:
        return "content";
    case Zone::trouble:
        return "trouble";
    }
    return "";
}

const char *button_name(Button button)
{
    switch (button)
    {
    case Button::subscribe:
        return "subscribe";
    case Button::sign_in:
        return "sign_in";
    case Button::notify:
        return "notify";
    case Button::search:
        return "search";
    }
    return "";
}

} // namespace

const char *section_label(std::string_view name)
{
    if (name == "home")
        return tr("Home");
    if (name == "videos")
        return tr("Videos");
    if (name == "shorts")
        return tr("Shorts");
    if (name == "live")
        return tr("Live");
    if (name == "playlists")
        return tr("Playlists");
    return "";
}

const char *order_label(std::string_view name)
{
    if (name == "newest")
        return tr("Latest");
    if (name == "popular")
        return tr("Popular");
    if (name == "oldest")
        return tr("Oldest");
    return "";
}

const char *level_label(std::string_view level)
{
    if (level == "all")
        return tr("All");
    if (level == "none")
        return tr("Off");
    return tr("Personalised");
}

Channel::Channel() : alive_(std::make_shared<int>(0))
{
    bar_.style.kind = ui::TabKind::underline;
    bar_.style.on_page = true;
    bar_.style.text_size = 26.0f;
    bar_.style.padding = 22.0f;
    bar_.style.gap = 12.0f;
    // REQUEST: update() has no Look, so the bar cannot wear the look's theme
    // there; draw() dresses a copy. The pace is the same in all four looks.
    bar_.style.theme.omega = 18.0f;
    bar_.style.theme.damping = 0.9f;
}

void Channel::enter(const Route &route, UpdateContext &context, bool restored)
{
    profile_ = context.services.data.profile();
    if (restored)
    {
        // Back from a video, the search or the sign-in page: everything stays where it was, read again.
        base_.restart();
        for (Tab &each : tabs_)
            each.feed.restart();
        read(context);
        return;
    }
    route_ = route;
    id_ = starts_with(route.argument, "channel:") ? route.argument.substr(8) : route.argument;
    base_.start("channel:" + id_);
    head_ = Head{};
    tabs_.clear();
    bar_.set_tabs({});
    active_ = 0;
    zone_ = Zone::tabs;
    button_ = 0;
    doing_ = Doing::nothing;
    last_ = Doing::nothing;
    act_ = Act{};
    outcome_ = "idle";
    override_ = false;
    subscribed_ = false;
    level_ = "personalized";
    asked_unsubscribe_ = false;
    sort_open_ = false;
    age_ = 0.0f;
    scroll_.snap(0.0f);
    content_focus_.snap(0.0f);
    sub_amount_.snap(0.0f);
    follow_width_.snap(0.0f);
    ring_set_ = false;
    read(context);
}

void Channel::update(const InputFrame &input, UpdateContext &context)
{
    age_ += context.dt;
    read(context);
    finish_action(context);
    sheet_.update(context);
    if (sheet_.take_changed())
    {
        // Something was done to the channel from its sheet: the page says how it stands now.
        override_ = false;
        context.services.data.forget(base_.catalog);
    }
    lay_out(context.services.fonts);
    if (context.top)
    {
        if (asked_unsubscribe_)
        {
            asked_unsubscribe_ = false;
            start(Doing::unsubscribe, "account:confirm:unsubscribe:" + id_, context);
        }
        handle(input, context);
        lay_out(context.services.fonts);
        // The page takes the banner's colours: a channel is one place, whatever is in focus.
        context.nav.ambient(!head_.banner.empty() ? head_.banner : head_.avatar);
    }
    advance(context);
    ask_pictures(context);
}

// Asks for the pictures of what is in view as soon as it is known, without waiting to be drawn: a page
// that arrives with its pictures on the way shows them sooner, and a screen that is updated without being
// drawn (the board harness draws the last frame only) has them at all.
void Channel::ask_pictures(UpdateContext &context)
{
    if (!head_.known)
        return;
    ImageCache &images = context.services.images;
    images.get(head_.banner, PictureSize::hero);
    images.get(head_.avatar, PictureSize::avatar);
    images.get(!head_.banner.empty() ? head_.banner : head_.avatar, PictureSize::ambient);
    if (!has_tab())
        return;
    const Tab &now = tab();
    const int count = static_cast<int>(now.cells.size());
    // A grid: the row in focus and the two below it. Home: the featured video and the uploads that show.
    const int from = now.home() ? 0 : now.focus / kColumns * kColumns;
    const int to = now.home() ? std::min(count, 1 + kLatestShown + 1) : std::min(count, from + 3 * kColumns);
    for (int i = from; i < to; ++i)
    {
        const Cell &cell = now.cells[static_cast<std::size_t>(i)];
        images.get(cell.video ? cell.v.thumb : cell.thumb, now.home() && i == 0 ? PictureSize::hero : PictureSize::card);
    }
}

const Channel::Cell *Channel::focused_cell() const
{
    if (!has_tab() || tab().cells.empty())
        return nullptr;
    return &tab().cells[static_cast<std::size_t>(tab().focus)];
}

std::string Channel::catalog_of(const Tab &of) const
{
    if (!of.chosen && (of.name == head_.tab || head_.tab.empty()))
        return base_.catalog;
    std::string id = "channel-tab:" + id_ + ":" + of.name;
    if (of.chosen && of.sort >= 0)
        id += ":" + of.sorts[static_cast<std::size_t>(of.sort)];
    return id;
}

// ---- data -------------------------------------------------------------------------------

void Channel::read(UpdateContext &context)
{
    Data &data = context.services.data;
    if (base_.read(data))
        read_head();
    if (!head_.known || !has_tab())
        return;
    Tab &now = tab();
    if (!now.feed.open)
        now.feed.start(catalog_of(now));
    now.feed.read(data);
    if (now.seen != now.feed.version)
        read_cells(now);
}

void Channel::read_head()
{
    if (base_.load != model::Load::ready)
    {
        if (base_.load != model::Load::loading && !head_.known)
            zone_ = Zone::trouble;
        return; // what was known stays on screen while the page is read again
    }
    std::size_t at = base_.channels.size();
    for (std::size_t i = 0; i < base_.channels.size(); ++i)
    {
        if (base_.channels[i].id == id_)
        {
            at = i;
            break;
        }
    }
    Head head;
    head.known = true;
    head.name = route_.title;
    std::vector<std::string> sections;
    if (at < base_.channels.size())
    {
        const model::Channel &channel = base_.channels[at];
        const model::Row &row = *base_.row_of_channel(at);
        head.name = channel.name;
        head.avatar = channel.avatar;
        head.subscribers = row.fact("subscribers").empty() ? channel.subscribers : row.fact("subscribers");
        head.handle = row.fact("handle");
        head.banner = row.fact("banner_url");
        head.videos = row.fact("videos");
        head.level = row.fact("notify");
        head.tab = row.fact("tab");
        head.subscribed = row.has("subscribed");
        head.can_search = row.fact("search").empty() || row.has("search");
        for (std::string &name : split(row.fact("tabs")))
        {
            if (section_label(name)[0] != '\0')
                sections.push_back(std::move(name));
        }
    }
    if (sections.empty())
    {
        // A channel that names no sections is the list its catalog holds.
        if (section_label(head.tab)[0] == '\0')
            head.tab = "videos";
        sections.push_back(head.tab);
    }
    if (std::find(sections.begin(), sections.end(), head.tab) == sections.end())
        head.tab = sections.front();
    const bool first = !head_.known;
    head_ = std::move(head);

    bool same = sections.size() == tabs_.size();
    for (std::size_t i = 0; same && i < sections.size(); ++i)
        same = tabs_[i].name == sections[i];
    if (!same)
    {
        std::vector<Tab> next;
        std::vector<ui::TabItem> items;
        const std::string was = has_tab() ? tab().name : head_.tab;
        for (const std::string &name : sections)
        {
            auto kept = std::find_if(tabs_.begin(), tabs_.end(), [&](const Tab &each) { return each.name == name; });
            Tab made = kept != tabs_.end() ? std::move(*kept) : Tab{};
            made.name = name;
            made.label = section_label(name);
            items.push_back({made.label});
            next.push_back(std::move(made));
        }
        tabs_ = std::move(next);
        active_ = 0;
        for (std::size_t i = 0; i < tabs_.size(); ++i)
        {
            if (tabs_[i].name == was)
                active_ = static_cast<int>(i);
        }
        bar_.set_tabs(std::move(items));
        bar_.set_active(active_, true);
    }
    if (first)
    {
        zone_ = Zone::tabs;
        age_ = std::min(age_, 0.12f); // the page assembles when it arrives, not before
    }
    if (!override_ && doing_ == Doing::nothing)
    {
        subscribed_ = head_.subscribed;
        level_ = head_.level.empty() ? "personalized" : head_.level;
        if (first)
            sub_amount_.snap(subscribed_ ? 1.0f : 0.0f);
    }
}

void Channel::read_cells(Tab &of)
{
    of.seen = of.feed.version;
    of.cells.clear();
    std::size_t video = 0;
    const model::Row *own = nullptr; // the channel's row of this section's catalog
    for (std::size_t r = 0; r < of.feed.rows.size(); ++r)
    {
        const model::Row &row = of.feed.rows[r];
        if (video < of.feed.video_row.size() && of.feed.video_row[video] == static_cast<int>(r))
        {
            Cell cell;
            cell.v = of.feed.videos[video++];
            of.cells.push_back(std::move(cell));
        }
        else if (starts_with(row.id, "channel:"))
        {
            if (own == nullptr)
                own = &row;
        }
        else if (row.folder && !row.interface_text)
        {
            Cell cell;
            cell.video = false;
            cell.id = row.id;
            cell.title = row.title;
            cell.thumb = row.thumb;
            cell.count = row.fact("count");
            cell.note = row.fact("updated");
            of.cells.push_back(std::move(cell));
        }
    }
    if (own != nullptr && of.feed.load == model::Load::ready)
    {
        std::vector<std::string> sorts;
        for (std::string &name : split(own->fact("sorts")))
        {
            if (order_label(name)[0] != '\0')
                sorts.push_back(std::move(name));
        }
        const std::string picked = of.chosen && of.sort >= 0 && of.sort < static_cast<int>(of.sorts.size())
                                       ? of.sorts[static_cast<std::size_t>(of.sort)]
                                       : own->fact("sort");
        of.sorts = std::move(sorts);
        of.sort = -1;
        for (std::size_t i = 0; i < of.sorts.size(); ++i)
        {
            if (of.sorts[i] == picked)
                of.sort = static_cast<int>(i);
        }
    }
    // The focus stays on what it was on, or on the nearest thing that is still there.
    const int count = static_cast<int>(of.cells.size());
    of.focus = std::clamp(of.focus, 0, std::max(count - 1, 0));
    of.list_focus = std::clamp(of.list_focus, 1, std::max(count - 1, 1));
}

// ---- the account ------------------------------------------------------------------------

void Channel::start(Doing doing, const std::string &row, UpdateContext &context)
{
    doing_ = doing;
    last_ = doing;
    outcome_ = "idle";
    act_.start(context.services.data, row);
}

void Channel::finish_action(UpdateContext &context)
{
    if (doing_ == Doing::nothing)
        return;
    const Page *answer = nullptr;
    const model::Load result = act_.poll(context.services.data, &answer);
    if (result == model::Load::loading || result == model::Load::idle)
        return;
    const Doing done = doing_;
    doing_ = Doing::nothing;
    const float x = buttons_.empty() ? 1400.0f : buttons_.front().rect.cx();
    if (result == model::Load::ready)
    {
        override_ = true; // the page that was read is older than this answer
        outcome_ = "confirmed";
        if (done == Doing::subscribe)
            subscribed_ = true;
        else if (done == Doing::unsubscribe)
            subscribed_ = false;
        else
            level_ = wanted_level_;
        // The button itself confirms: its sound, a tick in the hand, no toast.
        context.feedback.play(done == Doing::unsubscribe ? audio::Cue::favorite_off : audio::Cue::favorite_on, 1.0f, ui::pan_for_x(x));
        context.feedback.rumble(0.3f, 0.06f);
        if (!context.reduced_motion)
            pop_.trigger();
        // What lists this channel among the subscriptions is older than the account now.
        Data &data = context.services.data;
        data.forget(base_.catalog);
        if (done != Doing::level)
        {
            data.forget("library:channels");
            data.forget("library:subscriptions");
        }
        return;
    }
    outcome_ = "failed";
    const std::string message = answer != nullptr ? answer->error : std::string();
    context.feedback.play(audio::Cue::error, 1.0f, ui::pan_for_x(x));
    context.feedback.rumble(0.25f, 0.05f);
    if (!context.reduced_motion)
        shake_.trigger();
    if (!message.empty())
        context.nav.toast(message, ToastKind::error);
    else if (result == model::Load::offline)
        context.nav.toast(tr("YouTube cannot be reached. Nothing was changed."), ToastKind::error);
    else
        context.nav.toast(tr("YouTube did not confirm the change. Nothing was changed."), ToastKind::error);
}

void Channel::press_button(const Slot &slot, const InputFrame &input, UpdateContext &context)
{
    switch (slot.button)
    {
    case Button::sign_in:
        context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
        break;
    case Button::subscribe:
        if (doing_ != Doing::nothing)
            break; // one thing at a time: the answer is on its way
        if (!subscribed_)
        {
            context.feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(slot.rect.cx()));
            start(Doing::subscribe, "account:confirm:subscribe:" + id_, context);
        }
        else
        {
            // Leaving a channel is asked once more; the dialog brings its own sound.
            const std::weak_ptr<int> alive = alive_;
            context.nav.confirm(fill(tr("Unsubscribe from {0}?"), head_.name), tr("Its new videos will no longer arrive under Subscriptions."),
                                tr("Unsubscribe"), [this, alive](bool yes)
                                {
                                    if (!alive.expired() && yes)
                                        asked_unsubscribe_ = true;
                                });
        }
        break;
    case Button::notify:
        if (doing_ != Doing::nothing)
            break;
        if (!subscribed_)
        {
            refuse(context, input, shake_, slot.rect.cx());
            last_ = Doing::level;
            context.nav.toast(tr("Subscribe first to choose its notifications."), ToastKind::info);
            break;
        }
        wanted_level_ = next_level(level_);
        context.feedback.play(audio::Cue::toggle, wanted_level_ == "none" ? 0.94f : 1.08f, ui::pan_for_x(slot.rect.cx()));
        start(Doing::level, "notify:confirm:level:" + wanted_level_ + ":" + id_, context);
        break;
    case Button::search:
        search(input, context);
        break;
    }
}

// ---- input ------------------------------------------------------------------------------

void Channel::search(const InputFrame &input, UpdateContext &context)
{
    if (!head_.can_search)
    {
        refuse(context, input, nudge_);
        return;
    }
    // The search page takes the scope as its argument ("channel-search:<id>", the provider's catalog of a
    // search in a channel); what it finds is the grid of that search.
    context.nav.push(Route{ScreenId::search, "channel-search:" + id_, fill(tr("Search in {0}"), head_.name)}, Move::push);
}

void Channel::retry(UpdateContext &context)
{
    context.feedback.play(audio::Cue::select);
    context.services.data.refresh();
    age_ = 0.0f;
}

void Channel::go(Zone zone, UpdateContext &context)
{
    if (zone == zone_)
        return;
    zone_ = zone;
    const float row = zone == Zone::actions ? 0.0f : zone == Zone::content ? 2.0f : 1.0f;
    context.feedback.play(audio::Cue::tab, 1.10f - 0.07f * row);
}

void Channel::turn(int direction, const InputFrame &input, UpdateContext &context)
{
    if (tabs_.size() < 2)
    {
        refuse(context, input, nudge_);
        return;
    }
    if (bar_.step(direction, input, context.feedback) != ui::Event::changed)
        return;
    active_ = bar_.active();
    if (zone_ == Zone::sort || (zone_ == Zone::content && tab().feed.open && !can_focus(tab())))
        zone_ = Zone::tabs;
}

void Channel::handle(const InputFrame &input, UpdateContext &context)
{
    if (sheet_.open())
    {
        Route route;
        if (sheet_.handle(input, context, &route))
            context.nav.push(route, Move::push);
        return;
    }
    if (sort_open_)
    {
        handle_sort_menu(input, context);
        return;
    }
    if (pressed(input, Action::back))
    {
        context.nav.back();
        return;
    }
    if (!head_.known)
    {
        if (zone_ == Zone::trouble && pressed(input, Action::confirm))
            retry(context);
        else if (input.nav == Direction::left)
            context.nav.open_rail();
        else if (input.nav != Direction::none && zone_ == Zone::trouble)
            refuse(context, input, nudge_);
        return;
    }
    if (pressed(input, Action::page_next) || pressed(input, Action::page_prev))
    {
        turn(pressed(input, Action::page_next) ? 1 : -1, input, context);
        return;
    }
    if (pressed(input, Action::west))
    {
        search(input, context);
        return;
    }
    if (!has_tab())
        return;
    // A zone that has gone (the order of a section that offers none, an empty section) gives the focus to the tabs.
    if ((zone_ == Zone::content && !can_focus(tab())) || (zone_ == Zone::sort && tab().sorts.size() < 2) ||
        (zone_ == Zone::actions && buttons_.empty()) || zone_ == Zone::trouble)
        zone_ = Zone::tabs;
    const Cell *cell = zone_ == Zone::content ? focused_cell() : nullptr;
    if (pressed(input, Action::touch))
    {
        // The actions of the video in focus; anywhere else, those of the channel itself.
        if (cell != nullptr && cell->video)
            sheet_.show(cell->v.id, cell->v.title, context);
        else if (cell == nullptr)
            sheet_.show("channel:" + id_, head_.name, context);
        return;
    }
    switch (zone_)
    {
    case Zone::actions:
        handle_actions(input, context);
        break;
    case Zone::tabs:
        handle_tabs(input, context);
        break;
    case Zone::sort:
        handle_sort(input, context);
        break;
    case Zone::content:
        if (tab().cells.empty())
            handle_retry(input, context);
        else if (tab().home())
            handle_home(tab(), input, context);
        else
            handle_grid(tab(), input, context);
        break;
    case Zone::trouble:
        break;
    }
}

void Channel::handle_actions(const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(buttons_.size());
    button_ = std::clamp(button_, 0, count - 1);
    const Slot slot = buttons_[static_cast<std::size_t>(button_)];
    if (input.nav == Direction::left && button_ == 0)
        context.nav.open_rail();
    else if (input.nav == Direction::left || input.nav == Direction::right)
    {
        const int next = button_ + (input.nav == Direction::right ? 1 : -1);
        if (next < count)
        {
            button_ = next;
            context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(buttons_[static_cast<std::size_t>(next)].rect.cx()));
        }
        else
        {
            refuse(context, input, nudge_, slot.rect.cx());
        }
    }
    else if (input.nav == Direction::down)
        go(Zone::tabs, context);
    else if (input.nav == Direction::up)
        refuse(context, input, nudge_, slot.rect.cx());
    else if (pressed(input, Action::confirm))
        press_button(slot, input, context);
}

void Channel::handle_tabs(const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(tabs_.size());
    if (input.nav == Direction::left && active_ == 0)
        context.nav.open_rail();
    else if (input.nav == Direction::right && active_ == count - 1 && tab().sorts.size() > 1)
        go(Zone::sort, context);
    else if (input.nav == Direction::left || input.nav == Direction::right)
    {
        if (bar_.handle(input, context.feedback) == ui::Event::changed)
            active_ = bar_.active();
    }
    else if (input.nav == Direction::up && !buttons_.empty())
        go(Zone::actions, context);
    else if ((input.nav == Direction::down || pressed(input, Action::confirm)) && can_focus(tab()))
        enter_content(tab(), tab().home() ? 0 : tab().focus % kColumns, context);
    else if (input.nav != Direction::none)
        refuse(context, input, nudge_, tab_rect_.cx());
}

void Channel::handle_sort(const InputFrame &input, UpdateContext &context)
{
    if (input.nav == Direction::left)
        go(Zone::tabs, context);
    else if (input.nav == Direction::up && !buttons_.empty())
        go(Zone::actions, context);
    else if (input.nav == Direction::down && can_focus(tab()))
        enter_content(tab(), kColumns - 1, context);
    else if (input.nav != Direction::none)
        refuse(context, input, nudge_, sort_chip_.cx());
    else if (pressed(input, Action::confirm))
    {
        sort_open_ = true;
        sort_pick_ = std::max(tab().sort, 0);
        sort_pos_.snap(static_cast<float>(sort_pick_));
        context.feedback.play(audio::Cue::modal_open, 1.0f, ui::pan_for_x(sort_chip_.cx()));
    }
}

// Down from the tabs: the first row of a grid, at the column the focus last had.
void Channel::enter_content(Tab &of, int column, UpdateContext &context)
{
    if (!of.cells.empty())
    {
        const int last = static_cast<int>(of.cells.size()) - 1;
        if (of.home())
            of.focus = column == 0 ? 0 : std::min(of.list_focus, last);
        else
            of.focus = std::min(column, last);
    }
    go(Zone::content, context);
}

void Channel::handle_retry(const InputFrame &input, UpdateContext &context)
{
    if (pressed(input, Action::confirm))
        retry(context);
    else if (input.nav == Direction::up)
        go(Zone::tabs, context);
    else if (input.nav == Direction::left)
        context.nav.open_rail();
    else if (input.nav != Direction::none)
        refuse(context, input, nudge_, retry_.cx());
}

void Channel::handle_home(Tab &of, const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(of.cells.size());
    int next = of.focus;
    bool edge = false;
    switch (input.nav)
    {
    case Direction::left:
        if (of.focus == 0)
        {
            context.nav.open_rail();
            return;
        }
        of.list_focus = of.focus;
        next = 0;
        break;
    case Direction::right:
        if (of.focus == 0 && count > 1)
            next = std::clamp(of.list_focus, 1, count - 1);
        else
            edge = true;
        break;
    case Direction::up:
        if (of.focus <= 1)
        {
            go(Zone::tabs, context);
            return;
        }
        next = of.focus - 1;
        break;
    case Direction::down:
        if (of.focus >= 1 && of.focus + 1 < count)
            next = of.focus + 1;
        else
            edge = true;
        break;
    case Direction::none:
        break;
    }
    move_focus(of, next, edge, input, context);
    press_cell(of, input, context);
}

void Channel::handle_grid(Tab &of, const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(of.cells.size());
    const int column = of.focus % kColumns;
    int next = of.focus;
    bool edge = false;
    switch (input.nav)
    {
    case Direction::left:
        if (column == 0)
        {
            context.nav.open_rail();
            return;
        }
        next = of.focus - 1;
        break;
    case Direction::right:
        if (column < kColumns - 1 && of.focus + 1 < count)
            next = of.focus + 1;
        else
            edge = true;
        break;
    case Direction::up:
        if (of.focus < kColumns)
        {
            // What is above the first row: the tabs, and over the last column the order of the section.
            go(column == kColumns - 1 && of.sorts.size() > 1 ? Zone::sort : Zone::tabs, context);
            return;
        }
        next = of.focus - kColumns;
        break;
    case Direction::down:
        if (of.focus + kColumns < count)
            next = of.focus + kColumns;
        else if (of.focus / kColumns < (count - 1) / kColumns)
            next = count - 1; // the last row is shorter: its last card
        else
            edge = true;
        break;
    case Direction::none:
        break;
    }
    move_focus(of, next, edge, input, context);
    // The next page is asked for two rows before the end, so it is there when the focus is.
    if ((count - 1) / kColumns - of.focus / kColumns <= 1)
        of.feed.want_more();
    press_cell(of, input, context);
}

void Channel::move_focus(Tab &of, int next, bool edge, const InputFrame &input, UpdateContext &context)
{
    if (edge)
    {
        refuse(context, input, nudge_, on_screen(cell_rect(of, of.focus, true)).cx());
    }
    else if (next != of.focus)
    {
        of.focus = next;
        if (of.home() && next >= 1)
            of.list_focus = next;
        context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(cell_rect(of, next, true).cx()));
    }
}

void Channel::press_cell(Tab &of, const InputFrame &input, UpdateContext &context)
{
    if (!pressed(input, Action::confirm))
        return;
    const Cell &cell = of.cells[static_cast<std::size_t>(of.focus)];
    if (cell.video)
        family::play(cell.v, cell_rect(of, of.focus, true).cx(), context);
    else
        context.nav.push(Route{ScreenId::grid, cell.id, cell.title}, Move::push);
}

void Channel::handle_sort_menu(const InputFrame &input, UpdateContext &context)
{
    Tab &of = tab();
    const int count = static_cast<int>(of.sorts.size());
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int next = sort_pick_ + (input.nav == Direction::down ? 1 : -1);
        if (next >= 0 && next < count)
        {
            sort_pick_ = next;
            context.feedback.play(audio::Cue::focus, 1.06f - 0.03f * static_cast<float>(next), ui::pan_for_x(sort_chip_.cx()));
        }
        else
        {
            refuse(context, input, nudge_, sort_chip_.cx());
        }
    }
    else if (pressed(input, Action::confirm))
    {
        sort_open_ = false;
        if (sort_pick_ != of.sort)
        {
            // The section is read again in the order asked for, from its beginning.
            of.sort = sort_pick_;
            of.chosen = true;
            of.focus = 0;
            of.cells.clear();
            of.feed.start(catalog_of(of));
        }
        context.feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(sort_chip_.cx()));
    }
    else if (pressed(input, Action::back) || input.nav == Direction::left)
    {
        sort_open_ = false;
        context.feedback.play(audio::Cue::modal_close, 1.0f, ui::pan_for_x(sort_chip_.cx()));
    }
}

// ---- where things are -------------------------------------------------------------------

// The page's scroll for the section in focus: a grid past its first row rises under the tabs.
float Channel::wanted_scroll() const
{
    if (!has_tab() || zone_ != Zone::content || tab().home() || tab().cells.empty())
        return 0.0f;
    const int row = tab().focus / kColumns;
    return row == 0 ? 0.0f : kContentY - kRowRestY + kCellPitchY * static_cast<float>(row - 1);
}

Rect Channel::cell_rect(const Tab &of, int index, bool settled) const
{
    if (of.home())
    {
        if (index == 0)
            return kFeatured;
        const float scroll = settled ? of.list_scroll.target : of.list_scroll.value;
        return {kLatestX - 12.0f, kLatestY - 8.0f + kLatestPitch * static_cast<float>(index - 1) - scroll, kRight - kLatestX + 24.0f, 106.0f};
    }
    return {kLeft + static_cast<float>(index % kColumns) * kCellPitchX, kContentY + static_cast<float>(index / kColumns) * kCellPitchY, kCellW, kCellH};
}

Rect Channel::sort_menu() const
{
    const int count = has_tab() ? static_cast<int>(tab().sorts.size()) : 0;
    const float width = std::max(sort_chip_.w + 24.0f, 264.0f);
    return {kRight - width, sort_chip_.y + sort_chip_.h + 12.0f, width, 16.0f + kSortRow * static_cast<float>(count)};
}

Rect Channel::sort_row(int index) const
{
    const Rect menu = sort_menu();
    return {menu.x + 8.0f, menu.y + 8.0f + kSortRow * static_cast<float>(index), menu.w - 16.0f, kSortRow};
}

const char *Channel::sort_text(const Tab &of)
{
    return of.sort >= 0 ? order_label(of.sorts[static_cast<std::size_t>(of.sort)]) : tr("Default");
}

// Works out every rectangle the focus can be asked about, from the texts as they are now.
void Channel::lay_out(const ui::Fonts &fonts)
{
    if (!head_.known)
    {
        const Choice choices[1] = {{tr("Try again"), Icon::update}};
        trouble_.lay_out(fonts, (kLeft + kRight) * 0.5f, 300.0f,
                         base_.error.empty() ? tr("The channel did not arrive. Check the connection and try again.") : base_.error.c_str(), choices, 1);
        return;
    }
    // The buttons stand at the right edge, the search last: Subscribe grows to the left.
    buttons_.clear();
    float right = kRight;
    Slot find{Button::search, {}, {}};
    if (head_.can_search)
    {
        find.rect = {right - kActionH, kActionY, kActionH, kActionH};
        right = find.rect.x - 16.0f;
    }
    Slot bell{Button::notify, {}, level_label(doing_ == Doing::level ? wanted_level_ : level_)};
    if (profile_.signed_in)
    {
        const float width = std::max(200.0f, button_width(fonts, bell.label));
        bell.rect = {right - width, kActionY, width, kActionH};
        right = bell.rect.x - 16.0f;
    }
    Slot follow{profile_.signed_in ? Button::subscribe : Button::sign_in, {}, {}};
    follow.label = !profile_.signed_in ? tr("Sign in to subscribe") : sub_amount_.value > 0.5f ? tr("Subscribed") : tr("Subscribe");
    const char *resting = !profile_.signed_in ? tr("Sign in to subscribe") : subscribed_ ? tr("Subscribed") : tr("Subscribe");
    follow_width_.target = std::max(220.0f, button_width(fonts, resting));
    if (follow_width_.value <= 0.0f)
        follow_width_.snap(follow_width_.target);
    follow.rect = {right - follow_width_.value, kActionY, follow_width_.value, kActionH};
    name_width_ = std::max(200.0f, follow.rect.x - 28.0f - kNameX);
    buttons_.push_back(follow);
    if (profile_.signed_in)
        buttons_.push_back(bell);
    if (head_.can_search)
        buttons_.push_back(find);
    button_ = std::clamp(button_, 0, static_cast<int>(buttons_.size()) - 1);

    const float y = tabs_y(scroll_.target);
    bar_.set_bounds({kLeft - 22.0f, y, 900.0f, kTabsH});
    tab_rect_ = has_tab() ? bar_.tab_rect(fonts, active_) : Rect{kLeft, y, 100.0f, kTabsH};
    sort_chip_ = {};
    retry_ = {kLeft + 64.0f, kContentY + 96.0f, std::max(220.0f, button_width(fonts, tr("Try again"))), 60.0f};
    if (has_tab() && tab().sorts.size() > 1)
    {
        const float width = chip_width(fonts, sort_text(tab()), 44.0f, true);
        sort_chip_ = {kRight - width, y + 6.0f, width, 44.0f};
    }
}

// ---- motion -----------------------------------------------------------------------------

void Channel::advance(UpdateContext &context)
{
    const float dt = context.dt;
    const bool reduced = context.reduced_motion;
    const float quick = reduced ? 60.0f : 20.0f;
    scroll_.target = wanted_scroll();
    scroll_.update(dt, reduced ? 60.0f : 12.0f);
    content_focus_.target = zone_ == Zone::content && !sort_open_ && !sheet_.open() ? 1.0f : 0.0f;
    content_focus_.update(dt, quick);
    button_pos_.target = static_cast<float>(button_);
    button_pos_.update(dt, quick);
    actions_focus_.target = zone_ == Zone::actions && !sheet_.open() ? 1.0f : 0.0f;
    actions_focus_.update(dt, quick);
    sort_focus_.target = zone_ == Zone::sort && !sort_open_ ? 1.0f : 0.0f;
    sort_focus_.update(dt, quick);
    sort_amount_.target = sort_open_ ? 1.0f : 0.0f;
    sort_amount_.update(dt, reduced ? 60.0f : 18.0f);
    sort_pos_.target = static_cast<float>(sort_pick_);
    sort_pos_.update(dt, reduced ? 60.0f : 22.0f);
    sub_amount_.target = subscribed_ ? 1.0f : 0.0f;
    sub_amount_.update(dt, reduced ? 60.0f : 13.0f);
    follow_width_.update(dt, reduced ? 60.0f : 13.0f);
    pop_.update(dt, 7.0f);
    shake_.update(dt, 9.0f);
    nudge_.update(dt, 10.0f);
    bar_.set_focused(zone_ == Zone::tabs && !sort_open_ && !sheet_.open());
    bar_.style.reduced_motion = reduced;
    bar_.update(dt);
    for (Tab &each : tabs_)
        each.list_scroll.update(dt, reduced ? 60.0f : 14.0f);
    if (!has_tab())
        return;
    Tab &now = tab();
    if (now.home())
    {
        // The list keeps the row in focus among the three that show.
        const int row = std::max((now.focus >= 1 ? now.focus : now.list_focus) - 1, 0);
        const float limit = kLatestPitch * static_cast<float>(std::max(static_cast<int>(now.cells.size()) - 1 - kLatestShown, 0));
        float target = now.list_scroll.target;
        target = std::min(target, kLatestPitch * static_cast<float>(row));
        target = std::max(target, kLatestPitch * static_cast<float>(row - (kLatestShown - 1)));
        now.list_scroll.target = std::clamp(target, 0.0f, limit);
    }
    if (!now.cells.empty())
    {
        const Rect cell = cell_rect(now, now.focus, true);
        const Rect target = now.home() ? (now.focus == 0 ? grown(cell, 0.025f, 4.0f) : cell) : grown(cell, 0.05f, 5.0f);
        if (!ring_set_ || content_focus_.value < 0.02f)
            ring_.snap(target);
        else
            ring_.target(target);
        ring_set_ = true;
    }
    ring_.update(dt, quick);
}

// ---- what the frame and the tests ask -------------------------------------------------------

std::span<const ui::Hint> Channel::hints() const
{
    static ui::Hint items[4];
    std::size_t count = 0;
    if (sheet_.open())
        return sheet_.hints();
    if (sort_open_)
    {
        items[count++] = {ui::Button::cross, tr("Choose")};
        items[count++] = {ui::Button::circle, tr("Close")};
        return {items, count};
    }
    if (!head_.known)
    {
        if (zone_ == Zone::trouble)
            items[count++] = {ui::Button::cross, tr("Try again")};
        items[count++] = {ui::Button::circle, tr("Back")};
        return {items, count};
    }
    const Cell *cell = zone_ == Zone::content ? focused_cell() : nullptr;
    if (cell != nullptr)
        items[count++] = {ui::Button::cross, cell->video ? tr("Play") : tr("Open")};
    else if (zone_ != Zone::tabs)
        items[count++] = {ui::Button::cross, tr("Choose")};
    if (tabs_.size() > 1)
        items[count++] = {ui::Button::l1, tr("Section"), ui::Button::r1};
    if (head_.can_search)
        items[count++] = {ui::Button::square, tr("Search in channel")};
    if (cell == nullptr || cell->video)
        items[count++] = {ui::Button::touchpad, tr("Actions")};
    return {items, count};
}

FocusInfo Channel::focus() const
{
    FocusInfo info;
    if (sheet_.open())
        return sheet_.focus();
    if (!head_.known)
    {
        info.id = zone_ == Zone::trouble ? "state/retry" : "state/loading";
        info.text = zone_ == Zone::trouble ? tr("Try again") : route_.title.c_str();
        info.rect = zone_ == Zone::trouble ? trouble_.buttons[0] : kBanner;
        info.total = 1;
        return info;
    }
    if (sort_open_ && has_tab())
    {
        const std::string &name = tab().sorts[static_cast<std::size_t>(sort_pick_)];
        info.id = "sort/" + name;
        info.text = order_label(name);
        info.rect = sort_row(sort_pick_);
        info.index = sort_pick_;
        info.total = static_cast<int>(tab().sorts.size());
        return info;
    }
    switch (zone_)
    {
    case Zone::actions:
        if (!buttons_.empty())
        {
            const Slot &slot = buttons_[static_cast<std::size_t>(button_)];
            info.id = std::string("action/") + button_name(slot.button);
            info.text = slot.label.empty() ? tr("Search in channel") : slot.label;
            info.rect = slot.rect;
            info.index = button_;
            info.total = static_cast<int>(buttons_.size());
        }
        break;
    case Zone::tabs:
        if (has_tab())
        {
            info.id = "tab/" + tab().name;
            info.text = tab().label;
            info.rect = tab_rect_;
            info.index = active_;
            info.total = static_cast<int>(tabs_.size());
        }
        break;
    case Zone::sort:
        info.id = "sort/menu";
        info.text = tr("Sort by");
        info.rect = sort_chip_;
        info.total = 1;
        break;
    case Zone::content:
        if (has_tab() && tab().cells.empty())
        {
            info.id = "state/retry";
            info.text = tr("Try again");
            info.rect = on_screen(retry_);
            info.total = 1;
        }
        else if (has_tab())
        {
            const Cell &cell = tab().cells[static_cast<std::size_t>(tab().focus)];
            info.id = tab().name + "/" + std::to_string(tab().focus);
            info.text = cell.video ? cell.v.title : cell.title;
            info.rect = on_screen(cell_rect(tab(), tab().focus, true));
            info.index = tab().focus;
            info.total = static_cast<int>(tab().cells.size());
        }
        break;
    case Zone::trouble:
        break;
    }
    return info;
}

std::vector<std::pair<std::string, std::string>> Channel::focusable() const
{
    std::vector<std::pair<std::string, std::string>> out;
    if (!head_.known)
    {
        if (zone_ == Zone::trouble)
            out.emplace_back("state/retry", tr("Try again"));
        return out;
    }
    for (const Slot &slot : buttons_)
        out.emplace_back(std::string("action/") + button_name(slot.button), slot.label.empty() ? tr("Search in channel") : slot.label);
    for (const Tab &each : tabs_)
        out.emplace_back("tab/" + each.name, each.label);
    if (has_tab())
    {
        if (tab().sorts.size() > 1)
            out.emplace_back("sort/menu", tr("Sort by"));
        for (std::size_t i = 0; i < tab().cells.size() && i < 24; ++i)
            out.emplace_back(tab().name + "/" + std::to_string(i), tab().cells[i].video ? tab().cells[i].v.title : tab().cells[i].title);
        if (tab().cells.empty() && tab_trouble(tab()))
            out.emplace_back("state/retry", tr("Try again"));
    }
    return out;
}

void Channel::snapshot(Snapshot &out) const
{
    out.set("channel", id_);
    out.set("load", load_name(base_.load));
    out.set("name", head_.name);
    out.set("zone", zone_name(zone_));
    out.set("guest", profile_.signed_in ? 0 : 1);
    out.set("subscribed", subscribed_ ? 1 : 0);
    out.set("level", level_);
    out.set("action", doing_ != Doing::nothing ? "pending" : outcome_);
    out.set("tabs", static_cast<int>(tabs_.size()));
    out.set("scroll", static_cast<int>(scroll_.target));
    out.set("sort_open", sort_open_ ? 1 : 0);
    sheet_.snapshot(out);
    std::vector<std::string> names;
    for (const Tab &each : tabs_)
        names.push_back(each.name);
    out.list("tabs", std::move(names));
    if (!has_tab())
        return;
    out.set("tab", tab().name);
    out.set("tab_load", load_name(tab().feed.load));
    out.set("cells", static_cast<int>(tab().cells.size()));
    out.set("pages", tab().feed.taken);
    out.set("sort", tab().sort >= 0 ? tab().sorts[static_cast<std::size_t>(tab().sort)] : std::string());
    out.set("catalog", tab().feed.catalog);
    std::vector<std::string> titles;
    for (std::size_t i = 0; i < tab().cells.size() && i < 12; ++i)
        titles.push_back(tab().cells[i].video ? tab().cells[i].v.title : tab().cells[i].title);
    out.list("cells", std::move(titles));
}

} // namespace channel_page

std::unique_ptr<Screen> make_channel()
{
    return std::make_unique<channel_page::Channel>();
}

} // namespace afterglow
