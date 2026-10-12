/* Stand-in for the built-in service, for link checks of the package only
 * (PROSPEROTUBE_STUB_SERVICE=1): it never starts, so an app built with it
 * opens, says so on its first row, and plays nothing.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_service.h"

int tube_service_start(const char *state_dir, unsigned short *port, char token[65]) {
    (void)state_dir;
    if (port) *port = 0;
    if (token) token[0] = 0;
    return -1;
}

void tube_service_stop(void) {}

int tube_service_running(void) { return 0; }

const char *tube_service_error(void) { return "this build carries a stand-in, not the YouTube service"; }
