"""Execute profile configstring and fragment boundaries using production bodies."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def body(path, signature):
    source = (ROOT / path).read_text(encoding="utf-8")
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


class ProfileStateLimitsTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="fntech3-state-limits-", dir=ROOT / '.tmp') as directory:
            cpp = Path(directory) / "probe.cpp"
            exe = Path(directory) / "probe.exe"
            cpp.write_text(source, encoding="utf-8")
            # Match the active Windows library environment; Unix probes use
            # the shared engine's required x86 target.
            architecture = (["-DFNTECH3_Q3_ONLY"] if os.name == "nt" and
                            os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() != "x86" else ["-m32"])
            built = subprocess.run([compiler, *compiler_target_args(compiler), *architecture, "-std=c++17",
                                    "-I", str(ROOT / "code/qcommon"),
                                    str(cpp), "-o", str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_client_uses_selected_profile_before_connection_and_negotiated_profile_after(self):
        implementation = body("code/client/cl_main.cpp", "netchanWireProfile_t CL_ActiveWireProfile(")
        self.compile_and_run(r'''
#include "q_shared.h"
#include "qcommon.h"
#include "protocol_contract_profile.hpp"
#include <cassert>
#include <initializer_list>
struct {bool demoplaying;netchanWireProfile_t demoWireProfile;struct{netchanWireProfile_t wireProfile;}netchan;}clc;
struct {connstate_t state;}cls;
comGameProfile_t selected{};
const comGameProfile_t *Com_GameProfile(){return &selected;}
// Q3Test's wire is its pinned release's (cl_q3test.cpp).
int Com_GameIsQ3Test(){return 0;}
netchanWireProfile_t CL_Q3TestWireProfile(){assert(false);return NETCHAN_WIRE_Q3TEST_B;}
int Com_GameIsQuake3IHV(){return 0;}
netchanWireProfile_t CL_Q3IHVWireProfile(){assert(false);return NETCHAN_WIRE_Q3_IHV;}
''' + implementation + r'''
int main(){
 struct Mode{int protocol;netchanWireProfile_t wire;int commands;unsigned families;};
 for(Mode mode:{Mode{68,NETCHAN_WIRE_LEGACY_Q3,64,COM_PROTOCOL_FAMILY_LEGACY_QUAKE3},
               Mode{71,NETCHAN_WIRE_IOQ3,64,COM_PROTOCOL_FAMILY_IOQUAKE3},
               Mode{91,NETCHAN_WIRE_QL_RETAIL,64,COM_PROTOCOL_FAMILY_QUAKELIVE},
               Mode{49,NETCHAN_WIRE_RTCW_SP,256,COM_PROTOCOL_FAMILY_RTCW_SP},
               Mode{60,NETCHAN_WIRE_RTCW_MP,256,COM_PROTOCOL_FAMILY_RTCW_MP},
               Mode{26,NETCHAN_WIRE_JKA_MP,128,COM_PROTOCOL_FAMILY_JKA_MP},
               Mode{26,NETCHAN_WIRE_IOEF,64,COM_PROTOCOL_FAMILY_IOEF}}){
  selected.preferredProtocol=mode.protocol;selected.allowedProtocolFamilies=mode.families;clc={};
  for(auto state:{CA_DISCONNECTED,CA_CHALLENGING}){
   cls.state=state;assert(CL_ActiveWireProfile()==mode.wire);
   assert(Netchan_ReliableCommandLimit(CL_ActiveWireProfile())==mode.commands);
  }
  cls.state=CA_CONNECTED;clc.netchan.wireProfile=NETCHAN_WIRE_IOQ3;
  assert(CL_ActiveWireProfile()==NETCHAN_WIRE_IOQ3);
  clc.demoplaying=true;clc.demoWireProfile=mode.wire;
  assert(CL_ActiveWireProfile()==mode.wire);
 }
}
''')

    def test_configstring_rebuild_uses_profile_limit_and_preserves_other_slots(self):
        implementation = body("code/client/cl_cgame.cpp", "static void CL_ConfigstringModified(")
        self.compile_and_run(r'''
#include "q_shared.h"
#include "qcommon.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <initializer_list>
struct {unsigned before;gameState_t gameState;unsigned after;} cl;
int limit,clients,playerBase,charLimit,refreshes;
int Com_GameConfigstringLimit(){return limit;}
int Com_GameClientLimit(){return clients;}
int Com_GamePlayerConfigstringBase(){return playerBase;}
int Com_GameStateCharLimit(){return charLimit;}
std::string indexArg;
char payload[64];
const char *Cmd_Argv(int){return indexArg.c_str();}
char *Cmd_ArgsFrom(int){return payload;}
struct Drop{};
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
void CL_SystemInfoChanged(qboolean){}
void CL_WebHost_InvalidateFriendSnapshot(){++refreshes;}
''' + implementation + r'''
void update(int index,const char *text){indexArg=std::to_string(index);strcpy(payload,text);CL_ConfigstringModified();}
// Fill the text from `slot` up to exactly the profile's own limit, which is
// accepted; one byte more than that drops, whatever the storage holds.
void fillToLimit(int slot){
 std::string text;int last=-1;
 while(cl.gameState.dataCount<charLimit){
  const int remaining=charLimit-cl.gameState.dataCount;assert(remaining>=2);
  int length=std::min(62,remaining-1);
  if(remaining-(length+1)==1)--length;  // never leave a single byte over
  text.assign(length,char('a'+slot%26));last=slot;update(slot++,text.c_str());
 }
 assert(cl.gameState.dataCount==charLimit&&charLimit<=MAX_GAMESTATE_CHARS);
 text+='z';
 try{update(last,text.c_str());assert(false);}catch(Drop&){}
}
int main(){
 // Quake III, RTCW SP and RTCW MP keep Quake III's 16,000 bytes of text;
 // Medal of Honor 1.00 has 2,736 strings, its players at 1,684, and 32,000.
 struct Mode{int strings,clients,players,chars;};
 for(Mode mode:{Mode{1024,64,544,16000},Mode{2048,128,544,16000},Mode{2048,64,576,16000},
                Mode{2736,64,1684,32000}}){
  limit=mode.strings;clients=mode.clients;playerBase=mode.players;charLimit=mode.chars;cl={};
  cl.before=0x55aa55aa;cl.after=0xaa55aa55;cl.gameState.dataCount=1;refreshes=0;
  update(2,"first");update(limit-1,"last");
  assert(!strcmp(cl.gameState.stringData+cl.gameState.stringOffsets[2],"first"));
  assert(!strcmp(cl.gameState.stringData+cl.gameState.stringOffsets[limit-1],"last"));
  update(playerBase+clients-1,"player");assert(refreshes==1);
  update(playerBase+clients,"after players");assert(refreshes==1);
  const auto saved=cl;
  for(int index:{-1,limit,2048,MAX_GAME_CONFIGSTRINGS,0x7fffffff}){
   if(index>=0&&index<limit)continue;  // 2048 is one of Medal of Honor's own
   try{update(index,"invalid");assert(false);}catch(Drop&){}
   assert(!memcmp(&cl,&saved,sizeof(cl)));
  }
  for(int i=limit;i<MAX_GAME_CONFIGSTRINGS;++i)assert(cl.gameState.stringOffsets[i]==0);
  assert(cl.before==0x55aa55aa&&cl.after==0xaa55aa55);
  fillToLimit(3);
  assert(cl.before==0x55aa55aa&&cl.after==0xaa55aa55);
 }
}
''')

    def test_fragment_limits_include_sequence_header_and_reject_foreign_capacities(self):
        implementation = body("code/qcommon/net_chan.cpp", "qboolean Netchan_Process(")
        self.compile_and_run(r'''
#include "q_shared.h"
#include "qcommon.h"
#include "netchan_safety.hpp"
#include "stvef_netchan.hpp"
#include <cassert>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <vector>
constexpr std::uint32_t FragmentBit=0x80000000u;
constexpr int FragmentSize=1300;
const char *netsrcString[]{"client","server"};
cvar_t quiet{};
cvar_t *showpackets=&quiet,*showdrop=&quiet;
void QDECL Com_Printf(const char*,...){}
const char *NET_AdrToString(const netadr_t*){return "fixture";}
int SequenceChecksum(int challenge,int sequence){return fnql::net::SequenceChecksum(challenge,sequence);}
void MSG_BeginReadingOOB(msg_t *msg){msg->readcount=msg->bit=0;msg->oob=qtrue;}
int read(msg_t *msg,int bytes){
 if(msg->readcount<0||msg->readcount>msg->cursize-bytes){msg->readcount=msg->cursize+1;return -1;}
 std::uint32_t word=0;memcpy(&word,msg->data+msg->readcount,bytes);msg->readcount+=bytes;msg->bit=msg->readcount*8;
 return bytes==2?static_cast<short>(word):static_cast<int>(word);
}
int MSG_ReadLong(msg_t *msg){return read(msg,4);}
int MSG_ReadShort(msg_t *msg){return read(msg,2);}
''' + implementation + r'''
void append(msg_t &msg,std::uint32_t word,int bytes){assert(msg.cursize+bytes<=msg.maxsize);memcpy(msg.data+msg.cursize,&word,bytes);msg.cursize+=bytes;}
unsigned char value(int index){return static_cast<unsigned char>(index*17+3);}
void run(netchanWireProfile_t profile,int length,int capacity,bool expected,netsrc_t sock){
 netchan_t chan{};chan.wireProfile=profile;chan.sock=sock;chan.challenge=777;
 std::vector<byte> storage(static_cast<std::size_t>(capacity)+2,0xa5);
 msg_t msg{};msg.data=storage.data()+1;msg.maxsize=capacity;
 bool result=false;
 for(int start=0;start<length;start+=FragmentSize){
  const int count=std::min(FragmentSize,length-start);msg.cursize=0;
  append(msg,1u|FragmentBit,4);
  if(sock==NS_SERVER)append(msg,123,2);
  if(Netchan_WireHasFeature(profile,NETCHAN_FEATURE_SEQUENCE_CHECKSUM))append(msg,SequenceChecksum(chan.challenge,1),4);
  // Alice's wire carries the offset as a long (alice.exe 0x0044710e).
  append(msg,start,Netchan_WireHasFeature(profile,NETCHAN_FEATURE_LONG_FRAGMENT_START)?4:2);append(msg,count,2);
  for(int i=0;i<count;++i)append(msg,value(start+i),1);
  if(Netchan_WireHasFeature(profile,NETCHAN_FEATURE_PACKET_SCRAMBLE))
   assert(fntech3::stvef::netchan::Scramble(msg.data,static_cast<std::size_t>(msg.cursize)));
  result=Netchan_Process(&chan,&msg)!=qfalse;
  if(start+count<length)assert(!result&&chan.incomingSequence==0);
  assert(storage.front()==0xa5&&storage.back()==0xa5);
 }
 assert(result==expected);
 if(expected){
  assert(msg.cursize==length+4&&msg.readcount==4&&msg.bit==32&&chan.incomingSequence==1);
  for(int i=0;i<length;++i)assert(msg.data[i+4]==value(i));
 }else assert(chan.incomingSequence==0);
}
int main(){
 for(auto profile:{NETCHAN_WIRE_LEGACY_Q3,NETCHAN_WIRE_IOQ3,NETCHAN_WIRE_QL_RETAIL,NETCHAN_WIRE_RTCW_SP,NETCHAN_WIRE_RTCW_MP,
   NETCHAN_WIRE_ALICE}){
  const int limit=Netchan_MessageLimit(profile);
  for(auto sock:{NS_CLIENT,NS_SERVER}){
   run(profile,limit,limit+4,true,sock);
   run(profile,limit,limit,false,sock); // buffer has no room for the sequence header
   run(profile,limit+1,MAX_GAME_MSGLEN+8,false,sock);
   run(profile,20000,MAX_GAME_MSGLEN+8,limit>=20000,sock);
   // F.A.K.K.2's oracle gamestate: its later fragments start past 32,767.
   if(profile==NETCHAN_WIRE_ALICE)run(profile,40244,MAX_GAME_MSGLEN+8,true,sock);
  }
 }
 netchan_t invalid{};invalid.wireProfile=static_cast<netchanWireProfile_t>(99);
 byte bytes[4]{1,0,0,0};msg_t msg{};msg.data=bytes;msg.maxsize=msg.cursize=4;
 assert(!Netchan_Process(&invalid,&msg));
}
''')


if __name__ == "__main__":
    unittest.main()
