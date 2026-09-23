/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_CONSOLE_LOGO_HPP
#define FNTECH3_CLIENT_CONSOLE_LOGO_HPP

namespace fntech3::client {

struct ConsoleLogoRect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

/*
 * Where the console's logo layer goes, in screen pixels. Retail RTCW's
 * Con_DrawSolidConsole (WolfMP.exe 0x4075d0, WolfSP.exe 0x407300) draws it
 * only while the console is at least half open, as
 * SCR_DrawPic( 192, 70, 256, 128 ) on the 640x480 screen: centred, 70 units
 * from the top. Retail stretches that screen to the window. This keeps the
 * rectangle at the uniform 640x480 scale and centres it in the console's own
 * width, which is the same place on a 4:3 screen and keeps the logo's shape
 * on any other. False when the logo is not drawn.
 */
inline bool ConsoleLogoPlacement( float frac, float consoleX, float consoleWidth,
	float scale, ConsoleLogoRect &logo ) {
	if ( !( frac >= 0.5f ) || !( scale > 0.0f ) || !( consoleWidth > 0.0f ) ) {
		return false;
	}

	logo.width = 256.0f * scale;
	logo.height = 128.0f * scale;
	logo.x = consoleX + ( consoleWidth - logo.width ) * 0.5f;
	logo.y = 70.0f * scale;
	return true;
}

} // namespace fntech3::client

#endif
