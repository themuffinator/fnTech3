/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_CGAME_ABI_H
#define FNTECH3_COMPAT_JKA_SP_CGAME_ABI_H
#include <stdint.h>
#include "jka_sp_game_abi.h"
#include "jka_sp_player_abi.h"

/* The SP DLL contains both GetGameAPI and vmMain/dllEntry. Retail dllEntry
 * 0x201438c0 installs its __cdecl syscall then enters CG_PreInit, which calls
 * its cvar loop 0x2012b1e0 before server Init. Wrappers 0x201438d0..0x201439aa
 * establish these first eight ordinals and argument order independently of
 * the MP import table. OpenJK code/cgame/cg_public.h names the same calls. */
enum {
	FNTECH3_JKA_SP_CG_PRINT = 0,
	FNTECH3_JKA_SP_CG_ERROR = 1,
	FNTECH3_JKA_SP_CG_MILLISECONDS = 2,
	FNTECH3_JKA_SP_CG_CVAR_REGISTER = 3,
	FNTECH3_JKA_SP_CG_CVAR_UPDATE = 4,
	FNTECH3_JKA_SP_CG_CVAR_SET = 5,
	FNTECH3_JKA_SP_CG_ARGC = 6,
	FNTECH3_JKA_SP_CG_ARGV = 7,
	/* jasp's switch0x401530 uses table0x4021b8: slots8..12 branch to
	 * 0x4015ec/0x4015fe/0x401617/0x401632/0x40164d. ARGS accepts
	 * (char *,int32_t); OPEN (const char *,int32_t *,int32_t) returns
	 * length/-1; READ/WRITE (buffer,int32_t length,int32_t handle) and
	 * CLOSE (int32_t handle) return zero, discarding the FS byte count.
	 * ARGS joins argv1 onward with a space after each argument, including
	 * the final one (retail0x419bb0..0x419c31; maintained source differs).
	 * Module wrappers0x201439b0/0x201439d0/0x201439f0 corroborate9/10/12.
	 * These general services do not serve the SP client records. */
	FNTECH3_JKA_SP_CG_ARGS = 8,
	FNTECH3_JKA_SP_CG_FS_FOPENFILE = 9,
	FNTECH3_JKA_SP_CG_FS_READ = 10,
	FNTECH3_JKA_SP_CG_FS_WRITE = 11,
	FNTECH3_JKA_SP_CG_FS_FCLOSEFILE = 12,
	/* Retail wrapper0x20144370 pushes78,snapshotNumber,snapshotPointer.
	 * Measured for this record freeze; the service remains unserved. */
	FNTECH3_JKA_SP_CG_GETSNAPSHOT = 78
};
/* FS_FOpenFileByMode0x41efc0 dispatches these four values; the handle
 * destination is required even on a read failure. */
enum {
	FNTECH3_JKA_SP_FS_READ = 0,
	FNTECH3_JKA_SP_FS_WRITE = 1,
	FNTECH3_JKA_SP_FS_APPEND = 2,
	FNTECH3_JKA_SP_FS_APPEND_SYNC = 3
};
/* jasp.exe Cvar_Update 0x41c790 checks handle+0 and modificationCount+4,
 * stores value+8/integer+12 and copies at most 256 bytes to string+16.
 * No QL flags word is present. */
typedef struct {
	int32_t handle, modificationCount;
	float value;
	int32_t integer;
	char string[256];
} fntech3_jka_sp_vmCvar_t;
/* The retail SP snapshot includes ping at+4, which is absent from the pinned
 * modern OpenJK declaration. jasp CL_GetSnapshot0x401111..0x4011bf fixes
 * every named offset below,512 entities and272-byte strides. Its retail
 * module alternates buffers0x2070c090/0x2072f408 at0x20143480..0x20143496,
 * independently establishing the complete144248-byte size. Unwritten tail
 * bytes have no measured meanings here and remain opaque. Freezing this
 * record does not serve CG_GETSNAPSHOT or the local client boundary. */
enum {
	FNTECH3_JKA_SP_SNAPSHOT_BYTES = 144248,
	FNTECH3_JKA_SP_SNAPSHOT_ENTITIES = 512
};
typedef struct {
	int32_t snapFlags, ping, serverTime;
	uint8_t areamask[32];
	int32_t cmdNum;
	fntech3_jka_sp_playerState_t ps;
	int32_t numEntities;
	fntech3_jka_sp_entityState_t entities[FNTECH3_JKA_SP_SNAPSHOT_ENTITIES];
	uint8_t private_tail[12];
	int32_t serverCommandSequence;
} fntech3_jka_sp_snapshot_t;
#ifdef __cplusplus
#include <cstddef>
static_assert(sizeof(fntech3_jka_sp_vmCvar_t) == 272);
static_assert(offsetof(fntech3_jka_sp_vmCvar_t,string) == 16);
static_assert(sizeof(fntech3_jka_sp_snapshot_t) == FNTECH3_JKA_SP_SNAPSHOT_BYTES);
#endif
#endif
