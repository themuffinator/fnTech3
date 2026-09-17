"""Exercise RTCW write results, VM bounds, and save commits with real files."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/qcommon/filesystem_write_bounds.h"
#include "code/game/g_public.h"
#include "code/cgame/cg_public.h"
#include "code/ui/ui_public.h"
#include "code/compat/rtcw_module_abi.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
void QDECL Com_Error(errorParm_t code,const char*,...){throw static_cast<int>(code);}
void QDECL Com_Printf(const char*,...){}
void *VM_ExplicitArgPtr(const vm_t *vm,intptr_t address){
 if(!address)return nullptr;
 return vm->entryPoint||vm->dllExports?reinterpret_cast<void*>(address):vm->dataBase+address;
}
void VM_CheckBounds(const vm_t *vm,unsigned address,unsigned length){
 if(!vm->entryPoint&&!vm->dllExports && (address>=unsigned(vm->exactDataLength)||length>unsigned(vm->exactDataLength)-address))throw static_cast<int>(ERR_DROP);
}
template<class F>void rejects(F operation){bool failed=false;try{operation();}catch(int code){assert(code==ERR_DROP);failed=true;}assert(failed);}
'''


class RTCWFileWriteTests(unittest.TestCase):
    def execute(self, code):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/rtcw-manager'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='file-write-test-', dir=scratch) as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            cpp.write_text(code)
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() != 'x86':
                target = ['-DFNTECH3_Q3_ONLY']
            result = subprocess.run([compiler, *target, '-std=c++17',
                '-I', str(ROOT), str(cpp), '-o', str(exe)], cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_byte_results_owned_handles_and_actual_profile_dispatch(self):
        files = (ROOT / 'code/qcommon/files.c').read_text()
        code = PRELUDE + r'''
struct File {handleOwner_t owner=H_QAGAME; bool zipFile=false,handleSync=false;
 struct {union {void *v;FILE *o;} file;} handleFiles{};};
static File fsh[MAX_FILE_HANDLES];static bool fs_searchpaths=true;
static int writes=0,chunk=3,bytesBeforeFailure=INT_MAX;
static size_t controlledWrite(const void *data,size_t size,size_t count,FILE *file){
 assert(size==1);++writes;size_t n=std::min(count,size_t(std::min(chunk,bytesBeforeFailure)));
 bytesBeforeFailure-=int(n);return std::fwrite(data,1,n,file);
}
#define fwrite controlledWrite
''' + function(files, 'int FS_Write( const void *buffer') + r'''
#undef fwrite
''' + function(files, 'int FS_VM_WriteFile(') + r'''
static std::string renamedFrom,renamedTo;static int renames;
void FS_VM_RenameFile(const char *a,const char *b){++renames;renamedFrom=a;renamedTo=b;}
#include "code/qcommon/vm_rtcw_filesystem.hpp"
static vm_t game{},cgame{},ui{};static vm_t *gvm=&game,*cgvm=&cgame,*uivm=&ui;
static int profile;
int Com_GameIsRTCW(){return profile>=2;}
#undef VMA
#define VMA(i) VM_ExplicitArgPtr(vm,args[i])
'''
        for title, path, case, vm in [
            ('game', 'code/server/sv_game.cpp', 'G_FS_WRITE', 'gvm'),
            ('cgame', 'code/client/cl_cgame.cpp', 'CG_FS_WRITE', 'cgvm'),
            ('ui', 'code/client/cl_ui.cpp', 'UI_FS_WRITE', 'uivm')]:
            source = (ROOT / path).read_text()
            start = source.index('\tcase ' + case + ':')
            snippet = source[start:source.index('\n\tcase ', start + 1)]
            code += f'intptr_t {title}Write(intptr_t *args){{auto *vm={vm};switch(args[0]){{{snippet}\n default:return -99;}}}}\n'
        source = (ROOT / 'code/server/sv_game.cpp').read_text()
        start = source.index('\t\tcase Call::G_FS_RENAME:')
        snippet = source[start:source.index('\n\t\tcase ', start + 1)]
        code += 'intptr_t gameRename(intptr_t *args){using fntech3::rtcw::module::Call;switch(Call::G_FS_RENAME){' + snippet + '\ndefault:return -99;}}\n'
        code += r'''
static void openFile(){if(fsh[1].handleFiles.file.o)fclose(fsh[1].handleFiles.file.o);
 fsh[1]=File{};fsh[1].handleFiles.file.o=fopen("output.bin","w+b");assert(fsh[1].handleFiles.file.o);
 bytesBeforeFailure=INT_MAX;writes=0;}
static long fileLength(){fflush(fsh[1].handleFiles.file.o);return ftell(fsh[1].handleFiles.file.o);}
int main(){
 game.entryPoint=reinterpret_cast<decltype(game.entryPoint)>(1);cgame=ui=game;
 game.index=VM_GAME;cgame.index=VM_CGAME;ui.index=VM_UI;
 char data[]="autosave-bytes";const int count=sizeof(data);
 for(profile=0;profile<4;++profile){
  for(int module=0;module<3;++module){
   openFile();fsh[1].owner=module==0?H_QAGAME:module==1?H_CGAME:H_Q3UI;
   auto call=module==0?gameWrite:module==1?cgameWrite:uiWrite;
   intptr_t args[]={module==0?G_FS_WRITE:module==1?CG_FS_WRITE:UI_FS_WRITE,reinterpret_cast<intptr_t>(data),count,1};
   assert(call(args)==(profile>=2&&module<2?count:0));assert(fileLength()==count&&writes>1);
   rewind(fsh[1].handleFiles.file.o);char readback[sizeof(data)]{};
   assert(fread(readback,1,count,fsh[1].handleFiles.file.o)==count&&!memcmp(readback,data,count));
   openFile();fsh[1].owner=module==0?H_QAGAME:module==1?H_CGAME:H_Q3UI;bytesBeforeFailure=4;
   assert(call(args)==0&&fileLength()==4); // Real partial failure must not become success.
  }
 }
 openFile();assert(FS_VM_WriteFile(data,count,1,H_CGAME)==0&&!writes);
 for(int handle:{INT_MIN,-1,0,MAX_FILE_HANDLES,INT_MAX})assert(!FS_VM_WriteFile(data,count,handle,H_QAGAME));
 assert(!FS_VM_WriteFile(nullptr,1,1,H_QAGAME)&&!FS_VM_WriteFile(data,-1,1,H_QAGAME)&&!writes);
 assert(!VM_RTCWWriteFile(&game,0,0,1,H_QAGAME)&&!writes);
 fsh[1].zipFile=true;assert(!VM_RTCWWriteFile(&game,reinterpret_cast<intptr_t>(data),count,1,H_QAGAME));fsh[1].zipFile=false;
 fsh[1].handleSync=true;assert(VM_RTCWWriteFile(&game,reinterpret_cast<intptr_t>(data),count,1,H_QAGAME)==count);
 fclose(fsh[1].handleFiles.file.o);fsh[1].handleFiles.file.o=nullptr;
 assert(!VM_RTCWWriteFile(&game,reinterpret_cast<intptr_t>(data),count,1,H_QAGAME));
 rejects([&]{VM_RTCWWriteFile(nullptr,0,0,0,H_QAGAME);});
 rejects([&]{VM_RTCWWriteFile(&game,0,1,1,H_QAGAME);});
 rejects([&]{VM_RTCWWriteFile(&game,reinterpret_cast<intptr_t>(data),-1,1,H_QAGAME);});
 rejects([&]{VM_RTCWWriteFile(&game,-2,4,1,H_QAGAME);});
 if(sizeof(intptr_t)>4){
  const intptr_t large=static_cast<intptr_t>(std::int64_t(INT_MAX)+1);
  rejects([&]{VM_RTCWWriteFile(&game,reinterpret_cast<intptr_t>(data),large,1,H_QAGAME);});
  rejects([&]{VM_RTCWWriteFile(&game,reinterpret_cast<intptr_t>(data),1,large,H_QAGAME);});
 }
 byte bytes[192]{};vm_t qvm{};qvm.dataBase=bytes;qvm.dataMask=255;qvm.exactDataLength=192;
 openFile();memcpy(bytes+188,"tail",4);assert(VM_RTCWWriteFile(&qvm,188,4,1,H_QAGAME)==4);
 rejects([&]{VM_RTCWWriteFile(&qvm,188,5,1,H_QAGAME);});
 rejects([&]{VM_RTCWWriteFile(&qvm,192,1,1,H_QAGAME);});
 rejects([&]{VM_RTCWWriteFile(&qvm,-1,1,1,H_QAGAME);});
 fclose(fsh[1].handleFiles.file.o);fsh[1].handleFiles.file.o=nullptr;
 strcpy(reinterpret_cast<char*>(bytes+1),"save\\temp.svg");strcpy(reinterpret_cast<char*>(bytes+80),"save\\autosave\\escape1.svg");
 gvm=&qvm;intptr_t names[]={0,1,80};assert(!gameRename(names));
 assert(renames==1&&renamedFrom=="save\\temp.svg"&&renamedTo=="save\\autosave\\escape1.svg");
 VM_RTCWRenameFile(&qvm,1,1);assert(renames==2&&renamedFrom==renamedTo);
 memset(bytes+1,'x',MAX_QPATH);rejects([&]{VM_RTCWRenameFile(&qvm,1,80);});assert(renames==2);
 rejects([&]{VM_RTCWRenameFile(&qvm,0,80);});memset(bytes+188,'x',4);
 rejects([&]{VM_RTCWRenameFile(&qvm,188,80);});assert(renames==2);
}
'''
        self.execute(code)

    def test_save_rename_success_and_failures_preserve_existing_files(self):
        files = (ROOT / 'code/qcommon/files.c').read_text()
        code = PRELUDE + r'''
static bool fs_searchpaths=true;static cvar_t home{};static cvar_t *fs_homepath=&home;
static char fs_gamedir[]="main";static bool failReplace=false,failCreate=false;static int replacements;
void Q_strncpyz(char *out,const char *in,int size){snprintf(out,size,"%s",in);}
static std::string normalize(const char *name){std::string out=name;for(auto &c:out){if(c=='\\')c='/';}return out;}
qboolean FS_FilenameCompare(const char *a,const char *b){return normalize(a)==normalize(b)?qfalse:qtrue;}
char *FS_BuildOSPath(const char *base,const char *game,const char *file){
 static std::string scratch;scratch=(std::filesystem::path(base)/game/normalize(file)).string();return scratch.data();
}
static void FS_CheckFilenameIsNotAllowed(const char *name,const char *,qboolean){
 std::string value=name;if(value.find(".dll")!=std::string::npos||value.find(".pk3")!=std::string::npos)throw static_cast<int>(ERR_DROP);
}
qboolean FS_CreatePath(char *path){
 if(failCreate)return qtrue;std::error_code error;std::filesystem::create_directories(std::filesystem::path(path).parent_path(),error);
 return error?qtrue:qfalse;
}
qboolean Sys_ReplaceFile(const char *from,const char *to){
 ++replacements;if(failReplace)return qfalse;
#ifdef _WIN32
 const auto a=std::filesystem::path(from).wstring(),b=std::filesystem::path(to).wstring();
 return MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING)?qtrue:qfalse;
#else
 return rename(from,to)==0?qtrue:qfalse;
#endif
}
''' + function(files, 'void FS_VM_RenameFile(') + r'''
static std::string contents(const char *name){std::ifstream f(name,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});}
int main(){
 std::string base=std::filesystem::current_path().string();home.string=base.data();
 std::filesystem::create_directories("main/save");std::ofstream("main/save/temp.svg")<<"new complete autosave";
 FS_VM_RenameFile("save\\temp.svg","save\\autosave\\escape1.svg");
 assert(!std::filesystem::exists("main/save/temp.svg")&&contents("main/save/autosave/escape1.svg")=="new complete autosave");
 std::ofstream("main/save/temp.svg")<<"replacement";
 FS_VM_RenameFile("save/temp.svg","save/autosave/escape1.svg");assert(contents("main/save/autosave/escape1.svg")=="replacement");
 const int before=replacements;FS_VM_RenameFile("save/autosave/escape1.svg","save\\autosave\\escape1.svg");
 assert(replacements==before&&contents("main/save/autosave/escape1.svg")=="replacement");
 std::ofstream("main/save/temp.svg")<<"newer";failReplace=true;
 rejects([]{FS_VM_RenameFile("save/temp.svg","save/autosave/escape1.svg");});
 assert(contents("main/save/temp.svg")=="newer"&&contents("main/save/autosave/escape1.svg")=="replacement");
 failReplace=false;failCreate=true;rejects([]{FS_VM_RenameFile("save/temp.svg","save/autosave/escape1.svg");});
 failCreate=false;rejects([]{FS_VM_RenameFile("save/missing.svg","save/autosave/escape1.svg");});
 assert(contents("main/save/temp.svg")=="newer"&&contents("main/save/autosave/escape1.svg")=="replacement");
 for(const char *bad:{"", "/absolute.svg", "\\absolute.svg", "../escape.svg", "x\\..\\escape.svg", "E:escape.svg", "x/.. /escape.svg", "module.dll", "mod.pk3"}){
  rejects([&]{FS_VM_RenameFile(bad,"save/autosave/escape1.svg");});
  rejects([&]{FS_VM_RenameFile("save/temp.svg",bad);});
 }
 assert(contents("main/save/temp.svg")=="newer"&&contents("main/save/autosave/escape1.svg")=="replacement");
}
'''
        code = code.replace('#include <algorithm>', '#ifdef _WIN32\n#include <windows.h>\n#endif\n#include <algorithm>')
        self.execute(code)


if __name__ == '__main__':
    unittest.main()
