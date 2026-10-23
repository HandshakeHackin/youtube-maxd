#!/usr/bin/env python3
"""Real pinned kit font/layout regression for issue #1 (host, no PS5 claims)."""
import os
from pathlib import Path
import subprocess
import tempfile
import sys

import kit_text_patch
import playback_title_font
import ui_kit

ROOT = Path(__file__).resolve().parents[1]
kit = ui_kit.checkout()
with tempfile.TemporaryDirectory(prefix="title-font-") as directory:
    temp = Path(directory)
    output = temp / playback_title_font.NAME
    # prepare_native.py prints only the generated directory to stdout; a cold
    # font bake must keep its diagnostics on stderr so shell callers get a path.
    baked = subprocess.run([sys.executable, "-c",
        "import sys; from playback_title_font import stage; stage(*sys.argv[1:])",
        str(kit), str(output), str(temp / "cold-cache")],
        cwd=ROOT / "scripts", capture_output=True, text=True, check=True)
    assert not baked.stdout, "Font baker polluted prepare_native's path output: " + baked.stdout

    # Test the production-patched font implementation with no shaper installed:
    # exactly the Classic startup / Unicode-engine failure path.
    kit_text_patch.stage(kit, temp)
    test = temp / "test.cpp"
    test.write_text(r'''
#include "gfx/font.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using namespace hui::gfx;
static void load(Font &font,const char *path) {
  std::ifstream file(path,std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(file)),{});
  assert(font.load(data));
}
static bool same_uv(const GlyphQuad &a,const GlyphQuad &b) {
  return a.u0==b.u0 && a.v0==b.v0 && a.u1==b.u1 && a.v1==b.v1;
}
int main(int argc,char **argv) {
  assert(argc==3 && Font::shaper==nullptr);
  Font before,after; load(before,argv[1]);load(after,argv[2]);
  std::vector<GlyphQuad> q,missing,correct;
  before.layout("?",0,100,38,Align::left,q,0);
  before.layout("Русское",0,100,38,Align::left,missing,0);
  assert(missing.size()==7);
  for(const auto &quad:missing) assert(same_uv(quad,q[0]));
  after.layout("?",0,100,38,Align::left,q,0);
  // layout appends to the vector: use the last question mark for this atlas.
  const auto question=q.back();
  after.layout("Русское",0,100,38,Align::left,correct,0);
  assert(correct.size()==7);
  for(const auto &quad:correct) assert(!same_uv(quad,question));
  for(unsigned c=0x410;c<=0x44f;++c) assert(after.has_glyph(c));
  assert(after.has_glyph(0x401) && after.has_glyph(0x451));
  for(const char *title:{"Русское видео — Ёжик", "YouTube: Привет, мир! 2026", "ASCII title?"}) {
    assert(after.fit(title,38,10000)==title);
    auto fit=after.fit(title,38,120);
    assert(after.measure(fit,38)<=120.01f);
  }
  assert(before.measure("ASCII title?",38)==after.measure("ASCII title?",38));
}
''', encoding="utf-8")
    exe = temp / "test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(temp), "-I" + str(kit / "src"),
                    str(test), str(temp / "gfx/font.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe), str(kit / "assets/fonts" / playback_title_font.NAME), str(output)], check=True)
    print("PASS old display atlas draws ? for Cyrillic; rebuilt atlas draws distinct glyphs")
    print("PASS Russian alphabet, Ё/ё, mixed title, width fitting, unchanged ASCII advance")
