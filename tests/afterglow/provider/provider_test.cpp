// ProsperoTube Afterglow - the data layer on the shipping provider, against canned service answers.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ProviderSource and Data run on the real native/tube_provider.c. The network
// is a queue of requests this file answers with documents written below, the
// way tests/test_native.c does; nothing leaves the process. Covered: Guest
// Home assembles from the provider's catalogs, one request at a time; a
// service row's facts reach the view model; a failed job and a cancelled
// request; the browsing profile and the revision.

#include "afterglow/app/data_provider.hpp"

extern "C"
{
#include "evo_data_path.h"
#include "evo_net.h"
#include "tube_provider.h"
}

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

struct Pending
{
    std::string method, url, body;
    evo_net_cb cb;
    void *ud;
};
std::vector<Pending> g_pending;
std::string g_error;
int g_jobs_posted = 0;
int g_most_jobs_at_once = 0;

void reply(std::size_t index, int ok, int status, const std::string &body)
{
    assert(index < g_pending.size());
    const Pending p = g_pending[index];
    g_pending.erase(g_pending.begin() + static_cast<std::ptrdiff_t>(index));
    p.cb(ok, status, body.c_str(), body.size() + 1, p.ud);
}

// Trending as the built-in service answers it, with the facts Afterglow reads.
const char *kTrending =
    "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"aaaaaaaaaa1\",\"title\":\"Seven Days Alone on the Ridge Line\",\"subtitle\":\"Northlight Atlas\","
    "\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":2892,\"overview\":\"A silent hiking film.\","
    "\"art_url\":\"https://i.ytimg.com/vi/aaaaaaaaaa1/hq720.jpg\",\"views\":\"1.2M views\",\"published\":\"3 weeks ago\","
    "\"channel_art_url\":\"https://yt3.ggpht.com/fixture-avatar=s88\",\"badges\":[\"4k\",\"cc\"]},"
    "{\"id\":\"aaaaaaaaaa2\",\"title\":\"Synth Session, Live from the Harbour\",\"subtitle\":\"Nacre Sound\","
    "\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"is_live\":true,\"views\":\"4.1K watching\",\"badges\":[\"new\"]},"
    "{\"id\":\"aaaaaaaaaa3\",\"title\":\"What Would a Sunset Look Like on Another World?\",\"subtitle\":\"Orbitwell\","
    "\"channel_id\":\"UCcccccccccccccccccccccc\",\"duration_sec\":847,\"watch_percent\":40,\"is_short\":false}"
    "],\"has_more\":false}}";

// Answers whatever the provider has asked so far. job is the document a
// finished catalog job returns; "" leaves the job unanswered.
void serve(const char *job)
{
    for (int round = 0; round < 12 && !g_pending.empty(); ++round)
    {
        int jobs = 0;
        for (const Pending &p : g_pending)
            jobs += p.method == "POST" && p.url.find("/v1/jobs") != std::string::npos;
        g_most_jobs_at_once = jobs > g_most_jobs_at_once ? jobs : g_most_jobs_at_once;
        const Pending &p = g_pending[0];
        if (p.url.find("/v1/pair") != std::string::npos)
            reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
        else if (p.url.find("/v1/health") != std::string::npos)
            reply(0, 1, 404, "{}");
        else if (p.method == "POST" && p.url.find("/v1/jobs") != std::string::npos)
        {
            ++g_jobs_posted;
            reply(0, 1, 200, "{\"job\":\"fixture_job\",\"state\":\"pending\"}");
            usleep(300000);
            tube_poll();
        }
        else if (p.url.find("/v1/jobs/fixture_job") != std::string::npos)
        {
            if (*job == '\0')
                return;
            reply(0, 1, 200, job);
        }
        else
        {
            std::fprintf(stderr, "unexpected request: %s %s %s\n", p.method.c_str(), p.url.c_str(), p.body.c_str());
            assert(false);
        }
    }
}

const afterglow::model::Shelf *shelf(const afterglow::model::Home &home, const char *id)
{
    for (const afterglow::model::Shelf &s : home.shelves)
    {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

} // namespace

extern "C" int evo_net_request_async(const char *method, const char *url, const char *post, const char **headers,
                                     int header_count, evo_net_cb cb, void *ud)
{
    (void)headers;
    (void)header_count;
    g_pending.push_back({method, url, post != nullptr ? post : "", cb, ud});
    return 0;
}
extern "C" void evo_error_set(const char *fmt, ...)
{
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    g_error = text;
}
extern "C" int evo_error_take(char *out, size_t cap)
{
    snprintf(out, cap, "%s", g_error.c_str());
    const int had = !g_error.empty();
    g_error.clear();
    return had;
}

int main()
{
    using namespace afterglow;
    char dir[] = "/tmp/tube-afterglow-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    const evo_provider_t *p = &evo_provider_youtube;
    assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));

    // Facts are kept beside rows by id and name; the newest value of a name wins.
    tube_row_fact("row-one", "likes", "12");
    tube_row_fact("row-one", "likes", "13");
    tube_row_fact("row-one", "pinned", "1");
    tube_row_fact("row-one", "empty", "");
    const char *name = nullptr;
    const char *value = nullptr;
    assert(!std::strcmp(tube_row_fact_get("row-one", "likes"), "13") && !std::strcmp(tube_row_fact_get("row-one", "pinned"), "1"));
    assert(!*tube_row_fact_get("row-one", "empty") && !*tube_row_fact_get("row-two", "likes") && !*tube_row_fact_get(nullptr, "likes"));
    assert(tube_row_fact_at("row-one", 0, &name, &value) && tube_row_fact_at("row-one", 1, &name, &value) && !tube_row_fact_at("row-one", 2, &name, &value));

    {
        ProviderSource source;
        Data data(source);
        const model::Profile profile = source.profile();
        assert(profile.id == "guest" && profile.guest && !profile.signed_in && profile.name.empty());
        const unsigned revision = source.revision();

        // Guest Home: topics, continue watching, trending; each shelf with its own state.
        const model::Home *home = &data.home();
        assert(home->profile.guest && home->invite_sign_in && home->shelves.size() == 3 && home->load == model::Load::loading);
        assert(shelf(*home, "topics") && shelf(*home, "continue") && shelf(*home, "trending"));
        assert(shelf(*home, "trending")->load == model::Load::loading && shelf(*home, "trending")->catalog == "section:trending");
        for (int i = 0; i < 40; ++i)
        {
            data.pump();
            serve(kTrending);
            home = &data.home();
            if (home->load == model::Load::ready)
                break;
        }
        assert(home->load == model::Load::ready);
        assert(g_most_jobs_at_once <= 1); // the provider is asked one list at a time

        // The topics are the profile's Discover rows, as interface texts with their icons.
        const model::Shelf *topics = shelf(*home, "topics");
        assert(topics->load == model::Load::ready && topics->kind == model::ShelfKind::topics && topics->topics.size() >= 10);
        bool gaming = false;
        bool trending_topic = false;
        for (const model::Topic &topic : topics->topics)
        {
            gaming = gaming || (topic.id == "section:gaming" && topic.title == "Gaming" && topic.icon == "gamepad");
            trending_topic = trending_topic || (topic.id == "section:trending" && topic.icon == "trending");
        }
        assert(gaming && trending_topic);
        // Nothing was watched yet: Continue watching is ready and empty, which Home does not show.
        assert(shelf(*home, "continue")->load == model::Load::ready && shelf(*home, "continue")->videos.empty());
        // Trending: the service's rows, with the facts beside them.
        const model::Shelf *trending = shelf(*home, "trending");
        if (trending->videos.size() != 3)
            std::fprintf(stderr, "trending: load %d, %zu videos, more %d, error %s\n", static_cast<int>(trending->load), trending->videos.size(), trending->more ? 1 : 0, trending->error.c_str());
        assert(trending->load == model::Load::ready && trending->videos.size() == 3 && !trending->more);
        const model::Video &first = trending->videos[0];
        assert(first.id == "aaaaaaaaaa1" && first.video_id == "aaaaaaaaaa1" && first.title == "Seven Days Alone on the Ridge Line");
        assert(first.channel == "Northlight Atlas" && first.channel_id == "UCaaaaaaaaaaaaaaaaaaaaaa" && first.duration_s == 2892);
        assert(first.views == "1.2M views" && first.age == "3 weeks ago" && first.overview == "A silent hiking film.");
        assert(first.thumb == "https://i.ytimg.com/vi/aaaaaaaaaa1/hq720.jpg" && first.avatar == "https://yt3.ggpht.com/fixture-avatar=s88");
        assert((first.badges & model::kBadge4K) && (first.badges & model::kBadgeCaptions) && !(first.badges & model::kBadgeLive));
        const model::Video &live = trending->videos[1];
        assert((live.badges & model::kBadgeLive) && (live.badges & model::kBadgeNew) && live.duration_s == 0 && live.views == "4.1K watching");
        assert(trending->videos[2].progress > 0.39f && trending->videos[2].progress < 0.41f && !trending->videos[2].watched);
        assert(home->pinned.empty());
        for (int i = 0; i < 6 && source.pending() != 0; ++i)
        {
            data.pump(); // the main menu, asked for the rail's pinned channels, is the last answer
            serve(kTrending);
        }
        if (source.revision() != revision || source.pending() != 0)
            std::fprintf(stderr, "revision %u -> %u, pending %d\n", revision, source.revision(), source.pending());
        assert(source.revision() == revision && source.pending() == 0);

        // A catalog page read directly: the main menu's rows are interface rows.
        const Page *menu = &data.catalog("");
        for (int i = 0; i < 6 && menu->load == model::Load::loading; ++i)
        {
            data.pump();
            menu = &data.catalog("");
        }
        assert(menu->load == model::Load::ready && menu->rows.size() >= 5 && menu->videos.empty());
        bool settings = false;
        for (const model::Row &row : menu->rows)
            settings = settings || (row.id == "settings" && row.folder && row.interface_text);
        assert(settings);

        // A job that fails: the page says so in the provider's words; nothing is half-filled.
        const int before = g_jobs_posted;
        const Page *news = &data.catalog("section:news");
        for (int i = 0; i < 12 && news->load == model::Load::loading; ++i)
        {
            data.pump();
            serve("{\"state\":\"failed\",\"error\":\"fixture failure\"}");
            news = &data.catalog("section:news");
        }
        assert(g_jobs_posted == before + 1 && news->load == model::Load::failed && !news->error.empty() && news->videos.empty());

        // A request cancelled while the provider works on it: its answer is thrown away, the next one runs.
        bool cancelled_answered = false;
        bool next_answered = false;
        const unsigned ticket = source.catalog("section:music", 0, [&](Page &&) { cancelled_answered = true; });
        assert(ticket != 0 && source.catalog("", 0, [&](Page &&page) { next_answered = page.load == model::Load::ready; }) != 0);
        source.pump();
        source.cancel(ticket);
        for (int i = 0; i < 12 && !next_answered; ++i)
        {
            serve(kTrending);
            source.pump();
        }
        assert(!cancelled_answered && next_answered && source.pending() == 0);
        assert(source.catalog(std::string(400, 'x'), 0, [](Page &&) {}) == 0); // an id the provider cannot take
        assert(!source.play(model::Video{}));                                  // no player was given
    }
    p->shutdown();
    std::puts("PASS: Afterglow data on the provider: Guest Home from the provider's catalogs one request at a time, row facts in the view "
              "models, a failed job, a cancelled request, the browsing profile");
    return 0;
}
