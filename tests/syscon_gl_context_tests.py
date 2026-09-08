"""Console drawing must not redirect engine GL work into SDL's context."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class SysconGLContextTests(unittest.TestCase):
    def test_console_render_preserves_caller_context(self):
        cc = shutil.which('clang++') or shutil.which('g++')
        if not cc:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/unix/unix_syscon.cpp').read_text()
        start = source.index('class SysCon_CurrentGLContext')
        guard = source[start:source.index('\n};', start) + 3]
        start = source.index('static void SysCon_Render( void )')
        render = source[start:source.index('\n}', start) + 2]
        harness = r'''
#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <initializer_list>
struct SDL_Window {int id;};
using SDL_GLContext = void*;
SDL_Window game{1},console{2};
int gameContext,consoleContext;
SDL_Window *currentWindow=nullptr;
SDL_GLContext currentContext=nullptr;
bool failRestore=false,sizeAvailable=true;
int binds=0,presents=0;
SDL_Window *SDL_GL_GetCurrentWindow(){return currentWindow;}
SDL_GLContext SDL_GL_GetCurrentContext(){return currentContext;}
bool SDL_GL_MakeCurrent(SDL_Window*w,SDL_GLContext c){
 ++binds;if(failRestore)return false;currentWindow=w;currentContext=c;return true;
}
const char *SDL_GetError(){return "controlled binding failure";}
[[noreturn]] void Sys_Error(const char*,...){std::_Exit(75);}
void activateConsole(){currentWindow=&console;currentContext=&consoleContext;}
struct SDL_FRect {float x,y,w,h;};
struct sysconLayout_t {int logicalWidth;SDL_FRect headerRect,copyButtonRect,clearButtonRect;};
struct {SDL_Window*window=&console;void*renderer=&consoleContext;int visLevel=1;bool dirty=true;} s_syscon;
constexpr bool qfalse=false;
constexpr int SYSCON_WINDOW_BG=0,SYSCON_ACCENT=1,SYSCON_HEADER_BG=2,SYSCON_BORDER=3;
bool SDL_GetWindowSize(SDL_Window*,int*w,int*h){*w=800;*h=600;return sizeAvailable;}
void SysCon_BuildLayout(int,int,sysconLayout_t*l){l->logicalWidth=800;}
void SysCon_SetDrawColor(int){}
void SDL_RenderClear(void*){activateConsole();}
void SysCon_FillRect(SDL_FRect*,int){}
void SysCon_DrawRect(SDL_FRect*,int){}
void SysCon_DrawButton(SDL_FRect*,const char*){}
void SysCon_RenderStatusText(sysconLayout_t*){}
void SysCon_RenderError(sysconLayout_t*){}
void SysCon_RenderLog(sysconLayout_t*){}
void SysCon_RenderInput(sysconLayout_t*){}
void SDL_RenderPresent(void*){assert(currentContext==&consoleContext);++presents;}
''' + guard + '\n' + render + r'''
int main(int argc,char**) {
 for(bool withGame : {true,false}) {
  currentWindow=withGame?&game:nullptr;currentContext=withGame?&gameContext:nullptr;
  auto savedWindow=currentWindow;auto savedContext=currentContext;
  for(int i=0;i<2;++i) {
   SysCon_Render();
   assert(currentWindow==savedWindow && currentContext==savedContext);
  }
  int oldBinds=binds;
  sizeAvailable=false;SysCon_Render();sizeAvailable=true;
  s_syscon.visLevel=0;SysCon_Render();s_syscon.visLevel=1;
  assert(binds==oldBinds && currentContext==savedContext);
  {SysCon_CurrentGLContext creation;activateConsole();}
  assert(currentContext==savedContext);
  {SysCon_CurrentGLContext destruction(&console);activateConsole();currentWindow=nullptr;currentContext=nullptr;}
  assert(currentContext==savedContext);
 }
 // Never resurrect a context whose console window was destroyed.
 activateConsole();
 {SysCon_CurrentGLContext destruction(&console);currentWindow=nullptr;currentContext=nullptr;}
 assert(!currentContext && !currentWindow && presents==4);
 if(argc>1) {
  currentWindow=&game;currentContext=&gameContext;
  SysCon_CurrentGLContext guard;activateConsole();failRestore=true;
 }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            cpp.write_text(harness)
            result = subprocess.run([cc, *compiler_target_args(cc), '-std=c++17',
                                     str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for args, expected in (([], 0), (['restore-failure'], 75)):
                result = subprocess.run([str(exe), *args], capture_output=True, text=True)
                self.assertEqual(result.returncode, expected, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
