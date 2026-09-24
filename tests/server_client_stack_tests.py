"""Keep whole client_t temporaries out of the server's stack frames.

client_t is over 500 KB, and the Windows executables reserve 1 MB for the
main thread's stack. Aggregate assignment (`client = {}`) builds a complete
temporary in the caller's frame -- MSVC does so even at /O2 -- and
SV_ChangeMaxClients calling SV_AllocClients nested two of them, so changing
sv_maxclients on a running server overflowed the stack in _chkstk. MSVC's /Od
gave SV_DirectConnect two, more than the whole stack by itself.

Unoptimized code keeps every such temporary, so the translation units that
create, resize and clear client slots are compiled at -O0 with a frame limit
well below one client_t. A canary first shows that this compiler keeps the
temporary the check looks for.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

# A quarter of the main thread's stack, and far below sizeof(client_t).
FRAME_LIMIT = 256 * 1024

CLIENT_SLOT_UNITS = ('code/server/sv_init.cpp', 'code/server/sv_client.cpp')

CANARY = '''#include "code/server/server.h"
void Canary( client_t &client ) { client = {}; }
'''


class ServerClientStackTests(unittest.TestCase):
    def compile(self, compiler, target, source, folder):
        command = [compiler, *target, '-std=c++17', '-O0', '-c',
                   '-I', str(ROOT), '-I', str(ROOT / 'code'),
                   '-Wframe-larger-than=%d' % FRAME_LIMIT,
                   str(source), '-o', str(folder / (Path(source).stem + '.o'))]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        output = built.stdout + built.stderr
        self.assertEqual(built.returncode, 0, output)
        return [line for line in output.splitlines() if 'frame-larger-than' in line]

    def test_client_slots_are_cleared_without_stack_temporaries(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='server-client-stack-', dir=scratch) as folder:
            folder = Path(folder)
            canary = folder / 'canary.cpp'
            canary.write_text(CANARY, encoding='utf-8')
            if not self.compile(compiler, target, canary, folder):
                self.skipTest('%s builds `client = {}` without a stack temporary at -O0'
                              % Path(compiler).name)
            for unit in CLIENT_SLOT_UNITS:
                with self.subTest(unit=unit):
                    oversized = self.compile(compiler, target, ROOT / unit, folder)
                    self.assertEqual(oversized, [], '\n'.join(oversized))


if __name__ == '__main__':
    unittest.main()
