/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_shade_calc.c

#include "tr_local.h"
#include "../renderercommon/tr_q3_ihv_fog_render.h"
#include "../renderercommon/tr_jka_ranged_fog_shade.h"
#include "../renderercommon/tr_stvef_light.h"
#include "../renderercommon/tr_stvef_env.h"
// -EC-: avoid using ri.ftol
#define	WAVEVALUE( table, base, amplitude, phase, freq )  ((base) + table[ (int64_t)( ( ( (phase) + tess.shaderTime * (freq) ) * FUNCTABLE_SIZE ) ) & FUNCTABLE_MASK ] * (amplitude))

static float *TableForFunc( genFunc_t func )
{
	switch ( func )
	{
	case GF_SIN:
		return tr.sinTable;
	case GF_TRIANGLE:
		return tr.triangleTable;
	case GF_SQUARE:
		return tr.squareTable;
	case GF_SAWTOOTH:
		return tr.sawToothTable;
	case GF_INVERSE_SAWTOOTH:
		return tr.inverseSawToothTable;
	case GF_NONE:
	default:
		break;
	}

	ri.Error( ERR_DROP, "TableForFunc called with invalid function '%d' in shader '%s'", func, tess.shader->name );
	return NULL;
}


/*
** R_AliceWave
**
** American McGee's Alice: a waveform parameter written `fromEntity` comes
** from the entity's first four surface-state bytes -- base b0/16-8,
** amplitude b1/16, phase b2/16-8, frequency b3/16 (alice.exe
** 0x0049d122..0x0049d1b1, and the deform copy at 0x0049d21a).
*/
static const waveForm_t *R_AliceWave( const waveForm_t *wf, waveForm_t *resolved )
{
	const byte *bytes;

	if ( !COM_GAME_ID_IS_RITUAL( ri.gameId ) ||
		( wf->base != ALICE_SHADER_FROM_ENTITY && wf->amplitude != ALICE_SHADER_FROM_ENTITY &&
		  wf->phase != ALICE_SHADER_FROM_ENTITY && wf->frequency != ALICE_SHADER_FROM_ENTITY ) ) {
		return wf;
	}
	*resolved = *wf;
	bytes = backEnd.currentEntity ? backEnd.currentEntity->e.alice.surfaces : NULL;
	if ( resolved->base == ALICE_SHADER_FROM_ENTITY ) resolved->base = bytes ? bytes[0] / 16.0f - 8.0f : 0.0f;
	if ( resolved->amplitude == ALICE_SHADER_FROM_ENTITY ) resolved->amplitude = bytes ? bytes[1] / 16.0f : 0.0f;
	if ( resolved->phase == ALICE_SHADER_FROM_ENTITY ) resolved->phase = bytes ? bytes[2] / 16.0f - 8.0f : 0.0f;
	if ( resolved->frequency == ALICE_SHADER_FROM_ENTITY ) resolved->frequency = bytes ? bytes[3] / 16.0f : 0.0f;
	return resolved;
}


/*
** EvalWaveForm
**
** Evaluates a given waveForm_t, referencing backEnd.refdef.time directly
*/
static float EvalWaveForm( const waveForm_t *wf ) 
{
	float	*table;
	waveForm_t	aliceWave;

	wf = R_AliceWave( wf, &aliceWave );

	// Raven random: base + amplitude when a noise value from 0 to 2, chosen by
	// the view time plus phase, is at most the frequency; base otherwise
	// (stvoyHM.exe 0x47b5fa, stvoy.exe 0x46431a).
	if ( wf->func == GF_RANDOM ) {
		const int clock = (int)( (double)backEnd.refdef.time + wf->phase );
		return R_NoiseRandom( clock ) <= wf->frequency ? wf->base + wf->amplitude : wf->base;
	}
	// Outcast SP0x47a0bc/MP0x48a11c uses the view's float seconds, not
	// material shaderTime, and rounds the noise coordinate to binary32.
	if ( wf->func == GF_NOISE && ( ri.gameId == COM_GAME_JK2_SP || ri.gameId == COM_GAME_JK2_MP ) ) {
		volatile float clock = (float)backEnd.refdef.floatTime;
		volatile float coordinate = (float)( ( (double)clock + wf->phase ) * wf->frequency );
		return wf->base + R_NoiseGet4f( 0, 0, 0, coordinate ) * wf->amplitude;
	}
	// Elite Force also takes noise as a wave, as in tcMod stretch: base plus
	// amplitude times the noise at (time + phase) * frequency (stvoy.exe
	// EvalWaveForm 0x4642ec, stvoyHM.exe 0x47b5cc).
	if ( wf->func == GF_NOISE && ( ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP ) ) {
		return wf->base + R_NoiseGet4f( 0, 0, 0, ( backEnd.refdef.floatTime + wf->phase ) * wf->frequency ) * wf->amplitude;
	}

	table = TableForFunc( wf->func );

	return WAVEVALUE( table, wf->base, wf->amplitude, wf->phase, wf->frequency );
}

static float EvalWaveFormClamped( const waveForm_t *wf )
{
	float glow  = EvalWaveForm( wf );

	if ( glow < 0 )
	{
		return 0;
	}

	if ( glow > 1 )
	{
		return 1;
	}

	return glow;
}


/*
** RB_CalcStretchTexCoords
*/
void RB_CalcStretchTexCoords( const waveForm_t *wf, float *src, float *dst )
{
	float p;
	texModInfo_t tmi;

	p = 1.0f / EvalWaveForm( wf );

	tmi.matrix[0][0] = p;
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = 0.5f - 0.5f * p;

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = p;
	tmi.translate[1] = 0.5f - 0.5f * p;

	RB_CalcTransformTexCoords( &tmi, src, dst );
}


/*
====================================================================

DEFORMATIONS

====================================================================
*/


/*
========================
RB_CalcDeformVertexes
========================
*/
static void RB_CalcDeformVertexes( deformStage_t *ds )
{
	int i;
	vec3_t	offset;
	float	scale;
	float	*xyz = ( float * ) tess.xyz;
	float	*normal = ( float * ) tess.normal;
	float	*table;

	if ( ds->deformationWave.frequency == 0 )
	{
		scale = EvalWaveForm( &ds->deformationWave );

		for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 )
		{
			VectorScale( normal, scale, offset );
			
			xyz[0] += offset[0];
			xyz[1] += offset[1];
			xyz[2] += offset[2];
		}
	}
	else
	{
		table = TableForFunc( ds->deformationWave.func );

		for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 )
		{
			float off = ( xyz[0] + xyz[1] + xyz[2] ) * ds->deformationSpread;

			scale = WAVEVALUE( table, ds->deformationWave.base, 
				ds->deformationWave.amplitude,
				ds->deformationWave.phase + off,
				ds->deformationWave.frequency );

			VectorScale( normal, scale, offset );
			
			xyz[0] += offset[0];
			xyz[1] += offset[1];
			xyz[2] += offset[2];
		}
	}
}


/*
=========================
RB_CalcDeformNormals

Wiggle the normals for wavy environment mapping
=========================
*/
static void RB_CalcDeformNormals( deformStage_t *ds ) {
	int i;
	float	scale;
	float	*xyz = ( float * ) tess.xyz;
	float	*normal = ( float * ) tess.normal;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 ) {
		scale = 0.98f;
		scale = R_NoiseGet4f( xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 0 ] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f( 100 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 1 ] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f( 200 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 2 ] += ds->deformationWave.amplitude * scale;

		VectorNormalizeFast( normal );
	}
}


/*
========================
RB_CalcBulgeVertexes
========================
*/
static void RB_CalcBulgeVertexes( deformStage_t *ds ) {
	int i;
	const float *st = ( const float * ) tess.texCoords[0][0];
	float		*xyz = ( float * ) tess.xyz;
	float		*normal = ( float * ) tess.normal;
	double		now;

	now = backEnd.refdef.floatTime * ds->bulgeSpeed;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, st += 2, normal += 4 ) {
		int64_t off;
		float scale;

		off = (float)( FUNCTABLE_SIZE / (M_PI*2) ) * ( st[0] * ds->bulgeWidth + now );

		scale = tr.sinTable[ off & FUNCTABLE_MASK ] * ds->bulgeHeight;
			
		xyz[0] += normal[0] * scale;
		xyz[1] += normal[1] * scale;
		xyz[2] += normal[2] * scale;
	}
}


/*
======================
RB_CalcMoveVertexes

A deformation that can move an entire surface along a wave path
======================
*/
static void RB_CalcMoveVertexes( deformStage_t *ds ) {
	int			i;
	float		*xyz;
	float		*table;
	float		scale;
	vec3_t		offset;

	table = TableForFunc( ds->deformationWave.func );

	scale = WAVEVALUE( table, ds->deformationWave.base, 
		ds->deformationWave.amplitude,
		ds->deformationWave.phase,
		ds->deformationWave.frequency );

	VectorScale( ds->moveVector, scale, offset );

	xyz = ( float * ) tess.xyz;
	for ( i = 0; i < tess.numVertexes; i++, xyz += 4 ) {
		VectorAdd( xyz, offset, xyz );
	}
}


/*
========================
RB_AliceCalcWaveNormals

American McGee's Alice's `deformVertexes wavenormal` (alice.exe 0x0049d580):
each normal is pushed along the move vector by the wave at the vertex's
spread phase, then renormalized (0x0049d6dc); positions do not move. Only
the table functions drive it: retail's noise has no table there and moves
nothing here.
========================
*/
static void RB_AliceCalcWaveNormals( const deformStage_t *ds ) {
	const float	*table;
	const float	*xyz = ( const float * ) tess.xyz;
	float		*normal = ( float * ) tess.normal;
	int			i;

	switch ( ds->deformationWave.func ) {
	case GF_SIN:
	case GF_SQUARE:
	case GF_TRIANGLE:
	case GF_SAWTOOTH:
	case GF_INVERSE_SAWTOOTH:
		table = TableForFunc( ds->deformationWave.func );
		break;
	default:
		return;
	}

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 ) {
		const float off = ( xyz[0] + xyz[1] + xyz[2] ) * ds->deformationSpread;
		const float scale = WAVEVALUE( table, ds->deformationWave.base,
			ds->deformationWave.amplitude,
			ds->deformationWave.phase + off,
			ds->deformationWave.frequency );

		VectorMA( normal, scale, ds->moveVector, normal );
		VectorNormalize( normal );
	}
}


/*
=============
DeformText

Change a polygon into a bunch of text polygons
=============
*/
static void DeformText( const char *text ) {
	int		i;
	vec3_t	origin, width, height;
	int		len;
	int		ch;
	color4ub_t color;
	float	bottom, top;
	vec3_t	mid;

	height[0] = 0;
	height[1] = 0;
	height[2] = -1;
	CrossProduct( tess.normal[0], height, width );

	// find the midpoint of the box
	VectorClear( mid );
	bottom = 999999;
	top = -999999;
	for ( i = 0 ; i < 4 ; i++ ) {
		VectorAdd( tess.xyz[i], mid, mid );
		if ( tess.xyz[i][2] < bottom ) {
			bottom = tess.xyz[i][2];
		}
		if ( tess.xyz[i][2] > top ) {
			top = tess.xyz[i][2];
		}
	}
	VectorScale( mid, 0.25f, origin );

	// determine the individual character size
	height[0] = 0;
	height[1] = 0;
	height[2] = ( top - bottom ) * 0.5f;

	VectorScale( width, height[2] * -0.75f, width );

	// determine the starting position
	len = strlen( text );
	VectorMA( origin, (len-1), width, origin );

	// clear the shader indexes
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	color.u32 = ~0U;

	// draw each character
	for ( i = 0 ; i < len ; i++ ) {
		ch = text[i];
		ch &= 255;

		if ( ch != ' ' ) {
			int		row, col;
			float	frow, fcol, size;

			row = ch>>4;
			col = ch&15;

			frow = row*0.0625f;
			fcol = col*0.0625f;
			size = 0.0625f;

			RB_AddQuadStampExt( origin, width, height, color, fcol, frow, fcol + size, frow + size );
		}
		VectorMA( origin, -2, width, origin );
	}
}


/*
==================
GlobalVectorToLocal
==================
*/
static void GlobalVectorToLocal( const vec3_t in, vec3_t out ) {
	out[0] = DotProduct( in, backEnd.or.axis[0] );
	out[1] = DotProduct( in, backEnd.or.axis[1] );
	out[2] = DotProduct( in, backEnd.or.axis[2] );
}

#define R_MOHAA_SHADE_IMPLEMENTATION
#include "../renderercommon/tr_mohaa_shade.h"


/*
=====================
AutospriteDeform

Assuming all the triangles for this shader are independent
quads, rebuild them as forward facing sprites
=====================
*/
static void AutospriteDeform( void ) {
	int		i;
	int		oldVerts;
	float	*xyz;
	vec3_t	mid, delta;
	float	radius;
	vec3_t	left, up;
	vec3_t	leftDir, upDir;

	if ( tess.numVertexes & 3 ) {
		ri.Printf( PRINT_WARNING, "Autosprite shader %s had odd vertex count\n", tess.shader->name );
	}
	if ( tess.numIndexes != ( tess.numVertexes >> 2 ) * 6 ) {
		ri.Printf( PRINT_WARNING, "Autosprite shader %s had odd index count\n", tess.shader->name );
	}

	oldVerts = tess.numVertexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.or.axis[1], leftDir );
		GlobalVectorToLocal( backEnd.viewParms.or.axis[2], upDir );
	} else {
		VectorCopy( backEnd.viewParms.or.axis[1], leftDir );
		VectorCopy( backEnd.viewParms.or.axis[2], upDir );
	}

	for ( i = 0 ; i < oldVerts ; i+=4 ) {
		// find the midpoint
		xyz = tess.xyz[i];

		mid[0] = 0.25f * (xyz[0] + xyz[4] + xyz[8] + xyz[12]);
		mid[1] = 0.25f * (xyz[1] + xyz[5] + xyz[9] + xyz[13]);
		mid[2] = 0.25f * (xyz[2] + xyz[6] + xyz[10] + xyz[14]);

		VectorSubtract( xyz, mid, delta );
		radius = VectorLength( delta ) * 0.707f;		// / sqrt(2)

		VectorScale( leftDir, radius, left );
		VectorScale( upDir, radius, up );

		if ( R_ViewPassIsMirror( &backEnd.viewParms ) ) {
			VectorSubtract( vec3_origin, left, left );
		}

		// compensate for scale in the axes if necessary
		if ( backEnd.currentEntity->e.nonNormalizedAxes ) {
			float axisLength;
			axisLength = VectorLength( backEnd.currentEntity->e.axis[0] );
			if ( !axisLength ) {
				axisLength = 0;
			} else {
				axisLength = 1.0f / axisLength;
			}
			VectorScale(left, axisLength, left);
			VectorScale(up, axisLength, up);
		}

		RB_AddQuadStamp( mid, left, up, tess.vertexColors[i] );
	}
}


/*
=====================
Autosprite2Deform

Autosprite2 will pivot a rectangular quad along the center of its long axis
=====================
*/
static const unsigned int edgeVerts[6][2] = {
	{ 0, 1 },
	{ 0, 2 },
	{ 0, 3 },
	{ 1, 2 },
	{ 1, 3 },
	{ 2, 3 }
};

static void Autosprite2Deform( void ) {
	int		i, j, k;
	int		indexes;
	float	*xyz;
	vec3_t	forward;

	if ( tess.numVertexes & 3 ) {
		ri.Printf( PRINT_WARNING, "Autosprite2 shader %s had odd vertex count\n", tess.shader->name );
	}
	if ( tess.numIndexes != ( tess.numVertexes >> 2 ) * 6 ) {
		ri.Printf( PRINT_WARNING, "Autosprite2 shader %s had odd index count\n", tess.shader->name );
	}

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.or.axis[0], forward );
	} else {
		VectorCopy( backEnd.viewParms.or.axis[0], forward );
	}

	// this is a lot of work for two triangles...
	// we could precalculate a lot of it is an issue, but it would mess up
	// the shader abstraction
	for ( i = 0, indexes = 0 ; i < tess.numVertexes ; i+=4, indexes+=6 ) {
		float	lengths[2];
		int		nums[2];
		vec3_t	mid[2];
		vec3_t	major, minor;
		float	*v1, *v2;

		// find the midpoint
		xyz = tess.xyz[i];

		// identify the two shortest edges
		nums[0] = nums[1] = 0;
		lengths[0] = lengths[1] = 999999;

		for ( j = 0 ; j < 6 ; j++ ) {
			float	l;
			vec3_t	temp;

			v1 = xyz + 4 * edgeVerts[j][0];
			v2 = xyz + 4 * edgeVerts[j][1];

			VectorSubtract( v1, v2, temp );
			
			l = DotProduct( temp, temp );
			if ( l < lengths[0] ) {
				nums[1] = nums[0];
				lengths[1] = lengths[0];
				nums[0] = j;
				lengths[0] = l;
			} else if ( l < lengths[1] ) {
				nums[1] = j;
				lengths[1] = l;
			}
		}

		for ( j = 0 ; j < 2 ; j++ ) {
			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			mid[j][0] = 0.5f * (v1[0] + v2[0]);
			mid[j][1] = 0.5f * (v1[1] + v2[1]);
			mid[j][2] = 0.5f * (v1[2] + v2[2]);
		}

		// find the vector of the major axis
		VectorSubtract( mid[1], mid[0], major );

		// cross this with the view direction to get minor axis
		CrossProduct( major, forward, minor );
		VectorNormalize( minor );
		
		// re-project the points
		for ( j = 0 ; j < 2 ; j++ ) {
			float	l;

			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			l = 0.5 * sqrt( lengths[j] );
			
			// we need to see which direction this edge
			// is used to determine direction of projection
			for ( k = 0 ; k < 5 ; k++ ) {
				if ( tess.indexes[ indexes + k ] == i + edgeVerts[nums[j]][0]
					&& tess.indexes[ indexes + k + 1 ] == i + edgeVerts[nums[j]][1] ) {
					break;
				}
			}

			if ( k == 5 ) {
				VectorMA( mid[j], l, minor, v1 );
				VectorMA( mid[j], -l, minor, v2 );
			} else {
				VectorMA( mid[j], -l, minor, v1 );
				VectorMA( mid[j], l, minor, v2 );
			}
		}
	}
}


/*
=====================
RB_DeformTessGeometry

=====================
*/
void RB_DeformTessGeometry( void ) {
	int		i;
	deformStage_t	*ds;
	deformStage_t	aliceDeform;
	waveForm_t		aliceWave;

	for ( i = 0 ; i < tess.shader->numDeforms ; i++ ) {
		ds = &tess.shader->deforms[ i ];

		// Alice: `fromEntity` wave parameters resolved for this entity
		if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {
			aliceDeform = *ds;
			aliceDeform.deformationWave = *R_AliceWave( &ds->deformationWave, &aliceWave );
			ds = &aliceDeform;
		}

		switch ( ds->deformation ) {
		case DEFORM_NONE:
			break;
		case DEFORM_ALICE_WAVENORMAL:
			RB_AliceCalcWaveNormals( ds );
			break;
		case DEFORM_MOHAA_FLAP_S:
		case DEFORM_MOHAA_FLAP_T:
			RB_MOHAAFlapDeform( ds, ds->deformation == DEFORM_MOHAA_FLAP_T ? qtrue : qfalse );
			break;
		case DEFORM_MOHAA_LIGHTGLOW:
			RB_MOHAALightGlowDeform();
			break;
		case DEFORM_NORMALS:
			RB_CalcDeformNormals( ds );
			break;
		case DEFORM_WAVE:
			RB_CalcDeformVertexes( ds );
			break;
		case DEFORM_BULGE:
			RB_CalcBulgeVertexes( ds );
			break;
		case DEFORM_MOVE:
			RB_CalcMoveVertexes( ds );
			break;
		case DEFORM_PROJECTION_SHADOW:
			RB_ProjectionShadowDeform();
			break;
		case DEFORM_AUTOSPRITE:
			AutospriteDeform();
			break;
		case DEFORM_AUTOSPRITE2:
			Autosprite2Deform();
			break;
		case DEFORM_TEXT0:
		case DEFORM_TEXT1:
		case DEFORM_TEXT2:
		case DEFORM_TEXT3:
		case DEFORM_TEXT4:
		case DEFORM_TEXT5:
		case DEFORM_TEXT6:
		case DEFORM_TEXT7:
			DeformText( backEnd.refdef.text[ds->deformation - DEFORM_TEXT0] );
			break;
		}
	}
}

/*
====================================================================

COLORS

====================================================================
*/


/*
** RB_CalcColorFromEntity
*/
void RB_CalcColorFromEntity( unsigned char *dstColors )
{
	uint32_t c, *pColors = (uint32_t *)dstColors;
	int	i;

	if ( !backEnd.currentEntity )
		return;

	c = backEnd.currentEntity->e.shader.u32;

	for ( i = 0; i < tess.numVertexes; i++, pColors++ )
	{
		*pColors = c;
	}
}


/*
** RB_CalcColorFromOneMinusEntity
*/
void RB_CalcColorFromOneMinusEntity( unsigned char *dstColors )
{
	int	i;
	uint32_t *pColors = ( uint32_t * ) dstColors;
	color4ub_t invModulate;

	if ( !backEnd.currentEntity )
		return;

	invModulate.rgba[0] = 255 - backEnd.currentEntity->e.shader.rgba[0];
	invModulate.rgba[1] = 255 - backEnd.currentEntity->e.shader.rgba[1];
	invModulate.rgba[2] = 255 - backEnd.currentEntity->e.shader.rgba[2];
	invModulate.rgba[3] = 255 - backEnd.currentEntity->e.shader.rgba[3];	// this trashes alpha, but the AGEN block fixes it

	for ( i = 0; i < tess.numVertexes; i++, pColors++ )
	{
		*pColors = invModulate.u32;
	}
}


/*
** RB_CalcAlphaFromEntity
*/
void RB_CalcAlphaFromEntity( unsigned char *dstColors )
{
	int	i;

	if ( !backEnd.currentEntity )
		return;

	dstColors += 3;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		*dstColors = backEnd.currentEntity->e.shader.rgba[3];
	}
}


/*
** RB_CalcAlphaFromOneMinusEntity
*/
void RB_CalcAlphaFromOneMinusEntity( unsigned char *dstColors )
{
	int	i;

	if ( !backEnd.currentEntity )
		return;

	dstColors += 3;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		*dstColors = 0xff - backEnd.currentEntity->e.shader.rgba[3];
	}
}


/*
** RB_CalcWaveColor
*/
void RB_CalcWaveColor( const waveForm_t *wf, unsigned char *dstColors )
{
	int v, i;
	float glow;
	uint32_t *colors = ( uint32_t * ) dstColors;
	color4ub_t color;

	if ( wf->func == GF_NOISE ) {
		glow = wf->base + R_NoiseGet4f( 0, 0, 0, ( tess.shaderTime + wf->phase ) * wf->frequency ) * wf->amplitude;
	} else {
		glow = EvalWaveForm( wf ) * tr.identityLight;
	}

	v = myftol( 255 * glow );

	if ( v < 0 )
		v = 0;
	else if ( v > 255 )
		v = 255;

	color.rgba[0] = color.rgba[1] = color.rgba[2] = v;
	color.rgba[3] = 255;
	
	for ( i = 0; i < tess.numVertexes; i++, colors++ ) {
		*colors = color.u32;
	}
}


/*
** RB_CalcWaveAlpha
*/
void RB_CalcWaveAlpha( const waveForm_t *wf, unsigned char *dstColors )
{
	int i;
	int v;
	float glow;

	// A noise wave has no lookup table. Enemy Territory evaluates it as the
	// colour wave does, and does not clamp it before the byte is stored, so a
	// wave reaching outside zero and one wraps; its main-menu lightning is
	// drawn that way. No other shipped content uses a noise alpha wave, which
	// every other profile's renderer refused outright.
	if ( wf->func == GF_NOISE ) {
		glow = wf->base + R_NoiseGet4f( 0, 0, 0, ( tess.shaderTime + wf->phase ) * wf->frequency ) * wf->amplitude;
		// American McGee's Alice clamps a noise alpha as it clamps any other
		// (alice.exe 0x0049e6d0); 21 of its shipped stages use one.
		if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {
			glow = glow < 0.0f ? 0.0f : glow > 1.0f ? 1.0f : glow;
		}
	} else {
		glow = EvalWaveFormClamped( wf );
	}

	v = 255 * glow;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		dstColors[3] = v;
	}
}


/*
** RB_CalcModulateColorsByFog
*/
void RB_CalcModulateColorsByFog( unsigned char *colors ) {
	int		i;
	float	factors[SHADER_MAX_VERTEXES];

	RB_CalcFogFactors( factors );

	for ( i = 0; i < tess.numVertexes; i++, colors += 4 ) {
		float f = 1.0 - factors[i];
		colors[0] *= f;
		colors[1] *= f;
		colors[2] *= f;
	}
}


/*
** RB_CalcModulateAlphasByFog
*/
void RB_CalcModulateAlphasByFog( unsigned char *colors ) {
	int		i;
	float	factors[SHADER_MAX_VERTEXES];

	RB_CalcFogFactors( factors );

	for ( i = 0; i < tess.numVertexes; i++, colors += 4 ) {
		float f = 1.0 - factors[i];
		colors[3] *= f;
	}
}


/*
** RB_CalcModulateRGBAsByFog
*/
void RB_CalcModulateRGBAsByFog( unsigned char *colors ) {
	int		i;
	float	factors[SHADER_MAX_VERTEXES];

	RB_CalcFogFactors( factors );

	for ( i = 0; i < tess.numVertexes; i++, colors += 4 ) {
		float f = 1.0 - factors[i];
		colors[0] *= f;
		colors[1] *= f;
		colors[2] *= f;
		colors[3] *= f;
	}
}


/*
====================================================================

TEX COORDS

====================================================================
*/

/*
========================
RB_CalcFogTexCoords

To do the clipped fog plane really correctly, we should use
projected textures, but I don't trust the drivers and it
doesn't fit our shader data.
========================
*/
void RB_CalcFogTexCoords( float *st ) {
	int			i;
	float		*v;
	float		s, t;
	float		eyeT;
	qboolean	eyeOutside;
	const fog_t		*fog;
	vec3_t		local;
	float		fogScale, fogStart;
	vec4_t		fogDistanceVector, fogDepthVector = {0, 0, 0, 0};

	fog = tr.world->fogs + tess.fogNum;

	// all fogging distance is based on world Z units
	VectorSubtract( backEnd.or.origin, backEnd.viewParms.or.origin, local );
	fogDistanceVector[0] = -backEnd.or.modelMatrix[2];
	fogDistanceVector[1] = -backEnd.or.modelMatrix[6];
	fogDistanceVector[2] = -backEnd.or.modelMatrix[10];
	fogDistanceVector[3] = DotProduct( local, backEnd.viewParms.or.axis[0] );

	// scale the fog vectors based on the fog's thickness, or Academy's
	// zoom fog ramp beginning at its own distance
	RB_JKARangedFog( fog, &fogScale, &fogStart );
	fogDistanceVector[0] *= fogScale;
	fogDistanceVector[1] *= fogScale;
	fogDistanceVector[2] *= fogScale;
	fogDistanceVector[3] = ( fogDistanceVector[3] - fogStart ) * fogScale;

	// rotate the gradient vector for this orientation
	if ( fog->hasSurface ) {
		fogDepthVector[0] = fog->surface[0] * backEnd.or.axis[0][0] + 
			fog->surface[1] * backEnd.or.axis[0][1] + fog->surface[2] * backEnd.or.axis[0][2];
		fogDepthVector[1] = fog->surface[0] * backEnd.or.axis[1][0] + 
			fog->surface[1] * backEnd.or.axis[1][1] + fog->surface[2] * backEnd.or.axis[1][2];
		fogDepthVector[2] = fog->surface[0] * backEnd.or.axis[2][0] + 
			fog->surface[1] * backEnd.or.axis[2][1] + fog->surface[2] * backEnd.or.axis[2][2];
		fogDepthVector[3] = -fog->surface[3] + DotProduct( backEnd.or.origin, fog->surface );

		eyeT = DotProduct( backEnd.or.viewOrigin, fogDepthVector ) + fogDepthVector[3];
	} else {
		eyeT = 1;	// non-surface fog always has eye inside
	}

	// see if the viewpoint is outside
	// this is needed for clipping distance even for constant fog

	if ( eyeT < 0 ) {
		eyeOutside = qtrue;
	} else {
		eyeOutside = qfalse;
	}

	fogDistanceVector[3] += 1.0/512;

	// calculate density for each point
	for (i = 0, v = tess.xyz[0] ; i < tess.numVertexes ; i++, v += 4) {
		// calculate the length in fog
		s = DotProduct( v, fogDistanceVector ) + fogDistanceVector[3];
		t = DotProduct( v, fogDepthVector ) + fogDepthVector[3];

		// partially clipped fogs use the T axis		
		if ( eyeOutside ) {
			if ( t < 1.0 ) {
				t = 1.0/32;	// point is outside, so no fogging
			} else {
				t = 1.0/32 + 30.0/32 * t / ( t - eyeT );	// cut the distance at the fog plane
			}
		} else {
			if ( t < 0 ) {
				t = 1.0/32;	// point is outside, so no fogging
			} else {
				t = 31.0/32;
			}
		}

		st[0] = s;
		st[1] = t;
		st += 2;
	}
}


/*
========================
RB_CalcFogProgramParms
========================
*/
const fogProgramParms_t *RB_CalcFogProgramParms( void )
{
	static fogProgramParms_t parm;
	const fog_t	*fog;
	vec3_t		local;
	float		fogScale, fogStart;

	Com_Memset( parm.fogDepthVector, 0, sizeof( parm.fogDepthVector ) );

	fog = tr.world->fogs + tess.fogNum;

	// all fogging distance is based on world Z units
	VectorSubtract( backEnd.or.origin, backEnd.viewParms.or.origin, local );
	parm.fogDistanceVector[0] = -backEnd.or.modelMatrix[2];
	parm.fogDistanceVector[1] = -backEnd.or.modelMatrix[6];
	parm.fogDistanceVector[2] = -backEnd.or.modelMatrix[10];
	parm.fogDistanceVector[3] = DotProduct( local, backEnd.viewParms.or.axis[0] );

	// scale the fog vectors based on the fog's thickness, or Academy's
	// zoom fog ramp beginning at its own distance
	RB_JKARangedFog( fog, &fogScale, &fogStart );
	parm.fogDistanceVector[0] *= fogScale;
	parm.fogDistanceVector[1] *= fogScale;
	parm.fogDistanceVector[2] *= fogScale;
	parm.fogDistanceVector[3] = ( parm.fogDistanceVector[3] - fogStart ) * fogScale;

	// rotate the gradient vector for this orientation
	if ( fog->hasSurface ) {
		parm.fogDepthVector[0] = fog->surface[0] * backEnd.or.axis[0][0] + 
			fog->surface[1] * backEnd.or.axis[0][1] + fog->surface[2] * backEnd.or.axis[0][2];
		parm.fogDepthVector[1] = fog->surface[0] * backEnd.or.axis[1][0] + 
			fog->surface[1] * backEnd.or.axis[1][1] + fog->surface[2] * backEnd.or.axis[1][2];
		parm.fogDepthVector[2] = fog->surface[0] * backEnd.or.axis[2][0] + 
			fog->surface[1] * backEnd.or.axis[2][1] + fog->surface[2] * backEnd.or.axis[2][2];
		parm.fogDepthVector[3] = -fog->surface[3] + DotProduct( backEnd.or.origin, fog->surface );

		parm.eyeT = DotProduct( backEnd.or.viewOrigin, parm.fogDepthVector ) + parm.fogDepthVector[3];
	} else {
		parm.eyeT = 1.0f; // non-surface fog always has eye inside
	}

	// see if the viewpoint is outside
	// this is needed for clipping distance even for constant fog

	if ( parm.eyeT < 0 ) {
		parm.eyeOutside = qtrue;
	} else {
		parm.eyeOutside = qfalse;
	}

	parm.fogDistanceVector[3] += 1.0/512;
	parm.fogColor = fog->color;

	return &parm;
}


/*
========================
RB_CalcEnvironmentTexCoordsFPscr
========================
*/
static void RB_CalcEnvironmentTexCoordsFPscr( float *st ) {
	int			i;
	const float	*v, *normal;
	vec3_t		viewer, reflected;
	float		d;

	v = tess.xyz[0];
	normal = tess.normal[0];

	for (i = 0 ; i < tess.numVertexes ; i++, v += 4, normal += 4, st += 2 ) {
		VectorSubtract( backEnd.or.viewOrigin, v, viewer );
		VectorNormalizeFast( viewer );

		d = DotProduct( normal, viewer );

		reflected[1] = normal[1]*2*d - viewer[1];
		reflected[2] = normal[2]*2*d - viewer[2];

		st[0] = 0.5 - reflected[1] * 0.5;
		st[1] = 0.5 + reflected[2] * 0.5;
	}
}


/*
========================
RB_CalcEnvironmentTexCoordsFP

Special version for first-person models, borrowed from OpenArena
========================
*/
void RB_CalcEnvironmentTexCoordsFP( float *st, int screenMap ) {
	if ( R_STVEFEnvironment() ) {
		// Elite Force has one environment mapping, first person or not.
		RB_CalcEnvironmentTexCoordsSTVEF( st );
		return;
	}

	int			i;
	const float	*v, *normal;
	vec3_t		viewer, reflected, where, why, who; // what
	float		d; 

	if ( !backEnd.currentEntity || ( backEnd.currentEntity->e.renderfx & RF_FIRST_PERSON ) == 0 )
	{
		RB_CalcEnvironmentTexCoords( st );
		return;
	}

	if ( screenMap && backEnd.viewParms.frameSceneNum == 1 ) {
		RB_CalcEnvironmentTexCoordsFPscr( st );
		return;
	}

	v = tess.xyz[0];
	normal = tess.normal[0];

	for ( i = 0 ; i < tess.numVertexes ; i++, v += 4, normal += 4, st += 2 ) 
	{
		//VectorSubtract( backEnd.or.axis[0], v, what );
		VectorSubtract( backEnd.or.axis[1], v, why );
		VectorSubtract( backEnd.or.axis[2], v, who );

		VectorSubtract( backEnd.or.origin, v, where );
		VectorSubtract( backEnd.or.viewOrigin, v, viewer );

		VectorNormalizeFast( viewer );
		VectorNormalizeFast( where );
		//VectorNormalizeFast( what );
		VectorNormalizeFast( why );
		VectorNormalizeFast( who );

		d = DotProduct( normal, viewer );

		//reflected[0] = normal[0]*2*d - viewer[0] - (where[0] * 5) + (what[0] * 4);
		reflected[1] = normal[1]*2*d - viewer[1] - (where[1] * 5) + (why[1] * 4);
		reflected[2] = normal[2]*2*d - viewer[2] - (where[2] * 5) + (who[2] * 4);

		st[0] = 0.33 + reflected[1] * 0.33;
		st[1] = 0.33 - reflected[2] * 0.33;
	}
}


/*
** RB_CalcEnvironmentTexCoords
*/
void RB_CalcEnvironmentTexCoords( float *st )
{
	int			i;
	const float *v, *normal;
	vec3_t		viewer, reflected;
	float		d;

	if ( R_STVEFEnvironment() ) {
		RB_CalcEnvironmentTexCoordsSTVEF( st );
		return;
	}

	v = tess.xyz[0];
	normal = tess.normal[0];

	for (i = 0 ; i < tess.numVertexes ; i++, v += 4, normal += 4, st += 2 ) 
	{
		VectorSubtract (backEnd.or.viewOrigin, v, viewer);
		VectorNormalizeFast (viewer);

		d = DotProduct (normal, viewer);

		//reflected[0] = normal[0]*2*d - viewer[0];
		reflected[1] = normal[1]*2*d - viewer[1];
		reflected[2] = normal[2]*2*d - viewer[2];

		st[0] = 0.5 + reflected[1] * 0.5;
		st[1] = 0.5 - reflected[2] * 0.5;
	}
}


/*
** RB_CalcTurbulentTexCoords
*/
void RB_CalcTurbulentTexCoords( const waveForm_t *wf, float *src, float *dst )
{
	int i;
	double now; // -EC- set to double

	now = ( wf->phase + tess.shaderTime * wf->frequency );

	for ( i = 0; i < tess.numVertexes; i++, dst += 2, src += 2 )
	{
		dst[0] = src[0] + tr.sinTable[ ( ( int64_t ) ( ( ( tess.xyz[i][0] + tess.xyz[i][2] )* 1.0/128 * 0.125 + now ) * FUNCTABLE_SIZE ) ) & ( FUNCTABLE_MASK ) ] * wf->amplitude;
		dst[1] = src[1] + tr.sinTable[ ( ( int64_t ) ( ( tess.xyz[i][1] * 1.0/128 * 0.125 + now ) * FUNCTABLE_SIZE ) ) & ( FUNCTABLE_MASK ) ] * wf->amplitude;
	}
}


/*
** RB_CalcScaleTexCoords
*/
void RB_CalcScaleTexCoords( const float scale[2], float *src, float *dst )
{
	int i;

	for ( i = 0; i < tess.numVertexes; i++, dst += 2, src += 2 )
	{
		dst[0] = src[0] * scale[0];
		dst[1] = src[1] * scale[1];
	}
}


/*
** RB_CalcScrollTexCoords
*/
void RB_CalcScrollTexCoords( const float scrollSpeed[2], float *src, float *dst )
{
	int i;
	double	timeScale; // -EC-: set to double
	double	adjustedScrollS, adjustedScrollT; // -EC-: set to double

	timeScale = tess.shaderTime;

	adjustedScrollS = (double)scrollSpeed[0] * timeScale;
	adjustedScrollT = (double)scrollSpeed[1] * timeScale;

	// clamp so coordinates don't continuously get larger, causing problems
	// with hardware limits
	adjustedScrollS = adjustedScrollS - floor( adjustedScrollS );
	adjustedScrollT = adjustedScrollT - floor( adjustedScrollT );

	for ( i = 0; i < tess.numVertexes; i++, dst += 2, src += 2 )
	{
		dst[0] = src[0] + adjustedScrollS;
		dst[1] = src[1] + adjustedScrollT;
	}
}


/*
** RB_CalcTransformTexCoords
*/
void RB_CalcTransformTexCoords( const texModInfo_t *tmi, float *src, float *dst )
{
	int i;

	for ( i = 0; i < tess.numVertexes; i++, dst += 2, src += 2 )
	{
		const float s = src[0];
		const float t = src[1];

		dst[0] = s * tmi->matrix[0][0] + t * tmi->matrix[1][0] + tmi->translate[0];
		dst[1] = s * tmi->matrix[0][1] + t * tmi->matrix[1][1] + tmi->translate[1];
	}
}


/*
** RB_CalcRotateTexCoords
*/
void RB_CalcRotateTexCoords( float degsPerSecond, float *src, float *dst )
{
	double timeScale = tess.shaderTime; // -EC- set to double
	double degs; // -EC- set to double
	int64_t index;
	float sinValue, cosValue;
	texModInfo_t tmi;

	degs = -degsPerSecond * timeScale;
	index = degs * ( FUNCTABLE_SIZE / 360.0f );

	sinValue = tr.sinTable[ index & FUNCTABLE_MASK ];
	cosValue = tr.sinTable[ ( index + FUNCTABLE_SIZE / 4 ) & FUNCTABLE_MASK ];

	tmi.matrix[0][0] = cosValue;
	tmi.matrix[1][0] = -sinValue;
	tmi.translate[0] = 0.5 - 0.5 * cosValue + 0.5 * sinValue;

	tmi.matrix[0][1] = sinValue;
	tmi.matrix[1][1] = cosValue;
	tmi.translate[1] = 0.5 - 0.5 * sinValue - 0.5 * cosValue;

	RB_CalcTransformTexCoords( &tmi, src, dst );
}


/*
** RB_CalcSpecularAlpha
**
** Calculates specular coefficient and places it in the alpha channel
*/
vec3_t lightOrigin = { -960, 1980, 96 };		// FIXME: track dynamically

void RB_CalcSpecularAlpha( unsigned char *alphas ) {
	int			i;
	const float *v, *normal;
	vec3_t		viewer,  reflected;
	float		l, d;
	int			b;
	vec3_t		lightDir;
	int			numVertexes;

	v = tess.xyz[0];
	normal = tess.normal[0];

	alphas += 3;

	numVertexes = tess.numVertexes;
	for (i = 0 ; i < numVertexes ; i++, v += 4, normal += 4, alphas += 4) {
		float ilength;

		VectorSubtract( lightOrigin, v, lightDir );
//		ilength = Q_rsqrt( DotProduct( lightDir, lightDir ) );
		VectorNormalizeFast( lightDir );

		// calculate the specular color
		d = DotProduct (normal, lightDir);
//		d *= ilength;

		// we don't optimize for the d < 0 case since this tends to
		// cause visual artifacts such as faceted "snapping"
		reflected[0] = normal[0]*2*d - lightDir[0];
		reflected[1] = normal[1]*2*d - lightDir[1];
		reflected[2] = normal[2]*2*d - lightDir[2];

		VectorSubtract (backEnd.or.viewOrigin, v, viewer);
		ilength = Q_rsqrt( DotProduct( viewer, viewer ) );
		l = DotProduct (reflected, viewer);
		l *= ilength;

		if (l < 0) {
			b = 0;
		} else {
			l = l*l;
			l = l*l;
			b = l * 255;
			if (b > 255) {
				b = 255;
			}
		}

		*alphas = b;
	}
}


/*
** RB_CalcDiffuseColor
**
** The basic vertex lighting calc
*/
static void RB_CalcDiffuseColor_scalar( unsigned char *colors )
{
	int				i, j;
	float			*normal;
	float			incoming;
	const trRefEntity_t *ent;
	int				ambientLightInt;
	vec3_t			ambientLight;
	vec3_t			lightDir;
	vec3_t			directedLight;
	int				numVertexes;
	ent = backEnd.currentEntity;
	if ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active ) {
		for ( i = 0; i < tess.numVertexes; ++i )
			Q3IHV_LightDiffuse( tess.normal[i], ent->lightDir, ent->ambientLight, ent->directedLight, colors + i * 4 );
		return;
	}
	ambientLightInt = ent->ambientLightInt;
	VectorCopy( ent->ambientLight, ambientLight );
	VectorCopy( ent->directedLight, directedLight );
	VectorCopy( ent->lightDir, lightDir );

	normal = tess.normal[0];

	numVertexes = tess.numVertexes;
	for (i = 0 ; i < numVertexes ; i++, normal += 4) {
		incoming = DotProduct (normal, lightDir);
		if ( incoming <= 0 ) {
			*(int *)&colors[i*4] = ambientLightInt;
			R_CelQuantizeModelLighting( ent, &colors[i*4] );
			continue;
		}
		j = ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP
			? FnSTVEF_LightByte( ambientLight[0] + incoming * directedLight[0] )
			: myftol( ambientLight[0] + incoming * directedLight[0] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+0] = j;

		j = ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP
			? FnSTVEF_LightByte( ambientLight[1] + incoming * directedLight[1] )
			: myftol( ambientLight[1] + incoming * directedLight[1] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+1] = j;

		j = ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP
			? FnSTVEF_LightByte( ambientLight[2] + incoming * directedLight[2] )
			: myftol( ambientLight[2] + incoming * directedLight[2] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+2] = j;

		colors[i*4+3] = 255;
		R_CelQuantizeModelLighting( ent, &colors[i*4] );
	}
}


void RB_CalcDiffuseColor( unsigned char *colors )
{
	/* Alice and F.A.K.K.2 compile a static model into the world as vertex-lit
	 * triangles, and its shader's `rgbGen lightingDiffuse` then has no entity
	 * to take light from: fakk2.exe 1.02 calls no lighting function for the
	 * world entity and leaves whatever colours it last wrote (0x0047d45a).
	 * This engine lights such a surface with the colours the map baked on its
	 * vertices, as it does Medal of Honor's static models (AGENTS.md
	 * invariant 11); it had drawn their leaves, banners and lamps black. */
	if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) && backEnd.currentEntity == &tr.worldEntity ) {
		int i;
		for ( i = 0; i < tess.numVertexes; i++, colors += 4 ) {
			colors[0] = (byte)( tess.vertexColors[i].rgba[0] * tr.identityLight );
			colors[1] = (byte)( tess.vertexColors[i].rgba[1] * tr.identityLight );
			colors[2] = (byte)( tess.vertexColors[i].rgba[2] * tr.identityLight );
			colors[3] = 255;
		}
		return;
	}
	RB_CalcDiffuseColor_scalar( colors );
}


/*
====================================================================

AMERICAN MCGEE'S ALICE

Colour, alpha and texture-coordinate sources of Alice's shader dialect
(see code/renderercommon/tr_alice_shader.h), with the retail alice.exe
back-end routines they follow.

====================================================================
*/

static byte RB_AliceByte( float value ) {
	if ( !( value > 0.0f ) ) return 0;
	if ( value >= 255.0f ) return 255;
	return (byte)value;
}

/*
** RB_AliceCalcGlobalColor
**
** `rgbGen global`: the last SetColor's colour, its red, green and blue
** scaled by identityLight (alice.exe ComputeColors 0x00493680, rgbGen 15;
** R_SetColor 0x00498b80 stores them times identityLightByte).
*/
void RB_AliceCalcGlobalColor( unsigned char *dstColors ) {
	color4ub_t color;
	uint32_t *colors = ( uint32_t * ) dstColors;
	int i;

	color.rgba[0] = RB_AliceByte( backEnd.color2D.rgba[0] * tr.identityLight );
	color.rgba[1] = RB_AliceByte( backEnd.color2D.rgba[1] * tr.identityLight );
	color.rgba[2] = RB_AliceByte( backEnd.color2D.rgba[2] * tr.identityLight );
	color.rgba[3] = backEnd.color2D.rgba[3];
	for ( i = 0; i < tess.numVertexes; i++ ) {
		colors[i] = color.u32;
	}
}

/*
** RB_AliceCalcGlobalAlpha
**
** `alphaGen global`: the last SetColor's alpha (the byte at 0x00e9ce13).
*/
void RB_AliceCalcGlobalAlpha( unsigned char *dstColors ) {
	int i;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 ) {
		dstColors[3] = backEnd.color2D.rgba[3];
	}
}

/*
** RB_AliceCalcColorWave
**
** `rgbGen colorwave r g b <wave>`: the wave's value, as rgbGen wave computes
** it and clamped to one, times the constant colour; alpha 255 (alice.exe
** 0x0049392a passes the stage's constant colour to 0x0049e5c0).
*/
void RB_AliceCalcColorWave( const waveForm_t *wf, const color4ub_t constant, unsigned char *dstColors ) {
	uint32_t *colors = ( uint32_t * ) dstColors;
	color4ub_t color;
	float glow;
	int i;

	if ( wf->func == GF_NOISE ) {
		glow = wf->base + R_NoiseGet4f( 0, 0, 0, ( tess.shaderTime + wf->phase ) * wf->frequency ) * wf->amplitude;
	} else {
		glow = EvalWaveForm( wf ) * tr.identityLight;
	}
	glow = glow < 0.0f ? 0.0f : glow > 1.0f ? 1.0f : glow;

	color.rgba[0] = RB_AliceByte( glow * constant.rgba[0] );
	color.rgba[1] = RB_AliceByte( glow * constant.rgba[1] );
	color.rgba[2] = RB_AliceByte( glow * constant.rgba[2] );
	color.rgba[3] = 255;
	for ( i = 0; i < tess.numVertexes; i++ ) {
		colors[i] = color.u32;
	}
}

/*
** RB_AliceCalcDotAlpha
**
** `alphaGen dot [min [max]]` and `oneMinusDot`: with v the unit vector from
** the vertex to the viewer and n its normal, dot scales max( n.v, 0 ) and
** oneMinusDot min( ( 1 - n.v )^2, 1 ) into [min, max] (alice.exe 0x0049e370,
** 0x0049e450). Retail stores the low byte unclamped; this clamps, which no
** shipped range reaches.
*/
void RB_AliceCalcDotAlpha( const shaderStage_t *stage, unsigned char *dstColors ) {
	const float *xyz = tess.xyz[0];
	const float *normal = tess.normal[0];
	const float lo = stage->aliceDotRange[0];
	const float hi = stage->aliceDotRange[1];
	// the Vulkan family keeps a stage's alpha source on its bundle 0
	const qboolean oneMinus = stage->bundle[0].alphaGen == AGEN_ALICE_ONE_MINUS_DOT ? qtrue : qfalse;
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
		dstColors[3] = RB_AliceByte( ( lo + x * ( hi - lo ) ) * 255.0f );
	}
}

/*
** RB_AliceCalcSkyAlpha
**
** `alphaGen skyAlpha` and `oneMinusSkyAlpha`: the scene's sky alpha
** (alice.exe 0x00493b82, refdef +0x188).
*/
void RB_AliceCalcSkyAlpha( qboolean oneMinus, unsigned char *dstColors ) {
	float alpha = backEnd.refdef.alice.skyAlpha;
	byte value;
	int i;

	alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	value = RB_AliceByte( ( oneMinus ? 1.0f - alpha : alpha ) * 255.0f );
	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 ) {
		dstColors[3] = value;
	}
}

/*
** RB_AliceCalcSpecularAlpha
**
** `alphaGen lightingSpecular <scale> <x> <y> <z>`: Quake III's reflection
** term with the stage's own light origin, ( r.v )^4 times the scale, capped
** at the scale (alice.exe 0x004a0520).
*/
void RB_AliceCalcSpecularAlpha( const shaderStage_t *stage, unsigned char *dstColors ) {
	const float *xyz = tess.xyz[0];
	const float *normal = tess.normal[0];
	const float scale = stage->aliceSpecular[0];
	int i;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4, dstColors += 4 ) {
		vec3_t lightDir, reflected, viewer;
		float d, l;

		VectorSubtract( &stage->aliceSpecular[1], xyz, lightDir );
		VectorNormalizeFast( lightDir );
		d = DotProduct( normal, lightDir );
		reflected[0] = normal[0] * 2 * d - lightDir[0];
		reflected[1] = normal[1] * 2 * d - lightDir[1];
		reflected[2] = normal[2] * 2 * d - lightDir[2];
		VectorSubtract( backEnd.or.viewOrigin, xyz, viewer );
		l = DotProduct( reflected, viewer ) * Q_rsqrt( DotProduct( viewer, viewer ) );
		if ( l < 0.0f ) {
			l = 0.0f;
		}
		l = l * l;
		l = l * l;
		l *= scale;
		dstColors[3] = RB_AliceByte( l < scale ? l : scale );
	}
}

/*
** RB_AliceCalcTexMod
**
** Alice's texture modifiers (alice.exe ComputeTexCoords 0x00493d10):
** `fromEntity` scroll, rotate and offset read the entity's two shader-data
** shorts as speed / 2048, degrees per second / 64 and offset / 16384
** (0x0049f720, 0x0049f850, 0x0049f410); parallax shifts s and t by the
** viewer's x and y times its rates (0x0049f490); macro projects the vertex
** onto the base axes Quake III's map compiler picks for its normal, over the
** first stage's first image (0x0049f620, table 0x00528b60).
*/
static const vec3_t aliceBaseAxis[18] = {
	{ 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 },		// floor
	{ 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 },	// ceiling
	{ 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 },		// west wall
	{ -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 },	// east wall
	{ 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 },		// south wall
	{ 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }		// north wall
};

static float RB_AliceEntityShaderData( int index, float scale ) {
	if ( !backEnd.currentEntity ) {
		return 0.0f;
	}
	return backEnd.currentEntity->e.alice.shaderData[index] / scale;
}

void RB_AliceCalcTexMod( const texModInfo_t *tmi, float *src, float *dst ) {
	int i;

	switch ( tmi->type ) {
	case TMOD_ALICE_SCROLL: {
		float speed[2];
		speed[0] = tmi->scroll[0] == ALICE_SHADER_FROM_ENTITY ? RB_AliceEntityShaderData( 0, 2048.0f ) : tmi->scroll[0];
		speed[1] = tmi->scroll[1] == ALICE_SHADER_FROM_ENTITY ? RB_AliceEntityShaderData( 1, 2048.0f ) : tmi->scroll[1];
		RB_CalcScrollTexCoords( speed, src, dst );
		break;
	}
	case TMOD_ALICE_ROTATE:
		RB_CalcRotateTexCoords( RB_AliceEntityShaderData( 0, 64.0f ), src, dst );
		break;
	case TMOD_ALICE_OFFSET: {
		const float s = tmi->offset[0] == ALICE_SHADER_FROM_ENTITY ? RB_AliceEntityShaderData( 0, 16384.0f ) : tmi->offset[0];
		const float t = tmi->offset[1] == ALICE_SHADER_FROM_ENTITY ? RB_AliceEntityShaderData( 1, 16384.0f ) : tmi->offset[1];
		for ( i = 0; i < tess.numVertexes; i++ ) {
			dst[i * 2 + 0] = src[i * 2 + 0] + s;
			dst[i * 2 + 1] = src[i * 2 + 1] + t;
		}
		break;
	}
	case TMOD_ALICE_PARALLAX: {
		const float s = tmi->scale[0] * backEnd.refdef.vieworg[0];
		const float t = tmi->scale[1] * backEnd.refdef.vieworg[1];
		for ( i = 0; i < tess.numVertexes; i++ ) {
			dst[i * 2 + 0] = src[i * 2 + 0] + s;
			dst[i * 2 + 1] = src[i * 2 + 1] + t;
		}
		break;
	}
	case TMOD_ALICE_MACRO: {
		const image_t *image = tess.shader && tess.shader->numUnfoggedPasses > 0 && tess.shader->stages[0] ?
			tess.shader->stages[0]->bundle[0].image[0] : NULL;
		const float width = image && image->uploadWidth > 0 ? (float)image->uploadWidth : 1.0f;
		const float height = image && image->uploadHeight > 0 ? (float)image->uploadHeight : 1.0f;
		const float *xyz = tess.xyz[0];
		const float *normal = tess.normal[0];
		for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 ) {
			float best = -2.0f;
			int axis = 0, j;
			for ( j = 0; j < 6; j++ ) {
				const float d = DotProduct( normal, aliceBaseAxis[j * 3] );
				if ( d > best ) {
					best = d;
					axis = j;
				}
			}
			dst[i * 2 + 0] = DotProduct( xyz, aliceBaseAxis[axis * 3 + 1] ) * tmi->scale[0] / width;
			dst[i * 2 + 1] = DotProduct( xyz, aliceBaseAxis[axis * 3 + 2] ) * tmi->scale[1] / height;
		}
		break;
	}
	default:
		break;
	}
}
