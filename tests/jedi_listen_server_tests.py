"""Each profile's single-player game type and whether its listen server hides.

`SV_GameTypeIsSinglePlayer` and `SV_GameTypeHidesListenServer` are compiled
from code/server/sv_main.cpp and run for every profile over game types -1 to
10; the table they print must match the rules read from the retail engines
(docs/fntech3/2026-10-06-jedi-listen-server-game-types.md). Raven's Jedi
games number single player after their own modes: Jedi Academy's is 5 and its
engine never hides a listen server by game type (jamp.exe 1.01), Jedi
Outcast's is 4 and its engine hides one there (jk2mp.exe 1.02a). Every other
profile keeps the rule it had. Each mutation must change the table.
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
#include "code/compat/wolfet_gameinfo.hpp"
#include <cstdio>
static int game;
int Com_GameIsWolfET(){return game==COM_GAME_WOLFET;}
int Com_GameIsMOHAA(){return game==COM_GAME_MOHAA;}
int Com_GameIsJKAMP(){return game==COM_GAME_JKA_MP;}
int Com_GameIsJK2MP(){return game==COM_GAME_JK2_MP;}
// Retail ET's gameinfo.dat lists game type 0 alone as single player.
static fntech3::wolfet::gameinfo::GameInfo etInfo;
static const fntech3::wolfet::gameinfo::GameInfo &SV_WolfETGameInfo(){return etInfo;}
'''

CHECKS = r'''
int main(){
 etInfo.spGameTypes=1u<<0;
 std::printf("ids %d %d %d %d %d\n",int(COM_GAME_WOLFET),int(COM_GAME_MOHAA),int(COM_GAME_JKA_MP),
  int(COM_GAME_JK2_MP),int(COM_GAME_COUNT)-int(COM_GAME_QUAKE3));
 for(game=COM_GAME_QUAKE3;game<COM_GAME_COUNT;++game)for(int type=-1;type<=10;++type)
  std::printf("%d %d %d %d\n",game,type,int(SV_GameTypeIsSinglePlayer(type)),int(SV_GameTypeHidesListenServer(type)));
 return 0;
}
'''

SINGLE = 'qboolean SV_GameTypeIsSinglePlayer( int gameType ) {'
HIDES = 'qboolean SV_GameTypeHidesListenServer( int gameType ) {'

# Each mutation replaces text in one production function and must change the table.
MUTATIONS = {
    "Jedi Academy hides its single-player game": (
        HIDES, '\t\treturn qfalse;', '\t\treturn SV_GameTypeIsSinglePlayer( gameType );'),
    "Jedi Outcast never hides": (
        HIDES, 'if ( Com_GameIsJKAMP() ) {', 'if ( Com_GameIsJKAMP() || Com_GameIsJK2MP() ) {'),
    "Jedi Academy takes Quake III's number": (
        SINGLE, 'gameType == SV_JKA_GT_SINGLE_PLAYER', 'gameType == GT_SINGLE_PLAYER'),
    "Jedi Outcast takes Quake III's number": (
        SINGLE, 'gameType == SV_JK2_GT_SINGLE_PLAYER', 'gameType == GT_SINGLE_PLAYER'),
    "the Jedi rules reach the single-player modes too": (
        SINGLE, 'if ( Com_GameIsJKAMP() ) {', 'if ( Com_GameIsJKAMP() || game == COM_GAME_JKA_SP ) {'),
}


def read(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


def expected(ids, game, game_type):
    wolfet, mohaa, jka_mp, jk2_mp = ids
    if game == wolfet:
        single = game_type == 0
    elif game == mohaa:
        single = game_type == 0
    elif game == jka_mp:
        return game_type == 5, False
    elif game == jk2_mp:
        single = game_type == 4
    else:
        # Quake III's GT_SINGLE_PLAYER, unchanged for every other profile
        single = game_type == 2
    return single, single


class JediListenServerTests(unittest.TestCase):
    def setUp(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        main = read('code/server/sv_main.cpp')
        self.functions = {name: function(main, name) for name in (SINGLE, HIDES)}

    def run_table(self, functions):
        source = PRELUDE + functions[SINGLE] + '\n' + functions[HIDES] + '\n' + CHECKS
        with tempfile.TemporaryDirectory(prefix='jedi-listen-') as directory:
            folder = Path(directory)
            cpp = folder / 'probe.cpp'
            exe = folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(self.compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([self.compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        lines = run.stdout.splitlines()
        header = lines[0].split()
        self.assertEqual(header[0], 'ids')
        ids = tuple(int(value) for value in header[1:5])
        rows = [tuple(int(value) for value in line.split()) for line in lines[1:]]
        return ids, int(header[5]), rows

    def mismatches(self, ids, rows):
        found = []
        for game, game_type, single, hides in rows:
            want = expected(ids, game, game_type)
            if (bool(single), bool(hides)) != want:
                found.append((game, game_type, single, hides, want))
        return found

    def test_every_profile_follows_its_retail_engine(self):
        ids, count, rows = self.run_table(self.functions)
        # all sixteen profiles, game types -1 to 10
        self.assertEqual(count, 16)
        self.assertEqual(len(rows), count * 12)
        self.assertEqual(self.mismatches(ids, rows), [])
        wolfet, mohaa, jka_mp, jk2_mp = ids
        table = {(game, game_type): (single, hides) for game, game_type, single, hides in rows}
        # Jedi Master, 2, is no single player in either Jedi game, and a
        # listen server hosting it answers remote players in both.
        for game in (jka_mp, jk2_mp):
            self.assertEqual(table[(game, 2)], (0, 0))
        # Academy's single player is still single player, but hides nothing.
        self.assertEqual(table[(jka_mp, 5)], (1, 0))
        self.assertEqual(table[(jk2_mp, 4)], (1, 1))

    def test_each_mutation_changes_the_table(self):
        for name, (signature, old, new) in MUTATIONS.items():
            with self.subTest(name):
                functions = dict(self.functions)
                self.assertEqual(functions[signature].count(old), 1, old)
                functions[signature] = functions[signature].replace(old, new)
                ids, _, rows = self.run_table(functions)
                self.assertNotEqual(self.mismatches(ids, rows), [])

    def test_the_three_listen_gates_ask_the_listen_rule(self):
        call = 'SV_GameTypeHidesListenServer( Cvar_VariableIntegerValue( "g_gametype" ) ) || ' \
               'Cvar_VariableIntegerValue("ui_singlePlayerActive")'
        main = read('code/server/sv_main.cpp')
        client = read('code/server/sv_client.cpp')
        for source, signature in ((main, 'static void SVC_Status( const netadr_t *from ) {'),
                                  (main, 'static void SVC_Info( const netadr_t *from ) {'),
                                  (client, 'void SV_GetChallenge( const netadr_t *from, const msg_t *msg ) {')):
            with self.subTest(signature):
                body = function(source, signature)
                self.assertIn('#ifndef DEDICATED\n\tif ( ' + call + ') {\n\t\treturn;\n\t}\n#endif', body)
        server = ''.join(read(f'code/server/{path.name}') for path in sorted((ROOT / 'code/server').iterdir())
                         if path.suffix in {'.cpp', '.h', '.hpp'})
        self.assertEqual(server.count('SV_GameTypeHidesListenServer( Cvar_VariableIntegerValue'), 3)
        self.assertNotIn('SV_GameTypeIsSinglePlayer( Cvar_VariableIntegerValue( "g_gametype" ) ) || '
                         'Cvar_VariableIntegerValue("ui_singlePlayerActive")', server)
        header = read('code/server/server.h')
        self.assertIn('SV_JKA_GT_SINGLE_PLAYER = 5,\n\tSV_JK2_GT_SINGLE_PLAYER = 4\n', header)


if __name__ == '__main__':
    unittest.main()
