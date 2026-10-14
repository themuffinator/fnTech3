"""Medal of Honor: the `fadesound` server command.

Retail 1.00's game sends `fadesound <seconds times 1000>` when a level ends
or restarts and `fadesound 0.00` where a fade must end (gamex86.dll
0x310e38e0, 0x310e3910, 0x310e3a10); its client game has no handler for it,
so before this the command reached the client game and was printed as
unknown. The engine now takes it, as OpenMoHAA's engine does [O] and as
alice.exe, its parent engine, does at 0x004241f6: the whole mix falls in a
straight line to silence over that much real time, a command from the server
the client has left is dropped, and a new gamestate, a disconnect or a client
flush ends the fade. The probes compile the production fade, its engine entry,
the client game's command reader and both mixers against controlled hosts;
no retail file is read."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_mix_fixture import mixer
from rtcw_music_fixture import function, openal

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text()


def body(text, signature):
    first = text.index(signature)
    return text[first:text.index('\n}\n', first) + 3]


class MohaaSoundFadeTests(unittest.TestCase):
    def build_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'fade.cpp'
            exe = Path(directory) / 'fade.exe'
            cpp.write_text(source)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                     str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return result.stdout

    def test_fade_falls_in_a_straight_line_and_holds_silence_until_cancelled(self):
        self.build_and_run(r'''
#include "code/client/audio/shared/MOHAAFade.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <limits>
using fntech3::audio::mohaa::Fade;
int main() {
	Fade fade;
	assert( !fade.Active() && fade.Gain( 0 ) == 1.0f && fade.Gain( 123456 ) == 1.0f );
	/* `fadesound 4000.00` at 10 s: full at its start, three quarters a
	 * second in, silent at four seconds and for as long as nothing cancels. */
	fade.Start( 4000.0f, 10000 );
	assert( fade.Active() );
	assert( fade.Gain( 10000 ) == 1.0f );
	assert( fade.Gain( 11000 ) == 0.75f );
	assert( fade.Gain( 12000 ) == 0.5f );
	assert( fade.Gain( 14000 ) == 0.0f );
	assert( fade.Gain( 600000 ) == 0.0f );
	/* A clock read before the start does not raise the mix past full. */
	assert( fade.Gain( 9000 ) == 1.0f );
	/* A new fade starts again from full. */
	fade.Start( 100.0f, 20000 );
	assert( fade.Gain( 20050 ) == 0.5f && fade.Gain( 20100 ) == 0.0f );
	/* `fadesound 0.00`, a negative time and anything non-finite cancel. */
	for ( float cancel : { 0.0f, -5.0f, std::numeric_limits<float>::quiet_NaN(),
			std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() } ) {
		fade.Start( 4000.0f, 0 );
		fade.Start( cancel, 1000 );
		assert( !fade.Active() && fade.Gain( 1000 ) == 1.0f && fade.Gain( 99999 ) == 1.0f );
	}
	/* A fade far shorter than a frame is silent by the next frame. */
	fade.Start( 0.01f, 5000 );
	assert( fade.Gain( 5016 ) == 0.0f );
	return 0;
}
''')

    def test_the_engine_takes_fadesound_before_the_client_game(self):
        reader = body(read('code/client/cl_mohaa_cgame.cpp'),
                      'qboolean QDECL GetServerCommand( int serverCommandNumber, qboolean differentServer ) {')
        self.build_and_run(r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
typedef int qboolean;
enum { qfalse, qtrue };
#define QDECL
static std::vector<std::string> args;
static bool accessorResult = true;
static int asked = -1;
static qboolean CL_GetServerCommandForProfile( int number ) { asked = number; return accessorResult ? qtrue : qfalse; }
static const char *Cmd_Argv( int index ) {
	return index >= 0 && index < static_cast<int>( args.size() ) ? args[static_cast<std::size_t>( index )].c_str() : "";
}
static std::vector<float> fades;
static void S_MOHAAFadeSound( float ms ) { fades.push_back( ms ); }
''' + reader + r'''
static qboolean Read( std::vector<std::string> command, qboolean differentServer, int number = 7 ) {
	args = command;
	return GetServerCommand( number, differentServer );
}
int main() {
	/* The fade is the engine's: the client game never sees the command. */
	assert( Read( { "fadesound", "4000.00" }, qfalse ) == qfalse && asked == 7 );
	assert( fades.size() == 1 && fades[0] == 4000.0f );
	assert( Read( { "fadesound", "0.00" }, qfalse ) == qfalse );
	assert( fades.size() == 2 && fades[1] == 0.0f );
	/* No time reads as 0, which ends a fade. */
	assert( Read( { "fadesound" }, qfalse ) == qfalse && fades.size() == 3 && fades[2] == 0.0f );
	/* One from the server the client has left is dropped, and still
	 * swallowed, so a level's closing fade cannot silence the next. */
	assert( Read( { "fadesound", "4000.00" }, qtrue ) == qfalse && fades.size() == 3 );
	/* Every other command, and a name that only begins the same, reaches
	 * the client game as before. */
	assert( Read( { "print", "hello" }, qfalse ) == qtrue );
	assert( Read( { "fadesounds", "1" }, qfalse ) == qtrue );
	assert( Read( { "FADESOUND", "1" }, qfalse ) == qtrue );
	assert( Read( { "cs", "5", "x" }, qtrue ) == qtrue );
	assert( fades.size() == 3 );
	/* A command the accessor refuses is not read again from stale tokens. */
	accessorResult = false;
	assert( Read( { "fadesound", "4000.00" }, qfalse, 9 ) == qfalse && asked == 9 && fades.size() == 3 );
	return 0;
}
''')

    def test_the_engine_fade_is_medal_of_honor_s_and_runs_on_real_time(self):
        engine = read('code/client/audio/snd_main.cpp')
        first = engine.index('static fntech3::audio::mohaa::Fade mohaaFade;')
        entry = body(engine[first:], 'extern "C" float S_MOHAAFadeGain( void )')
        entry = engine[first:engine.index(entry, first) + len(entry)]
        self.build_and_run(r'''
#include "code/client/audio/shared/MOHAAFade.h"
#include <cassert>
static bool mohaa = false;
static int Com_GameIsMOHAA() { return mohaa ? 1 : 0; }
static struct { int realtime; } cls;
static int printed = 0;
static void Com_DPrintf( const char *, ... ) { ++printed; }
''' + entry + r'''
int main() {
	/* Another profile neither starts a fade nor reads one. */
	cls.realtime = 1000;
	S_MOHAAFadeSound( 4000.0f );
	assert( S_MOHAAFadeGain() == 1.0f && printed == 0 );
	mohaa = true;
	assert( S_MOHAAFadeGain() == 1.0f );
	/* Cancelling no fade prints nothing. */
	S_MOHAAFadeSound( 0.0f );
	assert( printed == 0 );
	S_MOHAAFadeSound( 4000.0f );
	assert( printed == 1 && S_MOHAAFadeGain() == 1.0f );
	cls.realtime = 3000;
	assert( S_MOHAAFadeGain() == 0.5f );
	cls.realtime = 6000;
	assert( S_MOHAAFadeGain() == 0.0f );
	S_MOHAAFadeSound( 0.0f );
	assert( printed == 2 && S_MOHAAFadeGain() == 1.0f );
	return 0;
}
''')

    def test_openal_scales_its_listener_by_the_fade(self):
        harness = openal()
        prefix = harness[:harness.index('int main() {')]
        self.build_and_run(prefix + r'''
int main() {
    AudioSystem system;
    isSP=false;system.hardMuted_=false;effectsVolume.value=0.5f;
    mohaaFade=1;system.Update(16);assert(system.device_.gain==0.5f);
    mohaaFade=0.25f;system.Update(16);assert(system.device_.gain==0.125f);
    mohaaFade=0;system.Update(16);assert(system.device_.gain==0);
    mohaaFade=1;system.Update(16);assert(system.device_.gain==0.5f);
    /* RTCW SP keeps its own master path, whatever the gain reads. */
    isSP=true;mohaaFade=0.25f;system.Update(16);assert(system.device_.gain==1);
    return 0;
}
''')

    def test_the_software_mixer_fades_channels_and_streams_alike(self):
        harness = mixer()
        prefix = harness[:harness.index('int main(){')]
        self.build_and_run(prefix + r'''
int main(){
 sndBuffer one;one.sndChunk[0]=1000;one.sndChunk[1]=-2000;one.sndChunk[2]=3000;
 sfx_t oneSfx{&one,3,1,0};
 isSP=false;effectsVolume.value=1;gain=1;voiceSample={128,-128};numLoopChannels=0;
 for(float fade:{1.f,0.75f,0.5f,0.25f,0.f}){
  mohaaFade=fade;
  s_channels[0]={255,255,100,false,1,1,&oneSfx};
  s_paintedtime=100;s_rawend=103;
  for(int i=100;i<103;++i)s_rawsamples[i&(MAX_RAW_SAMPLES-1)]={4096,-8192};
  rendered.clear();S_PaintChannels(103);assert(rendered.size()==3);
  // The channels through their volume, the music ring and voice where they
  // are copied: nothing escapes the fade.
  const int volume=fade<1?int(float(255)*fade):255;
  assert(snd_vol==volume);
  for(int i=0;i<3;++i){
   assert(rendered[i].left==int(float(4096+128)*fade)+effect(one.sndChunk[i],255*volume));
   assert(rendered[i].right==int(float(-8192-128)*fade)+effect(one.sndChunk[i],255*volume));
  }
 }
 return 0;
}
''')

    def test_level_changes_disconnects_and_flushes_end_the_fade(self):
        main = read('code/client/cl_main.cpp')
        for signature in ('void CL_ClearState( void ) {', 'void CL_ShutdownAll( void ) {'):
            self.assertIn('S_MOHAAFadeSound( 0.0f );', function(main, signature), signature)
        parse = read('code/client/cl_parse.cpp')
        self.assertIn('CL_ClearState();', function(parse, 'void CL_ParseGamestate( msg_t *msg ) {'))
        self.assertIn('CL_ClearState();', function(main, 'qboolean CL_Disconnect( qboolean showMainMenu ) {'))
        # `s_info` reports the fade under Medal of Honor in both backends.
        for path in ('code/client/audio/openal/AudioSystemBackend.inl', 'code/client/audio/legacy/snd_dma.cpp'):
            self.assertIn('Com_Printf( "fadesound gain: %.2f\\n", S_MOHAAFadeGain() );', read(path), path)


if __name__ == '__main__':
    unittest.main()
