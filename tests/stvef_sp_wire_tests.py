"""Elite Force SP's loopback message bodies.

Retail stvoy.exe talks only to itself and records no demos, so its wire is
never read by another program; fnTech3's bodies send each record as 32-bit
words, counted up to the last changed one, each changed word in the shortest
of five lossless forms. The probe drives them through a bit buffer with
random records, deltas, every word form and its boundaries, and checks that
every field, and every removal, arrives exactly and that a placed model's
baseline stays small; the source check pins their use for the SP wire.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include "code/compat/stvef_message_codec.hpp"
#include <cassert>
#include <cstring>
#include <random>
#include <vector>
namespace message = fntech3::stvef::message::sp;
struct Bits {
	std::vector<std::uint8_t> data;
	std::size_t writeBit = 0, readBit = 0;
	bool failed = false;
	void Write( std::uint32_t value, int bits ) {
		for ( int i = 0; i < bits; ++i, ++writeBit ) {
			if ( ( writeBit >> 3 ) >= data.size() ) data.push_back( 0 );
			if ( ( value >> i ) & 1u ) data[writeBit >> 3] |= static_cast<std::uint8_t>( 1u << ( writeBit & 7 ) );
		}
	}
	std::uint32_t Read( int bits ) {
		std::uint32_t value = 0;
		for ( int i = 0; i < bits; ++i, ++readBit ) {
			if ( readBit >= writeBit ) { failed = true; return 0; }
			value |= static_cast<std::uint32_t>( ( data[readBit >> 3] >> ( readBit & 7 ) ) & 1u ) << i;
		}
		return value;
	}
	bool Good() const { return !failed; }
	bool Fail() { failed = true; return false; }
};
template<class T> static void Randomize( T &record, std::mt19937 &random, int percent ) {
	auto *bytes = reinterpret_cast<unsigned char *>( &record );
	for ( std::size_t i = 0; i < sizeof( record ); ++i )
		if ( static_cast<int>( random() % 100 ) < percent ) bytes[i] = static_cast<unsigned char>( random() );
}
int main() {
	std::mt19937 random( 1234 );
	for ( int round = 0; round < 2000; ++round ) {
		// Entities: a baseline, a delta of it, and a removal.
		message::Entity base{}, next{}, decoded{};
		Randomize( base, random, 100 );
		base.number = static_cast<int>( random() % 1024 );
		next = base;
		Randomize( next, random, round % 50 );
		next.number = base.number;
		Bits bits;
		assert( message::WriteEntity( bits, &base, &next, false ) );
		if ( std::memcmp( &base, &next, sizeof( base ) ) ) {
			assert( static_cast<int>( bits.Read( 10 ) ) == next.number );
			assert( message::ReadEntity( bits, &base, &decoded, next.number ) );
			assert( !std::memcmp( &decoded, &next, sizeof( next ) ) && bits.Good() );
		} else {
			// Unchanged and not forced: nothing is sent.
			assert( bits.writeBit == 0 );
		}
		Bits forced;
		assert( message::WriteEntity( forced, &next, &next, true ) );
		assert( static_cast<int>( forced.Read( 10 ) ) == next.number );
		assert( message::ReadEntity( forced, &next, &decoded, next.number ) );
		assert( !std::memcmp( &decoded, &next, sizeof( next ) ) );
		Bits removal;
		assert( message::WriteEntity( removal, &next, nullptr, false ) );
		assert( static_cast<int>( removal.Read( 10 ) ) == next.number );
		assert( message::ReadEntity( removal, &next, &decoded, next.number ) && decoded.number == 1023 );

		// Players and commands, from nothing and from a previous record.
		message::Player player{}, playerNext{}, playerDecoded{};
		Randomize( player, random, 100 );
		playerNext = player;
		Randomize( playerNext, random, round % 30 );
		const message::Player *froms[] = { nullptr, &player };
		for ( const message::Player *from : froms ) {
			Bits wire;
			assert( message::WritePlayer( wire, from, &playerNext ) );
			assert( message::ReadPlayer( wire, from, &playerDecoded ) );
			assert( !std::memcmp( &playerDecoded, &playerNext, sizeof( playerNext ) ) && wire.readBit == wire.writeBit );
		}
		message::Command command{}, commandNext{}, commandDecoded{};
		Randomize( command, random, 100 );
		commandNext = command;
		Randomize( commandNext, random, 40 );
		Bits wire;
		assert( message::WriteCommand( wire, &command, &commandNext ) );
		assert( message::ReadCommand( wire, &command, &commandDecoded ) );
		assert( !std::memcmp( &commandDecoded, &commandNext, sizeof( commandNext ) ) );
	}
	// Every word form, its boundaries, and values that must stay exact: the
	// negative zero, a NaN and a denormal are not whole floats.
	using fntech3::stvef::message::FloatWord;
	const std::uint32_t specials[] = { 0, 1, 0x7f, 0x80, 0xffffff80u, 0xffffff7fu, 0x7fff, 0x8000,
		0xffff8000u, 0xffff7fffu, FloatWord( 1.0f ), FloatWord( -1.0f ), FloatWord( 4095.0f ),
		FloatWord( 4096.0f ), FloatWord( -4096.0f ), FloatWord( -4097.0f ), FloatWord( 0.5f ),
		0x80000000u, 0x7fc00000u, 0xffffffffu, 0x00400000u };
	for ( std::uint32_t value : specials ) {
		message::Player from{}, to{}, decoded{};
		std::memcpy( &to.origin[1], &value, 4 );
		std::memcpy( &to.ping, &value, 4 );
		Bits wire;
		assert( message::WritePlayer( wire, &from, &to ) );
		assert( message::ReadPlayer( wire, &from, &decoded ) );
		assert( !std::memcmp( &decoded, &to, sizeof( to ) ) && wire.readBit == wire.writeBit );
	}
	// A gamestate's baseline of a placed model, with whole-number position and
	// angles, takes a few dozen bytes.
	message::Entity placed{};
	placed.number = 300;
	placed.eType = 4;
	placed.pos.trBase[0] = placed.origin[0] = 1024.0f;
	placed.pos.trBase[1] = placed.origin[1] = -512.0f;
	placed.pos.trBase[2] = placed.origin[2] = 64.0f;
	placed.apos.trBase[1] = placed.angles[1] = 90.0f;
	placed.modelindex = 45;
	placed.solid = 0x00202020;
	Bits baseline;
	assert( message::WriteEntity( baseline, nullptr, &placed, true ) );
	assert( baseline.writeBit <= 40 * 8 );
	message::Entity placedBack{};
	assert( static_cast<int>( baseline.Read( 10 ) ) == 300 );
	assert( message::ReadEntity( baseline, nullptr, &placedBack, 300 ) );
	assert( !std::memcmp( &placedBack, &placed, sizeof( placed ) ) );
	// A word count beyond the record is refused.
	Bits oversized;
	oversized.Write( 0, 10 + 1 );
	oversized.Write( 255, 8 );
	message::Entity refused{};
	oversized.Read( 10 );
	assert( !message::ReadEntity( oversized, nullptr, &refused, 5 ) );

	// A truncated message publishes nothing.
	message::Player player{}, decoded{};
	player.ping = 99;
	decoded.ping = 7;
	Bits wire;
	assert( message::WritePlayer( wire, nullptr, &player ) );
	wire.writeBit -= 3;
	assert( !message::ReadPlayer( wire, nullptr, &decoded ) && decoded.ping == 7 );
	// Entity numbers outside the array are refused.
	message::Entity entity{};
	entity.number = 1024;
	Bits bad;
	assert( !message::WriteEntity( bad, nullptr, &entity, true ) );
}
'''


class STVEFSPWireTests(unittest.TestCase):
    def test_records_arrive_exactly(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-sp-wire-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_sp_wire_uses_the_bodies(self):
        source = (ROOT / 'code/qcommon/msg.cpp').read_text(encoding='utf-8')
        for body in ('WriteCommand', 'ReadCommand', 'WriteEntity', 'ReadEntity', 'WritePlayer', 'ReadPlayer'):
            self.assertIn(f'fntech3::stvef::message::sp::{body}( io,', source)
        self.assertNotIn('if ( profile == NETCHAN_WIRE_STVEF_SP ) {\n\t\treturn qfalse;', source.replace('\r\n', '\n'))


if __name__ == '__main__':
    unittest.main()
