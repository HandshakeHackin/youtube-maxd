// ProsperoTube Afterglow - a catalog read page by page, and an action on its way.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Two small things nearly every screen needs on top of Data: the pages of one
// catalog as one growing list (Feed), and an acting row followed until the
// provider has answered (Act). Both are polled, as Data is: a screen calls
// read() or poll() every frame it is up.

#pragma once

#include "afterglow/data.hpp"

#include <string>
#include <vector>

namespace afterglow
{

// Rows of several pages in one list, each video and channel with the way back
// to its row (the row keeps every fact the view model does not carry).
struct Lists
{
    std::vector<model::Row> rows;
    std::vector<model::Video> videos;
    std::vector<model::Channel> channels;
    std::vector<int> video_row;   // rows[video_row[i]] is the row of videos[i]
    std::vector<int> channel_row; // ... of channels[i]

    const model::Row *row_of_video(std::size_t index) const
    {
        return index < video_row.size() ? &rows[static_cast<std::size_t>(video_row[index])] : nullptr;
    }
    const model::Row *row_of_channel(std::size_t index) const
    {
        return index < channel_row.size() ? &rows[static_cast<std::size_t>(channel_row[index])] : nullptr;
    }
};

// The pages of one catalog as one list. A screen calls read() every frame it is
// up: the first call asks for the first page, want_more() for the next one, and
// what has arrived is copied once. When the data layer forgets the catalog (a
// refresh, an action that changed it) what was read stays on screen until
// every page that was shown has arrived again; the focus is the screen's and
// stays where it was.
struct Feed : Lists
{
    std::string catalog;
    bool open = false;
    model::Load load = model::Load::idle; // of the first page; ready while a list that was read is read again
    std::string error;
    bool more = false;        // a further page exists
    bool more_failed = false; // ... and could not be read
    int wanted = 1;
    int taken = 0;
    unsigned version = 0; // changes whenever anything above does

    void start(std::string id);
    // Copies the pages again at the next read: the screen was covered and the answers may be others.
    void restart();
    void want_more()
    {
        if (more && !more_failed && !again_ && taken == wanted)
            ++wanted;
    }
    bool fetching_more() const
    {
        return !again_ && taken > 0 && taken < wanted && !more_failed;
    }
    // True when the list or its state changed.
    bool read(Data &data);

  private:
    Lists incoming_;     // the pages of a list that is read again, until all of them are there
    bool again_ = false; // what is on screen was read before the data layer forgot it
};

// An account or library action on its way (ARCHITECTURE.md 5.6): the acting
// row is listed once and, where the provider answers with the row that applies
// the change ("account:apply:<n>", its review step in the classic interface),
// that row is listed too. The screen has asked the person already where the
// change deserves a question.
struct Act
{
    unsigned ticket = 0;
    bool running = false;
    bool applying = false;

    void start(Data &data, const std::string &row)
    {
        ticket = data.act(row);
        running = true;
        applying = false;
    }
    // The same for a row that takes a typed text (Data::act(row, text)).
    void start(Data &data, const std::string &row, std::string_view text)
    {
        ticket = data.act(row, text);
        running = true;
        applying = false;
    }
    // Load::loading while it runs; ready, failed or offline once, when it ends; idle otherwise.
    // answer is the page the provider listed, valid until the next call into the data layer.
    model::Load poll(Data &data, const Page **answer = nullptr);
};

} // namespace afterglow
