/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_TIKI_LOD_HPP
#define FNTECH3_COMPAT_ALICE_TIKI_LOD_HPP

/* How American McGee's Alice drops a model's detail with distance.
 *
 * Header-only and free of engine state, so the renderers share one answer and
 * a test can pin it. Every rule was read out of retail alice.exe; the file
 * properties it relies on were checked on every surface the retail
 * installation ships (278 skeletal, 1,059 vertex-animated).
 *
 *   The fraction   is computed once an entity survives culling, from the
 *                  radius its current frame stores times the model's scale
 *                  and the entity's, projected at the model's origin
 *                  (R_AddTikiSurfaces 0x004a67b1..0x004a6853 for a
 *                  vertex-animated model, R_AddSkelSurfaces 0x004a3cac..
 *                  0x004a3dca for a skeletal one; the projection is Quake
 *                  III's, 0x004a1770):
 *                    projected 0  -> 1
 *                    otherwise    -> min(r_lodscale, 20) * lod_scale * projected
 *                  minus (r_lodbias + lod_bias), clamped to [0, 1]. A portal
 *                  view keeps its own fraction (entity +0x204) apart from
 *                  the main view's (+0x200).
 *   The count      of vertices a surface keeps is numVerts * fraction,
 *                  truncated, raised to the surface's minLod and held to
 *                  numVerts; a count of zero or less below minLod draws
 *                  nothing (0x0049fe05..0x0049fe3c vertex-animated,
 *                  0x004a001f..0x004a0069 skeletal).
 *   The remap      keeps vertex i below the count as itself and sends one at
 *                  or past it to wherever its collapse-map entry went; every
 *                  shipped map points each vertex at an earlier one, so the
 *                  walk is one pass (0x0049feb8..0x0049fef1).
 *   The triangles  are copied unchanged at full detail; below it, one whose
 *                  corners remap onto fewer than three vertices is dropped
 *                  (0x0049ff14..0x0049ff8b). */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace lod {

/* Retail's cap on r_lodscale (the float at 0x00500388). */
inline constexpr float MaxLodScale = 20.0f;

inline float Fraction( float projectedRadius, float lodScaleCvar, float lodBiasCvar,
	float modelLodScale, float modelLodBias ) {
	float fraction;
	/* Retail's x87 compares treat an unordered value as equal and as less:
	 * a NaN projection counts as zero, and a NaN fraction clamps to zero. */
	if ( projectedRadius == projectedRadius && projectedRadius != 0.0f ) {
		/* `fcom`/`test ah, 0x41` keeps a value at or under the cap, and an
		 * unordered one: only a larger one is replaced. */
		const float scale = lodScaleCvar > MaxLodScale ? MaxLodScale : lodScaleCvar;
		fraction = scale * modelLodScale * projectedRadius;
	} else {
		fraction = 1.0f;
	}
	fraction -= lodBiasCvar + modelLodBias;
	if ( !( fraction >= 0.0f ) ) return 0.0f;
	if ( fraction > 1.0f ) return 1.0f;
	return fraction;
}

/* How many of a surface's vertices a fraction keeps; 0 means draw nothing. */
inline int VertexCount( int numVerts, int minLod, float fraction ) {
	if ( numVerts <= 0 ) return 0;
	/* Retail multiplies on the x87 stack, without rounding to a float, and
	 * truncates with its ftol; a double keeps the product exact enough that
	 * the truncation lands on the same side of an integer. A fraction outside
	 * [0, 1] never reaches here, but the product is bounded regardless. */
	const double product = static_cast<double>( numVerts ) * static_cast<double>( fraction );
	int count = product >= static_cast<double>( numVerts ) ? numVerts
		: product >= 1.0 ? static_cast<int>( product ) : 0;
	if ( count < minLod ) {
		count = minLod;
	}
	/* Retail goes on with an empty surface when both are zero and remaps
	 * every triangle through slots it never set; nothing it draws survives
	 * that, so nothing is drawn. */
	if ( count <= 0 ) return 0;
	if ( count > numVerts ) count = numVerts;
	return count;
}

/* Fill map[0..numVerts) for `count` kept vertices from a surface's collapse
 * map, which is numVerts little-endian 32-bit indices at `collapse`. Refuses
 * an entry that does not name an earlier vertex, which no shipped surface
 * has and which retail would resolve through a slot not yet written. */
inline bool Remap( const unsigned char *collapse, int numVerts, int count, int *map ) {
	if ( numVerts <= 0 || count <= 0 || count > numVerts ) return false;
	for ( int i = 0; i < count; ++i ) map[i] = i;
	for ( int i = count; i < numVerts; ++i ) {
		std::int32_t to;
		std::memcpy( &to, collapse + 4u * static_cast<std::size_t>( i ), sizeof( to ) );
		if ( to < 0 || to >= i ) return false;
		map[i] = map[to];
	}
	return true;
}

/* Write a surface's triangles as indexes into the kept vertices, offset by
 * `base`. At full detail every triangle is copied as it is; below it a
 * triangle collapsed onto an edge or a point is dropped. Returns the number of
 * indexes written, or -1 for a corner that names no vertex of the surface. */
inline int Triangles( const unsigned char *triangles, int numTriangles, int numVerts,
	int count, const int *map, unsigned int base, unsigned int *out ) {
	int written = 0;
	for ( int t = 0; t < numTriangles; ++t ) {
		std::int32_t corner[3];
		std::memcpy( corner, triangles + 12u * static_cast<std::size_t>( t ), sizeof( corner ) );
		for ( int k = 0; k < 3; ++k ) {
			if ( corner[k] < 0 || corner[k] >= numVerts ) return -1;
		}
		if ( count >= numVerts ) {
			for ( int k = 0; k < 3; ++k ) out[written++] = base + static_cast<unsigned int>( corner[k] );
			continue;
		}
		const int a = map[corner[0]], b = map[corner[1]], c = map[corner[2]];
		if ( a == b || b == c || c == a ) continue;
		out[written++] = base + static_cast<unsigned int>( a );
		out[written++] = base + static_cast<unsigned int>( b );
		out[written++] = base + static_cast<unsigned int>( c );
	}
	return written;
}

} // namespace lod
} // namespace alice
} // namespace fntech3

#endif
