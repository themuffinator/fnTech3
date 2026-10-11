"""Elite Force SP's lip-sync: how wide a mouth a voice opens.

Retail stvoy.exe hands its game one number an entity through the S_Override
slot of its import table (0x42562b) and fills that array as each mix ends
(0x421480): every entity is cleared, then every channel carrying a voice or
an attenuated voice is measured where the mix has reached (0x421520). The
measurement takes ten samples of the sound, one every hundred, squares the
top byte of each and takes the whole-number mean; that mean is compared with
s_threshold1 through s_threshold4 (0x41fb5d), each times the loudest byte the
sound itself reaches, and the answer is a closed mouth or one of four widths.

Retail keeps two arrays. The one the game reads is cleared every mix, so an
entity saying nothing reads no answer at all; the other remembers what each
entity last measured and answers for a channel whose turn has not come round
again, since none is measured more often than every 800 mixed samples. A
voice starting closes its entity's mouth (0x4208ec). The loudest byte comes
from Raven's own "#MAXVOL" tag, and a file without one counts as 128
(0x408c70, 0x408df2). Holomatch has none of this.

The checks pin the measurement itself, both arrays and the wait between
measurements, the tag, and both sound backends filling the array the game
reads.
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
#include <cstring>

#include "code/client/audio/shared/STVEFLipSync.h"
#include "code/client/audio/codecs/snd_mp3_frames.h"

namespace ls = fntech3::audio::stvef::lipsync;

static short gSamples[8000];

static void Fill( int value ) {
	for ( int i = 0; i < 8000; ++i ) gSamples[i] = static_cast<short>( value );
}

static short SampleAt( int index ) {
	return ( index >= 0 && index < 8000 ) ? gSamples[index] : 0;
}

// The mean of the squares of the top byte of ten samples, one every hundred.
static void Loudness() {
	Fill( 32767 );
	assert( ls::Energy( SampleAt, 0, 3000 ) == 127.0f * 127.0f );
	Fill( -32768 );
	assert( ls::Energy( SampleAt, 0, 3000 ) == 128.0f * 128.0f );
	Fill( 0 );
	assert( ls::Energy( SampleAt, 0, 3000 ) == 0.0f );

	// Only the samples inside the sound are counted, and a channel past the
	// end of its sound counts none at all.
	Fill( 32767 );
	assert( ls::Energy( SampleAt, 2950, 3000 ) == 127.0f * 127.0f );
	assert( !ls::Measurable( 3001, 3000 ) );
	assert( ls::Measure( SampleAt, 3001, 3000, 128.0f ) == ls::Unmeasured );

	// The mean stays a whole number: nine silent samples and one full-scale
	// one average to 1,612, not 1,612.9.
	Fill( 0 );
	gSamples[0] = 32767;
	assert( ls::Energy( SampleAt, 0, 3000 ) == 1612.0f );
}

// Each threshold times the loudest byte the sound reaches, in retail's order.
static void Widths() {
	const ls::Thresholds retail;
	assert( retail.closed == 0.5f && retail.narrow == 4.0f );
	assert( retail.wide == 7.0f && retail.widest == 8.0f );

	const float peak = 100.0f;
	assert( ls::Level( 49.0f, peak ) == ls::Closed );
	assert( ls::Level( 50.0f, peak ) == ls::NarrowestOpen );
	assert( ls::Level( 399.0f, peak ) == 5 );
	assert( ls::Level( 400.0f, peak ) == 6 );
	assert( ls::Level( 699.0f, peak ) == 6 );
	assert( ls::Level( 700.0f, peak ) == 7 );
	assert( ls::Level( 799.0f, peak ) == 7 );
	assert( ls::Level( 800.0f, peak ) == ls::WidestOpen );

	// A sound reporting no loudest byte of its own counts as 128.
	assert( ls::DefaultPeak == 128.0f );
	assert( ls::Level( 63.0f, 0.0f ) == ls::Closed );
	assert( ls::Level( 64.0f, 0.0f ) == ls::NarrowestOpen );
}

// The array the game reads, and the one the engine remembers.
static void Mouths() {
	ls::Mouths mouths;
	assert( ls::Mouths::Entities == 1024 );
	Fill( 32767 );

	// A channel at the very start of its sound is measured at once and kept.
	assert( ls::Answer( mouths, SampleAt, 7, 0, 0, 100, 3000, 125.0f ) == ls::WidestOpen );
	assert( mouths.Remembered( 7 ) == ls::WidestOpen );
	mouths.Mixed( 100 );

	// Before the wait runs out the entity answers with what it kept, even
	// though the sound beneath it has gone quiet.
	Fill( 0 );
	assert( ls::Answer( mouths, SampleAt, 7, 0, 500, 300, 3000, 125.0f ) == ls::WidestOpen );

	// Past the wait it is measured afresh.
	assert( ls::RefreshSamples == 800 );
	assert( ls::Answer( mouths, SampleAt, 7, 0, 500, 901, 3000, 125.0f ) == ls::Closed );

	// A channel at the very start is measured however recently the last one
	// was, and a channel past the end of its sound keeps nothing.
	Fill( 32767 );
	assert( ls::Answer( mouths, SampleAt, 8, 500, 500, 300, 3000, 125.0f ) == ls::WidestOpen );
	assert( ls::Answer( mouths, SampleAt, 9, 0, 4000, 5000, 3000, 125.0f ) == ls::Unmeasured );
	assert( mouths.Remembered( 9 ) == ls::Unmeasured );

	// The game's own array is cleared every mix; the memory is not. An entity
	// saying nothing reads no answer rather than a closed mouth.
	mouths.Write( 7, ls::WidestOpen );
	mouths.Clear();
	assert( mouths.Mouth( 7 ) == ls::Unmeasured );
	assert( mouths.Remembered( 7 ) == ls::Closed );
	assert( ls::Unmeasured == 0 && ls::Closed == -1 );

	// A voice starting closes that entity's mouth, and starting the sound
	// buffer again forgets everything.
	mouths.Close( 7 );
	assert( mouths.Mouth( 7 ) == ls::Closed );
	mouths.Reset();
	assert( mouths.Mouth( 7 ) == ls::Unmeasured && mouths.Remembered( 7 ) == ls::Unmeasured );

	// Nothing outside the array is read or written.
	mouths.Write( -1, ls::WidestOpen );
	mouths.Write( 1024, ls::WidestOpen );
	assert( mouths.Mouth( -1 ) == ls::Unmeasured && mouths.Mouth( 1024 ) == ls::Unmeasured );
}

// The top bytes the OpenAL backend keeps measure as the samples themselves do.
static void KeptBytes() {
	Fill( 0 );
	for ( int i = 0; i < 1000; ++i ) gSamples[i] = static_cast<short>( -32768 + i * 61 );
	ls::TopBytes kept;
	kept.Build( gSamples, 1000, 1 );
	assert( kept.Frames() == 1000 && !kept.Empty() );
	auto keptAt = [&]( int index ) { return kept.At( index ); };
	assert( ls::Energy( keptAt, 0, 900 ) == ls::Energy( SampleAt, 0, 900 ) );
	assert( kept.At( -1 ) == 0 && kept.At( 1000 ) == 0 );
	kept.Clear();
	assert( kept.Empty() );

	// One channel of a stereo sound is kept, as the sound is measured.
	short stereo[8] = { 32767, 0, 32767, 0, 32767, 0, 32767, 0 };
	kept.Build( stereo, 4, 2 );
	assert( kept.Frames() == 4 && kept.At( 1 ) == static_cast<short>( 127 << 8 ) );
}

// Raven's own tag reports the loudest byte the sound reaches.
static void RavenTag() {
	unsigned char file[200];
	std::memset( file, 0, sizeof( file ) );
	unsigned char *tag = file + sizeof( file ) - 128;
	std::memcpy( tag, "TAG", 3 );
	std::memcpy( tag + 0x3f, "#UNCOMP 129024", 14 );
	std::memcpy( tag + 0x61, "#MAXVOL 125", 11 );
	assert( fntech3::audio::Mp3RavenPeak( file, sizeof( file ) ) == 125.0f );

	// A file with no tag, a tag with no keyword, and a keyword with no
	// number all report none, which counts as 128.
	assert( fntech3::audio::Mp3RavenPeak( file, 64 ) == 0.0f );
	std::memcpy( tag, "XXX", 3 );
	assert( fntech3::audio::Mp3RavenPeak( file, sizeof( file ) ) == 0.0f );
	std::memcpy( tag, "TAG", 3 );
	std::memcpy( tag + 0x61, "#MAXVOL    ", 11 );
	assert( fntech3::audio::Mp3RavenPeak( file, sizeof( file ) ) == 0.0f );

	// The number is read no further than the comment field it sits in: the
	// digits run to the field's last byte, and the genre byte after it is a
	// digit too, which must not be counted.
	std::memset( tag + 0x61, 0, 31 );
	std::memcpy( tag + 0x61, "#MAXVOL", 7 );
	for ( int i = 7; i < 27; ++i ) tag[0x61 + i] = '0';
	std::memcpy( tag + 0x61 + 27, "125", 3 );
	tag[0x7f] = '7';
	assert( fntech3::audio::Mp3RavenPeak( file, sizeof( file ) ) == 125.0f );
}

int main() {
	Loudness();
	Widths();
	Mouths();
	KeptBytes();
	RavenTag();
	return 0;
}
'''


class STVEFSPLipSyncTests(unittest.TestCase):
    def setUp(self):
        self.header = read('code/client/audio/shared/STVEFLipSync.h')

    def test_the_measurement(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-sp-lipsync-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_the_array_the_game_reads(self):
        # The game reads one number an entity through its S_Override import,
        # and the engine measures exactly that many.
        abi = read('code/compat/stvef_game_abi.h')
        self.assertIn('FNTECH3_STVEF_SP_LIPSYNC_ENTITIES = 1024,', abi)
        self.assertIn('static constexpr std::size_t Entities = 1024;', self.header)
        api = read('code/compat/stvef_sp_game_api.h')
        self.assertIn('int32_t *S_Override;', api)
        game = read('code/server/server_stvef_sp_game.hpp')
        self.assertIn('import.S_Override = stvefSpVoiceVolume;', game)
        self.assertIn('std::int32_t stvefSpVoiceVolume[FNTECH3_STVEF_SP_LIPSYNC_ENTITIES]{};', game)

        # The client hands over what it measured, and only for single player,
        # so a dedicated server leaves every mouth shut.
        update = function(read('code/client/audio/snd_main.cpp'), 'extern "C" void S_Update( int msec )')
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {', update)
        self.assertIn('SV_STVEFSPVoiceVolumes( S_STVEFSPMouths() );', update)
        bridge = function(read('code/server/sv_game.cpp'), 'void SV_STVEFSPVoiceVolumes( const int *levels ) {')
        self.assertIn('std::copy( levels, levels + FNTECH3_STVEF_SP_LIPSYNC_ENTITIES, stvefSpVoiceVolume );', bridge)
        self.assertIn('if ( levels == NULL ) {', bridge)

    def test_the_cvars_a_mouth_is_measured_against(self):
        # Single player alone registers the four, unarchived, with retail's
        # own defaults, and they are read in the order retail compares them.
        registration = function(read('code/client/audio/snd_main.cpp'), 'extern "C" void S_Init( void )')
        for number, default in ((1, '0.5'), (2, '4.0'), (3, '7.0'), (4, '8.0')):
            self.assertIn('s_threshold%d = Cvar_Get( "s_threshold%d", "%s", 0 );' % (number, number, default),
                          registration)
        thresholds = registration[registration.index('s_threshold1 = Cvar_Get'):]
        gate = ('if ( Com_GameIsSTVEFSP() || Com_GameIsJKAMP() ) {' + chr(10) +
                '\t\t// Raven compares these in order')
        self.assertIn(gate, registration)
        self.assertNotIn('CVAR_ARCHIVE', thresholds[:thresholds.index('Cvar_SetDescription')])

        reader = function(read('code/client/audio/snd_local.h'),
                          'inline fntech3::audio::stvef::lipsync::Thresholds S_STVEFThresholds( void ) {')
        for number, field in ((1, 'closed'), (2, 'narrow'), (3, 'wide'), (4, 'widest')):
            self.assertIn('thresholds.%s = s_threshold%d->value;' % (field, number), reader)

    def test_the_channels_a_mouth_is_measured_on(self):
        # Retail measures the voice and the attenuated voice and nothing else,
        # and a voice starting closes that entity's mouth. Both backends do
        # the same, on the numbers those channels are marshalled to.
        measured = ('if ( ch.entchannel != fntech3::audio::stvef::VoiceChannel\n'
                    '\t\t\t&& ch.entchannel != fntech3::audio::stvef::AttenuatedVoiceChannel ) {')
        legacy = function(read('code/client/audio/legacy/snd_dma.cpp'),
                          'static void S_STVEFSPMeasureMouths( int position ) {')
        self.assertIn(measured, legacy)
        self.assertIn('stvefMouths.Clear();', legacy)
        self.assertIn('stvefMouths.Mixed( s_paintedtime );', legacy)

        openal = function(read('code/client/audio/openal/AudioSystemWorld.inl'),
                          'void Q3SoundWorld::STVEFSPMeasureMouths( int mixedSamples ) {')
        self.assertIn('if ( voice.entchannel != fntech3::audio::stvef::VoiceChannel', openal)
        self.assertIn('&& voice.entchannel != fntech3::audio::stvef::AttenuatedVoiceChannel ) {', openal)
        self.assertIn('stvefMouths_.Clear();', openal)
        self.assertIn('AL_SAMPLE_OFFSET', openal)

        closes = ('if ( Com_GameIsSTVEFSP() && ( entchannel == fntech3::audio::stvef::VoiceChannel\n'
                  '\t\t|| entchannel == fntech3::audio::stvef::AttenuatedVoiceChannel ) ) {')
        start = function(read('code/client/audio/legacy/snd_dma.cpp'),
                         'static void S_Base_StartSound( const vec3_t origin,')
        self.assertIn(closes, start)
        self.assertIn('stvefMouths.Close( entityNum );', start)
        world_start = function(read('code/client/audio/openal/AudioSystemWorld.inl'),
                               'void Q3SoundWorld::StartSound( int entityNum,')
        self.assertIn(closes, world_start)
        self.assertIn('stvefMouths_.Close( entityNum );', world_start)

    def test_the_loudest_byte_reaches_the_measurement(self):
        # Raven's tag is read by the codec, carried on the sound, and handed
        # to the measurement by both backends.
        self.assertIn('float peak = 0.0f;', read('code/client/audio/codecs/snd_codec.h'))
        codec = read('code/client/audio/codecs/snd_codec_mp3.cpp')
        self.assertIn('decodedInfo.peak = Mp3RavenPeak( compressed.as<std::uint8_t>(), length );', codec)
        self.assertIn("float			peak;", read('code/client/audio/snd_local.h'))
        self.assertIn('sfx->peak = info.peak;', read('code/client/audio/legacy/snd_mem.cpp'))

        legacy = function(read('code/client/audio/legacy/snd_dma.cpp'),
                          'static void S_STVEFSPMeasureMouths( int position ) {')
        self.assertIn('sfx->soundLength, sfx->peak, thresholds );', legacy)
        self.assertIn('stvefMouths.Write( ch.entnum, level );', legacy)
        world = read('code/client/audio/openal/AudioSystemWorld.inl')
        self.assertIn('if ( Com_GameIsSTVEFSP() ) stvefTopBytes_.Build(pcm.data(), frames_, channels_);', world)
        self.assertIn('peak_ = info.peak;', world)
        self.assertIn('sample->Frames(), sample->Peak(), thresholds ) );', world)

    def test_both_backends_answer_the_same_way(self):
        # The interface carries the array, every backend must supply it, and
        # neither backend is allowed to answer with a stale pointer.
        self.assertIn('const int *(*STVEFSPMouths)( void );', read('code/client/audio/snd_local.h'))
        main = read('code/client/audio/snd_main.cpp')
        self.assertIn('if( !s->STVEFSPMouths ) return false;', main)
        self.assertIn('return si.STVEFSPMouths ? si.STVEFSPMouths() : NULL;', main)
        self.assertIn('si->STVEFSPMouths = S_Base_STVEFSPMouths;', read('code/client/audio/legacy/snd_dma.cpp'))
        self.assertIn('si->STVEFSPMouths = []() { return AudioSystem::Get().STVEFSPMouths(); };',
                      read('code/client/audio/openal/AudioSystemBackend.inl'))

        # Holomatch keeps none of this: neither backend measures anything
        # outside the single-player profile.
        legacy = read('code/client/audio/legacy/snd_dma.cpp')
        before = legacy[:legacy.index('S_STVEFSPMeasureMouths( s_paintedtime );')]
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {', before[-200:])
        world = function(read('code/client/audio/openal/AudioSystemWorld.inl'),
                         'void Q3SoundWorld::STVEFSPMeasureMouths( int mixedSamples ) {')
        self.assertIn('if ( !device_ || !Com_GameIsSTVEFSP() ) {', world)
        self.assertLess(world.index('stvefMouths_.Clear();'), world.index('Com_GameIsSTVEFSP()'))


if __name__ == '__main__':
    unittest.main()
