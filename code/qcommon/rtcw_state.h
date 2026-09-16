/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_INTERNAL_STATE_H
#define FNTECH3_RTCW_INTERNAL_STATE_H

/* Engine-owned extensions, included by q_shared.h after trace_t. These are
 * not module or wire records. Common fields remain in the canonical state;
 * arrays here extend its existing event/ammo slots without duplicating them.
 * The frozen RTCW SP/MP declarations and boundary adapters own external layout.
 */
typedef struct {
	int weaponDelay, grenadeTimeLeft;
	float leanf;
	int events[2], eventParms[2], oldEventSequence, item;
	int ammo[48], ammoclip[64], holdable[16], holding, weapons[2];
	float mins[3], maxs[3], crouchMaxZ;
	float crouchViewHeight, standViewHeight, deadViewHeight;
	float runSpeedScale, sprintSpeedScale, crouchSpeedScale;
	int viewlocked, viewlocked_entNum;
	float friction;
	int aiChar, teamNum, gunfx, onFireStart;
	int serverCursorHint, serverCursorHintVal;
	trace_t serverCursorHintTrace;
	int sprintTime, sprintExertTime, classWeaponTime;
	int weapAnimTimer, weapAnim, releasedFire;
	float aimSpreadScaleFloat;
	int aimSpreadScale, lastFireTime, quickGrenTime, leanStopDebounceTime;
	int weapHeat[64], curWeapHeat, venomTime;
	int accShowBits, accHideBits, aiState;
	float footstepCount;
	int identifyClient;
} rtcwPlayerStateExtension_t;

typedef struct {
	unsigned char wbuttons, holdable;
	signed char wolfkick;
	unsigned char mpSetup, identClient;
	unsigned short cld;
} rtcwUsercmdExtension_t;

typedef struct {
	int dl_intensity, eventSequence, events[4], eventParms[4];
	int density, dmgFlags, onFireStart, onFireEnd, aiChar, teamNum;
	int effect1Time, effect2Time, effect3Time, aiState, animMovetype;
} rtcwEntityStateExtension_t;

enum {
	FNTECH3_RTCW_LIMBO_LINES = 7,
	FNTECH3_RTCW_LIMBO_WIDTH = 140,
	FNTECH3_RTCW_LIMBO_STORAGE = 421
};
typedef struct {
	char lines[FNTECH3_RTCW_LIMBO_LINES][FNTECH3_RTCW_LIMBO_STORAGE];
} rtcwLimboChat_t;

#endif
