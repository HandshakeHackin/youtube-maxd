/* Accounts end to end on the host: the shipping provider in built-in mode,
 * EVO's own HTTP client and the real service in this process, with Google and
 * YouTube played by canned answers (TUBE_JS_FIXTURES, written by
 * scripts/test_embedded_accounts.sh). No request leaves the machine and no
 * account exists.
 *
 *   start -> the account rows are offered -> sign in with a device code ->
 *   select the new profile -> its subscriptions -> like a video -> remove the
 *   account -> Guest again
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "tube_service.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_TOKEN "e2e-accounts-token-0123456789abcdef"
/* As the console build configures it, with the bundle as a file and a token this test knows. */
void __real_tube_service_configure(const tube_service_options *options);
void __wrap_tube_service_configure(const tube_service_options *options) {
    static tube_service_options host;
    host = *options;
    host.bundle_path = getenv("TUBE_E2E_BUNDLE");
    host.token = TEST_TOKEN;
    __real_tube_service_configure(&host);
}

static int item_calls, last_ok, last_count, fetch_calls, fetch_status;
static evo_provider_item_t last_items[26];
static char fetched[1 << 16];

static void items_cb(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    (void)ud; (void)more; ++item_calls; last_ok = ok; last_count = count;
    assert(count <= 26);
    if (items) memcpy(last_items, items, (size_t)count * sizeof *items);
}
static void fetch_cb(int ok, int status, const char *body, size_t size, void *ud) {
    (void)ud; (void)ok; ++fetch_calls; fetch_status = status;
    snprintf(fetched, sizeof fetched, "%.*s", (int)(size < sizeof fetched - 1 ? size : sizeof fetched - 1), body ? body : "");
}
static double seconds(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }
static void turn(void) { evo_net_poll(); tube_poll(); usleep(5000); }
static void run_until(const int *counter, int target, double limit, const char *what) {
    double end = seconds() + limit;
    while (*counter < target) {
        if (seconds() > end) { fprintf(stderr, "timed out waiting for %s\n", what); abort(); }
        turn();
    }
}
static int find(const char *id) { for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].id, id)) return i; return -1; }
/* A list that needs the service: wait for its rows. */
static void open_list(const evo_provider_t *p, const char *parent) {
    int calls = item_calls;
    assert(p->list_catalog(parent, 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, parent);
}
/* What the canned answers saw since the last look, straight from the service. */
static const char *upstream(unsigned port) {
    char url[128];
    const char *headers[] = {"Authorization: Bearer " TEST_TOKEN};
    int calls = fetch_calls;
    snprintf(url, sizeof url, "http://127.0.0.1:%u/v1/_upstream?clear=1", port);
    assert(evo_net_request_async("GET", url, NULL, headers, 1, fetch_cb, NULL) == 0);
    run_until(&fetch_calls, calls + 1, 20, "the upstream log");
    assert(fetch_status == 200);
    return fetched;
}
static const char *file_text(const char *path) {
    static char text[1 << 16];
    FILE *file = fopen(path, "rb");
    text[0] = 0;
    if (!file) return NULL;
    size_t got = fread(text, 1, sizeof text - 1, file); fclose(file); text[got] = 0;
    return text;
}

int main(void) {
    char dir[] = "/tmp/tube-e2e-accounts-XXXXXX", store[600];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    assert(getenv("TUBE_JS_FIXTURES") && getenv("TUBE_E2E_BUNDLE"));
    snprintf(store, sizeof store, "%s/accounts/oauth-profiles.json", dir);
    /* Boot with existing credentials: catalog() must clone and list them without a YouTube request. */
    char accounts_dir[600]; snprintf(accounts_dir, sizeof accounts_dir, "%s/accounts", dir);
    assert(!mkdir(accounts_dir, 0700));
    FILE *seed = fopen(store, "wb"); assert(seed);
    fputs("{\"version\":1,\"profiles\":[", seed);
    for (int i = 2; i <= 3; ++i)
        fprintf(seed, "%s{\"id\":\"oauth_%016x\",\"name\":\"Saved account %d\",\"cookie\":\"\","
            "\"oauth\":{\"access_token\":\"saved-access-%d\",\"refresh_token\":\"saved-refresh-%d\","
            "\"expiry_date\":\"2099-01-01T00:00:00.000Z\",\"client\":{\"client_id\":\"fixture.apps.invalid\","
            "\"client_secret\":\"fixture-secret\"}}}", i == 2 ? "" : ",", i, i, i, i);
    fputs("]}", seed); assert(!fclose(seed));
    const evo_provider_t *p = &evo_provider_youtube;
    assert(evo_net_init() == 0);
    assert(!p->init() && tube_embedded_mode() && tube_service_running());
    unsigned port = 0;
    assert(sscanf(tube_embedded_address(), "127.0.0.1:%u", &port) == 1);

    open_list(p, "library:profiles");
    assert(last_ok && last_count == 3 && find("library:profile:guest") >= 0 &&
        find("library:profile:oauth_0000000000000002") >= 0 && find("library:profile:oauth_0000000000000003") >= 0);
    assert(strstr(upstream(port), "\"requests\":[]"));

    /* The first request reads what the service offers: sign-in is among it, so the account rows are there. */
    int calls = item_calls;
    assert(p->search("canned", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "search results");
    assert(last_ok && last_count >= 2);
    p->list_catalog("discover", 0, items_cb, NULL);
    assert(last_ok && find("library") >= 0 && !strstr(last_items[find("library")].subtitle, "later version"));
    p->list_catalog("library", 0, items_cb, NULL);
    assert(last_ok && find("auth:start") >= 0 && find("library:profiles") >= 0 && find("library:subscriptions") >= 0 && find("account:notice") < 0);
    assert(find("account:remove") < 0);   /* Guest has nothing to remove */
    /* An account list as Guest is refused by the service, not hidden by the app. */
    open_list(p, "library:subscriptions");
    assert(!last_ok);
    assert(file_text(store) && strstr(file_text(store), "Saved account 2"));
    upstream(port);

    /* Sign in: the code and the address come from the service, the provider polls by itself. */
    open_list(p, "auth:start");
    assert(last_ok && last_count == 3 && !strcmp(last_items[0].title, "ABCD-EFGH") && !strcmp(last_items[1].title, "https://www.google.com/device") &&
           !strcmp(last_items[2].id, "auth:cancel"));
    printf("sign-in: code %s at %s\n", last_items[0].title, last_items[1].title);
    assert(strstr(upstream(port), "oauth2/device/code") && !strstr(fetched, "oauth2/token"));
    char profile[160] = "";
    for (double end = seconds() + 30; !*profile;) {
        if (seconds() > end) { fprintf(stderr, "timed out waiting for the authorization\n"); abort(); }
        for (int i = 0; i < 40; ++i) turn();
        p->list_catalog("auth:session", 0, items_cb, NULL);
        if (last_ok && last_count == 2 && !strncmp(last_items[0].id, "library:profile:oauth_", 22)) snprintf(profile, sizeof profile, "%s", last_items[0].id);
        else assert(last_ok && last_count == 3);   /* still waiting, the code still shown */
    }
    assert(!strcmp(last_items[0].subtitle, "YouTube account") && strstr(last_items[1].subtitle, "this PS5"));
    printf("authorized: %s\n", profile);
    /* One token request did it; the credentials are in the private file and nowhere else. */
    const char *log = upstream(port), *at = strstr(log, "oauth2/token");
    assert(at && !strstr(at + 1, "oauth2/token"));
    const char *kept = file_text(store);
    assert(kept && strstr(kept, "refresh-token-1") && strstr(kept, "access-token-1") && strstr(kept, profile + 16));
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL);   /* a change that saves the app's own state */
    const char *state = file_text(evo_data_path("youtube.json"));
    assert(state && !strstr(state, "token-1") && !strstr(state, "ABCD-EFGH") && !strstr(state, "fixture-client-secret"));

    /* The profile: selected, its rows, its subscriptions asked with its token. */
    p->list_catalog(profile, 0, items_cb, NULL);
    assert(last_ok && find("account:remove") >= 0 && find("library:subscriptions") >= 0 && strstr(last_items[0].title, profile + 16));
    open_list(p, "library:subscriptions");
    assert(last_ok && last_count >= 2 && find("subscribed1") >= 0 && find("subscribed2") >= 0);
    printf("subscriptions: %d rows, first \"%s\"\n", last_count, last_items[find("subscribed1")].title);
    assert(strstr(upstream(port), "\"bearer\":true,\"tag\":\"subscriptions\""));

    /* One change: like a video. A review row first, then exactly one request to YouTube. */
    p->list_catalog("account:confirm:like:fixtureVID1", 0, items_cb, NULL);
    assert(last_ok && last_count == 1 && !strncmp(last_items[0].id, "account:apply:", 14) && !strcmp(last_items[0].title, "Like video"));
    char apply[160]; snprintf(apply, sizeof apply, "%s", last_items[0].id);
    assert(!strstr(upstream(port), "like/like"));
    open_list(p, apply);
    assert(last_ok);
    log = upstream(port); at = strstr(log, "v1/like/like");
    assert(at && !strstr(at + 1, "v1/like/like") && strstr(at, "\"bearer\":true"));
    p->list_catalog(apply, 0, items_cb, NULL);   /* the same confirmation again sends nothing */
    for (int i = 0; i < 40; ++i) turn();
    assert(!strstr(upstream(port), "like/like"));
    puts("change: one like, sent once");

    /* Sign out: review, confirm, and the account is gone from the service, the file and the app. */
    p->list_catalog("account:remove", 0, items_cb, NULL);
    assert(last_ok && last_count == 1);
    snprintf(apply, sizeof apply, "%s", last_items[0].id);
    open_list(p, apply);
    assert(last_ok && last_count == 1 && !strcmp(last_items[0].title, "Account removed"));
    kept = file_text(store);
    assert(kept && !strstr(kept, "token-1") && strstr(kept, "Saved account 2") && strstr(kept, "Saved account 3"));
    p->list_catalog("library", 0, items_cb, NULL);
    assert(last_ok && strstr(last_items[0].title, "guest") && find("account:remove") < 0 && find("auth:start") >= 0);
    open_list(p, "library:profiles");
    assert(last_ok && last_count == 3 && find("library:profile:guest") >= 0 &&
        find("library:profile:oauth_0000000000000002") >= 0 && find("library:profile:oauth_0000000000000003") >= 0);
    assert(!strstr(upstream(port), "\"bearer\":true"));
    puts("removed: Guest and both pre-existing profiles remain");

    p->shutdown();
    assert(!tube_service_running());
    evo_net_shutdown();
    puts("PASS: accounts on the real built-in service: sign-in, profile, subscriptions, one change, removal");
    return 0;
}
