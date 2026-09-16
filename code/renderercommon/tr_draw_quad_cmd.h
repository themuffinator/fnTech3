/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_DRAW_QUAD_CMD_H
#define FNTECH3_TR_DRAW_QUAD_CMD_H

// Included by each renderer after its command allocator and RE_StretchPic.
void RE_DrawQuad( const refDrawQuad_t *quad ) {
	drawQuadCommand_t *cmd;
	int i, j;
	if ( !tr.registered || !quad ) return;
	if ( quad->colorMask & ~15u ) {
		ri.Error( ERR_DROP, "RE_DrawQuad: invalid color mask" );
	}
	for ( i = 0; i < 4; ++i ) {
		for ( j = 0; j < 2; ++j ) {
			if ( !isfinite( quad->xy[i][j] ) || !isfinite( quad->st[i][j] ) ) {
				ri.Error( ERR_DROP, "RE_DrawQuad: non-finite vertex" );
			}
		}
	}
	cmd = (drawQuadCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) return;
	Com_Memset( &cmd->base, 0, sizeof( cmd->base ) );
	cmd->base.commandId = RC_DRAW_QUAD;
	cmd->base.shader = R_GetShaderByHandle( quad->shader );
	cmd->quad = *quad;
}

#endif
