// ProsperoTube Afterglow - the data layer on the shipping provider, against canned service answers.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ProviderSource and Data run on the real native/tube_provider.c. The network
// is a queue of requests this file answers with documents written below, the
// way tests/test_native.c does; nothing leaves the process. Covered: Guest
// Home assembles from the provider's catalogs, one request at a time and a
// row further down only when the page reaches it; a service row's facts reach
// the view model; a failed job and a cancelled request; the browsing profile
// and the revision; a search while it is typed (completions and matches for
// the text, a newer text taking the place of an older request, nothing kept
// as a search).

#include "afterglow/app/data_provider.hpp"
#include "afterglow/app/play_start.hpp"
#include "link_phone_probe.hpp"

extern "C"
{
#include "evo_data_path.h"
#include "evo_net.h"
#include "tube_provider.h"
#include "tube_remote.h"
#include "tube_phone.h"
#include "cJSON.h"
}

#include <algorithm>
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
std::vector<std::string> g_jobs;   // the body of every job posted; job n is named fixture_job_<n>
int g_jobs_deleted = 0;            // jobs the provider called back
std::string g_health;              // the feature list a health request is answered with; "" for none
// When set, the document a finished job returns is chosen by what the job asked.
const char *(*g_route)(const std::string &job) = nullptr;

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
            reply(0, 1, g_health.empty() ? 404 : 200, g_health.empty() ? "{}" : g_health);
        else if (p.method == "POST" && p.url.find("/v1/jobs") != std::string::npos)
        {
            ++g_jobs_posted;
            g_jobs.push_back(p.body);
            reply(0, 1, 200, "{\"job\":\"fixture_job_" + std::to_string(g_jobs.size() - 1) + "\",\"state\":\"pending\"}");
            usleep(300000);
            tube_poll();
        }
        else if (p.url.find("/v1/jobs/fixture_job_") != std::string::npos)
        {
            if (p.method == "DELETE")
            {
                ++g_jobs_deleted;
                reply(0, 1, 200, "{}"); // a job called back
                continue;
            }
            // A job whose document is "" is left unanswered: what was asked after it is served all the same.
            std::size_t held = 0;
            const char *document = "";
            for (; held < g_pending.size(); ++held)
            {
                const std::size_t at = g_pending[held].url.find("/v1/jobs/fixture_job_");
                if (g_pending[held].method != "GET" || at == std::string::npos)
                    break;
                const std::string &asked = g_jobs[static_cast<std::size_t>(std::atoi(g_pending[held].url.c_str() + at + 21))];
                document = g_route != nullptr ? g_route(asked) : job;
                if (*document != '\0')
                    break;
            }
            if (held == g_pending.size())
                return;
            if (*document != '\0')
                reply(held, 1, 200, document);
            else
                std::rotate(g_pending.begin(), g_pending.begin() + static_cast<std::ptrdiff_t>(held), g_pending.end());
        }
        else
        {
            std::fprintf(stderr, "unexpected request: %s %s %s\n", p.method.c_str(), p.url.c_str(), p.body.c_str());
            assert(false);
        }
    }
}

// Home without an account as the built-in service answers it (seen live on 2026-10-13): YouTube's
// Home is a notice and no videos, Trending is refused, a topic answers with the videos of its rows.
const char *kNothing = "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"catalog\",\"items\":[],\"has_more\":false,\"cursor\":\"\"}}";
const char *kRefused = "{\"state\":\"failed\",\"error\":{\"code\":\"catalog_unavailable\",\"message\":\"YouTube does not offer this section here.\"}}";
const char *kGaming =
    "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"ccccccccc01\",\"title\":\"A Whole City in One Weekend\",\"subtitle\":\"Glassfern Games\",\"channel_id\":\"UCdddddddddddddddddddddd\",\"duration_sec\":902},"
    "{\"id\":\"ccccccccc02\",\"title\":\"Speedrunning My Own Level\",\"subtitle\":\"Glassfern Games\",\"channel_id\":\"UCdddddddddddddddddddddd\",\"duration_sec\":411}"
    "],\"has_more\":false,\"cursor\":\"\"}}";
const char *guest_home(const std::string &job)
{
    const auto asks = [&](const char *text) { return job.find(text) != std::string::npos; };
    if (asks("\"action\":\"home\""))
        return kNothing;
    if (asks("\"action\":\"topic_videos\""))
        return asks("\"section\":\"music\"") || asks("\"section\":\"live\"") ? kTrending : asks("\"section\":\"gaming\"") || asks("\"section\":\"news\"") ? kGaming : kNothing;
    if (asks("\"action\":\"topics\"") && asks("\"section\":\"trending\""))
        return kRefused;
    return kTrending;
}
// A search while it is typed. The completions as the service's library action answers them, and the
// matches twice: the plain video search (a companion that did not say what it offers) and the library's
// cards (the built-in service: views and age beside the row, a channel and a playlist among the videos).
const char *kCompletions = "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"search_suggestions\",\"query\":\"lofi be\","
                           "\"suggestions\":[\"lofi beats\",\"lofi beats to study to\",\"Lofi Beats\",\"lofi beach\"]}}";
const char *kStale = "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"search_suggestions\",\"query\":\"lofi\",\"suggestions\":[\"lofi girl\"]}}";
const char *kCards =
    "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"catalog\",\"items\":["
    "{\"kind\":\"channel\",\"id\":\"channel:UCcccccccccccccccccccccc\",\"title\":\"Orbitwell\",\"subtitle\":\"3.9M subscribers\",\"is_folder\":true},"
    "{\"kind\":\"video\",\"id\":\"bbbbbbbbbb1\",\"title\":\"How Tides Work\",\"subtitle\":\"Orbitwell\",\"channel_id\":\"UCcccccccccccccccccccccc\","
    "\"duration_sec\":611,\"views\":\"58M views\",\"published\":\"4 years ago\",\"art_url\":\"https://i.ytimg.com/vi/bbbbbbbbbb1/hq720.jpg\"},"
    "{\"kind\":\"playlist\",\"id\":\"library:playlist:PLaaaaaaaaaaaaaaaa\",\"title\":\"Tides, a series\",\"subtitle\":\"Orbitwell\",\"is_folder\":true},"
    "{\"kind\":\"video\",\"id\":\"bbbbbbbbbb2\",\"title\":\"The Deep Field\",\"subtitle\":\"Northlight Atlas\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\","
    "\"duration_sec\":1290,\"views\":\"14M views\",\"published\":\"1 month ago\"}"
    "],\"has_more\":true,\"cursor\":\"cursor0123456789\"}}";
bool g_stale_answers = false;
const char *typing(const std::string &job)
{
    const auto asks = [&](const char *text) { return job.find(text) != std::string::npos; };
    if (asks("\"action\":\"search_suggestions\""))
        return asks("\"query\":\"lofi be\"") ? kCompletions : g_stale_answers ? kStale : ""; // "lofi" is not answered until later
    if (asks("\"action\":\"library\"") && asks("\"action\":\"search\""))
        return kCards;
    if (asks("\"action\":\"search\""))
        return kSearch;
    return kTrending;
}

bool job_asked(const char *text)
{
    for (const std::string &job : g_jobs)
    {
        if (job.find(text) != std::string::npos)
            return true;
    }
    return false;
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

extern "C" int evo_net_callback_error(void) { return 0; }

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

        // Guest Home begins with videos: what YouTube recommends to the session (a notice and nothing
        // else for a new Guest), then the topics' own videos. Each row is asked in its turn, and no
        // further down than the page reaches: two filled rows to begin with.
        g_route = guest_home;
        const model::Home *home = &data.home();
        assert(home->profile.guest && home->invite_sign_in && home->shelves.size() == 9 && home->load == model::Load::loading);
        const char *order[] = {"continue", "recommended", "music", "gaming", "topics", "trending", "news", "sports", "live"};
        for (std::size_t i = 0; i < 9; ++i)
            assert(home->shelves[i].id == order[i]);
        assert(shelf(*home, "music")->catalog == "home-topic:music" && shelf(*home, "music")->load == model::Load::idle);
        assert(shelf(*home, "recommended")->catalog == "library:home" && shelf(*home, "trending")->catalog == "section:trending");
        for (int i = 0; i < 60; ++i)
        {
            data.pump();
            serve("");
            home = &data.home();
            if (home->load == model::Load::ready)
                break;
        }
        assert(home->load == model::Load::ready);
        assert(g_most_jobs_at_once <= 1); // the provider is asked one list at a time
        // Three requests went out: Home, Music, Gaming. YouTube's Home had no video, so the first shelf with any is Music.
        if (g_jobs_posted != 3)
            std::fprintf(stderr, "guest home: %d jobs\n", g_jobs_posted);
        assert(g_jobs_posted == 3 && job_asked("\"action\":\"home\"") && job_asked("\"section\":\"music\"") && job_asked("\"section\":\"gaming\""));
        assert(!job_asked("\"section\":\"trending\"") && !job_asked("\"section\":\"news\"") && !job_asked("\"section\":\"sports\""));
        assert(shelf(*home, "recommended")->load == model::Load::ready && shelf(*home, "recommended")->videos.empty());
        assert(shelf(*home, "gaming")->load == model::Load::ready && shelf(*home, "gaming")->videos.size() == 2);
        assert(shelf(*home, "gaming")->videos[0].id == "ccccccccc01" && shelf(*home, "gaming")->videos[1].channel == "Glassfern Games");
        for (const char *later : {"topics", "trending", "news", "sports", "live"})
            assert(shelf(*home, later)->load == model::Load::idle && shelf(*home, later)->videos.empty() && shelf(*home, later)->topics.empty());
        // The page is walked down: the topics (the provider's own list, no request), then Trending, which
        // YouTube refuses a Guest (one notice row, no video: the shelf is not shown), then News. Sports waits.
        data.home_reach(4);
        for (int i = 0; i < 60; ++i)
        {
            data.pump();
            serve("");
            home = &data.home();
            if (home->load == model::Load::ready && shelf(*home, "news")->load == model::Load::ready)
                break;
        }
        assert(home->load == model::Load::ready && g_jobs_posted == 5 && g_most_jobs_at_once <= 1);
        assert(shelf(*home, "trending")->load == model::Load::ready && shelf(*home, "trending")->videos.empty());
        assert(shelf(*home, "news")->videos.size() == 2 && shelf(*home, "sports")->load == model::Load::idle && !job_asked("\"section\":\"sports\""));
        // A shelf of a topic has one page, and only the topics Home shows are known.
        const Page *second = &data.catalog("home-topic:music", 1);
        const Page *unknown = &data.catalog("home-topic:movies");
        for (int i = 0; i < 6 && (second->load == model::Load::loading || unknown->load == model::Load::loading); ++i)
        {
            data.pump();
            second = &data.catalog("home-topic:music", 1);
            unknown = &data.catalog("home-topic:movies");
        }
        assert(second->load == model::Load::failed && unknown->load == model::Load::failed && g_jobs_posted == 5);
        g_route = nullptr;

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
        // Music: the service's rows, with the facts beside them.
        home = &data.home();
        const model::Shelf *trending = shelf(*home, "music");
        if (trending->videos.size() != 3)
            std::fprintf(stderr, "music: load %d, %zu videos, more %d, error %s\n", static_cast<int>(trending->load), trending->videos.size(), trending->more ? 1 : 0, trending->error.c_str());
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

        // Live now is read only when reached, and archived videos in the topic stay out.
        assert(shelf(*home,"live")->load == model::Load::idle && !job_asked("\"section\":\"live\""));
        g_route = guest_home;
        data.home_reach(20);
        for (int i=0; i<60; ++i) {
            data.pump();serve("");home=&data.home();
            if(shelf(*home,"live")->load == model::Load::ready) break;
        }
        assert(job_asked("\"section\":\"live\"") && shelf(*home,"live")->videos.size()==1 &&
               (shelf(*home,"live")->videos[0].badges & model::kBadgeLive));
        g_route = nullptr;

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

        // A search while it is typed. "search-suggest:<text>" asks YouTube's completions of the text
        // through the service; the rows are YouTube's words (not interface texts). A key typed
        // meanwhile lets the older request go: its job is called back, the newer text is asked at
        // once, and the older answer, when it comes after all, reaches nobody.
        g_route = typing;
        const std::size_t jobs_before = g_jobs.size();
        const int deleted_before = g_jobs_deleted;
        const Page *older = &data.catalog("search-suggest:lofi");
        for (int i = 0; i < 4; ++i)
        {
            data.pump();
            serve("");
        }
        assert(older->load == model::Load::loading && source.pending() == 1 && g_jobs.size() == jobs_before + 1);
        assert(g_jobs.back().find("\"action\":\"search_suggestions\"") != std::string::npos && g_jobs.back().find("\"query\":\"lofi\"") != std::string::npos);
        data.forget("search-suggest:lofi"); // the text moved on
        assert(source.pending() == 0);
        const Page *completions = &data.catalog("search-suggest:lofi be");
        const Page *matches = &data.catalog("search-preview:lofi be");
        for (int i = 0; i < 40 && (completions->load == model::Load::loading || matches->load == model::Load::loading); ++i)
        {
            data.pump();
            serve("");
        }
        if (completions->load != model::Load::ready || matches->load != model::Load::ready)
            std::fprintf(stderr, "typing: completions %d (%s), matches %d (%s)\n", static_cast<int>(completions->load), completions->error.c_str(),
                         static_cast<int>(matches->load), matches->error.c_str());
        assert(g_jobs_deleted > deleted_before && g_most_jobs_at_once <= 1); // the older job was called back
        assert(completions->load == model::Load::ready && completions->rows.size() == 3 && completions->videos.empty());
        assert(completions->rows[0].title == "lofi beats" && completions->rows[1].title == "lofi beats to study to" && completions->rows[2].title == "lofi beach");
        for (const model::Row &row : completions->rows)
            assert(row.folder && !row.interface_text);
        // The matches: the first videos of a search for the text, without the two rows that lead a search a person ran.
        assert(matches->load == model::Load::ready && matches->videos.size() == 2 && matches->rows.size() == 2 && !matches->more);
        assert(matches->videos[0].title == "How Tides Work" && matches->videos[0].channel == "Orbitwell" && matches->videos[1].duration_s == 1290);
        assert(g_jobs.back().find("\"query\":\"lofi be\"") != std::string::npos && g_jobs.back().find("\"action\":\"search\"") != std::string::npos);
        // The answer to the older text arrives now: nothing changes, nothing is delivered.
        g_stale_answers = true;
        for (int i = 0; i < 4; ++i)
        {
            serve("");
            data.pump();
        }
        assert(g_pending.empty() && source.pending() == 0 && completions->rows.size() == 3 && completions->rows[0].title == "lofi beats");
        // No text, a second page and a text that is no query are refused in words, without a request.
        const std::size_t jobs_typed = g_jobs.size();
        for (const char *refused : {"search-suggest:", "search-preview:", "search-preview:   "})
        {
            const Page *page = &data.catalog(refused);
            for (int i = 0; i < 4 && page->load == model::Load::loading; ++i)
                data.pump();
            assert(page->load == model::Load::failed && !page->error.empty());
        }
        const Page *second_page = &data.catalog("search-preview:lofi be", 1);
        for (int i = 0; i < 4 && second_page->load == model::Load::loading; ++i)
            data.pump();
        assert(second_page->load == model::Load::failed && g_jobs.size() == jobs_typed);
        // Typing is not searching: the profile's recent searches hold none of it.
        const Page *recent = &data.catalog("search-history");
        for (int i = 0; i < 4 && recent->load == model::Load::loading; ++i)
            data.pump();
        assert(recent->load == model::Load::ready);
        for (const model::Row &row : recent->rows)
            assert(row.title.find("lofi") == std::string::npos);
        // A service that lists the library (the built-in one does) is asked for cards: the matches then
        // carry views and age, and a channel or a playlist among them stays out of the strip's videos.
        g_health = "{\"api_version\":1,\"mode\":\"youtube\",\"features\":[\"search\",\"library\"]}";
        const Page *kids = &data.catalog("section:kids"); // a list that reads what the service offers first
        for (int i = 0; i < 12 && kids->load == model::Load::loading; ++i)
        {
            data.pump();
            serve("");
        }
        assert(kids->load == model::Load::failed); // this service has no Kids; its feature list is known now
        const Page *cards = &data.catalog("search-preview:tides");
        for (int i = 0; i < 12 && cards->load == model::Load::loading; ++i)
        {
            data.pump();
            serve("");
        }
        if (cards->load != model::Load::ready || cards->videos.size() != 2)
            std::fprintf(stderr, "cards: load %d, %zu videos, %zu rows, error %s\n", static_cast<int>(cards->load), cards->videos.size(), cards->rows.size(), cards->error.c_str());
        assert(g_jobs.back().find("\"action\":\"library\"") != std::string::npos && g_jobs.back().find("\"query\":\"tides\"") != std::string::npos);
        assert(cards->load == model::Load::ready && cards->videos.size() == 2 && cards->channels.size() == 1 && cards->rows.size() == 4);
        assert(cards->videos[0].views == "58M views" && cards->videos[0].age == "4 years ago" && cards->videos[1].age == "1 month ago");
        g_route = nullptr;
        g_health.clear();
        assert(source.pending() == 0);
    }
    {
        // Real service profile rows must carry the facts Accounts and Library consume.
        ProviderSource source;
        Data data(source);
        const char *profiles = R"({"state":"done","result":{"profiles":[{"id":"guest","name":"Guest","account_configured":false},{"id":"oauth_0123456789abcdef","name":"My account","account_configured":true}]}})";
        const auto list_profiles = [&]() -> const Page & {
            data.forget("library:profiles");
            const Page *page = &data.catalog("library:profiles");
            for (int i = 0; i < 12 && page->load == model::Load::loading; ++i) { data.pump(); serve(profiles); }
            assert(page->load == model::Load::ready && page->rows.size() == 2);
            return *page;
        };
        const char *saved_profiles = profiles;
        profiles = R"({"state":"done","result":{"profiles":[{"id":"guest","name":"Guest","account_configured":false}]}})";
        const Page *before = &data.catalog("library:profiles");
        for (int i = 0; i < 12 && before->load == model::Load::loading; ++i) { data.pump(); serve(profiles); }
        assert(before->load == model::Load::ready && before->rows.size() == 1);
        const unsigned auth = data.act("auth:start");
        const char *code = R"({"state":"done","result":{"session":"0123456789abcdef0123456789abcdef","state":"pending","user_code":"ABCD-EFGH","verification_url":"https://www.google.com/device","expires_in":900,"poll_after":0}})";
        for (int i = 0; i < 12 && data.action(auth).load == model::Load::loading; ++i) { data.pump(); serve(code); }
        assert(data.action(auth).load == model::Load::ready);
        usleep(1100000);
        tube_poll();
        serve(R"({"state":"done","result":{"session":"0123456789abcdef0123456789abcdef","state":"authorized","profile":{"id":"oauth_0123456789abcdef","name":"My account"}}})");
        data.pump();
        // Back from approval must refresh Accounts even without Watch as or an explicit forget.
        profiles = saved_profiles;
        const Page *after = &data.catalog("library:profiles");
        assert(after->load == model::Load::loading);
        for (int i = 0; i < 12 && after->load == model::Load::loading; ++i) { data.pump(); serve(profiles); }
        assert(after->load == model::Load::ready && after->rows.size() == 2 && source.profile().guest);
        const Page &initial = list_profiles();
        assert(initial.rows[0].has("guest") && initial.rows[0].has("selected") && !initial.rows[0].has("signed_in"));
        assert(initial.rows[1].has("signed_in") && !initial.rows[1].has("guest") && !initial.rows[1].has("selected"));
        const unsigned select = data.act("library:profile:oauth_0123456789abcdef");
        for (int i = 0; i < 4; ++i) data.pump();
        assert(data.action(select).load == model::Load::ready);
        assert(source.profile().signed_in && !source.profile().guest);
        const Page &selected = list_profiles();
        assert(selected.rows[1].has("signed_in") && selected.rows[1].has("selected") && !selected.rows[0].has("selected"));
        // The authenticated library requests must use the selected account, not Guest.
        const Page *history = &data.catalog("library:history");
        for (int i = 0; i < 12 && history->load == model::Load::loading; ++i) { data.pump(); serve(kSearch); }
        assert(history->load == model::Load::ready);
        assert(g_jobs.back().find("\"profile\":\"oauth_0123456789abcdef\"") != std::string::npos);
        const unsigned guest = data.act("library:profile:guest");
        for (int i = 0; i < 4; ++i) data.pump();
        assert(data.action(guest).load == model::Load::ready && !source.profile().signed_in);
        assert(list_profiles().rows[0].has("selected"));
    }
    {
        ProviderSource source;
        Data data(source);
        unsigned select = data.act("library:profile:bell-test");
        for (int i = 0; i < 4; ++i) data.pump();
        assert(data.action(select).load == model::Load::ready && source.profile().id == "bell-test");
        const char *good = "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"notification_count\",\"unseen_count\":13}}";
        const auto count_page = [&](const char *reply) -> const Page & {
            data.forget("library:notification-count");
            const Page *page = &data.catalog("library:notification-count");
            for (int i = 0; i < 12 && page->load == model::Load::loading; ++i) { data.pump(); serve(reply); }
            return *page;
        };
        const Page &count = count_page(good);
        assert(count.load == model::Load::ready && count.rows.size() == 1 && count.rows[0].fact("unseen_count") == "13");
        assert(g_jobs.back().find("\"action\":\"notification_count\"") != std::string::npos &&
               g_jobs.back().find("\"profile\":\"bell-test\"") != std::string::npos);
        for (const char *value : {"-1", "0.5", "2147483648", "null", "\"3\""}) {
            const std::string malformed = std::string("{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"notification_count\",\"unseen_count\":") + value + "}}";
            assert(count_page(malformed.c_str()).load == model::Load::failed);
        }
        const char *notices = "{\"state\":\"done\",\"result\":{\"api_version\":1,\"items\":["
            "{\"id\":\"notified001\",\"title\":\"First notification\",\"unread\":true},"
            "{\"id\":\"notified002\",\"title\":\"Second notification\",\"unread\":false}],"
            "\"has_more\":true,\"cursor\":\"11111111-1111-4111-8111-111111111111\"}}";
        const Page *list = &data.catalog("library:notifications");
        for (int i = 0; i < 12 && list->load == model::Load::loading; ++i) { data.pump(); serve(notices); }
        assert(list->load == model::Load::ready && list->rows.size() == 2 && list->more);
        assert(list->rows[0].has("unread") && !list->rows[1].has("unread"));
        assert(count_page(good).load == model::Load::ready);
        // The background bell query leaves the notification continuation owner/cursor alone.
        const Page *next = &data.catalog("library:notifications", 1);
        for (int i = 0; i < 12 && next->load == model::Load::loading; ++i) { data.pump(); serve(notices); }
        assert(next->load == model::Load::ready && g_jobs.back().find("\"action\":\"continue\"") != std::string::npos &&
               g_jobs.back().find("11111111-1111-4111-8111-111111111111") != std::string::npos);
        const std::size_t jobs = g_jobs.size();
        const Page *extra = &data.catalog("library:notification-count", 1);
        for (int i = 0; i < 4; ++i) data.pump();
        assert(extra->load == model::Load::failed && g_jobs.size() == jobs);
        data.forget("library:notification-count");
        data.catalog("library:notification-count");
        for (int i = 0; i < 12 && g_jobs.size() == jobs; ++i) { data.pump(); serve(""); }
        assert(g_jobs.size() == jobs + 1);
        // A profile change outside the frame cancels an outstanding account-owned count.
        p->list_catalog("library:profile:guest", 0,
            [](int ok, const evo_provider_item_t *, int, int, void *) { assert(ok); }, nullptr);
        data.pump(); serve(good);
        assert(source.profile().guest);
        assert(count_page(good).load == model::Load::failed && g_jobs.size() == jobs + 1);
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
    {
        // Real provider -> ProviderSource -> Data -> LinkPhone update/take. The code arrives after
        // the action answer, with no user refresh; connected events and cast arrive while mounted.
        serve(kNothing);
        assert(g_pending.empty());
        ProviderSource source;
        Data data(source);
        pairing_test::LinkPhoneProbe screen;
        const auto frames = [&](int count = 6) {
            for (int i = 0; i < count; ++i) { data.pump(); screen.update(data, 0.5f); }
        };
        const auto action_index = [&](const char *action) {
            const std::string field = std::string("\"action\":\"") + action + "\"";
            for (std::size_t i = 0; i < g_pending.size(); ++i)
                if (g_pending[i].url.find("/v1/remote") != std::string::npos && g_pending[i].body.find(field) != std::string::npos) return i;
            assert(false && "expected remote request"); return std::size_t(0);
        };
        const std::string session = "0123456789abcdef0123456789abcdef";
        const std::string status = "{\"api_version\":1,\"state\":\"listening\",\"session\":\"" + session + "\"";
        frames();
        // Merely entering LinkPhone requests a code: no extra confirm/action is required.
        assert(screen.state() == "connecting" && screen.remote_.on && screen.remote_.code.empty());
        tube_poll();
        const auto start = action_index("start");
        cJSON *request = cJSON_Parse(g_pending[start].body.c_str()); assert(request);
        const std::string device = cJSON_GetObjectItemCaseSensitive(request, "device_id")->valuestring;
        cJSON_Delete(request);
        frames(12); // Delayed registration must not disable catalog polling.
        reply(start, 1, 200, status + ",\"device_id\":\"" + device + "\",\"screen_id\":\"screen-afterglow\"}");
        tube_poll();
        const auto pair = action_index("pair");
        frames(12);
        assert(screen.remote_.code.empty() && screen.remote_.on);
        reply(pair, 1, 200, status + ",\"pairing_code\":\"123456789012\"}");
        frames(12);
        assert(screen.state() == "ready" && screen.remote_.code == "123456789012");
        pairing_test::LinkPhoneProbe reopened;
        for (int i = 0; i < 6; ++i) { data.pump(); reopened.update(data, 0.5f); }
        assert(reopened.remote_.code == "123456789012" && reopened.ticket_ == 0 && !reopened.pair_on_entry_);
        assert(!std::strcmp(tube_row_fact_get("settings:remote:toggle", "selected"), "1"));
        assert(!std::strcmp(tube_row_fact_get("settings:remote:code", "value"), "123456789012"));
        int64_t phone_time = 1000000000000LL;
        tube_remote_tick(phone_time);
        reply(action_index("poll"), 1, 200, status + ",\"ack\":0,\"commands\":["
            "{\"sequence\":1,\"action\":\"remoteConnected\",\"device_id\":\"phone-one\",\"name\":\"My phone\"},"
            "{\"sequence\":2,\"action\":\"loungeStatus\",\"devices\":[{\"device_id\":\"phone-one\",\"name\":\"My phone\"}]},"
            "{\"sequence\":3,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],"
            "\"playlist\":\"\",\"index\":0,\"ctt\":\"\",\"position_sec\":12.5}]}");
        frames(12);
        assert(screen.state() == "connected" && screen.remote_.devices.size() == 1 && screen.remote_.devices[0].name == "My phone");
        int browser_requests = 0;
        tube_phone_controls controls{};
        controls.context = &browser_requests;
        controls.snapshot = [](void *, tube_remote_player *) { return 0; };
        controls.browser = [](void *value) { ++*static_cast<int *>(value); };
        tube_phone_tick_at(&controls, ++phone_time); // Connected and status do not block the cast.
        tube_phone_tick_at(&controls, ++phone_time);
        tube_phone_tick_at(&controls, ++phone_time);
        char video[12]; double position = 0;
        const unsigned ticket = tube_phone_take(video, &position);
        assert(ticket && browser_requests == 1 && !std::strcmp(video, "abcdefghijk") && position == 12.5);
        assert(tube_phone_launch_bind(ticket, 17) && tube_phone_launch_finish(ticket, 1));
        frames();
        assert(screen.state() == "connected"); // Screen stays mounted throughout native launch admission.
        tube_remote_tick(phone_time += 3000);
        reply(action_index("update"), 1, 200, status + "}");
        tube_remote_tick(phone_time += 3000);
        reply(action_index("poll"), 1, 200, status + ",\"ack\":3,\"commands\":["
            "{\"sequence\":4,\"action\":\"loungeStatus\",\"devices\":[]}]}");
        frames(12);
        assert(screen.state() == "ready" && screen.remote_.devices.empty());
        tube_phone_tick_at(&controls, ++phone_time);
        tube_remote_command command{};
        // An invalid ordered poll fails the receiver; the ready page must show the failure and erase its stale code.
        tube_remote_tick(phone_time += 3000);
        reply(action_index("update"), 1, 200, status + "}");
        tube_remote_tick(phone_time += 3000);
        reply(action_index("poll"), 1, 200, status + ",\"ack\":4,\"commands\":[{\"sequence\":6,\"action\":\"play\"}]}");
        frames(12);
        assert(screen.state() == "stopped" && screen.remote_.code.empty() && screen.remote_.devices.empty());
        assert(!tube_remote_take(&command));
        screen.act(data, "settings:remote:toggle"); frames();
        assert(screen.state() == "off" && !screen.remote_.on);
        // Dispose the bounded stop request; no live network was used.
        while (!g_pending.empty()) reply(0, 1, 200, "{\"api_version\":1,\"state\":\"stopped\"}");
    }
    p->shutdown();
    std::puts("PASS: Afterglow data on the provider: Guest Home with videos first, one request at a time and no further than the page reaches, row facts in the view "
              "models, a failed job, a cancelled request, a typed text with no prompt armed, the browsing profile, completions and matches while a search is "
              "typed with a newer text taking the place of an older request; delayed phone code, connection/device changes, cast admission while mounted, and receiver failure");
    return 0;
}
