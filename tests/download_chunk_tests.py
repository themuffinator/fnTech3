"""Reject incomplete download messages before writing or acknowledging chunks."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class DownloadChunkTests(unittest.TestCase):
    def test_truncation_and_order(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_parse.cpp').read_text()
        start = source.index('static void CL_ParseDownload( msg_t *msg ) {')
        parser = source[start:source.index('\n}', start) + 2]
        harness = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
using byte=unsigned char;
using qboolean=bool;
constexpr bool qfalse=false;
constexpr int FS_INVALID_HANDLE=0,MAX_MSGLEN=16384,ERR_DROP=1;
#define S_COLOR_YELLOW ""
struct msg_t{std::vector<byte> data;int readcount=0,cursize=0;};
// Controlled byte transport: preserve MSG_Read* overread sentinel semantics.
int read(msg_t*m,int n){unsigned v=0;for(int i=0;i<n;++i){int p=m->readcount++;if(p<m->cursize)v|=unsigned(m->data[p])<<(i*8);}return m->readcount>m->cursize?-1:int(v);}
int MSG_ReadShort(msg_t*m){return static_cast<int16_t>(read(m,2));}
int MSG_ReadLong(msg_t*m){return read(m,4);}
void MSG_ReadData(msg_t*m,void*d,int n){auto*b=static_cast<byte*>(d);for(int i=0;i<n;++i){int v=read(m,1);if(v<0){memset(b+i,0,n-i);return;}b[i]=byte(v);}}
int profile;
int CL_ActiveWireProfile(){return profile;}
const char* MSG_ReadStringForWireProfile(msg_t*,int){return "server refusal";}
struct Connection{char downloadTempName[64]="baseq3/test.pk3.tmp",downloadName[64]="baseq3/test.pk3";int recordfile=0,download=0,downloadBlock=0,downloadSize=0,downloadCount=0;}clc;
std::vector<std::string> commands;
std::vector<byte> written;
int opens,closes,renames,packets,next;
void Com_Printf(const char*,...){}
void Com_DPrintf(const char*,...){}
[[noreturn]] void Com_Error(int,const char*,...){throw 1;}
void CL_StopRecord_f(){}
void Cvar_SetIntegerValue(const char*,int){}
void CL_AddReliableCommand(const char*c,bool){commands.emplace_back(c);}
const char*va(const char*f,...){static char text[128];va_list a;va_start(a,f);vsnprintf(text,sizeof(text),f,a);va_end(a);return text;}
bool CL_ValidPakSignature(const byte*d,int n){return n>=4&&!memcmp(d,"PK\3\4",4);}
int FS_SV_FOpenFileWrite(const char*){++opens;return 1;}
void FileWrite(int,const byte*d,int n){written.insert(written.end(),d,d+n);}
void CloseFile(int&f){++closes;f=0;}
void FS_SV_Rename(const char*,const char*){++renames;}
void CL_WritePacket(int){++packets;}
void CL_NextDownload(){++next;}
'''+parser+r'''
msg_t chunk(int block,bool initial,int declared,std::vector<byte> body){
 msg_t m;auto put=[&](int v,int n){for(int i=0;i<n;++i)m.data.push_back(byte(unsigned(v)>>(8*i)));};
 put(block,2);if(initial)put(8,4);put(declared,2);
 m.data.insert(m.data.end(),body.begin(),body.end());m.cursize=int(m.data.size());return m;
}
void reset(int block,int handle){clc=Connection{};clc.downloadBlock=block;clc.download=handle;clc.downloadSize=8;clc.downloadCount=handle?4:0;commands.clear();written.clear();opens=closes=renames=packets=next=0;}
void unchanged(int block,int count){assert(commands.empty()&&written.empty()&&!opens&&!closes&&!renames&&!packets&&!next);assert(clc.downloadBlock==block&&clc.downloadCount==count);}
int main(){
 for(int selected:{68,91}){profile=selected;
  for(bool initial:{false,true}){
   for(int length=0;length<4;++length){
    int block=initial?0:1;reset(block,initial?0:1);
    std::vector<byte> data={'P','K',3,4};data.resize(length);
    auto m=chunk(block,initial,4,data);bool dropped=false;
    try{CL_ParseDownload(&m);}catch(int){dropped=true;}
    assert(dropped);unchanged(block,initial?0:4);
   }
  }
  reset(0,0);auto first=chunk(0,true,4,{'P','K',3,4});CL_ParseDownload(&first);
  assert(opens==1&&written.size()==4&&clc.downloadBlock==1&&clc.downloadCount==4&&commands.back()=="nextdl 0");
  reset(1,1);auto second=chunk(1,false,4,{'a','b','c','d'});CL_ParseDownload(&second);
  assert(written.size()==4&&clc.downloadBlock==2&&clc.downloadCount==8&&commands.back()=="nextdl 1");
  reset(2,1);auto eof=chunk(2,false,0,{});CL_ParseDownload(&eof);
  assert(closes==1&&renames==1&&packets==1&&next==1&&commands.back()=="nextdl 2");
  reset(4,1);auto stale=chunk(3,false,4,{'a','b','c','d'});CL_ParseDownload(&stale);unchanged(4,4);
  reset(65536,1);auto wrap=chunk(0,false,4,{'a','b','c','d'});CL_ParseDownload(&wrap);
  assert(clc.downloadBlock==65537&&commands.back()=="nextdl 65536");
 }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            cpp.write_text(harness)
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
