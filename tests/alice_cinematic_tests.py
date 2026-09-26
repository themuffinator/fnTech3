"""American McGee's Alice's cinematic player and the intro it continues.

Retail alice.exe: `cinematic` 0x004276c0, CIN_Play 0x004272e0, CIN_Finish
0x004257a0, CIN_Stop 0x00425770, _intro 0x0040fc70. The steps live in
code/client/client_alice_cinematic.hpp as a template over an engine binding;
the probe below drives them with a recording host, and the source checks pin
the binding in cl_cin.cpp and the engine UI's use of it.
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

PROBE = r'''
#include "code/client/client_alice_cinematic.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace fntech3::alice::cinematic;

struct Host {
 std::vector<std::string> trace;
 bool playing=false, loading=false, preloads=true, opens=true, sounds=true;
 int length=1000;
 std::string nextmap;
 int introState=-9999;
 bool Playing() const { return playing; }
 void StartHunkUsers() { trace.push_back("hunk"); }
 void StopAllSounds() { trace.push_back("stopsounds"); }
 int VideoLength(const char *path) { trace.push_back(std::string("length ")+path); return length; }
 void MissingVideo(const char *name) { trace.push_back(std::string("missing ")+name); }
 void ShutdownServer() { trace.push_back("svshutdown"); }
 bool PreloadSound(const char *path) { trace.push_back(std::string("preload ")+path); return preloads; }
 void SoundNotPreloaded(const char *path) { trace.push_back(std::string("nopreload ")+path); }
 bool Open(const char *path, bool hold) {
  trace.push_back(std::string("open ")+path+(hold?" hold":""));
  if (opens) playing=true;
  return opens;
 }
 void BadHeader(const char *name) { trace.push_back(std::string("badheader ")+name); }
 void Opened(const char *name) { trace.push_back(std::string("playing ")+name); }
 void ClearSoundBuffer() { trace.push_back("clearsound"); }
 void DecodeFirstFrame() { trace.push_back("frame"); }
 bool PlaySound() { trace.push_back("playsound"); return sounds; }
 void SoundNotPlayed() { trace.push_back("nosound"); }
 std::string NextMap() const { return nextmap; }
 void Append(const char *text) { trace.push_back(std::string("append ")+text); }
 void ClearNextMap() { trace.push_back("clearnextmap"); nextmap.clear(); }
 bool MapLoading() const { return loading; }
 void SetIntroState(int state) { trace.push_back("introstate "+std::to_string(state)); introState=state; }
};

static void Same(const std::vector<std::string> &got, const std::vector<std::string> &want) {
 if (got != want) {
  for (const auto &line : got) std::fprintf(stderr, "got: %s\n", line.c_str());
  for (const auto &line : want) std::fprintf(stderr, "want: %s\n", line.c_str());
 }
 assert(got == want);
}

int main() {
 // Flags: '1' holds, as do demoend.roq and end.roq in any case; '2' and one
 // more character resume the intro at that character less '0', unchecked.
 PlayFlags f = ParsePlayFlags("ealogo.roq", "25");
 assert(!f.holdAtEnd && f.resumesIntro && f.introState == 5);
 f = ParsePlayFlags("roguelogo.roq", "26"); assert(f.resumesIntro && f.introState == 6);
 f = ParsePlayFlags("opening.roq", ""); assert(!f.holdAtEnd && !f.resumesIntro);
 f = ParsePlayFlags("opening.roq", nullptr); assert(!f.holdAtEnd && !f.resumesIntro);
 f = ParsePlayFlags("x.roq", "1"); assert(f.holdAtEnd && !f.resumesIntro);
 f = ParsePlayFlags("x.roq", "12"); assert(f.holdAtEnd && !f.resumesIntro);
 f = ParsePlayFlags("x.roq", "21"); assert(!f.holdAtEnd && f.resumesIntro && f.introState == 1);
 f = ParsePlayFlags("x.roq", "2"); assert(f.resumesIntro && f.introState == -48);
 f = ParsePlayFlags("x.roq", "2x"); assert(f.resumesIntro && f.introState == 'x' - '0');
 f = ParsePlayFlags("x.roq", "2\xff"); assert(f.resumesIntro && f.introState == -1 - '0');
 f = ParsePlayFlags("x.roq", "3"); assert(!f.holdAtEnd && !f.resumesIntro);
 f = ParsePlayFlags("x.roq", " 25"); assert(!f.resumesIntro);
 assert(ParsePlayFlags("DemoEnd.ROQ", "").holdAtEnd && ParsePlayFlags("end.roq", nullptr).holdAtEnd);
 assert(!ParsePlayFlags("theend.roq", "").holdAtEnd && !ParsePlayFlags(nullptr, "").holdAtEnd);

 // Paths: always under video/, cut to 127 characters; the sound's name ends
 // at the first '.'.
 assert(VideoPath("opening.roq") == "video/opening.roq");
 assert(VideoPath(std::string(200, 'a').c_str()).size() == 127);
 assert(SoundPath("video/opening.roq") == "video/opening.mp3");
 assert(SoundPath("video/a.b.roq") == "video/a.mp3");
 assert(SoundPath("video/plain") == "video/plain.mp3");

 // A logo, in retail's order, with its resume state recorded.
 {
  Host host; Record record;
  Play(host, record, "ealogo.roq", "25");
  Same(host.trace, {"hunk", "stopsounds", "length video/ealogo.roq", "svshutdown",
   "preload video/ealogo.mp3", "open video/ealogo.roq", "playing ealogo.roq",
   "clearsound", "frame", "playsound"});
  assert(record.resumesIntro && record.introState == 5);
  // One is playing: a second request is ignored outright, and the first
  // keeps its record.
  host.trace.clear();
  Play(host, record, "opening.roq", "");
  assert(host.trace.empty() && record.resumesIntro && record.introState == 5);
  // Its end: no nextmap, so the intro resumes at 5.
  host.playing = false; host.trace.clear();
  End(host, record);
  Same(host.trace, {"introstate 5", "append _intro\n"});
  assert(!record.resumesIntro);
 }
 // New Game: the opening ends into " map gvillage", then the intro's state 100.
 {
  Host host; Record record; host.nextmap = "gvillage";
  Play(host, record, "opening.roq", "");
  assert(!record.resumesIntro);
  host.playing = false; host.trace.clear();
  End(host, record);
  Same(host.trace, {"append  map gvillage\n", "clearnextmap", "introstate 100", "append _intro\n"});
 }
 // A nextmap beginning with ';' runs as " map ;..." and ends there.
 {
  Host host; Record record; host.nextmap = ";forcemenu main;forcemenu credits\n";
  record.resumesIntro = true; record.introState = 6;
  End(host, record);
  Same(host.trace, {"append  map ;forcemenu main;forcemenu credits\n\n", "clearnextmap"});
  assert(host.introState == -9999);
 }
 // A missing movie is a developer print and nothing else.
 for (int length : {0, -1}) {
  Host host; Record record; host.length = length; host.nextmap = "gvillage";
  record.resumesIntro = true; record.introState = 5;
  Play(host, record, "opening.roq", "");
  Same(host.trace, {"hunk", "stopsounds", "length video/opening.roq", "missing opening.roq"});
  assert(!record.resumesIntro && host.nextmap == "gvillage");
 }
 // A bad header ends nothing: the server is already down and the sound was
 // asked for, but no nextmap runs and the intro is not continued.
 {
  Host host; Record record; host.opens = false; host.nextmap = "gvillage";
  Play(host, record, "roguelogo.roq", "26");
  Same(host.trace, {"hunk", "stopsounds", "length video/roguelogo.roq", "svshutdown",
   "preload video/roguelogo.mp3", "open video/roguelogo.roq", "badheader roguelogo.roq"});
  assert(!record.resumesIntro && host.nextmap == "gvillage" && !host.playing);
 }
 // Sound failures are reported and the movie plays regardless.
 {
  Host host; Record record; host.preloads = false; host.sounds = false;
  Play(host, record, "end.roq", "");
  Same(host.trace, {"hunk", "stopsounds", "length video/end.roq", "svshutdown",
   "preload video/end.mp3", "nopreload video/end.mp3", "open video/end.roq hold",
   "playing end.roq", "clearsound", "frame", "playsound", "nosound"});
 }
 // CIN_Stop: nothing unless one plays; a stopped logo goes to the main menu.
 {
  Record record; record.resumesIntro = true; record.introState = 5;
  assert(!Stop(false, record) && record.resumesIntro);
  assert(Stop(true, record) && !record.resumesIntro);
  Host host; End(host, record);
  Same(host.trace, {"introstate 100", "append _intro\n"});
 }
 // Stopped from inside a map load, the load's -1 stands and the intro step
 // still follows the map, as retail's earlier stop leaves them.
 {
  Host host; Record record; host.loading = true;
  record.resumesIntro = true; record.introState = 6;
  Stop(true, record); End(host, record);
  Same(host.trace, {"append _intro\n"});
 }
 return 0;
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


class AliceCinematicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cin = (ROOT / 'code/client/cl_cin.cpp').read_text(encoding='utf-8')
        cls.ui = (ROOT / 'code/client/cl_alice_ui.cpp').read_text(encoding='utf-8')
        cls.header = (ROOT / 'code/client/client_alice_cinematic.hpp').read_text(encoding='utf-8')

    def test_driver_order_flags_end_and_stop(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='alice-cinematic-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', '-Wall', '-Werror',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_header_stands_alone(self):
        includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', self.header)
        self.assertEqual(sorted(includes), ['cstddef', 'string'])

    def test_command_dispatches_before_the_shared_path(self):
        command = function(self.cin, 'void CL_PlayCinematic_f( void ) {')
        alice = command.index('if ( Com_GameIsRitual() ) {')
        # Ignored while one plays (0x004276c0), then CIN_Play(argv 1, argv 2).
        self.assertLess(alice, command.index('if ( cls.state == CA_CINEMATIC ||'))
        self.assertLess(alice, command.index('CL_ShouldSkipIdLogoCinematic'))
        branch = command[alice:command.index('return;', alice)]
        self.assertIn('cls.state != CA_CINEMATIC', branch)
        self.assertIn('SCR_PlayCinematic( %s )', branch)
        self.assertIn('CL_AliceCinematicPlay( arg, s );', branch)
        # The shared path keeps Q3's loop flag; Alice's never reaches it.
        self.assertIn('bits |= CIN_loop;', command[alice:])

    def test_binding_never_loops_and_ends_through_the_driver(self):
        host = self.cin[self.cin.index('struct AliceCinematicHost {'):self.cin.index('\n};', self.cin.index('struct AliceCinematicHost {'))]
        self.assertNotIn('CIN_loop', host)
        self.assertIn('CIN_system | ( holdAtEnd ? CIN_hold : 0 )', host)
        self.assertIn('SV_Shutdown( "" )', host)
        self.assertIn('FS_FOpenFileRead( path, nullptr, qfalse )', host)
        self.assertIn('CL_AliceUISetIntroState( state )', host)
        self.assertIn('Cbuf_ExecuteText( EXEC_APPEND, text )', host)
        # The audio slice's MP3 preload, play and free stay with the movie.
        for call in ('S_AliceMP3Preload', 'S_AliceMP3PlayPreloaded( qfalse )', 'S_AliceMusicFreeAll'):
            self.assertIn(call, host)
        nextmap = function(self.cin, 'static void CL_RunCinematicNextmap( void )')
        self.assertLess(nextmap.index('CL_AliceCinematicEnd();'), nextmap.index('Cvar_VariableString( "nextmap" )'))
        stop = function(self.cin, 'void SCR_StopCinematic( void ) {')
        self.assertLess(stop.index('fntech3::alice::cinematic::Stop( cls.state == CA_CINEMATIC, aliceCinematic );'),
                        stop.index('CIN_StopCinematic(CL_handle);'))

    def test_intro_plays_logos_directly_and_lets_their_end_continue_it(self):
        step = function(self.ui, 'void RunIntroStep() {')
        video = step[step.index('case screen::IntroAction::Video:'):step.index('case screen::IntroAction::MainMenu:')]
        self.assertIn('Cvar_Set( "nextmap", "" );', video)
        self.assertLess(video.index('Cvar_Set( "nextmap", "" );'),
                        video.index('CL_AliceCinematicPlay( step.video, step.videoFlags );'))
        self.assertLess(video.index('CL_AliceCinematicPlay( step.video, step.videoFlags );'),
                        video.index('++introState;'))
        self.assertNotIn('ScheduleIntro', video)
        self.assertNotIn('EXEC_NOW', self.ui)
        self.assertNotIn('introVideoResume', self.ui)
        setter = function(self.ui, 'void CL_AliceUISetIntroState( int state ) {')
        self.assertIn('introState = state;', setter)
        self.assertNotIn('_intro', setter)
        soundtrack = step[step.index('case screen::IntroAction::Soundtrack:'):]
        self.assertIn('S_AliceMusicNewSoundtrack( CL_RitualMenuSoundtrack() );', soundtrack)
        # Alice's menu soundtrack is still its intro; F.A.K.K.2 names its own.
        font = (ROOT / 'code/client/cl_alice_font.cpp').read_text(encoding='utf-8')
        self.assertIn(': fntech3::alice::screen::IntroSoundtrack;',
                      function(font, 'const char *CL_RitualMenuSoundtrack( void ) {'))
        # The `_intro` a movie appends behind its map runs while this client
        # has its hosted UI down for the load; the command must outlive that.
        removed = self.ui[self.ui.index('const char *const kCommands[] = {'):]
        removed = removed[:removed.index('};')]
        self.assertNotIn('"_intro"', removed)
        self.assertNotIn('"dointro"', removed)
        register = function(self.ui, 'void RegisterCommands() {')
        self.assertLess(register.index('if ( !introCommandsRegistered ) {'),
                        register.index('Cmd_AddCommand( "_intro", Intro_f );'))
        menu = step[step.index('case screen::IntroAction::MainMenu:'):step.index('case screen::IntroAction::Soundtrack:')]
        self.assertIn('savedStack.assign( 1, "main" );', menu)

    def test_ui_update_gives_the_intro_up(self):
        frame = function(self.ui, 'void CL_AliceUIFrame( void ) {')
        self.assertIn('if ( !CL_AliceLoadingActive() && ( CL_AliceUIFullscreenMenuUp() || cls.state == CA_ACTIVE ) ) {\n'
                      '\t\tintroState = -1;', frame)
        # A movie with no menu up leaves the keys free for the any-key Escape.
        want = frame[frame.index('const bool wantKeys'):frame.index(';', frame.index('const bool wantKeys'))]
        self.assertNotIn('CA_CINEMATIC', want)


if __name__ == '__main__':
    unittest.main()
