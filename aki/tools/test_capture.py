#!/usr/bin/env python3
"""Screenshot shell failure paths, with deterministic stand-ins for Nix/X11.

    python3 -m unittest discover -s aki/tools -p test_capture.py
"""
import os
import pathlib
import subprocess
import tempfile
import unittest

TOOLS = pathlib.Path(__file__).resolve().parent


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        icd = self.root / "share/vulkan/icd.d"
        icd.mkdir(parents=True)
        (icd / "lvp_icd.test.json").touch()
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                        CAPTURE_TEST_ROOT=str(self.root))
        self.script("nix", '''
if [ "$1" = build ]; then echo "$CAPTURE_TEST_ROOT"; exit; fi
while [ "$1" != -c ]; do shift; done
shift
exec "$@"
''')
        self.script("xvfb-run", '''
shift 3
exec "$@"
''')
        self.script("import", '''
printf image > "$3"
exit "${CAPTURE_TEST_IMPORT_RC:-0}"
''')
        self.script("compare", '''
echo "${CAPTURE_TEST_METRIC:-0}" >&2
exit "${CAPTURE_TEST_COMPARE_RC:-0}"
''')

    def script(self, name, body):
        path = self.bin / name
        path.write_text("#!/usr/bin/env bash\nset -eu\n" + body)
        path.chmod(0o755)

    def capture(self, command, status):
        out = self.root / "capture.png"
        out.write_text("stale capture")
        result = subprocess.run([str(TOOLS / "xshot.sh"), str(out), "1", *command],
                                env=self.env, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, status, result.stdout + result.stderr)
        self.assertEqual(out.exists(), status == 0)
        return result

    def test_failed_application_rejected(self):
        result = self.capture(["false"], 1)
        self.assertIn("status 1", result.stderr)

    def test_early_successful_exit_rejected(self):
        self.capture(["true"], 1)

    def test_live_application_captured(self):
        self.capture(["sleep", "30"], 0)

    def test_failed_import_rejected_and_image_removed(self):
        self.env["CAPTURE_TEST_IMPORT_RC"] = "7"
        self.capture(["sleep", "30"], 7)

    def compare(self, rc, metric, expected):
        self.env.update(CAPTURE_TEST_COMPARE_RC=str(rc), CAPTURE_TEST_METRIC=metric)
        result = subprocess.run([str(TOOLS / "compare_shots.sh"), "a.png", "b.png"],
                                env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)

    def test_identical_images(self):
        self.compare(0, "0", 0)

    def test_different_images(self):
        self.compare(1, "3", 1)

    def test_compare_error_cannot_pass_with_zero_metric(self):
        self.compare(2, "0", 2)


if __name__ == "__main__":
    unittest.main()
