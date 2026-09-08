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

typedef enum comGameId_e {
	COM_GAME_AUTO = 0,
	COM_GAME_QUAKE3,
	COM_GAME_QUAKELIVE
} comGameId_t;

typedef enum comGameDetectionStatus_e {
	COM_GAME_DETECTION_OK = 0,
	COM_GAME_DETECTION_NOT_FOUND,
	COM_GAME_DETECTION_AMBIGUOUS,
	COM_GAME_DETECTION_UNKNOWN_REQUEST,
	COM_GAME_DETECTION_UNSUPPORTED_BUILD,
	COM_GAME_DETECTION_UNRECOGNIZED_PAK
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
	COM_GAME_MARKER_UNRECOGNIZED_PAK = 1 << 2
};

enum {
	COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 = 1 << 0,
	COM_PROTOCOL_FAMILY_IOQUAKE3 = 1 << 1,
	COM_PROTOCOL_FAMILY_QUAKELIVE = 1 << 2
};

enum {
	COM_MODULE_API_QVM = 1 << 0,
	COM_MODULE_API_LEGACY_NATIVE = 1 << 1,
	COM_MODULE_API_STRUCTURED_NATIVE = 1 << 2
};

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
	const char *q3SteamPath;
	const char *qlSteamPath;
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
