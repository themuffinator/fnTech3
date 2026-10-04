/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_KEY_ADAPTER_H
#define FNTECH3_JKA_KEY_ADAPTER_H
#include "../client/keycodes.h"
#include <stdint.h>

/* Read-only binding-slot mapping, not an input-event adapter. Retail JKA1.01
 * jamp.exe (SHA256 in jka_client_abi.h) table 0x570bc8 contains 320 16-byte
 * key-name rows. SetBinding 0x41bd00 stores at table.upper; UI GetBindingBuf
 * 0x423ed0 reads its raw slot. Consequently an ASCII letter's uppercase slot
 * holds the binding and its lowercase slot is empty. The host uses lowercase
 * letter slots. Numeric/key-name facts were measured independently; no source
 * implementation text is incorporated. The host's coalesced right modifiers,
 * shifted pseudo-keys, extended character slots and AUX16..31 have no distinct
 * binding slot here and must never alias an unrelated engine key. */
static inline int FNTech3_JKAUIBindingKey(intptr_t external) {
    if (external < 0 || external >= 320) return -1;
    if (external >= 'A' && external <= 'Z') return (int)external + ('a'-'A');
    if (external >= 'a' && external <= 'z') return -1;
    if (external >= 32 && external <= 126) return (int)external;
    if (external >= 256 && external <= 287) return K_JOY1 + (int)external - 256;
    if (external >= 288 && external <= 303) return K_AUX1 + (int)external - 288;
    switch (external) {
    case 1: return K_SHIFT; case 2: return K_CTRL; case 3: return K_ALT;
    case 4: return K_CAPSLOCK; case 5: return K_KP_NUMLOCK; case 6: return K_SCROLLOCK;
    case 7: return K_PAUSE; case 8: return K_BACKSPACE; case 9: return K_TAB; case 10: return K_ENTER;
    case 11: return K_KP_PLUS; case 12: return K_KP_MINUS; case 13: return K_KP_ENTER;
    case 14: return K_KP_DEL; case 15: return K_PRINT; case 16: return K_KP_INS;
    case 17: return K_KP_END; case 18: return K_KP_DOWNARROW; case 19: return K_KP_PGDN;
    case 20: return K_KP_LEFTARROW; case 21: return K_KP_5; case 22: return K_KP_RIGHTARROW;
    case 23: return K_KP_HOME; case 24: return K_KP_UPARROW; case 25: return K_KP_PGUP;
    case 26: return K_CONSOLE; case 27: return K_ESCAPE;
    case 28: return K_F1; case 29: return K_F2; case 30: return K_F3; case 31: return K_F4;
    case 127: return K_DEL; case 128: return K_EURO;
    case 132: return K_F5; case 133: return K_F6; case 134: return K_F7; case 135: return K_F8;
    case 137: return K_MWHEELUP; case 139: return K_MWHEELDOWN;
    case 141: return K_MOUSE1; case 142: return K_MOUSE2; case 143: return K_INS;
    case 144: return K_HOME; case 145: return K_PGUP;
    case 149: return K_F9; case 150: return K_F10; case 151: return K_F11; case 152: return K_F12;
    case 157: return K_END; case 158: return K_PGDN;
    case 166: return K_MOUSE3; case 167: return K_MOUSE4; case 168: return K_MOUSE5;
    case 170: return K_UPARROW; case 171: return K_DOWNARROW; case 172: return K_LEFTARROW; case 173: return K_RIGHTARROW;
    case 215: return K_KP_STAR; case 247: return K_KP_SLASH;
    default: return -1;
    }
}
#endif
