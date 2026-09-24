/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_NOISE_HPP
#define FNTECH3_COMPAT_ALICE_NOISE_HPP

/* American McGee's Alice's renderer noise, which its client game asks for
 * through import 66 to shake beams.
 *
 * Retail builds the table once from R_Init (0x00492350): `srand( 1001 )`,
 * then for each of 256 entries a value `rand() * (1 / 32767.0f) * 2 - 1` and a
 * permutation byte `rand() * (255 / 32767.0f)`, both constants single floats
 * (0x004ff9cc, 0x0050035c) multiplied in the x87's wider precision, with the
 * Microsoft C runtime's generator (0x004e74c5). The lookup (0x004923c0) is
 * trilinear in the three coordinates between lattice corners and then linear
 * in time between two such values. Everything here is pure, so the same
 * arguments give the same value on every run and every platform. */

#include <cmath>

namespace fntech3 {
namespace alice {
namespace noise {

inline constexpr int Size = 256;
inline constexpr int Mask = Size - 1;

struct Table {
	float value[Size];
	int perm[Size];
};

/* The Microsoft C runtime's rand(). */
struct MsvcRand {
	unsigned int hold;
	int Next() {
		hold = hold * 214013u + 2531011u;
		return static_cast<int>( ( hold >> 16 ) & 0x7fff );
	}
};

inline Table Build() {
	Table table{};
	const double valueScale = static_cast<double>( 3.0518509e-05f );
	const double permScale = static_cast<double>( 0.0077822199f );
	MsvcRand rand{ 1001u };
	for ( int i = 0; i < Size; i++ ) {
		table.value[i] = static_cast<float>( rand.Next() * valueScale * 2.0 - 1.0 );
		table.perm[i] = static_cast<int>( rand.Next() * permScale ) & 0xff;
	}
	return table;
}

inline float Lerp( float a, float b, float w ) {
	return a * ( 1.0f - w ) + b * w;
}

inline float Get4f( const Table &table, float x, float y, float z, float t ) {
	const auto perm = [&table]( int i ) { return table.perm[i & Mask]; };
	const auto value = [&]( int ix, int iy, int iz, int it ) {
		return table.value[perm( ix + perm( iy + perm( iz + perm( it ) ) ) )];
	};
	const int ix = static_cast<int>( std::floor( x ) );
	const int iy = static_cast<int>( std::floor( y ) );
	const int iz = static_cast<int>( std::floor( z ) );
	const int it = static_cast<int>( std::floor( t ) );
	const float fx = x - ix, fy = y - iy, fz = z - iz, ft = t - it;
	float at[2];
	for ( int i = 0; i < 2; i++ ) {
		const float front = Lerp(
			Lerp( value( ix, iy, iz, it + i ), value( ix + 1, iy, iz, it + i ), fx ),
			Lerp( value( ix, iy + 1, iz, it + i ), value( ix + 1, iy + 1, iz, it + i ), fx ), fy );
		const float back = Lerp(
			Lerp( value( ix, iy, iz + 1, it + i ), value( ix + 1, iy, iz + 1, it + i ), fx ),
			Lerp( value( ix, iy + 1, iz + 1, it + i ), value( ix + 1, iy + 1, iz + 1, it + i ), fx ), fy );
		at[i] = Lerp( front, back, fz );
	}
	return Lerp( at[0], at[1], ft );
}

} // namespace noise
} // namespace alice
} // namespace fntech3

#endif
