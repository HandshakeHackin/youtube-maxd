/* YouTube Max’d release notifications. No installation or upstream updater.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_RELEASE_NOTICE_H
#define MAXD_RELEASE_NOTICE_H
#include <pthread.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include "cJSON.h"
#include "brand.h"
#define MAXD_RELEASE_API "https://api.github.com/repos/HandshakeHackin/youtube-maxd/releases/latest"
#define MAXD_RELEASE_PAGE "https://github.com/HandshakeHackin/youtube-maxd/releases/latest"
#define MAXD_RELEASE_LIMIT (256u * 1024u)

typedef struct { int ok, newer; char version[32]; } MaxdReleaseResult;
/* Strict stable SemVer, with optional v. No lexical comparisons or previews. */
static int maxd_release_version(const char *s, unsigned parts[3]) {
    if (!s) return 0;
    if (*s == 'v') ++s;
    for (int i = 0; i < 3; ++i) {
        unsigned n = 0; const char *start = s;
        if (*s < '0' || *s > '9') return 0;
        while (*s >= '0' && *s <= '9') {
            unsigned digit = (unsigned)(*s++ - '0');
            if (n > (UINT_MAX - digit) / 10) return 0;
            n = n * 10 + digit;
        }
        if (s - start > 1 && *start == '0') return 0;
        parts[i] = n;
        if (i < 2 && *s++ != '.') return 0;
    }
    return !*s;
}
static MaxdReleaseResult maxd_release_evaluate(const char *json, const char *installed) {
    MaxdReleaseResult out = {0}; unsigned a[3], b[3];
    if (!maxd_release_version(installed, a) || !json) return out;
    cJSON *root = cJSON_ParseWithOpts(json, NULL, 1);
    if (!cJSON_IsObject(root)) { cJSON_Delete(root); return out; }
    const cJSON *tag = cJSON_GetObjectItemCaseSensitive(root, "tag_name");
    const cJSON *draft = cJSON_GetObjectItemCaseSensitive(root, "draft");
    const cJSON *pre = cJSON_GetObjectItemCaseSensitive(root, "prerelease");
    if (cJSON_IsString(tag) && strlen(tag->valuestring) < sizeof out.version &&
        cJSON_IsFalse(draft) && cJSON_IsFalse(pre) && maxd_release_version(tag->valuestring, b)) {
        out.ok = 1;
        snprintf(out.version, sizeof out.version, "%s", tag->valuestring);
        for (int i = 0; i < 3; ++i) if (a[i] != b[i]) { out.newer = b[i] > a[i]; break; }
    }
    cJSON_Delete(root); return out;
}

#ifndef MAXD_RELEASE_TEST
#ifndef MAXD_RELEASE_TEST_TRANSPORT
#include <curl/curl.h>
#include "console_curl.h"
#endif
#include "evo_data_path.h"
static struct {
    pthread_mutex_t mutex;
    pthread_t worker;
    int joinable, running, ready, enabled, checked, manual, refresh;
    long long last;
    MaxdReleaseResult result;
    char path[512], announced[32], notice[320], detail[200];
} g_release = {.mutex = PTHREAD_MUTEX_INITIALIZER};
#ifndef MAXD_RELEASE_TEST_TRANSPORT
typedef struct { char *data; size_t length; } MaxdReleaseBody;
static size_t maxd_release_write(char *data, size_t size, size_t count, void *user) {
    MaxdReleaseBody *body = user;
    if (size && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (bytes > MAXD_RELEASE_LIMIT - body->length) return 0;
    memcpy(body->data + body->length, data, bytes); body->length += bytes;
    body->data[body->length] = 0; return bytes;
}
#endif
static void *maxd_release_worker(void *unused) {
    (void)unused; MaxdReleaseResult result = {0};
#ifdef MAXD_RELEASE_TEST_TRANSPORT
    result = maxd_release_test_fetch();
#else
    MaxdReleaseBody body = {malloc(MAXD_RELEASE_LIMIT + 1), 0};
    CURL *curl = body.data ? curl_easy_init() : NULL;
    if (curl) {
        body.data[0] = 0;
        console_curl_setup(curl);
        curl_easy_setopt(curl, CURLOPT_URL, MAXD_RELEASE_API);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "YouTube-Maxd/" YM_VERSION);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, maxd_release_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        /* Fixed HTTPS endpoint, no account headers, cookies or redirects. */
        CURLcode code = curl_easy_perform(curl); long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        if (code == CURLE_OK && status == 200) result = maxd_release_evaluate(body.data, YM_VERSION);
        curl_easy_cleanup(curl);
    }
    free(body.data);
#endif
    pthread_mutex_lock(&g_release.mutex);
    g_release.result = result; g_release.ready = 1;
    pthread_mutex_unlock(&g_release.mutex);
    return NULL;
}
static void maxd_release_init(void) {
    g_release.enabled = 1; g_release.checked = g_release.manual = g_release.refresh = 0;
    g_release.announced[0] = g_release.notice[0] = 0;
    snprintf(g_release.detail, sizeof g_release.detail, "Not checked yet; installed version %s", YM_VERSION);
    snprintf(g_release.path, sizeof g_release.path, "%s/release-notifications", evo_data_dir());
    FILE *f = fopen(g_release.path, "r");
    if (f) { g_release.enabled = fgetc(f) != '0'; fclose(f); }
}
static int maxd_release_start(int manual) {
    if (g_release.running) return 0;
    g_release.manual = manual;
    g_release.checked = 1; g_release.last = clock_now();
    pthread_attr_t attr;
    int initialized = pthread_attr_init(&attr) == 0;
    int rc = initialized ? pthread_attr_setstacksize(&attr, 1024 * 1024) : -1;
    if (!rc) rc = pthread_create(&g_release.worker, &attr, maxd_release_worker, NULL);
    if (initialized) pthread_attr_destroy(&attr);
    if (rc) {
        snprintf(g_release.detail, sizeof g_release.detail, "Could not start the update check. Try again in About.");
        if (manual) snprintf(g_release.notice, sizeof g_release.notice, "%s", g_release.detail);
        g_release.manual = 0; g_release.refresh = 1;
        return 0;
    }
    g_release.joinable = g_release.running = 1;
    snprintf(g_release.detail, sizeof g_release.detail, "Checking GitHub for a stable release...");
    g_release.refresh = 1; return 1;
}
static void maxd_release_shutdown(void) {
    if (g_release.joinable) pthread_join(g_release.worker, NULL);
    g_release.joinable = g_release.running = g_release.ready = 0;
}
static void maxd_release_tick(int ready) {
    if (g_release.enabled && !g_release.running && ready &&
        (!g_release.checked || clock_now() - g_release.last >= 6 * 60 * 60)) maxd_release_start(0);
    pthread_mutex_lock(&g_release.mutex);
    int done = g_release.ready; MaxdReleaseResult result = g_release.result;
    g_release.ready = 0;
    pthread_mutex_unlock(&g_release.mutex);
    if (!done) return;
    pthread_join(g_release.worker, NULL); g_release.joinable = g_release.running = 0;
    g_release.refresh = 1;
    if (!result.ok) {
        snprintf(g_release.detail, sizeof g_release.detail, "Could not check GitHub. Try again later.");
        if (g_release.manual) snprintf(g_release.notice, sizeof g_release.notice, "%s", g_release.detail);
    } else if (result.newer) {
        snprintf(g_release.detail, sizeof g_release.detail, "%s available; installed %s", result.version, YM_VERSION);
        if ((g_release.enabled || g_release.manual) && (g_release.manual || strcmp(g_release.announced, result.version))) {
            snprintf(g_release.notice, sizeof g_release.notice,
                "YouTube Max’d %s is available. Open Settings > About > Download latest release.", result.version);
            snprintf(g_release.announced, sizeof g_release.announced, "%s", result.version);
        }
    } else {
        snprintf(g_release.detail, sizeof g_release.detail, "No newer stable release; installed %s", YM_VERSION);
        if (g_release.manual) snprintf(g_release.notice, sizeof g_release.notice, "%s", g_release.detail);
    }
    g_release.manual = 0;
}
static int maxd_release_action(const char *action) {
    if (!strcmp(action, "check")) { maxd_release_start(1); return 1; }
    if (strcmp(action, "notify")) return 0;
    int enabled = !g_release.enabled;
    char temporary[520]; snprintf(temporary, sizeof temporary, "%s.tmp", g_release.path);
    FILE *f = fopen(temporary, "w"); int saved = 0;
    if (f) {
        int written = fprintf(f, "%d\n", enabled) > 0;
        int closed = fclose(f) == 0;
        saved = written && closed && rename(temporary, g_release.path) == 0;
    }
    if (saved) { g_release.enabled = enabled; if (enabled) g_release.checked = 0; }
    else { remove(temporary); snprintf(g_release.notice, sizeof g_release.notice, "Could not save update notification setting. Try again."); }
    g_release.refresh = 1; return 1;
}
static int maxd_release_rows(tube_update_row *rows, int capacity) {
    const char *ids[] = {"settings:update:check", "settings:update:notify", "link:maxd-release"};
    const char *titles[] = {"Check for updates", g_release.enabled ? "Automatic update notices: On" : "Automatic update notices: Off", "Download latest release"};
    const char *details[] = {g_release.detail, "Checks at launch and every six hours; downloads nothing", MAXD_RELEASE_PAGE};
    int count = capacity < 3 ? capacity : 3;
    for (int i = 0; i < count; ++i) {
        snprintf(rows[i].id, sizeof rows[i].id, "%s", ids[i]);
        snprintf(rows[i].title, sizeof rows[i].title, "%s", titles[i]);
        snprintf(rows[i].detail, sizeof rows[i].detail, "%s", details[i]);
    }
    return count;
}
#endif
#endif
