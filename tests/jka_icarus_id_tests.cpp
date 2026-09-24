/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Hold Jedi Academy's ICARUS numbers to Raven's own enumerations, and hold
 * those to what a retail installation's scripts showed before the enumerations
 * were read. The two were arrived at separately, so where they agree the
 * agreement is worth asserting: a table edited by hand would break it.
 *
 * Nothing here runs a block. The names are interoperability facts. */

#include "../code/compat/jka_icarus_ids.h"
#include "../code/compat/jka_icarus_values.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace icarus = fntech3::jka::icarus;
using icarus::BlockId;
using icarus::Number;
using icarus::Token;

/* The three enumerations run on from one another with no gap and no repeat, so
 * one number space covers a member, a type and a block alike. */
static void CheckTheNumbersAreOneSpace()
{
	int expected = 0;
#define FNTECH3_JKA_ICARUS_TOKEN( name, number ) CHECK( ( number ) == expected++ );
#define FNTECH3_JKA_ICARUS_BLOCK( name, number ) CHECK( ( number ) == expected++ );
#define FNTECH3_JKA_ICARUS_TYPE( name, number ) CHECK( ( number ) == expected++ );
#include "../code/compat/jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
	CHECK( expected == icarus::TokenCount + icarus::BlockCount + icarus::TypeCount );
	CHECK( expected == 71 );

	/* A number this table does not hold has no name, rather than a wrong one. */
	CHECK( icarus::TokenName( 4 ) && !std::strcmp( icarus::TokenName( 4 ), "TK_STRING" ) );
	CHECK( icarus::BlockName( 26 ) && !std::strcmp( icarus::BlockName( 26 ), "ID_SET" ) );
	CHECK( icarus::TokenName( 26 ) == nullptr );
	CHECK( icarus::BlockName( 4 ) == nullptr );
	CHECK( icarus::TokenName( -1 ) == nullptr && icarus::BlockName( 51 ) == nullptr );
	CHECK( icarus::TypeName( 51 ) && !std::strcmp( icarus::TypeName( 51 ), "TYPE_WAIT_COMPLETE" ) );
	CHECK( icarus::TypeName( 50 ) == nullptr && icarus::TypeName( 71 ) == nullptr );
}

/* The types a block chooses between, each found in the corpus as a position
 * where only a handful of values ever occur -- before any of them had a name. */
static void CheckTheTypesAgreeWithTheScripts()
{
	using icarus::Type;
	/* An affect reaches its entity one of two ways, and both occur. */
	static_assert( Number( Type::TYPE_INSERT ) == 55 && Number( Type::TYPE_FLUSH ) == 56 );
	/* What a set or a move is addressing. */
	static_assert( Number( Type::TYPE_ANGLES ) == 53 && Number( Type::TYPE_ORIGIN ) == 54 );
	/* The camera types, nine of which occur and none of anything else. */
	static_assert( Number( Type::TYPE_PAN ) == 57 && Number( Type::TYPE_ZOOM ) == 58 );
	static_assert( Number( Type::TYPE_MOVE ) == 59 && Number( Type::TYPE_FADE ) == 60 );
	static_assert( Number( Type::TYPE_PATH ) == 61 && Number( Type::TYPE_ENABLE ) == 62 );
	static_assert( Number( Type::TYPE_DISABLE ) == 63 && Number( Type::TYPE_SHAKE ) == 64 );
	static_assert( Number( Type::TYPE_FOLLOW ) == 68 );
	/* The three unused camera types sit among them, named but never seen. */
	static_assert( Number( Type::TYPE_ROLL ) == 65 && Number( Type::TYPE_TRACK ) == 66 );
	static_assert( Number( Type::TYPE_DISTANCE ) == 67 );
	/* A declared variable is one of the three kinds a value can be, and the
	 * type numbers for those are the member numbers for them. */
	CHECK( icarus::IsTypeNumber( Number( icarus::Token::TK_FLOAT ) ) );
	static_assert( Number( Type::TYPE_VARIABLE ) == 69 );
}

/* What the value reader groups, under the names Raven gave those numbers. */
static void CheckTheValueReaderUsesTheSameNumbers()
{
	static_assert( icarus::MemberText == Number( Token::TK_STRING ) );
	static_assert( icarus::MemberNumber == Number( Token::TK_FLOAT ) );
	static_assert( icarus::MemberName == Number( Token::TK_IDENTIFIER ) );
	static_assert( icarus::MemberVector == Number( Token::TK_VECTOR ) );
	static_assert( icarus::MemberTyped == Number( BlockId::ID_GET ) );
	static_assert( icarus::MemberRange == Number( BlockId::ID_RANDOM ) );
	static_assert( icarus::MemberTagged == Number( BlockId::ID_TAG ) );

	/* The four comparisons are contiguous, so the reader's range is exactly
	 * them and nothing else. */
	static_assert( icarus::MemberCompareFirst == Number( Token::TK_GREATER_THAN ) );
	static_assert( icarus::MemberCompareLast == Number( Token::TK_NOT ) );
	static_assert( Number( Token::TK_LESS_THAN ) == icarus::MemberCompareFirst + 1 );
	static_assert( Number( Token::TK_EQUALS ) == icarus::MemberCompareFirst + 2 );
	static_assert( icarus::MemberCompareLast - icarus::MemberCompareFirst == 3 );

	/* A type number names a kind of value with that kind's own number. */
	CHECK( icarus::IsTypeNumber( Number( Token::TK_STRING ) ) );
	CHECK( icarus::IsTypeNumber( Number( Token::TK_FLOAT ) ) );
	CHECK( icarus::IsTypeNumber( Number( Token::TK_VECTOR ) ) );
	CHECK( !icarus::IsTypeNumber( Number( Token::TK_IDENTIFIER ) ) );
	CHECK( !icarus::IsTypeNumber( Number( Token::TK_INT ) ) );
	CHECK( !icarus::IsTypeNumber( 0 ) );
}

/* The corpus was read before these names were. Each line below is something a
 * retail installation's scripts showed on their own, written against the name
 * the number turned out to carry. */
static void CheckTheScriptsAgreeWithTheNames()
{
	/* The block with no members, which closes the five that open a body, is
	 * the one Raven calls the end of a block. */
	static_assert( Number( BlockId::ID_BLOCK_END ) == 25 );

	/* Those five openers are its five constructs that take a body. */
	static_assert( Number( BlockId::ID_AFFECT ) == 19 );
	static_assert( Number( BlockId::ID_LOOP ) == 27 );
	static_assert( Number( BlockId::ID_IF ) == 38 );
	static_assert( Number( BlockId::ID_ELSE ) == 39 );
	static_assert( Number( BlockId::ID_TASK ) == 41 );

	/* The only block carrying a comparison is the conditional. */
	static_assert( Number( BlockId::ID_IF ) == 38 );

	/* The block whose second string is always an installed sound, and the one
	 * whose first string is always `PLAY_ROFF`. */
	static_assert( Number( BlockId::ID_SOUND ) == 20 );
	static_assert( Number( BlockId::ID_PLAY ) == 48 );

	/* The block whose string always names an installed script. */
	static_assert( Number( BlockId::ID_RUN ) == 32 );

	/* The block whose leading string almost always begins `SET_`. */
	static_assert( Number( BlockId::ID_SET ) == 26 );

	/* The two whose strings name map entities, and the one that declares the
	 * names the two others refer to within a single script. */
	static_assert( Number( BlockId::ID_USE ) == 30 );
	static_assert( Number( BlockId::ID_REMOVE ) == 34 );
	static_assert( Number( BlockId::ID_TASK ) == 41 );
	static_assert( Number( BlockId::ID_DO ) == 42 );
	static_assert( Number( BlockId::ID_WAIT ) == 23 );

	/* The pair that names each other across scripts of one level. */
	static_assert( Number( BlockId::ID_SIGNAL ) == 46 );
	static_assert( Number( BlockId::ID_WAITSIGNAL ) == 47 );

	/* The variadic one, and the one whose single value is -1 as often as it is
	 * a count. */
	static_assert( Number( BlockId::ID_CAMERA ) == 35 );
	static_assert( Number( BlockId::ID_LOOP ) == 27 );

	/* Five numbers this table names never appear in a retail script: a block
	 * start that the container does not need, a loop end that the shared block
	 * end serves, a remark the compiler drops, and two the scripts never use.
	 * They are named so a script that does use one is recognized, not refused
	 * as nameless. */
	CHECK( icarus::BlockName( Number( BlockId::ID_BLOCK_START ) ) != nullptr );
	CHECK( icarus::BlockName( Number( BlockId::ID_LOOPEND ) ) != nullptr );
	CHECK( icarus::BlockName( Number( BlockId::ID_REM ) ) != nullptr );
	CHECK( icarus::BlockName( Number( BlockId::ID_DOWAIT ) ) != nullptr );
	CHECK( icarus::BlockName( Number( BlockId::ID_EOF ) ) != nullptr );
}

/* Raven's own reader compares the version for equality, so fnTech3's does too,
 * and the number it compares is the one a retail script carries. */
static void CheckTheVersion()
{
	const float version = 1.57f;
	std::uint32_t bits = 0;
	std::memcpy( &bits, &version, sizeof( bits ) );
	CHECK( bits == icarus::VersionBits );
	static_assert( icarus::Ident == 0x00494249u );   /* 'I', 'B', 'I', 0 */
}

int main()
{
	CheckTheNumbersAreOneSpace();
	CheckTheValueReaderUsesTheSameNumbers();
	CheckTheScriptsAgreeWithTheNames();
	CheckTheTypesAgreeWithTheScripts();
	CheckTheVersion();
	std::cout << "Jedi Academy ICARUS number tests passed: "
		<< icarus::TokenCount << " tokens and " << icarus::BlockCount << " block ids\n";
	return 0;
}
