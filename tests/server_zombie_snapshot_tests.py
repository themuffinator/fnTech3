"""A zombie's snapshot is one its own client can read.

A client the server has dropped stays a zombie until its slot is freed, and
SV_FinalMessage makes every client one before the server's last snapshot, so
the drop notice, the `disconnect` command and a quitting server's last words
all reach a client inside a zombie's snapshot. That snapshot carries no
changes: an empty player-state delta, then the end of the entity list. The
empty delta has to be the client's own, because each engine reads it
differently when every flag is clear:

- Quake III 1.32 (msg.c MSG_ReadDeltaPlayerstate) reads a field count and one
  bit for all four arrays. fnTech3's Quake Live layout has the same shape with
  its own fields, and protocols 66, 67 and 71 are Quake III's.
- RTCW MP (RTCW-MP msg.c 1619) and ET 2.60b (Enemy-Territory msg.c 1716) read
  the same count and array bit, then one bit for stored ammo (1775, 1872) and
  four for ammo in the clip (1790, 1887).
- RTCW SP (RTCW-SP msg.c 1790) has no count: a bit for each of its 76 fields
  (1823), then the array (1860), ammo (1937) and four clip bits (1952).
- Elite Force Holomatch flags its 48 fields and four arrays one bit each
  (stvoyHM.exe; compat/stvef_message_codec.hpp). ioEF's and Lilium Voyager's
  protocol 26 reads Quake III's count and array bit over its own fields
  (compat/stvef_ioef_codec.hpp). Elite Force SP, which only fnTech3 reads,
  sends a word count. Alice flags its 49 fields and six arrays
  (alice.exe 0x004468e0), and its snapshot ends with a sound block.

Until 2026-09-24 every profile framed in Huffman-coded bits was sent Quake
III's two fields. Retail WolfMP.exe and et.exe then took five bits too many
before the entity list and failed with `CL_ParsePacketEntities: end of
message`: the client that was kicked, and every client when the server quit.

This compiles the production SV_WriteSnapshotToClient and SV_EmitPacketEntities
with the engine's message codecs and, for every wire profile the engine
registers, reads a zombie's snapshot twice: with a reader written here from
the list above, and with the engine's own. An active client's snapshot must
still carry its player state. Each mutation below must make the probe fail.
"""
from pathlib import Path
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
    return source[start:source.index('\n}', start) + 2]


# Each profile's empty player-state delta as the widths of its reader's reads,
# in order; every read must give zero. An 8-bit read is one Huffman symbol in a
# compressed message. None: the engine has no message layout for the profile,
# so the snapshot is refused rather than sent without a player state.
QUAKE3 = (8, 1)
RTCW_MP_ET = (8, 1, 1) + (1,) * 4
RTCW_SP = (1,) * 76 + (1, 1) + (1,) * 4
HOLOMATCH = (1,) * 48 + (1,) * 4
STVEF_SP = (8,)
ALICE = (1,) * 49 + (1,) * 6
EMPTY = {
    'NETCHAN_WIRE_LEGACY_Q3': QUAKE3,
    'NETCHAN_WIRE_IOQ3': QUAKE3,
    'NETCHAN_WIRE_QL_RETAIL': QUAKE3,
    'NETCHAN_WIRE_Q3_66': QUAKE3,
    'NETCHAN_WIRE_Q3_67': QUAKE3,
    'NETCHAN_WIRE_RTCW_SP': RTCW_SP,
    'NETCHAN_WIRE_RTCW_MP': RTCW_MP_ET,
    'NETCHAN_WIRE_WOLFET': RTCW_MP_ET,
    'NETCHAN_WIRE_STVEF_HM': HOLOMATCH,
    'NETCHAN_WIRE_IOEF': QUAKE3,
    'NETCHAN_WIRE_STVEF_SP': STVEF_SP,
    'NETCHAN_WIRE_ALICE': ALICE,
    'NETCHAN_WIRE_Q3_IHV': None,
    # Medal of Honor's clients are sent the game's own records by
    # SV_MOHAASendClientSnapshot (server_mohaa_game.hpp), which
    # SV_SendClientSnapshot takes before this writer; msg.cpp's
    # MSG_LayoutIsServed refuses its layout here rather than guess one.
    'NETCHAN_WIRE_MOHAA': None,
    # Q3Test's clients are served by their own sessions
    # (server_q3test_net.hpp), which SV_SendClientMessages takes first.
    'NETCHAN_WIRE_Q3TEST_A': None,
    'NETCHAN_WIRE_Q3TEST_B': None,
}
# What a zombie's snapshot carries after its entities: Alice's empty sound
# block is one clear presence bit (alice.exe 0x00456cff).
TAIL = {'NETCHAN_WIRE_ALICE': (1,)}


def wire_profiles():
    header = (ROOT / 'code/qcommon/netchan_profile.h').read_text(encoding='utf-8')
    end = header.index('} netchanWireProfile_t;')
    block = header[header.rindex('typedef enum', 0, end):end]
    block = re.sub(r'/\*.*?\*/', '', block, flags=re.S)
    return re.findall(r'\b(NETCHAN_WIRE_[A-Z0-9_]+)\b', block)


PRELUDE = r'''
#include "code/server/server.h"
#include "code/qcommon/netchan_safety.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
server_t sv{};
serverStatic_t svs{};
static cvar_t developerCvar{}, padPacketsCvar{};
cvar_t *com_developer = &developerCvar;
cvar_t *sv_padPackets = &padPacketsCvar;
struct Drop {};
void QDECL Com_Error( errorParm_t, const char *, ... ) { throw Drop{}; }
void QDECL Com_Printf( const char *, ... ) {}
void QDECL Com_DPrintf( const char *, ... ) {}
// Elite Force single player's server runs on its own wire, and only there
// does its snapshot carry the command number (sv_snapshot.cpp).
static netchanWireProfile_t activeProfile;
int Com_GameIsSTVEFSP( void ) { return activeProfile == NETCHAN_WIRE_STVEF_SP; }
'''

# Present only while the engine refuses a layout through this helper.
ERROR_HELPER = r'''
void SV_ErrorForWireProfile( const char *, netchanWireProfile_t ) { throw Drop{}; }
'''

CHECKS = r'''
struct Case {
	const char *name;
	netchanWireProfile_t profile;
	bool refused;
	std::vector<int> empty, tail;
};

struct Short {};

// Reads as the original engines do: least significant bit first, and in a
// compressed message a read's bits below a whole byte raw, each whole byte
// one symbol of Quake III's static Huffman table.
struct Reader {
	const msg_t &msg;
	int bit;
	unsigned Read( int count ) {
		unsigned value = 0;
		const int limit = msg.cursize * 8;
		const int raw = msg.rawbit ? count : count & 7;
		for ( int i = 0; i < raw; ++i ) {
			if ( bit >= limit ) throw Short{};
			value |= unsigned( HuffmanGetBit( msg.data, bit++ ) ) << i;
		}
		for ( int i = raw; i < count; i += 8 ) {
			unsigned symbol = 0;
			const int used = HuffmanGetSymbolBounded( &symbol, msg.data, bit, limit );
			if ( used < 0 ) throw Short{};
			bit += used;
			value |= symbol << i;
		}
		return value;
	}
};

static int failures;
static void Fail( const Case &c, const char *what ) {
	std::printf( "%s: %s\n", c.name, what );
	++failures;
}

static std::vector<byte> buffer( 1 << 16 );

static std::unique_ptr<client_t> Client( const Case &c, clientState_t state ) {
	std::unique_ptr<client_t> client( new client_t() );
	client->state = state;
	client->netchan.wireProfile = c.profile;
	activeProfile = c.profile;
	client->netchan.outgoingSequence = 40;
	client->deltaMessage = 40;
	client->lastClientCommand = 7;
	// Whatever the slot's frame held last; a zombie's frame is not refreshed.
	playerState_t &ps = client->frames[ 40 & PACKET_MASK ].ps;
	ps.commandTime = 4321;
	ps.origin[0] = 64; ps.origin[1] = -32; ps.origin[2] = 24;
	ps.viewheight = 26;
	ps.clientNum = 1;
	ps.stats[0] = 100;
	return client;
}

// The message around a snapshot, as SV_SendClientSnapshot opens it.
static msg_t Open( const client_t &client ) {
	msg_t msg;
	MSG_Init( &msg, buffer.data(), int( buffer.size() ) );
	if ( SV_LegacyFraming( &client ) ) MSG_STVEFBitstream( &msg );
	else MSG_WriteLong( &msg, client.lastClientCommand );
	return msg;
}

// The same message read from its start, through the snapshot's area bits.
static msg_t Header( const Case &c, const client_t &client, const msg_t &out, bool active ) {
	msg_t in = out;
	in.readcount = 0;
	in.bit = 0;
	if ( !SV_LegacyFraming( &client ) && MSG_ReadLong( &in ) != client.lastClientCommand ) Fail( c, "acknowledgement" );
	if ( MSG_ReadByte( &in ) != svc_snapshot ) Fail( c, "opcode" );
	if ( SV_LegacyFraming( &client ) && MSG_ReadLong( &in ) != client.lastClientCommand ) Fail( c, "legacy acknowledgement" );
	if ( MSG_ReadLong( &in ) != sv.time ) Fail( c, "server time" );
	// Ritual's clients read the server's time residual, clamped to a byte,
	// before the delta byte (alice.exe 0x0042dc6e, fakk2.exe 0x0042385e).
	if ( c.profile == NETCHAN_WIRE_ALICE && MSG_ReadByte( &in ) != std::clamp( sv.timeResidual, 0, 255 ) )
		Fail( c, "time residual" );
	if ( MSG_ReadByte( &in ) != 0 ) Fail( c, "delta" );
	// Single player's command number follows the delta (cl_parse.cpp).
	if ( c.profile == NETCHAN_WIRE_STVEF_SP && MSG_ReadLong( &in ) != client.stvefSpCommandNumber )
		Fail( c, "command number" );
	const int flags = MSG_ReadByte( &in );
	if ( ( ( flags & SNAPFLAG_NOT_ACTIVE ) != 0 ) == active ) Fail( c, "snapshot flags" );
	if ( MSG_ReadByte( &in ) != 0 ) Fail( c, "area bytes" );
	MSG_ReadData( &in, nullptr, 0 );
	return in;
}

static bool Refused( const Case &c ) {
	auto client = Client( c, CS_ZOMBIE );
	msg_t out = Open( *client );
	try {
		SV_WriteSnapshotToClient( client.get(), &out );
	} catch ( Drop & ) {
		return true;
	}
	return false;
}

static void Zombie( const Case &c ) {
	auto client = Client( c, CS_ZOMBIE );
	msg_t out = Open( *client );
	try {
		SV_WriteSnapshotToClient( client.get(), &out );
	} catch ( Drop & ) {
		Fail( c, "a zombie's snapshot was refused" );
		return;
	}
	if ( out.overflowed ) Fail( c, "overflowed" );

	// The reader written from the original engines.
	msg_t in = Header( c, *client, out, false );
	Reader reader{ in, in.bit };
	try {
		for ( int width : c.empty )
			if ( reader.Read( width ) != 0 ) { Fail( c, "the player state is not an empty delta" ); return; }
		if ( reader.Read( GENTITYNUM_BITS ) != MAX_GENTITIES - 1 ) { Fail( c, "no entity end marker where the reader looks" ); return; }
		for ( int width : c.tail )
			if ( reader.Read( width ) != 0 ) { Fail( c, "the snapshot's tail" ); return; }
	} catch ( Short & ) {
		Fail( c, "the reader ran off the message" );
		return;
	}
	if ( reader.bit != out.bit ) Fail( c, "the snapshot is longer than an empty one" );

	// The engine's own reader.
	in = Header( c, *client, out, false );
	playerState_t decoded{};
	if ( !MSG_ReadDeltaPlayerstateForWireProfile( &in, nullptr, &decoded, c.profile ) ) Fail( c, "the engine's reader refused it" );
	if ( MSG_ReadEntitynum( &in ) != MAX_GENTITIES - 1 ) Fail( c, "the engine's reader found no end marker" );
	if ( c.tail.empty() && in.bit != out.bit ) Fail( c, "the engine's reader left bits over" );
}

// A client in the game still gets its own player state, whatever a zombie is sent.
static void Active( const Case &c ) {
	auto client = Client( c, CS_ACTIVE );
	msg_t out = Open( *client );
	try {
		SV_WriteSnapshotToClient( client.get(), &out );
	} catch ( Drop & ) {
		Fail( c, "an active client's snapshot was refused" );
		return;
	}
	const playerState_t &sent = client->frames[ 40 & PACKET_MASK ].ps;
	std::vector<byte> scratch( 1 << 16 );
	msg_t alone;
	MSG_Init( &alone, scratch.data(), int( scratch.size() ) );
	if ( SV_LegacyFraming( client.get() ) ) MSG_STVEFBitstream( &alone );
	MSG_WriteDeltaPlayerstateForWireProfile( &alone, nullptr, &sent, c.profile );
	msg_t back = alone;
	back.readcount = 0;
	back.bit = 0;
	playerState_t expected{}, decoded{};
	MSG_ReadDeltaPlayerstateForWireProfile( &back, nullptr, &expected, c.profile );

	msg_t in = Header( c, *client, out, true );
	if ( !MSG_ReadDeltaPlayerstateForWireProfile( &in, nullptr, &decoded, c.profile ) ||
		std::memcmp( &decoded, &expected, sizeof decoded ) != 0 ) Fail( c, "an active client's player state" );
	if ( MSG_ReadEntitynum( &in ) != MAX_GENTITIES - 1 ) Fail( c, "an active client's entities" );

	in = Header( c, *client, out, true );
	Reader reader{ in, in.bit };
	bool empty = true;
	try {
		for ( int width : c.empty ) empty = empty && reader.Read( width ) == 0;
	} catch ( Short & ) {
		empty = false;
	}
	if ( empty ) Fail( c, "an active client was sent a zombie's empty player state" );
}

int main() {
	// A residual no delta, flag or area byte could be mistaken for.
	sv.timeResidual = 37;
	for ( const Case &c : cases ) {
		if ( c.refused ) {
			if ( !Refused( c ) ) Fail( c, "a profile without a layout wrote a snapshot" );
			continue;
		}
		Zombie( c );
		Active( c );
	}
	const Case unregistered{ "unregistered", static_cast<netchanWireProfile_t>( 0x7f ), true, {}, {} };
	if ( !Refused( unregistered ) ) Fail( unregistered, "an unregistered profile wrote a snapshot" );
	std::printf( "%d failures\n", failures );
	return failures ? 1 : 0;
}
'''

ZOMBIE_WRITE = ('MSG_WriteDeltaPlayerstateForWireProfile( msg, &unchanged, &unchanged,\n'
                '\t\t\tclient->netchan.wireProfile )')
QUAKE3_EMPTY = '( MSG_WriteByte( msg, 0 ), MSG_WriteBits( msg, 0, 1 ), qtrue )'
ZOMBIE_ERROR = ('\t\t\tCom_Error( ERR_DROP, "%s: invalid wire profile %i",\n'
                '\t\t\t\t__func__, static_cast<int>( client->netchan.wireProfile ) );\n')

MUTATIONS = {
    "Quake III's empty delta for every profile": (ZOMBIE_WRITE, QUAKE3_EMPTY),
    'Quake III\'s for all but raw-bit framing, as before': (
        ZOMBIE_WRITE,
        '( SV_LegacyFraming( client ) ? ' + ZOMBIE_WRITE + ' : ' + QUAKE3_EMPTY + ' )'),
    "RTCW's empty delta for every profile": (
        ZOMBIE_WRITE, '( MSG_WriteByte( msg, 0 ), MSG_WriteBits( msg, 0, 6 ), qtrue )'),
    "Quake Live's encoding whatever the client's": (
        ZOMBIE_WRITE, ZOMBIE_WRITE.replace('client->netchan.wireProfile', 'NETCHAN_WIRE_QL_RETAIL')),
    "the slot's frame from nothing, as retail RTCW and ET send": (
        ZOMBIE_WRITE, ZOMBIE_WRITE.replace(
            '&unchanged, &unchanged', 'nullptr, &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ].ps')),
    'a write the profile cannot make goes on regardless': (ZOMBIE_ERROR, '\t\t\t;\n'),
    'no entity end marker': ('\t\tMSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );\n', ''),
    "every client is sent a zombie's snapshot": (
        'if ( client->state <= CS_ZOMBIE ) {', 'if ( client->state <= CS_ACTIVE ) {'),
    "a zombie is sent its frame's delta": (
        'if ( client->state <= CS_ZOMBIE ) {', 'if ( client->state < CS_ZOMBIE ) {'),
}


def production():
    snapshot = (ROOT / 'code/server/sv_snapshot.cpp').read_text(encoding='utf-8').replace('\r\n', '\n')
    parts = {'emit': function(snapshot, 'static void SV_EmitPacketEntities('),
             'write': function(snapshot, 'static void SV_WriteSnapshotToClient( const client_t *client, msg_t *msg ) {')}
    if 'SV_WriteAliceSnapshotTail(' in parts['write']:
        parts = dict(tail=function(snapshot, 'static void SV_WriteAliceSnapshotTail('), **parts)
    if 'SV_WriteRitualTimeResidual(' in parts['write']:
        parts = dict(residual=function(snapshot, 'static void SV_WriteRitualTimeResidual('), **parts)
    return parts


def cases(names):
    rows = []
    for name in names:
        empty = EMPTY[name]
        widths = ', '.join(str(width) for width in empty or ())
        tail = ', '.join(str(width) for width in TAIL.get(name, ()))
        rows.append('\t{ "%s", %s, %s, { %s }, { %s } },' % (
            name, name, 'true' if empty is None else 'false', widths, tail))
    return 'static const std::vector<Case> cases{\n' + '\n'.join(rows) + '\n};\n'


def source(parts, names):
    text = '\n'.join(parts.values())
    helper = ERROR_HELPER if 'SV_ErrorForWireProfile(' in text else ''
    head, checks = CHECKS.split('static int failures;')
    return PRELUDE + helper + text + '\n' + head + cases(names) + 'static int failures;' + checks


class ServerZombieSnapshotTests(unittest.TestCase):
    def test_every_registered_profile_pins_its_empty_delta(self):
        names = wire_profiles()
        self.assertIn('NETCHAN_WIRE_RTCW_MP', names)
        self.assertEqual(sorted(set(names) - set(EMPTY)), [])

    def test_a_quitting_server_sends_every_client_a_zombies_snapshot(self):
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8').replace('\r\n', '\n')
        final = function(init, 'static void SV_FinalMessage( const char *message ) {')
        zombie = final.index('client.state = CS_ZOMBIE;')
        self.assertLess(final.index('"disconnect \\"%s\\""'), zombie)
        self.assertLess(zombie, final.index('SV_SendClientSnapshot( &client );'))

    def run_probe(self, folder, name, text, objects):
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(text, encoding='utf-8')
        built = subprocess.run([self.compiler, *self.flags, str(cpp), *objects, '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=180)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        return subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)

    def test_each_profile_reads_a_zombies_snapshot(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(self.compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        common = [*target, '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT)]
        self.flags = [*common, '-std=c++17']
        names = wire_profiles()
        parts = production()
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='zombie-snapshot-', dir=ROOT / '.tmp') as directory:
            folder = Path(directory)
            objects = []
            for name, path, language in (
                    ('msg', 'code/qcommon/msg.cpp', ['-std=c++17', '-DDEDICATED']),
                    ('huffman', 'code/qcommon/huffman_static.c', ['-x', 'c', '-DDEDICATED'])):
                built = subprocess.run([self.compiler, *common, *language, '-c', str(ROOT / path),
                                        '-o', str(folder / f'{name}.o')],
                                       cwd=ROOT, capture_output=True, text=True, timeout=180)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                objects.append(str(folder / f'{name}.o'))
            ran = self.run_probe(folder, 'production', source(parts, names), objects)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            for index, (description, (old, new)) in enumerate(MUTATIONS.items()):
                with self.subTest(mutation=description):
                    self.assertEqual(parts['write'].count(old), 1, old)
                    mutated = dict(parts, write=parts['write'].replace(old, new))
                    ran = self.run_probe(folder, f'mutation{index}', source(mutated, names), objects)
                    self.assertNotEqual(ran.returncode, 0, description + '\n' + ran.stdout)


if __name__ == '__main__':
    unittest.main()
