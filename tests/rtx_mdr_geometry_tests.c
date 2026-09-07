/* Synthetic, redistributable MDR fixtures for the RT geometry boundary. */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qfiles.h"
#include "../code/rendererrtx/rtx_mdr_geometry.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef union {
	mdrHeader_t alignment;
	byte bytes[8192];
} fixtureStorage_t;

static fixtureStorage_t fixture;
static mdrHeader_t *header;
static mdrSurface_t *surface;
static mdrVertex_t *vertex;
static mdrFrame_t *frames[2];

static void reset_fixture( void )
{
	size_t offset, frameBytes;
	int f, b, lodIndex, v;
	memset( &fixture, 0, sizeof( fixture ) );
	header = (mdrHeader_t *)fixture.bytes;
	header->numFrames = 2;
	header->numBones = 2;
	header->ofsFrames = sizeof( *header );
	header->numLODs = 2;
	frameBytes = offsetof( mdrFrame_t, bones ) + 2 * sizeof( mdrBone_t );
	for ( f = 0; f < 2; f++ ) {
		frames[f] = (mdrFrame_t *)( fixture.bytes + header->ofsFrames + f * frameBytes );
		for ( b = 0; b < 2; b++ ) {
			frames[f]->bones[b].matrix[0][0] = 1.0f;
			frames[f]->bones[b].matrix[1][1] = 1.0f;
			frames[f]->bones[b].matrix[2][2] = 1.0f;
			frames[f]->bones[b].matrix[0][3] = ( f ? 10.0f : 2.0f ) * ( b + 1 );
		}
	}
	offset = header->ofsFrames + 2 * frameBytes;
	header->ofsLODs = (int)offset;
	for ( lodIndex = 0; lodIndex < 2; lodIndex++ ) {
		mdrLOD_t *lod = (mdrLOD_t *)( fixture.bytes + offset );
		mdrSurface_t *currentSurface;
		mdrTriangle_t *triangle;
		size_t lodStart = offset, surfaceStart;
		lod->numSurfaces = 1;
		lod->ofsSurfaces = sizeof( *lod );
		offset += sizeof( *lod );
		surfaceStart = offset;
		currentSurface = (mdrSurface_t *)( fixture.bytes + offset );
		currentSurface->numVerts = 3;
		currentSurface->numTriangles = 1;
		currentSurface->ofsVerts = sizeof( *currentSurface );
		offset += sizeof( *currentSurface );
		for ( v = 0; v < 3; v++ ) {
			mdrVertex_t *currentVertex = (mdrVertex_t *)( fixture.bytes + offset );
			currentVertex->normal[2] = 1.0f;
			currentVertex->texCoords[0] = (float)v;
			currentVertex->numWeights = v == 0 ? 2 : 1;
			for ( b = 0; b < currentVertex->numWeights; b++ ) {
				currentVertex->weights[b].boneIndex = b;
				currentVertex->weights[b].boneWeight = v == 0 ? ( b ? 0.75f : 0.25f ) : 1.0f;
				currentVertex->weights[b].offset[0] = 1.0f;
				currentVertex->weights[b].offset[1] = 2.0f + (float)v;
				currentVertex->weights[b].offset[2] = 3.0f;
			}
			offset += offsetof( mdrVertex_t, weights ) + currentVertex->numWeights * sizeof( mdrWeight_t );
		}
		currentSurface->ofsTriangles = (int)( offset - surfaceStart );
		triangle = (mdrTriangle_t *)( fixture.bytes + offset );
		triangle->indexes[0] = 0;
		triangle->indexes[1] = 1;
		triangle->indexes[2] = 2;
		offset += sizeof( *triangle );
		currentSurface->ofsEnd = (int)( offset - surfaceStart );
		lod->ofsEnd = (int)( offset - lodStart );
		if ( !lodIndex ) surface = currentSurface;
	}
	header->ofsEnd = (int)offset;
	vertex = (mdrVertex_t *)( (byte *)surface + surface->ofsVerts );
}

static void close_to( float actual, float expected )
{
	assert( fabsf( actual - expected ) < 0.00001f );
}

static void test_weighted_pose_and_frame_selection( void )
{
	mdrBone_t bones[MDR_MAX_BONES];
	vec3_t position, normal;
	reset_fixture();
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), 1, 0, qfalse, 0.5f, bones ) );
	assert( RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	assert( RTX_MDRSkinVertex( vertex, bones, position, normal ) );
	close_to( position[0], 11.5f );
	close_to( position[1], 2.0f );
	close_to( position[2], 3.0f );
	close_to( normal[2], 1.0f );
	/* With no blend, the unused old pose must not contaminate the result. */
	frames[0]->bones[0].matrix[0][0] = NAN;
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), 1, 0, qfalse, 0.0f, bones ) );
	assert( RTX_MDRSkinVertex( vertex, bones, position, normal ) );
	close_to( position[0], 18.5f );
	reset_fixture();
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), 3, 2, qtrue, 0.5f, bones ) );
	close_to( bones[0].matrix[0][3], 6.0f );
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), 3, 1, qfalse, 0.5f, bones ) );
	close_to( bones[0].matrix[0][3], 2.0f );
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), -1, 1, qtrue, 0.5f, bones ) );
	close_to( bones[0].matrix[0][3], 2.0f );
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), 1, 0, qfalse, NAN, bones ) );
	close_to( bones[0].matrix[0][3], 10.0f );
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), 1, 0, qfalse, 2.0f, bones ) );
	close_to( bones[0].matrix[0][3], 2.0f );
}

static void test_model_bounds_and_lod( void )
{
	mdrBone_t bones[MDR_MAX_BONES];
	size_t bytes;
	const mdrLOD_t *first, *last;
	reset_fixture();
	for ( bytes = 0; bytes < (size_t)header->ofsEnd; bytes++ ) {
		assert( !RTX_MDRPreparePose( header, bytes, 0, 0, qfalse, 0.0f, bones ) );
	}
	assert( RTX_MDRPreparePose( header, header->ofsEnd, 0, 0, qfalse, 0.0f, bones ) );
	first = RTX_MDRGetLOD( header, 0 );
	last = RTX_MDRGetLOD( header, 1 );
	assert( first && last && first != last );
	assert( RTX_MDRGetLOD( header, -20 ) == first );
	assert( RTX_MDRGetLOD( header, INT_MAX ) == last );
	header->numBones = MDR_MAX_BONES + 1;
	assert( !RTX_MDRPreparePose( header, sizeof( fixture ), 0, 0, qfalse, 0.0f, bones ) );
	header->numBones = 0;
	assert( !RTX_MDRPreparePose( header, sizeof( fixture ), 0, 0, qfalse, 0.0f, bones ) );
	reset_fixture();
	header->numFrames = INT_MAX;
	assert( !RTX_MDRPreparePose( header, sizeof( fixture ), 0, 0, qfalse, 0.0f, bones ) );
	reset_fixture();
	header->ofsFrames = INT_MAX;
	assert( !RTX_MDRPreparePose( header, sizeof( fixture ), 0, 0, qfalse, 0.0f, bones ) );
	reset_fixture();
	header->ofsFrames++;
	assert( !RTX_MDRPreparePose( header, sizeof( fixture ), 0, 0, qfalse, 0.0f, bones ) );
	reset_fixture();
	frames[0]->bones[0].matrix[2][1] = INFINITY;
	assert( !RTX_MDRPreparePose( header, sizeof( fixture ), 0, 0, qfalse, 0.0f, bones ) );
	reset_fixture();
	((mdrLOD_t *)RTX_MDRGetLOD( header, 0 ))->ofsEnd = 0;
	assert( !RTX_MDRGetLOD( header, 0 ) );
	reset_fixture();
	header->ofsLODs = INT_MAX;
	assert( !RTX_MDRGetLOD( header, 0 ) );
}

static void test_surface_bounds_and_weights( void )
{
	mdrBone_t bones[MDR_MAX_BONES];
	vec3_t position, normal;
	size_t bytes;
	reset_fixture();
	for ( bytes = 0; bytes < (size_t)surface->ofsEnd; bytes++ ) {
		assert( !RTX_MDRValidateSurface( surface, bytes, 2 ) );
	}
	vertex->weights[1].boneIndex = 2;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	vertex->weights[1].boneIndex = -1;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	vertex->numWeights = INT_MAX;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	vertex->numWeights = 0;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	vertex->weights[0].boneWeight = NAN;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	vertex->weights[0].offset[1] = INFINITY;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	surface->numVerts = INT_MAX;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	surface->ofsTriangles = surface->ofsVerts + 1;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	surface->numTriangles = INT_MAX;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	surface->ofsVerts++;
	assert( !RTX_MDRValidateSurface( surface, surface->ofsEnd, 2 ) );
	reset_fixture();
	assert( RTX_MDRPreparePose( header, sizeof( fixture ), 0, 0, qfalse, 0.0f, bones ) );
	vertex->weights[0].boneWeight = FLT_MAX;
	assert( !RTX_MDRSkinVertex( vertex, bones, position, normal ) );
}

int main( void )
{
	test_weighted_pose_and_frame_selection();
	test_model_bounds_and_lod();
	test_surface_bounds_and_weights();
	puts( "RTX MDR geometry tests passed" );
	return 0;
}
