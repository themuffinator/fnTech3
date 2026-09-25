/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_MOHAA_QCOMMON_H
#define FNTECH3_MOHAA_QCOMMON_H

/* The engine services OpenMoHAA's TIKI, skeletor and alias sources expect
 * from their own qcommon.h, served by fnTech3's. See q_shared.h beside this
 * file for why these sources see this header before the engine's. */

#include "q_shared.h"
#include "../../qcommon/qcommon.h"

/* The engine owns the global alias list (alias.h's APP_MODULE section). */
#ifndef APP_MODULE
#define APP_MODULE 1
#endif
#include "alias.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Model data is not zone memory here. A level's skeletal animations run to
 * tens of megabytes, and the zone is sized for Quake III; these sources'
 * allocations come from mohaa_tiki_memory.cpp instead, which frees them all
 * when the model cache is cleared. The engine's own zone calls in these
 * translation units are redirected to it, and nothing they allocate is
 * handed to the engine to free. */
void *MOHAA_TagMalloc( size_t size, int tag );
void *MOHAA_Malloc( size_t size );
void MOHAA_Free( void *ptr );

enum {
	TAG_TIKI = 1,
	TAG_SKEL = 2
};

#undef Z_TagMalloc
#undef Z_Malloc
#define Z_TagMalloc( size, tag ) MOHAA_TagMalloc( ( size ), ( tag ) )
#define Z_Malloc( size ) MOHAA_Malloc( ( size ) )
#define Z_Free( ptr ) MOHAA_Free( ( ptr ) )

/* Reads a file, quietly when asked; the engine's reader never complains. */
int MOHAA_ReadFileEx( const char *qpath, void **buffer, qboolean quiet );
#define FS_ReadFileEx( qpath, buffer, quiet ) MOHAA_ReadFileEx( ( qpath ), ( buffer ), ( quiet ) )

/* The loading screen's progress, which retail advances as each resource is
 * cached: MOHAA_LoadResource (mohaa_tiki.h) hands it to the engine's menus. */
void UI_LoadResource( const char *name );

/* The map a TIKI's `$mapspec` and `includes` sections are matched against:
 * the running level's name, or "utils" when none is loaded. */
const char *TIKI_MapName( void );

/* A path as Medal of Honor's engine keys its caches: lower case, blanks
 * dropped, separators made single forward slashes, and emptied if it begins
 * with one. The game module is handed the same routine (game import 30). */
void FS_CanonicalFilename( char *filename );

#ifdef __cplusplus
}
#endif

#endif
