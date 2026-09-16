/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GAME_MANAGER_H
#define FNTECH3_GAME_MANAGER_H

#include "game_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A title is an installation/product; a profile is one immutable engine
 * contract. RTCW's SP and MP share a title, but never share an active ABI.
 * The catalog is read-only and available before filesystem or Steam startup.
 * Implementation lives with the early profile decision layer in game_profile.cpp.
 */
typedef struct comGameTitle_s {
	comGameTitleId_t id;
	const char *key;
	const char *displayName;
} comGameTitle_t;

unsigned Com_GameManagerTitleCount( void );
const comGameTitle_t *Com_GameManagerTitleAt( unsigned index );
const comGameTitle_t *Com_GameManagerTitleById( comGameTitleId_t id );
unsigned Com_GameManagerProfileCount( void );
const comGameProfile_t *Com_GameManagerProfileAt( unsigned index );
/* Canonical keys and explicit mode aliases only; "rtcw" needs SP or MP. */
const comGameProfile_t *Com_GameManagerFindProfile( const char *key );
unsigned Com_GameManagerTitleProfileCount( comGameTitleId_t title );
const comGameProfile_t *Com_GameManagerTitleProfileAt(
	comGameTitleId_t title, unsigned index );
int Com_GameManagerCanActivate( const comGameProfile_t *profile );
/* Classify using the selected title's complete disk contract. Q3 and QL
 * retain their existing v43/v46/v47 support; RTCW SP/MP accept RTCW v47.
 * No profile (or a non-catalog profile) means no supported format. */
comGameBspFormat_t Com_GameManagerBspFormat( const comGameProfile_t *profile,
	int version );
comGameBspFormat_t Com_GameBspFormat( int version );
typedef enum comGameModeLaunchStatus_e {
	COM_GAME_MODE_LAUNCH_READY,
	COM_GAME_MODE_LAUNCH_INVALID_PROFILE,
	COM_GAME_MODE_LAUNCH_ALREADY_SELECTED,
	COM_GAME_MODE_LAUNCH_DIFFERENT_TITLE,
	COM_GAME_MODE_LAUNCH_INVALID_VALUE,
	COM_GAME_MODE_LAUNCH_TOO_LONG
} comGameModeLaunchStatus_t;
/* Format a proposed same-title mode relaunch. This never activates a profile
 * or starts a process: the caller must check CanActivate and prepare the same
 * executable before shutting down. Preserve installation/home/window mode;
 * clear fs_game so a mode-specific mod cannot leak into the other mode.
 * Unsupported quoted/control characters and oversized commands are rejected,
 * never escaped with syntax the engine tokenizer does not understand.
 * Destination is unchanged on failure, including already-selected requests.
 */
enum { COM_GAME_MODE_LAUNCH_BYTES = 1024 };
comGameModeLaunchStatus_t Com_GameManagerFormatModeLaunch(
	const comGameProfile_t *current, const comGameProfile_t *target,
	const char *basePath, const char *homePath, int fullscreen,
	char *destination, unsigned capacity );
const char *Com_GameManagerModeLaunchStatusMessage( comGameModeLaunchStatus_t status );
const char *Com_GameManagerConfigFile( const comGameProfile_t *profile,
	int dedicated );
/* SP intentionally leaves MP favorites alone. Cache layouts belong to the
 * engine, so RTCW MP uses a distinct file from either source engine. */
const char *Com_GameManagerServerCacheFile( const comGameProfile_t *profile );
/* Archive leaf names only. The comparator models retail's virtual zz_ sort
 * key without modifying filenames; ascending mounts give later names priority.
 */
int Com_GameManagerAllowsPak( const comGameProfile_t *profile,
	const char *filename );
int Com_GameManagerComparePaks( const comGameProfile_t *profile,
	const char *left, const char *right );
int Com_GameManagerInstallationMatches( const comGameProfile_t *profile,
	unsigned markers );
int Com_GameManagerIsConfigFile( const char *filename );
/* Ordered native-module candidates for cgame, qagame or ui. Architecture and
 * extension describe the host, never a user-selected ABI. A zero result ends
 * the candidate list or rejects invalid input; output is unchanged on failure.
 */
int Com_GameManagerNativeModuleFilename( const comGameProfile_t *profile,
	const char *module, const char *architecture, const char *extension,
	unsigned candidate, char *destination, unsigned capacity );
/* Pure-proof module slots: Q3 cgame/UI QVMs, QL's fixed native binary, or
 * RTCW MP's host-specific cgame/UI DLLs. SP has no verified pure contract.
 * Invalid slots/profiles leave the output unchanged. */
int Com_GameManagerPureModuleFilename( const comGameProfile_t *profile,
	unsigned slot, const char *architecture, const char *extension,
	char *destination, unsigned capacity );
/* Exhaust virtual/mod candidates before considering installation-root files.
 * Both outputs are unchanged on failure. Root reads use the same cache/pinning
 * as virtual files; they never invoke the installation's DLL directly.
 */
int Com_GameManagerNativeModuleCandidate( const comGameProfile_t *profile,
	const char *module, const char *architecture, const char *extension,
	unsigned candidate, char *destination, unsigned capacity, int *installationRoot );
/* Retail SP also installs modules beside the executable. Resolve only a known
 * module leaf within the supplied installation root; no general VFS root mount.
 * The result is an existing regular file, contained in the canonical root.
 */
int Com_GameManagerResolveRootModule( const comGameProfile_t *profile,
	const char *root, const char *filename, const char *architecture,
	const char *extension, char *destination, unsigned capacity );
/* Config for this build's client/dedicated role and the active profile. */
const char *Com_GameConfigFile( void );
/* Active-profile limits are distinct from engine storage capacities. */
int Com_GameClientLimit( void );
int Com_GameConfigstringLimit( void );
int Com_GamePlayerConfigstringBase( void );

#ifdef __cplusplus
}
#endif
#endif
