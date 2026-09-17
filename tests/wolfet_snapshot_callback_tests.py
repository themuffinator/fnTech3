"""Exercise checked ET callback reads and calls with real native entity layouts."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = r'''
#include "code/server/server.h"
#include "code/server/server_game_data.h"
#include <cassert>
#include <cstdarg>
static comGameId_t profile=COM_GAME_WOLFET;
int Com_GameIsWolfET(){return profile==COM_GAME_WOLFET;}
static fntech3::server::ServerGameDataView serverGameData;
static vm_t module{};
vm_t *gvm=&module;
struct Drop {};
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
static int calls,lastEntity,lastClient;
static intptr_t reply;
intptr_t QDECL VM_Call(vm_t *vm,int count,int command,...){
 assert(vm==&module&&count==2&&command==GAME_SNAPSHOT_CALLBACK);
 va_list args;va_start(args,command);lastEntity=va_arg(args,int);lastClient=va_arg(args,int);va_end(args);
 ++calls;return reply;
}
template<class F> void rejected(F f){bool threw=false;try{f();}catch(Drop&){threw=true;}assert(threw);}
'''
CHECKS = r'''
int main(){
 struct Slot{fntech3_wolfet_sharedEntity_t record;int sentinel=12345;} entities[3]{};
 fntech3_wolfet_playerState_t players[2]{};
 fntech3::server::NativeGameDataBinding binding{entities,sizeof(entities),3,sizeof(Slot),
  players,sizeof(players),2,sizeof(players[0])};
 assert(serverGameData.BindNative(COM_GAME_WOLFET,binding)==fntech3::server::GameDataBindError::None);
 assert(SV_WolfETSnapshotCallback(2,1)&&calls==0);
 entities[2].record.r.snapshotCallback=1;
 reply=0;assert(!SV_WolfETSnapshotCallback(2,1)&&calls==1&&lastEntity==2&&lastClient==1);
 reply=1;assert(SV_WolfETSnapshotCallback(2,0)&&calls==2&&lastClient==0);
 entities[2].record.r.snapshotCallback=0;
 assert(SV_WolfETSnapshotCallback(2,0)&&calls==2); // Module may change the flag each frame.
 for(int invalid:{-1,3,INT_MAX})rejected([&]{SV_WolfETSnapshotCallback(invalid,0);});
 for(int invalid:{-1,2,INT_MAX})rejected([&]{SV_WolfETSnapshotCallback(2,invalid);});
 assert(calls==2);
 entities[2].record.r.snapshotCallback=1;gvm=nullptr;
 rejected([&]{SV_WolfETSnapshotCallback(2,0);});gvm=&module;
 serverGameData.Invalidate();rejected([&]{SV_WolfETSnapshotCallback(2,0);});
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  profile=id;assert(SV_WolfETSnapshotCallback(-1,-1)&&calls==2);
 }
 for(const auto &entity:entities)assert(entity.sentinel==12345);
}
'''


class WolfETSnapshotCallbackTests(unittest.TestCase):
    def test_callback_layout_lifetime_bounds_and_profile_isolation(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        text = (ROOT/'code/server/sv_game.cpp').read_text(encoding='utf-8')
        begin = text.index('bool SV_WolfETSnapshotCallback(')
        body = text[begin:text.index('\n}',begin)+2]
        scratch = ROOT/'.tmp/wolfet-next'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'callback.cpp'
            executable = folder/('callback.exe' if os.name == 'nt' else 'callback')
            source.write_text(HOST+body+CHECKS,encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler,*target,'-std=c++17','-I',str(ROOT),
                str(source),str(ROOT/'code/server/server_game_data.cpp'),'-x','c++',
                str(ROOT/'code/compat/game_abi_adapter.c'),'-o',str(executable)],
                cwd=ROOT,capture_output=True,text=True,timeout=90)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run = subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__ == '__main__':
    unittest.main()
