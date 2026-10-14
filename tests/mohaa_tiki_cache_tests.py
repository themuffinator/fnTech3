"""Medal of Honor: finding a model definition already in the TIKI caches.

The loading screen's bar leaves out of its total every resource the caches
already hold (OpenMoHAA's UI_IsResourceLoaded, here MOHAA_ResourceCached),
asking TIKI_FindTikiAnim for a model's definition and its parts. Upstream's
TIKI_FindTikiAnim looked up a buffer it never filled, so no definition was
ever found. The production lookups, the canonicaliser and the resource check
are compiled here from code/mohaa against a stand-in for corepp's map that
records the key it is asked for; the stack is filled with a pattern before
each call, so an unfilled buffer cannot pass by holding an earlier call's
name."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def slice_between(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first)]


def function_body(text, signature):
    first = text.index(signature)
    return text[first:text.index('\n}\n', first) + 3]


class MohaaTikiCacheTests(unittest.TestCase):
    def test_cached_definitions_are_found_by_their_canonical_name(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        cache = (ROOT / 'code/mohaa/tiki/tiki_cache.cpp').read_text()
        lookups = slice_between(cache, 'struct pchar {',
                                '/*\n===============\nTIKI_RegisterTikiAnimFlags')
        engine = (ROOT / 'code/mohaa/mohaa_engine.cpp').read_text()
        canonical = function_body(engine, 'void FS_CanonicalFilename( char *filename ) {')
        api = (ROOT / 'code/mohaa/mohaa_tiki_api.cpp').read_text()
        cached = function_body(api, 'qboolean MOHAA_ResourceCached( const char *name ) {')
        harness = r'''
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>

typedef enum { qfalse, qtrue } qboolean;
static void Q_strncpyz( char *dest, const char *src, int destsize ) {
	std::strncpy( dest, src, destsize - 1 );
	dest[destsize - 1] = 0;
}
template<typename T> int HashCode( const T &key );
template<> int HashCode<const char *>( const char *const &key ) {
	int hash = 0;
	for ( const char *p = key; *p; p++ ) hash = *p + 31 * hash;
	return hash;
}
struct dtikianim_t { const char *name; };
struct dtiki_t { const char *name; };
class skeletor_c;
#define TIKI_MAX_ENTITY_CACHE 1024

/* corepp's con_map, reduced to what the lookups use; it keeps the last key
 * it was asked for, read no further than the lookups' 1,024-byte buffer. */
static std::string lastKey;
template<typename K, typename V>
struct con_map {
	std::map<std::string, V> entries;
	V *find( const K &key ) {
		lastKey.assign( key.m_Value, strnlen( key.m_Value, 1024 ) );
		auto at = entries.find( lastKey );
		return at == entries.end() ? nullptr : &at->second;
	}
	V &operator[]( const K &key ) { return entries[key.m_Value]; }
};

''' + canonical + '\n' + lookups + r'''
static bool skeletonCached;
static bool SkeletorCacheFindFilename( const char *, int * ) { return skeletonCached; }
''' + cached + r'''

/* Fill the stack below the caller with a pattern, so a buffer the lookup
 * leaves unfilled holds no earlier call's name. */
__attribute__(( noinline )) static void Poison( void ) {
	volatile char pattern[16384];
	for ( std::size_t i = 0; i < sizeof( pattern ); i++ ) pattern[i] = 'Z';
}

int main() {
	/* No cache yet: nothing is found, and nothing is asked. */
	Poison();
	assert( TIKI_FindTikiAnim( "models/human/allied_soldier.tik" ) == NULL );
	assert( TIKI_FindTiki( "models/human/allied_soldier.tik" ) == NULL );

	/* TIKI_RegisterTikiAnimFlags and TIKI_RegisterTikiFlags key each entry
	 * by the canonical name they loaded. */
	static dtikianim_t soldier = { "models/human/allied_soldier.tik" };
	static dtiki_t soldierModel = { "models/human/allied_soldier.tik" };
	tikianimcache = new con_map<pchar, dtikianim_t *>;
	tikicache = new con_map<pchar, dtiki_t *>;
	( *tikianimcache )[soldier.name] = &soldier;
	( *tikicache )[soldierModel.name] = &soldierModel;

	Poison();
	assert( TIKI_FindTikiAnim( "models/human/allied_soldier.tik" ) == &soldier );
	assert( lastKey == "models/human/allied_soldier.tik" );
	/* Any spelling the canonicaliser folds finds the same definition. */
	Poison();
	assert( TIKI_FindTikiAnim( "Models\\Human\\\\Allied_Soldier.TIK" ) == &soldier );
	assert( lastKey == "models/human/allied_soldier.tik" );
	Poison();
	assert( TIKI_FindTikiAnim( "models/human/axis_soldier.tik" ) == NULL );
	assert( lastKey == "models/human/axis_soldier.tik" );
	/* The model cache's lookup, which upstream already filled, agrees. */
	Poison();
	assert( TIKI_FindTiki( "Models/Human/Allied_Soldier.tik" ) == &soldierModel );

	/* A name longer than the buffer is cut, not overrun. */
	std::string longName( 3000, 'a' );
	Poison();
	assert( TIKI_FindTikiAnim( longName.c_str() ) == NULL );
	assert( lastKey.size() == 1023 );

	/* The loading bar's question: every model kind is answered by the
	 * definition cache, an animation by the skeletal cache, and the kinds
	 * the runtime does not cache are never reported cached. */
	const char kinds[] = { 'a', 'b', 'c', 'd', 'e', 'h' };
	for ( char kind : kinds ) {
		const std::string resource = std::string( 1, kind ) + "models/human/allied_soldier.tik";
		Poison();
		assert( MOHAA_ResourceCached( resource.c_str() ) == qtrue );
		const std::string missing = std::string( 1, kind ) + "models/human/axis_soldier.tik";
		Poison();
		assert( MOHAA_ResourceCached( missing.c_str() ) == qfalse );
	}
	assert( MOHAA_ResourceCached( "gmodels/human/walk.skc" ) == qfalse );
	skeletonCached = true;
	assert( MOHAA_ResourceCached( "gmodels/human/walk.skc" ) == qtrue );
	assert( MOHAA_ResourceCached( "kmodels/human/allied_soldier.tik" ) == qfalse );
	assert( MOHAA_ResourceCached( "nmodels/human/allied_soldier.tik" ) == qfalse );
	assert( MOHAA_ResourceCached( "" ) == qfalse );
	assert( MOHAA_ResourceCached( NULL ) == qfalse );
	return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'tiki_cache.cpp'
            exe = Path(directory) / 'tiki_cache.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp),
                                     '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
