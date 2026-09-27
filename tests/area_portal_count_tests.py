"""A close that takes an area portal's count below zero, profile by profile.

Quake III drops the level when a close takes a portal's reference count below
zero (CM_AdjustAreaPortalState). Ritual's engines only tell a developer and go
on: alice.exe (0x004361a0) and F.A.K.K.2 1.02's fakk2.exe (0x0042abb0) print
"CM_AdjustAreaPortalState: negative reference count" through a Com_DPrintf
that reads developer (0x00438a40, 0x0042d2a0), leave the count below zero and
flood the areas as after any close; their floods join two areas only over a
count above zero (0x00436126, 0x0042ab36). Medal of Honor's engine is
OpenMoHAA's reading of 2015's, which does the same. Both Ritual engines'
CM_ReadPortalState (0x004362e0, 0x0042acf0) put a saved block back whole, so
a count below zero survives a save.

The first fixture compiles the engine's own area-portal code, sliced out of
code/qcommon/cm_test.c, with the real profile catalog and profile tests from
code/qcommon/game_profile.cpp, and runs each catalog profile through it. The
second compiles the Alice and Medal of Honor savegame restores. No map or
retail file is read. See docs/fntech3/2026-09-27-alice-area-portal-counts.md.
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


def catalog():
    source = (ROOT / 'code/qcommon/game_profile.cpp').read_text(encoding='utf-8')
    return source[source.index('const comGameProfile_t profiles[]'):source.index('\nstatic_assert(')]


PORTALS_PRELUDE = r'''
#include "code/qcommon/cm_local.h"
#include <setjmp.h>
#include <string.h>
clipMap_t cm;
cvar_t *cm_noAreas;
static cvar_t noAreas;
static cArea_t areas[8];
static int portals[64];
static jmp_buf dropped;
static int reports;
static char dropText[128], reportText[128];
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
	(void)level;
	strncpy( dropText, fmt, sizeof( dropText ) - 1 );
	longjmp( dropped, 1 );
}
void QDECL Com_DPrintf( const char *fmt, ... ) {
	reports++;
	strncpy( reportText, fmt, sizeof( reportText ) - 1 );
}
'''

PORTALS_DRIVER = r'''
/* A fresh map of `count` areas, every portal shut, flooded as the loader
 * leaves it. */
void FixtureMap( int count ) {
	memset( &cm, 0, sizeof( cm ) );
	memset( areas, 0, sizeof( areas ) );
	memset( portals, 0, sizeof( portals ) );
	cm.numAreas = count;
	cm.areas = areas;
	cm.areaPortals = portals;
	cm_noAreas = &noAreas;
	reports = 0;
	dropText[0] = reportText[0] = 0;
	CM_FloodAreaConnections();
}
/* 1 when the engine dropped. */
int FixtureAdjust( int area1, int area2, int open ) {
	if ( setjmp( dropped ) ) {
		return 1;
	}
	CM_AdjustAreaPortalState( area1, area2, open ? qtrue : qfalse );
	return 0;
}
int FixtureCount( int area1, int area2 ) { return cm.areaPortals[area1 * cm.numAreas + area2]; }
int FixtureConnected( int area1, int area2 ) { return CM_AreasConnected( area1, area2 ) ? 1 : 0; }
int FixtureFloods( void ) { return cm.floodvalid; }
int FixtureReports( void ) { return reports; }
const char *FixtureReported( void ) { return reportText; }
const char *FixtureDropped( void ) { return dropText; }
'''

CATALOG_PRELUDE = r'''
#include "code/qcommon/game_profile.h"
#include <cstdio>
#include <cstring>
extern "C" {
void FixtureMap( int count );
int FixtureAdjust( int area1, int area2, int open );
int FixtureCount( int area1, int area2 );
int FixtureConnected( int area1, int area2 );
int FixtureFloods( void );
int FixtureReports( void );
const char *FixtureReported( void );
const char *FixtureDropped( void );
}
const comGameProfile_t *activeProfile;
'''

CATALOG_CHECKS = r'''
static int failures;
static void Check( bool yes, const char *what ) {
	if ( !yes ) {
		std::fprintf( stderr, "%s: %s\n", activeProfile->key, what );
		++failures;
	}
}

int main() {
	static const char negative[] = "CM_AdjustAreaPortalState: negative reference count";
	int runs = 0, reporting = 0;
	for ( const auto &profile : profiles ) {
		activeProfile = &profile;
		/* Ritual's two engines, and Medal of Honor's as OpenMoHAA reads it,
		 * report a close past zero; every other profile keeps the drop. */
		const bool report = profile.id == COM_GAME_ALICE || profile.id == COM_GAME_FAKK2
			|| profile.id == COM_GAME_MOHAA;

		/* Three areas, portals between 0 and 1 and between 1 and 2. An open
		 * joins two areas, and the close that answers it parts them. */
		FixtureMap( 3 );
		Check( !FixtureAdjust( 0, 1, 1 ) && FixtureCount( 0, 1 ) == 1 && FixtureCount( 1, 0 ) == 1,
			"an open counts once each way" );
		Check( FixtureConnected( 0, 1 ) && !FixtureConnected( 1, 2 ), "an open joins its two areas" );
		Check( !FixtureAdjust( 0, 1, 0 ) && FixtureCount( 0, 1 ) == 0 && !FixtureConnected( 0, 1 ),
			"the answering close parts them" );
		Check( FixtureReports() == 0 && !FixtureDropped()[0], "balanced opens and closes are quiet" );

		/* A close of a portal nothing opened. */
		const int floods = FixtureFloods();
		const int drop = FixtureAdjust( 1, 2, 0 );
		if ( report ) {
			++reporting;
			Check( !drop, "the level goes on" );
			Check( FixtureReports() == 1 && !std::strcmp( FixtureReported(), "CM_AdjustAreaPortalState: "
				"negative reference count\n" ), "a developer is told, in retail's words" );
			Check( FixtureCount( 1, 2 ) == -1 && FixtureCount( 2, 1 ) == -1, "the count stays below zero" );
			Check( FixtureFloods() == floods + 1, "the areas are flooded as after any close" );
			Check( !FixtureConnected( 1, 2 ), "the portal is shut" );
			/* An open makes the count up to zero, and the portal stays shut
			 * until a second one. */
			Check( !FixtureAdjust( 1, 2, 1 ) && FixtureCount( 1, 2 ) == 0 && !FixtureConnected( 1, 2 ),
				"one open only makes the count up" );
			Check( !FixtureAdjust( 1, 2, 1 ) && FixtureCount( 1, 2 ) == 1 && FixtureConnected( 2, 1 ),
				"the second open joins the areas" );
			Check( !FixtureAdjust( 1, 2, 0 ) && !FixtureAdjust( 1, 2, 0 ) && !FixtureAdjust( 1, 2, 0 ),
				"further closes go on" );
			Check( FixtureCount( 1, 2 ) == -2 && FixtureReports() == 3 && !FixtureConnected( 1, 2 ),
				"each close below zero is told" );
		} else {
			Check( drop && !std::strcmp( FixtureDropped(), negative ), "Quake III's drop" );
			Check( FixtureReports() == 0, "no report beside the drop" );
			Check( FixtureFloods() == floods, "no flood after the drop" );
		}

		/* An area the map does not have is a drop for every profile, and a
		 * negative one is ignored. */
		FixtureMap( 3 );
		Check( FixtureAdjust( 0, 3, 0 ) && !std::strcmp( FixtureDropped(), "CM_ChangeAreaPortalState: bad area number" ),
			"an area past the map drops" );
		FixtureMap( 3 );
		const int before = FixtureFloods();
		Check( !FixtureAdjust( -1, 1, 0 ) && !FixtureAdjust( 1, -1, 1 ), "a negative area is no drop" );
		Check( FixtureFloods() == before && FixtureReports() == 0 && FixtureCount( 0, 1 ) == 0,
			"and changes nothing" );
		++runs;
	}
	if ( runs != COM_GAME_COUNT - 1 || reporting != 3 ) {
		std::fprintf( stderr, "%d profile runs, %d reporting\n", runs, reporting );
		return 1;
	}
	if ( failures ) {
		return 1;
	}
	std::printf( "Area portal counts checked for %d profiles\n", runs );
	return 0;
}
'''

RESTORE_PRELUDE = r'''
#include "code/compat/alice_savegame.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
namespace alicesave = fntech3::alice::savegame;
enum errorParm_t { ERR_FATAL, ERR_DROP };
struct Drop { std::string message; };
static std::vector<int> loaded;
static int loadedAreas, floods;
static std::string printed;
void Com_Error( errorParm_t, const char *message, ... ) { throw Drop{ message }; }
void Com_Printf( const char *message, ... ) { printed = message; }
int *CM_AreaPortalCounts( int *count ) {
	*count = static_cast<int>( loaded.size() );
	return loaded.empty() ? nullptr : loaded.data();
}
int CM_NumAreas() { return loadedAreas; }
void CM_FloodAreaConnections() { ++floods; }
'''

RESTORE_CHECKS = r'''
static int failures;
static void Check( bool yes, const char *what ) {
	if ( !yes ) {
		std::fprintf( stderr, "%s\n", what );
		++failures;
	}
}

/* A three-area map as it spawned, every portal shut. */
static void Spawned() {
	loaded.assign( 9, 0 );
	loadedAreas = 3;
	floods = 0;
	printed.clear();
}

static std::string AliceDrop( const AliceServerFile &file ) {
	try {
		SV_AliceRestorePortalState( file );
	} catch ( const Drop &drop ) {
		return drop.message;
	}
	return {};
}

int main() {
	/* The counts wforest's last pass leaves: one portal closed twice from
	 * zero, both ways. */
	const std::vector<std::int32_t> saved = { 0, 0, 0, 0, 0, -2, 0, -2, 1 };

	/* Alice and F.A.K.K.2 put the block back as it was written. */
	AliceServerFile file;
	file.numAreas = 3;
	file.counts = saved;
	Spawned();
	Check( AliceDrop( file ).empty(), "Alice: a count below zero is no drop" );
	Check( std::vector<std::int32_t>( loaded.begin(), loaded.end() ) == saved, "Alice: the counts come back whole" );
	Check( floods == 1, "Alice: the areas are flooded again" );

	/* A block for another map, or one cut short, is still refused. */
	file.numAreas = 4;
	Spawned();
	Check( AliceDrop( file ) == "numareaportals differs from save game", "Alice: another map's block" );
	file.numAreas = 3;
	file.counts.pop_back();
	Check( AliceDrop( file ) == "The save game's area portals are cut short", "Alice: a block cut short" );
	Check( loaded == std::vector<int>( 9, 0 ) && floods == 0, "Alice: a refused block changes nothing" );

	/* Medal of Honor puts its own block back as it was written too. */
	Spawned();
	RestorePortals( 3, saved );
	Check( std::vector<std::int32_t>( loaded.begin(), loaded.end() ) == saved && floods == 1 && printed.empty(),
		"Medal of Honor: the counts come back whole" );
	Spawned();
	RestorePortals( 4, saved );
	Check( loaded == std::vector<int>( 9, 0 ) && floods == 0 && !printed.empty(),
		"Medal of Honor: another map's block leaves the counts as spawned" );

	if ( failures ) {
		return 1;
	}
	std::puts( "Portal restores checked" );
	return 0;
}
'''


def portals_source():
    test = (ROOT / 'code/qcommon/cm_test.c').read_text(encoding='utf-8')
    source = PORTALS_PRELUDE
    for signature in ['static void CM_FloodArea_r(', 'void\tCM_FloodAreaConnections( void ) {',
                      'void\tCM_AdjustAreaPortalState(', 'qboolean\tCM_AreasConnected(']:
        source += '\n' + function(test, signature)
    return source + PORTALS_DRIVER


def catalog_source():
    profiles = (ROOT / 'code/qcommon/game_profile.cpp').read_text(encoding='utf-8')
    source = CATALOG_PRELUDE + catalog()
    for signature in ['int Com_GameIsAlice( void )', 'int Com_GameIsFAKK2( void )',
                      'int Com_GameIsRitual( void )', 'int Com_GameIsMOHAA( void )']:
        source += '\n' + function(profiles, signature)
    return source + CATALOG_CHECKS


def restore_source():
    alice = (ROOT / 'code/server/server_alice_save.hpp').read_text(encoding='utf-8')
    mohaa = (ROOT / 'code/server/server_mohaa_save.hpp').read_text(encoding='utf-8')
    start = alice.index('struct AliceServerFile {')
    source = RESTORE_PRELUDE + alice[start:alice.index('\n};', start) + 3]
    source += '\n' + function(alice, 'void SV_AliceRestorePortalState(')
    source += '\n' + function(mohaa, 'void RestorePortals(')
    return source + RESTORE_CHECKS


class AreaPortalCountTests(unittest.TestCase):
    def compilers(self):
        cxx = shutil.which('clang++')
        cc = shutil.which('clang')
        if not cxx or not cc:
            self.skipTest('Clang C and C++ compilers unavailable')
        target = compiler_target_args(cxx)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        return cc, cxx, target

    def build_and_run(self, name, cpp_source, c_source=None):
        cc, cxx, target = self.compilers()
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=name, dir=scratch) as folder:
            folder = Path(folder)
            objects = []
            if c_source is not None:
                path = folder / f'{name}.c'
                path.write_text(c_source, encoding='utf-8')
                obj = folder / f'{name}-c.o'
                built = subprocess.run([cc, *target, '-std=c11', '-I', str(ROOT), '-I', str(ROOT / 'code/qcommon'),
                                        '-c', str(path), '-o', str(obj)],
                                       cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                objects.append(str(obj))
            cpp = folder / f'{name}.cpp'
            cpp.write_text(cpp_source, encoding='utf-8')
            exe = folder / f'{name}.exe'
            built = subprocess.run([cxx, *target, '-std=c++17', '-I', str(ROOT), '-I', str(ROOT / 'code/qcommon'),
                                    str(cpp), *objects, '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            return run.stdout

    def test_each_profile_drops_or_reports_a_close_past_zero_as_its_engine_does(self):
        output = self.build_and_run('portals', catalog_source(), portals_source())
        self.assertIn('Area portal counts checked for', output)

    def test_savegame_restores_keep_counts_below_zero(self):
        output = self.build_and_run('portal-restore', restore_source())
        self.assertIn('Portal restores checked', output)

    def test_the_report_is_scoped_to_the_ritual_lineage(self):
        test = (ROOT / 'code/qcommon/cm_test.c').read_text(encoding='utf-8')
        adjust = function(test, 'void\tCM_AdjustAreaPortalState(')
        # The profile test is asked only once a close has gone past zero, and
        # names Medal of Honor beside the Ritual profiles, never through them.
        self.assertEqual(adjust.count('Com_GameIsRitual() || Com_GameIsMOHAA()'), 1)
        self.assertLess(adjust.index('< 0 ) {'), adjust.index('Com_GameIsRitual()'))
        self.assertEqual(adjust.count('Com_Error (ERR_DROP, "CM_AdjustAreaPortalState: negative reference count");'), 1)
        self.assertLess(adjust.index('Com_DPrintf'), adjust.index('CM_FloodAreaConnections ();'))


if __name__ == '__main__':
    unittest.main()
