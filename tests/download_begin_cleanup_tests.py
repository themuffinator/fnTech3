"""A rejected archive basename must release partially initialized download state."""
from pathlib import Path
import shutil,subprocess,tempfile,unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class DownloadBeginCleanupTests(unittest.TestCase):
    def test_empty_basename_releases_partial_transfer(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        s=(ROOT/'code/client/cl_curl.cpp').read_text()
        parts=[]
        for signature in ('void Com_DL_Done(', 'qboolean Com_DL_InProgress(', 'void Com_DL_Cleanup('):
            start=s.index(signature);parts.append(s[start:s.index('\n}',start)+2])
        start=s.index('\tif ( !dl->Name[0] )',s.index('qboolean Com_DL_Begin('))
        end=s.index('\n\tCom_sprintf( dl->TempName',start)
        guard=s[start:end]
        harness=r'''
#include <cassert>
#include <cstring>
#include <initializer_list>
using qboolean=bool;constexpr bool qtrue=true,qfalse=false;
constexpr int FS_INVALID_HANDLE=0;
#define S_COLOR_YELLOW ""
int easyFreed,unloaded,liveEasy;
void easy_cleanup(void*p){assert(p);++easyFreed;--liveEasy;}
void Sys_UnloadLibrary(void*p){assert(p);++unloaded;}
void CloseFile(int&f){f=0;}
void Cvar_Set(const char*,const char*){}
void FS_SV_HomeRemove(const char*){assert(false);}
void Com_Printf(const char*,...){}
struct download_t {
 void*cURL=nullptr,*cURLM=nullptr;int fHandle=0,Size=0,Count=0;
 bool headerCheck=false,mapAutoDownload=false;struct{size_t used=0;}pakHeader;
 char URL[64]="http://127.0.0.1/.pk3",Name[64]="",Ext[8]=".pk3",TempName[64]="",progress[64]="";
 struct {
  void*lib=nullptr;
  void(*easy_cleanup)(void*)=nullptr;
  int(*multi_remove_handle)(void*,void*)=nullptr;
  int(*multi_cleanup)(void*)=nullptr;
 }func;
};
'''+ '\n'.join(parts)+'\nqboolean rejectEmpty(download_t*dl){\n'+guard+'\nreturn qtrue;\n}'+r'''
int main(){
 for(bool dynamic:{false,true}){
  download_t dl;dl.cURL=reinterpret_cast<void*>(1);dl.func.easy_cleanup=easy_cleanup;dl.func.lib=dynamic?reinterpret_cast<void*>(2):nullptr;
  liveEasy=1;easyFreed=unloaded=0;
  assert(Com_DL_InProgress(&dl));
  assert(!rejectEmpty(&dl));
  assert(!dl.cURL&&!dl.cURLM&&!Com_DL_InProgress(&dl));
  assert(!liveEasy&&easyFreed==1&&unloaded==int(dynamic));
  assert(!dl.URL[0]&&!dl.Ext[0]);
  Com_DL_Cleanup(&dl);assert(easyFreed==1&&unloaded==int(dynamic));
 }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'begin.cpp';exe=Path(folder)/'begin.exe';cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
if __name__=='__main__':unittest.main()

