"""Execute recursive filesystem rollback with controlled startup failures."""
from pathlib import Path
import shutil,subprocess,tempfile,unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class FilesystemRecoveryTests(unittest.TestCase):
    def test_recursive_recovery_and_initial_failure(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        s=(ROOT/'code/qcommon/files.c').read_text();start=s.index('void FS_Restart( int checksumFeed ) {')
        function=s[start:s.index('\n}',start)+2]
        harness=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define MAX_OSPATH 256
#define Q3CONFIG_CFG "q3config.cfg"
#define QL_CONFIG_HARDWARE_FILE "qzconfig.cfg"
#define QL_CONFIG_REPLICATE_FILE "repconfig.cfg"
using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;
enum {ERR_DROP=1,ERR_FATAL=2};
struct Cvar {char string[MAX_OSPATH];} base,game;
auto fs_basepath=&base;auto fs_gamedirvar=&game;
int fs_checksumFeed,starts,stops,clears,refreshes;
bool q3,safe,hardware,alwaysFail;int rtcw;
std::vector<std::string> commands;
int Q_stricmp(const char*a,const char*b){return strcmp(a,b);}
void Q_strncpyz(char*d,const char*s,int n){snprintf(d,n,"%s",s);}
bool Com_GameIsQuakeLive(){return !q3 && !rtcw;}
bool Com_GameIsJKAMP(){return false;}
bool MSG_SetJKAFieldOverrides(const void*,int,const void*,int){assert(false);return false;}
void FS_FreeFile(void*){assert(false);}
const char *Com_GameConfigFile(){return rtcw==1?"wolfconfig.cfg":rtcw==2?"wolfconfig_mp.cfg":"q3config.cfg";}
// Only a Ritual profile names another file; none is run here.
const char *Com_ConfigFile(){return Com_GameConfigFile();}
const char *va(const char *format,const char *name){static char text[128];snprintf(text,sizeof(text),format,name);return text;}
bool Com_SafeMode(){return safe;}
bool FS_ProfileFileExists(const char*n){assert(!q3&&!strcmp(n,QL_CONFIG_HARDWARE_FILE));return hardware;}
void Cbuf_AddText(const char*s){commands.emplace_back(s);}
void FS_Shutdown(bool close){assert(!close);++stops;}
void FS_Startup(){++starts;assert(starts<=6);}
int FS_ReadFile(const char*n,void*){assert(!strcmp(n,"default.cfg"));return !alwaysFail&&!strcmp(base.string,"valid")?16:-1;}
void FS_PureServerSetLoadedPaks(const char*a,const char*b){assert(!*a&&!*b);++clears;}
void Cvar_Set(const char*n,const char*v){
 assert(clears>0);
 if(!strcmp(n,"fs_basepath"))Q_strncpyz(base.string,v,MAX_OSPATH);
 else if(!strcmp(n,"fs_game"))Q_strncpyz(game.string,v,MAX_OSPATH);
 else assert(!strcmp(n,"fs_restrict")&&!strcmp(v,"0"));
}
[[noreturn]] void Com_Error(int severity,const char*){throw severity;}
void SV_FactoryRefreshMountedContent(){++refreshes;}
'''+function+r'''
int main(int argc,char**argv){
 assert(argc==6);q3=argv[1][0]=='1';rtcw=argv[1][0]>='2'?argv[1][0]-'1':0;safe=argv[2][0]=='1';hardware=argv[3][0]=='1';
 const int scenario=argv[4][0]-'0';bool baseGame=argv[5][0]=='1';
 Q_strncpyz(base.string,scenario==0?"bad":"valid",MAX_OSPATH);
 Q_strncpyz(game.string,baseGame?"":"savedmod",MAX_OSPATH);
 if(scenario==0){
  try{FS_Restart(7);assert(false);}catch(int e){assert(e==ERR_FATAL);}
  assert(starts==1&&stops==1&&clears==0&&refreshes==0&&commands.empty());return 0;
 }
 FS_Restart(7);assert(refreshes==1&&commands.empty());
 Q_strncpyz(base.string,"bad",MAX_OSPATH);Q_strncpyz(game.string,"brokenmod",MAX_OSPATH);
 alwaysFail=scenario==2;
 try{FS_Restart(99);assert(false);}catch(int e){assert(e==(alwaysFail?ERR_FATAL:ERR_DROP));}
 assert(!strcmp(base.string,"valid")&&!strcmp(game.string,baseGame?"":"savedmod"));
 assert(starts==3&&stops==3&&clears==1&&fs_checksumFeed==99);
 std::vector<std::string> expected;
 if(!alwaysFail&&!baseGame){
  if(q3 || rtcw){if(!safe)expected.emplace_back(std::string("exec ")+Com_GameConfigFile()+"\n");}
  else{
   if(!safe)expected.emplace_back(hardware?"execq qzconfig.cfg\n":"execq q3config.cfg\n");
#ifndef DEDICATED
   expected.emplace_back("execq repconfig.cfg\n");
#endif
  }
 }
 assert(commands==expected&&refreshes==(alwaysFail?1:2));
 if(!alwaysFail){commands.clear();FS_Restart(100);assert(commands.empty()&&refreshes==3&&clears==1);}
}
'''
        scratch=ROOT/'.tmp';scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='filesystem-recovery-',dir=scratch) as d:
            cpp=Path(d)/'test.cpp';cpp.write_text(harness)
            for dedicated in (False,True):
                # One executable per build: Windows can hold the last one run.
                exe=Path(d)/('test-dedicated.exe' if dedicated else 'test.exe')
                subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',*(['-DDEDICATED'] if dedicated else []),str(cpp),'-o',str(exe)],check=True,capture_output=True,text=True)
                for profile in (0,1,2,3):
                    for safe in (0,1):
                        for hardware in (0,1):
                            for scenario in (0,1,2):
                                for base in (0,1):
                                    subprocess.run([str(exe),*map(str,(profile,safe,hardware,scenario,base))],check=True,capture_output=True,text=True)
if __name__=='__main__':unittest.main()
