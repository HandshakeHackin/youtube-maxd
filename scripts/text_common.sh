# Sourced by the text engine's host scripts: compiles native/text for the host.
#   text_engine <plain|asan> <object folder>
# leaves TEXT_FLAGS (compiler flags), TEXT_OBJECTS and TEXT_LIBS (what to link).
# SPDX-License-Identifier: GPL-3.0-or-later
TEXT_UNITS=(tube_text tube_text_unicode tube_text_fonts tube_text_atlas tube_text_layout)

text_engine() {
  local flavour=$1 out=$2 root deps unit
  root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
  deps=$(bash "$root/scripts/text_deps.sh" "$flavour")
  local include
  mapfile -t include < "$deps/include.flags"
  mapfile -t TEXT_LIBS < "$deps/lib.flags"
  # The libraries' headers are not ours to warn about.
  include=("${include[@]/#-I/-isystem}")
  TEXT_FLAGS=(-std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -I"$root/native/text" -I"$root/tests" "${include[@]}")
  if [[ $flavour == asan ]]; then
    TEXT_FLAGS+=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize=vptr -fno-sanitize-recover=undefined)
  else
    TEXT_FLAGS+=(-O2)
  fi
  mkdir -p "$out"
  TEXT_OBJECTS=()
  for unit in "${TEXT_UNITS[@]}"; do
    "${CXX:-c++}" "${TEXT_FLAGS[@]}" -c "$root/native/text/$unit.cpp" -o "$out/$unit.o"
    TEXT_OBJECTS+=("$out/$unit.o")
  done
}
