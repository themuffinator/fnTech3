/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Academy's key events: the numbers retail jamp.exe 1.01 hands its UI and
 * client game, and the arguments its UI key export takes. */
#include "../code/compat/jka_key_adapter.h"
#include "../code/compat/jka_ui_exports.h"
#include <cstdio>

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)

/* Rows of the retail key-name table (jamp.exe 0x570bc8, 16-byte rows,
 * number at +8), read with capstone/pefile on 2026-10-15
 * (docs/fntech3/2026-10-15-jka-weapon-fire.json). */
struct Row { int internal; int academy; };
static const Row kRetailRows[] = {
    {K_SHIFT, 1}, {K_CTRL, 2}, {K_ALT, 3}, {K_CAPSLOCK, 4}, {K_KP_NUMLOCK, 5},
    {K_SCROLLOCK, 6}, {K_PAUSE, 7}, {K_BACKSPACE, 8}, {K_TAB, 9}, {K_ENTER, 10},
    {K_KP_PLUS, 11}, {K_KP_MINUS, 12}, {K_KP_ENTER, 13}, {K_KP_DEL, 14},
    {K_KP_INS, 16}, {K_KP_END, 17}, {K_KP_DOWNARROW, 18}, {K_KP_PGDN, 19},
    {K_KP_LEFTARROW, 20}, {K_KP_5, 21}, {K_KP_RIGHTARROW, 22}, {K_KP_HOME, 23},
    {K_KP_UPARROW, 24}, {K_KP_PGUP, 25}, {K_CONSOLE, 26}, {K_ESCAPE, 27},
    {K_F1, 28}, {K_F4, 31}, {K_SPACE, 32}, {K_SEMICOLON, 59}, {K_DEL, 127}, {K_EURO, 128},
    {K_F5, 132}, {K_F8, 135}, {K_MWHEELUP, 137}, {K_MWHEELDOWN, 139},
    {K_MOUSE1, 141}, {K_MOUSE2, 142}, {K_INS, 143}, {K_HOME, 144}, {K_PGUP, 145},
    {K_F9, 149}, {K_F12, 152}, {K_END, 157}, {K_PGDN, 158},
    {K_MOUSE3, 166}, {K_MOUSE4, 167}, {K_MOUSE5, 168},
    {K_UPARROW, 170}, {K_DOWNARROW, 171}, {K_LEFTARROW, 172}, {K_RIGHTARROW, 173},
    {K_KP_STAR, 215}, {K_KP_SLASH, 247},
    {K_JOY1, 256}, {K_JOY32, 287}, {K_AUX1, 288}, {K_AUX16, 303},
};

static int Numbers() {
    for (const Row &row : kRetailRows) CHECK(FNTech3_JKAKeyFromInternal(row.internal) == row.academy);
    /* Letters and other printable keys keep their characters, lowercase. */
    for (int key = 'a'; key <= 'z'; ++key) CHECK(FNTech3_JKAKeyFromInternal(key) == key);
    for (int key = '0'; key <= '9'; ++key) CHECK(FNTech3_JKAKeyFromInternal(key) == key);
    /* Keys Academy has no row for are not delivered. */
    const int unknown[] = {K_MOUSE6, K_MOUSE9, K_F13, K_F15, K_KP_EQUALS, K_COMMAND, K_POWER, K_SUPER,
                           K_PAD0_SOUTH, K_PAD0_TOUCHPAD, 0, 1, 14, 31, -1, MAX_KEYS};
    for (int key : unknown) CHECK(FNTech3_JKAKeyFromInternal(key) == -1);
    return 0;
}

/* Every number a key event hands Academy names the same key in the binding
 * table the UI reads (FNTech3_JKAUIBindingKey), whose letter slots are the
 * uppercase rows. */
static int RoundTrip() {
    int delivered = 0;
    for (int key = 0; key < MAX_KEYS; ++key) {
        if (key >= 'A' && key <= 'Z') continue; // the engine's letter keys are lowercase
        const int academy = FNTech3_JKAKeyFromInternal(key);
        if (academy < 0) continue;
        ++delivered;
        CHECK(academy < 320);
        const int slot = academy >= 'a' && academy <= 'z' ? academy - ('a' - 'A') : academy;
        CHECK(FNTech3_JKAUIBindingKey(slot) == key);
    }
    CHECK(delivered > 150);
    return 0;
}

/* Retail CL_CharEvent 0x41c640: (character | 0x400, down) for every byte but
 * '`' and '~'; key events go through the table. */
static int Events() {
    CHECK(K_CHAR_FLAG == 0x400);
    CHECK(FNTech3_JKAKeyEventFromInternal(K_CHAR_FLAG | 'q') == (0x400 | 'q'));
    CHECK(FNTech3_JKAKeyEventFromInternal(K_CHAR_FLAG | 0xe9) == (0x400 | 0xe9));
    CHECK(FNTech3_JKAKeyEventFromInternal(K_CHAR_FLAG | '\r') == (0x400 | '\r'));
    CHECK(FNTech3_JKAKeyEventFromInternal(K_CHAR_FLAG | '`') == -1);
    CHECK(FNTech3_JKAKeyEventFromInternal(K_CHAR_FLAG | '~') == -1);
    CHECK(FNTech3_JKAKeyEventFromInternal(K_CHAR_FLAG | 0x100) == -1);
    CHECK(FNTech3_JKAKeyEventFromInternal(K_ESCAPE) == 27);
    CHECK(FNTech3_JKAKeyEventFromInternal(K_BACKSPACE) == 8);
    CHECK(FNTech3_JKAKeyEventFromInternal(K_MOUSE6) == -1);
    return 0;
}

/* The engine's (key, down, time) reaches vmMain(3, key, down); every other
 * export keeps its exact count. */
static int Exports() {
    CHECK(FNTech3_JKAUIForwardedArguments(UI_KEY_EVENT, 3) == 2);
    CHECK(FNTech3_JKAUIForwardedArguments(UI_KEY_EVENT, 2) == 2);
    for (int nargs : {0, 1, 4}) CHECK(FNTech3_JKAUIForwardedArguments(UI_KEY_EVENT, nargs) == -1);
    CHECK(FNTech3_JKAUIForwardedArguments(UI_MOUSE_EVENT, 2) == 2);
    CHECK(FNTech3_JKAUIForwardedArguments(UI_MOUSE_EVENT, 3) == -1);
    CHECK(FNTech3_JKAUIForwardedArguments(UI_SET_ACTIVE_MENU, 1) == 1);
    CHECK(FNTech3_JKAUIForwardedArguments(UI_SET_ACTIVE_MENU, 2) == -1);
    CHECK(FNTech3_JKAUIForwardedArguments(UI_REFRESH_DISPLAY_CONTEXT, 0) == -1);
    CHECK(FNTech3_JKAUIForwardedArguments(-1, 0) == -1);
    return 0;
}

int main() {
    if (const int line = Numbers()) return line;
    if (const int line = RoundTrip()) return line;
    if (const int line = Events()) return line;
    if (const int line = Exports()) return line;
    std::puts("Academy key numbers, character events and the UI key export passed");
    return 0;
}
