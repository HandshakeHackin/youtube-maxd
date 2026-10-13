// ProsperoTube Afterglow - the data layer's source for tests: answers from files.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/data_fixture.hpp"

#include "afterglow/data_rows.hpp"

#include <cstdio>

namespace afterglow
{

namespace
{

bool read_file(const std::string &path, std::string *out)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;
    char buffer[65536];
    std::size_t got = 0;
    out->clear();
    while ((got = std::fread(buffer, 1, sizeof buffer, file)) > 0)
        out->append(buffer, got);
    std::fclose(file);
    return true;
}

} // namespace

FixtureSource::FixtureSource(std::string directory) : directory_(std::move(directory)) {}

void FixtureSource::set_directory(std::string directory)
{
    directory_ = std::move(directory);
    profile_known_ = false;
    ++revision_;
}

std::string FixtureSource::file_of(const std::string &id, int page)
{
    std::string name = id.empty() ? std::string("root") : id;
    for (char &c : name)
    {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!plain)
            c = '_';
    }
    if (page > 0)
        name += ".p" + std::to_string(page);
    return "catalog/" + name + ".json";
}

unsigned FixtureSource::catalog(const std::string &id, int page, Done done)
{
    asked_.push_back(id);
    const unsigned ticket = next_ticket_++;
    waiting_.push_back({ticket, delay_, id, page, std::move(done)});
    return ticket;
}

unsigned FixtureSource::answer(const std::string &row, std::string_view text, Done done)
{
    const std::string id = row.empty() ? std::string("prompt:answer") : row;
    answered_.push_back(id + "=" + std::string(text));
    return catalog(id, 0, std::move(done));
}

void FixtureSource::cancel(unsigned ticket)
{
    for (auto it = waiting_.begin(); it != waiting_.end(); ++it)
    {
        if (it->ticket == ticket)
        {
            waiting_.erase(it);
            return;
        }
    }
}

model::Profile FixtureSource::profile()
{
    if (!profile_known_)
    {
        std::string json;
        profile_ = model::Profile{};
        if (!read_file(directory_ + "/profile.json", &json) || !profile_from_json(json, &profile_))
            profile_.id = "guest";
        profile_known_ = true;
    }
    return profile_;
}

bool FixtureSource::play(const model::Video &video)
{
    played_.push_back(video.id);
    start_left_ = start_pumps_;
    start_failed_ = false;
    return true;
}

Source::Start FixtureSource::play_state(std::string *reason)
{
    if (start_left_ > 0)
        return Start::starting;
    if (!start_failed_)
        return Start::idle;
    start_failed_ = false; // said once
    if (reason != nullptr)
        *reason = start_failure_;
    return Start::failed;
}

bool FixtureSource::cancel_play()
{
    if (start_left_ <= 0 || !start_cancellable_)
        return false;
    start_left_ = 0;
    ++cancelled_;
    return true;
}

void FixtureSource::pump()
{
    if (start_left_ > 0 && --start_left_ == 0)
        start_failed_ = start_fails_;
    if (hold_)
        return;
    // One answer per pump, oldest first: the app's provider answers one request at a time too.
    for (Waiting &entry : waiting_)
        --entry.pumps_left;
    if (waiting_.empty() || waiting_.front().pumps_left > 0)
        return;
    Waiting entry = std::move(waiting_.front());
    waiting_.erase(waiting_.begin());
    Page page;
    std::string json;
    if (offline_)
    {
        page.load = model::Load::offline;
        page.error = "The YouTube service cannot be reached.";
    }
    else if (!read_file(directory_ + "/" + file_of(entry.id, entry.page), &json))
    {
        page.load = model::Load::failed;
        page.error = "No fixture for " + (entry.id.empty() ? std::string("the main menu") : entry.id);
    }
    else
    {
        page_from_json(json, &page);
    }
    entry.done(std::move(page));
}

} // namespace afterglow
