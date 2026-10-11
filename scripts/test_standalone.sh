#!/usr/bin/env bash
# Offline tests of the embedded companion (standalone/): engine self-test and
# the HTTP service against canned upstream answers, plain and under ASan/UBSan.
set -euo pipefail
exec bash "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)/standalone/test_host.sh"
