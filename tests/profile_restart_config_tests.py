"""Execute production filesystem-restart config scheduling for both profiles."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class ProfileRestartConfigTests(unittest.TestCase):
    def test_config_queue_respects_profile_and_transition(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        s=(ROOT/'code/qcommon/files.c').read_text()
        start=s.index('\tif ( Q_stricmp(fs_gamedirvar->string, lastValidGame) && execConfig )')
        end=s.index('\n\tQ_strncpyz( lastValidBase',start)
        block=s[start:end]
        harness=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#define Q3CONFIG_CFG "q3config.cfg"
#define QL_CONFIG_HARDWARE_FILE "qzconfig.cfg"
#define QL_CONFIG_REPLICATE_FILE "repconfig.cfg"
constexpr bool qfalse=false;
bool q3,safe,hardware,execConfig;int probes,rtcw;
struct {const char *string;} game;
auto fs_gamedirvar=&game;
const char *lastValidGame="old";
std::vector<std::string> commands;
int Q_stricmp(const char*a,const char*b){return strcmp(a,b);}
bool Com_GameIsQuakeLive(){return !q3 && !rtcw;}
const char *Com_GameConfigFile(){return rtcw==1?"wolfconfig.cfg":rtcw==2?"wolfconfig_mp.cfg":"q3config.cfg";}
const char *va(const char *format,const char *name){static char text[128];std::snprintf(text,sizeof(text),format,name);return text;}
bool Com_SafeMode(){return safe;}
bool FS_ProfileFileExists(const char *p){assert(!q3 && !strcmp(p,QL_CONFIG_HARDWARE_FILE));++probes;return hardware;}
void Cbuf_AddText(const char *s){commands.emplace_back(s);}
void schedule(){
'''+block+r'''
}
int main(){
 for(int p=0;p<4;++p)for(int sm=0;sm<2;++sm)
 for(int hw=0;hw<2;++hw)for(int changed=0;changed<2;++changed)
 for(int requested=0;requested<2;++requested){
  q3=p==1;rtcw=p>=2?p-1:0;safe=sm;hardware=hw;game.string=changed?"mod":"old";
  execConfig=requested;probes=0;commands.clear();schedule();
  assert(!execConfig);
  std::vector<std::string> expected;
  if(changed && requested){
   if(q3 || rtcw){if(!safe)expected.emplace_back(std::string("exec ")+Com_GameConfigFile()+"\n");}
   else{
    if(!safe)expected.emplace_back(hardware?"execq qzconfig.cfg\n":"execq q3config.cfg\n");
#ifndef DEDICATED
    expected.emplace_back("execq repconfig.cfg\n");
#endif
   }
  }
  assert(commands==expected);
  assert(probes==(!q3 && !rtcw && !safe && changed && requested ? 1:0));
 }
}
'''
        scratch=ROOT/'.tmp';scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='profile-restart-',dir=scratch) as d:
            cpp=Path(d)/'test.cpp';exe=Path(d)/'test.exe';cpp.write_text(harness)
            for dedicated in (False,True):
                subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',*(['-DDEDICATED'] if dedicated else []),str(cpp),'-o',str(exe)],check=True,capture_output=True,text=True)
                subprocess.run([str(exe)],check=True,capture_output=True,text=True)
if __name__=='__main__':unittest.main()
