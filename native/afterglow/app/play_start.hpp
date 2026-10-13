// ProsperoTube Afterglow - how the start of a video the interface handed to the app's player stands.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player the classic browser uses takes the screen only when the stream
// is open: the provider screen first resolves the video (a request to the
// service) and then opens the stream on a worker thread. All that time the
// interface is still what is drawn, and it keeps the player's screen up
// (Frame, Source::play_state). This is what the mount knows about it, from
// three things the provider screen tells (afterglow.h): whether a start is
// pending and can still be called back, and its status line.
//
// No EVO type and no clock of its own: the mount gives the time, and
// tests/afterglow/provider/provider_test.cpp runs it as it is.

#pragma once

#include "afterglow/data.hpp"

#include <string>

namespace afterglow
{

class PlayStart
{
  public:
    // Seconds the provider screen has to say that it took the video.
    static constexpr double kGrace = 1.5;
    // Seconds after a failure the interface announced during which the app's own notice of it stays away.
    static constexpr double kQuiet = 6.0;

    // The interface handed a video over.
    void asked(double now)
    {
        state_ = Source::Start::starting;
        seen_ = false;
        pending_ = false; // what was told before belongs to an earlier video
        cancellable_ = false;
        cancel_ = false;
        asked_at_ = now;
        status_.clear();
        error_ = false;
    }
    // The player has the screen, or had it: nothing is starting any more.
    void reset()
    {
        state_ = Source::Start::idle;
        pending_ = false;
        cancel_ = false;
    }
    // The provider screen, every frame it draws.
    void busy(bool pending, bool cancellable)
    {
        pending_ = pending;
        cancellable_ = cancellable;
    }
    // The provider screen's status line.
    void status(const char *text, bool error)
    {
        if (state_ != Source::Start::starting)
            return;
        status_ = text != nullptr ? text : "";
        error_ = error;
    }
    // Once per frame the interface is drawn, after busy(). True when the start ended without the
    // player and without a failure (something else called it back, or a notice of the app says what
    // follows): the page returns as it does after the player.
    bool step(double now)
    {
        if (state_ != Source::Start::starting)
            return false;
        if (pending_)
        {
            seen_ = true;
            return false;
        }
        if (!seen_ && now - asked_at_ < kGrace)
            return false; // the provider screen takes the video with its next input
        if (seen_ && !(error_ && !status_.empty()))
        {
            state_ = Source::Start::idle;
            return true;
        }
        state_ = Source::Start::failed; // with the status line's reason, or never taken at all
        reason_ = error_ ? status_ : std::string();
        return false;
    }
    // Source::play_state: a failure is said once.
    Source::Start state(std::string *reason, double now)
    {
        if (state_ != Source::Start::failed)
            return state_;
        if (reason != nullptr)
            *reason = reason_;
        state_ = Source::Start::idle;
        failed_at_ = now;
        announced_ = true;
        return Source::Start::failed;
    }
    // Source::cancel_play: Circle. True when the provider screen can still call the start back; it
    // then does so on its next input (take_cancel).
    bool cancel()
    {
        if (state_ != Source::Start::starting || !seen_ || !pending_ || !cancellable_)
            return false;
        state_ = Source::Start::idle;
        cancel_ = true;
        return true;
    }
    bool take_cancel()
    {
        const bool wanted = cancel_;
        cancel_ = false;
        return wanted;
    }
    bool starting() const
    {
        return state_ == Source::Start::starting;
    }
    bool failed_recently(double now) const
    {
        return announced_ && now - failed_at_ < kQuiet;
    }

  private:
    Source::Start state_ = Source::Start::idle;
    bool seen_ = false; // the provider screen said the start is pending
    bool pending_ = false;
    bool cancellable_ = false;
    bool cancel_ = false;
    bool error_ = false;
    bool announced_ = false;
    double asked_at_ = 0.0;
    double failed_at_ = 0.0;
    std::string status_;
    std::string reason_;
};

} // namespace afterglow
