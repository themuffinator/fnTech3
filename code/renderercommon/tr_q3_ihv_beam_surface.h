/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_BEAM_SURFACE_H
#define FNTECH3_TR_Q3_IHV_BEAM_SURFACE_H

#include "tr_q3_ihv_beam.h"

static void RB_SurfaceIHVBeam( void ) {
	float vertices[12][3];
	int base, i, component;
	if ( !Q3IHV_BeamVertices( backEnd.currentEntity->e.origin, backEnd.currentEntity->e.oldorigin, vertices ) ) return;
#ifdef USE_VBO
	VBO_Flush();
#endif
	RB_CHECKOVERFLOW( 12, 36 );
#ifdef RENDERER_GLX
	GLX_CompatMarkDynamicCategory( GLX_DYNAMIC_CATEGORY_MASK_BEAM );
#endif
#if defined(USE_VBO) || defined(RENDERER_GLX)
	tess.surfType = SF_ENTITY;
#endif
	base = tess.numVertexes;
	for ( i = 0; i < 12; ++i ) {
		memcpy( tess.xyz[base + i], vertices[i], sizeof( vertices[i] ) );
		tess.xyz[base + i][3] = 1;
		for ( component = 0; component < 3; ++component ) tess.normal[base + i][component] = vertices[i & ~1][component] * .25f;
		tess.texCoords[0][base + i][0] = tess.texCoords[0][base + i][1] = 0;
		tess.vertexColors[base + i].rgba[0] = tess.vertexColors[base + i].rgba[3] = 255;
		tess.vertexColors[base + i].rgba[1] = tess.vertexColors[base + i].rgba[2] = 0;
	}
	for ( i = 0; i < 6; ++i ) {
		const int a = base + i * 2, b = a + 1, c = base + ( ( i + 1 ) % 6 ) * 2, d = c + 1;
		tess.indexes[tess.numIndexes++] = a; tess.indexes[tess.numIndexes++] = b; tess.indexes[tess.numIndexes++] = c;
		tess.indexes[tess.numIndexes++] = c; tess.indexes[tess.numIndexes++] = b; tess.indexes[tess.numIndexes++] = d;
	}
	tess.numVertexes += 12;
}

#endif
