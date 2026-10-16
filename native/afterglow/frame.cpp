// ProsperoTube Afterglow - the frame: the stack, hand-overs, input routing, attract mode.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/frame.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace afterglow
{

namespace
{

float move_seconds(Move move)
{
    switch (move)
    {
    case Move::cut:
        return 0.0f;
    case Move::fade:
        return 0.34f;
    case Move::zoom:
        return 0.52f;
    case Move::shrink:
        return 0.44f;
    case Move::push:
    case Move::pop:
        return 0.4f;
    }
    return 0.3f;
}

// The way out of a screen that came in by a move.
Move reverse(Move move)
{
    switch (move)
    {
    case Move::zoom:
        return Move::shrink;
    case Move::push:
        return Move::pop;
    case Move::cut:
        return Move::cut;
    default:
        return Move::fade;
    }
}

double now_ms()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

// How long the whole-screen picture of a video handed to the player stands before the page
// returns under it: the player takes over long before, unless it could not start.
constexpr float kLaunchHold = 10.0f;

} // namespace

Frame::Frame(Platform &platform, Source &source, const ui::Fonts &fonts, const FrameOptions &options)
    : platform_(platform), fonts_(fonts), options_(options), look_(&look_for_scheme(options.scheme)), data_(source),
      images_(platform), services_{data_, images_, platform, fonts}
{
    set_platform(&platform_);
    // Baked fonts draw a small set of letters: YouTube's texts lose the rest on the way in. The text engine draws them all.
    if (gfx::Font::shaper == nullptr)
        set_reading_face(&fonts_.regular);
    rail_destinations();
    set_scheme(options.scheme);
    toasts_.style.anchor = ui::ToastAnchor::top_right;
    toasts_.style.width = 560.0f;
    toasts_.style.margin = 60.0f;
    toasts_.style.frosted = true;
    toasts_.style.frost = 0.62f;
    toasts_.style.duration = 6.0f;
    toasts_.set_bounds({0.0f, 76.0f, gfx::kVirtualWidth - 36.0f, gfx::kVirtualHeight - 76.0f});
    dialog_.style.frosted = true;
    dialog_.set_bounds({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight});
    open(Route{options.opening ? ScreenId::opening : ScreenId::home, {}, {}});
}

Frame::~Frame()
{
    stack_.clear();
    removed_.reset();
    images_.clear();
    if (platform() == &platform_)
    {
        set_platform(nullptr);
        set_reading_face(nullptr);
    }
}

void Frame::rail_destinations()
{
    destinations_ = {
        {Icon::home, tr_mark("Home"), ScreenId::home, 300.0f},
        {Icon::search, tr_mark("Search"), ScreenId::search, 384.0f},
        {Icon::subs, tr_mark("Subscriptions"), ScreenId::subscriptions, 468.0f},
        {Icon::library, tr_mark("Library"), ScreenId::library, 552.0f},
        {Icon::settings, tr_mark("Settings"), ScreenId::settings, 884.0f},
        {Icon::user, tr_mark("Switch profile"), ScreenId::accounts, 980.0f},
    };
}

void Frame::set_scheme(int scheme)
{
    options_.scheme = scheme;
    look_ = &look_for_scheme(scheme);
    wear(toasts_.style, *look_, options_.reduced_motion);
    wear(dialog_.style, *look_, options_.reduced_motion);
}

UpdateContext Frame::context(bool top)
{
    return UpdateContext{services_, *this, feedback_, 0.0f, clock_, options_.reduced_motion, top, look_};
}

ScreenId Frame::screen() const
{
    return stack_.empty() ? ScreenId::home : stack_.back()->screen->id();
}

bool Frame::animating() const
{
    return true; // the page ground drifts and pictures arrive: the interface is never still for long
}

bool Frame::take_exit_request()
{
    const bool asked = exit_request_;
    exit_request_ = false;
    return asked;
}

void Frame::put_sheets_away()
{
    sheet_.dismiss();
    sheet_changed_ = nullptr;
    text_sheet_.dismiss();
}

void Frame::open(const Route &route, Move move)
{
    UpdateContext c = context(true);
    put_sheets_away();
    if (!stack_.empty())
        stack_.back()->screen->leave(c);
    removed_.reset();
    outgoing_ = nullptr;
    if (!stack_.empty() && move != Move::cut)
    {
        removed_ = std::move(stack_.back());
        outgoing_ = removed_.get();
    }
    stack_.clear();
    auto entry = std::make_unique<Entry>();
    entry->route = route;
    entry->screen = make_screen(route.screen);
    entry->arrived = Move::cut;
    stack_.push_back(std::move(entry));
    const Phase arriving{0.0f, 0.0f, outgoing_ != nullptr ? move : Move::cut, {}, {}};
    c.arriving = arriving.move == Move::cut ? nullptr : &arriving;
    stack_.back()->screen->enter(route, c, false);
    rail_open_ = false;
    rail_amount_.target = 0.0f;
    start_move(arriving.move, {});
}

void Frame::resume()
{
    idle_ = 0.0f;
    if (launch_.active && !launch_.returning)
    {
        // The picture that became the whole screen returns to where it came from.
        launch_.returning = true;
        launch_.move.start(options_.reduced_motion ? 0.2f : 0.5f);
    }
    if (stack_.empty())
        return;
    UpdateContext c = context(true);
    stack_.back()->screen->enter(stack_.back()->route, c, true);
}

// ---- Nav ---------------------------------------------------------------------------

void Frame::push(const Route &route, Move move, const Rect &carried)
{
    push(route, move, carried, {});
}

void Frame::push(const Route &route, Move move, const Rect &carried, std::string_view picture)
{
    request_ = Request::push;
    request_route_ = route;
    request_move_ = move;
    request_carried_ = carried;
    request_picture_ = std::string(picture);
}

bool Frame::back_to(ScreenId screen)
{
    for (std::size_t i = stack_.size(); i-- > 1;)
    {
        if (stack_[i - 1]->screen->id() == screen)
        {
            request_ = Request::back_to;
            request_depth_ = i;
            return true;
        }
    }
    return false;
}

void Frame::back()
{
    request_ = Request::back;
}

void Frame::root(const Route &route)
{
    request_ = Request::root;
    request_route_ = route;
}

void Frame::open_rail()
{
    request_rail_ = true;
}

void Frame::toast(std::string_view text, ToastKind kind)
{
    toast(text, {}, kind);
}

void Frame::toast(std::string_view text, std::string_view detail, ToastKind kind)
{
    const ui::StatusKind status = kind == ToastKind::success   ? ui::StatusKind::success
                                  : kind == ToastKind::warning ? ui::StatusKind::warning
                                  : kind == ToastKind::error   ? ui::StatusKind::danger
                                                               : ui::StatusKind::info;
    toasts_.push(status, std::string(text), std::string(detail));
    toast_log_.emplace_back(text);
    toast_detail_log_.emplace_back(detail);
    if (toast_log_.size() > 4)
    {
        toast_log_.erase(toast_log_.begin());
        toast_detail_log_.erase(toast_detail_log_.begin());
    }
}

void Frame::confirm(std::string_view title, std::string_view body, std::string_view confirm_label,
                    std::function<void(bool)> answered)
{
    ui::DialogContent content;
    content.icon = ui::StatusKind::question;
    content.title = std::string(title);
    content.body = std::string(body);
    content.buttons = {{std::string(confirm_label), ui::ButtonKind::primary, false},
                       {tr("Cancel"), ui::ButtonKind::secondary, false}};
    content.default_button = 0;
    dialog_.style.width = 760.0f;
    dialog_.style.buttons = ui::DialogButtons::row;
    question_answered_ = nullptr;
    dialog_answered_ = std::move(answered);
    dialog_.open(std::move(content), feedback_);
}

void Frame::ask(Question question, std::function<void(int answer)> answered)
{
    ui::DialogContent content;
    content.icon = question.icon;
    content.title = std::move(question.title);
    content.body = std::move(question.body);
    const int count = std::min(static_cast<int>(question.answers.size()), 3);
    for (int i = 0; i < count; ++i)
    {
        // The answer that destroys is filled in the warning colour; without one the first answer leads.
        const bool warns = i == question.danger;
        const bool leads = question.danger < 0 && i == 0;
        content.buttons.push_back({std::move(question.answers[static_cast<std::size_t>(i)]),
                                   warns || leads ? ui::ButtonKind::primary : ui::ButtonKind::secondary, warns});
    }
    content.default_button = std::clamp(question.safe, 0, std::max(count - 1, 0));
    // Stacked answers are sentences: they get the room to be read whole.
    dialog_.style.width = question.stacked ? 900.0f : 760.0f;
    dialog_.style.buttons = question.stacked ? ui::DialogButtons::stacked : ui::DialogButtons::row;
    dialog_answered_ = nullptr;
    question_answered_ = std::move(answered);
    dialog_.open(std::move(content), feedback_);
}

void Frame::ask_text(TextQuestion question, std::function<void(bool taken, std::string text)> answered)
{
    UpdateContext c = context(true);
    text_sheet_.show(std::move(question), std::move(answered), c);
}

void Frame::actions(const std::string &row, std::string_view about, std::function<void()> changed)
{
    UpdateContext c = context(true);
    if (sheet_.show(row, about, c))
        sheet_changed_ = std::move(changed);
}

bool Frame::play(const model::Video &video, const Rect &carried, std::string_view picture)
{
    if (!data_.play(video))
        return false;
    // The player's screen opens at the end of this update, as for a video any screen hands to Data::play
    // itself; this one grows out of the picture's rectangle.
    launch_from_ = carried;
    launch_picture_ = std::string(picture);
    return true;
}

void Frame::begin_launch(const model::Video &video)
{
    launch_ = Launch{};
    launch_.active = true;
    launch_.title = video.title;
    launch_.channel = video.channel;
    launch_.grows = launch_from_.w > 0.0f && launch_from_.h > 0.0f && !launch_picture_.empty();
    launch_.from = launch_.grows ? launch_from_ : Rect{0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight};
    launch_.picture = launch_.grows ? launch_picture_ : video.thumb;
    launch_.move.start(options_.reduced_motion ? 0.2f : launch_.grows ? 0.62f : 0.3f);
    launch_from_ = Rect{};
    launch_picture_.clear();
    idle_ = 0.0f;
}

void Frame::end_launch()
{
    launch_.returning = true;
    launch_.started = false;
    launch_.move.start(options_.reduced_motion ? 0.2f : launch_.grows ? 0.5f : 0.3f);
}

void Frame::ambient(std::string_view picture_url)
{
    if (picture_url == ambient_now_)
        return;
    ambient_prev_ = ambient_now_;
    ambient_now_ = std::string(picture_url);
    ambient_fade_.start(options_.reduced_motion ? 0.2f : 0.7f);
}

void Frame::start_move(Move move, const Rect &carried, std::string picture)
{
    move_ = move;
    carried_ = carried;
    carried_picture_ = std::move(picture);
    outgoing_on_top_ = move == Move::pop || move == Move::shrink;
    float seconds = move_seconds(move);
    if (options_.reduced_motion)
        seconds = std::min(seconds, 0.2f);
    if (seconds <= 0.0f || outgoing_ == nullptr)
    {
        change_.running = false;
        outgoing_ = nullptr;
        removed_.reset();
    }
    else
    {
        change_.start(seconds);
    }
}

void Frame::apply_requests()
{
    const Request request = request_;
    request_ = Request::none;
    UpdateContext c = context(true);
    if (request == Request::push)
    {
        if (stack_.size() >= kStackLimit)
        {
            platform_.log("afterglow: the screen stack is full; the new screen was not opened");
        }
        else
        {
            put_sheets_away();
            stack_.back()->screen->leave(c);
            removed_.reset();
            outgoing_ = stack_.back().get();
            auto entry = std::make_unique<Entry>();
            entry->route = request_route_;
            entry->screen = make_screen(request_route_.screen);
            entry->arrived = request_move_;
            entry->carried = request_carried_;
            entry->picture = request_picture_;
            stack_.push_back(std::move(entry));
            const Phase arriving{0.0f, 0.0f, request_move_, request_carried_, request_picture_};
            c.arriving = request_move_ == Move::cut ? nullptr : &arriving;
            stack_.back()->screen->enter(request_route_, c, false);
            start_move(request_move_, request_carried_, request_picture_);
            feedback_.play(audio::Cue::open);
        }
    }
    else if (request == Request::back)
    {
        if (stack_.size() <= 1)
        {
            exit_request_ = true;
        }
        else
        {
            put_sheets_away();
            stack_.back()->screen->leave(c);
            const Move way = reverse(stack_.back()->arrived);
            const Rect carried = stack_.back()->carried;
            std::string picture = stack_.back()->picture;
            removed_ = std::move(stack_.back());
            stack_.pop_back();
            outgoing_ = removed_.get();
            stack_.back()->screen->enter(stack_.back()->route, c, true);
            start_move(way, carried, std::move(picture));
            feedback_.play(audio::Cue::back);
        }
    }
    else if (request == Request::back_to)
    {
        if (request_depth_ >= 1 && request_depth_ < stack_.size())
        {
            // Everything above the screen that was asked for goes; the top one is seen leaving.
            put_sheets_away();
            stack_.back()->screen->leave(c);
            removed_ = std::move(stack_.back());
            stack_.resize(request_depth_);
            outgoing_ = removed_.get();
            stack_.back()->screen->enter(stack_.back()->route, c, true);
            start_move(Move::pop, {});
            feedback_.play(audio::Cue::back);
        }
    }
    else if (request == Request::root)
    {
        if (stack_.size() == 1 && stack_.back()->route.screen == request_route_.screen &&
            stack_.back()->route.argument == request_route_.argument)
            feedback_.play(audio::Cue::select, 1.0f, -0.5f);
        else
        {
            // The opening hands over to Home in silence: the welcome is its sound.
            const bool from_opening = !stack_.empty() && stack_.back()->screen->id() == ScreenId::opening;
            open(request_route_, Move::fade);
            if (!from_opening)
                feedback_.play(audio::Cue::tab);
        }
    }
    if (request_rail_)
    {
        request_rail_ = false;
        if (!stack_.empty() && stack_.back()->screen->shows_rail() && !rail_open_)
        {
            rail_open_ = true;
            rail_focus_ = active_destination();
            rail_pos_.snap(static_cast<float>(rail_focus_));
            feedback_.play(audio::Cue::open, 1.0f, -0.5f);
        }
    }
}

int Frame::active_destination() const
{
    switch (stack_.empty() ? ScreenId::home : stack_.front()->screen->id())
    {
    case ScreenId::search:
    case ScreenId::results:
        return 1;
    case ScreenId::subscriptions:
    case ScreenId::channels:
        return 2;
    case ScreenId::library:
    case ScreenId::notifications:
        return 3;
    case ScreenId::settings:
        return 4;
    case ScreenId::accounts:
    case ScreenId::sign_in:
    case ScreenId::link_phone:
        return 5;
    default:
        return 0;
    }
}

void Frame::update_rail(const InputFrame &input)
{
    const int count = static_cast<int>(destinations_.size());
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int next = rail_focus_ + (input.nav == Direction::down ? 1 : -1);
        if (next >= 0 && next < count)
        {
            rail_focus_ = next;
            feedback_.play(audio::Cue::focus, 1.08f - 0.03f * static_cast<float>(next), -0.5f);
        }
        else if (!input.nav_repeat)
        {
            feedback_.play(audio::Cue::error, 1.0f, -0.5f, 0.6f);
            feedback_.rumble(0.25f, 0.05f);
        }
    }
    if (pressed(input, Action::confirm))
    {
        rail_open_ = false;
        root(Route{destinations_[static_cast<std::size_t>(rail_focus_)].screen, {}, {}});
    }
    else if (input.nav == Direction::right || pressed(input, Action::back))
    {
        rail_open_ = false;
        feedback_.play(audio::Cue::back, 1.0f, -0.5f);
    }
}

void Frame::update(const InputFrame &input, float dt, std::string_view typed)
{
    const double started = now_ms();
    feedback_.clear();
    clock_ += dt;
    stats_.frame_ms = dt * 1000.0f;
    ++stats_.frames;
    if (stats_.frame_ms > 25.0f)
        ++stats_.slow_frames;
    frame_times_[frame_time_at_] = stats_.frame_ms;
    frame_time_at_ = (frame_time_at_ + 1) % frame_times_.size();
    stats_.worst_frame_ms = *std::max_element(frame_times_.begin(), frame_times_.end());

    data_.pump();
    images_.pump(dt);
    change_.update(dt);
    if (!change_.running)
    {
        outgoing_ = nullptr;
        removed_.reset();
    }
    ambient_fade_.update(dt);
    ambient_level_.update(dt, 6.0f);
    rail_amount_.target = rail_open_ ? 1.0f : 0.0f;
    rail_amount_.update(dt, options_.reduced_motion ? 60.0f : 16.0f);
    rail_pos_.target = static_cast<float>(rail_focus_);
    rail_pos_.update(dt, options_.reduced_motion ? 60.0f : 22.0f);
    toasts_.update(dt, feedback_);
    dialog_.update(dt);
    launch_.move.update(dt);
    if (launch_.active && launch_.returning)
    {
        if (!launch_.move.running)
            launch_ = Launch{};
    }
    else if (launch_.active)
    {
        // The player's screen stands for as long as the app says the video is starting. A start that
        // failed brings the page back with the app's reason; an app that says nothing took over at once.
        std::string reason;
        const Source::Start start = data_.play_state(&reason);
        launch_.started = start == Source::Start::starting;
        if (start == Source::Start::failed)
        {
            end_launch();
            toast(tr("This video cannot be played right now."), reason, ToastKind::error);
        }
        else if (!launch_.started && !launch_.move.running && (launch_.held += dt) > kLaunchHold)
        {
            end_launch(); // the player never took over: the page comes back
        }
    }
    if (stack_.empty())
        return;

    // Attract mode: a press that ends it does nothing else.
    const bool any = input.pressed != 0 || input.nav != Direction::none || !typed.empty();
    InputFrame routed = input;
    if (any)
    {
        idle_ = 0.0f;
        if (attract_)
        {
            attract_ = false;
            stack_.back()->screen->attract(false);
            routed = InputFrame{};
            typed = {};
        }
    }
    else
    {
        idle_ += dt;
        if (background_title_.empty() && !attract_ && idle_ >= options_.attract_after && !dialog_.is_open() && !rail_open_ && !change_.running && !sheet_.open() &&
            !text_sheet_.open() && !launch_.active)
            attract_ = stack_.back()->screen->attract(true);
    }

    // While a video starts the pad is the player's screen's: Circle calls the start back where the app
    // still can, and nothing reaches the page underneath.
    if (launch_.active && launch_.started && !launch_.returning)
    {
        if (pressed(routed, Action::back) && data_.cancel_play())
        {
            end_launch();
            feedback_.play(audio::Cue::back);
        }
        routed = InputFrame{};
        typed = {};
    }

    const InputFrame none{};
    UpdateContext top = context(true);
    top.dt = dt;
    top.typed = typed;
    UpdateContext under = context(false);
    under.dt = dt;
    if (outgoing_ != nullptr)
        outgoing_->screen->update(none, under);
    if (dialog_.is_open())
    {
        const ui::Event event = dialog_.handle(routed, feedback_);
        if (event == ui::Event::activated || event == ui::Event::cancelled)
        {
            const int choice = event == ui::Event::activated ? dialog_.choice() : -1;
            if (question_answered_)
            {
                auto answered = std::move(question_answered_);
                question_answered_ = nullptr;
                answered(choice);
            }
            else if (dialog_answered_)
            {
                auto answered = std::move(dialog_answered_);
                dialog_answered_ = nullptr;
                answered(choice == 0);
            }
        }
        stack_.back()->screen->update(none, under);
    }
    else if (text_sheet_.open())
    {
        text_sheet_.handle(routed, top);
        stack_.back()->screen->update(none, under);
    }
    else if (sheet_.open())
    {
        Route route;
        if (sheet_.handle(routed, top, &route))
            push(route, Move::push, {});
        stack_.back()->screen->update(none, under);
    }
    else if (rail_open_)
    {
        update_rail(routed);
        stack_.back()->screen->update(none, under);
    }
    else
    {
        // A screen that is still arriving takes input at once: nothing waits for a transition.
        stack_.back()->screen->update(routed, top);
    }
    sheet_.update(top);
    text_sheet_.update(top);
    if (sheet_.take_changed() && sheet_changed_)
        sheet_changed_(); // the screen that asked forgets the lists the action changed
    // A video went to the player in this update, from whichever screen: its screen opens now.
    if (model::Video handed; data_.take_handed(&handed))
        begin_launch(handed);
    apply_requests();
    stats_.data_pending = data_.pending();
    stats_.pictures_pending = images_.pending();
    stats_.pictures_ready = images_.ready();
    stats_.pictures_failed = images_.failed();
    stats_.picture_bytes = images_.bytes();
    stats_.update_ms = static_cast<float>(now_ms() - started);
}

FocusInfo Frame::focus() const
{
    FocusInfo info;
    if (dialog_.is_open())
    {
        info.id = "dialog/" + std::to_string(dialog_.focus());
        info.text = dialog_.content().title;
        info.index = dialog_.focus();
        info.total = static_cast<int>(dialog_.content().buttons.size());
        info.rect = {580.0f, 340.0f, 760.0f, 400.0f}; // the panel stands in the middle of the page
        return info;
    }
    if (text_sheet_.open())
        return text_sheet_.focus();
    if (sheet_.open())
        return sheet_.focus();
    if (rail_open_)
    {
        const Destination &item = destinations_[static_cast<std::size_t>(rail_focus_)];
        info.id = std::string("rail/") + screen_name(item.screen);
        info.text = item.label;
        info.kind = "action";
        info.rect = {24.0f, item.y - 36.0f, kRail + 272.0f - 48.0f, 72.0f};
        info.index = rail_focus_;
        info.total = static_cast<int>(destinations_.size());
        return info;
    }
    return stack_.empty() ? info : stack_.back()->screen->focus();
}

} // namespace afterglow
