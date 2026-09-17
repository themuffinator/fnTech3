"""Execute the production FNZ resolver/reader with controlled ZIP I/O.

The real immutable profile catalog is linked into the probe. ZIP construction
and release validation are covered separately by release_packaging_tests.py.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RootArchiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            raise unittest.SkipTest("C++ compiler unavailable")
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(prefix="hud-root-archive-", dir=scratch)
        cls.addClassCleanup(cls.temporary.cleanup)
        directory = Path(cls.temporary.name)
        files = (ROOT / "code/qcommon/files.c").read_text()
        defines = "\n".join(line for line in files.splitlines()
                            if line.startswith("#define FNTECH3_ROOT_ARCHIVE_"))
        start = files.index("static qboolean FS_RootArchiveSafePath(")
        end = files.index("\nint FS_ReadFile(", start)
        reader = files[start:end]
        shared = (ROOT / "code/qcommon/q_shared.c").read_text()
        start = shared.index("unsigned int crc32_buffer(")
        crc = shared[start:shared.index("\n}", start) + 2]
        template = (ROOT / "tests/fixtures/root_archive_probe.cpp").read_text()
        source = directory / "root_archive_probe.cpp"
        source.write_text(template.replace("// INSERT_CRC", crc)
                          .replace("// INSERT_READER", defines + "\n" + reader))
        cls.executable = directory / "root_archive_probe.exe"
        result = subprocess.run(
            [compiler, *compiler_target_args(compiler), "-std=c++17", "-O2",
             "-DFNTECH3_RTCW_DEVELOPMENT", "-I", str(ROOT), str(source),
             str(ROOT / "code/qcommon/game_profile.cpp"), "-o", str(cls.executable)],
            cwd=directory, capture_output=True, text=True, timeout=120,
        )
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def test_profile_isolation_precedence_bounds_and_read_failures(self):
        for profile_id, key in enumerate(("quake3", "quakelive", "rtcw-sp", "rtcw-mp"), 1):
            with self.subTest(profile=key):
                result = subprocess.run([str(self.executable), str(profile_id), key],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
