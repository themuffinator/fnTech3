/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_FOG_VK_H
#define FNTECH3_TR_Q3_IHV_FOG_VK_H
#ifdef USE_VULKAN
/* Both Vulkan backends draw the observed vertex density directly. The retail
 * fog-only/collapsed pipelines evaluate a different function in the shader. */
static void RB_Q3IHVFogPass( qboolean rebindIndex ) {
	Vk_Pipeline_Def def;
	Com_Memset( &def, 0, sizeof( def ) );
	def.shader_type = TYPE_SIGNLE_TEXTURE;
	def.face_culling = tess.shader->cullType;
	def.polygon_offset = tess.shader->polygonOffset;
	def.state_bits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	if ( tess.shader->fogPass == FP_EQUAL ) def.state_bits |= GLS_DEPTHFUNC_EQUAL;
	RB_Q3IHVFogColors( tess.svars.colors[0][0].rgba, tess.svars.texcoords[0][0] );
	tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
	GL_SelectTexture( 0 );
	GL_Bind( tr.whiteImage );
	vk_bind_pipeline( vk_find_pipeline_ext( 0, &def, qtrue ) );
	if ( rebindIndex ) vk_bind_index();
	vk_bind_geometry( TESS_ST0 | TESS_RGBA0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
}
#endif
#endif
