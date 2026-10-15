#!/usr/bin/env python3
"""Stage a successfully compiled NRO; never converts another format to an NRO."""
from pathlib import Path
import hashlib
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    paths = list((ROOT / 'target').rglob('libkazuya_training_lab.nro'))
    if not paths:
        raise SystemExit('Expected Skyline NRO was not found. Nothing staged.')
    source = max(paths, key=lambda p: p.stat().st_mtime_ns)
    data = source.read_bytes()
    if len(data) < 128 or data[16:20] != b'NRO0':
        raise SystemExit('Invalid NRO header; refusing to stage.')
    size, = struct.unpack_from('<I', data, 24)
    if size < 128 or size > len(data):
        raise SystemExit('Invalid declared NRO image size.')
    dist = ROOT / 'dist'
    if dist.exists():
        shutil.rmtree(dist)
    plugin = dist / 'SD_ROOT/atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro'
    config = dist / 'SD_ROOT/ultimate/kazuya_lab/config.ini'
    plugin.parent.mkdir(parents=True)
    config.parent.mkdir(parents=True)
    shutil.copy2(source, plugin)
    shutil.copy2(ROOT / 'config/config.ini', config)
    if (ROOT / 'Cargo.lock').exists():
        shutil.copy2(ROOT / 'Cargo.lock', dist / 'Cargo.lock')
    (dist / 'NRO_SHA256.txt').write_text(hashlib.sha256(data).hexdigest()+'\n', encoding='utf-8')
    (dist / 'BUILD_OK.txt').write_text('Native build produced an NRO. NOT hardware-tested; diagnostics-only default.\n', encoding='utf-8')
    print(f'Staged {plugin}. This has NOT been installed on any console.')

if __name__ == '__main__':
    main()
