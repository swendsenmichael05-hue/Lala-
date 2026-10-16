#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p validation/local
CC="${KAZUYA_CLANG:-clang}"
for suite in core timing; do
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -Icore core/ewgf_core.c "tests/${suite}_tests.c" -o "validation/local/${suite}_tests"
    "validation/local/${suite}_tests" | tee "validation/local/${suite}_tests.txt"
done
bash tools/test_requests.sh
bash tools/test_speed.sh
