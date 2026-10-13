// ProsperoTube Afterglow - the data layer's source in the app: the shipping YouTube provider.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Catalog pages come from evo_provider_youtube.list_catalog, exactly as the
// classic browser gets them, so the profile's local state is already applied.
// One request is in flight at a time and the rest wait in the order they were
// asked: the provider's paging state belongs to one list at a time. Facts
// (tube_row_fact_*) are read in the provider's callback, while they are those
// of the page that just arrived. App only; the tests of the provider link it
// too (tests/afterglow/provider_test.cpp).

#pragma once

#include "afterglow/data.hpp"

#include <deque>
#include <functional>
#include <string>

struct evo_provider_item;

namespace afterglow
{

class ProviderSource final : public Source
{
  public:
    ProviderSource();
    ~ProviderSource() override;

    // What play() does: the app hands the row to its player. Without one,
    // play() refuses.
    void set_player(std::function<bool(const model::Video &)> player)
    {
        player_ = std::move(player);
    }

    unsigned catalog(const std::string &id, int page, Done done) override;
    // A typed answer: `row` is listed (it arms the provider's text prompt; "" when a listing before
    // armed it), the text is handed to that prompt and the catalog the provider names is listed.
    unsigned answer(const std::string &row, std::string_view text, Done done) override;
    void cancel(unsigned ticket) override;
    model::Profile profile() override;
    unsigned revision() override;
    bool play(const model::Video &video) override;
    std::string menu(const std::string &row, const std::string &title) override;
    void pump() override;
    int pending() const override
    {
        return static_cast<int>(waiting_.size() + finished_.size()) + (flying_ != 0 ? 1 : 0);
    }

  private:
    struct Request
    {
        unsigned ticket = 0;
        std::string id;
        int page = 0;
        Done done;
        bool typed = false; // an answer(): text goes to the prompt the listing of id arms
        std::string text;
    };
    struct Finished
    {
        unsigned ticket = 0;
        Done done;
        Page page;
    };
    static void answered(int ok, const ::evo_provider_item *items, int count, int more, void *self);
    void start_next();
    void list(const std::string &id, int page);
    void hand_over_text();
    void finish(Page &&page);

    std::deque<Request> waiting_;
    std::deque<Finished> finished_;
    unsigned flying_ = 0;      // the ticket of the request the provider is working on
    bool flying_dropped_ = false; // ... and whether it was cancelled meanwhile
    Done flying_done_;
    // A typed answer on its way: 0 none, 1 the row that arms the prompt is being listed, 2 the prompt
    // is armed and waits for the text, 3 the catalog the provider named is being listed.
    int typed_stage_ = 0;
    std::string typed_text_;
    std::string typed_listed_; // the catalog the provider named after the text
    unsigned next_ticket_ = 1;
    std::function<bool(const model::Video &)> player_;
    std::string last_profile_;
    unsigned profile_changes_ = 0;
};

} // namespace afterglow
