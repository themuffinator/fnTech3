/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_LIGHTMAP_H
#define FNTECH3_TR_WOLFET_LIGHTMAP_H

#include <stdio.h>
#include <string.h>

/* Enemy Territory's map compiler can write a map's lightmaps beside it rather
 * than into it: maps/<map>/lm_NNNN.tga, numbered as the surfaces name them.
 * The BSP's own lightmap lump is then empty. These are the naming and size
 * rules of the original ET renderer; they depend on nothing in the renderer. */

/* ET holds at most this many lightmaps. */
#define FN_WOLFET_MAX_LIGHTMAPS 256

/* worldName is the BSP's path, "maps/<map>.bsp"; the lightmaps live in the
 * directory of the same name. Returns 0 when the name does not fit. */
static inline int FnWolfET_ExternalLightmapName( char *out, size_t size, const char *worldName, int index ) {
	const char *slash, *dot;
	size_t stem;
	int written;
	if ( !out || !size || !worldName || index < 0 || index >= FN_WOLFET_MAX_LIGHTMAPS ) return 0;
	slash = strrchr( worldName, '/' );
	dot = strrchr( worldName, '.' );
	stem = dot && ( !slash || dot > slash ) ? (size_t)( dot - worldName ) : strlen( worldName );
	if ( stem > 0x7fffffff ) return 0;
	written = snprintf( out, size, "%.*s/lm_%04d.tga", (int)stem, worldName, index );
	return written > 0 && (size_t)written < size;
}

/* ET refuses a lightmap whose sides are not powers of two, and the surfaces
 * that name it are then lit by their vertices. */
static inline int FnWolfET_LightmapSizeValid( int width, int height ) {
	return width > 0 && height > 0 && !( width & ( width - 1 ) ) && !( height & ( height - 1 ) );
}

#endif
