/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_PLAYER_ABI_H
#define FNTECH3_COMPAT_JKA_SP_PLAYER_ABI_H
#include <stdint.h>

/* Academy 1.01 single-player playerState, independently measured in jasp.exe.
 * The server snapshot copy at 0x43c9bc reads gentity.client (+272), then copies
 * 0x4cd dwords from client offset zero. Its client snapshot copy at 0x401148
 * agrees: the record is 4916 bytes, not the 7284-byte private gclient record.
 *
 * All fifty scalar delta offsets are in the retail table at 0x4f91f8 and
 * jka_sp_player_fields.def. The array loops at 0x420cd5..0x421287 independently
 * establish their offsets and counts. The ping write at 0x43a7e1..0x43a7ee
 * establishes offset 640. These facts agree with the x86 OpenJK declarations
 * at 1a6a643427aa347553e9073dac5570b33337c4d9 (GPLv2; observation only).
 *
 * Gaps and the private suffix deliberately have no game member names. In
 * particular the suffix contains native saber data whose individual layout
 * is not frozen here; no engine code may dereference or interpret these bytes.
 * OpenJK's smaller saberInfoRetail-based player type converts old savegames;
 * it is not this measured module-memory record. This header freezes an ABI;
 * it does not establish a savegame or a served client/snapshot boundary. */
enum {
	FNTECH3_JKA_SP_PLAYER_STATE_BYTES = 4916,
	FNTECH3_JKA_SP_PLAYER_NETFIELD_COUNT = 50,
	FNTECH3_JKA_SP_PLAYER_STATS = 16,
	FNTECH3_JKA_SP_PLAYER_PERSISTANT = 16,
	FNTECH3_JKA_SP_PLAYER_POWERUPS = 16,
	FNTECH3_JKA_SP_PLAYER_AMMO = 10,
	FNTECH3_JKA_SP_PLAYER_INVENTORY = 15,
	FNTECH3_JKA_SP_PLAYER_PING_OFFSET = 640
};
typedef struct {
	int32_t commandTime, pm_type, bobCycle, pm_flags, pm_time;
	float origin[3], velocity[3];
	int32_t weaponTime, weaponChargeTime;
	uint8_t private_52[4];
	int32_t gravity, leanofs, friction, speed, delta_angles[3];
	int32_t groundEntityNum, legsAnim;
	uint8_t private_92[4];
	int32_t torsoAnim;
	uint8_t private_100[4];
	int32_t movementDir, eFlags, eventSequence, events[2], eventParms[2];
	int32_t externalEvent, externalEventParm;
	uint8_t private_140[4];
	int32_t clientNum, weapon, weaponstate, batteryCharge;
	float viewangles[3];
	uint8_t private_172[4];
	int32_t viewheight, damageEvent, damageYaw, damagePitch, damageCount;
	int32_t stats[FNTECH3_JKA_SP_PLAYER_STATS];
	int32_t persistant[FNTECH3_JKA_SP_PLAYER_PERSISTANT];
	int32_t powerups[FNTECH3_JKA_SP_PLAYER_POWERUPS];
	int32_t ammo[FNTECH3_JKA_SP_PLAYER_AMMO];
	int32_t inventory[FNTECH3_JKA_SP_PLAYER_INVENTORY];
	uint8_t private_488[120];
	float serverViewOrg[3];
	int32_t saberInFlight, viewEntity, forcePowersActive;
	uint8_t private_632[8];
	int32_t ping;
	uint8_t private_suffix[FNTECH3_JKA_SP_PLAYER_STATE_BYTES - 644];
} fntech3_jka_sp_playerState_t;
#endif
