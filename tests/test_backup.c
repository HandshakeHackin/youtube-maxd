// Local backup and restore in a temporary data directory.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tube_backup.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *g_dir;
static const char STATE_A[] = "{\"version\":2,\"token\":\"pair-A\",\"records\":[]}";
static const char STATE_B[] = "{\"version\":1,\"token\":\"pair-B\",\"records\":[{\"id\":\"abcdefghijk\"}]}\n";
#define T0 1791727380LL /* 2026-10-11 14:03:00 UTC */
#define DAY 86400LL

static const char *path_of(const char *leaf) {
    static char path[4][1024];
    static int slot;
    slot = (slot + 1) % 4;
    snprintf(path[slot], sizeof path[slot], "%s/%s", g_dir, leaf);
    return path[slot];
}

static void put(const char *leaf, const void *bytes, size_t length) {
    FILE *file = fopen(path_of(leaf), "wb");
    assert(file && fwrite(bytes, 1, length, file) == length && fclose(file) == 0);
}

static void put_text(const char *leaf, const char *text) { put(leaf, text, strlen(text)); }

static int present(const char *leaf) { struct stat info; return stat(path_of(leaf), &info) == 0; }

static int holds(const char *leaf, const void *bytes, size_t length) {
    static char buffer[4096];
    FILE *file = fopen(path_of(leaf), "rb");
    if (!file) return 0;
    size_t got = fread(buffer, 1, sizeof buffer, file);
    fclose(file);
    return got == length && !memcmp(buffer, bytes, length);
}

static int entries(const char *leaf) {
    DIR *dir = opendir(path_of(leaf));
    int count = 0;
    if (!dir) return -1;
    for (struct dirent *entry; (entry = readdir(dir)) != NULL;) count += strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..");
    closedir(dir);
    return count;
}

static void clear_backups(void) {
    static char names[512][256];
    int count = 0;
    DIR *dir = opendir(path_of("backup"));
    if (!dir) return;
    for (struct dirent *entry; (entry = readdir(dir)) != NULL && count < 512;)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) snprintf(names[count++], sizeof names[0], "%s", entry->d_name);
    closedir(dir);
    for (int i = 0; i < count; i++) {
        char leaf[300];
        snprintf(leaf, sizeof leaf, "backup/%s", names[i]);
        if (remove(path_of(leaf)) != 0) assert(rmdir(path_of(leaf)) == 0);
    }
    assert(rmdir(path_of("backup")) == 0);
}

static const char *backup_leaf(const char *name) {
    static char leaf[2][128];
    static int slot;
    slot ^= 1;
    snprintf(leaf[slot], sizeof leaf[slot], "backup/%s", name);
    return leaf[slot];
}

static void test_names(void) {
    static const char *bad[] = {
        "", "youtube.json", "youtube-", "../youtube-20261011-140300.json", "backup/youtube-20261011-140300.json",
        "youtube-20261011-140300.json/", "youtube-20261011-140300.json.tmp", "youtube-20261311-140300.json",
        "youtube-20260230-140300.json", "youtube-20250229-000000.json", "youtube-20261011-240000.json",
        "youtube-20261011-146000.json", "youtube-20261011-140360.json", "youtube-20261011-1403xx.json",
        "youtube-2026101-1140300.json", "Youtube-20261011-140300.json", "youtube-20261011-140300-safetyx.json",
        "youtube-20261011-140300-safety", "youtube-19691231-235959.json", "youtube-20261011-140300.JSON",
        "/etc/passwd", "youtube-20261011-14/300.json", "youtube-+0261011-140300.json", "youtube-20261000-140300.json",
        "youtube-20260011-140300.json", "youtube-20261011_140300.json", "youtube-20261011-140300.json ",
        "youtube-20261011-140300..json", "youtube-20261011-140300-safety-safety.json", "youtube-20261011-140300",
        "..", ".", "youtube-20261011-140300.json\\..\\x", " youtube-20261011-140300.json"};
    char name[TUBE_BACKUP_NAME_SIZE], text[32], tiny[8];
    long long created = 0;
    int safety = -1;
    assert(!tube_backup_name_valid(NULL));
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        assert(!tube_backup_name_valid(bad[i]));
        assert(tube_backup_name_info(bad[i], &created, &safety) == TUBE_BACKUP_E_NAME);
        assert(tube_backup_validate(g_dir, bad[i]) == TUBE_BACKUP_E_NAME);
        assert(tube_backup_restore(g_dir, bad[i], T0, NULL, 0) == TUBE_BACKUP_E_NAME);
        assert(tube_backup_delete(g_dir, bad[i]) == TUBE_BACKUP_E_NAME);
    }
    assert(tube_backup_name_info("youtube-20261011-140300.json", &created, &safety) == TUBE_BACKUP_OK);
    assert(created == T0 && safety == 0);
    assert(tube_backup_name_info("youtube-20261011-140300-safety.json", &created, &safety) == TUBE_BACKUP_OK);
    assert(created == T0 && safety == 1);
    assert(tube_backup_name_info("youtube-20240229-000000.json", &created, NULL) == TUBE_BACKUP_OK && created == 1709164800LL);
    assert(tube_backup_name_info("youtube-20000229-000000.json", &created, NULL) == TUBE_BACKUP_OK && created == 951782400LL);
    assert(!tube_backup_name_valid("youtube-21000229-000000.json") && tube_backup_name_valid("youtube-21000228-235959.json"));
    assert(tube_backup_name_valid("youtube-99991231-235959.json") && tube_backup_name_valid("youtube-19700101-000000.json"));

    assert(tube_backup_format_name(T0, 0, name, sizeof name) == TUBE_BACKUP_OK && !strcmp(name, "youtube-20261011-140300.json"));
    assert(tube_backup_format_name(T0, 1, name, sizeof name) == TUBE_BACKUP_OK && !strcmp(name, "youtube-20261011-140300-safety.json"));
    assert(tube_backup_format_name(0, 0, name, sizeof name) == TUBE_BACKUP_OK && !strcmp(name, "youtube-19700101-000000.json"));
    assert(tube_backup_format_name(253402300799LL, 0, name, sizeof name) == TUBE_BACKUP_OK && !strcmp(name, "youtube-99991231-235959.json"));
    assert(tube_backup_format_name(1709251199LL, 0, name, sizeof name) == TUBE_BACKUP_OK && !strcmp(name, "youtube-20240229-235959.json"));
    assert(tube_backup_format_name(1709251200LL, 0, name, sizeof name) == TUBE_BACKUP_OK && !strcmp(name, "youtube-20240301-000000.json"));
    assert(tube_backup_format_name(-1, 0, name, sizeof name) == TUBE_BACKUP_E_ARGUMENT && !*name);
    assert(tube_backup_format_name(253402300800LL, 0, name, sizeof name) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_format_name(T0, 0, tiny, sizeof tiny) == TUBE_BACKUP_E_ARGUMENT && !*tiny);
    assert(tube_backup_format_name(T0, 0, NULL, 0) == TUBE_BACKUP_E_ARGUMENT);
    /* Every formatted name parses back to the same second. */
    for (long long when = 0; when < 253402300800LL; when += 86400LL * 977 + 3607) {
        assert(tube_backup_format_name(when, (int)(when & 1), name, sizeof name) == TUBE_BACKUP_OK);
        assert(tube_backup_name_info(name, &created, &safety) == TUBE_BACKUP_OK && created == when && safety == (int)(when & 1));
    }
    assert(tube_backup_format_time(T0, text, sizeof text) == TUBE_BACKUP_OK && !strcmp(text, "2026-10-11 14:03 UTC"));
    assert(tube_backup_format_time(T0, tiny, sizeof tiny) == TUBE_BACKUP_E_ARGUMENT && tube_backup_format_time(-5, text, sizeof text) == TUBE_BACKUP_E_ARGUMENT);
    for (int code = 1; code >= -8; code--) assert(strlen(tube_backup_error_text(code)) > 4);
}

static void test_validation(void) {
    static char big[TUBE_BACKUP_MAX_BYTES + 2];
    static const struct { const char *text; int expected; } cases[] = {
        {"{\"version\":2,\"records\":[]}", TUBE_BACKUP_OK},
        {"{\"version\":1,\"records\":[]}", TUBE_BACKUP_OK},
        {"  {\"version\":2,\"records\":[{}]} \r\n\t", TUBE_BACKUP_OK},
        {"{\"version\":2,\"records\":[]", TUBE_BACKUP_E_INVALID},
        {"{\"version\":2,\"records\":[", TUBE_BACKUP_E_INVALID},
        {"{\"version\":2,\"reco", TUBE_BACKUP_E_INVALID},
        {"{\"version\":3,\"records\":[]}", TUBE_BACKUP_E_INVALID},
        {"{\"version\":0,\"records\":[]}", TUBE_BACKUP_E_INVALID},
        {"{\"version\":1.5,\"records\":[]}", TUBE_BACKUP_E_INVALID},
        {"{\"version\":\"2\",\"records\":[]}", TUBE_BACKUP_E_INVALID},
        {"{\"Version\":2,\"records\":[]}", TUBE_BACKUP_E_INVALID},
        {"{\"records\":[]}", TUBE_BACKUP_E_INVALID},
        {"{\"version\":2}", TUBE_BACKUP_E_INVALID},
        {"{\"version\":2,\"records\":{}}", TUBE_BACKUP_E_INVALID},
        {"[{\"version\":2,\"records\":[]}]", TUBE_BACKUP_E_INVALID},
        {"{\"version\":2,\"records\":[]}x", TUBE_BACKUP_E_INVALID},
        {"{\"version\":2,\"records\":[]}{\"version\":2,\"records\":[]}", TUBE_BACKUP_E_INVALID},
        {"null", TUBE_BACKUP_E_INVALID},
        {"not json at all", TUBE_BACKUP_E_INVALID}};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        put_text("candidate.json", cases[i].text);
        assert(tube_backup_validate_file(path_of("candidate.json")) == cases[i].expected);
    }
    put("candidate.json", "", 0);
    assert(tube_backup_validate_file(path_of("candidate.json")) == TUBE_BACKUP_E_INVALID);
    put("candidate.json", "{\"version\":2,\"records\":[]}\0{}", 29);
    assert(tube_backup_validate_file(path_of("candidate.json")) == TUBE_BACKUP_E_INVALID);
    put("candidate.json", "{\"version\":2,\0\"records\":[]}", 27);
    assert(tube_backup_validate_file(path_of("candidate.json")) == TUBE_BACKUP_E_INVALID);
    /* Exactly at the bound is accepted; one byte more is not read at all. */
    memset(big, ' ', sizeof big);
    memcpy(big, STATE_A, sizeof STATE_A - 1);
    put("candidate.json", big, TUBE_BACKUP_MAX_BYTES);
    assert(tube_backup_validate_file(path_of("candidate.json")) == TUBE_BACKUP_OK);
    put("candidate.json", big, TUBE_BACKUP_MAX_BYTES + 1);
    assert(tube_backup_validate_file(path_of("candidate.json")) == TUBE_BACKUP_E_TOO_LARGE);
    assert(remove(path_of("candidate.json")) == 0);
    assert(tube_backup_validate_file(path_of("candidate.json")) == TUBE_BACKUP_E_MISSING);
    assert(tube_backup_validate_file(NULL) == TUBE_BACKUP_E_ARGUMENT && tube_backup_validate_file("") == TUBE_BACKUP_E_ARGUMENT);
}

static void test_create_restore(void) {
    static char big[TUBE_BACKUP_MAX_BYTES + 2];
    char name[TUBE_BACKUP_NAME_SIZE] = "x", safety[TUBE_BACKUP_NAME_SIZE] = "x", tiny[8] = "x", long_dir[900];
    tube_backup_entry list[TUBE_BACKUP_LIST_MAX];

    /* Nothing to back up: no directory appears and nothing is listed. */
    assert(tube_backup_create(g_dir, T0, name, sizeof name) == TUBE_BACKUP_E_MISSING && !*name);
    assert(!present("backup") && tube_backup_list(g_dir, list, TUBE_BACKUP_LIST_MAX) == 0);
    assert(tube_backup_rotate(g_dir, 3) == 0 && tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, T0, 3, name, sizeof name) == 0);
    assert(tube_backup_validate(g_dir, "youtube-20261011-140300.json") == TUBE_BACKUP_E_MISSING);
    assert(tube_backup_restore(g_dir, "youtube-20261011-140300.json", T0, NULL, 0) == TUBE_BACKUP_E_MISSING);
    assert(tube_backup_delete(g_dir, "youtube-20261011-140300.json") == TUBE_BACKUP_E_MISSING && !present("backup"));

    /* A damaged or oversized state is never turned into a backup. */
    put_text("youtube.json", "{\"version\":2,\"records\":[");
    assert(tube_backup_create(g_dir, T0, name, sizeof name) == TUBE_BACKUP_E_INVALID && !*name && !present("backup"));
    memset(big, ' ', sizeof big);
    memcpy(big, STATE_A, sizeof STATE_A - 1);
    put("youtube.json", big, TUBE_BACKUP_MAX_BYTES + 1);
    assert(tube_backup_create(g_dir, T0, name, sizeof name) == TUBE_BACKUP_E_TOO_LARGE && !present("backup"));

    put_text("youtube.json", STATE_A);
    assert(tube_backup_create(g_dir, -1, name, sizeof name) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_create(NULL, T0, name, sizeof name) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_create("", T0, name, sizeof name) == TUBE_BACKUP_E_ARGUMENT);
    memset(long_dir, 'd', sizeof long_dir - 1); long_dir[0] = '/'; long_dir[sizeof long_dir - 1] = 0;
    assert(tube_backup_create(long_dir, T0, name, sizeof name) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_list(long_dir, list, 4) == TUBE_BACKUP_E_ARGUMENT && tube_backup_rotate(long_dir, 4) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_restore(long_dir, "youtube-20261011-140300.json", T0, NULL, 0) == TUBE_BACKUP_E_ARGUMENT);
    assert(!present("backup"));

    assert(tube_backup_create(g_dir, T0, name, sizeof name) == TUBE_BACKUP_OK && !strcmp(name, "youtube-20261011-140300.json"));
    assert(holds("backup/youtube-20261011-140300.json", STATE_A, sizeof STATE_A - 1) && entries("backup") == 1);
    assert(holds("youtube.json", STATE_A, sizeof STATE_A - 1));
    assert(tube_backup_validate(g_dir, name) == TUBE_BACKUP_OK);
    assert(tube_backup_list(g_dir, list, TUBE_BACKUP_LIST_MAX) == 1 && !strcmp(list[0].name, name));
    assert(list[0].created == T0 && list[0].bytes == (long long)sizeof STATE_A - 1 && !list[0].safety);
    assert(tube_backup_list(g_dir, NULL, 4) == TUBE_BACKUP_E_ARGUMENT && tube_backup_list(g_dir, list, 0) == TUBE_BACKUP_E_ARGUMENT);
    /* The same second again replaces the file; the name is optional. */
    assert(tube_backup_create(g_dir, T0, NULL, 0) == TUBE_BACKUP_OK && entries("backup") == 1);
    assert(tube_backup_create(g_dir, T0 + 1, tiny, sizeof tiny) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_delete(g_dir, "youtube-20261011-140301.json") == TUBE_BACKUP_E_MISSING && entries("backup") == 1);

    /* Restore replaces the state and keeps what it replaced. */
    put_text("youtube.json", STATE_B);
    assert(tube_backup_restore(g_dir, name, T0 + 60, tiny, sizeof tiny) == TUBE_BACKUP_E_ARGUMENT);
    assert(holds("youtube.json", STATE_B, sizeof STATE_B - 1) && entries("backup") == 1);
    assert(tube_backup_restore(g_dir, name, T0 + 60, safety, sizeof safety) == TUBE_BACKUP_OK);
    assert(!strcmp(safety, "youtube-20261011-140400-safety.json"));
    assert(holds("youtube.json", STATE_A, sizeof STATE_A - 1) && holds(backup_leaf(safety), STATE_B, sizeof STATE_B - 1));
    assert(holds(backup_leaf(name), STATE_A, sizeof STATE_A - 1) && entries("backup") == 2 && !present("youtube.json.tmp"));
    assert(tube_backup_list(g_dir, list, TUBE_BACKUP_LIST_MAX) == 2 && list[0].safety && !strcmp(list[0].name, safety) && !list[1].safety);
    /* The safety copy is itself restorable: undo. */
    assert(tube_backup_restore(g_dir, safety, T0 + 120, NULL, 0) == TUBE_BACKUP_OK && holds("youtube.json", STATE_B, sizeof STATE_B - 1));
    assert(holds("backup/youtube-20261011-140500-safety.json", STATE_A, sizeof STATE_A - 1) && entries("backup") == 3);

    /* A damaged current state does not block recovery and is preserved. */
    put_text("youtube.json", "{\"version\":2,\"reco");
    assert(tube_backup_restore(g_dir, name, T0 + 180, safety, sizeof safety) == TUBE_BACKUP_OK);
    assert(holds("youtube.json", STATE_A, sizeof STATE_A - 1) && holds(backup_leaf(safety), "{\"version\":2,\"reco", 18));
    assert(tube_backup_validate(g_dir, safety) == TUBE_BACKUP_E_INVALID);
    assert(tube_backup_restore(g_dir, safety, T0 + 240, NULL, 0) == TUBE_BACKUP_E_INVALID && holds("youtube.json", STATE_A, sizeof STATE_A - 1));
    assert(entries("backup") == 4);

    /* No current state: restore still works and reports no safety copy. */
    assert(remove(path_of("youtube.json")) == 0);
    strcpy(safety, "x");
    assert(tube_backup_restore(g_dir, name, T0 + 300, safety, sizeof safety) == TUBE_BACKUP_OK && !*safety);
    assert(holds("youtube.json", STATE_A, sizeof STATE_A - 1) && entries("backup") == 4);

    /* Damaged, truncated and oversized backups are refused before anything changes. */
    put_text("backup/youtube-20270101-000000.json", "{\"version\":2,\"records\":[{\"id\":");
    assert(tube_backup_validate(g_dir, "youtube-20270101-000000.json") == TUBE_BACKUP_E_INVALID);
    assert(tube_backup_restore(g_dir, "youtube-20270101-000000.json", T0 + 360, safety, sizeof safety) == TUBE_BACKUP_E_INVALID && !*safety);
    put("backup/youtube-20270101-000000.json", big, TUBE_BACKUP_MAX_BYTES + 1);
    assert(tube_backup_restore(g_dir, "youtube-20270101-000000.json", T0 + 360, NULL, 0) == TUBE_BACKUP_E_TOO_LARGE);
    put("backup/youtube-20270101-000000.json", "", 0);
    assert(tube_backup_restore(g_dir, "youtube-20270101-000000.json", T0 + 360, NULL, 0) == TUBE_BACKUP_E_INVALID);
    assert(holds("youtube.json", STATE_A, sizeof STATE_A - 1) && entries("backup") == 5);
    assert(tube_backup_delete(g_dir, "youtube-20270101-000000.json") == TUBE_BACKUP_OK);
    assert(tube_backup_delete(g_dir, "youtube-20270101-000000.json") == TUBE_BACKUP_E_MISSING);
    assert(tube_backup_delete(g_dir, NULL) == TUBE_BACKUP_E_NAME && tube_backup_delete(NULL, name) == TUBE_BACKUP_E_ARGUMENT);
    clear_backups();
}

static void test_list_rotate(void) {
    tube_backup_entry list[TUBE_BACKUP_LIST_MAX + 4];
    char name[TUBE_BACKUP_NAME_SIZE];
    put_text("youtube.json", STATE_A);
    assert(mkdir(path_of("backup"), 0777) == 0);
    /* 70 backups written out of order, 5 safety copies, and files that are not ours. */
    for (int i = 0; i < 70; i++) {
        int shuffled = i * 37 % 70;
        assert(tube_backup_format_name(T0 + shuffled * DAY, 0, name, sizeof name) == TUBE_BACKUP_OK);
        put_text(backup_leaf(name), STATE_A);
    }
    for (int i = 0; i < 5; i++) {
        assert(tube_backup_format_name(T0 + i * DAY, 1, name, sizeof name) == TUBE_BACKUP_OK);
        put_text(backup_leaf(name), STATE_B);
    }
    put_text("backup/notes.txt", "keep me");
    put_text("backup/youtube.json", STATE_A);
    put_text("backup/youtube-20261011-140300.json.tmp", "interrupted");
    put_text("backup/youtube-20261011-140300-safety.json.tmp", "interrupted");
    put_text("backup/other.tmp", "keep me");
    assert(mkdir(path_of("backup/youtube-20200101-000000.json"), 0777) == 0);

    assert(tube_backup_list(g_dir, list, TUBE_BACKUP_LIST_MAX + 4) == TUBE_BACKUP_LIST_MAX);
    for (int i = 0; i < TUBE_BACKUP_LIST_MAX; i++) {
        assert(tube_backup_name_valid(list[i].name) && list[i].bytes > 0);
        if (i) assert(list[i - 1].created > list[i].created || (list[i - 1].created == list[i].created && !list[i - 1].safety && list[i].safety));
    }
    assert(list[0].created == T0 + 69 * DAY && !list[0].safety);
    assert(tube_backup_list(g_dir, list, 5) == 5 && list[4].created == T0 + 65 * DAY);
    assert(tube_backup_list(g_dir, list, 1) == 1 && list[0].created == T0 + 69 * DAY);

    assert(tube_backup_rotate(g_dir, 0) == TUBE_BACKUP_E_ARGUMENT && tube_backup_rotate(g_dir, TUBE_BACKUP_LIST_MAX + 1) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_rotate(NULL, 3) == TUBE_BACKUP_E_ARGUMENT && entries("backup") == 81);
    assert(tube_backup_rotate(g_dir, 3) == 67 + 2);
    /* Three backups, three safety copies, three foreign files and a directory
     * that only looks like an old backup. */
    assert(entries("backup") == 10 && present("backup/notes.txt") && present("backup/other.tmp") && present("backup/youtube.json"));
    assert(present("backup/youtube-20200101-000000.json") && !present("backup/youtube-20261011-140300.json.tmp"));
    assert(tube_backup_list(g_dir, list, TUBE_BACKUP_LIST_MAX) == 6);
    assert(list[0].created == T0 + 69 * DAY && list[1].created == T0 + 68 * DAY && list[2].created == T0 + 67 * DAY);
    assert(list[3].safety && list[3].created == T0 + 4 * DAY && list[5].safety && list[5].created == T0 + 2 * DAY);
    assert(tube_backup_rotate(g_dir, 3) == 0 && tube_backup_rotate(g_dir, TUBE_BACKUP_LIST_MAX) == 0 && entries("backup") == 10);
    assert(tube_backup_rotate(g_dir, 1) == 2 && tube_backup_list(g_dir, list, 8) == 4 && list[0].created == T0 + 69 * DAY);
    clear_backups();
}

static void test_auto(void) {
    char name[TUBE_BACKUP_NAME_SIZE];
    tube_backup_entry list[TUBE_BACKUP_LIST_MAX];
    const long long now = T0;
    for (int days = -3; days <= 40; days++)
        assert(tube_backup_auto_valid(days) == (days == -1 || days == 1 || days == 7 || days == 30));
    assert(!tube_backup_auto_due(TUBE_BACKUP_AUTO_OFF, now, 0) && !tube_backup_auto_due(0, now, 0) && !tube_backup_auto_due(2, now, 0));
    assert(tube_backup_auto_due(TUBE_BACKUP_AUTO_DAILY, now, 0) && tube_backup_auto_due(TUBE_BACKUP_AUTO_MONTHLY, now, -5));
    /* A console clock that was never set must not start a backup. */
    assert(!tube_backup_auto_due(TUBE_BACKUP_AUTO_DAILY, 0, 0) && !tube_backup_auto_due(TUBE_BACKUP_AUTO_DAILY, -1, 0));
    assert(!tube_backup_auto_due(TUBE_BACKUP_AUTO_DAILY, 1577836799LL, 0) && tube_backup_auto_due(TUBE_BACKUP_AUTO_DAILY, 1577836800LL, 0));
    assert(!tube_backup_auto_due(TUBE_BACKUP_AUTO_DAILY, 253402300800LL, 0));
    static const int periods[3] = {TUBE_BACKUP_AUTO_DAILY, TUBE_BACKUP_AUTO_WEEKLY, TUBE_BACKUP_AUTO_MONTHLY};
    for (int i = 0; i < 3; i++) {
        long long period = periods[i] * DAY;
        assert(!tube_backup_auto_due(periods[i], now, now) && !tube_backup_auto_due(periods[i], now, now - period + 1));
        assert(tube_backup_auto_due(periods[i], now, now - period) && tube_backup_auto_due(periods[i], now, now - 400 * DAY));
        /* Clock moved back: wait unless the newest backup is implausibly far ahead. */
        assert(!tube_backup_auto_due(periods[i], now, now + 1) && !tube_backup_auto_due(periods[i], now, now + period));
        assert(tube_backup_auto_due(periods[i], now, now + period + 1));
    }

    put_text("youtube.json", STATE_A);
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_OFF, now, 5, name, sizeof name) == 0 && !*name && !present("backup"));
    assert(tube_backup_auto_run(g_dir, 3, now, 5, name, sizeof name) == 0 && !present("backup"));
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, now, 0, name, sizeof name) == TUBE_BACKUP_E_ARGUMENT);
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, now, 5, name, sizeof name) == 1 && !strcmp(name, "youtube-20261011-140300.json"));
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, now, 5, name, sizeof name) == 0 && !*name);
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, now + DAY - 1, 5, NULL, 0) == 0 && entries("backup") == 1);
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_WEEKLY, now + DAY, 5, NULL, 0) == 0);
    /* A safety copy from a restore does not postpone the next automatic backup. */
    assert(tube_backup_restore(g_dir, "youtube-20261011-140300.json", now + DAY - 10, NULL, 0) == TUBE_BACKUP_OK && entries("backup") == 2);
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, now + DAY, 5, name, sizeof name) == 1 && !strcmp(name, "youtube-20261012-140300.json"));
    /* A damaged state reports the reason instead of writing a backup. */
    put_text("youtube.json", "{");
    assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, now + 2 * DAY, 5, name, sizeof name) == TUBE_BACKUP_E_INVALID && entries("backup") == 3);
    put_text("youtube.json", STATE_B);
    for (int day = 2; day < 20; day++)
        assert(tube_backup_auto_run(g_dir, TUBE_BACKUP_AUTO_DAILY, now + day * DAY, 5, NULL, 0) == 1);
    assert(tube_backup_list(g_dir, list, TUBE_BACKUP_LIST_MAX) == 6 && list[0].created == now + 19 * DAY && list[4].created == now + 15 * DAY && list[5].safety);
    assert(holds(backup_leaf(list[0].name), STATE_B, sizeof STATE_B - 1));
    clear_backups();
    assert(remove(path_of("youtube.json")) == 0);
}

int main(void) {
    char directory[] = "/tmp/prosperotube-backup-XXXXXX";
    assert(mkdtemp(directory));
    /* Same convention as the provider tests: the data root comes from here. */
    assert(!setenv("EVO_DATA_DIR_OVERRIDE", directory, 1));
    g_dir = getenv("EVO_DATA_DIR_OVERRIDE");
    test_names();
    test_validation();
    test_create_restore();
    test_list_rotate();
    test_auto();
    assert(entries(".") == 0 && rmdir(directory) == 0);
    puts("PASS: local backups are dated, validated, listed newest first, restored with a safety copy, rotated and scheduled");
    return 0;
}
