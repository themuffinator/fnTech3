/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Path tracer materials

One GPU material per shader_t (after remapping), holding the shader's stages
as the parser left them (rt_stage_source.h). Classification decides which
surfaces the path tracer draws:

- traced: the first stage writes depth without blending (opaque), or a stage
  alpha-tests (masked); the trace shades these pixels;
- sky and portal: kept in the acceleration structure so primary rays stop at
  them, but the raster pass keeps those pixels (skybox, portal view);
- everything else (blended, additive, fog, polygon-offset decals, deformed
  geometry) stays with the raster overlay pass.

The stages keep their blends; rt_material.glsl evaluates them with the
lighting term (the $lightmap stage, the baked vertex light of a vertex-lit
world surface, a model's lightingDiffuse) at one and at zero, which splits
the drawn colour into reflectance and emission. An additive stage after the
lighting term with an environment-mapped coordinate generator is an authored
fake reflection, which traced specular replaces.

Included by vk.c.
===========================================================================
*/

typedef enum {
	RT_OWNER_RASTER,		// drawn by the overlay pass after the trace
	RT_OWNER_TRACED,		// opaque or masked; the trace owns its pixels
	RT_OWNER_BASE_RASTER	// sky and portals: drawn before the trace and kept
} rtOwner_t;

typedef struct {
	const shader_t	*shader;
	rtOwner_t		owner;
	qboolean		masked;
} rtMaterialInfo_t;

typedef struct {
	const shader_t		*defaultShader;	// identifies the renderer instance
	rtGpuMaterial_t		*materials;
	rtMaterialInfo_t	*info;
	uint32_t			numMaterials;
	uint32_t			maxMaterials;
	rtGpuStage_t		*stages;
	uint32_t			numStages;
	uint32_t			maxStages;
	int32_t				*shaderMaterial;	// by shader->index: material + 1, 0 if none
	uint32_t			generation;			// increments whenever the table grows
} rtMaterialTable_t;

static rtMaterialTable_t s_rtMaterials;

static void RT_MaterialsFree( void )
{
	if ( s_rtMaterials.materials ) {
		RT_HostFree( s_rtMaterials.materials );
	}
	if ( s_rtMaterials.info ) {
		RT_HostFree( s_rtMaterials.info );
	}
	if ( s_rtMaterials.stages ) {
		RT_HostFree( s_rtMaterials.stages );
	}
	if ( s_rtMaterials.shaderMaterial ) {
		RT_HostFree( s_rtMaterials.shaderMaterial );
	}
	Com_Memset( &s_rtMaterials, 0, sizeof( s_rtMaterials ) );
}

static int32_t RT_ImageIndex( const image_t *image )
{
	int i;

	if ( !image ) {
		return -1;
	}
	for ( i = 0; i < tr.numImages && i < RT_MAX_TEXTURES; i++ ) {
		if ( tr.images[i] == image ) {
			return i;
		}
	}
	return -1;
}

static qboolean RT_GrowArray( void **array, uint32_t *capacity, uint32_t required, size_t elementSize, uint32_t initial )
{
	uint32_t newCapacity;
	void *grown;

	if ( required <= *capacity ) {
		return qtrue;
	}
	newCapacity = *capacity ? *capacity : initial;
	while ( newCapacity < required ) {
		if ( newCapacity > ( 1u << 24 ) ) {
			return qfalse;
		}
		newCapacity *= 2u;
	}
	if ( (size_t)newCapacity * elementSize > (size_t)INT_MAX ) {
		return qfalse;
	}
	grown = RT_HostAlloc( (int)( (size_t)newCapacity * elementSize ) );
	if ( !grown ) {
		return qfalse;
	}
	Com_Memset( grown, 0, (size_t)newCapacity * elementSize );
	if ( *array ) {
		Com_Memcpy( grown, *array, (size_t)*capacity * elementSize );
		RT_HostFree( *array );
	}
	*array = grown;
	*capacity = newCapacity;
	return qtrue;
}

static qboolean RT_MaterialsBegin( void )
{
	if ( s_rtMaterials.defaultShader == tr.defaultShader && s_rtMaterials.shaderMaterial ) {
		return qtrue;
	}
	RT_MaterialsFree();
	s_rtMaterials.shaderMaterial = (int32_t *)RT_HostAlloc( MAX_SHADERS * sizeof( int32_t ) );
	if ( !s_rtMaterials.shaderMaterial ) {
		return qfalse;
	}
	Com_Memset( s_rtMaterials.shaderMaterial, 0, MAX_SHADERS * sizeof( int32_t ) );
	s_rtMaterials.defaultShader = tr.defaultShader;
	return qtrue;
}

static qboolean RT_StageIsLightmap( const rtStageSource_t *s )
{
	return ( s->lightmap != LIGHTMAP_INDEX_NONE || s->tcGen == TCGEN_LIGHTMAP ||
		( s->images[0] && ( s->images[0]->flags & IMGFLAG_LIGHTMAP ) ) ) ? qtrue : qfalse;
}

static qboolean RT_StageVertexIsLight( const shader_t *shader, const rtStageSource_t *s )
{
	if ( s->rgbGen != CGEN_VERTEX && s->rgbGen != CGEN_EXACT_VERTEX ) {
		return qfalse;
	}
	// BSP vertex colours of a vertex-lit surface are q3map's baked light
	return ( s->vertexLightmap || shader->lightmapIndex == LIGHTMAP_BY_VERTEX ) ? qtrue : qfalse;
}

static uint32_t RT_WaveFunc( genFunc_t func )
{
	return ( func >= GF_NONE && func <= GF_NOISE ) ? (uint32_t)func : (uint32_t)GF_SIN;
}

static void RT_PackWave( float out[4], const waveForm_t *wave )
{
	out[0] = wave->base;
	out[1] = wave->amplitude;
	out[2] = wave->phase;
	out[3] = wave->frequency;
}

static uint32_t RT_PackTexMods( const rtStageSource_t *s, rtGpuStage_t *gpu )
{
	uint32_t count = 0;
	int i;

	for ( i = 0; i < s->numTexMods && count < RT_GPU_MAX_TEXMODS; i++ ) {
		const texModInfo_t *tm = &s->texMods[i];
		float *a = gpu->texMod[count * 2 + 0];
		float *b = gpu->texMod[count * 2 + 1];

		Com_Memset( a, 0, sizeof( float ) * 4 );
		Com_Memset( b, 0, sizeof( float ) * 4 );
		switch ( tm->type ) {
		case TMOD_TRANSFORM:
			a[1] = tm->matrix[0][0];
			a[2] = tm->matrix[0][1];
			a[3] = tm->matrix[1][0];
			b[0] = tm->matrix[1][1];
			b[1] = tm->translate[0];
			b[2] = tm->translate[1];
			break;
		case TMOD_TURBULENT:
		case TMOD_STRETCH:
			a[1] = tm->wave.base;
			a[2] = tm->wave.amplitude;
			a[3] = tm->wave.phase;
			b[0] = tm->wave.frequency;
			b[3] = (float)RT_WaveFunc( tm->wave.func );
			break;
		case TMOD_SCROLL:
			a[1] = tm->scroll[0];
			a[2] = tm->scroll[1];
			break;
		case TMOD_SCALE:
		case TMOD_OFFSET:
		case TMOD_SCALE_OFFSET:
		case TMOD_OFFSET_SCALE:
			a[1] = tm->scale[0];
			a[2] = tm->scale[1];
			b[0] = tm->offset[0];
			b[1] = tm->offset[1];
			break;
		case TMOD_ROTATE:
			a[1] = tm->rotateSpeed;
			break;
		case TMOD_ENTITY_TRANSLATE:
			break;
		default:
			// Alice and other dialects' modifiers are not traced
			continue;
		}
		a[0] = (float)tm->type;
		count++;
	}
	return count;
}

static unsigned RT_StageBlend( const rtStageSource_t *s )
{
	return s->stateBits & GLS_BLEND_BITS;
}

// The stage's blend adds to what is under it (dst factor ONE).
static qboolean RT_StageAdds( const rtStageSource_t *s )
{
	return ( ( s->stateBits & GLS_DSTBLEND_BITS ) == GLS_DSTBLEND_ONE ) ? qtrue : qfalse;
}

// Sky by its skyparms or surfaceparm, or as the loaded map's compiler saw it
static qboolean RT_ShaderIsSky( const shader_t *shader )
{
	return ( shader->isSky || ( shader->surfaceFlags & SURF_SKY ) || shader->rtCompiledSky ) ? qtrue : qfalse;
}

/*
==================
Emitters

rtx_rt_lighting 0 and 3 light a scene from the sky and from the surfaces
that give off light, as Q2RTX does, with nothing baked; 0 adds the light
entities the map keeps. A surface gives off what its shader draws where the
lighting term is black. rt_material.glsl splits
every hit that way, texel by texel; the light builder needs one colour per
shader, so its stages are folded here as the GPU folds them, each with its
image's average colour and alpha, once with the lighting term at zero
(RT_ShaderEmission). A glow stage added over a lightmapped base, a
fullbright surface and an additive flame keep a colour there; a lit surface
keeps none.
==================
*/

#define RT_LIGHTING_LEVEL		0	// the level's own lights and its emitting surfaces
#define RT_LIGHTING_LIGHTMAPS	1	// the lightmaps, with traced dynamic lights
#define RT_LIGHTING_COMPILERS	2	// the map compilers' own lights and models
#define RT_LIGHTING_EMITTERS	3	// the sky and emitting surfaces alone (Q2RTX)
#define RT_EMISSION_MIN			0.03f	// dimmer shaders are not emitters

static int RT_LightingMode( void )
{
	return rtx_rt_lighting ? (int)Com_Clamp( 0, 3, rtx_rt_lighting->integer ) : RT_LIGHTING_LEVEL;
}

// Whether a mode lights with the sky and the surfaces that give off light:
// the level's own lights, and the emitters alone
static qboolean RT_LightingEmits( int mode )
{
	return ( mode == RT_LIGHTING_LEVEL || mode == RT_LIGHTING_EMITTERS ) ? qtrue : qfalse;
}

#define RT_CAMERA_UNITS		0	// the traced light as it is, in the map compiler's light units
#define RT_CAMERA_LINEAR	1	// linear light, automatic exposure and the tone mapper (Q2RTX)

// How a mode's light is shown (rtx_rt_camera). The level's own lights are
// in their map compiler's units, the units its designer set them in: by
// default the traced light a surface receives multiplies its colour as the
// engine multiplies a surface by its light, with no exposure, so a map is as
// bright or as dark as its own light sources make it; they can instead take
// the linear camera. The sky and emitters alone have no compiler's units to
// keep: they always take it. The lightmaps and the compilers' models, kept
// for comparison, are shown in their units.
static int RT_LightingCamera( int mode )
{
	if ( mode == RT_LIGHTING_EMITTERS ) {
		return RT_CAMERA_LINEAR;
	}
	if ( mode != RT_LIGHTING_LEVEL ) {
		return RT_CAMERA_UNITS;
	}
	return ( rtx_rt_camera && rtx_rt_camera->integer ) ? RT_CAMERA_LINEAR : RT_CAMERA_UNITS;
}

#define RT_IMAGE_AVERAGES	1024

typedef struct {
	const image_t	*image;
	float			rgba[4];
	float			brightScale[3];	// the colour scale the bright average was taken at
	float			bright[3];		// RT_ImageBright
	qboolean		brightValid;
} rtImageAverage_t;

static rtImageAverage_t s_rtImageAverages[RT_IMAGE_AVERAGES];
static int s_rtImageAverageCount;

// The averages belong to one set of images; a renderer restart makes new ones
static void RT_ResetImageAverages( void )
{
	s_rtImageAverageCount = 0;
}

// An image's average colour and alpha, loaded from its file once. The white
// images of shaders that colour them with rgbGen are white; a generated one
// (the default checkerboard, a dynamic image) shows no light of its own, and
// one whose file cannot be read counts as black.
static void RT_ImageAverage( const image_t *image, float rgba[4] )
{
	static const float unit[3] = { 1.0f, 1.0f, 1.0f };
	int i;

	rgba[0] = rgba[1] = rgba[2] = 0.0f;
	rgba[3] = 1.0f;
	if ( !image ) {
		return;
	}
	if ( image == tr.whiteImage || image == tr.identityLightImage ) {
		const float v = image == tr.whiteImage ? 1.0f : tr.identityLight;
		rgba[0] = rgba[1] = rgba[2] = v;
		return;
	}
	for ( i = 0; i < s_rtImageAverageCount; i++ ) {
		if ( s_rtImageAverages[i].image == image ) {
			Com_Memcpy( rgba, s_rtImageAverages[i].rgba, sizeof( s_rtImageAverages[i].rgba ) );
			return;
		}
	}
	if ( image->imgName && image->imgName[0] && image->imgName[0] != '*' && image->imgName[0] != '$' ) {
		R_ImageSums( image->imgName, unit, NULL, rgba, NULL );
	}
	if ( s_rtImageAverageCount < RT_IMAGE_AVERAGES ) {
		s_rtImageAverages[s_rtImageAverageCount].image = image;
		Com_Memcpy( s_rtImageAverages[s_rtImageAverageCount].rgba, rgba, sizeof( s_rtImageAverages[0].rgba ) );
		s_rtImageAverages[s_rtImageAverageCount].brightValid = qfalse;
		s_rtImageAverageCount++;
	}
}

// rt_common.glsl rt_bright_texel: how much of a texel drawn fullbright glows
static float RT_BrightTexel( float r, float g, float b )
{
	const float m = MAX( r, MAX( g, b ) );
	const float x = Com_Clamp( 0.0f, 1.0f, ( m - 0.55f ) / ( 0.85f - 0.55f ) );
	return x * x * ( 3.0f - 2.0f * x );
}

// The part of an image, drawn at `scale`, that glows: each pixel's colour
// times RT_BrightTexel of it, averaged. Kept for the last scale asked.
static void RT_ImageBright( const image_t *image, const float scale[3], float out[3] )
{
	rtImageAverage_t *entry = NULL;
	float rgba[4];
	int i;

	VectorClear( out );
	if ( !image ) {
		return;
	}
	if ( image == tr.whiteImage || image == tr.identityLightImage ) {
		const float v = image == tr.whiteImage ? 1.0f : tr.identityLight;
		const float glow = RT_BrightTexel( v * scale[0], v * scale[1], v * scale[2] );
		out[0] = v * scale[0] * glow;
		out[1] = v * scale[1] * glow;
		out[2] = v * scale[2] * glow;
		return;
	}
	RT_ImageAverage( image, rgba );	// finds or makes the cache entry
	for ( i = 0; i < s_rtImageAverageCount; i++ ) {
		if ( s_rtImageAverages[i].image == image ) {
			entry = &s_rtImageAverages[i];
			break;
		}
	}
	if ( entry && entry->brightValid && VectorCompare( entry->brightScale, scale ) ) {
		VectorCopy( entry->bright, out );
		return;
	}
	if ( image->imgName && image->imgName[0] && image->imgName[0] != '*' && image->imgName[0] != '$' ) {
		R_ImageSums( image->imgName, scale, RT_BrightTexel, rgba, out );
	}
	if ( entry ) {
		VectorCopy( scale, entry->brightScale );
		VectorCopy( out, entry->bright );
		entry->brightValid = qtrue;
	}
}

// A waveform's mean over its period: a sawtooth's half way up, the rest
// centred on their base
static float RT_WaveMean( const waveForm_t *wave )
{
	float mean = wave->base;
	if ( wave->func == GF_SAWTOOTH || wave->func == GF_INVERSE_SAWTOOTH ) {
		mean += 0.5f * wave->amplitude;
	}
	return Com_Clamp( 0.0f, 1.0f, mean );
}

// A stage's average colour and alpha with the lighting term at `light`.
// Colour generators that carry the light (a lightmap, a vertex-lit surface's
// baked colours, a model's diffuse light) scale with it. `white` stands every
// image in as white: a stack that shows nothing at zero light that way shows
// nothing with any images, and needs none loaded.
static qboolean RT_StageAverage( const shader_t *shader, const rtStageSource_t *s, float light,
	qboolean white, float rgba[4] )
{
	float image[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float scale;
	int n;

	if ( s->isDetail || s->isScreenMap || s->tcGen == TCGEN_FOG || s->rgbGen == CGEN_FOG ||
		s->alphaGen == AGEN_LIGHTING_SPECULAR ) {
		return qfalse;
	}
	if ( RT_StageIsLightmap( s ) ) {
		rgba[0] = rgba[1] = rgba[2] = light;
		rgba[3] = 1.0f;
		return qtrue;
	}
	if ( !white ) {
		if ( s->isVideoMap ) {
			image[0] = image[1] = image[2] = 0.5f;
		} else {
			// an animMap's frames, averaged
			const int frames = MAX( 1, MIN( s->numImages, RT_STAGE_MAX_ANIMATIONS ) );
			float frame[4];
			image[0] = image[1] = image[2] = image[3] = 0.0f;
			for ( n = 0; n < frames; n++ ) {
				RT_ImageAverage( s->images[n], frame );
				image[0] += frame[0] / frames;
				image[1] += frame[1] / frames;
				image[2] += frame[2] / frames;
				image[3] += frame[3] / frames;
			}
		}
	}

	switch ( s->rgbGen ) {
	case CGEN_IDENTITY_LIGHTING:
		scale = tr.identityLight;
		VectorScale( image, scale, rgba );
		break;
	case CGEN_CONST:
		rgba[0] = image[0] * s->constantColor.rgba[0] / 255.0f;
		rgba[1] = image[1] * s->constantColor.rgba[1] / 255.0f;
		rgba[2] = image[2] * s->constantColor.rgba[2] / 255.0f;
		break;
	case CGEN_WAVEFORM:
		scale = RT_WaveMean( &s->rgbWave );
		VectorScale( image, scale, rgba );
		break;
	case CGEN_EXACT_VERTEX:
	case CGEN_VERTEX:
	case CGEN_ONE_MINUS_VERTEX:
	case CGEN_LIGHTING_DIFFUSE:
		// the light the surface's colours or the model's grid hold
		VectorScale( image, light, rgba );
		break;
	default:
		VectorCopy( image, rgba );
		break;
	}

	switch ( s->alphaGen ) {
	case AGEN_CONST:
		rgba[3] = image[3] * s->constantColor.rgba[3] / 255.0f;
		break;
	case AGEN_WAVEFORM:
		rgba[3] = image[3] * RT_WaveMean( &s->alphaWave );
		break;
	case AGEN_VERTEX:
	case AGEN_ONE_MINUS_VERTEX:
		rgba[3] = image[3] * 0.5f;
		break;
	default:
		rgba[3] = image[3];
		break;
	}
	(void)shader;
	return qtrue;
}

// One blend factor, as its framebuffer blend applies it to colour and alpha
static void RT_BlendFactor( unsigned factor, qboolean source, const float src[4], const float dst[4], float out[4] )
{
	int c;
	for ( c = 0; c < 4; c++ ) {
		float f = 1.0f;
		switch ( factor ) {
		case GLS_SRCBLEND_ZERO:				f = 0.0f; break;
		case GLS_SRCBLEND_ONE:				f = 1.0f; break;
		case GLS_SRCBLEND_DST_COLOR:		f = dst[c]; break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:	f = 1.0f - dst[c]; break;
		case GLS_SRCBLEND_SRC_ALPHA:		f = src[3]; break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	f = 1.0f - src[3]; break;
		case GLS_SRCBLEND_DST_ALPHA:		f = dst[3]; break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:	f = 1.0f - dst[3]; break;
		case GLS_SRCBLEND_ALPHA_SATURATE:	f = c < 3 ? MIN( src[3], 1.0f - dst[3] ) : 1.0f; break;
		default:							f = source ? 1.0f : 0.0f; break;
		}
		out[c] = f;
	}
}

static void RT_BlendAverage( unsigned stateBits, const float src[4], float dst[4] )
{
	const unsigned blend = stateBits & GLS_BLEND_BITS;
	float sf[4], df[4];
	unsigned dstFactor;
	int c;

	if ( !blend ) {
		Com_Memcpy( dst, src, sizeof( float ) * 4 );
		return;
	}
	RT_BlendFactor( blend & GLS_SRCBLEND_BITS, qtrue, src, dst, sf );
	// the destination factors share the source factors' numbering, shifted
	dstFactor = ( blend & GLS_DSTBLEND_BITS ) >> 4;
	switch ( blend & GLS_DSTBLEND_BITS ) {
	case GLS_DSTBLEND_SRC_COLOR:
		for ( c = 0; c < 4; c++ ) df[c] = src[c];
		break;
	case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
		for ( c = 0; c < 4; c++ ) df[c] = 1.0f - src[c];
		break;
	default:
		RT_BlendFactor( dstFactor, qfalse, src, dst, df );
		break;
	}
	for ( c = 0; c < 4; c++ ) {
		dst[c] = MAX( 0.0f, src[c] * sf[c] + dst[c] * df[c] );
	}
}

// The average colour a shader's stages draw with the lighting term at `light`
static void RT_FoldStages( const shader_t *shader, float light, qboolean white, float out[4] )
{
	int lightingStage = -1;
	int i;

	out[0] = out[1] = out[2] = out[3] = 0.0f;
	for ( i = 0; i < shader->rtNumStages; i++ ) {
		const rtStageSource_t *s = &shader->rtStages[i];
		if ( RT_StageIsLightmap( s ) || RT_StageVertexIsLight( shader, s ) || s->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			lightingStage = i;
			break;
		}
	}
	for ( i = 0; i < shader->rtNumStages; i++ ) {
		const rtStageSource_t *s = &shader->rtStages[i];
		float src[4];
		// an authored fake reflection, which traced specular replaces
		if ( lightingStage >= 0 && i > lightingStage && RT_StageAdds( s ) &&
			( s->tcGen == TCGEN_ENVIRONMENT_MAPPED || s->tcGen == TCGEN_ENVIRONMENT_MAPPED_FP ) ) {
			continue;
		}
		if ( !RT_StageAverage( shader, s, light, white, src ) ) {
			continue;
		}
		RT_BlendAverage( s->stateBits, src, out );
	}
}

/*
A shader the raster pass draws fullbright - no lightmap, no vertex light, no
model light - that the scene lights all the same where the sky and emitters
light it (RT_MF_FULLBRIGHT_LIT): as Quake II's surfaces are lit whatever they
show, its bright texels alone glow. A q3map_surfacelight shader stays as it
was authored: it is a light, and shows as one. Only a surface the trace
draws takes it; sky and portals keep raster's pixels.
*/
static qboolean RT_ShaderFullbrightLit( const shader_t *shader )
{
	int i;

	if ( !RT_LightingEmits( RT_LightingMode() ) || !shader || shader->rtNumStages <= 0 || !shader->rtStages ||
		RT_ShaderIsSky( shader ) || shader->sort == SS_PORTAL || ( shader->surfaceLightValid && shader->surfaceLight > 0.0f ) ) {
		return qfalse;
	}
	for ( i = 0; i < shader->rtNumStages; i++ ) {
		const rtStageSource_t *s = &shader->rtStages[i];
		if ( RT_StageIsLightmap( s ) || RT_StageVertexIsLight( shader, s ) || s->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			return qfalse;
		}
	}
	return qtrue;
}

// The colour a stage's generator scales its image by, as RT_StageAverage
// takes it with the light at one
static void RT_StageScale( const rtStageSource_t *s, float scale[3] )
{
	float v = 1.0f;
	switch ( s->rgbGen ) {
	case CGEN_IDENTITY_LIGHTING:
		v = tr.identityLight;
		break;
	case CGEN_CONST:
		scale[0] = s->constantColor.rgba[0] / 255.0f;
		scale[1] = s->constantColor.rgba[1] / 255.0f;
		scale[2] = s->constantColor.rgba[2] / 255.0f;
		return;
	case CGEN_WAVEFORM:
		v = RT_WaveMean( &s->rgbWave );
		break;
	default:
		break;
	}
	scale[0] = scale[1] = scale[2] = v;
}

// What a fullbright-lit shader glows with: its base stage's bright texels,
// and whatever its later stages add on top of it
static void RT_FullbrightGlow( const shader_t *shader, vec3_t glow )
{
	qboolean base = qfalse;
	int i;

	VectorClear( glow );
	for ( i = 0; i < shader->rtNumStages; i++ ) {
		const rtStageSource_t *s = &shader->rtStages[i];
		float scale[3], part[4];
		if ( s->isDetail || s->isScreenMap || s->tcGen == TCGEN_FOG || s->rgbGen == CGEN_FOG ||
			s->alphaGen == AGEN_LIGHTING_SPECULAR ||
			s->tcGen == TCGEN_ENVIRONMENT_MAPPED || s->tcGen == TCGEN_ENVIRONMENT_MAPPED_FP ) {
			continue;
		}
		if ( !base ) {
			base = qtrue;
			RT_StageScale( s, scale );
			RT_ImageBright( s->images[0], scale, part );
			VectorAdd( glow, part, glow );
		} else if ( RT_StageAdds( s ) && RT_StageAverage( shader, s, 1.0f, qfalse, part ) ) {
			VectorAdd( glow, part, glow );
		}
	}
}

/*
The light a shader gives off by itself, as the average colour it draws with
the lighting term black: qtrue for an emitter. Sky, portals, fog and nodraw
give off none here (the sky lights the scene through rt_sample_sky_light),
and neither does a translucent surface that only blends over what is behind
it; one that adds to it (a flame, a glow) does.
*/
static qboolean RT_ShaderEmission( const shader_t *shader, vec3_t emission )
{
	const rtStageSource_t *first;
	unsigned blend;
	float dark[4];

	VectorClear( emission );
	// the default shader stands in for one that was not found: it shows a
	// placeholder, not a light
	if ( !shader || shader == tr.defaultShader || shader->defaultShader || shader->rtNumStages <= 0 ||
		!shader->rtStages || RT_ShaderIsSky( shader ) || shader->sort == SS_PORTAL ||
		( shader->contentFlags & CONTENTS_FOG ) || ( shader->surfaceFlags & SURF_NODRAW ) ) {
		return qfalse;
	}
	first = &shader->rtStages[0];
	blend = first->stateBits & GLS_BLEND_BITS;
	if ( blend && blend != ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO ) &&
		( first->stateBits & GLS_DSTBLEND_BITS ) != GLS_DSTBLEND_ONE ) {
		return qfalse;
	}
	if ( RT_ShaderFullbrightLit( shader ) ) {
		RT_FullbrightGlow( shader, emission );
		return ( 0.2126f * emission[0] + 0.7152f * emission[1] + 0.0722f * emission[2] ) >= RT_EMISSION_MIN ?
			qtrue : qfalse;
	}
	// structure first: nothing at zero light with white images is nothing
	RT_FoldStages( shader, 0.0f, qtrue, dark );
	if ( dark[0] + dark[1] + dark[2] < RT_EMISSION_MIN ) {
		return qfalse;
	}
	RT_FoldStages( shader, 0.0f, qfalse, dark );
	VectorCopy( dark, emission );
	return ( 0.2126f * dark[0] + 0.7152f * dark[1] + 0.0722f * dark[2] ) >= RT_EMISSION_MIN ? qtrue : qfalse;
}

static rtOwner_t RT_ClassifyOwner( const shader_t *shader, qboolean *masked )
{
	const rtStageSource_t *first;
	unsigned blend;
	int i;

	*masked = qfalse;
	if ( RT_ShaderIsSky( shader ) ) {
		return RT_OWNER_BASE_RASTER;
	}
	if ( shader->sort == SS_PORTAL ) {
		return RT_OWNER_BASE_RASTER;
	}
	if ( shader->rtNumStages <= 0 || !shader->rtStages ||
		shader->polygonOffset || shader->numDeforms > 0 || shader->hasScreenMap ||
		( shader->surfaceFlags & SURF_NODRAW ) ) {
		return RT_OWNER_RASTER;
	}
	if ( shader->sort > SS_OPAQUE && shader->sort != SS_SEE_THROUGH ) {
		return RT_OWNER_RASTER;
	}

	first = &shader->rtStages[0];
	blend = RT_StageBlend( first );
	if ( !( first->stateBits & GLS_DEPTHMASK_TRUE ) ||
		( first->stateBits & ( GLS_DEPTHTEST_DISABLE | GLS_POLYMODE_LINE ) ) ) {
		return RT_OWNER_RASTER;
	}
	if ( blend != 0 && blend != ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO ) ) {
		// a blended first stage composites over what is behind it
		return RT_OWNER_RASTER;
	}
	for ( i = 0; i < shader->rtNumStages; i++ ) {
		const rtStageSource_t *s = &shader->rtStages[i];
		if ( s->isScreenMap ) {
			return RT_OWNER_RASTER;
		}
		if ( s->stateBits & GLS_ATEST_BITS ) {
			*masked = qtrue;
		}
		if ( s->stateBits & GLS_ALICE_ATEST_BITS ) {
			// Alice's comparison alpha tests are not evaluated by the trace
			return RT_OWNER_RASTER;
		}
	}
	return RT_OWNER_TRACED;
}

/*
A stage's role in rt_material.glsl. lightingStage is the index of the
shader's first lighting stage, -1 when it has none.
*/
static uint32_t RT_StageRole( const rtStageSource_t *s, int index, int lightingStage )
{
	// an environment-mapped image added over the lit surface is a fake
	// reflection whatever the image, the lightmap included: Quake Live's
	// advertisements add a sheen of theirs ($lightmap, tcGen environment)
	if ( lightingStage >= 0 && index > lightingStage && RT_StageAdds( s ) &&
		( s->tcGen == TCGEN_ENVIRONMENT_MAPPED || s->tcGen == TCGEN_ENVIRONMENT_MAPPED_FP ) ) {
		return RT_ROLE_ENVIRONMENT;
	}
	if ( RT_StageIsLightmap( s ) ) {
		return RT_ROLE_LIGHTMAP;
	}
	if ( s->tcGen == TCGEN_FOG || s->rgbGen == CGEN_FOG ) {
		return RT_ROLE_SKIP;
	}
	return RT_ROLE_BASE;
}

static void RT_TranslateStage( const shader_t *shader, const rtStageSource_t *s, uint32_t role,
	uint32_t stageFlags, rtGpuStage_t *gpu )
{
	uint32_t images[RT_STAGE_MAX_ANIMATIONS];
	uint32_t numTexMods;
	int n;

	Com_Memset( gpu, 0, sizeof( *gpu ) );
	for ( n = 0; n < RT_STAGE_MAX_ANIMATIONS; n++ ) {
		int32_t index = ( n < s->numImages ) ? RT_ImageIndex( s->images[n] ) : -1;
		images[n] = ( index >= 0 ) ? (uint32_t)index : RT_NO_IMAGE;
	}
	for ( n = 0; n < 4; n++ ) {
		gpu->images[n] = images[n * 2] | ( images[n * 2 + 1] << 16 );
	}

	numTexMods = RT_PackTexMods( s, gpu );

	gpu->state[0] = s->stateBits;
	gpu->state[1] = (uint32_t)s->tcGen |
		( (uint32_t)s->rgbGen << 8 ) |
		( (uint32_t)s->alphaGen << 16 ) |
		( role << 24 );
	gpu->state[2] = (uint32_t)MIN( s->numImages, RT_STAGE_MAX_ANIMATIONS ) |
		( numTexMods << 4 ) |
		( RT_WaveFunc( s->rgbWave.func ) << 8 ) |
		( RT_WaveFunc( s->alphaWave.func ) << 12 ) |
		stageFlags;
	gpu->state[3] = s->constantColor.u32;

	gpu->animTc0[0] = s->animSpeed;
	gpu->animTc0[1] = s->tcGenVectors[0][0];
	gpu->animTc0[2] = s->tcGenVectors[0][1];
	gpu->animTc0[3] = s->tcGenVectors[0][2];
	gpu->tc1[0] = s->tcGenVectors[1][0];
	gpu->tc1[1] = s->tcGenVectors[1][1];
	gpu->tc1[2] = s->tcGenVectors[1][2];
	RT_PackWave( gpu->rgbWave, &s->rgbWave );
	RT_PackWave( gpu->alphaWave, &s->alphaWave );
	(void)shader;
}

static qboolean RT_TranslateMaterial( const shader_t *shader, rtGpuMaterial_t *out, rtMaterialInfo_t *info )
{
	int lightingStage = -1;
	int alphaTestStage = -1;
	qboolean lightmapped = qfalse;
	qboolean vertexLit = qfalse;
	qboolean entityLit = qfalse;
	qboolean envMapped = qfalse;
	uint32_t lightmapImage = RT_NO_IMAGE;
	uint32_t firstStage;
	int i;

	Com_Memset( out, 0, sizeof( *out ) );
	Com_Memset( info, 0, sizeof( *info ) );
	info->shader = shader;
	info->owner = RT_ClassifyOwner( shader, &info->masked );

	for ( i = 0; i < shader->rtNumStages; i++ ) {
		const rtStageSource_t *s = &shader->rtStages[i];
		if ( RT_StageIsLightmap( s ) ) {
			if ( lightingStage < 0 ) {
				lightingStage = i;
			}
			if ( shader->lightmapIndex == LIGHTMAP_BY_VERTEX ) {
				// r_vertexLight, or a surface without a lightmap: the vertex
				// colours carry the baked light (VertexLightingCollapse)
				vertexLit = qtrue;
				continue;
			}
			if ( lightmapImage == RT_NO_IMAGE ) {
				int32_t index = RT_ImageIndex( s->images[0] );
				lightmapImage = index >= 0 ? (uint32_t)index : RT_NO_IMAGE;
			}
			lightmapped = qtrue;
		} else if ( RT_StageVertexIsLight( shader, s ) ) {
			if ( lightingStage < 0 ) {
				lightingStage = i;
			}
			vertexLit = qtrue;
		} else if ( s->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			if ( lightingStage < 0 ) {
				lightingStage = i;
			}
			entityLit = qtrue;
		}
		if ( alphaTestStage < 0 && ( s->stateBits & GLS_ATEST_BITS ) ) {
			alphaTestStage = i;
		}
	}

	if ( !RT_GrowArray( (void **)&s_rtMaterials.stages, &s_rtMaterials.maxStages,
		s_rtMaterials.numStages + (uint32_t)MAX( shader->rtNumStages, 0 ), sizeof( rtGpuStage_t ), 1024u ) ) {
		return qfalse;
	}
	firstStage = s_rtMaterials.numStages;

	for ( i = 0; i < shader->rtNumStages && i < RT_GPU_MAX_STAGES; i++ ) {
		const rtStageSource_t *s = &shader->rtStages[i];
		const uint32_t role = RT_StageRole( s, i, lightingStage );
		uint32_t stageFlags = 0;

		if ( role == RT_ROLE_ENVIRONMENT ) {
			envMapped = qtrue;
		}

		if ( RT_StageVertexIsLight( shader, s ) ||
			( RT_StageIsLightmap( s ) && shader->lightmapIndex == LIGHTMAP_BY_VERTEX ) ) {
			stageFlags |= RT_SF_VERTEX_IS_LIGHT;
		}
		if ( i == alphaTestStage ) {
			stageFlags |= RT_SF_ALPHA_TEST;
		}
		if ( s->isDetail ) {
			stageFlags |= RT_SF_DETAIL;
		}
		if ( s->images[0] && s->images[0]->srgbDecode ) {
			stageFlags |= RT_SF_LINEAR_SAMPLE;
		}
		RT_TranslateStage( shader, s, role, stageFlags, &s_rtMaterials.stages[ s_rtMaterials.numStages++ ] );
	}

	out->firstStage = firstStage;
	out->stageCount = s_rtMaterials.numStages - firstStage;
	out->lightmapImage = lightmapImage;

	if ( info->masked ) {
		out->flags |= RT_MF_MASKED;
	}
	if ( RT_ShaderIsSky( shader ) ) {
		out->flags |= RT_MF_SKY;
	}
	if ( shader->sort == SS_PORTAL ) {
		out->flags |= RT_MF_PORTAL;
	}
	if ( shader->cullType == CT_TWO_SIDED ) {
		out->flags |= RT_MF_TWO_SIDED;
	} else if ( shader->cullType == CT_BACK_SIDED ) {
		out->flags |= RT_MF_CULL_FRONT;
	}
	if ( lightmapped ) {
		out->flags |= RT_MF_LIGHTMAPPED;
	}
	if ( vertexLit ) {
		out->flags |= RT_MF_VERTEX_LIT;
	}
	if ( entityLit ) {
		out->flags |= RT_MF_ENTITY_LIT;
	}
	if ( envMapped ) {
		out->flags |= RT_MF_ENV_MAPPED;
	}
	if ( !lightmapped && !vertexLit && !entityLit ) {
		out->flags |= RT_MF_NOLIGHT;
	}
	// the raster light pass lights only a shader with a lighting stage
	// (tr_shader.c FindLightingStages; tr_world.c leaves the rest unlit)
	if ( shader->lightingStage < 0 ) {
		out->flags |= RT_MF_NODLIGHT;
	}
	// an RTCW nofog material leaves the view's global fog, unless a stage
	// turns it back on (tr_rtcw_fog_vk.h fogs such a stage alone; the trace
	// fogs the whole surface)
	if ( shader->rtcwNoFog ) {
		qboolean stageFog = qfalse;
		for ( i = 0; i < MAX_SHADER_STAGES && shader->stages[i]; i++ ) {
			if ( shader->stages[i]->active && shader->stages[i]->rtcwFog ) {
				stageFog = qtrue;
			}
		}
		if ( !stageFog ) {
			out->flags |= RT_MF_NOFOG;
		}
	}
	// how the map compiler filtered light through it (rt_filter.glsl)
	if ( shader->rtCompiledFilter & RT_COMPILED_ALPHASHADOW ) {
		out->flags |= RT_MF_ALPHASHADOW;
	}
	if ( shader->rtCompiledFilter & RT_COMPILED_LIGHTFILTER ) {
		out->flags |= RT_MF_LIGHTFILTER;
	}
	if ( shader->surfaceLightValid && shader->surfaceLight > 0.0f ) {
		out->flags |= RT_MF_SURFACE_LIGHT;
		out->emit[0] = shader->rtEmitColorValid ? shader->rtEmitColor[0] : 1.0f;
		out->emit[1] = shader->rtEmitColorValid ? shader->rtEmitColor[1] : 1.0f;
		out->emit[2] = shader->rtEmitColorValid ? shader->rtEmitColor[2] : 1.0f;
		out->emit[3] = shader->surfaceLight;
	} else if ( RT_LightingEmits( RT_LightingMode() ) ) {
		// a glowing shader is sampled as a light (rt_lights.c
		// RT_AddEmissiveSurfaces): a bounce that meets it must not count it again
		vec3_t emission;
		if ( RT_ShaderEmission( shader, emission ) ) {
			out->flags |= RT_MF_SURFACE_LIGHT;
			VectorCopy( emission, out->emit );
		}
		if ( RT_ShaderFullbrightLit( shader ) ) {
			out->flags |= RT_MF_FULLBRIGHT_LIT;
		}
	}

	// Quake III materials carry no reflectance model beyond their stages.
	// An authored environment map marks a glossy surface.
	out->params[0] = envMapped ? 0.3f : 0.8f;	// roughness
	out->params[1] = 0.0f;						// metallic
	out->params[2] = envMapped ? 0.25f : 0.04f;	// normal-incidence specular reflectance

	// a texture pack's companion maps, sampled through the stage they
	// belong to (rt_material.glsl rt_material_maps)
	out->maps[0] = RT_NO_IMAGE;
	out->maps[1] = RT_NO_IMAGE;
	if ( ( shader->rtNormalMap || shader->rtMaterialMap ) &&
		shader->rtMaterialStage >= 0 && (uint32_t)shader->rtMaterialStage < out->stageCount ) {
		const int32_t normalImage = RT_ImageIndex( shader->rtNormalMap );
		const int32_t materialImage = RT_ImageIndex( shader->rtMaterialMap );
		if ( normalImage >= 0 ) {
			out->maps[0] = (uint32_t)normalImage;
		}
		if ( materialImage >= 0 ) {
			out->maps[1] = (uint32_t)materialImage;
			out->maps[2] = (uint32_t)shader->rtMaterialMapType;
		}
		out->maps[3] = (uint32_t)shader->rtMaterialStage;
		if ( normalImage >= 0 || materialImage >= 0 ) {
			out->flags |= RT_MF_MATERIAL_MAPS;
		}
	}

	// what the map compilers filtered light with (rt_filter.glsl)
	out->lightImage[0] = RT_NO_IMAGE;
	if ( shader->rtLightImageMissing ) {
		out->lightImage[1] |= RT_LIGHT_IMAGE_MISSING;
	} else if ( shader->rtLightImage ) {
		const int32_t lightImage = RT_ImageIndex( shader->rtLightImage );
		if ( lightImage >= 0 ) {
			out->lightImage[0] = (uint32_t)lightImage;
		}
		if ( shader->rtLightImage->srgbDecode ) {
			out->lightImage[1] |= RT_LIGHT_IMAGE_SRGB;
		}
	}
	out->timing[0] = (float)shader->timeOffset;
	out->timing[1] = (float)shader->clampTime;
	return qtrue;
}

static const shader_t *RT_ResolveShader( const shader_t *shader )
{
	if ( !shader ) {
		return tr.defaultShader;
	}
	return shader->remappedShader ? shader->remappedShader : shader;
}

// Global material index for a shader, translating it on first use.
static uint32_t RT_MaterialIndexForShader( const shader_t *shader )
{
	int32_t *slot;
	rtGpuMaterial_t material;
	rtMaterialInfo_t info;

	shader = RT_ResolveShader( shader );
	if ( !shader || shader->index < 0 || shader->index >= MAX_SHADERS || !RT_MaterialsBegin() ) {
		return 0;
	}
	slot = &s_rtMaterials.shaderMaterial[ shader->index ];
	if ( *slot > 0 ) {
		return (uint32_t)( *slot - 1 );
	}

	if ( s_rtMaterials.numMaterials >= s_rtMaterials.maxMaterials ) {
		uint32_t materialCapacity = s_rtMaterials.maxMaterials;
		uint32_t infoCapacity = s_rtMaterials.maxMaterials;
		if ( !RT_GrowArray( (void **)&s_rtMaterials.materials, &materialCapacity,
				s_rtMaterials.numMaterials + 1, sizeof( rtGpuMaterial_t ), 256u ) ||
			!RT_GrowArray( (void **)&s_rtMaterials.info, &infoCapacity,
				s_rtMaterials.numMaterials + 1, sizeof( rtMaterialInfo_t ), 256u ) ||
			materialCapacity != infoCapacity ) {
			return 0;
		}
		s_rtMaterials.maxMaterials = materialCapacity;
	}

	if ( !RT_TranslateMaterial( shader, &material, &info ) ) {
		return 0;
	}
	s_rtMaterials.materials[ s_rtMaterials.numMaterials ] = material;
	s_rtMaterials.info[ s_rtMaterials.numMaterials ] = info;
	*slot = (int32_t)s_rtMaterials.numMaterials + 1;
	s_rtMaterials.generation++;
	return s_rtMaterials.numMaterials++;
}

static rtOwner_t RT_MaterialOwner( uint32_t materialIndex )
{
	if ( materialIndex >= s_rtMaterials.numMaterials ) {
		return RT_OWNER_RASTER;
	}
	return s_rtMaterials.info[ materialIndex ].owner;
}

static qboolean RT_MaterialMasked( uint32_t materialIndex )
{
	return ( materialIndex < s_rtMaterials.numMaterials &&
		s_rtMaterials.info[ materialIndex ].masked ) ? qtrue : qfalse;
}
