"""Exercise the production cinematic path/cache prefix and bounded helpers."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    source = (ROOT / 'code/client/cl_cin.cpp').read_text()
    shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
    start = source.index('int CIN_PlayCinematic(')
    end = source.index('\t// The decoder cache', start)
    prefix = source[start:end]
    functions = ('void Q_strncpyz(', 'void Q_strcat(', 'const char *COM_GetExtension(',
                 'void COM_DefaultExtension(', 'int Q_stricmpn(', 'int Q_stricmp(',
                 'int QDECL Com_sprintf(')
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <array>
#include <cassert>
#include <cstring>
#include <string>
struct Drop{};
void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
void QDECL Com_Printf(const char *,...){}
void QDECL Com_DPrintf(const char *,...){}
static comGameId_t profile;
int Com_GameIsJKA(){return profile==COM_GAME_JKA_SP||profile==COM_GAME_JKA_MP;}
int Com_GameIsSTVEFSP(){return profile==COM_GAME_STVEF_SP;}
static constexpr int MAX_VIDEO_HANDLES=16;
static struct {int state=CA_LOADING;} cls;
struct Row{std::array<char,MAX_OSPATH> fileName{};};
static std::array<Row,MAX_VIDEO_HANDLES> cinTable;
static bool CL_ShouldSkipIdLogoCinematic(const char *){return false;}
static void CL_RunCinematicNextmap(){assert(false);}
static std::string opened;static int opens;static bool binkOpened;
static int CIN_PlayBink(const char *path,int x,int y,int w,int h,int bits){
 assert(x==11&&y==22&&w==320&&h==240&&bits==0);
 opened=path;++opens;binkOpened=true;return 9;
}
#ifdef _WIN32
''' + function(shared, 'int Q_vsnprintf(') + '\n#endif\n' + '\n'.join(function(shared, item) for item in functions) + '\n' + prefix + r'''
 opened=name.data();++opens;binkOpened=false;return 9;
}
static void Check(comGameId_t id,const char *input,const char *expected,bool bink=false){
 profile=id;cinTable={};opened.clear();opens=0;
 assert(CIN_PlayCinematic(input,11,22,320,240,0)==9);
 assert(opens==1&&opened==expected&&binkOpened==bink);
}
int main(){
 for(auto id:{COM_GAME_JKA_SP,COM_GAME_JKA_MP}){
  Check(id,"ja01","video/ja01.roq");Check(id,"video/ja01","video/ja01.roq");
  Check(id,"video\\ja01","video\\ja01.roq");Check(id,"video.dir/ja01","video.dir/ja01.roq");
  Check(id,"ja01.roq","video/ja01.roq");Check(id,"video/ja01.ROQ","video/ja01.ROQ");
  Check(id,"video/ja01.avi","video/ja01.avi");Check(id,"video/ja01.","video/ja01.");
  // Both spellings resolve to one existing handle before any decoder/open.
  profile=id;cinTable={};std::strcpy(cinTable[3].fileName.data(),"video/ja01.roq");opens=0;
  for(const char *name:{"ja01","video/ja01","ja01.roq","video/ja01.roq"})
   assert(CIN_PlayCinematic(name,0,0,0,0,0)==3&&opens==0);
  assert(CIN_PlayCinematic("video/ja01",0,0,0,0,CIN_system)==9&&opens==1&&opened=="video/ja01.roq");
  // Exact full extension fits; extension or source truncation is rejected
  // before matching a possibly aliased cache key or reaching file opening.
  const auto last=std::string("v/")+std::string(MAX_OSPATH-7,'a');
  Check(id,last.c_str(),(last+".roq").c_str());
  const auto explicitLast=std::string("v/")+std::string(MAX_OSPATH-7,'a')+".roq";
  Check(id,explicitLast.c_str(),explicitLast.c_str());
  opens=0;for(const auto &bad:{last+"x",std::string(MAX_OSPATH,'a'),explicitLast+"x"})
   assert(CIN_PlayCinematic(bad.c_str(),0,0,0,0,0)==-1&&opens==0);
 }
 // Every other profile keeps its existing filename rule, including Elite
 // Force SP's default Bink and explicit RoQ selection.
 Check(COM_GAME_STVEF_SP,"st_01","video/st_01.bik",true);
 Check(COM_GAME_STVEF_SP,"video/st_01.BIK","video/st_01.BIK",true);
 Check(COM_GAME_STVEF_SP,"st_01.roq","video/st_01.roq");
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,
              COM_GAME_STVEF_MP,COM_GAME_QUAKE3_IHV,COM_GAME_Q3TEST,COM_GAME_JK2_SP,COM_GAME_JK2_MP,
              COM_GAME_ALICE,COM_GAME_FAKK2,COM_GAME_MOHAA}){
  Check(id,"ja01","video/ja01");Check(id,"video/ja01","video/ja01");
  Check(id,"ja01.roq","video/ja01.roq");
 }
 // Run the same shared helper on a physical record with neighboring canaries.
 std::array<byte,MAX_OSPATH+8> record;record.fill(0xa5);
 char *name=reinterpret_cast<char *>(record.data()+4);const auto path=std::string("v/")+std::string(MAX_OSPATH-7,'a');
 std::strcpy(name,path.c_str());COM_DefaultExtension(name,MAX_OSPATH,".roq");
 assert(std::string(name)==path+".roq"&&record[3]==0xa5&&record[MAX_OSPATH+4]==0xa5);
}
'''


class JKACinematicPathTests(unittest.TestCase):
    def test_actual_path_helpers_profile_scope_bounds_and_cache(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-cinematic-path'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / 'host.cpp'
            executable = directory / ('host.exe' if os.name == 'nt' else 'host')
            source.write_text(fixture())
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                    '-O2', '-ffast-math', '-Werror', '-D_CRT_SECURE_NO_WARNINGS',
                                    '-I', str(ROOT), str(source), '-o', str(executable)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
