/* libc names that link from libScePosixForWebKit, a module a native app never
 * loads: without a definition in the app they are null pointers at run time.
 * OpenSSL and FFmpeg refer to them, so unlike the stand-ins inside
 * tube_js_net.o these are visible to the whole app. Same definitions as the
 * hardware-proven ps5-native-app-boilerplate (examples/update-check/
 * console_curl.c: gethostbyname, isatty, mkstemp). Console builds only.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <stddef.h>

struct hostent;

/* Name lookups go through getaddrinfo (EVO's evo_dns.c). */
struct hostent *gethostbyname(const char *name) {
    (void)name;
    return NULL;
}

int isatty(int descriptor) {
    (void)descriptor;
    errno = ENOTTY;
    return 0;
}

int mkstemp(char *template_name) {
    (void)template_name;
    errno = ENOSYS;
    return -1;
}
