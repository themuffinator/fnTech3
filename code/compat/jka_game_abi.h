/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_GAME_ABI_H
#define FNTECH3_COMPAT_JKA_GAME_ABI_H

/* Frozen Jedi Academy multiplayer records: the exact Win32 x86 layouts its
 * retail modules and its protocol 26 use. They are boundary PODs, not engine
 * types, and nothing outside this title's adapters may take their shapes.
 *
 * Every offset and size here is pinned in code/compat/jka_wire_layout.def and
 * asserted in tests/jka_abi_layout_tests.cpp. Member names and order are
 * interoperability facts read from Raven's released sources as carried by
 * OpenJK (codemp/qcommon/q_shared.h, codemp/game/g_public.h); no
 * implementation text is incorporated, and no engine behaviour lives here.
 * See docs/fntech3/2026-09-21-jka-records.md. */

#include <stdint.h>

/* Raven's own limits, which these records and the protocol both depend on. */
#define FNTECH3_JKA_MAX_CLIENTS              32
#define FNTECH3_JKA_MAX_GENTITIES            1024
#define FNTECH3_JKA_GENTITYNUM_BITS          10
#define FNTECH3_JKA_MAX_CONFIGSTRINGS        1700
#define FNTECH3_JKA_MAX_GAMESTATE_CHARS      16000
#define FNTECH3_JKA_MAX_STATS                16
#define FNTECH3_JKA_MAX_PERSISTANT           16
#define FNTECH3_JKA_MAX_POWERUPS             16
#define FNTECH3_JKA_MAX_WEAPONS              19
#define FNTECH3_JKA_MAX_PS_EVENTS            2
#define FNTECH3_JKA_NUM_FORCE_POWERS         18
#define FNTECH3_JKA_TRACK_CHANNEL_MAX        6
#define FNTECH3_JKA_MAX_CVAR_VALUE_STRING    256
/* The script task slots and behaviour sets an entity carries. */
#define FNTECH3_JKA_NUM_TASK_IDS             10
#define FNTECH3_JKA_NUM_BEHAVIOR_SETS        17

/* A cvar as a module sees it: its handle, its value and when it changed. */
typedef struct {
	int32_t handle;
	int32_t modificationCount;
	float value;
	int32_t integer;
	char string[256];
} fntech3_jka_vmCvar_t;

/* A plane, as Raven's collision reports one. Quake III's, unchanged. */
typedef struct {
	float normal[3];
	float dist;
	uint8_t type;
	uint8_t signbits;
	uint8_t pad[2];
} fntech3_jka_cplane_t;

/* What a trace tells the game.
 *
 * This is NOT Quake III's trace, and the difference is not a detail: Raven
 * packed the two solid flags and the entity number into the first four bytes,
 * so the record is 48 bytes and every field from the fraction onwards sits
 * four bytes earlier than Quake III puts it. Handing this game a Quake III
 * trace would put the fraction where it reads two flags, and the endpoint
 * where it reads the fraction.
 *
 * Measured from the retail module, not assumed: of the 233 calls it makes to
 * trap_Trace, 203 could be followed far enough to see what the caller then
 * reads out of the record. Offsets 0 and 1 are read as single bytes (58 and
 * 61 times), offset 2 only ever as a sixteen-bit word (54 times), offset 4
 * only ever as a four-byte float (96 times), and 36 and 37 only as bytes,
 * which is the plane's type and signbits. Nothing above 47 is read with any
 * regularity. See docs/fntech3/2026-09-22-jka-trace.md. */
typedef struct {
	uint8_t allsolid;
	uint8_t startsolid;
	int16_t entityNum;
	float fraction;
	float endpos[3];
	fntech3_jka_cplane_t plane;
	int32_t surfaceFlags;
	int32_t contents;
} fntech3_jka_trace_t;

/* A moving point: how it moves, when it started and for how long. */
typedef struct {
	int32_t trType;
	int32_t trTime;
	int32_t trDuration;
	float trBase[3];
	float trDelta[3];
} fntech3_jka_trajectory_t;

/* One client's input frame. */
typedef struct {
	int32_t serverTime;
	int32_t angles[3];
	int32_t buttons;
	uint8_t weapon;
	uint8_t forcesel;
	uint8_t invensel;
	uint8_t generic_cmd;
	int8_t forwardmove;
	int8_t rightmove;
	int8_t upmove;
} fntech3_jka_usercmd_t;

/* A player's Force powers, their levels and their timers. */
typedef struct {
	int32_t forcePowerDebounce[FNTECH3_JKA_NUM_FORCE_POWERS];
	int32_t forcePowersKnown;
	int32_t forcePowersActive;
	int32_t forcePowerSelected;
	int32_t forceButtonNeedRelease;
	int32_t forcePowerDuration[FNTECH3_JKA_NUM_FORCE_POWERS];
	int32_t forcePower;
	int32_t forcePowerMax;
	int32_t forcePowerRegenDebounceTime;
	int32_t forcePowerLevel[FNTECH3_JKA_NUM_FORCE_POWERS];
	int32_t forcePowerBaseLevel[FNTECH3_JKA_NUM_FORCE_POWERS];
	int32_t forceUsingAdded;
	float forceJumpZStart;
	float forceJumpCharge;
	int32_t forceJumpSound;
	int32_t forceJumpAddTime;
	int32_t forceGripEntityNum;
	int32_t forceGripDamageDebounceTime;
	float forceGripBeingGripped;
	int32_t forceGripCripple;
	int32_t forceGripUseTime;
	float forceGripSoundTime;
	float forceGripStarted;
	int32_t forceHealTime;
	int32_t forceHealAmount;
	int32_t forceMindtrickTargetIndex;
	int32_t forceMindtrickTargetIndex2;
	int32_t forceMindtrickTargetIndex3;
	int32_t forceMindtrickTargetIndex4;
	int32_t forceRageRecoveryTime;
	int32_t forceDrainEntNum;
	float forceDrainTime;
	int32_t forceDoInit;
	int32_t forceSide;
	int32_t forceRank;
	int32_t forceDeactivateAll;
	int32_t killSoundEntIndex[FNTECH3_JKA_TRACK_CHANNEL_MAX];
	int32_t sentryDeployed;
	int32_t saberAnimLevelBase;
	int32_t saberAnimLevel;
	int32_t saberDrawAnimLevel;
	int32_t suicides;
	int32_t privateDuelTime;
} fntech3_jka_forcedata_t;

/* What a client is told about one entity. */
typedef struct {
	int32_t number;
	int32_t eType;
	int32_t eFlags;
	int32_t eFlags2;
	fntech3_jka_trajectory_t pos;
	fntech3_jka_trajectory_t apos;
	int32_t time;
	int32_t time2;
	float origin[3];
	float origin2[3];
	float angles[3];
	float angles2[3];
	int32_t bolt1;
	int32_t bolt2;
	int32_t trickedentindex;
	int32_t trickedentindex2;
	int32_t trickedentindex3;
	int32_t trickedentindex4;
	float speed;
	int32_t fireflag;
	int32_t genericenemyindex;
	int32_t activeForcePass;
	int32_t emplacedOwner;
	int32_t otherEntityNum;
	int32_t otherEntityNum2;
	int32_t groundEntityNum;
	int32_t constantLight;
	int32_t loopSound;
	int32_t loopIsSoundset;
	int32_t soundSetIndex;
	int32_t modelGhoul2;
	int32_t g2radius;
	int32_t modelindex;
	int32_t modelindex2;
	int32_t clientNum;
	int32_t frame;
	int32_t saberInFlight;
	int32_t saberEntityNum;
	int32_t saberMove;
	int32_t forcePowersActive;
	int32_t saberHolstered;
	int32_t isJediMaster;
	int32_t isPortalEnt;
	int32_t solid;
	int32_t event;
	int32_t eventParm;
	int32_t owner;
	int32_t teamowner;
	int32_t shouldtarget;
	int32_t powerups;
	int32_t weapon;
	int32_t legsAnim;
	int32_t torsoAnim;
	int32_t legsFlip;
	int32_t torsoFlip;
	int32_t forceFrame;
	int32_t generic1;
	int32_t heldByClient;
	int32_t ragAttach;
	int32_t iModelScale;
	int32_t brokenLimbs;
	int32_t boltToPlayer;
	int32_t hasLookTarget;
	int32_t lookTarget;
	int32_t customRGBA[4];
	int32_t health;
	int32_t maxhealth;
	int32_t npcSaber1;
	int32_t npcSaber2;
	int32_t csSounds_Std;
	int32_t csSounds_Combat;
	int32_t csSounds_Extra;
	int32_t csSounds_Jedi;
	int32_t surfacesOn;
	int32_t surfacesOff;
	int32_t boneIndex1;
	int32_t boneIndex2;
	int32_t boneIndex3;
	int32_t boneIndex4;
	int32_t boneOrient;
	float boneAngles1[3];
	float boneAngles2[3];
	float boneAngles3[3];
	float boneAngles4[3];
	int32_t NPC_class;
	int32_t m_iVehicleNum;
	int32_t userInt1;
	int32_t userInt2;
	int32_t userInt3;
	float userFloat1;
	float userFloat2;
	float userFloat3;
	float userVec1[3];
	float userVec2[3];
} fntech3_jka_entityState_t;

/* Everything the server keeps about a player, whole. */
typedef struct {
	int32_t commandTime;
	int32_t pm_type;
	int32_t bobCycle;
	int32_t pm_flags;
	int32_t pm_time;
	float origin[3];
	float velocity[3];
	float moveDir[3];
	int32_t weaponTime;
	int32_t weaponChargeTime;
	int32_t weaponChargeSubtractTime;
	int32_t gravity;
	float speed;
	int32_t basespeed;
	int32_t delta_angles[3];
	int32_t slopeRecalcTime;
	int32_t useTime;
	int32_t groundEntityNum;
	int32_t legsTimer;
	int32_t legsAnim;
	int32_t torsoTimer;
	int32_t torsoAnim;
	int32_t legsFlip;
	int32_t torsoFlip;
	int32_t movementDir;
	int32_t eFlags;
	int32_t eFlags2;
	int32_t eventSequence;
	int32_t events[2];
	int32_t eventParms[2];
	int32_t externalEvent;
	int32_t externalEventParm;
	int32_t externalEventTime;
	int32_t clientNum;
	int32_t weapon;
	int32_t weaponstate;
	float viewangles[3];
	int32_t viewheight;
	int32_t damageEvent;
	int32_t damageYaw;
	int32_t damagePitch;
	int32_t damageCount;
	int32_t damageType;
	int32_t painTime;
	int32_t painDirection;
	float yawAngle;
	int32_t yawing;
	float pitchAngle;
	int32_t pitching;
	int32_t stats[16];
	int32_t persistant[16];
	int32_t powerups[16];
	int32_t ammo[19];
	int32_t generic1;
	int32_t loopSound;
	int32_t jumppad_ent;
	int32_t ping;
	int32_t pmove_framecount;
	int32_t jumppad_frame;
	int32_t entityEventSequence;
	int32_t lastOnGround;
	int32_t saberInFlight;
	int32_t saberMove;
	int32_t saberBlocking;
	int32_t saberBlocked;
	int32_t saberLockTime;
	int32_t saberLockEnemy;
	int32_t saberLockFrame;
	int32_t saberLockHits;
	int32_t saberLockHitCheckTime;
	int32_t saberLockHitIncrementTime;
	int32_t saberLockAdvance;
	int32_t saberEntityNum;
	float saberEntityDist;
	int32_t saberEntityState;
	int32_t saberThrowDelay;
	int32_t saberCanThrow;
	int32_t saberDidThrowTime;
	int32_t saberDamageDebounceTime;
	int32_t saberHitWallSoundDebounceTime;
	int32_t saberEventFlags;
	int32_t rocketLockIndex;
	float rocketLastValidTime;
	float rocketLockTime;
	float rocketTargetTime;
	int32_t emplacedIndex;
	float emplacedTime;
	int32_t isJediMaster;
	int32_t forceRestricted;
	int32_t trueJedi;
	int32_t trueNonJedi;
	int32_t saberIndex;
	int32_t genericEnemyIndex;
	float droneFireTime;
	float droneExistTime;
	int32_t activeForcePass;
	int32_t hasDetPackPlanted;
	float holocronsCarried[FNTECH3_JKA_NUM_FORCE_POWERS];
	int32_t holocronCantTouch;
	float holocronCantTouchTime;
	int32_t holocronBits;
	int32_t electrifyTime;
	int32_t saberAttackSequence;
	int32_t saberIdleWound;
	int32_t saberAttackWound;
	int32_t saberBlockTime;
	int32_t otherKiller;
	int32_t otherKillerTime;
	int32_t otherKillerDebounceTime;
	fntech3_jka_forcedata_t fd;
	int32_t forceJumpFlip;
	int32_t forceHandExtend;
	int32_t forceHandExtendTime;
	int32_t forceRageDrainTime;
	int32_t forceDodgeAnim;
	int32_t quickerGetup;
	int32_t groundTime;
	int32_t footstepTime;
	int32_t otherSoundTime;
	float otherSoundLen;
	int32_t forceGripMoveInterval;
	int32_t forceGripChangeMovetype;
	int32_t forceKickFlip;
	int32_t duelIndex;
	int32_t duelTime;
	int32_t duelInProgress;
	int32_t saberAttackChainCount;
	int32_t saberHolstered;
	int32_t forceAllowDeactivateTime;
	int32_t zoomMode;
	int32_t zoomTime;
	int32_t zoomLocked;
	float zoomFov;
	int32_t zoomLockTime;
	int32_t fallingToDeath;
	int32_t useDelay;
	int32_t inAirAnim;
	float lastHitLoc[3];
	int32_t heldByClient;
	int32_t ragAttach;
	int32_t iModelScale;
	int32_t brokenLimbs;
	int32_t hasLookTarget;
	int32_t lookTarget;
	int32_t customRGBA[4];
	int32_t standheight;
	int32_t crouchheight;
	int32_t m_iVehicleNum;
	float vehOrientation[3];
	int32_t vehBoarding;
	int32_t vehSurfaces;
	int32_t vehTurnaroundIndex;
	int32_t vehTurnaroundTime;
	int32_t vehWeaponsLinked;
	int32_t hyperSpaceTime;
	float hyperSpaceAngles[3];
	int32_t hackingTime;
	int32_t hackingBaseTime;
	int32_t jetpackFuel;
	int32_t cloakFuel;
	int32_t userInt1;
	int32_t userInt2;
	int32_t userInt3;
	float userFloat1;
	float userFloat2;
	float userFloat3;
	float userVec1[3];
	float userVec2[3];
} fntech3_jka_playerState_t;

/* The part of an entity the engine itself owns. */
typedef struct {
	int32_t linked;
	int32_t linkcount;
	int32_t svFlags;
	int32_t singleClient;
	int32_t bmodel;
	float mins[3];
	float maxs[3];
	int32_t contents;
	float absmin[3];
	float absmax[3];
	float currentOrigin[3];
	float currentAngles[3];
	int32_t mIsRoffing;
	int32_t ownerNum;
	uint32_t broadcastClients[2];
} fntech3_jka_entityShared_t;

/* The entity record the game and the engine share. */
typedef struct {
	fntech3_jka_entityState_t s;
	fntech3_jka_playerState_t *playerState;
	void *m_pVehicle;
	void *ghoul2;
	int32_t localAnimIndex;
	float modelScale[3];
	fntech3_jka_entityShared_t r;
	int32_t taskID[FNTECH3_JKA_NUM_TASK_IDS];
	void *parms;
	char *behaviorSet[FNTECH3_JKA_NUM_BEHAVIOR_SETS];
	char *script_targetname;
	int32_t delayScriptTime;
	char *fullName;
	char *targetname;
	char *classname;
	int32_t waypoint;
	int32_t lastWaypoint;
	int32_t lastValidWaypoint;
	int32_t noWaypointTime;
	int32_t combatPoint;
	int32_t failedWaypoints[8];
	int32_t failedWaypointCheckTime;
	int32_t next_roff_time;
} fntech3_jka_sharedEntity_t;

#endif
