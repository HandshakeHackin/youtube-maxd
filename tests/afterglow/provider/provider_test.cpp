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
#include "afterglow/app/play_start.hpp"

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

// A plain search as the built-in service answers it (standalone/js/catalog.js, item): no facts, the
// picture as YouTube's search names it (a signed query after .jpg) or the default one.
const char *kSearch =
    "{\"state\":\"done\",\"result\":{\"api_version\":1,\"items\":["
    "{\"id\":\"bbbbbbbbbb1\",\"title\":\"How Tides Work\",\"subtitle\":\"Orbitwell\",\"overview\":\"\","
    "\"art_url\":\"https://i.ytimg.com/vi/bbbbbbbbbb1/hq720.jpg?sqp=-oaymwEnCNAFEJQDSFryq4qpAxkIARUAAIhCGAHYAQHiAQoIGBACGAY4AUAB&rs=AOn4CLfixture\","
    "\"channel_id\":\"UCcccccccccccccccccccccc\",\"duration_sec\":611,\"is_folder\":false,\"is_live\":false,\"is_short\":false,"
    "\"resume_pos_sec\":0,\"played\":false,\"watch_percent\":-1,\"playback_context\":null},"
    "{\"id\":\"bbbbbbbbbb2\",\"title\":\"The Deep Field\",\"subtitle\":\"Northlight Atlas\",\"overview\":\"\","
    "\"art_url\":\"https://i.ytimg.com/vi/bbbbbbbbbb2/hqdefault.jpg\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":1290,"
    "\"is_folder\":false,\"is_live\":false,\"is_short\":false,\"resume_pos_sec\":0,\"played\":false,\"watch_percent\":-1,\"playback_context\":null}"
    "],\"has_more\":false,\"page\":0}}";

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

        // A topic that is a search ("Browse by topic" > Science): the service's rows of a search carry the
        // picture under an address with a signed query, and that address reaches the card whole.
        const Page *science = &data.catalog("search:science");
        for (int i = 0; i < 12 && science->load == model::Load::loading; ++i)
        {
            data.pump();
            serve(kSearch);
            science = &data.catalog("search:science");
        }
        if (science->load != model::Load::ready || science->videos.size() != 2)
            std::fprintf(stderr, "search: load %d, %zu videos, error %s\n", static_cast<int>(science->load), science->videos.size(), science->error.c_str());
        assert(science->load == model::Load::ready && science->videos.size() == 2);
        assert(science->videos[0].thumb == "https://i.ytimg.com/vi/bbbbbbbbbb1/hq720.jpg?sqp=-oaymwEnCNAFEJQDSFryq4qpAxkIARUAAIhCGAHYAQHiAQoIGBACGAY4AUAB&rs=AOn4CLfixture");
        assert(science->videos[1].thumb == "https://i.ytimg.com/vi/bbbbbbbbbb2/hqdefault.jpg" && science->videos[0].channel == "Orbitwell");
        for (const model::Video &video : science->videos)
            assert(science->rows[static_cast<std::size_t>(video.at)].thumb == video.thumb && tube_art_url_allowed(video.thumb.c_str()));

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

        // A typed answer: the row that asks for a text is listed (which arms the provider's prompt), the
        // text is handed to that prompt, and the catalog the provider names next is the answer.
        const auto typed = [&](const char *row, const char *text)
        {
            Page answer;
            bool done = false;
            assert(source.answer(row, text, [&](Page &&page) { answer = std::move(page); done = true; }) != 0);
            for (int i = 0; i < 12 && !done; ++i)
            {
                source.pump();
                serve(kTrending);
            }
            assert(done && source.pending() == 0);
            return answer;
        };
        // This build has no built-in service and no account, so no row arms a prompt here: what is
        // proven is that the row is listed first, that a text nobody asked for is refused in words, and
        // that the queue is free again afterwards. The accepted text is proven with fixtures only
        // (tests/afterglow/boards) and waits for a console session.
        const Page unarmed = typed("settings:sb:server", "https://sponsor.example.org");
        assert(unarmed.load == model::Load::failed && !unarmed.error.empty() && unarmed.rows.empty() && unarmed.listed.empty());
        const Page nothing = typed("", "a text nobody asked for");
        assert(nothing.load == model::Load::failed && nothing.error == unarmed.error);
        assert(source.answer(std::string(400, 'x'), "text", [](Page &&) {}) == 0); // a row the provider cannot take
        // A listing after a refused text is an ordinary one again.
        bool after = false;
        assert(source.catalog("", 0, [&](Page &&page) { after = page.load == model::Load::ready && page.listed.empty(); }) != 0);
        for (int i = 0; i < 12 && !after; ++i)
        {
            source.pump();
            serve(kTrending);
        }
        assert(after && source.pending() == 0);
    }
    {
        // A chosen video until the player has the screen (app/play_start.hpp), from what the provider
        // screen tells: pending or not, whether Circle can still call it back, and its status line.
        using Start = Source::Start;
        std::string reason = "untouched";
        PlayStart start;
        assert(start.state(&reason, 0.0) == Start::idle && !start.step(0.0) && !start.cancel() && !start.take_cancel());
        // Resolved, opened, and the player takes the screen: starting all the way, no failure, nothing to say.
        start.busy(true, true); // told about an earlier video: it does not count for the next
        start.asked(10.0);
        assert(start.starting() && !start.step(10.0) && start.state(&reason, 10.0) == Start::starting);
        start.busy(false, false); // the frame the video was chosen in: the provider screen has not taken it yet
        assert(!start.step(10.02) && start.starting());
        start.busy(true, true);
        assert(!start.step(10.04));
        start.busy(true, false); // the stream is being opened
        assert(!start.step(14.0) && start.state(&reason, 14.0) == Start::starting && !start.cancel());
        start.reset(); // the player had the screen
        assert(start.state(&reason, 60.0) == Start::idle && reason == "untouched" && !start.failed_recently(60.0));
        // The service refuses the video: the status line's reason, said once; the app's notice stays away for a while.
        start.asked(100.0);
        start.busy(true, true);
        assert(!start.step(100.1));
        start.status("This YouTube video is unavailable or has been removed. Choose another video.", true);
        start.busy(false, false);
        assert(!start.step(101.0) && !start.starting());
        assert(start.state(&reason, 101.0) == Start::failed && reason == "This YouTube video is unavailable or has been removed. Choose another video.");
        assert(start.state(&reason, 101.1) == Start::idle && start.failed_recently(103.0) && !start.failed_recently(101.0 + PlayStart::kQuiet + 0.1));
        start.status("Could not load the catalog", true); // nothing is starting: not a start's reason
        // The provider screen never takes the video: a failure without a reason, after the grace.
        start.asked(200.0);
        start.busy(false, false);
        assert(!start.step(200.5) && start.starting() && !start.step(200.0 + PlayStart::kGrace + 0.1));
        reason = "untouched";
        assert(start.state(&reason, 202.0) == Start::failed && reason.empty());
        // Circle while the video is resolved calls the start back, once; the provider screen takes it.
        start.asked(300.0);
        assert(!start.cancel()); // not yet known to be pending
        start.busy(true, true);
        assert(!start.step(300.1) && start.cancel() && !start.starting() && start.take_cancel() && !start.take_cancel());
        start.busy(false, false);
        assert(!start.step(300.2) && start.state(&reason, 300.2) == Start::idle);
        // Too late for Circle once the stream is being opened: the start goes on.
        start.asked(400.0);
        start.busy(true, false);
        assert(!start.step(400.1) && !start.cancel() && start.starting() && !start.take_cancel());
        // It ends without the player and without a failure (the app says what follows in a notice of its
        // own, as for the next video of a queue): the page returns as after the player.
        start.status("Video unavailable. Next queued video in 5 seconds; any button cancels.", false);
        start.busy(false, false);
        assert(start.step(400.5) && start.state(&reason, 400.5) == Start::idle);
    }
    p->shutdown();
    std::puts("PASS: Afterglow data on the provider: Guest Home from the provider's catalogs one request at a time, row facts in the view "
              "models, a failed job, a cancelled request, a typed text with no prompt armed, the browsing profile");
    return 0;
}
