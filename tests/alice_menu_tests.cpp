/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's menu widget rules (code/compat/alice_menu.hpp): colours, the
 * slider's arithmetic, the list and the field, the bind files, the load/save
 * pages and the credits scroller. Fixtures are written here; the retail
 * bind.scr and keybind.scr are also read, read-only, when an installation is
 * present. */

#include "../code/compat/alice_menu.hpp"
#include "alice_pak_reader.hpp"

#include <cmath>
#include <iostream>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::menu;

namespace {

bool Near( float a, float b, float epsilon = 1e-4f ) { return std::fabs( a - b ) <= epsilon; }

void TestColours() {
	// A grey keeps its hue undefined and its value; the light shade is 4/3 of it.
	const Hsv grey = RgbToHsv( { 0.75f, 0.75f, 0.75f, 1.0f } );
	CHECK( grey.h == -1.0f && Near( grey.s, 0.0f ) && Near( grey.v, 0.75f ) );
	const BorderColor border = MakeBorderColor( { 0.75f, 0.75f, 0.75f, 1.0f } );
	CHECK( Near( border.dark.r, 0.5f ) && Near( border.reallyDark.g, 0.25f ) && Near( border.light.b, 1.0f ) );
	CHECK( border.light.a == 1.0f && border.original.r == 0.75f );

	// A colour's hue survives the round trip.
	const Rgba blue = { 0.0f, 0.5f, 1.0f, 1.0f };
	const Hsv hsv = RgbToHsv( blue );
	CHECK( Near( hsv.h, 210.0f ) && Near( hsv.s, 1.0f ) && Near( hsv.v, 1.0f ) );
	const Rgba back = HsvToRgb( hsv );
	CHECK( Near( back.r, 0.0f ) && Near( back.g, 0.5f ) && Near( back.b, 1.0f ) );
	// Red wraps below zero to 360 - x.
	CHECK( Near( RgbToHsv( { 1.0f, 0.0f, 0.2f, 1.0f } ).h, 348.0f ) );
	// The light shade of a blue lowers the saturation and raises the value.
	const BorderColor blueBorder = MakeBorderColor( blue );
	CHECK( Near( blueBorder.dark.b, 0.6666667f ) && Near( blueBorder.reallyDark.g, 0.5f / 3.0f ) );
	CHECK( blueBorder.light.b > 1.0f && blueBorder.light.r > 0.0f );
	// A saturation within 0.035 of zero counts as grey both ways.
	CHECK( RgbToHsv( { 1.0f, 0.98f, 0.98f, 1.0f } ).h == -1.0f );
	const Rgba flat = HsvToRgb( { 120.0f, 0.03f, 0.4f, 0.5f } );
	// Cast inexact literals: GCC's i686 x87 C++ keeps a bare 0.4f at long double precision.
	CHECK( flat.r == float( 0.4f ) && flat.g == float( 0.4f ) && flat.b == float( 0.4f ) && flat.a == 0.5f );

	BoxEdge raised[6], indented[6];
	Box3DEdges( 10.0f, 20.0f, 100.0f, 16.0f, false, raised );
	Box3DEdges( 10.0f, 20.0f, 100.0f, 16.0f, true, indented );
	// Raised: light along the top and left, really dark on the outer bottom and right.
	CHECK( raised[4].shade == Shade::Light && raised[4].x == 10.0f && raised[4].h == 15.0f );
	CHECK( raised[5].shade == Shade::Light && raised[5].y == 20.0f && raised[5].w == 99.0f );
	CHECK( raised[1].shade == Shade::ReallyDark && raised[1].x == 109.0f && raised[1].h == 16.0f );
	CHECK( raised[2].shade == Shade::Dark && raised[2].y == 34.0f && raised[2].w == 98.0f );
	// Indented: dark top and left with a really dark inner line, light bottom and right.
	CHECK( indented[0].shade == Shade::Dark && indented[0].x == 10.0f && indented[0].h == 15.0f );
	CHECK( indented[1].shade == Shade::ReallyDark && indented[1].x == 11.0f && indented[1].h == 13.0f );
	CHECK( indented[4].shade == Shade::Light && indented[4].x == 109.0f );
	CHECK( indented[5].shade == Shade::Light && indented[5].y == 35.0f && indented[5].w == 100.0f );
	CHECK( &ShadeColor( border, Shade::Light ) == &border.light );
}

void TestCheckBox() {
	// Alice's box at 0 and its title after it; F.A.K.K.2's box 16 in and its
	// title 48 in, where both retail engines print it.
	static_assert( CheckSize == 16.0f && Fakk2CheckIndent == 16.0f );
	static_assert( CheckTitleX( 0.0f ) == 16.0f && CheckTitleX( Fakk2CheckIndent ) == 48.0f );
	std::cout << "alice_menu_tests: checkbox indents\n";
}

void TestSlider() {
	Slider slider;
	CHECK( slider.min == 0.0f && slider.max == 100.0f && slider.step == 1.0f && slider.type == SliderType::Integer );
	SetSliderType( slider, "FLOAT" );
	CHECK( slider.type == SliderType::Float );
	SetSliderType( slider, "int" );   // zound.urc's word, which retail passes over
	CHECK( slider.type == SliderType::Float );
	SetSliderType( slider, "integer" );
	CHECK( slider.type == SliderType::Integer );

	// controls.urc's brightness slider: rect 88 204 200 16 in a 640 menu.
	const SliderTrack track = TrackFor( slider, 88.0f, 200.0f );
	CHECK( track.start == 106 && track.end == 270 );
	slider.min = 0.5f;
	slider.max = 1.5f;
	slider.value = 1.0f;
	CHECK( Near( SliderFraction( slider ), 0.5f ) );
	CHECK( Near( ThumbCenter( slider, track ), 164.0f * 0.5f + 6.0f + 12.0f ) );
	CHECK( ValueAt( slider, track, 106.0f ) == 0.5f && ValueAt( slider, track, 300.0f ) == 1.5f );
	CHECK( Near( ValueAt( slider, track, 188.0f ), 1.0f ) );
	slider.value = 3.0f;
	CHECK( SliderFraction( slider ) == 1.0f );
	slider.value = -3.0f;
	CHECK( SliderFraction( slider ) == 0.0f );

	// A positive step holds the value from min up to max.
	Slider volume;
	volume.min = 0.0f; volume.max = 1.0f; volume.step = 0.05f; volume.value = 0.98f;
	CHECK( Stepped( volume, 1 ) == 1.0f );
	volume.value = 0.02f;
	CHECK( Stepped( volume, -1 ) == 0.0f );
	// r_picmip: setrange 3 0, stepsize -1, a reversed range.
	Slider picmip;
	picmip.min = 3.0f; picmip.max = 0.0f; picmip.step = -1.0f; picmip.value = 1.0f;
	CHECK( Stepped( picmip, 1 ) == 0.0f );
	CHECK( Stepped( picmip, -1 ) == 2.0f );
	picmip.value = 0.0f;
	CHECK( Stepped( picmip, 1 ) == 0.0f );
	picmip.value = 3.0f;
	CHECK( Stepped( picmip, -1 ) == 3.0f );
	CHECK( SliderFraction( picmip ) == 0.0f );
	// cg_cameradist: setrange 76 192 with stepsize -1 lands on max either way.
	Slider camera;
	camera.min = 76.0f; camera.max = 192.0f; camera.step = -1.0f; camera.value = 120.0f;
	CHECK( Stepped( camera, 1 ) == 192.0f && Stepped( camera, -1 ) == 192.0f );
	// F.A.K.K.2 holds to min and then max whatever the step: the same on an
	// ordinary range, and a reversed one always lands on max.
	volume.value = 0.98f;
	CHECK( SteppedFakk2( volume, 1 ) == 1.0f );
	volume.value = 0.02f;
	CHECK( SteppedFakk2( volume, -1 ) == 0.0f );
	picmip.value = 1.0f;
	CHECK( SteppedFakk2( picmip, 1 ) == 0.0f && SteppedFakk2( picmip, -1 ) == 0.0f );
	CHECK( SteppedFakk2( camera, 1 ) == 119.0f && SteppedFakk2( camera, -1 ) == 121.0f );
	// Its 192-wide sliders: arrows 15, a thumb 10, the thumb 15 + 152t across.
	Slider fakk2;
	fakk2.arrowWidth = 15; fakk2.thumbWidth = 10.0f; fakk2.min = 0.0f; fakk2.max = 1.0f; fakk2.value = 1.0f;
	const SliderTrack groove = TrackFor( fakk2, 385.0f, 192.0f );
	CHECK( groove.start == 405 && groove.end == 557 );
	CHECK( ThumbCenter( fakk2, groove ) - fakk2.thumbWidth * 0.5f == 167.0f );

	camera.type = SliderType::Integer;
	camera.value = 119.9f;
	CHECK( SliderCvarText( camera ) == "119" && SliderCommandValue( camera ) == "119" );
	camera.type = SliderType::Float;
	camera.value = 0.25f;
	CHECK( SliderCvarText( camera ) == "0.250000" && SliderCommandValue( camera ) == "0.250" );
}

void TestList() {
	const std::vector<ListItem> items = { { "3", "640x480" }, { "4", "800x600" }, { "miles", "" } };
	CHECK( ListFind( items, "4" ) == 2 && ListFind( items, "miles" ) == 3 );
	CHECK( ListFind( items, "MILES" ) == 0 && ListFind( items, nullptr ) == 0 );
	CHECK( ListShows( items[0] ) == "640x480" && ListShows( items[2] ) == "miles" );
	CHECK( ListNext( 3, 3 ) == 3 && ListNext( 1, 3 ) == 2 );
	CHECK( ListPrevious( 1 ) == 1 && ListPrevious( 0 ) == 1 && ListPrevious( 3 ) == 2 );
	CHECK( ListCycle( 3, 3 ) == 1 && ListCycle( 0, 3 ) == 1 );
}

void TestField() {
	EditLine line;
	CHECK( line.Insert( 'a' ) && line.Insert( 'c' ) );
	line.Left();
	CHECK( line.Insert( 'b' ) && line.text == "abc" && line.cursor == 2 );
	CHECK( line.Backspace() && line.text == "ac" && line.cursor == 1 );
	CHECK( line.Delete() && line.text == "a" && !line.Delete() );
	line.Home();
	CHECK( !line.Backspace() && line.cursor == 0 );
	line.End();
	CHECK( line.cursor == 1 && !line.Insert( '\x1f' ) );
	line.text.assign( FieldMaxLength, 'x' );
	line.End();
	CHECK( !line.Insert( 'y' ) && static_cast<int>( line.text.size() ) == FieldMaxLength );
	line.cursor = 1000;
	line.Clamp();
	CHECK( line.cursor == FieldMaxLength );

	// Ten units a character, a field 50 wide with a 10-unit indent.
	const auto width = []( const char *text ) { return static_cast<int>( std::strlen( text ) ) * 10; };
	CHECK( FieldFirstVisible( "abc", 3, 10.0f, 50.0f, 10.0f, width ) == 0 );
	CHECK( FieldFirstVisible( "abcdefgh", 8, 10.0f, 50.0f, 10.0f, width ) == 5 );
	CHECK( FieldFirstVisible( "abcdefgh", 1, 10.0f, 50.0f, 10.0f, width ) == 1 );
	CHECK( FieldFirstVisible( "abcdefgh", 6, 10.0f, 50.0f, 10.0f, width ) == 4 );
}

void TestBindScript() {
	const std::string text =
		"//header 1 \"Action\"\r\n"
		"bind_width\t\t230\t\t// 214\r\n"
		"bind_commandwidth 145\r\nbind_commandheight 22\r\nbind_primarykeywidth 68\r\nbind_primarykeyheight 22\r\n"
		"align centerx centery\r\n"
		"activebordercolor\t0 0 0 1\r\n"
		"inactivebackgroundcolor 1 1 1 0\r\n"
		"changesound sound/misc/menu/change.wav\r\n"
		"binditem \"Primary Attack\"\t+attackright\tknife_att_1_prim2\tfalse\r\n"
		"binditem \"Turn Left\" +left idle_stand_rocktoes true -45\r\n"
		"binditem \"Equip Blunderbuss\"\t\"use blunderbuss\"\r\n"
		"binditem \"Broken\"\r\n";
	const BindLayout layout = ParseBindScript( text.data(), text.size() );
	CHECK( layout.width == 230 && layout.commandWidth == 145 && layout.commandHeight == 22 );
	CHECK( layout.primaryKeyWidth == 68 && layout.primaryKeyHeight == 22 && layout.height == 0 );
	CHECK( layout.headers[0].empty() );
	CHECK( layout.activeBorder.a == 1.0f && layout.inactiveBackground.r == 1.0f && layout.inactiveBackground.a == 0.0f );
	CHECK( layout.changeSound == "sound/misc/menu/change.wav" );
	CHECK( layout.items.size() == 3 );
	CHECK( layout.items[0].name == "Primary Attack" && layout.items[0].command == "+attackright" );
	CHECK( layout.items[0].anim == "knife_att_1_prim2" && !layout.items[0].loop );
	CHECK( layout.items[1].loop && layout.items[1].rotation == -45.0f );
	CHECK( layout.items[2].command == "use blunderbuss" && layout.items[2].anim.empty() );

	// Heavy Metal: F.A.K.K.2's third words are pictures (fakk2.exe 1.02
	// header 0x00410580, binditem 0x00411c90, bind_fillwidth 0x00411f50).
	const std::string fakk2 =
		"header 1 \"Action\" \"textures/bind/action\"\r\n"
		"header 3 \"Alternate\"\r\n"
		"bind_fillwidth 16 \"textures/menu/F_A\"\r\n"
		"binditem \"Left Attack\"       +attackleft               \"textures/bind/leftattack\"\r\n";
	const BindLayout pictures = ParseBindScript( fakk2.data(), fakk2.size() );
	CHECK( pictures.headers[0] == "Action" && pictures.headerMaterials[0] == "textures/bind/action" );
	CHECK( pictures.headers[2] == "Alternate" && pictures.headerMaterials[2].empty() );
	CHECK( pictures.fillWidth == 16 && pictures.fillMaterial == "textures/menu/F_A" );
	CHECK( pictures.items.size() == 1 && pictures.items[0].material == "textures/bind/leftattack" );

	CHECK( BindPageSize( 300.0f, 22 ) == 12 && BindPageSize( 300.0f, 0 ) == 0 );
	CHECK( VisibleRowTop( 3, 0, 22 ) == 88 && VisibleRowTop( 3, 3, 22 ) == 22 );
	CHECK( RowVisible( 12, 0, 22, 300.0f ) && !RowVisible( 13, 0, 22, 300.0f ) && !RowVisible( 0, 1, 22, 300.0f ) );
	CHECK( ClampScroll( -1, 29, 12 ) == 0 && ClampScroll( 40, 29, 12 ) == 17 && ClampScroll( 5, 3, 12 ) == 0 );

	std::string error;
	const std::string aliases = "\"KP_HOME\"\t\"KP HOME\"\t0.85\r\n\"LEFTARROW\" \"LEFT\" -1.0\r\n// a comment\r\n";
	const std::vector<KeyAlias> parsed = ParseKeyAliases( aliases.data(), aliases.size(), &error );
	CHECK( error.empty() && parsed.size() == 2 );
	CHECK( FindKeyAlias( parsed, "kp_home" ) && FindKeyAlias( parsed, "kp_home" )->name == "KP HOME" );
	CHECK( FindKeyAlias( parsed, "LEFTARROW" )->scale == -1.0f && !FindKeyAlias( parsed, "MOUSE1" ) );
	const std::string broken = "\"KP_HOME\" \"KP HOME\"";
	CHECK( ParseKeyAliases( broken.data(), broken.size(), &error ).empty() );
	CHECK( error == "UI_LoadTransTable: Missing scale param." );
}

void TestSaves() {
	CHECK( !ListsSave( "quicksave" ) && !ListsSave( "QuickSave" ) && ListsSave( "garden10000" ) && !ListsSave( nullptr ) );
	std::vector<SaveRow> rows( 3 );
	rows[0].name = "c"; rows[0].saveTime = 300;
	rows[1].name = "a"; rows[1].saveTime = 100;
	rows[2].name = "b"; rows[2].saveTime = 100;
	SortSaves( rows );
	CHECK( rows[0].name == "a" && rows[1].name == "b" && rows[2].name == "c" );

	SavePage page;
	CHECK( page.tab == -1 && page.lastCount == -1 );
	page.Refreshed( 0 );
	CHECK( page.page == 0 && page.tab == -1 && page.lastCount == 0 );
	page.Refreshed( 8 );
	CHECK( page.page == 1 && page.tab == 2 && page.Selected() == 8 );
	page.tab = 1;
	page.Refreshed( 8 );   // the same length leaves the choice alone
	CHECK( page.tab == 1 );
	CHECK( !page.Next( 8 ) && page.Previous() && page.page == 0 && !page.Previous() );
	CHECK( page.Next( 8 ) && page.page == 1 );
	CHECK( !page.Select( 3, 8 ) && page.Select( 2, 8 ) && page.Selected() == 8 );
	page.page = 0;
	CHECK( page.Next( 7 ) && !page.Next( 7 ) );
	CHECK( !page.Select( 1, 0 ) );
	page.Refreshed( 6 );
	CHECK( page.page == 0 && page.tab == 6 );

	std::tm when{};
	when.tm_year = 126; when.tm_mon = 8; when.tm_mday = 4; when.tm_wday = 5;
	when.tm_hour = 7; when.tm_min = 5; when.tm_sec = 9;
	CHECK( CTimeText( when ) == "Fri Sep  4 07:05:09 2026" );
}

void TestSaveList() {
	// The drawn list's columns (0x0041a150): 40, 20 and 40 per cent, cut to whole units.
	static_assert( SaveColumns == 3, "Location, elapsed time and date" );
	CHECK( std::string( SaveColumnTitles[1] ) == "Elapased Time" );
	CHECK( SaveColumnWidth( 0, 512.0f ) == 204 && SaveColumnWidth( 1, 512.0f ) == 102 && SaveColumnWidth( 2, 512.0f ) == 204 );
	CHECK( SaveHeaderHeight( 16 ) == 20 );
	// The level's seconds as hours, then two-digit minutes and seconds (0x0041aa3a).
	CHECK( ElapsedText( 0 ) == "0:00:00" && ElapsedText( 1999 ) == "0:00:01" );
	CHECK( ElapsedText( 3723000 ) == "1:02:03" && ElapsedText( 36000000 + 59000 ) == "10:00:59" );
	CHECK( ElapsedText( -5000 ) == "0:00:00" );
	// F.A.K.K.2's list: 160 high, a 16-unit line, 20 of header: eight whole rows.
	CHECK( SaveRowsPerPage( 160.0f, 16 ) == 8 && SaveRowsPerPage( 10.0f, 16 ) == 1 && SaveRowsPerPage( 160.0f, 0 ) == 1 );
	// The first row shown moves only as far as keeps the choice on the page.
	CHECK( SaveTopFor( 1, 0, 7, 10 ) == 0 && SaveTopFor( 8, 0, 7, 10 ) == 1 && SaveTopFor( 10, 0, 7, 10 ) == 3 );
	CHECK( SaveTopFor( 2, 3, 7, 10 ) == 1 && SaveTopFor( 0, 9, 7, 10 ) == 3 && SaveTopFor( 1, 5, 7, 3 ) == 0 );
	// A chosen row is the page's choice, which loadgame and removegame read.
	SavePage page;
	SelectSaveRow( page, 8 );
	CHECK( page.page == 1 && page.tab == 2 && page.Selected() == 8 );
	SelectSaveRow( page, 1 );
	CHECK( page.page == 0 && page.tab == 1 && page.Selected() == 1 );
	SelectSaveRow( page, 0 );
	CHECK( page.Selected() == 1 );
}

void TestScroll() {
	CHECK( EdgeAlpha( 100.0f, 122.0f, 280.0f ) == 1.0f );
	CHECK( Near( EdgeAlpha( 20.0f, 42.0f, 280.0f ), 0.5f ) );
	CHECK( EdgeAlpha( -1.0f, 21.0f, 280.0f ) == 0.0f );
	CHECK( Near( EdgeAlpha( 100.0f, 270.0f, 280.0f ), 0.25f ) );
	CHECK( EdgeAlpha( 100.0f, 290.0f, 280.0f ) == 0.0f );
	// The bottom's rule wins when both edges are near.
	CHECK( Near( EdgeAlpha( 20.0f, 30.0f, 40.0f ), 0.25f ) );
	CHECK( ResetScrollPosition( 161.0f, 22.0f ) == -3542.0f && ResetScrollPosition( 0.0f, 22.0f ) == -0.0f );
	static_assert( DefaultScrollSpeed == float( 0.2f ), "UIAutoScroll's default speed (0x004c0393)" );
}

/* The composite a set of kept shares draws back to, one grey value a layer. */
float Composite( const std::vector<KeptLayer> &layers, const std::vector<float> &values ) {
	const std::vector<float> alpha = KeptAlphas( layers );
	float out = 0.0f;
	for ( std::size_t i = 0; i < layers.size(); ++i ) out = alpha[i] * values[i] + ( 1.0f - alpha[i] ) * out;
	return out;
}

/* F.A.K.K.2's uncleared screen under its menus, as shares. */
void TestKeptImage() {
	enum { Black, Fill, Art, OtherArt };
	std::vector<KeptLayer> layers;
	KeepDraw( layers, Black, -1, 1.0f );
	CHECK( layers.size() == 1 && layers[0].share == 1.0f );
	// A menu shown whole leaves its fill under its pictures: only they show.
	KeepDraw( layers, Fill, 0, 1.0f );
	KeepDraw( layers, Art, 0, 1.0f );
	CHECK( layers[0].share == 0.0f && layers[1].share == 0.0f && layers[2].share == 1.0f );
	DropFaintLayers( layers );
	CHECK( layers.size() == 1 && layers[0].kind == Art );
	// Fading at a steady alpha f, the pictures keep 1 / (2 - f) and the white
	// fill (1 - f) / (2 - f): a going menu hazes, never darkens.
	for ( int frame = 0; frame < 200; ++frame ) {
		KeepDraw( layers, Fill, 0, 0.2f );
		KeepDraw( layers, Art, 0, 0.2f );
	}
	float art = 0.0f, fill = 0.0f;
	for ( const KeptLayer &layer : layers ) ( layer.kind == Art ? art : fill ) += layer.share;
	CHECK( Near( art, 1.0f / 1.8f ) && Near( fill, 0.8f / 1.8f ) );
	// The next menu's pictures cover the last one's share by share.
	KeepDraw( layers, OtherArt, 1, 0.5f );
	float shares = 0.0f;
	for ( const KeptLayer &layer : layers ) shares += layer.share;
	CHECK( Near( shares, 1.0f ) );
	// Drawn back from the lowest up, the layers leave each its share.
	const std::vector<float> values = { 0.9f, 0.3f, 0.6f };
	float expected = 0.0f;
	for ( std::size_t i = 0; i < layers.size(); ++i ) expected += layers[i].share * values[i];
	CHECK( Near( Composite( layers, values ), expected ) );
	CHECK( KeptAlphas( layers ).back() == 0.5f );
}

/* The retail bind.scr and keybind.scr, when an installation is here. */
void TestRetailFiles() {
	fntech3_tests::AlicePak pak;
	if ( !pak.Available() ) {
		std::cout << "alice_menu_tests: retail files skipped, no installation at " << pak.Path() << "\n";
		return;
	}
	std::string bind, keys;
	for ( const fntech3_tests::PakEntry &entry : pak.Entries() ) {
		if ( entry.name == "ui/bind.scr" ) CHECK( pak.Read( entry, bind ) );
		if ( entry.name == "ui/keybind.scr" ) CHECK( pak.Read( entry, keys ) );
	}
	CHECK( !bind.empty() && !keys.empty() );
	const BindLayout layout = ParseBindScript( bind.data(), bind.size() );
	CHECK( layout.items.size() == 29 );
	CHECK( layout.width == 230 && layout.height == 300 && layout.fillWidth == 16 );
	CHECK( layout.commandWidth == 145 && layout.commandHeight == 22 );
	CHECK( layout.primaryKeyWidth == 68 && layout.primaryKeyHeight == 22 );
	CHECK( layout.alternateKeyWidth == 64 && layout.alternateKeyHeight == 16 );
	CHECK( layout.inactiveForeground.r == 0.0f && layout.inactiveForeground.a == 1.0f );
	CHECK( layout.selectForeground.r == 1.0f && layout.enterSound == "sound/misc/menu/select.wav" );
	CHECK( layout.items[0].name == "Primary Attack" && layout.items[0].command == "+attackright" );
	CHECK( layout.items[13].command == "cheshire" && layout.items[26].command == "loadgame quicksave" );
	CHECK( layout.items[23].name == "Equip Blunderbuss" && layout.items[23].anim.empty() );
	CHECK( layout.items[6].rotation == -45.0f && layout.items[6].loop );
	std::string error;
	const std::vector<KeyAlias> aliases = ParseKeyAliases( keys.data(), keys.size(), &error );
	CHECK( error.empty() && aliases.size() == 27 );
	CHECK( FindKeyAlias( aliases, "MWHEELUP" )->name == "WHEEL UP" && FindKeyAlias( aliases, "CAPSLOCK" )->scale == float( 0.8f ) );
	CHECK( FindKeyAlias( aliases, "UPARROW" )->scale == -1.0f );
	std::cout << "alice_menu_tests: retail bind.scr and keybind.scr read\n";
}

} // namespace

int main() {
	TestColours();
	TestCheckBox();
	TestSlider();
	TestList();
	TestField();
	TestBindScript();
	TestSaves();
	TestSaveList();
	TestScroll();
	TestKeptImage();
	TestRetailFiles();
	std::cout << "alice_menu_tests: ok\n";
	return 0;
}
