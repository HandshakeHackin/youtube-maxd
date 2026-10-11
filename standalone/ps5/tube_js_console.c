/* What PacBrew's libcurl 8.18.0 needs to run inside the PS5 app. The pattern is
 * the one proven on hardware in the ps5-native-app-boilerplate update check
 * (examples/update-check/console_curl.c, GPL-3.0-or-later, same author); name
 * lookup, _setjmp, dladdr, recvmmsg and sendmmsg are left out because the EVO
 * build tail already defines them (tools/native-app/stubs/evo_dns.c, libc_ext.c).
 *
 * This file is partially linked with tube_js_http.o and libcurl.a under
 * --wrap=fcntl (see build_ps5.sh), so the fcntl stand-in below reaches only
 * libcurl's calls and the rest of the app keeps the fcntl it has today.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_js_console.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

extern const char *sceKernelGetFsSandboxRandomWord(void);

enum { CONSOLE_SO_NBIO = 0x1200 };   /* the console's own non-blocking socket option */

int tube_js_console_socket_setup(int socket) {
    const int on = 1;
    return setsockopt(socket, SOL_SOCKET, CONSOLE_SO_NBIO, &on, sizeof on);
}

static int readable(const char *path) {
    struct stat facts;
    return path && *path && stat(path, &facts) == 0 && S_ISREG(facts.st_mode) && facts.st_size > 0;
}

const char *tube_js_console_ca_file(const char *packaged) {
    static char path[192];
    if (readable(packaged)) return packaged;
    /* Elevated, the system list is at its real path; sandboxed, under the
     * sandbox's random word. */
    snprintf(path, sizeof path, "/system/common/cert/CA_LIST.cer");
    if (!readable(path)) {
        const char *word = sceKernelGetFsSandboxRandomWord();
        snprintf(path, sizeof path, "/%s/common/cert/CA_LIST.cer", word ? word : "");
    }
    return path;
}

/* ---- libc stand-ins ----
 * Names libcurl references that the SDK's static libc.a would otherwise supply
 * with raw system calls (if_nametoindex and getifaddrs pull in its syscalls.o,
 * which is written for payloads, not for a signed app) or that nothing
 * supplies. build_ps5.sh makes every symbol of the joined object local except
 * tube_js_*, so these never meet another definition in the app. */

struct passwd;
/* No user database: "no such user", so libcurl looks for no home folder. */
int getpwuid_r(unsigned user, struct passwd *entry, char *buffer, size_t size, struct passwd **result) {
    (void)user; (void)entry; (void)buffer; (void)size;
    *result = NULL;
    return ENOENT;
}

/* Only CURLOPT_INTERFACE and IPv6 scope names ask; the app uses neither. */
unsigned if_nametoindex(const char *name) {
    (void)name;
    return 0;
}

struct ifaddrs;
int getifaddrs(struct ifaddrs **list) {
    *list = NULL;
    errno = ENOSYS;
    return -1;
}

void freeifaddrs(struct ifaddrs *list) { (void)list; }

/* The pipe behind curl_multi_wakeup(). The fcntl calls are this file's own
 * stand-in below, like every other fcntl in the joined object. */
int pipe2(int descriptors[2], int flags) {
    if (pipe(descriptors) != 0) return -1;
    for (int i = 0; i < 2; ++i) {
        if (flags & O_NONBLOCK) (void)fcntl(descriptors[i], F_SETFL, fcntl(descriptors[i], F_GETFL) | O_NONBLOCK);
        if (flags & O_CLOEXEC) (void)fcntl(descriptors[i], F_SETFD, FD_CLOEXEC);
    }
    return 0;
}

/* '*' and '?' only: all libcurl's wildcard matching asks for. */
int fnmatch(const char *pattern, const char *text, int flags) {
    for (; *pattern; ++pattern, ++text) {
        if (*pattern == '*') {
            while (*pattern == '*') ++pattern;
            if (!*pattern) return 0;
            for (; *text; ++text)
                if (fnmatch(pattern, text, flags) == 0) return 0;
            return 1;   /* FNM_NOMATCH */
        }
        if (!*text || (*pattern != '?' && *pattern != *text)) return 1;
    }
    return *text ? 1 : 0;
}

/* zstd's optional tracing hooks, referenced weakly by its decoder; 0 = not traced. */
unsigned long long ZSTD_trace_decompress_begin(const void *context) {
    (void)context;
    return 0;
}

void ZSTD_trace_decompress_end(unsigned long long trace, const void *record) {
    (void)trace; (void)record;
}

unsigned long long ZSTD_trace_compress_begin(const void *context) {
    (void)context;
    return 0;
}

void ZSTD_trace_compress_end(unsigned long long trace, const void *record) {
    (void)trace; (void)record;
}

/* ---- fcntl on sockets ----
 * The console's libc refuses fcntl on a socket with EINVAL. libcurl then fails
 * every connect ("fcntl set CLOEXEC: Invalid argument"). Only a refused call is
 * treated as a socket; everything else goes to the real fcntl unchanged. */

extern int __real_fcntl(int descriptor, int command, ...);

static int socket_flags(int socket, int command, int value) {
    switch (command) {
    case F_GETFD:
    case F_SETFD:
        return 0;   /* the app never execs another program */
    case F_SETFL: {
        const int on = (value & O_NONBLOCK) != 0;
        return setsockopt(socket, SOL_SOCKET, CONSOLE_SO_NBIO, &on, sizeof on);
    }
    case F_GETFL: {
        int on = 0;
        socklen_t length = sizeof on;
        if (getsockopt(socket, SOL_SOCKET, CONSOLE_SO_NBIO, &on, &length) < 0) return -1;
        return O_RDWR | (on ? O_NONBLOCK : 0);
    }
    default:
        errno = EINVAL;
        return -1;
    }
}

int __wrap_fcntl(int descriptor, int command, ...) {
    va_list arguments;
    va_start(arguments, command);
    /* Every command libcurl uses takes an int or a pointer. */
    const intptr_t argument = va_arg(arguments, intptr_t);
    va_end(arguments);
    const int result = __real_fcntl(descriptor, command, argument);
    if (result >= 0 || errno != EINVAL) return result;
    if (command == F_GETFD || command == F_SETFD || command == F_SETFL || command == F_GETFL)
        return socket_flags(descriptor, command, (int)argument);
    return result;
}
