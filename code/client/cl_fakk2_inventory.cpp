/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* Heavy Metal: F.A.K.K.2's inventory and weapon groups, which its engine's UI
 * owns. The script is global/inventory.scr (ui_inventoryfile; inv_restart
 * 0x0040eeb0); the grammar, the selection's steps and the commands an item
 * sends are code/compat/fakk2_inventory.hpp. The drawing is fakk2.exe 1.02's:
 * FakkInventory::Draw 0x004088c0 (the type column), FakkItemList::Draw
 * 0x00409950 (the cells, gauges and models), its model view 0x00407dd0, and
 * the renderer's DrawTilePicOffset 0x00481dd0 for the gauges. The rest is
 * alice.exe's, whose UI F.A.K.K.2's executable shares:
 *
 *   inventory          0x00408550   open with the cursor, the first type
 *   warpinv <type>     0x00408510   open on a type's first item
 *   nextinv, previnv   0x004084b0, 0x004084e0   the first type, then a step
 *   UI_DoInventory     0x00408570   refused in a cinematic or when open
 *   FakkInventory      0x004088e0   the type column: select 0x00408ea0, keys
 *                                   0x004091b0, press 0x00408d90, timeout
 *                                   0x00408d10 five seconds after the last
 *                                   change (0x00408ef2)
 *   FakkItemList       0x00409cc0   the type's items: draw 0x00409e30, hover
 *                                   0x0040a710, use 0x0040a1b0 and 0x0040a3c0
 *   UI_CloseInventory  0x004088b0   Escape, the timeout, a cinematic, a use
 *                                   from the keys
 *   saveweaps <n>      0x0040ffe0   the weapons in hand as alias weapgroup<n>
 *   UI_Render3DModel   0x00408210   a model in a view of its own
 *
 * The pop-up is drawn in the menus' 640 by 480 units, as this client's menus
 * are: retail places it in pixels (UI_DoInventory 0x00408649..0x0040868a), the
 * same at 640x480. The hand an item goes to is fakk2.exe 1.02's rule
 * (code/compat/fakk2_inventory.hpp). */

#include "client_alice_ui.hpp"
#include "../compat/alice_module_abi.h"
#include "../compat/fakk2_inventory.hpp"
#include "../compat/fakk2_module_abi.h"
#include "../qcommon/alice_tiki.h"

#include <cstring>
#include <string>

namespace {

namespace hud = fntech3::fakk2::hud;
namespace inv = fntech3::fakk2::inventory;

/* The master inventory (0x00531008). Retail opens a working copy of it whose
 * filter keeps every item (0x004084a0 is `mov al, 1; ret`), so the copy is
 * the master itself here. */
inv::Inventory master;
cvar_t *inventoryFile;
bool commandsRegistered;

/* The open pop-up: FakkInventory (s_main_inv, 0x0052fed4) and its list. */
struct {
	bool open = false;
	bool mouse = false;           // +0x1ec: opened by `inventory`
	inv::Selection sel;           // +0x1e4 and the list's +0x274
	int closeAt = -1;             // the pending _timeout, -1 for none
} pop;

constexpr int TimeoutMs = 5000;   // 5.0 s (0x40a00000 at 0x00408ef2)

int Now() { return cls.realtime; }

float ScaleX() { return cls.glconfig.vidWidth / 640.0f; }
float ScaleY() { return cls.glconfig.vidHeight / 480.0f; }

const int *Slots( int block ) { return cl.snap.ps.alice.block[block]; }

/* Owns an item (0x004071f0): one of the 32 inventory slots names it. */
bool Owns( const std::string &name ) {
	for ( int i = 0; i < 32; ++i ) {
		const int index = Slots( 2 )[i];
		if ( !index ) continue;
		const char *held = CL_RitualItemName( index );
		if ( held && !Q_stricmp( held, name.c_str() ) ) return true;
	}
	return false;
}

/* The ammunition of a kind (0x00407530): the first ammo slot naming it. */
void AmmoCount( const std::string &name, int &amount, int &max ) {
	amount = max = 0;
	for ( int i = 0; i < 32; ++i ) {
		const int index = Slots( 3 )[i];
		if ( !index ) continue;
		const char *held = CL_RitualItemName( index );
		if ( held && !Q_stricmp( held, name.c_str() ) ) {
			amount = Slots( 4 )[i];
			max = Slots( 5 )[i];
			return;
		}
	}
}

/* PlaySound (0x00408d20): a name through the global alias list. */
enum class Sound { Select, Reject, Change };

void PlaySound( Sound sound ) {
	const std::string &name = sound == Sound::Select ? master.selectSound
		: sound == Sound::Reject ? master.rejectSound : master.changeSound;
	if ( name.empty() ) return;
	const char *alias = AliceAlias_FindRandom( -1, name.c_str() );
	S_StartLocalSound( S_RegisterSound( alias ? alias : name.c_str(), qfalse ), CHAN_LOCAL_SOUND );
}

const inv::Type *CurrentType() {
	const int types = static_cast<int>( master.types.size() );
	return pop.sel.type >= 1 && pop.sel.type <= types ? &master.types[pop.sel.type - 1] : nullptr;
}

const inv::Item *CurrentItem() {
	const inv::Type *type = CurrentType();
	if ( !type ) return nullptr;
	const int items = static_cast<int>( type->items.size() );
	return pop.sel.item >= 1 && pop.sel.item <= items ? &type->items[pop.sel.item - 1] : nullptr;
}

inv::Rect Column() { return inv::ColumnRect( master, 640.0f ); }

/* The cursor to a point in the menus' units (0x004282f0). */
void WarpCursor( float x, float y ) {
	CL_AliceSetMousePosition( static_cast<int>( x * ScaleX() ), static_cast<int>( y * ScaleY() ) );
}

void Arm() { pop.closeAt = Now() + TimeoutMs; }

/* SelectItem (0x0040a600): the timeout again, the change sound even for none,
 * and the cursor onto the row's centre when asked. */
void SelectItem( int item, bool warp ) {
	Arm();
	pop.sel.item = item;
	PlaySound( Sound::Change );
	const inv::Type *type = CurrentType();
	if ( !warp || !type || item < 1 || item > static_cast<int>( type->items.size() ) ) return;
	const inv::Rect list = inv::ListRect( master, *type, Column() );
	float rowY = 0.0f;
	for ( int i = 1; i < item; ++i ) rowY += static_cast<float>( type->items[i - 1].height );
	const inv::Item &row = type->items[item - 1];
	WarpCursor( list.x + static_cast<float>( row.width ) * 0.5f, list.y + rowY + static_cast<float>( row.height ) * 0.5f );
}

/* SelectType (0x00408ea0): the timeout again; a new type shows its list with
 * nothing picked, with the change sound; the cursor onto its cell. */
void SelectType( int type, bool warp ) {
	Arm();
	if ( type != pop.sel.type ) {
		pop.sel.type = type;
		pop.sel.item = 0;
		if ( CurrentType() ) PlaySound( Sound::Change );
	}
	if ( !warp || !CurrentType() ) return;
	const inv::Rect column = Column();
	WarpCursor( column.x + static_cast<float>( master.typeWidth ) * 0.5f,
		column.y + static_cast<float>( master.typeHeight ) * static_cast<float>( pop.sel.type - 1 )
			+ static_cast<float>( master.typeHeight ) * 0.5f );
}

void SelectTypeIndex( int type ) {
	if ( type >= 1 && type <= static_cast<int>( master.types.size() ) ) SelectType( type, true );
}

/* WarpTo (0x004090e0): the named type on its first item, or nothing. */
void WarpTo( const char *name ) {
	const int type = inv::FindType( master, name );
	if ( !type ) return;
	SelectType( type, true );
	SelectItem( 1, true );
}

/* ChangeItem (0x00408b20), through SelectType and SelectItem for their
 * timeout and sounds. */
void ChangeItem( int direction ) {
	inv::Selection next = pop.sel;
	inv::ChangeItem( master, next, direction, []( const inv::Item &item ) { return Owns( item.name ); } );
	if ( next.type != pop.sel.type ) SelectType( next.type, false );
	SelectItem( next.item, false );
}

/* UI_DoInventory (0x00408570). */
bool DoInventory( bool mouse ) {
	if ( CL_RitualInCinematic() || pop.open ) return pop.open;
	if ( master.types.empty() ) {
		CL_AliceSetCenterPrint( "- No inventory -", 1.0f );
		return false;
	}
	pop.open = true;
	pop.mouse = mouse;
	pop.sel = inv::Selection();
	pop.closeAt = -1;
	return true;
}

/* The first word of a key's binding, and the word after it (strtok on ' '). */
void BindingWords( int key, std::string &first, std::string &second ) {
	first.clear();
	second.clear();
	const char *binding = Key_GetBinding( key );
	if ( !binding ) return;
	const char *p = binding;
	auto word = [&p]( std::string &out ) {
		while ( *p == ' ' ) ++p;
		while ( *p && *p != ' ' ) out.push_back( *p++ );
	};
	word( first );
	word( second );
}

/* An item used (0x0040a1b0, 0x0040a3c0). From a click, the button's own
 * binding decides: unbound or its own attack goes on, the other attack uses
 * the other hand, anything else stops. */
void Use( bool leftHand, bool click ) {
	if ( CL_RitualInCinematic() ) {
		CL_Fakk2InventoryClose();
		return;
	}
	const inv::Item *item = CurrentItem();
	if ( !item ) return;
	Arm();
	if ( click ) {
		std::string first, second;
		BindingWords( leftHand ? K_MOUSE1 : K_MOUSE2, first, second );
		if ( !first.empty() && Q_stricmp( first.c_str(), leftHand ? "+attackleft" : "+attackright" ) ) {
			if ( !Q_stricmp( first.c_str(), leftHand ? "+attackright" : "+attackleft" ) ) Use( !leftHand, false );
			return;
		}
	}
	if ( !Owns( item->name ) || ( item->equip & inv::EquipNone ) ) {
		PlaySound( Sound::Reject );
		return;
	}
	if ( item->checkAmmo ) {
		int amount = 0, max = 0;
		AmmoCount( item->ammo, amount, max );
		if ( !amount ) {
			PlaySound( Sound::Reject );
			return;
		}
	}
	const std::string command = inv::UseCommand( *item, leftHand );
	if ( command.empty() ) {
		PlaySound( Sound::Reject );
		return;
	}
	Cbuf_AddText( ( command + "\n" ).c_str() );
	PlaySound( Sound::Select );
	if ( !pop.mouse ) CL_Fakk2InventoryClose();
}

/* ---- the commands ---------------------------------------------------------- */

void Inventory_f() {
	if ( DoInventory( true ) ) SelectTypeIndex( 1 );
}

void WarpInv_f() {
	const bool open = DoInventory( false );
	// Its usage has no newline, and the command goes on (0x00408510).
	if ( Cmd_Argc() != 2 ) Com_Printf( "Usage: warpinv <slot_name>" );
	if ( open ) WarpTo( Cmd_Argv( 1 ) );
}

void NextInv_f() {
	if ( DoInventory( false ) ) {
		SelectTypeIndex( 1 );
		ChangeItem( 1 );
	}
}

void PrevInv_f() {
	if ( DoInventory( false ) ) {
		SelectTypeIndex( 1 );
		ChangeItem( -1 );
	}
}

/* Inventory_Load (0x00402b00). Retail's UI starts once a process; this one
 * starts again with every level and video restart, which read the script
 * again quietly. */
bool loadedOnce;

void LoadInventory() {
	master = inv::Inventory();
	const char *path = inventoryFile && inventoryFile->string[0] ? inventoryFile->string : "global/inventory.scr";
	if ( loadedOnce ) {
		Com_DPrintf( "Loading inventory...\n" );
	} else {
		Com_Printf( "Loading inventory...\n" );
		loadedOnce = true;
	}
	void *buffer = nullptr;
	const int length = FS_ReadFile( path, &buffer );
	if ( length < 0 || !buffer ) {
		Com_Printf( "Couldn't load %s\n", path );
		return;
	}
	const inv::LoadResult result = inv::Load( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ),
		master );
	FS_FreeFile( buffer );
	for ( const std::string &warning : result.warnings ) Com_DPrintf( "%s\n", warning.c_str() );
	if ( !result.error.empty() ) Com_Printf( "%s\n", result.error.c_str() );
}

/* inv_restart (0x0040eeb0). */
void InvRestart_f() {
	CL_Fakk2InventoryClose();
	loadedOnce = false;
	LoadInventory();
}

/* saveweaps <n> (0x0040ffe0): the weapons in hand as alias weapgroup<n>,
 * and the centre print. Its usage check can never fire (Cmd_Argc counts the
 * command), so without an argument the alias is `weapgroup` and the print
 * says group 0. */
void SaveWeaps_f() {
	const int *items = Slots( 1 );
	auto name = [items]( int slot ) {
		const char *text = items[slot] >= 0 ? CL_RitualItemName( items[slot] ) : nullptr;
		return std::string( text ? text : "" );
	};
	const std::string command = inv::WeaponGroupCommand( items[hud::ItemWeaponDual] >= 0,
		name( hud::ItemWeaponDual ), name( hud::ItemWeaponLeft ), name( hud::ItemWeaponRight ) );
	Cmd_SetAlias( va( "weapgroup%s", Cmd_Argv( 1 ) ), command.c_str() );
	CL_AliceSetCenterPrint( va( "Saved Weapon Group %d", atoi( Cmd_Argv( 1 ) ) ), 1.0f );
}

/* ui_hud [1|0]: alice.exe keeps the handler (0x0040fbe0) and removes a
 * command of that name as its UI shuts down (0x004103f8) but never
 * registers it. fakk2.exe 1.02 registers it (0x0040ec07..0x0040ec11); its
 * handler (0x0040e010) sets the process's HUD flag, which no cvar holds and
 * nothing saves, and shows or hides the HUD menus at once. Here the flag is
 * the unsaved `ui_hud` cvar (CL_RitualUiHudFlags), which the HUD's rule reads
 * each frame. */
void UiHud_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: ui_hud [1|0]" );
		return;
	}
	Cvar_Set( "ui_hud", atoi( Cmd_Argv( 1 ) ) != 0 ? "1" : "0" );
}

/* ---- drawing ---------------------------------------------------------------- */

void DrawPic( float x, float y, float w, float h, const std::string &shader ) {
	if ( shader.empty() ) return;
	const qhandle_t handle = CL_AliceShader( shader.c_str() );
	if ( !handle ) return;
	static const float white[4] = { 1, 1, 1, 1 };
	CL_AliceDrawPic( x * ScaleX(), y * ScaleY(), w * ScaleX(), h * ScaleY(), white, handle );
}

/* The cell's ammunition gauge (inv::AmmoGauge), `rowY` below the list's top. */
void DrawGauge( const inv::Item &item, float x, float y, float w, float h, float rowY ) {
	if ( item.barShader.empty() || item.ammo.empty() ) return;
	int amount = 0, max = 0;
	AmmoCount( item.ammo, amount, max );
	const qhandle_t handle = CL_AliceShader( item.barShader.c_str() );
	if ( !handle ) return;
	int imageW = 0, imageH = 0;
	if ( re.GetShaderImageSize ) re.GetShaderImageSize( handle, &imageW, &imageH );
	inv::Rect cell;
	cell.x = x;
	cell.y = y;
	cell.w = w;
	cell.h = h;
	const inv::Gauge gauge = inv::AmmoGauge( item, cell, rowY, amount, max, static_cast<float>( imageW ),
		static_cast<float>( imageH ) );
	if ( !gauge.draw ) return;
	static const float white[4] = { 1, 1, 1, 1 };
	re.SetColor( white );
	re.DrawStretchPic( gauge.rect.x * ScaleX(), gauge.rect.y * ScaleY(), gauge.rect.w * ScaleX(),
		gauge.rect.h * ScaleY(), gauge.s0, gauge.t0, gauge.s1, gauge.t1, handle );
	re.SetColor( nullptr );
}

} // namespace

/*
====================
CL_RitualItemName
====================
*/
const char *CL_RitualItemName( int item ) {
	if ( item < 0 ) return nullptr;
	const bool fakk2 = Com_GameIsFAKK2() != 0;
	const int index = ( fakk2 ? FNTECH3_FAKK2_CS_ITEMS : 0x400 ) + item;
	const int count = fakk2 ? FNTECH3_FAKK2_MAX_CONFIGSTRINGS : FNTECH3_ALICE_MAX_CONFIGSTRINGS;
	if ( index >= count || index >= MAX_GAME_CONFIGSTRINGS ) return nullptr;
	const char *text = cl.gameState.stringData + cl.gameState.stringOffsets[index];
	return text[0] ? text : nullptr;
}

void CL_Fakk2InventoryInit( void ) {
	if ( !Com_GameIsFAKK2() ) return;
	inventoryFile = Cvar_Get( "ui_inventoryfile", "global/inventory.scr", 0 );
	if ( !commandsRegistered ) {
		/* Once, as the UI's other commands are: a `warpinv` queued just before
		 * a load must not find itself unknown (cl_alice_ui.cpp). */
		commandsRegistered = true;
		Cmd_AddCommand( "inventory", Inventory_f );
		Cmd_AddCommand( "warpinv", WarpInv_f );
		Cmd_AddCommand( "nextinv", NextInv_f );
		Cmd_AddCommand( "previnv", PrevInv_f );
		Cmd_AddCommand( "inv_restart", InvRestart_f );
		Cmd_AddCommand( "saveweaps", SaveWeaps_f );
		Cmd_AddCommand( "ui_hud", UiHud_f );
	}
	CL_Fakk2InventoryClose();
	LoadInventory();
}

void CL_Fakk2InventoryShutdown( void ) {
	CL_Fakk2InventoryClose();
}

bool CL_Fakk2InventoryIsOpen( void ) {
	return pop.open;
}

bool CL_Fakk2InventoryWantsCursor( void ) {
	return pop.open && pop.mouse;
}

bool CL_Fakk2InventoryClose( void ) {
	if ( !pop.open ) return false;
	pop.open = false;
	pop.sel = inv::Selection();
	pop.closeAt = -1;
	return true;
}

void CL_Fakk2InventoryFrame( void ) {
	if ( !pop.open ) return;
	// A cinematic closes it each frame (UI_Update 0x0040c817); so does a menu,
	// whose end brings the view back, which closes it (0x0041f9e0).
	if ( cls.state != CA_ACTIVE || CL_RitualInCinematic() || CL_AliceUIMenuIsUp() ) {
		CL_Fakk2InventoryClose();
		return;
	}
	if ( pop.closeAt >= 0 && Now() >= pop.closeAt ) CL_Fakk2InventoryClose();
}

void CL_Fakk2InventoryDraw( void ) {
	if ( !pop.open || master.types.empty() ) return;
	const inv::Rect column = Column();
	const float tw = static_cast<float>( master.typeWidth ), th = static_cast<float>( master.typeHeight );
	/* One cell a type (FakkInventory::Draw, fakk2.exe 1.02 0x004088c0): its
	 * button picture, or for the current type its hover picture while the
	 * cursor is over the column -- the window manager's widget under the
	 * cursor, whichever way the inventory opened (0x0040890a) -- and its
	 * selected picture otherwise. */
	int cursorX = 0, cursorY = 0;
	CL_AliceGetMouseState( &cursorX, &cursorY, nullptr );
	const float vx = static_cast<float>( cursorX ) / ScaleX(), vy = static_cast<float>( cursorY ) / ScaleY();
	const bool overColumn = vx >= column.x && vx < column.x + column.w && vy >= column.y && vy < column.y + column.h;
	for ( std::size_t i = 0; i < master.types.size(); ++i ) {
		const inv::Type &type = master.types[i];
		const bool current = static_cast<int>( i ) + 1 == pop.sel.type;
		const std::string &shader = current ? ( overColumn ? type.hover : type.selected ) : type.button;
		DrawPic( column.x, column.y + th * static_cast<float>( i ), tw, th, shader );
	}
	const inv::Type *type = CurrentType();
	if ( !type ) return;
	const inv::Rect list = inv::ListRect( master, *type, column );
	// The type's background under the whole list, tiled or stretched (0x00409e30).
	if ( !type->bg.empty() ) {
		const qhandle_t handle = CL_AliceShader( type->bg.c_str() );
		int imageW = 0, imageH = 0;
		if ( handle && type->bgTile && re.GetShaderImageSize ) re.GetShaderImageSize( handle, &imageW, &imageH );
		static const float white[4] = { 1, 1, 1, 1 };
		if ( handle ) {
			re.SetColor( white );
			re.DrawStretchPic( list.x * ScaleX(), list.y * ScaleY(), list.w * ScaleX(), list.h * ScaleY(), 0.0f, 0.0f,
				imageW > 0 ? list.w / static_cast<float>( imageW ) : 1.0f, imageH > 0 ? list.h / static_cast<float>( imageH ) : 1.0f,
				handle );
			re.SetColor( nullptr );
		}
	}
	float rowY = 0.0f;
	for ( std::size_t i = 0; i < type->items.size(); ++i ) {
		const inv::Item &item = type->items[i];
		const float w = static_cast<float>( item.width ), h = static_cast<float>( item.height );
		const float y = list.y + rowY;
		const bool selected = static_cast<int>( i ) + 1 == pop.sel.item;
		/* FakkItemList::Draw (fakk2.exe 1.02 0x00409950), a row at a time: the
		 * gauge, the cell's picture over it, the selected picture unless it
		 * goes on top, the model of an item held, then a selected picture
		 * that goes on top. */
		DrawGauge( item, list.x, y, w, h, rowY );
		DrawPic( list.x, y, w, h, item.bgShader );
		if ( selected && !item.selectedShader.empty() && !item.selectedOnTop ) DrawPic( list.x, y, w, h, item.selectedShader );
		if ( Owns( item.name ) ) {
			// The model in its window of the cell, moving only while picked.
			CL_Fakk2RenderModel( list.x + item.window[0] * w, y + item.window[1] * h, w * item.window[2],
				h * item.window[3], item.model.c_str(), item.anim.c_str(), item.view, selected );
		}
		if ( selected && !item.selectedShader.empty() && item.selectedOnTop ) DrawPic( list.x, y, w, h, item.selectedShader );
		rowY += h;
	}
}

/* FakkInventory::KeyEvent (0x004091b0), for presses. */
void CL_Fakk2InventoryKey( int key ) {
	if ( !pop.open ) return;
	if ( CL_RitualInCinematic() ) {
		CL_Fakk2InventoryClose();
		return;
	}
	if ( master.types.empty() ) return;
	const bool picked = CurrentItem() != nullptr;

	std::string first, second;
	BindingWords( key, first, second );
	std::string attack, token;
	bool hasToken = false;
	if ( !Q_stricmp( first.c_str(), "+attackleft" ) ) {
		attack = "attackleft";
		token = first;
		hasToken = true;
	} else if ( !Q_stricmp( first.c_str(), "+attackright" ) ) {
		attack = "attackright";
		token = first;
		hasToken = true;
	} else if ( !Q_stricmp( first.c_str(), "warpinv" ) ) {
		token = second;
		hasToken = !second.empty();
	} else if ( !Q_stricmp( first.c_str(), "inventory" ) ) {
		CL_Fakk2InventoryClose();
		return;
	} else if ( !Q_stricmp( first.c_str(), "nextinv" ) ) {
		ChangeItem( 1 );
		return;
	} else if ( !Q_stricmp( first.c_str(), "previnv" ) ) {
		ChangeItem( -1 );
		return;
	}

	const bool upDown = key == K_UPARROW || key == K_DOWNARROW;
	const bool leftRight = key == K_LEFTARROW || key == K_RIGHTARROW;
	const bool wheel = key == K_MWHEELUP || key == K_MWHEELDOWN;
	const int direction = key == K_UPARROW || key == K_MWHEELUP ? -1 : 1;
	const bool cascadeLeft = master.cascade == inv::Cascade::Left;

	if ( !picked ) {
		if ( ( pop.mouse && upDown ) || wheel ) {
			// A step through the types, without more time (0x0040962c).
			SelectType( inv::StepType( master, pop.sel.type, direction ), true );
			return;
		}
		if ( pop.mouse && leftRight ) {
			// Towards the list: its first item.
			if ( CurrentType() && ( key == K_LEFTARROW ) == cascadeLeft ) SelectItem( 1, true );
		} else if ( hasToken ) {
			WarpTo( token.c_str() );
		}
	} else {
		const inv::Type *type = CurrentType();
		if ( pop.mouse && leftRight ) {
			// Away from the list: back to the column.
			if ( ( key == K_RIGHTARROW ) == cascadeLeft ) {
				SelectType( pop.sel.type, true );
				SelectItem( 0, false );
			}
		} else if ( ( pop.mouse && upDown ) || wheel || ( hasToken && !Q_stricmp( token.c_str(), type->name.c_str() ) ) ) {
			// The type's own key again: its next item, held or not (0x00409a07).
			SelectItem( inv::CycleItem( *type, pop.sel.item, direction ), true );
		} else if ( attack == "attackleft" ) {
			Use( true, false );
		} else if ( attack == "attackright" ) {
			Use( false, false );
		} else if ( hasToken ) {
			WarpTo( token.c_str() );
		} else {
			// Any other key does what it is bound to, and adds no time (0x0042a0a0).
			Key_ParseBinding( key, qtrue, static_cast<unsigned>( Sys_Milliseconds() ) );
			return;
		}
	}
	// A key it took puts the timeout five seconds further off (0x00409ae4).
	if ( pop.open && pop.closeAt >= 0 ) pop.closeAt += TimeoutMs;
}

/* A press on the column (0x00408d90) or on the list (0x0040a1b0, 0x0040a3c0). */
void CL_Fakk2InventoryMouseDown( int key, float x, float y ) {
	if ( !pop.open ) return;
	CL_Fakk2InventoryMouseMoved( x, y );
	const inv::Rect column = Column();
	if ( x >= column.x && x < column.x + column.w && y >= column.y && y < column.y + column.h ) {
		const int type = static_cast<int>( std::floor( ( y - column.y ) / static_cast<float>( master.typeHeight ) ) ) + 1;
		if ( type >= 1 && type <= static_cast<int>( master.types.size() ) ) {
			const int before = pop.sel.type;
			SelectType( type, true );
			if ( before == type ) SelectItem( 1, true );
		}
		return;
	}
	const inv::Type *current = CurrentType();
	if ( !current ) return;
	const inv::Rect list = inv::ListRect( master, *current, column );
	if ( x >= list.x && x < list.x + list.w && y >= list.y && y < list.y + list.h ) {
		if ( key == K_MOUSE1 ) Use( true, true );
		else if ( key == K_MOUSE2 ) Use( false, true );
	}
}

/* The list's hover (0x0040a710): the row under the cursor is picked. */
void CL_Fakk2InventoryMouseMoved( float x, float y ) {
	if ( !pop.open ) return;
	const inv::Rect column = Column();
	const inv::Type *type = CurrentType();
	if ( !type ) return;
	const inv::Rect list = inv::ListRect( master, *type, column );
	float rowY = list.y;
	for ( std::size_t i = 0; i < type->items.size(); ++i ) {
		const inv::Item &item = type->items[i];
		const float h = static_cast<float>( item.height );
		if ( x >= list.x && x <= list.x + static_cast<float>( item.width ) && y >= rowY && y <= rowY + h ) {
			if ( static_cast<int>( i ) + 1 != pop.sel.item ) SelectItem( static_cast<int>( i ) + 1, false );
			return;
		}
		rowY += h;
	}
}

const fntech3::fakk2::inventory::Item *CL_Fakk2FindInventoryItem( const char *name ) {
	return inv::FindItem( master, name );
}

/*
====================
CL_Fakk2RenderModel

UI_Render3DModel (0x00408210): the model at the distance its largest extent
fills a 30 degree view from, turned to its angles (moving by its bob or spin
when asked), placed by its offsets, alone in a view of the rectangle with no
world and no light, at its first animation's first frame unless it names
one. Both fields of view are 30 degrees whatever the rectangle's shape, as
retail's are.
====================
*/
void CL_Fakk2RenderModel( float x, float y, float w, float h, const char *model, const char *anim,
	const fntech3::fakk2::hud::View &view, bool moving ) {
	if ( !model || !model[0] || w <= 0.0f || h <= 0.0f ) return;
	const qhandle_t handle = re.RegisterModel( model );
	if ( !handle ) return;
	vec3_t mins, maxs;
	re.ModelBounds( handle, mins, maxs );
	const hud::Vec3 camera = hud::ModelOrigin( mins, maxs, view.scale );
	const hud::Vec3 angles = moving ? hud::MovedAngles( view, static_cast<double>( cls.realtime ) ) : view.angles;

	refEntity_t entity{};
	entity.reType = RT_MODEL;
	entity.hModel = handle;
	const vec3_t turn = { angles.x, angles.y, angles.z };
	AnglesToAxis( turn, entity.axis );
	const hud::Vec3 origin = hud::EntityOrigin( camera, view, entity.axis );
	entity.origin[0] = origin.x;
	entity.origin[1] = origin.y;
	entity.origin[2] = origin.z;
	VectorCopy( entity.origin, entity.oldorigin );
	VectorCopy( entity.origin, entity.lightingOrigin );
	aliceRenderEntity_t &alice = entity.alice;
	int animNum = 0;
	if ( anim && anim[0] && re.GetAliceTikiHandle ) {
		const int tiki = re.GetAliceTikiHandle( handle );
		animNum = tiki >= 0 ? AliceTiki_AnimNumForName( tiki, anim ) : 0;
		if ( animNum < 0 ) animNum = 0;
	}
	alice.anim = alice.oldanim = animNum;
	alice.frame = alice.oldframe = 0;
	alice.crossblendLerp = -1.0f;
	alice.torsoCrossblendLerp = 0.0f;
	alice.useLegs = qtrue;
	alice.scale = 1.0f;

	refdef_t refdef{};
	refdef.x = static_cast<int>( x * ScaleX() );
	refdef.y = static_cast<int>( y * ScaleY() );
	refdef.width = static_cast<int>( w * ScaleX() );
	refdef.height = static_cast<int>( h * ScaleY() );
	if ( refdef.width <= 0 || refdef.height <= 0 ) return;
	refdef.fov_x = refdef.fov_y = 30.0f;
	AxisClear( refdef.viewaxis );
	refdef.time = cl.serverTime;
	/* No world and nothing else (0x00407e4e): F.A.K.K.2 has no Alice's
	 * ui_skipDepthClear, so its view clears the depth as every view does and
	 * the model always draws in front of the world behind its rectangle. */
	refdef.rdflags = RDF_NOWORLDMODEL;
	refdef.alice.keepDepth = qfalse;
	// A HUD label's view goes where its menu's pictures go, keeping its fields of view.
	if ( CL_RitualHudLayout() ) CL_HudAdjustRefdef( &refdef );

	re.ClearScene();
	re.AddRefEntityToScene( &entity, qfalse );
	re.RenderScene( &refdef );
}
