#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cases=("$@")
if [ ${#cases[@]} -eq 0 ]; then cases=(maxd_support maxd_experience maxd_party_provider maxd_smart maxd_auto maxd_quality storage_failure queue_reorder live_owner phone_recommendation suggestions native playback_recovery stream_choice sponsor paid_content dearrow speed_memory music_context topic_catalog player_chat chat_reader voice_search_provider disconnect_preferences player_preferences profile_settings video_geometry queue_failure codecs playlist_context playlist_window playback_next manual_navigation group_navigation group_block_window catalog_progress video_actions history_remove channel_block channel_block_queue channel_block_window channel_block_next channel_block_name video_menu search_history history_modes remote_history search_options description library_sections feedback section_order menu_launch extra_provider companion_features channel_pages account_changes account_settings links_about backup_ui third_sections third_captions comment_post channel_marks subscriptions_import startup_lock guest_sections interface_settings library_paging typed_search); fi
for name in "${cases[@]}"; do
  case "$name" in maxd_support|maxd_party_provider|maxd_experience|maxd_smart|maxd_auto|maxd_quality|queue_reorder|live_owner|phone_recommendation|suggestions|native|playback_recovery|stream_choice|sponsor|paid_content|dearrow|speed_memory|music_context|topic_catalog|player_chat|chat_reader|voice_search_provider|disconnect_preferences|player_preferences|profile_settings|video_geometry|queue_failure|codecs|playlist_context|playlist_window|playback_next|manual_navigation|group_navigation|group_block_window|catalog_progress|video_actions|history_remove|channel_block|channel_block_queue|channel_block_window|channel_block_next|channel_block_name|video_menu|search_history|history_modes|remote_history|search_options|description|library_sections|feedback|section_order|menu_launch|extra_provider|companion_features|channel_pages|account_changes|account_settings|links_about|backup_ui|third_sections|third_captions|comment_post|channel_marks|subscriptions_import|startup_lock|guest_sections|interface_settings|library_paging|typed_search) ;; *) echo "Unknown native test: $name" >&2; exit 2 ;; esac
done
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/native-common"
mkdir -p "$destination"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${root}/native/maxd" -I"${evo}/include" -I"${evo}/addons/include")
sources=("${evo}/addons/src/tube_provider.c" "${root}/native/maxd/party_server.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" "${root}/native/tube_phone.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c")
objects=()
for source in "${sources[@]}"; do
  object="${destination}/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
for name in "${cases[@]}"; do
  cc "${flags[@]}" "${root}/tests/test_${name}.c" "${objects[@]}" -lm -o "${root}/build/tests/test_${name}"
  ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_${name}"
done
