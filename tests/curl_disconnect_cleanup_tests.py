"""Disconnect must release cURL handles before clearing their owning state."""
import shutil, subprocess, tempfile, unittest
from pathlib import Path
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class CurlDisconnectCleanupTests(unittest.TestCase):
    def test_cleanup_precedes_connection_reset(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler required')
        main=(ROOT/'code/client/cl_main.cpp').read_text()
        start=main.index('\t// send a disconnect message to the server',main.index('qboolean CL_Disconnect('))
        sequence=main[start:main.index('\n\tcls.state = CA_DISCONNECTED;',start)]
        curl=(ROOT/'code/client/cl_curl.cpp').read_text()
        start=curl.index('void CL_cURL_Cleanup()')
        cleanup=curl[start:curl.index('\n}',start)+2]
        harness=r'''
#include <cassert>
#include <cstring>
#include <initializer_list>
#define USE_CURL
using CURLMcode=int;
constexpr int CURLM_OK=0,CA_CONNECTED=3,CA_CINEMATIC=9;
constexpr bool qtrue=true;
struct {int state;}cls;
struct Connection {void*downloadCURL;void*downloadCURLM;bool demoplaying;struct {int used;}downloadPakHeader;}clc;
int removed,easyFreed,multiFreed,liveEasy,liveMulti;
int qcurl_multi_remove_handle(void*m,void*e){assert(m&&e);++removed;return 0;}
void qcurl_easy_cleanup(void*e){assert(e);++easyFreed;--liveEasy;}
int qcurl_multi_cleanup(void*m){assert(m);++multiFreed;--liveMulti;return 0;}
const char*qcurl_multi_strerror(int){return "failure";}
void Com_DPrintf(const char*,...){}
void CL_AddReliableCommand(const char*,bool){}
void CL_WritePacket(int){}
void CL_ClearState(){}
void Com_Memset(void*p,int value,int n){assert(!liveEasy&&!liveMulti);memset(p,value,n);}
'''+cleanup+'\nvoid disconnect(){'+sequence+r'''
}
int main(){
 for(bool easy:{false,true})for(bool multi:{false,true}){
  clc={easy?reinterpret_cast<void*>(1):nullptr,multi?reinterpret_cast<void*>(2):nullptr,false};
  liveEasy=easy;liveMulti=multi;removed=easyFreed=multiFreed=0;
  disconnect();
  assert(easyFreed==int(easy)&&multiFreed==int(multi)&&removed==int(easy&&multi));
  CL_cURL_Cleanup();assert(easyFreed==int(easy)&&multiFreed==int(multi));
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-curl-cleanup-') as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
if __name__=='__main__':unittest.main()
