/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RITUAL_FAR_FOG_RENDER_H
#define FNTECH3_TR_RITUAL_FAR_FOG_RENDER_H
#include "tr_ritual_far_fog.h"

/* Whether the surface being drawn takes the far-fog pass (tr_ritual_far_fog.h):
 * an Alice or F.A.K.K.2 view with its far fog on, outside 2D, and a shader
 * whose first stage is opaque. Its stages then draw unfogged. */
static qboolean RB_RitualFarFogTakesPass( const shader_t *shader ) {
	const shaderStage_t *first;
	if ( !COM_GAME_ID_IS_RITUAL( ri.gameId ) || !shader || backEnd.projection2D ) {
		return qfalse;
	}
	if ( !backEnd.viewParms.aliceFarFog || !( backEnd.viewParms.aliceFarDistance > 0.0f ) ) {
		return qfalse;
	}
	first = shader->stages[0];
	return FnRitualFarFogShaderTakesPass( shader->isSky, first && first->active ? qtrue : qfalse,
		first ? first->stateBits : 0 );
}

/* Under the far fog retail draws no fog volume: RB_BeginSurface gives every
 * 3D surface but a sky the plane does not cut the far fog's own number in
 * place of its fog (fakk2.exe 1.02 0x0047c553..0x0047c583; alice.exe
 * 0x004927d3..0x004927ff), which it takes only when the far fog is on.
 * `over` and `under` put the camera inside a black fog volume that retail
 * shows only when `r_farplane_nofog` turns the far fog off. */
static int RB_RitualSurfaceFogNum( const shader_t *shader, int fogNum ) {
	if ( fogNum && shader && COM_GAME_ID_IS_RITUAL( ri.gameId ) && backEnd.viewParms.aliceFarFog &&
		!backEnd.projection2D && ( !shader->isSky || backEnd.viewParms.aliceFarCull ) ) {
		return 0;
	}
	return fogNum;
}

/* The pass's colours: the view's fog colour, as the per-stage fog takes it,
 * with each vertex's distance from the eye in its alpha. */
static void RB_RitualFarFogColors( byte *colors, float *texCoords ) {
	byte rgb[3];
	int i;
	for ( i = 0; i < 3; i++ ) {
		const float value = backEnd.viewParms.aliceFarColor[i] * tr.identityLight;
		rgb[i] = value <= 0.0f ? 0 : value >= 1.0f ? 255 : (byte)( value * 255.0f );
	}
	for ( i = 0; i < tess.numVertexes; i++, colors += 4, texCoords += 2 ) {
		vec3_t delta;
		VectorSubtract( tess.xyz[i], backEnd.or.viewOrigin, delta );
		colors[0] = rgb[0];
		colors[1] = rgb[1];
		colors[2] = rgb[2];
		colors[3] = FnRitualFarFogAlpha( VectorLength( delta ), backEnd.viewParms.aliceFarDistance );
		texCoords[0] = texCoords[1] = 0.0f;
	}
}

#ifdef USE_VULKAN
/* Both Vulkan backends draw it as the Quake III IHV fog pass is drawn
 * (tr_q3_ihv_fog_vk.h): vertex colours over the white image, in the depth
 * range the stages drew in, which its depth test must meet. The surface
 * must not come from a static VBO, which leaves tess.xyz empty. */
static void RB_RitualFarFogPass( qboolean rebindIndex ) {
	Vk_Pipeline_Def def;
	Com_Memset( &def, 0, sizeof( def ) );
	def.shader_type = TYPE_SIGNLE_TEXTURE;
	def.face_culling = tess.shader->cullType;
	def.polygon_offset = tess.shader->polygonOffset;
	def.state_bits = FnRitualFarFogState( tess.shader->stages[0]->stateBits );
	RB_RitualFarFogColors( tess.svars.colors[0][0].rgba, tess.svars.texcoords[0][0] );
	tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
	GL_SelectTexture( 0 );
	GL_Bind( tr.whiteImage );
	vk_bind_pipeline( vk_find_pipeline_ext( 0, &def, qtrue ) );
	if ( rebindIndex ) vk_bind_index();
	vk_bind_geometry( TESS_ST0 | TESS_RGBA0 );
	vk_draw_geometry( tess.depthRange, qtrue );
}
#endif
#endif
