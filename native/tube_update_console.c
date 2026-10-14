/* ProsperoTube curl setup for the shared update kit. Uses the same verified
 * trust roots and socket setup as the service, on independent curl handles.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include <curl/curl.h>
#include "tube_js_console.h"
extern const unsigned char tube_js_ca_pem[];
extern const size_t tube_js_ca_pem_length;
static int setup_socket(void *user, curl_socket_t socket, curlsocktype purpose) {
    (void)user; (void)purpose;
    return tube_js_console_socket_setup(socket) ? CURL_SOCKOPT_ERROR : CURL_SOCKOPT_OK;
}
void console_curl_setup(CURL *easy) {
    struct curl_blob ca = {(void *)tube_js_ca_pem, tube_js_ca_pem_length, CURL_BLOB_COPY};
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_CAINFO_BLOB, &ca);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(easy, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(easy, CURLOPT_SOCKOPTFUNCTION, setup_socket);
}
