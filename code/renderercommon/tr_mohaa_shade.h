/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MOHAA_SHADE_H
#define FNTECH3_TR_MOHAA_SHADE_H

/*
===========================================================================

MEDAL OF HONOR: ALLIED ASSAULT'S SHADER SOURCES

The stage sources, deforms and texture modifiers Medal of Honor's shader
scripts add to the Ritual dialect, which code/renderercommon/tr_alice_shader.h
parses. Each is evaluated per vertex on the CPU, so a stage using one takes
the CPU path, as Alice's own sources do.

The retail engine executable is SafeDisc-encrypted on disk, so no retail
code settles these. The arithmetic follows OpenMoHAA's reconstruction
(renderergl1/tr_shade.c and tr_shade_calc.c, reference observation only)
except where noted:

- `deformVertexes flap` spreads its wave's phase over the vertices by
  their position, as Quake III's `wave` does with the same `<div>`; the
  reconstruction reads the `<div>` but never uses it.
- `tcGen environmentModel` is Quake III's reflection taken into world
  space, so an entity's reflection does not turn with it. The
  reconstruction computes that world-space vector but maps the model-space
  one.
- `rgbGen dot` and `oneMinusDot` use the Ritual engine's dot terms, which
  Alice's retail binary pins for its alpha twins (RB_AliceCalcDotAlpha);
  the reconstruction squares the dot product.
- `tcMod bulge` is parsed and changes nothing, as in the reconstruction.

A renderer includes this in its tr_shade_calc.c with
R_MOHAA_SHADE_IMPLEMENTATION defined, after EvalWaveForm, TableForFunc and
GlobalVectorToLocal; its other files get the declarations.
===========================================================================
*/

void RB_MOHAACalcDistFadeAlpha( qboolean oneMinus, unsigned char *dstColors );
void RB_MOHAACalcCoordColors( const shaderStage_t *stage, qboolean useT, qboolean alpha, unsigned char *dstColors );
void RB_MOHAACalcDotColor( const shaderStage_t *stage, qboolean oneMinus, unsigned char *dstColors );
void RB_MOHAACalcEnvironmentModelTexCoords( float *st );
void RB_MOHAACalcTexMod( const texModInfo_t *tmi, const float *src, float *dst );
void RB_MOHAAFlapDeform( const deformStage_t *ds, qboolean useT );
void RB_MOHAALightGlowDeform( void );
void RB_MOHAACalcLightingColor( unsigned char *dstColors );

#ifdef R_MOHAA_SHADE_IMPLEMENTATION

static byte RB_MOHAAByte( float value ) {
	return value <= 0.0f ? 0 : value >= 255.0f ? 255 : (byte)value;
}

/*
** RB_MOHAACalcDistFadeAlpha
**
** `alphaGen distFade [near [range]]`: opaque up to `near` units from the
** viewer, clear `range` units further, linear between; `oneMinusDistFade`
** the reverse. The distance is each vertex's own, so a model fades from its
** far side. Near and range default to 256 and belong to the shader.
*/
void RB_MOHAACalcDistFadeAlpha( qboolean oneMinus, unsigned char *dstColors ) {
	const float nearDistance = tess.shader->mohaaDistNear;
	const float range = tess.shader->mohaaDistRange;
	const float *xyz = tess.xyz[0];
	int i;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, dstColors += 4 ) {
		vec3_t delta;
		float f;

		VectorSubtract( xyz, backEnd.or.viewOrigin, delta );
		f = VectorLength( delta ) - nearDistance;
		if ( range > 0.0f ) {
			f /= range;
		} else {
			f = f > 0.0f ? 1.0f : 0.0f;
		}
		f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
		dstColors[3] = RB_MOHAAByte( ( oneMinus ? f : 1.0f - f ) * 255.0f );
	}
}

/*
** RB_MOHAACalcCoordColors
**
** `alphaGen sCoord|tCoord [min [max [low [high]]]]` and the rgbGen twins:
** the vertex's own s or t mapped from 0..1 onto min..max, times 255, held
** between low and high times 255. Min and max default to 0 and 1, low and
** high to 0 and 1.
*/
void RB_MOHAACalcCoordColors( const shaderStage_t *stage, qboolean useT, qboolean alpha, unsigned char *dstColors ) {
	const float lo = stage->mohaaCoordRange[0];
	const float hi = stage->mohaaCoordRange[1];
	const float capLo = (float)stage->mohaaCoordCaps[0];
	const float capHi = (float)stage->mohaaCoordCaps[1];
	int i;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 ) {
		float f = tess.texCoords[0][i][useT ? 1 : 0];
		byte value;

		f = ( ( hi - lo ) * f + lo ) * 255.0f;
		if ( f > capHi ) {
			f = capHi;
		}
		if ( f < capLo ) {
			f = capLo;
		}
		value = RB_MOHAAByte( f );
		if ( alpha ) {
			dstColors[3] = value;
		} else {
			dstColors[0] = dstColors[1] = dstColors[2] = value;
		}
	}
}

/*
** RB_MOHAACalcDotColor
**
** `rgbGen dot [min [max]]`: with v the unit vector from the vertex to the
** viewer and n its normal, max( n.v, 0 ) scaled into [min, max]; and
** `oneMinusDot` min( ( 1 - n.v )^2, 1 ), as the alpha forms do.
*/
void RB_MOHAACalcDotColor( const shaderStage_t *stage, qboolean oneMinus, unsigned char *dstColors ) {
	const float *xyz = tess.xyz[0];
	const float *normal = tess.normal[0];
	const float lo = stage->aliceDotRange[0];
	const float hi = stage->aliceDotRange[1];
	int i;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4, dstColors += 4 ) {
		vec3_t viewer;
		float d, x;

		VectorSubtract( backEnd.or.viewOrigin, xyz, viewer );
		VectorNormalize( viewer );
		d = DotProduct( normal, viewer );
		if ( oneMinus ) {
			x = ( 1.0f - d ) * ( 1.0f - d );
			if ( x > 1.0f ) {
				x = 1.0f;
			}
		} else {
			x = d > 0.0f ? d : 0.0f;
		}
		dstColors[0] = dstColors[1] = dstColors[2] = RB_MOHAAByte( ( lo + x * ( hi - lo ) ) * 255.0f );
	}
}

/*
** RB_MOHAACalcEnvironmentModelTexCoords
**
** `tcGen environmentModel`: Quake III's environment mapping with the
** reflection taken into world space first.
*/
void RB_MOHAACalcEnvironmentModelTexCoords( float *st ) {
	const float *xyz = tess.xyz[0];
	const float *normal = tess.normal[0];
	int i;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4, st += 2 ) {
		vec3_t viewer, reflected, world;
		float d;

		VectorSubtract( backEnd.or.viewOrigin, xyz, viewer );
		VectorNormalize( viewer );
		d = DotProduct( normal, viewer );
		reflected[0] = normal[0] * 2.0f * d - viewer[0];
		reflected[1] = normal[1] * 2.0f * d - viewer[1];
		reflected[2] = normal[2] * 2.0f * d - viewer[2];
		VectorScale( backEnd.or.axis[0], reflected[0], world );
		VectorMA( world, reflected[1], backEnd.or.axis[1], world );
		VectorMA( world, reflected[2], backEnd.or.axis[2], world );
		st[0] = 0.5f + world[1] * 0.5f;
		st[1] = 0.5f - world[2] * 0.5f;
	}
}

/*
** RB_MOHAACalcTexMod
**
** `tcMod wavetrans <wave>` and `wavetrant <wave>` shift s or t by the
** waveform's value; `bulge` leaves the coordinates as they are.
*/
void RB_MOHAACalcTexMod( const texModInfo_t *tmi, const float *src, float *dst ) {
	float shift[2] = { 0.0f, 0.0f };
	int i;

	switch ( tmi->type ) {
	case TMOD_MOHAA_WAVETRANS:
		shift[0] = EvalWaveForm( &tmi->wave );
		break;
	case TMOD_MOHAA_WAVETRANT:
		shift[1] = EvalWaveForm( &tmi->wave );
		break;
	default:
		break;
	}
	for ( i = 0; i < tess.numVertexes; i++ ) {
		dst[i * 2 + 0] = src[i * 2 + 0] + shift[0];
		dst[i * 2 + 1] = src[i * 2 + 1] + shift[1];
	}
}

/*
** RB_MOHAAFlapDeform
**
** `deformVertexes flap <s|t> <div> <wave> [min max]`: each vertex moves
** along its normal by the wave, phased by its position as `wave` is, times
** its s or t mapped from 0..1 onto min..max (0 and 1 by default) -- a leaf's
** or a curtain's free edge swings, the edge it hangs from stays. A wave
** with no frequency moves every vertex by the same amount.
*/
void RB_MOHAAFlapDeform( const deformStage_t *ds, qboolean useT ) {
	const float lo = ds->bulgeWidth;
	const float hi = ds->bulgeHeight;
	float *xyz = tess.xyz[0];
	const float *normal = tess.normal[0];
	int i;

	if ( ds->deformationWave.frequency == 0.0f ) {
		const float scale = EvalWaveForm( &ds->deformationWave );
		for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 ) {
			const float amount = scale * ( ( hi - lo ) * tess.texCoords[0][i][useT ? 1 : 0] + lo );
			VectorMA( xyz, amount, normal, xyz );
		}
		return;
	}
	{
		const float *table = TableForFunc( ds->deformationWave.func );
		for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 ) {
			const float off = ( xyz[0] + xyz[1] + xyz[2] ) * ds->deformationSpread;
			const float scale = WAVEVALUE( table, ds->deformationWave.base, ds->deformationWave.amplitude,
				ds->deformationWave.phase + off, ds->deformationWave.frequency );
			const float amount = scale * ( ( hi - lo ) * tess.texCoords[0][i][useT ? 1 : 0] + lo );
			VectorMA( xyz, amount, normal, xyz );
		}
	}
}

/*
** RB_MOHAALightGlowDeform
**
** `deformVertexes lightglow`: every quad becomes a sprite facing the viewer,
** as `autosprite` makes it, moved toward the viewer by its own radius but
** never nearer than four units, so a glow is not cut by the surface behind
** it.
*/
void RB_MOHAALightGlowDeform( void ) {
	const int oldVerts = tess.numVertexes;
	vec3_t leftDir, upDir;
	int i;

	if ( tess.numVertexes & 3 ) {
		ri.Printf( PRINT_WARNING, "LightGlowDeform shader %s had odd vertex count\n", tess.shader->name );
	}
	if ( tess.numIndexes != ( tess.numVertexes >> 2 ) * 6 ) {
		ri.Printf( PRINT_WARNING, "LightGlowDeform shader %s had odd index count\n", tess.shader->name );
	}
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.or.axis[1], leftDir );
		GlobalVectorToLocal( backEnd.viewParms.or.axis[2], upDir );
	} else {
		VectorCopy( backEnd.viewParms.or.axis[1], leftDir );
		VectorCopy( backEnd.viewParms.or.axis[2], upDir );
	}

	for ( i = 0; i + 3 < oldVerts; i += 4 ) {
		const float *xyz = tess.xyz[i];
		vec3_t mid, delta, forward, left, up;
		float radius, distance;

		mid[0] = 0.25f * ( xyz[0] + xyz[4] + xyz[8] + xyz[12] );
		mid[1] = 0.25f * ( xyz[1] + xyz[5] + xyz[9] + xyz[13] );
		mid[2] = 0.25f * ( xyz[2] + xyz[6] + xyz[10] + xyz[14] );
		VectorSubtract( xyz, mid, delta );
		radius = VectorLength( delta ) * 0.707f;

		VectorSubtract( backEnd.or.viewOrigin, mid, forward );
		distance = VectorNormalize( forward ) - 4.0f;
		VectorScale( leftDir, radius, left );
		VectorScale( upDir, radius, up );
		if ( R_ViewPassIsMirror( &backEnd.viewParms ) ) {
			VectorNegate( left, left );
		}
		if ( backEnd.currentEntity->e.nonNormalizedAxes ) {
			const float axisLength = VectorLength( backEnd.currentEntity->e.axis[0] );
			const float inverse = axisLength != 0.0f ? 1.0f / axisLength : 0.0f;
			VectorScale( left, inverse, left );
			VectorScale( up, inverse, up );
		}
		if ( distance < 0.0f ) {
			distance = 0.0f;
		}
		VectorMA( mid, radius < distance ? radius : distance, forward, mid );
		RB_AddQuadStamp( mid, left, up, tess.vertexColors[i] );
	}
}

/*
** RB_MOHAACalcLightingColor
**
** `rgbGen lightingSpherical` and `lightingGrid`. Retail lights a static
** model by its sphere lights or its origin's light grid value (OpenMoHAA's
** RB_Light_Real, RB_CalcLightGridColor); here its surfaces are part of the
** world, and carry the colours the map lit their vertices with
** (tr_mohaa_world.c), which stand in for both. An entity takes the engine's
** own diffuse lighting from the light grid and the scene's lights
** (AGENTS.md invariant 11).
*/
void RB_MOHAACalcLightingColor( unsigned char *dstColors ) {
	int i;

	if ( backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity ) {
		RB_CalcDiffuseColor( dstColors );
		return;
	}
	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 ) {
		dstColors[0] = RB_MOHAAByte( tess.vertexColors[i].rgba[0] * tr.identityLight );
		dstColors[1] = RB_MOHAAByte( tess.vertexColors[i].rgba[1] * tr.identityLight );
		dstColors[2] = RB_MOHAAByte( tess.vertexColors[i].rgba[2] * tr.identityLight );
		dstColors[3] = tess.vertexColors[i].rgba[3];
	}
}

#endif /* R_MOHAA_SHADE_IMPLEMENTATION */

#endif
