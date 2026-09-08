"""Execute rename fallback against real writable and unwritable destinations."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]


class RenameFallbackTests(unittest.TestCase):
    def test_source_survives_copy_failure(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        source=(ROOT/'code/qcommon/files.c').read_text()
        pieces=[]
        for name in ('FS_CopyFile','FS_SV_Rename','FS_Rename'):
            import re
            match=re.search(r'(?:static )?(?:void|qboolean) '+name+r'\( const char \*',source)
            self.assertIsNotNone(match)
            start=match.start();pieces.append(source[start:source.index('\n}',start)+2])
        harness=r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
using byte=unsigned char;using qboolean=bool;
constexpr bool qtrue=true,qfalse=false;
constexpr int ERR_FATAL=1,ERR_DROP=2,MAX_OSPATH=256;
struct Cvar{const char*string;int integer;};
Cvar home{},debug{};auto fs_homepath=&home;auto fs_debug=&debug;
bool fs_searchpaths=true;const char*fs_gamedir="";
bool replaceWorks=false;int closeCalls=0,failCloseAt=0;
int checkedClose(FILE*f){int result=fclose(f);return ++closeCalls==failCloseAt?EOF:result;}
#define fclose checkedClose
void Q_strncpyz(char*d,const char*s,int n){snprintf(d,n,"%s",s);}
void Com_Printf(const char*,...){}
[[noreturn]] void Com_Error(int severity,const char*,...){throw severity;}
FILE*Sys_FOpen(const char*p,const char*m){return fopen(p,m);}
int FS_FileLength(FILE*f){fseek(f,0,SEEK_END);int n=int(ftell(f));rewind(f);return n;}
bool FS_CreatePath(const char*){return true;}
void FS_Remove(const char*p){remove(p);}
bool FS_WriteQpathIsValid(const char*){return true;}
void FS_CheckFilenameIsNotAllowed(const char*,const char*,bool){}
const char*FS_BuildOSPath(const char*base,const char*game,const char*file){static std::string s;auto path=std::filesystem::path(base);if(*game)path/=game;if(file&&*file)path/=file;s=path.string();return s.c_str();}
bool Sys_ReplaceFile(const char*a,const char*b){return replaceWorks&&rename(a,b)==0;}
#define malloc(n) static_cast<byte*>(std::malloc(n))
'''+ '\n'.join(pieces)+r'''
int main(int argc,char**argv){
 assert(argc==2);std::string root=argv[1];home.string=root.c_str();
 auto from=std::filesystem::path(root)/"source.tmp";auto to=std::filesystem::path(root)/"destination";
 for(bool server:{false,true}){
  std::filesystem::create_directory(to);std::ofstream(to/"sentinel")<<"keep";
  std::ofstream(from)<<"complete download";
  bool dropped=false;try{if(server)FS_SV_Rename("source.tmp","destination");else FS_Rename("source.tmp","destination");}catch(int e){assert(e==ERR_DROP);dropped=true;}
  assert(std::filesystem::exists(from));
  assert(dropped&&std::filesystem::exists(to/"sentinel"));
  std::ifstream input(from);std::string contents;std::getline(input,contents);input.close();assert(contents=="complete download");
  std::filesystem::remove(to/"sentinel");std::filesystem::remove(to);
  if(server)FS_SV_Rename("source.tmp","destination");else FS_Rename("source.tmp","destination");
  assert(!std::filesystem::exists(from));std::ifstream result(to);std::getline(result,contents);result.close();assert(contents=="complete download");
  std::filesystem::remove(to);
  std::ofstream(from)<<"close failure";closeCalls=0;failCloseAt=server?2:3;
  dropped=false;try{if(server)FS_SV_Rename("source.tmp","destination");else FS_Rename("source.tmp","destination");}catch(int e){assert(e==ERR_DROP);dropped=true;}
  assert(dropped&&std::filesystem::exists(from));failCloseAt=0;
  std::filesystem::remove(to);std::filesystem::remove(from);
  std::ofstream(from).close();
  if(server)FS_SV_Rename("source.tmp","destination");else FS_Rename("source.tmp","destination");
  assert(!std::filesystem::exists(from)&&std::filesystem::file_size(to)==0);std::filesystem::remove(to);
  replaceWorks=true;std::ofstream(from)<<"direct";
  if(server)FS_SV_Rename("source.tmp","destination");else FS_Rename("source.tmp","destination");
  assert(!std::filesystem::exists(from)&&std::filesystem::exists(to));std::filesystem::remove(to);replaceWorks=false;
 }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';fixture=Path(folder)/'fixture';fixture.mkdir();cpp.write_text(harness)
            build=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe),str(fixture)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
