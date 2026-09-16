/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* RTCW projected lights use authored textures/blends and repeated passes;
 * material color/UV generators and normal-facing rejection do not apply. */
#ifndef FNTECH3_RTCW_LIGHT_BACKEND_H
#define FNTECH3_RTCW_LIGHT_BACKEND_H
#include "tr_rtcw_light_math.h"
#if !defined( USE_VULKAN ) && !defined( FNTECH3_RTCW_LIGHT_FLOAT_COLORS )
static byte rtcwLightColors[SHADER_MAX_VERTEXES][4];
#endif

static void RB_RTCWLightPass( const shaderStage_t *stage, int count, const glIndex_t *indexes ) {
	const unsigned state = ( stage ? stage->stateBits : GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE ) | GLS_DEPTHFUNC_EQUAL;
#ifdef USE_VULKAN
	Vk_Pipeline_Def definition;
	uint32_t pipeline;
	vk_get_pipeline_def( vk.dlight_pipelines[0][tess.shader->cullType][tess.shader->polygonOffset], &definition );
	definition.state_bits = state;
	pipeline = vk_find_pipeline_ext( 0, &definition, qfalse );
#ifdef FNTECH3_RTCW_LIGHT_RTX
	GL_SelectTexture( 0 );
	if ( stage ) R_BindAnimatedImage( &stage->bundle[0] );
	else GL_Bind( tr.dlightImage );
#else
	{
		vk_material_t material;
		vk_material_init( &material );
		vk_material_set_descriptor( &material, VK_DESC_TEXTURE0,
			stage ? R_AnimatedImageDescriptor( &stage->bundle[0] ) : R_ImageDescriptor( tr.dlightImage ) );
		vk_bind_material( &material );
	}
#endif
	vk_bind_pipeline( pipeline );
	vk_bind_index_ext( count, indexes );
	vk_bind_geometry( TESS_RGBA0 | TESS_ST0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
#else
	GL_SelectTexture( 0 );
	if ( stage ) R_BindAnimatedImage( &stage->bundle[0] );
	else GL_Bind( tr.dlightImage );
	GL_State( state );
	GL_ClientState( 1, CLS_NONE );
	GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );
#ifdef FNTECH3_RTCW_LIGHT_FLOAT_COLORS
	qglColorPointer( 4, GL_FLOAT, 0, tess.dlightColors );
#else
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, rtcwLightColors );
#endif
	R_DrawElements( count, indexes );
#endif
	backEnd.pc.c_totalIndexes += count;
	backEnd.pc.c_dlightIndexes += count;
}

/* Return whether the original index buffer must be rebound for a later pass. */
static qboolean RB_RTCWProjectLights( void ) {
	byte clips[SHADER_MAX_VERTEXES];
	glIndex_t indexes[SHADER_MAX_INDEXES];
	qboolean rebind = qfalse;
	int l, vertex, triangle;
	if ( tess.numVertexes < 0 || tess.numVertexes > SHADER_MAX_VERTEXES ||
		tess.numIndexes < 0 || tess.numIndexes > SHADER_MAX_INDEXES ) return qfalse;
	if ( backEnd.refdef.rtcw.snooperView ) return qfalse;
	for ( l = 0; l < backEnd.refdef.num_dlights && l < 32; ++l ) {
		const dlight_t *light = &backEnd.refdef.dlights[l];
		int count = 0, pass;
		if ( !( (uint32_t)tess.dlightBits & (UINT32_C(1) << l) ) ) continue;
		for ( vertex = 0; vertex < tess.numVertexes; ++vertex ) {
			const rtcwLightVertex_t projected = RTCW_ProjectLightVertex( light->transformed,
				light->radius, light->color, tess.xyz[vertex] );
			clips[vertex] = projected.clip;
			tess.svars.texcoords[0][vertex][0] = projected.st[0];
			tess.svars.texcoords[0][vertex][1] = projected.st[1];
#ifdef USE_VULKAN
			Com_Memcpy( tess.svars.colors[0][vertex].rgba, projected.color, 4 );
#elif defined( FNTECH3_RTCW_LIGHT_FLOAT_COLORS )
			for ( pass = 0; pass < 4; ++pass ) tess.dlightColors[vertex][pass] = projected.color[pass] / 255.0f;
#else
			Com_Memcpy( rtcwLightColors[vertex], projected.color, 4 );
#endif
			++backEnd.pc.c_dlightVertexes;
		}
		tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
		for ( triangle = 0; triangle + 2 < tess.numIndexes; triangle += 3 ) {
			const glIndex_t a = tess.indexes[triangle], b = tess.indexes[triangle+1], c = tess.indexes[triangle+2];
			if ( a >= (unsigned)tess.numVertexes || b >= (unsigned)tess.numVertexes || c >= (unsigned)tess.numVertexes ) continue;
			if ( (clips[a] | clips[b] | clips[c]) & 128 ) continue;
			if ( clips[a] & clips[b] & clips[c] ) continue;
			indexes[count++] = a; indexes[count++] = b; indexes[count++] = c;
		}
		if ( !count ) continue;
		if ( count != tess.numIndexes ) rebind = qtrue;
		if ( light->rtcwShader ) {
			for ( pass = 0; pass < light->rtcwShader->numUnfoggedPasses; ++pass )
				RB_RTCWLightPass( light->rtcwShader->stages[pass], count, indexes );
		} else {
			for ( pass = 0; pass <= light->rtcwOverdraw; ++pass ) RB_RTCWLightPass( NULL, count, indexes );
		}
	}
	return rebind;
}
#endif
