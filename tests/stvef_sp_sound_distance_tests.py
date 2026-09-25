"""Elite Force SP's sound distances: how far each channel carries.

Retail stvoy.exe measures every sound the same way (S_SpatializeOrigin
0x420510) and hears it at full volume much further away than the engine does:
256 units rather than 80, silent by 1,506. A voice holds its full volume to
1,024 units, and an attenuated voice to 38.4 and then falls five times as
fast. The two channels are spread by the s_separation cvar both of Elite
Force's executables register (0x41fad2), and a mono device hears a sound on
both channels at three quarters. Multiplayer keeps Quake III's own distances
(stvoyHM.exe 0x42a580), so none of this may reach it, or any other profile.

The checks pin the distances themselves, the same law as each channel's span
for a backend with a distance model of its own, the channel numbers single
player's sounds arrive on, the legacy mixer measuring as retail does, and the
OpenAL backend taking the spans (stvef_sp_openal_voice_tests runs it).
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

static bool Near( float a, float b ) { return a - b < 0.01f && b - a < 0.01f; }

static void Spans() {
	// The same law as the distances a backend with its own distance model
	// takes: full volume out to the first, silent from the second on.
	ef::Span span = ef::ChannelSpan( 0 );
	assert( span.full == 256.0f && Near( span.silent, 1506.0f ) );
	span = ef::ChannelSpan( ef::VoiceChannel );
	assert( span.full == 1024.0f && Near( span.silent, 2274.0f ) );
	span = ef::ChannelSpan( ef::AttenuatedVoiceChannel );
	assert( span.full == 38.4000015f && Near( span.silent, 288.4f ) );
	for ( const int channel : { -1, 1, 2, 4, 5, 15, 18 } ) {
		assert( ef::ChannelSpan( channel ).full == 256.0f );
	}
	// Between them, the law's own gain is retail's measure without its pan:
	// a sound ahead takes the separation's half of it on each channel.
	for ( const int channel : { 0, ef::VoiceChannel, ef::AttenuatedVoiceChannel } ) {
		span = ef::ChannelSpan( channel );
		for ( float distance = 0; distance < 2400; distance += 7.5f ) {
			const float measured = static_cast<float>( Ahead( distance, channel ) ) / 127.0f;
			const float gain = ef::SpanGain( span, distance );
			assert( gain >= 0.0f && gain <= 1.0f && measured - gain < 0.01f && gain - measured < 0.01f );
		}
		assert( ef::SpanGain( span, span.silent ) == 0.0f && ef::SpanGain( span, span.full ) == 1.0f );
	}
	assert( ef::SpanGain( span, -1.0f ) == 1.0f && ef::SpanGain( span, INFINITY ) == 0.0f );

	// The attenuated voice is a voice to the engine's tone classes, and the
	// menu channels are single player's own.
	assert( ef::ToneChannel( ef::AttenuatedVoiceChannel ) == ef::VoiceChannel );
	for ( const int channel : { 0, 2, 3, 6, 15, 18 } ) {
		assert( ef::ToneChannel( channel ) == channel );
	}
	assert( ef::IsMenuChannel( 18 ) && ef::IsMenuChannel( 19 ) && !ef::IsMenuChannel( 7 ) && !ef::IsMenuChannel( 15 ) );
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

static void AtTheListener() {
	// The sounds the player themselves make are held back on every channel
	// the game numbers below its ambient one: the engine's own up to its body
	// channel, and single player's attenuated voice (0x4208cc).
	assert( ef::ListenerScale == 0.75f );
	for ( int channel = 0; channel <= 5; ++channel ) {
		assert( ef::QuieterAtListener( channel ) );
	}
	assert( ef::QuieterAtListener( ef::AttenuatedVoiceChannel ) );

	// The engine's local sound and announcer channels, and the numbers single
	// player's ambient and menu channels take, keep the whole of a sound.
	for ( const int channel : { 6, 7, 15, 18, 19 } ) {
		assert( !ef::QuieterAtListener( channel ) );
	}
}

int main() {
	Falloff();
	Spans();
	Spread();
	AtTheListener();
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

    def channels(self):
        """The engine's channel numbers, single player's, and the marshalling."""
        engine = read('code/qcommon/q_shared.h')
        names = engine[engine.index('\tCHAN_AUTO,'):engine.index('} soundChannel_t;')]
        numbers = {name.strip().split('\t')[0].split(',')[0].strip(): index
                   for index, name in enumerate(names.strip().splitlines())}
        abi = read('code/compat/stvef_game_abi.h')
        single = {name: int(value) for name, value in
                  re.findall(r'FNTECH3_STVEF_SP_(CHAN_\w+) = (\d+)', abi)}
        marshalled = function(read('code/client/client_stvef_records.hpp'),
                              'inline int CL_STVEFSPSoundChannel( int channel ) {')
        return numbers, single, marshalled

    def marshalled_channels(self):
        """Every single-player channel and the number the engine sees it on."""
        numbers, single, marshalled = self.channels()
        named = {single[sp]: numbers[name] for sp, name in
                 re.findall(r'case FNTECH3_STVEF_SP_(CHAN_\w+): return (CHAN_\w+);', marshalled)}
        return {value: named.get(value, numbers['CHAN_ANNOUNCER'] + 1 + value)
                for value in single.values()}

    def test_the_channels_a_sound_arrives_on(self):
        # Single player's voice keeps the engine's own number; its attenuated
        # voice, which the engine has no channel for, is marshalled to a
        # number above the engine's, and the mixers must look for that one.
        numbers, single, marshalled = self.channels()
        announcer = numbers['CHAN_ANNOUNCER']
        self.assertIn('case FNTECH3_STVEF_SP_CHAN_VOICE: return CHAN_VOICE;', marshalled)
        self.assertIn('default: return channel < 0 ? channel : CHAN_ANNOUNCER + 1 + channel;', marshalled)
        self.assertIn('inline constexpr int VoiceChannel = %d;' % numbers['CHAN_VOICE'], self.header)
        self.assertIn('inline constexpr int AttenuatedVoiceChannel = %d;'
                      % (announcer + 1 + single['CHAN_VOICE_ATTEN']), self.header)
        # The two menu channels are marshalled the same way.
        self.assertIn('inline constexpr int MenuChannel = %d;' % (announcer + 1 + single['CHAN_MENU1']), self.header)
        self.assertIn('inline constexpr int SecondMenuChannel = %d;' % (announcer + 1 + single['CHAN_MENU2']),
                      self.header)

    def test_the_sounds_the_player_makes(self):
        # Retail holds back every channel the game numbers below its ambient
        # one, so the engine's own numbers for those are what the mixers look
        # for; single player's ambient, local, announcer and menu sounds keep
        # the whole of their volume.
        marshalled = self.marshalled_channels()
        ambient = self.channels()[1]['CHAN_AMBIENT']
        quieter = {engine for single, engine in marshalled.items() if single < ambient}
        louder = {engine for single, engine in marshalled.items() if single >= ambient}
        self.assertEqual({0, 1, 2, 3, 4, 5, 12}, quieter)
        self.assertFalse(quieter & louder)
        self.assertIn('return ( channel >= 0 && channel <= %d ) || channel == AttenuatedVoiceChannel;'
                      % max(channel for channel in quieter if channel < 12), self.header)
        self.assertIn('inline constexpr float ListenerScale = 0.75f;', self.header)

        # Both backends hold the sound back as it starts, as retail does.
        held = ('if ( Com_GameIsSTVEFSP() && entityNum == %s &&\n'
                '\t\tfntech3::audio::stvef::QuieterAtListener( entchannel ) ) {\n'
                '\t\t// Single player is quieter about what the player themselves do.\n'
                '\t\tvolume *= fntech3::audio::stvef::ListenerScale;\n'
                '\t}')
        legacy = function(read('code/client/audio/legacy/snd_dma.cpp'),
                          'static void S_Base_StartSound( const vec3_t origin,')
        self.assertIn(held % 'listener_number', legacy)
        self.assertLess(legacy.index('volume *= fntech3::audio::stvef::ListenerScale;'),
                        legacy.index('const int masterVolume = S_VolumeToMasterVolume( volume );'))
        openal = function(read('code/client/audio/openal/AudioSystemWorld.inl'),
                          'void Q3SoundWorld::StartSound( int entityNum,')
        self.assertIn(held % 'listenerNumber_', openal)

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

    def test_the_openal_backend_renders_the_law_as_spans(self):
        # The OpenAL backend takes each channel's distances rather than
        # retail's gain and pan: the device's distance model across the span,
        # faded toward its silent end, and the law's own gain only where the
        # device has no model (stvef_sp_openal_voice_tests runs it).
        world = read('code/client/audio/openal/AudioSystemWorld.inl')
        apply = function(world, 'void Q3SoundWorld::ApplyVoice( SoundVoice &voice, qboolean softMuted ) {')
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {\n\t\tApplySTVEFVoice( voice, softMuted );', apply)
        span = function(world, 'static fntech3::audio::stvef::Span STVEFVoiceSpan( const SoundVoice &voice ) {')
        self.assertIn('ChannelSpan( voice.stvefAmbient ? -1 : voice.entchannel );', span)
        body = function(world, 'void Q3SoundWorld::ApplySTVEFVoice( SoundVoice &voice, qboolean softMuted ) {')
        self.assertIn('device_->ConfigureSourceDistanceRange( voice.source, span.full, span.silent );', body)
        self.assertIn('gain *= modelDistance ? HorizonGainAt( distance, span.silent ) : stvef::SpanGain( span, distance );',
                      body)
        self.assertIn('stvef::SpreadRadius( stvef::SeparationSpread( separation ), distance )', body)
        # Retail's pan reaches no placed voice; only the room tone, a centred
        # sound, keeps the share of it the separation leaves.
        self.assertEqual(1, body.count('stvef::Spatialize('))
        self.assertIn('if ( voice.stvefAmbient && listenerSound ) {', body)


if __name__ == '__main__':
    unittest.main()
