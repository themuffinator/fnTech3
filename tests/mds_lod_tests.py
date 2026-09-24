"""MDS detail flags, finite projection policy and bounded collapse ancestry."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mds_format_tests import FIXTURE

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_mds_lod.h"
static void Near(float a, float b) { assert(fabsf(a - b) < 0.000001f); }
int main(void) {
    fnMdsView_t view; fnMdsSurfaceView_t surface;
    uint32_t map[5], saved[5];
    int i;
    Make(); assert(FnMds_Open(original, BYTES, &view)); assert(FnMds_Surface(&view, SURFACE, &surface));
    Near(FnMds_LodFraction(COM_GAME_RTCW_SP, 8, 0.2f, 1, 4, 1, 1), 1);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 8, 0.2f, 1, 0, 1, 0), 0.1f);
    Near(FnMds_LodFraction(COM_GAME_RTCW_SP, 32, 0.2f, 1, 0, 1, 0), 0.16f);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 40, 0.2f, 1, 0, 1, 0), 0.08f);
    Near(FnMds_LodFraction(COM_GAME_QUAKE3, 40, 0.2f, 1, 0, 1, 0), 0.2f);
    Near(FnMds_LodFraction(COM_GAME_QUAKELIVE, 40, 0.2f, 1, 0, 1, 0), 0.2f);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 0, 0, 5, 1, 1, 0.1f), 0.65f);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 0, 0.2f, 5, -1, 1, 0.1f), 1);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 0, -1, 1, 0, 1, 0), 0);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 0, NAN, 1, 0, 1, 0), 1);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 0, 1, INFINITY, 0, 1, 0), 1);
    Near(FnMds_LodFraction(COM_GAME_RTCW_MP, 0, 1, 1, NAN, 1, 0), 1);
    assert(FnMds_LodVertexCount(&surface, 0, 0) == 1);
    assert(FnMds_LodVertexCount(&surface, 1, 0) == 3);
    assert(FnMds_LodVertexCount(&surface, NAN, 0) == 3);
    assert(FnMds_LodVertexCount(&surface, -1, 0) == 1);
    surface.minLod = 3;
    assert(FnMds_LodVertexCount(&surface, 0, 0) == 3);
    assert(FnMds_LodVertexCount(&surface, 0, 1) == 1); // Dead models may go below minLod.
    for (i = 1; i <= 3; ++i) {
        map[0] = 0x12345678; map[4] = 0xabcdef01;
        assert(FnMds_CollapseVertices(&surface, i, map + 1, 3));
        assert(map[0] == 0x12345678 && map[4] == 0xabcdef01 && !map[1]);
        assert(map[2] == (i >= 2 ? 1 : 0) && map[3] == (i >= 3 ? 2 : i >= 2 ? 1 : 0));
    }
    memcpy(saved, map, sizeof(map));
    assert(!FnMds_CollapseVertices(&surface, 0, map + 1, 3));
    assert(!FnMds_CollapseVertices(&surface, 4, map + 1, 3));
    assert(!FnMds_CollapseVertices(&surface, 1, map + 1, 2));
    assert(!FnMds_CollapseVertices(NULL, 1, map + 1, 3));
    assert(!FnMds_CollapseVertices(&surface, 1, NULL, 3));
    assert(!memcmp(saved, map, sizeof(map)));
    Put32(original + SURFACE + COLLAPSE + 8, 2);
    assert(!FnMds_CollapseVertices(&surface, 1, map + 1, 3)); // A self-loop cannot escape the retained prefix.
    return 0;
}
'''


class MdsLodTests(unittest.TestCase):
    def test_profile_flags_and_collapse_maps(self):
        for language, compiler_name, standard in (('c', 'clang', 'c11'), ('cpp', 'clang++', 'c++17')):
            compiler = shutil.which(compiler_name)
            if not compiler:
                self.skipTest(compiler_name + ' unavailable')
            target = compiler_target_args(compiler)
            if os.name != 'nt':
                target = ['-m32']
            (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
            with self.subTest(language=language), tempfile.TemporaryDirectory(prefix='mds-lod-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp)
                source, executable = folder / ('probe.' + language), folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(FIXTURE[:FIXTURE.index('int main(void)')] + CHECKS, encoding='utf-8')
                command = [compiler, *target, '-std=' + standard, '-I', str(ROOT), str(source), '-o', str(executable)]
                if os.name != 'nt':
                    command.append('-lm')
                build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
