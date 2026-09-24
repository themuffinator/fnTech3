/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for how Alice drops a model's detail with
 * distance (code/compat/alice_tiki_lod.hpp). Every surface here is written by
 * this test; no retail file is used. The rules were read out of retail
 * alice.exe (R_AddTikiSurfaces 0x004a67b1, R_AddSkelSurfaces 0x004a3cac, the
 * surface builders at 0x0049fe05 and 0x004a001f); these checks pin them so a
 * later change cannot swap the bias's sign, round instead of truncate, forget
 * the minLod floor or keep a collapsed triangle. */

#include "../code/compat/alice_tiki_lod.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::lod;

static bool Near( float a, float b, float eps = 1e-6f )
{
	return std::fabs( a - b ) <= eps;
}

static std::vector<unsigned char> Ints( const std::vector<std::int32_t> &values )
{
	std::vector<unsigned char> out( values.size() * 4 );
	std::memcpy( out.data(), values.data(), out.size() );
	return out;
}

static void TestFraction()
{
	/* Retail's defaults: r_lodscale 5, r_lodbias 0, a definition without
	 * lod_scale (1) or lod_bias (0). */
	CHECK( Near( Fraction( 0.1f, 5.0f, 0.0f, 1.0f, 0.0f ), 0.5f ) );
	CHECK( Near( Fraction( 0.5f, 5.0f, 0.0f, 1.0f, 0.0f ), 1.0f ) );
	/* `lod_scale 1` is stored as 0.2. */
	CHECK( Near( Fraction( 0.5f, 5.0f, 0.0f, 0.2f, 0.0f ), 0.5f ) );
	/* Nothing projected (behind the view, or no radius) is full detail
	 * before the bias. */
	CHECK( Near( Fraction( 0.0f, 5.0f, 0.0f, 1.0f, 0.0f ), 1.0f ) );
	CHECK( Near( Fraction( 0.0f, 5.0f, 0.25f, 1.0f, 0.0f ), 0.75f ) );
	/* Both biases are subtracted, the cvar's and the definition's. */
	CHECK( Near( Fraction( 0.1f, 5.0f, 0.1f, 1.0f, 0.15f ), 0.25f ) );
	/* A negative bias raises the detail, still no higher than 1. */
	CHECK( Near( Fraction( 0.1f, 5.0f, -0.25f, 1.0f, 0.0f ), 0.75f ) );
	CHECK( Near( Fraction( 0.1f, 5.0f, -2.0f, 1.0f, 0.0f ), 1.0f ) );
	/* Clamped at zero. */
	CHECK( Near( Fraction( 0.01f, 5.0f, 1.0f, 1.0f, 0.0f ), 0.0f ) );
	/* r_lodscale is capped at 20 and not raised. */
	CHECK( Near( Fraction( 0.01f, 100.0f, 0.0f, 1.0f, 0.0f ), 0.2f ) );
	CHECK( Near( Fraction( 0.01f, 20.0f, 0.0f, 1.0f, 0.0f ), 0.2f ) );
	CHECK( Near( Fraction( 0.01f, 1.0f, 0.0f, 1.0f, 0.0f ), 0.01f ) );
	/* Unordered values follow the x87 flags: a NaN projection is zero, a NaN
	 * fraction clamps to zero. */
	const float nan = std::numeric_limits<float>::quiet_NaN();
	CHECK( Near( Fraction( nan, 5.0f, 0.0f, 1.0f, 0.0f ), 1.0f ) );
	CHECK( Near( Fraction( 0.1f, 5.0f, nan, 1.0f, 0.0f ), 0.0f ) );
}

static void TestVertexCount()
{
	/* Truncated, not rounded. */
	CHECK( VertexCount( 81, 43, 0.99f ) == 80 );
	CHECK( VertexCount( 100, 10, 0.555f ) == 55 );
	/* Raised to minLod. */
	CHECK( VertexCount( 81, 43, 0.1f ) == 43 );
	CHECK( VertexCount( 81, 43, 0.0f ) == 43 );
	/* Held to the surface. */
	CHECK( VertexCount( 81, 43, 1.0f ) == 81 );
	CHECK( VertexCount( 5, 9, 0.5f ) == 5 );
	/* A surface with no minLod at no detail draws nothing. */
	CHECK( VertexCount( 81, 0, 0.0f ) == 0 );
	CHECK( VertexCount( 81, 0, 0.01f ) == 0 );
	CHECK( VertexCount( 81, 0, 0.02f ) == 1 );
	CHECK( VertexCount( 0, 0, 1.0f ) == 0 );
	/* The product is truncated, never rounded up to the next vertex: just
	 * under a third of three keeps none. */
	CHECK( VertexCount( 3, 0, 1.0f / 3.0f ) == 1 );
	CHECK( VertexCount( 3, 0, std::nextafter( 1.0f / 3.0f, 0.0f ) ) == 0 );
}

static void TestRemapAndTriangles()
{
	/* A strip of four triangles over six vertices:
	 *   0-1-2, 1-3-2, 2-3-4, 3-5-4
	 * whose collapse map folds 5 onto 3, 4 onto 2, 3 onto 1. */
	const auto collapse = Ints( { 0, 0, 1, 1, 2, 3 } );
	const auto tris = Ints( { 0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4 } );
	int map[6];
	unsigned int out[12];

	/* Full detail copies every triangle, offset by the base. */
	CHECK( Remap( collapse.data(), 6, 6, map ) );
	CHECK( Triangles( tris.data(), 4, 6, 6, map, 100, out ) == 12 );
	CHECK( out[0] == 100 && out[1] == 101 && out[2] == 102 );
	CHECK( out[9] == 103 && out[10] == 105 && out[11] == 104 );

	/* Four kept: 5 -> 3, 4 -> 2. The last triangle becomes 3-3-2 and goes. */
	CHECK( Remap( collapse.data(), 6, 4, map ) );
	CHECK( map[4] == 2 && map[5] == 3 );
	CHECK( Triangles( tris.data(), 4, 6, 4, map, 0, out ) == 6 );
	CHECK( out[0] == 0 && out[1] == 1 && out[2] == 2 );
	CHECK( out[3] == 1 && out[4] == 3 && out[5] == 2 );

	/* Three kept: 3 -> 1, and 5 follows 3 to 1. Only the first survives. */
	CHECK( Remap( collapse.data(), 6, 3, map ) );
	CHECK( map[3] == 1 && map[5] == 1 && map[4] == 2 );
	CHECK( Triangles( tris.data(), 4, 6, 3, map, 7, out ) == 3 );
	CHECK( out[0] == 7 && out[1] == 8 && out[2] == 9 );

	/* A map entry must name an earlier vertex. */
	const auto forward = Ints( { 0, 0, 3, 1, 2, 3 } );
	CHECK( !Remap( forward.data(), 6, 2, map ) );
	const auto negative = Ints( { 0, 0, 1, -1, 2, 3 } );
	CHECK( !Remap( negative.data(), 6, 3, map ) );
	/* Entries below the count are not read. */
	CHECK( Remap( forward.data(), 6, 3, map ) );
	CHECK( map[3] == 1 && map[5] == 1 );
	CHECK( Remap( forward.data(), 6, 6, map ) );
	CHECK( !Remap( collapse.data(), 6, 0, map ) );
	CHECK( !Remap( collapse.data(), 6, 7, map ) );

	/* A corner outside the surface is refused at any detail. */
	const auto bad = Ints( { 0, 1, 6 } );
	CHECK( Triangles( bad.data(), 1, 6, 6, map, 0, out ) == -1 );
	CHECK( Remap( collapse.data(), 6, 4, map ) );
	CHECK( Triangles( bad.data(), 1, 6, 4, map, 0, out ) == -1 );

	/* An authored degenerate survives at full detail and not below it. */
	const auto flat = Ints( { 0, 1, 1, 0, 1, 2 } );
	CHECK( Remap( collapse.data(), 6, 6, map ) );
	CHECK( Triangles( flat.data(), 2, 6, 6, map, 0, out ) == 6 );
	CHECK( Remap( collapse.data(), 6, 5, map ) );
	CHECK( Triangles( flat.data(), 2, 6, 5, map, 0, out ) == 3 );
	CHECK( out[0] == 0 && out[1] == 1 && out[2] == 2 );
}

int main()
{
	TestFraction();
	TestVertexCount();
	TestRemapAndTriangles();
	std::cout << "alice tiki lod tests passed\n";
	return 0;
}
