/* Reads a release ZIP: one app folder, validated before anything is written.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_UPDATE_ZIP_H
#define TUBE_UPDATE_ZIP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct tube_update_zip tube_update_zip;
/* Non-zero ends the operation ("cancelled"). */
typedef int (*tube_update_zip_progress)(void *context, uint64_t done, uint64_t total);

/* Opens `archive` and checks its whole directory: every entry inside the one
 * folder `<title>/`, relative names without "..", links, special files,
 * encryption, duplicates or ZIP64, stored or deflated, at most `max_bytes`
 * unpacked. NULL with `error` filled when it is refused. */
tube_update_zip *tube_update_zip_open(const char *archive, const char *title, uint64_t max_bytes, char *error, size_t error_size);
void tube_update_zip_close(tube_update_zip *zip);
uint64_t tube_update_zip_bytes(const tube_update_zip *zip);   /* unpacked size of all files */
int tube_update_zip_files(const tube_update_zip *zip);

/* Writes the folder's content into `destination` (created; must not hold
 * these names), every file and folder with mode 0777, each file checked
 * against its size and CRC-32 as it is written. 0 = done. */
int tube_update_zip_unpack(tube_update_zip *zip, const char *destination, tube_update_zip_progress progress, void *context,
                           char *error, size_t error_size);
/* Reads every unpacked file back from `destination` and compares size and
 * CRC-32 with the archive's directory; nothing else may be there. 0 = identical. */
int tube_update_zip_verify(tube_update_zip *zip, const char *destination, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif
#endif
