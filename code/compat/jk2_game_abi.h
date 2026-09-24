/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JK2_GAME_ABI_H
#define FNTECH3_COMPAT_JK2_GAME_ABI_H

/* Frozen Jedi Outcast multiplayer records: the Win32 x86 layouts its retail
 * module and its protocol 15 use.
 *
 * Raven released no multiplayer source for this title, so every number here
 * was measured rather than read. Each says how:
 *
 *   entityState_t   296 bytes   its delta table's last member is `generic1`
 *                               at 292, and the engine's own SV_SetBrushModel
 *                               writes the entity's shared half from 296
 *   entityShared_t  100 bytes   SV_SetBrushModel writes a brush flag at +16,
 *                               two bounds vectors at +20 and +32 and a
 *                               contents mask at +44, which is Quake III's
 *                               spacing; the other nine members follow it
 *   playerState_t  1368 bytes   its delta table's last member sits at 1364,
 *                               and the engine copies 1,368 bytes whole
 *   gentity_t       828 bytes   the module states it when it publishes its
 *   gclient_t      1756 bytes   arrays, and the refusal of that call reports it
 *   MAX_GENTITIES  1024         SV_SvEntityForGentity's own bound
 *
 * Ten of the shared half's thirteen members are now directly evidenced, from
 * the retail engine's own link and unlink routines at 0x4502d0 and 0x450270:
 *
 *   linked          `mov [esi+0x128], 1` when it links, and 0 when it unlinks
 *   linkcount       `inc dword [esi+0x12c]`
 *   bmodel          tested at `[esi+0x138]` before the solid size is encoded
 *   mins, maxs      read at 0x13c-0x144 and 0x148-0x150 for that encoding
 *   contents        tested at `[esi+0x154]` against the solid and body bits
 *   absmin, absmax  written at 0x158 and 0x164, each the sum of
 *   currentOrigin   `[esi+0x170..0x178]` and the bounds above
 *   currentAngles   read at 0x17c-0x184 to decide whether the box is turned
 *
 * That leaves svFlags, singleClient and ownerNum. Their offsets are not in
 * doubt -- 8 and 12 are the only slots between the evidenced linkcount at 4
 * and the evidenced bmodel at 16, and 96 is the only slot before the measured
 * hundred-byte end -- but which of the three sits where is read from Quake
 * III's order and is the one thing here not separately evidenced.
 *
 * These are boundary records, not engine types. See
 * docs/fntech3/2026-09-22-jk2-contracts.md. */

#include <stdint.h>

#define FNTECH3_JK2_MAX_CLIENTS            32
#define FNTECH3_JK2_MAX_GENTITIES        1024
#define FNTECH3_JK2_MAX_CONFIGSTRINGS    1400
#define FNTECH3_JK2_ENTITY_STATE_BYTES    296
#define FNTECH3_JK2_ENTITY_SHARED_BYTES   100
#define FNTECH3_JK2_ENTITY_SHARED_OFFSET  296
#define FNTECH3_JK2_SHARED_ENTITY_BYTES   396
#define FNTECH3_JK2_PLAYER_STATE_BYTES   1368

/* Everything the server keeps about a player.
 *
 * 1,368 bytes, of which this names 680. Its delta table gives 105 members with
 * their offsets and widths, and the four arrays were measured separately out
 * of the engine's own delta reader, which prints their names as it walks them:
 * stats at 216, persistant at 280, powerups at 344 and ammunition at 408,
 * sixteen each. Powerups are read as long words there where the other three
 * are read as shorts, which is what a powerup is -- a time, not a count.
 *
 * The other 688 bytes have no name in any table this engine has read. They are
 * declared as unnamed spans rather than guessed at, so the record is exact at
 * every offset that is known and silent about every offset that is not. */
typedef struct {
	int32_t commandTime;                       /*    0 */
	int32_t pm_type;                           /*    4 */
	int32_t bobCycle;                          /*    8 */
	int32_t pm_flags;                          /*   12 */
	int32_t pm_time;                           /*   16 */
	float origin[3];                         /*   20 */
	float velocity[3];                       /*   32 */
	int32_t weaponTime;                        /*   44 */
	int32_t weaponChargeTime;                  /*   48 */
	int32_t weaponChargeSubtractTime;          /*   52 */
	int32_t gravity;                           /*   56 */
	int32_t speed;                             /*   60 */
	int32_t basespeed;                         /*   64 */
	int32_t delta_angles[3];                   /*   68 */
	unsigned char unnamed_80[4];
	int32_t groundEntityNum;                   /*   84 */
	int32_t legsTimer;                         /*   88 */
	int32_t legsAnim;                          /*   92 */
	int32_t torsoTimer;                        /*   96 */
	int32_t torsoAnim;                         /*  100 */
	int32_t movementDir;                       /*  104 */
	int32_t eFlags;                            /*  108 */
	int32_t eventSequence;                     /*  112 */
	int32_t events[2];                         /*  116 */
	int32_t eventParms[2];                     /*  124 */
	int32_t externalEvent;                     /*  132 */
	int32_t externalEventParm;                 /*  136 */
	unsigned char unnamed_140[4];
	int32_t clientNum;                         /*  144 */
	int32_t weapon;                            /*  148 */
	int32_t weaponstate;                       /*  152 */
	float viewangles[3];                     /*  156 */
	int32_t viewheight;                        /*  168 */
	int32_t damageEvent;                       /*  172 */
	int32_t damageYaw;                         /*  176 */
	int32_t damagePitch;                       /*  180 */
	int32_t damageCount;                       /*  184 */
	int32_t damageType;                        /*  188 */
	unsigned char unnamed_192[24];
	int32_t stats[16];                         /*  216 */
	int32_t persistant[16];                    /*  280 */
	int32_t powerups[16];                      /*  344 */
	int32_t ammo[16];                          /*  408 */
	int32_t generic1;                          /*  472 */
	int32_t loopSound;                         /*  476 */
	int32_t jumppad_ent;                       /*  480 */
	int32_t ping;                              /*  484 */
	unsigned char unnamed_488[16];
	int32_t saberInFlight;                     /*  504 */
	int32_t saberActive;                       /*  508 */
	int32_t saberMove;                         /*  512 */
	unsigned char unnamed_516[4];
	int32_t saberBlocked;                      /*  520 */
	int32_t saberLockTime;                     /*  524 */
	int32_t saberLockEnemy;                    /*  528 */
	int32_t saberLockFrame;                    /*  532 */
	unsigned char unnamed_536[4];
	int32_t saberLockAdvance;                  /*  540 */
	int32_t saberEntityNum;                    /*  544 */
	unsigned char unnamed_548[12];
	int32_t saberCanThrow;                     /*  560 */
	unsigned char unnamed_564[16];
	int32_t rocketLockIndex;                   /*  580 */
	unsigned char unnamed_584[4];
	int32_t rocketLockTime;                    /*  588 */
	int32_t rocketTargetTime;                  /*  592 */
	int32_t emplacedIndex;                     /*  596 */
	unsigned char unnamed_600[4];
	int32_t isJediMaster;                      /*  604 */
	unsigned char unnamed_608[4];
	int32_t genericEnemyIndex;                 /*  612 */
	unsigned char unnamed_616[8];
	int32_t activeForcePass;                   /*  624 */
	int32_t hasDetPackPlanted;                 /*  628 */
	unsigned char unnamed_632[80];
	int32_t holocronBits;                      /*  712 */
	unsigned char unnamed_716[12];
	int32_t electrifyTime;                     /*  728 */
	unsigned char unnamed_732[32];
	int32_t fd_forcePowerDebounce_FP_LEVITATION; /*  764 */
	unsigned char unnamed_768[64];
	int32_t fd_forcePowersKnown;               /*  832 */
	int32_t fd_forcePowersActive;              /*  836 */
	int32_t fd_forcePowerSelected;             /*  840 */
	unsigned char unnamed_844[76];
	int32_t fd_forcePower;                     /*  920 */
	unsigned char unnamed_924[12];
	int32_t fd_forcePowerLevel_FP_LEVITATION;  /*  936 */
	unsigned char unnamed_940[48];
	int32_t fd_forcePowerLevel_FP_SEE;         /*  988 */
	unsigned char unnamed_992[88];
	float fd_forceJumpZStart;                /* 1080 */
	unsigned char unnamed_1084[60];
	int32_t fd_forceMindtrickTargetIndex;      /* 1144 */
	int32_t fd_forceMindtrickTargetIndex2;     /* 1148 */
	int32_t fd_forceMindtrickTargetIndex3;     /* 1152 */
	int32_t fd_forceMindtrickTargetIndex4;     /* 1156 */
	int32_t fd_forceRageRecoveryTime;          /* 1160 */
	unsigned char unnamed_1164[12];
	int32_t fd_forceSide;                      /* 1176 */
	unsigned char unnamed_1180[32];
	int32_t fd_sentryDeployed;                 /* 1212 */
	int32_t fd_saberAnimLevel;                 /* 1216 */
	int32_t fd_saberDrawAnimLevel;             /* 1220 */
	unsigned char unnamed_1224[12];
	int32_t forceHandExtend;                   /* 1236 */
	unsigned char unnamed_1240[8];
	int32_t forceDodgeAnim;                    /* 1248 */
	unsigned char unnamed_1252[32];
	int32_t duelIndex;                         /* 1284 */
	int32_t duelTime;                          /* 1288 */
	int32_t duelInProgress;                    /* 1292 */
	int32_t saberAttackChainCount;             /* 1296 */
	int32_t saberHolstered;                    /* 1300 */
	int32_t usingATST;                         /* 1304 */
	int32_t atstAltFire;                       /* 1308 */
	unsigned char unnamed_1312[8];
	int32_t zoomMode;                          /* 1320 */
	int32_t zoomTime;                          /* 1324 */
	int32_t zoomLocked;                        /* 1328 */
	int32_t zoomFov;                           /* 1332 */
	unsigned char unnamed_1336[4];
	int32_t fallingToDeath;                    /* 1340 */
	unsigned char unnamed_1344[4];
	int32_t inAirAnim;                         /* 1348 */
	int32_t dualBlade;                         /* 1352 */
	float lastHitLoc[3];                     /* 1356 */
} fntech3_jk2_playerState_t;

/* One client's input frame.
 *
 * Twenty-eight bytes, which the engine copies whole: its own usercmd reader is
 * a `rep movsd` of seven dwords out of the client record, and every other
 * place the engine touches one does the same. So the engine never reads a
 * field, and nothing about the inside of these bytes follows from it. The
 * inside was read instead out of the client that builds one, at 0x4076c0:
 *
 *   0     `mov [ecx], eax` from the client's own time          serverTime
 *   16    built by OR-ing bits, `or [edx+0x10], 0x100` and a
 *         shifted mask a few instructions earlier              buttons
 *   20    `mov [ecx+0x14], al` from its own global             weapon
 *   21    `mov [ecx+0x15], al` from its own global             forcesel
 *   22    `mov [ecx+0x16], al` from its own global             invensel
 *   23    `mov [ecx+0x17], al`, and that global is cleared
 *         immediately after, which is a command that fires
 *         once                                                 generic_cmd
 *   24-26 three more bytes, written together elsewhere         the three moves
 *
 * That leaves 4, 8 and 12 as the only slots before the first byte field, and
 * they hold the view angles. Twenty-seven bytes of members, padded to 28.
 *
 * It is the same shape as Jedi Academy's, which is worth saying plainly
 * because it was not assumed from the shared size: Academy's was frozen from
 * its own measurement, and this was read out of Outcast's own client before
 * the two were ever compared. Quake III's is 24 bytes and is neither. */
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
} fntech3_jk2_usercmd_t;

/* A moving point, as Outcast records one. Its six members sit at the
 * offsets its own delta table gives them. */
typedef struct {
	int32_t trType;
	int32_t trTime;
	int32_t trDuration;
	float trBase[3];
	float trDelta[3];
} fntech3_jk2_trajectory_t;

/* Everything the engine tells a client about an entity.
 *
 * Read out of retail jk2mp.exe's own entity delta table, which begins at
 * file offset 0xf5ab0 and names 73 members: every four-byte slot from 4 to
 * 292 with no gap, and the width in bits each one is sent at. A member sent
 * at zero bits is a float, which is how this lineage has always written
 * one. `number` is at 0 and is not in the table, because no engine in this
 * line delta-codes an entity number: it is the index the entity is sent
 * under. The record's own 296 bytes were measured separately, from the
 * engine's brush-model writer, and the table ends exactly there. */
typedef struct {
	int32_t number;
	int32_t eType;                             /*   4, 8 bits */
	int32_t eFlags;                            /*   8, 32 bits */
	fntech3_jk2_trajectory_t pos;
	fntech3_jk2_trajectory_t apos;
	int32_t time;                              /*  84, 32 bits */
	int32_t time2;                             /*  88, 32 bits */
	float origin[3];                           /*  92, float */
	float origin2[3];                          /* 104, float */
	float angles[3];                           /* 116, float */
	float angles2[3];                          /* 128, float */
	int32_t bolt1;                             /* 140, 8 bits */
	int32_t bolt2;                             /* 144, 10 bits */
	int32_t trickedentindex;                   /* 148, 16 bits */
	int32_t trickedentindex2;                  /* 152, 16 bits */
	int32_t trickedentindex3;                  /* 156, 16 bits */
	int32_t trickedentindex4;                  /* 160, 16 bits */
	float speed;                               /* 164, float */
	int32_t fireflag;                          /* 168, 2 bits */
	int32_t genericenemyindex;                 /* 172, 32 bits */
	int32_t activeForcePass;                   /* 176, 6 bits */
	int32_t emplacedOwner;                     /* 180, 32 bits */
	int32_t otherEntityNum;                    /* 184, 10 bits */
	int32_t otherEntityNum2;                   /* 188, 10 bits */
	int32_t groundEntityNum;                   /* 192, 10 bits */
	int32_t constantLight;                     /* 196, 32 bits */
	int32_t loopSound;                         /* 200, 8 bits */
	int32_t modelGhoul2;                       /* 204, 4 bits */
	int32_t g2radius;                          /* 208, 8 bits */
	int32_t modelindex;                        /* 212, 8 bits */
	int32_t modelindex2;                       /* 216, 8 bits */
	int32_t clientNum;                         /* 220, 8 bits */
	int32_t frame;                             /* 224, 16 bits */
	int32_t saberInFlight;                     /* 228, 1 bit  */
	int32_t saberEntityNum;                    /* 232, 10 bits */
	int32_t saberMove;                         /* 236, 8 bits */
	int32_t forcePowersActive;                 /* 240, 32 bits */
	int32_t isJediMaster;                      /* 244, 1 bit  */
	int32_t solid;                             /* 248, 24 bits */
	int32_t event;                             /* 252, 10 bits */
	int32_t eventParm;                         /* 256, 8 bits */
	int32_t owner;                             /* 260, 10 bits */
	int32_t teamowner;                         /* 264, 8 bits */
	int32_t shouldtarget;                      /* 268, 1 bit  */
	int32_t powerups;                          /* 272, 16 bits */
	int32_t weapon;                            /* 276, 8 bits */
	int32_t legsAnim;                          /* 280, 16 bits */
	int32_t torsoAnim;                         /* 284, 16 bits */
	int32_t forceFrame;                        /* 288, 16 bits */
	int32_t generic1;                          /* 292, 8 bits */
} fntech3_jk2_entityState_t;

/* Where each member of the engine's half sits, from the start of an entity. */
#define FNTECH3_JK2_R( member )  ( FNTECH3_JK2_ENTITY_SHARED_OFFSET + ( member ) )
#define FNTECH3_JK2_R_LINKED           FNTECH3_JK2_R(  0 )   /* evidenced */
#define FNTECH3_JK2_R_LINKCOUNT        FNTECH3_JK2_R(  4 )   /* evidenced */
#define FNTECH3_JK2_R_SVFLAGS          FNTECH3_JK2_R(  8 )   /* placed */
#define FNTECH3_JK2_R_SINGLECLIENT     FNTECH3_JK2_R( 12 )   /* placed */
#define FNTECH3_JK2_R_BMODEL           FNTECH3_JK2_R( 16 )   /* evidenced */
#define FNTECH3_JK2_R_MINS             FNTECH3_JK2_R( 20 )   /* evidenced */
#define FNTECH3_JK2_R_MAXS             FNTECH3_JK2_R( 32 )   /* evidenced */
#define FNTECH3_JK2_R_CONTENTS         FNTECH3_JK2_R( 44 )   /* evidenced */
#define FNTECH3_JK2_R_ABSMIN           FNTECH3_JK2_R( 48 )   /* evidenced */
#define FNTECH3_JK2_R_ABSMAX           FNTECH3_JK2_R( 60 )   /* evidenced */
#define FNTECH3_JK2_R_CURRENTORIGIN    FNTECH3_JK2_R( 72 )   /* evidenced */
#define FNTECH3_JK2_R_CURRENTANGLES    FNTECH3_JK2_R( 84 )   /* evidenced */
#define FNTECH3_JK2_R_OWNERNUM         FNTECH3_JK2_R( 96 )   /* placed */

/* Members of the state at the front that the engine itself reads, each at the
 * offset its own delta table gives. */
#define FNTECH3_JK2_S_NUMBER              0
#define FNTECH3_JK2_S_ETYPE               4
#define FNTECH3_JK2_S_EFLAGS              8
#define FNTECH3_JK2_S_ORIGIN             92
#define FNTECH3_JK2_S_ORIGIN2           104
#define FNTECH3_JK2_S_SOLID             248
#define FNTECH3_JK2_S_CLIENTNUM         220
#define FNTECH3_JK2_S_GENERIC1          292

/* And of the player state. `ping` is not in the delta table, because no
 * engine in this lineage delta-codes it; it was read instead out of the place
 * the engine writes it. Retail jk2mp.exe's ping pass clamps to 999 at
 * 0x44efe9, calls its own client-state accessor at 0x44a720 -- which is a base
 * plus index times stride, and nothing else -- and stores the result through
 * `mov [eax + 0x1e4], edx`. */
#define FNTECH3_JK2_PS_CLIENTNUM        144
#define FNTECH3_JK2_PS_WEAPON           148
#define FNTECH3_JK2_PS_PING             484

#endif
