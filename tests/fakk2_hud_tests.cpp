/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Heavy Metal: F.A.K.K.2's HUD labels and inventory (code/compat/fakk2_hud.hpp,
 * code/compat/fakk2_inventory.hpp): the stat bar's fraction, flash and quad,
 * the HUD model's camera, which HUD menus show, the inventory script's grammar
 * and checks, the selection's steps and the commands an item or a weapon group
 * sends, each pinned to the retail address the headers name. When an
 * installation is present its fakk/pak0.pk3 is read, read-only, and the
 * shipped global/inventory.scr must load. Set FNTECH3_FAKK2_ROOT to an
 * installation root to override the default of E:/Games/Heavy Metal FAKK2. */

#include "../code/compat/fakk2_inventory.hpp"
#include "alice_pak_reader.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace hud = fntech3::fakk2::hud;
namespace inv = fntech3::fakk2::inventory;

namespace {

bool Near( float a, float b, float epsilon = 1e-4f ) { return std::fabs( a - b ) < epsilon; }

// The slots the retail game module stores (fakk/gamex86.dll), and the
// constants the headers pin.
static_assert( hud::StatBossHealth == 13 && hud::StatCinematic == 14, "boss and cinematic stats" );
static_assert( hud::StatAddFade == 15 && hud::StatLetterbox == 16, "fade and letterbox stats" );
static_assert( hud::ItemWeaponLeft == 2 && hud::ItemWeaponRight == 3 && hud::ItemWeaponDual == 4, "weapon slots" );
static_assert( hud::FlashRate == 0.00133333332f, "the flash fades over 750 ms (0x004ff964)" );
static_assert( hud::MoveRate == 0.00300300308, "bob and spin run at 1/333 a ms (0x004ff468)" );

void TestSlots() {
	int stats[32] = {};
	stats[31] = 7;
	CHECK( hud::ReadSlot( stats, 31 ) == 7 );
	CHECK( hud::ReadSlot( stats, 32 ) == 0 && hud::ReadSlot( stats, -2 ) == 0 );
	CHECK( hud::ReadSlot( nullptr, 0 ) == 0 );
}

void TestFraction() {
	int stats[32] = {};
	hud::Label health;
	health.stat = 0;
	health.orientation = hud::Orientation::Vertical;
	health.min = 0.0f;
	health.max = 100.0f;
	stats[0] = 70;
	CHECK( Near( hud::StatFraction( health, stats ), 0.7f ) );
	stats[0] = 250;
	CHECK( hud::StatFraction( health, stats ) == 1.0f );
	stats[0] = -5;
	CHECK( hud::StatFraction( health, stats ) == 0.0f );

	// The minimum is never taken from the value: 25 over 100 - 50 is a half.
	hud::Label shifted = health;
	shifted.min = 50.0f;
	stats[0] = 25;
	CHECK( Near( hud::StatFraction( shifted, stats ), 0.5f ) );

	// A max stat names the range; 0 over 0 (no weapon) is empty.
	hud::Label ammo;
	ammo.stat = 2;
	ammo.maxStat = 7;
	ammo.orientation = hud::Orientation::Vertical;
	stats[2] = 30;
	stats[7] = 120;
	CHECK( Near( hud::StatFraction( ammo, stats ), 0.25f ) );
	stats[2] = 0;
	stats[7] = 0;
	CHECK( hud::StatFraction( ammo, stats ) == 0.0f );
	stats[2] = 5;
	CHECK( hud::StatFraction( ammo, stats ) == 1.0f );

	// With no range at all (-1 - -1) a stat above zero is a full bar.
	hud::Label bare;
	bare.stat = 3;
	stats[3] = 4;
	CHECK( hud::StatFraction( bare, stats ) == 1.0f );
	stats[3] = 0;
	CHECK( hud::StatFraction( bare, stats ) == 0.0f );
}

void TestFlash() {
	hud::Flash flash;
	// The first bar flashes; the flash fades over 750 ms.
	CHECK( hud::StepFlash( flash, 0.7f, 1000.0f ) == 1.0f );
	CHECK( Near( hud::StepFlash( flash, 0.7f, 1375.0f ), 0.5f, 1e-3f ) );
	CHECK( hud::StepFlash( flash, 0.7f, 1750.0f ) == 0.0f );
	CHECK( hud::StepFlash( flash, 0.7f, 5000.0f ) == 0.0f );
	// Any change starts it again, up or down.
	CHECK( hud::StepFlash( flash, 0.6f, 6000.0f ) == 1.0f );
	CHECK( Near( hud::StepFlash( flash, 0.6f, 6075.0f ), 0.9f, 1e-3f ) );
	CHECK( hud::StepFlash( flash, 0.8f, 6100.0f ) == 1.0f );
}

void TestBarQuad() {
	// healthhud's bar: 70 of 100 in a 64 by 72 label over a 64 by 128 image,
	// filled from the bottom with the texture fixed.
	hud::Quad q = hud::BarQuad( hud::Orientation::Vertical, true, 0.7f, 64.0f, 72.0f, 64.0f, 128.0f );
	CHECK( q.draw );
	CHECK( Near( q.x, 0.0f ) && Near( q.y, 21.6f ) && Near( q.w, 64.0f ) && Near( q.h, 50.4f ) );
	CHECK( Near( q.s0, 0.0f ) && Near( q.s1, 1.0f ) );
	CHECK( Near( q.t0, 0.16875f ) && Near( q.t1, 0.5625f ) );

	// Tiled horizontal: from the left, the texture fixed.
	q = hud::BarQuad( hud::Orientation::Horizontal, true, 0.5f, 128.0f, 16.0f, 32.0f, 16.0f );
	CHECK( q.draw && Near( q.w, 64.0f ) && Near( q.h, 16.0f ) && Near( q.s1, 2.0f ) && Near( q.t1, 1.0f ) );

	// bosshud's stretched horizontal bar squeezes the whole image.
	q = hud::BarQuad( hud::Orientation::Horizontal, false, 0.25f, 128.0f, 16.0f, 128.0f, 16.0f );
	CHECK( q.draw && Near( q.w, 32.0f ) && q.s0 == 0.0f && q.s1 == 1.0f && q.t1 == 1.0f );

	// Retail's stretched vertical bar is drawn at f h: full shows nothing.
	q = hud::BarQuad( hud::Orientation::Vertical, false, 1.0f, 32.0f, 32.0f, 32.0f, 32.0f );
	CHECK( !q.draw );
	q = hud::BarQuad( hud::Orientation::Vertical, false, 0.25f, 32.0f, 16.0f, 32.0f, 32.0f );
	CHECK( q.draw && Near( q.y, 4.0f ) && Near( q.h, 12.0f ) && q.t0 == 0.0f && Near( q.t1, 0.75f ) );

	// No orientation, or nothing to fill, draws nothing.
	CHECK( !hud::BarQuad( hud::Orientation::None, true, 0.5f, 32.0f, 32.0f, 32.0f, 32.0f ).draw );
	CHECK( !hud::BarQuad( hud::Orientation::Vertical, true, 0.0f, 32.0f, 32.0f, 32.0f, 32.0f ).draw );

	// Without a material: a box from the left, whatever the orientation.
	q = hud::FallbackBox( 0.5f, 64.0f, 72.0f );
	CHECK( q.draw && q.x == 0.0f && q.y == 0.0f && Near( q.w, 32.0f ) && Near( q.h, 72.0f ) );
	CHECK( !hud::FallbackBox( 0.0f, 64.0f, 72.0f ).draw );
}

void TestModel() {
	// The largest extent sets the distance, the later axis on a tie.
	const float mins[3] = { -4.0f, -10.0f, -2.0f };
	const float maxs[3] = { 4.0f, 10.0f, 18.0f };
	hud::Vec3 o = hud::ModelOrigin( mins, maxs, 1.0f );
	CHECK( Near( o.x, 20.0f * 1.86567163f, 1e-3f ) );
	CHECK( Near( o.y, 0.0f ) && Near( o.z, -8.0f ) );
	// A larger scale moves the camera away, so the model is smaller.
	CHECK( hud::ModelOrigin( mins, maxs, 2.0f ).x > o.x );

	hud::View view;
	view.angles = { 0.0f, 40.0f, -25.0f };
	view.angleDeltas = { 0.0f, 30.0f, 0.0f };
	view.move = hud::Move::None;
	hud::Vec3 a = hud::MovedAngles( view, 123456.0 );
	CHECK( a.x == 0.0f && a.y == 40.0f && a.z == -25.0f );
	view.move = hud::Move::Bob;
	const double t = 1000.0;
	a = hud::MovedAngles( view, t );
	CHECK( Near( a.y, static_cast<float>( 40.0 + std::sin( t * hud::MoveRate ) * 30.0 ), 1e-3f ) );
	view.move = hud::Move::Spin;
	a = hud::MovedAngles( view, t );
	// 30 degrees of delta turns at 90.1 degrees a second.
	CHECK( Near( a.y, static_cast<float>( 40.0 + std::fmod( t * hud::MoveRate * 30.0, 360.0 ) ), 1e-3f ) );

	// The entity's origin: the offset plus the rotate offset along the axes.
	hud::View placed;
	placed.offset = { 1.0f, 2.0f, 3.0f };
	placed.rotateOffset = { -10.0f, 0.0f, 0.0f };
	const float axis[3][3] = { { 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	const hud::Vec3 origin{ 100.0f, 0.0f, 0.0f };
	const hud::Vec3 e = hud::EntityOrigin( origin, placed, axis );
	CHECK( Near( e.x, 101.0f ) && Near( e.y, -8.0f ) && Near( e.z, 3.0f ) );

	// The item is looked up again only in the second half of each second.
	CHECK( !hud::ItemLookupWindow( 1500 ) && hud::ItemLookupWindow( 1501 ) );
	CHECK( hud::ItemLookupWindow( 2999 ) && !hud::ItemLookupWindow( 3000 ) );
}

void TestSelection() {
	int stats[32] = {};
	int items[32];
	for ( int &item : items ) item = -1;
	// Empty hands still show the one-handed menu's two rings, as retail draws
	// them (fakk2.exe 1.02 0x0040b2d7).
	hud::Selection s = hud::Select( stats, items );
	CHECK( s.health && s.weapon && !s.dualWeapon && !s.boss );
	items[hud::ItemWeaponRight] = 3;
	s = hud::Select( stats, items );
	CHECK( s.weapon && !s.dualWeapon );
	items[hud::ItemWeaponDual] = 7;
	s = hud::Select( stats, items );
	CHECK( !s.weapon && s.dualWeapon );
	stats[hud::StatBossHealth] = 1;
	CHECK( hud::Select( stats, items ).boss );
	// The files' crossed names are the ones addressed.
	CHECK( std::string( hud::WeaponHud ) == "weaphud" && std::string( hud::DualWeaponHud ) == "dualweaphud" );
}

const char kScript[] =
	"inv_vert_offset 96\n"
	"inv_height 64\n"
	"inv_width 64 // the column\n"
	"inv_align right\n"
	"inv_cascade left\n"
	"inv_align middle\n"
	"inv_selectsound snd_inv_select\n"
	"typedef swords\n"
	"{\n"
	"button_shader textures/inv/inv_swords\n"
	"hover_shader textures/inv/inv_swords_selected\n"
	"sel_shader textures/inv/inv_swords_selected_2\n"
	"width 64\n"
	"height 48\n"
	"baroffsety -4\n"
	"item \"GreenSword\"\n"
	"equip right\n"
	"model models/weapon_sword.tik\n"
	"hudmove bob // bob or spin\n"
	"hudangledeltas \"0 30 0\"\n"
	"item \"Rocket Launcher\"\n"
	"height 64\n"
	"equip dual\n"
	"model models/weapon_rlauncher.tik\n"
	"hudmodel models/weapon_hud.tik\n"
	"modelwindow 0.2 0.2 .6 0.6\n"
	"selitem_shaderontop\n"
	"}\n"
	"typedef health\n"
	"button_shader a\n"
	"hover_shader b\n"
	"sel_shader c\n"
	"bg_tile textures/inv/bg\n"
	"width 64\n"
	"height 64\n"
	"item \"Health Vial\"\n"
	"equip use\n"
	"checkammo\n"
	"model models/item_healthvial.tik\n";

void TestLoad() {
	inv::Inventory inventory;
	inv::LoadResult result = inv::Load( kScript, sizeof( kScript ) - 1, inventory );
	CHECK( result.ok && result.error.empty() );
	CHECK( inventory.typeWidth == 64 && inventory.typeHeight == 64 && inventory.verticalOffset == 96 );
	CHECK( inventory.align == inv::Align::Right && inventory.cascade == inv::Cascade::Left );
	CHECK( inventory.selectSound == "snd_inv_select" );
	// An unknown side is a warning and keeps the value; a missing number too.
	CHECK( result.warnings.size() == 2 );
	CHECK( result.warnings[0] == "invlistener::Align: Invalid align side 'middle'" );
	CHECK( inventory.types.size() == 2 );
	const inv::Type &swords = inventory.types[0];
	CHECK( swords.name == "swords" && swords.items.size() == 2 && swords.hover == "textures/inv/inv_swords_selected" );
	const inv::Item &sword = swords.items[0];
	// The type's sizes are the defaults each item takes.
	CHECK( sword.width == 64 && sword.height == 48 && sword.barOffsetY == -4 && sword.barOffsetX == 0 );
	CHECK( sword.equip == inv::EquipRight );
	CHECK( sword.hudModel == "models/weapon_sword.tik" );   // filled from model
	CHECK( sword.hud.move == hud::Move::Bob && sword.hud.angleDeltas.y == 30.0f );
	CHECK( sword.view.angleDeltas.x == 45.0f );             // the block's default
	const inv::Item &rocket = swords.items[1];
	CHECK( rocket.name == "Rocket Launcher" && rocket.height == 64 && rocket.width == 64 );
	CHECK( rocket.equip == inv::EquipDual && rocket.hudModel == "models/weapon_hud.tik" );
	CHECK( rocket.selectedOnTop && Near( rocket.window[2], 0.6f ) );
	const inv::Type &health = inventory.types[1];
	CHECK( health.bgTile && health.bg == "textures/inv/bg" );
	CHECK( health.items[0].equip == inv::EquipUse && !health.items[0].checkAmmo );

	CHECK( inv::FindItem( inventory, "rocket launcher" ) == &rocket );
	CHECK( inv::FindItem( inventory, "Axe" ) == nullptr );
	CHECK( inv::FindType( inventory, "HEALTH" ) == 2 && inv::FindType( inventory, "guns" ) == 0 );

	// Errors empty the inventory and name the line.
	const char bad[] = "inv_width 64\ntypedef swords\nbogus 1\n";
	result = inv::Load( bad, sizeof( bad ) - 1, inventory );
	CHECK( !result.ok && result.error == "inv: error on line 3: invalid token" && inventory.types.empty() );
	const char equip[] = "typedef a\nitem x\nequip Left\n";
	result = inv::Load( equip, sizeof( equip ) - 1, inventory );
	CHECK( result.error == "inv: error on line 3: bad equip arguments" );
	const char noItem[] = "typedef a\nmodel m\n";
	result = inv::Load( noItem, sizeof( noItem ) - 1, inventory );
	CHECK( result.error == "inv: error on line 2: no current item" );
	const char noType[] = "item x\n";
	result = inv::Load( noType, sizeof( noType ) - 1, inventory );
	CHECK( result.error == "inv: error on line 1: no current type" );
	const char count[] = "inv_width 64 64\n";
	result = inv::Load( count, sizeof( count ) - 1, inventory );
	CHECK( result.error == "inv: error on line 1: bad arg count" );
	const char window[] = "typedef a\nitem x\nmodelwindow 0 0 1\n";
	result = inv::Load( window, sizeof( window ) - 1, inventory );
	CHECK( result.error == "inv: error on line 3: ModelWindow: bad arg count" );
	const char move[] = "typedef a\nitem x\nmove Bob\n";
	result = inv::Load( move, sizeof( move ) - 1, inventory );
	CHECK( result.error == "inv: error on line 3: bad move arguments" );

	// The checks after the last line.
	const char noModel[] = "typedef a\nbutton_shader b\nhover_shader h\nsel_shader s\nwidth 1\nheight 1\nitem x\n";
	result = inv::Load( noModel, sizeof( noModel ) - 1, inventory );
	CHECK( result.error == "inv: error after load: type a: item x: no model" && inventory.types.empty() );
	const char noHover[] = "typedef a\nbutton_shader b\n";
	result = inv::Load( noHover, sizeof( noHover ) - 1, inventory );
	CHECK( result.error == "inv: error after load: type a: no hover shader" );
	const char zero[] = "inv_width 0\ntypedef a\n";
	result = inv::Load( zero, sizeof( zero ) - 1, inventory );
	CHECK( result.error == "inv: error after load: type width/height not specified" );
	// No type at all loads nothing and says nothing.
	const char empty[] = "// nothing\ninv_width 64\n";
	result = inv::Load( empty, sizeof( empty ) - 1, inventory );
	CHECK( !result.ok && result.error.empty() && inventory.types.empty() );
	result = inv::Load( "", 0, inventory );
	CHECK( !result.ok && result.error.empty() );
}

void TestNavigation() {
	inv::Inventory inventory;
	inv::LoadResult result = inv::Load( kScript, sizeof( kScript ) - 1, inventory );
	CHECK( result.ok );
	std::vector<std::string> held;
	auto owned = [&held]( const inv::Item &item ) {
		for ( const std::string &name : held ) {
			if ( name == item.name ) return true;
		}
		return false;
	};

	// From nothing: the first type's first item, held or not.
	inv::Selection sel;
	CHECK( inv::ChangeItem( inventory, sel, 1, owned ) );
	CHECK( sel.type == 1 && sel.item == 1 );
	// Holding nothing more in this type: into the next type's first held
	// item, or its first item when none is held.
	CHECK( inv::ChangeItem( inventory, sel, 1, owned ) );
	CHECK( sel.type == 2 && sel.item == 1 );
	// Past the last type it wraps.
	held = { "Rocket Launcher" };
	CHECK( inv::ChangeItem( inventory, sel, 1, owned ) );
	CHECK( sel.type == 1 && sel.item == 2 );
	CHECK( inv::ChangeItem( inventory, sel, -1, owned ) && sel.type == 2 && sel.item == 1 );
	// A type with nothing picked picks its first item.
	sel = { 2, 0 };
	CHECK( !inv::ChangeItem( inventory, sel, -1, owned ) && sel.type == 2 && sel.item == 1 );

	CHECK( inv::CycleItem( inventory.types[0], 2, 1 ) == 1 );
	CHECK( inv::CycleItem( inventory.types[0], 1, -1 ) == 2 );
	CHECK( inv::StepType( inventory, 0, 1 ) == 1 );
	CHECK( inv::StepType( inventory, 2, 1 ) == 1 && inv::StepType( inventory, 1, -1 ) == 2 );
}

void TestCommands() {
	inv::Item item;
	item.name = "Crossbow";
	item.equip = inv::EquipLeft | inv::EquipRight;
	CHECK( inv::UseCommand( item, true ) == "use \"Crossbow\" lefthand" );
	CHECK( inv::UseCommand( item, false ) == "use \"Crossbow\" righthand" );
	item.name = "GreenSword";
	item.equip = inv::EquipRight;
	// A one-handed weapon used from the other hand is refused (0x0040a010).
	CHECK( inv::UseCommand( item, true ).empty() );
	CHECK( inv::UseCommand( item, false ) == "use \"GreenSword\" righthand" );
	item.name = "Rocket Launcher";
	item.equip = inv::EquipDual;
	CHECK( inv::UseCommand( item, false ) == "use \"Rocket Launcher\" dualhand" );
	CHECK( inv::UseCommand( item, true ) == "use \"Rocket Launcher\" dualhand" );
	item.name = "Health Vial";
	item.equip = inv::EquipUse;
	CHECK( inv::UseCommand( item, true ) == "use \"Health Vial\"" );
	item.equip = inv::EquipNone;
	CHECK( inv::UseCommand( item, true ).empty() );
	item.command = "dualwield Uzi DualUzi";
	CHECK( inv::UseCommand( item, true ) == "dualwield Uzi DualUzi" );

	CHECK( inv::WeaponGroupCommand( true, "Rocket Launcher", "", "" ) == "use \"Rocket Launcher\" dual" );
	CHECK( inv::WeaponGroupCommand( false, "", "Smallshield", "GreenSword" )
		== "dualwield \"Smallshield\" \"GreenSword\"" );
	CHECK( inv::WeaponGroupCommand( false, "", "Uzi", "" ) == "use \"Uzi\" left" );
	CHECK( inv::WeaponGroupCommand( false, "", "", "Uzi" ) == "use \"Uzi\" right" );
	CHECK( inv::WeaponGroupCommand( false, "", "", "" ) == "holster" );
}

void TestAmmoGauge() {
	// The retail ChainSword's gauge: a 27 by 27 box, the picture raised by 4.
	inv::Item item;
	item.ammo = "Gas";
	item.barShader = "textures/hud/in_grad_bottom";
	item.barWidth = 27;
	item.barHeight = 27;
	item.barOffsetY = -4;
	inv::Rect cell;
	cell.x = 512.0f;
	cell.y = 160.0f;
	cell.w = 64.0f;
	cell.h = 64.0f;

	// fakk2.exe 1.02's rule (FakkItemList::Draw 0x00409a22, DrawTilePicOffset
	// 0x00481dd0), over the 64 by 64 picture, in the list's first row.
	// Full: the whole box, 4 above the cell's foot, the picture sampled where
	// the box sat before the offset moved it.
	inv::Gauge g = inv::AmmoGauge( item, cell, 0.0f, 100, 100, 64.0f, 64.0f );
	CHECK( g.draw && g.rect.x == 512.0f && g.rect.w == 27.0f && g.rect.h == 27.0f );
	CHECK( Near( g.rect.y, 160.0f - 4.0f + 64.0f - 27.0f ) );
	CHECK( g.s0 == 0.0f && Near( g.s1, 27.0f / 64.0f ) && Near( g.t0, 37.0f / 64.0f ) && Near( g.t1, 1.0f ) );

	// Half: the lower half of the box, the picture held still.
	g = inv::AmmoGauge( item, cell, 0.0f, 50, 100, 64.0f, 64.0f );
	CHECK( g.draw && Near( g.rect.h, 13.5f ) && Near( g.rect.y + g.rect.h, 160.0f - 4.0f + 64.0f ) );
	CHECK( Near( g.t0, ( 37.0f + 13.5f ) / 64.0f ) && Near( g.t1, 1.0f ) && Near( g.s1, 27.0f / 64.0f ) );

	// A later row samples the picture a row further down; it repeats.
	g = inv::AmmoGauge( item, cell, 128.0f, 100, 100, 64.0f, 64.0f );
	CHECK( Near( g.t0, ( 128.0f + 37.0f ) / 64.0f ) && Near( g.t1, 3.0f ) );

	// More than the maximum is a full box; none, or no maximum, draws nothing.
	CHECK( Near( inv::AmmoGauge( item, cell, 0.0f, 150, 100, 64.0f, 64.0f ).rect.h, 27.0f ) );
	CHECK( !inv::AmmoGauge( item, cell, 0.0f, 0, 100, 64.0f, 64.0f ).draw );
	CHECK( !inv::AmmoGauge( item, cell, 0.0f, 5, 0, 64.0f, 64.0f ).draw );
	CHECK( !inv::AmmoGauge( item, cell, 0.0f, -3, 100, 64.0f, 64.0f ).draw );

	// A box larger than the cell is drawn as it is.
	inv::Item wide = item;
	wide.barWidth = 200;
	wide.barHeight = 90;
	g = inv::AmmoGauge( wide, cell, 0.0f, 1, 1, 64.0f, 64.0f );
	CHECK( g.draw && g.rect.w == 200.0f && g.rect.h == 90.0f && Near( g.s1, 200.0f / 64.0f ) );
	CHECK( Near( g.t0, -26.0f / 64.0f ) && Near( g.t1, 1.0f ) );

	// With neither size the gauge fills the cell's width from its foot.
	inv::Item unsized = item;
	unsized.barWidth = unsized.barHeight = 0;
	g = inv::AmmoGauge( unsized, cell, 0.0f, 1, 4, 64.0f, 64.0f );
	CHECK( g.draw && g.rect.w == 64.0f && Near( g.rect.h, 16.0f ) && Near( g.rect.y, 160.0f - 4.0f + 48.0f ) );
	CHECK( Near( g.s1, 1.0f ) && Near( g.t0, 48.0f / 64.0f ) );

	// Without a picture, an ammunition kind, a picture size or a height there
	// is no gauge; one size alone is a size.
	inv::Item bare = item;
	bare.barShader.clear();
	CHECK( !inv::AmmoGauge( bare, cell, 0.0f, 1, 1, 64.0f, 64.0f ).draw );
	bare = item;
	bare.ammo.clear();
	CHECK( !inv::AmmoGauge( bare, cell, 0.0f, 1, 1, 64.0f, 64.0f ).draw );
	CHECK( !inv::AmmoGauge( item, cell, 0.0f, 1, 1, 0.0f, 64.0f ).draw );
	bare = item;
	bare.barHeight = 0;
	CHECK( !inv::AmmoGauge( bare, cell, 0.0f, 1, 1, 64.0f, 64.0f ).draw );
}

void TestRetailScript() {
#ifdef _MSC_VER
#pragma warning( suppress : 4996 )
#endif
	const char *root = std::getenv( "FNTECH3_FAKK2_ROOT" );
	const std::string pakPath = std::string( root && root[0] ? root : "E:/Games/Heavy Metal FAKK2" ) + "/fakk/pak0.pk3";
	fntech3_tests::AlicePak pak( fntech3_tests::AlicePak::Path_{ pakPath } );
	if ( !pak.Available() ) {
		std::cout << "no Heavy Metal: F.A.K.K.2 installation at " << pakPath << "; retail script skipped\n";
		return;
	}
	std::string text;
	bool found = false;
	for ( const fntech3_tests::PakEntry &entry : pak.Entries() ) {
		if ( fntech3_tests::Lower( entry.name ) == "global/inventory.scr" ) {
			found = pak.Read( entry, text );
			break;
		}
	}
	CHECK( found );
	inv::Inventory inventory;
	const inv::LoadResult result = inv::Load( text.data(), text.size(), inventory );
	CHECK( result.ok );
	CHECK( result.warnings.empty() );
	const char *const types[] = { "swords", "defense", "guns", "explosives", "bigguns", "health" };
	const std::size_t counts[] = { 6, 5, 4, 2, 5, 3 };
	CHECK( inventory.types.size() == 6 );
	for ( std::size_t i = 0; i < 6; ++i ) {
		CHECK( inventory.types[i].name == types[i] );
		CHECK( inventory.types[i].items.size() == counts[i] );
		for ( const inv::Item &item : inventory.types[i].items ) {
			CHECK( item.width == 64 && item.height == 64 && !item.hudModel.empty() );
		}
	}
	CHECK( inventory.typeWidth == 64 && inventory.typeHeight == 64 && inventory.verticalOffset == 96 );
	CHECK( inventory.align == inv::Align::Right && inventory.cascade == inv::Cascade::Left );
	CHECK( inventory.changeSound == "snd_inv_change" );
	const inv::Item *dualUzi = inv::FindItem( inventory, "DualUzi" );
	// Its quoted command is one word, and the first word is what is kept.
	CHECK( dualUzi && dualUzi->command == "dualwield Uzi DualUzi" );
	CHECK( dualUzi->hudModel == "models/weapon_dualuzi.tik" && dualUzi->model == "models/weapon_2uzis.tik" );
	const inv::Item *flashbang = inv::FindItem( inventory, "Flashbang" );
	CHECK( flashbang && flashbang->checkAmmo && flashbang->ammo == "Flashbangs" );
	CHECK( flashbang->barWidth == 27 && flashbang->barHeight == 27 && flashbang->barOffsetY == -4 );
	// Every gauge in the script is the same arc picture in the same box.
	int gauges = 0;
	for ( const inv::Type &type : inventory.types ) {
		for ( const inv::Item &item : type.items ) {
			if ( item.barShader.empty() ) continue;
			++gauges;
			CHECK( item.barShader == "textures/hud/in_grad_bottom" && !item.ammo.empty() );
			CHECK( item.barWidth == 27 && item.barHeight == 27 && item.barOffsetY == -4 && item.barOffsetX == 0 );
		}
	}
	CHECK( gauges == 14 );
	const inv::Item *sword = inv::FindItem( inventory, "GreenSword" );
	CHECK( sword && sword->equip == inv::EquipRight && sword->selectedOnTop );
	CHECK( sword->hud.move == hud::Move::Bob && sword->hud.offset.x == -64.0f );
	CHECK( inv::UseCommand( *sword, true ).empty() );
	CHECK( inv::UseCommand( *sword, false ) == "use \"GreenSword\" righthand" );

	// The column sits at the right edge, 96 down, one type high per type; a
	// type's list opens to its left from the column's top.
	const inv::Rect column = inv::ColumnRect( inventory, 640.0f );
	CHECK( column.x == 576.0f && column.y == 96.0f && column.w == 64.0f && column.h == 384.0f );
	const inv::Rect list = inv::ListRect( inventory, inventory.types[0], column );
	CHECK( list.x == 512.0f && list.y == 96.0f && list.w == 64.0f && list.h == 384.0f );
}

} // namespace

int main() {
	TestSlots();
	TestFraction();
	TestFlash();
	TestBarQuad();
	TestModel();
	TestSelection();
	TestLoad();
	TestNavigation();
	TestCommands();
	TestAmmoGauge();
	TestRetailScript();
	std::cout << "fakk2 hud tests passed\n";
	return 0;
}
