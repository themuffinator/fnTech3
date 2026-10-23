"""Elite Force's engine registrations follow its two retail executables.

A census of stvoy.exe 1.20 and stvoyHM.exe 1.20 against fnTech3 under both
Elite Force profiles (docs/fntech3/2026-10-23-stvef-retail-registrations.md)
found where this engine published keys, protected cvars or lacked commands
that retail does not. These tests compile the production functions that now
follow retail with stubbed engine services, and pin where the engine calls
them. No game is started and no retail file is read.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def compile_and_run(test, source, prefix):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler unavailable')
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as folder:
        cpp = Path(folder) / 'probe.cpp'
        exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall', '-Wextra',
                                str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, **options)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        return ran.stdout


CVARS = r'''
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <string>
constexpr int CVAR_ARCHIVE = 1, CVAR_USERINFO = 2, CVAR_SERVERINFO = 4, CVAR_SYSTEMINFO = 8,
    CVAR_INIT = 16, CVAR_LATCH = 32, CVAR_ROM = 64, CVAR_TEMP = 256, CVAR_NORESTART = 1024,
    CVAR_ARCHIVE_ND = 0x4000, CVAR_PROTECTED = 0x100000;
struct cvar_t { int flags; };
static std::map<std::string, cvar_t> cvars;
static cvar_t *Cvar_Get( const char *name, const char *, int flags ) {
    auto found = cvars.find( name );
    if ( found == cvars.end() ) found = cvars.emplace( name, cvar_t{ flags } ).first;
    else found->second.flags |= flags;
    return &found->second;
}
static std::set<std::string> with( int flag ) {
    std::set<std::string> names;
    for ( const auto &entry : cvars ) if ( entry.second.flags & flag ) names.insert( entry.first );
    return names;
}
'''


USERINFO = CVARS + r'''
static bool singlePlayer;
static int Com_GameIsSTVEFSP( void ) { return singlePlayer; }

PRODUCTION

// The userinfo cvars CL_Init and the appearance policy register for each mode.
static void Register( bool sp ) {
    cvars.clear();
    singlePlayer = sp;
    Cvar_Get( "name", "", CVAR_USERINFO | CVAR_ARCHIVE_ND );
    Cvar_Get( "rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get( "snaps", "40", CVAR_USERINFO | CVAR_ARCHIVE );
    if ( sp ) {
        for ( const char *key : { "legsModel", "torsoModel", "headModel" } ) Cvar_Get( key, "", CVAR_USERINFO );
    } else {
        Cvar_Get( "model", "munro/red", CVAR_USERINFO | CVAR_ARCHIVE );
        Cvar_Get( "color", "4", CVAR_USERINFO | CVAR_ARCHIVE );
    }
    Cvar_Get( "handicap", "100", CVAR_USERINFO | CVAR_ARCHIVE_ND );
    Cvar_Get( "sex", "male", CVAR_USERINFO | CVAR_ARCHIVE_ND );
    Cvar_Get( "cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE_ND );
    Cvar_Get( "password", "", CVAR_USERINFO | CVAR_NORESTART );
    Cvar_Get( "cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get( "cl_guid", "", CVAR_USERINFO | CVAR_ROM | CVAR_PROTECTED );
}

int main() {
    // stvoyHM.exe CL_Init 0x40fabc..0x40fb4a: nine keys, no cl_anonymous or cl_guid.
    Register( false );
    CL_STVEFInfoKeys();
    assert( ( with( CVAR_USERINFO ) == std::set<std::string>{ "cg_predictItems", "color", "handicap", "model",
        "name", "password", "rate", "sex", "snaps" } ) );
    assert( cvars.at( "cl_guid" ).flags == ( CVAR_ROM | CVAR_PROTECTED ) );
    assert( cvars.at( "cl_anonymous" ).flags == CVAR_ARCHIVE_ND );
    // stvoy.exe 0x408872..0x4088db: seven, as every savegame lists them.
    Register( true );
    CL_STVEFInfoKeys();
    assert( ( with( CVAR_USERINFO ) == std::set<std::string>{ "handicap", "headModel", "legsModel", "name", "sex",
        "snaps", "torsoModel" } ) );
    assert( cvars.at( "rate" ).flags == CVAR_ARCHIVE && cvars.at( "password" ).flags == CVAR_NORESTART );
    assert( cvars.at( "cg_predictItems" ).flags == CVAR_ARCHIVE );
    // Again: nothing changes, and no cvar is made.
    const auto before = cvars;
    CL_STVEFInfoKeys();
    assert( cvars.size() == before.size() );
    for ( const auto &entry : before ) assert( cvars.at( entry.first ).flags == entry.second.flags );
    std::puts( "userinfo keys passed" );
}
'''


SERVERINFO = CVARS + r'''
PRODUCTION

int main() {
    // What SV_Init and the filesystem register before the call.
    const std::map<std::string, int> registered = {
        { "dmflags", CVAR_SERVERINFO }, { "timelimit", CVAR_SERVERINFO },
        { "g_gametype", CVAR_SERVERINFO | CVAR_LATCH }, { "sv_keywords", CVAR_SERVERINFO },
        { "protocol", CVAR_SERVERINFO | CVAR_ROM }, { "mapname", CVAR_SERVERINFO | CVAR_ROM },
        { "version", CVAR_SERVERINFO | CVAR_ROM }, { "sv_playingDemo", CVAR_SERVERINFO | CVAR_ROM },
        { "sv_privateClients", CVAR_SERVERINFO }, { "sv_hostname", CVAR_SERVERINFO | CVAR_ARCHIVE },
        { "sv_maxclients", CVAR_SERVERINFO | CVAR_LATCH }, { "sv_minRate", CVAR_ARCHIVE_ND | CVAR_SERVERINFO },
        { "sv_maxRate", CVAR_ARCHIVE_ND | CVAR_SERVERINFO }, { "sv_dlRate", CVAR_ARCHIVE | CVAR_SERVERINFO },
        { "sv_floodProtect", CVAR_ARCHIVE | CVAR_SERVERINFO }, { "sv_allowDownload", CVAR_SERVERINFO },
        { "sv_dlURL", CVAR_SERVERINFO | CVAR_ARCHIVE }, { "fs_game", CVAR_INIT | CVAR_SYSTEMINFO },
        { "sv_cheats", CVAR_SYSTEMINFO }, { "sv_serverid", CVAR_SYSTEMINFO | CVAR_ROM } };
    for ( const auto &entry : registered ) Cvar_Get( entry.first.c_str(), "", entry.second );
    SV_STVEFSPInfoKeys();
    // stvoy.exe 1.20: protocol and mapname from SV_Init (0x425c70..0x425dad),
    // the version from Com_Init, fs_game from the filesystem (0x415fb7).
    assert( ( with( CVAR_SERVERINFO ) == std::set<std::string>{ "fs_game", "mapname", "protocol", "version" } ) );
    assert( cvars.at( "fs_game" ).flags == ( CVAR_INIT | CVAR_SYSTEMINFO | CVAR_SERVERINFO ) );
    for ( const auto &entry : registered ) {
        if ( entry.first == "fs_game" ) continue;
        assert( ( cvars.at( entry.first ).flags | CVAR_SERVERINFO ) == ( entry.second | CVAR_SERVERINFO ) );
    }
    assert( ( with( CVAR_SYSTEMINFO ) == std::set<std::string>{ "fs_game", "sv_cheats", "sv_serverid" } ) );
    SV_STVEFSPInfoKeys();
    assert( cvars.size() == registered.size() );
    std::puts( "serverinfo keys passed" );
}
'''


RSERVERINFO = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
enum { CA_CONNECTED = 5, CA_ACTIVE = 8 };
constexpr int CS_SERVERINFO = 0, MAX_CONFIGSTRINGS = 1024;
struct { int state; char servername[256]; } cls;
struct { struct { int stringOffsets[MAX_CONFIGSTRINGS]; char stringData[256]; } gameState; } cl;
static std::string printed, infoPrinted;
static void Com_Printf( const char *format, ... ) {
    char text[512];
    va_list arguments;
    va_start( arguments, format );
    std::vsnprintf( text, sizeof( text ), format, arguments );
    va_end( arguments );
    printed += text;
}
static void Info_Print( const char *info ) { infoPrinted = info; }

PRODUCTION

int main() {
    std::strcpy( cls.servername, "localhost" );
    std::strcpy( cl.gameState.stringData + 1, "\\mapname\\hm_borg1\\protocol\\24" );
    cl.gameState.stringOffsets[CS_SERVERINFO] = 1;
    // stvoyHM.exe 0x40daa0: anything short of active is refused.
    cls.state = CA_CONNECTED;
    CL_RemoteServerinfo_f();
    assert( printed == "Not connected to a server.\n" && infoPrinted.empty() );
    printed.clear();
    cls.state = CA_ACTIVE;
    CL_RemoteServerinfo_f();
    assert( printed == "--------- Remote Server Information ---------\nServer: localhost settings:\n"
        "--------------------------------------\n" );
    assert( infoPrinted == "\\mapname\\hm_borg1\\protocol\\24" );
    std::puts( "rserverinfo passed" );
}
'''


class STVEFRetailRegistrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client = read('code/client/cl_main.cpp')
        cls.server_init = read('code/server/sv_init.cpp')
        cls.commands = read('code/server/sv_ccmds.cpp')

    def test_userinfo_holds_retail_keys_alone(self):
        production = function(self.client, 'static void CL_STVEFInfoKeys( void ) {')
        compile_and_run(self, USERINFO.replace('PRODUCTION', production), 'stvef-userinfo-')

    def test_client_init_filters_after_every_registration(self):
        init = function(self.client, 'void CL_Init( void ) {')
        call = init.index('\tif ( Com_GameIsSTVEF() ) {\n\t\tCL_STVEFInfoKeys();\n\t}\n')
        for registration in ('Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );',
                             'Cvar_Get ("cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE_ND );',
                             'Cvar_Get ("password", "", CVAR_USERINFO | CVAR_NORESTART);',
                             'Cvar_Get ("cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE );',
                             'Cvar_Get( "cl_guid", "", CVAR_USERINFO | CVAR_ROM | CVAR_PROTECTED );',
                             'CL_UpdateGUID( nullptr, 0 );'):
            self.assertLess(init.index(registration), call, registration)
        self.assertEqual(self.client.count('CL_STVEFInfoKeys();'), 1)

    def test_single_player_serverinfo_holds_retail_keys_alone(self):
        production = function(self.server_init, 'static void SV_STVEFSPInfoKeys( void ) {')
        compile_and_run(self, SERVERINFO.replace('PRODUCTION', production), 'stvef-serverinfo-sp-')
        init = function(self.server_init, 'void SV_Init( void )\n{')
        call = init.index('\tif ( Com_GameIsSTVEFSP() ) {\n\t\tSV_STVEFSPInfoKeys();\n\t}\n')
        for registration in ('Cvar_Get ("dmflags", "0", CVAR_SERVERINFO);',
                             'Cvar_Get ("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE);',
                             'sv_maxclients = Cvar_Get ("sv_maxclients"', 'sv_floodProtect = Cvar_Get( "sv_floodProtect"'):
            self.assertLess(init.index(registration), call, registration)
        # Single player's map commands are its own; they register no serverinfo key.
        self.assertIn('SV_STVEFSPAddCommands();', self.commands)

    def test_single_player_console_may_turn_cheats_on(self):
        init = function(self.server_init, 'void SV_Init( void )\n{')
        registration = init.index('sv_cheats = Cvar_Get( "sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM );')
        cleared = init.index('\tif ( Com_GameIsSTVEFSP() ) {', registration)
        self.assertLess(registration, cleared)
        self.assertIn('\t\tsv_cheats->flags &= ~CVAR_ROM;\n', init[cleared:cleared + 400])
        # No later server registration makes it read-only again.
        for name in ('code/server/sv_main.cpp', 'code/server/sv_ccmds.cpp', 'code/server/sv_client.cpp',
                     'code/server/sv_game.cpp'):
            for line in read(name).splitlines():
                if '"sv_cheats"' in line and 'Cvar_Get' in line:
                    self.assertNotIn('CVAR_ROM', line, f'{name}: {line.strip()}')

    def test_holomatch_map_command_keeps_game_type_out_of_userinfo(self):
        map_command = function(self.commands, 'static void SV_Map_f( void ) {')
        holomatch = map_command.index('\tif ( Com_GameIsSTVEFMP() ) {\n\t\tCvar_Get( "g_gametype", '
                                      'serverPolicy.gameType, CVAR_SERVERINFO | CVAR_LATCH );\n\t} else {\n')
        quake = map_command.index('Cvar_Get ("g_gametype", serverPolicy.gameType, '
                                  'CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH );')
        self.assertLess(holomatch, quake)

    def test_single_player_registers_the_spawn_target(self):
        init = function(self.server_init, 'void SV_Init( void )\n{')
        nextmap = init.index('Cvar_Get ("nextmap", "", CVAR_TEMP );')
        block = init.index('\tif ( Com_GameIsSTVEFSP() ) {', nextmap)
        self.assertIn('Cvar_Get( "spawntarget", "", 0 );', init[block:block + 300])

    def test_holomatch_registers_the_team_time_limit(self):
        init = function(self.server_init, 'void SV_Init( void )\n{')
        timelimit = init.index('Cvar_Get ("timelimit", "0", CVAR_SERVERINFO);')
        team = init.index('\tif ( Com_GameIsSTVEFMP() ) {', timelimit)
        self.assertIn('Cvar_Get( "timelimitWinningTeam", "", CVAR_TEMP );', init[team:team + 400])
        self.assertLess(team, init.index('sv_gametype = Cvar_Get ("g_gametype"'))

    def test_rserverinfo(self):
        production = function(self.client, 'static void CL_RemoteServerinfo_f( void ) {')
        compile_and_run(self, RSERVERINFO.replace('PRODUCTION', production), 'stvef-rserverinfo-')
        self.assertIn('if ( Com_GameIsSTVEFMP() ) Cmd_AddCommand( "rserverinfo", CL_RemoteServerinfo_f );',
                      self.client)
        self.assertIn('if ( Com_GameIsSTVEFMP() ) Cmd_RemoveCommand( "rserverinfo" );', self.client)

    def test_menu_cvars_are_registered_with_retail_defaults(self):
        init = function(self.client, 'void CL_Init( void ) {')
        block = init[init.index('Cvar_Get( "k_language", "american", CVAR_ARCHIVE | CVAR_NORESTART );'):]
        block = block[:block.index('CL_STVEFSPAttachCGame( qtrue );')]
        for registration in ('Cvar_Get( "s_khz", "22", CVAR_ARCHIVE );',
                             'Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH );',
                             'Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );',
                             'Cvar_Get( "joy_xbutton", "1", CVAR_ARCHIVE );',
                             'Cvar_Get( "joy_ybutton", "0", CVAR_ARCHIVE );',
                             'Cvar_Get( "r_lowEndVideo", "0", CVAR_ARCHIVE | CVAR_LATCH )'):
            self.assertIn(registration, block)
        voice = block.index('if ( Com_GameIsSTVEFSP() ) {')
        self.assertIn('Cvar_Get( "s_volumeVoice", "1.0", CVAR_ARCHIVE )', block[voice:])
        # They sit in the Elite Force block alone.
        self.assertLess(init.index('if ( Com_GameIsSTVEF() ) {'), init.index('Cvar_Get( "s_khz", "22", CVAR_ARCHIVE );'))


if __name__ == '__main__':
    unittest.main()
