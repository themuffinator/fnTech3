"""Execute demo command writing and module history bounds across counter wrap."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class ReliableHistoryWrapTests(unittest.TestCase):
    def test_writer_and_module_history_windows(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT/'code/client/cl_main.cpp').read_text(encoding='utf-8')
        start = source.index('static void CL_WriteServerCommands(')
        writer = source[start:source.index('\n}', start)+2]
        source = (ROOT/'code/client/cl_cgame.cpp').read_text(encoding='utf-8')
        start = source.index('// if we have irretrievably lost a reliable command')
        checks = source[start:source.index('\n\tindex = serverCommandNumber', start)]
        code = r'''
#include "netchan_safety.hpp"
#include "netchan_profile.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <vector>
constexpr int MAX_RELIABLE_COMMANDS=64,svc_serverCommand=5,ERR_DROP=1;
constexpr bool qfalse=false;
struct {int serverCommandSequence=0,demoCommandSequence=0;bool demoplaying=false;char serverCommands[256][16]{};}clc;
struct msg_t{};
int profile=0,clears=0,errors=0,bytes=0,strings=0;
std::vector<int> sequences;
int CL_ActiveWireProfile(){return profile;}
int CL_ReliableCommandLimit(){return Netchan_ReliableCommandLimit(static_cast<netchanWireProfile_t>(profile));}
void MSG_WriteByte(msg_t*,int value){assert(value==svc_serverCommand);++bytes;}
void MSG_WriteLong(msg_t*,int value){sequences.push_back(value);assert(sequences.size()<=static_cast<unsigned>(CL_ReliableCommandLimit()));}
void MSG_WriteStringForWireProfile(msg_t*,const char*s,int p){assert(p==profile);assert(s==clc.serverCommands[sequences.back()&(CL_ReliableCommandLimit()-1)]);++strings;}
void Cmd_Clear(){++clears;}
void Com_Error(int,const char*){++errors;}
''' + writer + '\nbool get(int serverCommandNumber){\n' + checks + '\nreturn true;\n}\n' + r'''
int main(){
 for(profile=0;profile<5;++profile){
  const unsigned limit=static_cast<unsigned>(CL_ReliableCommandLimit());
  for(int current:{INT_MIN+1,INT_MAX,0,10,INT_MIN}){
   for(unsigned pending:{0u,1u,3u,63u,64u,65u,100u,255u,256u,257u,300u}){
    clc.serverCommandSequence=current;clc.demoCommandSequence=fnql::net::CounterSubtract(current,pending);
    sequences.clear();bytes=strings=0;msg_t msg;CL_WriteServerCommands(&msg);
    unsigned count=std::min(pending,limit);assert(sequences.size()==count);
    assert(bytes==int(count)&&strings==int(count)&&clc.demoCommandSequence==current);
    for(unsigned i=0;i<count;++i)assert(sequences[i]==fnql::net::CounterSubtract(current,count-1-i));
   }
   for(unsigned ahead:{1u,64u,0x80000000u}){
    clc.demoCommandSequence=fnql::net::CounterAdd(current,ahead);sequences.clear();msg_t msg;
    CL_WriteServerCommands(&msg);assert(sequences.empty()&&clc.demoCommandSequence==current);
   }
   for(bool demo:{false,true}){
    clc.demoplaying=demo;
    for(unsigned age:{0u,1u,63u,64u,65u,255u,256u,257u,0x7fffffffu,0x80000000u,0xffffffffu}){
     clears=errors=0;bool accepted=get(fnql::net::CounterSubtract(current,age));
     assert(accepted==(age<limit));
     bool tooOld=age>=limit&&age<0x80000000u;
     assert(clears==int(tooOld&&demo));
     assert(errors==int(age>=limit&&!(tooOld&&demo)));
    }
   }
  }
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'history.cpp';exe=Path(directory)/'history.exe'
            cpp.write_text(code,encoding='utf-8')
            built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',
                                  '-I',str(ROOT/'code/qcommon'),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            ran=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)


if __name__=='__main__':
    unittest.main()
