/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_GAME_PROFILE_H
#define FNTECH3_GAME_PROFILE_H

#include "q3test_release.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum comGameId_e
#ifdef __cplusplus
	: unsigned int
#endif
{
	COM_GAME_AUTO = 0,
	COM_GAME_QUAKE3,
	COM_GAME_QUAKELIVE,
	COM_GAME_RTCW_SP,
	COM_GAME_RTCW_MP,
	COM_GAME_WOLFET,
	COM_GAME_STVEF_SP,
	COM_GAME_STVEF_MP,
	COM_GAME_QUAKE3_IHV,
	COM_GAME_JKA_SP,
	COM_GAME_JKA_MP,
	COM_GAME_JK2_SP,
	COM_GAME_JK2_MP,
	COM_GAME_ALICE,
	COM_GAME_FAKK2,
	COM_GAME_MOHAA,
	/* The public Q3Test builds 1.02 to 1.08 (1999). One profile under the
	 * Quake III title; it pins one release's contracts at startup
	 * (q3test_release.h). */
	COM_GAME_Q3TEST,
	COM_GAME_COUNT
} comGameId_t;

typedef enum comGameTitleId_e
#ifdef __cplusplus
	: unsigned int
#endif
{
	COM_GAME_TITLE_QUAKE3 = 1,
	COM_GAME_TITLE_QUAKELIVE,
	COM_GAME_TITLE_RTCW,
	COM_GAME_TITLE_WOLFET,
	COM_GAME_TITLE_STVEF,
	COM_GAME_TITLE_JKA,
	COM_GAME_TITLE_JK2,
	COM_GAME_TITLE_ALICE,
	COM_GAME_TITLE_FAKK2,
	COM_GAME_TITLE_MOHAA
} comGameTitleId_t;

/* BSP version 47 is ambiguous without a title: RTCW has 17 lumps, while
 * Quake Live appends an advertisement descriptor. These are engine format
 * identities, not on-disk version numbers or retail module ABI values. */
typedef enum comGameBspFormat_e
#ifdef __cplusplus
	: unsigned int
#endif
{
	COM_GAME_BSP_UNSUPPORTED = 0,
	COM_GAME_BSP_QUAKE3_IHV,
	COM_GAME_BSP_QUAKE3,
	COM_GAME_BSP_QUAKELIVE,
	COM_GAME_BSP_RTCW,
	COM_GAME_BSP_WOLFET,
	/* Raven's own ident: "RBSP" version 1, with the lightmap-style arrays and
	 * the light-array lump Quake III's IBSP 46 does not have. Jedi Outcast and
	 * Jedi Academy both use it, and every map either ships reads through the
	 * one translator: 41 of Outcast's and 57 of Academy's. */
	COM_GAME_BSP_RBSP,
	/* Ritual's own ident, "FAKK": version 12 for Heavy Metal: F.A.K.K.2
	 * and version 42 for Alice's revision of it. Its header carries a
	 * checksum Quake III's does not, and its twenty lumps are in a different
	 * order and include entity lights and per-surface light definitions, for
	 * which IBSP 46 has no counterpart. Both versions share one record layout;
	 * each profile admits only its own version. */
	COM_GAME_BSP_FAKK,
	/* 2015's ident for Medal of Honor: "2015" version 19 in Allied Assault
	 * 1.00. Its header carries the checksum Ritual's does, but its 28 lumps
	 * are its own: side equations for fence masks, terrain patches, sphere
	 * lights and static models, and 64-byte leaves that index the last two. */
	COM_GAME_BSP_MOHAA,
	/* The Q3Test releases' "IBSP" 44 (1.02 to 1.05) and 45 (1.06 to 1.08),
	 * Quake III's own format before its 1.09 revision. The Q3Test profile
	 * admits only the version its pinned release shipped. */
	COM_GAME_BSP_Q3TEST
} comGameBspFormat_t;

typedef enum comGameDetectionStatus_e {
	COM_GAME_DETECTION_OK = 0,
	COM_GAME_DETECTION_NOT_FOUND,
	COM_GAME_DETECTION_AMBIGUOUS,
	COM_GAME_DETECTION_UNKNOWN_REQUEST,
	COM_GAME_DETECTION_UNSUPPORTED_BUILD,
	COM_GAME_DETECTION_UNRECOGNIZED_PAK,
	COM_GAME_DETECTION_MODE_REQUIRED,
	COM_GAME_DETECTION_INCOMPLETE_ENGINE
} comGameDetectionStatus_t;

typedef enum comGameDetectionEvidence_e {
	COM_GAME_EVIDENCE_NONE = 0,
	COM_GAME_EVIDENCE_EXPLICIT,
	COM_GAME_EVIDENCE_BASE_PATH,
	COM_GAME_EVIDENCE_APP_PATH,
	COM_GAME_EVIDENCE_STEAM_PATH
} comGameDetectionEvidence_t;

enum {
	COM_GAME_MARKER_NONE = 0,
	COM_GAME_MARKER_QUAKE3 = 1 << 0,
	COM_GAME_MARKER_QUAKELIVE = 1 << 1,
	COM_GAME_MARKER_UNRECOGNIZED_PAK = 1 << 2,
	COM_GAME_MARKER_RTCW_SP = 1 << 3,
	COM_GAME_MARKER_RTCW_MP = 1 << 4,
	COM_GAME_MARKER_WOLFET = 1 << 5,
	/* Elite Force's single player and Holomatch modes share baseEF/pak0.pk3.
	 * The one pak sets both marks, so the user must choose the mode. */
	COM_GAME_MARKER_STVEF_SP = 1 << 6,
	COM_GAME_MARKER_STVEF_MP = 1 << 7,
	COM_GAME_MARKER_QUAKE3_IHV = 1 << 8,
	/* Jedi Academy's single player and multiplayer share one base/ directory
	 * and its assets; each mode is marked by its own game module beside them. */
	COM_GAME_MARKER_JKA_SP = 1 << 9,
	COM_GAME_MARKER_JKA_MP = 1 << 10,
	/* Jedi Outcast shares one base/ directory between its modes as well, and
	 * both of its configurations sit in the same pak, so the same mode choice
	 * applies. Its multiplayer modules are QVMs rather than libraries. */
	COM_GAME_MARKER_JK2_SP = 1 << 11,
	COM_GAME_MARKER_JK2_MP = 1 << 12,
	/* Alice is one title with one mode, so its base/pak0.pk3 sets a
	 * single mark and no mode choice is asked of the player. */
	COM_GAME_MARKER_ALICE = 1 << 13,
	/* Heavy Metal: F.A.K.K.2 is one title with one mode as well, marked by
	 * its fakk/pak0.pk3. */
	COM_GAME_MARKER_FAKK2 = 1 << 14,
	/* Medal of Honor: Allied Assault is one title and one mode here, marked
	 * by its main/Pak0.pk3. Return to Castle Wolfenstein's base directory is
	 * also main/ and its base pak the same name in lower case, so one file can
	 * answer both titles' probes; only its contents decide which it is. */
	COM_GAME_MARKER_MOHAA = 1 << 15,
	/* A Q3Test installation: its demoq3/pak0.pk3 and the Win32 modules beside
	 * the executable, recognized structurally (q3test_install.hpp), since the
	 * 1.11 demo keeps a pak of the same name in the same directory. */
	COM_GAME_MARKER_Q3TEST = 1 << 16
};

enum {
	COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 = 1 << 0,
	COM_PROTOCOL_FAMILY_IOQUAKE3 = 1 << 1,
	COM_PROTOCOL_FAMILY_QUAKELIVE = 1 << 2,
	COM_PROTOCOL_FAMILY_RTCW_SP = 1 << 3,
	COM_PROTOCOL_FAMILY_IORTCW_SP = 1 << 4,
	COM_PROTOCOL_FAMILY_RTCW_MP = 1 << 5,
	COM_PROTOCOL_FAMILY_IORTCW_MP = 1 << 6,
	COM_PROTOCOL_FAMILY_WOLFET = 1 << 7,
	/* Elite Force: SP loopback 40, retail Holomatch 24, ioEF/Lilium 26. */
	COM_PROTOCOL_FAMILY_STVEF_SP = 1 << 8,
	COM_PROTOCOL_FAMILY_STVEF_HM = 1 << 9,
	COM_PROTOCOL_FAMILY_IOEF = 1 << 10,
	COM_PROTOCOL_FAMILY_Q3_POINT_RELEASES = 1 << 11,
	COM_PROTOCOL_FAMILY_QUAKE3_IHV = 1 << 12,
	/* Jedi Academy: SP loopback 40 and multiplayer 26. Both numbers are also
	 * spoken by another title, so a version maps to every family that uses it
	 * and each profile admits only its own. */
	COM_PROTOCOL_FAMILY_JKA_SP = 1 << 13,
	COM_PROTOCOL_FAMILY_JKA_MP = 1 << 14,
	/* Jedi Outcast 1.02a: single player's loopback 40, which Elite Force and
	 * Jedi Academy also speak, and multiplayer's 15. Both numbers are read
	 * from the retail binaries' own cvar registrations. */
	COM_PROTOCOL_FAMILY_JK2_SP = 1 << 15,
	COM_PROTOCOL_FAMILY_JK2_MP = 1 << 16,
	/* Alice speaks protocol 1, read from retail alice.exe's own cvar
	 * registration and from the connect userinfo it writes. */
	COM_PROTOCOL_FAMILY_ALICE = 1 << 17,
	/* Heavy Metal: F.A.K.K.2 speaks protocol 1 too: PROTOCOL_VERSION in the
	 * engine header Ritual shipped with its SDK. The two titles share the
	 * number and nothing else, so each profile admits only its own family. */
	COM_PROTOCOL_FAMILY_FAKK2 = 1 << 18,
	/* Medal of Honor: Allied Assault 1.00 speaks protocol 6. MOHAA.exe is
	 * encrypted on disk, so the number is OpenMoHAA's reading of the 1.00
	 * engine rather than one read here; 1.11 and later speak 8. */
	COM_PROTOCOL_FAMILY_MOHAA = 1 << 19,
	/* Q3Test: 35 (1.02, 1.03), 36 (1.05), 38 (1.06), 39 (1.07) and 40 (1.08),
	 * read from each executable. 40 is also three single players' number; the
	 * profile admits only the protocol of the release it pinned. */
	COM_PROTOCOL_FAMILY_Q3TEST = 1 << 20
};

enum {
	COM_MODULE_API_QVM = 1 << 0,
	COM_MODULE_API_LEGACY_NATIVE = 1 << 1,
	COM_MODULE_API_STRUCTURED_NATIVE = 1 << 2,
	/* Elite Force SP: GetGameAPI/GetUIAPI function tables, with the cgame's
	 * dllEntry/vmMain taken from the same loaded game library. */
	COM_MODULE_API_STVEF_SP_NATIVE = 1 << 3,
	COM_MODULE_API_QUAKE3_IHV_NATIVE = 1 << 4,
	/* Jedi Academy SP: a GetGameAPI function table, with the cgame's
	 * dllEntry/vmMain taken from the same library and no separate UI module. */
	COM_MODULE_API_JKA_SP_NATIVE = 1 << 5,
	/* Jedi Outcast SP: the same shape as Jedi Academy's, a GetGameAPI table
	 * from a library in the installation root. Its multiplayer needs no entry
	 * of its own, because its modules are ordinary QVMs. */
	COM_MODULE_API_JK2_SP_NATIVE = 1 << 6,
	/* Alice: two libraries in base/, fgamex86.dll exporting GetGameAPI
	 * and cgamex86.dll exporting GetCGameAPI, each a function table
	 * rather than Quake III's dllEntry/vmMain pair. Its menus are the
	 * engine's own .urc resources, so there is no UI module. */
	COM_MODULE_API_ALICE_NATIVE = 1 << 7,
	/* Heavy Metal: F.A.K.K.2: the same two entry points, from gamex86.dll
	 * and cgamex86.dll in fakk/, over the shorter tables Alice later grew:
	 * 105 game and 124 client-game imports rather than 111 and 125. */
	COM_MODULE_API_FAKK2_NATIVE = 1 << 8,
	/* Medal of Honor: Allied Assault: gamex86.dll's GetGameAPI and
	 * cgamex86.dll's GetCGameAPI in main/, over 2015's own tables -- 167 game
	 * and 168 client-game imports, game API version 11 -- which share the
	 * entry points' names with Ritual's and little else. Both libraries import
	 * their allocator from system86.dll in the installation root. */
	COM_MODULE_API_MOHAA_NATIVE = 1 << 9,
	/* Q3Test: qagamex86.dll's GetGameAPI (plus GetBotAIAPI from 1.06),
	 * cgamex86.dll's GetCGameAPI (1.05 to 1.07) or dllEntry/vmMain with its
	 * own syscall numbers (1.08), and uix86.dll's GetUIAPI (1.06 to 1.08),
	 * all in the installation root. */
	COM_MODULE_API_Q3TEST_NATIVE = 1 << 10
};

/* Engine defaults are separate from limits and module-owned game rules.
 * SP map aliases may request a different slot count from campaign allocation. */
typedef struct comGameServerPolicy_s {
	const char *gameType;
	const char *fragLimit;
	const char *clients;
	const char *singlePlayerCommandClients;
	const char *mapSkill;             /* NULL when the engine does not own it. */
	const char *campaignClients;      /* RTCW SP's AI allocation, otherwise NULL. */
	int fragLimitInServerInfo;
	const char *pure;
	int pureLatched;
} comGameServerPolicy_t;

enum {
	COM_BOT_AI_WEAPONS = 1 << 0,
	COM_BOT_AI_GOALS = 1 << 1,
	COM_BOT_AI_CHAT = 1 << 2,
	COM_BOT_AI_MOVEMENT = 1 << 3,
	COM_BOT_AI_QUAKE = COM_BOT_AI_WEAPONS | COM_BOT_AI_GOALS
		| COM_BOT_AI_CHAT | COM_BOT_AI_MOVEMENT
};

/* Bot-library storage defaults are distinct from server player-slot limits.
 * RTCW SP runs cast AI without Quake's weapon, item and chat configuration. */
typedef struct comGameBotPolicy_s {
	const char *enabled;
	const char *clients;
	const char *entities;
	int maxEntities;
	unsigned aiSystems;
	int navigationWorlds;
	/* Server-owned AI slots: RTCW reserves slot zero for the listen player. */
	int firstClientSlot;
	int clientRate;
	int setSnapshotInterval;
	/* The reliable-command window of such a slot. Each retail engine rings
	 * one fixed count for all its clients, bots included, but a bot sets up
	 * no channel, so no wire profile can supply it: 64 in quake3.exe 1.32,
	 * quakelive_steam.exe, stvoyHM.exe and the three single-player engines,
	 * 128 in jamp.exe, jk2mp.exe and alice.exe, and 256 in WolfSP.exe,
	 * WolfMP.exe, et.exe and etded.exe. F.A.K.K.2's 1.0 engine is encrypted
	 * on disk; the 1.02 patch's fakk2.exe rings 64 (0x004468dd), as Ritual's
	 * SDK header has it. Zero where the engine rings no commands at all, as
	 * the IHV test does, and then no bot takes a client slot.
	 * docs/fntech3/2026-09-24-bot-reliable-windows.md has the addresses. */
	int reliableCommands;
} comGameBotPolicy_t;

/* Userinfo appearance keys are part of the selected game's client contract.
 * NULL team/secondary-color defaults mean that game has no such engine keys.
 * Trailing members are zero for the Quake and Wolfenstein games: a NULL
 * headKey or colorKey means no such key, a NULL bodyKey means "model", and a
 * NULL playerName keeps the engine's UnnamedPlayer default. Elite Force SP
 * publishes its three-part player as legsModel/torsoModel/headModel. */
typedef struct comGameAppearancePolicy_s {
	const char *bodyModel;
	const char *headKey;
	const char *headModel;
	const char *teamModel;
	const char *colorKey;
	const char *secondColor;
	int modelSetsHead;
	int archiveDefaults;
	const char *bodyKey;
	const char *torsoKey;
	const char *torsoModel;
	const char *playerName;
} comGameAppearancePolicy_t;

typedef enum comGamePureProof_e {
	COM_PURE_PROOF_UNAVAILABLE,
	COM_PURE_PROOF_QVM,
	COM_PURE_PROOF_QL_NATIVE,
	COM_PURE_PROOF_RTCW_NATIVE,
	COM_PURE_PROOF_WOLFET_NATIVE,
	COM_PURE_PROOF_JKA_NATIVE,
	/* Medal of Honor: the general pak references alone, `cp refs... mixed`,
	 * with no server id, module checksum or `@` (its modules are loose
	 * files in main/, not in a pak). */
	COM_PURE_PROOF_MOHAA_REFERENCES
} comGamePureProof_t;

typedef enum comGameEntityHullPolicy_e {
	COM_ENTITY_HULL_TRACE_SHAPE,
	COM_ENTITY_HULL_ENTITY_FLAG
} comGameEntityHullPolicy_t;

/* The contents a server sweep tests a box entity as. Quake III's temporary box
 * brush is always CONTENTS_BODY; RTCW MP and ET give it the traced entity's own
 * contents for that sweep, then put CONTENTS_BODY back. */
typedef enum comGameEntityBoxContentsPolicy_e {
	COM_ENTITY_BOX_CONTENTS_BODY,
	COM_ENTITY_BOX_CONTENTS_ENTITY
} comGameEntityBoxContentsPolicy_t;

typedef struct comGameCinematicPolicy_s {
	/* Trusted catalog commands, without a trailing newline; NULL omits a movie. */
	const char *logoCommand;
	const char *introCommand;
	int repeatIntro;
} comGameCinematicPolicy_t;

typedef struct comGameProfile_s {
	comGameId_t id;
	const char *key;
	const char *displayName;
	const char *baseGame;
	const char *basePakName;          /* NULL for loose asset/module identity. */
	int preferredProtocol;
	int preferredDemoProtocol;
	unsigned allowedProtocolFamilies;
	unsigned allowedModuleApis;
	unsigned preferredModuleApi;
	int usesSteam;
	int supportsWorkshop;
	const char *steamAppId;
	const char *steamInstallName;
	comGameTitleId_t titleId;
	unsigned installMarker;
	const char *aliases[4];
	const char *clientConfig;
	const char *serverConfig;
	const char *pakOverridePrefix;
	const char *pakExcludePrefix;
	/* NULL when startup is implemented; this is not a compatibility claim. */
	const char *runtimeBlocker;
	int maxClients;
	int maxConfigstrings;
	int playerConfigstringBase;
	/* Includes the two reserved temporary hull handles at the end. */
	int collisionModelHandleCount;
	comGameEntityHullPolicy_t entityHullPolicy;
	comGameEntityBoxContentsPolicy_t entityBoxContentsPolicy;
	/* Private engine cache; NULL means this profile never reads or writes it. */
	const char *serverCacheFile;
	comGameServerPolicy_t server;
	comGameBotPolicy_t bots;
	comGamePureProof_t pureProof;
	comGameAppearancePolicy_t appearance;
	comGameCinematicPolicy_t cinematics;
	/* Stem of the retail paks a profile refuses to download, numbered 0-8 in
	 * its own base directory. NULL keeps Quake's pak0-pak8 naming. */
	const char *officialPakStem;
} comGameProfile_t;

typedef struct comGameMarkerInputs_s {
	const char *requestedProfile;
	unsigned basePathMarkers;
	unsigned appPathMarkers;
	unsigned steamPathMarkers;
} comGameMarkerInputs_t;

typedef struct comGameDetectionInputs_s {
	const char *requestedProfile;
	const char *basePath;
	const char *appPath;
	/* Indexed by comGameId_t; initialize unused entries to NULL. */
	const char *steamPaths[COM_GAME_COUNT];
} comGameDetectionInputs_t;

typedef struct comGameDetectionResult_s {
	comGameDetectionStatus_t status;
	comGameDetectionEvidence_t evidence;
	const comGameProfile_t *profile;
} comGameDetectionResult_t;

/* Pure decision layer used by tests and by the filesystem-probing adapter. */
comGameDetectionResult_t Com_DetectGameProfileMarkers(
	const comGameMarkerInputs_t *inputs );

/* Probe retail installation markers below the supplied root directory. */
unsigned Com_ProbeGameProfileMarkers( const char *rootPath );

/* Detect without mutating the process-wide active profile. */
comGameDetectionResult_t Com_DetectGameProfile(
	const comGameDetectionInputs_t *inputs );

/* The active profile is immutable after the first successful activation. */
int Com_ActivateGameProfile( comGameId_t id );
const comGameProfile_t *Com_GameProfile( void );
const comGameProfile_t *Com_GameProfileById( comGameId_t id );
int Com_GameIsQuake3( void );
int Com_GameIsQuake3IHV( void );
int Com_GameIsQuakeLive( void );
int Com_GameIsRTCW( void );
int Com_GameIsRTCWSP( void );
int Com_GameIsRTCWMP( void );
int Com_GameIsWolfET( void );
int Com_GameIsSTVEF( void );
int Com_GameIsSTVEFSP( void );
int Com_GameIsSTVEFMP( void );
int Com_GameIsJKA( void );
int Com_GameIsJKASP( void );
int Com_GameIsJKAMP( void );
int Com_GameIsJK2( void );
int Com_GameIsJK2SP( void );
int Com_GameIsJK2MP( void );
int Com_GameIsAlice( void );
int Com_GameIsFAKK2( void );
/* Alice or Heavy Metal: F.A.K.K.2: the two titles on Ritual's engine, which
 * share its engine-side behaviour where neither profile's own contract says
 * otherwise. Contract values that differ between them are chosen by profile,
 * never through this. */
int Com_GameIsRitual( void );
/* Medal of Honor: Allied Assault. 2015 licensed Ritual's engine, but its
 * modules, records and formats are its own, so it is never a Ritual profile:
 * where it shares a path with Alice's engine the call site names it. */
int Com_GameIsMOHAA( void );
/* The Q3Test releases (q3test_release.h names the pinned one). */
int Com_GameIsQ3Test( void );
/* The protocol the active profile speaks by default: a Q3Test release's own
 * number, otherwise the catalog's preferred protocol. */
int Com_GamePreferredProtocol( void );
int Com_GamePreferredDemoProtocol( void );
/* The server port a profile listens on when net_port is not set. */
int Com_GameDefaultServerPort( void );
/* The master a profile's retail server publishes to and its browser asks
 * (sv_master1's default), or NULL where the profile has none of this kind. */
const char *Com_GameDefaultMasterServer( void );
/* The port a master address is sent to when it names none. */
int Com_GameMasterPort( void );
/* The game a "heartbeat" names, and the one it names at shutdown. */
const char *Com_GameHeartbeatName( void );
const char *Com_GameFlatlineName( void );
/* How much configstring text a profile's gamestate may hold. */
int Com_GameStateCharLimit( void );
/* The same test on a profile id, for code that holds its own copy of the
 * selection -- the renderers keep one in their import table. */
#define COM_GAME_ID_IS_RITUAL( id ) ( ( id ) == COM_GAME_ALICE || ( id ) == COM_GAME_FAKK2 )
/* RTCW SP/MP, ET and Jedi Academy multiplayer all prove pure admission with
 * their native cgame and UI module files rather than QVMs or one retail binary. */
int Com_PureProofIsNativeModulePair( const comGameProfile_t *profile );

/* Map exact supported protocol versions to profile capability bits. */
unsigned Com_ProtocolFamilyForVersion( int protocol );
int Com_GameProfileAllowsProtocol( const comGameProfile_t *profile,
	int protocol );
int Com_GameProfileAllowsModuleApi( const comGameProfile_t *profile,
	unsigned moduleApi );

const char *Com_GameDetectionStatusName( comGameDetectionStatus_t status );
const char *Com_GameDetectionEvidenceName( comGameDetectionEvidence_t evidence );

#ifdef __cplusplus
}
#endif

#endif
