"""Medal of Honor: the engine's three developer file pickers.

1.00's animate2, LODTool and ZOUND menus run viewspawnlist, lod_spawnlist and
soundpicker, which neither retail module registers: 2015's engine does
(OpenMoHAA UI_ViewSpawnList_f, UI_LODSpawnList_f and UI_SoundPicker_f on
FilePickerClass, reference only). fnTech3 serves them with its UI's picker.
The listing and the folder a picker opens are production's, compiled from
code/client/cl_mohaa_ui.cpp against a stub filesystem."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'code/client/cl_mohaa_ui.cpp'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


HARNESS = r'''
#include <algorithm>
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>
struct PickerItem { std::string label, command; };
static std::vector<std::string> args;
static int Cmd_Argc() { return static_cast<int>( args.size() ); }
static const char *Cmd_Argv( int i ) { return i < Cmd_Argc() ? args[i].c_str() : ""; }
static int Q_stricmpn( const char *a, const char *b, int n ) {
	for ( int i = 0; i < n; i++ ) {
		const int ca = tolower( static_cast<unsigned char>( a[i] ) ), cb = tolower( static_cast<unsigned char>( b[i] ) );
		if ( ca != cb ) return ca < cb ? -1 : 1;
		if ( !ca ) return 0;
	}
	return 0;
}
static int Q_stricmp( const char *a, const char *b ) { return Q_stricmpn( a, b, 1 << 20 ); }
/* folder -> extension -> names; "/" lists subfolders */
static std::map<std::string, std::map<std::string, std::vector<std::string>>> tree;
static char **FS_ListFiles( const char *folder, const char *extension, int *count ) {
	const auto &names = tree[folder][extension];
	*count = static_cast<int>( names.size() );
	char **list = new char *[names.size() + 1];
	for ( std::size_t i = 0; i < names.size(); i++ ) list[i] = strdup( names[i].c_str() );
	list[names.size()] = nullptr;
	return list;
}
static void FS_FreeFileList( char **list ) {
	for ( char **p = list; *p; p++ ) free( *p );
	delete[] list;
}
'''

CHECKS = r'''
static bool Has( const std::vector<PickerItem> &items, const char *label, const char *command ) {
	return std::any_of( items.begin(), items.end(), [&]( const PickerItem &i ) {
		return i.label == label && i.command == command;
	} );
}
int main() {
	/* The filesystem lists a pak's files one folder down too, and a folder
	 * once from the paks (with its slash) and once from the disk. */
	tree["models"][".tik"] = { "crate.tik", "human/sergeant.tik", "ammo\\clip.tik" };
	tree["models"]["/"] = { "human/", "Human", "ammo/", ".svn" };
	tree["models/human"][".tik"] = { "sergeant.tik" };
	tree["sound/weapons"][".wav"] = { "fire.wav" };
	tree["sound/weapons"][".mp3"] = { "distant.mp3" };

	/* The root lists its own files and each folder once, and no way above
	 * itself. */
	auto root = FileItems( "models", "models", { ".tik" }, "viewspawn", "viewspawnlist" );
	assert( Has( root, "crate.tik", "viewspawn \"models/crate.tik\"" ) );
	assert( Has( root, "human/", "viewspawnlist \"models/human\"" ) );
	assert( Has( root, "ammo/", "viewspawnlist \"models/ammo\"" ) );
	assert( root.size() == 3 );
	/* A folder below it lists its parent. */
	auto human = FileItems( "models", "models/human", { ".tik" }, "lod_spawn", "lod_spawnlist" );
	assert( Has( human, "sergeant.tik", "lod_spawn \"models/human/sergeant.tik\"" ) );
	assert( Has( human, "../", "lod_spawnlist \"models\"" ) );
	/* Every extension asked for is listed. */
	auto sounds = FileItems( "sound", "sound/weapons", { ".wav", ".mp3" }, "set ui_pickedsound", "soundpicker" );
	assert( Has( sounds, "fire.wav", "set ui_pickedsound \"sound/weapons/fire.wav\"" ) );
	assert( Has( sounds, "distant.mp3", "set ui_pickedsound \"sound/weapons/distant.mp3\"" ) );
	assert( Has( sounds, "../", "soundpicker \"sound\"" ) );

	/* A picker opens where its argument says, under its root and nowhere else. */
	args = { "viewspawnlist" };
	assert( PickerFolder( "models", "models" ) == "models" );
	args = { "viewspawnlist", "models/human/" };
	assert( PickerFolder( "models", "models" ) == "models/human" );
	args = { "viewspawnlist", "Models/Human" };
	assert( PickerFolder( "models", "models" ) == "Models/Human" );
	for ( const char *outside : { "maps", "modelsx", "models/../maps", "../models", "" } ) {
		args = { "viewspawnlist", outside };
		assert( PickerFolder( "models", "models" ) == "models" );
	}
	args = { "soundpicker" };
	assert( PickerFolder( "sound", "sound/weapons" ) == "sound/weapons" );
	return 0;
}
'''


class MohaaDeveloperPickerTests(unittest.TestCase):
    def test_the_pickers_list_files_and_folders_and_stay_under_their_roots(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = SOURCE.read_text(encoding='utf-8')
        harness = (HARNESS
                   + function(source, 'std::vector<PickerItem> FileItems( const std::string &root, const std::string &folder,')
                   + function(source, 'std::string PickerFolder( const std::string &root, const std::string &fallback ) {')
                   + CHECKS)
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'pickers.cpp'
            exe = Path(directory) / 'pickers.exe'
            cpp.write_text(harness, encoding='utf-8')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-D_CRT_NONSTDC_NO_DEPRECATE',
                                    '-D_CRT_SECURE_NO_WARNINGS', str(cpp), '-o', str(exe)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_the_three_commands_run_the_retail_game_commands(self):
        source = SOURCE.read_text(encoding='utf-8')
        self.assertIn('{ "viewspawnlist", ViewSpawnList_f }, { "lod_spawnlist", LODSpawnList_f }, '
                      '{ "soundpicker", SoundPicker_f },', source)
        self.assertIn('"viewspawn",', function(source, 'void ViewSpawnList_f() {'))
        self.assertIn('"lod_spawn",', function(source, 'void LODSpawnList_f() {'))
        sound = function(source, 'void SoundPicker_f() {')
        self.assertIn('Cvar_VariableString( "ui_pickedsound" )', sound)
        self.assertIn('"set ui_pickedsound"', sound)
        self.assertIn('"; play "', sound)


if __name__ == '__main__':
    unittest.main()
