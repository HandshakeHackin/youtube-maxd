/* Asynchronous HTTP(S) requests for a JavaScript `fetch` shim. See tube_js_http.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_js_http.h"

#include "tube_js_platform.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum { HEADER_BYTES_MAX = 256 * 1024, HEADER_COUNT_MAX = 512, METHOD_MAX = 16, FINISH_BATCH = 16 };

typedef struct transfer {
    tube_js_http_response response;   /* first: a response pointer is its transfer */
    struct transfer *next;
    tube_js_http *client;
    CURL *easy;
    struct curl_slist *list;
    char method[METHOD_MAX + 1];
    char *url;
    tube_js_http_header *request_headers;
    size_t request_header_count;
    unsigned char *request_body;
    size_t request_body_length;
    int has_body, redirect_mode, redirects;
    int stream;                       /* body handed over in pieces; deadline_ms is then the end of the current silence */
    long idle_ms, stream_status;
    uint64_t deadline_ms;
    size_t max_bytes;
    unsigned char *body;
    size_t body_length, body_capacity;
    tube_js_http_header *headers;
    size_t header_count, header_capacity, header_bytes;
    int too_large, out_of_memory;
    char curl_error[CURL_ERROR_SIZE];
    char error_text[CURL_ERROR_SIZE + 64];
} transfer;

struct tube_js_http {
    CURLM *multi;
    CURLSH *share;
    char *ca_file, *user_agent;
    unsigned char *ca_data;
    size_t ca_length;
    long connect_timeout_ms, timeout_ms;
    size_t max_response_bytes;
    int max_redirects, max_requests;
    int (*socket_setup)(int socket);
    int ipv4_only;
    int poll_slice_ms;
    atomic_int woken;
    int chunks;                       /* a streaming request may have a piece waiting */
    transfer *active, *done_head, *done_tail;
    int active_count, done_count;
    uint32_t last_id;
};

static pthread_once_t global_once = PTHREAD_ONCE_INIT;
static int global_failed;
static void global_init(void) { global_failed = curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK; }

/* ---- Small helpers ---- */

static char *copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

static int is_token(const char *text) {
    if (!text || !*text) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (*p <= 0x20 || *p >= 0x7f || strchr("()<>@,;:\\\"/[]?={}", *p)) return 0;
    return 1;
}

static int is_header_value(const char *text) {
    if (!text) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (*p == '\r' || *p == '\n') return 0;
    return 1;
}

static int is_http_url(const char *url) {
    return url && (!strncasecmp(url, "http://", 7) || !strncasecmp(url, "https://", 8));
}

/* Headers the transport owns; a caller's value would desynchronize the
 * connection or the decoding. fetch forbids the same names. */
static int is_transport_header(const char *name) {
    static const char *const names[] = {"accept-encoding", "content-length", "host", "connection", "expect",
                                        "transfer-encoding", "te", "upgrade", "keep-alive", "proxy-connection"};
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        if (!strcasecmp(name, names[i])) return 1;
    return 0;
}

static int is_body_header(const char *name) {
    static const char *const names[] = {"content-type", "content-encoding", "content-language", "content-location"};
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        if (!strcasecmp(name, names[i])) return 1;
    return 0;
}

static int normalize_method(const char *method, char output[METHOD_MAX + 1]) {
    static const char *const known[] = {"GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "PATCH"};
    if (!method || !*method) method = "GET";
    if (!is_token(method) || strlen(method) > METHOD_MAX) return -1;
    if (!strcasecmp(method, "CONNECT") || !strcasecmp(method, "TRACE") || !strcasecmp(method, "TRACK")) return -1;
    snprintf(output, METHOD_MAX + 1, "%s", method);
    for (size_t i = 0; i < sizeof known / sizeof *known; i++)
        if (!strcasecmp(method, known[i])) snprintf(output, METHOD_MAX + 1, "%s", known[i]);
    return 0;
}

static void free_headers(tube_js_http_header *headers, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free((char *)headers[i].name);
        free((char *)headers[i].value);
    }
    free(headers);
}

static void remove_request_headers(transfer *t, int (*matches)(const char *name)) {
    size_t kept = 0;
    for (size_t i = 0; i < t->request_header_count; i++) {
        if (matches(t->request_headers[i].name)) {
            free((char *)t->request_headers[i].name);
            free((char *)t->request_headers[i].value);
        } else {
            t->request_headers[kept++] = t->request_headers[i];
        }
    }
    t->request_header_count = kept;
}

static int is_authorization(const char *name) { return !strcasecmp(name, "authorization"); }

static int has_request_header(const transfer *t, const char *name) {
    for (size_t i = 0; i < t->request_header_count; i++)
        if (!strcasecmp(t->request_headers[i].name, name)) return 1;
    return 0;
}

/* Scheme, host and port equal. Anything that does not parse is another origin. */
static int same_origin(const char *first, const char *second) {
    static const CURLUPart parts[3] = {CURLUPART_SCHEME, CURLUPART_HOST, CURLUPART_PORT};
    CURLU *a = curl_url(), *b = curl_url();
    int same = a && b && !curl_url_set(a, CURLUPART_URL, first, 0) && !curl_url_set(b, CURLUPART_URL, second, 0);
    for (int i = 0; same && i < 3; i++) {
        char *x = NULL, *y = NULL;
        unsigned flags = parts[i] == CURLUPART_PORT ? CURLU_DEFAULT_PORT : 0;
        same = !curl_url_get(a, parts[i], &x, flags) && !curl_url_get(b, parts[i], &y, flags) && !strcasecmp(x, y);
        curl_free(x);
        curl_free(y);
    }
    curl_url_cleanup(a);
    curl_url_cleanup(b);
    return same;
}

static void transfer_free(transfer *t) {
    if (!t) return;
    if (t->easy) curl_easy_cleanup(t->easy);
    curl_slist_free_all(t->list);
    free(t->url);
    free_headers(t->request_headers, t->request_header_count);
    free(t->request_body);
    free(t->body);
    free_headers(t->headers, t->header_count);
    free(t);
}

static void reset_answer(transfer *t) {
    free_headers(t->headers, t->header_count);
    t->headers = NULL;
    t->header_count = t->header_capacity = t->header_bytes = 0;
    t->body_length = 0;
    if (t->body) t->body[0] = 0;
}

/* ---- libcurl callbacks ---- */

static int is_redirect(long status) { return status == 301 || status == 302 || status == 303 || status == 307 || status == 308; }

static size_t on_body(char *data, size_t size, size_t count, void *user) {
    transfer *t = user;
    size_t length = size * count;
    if (t->stream) {
        long status = 0;
        (void)curl_easy_getinfo(t->easy, CURLINFO_RESPONSE_CODE, &status);
        t->deadline_ms = tube_js_monotonic_ms() + (uint64_t)t->idle_ms;
        /* The body of a redirect that will be followed is not part of the stream. */
        if (t->redirect_mode == TUBE_JS_HTTP_REDIRECT_FOLLOW && is_redirect(status)) return length;
        t->stream_status = status;
        t->client->chunks = 1;
    }
    if (length > t->max_bytes - t->body_length) {
        t->too_large = 1;
        return CURL_WRITEFUNC_ERROR;
    }
    if (t->body_length + length + 1 > t->body_capacity) {
        size_t capacity = t->body_capacity ? t->body_capacity : 4096;
        while (capacity < t->body_length + length + 1) capacity *= 2;
        if (capacity > t->max_bytes + 1) capacity = t->max_bytes + 1;
        unsigned char *grown = realloc(t->body, capacity);
        if (!grown) {
            t->out_of_memory = 1;
            return CURL_WRITEFUNC_ERROR;
        }
        t->body = grown;
        t->body_capacity = capacity;
    }
    memcpy(t->body + t->body_length, data, length);
    t->body_length += length;
    t->body[t->body_length] = 0;   /* text answers can be read as a C string */
    return length;
}

static char *copy_trimmed(const char *start, const char *end) {
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
    char *copy = malloc((size_t)(end - start) + 1);
    if (!copy) return NULL;
    memcpy(copy, start, (size_t)(end - start));
    copy[end - start] = 0;
    return copy;
}

static size_t on_header(char *data, size_t size, size_t count, void *user) {
    transfer *t = user;
    size_t length = size * count;
    const char *end = data + length;
    if (t->stream) t->deadline_ms = tube_js_monotonic_ms() + (uint64_t)t->idle_ms;
    if (length >= 5 && !memcmp(data, "HTTP/", 5)) {   /* a new answer (after 1xx): start over */
        reset_answer(t);
        return length;
    }
    const char *colon = memchr(data, ':', length);
    if (!colon || colon == data) return length;
    t->header_bytes += length;
    if (t->header_bytes > HEADER_BYTES_MAX || t->header_count >= HEADER_COUNT_MAX) {
        t->too_large = 1;
        return CURL_WRITEFUNC_ERROR;
    }
    if (t->header_count == t->header_capacity) {
        size_t capacity = t->header_capacity ? t->header_capacity * 2 : 32;
        tube_js_http_header *grown = realloc(t->headers, capacity * sizeof *grown);
        if (!grown) {
            t->out_of_memory = 1;
            return CURL_WRITEFUNC_ERROR;
        }
        t->headers = grown;
        t->header_capacity = capacity;
    }
    char *name = copy_trimmed(data, colon), *value = copy_trimmed(colon + 1, end);
    if (!name || !value) {
        free(name);
        free(value);
        t->out_of_memory = 1;
        return CURL_WRITEFUNC_ERROR;
    }
    t->headers[t->header_count].name = name;
    t->headers[t->header_count].value = value;
    t->header_count++;
    return length;
}

static int on_socket(void *user, curl_socket_t socket, curlsocktype purpose) {
    tube_js_http *client = user;
    (void)purpose;
    if (client->socket_setup) (void)client->socket_setup((int)socket);
    return CURL_SOCKOPT_OK;
}

/* ---- One hop of a request ---- */

static int add_line(transfer *t, const char *line) {
    struct curl_slist *grown = curl_slist_append(t->list, line);
    if (!grown) return -1;
    t->list = grown;
    return 0;
}

/* Sets the easy handle up for the transfer's current method, URL, headers and
 * body. Returns a TUBE_JS_HTTP_* code. */
static int configure(transfer *t) {
    tube_js_http *client = t->client;
    CURL *easy = t->easy;
    uint64_t now = tube_js_monotonic_ms();
    if (now >= t->deadline_ms) return TUBE_JS_HTTP_TIMEOUT;
    long remaining = t->deadline_ms - now > 0x7fffffff ? 0x7fffffff : (long)(t->deadline_ms - now);

    curl_easy_reset(easy);
    curl_slist_free_all(t->list);
    t->list = NULL;
    t->curl_error[0] = 0;
    int failed = 0;
#define SET(option, value) failed |= curl_easy_setopt(easy, option, value) != CURLE_OK
    SET(CURLOPT_URL, t->url);
    SET(CURLOPT_PROTOCOLS_STR, "http,https");
    SET(CURLOPT_PRIVATE, t);
    SET(CURLOPT_ERRORBUFFER, t->curl_error);
    SET(CURLOPT_NOSIGNAL, 1L);
    if (client->ipv4_only) SET(CURLOPT_IPRESOLVE, (long)CURL_IPRESOLVE_V4);
    SET(CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);   /* what the console's libcurl speaks; the same everywhere */
    SET(CURLOPT_BUFFERSIZE, 256L * 1024L);
    SET(CURLOPT_QUICK_EXIT, 1L);            /* a cancel or shutdown never waits for a name lookup */
    SET(CURLOPT_FOLLOWLOCATION, 0L);       /* redirects are followed here, by fetch's rules */
    SET(CURLOPT_ACCEPT_ENCODING, "");       /* offer and decode every coding this libcurl has */
    SET(CURLOPT_WRITEFUNCTION, on_body);
    SET(CURLOPT_WRITEDATA, t);
    SET(CURLOPT_HEADERFUNCTION, on_header);
    SET(CURLOPT_HEADERDATA, t);
    SET(CURLOPT_CONNECTTIMEOUT_MS, client->connect_timeout_ms < remaining ? client->connect_timeout_ms : remaining);
    SET(CURLOPT_TIMEOUT_MS, t->stream ? 0L : remaining);   /* a stream has no whole-request limit: drive() ends a silent one */
    SET(CURLOPT_SSL_VERIFYPEER, 1L);
    SET(CURLOPT_SSL_VERIFYHOST, 2L);
    if (client->ca_data) {
        struct curl_blob roots = {client->ca_data, client->ca_length, CURL_BLOB_NOCOPY};
        SET(CURLOPT_CAINFO_BLOB, &roots);
        SET(CURLOPT_CAINFO, NULL);
        SET(CURLOPT_CAPATH, NULL);
    } else if (client->ca_file) {
        SET(CURLOPT_CAINFO, client->ca_file);
        SET(CURLOPT_CAPATH, NULL);
    }
    if (client->socket_setup) {
        SET(CURLOPT_SOCKOPTFUNCTION, on_socket);
        SET(CURLOPT_SOCKOPTDATA, client);
    }
    if (client->share) {
        SET(CURLOPT_SHARE, client->share);
        SET(CURLOPT_COOKIEFILE, "");
    }
    if (client->user_agent && !has_request_header(t, "user-agent")) SET(CURLOPT_USERAGENT, client->user_agent);

    int sends_body = t->has_body || !strcmp(t->method, "POST") || !strcmp(t->method, "PUT") ||
                     !strcmp(t->method, "PATCH");
    if (!strcmp(t->method, "HEAD")) {
        SET(CURLOPT_NOBODY, 1L);
    } else if (sends_body) {
        SET(CURLOPT_POST, 1L);
        SET(CURLOPT_POSTFIELDS, t->request_body ? (const char *)t->request_body : "");
        SET(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)t->request_body_length);
        if (strcmp(t->method, "POST")) SET(CURLOPT_CUSTOMREQUEST, t->method);
    } else if (strcmp(t->method, "GET")) {
        SET(CURLOPT_CUSTOMREQUEST, t->method);
    }

    for (size_t i = 0; i < t->request_header_count && !failed; i++) {
        const tube_js_http_header *header = &t->request_headers[i];
        size_t length = strlen(header->name) + strlen(header->value) + 3;
        char *line = malloc(length);
        if (!line) return TUBE_JS_HTTP_MEMORY;
        /* "Name;" is libcurl's spelling of a header with an empty value. */
        if (*header->value) snprintf(line, length, "%s: %s", header->name, header->value);
        else snprintf(line, length, "%s;", header->name);
        failed |= add_line(t, line);
        free(line);
    }
    failed |= add_line(t, "Expect:");   /* never wait for 100 Continue */
    /* libcurl would call a body form data; fetch sends no type unless told. */
    if (sends_body && !has_request_header(t, "content-type")) failed |= add_line(t, "Content-Type:");
    SET(CURLOPT_HTTPHEADER, t->list);
#undef SET
    return failed ? TUBE_JS_HTTP_MEMORY : TUBE_JS_HTTP_OK;
}

static void unlink_active(transfer *t) {
    for (transfer **link = &t->client->active; *link; link = &(*link)->next) {
        if (*link == t) {
            *link = t->next;
            t->client->active_count--;
            break;
        }
    }
    t->next = NULL;
}

/* Ends a request: it leaves the multi handle and waits to be taken. */
static void complete(transfer *t, int error, const char *text) {
    tube_js_http *client = t->client;
    unlink_active(t);
    if (t->easy) {
        if (error == TUBE_JS_HTTP_OK) (void)curl_easy_getinfo(t->easy, CURLINFO_RESPONSE_CODE, &t->response.status);
        (void)curl_multi_remove_handle(client->multi, t->easy);
        curl_easy_cleanup(t->easy);
        t->easy = NULL;
    }
    curl_slist_free_all(t->list);
    t->list = NULL;
    free(t->request_body);
    t->request_body = NULL;
    if (error != TUBE_JS_HTTP_OK) {
        reset_answer(t);
        t->response.status = 0;
    }
    snprintf(t->error_text, sizeof t->error_text, "%s", error == TUBE_JS_HTTP_OK ? "" : text && *text ? text : tube_js_http_error_name(error));
    t->response.error = error;
    t->response.error_text = t->error_text;
    t->response.url = t->url;
    t->response.headers = t->headers;
    t->response.header_count = t->header_count;
    t->response.body = t->body_length ? t->body : (const unsigned char *)"";
    t->response.body_length = t->body_length;
    if (client->done_tail) client->done_tail->next = t;
    else client->done_head = t;
    client->done_tail = t;
    client->done_count++;
}

static int map_curl_error(CURLcode code) {
    switch (code) {
    case CURLE_OPERATION_TIMEDOUT: return TUBE_JS_HTTP_TIMEOUT;
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_RESOLVE_PROXY: return TUBE_JS_HTTP_RESOLVE;
    case CURLE_COULDNT_CONNECT: return TUBE_JS_HTTP_CONNECT;
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_SSL_CACERT_BADFILE:
    case CURLE_SSL_ENGINE_NOTFOUND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_SSL_ENGINE_INITFAILED:
    case CURLE_SSL_CRL_BADFILE:
    case CURLE_SSL_ISSUER_ERROR:
    case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
    case CURLE_SSL_INVALIDCERTSTATUS:
    case CURLE_USE_SSL_FAILED: return TUBE_JS_HTTP_TLS;
    case CURLE_URL_MALFORMAT:
    case CURLE_UNSUPPORTED_PROTOCOL: return TUBE_JS_HTTP_INVALID;
    case CURLE_OUT_OF_MEMORY: return TUBE_JS_HTTP_MEMORY;
    default: return TUBE_JS_HTTP_NETWORK;
    }
}

/* Follows a redirect answer as fetch does. 1 = the request goes on (or was
 * completed with an error here); 0 = there is nowhere to go, deliver the answer. */
static int follow(transfer *t, long status) {
    tube_js_http *client = t->client;
    char *location = NULL;
    if (curl_easy_getinfo(t->easy, CURLINFO_REDIRECT_URL, &location) != CURLE_OK || !location || !*location) return 0;
    if (t->redirects >= client->max_redirects) {
        complete(t, TUBE_JS_HTTP_REDIRECT, "too many redirects");
        return 1;
    }
    if (!is_http_url(location)) {
        complete(t, TUBE_JS_HTTP_REDIRECT, "redirect to an address that is not http or https");
        return 1;
    }
    char *next = copy_string(location);   /* the handle owns `location` */
    if (!next) {
        complete(t, TUBE_JS_HTTP_MEMORY, NULL);
        return 1;
    }
    int becomes_get = ((status == 301 || status == 302) && !strcmp(t->method, "POST")) ||
                      (status == 303 && strcmp(t->method, "GET") && strcmp(t->method, "HEAD"));
    if (becomes_get) {
        snprintf(t->method, sizeof t->method, "GET");
        free(t->request_body);
        t->request_body = NULL;
        t->request_body_length = 0;
        t->has_body = 0;
        remove_request_headers(t, is_body_header);
    }
    if (!same_origin(t->url, next)) remove_request_headers(t, is_authorization);
    free(t->url);
    t->url = next;
    t->redirects++;
    t->response.redirected = 1;
    reset_answer(t);
    (void)curl_multi_remove_handle(client->multi, t->easy);
    int error = configure(t);
    if (error == TUBE_JS_HTTP_OK && curl_multi_add_handle(client->multi, t->easy) != CURLM_OK) error = TUBE_JS_HTTP_MEMORY;
    if (error != TUBE_JS_HTTP_OK) complete(t, error, NULL);
    return 1;
}

static void finish(transfer *t, CURLcode result) {
    if (t->too_large) {
        complete(t, TUBE_JS_HTTP_TOO_LARGE, "the answer is larger than this request allows");
        return;
    }
    if (t->out_of_memory) {
        complete(t, TUBE_JS_HTTP_MEMORY, NULL);
        return;
    }
    if (result != CURLE_OK) {
        complete(t, map_curl_error(result), *t->curl_error ? t->curl_error : curl_easy_strerror(result));
        return;
    }
    long status = 0;
    (void)curl_easy_getinfo(t->easy, CURLINFO_RESPONSE_CODE, &status);
    if (is_redirect(status)) {
        if (t->redirect_mode == TUBE_JS_HTTP_REDIRECT_ERROR) {
            complete(t, TUBE_JS_HTTP_REDIRECT, "the request does not allow redirects");
            return;
        }
        if (t->redirect_mode == TUBE_JS_HTTP_REDIRECT_FOLLOW && follow(t, status)) return;
    }
    complete(t, TUBE_JS_HTTP_OK, NULL);
}

static void drive(tube_js_http *client) {
    int again;
    do {
        again = 0;
        int running = 0;
        if (curl_multi_perform(client->multi, &running) != CURLM_OK) {
            while (client->active) complete(client->active, TUBE_JS_HTTP_NETWORK, "the transfer engine failed");
            return;
        }
        struct {
            transfer *t;
            CURLcode result;
        } finished[FINISH_BATCH];
        int count = 0, left = 0;
        CURLMsg *message;
        while (count < FINISH_BATCH && (message = curl_multi_info_read(client->multi, &left))) {
            char *owner = NULL;
            if (message->msg != CURLMSG_DONE) continue;
            if (curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &owner) != CURLE_OK || !owner) continue;
            finished[count].t = (transfer *)owner;
            finished[count].result = message->data.result;
            count++;
        }
        for (int i = 0; i < count; i++) {
            int before = client->done_count;
            finish(finished[i].t, finished[i].result);
            if (client->done_count == before) again = 1;   /* redirected: start the next hop now */
        }
        if (count == FINISH_BATCH) again = 1;
    } while (again);
    /* A streaming request that has been silent for longer than it allows. */
    uint64_t now = tube_js_monotonic_ms();
    for (transfer *t = client->active, *next; t; t = next) {
        next = t->next;
        if (t->stream && now >= t->deadline_ms) complete(t, TUBE_JS_HTTP_TIMEOUT, "the stream was silent for too long");
    }
}

/* ---- Public interface ---- */

tube_js_http *tube_js_http_new(const tube_js_http_config *config) {
    static const tube_js_http_config defaults = {0};
    if (!config) config = &defaults;
    pthread_once(&global_once, global_init);
    if (global_failed) return NULL;
    tube_js_http *client = calloc(1, sizeof *client);
    if (!client) return NULL;
    client->connect_timeout_ms = config->connect_timeout_ms > 0 ? config->connect_timeout_ms : 15000;
    client->timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 60000;
    client->max_response_bytes = config->max_response_bytes ? config->max_response_bytes : (size_t)16 << 20;
    client->max_redirects = config->max_redirects > 0 ? config->max_redirects : 20;
    client->max_requests = config->max_requests > 0 ? config->max_requests : 64;
    client->socket_setup = config->socket_setup;
    client->ipv4_only = config->ipv4_only;
    client->poll_slice_ms = config->poll_slice_ms > 0 ? config->poll_slice_ms : 0;
    atomic_init(&client->woken, 0);
    int failed = 0;
    if (config->ca_file) failed |= !(client->ca_file = copy_string(config->ca_file));
    if (config->ca_data && config->ca_length) {
        failed |= !(client->ca_data = malloc(config->ca_length));
        if (!failed) memcpy(client->ca_data, config->ca_data, config->ca_length);
        client->ca_length = config->ca_length;
    }
    if (config->user_agent) failed |= !(client->user_agent = copy_string(config->user_agent));
    failed |= !(client->multi = curl_multi_init());
    if (!failed) {
        long connections = config->max_connections > 0 ? config->max_connections : 6;
        failed |= curl_multi_setopt(client->multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, connections) != CURLM_OK;
        failed |= curl_multi_setopt(client->multi, CURLMOPT_MAX_HOST_CONNECTIONS, connections) != CURLM_OK;
    }
    if (!failed && config->cookies) {
        failed |= !(client->share = curl_share_init());
        if (!failed) failed |= curl_share_setopt(client->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE) != CURLSHE_OK;
    }
    if (failed) {
        tube_js_http_free(client);
        return NULL;
    }
    return client;
}

void tube_js_http_free(tube_js_http *client) {
    if (!client) return;
    while (client->active) {
        transfer *t = client->active;
        client->active = t->next;
        if (t->easy) (void)curl_multi_remove_handle(client->multi, t->easy);
        transfer_free(t);
    }
    while (client->done_head) {
        transfer *t = client->done_head;
        client->done_head = t->next;
        transfer_free(t);
    }
    if (client->multi) curl_multi_cleanup(client->multi);
    if (client->share) curl_share_cleanup(client->share);
    free(client->ca_file);
    free(client->ca_data);
    free(client->user_agent);
    free(client);
}

int tube_js_http_start(tube_js_http *client, const tube_js_http_request *request, uint32_t *id) {
    if (id) *id = 0;
    if (!client || !request || !id || !is_http_url(request->url)) return TUBE_JS_HTTP_INVALID;
    if (request->header_count && !request->headers) return TUBE_JS_HTTP_INVALID;
    if (request->body_length && !request->body) return TUBE_JS_HTTP_INVALID;
    if (request->redirect < TUBE_JS_HTTP_REDIRECT_FOLLOW || request->redirect > TUBE_JS_HTTP_REDIRECT_ERROR)
        return TUBE_JS_HTTP_INVALID;
    if (request->header_count > HEADER_COUNT_MAX) return TUBE_JS_HTTP_INVALID;
    char method[METHOD_MAX + 1];
    if (normalize_method(request->method, method)) return TUBE_JS_HTTP_INVALID;
    int has_body = request->body != NULL;
    if (has_body && (!strcmp(method, "GET") || !strcmp(method, "HEAD"))) return TUBE_JS_HTTP_INVALID;
    for (size_t i = 0; i < request->header_count; i++)
        if (!is_token(request->headers[i].name) || !is_header_value(request->headers[i].value))
            return TUBE_JS_HTTP_INVALID;
    if (client->active_count + client->done_count >= client->max_requests) return TUBE_JS_HTTP_BUSY;

    transfer *t = calloc(1, sizeof *t);
    if (!t) return TUBE_JS_HTTP_MEMORY;
    t->client = client;
    memcpy(t->method, method, sizeof method);
    t->has_body = has_body;
    t->redirect_mode = request->redirect;
    t->max_bytes = request->max_response_bytes ? request->max_response_bytes : client->max_response_bytes;
    t->stream = request->stream != 0;
    t->idle_ms = request->timeout_ms > 0 ? request->timeout_ms : client->timeout_ms;
    t->deadline_ms = tube_js_monotonic_ms() + (uint64_t)t->idle_ms;
    int failed = !(t->url = copy_string(request->url));
    if (!failed && request->body_length) {
        failed |= !(t->request_body = malloc(request->body_length));
        if (!failed) memcpy(t->request_body, request->body, request->body_length);
        t->request_body_length = request->body_length;
    }
    if (!failed && request->header_count) failed |= !(t->request_headers = calloc(request->header_count, sizeof *t->request_headers));
    for (size_t i = 0; !failed && i < request->header_count; i++) {
        if (is_transport_header(request->headers[i].name)) continue;
        char *name = copy_string(request->headers[i].name), *value = copy_string(request->headers[i].value);
        if (!name || !value) {
            free(name);
            free(value);
            failed = 1;
            break;
        }
        t->request_headers[t->request_header_count].name = name;
        t->request_headers[t->request_header_count].value = value;
        t->request_header_count++;
    }
    failed |= !failed && !(t->easy = curl_easy_init());
    if (failed) {
        transfer_free(t);
        return TUBE_JS_HTTP_MEMORY;
    }
    int error = configure(t);
    if (error == TUBE_JS_HTTP_OK && curl_multi_add_handle(client->multi, t->easy) != CURLM_OK) error = TUBE_JS_HTTP_MEMORY;
    if (error != TUBE_JS_HTTP_OK) {
        transfer_free(t);
        return error;
    }
    if (++client->last_id == 0) client->last_id = 1;
    t->response.id = client->last_id;
    t->response.error_text = "";
    t->next = client->active;
    client->active = t;
    client->active_count++;
    *id = t->response.id;
    return TUBE_JS_HTTP_OK;
}

int tube_js_http_cancel(tube_js_http *client, uint32_t id) {
    if (!client || !id) return 0;
    for (transfer *t = client->active; t; t = t->next) {
        if (t->response.id == id) {
            complete(t, TUBE_JS_HTTP_ABORTED, "the request was cancelled");
            return 1;
        }
    }
    return 0;
}

int tube_js_http_poll(tube_js_http *client, int wait_ms) {
    if (!client) return 0;
    drive(client);
    if (wait_ms > 0 && !client->done_count && !client->chunks) {
        uint64_t deadline = tube_js_monotonic_ms() + (uint64_t)wait_ms;
        /* A wake that came before this wait ends it at once. */
        while (!atomic_exchange(&client->woken, 0)) {
            uint64_t now = tube_js_monotonic_ms();
            if (now >= deadline) break;
            int slice = (int)(deadline - now), ready = 0;
            if (client->poll_slice_ms && slice > client->poll_slice_ms) slice = client->poll_slice_ms;
            (void)curl_multi_poll(client->multi, NULL, 0, slice, &ready);
            drive(client);
            if (!client->poll_slice_ms) {   /* the one wait is over, whatever ended it */
                atomic_store(&client->woken, 0);
                break;
            }
            if (client->done_count || client->chunks) break;
        }
    }
    return client->done_count;
}

int tube_js_http_take_chunk(tube_js_http *client, tube_js_http_chunk *chunk) {
    if (!client || !chunk || !client->chunks) return 0;
    for (int done = 0; done < 2; done++) {
        for (transfer *t = done ? client->done_head : client->active; t; t = t->next) {
            if (!t->stream || !t->body_length) continue;
            chunk->id = t->response.id;
            chunk->status = t->stream_status;
            chunk->data = t->body;
            chunk->length = t->body_length;
            t->body_length = 0;              /* the bytes stay where they are until the next call */
            t->response.body = (const unsigned char *)"";
            t->response.body_length = 0;
            return 1;
        }
    }
    client->chunks = 0;
    return 0;
}

tube_js_http_response *tube_js_http_take(tube_js_http *client) {
    if (!client || !client->done_head) return NULL;
    transfer *t = client->done_head;
    client->done_head = t->next;
    if (!client->done_head) client->done_tail = NULL;
    client->done_count--;
    t->next = NULL;
    t->client = NULL;
    return &t->response;
}

void tube_js_http_response_free(tube_js_http_response *response) { transfer_free((transfer *)response); }

int tube_js_http_active(const tube_js_http *client) { return client ? client->active_count : 0; }

int tube_js_http_wake(tube_js_http *client) {
    if (!client) return -1;
    atomic_store(&client->woken, 1);
    int delivered = curl_multi_wakeup(client->multi) == CURLM_OK;
    /* With slices the flag alone is enough: the wait sees it within one slice. */
    return delivered || client->poll_slice_ms ? 0 : -1;
}

const char *tube_js_http_error_name(int error) {
    static const char *const names[] = {"ok", "invalid request", "out of memory", "too many requests",
                                        "aborted", "timed out", "answer too large", "redirect refused",
                                        "name not resolved", "connection failed", "TLS failure", "network error"};
    return error >= 0 && error < (int)(sizeof names / sizeof *names) ? names[error] : "unknown error";
}

const char *tube_js_http_backend(void) { return curl_version(); }
