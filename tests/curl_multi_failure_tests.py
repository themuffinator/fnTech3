"""A failed cURL multi stack must stop before polling completion messages."""
import shutil, subprocess, tempfile, unittest
from pathlib import Path
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class CurlMultiFailureTests(unittest.TestCase):
    def test_polling_failure_boundary(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler required')
        source=(ROOT/'code/client/cl_curl.cpp').read_text()
        # Compile each actual polling prefix through info_read, replacing only
        # the subsequent per-transfer result handling with a probe return.
        parts=[]
        for signature, marker, tail in [
            ('void CL_cURL_PerformDownload( void )','msg = qcurl_multi_info_read','return;'),
            ('qboolean Com_DL_Perform( download_t *dl )','msg = dl->func.multi_info_read','return qtrue;')]:
            start=source.index(signature)
            end=source.index(';',source.index(marker,start))+1
            parts.append(source[start:end]+'\n'+tail+'\n}')
        harness=r'''
#include <array>
#include <cassert>
#include <cstring>
#include <initializer_list>
using CURLMcode=int; using qboolean=bool;
constexpr bool qtrue=true,qfalse=false;
constexpr int CURLM_OK=0,CURLM_CALL_MULTI_PERFORM=-1,ERR_DROP=1,CA_CONNECTED=3;
#define S_COLOR_RED ""
struct CURLMsg{};
int resultCode,retries,performed,readCount,cleaned,closed,removed,dropped;
int perform(void*,int*){++performed;if(retries-->0)return CURLM_CALL_MULTI_PERFORM;return resultCode;}
CURLMsg* info(void*,int*){++readCount;return nullptr;}
const char* error(int){return "controlled multi failure";}
struct {void* downloadCURLM;}clc;
struct {int state;}cls;
struct download_t {
 char TempName[64]="probe.tmp"; void*cURLM=nullptr; bool mapAutoDownload=false;
 struct {int(*multi_perform)(void*,int*)=perform;CURLMsg*(*multi_info_read)(void*,int*)=info;const char*(*multi_strerror)(int)=error;}func;
};
int qcurl_multi_perform(void*p,int*c){return perform(p,c);}
CURLMsg*qcurl_multi_info_read(void*p,int*c){return info(p,c);}
const char*qcurl_multi_strerror(int c){return error(c);}
void CL_cURL_CloseDownload(){++closed;}
void CL_cURL_Cleanup(){++cleaned;}
void Com_DL_Cleanup(download_t*dl){++cleaned;dl->TempName[0]=0;}
void FS_SV_HomeRemove(const char*p){assert(!strcmp(p,"probe.tmp"));++removed;}
void Q_strncpyz(char*d,const char*s,int n){assert(int(strlen(s))<n);strcpy(d,s);}
void Com_Printf(const char*,...){}
[[noreturn]]void Com_Error(int,const char*,...){++dropped;throw 1;}
void reset(int result,int retry=0){resultCode=result;retries=retry;performed=readCount=cleaned=closed=removed=dropped=0;}
'''+ '\n'.join(parts)+r'''
int main(){
 for(int retry:{0,2}){
  reset(4,retry);try{CL_cURL_PerformDownload();}catch(int){}
  assert(dropped==1&&readCount==0&&closed==1&&cleaned==1&&performed==retry+1);
  for(bool automatic:{false,true})for(int state:{0,CA_CONNECTED}){
   download_t dl;dl.mapAutoDownload=automatic;cls.state=state;
   reset(4,retry);bool active=false;try{active=Com_DL_Perform(&dl);}catch(int){}
   assert(!active&&readCount==0&&cleaned==1&&removed==1);
   assert(dropped==int(automatic&&state==CA_CONNECTED)&&performed==retry+1);
  }
  reset(CURLM_OK,retry);CL_cURL_PerformDownload();assert(readCount==1&&!cleaned&&!closed&&!dropped);
  download_t dl;reset(CURLM_OK,retry);assert(Com_DL_Perform(&dl));assert(readCount==1&&!cleaned&&!removed&&!dropped);
 }
 reset(CURLM_OK,1000);CL_cURL_PerformDownload();assert(performed==101&&!readCount&&!dropped);
 download_t dl;reset(CURLM_OK,1000);assert(Com_DL_Perform(&dl));assert(performed==129&&!readCount&&!cleaned);
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-curl-multi-') as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
if __name__=='__main__':unittest.main()
