#!/usr/bin/env python3
"""Offline issue #1 probes. These exercise bytes, not PS5 font rendering."""
import json
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
TITLES = ["Русское видео — Ёжик", "YouTube: Привет, мир! 2026", "Съешь ещё этих мягких французских булок"]

with tempfile.TemporaryDirectory(prefix="title-unicode-") as folder:
    temp = pathlib.Path(folder)
    modules = temp / "modules"
    modules.mkdir()
    (modules / "package.json").write_text('{"type":"module"}')
    for name in ("resolve.js", "errors.js", "segments.js"):
        shutil.copyfile(ROOT / "standalone/js" / name, modules / name)
    # Use the actual resolver, with invented metadata and an HLS fixture. No network.
    js = temp / "probe.mjs"
    js.write_text('''import assert from 'node:assert/strict';
const {createPlayback} = await import(process.argv[2]);
for (const title of JSON.parse(process.argv[3])) {
  const session = {hls_manifests:new Map(), tracks:new Map()};
  const result = await createPlayback({
    info:{basic_info:{title, id:'abcdefghijk', duration:60}}, list:[], live:true,
    hls:[{video_codec:'h264', audio_codec:'aac', height:720, width:1280,
          fps:30, bitrate:1000, master:'#EXTM3U'}]
  }, {id:'abcdefghijk', max_height:1080}, '/fixture', session, 300);
  assert.equal(result.title, title);
  assert.equal(JSON.parse(JSON.stringify(result)).title, title);
}
''', encoding="utf-8")
    subprocess.run([os.environ.get("NODE", "node"), str(js),
                    (modules / "resolve.js").as_uri(), json.dumps(TITLES)], check=True)
    print("PASS resolver metadata -> playback response -> JSON preserves Cyrillic")

    # Compile the production provider's UTF-8 bounded copy and JSON accessor,
    # linked to its vendored cJSON. Extracting avoids stubbing the whole provider.
    provider = (ROOT / "native/tube_provider.c").read_text()
    start = provider.index("static void copy(char *out, size_t cap, const char *src) {")
    end = provider.index("static double num(", start)
    functions = provider[start:end]
    payloads = [json.dumps({"title": title}, ensure_ascii=escaped)
                for escaped in (False, True) for title in TITLES]
    vectors = ",\n".join("{" + json.dumps(payload, ensure_ascii=False) + "," +
                           json.dumps(title, ensure_ascii=False) + "}"
                           for payload, title in zip(payloads, TITLES * 2))
    code = '''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
''' + functions + '''
int main(void) {
  const struct {const char *json, *title;} rows[] = {
''' + vectors + '''};
  for (size_t i=0; i<sizeof rows/sizeof *rows; ++i) {
    cJSON *root=cJSON_Parse(rows[i].json); assert(root);
    char out[256]; copy(out,sizeof out,str(root,"title"));
    assert(strcmp(out,rows[i].title)==0);
    cJSON_Delete(root);
  }
  /* A byte boundary within a Cyrillic letter must truncate before it. */
  char tiny[4]; copy(tiny,sizeof tiny,"АБВ"); assert(strcmp(tiny,"А")==0);
  puts("PASS native cJSON literal/escaped Cyrillic and bounded UTF-8 copy");
}
'''
    cfile = temp / "probe.c"
    cfile.write_text(code, encoding="utf-8")
    vendor = ROOT / "native/vendor/cJSON"
    binary = temp / "probe"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I" + str(vendor),
                    str(cfile), str(vendor / "cJSON.c"), "-lm", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

    # Isolate the actual adapter's filter using a deliberately ASCII-only face.
    # This tests drawable() semantics, not the unavailable kit's glyph rasterizer.
    adapter = (ROOT / "native/afterglow/text_kit.cpp").read_text()
    decode = adapter[adapter.index("std::uint32_t next_code_point("):adapter.index("} // namespace")]
    drawable = adapter[adapter.index("std::string drawable("):adapter.index("namespace\n{\nconst Face *g_reading_face")]
    cpp = temp / "filter.cpp"
    cpp.write_text('''#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
struct Font { bool has_glyph(std::uint32_t c) const {return c>=32 && c<127;} };
struct Face { Font *font; };
''' + decode + drawable + '''
int main() {
  Font font; Face face{&font};
  assert(drawable(face,"Русское видео").empty());
  assert(drawable(face,"Title Русское video")=="Title video");
  assert(drawable(face,"Title? video")=="Title? video");
}
''', encoding="utf-8")
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    print("PASS production drawable(): unsupported Cyrillic is stripped, not changed to question marks")

# Inspect actual EVO fallback assets; report coverage, without asserting that a
# future font update must keep lacking Cyrillic. This is not the private pinned
# kit's asset set, and must not be presented as proof of a release's contents.
fonts = ROOT / "third_party/evo/projects/evoplayer/assets/hui/fonts"
for path in sorted(fonts.glob("*.huifont")):
    data = path.read_bytes()
    count = struct.unpack_from("<I", data, 32)[0]
    points = {struct.unpack_from("<I", data, 40 + 24 * i)[0] for i in range(count)}
    missing = sorted({char for title in TITLES for char in title if 0x400 <= ord(char) <= 0x4ff and ord(char) not in points})
    print(f"INFO EVO {path.name}: {count} glyphs; missing fixture Cyrillic: {''.join(missing) or 'none'}")
if not fonts.is_dir():
    print("SKIP EVO font inventory: initialize third_party/evo first")
