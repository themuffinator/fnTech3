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
 *
 * Everything here is reached only under Com_GameIsAlice(). Nothing declared
 * in this header is a module or wire contract. */

#include "client.h"
#include "../compat/alice_ritualfont.hpp"
#include "../compat/alice_screen.hpp"

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

/* ---- cl_alice_hud.cpp ---------------------------------------------------- */

void CL_AliceHudInit( void );
void CL_AliceHudShutdown( void );
/* The HUD's visibility rule and draw (UI_Update 0x0040c93a-0x0040c9ab). */
void CL_AliceHudFrame( bool allowed );
void CL_AliceHudDraw( void );
/* The subtitle widget, `aliceDialog`, and its commands. */
bool CL_AliceDialogPrint( const char *widget, const char *text );
void CL_AliceDialogDraw( void );

#endif
