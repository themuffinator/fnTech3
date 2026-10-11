"""Outcast's random waveform selects the existing Fn evaluator in each renderer."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cstring>
#include <initializer_list>
static int warnings;
static void QDECL Print(int level,const char*,...){assert(level==PRINT_WARNING);++warnings;}
static struct {comGameId_t gameId;decltype(&Print) Printf;} ri{COM_GAME_AUTO,Print};
static struct {const char *name;} shader{"synthetic/wave"};
'''

CHECKS = r'''
int main(){
 static_assert(GF_RANDOM==7&&GF_NOISE==6);
 for(int profile=0;profile<COM_GAME_COUNT;++profile){
  ri.gameId=static_cast<comGameId_t>(profile);
  const bool random=profile==COM_GAME_STVEF_SP||profile==COM_GAME_STVEF_MP||
   profile==COM_GAME_JK2_SP||profile==COM_GAME_JK2_MP;
  warnings=0;assert(NameToGenFunc("random")== (random?GF_RANDOM:GF_SIN));
  assert(warnings==(random?0:1));
  warnings=0;assert(NameToGenFunc("RaNdOm")== (random?GF_RANDOM:GF_SIN));
  assert(warnings==(random?0:1));
  const char *names[]{"sin","square","triangle","sawtooth","inversesawtooth","noise"};
  const genFunc_t functions[]{GF_SIN,GF_SQUARE,GF_TRIANGLE,GF_SAWTOOTH,GF_INVERSE_SAWTOOTH,GF_NOISE};
  warnings=0;for(int n=0;n<6;++n)assert(NameToGenFunc(names[n])==functions[n]);assert(!warnings);
  for(const char *invalid:{"","random7","random ","unknown"}){
   warnings=0;assert(NameToGenFunc(invalid)==GF_SIN&&warnings==1);
  }
 }
}
'''


class JK2ShaderWaveNamesTests(unittest.TestCase):
    def test_actual_wave_name_mapping_and_profile_isolation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
        comparison = function(shared, 'int Q_stricmpn(') + function(shared, 'int Q_stricmp(')
        scratch = ROOT / '.tmp/tests/jk2-shader-wave-names'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            for renderer in ('renderer', 'renderervk', 'rendererrtx'):
                with self.subTest(renderer=renderer):
                    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
                    record = re.search(r'typedef enum\s*{\s*GF_NONE,.*?}\s*genFunc_t;', local, re.S).group(0)
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    fixture = folder / (renderer + '.cpp')
                    fixture.write_text(PRELUDE + record + comparison + function(source, 'static genFunc_t NameToGenFunc(') + CHECKS)
                    executable = folder / (renderer + ('.exe' if os.name == 'nt' else ''))
                    target = compiler_target_args(compiler)
                    if os.name == 'nt' and not target:
                        target = ['-DFNTECH3_Q3_ONLY']
                    built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Werror', '-I', str(ROOT),
                                            str(fixture), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
