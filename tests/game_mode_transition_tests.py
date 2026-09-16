"""Execute the real mode controller and shutdown sequence with synthetic services."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from process_relaunch_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_manager.h"
#include "code/platform/process_relaunch.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
static const comGameProfile_t *active;
static bool com_fullyInitialized,com_errorEntered,runtimeReady,prepareFails,writeFails,pending;
static cvar_t dedicated;
static cvar_t *com_dedicated=&dedicated;
static unsigned markers,probes;
static int fullscreen;
static std::string base,home,command,diagnostic,shutdownMessage;
static std::vector<std::string> args,events;
const comGameProfile_t *FixtureProfile() { return active; }
unsigned FixtureMarkers(const char *path) { assert(base==path); ++probes; return markers; }
int FixtureReady(const comGameProfile_t *profile) {
 assert(profile && Com_GameProfileById(profile->id)==profile); return runtimeReady;
}
int Cmd_Argc() { return int(args.size()); }
const char *Cmd_Argv(int i) { return i>=0 && unsigned(i)<args.size() ? args[i].c_str() : ""; }
const char *Cmd_ArgsFrom(int i) { return Cmd_Argv(i); }
const char *Cvar_VariableString(const char *key) {
 if(!std::strcmp(key,"fs_basepath")) return base.c_str();
 assert(!std::strcmp(key,"fs_homepath")); return home.c_str();
}
int Cvar_VariableIntegerValue(const char *key) { assert(!std::strcmp(key,"r_fullscreen")); return fullscreen; }
void QDECL Com_Printf(const char *format,...) {
 char text[2048]; va_list ap; va_start(ap,format); vsnprintf(text,sizeof(text),format,ap); va_end(ap);
 diagnostic+=text;
}
int Sys_PrepareRelaunch(const char *value,char *error,unsigned size) {
 events.emplace_back("prepare");
 if(prepareFails || pending) { snprintf(error,size,"fixture cannot prepare"); return 0; }
 pending=true; command=value; return 1;
}
int Sys_HasPendingRelaunch() { return pending; }
void Sys_CancelRelaunch() { pending=false; events.emplace_back("cancel"); }
void Com_WriteConfiguration() {
 events.emplace_back("write");
 if(writeFails) { Sys_CancelRelaunch(); throw 77; }
}
void VM_Forced_Unload_Start() { events.emplace_back("forced-start"); }
void VM_Forced_Unload_Done() { events.emplace_back("forced-done"); }
void SV_Shutdown(const char *message) { shutdownMessage=message; events.emplace_back("server"); }
void CL_Shutdown(const char *message,qboolean quit) {
 assert(shutdownMessage==message && quit); events.emplace_back("client");
}
void Com_Shutdown() { events.emplace_back("common"); }
void FS_Shutdown(qboolean close) { assert(close); events.emplace_back("filesystem"); }
void NET_Shutdown() { events.emplace_back("network"); }
void Sys_Quit() { events.emplace_back("exit"); throw 42; }
'''

HARNESS = r'''
static void Reset() {
 active=Com_GameProfileById(COM_GAME_RTCW_SP);
 com_fullyInitialized=true; com_errorEntered=false; runtimeReady=true;
 prepareFails=writeFails=pending=false; dedicated={}; com_dedicated=&dedicated;
 markers=COM_GAME_MARKER_RTCW_SP|COM_GAME_MARKER_RTCW_MP; probes=0; fullscreen=0;
 base=u8"E:/Games/Wölf +set ignored 1;"; home="E:/test home/";
 command=diagnostic=shutdownMessage=""; args={"gameMode","rtcw-mp"}; events.clear();
}
static void NoTransition() {
 const auto *before=active;
 Com_GameMode_f(); assert(active==before && !pending && events.empty() && !diagnostic.empty());
}
int main() {
 for(auto title : {COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  const auto other=title==COM_GAME_RTCW_SP ? COM_GAME_RTCW_MP : COM_GAME_RTCW_SP;
  for(const bool alias : {false,true}) {
   Reset(); active=Com_GameProfileById(title);
   if(alias) args={other==COM_GAME_RTCW_MP ? "startMULTIPLAYER" : "startSINGLEPLAYER"};
   else args={"gameMode",Com_GameProfileById(other)->key};
   const auto *before=active;
   int result=0; try { Com_GameMode_f(); } catch(int code) { result=code; }
   assert(result==42 && active==before && pending && probes==1);
   assert(events==std::vector<std::string>({"prepare","write","forced-start","server","client",
     "forced-done","common","filesystem","network","exit"}));
   assert(shutdownMessage=="Switching game mode");
   const std::string expected=std::string("+set com_gameProfile \"")+Com_GameProfileById(other)->key+
     "\" +set fs_basepath \""+base+"\" +set fs_homepath \""+home+
     "\" +set fs_game \"\" +set r_fullscreen \"0\"";
   assert(command==expected);
  }
 }
 for(const auto current : {COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  for(const auto target : {COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
   if(current!=target && Com_GameProfileById(current)->titleId==Com_GameProfileById(target)->titleId) continue;
   Reset(); active=Com_GameProfileById(current); args={"gameMode",Com_GameProfileById(target)->key};
   NoTransition(); assert(!probes);
  }
 }
 for(const std::vector<std::string> request : {std::vector<std::string>{},
   {"gameMode"},{"gameMode","rtcw"},{"gameMode","unknown"},{"gameMode","rtcw-mp","extra"},
   {"startMultiplayer","extra"},{"startSingleplayer"}}) {
  Reset(); args=request; NoTransition(); assert(!probes);
 }
 for(auto current : {COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}) {
  Reset(); active=Com_GameProfileById(current); args={"startMultiplayer"}; NoTransition();
 }
 Reset(); active=nullptr; NoTransition();
 Reset(); com_fullyInitialized=false; NoTransition();
 Reset(); com_errorEntered=true; NoTransition();
 Reset(); com_dedicated=nullptr; NoTransition();
 Reset(); dedicated.integer=1; NoTransition();
 Reset(); runtimeReady=false; NoTransition(); assert(!probes);
 assert(diagnostic.find(Com_GameProfileById(COM_GAME_RTCW_MP)->runtimeBlocker)!=std::string::npos);
 for(unsigned evidence : {0u, unsigned(COM_GAME_MARKER_RTCW_SP),
   unsigned(COM_GAME_MARKER_RTCW_MP|COM_GAME_MARKER_QUAKE3),
   unsigned(COM_GAME_MARKER_RTCW_MP|COM_GAME_MARKER_UNRECOGNIZED_PAK)}) {
  Reset(); markers=evidence; NoTransition(); assert(probes==1);
 }
 for(const char *bad : {"", "a\"b", "a\nb"}) {
  Reset(); base=bad; NoTransition(); assert(!probes);
  Reset(); home=bad; NoTransition(); assert(!probes);
 }
 Reset(); base=std::string(1024,'x'); NoTransition(); assert(!probes);
 Reset(); prepareFails=true; Com_GameMode_f();
 assert(!pending && events==std::vector<std::string>{"prepare"});
 assert(diagnostic.find("fixture cannot prepare")!=std::string::npos);
 Reset(); writeFails=true;
 try { Com_GameMode_f(); assert(false); } catch(int code) { assert(code==77); }
 assert(!pending && events==std::vector<std::string>({"prepare","write","cancel"}));
 // An ordinary quit and an error exit keep the existing shutdown behavior;
 // neither may run the extra network handoff with no prepared child.
 Reset(); args={"quit","ordinary quit"};
 try { Com_Quit_f(); assert(false); } catch(int code) { assert(code==42); }
 assert(shutdownMessage=="ordinary quit" && !pending);
 assert(events==std::vector<std::string>({"forced-start","server","client","forced-done",
   "common","filesystem","exit"}));
 Reset(); pending=true; com_errorEntered=true;
 try { Com_Quit_f(); assert(false); } catch(int code) { assert(code==42); }
 assert(!pending && events==std::vector<std::string>({"cancel","exit"}));
 assert(!Com_GameProfile()); // No test has changed the process's real profile.
}
'''


class GameModeTransitionTests(unittest.TestCase):
    def test_controller_validation_and_shutdown_order(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        common = (ROOT / 'code/qcommon/common.c').read_text()
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
        source = PRELUDE + '\n' + function(shared, 'int Q_stricmpn(') + '\n' + function(shared, 'int Q_stricmp(')
        source += '\n' + function(common, 'void Com_Quit_f(')
        source += '\n#define Com_GameProfile FixtureProfile\n#define Com_ProbeGameProfileMarkers FixtureMarkers\n'
        source += '#define Com_GameManagerCanActivate FixtureReady\n' + function(common, 'static void Com_GameMode_f(')
        source += '\n#undef Com_GameProfile\n#undef Com_ProbeGameProfileMarkers\n#undef Com_GameManagerCanActivate\n' + HARNESS
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'mode.cpp'
            exe = Path(directory) / 'mode.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler, *target, *abi, '-std=c++17', '-I', str(ROOT),
                str(cpp), str(ROOT / 'code/qcommon/game_profile.cpp'), '-o', str(exe)],
                cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=15, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_platform_error_paths_cancel_and_exit_with_failure(self):
        common = (ROOT / 'code/qcommon/common.c').read_text()
        error = function(common, 'void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Error(')
        self.assertLess(error.index('Sys_CancelRelaunch();'), error.index('if ( com_errorEntered )'))
        for platform in ['win32/win_main.cpp', 'unix/unix_main.cpp']:
            source = (ROOT / 'code' / platform).read_text()
            error = function(source, 'void NORETURN FORMAT_PRINTF(1, 2) QDECL Sys_Error(')
            self.assertIn('Sys_CancelRelaunch();', error)
            quit = function(source, 'void NORETURN Sys_Quit(')
            self.assertIn('Sys_LaunchRelaunch(', quit)
            if platform.startswith('win32'):
                self.assertIn('sys_exitCode = 1;', error)
                self.assertIn('exit( sys_exitCode );', quit)
                self.assertIn('Sys_DecodeStartupCommandLine( GetCommandLineW()', source)
            else:
                self.assertLess(quit.index('Sys_ConsoleInputShutdown();'), quit.index('Sys_LaunchRelaunch('))
                self.assertIn('Sys_Exit( 1 );', quit)


if __name__ == '__main__':
    unittest.main()
