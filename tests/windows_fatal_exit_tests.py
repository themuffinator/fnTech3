"""Windows dedicated fatal errors must terminate without GUI dismissal."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT = Path(__file__).resolve().parents[1]

class WindowsFatalExitTests(unittest.TestCase):
    def test_dedicated_failure_and_client_dialog(self):
        cc = shutil.which('clang++') or shutil.which('g++')
        if not cc: self.skipTest('C++ compiler unavailable')
        source = (ROOT/'code/win32/win_main.cpp').read_text()
        start = source.index('void NORETURN FORMAT_PRINTF(1, 2) QDECL Sys_Error(')
        function = source[start:source.index('\n}', start)+2]
        harness = r'''
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#define NORETURN __attribute__((noreturn))
#define FORMAT_PRINTF(a,b)
#define QDECL
struct MSG {};
constexpr bool qtrue=true;
int cleanup=0, timer=0, shown=0, shutdown=0, filter=0;
void Q_vsnprintf(char*d,int n,const char*f,va_list a){vsnprintf(d,n,f,a);}
void CL_Shutdown(const char*s,bool){if(strcmp(s,"fatal test 17"))std::_Exit(91);++shutdown;}
void Conbuf_AppendText(const char*){}
void Sys_SetErrorText(const char*){}
void Sys_ShowConsole(int,bool){++shown;}
void timeEndPeriod(int){++timer;}
void Cmd_Clear(){}
void Com_Quit_f(){std::_Exit(92);}
void TranslateMessage(MSG*){}
void DispatchMessage(MSG*){}
void SetUnhandledExceptionFilter(void*){++filter;}
void Sys_DestroyConsole(){++cleanup;}
int GetMessage(MSG*,void*,int,int){
#ifdef DEDICATED
 std::_Exit(93);
#else
 std::_Exit(shown==1 && shutdown==1 && timer==1 ? 0 : 94);
#endif
}
[[noreturn]] void testExit(int code){
 if(cleanup!=1 || timer!=1 || filter!=1 || shown || shutdown)std::_Exit(95);
 std::_Exit(code);
}
#define exit testExit
''' + function + '\nint main(){Sys_Error("fatal %s %d", "test", 17);}\n'
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'probe.cpp'; cpp.write_text(harness)
            for dedicated in (False, True):
                exe=Path(folder)/f'probe-{dedicated}.exe'
                flags=['-DDEDICATED'] if dedicated else []
                build=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',*flags,str(cpp),'-o',str(exe)],capture_output=True,text=True)
                self.assertEqual(build.returncode,0,build.stdout+build.stderr)
                run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=5)
                self.assertEqual(run.returncode,1 if dedicated else 0,run.stderr)
                if dedicated:self.assertIn('fatal test 17',run.stderr)

if __name__=='__main__': unittest.main()
