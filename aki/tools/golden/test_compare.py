#!/usr/bin/env python3
"""Run with python3 -m unittest discover -s aki/tools/golden -p 'test_compare.py'."""
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

HEADER = b"UGGD" + struct.pack("<I", 1)
FRAME = b"F" + struct.pack("<I6fB5I", 0, 0, 0, 640, 480, 1, 1, 1, 0, 0, 0, 0, 0)
DUMP = HEADER + FRAME + b"E"
COMPARE = pathlib.Path(__file__).with_name("compare.py")


class CompareTests(unittest.TestCase):
    def compare(self, a, b, status):
        with tempfile.TemporaryDirectory() as d:
            expected, actual = pathlib.Path(d) / "expected", pathlib.Path(d) / "actual"
            expected.write_bytes(a)
            actual.write_bytes(b)
            result = subprocess.run([sys.executable, str(COMPARE), str(expected), str(actual)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, status, result.stdout + result.stderr)
            return result.stdout + result.stderr

    def test_equal_complete_dump(self):
        self.assertIn("same: 1 frames", self.compare(DUMP, DUMP, 0))

    def test_every_truncated_prefix_fails_even_against_itself(self):
        for n in range(len(DUMP)):
            with self.subTest(length=n):
                self.compare(DUMP[:n], DUMP[:n], 1)

    def test_unsupported_version(self):
        other = b"UGGD" + struct.pack("<I", 2) + FRAME + b"E"
        self.compare(other, other, 1)

    def test_bytes_after_end(self):
        self.compare(DUMP, DUMP + b"X" + struct.pack("<Q", 2), 1)

    def test_shutdown_records_compared(self):
        a = HEADER + FRAME + b"X" + struct.pack("<Q", 1) + b"E"
        b = HEADER + FRAME + b"X" + struct.pack("<Q", 2) + b"E"
        self.compare(a, b, 1)

    def test_header_signed_zero_differs(self):
        other = bytearray(DUMP)
        struct.pack_into("<I", other, 13, 0x80000000)
        self.compare(DUMP, other, 1)

    def test_identical_nan_payloads_match(self):
        other = bytearray(DUMP)
        struct.pack_into("<I", other, 13, 0x7fc00001)
        self.compare(other, other, 0)

    def test_vertex_signed_zero_is_reported(self):
        head = FRAME[:30]
        a = HEADER + head + struct.pack("<I", 1) + bytes(48) + bytes(16) + b"E"
        b = bytearray(a)
        struct.pack_into("<I", b, 42, 0x80000000)
        self.assertIn("float bit patterns differ", self.compare(a, b, 1))


if __name__ == "__main__":
    unittest.main()
