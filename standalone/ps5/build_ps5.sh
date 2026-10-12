#!/usr/bin/env bash
# Offline PS5 build of the embedded JavaScript engine and its native bindings:
#   build/libquickjs.a        QuickJS-NG, compiled with EVO's C flags
#   build/obj/*.o             the bindings as PS5 objects
#   build/tube_js_net.o       tube_js_http + console shims + libcurl, one object
#   build/probe-pie.elf       link_probe linked the way package-app.sh links EVO
#   build/probe.elf           ... and converted to a PS5 module by EVO's converter
#   build/obj/tube_js_data.o  the pinned root certificates (and the service
#                             bundle, when TUBE_JS_BUNDLE names one) as C arrays
#   build/app-pie.elf         EVO's own objects relinked with all of the above
#   build/report.txt          sizes and every import the app does not have today
# Compiles and links only. Nothing here contacts a console or any other host.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out=$here/build
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
for need in "$SDK/bin/prospero-clang" "$HB/lib/libcurl.a" "$NATIVE/ps5-pie.ld" "$NATIVE/stubs/malloc_shim.c"; do
  [ -e "$need" ] || { echo "build_ps5: missing $need" >&2; exit 1; }
done
mkdir -p "$out/obj" "$out/tail"
report=$out/report.txt
: > "$report"
say() { echo "$*" | tee -a "$report"; }

# EVO compiles its C sources with these (projects/evoplayer/Makefile CFLAGS plus
# scripts/package-app.sh TFLAGS and APP_DEFS); warnings are EVO's -Wall.
TFLAGS=(-fno-plt -fno-stack-protector -ffunction-sections -fdata-sections)
EVO_CFLAGS=(-g -O2 "${TFLAGS[@]}" -DEVO_APP_MODULE=1 -DEVO_TARGET_PS5=1 -I"$HB/include")

# ---- 1. Engine --------------------------------------------------------------
qjs=$("$here/fetch_quickjs.sh")
# No source patch and no -D: the amalgamation builds as it is. -w only silences
# the third-party code's warnings under EVO's -Wall.
prospero-clang "${EVO_CFLAGS[@]}" -w -c "$qjs/quickjs-amalgam.c" -o "$out/obj/quickjs.o"
rm -f "$out/libquickjs.a"
prospero-ar rcs "$out/libquickjs.a" "$out/obj/quickjs.o"

# ---- 2. Bindings ------------------------------------------------------------
for unit in tube_js_platform tube_js_http tube_js_console tube_js_libc; do
  prospero-clang "${EVO_CFLAGS[@]}" -Wall -Wextra -Werror -DCURL_STATICLIB -I"$here" \
    -c "$here/$unit.c" -o "$out/obj/$unit.o"
done
prospero-clang "${EVO_CFLAGS[@]}" -Wall -Wextra -Werror -c "$here/link_probe_tail.c" -o "$out/obj/link_probe_tail.o"
for variant in "" "_nomain"; do
  prospero-clang "${EVO_CFLAGS[@]}" -Wall -Wextra -Werror -I"$here" -I"$qjs" \
    ${variant:+-DTUBE_JS_PROBE_NO_MAIN=1} -c "$here/link_probe.c" -o "$out/obj/link_probe$variant.o"
done

# The data the app would carry inside eboot.bin: the pinned root certificates,
# and the service bundle when TUBE_JS_BUNDLE names one (it is built elsewhere).
ca=$("$here/fetch_cacert.sh")
embed=(tube_js_ca_pem="$ca")
data_bytes=$(stat -c %s "$ca")
data_what="the root certificates ($data_bytes bytes)"
if [ -n "${TUBE_JS_BUNDLE:-}" ]; then
  [ -f "$TUBE_JS_BUNDLE" ] || { echo "build_ps5: TUBE_JS_BUNDLE is not a file" >&2; exit 1; }
  embed+=(tube_js_bundle="$TUBE_JS_BUNDLE")
  data_bytes=$((data_bytes + $(stat -c %s "$TUBE_JS_BUNDLE")))
  data_what="$data_what and the service bundle ($(stat -c %s "$TUBE_JS_BUNDLE") bytes)"
fi
python3 "$here/embed_data.py" "$out/tube_js_data.c" "${embed[@]}"
prospero-clang "${EVO_CFLAGS[@]}" -Wall -Wextra -Werror -c "$out/tube_js_data.c" -o "$out/obj/tube_js_data.o"

# ---- 3. The network object --------------------------------------------------
# tube_js_http + the console shims + the members of libcurl (and libpsl) they
# need, joined into one relocatable object. --wrap=fcntl is resolved here, so
# only libcurl's fcntl calls go through the socket stand-in; the final link of
# the app needs no --wrap and FFmpeg, OpenSSL and EVO keep their own fcntl.
prospero-lld -r --wrap=fcntl -o "$out/tube_js_net.o" \
  "$out/obj/tube_js_http.o" "$out/obj/tube_js_console.o" "$HB/lib/libcurl.a" "$HB/lib/libpsl.a"
if llvm-nm -u "$out/tube_js_net.o" | grep -qE ' (__wrap_fcntl|__real_fcntl)$'; then
  echo "build_ps5: the fcntl wrap did not resolve inside tube_js_net.o" >&2; exit 1
fi
# Only the bindings' own interface stays visible: libcurl and the libc
# stand-ins become private to this object and cannot collide with the app.
# ZSTD_trace_* stay visible too: libzstd.a, which the app links itself, refers
# to them weakly and the converter refuses an import left undefined.
prospero-objcopy --wildcard --keep-global-symbol='tube_js_*' --keep-global-symbol='ZSTD_trace_*' "$out/tube_js_net.o"
if llvm-nm --defined-only --extern-only "$out/tube_js_net.o" | awk '{print $NF}' | grep -vqE '^(tube_js_|ZSTD_trace_)'; then
  echo "build_ps5: tube_js_net.o exports more than its own interface" >&2; exit 1
fi

# ---- 4. EVO's link tail, compiled as scripts/package-app.sh section 6 does ----
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
    -L "$out/tail" -e _start -o "$output" --why-extract="$output.why" \
    "$out/tail/malloc_shim.o" "$out/tail/app_crt.o" "$out/tail/app_cpp_runtime.o" \
    "$out/tail/libc_ext.o" "$out/tail/evo_dns.o" \
    "$@" "${CXX_RUNTIME[@]}" \
    --as-needed "$STUBDIR"/*.so ${TAIL_STUBS[@]+"${TAIL_STUBS[@]}"} \
    --start-group "$STUBDIR/libc.a" --end-group 2> "$log"
}
undefined_of() { grep -oE 'undefined symbol: .*' "$1" | sed 's/^undefined symbol: //' | sort -u; }
imports_of() { llvm-nm -u "$1" 2>/dev/null | awk '{print $NF}' | sort -u; }

# ---- 5. Link proof: the probe alone -----------------------------------------
PROBE_ARCHIVES=(--start-group "$out/libquickjs.a")
for a in libssl libcrypto libz libzstd libm; do PROBE_ARCHIVES+=("$HB/lib/$a.a"); done
PROBE_ARCHIVES+=(--end-group)
if ! link_like_evo "$out/probe-pie.elf" "$out/probe-link.log" \
    "$out/obj/link_probe.o" "$out/obj/link_probe_tail.o" "$out/obj/tube_js_platform.o" "$out/obj/tube_js_libc.o" "$out/tube_js_net.o" \
    "${PROBE_ARCHIVES[@]}"; then
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

# The converter EVO's build made (tools/native-app sources); it turns the LLVM
# PIE into the PS5 module that gets signed into eboot.bin, and refuses an import
# no SDK stub library carries.
TOOL=$EVO_BUILD/host/ps5-native-tool
if [ -x "$TOOL" ]; then
  "$TOOL" link --in "$out/probe-pie.elf" --out "$out/probe.elf" --stub-dir "$STUBDIR" \
    --module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name eboot.elf > "$out/probe-convert.log"
  say "probe: linked $(stat -c %s "$out/probe-pie.elf") bytes, converted to a PS5 module of $(stat -c %s "$out/probe.elf") bytes"
else
  say "probe: linked $(stat -c %s "$out/probe-pie.elf") bytes; not converted (no $TOOL - build the app once)"
fi

# ---- 6. Link proof: EVO's own objects plus the engine ------------------------
# The object list comes from EVO's Makefile, the PRX stubs and the patched
# RmlUi archive from EVO's last build. Skipped when the tree was never built.
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
  # ... then with the engine, the bindings and libcurl added as plain objects.
  if ! link_like_evo "$out/app-pie.elf" "$out/app-link.log" \
      "${EVO_OBJECTS[@]}" "$out/obj/quickjs.o" "$out/obj/link_probe_nomain.o" \
      "$out/obj/tube_js_platform.o" "$out/obj/tube_js_libc.o" "$out/tube_js_net.o" "$out/obj/tube_js_data.o" \
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
    say "app: eboot.elf $base bytes as it is today, $with bytes with the engine, bindings, libcurl and data: +$((with - base)) bytes"
    say "     of which data: $data_what; code: about $((with - base - data_bytes)) bytes"
  fi
else
  say "app: relink skipped ($EVO_TREE has no finished build)"
fi

# ---- 7. Report --------------------------------------------------------------
say ""
say "sizes (llvm-size: text data bss):"
for f in "$out/obj/quickjs.o" "$out/tube_js_net.o" "$out/obj/tube_js_platform.o"; do
  say "  $(basename "$f"): $(llvm-size "$f" | awk 'NR==2 {print $1, $2, $3}')"
done

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
  local bad=0 symbol libs
  while read -r symbol; do
    libs=$(libraries_of "$symbol")
    case "$libs" in
      "") say "  $symbol  (no SDK stub: unresolved weak or tail-defined)";;
      "libScePosixForWebKit ") say "  $symbol  <- ONLY libScePosixForWebKit: null at run time"; bad=1;;
      *) say "  $symbol  <- $libs";;
    esac
  done < <(comm -23 "$1" "$2")
  return $bad
}
status=0
say ""
if [ "$app_linked" = 1 ]; then
  say "imports the app gains (app-pie.elf against the same link without the engine):"
  list_new "$out/app-imports.txt" "$out/app-base-imports.txt" || status=1
  if ! cmp -s "$out/app-base-imports.txt" "$EVO_BUILD/pie-imports.txt"; then
    say "note: the baseline relink imports differ from EVO's recorded pie-imports.txt:"
    say "$(diff "$EVO_BUILD/pie-imports.txt" "$out/app-base-imports.txt" | grep '^[<>]' | tr '\n' ' ')"
  fi
else
  say "imports of the probe that EVO's last build did not have:"
  [ -f "$EVO_BUILD/pie-imports.txt" ] && { list_new "$out/probe-imports.txt" "$EVO_BUILD/pie-imports.txt" || status=1; }
fi
[ "$status" = 0 ] || { echo "build_ps5: an import would be null on the console (see above)" >&2; exit 1; }
say ""
say "ok"
