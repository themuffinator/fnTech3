"""Medal of Honor: how far a sound carries.

Each 1.00 alias gives a min and a max distance (soundparms' fifth and sixth
numbers): full volume inside the min, silence from the max. Before this the
OpenAL backend scaled the engine's own 80/1,330-unit span by them, clamped to
twice and three times, and culled starts by the sound shader's range, so a
300/1,000 line was a quarter as loud 500 units off and a 10,000/10,000 line
meant for everyone never started past about 1,530 units. Now the alias's span
goes to the device's distance model unclamped and faded out toward the max,
the alias's straight line plays where the device has no model, a loaded
sound starts only inside its max and a streamed one (all dialogue) wherever
the listener is, as OpenMoHAA's engine does [O] (snd_openal_new.cpp
S_OPENAL_ShouldStart, S_OPENAL_Start2DSound, S_OPENAL_Respatialize). The
probes compile the shared law and read the production paths that apply it;
wolfenstein_openal_voice_tests runs the world's ApplyVoice on such a voice.
No retail file is read."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]
WORLD = 'code/client/audio/openal/AudioSystemWorld.inl'


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


class MohaaSoundDistanceTests(unittest.TestCase):
    def build_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='mohaa-distance-', dir=ROOT / '.tmp') as directory:
            cpp = Path(directory) / 'distance.cpp'
            exe = Path(directory) / 'distance.exe'
            cpp.write_text(source)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-fno-fast-math',
                                     '-I', str(ROOT), str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_alias_law(self):
        self.build_and_run(r'''
#include "code/client/audio/shared/MOHAADistance.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace fntech3::audio::mohaa;
int main() {
	/* The game's "none": 200 units of full volume, silent 64 times as far,
	 * each distance on its own; a malformed one is "none" too. */
	Span none = Resolve( -1.0f, -1.0f );
	assert( none.full == 200.0f && none.silent == 12800.0f );
	Span given = Resolve( 300.0f, -1.0f );
	assert( given.full == 300.0f && given.silent == 19200.0f );
	given = Resolve( -1.0f, 900.0f );
	assert( given.full == 200.0f && given.silent == 900.0f );
	const float nan = std::numeric_limits<float>::quiet_NaN();
	given = Resolve( nan, nan );
	assert( given.full == 200.0f && given.silent == 12800.0f );
	/* A dialogue line: full inside 300, a straight line to silence at 1,000. */
	const Span line = Resolve( 300.0f, 1000.0f );
	assert( Gain( 0.0f, line ) == 1.0f && Gain( 300.0f, line ) == 1.0f );
	assert( Gain( 650.0f, line ) == 0.5f && std::fabs( Gain( 825.0f, line ) - 0.25f ) < 1e-6f );
	assert( Gain( 1000.0f, line ) == 0.0f && Gain( 5000.0f, line ) == 0.0f );
	/* It starts only short of its max (the max squared above the distance
	 * squared). */
	assert( Audible( 999.0f, line ) && !Audible( 1000.0f, line ) && !Audible( 1e9f, line ) );
	/* A line meant for everyone: full volume to 10,000 units. */
	const Span everyone = Resolve( 10000.0f, 10000.0f );
	assert( Gain( 9999.0f, everyone ) == 1.0f && Audible( 9999.0f, everyone ) && !Audible( 10000.0f, everyone ) );
	/* A max of 0 is silent everywhere; the world never starts such a sound. */
	const Span nowhere = Resolve( 0.0f, 0.0f );
	assert( !Audible( 0.0f, nowhere ) );
	return 0;
}
''')

    def test_the_streamed_flag_reaches_the_world(self):
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        start = cgame[cgame.index('void QDECL StartSound('):cgame.index('void QDECL StopSound(')]
        self.assertNotIn('(void)streamed;', start)
        self.assertIn('pitch, channel,\n\t\tstreamed ? qtrue : qfalse );', start)
        self.assertIn('float pitch, int mohaaChannel, qboolean streamed );', read('code/client/audio/snd_public.h'))
        self.assertIn('pitch, mohaaChannel, streamed );', read('code/client/audio/snd_main.cpp'))
        backend = read('code/client/audio/openal/AudioSystemBackend.inl')
        self.assertIn('mohaaChannel, streamed != qfalse );', backend)
        self.assertIn('pitch, mohaaChannel,\n\t\tstreamed );', backend)
        # The software mixer has one distance model of its own.
        legacy = function(read('code/client/audio/legacy/snd_dma.cpp'),
                          'static void S_Base_StartMOHAASound( const vec3_t origin,')
        self.assertIn('(void)streamed;', legacy)

    def test_starts_follow_the_alias(self):
        world = read(WORLD)
        start = function(world, 'void Q3SoundWorld::StartMOHAASound( int entityNum,')
        self.assertIn('fntech3::audio::mohaa::Resolve( minDist, maxDist );', start)
        # The channel is taken over even for a sound that does not start.
        self.assertLess(start.index('StopMOHAASound( entityNum, mohaaChannel );'),
                        start.index('if ( !( span.silent > 0.0f ) ) {'))
        self.assertIn('mohaaNext_.streamed = streamed;', start)
        self.assertIn('mohaaNext_.minDist = span.full;', start)
        self.assertIn('mohaaNext_.maxDist = span.silent;', start)
        self.assertLess(start.index('mohaaNext_.active = true;'), start.index('StartSound( entityNum,'))
        self.assertIn('mohaaNext_ = MOHAASpan();', start)
        # A loaded sound is refused past its max; a streamed one never is,
        # and neither is held to the sound shader's range.
        generic = function(world, 'void Q3SoundWorld::StartSound( int entityNum,')
        rule = generic[generic.index('if ( mohaaNext_.active ) {'):generic.index('} else if ( !IsLocalOnlyChannel(')]
        self.assertIn('if ( !mohaaNext_.streamed && !IsLocalOnlyChannel( entchannel ) && entityNum != listenerNumber_ &&', rule)
        self.assertIn('if ( !fntech3::audio::mohaa::Audible(', rule)
        self.assertNotIn('AudibilityHorizonDistance', rule)
        self.assertLess(generic.index('if ( mohaaNext_.active ) {'), generic.index('FindFreeOneShot();'))

    def test_loops_voices_and_eviction_follow_the_alias(self):
        world = read(WORLD)
        loop = function(world, 'void Q3SoundWorld::AddMOHAALoopingSound( int slot,')
        self.assertIn('fntech3::audio::mohaa::Resolve( minDist, maxDist );', loop)
        self.assertLess(loop.index('if ( !( span.silent > 0.0f ) ) return;'), loop.index('AddLoopingSound( slot,'))
        self.assertIn('voice.mohaaMaxDist = span.silent;', loop)
        update = function(world, 'void Q3SoundWorld::Update( qboolean softMuted ) {')
        self.assertIn('const float horizon = voice.mohaaMaxDist > 0.0f ? voice.mohaaMaxDist : '
                      'AudibilityHorizonDistance( rangeScale );', update)
        apply = function(world, 'void Q3SoundWorld::ApplyVoice( SoundVoice &voice, qboolean softMuted ) {')
        self.assertIn('device_->ConfigureSourceDistanceRange( voice.source, mohaaSpan.full, mohaaSpan.silent );', apply)
        self.assertIn('spatial.gain = fntech3::audio::mohaa::Gain(', apply)
        self.assertIn('horizonGain = HorizonGainAt( distance, lawSilent );', apply)
        # No clamped scale of the engine's own span is left for these voices.
        self.assertNotIn('mohaaMinDist / kOpenALReferenceDistance', apply)
        self.assertNotIn('mohaaMaxDist / kOpenALMaxDistance', apply)
        estimate = function(world, 'static float EstimateVoiceAudibility( const SoundVoice &voice ) {')
        self.assertIn('voice.mohaaMaxDist > 0.0f ? ( std::max )( voice.mohaaMinDist, 1.0f ) : kOpenALReferenceDistance',
                      estimate)


if __name__ == '__main__':
    unittest.main()
