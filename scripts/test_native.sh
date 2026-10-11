#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cases=("$@")
if [ ${#cases[@]} -eq 0 ]; then cases=(native playback_recovery sponsor paid_content dearrow speed_memory music_context topic_catalog player_chat chat_reader voice_search_provider disconnect_preferences player_preferences profile_settings video_geometry queue_failure codecs playlist_context playlist_window playback_next manual_navigation group_navigation group_block_window video_actions history_remove channel_block channel_block_queue channel_block_window channel_block_next channel_block_name); fi
for name in "${cases[@]}"; do
  case "$name" in native|playback_recovery|sponsor|paid_content|dearrow|speed_memory|music_context|topic_catalog|player_chat|chat_reader|voice_search_provider|disconnect_preferences|player_preferences|profile_settings|video_geometry|queue_failure|codecs|playlist_context|playlist_window|playback_next|manual_navigation|group_navigation|group_block_window|video_actions|history_remove|channel_block|channel_block_queue|channel_block_window|channel_block_next|channel_block_name) ;; *) echo "Unknown native test: $name" >&2; exit 2 ;; esac
done
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/native-common"
mkdir -p "$destination"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include")
sources=("${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c")
objects=()
for source in "${sources[@]}"; do
  object="${destination}/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
for name in "${cases[@]}"; do
  cc "${flags[@]}" "${root}/tests/test_${name}.c" "${objects[@]}" -lm -o "${root}/build/tests/test_${name}"
  ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_${name}"
done
