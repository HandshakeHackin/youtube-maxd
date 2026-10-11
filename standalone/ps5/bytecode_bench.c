/* Host measurement for the start-up question: how long the engine takes to
 * compile a JavaScript bundle from source, how large its bytecode is, and how
 * long reading that bytecode back takes. Compiles only; the bundle is not run.
 *   bytecode_bench <bundle.js> [rounds]
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "quickjs.h"
#include "tube_js_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    int rounds = argc > 2 ? atoi(argv[2]) : 5;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 2;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *source = malloc((size_t)size + 1);
    if (!source || fread(source, 1, (size_t)size, file) != (size_t)size) return 2;
    source[size] = 0;
    fclose(file);
    int module = JS_DetectModule(source, (size_t)size);
    int flags = (module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL) | JS_EVAL_FLAG_COMPILE_ONLY;
    double compile_best = 1e18, read_best = 1e18;
    size_t bytecode_size = 0, heap_after_read = 0;
    for (int round = 0; round < rounds; round++) {
        JSRuntime *runtime = JS_NewRuntime();
        JSContext *context = JS_NewContext(runtime);
        uint64_t start = tube_js_monotonic_ns();
        JSValue function = JS_Eval(context, source, (size_t)size, "bundle.js", flags);
        double compile_ms = (double)(tube_js_monotonic_ns() - start) / 1e6;
        if (JS_IsException(function)) {
            JSValue exception = JS_GetException(context);
            const char *text = JS_ToCString(context, exception);
            fprintf(stderr, "compile failed: %s\n", text ? text : "?");
            return 1;
        }
        uint8_t *bytecode = JS_WriteObject(context, &bytecode_size, function, JS_WRITE_OBJ_BYTECODE);
        JS_FreeValue(context, function);
        if (!bytecode) return 1;
        uint8_t *copy = malloc(bytecode_size);
        if (!copy) return 1;
        memcpy(copy, bytecode, bytecode_size);
        js_free(context, bytecode);
        JS_FreeContext(context);
        JS_FreeRuntime(runtime);   /* the runtime that compiled it is gone before it is read */

        runtime = JS_NewRuntime();
        context = JS_NewContext(runtime);
        start = tube_js_monotonic_ns();
        JSValue loaded = JS_ReadObject(context, copy, bytecode_size, JS_READ_OBJ_BYTECODE);
        double read_ms = (double)(tube_js_monotonic_ns() - start) / 1e6;
        if (JS_IsException(loaded)) return 1;
        JSMemoryUsage usage;
        JS_ComputeMemoryUsage(runtime, &usage);
        heap_after_read = (size_t)usage.malloc_size;
        JS_FreeValue(context, loaded);
        JS_FreeContext(context);
        JS_FreeRuntime(runtime);
        free(copy);
        if (compile_ms < compile_best) compile_best = compile_ms;
        if (read_ms < read_best) read_best = read_ms;
    }
    printf("%s: %s, source %ld bytes, compile %.1f ms, bytecode %zu bytes, read %.1f ms, heap after read %zu bytes (QuickJS %s, best of %d)\n",
           argv[1], module ? "module" : "script", size, compile_best, bytecode_size, read_best, heap_after_read,
           JS_GetVersion(), rounds);
    free(source);
    return 0;
}
