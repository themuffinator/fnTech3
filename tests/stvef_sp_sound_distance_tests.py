"""Elite Force SP's sound distances: how far each channel carries.

Retail stvoy.exe measures every sound the same way (S_SpatializeOrigin
0x420510) and hears it at full volume much further away than the engine does:
256 units rather than 80, silent by 1,506. A voice holds its full volume to
1,024 units, and an attenuated voice to 38.4 and then falls five times as
fast. The two channels are spread by the s_separation cvar both of Elite
Force's executables register (0x41fad2), and a mono device hears a sound on
both channels at three quarters. Multiplayer keeps Quake III's own distances
(stvoyHM.exe 0x42a580), so none of this may reach it, or any other profile.

The checks pin the distances themselves, the channel numbers single player's
sounds arrive on, and both sound backends measuring the same way.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


PROBE = r'''
#include <cassert>
#include <cstddef>

typedef unsigned char byte;
#include "code/client/audio/shared/STVEFAmbient.h"

namespace ef = fntech3::audio::stvef;

static const float kListener[3] = { 0, 0, 0 };
static const float kAxis[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

// The loudness of a sound this far ahead of the listener, on this channel.
static int Ahead( float distance, int channel ) {
	const float origin[3] = { distance, 0, 0 };
	const ef::Gains gains = ef::Spatialize( origin, 255.0f, kListener, kAxis,
		false, ef::Separation, channel );
	assert( gains.left == gains.right );
	return gains.left;
}

static void Falloff() {
	float full = 0, attenuation = 0;

	// A sound with no channel of its own carries to 256 units at full volume
	// and is silent by 1,506 (0x420578).
	ef::ChannelFalloff( 0, full, attenuation );
	assert( full == 256.0f && attenuation == 0.0008f );
	assert( Ahead( 0, 0 ) == 127 && Ahead( 256, 0 ) == 127 );
	assert( Ahead( 881, 0 ) == 63 );
	assert( Ahead( 1506, 0 ) == 0 && Ahead( 4000, 0 ) == 0 );

	// A voice holds its full volume four times as far (0x42055b).
	ef::ChannelFalloff( ef::VoiceChannel, full, attenuation );
	assert( full == 1024.0f && attenuation == 0.0008f );
	assert( Ahead( 1024, ef::VoiceChannel ) == 127 );
	assert( Ahead( 1506, ef::VoiceChannel ) > 0 );
	assert( Ahead( 2274, ef::VoiceChannel ) == 0 );

	// An attenuated voice hardly carries at all, and falls five times as
	// fast, silent by about 290 (0x420568).
	ef::ChannelFalloff( ef::AttenuatedVoiceChannel, full, attenuation );
	assert( full == 38.4000015f && attenuation == 0.004f );
	assert( Ahead( 38, ef::AttenuatedVoiceChannel ) == 127 );
	assert( Ahead( 163, ef::AttenuatedVoiceChannel ) == 63 );
	assert( Ahead( 289, ef::AttenuatedVoiceChannel ) == 0 );

	// Every other channel carries as far as a sound with none.
	for ( int channel = 0; channel < 20; ++channel ) {
		if ( channel == ef::VoiceChannel || channel == ef::AttenuatedVoiceChannel ) continue;
		assert( Ahead( 1000, channel ) == Ahead( 1000, 0 ) );
	}
	// A sound whose channel is not the game's own is measured that way too.
	assert( Ahead( 1200, -1 ) == Ahead( 1200, 0 ) );
}

static void Spread() {
	const float right[3] = { 0, -100, 0 };

	// None of a sound reaches the far channel when the two are spread as far
	// as they go, and both hear all of it when they are not spread at all.
	ef::Gains gains = ef::Spatialize( right, 255.0f, kListener, kAxis, false, 0.0f );
	assert( gains.right == 255 && gains.left == 0 );
	gains = ef::Spatialize( right, 255.0f, kListener, kAxis, false, 1.0f );
	assert( gains.right == 255 && gains.left == 255 );

	// Half, the cvar's default, is the even spread the engine has of its own.
	gains = ef::Spatialize( right, 255.0f, kListener, kAxis, false, ef::Separation );
	assert( gains.right == 255 && gains.left == 0 );
	const float ahead[3] = { 100, 0, 0 };
	gains = ef::Spatialize( ahead, 255.0f, kListener, kAxis, false, ef::Separation );
	assert( gains.right == 127 && gains.left == 127 );

	// A mono device hears a sound on both channels at three quarters,
	// however it is spread (0x4205d1).
	gains = ef::Spatialize( right, 255.0f, kListener, kAxis, true, 0.0f );
	assert( gains.left == 191 && gains.right == 191 );
}

int main() {
	Falloff();
	Spread();
	return 0;
}
'''


class STVEFSPSoundDistanceTests(unittest.TestCase):
    def setUp(self):
        self.header = read('code/client/audio/shared/STVEFAmbient.h')

    def test_the_distances(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-sp-distance-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_the_channels_a_sound_arrives_on(self):
        # Single player's voice keeps the engine's own number; its attenuated
        # voice, which the engine has no channel for, is marshalled to a
        # number above the engine's, and the mixers must look for that one.
        engine = read('code/qcommon/q_shared.h')
        names = engine[engine.index('\tCHAN_AUTO,'):engine.index('} soundChannel_t;')]
        numbers = {name.strip().split('\t')[0].split(',')[0].strip(): index
                   for index, name in enumerate(names.strip().splitlines())}
        announcer = numbers['CHAN_ANNOUNCER']
        abi = read('code/compat/stvef_game_abi.h')
        single = {name: int(value) for name, value in
                  re.findall(r'FNTECH3_STVEF_SP_(CHAN_\w+) = (\d+)', abi)}
        marshalled = function(read('code/client/client_stvef_records.hpp'),
                              'inline int CL_STVEFSPSoundChannel( int channel ) {')
        self.assertIn('case FNTECH3_STVEF_SP_CHAN_VOICE: return CHAN_VOICE;', marshalled)
        self.assertIn('default: return channel < 0 ? channel : CHAN_ANNOUNCER + 1 + channel;', marshalled)
        self.assertIn('inline constexpr int VoiceChannel = %d;' % numbers['CHAN_VOICE'], self.header)
        self.assertIn('inline constexpr int AttenuatedVoiceChannel = %d;'
                      % (announcer + 1 + single['CHAN_VOICE_ATTEN']), self.header)

    def test_the_cvar_the_channels_are_spread_by(self):
        registration = function(read('code/client/audio/snd_main.cpp'), 'extern "C" void S_Init( void )')
        self.assertIn('s_separation = Cvar_Get( "s_separation", "0.5", CVAR_ARCHIVE );', registration)
        # Only Elite Force registers it; the games that do not are spread by
        # the same half of their own.
        self.assertIn('if ( Com_GameIsSTVEF() ) {', registration)
        self.assertLess(registration.index('if ( Com_GameIsSTVEF() ) {'),
                        registration.index('s_separation = Cvar_Get('))
        helper = function(read('code/client/audio/snd_local.h'), 'static ID_INLINE float S_STVEFSeparation( void ) {')
        self.assertIn('if ( s_separation == NULL ) return 0.5f;', helper)

    def test_the_legacy_mixer_measures_each_channel(self):
        mixer = read('code/client/audio/legacy/snd_dma.cpp')
        spatialize = function(mixer, 'static void S_SpatializeOrigin( const vec3_t origin,')
        self.assertIn('int entchannel )', spatialize)
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {', spatialize)
        self.assertIn('ChannelFalloff( entchannel, full_volume, dist_mult );', spatialize)
        self.assertIn('mono_scale = fntech3::audio::stvef::MonoScale;', spatialize)
        self.assertIn('dist -= full_volume;', spatialize)
        self.assertIn('separation = S_STVEFSeparation();', spatialize)
        # The channel a sound plays on reaches the measurement, and retail
        # measures its ambient loops as sounds with no channel of their own
        # (0x420d28 passes none).
        self.assertIn('S_SpatializeOrigin (origin, ch->master_vol, &ch->leftvol, &ch->rightvol, ch->entchannel);', mixer)
        self.assertEqual(4, mixer.count('&right_total, CHAN_AUTO)') + mixer.count('&right, CHAN_AUTO)'))

    def test_the_openal_backend_measures_the_same_way(self):
        world = read('code/client/audio/openal/AudioSystemWorld.inl')
        apply = function(world, 'void Q3SoundWorld::ApplyVoice( SoundVoice &voice, qboolean softMuted ) {')
        self.assertIn('const bool stvefSpatial = Com_GameIsSTVEFSP() && !voice.stvefAmbient;', apply)
        self.assertIn('} else if ( stvefSpatial ) {', apply)
        self.assertIn('S_STVEFSeparation(), voice.entchannel );', apply)
        # The device may not attenuate what single player has measured
        # itself, and the fade that hides the device's own floor would cut a
        # voice short of the distance it carries.
        self.assertIn('const bool worldDistance = positionalSource && !stvefSpatial;', apply)
        self.assertIn('const bool useOpenALDistance = worldDistance && device_->UsesOpenALDistanceAttenuation();', apply)
        self.assertIn('ConfigureSourceDistance( voice.source, worldDistance,', apply)
        self.assertIn('if ( worldDistance ) {\n\t\t\t\thorizonGain = HorizonGain(', apply)
        # A single player sound is still placed in the world, so the engine's
        # reverb and occlusion reach it as they reach every other profile's.
        self.assertNotIn('stvefSpatial', re.search(r'\tconst bool rtcwSpatial = .*', apply).group( 0 ))


if __name__ == '__main__':
    unittest.main()
