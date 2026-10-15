#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p validation/local
CC="${KAZUYA_CLANG:-clang}"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Icore core/ewgf_core.c tests/core_tests.c -o validation/local/core_tests
validation/local/core_tests | tee validation/local/host_tests.txt
