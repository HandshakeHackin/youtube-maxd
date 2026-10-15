"""Compile the generated application notice hook used by Classic and Afterglow."""
import os
from pathlib import Path
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home()/".cache/prosperotube/native"))
source = (work/"projects/evoplayer/core/src/Application.cpp").read_text()
needle = '        if (!m_softClose) {\n            if (const char* notice = tube_take_storage_notice()) toast("LOCAL DATA", notice);\n        }'
assert source.count(needle) == 1
program = r'''#include <cassert>
#include <cstring>
static int taken, shown;
static const char *pending;
static const char *tube_take_storage_notice() { ++taken; const char *value = pending; pending = nullptr; return value; }
static void toast(const char *title, const char *text) { assert(!strcmp(title,"LOCAL DATA") && !strcmp(text,"save failed")); ++shown; }
struct App { bool m_softClose; void poll() {
''' + needle + r'''
} };
int main() {
    App app{true}; pending="save failed"; app.poll(); assert(!taken && !shown);
    app.m_softClose=false; app.poll(); assert(taken==1 && shown==1);
    app.poll(); assert(taken==2 && shown==1);
}
'''
with tempfile.TemporaryDirectory(prefix="tube-storage-notice-") as directory:
    path = Path(directory)
    (path/"test.cpp").write_text(program)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined", str(path/"test.cpp"), "-o", str(path/"test")], check=True)
    subprocess.run([str(path/"test")], check=True)
print("PASS: generated shared storage toast, once-only consumption and soft-close suppression")
