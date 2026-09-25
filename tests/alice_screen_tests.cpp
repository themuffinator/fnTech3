/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's engine-drawn screen arithmetic and tables
 * (code/compat/alice_screen.hpp): print timing, located-print placement,
 * fade and letterbox decoding, the intro's states, and the loading screen's
 * tables and map list. Every constant is pinned to the retail address the
 * header names. The retail map list is also read, read-only, when an
 * installation is present. */

#include "../code/compat/alice_screen.hpp"
#include "alice_pak_reader.hpp"

#include <cmath>
#include <iostream>

#ifdef _MSC_VER
// Several checks pin constexpr tables, whose conditions are constant.
#pragma warning( disable : 4127 )
#endif

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::screen;

namespace {

bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }

void TestPrints() {
	CHECK( PrintTimeMs == 4000.0f && PrintFadeMs == 750.0f && PrintHoldEndMs == 3250.0f );
	// 750 ms in, the plateau, 750 ms out.
	CHECK( Near( PrintAlpha( 4000, 1 ), 0 ) );
	CHECK( Near( PrintAlpha( 3625, 1 ), 0.5f ) );
	CHECK( Near( PrintAlpha( 3250, 1 ), 1 ) );
	CHECK( Near( PrintAlpha( 2000, 0.25f ), 1 ) );   // alpha ignored on the plateau
	CHECK( Near( PrintAlpha( 750, 1 ), 1 ) );
	CHECK( Near( PrintAlpha( 375, 1 ), 0.5f ) );
	CHECK( Near( PrintAlpha( 375, 0.5f ), 0.25f ) );  // alpha scales the ramps
	CHECK( Near( PrintAlpha( 4000, 0.5f ), 0.5f ) );
	CHECK( Near( PrintAlpha( 0, 1 ), 0 ) );
	CHECK( Near( PrintAlpha( 3500, 2.0f ), 1 - 250.0f / 750.0f * 2.0f ) );
	CHECK( Near( PrintAlpha( 5000, 1 ), 0 ) );        // clamped at zero
	CHECK( Near( PrintAlpha( 3000, -1 ), 1 ) );
	CHECK( AdvancePrintTimer( 100, 16 ) == 84 );
	CHECK( AdvancePrintTimer( 10, 16 ) == 0 );

	// A located print: x from the left, y up from the bottom to the text's
	// bottom edge, both truncated; -1 centres.
	LocatedText text = LocatedTextOrigin( 320, 100, 13, 800, 600 );
	CHECK( text.x == 400 && text.y == static_cast<int>( ( 480 - 100 - 13 ) / 480.0f * 600.0f ) );
	CHECK( text.horizontal == 1 && text.vertical == 0 );
	text = LocatedTextOrigin( -1, -1, 13, 800, 600 );
	CHECK( text.x == 0 && text.y == 0 && text.horizontal == 0 && text.vertical == 1 );
	text = LocatedTextOrigin( 1, 0, 16, 1000, 480 );
	CHECK( text.x == 1 && text.y == 464 );
}

void TestView() {
	CHECK( FadeKind( 0 ) == Fade::Blend && FadeKind( 1 ) == Fade::Additive );
	CHECK( FadeKind( 2 ) == Fade::Drug && FadeKind( 3 ) == Fade::Ice );
	CHECK( FadeKind( 4 ) == Fade::None && FadeKind( -1 ) == Fade::None );
	CHECK( Near( LetterboxFraction( 32767 ), 1.0f ) );
	CHECK( Near( LetterboxFraction( 0 ), 0.0f ) );
	CHECK( LetterboxFraction( -5 ) < 0.0f );
	CHECK( LetterboxFraction( 65534 ) > 1.9f );       // not clamped
	CHECK( Near( FpsGoodAbove, 23.94f ) && FpsFairAbove == 18.0f );
	CHECK( HudShowDelayMs == 550 );
}

void TestDialog() {
	// asrafel's height is 30: 20-unit lines, three to the 64-unit box.
	const DialogLayout layout = DialogLayoutFor( DialogWidth, DialogHeight, 30 );
	CHECK( layout.x == 128 && layout.y == 406 && layout.wrapWidth == 296 );
	CHECK( layout.lineHeight == 20 && layout.visibleLines == 3 && layout.textX == 40 );

	DialogTiming timing;
	CHECK( timing.fadeIn == 200 && timing.fadeTime == 5000 && timing.fadeSpeed == 1000 );
	CHECK( DialogCommand( "*fadetime 2.5", timing ) == 0 && timing.fadeTime == 2500 );
	CHECK( DialogCommand( "*fadetime -1", timing ) == 0 && timing.fadeTime == 2500 );
	DialogCommand( "*fadespeed 0.5", timing );
	CHECK( timing.fadeSpeed == 500 );
	DialogCommand( "*fadeinspeed 0.25", timing );
	CHECK( timing.fadeIn == 250 );
	DialogCommand( "*position 4", timing );
	CHECK( timing.position == 4 );
	DialogCommand( "*position 11", timing );
	CHECK( timing.position == 4 );
	CHECK( DialogCommand( "*cleartext", timing ) == 1 );
	CHECK( DialogCommand( "*resetdialog", timing ) == 2 );
	DialogCommand( "*time 3", timing );
	CHECK( timing.time == 3000 && timing.fadeTime == 3200 && timing.fadeIn == 200 && timing.fadeSpeed == 1000 );
	DialogCommand( "*time 0", timing );
	CHECK( timing.time == 0 && timing.fadeTime == 5000 );
	CHECK( DialogCommand( "*unknown 1", timing ) == 0 );

	CHECK( Near( DialogAlpha( 100, timing ), 0.5f ) );
	CHECK( Near( DialogAlpha( 1000, timing ), 1 ) );
	CHECK( Near( DialogAlpha( 5500, timing ), 0.5f ) );
	CHECK( DialogAlpha( 6000, timing ) < 0 );

	CHECK( DialogLinesShown( 0, 5000, 5, 3 ) == 1 );
	CHECK( DialogLinesShown( 2500, 5000, 5, 3 ) == 2 );
	CHECK( DialogLinesShown( 5000, 5000, 5, 3 ) == 3 );

	// Every character ten wide, the wrap at 50: five to a line.
	const auto ten = []( char c ) { return c == '\n' ? 0 : 10; };
	std::vector<std::string> lines = DialogWrap( "abc\n", 50, ten );
	CHECK( lines.size() == 2 && lines[0] == "abc" && lines[1].empty() );
	// Without the game's newline the first pass's width carries over.
	lines = DialogWrap( "abc", 50, ten );
	CHECK( lines.size() == 2 && lines[0] == "ab" && lines[1] == "c" );
	// The overflow backs up to the last space.
	lines = DialogWrap( "ab cdef gh", 50, ten );
	CHECK( lines.size() == 3 );
	CHECK( lines[0] == "ab" && lines[1] == "cdef" && lines[2] == "gh" );
	// The second pass carries the first pass's final width (two characters
	// here) into the first line, which then breaks a word early.
	lines = DialogWrap( "abcd de", 50, ten );
	CHECK( lines.size() == 3 );
	CHECK( lines[0] == "abc" && lines[1] == "d" && lines[2] == "de" );
	// A newline is kept as a break.
	lines = DialogWrap( "a\nb", 50, ten );
	CHECK( lines.size() == 2 && lines[0] == "a" && lines[1] == "b" );
	// The game's own lines end with a newline, which clears the carried
	// width and leaves an empty last line.
	lines = DialogWrap( "abcd de\n", 50, ten );
	CHECK( lines.size() == 3 && lines[0] == "abcd" && lines[1] == "de" && lines[2].empty() );
	CHECK( DialogTextColor[0] > 0.12f && DialogTextColor[2] < 0.02f );
	CHECK( DialogWrap( nullptr, 50, ten ).size() == 1 );
}

void TestIntro() {
	// -1 lands in the main menu's case after its push (0x0040fdd2).
	IntroStep step = Intro( -1 );
	CHECK( step.action == IntroAction::Soundtrack && step.nextState == 0 && step.waitMs == -1 );
	step = Intro( 0 );
	CHECK( step.action == IntroAction::Black && step.nextState == 1 && step.waitMs == 5000 );
	step = Intro( 1 );
	CHECK( step.action == IntroAction::PlateIn && step.nextState == 2 && step.waitMs == 1000 );
	CHECK( step.plateFrom == 0 && step.plateTo == 1 && step.plateMs == 1000 );
	step = Intro( 2 );
	CHECK( step.action == IntroAction::PlateHold && step.waitMs == 5000 && step.plateMs == 0 );
	step = Intro( 3 );
	CHECK( step.action == IntroAction::PlateOut && step.waitMs == 1000 && step.plateTo == 0 );
	step = Intro( 4 );
	CHECK( step.action == IntroAction::Video && !std::strcmp( step.video, "ealogo.roq" ) );
	CHECK( !std::strcmp( step.videoFlags, "25" ) && step.videoResumeState == 5 && step.waitMs == -1 );
	step = Intro( 5 );
	CHECK( !std::strcmp( step.video, "roguelogo.roq" ) && step.videoResumeState == 6 );
	CHECK( !std::strcmp( step.videoFlags, "26" ) && step.waitMs == -1 );
	CHECK( Intro( 6 ).action == IntroAction::MainMenu && Intro( 6 ).nextState == 0 );
	CHECK( Intro( 100 ).action == IntroAction::MainMenu );
	step = Intro( 7 );
	CHECK( step.action == IntroAction::Skip && step.nextState == 8 && step.waitMs == 0 );
	CHECK( Intro( 99 ).nextState == 100 );
	// Outside -1..100 the default steps up with "wait 0" (0x0040fc85).
	step = Intro( 101 );
	CHECK( step.action == IntroAction::Skip && step.nextState == 102 && step.waitMs == 0 );
	step = Intro( -2 );
	CHECK( step.action == IntroAction::Skip && step.nextState == -1 && step.waitMs == 0 );
	CHECK( Intro( -48 ).nextState == -47 );  // a bare "2" flag's state
	CHECK( Intro( 2147483647 ).action == IntroAction::Skip );
	CHECK( IntroPlateScale == 1.8f );

	CHECK( Near( IntroPlateAlpha( 0, 1, 1000, 500 ), 0.5f ) );
	CHECK( Near( IntroPlateAlpha( 0, 1, 1000, 1500 ), 1 ) );
	CHECK( Near( IntroPlateAlpha( 1, 0, 1000, -10 ), 1 ) );
	CHECK( Near( IntroPlateAlpha( 1, 0, 0, 5000 ), 1 ) );  // no length: full
	CHECK( Intro( 1 ).plateScale == 1.8f && !std::strcmp( Intro( 2 ).plateImage, IntroPlateImage ) );
	CHECK( Intro( -1 ).soundtrack && Intro( 6 ).soundtrack && !Intro( 0 ).soundtrack );
}

// Heavy Metal: F.A.K.K.2's (fakk2.exe 1.02 _intro 0x0040e0b0).
void TestFakk2Intro() {
	IntroStep step = Fakk2Intro( 0 );
	CHECK( step.action == IntroAction::Soundtrack && step.soundtrack && step.nextState == 1 && step.waitMs == 3000 );
	const char *plates[] = { "textures/menu/ritual", "textures/menu/gathering" };
	for ( int plate = 0; plate < 2; ++plate ) {
		const int first = 1 + plate * 3;
		step = Fakk2Intro( first );
		CHECK( step.action == IntroAction::PlateIn && step.nextState == first + 1 && step.waitMs == 1000 );
		CHECK( step.plateFrom == 0 && step.plateTo == 1 && step.plateMs == 1000 && step.plateScale == 1.5f );
		CHECK( !std::strcmp( step.plateImage, plates[plate] ) );
		step = Fakk2Intro( first + 1 );
		CHECK( step.action == IntroAction::PlateHold && step.waitMs == 3000 && step.plateMs == 0 );
		CHECK( !std::strcmp( step.plateImage, plates[plate] ) );
		step = Fakk2Intro( first + 2 );
		CHECK( step.action == IntroAction::PlateOut && step.waitMs == 1000 && step.plateTo == 0 );
		CHECK( step.nextState == first + 3 && !std::strcmp( step.plateImage, plates[plate] ) );
	}
	// The menu, with the soundtrack already playing, and nothing queued after.
	step = Fakk2Intro( 7 );
	CHECK( step.action == IntroAction::MainMenu && step.nextState == 0 && step.waitMs == -1 && !step.soundtrack );
	// A map loaded only puts the plate away.
	step = Fakk2Intro( -1 );
	CHECK( step.action == IntroAction::End && step.nextState == 0 && step.waitMs == -1 && !step.soundtrack );
	step = Fakk2Intro( 8 );
	CHECK( step.action == IntroAction::Skip && step.nextState == 9 && step.waitMs == 0 );
	CHECK( Fakk2Intro( -2 ).nextState == -1 && Fakk2Intro( 2147483647 ).action == IntroAction::Skip );
	for ( int state = 0; state < 8; ++state ) CHECK( Fakk2Intro( state ).video == nullptr );
}

void TestLoadingTables() {
	CHECK( sizeof( WorldMapTiles ) / sizeof( WorldMapTiles[0] ) == 6 );
	float area = 0;
	for ( const Tile &tile : WorldMapTiles ) area += tile.w * tile.h;
	CHECK( area == 640.0f * 480.0f );
	CHECK( sizeof( Zones ) / sizeof( Zones[0] ) == 9 );
	CHECK( Zones[0].x == 20 && Zones[0].y == 318 && Zones[8].x == 430 && Zones[8].y == 40 );
	CHECK( sizeof( Regions ) / sizeof( Regions[0] ) == 13 );
	CHECK( Regions[0].x == 107 && Regions[0].zone == 2 && Regions[12].zone == 9 );
	CHECK( Regions[10].h == 200 );
	CHECK( LoadingDotX[0] == 426 && LoadingDotX[8] == 567 && LoadingDotY == 418 );
	CHECK( IndicatorRect[0] == 370 && IndicatorRect[1] == 401 );
	CHECK( ClampLoadingStage( 12 ) == 9 && ClampLoadingStage( 3 ) == 3 );
	CHECK( LoadingTitleX == 400 && LoadingTitleY == 440 );
}

void TestMapList() {
	const std::string text =
		"// Village of the Doomed\r\n"
		"1\t0.9\t\tgvillage\t\t\"Dementia\" \r\n"
		"4 0.8 garden1 \"Dry Landing\"\r\n";
	MapList list = ParseMapList( text.data(), text.size() );
	CHECK( list.error.empty() && list.entries.size() == 2 );
	CHECK( list.entries[1].zone == 4 && Near( list.entries[1].scale, 0.8f ) );
	CHECK( list.entries[1].title == "Dry Landing" );
	CHECK( list.Find( "garden1" ) == 1 && list.Find( "GARDEN1" ) == -1 && list.Find( nullptr ) == -1 );

	const std::string shortRow = "1 0.9 gvillage \"Dementia\"\n2 0.9\n";
	list = ParseMapList( shortRow.data(), shortRow.size() );
	CHECK( list.entries.size() == 1 && list.error == "UI_LoadMapList: Missing map file name." );
	const std::string noTitle = "1 0.9 gvillage";
	list = ParseMapList( noTitle.data(), noTitle.size() );
	CHECK( list.entries.empty() && list.error == "UI_LoadMapList: Missing text name." );
	const std::string noScale = "1";
	list = ParseMapList( noScale.data(), noScale.size() );
	CHECK( list.error == "UI_LoadMapList: Missing scale param." );
	CHECK( ParseMapList( nullptr, 0 ).entries.empty() );

	CHECK( MapNameForLoading( "garden1$garden1_start1" ) == "garden1" );
	CHECK( MapNameForLoading( "gvillage" ) == "gvillage" );
	CHECK( MapNameForLoading( nullptr ).empty() );
}

void TestRetailMapList() {
	fntech3_tests::AlicePak pak;
	if ( !pak.Available() ) {
		std::cout << "retail map list: skipped, no installation at " << pak.Path() << "\n";
		return;
	}
	for ( const fntech3_tests::PakEntry &entry : pak.Entries() ) {
		if ( fntech3_tests::Lower( entry.name ) != "ui/maplist.scr" ) continue;
		std::string text;
		CHECK( pak.Read( entry, text ) );
		const MapList list = ParseMapList( text.data(), text.size() );
		CHECK( list.error.empty() );
		const int village = list.Find( "gvillage" );
		CHECK( village >= 0 && list.entries[village].zone == 1 && list.entries[village].title == "Dementia" );
		const int garden = list.Find( "garden1" );
		CHECK( garden >= 0 && list.entries[garden].zone == 4 && list.entries[garden].title == "Dry Landing" );
		for ( const MapListEntry &row : list.entries ) {
			CHECK( row.zone >= 1 && row.zone <= 9 );
		}
		std::cout << "retail map list: " << list.entries.size() << " maps\n";
		return;
	}
	CHECK( !"ui/maplist.scr is missing from the retail pak" );
}

} // namespace

int main() {
	TestPrints();
	TestView();
	TestDialog();
	TestIntro();
	TestFakk2Intro();
	TestLoadingTables();
	TestMapList();
	TestRetailMapList();
	std::cout << "alice screen tests passed\n";
	return 0;
}
