"""Jedi Academy weapon fire for a local player: the client game's sound mute
(import 36, a weapon switch) through the engine's wrapper and the legacy
mixer, and the host contracts the 2026-10-15 fire probe depends on."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


MUTE_PROBE = r'''
#include "code/qcommon/q_shared.h"
#include "code/client/client_jka_sound.hpp"
#include <cassert>
#include <vector>
struct ProbeError {};
static bool academy = true;
static int Com_GameIsJKAMP() { return academy; }
extern "C" void QDECL Com_Error( errorParm_t, const char *, ... ) { throw ProbeError{}; }
struct Stop { int entity, channel; };
static std::vector<Stop> stops;
static struct { void (*StopEntityChannelSound)( int, int ); } si;
static void Record( int entity, int channel ) { stops.push_back( { entity, channel } ); }
MUTE
int main() {
	using namespace fntech3::jka::sound;
	si.StopEntityChannelSound = Record;
	// A weapon switch mutes the player's weapon channel, Fn's own CHAN_WEAPON.
	S_MuteJKASound( 3, ChanWeapon );
	assert( stops.size() == 1 && stops[0].entity == 3 && stops[0].channel == CHAN_WEAPON );
	// Academy's own channels keep their own engine numbers.
	S_MuteJKASound( 7, ChanVoiceAtten );
	assert( stops.size() == 2 && stops[1].channel == EngineChannel( ChanVoiceAtten ) );
	assert( stops[1].channel != ChanVoiceAtten );
	// The automatic and less-attenuated channels never hold a sound to end.
	S_MuteJKASound( 3, ChanAuto );
	S_MuteJKASound( 3, ChanLessAtten );
	assert( stops.size() == 2 );
	// No backend: nothing to end, and nothing fails.
	si.StopEntityChannelSound = nullptr;
	S_MuteJKASound( 3, ChanWeapon );
	assert( stops.size() == 2 );
	// Another profile never reaches Academy's mute.
	academy = false;
	bool refused = false;
	try { S_MuteJKASound( 3, ChanWeapon ); } catch ( const ProbeError & ) { refused = true; }
	assert( refused );
}
'''

LEGACY_PROBE = r'''
#include <cassert>
enum { MAX_CHANNELS = 8 };
struct sfx_t { int id; };
struct channel_t { int entnum, entchannel; sfx_t *thesfx; };
static channel_t s_channels[MAX_CHANNELS];
static bool s_soundStarted = true;
static int freed;
static void S_ChannelFree( channel_t *v ) { v->thesfx = nullptr; ++freed; }
STOP
int main() {
	static sfx_t shot{ 1 };
	s_channels[0] = { 3, 2, &shot };   // the player's weapon: ends
	s_channels[1] = { 3, 2, &shot };   // a second voice there: ends too
	s_channels[2] = { 3, 3, &shot };   // the player's voice: plays on
	s_channels[3] = { 4, 2, &shot };   // another entity's weapon: plays on
	s_channels[4] = { 3, 2, nullptr }; // a free voice: untouched
	S_Base_StopEntityChannelSound( 3, 2 );
	assert( freed == 2 && !s_channels[0].thesfx && !s_channels[1].thesfx );
	assert( s_channels[2].thesfx && s_channels[3].thesfx );
	s_soundStarted = false;
	S_Base_StopEntityChannelSound( 3, 3 );
	assert( freed == 2 && s_channels[2].thesfx );
}
'''


class JKAWeaponFireTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = engine_fixture_args(compiler)
        if os.name != 'nt' and not target:
            target = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='jka-weapon-fire-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_mute_ends_one_academy_channel(self):
        main = read('code/client/audio/snd_main.cpp')
        mute = function(main, 'extern "C" void S_MuteJKASound(').replace('extern "C" ', 'static ')
        self.compile_and_run(MUTE_PROBE.replace('MUTE', mute))

    def test_legacy_mixer_frees_only_that_entity_channel(self):
        dma = read('code/client/audio/legacy/snd_dma.cpp')
        stop = function(dma, 'static void S_Base_StopEntityChannelSound(')
        self.compile_and_run(LEGACY_PROBE.replace('STOP', stop))
        self.assertIn('\tsi->StopEntityChannelSound = S_Base_StopEntityChannelSound;\n', dma)

    def test_openal_world_releases_only_that_entity_channel(self):
        world = read('code/client/audio/openal/AudioSystemWorld.inl')
        stop = function(world, 'void Q3SoundWorld::StopEntityChannelSound(')
        self.assertIn('voice.active && voice.entnum == entityNum && voice.entchannel == entchannel', stop)
        self.assertIn('voice.Release( *device_ );', stop)
        backend = read('code/client/audio/openal/AudioSystemBackend.inl')
        self.assertIn('si->StopEntityChannelSound = []( int entnum, int entchannel ) '
                      '{ AudioSystem::Get().StopEntityChannelSound( entnum, entchannel ); };', backend)
        self.assertIn('if ( started_ ) {\n\t\tworld_.StopEntityChannelSound( entnum, entchannel );',
                      function(backend, 'void AudioSystem::StopEntityChannelSound('))

    def test_cgame_serves_import_36_with_bounded_arguments(self):
        host = read('code/client/client_jka_cgame_ambient.hpp')
        case = host[host.index('    case Call::CG_S_MUTESOUND:'):host.index('    case Call::CG_S_UPDATEAMBIENTSET:')]
        self.assertIn('args[1] < 0 || args[1] >= MAX_GENTITIES', case)
        self.assertIn('S_MuteJKASound(static_cast<int>(args[1]), static_cast<int>(args[2]));', case)
        self.assertIn('result = 0;', case)

    def test_sparse_model_copy_is_served_not_refused(self):
        ghoul2 = read('code/client/client_jka_cgame_ghoul2.hpp')
        self.assertNotIn('sparse model-copy destinations are not served yet', ghoul2)
        self.assertIn('Jedi Academy cgame Ghoul2 model copy exceeds its owned model budget', ghoul2)

    def test_ui_key_event_reaches_the_module_as_retail_hands_it(self):
        vm = read('code/qcommon/vm.c')
        block = vm[vm.index('\tif ( Com_GameIsJKAMP() && vm->index == VM_UI ) {'):]
        block = block[:block.index('\t\treturn r;')]
        self.assertIn('const int forwarded = FNTech3_JKAUIForwardedArguments( callnum, nargs );', block)
        self.assertIn('for ( i = 0; i < forwarded; ++i ) args[i] = va_arg( ap, int32_t );', block)
        keys = read('code/client/cl_keys.cpp')
        self.assertIn('if ( Com_GameIsJKAMP() ) {\n\t\treturn FNTech3_JKAKeyEventFromInternal( key );',
                      function(keys, 'static int CL_ModuleKeyEvent('))


if __name__ == '__main__':
    unittest.main()
