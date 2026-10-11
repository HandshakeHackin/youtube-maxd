#!/usr/bin/env bash
# Run every offline host check in order. One PASS/FAIL line per step, logs in
# build/test-logs/, non-zero exit when any step failed. Optional arguments
# select steps whose name contains one of them.
set -uo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${root}"
logs="${root}/build/test-logs"
rm -rf "${logs}"
mkdir -p "${logs}"
if [[ -n "${PT_PY:-}" ]]; then py="${PT_PY}"
elif [[ -x "${root}/.venv/bin/python" ]]; then py="${root}/.venv/bin/python"
else py=python3; fi
filters=("$@")
passed=0
failed=()

step() {
    local name="$1" wanted=0 filter
    shift
    if [[ ${#filters[@]} -gt 0 ]]; then
        for filter in "${filters[@]}"; do [[ "${name}" == *"${filter}"* ]] && wanted=1; done
        [[ ${wanted} -eq 1 ]] || return 0
    fi
    if "$@" > "${logs}/${name//\//_}.log" 2>&1 < /dev/null; then
        echo "PASS ${name}"
        passed=$((passed + 1))
    else
        echo "FAIL ${name}"
        failed+=("${name}")
    fi
}

fixtures() {
    local name
    for name in video.mp4 video.webm video-av1.mp4 video-hevc.mp4 audio.m4a audio.webm muxed.mp4 fixture.m3u8 thumb.png captions.vtt; do
        [[ -s "${root}/fixtures/generated/${name}" ]] || { python3 "${root}/scripts/generate_fixtures.py"; return; }
    done
    echo "fixtures present"
}

worker() (
    cd "${root}/companion/innertube" || exit 1
    [[ -d node_modules ]] || npm ci || exit 1
    npm run build && npm test
)

step fixtures fixtures
step worker worker
step pytest env PYTHONPATH="${root}/companion" "${py}" -m pytest -q
# Native preparations share one generated tree, so everything stays serial.
step scripts/test_native.sh bash "${root}/scripts/test_native.sh"
for script in "${root}"/scripts/test_*.sh; do
    name="scripts/$(basename "${script}")"
    case "${name}" in scripts/test_all.sh|scripts/test_native.sh) continue ;; esac
    step "${name}" bash "${script}"
done
for script in "${root}"/scripts/test_*.py; do
    step "scripts/$(basename "${script}")" python3 "${script}"
done

echo "SUMMARY ${passed} passed, ${#failed[@]} failed (logs: ${logs})"
if [[ ${#failed[@]} -gt 0 ]]; then
    printf 'FAILED %s\n' "${failed[@]}"
    exit 1
fi
