"""Xlib callbacks must outlive failed extension discovery and renderer restarts."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(path, signature):
    text = (ROOT / path).read_text()
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]


class X11ExtensionLifetimeTests(unittest.TestCase):
    def test_discovery_failure_restart_and_display_close(self):
        cc = shutil.which('clang++') or shutil.which('g++')
        if not cc:
            self.skipTest('C++ compiler unavailable')
        dga = (ROOT / 'code/unix/x11_dga.cpp').read_text()
        dga = dga[dga.index('static GLW_ScopedLibrary'):dga.index('void DGA_Mouse')]
        shutdown = '\n'.join(function('code/unix/linux_glimp.cpp',
                                     f'void {api}imp_Shutdown( qboolean unloadDLL )')
                             for api in ('GL', 'VK'))
        harness = r'''
#include <cassert>
#include <cstddef>
using qboolean=bool; using Bool=int; using Status=int; using Display=int;
constexpr bool qtrue=true,qfalse=false,True=true,False=false;
constexpr int None=0;
Display display=1; Display *dpy=&display; void *ctx=nullptr;
int win=0,invisible_cursor=0;
struct { bool dga_ext,randr_gamma,gammaSet,vidmode_active,desktop_ok,cdsFullscreen; } glw_state{};
bool loaded=false,registered=false,available=false,version=true,symbols=true,loadable=true;
int loads=0,unloads=0,closes=0;
struct GLW_ScopedLibrary {
 void *handle=nullptr;
 explicit operator bool() const {return handle!=nullptr;}
 void *get() const {return handle;}
 void reset(void *p=nullptr) {
  if(handle && handle!=p) {assert(!registered);loaded=false;++unloads;}
  handle=p;
 }
};
struct sym_t {void **pointer;const char *name;};
void *GLW_LoadLibraryFallback(const char*,const char*) {
 if(!loadable)return nullptr;
 loaded=true;++loads;return &display;
}
Bool query(Display*,int*,int*) {assert(loaded);registered=true;return available;}
Bool queryVersion(Display*,int*,int*) {assert(loaded);registered=true;return version;}
Status direct(Display*,int,int) {return 1;}
bool GLW_LoadSymbols(void*,sym_t (&s)[3],const char*) {
 if(!symbols)return false;
 *s[0].pointer=reinterpret_cast<void*>(queryVersion);
 *s[1].pointer=reinterpret_cast<void*>(query);
 *s[2].pointer=reinterpret_cast<void*>(direct);return true;
}
void Com_Printf(const char*,...) {}
void DGA_Done();
void IN_DeactivateMouse(){} void IN_Shutdown(){}
void XSync(Display*,bool){} void X11_DestroyInputContext(){}
void RandR_RestoreGamma(){} void RandR_RestoreMode(){}
void VidMode_RestoreGamma(){} void VidMode_RestoreMode(){}
void qglXMakeCurrent(Display*,int,void*){} void qglXDestroyContext(Display*,void*){}
void XDestroyWindow(Display*,int){} void XFreeCursor(Display*,int){}
void X11_CloseInputMethod(){}
void XCloseDisplay(Display*) {
 if(registered)assert(loaded); // Exercise the registered callback's lifetime.
 registered=false;++closes;
}
void RandR_Done(){assert(!dpy);} void VidMode_Done(){assert(!dpy);}
void unsetenv(const char*){} void QGL_Shutdown(bool){} void QVK_Shutdown(bool){}
''' + dga + '\n' + shutdown + r'''
int main() {
 for(auto shutdown : {GLimp_Shutdown,VKimp_Shutdown}) {
  for(int scenario : {2,3,4,0,1}) {
   dpy=&display;available=scenario>=3;version=scenario!=3;
   loadable=scenario!=0;symbols=scenario!=1;
   int initialLoads=loads,initialCloses=closes;
   assert(DGA_Init(dpy)==(scenario==4));
   assert(glw_state.dga_ext==(scenario==4));
   shutdown(false); // Context/window restart retains the display and callback.
   assert(dpy && closes==initialCloses);
   assert(DGA_Init(dpy)==(scenario==4));
   assert(loads-initialLoads==(loadable?1:0));
   shutdown(true);
   assert(!dpy && !loaded && !registered && !glw_state.dga_ext);
   assert(closes==initialCloses+1 && loads==unloads);
   shutdown(true); // Repeated cleanup and partial initialization are harmless.
   assert(closes==initialCloses+1 && loads==unloads);
  }
 }
}
'''
        harness = '#include <initializer_list>\n' + harness
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            source.write_text(harness)
            result = subprocess.run([cc, *compiler_target_args(cc), '-std=c++17',
                                     str(source), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
