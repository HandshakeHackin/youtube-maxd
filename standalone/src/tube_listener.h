/* Minimal HTTP/1.1 listener for the loopback interface only, on its own
 * thread. Complete requests wait in a queue for the engine thread; answers go
 * back through tube_listener_respond().
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_LISTENER_H
#define TUBE_LISTENER_H

#include <stddef.h>
#include <stdint.h>

#define TUBE_LISTENER_CONNECTIONS 32
#define TUBE_LISTENER_HEAD_LIMIT 8192      /* request line + headers */
#define TUBE_LISTENER_BODY_LIMIT 8192      /* as the companion: 8 KiB request bodies */
/* POST /v1/subscriptions/import alone: the body is the user's export file, up
 * to 1 MiB as the companion allows; one such body is received at a time. */
#define TUBE_LISTENER_LARGE_BODY_LIMIT (1024 * 1024)

typedef struct TubeListener TubeListener;

typedef struct {
  uint32_t id;               /* names the connection in tube_listener_respond */
  char method[8];            /* GET, HEAD, POST or DELETE */
  char *target;              /* path and query as sent */
  char *headers;             /* "name: value\n" lines, names in lower case */
  uint8_t *body;
  size_t body_size;
} TubeRequest;

/* Binds 127.0.0.1 only. port 0 = a free port between 20000 and 39999 (the
 * console refuses to listen on 49152 and above). `notify` is called on the
 * listener's thread each time a request is queued. NULL on failure. */
TubeListener *tube_listener_start(unsigned short port, void (*notify)(void *), void *opaque);
/* Closes every connection and joins the thread. */
void tube_listener_stop(TubeListener *listener);
unsigned short tube_listener_port(const TubeListener *listener);

/* The next complete request, or 0. The caller frees it with tube_request_free(). */
int tube_listener_take(TubeListener *listener, TubeRequest *request);
void tube_request_free(TubeRequest *request);
/* Answers a request from any thread. `headers` are complete "Name: value\r\n"
 * lines (may be empty); Content-Length and Connection: close are added. */
void tube_listener_respond(TubeListener *listener, uint32_t id, int status, const char *headers,
                           const uint8_t *body, size_t body_size);

#endif
