"""The pure-server gamestate budget charges what a client's gamestate spends.

A pure server publishes the checksums of its loaded paks in sv_paks unless
SV_RemainingGameState says they would not fit in the gamestate; then it runs
degraded pure. Retail etded.exe 2.60b (0x0042d271..0x0042d2cc), jk2mp.exe
1.02a (0x0044dbce..0x0044dc29) and RTCW publish sv_paks without any estimate,
so fnTech3's budget keeps a server only from a guaranteed gamestate overflow,
and it has to measure the gamestate its clients are actually sent: their wire's
encoding inside their wire's message. Enemy Territory's is 32 KiB with its own
tables; Outcast's entity fields come in Raven's order, so an item or mover
costs it more than Quake Live's measure says.

This compiles the production SV_RemainingGameState with the gamestate writers
it shares with SV_SendClientGameState (code/server/sv_client.cpp), the
gamestate part of SV_SendClientGameState itself, the engine's message codecs
and the real profile catalog. For each profile measured on its own wire, the
estimate must equal the client's wire limit less the reserve and the bytes of
the client's own gamestate plus the estimate's 256 command bytes and closing
opcode. Quake III keeps Quake Live's conservative measure. The mutation must
fail. See docs/fntech3/2026-10-15-census-small-gaps.md.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/server/server.h"
#include "code/qcommon/protocol_contract_profile.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
server_t sv{};
serverStatic_t svs{};
static cvar_t developerCvar{}, padPacketsCvar{}, pureCvar{};
cvar_t *com_developer = &developerCvar;
cvar_t *sv_padPackets = &padPacketsCvar;
cvar_t *sv_pure = &pureCvar;
struct Drop {};
void QDECL Com_Error( errorParm_t, const char *, ... ) { throw Drop{}; }
void QDECL Com_Printf( const char *, ... ) {}
void QDECL Com_DPrintf( const char *, ... ) {}
static const comGameProfile_t *activeProfile;
static std::string serverInfo, systemInfo;
const char *Cvar_InfoString( int, qboolean * ) { return serverInfo.c_str(); }
const char *Cvar_InfoString_Big( int, qboolean * ) { return systemInfo.c_str(); }
const char *FS_GetCurrentGameDir( void ) { return "base"; }
qboolean FS_InvalidGameDir( const char * ) { return qfalse; }
// No server command waits, the pure flags agree and no profile here sends
// Medal of Honor's baselines, so these are not reached.
void SV_UpdateServerCommandsToClient( client_t *, msg_t * ) {}
void SV_MOHAAWriteBaselines( msg_t * ) { assert( false ); }
void Q_strncpyz( char *, const char *, int ) { assert( false ); }
qboolean Info_SetValueForKey_s( char *, int, const char *, const char * ) { assert( false ); return qfalse; }
const char *QDECL va( const char *, ... ) { assert( false ); return ""; }
int Com_GameIsSTVEFSP( void ) { return 0; }
'''

PREDICATES = ('const comGameProfile_t *Com_GameProfile( void )', 'int Com_GameConfigstringLimit( void )',
              'int Com_GameIsRTCW( void )', 'int Com_GameIsWolfET( void )', 'int Com_GameIsSTVEFMP( void )',
              'int Com_GameIsJKAMP( void )', 'int Com_GameIsJK2MP( void )', 'int Com_GameIsAlice( void )',
              'int Com_GameIsFAKK2( void )', 'int Com_GameIsRitual( void )')

WRITERS = ('static void SV_WriteConfigstringMessage(', 'static void SV_WritePureAwareConfigstring(',
           'void SV_ErrorForWireProfile(', 'static void SV_WriteBaselineMessage(',
           'static void SV_WriteJKAGameDirectory(', 'int SV_RemainingGameState( void )')

CHECKS = r'''
static std::vector<std::string> strings;
static std::unique_ptr<client_t[]> clients;

// The server a level leaves: its two info strings, `extra` other configstrings
// and `items` item baselines.
static void Level( int extra, int items, int systemLength ) {
	strings.assign( MAX_GAME_CONFIGSTRINGS, std::string() );
	serverInfo = "\\sv_hostname\\budget\\mapname\\probe\\g_gametype\\0\\sv_maxclients\\16";
	systemInfo = "\\sv_serverid\\1234\\sv_pure\\1\\sv_paks\\";
	while ( (int)systemInfo.size() < systemLength ) systemInfo += "1234567890 ";
	strings[CS_SERVERINFO] = serverInfo;
	strings[CS_SYSTEMINFO] = systemInfo;
	for ( int i = 0; i < extra; ++i ) strings[CS_SYSTEMINFO + 2 + i] = "models/mapobjects/probe" + std::to_string( i ) + ".md3";
	for ( int i = 0; i < MAX_GAME_CONFIGSTRINGS; ++i ) sv.configstrings[i] = &strings[i][0];
	std::memset( sv.baselineUsed, 0, sizeof( sv.baselineUsed ) );
	for ( auto &entity : sv.svEntities ) entity.baseline = entityState_t{};
	for ( int i = 0; i < items; ++i ) {
		const int number = 64 + i;
		entityState_t &state = sv.svEntities[number].baseline;
		state.number = number;
		state.eType = 2;
		state.modelindex = 3 + i % 5;
		state.pos.trBase[0] = state.origin[0] = float( 8 * i );
		state.pos.trBase[1] = state.origin[1] = -64;
		state.pos.trBase[2] = state.origin[2] = 24;
		sv.baselineUsed[number] = 1;
	}
	sv.checksumFeed = 0x1234567;
	sv.pure = pureCvar.integer = 1;
}

// The gamestate SV_SendClientGameState sends a client of this wire.
static int ClientBits( netchanWireProfile_t wire ) {
	client_t *client = &clients[7];
	*client = client_t{};
	client->netchan.wireProfile = wire;
	client->lastClientCommand = 7;
	client->reliableSequence = 7;
	static std::array<byte, MAX_GAME_MSGLEN_BUF> msgBuffer;
	msgBuffer.fill( 0 );
	entityState_t nullstate{};
	msg_t msg;
	bool csUpdated;
GAMESTATE
	assert( !msg.overflowed );
	return msg.bit;
}

// What the estimate writes and a client is not sent: 256 command bytes and
// the opcode SV_SendMessageToClient closes the message with.
static int EstimateOnlyBits( netchanWireProfile_t wire ) {
	static std::array<byte, MAX_GAME_MSGLEN_BUF> buffer;
	msg_t msg;
	MSG_Init( &msg, buffer.data(), int( buffer.size() ) );
	for ( int i = 0; i < 256; ++i ) MSG_WriteByte( &msg, i & 127 );
	MSG_WriteByte( &msg, Netchan_ServerEOFOpcode( wire ) );
	return msg.bit;
}

int main() {
	clients.reset( new client_t[8]() );
	svs.clients = clients.get();
	struct Case { comGameId_t id; bool exact; };
	const Case cases[] = {
		{ COM_GAME_JK2_MP, true }, { COM_GAME_WOLFET, true }, { COM_GAME_RTCW_MP, true },
		{ COM_GAME_JKA_MP, true }, { COM_GAME_QUAKELIVE, true }, { COM_GAME_QUAKE3, false },
	};
	struct State { int extra, items, systemLength; };
	const State states[] = { { 0, 0, 64 }, { 20, 64, 600 }, { 60, 300, 1500 } };
	int failures = 0;
	for ( const Case &c : cases ) {
		activeProfile = nullptr;
		for ( const auto &profile : profiles ) if ( profile.id == c.id ) activeProfile = &profile;
		assert( activeProfile );
		const auto *contract = fnql::protocol::FindForProfile( activeProfile->preferredProtocol, activeProfile );
		assert( contract );
		const netchanWireProfile_t wire = contract->wireProfile;
		for ( const State &s : states ) {
			Level( s.extra, s.items, s.systemLength );
			const int bits = ClientBits( wire );
			const int expected = Netchan_MessageLimit( wire ) - 512
				- PAD( bits + EstimateOnlyBits( wire ), 8 ) / 8;
			const int estimate = SV_RemainingGameState();
			const bool good = c.exact ? estimate == expected : estimate <= expected;
			std::printf( "%s items %d: estimate %d, client gamestate leaves %d\n",
				activeProfile->key, s.items, estimate, expected );
			if ( !good ) ++failures;
		}
	}
	std::printf( "%d failures\n", failures );
	return failures ? 1 : 0;
}
'''

MUTATION = (' || Com_GameIsWolfET()', ' || Com_GameIsJK2MP()')


def production():
    server = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text(encoding='utf-8')
    sender = function(server, 'static void SV_SendClientGameState( client_t *client ) {')
    start = sender.index('\tMSG_Init( &msg, msgBuffer.data(), Netchan_MessageLimit( client->netchan.wireProfile ) );')
    end = sender.index('\n\t// it is important to handle gamestate overflow', start)
    return {
        'catalog': catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')],
        'predicates': '\n'.join(function(catalog, signature) for signature in PREDICATES),
        'writers': '\n'.join(function(server, signature) for signature in WRITERS),
        'gamestate': sender[start:end],
    }


def source(parts):
    return (PRELUDE + parts['catalog'] + '\n' + parts['predicates'] + '\n' + parts['writers'] + '\n'
            + CHECKS.replace('GAMESTATE', parts['gamestate']))


class GamestateBudgetWireTests(unittest.TestCase):
    def run_probe(self, folder, name, text):
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(text, encoding='utf-8')
        built = subprocess.run([self.compiler, *self.flags, str(cpp), *self.objects, '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=180)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        return subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)

    def test_estimate_charges_each_clients_own_gamestate(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(self.compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        common = [*target, '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT)]
        self.flags = [*common, '-std=c++17', '-Wno-comment']
        parts = production()
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='gamestate-budget-', dir=ROOT / '.tmp') as directory:
            folder = Path(directory)
            self.objects = []
            for name, path, language in (
                    ('msg', 'code/qcommon/msg.cpp', ['-std=c++17', '-DDEDICATED']),
                    ('huffman', 'code/qcommon/huffman_static.c', ['-x', 'c', '-DDEDICATED'])):
                built = subprocess.run([self.compiler, *common, *language, '-c', str(ROOT / path),
                                        '-o', str(folder / f'{name}.o')],
                                       cwd=ROOT, capture_output=True, text=True, timeout=180)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                self.objects.append(str(folder / f'{name}.o'))
            ran = self.run_probe(folder, 'production', source(parts))
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            # Enemy Territory and Outcast measured as Quake Live, as before.
            writers = parts['writers']
            for old in MUTATION:
                self.assertEqual(writers.count(old), 1, old)
                writers = writers.replace(old, '')
            ran = self.run_probe(folder, 'mutation', source(dict(parts, writers=writers)))
            self.assertNotEqual(ran.returncode, 0, ran.stdout)


if __name__ == '__main__':
    unittest.main()
