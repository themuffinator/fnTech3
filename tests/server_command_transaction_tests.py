"""Execute reliable-command parsing at truncation and counter boundaries."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class ServerCommandTransactionTests(unittest.TestCase):
    def test_rejected_command_preserves_history_and_wrap_order(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')
        start = source.index('static void CL_ParseCommandString(')
        function = source[start:source.index('\n}', start) + 2]
        code = r'''
#include "netchan_safety.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <climits>
#include <cstdarg>
#include <string>
constexpr int MAX_STRING_CHARS=1024, MAX_RELIABLE_COMMANDS=64;
constexpr int NETCHAN_WIRE_QL_RETAIL=1, ERR_DROP=1, EM_COMMAND=4;
constexpr int CA_CONNECTED=1, CA_PRIMED=2, FS_INVALID_HANDLE=0;
constexpr bool qfalse=false,qtrue=true;
struct State {int serverCommandSequence=10;int serverCommandHashes[64]{};
 char serverCommands[64][1024]{};bool serverCommandsIgnore[64]{};
 int download=0,eventMask=0;bool cURLUsed=false;} clc;
struct {int state=0;}cls;
struct {int integer=0;}shownet;
auto cl_shownet=&shownet;
void *cgvm=nullptr;
int profile=0,hashes=0,translations=0,disconnects=0;
struct msg_t {int seq=11,readcount=0,cursize=8;bool truncated=false;const char*text="print ok";};
int MSG_ReadLong(msg_t*m){return m->seq;}
int CL_ActiveWireProfile(){return profile;}
const char*MSG_ReadCommandStringForWireProfile(msg_t*m,int){m->readcount=m->cursize+(m->truncated?1:0);return m->text;}
int MSG_HashKeyForWireProfile(int,const char*s,int){++hashes;return s[0]+100;}
bool CL_TranslateLegacyConfigstringCommand(const char*s,char*d,int n){++translations;strncpy(d,s,n);return true;}
void Q_strncpyz(char*d,const char*s,int n){strncpy(d,s,n);d[n-1]=0;}
void Com_Printf(const char*,...){}
struct Drop{};
void Com_Error(int,const char*,...){throw Drop{};}
void Cmd_TokenizeString(const char*){}
const char*Cmd_Argv(int){return "disconnect";}
int Cmd_Argc(){return 1;}
int Q_stricmp(const char*a,const char*b){return strcmp(a,b);}
const char*va(const char*,...){return "disconnected";}
void Cvar_Set(const char*,const char*){}
bool CL_Disconnect(bool){++disconnects;return true;}
void CL_FlushMemory(){}
''' + function + r'''
void reset(int current){clc={};clc.serverCommandSequence=current;
 strcpy(clc.serverCommands[11],"retained");clc.serverCommandHashes[11]=777;
 hashes=translations=disconnects=0;}
int main(){
 for(profile=0;profile<2;++profile){
  // An incomplete new, duplicate or stale command must fail before hashing,
  // translation, history publication or disconnect side effects.
  for(int seq:{9,10,11}){
   reset(10);State before=clc;msg_t m;m.seq=seq;m.truncated=true;
   cls.state=CA_CONNECTED;clc.download=1;before=clc;
   try{CL_ParseCommandString(&m);assert(false);}catch(Drop&){}
   assert(memcmp(&before,&clc,sizeof(clc))==0);
   assert(hashes==0&&translations==0&&disconnects==0);cls.state=0;
  }
  struct Case{int current,incoming;bool accept;};
  for(Case c: {Case{10,11,true},Case{10,10,false},Case{10,9,false},
    Case{INT_MAX,INT_MIN,true},Case{INT_MIN,INT_MAX,false},
    Case{-1,0,true},Case{0,-1,false},Case{0,INT_MIN,false}}){
   reset(c.current);msg_t m;m.seq=c.incoming;CL_ParseCommandString(&m);
   assert(clc.serverCommandSequence==(c.accept?c.incoming:c.current));
   assert(bool(clc.eventMask&EM_COMMAND)==c.accept);
   if(c.accept)assert(strcmp(clc.serverCommands[c.incoming&63],"print ok")==0);
  }
  // The wire reader preserves QL percent bytes but sanitizes Q3's already.
  reset(10);msg_t m;m.text=profile?"%command":".command";CL_ParseCommandString(&m);
  assert(clc.serverCommandHashes[11]==(profile?'%':'.')+100);
  assert(clc.serverCommands[11][0]=='.');
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'commands.cpp';exe=Path(directory)/'commands.exe'
            cpp.write_text(code, encoding='utf-8')
            built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',
                                  '-I',str(ROOT/'code/qcommon'),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            ran=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)


if __name__=='__main__':
    unittest.main()
