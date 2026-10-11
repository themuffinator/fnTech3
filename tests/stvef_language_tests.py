"""Elite Force's languages: German voices, translated key names and the
language cvars, and the key numbers its UI is given; and Jedi Outcast's
German and French voices and Jedi Academy's German, French and Spanish ones.

Retail S_LoadSound (stvoyHM.exe 0x42c690, stvoy.exe 0x4232f3) plays the
German recording under sound/vox_d when s_language is DEUTSCH, and Outcast's
loaders (jk2mp.exe 0x45f520, jk2sp.exe 0x42adb0) the one under sound/chr_d or
sound/chr_f when it is DEUTSCH or FRANCAIS; Academy's (jamp.exe 0x4723e0,
jasp.exe 0x42dc80) add sound/chr_e for ESPANOL; the probe compiles fnTech3's
S_CodecLoad against recording stubs and checks the order it tries names in. The key probes compile the adapter and name headers; the
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
static bool outcast = false;
static bool outcastSP = false;
static std::string language = "DEUTSCH";
static bool Com_GameIsSTVEF() { return eliteForce; }
static bool Com_GameIsJK2() { return outcast; }
static bool Com_GameIsJK2SP() { return outcast && outcastSP; }
static bool academy = false;
static bool academySP = false;
static bool Com_GameIsJKASP() { return academy && academySP; }
static bool Com_GameIsJKAMP() { return academy && !academySP; }
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
JK2FILE
JK2VOICE
JKAVOICE
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
	// Jedi Outcast reads its own "<name>.wav" first and has no "vox_d".
	outcast = true;
	Check( "sound/voice/computer/misc/1_frag.mp3", { { voice, false } }, false );
	// Its voices: the first "chars" becomes "chr_d" or "chr_f", tried without
	// a warning before the English recording (jk2mp.exe 0x45f520).
	const char *kyle = "sound/chars/kyle/misc/death1.wav";
	const char *kyleGerman = "sound/chr_d/kyle/misc/death1.wav", *kyleFrench = "sound/chr_f/kyle/misc/death1.wav";
	present = { kyleGerman, kyleFrench, kyle };
	for ( const char *german : { "DEUTSCH", "deutsch", "Deutsch" } ) {
		language = german;
		Check( "sound/chars/kyle/misc/death1.mp3", { { kyleGerman, true } }, true );
	}
	for ( const char *french : { "FRANCAIS", "francais" } ) {
		language = french;
		Check( "sound/chars/kyle/misc/death1", { { kyleFrench, true } }, true );
	}
	// No French recording: the English one, which warns only when it is missing too.
	present = { kyleGerman, kyle };
	Check( kyle, { { kyleFrench, true }, { kyle, false } }, true );
	present.clear();
	Check( kyle, { { kyleFrench, true }, { kyle, false } }, false );
	// Only the first "chars", wherever it is.
	Check( "sound/chars/chars/x.wav", { { "sound/chr_f/chars/x.wav", true }, { "sound/chars/chars/x.wav", false } }, false );
	Check( "sound/xcharsx/y", { { "sound/xchr_fx/y.wav", true }, { "sound/xcharsx/y.wav", false } }, false );
	// English, a language Outcast has no voices in, or no "chars": the name alone.
	present = { kyleGerman, kyleFrench, kyle };
	for ( const char *other : { "", "english", "DEUTSCH ", "german", "italiano" } ) {
		language = other;
		Check( kyle, { { kyle, false } }, true );
	}
	language = "DEUTSCH";
	Check( "sound/weapons/saber/saberon.mp3", { { "sound/weapons/saber/saberon.wav", false } }, false );
	// Multiplayer finds "chars" as the module wrote it; single player lowercases
	// the name first (jk2sp.exe 0x42a96a), so it finds any case.
	const char *upper = "sound/CHARS/kyle/misc/death1.wav";
	present = { kyleGerman, upper };
	Check( upper, { { upper, false } }, true );
	outcastSP = true;
	Check( upper, { { kyleGerman, true } }, true );
	Check( kyle, { { kyleGerman, true } }, true );
	outcastSP = false;
	// Elite Force's German rule never reads "chars", nor Outcast's "voice/".
	outcast = false;
	Check( kyle, { { kyle, false } }, false );
	present.clear();
	eliteForce = true;
	Check( kyle, { { kyle, false } }, false );
	// Retail copies 64 bytes: a "voice/" beyond them is not seen.
	const std::string longName = "sound/" + std::string( 60, 'a' ) + "/voice/x.wav";
	Check( longName.c_str(), { { longName, false } }, false );

	// Jedi Academy, both profiles: each lowercases the name first (jamp.exe
	// 0x472726, jasp.exe 0x42dfc0), so its first "chars" in any case becomes
	// "chr_d", "chr_f" or "chr_e" for DEUTSCH, FRANCAIS or ESPANOL, tried
	// without a warning before the English recording, each name as the module
	// wrote it (jamp.exe 0x4723e0, jasp.exe 0x42dc80).
	eliteForce = false;
	academy = true;
	const char *taunt = "sound/chars/kyle/misc/taunt1.wav";
	const char *tauntGerman = "sound/chr_d/kyle/misc/taunt1.wav", *tauntFrench = "sound/chr_f/kyle/misc/taunt1.wav";
	const char *tauntSpanish = "sound/chr_e/kyle/misc/taunt1.wav";
	for ( bool singlePlayer : { false, true } ) {
		academySP = singlePlayer;
		present = { tauntGerman, tauntFrench, tauntSpanish, taunt };
		for ( const char *german : { "DEUTSCH", "deutsch", "Deutsch" } ) {
			language = german;
			Check( taunt, { { tauntGerman, true } }, true );
		}
		for ( const char *french : { "FRANCAIS", "francais" } ) {
			language = french;
			Check( taunt, { { tauntFrench, true } }, true );
		}
		for ( const char *spanish : { "ESPANOL", "espanol", "Espanol" } ) {
			language = spanish;
			Check( taunt, { { tauntSpanish, true } }, true );
		}
		// The extension the module asked for, or none, is kept.
		Check( "sound/chars/kyle/misc/taunt1.mp3", { { "sound/chr_e/kyle/misc/taunt1.mp3", true },
			{ "sound/chars/kyle/misc/taunt1.mp3", false } }, false );
		Check( "sound/chars/kyle/misc/taunt1", { { "sound/chr_e/kyle/misc/taunt1", true },
			{ "sound/chars/kyle/misc/taunt1", false } }, false );
		// No Spanish recording: the English one, which warns only when it is missing too.
		present = { tauntGerman, tauntFrench, taunt };
		Check( taunt, { { tauntSpanish, true }, { taunt, false } }, true );
		present.clear();
		Check( taunt, { { tauntSpanish, true }, { taunt, false } }, false );
		// "chars" in any case, the first only, wherever it is; the rest keeps its case.
		Check( "sound/CHARS/Kyle/x.wav", { { "sound/chr_e/Kyle/x.wav", true }, { "sound/CHARS/Kyle/x.wav", false } }, false );
		Check( "sound/Chars/chars/x.wav", { { "sound/chr_e/chars/x.wav", true }, { "sound/Chars/chars/x.wav", false } }, false );
		Check( "sound/xcharsx/y.wav", { { "sound/xchr_ex/y.wav", true }, { "sound/xcharsx/y.wav", false } }, false );
		// English, a language Academy has no voices in, or no "chars": the name alone.
		present = { tauntGerman, tauntFrench, tauntSpanish, taunt };
		for ( const char *other : { "", "english", "ESPANOL ", "espa\xf1ol", "spanish", "german", "italiano" } ) {
			language = other;
			Check( taunt, { { taunt, false } }, true );
		}
		language = "ESPANOL";
		Check( "sound/weapons/saber/saberon.wav", { { "sound/weapons/saber/saberon.wav", false } }, false );
		// Neither Elite Force's "voice/" nor Outcast's cut name.
		language = "DEUTSCH";
		Check( voice, { { voice, false } }, false );
		Check( "sound/chars/kyle/misc/taunt1.mp3", { { "sound/chr_d/kyle/misc/taunt1.mp3", true },
			{ "sound/chars/kyle/misc/taunt1.mp3", false } }, false );
	}
	// Spanish is Academy's alone: Outcast and Elite Force play English for it.
	academy = false;
	language = "ESPANOL";
	present = { tauntSpanish, taunt };
	outcast = true;
	Check( taunt, { { taunt, false } }, true );
	outcast = false;
	eliteForce = true;
	Check( taunt, { { taunt, false } }, true );
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
        outcast = function(codec, 'bool S_JK2SoundFile(')
        foreign = function(codec, 'bool S_JK2ForeignVoice(')
        academy = function(codec, 'bool S_JKAForeignVoice(')
        load = function(codec, 'extern "C" void *S_CodecLoad(').replace('extern "C" ', 'static ')
        probe = (VOICE_PROBE.replace('GERMAN', german).replace('JK2FILE', outcast).replace('JK2VOICE', foreign)
                 .replace('JKAVOICE', academy).replace('LOAD', load))
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
        # Jedi Outcast's: "english", archived, kept by cvar_restart, in both
        # profiles' S_Init before s_initsound (jk2mp.exe 0x45ca1a, jk2sp.exe
        # 0x423350). Its menus offer english, francais and deutsch.
        outcast = init.index('cv = Cvar_Get( "s_language", "english", CVAR_ARCHIVE | CVAR_NORESTART );',
                             init.index('\n\tif ( Com_GameIsJK2() ) {'))
        self.assertLess(outcast, init.index('cv = Cvar_Get( "s_initsound", "1", 0 );'))
        block = init[init.rindex('\n\tif ( ', 0, outcast):outcast]
        self.assertTrue(block.startswith('\n\tif ( Com_GameIsJK2() ) {'), block)
        # Jedi Academy's: the same, in both of its profiles' S_Init (jamp.exe
        # 0x4695cb, jasp.exe 0x425a2f). Its menus add espanol.
        academy = init.index('cv = Cvar_Get( "s_language", "english", CVAR_ARCHIVE | CVAR_NORESTART );',
                             init.index('\n\tif ( Com_GameIsJKASP() || Com_GameIsJKAMP() ) {'))
        self.assertLess(academy, init.index('cv = Cvar_Get( "s_initsound", "1", 0 );'))
        block = init[init.rindex('\n\tif ( ', 0, academy):academy]
        self.assertTrue(block.startswith('\n\tif ( Com_GameIsJKASP() || Com_GameIsJKAMP() ) {'), block)
        self.assertEqual(init.count('Cvar_Get( "s_language"'), 3)
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
        self.assertIn('if ( Com_GameIsSTVEF() ) {\n\t\treturn FnTech3_STVEFKeyEventFromInternal( key );',
                      function(keys, 'static int CL_ModuleKeyEvent('))
        self.assertIn('key = CL_ModuleKeyEvent( key );', function(keys, 'static void CL_UIKeyDownEvent('))
        # Every key press the UI is handed goes through it.
        self.assertEqual(keys.count('VM_Call( uivm, 3, UI_KEY_EVENT, key, qtrue, time );'), 1)
        self.assertEqual(keys.count('CL_UIKeyDownEvent( key, time );'), 2)


if __name__ == '__main__':
    unittest.main()
