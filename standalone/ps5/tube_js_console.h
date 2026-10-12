/* What libcurl needs to run inside the PS5 app; console builds only.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_JS_CONSOLE_H
#define TUBE_JS_CONSOLE_H
#ifdef __cplusplus
extern "C" {
#endif

/* The console's client configuration, in full:
 *   {.ca_data = tube_js_ca_pem, .ca_length = tube_js_ca_pem_length,
 *    .socket_setup = tube_js_console_socket_setup, .poll_slice_ms = 10}
 *
 * tube_js_http_config.socket_setup for the console: switches the socket to
 * non-blocking with the console's own option (SO_NBIO). Without it a transfer
 * returns only when the server closes the idle connection. */
int tube_js_console_socket_setup(int socket);

/* The PEM file certificate verification should use: `packaged` (the Mozilla
 * bundle shipped in the app's assets) when it can be read, otherwise the
 * console's own list. Never NULL; the result is valid until the next call. */
const char *tube_js_console_ca_file(const char *packaged);

#ifdef __cplusplus
}
#endif
#endif
