/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_JEDI_SPRITE_DRAW_H
#define FNTECH3_TR_JEDI_SPRITE_DRAW_H

/*
 * Raven's surface sprites, generated over a surface's triangles once its own
 * passes are drawn (record 2026-10-15-jedi-surface-sprites). Each renderer's
 * tr_shade.c includes this, and its RB_EndSurface calls
 * RB_JediSurfaceSprites() before it clears the batch. The sprites then draw
 * as the sprite stage's own shader (tr_jedi_sprite_shader.h), so the
 * renderer's pipeline -- its blending, alpha test, fog adjustment and
 * Vulkan pipelines -- draws them like any other batch.
 *
 * From retail (jamp.exe 1.01's sprite code, 0x4be700-0x4c1000, whose
 * constants match the reference listed in the record): sprites sit on
 * triangles whose three vertex normals all pass the type's test, a triangle
 * of at most one square unit in the x/y plane takes none, the grid step is
 * the density over the root of twice the triangle's x/y area, each vertex
 * fades between the fade distance and the fade maximum (squared distances),
 * a sprite starts to fade at its own random point inside a 250-unit band,
 * the fade distances scale with the view's field of view against the first
 * ordinary one seen, sizes vary by the variance, half the sprites are
 * mirrored, fadeScale widens distant ones, idle sway circles at 0.0015 rad/ms
 * by 0.075 of the height times windIdle, an additive stage takes its fade as
 * brightness, and effect sprites cycle over fxDuration, growing and fading
 * as their range says. Weather effects draw only when r_surfaceWeather (0
 * by default) or a weather effect asks; fnTech3 has no weather effects.
 *
 * Fn's own: the random numbers are a hash seeded from each triangle (retail
 * steps through a 256-entry table seeded the same way), so every sprite
 * keeps its place from frame to frame; the light is the surface's full vertex
 * colour where retail took its blue channel as grey; and there is no global
 * wind, which retail takes from Academy's weather system or the r_wind*
 * cvars (all 0 by default), so only the shader's idle sway moves sprites.
 */

#define JEDI_SPRITE_FADE_RANGE		250.0f
/* An engine bound on one triangle's grid: a tiny density on a huge
 * triangle must not stall the frame. Retail has none; installed shaders
 * place a few thousand at most. */
#define JEDI_SPRITE_MAX_GRID		65536

typedef struct {
	int			numVertexes;
	int			numIndexes;
	vec3_t		xyz[SHADER_MAX_VERTEXES];
	float		normalZ[SHADER_MAX_VERTEXES];
	float		alpha[SHADER_MAX_VERTEXES];
	float		light[SHADER_MAX_VERTEXES][3];
	glIndex_t	indexes[SHADER_MAX_INDEXES];
} rbJediSpriteSource_t;

typedef struct {
	vec3_t	origin;			// the view, in the surface's own space
	vec3_t	right;
	vec3_t	up;
	vec3_t	fwd;			// right x (0 0 1)
	vec3_t	rightVectors[4];
	float	rangeScale;
	float	time;			// milliseconds
	float	weather;
	qboolean additive;
} rbJediSpriteView_t;

static rbJediSpriteSource_t rb_jediSpriteSource;
static cvar_t *rb_jediSurfaceSprites;
static cvar_t *rb_jediSurfaceWeather;
static float rb_jediStandardFovScale;
static qboolean rb_jediStandardFov;

static float RB_JediSpriteRandom( unsigned int *state ) {
	*state = *state * 1664525u + 1013904223u;
	return (float)( *state >> 8 ) * ( 1.0f / 16777216.0f );
}

static unsigned int RB_JediSpriteSeed( const float *v1, const float *v2, const float *v3, unsigned int salt ) {
	const float *v[3];
	unsigned int hash = 2166136261u ^ salt;
	int i, k;

	v[0] = v1; v[1] = v2; v[2] = v3;
	for ( i = 0; i < 3; i++ ) {
		for ( k = 0; k < 3; k++ ) {
			hash = ( hash ^ (unsigned int)(int)floorf( v[i][k] * 4.0f ) ) * 16777619u;
		}
	}
	return hash;
}

/* Retail's range scale: the fade distances grow as the view narrows,
 * measured against the first field of view between 50 and 135 degrees. */
static float RB_JediSpriteRangeScale( float fovX ) {
	float fov = fovX;

	if ( !rb_jediStandardFov ) {
		if ( fov > 50.0f && fov < 135.0f ) {
			rb_jediStandardFovScale = tanf( DEG2RAD( fov ) * 0.5f );
			rb_jediStandardFov = qtrue;
		}
		return 1.0f;
	}
	if ( fov > 135.0f ) {
		fov = 135.0f;
	}
	if ( fov <= 0.0f ) {
		return 1.0f;
	}
	return rb_jediStandardFovScale / tanf( DEG2RAD( fov ) * 0.5f );
}

static void RB_JediSpriteVertex( int ndx, const vec3_t xyz, float s, float t, const byte rgb[3], byte alpha ) {
	VectorCopy( xyz, tess.xyz[ndx] );
	VectorSet( tess.normal[ndx], 0.0f, 0.0f, 1.0f );
	tess.texCoords[0][ndx][0] = tess.texCoords[1][ndx][0] = s;
	tess.texCoords[0][ndx][1] = tess.texCoords[1][ndx][1] = t;
	tess.vertexColors[ndx].rgba[0] = rgb[0];
	tess.vertexColors[ndx].rgba[1] = rgb[1];
	tess.vertexColors[ndx].rgba[2] = rgb[2];
	tess.vertexColors[ndx].rgba[3] = alpha;
}

/* Retail's corner order: bottom right, top right, top left, bottom left,
 * textured (1 1) (1 0) (0 0) (0 1). */
static void RB_JediSpriteQuad( const vec3_t br, const vec3_t tr_, const vec3_t tl, const vec3_t bl,
	const float light[3], float alpha ) {
	byte rgb[3];
	byte a;
	int ndx, i;

	for ( i = 0; i < 3; i++ ) {
		rgb[i] = (byte)Com_Clamp( 0.0f, 255.0f, light[i] );
	}
	a = (byte)Com_Clamp( 0.0f, 255.0f, alpha * 255.0f );

	RB_CHECKOVERFLOW( 4, 6 );
	tess.surfType = SF_TRIANGLES;
	ndx = tess.numVertexes;
	RB_JediSpriteVertex( ndx + 0, br, 1.0f, 1.0f, rgb, a );
	RB_JediSpriteVertex( ndx + 1, tr_, 1.0f, 0.0f, rgb, a );
	RB_JediSpriteVertex( ndx + 2, tl, 0.0f, 0.0f, rgb, a );
	RB_JediSpriteVertex( ndx + 3, bl, 0.0f, 1.0f, rgb, a );
	tess.indexes[tess.numIndexes + 0] = ndx + 0;
	tess.indexes[tess.numIndexes + 1] = ndx + 1;
	tess.indexes[tess.numIndexes + 2] = ndx + 2;
	tess.indexes[tess.numIndexes + 3] = ndx + 0;
	tess.indexes[tess.numIndexes + 4] = ndx + 2;
	tess.indexes[tess.numIndexes + 5] = ndx + 3;
	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

/* Oriented and effect sprites: a billboard standing on loc, or with ssFaceup
 * (or any facing word) a square lying one unit above it. */
static void RB_JediSpriteBillboard( const rbJediSpriteView_t *view, const vec3_t loc,
	float width, float height, int facing, const float light[3], float alpha ) {
	vec3_t br, tr_, tl, bl, loc2, right;

	if ( facing ) {
		width *= 0.5f;
		VectorSet( br, loc[0] + width, loc[1] - width, loc[2] + 1.0f );
		VectorSet( tr_, loc[0] + width, loc[1] + width, loc[2] + 1.0f );
		VectorSet( tl, loc[0] - width, loc[1] + width, loc[2] + 1.0f );
		VectorSet( bl, loc[0] - width, loc[1] - width, loc[2] + 1.0f );
	} else {
		VectorMA( loc, height, view->up, loc2 );
		VectorScale( view->right, width * 0.5f, right );
		VectorAdd( loc, right, br );
		VectorAdd( loc2, right, tr_ );
		VectorSubtract( loc2, right, tl );
		VectorSubtract( loc, right, bl );
	}
	RB_JediSpriteQuad( br, tr_, tl, bl, light, alpha );
}

/* Vertical (and flattened) sprites: upright from loc, the top swaying by the
 * idle wind, the top left pulled a fifth of the width toward the view's
 * forward so the sprite is not quite flat. */
static void RB_JediSpriteVertical( const rbJediSpriteView_t *view, const jediSurfaceSprite_t *ss,
	const vec3_t loc, float width, float height, const vec2_t skew, int rightIndex,
	const float light[3], float alpha ) {
	vec3_t br, tr_, tl, bl, loc2, right;
	float angle, sway;

	angle = ( loc[0] + loc[1] ) * 0.02f + view->time * 0.0015f;
	loc2[0] = loc[0] + skew[0];
	loc2[1] = loc[1] + skew[1];
	if ( ss->windIdle > 0.0f ) {
		sway = height * ss->windIdle * 0.075f;
		loc2[0] += cosf( angle ) * sway;
		loc2[1] += sinf( angle ) * sway;
	}
	loc2[2] = ss->facing ? loc[2] - height : loc[2] + height;

	if ( ss->type == JEDI_SPRITE_FLATTENED ) {
		right[0] = sinf( DEG2RAD( loc[0] ) ) * width;
		right[1] = cosf( DEG2RAD( loc[0] ) ) * height;
		right[2] = 0.0f;
	} else {
		VectorScale( view->rightVectors[rightIndex], width * 0.5f, right );
	}

	VectorAdd( loc, right, br );
	VectorAdd( loc2, right, tr_ );
	tl[0] = loc2[0] - right[0] + view->fwd[0] * width * 0.2f;
	tl[1] = loc2[1] - right[1] + view->fwd[1] * width * 0.2f;
	tl[2] = loc2[2] - right[2];
	VectorSubtract( loc, right, bl );
	RB_JediSpriteQuad( br, tr_, tl, bl, light, alpha );
}

static qboolean RB_JediSpriteNormalPasses( const jediSurfaceSprite_t *ss, float z ) {
	if ( ss->type == JEDI_SPRITE_VERTICAL || ss->type == JEDI_SPRITE_FLATTENED ) {
		// any facing word hangs a vertical sprite down from a ceiling
		return ss->facing ? ( z <= -0.5f ? qtrue : qfalse ) : ( z >= 0.5f ? qtrue : qfalse );
	}
	return z >= ( ss->facing ? 0.99f : 0.5f ) ? qtrue : qfalse;
}

static void RB_JediSpriteTriangle( const rbJediSpriteView_t *view, const jediSurfaceSprite_t *ss,
	int i1, int i2, int i3, float density, float faderange ) {
	const rbJediSpriteSource_t *src = &rb_jediSpriteSource;
	const float *v1 = src->xyz[i1], *v2 = src->xyz[i2], *v3 = src->xyz[i3];
	const float a1 = src->alpha[i1], a2 = src->alpha[i2], a3 = src->alpha[i3];
	const qboolean effect = ss->type == JEDI_SPRITE_EFFECT || ss->type == JEDI_SPRITE_WEATHERFX ? qtrue : qfalse;
	const qboolean fadeInOut = effect && ss->fxAlphaEnd < 0.05f && ss->height >= 0.1f && ss->width >= 0.1f ? qtrue : qfalse;
	const float fxAlpha = ss->fxAlphaEnd - ss->fxAlphaStart;
	float triarea, step, posi, posj, fa, fb, fc, alphapos, fadestart, alpha, width, height, r;
	float light[3];
	vec3_t loc;
	vec2_t skew;
	unsigned int state, cell;
	int k, grid = 0, rightIndex = 0;

	if ( !RB_JediSpriteNormalPasses( ss, src->normalZ[i1] ) || !RB_JediSpriteNormalPasses( ss, src->normalZ[i2] ) ||
		!RB_JediSpriteNormalPasses( ss, src->normalZ[i3] ) ) {
		return;
	}
	if ( a1 <= 0.0f && a2 <= 0.0f && a3 <= 0.0f ) {
		return;
	}
	triarea = fabsf( ( v3[0] - v1[0] ) * ( v2[1] - v1[1] ) - ( v3[1] - v1[1] ) * ( v2[0] - v1[0] ) );
	if ( triarea <= 1.0f ) {
		return;
	}
	step = density / sqrtf( triarea );
	state = RB_JediSpriteSeed( v1, v2, v3, (unsigned int)ss->type );

	for ( posi = 0.0f; posi < 1.0f; posi += step ) {
		for ( posj = 0.0f; posj < 1.0f - posi; posj += step ) {
			if ( ++grid > JEDI_SPRITE_MAX_GRID ) {
				return;
			}
			if ( effect ) {
				// each cycle of the effect lands somewhere new in its cell
				const float cycle = ( view->time + 10000.0f * RB_JediSpriteRandom( &state ) ) / ss->fxDuration;
				const float effectpos = cycle - floorf( cycle );
				cell = state ^ ( (unsigned int)(int)floorf( cycle ) * 2654435761u );
				fa = posi + RB_JediSpriteRandom( &cell ) * step;
				if ( fa > 1.0f ) continue;
				fb = posj + RB_JediSpriteRandom( &cell ) * step;
				if ( fb > 1.0f - fa ) continue;
				fc = 1.0f - fa - fb;
				alphapos = a1 * fa + a2 * fb + a3 * fc;
				fadestart = faderange + ( 1.0f - faderange ) * RB_JediSpriteRandom( &cell );
				alpha = 1.0f - ( fadestart - alphapos ) / faderange;
				if ( alpha <= 0.0f ) continue;
				if ( alpha > 1.0f ) alpha = 1.0f;
				for ( k = 0; k < 3; k++ ) {
					loc[k] = v1[k] * fa + v2[k] * fb + v3[k] * fc;
					light[k] = src->light[i1][k] * fa + src->light[i2][k] * fb + src->light[i3][k] * fc;
				}
				r = RB_JediSpriteRandom( &cell );
				width = ss->width * ( 1.0f + ss->variance[0] * r );
				height = ss->height * ( 1.0f + ss->variance[1] * r );
				width += effectpos * ss->fxGrow[0] * width;
				height += effectpos * ss->fxGrow[1] * height;
				if ( fadeInOut ) {
					alpha *= ss->fxAlphaStart + fxAlpha * ( effectpos > 0.5f ? effectpos - 0.5f : 0.5f - effectpos ) * 2.0f;
				} else {
					alpha *= ss->fxAlphaStart + fxAlpha * effectpos;
				}
				if ( view->additive ) {
					for ( k = 0; k < 3; k++ ) {
						light[k] = ( 128.0f + light[k] * 0.5f ) * alpha;
					}
					alpha = 1.0f;
				}
				if ( RB_JediSpriteRandom( &cell ) > 0.5f ) {
					width = -width;
				}
				if ( ss->fadeScale != 0.0f && alphapos < 1.0f ) {
					width *= 1.0f + ss->fadeScale * ( 1.0f - alphapos );
				}
				RB_JediSpriteBillboard( view, loc, width, height, ss->facing, light, alpha );
				continue;
			}

			fa = posi + RB_JediSpriteRandom( &state ) * step;
			fb = posj + RB_JediSpriteRandom( &state ) * step;
			rightIndex = ( rightIndex + 1 ) & 3;
			if ( fa > 1.0f ) continue;
			if ( fb > 1.0f - fa ) continue;
			fc = 1.0f - fa - fb;
			alphapos = a1 * fa + a2 * fb + a3 * fc;
			fadestart = faderange + ( 1.0f - faderange ) * RB_JediSpriteRandom( &state );
			alpha = 1.0f - ( fadestart - alphapos ) / faderange;
			r = RB_JediSpriteRandom( &state );
			if ( alpha <= 0.0f ) continue;
			if ( alpha > 1.0f ) alpha = 1.0f;
			for ( k = 0; k < 3; k++ ) {
				loc[k] = v1[k] * fa + v2[k] * fb + v3[k] * fc;
				light[k] = src->light[i1][k] * fa + src->light[i2][k] * fb + src->light[i3][k] * fc;
			}
			if ( view->additive ) {
				for ( k = 0; k < 3; k++ ) {
					light[k] = ( 128.0f + light[k] * 0.5f ) * alpha;
				}
				alpha = 1.0f;
			}
			// one draw sizes both sides, so a sprite keeps its shape
			cell = state ^ 0x9e3779b9u;
			width = ss->width * ( 1.0f + ss->variance[0] * r );
			height = ss->height * ( 1.0f + ss->variance[1] * r );
			if ( RB_JediSpriteRandom( &cell ) > 0.5f ) {
				width = -width;
			}
			if ( ss->fadeScale != 0.0f && alphapos < 1.0f ) {
				width *= 1.0f + ss->fadeScale * ( 1.0f - alphapos );
			}
			if ( ss->type == JEDI_SPRITE_ORIENTED ) {
				RB_JediSpriteBillboard( view, loc, width, height, ss->facing, light, alpha );
				continue;
			}
			skew[0] = skew[1] = 0.0f;
			if ( ss->vertSkew != 0.0f ) {
				skew[0] = height * ( ss->vertSkew * 2.0f * RB_JediSpriteRandom( &cell ) - ss->vertSkew );
				skew[1] = height * ( ss->vertSkew * 2.0f * RB_JediSpriteRandom( &cell ) - ss->vertSkew );
			}
			RB_JediSpriteVertical( view, ss, loc, width, height, skew, rightIndex, light, alpha );
		}
	}
}

static void RB_JediSpritePass( const rbJediSpriteView_t *view, const jediSurfaceSprite_t *ss ) {
	rbJediSpriteSource_t *src = &rb_jediSpriteSource;
	float cut, fade, cut2, fade2, invFade, faderange, density;
	vec3_t d;
	int i;

	density = ss->density;
	if ( ss->type == JEDI_SPRITE_WEATHERFX ) {
		if ( view->weather < 0.01f ) {
			return;
		}
		density /= view->weather;
	}
	cut = ss->fadeMax * view->rangeScale;
	fade = ss->fadeDist * view->rangeScale;
	cut2 = cut * cut;
	fade2 = fade * fade;
	if ( cut2 <= fade2 ) {
		return;	// retail asserts here; a fade maximum at the fade distance has no band
	}
	invFade = 1.0f / ( cut2 - fade2 );
	faderange = JEDI_SPRITE_FADE_RANGE / ( cut - fade );
	if ( faderange > 1.0f ) {
		faderange = 1.0f;
	}
	for ( i = 0; i < src->numVertexes; i++ ) {
		VectorSubtract( view->origin, src->xyz[i], d );
		src->alpha[i] = 1.0f - ( DotProduct( d, d ) - fade2 ) * invFade;
	}
	for ( i = 0; i + 2 < src->numIndexes; i += 3 ) {
		RB_JediSpriteTriangle( view, ss, src->indexes[i], src->indexes[i + 1], src->indexes[i + 2], density, faderange );
	}
}

static void RB_JediSpriteViewSetup( rbJediSpriteView_t *view ) {
	static const vec3_t worldUp = { 0.0f, 0.0f, 1.0f };
	const orientationr_t *or = &backEnd.or;
	int k;

	VectorCopy( or->viewOrigin, view->origin );
	// the view's right and up, turned into the surface's own space
	for ( k = 0; k < 3; k++ ) {
		view->right[k] = DotProduct( backEnd.viewParms.or.axis[1], or->axis[k] );
		view->up[k] = DotProduct( backEnd.viewParms.or.axis[2], or->axis[k] );
	}
	CrossProduct( view->right, worldUp, view->fwd );
	VectorScale( view->right, 0.985f, view->rightVectors[0] );
	VectorMA( view->rightVectors[0], 0.174f, view->fwd, view->rightVectors[0] );
	VectorScale( view->right, 0.866f, view->rightVectors[1] );
	VectorMA( view->rightVectors[1], -0.5f, view->fwd, view->rightVectors[1] );
	VectorScale( view->right, 0.866f, view->rightVectors[2] );
	VectorMA( view->rightVectors[2], 0.5f, view->fwd, view->rightVectors[2] );
	VectorScale( view->right, 0.985f, view->rightVectors[3] );
	VectorMA( view->rightVectors[3], -0.174f, view->fwd, view->rightVectors[3] );
	/* The game's own field, not the one r_fovCorrection widened for the
	 * screen: retail measured its zoom against what the game asked for. */
	view->rangeScale = RB_JediSpriteRangeScale( backEnd.refdef.fovXAuthored > 0.0f
		? backEnd.refdef.fovXAuthored : backEnd.refdef.fov_x );
	view->time = (float)backEnd.refdef.time;
	view->weather = rb_jediSurfaceWeather ? rb_jediSurfaceWeather->value : 0.0f;
}

/* RB_EndSurface, after the surface's passes: draw its sprite stages. */
static void RB_JediSurfaceSprites( void ) {
	shader_t *parent = tess.shader;
	const int fogNum = tess.fogNum;
	const qboolean allowVBO = tess.allowVBO;
	rbJediSpriteSource_t *src = &rb_jediSpriteSource;
	rbJediSpriteView_t view;
	int i, k;

	if ( !parent || !parent->numJediSprites || tess.numIndexes < 3 || backEnd.projection2D ) {
		return;
	}
#ifdef USE_PMLIGHT
	if ( tess.dlightPass || tess.csmCasterPass || tess.csmShadowPass ||
		( backEnd.viewParms.passFlags & VPF_DLIGHT_SHADOW ) ) {
		return;
	}
#endif
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		return;
	}
#endif
	if ( !rb_jediSurfaceSprites ) {
		rb_jediSurfaceSprites = ri.Cvar_Get( "r_surfaceSprites", "1", CVAR_ARCHIVE_ND );
		rb_jediSurfaceWeather = ri.Cvar_Get( "r_surfaceWeather", "0", CVAR_ARCHIVE_ND );
	}
	if ( !rb_jediSurfaceSprites->integer ) {
		return;
	}

	src->numVertexes = tess.numVertexes;
	src->numIndexes = tess.numIndexes;
	for ( i = 0; i < tess.numVertexes; i++ ) {
		VectorCopy( tess.xyz[i], src->xyz[i] );
		src->normalZ[i] = tess.normal[i][2];
		for ( k = 0; k < 3; k++ ) {
			src->light[i][k] = tess.vertexColors[i].rgba[k];
		}
	}
	Com_Memcpy( src->indexes, tess.indexes, tess.numIndexes * sizeof( src->indexes[0] ) );
	RB_JediSpriteViewSetup( &view );

	for ( i = 0; i < parent->numJediSprites; i++ ) {
		const jediSpritePass_t *pass = &parent->jediSprites[i];
		if ( !pass->shader || pass->shader->defaultShader || !pass->shader->stages[0] ) {
			continue;
		}
		view.additive = ( pass->shader->stages[0]->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) ==
			( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) ? qtrue : qfalse;
		RB_BeginSurface( pass->shader, fogNum );
		RB_JediSpritePass( &view, &pass->parms );
		RB_EndSurface();
	}

	// the surface's batch goes on as it was begun
	RB_BeginSurface( parent, fogNum );
	tess.allowVBO = allowVBO;
}

#endif
