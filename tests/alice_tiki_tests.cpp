/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Independently authored fixtures for Alice's `.tik` model definitions. Every
 * definition here is written by this test; no retail asset is used. */

#include "../code/compat/alice_tiki_format.hpp"

#include <cstring>
#include <iostream>
#include <string>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::tiki;

static bool Is( const Token &token, const char *text )
{
	return token.length == std::strlen( text )
		&& !std::memcmp( token.text, text, token.length );
}

static Error Parse( const std::string &text, Definition &out )
{
	return ParseDefinition( text.data(), text.size(), out );
}

/* A definition with every wrinkle the installed corpus shows: a preprocessor
 * preamble, a macro, comments of both kinds, a quoted value, surface lines
 * with and without a shader, and animations with and without a command block. */
static const char *kFixture =
	"#define HEAD_TAG 0\n"
	"#define TORSO_TAG 1\n"
	"TIKI\n"
	"$define workdir /sound/character/fixture\n"
	"setup\n"
	"{\n"
	"\tscale 1\n"
	"\tpath models/characters/fixture\n"
	"\tsurface body shader skin01.tga\n"
	"\tsurface body flags nodraw\n"           /* not a shader line */
	"\tsurface head shader \"skin 02.tga\"\n" /* a quoted image name */
	"}\n"
	"// a line comment\n"
	"init\n"
	"{\n"
	"\tserver { classname object }\n"
	"\tclient { }\n"
	"}\n"
	"/* a block comment */\n"
	"animations\n"
	"{\n"
	"\tidle idle.ska\n"
	"\twalk walk.ska\n"
	"\t{\n"
	"\t\tclient { 2 sound $workdir$/step.wav 0 .6 128 }\n"
	"\t}\n"
	"\tdeath death.tan\n"
	"}\n";

static void CheckParse()
{
	Definition def{};
	CHECK( Parse( kFixture, def ) == Error::None );
	CHECK( def.hasSetup && def.hasInit && def.hasAnimations );
	CHECK( !def.unterminated );

	/* The preprocessor preamble is skipped a line at a time, so its tokens
	 * never reach the grammar. */
	CHECK( def.hasPath && Is( def.path, "models/characters/fixture" ) );

	/* Only the `surface <name> shader <image>` form names an image. */
	CHECK( def.surfaces == 2 );
	CHECK( Is( def.surface[0].name, "body" ) && Is( def.surface[0].shader, "skin01.tga" ) );
	CHECK( Is( def.surface[1].name, "head" ) && Is( def.surface[1].shader, "skin 02.tga" ) );

	CHECK( def.animations == 3 );
	CHECK( Is( def.animation[0].name, "idle" ) && Is( def.animation[0].file, "idle.ska" ) );
	CHECK( !def.animation[0].hasCommands );
	CHECK( Is( def.animation[1].name, "walk" ) && Is( def.animation[1].file, "walk.ska" ) );
	CHECK( def.animation[1].hasCommands );
	CHECK( Is( def.animation[2].name, "death" ) && Is( def.animation[2].file, "death.tan" ) );
	CHECK( !def.animation[2].hasCommands );

	/* The macro is declared at file scope, above the first block, which is
	 * where the installed definitions put it. */
	CHECK( def.macros == 1 );
	CHECK( Is( def.macro[0].name, "workdir" ) );
	Token expanded{};
	const std::string reference = "$workdir$";
	CHECK( ExpandMacro( def, Token{ reference.data(), reference.size() }, expanded ) );
	CHECK( Is( expanded, "/sound/character/fixture" ) );
	/* A reference the definition never declared does not expand, and neither
	 * does a token that is not a reference at all. */
	const std::string unknown = "$missing$";
	CHECK( !ExpandMacro( def, Token{ unknown.data(), unknown.size() }, expanded ) );
	const std::string plain = "walk.ska";
	CHECK( !ExpandMacro( def, Token{ plain.data(), plain.size() }, expanded ) );
	const std::string unclosed = "$workdir";
	CHECK( !ExpandMacro( def, Token{ unclosed.data(), unclosed.size() }, expanded ) );
}

static void CheckShippedShapes()
{
	/* One installed definition ends inside its animations block. The retail
	 * engine loads it, so it is reported and not refused. */
	Definition def{};
	const std::string cut =
		"TIKI\nsetup { path models/x }\nanimations\n{\n\tidle idle.ska\n";
	CHECK( Parse( cut, def ) == Error::None );
	CHECK( def.unterminated );
	CHECK( def.animations == 1 && Is( def.animation[0].file, "idle.ska" ) );

	/* A definition with no animations block at all still reads. */
	const std::string bare = "TIKI\nsetup { path models/y scale 1 }\n";
	CHECK( Parse( bare, def ) == Error::None );
	CHECK( def.hasSetup && !def.hasAnimations && !def.unterminated );
	CHECK( def.hasPath && Is( def.path, "models/y" ) );

	/* The magic alone is a valid, empty definition. */
	const std::string empty = "TIKI\n";
	CHECK( Parse( empty, def ) == Error::None );
	CHECK( !def.hasSetup && !def.hasAnimations && !def.hasPath );
}

static void CheckRejections()
{
	Definition def{};

	/* Something other than a preprocessor line before the magic is not a
	 * definition, and neither is a file without the magic at all. */
	for ( const char *text : { "", "setup { }", "NOTTIKI\nsetup { }", "\n\n" } ) {
		const Error error = Parse( text, def );
		CHECK( error == Error::Size || error == Error::Magic );
	}
	CHECK( ParseDefinition( nullptr, 16, def ) == Error::Size );
	CHECK( ParseDefinition( "TIKI", 0, def ) == Error::Size );

	/* A closing brace with nothing open is a malformed file, unlike a file
	 * that merely ends early. */
	CHECK( Parse( "TIKI\nsetup { }\n}\n", def ) == Error::Depth );
	CHECK( Parse( "TIKI\n}\n", def ) == Error::Depth );

	/* A statement whose value is missing runs off the end of the file. */
	CHECK( Parse( "TIKI\n$define name", def ) == Error::Truncated );
	CHECK( Parse( "TIKI\nsetup { path", def ) == Error::Truncated );
	CHECK( Parse( "TIKI\nsetup { surface body", def ) == Error::Truncated );
	CHECK( Parse( "TIKI\nsetup { surface body shader", def ) == Error::Truncated );

	/* More records than a definition may hold is refused rather than
	 * truncated silently. */
	{
		std::string many = "TIKI\nsetup\n{\n";
		for ( std::size_t i = 0; i <= MaxSurfaces; ++i ) {
			many += "surface s shader i.tga\n";
		}
		many += "}\n";
		CHECK( Parse( many, def ) == Error::TooMany );
	}
	{
		std::string many = "TIKI\nanimations\n{\n";
		for ( std::size_t i = 0; i <= MaxAnimations; ++i ) {
			many += "a a.ska\n";
		}
		many += "}\n";
		CHECK( Parse( many, def ) == Error::TooMany );
	}
	{
		std::string many = "TIKI\n";
		for ( std::size_t i = 0; i <= MaxMacros; ++i ) {
			many += "$define m v\n";
		}
		CHECK( Parse( many, def ) == Error::TooMany );
	}

	for ( Error error : { Error::None, Error::Size, Error::Magic, Error::Depth,
		Error::Truncated, Error::TooMany } ) {
		CHECK( ErrorName( error ) && ErrorName( error )[0] );
		CHECK( std::strcmp( ErrorName( error ), "unknown" ) );
	}
}

int main()
{
	CheckParse();
	CheckShippedShapes();
	CheckRejections();
	std::cout << "Alice TIKI definition reader tests passed\n";
	return 0;
}
