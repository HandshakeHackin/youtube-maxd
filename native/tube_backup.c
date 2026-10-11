// Local backup and restore of the ProsperoTube state file; see tube_backup.h.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tube_backup.h"

#include "cJSON.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef EVO_TARGET_PS5
/* The application sandbox has neither POSIX mkdir() nor a working opendir(). */
#include "evo_data_path.h"
#include "evo_readdir.h"
typedef evo_dir_t backup_dir;
#define backup_opendir evo_opendir
#define backup_readdir evo_readdir
#define backup_closedir evo_closedir
static int backup_mkdir(const char *path) { return evo_mkdir(path) == 0 ? 0 : -1; }
#else
#include <dirent.h>
typedef DIR backup_dir;
#define backup_opendir opendir
#define backup_readdir readdir
#define backup_closedir closedir
static int backup_mkdir(const char *path) { return mkdir(path, 0777) == 0 || errno == EEXIST ? 0 : -1; }
#endif

#define PATH_SIZE 768
#define PREFIX "youtube-"
#define SAFETY_SUFFIX "-safety"
#define EXTENSION ".json"
#define TEMPORARY ".tmp"
/* A state being replaced is copied whatever it holds, within this bound. */
#define RAW_COPY_MAX_BYTES (16LL * 1024 * 1024)
#define SECONDS_PER_DAY 86400LL
#define YEAR_2020 1577836800LL
#define YEAR_10000 253402300800LL

const char *tube_backup_error_text(int code) {
    switch (code) {
    case TUBE_BACKUP_OK: return "Done.";
    case TUBE_BACKUP_E_ARGUMENT: return "The backup request was not valid.";
    case TUBE_BACKUP_E_NAME: return "That is not a ProsperoTube backup file.";
    case TUBE_BACKUP_E_MISSING: return "There is nothing to back up or restore yet.";
    case TUBE_BACKUP_E_TOO_LARGE: return "The file is larger than a ProsperoTube backup can be.";
    case TUBE_BACKUP_E_INVALID: return "The file is damaged or is not a ProsperoTube backup.";
    case TUBE_BACKUP_E_IO: return "The backup could not be read or written. Check the ProsperoTube data directory.";
    default: return "The backup operation failed.";
    }
}

static int leap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

static int month_days(int year, int month) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month - 1] + (month == 2 && leap(year));
}

typedef struct { int year, month, day, hour, minute, second; } civil_time;

static int to_civil(long long utc, civil_time *out) {
    if (utc < 0 || utc >= YEAR_10000) return -1;
    long long days = utc / SECONDS_PER_DAY;
    int rest = (int)(utc % SECONDS_PER_DAY);
    out->year = 1970;
    while (days >= 365 + leap(out->year)) { days -= 365 + leap(out->year); out->year++; }
    out->month = 1;
    while (days >= month_days(out->year, out->month)) { days -= month_days(out->year, out->month); out->month++; }
    out->day = (int)days + 1;
    out->hour = rest / 3600; out->minute = rest / 60 % 60; out->second = rest % 60;
    return 0;
}

static long long from_civil(const civil_time *value) {
    long long days = 0;
    for (int year = 1970; year < value->year; year++) days += 365 + leap(year);
    for (int month = 1; month < value->month; month++) days += month_days(value->year, month);
    days += value->day - 1;
    return days * SECONDS_PER_DAY + value->hour * 3600 + value->minute * 60 + value->second;
}

static int digits(const char *text, int count) {
    int value = 0;
    for (int i = 0; i < count; i++) {
        if (text[i] < '0' || text[i] > '9') return -1;
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

int tube_backup_name_info(const char *name, long long *created, int *safety) {
    civil_time when;
    size_t prefix = sizeof PREFIX - 1;
    if (!name || strncmp(name, PREFIX, prefix)) return TUBE_BACKUP_E_NAME;
    /* strncmp stopped at any early terminator, so the fixed fields are read
     * only after their length is known. */
    if (strlen(name) < prefix + 15) return TUBE_BACKUP_E_NAME;
    const char *at = name + prefix;
    when.year = digits(at, 4); when.month = digits(at + 4, 2); when.day = digits(at + 6, 2);
    when.hour = digits(at + 9, 2); when.minute = digits(at + 11, 2); when.second = digits(at + 13, 2);
    if (at[8] != '-' || when.year < 1970 || when.month < 1 || when.month > 12 || when.day < 1 ||
        when.day > month_days(when.year, when.month) || when.hour < 0 || when.hour > 23 ||
        when.minute < 0 || when.minute > 59 || when.second < 0 || when.second > 59)
        return TUBE_BACKUP_E_NAME;
    at += 15;
    int is_safety = !strncmp(at, SAFETY_SUFFIX, sizeof SAFETY_SUFFIX - 1);
    if (is_safety) at += sizeof SAFETY_SUFFIX - 1;
    if (strcmp(at, EXTENSION)) return TUBE_BACKUP_E_NAME;
    if (created) *created = from_civil(&when);
    if (safety) *safety = is_safety;
    return TUBE_BACKUP_OK;
}

int tube_backup_name_valid(const char *name) { return tube_backup_name_info(name, NULL, NULL) == TUBE_BACKUP_OK; }

int tube_backup_format_name(long long utc, int safety, char *out, size_t size) {
    civil_time when;
    if (!out || !size) return TUBE_BACKUP_E_ARGUMENT;
    out[0] = 0;
    if (to_civil(utc, &when)) return TUBE_BACKUP_E_ARGUMENT;
    int written = snprintf(out, size, PREFIX "%04d%02d%02d-%02d%02d%02d%s" EXTENSION, when.year, when.month, when.day,
                           when.hour, when.minute, when.second, safety ? SAFETY_SUFFIX : "");
    if (written < 0 || (size_t)written >= size) { out[0] = 0; return TUBE_BACKUP_E_ARGUMENT; }
    return TUBE_BACKUP_OK;
}

int tube_backup_format_time(long long utc, char *out, size_t size) {
    civil_time when;
    if (!out || !size) return TUBE_BACKUP_E_ARGUMENT;
    out[0] = 0;
    if (to_civil(utc, &when)) return TUBE_BACKUP_E_ARGUMENT;
    int written = snprintf(out, size, "%04d-%02d-%02d %02d:%02d UTC", when.year, when.month, when.day, when.hour, when.minute);
    if (written < 0 || (size_t)written >= size) { out[0] = 0; return TUBE_BACKUP_E_ARGUMENT; }
    return TUBE_BACKUP_OK;
}

/* <data_dir>/<a>[/<b>][suffix]; fails rather than truncating. */
static int join(char *out, const char *data_dir, const char *a, const char *b, const char *suffix) {
    if (!data_dir || !*data_dir) return TUBE_BACKUP_E_ARGUMENT;
    int written = snprintf(out, PATH_SIZE, "%s/%s%s%s%s", data_dir, a, b ? "/" : "", b ? b : "", suffix ? suffix : "");
    return written < 0 || written >= PATH_SIZE ? TUBE_BACKUP_E_ARGUMENT : TUBE_BACKUP_OK;
}

int tube_backup_validate_file(const char *path) {
    if (!path || !*path) return TUBE_BACKUP_E_ARGUMENT;
    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? TUBE_BACKUP_E_MISSING : TUBE_BACKUP_E_IO;
    long size = -1;
    if (fseek(file, 0, SEEK_END) == 0) size = ftell(file);
    if (size < 0) { fclose(file); return TUBE_BACKUP_E_IO; }
    if (size > TUBE_BACKUP_MAX_BYTES) { fclose(file); return TUBE_BACKUP_E_TOO_LARGE; }
    if (size == 0) { fclose(file); return TUBE_BACKUP_E_INVALID; }
    rewind(file);
    char *body = malloc((size_t)size + 1);
    if (!body) { fclose(file); return TUBE_BACKUP_E_IO; }
    size_t got = fread(body, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) { free(body); return TUBE_BACKUP_E_IO; }
    body[size] = 0;
    /* cJSON skips control bytes between tokens, NUL included; a text file
     * with one in it is damaged. Anything after the object must be blank. */
    const char *end = NULL;
    cJSON *root = memchr(body, 0, (size_t)size) ? NULL : cJSON_ParseWithLengthOpts(body, (size_t)size + 1, &end, 0);
    int valid = root && cJSON_IsObject(root) && end;
    for (const char *tail = end; valid && tail < body + size; tail++)
        if (*tail != ' ' && *tail != '\n' && *tail != '\r' && *tail != '\t') valid = 0;
    if (valid) {
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
        valid = cJSON_IsNumber(version) && version->valuedouble >= TUBE_BACKUP_VERSION_MIN &&
                version->valuedouble <= TUBE_BACKUP_VERSION_MAX && version->valuedouble == (double)(int)version->valuedouble &&
                cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(root, "records"));
    }
    cJSON_Delete(root);
    free(body);
    return valid ? TUBE_BACKUP_OK : TUBE_BACKUP_E_INVALID;
}

/* Copy through `<to><TEMPORARY>` and rename, so `to` is either the complete
 * old file or the complete new one. */
static int copy_file(const char *from, const char *to, long long limit) {
    char temporary[PATH_SIZE + sizeof TEMPORARY], buffer[8192];
    snprintf(temporary, sizeof temporary, "%s" TEMPORARY, to);
    FILE *in = fopen(from, "rb");
    if (!in) return errno == ENOENT ? TUBE_BACKUP_E_MISSING : TUBE_BACKUP_E_IO;
    FILE *out = fopen(temporary, "wb");
    if (!out) { fclose(in); return TUBE_BACKUP_E_IO; }
    int result = TUBE_BACKUP_OK;
    long long total = 0;
    size_t got;
    while (result == TUBE_BACKUP_OK && (got = fread(buffer, 1, sizeof buffer, in)) > 0) {
        total += (long long)got;
        if (total > limit) result = TUBE_BACKUP_E_TOO_LARGE;
        else if (fwrite(buffer, 1, got, out) != got) result = TUBE_BACKUP_E_IO;
    }
    if (ferror(in)) result = result == TUBE_BACKUP_OK ? TUBE_BACKUP_E_IO : result;
    fclose(in);
    if (result == TUBE_BACKUP_OK && fflush(out) != 0) result = TUBE_BACKUP_E_IO;
#ifndef TUBE_BACKUP_NO_FSYNC
    if (result == TUBE_BACKUP_OK && fsync(fileno(out)) != 0) result = TUBE_BACKUP_E_IO;
#endif
    if (fclose(out) != 0 && result == TUBE_BACKUP_OK) result = TUBE_BACKUP_E_IO;
    if (result == TUBE_BACKUP_OK && rename(temporary, to) != 0) result = TUBE_BACKUP_E_IO;
    if (result != TUBE_BACKUP_OK) remove(temporary);
    return result;
}

static int ensure_directory(const char *data_dir, char *directory) {
    int result = join(directory, data_dir, TUBE_BACKUP_DIRECTORY, NULL, NULL);
    if (result != TUBE_BACKUP_OK) return result;
    return backup_mkdir(directory) == 0 ? TUBE_BACKUP_OK : TUBE_BACKUP_E_IO;
}

int tube_backup_create(const char *data_dir, long long now, char *name_out, size_t name_size) {
    char state[PATH_SIZE], directory[PATH_SIZE], target[PATH_SIZE], name[TUBE_BACKUP_NAME_SIZE];
    if (name_out && name_size) name_out[0] = 0;
    int result = join(state, data_dir, TUBE_BACKUP_STATE_FILE, NULL, NULL);
    if (result == TUBE_BACKUP_OK) result = tube_backup_format_name(now, 0, name, sizeof name);
    if (result == TUBE_BACKUP_OK && name_out && strlen(name) >= name_size) result = TUBE_BACKUP_E_ARGUMENT;
    if (result == TUBE_BACKUP_OK) result = join(target, data_dir, TUBE_BACKUP_DIRECTORY, name, TEMPORARY);
    if (result == TUBE_BACKUP_OK) result = join(target, data_dir, TUBE_BACKUP_DIRECTORY, name, NULL);
    if (result == TUBE_BACKUP_OK) result = tube_backup_validate_file(state);
    if (result == TUBE_BACKUP_OK) result = ensure_directory(data_dir, directory);
    if (result == TUBE_BACKUP_OK) result = copy_file(state, target, TUBE_BACKUP_MAX_BYTES);
    if (result == TUBE_BACKUP_OK && name_out) memcpy(name_out, name, strlen(name) + 1);
    return result;
}

/* Newest `max` entries of one kind (0 regular, 1 safety, -1 both). Names are
 * fixed-width, so descending name order is newest first. */
static int scan(const char *data_dir, int kind, tube_backup_entry *out, int max) {
    char directory[PATH_SIZE], path[PATH_SIZE];
    int count = 0, result = join(directory, data_dir, TUBE_BACKUP_DIRECTORY, NULL, NULL);
    if (result != TUBE_BACKUP_OK) return result;
    backup_dir *dir = backup_opendir(directory);
    if (!dir) return errno == ENOENT ? 0 : TUBE_BACKUP_E_IO;
    struct dirent *entry;
    while ((entry = backup_readdir(dir)) != NULL) {
        tube_backup_entry found;
        struct stat info;
        memset(&found, 0, sizeof found);
        if (tube_backup_name_info(entry->d_name, &found.created, &found.safety) != TUBE_BACKUP_OK) continue;
        if (kind >= 0 && found.safety != kind) continue;
        if (join(path, data_dir, TUBE_BACKUP_DIRECTORY, entry->d_name, NULL) != TUBE_BACKUP_OK) continue;
        if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) continue;
        found.bytes = (long long)info.st_size;
        memcpy(found.name, entry->d_name, strlen(entry->d_name) + 1);
        int at = count;
        while (at > 0 && strcmp(found.name, out[at - 1].name) > 0) at--;
        if (at >= max) continue;
        if (count < max) count++;
        memmove(out + at + 1, out + at, (size_t)(count - at - 1) * sizeof *out);
        out[at] = found;
    }
    backup_closedir(dir);
    return count;
}

int tube_backup_list(const char *data_dir, tube_backup_entry *out, int max) {
    if (!out || max < 1) return TUBE_BACKUP_E_ARGUMENT;
    return scan(data_dir, -1, out, max > TUBE_BACKUP_LIST_MAX ? TUBE_BACKUP_LIST_MAX : max);
}

static int backup_path(char *out, const char *data_dir, const char *name) {
    if (!tube_backup_name_valid(name)) return TUBE_BACKUP_E_NAME;
    return join(out, data_dir, TUBE_BACKUP_DIRECTORY, name, NULL);
}

int tube_backup_validate(const char *data_dir, const char *name) {
    char path[PATH_SIZE];
    int result = backup_path(path, data_dir, name);
    return result == TUBE_BACKUP_OK ? tube_backup_validate_file(path) : result;
}

/* Name and path of a safety copy made at `now`. Names carry seconds, and a
 * copy never replaces an existing one - least of all the backup a restore is
 * reading - so a taken name gives way to the next free second. */
static int free_safety_name(const char *data_dir, long long now, char *name, char *path) {
    enum { SECONDS = 3600 };
    for (int i = 0; i < SECONDS; i++) {
        struct stat info;
        int result = tube_backup_format_name(now + i, 1, name, TUBE_BACKUP_NAME_SIZE);
        if (result == TUBE_BACKUP_OK) result = join(path, data_dir, TUBE_BACKUP_DIRECTORY, name, TEMPORARY);
        if (result == TUBE_BACKUP_OK) result = join(path, data_dir, TUBE_BACKUP_DIRECTORY, name, NULL);
        if (result != TUBE_BACKUP_OK) return result;
        if (stat(path, &info) != 0) return errno == ENOENT ? TUBE_BACKUP_OK : TUBE_BACKUP_E_IO;
    }
    return TUBE_BACKUP_E_IO;
}

int tube_backup_restore(const char *data_dir, const char *name, long long now, char *safety_out, size_t safety_size) {
    char source[PATH_SIZE], state[PATH_SIZE], directory[PATH_SIZE], safety[PATH_SIZE], safety_name[TUBE_BACKUP_NAME_SIZE];
    if (safety_out && safety_size) safety_out[0] = 0;
    int result = backup_path(source, data_dir, name);
    if (result == TUBE_BACKUP_OK) result = join(state, data_dir, TUBE_BACKUP_STATE_FILE, NULL, TEMPORARY);
    if (result == TUBE_BACKUP_OK) result = join(state, data_dir, TUBE_BACKUP_STATE_FILE, NULL, NULL);
    if (result == TUBE_BACKUP_OK) result = free_safety_name(data_dir, now, safety_name, safety);
    if (result == TUBE_BACKUP_OK && safety_out && strlen(safety_name) >= safety_size) result = TUBE_BACKUP_E_ARGUMENT;
    if (result == TUBE_BACKUP_OK) result = tube_backup_validate_file(source);
    if (result == TUBE_BACKUP_OK) result = ensure_directory(data_dir, directory);
    if (result != TUBE_BACKUP_OK) return result;
    /* The state about to be replaced is kept whatever it contains: it may be
     * the very file the user is trying to recover from. */
    result = copy_file(state, safety, RAW_COPY_MAX_BYTES);
    if (result == TUBE_BACKUP_OK) { if (safety_out) memcpy(safety_out, safety_name, strlen(safety_name) + 1); }
    else if (result != TUBE_BACKUP_E_MISSING) return result;
    return copy_file(source, state, TUBE_BACKUP_MAX_BYTES);
}

int tube_backup_delete(const char *data_dir, const char *name) {
    char path[PATH_SIZE];
    int result = backup_path(path, data_dir, name);
    if (result != TUBE_BACKUP_OK) return result;
    if (remove(path) == 0) return TUBE_BACKUP_OK;
    return errno == ENOENT ? TUBE_BACKUP_E_MISSING : TUBE_BACKUP_E_IO;
}

/* A leftover from an interrupted copy: a backup name followed by ".tmp". */
static int stale_temporary(const char *name) {
    char stem[TUBE_BACKUP_NAME_SIZE];
    size_t length = strlen(name), suffix = sizeof TEMPORARY - 1;
    if (length <= suffix || length - suffix >= sizeof stem || strcmp(name + length - suffix, TEMPORARY)) return 0;
    memcpy(stem, name, length - suffix);
    stem[length - suffix] = 0;
    return tube_backup_name_valid(stem);
}

int tube_backup_rotate(const char *data_dir, int keep) {
    enum { BATCH = 32, PASSES = 256 };
    static const int kinds[2] = {0, 1};
    char directory[PATH_SIZE], path[PATH_SIZE], victims[BATCH][TUBE_BACKUP_NAME_SIZE + sizeof TEMPORARY];
    tube_backup_entry kept[TUBE_BACKUP_LIST_MAX];
    int removed = 0;
    if (keep < 1 || keep > TUBE_BACKUP_LIST_MAX) return TUBE_BACKUP_E_ARGUMENT;
    int result = join(directory, data_dir, TUBE_BACKUP_DIRECTORY, NULL, NULL);
    if (result != TUBE_BACKUP_OK) return result;
    for (int pass = 0; pass < PASSES; pass++) {
        char oldest_kept[2][TUBE_BACKUP_NAME_SIZE] = {"", ""};
        int found = 0;
        for (int k = 0; k < 2; k++) {
            int limit = kinds[k] ? TUBE_BACKUP_KEEP_SAFETY : keep, count = scan(data_dir, kinds[k], kept, limit);
            if (count < 0) return count;
            if (count == limit) memcpy(oldest_kept[k], kept[count - 1].name, sizeof oldest_kept[k]);
        }
        backup_dir *dir = backup_opendir(directory);
        if (!dir) return errno == ENOENT ? removed : TUBE_BACKUP_E_IO;
        struct dirent *entry;
        while (found < BATCH && (entry = backup_readdir(dir)) != NULL) {
            int safety = 0, old = 0;
            if (tube_backup_name_info(entry->d_name, NULL, &safety) == TUBE_BACKUP_OK)
                old = *oldest_kept[safety] && strcmp(entry->d_name, oldest_kept[safety]) < 0;
            else if (!stale_temporary(entry->d_name)) continue;
            else old = -1;
            /* remove() would also take an empty directory; only files are ours. */
            struct stat info;
            if (!old || join(path, data_dir, TUBE_BACKUP_DIRECTORY, entry->d_name, NULL) != TUBE_BACKUP_OK ||
                stat(path, &info) != 0 || !S_ISREG(info.st_mode))
                continue;
            memcpy(victims[found++], entry->d_name, strlen(entry->d_name) + 1);
        }
        backup_closedir(dir);
        if (!found) break;
        for (int i = 0; i < found; i++) {
            if (join(path, data_dir, TUBE_BACKUP_DIRECTORY, victims[i], NULL) != TUBE_BACKUP_OK) return TUBE_BACKUP_E_ARGUMENT;
            if (remove(path) != 0 && errno != ENOENT) return TUBE_BACKUP_E_IO;
            if (!stale_temporary(victims[i])) removed++;
        }
    }
    return removed;
}

int tube_backup_auto_valid(int frequency_days) {
    return frequency_days == TUBE_BACKUP_AUTO_OFF || frequency_days == TUBE_BACKUP_AUTO_DAILY ||
           frequency_days == TUBE_BACKUP_AUTO_WEEKLY || frequency_days == TUBE_BACKUP_AUTO_MONTHLY;
}

int tube_backup_auto_due(int frequency_days, long long now, long long newest) {
    if (!tube_backup_auto_valid(frequency_days) || frequency_days == TUBE_BACKUP_AUTO_OFF) return 0;
    if (now < YEAR_2020 || now >= YEAR_10000) return 0;
    if (newest <= 0) return 1;
    long long period = frequency_days * SECONDS_PER_DAY;
    if (newest > now) return newest - now > period;
    return now - newest >= period;
}

int tube_backup_auto_run(const char *data_dir, int frequency_days, long long now, int keep, char *name_out, size_t name_size) {
    tube_backup_entry newest;
    if (name_out && name_size) name_out[0] = 0;
    if (keep < 1 || keep > TUBE_BACKUP_LIST_MAX) return TUBE_BACKUP_E_ARGUMENT;
    int count = scan(data_dir, 0, &newest, 1);
    if (count < 0) return count;
    if (!tube_backup_auto_due(frequency_days, now, count ? newest.created : 0)) return 0;
    int result = tube_backup_create(data_dir, now, name_out, name_size);
    if (result == TUBE_BACKUP_E_MISSING) return 0;
    if (result != TUBE_BACKUP_OK) return result;
    result = tube_backup_rotate(data_dir, keep);
    return result < 0 ? result : 1;
}
