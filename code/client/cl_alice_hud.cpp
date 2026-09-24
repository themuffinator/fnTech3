/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* American McGee's Alice's HUD and subtitles, both engine widgets in retail.
 *
 * Subtitles are AliceDialogText (constructor 0x00416c60, Init 0x00417020,
 * Print 0x00417130, Draw 0x00417670, commands 0x00417a80): a 384 by 64 box
 * named `aliceDialog`, framed by two pictures, whose text the game module
 * sends through its WidgetPrintf import (game import 106, 0x004389e0 ->
 * UI_WidgetPrint 0x00410e30). A line of plain text replaces what the box
 * shows; a line beginning with `*` changes its timing. The box fades in,
 * holds, scrolls through any lines past its third over the hold, and fades
 * out, and it draws only with ui_subtitles set and no menu or loading screen
 * up.
 *
 * The HUD is AliceHud (0x00411080, children built at 0x004110d0): an
 * invisible desktop widget whose own Draw (0x00411bf0) is empty and whose
 * eight children each render one TIKI model into a 2D view of its own -- the
 * health and mana gauges (a back, a riser and a bar each) and the weapon and
 * powerup foldouts. All eight are served here as retail draws them:
 *
 *   UIWidget3D::Draw    0x004d2b10  angles (0, 180, 0), frame 0, "idle"
 *   riser Draw          0x00411d50  a stat's value, eased over 500 ms, as the
 *                                   frame (100 - value) * 0.98
 *   weapon foldout Draw 0x004121c0  the weapon in the hand, a card per weapon
 *   item foldout Draw   0x00412a00  the powerup running and its time left
 *   UIWidget3D render   0x004d29c0  the model's bounds place and size it
 *   UI_Render3DModel    0x00408210  one entity in a view of its own
 *
 * The view is the whole screen's size, shifted by the widget's offset (in
 * 640x480 units from the screen's centre); its field of view is 30 degrees on
 * both axes, and the model stands at the distance at which its largest extent
 * times the widget's scale fills the view's height (tan 15 = 0.268).
 *
 * The weapon foldout reads the player state's active item 3 (0x004121dc), an
 * item index or -1, names it through the inventory array beside it and the
 * item configstrings (0x00407500), and shows that weapon's card from a table
 * of ten (0x0050c668): a new weapon swaps the card at once, losing the weapon
 * folds the card shut over 200 ms and a weapon arriving at a shut card
 * unfolds it with the new card; the demon dice's card counts the dice held.
 * The item foldout reads the 60-byte block the game module and the engine
 * share (0x0042c490), where the game writes which of the rage box, the
 * looking glass, the grasshopper tea and the deadtime watch is running and
 * the time left: a powerup's card unfolds over 200 ms and then folds back by
 * the fraction of its time used, and goes when the time runs out. The weapon
 * card also turns a degree either way on sin(realtime / 333). Both run only
 * while the HUD shows, as retail's hidden widgets are not drawn. The state
 * machines and their tables are code/compat/alice_hud.hpp; see
 * docs/fntech3/2026-09-24-alice-hud.md.
 *
 * AliceHud draws no ammunition, inventory, pickup list or crosshair: retail's
 * ui_crosshair and ui_jumpreticle cvars are registered by the engine (0x004105a0,
 * 0x004105b6) and never read by it. */

#include "client_alice_ui.hpp"
#include "../compat/alice_hud.hpp"
#include "../compat/alice_module_abi.h"
#include "../qcommon/alice_runtime.h"
#include "../qcommon/alice_tiki.h"

#include <iterator>
#include <string>
#include <vector>

namespace {

namespace screen = fntech3::alice::screen;
namespace font = fntech3::alice::ritualfont;
namespace hud = fntech3::alice::hud;

cvar_t *hudEnabled;
cvar_t *subtitlesEnabled;
/* 0x00531614: how long the HUD has been allowed, for its 550 ms delay. */
int hudDelay;
bool hudShown;

struct Dialog {
	screen::DialogLayout layout;
	screen::DialogTiming timing;
	std::vector<std::string> lines;
	int font = -1;
	int start = screen::DialogResetTime;   /* +0x200 */
	bool shown = false;                    /* +0xcc */
	int scrollTimer = 0;                   /* +0x20c */
	int lastShown = -1;                    /* +0x210 */
};

Dialog dialog;
bool dialogReady;

/* A model the HUD draws, registered by name on first use together with its
 * "idle" animation (UIWidget3D's SetAnim, 0x004d27c0). The foldouts swap
 * among the models of their tables, so handles are kept by name until the
 * renderer restarts. */
struct HudModel {
	std::string name;
	qhandle_t handle = 0;
	int anim = 0;
};

std::vector<HudModel> hudModels;

HudModel HudModelFor( const std::string &name ) {
	for ( const HudModel &model : hudModels ) {
		if ( model.name == name ) {
			return model;
		}
	}
	HudModel model;
	model.name = name;
	model.handle = re.RegisterModel( name.c_str() );
	const int tiki = model.handle && re.GetAliceTikiHandle ? re.GetAliceTikiHandle( model.handle ) : -1;
	model.anim = tiki >= 0 ? AliceTiki_AnimNumForName( tiki, "idle" ) : 0;
	if ( model.anim < 0 ) {
		model.anim = 0;
	}
	hudModels.push_back( model );
	return model;
}

/* Each riser's ease (0x00411c00 builds it; 0x00411d50 runs it), by widget. */
hud::Riser risers[std::size( hud::Widgets )];
hud::WeaponFoldout weaponFoldout;
hud::ItemFoldout itemFoldout;

cvar_t *skipDepthClear;

/* UIWidget3D's render (0x004d29c0) and UI_Render3DModel (0x00408210). */
void DrawModel( const HudModel &model, int frame, const hud::Widget &widget, float yaw ) {
	if ( !model.handle ) {
		return;
	}
	vec3_t mins, maxs;
	re.ModelBounds( model.handle, mins, maxs );
	const float W = static_cast<float>( cls.glconfig.vidWidth );
	const float H = static_cast<float>( cls.glconfig.vidHeight );
	// The largest extent, taking the later axis on a tie, as retail compares.
	float extent = maxs[1] - mins[1];
	if ( !( extent > maxs[2] - mins[2] ) ) extent = maxs[2] - mins[2];
	if ( !( extent > maxs[0] - mins[0] ) ) extent = maxs[0] - mins[0];
	const float distance = extent * ( widget.scale * ( 4.0f / 3.0f ) / ( W / H ) ) * 0.5f / 0.268f;

	refEntity_t entity{};
	entity.reType = RT_MODEL;
	entity.hModel = model.handle;
	const vec3_t angles = { 0.0f, yaw, 0.0f };
	AnglesToAxis( angles, entity.axis );
	entity.origin[0] = distance;
	entity.origin[1] = ( maxs[1] + mins[1] ) * 0.5f;
	entity.origin[2] = ( maxs[2] + mins[2] ) * -0.5f;
	VectorCopy( entity.origin, entity.oldorigin );
	VectorCopy( entity.origin, entity.lightingOrigin );
	aliceRenderEntity_t &alice = entity.alice;
	alice.anim = alice.oldanim = model.anim;
	alice.frame = frame;
	alice.oldframe = 0;
	alice.crossblendLerp = -1.0f;
	alice.torsoCrossblendLerp = 0.0f;
	alice.useLegs = qtrue;
	alice.scale = 1.0f;

	refdef_t view{};
	view.x = static_cast<int>( W / 640.0f * widget.offsetX );
	view.y = static_cast<int>( H / 480.0f * widget.offsetY );
	view.width = cls.glconfig.vidWidth;
	view.height = cls.glconfig.vidHeight;
	view.fov_x = view.fov_y = 30.0f;
	AxisClear( view.viewaxis );
	view.time = cl.serverTime;
	view.rdflags = RDF_NOWORLDMODEL;
	view.alice.keepDepth = skipDepthClear && skipDepthClear->integer ? qtrue : qfalse;

	re.ClearScene();
	re.AddRefEntityToScene( &entity, qfalse );
	re.RenderScene( &view );
}

/* The item's name for the weapon foldout (0x00407500): the configstring of an
 * item the inventory array holds, or none. Items are configstrings 0x400 on
 * (the game's item index import at 0x400 over 64). Retail stops the client
 * with CL_ConfigString's "bad index" drop past the configstrings' end
 * (0x0042b649); this one has no name there instead. */
const char *InventoryItemName( int item ) {
	const playerState_t &ps = cl.snap.ps;
	if ( !hud::InventoryHolds( ps.alice.block[2], item ) ) {
		return nullptr;
	}
	const int index = 0x400 + item;
	if ( index < 0 || index >= FNTECH3_ALICE_MAX_CONFIGSTRINGS ) {
		return nullptr;
	}
	return cl.gameState.stringData + cl.gameState.stringOffsets[index];
}

/* The HUD half of the shared block the game module writes (import 107,
 * 0x0042c490). */
hud::PowerupState SharedPowerups() {
	return hud::ReadPowerupState( Alice_SharedBlock() );
}

/* 0x004121c0. */
void WeaponFoldoutDraw( const hud::Widget &widget ) {
	const int activeItem = cl.snap.ps.alice.block[1][3];
	const hud::PowerupState shared = SharedPowerups();
	const char *name = activeItem != -1 ? InventoryItemName( activeItem ) : nullptr;
	const hud::Fold fold = weaponFoldout.fold;
	const std::string model = weaponFoldout.model;
	const hud::WeaponRequest request = hud::RequestWeapon( activeItem, name );
	hud::StepWeaponFoldout( weaponFoldout, request, &shared.dice, cls.frametime );
	static int lastItem = -2;
	if ( activeItem != lastItem || weaponFoldout.fold != fold || weaponFoldout.model != model ) {
		lastItem = activeItem;
		Com_DPrintf( "AliceHud: weapon foldout: item %d \"%s\" -> entry %d, fold %d, frame %g, %s\n", activeItem,
			name ? name : "", request.index, static_cast<int>( weaponFoldout.fold ), weaponFoldout.frame,
			weaponFoldout.model.c_str() );
	}
	DrawModel( HudModelFor( weaponFoldout.model ), static_cast<int>( weaponFoldout.frame ), widget,
		hud::WeaponFoldoutYaw( cls.realtime ) );
}

/* 0x00412a00. */
void ItemFoldoutDraw( const hud::Widget &widget ) {
	const hud::PowerupState shared = SharedPowerups();
	const hud::ItemFold state = itemFoldout.state;
	const hud::Powerup kind = itemFoldout.kind;
	const bool drawn = hud::StepItemFoldout( itemFoldout, &shared, cls.frametime );
	static int lastFlags = -1;
	const int flags = ( shared.tea ? 1 : 0 ) | ( shared.rageBox ? 2 : 0 ) | ( shared.glass ? 4 : 0 )
		| ( shared.watch ? 8 : 0 );
	if ( itemFoldout.state != state || itemFoldout.kind != kind || flags != lastFlags ) {
		lastFlags = flags;
		Com_DPrintf( "AliceHud: item foldout: state %d, powerup %d (tea %d, rage box %d, glass %d, watch %d), "
			"time %g of %d\n", static_cast<int>( itemFoldout.state ), static_cast<int>( itemFoldout.kind ),
			shared.tea, shared.rageBox, shared.glass, shared.watch, shared.time, itemFoldout.total );
	}
	if ( !drawn ) {
		return;
	}
	DrawModel( HudModelFor( itemFoldout.model ), static_cast<int>( itemFoldout.frame ), widget, hud::BaseYaw );
}

/* One child's Draw, by its class. */
void WidgetDraw( std::size_t index ) {
	const hud::Widget &widget = hud::Widgets[index];
	switch ( widget.kind ) {
	case hud::WidgetKind::Riser: {
		const int frame = hud::RiserFrame( risers[index], cl.snap.ps.alice.block[0][widget.stat], cls.frametime );
		DrawModel( HudModelFor( widget.model ), frame, widget, hud::BaseYaw );
		break;
	}
	case hud::WidgetKind::WeaponFoldout:
		WeaponFoldoutDraw( widget );
		break;
	case hud::WidgetKind::ItemFoldout:
		ItemFoldoutDraw( widget );
		break;
	case hud::WidgetKind::Still:
	default:
		DrawModel( HudModelFor( widget.model ), 0, widget, hud::BaseYaw );
		break;
	}
}

void DialogSetup() {
	if ( dialogReady ) {
		return;
	}
	dialogReady = true;
	dialog.font = CL_AliceFontRegister( screen::DialogFont );
	dialog.layout = screen::DialogLayoutFor( screen::DialogWidth, screen::DialogHeight,
		CL_AliceFontLineHeight( dialog.font ) );
	// AliceDialogText::Init clamps the line count to at least one (0x00417633).
	if ( dialog.layout.visibleLines < 1 ) {
		dialog.layout.visibleLines = 1;
	}
}

void DialogPrint( const char *text ) {
	DialogSetup();
	if ( text[0] == '*' ) {
		switch ( screen::DialogCommand( text, dialog.timing ) ) {
		case 1:
			dialog.lines.clear();
			dialog.scrollTimer = 0;
			dialog.lastShown = 0;
			break;
		case 2:
			dialog.start = screen::DialogResetTime;
			break;
		default:
			break;
		}
		return;
	}
	const font::Font *metrics = CL_AliceFontMetrics( dialog.font );
	dialog.lines = screen::DialogWrap( text, dialog.layout.wrapWidth,
		[metrics]( char c ) { return font::CharWidth( metrics, c ); } );
	// A line past 128 characters replaces the text with a single newline
	// (0x0041761e).
	if ( !dialog.lines.empty() && dialog.lines.back().size() > 128 ) {
		dialog.lines = screen::DialogWrap( "\n", dialog.layout.wrapWidth,
			[metrics]( char c ) { return font::CharWidth( metrics, c ); } );
	}
	dialog.start = cls.realtime;
	dialog.shown = true;
}

void DialogLine( int index, float y, const float *color ) {
	CL_AliceFontPrint( dialog.font, color, true, dialog.layout.x + dialog.layout.textX, dialog.layout.y + y,
		dialog.lines[index].c_str(), -1, 0, 0, 0, 1, 1 );
}

/* Retail draws the box inside a 2D window of its own rectangle, so a line
 * scrolling past an edge is cut there. This engine has no 2D clip, so a line
 * is drawn only while at least half of it lies inside the box. */
bool DialogLineVisible( float y ) {
	const float half = dialog.layout.lineHeight * 0.5f;
	return y >= -half && y <= dialog.layout.h - half;
}

} // namespace

void CL_AliceHudInit( void ) {
	hudEnabled = Cvar_Get( "ui_hud", "1", CVAR_ARCHIVE );
	subtitlesEnabled = Cvar_Get( "ui_subtitles", "1", CVAR_ARCHIVE );
	skipDepthClear = Cvar_Get( "ui_skipDepthClear", "1", 0 );
	// The font's picture and the models are registered again by the new
	// renderer. The widgets' own state carries over, as retail builds them
	// once (UI_Init 0x00410bc9).
	dialogReady = false;
	hudModels.clear();
}

void CL_AliceHudShutdown( void ) {
	hudShown = false;
	hudDelay = 0;
}

/* The HUD shows once it has been allowed for 550 ms: outside cinematics
 * (stats[5] bit 0), with ui_hud set, in the game, with no letterbox, menu or
 * loading screen (UI_Update 0x0040c93a). */
void CL_AliceHudFrame( bool allowed ) {
	allowed = allowed && hudEnabled && hudEnabled->integer && !( cl.snap.ps.alice.block[0][5] & 1 );
	if ( !allowed ) {
		hudDelay = 0;
		hudShown = false;
		return;
	}
	if ( hudDelay >= screen::HudShowDelayMs ) {
		hudShown = true;
	} else {
		hudDelay += cls.frametime;
		hudShown = false;
	}
}

/* AliceHud's children in creation order, which is the order they are drawn
 * in. A hidden HUD draws none of them, so no foldout advances while it is
 * hidden. */
void CL_AliceHudDraw( void ) {
	if ( !hudShown || cls.state != CA_ACTIVE ) {
		return;
	}
	for ( std::size_t i = 0; i < std::size( hud::Widgets ); ++i ) {
		WidgetDraw( i );
	}
}

/* UI_WidgetPrint's search finds the one desktop widget with a name. */
bool CL_AliceDialogPrint( const char *widget, const char *text ) {
	if ( !widget || Q_stricmp( widget, "aliceDialog" ) || !text ) {
		return false;
	}
	DialogPrint( text );
	return true;
}

/*
====================
CL_AliceDialogDraw
====================
*/
void CL_AliceDialogDraw( void ) {
	if ( !dialog.shown || !subtitlesEnabled || !subtitlesEnabled->integer ) {
		return;
	}
	if ( CL_AliceUIMenuIsUp() || CL_AliceLoadingActive() ) {
		return;
	}
	DialogSetup();
	const float elapsed = static_cast<float>( cls.realtime - dialog.start );
	const float alpha = screen::DialogAlpha( elapsed, dialog.timing );
	if ( alpha < 0.0f ) {
		dialog.shown = false;
		dialog.lastShown = -1;
		return;
	}

	const screen::DialogLayout &box = dialog.layout;
	const float frame[4] = { 1, 1, 1, alpha };
	CL_AliceDrawPic( box.x, box.y, box.w * 0.5f, box.h, frame, CL_AliceShader( screen::DialogLeftFrame ) );
	CL_AliceDrawPic( box.x + box.w * 0.5f, box.y, box.w * 0.5f, box.h, frame,
		CL_AliceShader( screen::DialogRightFrame ) );

	const int count = static_cast<int>( dialog.lines.size() );
	if ( !count ) {
		return;
	}
	const float lineHeight = static_cast<float>( box.lineHeight );
	// The box's foreground colour at its alpha (0x0041781c, 0x00417834).
	const float text[4] = { screen::DialogTextColor[0], screen::DialogTextColor[1], screen::DialogTextColor[2],
		alpha };

	if ( box.visibleLines >= count ) {
		// Everything fits: the last line at the bottom, the rest above it.
		float y = box.h - lineHeight;
		for ( int i = count - 1; i >= 0 && y > -lineHeight; --i, y -= lineHeight ) {
			if ( DialogLineVisible( y ) ) {
				DialogLine( i, y, text );
			}
		}
		return;
	}

	// More lines than fit: they scroll in over the hold, each new one sliding
	// the text up over 500 ms (0x00417965, 0x0041797d).
	int shown = screen::DialogLinesShown( elapsed, dialog.timing.fadeTime, count, box.visibleLines );
	int last = box.visibleLines + shown - 1;
	if ( last > count ) {
		last = count;
		shown = count - box.visibleLines + 1;
	}
	float y = -( shown - 1 ) * lineHeight;
	if ( shown != 1 && shown != dialog.lastShown ) {
		dialog.scrollTimer = 500;
	}
	if ( dialog.scrollTimer > 0 ) {
		y += dialog.scrollTimer * 0.002f * lineHeight;
	}
	for ( int i = 0; i < last; ++i, y += lineHeight ) {
		if ( y >= -lineHeight && y <= box.h + lineHeight && DialogLineVisible( y ) ) {
			DialogLine( i, y, text );
		}
	}
	dialog.lastShown = shown;
	dialog.scrollTimer -= cls.frametime;
}
