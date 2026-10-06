"""Quake Live's server type decides which remote players a server hears.

Retail quakelive_steam.exe never reads g_gametype or ui_singlePlayerActive
when it answers a packet: its SV_PacketEvent (0x4e4500) drops every packet
from anywhere but its own client while sv_serverType is 0 (offline), and its
SV_GetChallenge (0x4df430) refuses a challenge from outside the LAN while it is
1 (docs/fntech3/2026-10-06-ql-listen-server-type.md).

`SV_ServerTypeDropsPacket` and `SV_ServerTypeRefusesChallenge` are compiled
from code/server/sv_main.cpp, in a client and a dedicated build, and run for
every profile, server types -1 to 3 and four kinds of address. The table they
print must match retail's rules for Quake Live and change nothing for any other
profile; a dedicated build, which no retail binary here settles, never drops.
Each mutation must change the table. The source checks pin where the engine
asks them.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/server/server.h"
#include <cstdio>
#include <cstring>
static int game, serverType, cvarReads;
int Com_GameIsQuakeLive(){return game==COM_GAME_QUAKELIVE;}
int Cvar_VariableIntegerValue(const char *name){
 if(std::strcmp(name,"sv_serverType")){std::printf("unexpected cvar %s\n",name);return -99;}
 ++cvarReads;return serverType;
}
// The engine's LAN test as the fixture needs it: loopback, and 192.168/16.
qboolean Sys_IsLANAddress(const netadr_t *adr){
 if(adr->type==NA_LOOPBACK)return qtrue;
 return adr->type==NA_IP&&adr->ipv._4[0]==192&&adr->ipv._4[1]==168?qtrue:qfalse;
}
'''

CHECKS = r'''
int main(){
 std::printf("ids %d %d\n",int(COM_GAME_QUAKELIVE),int(COM_GAME_COUNT)-int(COM_GAME_QUAKE3));
 // 0 the local client, 1 a bot, 2 a LAN address, 3 an Internet address
 netadr_t addresses[4]{};
 addresses[0].type=NA_LOOPBACK;addresses[1].type=NA_BOT;
 addresses[2].type=NA_IP;addresses[2].ipv._4[0]=192;addresses[2].ipv._4[1]=168;addresses[2].ipv._4[3]=7;
 addresses[3].type=NA_IP;addresses[3].ipv._4[0]=8;addresses[3].ipv._4[1]=8;addresses[3].ipv._4[2]=8;
 addresses[3].ipv._4[3]=8;
 for(game=COM_GAME_QUAKE3;game<COM_GAME_COUNT;++game)for(serverType=-1;serverType<=3;++serverType)
  for(int kind=0;kind<4;++kind){
   cvarReads=0;
   const int drop=SV_ServerTypeDropsPacket(&addresses[kind]),refuse=SV_ServerTypeRefusesChallenge(&addresses[kind]);
   std::printf("%d %d %d %d %d %d\n",game,serverType,kind,drop,refuse,cvarReads);
  }
 return 0;
}
'''

DROPS = 'qboolean SV_ServerTypeDropsPacket( const netadr_t *from ) {'
REFUSES = 'qboolean SV_ServerTypeRefusesChallenge( const netadr_t *from ) {'

# Each mutation replaces text in one production function and must change the table.
MUTATIONS = {
    'an offline server hears a LAN': (
        DROPS, 'Cvar_VariableIntegerValue( "sv_serverType" ) == 0',
        'Cvar_VariableIntegerValue( "sv_serverType" ) <= 0 && from->type != NA_IP'),
    'a LAN server is offline too': (
        DROPS, 'Cvar_VariableIntegerValue( "sv_serverType" ) == 0', 'Cvar_VariableIntegerValue( "sv_serverType" ) <= 1'),
    'the local client is dropped': (DROPS, 'from->type != NA_LOOPBACK', 'from->type != NA_BAD'),
    'every profile drops': (DROPS, 'return Com_GameIsQuakeLive() && from', 'return from'),
    'a dedicated build drops': (DROPS, '#ifdef DEDICATED', '#ifdef FNTECH3_NEVER_DEFINED'),
    'a client build never drops': (DROPS, '#ifdef DEDICATED', '#ifndef DEDICATED'),
    'the LAN rule refuses the LAN': (REFUSES, '\t\t&& !Sys_IsLANAddress( from )', '\t\t&& true'),
    'an Internet server is LAN-only': (
        REFUSES, 'Cvar_VariableIntegerValue( "sv_serverType" ) == 1', 'Cvar_VariableIntegerValue( "sv_serverType" ) >= 1'),
    'every profile is LAN-only': (REFUSES, 'return Com_GameIsQuakeLive() && Cvar', 'return Cvar'),
}


def read(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


def expected(quakelive, dedicated, game, server_type, kind):
    ql = game == quakelive
    drop = not dedicated and ql and kind != 0 and server_type == 0
    refuse = ql and server_type == 1 and kind not in (0, 2)
    return int(drop), int(refuse)


class QuakeLiveServerTypeTests(unittest.TestCase):
    def setUp(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        main = read('code/server/sv_main.cpp')
        self.functions = {name: function(main, name) for name in (DROPS, REFUSES)}

    def run_table(self, functions, dedicated):
        source = PRELUDE + functions[DROPS] + '\n' + functions[REFUSES] + '\n' + CHECKS
        with tempfile.TemporaryDirectory(prefix='ql-server-type-') as directory:
            folder = Path(directory)
            cpp = folder / 'probe.cpp'
            exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(self.compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([self.compiler, *target, *(['-DDEDICATED'] if dedicated else []), '-std=c++17',
                                    '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        lines = run.stdout.splitlines()
        self.assertFalse([line for line in lines if line.startswith('unexpected')], lines[:3])
        header = lines[0].split()
        self.assertEqual(header[0], 'ids')
        rows = [tuple(int(value) for value in line.split()) for line in lines[1:]]
        return int(header[1]), int(header[2]), rows

    def mismatches(self, quakelive, dedicated, rows):
        return [row for row in rows
                if (row[3], row[4]) != expected(quakelive, dedicated, row[0], row[1], row[2])]

    def test_every_profile_in_both_builds(self):
        for dedicated in (False, True):
            with self.subTest(dedicated=dedicated):
                quakelive, count, rows = self.run_table(self.functions, dedicated)
                self.assertEqual(count, 16)
                self.assertEqual(len(rows), count * 5 * 4)
                self.assertEqual(self.mismatches(quakelive, dedicated, rows), [])
                # only Quake Live reads its server type
                self.assertFalse([row for row in rows if row[0] != quakelive and row[5]])

    def test_each_mutation_changes_the_table(self):
        for name, (signature, old, new) in MUTATIONS.items():
            with self.subTest(name):
                functions = dict(self.functions)
                self.assertEqual(functions[signature].count(old), 1, old)
                functions[signature] = functions[signature].replace(old, new)
                changed = False
                for dedicated in (False, True):
                    quakelive, _, rows = self.run_table(functions, dedicated)
                    changed = changed or bool(self.mismatches(quakelive, dedicated, rows))
                self.assertTrue(changed)

    def test_the_engine_asks_them_where_retail_does(self):
        main = read('code/server/sv_main.cpp')
        client = read('code/server/sv_client.cpp')
        # every packet, connectionless or not, before Steam's or the engine's
        # own handling sees it
        packet = function(main, 'void SV_PacketEvent( const netadr_t *from, msg_t *msg ) {')
        drop = packet.index('if ( SV_ServerTypeDropsPacket( from ) ) {\n\t\treturn;\n\t}')
        self.assertLess(drop, packet.index('SV_SteamHandleIncomingPacket( from, msg );'))
        self.assertLess(drop, packet.index('SV_ConnectionlessPacket( from, msg );'))
        # the challenge: Quake Live never reads the flag its local match sets,
        # and a LAN server answers an outsider with retail's text, before any
        # challenge work
        challenge = function(client, 'void SV_GetChallenge( const netadr_t *from, const msg_t *msg ) {')
        self.assertIn('|| ( !Com_GameIsQuakeLive() && Cvar_VariableIntegerValue("ui_singlePlayerActive") )',
                      challenge)
        refusal = challenge.index('if ( SV_ServerTypeRefusesChallenge( from ) ) {\n\t\tNET_OutOfBandPrint( '
                                  'NS_SERVER, from, "print\\nServer is for LAN clients only.\\n" );\n\t\treturn;')
        self.assertLess(refusal, challenge.index('SVC_RateLimitAddress( from, 10, 1000 )'))
        self.assertLess(refusal, challenge.index('SV_CreateChallenge('))
        # info and status are fnTech3's own answers in Quake Live, and a local
        # match still keeps them to itself, as retail answers neither
        for signature in ('static void SVC_Status( const netadr_t *from ) {',
                          'static void SVC_Info( const netadr_t *from ) {'):
            self.assertIn('|| Cvar_VariableIntegerValue("ui_singlePlayerActive")) {', function(main, signature))
        commands = read('code/server/sv_ccmds.cpp')
        self.assertIn('if ( quakeLive && !com_dedicated->integer && !singlePlayerCommand ) {\n'
                      '\t\tCvar_Set( "ui_singlePlayerActive", "1" );', commands)
        # retail's name, default and archive flag
        self.assertIn('svServerType = Cvar_Get( "sv_serverType", "0", CVAR_ARCHIVE );',
                      read('code/server/sv_factory.cpp'))
        header = read('code/server/server.h')
        self.assertIn('qboolean SV_ServerTypeDropsPacket( const netadr_t *from );', header)
        self.assertIn('qboolean SV_ServerTypeRefusesChallenge( const netadr_t *from );', header)


if __name__ == '__main__':
    unittest.main()
