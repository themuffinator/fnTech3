"""Fatal signals must not report success or recurse through debug assertions."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]


class UnixSignalExitTests(unittest.TestCase):
    def test_fatal_and_requested_shutdown_status(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        def extract(path,signature):
            text=(ROOT/path).read_text();start=text.index(signature)
            return text[start:text.index('\n}',start)+2]
        exit_function=extract('code/unix/unix_main.cpp','void NORETURN Sys_Exit( int code )')
        fatal=extract('code/unix/linux_signals.cpp','static void fatal_signal_handler( int sig )')
        pending=extract('code/unix/linux_signals.cpp','void Sys_HandlePendingSignals( void )')
        harness=r'''
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#define NORETURN __attribute__((noreturn))
using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;
bool signalcaught=false;
volatile sig_atomic_t pendingShutdownSignal=0;
int cleanup=0;
void Sys_DestroyConsole(){++cleanup;}
void Sys_ConsoleInputShutdown(){++cleanup;}
[[noreturn]] void testExit(int code){if(cleanup!=2)std::_Exit(99);std::_Exit(code);}
#define _exit testExit
#define exit testExit
void Com_sprintf(char*d,int n,const char*f,...){va_list a;va_start(a,f);vsnprintf(d,n,f,a);va_end(a);}
void VM_Forced_Unload_Start(){}
void VM_Forced_Unload_Done(){}
void CL_Shutdown(const char*,bool){}
void SV_Shutdown(const char*){}
'''+exit_function+'\n'+fatal+'\n'+pending+r'''
int main(int argc,char**argv){assert(argc==2);std::string mode=argv[1];
 if(mode=="fatal")fatal_signal_handler(SIGSEGV);
 if(mode=="double"){signalcaught=true;fatal_signal_handler(SIGSEGV);}
 if(mode=="requested"){pendingShutdownSignal=SIGTERM;Sys_HandlePendingSignals();}
 if(mode=="idle"){Sys_HandlePendingSignals();return cleanup==0?0:98;}
 if(mode=="error")Sys_Exit(7);
 return 97;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'probe.cpp';cpp.write_text(harness)
            for release in (False,True):
                for dedicated in (False,True):
                    exe=Path(folder)/f'probe-{release}-{dedicated}.exe'
                    flags=(['-DNDEBUG'] if release else [])+(['-DDEDICATED'] if dedicated else [])
                    build=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',*flags,str(cpp),'-o',str(exe)],capture_output=True,text=True)
                    self.assertEqual(build.returncode,0,build.stdout+build.stderr)
                    for mode,expected in [('fatal',1),('double',1),('requested',0),('idle',0),('error',7)]:
                        result=subprocess.run([str(exe),mode],capture_output=True,text=True)
                        self.assertEqual(result.returncode,expected,(release,dedicated,mode,result.stderr))


if __name__=='__main__':unittest.main()

