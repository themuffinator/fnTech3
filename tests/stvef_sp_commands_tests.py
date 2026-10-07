"""Exercise server command registration and nextmap across the first seven profiles.

Quake III to Elite Force Holomatch; the Ritual engines and Medal of Honor
register their own command sets, which their own tests cover.
"""
from pathlib import Path
import itertools
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if not depth:
                return source[start:i + 1]
    raise AssertionError(signature)


HARNESS = r'''
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include "code/qcommon/game_profile.h"
#undef assert
#define assert(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using xcommand_t = void (*)();
using qboolean = int;
static comGameProfile_t profile{};
static int standalone;
int Com_GameIsSTVEFSP() { return profile.id==COM_GAME_STVEF_SP; }
int Com_GameIsRTCWSP() { return profile.id==COM_GAME_RTCW_SP; }
int Com_GameIsRTCWMP() { return profile.id==COM_GAME_RTCW_MP; }
int Com_GameIsWolfET() { return profile.id==COM_GAME_WOLFET; }
int Com_GameIsSTVEFMP() { return profile.id==COM_GAME_STVEF_MP; }
int Com_GameIsJKAMP() { return profile.id==COM_GAME_JKA_MP; }
int Com_GameIsJK2MP() { return profile.id==COM_GAME_JK2_MP; }
int Com_GameIsQuakeLive() { return profile.id==COM_GAME_QUAKELIVE; }
int Com_GameIsRitual() { return profile.id==COM_GAME_ALICE || profile.id==COM_GAME_FAKK2; }
int Com_GameIsMOHAA() { return profile.id==COM_GAME_MOHAA; }
static void SV_AliceAddCommands() { assert(false); }
static void SV_MOHAAAddCommands() { assert(false); }
const comGameProfile_t *Com_GameProfile() { return &profile; }
static std::map<std::string,xcommand_t> commands,completions;
static std::map<std::string,std::string> cvars;
static int gets;
static void *stvefSpTestSave;
static void Cmd_AddCommand(const char *name,xcommand_t callback) {
    assert(callback && !commands.count(name)); commands[name]=callback;
}
static void Cmd_SetCommandCompletionFunc(const char *name,xcommand_t callback) {
    assert(commands.count(name) && !completions.count(name)); completions[name]=callback;
}
static int Cvar_VariableIntegerValue(const char *name) {
    assert(!std::strcmp(name,"com_standalone")); return standalone;
}
static void *Cvar_Get(const char *name,const char *value,int flags) {
    assert(!std::strcmp(name,"sv_testsave") && !std::strcmp(value,"0") && flags==0);
    ++gets; return &gets;
}
static void Cvar_Set(const char *name,const char *value) { cvars[name]=value; }
static const char *va(const char *format,const char *value) {
    static char buffer[1024]; std::snprintf(buffer,sizeof(buffer),format,value); return buffer;
}
STUBS
HELPERS
SP_COMMANDS
OPERATOR_COMMANDS
NEXT_MAP_FUNCTION
static void Expect(const char *name,xcommand_t callback,bool enabled=true) {
    assert((commands.count(name)!=0)==enabled);
    if(enabled) assert(commands.at(name)==callback);
}
int main(int argc,char **argv) {
    assert(argc==3); profile.id=static_cast<comGameId_t>(std::atoi(argv[1]));
    profile.supportsWorkshop=Com_GameIsQuakeLive(); standalone=std::atoi(argv[2]);
    SV_AddOperatorCommands();
    Expect("map_restart",SV_MapRestart_f,!Com_GameIsSTVEFSP());
    Expect("map",Com_GameIsSTVEFSP()?SV_STVEFSPMap_f:SV_Map_f);
    for(const char *name: {"maptransition","loadtransition","save","load","wipe"})
        assert((commands.count(name)!=0)==(Com_GameIsSTVEFSP()!=0));
    if(Com_GameIsSTVEFSP()) {
        Expect("devmap",SV_STVEFSPMap_f);
        Expect("maptransition",SV_STVEFSPMapTransition_f);
        Expect("loadtransition",SV_STVEFSPLoadTransition_f);
        Expect("save",SV_STVEFSPSave_f); Expect("load",SV_STVEFSPLoad_f);
        Expect("wipe",SV_STVEFSPWipe_f);
        assert(!commands.count("spmap") && !commands.count("spdevmap") && gets==1);
        assert(completions.size()==4);
    } else {
#ifndef PRE_RELEASE_DEMO
        Expect("devmap",SV_Map_f); Expect("spmap",SV_Map_f); Expect("spdevmap",SV_Map_f);
        assert(completions.size()==4);
#else
        assert(!commands.count("devmap") && !commands.count("spmap") && !commands.count("spdevmap"));
        assert(completions.size()==1);
#endif
        assert(gets==0);
    }
    for(const auto &entry:completions) assert(entry.second==SV_CompleteMapName);
    Expect("loadgame",SV_LoadGame_f,Com_GameIsRTCWSP()!=0);
    Expect("gameCompleteStatus",SV_GameCompleteStatus_f,Com_GameIsRTCWMP()||Com_GameIsWolfET());
    Expect("svsay",SV_ConSvSay_f,Com_GameIsSTVEFMP()!=0);
    Expect("steam_downloadugc",SV_SteamDownloadUGC_f,Com_GameIsQuakeLive()!=0);
    Expect("steam_subscribeugc",SV_SteamSubscribeUGC_f,Com_GameIsQuakeLive()!=0);
    Expect("steam_unsubscribeugc",SV_SteamUnsubscribeUGC_f,Com_GameIsQuakeLive()!=0);
    Expect("reload_arenas",SV_ArenaReload_f,Com_GameIsQuakeLive()!=0);
    Expect("reload_factories",SV_FactoryReload_f,Com_GameIsQuakeLive()!=0);
    Expect("reload_mappool",SV_MapPoolReload_f,Com_GameIsQuakeLive()!=0);
    Expect("startRandomMap",SV_StartRandomMap_f,Com_GameIsQuakeLive()!=0);
    bool bans=false,accountBans=false;
#ifdef USE_BANS
    bans=!Com_GameIsSTVEFSP();
#ifndef STANDALONE
    accountBans=!standalone;
#endif
#endif
    for(const char *name: {"rehashbans","listbans","banaddr","exceptaddr","bandel","exceptdel","flushbans"})
        assert((commands.count(name)!=0)==bans);
    assert((commands.count("banUser")!=0)==accountBans && (commands.count("banClient")!=0)==accountBans);
    const auto oldCommands=commands,oldCompletions=completions; const int oldGets=gets;
    SV_AddOperatorCommands();
    assert(commands==oldCommands && completions==oldCompletions && gets==oldGets);
    for(const char *map: {MAP_INPUTS}) {
        cvars["nextmap"]="echo previous continuation";
        SetNextMap(map,0);
        assert(cvars.size()==1);
        assert(cvars.at("nextmap")== (Com_GameIsSTVEFSP()?std::string("map ")+map:"map_restart 0"));
    }
}
'''


class STVEFSPCommandsTests(unittest.TestCase):
    def test_profile_commands_and_nextmap(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        ccmds = (ROOT / 'code/server/sv_ccmds.cpp').read_text()
        save = (ROOT / 'code/server/server_stvef_sp_save.hpp').read_text()
        init = (ROOT / 'code/server/sv_init.cpp').read_text()
        operator = function(ccmds, 'void SV_AddOperatorCommands( void )')
        sp_commands = function(save, 'void SV_STVEFSPAddCommands( void )')
        start = ccmds.index('struct operatorCommand_t {')
        helpers = ccmds[start:ccmds.index('\n\n/*', start)]
        spawn = function(init, 'void SV_SpawnServer(')
        start = spawn.index('// set nextmap to the same map')
        next_map = spawn[start:spawn.index('// try to reset level time', start)]
        next_map_function = spawn[:spawn.index('{')].replace('SV_SpawnServer', 'SetNextMap')
        next_map_function += '{\n' + next_map + '\n}'
        # Empty callbacks are only identities: registration must route each
        # command to the appropriate real function, without executing a game.
        names = set(re.findall(r'\bSV_\w+_f\b', operator + sp_commands))
        names.add('SV_CompleteMapName')
        stubs = '\n'.join(f'static void {name}() {{}}' for name in sorted(names))
        fixture = json.loads((ROOT / 'tests/fixtures/stvef_sp_commands_retail.json').read_text())
        native = [row['name'] for row in fixture['observation']['commands']]
        self.assertNotIn('map_restart', native)
        self.assertEqual(fixture['observation']['secondCallCount'], 0)
        maps = [row['value'].removeprefix('map ') for row in fixture['observation']['nextMaps']]
        self.assertTrue(all(row['name'] == 'nextmap' and row['value'].startswith('map ')
                            for row in fixture['observation']['nextMaps']))
        harness = HARNESS.replace('STUBS', stubs).replace('HELPERS', helpers)
        harness = harness.replace('SP_COMMANDS', sp_commands).replace('OPERATOR_COMMANDS', operator)
        harness = harness.replace('NEXT_MAP_FUNCTION', next_map_function)
        harness = harness.replace('MAP_INPUTS', ','.join(map(json.dumps, maps)))
        sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        with tempfile.TemporaryDirectory(prefix='stvef-commands-') as folder:
            cpp = Path(folder) / 'probe.cpp'
            cpp.write_text(harness)
            for flags in itertools.product((False, True), repeat=3):
                # One executable per build: Windows can hold the last one run.
                name = 'probe-' + ''.join('1' if on else '0' for on in flags)
                exe = Path(folder) / (name + '.exe' if os.name == 'nt' else name)
                defines = ['-D' + name for name, on in zip(('PRE_RELEASE_DEMO', 'USE_BANS', 'STANDALONE'), flags) if on]
                built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1',
                    '-D_CRT_SECURE_NO_WARNINGS', *sanitizer, *defines, '-I', str(ROOT), str(cpp), '-o', str(exe)],
                    cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                for profile, standalone in itertools.product(range(1, 8), range(2)):
                    with self.subTest(flags=flags, profile=profile, standalone=standalone):
                        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                        ran = subprocess.run([str(exe), str(profile), str(standalone)], cwd=ROOT,
                            capture_output=True, text=True, timeout=15, **options)
                        self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
