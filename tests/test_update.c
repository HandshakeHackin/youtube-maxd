// The update engine against a loopback server: versions, release metadata, the
// daily check, download with resume and cancel, staging, the replacement with a
// failure injected at every step, rollback, and the copies that never update.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tube_update.h"
#include "tube_update_fs.h"
#include "tube_update_sha256.h"
#include "tube_update_zip.h"
#include "tube_js_http.h"

#include "cJSON.h"

#include <assert.h>
#include <stdarg.h>

#define OLD "01.000.000"
#define NEW "01.000.010"

static const char *g_root, *g_base;            /* fixture folder and "http://127.0.0.1:port" */
static char g_case[512], g_data[600], g_install[600], g_registered[600], g_prefix[128];
static int g_cases;
static tube_update_http *g_http;
static long long g_now = 1791727380LL, g_free = -1, g_free_later = -1;
static int g_free_calls;
static tube_update *g_engine;

/* ---- Fault injection ---- */
static char g_fault_step[40], g_steps[64][40];
static int g_fault_kind, g_fault_skip, g_step_count, g_cancel_at_chunk;

static int fault(const char *step) {
    if (g_step_count < 64) snprintf(g_steps[g_step_count++], sizeof g_steps[0], "%s", step);
    if (g_cancel_at_chunk && !strcmp(step, "download:chunk") && !--g_cancel_at_chunk) tube_update_cancel(g_engine);
    if (!*g_fault_step || strcmp(step, g_fault_step)) return TUBE_UPDATE_FAULT_NONE;
    if (g_fault_skip-- > 0) return TUBE_UPDATE_FAULT_NONE;
    g_fault_step[0] = 0;
    return g_fault_kind;
}
static void arm(const char *step, int kind, int skip) { snprintf(g_fault_step, sizeof g_fault_step, "%s", step); g_fault_kind = kind; g_fault_skip = skip; }
static long long fake_now(void) { return g_now; }
static long long fake_free(const char *path) { (void)path; return g_free_calls++ ? (g_free_later >= 0 ? g_free_later : g_free) : g_free; }

/* ---- Files ---- */
static const char *in(const char *folder, const char *leaf) {
    static char paths[8][TUBE_UPDATE_PATH];
    static int slot;
    slot = (slot + 1) % 8;
    snprintf(paths[slot], sizeof paths[slot], "%s/%s", folder, leaf);
    return paths[slot];
}
static void put(const char *path, const char *body) {
    char parent[TUBE_UPDATE_PATH];
    snprintf(parent, sizeof parent, "%s", path); *strrchr(parent, '/') = 0;
    assert(tube_update_mkdirs(g_root, parent) == 0);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(body, 1, strlen(body), file) == strlen(body) && fclose(file) == 0);
}
static int holds(const char *path, const char *body) {
    char *text = tube_update_read(path, 1 << 20, NULL);
    const int same = text && !strcmp(text, body);
    free(text);
    return same;
}
static int compare_names(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
/* A digest of a whole tree: names, kinds and contents, in name order. */
static void tree_add(tube_update_sha256 *sha, const char *path, const char *relative) {
    char (*names)[256] = calloc(256, 256);
    int count = 0;
    DIR *dir = opendir(path);
    assert(dir && names);
    for (struct dirent *item; (item = readdir(dir)) != NULL && count < 256;)
        if (strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) snprintf(names[count++], 256, "%s", item->d_name);
    closedir(dir);
    qsort(names, (size_t)count, 256, compare_names);
    for (int i = 0; i < count; ++i) {
        char child[TUBE_UPDATE_PATH], name[TUBE_UPDATE_PATH];
        snprintf(child, sizeof child, "%s/%s", path, names[i]); snprintf(name, sizeof name, "%s/%s", relative, names[i]);
        const int kind = tube_update_kind(child);
        tube_update_sha256_add(sha, name, strlen(name) + 1); tube_update_sha256_add(sha, &kind, sizeof kind);
        if (kind == 2) tree_add(sha, child, name);
        else {
            size_t length = 0;
            char *body = tube_update_read(child, 8 << 20, &length);
            assert(body); tube_update_sha256_add(sha, body, length); free(body);
        }
    }
    free(names);
}
static const char *tree(const char *path) {
    static char digests[4][65];
    static int slot;
    tube_update_sha256 sha;
    slot = (slot + 1) % 4;
    tube_update_sha256_init(&sha); tree_add(&sha, path, ""); tube_update_sha256_finish(&sha, digests[slot]);
    return digests[slot];
}
static void write_param(const char *folder, const char *version, const char *title) {
    char body[256];
    snprintf(body, sizeof body, "{\"titleId\":\"%s\",\"contentVersion\":\"%s\"}", title, version);
    put(in(folder, "sce_sys/param.json"), body);
}
/* A new sandbox: an installed 01.000.000 with files a release replaces and files it does not know. */
static void fresh(void) {
    snprintf(g_case, sizeof g_case, "%s/run-%d/case-%d", g_root, (int)getpid(), ++g_cases);   /* the script runs two builds on one server */
    snprintf(g_data, sizeof g_data, "%s/data", g_case); snprintf(g_install, sizeof g_install, "%s/homebrew/PPSA99142", g_case);
    snprintf(g_registered, sizeof g_registered, "%s/user", g_case);
    assert(tube_update_mkdirs(g_root, g_data) == 0);
    put(in(g_install, "eboot.bin"), "old program");
    put(in(g_install, "lapy.elf"), "old helper");
    write_param(g_install, OLD, "PPSA99142");
    put(in(g_install, "sce_sys/icon0.png"), "old icon");
    put(in(g_install, "assets/old.txt"), "an asset the new version dropped");
    put(in(g_install, "user-note.txt"), "a file the user put here");
    put(in(g_install, "old-only/keep.txt"), "a folder no release names");
    g_free = g_free_later = -1; g_free_calls = 0; g_fault_step[0] = 0; g_step_count = 0; g_cancel_at_chunk = 0;
    unlink(in(g_root, "mode"));
}

/* ---- The server's side ---- */
static int requests(void) {
    char *log = tube_update_read(in(g_root, "requests.log"), 8 << 20, NULL);
    int lines = 0;
    for (const char *p = log; p && *p; ++p) lines += *p == '\n';
    free(log);
    return lines;
}
/* The n-th request from the end (0 = the last), parsed. */
static cJSON *request(int back) {
    char *log = tube_update_read(in(g_root, "requests.log"), 8 << 20, NULL);
    assert(log);
    char *end = log + strlen(log);
    for (int i = 0; i <= back; ++i) { assert(end > log); *--end = 0; while (end > log && end[-1] != '\n') --end; }
    cJSON *parsed = cJSON_Parse(end);
    free(log); assert(parsed);
    return parsed;
}
static const char *header(const cJSON *entry, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(entry, "headers"), name);
    return cJSON_IsString(value) ? value->valuestring : "";
}
static void server_mode(const char *mode) { put(in(g_root, "mode"), mode); }
typedef struct { char name[40], sha256[65]; unsigned long long size; } fixture;
static fixture g_fixtures[24];
static const fixture *archive(const char *name) {
    for (int i = 0; i < 24; ++i) if (!strcmp(g_fixtures[i].name, name)) return &g_fixtures[i];
    assert(!"fixture"); return NULL;
}
static const char *archive_path(const char *name) { static char path[700]; snprintf(path, sizeof path, "%s/www/%s", g_root, name); return path; }
/* A releases answer naming `zip` (through the server's `kind` path) as version `tag`'s file. */
static const char *manifest(const char *file, const char *tag, const char *kind, const char *zip, const char *sha256, unsigned long long size) {
    static char url[300];
    char body[4096];
    snprintf(body, sizeof body,
        "[{\"tag_name\":\"v09.000.000\",\"draft\":true,\"name\":\"A draft\",\"assets\":[]},"
        "{\"tag_name\":\"build-cache\",\"draft\":false,\"name\":\"Compiler cache\",\"assets\":[]},"
        "{\"tag_name\":\"v01.000.005\",\"draft\":false,\"prerelease\":false,\"name\":\"Older\",\"html_url\":\"https://github.com/blackbearreloaded/ProsperoTube/releases/tag/v01.000.005\",\"body\":\"old\",\"assets\":[]},"
        "{\"tag_name\":\"%s\",\"draft\":false,\"prerelease\":true,\"name\":\"ProsperoTube 0.2.0\","
        "\"html_url\":\"https://github.com/blackbearreloaded/ProsperoTube/releases/tag/%s\","
        "\"body\":\"## Fixed\\r\\n- **Search** works again\\r\\n- See [the notes](https://example.org/x)\\r\\n\\r\\nThanks.\","
        "\"assets\":[{\"name\":\"SHA256SUMS\",\"state\":\"uploaded\",\"size\":80,\"browser_download_url\":\"%s/files/SHA256SUMS\",\"digest\":null},"
        "{\"name\":\"PPSA99142.zip\",\"state\":\"uploaded\",\"size\":%llu,\"browser_download_url\":\"%s/%s/%s\",\"digest\":\"sha256:%s\"}]}]",
        tag, tag, g_base, size, g_base, kind, zip, sha256);
    put(archive_path(file), body);
    snprintf(url, sizeof url, "%s/files/%s", g_base, file);
    return url;
}
static const char *good_manifest(const char *kind) {
    const fixture *good = archive("good.zip");
    return manifest("releases.json", "v" NEW, kind, "good.zip", good->sha256, good->size);
}

/* ---- Engines ---- */
static tube_update_config settings(const char *manifest_url) {
    tube_update_config config = {.data_dir = g_data, .install_dir = g_install, .running_dir = g_install, .manifest_url = manifest_url,
                                 .download_prefix = g_prefix, .any_host = 1, .chunk_bytes = 65536, .now = fake_now, .free_bytes = fake_free, .fault = fault};
    config.transport = tube_update_http_transport(g_http);
    return config;
}
static tube_update *engine(const char *manifest_url) {
    tube_update_config config = settings(manifest_url);
    g_engine = tube_update_new(&config);
    assert(g_engine);
    return g_engine;
}
static tube_update_status state(tube_update *u) { tube_update_status status; tube_update_get(u, &status); return status; }
static void wait_idle(tube_update *u) {
    for (int i = 0; i < 3000 && state(u).busy; ++i) usleep(10000);
    assert(!state(u).busy);
}
/* Checks, downloads and stages the good release; leaves the engine STAGED. */
static tube_update *staged(void) {
    tube_update *u = engine(good_manifest("files"));
    tube_update_run_check(u, 1); assert(state(u).check == TUBE_UPDATE_AVAILABLE);
    tube_update_run_download(u);
    if (state(u).phase != TUBE_UPDATE_STAGED) fprintf(stderr, "not staged: %s\n", state(u).error);
    assert(state(u).phase == TUBE_UPDATE_STAGED && !strcmp(state(u).staged, NEW));
    return u;
}
static int installed_is_new(void) {
    char *body = tube_update_read(in(g_install, "sce_sys/param.json"), 4096, NULL);
    const int is_new = body && strstr(body, NEW) != NULL;
    free(body);
    return is_new && holds(in(g_install, "assets/new.txt"), "new in " NEW "\n") && !tube_update_kind(in(g_install, "assets/old.txt")) &&
           holds(in(g_install, "user-note.txt"), "a file the user put here") && holds(in(g_install, "old-only/keep.txt"), "a folder no release names");
}

/* ---- Cases ---- */
static void versions(void) {
    unsigned parts[3];
    assert(tube_update_version_parse("01.000.010", parts) && parts[0] == 1 && parts[1] == 0 && parts[2] == 10);
    assert(tube_update_version_parse("v1.2.3", parts) && parts[2] == 3 && tube_update_version_parse("V0.1.0", parts));
    const char *bad[] = {"", "v", "1", "1.2", "1.2.3.4", "1.2.x", "1..3", "01.000.010 ", " 1.2.3", "1.2.-3", "build-cache", "1234567.0.0", "1.2.3beta", NULL};
    for (int i = 0; bad[i]; ++i) assert(!tube_update_version_parse(bad[i], parts));
    assert(!tube_update_version_parse(NULL, parts));
    assert(tube_update_version_compare("01.000.010", "01.000.000") > 0 && tube_update_version_compare("01.000.000", "01.000.010") < 0);
    assert(tube_update_version_compare("v1.0.10", "01.000.010") == 0 && tube_update_version_compare("01.001.000", "01.000.990") > 0);
    assert(tube_update_version_compare("02.000.000", "01.999.999") > 0 && tube_update_version_compare("1.0.9", "1.0.10") < 0);
    assert(tube_update_version_compare("junk", "1.0.0") < 0 && tube_update_version_compare("1.0.0", "junk") > 0 && tube_update_version_compare("junk", "") == 0);
}

static void digests(void) {
    tube_update_sha256 sha;
    char hex[65], block[1000];
    tube_update_sha256_init(&sha); tube_update_sha256_finish(&sha, hex);
    assert(!strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    tube_update_sha256_init(&sha); tube_update_sha256_add(&sha, "abc", 3); tube_update_sha256_finish(&sha, hex);
    assert(!strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    tube_update_sha256_init(&sha);
    tube_update_sha256_add(&sha, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56); tube_update_sha256_finish(&sha, hex);
    assert(!strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    memset(block, 'a', sizeof block); tube_update_sha256_init(&sha);   /* a million 'a', in uneven pieces */
    for (int left = 1000000, step = 1; left; step = step % 997 + 1) { const int take = step < left ? step : left; tube_update_sha256_add(&sha, block, (size_t)take); left -= take; }
    tube_update_sha256_finish(&sha, hex);
    assert(!strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

static void notes(void) {
    char out[200], small[24];
    assert(!tube_update_plain_notes("## What's new\r\n\r\n* **Bold** and `code`\r\n- [link](https://x.example/y) text\r\n> [!WARNING]\r\n> Careful.\r\n\r\n\r\n### End\r\nBye\x01.\r\n", out, sizeof out));
    assert(!strcmp(out, "What's new\n\n- Bold and code\n- link text\n[!WARNING]\nCareful.\n\nEnd\nBye."));
    assert(!tube_update_plain_notes("", out, sizeof out) && !*out && !tube_update_plain_notes(NULL, out, sizeof out) && !*out);
    assert(tube_update_plain_notes("A line that is much longer than the buffer\nmore", small, sizeof small) == 1 && strlen(small) < sizeof small);
    assert(tube_update_plain_notes("caf\xc3\xa9 caf\xc3\xa9 caf\xc3\xa9 caf\xc3\xa9 caf\xc3\xa9 x", small, 9) == 1 && !strcmp(small, "caf\xc3\xa9 ca"));
    assert(tube_update_plain_notes("abcdefg\xc3\xa9", small, 9) == 1 && !strcmp(small, "abcdefg"));   /* never half a character */
}

static int parse(const char *json, tube_update_offer *offer) { return tube_update_manifest_parse(json, strlen(json), "https://github.com/blackbearreloaded/ProsperoTube/releases/download/", offer); }
#define ASSET(size, url, digest) "{\"name\":\"PPSA99142.zip\",\"state\":\"uploaded\",\"size\":" size ",\"browser_download_url\":\"" url "\",\"digest\":" digest "}"
#define URL "https://github.com/blackbearreloaded/ProsperoTube/releases/download/v01.000.010/PPSA99142.zip"
#define SHA "\"sha256:0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef\""
#define RELEASE(tag, assets) "{\"tag_name\":\"" tag "\",\"draft\":false,\"name\":\"N " tag "\",\"html_url\":\"https://github.com/blackbearreloaded/ProsperoTube/releases/tag/" tag "\",\"body\":\"- one\",\"assets\":[" assets "]}"
static void manifests(void) {
    static tube_update_offer offer;
    /* The highest version wins whatever the order; drafts and other tags are passed over; a pre-release counts. */
    assert(parse("[" RELEASE("v01.000.005", "") "," RELEASE("v01.000.010", ASSET("1234", URL, SHA)) "," RELEASE("build-cache", "") ","
                 "{\"tag_name\":\"v02.000.000\",\"draft\":true,\"assets\":[]}," RELEASE("01.000.009", "") "]", &offer) == TUBE_UPDATE_MANIFEST_OK);
    assert(!strcmp(offer.version, "01.000.010") && !strcmp(offer.name, "N v01.000.010") && offer.size == 1234 && !strcmp(offer.url, URL));
    assert(!strcmp(offer.sha256, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") && !strcmp(offer.notes, "- one"));
    assert(!strcmp(offer.page, "https://github.com/blackbearreloaded/ProsperoTube/releases/tag/v01.000.010"));
    /* Malformed answers. */
    const char *invalid[] = {"", "{", "null", "{\"message\":\"Not Found\"}", "[1]", "[\"x\"]", "[{\"tag_name\":\"v01.000.010\"}", NULL};
    for (int i = 0; invalid[i]; ++i) assert(parse(invalid[i], &offer) == TUBE_UPDATE_MANIFEST_INVALID && !*offer.version);
    /* No release that is a version. */
    assert(parse("[]", &offer) == TUBE_UPDATE_MANIFEST_NONE && parse("[" RELEASE("build-cache", "") ",{\"tag_name\":7},{}]", &offer) == TUBE_UPDATE_MANIFEST_NONE);
    /* The newest release without a usable file: named, never installed. An older installable one is not offered instead. */
    const char *unusable[] = {
        RELEASE("v01.000.010", ""),                                                                 /* no asset */
        RELEASE("v01.000.010", "{\"name\":\"other.zip\",\"size\":5,\"browser_download_url\":\"" URL "\",\"digest\":" SHA "}"),
        RELEASE("v01.000.010", ASSET("1234", URL, "null")),                                         /* no digest */
        RELEASE("v01.000.010", ASSET("1234", URL, "\"sha256:0123\"")),                              /* short */
        RELEASE("v01.000.010", ASSET("1234", URL, "\"sha1:0123456789abcdef0123456789abcdef01234567\"")),
        RELEASE("v01.000.010", ASSET("1234", URL, "\"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg\"")),
        RELEASE("v01.000.010", ASSET("0", URL, SHA)), RELEASE("v01.000.010", ASSET("-5", URL, SHA)), RELEASE("v01.000.010", ASSET("1.5", URL, SHA)),
        RELEASE("v01.000.010", ASSET("9999999999999", URL, SHA)), RELEASE("v01.000.010", ASSET("\"1234\"", URL, SHA)),
        RELEASE("v01.000.010", ASSET("1234", "https://example.org/PPSA99142.zip", SHA)),            /* another host */
        RELEASE("v01.000.010", ASSET("1234", "http://github.com/blackbearreloaded/ProsperoTube/releases/download/x/PPSA99142.zip", SHA)),
        RELEASE("v01.000.010", ASSET("1234", "https://github.com/someone/else/releases/download/x/PPSA99142.zip", SHA)),
        RELEASE("v01.000.010", ASSET("1234", URL " x", SHA)),
        RELEASE("v01.000.010", "{\"name\":\"PPSA99142.zip\",\"state\":\"starter\",\"size\":1234,\"browser_download_url\":\"" URL "\",\"digest\":" SHA "}"),
        NULL};
    for (int i = 0; unusable[i]; ++i) {
        char json[2048];
        snprintf(json, sizeof json, "[%s,%s]", unusable[i], RELEASE("v01.000.005", ASSET("1234", URL, SHA)));
        if (parse(json, &offer) != TUBE_UPDATE_MANIFEST_NOT_INSTALLABLE) { fprintf(stderr, "accepted: %s\n", unusable[i]); assert(0); }
        assert(!strcmp(offer.version, "01.000.010") && !*offer.url && !*offer.sha256 && !offer.size && *offer.page);
    }
    /* A page that is not GitHub's is dropped; control characters never reach a title. */
    assert(parse("[{\"tag_name\":\"v1.0.0\",\"name\":\"a\\nb\",\"html_url\":\"https://evil.example/x\",\"assets\":[]}]", &offer) == TUBE_UPDATE_MANIFEST_NOT_INSTALLABLE);
    assert(!*offer.page && !strcmp(offer.name, "a b") && !strcmp(offer.version, "1.0.0"));
}

static void archives(void) {
    char error[160], stage[700];
    const char *refused[] = {"wrongtitle.zip", "traversal.zip", "absolute.zip", "outside.zip", "backslash.zip", "symlink.zip", "duplicate.zip",
                             "encrypted.zip", "notazip.zip", NULL};
    fresh();
    for (int i = 0; refused[i]; ++i) {
        error[0] = 0;
        tube_update_zip *zip = tube_update_zip_open(archive_path(refused[i]), "PPSA99142", TUBE_UPDATE_MAX_UNPACKED, error, sizeof error);
        if (zip) { fprintf(stderr, "opened %s\n", refused[i]); assert(0); }
        assert(*error);
    }
    assert(!tube_update_zip_open(in(g_root, "www/missing.zip"), "PPSA99142", TUBE_UPDATE_MAX_UNPACKED, error, sizeof error));
    assert(!tube_update_zip_open(archive_path("good.zip"), "PPSA99142", 1000, error, sizeof error) && strstr(error, "more than"));
    tube_update_zip *zip = tube_update_zip_open(archive_path("good.zip"), "PPSA99142", TUBE_UPDATE_MAX_UNPACKED, error, sizeof error);
    assert(zip && tube_update_zip_files(zip) == 7 && tube_update_zip_bytes(zip) > 400000);
    snprintf(stage, sizeof stage, "%s/stage", g_case);
    error[0] = 0;
    const int unpacked = tube_update_zip_unpack(zip, stage, NULL, NULL, error, sizeof error) == 0 && tube_update_zip_verify(zip, stage, error, sizeof error) == 0;
    if (!unpacked) fprintf(stderr, "unpack: %s\n", error);
    assert(unpacked);
    struct stat info;   /* every file and folder open to all, as the console wants an app */
    const char *open_to_all[] = {"eboot.bin", "lapy.elf", "sce_sys", "sce_sys/param.json", "assets", "assets/fonts", "assets/fonts/a.ttf", "assets/deep/er", "assets/deep/er/empty.txt", NULL};
    for (int i = 0; open_to_all[i]; ++i) assert(stat(in(stage, open_to_all[i]), &info) == 0 && (info.st_mode & 0777) == 0777);
    assert(holds(in(stage, "assets/new.txt"), "new in " NEW "\n") && info.st_size == 0);
    /* Reading back notices a changed, a missing and an extra file. */
    put(in(stage, "assets/new.txt"), "new in 01.000.011\n"); chmod(in(stage, "assets/new.txt"), 0777);
    assert(tube_update_zip_verify(zip, stage, error, sizeof error) != 0 && strstr(error, "match"));
    put(in(stage, "assets/new.txt"), "new in " NEW "\n"); chmod(in(stage, "assets/new.txt"), 0777);
    assert(tube_update_zip_verify(zip, stage, error, sizeof error) == 0);
    put(in(stage, "assets/extra.txt"), "x");
    assert(tube_update_zip_verify(zip, stage, error, sizeof error) != 0 && strstr(error, "archive does not"));
    unlink(in(stage, "assets/extra.txt")); unlink(in(stage, "lapy.elf"));
    assert(tube_update_zip_verify(zip, stage, error, sizeof error) != 0 && strstr(error, "missing"));
    /* Nothing is written over what is already there. */
    assert(tube_update_zip_unpack(zip, stage, NULL, NULL, error, sizeof error) != 0);
    tube_update_zip_close(zip);
    assert(tube_update_remove(stage) == 0 && !tube_update_kind(stage));
    /* Damaged data passes the directory checks and fails while unpacking. */
    zip = tube_update_zip_open(archive_path("corrupt.zip"), "PPSA99142", TUBE_UPDATE_MAX_UNPACKED, error, sizeof error);
    assert(zip && tube_update_zip_unpack(zip, stage, NULL, NULL, error, sizeof error) != 0 && strstr(error, "damaged"));
    tube_update_zip_close(zip);
}

static void checking(void) {
    fresh();
    const int before = requests();
    tube_update *u = engine(good_manifest("files"));
    tube_update_status s = state(u);
    assert(s.check == TUBE_UPDATE_UNKNOWN && !strcmp(s.installed, OLD) && s.notify && !s.blocked && !s.checked_at);
    assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_NONE && !tube_update_take_offer(u));
    tube_update_run_check(u, 0);
    s = state(u);
    assert(s.check == TUBE_UPDATE_AVAILABLE && !strcmp(s.latest, NEW) && !strcmp(s.latest_name, "ProsperoTube 0.2.0") && s.checked_at == g_now && !*s.error);
    assert(s.size == archive("good.zip")->size && strstr(s.page, "/releases/tag/v" NEW) && !s.from_cache && s.phase == TUBE_UPDATE_IDLE);
    assert(!strcmp(tube_update_notes(u), "Fixed\n- Search works again\n- See the notes\n\nThanks.") && !tube_update_notes_truncated(u));
    assert(requests() == before + 1);
    /* Privacy: one request, a generic user agent, nothing that names the console, the user or the installed version. */
    cJSON *sent = request(0);
    const cJSON *headers = cJSON_GetObjectItemCaseSensitive(sent, "headers"), *one;
    assert(!strcmp(header(sent, "user-agent"), "ProsperoTube-update") && !strcmp(header(sent, "accept"), "application/vnd.github+json"));
    cJSON_ArrayForEach(one, headers) {
        const char *allowed[] = {"host", "user-agent", "accept", "accept-encoding", NULL};
        int known = 0;
        for (int i = 0; allowed[i]; ++i) known |= !strcmp(one->string, allowed[i]);
        if (!known) { fprintf(stderr, "sent header %s\n", one->string); assert(0); }
    }
    assert(!strstr(cJSON_GetObjectItemCaseSensitive(sent, "path")->valuestring, "?"));
    cJSON_Delete(sent);
    /* The notice: once per run. */
    assert(tube_update_take_offer(u) && !tube_update_take_offer(u));
    /* At most once a day on its own; a check the user asks for always goes out. */
    g_now += 3600; tube_update_run_check(u, 0); assert(requests() == before + 1 && state(u).checked_at == g_now - 3600);
    tube_update_run_check(u, 1); assert(requests() == before + 2 && state(u).checked_at == g_now);
    g_now += 86399; tube_update_run_check(u, 0); assert(requests() == before + 2);
    g_now += 1; tube_update_run_check(u, 0); assert(requests() == before + 3);
    g_now -= 500000; tube_update_run_check(u, 0); assert(requests() == before + 4);   /* a clock that went back does not silence the check */
    tube_update_free(u);
    /* The answer is kept: the next run knows it without asking, and tells once. */
    u = engine(good_manifest("files"));
    s = state(u);
    assert(s.check == TUBE_UPDATE_AVAILABLE && s.from_cache && !strcmp(s.latest, NEW) && s.checked_at == g_now && *tube_update_notes(u));
    tube_update_run_check(u, 0); assert(requests() == before + 4 && tube_update_take_offer(u) && !tube_update_take_offer(u));
    /* "Notify about updates" off, and "skip this version", both silence the notice and survive a restart. */
    tube_update_set_notify(u, 0); tube_update_free(u);
    u = engine(good_manifest("files")); assert(!state(u).notify && !tube_update_take_offer(u));
    tube_update_set_notify(u, 1); tube_update_skip(u, NEW); tube_update_free(u);
    u = engine(good_manifest("files")); assert(state(u).notify && !strcmp(state(u).skipped, NEW) && !tube_update_take_offer(u));
    tube_update_skip(u, ""); assert(tube_update_take_offer(u));
    tube_update_skip(u, "not a version"); assert(!*state(u).skipped);
    /* Failures: silent on its own, told when the user asked. The stored answer stays. */
    g_now += 200000;
    server_mode("down");
    tube_update_run_check(u, 0); s = state(u);
    assert(!*s.error && s.check == TUBE_UPDATE_AVAILABLE && s.phase == TUBE_UPDATE_IDLE && s.checked_at != g_now);
    tube_update_run_check(u, 1); s = state(u);
    assert(strstr(s.error, "could not be reached") && s.check == TUBE_UPDATE_AVAILABLE && s.phase == TUBE_UPDATE_IDLE);
    server_mode("up");
    tube_update_free(u);
    const struct { const char *url, *message; } failures[] = {
        {"/status/404/releases.json", "No releases were found"}, {"/status/403/releases.json", "too many requests"},
        {"/status/500/releases.json", "status 500"}, {"/files/good.zip", "could not be read"}, {"/files/absent.json", "No releases"}};
    for (unsigned i = 0; i < sizeof failures / sizeof *failures; ++i) {
        char url[300];
        fresh(); snprintf(url, sizeof url, "%s%s", g_base, failures[i].url);
        u = engine(url);
        tube_update_run_check(u, 0); assert(!*state(u).error && state(u).check == TUBE_UPDATE_UNKNOWN && !state(u).checked_at);
        tube_update_run_check(u, 1);
        if (!strstr(state(u).error, failures[i].message)) { fprintf(stderr, "%s: %s\n", failures[i].url, state(u).error); assert(0); }
        assert(!tube_update_take_offer(u));
        tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_IDLE);   /* nothing to download */
        tube_update_free(u);
    }
    /* Up to date; and a newer release that cannot be installed is named, not downloaded. */
    fresh(); write_param(g_install, NEW, "PPSA99142");
    u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    assert(state(u).check == TUBE_UPDATE_UP_TO_DATE && !*state(u).error && !tube_update_take_offer(u));
    tube_update_free(u);
    fresh();
    u = engine(manifest("unusable.json", "v" NEW, "files", "good.zip", "sha256-missing", 5)); tube_update_run_check(u, 1);
    s = state(u); assert(s.check == TUBE_UPDATE_NOT_INSTALLABLE && !strcmp(s.latest, NEW) && *s.page && tube_update_take_offer(u));
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_IDLE && !tube_update_kind(in(g_data, "update/download.zip")));
    tube_update_free(u);
    /* Without the test switch only GitHub's hosts are asked, over HTTPS. */
    fresh();
    tube_update_config strict = settings(good_manifest("files")); strict.any_host = 0;
    const int asked = requests();
    u = tube_update_new(&strict); tube_update_run_check(u, 1);
    assert(strstr(state(u).error, "not one of GitHub's") && requests() == asked);
    tube_update_free(u);
}

static void downloading(void) {
    const fixture *good = archive("good.zip");
    char original[65];
    /* The whole path, in several ranges. */
    fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
    int before = requests();
    tube_update *u = staged();
    tube_update_status s = state(u);
    const int chunks = (int)((good->size + 65535) / 65536);
    assert(chunks >= 3 && requests() == before + 1 + chunks && s.total == s.done && !*s.error);
    for (int i = 0; i < chunks; ++i) {
        char range[80];
        cJSON *sent = request(chunks - 1 - i);
        const unsigned long long first = (unsigned long long)i * 65536, last = first + 65535 < good->size ? first + 65535 : good->size - 1;
        snprintf(range, sizeof range, "bytes=%llu-%llu", first, last);
        assert(!strcmp(header(sent, "range"), range) && !strcmp(header(sent, "user-agent"), "ProsperoTube-update") && !*header(sent, "cookie"));
        cJSON_Delete(sent);
    }
    assert(!strcmp(tree(g_install), original));   /* staging changes nothing that is installed */
    assert(holds(in(g_data, "update/stage/assets/new.txt"), "new in " NEW "\n") && tube_update_kind(in(g_data, "update/state.json")) == 1);
    assert(!tube_update_kind(in(g_data, "update/download.zip")) && !tube_update_kind(in(g_data, "update/download.json")));
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_STAGED);   /* nothing to do twice */
    tube_update_discard(u);
    assert(state(u).phase == TUBE_UPDATE_IDLE && !tube_update_kind(in(g_data, "update/stage")) && !tube_update_kind(in(g_data, "update/state.json")));
    tube_update_free(u);
    /* Through a redirect (relative and absolute), from a server without ranges, and past a server that fails once per piece. */
    const char *routes[] = {"redirect", "redirect-abs", "norange", "flaky"};
    for (int i = 0; i < 4; ++i) {
        fresh(); u = engine(good_manifest(routes[i]));
        tube_update_run_check(u, 1); tube_update_run_download(u);
        if (state(u).phase != TUBE_UPDATE_STAGED) { fprintf(stderr, "%s: %s\n", routes[i], state(u).error); assert(0); }
        tube_update_free(u);
    }
    /* What the release lists must be what arrives: digest and size. */
    fresh(); u = engine(manifest("badhash.json", "v" NEW, "files", "good.zip", archive("corrupt.zip")->sha256, good->size));
    tube_update_run_check(u, 1); tube_update_run_download(u); s = state(u);
    assert(s.phase == TUBE_UPDATE_FAILED && strstr(s.error, "SHA-256") && !tube_update_kind(in(g_data, "update/download.zip")) &&
           !tube_update_kind(in(g_data, "update/download.json")) && !tube_update_kind(in(g_data, "update/stage")));
    tube_update_free(u);
    const unsigned long long sizes[] = {good->size - 1, good->size + 1, 65536, good->size + 70000};
    for (int i = 0; i < 4; ++i) {
        fresh(); u = engine(manifest("badsize.json", "v" NEW, i & 1 ? "norange" : "files", "good.zip", good->sha256, sizes[i]));
        tube_update_run_check(u, 1); tube_update_run_download(u); s = state(u);
        if (s.phase != TUBE_UPDATE_FAILED) { fprintf(stderr, "size %llu accepted\n", sizes[i]); assert(0); }
        assert(*s.error && !tube_update_kind(in(g_data, "update/stage")));
        tube_update_free(u);
    }
    /* A server error, and a release whose archive is refused or is another version. */
    fresh(); u = engine(manifest("gone.json", "v" NEW, "status/404", "good.zip", good->sha256, good->size));
    tube_update_run_check(u, 1); tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_FAILED && strstr(state(u).error, "status 404"));
    tube_update_free(u);
    const char *rejected[] = {"wrongversion.zip", "wrongtitle.zip", "traversal.zip", "symlink.zip", "corrupt.zip", "noeboot.zip", "notazip.zip"};
    for (unsigned i = 0; i < sizeof rejected / sizeof *rejected; ++i) {
        const fixture *bad = archive(rejected[i]);
        fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
        u = engine(manifest("rejected.json", "v" NEW, "files", rejected[i], bad->sha256, bad->size));
        tube_update_run_check(u, 1); tube_update_run_download(u); s = state(u);
        if (s.phase != TUBE_UPDATE_FAILED || !*s.error) { fprintf(stderr, "%s was staged\n", rejected[i]); assert(0); }
        assert(!tube_update_kind(in(g_data, "update/stage")) && !tube_update_kind(in(g_data, "update/state.json")) && !strcmp(tree(g_install), original));
        assert(!tube_update_apply(u));
        tube_update_free(u);
    }
    /* Not enough space: refused before the download, and again before unpacking. */
    fresh(); u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    g_free = 100000; before = requests();
    tube_update_run_download(u); s = state(u);
    assert(s.phase == TUBE_UPDATE_FAILED && strstr(s.error, "Not enough free space") && requests() == before && !tube_update_kind(in(g_data, "update/download.zip")));
    g_free = 1ll << 40; g_free_later = 300000; g_free_calls = 0;
    tube_update_run_download(u); s = state(u);
    assert(s.phase == TUBE_UPDATE_FAILED && strstr(s.error, "to unpack") && !tube_update_kind(in(g_data, "update/stage")));
    assert(tube_update_kind(in(g_data, "update/download.zip")) == 1);   /* the verified download is kept for the next try */
    g_free_later = -1; before = requests();
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_STAGED && requests() == before);   /* ... which downloads nothing again */
    tube_update_free(u);
}

static void resuming(void) {
    const fixture *good = archive("good.zip");
    char range[80];
    /* A failure in the third piece: two pieces stay, and the next try starts after them. */
    fresh();
    tube_update *u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    arm("download:chunk", TUBE_UPDATE_FAULT_ERROR, 2);
    tube_update_run_download(u);
    tube_update_status s = state(u);
    assert(s.phase == TUBE_UPDATE_FAILED && strstr(s.error, "continue") && s.done == 131072 && s.total == good->size);
    struct stat info;
    assert(stat(in(g_data, "update/download.zip"), &info) == 0 && info.st_size == 131072);
    int before = requests();
    tube_update_run_download(u);
    assert(state(u).phase == TUBE_UPDATE_STAGED);
    const int rest = (int)((good->size - 131072 + 65535) / 65536);
    assert(requests() == before + rest);
    cJSON *sent = request(rest - 1);
    snprintf(range, sizeof range, "bytes=131072-%llu", 131072ull + 65535 < good->size ? 131072ull + 65535 : good->size - 1);
    assert(!strcmp(header(sent, "range"), range)); cJSON_Delete(sent);
    tube_update_free(u);
    /* Power lost mid-download: the next run of the app continues the same file. */
    fresh(); u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    arm("download:chunk", TUBE_UPDATE_FAULT_STOP, 1);
    tube_update_run_download(u); tube_update_free(u);
    assert(stat(in(g_data, "update/download.zip"), &info) == 0 && info.st_size == 65536);
    u = engine(good_manifest("files"));
    assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_NONE && tube_update_kind(in(g_data, "update/download.zip")) == 1);
    before = requests(); tube_update_run_download(u);
    assert(state(u).phase == TUBE_UPDATE_STAGED && requests() == before + (int)((good->size - 65536 + 65535) / 65536));
    tube_update_free(u);
    /* A part file that was damaged on the disk fails the digest, is dropped, and the try after that starts clean. */
    fresh(); u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    arm("download:chunk", TUBE_UPDATE_FAULT_ERROR, 1);
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_FAILED);
    FILE *part = fopen(in(g_data, "update/download.zip"), "r+b");
    assert(part && fseek(part, 100, SEEK_SET) == 0 && fputc('!', part) != EOF && fclose(part) == 0);
    tube_update_run_download(u);
    assert(state(u).phase == TUBE_UPDATE_FAILED && strstr(state(u).error, "SHA-256") && !tube_update_kind(in(g_data, "update/download.zip")));
    before = requests(); tube_update_run_download(u);
    sent = request((int)((good->size + 65535) / 65536) - 1);
    assert(state(u).phase == TUBE_UPDATE_STAGED && !strncmp(header(sent, "range"), "bytes=0-", 8)); cJSON_Delete(sent);
    tube_update_free(u);
    /* A part file of another release is not continued. */
    fresh(); u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    arm("download:chunk", TUBE_UPDATE_FAULT_ERROR, 1);
    tube_update_run_download(u); tube_update_free(u);
    const fixture *other = archive("wrongversion.zip");
    u = engine(manifest("other.json", "v01.000.020", "files", "wrongversion.zip", other->sha256, other->size));
    tube_update_run_check(u, 1); tube_update_run_download(u);
    assert(state(u).phase == TUBE_UPDATE_STAGED && !strcmp(state(u).staged, "01.000.020"));
    tube_update_free(u);
    /* Cancel between pieces: no error, the part stays, the same row continues it. */
    fresh(); u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    g_cancel_at_chunk = 2;
    tube_update_run_download(u); s = state(u);
    assert(s.phase == TUBE_UPDATE_IDLE && !*s.error && stat(in(g_data, "update/download.zip"), &info) == 0 && info.st_size == 65536);
    assert(tube_update_download(u)); wait_idle(u);   /* this time on the worker */
    assert(state(u).phase == TUBE_UPDATE_STAGED);
    tube_update_free(u);
    /* Cancel while a slow piece is on the wire, from another thread. */
    fresh(); u = engine(good_manifest("slow")); tube_update_run_check(u, 1);
    assert(tube_update_download(u) && !tube_update_download(u) && !tube_update_check(u, 1));   /* one job at a time */
    for (int i = 0; i < 500 && state(u).phase != TUBE_UPDATE_DOWNLOADING; ++i) usleep(2000);
    usleep(150000); tube_update_cancel(u); wait_idle(u);
    s = state(u); assert(s.phase == TUBE_UPDATE_IDLE && !*s.error && s.done < s.total);
    tube_update_discard(u); assert(!tube_update_kind(in(g_data, "update/download.zip")));
    /* Freeing with a download in flight cancels it. */
    assert(tube_update_download(u)); usleep(100000);
    tube_update_free(u);
}

static void staging_failures(void) {
    const char *steps[] = {"verify", "unpack", "verify:files", "stage:commit"};
    char original[65];
    for (int kind = TUBE_UPDATE_FAULT_ERROR; kind <= TUBE_UPDATE_FAULT_STOP; ++kind)
        for (int i = 0; i < 4; ++i) {
            fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
            tube_update *u = engine(good_manifest("files")); tube_update_run_check(u, 1);
            arm(steps[i], kind, 0);
            tube_update_run_download(u);
            assert(state(u).phase == (kind == TUBE_UPDATE_FAULT_ERROR ? TUBE_UPDATE_FAILED : TUBE_UPDATE_IDLE) && !*g_fault_step);
            assert(!tube_update_apply(u) && !strcmp(tree(g_install), original) && !tube_update_kind(in(g_data, "update/state.json")));
            tube_update_free(u);
            /* The next run clears what was left and can stage again. */
            u = engine(good_manifest("files"));
            assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_NONE && !tube_update_kind(in(g_data, "update/stage")) && state(u).phase == TUBE_UPDATE_IDLE);
            tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_STAGED && !strcmp(tree(g_install), original));
            tube_update_free(u);
        }
}

static void applying(void) {
    char original[65], updated[65];
    /* Staged survives a restart of the app; the replacement then works from the new run. */
    fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
    put(in(g_registered, "appmeta/PPSA99142/param.json"), "stale"); put(in(g_registered, "app/PPSA99142/sce_sys/icon0.png"), "stale");
    tube_update *u = staged(); tube_update_free(u);
    tube_update_config config = settings(good_manifest("files")); config.registered_dir = g_registered;
    u = g_engine = tube_update_new(&config);
    assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_STAGED && state(u).phase == TUBE_UPDATE_STAGED && !strcmp(state(u).staged, NEW));
    assert(tube_update_apply(u) && state(u).phase == TUBE_UPDATE_APPLIED && installed_is_new());
    assert(!tube_update_apply(u));   /* once */
    snprintf(updated, sizeof updated, "%s", tree(g_install));
    /* The previous version waits beside the data; the home screen's copies follow the new one. */
    assert(holds(in(g_data, "update/previous/eboot.bin"), "old program") && holds(in(g_data, "update/previous/assets/old.txt"), "an asset the new version dropped"));
    char *copy = tube_update_read(in(g_registered, "appmeta/PPSA99142/param.json"), 4096, NULL);
    assert(copy && strstr(copy, NEW) && !holds(in(g_registered, "app/PPSA99142/sce_sys/icon0.png"), "stale")); free(copy);
    tube_update_discard(u); assert(state(u).phase == TUBE_UPDATE_APPLIED && tube_update_kind(in(g_data, "update/previous")) == 2);   /* too late to discard */
    tube_update_free(u);
    /* First start of the new version: pending until it has run, then the previous version goes. */
    u = engine(good_manifest("files"));
    assert(!strcmp(state(u).installed, NEW) && tube_update_boot(u) == TUBE_UPDATE_BOOT_PENDING);
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_IDLE);   /* nothing else while it is unconfirmed */
    assert(tube_update_confirm_start(u) == 1 && tube_update_confirm_start(u) == 0);
    assert(!tube_update_kind(in(g_data, "update/previous")) && !tube_update_kind(in(g_data, "update/state.json")) && !tube_update_kind(in(g_data, "update/stage")));
    assert(!strcmp(tree(g_install), updated));
    tube_update_free(u);
    u = engine(good_manifest("files")); assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_NONE);
    tube_update_run_check(u, 1); assert(state(u).check == TUBE_UPDATE_UP_TO_DATE);
    tube_update_free(u);

    /* A new version that does not start: two unconfirmed starts are waited for, the third puts the old one back. */
    fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
    u = staged(); assert(tube_update_apply(u)); tube_update_free(u);
    for (int start = 0; start < 2; ++start) {
        u = engine(good_manifest("files")); assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_PENDING && installed_is_new()); tube_update_free(u);
    }
    u = engine(good_manifest("files"));
    assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_ROLLED_BACK && !strcmp(tree(g_install), original) && !strcmp(state(u).installed, OLD));
    assert(!tube_update_kind(in(g_data, "update/previous")) && !tube_update_kind(in(g_data, "update/state.json")) && !tube_update_confirm_start(u));
    /* The version that failed is still listed, but no longer announced at start. */
    assert(state(u).check == TUBE_UPDATE_AVAILABLE && !tube_update_take_offer(u));
    tube_update_free(u);
    u = engine(good_manifest("files")); assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_NONE && !tube_update_take_offer(u));
    /* ... and can be tried again by hand; a start that works forgets the failure. */
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_STAGED && tube_update_apply(u)); tube_update_free(u);
    u = engine(good_manifest("files")); assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_PENDING && tube_update_confirm_start(u)); tube_update_free(u);
    assert(installed_is_new());

    /* The journal says applied, yet the old version is what runs: undone. */
    fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
    u = staged(); assert(tube_update_apply(u)); tube_update_free(u);
    write_param(g_install, OLD, "PPSA99142");
    u = engine(good_manifest("files")); assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_RECOVERED && !strcmp(tree(g_install), original)); tube_update_free(u);

    /* A failure, or a power loss, at every step of the replacement. */
    fresh(); u = staged(); g_step_count = 0; assert(tube_update_apply(u)); tube_update_free(u);
    char steps[64][40];
    const int count = g_step_count;
    memcpy(steps, g_steps, sizeof steps);
    assert(count >= 2 + 2 * 4 && !strcmp(steps[0], "swap:journal") && !strcmp(steps[count - 1], "swap:commit"));
    for (int kind = TUBE_UPDATE_FAULT_ERROR; kind <= TUBE_UPDATE_FAULT_STOP; ++kind)
        for (int i = 0; i < count; ++i) {
            fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
            u = staged();
            arm(steps[i], kind, 0);
            assert(!tube_update_apply(u) && !*g_fault_step);
            if (kind == TUBE_UPDATE_FAULT_ERROR) {   /* undone on the spot */
                assert(state(u).phase == TUBE_UPDATE_FAILED && strstr(state(u).error, "unchanged") && !*state(u).staged);
                if (strcmp(tree(g_install), original)) { fprintf(stderr, "not undone after %s\n", steps[i]); assert(0); }
                assert(!tube_update_kind(in(g_data, "update/previous")) && !tube_update_kind(in(g_data, "update/state.json")));
            }
            tube_update_free(u);
            u = engine(good_manifest("files"));
            const int boot = tube_update_boot(u);
            if (kind == TUBE_UPDATE_FAULT_ERROR) assert(boot == TUBE_UPDATE_BOOT_NONE);
            else if (!strcmp(steps[i], "swap:journal")) assert(boot == TUBE_UPDATE_BOOT_STAGED);   /* nothing had moved: still ready */
            else assert(boot == TUBE_UPDATE_BOOT_RECOVERED);
            if (strcmp(tree(g_install), original)) { fprintf(stderr, "not recovered after %s (%d)\n", steps[i], kind); assert(0); }
            assert(!strcmp(state(u).installed, OLD) && !tube_update_kind(in(g_data, "update/previous")));
            /* And the update can still be made afterwards. */
            if (boot != TUBE_UPDATE_BOOT_STAGED) { tube_update_run_check(u, 1); tube_update_run_download(u); }
            assert(state(u).phase == TUBE_UPDATE_STAGED && tube_update_apply(u) && installed_is_new());
            tube_update_free(u);
        }
    /* An unreadable journal is not acted on. */
    fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
    const char *journals[] = {"{", "{\"phase\":\"applied\",\"to\":\"junk\"}", "{\"phase\":\"swapping\",\"to\":\"01.000.010\",\"entries\":[{\"name\":\"../../x\",\"had\":true}]}",
                              "{\"phase\":\"other\",\"to\":\"01.000.010\"}"};
    for (int i = 0; i < 4; ++i) {
        put(in(g_data, "update/state.json"), journals[i]); put(in(g_data, "update/previous/eboot.bin"), "stray");
        u = engine(good_manifest("files"));
        assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_NONE && !strcmp(tree(g_install), original) && !tube_update_kind(in(g_data, "update/state.json")) &&
               !tube_update_kind(in(g_data, "update/previous")));
        tube_update_free(u);
    }
}

static void refusals(void) {
    char original[65], elsewhere[700], offer[1024];
    const fixture *good = archive("good.zip");
    /* A development build checks, and never updates itself. */
    fresh(); snprintf(original, sizeof original, "%s", tree(g_install));
    tube_update_config config = settings(good_manifest("files")); config.development = 1;
    tube_update *u = g_engine = tube_update_new(&config);
    assert(state(u).blocked == TUBE_UPDATE_BLOCKED_DEVELOPMENT && strstr(state(u).blocked_text, "development build"));
    tube_update_run_check(u, 1); assert(state(u).check == TUBE_UPDATE_AVAILABLE);
    int before = requests();
    tube_update_run_download(u);
    assert(state(u).phase == TUBE_UPDATE_IDLE && strstr(state(u).error, "development build") && requests() == before && !tube_update_apply(u));
    assert(!tube_update_kind(in(g_data, "update/download.zip")) && !strcmp(tree(g_install), original));
    tube_update_free(u);
    /* A release build ignores the test offer file. */
    snprintf(offer, sizeof offer, "01.000.030\nTest build\n%s/files/good.zip\n%s\n%llu\n## Notes\n- from the file\n", g_base, good->sha256, good->size);
    put(in(g_data, "update-offer.txt"), offer);
    u = engine(good_manifest("files")); tube_update_run_check(u, 1);
    assert(!strcmp(state(u).latest, NEW)); tube_update_free(u);
    /* The development build takes its offer from that file, without asking anyone, and may then run the whole sequence. */
    snprintf(offer, sizeof offer, "%s\nTest build\n%s/files/good.zip\n%s\n%llu\n## Notes\n- from the file\n", NEW, g_base, good->sha256, good->size);
    put(in(g_data, "update-offer.txt"), offer);
    u = g_engine = tube_update_new(&config);
    assert(!state(u).blocked && state(u).check == TUBE_UPDATE_AVAILABLE && !strcmp(state(u).latest_name, "Test build"));
    before = requests(); tube_update_run_check(u, 0);
    assert(requests() == before && !strcmp(tube_update_notes(u), "Notes\n- from the file"));
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_STAGED && tube_update_apply(u) && installed_is_new());
    tube_update_free(u);
    /* A malformed offer file is no offer. */
    fresh(); put(in(g_data, "update-offer.txt"), "01.000.030\nname\nhttps://example.org/x.zip\nnothex\n12\n");
    u = g_engine = tube_update_new(&config); assert(state(u).blocked == TUBE_UPDATE_BLOCKED_DEVELOPMENT); tube_update_free(u);

    /* Running from somewhere else: the offer is shown, the update is not made. */
    fresh(); snprintf(elsewhere, sizeof elsewhere, "%s/usb/PPSA99142", g_case);
    put(in(elsewhere, "eboot.bin"), "old program"); write_param(elsewhere, OLD, "PPSA99142");
    assert(tube_update_remove(g_install) == 0);
    config = settings(good_manifest("files")); config.running_dir = elsewhere;
    u = g_engine = tube_update_new(&config);
    assert(state(u).blocked == TUBE_UPDATE_BLOCKED_LOCATION && strstr(state(u).blocked_text, g_install) && !strcmp(state(u).installed, OLD));
    tube_update_run_check(u, 1); assert(state(u).check == TUBE_UPDATE_AVAILABLE && tube_update_take_offer(u));
    tube_update_run_download(u); assert(state(u).phase == TUBE_UPDATE_IDLE && strstr(state(u).error, "not installed in"));
    tube_update_free(u);
    /* The install folder holds another copy: left alone. */
    put(in(g_install, "eboot.bin"), "another build"); write_param(g_install, "01.000.005", "PPSA99142");
    u = g_engine = tube_update_new(&config); assert(state(u).blocked == TUBE_UPDATE_BLOCKED_VERSION); tube_update_free(u);
    write_param(g_install, OLD, "PPSA99142");   /* the same version, another program file */
    u = g_engine = tube_update_new(&config); assert(state(u).blocked == TUBE_UPDATE_BLOCKED_VERSION); tube_update_free(u);
    put(in(g_install, "eboot.bin"), "old program"); write_param(g_install, OLD, "PPSA00001");
    u = g_engine = tube_update_new(&config); assert(state(u).blocked == TUBE_UPDATE_BLOCKED_LOCATION); tube_update_free(u);
    /* The console's own record of where the app was mounted from decides first. */
    fresh(); config = settings(good_manifest("files")); config.registered_dir = g_registered;
    put(in(g_registered, "app/PPSA99142/mount.lnk"), "/mnt/usb0/homebrew/PPSA99142\n");
    u = g_engine = tube_update_new(&config);
    assert(state(u).blocked == TUBE_UPDATE_BLOCKED_LOCATION && strstr(state(u).blocked_text, "/mnt/usb0/homebrew/PPSA99142")); tube_update_free(u);
    char record[800]; snprintf(record, sizeof record, "%s/\n", g_install);
    put(in(g_registered, "app/PPSA99142/mount.lnk"), record);
    u = g_engine = tube_update_new(&config); assert(!state(u).blocked); tube_update_free(u);
    unlink(in(g_registered, "app/PPSA99142/mount.lnk")); put(in(g_registered, "app/PPSA99142/mount_img.lnk"), "/data/homebrew/PPSA99142.ffpfsc");
    u = g_engine = tube_update_new(&config); assert(state(u).blocked == TUBE_UPDATE_BLOCKED_IMAGE && strstr(state(u).blocked_text, "image")); tube_update_free(u);
    /* A staged update of a copy that has since become one of these is dropped at start. */
    fresh(); u = staged(); tube_update_free(u);
    config = settings(good_manifest("files")); config.development = 1;
    u = g_engine = tube_update_new(&config);
    assert(tube_update_boot(u) == TUBE_UPDATE_BOOT_NONE && !tube_update_kind(in(g_data, "update/stage")) && state(u).phase == TUBE_UPDATE_IDLE);
    tube_update_free(u);
    /* No data folder to work in. */
    assert(!tube_update_new(NULL));
    tube_update_config none = {0}; assert(!tube_update_new(&none));
}

static void secure(void) {
    const char *url = getenv("TUBE_UPDATE_TEST_TLS_URL"), *roots = getenv("TUBE_UPDATE_TEST_CA");
    char manifest_url[300];
    if (!url || !roots) { puts("SKIP: no openssl, the HTTPS cases did not run"); return; }
    fresh(); (void)good_manifest("files");
    snprintf(manifest_url, sizeof manifest_url, "%s/files/releases.json", url);
    /* The server's certificate is verified: the right root answers, any other root does not. */
    tube_js_http_config trusted = {.ca_file = roots}, untrusted = {0};
    tube_update_http *with = tube_update_http_new(&trusted), *without = tube_update_http_new(&untrusted);
    tube_update_config config = settings(manifest_url);
    config.transport = tube_update_http_transport(with);
    tube_update *u = tube_update_new(&config); tube_update_run_check(u, 1);
    if (state(u).check != TUBE_UPDATE_AVAILABLE) fprintf(stderr, "https: %s\n", state(u).error);
    assert(state(u).check == TUBE_UPDATE_AVAILABLE && !*state(u).error);
    tube_update_free(u);
    fresh();
    config = settings(manifest_url); config.transport = tube_update_http_transport(without);
    u = tube_update_new(&config); tube_update_run_check(u, 1);
    assert(state(u).check == TUBE_UPDATE_UNKNOWN && strstr(state(u).error, "could not be reached"));
    tube_update_free(u);
    tube_update_http_free(with); tube_update_http_free(without);
}

int main(void) {
    char *listing, *line;
    g_root = getenv("TUBE_UPDATE_TEST_DIR"); g_base = getenv("TUBE_UPDATE_TEST_URL");
    assert(g_root && g_base && strlen(g_root) < 300);
    snprintf(g_prefix, sizeof g_prefix, "%s/", g_base);
    listing = tube_update_read(in(g_root, "www/index.txt"), 1 << 16, NULL); assert(listing);
    int count = 0;
    for (line = strtok(listing, "\n"); line && count < 24; line = strtok(NULL, "\n"), ++count)
        assert(sscanf(line, "%39s %llu %64s", g_fixtures[count].name, &g_fixtures[count].size, g_fixtures[count].sha256) == 3);
    free(listing);
    tube_js_http_config plain = {.timeout_ms = 20000};
    g_http = tube_update_http_new(&plain); assert(g_http);
    versions(); digests(); notes(); manifests(); archives();
    checking(); downloading(); resuming(); staging_failures(); applying(); refusals(); secure();
    tube_update_http_free(g_http);
    puts("PASS: update versions, release metadata, daily check and privacy, ranged download with resume and cancel, staging, "
         "replacement with a failure at every step, rollback, development and location refusals");
    return 0;
}
