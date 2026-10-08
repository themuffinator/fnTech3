/*
===========================================================================
Copyright (C) 2026 FnQL contributors

This file is part of FnQL.

FnQL is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.
===========================================================================
*/

/*
 * Behavioral gate for the capsule-vs-capsule sweep.  This links the real
 * cm_trace.c against a stub collision model that only owns the temporary
 * capsule bounds, so the QL geometry and the debug trace-plane invariant are
 * exercised rather than merely described.  A source-shaped test cannot catch
 * a capsule profile that is self-consistent but geometrically wrong.
 */

#include "cm_local.h"
#include "cm_trace_contract.h"
#include "cm_mohaa_terrain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ harness */

static int failures;

static void Check( int condition, const char *expression, int line )
{
	if ( condition ) {
		return;
	}
	fprintf( stderr, "line %d: check failed: %s\n", line, expression );
	++failures;
}

#define CHECK( expression ) Check( ( expression ) ? 1 : 0, #expression, __LINE__ )

/* --------------------------------------------- collision model stubs */

clipMap_t	cm;
int			c_pointcontents;
int			c_traces, c_brush_traces, c_patch_traces, c_totalPatchTraces;
cvar_t		*cm_noAreas, *cm_noCurves, *cm_playerCurveClip;

static cvar_t	stubCvars[8];
static int		stubCvarCount;
static cmodel_t	stubModel;
static comGameProfile_t stubGameProfile;

const comGameProfile_t *Com_GameProfile( void )
{
	return &stubGameProfile;
}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags )
{
	int i;

	for ( i = 0; i < stubCvarCount; ++i ) {
		if ( !strcmp( stubCvars[i].name, var_name ) ) {
			return &stubCvars[i];
		}
	}
	if ( stubCvarCount == (int)( sizeof( stubCvars ) / sizeof( stubCvars[0] ) ) ) {
		fprintf( stderr, "stub cvar table overflow: %s\n", var_name );
		exit( EXIT_FAILURE );
	}
	stubCvars[stubCvarCount].name = (char *)var_name;
	stubCvars[stubCvarCount].string = (char *)value;
	stubCvars[stubCvarCount].value = (float)atof( value );
	stubCvars[stubCvarCount].integer = atoi( value );
	(void)flags;
	return &stubCvars[stubCvarCount++];
}

void QDECL Com_Error( errorParm_t level, const char *fmt, ... )
{
	(void)level;
	fprintf( stderr, "unexpected Com_Error: %s\n", fmt );
	exit( EXIT_FAILURE );
}

void QDECL Com_Printf( const char *fmt, ... )
{
	(void)fmt;
}

cmodel_t *CM_ClipHandleToModel( clipHandle_t handle )
{
	(void)handle;
	return &stubModel;
}

void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs )
{
	const cmodel_t *cmod = CM_ClipHandleToModel( model );

	VectorCopy( cmod->mins, mins );
	VectorCopy( cmod->maxs, maxs );
}

clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int capsule )
{
	VectorCopy( mins, stubModel.mins );
	VectorCopy( maxs, stubModel.maxs );
	return CM_TemporaryModelHandle( capsule ? qtrue : qfalse );
}

void CM_BoxLeafnums_r( leafList_t *ll, int nodenum )
{
	(void)ll;
	(void)nodenum;
}

void CM_StoreLeafs( leafList_t *ll, int nodenum )
{
	(void)ll;
	(void)nodenum;
}

qboolean CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs,
	const vec3_t mins2, const vec3_t maxs2 )
{
	if ( maxs[0] < mins2[0] - SURFACE_CLIP_EPSILON
		|| maxs[1] < mins2[1] - SURFACE_CLIP_EPSILON
		|| maxs[2] < mins2[2] - SURFACE_CLIP_EPSILON
		|| mins[0] > maxs2[0] + SURFACE_CLIP_EPSILON
		|| mins[1] > maxs2[1] + SURFACE_CLIP_EPSILON
		|| mins[2] > maxs2[2] + SURFACE_CLIP_EPSILON ) {
		return qfalse;
	}
	return qtrue;
}

void CM_TraceThroughPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc )
{
	(void)tw;
	(void)pc;
}

qboolean CM_PositionTestInPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc )
{
	(void)tw;
	(void)pc;
	return qfalse;
}

/* Medal of Honor's terrain; this stub world has none. */
void CM_MOHAA_TraceThroughTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc )
{
	(void)tw;
	(void)tc;
}

qboolean CM_MOHAA_PositionTestInTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc )
{
	(void)tw;
	(void)tc;
	return qfalse;
}

/* ----------------------------------------------------------- fixtures */

/* retail QL player hull */
static const vec3_t playerMins = { -15.0f, -15.0f, -24.0f };
static const vec3_t playerMaxs = { 15.0f, 15.0f, 32.0f };

#define PLAYER_CLIP_MASK ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY )

static void CapsuleSweep( trace_t *trace, const vec3_t start, const vec3_t end )
{
	CM_BoxTrace( trace, start, end, playerMins, playerMaxs,
		CM_TemporaryModelHandle( qtrue ), PLAYER_CLIP_MASK, qtrue );
}

static float SweepFraction( float sx, float sy, float sz, float ex, float ey, float ez )
{
	trace_t trace;
	vec3_t start, end;

	VectorSet( start, sx, sy, sz );
	VectorSet( end, ex, ey, ez );
	CapsuleSweep( &trace, start, end );
	return trace.fraction;
}

static qboolean NearlyEqual( float value, float expected )
{
	const float delta = value - expected;

	return ( delta > -0.001f && delta < 0.001f ) ? qtrue : qfalse;
}

/* --------------------------------------------------------------- tests */

/*
 * Q3 uses the inherited expanded capsule radius (30 for this fixture).  QL
 * alone applies sv_cylinderScale, making the default radius 33.  Switching the
 * test stub back after the QL cvar exists proves that the cvar cannot select
 * QL collision behavior by itself.
 */
static void TestGameProfileOwnsCylinderScale( void )
{
	cvar_t *cylinderScale;
	float quake3Fraction;
	float quakeLiveFraction;
	int cvarsBeforeTrace;
	int i;

	stubGameProfile.id = COM_GAME_QUAKE3;
	cvarsBeforeTrace = stubCvarCount;
	quake3Fraction = SweepFraction( 80, 0, 0, 20, 0, 0 );
	CHECK( NearlyEqual( quake3Fraction, 0.816667f ) );
	CHECK( stubCvarCount == cvarsBeforeTrace );

	stubGameProfile.id = COM_GAME_QUAKELIVE;
	quakeLiveFraction = SweepFraction( 80, 0, 0, 20, 0, 0 );
	CHECK( NearlyEqual( quakeLiveFraction, 0.766667f ) );
	CHECK( stubCvarCount == cvarsBeforeTrace + 1 );
	CHECK( quakeLiveFraction < quake3Fraction );

	cylinderScale = NULL;
	for ( i = 0; i < stubCvarCount; ++i ) {
		if ( !strcmp( stubCvars[i].name, "sv_cylinderScale" ) ) {
			cylinderScale = &stubCvars[i];
			break;
		}
	}
	CHECK( cylinderScale != NULL );
	if ( !cylinderScale ) {
		return;
	}

	cylinderScale->value = 2.0f;
	CHECK( NearlyEqual( SweepFraction( 80, 0, 0, 20, 0, 0 ), 0.316667f ) );

	stubGameProfile.id = COM_GAME_QUAKE3;
	CHECK( NearlyEqual( SweepFraction( 80, 0, 0, 20, 0, 0 ), 0.816667f ) );

	cylinderScale->value = 1.1f;
	stubGameProfile.id = COM_GAME_QUAKELIVE;
}

/*
 * A capsule target is a cylinder capped by a sphere at each end.  Every
 * approach that reaches the hull has to be blocked, including the ones that
 * only the cap spheres can catch.  An engine that models the target as one
 * cylinder plus a single head sphere passes straight through from below and
 * at foot level, which is the regression this gate exists for.
 */
static void TestEveryApproachIsBlocked( void )
{
	CHECK( NearlyEqual( SweepFraction( 80, 0, 0, 20, 0, 0 ), 0.766667f ) );
	CHECK( NearlyEqual( SweepFraction( 0, 0, 200, 0, 0, 20 ), 0.794445f ) );
	CHECK( NearlyEqual( SweepFraction( 0, 0, -200, 0, 0, -20 ), 0.794445f ) );
	CHECK( NearlyEqual( SweepFraction( 90, 0, 90, 0, 0, 30 ), 0.735575f ) );
	CHECK( NearlyEqual( SweepFraction( 90, 0, -90, 0, 0, -30 ), 0.735575f ) );
	CHECK( NearlyEqual( SweepFraction( 90, 0, 56, 0, 0, 56 ), 0.913220f ) );
	CHECK( NearlyEqual( SweepFraction( 90, 0, -30, 0, 0, -30 ), 0.658436f ) );

	/* a sweep that clears the expanded radius still misses */
	CHECK( SweepFraction( 100, 90, 0, -100, 90, 0 ) == 1.0f );
	CHECK( SweepFraction( 0, 0, 400, 0, 0, 300 ) == 1.0f );
}

/*
 * The cap spheres answer with an outward radial plane, so a sweep from below
 * must be pushed down and a sweep from above must be pushed up.
 */
static void TestCapContactsFaceTheSweep( void )
{
	trace_t above, below;
	vec3_t start, end;

	VectorSet( start, 0, 0, 200 );
	VectorSet( end, 0, 0, 20 );
	CapsuleSweep( &above, start, end );

	VectorSet( start, 0, 0, -200 );
	VectorSet( end, 0, 0, -20 );
	CapsuleSweep( &below, start, end );

	CHECK( above.plane.normal[2] > 0.99f );
	CHECK( below.plane.normal[2] < -0.99f );
	CHECK( above.fraction < 1.0f && below.fraction < 1.0f );
}

/*
 * QL reports a capsule contact as CONTENTS_BODY only; the trace layer adds no
 * head-hit metadata of its own, and 0x0400 stays unallocated.
 */
static void TestContactContentsStayRetailShaped( void )
{
	trace_t trace;
	vec3_t start, end;

	VectorSet( start, 0, 0, 200 );
	VectorSet( end, 0, 0, 20 );
	CapsuleSweep( &trace, start, end );

	CHECK( trace.fraction < 1.0f );
	CHECK( trace.contents == CONTENTS_BODY );
}

/*
 * Position tests and zero-length sweeps must stay startsolid-shaped rather
 * than reporting a blocked fractional move.
 */
static void TestOverlapReportsStartSolid( void )
{
	trace_t trace;
	vec3_t start, end;

	VectorSet( start, 0, 0, 0 );
	VectorSet( end, 0, 0, 0 );
	CapsuleSweep( &trace, start, end );

	CHECK( trace.startsolid == qtrue );
	CHECK( trace.fraction == 0.0f );
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );
}

/*
 * The analytic capsule plane is scaled by 1/(radius + RADIUS_EPSILON) in
 * single precision, so it drifts off unit length as the sweep start moves
 * away from the target.  Sweeping the whole world extent must still satisfy
 * the invariant the debug build asserts.
 */
static void TestPlaneContractHoldsAcrossTheWorld( void )
{
	static const float ranges[] = { 120.0f, 4000.0f, 65536.0f };
	unsigned int rng = 0x1234567u;
	unsigned int range;
	int violations = 0;
	int i;

	for ( range = 0; range < sizeof( ranges ) / sizeof( ranges[0] ); ++range ) {
		for ( i = 0; i < 20000; ++i ) {
			trace_t trace;
			vec3_t start, end;
			float axis[6];
			int axisIndex;

			for ( axisIndex = 0; axisIndex < 6; ++axisIndex ) {
				rng = rng * 1664525u + 1013904223u;
				axis[axisIndex] = ( (float)( ( rng >> 8 ) & 0xFFFFFF )
					/ (float)0x1000000 - 0.5f ) * 2.0f;
			}

			VectorSet( start, axis[0] * ranges[range], axis[1] * ranges[range],
				axis[2] * ranges[range] * 0.15f );
			VectorSet( end, axis[3] * 60.0f, axis[4] * 60.0f, axis[5] * 60.0f );
			if ( VectorCompare( start, end ) ) {
				continue;
			}

			CapsuleSweep( &trace, start, end );
			if ( !CM_TraceResultHasValidPlaneContract( &trace,
					CM_TRACE_PLANE_ANALYTIC ) ) {
				++violations;
			}
		}
	}

	CHECK( violations == 0 );
}

/*
 * A 128-unit cube in the stub model's leaf, for the brush sweep: Q3Test's
 * sweep that starts and ends inside a brush writes only the solid flags and
 * fraction, keeping the contents an earlier brush wrote (1.08
 * CM_TraceThroughBrush 0x0040eeb4..0x0040eec9), where Quake III writes the
 * brush's. A sweep into the brush and a position test write them in both.
 */
static cplane_t		cubePlanes[6];
static cbrushside_t	cubeSides[6];
static cbrush_t		cubeBrush;
static int			cubeLeafBrushes[1];

static void BrushTrace( trace_t *trace, float sx, float ex )
{
	const vec3_t point = { 0.0f, 0.0f, 0.0f };
	vec3_t start, end;

	VectorSet( start, sx, 0, 0 );
	VectorSet( end, ex, 0, 0 );
	CM_BoxTrace( trace, start, end, point, point, 1, CONTENTS_SOLID, qfalse );
}

static void BuildCube( void )
{
	int i;

	for ( i = 0; i < 6; ++i ) {
		cplane_t *plane = &cubePlanes[i];

		VectorClear( plane->normal );
		plane->normal[i >> 1] = ( i & 1 ) ? -1.0f : 1.0f;
		plane->dist = 64.0f;
		plane->type = ( i & 1 ) ? PLANE_NON_AXIAL : i >> 1;
		SetPlaneSignbits( plane );
		cubeSides[i].plane = plane;
	}
	cubeBrush.contents = CONTENTS_SOLID;
	VectorSet( cubeBrush.bounds[0], -64, -64, -64 );
	VectorSet( cubeBrush.bounds[1], 64, 64, 64 );
	cubeBrush.numsides = 6;
	cubeBrush.sides = cubeSides;
	cm.brushes = &cubeBrush;
	cm.numBrushes = 1;
	cm.leafbrushes = cubeLeafBrushes;
	cm.numLeafBrushes = 1;
	stubModel.leaf.firstLeafBrush = 0;
	stubModel.leaf.numLeafBrushes = 1;
}

static void ClearCube( void )
{
	stubModel.leaf.numLeafBrushes = 0;
	cm.brushes = NULL;
	cm.numBrushes = 0;
	cm.leafbrushes = NULL;
	cm.numLeafBrushes = 0;
}

static void TestQ3TestBrushSolidKeepsContents( void )
{
	trace_t trace;

	BuildCube();
	cm.brushSolidKeepsContents = qfalse;
	BrushTrace( &trace, 0, 10 );
	CHECK( trace.allsolid && trace.startsolid && trace.fraction == 0.0f );
	CHECK( trace.contents == CONTENTS_SOLID );

	cm.brushSolidKeepsContents = qtrue;
	BrushTrace( &trace, 0, 10 );
	CHECK( trace.allsolid && trace.startsolid && trace.fraction == 0.0f );
	CHECK( trace.contents == 0 );
	BrushTrace( &trace, 100, 0 );
	CHECK( !trace.startsolid && trace.fraction < 1.0f && trace.contents == CONTENTS_SOLID );
	BrushTrace( &trace, 0, 0 );
	CHECK( trace.allsolid && trace.contents == CONTENTS_SOLID );

	cm.brushSolidKeepsContents = qfalse;
	ClearCube();
}

/*
 * RTCW SP traces its temporary capsule model as the box CM_TempBoxModel
 * builds for it too, with the capsule hull off (WolfSP.exe CM_Trace
 * 0x41a77b..0x41a7ae). The cube stands in for that box here: the sweep stops
 * on its face, 79 units out for this hull, where the capsule-vs-capsule sweep
 * stops on the player capsule.
 */
static void TestRTCWSPTracesTemporaryModelsAsBoxes( void )
{
	const float boxFraction = ( 200.0f - 79.0f - (float)SURFACE_CLIP_EPSILON ) / 200.0f;
	trace_t trace;
	vec3_t start, end;

	BuildCube();
	cm.tempModelsAreBoxes = qtrue;
	CHECK( NearlyEqual( SweepFraction( 200, 0, 0, 0, 0, 0 ), boxFraction ) );
	VectorSet( start, 70, 0, 0 );
	VectorCopy( start, end );
	CapsuleSweep( &trace, start, end );
	CHECK( trace.startsolid && trace.allsolid && trace.contents == CONTENTS_SOLID );

	cm.tempModelsAreBoxes = qfalse;
	CHECK( SweepFraction( 200, 0, 0, 0, 0, 0 ) > boxFraction + 0.1f );
	ClearCube();
}

/*
 * A point traced against a capsule model sweeps the capsule against a box of
 * the point's size instead (CM_TraceBoundingBoxThroughCapsule); the cube
 * stands in for that box. 70 units out the point misses the cube, but the
 * player capsule, 15 across, reaches it: RTCW MP's server stops the trace
 * there, where the trace's own bounds, a line, had passed the cube over.
 */
static void TestPointSweepReachesWhatTheCapsuleReaches( void )
{
	const vec3_t point = { 0.0f, 0.0f, 0.0f };
	const float expected = ( 196.0f - 13.0f - 79.0f - (float)SURFACE_CLIP_EPSILON ) / 400.0f;
	trace_t trace;
	vec3_t start, end;

	BuildCube();
	VectorCopy( playerMins, stubModel.mins );
	VectorCopy( playerMaxs, stubModel.maxs );
	VectorSet( start, 70, 0, 200 );
	VectorSet( end, 70, 0, -200 );
	CM_BoxTrace( &trace, start, end, point, point, CM_TemporaryModelHandle( qtrue ), CONTENTS_SOLID, qfalse );
	CHECK( NearlyEqual( trace.fraction, expected ) );
	CHECK( trace.plane.normal[2] > 0.99f );

	VectorSet( start, 100, 0, 200 );
	VectorSet( end, 100, 0, -200 );
	CM_BoxTrace( &trace, start, end, point, point, CM_TemporaryModelHandle( qtrue ), CONTENTS_SOLID, qfalse );
	CHECK( trace.fraction == 1.0f );
	ClearCube();
}

int main( void )
{
	cm.numNodes = 1;
	stubGameProfile.id = COM_GAME_QUAKELIVE;
	stubGameProfile.collisionModelHandleCount = 256;
	cm_noCurves = Cvar_Get( "cm_noCurves", "0", 0 );
	cm_playerCurveClip = Cvar_Get( "cm_playerCurveClip", "1", 0 );
	VectorCopy( playerMins, stubModel.mins );
	VectorCopy( playerMaxs, stubModel.maxs );

	TestGameProfileOwnsCylinderScale();
	TestEveryApproachIsBlocked();
	TestCapContactsFaceTheSweep();
	TestContactContentsStayRetailShaped();
	TestOverlapReportsStartSolid();
	TestPlaneContractHoldsAcrossTheWorld();
	TestQ3TestBrushSolidKeepsContents();
	{
		const comGameId_t games[] = { COM_GAME_RTCW_SP, COM_GAME_RTCW_MP };
		unsigned i;
		for ( i = 0; i < sizeof( games ) / sizeof( games[0] ); ++i ) {
			stubGameProfile.id = games[i];
			stubGameProfile.collisionModelHandleCount = 512;
			CHECK( CM_TemporaryModelHandle( qfalse ) == 511 );
			CHECK( CM_TemporaryModelHandle( qtrue ) == 510 );
			CHECK( NearlyEqual( SweepFraction( 80, 0, 0, 20, 0, 0 ), 0.816667f ) );
			TestCapContactsFaceTheSweep();
			TestContactContentsStayRetailShaped();
			TestOverlapReportsStartSolid();
			TestPlaneContractHoldsAcrossTheWorld();
		}
		TestRTCWSPTracesTemporaryModelsAsBoxes();
		TestPointSweepReachesWhatTheCapsuleReaches();
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
