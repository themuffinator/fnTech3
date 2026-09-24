"""Which variable changes make map_restart reload the level, for every profile.

Retail WolfMP.exe (SV_MapRestart_f 0x432eb0) reloads only when sv_maxclients
changed (0x433016..0x43301f), as do et.exe (0x43faf6..0x43faff) and etded.exe
(0x425f64..0x425f6d), and each prints "sv_maxclients variable change --
restarting.". Every other profile keeps its own check. This compiles the
production check out of SV_MapRestart_f and runs it for all thirteen profiles
against each combination of changed cvars.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
enum { Q3, QL, RTCW_SP, RTCW_MP, WOLFET, STVEF_SP, STVEF_MP, Q3_IHV, JKA_SP, JKA_MP, JK2_SP, JK2_MP, ALICE, PROFILES };
static int profile;
int Com_GameIsQuakeLive(){return profile==QL;}
int Com_GameIsRTCWMP(){return profile==RTCW_MP;}
int Com_GameIsWolfET(){return profile==WOLFET;}
struct Cvar { int modified; };
static Cvar maxclients, gametype, pure;
static Cvar *sv_maxclients=&maxclients, *sv_gametype=&gametype, *sv_pure=&pure;
// Holomatch's Expansion Pack modes; production reports them for stvef-mp only.
static bool modes;
bool SV_STVEFModesModified(){return profile==STVEF_MP&&modes;}
static constexpr int MAX_QPATH=64, qfalse=0;
template<typename T,size_t N>int SV_ArraySize(const std::array<T,N>&){return static_cast<int>(N);}
void Q_strncpyz(char *out,const char *in,int capacity){std::snprintf(out,capacity,"%s",in);}
const char *Cvar_VariableString(const char *name){assert(std::string(name)=="mapname");return "mp_beach";}
static std::vector<std::string> spawns, printed;
void SV_SpawnServer(const char *map,int killBots){assert(!killBots);spawns.push_back(map);}
void Com_Printf(const char *text,...){printed.push_back(text);}
static bool inPlace;
'''
CHECKS = r'''
int main(){
 // Bit n of a mask is set when combination n reloads the level. A combination
 // sets bit 0 for sv_maxclients, 1 for g_gametype, 2 for sv_pure and 3 for the
 // Holomatch modes.
 constexpr unsigned wolfenstein=0xAAAA, quake=0xFEFE, holomatch=0xFFFE, none=0;
 const unsigned expected[PROFILES]={quake,none,quake,wolfenstein,wolfenstein,quake,holomatch,
  quake,quake,quake,quake,quake,quake};
 for(int p=0;p<PROFILES;++p)for(int bits=0;bits<16;++bits){
  profile=p;maxclients.modified=bits&1;gametype.modified=bits>>1&1;pure.modified=bits>>2&1;modes=bits>>3&1;
  spawns.clear();printed.clear();inPlace=false;
  RestartCheck();
  const bool reload=expected[p]>>bits&1;
  assert(spawns.size()==(reload?1u:0u)&&inPlace==!reload);
  if(!reload){assert(printed.empty());continue;}
  assert(spawns[0]=="mp_beach"&&printed.size()==1);
  assert(printed[0]==(p==RTCW_MP||p==WOLFET?"sv_maxclients variable change -- restarting.\n"
   :"variable change -- restarting.\n"));
 }
}
'''
# Each mutation must make the checks fail.
MUTATIONS = {
    'every profile keeps the shared check': (
        'const bool clientCountOnly = Com_GameIsRTCWMP() || Com_GameIsWolfET();',
        'const bool clientCountOnly = false;'),
    'Enemy Territory keeps the shared check': (
        'const bool clientCountOnly = Com_GameIsRTCWMP() || Com_GameIsWolfET();',
        'const bool clientCountOnly = Com_GameIsRTCWMP();'),
    'RTCW MP keeps the shared check': (
        'const bool clientCountOnly = Com_GameIsRTCWMP() || Com_GameIsWolfET();',
        'const bool clientCountOnly = Com_GameIsWolfET();'),
    'every profile ignores game type and pure mode': (
        'const bool clientCountOnly = Com_GameIsRTCWMP() || Com_GameIsWolfET();',
        'const bool clientCountOnly = !Com_GameIsQuakeLive();'),
    'no client-count reload in RTCW MP or ET': (
        'sv_maxclients->modified || ( !clientCountOnly',
        '( !clientCountOnly'),
    'the shared message': (
        '"sv_maxclients variable change -- restarting.\\n"',
        '"variable change -- restarting.\\n"'),
}


def restart_check(command):
    """The production check, from its flag to the end of its reload block."""
    start = command.index('\n\tconst bool clientCountOnly')
    condition = command.index('\n\tif ( !Com_GameIsQuakeLive()', start)
    end = command.index('\n\t}\n', condition) + len('\n\t}\n')
    return command[start:end]


class RestartVariableChangeTests(unittest.TestCase):
    def run_checks(self, check, folder, name):
        source = PRELUDE + 'static void RestartCheck() {' + check + '\tinPlace = true;\n}\n' + CHECKS
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(source)
        built = subprocess.run([self.compiler, *compiler_target_args(self.compiler), '-std=c++17',
                                str(cpp), '-o', str(exe)], cwd=folder, capture_output=True, text=True)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        return subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)

    def test_only_rtcw_mp_and_et_reload_for_client_count_alone(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler unavailable')
        command = function((ROOT / 'code/server/sv_ccmds.cpp').read_text(), 'static void SV_MapRestart_f(')
        check = restart_check(command)
        # Retail refuses a match-state transition before it looks at any cvar,
        # and only a restart that gets past the check toggles the snapshot bit.
        self.assertLess(command.rindex('RestartMatchState'), command.index('const bool clientCountOnly'))
        self.assertLess(command.index('const bool clientCountOnly'), command.index('svs.snapFlagServerBit ^='))
        scratch = ROOT / '.tmp/restart-variable-change/tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='check-', dir=scratch) as directory:
            folder = Path(directory)
            ran = self.run_checks(check, folder, 'production')
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            for index, (description, (old, new)) in enumerate(MUTATIONS.items()):
                with self.subTest(mutation=description):
                    self.assertEqual(check.count(old), 1, old)
                    mutated = self.run_checks(check.replace(old, new), folder, f'mutation{index}')
                    self.assertNotEqual(mutated.returncode, 0, description)


if __name__ == '__main__':
    unittest.main()
