/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* Client-side state American McGee's Alice keeps in its engine rather than in
 * its client game, and the client's hooks into that engine-side layer.
 *
 * Alice's client game draws the 3D view and its own effects, but the engine
 * owns everything around it: the menus, the console, the fonts they draw
 * with, the loading screen, and the centre and located prints the server sends
 * as opcodes of their own. Retail keeps those prints on its in-game view widget
 * (the `View3D` whose methods at 0x0041f560 and 0x0041f760 store them) and
 * draws them in that widget's own pass; see cl_alice_screen.cpp. */

#include "client_alice_ui.hpp"

/*
====================
CL_AliceCenterPrint

A print the server sent with `svc_centerprint` (x and y -1) or
`svc_locprint`. A new print replaces the one on screen. Retail hands both to
its view at full alpha (0x0042dbd0 -> UI_CenterPrint 0x0040ed40 with 1.0;
0x0042dbf0 -> UI_LocationPrint 0x0040ed60 with 1.0 and a size of -1, -1).
====================
*/
void CL_AliceCenterPrint( const char *text, int x, int y, qboolean located ) {
	if ( located ) {
		CL_AliceSetLocationPrint( x, y, text, 1.0f, -1, -1 );
	} else {
		CL_AliceSetCenterPrint( text, 1.0f );
	}
	if ( text && text[0] ) {
		Com_DPrintf( "Alice %s print: %s\n", located ? "located" : "centre", text );
	}
}

void CL_AliceClearState( void ) {
	CL_AliceClearPrints();
}

/*
====================
CL_AliceLoadingScreenRefresh

The client game's slot 19 and the game's slot 109 reach retail's
UpdateLoadingScreen (0x0042d8d0), which modules call while they register media
so the loading screen keeps moving. It refreshes only while the client is
still loading and no menu is up (`cmp [0xc23940], 6; jg` and UI_MenuUp
0x0040c3b0); once primed or in the game the frame is drawn by the frame loop,
and a nested one would be drawn inside it.
====================
*/
void CL_AliceLoadingScreenRefresh( void ) {
	if ( cls.state > CA_LOADING || CL_AliceUIMenuUp() ) {
		return;
	}
	SCR_UpdateScreen();
}

/*
====================
CL_AliceRegisterClientArt

CL_InitRenderer's art under Alice, which ships none of the Quake III names:
no bigchars, no white or console shader, no recording icon, no menu cursor
(scan of all ten retail paks). The fills are the engine's own white; the
console's character sheet is built from courier-14, the font retail's
console window draws in; the cursor is Alice's own `gfx/2d/mouse_arrow`
(referenced at 0x004b3dc0).
====================
*/
void CL_AliceRegisterClientArt( void ) {
	CL_AliceArtRendererStarted();
	cls.whiteShader = CL_AliceWhite();
	cls.consoleShader = cls.whiteShader;
	/* Heavy Metal: F.A.K.K.2 ships Quake III's character sheet itself --
	 * gfx/2d/bigchars, which its scripts/gfx.shader declares -- so its
	 * console draws from that rather than from a sheet built out of a font. */
	cls.charSetShader = Com_GameIsFAKK2() ? re.RegisterShaderNoMip( "gfx/2d/bigchars" )
		: CL_AliceConsoleCharset();
	cls.recordShader = 0;
	cls.consoleLogoShader = 0;
	cls.cursorShader = re.RegisterShaderNoMip( "gfx/2d/mouse_arrow" );
}

/*
====================
CL_AliceMapLoading

CL_MapLoading's first acts in retail (0x0042aa80): the loading stage to zero
(0x0040b590), the intro given up and the loading screen raised (UI_BeginLoad
0x0040ed20), every menu forced away (0x0040d540(1)). The screen update that
follows is the caller's.
====================
*/
void CL_AliceMapLoading( void ) {
	CL_AliceUISetIntroStateInternal( -1 );
	CL_AliceLoadingBegin();
	CL_AliceUIClearMenus( true );
}

/*
====================
CL_AliceFirstSnapshot

The first playable frame (CL_FirstSnapshot 0x00424aa0): UI_EndLoad
(0x0040ecf0) releases any pause a menu took and lowers the loading screen,
and the view is shown.
====================
*/
void CL_AliceFirstSnapshot( void ) {
	CL_AliceUIReleasePause();
	CL_AliceLoadingEnd();
}

/*
====================
CL_AliceMapLoadingKeep

CL_MapLoading as the server's reuse path calls it, with a zero argument
(0x0045513f): the loading screen goes up as for any spawn, and the
connection, the gamestate and the client game are all kept (0x0042aaab
returns before the reconnect). The console closes as this engine's own map
loading closes it.
====================
*/
void CL_AliceMapLoadingKeep( void ) {
	if ( !com_cl_running || !com_cl_running->integer ) {
		return;
	}
	Con_Close();
	Key_SetCatcher( 0 );
	CL_AliceMapLoading();
	SCR_UpdateScreen();
}

/*
====================
CL_AliceSnapshotServerCount

CL_SetCGameTime's test of each frame's snapshot (0x00424b9f..0x00424bf1): when
the server bit differs from the last frame's, the server has spawned again,
and 0x0042d890 runs -- every sound stops but the music (S_StopAllSounds(0)),
UI_EndLoad lowers the loading screen and releases a menu's pause (0x0040ecf0),
the sound fade is cancelled (0x0040eed0, S_FadeSound(0)) and the view angles
are cleared (0x00c36428..0x00c36430). A server time behind the last frame's
is then allowed, which only a restart in place can send. True when it ran.
====================
*/
qboolean CL_AliceSnapshotServerCount( int snapFlags ) {
	/* Retail keeps the last frame's flags in a static (0x00532a30), which no
	 * state clear touches. */
	static int lastSnapFlags;
	const bool changed = ( ( snapFlags ^ lastSnapFlags ) & SNAPFLAG_SERVERCOUNT ) != 0;

	lastSnapFlags = snapFlags;
	if ( !changed ) {
		return qfalse;
	}
	S_AliceSoundStopAll();
	CL_AliceUIReleasePause();
	CL_AliceLoadingEnd();
	S_AliceFadeSound( 0.0f );
	VectorClear( cl.viewangles );
	Com_DPrintf( "%s: server count changed\n", Com_GameIsFAKK2() ? "F.A.K.K.2" : "Alice" );
	return qtrue;
}

/*
====================
CL_AliceWidgetPrint

UI_WidgetPrint (0x00410e30): the first desktop widget of that name prints the
text; with none, a warning and the text go to the console.
====================
*/
void CL_AliceWidgetPrint( const char *widget, const char *text ) {
	if ( !widget || !text ) {
		return;
	}
	if ( CL_AliceDialogPrint( widget, text ) ) {
		return;
	}
	Com_Printf( "WARNING: UI_WidgetPrint couldn't find widget named %s\n", widget );
	Com_Printf( "%s", text );
}

/* ---- the engine UI's side of Alice's game imports 106, 108 and 109 ------- */

/* Import 106, WidgetPrintf (0x004389e0): skipped on a dedicated server. */
void Com_AliceWidgetPrint( const char *widget, const char *text ) {
	if ( !Com_GameIsAlice() || ( com_dedicated && com_dedicated->integer ) ) {
		return;
	}
	CL_AliceWidgetPrint( widget, text );
}

/* Import 108, the loading stage (0x0040b590). */
void Com_AliceLoadingStage( int stage ) {
	if ( !Com_GameIsAlice() || ( com_dedicated && com_dedicated->integer ) ) {
		return;
	}
	CL_AliceLoadingSetStage( stage );
}

/* Import 109, UpdateLoadingScreen: the same function as client-game import 19
 * (0x0042d8d0). */
void Com_AliceUpdateLoadingScreen( void ) {
	if ( !Com_GameIsAlice() || ( com_dedicated && com_dedicated->integer ) ) {
		return;
	}
	CL_AliceLoadingScreenRefresh();
}
