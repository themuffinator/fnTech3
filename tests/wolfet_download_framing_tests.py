"""ET's download terminator must not become a binary cgame message.

Compile the production caller and netchan terminator slices with the production
Huffman/message codecs. Read the result as ET does: stop at the first EOF, align
to a byte and deliver any remainder to cgame. A duplicate EOF must fail this
fixture, while non-ET profiles retain their established caller terminator.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]


class WolfETDownloadFramingTests(unittest.TestCase):
    def test_download_bodies_leave_no_et_binary_payload(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        server = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
        caller = function(server, 'static void SV_WriteDownloadEOF(')
        download = function(server, 'static int SV_WriteDownloadToClient(')
        self.assertEqual(download.count('SV_WriteDownloadEOF( cl, &msg );'), 2)
        self.assertNotIn('Netchan_ServerEOFOpcode', download)
        netchan = function((ROOT / 'code/server/sv_net_chan.cpp').read_text(encoding='utf-8'),
                           'void SV_Netchan_Transmit( client_t *client, msg_t *msg)')
        begin = netchan.index('\tif ( !SV_LegacyFraming( client ) )')
        end = netchan.index('\tSV_WolfETWriteBinaryMessage', begin)
        terminator = 'static void Finish( client_t *client, msg_t *msg ) {\n' + netchan[begin:end] + '}\n'
        prelude = r'''
#include "code/server/server.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
struct Drop {};
void QDECL Com_Error(errorParm_t,const char*,...){throw Drop{};}
void QDECL Com_Printf(const char*,...){}
void QDECL Com_DPrintf(const char*,...){}
static netchanWireProfile_t active;
int Com_GameIsWolfET(){return active==NETCHAN_WIRE_WOLFET;}
int Com_GameIsSTVEFSP(){return active==NETCHAN_WIRE_STVEF_SP;}
'''
        checks = r'''
int main(){
 std::array<byte,16384> storage{};
 const std::array<byte,4> pak{'P','K',3,4};
 auto cl=std::make_unique<client_t>();
 for(auto profile:{NETCHAN_WIRE_LEGACY_Q3,NETCHAN_WIRE_IOQ3,NETCHAN_WIRE_QL_RETAIL,
                   NETCHAN_WIRE_RTCW_SP,NETCHAN_WIRE_RTCW_MP,NETCHAN_WIRE_WOLFET,
                   NETCHAN_WIRE_MOHAA}){
  active=profile;cl->netchan.wireProfile=profile;
  for(int kind:{0,1,2,3}) for(int length:{0,4,27,2048}){
   msg_t out;MSG_Init(&out,storage.data(),int(storage.size()));
   MSG_WriteLong(&out,7);MSG_WriteByte(&out,svc_download);
   MSG_WriteShort(&out,kind==0||kind==3?0:1);
   if(kind==0)MSG_WriteLong(&out,32916);
   if(kind==3){MSG_WriteLong(&out,-1);MSG_WriteString(&out,"fixture refused");}
   else{
    MSG_WriteShort(&out,kind==2?0:length);
    if(kind!=2)for(int i=0;i<length;++i)MSG_WriteByte(&out,pak[i%4]);
   }
   SV_WriteDownloadEOF(cl.get(),&out);Finish(cl.get(),&out);
   msg_t in=out;MSG_BeginReading(&in);
   assert(MSG_ReadLong(&in)==7&&MSG_ReadByte(&in)==svc_download);
   assert(MSG_ReadShort(&in)==(kind==0||kind==3?0:1));
   if(kind==0)assert(MSG_ReadLong(&in)==32916);
   if(kind==3){assert(MSG_ReadLong(&in)==-1);assert(!strcmp(MSG_ReadString(&in),"fixture refused"));}
   else{int count=MSG_ReadShort(&in);assert(count==(kind==2?0:length));
    for(int i=0;i<count;++i)assert(MSG_ReadByte(&in)==pak[i%4]);}
   assert(MSG_ReadByte(&in)==Netchan_ServerEOFOpcode(profile));
   if(profile==NETCHAN_WIRE_WOLFET){
    MSG_Uncompressed(&in);
    // ET passes every remaining byte to cgvm, which does not exist while
    // a connecting client downloads. No synthetic payload may remain.
    if(in.cursize!=in.readcount){fprintf(stderr,"ET kind %d length %d has %d binary bytes\n",kind,length,in.cursize-in.readcount);return 1;}
   }else{
    assert(MSG_ReadByte(&in)==Netchan_ServerEOFOpcode(profile));
   }
  }
 }
 // Holomatch's raw, byte-aligned messages have no EOF opcode at either
 // boundary. Keep its complete payload and bit position unchanged.
 active=NETCHAN_WIRE_STVEF_HM;cl->netchan.wireProfile=active;
 msg_t legacy;MSG_Init(&legacy,storage.data(),int(storage.size()));
 MSG_STVEFBitstream(&legacy);MSG_WriteLong(&legacy,7);MSG_WriteByte(&legacy,svc_download);
 MSG_WriteShort(&legacy,0);MSG_WriteLong(&legacy,4);MSG_WriteShort(&legacy,4);
 MSG_WriteData(&legacy,pak.data(),int(pak.size()));
 const int legacyBits=legacy.bit,legacyBytes=legacy.cursize;
 SV_WriteDownloadEOF(cl.get(),&legacy);Finish(cl.get(),&legacy);
 assert(legacy.bit==legacyBits&&legacy.cursize==legacyBytes);
}
'''
        scratch = ROOT / '.tmp' / 'compatibility-round-2026-10-02' / 'download-framing-tests'
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
            for name, source, success in (
                    ('production', caller, True),
                    ('double-eof-mutation', caller.replace(' && !Com_GameIsWolfET()', ''), False)):
                cpp, exe = folder / f'{name}.cpp', folder / f'{name}.exe'
                cpp.write_text(prelude + source + terminator + checks, encoding='utf-8')
                built = subprocess.run([compiler, *common, '-std=c++17', str(cpp), *objects, '-o', str(exe)],
                                       cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                if success:
                    self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
                else:
                    self.assertNotEqual(ran.returncode, 0, 'duplicate EOF was not detected')


if __name__ == '__main__':
    unittest.main()
