"""Ritual's infostrings carry only the keys retail alice.exe and fakk2.exe 1.02
register for them, and their clients take the cheat state from `cheats`.

The fixture compiles the server's own SV_RitualInfoKeys against a small cvar
table holding what fnTech3 registers before it runs, and builds both
infostrings from the flags it leaves; no engine, game or network is used.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include <cassert>
#include <cstring>
#include <map>
#include <set>
#include <string>
enum { CVAR_ARCHIVE = 1, CVAR_USERINFO = 2, CVAR_SERVERINFO = 4, CVAR_SYSTEMINFO = 8,
       CVAR_LATCH = 0x20, CVAR_ROM = 0x40, CVAR_CHEAT = 0x200 };
struct cvar_t { std::string string; int flags; };
std::map<std::string, cvar_t> cvars;
int cvar_modifiedFlags;
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	auto found = cvars.find( name );
	if ( found == cvars.end() ) found = cvars.emplace( name, cvar_t{ value, 0 } ).first;
	found->second.flags |= flags;
	return &found->second;
}
std::set<std::string> Keys( int flag ) {
	std::set<std::string> keys;
	for ( const auto &[name, var] : cvars ) {
		if ( ( var.flags & flag ) && !var.string.empty() ) keys.insert( name );
	}
	return keys;
}
'''

CHECKS = r'''
int main() {
	// What the engine and a Ritual game module register before the first spawn.
	Cvar_Get( "sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_serverid", "1408", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_pure", "0", CVAR_SYSTEMINFO );
	Cvar_Get( "sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_pakNames", "base/pak0", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_referencedPaks", "1869240561 ", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_referencedPakNames", "base/pak0", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "timescale", "1", CVAR_SYSTEMINFO | CVAR_CHEAT );
	Cvar_Get( "sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_Get( "sv_maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH );
	Cvar_Get( "sv_maxRate", "0", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_Get( "sv_floodProtect", "1", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_Get( "sv_allowDownload", "1", CVAR_SERVERINFO );
	Cvar_Get( "sv_dlURL", "http://example.invalid", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_Get( "sv_dlRate", "100", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_Get( "sv_minRate", "0", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_Get( "sv_playingDemo", "0", CVAR_SERVERINFO | CVAR_ROM );
	Cvar_Get( "mapname", "garden1", CVAR_SERVERINFO | CVAR_ROM );
	cvar_modifiedFlags = 0;

	SV_RitualInfoKeys();

	// retail's systeminfo, sv_paks being empty until a pure server fills it
	assert( Keys( CVAR_SYSTEMINFO ) == ( std::set<std::string>{ "cheats", "sv_pure", "sv_serverid", "timescale" } ) );
	assert( Keys( CVAR_SERVERINFO ) == ( std::set<std::string>{ "mapname", "sv_floodProtect", "sv_hostname",
		"sv_maxRate", "sv_maxclients" } ) );
	// the keys stay as cvars, with their other flags and values
	assert( cvars["sv_cheats"].flags == CVAR_ROM && cvars["sv_cheats"].string == "1" );
	assert( cvars["sv_referencedPaks"].flags == CVAR_ROM );
	assert( cvars["sv_dlRate"].flags == CVAR_ARCHIVE && cvars["sv_playingDemo"].flags == CVAR_ROM );
	assert( cvars["timescale"].flags == ( CVAR_SYSTEMINFO | CVAR_CHEAT ) );
	// both strings are built again
	assert( ( cvar_modifiedFlags & ( CVAR_SERVERINFO | CVAR_SYSTEMINFO ) ) == ( CVAR_SERVERINFO | CVAR_SYSTEMINFO ) );
	// a second pass changes nothing
	SV_RitualInfoKeys();
	assert( Keys( CVAR_SYSTEMINFO ).size() == 4 && Keys( CVAR_SERVERINFO ).size() == 5 );
	return 0;
}
'''


class RitualInfoKeysTests(unittest.TestCase):
    def setUp(self):
        self.init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
        self.parse = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')

    def test_the_server_keeps_its_own_keys_off_ritual_infostrings(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        flags = compiler_target_args(compiler)
        body = FIXTURE + function(self.init, 'static void SV_RitualInfoKeys( void ) {') + CHECKS
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ritual-info-', dir=ROOT / '.tmp') as tmp:
            source = Path(tmp) / 'probe.cpp'
            exe = Path(tmp) / 'probe.exe'
            source.write_text(body)
            built = subprocess.run([compiler, *flags, '-std=c++17', str(source), '-o', str(exe)],
                                   capture_output=True, text=True, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], capture_output=True, text=True, cwd=ROOT, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_sv_init_applies_it_for_the_ritual_profiles_after_registering_every_key(self):
        init = function(self.init, 'void SV_Init( void )')
        call = 'if ( Com_GameIsRitual() ) {\n\t\tSV_RitualInfoKeys();\n\t}'
        self.assertIn(call, init)
        at = init.index(call)
        for registration in ('Cvar_Get( "sv_playingDemo", "0", CVAR_SERVERINFO | CVAR_ROM )',
                             'sv_minRate = Cvar_Get(', 'sv_dlRate = Cvar_Get(',
                             'sv_cheats = Cvar_Get( "sv_cheats"', 'Cvar_Get( "sv_referencedPaks"',
                             'sv_allowDownload = Cvar_Get (', 'Cvar_Get ("sv_dlURL"'):
            self.assertLess(init.index(registration), at, registration)

    def test_a_ritual_client_takes_the_cheat_state_from_cheats(self):
        changed = function(self.parse, 'void CL_SystemInfoChanged( qboolean onlyGame ) {')
        ritual = ('if ( Com_GameIsRitual() ) {', 's = Info_ValueForKey( systemInfo, "cheats" );',
                  'Cvar_Set( "sv_cheats", std::atoi( s ) ? "1" : "0" );', '} else {',
                  's = Info_ValueForKey( systemInfo, "sv_cheats" );',
                  'cl_connectedToCheatServer = std::atoi( s );', 'Cvar_SetCheatState();')
        at = 0
        for line in ritual:
            at = changed.index(line, at)
        # still after a demo returns, as retail's test of the demo flag does
        self.assertLess(changed.index('if ( clc.demoplaying ) {'), changed.index(ritual[0]))


if __name__ == '__main__':
    unittest.main()
