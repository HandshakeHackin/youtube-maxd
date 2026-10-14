// ProsperoTube Afterglow - what the account screens share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Accounts ("Who is watching?"), the sign-in page and the phone page are one
// family: they read the provider's account catalogs, where listing a catalog
// is often the action itself (listing "library:profile:<id>" selects that
// profile, "auth:start" asks for a sign-in code, "settings:remote:pair" for a
// TV code), they share the two-line page head, the boxes a code is shown in,
// the notice for what a build does not offer, the question with a safe answer
// and the sheet a profile's password is typed on.

#pragma once

#include "afterglow/screen.hpp"

#include "ui/components/dialog.hpp"
#include "ui/components/keyboard.hpp"
#include "ui/components/text_field.hpp"

#include <span>
#include <string>
#include <string_view>

namespace afterglow::acct
{

// ---- data ------------------------------------------------------------------------

// True while an action has not been answered. A ticket that is gone counts as
// answered: its Page has Load::idle, which the screens treat as a failure.
inline bool pending(const Page &action)
{
    return action.load == model::Load::loading;
}

const model::Row *find_row(const Page &page, std::string_view id);
const model::Row *find_prefix(const Page &page, std::string_view prefix);
// The fact of a name on the first row that has it; "" when none has.
const std::string &fact_of(const Page &page, std::string_view name);
// m:ss of a number of seconds.
std::string clock_text(float seconds);

// ---- pieces ----------------------------------------------------------------------

// The head of a page that names its family above its title ("SIGN IN TO
// YOUTUBE" over "Finish on your phone or computer").
void page_head(G &g, std::string_view family, std::string_view title, float amount);
// A numbered step's disc; the line beside it is the caller's.
void step_disc(G &g, float cx, float cy, int number);
// One box of a code: `in` 0..1 pops it in (it overshoots), `text` is drawn in the display face.
void code_box(G &g, const Rect &r, std::string_view value, float size, float in, float radius, float tracking = 0.0f);
// The same box while the code is on its way.
void code_bone(G &g, const Rect &r, float radius, int index);
// A panel that says one thing: an icon on a disc, a line and what it means.
// tone tints the disc (the look's warning or danger); alpha 0 keeps it neutral.
void notice(G &g, const Rect &r, Icon glyph, std::string_view title, std::string_view body, Color tone = clear());
// The whole page as one message: what a build does not offer, or what failed.
// action is the label of the one way on ("" for none); focus lights it.
// Returns the button's rectangle (zero when there is none).
Rect message_page(G &g, Icon glyph, std::string_view title, std::string_view body, std::string_view action, float focus,
                  float amount, Color tone = clear());
Rect message_button_rect(const ui::Fonts &fonts, std::string_view body, std::string_view action);

// ---- the sheet a password is typed on -------------------------------------------------

class PasswordSheet
{
  public:
    PasswordSheet();
    // masked hides what is typed (a password that exists); a new one is shown, so it can be read back.
    void open(std::string title, std::string label, bool masked, const ui::Fonts &fonts, Feedback &feedback);
    void close();
    bool is_open() const
    {
        return open_;
    }
    bool visible() const
    {
        return open_ || shown_.value > 0.01f;
    }
    // The provider refused the text: say why and let it be typed again.
    void refuse(std::string reason, Feedback &feedback);
    // The text was handed over and its answer is awaited.
    void set_busy(bool busy)
    {
        busy_ = busy;
    }
    bool busy() const
    {
        return busy_;
    }
    // 1: the text was submitted (take() it), -1: the sheet was put away, 0: neither.
    int update(const InputFrame &input, UpdateContext &context);
    void draw(const DrawContext &context) const;
    // The typed text; the sheet forgets it.
    std::string take();
    FocusInfo focus() const;
    std::span<const ui::Hint> hints() const;
    const std::string &label() const
    {
        return label_;
    }

  private:
    void dress(const UpdateContext &context);

    mutable ui::Keyboard keys_;
    mutable ui::TextField field_;
    std::string title_;
    std::string label_;
    float extra_ = 0.0f; // what a title of two lines adds to the sheet's height
    bool open_ = false;
    bool busy_ = false;
    tween::Spring shown_;
};

} // namespace afterglow::acct
