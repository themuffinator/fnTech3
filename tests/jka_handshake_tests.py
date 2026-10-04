"""Execute Academy reply rules and production handshake state/protocol transfers."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def block(text, signature, start):
    first = text.index(signature, start)
    brace = text.index('{', first)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[first:end]


def fixture():
    main = (ROOT / 'code/client/cl_main.cpp').read_text()
    challenge = main.index('if ( !Q_stricmp(c, "challengeResponse" ) )')
    connect = main.index('if ( !Q_stricmp(c, "connectResponse") )')
    resend = main.index('\t\tconnectProtocol = clc.compat ?')
    selection = main[resend:main.index(';', resend) + 1]
    return r'''
#include "code/qcommon/jka_handshake.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <vector>
enum { qfalse=0,qtrue=1,CA_CHALLENGING=4,OLD_PROTOCOL_VERSION=68 };
struct Address { int endpoint; };
static struct {int state;} cls;
static struct {int challenge,handshakeProtocol,compat,connectPacketCount,connectTime;Address serverAddress;} clc;
static struct {int integer;} protocol{26};
static auto *com_protocol=&protocol;
static bool jka=true,q3=false;
static bool Com_GameIsJKAMP(){return jka;}
static bool Com_GameIsQuake3(){return q3;}
static std::vector<const char *> words;
static int Cmd_Argc(){return static_cast<int>(words.size());}
static const char *Cmd_Argv(int n){assert(n>=0&&n<Cmd_Argc());return words[n];}
static bool NET_CompareAdr(const Address *a,const Address *b){return a->endpoint==b->endpoint;}
static void Com_Printf(const char *){}
static void Com_DPrintf(const char *){}
static Address source{10};static const auto *from=&source;
static bool Challenge(){
''' + block(main, 'if ( Com_GameIsJKAMP() ) {', challenge) + r'''
 return false;
}
static bool Connect(){
''' + block(main, 'if (Com_GameIsJKAMP()) {', connect) + r'''
 return true;
}
static int OutboundProtocol(){int connectProtocol;
''' + selection + r'''
 return connectProtocol;
}
int main(){
 using namespace fntech3::jka::handshake;
 clc.serverAddress=source;clc.challenge=123;clc.compat=qtrue;
 words={"challengeResponse","456","123"};assert(Challenge());
 assert(cls.state==CA_CHALLENGING&&clc.challenge==456&&clc.handshakeProtocol==26&&!clc.compat);
 assert(clc.connectPacketCount==0&&clc.connectTime==-99999&&OutboundProtocol()==26);
 words={"connectResponse"};assert(Connect());source.endpoint=11;assert(!Connect());source.endpoint=10;
 words={"connectResponse","456","26"};assert(Connect());
 words={"connectResponse","457"};assert(!Connect());
 words={"connectResponse","456","68"};assert(!Connect());
 int challenge=99;
 const char *bare[]{"challengeResponse","-2147483648"};
 assert(ParseChallengeReply(2,bare,123,true,challenge)&&challenge==INT_MIN);
 assert(!ParseChallengeReply(2,bare,123,false,challenge)&&challenge==INT_MIN);
 const char *echo[]{"challengeResponse","2147483647","123","26","FnQL"};
 for(int count:{3,4,5})assert(ParseChallengeReply(count,echo,123,false,challenge)&&challenge==INT_MAX);
 assert(!ParseChallengeReply(6,echo,123,true,challenge));
 assert(!ParseChallengeReply(3,echo,124,true,challenge));
 for(const char *bad:{"","2147483648","-2147483649","2tail"," 2","+2"}){
  bare[1]=bad;assert(!ParseChallengeReply(2,bare,123,true,challenge)&&challenge==INT_MAX);
 }
 echo[3]="68";assert(!ParseChallengeReply(4,echo,123,true,challenge));echo[3]="26";
 echo[4]="unknown";assert(!ParseChallengeReply(5,echo,123,true,challenge));
 assert(!ParseChallengeReply(0,nullptr,123,true,challenge));
 clc.challenge=123;clc.serverAddress=source;words={"challengeResponse","456","122"};
 assert(!Challenge()&&clc.challenge==123);words={"challengeResponse","456"};assert(Challenge());
 jka=false;clc.challenge=123;assert(!Challenge()&&clc.challenge==123);jka=true;
 // Shared Q3 send selection remains independent of the Academy reply branch.
 q3=true;clc.compat=qtrue;clc.handshakeProtocol=66;assert(OutboundProtocol()==66);
 clc.handshakeProtocol=0;assert(OutboundProtocol()==68);
 clc.compat=qfalse;clc.handshakeProtocol=91;assert(OutboundProtocol()==91);
}
'''


class JKAHandshakeTests(unittest.TestCase):
    def test_replies_and_production_state_transfers(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-handshake'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            source = Path(folder) / 'handshake.cpp'
            executable = Path(folder) / ('handshake.exe' if os.name == 'nt' else 'handshake')
            source.write_text(fixture())
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                '-O2', '-Wall', '-Werror', '-I', str(ROOT), str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
