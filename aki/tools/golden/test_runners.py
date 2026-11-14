#!/usr/bin/env python3
"""Negative integration tests: test_runners.py <c++ runner> <aki runner>.

Run in the nix dev shell after building both runners.
"""
import pathlib
import subprocess
import sys
import tempfile
import unittest

RUNNERS = [str(pathlib.Path(p).resolve()) for p in sys.argv[1:]]
sys.argv[1:] = []
ROOT = pathlib.Path(__file__).resolve().parents[3]


class RunnerTests(unittest.TestCase):
    def run_scenario(self, source, status, script=None, stdout_contains=None):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            if script is not None:
                (d / "script.lua").write_text(script)
                source = source.replace("SCRIPT", str(d / "script.lua"))
            (d / "ui.ugui").write_text('panel root { width: 200; height: 100; '
                                       'button clicked { width: 100; height: 60; } }')
            source = source.replace("UI", str(d / "ui.ugui"))
            scenario = d / "scenario.txt"
            scenario.write_text(source)
            for runner in RUNNERS:
                with self.subTest(runner=runner):
                    result = subprocess.run([runner, str(scenario), str(d / "out.bin")],
                                            cwd=ROOT, capture_output=True, text=True)
                    self.assertEqual(result.returncode, status, result.stdout + result.stderr)
                    if stdout_contains is not None:
                        self.assertIn(stdout_contains, result.stdout)

    def test_valid_frame(self):
        self.run_scenario("frame\n", 0)

    def test_missing_ui(self):
        self.run_scenario("ui /no/review-test-ui.ugui\nframe\n", 1)

    def test_missing_script(self):
        self.run_scenario("script /no/review-test-script.lua\nframe\n", 1)

    def test_script_syntax_error(self):
        self.run_scenario("script SCRIPT\nframe\n", 1, "function (")

    def test_script_runtime_error(self):
        self.run_scenario("script SCRIPT\nframe\n", 1, 'error("review-test")')

    def test_lua_click_handler_runs(self):
        self.run_scenario("viewport 200 100 200 100\nui UI\nscript SCRIPT\nframe\n"
                          "move 20 20\nframe\ndown left\nframe\nup left\nframe\n", 0,
                          'function on_clicked(w) print("review-click-handler:" .. w.name) end',
                          "review-click-handler:clicked")


if __name__ == "__main__":
    if len(RUNNERS) != 2:
        sys.exit(__doc__)
    unittest.main()
