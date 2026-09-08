"""Download base URLs must join with exactly one trailing separator."""
from pathlib import Path
import shutil,subprocess,tempfile,unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class DownloadUrlTests(unittest.TestCase):
    def test_base_url_join(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        s=(ROOT/'code/client/cl_curl.cpp').read_text()
        start=s.index('\t\tQ_strncpyz( dl->URL, remoteURL',s.index('qboolean Com_DL_Begin('))
        end=s.index('\n\t}\n\n\tCom_Printf( "URL:',start)
        block=s[start:end]
        harness=r'''
#include <cassert>
#include <cstring>
#include <string>
using qboolean=bool;constexpr bool qtrue=true,qfalse=false;
struct download_t{char URL[256];bool headerCheck;};
void Q_strncpyz(char*d,const char*s,size_t n){assert(strlen(s)<n);strcpy(d,s);}
void Q_strcat(char*d,size_t n,const char*s){assert(strlen(d)+strlen(s)<n);strcat(d,s);}
bool Q_replace(const char*find,const char*value,char*d,size_t n){std::string s=d;auto pos=s.find(find);if(pos==std::string::npos)return false;s.replace(pos,strlen(find),value);Q_strncpyz(d,s.c_str(),n);return true;}
struct Escaped{const char*get(){return "a%20b.pk3";}}escapedName;
void join(download_t*dl,const char*remoteURL){
'''+block+r'''
}
int main(){
 download_t dl{};
 for(const char*base:{"http://127.0.0.1/maps","http://127.0.0.1/maps/"}){
  join(&dl,base);assert(!strcmp(dl.URL,"http://127.0.0.1/maps/a%20b.pk3"));assert(!dl.headerCheck);
 }
 join(&dl,"http://127.0.0.1/maps/%1?raw=1");assert(!strcmp(dl.URL,"http://127.0.0.1/maps/a%20b.pk3?raw=1")&&dl.headerCheck);
 join(&dl,"");assert(!strcmp(dl.URL,"/a%20b.pk3"));
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'url.cpp';exe=Path(folder)/'url.exe';cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
if __name__=='__main__':unittest.main()
