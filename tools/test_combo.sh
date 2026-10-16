#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p validation/local
CC="${KAZUYA_CLANG:-clang}"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Icore core/ewgf_core.c tests/combo_tests.c -lm -o validation/local/combo_tests
validation/local/combo_tests | tee validation/local/combo_tests.txt
"$CC" -std=c11 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -Wall -Wextra -Werror -Icore core/ewgf_core.c tests/combo_tests.c -lm -o validation/local/combo_sanitizers
ASAN_OPTIONS=detect_leaks=1 validation/local/combo_sanitizers | tee validation/local/combo_sanitizers.txt
"$CC" --target=aarch64-unknown-none -std=c11 -O2 -fPIC -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -Werror -Icore -c core/combo_lab.c -o validation/local/combo_aarch64.o
printf 'Freestanding AArch64 object compiled. Not an installable NRO or a console test.\n' | tee validation/local/combo_cross_compile.txt
