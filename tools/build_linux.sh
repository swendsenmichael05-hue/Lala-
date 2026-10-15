#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
exec > >(tee build.log) 2>&1
rm -f dist/BUILD_OK.txt
for tool in cargo rustup git clang python3; do
    command -v "$tool" >/dev/null || { echo "Missing: $tool. Read README.md."; exit 1; }
done
cargo skyline --version
bash tools/test_core.sh
cargo skyline build --release
python3 tools/stage_nro.py
