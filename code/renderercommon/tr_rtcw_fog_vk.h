/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_VK_H
#define FNTECH3_TR_RTCW_FOG_VK_H
#include "tr_rtcw_fog_blend.h"

/* Both Vulkan backends use the same owned, per-draw fog data. Append this
 * block to the existing UBO so the other shader families keep their offsets. */
typedef char fnRTCWFogUniformOffsetCheck[
	offsetof( vkUniform_t, rtcwFogColor ) == 368 ? 1 : -1];

void VK_SetRTCWFogParams( vkUniform_t *data, const shaderStage_t *stage ) {
	fnRTCWFogDraw_t draw = FnRTCWFogDraw( &backEnd.refdef.rtcwFog,
		tess.shader && tess.shader->isSky ? qtrue : qfalse,
		backEnd.projection2D,
		tess.shader && tess.shader->rtcwNoFog ? qtrue : qfalse,
		stage && stage->rtcwFog ? qtrue : qfalse,
		ri.gameId == COM_GAME_RTCW_SP && backEnd.refdef.rtcw.snooperView ? qtrue : qfalse,
		qtrue );
	int row, column;
	FnRTCWFogBlendColor( &draw, stage ? stage->stateBits : 0 );
	Vector4Copy( draw.color, data->rtcwFogColor );
	Vector4Copy( draw.params, data->rtcwFogParams );
	/* Use the actual local-to-eye transform, including scaled models. Camera
 * distance in world space alone is wrong for entity-local vertices. */
	for ( row = 0; row < 3; ++row ) {
		for ( column = 0; column < 4; ++column ) {
			data->rtcwFogEye[row][column] = backEnd.or.modelMatrix[column * 4 + row];
		}
	}
}
#endif
