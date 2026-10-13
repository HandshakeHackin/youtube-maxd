// ProsperoTube Afterglow - the frame's sheets: a row's actions, and a text that is typed.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Both belong to the frame, as the dialog does: a screen asks for them through
// Nav (actions(), ask_text()), the frame gives them the input while they are
// open and draws them over everything else of the page.

#pragma once

#include "afterglow/data_feed.hpp"
#include "afterglow/screen.hpp"

#include "ui/components/keyboard.hpp"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace afterglow
{

// The actions of a row (board 14): the catalog Data::menu() names, its rows as
// a list, a row chosen through Data::act(), an answer that is a further list
// shown in place, a row that is another page handed back as a Route.
class ActionsSheet
{
  public:
    ActionsSheet();

    bool open() const
    {
        return open_;
    }
    // True while the sheet is on screen, also on its way out.
    bool visible() const
    {
        return amount_.value > 0.01f;
    }
    // Opens the actions of a row ("<video id>", "channel:UC..."); about is what the sheet is about, in
    // words. False when the row has no actions: the refusal has been played.
    bool show(const std::string &row, std::string_view about, UpdateContext &context);
    // Puts the sheet away without a sound: the stack changed under it.
    void dismiss();
    // One frame of input while the sheet is open. True when the choice is another page: route says which.
    bool handle(const InputFrame &input, UpdateContext &context, Route *route);
    // Every frame: reads the menu, follows a running action, moves.
    void update(UpdateContext &context);
    void draw(G &g) const;
    // True once after an action went through: the lists of the screen may be older than the account now.
    bool take_changed();

    FocusInfo focus() const;
    std::span<const ui::Hint> hints() const;
    void snapshot(Snapshot &out) const;

  private:
    struct Level
    {
        std::string title;
        std::vector<model::Row> rows;
        int focus = 0;
    };
    void close(UpdateContext &context, bool sound);
    void begin(const model::Row &row, UpdateContext &context);
    Rect row_rect(int index) const;

    bool open_ = false;
    std::string menu_;
    std::string about_;
    std::vector<Level> levels_;
    model::Load load_ = model::Load::idle;
    std::string error_;
    Act act_;
    int acting_ = -1;
    std::string acting_title_;
    std::string confirmed_; // the row the person agreed to in the dialog
    bool changed_ = false;
    tween::Spring amount_;
    tween::Spring pos_;
    tween::Spring scroll_;
    ui::Pulse nudge_;
    std::shared_ptr<int> alive_;
};

// A text typed on the kit's keyboard, in a sheet in the middle of the page.
// Cross types the key in focus, Square deletes, Triangle is a space, L2 shifts,
// R2 turns to the symbols, Options takes the text, Circle puts the sheet away.
class TextSheet
{
  public:
    bool open() const
    {
        return open_;
    }
    bool visible() const
    {
        return amount_.value > 0.01f;
    }
    void show(TextQuestion question, std::function<void(bool, std::string)> answered, UpdateContext &context);
    void dismiss();
    void handle(const InputFrame &input, UpdateContext &context);
    void update(UpdateContext &context);
    void draw(G &g) const;

    FocusInfo focus() const;
    std::span<const ui::Hint> hints() const;
    void snapshot(Snapshot &out) const;

  private:
    void finish(bool taken, UpdateContext &context);

    bool open_ = false;
    TextQuestion question_;
    std::string text_;
    std::function<void(bool, std::string)> answered_;
    ui::Keyboard keys_;
    tween::Spring amount_;
    ui::Pulse nudge_;
    const Look *worn_ = nullptr;
};

} // namespace afterglow
