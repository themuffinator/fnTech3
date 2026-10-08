"""The IHV Test's serverinfo is the original's key set (server_q3_ihv_net.hpp).

quake3.exe 4.01's SV_Init (0x419a40) gives its serverinfo bit (8) to
timelimit, cheats, protocol, mapname, privateclients, hostname and
sv_maxclients; the game adds its own serverinfo cvars. A client of the
original server reads them as teamflags, maxclients, gamedate, gamename,
sv_maxclients, privateclients, mapname, protocol, timelimit, fraglimit,
dmflags, version, hostname and cheats, in that order. fnTech3's own
serverinfo cvars have no IHV counterpart and stay out.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
NET = ROOT / 'code/server/server_q3_ihv_net.hpp'

# What a client of the original server read on tim_dm1 (2026-10-08,
# docs/fntech3/2026-10-08-q3-ihv-serverinfo.md); only the version differs.
ORIGINAL = ('\\teamflags\\0\\maxclients\\8\\gamedate\\Feb 12 1999\\gamename\\baseq3\\sv_maxclients\\8'
            '\\privateclients\\0\\mapname\\tim_dm1\\protocol\\33\\timelimit\\0\\fraglimit\\0\\dmflags\\0'
            '\\version\\4.01 x86 Feb 12 1999 RELEASE\\hostname\\noname\\cheats\\1')

PRELUDE = r'''
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#define MAX_INFO_STRING 1024
#define CVAR_SERVERINFO 4
static int Q_stricmp(const char *a, const char *b) {
 while (*a && *b && std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b)) { ++a; ++b; }
 return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}
namespace fntech3::q3ihv { inline int CvarFlagsToEngine(int flags) { return flags & 8 ? CVAR_SERVERINFO : 0; } }
struct IhvGameCvar { char name[256]; int flags; IhvGameCvar *next; };
static IhvGameCvar *ihvGameCvars = nullptr;
static std::map<std::string, std::string> cvars;
static const char *Cvar_VariableString(const char *name) {
 auto it = cvars.find(name); return it == cvars.end() ? "" : it->second.c_str();
}
static const char *Info_ValueForKey(const char *s, const char *key) {
 static std::string value; const std::string text(s), needle = std::string("\\") + key + "\\";
 for (size_t at = 0; (at = text.find(needle, at)) != std::string::npos; ++at) {
  size_t count = 0; for (size_t i = 0; i < at; ++i) count += text[i] == '\\';
  if (count % 2) continue;
  const size_t start = at + needle.size(), end = text.find('\\', start);
  value = text.substr(start, end == std::string::npos ? std::string::npos : end - start); return value.c_str();
 }
 return "";
}
static void Info_SetValueForKey(char *s, const char *key, const char *value) {
 assert(!Info_ValueForKey(s, key)[0]);
 if (value && *value) { std::strcat(s, "\\"); std::strcat(s, key); std::strcat(s, "\\"); std::strcat(s, value); }
}
'''


class Q3IHVServerinfoTests(unittest.TestCase):
    def execute(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ihv-serverinfo-', dir=ROOT / '.tmp') as folder:
            folder = Path(folder)
            src, exe = folder / 'test.cpp', folder / 'test.exe'
            src.write_text(source, encoding='utf-8')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(src), '-o', str(exe)], cwd=ROOT, text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            return run.stdout

    def test_serverinfo_is_the_originals_key_set(self):
        body = function(NET.read_text(encoding='utf-8'), 'const char *SV_IHVServerinfo( void )')
        expected = ORIGINAL.replace('4.01 x86 Feb 12 1999 RELEASE', 'fnTech3 test') + '\\g_extra\\7'
        out = self.execute(PRELUDE + body + r'''
static IhvGameCvar game[] = {
 // The build's game registers these; newest first, as its list holds them.
 {"g_extra", 8, nullptr}, {"g_speed", 0, nullptr}, {"dmflags", 8, nullptr}, {"fraglimit", 8, nullptr},
 {"timelimit", 8, nullptr}, {"cheats", 0, nullptr}, {"gamename", 8, nullptr}, {"gamedate", 8, nullptr},
 {"maxclients", 8, nullptr}, {"teamflags", 8, nullptr},
};
int main() {
 for (size_t i = 0; i + 1 < sizeof game / sizeof *game; ++i) game[i].next = &game[i + 1];
 ihvGameCvars = game;
 cvars = { {"teamflags", "0"}, {"maxclients", "8"}, {"gamedate", "Feb 12 1999"}, {"gamename", "baseq3"},
  {"sv_maxclients", "8"}, {"privateclients", "0"}, {"mapname", "tim_dm1"}, {"protocol", "33"},
  {"timelimit", "0"}, {"fraglimit", "0"}, {"dmflags", "0"}, {"version", "fnTech3 test"},
  {"hostname", "noname"}, {"cheats", "1"}, {"g_speed", "320"}, {"g_extra", "7"},
  // This engine's own serverinfo cvars, absent from the IHV Test's.
  {"sv_hostname", "noname"}, {"sv_privateClients", "0"}, {"sv_allowDownload", "1"}, {"sv_dlRate", "100"},
  {"sv_floodProtect", "1"}, {"sv_minRate", "0"}, {"sv_maxRate", "0"}, {"sv_playingDemo", "0"},
  {"g_gametype", "0"} };
 std::printf("%s", SV_IHVServerinfo());
 // A key without a value is left out, as the original's empty cvars are.
 cvars["hostname"] = "";
 assert(!std::strstr(SV_IHVServerinfo(), "hostname"));
}
''')
        self.assertEqual(out, expected)

    def test_every_serverinfo_reader_takes_it(self):
        net = NET.read_text(encoding='utf-8')
        self.assertIn('ihvsv::StatusReply( Cmd_Argv( 1 ), SV_IHVServerinfo(), players )', net)
        self.assertIn('Cvar_Get( "hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE );', net)
        self.assertIn('Cvar_Get( "privateclients", "0", CVAR_SERVERINFO );', net)
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
        override = init.index('} else if ( Com_GameIsQuake3IHV() && index == CS_SERVERINFO ) {')
        self.assertIn('val = SV_IHVServerinfo();', init[override:override + 160])
        self.assertLess(override, init.index('// don\'t bother broadcasting an update if no change'))
        self.assertIn('const char *SV_IHVServerinfo( void );',
                      (ROOT / 'code/server/server.h').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
