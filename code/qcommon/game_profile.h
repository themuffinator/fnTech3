/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_GAME_PROFILE_H
#define FNTECH3_GAME_PROFILE_H

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
	COM_GAME_COUNT
} comGameId_t;

typedef enum comGameTitleId_e
#ifdef __cplusplus
	: unsigned int
#endif
{
	COM_GAME_TITLE_QUAKE3 = 1,
	COM_GAME_TITLE_QUAKELIVE,
	COM_GAME_TITLE_RTCW
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
	COM_GAME_BSP_RTCW
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
	COM_GAME_MARKER_RTCW_MP = 1 << 4
};

enum {
	COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 = 1 << 0,
	COM_PROTOCOL_FAMILY_IOQUAKE3 = 1 << 1,
	COM_PROTOCOL_FAMILY_QUAKELIVE = 1 << 2,
	COM_PROTOCOL_FAMILY_RTCW_SP = 1 << 3,
	COM_PROTOCOL_FAMILY_IORTCW_SP = 1 << 4,
	COM_PROTOCOL_FAMILY_RTCW_MP = 1 << 5,
	COM_PROTOCOL_FAMILY_IORTCW_MP = 1 << 6
};

enum {
	COM_MODULE_API_QVM = 1 << 0,
	COM_MODULE_API_LEGACY_NATIVE = 1 << 1,
	COM_MODULE_API_STRUCTURED_NATIVE = 1 << 2
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
} comGameBotPolicy_t;

/* Userinfo appearance keys are part of the selected game's client contract.
 * NULL team/secondary-color defaults mean that game has no such engine keys. */
typedef struct comGameAppearancePolicy_s {
	const char *bodyModel;
	const char *headKey;
	const char *headModel;
	const char *teamModel;
	const char *colorKey;
	const char *secondColor;
	int modelSetsHead;
	int archiveDefaults;
} comGameAppearancePolicy_t;

typedef enum comGamePureProof_e {
	COM_PURE_PROOF_UNAVAILABLE,
	COM_PURE_PROOF_QVM,
	COM_PURE_PROOF_QL_NATIVE,
	COM_PURE_PROOF_RTCW_NATIVE
} comGamePureProof_t;

typedef enum comGameEntityHullPolicy_e {
	COM_ENTITY_HULL_TRACE_SHAPE,
	COM_ENTITY_HULL_ENTITY_FLAG
} comGameEntityHullPolicy_t;

typedef struct comGameProfile_s {
	comGameId_t id;
	const char *key;
	const char *displayName;
	const char *baseGame;
	const char *basePakName;
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
	/* Private engine cache; NULL means this profile never reads or writes it. */
	const char *serverCacheFile;
	comGameServerPolicy_t server;
	comGameBotPolicy_t bots;
	comGamePureProof_t pureProof;
	comGameAppearancePolicy_t appearance;
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
int Com_GameIsQuakeLive( void );
int Com_GameIsRTCW( void );
int Com_GameIsRTCWSP( void );
int Com_GameIsRTCWMP( void );

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
