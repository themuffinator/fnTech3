/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_SP_SCREEN_HPP
#define FNTECH3_CLIENT_STVEF_SP_SCREEN_HPP
#include "../compat/stvef_sp_savegame.hpp"
#include "client_stvef_records.hpp"

/*
 * The picture Elite Force single player keeps of the screen. Retail takes it
 * whenever a level is being played and no menu is up (SCR_PrecacheScreenshot
 * 0x40a360), keeps the one picture (0x575034) and hands it to the savegames,
 * the menus and the loading screen. Retail reads the frame that was last
 * shown; fnTech3 asks the renderer for the next frame instead and draws one,
 * so the picture is of a frame that was drawn whole.
 */

// The savegame's own name for a level's picture (SG_WriteScreenshot 0x427690).
static const char *CL_STVEFSPLevelshotPath( const char *map, char *path, int size ) {
	Com_sprintf( path, size, "levelshots/%s.tga", map ? map : "" );
	return path;
}

void CL_STVEFSPUnprecacheScreenshot( void ) { CL_STVEFSPScreenshot().valid = false; }

void CL_STVEFSPPrecacheScreenshot( void ) {
	static bool capturing;
	STVEFSPScreenshot &screenshot = CL_STVEFSPScreenshot();
	if ( capturing || cls.state != CA_ACTIVE || Key_GetCatcher() != 0 || !cls.rendererStarted ) {
		return;
	}
	capturing = true;
	// Retail silences the sound buffer first, so that reading the screen
	// cannot leave it looping (0x4209e0).
	S_ClearSoundBuffer();
	qboolean taken = qfalse;
	re.CaptureSTVEFThumbnail( screenshot.rgba, FNTECH3_STVEF_SP_SCREENSHOT_SIZE, &taken );
	SCR_UpdateScreen();
	// A frame the renderer did not draw leaves the picture as it was.
	re.CaptureSTVEFThumbnail( nullptr, 0, nullptr );
	if ( taken ) {
		screenshot.valid = true;
	}
	capturing = false;
}

/* The kept picture, taken now if there is none (SCR_GetScreenshot 0x40a3a0). */
const byte *CL_STVEFSPScreenshotPixels( void ) {
	STVEFSPScreenshot &screenshot = CL_STVEFSPScreenshot();
	if ( !screenshot.valid ) {
		CL_STVEFSPPrecacheScreenshot();
	}
	return screenshot.rgba;
}

qboolean CL_STVEFSPScreenshotValid( void ) { return CL_STVEFSPScreenshot().valid ? qtrue : qfalse; }

/* A savegame's picture becomes the kept one while its level loads; a picture
 * of another size clears it (SCR_SetScreenshot 0x40a3d0). */
void CL_STVEFSPSetScreenshot( const byte *pixels, int width, int height ) {
	STVEFSPScreenshot &screenshot = CL_STVEFSPScreenshot();
	if ( !pixels || width != FNTECH3_STVEF_SP_SCREENSHOT_SIZE || height != FNTECH3_STVEF_SP_SCREENSHOT_SIZE ) {
		std::memset( screenshot.rgba, 0, sizeof( screenshot.rgba ) );
		screenshot.valid = false;
		return;
	}
	std::memcpy( screenshot.rgba, pixels, sizeof( screenshot.rgba ) );
	screenshot.valid = true;
}

/* A level's own picture for an autosave: its levelshot fitted to the
 * savegame's size and turned over, since a savegame keeps its picture the way
 * the screen is read (0x427690 through 0x44ff50). */
qboolean CL_STVEFSPLevelshot( const char *map, byte *pixels ) {
	char path[MAX_QPATH];
	int width = 0, height = 0;
	if ( !pixels || !cls.rendererStarted ) {
		return qfalse;
	}
	byte *image = re.LoadImagePixels( CL_STVEFSPLevelshotPath( map, path, sizeof( path ) ), &width, &height );
	if ( !image ) {
		return qfalse;
	}
	if ( width == FNTECH3_STVEF_SP_SCREENSHOT_SIZE && height == FNTECH3_STVEF_SP_SCREENSHOT_SIZE ) {
		std::memcpy( pixels, image, FNTECH3_STVEF_SP_SCREENSHOT_SIZE * FNTECH3_STVEF_SP_SCREENSHOT_SIZE * 4 );
	} else {
		fntech3::stvef::savegame::Resample( image, width, height, pixels,
			FNTECH3_STVEF_SP_SCREENSHOT_SIZE, FNTECH3_STVEF_SP_SCREENSHOT_SIZE );
	}
	Z_Free( image );
	fntech3::stvef::savegame::FlipRows( pixels, FNTECH3_STVEF_SP_SCREENSHOT_SIZE, FNTECH3_STVEF_SP_SCREENSHOT_SIZE );
	return qtrue;
}

#endif
