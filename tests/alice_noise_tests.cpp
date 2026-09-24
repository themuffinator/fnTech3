/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's renderer noise (compat/alice_noise.hpp). The table is what retail
 * builds from srand( 1001 ) with the Microsoft C runtime's generator; the
 * first values are pinned here so a change to the generator, the seed or the
 * scaling fails, and the lookup is checked against the lattice it
 * interpolates. */

#include "../code/compat/alice_noise.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::noise;

static bool Near( float a, float b, float eps = 1e-6f ) {
	return std::fabs( a - b ) <= eps;
}

int main() {
	/* The generator itself: holdrand * 214013 + 2531011, bits 16 to 30. */
	MsvcRand rand{ 1001u };
	const int first = rand.Next();
	CHECK( first == static_cast<int>( ( ( 1001u * 214013u + 2531011u ) >> 16 ) & 0x7fff ) );

	const Table table = Build();
	CHECK( Near( table.value[0], -0.79815060f ) );
	CHECK( Near( table.value[1], -0.27091280f ) );
	CHECK( Near( table.value[2], -0.27829829f ) );
	CHECK( Near( table.value[3], -0.33048493f ) );
	CHECK( Near( table.value[255], 0.97717214f ) );
	const int perm[8] = { 147, 41, 80, 143, 31, 51, 119, 33 };
	for ( int i = 0; i < 8; i++ ) {
		CHECK( table.perm[i] == perm[i] );
	}
	CHECK( table.perm[255] == 145 );
	for ( int i = 0; i < Size; i++ ) {
		CHECK( table.value[i] >= -1.0f && table.value[i] <= 1.0f );
		CHECK( table.perm[i] >= 0 && table.perm[i] <= 255 );
	}

	/* On a lattice point the lookup is the table entry the permutation
	 * chain names; halfway along an axis it is the mean of the two ends. */
	const auto lattice = [&table]( int x, int y, int z, int t ) {
		const auto p = [&table]( int i ) { return table.perm[i & Mask]; };
		return table.value[p( x + p( y + p( z + p( t ) ) ) )];
	};
	CHECK( Near( Get4f( table, 3.0f, 5.0f, 7.0f, 11.0f ), lattice( 3, 5, 7, 11 ) ) );
	CHECK( Near( Get4f( table, -2.0f, 0.0f, 9.0f, 1.0f ), lattice( -2, 0, 9, 1 ) ) );
	CHECK( Near( Get4f( table, 3.5f, 5.0f, 7.0f, 11.0f ),
		0.5f * ( lattice( 3, 5, 7, 11 ) + lattice( 4, 5, 7, 11 ) ) ) );
	CHECK( Near( Get4f( table, 3.0f, 5.0f, 7.0f, 11.25f ),
		0.75f * lattice( 3, 5, 7, 11 ) + 0.25f * lattice( 3, 5, 7, 12 ) ) );
	/* Pure: the same arguments, the same answer. */
	CHECK( Get4f( table, 12.3f, -4.5f, 6.7f, 1000.0f ) == Get4f( Build(), 12.3f, -4.5f, 6.7f, 1000.0f ) );

	std::cout << "alice noise tests passed\n";
	return 0;
}
