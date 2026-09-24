/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for the value layer over Jedi Academy's
 * ICARUS container. Every script here is written by this test; no retail file
 * is used, and nothing here interprets a block. The shapes are the ones
 * recorded in docs/fntech3/2026-09-21-jka-icarus.md. */

#include "../code/compat/jka_icarus_values.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace icarus = fntech3::jka::icarus;
using icarus::ValueError;
using icarus::ValueKind;
using Bytes = std::vector<std::uint8_t>;

static void AppendInt( Bytes &file, std::int32_t value )
{
	for ( int i = 0; i < 4; ++i ) file.push_back( std::uint8_t( std::uint32_t( value ) >> ( 8 * i ) ) );
}

static Bytes Header()
{
	Bytes file{ 'I', 'B', 'I', 0 };
	AppendInt( file, std::int32_t( icarus::VersionBits ) );
	return file;
}

static void AppendBlock( Bytes &file, std::int32_t id, int members, std::uint8_t flags = 0 )
{
	AppendInt( file, id );
	AppendInt( file, members );
	file.push_back( flags );
}

static void AppendText( Bytes &file, std::int32_t id, const std::string &value )
{
	AppendInt( file, id );
	AppendInt( file, std::int32_t( value.size() + 1 ) );
	file.insert( file.end(), value.begin(), value.end() );
	file.push_back( 0 );
}

static void AppendNumber( Bytes &file, float value, std::int32_t id = icarus::MemberNumber )
{
	std::uint32_t word;
	std::memcpy( &word, &value, sizeof( word ) );
	AppendInt( file, id );
	AppendInt( file, 4 );
	for ( int i = 0; i < 4; ++i ) file.push_back( std::uint8_t( word >> ( 8 * i ) ) );
}

/* A member that introduces others carries its own number as its payload, the
 * way Raven's compiler wrote it; nothing reads that payload. */
static void AppendMarker( Bytes &file, std::int32_t id )
{
	AppendNumber( file, float( id ), id );
}

struct Arena {
	icarus::Statement statements[64];
	icarus::Value values[128];
	icarus::ScriptValues view;

	Arena()
	{
		view.statements = statements;
		view.statementCapacity = 64;
		view.values = values;
		view.valueCapacity = 128;
	}
	ValueError Read( const Bytes &file ) { return icarus::ReadValues( file.data(), file.size(), view ); }
};

static void CheckEveryKind()
{
	Bytes file = Header();
	AppendBlock( file, 26, 2 );
	AppendText( file, icarus::MemberText, "SET_ANIM_BOTH" );
	AppendText( file, icarus::MemberText, "BOTH_STAND1" );

	AppendBlock( file, 20, 2 );
	AppendText( file, icarus::MemberName, "CHAN_VOICE" );
	AppendText( file, icarus::MemberText, "sound/chars/x/y.mp3" );

	/* A block header counts members, not value groups: this one is a number,
	 * a vector's marker and its three floats, and a trailing number. */
	AppendBlock( file, 35, 6 );
	AppendNumber( file, 57.0f );
	AppendMarker( file, icarus::MemberVector );
	AppendNumber( file, 1.5f );
	AppendNumber( file, -2.0f );
	AppendNumber( file, 3.25f );
	AppendNumber( file, 2000.0f );

	AppendBlock( file, 23, 3 );
	AppendMarker( file, icarus::MemberRange );
	AppendNumber( file, 500.0f );
	AppendNumber( file, 1500.0f );

	AppendBlock( file, 21, 4 );
	AppendMarker( file, icarus::MemberTagged );
	AppendText( file, icarus::MemberText, "run_here" );
	AppendNumber( file, 49.0f );
	AppendNumber( file, 4.0f );

	AppendBlock( file, 38, 5 );
	AppendMarker( file, icarus::MemberTyped );
	AppendNumber( file, 6.0f );
	AppendText( file, icarus::MemberText, "fUsedBeam" );
	AppendMarker( file, 17 );
	AppendNumber( file, 1.0f );

	Arena arena;
	CHECK( arena.Read( file ) == ValueError::None );
	const auto &view = arena.view;
	CHECK( view.statementCount == 6 );

	/* Every statement's values are contiguous and nothing else sits between
	 * them, so a caller can walk one statement without knowing the rest. */
	int expected = 0;
	for ( int i = 0; i < view.statementCount; ++i ) {
		CHECK( view.statements[i].firstValue == expected );
		expected += view.statements[i].values;
	}
	CHECK( expected == view.valueCount );

	const icarus::Value *set = view.values + view.statements[0].firstValue;
	CHECK( view.statements[0].values == 2 );
	CHECK( set[0].kind == ValueKind::Text && !std::strcmp( set[0].text, "SET_ANIM_BOTH" ) );
	CHECK( set[1].kind == ValueKind::Text && !std::strcmp( set[1].text, "BOTH_STAND1" ) );

	const icarus::Value *sound = view.values + view.statements[1].firstValue;
	CHECK( view.statements[1].values == 2 );
	CHECK( sound[0].kind == ValueKind::Name && !std::strcmp( sound[0].text, "CHAN_VOICE" ) );
	CHECK( sound[1].kind == ValueKind::Text );

	const icarus::Value *move = view.values + view.statements[2].firstValue;
	CHECK( view.statements[2].values == 3 );
	CHECK( move[0].kind == ValueKind::Number && move[0].numbers[0] == 57.0f );
	CHECK( move[1].kind == ValueKind::Vector );
	CHECK( move[1].numbers[0] == 1.5f && move[1].numbers[1] == -2.0f && move[1].numbers[2] == 3.25f );
	CHECK( move[1].text == nullptr );
	CHECK( move[2].kind == ValueKind::Number && move[2].numbers[0] == 2000.0f );

	const icarus::Value *range = view.values + view.statements[3].firstValue;
	CHECK( view.statements[3].values == 1 );
	CHECK( range[0].kind == ValueKind::Range );
	CHECK( range[0].numbers[0] == 500.0f && range[0].numbers[1] == 1500.0f );

	const icarus::Value *tagged = view.values + view.statements[4].firstValue;
	CHECK( view.statements[4].values == 2 );
	CHECK( tagged[0].kind == ValueKind::Tagged );
	CHECK( !std::strcmp( tagged[0].text, "run_here" ) );
	CHECK( tagged[0].type == 0 && tagged[0].numbers[0] == 49.0f );
	CHECK( tagged[1].kind == ValueKind::Number && tagged[1].numbers[0] == 4.0f );

	const icarus::Statement &conditional = view.statements[5];
	CHECK( conditional.values == 2 && conditional.compare == 17 );
	CHECK( conditional.compareValue == conditional.firstValue + 1 );
	const icarus::Value *left = view.values + conditional.firstValue;
	CHECK( left[0].kind == ValueKind::Typed && left[0].type == icarus::MemberNumber );
	CHECK( !std::strcmp( left[0].text, "fUsedBeam" ) );
	CHECK( left[1].kind == ValueKind::Number && left[1].numbers[0] == 1.0f );
}

/* A tag may take its name through a type rather than plainly. That nests one
 * deep and never more, so it stays a single value. */
static void CheckTaggedThroughAType()
{
	Bytes file = Header();
	AppendBlock( file, 21, 5 );
	AppendMarker( file, icarus::MemberTagged );
	AppendMarker( file, icarus::MemberTyped );
	AppendNumber( file, 4.0f );
	AppendText( file, icarus::MemberText, "npc_tavion" );
	AppendNumber( file, 49.0f );

	Arena arena;
	CHECK( arena.Read( file ) == ValueError::None );
	CHECK( arena.view.statementCount == 1 && arena.view.valueCount == 1 );
	const icarus::Value &value = arena.view.values[0];
	CHECK( value.kind == ValueKind::Tagged && value.type == icarus::MemberText );
	CHECK( !std::strcmp( value.text, "npc_tavion" ) && value.numbers[0] == 49.0f );
}

static void CheckEmptyBlocks()
{
	Bytes file = Header();
	AppendBlock( file, 25, 0 );
	AppendBlock( file, 39, 0, 0 );
	Arena arena;
	CHECK( arena.Read( file ) == ValueError::None );
	CHECK( arena.view.statementCount == 2 && arena.view.valueCount == 0 );
	CHECK( arena.view.statements[0].block == 25 && arena.view.statements[0].values == 0 );
	CHECK( arena.view.statements[1].block == 39 && arena.view.statements[1].compare == 0 );
}

static void CheckRejections()
{
	Arena arena;

	/* A member number no rule covers is refused, not skipped. */
	Bytes unknown = Header();
	AppendBlock( unknown, 26, 1 );
	AppendNumber( unknown, 1.0f, 99 );
	CHECK( arena.Read( unknown ) == ValueError::Member );

	/* A vector whose three floats are not all there. */
	Bytes shortVector = Header();
	AppendBlock( shortVector, 35, 3 );
	AppendMarker( shortVector, icarus::MemberVector );
	AppendNumber( shortVector, 1.0f );
	AppendNumber( shortVector, 2.0f );
	CHECK( arena.Read( shortVector ) == ValueError::Group );

	/* A vector whose parts are the wrong kind of member. */
	Bytes wrongVector = Header();
	AppendBlock( wrongVector, 35, 4 );
	AppendMarker( wrongVector, icarus::MemberVector );
	AppendNumber( wrongVector, 1.0f );
	AppendText( wrongVector, icarus::MemberText, "up" );
	AppendNumber( wrongVector, 3.0f );
	CHECK( arena.Read( wrongVector ) == ValueError::Group );

	/* A float member that is not four bytes. */
	Bytes shortFloat = Header();
	AppendBlock( shortFloat, 23, 1 );
	AppendInt( shortFloat, icarus::MemberNumber );
	AppendInt( shortFloat, 2 );
	shortFloat.push_back( 0 );
	shortFloat.push_back( 0 );
	CHECK( arena.Read( shortFloat ) == ValueError::Number );

	/* A type number that names no kind of value. */
	Bytes wrongType = Header();
	AppendBlock( wrongType, 38, 3 );
	AppendMarker( wrongType, icarus::MemberTyped );
	AppendNumber( wrongType, 7.0f );
	AppendText( wrongType, icarus::MemberText, "name" );
	CHECK( arena.Read( wrongType ) == ValueError::Type );

	/* A type number that is not whole. */
	Bytes fractionalType = Header();
	AppendBlock( fractionalType, 38, 3 );
	AppendMarker( fractionalType, icarus::MemberTyped );
	AppendNumber( fractionalType, 4.5f );
	AppendText( fractionalType, icarus::MemberText, "name" );
	CHECK( arena.Read( fractionalType ) == ValueError::Type );

	/* A comparison with something after the value it takes. */
	Bytes trailingAfterCompare = Header();
	AppendBlock( trailingAfterCompare, 38, 4 );
	AppendText( trailingAfterCompare, icarus::MemberText, "left" );
	AppendMarker( trailingAfterCompare, 17 );
	AppendNumber( trailingAfterCompare, 1.0f );
	AppendNumber( trailingAfterCompare, 2.0f );
	CHECK( arena.Read( trailingAfterCompare ) == ValueError::Compare );

	/* Two comparisons in a row, before either has its value. */
	Bytes twoCompares = Header();
	AppendBlock( twoCompares, 38, 3 );
	AppendMarker( twoCompares, 17 );
	AppendMarker( twoCompares, 18 );
	AppendNumber( twoCompares, 1.0f );
	CHECK( arena.Read( twoCompares ) == ValueError::Compare );

	/* A comparison with no value at all. */
	Bytes danglingCompare = Header();
	AppendBlock( danglingCompare, 38, 2 );
	AppendText( danglingCompare, icarus::MemberText, "left" );
	AppendMarker( danglingCompare, 17 );
	CHECK( arena.Read( danglingCompare ) == ValueError::Compare );

	/* A tag whose name is neither text nor a type. */
	Bytes wrongTag = Header();
	AppendBlock( wrongTag, 21, 3 );
	AppendMarker( wrongTag, icarus::MemberTagged );
	AppendNumber( wrongTag, 1.0f );
	AppendNumber( wrongTag, 49.0f );
	CHECK( arena.Read( wrongTag ) == ValueError::Group );

	/* A malformed container is reported as one, with its own reason kept. */
	Bytes broken = Header();
	broken[1] = 'X';
	CHECK( arena.Read( broken ) == ValueError::Container );
	CHECK( arena.view.container == icarus::Error::Ident );

	/* Arrays the script does not fit. */
	Bytes many = Header();
	for ( int i = 0; i < 3; ++i ) {
		AppendBlock( many, 23, 1 );
		AppendNumber( many, float( i ) );
	}
	icarus::Statement statements[2];
	icarus::Value values[8];
	icarus::ScriptValues small;
	small.statements = statements;
	small.statementCapacity = 2;
	small.values = values;
	small.valueCapacity = 8;
	CHECK( icarus::ReadValues( many.data(), many.size(), small ) == ValueError::Capacity );

	icarus::ScriptValues noValues = small;
	noValues.statementCapacity = 8;
	noValues.valueCapacity = 1;
	CHECK( icarus::ReadValues( many.data(), many.size(), noValues ) == ValueError::Capacity );

	for ( ValueError error : { ValueError::None, ValueError::Container, ValueError::Member,
		ValueError::Group, ValueError::Text, ValueError::Number, ValueError::Type,
		ValueError::Compare, ValueError::Capacity } ) {
		const char *name = icarus::ValueErrorName( error );
		CHECK( name && name[0] && std::strcmp( name, "unknown" ) );
	}
	CHECK( !std::strcmp( icarus::ValueErrorName( (ValueError)42 ), "unknown" ) );
}

int main()
{
	CheckEveryKind();
	CheckTaggedThroughAType();
	CheckEmptyBlocks();
	CheckRejections();
	std::cout << "Jedi Academy ICARUS value grouping and malformed-value tests passed\n";
	return 0;
}
