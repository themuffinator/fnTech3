/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's `.urc` menu resource reader (code/compat/alice_urc.hpp). The
 * fixtures are written here; the twelve menus a retail installation ships are
 * also read, read-only, when one is present. */

#include "../code/compat/alice_urc.hpp"
#include "alice_pak_reader.hpp"

#include <iostream>
#include <map>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::urc;

namespace {

Layout ParseText( const std::string &text, const IncludeReader &include = IncludeReader() ) {
	return Parse( text.data(), text.size(), include );
}

void TestMenu() {
	const std::string text =
		"menu \"main\"\t\t640 480 NONE 0.5\r\n"
		"//fadeoverlay\t0.5\r\n"
		"bgcolor\t\t\t0 0 0 1\r\n"
		"fullscreen 1\r\n"
		"showcommand\t\t\"widgetcommand backtogame showifalive\\n\"\r\n"
		"resource\r\n"
		"Label\r\n"
		"{\r\n"
		"\tname\t\t\t\"Default\"\r\n"
		"\trect\t\t\t0 0 256 256\r\n"
		"\tstaticshader\t0 ui/main/mainA\r\n"
		"}\r\n"
		"resource\r\n"
		"Button\r\n"
		"{\r\n"
		"\tname \"newgame\"\r\n"
		"\tstuffcommand\t\"seta skill 0; seta nextmap gvillage; cinematic opening.roq; togglemenu newgame;\"\r\n"
		"\tkeycommand y quit ; trailing comment\r\n"
		"\tfontspacing 1,2\r\n"
		"}\r\n"
		"end.\r\n"
		"menu \"ignored\" 1 1 NONE\r\n";
	const Layout layout = ParseText( text );
	CHECK( layout.ended );
	CHECK( layout.menus.size() == 1 );
	const Menu &menu = layout.menus[0];
	MenuHeader header;
	CHECK( ReadMenuHeader( menu.header, header ) );
	CHECK( header.name == "main" && header.width == 640 && header.height == 480 );
	CHECK( header.direction == Direction::None && header.hasMotionTime && header.motionTime == 0.5f );
	CHECK( menu.events.size() == 3 );
	CHECK( menu.events[0].name == "bgcolor" && menu.events[0].args.size() == 4 );
	// A quoted \n is a line feed.
	CHECK( menu.events[2].args.size() == 1 && menu.events[2].args[0] == "widgetcommand backtogame showifalive\n" );
	CHECK( menu.widgets.size() == 2 );
	CHECK( menu.widgets[0].classToken == "Label" && CanonicalClass( "Label" ) == "FakkLabel" );
	CHECK( menu.widgets[0].properties.size() == 3 );
	CHECK( menu.widgets[0].properties[1].args.size() == 4 );
	CHECK( menu.widgets[0].properties[2].args[1] == "ui/main/mainA" );
	const Widget &button = menu.widgets[1];
	CHECK( CanonicalClass( button.classToken ) == "UIButton" );
	// A quoted ';' is text; outside quotes it starts a comment.
	CHECK( button.properties[1].args[0].find( "togglemenu newgame;" ) != std::string::npos );
	CHECK( button.properties[2].args.size() == 2 && button.properties[2].args[1] == "quit" );
	// The comma separates like whitespace.
	CHECK( button.properties[3].args.size() == 2 && button.properties[3].args[1] == "2" );
	CHECK( button.properties[3].line == 19 );
}

void TestHeaders() {
	Property header;
	header.args = { "zound", "640", "480", "from_left" };
	MenuHeader out;
	CHECK( ReadMenuHeader( header, out ) );
	CHECK( out.direction == Direction::FromLeft && !out.hasMotionTime );
	header.args = { "a", "1", "2" };
	CHECK( !ReadMenuHeader( header, out ) );
	// from_point is a widget direction only.
	CHECK( DirectionFromName( "from_point", false ) == Direction::None );
	CHECK( DirectionFromName( "FROM_POINT", true ) == Direction::FromPoint );
	int h = 0, v = 1;
	ReadJustify( { "right", "bottom" }, h, v );
	CHECK( h == 2 && v == 2 );
	ReadJustify( { "sideways" }, h, v );
	CHECK( h == 2 && v == 2 );
	int style = 2;
	CHECK( ReadBorderStyle( "none", style ) && style == 0 );
	CHECK( !ReadBorderStyle( "RAISED", style ) && style == 0 );
	CHECK( Join( { "Back", "to", "game" } ) == "Back to game" );
	CHECK( Join( { "y", "quit", "now" }, 1 ) == "quit now" );
}

void TestMalformed() {
	// A resource with no brace, an unknown class, an event outside a menu, a
	// widget outside a menu, and an unterminated resource.
	Layout layout = ParseText( "bgcolor 1 1 1 1\nresource Label { name x }\n" );
	CHECK( layout.menus.empty() && layout.diagnostics.size() == 2 );
	layout = ParseText( "menu m 640 480 NONE\nresource Label name x\n" );
	CHECK( layout.menus.size() == 1 && layout.menus[0].widgets.empty() && !layout.diagnostics.empty() );
	layout = ParseText( "menu m 640 480 NONE\nresource Gizmo {\n name x\n}\n" );
	CHECK( layout.menus[0].widgets.size() == 1 && layout.diagnostics.size() == 1 );
	CHECK( CanonicalClass( "Gizmo" ).empty() );
	layout = ParseText( "menu m 640 480 NONE\nresource Button {\n name x\n" );
	CHECK( layout.menus[0].widgets.empty() && layout.diagnostics.size() == 1 );
	// An unterminated quote ends at its line.
	layout = ParseText( "menu m 640 480 NONE\ntitle \"open\nbgcolor 0 0 0 0\n" );
	CHECK( layout.menus[0].events.size() == 2 && layout.menus[0].events[0].args[0] == "open" );
	// A token longer than 255 bytes is cut there.
	layout = ParseText( "menu m 640 480 NONE\ntitle " + std::string( 300, 'x' ) + "\n" );
	CHECK( layout.menus[0].events[0].args[0].size() == MaxTokenBytes );
	// `end.` is case-sensitive; `End.` is an event.
	layout = ParseText( "menu m 640 480 NONE\nEnd.\nfullscreen 1\n" );
	CHECK( !layout.ended && layout.menus[0].events.size() == 2 );
	// Escapes outside quotes.
	layout = ParseText( "menu m 640 480 NONE\ntitle a\\\"b\n" );
	CHECK( layout.menus[0].events[0].args[0] == "a\"b" );
}

void TestInclude() {
	std::map<std::string, std::string> files = {
		{ "ui/common.inc", "bgcolor 1 0 0 1\nend.\nfullscreen 1\n" },
		{ "ui/self.inc", "include ui/self.inc\n" },
	};
	const IncludeReader reader = [&files]( const std::string &name, std::string &text ) {
		const auto found = files.find( name );
		if ( found == files.end() ) return false;
		text = found->second;
		return true;
	};
	Layout layout = ParseText( "menu m 640 480 NONE\ninclude ui/common.inc\nborderstyle NONE\n", reader );
	// The included file's end. ends that file only.
	CHECK( layout.menus[0].events.size() == 2 );
	CHECK( layout.menus[0].events[0].name == "bgcolor" && layout.menus[0].events[1].name == "borderstyle" );
	layout = ParseText( "menu m 640 480 NONE\ninclude ui/missing.inc\n", reader );
	CHECK( layout.diagnostics.size() == 1 );
	layout = ParseText( "menu m 640 480 NONE\ninclude ui/self.inc\n", reader );
	CHECK( !layout.diagnostics.empty() );
}

/* An include cycle cannot multiply the work: a file that includes itself
 * three times would reach 3^8 files at the depth limit alone, but one parse
 * reads at most MaxIncludes files and MaxIncludedBytes of included text,
 * and keeps MaxDiagnostics diagnostics. */
void TestIncludeBudget() {
	int reads = 0;
	std::string fanOut = "title x\n";
	for ( int i = 0; i < 3; ++i ) fanOut += "include ui/fan.inc\n";
	const IncludeReader fan = [&]( const std::string &name, std::string &text ) {
		++reads;
		if ( name != "ui/fan.inc" ) return false;
		text = fanOut;
		return true;
	};
	Layout layout = ParseText( "menu m 640 480 NONE\ninclude ui/fan.inc\n", fan );
	CHECK( reads == MaxIncludes );
	CHECK( layout.menus.size() == 1 && layout.menus[0].events.size() == static_cast<std::size_t>( MaxIncludes ) );
	bool refused = false;
	for ( const Diagnostic &diagnostic : layout.diagnostics ) {
		if ( diagnostic.message == "too many includes: ui/fan.inc" ) refused = true;
	}
	CHECK( refused );

	// A missing file spends an include too, so a file naming thousands of
	// them asks the reader MaxIncludes times.
	reads = 0;
	std::string missing = "menu m 640 480 NONE\n";
	for ( int i = 0; i < 1000; ++i ) missing += "include ui/none.inc\n";
	layout = ParseText( missing, fan );
	CHECK( reads == MaxIncludes );
	CHECK( layout.diagnostics.size() == MaxDiagnostics );
	CHECK( layout.diagnosticsDropped == 1000 - MaxDiagnostics );

	// The included bytes are budgeted across files.
	const std::string big( MaxIncludedBytes / 2 + 1, ' ' );
	const IncludeReader large = [&]( const std::string &, std::string &text ) {
		text = big;
		return true;
	};
	layout = ParseText( "menu m 640 480 NONE\ninclude ui/a.inc\ninclude ui/b.inc\n", large );
	CHECK( layout.diagnostics.size() == 1 );
	CHECK( layout.diagnostics[0].message == "included text too large: ui/b.inc" && layout.diagnostics[0].line == 3 );

	// Diagnostics past the cap are counted, not kept.
	std::string noise;
	for ( int i = 0; i < 300; ++i ) noise += "orphan\n";
	layout = ParseText( noise );
	CHECK( layout.diagnostics.size() == MaxDiagnostics && layout.diagnosticsDropped == 300 - MaxDiagnostics );
	CHECK( layout.diagnostics.back().line == static_cast<int>( MaxDiagnostics ) );
}

/* The twelve retail menus: every one parses to one menu with a header, every
 * widget names a known class, and the menus the main flow needs carry the
 * widgets and commands it uses. */
void TestRetailMenus() {
	fntech3_tests::AlicePak pak;
	if ( !pak.Available() ) {
		std::cout << "retail menus: skipped, no installation at " << pak.Path() << "\n";
		return;
	}
	int files = 0, widgets = 0;
	for ( const fntech3_tests::PakEntry &entry : pak.Entries() ) {
		const std::string lower = fntech3_tests::Lower( entry.name );
		if ( !fntech3_tests::EndsWith( lower, ".urc" ) ) continue;
		std::string text;
		CHECK( pak.Read( entry, text ) );
		const Layout layout = ParseText( text );
		for ( const Diagnostic &diagnostic : layout.diagnostics ) {
			std::cerr << entry.name << ":" << diagnostic.line << ": " << diagnostic.message << "\n";
		}
		CHECK( layout.diagnostics.empty() );
		CHECK( layout.ended );
		CHECK( layout.menus.size() == 1 );
		MenuHeader header;
		CHECK( ReadMenuHeader( layout.menus[0].header, header ) );
		for ( const Widget &widget : layout.menus[0].widgets ) {
			CHECK( !CanonicalClass( widget.classToken ).empty() );
			++widgets;
		}
		if ( lower == "ui/main.urc" ) {
			CHECK( header.name == "main" && header.motionTime == 0.5f );
			CHECK( layout.menus[0].widgets.size() == 20 );
			CHECK( layout.menus[0].widgets[12].classToken == "Button" );
		}
		if ( lower == "ui/newgame.urc" ) CHECK( layout.menus[0].widgets.size() == 11 );
		if ( lower == "ui/quit.urc" ) CHECK( layout.menus[0].widgets.size() == 10 );
		++files;
	}
	CHECK( files == 12 );
	std::cout << "retail menus: " << files << " files, " << widgets << " widgets from " << pak.Path() << "\n";
}

} // namespace

int main() {
	TestMenu();
	TestHeaders();
	TestMalformed();
	TestInclude();
	TestIncludeBudget();
	TestRetailMenus();
	std::cout << "alice urc tests passed\n";
	return 0;
}
