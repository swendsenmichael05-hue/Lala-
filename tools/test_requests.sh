#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p validation/local
CC="${KAZUYA_CLANG:-clang}"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Icore core/ewgf_core.c core/request_gate.c tests/request_tests.c -o validation/local/request_tests
validation/local/request_tests | tee validation/local/request_tests.txt
"$CC" -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Icore core/ewgf_core.c core/request_gate.c tests/request_tests.c -o validation/local/request_sanitizers
validation/local/request_sanitizers | tee validation/local/request_sanitizers.txt
