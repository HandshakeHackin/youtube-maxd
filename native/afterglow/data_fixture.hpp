// ProsperoTube Afterglow - the data layer's source for tests: answers from files.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A fixture set is a directory:
//   profile.json            {"id":"guest","name":"","guest":true,"signed_in":false}
//   catalog/<name>.json     one page of a catalog in the form of data_rows.hpp;
//                           <name> is the catalog id with ':' written as '_',
//                           "root" for the main menu, and ".pN" appended for page N > 0
// A catalog without a file answers Load::failed. Answers arrive a few pumps
// after they were asked for, as they do in the app, so loading states are seen.
// Host only: not compiled into the app.

#pragma once

#include "afterglow/data.hpp"

#include <string>
#include <vector>

namespace afterglow
{

class FixtureSource final : public Source
{
  public:
    explicit FixtureSource(std::string directory);

    // Pumps between a request and its answer (default 3).
    void set_delay(int pumps)
    {
        delay_ = pumps;
    }
    // While held, nothing is answered: the loading states stay up.
    void set_hold(bool hold)
    {
        hold_ = hold;
    }
    // Every catalog answers Load::offline, as when the service cannot be reached.
    void set_offline(bool offline)
    {
        offline_ = offline;
    }
    // Another set (another profile): bumps the revision.
    void set_directory(std::string directory);
    // Who is browsing, whatever the set's profile.json says: a board that shows
    // a set's catalogs to somebody else (a signed-in person with nothing yet).
    void set_profile(const model::Profile &profile)
    {
        profile_ = profile;
        profile_known_ = true;
    }
    // Videos handed to play(), oldest first.
    const std::vector<std::string> &played() const
    {
        return played_;
    }
    // Catalogs asked for, in order ("history", "library:home", ...).
    const std::vector<std::string> &asked() const
    {
        return asked_;
    }

    // Texts handed over with answer(), oldest first, each as "<row>=<text>"
    // ("prompt:answer=<text>" for the armed prompt): what a board checks.
    const std::vector<std::string> &answered() const
    {
        return answered_;
    }

    unsigned catalog(const std::string &id, int page, Done done) override;
    // A typed answer is the fixture catalog of the row (its rows are what the
    // provider lists afterwards); the armed prompt's is "prompt:answer".
    unsigned answer(const std::string &row, std::string_view text, Done done) override;
    void cancel(unsigned ticket) override;
    model::Profile profile() override;
    unsigned revision() override
    {
        return revision_;
    }
    bool play(const model::Video &video) override;
    // A row's actions menu is the fixture catalog "menu:<row>".
    std::string menu(const std::string &row, const std::string &title) override
    {
        (void)title;
        return "menu:" + row;
    }
    void pump() override;
    int pending() const override
    {
        return static_cast<int>(waiting_.size());
    }

    // The file name of a catalog page inside a set ("catalog/library_home.json").
    static std::string file_of(const std::string &id, int page);

  private:
    struct Waiting
    {
        unsigned ticket;
        int pumps_left;
        std::string id;
        int page;
        Done done;
    };
    std::string directory_;
    model::Profile profile_; // read once per set: the rail asks every frame
    bool profile_known_ = false;
    std::vector<Waiting> waiting_;
    std::vector<std::string> played_;
    std::vector<std::string> asked_;
    std::vector<std::string> answered_;
    unsigned next_ticket_ = 1;
    unsigned revision_ = 1;
    int delay_ = 3;
    bool hold_ = false;
    bool offline_ = false;
};

} // namespace afterglow
