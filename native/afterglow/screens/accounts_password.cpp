// ProsperoTube Afterglow - the sheet a profile's password is typed on.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The kit's keyboard and text field on one panel over the accounts page. The
// provider arms a prompt ("Password of this profile", "New password of this
// profile", "Current password of this profile") and compares or hashes what is
// typed; the sheet only collects the text and hands it on. A password that
// exists is typed masked; a new one is shown, because it is asked for once
// and has to be read back.

#include "afterglow/screens/accounts.hpp"

#include <algorithm>

namespace afterglow::acct
{

namespace
{

constexpr Rect kSheet{510.0f, 140.0f, 900.0f, 716.0f};
constexpr int kLongest = 150; // the provider's limit for a password

} // namespace

PasswordSheet::PasswordSheet()
{
    keys_.style.bindings = ui::KeyboardBindings::standard();
    keys_.style.bindings.done = Action::menu; // Options submits, as it does wherever text is written
    keys_.style.max_length = kLongest;
    keys_.style.panel = false; // the sheet is the panel
    keys_.set_bounds({kSheet.x + 44.0f, kSheet.y + 236.0f, kSheet.w - 88.0f, 372.0f});
    field_.style.max_length = kLongest;
    field_.style.counter = false;
    field_.set_bounds({kSheet.x + 44.0f, kSheet.y + 96.0f, kSheet.w - 88.0f, field_.preferred_height()});
}

void PasswordSheet::open(std::string title, std::string label, bool masked, const ui::Fonts &fonts, Feedback &feedback)
{
    title_ = std::move(title);
    label_ = std::move(label);
    // A title that needs a second line ("... (experimental)" in a longer language) gets it: the sheet grows downward.
    extra_ = measure_block(fonts.semibold, title_, kTypeShelf, kSheet.w - 88.0f - 60.0f, 34.0f, 2).lines > 1 ? 34.0f : 0.0f;
    field_.set_bounds({kSheet.x + 44.0f, kSheet.y + 96.0f + extra_, kSheet.w - 88.0f, field_.preferred_height()});
    keys_.set_bounds({kSheet.x + 44.0f, kSheet.y + 236.0f + extra_, kSheet.w - 88.0f, 372.0f});
    field_.clear();
    field_.set_label(label_);
    field_.set_error({});
    field_.style.password = masked;
    field_.set_active(true);
    keys_.set_layout(0);
    keys_.set_shift(ui::KeyboardShift::off);
    keys_.set_focus(1, 0);
    keys_.set_length(0);
    keys_.enter();
    if (!open_)
        feedback.play(audio::Cue::modal_open);
    open_ = true;
    busy_ = false;
}

void PasswordSheet::close()
{
    open_ = false;
    busy_ = false;
    field_.clear();
    field_.set_active(false);
}

void PasswordSheet::refuse(std::string reason, Feedback &feedback)
{
    busy_ = false;
    field_.clear();
    keys_.set_length(0);
    field_.set_error(std::move(reason));
    feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.8f);
    feedback.rumble(0.25f, 0.05f);
}

std::string PasswordSheet::take()
{
    std::string text = field_.text();
    field_.clear();
    keys_.set_length(0);
    return text;
}

int PasswordSheet::update(const InputFrame &input, UpdateContext &context)
{
    dress(context);
    shown_.target = open_ ? 1.0f : 0.0f;
    shown_.update(context.dt, context.reduced_motion ? 60.0f : 15.0f);
    keys_.update(context.dt);
    field_.update(context.dt);
    if (!open_ || !context.top)
        return 0;
    if (busy_)
    {
        // The text is with the provider: only leaving is taken meanwhile.
        if (pressed(input, Action::back) || context.typed.find('\x1b') != std::string_view::npos)
        {
            close();
            context.feedback.play(audio::Cue::modal_close);
            return -1;
        }
        return 0;
    }
    ui::Event event = keys_.handle(input, context.feedback);
    bool edited = false;
    for (int i = 0; i < keys_.erased(); ++i)
        edited = field_.backspace() || edited;
    if (!keys_.typed().empty())
        edited = field_.insert(std::string_view(keys_.typed())) || edited;
    for (const char ch : context.typed)
    {
        if (ch == '\r') { event = ui::Event::activated; break; }
        if (ch == '\x1b') { event = ui::Event::cancelled; break; }
        edited = (ch == '\b' ? field_.backspace() : field_.insert(std::string_view(&ch, 1))) || edited;
    }
    if (edited && !field_.error().empty())
        field_.set_error({});
    keys_.set_length(field_.length());
    if (event == ui::Event::activated)
    {
        if (field_.length() == 0)
        {
            refuse(tr("Enter a password of 1 to 150 characters."), context.feedback);
            return 0;
        }
        return 1;
    }
    if (event == ui::Event::cancelled)
    {
        close();
        context.feedback.play(audio::Cue::modal_close);
        return -1;
    }
    return 0;
}

void PasswordSheet::dress(const UpdateContext &context)
{
    if (!context.look)
        return;
    for (ui::ComponentStyle *style : {static_cast<ui::ComponentStyle *>(&keys_.style), static_cast<ui::ComponentStyle *>(&field_.style)})
    {
        wear(*style, *context.look, context.reduced_motion);
    }
    // The bullet where the face has it.
    field_.style.mask = covers(context.services.fonts.regular, "\xE2\x80\xA2") ? "\xE2\x80\xA2" : "*";
}

void PasswordSheet::draw(const DrawContext &context) const
{
    if (!visible())
        return;
    G g = context.g();
    const float a = tween::clamp01(shown_.value);
    g.l.rounded_rect({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f, kBlack.with_alpha((g.k.dark ? 0.55f : 0.3f) * a));
    g.l.push_opacity(a);
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, g.reduced ? 0.0f : (1.0f - a) * 70.0f);
    const Rect sheet{kSheet.x, kSheet.y, kSheet.w, kSheet.h + extra_};
    lift(g, sheet, 36.0f, 1.1f);
    glass_panel(g, sheet, 36.0f, 1.5f);
    const float x = kSheet.x + 44.0f;
    const float room = kSheet.w - 88.0f;
    paragraph(g, g.f.semibold, title_, x, kSheet.y + 64.0f, kTypeShelf, room - 60.0f, 34.0f, g.k.ink, 2);
    if (busy_)
        spinner(g, kSheet.x + kSheet.w - 62.0f, kSheet.y + 56.0f, 16.0f, g.k.accent);
    ui::Canvas canvas{context.ui, context.fonts, context.glass, context.time};
    field_.draw(canvas);
    keys_.draw(canvas);
    paragraph(g, g.f.regular,
              tr("Experimental: tested offline only, not yet on a PS5 or against live YouTube. It may fail or change in a later version."),
              x, kSheet.y + 650.0f + extra_, kTypeMeta, room, 28.0f, g.k.ink3, 2);
    g.l.pop_transform();
    g.l.pop_opacity();
}

FocusInfo PasswordSheet::focus() const
{
    FocusInfo info;
    info.id = "password/key";
    const ui::KeyboardKey &key = keys_.focused();
    info.text = !key.text.empty() ? key.text : key.label;
    info.rect = keys_.key_rect(keys_.focus_row(), keys_.focus_key());
    info.index = keys_.focus_key();
    info.total = 10;
    return info;
}

std::span<const ui::Hint> PasswordSheet::hints() const
{
    static ui::Hint items[6];
    items[0] = {ui::Button::l2, tr("Shift")};
    items[1] = {ui::Button::r2, tr("Symbols")};
    items[2] = {ui::Button::triangle, tr("Space")};
    items[3] = {ui::Button::square, tr("Delete")};
    items[4] = {ui::Button::options, tr("Done")};
    items[5] = {ui::Button::circle, tr("Cancel")};
    return items;
}

} // namespace afterglow::acct
