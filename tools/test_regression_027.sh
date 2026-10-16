#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p validation/local
CC="${KAZUYA_CLANG:-clang}"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Icore -c core/ewgf_core.c -o validation/local/bridge_pulse.o
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Icore -c core/request_gate.c -o validation/local/bridge_request.o
ar rcs validation/local/libkz_bridge.a validation/local/bridge_pulse.o validation/local/bridge_request.o
rustc --edition=2021 --test tests/regression_027.rs -L native=validation/local -l static=kz_bridge -o validation/local/regression_027
validation/local/regression_027 --test-threads=1 | tee validation/local/regression_027.txt
