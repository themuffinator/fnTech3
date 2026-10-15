"""The pure-server gamestate budget charges what a client's gamestate spends.

A pure server publishes the checksums of its loaded paks in sv_paks unless
SV_SetPurePaks finds, through SV_RemainingGameState, that they would not fit
in the gamestate; then it runs degraded pure. Retail etded.exe 2.60b
(0x0042d271..0x0042d2cc), jk2mp.exe 1.02a (0x0044dbce..0x0044dc29) and RTCW
publish sv_paks without any estimate, as Medal of Honor's engine does in
OpenMoHAA's reading, so fnTech3's budget keeps a server only from a guaranteed
gamestate overflow, and it has to measure the gamestate its clients are
actually sent: their wire's encoding inside their wire's message. Enemy
Territory's is 32 KiB with its own tables; Outcast's entity fields come in
Raven's order, so an item or mover costs it more than Quake Live's measure
says. Medal of Honor's is 48 KiB and its baselines are its own entity states,
written by SV_MOHAAWriteBaselines; its multiplayer game registers its sounds,
each a configstring, only after the pak list was first measured, so the end
of its spawn measures the list again. The IHV Test and Q3Test send their
clients a systeminfo of their releases' own keys, which never carries a pak
list, and no engine of theirs has a pure mode, so no budget applies to them
and SV_SetPurePaks publishes nothing for them.

This compiles the production SV_RemainingGameState with the gamestate writers
it shares with SV_SendClientGameState (code/server/sv_client.cpp), the
gamestate part of SV_SendClientGameState itself, Medal of Honor's baseline
writer (code/server/server_mohaa_net.hpp), SV_SetPurePaks and the end of
Medal of Honor's spawn (code/server/sv_init.cpp), the engine's message codecs
and the real profile catalog. For each profile measured on its own wire, the
estimate must equal the client's wire limit less the reserve and the bytes of
the client's own gamestate plus the estimate's 256 command bytes and closing
opcode. Quake III keeps Quake Live's conservative measure. A Medal of Honor
spawn whose sounds take the room its pak list had must end without the list,
and one with room must keep it, its gamestate inside the budget either way.
Each mutation restores an earlier behaviour and must fail its own check. See
docs/fntech3/2026-10-15-census-small-gaps.md and
docs/fntech3/2026-10-15-pure-budget-own-wires.md.
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
#include "code/compat/mohaa_message_codec.hpp"
#include <array>
#include <cassert>
#include <cstdarg>
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
static std::string lastPrint;
void QDECL Com_Printf( const char *format, ... ) {
	char text[1024];
	va_list args;
	va_start( args, format );
	std::vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	lastPrint = text;
}
void QDECL Com_DPrintf( const char *, ... ) {}
static const comGameProfile_t *activeProfile;
// The serverinfo, and the systeminfo its cvars make, sv_paks last once set.
static std::string serverInfo, systemBase, paks, pakNames, loadedPaks;
static int loadedPakLists;
static std::string SystemInfo() { return paks.empty() ? systemBase : systemBase + "\\sv_paks\\" + paks; }
const char *Cvar_InfoString( int, qboolean * ) { return serverInfo.c_str(); }
const char *Cvar_InfoString_Big( int, qboolean *truncated ) {
	static std::string text;
	text = SystemInfo();
	if ( truncated ) *truncated = qfalse;
	return text.c_str();
}
void Cvar_Set( const char *name, const char *value ) {
	if ( !std::strcmp( name, "sv_paks" ) ) paks = value;
	else if ( !std::strcmp( name, "sv_pakNames" ) ) pakNames = value;
	else assert( false );
}
const char *FS_LoadedPakChecksums( qboolean *overflowed ) {
	++loadedPakLists;
	*overflowed = qfalse;
	return loadedPaks.c_str();
}
const char *FS_LoadedPakPureNames( qboolean *overflowed ) {
	++loadedPakLists;
	*overflowed = qfalse;
	return "";
}
const char *FS_GetCurrentGameDir( void ) { return "base"; }
qboolean FS_InvalidGameDir( const char * ) { return qfalse; }
// No server command waits and the pure flags agree, so these are not reached.
void SV_UpdateServerCommandsToClient( client_t *, msg_t * ) {}
void Q_strncpyz( char *, const char *, int ) { assert( false ); }
qboolean Info_SetValueForKey_s( char *, int, const char *, const char * ) { assert( false ); return qfalse; }
const char *QDECL va( const char *, ... ) { assert( false ); return ""; }
int Com_GameIsSTVEFSP( void ) { return 0; }
// Medal of Honor's reader rebuilds quaternions; nothing here reads a delta.
extern "C" void EulerToQuat( float *, float * ) { assert( false ); }
'''

PREDICATES = ('const comGameProfile_t *Com_GameProfile( void )', 'int Com_GameConfigstringLimit( void )',
              'int Com_GameIsRTCW( void )', 'int Com_GameIsWolfET( void )', 'int Com_GameIsSTVEFMP( void )',
              'int Com_GameIsJKAMP( void )', 'int Com_GameIsJK2MP( void )', 'int Com_GameIsAlice( void )',
              'int Com_GameIsFAKK2( void )', 'int Com_GameIsRitual( void )', 'int Com_GameIsMOHAA( void )',
              'int Com_GameIsQuake3IHV( void )', 'int Com_GameIsQ3Test( void )')

WRITERS = ('static void SV_WriteConfigstringMessage(', 'static void SV_WritePureAwareConfigstring(',
           'void SV_ErrorForWireProfile(', 'static void SV_WriteBaselineMessage(',
           'static void SV_WriteJKAGameDirectory(', 'int SV_RemainingGameState( void )')

HELPERS = r'''
static std::vector<std::string> strings;
static std::unique_ptr<client_t[]> clients;

static void SetString( int index, std::string text ) {
	strings[index] = std::move( text );
	sv.configstrings[index] = &strings[index][0];
}

// The server a level leaves: its two info strings, `extra` other
// configstrings and `items` item baselines, which Medal of Honor keeps as its
// own entity states. Its sound block is left for its game's sounds.
static void Level( int extra, int items, int systemLength, bool mohaa ) {
	strings.assign( MAX_GAME_CONFIGSTRINGS, std::string() );
	for ( int i = 0; i < MAX_GAME_CONFIGSTRINGS; ++i ) sv.configstrings[i] = &strings[i][0];
	paks.clear();
	pakNames.clear();
	serverInfo = "\\sv_hostname\\budget\\mapname\\probe\\g_gametype\\0\\sv_maxclients\\16";
	systemBase = "\\sv_serverid\\1234\\sv_pure\\1\\sv_referencedPaks\\";
	while ( (int)systemBase.size() < systemLength ) systemBase += "1234567890 ";
	SetString( CS_SERVERINFO, serverInfo );
	SetString( CS_SYSTEMINFO, SystemInfo() );
	for ( int i = 0, index = CS_SYSTEMINFO + 2; i < extra; ++i, ++index ) {
		if ( mohaa && index == FNTECH3_MOHAA_CS_SOUNDS ) index += FNTECH3_MOHAA_MAX_SOUNDS;
		SetString( index, "models/mapobjects/probe" + std::to_string( i ) + ".md3" );
	}
	std::memset( sv.baselineUsed, 0, sizeof( sv.baselineUsed ) );
	for ( auto &entity : sv.svEntities ) entity.baseline = entityState_t{};
	std::memset( mohaa_game::baselineUsed, 0, sizeof( mohaa_game::baselineUsed ) );
	for ( int i = 0; i < items; ++i ) {
		const int number = 64 + i;
		if ( mohaa ) {
			fntech3::mohaa::EntityState &state = mohaa_game::baselines[number];
			fntech3::mohaa::wire::NullEntityState( state );
			state.number = number;
			state.eType = 2;
			state.modelindex = 3 + i % 5;
			state.netorigin[0] = state.origin[0] = float( 8 * i );
			state.netorigin[1] = state.origin[1] = -64;
			state.netorigin[2] = state.origin[2] = 24;
			mohaa_game::baselineUsed[number] = true;
			continue;
		}
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

// Medal of Honor's multiplayer game, registering a level's sounds once it has
// spawned: each takes the next slot of the sound block, its streamed flag
// last, as SoundIndex names it (code/server/server_mohaa_game.hpp).
static int soundsToRegister;
void SV_MOHAASpawnDone( void ) {
	for ( int i = 1; i <= soundsToRegister; ++i ) {
		SetString( FNTECH3_MOHAA_CS_SOUNDS + i, "sound/weapons/fire/probe_" + std::to_string( i ) + ".wav0" );
	}
}
void SV_UpdateSysteminfoConfigstring( void ) { SetString( CS_SYSTEMINFO, SystemInfo() ); }

// The gamestate SV_SendClientGameState sends a client of this wire, or more
// than the wire's limit when it does not fit.
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
	try {
GAMESTATE
	} catch ( const Drop & ) {
		msg.overflowed = qtrue;
	}
	return msg.overflowed ? ( Netchan_MessageLimit( wire ) + 1 ) * 8 : msg.bit;
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
'''

CHECKS = r'''
static int failures;
static void Check( bool good, const std::string &name, const std::string &detail ) {
	std::printf( "%s %s: %s\n", good ? "PASS" : "FAIL", name.c_str(), detail.c_str() );
	if ( !good ) ++failures;
}

static void Select( comGameId_t id ) {
	activeProfile = nullptr;
	for ( const auto &profile : profiles ) if ( profile.id == id ) activeProfile = &profile;
	assert( activeProfile );
}

// The room the gamestate a client of this wire is sent now leaves under the
// wire's limit and the reserve, less the estimate's command bytes: what the
// estimate has to report.
static int Left( netchanWireProfile_t wire ) {
	return Netchan_MessageLimit( wire ) - 512 - PAD( ClientBits( wire ) + EstimateOnlyBits( wire ), 8 ) / 8;
}

// A pak checksum list of `length` characters, as sv_paks lists them.
static std::string PakList( int length ) {
	std::string list;
	for ( long long n = 1; (int)list.size() < length; ++n ) {
		list += std::to_string( ( n * 2654435761LL ) % 4294967296LL - 2147483648LL ) + " ";
	}
	list.resize( length );
	return list;
}

// A Medal of Honor spawn with sv_pure set: the pak list is measured where
// every profile's is, the systeminfo published, and then the end of its
// spawn runs, which is where the game registers its sounds.
struct Spawn { bool before, after; int left; };
static Spawn SpawnMOHAA( int extra, int sounds, const std::string &list ) {
	Level( extra, 64, 64, true );
	loadedPaks = list;
	soundsToRegister = sounds;
	SV_SetPurePaks();
	Spawn spawn{};
	spawn.before = !paks.empty();
	SV_UpdateSysteminfoConfigstring();
	FinishMOHAASpawn();
	spawn.after = paks == list;
	spawn.left = Left( NETCHAN_WIRE_MOHAA );
	return spawn;
}

int main() {
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	clients.reset( new client_t[8]() );
	svs.clients = clients.get();

	struct Case { comGameId_t id; bool exact; };
	const Case cases[] = {
		{ COM_GAME_JK2_MP, true }, { COM_GAME_WOLFET, true }, { COM_GAME_RTCW_MP, true },
		{ COM_GAME_JKA_MP, true }, { COM_GAME_QUAKELIVE, true }, { COM_GAME_MOHAA, true },
		{ COM_GAME_QUAKE3, false },
	};
	struct State { int extra, items, systemLength; };
	const State states[] = { { 0, 0, 64 }, { 20, 64, 600 }, { 60, 300, 1500 } };
	for ( const Case &c : cases ) {
		Select( c.id );
		const auto *contract = fnql::protocol::FindForProfile( activeProfile->preferredProtocol, activeProfile );
		assert( contract );
		const netchanWireProfile_t wire = contract->wireProfile;
		for ( const State &s : states ) {
			Level( s.extra, s.items, s.systemLength, wire == NETCHAN_WIRE_MOHAA );
			const int expected = Left( wire );
			const int estimate = SV_RemainingGameState();
			Check( c.exact ? estimate == expected : estimate <= expected,
				std::string( "estimate " ) + activeProfile->key + " items " + std::to_string( s.items ),
				"estimate " + std::to_string( estimate ) + ", client gamestate leaves " + std::to_string( expected ) );
		}
	}

	// Every other profile publishes a list that fits, as before.
	Select( COM_GAME_QUAKE3 );
	Level( 0, 0, 64, false );
	loadedPaks = PakList( 72 );
	SV_SetPurePaks();
	Check( paks == loadedPaks, "publish quake3", "sv_paks \"" + paks + "\"" );

	// Medal of Honor: the stock maps' 364 to 381 sounds leave room for the
	// installation's six paks...
	Select( COM_GAME_MOHAA );
	const std::string stock = PakList( 72 );
	Spawn spawn = SpawnMOHAA( 150, 381, stock );
	Check( spawn.before && spawn.after && spawn.left >= 0, "mohaa-sounds room",
		"published " + std::to_string( spawn.before ) + "/" + std::to_string( spawn.after )
		+ ", leaves " + std::to_string( spawn.left ) );

	// ...but on a level whose list fits only before its sounds, the list has
	// to go once they are in, and the gamestate without it still fits: about
	// 2,000 bytes are left after the sounds, and the list takes 1,000 more.
	Level( 0, 64, 64, true );
	const int empty = SV_RemainingGameState();
	soundsToRegister = 381;
	SV_MOHAASpawnDone();
	const int cost = empty - SV_RemainingGameState();
	int extra = 0, room = empty;
	for ( ; room > cost + 2000; extra += 10 ) {
		Level( extra + 10, 64, 64, true );
		room = SV_RemainingGameState();
	}
	const std::string tight = PakList( room - cost + 1000 - 9 );
	spawn = SpawnMOHAA( extra, 381, tight );
	Check( spawn.before && !spawn.after && paks.empty() && spawn.left >= 0, "mohaa-sounds tight",
		"sounds cost " + std::to_string( cost ) + ", room before them " + std::to_string( room )
		+ ", list " + std::to_string( tight.size() ) + ", published " + std::to_string( spawn.before ) + "/"
		+ std::to_string( spawn.after ) + ", leaves " + std::to_string( spawn.left ) );

	// The IHV Test and Q3Test have no pure mode and send no pak list.
	for ( comGameId_t id : { COM_GAME_QUAKE3_IHV, COM_GAME_Q3TEST } ) {
		Select( id );
		Level( 0, 0, 64, false );
		loadedPaks = PakList( 72 );
		loadedPakLists = 0;
		lastPrint.clear();
		SV_SetPurePaks();
		Check( paks.empty() && pakNames.empty() && !loadedPakLists
				&& lastPrint.find( "has no pure mode" ) != std::string::npos,
			std::string( "no-pure " ) + activeProfile->key, "sv_paks \"" + paks + "\", said \"" + lastPrint + "\"" );
		sv.pure = pureCvar.integer = 0;
		lastPrint.clear();
		SV_SetPurePaks();
		Check( paks.empty() && lastPrint.empty(), std::string( "not-pure " ) + activeProfile->key,
			"said \"" + lastPrint + "\"" );
	}

	std::printf( "%d failures\n", failures );
	return failures ? 1 : 0;
}
'''

# Each restores an earlier behaviour; the checks it must fail.
MUTATIONS = (
    ('Enemy Territory and Outcast measured as Quake Live',
     (('writers', ' || Com_GameIsWolfET()', ''), ('writers', ' || Com_GameIsJK2MP()', '')),
     ('FAIL estimate wolfet items 0', 'FAIL estimate jk2-mp items 300')),
    ('Medal of Honor measured as Quake Live',
     (('writers', ' || Com_GameIsMOHAA()', ''),),
     ('FAIL estimate mohaa items 0', 'FAIL estimate mohaa items 300')),
    ('Medal of Honor measured without its own baselines',
     (('writers', 'SV_MOHAAWriteBaselines( &msg );', '(void)0;'),),
     ('PASS estimate mohaa items 0', 'FAIL estimate mohaa items 64', 'FAIL estimate mohaa items 300')),
    ("Medal of Honor's list not measured again after its sounds",
     (('finish', '\t\tSV_SetPurePaks();\n', ''),),
     ('PASS mohaa-sounds room', 'FAIL mohaa-sounds tight')),
    ('The IHV Test and Q3Test measured as pure servers',
     (('pure', 'if ( sv.pure != 0 && ( Com_GameIsQuake3IHV() || Com_GameIsQ3Test() ) ) {', 'if ( false ) {'),),
     ('FAIL no-pure q3-ihv', 'FAIL no-pure q3test')),
)


def production():
    server = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
    init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
    game = (ROOT / 'code/server/server_mohaa_game.hpp').read_text(encoding='utf-8')
    net = (ROOT / 'code/server/server_mohaa_net.hpp').read_text(encoding='utf-8')
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text(encoding='utf-8')
    sender = function(server, 'static void SV_SendClientGameState( client_t *client ) {')
    start = sender.index('\tMSG_Init( &msg, msgBuffer.data(), Netchan_MessageLimit( client->netchan.wireProfile ) );')
    end = sender.index('\n\t// it is important to handle gamestate overflow', start)
    stored = game.index('fntech3::mohaa::EntityState baselines[FNTECH3_MOHAA_MAX_GENTITIES];')
    stored = game[stored:game.index('\n', game.index('bool baselineUsed[FNTECH3_MOHAA_MAX_GENTITIES];', stored))]
    finish = init.index('\tif ( Com_GameIsMOHAA() ) {\n\t\tSV_MOHAASpawnDone();')
    finish = init[finish:init.index('\n\t}\n', finish) + 3]
    return {
        'catalog': catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')],
        'predicates': '\n'.join(function(catalog, signature) for signature in PREDICATES),
        'mohaa': ('namespace mohaa_game {\n' + stored + '\n}\nnamespace mohaa_net {\n'
                  'using fntech3::mohaa::EntityState;\n' + function(net, 'void WriteBaselines( msg_t *msg ) {')
                  + '\n}\n' + function(game, 'void SV_MOHAAWriteBaselines( msg_t *msg ) {')),
        'writers': '\n'.join(function(server, signature) for signature in WRITERS),
        'gamestate': sender[start:end],
        'pure': function(init, 'static void SV_SetPurePaks( void ) {'),
        'finish': 'static void FinishMOHAASpawn( void ) {\n' + finish + '}\n',
    }


def source(parts):
    return (PRELUDE + parts['catalog'] + '\n' + parts['predicates'] + '\n' + parts['mohaa'] + '\n'
            + parts['writers'] + '\n' + HELPERS.replace('GAMESTATE', parts['gamestate']) + '\n'
            + parts['pure'] + '\n' + parts['finish'] + CHECKS)


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
                    ('mohaa_msg', 'code/qcommon/mohaa_msg.cpp', ['-std=c++17', '-DDEDICATED']),
                    ('huffman', 'code/qcommon/huffman_static.c', ['-x', 'c', '-DDEDICATED'])):
                built = subprocess.run([self.compiler, *common, *language, '-c', str(ROOT / path),
                                        '-o', str(folder / f'{name}.o')],
                                       cwd=ROOT, capture_output=True, text=True, timeout=180)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                self.objects.append(str(folder / f'{name}.o'))
            ran = self.run_probe(folder, 'production', source(parts))
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertNotIn('FAIL', ran.stdout)
            for number, (name, edits, expected) in enumerate(MUTATIONS):
                mutated = dict(parts)
                for part, old, new in edits:
                    self.assertEqual(mutated[part].count(old), 1, f'{name}: {old}')
                    mutated[part] = mutated[part].replace(old, new)
                ran = self.run_probe(folder, f'mutation{number}', source(mutated))
                self.assertNotEqual(ran.returncode, 0, f'{name}\n{ran.stdout}')
                for line in expected:
                    self.assertIn(line, ran.stdout, f'{name}\n{ran.stdout}')


if __name__ == '__main__':
    unittest.main()
