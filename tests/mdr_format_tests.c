/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Synthetic fixtures exercise the same loader included by all renderers. */
#define _CRT_SECURE_NO_WARNINGS 1
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qfiles.h"
#include "../code/qcommon/game_profile.h"
#include "../code/renderercommon/tr_mdr_format.h"
#include "../code/renderercommon/tr_mdr_frames.h"
#include "../code/renderercommon/tr_stvef_lod.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

enum { MOD_MDR = 3, SF_MDR = 7, LIGHTMAP_NONE = -1 };
typedef struct { int type, dataSize, numLods; void *modelData; } model_t;
typedef struct { int defaultShader, index; } shader_t;
static unsigned char *allocation;
static size_t allocationBytes;
static int allocations, shaders;
static void Print( int level, const char *format, ... ) { (void)level; (void)format; }
static void *Allocate( int bytes, ha_pref preference ) {
	(void)preference;
	assert( bytes > 0 && !allocation );
	allocationBytes = (size_t)bytes;
	allocation = (unsigned char *)malloc( allocationBytes + 32 ); assert( allocation );
	memset( allocation, 0xa5, allocationBytes + 32 ); ++allocations;
	return allocation + 16;
}
static struct { void (*Printf)(int, const char *, ...); void *(*Hunk_Alloc)(int, ha_pref); comGameId_t gameId; } ri = {Print, Allocate, COM_GAME_QUAKE3};
static shader_t *R_FindShader( const char *name, int lightmap, qboolean raw ) {
	static shader_t shader = {0, 42};
	assert( name && memchr( name, 0, 64 ) && lightmap == LIGHTMAP_NONE && raw ); ++shaders; return &shader;
}
char *Q_strlwr( char *s ) { char *p; for ( p = s; *p; ++p ) if ( *p >= 'A' && *p <= 'Z' ) *p += 'a' - 'A'; return s; }
#include "../code/renderercommon/tr_mdr_model.h"

static unsigned char original[2048];
static uint32_t length, frames, frameStride, lod, surface, vertices, triangles, references, tags;
static void Put32( unsigned char *p, uint32_t value ) { int i; for ( i = 0; i < 4; ++i ) p[i] = (unsigned char)( value >> ( i * 8 ) ); }
static void PutFloat( unsigned char *p, float value ) { uint32_t bits; memcpy( &bits, &value, 4 ); Put32( p, bits ); }
static void Make( int compressed ) {
	unsigned char *s;
	uint32_t f, b, v, o;
	frames = 105; frameStride = compressed ? 88 : 152; lod = frames + 2 * frameStride + 1;
	surface = lod + 13; vertices = surface + 169; triangles = vertices + 152 + 1;
	references = triangles + 12 + 1; tags = references + 8 + 1; length = tags + 36;
	memset( original, 0, sizeof( original ) );
	Put32( original, 0x354d4452 ); Put32( original + 4, 2 ); memset( original + 8, 'N', 64 );
	Put32( original + 72, 2 ); Put32( original + 76, 2 ); Put32( original + 80, compressed ? 0u - frames : frames );
	Put32( original + 84, 1 ); Put32( original + 88, lod ); Put32( original + 92, 1 );
	Put32( original + 96, tags ); Put32( original + 100, length );
	for ( f = 0; f < 2; ++f ) {
		unsigned char *frame = original + frames + f * frameStride;
		int axis;
		for ( axis = 0; axis < 3; ++axis ) {
			PutFloat( frame + axis * 4, -64 ); PutFloat( frame + 12 + axis * 4, 64 );
			PutFloat( frame + 24 + axis * 4, (float)axis );
		}
		PutFloat( frame + 36, 110 );
		if ( !compressed ) memcpy( frame + 40, "frame", 6 );
		for ( b = 0; b < 2; ++b ) {
			unsigned char packed[24]; float matrix[3][4]; int i;
			for ( i = 0; i < 12; ++i ) {
				unsigned value = i < 3 ? 32768 + f * 640 + b * 64 + i : ( i == 3 || i == 7 || i == 11 ? 65534 : 32768 );
				packed[i*2] = (unsigned char)value; packed[i*2+1] = (unsigned char)( value >> 8 );
			}
			if ( compressed ) memcpy( frame + 40 + b * 24, packed, 24 );
			else {
				FnMdr_Uncompress( packed, matrix );
				for ( i = 0; i < 12; ++i ) PutFloat( frame + 56 + b * 48 + i * 4, matrix[i / 4][i % 4] );
			}
		}
	}
	Put32( original + lod, 1 ); Put32( original + lod + 4, surface - lod ); Put32( original + lod + 8, tags - 1 - lod );
	s = original + surface;
	Put32( s, 0xdeadbeef ); memcpy( s + 4, "BODY", 5 ); memset( s + 68, 'S', 64 );
	Put32( s + 132, 0xdeadbeef ); Put32( s + 136, 0u - surface );
	Put32( s + 140, 3 ); Put32( s + 144, vertices - surface ); Put32( s + 148, 1 ); Put32( s + 152, triangles - surface );
	Put32( s + 156, 2 ); Put32( s + 160, references - surface ); Put32( s + 164, tags - 1 - surface );
	o = vertices;
	for ( v = 0; v < 3; ++v ) {
		const uint32_t count = v ? 1 : 2;
		uint32_t w;
		PutFloat( original + o + 8, 1 ); PutFloat( original + o + 12, v * 0.5f ); PutFloat( original + o + 16, v * -0.25f );
		Put32( original + o + 20, count );
		for ( w = 0; w < count; ++w ) {
			unsigned char *weight = original + o + 24 + w * 20;
			Put32( weight, w ); PutFloat( weight + 4, count == 1 ? 1 : w ? 0.75f : 0.25f );
			PutFloat( weight + 8, 1 ); PutFloat( weight + 12, 2 + (float)v ); PutFloat( weight + 16, 3 );
		}
		o += 24 + count * 20;
	}
	Put32( original + triangles, 0 ); Put32( original + triangles + 4, 2 ); Put32( original + triangles + 8, 1 );
	Put32( original + references, 1 ); Put32( original + references + 4, 0 );
	Put32( original + tags, 1 ); memset( original + tags + 4, 'T', 32 );
}

static int Exercise( unsigned char *data, size_t bytes ) {
	fnMdrView_t view, before;
	model_t model = {0}; unsigned char *saved;
	int valid, loaded; uint32_t i;
	memset( &view, 0xa5, sizeof( view ) ); before = view;
	saved = (unsigned char *)malloc( bytes ? bytes : 1 ); assert( saved ); memcpy( saved, data, bytes );
	allocations = shaders = 0;
	valid = FnMdr_Open( data, bytes, SHADER_MAX_VERTEXES, SHADER_MAX_INDEXES / 3, &view );
	loaded = R_LoadMDR( &model, data, (int)bytes, "synthetic.mdr" );
	assert( valid == loaded && !memcmp( saved, data, bytes ) ); free( saved );
	if ( !valid ) {
		assert( !memcmp( &view, &before, sizeof( view ) ) && !allocations && !shaders && !model.modelData && !model.dataSize ); return 0;
	}
	assert( allocations == 1 && model.type == MOD_MDR && model.modelData == allocation + 16 );
	assert( model.dataSize == (int)view.expandedBytes );
	{
		mdrHeader_t *header = (mdrHeader_t *)model.modelData;
		mdrTag_t *tag = (mdrTag_t *)( (byte *)header + header->ofsTags );
		uint32_t offset = header->ofsLODs;
		assert( header->ofsEnd == model.dataSize && !header->name[63] );
		assert( model.numLods == header->numLODs - ( ( ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP ) && header->numLODs > 1 ) );
		assert( header->numBones <= MDR_MAX_BONES );
		for ( i = 0; i < view.numTags; ++i ) assert( (uint32_t)tag[i].boneIndex < view.numBones && !tag[i].name[31] );
		for ( i = 0; i < view.numLODs; ++i ) {
			mdrLOD_t *outputLod = (mdrLOD_t *)( (byte *)header + offset );
			mdrSurface_t *outputSurface = (mdrSurface_t *)( (byte *)outputLod + outputLod->ofsSurfaces );
			int s;
			for ( s = 0; s < outputLod->numSurfaces; ++s ) {
				mdrVertex_t *vertex = (mdrVertex_t *)( (byte *)outputSurface + outputSurface->ofsVerts ); int v;
				assert( (byte *)outputSurface + outputSurface->ofsHeader == (byte *)header );
				assert( outputSurface->shaderIndex == 42 && outputSurface->ident == SF_MDR );
				assert( !outputSurface->shader[63] && !outputSurface->name[63] );
				for ( v = 0; v < outputSurface->numVerts; ++v ) {
					int a; for ( a = 0; a < 3; ++a ) assert( isfinite( vertex->normal[a] ) );
					vertex = (mdrVertex_t *)&vertex->weights[vertex->numWeights];
				}
				outputSurface = (mdrSurface_t *)( (byte *)outputSurface + outputSurface->ofsEnd );
			}
			offset += outputLod->ofsEnd;
		}
	}
	for ( i = 0; i < 16; ++i ) assert( allocation[i] == 0xa5 && allocation[16 + allocationBytes + i] == 0xa5 );
	free( allocation ); allocation = NULL;
	return 1;
}
static void Bad32( uint32_t offset, uint32_t value ) {
	unsigned char bad[2048]; memcpy( bad, original, length ); Put32( bad + offset, value ); assert( !Exercise( bad, length ) );
}
int main( int argc, char **argv ) {
	int compressed;
	assert( sizeof(mdrHeader_t) == 104 && sizeof(mdrLOD_t) == 12 && sizeof(mdrSurface_t) == 168 );
	assert( offsetof(mdrVertex_t, weights) == 24 && sizeof(mdrWeight_t) == 20 && sizeof(mdrTag_t) == 36 );
	assert( offsetof(mdrFrame_t, bones) == 56 && sizeof(mdrBone_t) == 48 && MDR_MAX_BONES == FN_MDR_MAX_BONES );
	if ( argc == 2 ) {
		FILE *file = fopen( argv[1], "rb" ); uint32_t bytes; unsigned count = 0;
		assert( file );
		while ( fread( &bytes, 4, 1, file ) == 1 ) {
			unsigned char *data = (unsigned char *)malloc( bytes ); assert( data );
			assert( fread( data, bytes, 1, file ) == 1 );
			if ( !Exercise( data, bytes ) ) { fprintf( stderr, "rejected model %u\n", count ); return 1; }
			ri.gameId = COM_GAME_STVEF_SP; assert( Exercise( data, bytes ) ); ri.gameId = COM_GAME_QUAKE3;
			free( data ); ++count;
		}
		fclose( file ); printf( "%u models validated and loaded\n", count ); return 0;
	}
	for ( compressed = 0; compressed < 2; ++compressed ) {
		uint32_t i, j;
		fnMdrView_t view;
		float matrix[3][4], savedMatrix[3][4];
		Make( compressed ); assert( Exercise( original, length ) );
		assert( FnMdr_Open( original, length, 1000, 2000, &view ) );
		assert( FnMdr_Bone( &view, 1, 1, matrix ) && matrix[0][3] == 11 && matrix[2][2] == 1 );
		memcpy( savedMatrix, matrix, sizeof(matrix) );
		assert( !FnMdr_Bone( &view, 2, 0, matrix ) && !memcmp( matrix, savedMatrix, sizeof(matrix) ) );
		view.frameStride = 1; assert( !FnMdr_Bone( &view, 0, 0, matrix ) );
		for ( i = 0; i < length; ++i ) {
			unsigned char *shortData = (unsigned char *)malloc( i + 1 ); assert( shortData );
			memcpy( shortData + 1, original, i ); assert( !Exercise( shortData + 1, i ) ); free( shortData );
		}
		for ( i = 0; i < length; ++i ) for ( j = 0; j < 2; ++j ) {
			unsigned char mutated[2049]; memcpy( mutated + 1, original, length ); mutated[i+1] = j ? 255 : 0;
			Exercise( mutated + 1, length );
		}
		Bad32( 0, 0 ); Bad32( 4, 3 ); Bad32( 72, 0 ); Bad32( 72, INT_MAX ); Bad32( 76, 0 ); Bad32( 76, 129 );
		Bad32( 80, INT32_MIN ); Bad32( 80, 4 ); Bad32( 84, 0 ); Bad32( 84, INT_MAX ); Bad32( 88, length-1 );
		Bad32( 92, INT_MAX ); Bad32( 96, length-1 ); Bad32( 100, 103 ); Bad32( 100, INT_MAX );
		Bad32( lod, INT_MAX ); Bad32( lod+4, 0 ); Bad32( lod+8, 11 );
		Bad32( surface+140, 1001 ); Bad32( surface+144, 0 ); Bad32( surface+148, INT_MAX ); Bad32( surface+152, length );
		Bad32( surface+156, 129 ); Bad32( surface+160, length ); Bad32( surface+164, 167 );
		Bad32( vertices+20, INT_MAX ); Bad32( vertices+24, 2 ); Bad32( triangles, 3 ); Bad32( references, 2 ); Bad32( tags, 2 );
		Bad32( frames+36, 0xbf800000 );
		{
			uint32_t offsets[] = {frames, frames+12, frames+24, frames+36, vertices+12, vertices+28, vertices+32};
			for ( i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i ) {
				Bad32( offsets[i], 0x7fc00000 ); Bad32( offsets[i], 0x7f800000 ); Bad32( offsets[i], 0xff800000 );
			}
		}
		if ( !compressed ) Bad32( frames+56, 0x7fc00000 );
		{
			unsigned char unusual[2048];
			memcpy( unusual, original, length ); Put32( unusual+surface+136, UINT32_MAX ); assert( Exercise( unusual, length ) );
			memcpy( unusual, original, length ); PutFloat( unusual+vertices+28, -0.5f ); assert( Exercise( unusual, length ) );
			memcpy( unusual, original, length ); Put32( unusual+vertices, 0x7fc00000 ); assert( Exercise( unusual, length ) );
			memcpy( unusual, original, length ); Put32( unusual+lod, 0 ); Put32( unusual+lod+4, 0 ); assert( Exercise( unusual, length ) );
			memcpy( unusual, original, length ); Put32( unusual+92, 0 ); Put32( unusual+96, 0 ); assert( Exercise( unusual, length ) );
		}
	}
	{
		int a, b, profile;
		a = 9; b = 8; assert( FnMdr_SelectFrames( 4, 1, 1, &a, &b ) && a == 3 && b == 3 );
		a = 9; b = 8; assert( FnMdr_SelectFrames( 4, 0, 1, &a, &b ) && a == 1 && b == 0 );
		a = 9; b = 8; assert( !FnMdr_SelectFrames( 4, 0, 0, &a, &b ) && !a && !b );
		a = -1; b = INT_MAX; assert( !FnMdr_SelectFrames( 4, 1, 1, &a, &b ) && !a && !b );
		a = INT_MIN; b = INT_MAX; assert( !FnMdr_SelectFrames( 0, 0, 1, &a, &b ) && !a && !b );
		a = 2; b = 1; assert( FnMdr_SelectFrames( 4, 1, 0, &a, &b ) && a == 2 && b == 1 );
		/* Values observed by the optional local retail mathematical probe. */
		assert( FnSTVEF_ComputeLOD( 4, 0.875f, 1, 0 ) == 0 );
		assert( FnSTVEF_ComputeLOD( 4, 0.625f, 1, 0 ) == 2 );
		assert( FnSTVEF_ComputeLOD( 4, 0.375f, 1, 0 ) == 2 );
		assert( FnSTVEF_ComputeLOD( 3, 0.5f, 1, 0 ) == 2 );
		assert( FnSTVEF_ComputeLOD( 4, 1.0f/32, 40, 0 ) == 0 );
		assert( FnSTVEF_ComputeLOD( 4, 0, 4, 1 ) == 1 );
		assert( FnSTVEF_ComputeLOD( 1, 0.5f, 4, INT_MAX ) == 0 );
		assert( FnSTVEF_ComputeLOD( INT_MAX, 0.5f, 1, INT_MAX ) == INT_MAX - 1 );
		assert( FnSTVEF_ComputeLOD( 4, 0.5f, 1, INT_MIN ) == 0 );
		Make( 1 );
		memmove( original + tags + 23, original + tags - 1, 37 );
		memset( original + tags - 1, 0, 24 );
		Put32( original + tags + 7, 12 ); Put32( original + tags + 19, 12 );
		Put32( original + 84, 3 ); Put32( original + 96, tags + 24 ); Put32( original + 100, length + 24 );
		for ( profile = COM_GAME_QUAKE3; profile < COM_GAME_COUNT; ++profile ) {
			ri.gameId = (comGameId_t)profile; assert( Exercise( original, length + 24 ) );
		}
	}
	{
		uint32_t size = INT_MAX-1;
		assert( !FnMdr_AddSize( &size, SIZE_MAX, 48 ) && size == INT_MAX-1 );
		assert( !FnMdr_Range( SIZE_MAX, 0, SIZE_MAX, 48, 0 ) );
		assert( !FnMdr_Open( NULL, 0, 1000, 2000, NULL ) );
	}
	return 0;
}
