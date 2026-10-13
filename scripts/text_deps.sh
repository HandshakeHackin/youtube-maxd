#!/usr/bin/env bash
# The text engine's three libraries for the host tests: fetch the pinned source
# releases into the build cache (never committed), build static libraries and
# print the directory that holds them. The versions are the ones the PS5 SDK's
# Pacbrew set carries (and the app already links), so the host shapes, orders
# and rasterises with the code the console runs:
#   HarfBuzz 9.0.0 (MIT), FreeType 2.13.2 (FTL), GNU FriBidi 1.0.15 (LGPL-2.1+)
# usage: text_deps.sh [plain|asan]     (default plain: -O2; asan: -O1 -g with
#                                       AddressSanitizer in the libraries too)
# The directory has include.flags and lib.flags, one compiler argument a line.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
HB_VERSION=9.0.0
HB_SHA256=a41b272ceeb920c57263ec851604542d9ec85ee3030506d94662067c7b6ab89e
HB_URL=https://github.com/harfbuzz/harfbuzz/releases/download/$HB_VERSION/harfbuzz-$HB_VERSION.tar.xz
FT_VERSION=2.13.2
FT_SHA256=12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d
FT_URL=https://download.savannah.gnu.org/releases/freetype/freetype-$FT_VERSION.tar.xz
FB_VERSION=1.0.15
FB_SHA256=0bbc7ff633bfa208ae32d7e369cf5a7d20d5d2557a0b067c9aa98bcbf9967587
FB_URL=https://github.com/fribidi/fribidi/releases/download/v$FB_VERSION/fribidi-$FB_VERSION.tar.xz

flavour=${1:-plain}
case "$flavour" in
  plain) opt=(-O2) ;;
  asan) opt=(-O1 -g -fno-omit-frame-pointer -fsanitize=address) ;;
  versions) echo "harfbuzz $HB_VERSION freetype $FT_VERSION fribidi $FB_VERSION"; exit 0 ;;
  *) echo "text_deps: unknown flavour $flavour" >&2; exit 2 ;;
esac
cache=${TUBE_TEXT_CACHE:-$HOME/.cache/prosperotube/text-deps}
cc=${CC:-cc}
cxx=${CXX:-c++}
mkdir -p "$cache/dl" "$cache/src"

# One build at a time: worktrees share the cache.
exec 8>"$cache/.lock"
flock 8

fetch() {   # url, file name, sha256, directory inside the archive
  local url=$1 file=$cache/dl/$2 sha=$3 dir=$cache/src/$4
  if [ ! -f "$file" ]; then
    curl -fsSL -o "$file.part" "$url"
    mv "$file.part" "$file"
  fi
  echo "$sha  $file" | sha256sum -c - >&2
  if [ ! -f "$dir/.unpacked-$sha" ]; then
    rm -rf "$dir"
    tar -xJf "$file" -C "$cache/src"
    [ -d "$dir" ] || { echo "text_deps: $4 missing from the archive" >&2; exit 1; }
    : > "$dir/.unpacked-$sha"
  fi
}
fetch "$HB_URL" "harfbuzz-$HB_VERSION.tar.xz" "$HB_SHA256" "harfbuzz-$HB_VERSION"
fetch "$FT_URL" "freetype-$FT_VERSION.tar.xz" "$FT_SHA256" "freetype-$FT_VERSION"
fetch "$FB_URL" "fribidi-$FB_VERSION.tar.xz" "$FB_SHA256" "fribidi-$FB_VERSION"
hb=$cache/src/harfbuzz-$HB_VERSION
ft=$cache/src/freetype-$FT_VERSION
fb=$cache/src/fribidi-$FB_VERSION

# The build is named after everything that decides its content.
key=$(printf '%s\n' "$HB_SHA256" "$FT_SHA256" "$FB_SHA256" "$flavour" "${opt[*]}" \
  "$("$cc" --version | head -1)" "$("$cxx" --version | head -1)" "recipe 3" | sha256sum | cut -c1-16)
out=$cache/host-$flavour-$key
if [ -f "$out/.built" ]; then
  echo "$out"
  exit 0
fi
rm -rf "$out"
mkdir -p "$out/obj/ft" "$out/obj/fb" "$out/cfg/freetype/config" "$out/cfg/fribidi"

# ---- FreeType: the TrueType and CFF drivers and the anti-aliased rasteriser.
# No hinter, no bitmap formats, no PNG, bzip2 or Brotli: the engine loads
# outlines unhinted and draws them itself.
cat > "$out/cfg/freetype/config/ftmodule.h" <<'MODULES'
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
MODULES
ft_sources=(base/ftsystem.c base/ftinit.c base/ftdebug.c base/ftbase.c base/ftbbox.c base/ftbitmap.c
            base/ftglyph.c base/ftmm.c base/ftsynth.c truetype/truetype.c cff/cff.c sfnt/sfnt.c
            psnames/psnames.c pshinter/pshinter.c psaux/psaux.c smooth/smooth.c gzip/ftgzip.c)
(
  for source in "${ft_sources[@]}"; do
    "$cc" "${opt[@]}" -w -DFT2_BUILD_LIBRARY -I"$out/cfg" -I"$ft/include" \
      -c "$ft/src/$source" -o "$out/obj/ft/$(basename "$source" .c).o"
  done
  ar rcs "$out/libfreetype.a" "$out/obj/ft"/*.o
) &
ft_job=$!

# ---- FriBidi: the release carries its generated tables; only the version
# header is made here, as its build does.
sed -e 's/@PACKAGE@/fribidi/' -e 's/@PACKAGE_NAME@/GNU FriBidi/' \
    -e 's|@PACKAGE_BUGREPORT@|https://github.com/fribidi/fribidi/issues/new|' \
    -e "s/@FRIBIDI_VERSION@/$FB_VERSION/" \
    -e "s/@FRIBIDI_MAJOR_VERSION@/${FB_VERSION%%.*}/" \
    -e "s/@FRIBIDI_MINOR_VERSION@/$(echo "$FB_VERSION" | cut -d. -f2)/" \
    -e "s/@FRIBIDI_MICRO_VERSION@/$(echo "$FB_VERSION" | cut -d. -f3)/" \
    -e 's/@FRIBIDI_INTERFACE_VERSION@/4/' -e 's/@SIZEOF_INT@/4/' \
    -e 's/@FRIBIDI_MSVC_BUILD_PLACEHOLDER@//' \
    "$fb/lib/fribidi-config.h.in" > "$out/cfg/fribidi/fribidi-config.h"
if grep -q '@[A-Z_]*@' "$out/cfg/fribidi/fribidi-config.h"; then
  echo "text_deps: fribidi-config.h.in has a placeholder this script does not know" >&2
  exit 1
fi
for header in "$fb"/lib/fribidi*.h; do
  cp "$header" "$out/cfg/fribidi/"
done
fb_defs=(-DFRIBIDI_LIB_STATIC -DFRIBIDI_BUILD -DHAVE_STRING_H=1 -DHAVE_STRINGS_H=1 -DHAVE_STDLIB_H=1
         -DHAVE_MEMSET=1 -DHAVE_MEMMOVE=1 -DHAVE_STRINGIZE=1 -DSTDC_HEADERS=1)
for source in fribidi fribidi-arabic fribidi-bidi fribidi-bidi-types fribidi-brackets fribidi-deprecated \
              fribidi-joining fribidi-joining-types fribidi-mirroring fribidi-run fribidi-shape; do
  "$cc" "${opt[@]}" -w "${fb_defs[@]}" -I"$out/cfg/fribidi" -I"$fb/lib" \
    -c "$fb/lib/$source.c" -o "$out/obj/fb/$source.o"
done
ar rcs "$out/libfribidi.a" "$out/obj/fb"/*.o

# ---- HarfBuzz: its one-file build, with its own OpenType font functions and
# Unicode tables (no FreeType, GLib, ICU or Graphite binding).
"$cxx" "${opt[@]}" -w -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -DHAVE_PTHREAD=1 \
  -I"$hb/src" -c "$hb/src/harfbuzz.cc" -o "$out/obj/harfbuzz.o"
ar rcs "$out/libharfbuzz.a" "$out/obj/harfbuzz.o"
wait "$ft_job"

printf '%s\n' "-I$out/cfg" "-I$out/cfg/fribidi" "-I$ft/include" "-I$hb/src" "-DFRIBIDI_LIB_STATIC" > "$out/include.flags"
printf '%s\n' "$out/libharfbuzz.a" "$out/libfreetype.a" "$out/libfribidi.a" "-lpthread" "-lm" > "$out/lib.flags"
cp "$hb/COPYING" "$out/COPYING.harfbuzz"
cp "$ft/docs/FTL.TXT" "$out/COPYING.freetype"
cp "$fb/COPYING" "$out/COPYING.fribidi"
: > "$out/.built"
echo "$out"
