/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Optional fixture generator. Compile against a user-supplied GPL Lilium
 * Voyager checkout's qcommon/msg.c and huffman.c with -DELITEFORCE.
 * It exercises reference code without incorporating it into fnTech3.
 * Pinned reference: 0f7dcd89d142c8b635ce2756a80cb62411731848. */
#include "q_shared.h"
#include "qcommon.h"

cvar_t *cl_shownet;
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_Error(int code, const char *format, ...) {
	va_list args;
	(void)code;
	va_start(args, format); vfprintf(stderr, format, args); va_end(args);
	exit(1);
}
void Q_strncpyz(char *out, const char *in, int size) {
	if (size > 0) { strncpy(out, in, size - 1); out[size - 1] = 0; }
}
int QDECL Com_sprintf(char *out, int size, const char *format, ...) {
	va_list args;
	int result;
	va_start(args, format); result = vsnprintf(out, size, format, args); va_end(args);
	return result;
}

typedef entityState_t IOEF_Entity;
typedef playerState_t IOEF_Player;
typedef usercmd_t IOEF_Command;
#include "stvef_ioef_fixture.h"

static byte storage[16384];
static msg_t msg;
static void Begin(void) { memset(storage, 0, sizeof(storage)); MSG_Init(&msg, storage, sizeof(storage)); }
static void Dump(const char *name) {
	int i;
	printf("IOEF_VECTOR(%s, %d, \"", name, msg.bit);
	for (i = 0; i < msg.cursize; ++i) printf("%02x", storage[i]);
	puts("\")");
}

int main(void) {
	entityState_t entity = {0}, entityBase = {0};
	playerState_t player = {0};
	usercmd_t command = {0}, commandBase = {0};
	IOEF_FillEntity(&entity);
	Begin(); MSG_WriteDeltaEntity(&msg, &entityBase, &entity, qfalse); Dump("entity_all");
	entityBase = entity;
	memset(&entity, 0, sizeof(entity)); entity.number = 7; entity.origin[0] = -0.0f;
	Begin(); MSG_WriteDeltaEntity(&msg, &entityBase, &entity, qfalse); Dump("entity_zero");
	Begin(); MSG_WriteDeltaEntity(&msg, &entityBase, NULL, qfalse); Dump("entity_remove");
	Begin(); MSG_WriteDeltaEntity(&msg, &entityBase, &entityBase, qtrue); Dump("entity_same");

	IOEF_FillPlayer(&player);
	Begin(); MSG_WriteDeltaPlayerstate(&msg, NULL, &player); Dump("player_all");
	Begin(); MSG_WriteDeltaPlayerstate(&msg, &player, &player); Dump("player_same");

	commandBase.serverTime = 100000; IOEF_FillCommand(&command);
	Begin(); MSG_WriteDeltaUsercmd(&msg, &commandBase, &command); Dump("command_all");
	commandBase = command; command.serverTime += 300;
	Begin(); MSG_WriteDeltaUsercmd(&msg, &commandBase, &command); Dump("command_same");
	Begin(); MSG_WriteBits(&msg, 5, 3); MSG_WriteString(&msg, "h\xe9%"); Dump("string");
	return 0;
}
