/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_RECORDS_HPP
#define FNTECH3_CLIENT_STVEF_RECORDS_HPP
#include "../compat/stvef_game_abi.h"
#include <cstring>

/* Records the Holomatch cgame and UI both receive from the client. */

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
