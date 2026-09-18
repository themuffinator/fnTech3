/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_LIGHTMAP_LOAD_H
#define FNTECH3_TR_WOLFET_LIGHTMAP_LOAD_H

#include "tr_wolfet_lightmap.h"

/* Included by a renderer's BSP loader after R_LoadLightmaps. An external
 * lightmap is shifted for the map's overbright range exactly as a lightmap in
 * the BSP is, is never compressed, and draws opaque. */
/* The ray-traced renderer keeps no per-lightmap average colours. */
#ifndef RENDERER_RTX
#define R_WOLFET_LIGHTMAP_AVERAGES
#endif

static image_t *R_LoadWolfETExternalLightmap( const char *name, float *average ) {
	const qboolean debugIntensity = r_lightmap->integer == 2;
	int width, height, i;
	byte *pic = R_LoadImagePixels( name, &width, &height );
	image_t *image;
	if ( !pic ) {
		return NULL;
	}
	if ( !FnWolfET_LightmapSizeValid( width, height ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: lightmap %s is not a power of two in size\n", name );
		ri.Free( pic );
		return NULL;
	}
	for ( i = 0; i < width * height; i++ ) {
		byte *texel = pic + i * 4;
		if ( debugIntensity ) {
			float intensity = 0.33f * texel[0] + 0.685f * texel[1] + 0.063f * texel[2];
			float out[3];
			intensity = intensity > 255.0f ? 1.0f : intensity / 255.0f;
			HSVtoRGB( intensity, 1.0f, 0.5f, out );
			texel[0] = (byte)( out[0] * 255.0f );
			texel[1] = (byte)( out[1] * 255.0f );
			texel[2] = (byte)( out[2] * 255.0f );
		} else {
			R_ColorShiftLightingBytes( texel, texel, qfalse );
		}
		texel[3] = 255;
	}
#ifdef R_WOLFET_LIGHTMAP_AVERAGES
	R_LightmapAverageColor( pic, width, 0, 0, width, height, average );
#else
	(void)average;
#endif
	image = R_CreateImage( name, NULL, pic, width, height, lightmapFlags | IMGFLAG_CLAMPTOEDGE );
	ri.Free( pic );
	return image;
}

/* An Enemy Territory map with no lightmaps of its own names external ones by
 * index. Every index a surface names is loaded now, and one that cannot be
 * leaves an empty slot, so its surfaces are lit by their vertices, as in ET. */
static void R_LoadWolfETExternalLightmaps( const lump_t *surfaces ) {
	const dsurface_t *in;
	int count, i, highest = -1, loaded = 0;
	if ( ri.gameId != COM_GAME_WOLFET || tr.numLightmaps || !tr.worldMapLoaded ) {
		return;
	}
	if ( surfaces->filelen % sizeof( *in ) ) {
		return;	// R_LoadSurfaces reports the malformed lump
	}
	in = (const dsurface_t *)( fileBase + surfaces->fileofs );
	count = surfaces->filelen / sizeof( *in );
	for ( i = 0; i < count; i++ ) {
		const int lightmap = LittleLong( in[i].lightmapNum );
		if ( lightmap > highest && lightmap < FN_WOLFET_MAX_LIGHTMAPS ) {
			highest = lightmap;
		}
	}
	if ( highest < 0 ) {
		return;
	}
	tr.numLightmaps = highest + 1;
	tr.lightmaps = ri.Hunk_Alloc( tr.numLightmaps * sizeof( image_t * ), h_low );
#ifdef R_WOLFET_LIGHTMAP_AVERAGES
	tr.lightmapAverageColors = ri.Hunk_Alloc( tr.numLightmaps * sizeof( *tr.lightmapAverageColors ), h_low );
#endif
	for ( i = 0; i <= highest; i++ ) {
		char name[MAX_QPATH];
		if ( !FnWolfET_ExternalLightmapName( name, sizeof( name ), s_worldData.name, i ) ) {
			continue;
		}
#ifdef R_WOLFET_LIGHTMAP_AVERAGES
		tr.lightmaps[i] = R_LoadWolfETExternalLightmap( name, tr.lightmapAverageColors[i] );
#else
		tr.lightmaps[i] = R_LoadWolfETExternalLightmap( name, NULL );
#endif
		if ( tr.lightmaps[i] ) {
			loaded++;
		}
	}
	ri.Printf( PRINT_ALL, "...loaded %i of %i external lightmaps\n", loaded, tr.numLightmaps );
}

#endif
