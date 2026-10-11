"""Motion correspondence must never exchange same-model entity histories."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RtxMotionIdentityTests(unittest.TestCase):
    def test_identity_ambiguity_reordering_and_teleports(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        source = '''#include <cassert>
#include <cstdint>
#include <limits>
#include <cstdio>
typedef int qboolean;
typedef int qhandle_t;
typedef float vec3_t[3];
'''
        source += (ROOT / 'code/rendererrtx/rt/rt_motion.h').read_text()
        source += r'''
int main() {
    rtMotionEntity_t previous[2]={}, current[2]={};
    previous[0].model=previous[1].model=7;
    previous[0].count=previous[1].count=120;
    previous[0].topology=previous[1].topology=112;
    previous[0].materialGeneration=previous[1].materialGeneration=2;
    previous[0].identity=41;previous[1].identity=42;
    previous[1].origin[0]=4;
    // Two players exchange order and position: stable IDs keep their poses.
    current[0]=previous[1]; current[1]=previous[0];
    current[0].origin[0]=0; current[1].origin[0]=4;
    assert(RT_MotionMatch(current,2,0,previous,2,16)==1);
    assert(RT_MotionMatch(current,2,1,previous,2,16)==0);
    // No identity: neither submission order nor nearest distance proves
    // which of the identical parts it was. Both must discard history.
    current[0].identity=current[1].identity=previous[0].identity=previous[1].identity=-1;
    assert(RT_MotionMatch(current,2,0,previous,2,16)==-1);
    assert(RT_MotionMatch(current,2,1,previous,2,16)==-1);
    assert(RT_MotionMatch(current,1,0,previous,2,16)==-1);
    // A unique part is safe, but a disappearance, teleport, topology or
    // material-table change invalidates it even with equal vertex counts.
    assert(RT_MotionMatch(current,1,0,previous,1,16)==0);
    assert(RT_MotionMatch(current,1,0,previous,0,16)==-1);
    current[0].origin[0]=200;
    assert(RT_MotionMatch(current,1,0,previous,1,16)==-1);
    current[0]=previous[0];current[0].topology++;
    assert(RT_MotionMatch(current,1,0,previous,1,16)==-1);
    current[0]=previous[0];current[0].materialGeneration++;
    assert(RT_MotionMatch(current,1,0,previous,1,16)==-1);
    current[0]=previous[0];previous[0].taken=1;
    assert(RT_MotionMatch(current,1,0,previous,1,16)==-1);
    previous[0].taken=0;current[0].origin[0]=std::numeric_limits<float>::quiet_NaN();
    assert(RT_MotionMatch(current,1,0,previous,1,16)==-1);
    // Duplicate stable identities are ambiguous as well (same-model parts).
    current[0]=current[1]=previous[0];
    current[0].identity=current[1].identity=previous[0].identity=88;
    assert(RT_MotionMatch(current,2,0,previous,1,16)==-1);
    std::puts("identity, ambiguous parts, reordering, despawn and teleport: PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-motion-') as temp:
            path = Path(temp)
            cpp = path / 'motion.cpp'
            exe = path / ('motion.exe' if os.name == 'nt' else 'motion')
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                           cwd=path, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], cwd=path, check=True, capture_output=True, text=True)
            self.assertIn('PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
