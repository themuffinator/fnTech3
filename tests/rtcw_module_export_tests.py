"""Exercise the real VM export boundary with synthetic modules, without input I/O."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include "code/qcommon/vm_local.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/wolfet_module_abi.h"
#include "code/compat/rtcw_game_abi.h"
#include "code/game/g_public.h"
#include "code/ui/ui_public.h"
#include "code/renderercommon/tr_types.h"
#ifndef USE_DEDICATED
#include "code/cgame/cg_public.h"
#include "code/compat/rtcw_key_adapter.h"
#include "code/compat/wolfet_key_adapter.h"
#include "code/compat/wolfet_menu_adapter.h"
#include "code/compat/jka_cgame_exports.h"
#include "code/compat/jka_ui_exports.h"
#endif
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile(void) { return &profile; }
int Com_GameIsWolfET(void) { return profile.id == COM_GAME_WOLFET; }
int Com_GameIsJKAMP(void) { return profile.id == COM_GAME_JKA_MP; }
int Com_GameIsJKASP(void) { return profile.id == COM_GAME_JKA_SP; }
int Com_GameIsRTCW(void) { return profile.id == COM_GAME_RTCW_SP || profile.id == COM_GAME_RTCW_MP; }
int Com_GameIsRTCWSP(void) { return profile.id == COM_GAME_RTCW_SP; }
static jmp_buf errorJump;
static int errors, nativeCalls, structuredCalls, compiledCalls, interpretedCalls;
static int observedCall, observedLevel, observedCounter, observedCount;
static intptr_t observedArgs[16];
static vm_t module;
static intptr_t nativeReturn = -1234567;
static int escapeCalls;
#ifndef USE_DEDICATED
static vm_t *uivm = &module;
static struct { int state; } cls;
static struct { qboolean demoplaying; } clc;
#endif
void QDECL Com_Error(errorParm_t level, const char *format, ...) {
    (void)level; (void)format; ++errors; longjmp(errorJump, 1);
}
static intptr_t QDECL Native(int call, ...) {
    va_list ap; int i;
    ++nativeCalls; observedCall = call;
    observedLevel = module.callLevel; observedCounter = module.syscallCount;
    va_start(ap, call);
    for (i = 0; i < 15; ++i) observedArgs[i] = va_arg(ap, int32_t);
    va_end(ap);
#ifndef USE_DEDICATED
    if (module.index == VM_UI && call == 3 && observedArgs[0] == 27 && observedArgs[1] == 1) ++escapeCalls;
#endif
    ++module.syscallCount;
    return nativeReturn;
}
static intptr_t VM_CallNativeExports(vm_t *vm, int call, intptr_t *args) {
    assert(vm == &module); ++structuredCalls; observedCall = call;
    observedLevel = module.callLevel; observedCounter = module.syscallCount;
    memcpy(observedArgs, args, sizeof(observedArgs));
    return -7654321;
}
int32_t VM_CallCompiled(vm_t *vm, int nargs, int32_t *args) {
    int i; assert(vm == &module); ++compiledCalls; observedCount = nargs;
    observedLevel = module.callLevel; observedCounter = module.syscallCount;
    for (i = 0; i < nargs; ++i) observedArgs[i] = args[i];
    return -1020304;
}
int32_t VM_CallInterpreted2(vm_t *vm, int nargs, int32_t *args) {
    const int32_t result = VM_CallCompiled(vm, nargs, args);
    --compiledCalls; ++interpretedCalls; return result;
}
'''

CHECKS = r'''
#define ERROR(expression) do { \
    const int before = errors; \
    if (!setjmp(errorJump)) { expression; assert(!"expected VM error"); } \
    assert(errors == before + 1); \
} while (0)
static void Reset(comGameId_t id) {
    memset(&profile, 0, sizeof(profile)); profile.id = id; profile.key = "test";
    memset(&module, 0, sizeof(module)); module.name = "test"; module.index = VM_GAME;
    module.entryPoint = Native; module.syscallCount = 73;
    nativeReturn = -1234567; escapeCalls = 0;
    nativeCalls = structuredCalls = compiledCalls = interpretedCalls = 0;
    memset(observedArgs, 0x5a, sizeof(observedArgs));
}
static void Reject(vmIndex_t index, int call, int nargs, int a, int b, int c) {
    int32_t args[5] = {a, b, c, 1, 1}, saved[5]; int output = 1234;
    memcpy(saved, args, sizeof(args));
    assert(!VM_PrepareRTCWExport(profile.id, index, call, nargs, args, &output));
    assert(output == 1234 && !memcmp(saved, args, sizeof(args)));
}
static void RTCW(comGameId_t id) {
    // Expected source ordinals are deliberately explicit, including shifted UI exports.
    static const struct { vmIndex_t index; int call, sp, mp, spArgs, mpArgs; } calls[] = {
        {VM_GAME, GAME_INIT, 0, 0, 3, 3}, {VM_GAME, GAME_SHUTDOWN, 1, 1, 1, 1},
        {VM_GAME, GAME_CLIENT_CONNECT, 2, 2, 3, 3}, {VM_GAME, GAME_CLIENT_BEGIN, 3, 3, 1, 1},
        {VM_GAME, GAME_CLIENT_USERINFO_CHANGED, 4, 4, 1, 1},
        {VM_GAME, GAME_CLIENT_DISCONNECT, 5, 5, 1, 1},
        {VM_GAME, GAME_CLIENT_COMMAND, 6, 6, 1, 1}, {VM_GAME, GAME_CLIENT_THINK, 7, 7, 1, 1},
        {VM_GAME, GAME_RUN_FRAME, 8, 8, 1, 1}, {VM_GAME, GAME_CONSOLE_COMMAND, 9, 9, 0, 0},
        {VM_GAME, BOTAI_START_FRAME, 10, 10, 1, 1},
        {VM_GAME, GAME_RETRIEVE_MOVESPEEDS_FROM_CLIENT, 13, 13, 2, 2},
        {VM_GAME, GAME_GETMODELINFO, 14, -1, 3, 3},
        {VM_GAME, AICAST_VISIBLEFROMPOS, 11, 11, 5, 5},
        {VM_GAME, AICAST_CHECKATTACKATPOS, 12, 12, 5, 5},
#ifndef USE_DEDICATED
        {VM_UI, UI_GETAPIVERSION, 0, 0, 0, 0}, {VM_UI, UI_INIT, 1, 1, 1, 1},
        {VM_UI, UI_SHUTDOWN, 2, 2, 0, 0}, {VM_UI, UI_MOUSE_EVENT, 4, 4, 2, 2},
        {VM_UI, UI_KEY_EVENT, 3, 3, 2, 2},
        {VM_UI, UI_REFRESH, 5, 5, 1, 1}, {VM_UI, UI_IS_FULLSCREEN, 6, 6, 0, 0},
        {VM_UI, UI_SET_ACTIVE_MENU, 7, 7, 1, 1}, {VM_UI, UI_CONSOLE_COMMAND, 9, 9, 1, 1},
        {VM_UI, UI_DRAW_CONNECT_SCREEN, 10, 10, 1, 1}, {VM_UI, UI_HASUNIQUECDKEY, 11, 11, 0, 0},
        {VM_CGAME, CG_INIT, 0, 0, 2, 3}, {VM_CGAME, CG_SHUTDOWN, 1, 1, 0, 0},
        {VM_CGAME, CG_CONSOLE_COMMAND, 2, 2, 0, 0},
        {VM_CGAME, CG_DRAW_ACTIVE_FRAME, 3, 3, 3, 3},
        {VM_CGAME, CG_CROSSHAIR_PLAYER, 4, 4, 0, 0},
        {VM_CGAME, CG_LAST_ATTACKER, 5, 5, 0, 0},
        {VM_CGAME, CG_KEY_EVENT, 6, 6, 2, 2},
        {VM_CGAME, CG_MOUSE_EVENT, 7, 7, 2, 2}, {VM_CGAME, CG_EVENT_HANDLING, 8, 8, 1, 1},
        {VM_CGAME, CG_GET_TAG, 9, 9, 3, 3},
#endif
    };
    unsigned i; int n;
    Reset(id);
    for (i = 0; i < sizeof(calls)/sizeof(calls[0]); ++i) {
        const int count = id == COM_GAME_RTCW_SP ? calls[i].spArgs : calls[i].mpArgs;
        if ((id == COM_GAME_RTCW_SP ? calls[i].sp : calls[i].mp) < 0) {
            const int before = nativeCalls;
            Reject(calls[i].index, calls[i].call, count, 0, 1, 1);
            module.index = calls[i].index; module.callLevel = 0; module.syscallCount = 73;
            ERROR(VM_Call(&module, count, calls[i].call, 0, 1, 1, 1, 1));
            assert(nativeCalls == before && module.callLevel == 0 && module.syscallCount == 73);
            continue;
        }
        for (n = 0; n < count; ++n) Reject(calls[i].index, calls[i].call, n, 0, 1, 1);
        module.index = calls[i].index; module.callLevel = 0; module.syscallCount = 73;
        assert(VM_Call(&module, count, calls[i].call, 0, 1, 1, 1, 1) == -1234567);
        assert(observedCall == (id == COM_GAME_RTCW_SP ? calls[i].sp : calls[i].mp));
        assert(observedLevel == 1 && observedCounter == 0 && module.syscallCount == 1 && module.callLevel == 0);
        for (n = 0; n < 15; ++n) assert(observedArgs[n] == (n > 0 && n < count ? 1 : 0));
        module.callLevel = 3; module.syscallCount = 97;
        assert(VM_Call(&module, count, calls[i].call, 0, 1, 1, 1, 1) == -1234567);
        assert(observedLevel == 4 && observedCounter == 97 && module.syscallCount == 98 && module.callLevel == 3);
    }
    Reject(VM_BAD, GAME_INIT, 3, 0, 1, 1);
    Reject(VM_COUNT, GAME_INIT, 3, 0, 1, 1);
    Reject(VM_GAME, GAME_GET_CLIENT_SCORE, 1, 0, 1, 1);
    Reject(VM_GAME, GAME_IS_OBJECTIVE_ENTITY, 2, 0, 1, 1); // QL export 13 is not RTCW export 13.
    Reject(VM_GAME, -1, 0, 0, 1, 1); Reject(VM_GAME, INT_MAX, 0, 0, 1, 1);
#ifndef USE_DEDICATED
    {
        static const int sp[] = {0, 1, 2, 3, 5, 6, 8}, mp[] = {0, 1, 2, 3, 4, 5, 6};
        module.index = VM_UI;
        for (n = UIMENU_NONE; n <= UIMENU_POSTGAME; ++n) {
            assert(VM_Call(&module, 1, UI_SET_ACTIVE_MENU, n) == -1234567);
            assert(observedArgs[0] == (id == COM_GAME_RTCW_SP ? sp[n] : mp[n]));
        }
        Reject(VM_UI, UI_SET_ACTIVE_MENU, 1, -1, 0, 0);
        Reject(VM_UI, UI_SET_ACTIVE_MENU, 1, UIMENU_COUNT, 0, 0);
        Reject(VM_UI, UI_KEY_EVENT, 2, 13, 2, 0);
        Reject(VM_UI, UI_REFRESH_DISPLAY_CONTEXT, 0, 0, 0, 0);
        Reject(VM_CGAME, CG_KEY_EVENT, 2, 13, -1, 0);
        Reject(VM_CGAME, CG_CHAT_DOWN, 0, 0, 0, 0);
        Reject(VM_CGAME, CG_EVENT_HANDLING, 1, -1, 0, 0);
        Reject(VM_CGAME, CG_EVENT_HANDLING, 1, 4, 0, 0);
        Reject(VM_CGAME, CG_DRAW_ACTIVE_FRAME, 3, 0, -1, 0);
        Reject(VM_CGAME, CG_DRAW_ACTIVE_FRAME, 3, 0, 3, 0);
        Reject(VM_CGAME, CG_DRAW_ACTIVE_FRAME, 3, 0, 0, -1);
        Reject(VM_CGAME, CG_DRAW_ACTIVE_FRAME, 3, 0, 0, 2);
        for (n = 0; n < MAX_KEYS; ++n) {
            const int expected = n <= 182 ? n : (n >= 187 && n <= 236 ? n - 4 : -1);
            int before;
            assert(FnTech3_RTCWKeyFromInternal(n) == expected);
            if (expected >= 0) assert(FnTech3_RTCWKeyToInternal(expected) == n);
            for (int ui = 0; ui < 2; ++ui) {
                module.index = ui ? VM_UI : VM_CGAME;
                module.callLevel = 0; module.syscallCount = 91; before = nativeCalls;
                assert(VM_Call(&module, 2, ui ? UI_KEY_EVENT : CG_KEY_EVENT, n, 1) ==
                    (expected < 0 ? 0 : -1234567));
                assert(module.callLevel == 0);
                if (expected < 0) assert(nativeCalls == before && module.syscallCount == 91);
                else assert(nativeCalls == before + 1 && observedArgs[0] == expected && observedArgs[1] == 1);
            }
        }
        for (n = 233; n < 4096; ++n) assert(FnTech3_RTCWKeyToInternal(n) == -1);
        assert(FnTech3_RTCWKeyToInternal(-1) == -1 && FnTech3_RTCWKeyToInternal(INT_MAX) == -1);
        assert(FnTech3_RTCWKeyEventFromInternal(INT_MIN) == -1);
        module.index = VM_UI;
        for (n = 0; n < 256; ++n) {
            assert(VM_Call(&module, 2, UI_KEY_EVENT, n | K_CHAR_FLAG, 1) == -1234567);
            assert(observedArgs[0] == (n | 1024));
        }
        { // All original menu returns cross back to the canonical vocabulary.
            /* The last five ET menus and Academy's appended player menu are absent in both RTCW modes. */
            static const int spAll[] = {0,1,2,3,5,6,8,4,7,9,10,11,12,13,14,15,16,17,-1,18,-1,19,-1,-1,-1,-1,-1,-1};
            static const int mpAll[] = {0,1,2,3,4,5,6,-1,-1,7,8,9,10,11,12,13,14,15,16,17,18,-1,-1,-1,-1,-1,-1,-1};
            _Static_assert(sizeof(spAll)/sizeof(spAll[0]) == UIMENU_COUNT, "one SP entry per menu");
            _Static_assert(sizeof(mpAll)/sizeof(mpAll[0]) == UIMENU_COUNT, "one MP entry per menu");
            for (n = 0; n < UIMENU_COUNT; ++n) {
                const int external = id == COM_GAME_RTCW_SP ? spAll[n] : mpAll[n];
                if (external < 0) { Reject(VM_UI, UI_SET_ACTIVE_MENU, 1, n, 0, 0); continue; }
                assert(VM_Call(&module, 1, UI_SET_ACTIVE_MENU, n) == -1234567);
                assert(observedCall == 7 && observedArgs[0] == external);
                nativeReturn = external;
                assert(VM_Call(&module, 0, UI_GET_ACTIVE_MENU) == n && observedCall == 8);
                nativeReturn = -1234567;
            }
            const intptr_t invalid[] = {-1, id == COM_GAME_RTCW_SP ? 20 : 19, INT_MAX, INTPTR_MAX};
            module.callLevel = 0;
            for (unsigned j = 0; j < sizeof(invalid)/sizeof(invalid[0]); ++j) {
                nativeReturn = invalid[j]; ERROR(VM_Call(&module, 0, UI_GET_ACTIVE_MENU));
                assert(module.callLevel == 0);
            }
        }
    }
#endif
    Reset(id);
    ERROR(VM_Call(&module, 0, GAME_INIT));
    ERROR(VM_Call(&module, 0, GAME_GET_CLIENT_SCORE));
    module.entryPoint = NULL; ERROR(VM_Call(&module, 0, GAME_CONSOLE_COMMAND));
    module.dllExports = &module; ERROR(VM_Call(&module, 0, GAME_CONSOLE_COMMAND));
    module.entryPoint = Native; ERROR(VM_Call(&module, 0, GAME_CONSOLE_COMMAND));
    assert(module.callLevel == 0 && module.syscallCount == 73);
    assert(nativeCalls == 0 && structuredCalls == 0 && compiledCalls == 0 && interpretedCalls == 0);
}
#ifndef USE_DEDICATED
static void Popups(comGameId_t id) {
    const struct { const char *name; int sp, mp; } cases[] = {
        {"UIMENU_WM_PICKTEAM",15,13}, {"UIMENU_WM_PICKPLAYER",16,14},
        {"uimenu_wm_quickmessage",17,15}, {"UIMENU_WM_QUICKMESSAGEALT",10,16},
        {"UIMENU_WM_LIMBO",18,17}, {"UIMENU_WM_AUTOUPDATE",10,18},
        {"hbook1",12,10}, {"HBOOK2",13,11}, {"hbook3",14,12},
        {"pregame",7,8}, {"briefing",19,8}, {NULL,10,8}, {"unknown",10,8}
    };
    unsigned i; int state, demo;
    for (i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        for (state = CA_UNINITIALIZED; state <= CA_CINEMATIC; ++state) {
            for (demo = 0; demo < 2; ++demo) {
                const int briefing = id == COM_GAME_RTCW_SP && cases[i].name && !strcmp(cases[i].name, "briefing");
                const int opens = briefing || (state == CA_ACTIVE && !demo);
                Reset(id); module.index = VM_UI; cls.state = state; clc.demoplaying = demo ? qtrue : qfalse;
                CL_RTCWPopup(cases[i].name, qfalse);
                assert(nativeCalls == opens);
                if (opens) assert(observedCall == 7 && observedArgs[0] ==
                    (id == COM_GAME_RTCW_SP ? cases[i].sp : cases[i].mp));
            }
        }
    }
    for (i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        for (int active = 0; active < 2; ++active) {
            const int query = id == COM_GAME_RTCW_MP && cases[i].name && !strcmp(cases[i].name, "UIMENU_WM_LIMBO");
            const int escapes = id == COM_GAME_RTCW_SP ? 1 : (query && active ? 2 : 0);
            Reset(id); module.index = VM_UI; nativeReturn = active ? 17 : 0;
            CL_RTCWPopup(cases[i].name, qtrue);
            assert(nativeCalls == query + escapes && escapeCalls == escapes);
            assert(module.callLevel == 0);
        }
    }
    Reset(id); module.index = VM_UI; uivm = NULL;
    CL_RTCWPopup("briefing", qfalse); CL_RTCWPopup("UIMENU_WM_LIMBO", qtrue);
    assert(nativeCalls == 0); uivm = &module;
    for (int ql = 0; ql < 2; ++ql) {
        Reset(ql ? COM_GAME_QUAKELIVE : COM_GAME_QUAKE3); module.index = VM_UI;
        CL_RTCWPopup("briefing", qfalse); CL_RTCWPopup(NULL, qtrue);
        assert(nativeCalls == 0);
    }
}
#endif
static void Existing(comGameId_t id) {
    int i; Reset(id);
    ERROR(VM_Call(&module, 2, GAME_RETRIEVE_MOVESPEEDS_FROM_CLIENT, 0, 0));
    ERROR(VM_Call(&module, 3, GAME_GETMODELINFO, 0, 0, 0));
#ifndef USE_DEDICATED
    module.index = VM_CGAME;
    ERROR(VM_Call(&module, 3, CG_GET_TAG, 0, 0, 0));
    module.index = VM_GAME;
#endif
    assert(nativeCalls == 0 && module.callLevel == 0 && module.syscallCount == 73);
    assert(VM_Call(&module, 15, 981, -1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, INT_MIN) == -1234567);
    assert(observedCall == 981 && observedArgs[0] == -1 && observedArgs[14] == INT_MIN);
    for (i = 1; i < 14; ++i) assert(observedArgs[i] == i + 1);
    assert(VM_Call(&module, 0, -12) == -1234567);
    for (i = 0; i < 15; ++i) assert(observedArgs[i] == 0);
    assert(observedCall == -12 && observedLevel == 1 && observedCounter == 0 && module.callLevel == 0);
    module.entryPoint = NULL; module.dllExports = &module;
    assert(VM_Call(&module, 2, 765, (intptr_t)&module, (intptr_t)-37) == -7654321);
    assert(observedCall == 765 && observedArgs[0] == (intptr_t)&module && observedArgs[1] == -37);
    for (i = 2; i < 16; ++i) assert(observedArgs[i] == 0);
    module.dllExports = NULL;
    for (i = 0; i < 2; ++i) {
        module.compiled = i ? qtrue : qfalse;
        assert(VM_Call(&module, 2, 573, INT_MIN, INT_MAX) == -1020304);
        assert(observedCount == 3 && observedArgs[0] == 573 && observedArgs[1] == INT_MIN && observedArgs[2] == INT_MAX);
        assert(observedLevel == 1 && observedCounter == 0 && module.callLevel == 0);
    }
    assert(nativeCalls == 2 && structuredCalls == 1 && compiledCalls == 1 && interpretedCalls == 1);
    { int32_t args[3] = {0}; int output = 42;
      assert(!VM_PrepareRTCWExport(id, VM_GAME, GAME_INIT, 3, args, &output) && output == 42); }
}
int main(void) {
    const struct { int call, args, external; } etCalls[] = {
        {GAME_INIT,3,0}, {GAME_SHUTDOWN,1,1}, {GAME_CLIENT_CONNECT,3,2},
        {GAME_CLIENT_BEGIN,1,3}, {GAME_CLIENT_USERINFO_CHANGED,1,4},
        {GAME_CLIENT_DISCONNECT,1,5}, {GAME_CLIENT_COMMAND,1,6},
        {GAME_CLIENT_THINK,1,7}, {GAME_RUN_FRAME,1,8},
        {GAME_CONSOLE_COMMAND,0,9}, {GAME_SNAPSHOT_CALLBACK,2,10}, {BOTAI_START_FRAME,1,11}
    };
    comGameId_t ids[] = {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP};
    unsigned i;
    for (i = 0; i < sizeof(etCalls)/sizeof(etCalls[0]); ++i) {
        Reset(COM_GAME_WOLFET);
        assert(VM_Call(&module, etCalls[i].args, etCalls[i].call, 123, -42, 1) == nativeReturn);
        assert(observedCall == etCalls[i].external && module.callLevel == 0);
        if (etCalls[i].args) {
            assert(observedArgs[0] == 123);
            ERROR(VM_Call(&module, etCalls[i].args - 1, etCalls[i].call, 123, -42));
        }
    }
    Reset(COM_GAME_WOLFET);
    ERROR(VM_Call(&module, 0, 99999));
#ifndef USE_DEDICATED
    /* ET's UI exports are translated; an export only Quake Live's UI has
     * carries no ET ordinal and is refused. */
    module.index = VM_UI; ERROR(VM_Call(&module, 0, UI_REFRESH_DISPLAY_CONTEXT));
#endif
    module.index = VM_GAME; module.entryPoint = NULL;
    ERROR(VM_Call(&module, 3, GAME_INIT, 0, 0, 0));
    module.dllExports = &module;
    ERROR(VM_Call(&module, 3, GAME_INIT, 0, 0, 0));
    for (i = 0; i < sizeof(ids)/sizeof(ids[0]); ++i) {
        int32_t args[3] = {0}; int output = 42;
        Reset(ids[i]);
        ERROR(VM_Call(&module, 2, GAME_SNAPSHOT_CALLBACK, 0, 1));
        // No varargs supplied: release builds must reject these before reading.
        ERROR(VM_Call(&module, -1, GAME_INIT)); ERROR(VM_Call(&module, INT_MIN, GAME_INIT));
        ERROR(VM_Call(&module, MAX_VMMAIN_CALL_ARGS, GAME_INIT));
        ERROR(VM_Call(&module, INT_MAX, GAME_INIT)); ERROR(VM_Call(NULL, 0, GAME_INIT));
        assert(module.callLevel == 0 && module.syscallCount == 73 && nativeCalls == 0);
        assert(!VM_PrepareRTCWExport(ids[i], VM_GAME, GAME_INIT, 3, NULL, &output) && output == 42);
        assert(!VM_PrepareRTCWExport(ids[i], VM_GAME, GAME_INIT, 3, args, NULL));
        Reject(VM_GAME, GAME_INIT, -1, 0, 1, 1);
        Reject(VM_GAME, GAME_INIT, MAX_VMMAIN_CALL_ARGS, 0, 1, 1);
    }
    Existing(COM_GAME_QUAKE3); Existing(COM_GAME_QUAKELIVE);
    RTCW(COM_GAME_RTCW_SP); RTCW(COM_GAME_RTCW_MP);
#ifndef USE_DEDICATED
    Popups(COM_GAME_RTCW_SP); Popups(COM_GAME_RTCW_MP);
#endif
    return 0;
}
'''


class RTCWModuleExportTests(unittest.TestCase):
    def test_native_exports_arity_arguments_and_profile_isolation(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        text = (ROOT / "code/qcommon/vm.c").read_text(encoding="utf-8")
        start = text.index("#ifndef USE_DEDICATED\nstatic int VM_RTCWMenu(")
        end = text.index("\n}\n", text.index("intptr_t QDECL VM_Call(", start)) + 3
        def function(source, signature):
            position = source.index(signature)
            return source[position:source.index("\n}", position) + 2]
        ui = (ROOT / "code/client/cl_ui.cpp").read_text(encoding="utf-8")
        shared = (ROOT / "code/qcommon/q_shared.c").read_text(encoding="utf-8")
        popup = "\n#ifndef USE_DEDICATED\n" + function(shared, "int Q_stricmp(")
        popup += "\n" + function(ui, "void CL_RTCWPopup(") + "\n#endif\n"
        (ROOT / ".tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtcw-exports-", dir=ROOT / ".tmp") as temp:
            folder = Path(temp)
            fixture = folder / "probe.c"
            fixture.write_text(PRELUDE + text[start:end] + popup + CHECKS, encoding="utf-8")
            target = compiler_target_args(compiler)
            if os.name == "nt" and not target:
                target = ["-DFNTECH3_Q3_ONLY"]
            elif os.name != "nt":
                target = ["-m32"]
            for dedicated in (False, True):
                with self.subTest(dedicated=dedicated):
                    executable = folder / ("probe.exe" if os.name == "nt" else "probe")
                    flags = ["-DUSE_DEDICATED"] if dedicated else []
                    build = subprocess.run([compiler, *target, *flags, "-std=c11", "-I", str(ROOT),
                        "-I", str(ROOT / "code/qcommon"),
                        str(fixture), "-o", str(executable)], capture_output=True, text=True, timeout=60, cwd=ROOT)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
