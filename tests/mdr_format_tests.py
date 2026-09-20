"""RDM5 disk validation and production renderer-loader regressions."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class MdrFormatTests(unittest.TestCase):
    def test_bounded_loader_c_and_cpp(self):
        for language in ('c', 'c++'):
            compiler = shutil.which('clang' if language == 'c' else 'clang++')
            if not compiler:
                self.skipTest('Clang C/C++ compiler unavailable')
            with self.subTest(language=language), tempfile.TemporaryDirectory(prefix='mdr-format-', dir=ROOT / '.tmp') as temp:
                executable = Path(temp) / ('probe.exe' if os.name == 'nt' else 'probe')
                args = [compiler, *compiler_target_args(compiler), '-x', language,
                        '-std=' + ('c11' if language == 'c' else 'c++17'),
                        str(ROOT / 'tests/mdr_format_tests.c'), '-o', str(executable)]
                if os.name != 'nt':
                    args += ['-lm']
                build = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
