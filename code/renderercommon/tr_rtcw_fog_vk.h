/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RTCW_FOG_VK_H
#define FNTECH3_TR_RTCW_FOG_VK_H
#include "tr_rtcw_fog_blend.h"

/* Both Vulkan backends use the same owned, per-draw fog data. Append this
 * block to the existing UBO so the other shader families keep their offsets. */
typedef char fnRTCWFogUniformOffsetCheck[
	offsetof( vkUniform_t, rtcwFogColor ) == 368 ? 1 : -1];

#ifdef FN_RTCW_FOG_VK_ALICE
/* American McGee's Alice's far plane fogs linearly from the eye to its
 * distance in its colour, the same draw ../renderer/tr_rtcw_fog_gl.h builds:
 * retail's far-plane fog pass sets alpha to min( |v - eye| / distance, 1 )
 * per vertex (alice.exe 0x0049ebb0, colour 0x0049eb40), so the distance is
 * radial. A sky is fogged only when the far plane culls (RB_BeginSurface
 * 0x004927d3..0x004927ff). Blended stages take RTCW's neutral colour below.
 * A renderer whose view parameters carry the far plane defines
 * FN_RTCW_FOG_VK_ALICE before including this. */
static fnRTCWFogDraw_t VK_AliceFarFogDraw( void ) {
	fnRTCWFogDraw_t draw;
	int channel;
	memset( &draw, 0, sizeof( draw ) );
	if ( !backEnd.viewParms.aliceFarFog || !( backEnd.viewParms.aliceFarDistance > 0.0f ) ||
		backEnd.projection2D || ( tess.shader && tess.shader->isSky && !backEnd.viewParms.aliceFarCull ) ) {
		return draw;
	}
	for ( channel = 0; channel < 3; ++channel ) {
		const float value = backEnd.viewParms.aliceFarColor[channel] * tr.identityLight;
		draw.color[channel] = value < 0 ? 0 : value > 1 ? 1 : value;
	}
	draw.color[3] = 1.0f;	/* linear */
	draw.params[0] = 0.0f;
	draw.params[1] = backEnd.viewParms.aliceFarDistance;
	draw.params[2] = 1.0f;
	draw.params[3] = 1.0f;	/* radial */
	return draw;
}
#endif

void VK_SetRTCWFogParams( vkUniform_t *data, const shaderStage_t *stage ) {
	fnRTCWFogDraw_t draw = FnRTCWFogDraw( &backEnd.refdef.rtcwFog,
		tess.shader && tess.shader->isSky ? qtrue : qfalse,
		backEnd.projection2D,
		tess.shader && tess.shader->rtcwNoFog ? qtrue : qfalse,
		stage && stage->rtcwFog ? qtrue : qfalse,
		ri.gameId == COM_GAME_RTCW_SP && backEnd.refdef.rtcw.snooperView ? qtrue : qfalse,
		qtrue );
	int row, column;
#ifdef FN_RTCW_FOG_VK_ALICE
	if ( ri.gameId == COM_GAME_ALICE ) {
		draw = VK_AliceFarFogDraw();
	}
#endif
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
