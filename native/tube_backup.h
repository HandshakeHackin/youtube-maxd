// Local backup and restore of the ProsperoTube state file.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The provider keeps everything it persists (profiles, favorites, history,
// queues, preferences, the companion address) in one file, `youtube.json`,
// under the application data directory. A backup is a dated, validated copy:
//
//     <data>/backup/youtube-YYYYMMDD-HHMMSS.json          manual or automatic
//     <data>/backup/youtube-YYYYMMDD-HHMMSS-safety.json   state replaced by a restore
//
// PRIVACY: the state file contains the companion pairing token and the remote
// device identity. Nothing is stripped from a backup, so a backup grants the
// same companion access as the live file. Backups therefore stay inside the
// application data directory (/data/youtube-maxd on the console) and this
// module never writes anywhere else. Restoring an old backup also restores
// the token it held; if the companion was paired again since, pair once more.
//
// Times are UTC seconds since 1970 supplied by the caller (time(NULL) in the
// application, a fixed clock in tests). File names carry UTC, not local time.
//
// After tube_backup_restore() succeeds the caller must reload the state file
// before anything saves the in-memory state again, or the restore is lost.
#ifndef TUBE_BACKUP_H
#define TUBE_BACKUP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TUBE_BACKUP_STATE_FILE "youtube.json"
#define TUBE_BACKUP_DIRECTORY "backup"
/* The provider refuses to load a larger state file, so a backup is bounded too. */
#define TUBE_BACKUP_MAX_BYTES (2 * 1024 * 1024)
/* Top-level "version" values the provider's loader accepts. */
#define TUBE_BACKUP_VERSION_MIN 1
#define TUBE_BACKUP_VERSION_MAX 2
#define TUBE_BACKUP_NAME_SIZE 40
#define TUBE_BACKUP_LIST_MAX 64
#define TUBE_BACKUP_KEEP_DEFAULT 10
/* Safety copies made by restores are rotated separately and kept short. */
#define TUBE_BACKUP_KEEP_SAFETY 3

enum {
    TUBE_BACKUP_OK = 0,
    TUBE_BACKUP_E_ARGUMENT = -1, /* missing argument, path too long, time out of range */
    TUBE_BACKUP_E_NAME = -2,     /* not a backup file name (also every path-like name) */
    TUBE_BACKUP_E_MISSING = -3,  /* no such backup, or no state file to back up */
    TUBE_BACKUP_E_TOO_LARGE = -4,
    TUBE_BACKUP_E_INVALID = -5,  /* not a state file the provider would load */
    TUBE_BACKUP_E_IO = -6        /* could not read, write or rename */
};

/* SmartTube's stored frequency values, in days. */
enum {
    TUBE_BACKUP_AUTO_OFF = -1,
    TUBE_BACKUP_AUTO_DAILY = 1,
    TUBE_BACKUP_AUTO_WEEKLY = 7,
    TUBE_BACKUP_AUTO_MONTHLY = 30
};

typedef struct {
    char name[TUBE_BACKUP_NAME_SIZE]; /* file name inside the backup directory */
    long long created;                /* UTC seconds, from the name */
    long long bytes;
    int safety;                       /* 1 for a copy made by a restore */
} tube_backup_entry;

/* Short English sentence for a result code, for a status line. */
const char *tube_backup_error_text(int code);

/* 1 when `name` is exactly a backup file name with a real calendar time. */
int tube_backup_name_valid(const char *name);
/* Time and kind encoded in a valid name. Returns TUBE_BACKUP_OK or E_NAME. */
int tube_backup_name_info(const char *name, long long *created, int *safety);
/* Name for a time (years 1970 to 9999). */
int tube_backup_format_name(long long utc, int safety, char *out, size_t size);
/* "2026-10-11 14:03 UTC" for a menu row. */
int tube_backup_format_time(long long utc, char *out, size_t size);

/* Checks any file as a state file: size bound, one well-formed JSON object,
 * supported "version", and a "records" array. */
int tube_backup_validate_file(const char *path);

/* Copy the current state file into a dated backup. The state must validate;
 * a corrupt state is never turned into a backup. `name_out` may be NULL. */
int tube_backup_create(const char *data_dir, long long now, char *name_out, size_t name_size);

/* Newest first, at most `max` (and TUBE_BACKUP_LIST_MAX) entries. Files with
 * other names are ignored. Returns the count, 0 when there is no backup
 * directory, or a negative code. */
int tube_backup_list(const char *data_dir, tube_backup_entry *out, int max);

int tube_backup_validate(const char *data_dir, const char *name);

/* Replace the state file with a validated backup. The state being replaced
 * is first copied byte for byte to a "-safety" backup (named in `safety_out`
 * when given; empty when there was no state file). That copy takes the first
 * free name from `now` on, so it never replaces an existing backup, the one
 * being restored included. The replacement is a rename, so a failure leaves
 * the previous state in place. */
int tube_backup_restore(const char *data_dir, const char *name, long long now, char *safety_out, size_t safety_size);

int tube_backup_delete(const char *data_dir, const char *name);

/* Keep the newest `keep` backups (1..TUBE_BACKUP_LIST_MAX) and the newest
 * TUBE_BACKUP_KEEP_SAFETY safety copies. Returns how many files were removed
 * or a negative code. */
int tube_backup_rotate(const char *data_dir, int keep);

/* 1 for the four frequency values above. */
int tube_backup_auto_valid(int frequency_days);

/* Whether an automatic backup is due. `newest` is the time of the newest
 * regular backup, 0 when there is none. Off, an invalid frequency or a clock
 * before 2020 is never due; no backup is always due; a newest backup dated
 * later than `now` is due only when it is further ahead than one period (the
 * clock was moved back a long way). */
int tube_backup_auto_due(int frequency_days, long long now, long long newest);

/* Start-up helper: create and rotate when due. Returns 1 when a backup was
 * written, 0 when none was due or there is no state yet, or a negative code. */
int tube_backup_auto_run(const char *data_dir, int frequency_days, long long now, int keep, char *name_out, size_t name_size);

#ifdef __cplusplus
}
#endif

#endif
