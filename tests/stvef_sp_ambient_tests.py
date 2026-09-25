"""Elite Force SP's ambient sound sets: sound/sound.txt and what plays from it.

Retail stvoy.exe reads the file once for each kind of set (AS_ParseSets
0x41d7e0): a generalSet is the room tone of wherever the player is, a
localSet plays around a point within its radius, and a bmodelSet holds the
sounds a mover starts, runs and stops with. Each set holds the seconds
between its sub-waves, up to eight of them, one looping wave and the volume
range they play at. The cgame names the sets a level uses before the file is
read, hands the engine the player's set every frame and the local sets
around them, and the game asks the same sets for its movers' sounds. The
checks pin the file's grammar, retail's corrections and warnings, the
crossfade between rooms, the timing and volume of sub-waves, and the loops'
own distances and pan.
"""
from pathlib import Path
import os
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
#include <cstring>
#include <string>
#include <vector>

#include "code/compat/stvef_sp_ambient.hpp"

typedef unsigned char byte;
#include "code/client/audio/shared/STVEFAmbient.h"

namespace amb = fntech3::stvef::ambient;
namespace loops = fntech3::audio::stvef;

static const char *kFile =
	"type ambientset\n"
	"\n"
	"amsdir q:/quake/baseef/sound\n"
	"outdir sound\n"
	"\n"
	";a comment\n"
	"generalSet roomtone\n"
	"timeBetweenWaves 5 2\n"
	"subWaves ambience/room one two three\n"
	"loopedWave ambience/room/tone\n"
	"volRange 250 100\n"
	"radius 300\n"
	"\n"
	"localSet console\n"
	"timeBetweenWaves 1 4\n"
	"loopedWave ambience/console/hum\n"
	"volRange 10 20\n"
	"radius 400\n"
	"bogusKeyword 3\n"
	"\n"
	"bmodelSet door\n"
	"subWaves movers/doors start move stop\n"
	"volRange 1 2\n"
	"\n"
	"generalSet crowded\n"
	"subWaves ambience/many a b c d e f g h i j k\n";

static void Grammar() {
	std::vector<amb::Set> sets;
	int swappedTimes = 0, swappedVolumes = 0, tooMany = 0, unknown = 0;
	const int count = amb::Parse( kFile,
		[&]( const amb::Set &set ) { sets.push_back( set ); },
		[&]( amb::Warning warning, const char * ) {
			switch ( warning ) {
			case amb::Warning::SwappedTimes: ++swappedTimes; break;
			case amb::Warning::SwappedVolumes: ++swappedVolumes; break;
			case amb::Warning::TooManySubWaves: ++tooMany; break;
			default: ++unknown; break;
			}
		} );
	assert( count == 4 && sets.size() == 4 );
	// The sets come in the order retail reads them: general, local, mover.
	assert( std::string( sets[0].name ) == "roomtone" && sets[0].type == amb::SetType::General );
	assert( std::string( sets[1].name ) == "crowded" && sets[1].type == amb::SetType::General );
	assert( std::string( sets[2].name ) == "console" && sets[2].type == amb::SetType::Local );
	assert( std::string( sets[3].name ) == "door" && sets[3].type == amb::SetType::BModel );

	// Swapped times and volumes are put right, each with a warning.
	assert( sets[0].timeStart == 2 && sets[0].timeEnd == 5 );
	assert( sets[0].volumeMin == 100 && sets[0].volumeMax == 250 );
	assert( swappedTimes == 1 && swappedVolumes == 1 );
	// A general set takes no radius, and the keyword ends it.
	assert( sets[0].radius == 0 );
	assert( sets[0].numSubWaves == 3 );
	assert( std::string( sets[0].subWaves[0] ) == "sound/ambience/room/one.wav" );
	assert( std::string( sets[0].subWaves[2] ) == "sound/ambience/room/three.wav" );
	assert( std::string( sets[0].loopedWave ) == "sound/ambience/room/tone.wav" );

	// A local set takes its radius, and an unknown keyword is reported once.
	assert( sets[2].radius == 400 && sets[2].timeStart == 1 && sets[2].timeEnd == 4 );
	assert( sets[2].volumeMin == 10 && sets[2].volumeMax == 20 );
	assert( std::string( sets[2].loopedWave ) == "sound/ambience/console/hum.wav" );
	assert( unknown == 1 );

	// A mover set takes only its sounds: the volume range ends it.
	assert( sets[3].numSubWaves == 3 && sets[3].volumeMin == 0 && sets[3].volumeMax == 0 );
	assert( std::string( sets[3].subWaves[0] ) == "sound/movers/doors/start.wav" );
	assert( std::string( sets[3].subWaves[2] ) == "sound/movers/doors/stop.wav" );
	assert( !sets[3].loopedWave[0] );

	// No set keeps more than eight sounds, and each one beyond is reported.
	assert( sets[1].numSubWaves == amb::MaxSubWaves );
	assert( tooMany == 3 );
	assert( std::string( sets[1].subWaves[7] ) == "sound/ambience/many/h.wav" );

	char warning[256];
	amb::WarningText( amb::Warning::TooManySubWaves, "crowded", warning, sizeof( warning ) );
	assert( std::string( warning ) == "^3WARNING: Too many subwaves on set \"crowded\"\n" );
}

static void Distances() {
	const float listener[3] = { 0, 0, 0 };
	const float axis[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	// At the listener, a full-volume loop is centred.
	const float here[3] = { 0, 0, 0 };
	loops::Gains gains = loops::Spatialize( here, 255.0f, listener, axis );
	assert( gains.left == 127 && gains.right == 127 );

	// Full volume holds to 256 units, and the pan follows the right axis.
	const float right[3] = { 0, 256, 0 };
	gains = loops::Spatialize( right, 255.0f, listener, axis );
	assert( gains.left == 255 && gains.right == 0 );
	const float left[3] = { 0, -256, 0 };
	gains = loops::Spatialize( left, 255.0f, listener, axis );
	assert( gains.left == 0 && gains.right == 255 );

	// Beyond it the volume falls by 0.0008 a unit, to silence at 1,506.
	const float half[3] = { 881, 0, 0 };
	gains = loops::Spatialize( half, 255.0f, listener, axis );
	assert( gains.left == 63 && gains.right == 63 );
	const float far[3] = { 1506, 0, 0 };
	gains = loops::Spatialize( far, 255.0f, listener, axis );
	assert( !gains.left && !gains.right );
	const float beyond[3] = { 4000, 0, 0 };
	gains = loops::Spatialize( beyond, 255.0f, listener, axis );
	assert( !gains.left && !gains.right );

	// A mono device hears both channels at three quarters.
	gains = loops::Spatialize( here, 255.0f, listener, axis, true );
	assert( gains.left == 191 && gains.right == 191 );

	// A silent set is not heard at all.
	gains = loops::Spatialize( here, 0.0f, listener, axis );
	assert( !gains.left && !gains.right );
}

static void Frame() {
	loops::AmbientLoopFrame frame;
	const float origin[3] = { 0, 0, 0 };
	for ( std::size_t i = 0; i < loops::AmbientLoopFrame::Capacity; ++i ) {
		assert( frame.Append( origin, 1, 200 ) >= 0 );
	}
	// Retail keeps no more than 32 a frame (0x420bf1).
	assert( !frame.Accepts() && frame.Append( origin, 1, 200 ) < 0 );
	frame.Clear();
	assert( frame.Size() == 0 );

	// Loops playing one sound are summed; another sound is left alone.
	const float listener[3] = { 0, 0, 0 };
	const float axis[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	frame.Append( origin, 7, 100 );
	frame.Append( origin, 9, 255 );
	frame.Append( origin, 7, 100 );
	loops::Gains gains = loops::SpatializeMerged( frame, 0, listener, axis );
	assert( gains.left == 100 && gains.right == 100 );
	gains = loops::SpatializeMerged( frame, 1, listener, axis );
	assert( gains.left == 127 && gains.right == 127 );
	// However many loops play one sound, they never exceed full volume.
	for ( int i = 0; i < 8; ++i ) frame.Append( origin, 7, 200 );
	gains = loops::SpatializeMerged( frame, 0, listener, axis );
	assert( gains.left == 255 && gains.right == 255 );
}

int main() {
	Grammar();
	Distances();
	Frame();
	return 0;
}
'''


class STVEFSPAmbientTests(unittest.TestCase):
    def setUp(self):
        self.runtime = read('code/client/client_stvef_sp_ambient.hpp')

    def test_the_file_and_the_loops(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-sp-ambient-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_the_cgame_reaches_the_sets(self):
        host = read('code/client/client_stvef_sp_cgame.hpp')
        common = function(host, 'static intptr_t CL_STVEFSPCgameCommonCall(')
        self.assertIn('CL_STVEFSPAmbientUpdateSet(', common)
        self.assertIn('CL_STVEFSPAmbientParseSets();', common)
        self.assertIn('CL_STVEFSPAmbientAddPrecacheEntry(', common)
        self.assertIn('return CL_STVEFSPAmbientAddLocalSet(', common)
        self.assertIn('return CL_STVEFSPAmbientBModelSound(', common)
        # The sets are answered whether or not the cgame has started, because
        # the game half asks for its movers' sounds while a level spawns.
        self.assertIn('#include "client_stvef_sp_ambient.hpp"', host)
        self.assertIn('if ( cls.soundRegistered ) {', common)

    def test_the_file_is_read_as_a_level_loads(self):
        parse = function(self.runtime, 'void CL_STVEFSPAmbientParseSets() {')
        self.assertIn('FS_ReadFile( "sound/sound.txt"', parse)
        self.assertIn('"^1ERROR: Couldn\'t load ambient sound sets from %s"', parse)
        self.assertIn('Com_Printf( "AS_ParseFile: Parsed %d ambient set(s)\\n", count );', parse)
        # Only the sets a level named have their waves loaded, as retail does.
        self.assertIn('for ( const std::string &kept : stvefSpAmbientPrecache ) {', parse)
        self.assertIn('if ( wanted ) {', parse)
        self.assertLess(parse.index('stvefSpAmbientSets.clear();'), parse.index('FS_ReadFile'))
        self.assertGreater(parse.rindex('stvefSpAmbientPrecache.clear();'), parse.index('AS_ParseFile'))
        register = function(self.runtime, 'sfxHandle_t CL_STVEFSPAmbientRegister( const char *path ) {')
        self.assertIn('S_RegisterSound( path, qfalse )', register)
        self.assertIn('"^3WARNING: Unable to load ambient sound \\"%s\\"\\n"', register)

    def test_a_room_fades_into_the_next(self):
        fade = function(self.runtime, 'void CL_STVEFSPAmbientSetCurrent( int index ) {')
        # The set the player leaves starts at full volume and falls; the one
        # they walk into starts at nothing and rises, over a second.
        self.assertIn('stvefSpAmbientSets[stvefSpAmbientOld].masterVolume = 255;', fade)
        self.assertIn('stvefSpAmbientSets[stvefSpAmbientCurrent].masterVolume = 0;', fade)
        self.assertIn('constexpr int kSTVEFSPAmbientFadeTime = 1000;', self.runtime)
        self.assertIn('before.masterVolume = before.masterVolume <= 0 ? 0 : 255 - faded( before.fadeTime );', fade)
        self.assertIn('stvefSpAmbientOld = -1;', fade)
        update = function(self.runtime, 'void CL_STVEFSPAmbientUpdateSet( const char *name, const float *origin ) {')
        # Both rooms play while the one behind is still fading.
        self.assertLess(update.index('CL_STVEFSPAmbientSetCurrent( index );'),
                        update.index('stvefSpAmbientCurrentWave'))
        self.assertIn('stvefSpAmbientOldWave', update)

    def test_the_waves_a_set_plays(self):
        play = function(self.runtime, 'void CL_STVEFSPAmbientPlay( const float *origin, STVEFSPAmbientSet &set,')
        # The looping wave every frame, at the volume the set has faded to.
        self.assertIn('S_AddSTVEFAmbientLoopingSound( origin, set.loopedWave, volume );', play)
        # A sub-wave once a fresh random delay in seconds has passed.
        self.assertIn('const int span = set.file.timeEnd + 1 - set.file.timeStart;', play)
        self.assertIn('( CL_STVEFSPAmbientRandom( span ) + set.file.timeStart ) * 1000', play)
        self.assertIn('if ( cls.realtime - lastWave < delay ) {', play)
        # Its volume comes from the set's range, scaled by the same volume.
        self.assertIn('const int low = static_cast<int>( set.file.volumeMin * scale );', play)
        self.assertIn('const int high = static_cast<int>( set.file.volumeMax * scale ) + 1;', play)
        self.assertIn('if ( wave > volume ) wave = volume;', play)
        self.assertIn('CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_AMBIENT )', play)
        random = function(self.runtime, 'int CL_STVEFSPAmbientRandom( int count ) {')
        self.assertIn('( ( std::rand() & 0x7fff ) * count ) >> 15', random)

    def test_a_local_set_is_heard_within_its_radius(self):
        local = function(self.runtime, 'int CL_STVEFSPAmbientAddLocalSet( const char *name, const float *listener,')
        # Full volume within half the radius, then falling to nothing at it.
        self.assertIn('const float half = radius * 0.5f;', local)
        self.assertIn('if ( distance < half ) {\n\t\tscale = 1.0f;', local)
        self.assertIn('scale = ( radius - distance ) / half;', local)
        self.assertIn('if ( scale > 1.0f || scale < 0.0f ) {\n\t\t\tscale = 0.0f;', local)
        # The answer is when the set last played, which the game keeps.
        self.assertIn('return cls.realtime;', local)
        self.assertIn('return wave;', local)
        mover = function(self.runtime, 'sfxHandle_t CL_STVEFSPAmbientBModelSound( const char *name, int stage ) {')
        self.assertIn('stage >= 0 && stage < stvefSpAmbientSets[index].numSubWaves', mover)
        self.assertIn('return -1;', mover)

    def test_both_sound_backends_play_the_loops(self):
        self.assertIn('void S_AddSTVEFAmbientLoopingSound( const vec3_t origin, sfxHandle_t sfx, int volume );',
                      read('code/client/audio/snd_public.h'))
        self.assertIn('if( !s->AddSTVEFAmbientLoopingSound ) return false;', read('code/client/audio/snd_main.cpp'))
        legacy = read('code/client/audio/legacy/snd_dma.cpp')
        add = function(legacy, 'static void S_Base_AddSTVEFAmbientLoopingSound( const vec3_t origin,')
        self.assertIn('stvefAmbientLoops.Append( origin, handle, volume );', add)
        self.assertIn('"%s has length 0"', add)
        mix = function(legacy, 'static void S_AddSTVEFAmbientLoopSounds( void ) {')
        self.assertIn('fntech3::audio::stvef::SpatializeMerged( stvefAmbientLoops, i,', mix)
        self.assertIn('if ( stvefAmbientLoops[j].handle == loop.handle ) merged[j] = true;', mix)
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {\n\t\tS_AddSTVEFAmbientLoopSounds();', legacy)
        # Every place the frame's loops are dropped drops the ambient ones.
        self.assertEqual(legacy.count('stvefAmbientLoops.Clear();'), 4)
        world = read('code/client/audio/openal/AudioSystemWorld.inl')
        openal = function(world, 'void Q3SoundWorld::AddSTVEFAmbientLoopingSound( const float *origin, sfxHandle_t handle,')
        self.assertIn('stvefAmbientLoops_.Append( origin, handle, volume )', openal)
        self.assertIn('voice.stvefAmbient = true;', openal)
        self.assertIn('voice.entnum = ENTITYNUM_NONE;', openal)
        # Each loop is its own placed voice, at its set's volume over a
        # one-shot's 127 and measured as a sound with no channel; the room
        # tone, played where the listener is, is heard there
        # (stvef_sp_openal_voice_tests runs both).
        apply = function(world, 'void Q3SoundWorld::ApplySTVEFVoice( SoundVoice &voice, qboolean softMuted ) {')
        self.assertIn('float gain = voice.stvefAmbient ? static_cast<float>( voice.stvefVolume ) / 127.0f : '
                      'voice.volumeScale;', apply)
        self.assertIn('if ( voice.stvefAmbient && listenerSound ) {', apply)
        span = function(world, 'static fntech3::audio::stvef::Span STVEFVoiceSpan( const SoundVoice &voice ) {')
        self.assertIn('ChannelSpan( voice.stvefAmbient ? -1 : voice.entchannel );', span)
        at = function(world, 'static bool STVEFHeardAtListener( const SoundVoice &voice, float distance, int listenerNumber ) {')
        self.assertIn('voice.stvefAmbient ? distance < fntech3::audio::stvef::AtListenerDistance', at)
        backend = read('code/client/audio/openal/AudioSystemBackend.inl')
        self.assertIn('void AudioSystem::AddSTVEFAmbientLoopingSound( const float *origin, sfxHandle_t handle, int volume ) {',
                      backend)
        self.assertIn('si->AddSTVEFAmbientLoopingSound =', backend)
        # A loop that belongs to no entity takes a voice of its own, so it
        # cannot take the voice of an entity that is looping a sound.
        self.assertIn('std::array<SoundVoice, MAX_GENTITIES + fntech3::audio::stvef::AmbientLoopFrame::Capacity> loopVoices_;',
                      world)
        self.assertIn('const int slot = MAX_GENTITIES + appended;', openal)
        self.assertIn('entityNum >= static_cast<int>( loopVoices_.size() )', world)
        # The ambient loops are dropped wherever the frame's loops are.
        self.assertEqual(world.count('stvefAmbientLoops_.Clear();'), 3)

    def test_the_sets_do_not_outlive_the_sounds_they_hold(self):
        clear = function(self.runtime, 'void CL_STVEFSPClearAmbientSets( void ) {')
        self.assertIn('stvefSpAmbientSets.clear();', clear)
        self.assertIn('stvefSpAmbientPrecache.clear();', clear)
        self.assertIn('stvefSpAmbientCurrent = -1;', clear)
        init = function(read('code/client/audio/snd_main.cpp'), 'extern "C" void S_Init( void )')
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {\n\t\t// Elite Force\'s ambient sets hold sounds of the '
                      'system being started.\n\t\tCL_STVEFSPClearAmbientSets();', init)


if __name__ == '__main__':
    unittest.main()
