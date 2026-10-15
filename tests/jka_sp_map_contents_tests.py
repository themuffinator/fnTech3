"""Execute the production Raven SP map-wide collision-contents getter."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args, configured_target_is_windows_x64
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/cm_local.h"
#include <cassert>
#include <cstring>
#include "code/qcommon/game_profile.h"
clipMap_t cm{};
static comGameId_t selectedProfile = COM_GAME_QUAKE3;
const comGameProfile_t *Com_GameProfile(void) {
 static comGameProfile_t profile{};profile.id=selectedProfile;return &profile;
}
int Com_GameIsJKASP(void) { return selectedProfile == COM_GAME_JKA_SP; }
'''
CHECKS = r'''
int main(){
 // All supported profiles retain their own initial seed. Profile selection
 // is fixed before each production process begins; this fixture enumerates it.
 for(unsigned profile=COM_GAME_AUTO;profile<COM_GAME_COUNT;++profile){
  selectedProfile=static_cast<comGameId_t>(profile);
  cm={};
  const int body=selectedProfile==COM_GAME_JKA_SP||selectedProfile==COM_GAME_JK2_MP
   ||selectedProfile==COM_GAME_JKA_MP ? 0x100 : CONTENTS_BODY;
  assert(CM_TotalMapContents()==body);
  cbrush_t brushes[2]{};brushes[0].contents=CONTENTS_SOLID;brushes[1].contents=CONTENTS_WATER;
  cPatch_t patch{};patch.contents=0x40000000;
  cPatch_t *surfaces[3]{nullptr,&patch,nullptr};
  cm.numBrushes=2;cm.brushes=brushes;cm.numSurfaces=3;cm.surfaces=surfaces;
  assert(CM_TotalMapContents()==(body|CONTENTS_SOLID|CONTENTS_WATER|0x40000000));
  const auto before=cm;const auto first=brushes[0];const auto second=brushes[1];const auto curve=patch;
  (void)CM_TotalMapContents();
  assert(!memcmp(&before,&cm,sizeof(cm))&&!memcmp(&first,&brushes[0],sizeof(first)));
  assert(!memcmp(&second,&brushes[1],sizeof(second))&&!memcmp(&curve,&patch,sizeof(curve)));
  // Genuine native flag bits are retained even when they equal another
  // profile's BODY bit. Never strip CONTENTS_BODY from the aggregate.
  brushes[1].contents=CONTENTS_BODY;patch.contents=0;
  assert(CM_TotalMapContents()==(body|CONTENTS_SOLID|CONTENTS_BODY));
  brushes[1].contents=0;
  assert(CM_TotalMapContents()==(body|CONTENTS_SOLID));
  cm={};assert(CM_TotalMapContents()==body);
 }
}
'''


class JKASPMapContentsTests(unittest.TestCase):
    def test_map_query_uses_loaded_brushes_patches_and_body(self):
        compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/qcommon/cm_test.c').read_text(encoding='utf-8')
        actual = function(source, 'int CM_TotalMapContents( void )')
        temporary_root = ROOT / '.tmp/jka-compat'
        temporary_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='map-contents-', dir=temporary_root) as folder:
            work = Path(folder).resolve()
            self.assertTrue(work.is_relative_to(temporary_root.resolve()))
            cpp = work / 'probe.cpp'
            exe = work / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(PRELUDE + actual + CHECKS, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = (['-DFNTECH3_Q3_ONLY'] if configured_target_is_windows_x64()
                          else ['--target=i686-pc-windows-msvc'] if 'clang' in Path(compiler).name.lower()
                          else [])
            subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp),
                            '-o', str(exe)], check=True, cwd=ROOT, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, cwd=ROOT, capture_output=True, text=True)
            # Every generated entry must stay disposable and inside this test's
            # verified directory before TemporaryDirectory removes it.
            for entry in work.rglob('*'):
                self.assertFalse(entry.is_symlink())
                self.assertTrue(entry.resolve().is_relative_to(work))


if __name__ == '__main__':
    unittest.main()
