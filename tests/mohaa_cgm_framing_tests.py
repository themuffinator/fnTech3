"""MOHAA client-game messages retain their final continuation bit at capacity.

Compile the production queue/replay bodies and buffer declarations with the
production Huffman/message codec. The retail cgame's ParseCGMessage reads a
six-bit type and continues until a false bit (1.00 cgamex86.dll, export 10,
0x3002f943 and 0x30030836). A full engine-side queue must still write that bit.
No retail code or data is incorporated in this fixture.
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


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]


class MohaaCgmFramingTests(unittest.TestCase):
    def test_queue_capacity_valid_widths_and_malformed_imports(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        source = (ROOT / 'code/server/server_mohaa_net.hpp').read_text(encoding='utf-8')
        constants = '\n'.join(re.findall(r'^constexpr int kCgm\w+ = [^;]+;', source, re.M))
        start = source.index('\tbyte cgmData[kCgmDataSize];')
        fields = source[start:source.index('\n};', start)]
        queue = function(source, 'void WriteCGMBits(')
        replay = function(source, 'void WriteCGMToClient(')
        prelude = r'''
#include "code/server/server.h"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
struct Drop {};
std::string error;
void QDECL Com_Error(errorParm_t,const char *fmt,...){
 char text[256];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);
 error=text;throw Drop{};
}
void QDECL Com_Printf(const char*,...){}
void QDECL Com_DPrintf(const char*,...){}
static int overflowReports;
'''
        host = (prelude + constants + '\nstruct ClientState {\n' + fields + '\n};\n'
                'void ReportCGMOverflow(ClientState &,int,bool){++overflowReports;}\n')
        checks = r'''
struct Datum {int value,bits;};
std::vector<byte> Bytes(msg_t &msg){return {msg.data,msg.data+msg.cursize};}
void CheckFrame(const std::vector<Datum> &values,int expectedSize){
 ClientState state{};std::array<byte,16384> encoded{},reference{};
 msg_t out,want;MSG_Init(&out,encoded.data(),int(encoded.size()));
 MSG_Init(&want,reference.data(),int(reference.size()));
 overflowReports=0;
 for(const auto &v:values){WriteCGMBits(state,0,v.value,v.bits);MSG_WriteBits(&want,v.value,v.bits<0?-v.bits:v.bits);}
 assert(state.cgmSize==expectedSize&&state.cgmTypesUsed==int(values.size()));
 assert(!overflowReports);
 const int before=state.cgmSize;
 WriteCGMToClient(state,0,&out);MSG_WriteBits(&want,0,1);
 assert(!out.overflowed&&!want.overflowed);
 if(out.bit!=want.bit||Bytes(out)!=Bytes(want)){
  fprintf(stderr,"CGM closing bit missing: queue %d bytes, actual %d expected %d bits\n",before,out.bit,want.bit);
  std::exit(1);
 }
 assert(!state.cgmSize&&!state.cgmTypesUsed&&!overflowReports);
 // A following MOHAA server opcode remains independent of the CGM decoder.
 MSG_WriteByte(&out,11);MSG_BeginReading(&out);
 for(const auto &v:values){const int bits=v.bits<0?-v.bits:v.bits;
  const auto expected=uint32_t(v.value)&(bits==32?~uint32_t(0):(uint32_t(1)<<bits)-1);
  assert(uint32_t(MSG_ReadBitsExternal(&out,bits))==expected);
 }
 assert(MSG_ReadBitsExternal(&out,1)==0&&MSG_ReadByte(&out)==11);
 // Reusing a flushed queue cannot resend its contents or add an empty CGM.
 const int end=out.bit;WriteCGMToClient(state,0,&out);assert(out.bit==end);
}
int main(){
 // CGM normalizes signed widths before replay, preserving even -32.
 std::vector<Datum> mixed{{10,8},{24,6}};int mixedBytes=2;
 for(int width=1;width<=32;++width)for(int sign:{-1,1}){
  mixed.push_back({width%2?-123456789:123456789,sign*width});
  mixedBytes+=width<=8?1:width<=16?2:4;
 }
 CheckFrame(mixed,mixedBytes);
 // Preserve complete queue bodies at all terminal boundaries. Its policy
 // accepts a four-byte datum when exactly four bytes remain.
 for(int prefix:{0,1,2,3}){
  std::vector<Datum> full;
  for(int i=0;i<prefix;++i)full.push_back({i+1,8});
  while(int(full.size())<prefix+(4096-prefix)/4)full.push_back({0x19345678,32});
  CheckFrame(full,prefix+((4096-prefix)/4)*4);
 }
 // Invalid widths are rejected at the module boundary, before any queued
 // state changes, including a full buffer that would otherwise hide them.
 for(int prior:{0,4096})for(int width:{0,33,-33,256,-256,
   std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}){
  ClientState state{};state.cgmSize=prior;state.cgmTypesUsed=prior/4;
  std::memset(state.cgmData,0xa5,sizeof(state.cgmData));
  std::memset(state.cgmTypes,0x5a,sizeof(state.cgmTypes));const ClientState before=state;
  error.clear();bool dropped=false;overflowReports=0;
  try{WriteCGMBits(state,0,17,width);}catch(Drop){dropped=true;}
  assert(dropped&&error.find("PF_MSG_WriteBits: bad bits")!=std::string::npos);
  assert(!memcmp(&state,&before,sizeof(state))&&!overflowReports);
 }
 // Capacity refusal still leaves accepted payload intact.
 ClientState state{};state.cgmSize=4093;state.cgmTypesUsed=8;
 const ClientState before=state;overflowReports=0;
 WriteCGMBits(state,0,123,16);assert(overflowReports==1&&!memcmp(&state,&before,sizeof(state)));
 puts("MOHAA CGM queue/replay controls passed");
}
'''
        scratch = ROOT / '.tmp' / 'ubertools-round-2026-10-02' / 'mohaa-cgm-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        common = [*target, '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT)]
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            objects = []
            for name, path, language in (
                    ('msg', 'code/qcommon/msg.cpp', ['-std=c++17', '-DDEDICATED']),
                    ('huffman', 'code/qcommon/huffman_static.c', ['-x', 'c', '-DDEDICATED'])):
                obj = folder / f'{name}.o'
                built = subprocess.run([compiler, *common, *language, '-c', str(ROOT / path), '-o', str(obj)],
                                       cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                objects.append(str(obj))
            # Restore the former terminator queueing to prove the fixture
            # distinguishes a valid ordinary frame from a capacity failure.
            mutation = replay.replace('MSG_WriteBits( msg, 0, 1 );', '')
            mutation = mutation.replace('const byte *data = state.cgmData;',
                                        'WriteCGMBits( state, 0, 0, 1 );\n\tconst byte *data = state.cgmData;')
            for name, body, success in (
                    ('production', replay, True), ('queued-end-bit-mutation', mutation, False)):
                cpp, exe = folder / f'{name}.cpp', folder / f'{name}.exe'
                cpp.write_text(host + queue + body + checks, encoding='utf-8')
                built = subprocess.run([compiler, *common, '-std=c++17', str(cpp), *objects, '-o', str(exe)],
                                       cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                if success:
                    self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
                    self.assertIn('MOHAA CGM queue/replay controls passed', ran.stdout)
                else:
                    self.assertNotEqual(ran.returncode, 0, 'queue capacity failure was not detected')
                    self.assertIn('CGM closing bit missing', ran.stderr)


if __name__ == '__main__':
    unittest.main()
