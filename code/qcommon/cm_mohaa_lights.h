/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor's sphere lights, for its client game; see cm_mohaa_lights.c.
===========================================================================
*/
#ifndef FNTECH3_CM_MOHAA_LIGHTS_H
#define FNTECH3_CM_MOHAA_LIGHTS_H

#include "bsp_mohaa.h"

typedef struct {
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
	int		leaf;			/* the leaf the light is in, or -1 */
} cmMOHAALight_t;

typedef struct cmMOHAALights_s {
	int				numLights;
	cmMOHAALight_t	*lights;
	/* each leaf's lights: `count` indexes into `list` from `first`, and
	 * whether its list named the sun */
	int				numLeafs;
	int				*leafFirst;
	int				*leafCount;
	qboolean		*leafSun;
	int				*list;
	/* the worldspawn's sun */
	qboolean		sun;
	vec3_t			sunColor;
	vec3_t			sunDirection;
} cmMOHAALights_t;

void	CMod_LoadMOHAALights( const mohaa_translation_t *translation );

#endif
