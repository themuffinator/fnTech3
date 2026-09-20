/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_STVEF_SCISSOR_BACKEND_H
#define FNTECH3_TR_STVEF_SCISSOR_BACKEND_H

static const void *RB_SetSTVEFScissor( const void *data ) {
	const stvefScissorCommand_t *cmd = (const stvefScissorCommand_t *)data;
	/* Pictures preceding this command must keep their previous rectangle. */
	if ( tess.numIndexes ) RB_EndSurface();
	if ( !backEnd.projection2D ) RB_SetGL2D();
	backEnd.scissor2D = cmd->rect;
	backEnd.scissor2DSet = qtrue;
#ifdef USE_VULKAN
	/* The next draw must re-emit the dynamic scissor even at the same depth. */
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
#else
	qglScissor( cmd->rect.x, glConfig.vidHeight - cmd->rect.y - cmd->rect.height,
		cmd->rect.width, cmd->rect.height );
#endif
	return (const void *)( cmd + 1 );
}

#endif
