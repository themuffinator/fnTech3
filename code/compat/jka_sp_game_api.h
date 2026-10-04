/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_GAME_API_H
#define FNTECH3_COMPAT_JKA_SP_GAME_API_H
#include "jka_module_abi.h"
#include "jka_sp_game_abi.h"
#include <stdint.h>

/* Retail 1.01 GetGameAPI copies 0x83 dwords at 0x20073439 and returns
 * its API-8 table at 0x201f4bf8. jasp.exe independently assembles these
 * imports at 0x4392e8..0x439823 and checks API 8 at 0x439891.
 * The source-carried API 10 is NOT this interface: it replaces three save
 * callbacks with one C++ saved-game object. Every callback is x86 __cdecl.
 * The 80-byte gap before retail imports includes four bytes of linker
 * alignment: the 19 export members occupy 76 bytes, not 80. */
enum {
	FNTECH3_JKA_SP_IMPORT_COUNT = 131,
	FNTECH3_JKA_SP_EXPORT_COUNT = 19,
#define FNTECH3_JKA_SP_IMPORT(name, slot) FNTECH3_JKA_SP_IMPORT_##name = slot,
#include "jka_sp_imports.def"
#undef FNTECH3_JKA_SP_IMPORT
};
/* Slot storage preserves unserved signatures without pretending they are
 * implemented. A served slot is assigned/called only with its frozen type.
 * Data slot VoiceVolume carries an object pointer, not a callback. */
typedef struct {
	uintptr_t slots[FNTECH3_JKA_SP_IMPORT_COUNT];
} fntech3_jka_sp_game_import_t;
/* Served file callbacks. Retail's long is a signed Win32 32-bit scalar. */
typedef int32_t (*fntech3_jka_sp_FS_FOpenFile_t)(const char *, int32_t *, int32_t);
typedef int32_t (*fntech3_jka_sp_FS_Read_t)(void *, int32_t, int32_t);
typedef int32_t (*fntech3_jka_sp_FS_Write_t)(const void *, int32_t, int32_t);
typedef void (*fntech3_jka_sp_FS_FCloseFile_t)(int32_t);
typedef int32_t (*fntech3_jka_sp_FS_ReadFile_t)(const char *, void **);
typedef void (*fntech3_jka_sp_FS_FreeFile_t)(void *);
typedef int32_t (*fntech3_jka_sp_FS_GetFileList_t)(const char *, const char *, char *, int32_t);
typedef int32_t (*fntech3_jka_sp_RE_GetAnimationCFG_t)(const char *, char *, int32_t);
typedef struct {
	int32_t apiversion;
	void (*Init)(const char *, const char *, int32_t, const char *,
		int32_t, int32_t, int32_t, int32_t, int32_t);
	void (*Shutdown)(void);
	void (*WriteLevel)(int32_t);
	void (*ReadLevel)(int32_t, int32_t);
	int32_t (*GameAllowedToSaveHere)(void);
	char *(*ClientConnect)(int32_t, int32_t, int32_t);
	void (*ClientBegin)(int32_t, fntech3_jka_sp_usercmd_t *, int32_t);
	void (*ClientUserinfoChanged)(int32_t);
	void (*ClientDisconnect)(int32_t);
	void (*ClientCommand)(int32_t);
	void (*ClientThink)(int32_t, fntech3_jka_sp_usercmd_t *);
	void (*RunFrame)(int32_t);
	void (*ConnectNavs)(const char *, int32_t);
	int32_t (*ConsoleCommand)(void);
	void (*GameSpawnRMGEntity)(char *);
	void *gentities;
	int32_t gentitySize, num_entities;
} fntech3_jka_sp_game_export_t;
typedef fntech3_jka_sp_game_export_t *(*fntech3_jka_sp_GetGameAPI_t)(fntech3_jka_sp_game_import_t *);

#ifdef __cplusplus
#include <cstddef>
#include <limits>
namespace fntech3::jka::sp {
inline constexpr const char *importNames[] = {
#define FNTECH3_JKA_SP_IMPORT(name, slot) #name,
#include "jka_sp_imports.def"
#undef FNTECH3_JKA_SP_IMPORT
};
inline constexpr const char *ImportName(int slot) {
	return slot >= 0 && slot < FNTECH3_JKA_SP_IMPORT_COUNT ? importNames[slot] : nullptr;
}
/* Table validation precedes any export call. Entity allocation stays in the
 * game; only its declared bounds are validated here. World access requires
 * the SP field adapter; its frozen client/snapshot records remain unserved. */
inline constexpr const char *Validate(const fntech3_jka_sp_game_export_t *game) {
	if (!game) return "no export table";
	if (game->apiversion != FNTECH3_JKA_SP_GAME_API_VERSION) return "requires retail API 8";
	if (!game->Init || !game->Shutdown) return "missing Init or Shutdown";
	if (game->gentitySize != FNTECH3_JKA_SP_GENTITY_BYTES) return "unsupported entity stride";
	if (game->num_entities < 0 || game->num_entities > FNTECH3_JKA_SP_MAX_GENTITIES)
		return "entity count outside allocation";
	if (game->num_entities && !game->gentities) return "entity count without allocation";
	const auto base = reinterpret_cast<uintptr_t>(game->gentities);
	const auto span = uintptr_t(game->gentitySize) * FNTECH3_JKA_SP_MAX_GENTITIES;
	if (base && base > (std::numeric_limits<uintptr_t>::max)() - span)
		return "entity allocation address overflows";
	return nullptr;
}
static_assert(sizeof(importNames) / sizeof(importNames[0]) == FNTECH3_JKA_SP_IMPORT_COUNT);
#if UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(fntech3_jka_sp_game_import_t) == 524);
static_assert(sizeof(fntech3_jka_sp_game_export_t) == 76);
static_assert(offsetof(fntech3_jka_sp_game_export_t, gentities) == 64);
static_assert(offsetof(fntech3_jka_sp_game_export_t, num_entities) == 72);
#endif
}
#endif
#endif
