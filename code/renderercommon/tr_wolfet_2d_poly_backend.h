/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_2D_POLY_BACKEND_H
#define FNTECH3_TR_WOLFET_2D_POLY_BACKEND_H

/* The fan is built the way ET's own backend builds it: every triangle shares
 * the first point, the screen depth is zero, and each point keeps the colour it
 * was submitted with rather than taking the current 2D colour. */
static const void *RB_DrawWolfET2DPolys( const void *data ) {
	const wolfet2DPolyCommand_t *cmd = (const wolfet2DPolyCommand_t *)data;
	const polyVert_t *verts = (const polyVert_t *)( cmd + 1 );
	int i, first;

	if ( cmd->shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( cmd->shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

#ifdef USE_FBO
	R_BloomScreen();
#endif

	RB_CHECKOVERFLOW( cmd->numVerts, ( cmd->numVerts - 2 ) * 3 );

	first = tess.numVertexes;
	for ( i = 0; i < cmd->numVerts - 2; ++i ) {
		tess.indexes[tess.numIndexes + 0] = first;
		tess.indexes[tess.numIndexes + 1] = first + i + 1;
		tess.indexes[tess.numIndexes + 2] = first + i + 2;
		tess.numIndexes += 3;
	}
	for ( i = 0; i < cmd->numVerts; ++i ) {
		tess.xyz[first + i][0] = verts[i].xyz[0];
		tess.xyz[first + i][1] = verts[i].xyz[1];
		tess.xyz[first + i][2] = 0;
		tess.texCoords[0][first + i][0] = verts[i].st[0];
		tess.texCoords[0][first + i][1] = verts[i].st[1];
		tess.vertexColors[first + i] = verts[i].modulate;
	}
	tess.numVertexes += cmd->numVerts;

	/* The points were stored inside this command, so the next one begins after
	 * the padded size the allocator handed out. */
	return (const void *)( (const byte *)cmd + cmd->commandBytes );
}

#endif
