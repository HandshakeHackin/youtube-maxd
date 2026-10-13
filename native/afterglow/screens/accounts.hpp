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

// REQUEST: Data::answer(text): hand a typed text to the provider's armed
// prompt (a profile's password) and list the catalog it names, as an action;
// a refusal is Load::failed with the provider's reason. Until then the
// stand-in acts on this row, which carries no text (requests/F.md, 1).
inline constexpr const char *kPromptAnswer = "prompt:answer";

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
// "{0}" and "{1}" of a translated pattern replaced.
std::string with(const char *pattern, std::string_view first, std::string_view second = {});
// "ready", "loading", "failed", "offline": the snapshot's word for a Load.
const char *load_name(model::Load load);
// m:ss of a number of seconds.
std::string clock_text(float seconds);

// ---- motion ----------------------------------------------------------------------

// A page in a hand-over: it fades, slides a little the way the move goes and
// stays put otherwise (design notes, section 8: deeper and back out).
// REQUEST: a shared piece for this in design.hpp, so every screen moves alike
// (requests/F.md, 4).
class Travel
{
  public:
    Travel(G &g, const Phase &phase);
    ~Travel();
    Travel(const Travel &) = delete;
    Travel &operator=(const Travel &) = delete;
    float alpha() const
    {
        return alpha_;
    }

  private:
    gfx::DrawList &l_;
    float alpha_ = 1.0f;
};

// How far group `index` of a page that is `age` seconds old has arrived.
inline float arrive(float age, int index, float step = 0.06f, float duration = 0.5f)
{
    return tween::cubic_out(tween::clamp01((age - static_cast<float>(index) * step) / duration));
}

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
// A working arc: something is being waited for.
void spinner(G &g, float cx, float cy, float radius, Color color);
// A panel that says one thing: an icon on a disc, a line and what it means.
// tone tints the disc (the look's warning or danger); alpha 0 keeps it neutral.
void notice(G &g, const Rect &r, Icon glyph, std::string_view title, std::string_view body, Color tone = clear());
// The whole page as one message: what a build does not offer, or what failed.
// action is the label of the one way on ("" for none); focus lights it.
// Returns the button's rectangle (zero when there is none).
Rect message_page(G &g, Icon glyph, std::string_view title, std::string_view body, std::string_view action, float focus,
                  float amount, Color tone = clear());
Rect message_button_rect(const ui::Fonts &fonts, std::string_view body, std::string_view action);

// ---- the question with a safe answer ------------------------------------------------

// REQUEST: Nav::confirm with a destructive answer, its own two labels and the
// focus on the safe one (board 41), and a question with three answers. The
// frame's dialog opens on the confirming answer and names the other "Cancel";
// until it can do this, the account screens keep the kit's dialog themselves
// for these two questions (requests/F.md, 2).
class Question
{
  public:
    // answers: one to three labels; `danger` is the index of the answer drawn
    // in the warning colour (-1 for none); the focus opens on `safe`.
    void ask(ui::StatusKind icon, std::string title, std::string body, std::vector<std::string> answers, int danger,
             int safe, bool stacked, Feedback &feedback);
    bool is_open() const
    {
        return dialog_.is_open();
    }
    // The answer chosen this frame (its index), -1 when the question was put
    // away, -2 while nothing was decided.
    int update(const InputFrame &input, float dt, Feedback &feedback, bool top);
    void draw(const DrawContext &context) const;
    int focus() const
    {
        return dialog_.focus();
    }
    const std::string &title() const
    {
        return dialog_.content().title;
    }
    std::string answer(int index) const;

  private:
    mutable ui::Dialog dialog_;
    mutable const Look *worn_ = nullptr;
};

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
    void dress(const DrawContext &context) const;

    mutable ui::Keyboard keys_;
    mutable ui::TextField field_;
    mutable const Look *worn_ = nullptr;
    std::string title_;
    std::string label_;
    float extra_ = 0.0f; // what a title of two lines adds to the sheet's height
    bool open_ = false;
    bool busy_ = false;
    tween::Spring shown_;
};

} // namespace afterglow::acct
