/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's HUD (code/compat/alice_hud.hpp): AliceHud's eight children, the
 * riser's ease, and the weapon and item foldouts' state machines, each
 * pinned to the retail address the header names. When an installation is
 * present its pak0.pk3 is also read, read-only, to check that every model
 * the HUD can name is shipped and that both fold animations run 0 to 100. */

#include "../code/compat/alice_hud.hpp"
#include "../code/compat/alice_tan_format.hpp"
#include "alice_pak_reader.hpp"

#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#ifdef _MSC_VER
// Several checks pin constexpr tables, whose conditions are constant.
#pragma warning( disable : 4127 )
#endif

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::hud;

namespace {

bool Near( float a, float b, float epsilon = 1e-3f ) { return std::fabs( a - b ) < epsilon; }

void TestWidgets() {
	CHECK( sizeof( Widgets ) / sizeof( Widgets[0] ) == 8 );
	// Creation order: back, riser, weapon foldout, bar, item foldout, then mana.
	CHECK( Widgets[0].kind == WidgetKind::Still && Widgets[1].kind == WidgetKind::Riser );
	CHECK( Widgets[2].kind == WidgetKind::WeaponFoldout && Widgets[3].kind == WidgetKind::Still );
	CHECK( Widgets[4].kind == WidgetKind::ItemFoldout && Widgets[6].kind == WidgetKind::Riser );
	CHECK( Widgets[1].stat == 0 && Widgets[6].stat == 8 );
	CHECK( Widgets[2].offsetX == -275.0f && Widgets[2].offsetY == 182.0f && Widgets[2].scale == 6.0f );
	CHECK( Widgets[4].offsetX == 275.0f && Widgets[4].offsetY == 182.0f && Widgets[4].scale == 6.0f );
	CHECK( !std::strcmp( Widgets[2].model, "models/hud_foldout_knife.tik" ) );
	CHECK( !std::strcmp( Widgets[4].model, "models/hud_itemfoldout.tik" ) );
	CHECK( Widgets[0].offsetX == -312.0f && Widgets[7].offsetX == 312.0f && Widgets[7].offsetY == 42.0f );
	CHECK( Widgets[5].scale == 2.45f && Widgets[7].scale == 1.2f );
	CHECK( BaseYaw == 180.0f );
}

void TestRiser() {
	// The constructor's target of 100 shows a full gauge at once.
	Riser riser;
	CHECK( riser.target == 100 && riser.shown == 0 && riser.timer == 0 );
	CHECK( RiserFrame( riser, 100, 16 ) == 0 );
	CHECK( riser.shown == 100 && riser.from == 100 );

	// A drop to 80 eases over 500 ms from the value shown.
	CHECK( RiserFrame( riser, 80, 250 ) == static_cast<int>( ( 100 - 90 ) * 0.98f ) );
	CHECK( riser.shown == 90 && riser.timer == 250 );
	CHECK( RiserFrame( riser, 80, 250 ) == static_cast<int>( 20 * 0.98f ) );
	CHECK( riser.shown == 80 );
	// The timer is written only while time is left: a spent ease stays spent.
	CHECK( riser.timer == 250 );
	CHECK( RiserFrame( riser, 80, 300 ) == 19 && riser.shown == 80 );

	// Past 100 draws frame 0; below zero is held at 100.
	Riser over;
	CHECK( RiserFrame( over, 150, 1000 ) == 0 );
	Riser under;
	CHECK( RiserFrame( under, -50, 1000 ) == 100 );
}

void TestFold() {
	CHECK( FoldMs == 200 && FoldHalf == 50.0f && FoldMaxFrame == 100.0f );
	CHECK( Near( FoldFrames( 200 ), 50.0f ) );
	CHECK( Near( FoldFrames( 100 ), 25.0f ) );
	CHECK( FoldFrames( -16 ) == 0.0f );
}

void TestWeaponTable() {
	CHECK( FoldoutWeaponCount == 10 );
	CHECK( FoldoutWeaponIndex( "knife" ) == 0 );
	CHECK( FoldoutWeaponIndex( "KNIFE" ) == 0 );            // case-blind
	CHECK( FoldoutWeaponIndex( "DemonDice" ) == DemonDiceWeapon );
	CHECK( FoldoutWeaponIndex( "watch" ) == 9 );
	CHECK( FoldoutWeaponIndex( "knife2" ) == -1 );
	CHECK( FoldoutWeaponIndex( "" ) == -1 );
	CHECK( FoldoutWeaponIndex( nullptr ) == -1 );
	CHECK( !std::strcmp( FoldoutWeaponModel( 9 ), "models/hud_foldout_watchweapon.tik" ) );
	CHECK( FoldoutWeaponModel( -1 ) == nullptr && FoldoutWeaponModel( 10 ) == nullptr );
	CHECK( DiceModel( 2 ) == "models/hud_foldout_dice2.tik" );
	CHECK( DiceModel( 1 ) == FoldoutWeapons[DemonDiceWeapon].model );
	CHECK( CompareNoCase( "a", "B" ) < 0 && CompareNoCase( "b", "A" ) > 0 && !CompareNoCase( "Ab", "aB" ) );

	int inventory[32] = {};
	inventory[4] = 17;
	CHECK( InventoryHolds( inventory, 17 ) && InventoryHolds( inventory, 0 ) && !InventoryHolds( inventory, 18 ) );

	WeaponRequest request = RequestWeapon( -1, "knife" );
	CHECK( request.index == -1 && !request.named );
	// An item without a name stands in for itself, as retail's register does.
	request = RequestWeapon( 5, nullptr );
	CHECK( request.index == 5 && !request.named );
	request = RequestWeapon( 5, "mallet" );
	CHECK( request.index == 2 && request.named );
	request = RequestWeapon( 5, "" );
	CHECK( request.index == -1 && request.named );
}

WeaponRequest Named( int index ) {
	WeaponRequest request;
	request.index = index;
	request.named = index != -1;
	return request;
}

/* Steps until the fold is idle again, returning the frames drawn. */
std::vector<int> RunFold( WeaponFoldout &f, const WeaponRequest &request, const int *dice, int ms ) {
	std::vector<int> frames;
	for ( int guard = 0; guard < 100; ++guard ) {
		StepWeaponFoldout( f, request, dice, ms );
		frames.push_back( static_cast<int>( f.frame ) );
		if ( f.fold == Fold::Idle ) break;
	}
	return frames;
}

void TestWeaponFoldout() {
	const int noDice = 0;

	// Built showing the knife, unfolded.
	WeaponFoldout f;
	CHECK( f.current == 0 && f.open && f.swapped && f.fold == Fold::Idle && f.frame == 0.0f );
	CHECK( f.model == "models/hud_foldout_knife.tik" );
	StepWeaponFoldout( f, Named( 0 ), &noDice, 16 );
	CHECK( f.fold == Fold::Idle && f.frame == 0.0f && f.model == "models/hud_foldout_knife.tik" );

	// A different weapon swaps the card at once and keeps the frame.
	StepWeaponFoldout( f, Named( 2 ), &noDice, 16 );
	CHECK( f.current == 2 && f.fold == Fold::Idle && f.model == "models/hud_foldout_mallet.tik" && f.frame == 0.0f );

	// Losing the weapon folds the card shut: 100 down to 50 over 200 ms.
	StepWeaponFoldout( f, Named( -1 ), &noDice, 16 );
	CHECK( f.fold == Fold::Closing && f.current == -1 && f.timer == 184 );
	CHECK( Near( f.frame, 96.0f ) );
	std::vector<int> frames = RunFold( f, Named( -1 ), &noDice, 16 );
	CHECK( frames.back() == 50 && f.fold == Fold::Idle && !f.open );
	CHECK( f.model == "models/hud_foldout_mallet.tik" );    // the old card folds away
	StepWeaponFoldout( f, Named( -1 ), &noDice, 16 );
	CHECK( f.fold == Fold::Idle && f.frame == 50.0f );

	// A weapon arriving at the shut card unfolds it, 50 down to 0, and the
	// new card goes up as the fold passes below 50 -- on its first frame.
	StepWeaponFoldout( f, Named( 8 ), &noDice, 16 );
	CHECK( f.fold == Fold::Opening && f.current == 8 && f.swapped );
	CHECK( Near( f.frame, 46.0f ) && f.model == "models/hud_foldout_blunderbuss.tik" );
	frames = RunFold( f, Named( 8 ), &noDice, 16 );
	CHECK( frames.back() == 0 && f.fold == Fold::Idle && f.open );

	// A weapon without a name does not unfold a shut card.
	WeaponFoldout shut;
	shut.current = -1;
	shut.open = false;
	shut.frame = 50.0f;
	WeaponRequest unnamed;
	unnamed.index = 4;
	StepWeaponFoldout( shut, unnamed, &noDice, 16 );
	CHECK( shut.fold == Fold::Idle && shut.current == 4 && shut.frame == 50.0f );

	// The demon dice's card counts the dice.
	const int two = 2, three = 3;
	StepWeaponFoldout( f, Named( DemonDiceWeapon ), &two, 16 );
	CHECK( f.current == DemonDiceWeapon && f.model == "models/hud_foldout_dice2.tik" && f.lastDice == 2 );
	StepWeaponFoldout( f, Named( DemonDiceWeapon ), &three, 16 );
	CHECK( f.model == "models/hud_foldout_dice3.tik" && f.lastDice == 3 );
	StepWeaponFoldout( f, Named( DemonDiceWeapon ), &three, 16 );
	CHECK( f.model == "models/hud_foldout_dice3.tik" );
	// No count keeps the table's card; no block reads no count.
	WeaponFoldout plain;
	StepWeaponFoldout( plain, Named( DemonDiceWeapon ), &noDice, 16 );
	CHECK( plain.model == "models/hud_foldout_dice1.tik" );
	WeaponFoldout blockless;
	StepWeaponFoldout( blockless, Named( DemonDiceWeapon ), nullptr, 16 );
	CHECK( blockless.model == "models/hud_foldout_dice1.tik" );

	// Retail's own path: a named weapon arriving while the card is open with
	// nothing shown folds it shut, and it stays shut on that weapon.
	WeaponFoldout empty;
	empty.current = -1;
	StepWeaponFoldout( empty, Named( 1 ), &noDice, 16 );
	CHECK( empty.fold == Fold::Closing && empty.current == 1 );
	RunFold( empty, Named( 1 ), &noDice, 16 );
	CHECK( !empty.open && static_cast<int>( empty.frame ) == 50 );
	StepWeaponFoldout( empty, Named( 1 ), &noDice, 16 );
	CHECK( empty.fold == Fold::Idle && !empty.open );

	// A nameless item index past the table is kept but puts no card up.
	WeaponFoldout past;
	WeaponRequest raw;
	raw.index = 40;
	StepWeaponFoldout( past, raw, &noDice, 16 );
	CHECK( past.current == 40 && past.model == "models/hud_foldout_knife.tik" );
	raw.index = 5;
	StepWeaponFoldout( past, raw, &noDice, 16 );
	CHECK( past.current == 5 && past.model == "models/hud_foldout_icewand.tik" );

	// The wobble: a degree either way about 180.
	CHECK( WeaponFoldoutYaw( 0 ) == 180.0f );
	CHECK( Near( WeaponFoldoutYaw( 523 ), 181.0f, 1e-3f ) );
	CHECK( Near( WeaponFoldoutYaw( 1569 ), 179.0f, 1e-3f ) );
}

void TestSharedBlock() {
	unsigned char block[60] = {};
	const float time = 12.5f;
	const int one = 1, two = 2, sixteen = 16, three = 3;
	std::memcpy( block + 0x20, &time, 4 );
	std::memcpy( block + 0x28, &one, 4 );
	std::memcpy( block + 0x2c, &two, 4 );
	std::memcpy( block + 0x30, &sixteen, 4 );
	std::memcpy( block + 0x34, &one, 4 );
	std::memcpy( block + 0x38, &three, 4 );
	const PowerupState state = ReadPowerupState( block );
	CHECK( state.time == 12.5f && state.tea == 1 && state.rageBox == 2 && state.glass == 16 );
	CHECK( state.watch == 1 && state.dice == 3 );
	CHECK( ReadPowerupState( nullptr ).dice == 0 );
	CHECK( DiceOffset + 4 <= 60 );
}

void TestItemFoldout() {
	PowerupState none;
	ItemFoldout f;
	CHECK( f.state == ItemFold::Hidden && !std::strcmp( f.model, "models/hud_itemfoldout.tik" ) );
	CHECK( !StepItemFoldout( f, &none, 16 ) );
	CHECK( !StepItemFoldout( f, nullptr, 16 ) );

	// Priority: the rage box, the glass, the tea, then the watch.
	PowerupState all;
	all.tea = 1; all.rageBox = 2; all.glass = 16; all.watch = 1;
	CHECK( ActivePowerup( all ) == Powerup::RageBox );
	all.rageBox = 0;
	CHECK( ActivePowerup( all ) == Powerup::Glass );
	all.glass = 0;
	CHECK( ActivePowerup( all ) == Powerup::Tea );
	all.tea = 0;
	CHECK( ActivePowerup( all ) == Powerup::Watch );

	// A rage box unfolds its card over 200 ms, 50 down to 0.
	PowerupState rage;
	rage.rageBox = 2;
	rage.time = 13000.0f;
	CHECK( StepItemFoldout( f, &rage, 16 ) );
	CHECK( f.state == ItemFold::Opening && f.kind == Powerup::RageBox && f.total == 13000 );
	CHECK( !std::strcmp( f.model, "models/hud_foldout_ragebox.tik" ) && Near( f.frame, 46.0f ) );
	int guard = 0;
	while ( f.state == ItemFold::Opening && guard++ < 100 ) {
		CHECK( StepItemFoldout( f, &rage, 16 ) );
	}
	// The step that ends the unfold draws at 0 and starts timing.
	CHECK( f.state == ItemFold::Timing && f.timer == 1 && f.frame == 0.0f );

	// Timing: a full timer at 100, half at 82.5, and gone at zero.
	CHECK( StepItemFoldout( f, &rage, 16 ) && f.frame == 100.0f );
	rage.time = 6500.0f;
	CHECK( StepItemFoldout( f, &rage, 16 ) && Near( f.frame, 82.5f ) );
	CHECK( f.timer == 1 );                               // never counted down
	rage.time = 1.0f;
	CHECK( StepItemFoldout( f, &rage, 16 ) && f.frame >= 65.0f && f.frame < 65.01f );

	// A different powerup opens its own card.
	PowerupState glass;
	glass.glass = 16;
	glass.time = 5000.0f;
	CHECK( StepItemFoldout( f, &glass, 16 ) );
	CHECK( f.state == ItemFold::Opening && f.kind == Powerup::Glass && f.total == 5000 );
	CHECK( !std::strcmp( f.model, "models/hud_foldout_glass.tik" ) );

	// Everything off while unfolding: the card times, reads zero and goes.
	CHECK( !StepItemFoldout( f, &none, 16 ) );
	CHECK( f.state == ItemFold::Hidden );
	CHECK( !StepItemFoldout( f, &none, 16 ) );

	// Everything off while timing: gone at once.
	PowerupState tea;
	tea.tea = 1;
	tea.time = 8000.0f;
	ItemFoldout t;
	for ( int i = 0; i < 20; ++i ) StepItemFoldout( t, &tea, 16 );
	CHECK( t.state == ItemFold::Timing && !std::strcmp( t.model, "models/hud_foldout_tea.tik" ) );
	CHECK( !StepItemFoldout( t, &none, 16 ) && t.state == ItemFold::Hidden );

	// The watch's time is seconds; under one second truncates to a zero
	// total, which divides to an infinity and holds the card at 100.
	PowerupState watch;
	watch.watch = 1;
	watch.time = 0.5f;
	ItemFoldout w;
	for ( int i = 0; i < 20; ++i ) StepItemFoldout( w, &watch, 16 );
	CHECK( w.state == ItemFold::Timing && w.total == 0 );
	CHECK( !std::strcmp( w.model, "models/hud_foldout_watch.tik" ) );
	CHECK( StepItemFoldout( w, &watch, 16 ) && w.frame == 100.0f );
	CHECK( std::isnan( TimerFraction( 0.0f, 0 ) ) && std::isinf( TimerFraction( -1.0f, 0 ) ) );
	CHECK( TimerFraction( 3.0f, 6 ) == 0.5 );
	// A running powerup whose time reads zero goes, then opens again.
	watch.time = 0.0f;
	CHECK( !StepItemFoldout( w, &watch, 16 ) && w.state == ItemFold::Hidden );
	CHECK( StepItemFoldout( w, &watch, 16 ) && w.state == ItemFold::Opening );
}

void TestRetailAssets() {
	fntech3_tests::AlicePak pak;
	if ( !pak.Available() ) {
		std::cout << "retail HUD models: skipped, no installation at " << pak.Path() << "\n";
		return;
	}
	std::set<std::string> names;
	for ( const fntech3_tests::PakEntry &entry : pak.Entries() ) {
		names.insert( fntech3_tests::Lower( entry.name ) );
	}
	std::vector<std::string> models;
	for ( const Widget &widget : Widgets ) models.push_back( widget.model );
	for ( const FoldoutWeapon &weapon : FoldoutWeapons ) models.push_back( weapon.model );
	for ( int count = 1; count <= 3; ++count ) models.push_back( DiceModel( count ) );
	for ( Powerup p : { Powerup::Watch, Powerup::RageBox, Powerup::Tea, Powerup::Glass } ) {
		models.push_back( PowerupModel( p ) );
	}
	for ( const std::string &model : models ) {
		CHECK( names.count( model ) );
	}

	// Both fold animations are 101 frames: 0 open, 50 shut, 100 open.
	int checked = 0;
	for ( const fntech3_tests::PakEntry &entry : pak.Entries() ) {
		const std::string name = fntech3_tests::Lower( entry.name );
		if ( name != "models/ui/hud/foldout/folding.tan" && name != "models/ui/hud/itemfoldout/folding.tan" ) {
			continue;
		}
		std::string data;
		CHECK( pak.Read( entry, data ) );
		fntech3::alice::model::Tan tan;
		CHECK( fntech3::alice::model::ParseTan( data.data(), data.size(), tan ) == fntech3::alice::model::Error::None );
		CHECK( tan.frames == 101 );
		++checked;
	}
	CHECK( checked == 2 );
	std::cout << "retail HUD models: " << models.size() << " names, both fold animations 101 frames\n";
}

} // namespace

int main() {
	TestWidgets();
	TestRiser();
	TestFold();
	TestWeaponTable();
	TestWeaponFoldout();
	TestSharedBlock();
	TestItemFoldout();
	TestRetailAssets();
	std::cout << "alice hud tests passed\n";
	return 0;
}
