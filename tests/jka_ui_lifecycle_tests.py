"""Exercise Academy's production UI cleanup and recovery dispatch."""

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
    return source[start:source.index("\n}", start) + 2]


PRELUDE = r'''
#include <cassert>
#include <cstring>
typedef int qboolean;
enum { qfalse, qtrue, KEYCATCH_UI = 2, H_Q3UI = 3, UI_SHUTDOWN = 2,
    CA_LOADING = 6, CS_SERVERINFO = 0 };
struct vm_t {};
static vm_t module;
static vm_t *uivm;
static bool jka, jk2, com_errorEntered, cl_jkaUIInitialized, cl_jk2UIInitialized;
static int catcher, shutdownCalls, freeCalls, fileCloses, fontResets, stringResets, ghoul2Resets;
static int rendererCalls, soundCalls, soundRegistrationCalls, initCalls;
static bool Com_GameIsJKAMP() { return jka; }
static bool Com_GameIsJK2MP() { return jk2; }
static int jk2StringResets;
static void CL_JK2StringReset(unsigned owner) { assert(owner == 4); ++jk2StringResets; }
static bool Com_GameIsSTVEFSP() { return false; }
static bool Com_GameIsQ3Test() { return false; }
static int Key_GetCatcher() { return catcher; }
static void Key_SetCatcher(int value) { catcher = value; }
static void CL_JKAFontReset() { ++fontResets; }
static void CL_JKAStringReset() { ++stringResets; }
static void CL_JKAUIGhoul2Clear() { ++ghoul2Resets; }
static void CL_JKAUIRenderClear() {}
static void FS_VM_CloseFiles(int handle) { assert(handle == H_Q3UI); ++fileCloses; }
static void Cmd_RemoveCommand(const char *) { assert(false); }
static void CL_Q3TestShutdownUI() { assert(false); }
static int VM_Call(vm_t *vm, int count, int ordinal) {
    assert(vm == &module && count == 0 && ordinal == UI_SHUTDOWN);
    // The initialized marker must be cleared before calling into the image,
    // while its engine-owned font/string storage is still alive.
    assert(!cl_jkaUIInitialized && !cl_jk2UIInitialized && fontResets == 0 &&
        stringResets == 0 && ghoul2Resets == 0 && jk2StringResets == 0);
    ++shutdownCalls;
    return 0;
}
static void VM_Free(vm_t *vm) { assert(vm == &module); ++freeCalls; }
struct ClientStatic { int state; qboolean rendererStarted, soundStarted,
    soundRegistered, uiStarted; };
static ClientStatic cls;
struct Cvar { int integer; };
static Cvar running{1};
static Cvar *com_cl_running = &running;
struct GameState { char stringData[4]; int stringOffsets[1]; };
static struct { GameState gameState; } cl;
static const char *Info_ValueForKey(const char *, const char *) { return ""; }
static const char *va(const char *, const char *) { return ""; }
static const char *Cvar_VariableString(const char *) { return ""; }
static void Cbuf_AddText(const char *) { assert(false); }
static void CL_InitRenderer() { ++rendererCalls; }
static void S_Init() { ++soundCalls; }
static void S_BeginRegistration() { ++soundRegistrationCalls; }
static void CL_InitUI() {
    assert(!com_errorEntered);
    ++initCalls;
    uivm = &module;
    cl_jkaUIInitialized = jka;
    cl_jk2UIInitialized = jk2;
}
static void Reset() {
    cls = {}; uivm = &module; cl_jkaUIInitialized = cl_jk2UIInitialized = false;
    jk2StringResets = 0;
    com_errorEntered = false; catcher = 7;
    shutdownCalls = freeCalls = fileCloses = fontResets = stringResets = ghoul2Resets = 0;
    rendererCalls = soundCalls = soundRegistrationCalls = initCalls = 0;
}
'''


CHECKS = r'''
int main() {
    // A load or UI_INIT failure owns an image/files, but cannot call shutdown.
    jka = true; Reset();
    assert(!CL_UIModuleReady());
    CL_ShutdownUI();
    assert(shutdownCalls == 0 && freeCalls == 1 && fileCloses == 1);
    assert(fontResets == 1 && stringResets == 1 && ghoul2Resets == 1 && !uivm && !cls.uiStarted);
    assert(!(catcher & KEYCATCH_UI));

    // An error inside a later export must not reenter its abandoned stack.
    Reset(); cl_jkaUIInitialized = true; com_errorEntered = true;
    assert(!CL_UIModuleReady());
    CL_ShutdownUI();
    assert(shutdownCalls == 0 && freeCalls == 1 && fileCloses == 1);
    assert(fontResets == 1 && stringResets == 1 && ghoul2Resets == 1);
    CL_StartHunkUsers();
    assert(initCalls == 0 && rendererCalls == 0 && soundCalls == 0);
    assert(!cls.uiStarted && !uivm);
    com_errorEntered = false;
    CL_StartHunkUsers();
    assert(initCalls == 1 && rendererCalls == 1 && soundCalls == 1);
    assert(soundRegistrationCalls == 1 && cls.uiStarted && CL_UIModuleReady());

    // Normal shutdown retains its resources until the export has returned.
    Reset(); cl_jkaUIInitialized = true;
    assert(CL_UIModuleReady());
    CL_ShutdownUI();
    assert(shutdownCalls == 1 && freeCalls == 1 && fileCloses == 1);
    assert(fontResets == 1 && stringResets == 1 && ghoul2Resets == 1 && !CL_UIModuleReady());

    // Failed image creation still clears independently owned resources.
    Reset(); uivm = nullptr;
    CL_ShutdownUI();
    assert(!freeCalls && !shutdownCalls && fileCloses == 1);
    assert(fontResets == 1 && stringResets == 1 && ghoul2Resets == 1);

    // Outcast's QVM must also avoid reentering an abandoned UI_INIT/export.
    jka = false; jk2 = true; Reset();
    assert(!CL_UIModuleReady()); CL_ShutdownUI();
    assert(!shutdownCalls && freeCalls == 1 && fileCloses == 1 && jk2StringResets == 1);
    assert(!fontResets && !stringResets && !ghoul2Resets);
    Reset(); cl_jk2UIInitialized = true; com_errorEntered = true;
    assert(!CL_UIModuleReady()); CL_ShutdownUI();
    assert(!shutdownCalls && freeCalls == 1 && fileCloses == 1 && jk2StringResets == 1);
    CL_StartHunkUsers(); assert(!initCalls && !rendererCalls && !soundCalls);
    com_errorEntered = false; CL_StartHunkUsers();
    assert(initCalls == 1 && CL_UIModuleReady());
    Reset(); cl_jk2UIInitialized = true;
    assert(CL_UIModuleReady()); CL_ShutdownUI();
    assert(shutdownCalls == 1 && freeCalls == 1 && fileCloses == 1 && jk2StringResets == 1);
    assert(!CL_UIModuleReady());
    Reset(); uivm = nullptr; CL_ShutdownUI();
    assert(!shutdownCalls && !freeCalls && fileCloses == 1 && jk2StringResets == 1);
    CL_ShutdownUI(); assert(jk2StringResets == 2 && fileCloses == 2);
    jk2 = false;

    // Existing profiles keep their shutdown/restart dispatch during recovery.
    jka = false; Reset(); com_errorEntered = true;
    assert(CL_UIModuleReady());
    CL_ShutdownUI();
    assert(shutdownCalls == 1 && freeCalls == 1 && fileCloses == 1);
    assert(!fontResets && !stringResets && !ghoul2Resets);
    com_errorEntered = false;
    CL_StartHunkUsers();
    assert(initCalls == 1 && CL_UIModuleReady());
    return 0;
}
'''


class JkaUiLifecycleTests(unittest.TestCase):
    def test_native_cleanup_and_deferred_restart(self):
        compiler = next((path for name in ("clang++", "g++", "c++")
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        ui = (ROOT / "code/client/cl_ui.cpp").read_text(encoding="utf-8")
        main = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        init = function(ui, "void CL_InitUI( void )")
        self.assertLess(init.index("cl_jkaUIInitialized = false;"),
                        init.index("// sanity check"))
        self.assertGreater(init.index("cl_jkaUIInitialized = Com_GameIsJKAMP();"),
                           init.index("// init for this gamestate"))
        self.assertLess(init.index("cl_jk2UIInitialized = false;"), init.index("// sanity check"))
        self.assertGreater(init.index("cl_jk2UIInitialized = Com_GameIsJK2MP();"),
                           init.index("// init for this gamestate"))
        disconnect = function(main, "qboolean CL_Disconnect( qboolean showMainMenu )")
        self.assertIn("CL_UIModuleReady() && showMainMenu", disconnect)
        restore = function(main, "static void CL_RestoreMainMenuAfterDisconnect( void )")
        self.assertIn("if ( CL_UIModuleReady() )", restore)
        production = "\n".join([
            function(ui, "qboolean CL_UIModuleReady( void )"),
            function(ui, "void CL_ShutdownUI( void )"),
            function(main, "void CL_StartHunkUsers( void )"),
        ])
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="jka-ui-lifecycle-", dir=scratch) as folder:
            path = Path(folder)
            fixture = path / "lifecycle.cpp"
            executable = path / ("lifecycle.exe" if os.name == "nt" else "lifecycle")
            fixture.write_text(PRELUDE + production + CHECKS, encoding="utf-8")
            build = subprocess.run([compiler, *compiler_target_args(compiler),
                "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", str(fixture),
                "-o", str(executable)], text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
