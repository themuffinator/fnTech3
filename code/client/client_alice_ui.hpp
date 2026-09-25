/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_ALICE_UI_HPP
#define FNTECH3_CLIENT_ALICE_UI_HPP

/* American McGee's Alice's engine-side presentation layer, shared between the
 * client files that implement it:
 *
 *   cl_alice_font.cpp     Ritual fonts, 2D art handles and fills
 *   cl_alice_screen.cpp   the frame composition (retail UI_Update 0x0040c800)
 *                         and the in-game view's overlay (View3D 0x00420900)
 *   cl_alice_loading.cpp  the world-map loading screen and its stages
 *   cl_alice_ui.cpp       the .urc menus, the intro and the hosted UI module
 *   cl_alice_hud.cpp      the HUD's models, bars and foldouts, and subtitles
 *   cl_alice.cpp          the centre and located prints the server sends
 *   cl_fakk2_inventory.cpp  Heavy Metal: F.A.K.K.2's inventory and weapon
 *                         groups, which its HUD's model labels read too
 *
 * Everything here is reached only under Com_GameIsRitual(). Nothing declared
 * in this header is a module or wire contract. */

#include "client.h"
#include "../compat/alice_ritualfont.hpp"
#include "../compat/alice_screen.hpp"
#include "../compat/fakk2_inventory.hpp"

#include <string>

/* ---- cl_alice_font.cpp: fonts and 2D art -------------------------------- */

/* A font registered by name, as retail's UIFont does (R_LoadFont 0x00498160),
 * or -1 for one that did not load. Handles stay valid for the process; the
 * renderer image behind each is registered again after a renderer restart. */
int CL_AliceFontRegister( const char *name );
const fntech3::alice::ritualfont::Font *CL_AliceFontMetrics( int font );

/* UIFont::Print (0x004c54e0): x, y and the scales are in 640x480 units unless
 * noRescale, in which case they are real pixels. */
void CL_AliceFontPrint( int font, const float *rgba, bool noRescale, float x, float y,
	const char *text, int maxLength, float angle, float charSpacing, float lineSpacing,
	float wScale, float hScale );
/* UIFont::PrintJustified (0x004c55a0) in the same units. */
void CL_AliceFontPrintJustified( int font, const float *rgba, bool noRescale,
	const fntech3::alice::ritualfont::Rect &rect, int horizontal, int vertical, const char *text,
	float angle, float charSpacing, float lineSpacing, float wScale, float hScale );
int CL_AliceFontLineHeight( int font );

/* The renderer's art: a shader or image registered as a 2D picture (cached by
 * name until the renderer restarts), and the engine's own white. */
qhandle_t CL_AliceShader( const char *name );
qhandle_t CL_AliceWhite( void );
/* A filled box in real pixels with alpha blending (retail DrawBox, renderer
 * +0x68), leaving the colour unset afterwards. */
void CL_AliceFill( float x, float y, float w, float h, const float *rgba );
void CL_AliceDrawPic( float x, float y, float w, float h, const float *rgba, qhandle_t shader );
/* The renderer's DrawStretchPic, cut to the clip while one is set. */
void CL_RitualStretchPic( float x, float y, float w, float h, float s0, float t0, float s1, float t1,
	qhandle_t shader );
/* A rectangle in pixels (x, y, w, h) that the pictures, fills and unturned
 * glyphs above are cut to, texture coordinates alike, as a renderer scissor
 * cuts them; null ends it. */
void CL_RitualSetClip( const float *rect );
/* While on, the pictures, fills and unturned glyphs above are F.A.K.K.2's HUD
 * menus: after the clip, each is placed by the HUD's aspect rules
 * (cl_hud.cpp, fntech3-hud.json) instead of stretched with the screen, and a
 * model label's view follows them. */
void CL_RitualSetHudLayout( bool on );
bool CL_RitualHudLayout( void );
/* The size of a picture's first image, or 0x0. */
void CL_AliceShaderSize( qhandle_t shader, int *width, int *height );
/* Black over the whole screen (UI_ClearBlack 0x0040c560). */
void CL_AliceClearBlack( void );
/* Retires every cached art handle; called once the renderer has started. */
void CL_AliceArtRendererStarted( void );
/* A 16 by 16 character sheet for this engine's console, built from
 * courier-14's glyphs; 0 if the font or its picture is missing. */
qhandle_t CL_AliceConsoleCharset( void );

/* ---- cl_alice_screen.cpp ------------------------------------------------- */

void CL_AliceScreenInit( void );
void CL_AliceScreenShutdown( void );
void CL_AliceSetCenterPrint( const char *text, float alpha );
void CL_AliceSetLocationPrint( int x, int y, const char *text, float alpha, int w, int h );
void CL_AliceClearPrints( void );

/* ---- cl_alice_loading.cpp ------------------------------------------------ */

void CL_AliceLoadingInit( void );
bool CL_AliceLoadingActive( void );
void CL_AliceLoadingBegin( void );
void CL_AliceLoadingEnd( void );
void CL_AliceLoadingSetStage( int stage );
void CL_AliceDrawLoadingScreen( void );

/* ---- cl_alice_ui.cpp ----------------------------------------------------- */

/* Once a frame, before anything is drawn: the intro's timer, the menus'
 * waits and motion, the pause a full-screen menu holds, and which of the
 * engine's input catchers the menus own. */
void CL_AliceUIFrame( void );
/* The current menu, whether it covers the view, and the desktop draw. */
bool CL_AliceUIFullscreenMenuUp( void );
bool CL_AliceUIMenuIsUp( void );
void CL_AliceUIDrawMenus( void );
/* F.A.K.K.2 with a full-screen menu up and `ui_menuDissolve` set: moves the
 * shares of the image its retail engine's uncleared screen keeps under its
 * menus, once a frame, and returns whether the game's view goes under them.
 * `viewShown` is whether the last frame showed the view. The end call forgets
 * the kept image. */
bool CL_RitualMenuCarry( bool viewShown );
void CL_RitualEndMenuCarry( void );
void CL_AliceUIDrawIntroPlate( void );
void CL_AliceUIDrawCursor( void );
/* UI_EndLoad (0x0040ecf0): any pause is released, whoever took it, and the
 * menus' pause state is forgotten. */
void CL_AliceUIReleasePause( void );
/* Force every menu away at once (ClearMenus(1), 0x0040d540). */
void CL_AliceUIClearMenus( bool force );
/* The intro state (0x005315f8): -1 once a map has loaded. */
int CL_AliceUIIntroState( void );
void CL_AliceUISetIntroStateInternal( int state );

/* ---- cl_cin.cpp ---------------------------------------------------------- */

/* CIN_Play (0x004272e0), which the intro calls directly for its logos, as
 * retail's _intro does, and the `cinematic` command reaches too. The flags are
 * retail's: '1' holds the last frame, '2' and a character resume the intro at
 * that character less '0' when the movie ends. Nothing loops. */
void CL_AliceCinematicPlay( const char *name, const char *flags );

/* ---- the title's own screen ----------------------------------------------- */

/* Heavy Metal: F.A.K.K.2 runs on this same engine UI with its own art. Its
 * widgets name no font, so the default carries the menus: Alice's engine
 * defaults to courier-14, which F.A.K.K.2 does not ship, and F.A.K.K.2's
 * resident shader list (scripts/static_shaders.txt) names verdana-12 and
 * verdana-14 as the two fonts its engine keeps loaded -- the pair Ritual's
 * later UI library defaults to, verdana-14 for widgets. */
const char *CL_RitualDefaultFont( void );
/* The soundtrack the main menu plays: Alice's `sound/music/intro`, and for
 * F.A.K.K.2 the `music/intro.mus` its menus have and no level names. */
const char *CL_RitualMenuSoundtrack( void );

/* ---- cl_alice_hud.cpp ---------------------------------------------------- */

/* ui_hud's flags. Alice's is an archived cvar (alice.exe 0x00410564).
 * F.A.K.K.2's is a flag of the process that starts at 1 (fakk2.exe 1.02
 * 0x004df7b8); only its command changes it, and nothing saves it. */
inline int CL_RitualUiHudFlags( void ) {
	return Com_GameIsFAKK2() ? 0 : CVAR_ARCHIVE;
}

void CL_AliceHudInit( void );
void CL_AliceHudShutdown( void );
/* The HUD's visibility rule and draw (UI_Update 0x0040c93a-0x0040c9ab). */
void CL_AliceHudFrame( bool allowed );
void CL_AliceHudDraw( void );
/* The subtitle widget, `aliceDialog`, and its commands. */
bool CL_AliceDialogPrint( const char *widget, const char *text );
void CL_AliceDialogDraw( void );

/* ---- the player state's slots, by title ------------------------------------ */

/* The stats the engine itself reads: Alice keeps its cinematic flags, fade
 * kind and letterbox at 5, 6 and 7 (UI_Update 0x0040ca29, View3D 0x00420620
 * and 0x00420570); F.A.K.K.2's game module at 14, 15 and 16
 * (fntech3::fakk2::hud). Both fill bit 0 of the first for a cinematic, and
 * scale the last by 32767. */
inline int CL_RitualCinematicStat( void ) {
	return Com_GameIsFAKK2() ? fntech3::fakk2::hud::StatCinematic : 5;
}
inline int CL_RitualFadeStat( void ) {
	return Com_GameIsFAKK2() ? fntech3::fakk2::hud::StatAddFade : 6;
}
inline int CL_RitualLetterboxStat( void ) {
	return Com_GameIsFAKK2() ? fntech3::fakk2::hud::StatLetterbox : 7;
}
inline bool CL_RitualInCinematic( void ) {
	return ( cl.snap.ps.alice.block[0][CL_RitualCinematicStat()] & 1 ) != 0;
}
/* The configstring of an item index (Alice's items at 0x400, F.A.K.K.2's
 * CS_ITEMS at 896), or null outside the configstrings or for an empty one. */
const char *CL_RitualItemName( int item );

/* ---- cl_alice_ui.cpp: F.A.K.K.2's HUD menus -------------------------------- */

/* Which of F.A.K.K.2's HUD menus show, by its rule, while `allowed`
 * (CL_AliceHudFrame's gate); they draw with the menus. */
void CL_RitualHudMenusFrame( bool allowed );

/* ---- cl_fakk2_inventory.cpp ------------------------------------------------ */

/* The inventory's commands and the script they read (UI_Init's inv_restart
 * 0x004105f2); F.A.K.K.2 only, its registration outliving UI restarts. */
void CL_Fakk2InventoryInit( void );
void CL_Fakk2InventoryShutdown( void );
bool CL_Fakk2InventoryIsOpen( void );
/* Opened by `inventory`, which turns the cursor on (0x00408570). */
bool CL_Fakk2InventoryWantsCursor( void );
/* UI_CloseInventory (0x004088b0); returns whether one was open. */
bool CL_Fakk2InventoryClose( void );
/* Its timeout and its closing in a cinematic (UI_Update 0x0040c817). */
void CL_Fakk2InventoryFrame( void );
void CL_Fakk2InventoryDraw( void );
/* A key pressed while it is open (FakkInventory::KeyEvent 0x004091b0). */
void CL_Fakk2InventoryKey( int key );
/* The cursor's button presses and motion, in the menus' units. */
void CL_Fakk2InventoryMouseDown( int key, float x, float y );
void CL_Fakk2InventoryMouseMoved( float x, float y );
/* The item of the master inventory named so, or null (0x00407130). */
const fntech3::fakk2::inventory::Item *CL_Fakk2FindInventoryItem( const char *name );
/* UI_Render3DModel (0x00408210) over a rectangle in the menus' units. */
void CL_Fakk2RenderModel( float x, float y, float w, float h, const char *model, const char *anim,
	const fntech3::fakk2::hud::View &view, bool moving );

#endif
