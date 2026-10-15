"""Tests header/staging checks with temporary artificial fixtures, NOT real NROs."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

HERE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('stage_nro', HERE / 'tools/stage_nro.py')
stage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage)

class StagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        stage.ROOT = self.root
        (self.root / 'config').mkdir()
        (self.root / 'config/config.ini').write_text('enable_macro=false\n')
        (self.root / 'target').mkdir()
    def tearDown(self):
        self.temp.cleanup()
    def fixture(self, data):
        (self.root / 'target/libkazuya_training_lab.nro').write_bytes(data)
    def test_missing_output(self):
        with self.assertRaises(SystemExit): stage.main()
        self.assertFalse((self.root/'dist').exists())
    def test_wrong_format(self):
        self.fixture(b'\x7fELF'+b'\0'*252)
        with self.assertRaises(SystemExit): stage.main()
        self.assertFalse((self.root/'dist').exists())
    def test_size_past_eof(self):
        raw=bytearray(256); raw[16:20]=b'NRO0'; struct.pack_into('<I',raw,24,9999)
        self.fixture(raw)
        with self.assertRaises(SystemExit): stage.main()
    def test_tiny_size(self):
        raw=bytearray(256); raw[16:20]=b'NRO0'; struct.pack_into('<I',raw,24,16)
        self.fixture(raw)
        with self.assertRaises(SystemExit): stage.main()
    def test_staging_uses_exact_bytes_and_diagnostic_default(self):
        raw=bytearray(256); raw[16:20]=b'NRO0'; struct.pack_into('<I',raw,24,256)
        self.fixture(raw); stage.main()
        binary=self.root/'dist/SD_ROOT/atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro'
        self.assertEqual(binary.read_bytes(),raw)
        self.assertIn('false',(self.root/'dist/SD_ROOT/ultimate/kazuya_lab/config.ini').read_text())
        self.assertTrue((self.root/'dist/BUILD_OK.txt').exists())

if __name__=='__main__': unittest.main(verbosity=2)
