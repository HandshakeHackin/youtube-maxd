/* Minimal HTTP/1.1 listener: 127.0.0.1 only, one request per connection,
 * bounded sizes and lifetimes, plain BSD sockets. One thread owns the sockets;
 * requests and answers cross to the engine thread through two small queues.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_listener.h"
#include "tube_js_platform.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

/* ---- the few places where the console differs ---- */
#if defined(EVO_TARGET_PS5) || defined(__PROSPERO__)
#ifndef SO_NBIO
#define SO_NBIO 0x1200
#endif
/* fcntl() does not work on the console's sockets; its own option does. */
static void socket_nonblocking(int fd) { int one = 1; setsockopt(fd, SOL_SOCKET, SO_NBIO, &one, sizeof one); }
#else
static void socket_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  fcntl(fd, F_SETFD, FD_CLOEXEC);
}
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
static void socket_quiet(int fd) {   /* a closed peer must not raise SIGPIPE */
#ifdef SO_NOSIGPIPE
  int one = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#else
  (void)fd;
#endif
}

enum { FREE, READING, WAITING, WRITING };
enum { READ_MS = 15000, WAIT_MS = 90000, WRITE_MS = 30000 };

typedef struct {
  int state, fd, head_only;
  uint32_t id;
  uint64_t deadline;
  char in[TUBE_LISTENER_HEAD_LIMIT + TUBE_LISTENER_BODY_LIMIT + 1];
  size_t in_size, head_size, body_size;
  char headers[TUBE_LISTENER_HEAD_LIMIT];
  char *method, *target;
  uint8_t *out;
  size_t out_size, out_sent;
} Connection;

typedef struct Queued { struct Queued *next; TubeRequest request; } Queued;
typedef struct Answer { struct Answer *next; uint32_t id; uint8_t *bytes; size_t size, head_size; } Answer;

struct TubeListener {
  int fd, wake_fd;
  unsigned short port;
  uint32_t next_id;
  pthread_t thread;
  atomic_int stop;
  void (*notify)(void *);
  void *opaque;
  pthread_mutex_t lock;          /* guards the two queues only */
  Queued *ready, *ready_tail;
  Answer *answers, *answers_tail;
  Connection connections[TUBE_LISTENER_CONNECTIONS];
};

static void drop(Connection *connection) {
  if (connection->state == FREE) return;
  close(connection->fd);
  free(connection->out);
  connection->out = NULL;
  connection->state = FREE;
}

static const char *reason(int status) {
  switch (status) {
    case 200: return "OK"; case 206: return "Partial Content"; case 302: return "Found"; case 400: return "Bad Request";
    case 401: return "Unauthorized"; case 403: return "Forbidden"; case 404: return "Not Found";
    case 405: return "Method Not Allowed"; case 408: return "Request Timeout"; case 410: return "Gone";
    case 413: return "Content Too Large"; case 416: return "Range Not Satisfiable";
    case 422: return "Unprocessable Content"; case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large"; case 502: return "Bad Gateway";
    case 503: return "Service Unavailable"; case 504: return "Gateway Timeout";
    default: return status >= 500 ? "Internal Server Error" : status >= 400 ? "Error" : "OK";
  }
}

/* The whole response as one block; `head_size` is where the body begins. */
static uint8_t *compose(int status, const char *headers, const uint8_t *body, size_t body_size, size_t *size, size_t *head_size) {
  char head[1024];
  if (!headers || strlen(headers) > 700) headers = "";
  int length = snprintf(head, sizeof head, "HTTP/1.1 %d %s\r\n%sContent-Length: %zu\r\nConnection: close\r\n\r\n",
                        status, reason(status), headers, body_size);
  uint8_t *bytes = malloc((size_t)length + body_size + 1);
  if (!bytes) return NULL;
  memcpy(bytes, head, (size_t)length);
  if (body_size) memcpy(bytes + length, body, body_size);
  *size = (size_t)length + body_size; *head_size = (size_t)length;
  return bytes;
}

/* Takes ownership of `bytes`. */
static void send_block(Connection *connection, uint8_t *bytes, size_t size, size_t head_size) {
  free(connection->out);
  connection->out = bytes;
  connection->out_size = connection->head_only ? head_size : size;
  connection->out_sent = 0;
  connection->state = WRITING;
  connection->deadline = tube_js_monotonic_ms() + WRITE_MS;
}

static void refuse(Connection *connection, int status) {
  size_t size = 0, head_size = 0;
  uint8_t *bytes = compose(status, "", NULL, 0, &size, &head_size);
  if (bytes) send_block(connection, bytes, size, head_size); else drop(connection);
}

/* Splits the request head in place. Returns 0 when it is acceptable. */
static int parse_head(Connection *connection, long *content_length) {
  char *head = connection->in, *line_end = strstr(head, "\r\n");
  char *space = memchr(head, ' ', (size_t)(line_end - head));
  if (!space) return 400;
  *space = 0;
  connection->method = head;
  connection->target = space + 1;
  char *version = memchr(connection->target, ' ', (size_t)(line_end - connection->target));
  if (!version || strncmp(version + 1, "HTTP/1.", 7) || connection->target[0] != '/') return 400;
  *version = 0;
  if (strcmp(head, "GET") && strcmp(head, "HEAD") && strcmp(head, "POST") && strcmp(head, "DELETE")) return 405;
  connection->head_only = !strcmp(head, "HEAD");
  size_t used = 0;
  *content_length = 0;
  for (char *line = line_end + 2; line < head + connection->head_size - 2;) {
    char *end = strstr(line, "\r\n"), *colon = memchr(line, ':', (size_t)(end - line));
    if (!colon || colon == line) return 400;
    char *value = colon + 1;
    while (value < end && (*value == ' ' || *value == '\t')) value++;
    size_t name_size = (size_t)(colon - line), value_size = (size_t)(end - value);
    if (used + name_size + value_size + 4 > sizeof connection->headers) return 431;
    for (size_t i = 0; i < name_size; i++) connection->headers[used++] = (char)tolower((unsigned char)line[i]);
    connection->headers[used++] = ':'; connection->headers[used++] = ' ';
    memcpy(connection->headers + used, value, value_size); used += value_size;
    connection->headers[used++] = '\n';
    if (name_size == 14 && !strncasecmp(line, "content-length", 14)) {
      char *stop = NULL;
      errno = 0;
      *content_length = strtol(value, &stop, 10);
      if (errno || stop != end || *content_length < 0 || !value_size) return 400;
    }
    if (name_size == 17 && !strncasecmp(line, "transfer-encoding", 17)) return 400;   /* no chunked bodies */
    line = end + 2;
  }
  connection->headers[used] = 0;
  if (*content_length > TUBE_LISTENER_BODY_LIMIT) return 413;
  return 0;
}

static char *copy_text(const char *text) {
  size_t size = strlen(text) + 1;
  char *copy = malloc(size);
  if (copy) memcpy(copy, text, size);
  return copy;
}

static void queue_request(TubeListener *listener, Connection *connection) {
  Queued *queued = calloc(1, sizeof *queued);
  if (queued) {
    TubeRequest *request = &queued->request;
    request->id = connection->id;
    snprintf(request->method, sizeof request->method, "%s", connection->method);
    request->target = copy_text(connection->target);
    request->headers = copy_text(connection->headers);
    request->body_size = connection->body_size;
    request->body = malloc(connection->body_size + 1);
    if (request->body) memcpy(request->body, connection->in + connection->head_size, connection->body_size);
    if (!request->target || !request->headers || !request->body) { tube_request_free(request); free(queued); queued = NULL; }
  }
  if (!queued) { refuse(connection, 503); return; }
  connection->state = WAITING;
  connection->deadline = tube_js_monotonic_ms() + WAIT_MS;
  pthread_mutex_lock(&listener->lock);
  if (listener->ready_tail) listener->ready_tail->next = queued; else listener->ready = queued;
  listener->ready_tail = queued;
  pthread_mutex_unlock(&listener->lock);
  if (listener->notify) listener->notify(listener->opaque);
}

static void read_request(TubeListener *listener, Connection *connection) {
  ssize_t got = recv(connection->fd, connection->in + connection->in_size, sizeof connection->in - 1 - connection->in_size, 0);
  if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
  if (got <= 0) { drop(connection); return; }
  connection->in_size += (size_t)got;
  connection->in[connection->in_size] = 0;
  if (!connection->head_size) {
    char *end = strstr(connection->in, "\r\n\r\n");
    if (!end) {
      if (connection->in_size >= TUBE_LISTENER_HEAD_LIMIT) refuse(connection, 431);
      return;
    }
    connection->head_size = (size_t)(end - connection->in) + 4;
    long length = 0;
    int refused = connection->head_size > TUBE_LISTENER_HEAD_LIMIT ? 431 : parse_head(connection, &length);
    if (refused) { refuse(connection, refused); return; }
    connection->body_size = (size_t)length;
  }
  if (connection->in_size - connection->head_size >= connection->body_size) queue_request(listener, connection);
}

static void write_response(Connection *connection) {
  ssize_t sent = send(connection->fd, connection->out + connection->out_sent, connection->out_size - connection->out_sent, MSG_NOSIGNAL);
  if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
  if (sent <= 0) { drop(connection); return; }
  connection->out_sent += (size_t)sent;
  if (connection->out_sent == connection->out_size) { shutdown(connection->fd, SHUT_WR); drop(connection); }
}

static void accept_connections(TubeListener *listener) {
  for (;;) {
    int fd = accept(listener->fd, NULL, NULL);
    if (fd < 0) return;
    Connection *slot = NULL;
    for (int i = 0; i < TUBE_LISTENER_CONNECTIONS && !slot; i++)
      if (listener->connections[i].state == FREE) slot = &listener->connections[i];
    if (!slot) { close(fd); continue; }       /* at capacity: the client retries */
    socket_nonblocking(fd);
    socket_quiet(fd);
    slot->fd = fd; slot->state = READING; slot->head_only = 0; slot->deadline = tube_js_monotonic_ms() + READ_MS;
    slot->in_size = slot->head_size = slot->body_size = 0;
    slot->in[0] = 0; slot->out = NULL;
    if (!++listener->next_id) ++listener->next_id;
    slot->id = listener->next_id;
  }
}

static void apply_answers(TubeListener *listener) {
  pthread_mutex_lock(&listener->lock);
  Answer *answer = listener->answers;
  listener->answers = listener->answers_tail = NULL;
  pthread_mutex_unlock(&listener->lock);
  while (answer) {
    Answer *next = answer->next;
    Connection *target = NULL;
    for (int i = 0; i < TUBE_LISTENER_CONNECTIONS && !target; i++)
      if (listener->connections[i].state == WAITING && listener->connections[i].id == answer->id) target = &listener->connections[i];
    if (target) send_block(target, answer->bytes, answer->size, answer->head_size); else free(answer->bytes);
    free(answer);
    answer = next;
  }
}

static void *listener_thread(void *opaque) {
  TubeListener *listener = opaque;
  while (!atomic_load(&listener->stop)) {
    struct pollfd fds[TUBE_LISTENER_CONNECTIONS + 2];
    Connection *owners[TUBE_LISTENER_CONNECTIONS + 2] = { NULL };
    unsigned count = 0;
    int waiting = 0;
    fds[count++] = (struct pollfd){ .fd = listener->fd, .events = POLLIN };
    fds[count++] = (struct pollfd){ .fd = listener->wake_fd, .events = POLLIN };
    for (int i = 0; i < TUBE_LISTENER_CONNECTIONS; i++) {
      Connection *connection = &listener->connections[i];
      if (connection->state == WAITING) waiting = 1;
      if (connection->state != READING && connection->state != WRITING) continue;
      owners[count] = connection;
      fds[count++] = (struct pollfd){ .fd = connection->fd, .events = connection->state == READING ? POLLIN : POLLOUT };
    }
    /* The short wait while an answer is due makes a lost wake cost little. */
    poll(fds, count, waiting ? (listener->wake_fd < 0 ? 10 : 50) : 250);
    if (listener->wake_fd >= 0 && fds[1].revents) { char sink[64]; while (recv(listener->wake_fd, sink, sizeof sink, 0) > 0) {} }
    apply_answers(listener);
    if (fds[0].revents) accept_connections(listener);
    for (unsigned i = 2; i < count; i++) {
      if (!fds[i].revents || owners[i]->state == FREE || owners[i]->fd != fds[i].fd) continue;
      if (owners[i]->state == READING) read_request(listener, owners[i]);
      else if (owners[i]->state == WRITING) write_response(owners[i]);
    }
    uint64_t now = tube_js_monotonic_ms();
    for (int i = 0; i < TUBE_LISTENER_CONNECTIONS; i++) {
      Connection *connection = &listener->connections[i];
      if (connection->state == FREE || connection->deadline > now) continue;
      if (connection->state == WAITING) refuse(connection, 504); else drop(connection);
    }
  }
  return NULL;
}

static int loopback_socket(int type, unsigned short port, struct sockaddr_in *bound) {
  struct sockaddr_in address;
  memset(&address, 0, sizeof address);
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* never any other interface */
  socklen_t size = sizeof address;
  int fd = socket(AF_INET, type, 0), one = 1;
  if (fd < 0) return -1;
  if (type == SOCK_STREAM) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  if (bind(fd, (struct sockaddr *)&address, sizeof address) || (type == SOCK_STREAM && listen(fd, 16)) ||
      getsockname(fd, (struct sockaddr *)&address, &size)) { close(fd); return -1; }
  socket_nonblocking(fd);
  socket_quiet(fd);
  *bound = address;
  return fd;
}

TubeListener *tube_listener_start(unsigned short port, void (*notify)(void *), void *opaque) {
  TubeListener *listener = calloc(1, sizeof *listener);
  if (!listener) return NULL;
  struct sockaddr_in bound;
  listener->fd = listener->wake_fd = -1;
  if (port) listener->fd = loopback_socket(SOCK_STREAM, port, &bound);
  else for (int attempt = 0; attempt < 24 && listener->fd < 0; attempt++) {
    uint16_t pick = 0;
    if (tube_js_random_bytes(&pick, sizeof pick)) pick = (uint16_t)(tube_js_monotonic_ns() >> 7);
    listener->fd = loopback_socket(SOCK_STREAM, (unsigned short)(20000 + pick % 20000), &bound);
  }
  if (listener->fd >= 0) listener->port = ntohs(bound.sin_port);
  if (listener->fd < 0) goto failed;
  /* A datagram socket connected to itself: any thread can end the poll wait.
   * Nothing depends on it: where it cannot be made (it is unproven on the
   * console) the short waits of the loop above stand in for the wake. */
  struct sockaddr_in self;
  listener->wake_fd = loopback_socket(SOCK_DGRAM, 0, &self);
  if (listener->wake_fd >= 0 && connect(listener->wake_fd, (struct sockaddr *)&self, sizeof self)) {
    close(listener->wake_fd); listener->wake_fd = -1;
  }
  listener->notify = notify; listener->opaque = opaque;
  pthread_mutex_init(&listener->lock, NULL);
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, (size_t)512 << 10);
  int failed = pthread_create(&listener->thread, &attributes, listener_thread, listener);
  pthread_attr_destroy(&attributes);
  if (!failed) return listener;
  pthread_mutex_destroy(&listener->lock);
failed:
  if (listener->fd >= 0) close(listener->fd);
  if (listener->wake_fd >= 0) close(listener->wake_fd);
  free(listener);
  return NULL;
}

static void wake(TubeListener *listener) { if (listener->wake_fd >= 0) send(listener->wake_fd, "w", 1, MSG_NOSIGNAL); }

void tube_listener_stop(TubeListener *listener) {
  if (!listener) return;
  atomic_store(&listener->stop, 1);
  wake(listener);
  pthread_join(listener->thread, NULL);
  for (int i = 0; i < TUBE_LISTENER_CONNECTIONS; i++) drop(&listener->connections[i]);
  close(listener->fd);
  if (listener->wake_fd >= 0) close(listener->wake_fd);
  TubeRequest request;
  while (tube_listener_take(listener, &request)) tube_request_free(&request);
  for (Answer *answer = listener->answers; answer;) { Answer *next = answer->next; free(answer->bytes); free(answer); answer = next; }
  pthread_mutex_destroy(&listener->lock);
  free(listener);
}

unsigned short tube_listener_port(const TubeListener *listener) { return listener->port; }

int tube_listener_take(TubeListener *listener, TubeRequest *request) {
  pthread_mutex_lock(&listener->lock);
  Queued *queued = listener->ready;
  if (queued) { listener->ready = queued->next; if (!listener->ready) listener->ready_tail = NULL; }
  pthread_mutex_unlock(&listener->lock);
  if (!queued) return 0;
  *request = queued->request;
  free(queued);
  return 1;
}

void tube_request_free(TubeRequest *request) {
  free(request->target); free(request->headers); free(request->body);
  request->target = request->headers = NULL; request->body = NULL;
}

void tube_listener_respond(TubeListener *listener, uint32_t id, int status, const char *headers,
                           const uint8_t *body, size_t body_size) {
  Answer *answer = calloc(1, sizeof *answer);
  if (!answer) return;
  answer->id = id;
  answer->bytes = compose(status, headers, body, body_size, &answer->size, &answer->head_size);
  if (!answer->bytes) { free(answer); return; }
  pthread_mutex_lock(&listener->lock);
  if (listener->answers_tail) listener->answers_tail->next = answer; else listener->answers = answer;
  listener->answers_tail = answer;
  pthread_mutex_unlock(&listener->lock);
  wake(listener);
}
