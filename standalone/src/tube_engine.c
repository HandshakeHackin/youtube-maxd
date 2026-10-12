/* The engine thread: QuickJS-NG running the service bundle over the native
 * layer of standalone/ps5 (HTTPS client, clock, timers, random bytes, hashes)
 * and the loopback listener. The same C builds for the host and the console.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * One thread owns the runtime, the HTTPS client and the timer queue, so none
 * of them needs a lock. Its loop: take requests from the listener, drain
 * promise jobs, fire due timers, wait on the transfers (the listener wakes
 * that wait), hand finished transfers to JavaScript. JavaScript sees the
 * machine only through `__host`; no QuickJS `std`/`os` module exists. */
#include "tube_service.h"
#include "quickjs.h"
#include "tube_js_http.h"
#include "tube_js_platform.h"
#include "tube_listener.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/resource.h>
#endif

#if defined(EVO_TARGET_PS5) || defined(__PROSPERO__)
/* On the console the bundle and the root certificates are part of the
 * executable (standalone/ps5/embed_data.py). */
#include "tube_js_console.h"
extern const unsigned char tube_js_bundle[], tube_js_ca_pem[];
extern const size_t tube_js_bundle_length, tube_js_ca_pem_length;
#define TUBE_SERVICE_EMBEDDED 1
/* The POSIX name is not part of the app's libc surface; EVO creates its own
 * folders the same way (src/evo_data_path.c). */
int sceKernelMkdir(const char *path, int mode);
#define mkdir(path, mode) sceKernelMkdir(path, mode)
#endif

#define FILE_LIMIT ((size_t)16 << 20)
#define HEADER_LIMIT 64

typedef struct {
  JSRuntime *runtime;
  JSContext *ctx;
  tube_js_http *http;
  tube_js_timers *timers;
  TubeListener *listener;
  JSValue on_timer, http_done, on_request;
  atomic_int stop;
  unsigned requests, served;
  uint64_t bytes_in;
  double bundle_ms;
} Engine;

static double now_ms(void) { return (double)tube_js_monotonic_ns() / 1e6; }

/* Bytes of an ArrayBuffer or typed array; NULL (no exception) for anything else. */
static uint8_t *bytes_of(JSContext *ctx, JSValueConst value, size_t *size) {
  *size = 0;
  if (!JS_IsObject(value)) return NULL;
  uint8_t *data = JS_GetArrayBuffer(ctx, size, value);
  if (data) return data;
  JS_FreeValue(ctx, JS_GetException(ctx));
  size_t offset = 0, length = 0, whole = 0;
  JSValue buffer = JS_GetTypedArrayBuffer(ctx, value, &offset, &length, NULL);
  if (JS_IsException(buffer)) { JS_FreeValue(ctx, JS_GetException(ctx)); return NULL; }
  data = JS_GetArrayBuffer(ctx, &whole, buffer);
  JS_FreeValue(ctx, buffer);
  if (!data) { JS_FreeValue(ctx, JS_GetException(ctx)); return NULL; }
  *size = length;
  return data + offset;
}

static void report(JSContext *ctx, JSValueConst error) {
  const char *text = JS_ToCString(ctx, error);
  fprintf(stderr, "tube_js: %s\n", text ? text : "unknown failure");
  JS_FreeCString(ctx, text);
  JSValue stack = JS_GetPropertyStr(ctx, error, "stack");
  if (JS_IsString(stack) && getenv("TUBE_JS_TRACE")) {
    const char *trace = JS_ToCString(ctx, stack);
    if (trace) fprintf(stderr, "%s\n", trace);
    JS_FreeCString(ctx, trace);
  }
  JS_FreeValue(ctx, stack);
}
static void report_pending(JSContext *ctx) {
  JSValue error = JS_GetException(ctx);
  report(ctx, error);
  JS_FreeValue(ctx, error);
}

/* sha1(data) / sha256(data) -> hex; data is a string (its UTF-8) or bytes. */
static JSValue host_hash(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv, int magic) {
  (void)self; (void)argc;
  unsigned char digest[32]; char hex[65]; size_t size = 0, length = magic ? 32 : 20;
  const char *text = NULL;
  const void *data = bytes_of(ctx, argv[0], &size);
  if (!data && !(data = text = JS_ToCStringLen(ctx, &size, argv[0]))) return JS_EXCEPTION;
  if (magic) tube_js_sha256_digest(data, size, digest); else tube_js_sha1_digest(data, size, digest);
  JS_FreeCString(ctx, text);
  for (size_t i = 0; i < length; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return JS_NewStringLen(ctx, hex, length * 2);
}

static JSValue host_random(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  uint32_t count = 0; uint8_t data[256];
  if (JS_ToUint32(ctx, &count, argv[0])) return JS_EXCEPTION;
  if (count > sizeof data || tube_js_random_bytes(data, count)) return JS_ThrowRangeError(ctx, "random bytes unavailable");
  return JS_NewArrayBufferCopy(ctx, data, count);
}

static JSValue host_now(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc; (void)argv;
  return JS_NewFloat64(ctx, now_ms());
}

/* timerAdd(delay_ms, repeat_ms) -> id; the loop calls __tubeTimer(id) when it is due. */
static JSValue host_timer_add(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  Engine *engine = JS_GetContextOpaque(ctx);
  double delay = 0, repeat = 0;
  if (JS_ToFloat64(ctx, &delay, argv[0]) || JS_ToFloat64(ctx, &repeat, argv[1])) return JS_EXCEPTION;
  if (!(delay >= 0)) delay = 0;
  if (!(repeat >= 0)) repeat = 0;
  if (delay > 86400000.0 * 30) delay = 86400000.0 * 30;
  uint32_t id = tube_js_timers_add(engine->timers, tube_js_monotonic_ms(), (uint64_t)(delay + 0.999), (uint64_t)(repeat + 0.999));
  return id ? JS_NewUint32(ctx, id) : JS_ThrowRangeError(ctx, "too many timers");
}
static JSValue host_timer_cancel(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  Engine *engine = JS_GetContextOpaque(ctx);
  uint32_t id = 0;
  if (JS_ToUint32(ctx, &id, argv[0])) return JS_EXCEPTION;
  return JS_NewBool(ctx, tube_js_timers_cancel(engine->timers, id));
}

static JSValue host_utf8_decode(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  size_t size = 0;
  uint8_t *data = bytes_of(ctx, argv[0], &size);
  if (!data) return JS_NewStringLen(ctx, "", 0);
  if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) { data += 3; size -= 3; }
  return JS_NewStringLen(ctx, (const char *)data, size);
}

static JSValue host_utf8_encode(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  size_t size = 0;
  const char *text = JS_ToCStringLen(ctx, &size, argv[0]);
  if (!text) return JS_EXCEPTION;
  JSValue result = JS_NewArrayBufferCopy(ctx, (const uint8_t *)text, size);
  JS_FreeCString(ctx, text);
  return result;
}

static JSValue host_write(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv, int magic) {
  (void)self; (void)argc;
  size_t size = 0;
  const char *text = JS_ToCStringLen(ctx, &size, argv[0]);
  if (!text) return JS_EXCEPTION;
  fwrite(text, 1, size, magic ? stderr : stdout);
  fputc('\n', magic ? stderr : stdout);
  JS_FreeCString(ctx, text);
  return JS_UNDEFINED;
}

static JSValue host_getenv(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  const char *name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_EXCEPTION;
  const char *value = getenv(name);
  JS_FreeCString(ctx, name);
  return value ? JS_NewString(ctx, value) : JS_UNDEFINED;
}

static uint8_t *load(const char *path, size_t *size, size_t limit) {
  FILE *file = fopen(path, "rb");
  if (!file) return NULL;
  uint8_t *data = NULL;
  if (!fseek(file, 0, SEEK_END)) {
    long length = ftell(file);
    rewind(file);
    if (length >= 0 && (size_t)length <= limit && (data = malloc((size_t)length + 1))) {
      if (fread(data, 1, (size_t)length, file) != (size_t)length) { free(data); data = NULL; }
      else { data[length] = 0; *size = (size_t)length; }
    }
  }
  fclose(file);
  return data;
}

static JSValue host_read_file(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  const char *path = JS_ToCString(ctx, argv[0]);
  if (!path) return JS_EXCEPTION;
  size_t size = 0;
  uint8_t *data = load(path, &size, FILE_LIMIT);
  JS_FreeCString(ctx, path);
  if (!data) return JS_UNDEFINED;
  JSValue result = JS_NewArrayBufferCopy(ctx, data, size);
  free(data);
  return result;
}

static void make_parents(const char *path) {
  char directory[1024];
  snprintf(directory, sizeof directory, "%s", path);
  for (char *slash = directory + 1; (slash = strchr(slash, '/')); slash++) { *slash = 0; mkdir(directory, 0700); *slash = '/'; }
}

/* Private file, written through a temporary name so a crash never leaves half an entry. */
static int store(const char *path, const uint8_t *data, size_t size) {
  char temporary[1040];
  make_parents(path);
  snprintf(temporary, sizeof temporary, "%s.tmp", path);
  int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) return -1;
  int ok = 1;
  for (size_t sent = 0; ok && sent < size;) {
    ssize_t wrote = write(fd, data + sent, size - sent);
    if (wrote <= 0) ok = 0; else sent += (size_t)wrote;
  }
  ok = close(fd) == 0 && ok;
  if (!ok || rename(temporary, path)) { remove(temporary); return -1; }
  return 0;
}

static JSValue host_write_file(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  size_t size = 0;
  uint8_t *data = bytes_of(ctx, argv[1], &size);
  const char *path = JS_ToCString(ctx, argv[0]);
  if (!path) return JS_EXCEPTION;
  int ok = data && !store(path, data, size);
  JS_FreeCString(ctx, path);
  return JS_NewBool(ctx, ok);
}

static JSValue host_remove_file(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  const char *path = JS_ToCString(ctx, argv[0]);
  if (!path) return JS_EXCEPTION;
  int ok = remove(path) == 0;
  JS_FreeCString(ctx, path);
  return JS_NewBool(ctx, ok);
}

static JSValue host_stats(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc; (void)argv;
  Engine *engine = JS_GetContextOpaque(ctx);
  JSMemoryUsage memory;
  double peak = 0;
#ifdef __linux__
  struct rusage usage;
  if (!getrusage(RUSAGE_SELF, &usage)) peak = (double)usage.ru_maxrss;
#endif
  JS_ComputeMemoryUsage(JS_GetRuntime(ctx), &memory);
  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "requests", JS_NewUint32(ctx, engine->requests));
  JS_SetPropertyStr(ctx, result, "served", JS_NewUint32(ctx, engine->served));
  JS_SetPropertyStr(ctx, result, "bytes_in", JS_NewFloat64(ctx, (double)engine->bytes_in));
  JS_SetPropertyStr(ctx, result, "bundle_ms", JS_NewFloat64(ctx, engine->bundle_ms));
  JS_SetPropertyStr(ctx, result, "peak_rss_kb", JS_NewFloat64(ctx, peak));
  JS_SetPropertyStr(ctx, result, "js_heap_kb", JS_NewFloat64(ctx, (double)memory.malloc_size / 1024.0));
  JS_SetPropertyStr(ctx, result, "http", JS_NewString(ctx, tube_js_http_backend()));
  return result;
}

/* Evaluate a script in a fresh context of the same runtime and return its
 * string result. The context has the language built-ins only: no `__host`,
 * no network, no files. Used for functions extracted from the player script. */
static JSValue host_eval_isolated(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  size_t size = 0;
  const char *code = JS_ToCStringLen(ctx, &size, argv[0]);
  if (!code) return JS_EXCEPTION;
  JSContext *inner = JS_NewContext(JS_GetRuntime(ctx));
  if (!inner) { JS_FreeCString(ctx, code); return JS_ThrowOutOfMemory(ctx); }
  JSValue value = JS_Eval(inner, code, size, "player.js", JS_EVAL_TYPE_GLOBAL), result;
  JS_FreeCString(ctx, code);
  int failed = JS_IsException(value);
  if (failed) value = JS_GetException(inner);
  size_t length = 0;
  const char *text = JS_ToCStringLen(inner, &length, value);
  if (failed || !text) result = JS_ThrowTypeError(ctx, "player evaluation failed: %s", text ? text : "no result");
  else result = JS_NewStringLen(ctx, text, length);
  JS_FreeCString(inner, text);
  JS_FreeValue(inner, value);
  JS_FreeContext(inner);
  return result;
}

static const char *string_property(JSContext *ctx, JSValueConst object, const char *name) {
  JSValue value = JS_GetPropertyStr(ctx, object, name);
  const char *text = JS_IsString(value) ? JS_ToCString(ctx, value) : NULL;
  JS_FreeValue(ctx, value);
  return text;   /* JS_FreeCString by the caller */
}
static double number_property(JSContext *ctx, JSValueConst object, const char *name, double fallback) {
  JSValue value = JS_GetPropertyStr(ctx, object, name);
  double number = fallback;
  if (JS_IsNumber(value)) JS_ToFloat64(ctx, &number, value);
  JS_FreeValue(ctx, value);
  return number;
}

/* httpStart({method, url, headers: [[name, value]...], body, redirect, timeout_ms, max_bytes}) -> id.
 * `redirect` is 0 follow, 1 manual, 2 error. The answer arrives later through
 * __tubeHttpDone(id, {status, url, redirected, headers, body} | undefined, error). */
static JSValue host_http_start(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  Engine *engine = JS_GetContextOpaque(ctx);
  const char *method = string_property(ctx, argv[0], "method"), *url = string_property(ctx, argv[0], "url");
  const char *names[HEADER_LIMIT], *values[HEADER_LIMIT];
  tube_js_http_header headers[HEADER_LIMIT];
  size_t count = 0, body_size = 0;
  JSValue list = JS_GetPropertyStr(ctx, argv[0], "headers"), body = JS_GetPropertyStr(ctx, argv[0], "body");
  for (uint32_t i = 0; count < HEADER_LIMIT; i++) {
    JSValue pair = JS_GetPropertyUint32(ctx, list, i);
    if (!JS_IsObject(pair)) { JS_FreeValue(ctx, pair); break; }
    JSValue name = JS_GetPropertyUint32(ctx, pair, 0), value = JS_GetPropertyUint32(ctx, pair, 1);
    names[count] = JS_ToCString(ctx, name); values[count] = JS_ToCString(ctx, value);
    if (names[count] && values[count]) { headers[count].name = names[count]; headers[count].value = values[count]; count++; }
    else { JS_FreeCString(ctx, names[count]); JS_FreeCString(ctx, values[count]); }
    JS_FreeValue(ctx, name); JS_FreeValue(ctx, value); JS_FreeValue(ctx, pair);
  }
  uint8_t *body_data = bytes_of(ctx, body, &body_size);
  tube_js_http_request request = { method, url, headers, count, body_data, body_size,
    (int)number_property(ctx, argv[0], "redirect", TUBE_JS_HTTP_REDIRECT_FOLLOW), (long)number_property(ctx, argv[0], "timeout_ms", 0),
    (size_t)number_property(ctx, argv[0], "max_bytes", 0) };
  uint32_t id = 0;
  int error = url && !strncmp(url, "https://", 8) ? tube_js_http_start(engine->http, &request, &id) : TUBE_JS_HTTP_INVALID;
  if (!error) engine->requests++;
  for (size_t i = 0; i < count; i++) { JS_FreeCString(ctx, names[i]); JS_FreeCString(ctx, values[i]); }
  JS_FreeValue(ctx, list); JS_FreeValue(ctx, body);
  JS_FreeCString(ctx, method); JS_FreeCString(ctx, url);
  return error ? JS_ThrowTypeError(ctx, "fetch failed: %s", tube_js_http_error_name(error)) : JS_NewUint32(ctx, id);
}

static JSValue host_http_cancel(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  Engine *engine = JS_GetContextOpaque(ctx);
  uint32_t id = 0;
  if (JS_ToUint32(ctx, &id, argv[0])) return JS_EXCEPTION;
  return JS_NewBool(ctx, tube_js_http_cancel(engine->http, id));
}

/* respond(id, status, [[name, value]...], body): answers a listener request. */
static JSValue host_respond(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  Engine *engine = JS_GetContextOpaque(ctx);
  uint32_t id = 0; int32_t status = 500;
  if (!engine->listener || JS_ToUint32(ctx, &id, argv[0]) || JS_ToInt32(ctx, &status, argv[1])) return JS_FALSE;
  char headers[700] = "";
  size_t used = 0, size = 0;
  for (uint32_t i = 0; i < 16; i++) {
    JSValue pair = JS_GetPropertyUint32(ctx, argv[2], i);
    if (!JS_IsObject(pair)) { JS_FreeValue(ctx, pair); break; }
    JSValue name = JS_GetPropertyUint32(ctx, pair, 0), value = JS_GetPropertyUint32(ctx, pair, 1);
    const char *n = JS_ToCString(ctx, name), *v = JS_ToCString(ctx, value);
    if (n && v && *n && !strpbrk(n, "\r\n: ") && !strpbrk(v, "\r\n") && used + strlen(n) + strlen(v) + 5 < sizeof headers)
      used += (size_t)snprintf(headers + used, sizeof headers - used, "%s: %s\r\n", n, v);
    JS_FreeCString(ctx, n); JS_FreeCString(ctx, v);
    JS_FreeValue(ctx, name); JS_FreeValue(ctx, value); JS_FreeValue(ctx, pair);
  }
  uint8_t *data = bytes_of(ctx, argv[3], &size);
  tube_listener_respond(engine->listener, id, status < 100 || status > 599 ? 500 : status, headers, data, size);
  return JS_TRUE;
}

static const JSCFunctionListEntry host_functions[] = {
  JS_CFUNC_DEF("httpStart", 1, host_http_start),
  JS_CFUNC_DEF("httpCancel", 1, host_http_cancel),
  JS_CFUNC_DEF("respond", 4, host_respond),
  JS_CFUNC_DEF("timerAdd", 2, host_timer_add),
  JS_CFUNC_DEF("timerCancel", 1, host_timer_cancel),
  JS_CFUNC_DEF("evalIsolated", 1, host_eval_isolated),
  JS_CFUNC_MAGIC_DEF("sha1", 1, host_hash, 0),
  JS_CFUNC_MAGIC_DEF("sha256", 1, host_hash, 1),
  JS_CFUNC_DEF("random", 1, host_random),
  JS_CFUNC_DEF("now", 0, host_now),
  JS_CFUNC_DEF("utf8Decode", 1, host_utf8_decode),
  JS_CFUNC_DEF("utf8Encode", 1, host_utf8_encode),
  JS_CFUNC_MAGIC_DEF("print", 1, host_write, 0),
  JS_CFUNC_MAGIC_DEF("log", 1, host_write, 1),
  JS_CFUNC_DEF("getenv", 1, host_getenv),
  JS_CFUNC_DEF("readFile", 1, host_read_file),
  JS_CFUNC_DEF("writeFile", 2, host_write_file),
  JS_CFUNC_DEF("removeFile", 1, host_remove_file),
  JS_CFUNC_DEF("stats", 0, host_stats),
};

static void call_and_free(Engine *engine, JSValueConst function, int argc, JSValue *argv) {
  JSValue result = JS_Call(engine->ctx, function, JS_UNDEFINED, argc, argv);
  if (JS_IsException(result)) report_pending(engine->ctx);
  JS_FreeValue(engine->ctx, result);
  for (int i = 0; i < argc; i++) JS_FreeValue(engine->ctx, argv[i]);
}

static void deliver_transfer(Engine *engine, const tube_js_http_response *response) {
  JSContext *ctx = engine->ctx;
  JSValue argv[3] = { JS_NewUint32(ctx, response->id), JS_UNDEFINED, JS_UNDEFINED };
  if (response->error == TUBE_JS_HTTP_OK) {
    engine->bytes_in += response->body_length;
    JSValue headers = JS_NewArray(ctx);
    for (size_t i = 0; i < response->header_count; i++) {
      JSValue pair = JS_NewArray(ctx);
      JS_SetPropertyUint32(ctx, pair, 0, JS_NewString(ctx, response->headers[i].name));
      JS_SetPropertyUint32(ctx, pair, 1, JS_NewString(ctx, response->headers[i].value));
      JS_SetPropertyUint32(ctx, headers, (uint32_t)i, pair);
    }
    argv[1] = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, argv[1], "status", JS_NewInt32(ctx, (int32_t)response->status));
    JS_SetPropertyStr(ctx, argv[1], "url", JS_NewString(ctx, response->url ? response->url : ""));
    JS_SetPropertyStr(ctx, argv[1], "redirected", JS_NewBool(ctx, response->redirected));
    JS_SetPropertyStr(ctx, argv[1], "headers", headers);
    JS_SetPropertyStr(ctx, argv[1], "body", JS_NewArrayBufferCopy(ctx, response->body ? response->body : (const uint8_t *)"", response->body_length));
  } else argv[2] = JS_NewString(ctx, *response->error_text ? response->error_text : tube_js_http_error_name(response->error));
  call_and_free(engine, engine->http_done, 3, argv);
}

static void deliver_request(Engine *engine, const TubeRequest *request) {
  JSContext *ctx = engine->ctx;
  engine->served++;
  JSValue argv[5] = { JS_NewUint32(ctx, request->id), JS_NewString(ctx, request->method), JS_NewString(ctx, request->target),
    JS_NewString(ctx, request->headers), JS_NewArrayBufferCopy(ctx, request->body, request->body_size) };
  call_and_free(engine, engine->on_request, 5, argv);
}

/* Runs until `promise` settles (command mode) or until a stop is requested. */
static void engine_loop(Engine *engine, JSValueConst promise) {
  JSContext *ctx = engine->ctx;
  int command = JS_IsObject(promise);
  while (!atomic_load(&engine->stop)) {
    TubeRequest request;
    while (engine->listener && tube_listener_take(engine->listener, &request)) { deliver_request(engine, &request); tube_request_free(&request); }
    JSContext *job_ctx;
    int ran;
    while ((ran = JS_ExecutePendingJob(engine->runtime, &job_ctx)) > 0) {}
    if (ran < 0) report_pending(job_ctx);
    if (command && JS_PromiseState(ctx, promise) != JS_PROMISE_PENDING) break;
    uint32_t due;
    for (int fired = 0; fired < 64 && (due = tube_js_timers_pop_due(engine->timers, tube_js_monotonic_ms())); fired++) {
      JSValue argv[1] = { JS_NewUint32(ctx, due) };
      call_and_free(engine, engine->on_timer, 1, argv);
    }
    int busy = JS_IsJobPending(engine->runtime), delay = tube_js_timers_next_delay(engine->timers, tube_js_monotonic_ms());
    if (command && delay < 0 && !busy && !tube_js_http_active(engine->http)) { fprintf(stderr, "tube_js: the operation stalled\n"); break; }
    /* The cap keeps the loop correct even where the wake cannot be delivered. */
    tube_js_http_poll(engine->http, busy ? 0 : delay < 0 || delay > 100 ? 100 : delay);
    tube_js_http_response *response;
    while ((response = tube_js_http_take(engine->http))) { deliver_transfer(engine, response); tube_js_http_response_free(response); }
  }
}

static int interrupted(JSRuntime *runtime, void *opaque) { (void)runtime; return atomic_load(&((Engine *)opaque)->stop); }

/* Creates the runtime and evaluates the bundle on the calling thread.
 * `config` becomes `__host.config` (consumed). */
static const char *engine_open(Engine *engine, const tube_service_options *options, JSValue (*settings)(JSContext *, void *), void *opaque) {
  engine->on_timer = engine->http_done = engine->on_request = JS_UNDEFINED;
  /* IPv4 only: media links are bound to the address that asked for them, and
   * the player (FFmpeg) reaches the media servers over IPv4. With links
   * obtained over IPv6 every read by the player was refused with 403. */
  tube_js_http_config http = { .ca_file = options->ca_file, .timeout_ms = 20000, .max_requests = 32, .ipv4_only = 1 };
#ifdef TUBE_SERVICE_EMBEDDED
  if (!options->ca_file) { http.ca_data = tube_js_ca_pem; http.ca_length = tube_js_ca_pem_length; }
  http.socket_setup = tube_js_console_socket_setup;
  http.poll_slice_ms = 10;   /* the console's wait does not always end when a socket is ready */
#endif
  engine->http = tube_js_http_new(&http);
  engine->timers = tube_js_timers_new();
  engine->runtime = JS_NewRuntime();
  if (!engine->http || !engine->timers || !engine->runtime) return "The service engine could not be created.";
  /* The player-script analysis needs far more stack and heap than the normal
   * path; it is only possible when the host said so (and gave the thread the
   * stack for it). */
  JS_SetMaxStackSize(engine->runtime, options->allow_player_script ? (size_t)60 << 20 : TUBE_SERVICE_JS_STACK);
  JS_SetMemoryLimit(engine->runtime, options->allow_player_script ? (size_t)640 << 20 : TUBE_SERVICE_JS_MEMORY);
  JS_SetInterruptHandler(engine->runtime, interrupted, engine);
  JSContext *ctx = engine->ctx = JS_NewContext(engine->runtime);
  if (!ctx) return "The service engine could not be created.";
  JS_SetContextOpaque(ctx, engine);
  JSValue global = JS_GetGlobalObject(ctx), host = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, host, host_functions, sizeof host_functions / sizeof *host_functions);
  JS_SetPropertyStr(ctx, host, "engine", JS_NewString(ctx, JS_GetVersion()));
  JSValue config = settings(ctx, opaque);
  JS_SetPropertyStr(ctx, config, "allow_player_script", JS_NewBool(ctx, options->allow_player_script));
  JS_SetPropertyStr(ctx, host, "config", config);
  JS_SetPropertyStr(ctx, global, "__host", host);

  const char *failure = NULL;
  const uint8_t *source = options->bundle_data;
  size_t size = options->bundle_length;
  uint8_t *loaded_file = NULL;
#ifdef TUBE_SERVICE_EMBEDDED
  if (!source && !options->bundle_path) { source = tube_js_bundle; size = tube_js_bundle_length; }
#endif
  if (!source && options->bundle_path) source = loaded_file = load(options->bundle_path, &size, FILE_LIMIT);
  if (!source) failure = "The service bundle could not be read.";
  else {
    double started = now_ms();
    /* JS_Eval needs a terminating zero after the text; both sources have one. */
    JSValue loaded = JS_Eval(ctx, (const char *)source, size, "tube_js.bundle.js", JS_EVAL_TYPE_GLOBAL);
    engine->bundle_ms = now_ms() - started;
    if (JS_IsException(loaded)) { report_pending(ctx); failure = "The service bundle failed to start."; }
    else {
      engine->on_timer = JS_GetPropertyStr(ctx, global, "__tubeTimer");
      engine->http_done = JS_GetPropertyStr(ctx, global, "__tubeHttpDone");
      engine->on_request = JS_GetPropertyStr(ctx, global, "__tubeRequest");
      if (!JS_IsFunction(ctx, engine->on_timer) || !JS_IsFunction(ctx, engine->http_done) || !JS_IsFunction(ctx, engine->on_request))
        failure = "The service bundle is not a service.";
    }
    JS_FreeValue(ctx, loaded);
  }
  free(loaded_file);
  JS_FreeValue(ctx, global);
  return failure;
}

static void engine_close(Engine *engine) {
  if (engine->http) tube_js_http_free(engine->http);     /* before the engine: no result can arrive afterwards */
  if (engine->ctx) {
    JS_FreeValue(engine->ctx, engine->on_timer);
    JS_FreeValue(engine->ctx, engine->http_done);
    JS_FreeValue(engine->ctx, engine->on_request);
    JS_FreeContext(engine->ctx);
  }
  if (engine->runtime) JS_FreeRuntime(engine->runtime);
  if (engine->timers) tube_js_timers_free(engine->timers);
  engine->http = NULL; engine->ctx = NULL; engine->runtime = NULL; engine->timers = NULL;
}

static void set_string(JSContext *ctx, JSValueConst object, const char *name, const char *value) {
  JS_SetPropertyStr(ctx, object, name, value ? JS_NewString(ctx, value) : JS_UNDEFINED);
}

static size_t thread_stack(const tube_service_options *options) {
  return options->allow_player_script ? (size_t)64 << 20 : TUBE_SERVICE_THREAD_STACK;
}

/* ---- the service (one per process) ---- */
static struct {
  pthread_mutex_t lock;            /* start/stop/running from the app's threads */
  pthread_mutex_t ready_lock;
  pthread_cond_t ready_changed;
  int running, ready;              /* ready: 0 starting, 1 serving, -1 failed */
  pthread_t thread;
  Engine engine;
  tube_service_options options;
  char state_dir[900], origin[48], token[65];
  unsigned short port;
  const char *error;
} service = { .lock = PTHREAD_MUTEX_INITIALIZER, .ready_lock = PTHREAD_MUTEX_INITIALIZER,
              .ready_changed = PTHREAD_COND_INITIALIZER, .error = "" };

static JSValue service_settings(JSContext *ctx, void *opaque) {
  (void)opaque;
  JSValue config = JS_NewObject(ctx);
  set_string(ctx, config, "mode", "serve");
  set_string(ctx, config, "origin", service.origin);
  set_string(ctx, config, "token", service.token);
  set_string(ctx, config, "state_dir", service.state_dir);
  set_string(ctx, config, "sponsorblock_server", service.options.sponsorblock_server);
  set_string(ctx, config, "pairing_code", service.options.pairing_code);
  JS_SetPropertyStr(ctx, config, "sponsorblock", JS_NewBool(ctx, service.options.sponsorblock));
  JS_SetPropertyStr(ctx, config, "ranged_manifest", JS_NewBool(ctx, service.options.ranged_manifest));
  return config;
}

static void wake_engine(void *opaque) { tube_js_http_wake(((Engine *)opaque)->http); }

static void *service_thread(void *opaque) {
  (void)opaque;
  Engine *engine = &service.engine;
  const char *failure = engine_open(engine, &service.options, service_settings, NULL);
  /* The listener starts last: no request can arrive before the engine can answer. */
  if (!failure && !(engine->listener = tube_listener_start(service.port, wake_engine, engine)))
    failure = "No port on 127.0.0.1 could be opened.";
  if (!failure) {
    service.port = tube_listener_port(engine->listener);
    /* The bundle read the origin while loading; hand over the real one now. */
    snprintf(service.origin, sizeof service.origin, "http://127.0.0.1:%u", (unsigned)service.port);
    JSValue global = JS_GetGlobalObject(engine->ctx), host = JS_GetPropertyStr(engine->ctx, global, "__host");
    JSValue config = JS_GetPropertyStr(engine->ctx, host, "config");
    set_string(engine->ctx, config, "origin", service.origin);
    JS_FreeValue(engine->ctx, config); JS_FreeValue(engine->ctx, host); JS_FreeValue(engine->ctx, global);
  }
  pthread_mutex_lock(&service.ready_lock);
  service.error = failure ? failure : "";
  service.ready = failure ? -1 : 1;
  pthread_cond_broadcast(&service.ready_changed);
  pthread_mutex_unlock(&service.ready_lock);
  if (!failure) engine_loop(engine, JS_UNDEFINED);
  tube_listener_stop(engine->listener);
  engine->listener = NULL;
  engine_close(engine);
  return NULL;
}

static const char token_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

void tube_service_configure(const tube_service_options *options) {
  pthread_mutex_lock(&service.lock);
  if (!service.running) { if (options) service.options = *options; else memset(&service.options, 0, sizeof service.options); }
  pthread_mutex_unlock(&service.lock);
}

int tube_service_start(const char *state_dir, unsigned short *port, char token[65]) {
  pthread_mutex_lock(&service.lock);
  const char *failure = NULL;
  if (!service.running) {
    unsigned char random[32];
    const char *given = service.options.token;
    size_t length = given ? strlen(given) : 0;
    if (!state_dir || !*state_dir || strlen(state_dir) >= sizeof service.state_dir) failure = "The service needs a data directory.";
    else if (given && (length < 16 || length > 64 || strspn(given, token_alphabet) != length)) failure = "The service token is not usable.";
    else if (!given && tube_js_random_bytes(random, sizeof random)) failure = "No secure random numbers are available.";
    else {
      if (given) memcpy(service.token, given, length + 1);
      else for (size_t i = 0; i < sizeof random; i++) snprintf(service.token + i * 2, 3, "%02x", random[i]);
      snprintf(service.state_dir, sizeof service.state_dir, "%s", state_dir);
      char probe[1000];
      snprintf(probe, sizeof probe, "%s/cache/.keep", service.state_dir);
      make_parents(probe);
      service.port = service.options.port;
      snprintf(service.origin, sizeof service.origin, "http://127.0.0.1:%u", (unsigned)service.port);
      atomic_store(&service.engine.stop, 0);
      service.ready = 0;
      pthread_attr_t attributes;
      pthread_attr_init(&attributes);
      pthread_attr_setstacksize(&attributes, thread_stack(&service.options));
      int refused = pthread_create(&service.thread, &attributes, service_thread, NULL);
      pthread_attr_destroy(&attributes);
      if (refused) failure = "The service thread could not be created.";
      else {
        pthread_mutex_lock(&service.ready_lock);
        while (!service.ready) pthread_cond_wait(&service.ready_changed, &service.ready_lock);
        int ready = service.ready;
        pthread_mutex_unlock(&service.ready_lock);
        if (ready == 1) service.running = 1;
        else { pthread_join(service.thread, NULL); failure = service.error; }
      }
    }
  }
  if (failure) memset(service.token, 0, sizeof service.token);
  else {
    if (port) *port = service.port;
    if (token) memcpy(token, service.token, sizeof service.token);
  }
  service.error = failure ? failure : "";
  pthread_mutex_unlock(&service.lock);
  return failure ? -1 : 0;
}

void tube_service_stop(void) {
  pthread_mutex_lock(&service.lock);
  if (service.running) {
    atomic_store(&service.engine.stop, 1);     /* also interrupts a running script */
    tube_js_http_wake(service.engine.http);
    pthread_join(service.thread, NULL);
    service.running = 0;
    memset(service.token, 0, sizeof service.token);
  }
  pthread_mutex_unlock(&service.lock);
}

int tube_service_running(void) {
  pthread_mutex_lock(&service.lock);
  int running = service.running;
  pthread_mutex_unlock(&service.lock);
  return running;
}

const char *tube_service_error(void) { return service.error; }

/* ---- command mode (host tool) ---- */
typedef struct { const tube_service_options *options; int argc; char **argv; int status; } Command;

static JSValue command_settings(JSContext *ctx, void *opaque) {
  Command *command = opaque;
  JSValue config = JS_NewObject(ctx), arguments = JS_NewArray(ctx);
  for (int i = 0; i < command->argc; i++) JS_SetPropertyUint32(ctx, arguments, (uint32_t)i, JS_NewString(ctx, command->argv[i]));
  set_string(ctx, config, "mode", "command");
  JS_SetPropertyStr(ctx, config, "argv", arguments);
  return config;
}

static void *command_thread(void *opaque) {
  Command *command = opaque;
  Engine engine = { 0 };
  const char *failure = engine_open(&engine, command->options, command_settings, command);
  if (failure) fprintf(stderr, "tube_js: %s\n", failure);
  else {
    JSContext *ctx = engine.ctx;
    JSValue global = JS_GetGlobalObject(ctx), start = JS_GetPropertyStr(ctx, global, "tubeMain");
    JSValue promise = JS_Call(ctx, start, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, start); JS_FreeValue(ctx, global);
    if (JS_IsException(promise)) report_pending(ctx);
    else {
      engine_loop(&engine, promise);
      JSValue result = JS_PromiseResult(ctx, promise);
      if (JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED) {
        const char *text = JS_ToCString(ctx, result);
        if (text) { puts(text); command->status = strncmp(text, "{\"error\"", 8) ? 0 : 1; }
        JS_FreeCString(ctx, text);
      } else if (JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED) report(ctx, result);
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, promise);
  }
  engine_close(&engine);
  return NULL;
}

int tube_service_command(const tube_service_options *options, int argc, char **argv) {
  Command command = { options, argc, argv, 1 };
  pthread_attr_t attributes; pthread_t thread;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, thread_stack(options));
  int failed = pthread_create(&thread, &attributes, command_thread, &command);
  pthread_attr_destroy(&attributes);
  if (failed) return 1;
  pthread_join(thread, NULL);
  fflush(stdout);
  return command.status;
}
