// ProsperoTube Afterglow - linking the YouTube app on a phone with a TV code.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The page shows the provider's phone-pairing catalog "settings:remote" and
// acts on its rows: "settings:remote:pair" turns the connection on and asks
// for a TV code, "settings:remote:toggle" turns the connection on or off,
// "settings:remote:disconnect" and "settings:remote:history" change what
// happens when the phone leaves and whether phone playback is recorded,
// "settings:remote:reset" forgets this TV's pairing. An acting row answers
// with the catalog as it is afterwards.
//
// Facts it reads: on "settings:remote:info" state (off, connecting, ready,
// stopped; anything else is shown in the provider's own words from value) and
// available ("0" for a build whose service links no phones); on
// "settings:remote:code" value (the digits); on "settings:remote:toggle"
// selected (the connection is on); rows "settings:remote:device:<id>" are the
// phones linked now (title: the phone's name, subtitle: since when, fact
// connected). The three settings rows are shown in the provider's own words.

#include "afterglow/screens/accounts.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow
{

namespace
{

using namespace acct;

constexpr float kColumn = 900.0f;
constexpr float kPoll = 2.0f; // seconds between two looks while a code is awaited
constexpr Rect kDevices{kLeft, 728.0f, kColumn, 152.0f};
constexpr Rect kPhone{1380.0f, 220.0f, 320.0f, 640.0f};
constexpr Rect kSheet{1180.0f, 212.0f, 644.0f, 484.0f};
constexpr float kRowHeight = 126.0f;
constexpr const char *kOptionRows[] = {"settings:remote:disconnect", "settings:remote:history", "settings:remote:reset"};
constexpr const char *kOptionIds[] = {"option/disconnect", "option/history", "option/reset"};

struct Device
{
    std::string name;
    std::string detail;
    bool connected = true;
};

// The catalog as the page shows it.
struct Remote
{
    bool available = true;
    std::string state;       // off, connecting, ready, stopped, or the provider's own word
    std::string state_words; // ... as the provider says it
    std::string code;        // digits, "" for none
    bool on = false;
    std::vector<Device> devices;
    std::string option_title[3];
    std::string option_detail[3];
};

class LinkPhone final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::link_phone;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        fonts_ = &context.services.fonts;
        if (restored)
            return;
        route_ = route;
        age_ = 0.0f;
        load_ = model::Load::loading;
        known_ = false;
        remote_ = Remote{};
        ticket_ = 0;
        sheet_open_ = false;
        option_ = 0;
        code_age_ = 10.0f;
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        code_age_ += context.dt;
        fonts_ = &context.services.fonts;
        Data &data = context.services.data;
        sheet_shown_.target = sheet_open_ ? 1.0f : 0.0f;
        sheet_shown_.update(context.dt, context.reduced_motion ? 60.0f : 13.0f);
        option_pos_.target = static_cast<float>(option_);
        option_pos_.update(context.dt, context.reduced_motion ? 60.0f : 20.0f);
        nudge_.update(context.dt, 10.0f);
        busy_age_ = ticket_ != 0 ? busy_age_ + context.dt : 0.0f;

        // The catalog, read every frame; what is shown stays until a newer answer is there.
        const Page &page = data.catalog("settings:remote");
        if (page.load == model::Load::ready)
        {
            take(page, context);
            load_ = model::Load::ready;
        }
        else if (page.load == model::Load::failed || page.load == model::Load::offline)
        {
            if (!known_)
            {
                load_ = page.load;
                error_ = page.error;
            }
        }
        else if (!known_)
        {
            load_ = model::Load::loading;
        }
        // A row that was acted on answers with the catalog as it is now.
        if (ticket_ != 0)
        {
            const Page &answer = data.action(ticket_);
            if (!pending(answer))
            {
                const Page copy = answer;
                ticket_ = 0;
                if (copy.load == model::Load::ready)
                {
                    if (find_row(copy, "settings:remote:info") != nullptr)
                        take(copy, context);
                    data.forget("settings:remote");
                    if (acted_ == "settings:remote:reset")
                        context.nav.toast(tr("Phone pairing was reset"), ToastKind::success);
                }
                else
                {
                    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.8f);
                    context.nav.toast(!copy.error.empty()                    ? copy.error
                                      : copy.load == model::Load::offline ? std::string(tr("The YouTube service cannot be reached."))
                                                                          : std::string(tr("That did not work. Try again.")),
                                      ToastKind::error);
                }
            }
        }
        // While YouTube is asked for a code, the catalog is looked at again.
        if (known_ && state() == "connecting" && ticket_ == 0)
        {
            poll_in_ -= context.dt;
            if (poll_in_ <= 0.0f)
            {
                poll_in_ = kPoll;
                data.forget("settings:remote");
            }
        }

        const int decided = question_.update(input, context.dt, context.feedback, context.top);
        if (decided != -2)
        {
            if (decided == 1)
                act(data, "settings:remote:reset");
            return;
        }
        if (!context.top || question_.is_open())
            return;

        if (sheet_open_)
        {
            update_sheet(input, context);
            return;
        }
        if (pressed(input, Action::back))
        {
            context.nav.back();
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
        if (load_ != model::Load::ready)
        {
            if (pressed(input, Action::confirm) && load_ != model::Load::loading)
            {
                context.feedback.play(audio::Cue::select);
                data.forget("settings:remote");
                load_ = model::Load::loading;
            }
            return;
        }
        if (!remote_.available)
            return;
        if (pressed(input, Action::confirm))
        {
            if (ticket_ != 0)
                refuse(input, context);
            else
            {
                context.feedback.play(audio::Cue::select);
                act(data, "settings:remote:pair");
            }
        }
        else if (pressed(input, Action::west))
        {
            if (ticket_ != 0)
                refuse(input, context);
            else
            {
                // A switch: up when it turns on, down when it turns off.
                context.feedback.play(audio::Cue::toggle, remote_.on ? 0.94f : 1.08f);
                act(data, "settings:remote:toggle");
            }
        }
        else if (pressed(input, Action::menu))
        {
            sheet_open_ = true;
            option_ = 0;
            option_pos_.snap(0.0f);
            context.feedback.play(audio::Cue::modal_open, 1.0f, 0.4f);
        }
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        {
            Travel travel(g, phase);
            page_head(g, tr("Phone pairing (experimental)"), tr("Link the YouTube app on your phone"), arrive(age_, 0, 0.05f, 0.5f));
            if (load_ == model::Load::ready && !remote_.available)
            {
                message_page(g, Icon::phone, tr("Not available in this build"), tr("Linking a phone is not part of this built-in YouTube service."), {},
                             0.0f, arrive(age_, 1, 0.06f, 0.5f), g.k.theme.warning);
            }
            else if (load_ == model::Load::failed || load_ == model::Load::offline)
            {
                const bool offline = load_ == model::Load::offline;
                message_page(g, offline ? Icon::offline : Icon::alert,
                             offline ? tr("No connection to the YouTube service") : tr("The phone connection could not be read"), failure_words(),
                             tr("Try again"), 1.0f, arrive(age_, 1, 0.06f, 0.5f), offline ? g.k.theme.warning : g.k.theme.danger);
            }
            else
            {
                draw_steps(g);
                draw_code(g);
                draw_devices(g);
                draw_phone(g);
                draw_sheet(g);
            }
        }
        question_.draw(context);
    }

    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[4];
        int count = 0;
        if (question_.is_open())
        {
            items[count++] = {ui::Button::cross, tr("Choose")};
            items[count++] = {ui::Button::circle, tr("Cancel")};
        }
        else if (sheet_open_)
        {
            items[count++] = {ui::Button::cross, option_ == 2 ? tr("Choose") : tr("Change")};
            items[count++] = {ui::Button::circle, tr("Close")};
        }
        else if (load_ == model::Load::failed || load_ == model::Load::offline)
        {
            items[count++] = {ui::Button::cross, tr("Try again")};
            items[count++] = {ui::Button::circle, tr("Back")};
        }
        else if (load_ != model::Load::ready || !remote_.available)
        {
            items[count++] = {ui::Button::circle, tr("Back")};
        }
        else
        {
            items[count++] = {ui::Button::cross, remote_.code.empty() ? tr("Link a phone") : tr("Get a new code")};
            items[count++] = {ui::Button::square, remote_.on ? tr("Turn connection off") : tr("Turn connection on")};
            items[count++] = {ui::Button::options, tr("More")};
            items[count++] = {ui::Button::circle, tr("Back")};
        }
        return std::span<const ui::Hint>(items, static_cast<std::size_t>(count));
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        info.total = 1;
        if (question_.is_open())
        {
            info.id = "dialog/" + std::to_string(question_.focus());
            info.text = question_.answer(question_.focus());
            info.index = question_.focus();
            info.total = 2;
        }
        else if (sheet_open_)
        {
            info.id = kOptionIds[option_];
            info.text = remote_.option_title[option_];
            info.rect = option_rect(option_);
            info.index = option_;
            info.total = 3;
        }
        else if (load_ == model::Load::ready && !remote_.available)
        {
            info.id = "unavailable";
            info.text = tr("Not available in this build");
        }
        else if (load_ == model::Load::failed || load_ == model::Load::offline)
        {
            info.id = "action/retry";
            info.text = tr("Try again");
            info.rect = fonts_ != nullptr ? message_button_rect(*fonts_, failure_words(), tr("Try again")) : Rect{};
        }
        else
        {
            info.id = "code";
            info.text = remote_.code;
            info.rect = {kLeft, 440.0f, kColumn, 124.0f};
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        if (sheet_open_ && !question_.is_open())
            return {{kOptionIds[0], remote_.option_title[0]}, {kOptionIds[1], remote_.option_title[1]}, {kOptionIds[2], remote_.option_title[2]}};
        const FocusInfo only = focus();
        return {{only.id, only.text}};
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("load", load_name(load_));
        out.set("state", load_ != model::Load::ready ? load_name(load_) : !remote_.available ? "unavailable" : state());
        out.set("code", remote_.code);
        out.set("connection", remote_.on ? "on" : "off");
        out.set("devices", static_cast<int>(remote_.devices.size()));
        out.set("sheet", sheet_open_ ? 1 : 0);
        out.set("question", question_.is_open() ? "reset" : "");
        out.set("busy", ticket_ != 0 ? acted_ : std::string());
    }

  private:
    // ---- data ------------------------------------------------------------------

    void take(const Page &page, UpdateContext &context)
    {
        Remote next;
        const model::Row *info = find_row(page, "settings:remote:info");
        const model::Row *code = find_row(page, "settings:remote:code");
        const model::Row *toggle = find_row(page, "settings:remote:toggle");
        next.available = !page.rows.empty() && (info == nullptr || info->fact("available") != "0");
        if (code != nullptr)
        {
            // The digits: the fact, else whatever digits the row's words hold.
            const std::string &source = code->fact("value").empty() ? code->title : code->fact("value");
            for (const char c : source)
            {
                if (c >= '0' && c <= '9')
                    next.code.push_back(c);
            }
        }
        if (info != nullptr)
        {
            next.state = info->fact("state");
            next.state_words = info->fact("value");
        }
        next.on = toggle != nullptr && toggle->has("selected");
        if (next.state.empty())
            next.state = !next.code.empty() ? "ready" : next.on ? "connecting" : "off";
        if (next.state != "off")
            next.on = true;
        for (const model::Row &row : page.rows)
        {
            if (row.id.rfind("settings:remote:device:", 0) == 0)
                next.devices.push_back({row.title, row.subtitle, !row.has("connected") ? row.fact("connected").empty() : true});
        }
        for (int i = 0; i < 3; ++i)
        {
            const model::Row *row = find_row(page, kOptionRows[i]);
            next.option_title[i] = row != nullptr ? row->title : std::string();
            next.option_detail[i] = row != nullptr ? row->subtitle : std::string();
        }
        if (next.code != remote_.code && !next.code.empty())
        {
            code_age_ = 0.0f; // the groups arrive one by one
            if (known_)
                context.feedback.play(audio::Cue::notify, 1.0f, -0.2f, 0.6f);
        }
        remote_ = std::move(next);
        known_ = true;
    }

    const std::string &state() const
    {
        return remote_.state;
    }

    // Why the page has nothing to show, and what that leaves untouched.
    std::string failure_words() const
    {
        if (load_ == model::Load::offline)
            return tr("Linking a phone needs the YouTube service, which is not answering. What is saved on this PS5 stays available.");
        return error_.empty() ? std::string(tr("That did not work. Try again.")) : error_;
    }

    void act(Data &data, const char *row)
    {
        acted_ = row;
        ticket_ = data.act(row);
        poll_in_ = kPoll;
    }

    void refuse(const InputFrame &input, UpdateContext &context)
    {
        if (input.nav_repeat)
            return;
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
    }

    void update_sheet(const InputFrame &input, UpdateContext &context)
    {
        if (pressed(input, Action::back) || pressed(input, Action::menu) || input.nav == Direction::left)
        {
            sheet_open_ = false;
            context.feedback.play(audio::Cue::modal_close, 1.0f, 0.4f);
        }
        else if (input.nav == Direction::up || input.nav == Direction::down)
        {
            const int next = option_ + (input.nav == Direction::down ? 1 : -1);
            if (next < 0 || next > 2)
                refuse(input, context);
            else
            {
                option_ = next;
                context.feedback.play(audio::Cue::focus, 1.0f - 0.03f * static_cast<float>(next), 0.4f);
            }
        }
        else if (input.nav == Direction::right)
        {
            refuse(input, context);
        }
        else if (pressed(input, Action::confirm))
        {
            if (ticket_ != 0)
            {
                refuse(input, context);
            }
            else if (option_ == 2)
            {
                question_.ask(ui::StatusKind::danger, tr("Reset phone pairing?"),
                              tr("The connection is turned off and this TV's pairing identity is forgotten. A phone has to be linked again with a new TV code."),
                              {tr("Keep"), tr("Reset")}, 1, 0, false, context.feedback);
            }
            else
            {
                context.feedback.play(audio::Cue::toggle, 1.0f, 0.4f);
                act(context.services.data, kOptionRows[option_]);
            }
        }
    }

    // ---- layout ---------------------------------------------------------------------

    std::vector<std::string> groups() const
    {
        std::vector<std::string> parts;
        for (std::size_t at = 0; at < remote_.code.size(); at += 3)
            parts.push_back(remote_.code.substr(at, 3));
        return parts;
    }

    Rect option_rect(int index) const
    {
        return {kSheet.x + 16.0f, kSheet.y + 84.0f + kRowHeight * static_cast<float>(index), kSheet.w - 32.0f, kRowHeight - 8.0f};
    }

    // ---- drawing --------------------------------------------------------------------

    bool waiting() const
    {
        return !known_ || (remote_.code.empty() && (state() == "connecting" || (ticket_ != 0 && acted_ == "settings:remote:pair")));
    }

    void draw_steps(G &g) const
    {
        Enter enter(g, arrive(age_, 1, 0.07f, 0.5f), 22.0f);
        text(g, g.f.regular, fit(g.f.regular, tr("On your phone, in the YouTube app:"), 26.0f, kColumn), kLeft, 246.0f, 26.0f, g.k.ink2);
        // The way through the YouTube app, in its own words.
        const char *path[] = {tr("Settings"), tr("Watch on TV"), tr("Enter TV code")};
        float size = 25.0f;
        float total = 0.0f;
        for (const char *name : path)
            total += measure(g.f.semibold, name, size) + 44.0f;
        total += 2.0f * 42.0f;
        if (total > kColumn)
            size = std::max(18.0f, size * kColumn / total); // long words in another language: the row shrinks before it breaks
        float x = kLeft;
        for (int i = 0; i < 3; ++i)
        {
            const float w = measure(g.f.semibold, path[i], size) + 44.0f;
            g.l.rounded_rect({x, 272.0f, w, 54.0f}, 27.0f, g.k.raised);
            text(g, g.f.semibold, path[i], x + 22.0f, 299.0f + size * 0.34f, size, g.k.ink);
            x += w + 12.0f;
            if (i < 2)
            {
                icon(g, Icon::chevron_right, x + 8.0f, 299.0f, 20.0f, g.k.ink3);
                x += 30.0f;
            }
        }
    }

    void draw_code(G &g) const
    {
        Enter enter(g, arrive(age_, 2, 0.07f, 0.5f), 22.0f);
        overline(g, tr("TV code"), kLeft + 2.0f, 414.0f, g.k.ink3);
        const std::vector<std::string> parts = groups();
        const int count = parts.empty() ? 4 : static_cast<int>(parts.size());
        const float pitch = std::min(228.0f, (kColumn + 16.0f) / static_cast<float>(count));
        const float width = pitch - 16.0f;
        if (ticket_ != 0 && busy_age_ > 0.25f && !sheet_open_)
            spinner(g, kLeft + kColumn - 16.0f, 408.0f, 12.0f, g.k.accent);
        for (int i = 0; i < count; ++i)
        {
            const Rect box{kLeft + pitch * static_cast<float>(i), 440.0f, width, 124.0f};
            if (waiting())
            {
                code_bone(g, box, 22.0f, i);
            }
            else if (parts.empty())
            {
                // No code yet: the boxes are there, empty.
                g.l.push_opacity(0.6f);
                glass_panel(g, box, 22.0f, 1.1f);
                g.l.rounded_rect({box.cx() - 18.0f, box.cy() - 2.5f, 36.0f, 5.0f}, 2.5f, g.k.ink3);
                g.l.pop_opacity();
            }
            else
            {
                code_box(g, box, parts[static_cast<std::size_t>(i)], 66.0f * std::min(1.0f, width / 212.0f),
                         (code_age_ - 0.07f * static_cast<float>(i)) / 0.4f, 22.0f, 4.0f);
            }
        }
        paragraph(g, g.f.regular,
                  tr("Linking lets the phone send videos here, queue more and control playback. The code is asked for again whenever it has run out."),
                  kLeft, 628.0f, 22.0f, kColumn, 32.0f, g.k.ink2, 2);
    }

    void draw_devices(G &g) const
    {
        Enter enter(g, arrive(age_, 3, 0.07f, 0.5f), 22.0f);
        glass_panel(g, kDevices, 26.0f);
        if (!known_)
        {
            bone(g, {kDevices.x + 30.0f, kDevices.y + 34.0f, 150.0f, 14.0f}, 7.0f, 0);
            bone(g, {kDevices.x + 30.0f, kDevices.y + 74.0f, 48.0f, 48.0f}, 24.0f, 1);
            bone(g, {kDevices.x + 96.0f, kDevices.y + 76.0f, 240.0f, 20.0f}, 10.0f, 2);
            bone(g, {kDevices.x + 96.0f, kDevices.y + 108.0f, 380.0f, 14.0f}, 7.0f, 3);
        }
        else
        {
            const bool linked = !remote_.devices.empty();
            const std::string heading = !linked                        ? std::string(tr("Phone connection"))
                                        : remote_.devices.size() == 1 ? std::string(tr("Linked now"))
                                                                      : with(tr("Linked now: {0} phones"), std::to_string(remote_.devices.size()));
            overline(g, heading, kDevices.x + 30.0f, kDevices.y + 46.0f, g.k.ink3);
            // The connection, in a word and a mark: never by colour alone.
            const std::string &now = state();
            const char *words = now == "ready" ? tr("Connection is on") : now == "connecting" ? tr("Connecting") : now == "off" ? tr("Connection is off")
                                : now == "stopped" ? tr("Connection stopped") : remote_.state_words.c_str();
            const Color dot = now == "ready" ? g.k.theme.success : now == "connecting" ? g.k.theme.warning : now == "off" ? g.k.ink3 : g.k.theme.danger;
            const std::string fitted = fit(g.f.regular, words, 22.0f, 300.0f);
            const float w = text(g, g.f.regular, fitted, kDevices.x + kDevices.w - 30.0f, kDevices.y + 105.0f, 22.0f, g.k.ink2, gfx::Align::right);
            const float dot_x = kDevices.x + kDevices.w - 30.0f - w - 22.0f;
            if (now == "ready")
                g.l.circle(dot_x, kDevices.y + 98.0f, 6.0f, dot);
            else if (now == "connecting")
                g.l.circle(dot_x, kDevices.y + 98.0f, 6.0f, dot.with_alpha(g.reduced ? 1.0f : 0.5f + 0.5f * ui::breathe(g.time, 1.6f)));
            else
                g.l.ring(dot_x, kDevices.y + 98.0f, 6.5f, 2.5f, dot);
            const float room = kDevices.w - 96.0f - 30.0f - w - 60.0f;
            g.l.circle(kDevices.x + 54.0f, kDevices.y + 98.0f, 24.0f, g.k.raised);
            icon(g, Icon::phone, kDevices.x + 54.0f, kDevices.y + 98.0f, 26.0f, linked ? g.k.ink : g.k.ink3);
            std::string name;
            std::string detail;
            if (linked)
            {
                name = remote_.devices.front().name;
                detail = remote_.devices.front().detail;
            }
            else
            {
                name = tr("No phone is linked yet");
                detail = remote_.code.empty() ? tr("Get a TV code and enter it in the YouTube app.") : tr("Enter the TV code in the YouTube app.");
            }
            const bool one_line = detail.empty();
            text(g, g.f.semibold, fit(g.f.semibold, name, kTypeBody, room), kDevices.x + 96.0f, kDevices.y + (one_line ? 106.5f : 94.0f), kTypeBody,
                 linked ? g.k.ink : g.k.ink2);
            if (!one_line)
                text(g, g.f.regular, fit(g.f.regular, detail, kTypeMeta, room), kDevices.x + 96.0f, kDevices.y + 122.0f, kTypeMeta, g.k.ink3);
        }
        paragraph(g, g.f.regular,
                  tr("Experimental: tested offline only, not yet on a PS5 or against live YouTube. It may fail or change in a later version."),
                  kLeft, 916.0f, 19.0f, kColumn, 26.0f, g.k.ink3, 2);
    }

    // A picture of where the code goes: a phone, the YouTube app's field, the code being typed.
    void draw_phone(G &g) const
    {
        const float away = tween::clamp01(sheet_shown_.value);
        if (away >= 0.99f)
            return;
        Enter enter(g, arrive(age_, 2, 0.08f, 0.6f) * (1.0f - away), 0.0f, 48.0f);
        g.l.glow(kPhone.inset(40.0f), 60.0f, 140.0f, g.k.accent.with_alpha(0.14f));
        lift(g, kPhone, 48.0f, 1.2f);
        g.l.rounded_rect(kPhone, 48.0f, g.k.panel_solid.with_alpha(1.0f));
        g.l.bordered_rect(kPhone, 48.0f, clear(), 3.0f, g.k.hairline);
        const Rect glass{kPhone.x + 14.0f, kPhone.y + 14.0f, kPhone.w - 28.0f, kPhone.h - 28.0f};
        g.l.gradient_rect(glass, 36.0f, g.k.base2, g.k.base);
        g.l.bordered_rect(glass, 36.0f, clear(), 1.5f, g.k.hairline);
        g.l.rounded_rect({kPhone.cx() - 44.0f, kPhone.y + 28.0f, 88.0f, 22.0f}, 11.0f, g.k.dark ? g.k.base : g.k.ink.with_alpha(0.85f));
        text(g, g.f.semibold, fit(g.f.semibold, tr("Enter TV code"), 22.0f, glass.w - 32.0f), kPhone.cx(), kPhone.y + 124.0f, 22.0f, g.k.ink,
             gfx::Align::center);
        const Rect field{glass.x + 22.0f, kPhone.y + 156.0f, glass.w - 44.0f, 66.0f};
        g.l.rounded_rect(field, 14.0f, g.k.raised);
        g.l.bordered_rect(field, 14.0f, clear(), 2.0f, g.k.accent);
        // The phone types the code that is on the page, digit by digit, and starts over.
        const std::string &code = remote_.code;
        const int digits = static_cast<int>(code.size());
        int typed = 0;
        if (digits > 0)
        {
            const float period = 0.45f * static_cast<float>(digits) + 3.0f;
            const float t = g.reduced ? 0.45f * static_cast<float>(digits * 2 / 3) : std::fmod(std::max(code_age_ - 1.0f, 0.0f), period);
            typed = std::clamp(static_cast<int>(t / 0.45f), 0, digits);
        }
        std::string shown;
        for (int i = 0; i < typed; ++i)
        {
            if (i > 0 && i % 3 == 0)
                shown.push_back(' ');
            shown.push_back(code[static_cast<std::size_t>(i)]);
        }
        const float size = digits > 9 ? 22.0f : 25.0f;
        const float w = text(g, g.f.mono, shown, field.x + 16.0f, field.cy() + 8.0f, size, g.k.ink);
        if (g.reduced || std::fmod(g.time, 1.0f) < 0.55f)
            g.l.rounded_rect({field.x + 18.0f + w, field.cy() - 15.0f, 2.5f, 30.0f}, 1.0f, g.k.accent);
        const char next = typed < digits ? code[static_cast<std::size_t>(typed)] : '\0';
        static constexpr const char *kKeys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "", "0", ""};
        for (int i = 0; i < 12; ++i)
        {
            const float kx = glass.x + 46.0f + static_cast<float>(i % 3) * 86.0f;
            const float ky = kPhone.y + 262.0f + static_cast<float>(i / 3) * 78.0f;
            const bool lit = kKeys[i][0] != '\0' && kKeys[i][0] == next;
            g.l.rounded_rect({kx, ky, 70.0f, 62.0f}, 16.0f, lit ? g.k.ink.with_alpha(0.28f) : g.k.raised);
            if (kKeys[i][0] != '\0')
                text(g, g.f.semibold, kKeys[i], kx + 35.0f, ky + 40.0f, 24.0f, g.k.ink, gfx::Align::center);
        }
        g.l.rounded_rect({kPhone.cx() - 54.0f, kPhone.y + kPhone.h - 34.0f, 108.0f, 5.0f}, 2.5f, g.k.ink3);
    }

    // What happens around the connection: the provider's own three rows, on a sheet from the right.
    void draw_sheet(G &g) const
    {
        const float shown = tween::clamp01(sheet_shown_.value);
        if (shown <= 0.01f)
            return;
        g.l.push_opacity(shown);
        g.l.push_transform(1.0f, 0.0f, 0.0f, g.reduced ? 0.0f : (1.0f - shown) * 150.0f, 0.0f);
        lift(g, kSheet, 36.0f, 1.1f);
        glass_panel(g, kSheet, 36.0f, 1.4f);
        text(g, g.f.display, fit(g.f.display, tr("Phone connection"), 30.0f, kSheet.w - 88.0f), kSheet.x + 44.0f, kSheet.y + 60.0f, 30.0f, g.k.ink);
        // The plate glides under the row in focus.
        const float at = option_pos_.value;
        Rect plate = option_rect(0);
        plate.y += kRowHeight * at;
        plate.x += g.reduced ? 0.0f : ui::shake(nudge_.value, g.time);
        g.l.rounded_rect(plate, 24.0f, g.k.raised);
        focus_ring(g, plate, 24.0f, g.k.focus, 1.0f);
        static constexpr Icon kIcons[] = {Icon::phone, Icon::clock, Icon::update};
        for (int i = 0; i < 3; ++i)
        {
            const Rect r = option_rect(i);
            const bool busy = ticket_ != 0 && acted_ == kOptionRows[i] && busy_age_ > 0.25f;
            const float cx = r.x + 46.0f;
            if (busy)
                spinner(g, cx, r.cy(), 15.0f, g.k.accent);
            else
            {
                g.l.circle(cx, r.cy(), 24.0f, g.k.raised);
                icon(g, kIcons[i], cx, r.cy(), 26.0f, i == 2 ? g.k.theme.danger : g.k.ink);
            }
            // The provider's own words: what the row is and does, on up to two lines under its name.
            const float room = r.w - 88.0f - 24.0f;
            const int lines = remote_.option_detail[i].empty() ? 0 : measure_block(g.f.regular, remote_.option_detail[i], 19.0f, room, 25.0f, 2).lines;
            const float top = r.cy() - (lines == 2 ? 20.0f : lines == 1 ? 6.0f : -8.0f);
            text(g, g.f.semibold, fit(g.f.semibold, remote_.option_title[i], 23.0f, room), r.x + 88.0f, top, 23.0f, g.k.ink);
            if (lines > 0)
                paragraph(g, g.f.regular, remote_.option_detail[i], r.x + 88.0f, top + 28.0f, 19.0f, room, 25.0f, g.k.ink2, 2);
        }
        g.l.pop_transform();
        g.l.pop_opacity();
    }

    Route route_;
    const ui::Fonts *fonts_ = nullptr;
    float age_ = 0.0f;

    model::Load load_ = model::Load::loading;
    std::string error_;
    Remote remote_;
    bool known_ = false; // the catalog was answered at least once
    float code_age_ = 10.0f;
    float poll_in_ = kPoll;

    unsigned ticket_ = 0; // the row that was acted on and has not answered yet
    std::string acted_;
    float busy_age_ = 0.0f;

    bool sheet_open_ = false;
    int option_ = 0;
    tween::Spring sheet_shown_;
    tween::Spring option_pos_;
    ui::Pulse nudge_;
    Question question_;
};

} // namespace

std::unique_ptr<Screen> make_link_phone()
{
    return std::make_unique<LinkPhone>();
}

} // namespace afterglow
