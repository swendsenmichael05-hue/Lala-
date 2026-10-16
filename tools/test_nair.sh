#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p validation/local
# The policy is dependency-free Rust, so no game bindings or fake native APIs.
rustc --edition=2021 --test src/action_gate.rs -o validation/local/action_gate_tests
validation/local/action_gate_tests --nocapture | tee validation/local/action_gate_tests.txt
CC="${KAZUYA_CLANG:-clang}"
for mode in optimized sanitized; do
  flags=(-O2)
  if [ "$mode" = sanitized ]; then flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer); fi
  "$CC" -std=c11 -Wall -Wextra -Werror "${flags[@]}" -Icore core/ewgf_core.c core/request_gate.c tests/nair_tests.c -o "validation/local/nair_${mode}"
  "validation/local/nair_${mode}" | tee "validation/local/nair_${mode}.txt"
done
