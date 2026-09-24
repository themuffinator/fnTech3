/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Jedi Academy's compiled ICARUS scripts.
 * Every script here is written by this test; no retail file is used, and
 * nothing here interprets a block. The shapes come from the container
 * established over a legitimate installation's 1,018 scripts, recorded in
 * docs/fntech3/2026-09-21-jka-icarus.md. */

#include "../code/compat/jka_icarus_format.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace icarus = fntech3::jka::icarus;
using icarus::Error;
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

/* One block, then its members, exactly as the container carries them. */
static void AppendBlock( Bytes &file, std::int32_t id, int members, std::uint8_t flags = 0 )
{
	AppendInt( file, id );
	AppendInt( file, members );
	file.push_back( flags );
}

static void AppendMember( Bytes &file, std::int32_t id, const std::string &payload )
{
	AppendInt( file, id );
	AppendInt( file, std::int32_t( payload.size() ) );
	file.insert( file.end(), payload.begin(), payload.end() );
}

static void AppendFloatMember( Bytes &file, std::int32_t id, float value )
{
	std::uint32_t word;
	std::memcpy( &word, &value, sizeof( word ) );
	AppendInt( file, id );
	AppendInt( file, 4 );
	for ( int i = 0; i < 4; ++i ) file.push_back( std::uint8_t( word >> ( 8 * i ) ) );
}

/* A header and nothing else is a whole script: ten installed ones are that. */
static void CheckEmptyScript()
{
	const Bytes file = Header();
	icarus::Script script;
	CHECK( icarus::Parse( file.data(), file.size(), script ) == Error::None );
	CHECK( script.blocks == 0 && script.members == 0 && script.memberBytes == 0 );
	CHECK( script.versionBits == icarus::VersionBits );
}

static void CheckWalk()
{
	Bytes file = Header();
	AppendBlock( file, 26, 2 );
	AppendMember( file, 4, std::string( "SET_CINEMATIC_SKIPSCRIPT" ) + '\0' );
	AppendFloatMember( file, 6, 62.0f );
	AppendBlock( file, 48, 0 );                 /* a block with no members */
	AppendBlock( file, 19, 1 );
	AppendMember( file, 7, std::string( "CHAN_VOICE" ) + '\0' );

	icarus::Script script;
	std::vector<std::int32_t> blockIds, memberIds;
	std::vector<std::string> text;
	int emptyBlocks = 0;
	const Error error = icarus::Walk( file.data(), file.size(), script,
		[&]( const icarus::Block &block, const icarus::Member *member ) {
			if ( blockIds.empty() || blockIds.back() != block.id ) blockIds.push_back( block.id );
			if ( !member ) { ++emptyBlocks; return; }
			memberIds.push_back( member->id );
			const char *value = nullptr;
			if ( icarus::TerminatedText( file.data(), *member, value ) ) text.push_back( value );
		} );
	CHECK( error == Error::None );
	CHECK( script.blocks == 3 && script.members == 3 );
	CHECK( script.memberBytes == 25 + 4 + 11 );
	CHECK( ( memberIds == std::vector<std::int32_t>{ 4, 6, 7 } ) );
	/* Every block is offered, the memberless one included and in its place. */
	CHECK( ( blockIds == std::vector<std::int32_t>{ 26, 48, 19 } ) );
	CHECK( emptyBlocks == 1 );
	CHECK( text.size() == 2 && text[0] == "SET_CINEMATIC_SKIPSCRIPT" && text[1] == "CHAN_VOICE" );

	/* A member's bytes are named, never copied: each range lies in the file. */
	CHECK( icarus::Walk( file.data(), file.size(), script,
		[&]( const icarus::Block &, const icarus::Member *member ) {
			if ( member ) CHECK( member->offset + member->bytes <= file.size() );
		} ) == Error::None );
}

/* Text members are only text when they end in one terminator and hold none
 * before it, so a fixed-width value is never handed out as a string. */
static void CheckText()
{
	Bytes file = Header();
	AppendBlock( file, 20, 3 );
	AppendMember( file, 4, std::string( "two\0halves", 10 ) );  /* an inner NUL */
	AppendMember( file, 4, std::string( "unterminated" ) );
	AppendFloatMember( file, 6, 1.0f );

	icarus::Script script;
	int strings = 0, members = 0;
	CHECK( icarus::Walk( file.data(), file.size(), script,
		[&]( const icarus::Block &, const icarus::Member *member ) {
			if ( !member ) return;
			++members;
			const char *value = nullptr;
			if ( icarus::TerminatedText( file.data(), *member, value ) ) ++strings;
		} ) == Error::None );
	CHECK( members == 3 && strings == 0 );
}

static void CheckRejections()
{
	icarus::Script script;
	const Bytes good = Header();

	CHECK( icarus::Parse( nullptr, 16, script ) == Error::Size );
	CHECK( icarus::Parse( good.data(), 7, script ) == Error::Size );

	Bytes wrongIdent = good;
	wrongIdent[1] = 'X';
	CHECK( icarus::Parse( wrongIdent.data(), wrongIdent.size(), script ) == Error::Ident );

	Bytes wrongVersion = good;
	wrongVersion[7] = std::uint8_t( wrongVersion[7] + 1 );
	CHECK( icarus::Parse( wrongVersion.data(), wrongVersion.size(), script ) == Error::Version );

	/* A block header that does not fit. */
	Bytes shortBlock = good;
	AppendInt( shortBlock, 26 );
	AppendInt( shortBlock, 0 );
	CHECK( icarus::Parse( shortBlock.data(), shortBlock.size(), script ) == Error::Extent );

	/* A member count no remaining byte could serve. */
	Bytes lyingCount = good;
	AppendBlock( lyingCount, 26, 2 );
	AppendMember( lyingCount, 4, std::string( "x" ) + '\0' );
	CHECK( icarus::Parse( lyingCount.data(), lyingCount.size(), script ) == Error::Counts );

	Bytes negativeCount = good;
	AppendBlock( negativeCount, 26, -1 );
	CHECK( icarus::Parse( negativeCount.data(), negativeCount.size(), script ) == Error::Counts );

	/* A member whose size reaches past the file, and one that claims a
	 * negative size, which unchecked arithmetic would turn into a huge one. */
	Bytes longMember = good;
	AppendBlock( longMember, 26, 1 );
	AppendInt( longMember, 4 );
	AppendInt( longMember, 64 );
	longMember.push_back( 'x' );
	CHECK( icarus::Parse( longMember.data(), longMember.size(), script ) == Error::Extent );

	Bytes negativeMember = good;
	AppendBlock( negativeMember, 26, 1 );
	AppendInt( negativeMember, 4 );
	AppendInt( negativeMember, -8 );
	CHECK( icarus::Parse( negativeMember.data(), negativeMember.size(), script ) == Error::Counts );

	/* Bytes after the last member belong to no block. */
	Bytes trailing = good;
	AppendBlock( trailing, 26, 1 );
	AppendMember( trailing, 4, std::string( "x" ) + '\0' );
	trailing.push_back( 0 );
	CHECK( icarus::Parse( trailing.data(), trailing.size(), script ) == Error::Extent );

	for ( Error error : { Error::None, Error::Size, Error::Ident, Error::Version,
		Error::Counts, Error::Extent } ) {
		const char *name = icarus::ErrorName( error );
		CHECK( name && name[0] && std::strcmp( name, "unknown" ) );
	}
	CHECK( !std::strcmp( icarus::ErrorName( (Error)42 ), "unknown" ) );
}

int main()
{
	CheckEmptyScript();
	CheckWalk();
	CheckText();
	CheckRejections();
	std::cout << "Jedi Academy ICARUS script container and malformed-script tests passed\n";
	return 0;
}
