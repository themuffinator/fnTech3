"""Offline key-state backend shared by the RTCW UI/cgame import fixtures."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

BACKEND = r'''
#include "code/compat/rtcw_key_adapter.h"
#include "code/compat/rtcw_key_names.hpp"
#include "code/client/keys.h"
static const char *keyBindings[MAX_KEYS];
static bool keyDown[MAX_KEYS];
static int keyReads, keyWrites, keyObserved, keyCatcher;
static qboolean overstrike;
qboolean Key_IsDown(int key) {
    assert(key >= 0 && key < MAX_KEYS); ++keyReads; keyObserved = key;
    return keyDown[key] ? qtrue : qfalse;
}
const char *Key_GetBinding(int key) {
    ++keyReads; keyObserved = key;
    return key >= 0 && key < MAX_KEYS ? keyBindings[key] : "";
}
void Key_SetBinding(int key, const char *binding) {
    assert(key >= 0 && key < MAX_KEYS && binding); ++keyWrites; keyObserved = key;
    keyBindings[key] = binding;
}
const char *Key_KeynumToString(int key) {
    ++keyReads; keyObserved = key;
    return key == K_MWHEELDOWN ? "MWHEELDOWN" : "unknown";
}
int Key_GetCatcher(void) { return keyCatcher; }
void Key_SetCatcher(int value) { keyCatcher = value; }
void Key_SetOverstrikeMode(qboolean value) { overstrike = value; }
'''

CHECKS = r'''
static void CheckRTCWKeyImports(intptr_t (*dispatch)(intptr_t *), bool ui) {
    using namespace fntech3::rtcw::module;
    std::memset(keyBindings, 0, sizeof(keyBindings)); keyReads = keyWrites = 0;
    for (int key = 0; key <= 232; ++key) {
        const int internal = key <= 182 ? key : key + 4;
        keyDown[internal] = (key & 1) != 0;
        intptr_t args[] = {Ordinal(profile.id, ui ? Call::UI_KEY_ISDOWN : Call::CG_KEY_ISDOWN), key};
        assert(dispatch(args) == (key & 1) && keyObserved == internal);
    }
    for (int key : {-1, 233, 1024, INT_MIN, INT_MAX}) {
        const int before = keyReads;
        intptr_t args[] = {Ordinal(profile.id, ui ? Call::UI_KEY_ISDOWN : Call::CG_KEY_ISDOWN), key};
        assert(dispatch(args) == 0 && keyReads == before);
        CL_RTCWSetBinding(key, "bad"); assert(keyWrites == 0);
    }
    CL_RTCWSetBinding(183, nullptr); assert(keyWrites == 0);
    const bool bindingImports = ui || profile.id == COM_GAME_RTCW_MP;
    if (bindingImports) {
        intptr_t set[] = {Ordinal(profile.id, ui ? Call::UI_KEY_SETBINDING : Call::CG_KEY_SETBINDING),
            183, (intptr_t)"binding"};
        assert(dispatch(set) == 0 && keyWrites == 1 && keyObserved == K_MWHEELDOWN);
        struct { char before; char value[7]; char after; } buffer{'B', {}, 'A'};
        intptr_t read[] = {Ordinal(profile.id, ui ? Call::UI_KEY_GETBINDINGBUF : Call::CG_KEY_GETBINDINGBUF),
            183, (intptr_t)buffer.value, 7};
        assert(dispatch(read) == 0 && keyObserved == K_MWHEELDOWN && lastBounds == 7);
        assert(!std::strcmp(buffer.value, "bindin") && buffer.before == 'B' && buffer.after == 'A');
        // Names come from the module's own key number, as original RTCW's
        // translated lookup does, without consulting the engine's key state.
        read[0] = Ordinal(profile.id, ui ? Call::UI_KEY_KEYNUMTOSTRINGBUF : Call::CG_KEY_KEYNUMTOSTRINGBUF);
        const int named = keyReads;
        assert(dispatch(read) == 0 && keyReads == named);
        assert(!std::strcmp(buffer.value, "MWHEEL") && buffer.before == 'B' && buffer.after == 'A');
        read[0] = Ordinal(profile.id, ui ? Call::UI_KEY_GETBINDINGBUF : Call::CG_KEY_GETBINDINGBUF);
        read[1] = 184;
        assert(dispatch(read) == 0 && buffer.value[0] == 0);
        read[1] = INT_MAX;
        assert(dispatch(read) == 0 && buffer.value[0] == 0);
        read[2] = 0; read[3] = 0; const int before = keyReads;
        assert(dispatch(read) == 0 && keyReads == before);
        read[3] = 1; bool threw = false;
        try { dispatch(read); } catch (int) { threw = true; }
        assert(threw && keyReads == before);
    }
    keyBindings[K_MOUSE6] = "duplicate";
    keyBindings[K_JOY3] = "duplicate";
    keyBindings[K_AUX1] = "duplicate";
    assert(CL_RTCWKeyForBinding("DUPLICATE") == 187);
    assert(CL_RTCWKeyForBinding(nullptr) == -1 && CL_RTCWKeyForBinding("absent") == -1);
    if (!ui) {
        intptr_t find[] = {Ordinal(profile.id, Call::CG_KEY_GETKEY), (intptr_t)"DUPLICATE"};
        assert(dispatch(find) == 187);
    }
    for (int value = 0; value < 256; ++value) {
        keyCatcher = KEYCATCH_BROWSER | KEYCATCH_RETAIL_MOUSEPASS | 15;
        intptr_t set[] = {Ordinal(profile.id, ui ? Call::UI_KEY_SETCATCHER : Call::CG_KEY_SETCATCHER), value};
        intptr_t get[] = {Ordinal(profile.id, ui ? Call::UI_KEY_GETCATCHER : Call::CG_KEY_GETCATCHER)};
        assert(dispatch(set) == 0 && keyCatcher == (KEYCATCH_BROWSER | (value & 15)));
        assert(dispatch(get) == (value & 15));
        keyCatcher = 0; assert(dispatch(set) == 0 && keyCatcher == (value & 15));
    }
    if (ui) {
        for (int value : {0, 1, INT_MIN, INT_MAX}) {
            intptr_t args[] = {Ordinal(profile.id, Call::UI_KEY_SETOVERSTRIKEMODE), value};
            assert(dispatch(args) == 0 && overstrike == (value ? qtrue : qfalse));
        }
    }
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


def source(include_copy=False):
    keys = (ROOT / "code/client/cl_keys.cpp").read_text(encoding="utf-8")
    shared = (ROOT / "code/qcommon/q_shared.c").read_text(encoding="utf-8")
    result = BACKEND + function(shared, "int Q_stricmp(") + "\n"
    if include_copy:
        result += function(shared, "void Q_strncpyz(") + "\n"
    for signature in ("qboolean CL_RTCWKeyIsDown(", "void CL_RTCWSetBinding(",
                      "void CL_RTCWKeyString(", "int CL_RTCWKeyForBinding(",
                      "int CL_RTCWGetKeyCatcher(", "void CL_RTCWSetKeyCatcher("):
        result += function(keys, signature) + "\n"
    return result
