"""HTTP download callbacks must report storage failures to their caller."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT = Path(__file__).resolve().parents[1]


class DownloadWriteTests(unittest.TestCase):
    def test_http_short_writes(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT/'code/client/cl_curl.cpp').read_text()
        functions = []
        for name in ('CL_cURL_CallbackWrite', 'Com_DL_CallbackWrite'):
            start = source.index('static size_t '+name+'(')
            functions.append(source[start:source.index('\n}', start)+2])
        harness = r'''
#include <cassert>
#include <cstddef>
#include <cstring>
using byte=unsigned char;
using fileHandle_t=int;
constexpr int FS_INVALID_HANDLE=0,ERR_DROP=1;
#define S_COLOR_YELLOW ""
struct Connection{int download=0;char downloadName[32]="test.pk3",downloadTempName[32]="test.pk3.tmp";}clc;
struct download_t{int fHandle=0;char Name[32]="test.pk3",TempName[32]="test.pk3.tmp";};
int writeResult,opens,writes;
bool CL_ValidPakSignature(const byte*d,int n){return n>=4&&!memcmp(d,"PK\3\4",4);}
int FS_SV_FOpenFileWrite(const char*){++opens;return 3;}
int FileWrite(int handle,const void*,size_t n){assert(handle==3&&n==4);++writes;return writeResult;}
[[noreturn]] void Com_Error(int,const char*,...){throw 1;}
void Com_Printf(const char*,...){}
'''+ '\n'.join(functions) + r'''
int main(){
 char data[]={'P','K',3,4};
 for(bool initial:{false,true}){
  for(writeResult=-1;writeResult<=4;++writeResult){
   clc.download=initial?0:3;opens=writes=0;
   size_t result=CL_cURL_CallbackWrite(data,1,4,&clc.download);
   assert(result==size_t(writeResult>0?writeResult:0));
   assert(opens==int(initial)&&writes==1);
   download_t dl;dl.fHandle=initial?0:3;opens=writes=0;
   result=Com_DL_CallbackWrite(data,2,2,&dl);
   assert(result==size_t(writeResult>0?writeResult:0));
   assert(opens==int(initial)&&writes==1);
  }
 }
}
'''
        harness = '#include <initializer_list>\n' + harness
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
