/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_2D_POLY_CMD_H
#define FNTECH3_TR_WOLFET_2D_POLY_CMD_H

/* Enemy Territory's only variable-shape 2D submission: a screen-space triangle
 * fan of textured, per-vertex-coloured points. Included by each renderer's
 * command file, where the command buffer allocator is in scope.
 *
 * The points travel inside the command itself, so a fan cannot outlive the
 * frame that submitted it and no module memory is retained. ET's own renderer
 * silently drops a batch that will not fit its vertex pool; a batch that will
 * not fit this command buffer is dropped the same way, because a half-drawn fan
 * is a different picture from no fan at all. */
#define FNTECH3_WOLFET_MAX_2D_POLY_VERTS 1024

void RE_AddWolfET2DPolys( const polyVert_t *verts, int numVerts, qhandle_t hShader ) {
	wolfet2DPolyCommand_t *cmd;
	polyVert_t *stored;
	int i, bytes;
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET 2D polygons submitted outside their game profile" );
		return;
	}
	if ( !tr.registered || !verts ) return;
	/* A fan needs three points, and nothing beyond the recorded limit. */
	if ( numVerts < 3 || numVerts > FNTECH3_WOLFET_MAX_2D_POLY_VERTS ) return;
	for ( i = 0; i < numVerts; ++i ) {
		if ( !isfinite( verts[i].xyz[0] ) || !isfinite( verts[i].xyz[1] ) ||
			!isfinite( verts[i].st[0] ) || !isfinite( verts[i].st[1] ) ) {
			ri.Error( ERR_DROP, "RE_AddWolfET2DPolys: non-finite vertex" );
			return;
		}
	}
	bytes = PAD( (int)sizeof( *cmd ) + numVerts * (int)sizeof( polyVert_t ), (int)sizeof( void * ) );
	cmd = (wolfet2DPolyCommand_t *)R_GetCommandBuffer( bytes );
	if ( !cmd ) return;
	cmd->commandId = RC_WOLFET_2D_POLYS;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->numVerts = numVerts;
	cmd->commandBytes = bytes;
	stored = (polyVert_t *)( cmd + 1 );
	Com_Memcpy( stored, verts, sizeof( polyVert_t ) * (size_t)numVerts );
}

#endif
