/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_CSM_CASTERS_H
#define FNTECH3_TR_CSM_CASTERS_H

/* Opaque geometry can share one cheap depth material. Cutouts and deforms
 * must retain their authored stage, UV animation, alpha and geometry. */
static shader_t *R_CSMCasterShader( shader_t *shader ) {
	int i;
	if ( shader->remappedShader ) shader = shader->remappedShader;
	if ( shader->numDeforms ) return shader;
	for ( i = 0; i < shader->numUnfoggedPasses; ++i ) {
		const shaderStage_t *stage = shader->stages[i];
		if ( stage && ( stage->stateBits & ( GLS_ATEST_BITS | GLS_ALICE_ATEST_BITS ) ) ) return shader;
	}
	return tr.defaultShader;
}

/* Material coverage can change without a model frame changing. Re-render
 * these casters instead of reusing an atlas with stale holes. */
static qboolean R_CSMCasterMaterialStatic( const shader_t *shader ) {
	int i, b;
	if ( shader->remappedShader ) shader = shader->remappedShader;
	if ( shader->numDeforms ) return qfalse;
	for ( i = 0; i < shader->numUnfoggedPasses; ++i ) {
		const shaderStage_t *stage = shader->stages[i];
		if ( !stage || !( stage->stateBits & ( GLS_ATEST_BITS | GLS_ALICE_ATEST_BITS ) ) ) continue;
#ifndef USE_VULKAN
		if ( stage->alphaGen != AGEN_IDENTITY && stage->alphaGen != AGEN_SKIP &&
			stage->alphaGen != AGEN_CONST && stage->alphaGen != AGEN_VERTEX ) return qfalse;
#endif
		for ( b = 0; b < NUM_TEXTURE_BUNDLES; ++b ) {
			const textureBundle_t *bundle = &stage->bundle[b];
			if ( bundle->numImageAnimations > 1 || bundle->numTexMods || bundle->isVideoMap ||
				bundle->tcGen == TCGEN_ENVIRONMENT_MAPPED ||
				bundle->tcGen == TCGEN_ENVIRONMENT_MAPPED_FP ||
				bundle->tcGen == TCGEN_MOHAA_ENVIRONMENT_MODEL || bundle->tcGen == TCGEN_FOG ) return qfalse;
#ifdef USE_VULKAN
			if ( bundle->alphaGen != AGEN_IDENTITY && bundle->alphaGen != AGEN_SKIP &&
				bundle->alphaGen != AGEN_CONST && bundle->alphaGen != AGEN_VERTEX ) return qfalse;
#endif
		}
	}
	return qtrue;
}

/* Project an AABB into camera depth without tessellating it. Works with
 * rotated/scaled entity axes and keeps a surface touching either split. */
static qboolean R_CSMBoundsOutsideSplit( const vec3_t mins, const vec3_t maxs,
	const orientationr_t *orientation, const orientationr_t *view, float splitNear, float splitFar ) {
	vec3_t delta;
	float lo, hi;
	int i;
	VectorSubtract( orientation->origin, view->origin, delta );
	lo = hi = DotProduct( delta, view->axis[0] );
	for ( i = 0; i < 3; ++i ) {
		const float scale = DotProduct( orientation->axis[i], view->axis[0] );
		const float a = scale * mins[i], b = scale * maxs[i];
		lo += MIN( a, b );
		hi += MAX( a, b );
	}
	return hi < splitNear - 1.0f || lo > splitFar + 1.0f;
}

#endif
