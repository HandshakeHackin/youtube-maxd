/* Reads a release ZIP; see tube_update_zip.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The rules an archive must meet are those of the update helper in
 * ps5-native-app-boilerplate (same author): one app, relative paths, no links
 * or special files, no encryption, no duplicates, bounded sizes, files 0777. */
#include "tube_update_zip.h"
#include "tube_update_fs.h"

#include <zlib.h>

#define ZIP_MAX_ENTRIES 8192
#define ZIP_MAX_NAME 512
#define ZIP_MAX_DEPTH 16
#define ZIP_BUFFER 65536

typedef struct {
    char *name;               /* relative to the app folder; "" never */
    int folder, method;
    uint32_t crc;
    uint64_t packed, size, header;
} zip_entry;

struct tube_update_zip {
    FILE *file;
    uint64_t length, directory, bytes;
    zip_entry *entries;
    int count, files;
};

static uint32_t le16(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const unsigned char *p) { return le16(p) | le16(p + 2) << 16; }

static void *refuse(tube_update_zip *zip, char *error, size_t size, const char *why) {
    if (error && size) snprintf(error, size, "%s", why);
    tube_update_zip_close(zip);
    return NULL;
}

static int fail(char *error, size_t size, const char *why) {
    if (error && size) snprintf(error, size, "%s", why);
    return -1;
}

static int read_at(FILE *file, uint64_t offset, void *out, size_t size) {
    return fseeko(file, (off_t)offset, SEEK_SET) == 0 && fread(out, 1, size, file) == size ? 0 : -1;
}

/* A relative path of plain segments: no "", ".", "..", backslash, colon or control character. */
static int safe_name(const char *name, size_t length) {
    size_t segment = 0;
    int depth = 1;
    if (!length || length > ZIP_MAX_NAME || name[0] == '/') return 0;
    for (size_t i = 0; i <= length; ++i) {
        const unsigned char c = i < length ? (unsigned char)name[i] : '/';
        if (c == '/') {
            if (i == length && !segment) break;   /* a folder's trailing slash */
            if (!segment || (segment == 1 && name[i - 1] == '.') || (segment == 2 && name[i - 1] == '.' && name[i - 2] == '.')) return 0;
            segment = 0;
            if (i < length && ++depth > ZIP_MAX_DEPTH) return 0;
        } else {
            if (c < 0x20 || c == 0x7f || c == '\\' || c == ':') return 0;
            ++segment;
        }
    }
    return 1;
}

tube_update_zip *tube_update_zip_open(const char *archive, const char *title, uint64_t max_bytes, char *error, size_t error_size) {
    enum { TAIL = 22 + 65535 };
    unsigned char *tail = NULL, *directory = NULL, end[22];
    const size_t prefix = strlen(title) + 1;
    tube_update_zip *zip = (tube_update_zip *)calloc(1, sizeof *zip);
    if (!zip) return refuse(zip, error, error_size, "Out of memory");
    zip->file = fopen(archive, "rb");
    if (!zip->file || fseeko(zip->file, 0, SEEK_END) != 0) return refuse(zip, error, error_size, "The download could not be opened");
    zip->length = (uint64_t)ftello(zip->file);
    size_t window = zip->length < TAIL ? (size_t)zip->length : TAIL, at = 0;
    int found = 0;
    tail = (unsigned char *)malloc(TAIL);
    if (tail && window >= 22 && read_at(zip->file, zip->length - window, tail, window) == 0)
        for (at = window - 22;; --at) {
            if (le32(tail + at) == 0x06054b50 && at + 22 + le16(tail + at + 20) == window) { found = 1; break; }
            if (!at) break;
        }
    if (found) memcpy(end, tail + at, sizeof end);
    free(tail);
    if (!found) return refuse(zip, error, error_size, "The download is not a ZIP archive");
    const uint32_t total = le16(end + 10), size = le32(end + 12), offset = le32(end + 16);
    if (le16(end + 4) || le16(end + 6) || le16(end + 8) != total || total == 0xffff || size == 0xffffffffu || offset == 0xffffffffu)
        return refuse(zip, error, error_size, "The archive spans disks or is too large (ZIP64)");
    if (!total || total > ZIP_MAX_ENTRIES || (uint64_t)offset + size > zip->length - window + at)
        return refuse(zip, error, error_size, "The archive's directory is damaged");
    zip->directory = offset;
    directory = (unsigned char *)malloc(size ? size : 1);
    zip->entries = (zip_entry *)calloc(total, sizeof *zip->entries);
    if (!directory || !zip->entries || read_at(zip->file, offset, directory, size) != 0) {
        free(directory);
        return refuse(zip, error, error_size, "The archive's directory could not be read");
    }
    const char *why = NULL;
    size_t cursor = 0;
    for (uint32_t i = 0; i < total && !why; ++i) {
        const unsigned char *p = directory + cursor;
        if (cursor + 46 > size || le32(p) != 0x02014b50) { why = "The archive's directory is damaged"; break; }
        const uint32_t flags = le16(p + 8), method = le16(p + 10), name_length = le16(p + 28);
        const uint32_t mode = le32(p + 38) >> 16, host = p[5];
        const size_t step = 46 + (size_t)name_length + le16(p + 30) + le16(p + 32);
        const char *name = (const char *)p + 46;
        if (cursor + step > size) { why = "The archive's directory is damaged"; break; }
        cursor += step;
        zip_entry *entry = &zip->entries[zip->count];
        entry->crc = le32(p + 16); entry->packed = le32(p + 20); entry->size = le32(p + 24); entry->header = le32(p + 42);
        entry->method = (int)method;
        if (flags & 0x2041) why = "The archive is encrypted";
        else if (method != 0 && method != 8) why = "The archive uses a compression this app cannot read";
        else if (entry->packed == 0xffffffffu || entry->size == 0xffffffffu || entry->header == 0xffffffffu) why = "The archive is too large (ZIP64)";
        else if (!safe_name(name, name_length)) why = "The archive holds an unsafe path";
        else if (name_length < prefix || memcmp(name, title, prefix - 1) || name[prefix - 1] != '/') why = "The archive is not this app's folder";
        else if (host == 3 && (mode & 0170000) && (mode & 0170000) != 0100000 && (mode & 0170000) != 0040000) why = "The archive holds a link or a special file";
        else if (entry->header + 30 > zip->directory) why = "The archive's directory is damaged";
        if (why) break;
        entry->folder = name[name_length - 1] == '/' || (host == 3 && (mode & 0170000) == 0040000);
        size_t length = name_length - prefix;
        if (length && name[prefix + length - 1] == '/') --length;
        if (!length) continue;   /* the app folder itself */
        if (entry->folder && entry->size) { why = "The archive's directory is damaged"; break; }
        entry->name = (char *)malloc(length + 1);
        if (!entry->name) { why = "Out of memory"; break; }
        memcpy(entry->name, name + prefix, length); entry->name[length] = 0;
        for (int j = 0; j < zip->count && !why; ++j)
            if (!strcmp(zip->entries[j].name, entry->name)) why = "The archive names a file twice";
        ++zip->count;   /* counted first, so a refusal frees its name too */
        if (!entry->folder) { zip->bytes += entry->size; ++zip->files; }
        if (!why && zip->bytes > max_bytes) why = "The archive unpacks to more than an update may hold";
    }
    free(directory);
    if (!why && !zip->files) why = "The archive is empty";
    if (why) return refuse(zip, error, error_size, why);
    return zip;
}

void tube_update_zip_close(tube_update_zip *zip) {
    if (!zip) return;
    for (int i = 0; i < zip->count; ++i) free(zip->entries[i].name);
    free(zip->entries);
    if (zip->file) fclose(zip->file);
    free(zip);
}

uint64_t tube_update_zip_bytes(const tube_update_zip *zip) { return zip->bytes; }
int tube_update_zip_files(const tube_update_zip *zip) { return zip->files; }

/* Unpacks one file's data to `out`; checks packed and unpacked sizes and the CRC. */
static int extract(tube_update_zip *zip, const zip_entry *entry, FILE *out, uint64_t *done,
                   tube_update_zip_progress progress, void *context) {
    unsigned char header[30], *in = NULL, *unpacked = NULL;
    z_stream stream;
    uint64_t left = entry->packed, written = 0;
    uint32_t crc = (uint32_t)crc32(0L, Z_NULL, 0);
    int result = -1, inflating = 0, ended = entry->method == 0;
    if (read_at(zip->file, entry->header, header, sizeof header) != 0 || le32(header) != 0x04034b50) return -1;
    const uint64_t data = entry->header + 30 + le16(header + 26) + le16(header + 28);
    if (data + entry->packed > zip->directory || fseeko(zip->file, (off_t)data, SEEK_SET) != 0) return -1;
    in = (unsigned char *)malloc(ZIP_BUFFER); unpacked = (unsigned char *)malloc(ZIP_BUFFER);
    memset(&stream, 0, sizeof stream);
    if (!in || !unpacked) goto end;
    if (entry->method == 8) {
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) goto end;
        inflating = 1;
    }
    while (left || (entry->method == 8 && !ended)) {
        const size_t take = left < ZIP_BUFFER ? (size_t)left : ZIP_BUFFER;
        if (take && fread(in, 1, take, zip->file) != take) goto end;
        left -= take;
        if (entry->method == 0) {
            crc = (uint32_t)crc32(crc, in, (uInt)take); written += take;
            if (written > entry->size || (out && fwrite(in, 1, take, out) != take)) goto end;
        } else {
            if (!take && !stream.avail_in) goto end;   /* the data ended before the stream did */
            stream.next_in = in; stream.avail_in = (uInt)take;
            do {
                stream.next_out = unpacked; stream.avail_out = ZIP_BUFFER;
                const int code = inflate(&stream, Z_NO_FLUSH);
                const size_t made = ZIP_BUFFER - stream.avail_out;
                if (code != Z_OK && code != Z_STREAM_END && code != Z_BUF_ERROR) goto end;
                crc = (uint32_t)crc32(crc, unpacked, (uInt)made); written += made;
                if (written > entry->size || (out && made && fwrite(unpacked, 1, made, out) != made)) goto end;
                if (code == Z_STREAM_END) { ended = 1; break; }
                if (code == Z_BUF_ERROR && !made) break;   /* needs more input */
            } while (stream.avail_in || !stream.avail_out);
            if (ended && (stream.avail_in || left)) goto end;
        }
        if (progress && progress(context, *done + written, zip->bytes) != 0) { result = -2; goto end; }
    }
    if (ended && written == entry->size && crc == entry->crc) result = 0;
end:
    if (inflating) inflateEnd(&stream);
    free(in); free(unpacked);
    if (result == 0) *done += written;
    return result;
}

/* Every folder from `root` down to `path`, each open to all: the console starts
 * an app only when its files and folders are (as copied ones are). */
static int make_folders(const char *root, const char *path) {
    char partial[TUBE_UPDATE_PATH];
    const size_t length = strlen(path);
    if (length >= sizeof partial) return -1;
    memcpy(partial, path, length + 1);
    for (size_t i = strlen(root) + 1; i <= length; ++i)
        if (partial[i] == '/' || !partial[i]) {
            const char kept = partial[i];
            partial[i] = 0;
            if ((tube_update_mkdir(partial) != 0 && tube_update_kind(partial) != 2) || chmod(partial, 0777) != 0) return -1;
            partial[i] = kept;
        }
    return 0;
}

int tube_update_zip_unpack(tube_update_zip *zip, const char *destination, tube_update_zip_progress progress, void *context,
                           char *error, size_t error_size) {
    uint64_t done = 0;
    if (tube_update_mkdir(destination) != 0 && tube_update_kind(destination) != 2) return fail(error, error_size, "The staging folder could not be created");
    (void)chmod(destination, 0777);
    for (int i = 0; i < zip->count; ++i) {
        const zip_entry *entry = &zip->entries[i];
        char path[TUBE_UPDATE_PATH], parent[TUBE_UPDATE_PATH];
        if (tube_update_join(path, sizeof path, destination, entry->name) != 0) return fail(error, error_size, "A path in the archive is too long");
        snprintf(parent, sizeof parent, "%s", path);
        if (!entry->folder) *strrchr(parent, '/') = 0;
        if (make_folders(destination, parent) != 0) return fail(error, error_size, "A folder could not be created while unpacking");
        if (entry->folder) continue;
        const int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0777);
        FILE *out = descriptor >= 0 ? fdopen(descriptor, "wb") : NULL;
        if (!out) { if (descriptor >= 0) close(descriptor); return fail(error, error_size, "A file could not be created while unpacking"); }
        int result = extract(zip, entry, out, &done, progress, context);
        if (result == 0 && (fflush(out) != 0 || fchmod(fileno(out), 0777) != 0 || fsync(fileno(out)) != 0)) result = -3;
        if (fclose(out) != 0 && result == 0) result = -3;
        if (result == -2) return fail(error, error_size, "Cancelled");
        if (result == -3) return fail(error, error_size, "A file could not be written while unpacking. Is the storage full?");
        if (result != 0) return fail(error, error_size, "A file in the archive is damaged");
    }
    return 0;
}

/* Regular files below `path`, or -1 when something that is neither file nor folder is there. */
static int count_files(const char *path, int depth) {
    char names[32][256];
    int count = 0, total = 0, seen = 0;
    if (depth > ZIP_MAX_DEPTH + 1) return -1;
    for (int skip = 0;; skip += count) {   /* in batches, to bound the stack */
        tube_update_dir *dir = tube_update_opendir(path);
        if (!dir) return -1;
        count = 0; seen = 0;
        for (struct dirent *entry; count < 32 && (entry = tube_update_readdir(dir)) != NULL;) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            if (seen++ >= skip) snprintf(names[count++], sizeof names[0], "%s", entry->d_name);
        }
        tube_update_closedir(dir);
        for (int i = 0; i < count; ++i) {
            char child[TUBE_UPDATE_PATH];
            if (tube_update_join(child, sizeof child, path, names[i]) != 0) return -1;
            const int kind = tube_update_kind(child);
            if (kind == 1) { ++total; continue; }
            const int below = kind == 2 ? count_files(child, depth + 1) : -1;
            if (below < 0) return -1;
            total += below;
        }
        if (count < 32) break;
    }
    return total;
}

int tube_update_zip_verify(tube_update_zip *zip, const char *destination, char *error, size_t error_size) {
    unsigned char *buffer = (unsigned char *)malloc(ZIP_BUFFER);
    const char *why = buffer ? NULL : "Out of memory";
    for (int i = 0; i < zip->count && !why; ++i) {
        const zip_entry *entry = &zip->entries[i];
        char path[TUBE_UPDATE_PATH];
        struct stat info;
        if (tube_update_join(path, sizeof path, destination, entry->name) != 0 || lstat(path, &info) != 0) { why = "An unpacked file is missing"; break; }
        if ((info.st_mode & 0777) != 0777) { why = "An unpacked file has the wrong permissions"; break; }
        if (entry->folder) { if (!S_ISDIR(info.st_mode)) why = "An unpacked folder is missing"; continue; }
        if (!S_ISREG(info.st_mode) || (uint64_t)info.st_size != entry->size) { why = "An unpacked file has the wrong size"; break; }
        FILE *file = fopen(path, "rb");
        uint32_t crc = (uint32_t)crc32(0L, Z_NULL, 0);
        size_t got;
        if (!file) { why = "An unpacked file could not be read back"; break; }
        while ((got = fread(buffer, 1, ZIP_BUFFER, file)) > 0) crc = (uint32_t)crc32(crc, buffer, (uInt)got);
        if (ferror(file) || crc != entry->crc) why = "An unpacked file does not match the archive";
        fclose(file);
    }
    free(buffer);
    if (!why && count_files(destination, 0) != zip->files) why = "The staging folder holds something the archive does not";
    return why ? fail(error, error_size, why) : 0;
}
