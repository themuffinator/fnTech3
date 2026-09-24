"""Each profile's default player name is its own engine's.

The client registers `name` with the profile's appearance.playerName, or
Quake III's UnnamedPlayer when that is NULL. RTCW and Enemy Territory had
NULL, so a fresh home joined an RTCW MP game as UnnamedPlayer where retail
WolfMP.exe joins the same map as WolfPlayer (observed live with
scripts/probe_rtcw_ripple.py --retail). WolfSP.exe registers "Player" and
et.exe "ETPlayer", in their GPL sources and as strings in the executables.

The test compiles the real profile catalog and reads every profile's name.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

SOURCE = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include "game_profile.h"

static const char *Name( comGameId_t id ) {
	const comGameProfile_t *profile = Com_GameProfileById( id );
	assert( profile );
	return profile->appearance.playerName ? profile->appearance.playerName : "UnnamedPlayer";
}

static void Expect( comGameId_t id, const char *name ) {
	if ( std::strcmp( Name( id ), name ) ) {
		std::fprintf( stderr, "profile %u: %s, expected %s\n", (unsigned)id, Name( id ), name );
		assert( false );
	}
}

int main() {
	Expect( COM_GAME_QUAKE3, "UnnamedPlayer" );
	Expect( COM_GAME_QUAKELIVE, "UnnamedPlayer" );
	Expect( COM_GAME_RTCW_SP, "Player" );
	Expect( COM_GAME_RTCW_MP, "WolfPlayer" );
	Expect( COM_GAME_WOLFET, "ETPlayer" );
	Expect( COM_GAME_STVEF_SP, "Munro" );
	return 0;
}
'''


class ProfilePlayerNameTests(unittest.TestCase):
    def test_each_profile_registers_its_engines_default_name(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='player-name-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(SOURCE, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT / 'code/qcommon'),
                                    str(cpp), str(ROOT / 'code/qcommon/game_profile.cpp'), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
