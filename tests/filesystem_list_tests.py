"""Execute file-list boundaries and packed mod lists for every real profile."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    source = (ROOT / 'code/qcommon/files.c').read_text(encoding='utf-8')
    compare = function((ROOT / 'code/qcommon/q_shared.c').read_text(encoding='utf-8'), 'int Q_stricmp(')
    return (r'''
#define _CRT_SECURE_NO_WARNINGS
#include "code/qcommon/game_manager.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
#undef assert
#define assert(value) do{if(!(value)){fprintf(stderr,"CHECK failed: %s at %d\n",#value,__LINE__);std::_Exit(3);}}while(0)
using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;
constexpr int MAX_OSPATH=256,FS_MATCH_ANY=1;
#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))
#define FNTECH3_ROOT_ARCHIVE_RENDERER_SHADER "scripts/fnql.shader"
const comGameProfile_t *selected;
struct cvar_t{const char *string;};
cvar_t base{"base"},home{"home"},steam{""};
cvar_t *fs_basepath=&base,*fs_homepath=&home,*fs_steampath=&steam;
int listed=0,freed=0,rootReads=0,allocations=0;
bool rootAvailable=true,existingShader=false;
// INSERT_COMPARE
bool FS_FilenameCompare(const char *a,const char *b){return Q_stricmp(a,b)!=0;}
struct Allocation{void *memory;template<class T>operator T*()const{return static_cast<T*>(memory);}};
Allocation Z_Malloc(size_t bytes){++allocations;auto p=calloc(1,bytes);assert(p);return {p};}
void Z_Free(void *p){if(p){--allocations;free(p);}}
char *CopyString(const char *s){auto p=static_cast<char*>(Z_Malloc(strlen(s)+1));strcpy(p,s);return p;}
char **list(std::initializer_list<const char*> values,int *count){
 *count=int(values.size());if(!*count)return nullptr;
 auto out=static_cast<char**>(Z_Malloc((values.size()+1)*sizeof(char*)));
 int i=0;for(auto value:values)out[i++]=CopyString(value);return out;
}
void Sys_FreeFileList(char **items){if(items){for(int i=0;items[i];++i)Z_Free(items[i]);Z_Free(items);}}
char **FS_ListFilteredFiles(const char *path,const char *extension,const char *,int *count,int){
 ++listed;assert(count);*count=0;
 if(!path)return nullptr;
 if(!strcmp(path,"scripts"))return existingShader?list({"fnql.shader"},count):list({"custom.shader"},count);
 return list({"one.cfg","two.cfg"},count);
}
int FS_ReadFileFromRootArchive(const char *path,void*){
 ++rootReads;assert(!strcmp(path,FNTECH3_ROOT_ARCHIVE_RENDERER_SHADER));return rootAvailable?6:-1;
}
void FS_FreeFileList(char **items){++freed;Sys_FreeFileList(items);}
unsigned Sys_CountFileList(char **items){unsigned n=0;if(items)while(items[n])++n;return n;}
char **FS_ConcatenateFileLists(char **a,char **b){
 auto n=Sys_CountFileList(a),m=Sys_CountFileList(b);
 auto out=static_cast<char**>(Z_Malloc((n+m+1)*sizeof(char*)));
 for(unsigned i=0;i<n;++i)out[i]=a[i];for(unsigned i=0;i<m;++i)out[n+i]=b[i];
 Z_Free(a);Z_Free(b);return out;
}
void FS_SortFileList(char **items,int last){
 std::sort(items,items+last+1,[](const char *a,const char *b){return strcmp(a,b)<0;});
}
bool FS_IsBaseGame(const char *name){return !strcmp(name,selected->baseGame);}
bool FS_IsExt(const char *name,const char *ext,size_t length){
 auto n=strlen(ext);return length>=n&&!strcmp(name+length-n,ext);
}
const char *FS_BuildOSPath(const char *path,const char *name,const char *){
 static std::string joined;joined=std::string(path)+"/"+name;return joined.c_str();
}
char **Sys_ListFiles(const char *path,const char *ext,const char *,int *count,int){
 ++listed;
 if(!strcmp(path,"base")&&!strcmp(ext,"/"))return list({selected->baseGame,"fixturemod","fixturemod",".",".."},count);
 if(!strcmp(path,"home")&&!strcmp(ext,"/"))return list({"homeonly"},count);
 if(!strcmp(path,"base/fixturemod")&&!strcmp(ext,".pk3"))return list({"addon.pk3"},count);
 if(!strcmp(path,"home/homeonly")&&!strcmp(ext,".pak"))return list({"addon.pak"},count);
 *count=0;return nullptr;
}
void FS_GetModDescription(const char *name,char *out,int size){snprintf(out,size,"%s description",name);}
static int FS_GetModList(char*,int);
''' + function(source, 'char **FS_ListFiles(') + '\n' + function(
        source, 'int\tFS_GetFileList(') + '\n' + function(
        source, 'static int FS_GetModList( char *listbuf, int bufsize ) {') + r'''
int main(int argc,char **argv){
 assert(argc==2);
 for(unsigned profile=0;profile<Com_GameManagerProfileCount();++profile){
  selected=Com_GameManagerProfileAt(profile);assert(selected);
  if(!strcmp(argv[1],"invalid-output")){
   for(int size:{0,-1,INT_MIN})for(const char *path:{"scripts","$modlist"}){
    char bytes[16];memset(bytes,0x5a,sizeof(bytes));int calls=listed;
    assert(FS_GetFileList(path,".cfg",bytes,size)==0);
    for(char byte:bytes)assert(byte==0x5a);assert(listed==calls);
    assert(FS_GetFileList(path,".cfg",nullptr,size)==0);
    assert(FS_GetModList(bytes,size)==0&&listed==calls);
    for(char byte:bytes)assert(byte==0x5a);
   }
   int calls=listed;assert(!FS_ListFiles("scripts",".shader",nullptr)&&listed==calls);
   assert(FS_GetFileList("scripts",".cfg",nullptr,16)==0);
   assert(FS_GetModList(nullptr,16)==0);
  }else if(!strcmp(argv[1],"null-queries")){
   int count=99,reads=rootReads;
   auto files=FS_ListFiles(nullptr,".shader",&count);assert(!files&&count==0&&rootReads==reads);
   files=FS_ListFiles("scripts",nullptr,&count);
   assert(count==1&&!strcmp(files[0],"custom.shader")&&rootReads==reads);FS_FreeFileList(files);
   char bytes[16];memset(bytes,0x5a,sizeof(bytes));
   assert(FS_GetFileList(nullptr,".cfg",bytes,sizeof(bytes))==0&&bytes[0]==0);
   for(unsigned i=1;i<sizeof(bytes);++i)assert(bytes[i]==0x5a);
  }else if(!strcmp(argv[1],"listing")){
   int count=0;existingShader=false;rootAvailable=true;
   auto files=FS_ListFiles("scripts",".shader",&count);
   assert(count==2&&!strcmp(files[0],"custom.shader")&&!strcmp(files[1],"fnql.shader")&&!files[2]);FS_FreeFileList(files);
   existingShader=true;files=FS_ListFiles("scripts",".shader",&count);assert(count==1);FS_FreeFileList(files);
   existingShader=false;rootAvailable=false;files=FS_ListFiles("scripts",".shader",&count);assert(count==1);FS_FreeFileList(files);
   rootAvailable=true;
   for(int size:{1,9,10,17,18}){
    char bytes[32];memset(bytes,0x5a,sizeof(bytes));
    int expected=size>=18?2:size>=10?1:0;
    assert(FS_GetFileList("configs",".cfg",bytes,size)==expected);
    if(!expected)assert(bytes[0]==0);
    else assert(!memcmp(bytes,"one.cfg\0",8));
    if(expected==2)assert(!memcmp(bytes+8,"two.cfg\0",8));
    for(unsigned i=unsigned(size);i<sizeof(bytes);++i)assert(bytes[i]==0x5a);
   }
  }else if(!strcmp(argv[1],"modlist")){
   char bytes[128];memset(bytes,0x5a,sizeof(bytes));
   assert(FS_GetFileList("$modlist","",bytes,sizeof(bytes))==2);
   const char expected[]="fixturemod\0fixturemod description\0homeonly\0homeonly description";
   assert(!memcmp(bytes,expected,sizeof(expected))&&bytes[sizeof(expected)]==0x5a);
   for(int size:{1,16,32}){
    memset(bytes,0x5a,sizeof(bytes));assert(FS_GetFileList("$modlist","",bytes,size)==0&&bytes[0]==0);
    for(unsigned i=unsigned(size);i<sizeof(bytes);++i)assert(bytes[i]==0x5a);
   }
  }else assert(false);
  assert(allocations==0);
 }
 printf("%s: checked all %u profiles\n",argv[1],Com_GameManagerProfileCount());
}
''').replace('// INSERT_COMPARE', compare)


class FilesystemListTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            raise unittest.SkipTest('C++17 compiler unavailable')
        cls.temporary = tempfile.TemporaryDirectory(prefix='filesystem-list-', dir=ROOT / '.tmp')
        cls.addClassCleanup(cls.temporary.cleanup)
        directory = Path(cls.temporary.name)
        source = directory / 'lists.cpp'
        source.write_text(fixture(), encoding='utf-8')
        cls.executable = directory / ('lists.exe' if os.name == 'nt' else 'lists')
        flags = ['-DFNTECH3_' + name + '_DEVELOPMENT' for name in
                 ('RTCW', 'WOLFET', 'STVEF', 'Q3_IHV', 'JKA', 'JK2', 'ALICE', 'FAKK2', 'MOHAA', 'Q3TEST')]
        result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1',
                                 *flags, '-I', str(ROOT), str(source),
                                 str(ROOT / 'code/qcommon/game_profile.cpp'), '-o', str(cls.executable)],
                                cwd=directory, capture_output=True, text=True, timeout=120)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def execute(self, case):
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        result = subprocess.run([str(self.executable), case], cwd=ROOT,
                                capture_output=True, text=True, timeout=20, **options)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('checked all 16 profiles', result.stdout)

    def test_invalid_output_does_not_write_or_enumerate(self):
        self.execute('invalid-output')

    def test_nullable_queries_preserve_filtered_list_contract(self):
        self.execute('null-queries')

    def test_shader_and_terminated_file_lists_keep_existing_behavior(self):
        self.execute('listing')

    def test_packed_mod_list_keeps_profile_base_exclusions_and_bounds(self):
        self.execute('modlist')


if __name__ == '__main__':
    unittest.main()
