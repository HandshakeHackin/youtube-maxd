/* Link proof for the embedded JavaScript engine: a runtime with the limits the
 * app would set, a script compiled to bytecode, written out, read back and
 * run, a promise settled through the job queue, and one call into each native
 * binding so they link too. Built for the PS5 (never run there by this
 * repository's tools) and for the host, where test_bindings.sh runs it.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "quickjs.h"
#include "tube_js_http.h"
#include "tube_js_platform.h"

#include <stdio.h>
#include <string.h>

static const char probe_script[] =
    "globalThis.answer = 0;\n"
    "(async () => {\n"
    "  const parts = await Promise.all([1, 2, 3].map(async (n) => n * 7));\n"
    "  const text = JSON.stringify({ sum: parts.reduce((a, b) => a + b, 0), re: /(\\d+)-(\\d+)/.exec('x 12-34')[2],\n"
    "    big: (2n ** 70n).toString(), date: new Date(0).toISOString(), map: [...new Map([['k', 1]]).keys()] });\n"
    "  globalThis.answer = text;\n"
    "})();\n";

/* 0 on success; `output` then holds the script's answer. Otherwise the step
 * that failed (negative) and what the engine said. */
int tube_js_link_probe(char *output, size_t capacity) {
    int result = -1;
    JSRuntime *runtime = JS_NewRuntime();
    if (!runtime) return result;
    JS_SetMemoryLimit(runtime, (size_t)128 << 20);
    JS_SetMaxStackSize(runtime, (size_t)3 << 20);
    JS_UpdateStackTop(runtime);
    JSContext *context = JS_NewContext(runtime);
    if (!context) {
        JS_FreeRuntime(runtime);
        return result;
    }
    JSValue function = JS_Eval(context, probe_script, sizeof probe_script - 1, "probe.js",
                               JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    result = -2;
    if (!JS_IsException(function)) {
        size_t size = 0;
        uint8_t *bytecode = JS_WriteObject(context, &size, function, JS_WRITE_OBJ_BYTECODE);
        JS_FreeValue(context, function);
        result = -3;
        if (bytecode) {
            JSValue loaded = JS_ReadObject(context, bytecode, size, JS_READ_OBJ_BYTECODE);
            js_free(context, bytecode);
            result = -4;
            if (!JS_IsException(loaded)) {
                JSValue value = JS_EvalFunction(context, loaded);
                result = -5;
                if (!JS_IsException(value)) {
                    JSContext *job_context = NULL;
                    int jobs;
                    while ((jobs = JS_ExecutePendingJob(runtime, &job_context)) > 0) continue;
                    result = jobs < 0 ? -6 : 0;
                }
                JS_FreeValue(context, value);
            }
        }
    }
    if (result == 0) {
        JSValue global = JS_GetGlobalObject(context);
        JSValue answer = JS_GetPropertyStr(context, global, "answer");
        const char *text = JS_ToCString(context, answer);
        if (text) snprintf(output, capacity, "%s", text);
        else result = -7;
        JS_FreeCString(context, text);
        JS_FreeValue(context, answer);
        JS_FreeValue(context, global);
    } else {
        JSValue exception = JS_GetException(context);
        const char *text = JS_ToCString(context, exception);
        snprintf(output, capacity, "%s", text ? text : "no message");
        JS_FreeCString(context, text);
        JS_FreeValue(context, exception);
    }
    JS_FreeContext(context);
    JS_FreeRuntime(runtime);
    return result;
}

/* One call into each binding. Starts no request: nothing here touches a network. */
int tube_js_link_probe_bindings(char *output, size_t capacity) {
    unsigned char random[16], digest[32];
    if (tube_js_random_bytes(random, sizeof random)) return -1;
    tube_js_sha256_digest("abc", 3, digest);
    tube_js_sha1_digest("abc", 3, digest);
    tube_js_timers *timers = tube_js_timers_new();
    if (!timers) return -2;
    uint64_t now = tube_js_monotonic_ms();
    uint32_t timer = tube_js_timers_add(timers, now, 0, 0);
    int due = tube_js_timers_pop_due(timers, now) == timer;
    tube_js_timers_free(timers);
    tube_js_http *client = tube_js_http_new(NULL);
    if (!client) return -3;
    int waiting = tube_js_http_poll(client, 0);
    (void)tube_js_http_wake(client);
    tube_js_http_free(client);
    snprintf(output, capacity, "%s; timer %s; wall %lld", tube_js_http_backend(), due ? "due" : "late",
             (long long)tube_js_wall_ms());
    return due && !waiting ? 0 : -4;
}

#ifndef TUBE_JS_PROBE_NO_MAIN
int main(void) {
    char text[512];
    int engine = tube_js_link_probe(text, sizeof text);
    printf("engine %s: %d %s\n", JS_GetVersion(), engine, text);
    int bindings = tube_js_link_probe_bindings(text, sizeof text);
    printf("bindings: %d %s\n", bindings, text);
    return engine || bindings;
}
#endif
