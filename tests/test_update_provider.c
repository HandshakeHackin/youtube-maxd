/* About's update rows through the shipping provider: the check at start and on
 * request, "Notify about updates", the release notes in the long-text reader,
 * skip, download, restart and the first start of the new version.
 * The network is a function in this file. SPDX-License-Identifier: GPL-3.0-or-later */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_update.h"
#include "tube_update_fs.h"
#include "tube_update_sha256.h"
#include "tube_about.h"

static const evo_provider_t *p;
static unsigned char *g_zip;
static size_t g_zip_size;
static char g_manifest[2048], g_dir[] = "/tmp/tube-update-ui-XXXXXX", g_install[128];
static int g_fetches;
static long long g_clock = 1791727380LL;
static long long test_clock(void) { return g_clock; }

static int fixture_get(void *context, const char *url, int ranged, uint64_t first, uint64_t last, size_t limit, const volatile int *cancel, tube_update_reply *reply) {
    (void)context; (void)limit; (void)cancel;
    memset(reply, 0, sizeof *reply);
    __atomic_add_fetch(&g_fetches, 1, __ATOMIC_SEQ_CST);
    if (!strcmp(url, "http://fixture/releases.json")) {
        reply->status = 200; reply->length = strlen(g_manifest);
        reply->body = malloc(reply->length); memcpy(reply->body, g_manifest, reply->length);
    } else if (!strcmp(url, "http://fixture/v01.000.010/PPSA99142.zip") && ranged && first <= last && last < g_zip_size) {
        reply->status = 206; reply->length = (size_t)(last - first + 1);
        reply->body = malloc(reply->length); memcpy(reply->body, g_zip + first, reply->length);
        snprintf(reply->content_range, sizeof reply->content_range, "bytes %llu-%llu/%zu", (unsigned long long)first, (unsigned long long)last, g_zip_size);
    } else reply->status = 404;
    return 0;
}

static void show(const char *id) { p->list_catalog(id, 0, items_cb, NULL); assert(last_ok && !pending_count); }
static int find(const char *id) {
    for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].id, id)) return i;
    return -1;
}
static void write_text(const char *path, const char *body) {
    FILE *file = fopen(path, "wb");
    assert(file && fputs(body, file) >= 0 && fclose(file) == 0);
}
static tube_update *engine(int development) {
    tube_update_config config = {.data_dir = g_dir, .install_dir = g_install, .manifest_url = "http://fixture/releases.json",
                                 .download_prefix = "http://fixture/", .any_host = 1, .chunk_bytes = 65536, .development = development,
                                 .now = test_clock, .transport = {NULL, fixture_get}};
    tube_update *update = tube_update_new(&config);
    assert(update);
    return update;
}
/* Frames until the worker has finished and its outcome was taken. */
static void settle(tube_update *update) {
    tube_update_status status;
    for (int i = 0; i < 4000; ++i) {
        tube_poll(); tube_update_get(update, &status);
        if (!status.busy) { tube_poll(); return; }
        usleep(2000);
    }
    assert(!"the worker did not finish");
}
/* Frames until `notice` is shown (NULL: until the row `id` is listed in About). */
static void frames(const char *notice, const char *id) {
    for (int i = 0; i < 4000; ++i) {
        tube_poll();
        const char *shown = tube_update_app_take_notice();
        if (shown && notice && strstr(shown, notice)) return;
        if (shown && notice) { fprintf(stderr, "notice: %s\n", shown); assert(!"another notice"); }
        if (!notice) { show("settings:about"); if (find(id) >= 0) return; }
        usleep(2000);
    }
    assert(!"the update did not get there");
}

int main(int argc, char **argv) {
    char path[256], sha[65], title[96];
    tube_update_sha256 hash;
    assert(argc == 2 && mkdtemp(g_dir) && !setenv("EVO_DATA_DIR_OVERRIDE", g_dir, 1));
    g_zip = (unsigned char *)tube_update_read(argv[1], 8 << 20, &g_zip_size); assert(g_zip);
    tube_update_sha256_init(&hash); tube_update_sha256_add(&hash, g_zip, g_zip_size); tube_update_sha256_finish(&hash, sha);
    snprintf(g_manifest, sizeof g_manifest,
        "[{\"tag_name\":\"v01.000.010\",\"draft\":false,\"prerelease\":true,\"name\":\"ProsperoTube 0.2.0\","
        "\"html_url\":\"https://github.com/blackbearreloaded/ProsperoTube/releases/tag/v01.000.010\",\"body\":\"## Fixed\\n- Search works again\","
        "\"assets\":[{\"name\":\"PPSA99142.zip\",\"state\":\"uploaded\",\"size\":%zu,\"browser_download_url\":\"http://fixture/v01.000.010/PPSA99142.zip\",\"digest\":\"sha256:%s\"}]}]",
        g_zip_size, sha);
    snprintf(g_install, sizeof g_install, "%s/homebrew/PPSA99142", g_dir);
    snprintf(path, sizeof path, "%s/sce_sys", g_install); assert(tube_update_mkdirs(g_dir, path) == 0);
    snprintf(path, sizeof path, "%s/sce_sys/param.json", g_install); write_text(path, "{\"titleId\":\"PPSA99142\",\"contentVersion\":\"01.000.000\"}");
    snprintf(path, sizeof path, "%s/eboot.bin", g_install); write_text(path, "old program");
    tube_update_app_set_clock(test_clock);

    /* Without an updater (a companion-only package) About is what it was. */
    p = &evo_provider_youtube; assert(!p->init());
    tube_about_title(title, sizeof title);
    show("settings:about"); assert(last_count == 5 && !tube_update_app_take_notice());
    show("settings:update:check"); assert(last_count == 5);   /* an old row id lists About and does nothing */
    show("settings:section:0"); assert(find("settings:refresh") >= 0 && find("settings:update:check") < 0);

    /* With one: the installed version, the check and the switch. Nothing was asked yet. */
    tube_update *update = engine(0);
    tube_update_app_use(update);
    show("settings:about");
    assert(last_count == 7 && !strcmp(last_items[0].title, title) && !strcmp(last_items[4].id, "settings:about:changelog"));
    assert(!strcmp(last_items[5].id, "settings:update:check") && !strcmp(last_items[5].title, "Check for updates") &&
           !strcmp(last_items[5].subtitle, "Installed 01.000.000; the latest version is not known yet"));
    assert(!strcmp(last_items[6].id, "settings:update:notify") && !strcmp(last_items[6].title, "Notify about updates: on"));
    assert(last_items[5].is_folder && last_items[6].is_folder && !g_fetches);
    /* A section's menu (Touchpad on a main-menu row) offers the check too. */
    show("settings:section:0");
    assert(find("settings:update:check") == last_count - 1 && !strcmp(last_items[last_count - 1].title, "Check for updates") && !g_fetches);
    /* The switch acts in place and is kept. */
    show("settings:update:notify"); assert(last_count == 7 && !strcmp(last_items[6].title, "Notify about updates: off"));
    snprintf(path, sizeof path, "%s/update/check.json", g_dir);
    char *stored = tube_update_read(path, 1 << 16, NULL); assert(stored && strstr(stored, "\"notify\":false")); free(stored);
    /* Off: the check at start still runs, and says nothing. */
    for (int i = 0; i < 400 && !g_fetches; ++i) { tube_poll(); usleep(2000); }
    assert(g_fetches == 1);
    frames(NULL, "settings:update:install"); assert(!tube_update_app_take_notice());
    tube_update_app_use(NULL); tube_update_free(update);

    /* On, in the next run: the stored answer is announced once, without asking again that day. */
    update = engine(0); tube_update_set_notify(update, 1); tube_update_app_use(update);
    frames("Version 01.000.010 is available", NULL);
    for (int i = 0; i < 50; ++i) { tube_poll(); assert(!tube_update_app_take_notice()); }
    assert(g_fetches == 1);
    show("settings:about");
    assert(last_count == 10 && !strcmp(last_items[5].subtitle, "Installed 01.000.000; latest 01.000.010 (ProsperoTube 0.2.0)"));
    assert(!strcmp(last_items[7].id, "description:update") && !strcmp(last_items[7].title, "What is new in ProsperoTube 0.2.0"));
    assert(!strcmp(last_items[8].id, "settings:update:install") && !strcmp(last_items[8].title, "Update to 01.000.010 now") && strstr(last_items[8].subtitle, "Downloads 1 MB"));
    assert(!strcmp(last_items[9].id, "settings:update:skip") && !strcmp(last_items[9].title, "Skip version 01.000.010"));
    /* The notes open in the reader. */
    show("description:update");
    assert(last_count == 1 && !strcmp(last_items[0].id, "description:text:update") && !strcmp(last_items[0].title, "What is new in ProsperoTube 0.2.0"));
    assert(!strcmp(last_items[0].overview, "Fixed\n- Search works again") && !strcmp(tube_comment_text(last_items[0].id), "Fixed\n- Search works again"));
    show("description:text:update"); assert(last_count == 1 && !strcmp(last_items[0].id, "account:notice"));
    /* A check the user asks for goes out again and reports. */
    show("settings:update:check"); settle(update);
    assert(g_fetches == 2 && !tube_update_app_take_notice());   /* still the same newer version: About says it */
    snprintf(g_manifest, sizeof g_manifest, "[]");
    show("settings:update:check"); frames("ProsperoTube is up to date", NULL);
    show("settings:about"); assert(last_count == 7 && g_fetches == 3);
    snprintf(g_manifest, sizeof g_manifest, "{");
    show("settings:update:check"); frames("could not be read", NULL);
    show("settings:about"); assert(strstr(last_items[5].subtitle, "could not be read"));
    snprintf(g_manifest, sizeof g_manifest,
        "[{\"tag_name\":\"v01.000.010\",\"draft\":false,\"name\":\"ProsperoTube 0.2.0\",\"body\":\"\","
        "\"assets\":[{\"name\":\"PPSA99142.zip\",\"state\":\"uploaded\",\"size\":%zu,\"browser_download_url\":\"http://fixture/v01.000.010/PPSA99142.zip\",\"digest\":\"sha256:%s\"}]}]",
        g_zip_size, sha);
    show("settings:update:check"); settle(update); show("settings:about");
    assert(last_count == 9 && find("description:update") < 0 && find("settings:update:install") == 7);   /* a release without notes has no notes row */
    /* Skip, and unskip. */
    show("settings:update:skip"); assert(!strcmp(last_items[8].title, "Version 01.000.010 is skipped - select to be told again"));
    show("settings:update:skip"); assert(!strcmp(last_items[8].title, "Skip version 01.000.010"));
    /* Update now: progress rows while it runs, then the restart row and a notice. */
    int redraws = 0;
    show("settings:update:install");
    for (int i = 0; i < 4000; ++i) {
        tube_poll();
        redraws += tube_update_app_take_refresh("settings:update:cancel");
        const char *shown = tube_update_app_take_notice();
        if (shown) { assert(strstr(shown, "The update is ready")); break; }
        usleep(2000);
    }
    assert(redraws >= 1 && !tube_update_app_take_refresh("link:about") && !tube_update_app_take_refresh("settings:update:apply"));
    show("settings:about");
    assert(last_count == 9 && !strcmp(last_items[7].id, "settings:update:apply") && !strcmp(last_items[7].title, "Restart to finish updating to 01.000.010"));
    assert(!strcmp(last_items[8].id, "settings:update:discard") && !tube_update_app_take_restart());
    snprintf(path, sizeof path, "%s/eboot.bin", g_install);
    stored = tube_update_read(path, 64, NULL); assert(stored && !strcmp(stored, "old program")); free(stored);
    /* Restart: the files are replaced and the host is asked to close the app. */
    show("settings:update:apply");
    assert(tube_update_app_take_restart() && !tube_update_app_take_restart() && strstr(tube_update_app_take_notice(), "closes now"));
    assert(!strcmp(last_items[7].title, "Close ProsperoTube to finish the update"));
    snprintf(path, sizeof path, "%s/sce_sys/param.json", g_install);
    stored = tube_update_read(path, 4096, NULL); assert(stored && strstr(stored, "01.000.010")); free(stored);
    p->shutdown(); tube_update_free(update);

    /* The new version's first run: confirmed once it has run for a while, then the previous files go. */
    assert(!p->init());
    update = engine(0); tube_update_app_use(update);
    snprintf(path, sizeof path, "%s/update/previous/eboot.bin", g_dir);
    for (int i = 0; i < 20; ++i) { tube_poll(); assert(!tube_update_app_take_notice()); }
    assert(tube_update_kind(path) == 1);
    g_clock += 11; frames("ProsperoTube was updated to version 01.000.010", NULL);
    assert(!tube_update_kind(path));
    show("settings:about"); assert(last_count == 7 && !strcmp(last_items[5].subtitle, "Installed 01.000.010 is the latest version"));
    tube_update_app_use(NULL); tube_update_free(update);

    /* A development build lists the newer version and says why it stays as it is. */
    snprintf(path, sizeof path, "%s/sce_sys/param.json", g_install); write_text(path, "{\"titleId\":\"PPSA99142\",\"contentVersion\":\"01.000.000\"}");
    update = engine(1); tube_update_app_use(update);
    frames(NULL, "settings:update:blocked");
    const int row = find("settings:update:blocked");
    assert(!strcmp(last_items[row].title, "This copy does not update itself") && strstr(last_items[row].subtitle, "development build") && find("settings:update:install") < 0);
    show("settings:update:install"); show("settings:update:apply"); (void)tube_update_app_take_notice();
    assert(!tube_update_app_take_restart());
    snprintf(path, sizeof path, "%s/update/stage", g_dir); assert(!tube_update_kind(path));
    tube_update_app_use(NULL); tube_update_free(update);

    p->shutdown(); free(g_zip);
    snprintf(path, sizeof path, "%s", g_dir); assert(tube_update_remove(path) == 0);
    puts("PASS: About lists Check for updates, Notify about updates, release notes, skip, update, restart and the confirmed new version; a development build is told why not");
    return 0;
}
