"""American McGee's Alice's demos: record, stoprecord and demo, per alice.exe.

Retail alice.exe registers `record` (CL_Record_f 0x0042a410), `demo`
(CL_PlayDemo_f 0x0042a890) and `stoprecord` (CL_StopRecord_f 0x0042a2e0).
Its file is Quake III's container under one fixed extension, `.dm3`: a
gamestate record the recorder synthesizes, then each parsed message as a
little-endian sequence, length and bytes, then two -1 words.

These tests hold the engine to that contract without retail files: the name
rules in code/client/client_alice_demo.hpp, the null entity every gamestate
baseline is delta-coded from, and a gamestate written the recorder's way and
read back through the production envelope reader and Alice codec. When the
retail executable is present they also pin every constant here to its bytes.
The executable is only read.
"""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
ALICE_ROOT = Path(os.environ.get('FNTECH3_ALICE_ROOT', r'E:\Games\Alice'))


def compiler():
    return next((path for name in ('clang++', 'g++', 'c++') if (path := shutil.which(name))), None)


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]


NAME_RULES = r'''
#include "code/client/client_alice_demo.hpp"
#include <cassert>
#include <cstring>
#include <set>
#include <string>

using namespace fntech3::alice::demo;

int main() {
    char name[kNameBytes];
    char path[kPathBytes];
    static_assert( kNameBytes == 64 && kPathBytes == 128 && kLastAutomaticNumber == 9999 );
    assert( !std::strcmp( kExtension, "dm3" ) );

    // CL_DemoFilename 0x0042a360: four digits, and the screenshot namer's
    // leftover out of range.
    const std::pair<int, const char *> numbered[] = {
        { 0, "demo0000" }, { 7, "demo0007" }, { 42, "demo0042" }, { 305, "demo0305" },
        { 1234, "demo1234" }, { 9999, "demo9999" }, { -1, "demo9999.tga" }, { 10000, "demo9999.tga" } };
    for ( const auto &[number, expected] : numbered ) {
        AutomaticName( number, name, sizeof( name ) );
        assert( !std::strcmp( name, expected ) );
    }

    RecordPath( "garden", path, sizeof( path ) );
    assert( !std::strcmp( path, "demos/garden.dm3" ) );

    // A typed name is kept, extension and all, and cut to 63 characters.
    ExplicitName( "garden.dm3", name );
    assert( !std::strcmp( name, "garden.dm3" ) );
    ExplicitName( std::string( 100, 'n' ).c_str(), name );
    assert( std::string( name ) == std::string( 63, 'n' ) );

    // The first number whose file does not read back as longer than nothing.
    std::set<std::string> taken;
    int probes = 0;
    auto length = [&]( const char *candidate ) { ++probes; return taken.count( candidate ) ? 100 : -1; };
    AutomaticRecordName( length, name, path, sizeof( path ) );
    assert( !std::strcmp( name, "demo0000" ) && !std::strcmp( path, "demos/demo0000.dm3" ) && probes == 1 );
    taken = { "demos/demo0000.dm3", "demos/demo0001.dm3", "demos/demo0002.dm3" };
    probes = 0;
    AutomaticRecordName( length, name, path, sizeof( path ) );
    assert( !std::strcmp( name, "demo0003" ) && probes == 4 );
    // An empty file is free (FS_ReadFile( name, NULL ) <= 0 at 0x0042a50e).
    probes = 0;
    AutomaticRecordName( [&]( const char * ) { ++probes; return 0; }, name, path, sizeof( path ) );
    assert( !std::strcmp( name, "demo0000" ) && probes == 1 );
    // Every number taken leaves demo9999, which the recorder overwrites.
    probes = 0;
    AutomaticRecordName( [&]( const char * ) { ++probes; return 1; }, name, path, sizeof( path ) );
    assert( !std::strcmp( name, "demo9999" ) && !std::strcmp( path, "demos/demo9999.dm3" ) && probes == 10000 );

    // CL_PlayDemo_f 0x0042a8dd: strstr( arg, ".dm3" ), case sensitive.
    const std::pair<const char *, const char *> played[] = {
        { "garden", "demos/garden.dm3" }, { "garden.dm3", "demos/garden.dm3" },
        { "garden.dm3.bak", "demos/garden.dm3.bak" }, { "garden.DM3", "demos/garden.DM3.dm3" },
        { "sub/garden", "demos/sub/garden.dm3" }, { "garden.dm_68", "demos/garden.dm_68.dm3" } };
    for ( const auto &[argument, expected] : played ) {
        PlaybackPath( argument, path, sizeof( path ) );
        assert( !std::strcmp( path, expected ) );
    }
    // A long argument is cut to the path buffer, never past it.
    PlaybackPath( std::string( 300, 'p' ).c_str(), path, sizeof( path ) );
    assert( std::strlen( path ) == kPathBytes - 1 );

    assert( !std::strcmp( kSynchronousClientsCvar, "g_syncronousClients" ) );
    assert( !std::strcmp( kSynchronousClientsRefusal,
        "The server must have 'g_syncronousClients 1' set for demos\n" ) );
    return 0;
}
'''


ROUND_TRIP = r'''
#include "code/compat/alice_message_codec.hpp"
#include "code/client/demo_stream.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace message = fntech3::alice::message;
using Entity = message::Entity;

// alice.exe fills its null entity at 0x00446460; these are the offsets its
// stores name.
static_assert( offsetof( Entity, constantLight ) == 0x90 );
static_assert( offsetof( Entity, parent ) == 0xa0 );
static_assert( offsetof( Entity, bonemod ) == 0xd8 && sizeof( Entity::bonemod[0] ) == 36 );
static_assert( offsetof( Entity, scale ) == 0x1b8 );
static_assert( offsetof( Entity, alpha ) == 0x1bc );
static_assert( offsetof( Entity, renderfx ) == 0x1c0 );
static_assert( sizeof( Entity ) == 512 );

// LSB-first bits, as MSG_WriteBits at 0x00445520 writes them.
struct Bits {
    std::vector<std::uint8_t> data;
    std::size_t cursor = 0;
    bool good = true;
    void Write( std::uint32_t value, int bits ) {
        const int width = bits < 0 ? -bits : bits;
        for ( int i = 0; i < width; ++i, ++cursor ) {
            if ( ( cursor >> 3 ) >= data.size() ) data.push_back( 0 );
            if ( ( value >> i ) & 1u ) data[cursor >> 3] |= static_cast<std::uint8_t>( 1u << ( cursor & 7 ) );
        }
    }
    std::uint32_t Read( int bits ) {
        const int width = bits < 0 ? -bits : bits;
        std::uint32_t value = 0;
        for ( int i = 0; i < width; ++i, ++cursor ) {
            if ( ( cursor >> 3 ) >= data.size() ) { good = false; return 0; }
            if ( ( data[cursor >> 3] >> ( cursor & 7 ) ) & 1u ) value |= 1u << i;
        }
        if ( bits < 0 && width < 32 && ( value >> ( width - 1 ) ) & 1u ) value |= ~0u << width;
        return value;
    }
    bool Good() const { return good; }
    bool Fail() { good = false; return false; }
};

static void Append32( std::vector<std::uint8_t> &out, std::int32_t value ) {
    const auto word = static_cast<std::uint32_t>( value );
    for ( int i = 0; i < 4; ++i ) out.push_back( static_cast<std::uint8_t>( word >> ( 8 * i ) ) );
}

static bool Same( const Entity &a, const Entity &b ) {
    for ( const auto &field : message::EntityFields ) {
        if ( message::Word( a, field.offset ) != message::Word( b, field.offset ) ) return false;
    }
    return a.number == b.number && !std::memcmp( a.tagged, b.tagged, sizeof( a.tagged ) );
}

// The recorder's gamestate record (CL_Record_f 0x0042a585..0x0042a6d7):
// svc_gamestate, the command sequence, each baseline as svc_baseline and a
// forced delta from the null entity, and a zero byte. No acknowledgement
// leads it and nothing follows the zero.
static std::vector<std::uint8_t> Gamestate( const Entity &null, const std::vector<Entity> &baselines ) {
    Bits bits;
    bits.Write( 2, 8 );
    bits.Write( 17, 32 );
    for ( const Entity &baseline : baselines ) {
        bits.Write( 4, 8 );
        assert( message::WriteEntity( bits, &null, &baseline, true ) );
    }
    bits.Write( 0, 8 );
    return bits.data;
}

static std::vector<Entity> Parse( const std::vector<std::uint8_t> &body, const Entity &null ) {
    Bits bits;
    bits.data = body;
    assert( bits.Read( 8 ) == 2 && bits.Read( 32 ) == 17 );
    std::vector<Entity> out;
    for ( ;; ) {
        const std::uint32_t command = bits.Read( 8 );
        if ( !bits.Good() || command == 0 ) break;
        assert( command == 4 );
        const int number = static_cast<int>( bits.Read( message::kEntityNumberBits ) );
        Entity entity{};
        assert( message::ReadEntity( bits, &null, &entity, number ) );
        out.push_back( entity );
    }
    return out;
}

int main() {
    Entity retailNull{};
    message::InitNullEntity( retailNull );
    const Entity zeroNull{};
    std::uint8_t expected[512] = {};
    const std::uint32_t light = 0x00ffffff, parent = 1023, fx = 0x10, one = 0x3f800000;
    std::memcpy( expected + 0x90, &light, 4 );
    std::memcpy( expected + 0xa0, &parent, 4 );
    std::memcpy( expected + 0x1b8, &one, 4 );
    std::memcpy( expected + 0x1bc, &one, 4 );
    std::memcpy( expected + 0x1c0, &fx, 4 );
    for ( int i = 0; i < 5; ++i ) std::memset( expected + 0xd8 + 36 * i, 0xff, 4 );
    assert( !std::memcmp( &retailNull, expected, sizeof( expected ) ) );

    // A plain entity: most of it equal to the null, as a level's are.
    Entity plain = retailNull;
    plain.number = 12;
    plain.modelindex = 3;
    plain.netorigin[0] = 128.0f; plain.netorigin[1] = -64.5f; plain.netorigin[2] = 24.0f;
    // One whose members are zero where the null's are not.
    Entity zeroed{};
    zeroed.number = 40;
    zeroed.eType = 2;
    zeroed.tagged[5] = 9;
    const std::vector<Entity> baselines = { plain, zeroed };

    // Recorded: gamestate record at sequence - 1, one later record, trailer.
    const std::vector<std::uint8_t> gamestate = Gamestate( retailNull, baselines );
    std::vector<std::uint8_t> file;
    Append32( file, 172 );
    Append32( file, static_cast<std::int32_t>( gamestate.size() ) );
    file.insert( file.end(), gamestate.begin(), gamestate.end() );
    const std::uint8_t later[] = { 7, 1, 2, 3 };
    Append32( file, 174 );
    Append32( file, 4 );
    file.insert( file.end(), later, later + 4 );
    Append32( file, -1 );
    Append32( file, -1 );

    // Played back through the engine's envelope reader.
    std::size_t cursor = 0;
    auto reader = [&]( std::uint8_t *destination, std::size_t bytes ) -> std::ptrdiff_t {
        const std::size_t n = bytes < file.size() - cursor ? bytes : file.size() - cursor;
        std::memcpy( destination, file.data() + cursor, n );
        cursor += n;
        return static_cast<std::ptrdiff_t>( n );
    };
    std::vector<std::uint8_t> payload( 49152 );
    auto record = fnql::demo::ReadEnvelope( reader, payload.data(), payload.size() );
    assert( record.status == fnql::demo::EnvelopeStatus::Message && record.sequence == 172 );
    payload.resize( static_cast<std::size_t>( record.payloadLength ) );
    const std::vector<Entity> parsed = Parse( payload, retailNull );
    assert( parsed.size() == 2 && Same( parsed[0], plain ) && Same( parsed[1], zeroed ) );
    payload.assign( 49152, 0 );
    record = fnql::demo::ReadEnvelope( reader, payload.data(), payload.size() );
    assert( record.status == fnql::demo::EnvelopeStatus::Message && record.sequence == 174 && record.payloadLength == 4 );
    record = fnql::demo::ReadEnvelope( reader, payload.data(), payload.size() );
    assert( record.status == fnql::demo::EnvelopeStatus::EndOfStreamTrailer );

    // The null is part of the format. Read against a zero one, a retail
    // baseline loses every member it left implicit: this entity's alpha,
    // scale and parent come back zero.
    const std::vector<Entity> misread = Parse( gamestate, zeroNull );
    assert( !Same( misread[0], plain ) && misread[0].alpha == 0.0f && misread[0].parent == 0 );
    // Written against a zero one, a retail reader turns zeros into the null's.
    const std::vector<Entity> skewed = Parse( Gamestate( zeroNull, baselines ), retailNull );
    assert( !Same( skewed[1], zeroed ) && skewed[1].alpha == 1.0f && skewed[1].parent == 1023 );
    // And it costs: the retail null leaves the defaults off the wire.
    assert( Gamestate( retailNull, { plain } ).size() < Gamestate( zeroNull, { plain } ).size() );
    return 0;
}
'''


class AliceDemoTests(unittest.TestCase):
    def compile_and_run(self, source, prefix):
        path = compiler()
        if path is None:
            self.skipTest('requires a C++17 compiler')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([path, *compiler_target_args(path), '-std=c++17', '-Wall', '-Wextra',
                                    '-Werror', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_names_follow_alice_exe(self):
        self.compile_and_run(NAME_RULES, 'alice-demo-names-')

    def test_a_recorded_gamestate_reads_back_against_the_retail_null(self):
        self.compile_and_run(ROUND_TRIP, 'alice-demo-round-trip-')

    def test_every_gamestate_writer_and_reader_uses_the_profile_null(self):
        # A gamestate's baselines are deltas from the null entity: the server,
        # the client's parse and the demo recorder must agree on it, or every
        # member left implicit changes on the way.
        server = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
        parse = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        for text, signature, use in (
                (server, 'static void SV_SendClientGameState( client_t *client ) {',
                 'SV_WriteBaselineMessage( msg, nullstate,'),
                (server, 'static void SV_WriteDemoGamestate( client_t *client )',
                 'SV_WriteBaselineMessage( msg, nullstate,'),
                (parse, 'static void CL_ParseGamestate( msg_t *msg ) {',
                 'CL_ReadDeltaEntity( msg, &nullstate, es, newnum );'),
                (main, 'static void CL_WriteGamestate( qboolean initial )',
                 '&msg, &nullstate, ent, qtrue );')):
            with self.subTest(function=signature):
                body = function(text, signature)
                initialised = body.find('MSG_NullEntityStateForWireProfile( &nullstate,')
                self.assertGreaterEqual(initialised, 0)
                self.assertLess(initialised, body.index(use))
        msg = (ROOT / 'code/qcommon/msg.cpp').read_text(encoding='utf-8')
        null = function(msg, 'void MSG_NullEntityStateForWireProfile(')
        self.assertIn('if ( MSG_IsAliceWireProfile( profile ) ) {', null)
        self.assertIn('*state = {};', null)

    def test_playback_never_rerecords_an_alice_demo(self):
        # Quake III's snapshot writer has no room for Alice's sound block, and
        # alice.exe's reader copies nothing to a recording (0x0042a760).
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        body = function(main, 'void CL_ReadDemoMessage( void )')
        self.assertIn('if ( clc.demorecording && CL_ActiveWireProfile() != NETCHAN_WIRE_ALICE ) {', body)

    def test_constants_match_retail_alice_exe(self):
        exe = ALICE_ROOT / 'alice.exe'
        if not exe.is_file():
            self.skipTest('retail alice.exe not present')
        data = exe.read_bytes()
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        self.assertEqual(data[pe:pe + 4], b'PE\0\0')
        count = struct.unpack_from('<H', data, pe + 6)[0]
        optional_size = struct.unpack_from('<H', data, pe + 20)[0]
        base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
        sections = []
        for i in range(count):
            entry = pe + 24 + optional_size + 40 * i
            virtual_size, address, raw_size, raw = struct.unpack_from('<IIII', data, entry + 8)
            sections.append((base + address, max(virtual_size, raw_size), raw, raw_size))

        def read(address, size):
            for start, length, raw, raw_size in sections:
                if start <= address < start + length:
                    offset = address - start
                    self.assertLessEqual(offset + size, raw_size)
                    return data[raw + offset:raw + offset + size]
            self.fail('0x%08x is outside every section' % address)

        def text(address):
            chunk = read(address, 128)
            return chunk[:chunk.index(b'\0')].decode('latin-1')

        header = (ROOT / 'code/client/client_alice_demo.hpp').read_text(encoding='utf-8')
        refusal = re.search(r'kSynchronousClientsRefusal =\s*"([^"]*)\\n";', header).group(1) + '\n'
        strings = {
            0x0050f314: 'demos/%s.dm3', 0x0050f444: 'demos/%s', 0x0050f450: '.dm3',
            0x0050f2d8: 'demo%i%i%i%i', 0x0050f2c8: 'demo9999.tga',
            0x0050f360: 'g_syncronousClients', 0x0050f324: refusal,
            0x0050fc40: 'record', 0x0050f578: 'demo', 0x0050fc28: 'stoprecord',
            0x0050f374: 'You must be in a level to record.\n', 0x0050f300: 'recording to %s.\n',
            0x0050f3e8: 'Demo file was truncated.\n', 0x0050fe38: 'timedemo',
        }
        for address, expected in strings.items():
            with self.subTest(address=hex(address)):
                self.assertEqual(text(address), expected)

        def call_target(address):
            self.assertEqual(read(address, 1), b'\xe8')
            return (address + 5 + struct.unpack('<i', read(address + 1, 4))[0]) & 0xffffffff

        code = {
            # Cmd_AddCommand( "record" / "demo" / "stoprecord", handler )
            0x0042cba0: '68 10 a4 42 00 68 40 fc 50 00 e8',
            0x0042cbaf: '68 90 a8 42 00 68 78 f5 50 00 e8',
            0x0042cbcd: '68 e0 a2 42 00 68 28 fc 50 00 e8',
            # the refusal's cvar, the recorder's and the player's formats
            0x0042a476: '68 60 f3 50 00', 0x0042a4c8: '68 14 f3 50 00', 0x0042a8d7: '68 50 f4 50 00',
            # 64-byte demo name, the 9,999 bound, 48 KB messages both ways
            0x0042a4b6: '6a 40', 0x0042a51f: '81 fe 0f 27 00 00',
            0x0042a585: '68 00 c0 00 00', 0x0042a7b3: '68 00 c0 00 00',
            # the gamestate goes out at serverMessageSequence - 1
            0x0042a683: 'a1 90 77 bf 00 48',
            # stoprecord writes -1 twice
            0x0042a306: 'c7 44 24 0c ff ff ff ff',
            # the null entity: clear 128 dwords, then the members InitNullEntity sets
            0x00446460: '8b 54 24 04 57 b9 80 00 00 00 33 c0 8b fa f3 ab b8 00 00 80 3f '
                        'c7 82 a0 00 00 00 ff 03 00 00 89 82 bc 01 00 00 89 82 b8 01 00 00 '
                        'c7 82 90 00 00 00 ff ff ff 00 c7 82 c0 01 00 00 10 00 00 00 '
                        '8d 82 dc 00 00 00 ba 05 00 00 00 33 c9 5f c7 40 fc ff ff ff ff',
        }
        for address, expected in code.items():
            with self.subTest(address=hex(address)):
                wanted = bytes.fromhex(expected)
                self.assertEqual(read(address, len(wanted)), wanted)
        # Its three callers: the recorder, the client's parse and the server's gamestate.
        for address in (0x0042a60b, 0x0042e0ab, 0x00452b6f):
            with self.subTest(caller=hex(address)):
                self.assertEqual(call_target(address), 0x00446460)
        # Each registration's call is Cmd_AddCommand's.
        targets = {call_target(address + 10) for address in (0x0042cba0, 0x0042cbaf, 0x0042cbcd)}
        self.assertEqual(len(targets), 1)


if __name__ == '__main__':
    unittest.main()
