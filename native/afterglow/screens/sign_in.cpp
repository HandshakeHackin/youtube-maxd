// ProsperoTube Afterglow - signing in to YouTube with a code approved on another device.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The page acts on the provider's sign-in rows and shows what they answer:
// "auth:start" asks for a code (once per attempt), "auth:session" is the
// attempt's state while it is waited for, "auth:cancel" ends it. The first
// row that has them carries the facts state (starting, pending, authorized,
// canceled, expired, denied, failed), user_code, verification_url, expires_in
// (seconds) and, once authorized, name; the authorized profile is the row
// "library:profile:<id>". Without the facts the state is read from the rows
// themselves ("auth:info:code", "auth:info:url", a profile's row).
//
// States: starting (the code is asked for), code (shown, waiting, counting
// down), approved, denied, expired, canceled, failed, offline, and unavailable
// for a build whose service does not sign in ("library" has no "auth:start").

#include "afterglow/screens/accounts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace afterglow
{

namespace
{

using namespace acct;

constexpr float kPoll = 2.0f;        // seconds between two looks at the attempt
constexpr float kColumn = 900.0f;    // the steps' column
constexpr float kStepText = kLeft + 72.0f;
constexpr Rect kStatus{kLeft, 720.0f, kColumn, 84.0f};
constexpr Rect kScan{1180.0f, 212.0f, 644.0f, 660.0f};
constexpr float kButtonTop = 828.0f;

enum class State : std::uint8_t
{
    starting,
    code,
    approved,
    denied,
    expired,
    canceled,
    failed,
    offline,
    unavailable,
};

constexpr const char *kStateNames[] = {"starting", "code", "approved", "denied", "expired", "canceled", "failed", "offline", "unavailable"};

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

// "https://www.google.com/device" as a person types it: "google.com/device".
std::string spoken(std::string_view url)
{
    for (std::string_view lead : {"https://", "http://", "www."})
    {
        if (starts_with(url, lead))
            url.remove_prefix(lead.size());
    }
    return std::string(url);
}

class SignIn final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::sign_in;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        fonts_ = &context.services.fonts;
        if (restored)
            return;
        route_ = route;
        age_ = 0.0f;
        start_ticket_ = poll_ticket_ = use_ticket_ = 0;
        asked_ = false;
        code_.clear();
        url_.clear();
        error_.clear();
        profile_row_.clear();
        profile_name_.clear();
        choice_ = 0;
        missed_ = 0;
        state_ = State::starting;
        state_age_ = 0.0f;
    }

    // Leaving while a code is out ends the attempt, however the page is left.
    void leave(UpdateContext &context) override
    {
        if (state_ != State::starting && state_ != State::code)
            return;
        if (asked_)
        {
            (void)context.services.data.act("auth:cancel");
            context.nav.toast(tr("Sign-in canceled"), ToastKind::info);
        }
        start_ticket_ = poll_ticket_ = 0;
        set(State::canceled);
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        state_age_ += context.dt;
        code_age_ += context.dt;
        fonts_ = &context.services.fonts;
        Data &data = context.services.data;
        const float speed = context.reduced_motion ? 60.0f : 20.0f;
        for (int i = 0; i < 2; ++i)
        {
            choice_focus_[i].target = choice_ == i ? 1.0f : 0.0f;
            choice_focus_[i].update(context.dt, speed);
        }
        nudge_.update(context.dt, 10.0f);

        if (state_ == State::starting && !asked_)
        {
            // Is signing in part of this build at all? The accounts catalog says.
            const Page &library = data.catalog("library");
            if (library.load == model::Load::ready && find_row(library, "auth:start") == nullptr)
                set(State::unavailable);
            else if (library.load != model::Load::loading && library.load != model::Load::idle)
                start(data);
        }
        if (start_ticket_ != 0)
        {
            const Page &answer = data.action(start_ticket_);
            if (!pending(answer))
            {
                const Page copy = answer;
                start_ticket_ = 0;
                apply(copy, true, context);
            }
        }
        if (poll_ticket_ != 0)
        {
            const Page &answer = data.action(poll_ticket_);
            if (!pending(answer))
            {
                const Page copy = answer;
                poll_ticket_ = 0;
                apply(copy, false, context);
            }
        }
        if (state_ == State::starting && asked_ && start_ticket_ == 0 && poll_ticket_ == 0)
        {
            // The provider is still asking YouTube for the code: look again shortly.
            poll_in_ -= context.dt;
            if (poll_in_ <= 0.0f)
            {
                poll_ticket_ = data.act("auth:session");
                poll_in_ = 1.0f;
            }
        }
        if (state_ == State::code)
        {
            remaining_ -= context.dt;
            poll_in_ -= context.dt;
            if (poll_ticket_ == 0 && (poll_in_ <= 0.0f || remaining_ <= 0.0f))
            {
                poll_ticket_ = data.act("auth:session");
                poll_in_ = kPoll;
            }
            if (remaining_ <= -1.5f)
                set(State::expired); // the provider says so too; this is for an answer that does not come
        }
        if (use_ticket_ != 0)
        {
            const Page &answer = data.action(use_ticket_);
            if (!pending(answer))
            {
                const bool done = answer.load == model::Load::ready;
                const std::string reason = answer.error;
                use_ticket_ = 0;
                if (done)
                {
                    context.nav.toast(with(tr("Watching as {0}"), profile_name_), ToastKind::success);
                    context.nav.root(Route{ScreenId::home, {}, {}});
                }
                else
                {
                    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.8f);
                    context.nav.toast(reason.empty() ? std::string(tr("That did not work. Try again.")) : reason, ToastKind::error);
                }
            }
        }
        if (!context.top)
            return;

        if (pressed(input, Action::back))
        {
            context.nav.back(); // leave() ends an attempt that is still open
            return;
        }
        if (state_ == State::approved)
        {
            if (use_ticket_ != 0)
                return;
            if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int next = choice_ + (input.nav == Direction::right ? 1 : -1);
                if (next < 0)
                    context.nav.open_rail();
                else if (next > 1)
                    refuse(input, context);
                else
                {
                    choice_ = next;
                    context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(choice_rect(next).cx()));
                }
            }
            else if (input.nav != Direction::none)
                refuse(input, context);
            else if (pressed(input, Action::confirm))
            {
                context.feedback.play(audio::Cue::select);
                data.forget("library:profiles");
                data.forget("library");
                if (choice_ == 0)
                    use_ticket_ = data.act(profile_row_);
                else
                    context.nav.root(Route{ScreenId::accounts, {}, {}});
            }
            return;
        }
        if (input.nav == Direction::left)
        {
            context.nav.open_rail();
            return;
        }
        if (input.nav != Direction::none)
        {
            refuse(input, context);
            return;
        }
        const bool ended = state_ == State::denied || state_ == State::expired || state_ == State::canceled ||
                           state_ == State::failed || state_ == State::offline;
        if (pressed(input, Action::west) || (ended && pressed(input, Action::confirm)))
        {
            if (state_ == State::unavailable || state_ == State::starting)
            {
                refuse(input, context);
                return;
            }
            // A new code: the attempt that is open ends first.
            context.feedback.play(audio::Cue::select);
            if (state_ == State::code)
                (void)data.act("auth:cancel");
            poll_ticket_ = 0;
            set(State::starting);
            start(data);
        }
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        Travel travel(g, phase);
        const std::string title = state_ == State::approved ? with(tr("Signed in as {0}"), profile_name_)
                                                            : std::string(tr("Finish on your phone or computer"));
        page_head(g, tr("Sign in to YouTube"), title, arrive(age_, 0, 0.05f, 0.5f));
        if (state_ == State::unavailable)
        {
            message_page(g, Icon::info, tr("Not available in this build"),
                         tr("Sign-in to YouTube arrives in a later version. This version browses and plays as Guest, without an account."),
                         {}, 0.0f, arrive(age_, 1, 0.06f, 0.5f), g.k.theme.warning);
            return;
        }
        draw_steps(g);
        draw_status(g);
        draw_scan(g);
    }

    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[2];
        int count = 0;
        if (state_ == State::code)
            items[count++] = {ui::Button::square, tr("Get a new code")};
        else if (state_ != State::starting && state_ != State::unavailable)
            items[count++] = {ui::Button::cross, tr("Choose")};
        items[count++] = {ui::Button::circle, state_ == State::starting || state_ == State::code ? tr("Cancel sign-in") : tr("Back")};
        return std::span<const ui::Hint>(items, static_cast<std::size_t>(count));
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        info.total = 1;
        if (state_ == State::unavailable)
        {
            info.id = "unavailable";
            info.text = tr("Not available in this build");
        }
        else if (state_ == State::approved)
        {
            info.id = choice_ == 0 ? "action/use" : "action/profiles";
            info.text = choice_label(choice_);
            info.rect = choice_rect(choice_);
            info.index = choice_;
            info.total = 2;
        }
        else if (state_ == State::starting || state_ == State::code)
        {
            info.id = "code";
            info.text = code_;
            info.rect = code_rect();
        }
        else
        {
            info.id = "action/restart";
            info.text = tr("Start a new sign-in");
            info.rect = restart_rect();
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        if (state_ == State::approved)
            return {{"action/use", choice_label(0)}, {"action/profiles", choice_label(1)}};
        const FocusInfo only = focus();
        return {{only.id, only.text}};
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("state", kStateNames[static_cast<std::size_t>(state_)]);
        out.set("code", code_);
        out.set("address", spoken(url_));
        out.set("seconds_left", state_ == State::code ? std::max(0, static_cast<int>(std::ceil(remaining_))) : 0);
        out.set("profile", profile_name_);
        out.set("error", error_);
    }

  private:
    void set(State state)
    {
        if (state_ != state)
            state_age_ = 0.0f;
        state_ = state;
    }

    void start(Data &data)
    {
        asked_ = true;
        error_.clear();
        start_ticket_ = data.act("auth:start");
    }

    void refuse(const InputFrame &input, UpdateContext &context)
    {
        if (input.nav_repeat)
            return;
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
    }

    // What the provider answered: the attempt's state and what belongs to it.
    void apply(const Page &page, bool first, UpdateContext &context)
    {
        if (page.load != model::Load::ready)
        {
            // A look that failed while a code is out is tried again; the code may still be approved.
            if (!first && state_ == State::code && ++missed_ < 3)
                return;
            error_ = page.error;
            set(page.load == model::Load::offline ? State::offline : State::failed);
            return;
        }
        missed_ = 0;
        std::string state = fact_of(page, "state");
        const model::Row *code_row = find_row(page, "auth:info:code");
        const model::Row *profile = find_prefix(page, "library:profile:");
        if (state.empty())
            state = code_row != nullptr ? "pending" : profile != nullptr ? "authorized" : "failed";
        if (state == "pending")
        {
            std::string code = fact_of(page, "user_code");
            if (code.empty() && code_row != nullptr)
                code = code_row->title;
            std::string url = fact_of(page, "verification_url");
            if (url.empty())
            {
                const model::Row *address = find_row(page, "auth:info:url");
                url = address != nullptr ? address->title : std::string();
            }
            if (code.empty() || url.empty())
            {
                set(State::failed);
                return;
            }
            const float expires = static_cast<float>(std::atof(fact_of(page, "expires_in").c_str()));
            if (code != code_ || state_ != State::code)
            {
                code_ = std::move(code);
                code_age_ = 0.0f; // the boxes arrive one by one
                remaining_ = expires > 0.0f ? expires : 900.0f;
                poll_in_ = kPoll;
                if (!first || state_age_ > 0.6f)
                    context.feedback.play(audio::Cue::notify, 1.0f, 0.0f, 0.6f);
            }
            else if (expires > 0.0f && std::fabs(expires - remaining_) > 3.0f)
            {
                remaining_ = expires; // the provider's clock is the one that counts
            }
            url_ = std::move(url);
            set(State::code);
        }
        else if (state == "authorized" && profile != nullptr)
        {
            profile_row_ = profile->id;
            profile_name_ = profile->fact("name");
            if (profile_name_.empty())
                profile_name_ = profile->subtitle;
            if (state_ != State::approved)
            {
                // The one celebration the app has (sound design, cue 34), once per sign-in.
                choice_ = 0;
                context.feedback.play(audio::Cue::saved);
                context.feedback.rumble(0.3f, 0.06f);
            }
            set(State::approved);
        }
        else if (state == "starting")
        {
            poll_in_ = 1.0f;
            set(State::starting);
        }
        else
        {
            error_.clear();
            set(state == "denied" ? State::denied : state == "expired" ? State::expired : state == "canceled" || state == "cancelled" ? State::canceled : State::failed);
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.7f);
        }
    }

    bool live() const
    {
        return state_ == State::starting || state_ == State::code;
    }

    // ---- layout ---------------------------------------------------------------------

    struct CodeLayout
    {
        float box = 84.0f;
        float height = 112.0f;
        float pitch = 96.0f;
        float dash = 38.0f;
        float width = 0.0f;
        std::vector<std::string> groups;
    };

    // The code as groups of characters, in boxes that fit the column however long it is.
    CodeLayout code_layout() const
    {
        CodeLayout layout;
        std::string group;
        for (const char c : code_)
        {
            if (c == '-' || c == ' ')
            {
                if (!group.empty())
                    layout.groups.push_back(std::move(group));
                group.clear();
            }
            else
                group.push_back(c);
        }
        if (!group.empty())
            layout.groups.push_back(std::move(group));
        if (layout.groups.empty())
            layout.groups = {"    ", "    "}; // the bones of a code that has not arrived
        else if (layout.groups.size() == 1 && layout.groups[0].size() == 8)
            layout.groups = {layout.groups[0].substr(0, 4), layout.groups[0].substr(4)};
        std::size_t count = 0;
        for (const std::string &part : layout.groups)
            count += part.size();
        const float natural = static_cast<float>(count) * 96.0f - 12.0f + static_cast<float>(layout.groups.size() - 1) * 38.0f;
        const float scale = std::min(1.0f, (kColumn - 72.0f) / std::max(natural, 1.0f));
        layout.box *= scale;
        layout.height *= std::max(scale, 0.7f);
        layout.pitch *= scale;
        layout.dash *= scale;
        layout.width = natural * scale;
        return layout;
    }

    Rect code_rect() const
    {
        const CodeLayout layout = code_layout();
        return {kStepText, 404.0f, layout.width, layout.height};
    }

    std::string choice_label(int index) const
    {
        return index == 0 ? with(tr("Watch as {0}"), profile_name_) : std::string(tr("All profiles"));
    }

    Rect choice_rect(int index) const
    {
        const float first = fonts_ != nullptr ? std::min(measure(fonts_->semibold, choice_label(0), kTypeBody) + 96.0f, 560.0f) : 320.0f;
        if (index == 0)
            return {kLeft, kButtonTop, first, 64.0f};
        const float second = fonts_ != nullptr ? measure(fonts_->semibold, choice_label(1), kTypeBody) + 96.0f : 240.0f;
        return {kLeft + first + 16.0f, kButtonTop, second, 64.0f};
    }

    Rect restart_rect() const
    {
        const float width = fonts_ != nullptr ? measure(fonts_->semibold, tr("Start a new sign-in"), kTypeBody) + 96.0f : 360.0f;
        return {kLeft, kButtonTop, std::min(width, 620.0f), 64.0f};
    }

    // ---- drawing --------------------------------------------------------------------

    void draw_steps(G &g) const
    {
        // Instructions for a code that is no longer good step back.
        const float presence = live() ? 1.0f : state_ == State::approved ? 0.6f : 0.38f;
        g.l.push_opacity(presence);
        const bool done = state_ == State::approved;
        const auto disc = [&](int number, float y)
        {
            if (!done)
            {
                step_disc(g, kLeft + 24.0f, y - 9.0f, number);
                return;
            }
            // The step is done: its number gives way to a check, one disc after the other.
            const float in = tween::back_out(tween::clamp01((state_age_ - 0.07f * static_cast<float>(number - 1)) / 0.4f));
            const float radius = 24.0f * (g.reduced ? 1.0f : 0.7f + 0.3f * in);
            g.l.circle(kLeft + 24.0f, y - 9.0f, radius, g.k.theme.success.with_alpha(0.22f));
            g.l.ring(kLeft + 24.0f, y - 9.0f, radius, 1.5f, g.k.theme.success);
            icon(g, Icon::check, kLeft + 24.0f, y - 9.0f, 26.0f, g.k.ink);
        };
        {
            Enter enter(g, arrive(age_, 1, 0.07f, 0.5f), 22.0f);
            disc(1, 268.0f);
            const float w = text(g, g.f.regular, tr("Open"), kStepText, 268.0f, 27.0f, g.k.ink);
            const std::string address = spoken(url_);
            const float room = kColumn - 72.0f - w - 18.0f;
            if (address.empty())
            {
                bone(g, {kStepText + w + 18.0f, 232.0f, std::min(320.0f, room), 56.0f}, 16.0f, 0);
            }
            else
            {
                const std::string fitted = fit(g.f.mono, address, 27.0f, room - 44.0f);
                const Rect plate{kStepText + w + 18.0f, 232.0f, measure(g.f.mono, fitted, 27.0f) + 44.0f, 56.0f};
                g.l.rounded_rect(plate, 16.0f, g.k.raised);
                text(g, g.f.mono, fitted, plate.x + 22.0f, 268.0f, 27.0f, g.k.ink);
            }
        }
        {
            Enter enter(g, arrive(age_, 2, 0.07f, 0.5f), 22.0f);
            disc(2, 364.0f);
            text(g, g.f.regular, fit(g.f.regular, tr("Enter this code"), 27.0f, kColumn - 72.0f), kStepText, 364.0f, 27.0f, g.k.ink);
            // The code, large enough to read from the sofa and copy without mistakes.
            const CodeLayout layout = code_layout();
            float x = kStepText;
            int index = 0;
            for (std::size_t part = 0; part < layout.groups.size(); ++part)
            {
                for (const char c : layout.groups[part])
                {
                    const Rect box{x, 404.0f, layout.box, layout.height};
                    if (code_.empty())
                    {
                        code_bone(g, box, 18.0f, index);
                    }
                    else
                    {
                        const char letter[2] = {c, '\0'};
                        code_box(g, box, letter, 60.0f * layout.box / 84.0f, (code_age_ - 0.05f * static_cast<float>(index)) / 0.4f, 18.0f);
                    }
                    x += layout.pitch;
                    ++index;
                }
                if (part + 1 < layout.groups.size())
                {
                    g.l.rounded_rect({x + 2.0f * layout.dash / 38.0f, 404.0f + layout.height * 0.5f - 2.5f, 22.0f * layout.dash / 38.0f, 5.0f}, 2.5f, g.k.ink3);
                    x += layout.dash;
                }
            }
        }
        {
            Enter enter(g, arrive(age_, 3, 0.07f, 0.5f), 22.0f);
            disc(3, 600.0f);
            text(g, g.f.regular, fit(g.f.regular, tr("Choose your account and allow access."), 27.0f, kColumn - 72.0f), kStepText, 600.0f, 27.0f, g.k.ink);
            text(g, g.f.regular, fit(g.f.regular, tr("This screen carries on by itself."), 23.0f, kColumn - 72.0f), kStepText, 638.0f, 23.0f, g.k.ink2);
        }
        g.l.pop_opacity();
    }

    void draw_status(G &g) const
    {
        Enter enter(g, arrive(age_, 4, 0.07f, 0.5f), 22.0f);
        const float turn = tween::cubic_out(tween::clamp01(state_age_ / 0.3f)); // a new state fades in where the old one was
        glass_panel(g, kStatus, 26.0f);
        const float cx = kStatus.x + 44.0f;
        const float cy = kStatus.cy();
        std::string line;
        std::string side;
        Color side_ink = g.k.ink2;
        g.l.push_opacity(turn);
        switch (state_)
        {
        case State::starting:
            spinner(g, cx, cy, 16.0f, g.k.accent);
            line = tr("Requesting YouTube sign-in code");
            break;
        case State::code:
            spinner(g, cx, cy, 16.0f, g.k.accent);
            line = tr("Waiting for your approval");
            side = with(tr("The code is good for {0}"), clock_text(remaining_));
            if (remaining_ < 60.0f)
                side_ink = g.k.theme.warning; // the last minute, said in words and in colour
            break;
        case State::approved:
        {
            const float pop = g.reduced ? 1.0f : tween::back_out(tween::clamp01(state_age_ / 0.4f));
            g.l.circle(cx, cy, 20.0f * pop, g.k.theme.success);
            icon(g, Icon::check, cx, cy, 26.0f * pop, g.k.on_accent);
            line = with(tr("Signed in as {0}"), profile_name_);
            break;
        }
        case State::denied:
            icon(g, Icon::block, cx, cy, 34.0f, g.k.theme.danger);
            line = tr("YouTube authorization denied");
            break;
        case State::expired:
            icon(g, Icon::clock, cx, cy, 34.0f, g.k.theme.warning);
            line = tr("Sign-in code expired");
            break;
        case State::canceled:
            icon(g, Icon::close, cx, cy, 34.0f, g.k.ink2);
            line = tr("Sign-in canceled");
            break;
        case State::offline:
            icon(g, Icon::offline, cx, cy, 34.0f, g.k.theme.warning);
            line = tr("No connection to the YouTube service");
            break;
        default:
            icon(g, Icon::alert, cx, cy, 34.0f, g.k.theme.danger);
            line = tr("YouTube sign-in could not be completed");
            break;
        }
        float side_width = 0.0f;
        if (!side.empty())
        {
            const std::string fitted = fit(g.f.regular, side, 22.0f, 430.0f);
            side_width = text(g, g.f.regular, fitted, kStatus.x + kStatus.w - 28.0f, cy + 7.5f, 22.0f, side_ink, gfx::Align::right) + 24.0f;
        }
        text(g, g.f.semibold, fit(g.f.semibold, line, kTypeBody, kStatus.w - 84.0f - 28.0f - side_width), kStatus.x + 84.0f, cy + 8.5f,
             kTypeBody, g.k.ink);
        g.l.pop_opacity();

        if (live())
        {
            paragraph(g, g.f.regular,
                      tr("Your password is never typed on the console. The sign-in is kept in the app's folder on this PS5 and can be removed at any time."),
                      kLeft, 856.0f, 21.0f, kColumn, 30.0f, g.k.ink3, 2);
            return;
        }
        // What comes next, as buttons under the state.
        Enter rise(g, tween::clamp01(state_age_ / 0.35f - 0.3f), 16.0f);
        const float shift = g.reduced ? 0.0f : ui::shake(nudge_.value, g.time);
        if (state_ == State::approved)
        {
            for (int i = 0; i < 2; ++i)
            {
                Rect r = choice_rect(i);
                r.x += choice_ == i ? shift : 0.0f;
                const std::string label = fit(g.f.semibold, choice_label(i), kTypeBody, r.w - 96.0f + 1.0f);
                // Neither rests filled: with two answers side by side only the focus may wear the plate.
                button(g, r, label, i == 0 ? Icon::play : Icon::users, choice_focus_[i].value);
            }
            return;
        }
        Rect r = restart_rect();
        r.x += shift;
        button(g, r, fit(g.f.semibold, tr("Start a new sign-in"), kTypeBody, r.w - 95.0f), Icon::update, 1.0f, true);
        const std::string why = !error_.empty()                 ? error_
                                : state_ == State::expired     ? std::string(tr("A code is good for a limited time. A new one starts over."))
                                : state_ == State::denied      ? std::string(tr("Access was not allowed on the other device."))
                                : state_ == State::offline     ? std::string(tr("The code is asked for again once the service answers."))
                                : state_ == State::failed      ? std::string(tr("Check the connection of this PS5 and try again."))
                                                               : std::string();
        if (!why.empty())
        {
            const float room = kColumn - r.w - 24.0f;
            const bool one_line = measure_block(g.f.regular, why, 21.0f, room, 30.0f, 2).lines <= 1;
            paragraph(g, g.f.regular, why, r.x + r.w + 24.0f - shift, kButtonTop + (one_line ? 39.0f : 25.0f), 21.0f, room, 30.0f, g.k.ink2, 2);
        }
    }

    void draw_scan(G &g) const
    {
        Enter enter(g, arrive(age_, 2, 0.08f, 0.6f), 0.0f, 48.0f);
        lift(g, kScan, kRadiusPanel, 0.9f);
        glass_panel(g, kScan, kRadiusPanel);
        const Rect square{kScan.cx() - 210.0f, kScan.y + 140.0f, 420.0f, 420.0f};
        const float room = kScan.w - 88.0f;
        if (state_ == State::approved)
        {
            // The picture of the moment: who is signed in.
            overline(g, tr("Signed in to YouTube"), kScan.x + 44.0f, kScan.y + 62.0f, g.k.accent);
            const float pop = g.reduced ? 1.0f : tween::back_out(tween::clamp01((state_age_ - 0.1f) / 0.5f));
            g.l.glow({kScan.cx() - 80.0f, square.cy() - 80.0f, 160.0f, 160.0f}, 80.0f, 90.0f, g.k.theme.success.with_alpha(0.3f * tween::clamp01(pop)));
            avatar(g, kScan.cx(), square.cy() - 10.0f, 96.0f * pop, profile_name_, profile_row_.substr(std::min<std::size_t>(profile_row_.size(), 16)), Picture{});
            g.l.circle(kScan.cx() + 68.0f, square.cy() + 58.0f, 30.0f * pop, g.k.panel_solid.with_alpha(1.0f));
            g.l.circle(kScan.cx() + 68.0f, square.cy() + 58.0f, 25.0f * pop, g.k.theme.success);
            icon(g, Icon::check, kScan.cx() + 68.0f, square.cy() + 58.0f, 30.0f * pop, g.k.on_accent);
            text(g, g.f.display, fit(g.f.display, profile_name_, kTypeTitle, room), kScan.cx(), square.cy() + 150.0f, kTypeTitle, g.k.ink, gfx::Align::center);
            return;
        }
        overline(g, tr("Or scan"), kScan.x + 44.0f, kScan.y + 62.0f, live() ? g.k.accent : g.k.ink3);
        if (state_ == State::code)
        {
            // google.com/device takes the code in the address; any other page is only opened.
            const bool filled = url_.find("google.com/device") != std::string::npos;
            text(g, g.f.semibold, fit(g.f.semibold, filled ? tr("The code is already filled in") : tr("Opens the sign-in page"), 26.0f, room), kScan.x + 44.0f,
                 kScan.y + 102.0f, 26.0f, g.k.ink);
            const float in = tween::cubic_out(tween::clamp01(code_age_ / 0.4f));
            g.l.push_opacity(in);
            // A code is scanned dark on light, whatever the look.
            qr_code(g, square, filled ? url_ + "?user_code=" + code_ : url_, g.k.dark ? g.k.on_plate : g.k.plate, g.k.dark ? g.k.plate : g.k.on_plate);
            g.l.pop_opacity();
            text(g, g.f.regular, fit(g.f.regular, spoken(url_), 22.0f, room), kScan.cx(), kScan.y + 612.0f, 22.0f, g.k.ink2, gfx::Align::center);
            return;
        }
        // No code to scan: the square waits, or says that the code is gone.
        if (state_ == State::starting)
        {
            bone(g, {kScan.x + 44.0f, kScan.y + 84.0f, 330.0f, 22.0f}, 11.0f, 1);
            bone(g, square, 18.0f, 2);
            spinner(g, square.cx(), square.cy(), 28.0f, g.k.accent);
            return;
        }
        text(g, g.f.semibold, fit(g.f.semibold, tr("No code to scan"), 26.0f, room), kScan.x + 44.0f, kScan.y + 102.0f, 26.0f, g.k.ink2);
        g.l.rounded_rect(square, 18.0f, g.k.raised);
        g.l.bordered_rect(square, 18.0f, clear(), 1.5f, g.k.hairline);
        icon(g, Icon::qr, square.cx(), square.cy(), 120.0f, g.k.ink3);
    }

    Route route_;
    const ui::Fonts *fonts_ = nullptr;
    float age_ = 0.0f;
    State state_ = State::starting;
    float state_age_ = 0.0f;
    bool asked_ = false; // "auth:start" was listed for this visit

    unsigned start_ticket_ = 0;
    unsigned poll_ticket_ = 0;
    unsigned use_ticket_ = 0;
    float poll_in_ = kPoll;
    int missed_ = 0;

    std::string code_;
    std::string url_;
    float remaining_ = 0.0f; // seconds the code is still good for
    float code_age_ = 0.0f;
    std::string error_;
    std::string profile_row_;
    std::string profile_name_;

    int choice_ = 0;
    tween::Spring choice_focus_[2];
    ui::Pulse nudge_;
};

} // namespace

std::unique_ptr<Screen> make_sign_in()
{
    return std::make_unique<SignIn>();
}

} // namespace afterglow
