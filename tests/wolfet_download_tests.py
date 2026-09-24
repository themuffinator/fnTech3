"""Exercise the profile-aware official-pak policy that gates ET downloads."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

# Profile activation is immutable for a process, so the probe drives the real
# policy with small profile records instead of linking the catalog. The base
# game names those records use are pinned against the catalog separately.
PROBE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <initializer_list>

static comGameProfile_t record;
static comGameId_t active = COM_GAME_QUAKE3;

const comGameProfile_t *Com_GameProfile() { return &record; }
int Com_GameIsQuake3() { return active == COM_GAME_QUAKE3; }
int Com_GameIsQuakeLive() { return active == COM_GAME_QUAKELIVE; }
int Com_GameIsWolfET() { return active == COM_GAME_WOLFET; }
int Com_GameIsJKA() { return record.titleId == COM_GAME_TITLE_JKA; }

qboolean FS_FilenameCompare(const char *s1, const char *s2) {
 // Stands in for the engine comparison: case folded, separators equivalent.
 for (;; ++s1, ++s2) {
  int c1 = *s1, c2 = *s2;
  if (c1 >= 'a' && c1 <= 'z') c1 -= 'a' - 'A';
  if (c2 >= 'a' && c2 <= 'z') c2 -= 'a' - 'A';
  if (c1 == '\\') c1 = '/';
  if (c2 == '\\') c2 = '/';
  if (c1 != c2) return qtrue;
  if (!c1) return qfalse;
 }
}

int Q_stricmp(const char *a, const char *b) {
 return FS_FilenameCompare(a, b) ? 1 : 0;
}

static char vaBuffer[1024];
const char *va(const char *format, ...) {
 va_list arguments;
 va_start(arguments, format);
 std::vsnprintf(vaBuffer, sizeof(vaBuffer), format, arguments);
 va_end(arguments);
 return vaBuffer;
}
'''

CHECKS = r'''
static void select(comGameId_t id, const char *base, const char *stem = nullptr,
                   comGameTitleId_t title = COM_GAME_TITLE_QUAKE3) {
 active = id; record.id = id; record.baseGame = base;
 record.officialPakStem = stem; record.titleId = title;
}

static void official(std::initializer_list<const char *> yes,
                     std::initializer_list<const char *> no) {
 for (const char *pak : yes) assert(FS_OfficialPak(pak));
 for (const char *pak : no) assert(!FS_OfficialPak(pak));
}

int main() {
 // Each profile refuses the numbered paks of its own base game, and no other's.
 select(COM_GAME_QUAKE3, "baseq3");
 official({"baseq3/pak0", "baseq3/pak8", "missionpack/pak0", "missionpack/pak3",
           "BASEQ3\\PAK1"},
          {"baseq3/pak9", "baseq3/pak00", "baseq3/mp_bin", "etmain/pak0",
           "main/pak0", "baseq3/myaddon", "missionpack/pak4", ""});

 // Retail QL's two-digit pak is official; the same name stays open for Q3.
 // QL shares Quake's base directory, so Team Arena's paks stay refused there.
 select(COM_GAME_QUAKELIVE, "baseq3");
 official({"baseq3/pak00", "baseq3/pak0", "missionpack/pak0", "missionpack/pak3"},
          {"missionpack/pak4", "baseq3/mp_bin", "etmain/pak0"});

 // ET refuses etmain's numbered paks and the module pak it ships by name.
 select(COM_GAME_WOLFET, "etmain");
 official({"etmain/pak0", "etmain/pak1", "etmain/pak2", "etmain/pak8",
           "etmain/mp_bin", "ETMAIN/MP_BIN", "etmain\\pak0"},
          {"etmain/pak9", "etmain/pak00", "etmain/mp_bin2", "etmain/mymod",
           "baseq3/pak0", "missionpack/pak0", "etpub/pak0"});

 // RTCW shares one base directory across both of its profiles.
 select(COM_GAME_RTCW_MP, "main");
 official({"main/pak0", "main/pak2"},
          {"main/mp_bin", "main/pak9", "baseq3/pak0", "etmain/pak0"});
 select(COM_GAME_RTCW_SP, "main");
 official({"main/pak0"}, {"main/mp_bin", "missionpack/pak0", "baseq3/pak0"});

 // Elite Force's two modes share baseEF and its Raven paks, including the
 // Expansion Pack's pak3; Quake's Team Arena paks are not theirs.
 for (auto id : {COM_GAME_STVEF_SP, COM_GAME_STVEF_MP}) {
  select(id, "baseEF");
  official({"baseEF/pak0", "baseEF/pak2", "BASEEF\\PAK3"},
           {"baseEF/pak9", "baseq3/pak0", "missionpack/pak0", "main/pak0"});
 }

 // Jedi Academy's modes share base/ and Raven's assets0-assets8 naming. Its
 // own rule refuses that set under missionpack too, and leaves Quake's
 // numbered names free for a mod to offer.
 for (auto id : {COM_GAME_JKA_SP, COM_GAME_JKA_MP, COM_GAME_JK2_SP, COM_GAME_JK2_MP}) {
  select(id, "base", "assets", COM_GAME_TITLE_JKA);
  official({"base/assets0", "base/assets8", "BASE\\ASSETS3",
            "missionpack/assets0", "missionpack/assets8"},
           {"base/assets9", "base/assets00", "base/pak0", "missionpack/pak0",
            "baseq3/assets0", "japlus/assets0", "base/mymod", ""});
 }

 // An empty or absent name is never official, and a profile with no base game
 // falls back to the compile-time directory rather than matching everything.
 for (auto id : {COM_GAME_QUAKE3, COM_GAME_WOLFET, COM_GAME_RTCW_MP}) {
  select(id, id == COM_GAME_WOLFET ? "etmain" : id == COM_GAME_RTCW_MP ? "main" : "baseq3");
  assert(!FS_OfficialPak(nullptr) && !FS_OfficialPak(""));
 }
 return 0;
}
'''


def _catalog_base_games():
    text = (ROOT/'code/qcommon/game_profile.cpp').read_text(encoding='utf-8')
    found = {}
    for match in re.finditer(r'\tCOM_GAME_(\w+),\n\t"[^"]*",\n\t"[^"]*",\n\t"([^"]*)",', text):
        found[match.group(1)] = match.group(2)
    return found


class WolfETDownloadTests(unittest.TestCase):
    def test_official_pak_policy_is_profile_owned(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        text = (ROOT/'code/qcommon/files.c').read_text(encoding='utf-8')
        begin = text.index('qboolean FS_OfficialPak( const char *pak )')
        body = text[begin:text.index('\n}', begin)+2]
        scratch = ROOT/'.tmp/wolfet-next'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'official.cpp'
            executable = folder/('official.exe' if os.name == 'nt' else 'official')
            source.write_text(PROBE + body + CHECKS, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT),
                str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT,
                capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_probe_base_games_match_the_catalog(self):
        """The policy above is only meaningful if these are the real directories."""
        self.assertEqual(_catalog_base_games(), {
            'QUAKE3': 'baseq3', 'QUAKELIVE': 'baseq3', 'RTCW_SP': 'main',
            'RTCW_MP': 'main', 'WOLFET': 'etmain', 'STVEF_SP': 'baseEF',
            'STVEF_MP': 'baseEF', 'QUAKE3_IHV': 'baseq3', 'JKA_SP': 'base',
            'JKA_MP': 'base', 'JK2_SP': 'base', 'JK2_MP': 'base',
            'ALICE': 'base'})

    def test_download_paths_use_the_profile_policy(self):
        """Neither the server nor the client may fall back to Quake's BASEGAME."""
        server = (ROOT/'code/server/sv_client.cpp').read_text(encoding='utf-8')
        begin = server.index('bool idPack = false;')
        block = server[begin:begin+3000]
        self.assertIn('FS_OfficialPak( pakbuf.data() )', block)
        self.assertNotIn('FS_idPak( pakbuf.data(), BASEGAME', block)

        files = (ROOT/'code/qcommon/files.c').read_text(encoding='utf-8')
        marker = files.index("never autodownload any of the profile's official paks")
        self.assertIn('FS_OfficialPak( pakName )', files[marker:marker+200])


if __name__ == '__main__':
    unittest.main()
