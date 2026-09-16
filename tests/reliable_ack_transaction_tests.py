"""An invalid peer acknowledgement must not poison disconnect cleanup."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from compiler_support import compiler_target_args
ROOT = Path(__file__).resolve().parents[1]

class ReliableAckTransactionTests(unittest.TestCase):
    def test_invalid_ack_preserves_cleanup_window_and_demo_policy(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++ compiler required')
        source = (ROOT/'code/client/cl_parse.cpp').read_text()
        start = source.index('static void CL_ValidateReliableAcknowledge(')
        validation = source[start:source.index('\n}', start)+2]
        start = source.index('if ( !CL_IsLegacyDemoMessage() ||', source.index('void CL_ParseServerMessage('))
        caller = source[start:source.index('\n\t// parse the message', start)]
        main = (ROOT/'code/client/cl_main.cpp').read_text()
        start = main.index('void CL_AddReliableCommand(')
        add = main[start:main.index('\n}',start)+2]
        inputs = (ROOT/'code/client/cl_input.cpp').read_text()
        start = inputs.index('std::uint32_t pendingReliable = 0;')
        window = inputs[start:inputs.index('\n\tn = static_cast<int>( pendingReliable );',start)]
        harness = r'''
#include "netchan_safety.hpp"
#include <cassert>
#include <cstring>
#include <limits>
#include <initializer_list>
constexpr int MAX_RELIABLE_COMMANDS=64,NA_BAD=0,ERR_DROP=1;
int CL_ReliableCommandLimit(){return MAX_RELIABLE_COMMANDS;}
using qboolean=bool;
#define S_COLOR_YELLOW ""
struct {int reliableSequence,reliableAcknowledge;bool demoplaying;struct{int type;} serverAddress;char reliableCommands[64][1024];} clc;
bool com_errorEntered;
int errors,warnings;
struct Failure {};
void Com_Error(int,const char*,...){++errors;throw Failure{};}
void Com_Printf(const char*,...){++warnings;}
void Q_strncpyz(char*d,const char*s,int n){strncpy(d,s,n);d[n-1]=0;}
bool CL_IsLegacyDemoMessage(){return false;}
int CL_LegacyDemoProtocol(){return 0;}
int MSG_ReadLong(int *msg){return *msg;}
''' + validation + '\n' + add + '\nvoid parse(int *msg){'+caller+'}\nvoid writeWindow(){'+window+'}\n' + r'''
void setup(int seq,int ack,bool demo=false){clc={};clc.reliableSequence=seq;clc.reliableAcknowledge=ack;clc.demoplaying=demo;clc.serverAddress.type=1;errors=warnings=0;com_errorEntered=false;}
void apply(int incoming){parse(&incoming);}
int main(){
 // Live malformed header: cleanup must still be able to queue/transmit disconnect.
 for(int future:{11,1000}){
  setup(10,8);
  try{apply(future);assert(false);}catch(Failure&){}
  com_errorEntered=true;
  CL_AddReliableCommand("disconnect",true);
  try{writeWindow();}catch(Failure&){}
  assert(errors==1);
  assert(clc.reliableAcknowledge==8 && clc.reliableSequence==11);
  assert(!strcmp(clc.reliableCommands[11],"disconnect"));
 }
 setup(10,8);apply(9);assert(clc.reliableAcknowledge==9 && !errors);
 setup(100,99);apply(36);assert(clc.reliableAcknowledge==36 && !warnings);
 setup(100,99);apply(35);assert(clc.reliableAcknowledge==100 && warnings==1);
 setup(10,8,true);apply(20);assert(clc.reliableSequence==20 && clc.reliableAcknowledge==20 && !errors);
 setup(100,99,true);apply(35);assert(clc.reliableAcknowledge==100 && !warnings);
 setup(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
 apply(std::numeric_limits<int>::min());assert(!errors && clc.reliableAcknowledge==std::numeric_limits<int>::min());
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-reliable-ack-') as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
            result=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-I',str(ROOT/'code/qcommon'),str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

if __name__=='__main__':unittest.main()
