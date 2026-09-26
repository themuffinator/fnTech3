/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_SCISSOR_CMD_H
#define FNTECH3_TR_STVEF_SCISSOR_CMD_H

void RE_SetSTVEFScissor( float x, float y, float w, float h ) {
	stvefScissorCommand_t *cmd;
	fnScissorRect_t rect;
	if ( !tr.registered ) return;
	if ( !FN_STVEFScissorRect( x, y, w, h, glConfig.vidWidth, glConfig.vidHeight, &rect ) ) {
		ri.Error( ERR_DROP, "Invalid Elite Force scissor rectangle" );
		return;
	}
	cmd = (stvefScissorCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) return;
	cmd->commandId = RC_STVEF_SCISSOR;
	cmd->rect = rect;
}

#endif
