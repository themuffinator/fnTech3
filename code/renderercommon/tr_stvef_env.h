/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_ENV_H
#define FNTECH3_TR_STVEF_ENV_H

/*
 * Elite Force's environment mapping, as stvoyHM.exe 1.20 computes it
 * (RB_CalcEnvironmentTexCoords 0x47cb50). Where Quake III maps the reflected
 * view vector's y and z around 0.5, Elite Force takes half its x and y:
 * s = (n.v) n.x - v.x / 2 and t = (n.v) n.y - v.y / 2, with v the normalized
 * direction to the viewer. A first-person entity reflects its own light
 * direction instead: s = (n.L) n.x - L.x and t = (n.L) n.y - L.y.
 *
 * Each renderer's tr_shade_calc.c includes this after tr_local.h. Its GPU
 * environment paths are off for Elite Force, so every environment-mapped
 * stage comes here.
 */

static ID_INLINE qboolean R_STVEFEnvironment( void ) {
	return ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP ? qtrue : qfalse;
}

static void RB_CalcEnvironmentTexCoordsSTVEF( float *st ) {
	const float *v = tess.xyz[0];
	const float *normal = tess.normal[0];
	int i;

	if ( backEnd.currentEntity && ( backEnd.currentEntity->e.renderfx & RF_FIRST_PERSON ) ) {
		const float *light = backEnd.currentEntity->lightDir;
		for ( i = 0; i < tess.numVertexes; i++, normal += 4, st += 2 ) {
			const float d = normal[2] * light[2] + normal[1] * light[1] + normal[0] * light[0];
			st[0] = d * normal[0] - light[0];
			st[1] = d * normal[1] - light[1];
		}
		return;
	}

	for ( i = 0; i < tess.numVertexes; i++, v += 4, normal += 4, st += 2 ) {
		vec3_t viewer;
		float d;
		VectorSubtract( backEnd.or.viewOrigin, v, viewer );
		VectorNormalizeFast( viewer );
		d = viewer[2] * normal[2] + viewer[1] * normal[1] + viewer[0] * normal[0];
		st[0] = d * normal[0] - viewer[0] * 0.5f;
		st[1] = d * normal[1] - viewer[1] * 0.5f;
	}
}

#endif /* FNTECH3_TR_STVEF_ENV_H */
