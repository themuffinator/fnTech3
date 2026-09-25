/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* American McGee's Alice's loading screen: a map of Wonderland with the
 * current zone lit, the regions still ahead covered, a row of glowing dots for
 * the loading stage and the map's title from `ui/maplist.scr`.
 *
 * Retail, all in alice.exe:
 *
 *   UI_BeginLoad     0x0040ed20  (from CL_MapLoading 0x0042aa97) sets the
 *                                loading flag 0x006a006c
 *   UI_EndLoad       0x0040ecf0  (from CL_FirstSnapshot 0x00424ab3, the drop
 *                                path 0x0042ac55) clears it and releases any
 *                                pause a menu took
 *   loading stage    0x0040b590  game import 108; also CM_LoadMap (0 and 1)
 *                                and the renderer's world load (7 and 8)
 *   the draw         0x0040c8c4  world map 0x0040b250, zone art 0x0040b9d0,
 *                                region art 0x0040bc20, progress 0x0040be20
 *   UI_LoadMapList   0x0040b5a0  `zone scale mapname "Title"` rows
 *
 * The drawing is in 640x480 units stretched to the screen, as the view's
 * frame is. While the flag is set the loading screen owns the frame: nothing
 * else is drawn (UI_Update 0x0040c8a0). */

#include "client_alice_ui.hpp"

namespace {

namespace screen = fntech3::alice::screen;

bool loadingActive;
int loadingStage;
screen::MapList mapList;
bool mapListRead;
int titleFont = -1;

void ReadMapList() {
	if ( mapListRead ) {
		return;
	}
	mapListRead = true;
	mapList = screen::MapList();
	void *buffer = nullptr;
	const int length = FS_ReadFile( "ui/maplist.scr", &buffer );
	if ( length < 0 || !buffer ) {
		Com_DPrintf( "Alice: no ui/maplist.scr; the loading screen names no map\n" );
		return;
	}
	mapList = screen::ParseMapList( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ) );
	FS_FreeFile( buffer );
	if ( !mapList.error.empty() ) {
		Com_Printf( S_COLOR_YELLOW "%s\n", mapList.error.c_str() );
	}
}

/* The map being loaded: the server's map name up to any `$` spawn target.
 * Retail copies it when the server spawns (0x00b59b80); this client learns it
 * from the gamestate's server info, so a frame drawn before the gamestate
 * arrives shows the map without a zone or title, as retail draws an unknown
 * map. */
std::string LoadingMapName() {
	if ( cls.state < CA_CONNECTED || !cl.gameState.dataCount ) {
		return std::string();
	}
	const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
	return screen::MapNameForLoading( Info_ValueForKey( info, "mapname" ) );
}

} // namespace

void CL_AliceLoadingInit( void ) {
	mapListRead = false;
	titleFont = -1;
	// Heavy Metal: F.A.K.K.2's loading screen has no map list or title.
	if ( Com_GameIsFAKK2() ) {
		return;
	}
	ReadMapList();
	titleFont = CL_AliceFontRegister( screen::LoadingTitleFont );
}

bool CL_AliceLoadingActive( void ) {
	return loadingActive;
}

void CL_AliceLoadingBegin( void ) {
	loadingStage = 0;
	loadingActive = true;
}

void CL_AliceLoadingEnd( void ) {
	loadingActive = false;
}

void CL_AliceLoadingSetStage( int stage ) {
	loadingStage = stage;
}

/*
====================
CL_AliceDrawLoadingScreen
====================
*/
void CL_AliceDrawLoadingScreen( void ) {
	/* Heavy Metal: F.A.K.K.2 has no world map. Its UI registers
	 * textures/menu/loading as it starts (fakk2.exe 1.02 0x0040ece7); while a
	 * level loads it fills the screen black (0x0040acd0) and draws the picture
	 * white at its own size, centred on the view's full-screen frame
	 * (0x0040b14c..0x0040b222). Retail counts that size in pixels; this client
	 * scales it with the window, as it does the menus. Both languages' pictures
	 * are 256 by 256, pulsed by a second additive pass in scripts/menu.shader. */
	if ( Com_GameIsFAKK2() ) {
		static const float white[4] = { 1, 1, 1, 1 };
		const qhandle_t loading = CL_AliceShader( "textures/menu/loading" );
		int width = 0, height = 0;
		if ( re.GetShaderImageSize ) re.GetShaderImageSize( loading, &width, &height );
		const float w = width * ( cls.glconfig.vidWidth / 640.0f );
		const float h = height * ( cls.glconfig.vidHeight / 480.0f );
		CL_AliceClearBlack();
		if ( width > 0 && height > 0 ) {
			CL_AliceDrawPic( ( cls.glconfig.vidWidth - w ) * 0.5f, ( cls.glconfig.vidHeight - h ) * 0.5f, w, h, white,
				loading );
		}
		return;
	}
	ReadMapList();
	const float sx = cls.glconfig.vidWidth / 640.0f;
	const float sy = cls.glconfig.vidHeight / 480.0f;
	static const float white[4] = { 1, 1, 1, 1 };

	for ( const screen::Tile &tile : screen::WorldMapTiles ) {
		CL_AliceDrawPic( tile.x * sx, tile.y * sy, tile.w * sx, tile.h * sy, white, CL_AliceShader( tile.image ) );
	}

	const std::string map = LoadingMapName();
	const int entry = map.empty() ? -1 : mapList.Find( map.c_str() );

	if ( entry >= 0 ) {
		const int zone = mapList.entries[entry].zone - 1;
		if ( zone < 0 || zone >= static_cast<int>( sizeof( screen::Zones ) / sizeof( screen::Zones[0] ) ) ) {
			Com_Printf( "UI_DrawZoneText( %i ) Zone out of range.\n", zone );
		} else {
			// Every zone half lit, the current one fully (0x0040ba22).
			for ( int i = 0; i < static_cast<int>( sizeof( screen::Zones ) / sizeof( screen::Zones[0] ) ); ++i ) {
				const screen::Zone &art = screen::Zones[i];
				const float color[4] = { 1, 1, 1, i == zone ? 1.0f : screen::ZoneDimAlpha };
				CL_AliceDrawPic( art.x * sx, art.y * sy, screen::ZoneSize * sx, screen::ZoneSize * sy, color,
					CL_AliceShader( art.image ) );
			}
		}
		// The regions beyond the current zone stay covered.
		for ( const screen::Region &region : screen::Regions ) {
			if ( region.zone > mapList.entries[entry].zone ) {
				CL_AliceDrawPic( region.x * sx, region.y * sy, region.w * sx, region.h * sy, white,
					CL_AliceShader( region.image ) );
			}
		}
	}

	CL_AliceDrawPic( screen::IndicatorRect[0] * sx, screen::IndicatorRect[1] * sy,
		screen::IndicatorRect[2] * sx, screen::IndicatorRect[3] * sy, white, CL_AliceShader( screen::IndicatorImage ) );
	const qhandle_t glow = CL_AliceShader( screen::IndicatorGlowImage );
	const int dots = screen::ClampLoadingStage( loadingStage );
	for ( int i = 0; i < dots; ++i ) {
		CL_AliceDrawPic( screen::LoadingDotX[i] * sx, screen::LoadingDotY * sy,
			screen::LoadingDotSize * sx, screen::LoadingDotSize * sy, white, glow );
	}

	if ( entry >= 0 ) {
		const screen::MapListEntry &row = mapList.entries[entry];
		if ( titleFont < 0 ) {
			titleFont = CL_AliceFontRegister( screen::LoadingTitleFont );
		}
		CL_AliceFontPrint( titleFont, screen::LoadingTitleColor, false, screen::LoadingTitleX,
			screen::LoadingTitleY, row.title.c_str(), -1, 0.0f, 1.0f, 0.0f, row.scale, row.scale );
	}
}
