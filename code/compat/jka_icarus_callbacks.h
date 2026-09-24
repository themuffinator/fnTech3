/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_ICARUS_CALLBACKS_H
#define FNTECH3_COMPAT_JKA_ICARUS_CALLBACKS_H

/* The records Jedi Academy's engine and game pass ICARUS commands in.
 *
 * These do not travel in a call's arguments. The module lends the engine one
 * scratch buffer at startup -- that is what G_SET_SHARED_BUFFER is for -- and
 * a command the engine hands back to the game is written into that buffer and
 * the game's export is called with nothing else. So these layouts are a wire
 * contract in every sense: a member in the wrong place is read as another.
 *
 * Frozen Win32 x86 layouts, member names and order read as interoperability
 * facts from Raven's released multiplayer sources as carried by OpenJK at
 * 1a6a643427aa347553e9073dac5570b33337c4d9, codemp/game/g_public.h:271-370.
 * No implementation text is incorporated. Every size and offset below is
 * asserted at compile time, and tests/jka_icarus_callback_tests.cpp holds the
 * whole set to the table.
 *
 * Note that Raven did not order these the same way throughout: most begin with
 * the task identifier and then the entity, and the two lerp endpoints begin
 * with the entity. That is preserved exactly, because the module reads them
 * where they are and not where they would be tidier. */

#include <stdint.h>

/* Raven's own bound for every string in these records. */
#define FNTECH3_JKA_ICARUS_STRING_BYTES 2048

typedef struct {
	int32_t taskID;
	int32_t entID;
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
	char channel[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_playsound_t;

typedef struct {
	int32_t taskID;
	int32_t entID;
	char type_name[FNTECH3_JKA_ICARUS_STRING_BYTES];
	char data[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_set_t;

typedef struct {
	int32_t taskID;
	int32_t entID;
	float origin[3];
	float angles[3];
	float duration;
	int32_t nullAngles;
} fntech3_jka_icarus_lerp2pos_t;

typedef struct {
	int32_t taskID;
	int32_t entID;
	float origin[3];
	float duration;
} fntech3_jka_icarus_lerp2origin_t;

typedef struct {
	int32_t taskID;
	int32_t entID;
	float angles[3];
	float duration;
} fntech3_jka_icarus_lerp2angles_t;

typedef struct {
	int32_t entID;
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
	int32_t lookup;
	float info[3];
} fntech3_jka_icarus_gettag_t;

/* The entity comes first in these two, and the task identifier second. */
typedef struct {
	int32_t entID;
	int32_t taskID;
	float duration;
} fntech3_jka_icarus_lerp2start_t;

typedef struct {
	int32_t entID;
	int32_t taskID;
	float duration;
} fntech3_jka_icarus_lerp2end_t;

typedef struct {
	int32_t entID;
	char target[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_use_t;

typedef struct {
	int32_t entID;
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_kill_t;

typedef struct {
	int32_t entID;
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_remove_t;

typedef struct {
	int32_t taskID;
	int32_t entID;
	char type[FNTECH3_JKA_ICARUS_STRING_BYTES];
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_play_t;

typedef struct {
	int32_t entID;
	int32_t type;
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
	float value;
} fntech3_jka_icarus_getfloat_t;

typedef struct {
	int32_t entID;
	int32_t type;
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
	float value[3];
} fntech3_jka_icarus_getvector_t;

typedef struct {
	int32_t entID;
	int32_t type;
	char name[FNTECH3_JKA_ICARUS_STRING_BYTES];
	char value[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_getstring_t;

typedef struct {
	char filename[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_soundindex_t;

typedef struct {
	char string[FNTECH3_JKA_ICARUS_STRING_BYTES];
} fntech3_jka_icarus_setidforstring_t;

/* The largest of them, which is what a shared buffer has to be able to hold
 * before any of these may be written into it. */
#define FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES 4104

#endif
