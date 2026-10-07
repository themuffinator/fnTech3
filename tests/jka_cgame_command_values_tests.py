"""Execute the real command finisher with authored state; no physical input."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


class JKACgameCommandValuesTests(unittest.TestCase):
    def test_real_finisher_publishes_only_the_selected_profile_and_abi_bytes(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/client/input_compat.hpp"
#include "code/client/client_jka_command_values.hpp"
#include "code/compat/jka_state_adapter.hpp"
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
static comGameId_t selected;
int Com_GameIsRTCW(){return selected==COM_GAME_RTCW_SP||selected==COM_GAME_RTCW_MP;}
int Com_GameIsWolfET(){return selected==COM_GAME_WOLFET;}
int Com_GameIsJKAMP(){return selected==COM_GAME_JKA_MP;}
static struct {
 int cgameUserCmdValue, cgameUserCmdPrimary, cgameUserCmdFov, serverTime;
 float viewangles[3];
 rtcwUsercmdExtension_t cgameRTCWCommand;
 struct {unsigned char flags,identClient;} cgameWolfETCommand;
} cl;
''' + function((ROOT / 'code/client/cl_input.cpp').read_text(), 'static void CL_FinishMove( usercmd_t *cmd ) {') + r'''
int main(){
 using fntech3::jka::command::selections;
 cl.cgameUserCmdValue=257;cl.cgameUserCmdPrimary=41;cl.cgameUserCmdFov=93;cl.serverTime=1001;
 cl.cgameRTCWCommand.holdable=3;cl.cgameRTCWCommand.cld=777;
 cl.cgameRTCWCommand.mpSetup=5;cl.cgameRTCWCommand.identClient=6;
 cl.cgameWolfETCommand.flags=7;cl.cgameWolfETCommand.identClient=8;
 for(int force:{INT_MIN,-257,-1,0,1,255,256,257,INT_MAX}){
  for(int inventory:{INT_MIN,-257,-1,0,1,255,256,257,INT_MAX}){
   selections.Set(force,inventory);
   for(unsigned id=COM_GAME_QUAKE3;id<COM_GAME_COUNT;++id){
    selected=static_cast<comGameId_t>(id);
    usercmd_t command{};command.raven.forcesel=11;command.raven.invensel=12;
    command.raven.generic_cmd=77;command.buttons=1234;
    command.forwardmove=3;command.rightmove=-4;command.upmove=5;
    CL_FinishMove(&command);
    assert(command.weapon==1&&command.weaponPrimary==41&&command.fov==93&&command.serverTime==1001);
    assert(command.buttons==1234&&command.forwardmove==3&&command.rightmove==-4&&command.upmove==5);
    assert(command.raven.generic_cmd==77);
    if(selected==COM_GAME_JKA_MP){
     assert(command.raven.forcesel==static_cast<unsigned char>(force));
     assert(command.raven.invensel==static_cast<unsigned char>(inventory));
     fntech3_jka_usercmd_t output{};assert(fntech3::jka::state::FromInternal(command,output));
     std::array<byte,sizeof(output)+8> bytes;bytes.fill(0xa5);
     std::memcpy(bytes.data()+3,&output,sizeof(output));
     assert(bytes[3+20]==1&&bytes[3+21]==static_cast<unsigned char>(force));
     assert(bytes[3+22]==static_cast<unsigned char>(inventory)&&bytes[3+23]==77);
     assert(bytes[2]==0xa5&&bytes[3+sizeof(output)]==0xa5);
    }else assert(command.raven.forcesel==11&&command.raven.invensel==12);
    if(Com_GameIsRTCW())assert(command.rtcw.holdable==3&&command.rtcw.cld==777&&command.rtcw.mpSetup==5&&command.rtcw.identClient==6);
    else assert(command.rtcw.holdable==0&&command.rtcw.cld==0&&command.rtcw.mpSetup==0&&command.rtcw.identClient==0);
    if(Com_GameIsWolfET())assert(command.wolfet.flags==7&&command.wolfet.identClient==8);
    else assert(command.wolfet.flags==0&&command.wolfet.identClient==0);
   }
  }
 }
 selections.Reset();usercmd_t empty{};empty.raven.generic_cmd=99;selections.Publish(empty);
 assert(!empty.raven.forcesel&&!empty.raven.invensel&&empty.raven.generic_cmd==99);
}
'''
        target = compiler_target_args(compiler)
        parent = ROOT / '.tmp/tests/jka-cgame-command-values'
        parent.mkdir(parents=True, exist_ok=True)
        self.assertTrue(parent.resolve().is_relative_to(ROOT.resolve()))
        with tempfile.TemporaryDirectory(dir=parent) as directory:
            cpp = Path(directory) / 'command.cpp'
            cpp.write_text(source)
            for flags in ([], ['-O2', '-ffast-math']):
                # One executable per build: Windows can hold the last one run.
                exe = Path(directory) / ('command-fast.exe' if flags else 'command.exe')
                built = subprocess.run([compiler, *target, '-std=c++17', *flags, '-I', str(ROOT),
                    str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                ran = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=20, **options)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertTrue(Path(directory).resolve().is_relative_to(parent.resolve()))
            self.assertFalse(any(path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & 0x400
                for path in Path(directory).rglob('*')))


if __name__ == '__main__':
    unittest.main()
