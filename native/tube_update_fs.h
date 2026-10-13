/* File helpers shared by the update engine and its archive reader.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_UPDATE_FS_H
#define TUBE_UPDATE_FS_H
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef EVO_TARGET_PS5
/* The application has neither POSIX mkdir() nor a working opendir(). */
#include "evo_data_path.h"
#include "evo_readdir.h"
typedef evo_dir_t tube_update_dir;
#define tube_update_opendir evo_opendir
#define tube_update_readdir evo_readdir
#define tube_update_closedir evo_closedir
static inline int tube_update_mkdir(const char *path) { return evo_mkdir(path) == 0 ? 0 : -1; }
#else
#include <dirent.h>
typedef DIR tube_update_dir;
#define tube_update_opendir opendir
#define tube_update_readdir readdir
#define tube_update_closedir closedir
static inline int tube_update_mkdir(const char *path) { return mkdir(path, 0777) == 0 || errno == EEXIST ? 0 : -1; }
#endif

#define TUBE_UPDATE_PATH 1024

/* 0 = absent, 1 = a file, 2 = a folder, 3 = anything else (links are not followed). */
static inline int tube_update_kind(const char *path) {
    struct stat info;
    if (lstat(path, &info) != 0) return 0;
    return S_ISREG(info.st_mode) ? 1 : S_ISDIR(info.st_mode) ? 2 : 3;
}

static inline int tube_update_join(char *out, size_t size, const char *folder, const char *leaf) {
    int written = snprintf(out, size, "%s/%s", folder, leaf);
    if (written < 0 || (size_t)written >= size) { if (size) out[0] = 0; return -1; }
    return 0;
}

/* Creates `path` and every missing folder above it, from `root` down. */
static inline int tube_update_mkdirs(const char *root, const char *path) {
    char partial[TUBE_UPDATE_PATH];
    size_t length = strlen(path), start = strlen(root);
    if (length >= sizeof partial || strncmp(root, path, start)) return -1;
    memcpy(partial, path, length + 1);
    for (size_t i = start + 1; i <= length; ++i)
        if (partial[i] == '/' || !partial[i]) {
            const char kept = partial[i];
            partial[i] = 0;
            if (tube_update_mkdir(partial) != 0 && tube_update_kind(partial) != 2) return -1;
            partial[i] = kept;
        }
    return 0;
}

/* Removes a file or a whole folder. 0 = it is gone (or never was). */
static inline int tube_update_remove(const char *path) {
    const int kind = tube_update_kind(path);
    if (!kind) return 0;
    if (kind != 2) return unlink(path) == 0 ? 0 : -1;
    for (;;) {   /* one batch of names at a time: entries are removed while listing */
        char names[32][256];
        int count = 0;
        tube_update_dir *dir = tube_update_opendir(path);
        if (!dir) return -1;
        for (struct dirent *entry; count < 32 && (entry = tube_update_readdir(dir)) != NULL;)
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) snprintf(names[count++], sizeof names[0], "%s", entry->d_name);
        tube_update_closedir(dir);
        if (!count) break;
        for (int i = 0; i < count; ++i) {
            char child[TUBE_UPDATE_PATH];
            if (tube_update_join(child, sizeof child, path, names[i]) != 0 || tube_update_remove(child) != 0) return -1;
        }
    }
    return rmdir(path) == 0 ? 0 : -1;
}

/* A small file as text. Returns its malloc'd, terminated content, or NULL. */
static inline char *tube_update_read(const char *path, size_t limit, size_t *length) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    char *text = (char *)malloc(limit + 1);
    size_t got = text ? fread(text, 1, limit + 1, file) : 0;
    fclose(file);
    if (!text || got > limit) { free(text); return NULL; }
    text[got] = 0;
    if (length) *length = got;
    return text;
}

/* Writes a file completely or not at all: temporary name, flushed to the disk, renamed. */
static inline int tube_update_write(const char *path, const char *text, size_t length) {
    char temporary[TUBE_UPDATE_PATH];
    if (snprintf(temporary, sizeof temporary, "%s.tmp", path) >= (int)sizeof temporary) return -1;
    FILE *file = fopen(temporary, "wb");
    if (!file) return -1;
    int failed = fwrite(text, 1, length, file) != length;
    failed |= fflush(file) != 0 || fsync(fileno(file)) != 0;
    failed |= fclose(file) != 0;
    if (!failed && rename(temporary, path) != 0) failed = 1;
    if (failed) unlink(temporary);
    return failed ? -1 : 0;
}
#endif
