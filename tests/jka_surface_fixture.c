/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Draws tests/jka_surface_cases.h through one renderer's Jedi Academy
 * primitives (renderercommon/tr_jka_surface.h), with that renderer's own
 * quad stamp and the shared Elite Force helpers, and prints what they leave
 * in tess in the form tests/fixtures/jka_surface_retail.json holds.
 * `jka_surface_fixture <base>` prints the cases with <base> vertices already
 * batched; `jka_surface_fixture bounds` checks the batch flush and the
 * sprite bound. */
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <float.h>
enum { RT_STVEF_ORIENTEDSPRITE = 100, RT_STVEF_ALPHAVERTPOLY, RT_STVEF_LINE, RT_STVEF_ORIENTEDLINE,
	RT_STVEF_LINE2, RT_STVEF_BEZIER, RT_STVEF_CYLINDER, RT_STVEF_ELECTRICITY,
	RT_JKA_SABER_GLOW = 200, RT_JKA_LINE, RT_JKA_ORIENTEDLINE, SF_TRIANGLES = 1 };
typedef struct {
	int reType; vec3_t axis[3], origin, oldorigin; float radius, rotation; color4ub_t shader;
	struct { float width, width2, stscale, height, bias, deviation; int wrap, taper;
		vec3_t control1, control2; color4ub_t vertexColors[4]; } stvef;
	struct { float saberLength, lineWidth, lineSTScale; } jka;
} refEntity_t;
static struct { refEntity_t e; } entity;
typedef struct { struct { vec3_t origin, axis[3]; } or; int mirror; } View;
static struct { View viewParms; struct { refEntity_t e; } *currentEntity; } backEnd;
#define TESS_VERTEXES 1024
#define TESS_INDEXES 1536
static struct { int surfType, numVertexes, numIndexes; unsigned indexes[TESS_INDEXES];
	float xyz[TESS_VERTEXES][4], normal[TESS_VERTEXES][4], texCoords[2][TESS_VERTEXES][2];
	color4ub_t vertexColors[TESS_VERTEXES]; } tess;
const vec3_t vec3_origin = { 0, 0, 0 };
static qboolean R_ViewPassIsMirror( const View *v ) { return v->mirror ? qtrue : qfalse; }
static int flushes;
static void CheckIndexes( void ) {
	int i;
	assert( tess.numVertexes >= 0 && tess.numVertexes <= TESS_VERTEXES );
	assert( tess.numIndexes >= 0 && tess.numIndexes <= TESS_INDEXES );
	for ( i = 0; i < tess.numIndexes; ++i ) assert( tess.indexes[i] < (unsigned)tess.numVertexes );
}
static void CheckOverflow( int vertices, int indexes ) {
	assert( vertices >= 0 && vertices <= TESS_VERTEXES && indexes >= 0 && indexes <= TESS_INDEXES );
	if ( tess.numVertexes + vertices > TESS_VERTEXES || tess.numIndexes + indexes > TESS_INDEXES ) {
		CheckIndexes(); ++flushes; memset( &tess, 0, sizeof( tess ) );
	}
}
#define RB_CHECKOVERFLOW( v, i ) CheckOverflow( v, i )
#include "tests/jka_surface_cases.h"
static unsigned randomCalls;
static int FixtureRand( void ) { ++randomCalls; return JKASurfaceRandom(); }
#define rand() FixtureRand()
/* The engine's math, this renderer's quad stamp and both shared effect
 * headers are inserted here. */
#include "jka_surface_implementation.inc"

static void Load( int index, jkaSurfaceCase_t *c ) {
	refEntity_t *e = &entity.e;
	JKASurfaceCase( index, c );
	memset( e, 0, sizeof( *e ) );
	e->reType = c->type == 5 ? RT_JKA_SABER_GLOW : c->type == 8 ? RT_JKA_LINE : RT_JKA_ORIENTEDLINE;
	memcpy( e->axis, c->axis, sizeof( e->axis ) );
	VectorCopy( c->origin, e->origin );
	VectorCopy( c->oldorigin, e->oldorigin );
	memcpy( e->shader.rgba, c->rgba, 4 );
	e->radius = c->radius;
	e->jka.saberLength = c->saberLength;
	e->jka.lineWidth = c->lineWidth;
	e->jka.lineSTScale = c->lineSTScale;
	VectorCopy( c->viewOrigin, backEnd.viewParms.or.origin );
	memcpy( backEnd.viewParms.or.axis, c->viewAxis, sizeof( c->viewAxis ) );
	backEnd.viewParms.mirror = c->mirror;
	randomCalls = 0;
	jkaSurfaceRandomIndex = 0;
}

static int Bounds( void );

int main( int argc, char **argv ) {
	int index, i, k, base;
	backEnd.currentEntity = (void *)&entity;
	if ( argc > 1 && !strcmp( argv[1], "bounds" ) ) return Bounds();
	base = argc > 1 ? atoi( argv[1] ) : 0;
	assert( base >= 0 && base < 16 );
	printf( "[\n" );
	for ( index = 0; index < JKA_SURFACE_CASES; ++index ) {
		jkaSurfaceCase_t c;
		Load( index, &c );
		memset( &tess, 0, sizeof( tess ) );
		flushes = 0;
		tess.numVertexes = tess.numIndexes = base;
		for ( i = 0; i < base; ++i ) { tess.xyz[i][0] = -999; tess.indexes[i] = i; }
		assert( RB_SurfaceJKA() );
		CheckIndexes();
		assert( !flushes );
		for ( i = 0; i < base; ++i ) assert( tess.xyz[i][0] == -999 && tess.indexes[i] == (unsigned)i );
		for ( i = base; i < tess.numVertexes; ++i ) for ( k = 0; k < 3; ++k ) assert( isfinite( tess.xyz[i][k] ) );
		printf( "%s{\"case\":%d,\"type\":%d,\"vertices\":[", index ? ",\n" : "", index, c.type );
		for ( i = base; i < tess.numVertexes; ++i ) {
			const float *v = tess.xyz[i], *st = tess.texCoords[0][i];
			const byte *rgba = tess.vertexColors[i].rgba;
			printf( "%s[%.9g,%.9g,%.9g,%.9g,%.9g,%u,%u,%u,%u]", i > base ? "," : "", v[0], v[1], v[2], st[0], st[1],
				rgba[0], rgba[1], rgba[2], rgba[3] );
		}
		printf( "],\"randomCalls\":%u,\"indexes\":[", randomCalls );
		for ( i = base; i < tess.numIndexes; ++i ) printf( "%s%u", i > base ? "," : "", tess.indexes[i] - base );
		printf( "]}" );
	}
	printf( "\n]\n" );
	/* Another profile's type is not drawn here. */
	entity.e.reType = RT_STVEF_LINE;
	assert( !RB_SurfaceJKA() );
	return 0;
}

static int Bounds( void ) {
	static const float lengths[] = { FLT_MAX, 1.0e30f, 1.0e6f };
	jkaSurfaceCase_t c;
	int i, k, n;
	/* A batch that cannot take the next quad is flushed first, and every
	 * index left refers to a vertex of its own batch. */
	for ( n = 0; n < JKA_SURFACE_CASES; ++n ) {
		Load( n, &c );
		memset( &tess, 0, sizeof( tess ) );
		flushes = 0;
		tess.numVertexes = TESS_VERTEXES - 2;
		tess.numIndexes = 3;
		assert( RB_SurfaceJKA() && flushes == 1 && tess.numVertexes > 0 );
		CheckIndexes();
	}
	/* A blade too long to draw stops at the engine's bound, still ending in
	 * the hilt, however small its radius. */
	for ( n = 0; n < (int)( sizeof( lengths ) / sizeof( lengths[0] ) ); ++n ) {
		for ( k = 0; k < 2; ++k ) {
			int quads = 0;
			Load( 0, &c );
			entity.e.jka.saberLength = lengths[n];
			entity.e.radius = k ? 0.0f : 0.01f;
			memset( &tess, 0, sizeof( tess ) );
			flushes = 0;
			assert( RB_SurfaceJKA() );
			CheckIndexes();
			quads = flushes * ( TESS_VERTEXES / 4 ) + tess.numVertexes / 4;
			assert( quads == JKA_SABER_GLOW_MAX_SPRITES + 1 );
			for ( i = 0; i < tess.numVertexes; ++i ) assert( isfinite( tess.xyz[i][0] ) );
			assert( randomCalls == 1 );
		}
	}
	printf( "bounds passed\n" );
	return 0;
}
