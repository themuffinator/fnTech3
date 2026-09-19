"""Elite Force's languages: German voices, translated key names and the
language cvars, and the key numbers its UI is given.

Retail S_LoadSound (stvoyHM.exe 0x42c690, stvoy.exe 0x4232f3) plays the
German recording under sound/vox_d when s_language is DEUTSCH; the probe
compiles fnTech3's S_CodecLoad against recording stubs and checks the order it
tries names in. The key probes compile the adapter and name headers; the
retail tables themselves are compared by scripts/probe_stvef_key_names.py. The
source checks pin where the cvars are registered and that the Holomatch UI's
key imports and key events go through Elite Force's own key numbers.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

VOICE_PROBE = r'''
#include <array>
#include <cassert>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#define MAX_QPATH 64
struct snd_info_t { int rate; };
static bool eliteForce = true;
static std::string language = "DEUTSCH";
static bool Com_GameIsSTVEF() { return eliteForce; }
static const char *Cvar_VariableString( const char * ) { return language.c_str(); }
static int Q_stricmp( const char *a, const char *b ) {
	for ( ;; ++a, ++b ) {
		const int x = std::toupper( static_cast<unsigned char>( *a ) ), y = std::toupper( static_cast<unsigned char>( *b ) );
		if ( x != y ) return x < y ? -1 : 1;
		if ( !x ) return 0;
	}
}
static void Q_strncpyz( char *dest, const char *src, int size ) {
	int i = 0;
	for ( ; i + 1 < size && src[i]; ++i ) dest[i] = src[i];
	dest[i] = '\0';
}
static const char *Q_stristr( const char *s, const char *find ) {
	for ( ; *s; ++s ) {
		int i = 0;
		while ( find[i] && std::tolower( static_cast<unsigned char>( s[i] ) ) == std::tolower( static_cast<unsigned char>( find[i] ) ) ) ++i;
		if ( !find[i] ) return s;
	}
	return nullptr;
}
struct Attempt { std::string name; bool quiet; };
static std::vector<Attempt> attempts;
static std::vector<std::string> present;
static int found;
static void *S_CodecGetSound( const char *filename, snd_info_t *, bool quiet = false ) {
	attempts.push_back( { filename, quiet } );
	for ( const std::string &name : present ) if ( name == filename ) return &found;
	return nullptr;
}
GERMAN
LOAD
static void Check( const char *name, std::vector<Attempt> expected, bool loads ) {
	attempts.clear();
	snd_info_t info{};
	void *data = S_CodecLoad( name, &info );
	assert( ( data != nullptr ) == loads );
	assert( attempts.size() == expected.size() );
	for ( std::size_t i = 0; i < expected.size(); ++i ) {
		assert( attempts[i].name == expected[i].name && attempts[i].quiet == expected[i].quiet );
	}
}
int main() {
	const char *voice = "sound/voice/computer/misc/1_frag.wav";
	present = { "sound/vox_d/computer/misc/1_frag.wav", voice };
	// German first, without a warning when it is missing.
	Check( voice, { { "sound/vox_d/computer/misc/1_frag.wav", true } }, true );
	language = "deutsch";
	Check( voice, { { "sound/vox_d/computer/misc/1_frag.wav", true } }, true );
	// No German recording: the name as given.
	present = { voice };
	Check( voice, { { "sound/vox_d/computer/misc/1_frag.wav", true }, { voice, false } }, true );
	present.clear();
	Check( voice, { { "sound/vox_d/computer/misc/1_frag.wav", true }, { voice, false } }, false );
	// Only the first "voice/", found without regard to case.
	Check( "sound/Voice/voice/x.wav", { { "sound/vox_d/voice/x.wav", true }, { "sound/Voice/voice/x.wav", false } }, false );
	// Not a voice, not German, or not Elite Force: the name as given.
	Check( "sound/weapons/phaser.wav", { { "sound/weapons/phaser.wav", false } }, false );
	for ( const char *other : { "", "francais", "DEUTSCH ", "german" } ) {
		language = other;
		Check( voice, { { voice, false } }, false );
	}
	language = "DEUTSCH";
	eliteForce = false;
	Check( voice, { { voice, false } }, false );
	eliteForce = true;
	// Retail copies 64 bytes: a "voice/" beyond them is not seen.
	const std::string longName = "sound/" + std::string( 60, 'a' ) + "/voice/x.wav";
	Check( longName.c_str(), { { longName, false } }, false );
}
'''

KEY_PROBE = r'''
#include "code/compat/stvef_key_adapter.h"
#include "code/compat/stvef_key_names.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
using fntech3::stvef::keys::NameForModule;
static bool Is( int key, const char *language, bool mp, const char *expected ) {
	char tiny[5];
	return !std::strcmp( NameForModule( key, language, mp, tiny ), expected );
}
int main() {
	// Elite Force's numbers, which are original Quake III's.
	assert( FnTech3_STVEFKeyToInternal( 'a' ) == 'a' && FnTech3_STVEFKeyToInternal( 127 ) == K_BACKSPACE );
	assert( FnTech3_STVEFKeyToInternal( 178 ) == K_MOUSE1 && FnTech3_STVEFKeyToInternal( 182 ) == K_MOUSE5 );
	assert( FnTech3_STVEFKeyToInternal( 183 ) == K_MWHEELDOWN && FnTech3_STVEFKeyToInternal( 184 ) == K_MWHEELUP );
	assert( FnTech3_STVEFKeyToInternal( 185 ) == K_JOY1 && FnTech3_STVEFKeyToInternal( 216 ) == K_JOY32 );
	assert( FnTech3_STVEFKeyToInternal( 217 ) == K_AUX1 && FnTech3_STVEFKeyToInternal( 232 ) == K_AUX16 );
	assert( FnTech3_STVEFKeyToInternal( 233 ) == -1 && FnTech3_STVEFKeyToInternal( -2 ) == -1 );
	// Events: the wheel is the wheel, never joystick button 3 or 4, which
	// Elite Force's menus take as Enter; keys it has no number for are dropped.
	assert( FnTech3_STVEFKeyEventFromInternal( K_MWHEELDOWN ) == 183 && FnTech3_STVEFKeyEventFromInternal( K_MWHEELUP ) == 184 );
	assert( FnTech3_STVEFKeyEventFromInternal( K_JOY1 ) == 185 && FnTech3_STVEFKeyEventFromInternal( K_AUX16 ) == 232 );
	for ( int key : { K_MOUSE6, K_MOUSE7, K_MOUSE8, K_MOUSE9 } ) assert( FnTech3_STVEFKeyEventFromInternal( key ) == -1 );
	for ( int key = K_AUX16 + 1; key < MAX_KEYS; ++key ) assert( FnTech3_STVEFKeyEventFromInternal( key ) == -1 );
	assert( FnTech3_STVEFKeyEventFromInternal( K_CHAR_FLAG | 'x' ) == ( 1024 | 'x' ) );
	assert( FnTech3_STVEFKeyEventFromInternal( K_ESCAPE ) == 27 && FnTech3_STVEFKeyEventFromInternal( K_ENTER ) == 13 );

	// Names by g_language, compared without regard to case.
	assert( Is( 13, "", true, "ENTER" ) && Is( 13, "deutsch", true, "EINGABETASTE" ) && Is( 13, "FRANCAIS", false, "ENTREE" ) );
	assert( Is( 138, "DEUTSCH", true, "UMSCHALLT" ) && Is( 138, "Deutsch", false, "UMSCHALT" ) );
	assert( Is( 138, "francais", true, "MAJ" ) && Is( 138, "italiano", true, "SHIFT" ) && Is( 138, nullptr, false, "SHIFT" ) );
	assert( Is( 183, "deutsch", true, "MRADUNTEN" ) && Is( 184, "francais", true, "MOLETTEHT." ) && Is( 183, "", true, "MWHEELDOWN" ) );
	assert( Is( 127, "deutsch", true, "R\xDC" "CKTASTE" ) && Is( 32, "francais", true, "ESPACE" ) );
	// Printable keys are themselves, ';' included; '"' and unnamed keys are hex.
	assert( Is( ';', "", true, ";" ) && Is( 'q', "deutsch", true, "q" ) && Is( '"', "", true, "0x22" ) && Is( 233, "", true, "0xe9" ) );
	assert( Is( -1, "", true, "<KEY NOT FOUND>" ) && Is( 256, "deutsch", true, "<OUT OF RANGE>" ) );
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def build_and_run(test, source, prefix, compiler_names, std):
    compiler = next(filter(None, (shutil.which(name) for name in compiler_names)), None)
    if not compiler:
        test.skipTest('compiler unavailable')
    target = compiler_target_args(compiler)
    if os.name == 'nt' and not target:
        target = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as folder:
        cpp = Path(folder) / 'probe.cpp'
        binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='latin-1')
        built = subprocess.run([compiler, *target, std, '-Wall', '-Wextra', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                               capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30, **options)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


class STVEFLanguageTests(unittest.TestCase):
    def test_german_voices_load_first(self):
        codec = read('code/client/audio/codecs/snd_codec.cpp')
        german = function(codec, 'bool S_STVEFGermanVoice(')
        load = function(codec, 'extern "C" void *S_CodecLoad(').replace('extern "C" ', 'static ')
        probe = VOICE_PROBE.replace('GERMAN', german).replace('LOAD', load)
        build_and_run(self, probe, 'stvef-voice-', ('clang++', 'g++'), '-std=c++17')

    def test_key_numbers_and_names(self):
        build_and_run(self, KEY_PROBE, 'stvef-keys-', ('clang++', 'g++'), '-std=c++17')

    def test_language_cvars_are_registered_as_retail_does(self):
        sound = read('code/client/audio/snd_main.cpp')
        init = function(sound, 'extern "C" void S_Init( void )')
        language = init.index('cv = Cvar_Get( "s_language", "", CVAR_ARCHIVE | CVAR_NORESTART );')
        # Before the s_initsound check, as retail registers it.
        self.assertLess(language, init.index('cv = Cvar_Get( "s_initsound", "1", 0 );'))
        self.assertIn('if ( Com_GameIsSTVEF() ) {', init[language - 200:language])
        client = read('code/client/cl_main.cpp')
        block = client[client.index('	if ( Com_GameIsSTVEF() ) {\n		// Retail\'s text and keyboard languages'):]
        block = block[:block.index('\n	}\n')]
        self.assertIn('Cvar_Get( "g_language", "", CVAR_ARCHIVE | CVAR_NORESTART );', block)
        self.assertIn('Cvar_Get( "k_language", "american", CVAR_ARCHIVE | CVAR_NORESTART );', block)

    def test_ui_keys_use_elite_force_numbers(self):
        ui = read('code/client/client_stvef_ui.hpp')
        shared = function(ui, 'static intptr_t CL_STVEFUISharedCall(')
        for call in ('UI_KEY_KEYNUMTOSTRINGBUF', 'UI_KEY_GETBINDINGBUF', 'UI_KEY_SETBINDING', 'UI_KEY_ISDOWN'):
            self.assertNotIn(f'STVEF_SHARED_CALL({call})', shared)
        dispatch = function(ui, 'static bool CL_STVEFUICall(')
        self.assertIn('CL_STVEFKeyString( static_cast<int>( args[1] ), VMA(2), static_cast<int>( args[3] ),\n'
                      '			call == Call::UI_KEY_GETBINDINGBUF ? qtrue : qfalse );', dispatch)
        self.assertIn('CL_STVEFSetBinding( static_cast<int>( args[1] ), VMA(2) );', dispatch)
        self.assertIn('result = CL_STVEFKeyIsDown( static_cast<int>( args[1] ) );', dispatch)
        keys = read('code/client/cl_keys.cpp')
        self.assertIn('fntech3::stvef::keys::NameForModule( key,\n'
                      '			Cvar_VariableString( "g_language" ), Com_GameIsSTVEFMP() != 0, tiny.data() )',
                      function(keys, 'void CL_STVEFKeyString('))
        event = function(keys, 'static void CL_UIKeyDownEvent(')
        self.assertIn('key = FnTech3_STVEFKeyEventFromInternal( key );', event)
        # Every key press the UI is handed goes through it.
        self.assertEqual(keys.count('VM_Call( uivm, 3, UI_KEY_EVENT, key, qtrue, time );'), 1)
        self.assertEqual(keys.count('CL_UIKeyDownEvent( key, time );'), 2)


if __name__ == '__main__':
    unittest.main()
