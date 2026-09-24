/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Jedi Academy's RBSP v1 map boundary.
 * Every map here is written by this test; no retail map or asset is used. */

#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/bsp_rbsp.h"

#include <cstring>
#include <iostream>
#include <vector>

void Q_strncpyz( char *out, const char *in, int size )
{
	if ( size <= 0 ) return;
	std::strncpy( out, in, size - 1 );
	out[size - 1] = 0;
}

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using Bytes = std::vector<byte>;

/* Raven's records, as the format defines them. A mismatch here would mean the
 * translator reads a different file than Jedi Academy writes. */
static_assert( sizeof( dheaderRBSP_t ) == 152, "RBSP directory has eighteen lumps" );
static_assert( sizeof( dbrushsideRBSP_t ) == 12, "RBSP brush sides carry a draw surface" );
static_assert( sizeof( drawVertRBSP_t ) == 80, "RBSP vertices carry four lightmap sets" );
static_assert( sizeof( dsurfaceRBSP_t ) == 148, "RBSP surfaces carry four lightmap sets" );
static_assert( sizeof( dgridRBSP_t ) == 30, "RBSP light grid carries four styles" );
static_assert( sizeof( dbrushside_t ) == 8 && sizeof( drawVert_t ) == 44, "Quake III records" );
static_assert( sizeof( dsurface_t ) == 104 && sizeof( dheader_t ) == 144, "Quake III records" );

static int allocations;
static int allocationFailure = -1;

static void *Alloc( int size )
{
	if ( allocations++ == allocationFailure ) return nullptr;
	return std::calloc( 1, static_cast<size_t>( size ) );
}

static void Free( void *pointer ) { std::free( pointer ); }

template<class T>
static void Lump( Bytes &file, int index, const std::vector<T> &records )
{
	// Deliberately unaligned disk offsets: the translator must normalize them.
	file.push_back( 0 );
	const lump_t lump{ LittleLong( int( file.size() ) ),
		LittleLong( int( records.size() * sizeof( T ) ) ) };
	std::memcpy( file.data() + 8 + index * sizeof( lump ), &lump, sizeof( lump ) );
	const auto *bytes = reinterpret_cast<const byte *>( records.data() );
	if ( !records.empty() ) file.insert( file.end(), bytes, bytes + records.size() * sizeof( T ) );
}

static lump_t Directory( const Bytes &file, int index )
{
	lump_t lump;
	std::memcpy( &lump, file.data() + 8 + index * sizeof( lump ), sizeof( lump ) );
	return lump;
}

static void SetDirectory( Bytes &file, int index, int offset, int length )
{
	const lump_t lump{ LittleLong( offset ), LittleLong( length ) };
	std::memcpy( file.data() + 8 + index * sizeof( lump ), &lump, sizeof( lump ) );
}

/* One world, one submodel, one patch and one planar surface, with four
 * lightmap sets and styles on everything that carries them. */
static Bytes Fixture()
{
	dheaderRBSP_t header{};
	header.ident = LittleLong( RBSP_IDENT );
	header.version = LittleLong( RBSP_VERSION );
	Bytes file( sizeof( header ) );
	std::memcpy( file.data(), &header, sizeof( header ) );

	Lump<char>( file, LUMP_ENTITIES, { '{', '}', '\n', 0 } );

	std::vector<dshader_t> shaders( 2 );
	Q_strncpyz( shaders[0].shader, "textures/common/caulk", MAX_QPATH );
	Q_strncpyz( shaders[1].shader, "textures/yavin/grass", MAX_QPATH );
	shaders[1].contentFlags = LittleLong( 1 );
	Lump( file, LUMP_SHADERS, shaders );

	std::vector<dplane_t> planes( 6 );
	for ( int i = 0; i < 6; ++i ) {
		planes[i].normal[i / 2] = LittleFloat( i % 2 ? 1.0f : -1.0f );
		planes[i].dist = LittleFloat( 64.0f );
	}
	Lump( file, LUMP_PLANES, planes );

	dnode_t node{};
	node.children[0] = LittleLong( -1 );
	node.children[1] = LittleLong( -2 );
	Lump<dnode_t>( file, LUMP_NODES, { node } );

	dleaf_t solid{}, open{};
	solid.cluster = LittleLong( -1 );
	open.numLeafBrushes = LittleLong( 1 );
	open.numLeafSurfaces = LittleLong( 2 );
	Lump<dleaf_t>( file, LUMP_LEAFS, { solid, open } );
	Lump<int>( file, LUMP_LEAFSURFACES, { LittleLong( 0 ), LittleLong( 1 ) } );
	Lump<int>( file, LUMP_LEAFBRUSHES, { LittleLong( 0 ) } );

	dmodel_t world{}, submodel{};
	world.numSurfaces = LittleLong( 2 );
	world.numBrushes = LittleLong( 1 );
	submodel.firstSurface = LittleLong( 1 );
	submodel.numSurfaces = LittleLong( 1 );
	for ( int i = 0; i < 3; ++i ) {
		world.mins[i] = submodel.mins[i] = LittleFloat( -64 );
		world.maxs[i] = submodel.maxs[i] = LittleFloat( 64 );
	}
	Lump<dmodel_t>( file, LUMP_MODELS, { world, submodel } );

	dbrush_t brush{};
	brush.numSides = LittleLong( 6 );
	brush.shaderNum = LittleLong( 0 );
	Lump<dbrush_t>( file, LUMP_BRUSHES, { brush } );

	std::vector<dbrushsideRBSP_t> sides( 6 );
	for ( int i = 0; i < 6; ++i ) {
		sides[i].planeNum = LittleLong( i );
		sides[i].shaderNum = LittleLong( i % 2 );
		sides[i].drawSurfNum = LittleLong( 100 + i );
	}
	Lump( file, LUMP_BRUSHSIDES, sides );

	std::vector<drawVertRBSP_t> vertices( 9 );
	for ( int i = 0; i < 9; ++i ) {
		vertices[i].xyz[0] = LittleFloat( float( i * 16 ) );
		vertices[i].xyz[1] = LittleFloat( float( i % 3 ) );
		vertices[i].st[0] = LittleFloat( float( i ) / 8.0f );
		vertices[i].normal[2] = LittleFloat( 1 );
		for ( int set = 0; set < RBSP_MAXLIGHTMAPS; ++set ) {
			vertices[i].lightmap[set][0] = LittleFloat( float( set ) + 0.25f );
			vertices[i].lightmap[set][1] = LittleFloat( float( set ) + 0.5f );
			for ( int c = 0; c < 4; ++c ) {
				vertices[i].color[set][c] = static_cast<byte>( 16 * set + c );
			}
		}
	}
	Lump( file, LUMP_DRAWVERTS, vertices );

	std::vector<int> indexes( 6 );
	for ( int i = 0; i < 6; ++i ) indexes[i] = LittleLong( i % 4 );
	Lump( file, LUMP_DRAWINDEXES, indexes );

	dfog_t fog{};
	Q_strncpyz( fog.shader, "textures/yavin/fog", MAX_QPATH );
	fog.visibleSide = LittleLong( -1 );
	Lump<dfog_t>( file, LUMP_FOGS, { fog } );

	std::vector<dsurfaceRBSP_t> surfaces( 2 );
	surfaces[0].shaderNum = LittleLong( 1 );
	surfaces[0].fogNum = LittleLong( -1 );
	surfaces[0].surfaceType = LittleLong( MST_PLANAR );
	surfaces[0].numVerts = LittleLong( 4 );
	surfaces[0].numIndexes = LittleLong( 6 );
	surfaces[0].lightmapWidth = LittleLong( 4 );
	surfaces[0].lightmapHeight = LittleLong( 4 );
	surfaces[1].shaderNum = LittleLong( 1 );
	surfaces[1].fogNum = LittleLong( 0 );
	surfaces[1].surfaceType = LittleLong( MST_PATCH );
	surfaces[1].firstVert = LittleLong( 0 );
	surfaces[1].numVerts = LittleLong( 9 );
	surfaces[1].patchWidth = LittleLong( 3 );
	surfaces[1].patchHeight = LittleLong( 3 );
	for ( auto &surface : surfaces ) {
		for ( int set = 0; set < RBSP_MAXLIGHTMAPS; ++set ) {
			surface.lightmapStyles[set] = static_cast<byte>( set ? RBSP_LS_UNUSED : RBSP_LS_NORMAL );
			surface.vertexStyles[set] = static_cast<byte>( set ? RBSP_LS_NONE : RBSP_LS_NORMAL );
			surface.lightmapNum[set] = LittleLong( set );
			surface.lightmapX[set] = LittleLong( 8 * set );
			surface.lightmapY[set] = LittleLong( 16 * set );
		}
		surface.lightmapOrigin[1] = LittleFloat( 3 );
		surface.lightmapVecs[2][2] = LittleFloat( 1 );
	}
	Lump( file, LUMP_SURFACES, surfaces );

	Lump<byte>( file, LUMP_LIGHTMAPS, Bytes( RBSP_LIGHTMAP_BYTES, 0x5a ) );

	std::vector<dgridRBSP_t> grid( 2 );
	for ( int point = 0; point < 2; ++point ) {
		for ( int set = 0; set < RBSP_MAXLIGHTMAPS; ++set ) {
			for ( int c = 0; c < 3; ++c ) {
				grid[point].ambientLight[set][c] = static_cast<byte>( 10 * set + c + point );
				grid[point].directLight[set][c] = static_cast<byte>( 50 * set + c + point );
			}
			grid[point].styles[set] = static_cast<byte>( set );
		}
		grid[point].latLong[0] = static_cast<byte>( 7 + point );
		grid[point].latLong[1] = static_cast<byte>( 9 + point );
	}
	Lump( file, LUMP_LIGHTGRID, grid );

	Lump<int>( file, LUMP_VISIBILITY, { LittleLong( 1 ), LittleLong( 4 ), LittleLong( 1 ) } );
	Lump<unsigned short>( file, LUMP_RBSP_LIGHTARRAY, { 0, 1, 1, 0 } );
	return file;
}

static void CheckTranslation()
{
	const Bytes file = Fixture();
	rbsp_translation_t translation{};
	char error[128] = "unchanged";

	allocations = 0;
	CHECK( RBSP_TranslateToV46( file.data(), int( file.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( translation.data && translation.length > int( sizeof( dheader_t ) ) && !error[0] );

	const auto *output = static_cast<const byte *>( translation.data );
	dheader_t header;
	std::memcpy( &header, output, sizeof( header ) );
	CHECK( LittleLong( header.ident ) == BSP_IDENT && LittleLong( header.version ) == BSP_VERSION );

	// Every lump lands inside the buffer, aligned, without overlapping another.
	int covered = int( sizeof( dheader_t ) );
	for ( int i = 0; i < HEADER_LUMPS; ++i ) {
		const int offset = LittleLong( header.lumps[i].fileofs );
		const int length = LittleLong( header.lumps[i].filelen );
		CHECK( offset >= int( sizeof( dheader_t ) ) && length >= 0 );
		CHECK( offset % 4 == 0 && offset >= covered && offset + length <= translation.length );
		covered = offset + length;
	}

	// The records both formats share cross unchanged.
	for ( int lump : { LUMP_ENTITIES, LUMP_SHADERS, LUMP_PLANES, LUMP_NODES, LUMP_LEAFS,
		LUMP_LEAFSURFACES, LUMP_LEAFBRUSHES, LUMP_MODELS, LUMP_BRUSHES, LUMP_DRAWINDEXES,
		LUMP_FOGS, LUMP_LIGHTMAPS, LUMP_VISIBILITY } ) {
		const lump_t source = Directory( file, lump );
		CHECK( LittleLong( header.lumps[lump].filelen ) == LittleLong( source.filelen ) );
		CHECK( !std::memcmp( output + LittleLong( header.lumps[lump].fileofs ),
			file.data() + LittleLong( source.fileofs ), LittleLong( source.filelen ) ) );
	}

	// Brush sides keep their plane and shader and drop the draw surface.
	const auto *sides = reinterpret_cast<const dbrushside_t *>(
		output + LittleLong( header.lumps[LUMP_BRUSHSIDES].fileofs ) );
	CHECK( LittleLong( header.lumps[LUMP_BRUSHSIDES].filelen ) == 6 * int( sizeof( dbrushside_t ) ) );
	for ( int i = 0; i < 6; ++i ) {
		CHECK( LittleLong( sides[i].planeNum ) == i && LittleLong( sides[i].shaderNum ) == i % 2 );
	}

	// Vertices keep their position, texture coordinates, normal and the first
	// lightmap set and colour.
	const auto *verts = reinterpret_cast<const drawVert_t *>(
		output + LittleLong( header.lumps[LUMP_DRAWVERTS].fileofs ) );
	CHECK( LittleLong( header.lumps[LUMP_DRAWVERTS].filelen ) == 9 * int( sizeof( drawVert_t ) ) );
	for ( int i = 0; i < 9; ++i ) {
		CHECK( LittleFloat( verts[i].xyz[0] ) == float( i * 16 ) );
		CHECK( LittleFloat( verts[i].xyz[1] ) == float( i % 3 ) );
		CHECK( LittleFloat( verts[i].st[0] ) == float( i ) / 8.0f );
		CHECK( LittleFloat( verts[i].normal[2] ) == 1 );
		CHECK( LittleFloat( verts[i].lightmap[0] ) == 0.25f && LittleFloat( verts[i].lightmap[1] ) == 0.5f );
		for ( int c = 0; c < 4; ++c ) CHECK( verts[i].color.rgba[c] == c );
	}

	// Surfaces keep the collision-bearing fields and the first lightmap set.
	const auto *surfaces = reinterpret_cast<const dsurface_t *>(
		output + LittleLong( header.lumps[LUMP_SURFACES].fileofs ) );
	CHECK( LittleLong( header.lumps[LUMP_SURFACES].filelen ) == 2 * int( sizeof( dsurface_t ) ) );
	CHECK( LittleLong( surfaces[0].surfaceType ) == MST_PLANAR );
	CHECK( LittleLong( surfaces[0].shaderNum ) == 1 && LittleLong( surfaces[0].fogNum ) == -1 );
	CHECK( LittleLong( surfaces[0].numVerts ) == 4 && LittleLong( surfaces[0].numIndexes ) == 6 );
	CHECK( LittleLong( surfaces[0].lightmapWidth ) == 4 && LittleLong( surfaces[0].lightmapHeight ) == 4 );
	CHECK( LittleLong( surfaces[1].surfaceType ) == MST_PATCH );
	CHECK( LittleLong( surfaces[1].numVerts ) == 9 && LittleLong( surfaces[1].fogNum ) == 0 );
	CHECK( LittleLong( surfaces[1].patchWidth ) == 3 && LittleLong( surfaces[1].patchHeight ) == 3 );
	for ( int i = 0; i < 2; ++i ) {
		CHECK( LittleLong( surfaces[i].lightmapNum ) == 0 );
		CHECK( LittleLong( surfaces[i].lightmapX ) == 0 && LittleLong( surfaces[i].lightmapY ) == 0 );
		CHECK( LittleFloat( surfaces[i].lightmapOrigin[1] ) == 3 );
		CHECK( LittleFloat( surfaces[i].lightmapVecs[2][2] ) == 1 );
	}

	// The light grid keeps its first set and its direction.
	const byte *grid = output + LittleLong( header.lumps[LUMP_LIGHTGRID].fileofs );
	CHECK( LittleLong( header.lumps[LUMP_LIGHTGRID].filelen ) == 2 * RBSP_GRID_BYTES );
	for ( int point = 0; point < 2; ++point ) {
		const byte *record = grid + point * RBSP_GRID_BYTES;
		for ( int c = 0; c < 3; ++c ) {
			CHECK( record[c] == c + point && record[3 + c] == c + point );
		}
		CHECK( record[6] == 7 + point && record[7] == 9 + point );
	}

	Free( translation.data );
}

static void CheckRejections()
{
	const Bytes valid = Fixture();
	rbsp_translation_t translation{};
	char error[128];

	// A map of another format is never translated here.
	for ( int word : { 0, 1 } ) {
		Bytes file = valid;
		const int wrong = LittleLong( word ? 46 : BSP_IDENT );
		std::memcpy( file.data() + word * 4, &wrong, sizeof( wrong ) );
		allocations = 0;
		CHECK( !RBSP_TranslateToV46( file.data(), int( file.size() ), Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && !allocations && std::strstr( error, "wrong map format" ) );
	}

	// A lump that leaves the file, runs backwards or covers the directory is
	// refused before a single record is read.
	struct Damage { int lump, offset, length; };
	for ( auto damage : { Damage{ LUMP_PLANES, int( valid.size() ), 4 },
		{ LUMP_PLANES, -4, 4 }, { LUMP_NODES, 8, int( sizeof( dnode_t ) ) },
		{ LUMP_LEAFS, 0, int( sizeof( dleaf_t ) ) },
		{ LUMP_VISIBILITY, 64, -4 },
		{ LUMP_RBSP_LIGHTARRAY, int( valid.size() ) - 2, 8 } } ) {
		Bytes file = valid;
		SetDirectory( file, damage.lump, damage.offset, damage.length );
		allocations = 0;
		CHECK( !RBSP_TranslateToV46( file.data(), int( file.size() ), Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && !allocations && std::strstr( error, "lump directory" ) );
	}

	// A lump whose length is not a whole number of records is malformed, and
	// a record size from the wrong format is exactly that case.
	for ( int lump : { LUMP_BRUSHSIDES, LUMP_DRAWVERTS, LUMP_SURFACES, LUMP_LIGHTGRID } ) {
		Bytes file = valid;
		const lump_t source = Directory( file, lump );
		SetDirectory( file, lump, LittleLong( source.fileofs ), LittleLong( source.filelen ) - 1 );
		allocations = 0;
		CHECK( !RBSP_TranslateToV46( file.data(), int( file.size() ), Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && !allocations && std::strstr( error, "whole number of records" ) );
	}

	// A truncated header, a missing map and a missing target are all refused.
	for ( int length = 0; length < int( sizeof( dheaderRBSP_t ) ); ++length ) {
		CHECK( !RBSP_TranslateToV46( valid.data(), length, Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && std::strstr( error, "truncated" ) );
	}
	CHECK( !RBSP_TranslateToV46( nullptr, int( valid.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( !RBSP_TranslateToV46( valid.data(), int( valid.size() ), Alloc, Free,
		nullptr, error, sizeof( error ) ) );
	CHECK( !RBSP_TranslateToV46( valid.data(), int( valid.size() ), nullptr, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( !RBSP_TranslateToV46( valid.data(), int( valid.size() ), Alloc, nullptr,
		&translation, error, sizeof( error ) ) );

	// A refused allocation reports itself rather than handing back a map.
	allocations = 0;
	allocationFailure = 0;
	CHECK( !RBSP_TranslateToV46( valid.data(), int( valid.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( !translation.data && std::strstr( error, "allocate" ) );
	allocationFailure = -1;

	// An empty map with no content at all still translates into a valid
	// directory, because a profile may load a map fnTech3 did not build.
	dheaderRBSP_t header{};
	header.ident = LittleLong( RBSP_IDENT );
	header.version = LittleLong( RBSP_VERSION );
	Bytes empty( sizeof( header ) );
	std::memcpy( empty.data(), &header, sizeof( header ) );
	allocations = 0;
	CHECK( RBSP_TranslateToV46( empty.data(), int( empty.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( translation.data && translation.length == int( sizeof( dheader_t ) ) );
	Free( translation.data );

	// Reporting is optional: a caller may pass no error buffer at all.
	CHECK( !RBSP_TranslateToV46( empty.data(), 4, Alloc, Free, &translation, nullptr, 0 ) );
}

int main()
{
	CheckTranslation();
	CheckRejections();
	std::cout << "Jedi Academy RBSP v1 translation and malformed-map tests passed\n";
	return 0;
}
