"""Exercise ET pure admission: proof names, loaded-pak names and profile scope."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

void QDECL Com_Error(errorParm_t, const char *, ...) { std::abort(); }
void QDECL Com_Printf(const char *, ...) {}
void QDECL Com_DPrintf(const char *, ...) {}

static char storage[256];
static int Filename(comGameId_t id, unsigned slot) {
 const auto *profile = Com_GameProfileById(id);
 return Com_GameManagerPureModuleFilename(profile, slot, "x86", ".dll",
  storage, sizeof(storage));
}

int main() {
 // Only the multiplayer profiles prove pure with two native module files.
 // RTCW single player has no pure admission at all.
 for (auto id : {COM_GAME_RTCW_MP, COM_GAME_WOLFET})
  assert(Com_PureProofIsNativeModulePair(Com_GameProfileById(id)));
 for (auto id : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP})
  assert(!Com_PureProofIsNativeModulePair(Com_GameProfileById(id)));
 assert(!Com_PureProofIsNativeModulePair(nullptr));

 // ET proves with the two retail module filenames its own engine obfuscates.
 assert(Filename(COM_GAME_WOLFET, 0) && !std::strcmp(storage, "cgame_mp_x86.dll"));
 assert(Filename(COM_GAME_WOLFET, 1) && !std::strcmp(storage, "ui_mp_x86.dll"));
 assert(!Filename(COM_GAME_WOLFET, 2));

 // RTCW MP names its own modules the same way; ET does not borrow them.
 assert(Filename(COM_GAME_RTCW_MP, 0) && !std::strcmp(storage, "cgame_mp_x86.dll"));
 assert(Filename(COM_GAME_RTCW_MP, 1) && !std::strcmp(storage, "ui_mp_x86.dll"));
 assert(!Filename(COM_GAME_RTCW_SP, 0) && !Filename(COM_GAME_QUAKELIVE, 1));

 // Retail ET defaults sv_pure on and does not latch it.
 const auto *et = Com_GameProfileById(COM_GAME_WOLFET);
 assert(!std::strcmp(et->server.pure, "1") && et->server.pureLatched == 0);
 return 0;
}
'''

# The two ET module names, recovered from the shifted strings in the retail
# engine's qcommon.h (SYS_DLLNAME_CGAME shifted by 2, SYS_DLLNAME_UI by 5).
SHIFTED = (('eicogaoraz:80fnn', 2, 'cgame_mp_x86.dll'),
           ('zndrud}=;3iqq', 5, 'ui_mp_x86.dll'),
           ('wgmgskesve~><4jrr', 6, 'qagame_mp_x86.dll'))


class WolfETPureTests(unittest.TestCase):
    def test_proof_names_profile_scope_and_defaults(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-next'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'pure.cpp'
            executable = folder/('pure.exe' if os.name == 'nt' else 'pure')
            source.write_text(PROBE, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT),
                str(source), str(ROOT/'code/qcommon/game_profile.cpp'), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT,
                capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_retail_shifted_module_names_decode_as_expected(self):
        """Pin the retail ET names our pure proof has to reproduce."""
        for shifted, shift, expected in SHIFTED:
            self.assertEqual(''.join(chr(ord(c) - shift) for c in shifted), expected)

    def test_pure_mode_is_no_longer_refused_for_et(self):
        text = (ROOT/'code/server/sv_init.cpp').read_text(encoding='utf-8')
        self.assertNotIn('WolfET pure admission is not implemented', text)

    def test_loaded_pure_names_match_checksum_selection(self):
        """The names must be built from the same paks, in the same order."""
        text = (ROOT/'code/qcommon/files.c').read_text(encoding='utf-8')
        bodies = {}
        for name in ('FS_LoadedPakChecksums', 'FS_LoadedPakPureNames'):
            start = text.index('const char *%s(' % name)
            bodies[name] = text[start:text.index('\n}', start)]
        for body in bodies.values():
            self.assertIn('for ( search = fs_searchpaths ; search ; search = search->next )', body)
            self.assertIn('if ( !search->pack )', body)
            self.assertIn('if ( search->pack->exclude )', body)
            self.assertIn('*overflowed = qtrue;', body)
        names = bodies['FS_LoadedPakPureNames']
        self.assertIn('search->pack->pakGamename', names)
        self.assertIn('search->pack->pakBasename', names)
        # Unlike the console-only FS_LoadedPakNames, this one must be available
        # to dedicated servers, so its definition sits outside that guard.
        guard = text.index('#ifndef DEDICATED\nconst char *FS_LoadedPakNames( void ) {')
        pure = text.index('const char *FS_LoadedPakPureNames(')
        self.assertLess(guard, pure)
        closing = text.index('#endif', guard)
        self.assertLess(closing, pure)
        self.assertNotIn('#ifndef DEDICATED', text[closing:pure])

    def test_only_et_publishes_loaded_pak_names(self):
        """Other profiles keep their existing empty sv_pakNames."""
        text = (ROOT/'code/server/sv_init.cpp').read_text(encoding='utf-8')
        self.assertIn('const bool publishNames = Com_GameIsWolfET();', text)
        start = text.index('const bool publishNames')
        block = text[start:text.index('\n\t}', start)]
        self.assertIn('nameslen', block)
        self.assertIn('namesOverflowed', block)
        self.assertRegex(block, r'if \( publishNames \) \{\s*\n\s*Cvar_Set\( "sv_pakNames", names \);')


if __name__ == '__main__':
    unittest.main()
