/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Ray-traced material sources

The raster renderer folds a shader's stages into multitexture passes after
parsing (CollapseMultitexture), swapping lightmap bundles and rewriting blend
state as it goes. The path tracer evaluates Quake III's stages on the GPU, one
blend at a time, so FinishShader keeps a copy of each active stage as the
parser left it: one texture bundle, its coordinate and colour generators and
its framebuffer blend. That copy is taken after the renderer's own
simplifications (detail culling, vertex-light collapse, the white-image
filter) and before multitexture folding.
===========================================================================
*/

#ifndef RT_STAGE_SOURCE_H
#define RT_STAGE_SOURCE_H

#define RT_STAGE_MAX_ANIMATIONS 8
// A lightmap bundle may carry one extra transform for an external lightmap
// atlas (tr_shader.c reserves TR_MAX_TEXMODS + 1 slots).
#define RT_STAGE_MAX_TEXMODS ( TR_MAX_TEXMODS + 1 )

typedef struct rtStageSource_s {
	image_t			*images[RT_STAGE_MAX_ANIMATIONS];
	int				numImages;			// animMap frames kept (1 for a static map)
	float			animSpeed;			// animMap frames per second

	unsigned		stateBits;			// GLS_ blend, alpha test and depth bits

	texCoordGen_t	tcGen;
	vec3_t			tcGenVectors[2];
	int				numTexMods;
	texModInfo_t	texMods[RT_STAGE_MAX_TEXMODS];

	colorGen_t		rgbGen;
	waveForm_t		rgbWave;
	alphaGen_t		alphaGen;
	waveForm_t		alphaWave;
	color4ub_t		constantColor;

	int				lightmap;			// LIGHTMAP_INDEX_*
	qboolean		vertexLightmap;
	qboolean		isDetail;
	qboolean		isVideoMap;
	qboolean		isScreenMap;
} rtStageSource_t;

#endif
