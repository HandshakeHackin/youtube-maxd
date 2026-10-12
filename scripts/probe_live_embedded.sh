#!/usr/bin/env bash
# One bounded LIVE check of the built-in service, as Guest and read-only: the
# provider (built-in mode) searches and resolves one video through the real
# service in its own process, and a host FFmpeg carrying the project's DASH
# patch plays the manifest for three seconds from the start and after one seek
# to the middle. Not part of any test run: it talks to YouTube.
#   about 10 API requests; media: three seconds of one small video, twice.
#   every FFmpeg run is limited to 25 s and 20 MB.
# Usage: scripts/probe_live_embedded.sh [video-id] [search words] [seek seconds]
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
video="${1:-jNQXAC9IVRw}"
query="${2:-Me at the zoo}"
seek="${3:-9}"
ffmpeg="${PROSPEROTUBE_HOST_FFMPEG:-${HOME}/.cache/prosperotube/ffmpeg-host/patched/ffmpeg}"
[[ -x "${ffmpeg}" ]] || { echo "No patched host FFmpeg (set PROSPEROTUBE_HOST_FFMPEG)." >&2; exit 2; }
bash "${root}/scripts/test_embedded_e2e.sh" > /dev/null   # builds the program; its own run is offline
scratch="$(mktemp -d)"
trap 'touch "${scratch}/done"; wait 2>/dev/null || true; rm -rf "${scratch}"' EXIT
TUBE_E2E_LIVE=1 TUBE_E2E_QUERY="${query}" TUBE_E2E_VIDEO="${video}" TUBE_E2E_URL_FILE="${scratch}/url" \
  TUBE_E2E_DONE_FILE="${scratch}/done" TUBE_E2E_BUNDLE="${root}/standalone/build/tube_js.bundle.js" \
  ASAN_OPTIONS=detect_leaks=0 "${root}/build/tests/test_embedded_e2e" &
for _ in $(seq 1 120); do [[ -s "${scratch}/url" ]] && break; sleep 0.5; done
[[ -s "${scratch}/url" ]] || { echo "The provider did not reach a manifest." >&2; exit 1; }
manifest="$(cat "${scratch}/url")"
echo "manifest ${manifest%%/media/*}/media/<session>/${manifest##*/}"
play() {   # label, then FFmpeg input options
  local label=$1 start
  shift
  start=$(date +%s.%N)
  # The app's own options (media/src/evo_stream_io.c); three seconds, copied to nowhere.
  timeout 25 "${ffmpeg}" -hide_banner -nostdin -v error -allowed_extensions ALL -reconnect 1 -reconnect_streamed 1 \
    -reconnect_on_network_error 1 -reconnect_delay_max 2 -rw_timeout 5000000 -reconnect_max_retries 3 \
    -reconnect_delay_total_max 6 "$@" -i "${manifest}" -t 3 -c copy -f null - -progress "${scratch}/progress" 2> "${scratch}/log" \
    || { echo "${label}: FFmpeg failed"; grep -v "googlevideo" "${scratch}/log" | tail -5 | sed 's/^/  /'; return 1; }
  printf '%s: ok in %.1f s; %s video frames, %s microseconds of media copied\n' "${label}" \
    "$(echo "$(date +%s.%N) - ${start}" | bc)" "$(grep '^frame=' "${scratch}/progress" | tail -1 | cut -d= -f2)" \
    "$(grep '^out_time_us=' "${scratch}/progress" | tail -1 | cut -d= -f2)"
  [[ "$(grep '^frame=' "${scratch}/progress" | tail -1 | cut -d= -f2)" -gt 0 ]]
}
status=0
play "from the start" || status=1
play "after a seek to ${seek} s" -ss "${seek}" || status=1
touch "${scratch}/done"
wait || status=1
exit "${status}"
