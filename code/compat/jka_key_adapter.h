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

/* Key events, the other direction. Retail CL_KeyEvent 0x41c290 hands the UI
 * vmMain(3, key, down) and the cgame vmMain(6, key, down) in the same table's
 * numbering (Escape is 27, both on press and release), and CL_CharEvent
 * 0x41c640 hands the UI (character | 0x400, 1) for every character except
 * '`' and '~', which it drops. A physical key is the row whose name the
 * binding table above reads, lowercase for letters. An engine key with no
 * row (mouse buttons 6-9, F13-F15, keypad equals, the extra pad buttons) is
 * not delivered rather than borrowing another key's number. */
static inline int FNTech3_JKAKeyFromInternal(int key) {
    if (key == K_TAB || key == K_ESCAPE) return key;
    if (key == K_ENTER) return 10;
    if (key == K_BACKSPACE) return 8;
    if (key >= 32 && key <= 126) return key;
    if (key >= K_JOY1 && key <= K_JOY32) return 256 + (key - K_JOY1);
    if (key >= K_AUX1 && key <= K_AUX16) return 288 + (key - K_AUX1);
    switch (key) {
    case K_SHIFT: return 1; case K_CTRL: return 2; case K_ALT: return 3;
    case K_CAPSLOCK: return 4; case K_KP_NUMLOCK: return 5; case K_SCROLLOCK: return 6;
    case K_PAUSE: return 7; case K_KP_PLUS: return 11; case K_KP_MINUS: return 12;
    case K_KP_ENTER: return 13; case K_KP_DEL: return 14; case K_PRINT: return 15;
    case K_KP_INS: return 16; case K_KP_END: return 17; case K_KP_DOWNARROW: return 18;
    case K_KP_PGDN: return 19; case K_KP_LEFTARROW: return 20; case K_KP_5: return 21;
    case K_KP_RIGHTARROW: return 22; case K_KP_HOME: return 23; case K_KP_UPARROW: return 24;
    case K_KP_PGUP: return 25; case K_CONSOLE: return 26;
    case K_F1: return 28; case K_F2: return 29; case K_F3: return 30; case K_F4: return 31;
    case K_DEL: return 127; case K_EURO: return 128;
    case K_F5: return 132; case K_F6: return 133; case K_F7: return 134; case K_F8: return 135;
    case K_MWHEELUP: return 137; case K_MWHEELDOWN: return 139;
    case K_MOUSE1: return 141; case K_MOUSE2: return 142; case K_INS: return 143;
    case K_HOME: return 144; case K_PGUP: return 145;
    case K_F9: return 149; case K_F10: return 150; case K_F11: return 151; case K_F12: return 152;
    case K_END: return 157; case K_PGDN: return 158;
    case K_MOUSE3: return 166; case K_MOUSE4: return 167; case K_MOUSE5: return 168;
    case K_UPARROW: return 170; case K_DOWNARROW: return 171; case K_LEFTARROW: return 172; case K_RIGHTARROW: return 173;
    case K_KP_STAR: return 215; case K_KP_SLASH: return 247;
    default: return -1;
    }
}

static inline int FNTech3_JKAKeyEventFromInternal(int key) {
    if (key & K_CHAR_FLAG) {
        const int character = key & ~K_CHAR_FLAG;
        if (character < 0 || character > 255 || character == '`' || character == '~') return -1;
        return K_CHAR_FLAG | character;
    }
    return FNTech3_JKAKeyFromInternal(key);
}
#endif
