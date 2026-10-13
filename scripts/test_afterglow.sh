#!/usr/bin/env bash
# The Afterglow interface on the PC: builds it against the pinned UI kit and
# runs every board of tests/afterglow/boards in the four looks. Each board is
# brought up from fixtures, driven by its steps and checked: what has the focus
# and where, the state the screen reports, no text across the edge of the
# screen, the looks' text contrast, and a picture that is not blank. Pictures
# land in build/afterglow/pictures. Needs Mesa's surfaceless EGL (llvmpipe).
#
# The four looks run at the same time, one process each, on one build; their
# reports are in build/afterglow/pictures/<look>.log. Harness options given
# here go to every look (--only <text>, --lang <tag>).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
pictures="${root}/build/afterglow/pictures"
rm -rf "${pictures}"
mkdir -p "${pictures}"
bash "${root}/scripts/afterglow_host.sh" --build-only

looks=(afterglow onyx daybreak contrast)
pids=()
for look in "${looks[@]}"; do
    bash "${root}/scripts/afterglow_host.sh" --run-only "${pictures}" --look "${look}" "$@" > "${pictures}/${look}.log" 2>&1 &
    pids+=("$!")
done
failed=0
for index in "${!looks[@]}"; do
    wait "${pids[${index}]}" || failed=1
done
boards=0; checks=0; failures=0
for look in "${looks[@]}"; do
    grep -E '^FAIL ' "${pictures}/${look}.log" || true
    summary="$(grep -E '^(PASS|FAIL): afterglow host:' "${pictures}/${look}.log" | tail -1 || true)"
    if [[ -z "${summary}" ]]; then
        echo "FAIL: the ${look} run ended without a summary (${pictures}/${look}.log)"
        tail -5 "${pictures}/${look}.log" || true
        failed=1
        continue
    fi
    echo "${summary}"
    [[ "${summary}" =~ ([0-9]+)\ boards,\ ([0-9]+)\ pictures,\ ([0-9]+)\ checks,\ ([0-9]+)\ failed ]] || { failed=1; continue; }
    boards="${BASH_REMATCH[1]}"
    checks=$((checks + BASH_REMATCH[3]))
    failures=$((failures + BASH_REMATCH[4]))
done
grep -h -E '^language ' "${pictures}"/*.log | sort -u || true
if [[ "${failed}" -ne 0 || "${failures}" -ne 0 ]]; then
    echo "FAIL: afterglow: ${boards} boards in ${#looks[@]} looks, ${checks} checks, ${failures} failed"
    exit 1
fi
echo "PASS: afterglow: ${boards} boards in ${#looks[@]} looks, ${checks} checks, 0 failed"
