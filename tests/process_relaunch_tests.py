"""Exercise same-image relaunch with a console fixture, never the game client."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import time
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_manager.h"
#include "code/platform/process_relaunch.h"
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
void QDECL Com_Error(errorParm_t, const char *, ...) { std::abort(); }
void Sys_Error(const char *, ...) { std::abort(); }
#define MAX_CONSOLE_LINES 32
static int com_numConsoleLines;
static char *com_consoleLines[MAX_CONSOLE_LINES];
static char rconPassword2[1024];
static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static int cmd_argoffsets[MAX_STRING_TOKENS];
static qboolean cmd_quoted[MAX_STRING_TOKENS];
static char cmd_tokenized[BIG_INFO_STRING + MAX_STRING_TOKENS];
static char cmd_cmd[BIG_INFO_STRING];
'''

HARNESS = r'''
#ifdef _WIN32
static std::string Identity(const BY_HANDLE_FILE_INFORMATION &info) {
 return std::to_string(info.dwVolumeSerialNumber)+":"+std::to_string(info.nFileIndexHigh)+":"+std::to_string(info.nFileIndexLow);
}
#endif
static std::string Image() {
#ifdef _WIN32
 wchar_t path[32768]; DWORD size=GetModuleFileNameW(nullptr,path,32768);
 assert(size && size<32768); return std::filesystem::path(path).u8string();
#else
 char path[4096]; auto size=readlink("/proc/self/exe",path,sizeof(path));
 assert(size>0 && size<sizeof(path)); return std::string(path,size);
#endif
}
static void Units() {
 char error[256];
 assert(!Sys_HasPendingRelaunch());
 assert(!Sys_LaunchRelaunch(error,sizeof(error)) && error[0]);
 for(const char *value : {"", "a\rb", "a\nb", "a\tb", "a\x7f"}) {
  assert(!Sys_PrepareRelaunch(value,error,sizeof(error)) && error[0]);
  assert(!Sys_HasPendingRelaunch());
 }
 assert(!Sys_PrepareRelaunch(nullptr,error,sizeof(error)));
 const std::string tooLong(SYS_RELAUNCH_COMMAND_BYTES,'x');
 assert(!Sys_PrepareRelaunch(tooLong.c_str(),error,sizeof(error)));
#ifdef _WIN32
 assert(!Sys_PrepareRelaunch("\xff",error,sizeof(error)));
 for(const wchar_t *line : {L"\"E:\\A B\\engine.exe\" +set name \"Wölf 東京\\\"",
                           L"engine.exe +set path \"a;b + c\\\""}) {
  char output[1024]; std::memset(output,0x5a,sizeof(output));
  const wchar_t *arguments=wcsstr(line,L"+set");
  char expected[1024]; int bytes=WideCharToMultiByte(CP_UTF8,0,arguments,-1,expected,1024,nullptr,nullptr);
  assert(bytes>0);
  for(unsigned capacity=0;capacity<unsigned(bytes);++capacity) {
   assert(!Sys_DecodeStartupCommandLine(line,output,capacity));
   for(char c:output) assert(c==0x5a);
  }
  assert(Sys_DecodeStartupCommandLine(line,output,bytes));
  assert(!std::strcmp(output,expected) && output[bytes]==0x5a);
 }
 for(const wchar_t *line : {L"",L"engine.exe",L"\"E:\\A B\\engine.exe\" \t "}) {
  char output[2]={'x','y'};
  assert(Sys_DecodeStartupCommandLine(line,output,1) && !output[0] && output[1]=='y');
 }
 char output[4]={'a','b','c','d'};
 const wchar_t invalid[]={L'e',L' ',0xd800,0};
 assert(!Sys_DecodeStartupCommandLine(invalid,output,sizeof(output)));
 assert(!Sys_DecodeStartupCommandLine(nullptr,output,sizeof(output)));
 assert(!Sys_DecodeStartupCommandLine(L"e a",nullptr,2));
 assert(!std::memcmp(output,"abcd",4));
#endif
 assert(Sys_PrepareRelaunch("+set fixture \"quoted value\"",error,sizeof(error)) && !error[0]);
 assert(Sys_HasPendingRelaunch());
 assert(!Sys_PrepareRelaunch("+set different 1",error,sizeof(error)) && error[0]);
 assert(Sys_HasPendingRelaunch());
 Sys_CancelRelaunch(); Sys_CancelRelaunch();
 assert(!Sys_HasPendingRelaunch());
 assert(!Sys_LaunchRelaunch(nullptr,0));
}
int main(int argc,char **argv) {
 const auto root=std::filesystem::current_path();
 if(argc==2 && !std::strcmp(argv[1],"unit")) { Units(); return 0; }
 if(argc==3 && !std::strcmp(argv[1],"parent")) {
  const auto *target=Com_GameManagerFindProfile(argv[2]); assert(target);
  const auto *source=Com_GameProfileById(target->id==COM_GAME_RTCW_SP ? COM_GAME_RTCW_MP : COM_GAME_RTCW_SP);
  const std::string path=root.u8string()+char(std::filesystem::path::preferred_separator);
  char command[COM_GAME_MODE_LAUNCH_BYTES],error[256];
  assert(Com_GameManagerFormatModeLaunch(source,target,path.c_str(),path.c_str(),0,
   command,sizeof(command))==COM_GAME_MODE_LAUNCH_READY);
  // Deliberately inheritable sentinel: neither implementation may carry it
  // into the fresh process, even if an unrelated subsystem leaked it.
#ifdef _WIN32
  SECURITY_ATTRIBUTES attributes={sizeof(attributes),nullptr,TRUE};
  HANDLE descriptor=CreateFileW((root/"inherited.txt").c_str(),GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,CREATE_ALWAYS,0,nullptr);
  assert(descriptor!=INVALID_HANDLE_VALUE);
  assert(!_putenv_s("FNTECH3_RELAUNCH_FIXTURE_DESCRIPTOR",std::to_string(uintptr_t(descriptor)).c_str()));
  BY_HANDLE_FILE_INFORMATION info; assert(GetFileInformationByHandle(descriptor,&info));
  assert(!_putenv_s("FNTECH3_RELAUNCH_FIXTURE_IDENTITY",Identity(info).c_str()));
#else
  int descriptor=open((root/"inherited.txt").c_str(),O_RDWR|O_CREAT|O_TRUNC,0600); assert(descriptor>=3);
  assert(!setenv("FNTECH3_RELAUNCH_FIXTURE_DESCRIPTOR",std::to_string(descriptor).c_str(),1));
#endif
  assert(Sys_PrepareRelaunch(command,error,sizeof(error)));
  // The plan owns its arguments; current-profile data can be discarded now.
  std::memset(command,0x5a,sizeof(command));
  { std::ofstream ready(root/"ready.txt"); ready << "shutdown completed"; }
  assert(Sys_LaunchRelaunch(error,sizeof(error)) && !error[0]);
  assert(!Sys_HasPendingRelaunch());
#ifdef _WIN32
  CloseHandle(descriptor);
#endif
  return 0;
 }
 // Fresh process: consume the same entry-point command path as the engine.
 assert(!Sys_HasPendingRelaunch() && !Com_GameProfile());
 assert(std::filesystem::exists(root/"ready.txt"));
 const char *inherited=std::getenv("FNTECH3_RELAUNCH_FIXTURE_DESCRIPTOR"); assert(inherited);
#ifdef _WIN32
 HANDLE descriptor=reinterpret_cast<HANDLE>(uintptr_t(std::stoull(inherited)));
 BY_HANDLE_FILE_INFORMATION info;
 if(GetFileInformationByHandle(descriptor,&info)) {
  assert(Identity(info)!=std::getenv("FNTECH3_RELAUNCH_FIXTURE_IDENTITY"));
 }
 char command[SYS_RELAUNCH_COMMAND_BYTES];
 assert(Sys_DecodeStartupCommandLine(GetCommandLineW(),command,sizeof(command)));
 std::string raw=command;
#else
 assert(fcntl(std::stoi(inherited),F_GETFD)==-1 && errno==EBADF);
 std::vector<const char *> arguments(argv,argv+argc);
 std::string raw=Sys_BuildCommandLine(argc,arguments.data());
#endif
 std::string parsed=raw;
 Com_ParseCommandLine(parsed.data());
 assert(com_numConsoleLines==5);
 std::map<std::string,std::string> values;
 for(int i=0;i<com_numConsoleLines;++i) {
  Cmd_TokenizeString(com_consoleLines[i]);
  assert(cmd_argc==3 && !std::strcmp(cmd_argv[0],"set"));
  assert(values.emplace(cmd_argv[1],cmd_argv[2]).second);
 }
 assert(values.size()==5);
 const auto *target=Com_GameManagerFindProfile(values.at("com_gameProfile").c_str());
 assert(target && target->titleId==COM_GAME_TITLE_RTCW);
 const std::string path=root.u8string()+char(std::filesystem::path::preferred_separator);
 assert(values.at("fs_basepath")==path && values.at("fs_homepath")==path);
 assert(values.at("fs_game").empty() && values.at("r_fullscreen")=="0");
 { std::ofstream report(root/"report.tmp",std::ios::binary);
   report << Image() << '\n' << target->key << '\n' << raw << '\n';
#ifdef _WIN32
   report << GetCurrentProcessId() << '\n';
#else
   report << getpid() << '\n';
#endif
 }
 std::filesystem::rename(root/"report.tmp",root/"report.txt");
 return 0;
}
'''

FAILURES = r'''
#include <cassert>
#include <cerrno>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static bool failImage;
static bool failLaunch=true;
static HANDLE launchedProcess,launchedThread;
DWORD WINAPI FixtureImage(HMODULE module,LPWSTR name,DWORD size) {
 if(failImage) { SetLastError(ERROR_ACCESS_DENIED); return 0; }
 return GetModuleFileNameW(module,name,size);
}
BOOL WINAPI FixtureLaunch(LPCWSTR image,LPWSTR command,LPSECURITY_ATTRIBUTES,
 LPSECURITY_ATTRIBUTES,BOOL inherit,DWORD flags,LPVOID,LPCWSTR,
 LPSTARTUPINFOW, LPPROCESS_INFORMATION process) {
 assert(image && image[0] && command && command[0]==L'"');
 assert(!inherit && flags==0);
 if(failLaunch) { SetLastError(ERROR_FILE_NOT_FOUND); return FALSE; }
 process->hProcess=launchedProcess=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 process->hThread=launchedThread=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 assert(launchedProcess && launchedThread); return TRUE;
}
#define GetModuleFileNameW FixtureImage
#define CreateProcessW FixtureLaunch
#else
#include <unistd.h>
static bool failImage;
ssize_t FixtureImage(const char *path,char *destination,size_t size) {
 assert(!std::strcmp(path,"/proc/self/exe"));
 if(failImage) { errno=EACCES; return -1; }
 return readlink(path,destination,size);
}
int FixtureLaunch(const char *image,char *const argv[]) {
 assert(image && image[0]=='/' && argv[0]==image && argv[1] && !argv[2]);
 errno=ENOENT; return -1;
}
#define readlink FixtureImage
#define execv FixtureLaunch
#endif
#include "code/platform/process_relaunch.cpp"
int main() {
 char error[256]; failImage=true;
 assert(!Sys_PrepareRelaunch("+set fixture \"value\"",error,sizeof(error)) && error[0]);
 assert(!Sys_HasPendingRelaunch()); failImage=false;
 assert(Sys_PrepareRelaunch("+set fixture \"value\"",error,sizeof(error)));
 assert(!Sys_LaunchRelaunch(error,sizeof(error)) && error[0]);
 assert(!Sys_HasPendingRelaunch());
 assert(!Sys_LaunchRelaunch(error,sizeof(error)));
 assert(Sys_PrepareRelaunch("+set fixture \"value\"",error,sizeof(error)));
 Sys_CancelRelaunch(); assert(!Sys_HasPendingRelaunch());
#ifdef _WIN32
 failLaunch=false;
 assert(Sys_PrepareRelaunch("+set fixture \"value\"",error,sizeof(error)));
 assert(Sys_LaunchRelaunch(error,sizeof(error)) && !error[0]);
 assert(!Sys_HasPendingRelaunch());
 DWORD flags;
 assert(!GetHandleInformation(launchedProcess,&flags) && GetLastError()==ERROR_INVALID_HANDLE);
 assert(!GetHandleInformation(launchedThread,&flags) && GetLastError()==ERROR_INVALID_HANDLE);
#endif
}
'''


class ProcessRelaunchTests(unittest.TestCase):
    def compiler(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        if os.name != 'nt' and not Path('/proc/self/exe').exists():
            self.skipTest('self-relaunch runtime fixture currently covers Windows and Linux')
        return compiler

    def build(self, compiler, source, exe, extra=()):
        # MinGW's linker cannot name Unicode output paths. Build under a
        # plain name, then exercise the engine from the requested Unicode name.
        cpp = exe.parent / 'relaunch-probe.cpp'
        built_exe = exe.parent / 'relaunch-probe.exe'
        cpp.write_text(source, encoding='utf-8')
        # Use a windowless GUI-subsystem test image on Windows. It still enters
        # the console C runtime's main(), but a self-launched child cannot open
        # a console window or take focus from the user's applications.
        linker = []
        if os.name == 'nt':
            linker = (['-mwindows'] if 'g++' in Path(compiler).name.lower() else
                      ['-Xlinker', '/subsystem:windows', '-Xlinker', '/entry:mainCRTStartup'])
        target = compiler_target_args(compiler)
        abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
        result = subprocess.run([compiler, *target, *abi, '-std=c++17',
            '-I', str(ROOT), str(cpp), *map(str, extra), *linker, '-o', str(built_exe)],
            cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        built_exe.replace(exe)

    def test_same_executable_both_modes_unicode_quotes_and_descriptor_isolation(self):
        compiler = self.compiler()
        command_source = (ROOT / 'code/qcommon/cmd.c').read_text()
        common_source = (ROOT / 'code/qcommon/common.c').read_text()
        shared_source = (ROOT / 'code/qcommon/q_shared.c').read_text()
        unix_source = (ROOT / 'code/unix/unix_main.cpp').read_text()
        functions = '\n'.join([
            function(shared_source, 'void Q_strncpyz('),
            function(command_source, 'static void Cmd_TokenizeString2('),
            function(command_source, 'void Cmd_TokenizeString('),
            function(common_source, 'static void Com_ParseCommandLine('),
            function(unix_source, 'static bool Sys_CommandLineArgNeedsQuotes('),
            function(unix_source, 'static std::string Sys_BuildCommandLine('),
        ])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = root / 'same engine Wölf 東京.exe'
            self.build(compiler, PRELUDE + functions + HARNESS, exe, [
                ROOT / 'code/platform/process_relaunch.cpp', ROOT / 'code/qcommon/game_profile.cpp'])
            unit = subprocess.run([str(exe), 'unit'], cwd=root, capture_output=True, text=True, timeout=15)
            self.assertEqual(unit.returncode, 0, unit.stdout + unit.stderr)
            for mode in ['rtcw-sp', 'rtcw-mp']:
                home = root / (mode + " Wölf 東京 +set ignored 1; %&'()")
                home.mkdir()
                ran = subprocess.run([str(exe), 'parent', mode], cwd=home,
                    capture_output=True, text=True, timeout=15)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
                report = home / 'report.txt'
                deadline = time.monotonic() + 10
                while not report.exists() and time.monotonic() < deadline:
                    time.sleep(0.02)
                self.assertTrue(report.exists(), 'relaunched fixture did not publish its result')
                image, target, raw, child_pid = report.read_text(encoding='utf-8').splitlines()
                if os.name == 'nt':
                    # The report is published just before child exit. Wait for
                    # that specific process before deleting the fixture image.
                    import ctypes
                    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
                    kernel.OpenProcess.restype = ctypes.c_void_p
                    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
                    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
                    child = kernel.OpenProcess(0x100000, False, int(child_pid))
                    if child:
                        try:
                            self.assertEqual(kernel.WaitForSingleObject(child, 10000), 0)
                        finally:
                            kernel.CloseHandle(child)
                    else:
                        self.assertEqual(ctypes.get_last_error(), 87)  # already exited
                self.assertTrue(Path(image).samefile(exe))
                self.assertEqual(target, mode)
                path = str(home) + os.sep
                self.assertEqual(raw, f'+set com_gameProfile "{mode}" +set fs_basepath "{path}" '
                    f'+set fs_homepath "{path}" +set fs_game "" +set r_fullscreen "0"')

    def test_prepare_and_launch_failures_consume_no_future_quit(self):
        compiler = self.compiler()
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / 'relaunch_failure.exe'
            self.build(compiler, FAILURES, exe)
            ran = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=15)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
