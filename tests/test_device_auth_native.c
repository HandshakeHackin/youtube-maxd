/* Shipping native sign-in state machine with deterministic time/HTTP.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *session = "0123456789abcdef0123456789abcdef";
static long long clock_ms = 100000;
int tube_test_clock_gettime(clockid_t clock_id, struct timespec *t) {
    assert(clock_id == CLOCK_MONOTONIC);
    t->tv_sec = clock_ms / 1000; t->tv_nsec = clock_ms % 1000 * 1000000; return 0;
}
typedef struct { char method[8], url[1024], body[2048], bearer[160]; evo_net_cb cb; void *ud; } Pending;
static Pending pending[32]; static int count, ok, calls, rows;
static evo_provider_item_t items[24];
int evo_net_request_async(const char *method, const char *url, const char *body,
        const char **headers, int n, evo_net_cb cb, void *ud) {
    assert(count < 32); Pending *p = &pending[count++]; memset(p, 0, sizeof *p);
    snprintf(p->method, sizeof p->method, "%s", method);
    snprintf(p->url, sizeof p->url, "%s", url);
    snprintf(p->body, sizeof p->body, "%s", body ? body : "");
    for (int i = 0; i < n; ++i) if (!strncmp(headers[i], "Authorization:", 14))
        snprintf(p->bearer, sizeof p->bearer, "%s", headers[i]);
    p->cb = cb; p->ud = ud; return 0;
}
void evo_error_set(const char *format, ...) { (void)format; }
static void reply(int i, int status, const char *body) {
    assert(i < count); Pending p = pending[i];
    memmove(pending + i, pending + i + 1, (size_t)(--count - i) * sizeof *pending);
    p.cb(status == 200, status, body, strlen(body) + 1, p.ud);
}
static void list(int success, const evo_provider_item_t *value, int n, int more, void *ud) {
    (void)ud; assert(!more); ++calls; ok = success; rows = n;
    assert(n <= 24); if (value) memcpy(items, value, (size_t)n * sizeof *value);
}
static void clear_ignored(void) {
    while (count) {
        assert(!strcmp(pending[0].method, "DELETE") || strstr(pending[0].url, "/v1/auth/cancel"));
        reply(0, 200, "{}");
    }
}
static void job_done(const char *result) {
    assert(count == 1 && strstr(pending[0].url, "/v1/jobs") && !strcmp(pending[0].method, "POST"));
    reply(0, 200, "{\"job\":\"auth_job\",\"state\":\"pending\"}");
    clock_ms += 250; tube_poll();
    assert(count == 1 && !strcmp(pending[0].method, "GET"));
    char response[2048]; snprintf(response, sizeof response, "{\"state\":\"done\",\"result\":%s}", result);
    reply(0, 200, response);
}
static void code_done(int expiry, int delay) {
    char result[1024];
    snprintf(result, sizeof result, "{\"state\":\"pending\",\"session\":\"%s\",\"user_code\":\"ABCD-EFGH\",\"verification_url\":\"https://www.google.com/device\",\"expires_in\":%d,\"poll_after\":%d}", session, expiry, delay);
    job_done(result);
    assert(!count);
}
static void check_store(void) {
    FILE *file = fopen(evo_data_path("youtube.json"), "rb"); assert(file);
    char data[8192] = {0}; assert(fread(data, 1, sizeof data - 1, file) > 0); fclose(file);
    assert(!strstr(data, session) && !strstr(data, "ABCD-EFGH") && !strstr(data, "google.com/device"));
    assert(!strstr(data, "access_token") && !strstr(data, "refresh_token"));
}
int main(void) {
    char dir[] = "/tmp/prosperotube-auth-XXXXXX"; assert(mkdtemp(dir));
    assert(!setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    const evo_provider_t *p = &evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!p->list_catalog("auth:start", 0, list, NULL));
    assert(count == 1 && strstr(pending[0].url, "/v1/pair"));
    reply(0, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(strstr(pending[0].body, "\"action\":\"auth\"") && strstr(pending[0].body, "\"start\""));
    code_done(900, 5);
    assert(ok && rows == 3 && !strcmp(items[0].title, "ABCD-EFGH"));
    assert(strstr(items[0].overview, "automatic") && !strcmp(items[2].id, "auth:cancel"));
    unsigned revision = tube_auth_revision();
    p->list_catalog("auth:start", 0, list, NULL); assert(!count && rows == 3);
    p->list_catalog("auth:session", 0, list, NULL); assert(!count);
    clock_ms += 4999; tube_poll(); assert(!count);
    clock_ms++; tube_poll(); assert(count == 1 && strstr(pending[0].body, session));
    tube_poll(); assert(count == 1);  /* Single-flight upstream poll. */
    code_done(890, 10); assert(tube_auth_revision() == revision); /* Preserve focus during pending polls. */
    clock_ms += 9999; tube_poll(); assert(!count);
    clock_ms++; tube_poll(); assert(count == 1);
    job_done("{\"state\":\"authorized\",\"session\":\"0123456789abcdef0123456789abcdef\",\"profile\":{\"id\":\"oauth_0123456789abcdef\",\"name\":\"Living room\"}}");
    p->list_catalog("auth:session", 0, list, NULL);
    assert(ok && rows == 2 && !strcmp(items[0].id, "library:profile:oauth_0123456789abcdef"));
    assert(!strcmp(items[0].subtitle, "Living room")); check_store();
    p->list_catalog(items[0].id, 0, list, NULL); assert(rows == 12 && !count);
    assert(!strcmp(items[11].id, "account:remove"));
    unsigned accounts_revision = tube_accounts_revision();
    p->list_catalog("account:remove", 0, list, NULL);
    assert(ok && rows == 1 && !count && strstr(items[0].overview, "oauth_0123456789abcdef"));
    char apply[160]; snprintf(apply, sizeof apply, "%s", items[0].id);
    assert(strstr(tube_comment_text(apply), "saved sign-in"));
    p->list_catalog("library:profile:guest", 0, list, NULL);
    p->list_catalog(apply, 0, list, NULL); assert(!ok && !count);
    p->list_catalog("library:profile:oauth_0123456789abcdef", 0, list, NULL);
    p->list_catalog("account:remove", 0, list, NULL);
    snprintf(apply, sizeof apply, "%s", items[0].id);
    p->list_catalog(apply, 0, list, NULL);
    assert(count == 1 && strstr(pending[0].body, "profile_remove") && strstr(pending[0].body, "oauth_0123456789abcdef"));
    job_done("{\"success\":true,\"profile\":\"wrong_profile\"}");
    assert(!ok && tube_accounts_revision() == accounts_revision);
    p->list_catalog(apply, 0, list, NULL); assert(ok && !count); /* one attempt */
    p->list_catalog("account:remove", 0, list, NULL);
    snprintf(apply, sizeof apply, "%s", items[0].id);
    p->list_catalog(apply, 0, list, NULL);
    job_done("{\"success\":true,\"profile\":\"oauth_0123456789abcdef\"}");
    assert(ok && rows == 1 && !strcmp(items[0].title, "Account removed"));
    assert(tube_accounts_revision() == accounts_revision + 1);
    p->list_catalog("library", 0, list, NULL);
    assert(rows == 11 && strstr(items[0].title, "guest"));
    p->list_catalog(apply, 0, list, NULL); assert(!ok && !count);
    p->list_catalog("account:remove", 0, list, NULL); assert(!ok && !count);
    check_store();
    /* Abandoned/externally removed accounts are reconciled by an authoritative list. */
    p->list_catalog("library:profile:external", 0, list, NULL);
    accounts_revision = tube_accounts_revision();
    p->list_catalog("library:profiles", 0, list, NULL);
    job_done("{}"); assert(!ok && tube_accounts_revision() == accounts_revision);
    p->list_catalog("library:profiles", 0, list, NULL);
    job_done("{\"profiles\":[{\"id\":\"external\",\"name\":\"External\"}]}");
    assert(!ok && tube_accounts_revision() == accounts_revision);
    p->list_catalog("library:profiles", 0, list, NULL);
    job_done("{\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\"}]}");
    assert(ok && tube_accounts_revision() == accounts_revision + 1);
    p->list_catalog("library", 0, list, NULL); assert(rows == 11 && strstr(items[0].title, "guest"));

    /* Leaving before start POST completes cleans a late job using its original source/token. */
    p->list_catalog("auth:start", 0, list, NULL); assert(count == 1);
    p->set_source("http://127.0.0.2:8765#654321");
    reply(0, 200, "{\"job\":\"late_auth_job\",\"state\":\"pending\"}");
    assert(count == 1 && !strcmp(pending[0].method, "DELETE"));
    assert(strstr(pending[0].url, "http://127.0.0.1:8765/v1/jobs/late_auth_job"));
    assert(strstr(pending[0].bearer, "abcdefghijklmnopqrstuvwxabcdefghijkl")); clear_ignored();
    p->set_source("http://127.0.0.1:8765#123456");
    p->list_catalog("auth:start", 0, list, NULL);
    reply(0, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    code_done(1, 5); clock_ms += 1000; tube_poll();
    assert(count == 1 && strstr(pending[0].url, "/v1/auth/cancel")); clear_ignored();
    p->list_catalog("auth:session", 0, list, NULL);
    assert(rows == 2 && strstr(items[0].title, "expired"));

    p->list_catalog("auth:start", 0, list, NULL); code_done(900, 0);
    clock_ms += 1000; tube_poll(); assert(count == 1);
    reply(0, 200, "{\"job\":\"late_poll\",\"state\":\"pending\"}");
    clock_ms += 250; tube_poll(); assert(count == 1 && !strcmp(pending[0].method, "GET"));
    p->list_catalog("library", 0, list, NULL); assert(rows == 11);
    reply(0, 200, "{\"state\":\"done\",\"result\":{\"state\":\"authorized\",\"session\":\"0123456789abcdef0123456789abcdef\",\"profile\":{\"id\":\"wrong\",\"name\":\"Late\"}}}");
    assert(count >= 1); clear_ignored();
    p->list_catalog("auth:session", 0, list, NULL); assert(strstr(items[0].title, "canceled"));
    check_store();

    /* Malformed pending data fails without displaying an attacker-controlled URL. */
    p->list_catalog("auth:start", 0, list, NULL);
    job_done("{\"state\":\"pending\",\"session\":\"0123456789abcdef0123456789abcdef\",\"user_code\":\"ABCD-EFGH\",\"verification_url\":\"https://evil.example/device\",\"expires_in\":900,\"poll_after\":5}");
    clear_ignored(); p->list_catalog("auth:session", 0, list, NULL);
    assert(rows == 2 && strstr(items[0].title, "could not"));
    p->list_catalog("auth:start", 0, list, NULL);
    job_done("{\"state\":\"pending\",\"session\":\"0123456789abcdef0123456789abcdef\",\"user_code\":\"ABCD-EFGH\",\"verification_url\":\"https://www.google.com/device\",\"expires_in\":900,\"poll_after\":-1}");
    clear_ignored(); p->list_catalog("auth:session", 0, list, NULL);
    assert(rows == 2 && strstr(items[0].title, "could not"));
    p->list_catalog("auth:start", 0, list, NULL); code_done(900, 0);
    clock_ms += 1000; tube_poll();
    job_done("{\"state\":\"denied\",\"session\":\"0123456789abcdef0123456789abcdef\"}");
    p->list_catalog("auth:session", 0, list, NULL);
    assert(rows == 2 && strstr(items[0].title, "denied"));
    clock_ms += 10000; tube_poll(); assert(!count);
    assert(!p->list_catalog("auth:start", 1, list, NULL) && !ok && !count);
    assert(!p->list_catalog("auth:unknown", 0, list, NULL) && !ok && !count);
    assert(calls > 10);
    unlink(evo_data_path("youtube.json")); assert(!rmdir(dir));
    puts("native account sign-in/removal: polling, confirmation, Guest reset, reconciliation, cancellation and privacy passed");
    return 0;
}
