"""Academy's client-game menu numbers reach its UI module as retail hands them.

Retail jamp.exe 1.01 serves client-game import 238 (CG_OPENUIMENU) at 0x413032
as VM_Call(uivm, 7, args[1]): the number belongs to the UI module and the
engine neither translates nor bounds it. uix86.dll 1.01's _UI_SetActiveMenu
(0x40010de0) opens menus 0 to 11 and ignores every other number. These cases
compile vm.c's Academy UI branch as written, with the production boundary
header, and check what reaches the module's entry point
(docs/fntech3/2026-10-15-jka-ui-menus.md).
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import time
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
ANCHOR = '\tif ( Com_GameIsJKAMP() && vm->index == VM_UI ) {'


def block(source, anchor):
    """The anchored statement through its matching closing brace."""
    start = source.index(anchor)
    depth = 0
    for index in range(start, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise ValueError('unterminated block: ' + anchor)


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/compat/jka_ui_exports.h"
#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#ifdef _MSC_VER
#include <cstdlib>
#endif
struct Drop {};
static char lastError[256];
void QDECL Com_Error(errorParm_t, const char *format, ...) {
 va_list args;va_start(args,format);std::vsnprintf(lastError,sizeof(lastError),format,args);va_end(args);
 throw Drop{};
}
static int academy=1;
int Com_GameIsJKAMP(void){return academy;}
static int entries,lastCommand,lastArguments[15];
static intptr_t QDECL Module(int command,...) {
 va_list args;va_start(args,command);
 for(int &value:lastArguments)value=va_arg(args,int);
 va_end(args);
 ++entries;lastCommand=command;return 1234;
}
// vm.c's Academy UI branch, compiled as written.
static intptr_t AcademyUICall(vm_t *vm,int nargs,int callnum,...) {
 intptr_t r;int i;
'''

CHECKS = r'''
 return -1; // the branch was not taken
}
template<class F> static void rejected(F call,const char *text){
 const int before=entries;bool dropped=false;
 try{call();}catch(Drop){dropped=true;}
 assert(dropped&&entries==before&&std::strstr(lastError,text));
}
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 vm_t vm{};vm.index=VM_UI;vm.entryPoint=Module;vm.syscallCount=77;
 // A client game's number reaches export 7 as its one argument, untranslated,
 // whatever the number: the module's own switch decides what it opens.
 for(const int menu:{0,1,2,3,4,5,6,7,8,9,10,11,12,255,-1,INT_MIN,INT_MAX}){
  const int before=entries;
  assert(AcademyUICall(&vm,1,UI_SET_MODULE_MENU,menu)==1234&&entries==before+1);
  assert(lastCommand==FNTECH3_JKA_UI_SET_ACTIVE_MENU&&lastArguments[0]==menu);
  for(int n=1;n<15;++n)assert(lastArguments[n]==0);
  assert(vm.callLevel==0&&vm.syscallCount==0);
 }
 // The engine's own requests keep their vocabulary and its refusals.
 assert(AcademyUICall(&vm,1,UI_SET_ACTIVE_MENU,int(UIMENU_MAIN))==1234&&lastCommand==7&&lastArguments[0]==1);
 assert(AcademyUICall(&vm,1,UI_SET_ACTIVE_MENU,int(UIMENU_JKA_PLAYERCONFIG))==1234&&lastArguments[0]==3);
 assert(AcademyUICall(&vm,1,UI_SET_ACTIVE_MENU,int(UIMENU_POSTGAME))==1234&&lastArguments[0]==5);
 for(const int menu:{int(UIMENU_NEED_CD),int(UIMENU_BAD_CD_KEY),int(UIMENU_ENDGAME),int(UIMENU_HELP),-1,int(UIMENU_COUNT)})
  rejected([&]{AcademyUICall(&vm,1,UI_SET_ACTIVE_MENU,menu);},"Unsupported Jedi Academy UI menu");
 // One argument only, and only a module the boundary can call.
 rejected([&]{AcademyUICall(&vm,0,UI_SET_MODULE_MENU);},"native UI export");
 rejected([&]{AcademyUICall(&vm,2,UI_SET_MODULE_MENU,11,0);},"native UI export");
 vm.entryPoint=nullptr;rejected([&]{AcademyUICall(&vm,1,UI_SET_MODULE_MENU,11);},"native UI export");
 vm.entryPoint=Module;vm.dllExports=&vm;
 rejected([&]{AcademyUICall(&vm,1,UI_SET_MODULE_MENU,11);},"native UI export");vm.dllExports=nullptr;
 // Inside an outer call to the same module, the outer call's counters stand.
 vm.callLevel=1;vm.syscallCount=5;
 assert(AcademyUICall(&vm,1,UI_SET_MODULE_MENU,9)==1234&&lastArguments[0]==9);
 assert(vm.callLevel==1&&vm.syscallCount==5);vm.callLevel=0;
 // Another profile, or another module, never takes this branch.
 const int before=entries;
 academy=0;assert(AcademyUICall(&vm,1,UI_SET_MODULE_MENU,11)==-1);academy=1;
 vm.index=VM_CGAME;assert(AcademyUICall(&vm,1,UI_SET_MODULE_MENU,11)==-1);vm.index=VM_UI;
 assert(entries==before);
 std::puts("ok");
 return 0;
}
'''


def fixture():
    vm = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
    return PRELUDE + block(vm, ANCHOR) + CHECKS


class AcademyUIMenuTests(unittest.TestCase):
    def test_vm_branch_hands_client_game_menus_to_export_7_untranslated(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parent = ROOT / '.tmp/tests/jka-ui-menu'
        parent.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.mkdtemp(dir=parent)
        try:
            folder = Path(temporary).resolve()
            cpp = folder / 'probe.cpp'
            exe = folder / 'probe.exe'
            cpp.write_text(fixture(), encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Wall', '-Wextra',
                '-Wno-unused-parameter', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                cwd=folder, text=True, capture_output=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=folder, text=True, capture_output=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(run.stdout.strip(), 'ok')
        finally:
            for attempt in range(10):
                try:
                    shutil.rmtree(temporary)
                    break
                except PermissionError:
                    time.sleep(0.5)

    def test_client_game_request_is_the_module_menu_request(self):
        host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text(encoding='utf-8')
        case = host[host.index('    case Call::CG_OPENUIMENU:'):host.index('    case Call::CG_SETUSERCMDVALUE:')]
        self.assertIn('VM_Call(uivm, 1, UI_SET_MODULE_MENU, static_cast<int>(args[1]));', case)
        self.assertNotIn('UI_SET_ACTIVE_MENU', case)
        self.assertNotIn('is not served yet', case)
        self.assertIn('result = 0;', case)
        exports = (ROOT / 'code/compat/jka_ui_exports.h').read_text(encoding='utf-8')
        self.assertNotIn('FNTech3_JKAUIMenuToInternal', exports)
        self.assertIn('case UI_SET_MODULE_MENU: return FNTECH3_JKA_UI_SET_ACTIVE_MENU;', exports)


if __name__ == '__main__':
    unittest.main()
