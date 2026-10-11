/* Asynchronous HTTP(S) requests for a JavaScript `fetch` shim, on libcurl's
 * multi interface. One client belongs to one thread (the engine thread): every
 * call except tube_js_http_wake() must come from it. Nothing here blocks other
 * than tube_js_http_poll() for the time it is given.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_JS_HTTP_H
#define TUBE_JS_HTTP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct tube_js_http tube_js_http;

enum {
    TUBE_JS_HTTP_OK = 0,
    TUBE_JS_HTTP_INVALID,     /* bad argument, URL, scheme, header or method */
    TUBE_JS_HTTP_MEMORY,
    TUBE_JS_HTTP_BUSY,        /* too many requests in flight */
    TUBE_JS_HTTP_ABORTED,     /* tube_js_http_cancel() */
    TUBE_JS_HTTP_TIMEOUT,
    TUBE_JS_HTTP_TOO_LARGE,   /* the decoded body or the headers passed their bound */
    TUBE_JS_HTTP_REDIRECT,    /* too many redirects, or one met in "error" mode */
    TUBE_JS_HTTP_RESOLVE,     /* the name did not resolve */
    TUBE_JS_HTTP_CONNECT,
    TUBE_JS_HTTP_TLS,         /* handshake or certificate verification failed */
    TUBE_JS_HTTP_NETWORK      /* anything else that went wrong on the wire */
};

/* fetch's `redirect` option. */
enum { TUBE_JS_HTTP_REDIRECT_FOLLOW = 0, TUBE_JS_HTTP_REDIRECT_MANUAL, TUBE_JS_HTTP_REDIRECT_ERROR };

typedef struct tube_js_http_header {
    const char *name, *value;
} tube_js_http_header;

typedef struct tube_js_http_config {
    /* PEM file of trusted roots. NULL = libcurl's built-in default, which is
     * right on a desktop and wrong on the console: there, always set it. A
     * file that cannot be read fails every HTTPS request; verification is
     * never switched off. */
    const char *ca_file;
    /* The same roots as PEM text in memory (copied). When given they are the
     * only roots: neither ca_file nor libcurl's default is consulted. */
    const void *ca_data;
    size_t ca_length;
    const char *user_agent;      /* sent when a request names none; NULL = none */
    long connect_timeout_ms;      /* 0 = 15000 */
    long timeout_ms;              /* whole request, redirects included; 0 = 60000 */
    size_t max_response_bytes;    /* decoded body; 0 = 16 MiB */
    int max_redirects;            /* 0 = 20, as fetch */
    int max_connections;          /* open connections in total; 0 = 6 */
    int max_requests;             /* in flight or waiting to be taken; 0 = 64 */
    int cookies;                  /* 1 = an in-memory cookie jar shared by the client's requests */
    /* Called on each new socket before it connects (console: SO_NBIO, see
     * tube_js_console.c). A non-zero return is ignored. NULL = nothing. */
    int (*socket_setup)(int socket);
} tube_js_http_config;

typedef struct tube_js_http_request {
    const char *method;                    /* NULL = "GET" */
    const char *url;                       /* http:// or https:// */
    const tube_js_http_header *headers;
    size_t header_count;
    const void *body;                      /* may hold any bytes; NULL = none */
    size_t body_length;
    int redirect;                          /* TUBE_JS_HTTP_REDIRECT_* */
    long timeout_ms;                       /* 0 = the client's */
    size_t max_response_bytes;             /* 0 = the client's */
} tube_js_http_request;

typedef struct tube_js_http_response {
    uint32_t id;
    int error;                             /* TUBE_JS_HTTP_OK: status, headers and body are valid */
    const char *error_text;                /* never NULL; "" when error is OK */
    long status;
    const char *url;                       /* where the answer came from, after redirects */
    int redirected;
    const tube_js_http_header *headers;    /* of the final answer, in arrival order */
    size_t header_count;
    const unsigned char *body;             /* decoded (gzip, deflate, and what else libcurl was built with) */
    size_t body_length;
} tube_js_http_response;

tube_js_http *tube_js_http_new(const tube_js_http_config *config);   /* NULL config = all defaults */
/* Drops every request not yet taken. Responses already taken stay valid and
 * are freed on their own. */
void tube_js_http_free(tube_js_http *client);

/* Queues a request; everything it points at is copied. Returns TUBE_JS_HTTP_OK
 * and the request's id (never 0), or the reason nothing was queued. */
int tube_js_http_start(tube_js_http *client, const tube_js_http_request *request, uint32_t *id);

/* 1 = the request was in flight and now completes as TUBE_JS_HTTP_ABORTED;
 * 0 = unknown id or already complete. */
int tube_js_http_cancel(tube_js_http *client, uint32_t id);

/* Moves the transfers along, waiting up to `wait_ms` for network activity or a
 * wake (0 = do not wait). Returns how many completed requests can be taken. */
int tube_js_http_poll(tube_js_http *client, int wait_ms);

/* The next completed request in completion order, or NULL. The caller owns it
 * until tube_js_http_response_free(). */
tube_js_http_response *tube_js_http_take(tube_js_http *client);
void tube_js_http_response_free(tube_js_http_response *response);

int tube_js_http_active(const tube_js_http *client);   /* requests not yet complete */

/* The one call another thread may make: ends a tube_js_http_poll() wait early.
 * Returns 0 when the wake was delivered. */
int tube_js_http_wake(tube_js_http *client);

const char *tube_js_http_error_name(int error);
/* "libcurl/8.18.0 OpenSSL/3.5.2 zlib/..." of the library actually linked. */
const char *tube_js_http_backend(void);

#ifdef __cplusplus
}
#endif
#endif
