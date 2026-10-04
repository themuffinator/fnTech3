/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RAVEN_INTERNAL_STATE_H
#define FNTECH3_RAVEN_INTERNAL_STATE_H

/* Engine-owned extensions for Raven's two Jedi titles, included by q_shared.h
 * beside the other titles'. This is not a module or wire record: the frozen
 * declarations in compat/jka_game_abi.h and compat/jk2_game_abi.h and their
 * separate adapters own external layout, and this holds only what those games
 * carry that the canonical state has no field for.
 *
 * Both titles are carried here, and that is deliberate rather than convenient.
 * Jedi Academy's entity state is ninety-four fields and Jedi Outcast's is
 * seventy-four; twenty-nine of each are Quake III's and stay in the canonical
 * record, and every one of Outcast's remaining twenty-two is a field Academy
 * has too, under the same name. The engine reads none of them -- it carries
 * them between a module and a packet and interprets nothing -- so one carrier
 * serves both without either title's contract touching the other's. The two
 * external layouts stay entirely separate, at 532 bytes and 296, and neither
 * adapter can reach the other's record.
 *
 * A field is kept with the name its module knows it by, so that what it means
 * stays checkable against the layout it was frozen from. Fields only Academy
 * has are marked; Outcast leaves them zero. */

typedef struct {
	int eFlags2;
	/* Two Ghoul2 bolt indices and the entity a bolt attaches to. */
	int bolt1, bolt2, boltToPlayer;
	/* The four entities this one has tricked, for force sight and mind trick. */
	int trickedentindex, trickedentindex2, trickedentindex3, trickedentindex4;
	float speed;
	int fireflag, genericenemyindex, activeForcePass, emplacedOwner;
	/* A looping sound may name a sound set rather than a single sound. */
	int loopIsSoundset, soundSetIndex;
	/* The Ghoul2 model this entity draws with, and the radius it claims. */
	int modelGhoul2, g2radius;
	/* Saber and force state the client needs to draw a duel. */
	int saberInFlight, saberEntityNum, saberMove, forcePowersActive;
	int saberHolstered, isJediMaster, isPortalEnt;
	int owner, teamowner, shouldtarget;
	/* Academy alternates animations rather than toggling a bit. */
	int legsFlip, torsoFlip, forceFrame;
	int heldByClient, ragAttach, iModelScale, brokenLimbs;
	int hasLookTarget, lookTarget;
	int customRGBA[4];
	int maxhealth;
	int npcSaber1, npcSaber2;
	/* Four configstring indices naming an NPC's sound sets. */
	int csSounds_Std, csSounds_Combat, csSounds_Extra, csSounds_Jedi;
	/* Bitfields over a Ghoul2 model's surfaces. */
	int surfacesOn, surfacesOff;
	/* Four bones the game bends itself, and the angles it bends them to. */
	int boneIndex1, boneIndex2, boneIndex3, boneIndex4, boneOrient;
	float boneAngles1[3], boneAngles2[3], boneAngles3[3], boneAngles4[3];
	int NPC_class, m_iVehicleNum;
	/* Fields Raven left for a mod to mean whatever it likes. */
	int userInt1, userInt2, userInt3;
	float userFloat1, userFloat2, userFloat3;
	float userVec1[3], userVec2[3];
	/* The SP titles carry several different presentation fields. These are
	 * engine storage, never an external structure or a packet layout. */
	struct {
		int modelindex3, legsAnimTimer, torsoAnimTimer, scale, saberActive;
		int vehicleModel, vehicleArmor;
		float vehicleAngles[3], modelScale[3];
		int radius, boltInfo;
	} sp;
} ravenEntityStateExtension_t;

/* And what Raven's player state carries that the canonical one has no field
 * for. Jedi Academy's is 168 fields; 43 of them are Quake III's at Quake III's
 * widths and stay in the canonical record, and these 124 are carried here.
 *
 * The force block is carried whole and unread. The engine has no business
 * knowing what a force power is: it moves the bytes between a module and a
 * packet, and the game decides what they mean. A round-trip test proves the
 * block comes back as it went.
 *
 * Every member below was generated from the measured table in
 * compat/jka_wire_layout.def, not written out by hand, so the names and the
 * widths are the ones that were measured. */
typedef struct {
	float moveDir[3];
	int weaponChargeTime;
	int weaponChargeSubtractTime;
	int basespeed;
	int slopeRecalcTime;
	int useTime;
	int legsFlip;
	int torsoFlip;
	int eFlags2;
	int damageType;
	int painTime;
	int painDirection;
	float yawAngle;
	int yawing;
	float pitchAngle;
	int pitching;
	int lastOnGround;
	int saberInFlight;
	int saberMove;
	int saberBlocking;
	int saberBlocked;
	int saberLockTime;
	int saberLockEnemy;
	int saberLockFrame;
	int saberLockHits;
	int saberLockHitCheckTime;
	int saberLockHitIncrementTime;
	int saberLockAdvance;
	int saberEntityNum;
	float saberEntityDist;
	int saberEntityState;
	int saberThrowDelay;
	int saberCanThrow;
	int saberDidThrowTime;
	int saberDamageDebounceTime;
	int saberHitWallSoundDebounceTime;
	int saberEventFlags;
	int rocketLockIndex;
	float rocketLastValidTime;
	float rocketLockTime;
	float rocketTargetTime;
	int emplacedIndex;
	float emplacedTime;
	int isJediMaster;
	int forceRestricted;
	int trueJedi;
	int trueNonJedi;
	int saberIndex;
	int genericEnemyIndex;
	float droneFireTime;
	float droneExistTime;
	int activeForcePass;
	int hasDetPackPlanted;
	float holocronsCarried[18];
	int holocronCantTouch;
	float holocronCantTouchTime;
	int holocronBits;
	int electrifyTime;
	int saberAttackSequence;
	int saberIdleWound;
	int saberAttackWound;
	int saberBlockTime;
	int otherKiller;
	int otherKillerTime;
	int otherKillerDebounceTime;
	unsigned char fd[464];
	int forceJumpFlip;
	int forceHandExtend;
	int forceHandExtendTime;
	int forceRageDrainTime;
	int forceDodgeAnim;
	int quickerGetup;
	int groundTime;
	int footstepTime;
	int otherSoundTime;
	float otherSoundLen;
	int forceGripMoveInterval;
	int forceGripChangeMovetype;
	int forceKickFlip;
	int duelIndex;
	int duelTime;
	int duelInProgress;
	int saberAttackChainCount;
	int saberHolstered;
	int forceAllowDeactivateTime;
	int zoomMode;
	int zoomTime;
	int zoomLocked;
	float zoomFov;
	int zoomLockTime;
	int fallingToDeath;
	int useDelay;
	int inAirAnim;
	float lastHitLoc[3];
	int heldByClient;
	int ragAttach;
	int iModelScale;
	int brokenLimbs;
	int hasLookTarget;
	int lookTarget;
	int customRGBA[4];
	int standheight;
	int crouchheight;
	int m_iVehicleNum;
	float vehOrientation[3];
	int vehBoarding;
	int vehSurfaces;
	int vehTurnaroundIndex;
	int vehTurnaroundTime;
	int vehWeaponsLinked;
	int hyperSpaceTime;
	float hyperSpaceAngles[3];
	int hackingTime;
	int hackingBaseTime;
	int jetpackFuel;
	int cloakFuel;
	int userInt1;
	int userInt2;
	int userInt3;
	float userFloat1;
	float userFloat2;
	float userFloat3;
	float userVec1[3];
	float userVec2[3];
	/* Academy carries nineteen ammunition slots; the canonical record
	 * holds the first sixteen, and these are the rest. */
	int ammoExtra[3];
	/* Jedi Outcast's own. Four it has that Jedi Academy dropped -- a second
	 * sabre blade and the walker a player can drive -- and the force fields
	 * its delta table names one at a time, where Academy's record carries
	 * its force block whole and unnamed. The underscore stands for the dot
	 * in the name its own table gives, so what each one means stays
	 * checkable against that table. */
	int saberActive;
	int usingATST;
	int atstAltFire;
	int dualBlade;
	int fd_forcePowerDebounce_FP_LEVITATION;
	int fd_forcePowersKnown;
	int fd_forcePowersActive;
	int fd_forcePowerSelected;
	int fd_forcePower;
	int fd_forcePowerLevel_FP_LEVITATION;
	int fd_forcePowerLevel_FP_SEE;
	float fd_forceJumpZStart;
	int fd_forceMindtrickTargetIndex;
	int fd_forceMindtrickTargetIndex2;
	int fd_forceMindtrickTargetIndex3;
	int fd_forceMindtrickTargetIndex4;
	int fd_forceRageRecoveryTime;
	int fd_forceSide;
	int fd_sentryDeployed;
	int fd_saberAnimLevel;
	int fd_saberDrawAnimLevel;
	/* Outcast's untransmitted members have no measured names. Carry its whole
	 * module record unchanged here, then overlay named fields at its adapter;
	 * this is storage owned by the engine, never an exposed external layout. */
	unsigned char jk2Player[1368];
} ravenPlayerStateExtension_t;

#endif
