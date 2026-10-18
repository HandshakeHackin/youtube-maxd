"""Exclude desktop debugger process creation from the PS5 GGML build."""


def apply(source):
    c = source / "ggml/src/ggml.c"
    text = c.read_text()
    old = "#if defined(__linux__) || \\\n    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \\\n    (defined(__APPLE__) && !TARGET_OS_TV && !TARGET_OS_WATCH)"
    new = "#if !defined(EVO_TARGET_PS5) && (defined(__linux__) || \\\n    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \\\n    (defined(__APPLE__) && !TARGET_OS_TV && !TARGET_OS_WATCH))"
    if new not in text:
        assert text.count(old) == 1, "GGML backtrace guard changed"
        c.write_text(text.replace(old, new))
    cpp = source / "ggml/src/ggml.cpp"
    text = cpp.read_text()
    marker = "/* Max'd: desktop terminate/backtrace hooks are omitted on PS5. */"
    if marker not in text:
        start = text.index("static std::terminate_handler previous_terminate_handler;")
        text = text[:start] + marker + "\n#if !defined(EVO_TARGET_PS5)\n" + text[start:] + "\n#endif\n"
        cpp.write_text(text)
    registry = source / "ggml/src/ggml-backend-reg.cpp"
    text = registry.read_text()
    marker = "/* Max'd: the console uses only the statically registered CPU backend. */"
    if marker not in text:
        start = text.index("// Dynamic loading\n")
        replacement = marker + '''
#if defined(EVO_TARGET_PS5)
ggml_backend_reg_t ggml_backend_load(const char *) { return nullptr; }
void ggml_backend_unload(ggml_backend_reg_t) {}
void ggml_backend_load_all() {}
void ggml_backend_load_all_from_path(const char *) {}
#else
'''
        registry.write_text(text[:start] + replacement + text[start:] + "\n#endif\n")
