"""Execute Windows path boundaries with disposable Unicode files and a test DLL."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from process_relaunch_tests import function

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/win32/win_utf8.h"
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
void QDECL Com_Error(errorParm_t,const char*,...) { std::abort(); }
int QDECL Com_sprintf(char *out,int size,const char *format,...) {
 va_list args; va_start(args,format); int n=vsnprintf(out,size,format,args); va_end(args); return n;
}
char *FS_CopyString(const char *value) {
 auto size=strlen(value)+1; auto copy=static_cast<char*>(malloc(size)); memcpy(copy,value,size); return copy;
}
qboolean FS_AllowedExtension(const char *,qboolean,const char **) { return qfalse; }
qboolean Com_FilterExt(const char *,const char *) { std::abort(); }
int Com_FilterPath(const char *,const char *) { std::abort(); }
static int closedKeys;
namespace fnql::win {
class ScopedRegistryKey {
 HKEY value=nullptr;
public:
 HKEY *receive() { return &value; }
 HKEY get() { return value; }
 ~ScopedRegistryKey() { if(value) ++closedKeys; }
};
}
static bool keyMissing,queryFails;
static DWORD registryType,registryBytes;
static std::vector<wchar_t> registryValue;
LSTATUS FixtureOpen(HKEY,const wchar_t *key,DWORD,REGSAM,HKEY *out) {
 assert(!wcscmp(key,L"fixture 東京"));
 if(keyMissing) return ERROR_FILE_NOT_FOUND;
 *out=reinterpret_cast<HKEY>(uintptr_t(42)); return ERROR_SUCCESS;
}
LSTATUS FixtureQuery(HKEY,const wchar_t *name,LPDWORD,LPDWORD type,LPBYTE data,LPDWORD size) {
 assert(!wcscmp(name,L"path"));
 if(queryFails) return ERROR_ACCESS_DENIED;
 *type=registryType;
 if(registryBytes>*size) { *size=registryBytes; return ERROR_MORE_DATA; }
 memcpy(data,registryValue.data(),registryBytes); *size=registryBytes; return ERROR_SUCCESS;
}
void Sys_NormalizeSteamPath(char *path) { assert(path); }
'''
HARNESS = r'''
static void Write(const std::string &path,const char *value) {
 FILE *file=Sys_FOpen(path.c_str(),"wb"); assert(file);
 assert(fwrite(value,1,strlen(value),file)==strlen(value)); assert(!fclose(file));
}
static void Registry(const std::wstring &value,DWORD type=REG_SZ,bool terminate=true) {
 registryValue.assign(value.begin(),value.end()); if(terminate) registryValue.push_back(0);
 registryBytes=DWORD(registryValue.size()*sizeof(wchar_t)); registryType=type;
 keyMissing=queryFails=false; closedKeys=0;
}
int main() {
 const auto working=std::filesystem::current_path();
 const auto root=working/std::filesystem::u8path(u8"profile Wölf 東京 🐺 +set a 1;");
 const auto directory=root.u8string();
 assert(Sys_Mkdir(directory.c_str()) && Sys_Mkdir(directory.c_str()));
 assert(std::filesystem::is_directory(root));
 const auto first=(root/std::filesystem::u8path(u8"wolf_東京.cfg")).u8string();
 const auto next=(root/std::filesystem::u8path(u8"wolf_東京.tmp")).u8string();
 Write(first,"old"); Write(next,"new-value");
 assert(Sys_ReplaceFile(next.c_str(),first.c_str()));
 assert(!std::filesystem::exists(std::filesystem::u8path(next)));
 FILE *file=Sys_FOpen(first.c_str(),"rb"); assert(file);
 char text[16]={}; assert(fread(text,1,sizeof(text),file)==9 && !strcmp(text,"new-value")); fclose(file);
 fileOffset_t size=-1; fileTime_t modified=0,created=0;
 assert(Sys_GetFileStats(first.c_str(),&size,&modified,&created) && size==9 && modified>0 && created>0);
 const fntech3::win::Path wideFile(first.c_str());
 assert(SetFileAttributesW(wideFile.c_str(),FILE_ATTRIBUTE_READONLY));
 assert(Sys_ResetReadOnlyAttribute(first.c_str()));
 assert(!(GetFileAttributesW(wideFile.c_str()) & FILE_ATTRIBUTE_READONLY));
 assert(!Sys_ResetReadOnlyAttribute(first.c_str()));
 assert(!Sys_ResetReadOnlyAttribute(next.c_str()));
 size=99; modified=created=99;
 assert(!Sys_GetFileStats(next.c_str(),&size,&modified,&created) && !size && !modified && !created);
 const auto upper=(root/"UPPER.CFG").u8string(); Write(upper,"upper");
 const auto sub=root/std::filesystem::u8path(u8"子目录");
 assert(Sys_Mkdir(sub.u8string().c_str()));
 Write((sub/"nested.cfg").u8string(),"nested");
 const std::set<std::string> expected={u8"wolf_東京.cfg","UPPER.CFG",u8"子目录\\nested.cfg"};
 for(int capacity=0;capacity<=4;++capacity) {
  char *list[5]={}; list[capacity]=reinterpret_cast<char*>(uintptr_t(7));
  const int count=Sys_ListExtFiles(directory.c_str(),"",".cfg",nullptr,list,capacity,1);
  assert(count==(capacity<3 ? capacity : 3));
  assert(list[capacity]==reinterpret_cast<char*>(uintptr_t(7)));
  std::set<std::string> found;
  for(int i=0;i<count;++i) { assert(expected.count(list[i])); found.insert(list[i]); free(list[i]); }
  if(capacity>=3) assert(found==expected);
 }
 char *list[4]={};
 assert(Sys_ListExtFiles(directory.c_str(),"",".cfg",nullptr,list,4,0)==2);
 free(list[0]); free(list[1]);
 assert(Sys_ListExtFiles("\xff","",".cfg",nullptr,list,4,0)==0);
 for(const char *bad : {"\xff", "\xc0\xaf", "\xed\xa0\x80"}) {
  assert(!Sys_Mkdir(bad) && !Sys_FOpen(bad,"wb") && Sys_Remove(bad)==-1);
  assert(!Sys_ReplaceFile(first.c_str(),bad) && std::filesystem::exists(std::filesystem::u8path(first)));
 }
 assert(!Sys_FOpen(nullptr,"wb") && !Sys_FOpen(first.c_str(),nullptr));
 assert(!Sys_FOpen((first+".").c_str(),"wb") && !Sys_FOpen((first+" ").c_str(),"wb"));
 wchar_t smallWide[2]={L'x',L'y'}; char smallUtf8[2]={'x','y'};
 assert(!fntech3::win::ToWide(u8"東京",smallWide) && smallWide[0]==L'x' && smallWide[1]==L'y');
 assert(!fntech3::win::ToUtf8(L"東京",smallUtf8) && smallUtf8[0]=='x' && smallUtf8[1]=='y');
 const wchar_t badWide[]={0xd800,0};
 assert(!fntech3::win::ToUtf8(badWide,smallUtf8) && smallUtf8[0]=='x' && smallUtf8[1]=='y');
 char longName[1100]; memset(longName,'a',sizeof(longName)-1); longName[sizeof(longName)-1]=0;
 assert(!Sys_FOpen(longName,"wb"));
 // The actual DLL loader must resolve Unicode cache/image names too.
 const auto dll=(working/std::filesystem::u8path(u8"native 東京.dll")).u8string();
 auto module=static_cast<HMODULE>(Sys_LoadLibrary(dll.c_str())); assert(module);
 auto address=GetProcAddress(module,"FnTech3Probe"); if(!address) address=GetProcAddress(module,"_FnTech3Probe");
 assert(address && reinterpret_cast<int(*)()>(address)()==17); assert(FreeLibrary(module));
 wchar_t executable[1024]; assert(GetModuleFileNameW(nullptr,executable,1024));
 assert(std::filesystem::u8path(Sys_Pwd())==std::filesystem::path(executable).parent_path());
 assert(Sys_Remove(first.c_str())==0 && !std::filesystem::exists(std::filesystem::u8path(first)));
 assert(Sys_Remove(first.c_str())==-1);
 // Registry reads are mocked: this test never writes the user's registry.
 char output[256];
 Registry(L"E:\\Wölf 東京"); memset(output,0x5a,sizeof(output));
 assert(Sys_QueryRegistryString(nullptr,u8"fixture 東京",0,"path",output,sizeof(output)));
 assert(!strcmp(output,u8"E:\\Wölf 東京") && closedKeys==1);
 Registry(L"E:\\Wölf 東京",REG_SZ,false);
 assert(Sys_QueryRegistryString(nullptr,u8"fixture 東京",0,"path",output,sizeof(output)));
 Registry(L"%FNTECH3_UTF8_PATH_FIXTURE%\\東京",REG_EXPAND_SZ);
 assert(SetEnvironmentVariableW(L"FNTECH3_UTF8_PATH_FIXTURE",L"E:\\Wölf"));
 assert(Sys_QueryRegistryString(nullptr,u8"fixture 東京",0,"path",output,sizeof(output)));
 assert(!strcmp(output,u8"E:\\Wölf\\東京"));
 assert(SetEnvironmentVariableW(L"FNTECH3_UTF8_PATH_FIXTURE",nullptr));
 for(int failure=0;failure<7;++failure) {
  Registry(L"fixture");
  if(failure==0) keyMissing=true;
  if(failure==1) queryFails=true;
  if(failure==2) registryType=REG_DWORD;
  if(failure==3) --registryBytes;
  if(failure==4) Registry(std::wstring(MAX_OSPATH,L'x'),REG_SZ,false);
  if(failure==5) Registry(std::wstring(MAX_OSPATH+1,L'x'));
  if(failure==6) Registry(std::wstring(1,0xd800));
  memset(output,0x5a,sizeof(output));
  assert(!Sys_QueryRegistryString(nullptr,u8"fixture 東京",0,"path",output,sizeof(output)));
  for(char c:output) assert(c==0x5a);
  assert(closedKeys==(failure==0 ? 0 : 1));
 }
 Registry(L"E:\\Wölf 東京"); memset(output,0x5a,sizeof(output));
 assert(!Sys_QueryRegistryString(nullptr,u8"fixture 東京",0,"path",output,2));
 for(char c:output) assert(c==0x5a);
}
'''


@unittest.skipUnless(os.name == 'nt', 'Windows UTF-8 filesystem boundary')
class WindowsUtf8FilesTests(unittest.TestCase):
    def test_files_scanning_native_loading_and_registry_boundaries(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        main = (ROOT / 'code/win32/win_main.cpp').read_text()
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
        common = (ROOT / 'code/qcommon/common.c').read_text()
        platform = (ROOT / 'code/win32/win_shared.cpp').read_text()
        source = PRELUDE
        for signature in ['void Q_strncpyz(', 'void Q_strcat(', 'int Q_stricmpn(', 'int Q_stricmp(', 'qboolean Q_streq(']:
            source += '\n' + function(shared, signature)
        source += '\n' + function(common, 'qboolean Com_HasPatterns(')
        for signature in ['qboolean Sys_Mkdir(', 'FILE *Sys_FOpen(', 'int Sys_Remove(',
                          'qboolean Sys_ReplaceFile(', 'qboolean Sys_ResetReadOnlyAttribute(',
                          'const char *Sys_Pwd(', 'static int Sys_ListExtFiles(',
                          'qboolean Sys_GetFileStats(', 'void *Sys_LoadLibrary(']:
            source += '\n' + function(main, signature)
        source += '\n#define RegOpenKeyExW FixtureOpen\n#define RegQueryValueExW FixtureQuery\n'
        source += function(platform, 'static qboolean Sys_QueryRegistryString(')
        source += '\n#undef RegOpenKeyExW\n#undef RegQueryValueExW\n' + HARNESS
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary_root = root / 'engine Wölf 東京'
            binary_root.mkdir()
            home = root / 'working'
            home.mkdir()
            cpp = root / 'files.cpp'
            cpp.write_text(source, encoding='utf-8')
            dll_cpp = root / 'fixture.cpp'
            dll_cpp.write_text('extern "C" __declspec(dllexport) int FnTech3Probe(){return 17;}\n')
            exe = binary_root / 'files.exe'
            target = compiler_target_args(compiler)
            abi = [] if target else ['-DFNTECH3_Q3_ONLY']
            for command in [
                [compiler, *target, '-shared', str(dll_cpp), '-o', str(home / 'native 東京.dll')],
                [compiler, *target, *abi, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                 str(cpp), '-o', str(exe)],
            ]:
                built = subprocess.run(command, cwd=root, capture_output=True, text=True, encoding='utf-8', errors='replace')
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], cwd=home, capture_output=True, text=True, timeout=15,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
