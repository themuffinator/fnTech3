"""Execute shared lifecycle/filesystem paths and IHV build gates in isolation."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


class IhvGameLifecycleTests(unittest.TestCase):
    def execute(self, source, defines=(), sources=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        scratch = ROOT/'.tmp'; scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ihv-lifecycle-', dir=scratch) as folder:
            folder = Path(folder); src = folder/'test.cpp'; exe = folder/'test.exe'
            src.write_text(source, encoding='utf-8')
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                *defines, str(src), *(str(ROOT/p) for p in sources), '-o', str(exe)], cwd=ROOT,
                text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            result = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_native_write_path_preserves_failure_and_does_not_open_file(self):
        source = (ROOT/'code/qcommon/files.c').read_text()
        self.execute(r'''
#include "code/qcommon/filesystem_write_bounds.h"
#include <cstdio>
#include <cstring>
#include <string>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void *fs_searchpaths=reinterpret_cast<void*>(1);
static cvar_t home{};static cvar_t *fs_homepath=&home;static const char *fs_gamedir="baseq3";
static std::string path;static int createCalls,policyCalls;static bool createFail;
char *FS_BuildOSPath(const char *,const char *,const char *name){path=std::string("home/baseq3/")+name;return path.data();}
static void FS_CheckFilenameIsNotAllowed(const char *,const char *,qboolean){++policyCalls;}
static qboolean FS_CreatePath(char const *){++createCalls;return createFail?qtrue:qfalse;}
''' + function(source, 'qboolean FS_NativeWritePath(') + r'''
int main(){
 char out[128]="unchanged",base[]="home";home.string=base;
 for(const char *invalid:{static_cast<const char *>(nullptr),"","../out","/out","C:/out","safe/.. /out","safe/file:stream"}){
  CHECK(!FS_NativeWritePath(invalid,out,sizeof out)&&!std::strcmp(out,"unchanged")&&!createCalls&&!policyCalls);
 }
 CHECK(!FS_NativeWritePath("test",nullptr,128)&&!FS_NativeWritePath("test",out,0));
 CHECK(!FS_NativeWritePath("test",out,16)&&!createCalls&&!policyCalls&&!std::strcmp(out,"unchanged"));
 std::string huge(MAX_OSPATH*2,'x');CHECK(!FS_NativeWritePath(huge.c_str(),out,100000)&&!createCalls);
 huge.resize(MAX_OSPATH*2-7);CHECK(!FS_NativeWritePath(huge.c_str(),out,100000)&&!createCalls);
 fs_searchpaths=nullptr;CHECK(!FS_NativeWritePath("test",out,128));fs_searchpaths=reinterpret_cast<void*>(1);
 createFail=true;CHECK(!FS_NativeWritePath("test",out,128)&&!std::strcmp(out,"unchanged")&&createCalls==1&&policyCalls==1);
 createFail=false;CHECK(FS_NativeWritePath("test",out,17));CHECK(!std::strcmp(out,"home/baseq3/test"));CHECK(createCalls==2);CHECK(policyCalls==2);
}
''')

    def test_hunk_map_preservation_and_full_shutdown(self):
        common = (ROOT/'code/qcommon/common.c').read_text()
        vm = (ROOT/'code/qcommon/vm.c').read_text()
        source = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include <cstdio>
#include <cstring>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
static bool ihv;static std::vector<int> events;static vm_t vmTable[VM_COUNT];
struct Hunk {int mark,permanent,temp,tempHighwater;};
static Hunk hunk_low,hunk_high,*hunk_permanent,*hunk_temp;
int Com_GameIsQuake3IHV(){return ihv;}
void CL_ShutdownCGame(){events.push_back(1);}void CL_ShutdownUI(){events.push_back(2);}
void SV_ShutdownGameProgs(){events.push_back(3);}void CIN_CloseAllVideos(){events.push_back(4);}
void QDECL Com_Printf(const char *,...){}
void VM_Free(vm_t *vm){events.push_back(10+static_cast<int>(vm-vmTable));std::memset(vm,0,sizeof(*vm));}
intptr_t QDECL Entry(int,...){return 0;}
''' + '\n'.join(function(vm, f) for f in ('void VM_ClearExcept(', 'void VM_Clear( void )'))
        source += '\n' + '\n'.join(function(common, f) for f in (
            'static void Hunk_ClearInternal(', 'void Hunk_Clear( void )', 'void Hunk_ClearForMap('))
        source += r'''
int main(){
 for(bool native:{false,true})for(bool selected:{false,true})for(bool full:{false,true}){
  events.clear();ihv=selected;hunk_low={1,2,3,4};hunk_high={5,6,7,8};
  for(auto &vm:vmTable){vm={};vm.name="test";vm.dllHandle=native?reinterpret_cast<void*>(1):nullptr;vm.entryPoint=Entry;}
  if(full)Hunk_Clear();else Hunk_ClearForMap();
  const bool keep=selected&&!full;
  std::vector<int> expected={1,2};if(!keep)expected.push_back(3);expected.push_back(4);
  for(int i=0;i<VM_COUNT;++i)if(i!=VM_GAME||!keep||!native)expected.push_back(10+i);
  CHECK(events==expected);CHECK((vmTable[VM_GAME].name!=nullptr)==(keep&&native));
  Hunk empty{};CHECK(!std::memcmp(&hunk_low,&empty,sizeof empty)&&!std::memcmp(&hunk_high,&empty,sizeof empty));
  CHECK(hunk_permanent==&hunk_low&&hunk_temp==&hunk_high);
 }
 events.clear();vmTable[VM_GAME].dllHandle=reinterpret_cast<void*>(1);vmTable[VM_GAME].entryPoint=nullptr;
 VM_ClearExcept(VM_GAME);CHECK(events.size()==VM_COUNT&&!vmTable[VM_GAME].dllHandle);
}
'''
        self.execute(source)

    def test_default_client_dedicated_and_developer_gates(self):
        source = r'''
#include "code/qcommon/game_manager.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 bool enabled=false;
#if defined(FNTECH3_Q3_IHV_DEVELOPMENT) && defined(_WIN32) && (defined(_M_IX86)||defined(__i386__))
 enabled=true;
#endif
 const auto *profile=Com_GameProfileById(COM_GAME_QUAKE3_IHV);
 CHECK(bool(Com_GameManagerCanActivate(profile))==enabled);
 comGameMarkerInputs_t input{"q3-ihv",COM_GAME_MARKER_QUAKE3_IHV,0,0};
 const auto detected=Com_DetectGameProfileMarkers(&input);
 CHECK(enabled?detected.profile==profile:detected.status==COM_GAME_DETECTION_INCOMPLETE_ENGINE);
 CHECK(Com_GameManagerCanActivate(Com_GameProfileById(COM_GAME_QUAKE3)));
 CHECK(Com_GameManagerCanActivate(Com_GameProfileById(COM_GAME_QUAKELIVE)));
 CHECK(!Com_GameManagerCanActivate(Com_GameProfileById(COM_GAME_WOLFET)));
}
'''
        for dev in (False, True):
            for dedicated in (False, True):
                with self.subTest(development=dev, dedicated=dedicated):
                    defines = (['-DFNTECH3_Q3_IHV_DEVELOPMENT=1'] if dev else []) + (['-DDEDICATED'] if dedicated else [])
                    self.execute(source, defines, ['code/qcommon/game_profile.cpp'])

    def test_systeminfo_keys_and_other_profile_isolation(self):
        source = (ROOT/'code/server/sv_init.cpp').read_text()
        self.execute(r'''
#include "code/server/server.h"
#include <cstdarg>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}}while(0)
server_t sv{};int cvar_modifiedFlags;
static comGameId_t profile;static int infoCalls,configIndex;static std::string value;
static std::vector<std::pair<int,std::string>> sets;
int Com_GameIsQuake3IHV(){return profile==COM_GAME_QUAKE3_IHV;}
int Com_GameIsQ3Test(){return profile==COM_GAME_Q3TEST;}
const char *SV_Q3TestSysteminfo(){return "q3test systeminfo";}
const char *SV_Q3TestServerinfo(){return "q3test serverinfo";}
const char *Cvar_InfoString_Big(int flag,qboolean *truncated){if(flag!=CVAR_SYSTEMINFO||truncated)std::abort();++infoCalls;return "retail info";}
const char *Cvar_VariableString(const char *name){if(std::string(name)!="sv_mapChecksum")std::abort();return "-99";}
const char *QDECL va(const char *format,...){static char text[1024];va_list args;va_start(args,format);vsnprintf(text,sizeof text,format,args);va_end(args);return text;}
void SV_SetConfigstring(int i,const char *text){configIndex=i;value=text;sets.emplace_back(i,text);}
''' + function(source, 'void SV_UpdateSysteminfoConfigstring(') + r'''
int main(){
 sv.serverId=-7;
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP,COM_GAME_QUAKE3_IHV,COM_GAME_Q3TEST}){
  profile=id;infoCalls=0;sets.clear();cvar_modifiedFlags=CVAR_SYSTEMINFO|CVAR_SERVERINFO;SV_UpdateSysteminfoConfigstring();
  CHECK(cvar_modifiedFlags==CVAR_SERVERINFO);
  // Q3Test publishes its release's own systeminfo, then its serverinfo.
  if(id==COM_GAME_Q3TEST){
   CHECK(infoCalls==0&&sets.size()==2);
   CHECK(sets[0].first==1&&sets[0].second=="q3test systeminfo");
   CHECK(sets[1].first==0&&sets[1].second=="q3test serverinfo");
   continue;
  }
  CHECK(configIndex==1&&sets.size()==1);
  CHECK(id==COM_GAME_QUAKE3_IHV?(infoCalls==0&&value=="\\serverid\\-7\\mapChecksum\\-99"):(infoCalls==1&&value=="retail info"));
 }
}
''')


if __name__ == '__main__': unittest.main()
