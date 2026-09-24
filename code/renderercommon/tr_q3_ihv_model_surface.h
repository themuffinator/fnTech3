/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_MODEL_SURFACE_H
#define FNTECH3_TR_Q3_IHV_MODEL_SURFACE_H

#include "tr_q3_ihv_model.h"

/* The MD3 loader owns the allocation and endian conversion. Check the native
 * surface/frame span again before backend indexing. Outputs are caller-owned
 * scratch; publish them only after the whole surface succeeds. */
static int Q3IHV_ModelSurface( const md3Surface_t *surface, int oldframe, int frame,
	float fraction, const float move[3], const float sines[1024],
	float (*xyz)[3], float (*normal)[3], int capacity ) {
	const unsigned char *records;
	int vertex;
	int64_t bytes;
	if ( !surface || !xyz || !normal || surface->numVerts < 0 ||
		surface->numVerts > capacity || surface->numVerts >= SHADER_MAX_VERTEXES || surface->numFrames <= 0 ||
		frame < 0 || oldframe < 0 || frame >= surface->numFrames || oldframe >= surface->numFrames ||
		surface->ofsXyzNormals < sizeof( *surface ) || surface->ofsEnd > INT32_MAX ||
		surface->ofsEnd < surface->ofsXyzNormals ||
		!Q3IHV_EntityFinite( &fraction ) ) return 0;
	bytes = (int64_t)surface->numVerts * surface->numFrames * 8;
	if ( bytes > surface->ofsEnd - surface->ofsXyzNormals ) return 0;
	records = (const unsigned char *)surface + surface->ofsXyzNormals;
	for ( vertex = 0; vertex < surface->numVerts; ++vertex ) {
		if ( !Q3IHV_ModelVertex( records + ( (size_t)oldframe * surface->numVerts + vertex ) * 8,
			records + ( (size_t)frame * surface->numVerts + vertex ) * 8,
			fraction, move, sines, xyz[vertex], normal[vertex] ) ) return 0;
	}
	return 1;
}

#endif
