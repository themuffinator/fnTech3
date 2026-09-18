/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_FOLIAGE_TYPES_H
#define FNTECH3_TR_WOLFET_FOLIAGE_TYPES_H

#include "tr_wolfet_foliage.h"

typedef struct {
	vec3_t origin;
	color4ub_t color;	// already shifted for the map's overbright range
} wolfetFoliageInstance_t;

/* One foliage surface of an Enemy Territory map: a mesh and the origins it is
 * drawn at. The bounds and sphere enclose every instance. */
typedef struct {
	surfaceType_t surfaceType;
#ifdef USE_LEGACY_DLIGHTS
	int dlightBits;
#endif
	vec3_t bounds[2];
	vec3_t origin;
	float radius;

	int numIndexes;
	int *indexes;
	int numVerts;
	vec4_t *xyz;
	vec4_t *normal;
	vec2_t *texCoords;
	vec2_t *lmTexCoords;
	int numInstances;
	wolfetFoliageInstance_t *instances;
} srfWolfETFoliage_t;

#endif
