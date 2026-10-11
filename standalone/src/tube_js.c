// Host program: QuickJS-NG running the bundled YouTube service with native
// HTTPS, time, randomness, hashing and files. SPDX-License-Identifier: GPL-3.0-or-later
//
// The engine runs on its own thread with a large stack (the player-script
// analysis recurses deeply). Everything the JavaScript side may ask of the
// machine goes through the `__host` object defined here; nothing else of the
// operating system is exposed (no QuickJS `std`/`os` modules).
#include "quickjs.h"

#include <curl/curl.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define TUBE_STACK_BYTES ((size_t)64 << 20)
#define TUBE_BODY_LIMIT ((size_t)16 << 20)

typedef struct { uint8_t *data; size_t size, capacity, limit; int overflow; } Buffer;

static struct { unsigned requests; uint64_t bytes_in, bytes_out; double network_ms, bundle_ms; } stats;
static CURL *easy;

static double now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

static int buffer_add(Buffer *buffer, const void *data, size_t size) {
  if (buffer->size + size > buffer->limit) { buffer->overflow = 1; return -1; }
  if (buffer->size + size > buffer->capacity) {
    size_t capacity = buffer->capacity ? buffer->capacity * 2 : 65536;
    while (capacity < buffer->size + size) capacity *= 2;
    uint8_t *grown = realloc(buffer->data, capacity);
    if (!grown) return -1;
    buffer->data = grown; buffer->capacity = capacity;
  }
  memcpy(buffer->data + buffer->size, data, size);
  buffer->size += size;
  return 0;
}

// ---- SHA-1 (RFC 3174); the service only hashes short strings. ----
static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
static void sha1(const uint8_t *data, size_t size, uint8_t out[20]) {
  uint32_t h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
  uint64_t bits = (uint64_t)size * 8;
  size_t total = ((size + 8) / 64 + 1) * 64;
  for (size_t offset = 0; offset < total; offset += 64) {
    uint8_t block[64];
    for (size_t i = 0; i < 64; i++) {
      size_t at = offset + i;
      block[i] = at < size ? data[at] : at == size ? 0x80 :
        at >= total - 8 ? (uint8_t)(bits >> (8 * (total - 1 - at))) : 0;
    }
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
      w[i] = (uint32_t)block[i * 4] << 24 | (uint32_t)block[i * 4 + 1] << 16 | (uint32_t)block[i * 4 + 2] << 8 | block[i * 4 + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
      uint32_t f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
      uint32_t k = i < 20 ? 0x5A827999u : i < 40 ? 0x6ED9EBA1u : i < 60 ? 0x8F1BBCDCu : 0xCA62C1D6u;
      uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  }
  for (int i = 0; i < 20; i++) out[i] = (uint8_t)(h[i / 4] >> (8 * (3 - i % 4)));
}

// Bytes of an ArrayBuffer or typed array; NULL (no exception) for anything else.
static uint8_t *bytes_of(JSContext *ctx, JSValueConst value, size_t *size) {
  uint8_t *data = JS_GetArrayBuffer(ctx, size, value);
  if (data) return data;
  JS_FreeValue(ctx, JS_GetException(ctx));
  size_t offset = 0, length = 0;
  JSValue buffer = JS_GetTypedArrayBuffer(ctx, value, &offset, &length, NULL);
  if (JS_IsException(buffer)) { JS_FreeValue(ctx, JS_GetException(ctx)); return NULL; }
  size_t whole = 0;
  data = JS_GetArrayBuffer(ctx, &whole, buffer);
  JS_FreeValue(ctx, buffer);
  if (!data) { JS_FreeValue(ctx, JS_GetException(ctx)); return NULL; }
  *size = length;
  return data + offset;
}

static JSValue host_sha1(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  uint8_t digest[20]; char hex[41]; size_t size = 0;
  uint8_t *data = bytes_of(ctx, argv[0], &size);
  if (data) sha1(data, size, digest);
  else {
    const char *text = JS_ToCStringLen(ctx, &size, argv[0]);
    if (!text) return JS_EXCEPTION;
    sha1((const uint8_t *)text, size, digest);
    JS_FreeCString(ctx, text);
  }
  for (int i = 0; i < 20; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return JS_NewStringLen(ctx, hex, 40);
}

static JSValue host_random(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  uint32_t count = 0; uint8_t data[256];
  if (JS_ToUint32(ctx, &count, argv[0])) return JS_EXCEPTION;
  if (count > sizeof data || getentropy(data, count)) return JS_ThrowRangeError(ctx, "random bytes unavailable");
  return JS_NewArrayBufferCopy(ctx, data, count);
}

static JSValue host_now(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc; (void)argv;
  return JS_NewFloat64(ctx, now_ms());
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

static JSValue host_read_file(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  const char *path = JS_ToCString(ctx, argv[0]);
  if (!path) return JS_EXCEPTION;
  FILE *file = fopen(path, "rb");
  JS_FreeCString(ctx, path);
  if (!file) return JS_UNDEFINED;
  Buffer buffer = { .limit = TUBE_BODY_LIMIT };
  uint8_t chunk[65536]; size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, file)) > 0 && !buffer_add(&buffer, chunk, got)) {}
  fclose(file);
  JSValue result = buffer.overflow ? JS_ThrowRangeError(ctx, "file exceeds the size limit") :
    JS_NewArrayBufferCopy(ctx, buffer.data, buffer.size);
  free(buffer.data);
  return result;
}

// Writes through a temporary name so a crash never leaves a half-written entry.
static JSValue host_write_file(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  size_t size = 0; char temporary[1024]; int ok = 0;
  uint8_t *data = bytes_of(ctx, argv[1], &size);
  const char *path = JS_ToCString(ctx, argv[0]);
  if (!path) return JS_EXCEPTION;
  char directory[1024];
  snprintf(directory, sizeof directory, "%s", path);
  for (char *slash = directory + 1; (slash = strchr(slash, '/')); slash++) { *slash = 0; mkdir(directory, 0700); *slash = '/'; }
  snprintf(temporary, sizeof temporary, "%s.tmp", path);
  FILE *file = data ? fopen(temporary, "wb") : NULL;
  if (file) {
    ok = fwrite(data, 1, size, file) == size;
    ok = fclose(file) == 0 && ok && rename(temporary, path) == 0;
  }
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
  struct rusage usage; JSMemoryUsage memory;
  getrusage(RUSAGE_SELF, &usage);
  JS_ComputeMemoryUsage(JS_GetRuntime(ctx), &memory);
  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "requests", JS_NewUint32(ctx, stats.requests));
  JS_SetPropertyStr(ctx, result, "bytes_in", JS_NewFloat64(ctx, (double)stats.bytes_in));
  JS_SetPropertyStr(ctx, result, "bytes_out", JS_NewFloat64(ctx, (double)stats.bytes_out));
  JS_SetPropertyStr(ctx, result, "network_ms", JS_NewFloat64(ctx, stats.network_ms));
  JS_SetPropertyStr(ctx, result, "bundle_ms", JS_NewFloat64(ctx, stats.bundle_ms));
  JS_SetPropertyStr(ctx, result, "peak_rss_kb", JS_NewFloat64(ctx, (double)usage.ru_maxrss));
  JS_SetPropertyStr(ctx, result, "js_heap_kb", JS_NewFloat64(ctx, (double)memory.malloc_size / 1024.0));
  return result;
}

// ---- HTTPS: one blocking transfer at a time on a reused connection. ----
static size_t on_body(char *data, size_t size, size_t count, void *opaque) {
  return buffer_add(opaque, data, size * count) ? 0 : size * count;
}
typedef struct { JSContext *ctx; JSValue list; uint32_t count; } HeaderSink;
static size_t on_header(char *data, size_t size, size_t count, void *opaque) {
  HeaderSink *sink = opaque;
  size_t length = size * count;
  if (length >= 5 && !memcmp(data, "HTTP/", 5)) {  // a redirect starts a new header block
    JS_FreeValue(sink->ctx, sink->list);
    sink->list = JS_NewArray(sink->ctx); sink->count = 0;
    return length;
  }
  char *colon = memchr(data, ':', length);
  if (!colon) return length;
  char *value = colon + 1, *end = data + length;
  while (value < end && (*value == ' ' || *value == '\t')) value++;
  while (end > value && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ')) end--;
  JSValue pair = JS_NewArray(sink->ctx);
  JS_SetPropertyUint32(sink->ctx, pair, 0, JS_NewStringLen(sink->ctx, data, (size_t)(colon - data)));
  JS_SetPropertyUint32(sink->ctx, pair, 1, JS_NewStringLen(sink->ctx, value, (size_t)(end - value)));
  JS_SetPropertyUint32(sink->ctx, sink->list, sink->count++, pair);
  return length;
}

static char *string_property(JSContext *ctx, JSValueConst object, const char *name) {
  JSValue value = JS_GetPropertyStr(ctx, object, name);
  char *copy = NULL;
  if (JS_IsString(value)) {
    const char *text = JS_ToCString(ctx, value);
    if (text) { copy = strdup(text); JS_FreeCString(ctx, text); }
  }
  JS_FreeValue(ctx, value);
  return copy;
}
static double number_property(JSContext *ctx, JSValueConst object, const char *name, double fallback) {
  JSValue value = JS_GetPropertyStr(ctx, object, name);
  double number = fallback;
  if (JS_IsNumber(value)) JS_ToFloat64(ctx, &number, value);
  JS_FreeValue(ctx, value);
  return number;
}

// http({method, url, headers: [[name, value]...], body, follow, timeout_ms, max_bytes})
//   -> {status, url, headers: [[name, value]...], body: ArrayBuffer}
static JSValue host_http(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  char *method = string_property(ctx, argv[0], "method"), *url = string_property(ctx, argv[0], "url");
  if (!method || !url || strncmp(url, "https://", 8)) {
    free(method); free(url);
    return JS_ThrowTypeError(ctx, "only https requests are supported");
  }
  struct curl_slist *headers = curl_slist_append(NULL, "Expect:");
  JSValue list = JS_GetPropertyStr(ctx, argv[0], "headers");
  for (uint32_t i = 0; i < 64; i++) {
    JSValue pair = JS_GetPropertyUint32(ctx, list, i);
    if (!JS_IsObject(pair)) { JS_FreeValue(ctx, pair); break; }
    JSValue name = JS_GetPropertyUint32(ctx, pair, 0), value = JS_GetPropertyUint32(ctx, pair, 1);
    const char *n = JS_ToCString(ctx, name), *v = JS_ToCString(ctx, value);
    if (n && v && !strpbrk(n, "\r\n:") && !strpbrk(v, "\r\n")) {
      size_t size = strlen(n) + strlen(v) + 3;
      char *line = malloc(size);
      snprintf(line, size, *v ? "%s: %s" : "%s;", n, v);
      headers = curl_slist_append(headers, line);
      free(line);
    }
    JS_FreeCString(ctx, n); JS_FreeCString(ctx, v);
    JS_FreeValue(ctx, name); JS_FreeValue(ctx, value); JS_FreeValue(ctx, pair);
  }
  JS_FreeValue(ctx, list);
  JSValue body = JS_GetPropertyStr(ctx, argv[0], "body");
  size_t body_size = 0;
  uint8_t *body_data = bytes_of(ctx, body, &body_size);
  Buffer received = { .limit = (size_t)number_property(ctx, argv[0], "max_bytes", (double)TUBE_BODY_LIMIT) };
  HeaderSink sink = { ctx, JS_NewArray(ctx), 0 };
  char failure[CURL_ERROR_SIZE] = "";

  if (!easy) easy = curl_easy_init();
  curl_easy_reset(easy);
  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https");
  curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "https");
  curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");  // libcurl negotiates and decodes
  curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, (long)(number_property(ctx, argv[0], "follow", 0) != 0));
  curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, (long)number_property(ctx, argv[0], "timeout_ms", 15000));
  curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, on_body);
  curl_easy_setopt(easy, CURLOPT_WRITEDATA, &received);
  curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, on_header);
  curl_easy_setopt(easy, CURLOPT_HEADERDATA, &sink);
  curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, failure);
  if (getenv("TUBE_JS_CA_BUNDLE")) curl_easy_setopt(easy, CURLOPT_CAINFO, getenv("TUBE_JS_CA_BUNDLE"));
  if (!strcmp(method, "HEAD")) curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
  else if (body_data || strcmp(method, "GET")) {
    curl_easy_setopt(easy, CURLOPT_POSTFIELDS, body_data ? (const char *)body_data : "");
    curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body_size);
    curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method);
  }
  double started = now_ms();
  CURLcode code = curl_easy_perform(easy);
  stats.network_ms += now_ms() - started;
  stats.requests++; stats.bytes_in += received.size; stats.bytes_out += body_size;
  long status = 0; char *effective = NULL;
  curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective);

  JSValue result;
  if (code != CURLE_OK) {
    result = JS_ThrowTypeError(ctx, "fetch failed: %s", received.overflow ? "response exceeds the size limit" :
      *failure ? failure : curl_easy_strerror(code));
    JS_FreeValue(ctx, sink.list);
  } else {
    result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "status", JS_NewInt32(ctx, (int32_t)status));
    JS_SetPropertyStr(ctx, result, "url", JS_NewString(ctx, effective ? effective : url));
    JS_SetPropertyStr(ctx, result, "headers", sink.list);
    JS_SetPropertyStr(ctx, result, "body", JS_NewArrayBufferCopy(ctx, received.data, received.size));
  }
  JS_FreeValue(ctx, body);
  curl_slist_free_all(headers);
  free(received.data); free(method); free(url);
  return result;
}

// Evaluate a script in a fresh context of the same runtime and return its
// string result. The context has the language built-ins only: no `__host`,
// no network, no files. Used for functions extracted from the player script.
static JSValue host_eval_isolated(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
  (void)self; (void)argc;
  size_t size = 0;
  const char *code = JS_ToCStringLen(ctx, &size, argv[0]);
  if (!code) return JS_EXCEPTION;
  JSContext *inner = JS_NewContext(JS_GetRuntime(ctx));
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

static const JSCFunctionListEntry host_functions[] = {
  JS_CFUNC_DEF("evalIsolated", 1, host_eval_isolated),
  JS_CFUNC_DEF("http", 1, host_http),
  JS_CFUNC_DEF("sha1", 1, host_sha1),
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

static uint8_t *load(const char *path, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (!file) return NULL;
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  rewind(file);
  uint8_t *data = length >= 0 ? malloc((size_t)length + 1) : NULL;
  if (data && fread(data, 1, (size_t)length, file) != (size_t)length) { free(data); data = NULL; }
  fclose(file);
  if (data) { data[length] = 0; *size = (size_t)length; }
  return data;
}

typedef struct { int argc; char **argv; const char *bundle; int status; } Run;

static void *engine(void *opaque) {
  Run *run = opaque;
  run->status = 1;
  JSRuntime *runtime = JS_NewRuntime();
  JS_SetMaxStackSize(runtime, TUBE_STACK_BYTES - ((size_t)4 << 20));
  JS_SetMemoryLimit(runtime, (size_t)768 << 20);
  JSContext *ctx = JS_NewContext(runtime);
  JSValue global = JS_GetGlobalObject(ctx), host = JS_NewObject(ctx), arguments = JS_NewArray(ctx);
  JS_SetPropertyFunctionList(ctx, host, host_functions, sizeof host_functions / sizeof *host_functions);
  for (int i = 1; i < run->argc; i++) JS_SetPropertyUint32(ctx, arguments, (uint32_t)(i - 1), JS_NewString(ctx, run->argv[i]));
  JS_SetPropertyStr(ctx, host, "argv", arguments);
  JS_SetPropertyStr(ctx, host, "engine", JS_NewString(ctx, JS_GetVersion()));
  JS_SetPropertyStr(ctx, global, "__host", host);

  size_t size = 0;
  uint8_t *source = load(run->bundle, &size);
  if (!source) { fprintf(stderr, "tube_js: cannot read %s\n", run->bundle); goto done; }
  double started = now_ms();
  // A bundle may be shipped as source or as engine bytecode (`.jsc`).
  size_t name = strlen(run->bundle);
  JSValue loaded;
  if (name > 4 && !strcmp(run->bundle + name - 4, ".jsc")) {
    JSValue function = JS_ReadObject(ctx, source, size, JS_READ_OBJ_BYTECODE);
    loaded = JS_IsException(function) ? function : JS_EvalFunction(ctx, function);
  } else loaded = JS_Eval(ctx, (const char *)source, size, "tube_js.bundle.js", JS_EVAL_TYPE_GLOBAL);
  free(source);
  stats.bundle_ms = now_ms() - started;
  if (JS_IsException(loaded)) { JSValue error = JS_GetException(ctx); report(ctx, error); JS_FreeValue(ctx, error); goto done; }
  JS_FreeValue(ctx, loaded);

  JSValue start = JS_GetPropertyStr(ctx, global, "tubeMain");
  JSValue timers = JS_GetPropertyStr(ctx, global, "__tubeTimers");
  JSValue promise = JS_Call(ctx, start, JS_UNDEFINED, 0, NULL);
  JS_FreeValue(ctx, start);
  if (JS_IsException(promise)) { JSValue error = JS_GetException(ctx); report(ctx, error); JS_FreeValue(ctx, error); }
  // The loop: drain promise jobs, then let the JavaScript timer queue run or
  // tell how long to sleep. A blocking transfer happens inside a job.
  while (JS_IsObject(promise) && JS_PromiseState(ctx, promise) == JS_PROMISE_PENDING) {
    JSContext *job_ctx;
    int ran;
    while ((ran = JS_ExecutePendingJob(runtime, &job_ctx)) > 0) {}
    if (ran < 0) { JSValue error = JS_GetException(job_ctx); report(job_ctx, error); JS_FreeValue(job_ctx, error); }
    if (JS_PromiseState(ctx, promise) != JS_PROMISE_PENDING) break;
    JSValue wait = JS_Call(ctx, timers, JS_UNDEFINED, 0, NULL);
    double delay = -1;
    if (JS_IsException(wait)) { JSValue error = JS_GetException(ctx); report(ctx, error); JS_FreeValue(ctx, error); }
    else JS_ToFloat64(ctx, &delay, wait);
    JS_FreeValue(ctx, wait);
    if (delay < 0 && !JS_IsJobPending(runtime)) { fprintf(stderr, "tube_js: the operation stalled\n"); break; }
    if (delay > 0) usleep((useconds_t)(delay > 1000 ? 1000000 : delay * 1000));
  }
  if (JS_IsObject(promise)) {
    JSValue result = JS_PromiseResult(ctx, promise);
    if (JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED) {
      const char *text = JS_ToCString(ctx, result);
      if (text) { puts(text); run->status = strncmp(text, "{\"error\"", 8) ? 0 : 1; }
      JS_FreeCString(ctx, text);
    } else if (JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED) report(ctx, result);
    JS_FreeValue(ctx, result);
  }
  if (getenv("TUBE_JS_STATS")) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    fprintf(stderr, "{\"stats\":{\"bundle_ms\":%.0f,\"requests\":%u,\"bytes_in\":%llu,\"network_ms\":%.0f,\"peak_rss_kb\":%ld}}\n",
      stats.bundle_ms, stats.requests, (unsigned long long)stats.bytes_in, stats.network_ms, usage.ru_maxrss);
  }
  JS_FreeValue(ctx, promise); JS_FreeValue(ctx, timers);
done:
  // Leaving the process frees the engine; a long-lived host would call
  // JS_FreeContext/JS_FreeRuntime here.
  JS_FreeValue(ctx, global);
  return NULL;
}

int main(int argc, char **argv) {
  char bundle[1024];
  const char *configured = getenv("TUBE_JS_BUNDLE");
  if (configured) snprintf(bundle, sizeof bundle, "%s", configured);
  else {
    // Default: the bundle next to the executable.
    char self[900];
    ssize_t length = readlink("/proc/self/exe", self, sizeof self - 1);
    if (length < 0) length = 0;
    self[length] = 0;
    char *slash = strrchr(self, '/');
    if (slash) *slash = 0;
    snprintf(bundle, sizeof bundle, "%s/tube_js.bundle.js", slash ? self : ".");
  }
  curl_global_init(CURL_GLOBAL_DEFAULT);
  Run run = { argc, argv, bundle, 1 };
  pthread_attr_t attributes; pthread_t thread;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, TUBE_STACK_BYTES);
  if (pthread_create(&thread, &attributes, engine, &run)) return 1;
  pthread_join(thread, NULL);
  fflush(stdout);
  return run.status;
}
