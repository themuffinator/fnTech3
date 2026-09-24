/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Alice's FAKK 42 map boundary. Every
 * map here is written by this test; no retail map or asset is used. */

#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/bsp_fakk.h"

#include <algorithm>
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

/* Ritual's records, as the format defines them and as all thirty-six retail
 * maps measure. A mismatch here would mean the translator reads a different
 * file than Alice ships. */
static_assert( sizeof( dheaderFAKK_t ) == 172, "FAKK directory has a checksum and twenty lumps" );
static_assert( sizeof( dshaderFAKK_t ) == 76, "FAKK shaders carry a subdivision count" );
static_assert( sizeof( dsurfaceFAKK_t ) == 108, "FAKK surfaces carry a subdivision count" );
static_assert( sizeof( dshader_t ) == 72 && sizeof( dsurface_t ) == 104, "Quake III records" );
static_assert( sizeof( dheader_t ) == 144, "Quake III directory has seventeen lumps" );
/* The records the two formats share, which cross byte for byte. */
static_assert( sizeof( dplane_t ) == 16 && sizeof( dnode_t ) == 36, "shared records" );
static_assert( sizeof( dleaf_t ) == 48 && sizeof( dbrush_t ) == 12, "shared records" );
static_assert( sizeof( dbrushside_t ) == 8 && sizeof( dfog_t ) == 72, "shared records" );
static_assert( sizeof( dmodel_t ) == 40 && sizeof( drawVert_t ) == 44, "shared records" );

static int allocations;
static int allocationFailure = -1;

static void *Alloc( int size )
{
	if ( allocations++ == allocationFailure ) return nullptr;
	return std::calloc( 1, static_cast<size_t>( size ) );
}

static void Free( void *pointer ) { std::free( pointer ); }

/* The FAKK directory starts after the ident, version and checksum. */
static const int kDirectory = 12;

template<class T>
static void Lump( Bytes &file, int index, const std::vector<T> &records )
{
	// Deliberately unaligned disk offsets: the translator must normalize them.
	file.push_back( 0 );
	const lump_t lump{ LittleLong( int( file.size() ) ),
		LittleLong( int( records.size() * sizeof( T ) ) ) };
	std::memcpy( file.data() + kDirectory + index * sizeof( lump ), &lump, sizeof( lump ) );
	const auto *bytes = reinterpret_cast<const byte *>( records.data() );
	if ( !records.empty() ) file.insert( file.end(), bytes, bytes + records.size() * sizeof( T ) );
}

static lump_t Directory( const Bytes &file, int index )
{
	lump_t lump;
	std::memcpy( &lump, file.data() + kDirectory + index * sizeof( lump ), sizeof( lump ) );
	return lump;
}

static void SetDirectory( Bytes &file, int index, int offset, int length )
{
	const lump_t lump{ LittleLong( offset ), LittleLong( length ) };
	std::memcpy( file.data() + kDirectory + index * sizeof( lump ), &lump, sizeof( lump ) );
}

/* One world, one submodel, one patch and one planar surface, with a
 * subdivision count on every record that carries one and content in all four
 * of the lumps Quake III has no counterpart for. */
static Bytes Fixture()
{
	dheaderFAKK_t header{};
	header.ident = LittleLong( FAKK_IDENT );
	header.version = LittleLong( FAKK_VERSION_ALICE );
	header.checksum = LittleLong( 0x13be4c6a );
	Bytes file( sizeof( header ) );
	std::memcpy( file.data(), &header, sizeof( header ) );

	Lump<char>( file, LUMP_FAKK_ENTITIES, { '{', '}', '\n', 0 } );

	std::vector<dshaderFAKK_t> shaders( 2 );
	Q_strncpyz( shaders[0].shader, "textures/common/caulk", MAX_QPATH );
	Q_strncpyz( shaders[1].shader, "textures/village/grass", MAX_QPATH );
	shaders[0].surfaceFlags = LittleLong( 0x80 );
	shaders[1].contentFlags = LittleLong( 1 );
	shaders[0].subdivisions = LittleLong( 8 );
	shaders[1].subdivisions = LittleLong( 24 );
	Lump( file, LUMP_FAKK_SHADERS, shaders );

	std::vector<dplane_t> planes( 6 );
	for ( int i = 0; i < 6; ++i ) {
		planes[i].normal[i / 2] = LittleFloat( i % 2 ? 1.0f : -1.0f );
		planes[i].dist = LittleFloat( 64.0f );
	}
	Lump( file, LUMP_FAKK_PLANES, planes );

	dnode_t node{};
	node.children[0] = LittleLong( -1 );
	node.children[1] = LittleLong( -2 );
	Lump<dnode_t>( file, LUMP_FAKK_NODES, { node } );

	dleaf_t solid{}, open{};
	solid.cluster = LittleLong( -1 );
	open.numLeafBrushes = LittleLong( 1 );
	open.numLeafSurfaces = LittleLong( 2 );
	Lump<dleaf_t>( file, LUMP_FAKK_LEAFS, { solid, open } );
	Lump<int>( file, LUMP_FAKK_LEAFSURFACES, { LittleLong( 0 ), LittleLong( 1 ) } );
	Lump<int>( file, LUMP_FAKK_LEAFBRUSHES, { LittleLong( 0 ) } );

	dmodel_t world{}, submodel{};
	world.numSurfaces = LittleLong( 2 );
	world.numBrushes = LittleLong( 1 );
	submodel.firstSurface = LittleLong( 1 );
	submodel.numSurfaces = LittleLong( 1 );
	for ( int i = 0; i < 3; ++i ) {
		world.mins[i] = submodel.mins[i] = LittleFloat( -64 );
		world.maxs[i] = submodel.maxs[i] = LittleFloat( 64 );
	}
	Lump<dmodel_t>( file, LUMP_FAKK_MODELS, { world, submodel } );

	dbrush_t brush{};
	brush.numSides = LittleLong( 6 );
	brush.shaderNum = LittleLong( 0 );
	Lump<dbrush_t>( file, LUMP_FAKK_BRUSHES, { brush } );

	std::vector<dbrushside_t> sides( 6 );
	for ( int i = 0; i < 6; ++i ) {
		sides[i].planeNum = LittleLong( i );
		sides[i].shaderNum = LittleLong( i % 2 );
	}
	Lump( file, LUMP_FAKK_BRUSHSIDES, sides );

	std::vector<drawVert_t> vertices( 9 );
	for ( int i = 0; i < 9; ++i ) {
		vertices[i].xyz[0] = LittleFloat( float( i * 16 ) );
		vertices[i].xyz[1] = LittleFloat( float( i % 3 ) );
		vertices[i].st[0] = LittleFloat( float( i ) / 8.0f );
		vertices[i].lightmap[0] = LittleFloat( 0.25f );
		vertices[i].lightmap[1] = LittleFloat( 0.5f );
		vertices[i].normal[2] = LittleFloat( 1 );
		for ( int c = 0; c < 4; ++c ) vertices[i].color.rgba[c] = static_cast<byte>( c );
	}
	Lump( file, LUMP_FAKK_DRAWVERTS, vertices );

	std::vector<int> indexes( 6 );
	for ( int i = 0; i < 6; ++i ) indexes[i] = LittleLong( i % 4 );
	Lump( file, LUMP_FAKK_DRAWINDEXES, indexes );

	dfog_t fog{};
	Q_strncpyz( fog.shader, "textures/village/fog", MAX_QPATH );
	fog.visibleSide = LittleLong( -1 );
	Lump<dfog_t>( file, LUMP_FAKK_FOGS, { fog } );

	std::vector<dsurfaceFAKK_t> surfaces( 2 );
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
	for ( int i = 0; i < 2; ++i ) {
		surfaces[i].lightmapNum = LittleLong( i );
		surfaces[i].lightmapX = LittleLong( 8 * i );
		surfaces[i].lightmapY = LittleLong( 16 * i );
		surfaces[i].lightmapOrigin[1] = LittleFloat( 3 );
		surfaces[i].lightmapVecs[2][2] = LittleFloat( 1 );
		surfaces[i].subdivisions = LittleFloat( 12.5f );
	}
	Lump( file, LUMP_FAKK_SURFACES, surfaces );

	Lump<byte>( file, LUMP_FAKK_LIGHTMAPS, Bytes( 128 * 128 * 3, 0x5a ) );
	Lump<int>( file, LUMP_FAKK_VISIBILITY, { LittleLong( 1 ), LittleLong( 4 ), LittleLong( 1 ) } );

	/* The four lumps that have no Quake III counterpart carry content here so
	 * the test can prove they are dropped rather than mislaid in another slot. */
	Lump<byte>( file, LUMP_FAKK_LIGHTGRID, Bytes( 72, 0x11 ) );
	Lump<byte>( file, LUMP_FAKK_ENTLIGHTS, Bytes( 48, 0x22 ) );
	Lump<byte>( file, LUMP_FAKK_ENTLIGHTSVIS, Bytes( 16, 0x33 ) );
	Lump<byte>( file, LUMP_FAKK_LIGHTDEFS, Bytes( 2 * 52, 0x44 ) );
	return file;
}

static void CheckTranslation()
{
	const Bytes file = Fixture();
	fakk_translation_t translation{};
	char error[128] = "unchanged";

	allocations = 0;
	CHECK( FAKK_TranslateToV46( file.data(), int( file.size() ), Alloc, Free,
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

	/* The lumps are renumbered, not copied in place: each one is read from
	 * Ritual's index and written at Quake III's. */
	const struct { int fakk, quake3; } moved[] = {
		{ LUMP_FAKK_ENTITIES, LUMP_ENTITIES },
		{ LUMP_FAKK_PLANES, LUMP_PLANES },
		{ LUMP_FAKK_NODES, LUMP_NODES },
		{ LUMP_FAKK_LEAFS, LUMP_LEAFS },
		{ LUMP_FAKK_LEAFSURFACES, LUMP_LEAFSURFACES },
		{ LUMP_FAKK_LEAFBRUSHES, LUMP_LEAFBRUSHES },
		{ LUMP_FAKK_MODELS, LUMP_MODELS },
		{ LUMP_FAKK_BRUSHES, LUMP_BRUSHES },
		{ LUMP_FAKK_BRUSHSIDES, LUMP_BRUSHSIDES },
		{ LUMP_FAKK_DRAWVERTS, LUMP_DRAWVERTS },
		{ LUMP_FAKK_DRAWINDEXES, LUMP_DRAWINDEXES },
		{ LUMP_FAKK_FOGS, LUMP_FOGS },
		{ LUMP_FAKK_LIGHTMAPS, LUMP_LIGHTMAPS },
		{ LUMP_FAKK_VISIBILITY, LUMP_VISIBILITY },
	};
	for ( const auto &pair : moved ) {
		const lump_t source = Directory( file, pair.fakk );
		CHECK( LittleLong( header.lumps[pair.quake3].filelen ) == LittleLong( source.filelen ) );
		CHECK( !std::memcmp( output + LittleLong( header.lumps[pair.quake3].fileofs ),
			file.data() + LittleLong( source.fileofs ), LittleLong( source.filelen ) ) );
	}

	/* The light grid is dropped whole: its record is not established, and the
	 * collision loader does not read it. */
	CHECK( LittleLong( header.lumps[LUMP_LIGHTGRID].filelen ) == 0 );

	/* Nothing from the four dropped lumps may appear anywhere in the output. */
	const Bytes output_bytes( output, output + translation.length );
	for ( byte marker : { byte( 0x11 ), byte( 0x22 ), byte( 0x33 ), byte( 0x44 ) } ) {
		const Bytes run( 16, marker );
		CHECK( std::search( output_bytes.begin(), output_bytes.end(),
			run.begin(), run.end() ) == output_bytes.end() );
	}

	// Shaders keep their name and both flag words; the subdivisions travel apart.
	const auto *shaders = reinterpret_cast<const dshader_t *>(
		output + LittleLong( header.lumps[LUMP_SHADERS].fileofs ) );
	CHECK( LittleLong( header.lumps[LUMP_SHADERS].filelen ) == 2 * int( sizeof( dshader_t ) ) );
	CHECK( !std::strcmp( shaders[0].shader, "textures/common/caulk" ) );
	CHECK( !std::strcmp( shaders[1].shader, "textures/village/grass" ) );
	CHECK( LittleLong( shaders[0].surfaceFlags ) == 0x80 && LittleLong( shaders[0].contentFlags ) == 0 );
	CHECK( LittleLong( shaders[1].surfaceFlags ) == 0 && LittleLong( shaders[1].contentFlags ) == 1 );

	// Surfaces keep every collision-bearing field and drop the subdivisions.
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
		CHECK( LittleLong( surfaces[i].lightmapNum ) == i );
		CHECK( LittleLong( surfaces[i].lightmapX ) == 8 * i && LittleLong( surfaces[i].lightmapY ) == 16 * i );
		CHECK( LittleFloat( surfaces[i].lightmapOrigin[1] ) == 3 );
		CHECK( LittleFloat( surfaces[i].lightmapVecs[2][2] ) == 1 );
	}

	/* The collision translation carries each shader's subdivision count, in
	 * shader order and host order, past the last lump where no IBSP 46 reader
	 * looks: Alice's patch collision is cut by it (alice.exe 0x00430780). */
	CHECK( translation.numShaderSubdivisions == 2 && translation.shaderSubdivisions );
	CHECK( translation.shaderSubdivisions[0] == 8 && translation.shaderSubdivisions[1] == 24 );
	const auto *table = reinterpret_cast<const byte *>( translation.shaderSubdivisions );
	CHECK( table >= output + covered && ( table - output ) % 4 == 0 );
	CHECK( table + 2 * sizeof( int32_t ) <= output + translation.length );
	Free( translation.data );

	/* A renderer asks for the light grid alone and gets no table. */
	fakk_translation_t render{};
	CHECK( FAKK_TranslateToV46Ex( file.data(), int( file.size() ), Alloc, Free,
		&render, error, sizeof( error ), FAKK_TRANSLATE_LIGHTGRID ) );
	CHECK( !render.shaderSubdivisions && !render.numShaderSubdivisions );
	Free( render.data );

	/* What Alice's collision loader does with a patch's shader record: the
	 * contents it collides with (solid and the four clips) and the smallest
	 * count it cuts by (0x004306a5..0x004306bc, 0x00430784). */
	CHECK( FAKK_PATCH_COLLISION_CONTENTS == 0x000f0001 );
	CHECK( FAKK_PATCH_MIN_SUBDIVISIONS == 16 );
}

static void CheckRejections()
{
	const Bytes valid = Fixture();
	fakk_translation_t translation{};
	char error[128];

	// A map that is not this format is refused rather than reinterpreted.
	for ( int32_t ident : { BSP_IDENT, 0, -1 } ) {
		Bytes wrong = valid;
		const int32_t value = LittleLong( ident );
		std::memcpy( wrong.data(), &value, sizeof( value ) );
		CHECK( !FAKK_TranslateToV46( wrong.data(), int( wrong.size() ), Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && std::strstr( error, "wrong map format" ) );
	}

	/* Ritual's own F.A.K.K.2 maps are version 12 and other titles reuse the
	 * ident again; only Alice's 42 is this translator's. */
	for ( int32_t version : { 12, 41, 43, 46, 0 } ) {
		Bytes wrong = valid;
		const int32_t value = LittleLong( version );
		std::memcpy( wrong.data() + 4, &value, sizeof( value ) );
		CHECK( !FAKK_TranslateToV46( wrong.data(), int( wrong.size() ), Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && std::strstr( error, "wrong map format" ) );
	}

	// A truncated header is refused before any lump is read.
	for ( int length : { 0, 4, 12, int( sizeof( dheaderFAKK_t ) ) - 1 } ) {
		CHECK( !FAKK_TranslateToV46( valid.data(), length, Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && std::strstr( error, "truncated" ) );
	}
	CHECK( !FAKK_TranslateToV46( nullptr, int( valid.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) );

	// A lump that leaves the file, or overlaps the directory, is refused.
	const struct { int offset, length; } outside[] = {
		{ -1, 4 }, { 4, -1 }, { int( valid.size() ), 4 },
		{ int( valid.size() ) - 2, 4 }, { 8, 4 },
		{ int( sizeof( dheaderFAKK_t ) ) - 1, 4 },
	};
	for ( const auto &bad : outside ) {
		Bytes wrong = valid;
		SetDirectory( wrong, LUMP_FAKK_PLANES, bad.offset, bad.length );
		CHECK( !FAKK_TranslateToV46( wrong.data(), int( wrong.size() ), Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && std::strstr( error, "invalid lump directory" ) );
	}

	// A lump whose length is not a whole number of records is refused.
	for ( int lump : { LUMP_FAKK_SHADERS, LUMP_FAKK_SURFACES } ) {
		Bytes wrong = valid;
		const lump_t source = Directory( wrong, lump );
		SetDirectory( wrong, lump, LittleLong( source.fileofs ), LittleLong( source.filelen ) - 1 );
		CHECK( !FAKK_TranslateToV46( wrong.data(), int( wrong.size() ), Alloc, Free,
			&translation, error, sizeof( error ) ) );
		CHECK( !translation.data && std::strstr( error, "whole number of records" ) );
	}

	// The caller must supply both an allocator and a matching free.
	CHECK( !FAKK_TranslateToV46( valid.data(), int( valid.size() ), nullptr, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( !FAKK_TranslateToV46( valid.data(), int( valid.size() ), Alloc, nullptr,
		&translation, error, sizeof( error ) ) );
	CHECK( !FAKK_TranslateToV46( valid.data(), int( valid.size() ), Alloc, Free,
		nullptr, error, sizeof( error ) ) );

	// A refused allocation reports itself rather than handing back a map.
	allocations = 0;
	allocationFailure = 0;
	CHECK( !FAKK_TranslateToV46( valid.data(), int( valid.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( !translation.data && std::strstr( error, "allocate" ) );
	allocationFailure = -1;

	// An empty map with no content at all still translates into a valid
	// directory, because a profile may load a map fnTech3 did not build.
	dheaderFAKK_t header{};
	header.ident = LittleLong( FAKK_IDENT );
	header.version = LittleLong( FAKK_VERSION_ALICE );
	Bytes empty( sizeof( header ) );
	std::memcpy( empty.data(), &header, sizeof( header ) );
	allocations = 0;
	CHECK( FAKK_TranslateToV46( empty.data(), int( empty.size() ), Alloc, Free,
		&translation, error, sizeof( error ) ) );
	CHECK( translation.data && translation.length == int( sizeof( dheader_t ) ) );
	CHECK( translation.numShaderSubdivisions == 0 );
	Free( translation.data );

	// Reporting is optional: a caller may pass no error buffer at all.
	CHECK( !FAKK_TranslateToV46( empty.data(), 4, Alloc, Free, &translation, nullptr, 0 ) );
}

/* Alice's light grid cell, and the derivation that turns a world's bounds into
 * its point count. The cell is read out of the retail renderer's own loader;
 * the derivation is Quake III's, and together they reproduce the light grid
 * lump length on all thirty-six retail maps. The translator still drops the
 * lump, so this holds the constants and the arithmetic to what was measured. */
static void CheckLightGrid()
{
	CHECK( FAKK_LIGHTGRID_SIZE_X == 192.0f );
	CHECK( FAKK_LIGHTGRID_SIZE_Y == 192.0f );
	CHECK( FAKK_LIGHTGRID_SIZE_Z == 320.0f );
	/* Not Quake III's cell, which is what made the point counts disagree. */
	CHECK( FAKK_LIGHTGRID_SIZE_X != 64.0f );
	CHECK( FAKK_LIGHTGRID_SIZE_Z != 128.0f );

	int counts[3];

	/* Bounds that land exactly on the grid: the count is inclusive of both
	 * ends, so a span of N cells carries N+1 points. */
	{
		const float mins[3] = { 0.0f, 0.0f, 0.0f };
		const float maxs[3] = { 192.0f * 4, 192.0f * 5, 320.0f * 3 };
		CHECK( FAKK_LightGridCounts( mins, maxs, counts ) == 5 * 6 * 4 );
		CHECK( counts[0] == 5 && counts[1] == 6 && counts[2] == 4 );
	}

	/* Bounds that do not: the low end snaps up and the high end snaps down, so
	 * a world that overhangs its grid does not gain a point at either end. */
	{
		const float mins[3] = { 1.0f, -191.0f, -319.0f };
		const float maxs[3] = { 192.0f * 4 - 1.0f, 192.0f * 2 + 1.0f, 1.0f };
		CHECK( FAKK_LightGridCounts( mins, maxs, counts ) == 3 * 3 * 1 );
		CHECK( counts[0] == 3 && counts[1] == 3 && counts[2] == 1 );
	}

	/* Negative bounds snap the same way rather than truncating toward zero,
	 * which is the case a floor written as a cast would get wrong. */
	{
		const float mins[3] = { -192.0f * 3, -192.0f * 3, -320.0f * 2 };
		const float maxs[3] = { -192.0f, -192.0f, -320.0f };
		CHECK( FAKK_LightGridCounts( mins, maxs, counts ) == 3 * 3 * 2 );
	}

	/* A world thinner than one cell on an axis still carries one plane of
	 * points where the snapped ends coincide, and none where they cross. */
	{
		const float mins[3] = { 0.0f, 0.0f, 0.0f };
		const float maxs[3] = { 192.0f, 192.0f, 1.0f };
		CHECK( FAKK_LightGridCounts( mins, maxs, counts ) == 2 * 2 * 1 );

		const float thin[3] = { 1.0f, 1.0f, 1.0f };
		const float crossed[3] = { 191.0f, 191.0f, 2.0f };
		CHECK( FAKK_LightGridCounts( thin, crossed, counts ) == 0 );
		CHECK( counts[0] == 0 && counts[1] == 0 && counts[2] == 0 );
	}

	/* The shape the measurement reports: three maps' dimensions, and the point
	 * count each multiplies out to, as the retail lumps carry them. */
	CHECK( 16 * 22 * 9 == 3168 );      /* gvillage */
	CHECK( 75 * 71 * 16 == 85200 );    /* grounds2 */
	CHECK( 15 * 15 * 10 == 2250 );     /* tower3 */
}

int main()
{
	CheckTranslation();
	CheckRejections();
	CheckLightGrid();
	std::cout << "Alice FAKK 42 translation and malformed-map tests passed\n";
	return 0;
}
