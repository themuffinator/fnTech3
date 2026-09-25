/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_Q3TEST_RELEASE_H
#define FNTECH3_Q3TEST_RELEASE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The public Q3Test builds of 1999, served by the one `q3test` profile. Each
 * release is its own contract set -- protocol, map format and the tables its
 * own qagamex86.dll, cgamex86.dll and uix86.dll were built against -- so the
 * profile pins exactly one of them at startup, from installation evidence,
 * before the filesystem, the protocol or any module starts, and never changes
 * it afterwards. Observations: docs/fntech3/Q3TEST.md. */
typedef enum q3testReleaseId_e
#ifdef __cplusplus
	: unsigned int
#endif
{
	Q3TEST_RELEASE_NONE = 0,
	Q3TEST_RELEASE_1_02,
	Q3TEST_RELEASE_1_03,
	Q3TEST_RELEASE_1_05,
	Q3TEST_RELEASE_1_06,
	Q3TEST_RELEASE_1_07,
	Q3TEST_RELEASE_1_08,
	Q3TEST_RELEASE_COUNT
} q3testReleaseId_t;

/* How a release's client game is entered: a GetCGameAPI function table
 * (1.05 to 1.07), or dllEntry/vmMain with syscall numbers (1.08). 1.02 and
 * 1.03 shipped for Windows as dedicated servers alone, with no client game. */
typedef enum q3testCgameEntry_e {
	Q3TEST_CGAME_NONE = 0,
	Q3TEST_CGAME_TABLE,
	Q3TEST_CGAME_SYSCALLS
} q3testCgameEntry_t;

typedef struct q3testRelease_s {
	q3testReleaseId_t id;
	const char *version;        /* "1.02" */
	const char *versionString;  /* the engine's own version text, "Q3T 1.02" */
	int protocol;
	int bspVersion;
	/* PE header TimeDateStamp of each module the release shipped for Win32
	 * x86; zero where it shipped none. These name a build, they do not
	 * authenticate one. */
	unsigned gameStamp;
	unsigned cgameStamp;
	unsigned uiStamp;
	q3testCgameEntry_t cgameEntry;
	/* Whether the game library also exports GetBotAIAPI (1.06 and later). */
	int botAI;
	/* demoq3/pak0.pk3's maps/q3test1.bsp, which names the pak a release
	 * shipped: 1.02 and 1.03 share one pak, and 1.06 to 1.08 share the map. */
	unsigned mapSize;
	unsigned mapCrc;
} q3testRelease_t;

const q3testRelease_t *Com_Q3TestReleaseById( q3testReleaseId_t id );
/* The release the active `q3test` profile pinned, or NULL under any other
 * profile or before activation. */
const q3testRelease_t *Com_Q3TestRelease( void );
/* Pins the release once. A second call succeeds only with the same release. */
int Com_Q3TestActivateRelease( q3testReleaseId_t id );
/* Reads installation evidence below rootPath without loading any module:
 * the root's qagamex86.dll (and cgamex86.dll / uix86.dll when present) and
 * demoq3/pak0.pk3. Unknown, mixed or incomplete evidence gives NONE. */
q3testReleaseId_t Com_Q3TestDetectRelease( const char *rootPath );

#ifdef __cplusplus
}
#endif

#endif
