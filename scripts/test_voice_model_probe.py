#!/usr/bin/env python3
"""Exercise production model preflight with readable files but denied stat."""
import os
from pathlib import Path
import subprocess
import tempfile

from bootstrap_local_voice import MODEL_SIZE

root = Path(__file__).resolve().parents[1]
source = (root / "native/maxd/local_asr.cpp").read_text()
start = source.index('extern "C" int maxd_asr_available(')
end = source.index('extern "C" int maxd_asr_start(', start)
with tempfile.TemporaryDirectory(prefix="voice-probe-") as directory:
    temp = Path(directory)
    for name, size in (("model", MODEL_SIZE), ("short", MODEL_SIZE - 1),
                       ("long", MODEL_SIZE + 1), ("empty", 0)):
        with (temp / name).open("wb") as file:
            file.truncate(size)
    test = temp / "probe.cpp"
    test.write_text('''#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>
static int stat_calls;
static char model_failure[160];
static bool deny_seek, deny_tell;
static int probe_seek(FILE *file, long offset, int origin) {
  if (deny_seek) { errno=EIO; return -1; }
  return std::fseek(file,offset,origin);
}
static long probe_tell(FILE *file) {
  if (deny_tell) { errno=EIO; return -1; }
  return std::ftell(file);
}
/* Simulate the launcher metadata restriction independently of readable data. */
extern "C" int stat(const char *, struct stat *) noexcept {
  ++stat_calls; errno=EPERM; return -1;
}
#define MAXD_VOICE_MODEL_SIZE ''' + str(MODEL_SIZE) + '''u
''' + source[start:end].replace("std::fseek(", "probe_seek(").replace("std::ftell(", "probe_tell(") + '''
int main(int argc, char **argv) {
  assert(argc==2); const std::string root=argv[1];
  assert(!maxd_asr_available(nullptr));
  assert(strstr(model_failure,"path is invalid"));
  assert(!maxd_asr_available(""));
  assert(!maxd_asr_available(std::string(512,'x').c_str()));
  for (const char *name:{"short","long","empty","missing"})
    assert(!maxd_asr_available((root+"/"+name).c_str()));
  assert(strstr(model_failure,"cannot be opened (error"));
  assert(!maxd_asr_available((root+"/short").c_str()));
  assert(strstr(model_failure,"bytes; expected"));
  assert(!maxd_asr_available(root.c_str()));
  const auto path=root+"/model";
  deny_seek=true;
  assert(!maxd_asr_available(path.c_str()));
  assert(strstr(model_failure,"cannot be seeked (error"));
  deny_seek=false; deny_tell=true;
  assert(!maxd_asr_available(path.c_str()));
  assert(strstr(model_failure,"size cannot be read (error"));
  deny_tell=false;
  for(int i=0;i<100;++i) assert(maxd_asr_available(path.c_str()));
  assert(!*model_failure);
  assert(stat_calls==0);
  assert(std::remove(path.c_str())==0);
  assert(!maxd_asr_available(path.c_str()));
}
''')
    exe = temp / "probe"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(exe)], check=True)
    subprocess.run([str(exe), str(temp)], check=True)
print("PASS readable model with denied metadata; missing/directory/empty/short/long models rejected")
