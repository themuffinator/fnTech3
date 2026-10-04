/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_sp_game_api.h"
#include "../code/compat/jka_sp_cgame_abi.h"
#include <cassert>
#include <cstring>

#define OFFSET(type, member, expected) static_assert(offsetof(type, member) == expected)
static_assert(FNTECH3_JKA_SP_GAME_API_VERSION == 8);
static_assert(FNTECH3_JKA_SP_IMPORT_COUNT == 131);
static_assert(FNTECH3_JKA_SP_EXPORT_COUNT == 19);
static_assert(FNTECH3_JKA_SP_CG_CVAR_REGISTER == 3);
static_assert(FNTECH3_JKA_SP_CG_ARGS == 8);
static_assert(FNTECH3_JKA_SP_CG_FS_FOPENFILE == 9);
static_assert(FNTECH3_JKA_SP_CG_FS_READ == 10);
static_assert(FNTECH3_JKA_SP_CG_FS_WRITE == 11);
static_assert(FNTECH3_JKA_SP_CG_FS_FCLOSEFILE == 12);
static_assert(FNTECH3_JKA_SP_FS_READ == 0 && FNTECH3_JKA_SP_FS_WRITE == 1);
static_assert(FNTECH3_JKA_SP_FS_APPEND == 2 && FNTECH3_JKA_SP_FS_APPEND_SYNC == 3);
static_assert(sizeof(fntech3_jka_sp_vmCvar_t) == 272);
OFFSET(fntech3_jka_sp_vmCvar_t, string, 16);
static_assert(sizeof(fntech3_jka_sp_snapshot_t) == 144248);
OFFSET(fntech3_jka_sp_snapshot_t, snapFlags, 0);
OFFSET(fntech3_jka_sp_snapshot_t, ping, 4);
OFFSET(fntech3_jka_sp_snapshot_t, serverTime, 8);
OFFSET(fntech3_jka_sp_snapshot_t, areamask, 12);
OFFSET(fntech3_jka_sp_snapshot_t, cmdNum, 44);
OFFSET(fntech3_jka_sp_snapshot_t, ps, 48);
OFFSET(fntech3_jka_sp_snapshot_t, numEntities, 4964);
OFFSET(fntech3_jka_sp_snapshot_t, entities, 4968);
OFFSET(fntech3_jka_sp_snapshot_t, private_tail, 144232);
OFFSET(fntech3_jka_sp_snapshot_t, serverCommandSequence, 144244);
static_assert(sizeof(fntech3_jka_sp_snapshot_t::areamask) == 32);
static_assert(sizeof(fntech3_jka_sp_snapshot_t::entities) == 512 * 272);
static_assert(sizeof(fntech3_jka_sp_usercmd_t) == 28);
OFFSET(fntech3_jka_sp_usercmd_t, weapon, 8);
OFFSET(fntech3_jka_sp_usercmd_t, angles, 12);
OFFSET(fntech3_jka_sp_usercmd_t, generic_cmd, 24);
OFFSET(fntech3_jka_sp_usercmd_t, upmove, 27);
static_assert(sizeof(fntech3_jka_sp_trajectory_t) == 36);
static_assert(sizeof(fntech3_jka_sp_entityState_t) == 272);
OFFSET(fntech3_jka_sp_entityState_t, pos, 12);
OFFSET(fntech3_jka_sp_entityState_t, apos, 48);
OFFSET(fntech3_jka_sp_entityState_t, origin, 92);
OFFSET(fntech3_jka_sp_entityState_t, modelindex, 160);
OFFSET(fntech3_jka_sp_entityState_t, modelindex3, 168);
OFFSET(fntech3_jka_sp_entityState_t, solid, 180);
OFFSET(fntech3_jka_sp_entityState_t, vehicleAngles, 228);
OFFSET(fntech3_jka_sp_entityState_t, modelScale, 248);
OFFSET(fntech3_jka_sp_entityState_t, isPortalEnt, 268);
static_assert(sizeof(fntech3_jka_sp_gentity_prefix_t) == 376);
OFFSET(fntech3_jka_sp_gentity_prefix_t, client_address, 272);
OFFSET(fntech3_jka_sp_gentity_prefix_t, inuse, 276);
OFFSET(fntech3_jka_sp_gentity_prefix_t, linked, 280);
OFFSET(fntech3_jka_sp_gentity_prefix_t, svFlags, 284);
OFFSET(fntech3_jka_sp_gentity_prefix_t, bmodel, 288);
OFFSET(fntech3_jka_sp_gentity_prefix_t, mins, 292);
OFFSET(fntech3_jka_sp_gentity_prefix_t, maxs, 304);
OFFSET(fntech3_jka_sp_gentity_prefix_t, contents, 316);
OFFSET(fntech3_jka_sp_gentity_prefix_t, absmin, 320);
OFFSET(fntech3_jka_sp_gentity_prefix_t, absmax, 332);
OFFSET(fntech3_jka_sp_gentity_prefix_t, currentOrigin, 344);
OFFSET(fntech3_jka_sp_gentity_prefix_t, currentAngles, 356);
OFFSET(fntech3_jka_sp_gentity_prefix_t, owner_address, 368);
OFFSET(fntech3_jka_sp_gentity_prefix_t, ghoul2_handle, 372);
static_assert(sizeof(fntech3_jka_sp_collision_t) == 64);
OFFSET(fntech3_jka_sp_collision_t, collisionPosition, 20);
OFFSET(fntech3_jka_sp_collision_t, collisionNormal, 32);
OFFSET(fntech3_jka_sp_collision_t, barycentricI, 56);
static_assert(sizeof(fntech3_jka_sp_trace_t) == 1080);
OFFSET(fntech3_jka_sp_trace_t, fraction, 8);
OFFSET(fntech3_jka_sp_trace_t, plane, 24);
OFFSET(fntech3_jka_sp_trace_t, entityNum, 52);
OFFSET(fntech3_jka_sp_trace_t, collision, 56);

static void Init(const char *,const char *,int32_t,const char *,int32_t,int32_t,int32_t,int32_t,int32_t) {}
static void Shutdown() {}
int main() {
	int count = 0;
#define FNTECH3_JKA_SP_IMPORT(name, slot) \
	assert(FNTECH3_JKA_SP_IMPORT_##name == count++); \
	assert(!std::strcmp(fntech3::jka::sp::ImportName(slot), #name));
#include "../code/compat/jka_sp_imports.def"
#undef FNTECH3_JKA_SP_IMPORT
	assert(count == 131);
	assert(!fntech3::jka::sp::ImportName(-1));
	assert(!fntech3::jka::sp::ImportName(131));
	assert(FNTECH3_JKA_SP_IMPORT_AppendToSaveGame == 18);
	assert(FNTECH3_JKA_SP_IMPORT_ReadFromSaveGameOptional == 20);
	assert(FNTECH3_JKA_SP_IMPORT_trace == 30);
	assert(FNTECH3_JKA_SP_IMPORT_VoiceVolume == 41);
	assert(FNTECH3_JKA_SP_IMPORT_G2API_PrecacheGhoul2Model == 45);
	assert(FNTECH3_JKA_SP_IMPORT_WE_SetTempGlobalFogColor == 130);
	using fntech3::jka::sp::Validate;
	fntech3_jka_sp_game_export_t game{};
	assert(Validate(nullptr));
	game.apiversion = 10; assert(Validate(&game));
	game.apiversion = 8; assert(Validate(&game));
	game.Init = Init; game.Shutdown = Shutdown;
	game.gentitySize = 1252; assert(!Validate(&game));
	for (int bad : {-1,0,376,1251,1253}) { game.gentitySize = bad; assert(Validate(&game)); }
	game.gentitySize = 1252;
	for (int bad : {-1,1025}) { game.num_entities = bad; assert(Validate(&game)); }
	game.num_entities = 1; assert(Validate(&game));
	game.gentities = reinterpret_cast<void *>(uintptr_t(4096));
	for (int good : {0,1,1024}) { game.num_entities = good; assert(!Validate(&game)); }
	game.gentities = reinterpret_cast<void *>((std::numeric_limits<uintptr_t>::max)() - 1);
	assert(Validate(&game));
	return 0;
}
