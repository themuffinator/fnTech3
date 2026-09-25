"""Medal of Honor: the sounds a level's script waits on.

The client's record of each waited sound is production's code, compiled from
code/client/cl_mohaa_sound.cpp against stub codecs and a stub clock. The game
module asks after a sound on the channel the client told it of until the
sound has ended (import 135); a script's `playsound <alias> wait` and
`waittill sounddone` hang on it."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class MohaaWaitedSoundTests(unittest.TestCase):
    def test_waited_sounds_play_for_their_length_and_end_when_replaced(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_mohaa_sound.cpp').read_text()
        start = source.index('namespace {\n\nstruct WaitedSound {')
        tracker = source[start:]
        harness = r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
struct snd_info_t { int rate, width, channels, samples, size, dataofs; };
struct snd_stream_t { snd_info_t info; };
struct { int realtime; } cls;
int opens;
/* Two files the codecs read: a second at 22050 Hz and half a second at
 * 44100 Hz; any other is unreadable. */
snd_stream_t *S_CodecOpenStream( const char *name ) {
	static snd_stream_t stream;
	opens++;
	if ( !std::strcmp( name, "sound/one.wav" ) || !std::strcmp( name, "SOUND/ONE.WAV" ) ) {
		stream.info = { 22050, 2, 1, 22050, 44100, 0 };
		return &stream;
	}
	if ( !std::strcmp( name, "sound/half.mp3" ) ) {
		stream.info = { 44100, 2, 2, 22050, 88200, 0 };
		return &stream;
	}
	return nullptr;
}
void S_CodecCloseStream( snd_stream_t * ) {}
int Q_stricmp( const char *a, const char *b ) {
	for ( ;; a++, b++ ) {
		const int x = std::tolower( static_cast<unsigned char>( *a ) );
		const int y = std::tolower( static_cast<unsigned char>( *b ) );
		if ( x != y ) return x - y;
		if ( !x ) return 0;
	}
}
''' + tracker + r'''
int main() {
	cls.realtime = 1000;
	/* A waited sound plays for its file's length, and only under its own name. */
	const int one = CL_MOHAASoundStarted( 5, 4, "sound/one.wav", -1.0f, true );
	assert( one >= 0 );
	assert( CL_MOHAASoundPlaying( one, "sound/one.wav" ) );
	assert( CL_MOHAASoundPlaying( one, "SOUND/ONE.WAV" ) );
	assert( !CL_MOHAASoundPlaying( one, "sound/half.mp3" ) );
	assert( !CL_MOHAASoundPlaying( one, nullptr ) );
	assert( !CL_MOHAASoundPlaying( -1, "sound/one.wav" ) && !CL_MOHAASoundPlaying( 64, "sound/one.wav" ) );
	cls.realtime = 1999;
	assert( CL_MOHAASoundPlaying( one, "sound/one.wav" ) );
	cls.realtime = 2000;
	assert( !CL_MOHAASoundPlaying( one, "sound/one.wav" ) );

	/* Pitch shortens or lengthens it; a file's length is read once, whatever
	 * the case of its name. */
	const int readsBefore = opens;
	const int fast = CL_MOHAASoundStarted( 6, 4, "SOUND/ONE.WAV", 2.0f, true );
	assert( opens == readsBefore );
	cls.realtime = 2499;
	assert( CL_MOHAASoundPlaying( fast, "sound/one.wav" ) );
	cls.realtime = 2500;
	assert( !CL_MOHAASoundPlaying( fast, "sound/one.wav" ) );

	/* A later sound on the same entity channel takes the channel over: an
	 * unwaited one ends the waited, a waited one plays on the same channel
	 * under its own name. */
	cls.realtime = 3000;
	const int voice = CL_MOHAASoundStarted( 7, 4, "sound/one.wav", 1.0f, true );
	assert( CL_MOHAASoundStarted( 7, 4, "sound/half.mp3", 1.0f, false ) == -1 );
	assert( !CL_MOHAASoundPlaying( voice, "sound/one.wav" ) );
	const int again = CL_MOHAASoundStarted( 8, 4, "sound/one.wav", 1.0f, true );
	const int replaced = CL_MOHAASoundStarted( 8, 4, "sound/half.mp3", 1.0f, true );
	assert( replaced == again );
	assert( !CL_MOHAASoundPlaying( again, "sound/one.wav" ) );
	assert( CL_MOHAASoundPlaying( replaced, "sound/half.mp3" ) );
	/* Another entity, or another channel of the same one, leaves it be. */
	CL_MOHAASoundStarted( 9, 4, "sound/one.wav", 1.0f, false );
	CL_MOHAASoundStarted( 8, 5, "sound/one.wav", 1.0f, false );
	assert( CL_MOHAASoundPlaying( replaced, "sound/half.mp3" ) );
	/* The automatic channel never takes one over. */
	const int automatic = CL_MOHAASoundStarted( 10, 0, "sound/one.wav", 1.0f, true );
	CL_MOHAASoundStarted( 10, 0, "sound/half.mp3", 1.0f, false );
	const int second = CL_MOHAASoundStarted( 10, 0, "sound/one.wav", 1.0f, true );
	assert( second != automatic );
	assert( CL_MOHAASoundPlaying( automatic, "sound/one.wav" ) && CL_MOHAASoundPlaying( second, "sound/one.wav" ) );

	/* A stop ends what its entity channel plays, and nothing else. */
	CL_MOHAASoundStopped( 8, 4 );
	assert( !CL_MOHAASoundPlaying( replaced, "sound/half.mp3" ) );
	assert( CL_MOHAASoundPlaying( automatic, "sound/one.wav" ) );

	/* A file the codecs cannot read still has a channel, so the game is told
	 * and its wait ends at once rather than never. */
	const int missing = CL_MOHAASoundStarted( 11, 4, "sound/missing.wav", 1.0f, true );
	assert( missing >= 0 && !CL_MOHAASoundPlaying( missing, "sound/missing.wav" ) );
	/* Without a name there is nothing to wait on. */
	assert( CL_MOHAASoundStarted( 12, 4, "", 1.0f, true ) == -1 );
	assert( CL_MOHAASoundStarted( 12, 4, nullptr, 1.0f, true ) == -1 );

	/* Sixty-four channels: a new waited sound takes one that has ended, and
	 * takes a playing one only when every one plays. */
	CL_MOHAASoundsClear();
	cls.realtime = 10000;
	int ids[64];
	for ( int i = 0; i < 64; i++ ) {
		ids[i] = CL_MOHAASoundStarted( 100 + i, 0, "sound/one.wav", 1.0f, true );
		assert( ids[i] == i );
	}
	CL_MOHAASoundStopped( 100 + 30, 0 );
	CL_MOHAASoundStopped( 100 + 3, 0 );
	/* The ring carries on from where it stopped: past the ones still playing
	 * to the first that is free. */
	const int reuse = CL_MOHAASoundStarted( 200, 0, "sound/one.wav", 1.0f, true );
	assert( reuse == 3 );
	for ( int i = 0; i < 64; i++ ) {
		if ( i != 3 && i != 30 ) assert( CL_MOHAASoundPlaying( ids[i], "sound/one.wav" ) );
	}
	const int next = CL_MOHAASoundStarted( 201, 0, "sound/one.wav", 1.0f, true );
	assert( next == 30 );
	/* With all sixty-four playing, the next in turn goes. */
	const int full = CL_MOHAASoundStarted( 202, 0, "sound/half.mp3", 1.0f, true );
	assert( full == 31 );
	assert( CL_MOHAASoundPlaying( full, "sound/half.mp3" ) && !CL_MOHAASoundPlaying( full, "sound/one.wav" ) );
	assert( CL_MOHAASoundPlaying( next, "sound/one.wav" ) );

	/* A new client game waits on nothing it did not start. */
	CL_MOHAASoundsClear();
	for ( int i = 0; i < 64; i++ ) assert( !CL_MOHAASoundPlaying( i, "sound/one.wav" ) );
	return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'waited.cpp'
            exe = Path(directory) / 'waited.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_flag_reaches_the_game_and_its_question_the_client(self):
        cgame = (ROOT / 'code/client/cl_mohaa_cgame.cpp').read_text()
        start = cgame[cgame.index('void QDECL StartSound('):cgame.index('void QDECL StopSound(')]
        # The flag comes off the entity number before anything uses it, and
        # the local game hears of the channel only when a server runs here.
        self.assertIn('constexpr int kSoundWaitedFlag = 0x400;', cgame)
        self.assertLess(start.index('entityNumber &= ~kSoundWaitedFlag;'),
                        start.index('S_StartMOHAASound('))
        self.assertIn('if ( waited && id >= 0 && com_sv_running && com_sv_running->integer ) {', start)
        self.assertIn('SV_MOHAASoundCallback( entityNumber, id, sound );', start)
        stop = cgame[cgame.index('void QDECL StopSound('):]
        self.assertIn('CL_MOHAASoundStopped( entityNumber, channel );', stop[:stop.index('\n}\n')])

        game = (ROOT / 'code/server/server_mohaa_game.hpp').read_text()
        playing = game[game.index('int QDECL IsSoundPlaying('):]
        playing = playing[:playing.index('\n}\n')]
        self.assertIn('return CL_MOHAASoundPlaying( channel, name ) ? 1 : 0;', playing)
        callback = game[game.index('extern "C" void SV_MOHAASoundCallback('):]
        callback = callback[:callback.index('\n}\n')]
        # Retail's SoundCallback throws out of the module for a freed entity:
        # the object pointer is read before the call is made.
        self.assertIn('fntech3::mohaa::kGEntityObjectOffset', callback)
        self.assertLess(callback.index('if ( !object ) {'), callback.index('callback( entity, channel,'))
        self.assertIn('entity >= EntityCapacity()', callback)


if __name__ == '__main__':
    unittest.main()
