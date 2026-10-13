/* The update engine's transport over tube_js_http; see tube_update.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 *
 * One client of its own, used by one thread at a time (the update worker): it
 * shares nothing with the JavaScript service's client and works without it. */
#include "tube_update.h"
#include "tube_js_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct tube_update_http {
    tube_js_http_config config;
    tube_js_http *client;
};

tube_update_http *tube_update_http_new(const void *http_config) {
    tube_update_http *http = (tube_update_http *)calloc(1, sizeof *http);
    if (!http) return NULL;
    if (http_config) http->config = *(const tube_js_http_config *)http_config;
    http->config.user_agent = TUBE_UPDATE_USER_AGENT;
    http->config.cookies = 0;
    if (!http->config.timeout_ms) http->config.timeout_ms = 120000;
    return http;
}

void tube_update_http_free(tube_update_http *http) {
    if (!http) return;
    if (http->client) tube_js_http_free(http->client);
    free(http);
}

static int get(void *context, const char *url, int ranged, uint64_t first, uint64_t last, size_t limit,
               const volatile int *cancel, tube_update_reply *reply) {
    tube_update_http *http = (tube_update_http *)context;
    char range[64];
    tube_js_http_header headers[1];
    tube_js_http_request request = {.method = "GET", .url = url, .headers = headers, .header_count = 1,
                                    .redirect = TUBE_JS_HTTP_REDIRECT_MANUAL, .max_response_bytes = limit ? limit : 1};
    tube_js_http_response *response = NULL;
    uint32_t id = 0;
    memset(reply, 0, sizeof *reply);
    if (!http->client) http->client = tube_js_http_new(&http->config);   /* the configuration's strings and roots are copied */
    if (!http->client) { reply->error = TUBE_JS_HTTP_MEMORY; snprintf(reply->message, sizeof reply->message, "out of memory"); return 0; }
    if (ranged) {
        snprintf(range, sizeof range, "bytes=%llu-%llu", (unsigned long long)first, (unsigned long long)last);
        headers[0] = (tube_js_http_header){"Range", range};
    } else headers[0] = (tube_js_http_header){"Accept", "application/vnd.github+json"};
    int error = tube_js_http_start(http->client, &request, &id);
    if (error != TUBE_JS_HTTP_OK) {
        reply->error = error; snprintf(reply->message, sizeof reply->message, "%s", tube_js_http_error_name(error));
        return 0;
    }
    for (int cancelled = 0; !response;) {
        if (cancel && *cancel && !cancelled) { (void)tube_js_http_cancel(http->client, id); cancelled = 1; }
        (void)tube_js_http_poll(http->client, 50);
        response = tube_js_http_take(http->client);
        if (response && response->id != id) { tube_js_http_response_free(response); response = NULL; }   /* never: one request at a time */
    }
    if (response->error != TUBE_JS_HTTP_OK) {
        reply->error = response->error;
        snprintf(reply->message, sizeof reply->message, "%s", *response->error_text ? response->error_text : tube_js_http_error_name(response->error));
    } else {
        reply->status = response->status;
        for (size_t i = 0; i < response->header_count; ++i) {
            if (!strcasecmp(response->headers[i].name, "location")) snprintf(reply->location, sizeof reply->location, "%s", response->headers[i].value);
            else if (!strcasecmp(response->headers[i].name, "content-range")) snprintf(reply->content_range, sizeof reply->content_range, "%s", response->headers[i].value);
        }
        reply->body = (unsigned char *)malloc(response->body_length ? response->body_length : 1);
        if (!reply->body) { reply->error = TUBE_JS_HTTP_MEMORY; snprintf(reply->message, sizeof reply->message, "out of memory"); }
        else { memcpy(reply->body, response->body, response->body_length); reply->length = response->body_length; }
    }
    tube_js_http_response_free(response);
    return 0;
}

tube_update_transport tube_update_http_transport(tube_update_http *http) {
    tube_update_transport transport = {http, get};
    return transport;
}
