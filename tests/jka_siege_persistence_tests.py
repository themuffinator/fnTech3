"""Run Academy's siege round record imports (42 and 43) as the server serves them.

Retail jamp.exe 1.01's import 42 (0x45ae89) copies the game's 12-byte record
into three globals beside its game VM pointer (0x12a4d34..0x12a4d3f) and import
43 (0x45aeb3) copies them back, both answering 0. Nothing else in the engine
touches those words, so the record starts zeroed with the process and survives
every level change, restart and game module. This compiles the dispatcher's own
two cases, its record and SV_JKAClearGame out of code/server/server_jka_game.hpp,
with the server's own VM_ArgPtr and VM_CheckBounds, and runs them against a QVM
data segment and a native module's memory. See docs/fntech3/2026-10-15-jka-siege.md.
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
HOST = 'code/server/server_jka_game.hpp'


def between(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first)]


def source():
    host = (ROOT / HOST).read_text(encoding='utf-8')
    record = between(host, "/* Siege's round record, kept for the game",
                     'static fntech3_jka_siegePers_t jkaSiegePersistence{};')
    record += 'static fntech3_jka_siegePers_t jkaSiegePersistence{};\n'
    cases = between(host, "\t/* The round record's setter and getter", '\tcase Call::G_GET_ENTITY_TOKEN: {')
    clear = function(host, 'static void SV_JKAClearGame( void )')
    argument = function((ROOT / 'code/server/sv_game.cpp').read_text(encoding='utf-8'),
                        'static void *VM_ArgPtr( intptr_t intValue ) {')
    bounds = function((ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8'),
                      'void VM_CheckBounds( const vm_t *vm, unsigned int address, unsigned int length )')
    return r'''
#include "code/server/server.h"
#include "code/compat/jka_game_abi.h"
#include "code/compat/jka_module_abi.h"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
struct Drop { std::string text; };
void QDECL Com_Error( errorParm_t, const char *format, ... ) {
	char text[512]; va_list list; va_start( list, format );
	std::vsnprintf( text, sizeof( text ), format, list ); va_end( list );
	throw Drop{ text };
}
vm_t *gvm;
''' + argument + '\n' + bounds + r'''
// What SV_JKAClearGame clears besides, counted rather than built.
void *jkaSharedBuffer;
static struct { int cleared = 0; void Clear() { ++cleared; } } jkaNavigation;
static float jkaServerCullDistance;
static int icarusClears, assetClears;
static void SV_JKAIcarusClear() { ++icarusClears; }
static void SV_RavenAssetsClear() { ++assetClears; }
''' + record + '\n' + clear + r'''
static bool Dispatch( const intptr_t *args, intptr_t &result ) {
	using namespace fntech3::jka::module;
	result = 0;
	const Call call = Lookup( COM_GAME_JKA_MP, Module::GAME, Direction::IMPORT, args[0] );
	const auto pointer = [args]( int index ) { return VM_ArgPtr( args[index] ); };
	switch ( call ) {
''' + cases + r'''
	default: return false;
	}
}
template<class F> static std::string Refused( F attempt ) {
	try { attempt(); } catch ( Drop &drop ) { return drop.text; }
	assert( !"accepted" ); return {};
}
static void Words( const unsigned char *bytes, int32_t a, int32_t b, int32_t c ) {
	int32_t words[3]; std::memcpy( words, bytes, sizeof( words ) );
	assert( words[0] == a && words[1] == b && words[2] == c );
}
static_assert( sizeof( fntech3_jka_siegePers_t ) == 12, "three words, as retail copies" );
static_assert( offsetof( fntech3_jka_siegePers_t, beatingTime ) == 0 );
static_assert( offsetof( fntech3_jka_siegePers_t, lastTeam ) == 4 );
static_assert( offsetof( fntech3_jka_siegePers_t, lastTime ) == 8 );
static void Check( bool native ) {
	// A QVM's data segment: a power of two, so its mask bounds every address.
	alignas( 4 ) static std::array<unsigned char, 256> memory;
	memory.fill( 0xa5 );
	vm_t vm{};
	vm.dataBase = memory.data();
	vm.dataMask = uint32_t( memory.size() - 1 );
	vm.dataLength = vm.exactDataLength = uint32_t( memory.size() );
	if ( native ) vm.entryPoint = reinterpret_cast<decltype( vm.entryPoint )>( 1 );
	gvm = &vm;
	const auto address = [&]( size_t offset ) -> intptr_t {
		return native ? reinterpret_cast<intptr_t>( memory.data() + offset ) : intptr_t( offset );
	};
	const auto call = [&]( intptr_t ordinal, intptr_t record ) {
		intptr_t args[2] = { ordinal, record }, result = 77;
		assert( Dispatch( args, result ) && result == 0 );
	};

	// Asked for before any round set it: the process's zeroed record, and
	// only the record's own twelve bytes are written.
	if ( !native ) {
		call( 43, address( 16 ) );
		Words( memory.data() + 16, 0, 0, 0 );
		assert( memory[15] == 0xa5 && memory[28] == 0xa5 );
	}

	// A round sets it; the words are kept as handed, whatever they hold.
	const int32_t handed[3] = { 1, 2, 371234 };
	std::memcpy( memory.data() + 40, handed, sizeof( handed ) );
	const auto before = memory;
	call( 42, address( 40 ) );
	assert( memory == before );
	const int32_t odd[3] = { 7, -1, INT32_MIN };
	std::memcpy( memory.data() + 64, odd, sizeof( odd ) );
	call( 42, address( 64 ) );
	call( 43, address( 100 ) );
	Words( memory.data() + 100, 7, -1, INT32_MIN );
	call( 42, address( 40 ) );

	// The level ends: SV_JKAClearGame clears what it owns and not the record.
	const int clears = jkaNavigation.cleared;
	SV_JKAClearGame();
	assert( jkaNavigation.cleared == clears + 1 && !jkaSharedBuffer );
	memory.fill( 0x5a );
	call( 43, address( 4 ) );
	Words( memory.data() + 4, 1, 2, 371234 );
	assert( memory[3] == 0x5a && memory[16] == 0x5a );

	// A missing record is refused by name before anything is copied either
	// way; retail would fault reading or writing through it.
	auto unchanged = jkaSiegePersistence;
	assert( Refused( [&] { call( 42, 0 ); } ).find( "siege round record set was handed no record" ) != std::string::npos );
	assert( Refused( [&] { call( 43, 0 ); } ).find( "siege round record get was handed no record" ) != std::string::npos );
	assert( !std::memcmp( &unchanged, &jkaSiegePersistence, sizeof( unchanged ) ) );

	// A QVM record that leaves the data segment is refused by the bounds
	// check, and neither the record nor the segment changes.
	if ( !native ) {
		const auto segment = memory;
		for ( intptr_t outside : { intptr_t( memory.size() - 11 ), intptr_t( memory.size() - 4 ),
				intptr_t( memory.size() ), intptr_t( 0x7ffffff0 ), intptr_t( -4 ) } ) {
			assert( Refused( [&] { call( 42, outside ); } ) == "program tried to bypass data segment bounds" );
			assert( Refused( [&] { call( 43, outside ); } ) == "program tried to bypass data segment bounds" );
		}
		assert( memory == segment && !std::memcmp( &unchanged, &jkaSiegePersistence, sizeof( unchanged ) ) );
		// Near the segment's end a record is served while the shared check
		// admits it: the check keeps the segment's last byte back.
		call( 43, address( memory.size() - 16 ) );
		Words( memory.data() + memory.size() - 16, 1, 2, 371234 );
	}

	// Nothing else is served here: the case is only these two imports.
	intptr_t other[2] = { 41, 0 }, result = 0;
	assert( !Dispatch( other, result ) );
	other[0] = 44;
	assert( !Dispatch( other, result ) );
	gvm = nullptr;
}
int main() {
#ifdef _MSC_VER
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
#endif
	Check( false );
	// The record outlives the QVM it was set from: a native module asks next.
	Check( true );
	std::puts( "siege round record: ok" );
	return 0;
}
'''


class JKASiegePersistenceTests(unittest.TestCase):
    def test_round_record_is_copied_both_ways_and_outlives_the_level(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-siege-persistence'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary).resolve()
            cpp = folder / 'probe.cpp'
            exe = folder / 'probe.exe'
            cpp.write_text(source(), encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-O2', '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=folder, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('siege round record: ok', run.stdout)

    def test_the_dispatcher_serves_both_imports_and_never_clears_the_record(self):
        host = (ROOT / HOST).read_text(encoding='utf-8')
        cases = between(host, "\t/* The round record's setter and getter", '\tcase Call::G_GET_ENTITY_TOKEN: {')
        self.assertIn('case Call::G_SIEGEPERSSET:', cases)
        self.assertIn('case Call::G_SIEGEPERSGET:', cases)
        # The two are numbered as the retail game pushes them (0x200a6335,
        # 0x200a6355 in assets3.pk3's jampgamex86.dll).
        calls = (ROOT / 'code/compat/jka_module_calls.def').read_text(encoding='utf-8')
        self.assertIn('FNTECH3_JKA_MODULE_CALL(GAME, IMPORT, G_SIEGEPERSSET, 42)', calls)
        self.assertIn('FNTECH3_JKA_MODULE_CALL(GAME, IMPORT, G_SIEGEPERSGET, 43)', calls)
        # Only these two cases touch the record, and nothing resets it.
        declaration = 'static fntech3_jka_siegePers_t jkaSiegePersistence{};'
        self.assertEqual(host.count(declaration), 1)
        self.assertNotIn('jkaSiegePersistence', host.replace(declaration, '').replace(cases, ''))
        clear = function(host, 'static void SV_JKAClearGame( void )')
        self.assertNotIn('jkaSiegePersistence', clear)


if __name__ == '__main__':
    unittest.main()
