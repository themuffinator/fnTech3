"""The audio zone file a Ritual level loads after a level change.

A Ritual game module leaves a level with `gamemap "<map>$<spawn target>"`
(Heavy Metal: F.A.K.K.2's fakk/gamex86.dll 0x10083b6a), and retail alice.exe's
spawn keeps that whole argument as the server's `mapname` (0x00455223), so the
client's map name becomes `maps/training$spawn_fakkhouse.bsp` (0x004248f5). The
BSP is the part before the `$` (0x00450f96); so are its zones. The probe
compiles the production lookup from AudioSystemShared.inl against a controlled
host; no retail file is read.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[2]
SHARED = ROOT / 'code/client/audio/openal/AudioSystemShared.inl'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
constexpr int MAX_QPATH = 64;
static int ritualProfile = 0;
static int Com_GameIsRitual() { return ritualProfile; }
static struct { char mapname[MAX_QPATH]; } cl;
static void COM_StripExtension( const char *in, char *out, int destsize ) {
	std::string name( in );
	const std::size_t dot = name.find_last_of( '.' );
	const std::size_t slash = name.find_last_of( '/' );
	if ( dot != std::string::npos && ( slash == std::string::npos || dot > slash ) ) {
		name.erase( dot );
	}
	std::snprintf( out, static_cast<std::size_t>( destsize ), "%s", name.c_str() );
}
static int Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	const int length = std::vsnprintf( dest, static_cast<std::size_t>( size ), fmt, args );
	va_end( args );
	return length;
}
'''

MAIN = r'''
static int failures = 0;
static void Expect( int ritual, const char *mapname, const char *zones ) {
	ritualProfile = ritual;
	std::snprintf( cl.mapname, sizeof( cl.mapname ), "%s", mapname );
	const std::string found = AudioZoneQpathForCurrentMap();
	if ( found != zones ) {
		std::printf( "ritual=%d %s: got '%s', want '%s'\n", ritual, mapname, found.c_str(), zones );
		++failures;
	}
}
int main() {
	Expect( 1, "maps/training$spawn_fakkhouse.bsp", "maps/training.azb" );
	Expect( 1, "maps/fakkhouse.bsp", "maps/fakkhouse.azb" );
	Expect( 1, "maps/mapel4b$spawn.2.bsp", "maps/mapel4b.azb" );
	Expect( 1, "", "" );
	// Other profiles never name a spawn target, and keep the whole name.
	Expect( 0, "maps/q3dm$1.bsp", "maps/q3dm$1.azb" );
	Expect( 0, "maps/q3dm17.bsp", "maps/q3dm17.azb" );
	return failures ? 1 : 0;
}
'''


class RitualAudioZonePathTests(unittest.TestCase):
    def test_a_level_change_loads_the_bsps_own_zones(self):
        lookup = function(SHARED.read_text(encoding='utf-8'),
                          'static std::string AudioZoneQpathForCurrentMap() {')
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ritual-zone-path-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(PRELUDE + lookup + MAIN, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
