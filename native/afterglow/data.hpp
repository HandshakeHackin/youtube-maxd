// ProsperoTube Afterglow - the data layer: provider and service answers as view models.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Screens read view models from Data and never talk to the provider, the
// service or the network. Data asks a Source: the app's is ProviderSource
// (data_provider.cpp, the shipping YouTube provider and its job queue), the
// tests' is FixtureSource (data_fixture.cpp, JSON files). Both build their
// answers with the same functions (data_rows.cpp), so a fixture is the
// provider's own answer written down.
//
// Reading is polling: a screen calls data.home() or data.catalog(id) every
// frame it needs them and looks at the Load state. The first call asks, later
// calls return what has arrived. Nothing is delivered by callback to a screen.

#pragma once

#include "afterglow/models.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace afterglow
{

namespace model
{

// One row of a provider catalog that is neither a video nor a channel nor a
// topic: a folder, a menu entry, a preference, a notice.
struct Row
{
    std::string id;
    std::string title;    // interface rows: already in the profile's language
    std::string subtitle;
    std::string overview;
    std::string thumb;
    bool folder = false;
    bool interface_text = false; // the provider wrote it (a menu), not YouTube
    // Everything else the provider knows about the row, by name: a comment's
    // "likes", a channel's "subscribers", a preference's "value". The names a
    // catalog gives are listed in docs/afterglow/ARCHITECTURE.md.
    std::vector<std::pair<std::string, std::string>> facts;

    // The fact of a name, "" when the row has none.
    const std::string &fact(std::string_view name) const
    {
        static const std::string kNone;
        for (const auto &entry : facts)
        {
            if (entry.first == name)
                return entry.second;
        }
        return kNone;
    }
    bool has(std::string_view name) const
    {
        const std::string &value = fact(name);
        return !value.empty() && value != "0" && value != "false";
    }
};

} // namespace model

// One page of a provider catalog, sorted into what its rows are.
struct Page
{
    model::Load load = model::Load::idle;
    std::string error;               // Load::failed: the provider's message, in the profile's language
    std::vector<model::Row> rows;    // every row in order, whatever it is
    std::vector<model::Video> videos;     // the rows that are videos, in order
    std::vector<model::Channel> channels; // ... channels
    std::vector<model::Topic> topics;     // ... topics and sections
    bool more = false;               // a further page exists
};

// What the data layer asks of the app.
class Source
{
  public:
    virtual ~Source() = default;

    using Done = std::function<void(Page &&)>;
    // One page of a catalog ("" is the main menu, "history", "library:home",
    // "section:trending", "discover", "settings", ...; the ids are the
    // provider's). done runs exactly once, during a later pump(), with
    // Load::ready, failed or offline. Returns a ticket; 0 when the request
    // could not be queued (done is then never called).
    virtual unsigned catalog(const std::string &id, int page, Done done) = 0;
    virtual void cancel(unsigned ticket) = 0;

    // The profile that is browsing. Immediate: local state.
    virtual model::Profile profile() = 0;
    // Changes when cached pages may be stale: another profile, an account
    // signed in or out, the library edited, the content language changed.
    virtual unsigned revision() = 0;

    // Hands a video to the app's player. False when the app refused.
    virtual bool play(const model::Video &video) = 0;
    // The catalog that lists what can be done with a row (the video actions
    // menu, a channel's, a playlist's); "" when the row has none.
    virtual std::string menu(const std::string &row, const std::string &title) = 0;

    // Delivers finished requests. Once per frame.
    virtual void pump() = 0;
    // Requests asked and not yet answered.
    virtual int pending() const = 0;
};

class Data
{
  public:
    explicit Data(Source &source);
    ~Data();
    Data(const Data &) = delete;
    Data &operator=(const Data &) = delete;

    // Once per frame, before the screens update.
    void pump();

    // Home as the profile sees it. Shelves appear in display order at once,
    // each with its own Load state, and fill as their answers arrive.
    const model::Home &home();
    // One page of a catalog, kept until the source's revision changes.
    const Page &catalog(const std::string &id, int page = 0);
    // Forgets everything that was fetched; the next read asks again.
    void refresh();
    // Forgets one catalog (every page of it): after an action changed it.
    void forget(const std::string &id);

    // Actions. Much of what a person does is a row of the provider that acts
    // when it is listed: a choice of a preference ("settings:quality:3"), an
    // entry of an actions menu ("account:confirm:like:<video>"), a local one
    // ("local:video:..."). act() lists such a row once, never from the cache,
    // and returns a ticket; action(ticket) is its state: loading until the
    // provider answered, then ready (rows are what the provider listed in
    // answer: the menu again, or a notice) or failed with the provider's
    // message. A finished action is kept for ten seconds. Show the pending
    // state on the control that was pressed, the result as a toast.
    unsigned act(const std::string &row);
    const Page &action(unsigned ticket);
    // The actions menu of a row, as a catalog id for catalog(); "" for none.
    std::string menu(const std::string &row, const std::string &title)
    {
        return source_.menu(row, title);
    }

    model::Profile profile()
    {
        return source_.profile();
    }
    bool play(const model::Video &video)
    {
        return source_.play(video);
    }
    int pending() const
    {
        return source_.pending();
    }

  private:
    struct Slot;
    Slot &slot(const std::string &id, int page);
    void build_home();

    Source &source_;
    std::map<std::string, std::unique_ptr<Slot>> slots_;
    struct Action;
    std::map<unsigned, std::unique_ptr<Action>> actions_;
    unsigned next_action_ = 1;
    model::Home home_;
    bool home_built_ = false;
    unsigned revision_ = 0;
    unsigned language_ = 0;
    std::shared_ptr<int> alive_;
};

} // namespace afterglow
