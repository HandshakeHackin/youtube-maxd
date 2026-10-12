/* The built-in YouTube service: an HTTP service on 127.0.0.1 inside the app
 * that speaks the companion's API. This copy of the interface stands in until
 * standalone/include/tube_service.h arrives with the implementation; the build
 * prefers that file when it exists, and the two must stay identical.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_SERVICE_H
#define TUBE_SERVICE_H
#ifdef __cplusplus
extern "C" {
#endif

/* 0 = running: `port` is its loopback port and `token` the bearer token every
 * request must carry. Loopback only; its own thread(s); never blocks on the
 * network; calling it while running returns the same port and token. */
int tube_service_start(const char *state_dir, unsigned short *port, char token[65]);
/* Cancels and joins; safe when not running. */
void tube_service_stop(void);
int tube_service_running(void);
/* Short static text for the last failure, "" when none. */
const char *tube_service_error(void);

#ifdef __cplusplus
}
#endif
#endif
