"""Exercise the Windows replace-file retry against a real exclusive handle."""
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
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "code/win32/win_utf8.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
using qboolean=bool;
constexpr qboolean qtrue=true,qfalse=false;
'''
HARNESS = r'''
static void Write(const std::filesystem::path &path,const char *value) {
 std::ofstream stream(path,std::ios::binary); assert(stream);
 stream<<value; stream.close(); assert(stream);
}
static std::string Read(const std::filesystem::path &path) {
 std::ifstream stream(path,std::ios::binary); assert(stream);
 return std::string(std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>());
}
// Deny every further open of the destination, exactly as a scanner that is
// reading the file the engine just wrote denies one.
static HANDLE Hold(const std::filesystem::path &path) {
 HANDLE handle=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
 assert(handle!=INVALID_HANDLE_VALUE); return handle;
}
struct Release { HANDLE handle; DWORD after; };
static DWORD WINAPI ReleaseLater(LPVOID parameter) {
 const auto *release=static_cast<const Release*>(parameter);
 Sleep(release->after); assert(CloseHandle(release->handle)); return 0;
}
int main(int argc,char **argv) {
 assert(argc==2); const auto root=std::filesystem::u8path(argv[1]);
 const auto source=root/"module.tmp",destination=root/"module.dll";
 const std::string from=source.u8string(),to=destination.u8string();

 // Measure the whole retry budget against a holder that never lets go: the
 // replacement is bounded, and both files are retained. Everything that must
 // not retry is then timed against half of what this machine just spent,
 // which stays a decision rather than a stopwatch under a loaded test run.
 Write(source,"blocked"); Write(destination,"kept");
 HANDLE held=Hold(destination);
 auto start=GetTickCount64();
 const qboolean blocked=Sys_ReplaceFile(from.c_str(),to.c_str());
 const auto budget=GetTickCount64()-start;
 assert(CloseHandle(held));
 assert(!blocked && budget>=1000 && budget<60000);
 assert(Read(source)=="blocked" && Read(destination)=="kept");

 // Nobody holding the destination: the first attempt commits it.
 Write(source,"new"); Write(destination,"old");
 start=GetTickCount64();
 assert(Sys_ReplaceFile(from.c_str(),to.c_str()));
 assert(GetTickCount64()-start<budget/2);
 assert(!std::filesystem::exists(source) && Read(destination)=="new");

 // A holder that lets go part way through: the replacement waits it out
 // instead of reporting the failure the caller cannot recover from.
 Write(source,"retry");
 Release release{Hold(destination),100};
 HANDLE thread=CreateThread(nullptr,0,ReleaseLater,&release,0,nullptr); assert(thread);
 start=GetTickCount64();
 const qboolean retried=Sys_ReplaceFile(from.c_str(),to.c_str());
 const auto waited=GetTickCount64()-start;
 assert(WaitForSingleObject(thread,30000)==WAIT_OBJECT_0); assert(CloseHandle(thread));
 assert(retried && waited>=50);
 assert(!std::filesystem::exists(source) && Read(destination)=="retry");

 // A missing source is not transient and never spends that budget.
 Write(source,"blocked"); Write(destination,"kept");
 const auto absent=root/"absent.tmp"; const std::string missing=absent.u8string();
 assert(!std::filesystem::exists(absent));
 start=GetTickCount64();
 assert(!Sys_ReplaceFile(missing.c_str(),to.c_str()));
 assert(GetTickCount64()-start<budget/2 && Read(destination)=="kept");

 // Neither is a destination directory that was never created.
 const std::string nowhere=(root/"absent"/"module.dll").u8string();
 start=GetTickCount64();
 assert(!Sys_ReplaceFile(from.c_str(),nowhere.c_str()));
 assert(GetTickCount64()-start<budget/2 && Read(source)=="blocked");

 // An unusable path fails before the first attempt.
 start=GetTickCount64();
 assert(!Sys_ReplaceFile(from.c_str(),"\xff") && !Sys_ReplaceFile("\xff",to.c_str()));
 assert(GetTickCount64()-start<budget/2);
 assert(Read(source)=="blocked" && Read(destination)=="kept");
}
'''


@unittest.skipUnless(os.name == 'nt', 'Windows replacement retry')
class WindowsReplaceFileRetryTests(unittest.TestCase):
    def test_replacement_waits_out_a_holder_and_fails_fast_otherwise(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        main = (ROOT / 'code/win32/win_main.cpp').read_text(encoding='utf-8')
        source = PRELUDE + '\n' + function(main, 'qboolean Sys_ReplaceFile(') + HARNESS
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            cpp = folder / 'replace.cpp'
            exe = folder / 'replace.exe'
            fixture = folder / 'fixture'
            fixture.mkdir()
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run(
                [compiler, *compiler_target_args(compiler), '-std=c++17',
                 '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                capture_output=True, text=True, encoding='utf-8', errors='replace')
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe), str(fixture)], capture_output=True, text=True,
                                 timeout=120, creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
