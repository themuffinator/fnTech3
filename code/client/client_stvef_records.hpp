/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_RECORDS_HPP
#define FNTECH3_CLIENT_STVEF_RECORDS_HPP
#include "../compat/stvef_game_abi.h"
#include <cstdint>
#include <cstring>

/* Records the Elite Force cgame and UI receive from the client. */

/* Single player's screenshot: a 256x256 RGBA picture that a savegame keeps
 * and the loading screen draws when a level has no levelshot (stvoy.exe
 * keeps one at 0x575034, SCR_GetScreenshot 0x40a3a0). The menus read it and
 * the cgame draws it, so both share this one. */
struct STVEFSPScreenshot {
	std::uint8_t rgba[FNTECH3_STVEF_SP_SCREENSHOT_SIZE * FNTECH3_STVEF_SP_SCREENSHOT_SIZE * 4];
	bool valid;
};

inline STVEFSPScreenshot &CL_STVEFSPScreenshot() {
	static STVEFSPScreenshot screenshot{};
	return screenshot;
}

/* Single player's sound channel as the engine's. The channels the engine
 * knows keep their meaning, local ones playing at the listener; single
 * player's attenuated voice, ambient and menu channels, and any other number,
 * take numbers of their own above the engine's, so that a sound still
 * replaces only the one on its own channel. */
inline int CL_STVEFSPSoundChannel( int channel ) {
	switch ( channel ) {
	case FNTECH3_STVEF_SP_CHAN_AUTO: return CHAN_AUTO;
	case FNTECH3_STVEF_SP_CHAN_LOCAL: return CHAN_LOCAL;
	case FNTECH3_STVEF_SP_CHAN_WEAPON: return CHAN_WEAPON;
	case FNTECH3_STVEF_SP_CHAN_VOICE: return CHAN_VOICE;
	case FNTECH3_STVEF_SP_CHAN_ITEM: return CHAN_ITEM;
	case FNTECH3_STVEF_SP_CHAN_BODY: return CHAN_BODY;
	case FNTECH3_STVEF_SP_CHAN_LOCAL_SOUND: return CHAN_LOCAL_SOUND;
	case FNTECH3_STVEF_SP_CHAN_ANNOUNCER: return CHAN_ANNOUNCER;
	default: return channel < 0 ? channel : CHAN_ANNOUNCER + 1 + channel;
	}
}

// Holomatch's glconfig_t: Quake III 1.17's strings with a 2 KB extension list,
// and an anisotropic filtering flag after the texture environment one.
inline void CL_CopySTVEFGlconfig( fntech3_stvef_mp_glconfig_t *destination, const glconfig_t &source ) {
	fntech3_stvef_mp_glconfig_t config{};
	Q_strncpyz( config.renderer_string, source.renderer_string, sizeof( config.renderer_string ) );
	Q_strncpyz( config.vendor_string, source.vendor_string, sizeof( config.vendor_string ) );
	Q_strncpyz( config.version_string, source.version_string, sizeof( config.version_string ) );
	Q_strncpyz( config.extensions_string, source.extensions_string, sizeof( config.extensions_string ) );
	config.maxTextureSize = source.maxTextureSize;
	config.numTextureUnits = source.numTextureUnits;
	config.colorBits = source.colorBits;
	config.depthBits = source.depthBits;
	config.stencilBits = source.stencilBits;
	config.driverType = source.driverType;
	config.hardwareType = source.hardwareType;
	config.deviceSupportsGamma = source.deviceSupportsGamma;
	config.textureCompression = source.textureCompression;
	config.textureEnvAddAvailable = source.textureEnvAddAvailable;
	// Every renderer this engine loads filters anisotropically.
	config.textureFilterAnisotropicAvailable = 1;
	config.vidWidth = source.vidWidth;
	config.vidHeight = source.vidHeight;
	config.windowAspect = source.windowAspect;
	config.displayFrequency = source.displayFrequency;
	config.isFullscreen = source.isFullscreen;
	config.stereoEnabled = source.stereoEnabled;
	config.smpActive = source.smpActive;
	std::memcpy( destination, &config, sizeof( config ) );
}

// Single player's glconfig names the texture units maxActiveTextures and adds
// clampToEdgeAvailable after the anisotropy flag.
inline void CL_CopySTVEFSPGlconfig( fntech3_stvef_sp_glconfig_t *destination, const glconfig_t &source ) {
	fntech3_stvef_sp_glconfig_t config{};
	Q_strncpyz( config.renderer_string, source.renderer_string, sizeof( config.renderer_string ) );
	Q_strncpyz( config.vendor_string, source.vendor_string, sizeof( config.vendor_string ) );
	Q_strncpyz( config.version_string, source.version_string, sizeof( config.version_string ) );
	Q_strncpyz( config.extensions_string, source.extensions_string, sizeof( config.extensions_string ) );
	config.maxTextureSize = source.maxTextureSize;
	config.maxActiveTextures = source.numTextureUnits;
	config.colorBits = source.colorBits;
	config.depthBits = source.depthBits;
	config.stencilBits = source.stencilBits;
	config.driverType = source.driverType;
	config.hardwareType = source.hardwareType;
	config.deviceSupportsGamma = source.deviceSupportsGamma;
	config.textureCompression = source.textureCompression;
	config.textureEnvAddAvailable = source.textureEnvAddAvailable;
	// Every renderer this engine loads filters anisotropically and clamps to edge.
	config.textureFilterAnisotropicAvailable = 1;
	config.clampToEdgeAvailable = 1;
	config.vidWidth = source.vidWidth;
	config.vidHeight = source.vidHeight;
	config.windowAspect = source.windowAspect;
	config.displayFrequency = source.displayFrequency;
	config.isFullscreen = source.isFullscreen;
	config.stereoEnabled = source.stereoEnabled;
	config.smpActive = source.smpActive;
	std::memcpy( destination, &config, sizeof( config ) );
}

#endif
