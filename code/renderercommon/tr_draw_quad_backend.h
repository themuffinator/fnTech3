/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_DRAW_QUAD_BACKEND_H
#define FNTECH3_TR_DRAW_QUAD_BACKEND_H

// Use the existing 2D path for shader changes, overflow/flush, bloom and frame
// bookkeeping. It reserves four vertices; replace their geometry before the
// batch is submitted. Legacy stretch-picture commands keep their original size.
static const void *RB_DrawQuad( const void *data ) {
	const drawQuadCommand_t *cmd = (const drawQuadCommand_t *)data;
	int first, i;
	RB_StretchPic( &cmd->base );
	first = tess.numVertexes - 4;
	for ( i = 0; i < 4; ++i ) {
		tess.xyz[first + i][0] = cmd->quad.xy[i][0];
		tess.xyz[first + i][1] = cmd->quad.xy[i][1];
		tess.texCoords[0][first + i][0] = cmd->quad.st[i][0];
		tess.texCoords[0][first + i][1] = cmd->quad.st[i][1];
		if ( cmd->quad.colorMask & ( 1u << i ) ) tess.vertexColors[first + i] = cmd->quad.colors[i];
	}
	return (const void *)( cmd + 1 );
}

#endif
