"""Compile the canonical MDR byte builder and exercise engine reader contracts."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class JKAMDRTests(unittest.TestCase):
    def test_generated_records_weighted_results_limits_and_bounds(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/jka-compat/mdr-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    exe = Path(temporary) / ('fast.exe' if fast else 'default.exe')
                    flags = ['-ffast-math'] if fast else []
                    built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
                        '-Wall', '-Wextra', '-Werror', *flags, '-I', str(ROOT),
                        str(ROOT / 'tests/jka_mdr_tests.cpp'), '-o', str(exe)],
                        cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    self.assertIn('canonical MDR builder tests passed', run.stdout)


if __name__ == '__main__':
    unittest.main()
