"""HTTP archive validation spans arbitrary callback boundaries and rejects writes."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class DownloadWriteTests(unittest.TestCase):
    def test_http_headers_and_short_writes(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        source=(ROOT/'code/client/cl_curl.cpp').read_text()
        functions=[]
        for name in ('HTTP_WritePak','CL_cURL_CallbackWrite','Com_DL_CallbackWrite'):
            start=source.index('static size_t '+name+'(')
            functions.append(source[start:source.index('\n}',start)+2])
        parse=(ROOT/'code/client/cl_parse.cpp').read_text()
        start=parse.index('qboolean CL_ValidPakSignature(')
        signature=parse[start:parse.index('\n}',start)+2]
        harness=r'''
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <vector>
using byte=unsigned char;using fileHandle_t=int;using qboolean=bool;
constexpr bool qtrue=true,qfalse=false;constexpr int FS_INVALID_HANDLE=0;
#define S_COLOR_YELLOW ""
struct httpPakHeader_t {byte data[22];size_t used;};
struct Connection{int download=0;httpPakHeader_t downloadPakHeader{};char downloadTempName[32]="test.tmp";}clc;
struct download_t{int fHandle=0;httpPakHeader_t pakHeader{};char TempName[32]="test.tmp";};
int opens,writes,failAt,shortBy;bool openFail;
std::vector<byte> stored;
int FS_SV_FOpenFileWrite(const char*){++opens;return openFail?0:3;}
int FileWrite(int handle,const void*p,size_t n){assert(handle==3);++writes;if(writes==failAt)return int(n)-shortBy;auto b=static_cast<const byte*>(p);stored.insert(stored.end(),b,b+n);return int(n);}
void Com_Printf(const char*,...){}
void reset(){clc=Connection{};opens=writes=failAt=0;shortBy=1;openFail=false;stored.clear();}
'''+signature+'\n'+'\n'.join(functions)+r'''
int main(){
 for(bool general:{false,true}){
  for(int format:{0,1,2}){
   std::vector<byte> data(format==2?12:64,0);data[0]='P';data[1]=format==2?'A':'K';data[2]=format==2?'C':format==1?5:3;data[3]=format==2?'K':format==1?6:4;
   for(size_t chunk=1;chunk<=data.size();++chunk){
    reset();download_t dl;
    for(size_t offset=0;offset<data.size();offset+=chunk){size_t n=std::min(chunk,data.size()-offset);
     size_t result=general?Com_DL_CallbackWrite(data.data()+offset,1,n,&dl):CL_cURL_CallbackWrite(data.data()+offset,1,n,&clc.download);
     assert(result==n);
    }
    assert(opens==1&&stored==data);
   }
  }
  std::vector<byte> data(64,0);data[0]='P';data[1]='K';data[2]=3;data[3]=4;
  for(int failure:{1,2,3})for(int shortfall:{1,22,100}){
   reset();download_t dl;failAt=failure;shortBy=shortfall;
   size_t result=general?Com_DL_CallbackWrite(data.data(),1,64,&dl):CL_cURL_CallbackWrite(data.data(),1,64,&clc.download);
   if(failure<=2)assert(result==0);else{
    assert(result==64);
    result=general?Com_DL_CallbackWrite(data.data(),1,64,&dl):CL_cURL_CallbackWrite(data.data(),1,64,&clc.download);
    assert(result==0);
   }
  }
  reset();download_t dl;openFail=true;
  assert((general?Com_DL_CallbackWrite(data.data(),1,64,&dl):CL_cURL_CallbackWrite(data.data(),1,64,&clc.download))==0);
  assert(opens==1&&!writes);
  reset();dl=download_t{};data[0]='X';
  assert((general?Com_DL_CallbackWrite(data.data(),1,64,&dl):CL_cURL_CallbackWrite(data.data(),1,64,&clc.download))==0);
  assert(!opens&&!writes);
  reset();dl=download_t{};
  assert((general?Com_DL_CallbackWrite(nullptr,1,0,&dl):CL_cURL_CallbackWrite(nullptr,1,0,&clc.download))==0);
  assert(!opens&&!writes);
 }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
    def test_completion_requires_a_valid_accumulated_header(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        source=(ROOT/'code/client/cl_curl.cpp').read_text()
        start=source.index('\tCL_cURL_CloseDownload();',source.index('msg = qcurl_multi_info_read'))
        end=source.index('\n\t\tclc.downloadRestart',start)
        client=source[start:end]+'\n}\n}'
        start=source.index('\tCURLcode transferResult =',source.index('qboolean Com_DL_Perform('))
        end=source.index('\n\t\tbool autoDownload',start)
        general=source[start:end]+'\n++renames;\n}\n}'
        parse=(ROOT/'code/client/cl_parse.cpp').read_text();start=parse.index('qboolean CL_ValidPakSignature(')
        signature=parse[start:parse.index('\n}',start)+2]
        harness=r'''
#include <cassert>
#include <cstring>
using byte=unsigned char;using qboolean=bool;using CURLcode=int;
constexpr bool qtrue=true,qfalse=false;
constexpr int CURLMSG_DONE=1,CURLE_OK=0,CURLE_WRITE_ERROR=23,FS_INVALID_HANDLE=0,ERR_DROP=1;
#define S_COLOR_YELLOW ""
struct Header{byte data[22];size_t used;};
struct {Header downloadPakHeader;char downloadTempName[16],downloadName[16];}clc;
struct download_t{Header pakHeader;int fHandle;char Name[16];};
struct Message{int msg;struct{int result;}data;};
int renames,drops;
void CL_cURL_CloseDownload(){}
void CloseFile(int&){}
void FS_SV_Rename(const char*,const char*){++renames;}
void Com_Printf(const char*,...){}
[[noreturn]]void Com_Error(int,const char*,...){++drops;throw 1;}
'''+signature+'\nvoid clientFinish(Message*msg){\n'+client+'\nvoid generalFinish(download_t*dl,Message*msg){\n'+general+r'''
int main(){
 for(int kind=0;kind<3;++kind)for(size_t bytes=0;bytes<=22;++bytes){
  Header h{};h.used=bytes;h.data[0]='P';h.data[1]=kind==2?'A':'K';h.data[2]=kind==2?'C':kind==1?5:3;h.data[3]=kind==2?'K':kind==1?6:4;
  const bool valid=kind==2?bytes>=12:bytes>=22;
  clc.downloadPakHeader=h;renames=drops=0;Message msg{CURLMSG_DONE,{CURLE_OK}};
  try{clientFinish(&msg);}catch(int){}
  assert(renames==int(valid)&&drops==int(!valid));
  download_t dl{};dl.pakHeader=h;renames=drops=0;generalFinish(&dl,&msg);
  assert(renames==int(valid)&&drops==0&&msg.data.result==CURLE_OK);
 }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'finish.cpp';exe=Path(folder)/'finish.exe';cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
if __name__=='__main__':unittest.main()
