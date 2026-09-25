/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

American McGee's Alice's savegame picture

Retail alice.exe's server asks for it as the last step of every save, with
EXEC_NOW "saveshot save/<config>_<name>.tga 256 256" (0x00451983). Its client
command `saveshot` (0x0042d570) does nothing until the renderer has started;
otherwise it hides the console, the menus and the HUD for one frame, draws
that frame and runs the renderer's `screenshot` with the same arguments. With
a path and both sizes, the renderer reads the whole frame back and reduces it
to a grey picture of that size, written as an uncompressed TGA
(code/compat/alice_saveshot.h). Heavy Metal: F.A.K.K.2's renderer reduces it
the same way in colour.

This engine's renderer takes the picture from its own render target as the
frame finishes (re.CaptureAliceSaveshot), so the image is the frame the
engine drew, never a capture of the desktop. Hiding the overlays is the
engine UI's part: its `saveshot` handler asks for a view-only frame and then
calls CL_AliceSaveshotCommand.

See docs/fntech3/2026-09-23-alice-savegames.md.
===========================================================================
*/

#include "client.h"
#include "../compat/alice_savegame.hpp"

#include <cstdlib>
#include <vector>

namespace {

namespace alicesave = fntech3::alice::savegame;

/* Retail sizes its buffer from the arguments alone; this bounds them. */
constexpr int kAliceSaveshotLargest = 4096;

bool aliceSaveshotDrawing;

} // namespace

/*
====================
CL_AliceWriteSaveshot

Draw one frame and keep it as a width by height TGA at path, in the home path:
grey for Alice, in colour for F.A.K.K.2. False, with nothing written, when there is no renderer, when the
renderer cannot take the picture, or when it drew no frame.
====================
*/
qboolean CL_AliceWriteSaveshot( const char *path, int width, int height ) {
	if ( !path || !path[0] || width <= 0 || height <= 0
		|| width > kAliceSaveshotLargest || height > kAliceSaveshotLargest ) {
		Com_Printf( "saveshot: %s at %i by %i is not a picture this engine takes\n",
			path ? path : "(null)", width, height );
		return qfalse;
	}
	if ( !cls.rendererStarted || !re.CaptureAliceSaveshot || aliceSaveshotDrawing ) {
		return qfalse;
	}

	const std::size_t pixels = static_cast<std::size_t>( width ) * height * 3u;
	std::vector<byte> file( alicesave::TgaHeaderBytes + pixels );
	qboolean taken = qfalse;

	aliceSaveshotDrawing = true;
	re.CaptureAliceSaveshot( file.data() + alicesave::TgaHeaderBytes, width, height, &taken );
	SCR_UpdateScreen();
	// A frame the renderer did not draw leaves nothing to write.
	re.CaptureAliceSaveshot( nullptr, 0, 0, nullptr );
	aliceSaveshotDrawing = false;

	if ( !taken ) {
		Com_Printf( "saveshot: no frame was drawn for %s\n", path );
		return qfalse;
	}
	FNTECH3_AliceTgaHeader( file.data(), width, height );
	FS_WriteFile( path, file.data(), static_cast<int>( file.size() ) );
	Com_DPrintf( "Wrote %s (%ix%i)\n", path, width, height );
	return qtrue;
}

/*
====================
CL_AliceSaveshotCommand

`saveshot <path> <width> <height>`: the renderer's `screenshot` form retail
forwards to. A name with a slash is used as it is and any other becomes
screenshots/<name>.tga (0x004746b0); with both sizes the picture is Alice's
grey reduction, and without them it is an ordinary screenshot, which this
engine's own `screenshot` command takes.
====================
*/
void CL_AliceSaveshotCommand( void ) {
	if ( !cls.rendererStarted ) {
		return;
	}
	const int width = Cmd_Argc() > 2 ? atoi( Cmd_Argv( 2 ) ) : 0;
	const int height = Cmd_Argc() > 3 ? atoi( Cmd_Argv( 3 ) ) : 0;
	if ( Cmd_Argc() < 2 || !width || !height ) {
		Cbuf_ExecuteText( EXEC_NOW, va( "screenshot %s\n", Cmd_ArgsFrom( 1 ) ) );
		return;
	}

	char path[MAX_QPATH];
	const char *name = Cmd_Argv( 1 );
	if ( strchr( name, '/' ) ) {
		Q_strncpyz( path, name, sizeof( path ) );
	} else {
		Com_sprintf( path, sizeof( path ), "screenshots/%s.tga", name );
	}
	CL_AliceWriteSaveshot( path, width, height );
}
