#!/usr/bin/env bash
# Offline PS5 build of the text engine (native/text), after the model of
# standalone/ps5/build_ps5.sh:
#   obj/*.o               the engine as PS5 objects, compiled the way EVO compiles
#                         the interface kit's C++ (and checked as C++17 and
#                         without exceptions and RTTI)
#   probe-pie.elf         text_probe linked the way package-app.sh links EVO, with
#                         the SDK's own FreeType, HarfBuzz and FriBidi (the app
#                         links the same archives already)
#   probe.elf             ... converted to a PS5 module by EVO's converter
#   app-pie.elf           EVO's own objects relinked with the engine added
#   report.txt            sizes, what each library weighs in the link, and every
#                         import the app does not have today
# Output goes to build/text-ps5 (TUBE_TEXT_PS5_OUT). Compiles and links only.
# Nothing here contacts a console or any other host.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=${TUBE_TEXT_PS5_OUT:-$root/build/text-ps5}
SDK=${PS5_PAYLOAD_SDK:-$HOME/.cache/prosperotube/sdk}
export PS5_PAYLOAD_SDK=$SDK
# A prepared and built EVO work tree; read, never written. TUBE_PS5_EVO_TREE
# names one; otherwise the first tree whose last build left every object.
evo_tree_complete() {
  local evo=$1/projects/evoplayer object
  [ -f "$evo/Makefile" ] && [ -f "$1/output/app/.build/librmlui.a" ] || return 1
  for object in $(make -C "$evo" -s AGC_DEVICE=1 print-objects 2>/dev/null); do
    [ -f "$evo/$object" ] || return 1
  done
}
EVO_TREE=${TUBE_PS5_EVO_TREE:-}
if [ -z "$EVO_TREE" ]; then
  for candidate in "$HOME"/.cache/prosperotube/native "$HOME"/.cache/prosperotube/native-*; do
    if evo_tree_complete "$candidate"; then EVO_TREE=$candidate; break; fi
  done
  EVO_TREE=${EVO_TREE:-$HOME/.cache/prosperotube/native}
fi
export PATH=/usr/lib/llvm-18/bin:$SDK/bin:$PATH
HB=$SDK/target/user/homebrew
STUBDIR=$SDK/target/lib
NATIVE=$EVO_TREE/tools/native-app
EVO=$EVO_TREE/projects/evoplayer
EVO_BUILD=$EVO_TREE/output/app/.build
for need in "$SDK/bin/prospero-clang++" "$HB/lib/libharfbuzz.a" "$HB/lib/libfreetype.a" "$HB/lib/libfribidi.a" \
            "$NATIVE/ps5-pie.ld" "$NATIVE/stubs/malloc_shim.c"; do
  [ -e "$need" ] || { echo "build_ps5: missing $need" >&2; exit 1; }
done
mkdir -p "$out/obj" "$out/tail"
report=$out/report.txt
: > "$report"
say() { echo "$*" | tee -a "$report"; }

# ---- 0. The libraries: the SDK's, and the versions the host tests pin --------
sdk_hb=$(sed -n 's/^#define HB_VERSION_STRING "\(.*\)"/\1/p' "$HB/include/harfbuzz/hb-version.h")
sdk_ft=$(awk '/#define FREETYPE_MAJOR/ {a=$3} /#define FREETYPE_MINOR/ {b=$3} /#define FREETYPE_PATCH/ {c=$3} END {print a "." b "." c}' \
  "$HB/include/freetype2/freetype/freetype.h")
sdk_fb=$(sed -n 's/^Version: //p' "$HB/lib/pkgconfig/fribidi.pc")
pinned=$(bash "$root/scripts/text_deps.sh" versions)
say "libraries of the SDK: harfbuzz $sdk_hb freetype $sdk_ft fribidi $sdk_fb"
say "pinned for the host:  $pinned"
if [ "harfbuzz $sdk_hb freetype $sdk_ft fribidi $sdk_fb" != "$pinned" ]; then
  say "note: the host tests run other versions than the console would; move the pins in scripts/text_deps.sh"
fi

# ---- 1. The engine ------------------------------------------------------------
# EVO compiles the kit's C++ with these (projects/evoplayer/Makefile CXXFLAGS and
# HUI_CXXFLAGS, scripts/package-app.sh TFLAGS and APP_DEFS; output/app/.build/
# compile.log has the line): exceptions and RTTI on, -Wall. The engine adds
# -Wextra -Werror for itself.
TFLAGS=(-fno-plt -fno-stack-protector -ffunction-sections -fdata-sections)
INCLUDES=(-I"$root/native/text" -isystem "$HB/include/freetype2" -isystem "$HB/include/harfbuzz"
          -isystem "$HB/include/fribidi" -isystem "$HB/include")
CXX_FLAGS=(-g -O2 "${TFLAGS[@]}" -DEVO_APP_MODULE=1 -DEVO_TARGET_PS5=1 -Wall -Wextra -Werror "${INCLUDES[@]}")
UNITS=(tube_text tube_text_unicode tube_text_fonts tube_text_atlas tube_text_layout)
ENGINE=()
for unit in "${UNITS[@]}"; do
  prospero-clang++ -std=c++20 "${CXX_FLAGS[@]}" -c "$root/native/text/$unit.cpp" -o "$out/obj/$unit.o"
  ENGINE+=("$out/obj/$unit.o")
  # The same source is C++17, and needs neither exceptions nor RTTI.
  prospero-clang++ -std=c++17 -fno-exceptions -fno-rtti "${CXX_FLAGS[@]}" -fsyntax-only "$root/native/text/$unit.cpp"
done
for variant in "" "_nomain"; do
  prospero-clang++ -std=c++20 "${CXX_FLAGS[@]}" ${variant:+-DTUBE_TEXT_PROBE_NO_MAIN=1} \
    -c "$here/text_probe.cpp" -o "$out/obj/text_probe$variant.o"
done
prospero-clang -g -O2 "${TFLAGS[@]}" -Wall -Wextra -Werror -c "$here/text_probe_tail.c" -o "$out/obj/text_probe_tail.o"

# ---- 2. EVO's link tail, compiled as scripts/package-app.sh section 6 does ----
prospero-clang++ -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti "${TFLAGS[@]}" \
  -c "$NATIVE/app_crt.cpp" -o "$out/tail/app_crt.o"
prospero-clang++ -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti "${TFLAGS[@]}" \
  -c "$NATIVE/app_cpp_runtime.cpp" -o "$out/tail/app_cpp_runtime.o"
rm -f "$out/tail/libpthread.a"
prospero-ar rcs "$out/tail/libpthread.a" "$out/tail/app_cpp_runtime.o"
prospero-clang -std=gnu11 -O2 -w "${TFLAGS[@]}" -c "$NATIVE/stubs/libc_ext.c" -o "$out/tail/libc_ext.o"
prospero-clang -std=gnu11 -O2 -Wall -Wextra "${TFLAGS[@]}" -c "$NATIVE/stubs/evo_dns.c" -o "$out/tail/evo_dns.o"
prospero-clang -std=gnu11 -O2 -w "${TFLAGS[@]}" -c "$NATIVE/stubs/malloc_shim.c" -o "$out/tail/malloc_shim.o"

CXX_RUNTIME=("$STUBDIR/libc++.a" "$STUBDIR/libc++abi.a" "$STUBDIR/libunwind.a")
# package-app.sh section 7: same script, same options, same tail order.
TAIL_STUBS=()   # EVO's PRX import stubs, repeated in the tail as package-app.sh does
link_like_evo() {   # output, log, then the inputs that stand where EVO's objects and archives do
  local output=$1 log=$2
  shift 2
  prospero-lld -T "$NATIVE/ps5-pie.ld" --eh-frame-hdr \
    --version-script "$NATIVE/app-symbols.map" \
    --exclude-libs=ALL --error-limit=0 \
    -L "$out/tail" -e _start -o "$output" --why-extract="$output.why" -Map="$output.map" \
    "$out/tail/malloc_shim.o" "$out/tail/app_crt.o" "$out/tail/app_cpp_runtime.o" \
    "$out/tail/libc_ext.o" "$out/tail/evo_dns.o" \
    "$@" "${CXX_RUNTIME[@]}" \
    --as-needed "$STUBDIR"/*.so ${TAIL_STUBS[@]+"${TAIL_STUBS[@]}"} \
    --start-group "$STUBDIR/libc.a" --end-group 2> "$log"
}
undefined_of() { grep -oE 'undefined symbol: .*' "$1" | sed 's/^undefined symbol: //' | sort -u; }
imports_of() { llvm-nm -u "$1" 2>/dev/null | awk '{print $NF}' | sort -u; }
# Bytes a link took from an archive (or from objects whose path has the name), in
# the sections that are loaded: code, constants, data, unwind tables. Debug
# information is in the link map too and is not counted (it is not in eboot.bin).
weight_of() {   # map file, name
  awk -v name="$2" '
    $5 ~ /^\./ && $5 !~ /:\(/ { loaded = $5 !~ /^\.(debug|comment|symtab|strtab|shstrtab)/ && $5 != ".bss" }
    loaded && $5 ~ /:\(/ && index($0, name) { total += strtonum("0x" $3) }
    END { print total + 0 }' "$1"
}

# ---- 3. Link proof: the probe alone -------------------------------------------
TEXT_ARCHIVES=(--start-group)
for a in libharfbuzz libfreetype libfribidi libpng16 libz libbz2 libm; do TEXT_ARCHIVES+=("$HB/lib/$a.a"); done
TEXT_ARCHIVES+=(--end-group)
if ! link_like_evo "$out/probe-pie.elf" "$out/probe-link.log" \
    "$out/obj/text_probe.o" "$out/obj/text_probe_tail.o" "${ENGINE[@]}" "${TEXT_ARCHIVES[@]}"; then
  echo "build_ps5: the probe did not link; undefined symbols:" >&2
  undefined_of "$out/probe-link.log" >&2
  tail -5 "$out/probe-link.log" >&2
  exit 1
fi
imports_of "$out/probe-pie.elf" > "$out/probe-imports.txt"
# The SDK's static libc.a is written for payloads; its syscalls.o issues raw
# system calls. Nothing of ours may pull that member in.
if grep -q 'libc\.a(syscalls\.o)' "$out/probe-pie.elf.why"; then
  echo "build_ps5: the probe pulls in libc.a(syscalls.o):" >&2
  grep 'libc\.a(syscalls\.o)' "$out/probe-pie.elf.why" >&2
  exit 1
fi
TOOL=$EVO_BUILD/host/ps5-native-tool
if [ -x "$TOOL" ]; then
  "$TOOL" link --in "$out/probe-pie.elf" --out "$out/probe.elf" --stub-dir "$STUBDIR" \
    --module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name eboot.elf > "$out/probe-convert.log"
  say "probe: linked $(stat -c %s "$out/probe-pie.elf") bytes, converted to a PS5 module of $(stat -c %s "$out/probe.elf") bytes"
else
  say "probe: linked $(stat -c %s "$out/probe-pie.elf") bytes; not converted (no $TOOL - build the app once)"
fi

# ---- 4. Link proof: EVO's own objects plus the engine --------------------------
app_linked=0
if evo_tree_complete "$EVO_TREE" && [ -f "$EVO_BUILD/pie-imports.txt" ]; then
  say "app: relinking the objects of $EVO_TREE"
  mapfile -t EVO_OBJECTS < <(make -C "$EVO" -s AGC_DEVICE=1 print-objects | tr ' ' '\n' | grep -E '\.o$' | sed "s|^|$EVO/|")
  mapfile -t PRX_STUBS < <(ls "$EVO_BUILD"/stubs/*.so)
  TAIL_STUBS=("${PRX_STUBS[@]}")
  EVO_ARCHIVES=(--start-group "$EVO_BUILD/librmlui.a")
  for a in libSDL2 libavfilter libavformat libavcodec libswresample libavutil libswscale libdav1d \
           libass libfreetype libharfbuzz libharfbuzz-subset libfribidi \
           libpng16 libsamplerate libssl libcrypto libiconv libxml2 libz libbz2 liblzma libzstd libm; do
    EVO_ARCHIVES+=("$HB/lib/$a.a")
  done
  EVO_ARCHIVES+=(--end-group)
  # First as the app is today, to have the same link to compare with ...
  if ! link_like_evo "$out/app-base-pie.elf" "$out/app-base-link.log" \
      "${EVO_OBJECTS[@]}" "${PRX_STUBS[@]}" "${EVO_ARCHIVES[@]}"; then
    echo "build_ps5: EVO's own objects did not relink:" >&2
    head -20 "$out/app-base-link.log" >&2
    exit 1
  fi
  # ... then with the engine and the probe (without its main) added as plain objects.
  if ! link_like_evo "$out/app-pie.elf" "$out/app-link.log" \
      "${EVO_OBJECTS[@]}" "${ENGINE[@]}" "$out/obj/text_probe_nomain.o" \
      "${PRX_STUBS[@]}" "${EVO_ARCHIVES[@]}"; then
    echo "build_ps5: EVO plus the engine did not link; undefined symbols:" >&2
    undefined_of "$out/app-link.log" >&2
    exit 1
  fi
  app_linked=1
  imports_of "$out/app-base-pie.elf" > "$out/app-base-imports.txt"
  imports_of "$out/app-pie.elf" > "$out/app-imports.txt"
  if [ -x "$TOOL" ]; then
    stub_args=()
    for so in "${PRX_STUBS[@]}"; do stub_args+=(--stub "$so"); done
    for which in app-base app; do
      "$TOOL" link --in "$out/$which-pie.elf" --out "$out/$which.elf" --stub-dir "$STUBDIR" "${stub_args[@]}" \
        --module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name eboot.elf > "$out/$which-convert.log"
    done
    base=$(stat -c %s "$out/app-base.elf")
    with=$(stat -c %s "$out/app.elf")
    say "app: eboot.elf $base bytes as it is today, $with bytes with the text engine: +$((with - base)) bytes"
  fi
else
  say "app: relink skipped ($EVO_TREE has no finished build)"
fi

# ---- 5. Report -----------------------------------------------------------------
say ""
say "the engine's objects (llvm-size: text data bss):"
total_text=0
total_data=0
for f in "${ENGINE[@]}"; do
  read -r text data bss _ < <(llvm-size "$f" | awk 'NR==2 {print $1, $2, $3}')
  say "  $(basename "$f"): $text $data $bss"
  total_text=$((total_text + text))
  total_data=$((total_data + data))
done
say "  together: text $total_text, data $total_data"
say ""
say "loaded bytes each library brings to a link (from the link maps):"
say "  library       probe alone   app today   app with the engine"
for lib in libharfbuzz.a libfreetype.a libfribidi.a libpng16.a libz.a libbz2.a; do
  alone=$(weight_of "$out/probe-pie.elf.map" "/$lib(")
  if [ "$app_linked" = 1 ]; then
    before=$(weight_of "$out/app-base-pie.elf.map" "/$lib(")
    after=$(weight_of "$out/app-pie.elf.map" "/$lib(")
  else
    before=-
    after=-
  fi
  say "$(printf '  %-13s %11s %11s %11s' "$lib" "$alone" "$before" "$after")"
done
say "$(printf '  %-13s %11s %11s %11s' "the engine" "$(weight_of "$out/probe-pie.elf.map" "$out/obj/tube_text")" "0" \
  "$([ "$app_linked" = 1 ] && weight_of "$out/app-pie.elf.map" "$out/obj/tube_text" || echo -)")"

# Which SDK stub library carries each import. An import carried only by
# libScePosixForWebKit is a null pointer at run time in a native app (EVO's
# tools/native-app/README.md, evo_dns.c) and needs a definition of its own.
index=$out/stub-index.txt
: > "$index"
for so in "$STUBDIR"/*.so; do
  llvm-nm -D --defined-only "$so" 2>/dev/null | awk -v lib="$(basename "$so" .so)" '{print $NF, lib}' >> "$index"
done
libraries_of() { awk -v s="$1" '$1 == s {printf "%s ", $2}' "$index"; }
list_new() {   # imports file, baseline file
  local bad=0 symbol libs any=0
  while read -r symbol; do
    any=1
    libs=$(libraries_of "$symbol")
    case "$libs" in
      "") say "  $symbol  (no SDK stub: unresolved weak or tail-defined)";;
      "libScePosixForWebKit ") say "  $symbol  <- ONLY libScePosixForWebKit: null at run time"; bad=1;;
      *) say "  $symbol  <- $libs";;
    esac
  done < <(comm -23 "$1" "$2")
  [ "$any" = 1 ] || say "  none"
  return $bad
}
status=0
say ""
if [ "$app_linked" = 1 ]; then
  say "imports the app gains (app-pie.elf against the same link without the engine):"
  list_new "$out/app-imports.txt" "$out/app-base-imports.txt" || status=1
else
  say "imports of the probe that EVO's last build did not have:"
  [ -f "$EVO_BUILD/pie-imports.txt" ] && { list_new "$out/probe-imports.txt" "$EVO_BUILD/pie-imports.txt" || status=1; }
fi
[ "$status" = 0 ] || { echo "build_ps5: an import would be null on the console (see above)" >&2; exit 1; }
say ""
say "ok"
