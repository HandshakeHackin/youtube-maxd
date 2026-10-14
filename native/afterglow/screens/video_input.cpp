// ProsperoTube Afterglow - the video page: where the focus goes, what the buttons do, one frame.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the page reads is in video.cpp, the drawing in video_draw.cpp.

#include "afterglow/screens/video.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow::video_page
{

namespace
{

const char *act_name(Act act)
{
    switch (act)
    {
    case Act::play:
        return "play";
    case Act::restart:
        return "restart";
    case Act::queue:
        return "queue";
    case Act::later:
        return "later";
    case Act::like:
        return "like";
    case Act::share:
        return "share";
    case Act::more:
        return "more";
    case Act::retry:
        return "retry";
    }
    return "";
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
    case model::Load::idle:
        return "idle";
    case model::Load::loading:
        break;
    }
    return "loading";
}

const char *request_name(Request::Kind kind)
{
    static constexpr const char *kNames[] = {"", "like", "unrate", "save", "unsave", "subscribe", "unsubscribe", "queue",
                                             "restart", "incognito", "menu"};
    return kNames[static_cast<std::size_t>(kind)];
}

} // namespace

// ---- requests --------------------------------------------------------------------------

void VideoPage::launch(const model::Video &video, UpdateContext &context)
{
    play_video(context, video, kHero);
}

void VideoPage::refuse(const InputFrame &input, UpdateContext &context)
{
    if (input.nav_repeat)
        return;
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
}

void VideoPage::edge(const InputFrame &input, UpdateContext &context, float direction)
{
    if (input.nav_repeat)
        return; // a held direction stays silent
    refuse(input, context);
    nudge_.trigger();
    nudge_dir_ = direction;
}

bool VideoPage::needs_account(UpdateContext &context)
{
    if (profile_.signed_in)
        return false;
    const std::weak_ptr<int> answer = answer_;
    context.nav.confirm(tr("Sign in to YouTube"),
                        tr("Liking, saving and subscribing change a YouTube account. This profile has none yet."), tr("Sign in"),
                        [answer](bool yes)
                        {
                            if (const std::shared_ptr<int> held = answer.lock())
                                *held = yes ? 2 : 0;
                        });
    return true;
}

void VideoPage::start(Request request, UpdateContext &context)
{
    // An account change is prepared by one row and sent by the row that answers.
    request.applying = !(starts_with(request.row, "account:confirm:") || starts_with(request.row, "playlist:confirm:"));
    request.ticket = context.services.data.act(request.row);
    request_ = std::move(request);
}

void VideoPage::ask_first(Request request, std::string_view title, std::string_view body, std::string_view confirm,
                          UpdateContext &context)
{
    after_dialog_ = std::move(request);
    const std::weak_ptr<int> answer = answer_;
    context.nav.confirm(title, body, confirm, [answer](bool yes)
    {
        if (const std::shared_ptr<int> held = answer.lock())
            *held = yes ? 1 : 3;
    });
}

void VideoPage::advance_request(UpdateContext &context)
{
    if (*answer_ == 1 && after_dialog_.has_value())
    {
        Request request = std::move(*after_dialog_);
        after_dialog_.reset();
        *answer_ = 0;
        start(std::move(request), context);
    }
    else if (*answer_ == 2)
    {
        *answer_ = 0;
        context.nav.push(Route{ScreenId::accounts, {}, {}});
    }
    else if (*answer_ == 3)
    {
        *answer_ = 0;
        after_dialog_.reset();
    }
    if (request_.kind == Request::Kind::none)
        return;
    Data &data = context.services.data;
    const Page &answer = data.action(request_.ticket);
    if (answer.load == model::Load::loading)
        return;
    if (answer.load != model::Load::ready)
    {
        failed(answer.error, context);
        return;
    }
    if (!request_.applying)
    {
        std::string apply;
        std::string words; // what the provider says instead, when it prepares nothing
        for (const model::Row &row : answer.rows)
        {
            if (starts_with(row.id, "account:apply:"))
                apply = row.id;
            else if (words.empty())
                words = row.title;
        }
        if (apply.empty())
        {
            failed(words, context);
            return;
        }
        request_.applying = true;
        request_.ticket = data.act(apply);
        return;
    }
    confirmed(answer, context);
}

void VideoPage::confirmed(const Page &answer, UpdateContext &context)
{
    const Request done = std::move(request_);
    request_ = Request{};
    Data &data = context.services.data;
    const float pan = ui::pan_for_x(button_rect(button_index(action_)).cx());
    switch (done.kind)
    {
    case Request::Kind::like:
        liked_ = true;
        like_pop_.trigger();
        context.feedback.play(audio::Cue::favorite_on, 1.12f, pan);
        context.feedback.rumble(0.3f, 0.06f);
        break;
    case Request::Kind::unrate:
        liked_ = false;
        context.feedback.play(audio::Cue::favorite_off, 1.0f, pan);
        break;
    case Request::Kind::save:
    case Request::Kind::unsave:
        saved_ = done.kind == Request::Kind::save;
        if (*saved_)
        {
            saved_pop_.trigger();
            context.feedback.rumble(0.3f, 0.06f);
        }
        context.feedback.play(*saved_ ? audio::Cue::favorite_on : audio::Cue::favorite_off, 1.0f, pan);
        data.forget("library:watch_later");
        break;
    case Request::Kind::subscribe:
    case Request::Kind::unsubscribe:
        subscribed_ = done.kind == Request::Kind::subscribe;
        if (*subscribed_)
        {
            subscribe_pop_.trigger();
            context.feedback.rumble(0.3f, 0.06f);
        }
        context.feedback.play(*subscribed_ ? audio::Cue::favorite_on : audio::Cue::favorite_off, 1.0f, ui::pan_for_x(subscribe_rect().cx()));
        context.nav.toast(fill(*subscribed_ ? tr("Subscribed to {0}") : tr("Unsubscribed from {0}"), details_.channel),
                          *subscribed_ ? ToastKind::success : ToastKind::info);
        data.forget("library:subscriptions");
        data.forget("library:channels");
        data.forget("subscriptions");
        break;
    case Request::Kind::queue:
        context.nav.toast(tr("Added to the queue"), ToastKind::success);
        data.forget("queue");
        break;
    case Request::Kind::restart:
        // The provider answers with the row to play, set back to its start.
        launch(answer.videos.empty() ? own() : answer.videos.front(), context);
        break;
    case Request::Kind::incognito:
        launch(own(), context); // the provider plays its next video without history
        break;
    case Request::Kind::menu:
        context.nav.toast(done.label, ToastKind::success);
        break;
    case Request::Kind::none:
        break;
    }
}

void VideoPage::failed(const std::string &message, UpdateContext &context)
{
    request_ = Request{};
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    // Nothing is sent a second time: the viewer decides, with the provider's words in front of them.
    context.nav.toast(message.empty() ? std::string(tr("YouTube did not confirm the change. Check your account before repeating it.")) : message,
                      ToastKind::error);
}

// ---- the layer over the page -------------------------------------------------------------

void VideoPage::open_layer(Layer layer, UpdateContext &context)
{
    layer_ = layer;
    shown_layer_ = layer;
    if (layer == Layer::menu)
    {
        sheet_catalog_.clear();
        sheet_title_.clear();
        list_.set_focus(0);
        list_.enter();
    }
    else
    {
        share_asked_ = true;
    }
    context.feedback.play(audio::Cue::modal_open, 1.0f, layer == Layer::menu ? 0.4f : 0.0f);
}

void VideoPage::close_layer(UpdateContext &context)
{
    (void)context;
    layer_ = Layer::none;
}

void VideoPage::open_row(const MenuRow &row, UpdateContext &context)
{
    const InputFrame press{};
    Request request;
    request.row = row.id;
    request.label = row.title;
    switch (row.kind)
    {
    case RowKind::account:
    case RowKind::local:
    {
        if (request_.kind != Request::Kind::none)
        {
            refuse(press, context); // one change at a time: the first is still on its way
            return;
        }
        if (row.kind == RowKind::account && needs_account(context))
        {
            close_layer(context);
            return;
        }
        request.kind = Request::Kind::menu;
        close_layer(context);
        // The two that reach beyond this video are asked about first.
        if (verb_of(row.id) == "block")
            ask_first(std::move(request), tr("Block this channel?"), tr("Its videos no longer appear in any list of this profile."),
                      tr("Block"), context);
        else if (account_action(row.id) == "dont_recommend_channel")
            ask_first(std::move(request), tr("Stop recommendations from this channel?"),
                      tr("YouTube is told not to recommend this channel to your account."), tr("Don't recommend"), context);
        else
            start(std::move(request), context);
        return;
    }
    case RowKind::restart:
    case RowKind::incognito:
        request.kind = row.kind == RowKind::restart ? Request::Kind::restart : Request::Kind::incognito;
        close_layer(context);
        start(std::move(request), context);
        return;
    case RowKind::channel:
        close_layer(context);
        context.nav.push(Route{ScreenId::channel, row.id.substr(8), row.title});
        return;
    case RowKind::reader:
        close_layer(context);
        context.nav.push(Route{ScreenId::reader, row.id, tr("Description")});
        return;
    case RowKind::grid:
        close_layer(context);
        context.nav.push(Route{ScreenId::grid, row.id, row.title});
        return;
    case RowKind::sublist:
        // The choices arrive in the sheet itself; Circle returns to the actions.
        sheet_catalog_ = row.id;
        sheet_title_ = row.title;
        list_.set_focus(0);
        list_.enter();
        return;
    case RowKind::note:
    case RowKind::hidden:
        refuse(press, context);
        return;
    }
}

// ---- input -----------------------------------------------------------------------------

void VideoPage::activate(Act act, UpdateContext &context)
{
    const InputFrame press{};
    const bool busy = request_.kind != Request::Kind::none;
    Request request;
    switch (act)
    {
    case Act::play:
        launch(own(), context);
        break;
    case Act::restart:
    case Act::queue:
    {
        request.row = menu_row(act == Act::restart ? "restart" : "next");
        if (busy)
        {
            refuse(press, context);
        }
        else if (request.row.empty())
        {
            // The profile's menu does not offer it (or has not arrived yet): say so instead of doing nothing.
            refuse(press, context);
            context.nav.toast(menu_load_ == model::Load::loading || menu_load_ == model::Load::idle
                                  ? tr("One moment: the actions of this video are still loading.")
                                  : tr("This profile's actions menu does not offer that for this video."),
                              ToastKind::info);
        }
        else
        {
            request.kind = act == Act::restart ? Request::Kind::restart : Request::Kind::queue;
            context.feedback.play(audio::Cue::select);
            start(std::move(request), context);
        }
        break;
    }
    case Act::later:
    case Act::like:
        if (needs_account(context))
            break;
        if (busy)
        {
            refuse(press, context);
            break;
        }
        if (act == Act::later)
        {
            request.kind = saved() ? Request::Kind::unsave : Request::Kind::save;
            request.row = std::string("account:confirm:") + (saved() ? "watch_later_remove:" : "watch_later_add:") + id_;
        }
        else
        {
            request.kind = liked() ? Request::Kind::unrate : Request::Kind::like;
            request.row = std::string("account:confirm:") + (liked() ? "unrate:" : "like:") + id_;
        }
        context.feedback.play(audio::Cue::select);
        start(std::move(request), context);
        break;
    case Act::share:
        open_layer(Layer::share, context);
        break;
    case Act::more:
        open_layer(Layer::menu, context);
        break;
    case Act::retry:
        context.feedback.play(audio::Cue::select);
        context.services.data.refresh();
        break;
    }
}

void VideoPage::input_page(const InputFrame &input, UpdateContext &context)
{
    if (pressed(input, Action::back))
    {
        context.nav.back();
        return;
    }
    if (pressed(input, Action::jump_next))
    {
        context.nav.push(Route{ScreenId::comments, id_, {}});
        return;
    }
    if (pressed(input, Action::touch))
    {
        open_layer(Layer::menu, context);
        return;
    }
    const Direction nav = input.nav;
    const int count = static_cast<int>(buttons_.size());
    const int related = static_cast<int>(related_.size());
    if (zone_ == Zone::actions)
    {
        const int index = std::max(button_index(action_), 0);
        if (nav == Direction::left || nav == Direction::right)
        {
            const int next = index + (nav == Direction::right ? 1 : -1);
            if (next < 0)
                context.nav.open_rail();
            else if (next >= count)
                edge(input, context, 1.0f);
            else
            {
                action_ = buttons_[static_cast<std::size_t>(next)].act;
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(button_rect(next).cx()));
            }
        }
        else if (nav == Direction::up)
        {
            if (can_subscribe())
            {
                zone_ = Zone::channel;
                context.feedback.play(audio::Cue::tab, 1.1f);
            }
            else
                edge(input, context, 0.0f);
        }
        else if (nav == Direction::down)
        {
            if (related > 0)
            {
                zone_ = Zone::related;
                context.feedback.play(audio::Cue::tab, 0.96f);
            }
            else
                edge(input, context, 0.0f);
        }
        else if (pressed(input, Action::confirm))
        {
            activate(action_, context);
        }
    }
    else if (zone_ == Zone::channel)
    {
        if (nav == Direction::down)
        {
            zone_ = Zone::actions;
            context.feedback.play(audio::Cue::tab, 1.03f);
        }
        else if (nav == Direction::left)
            context.nav.open_rail();
        else if (nav == Direction::up || nav == Direction::right)
            edge(input, context, nav == Direction::right ? 1.0f : 0.0f);
        else if (pressed(input, Action::confirm) && !needs_account(context))
        {
            Request request;
            if (request_.kind != Request::Kind::none)
            {
                refuse(input, context);
            }
            else if (subscribed())
            {
                request.kind = Request::Kind::unsubscribe;
                request.row = "account:confirm:unsubscribe:" + details_.channel_id;
                ask_first(std::move(request), fill(tr("Unsubscribe from {0}?"), details_.channel),
                          tr("Its new videos stop appearing under Subscriptions."), tr("Unsubscribe"), context);
            }
            else
            {
                request.kind = Request::Kind::subscribe;
                request.row = "account:confirm:subscribe:" + details_.channel_id;
                context.feedback.play(audio::Cue::select);
                start(std::move(request), context);
            }
        }
    }
    else
    {
        if (nav == Direction::left || nav == Direction::right)
        {
            const int next = related_col_ + (nav == Direction::right ? 1 : -1);
            if (next < 0)
                context.nav.open_rail();
            else if (next >= related)
            {
                if (!related_more_)
                    edge(input, context, 1.0f); // while more cards are on their way the end is not the end
            }
            else
            {
                related_col_ = next;
                const float x = kLeft + 140.0f + kRelatedPitch * static_cast<float>(next) - related_scroll_.offset();
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(x));
            }
        }
        else if (nav == Direction::up)
        {
            zone_ = Zone::actions;
            context.feedback.play(audio::Cue::tab, 1.03f);
        }
        else if (nav == Direction::down)
        {
            edge(input, context, 0.0f);
        }
        else if (related > 0 && pressed(input, Action::confirm))
        {
            launch(related_[static_cast<std::size_t>(related_col_)], context);
        }
        else if (related > 0 && pressed(input, Action::north))
        {
            // The card's picture grows into the next page's picture.
            const Rect card = grown(related_rect(related_col_, related_scroll_.offset()), 0.06f, 5.0f);
            context.nav.push(Route{ScreenId::video, related_[static_cast<std::size_t>(related_col_)].id, {}}, Move::zoom, card);
        }
    }
}

void VideoPage::input_menu(const InputFrame &input, UpdateContext &context)
{
    if (pressed(input, Action::touch))
    {
        context.feedback.play(audio::Cue::modal_close, 1.0f, 0.4f);
        close_layer(context);
        return;
    }
    if (sheet_rows_.empty())
    {
        // Nothing to choose yet (or at all): Circle is the one way on.
        if (pressed(input, Action::back))
        {
            context.feedback.play(audio::Cue::modal_close, 1.0f, 0.4f);
            if (!sheet_catalog_.empty())
                sheet_catalog_.clear();
            else
                close_layer(context);
        }
        else if (input.nav != Direction::none || pressed(input, Action::confirm))
            refuse(input, context);
        return;
    }
    const ui::Event event = list_.handle(input, context.feedback);
    if (event == ui::Event::activated)
    {
        const MenuRow row = sheet_rows_[static_cast<std::size_t>(std::clamp(list_.focus(), 0, static_cast<int>(sheet_rows_.size()) - 1))];
        open_row(row, context);
    }
    else if (event == ui::Event::cancelled)
    {
        if (!sheet_catalog_.empty())
        {
            sheet_catalog_.clear(); // back from a list of choices to the actions
            sheet_title_.clear();
            list_.enter();
        }
        else
        {
            close_layer(context);
        }
    }
}

void VideoPage::input_share(const InputFrame &input, UpdateContext &context)
{
    if (pressed(input, Action::back) || pressed(input, Action::confirm))
    {
        context.feedback.play(audio::Cue::modal_close);
        close_layer(context);
        return;
    }
    const int count = static_cast<int>(links_.size());
    if (input.nav == Direction::left || input.nav == Direction::right || input.nav == Direction::up || input.nav == Direction::down)
    {
        const int step = input.nav == Direction::right || input.nav == Direction::down ? 1 : -1;
        const int next = link_ + step;
        if (next < 0 || next >= count)
        {
            refuse(input, context);
        }
        else
        {
            link_ = next;
            link_chosen_ = true;
            context.feedback.play(audio::Cue::toggle, 1.0f + 0.04f * static_cast<float>(next),
                                  ui::pan_for_x(links_[static_cast<std::size_t>(next)].chip.cx()));
        }
    }
}

// ---- one frame ---------------------------------------------------------------------------

// Pictures are asked for while updating, a frame before they are drawn: the
// page ground's copy too, so the wash is there when the page is first shown.
void VideoPage::ask_pictures(UpdateContext &context)
{
    ImageCache &images = context.services.images;
    if (!details_.thumb.empty())
    {
        images.get(details_.thumb, PictureSize::ambient);
        images.get(details_.thumb, PictureSize::hero);
    }
    if (!details_.avatar.empty())
        images.get(details_.avatar, PictureSize::avatar);
    for (std::size_t i = 0; i < related_.size(); ++i)
    {
        const Rect r = related_rect(static_cast<int>(i), related_scroll_.position.target);
        if (r.x < gfx::kVirtualWidth + kRelatedPitch && r.x + r.w > kRail - kRelatedPitch)
            images.get(related_[i].thumb, PictureSize::card);
    }
}

void VideoPage::animate(UpdateContext &context)
{
    const float dt = context.dt;
    const float quick = context.reduced_motion ? 60.0f : 20.0f;
    const float soft = context.reduced_motion ? 60.0f : 13.0f;
    if (have_)
        since_ready_ += dt;
    action_pos_.target = static_cast<float>(std::max(button_index(action_), 0));
    action_pos_.update(dt, quick);
    channel_focus_.target = zone_ == Zone::channel ? 1.0f : 0.0f;
    channel_focus_.update(dt, quick);
    {
        const float wanted = button_width(context.services.fonts, subscribed() ? tr("Subscribed") : tr("Subscribe"), true);
        if (subscribe_w_.target == 0.0f)
            subscribe_w_.snap(wanted);
        subscribe_w_.target = wanted;
        subscribe_w_.update(dt, soft);
    }
    like_pop_.update(dt, 5.0f);
    saved_pop_.update(dt, 5.0f);
    subscribe_pop_.update(dt, 5.0f);
    nudge_.update(dt, 9.0f);
    if (likes_number_ >= 0)
    {
        // The count follows what YouTube confirmed here, on top of what details: said.
        const long long delta = (liked() ? 1 : 0) - (details_.rating == "like" ? 1 : 0);
        like_count_.target = static_cast<float>(likes_number_ + delta);
        if (!like_count_placed_)
        {
            like_count_.snap(like_count_.target);
            like_count_placed_ = true;
        }
        like_count_.update(dt, 9.0f);
    }
    layer_amount_.target = layer_ != Layer::none ? 1.0f : 0.0f;
    layer_amount_.update(dt, soft);
    const float start = static_cast<float>(related_col_) * kRelatedPitch;
    related_scroll_.reveal(start, start + kRelatedW, kRight - kLeft, kRelatedPitch * 0.5f);
    related_scroll_.update(dt, context.reduced_motion ? 60.0f : 12.0f);
    const Rect target = grown(related_rect(related_col_, related_scroll_.position.target), 0.06f, 5.0f);
    if (!ring_placed_ || zone_ != Zone::related)
    {
        ring_.snap(target); // the ring is where the focus will arrive, not where it last was
        ring_placed_ = true;
    }
    ring_.target(target);
    ring_.update(dt, quick);
    list_.update(dt);
}

void VideoPage::update(const InputFrame &input, UpdateContext &context)
{
    if (context.look)
        wear(list_.style, *context.look, context.reduced_motion);
    age_ += context.dt;
    clock_ = context.time;
    reduced_ = context.reduced_motion;
    advance_request(context); // answers first: what YouTube confirmed shows on this frame's buttons
    read(context);
    lay_out(context.services.fonts);
    build_buttons(context.services.fonts);
    fill_sheet(context);
    ask_pictures(context);
    if (context.top)
    {
        if (!details_.thumb.empty())
            context.nav.ambient(details_.thumb); // the page takes its colour from the video
        if (layer_ == Layer::menu)
            input_menu(input, context);
        else if (layer_ == Layer::share)
            input_share(input, context);
        else
            input_page(input, context);
        build_buttons(context.services.fonts); // a press may have changed what a button says
    }
    animate(context);
}

// ---- what the frame and the tests ask ---------------------------------------------------

std::span<const ui::Hint> VideoPage::hints() const
{
    static ui::Hint items[5];
    int count = 0;
    if (layer_ == Layer::menu)
    {
        if (!sheet_rows_.empty())
            items[count++] = {ui::Button::cross, tr("Choose")};
        items[count++] = {ui::Button::circle, sheet_catalog_.empty() ? tr("Close") : tr("Back")};
    }
    else if (layer_ == Layer::share)
    {
        return {}; // the panel carries its own
    }
    else if (!have_ && (load_ == model::Load::failed || load_ == model::Load::offline))
    {
        items[count++] = {ui::Button::cross, tr("Try again")};
        items[count++] = {ui::Button::circle, tr("Back")};
    }
    else if (zone_ == Zone::related)
    {
        items[count++] = {ui::Button::cross, tr("Play")};
        items[count++] = {ui::Button::triangle, tr("Details")};
        items[count++] = {ui::Button::circle, tr("Back")};
        items[count++] = {ui::Button::r2, tr("Comments")};
        items[count++] = {ui::Button::touchpad, tr("Actions")};
    }
    else
    {
        items[count++] = {ui::Button::cross, tr("Choose")};
        items[count++] = {ui::Button::circle, tr("Back")};
        items[count++] = {ui::Button::r2, tr("Comments")};
        items[count++] = {ui::Button::touchpad, tr("Actions")};
    }
    return {items, static_cast<std::size_t>(count)};
}

FocusInfo VideoPage::focus() const
{
    FocusInfo info;
    if (layer_ == Layer::menu)
    {
        const int total = static_cast<int>(sheet_rows_.size());
        const int index = std::clamp(list_.focus(), 0, std::max(total - 1, 0));
        info.id = "menu/" + std::to_string(index);
        info.text = total > 0 ? sheet_rows_[static_cast<std::size_t>(index)].title : std::string(tr("More actions"));
        info.rect = total > 0 ? list_.row_rect(index) : Rect{kSheetList.x, kSheetList.y, kSheetList.w, 68.0f};
        info.index = index;
        info.total = total;
        return info;
    }
    if (layer_ == Layer::share)
    {
        const int total = static_cast<int>(links_.size());
        info.id = total > 0 ? "share/" + links_[static_cast<std::size_t>(link_)].id : std::string("share/code");
        info.text = total > 0 ? links_[static_cast<std::size_t>(link_)].url : std::string();
        info.rect = total > 0 ? links_[static_cast<std::size_t>(link_)].chip
                              : Rect{kSharePanel.x + 56.0f, kSharePanel.y + 56.0f, 420.0f, 420.0f};
        info.index = link_;
        info.total = total;
        return info;
    }
    if (zone_ == Zone::channel)
    {
        info.id = "channel/subscribe";
        info.text = subscribed() ? tr("Subscribed") : tr("Subscribe");
        info.rect = subscribe_rect();
        info.total = 1;
        return info;
    }
    if (zone_ == Zone::related && !related_.empty())
    {
        info.id = "related/" + std::to_string(related_col_);
        info.text = related_[static_cast<std::size_t>(related_col_)].title;
        info.rect = related_rect(related_col_, related_scroll_.position.target);
        info.index = related_col_;
        info.total = static_cast<int>(related_.size());
        return info;
    }
    const int index = std::max(button_index(action_), 0);
    info.id = std::string("action/") + act_name(action_);
    info.text = index < static_cast<int>(buttons_.size()) ? buttons_[static_cast<std::size_t>(index)].label : std::string();
    if (info.text.empty() && action_ == Act::more)
        info.text = tr("More actions");
    info.rect = button_rect(index);
    info.index = index;
    info.total = static_cast<int>(buttons_.size());
    return info;
}

std::vector<std::pair<std::string, std::string>> VideoPage::focusable() const
{
    std::vector<std::pair<std::string, std::string>> out;
    if (layer_ == Layer::menu)
    {
        for (std::size_t i = 0; i < sheet_rows_.size(); ++i)
            out.emplace_back("menu/" + std::to_string(i), sheet_rows_[i].title);
        return out;
    }
    if (layer_ == Layer::share)
    {
        for (const ShareLink &link : links_)
            out.emplace_back("share/" + link.id, link.label);
        return out;
    }
    if (can_subscribe())
        out.emplace_back("channel/subscribe", subscribed() ? tr("Subscribed") : tr("Subscribe"));
    for (const ActionButton &button : buttons_)
        out.emplace_back(std::string("action/") + act_name(button.act),
                         button.label.empty() && button.act == Act::more ? std::string(tr("More actions")) : button.label);
    for (std::size_t i = 0; i < related_.size(); ++i)
        out.emplace_back("related/" + std::to_string(i), related_[i].title);
    return out;
}

void VideoPage::snapshot(Snapshot &out) const
{
    out.set("video", id_);
    out.set("load", have_ ? "ready" : load_name(load_));
    out.set("zone", zone_ == Zone::channel ? "channel" : zone_ == Zone::related ? "related" : "actions");
    out.set("layer", layer_ == Layer::menu ? (sheet_catalog_.empty() ? "menu" : "choices") : layer_ == Layer::share ? "share" : "none");
    out.set("title_lines", title_lines_);
    out.set("description_lines", description_lines_);
    out.set("resume", details_.resume_s);
    out.set("liked", liked() ? "1" : "0");
    out.set("saved", saved() ? "1" : "0");
    out.set("subscribed", subscribed() ? "1" : "0");
    out.set("likes", like_label());
    out.set("pending", request_name(request_.kind));
    out.set("related", static_cast<int>(related_.size()));
    out.set("related_load", load_name(related_load_));
    out.set("related_pages", related_pages_);
    out.set("menu_rows", static_cast<int>(sheet_rows_.size()));
    out.set("link", !links_.empty() ? links_[static_cast<std::size_t>(link_)].url : std::string());
    std::vector<std::string> labels;
    for (const ActionButton &button : buttons_)
        labels.push_back(std::string(act_name(button.act)) + (button.label.empty() ? "" : ": " + button.label));
    out.list("actions", std::move(labels));
    std::vector<std::string> rows;
    for (const MenuRow &row : sheet_rows_)
        rows.push_back(row.title);
    out.list("menu", std::move(rows));
}

} // namespace afterglow::video_page
