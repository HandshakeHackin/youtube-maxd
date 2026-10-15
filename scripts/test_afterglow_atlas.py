#!/usr/bin/env python3
"""Isolated real text-engine regression; no native preparation or console required."""
from pathlib import Path
import subprocess
import tempfile
import afterglow_overlay
import agc_gpu_pool_overlay
import kit_text_patch
import ui_kit

root = Path(__file__).resolve().parents[1]
kit = ui_kit.checkout()

def function(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

with tempfile.TemporaryDirectory(prefix="afterglow-atlas-") as directory:
    out = Path(directory)
    files = {}
    player = "projects/evoplayer/"
    def replace(path, old, new, count=1):
        if path not in (player + "ui_kit/src/hui_agc_batch.cpp", player + "ui_kit/include/hui_agc_batch.hpp"):
            return
        source = files.setdefault(path, (root / "third_party/evo" / path).read_text())
        assert source.count(old) == count, (path, old)
        files[path] = source.replace(old, new)
    agc_gpu_pool_overlay.apply_agc_gpu_pool_overlay(replace, player)
    afterglow_overlay.apply_atlas_update(replace, player)
    backend = files[player + "ui_kit/src/hui_agc_batch.cpp"]
    methods = "\n".join(function(backend, signature) for signature in (
        "std::uint32_t AgcBatch::create_texture", "bool AgcBatch::update_texture",
        "void AgcBatch::release_texture", "void AgcBatch::end_frame"))
    (out / "atlas_backend.hpp").write_text(r"""
#include <vector>
#include <cstdint>
static bool idle=false;
static int evo_agc_runtime_gpu_data_idle() {return idle;}
namespace evo::hui_agc {
constexpr int kFreeDelayFrames=4;
struct AgcBatch {
    struct Texture {std::vector<std::uint8_t> pixels;};
    struct Pending {Texture *texture; int frames_left;};
    bool ok_=true,fail=false;
    std::vector<Texture*> textures_;
    std::vector<Pending> pending_free_;
    Texture *make_texture(int w,int h,int,const std::uint8_t *p) {
        if(fail || w<=0 || h<=0 || !p)return nullptr;
        auto *t=new Texture;t->pixels.assign(p,p+w*h);return t;
    }
    void destroy(Texture *p) {delete p;}
    ~AgcBatch(){for(auto*p:textures_)delete p;for(auto p:pending_free_)delete p.texture;}
    std::uint32_t create_texture(int,int,const std::uint8_t*);
    bool update_texture(std::uint32_t,int,int,const std::uint8_t*);
    void release_texture(std::uint32_t);
    void end_frame();
};
""" + methods + "\n}\n")
    header, _ = kit_text_patch.patch((kit / "src/gfx/font.hpp").read_text(), (kit / "src/gfx/font.cpp").read_text())
    (out / "gfx").mkdir()
    (out / "gfx/font.hpp").write_text(header)
    # The existing pinned host dependencies are reused; all compiler output is private.
    command = r"""
set -euo pipefail
root=$1; out=$2; kit=$3
source "$root/scripts/text_common.sh"
text_engine asan "$out/obj"
"${CXX:-c++}" "${TEXT_FLAGS[@]}" -std=c++20 -I"$out" -I"$kit/src" -I"$kit" -I"$root/native" \
    "$root/tests/test_afterglow_atlas.cpp" "$root/native/afterglow/text_engine.cpp" \
    "$root/native/afterglow/platform.cpp" "${TEXT_OBJECTS[@]}" "${TEXT_LIBS[@]}" -o "$out/test"
ASAN_OPTIONS=detect_leaks=1 "$out/test" "$kit/third_party/fonts/Inter-Regular.ttf"
"""
    subprocess.run(["bash", "-c", command, "atlas-test", str(root), str(out), str(kit)], check=True)
