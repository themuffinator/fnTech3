"""Execute SP command recovery against native observations and malformed packets."""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from spawn_view_angle_source_tests import function_body
import server_user_move_tests as shared

ROOT = Path(__file__).resolve().parents[1]

CALLBACK = r'''
constexpr int GAME_CLIENT_THINK=7;
static void *gvm=nullptr;
static client_t *current=nullptr;
static bool mutate=false;
static int dropAt=0;
static std::vector<std::array<int,3>> calls;
// Medal of Honor's moves go to its own reader; no client here is on its wire.
void SV_MOHAAUserMove(client_t*,msg_t*,qboolean){CHECK(false);}
int SV_ClientIndex(const client_t *client){CHECK(client==current);return 0;}
void VM_Call(void*,int arguments,int operation,int index,...){
 CHECK(operation==GAME_CLIENT_THINK&&index==0);
 ++current->thought;
 usercmd_t *command=&current->lastUsercmd;
 if(Com_GameIsSTVEFSP()){
  CHECK(arguments==2);va_list ap;va_start(ap,index);
  command=reinterpret_cast<usercmd_t*>(va_arg(ap,intptr_t));va_end(ap);
  CHECK(command!=&current->lastUsercmd);
 }else CHECK(arguments==1);
 CHECK(command->serverTime==current->lastUsercmd.serverTime);
 calls.push_back({command->serverTime,command->weapon,current->stvefSpCommandNumber});
 if(mutate)command->serverTime+=17;
 if(dropAt&&int(calls.size())==dropAt)current->state=CS_ZOMBIE;
}
'''

CHECKS = r'''
struct Case{int last,newest,count,mode,mutate,final,dropped,lastTime;std::vector<std::array<int,3>> calls;};
static const std::vector<Case> nativeCases={@CASES@};
static void Reset(client_t &client){current=&client;calls.clear();dropAt=0;mutate=false;}
int main(){
 for(const auto &item:nativeCases){
  client_t client{};Reset(client);client.stvefSpCommandNumber=item.last;
  client.lastUsercmd.serverTime=987654321;mutate=item.mutate;
  std::array<usercmd_t,32> commands{};
  for(int i=0;i<item.count;++i)commands[i]={item.mode==0?0:(item.mode==1?100+i:100-i),i+1};
  SV_ExecuteSTVEFSPCommands(&client,commands.data(),item.count,item.newest);
  CHECK(calls==item.calls);CHECK(client.stvefSpCommandNumber==item.final);
  CHECK(client.stvefSpDroppedCommands==bool(item.dropped));
  CHECK(client.lastUsercmd.serverTime==item.lastTime);
 }
 // Full decoder: no command executes or enters the world after a partial record.
 for(int state:{CS_PRIMED,CS_ACTIVE})for(int bad=0;bad<3;++bad){
  client_t client{};Reset(client);client.state=state;
  msg_t packet{};packet.newest=3;packet.failAt=bad;
  SV_UserMove(&client,&packet,true);
  CHECK(client.dropped==1&&client.entered==0&&calls.empty());
  CHECK(client.stvefSpCommandNumber==0&&!client.stvefSpDroppedCommands);
 }
 // Initial commands may all have time zero, and the first command really runs.
 client_t client{};Reset(client);client.state=CS_PRIMED;
 msg_t packet{};packet.newest=3;packet.mode=0;
 SV_UserMove(&client,&packet,true);
 CHECK(client.entered==1&&calls.size()==3&&client.stvefSpCommandNumber==3);
 calls.clear();packet.decoded=0;SV_UserMove(&client,&packet,false);
 CHECK(calls.empty()&&client.stvefSpCommandNumber==3);
 packet.newest=4;packet.decoded=0;SV_UserMove(&client,&packet,true);
 CHECK(calls.size()==1&&client.stvefSpCommandNumber==4);
 // Rewinding a command time after a save does not make its new ordinal stale.
 calls.clear();packet.newest=5;packet.decoded=0;packet.mode=2;
 client.lastUsercmd.serverTime=100000;SV_UserMove(&client,&packet,true);
 CHECK(calls.size()==1&&client.lastUsercmd.serverTime==98);
 // Invalid count, null commands, inactive clients and ambiguous/old ordinals.
 std::array<usercmd_t,32> commands{};
 for(int count:{-1,0,33,INT_MAX}){calls.clear();SV_ExecuteSTVEFSPCommands(&client,commands.data(),count,6);CHECK(calls.empty());}
 calls.clear();SV_ExecuteSTVEFSPCommands(&client,nullptr,1,6);CHECK(calls.empty());
 for(int newest:{4,5,fnql::net::CounterAdd(5,0x80000000u)}){
  calls.clear();SV_ExecuteSTVEFSPCommands(&client,commands.data(),1,newest);
  CHECK(calls.empty()&&client.stvefSpCommandNumber==5);
 }
 for(int state:{CS_PRIMED,CS_ZOMBIE}){
  client.state=state;SV_ExecuteSTVEFSPCommands(&client,commands.data(),1,6);CHECK(calls.empty());
 }
 // A game callback that drops its client stops bounded recovery immediately.
 client={};Reset(client);dropAt=2;SV_ExecuteSTVEFSPCommands(&client,commands.data(),1,40);
 CHECK(calls.size()==2&&client.state==CS_ZOMBIE);
 client={};Reset(client);dropAt=1;SV_ExecuteSTVEFSPCommands(&client,commands.data(),3,3);
 CHECK(calls.size()==1&&client.state==CS_ZOMBIE);
 // Full-width counter wrap does not overflow or replay acknowledged commands.
 for(int last:{INT_MAX-1,-2,-1}){
  client={};Reset(client);client.stvefSpCommandNumber=last;
  const int newest=fnql::net::CounterAdd(last,3);
  SV_ExecuteSTVEFSPCommands(&client,commands.data(),3,newest);
  CHECK(calls.size()==3&&client.stvefSpCommandNumber==newest);
 }
 // Shared dispatch still gives other profiles the original one-argument export.
 stvefSP=false;client={};Reset(client);usercmd_t command{123,7};
 SV_ClientThink(&client,&command);CHECK(calls.size()==1&&client.lastUsercmd.serverTime==123);
 puts("72 native command-recovery cases plus decoder, lifecycle and counter boundaries");
}
'''


class STVEFSPUserMoveTests(unittest.TestCase):
    def compile_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        sanitizers = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        temporary_root = ROOT / '.tmp'
        temporary_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-commands-', dir=temporary_root) as folder:
            cpp = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source)
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', *sanitizers,
                '-I', str(ROOT / 'code/qcommon'), '-I', str(ROOT), str(cpp), '-o', str(binary)], cwd=ROOT,
                capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_native_command_recovery_and_decode_boundaries(self):
        server = (ROOT / 'code/server/sv_client.cpp').read_text()
        host = shared.HARNESS[:shared.HARNESS.index('// The entire SV_UserMove')]
        host = host.replace('#include <array>', '''#include <array>
#include <cstdarg>
#include <cstdio>
#include <climits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\\n",__LINE__,#x);std::exit(1);}}while(0)''')
        host = host.replace('int serverTime = 0;', 'int serverTime = 0; int weapon = 0;')
        host = host.replace('int count = 3;', 'int count = 3; int newest = 0; int mode = 1;')
        host = host.replace('bool Com_GameIsSTVEFSP() { return false; }', 'static bool stvefSP=true; bool Com_GameIsSTVEFSP() { return stvefSP; }')
        host = host.replace('int MSG_ReadLong(msg_t *) { return 0; }', 'int MSG_ReadLong(msg_t *msg) { return msg->newest; }')
        host = host.replace('out->serverTime = 100 + index;', 'out->serverTime = msg->mode==0?0:(msg->mode==1?100+index:100-index); out->weapon=index+1;')
        start = host.index('void SV_ClientThink(')
        host = host[:start] + CALLBACK + function_body(server, 'SV_ClientThink')
        body = function_body(server, 'SV_ExecuteSTVEFSPCommands') + function_body(server, 'SV_UserMove')
        fixture = json.loads((ROOT / 'tests/fixtures/stvef_sp_user_move_retail.json').read_text())
        self.assertEqual(len(fixture['cases']), 72)
        cases = []
        for item in fixture['cases']:
            scalars = ','.join(str(item[k]) for k in ('last','newest','count','times','mutate','final','dropped','lastTime'))
            calls = ','.join('{' + ','.join(map(str, call)) + '}' for call in item['calls'])
            cases.append('{' + scalars + ',{' + calls + '}}')
        self.compile_run(host + body + CHECKS.replace('@CASES@', ',\n'.join(cases)))

    def test_game_adjusts_the_supplied_command(self):
        host = (ROOT / 'code/server/server_stvef_sp_game.hpp').read_text()
        start = host.index('case GAME_CLIENT_THINK: {')
        body = host[start:host.index('\n\tcase GAME_RUN_FRAME:', start)]
        self.compile_run(r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "code/qcommon/q_shared.h"
#include "code/compat/stvef_state_adapter.hpp"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
enum{GAME_CLIENT_THINK=7};
struct Drop{};
static int called=0;
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
struct Game{void (*ClientThink)(int,fntech3_stvef_sp_usercmd_t*);};
static void Think(int index,fntech3_stvef_sp_usercmd_t *cmd){
 CHECK(index==0);++called;cmd->serverTime=550;cmd->weapon=6;cmd->buttons=0x10203040;
 cmd->angles[1]=1234;cmd->forwardmove=-80;
}
static Game storage{Think},*game=&storage;
static intptr_t Entry(int command,intptr_t arg[3]){
 switch(command){@BODY@}return 1;
}
int main(){
 usercmd_t command{};command.serverTime=0;command.weapon=2;
 intptr_t args[3]={0,reinterpret_cast<intptr_t>(&command),0};
 CHECK(Entry(GAME_CLIENT_THINK,args)==1&&called==1);
 CHECK(command.serverTime==550&&command.weapon==6&&command.buttons==0x10203040);
 CHECK(command.angles[1]==1234&&command.forwardmove==-80);
 args[1]=0;bool dropped=false;try{Entry(GAME_CLIENT_THINK,args);}catch(Drop&){dropped=true;}
 CHECK(dropped&&called==1);
}
'''.replace('@BODY@', body))

    def test_snapshot_acknowledgement_header_round_trip(self):
        server = (ROOT / 'code/server/sv_snapshot.cpp').read_text()
        client = (ROOT / 'code/client/cl_parse.cpp').read_text()
        a = server.index('\tMSG_WriteByte( msg, svc_snapshot );')
        b = server.index('\n\t// send over the areabits', a)
        writer = server[a:b]
        a = client.index('\tnewSnap.serverCommandNum = clc.serverCommandSequence;')
        b = client.index('\n\t// If the frame is delta compressed', a)
        reader = client[a:b]
        bridge = (ROOT / 'code/client/client_stvef_sp_cgame.hpp').read_text()
        a = bridge.index('\tresult.snapFlags = source->snapFlags & SNAPFLAG_RATE_DELAYED;')
        b = bridge.index('\n\tresult.ping =', a)
        flags = bridge[a:b]
        self.compile_run(r'''
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include "code/qcommon/q_shared.h"
#include "code/qcommon/netchan_safety.hpp"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
enum{CS_ACTIVE=4,svc_snapshot=7};
struct msg_t{std::array<unsigned char,64> data{};int readcount=0,cursize=0;};
static int profile=0;
bool Com_GameIsSTVEFSP(){return profile==5;}
bool CL_STVEFFraming(){return profile==6;}
bool CL_LegacyFraming(){return CL_STVEFFraming();}
// The Ritual engines' time residual follows the time; no profile here is one.
enum{NETCHAN_WIRE_ALICE=1000};
int CL_ActiveWireProfile(){return 0;}
bool CL_IsLegacyDemoMessage(){return false;}
int CL_LegacyDemoProtocol(){return 68;}
struct client_t{int lastClientCommand=19,oldServerTime=0,stvefSpCommandNumber=0,state=CS_ACTIVE;bool rateDelayed=false,stvefSpDroppedCommands=false;};
bool SV_LegacyFraming(const client_t*){return CL_STVEFFraming();}
void SV_WriteRitualTimeResidual(const client_t*,msg_t*){}
struct {int time=1600;} sv;
struct {int snapFlagServerBit=4;} svs;
struct {int serverCommandSequence=23,serverMessageSequence=12;} clc;
static int reliableAck;
void CL_ValidateReliableAcknowledge(int n){reliableAck=n;}
struct {bool modified;} pauseStorage,*cl_paused=&pauseStorage;
struct Snapshot{int serverCommandNum=0,cmdNum=-77,serverTime=0,messageNum=0,deltaNum=0,snapFlags=0;};
struct Drop{};
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
void MSG_WriteByte(msg_t *msg,int v){CHECK(msg->cursize<64);msg->data[msg->cursize++]=v;}
void MSG_WriteLong(msg_t *msg,int v){for(int i=0;i<4;++i)MSG_WriteByte(msg,uint32_t(v)>>(i*8));}
int MSG_ReadByte(msg_t *msg){int i=msg->readcount++;return i<msg->cursize?msg->data[i]:-1;}
int MSG_ReadLong(msg_t *msg){uint32_t v=0;for(int i=0;i<4;++i){int b=MSG_ReadByte(msg);if(b<0)return -1;v|=uint32_t(b)<<(i*8);}int out;std::memcpy(&out,&v,4);return out;}
void Write(const client_t *client,msg_t *msg,int lastframe){int snapFlags;@WRITE@}
void Read(msg_t *msg,Snapshot &newSnap){int deltaNum;@READ@}
int ExternalFlags(const Snapshot *source){Snapshot result;@FLAGS@ return result.snapFlags;}
int main(){
 int cases=0;
 for(profile=0;profile<7;++profile)for(int number:{0,1,200,INT_MAX,INT_MIN,-1})
 for(int delta:{0,1,7})for(int bits=0;bits<8;++bits){
  client_t client;client.stvefSpCommandNumber=number;client.rateDelayed=bits&1;
  client.stvefSpDroppedCommands=bits&2;if(bits&4)client.state=3;
  client.oldServerTime=125;msg_t msg;Write(&client,&msg,delta);
  CHECK(msg.cursize==(Com_GameIsSTVEFSP()?11:(CL_STVEFFraming()?11:7)));
  CHECK(MSG_ReadByte(&msg)==svc_snapshot);Snapshot snap;Read(&msg,snap);
  CHECK(msg.readcount==msg.cursize&&snap.serverTime==1725);
  CHECK(snap.cmdNum==(Com_GameIsSTVEFSP()?number:-77));
  CHECK(snap.deltaNum==(delta?12-delta:-1));
  CHECK(snap.snapFlags==(4|(bits&1)|((bits&4)?2:0)|((Com_GameIsSTVEFSP()&&(bits&2))?8:0)));
  if(Com_GameIsSTVEFSP())CHECK(ExternalFlags(&snap)==((bits&1)|((bits&2)?2:0)));
  if(CL_STVEFFraming())CHECK(reliableAck==19);
  ++cases;
 }
 profile=5;client_t client;client.stvefSpCommandNumber=12345;msg_t full;Write(&client,&full,0);
 // Truncate each of the four ordinal bytes; no acknowledgement is published.
 for(int size=6;size<10;++size){msg_t broken=full;broken.cursize=size;broken.readcount=1;Snapshot snap;bool drop=false;
  try{Read(&broken,snap);}catch(Drop&){drop=true;}CHECK(drop);
 }
 printf("%d snapshot header cases across seven profiles plus truncated ordinals\n",cases);
}
'''.replace('@WRITE@', writer).replace('@READ@', reader).replace('@FLAGS@', flags))


if __name__ == '__main__':
    unittest.main()
