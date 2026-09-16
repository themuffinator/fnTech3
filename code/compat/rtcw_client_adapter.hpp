/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_CLIENT_ADAPTER_HPP
#define FNTECH3_RTCW_CLIENT_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "../ui/ui_public.h"
#include "rtcw_game_abi.h"
#include <cstdint>
#include <cstring>

namespace fntech3::rtcw::client {
namespace detail {

template<std::size_t N, std::size_t M>
void Text( char ( &destination )[N], const char ( &source )[M] ) noexcept {
	static_assert( N > 0 && M > 0 );
	std::size_t index = 0;
	while ( index + 1 < N && index < M && source[index] ) {
		destination[index] = source[index];
		++index;
	}
	destination[index] = '\0';
}

template<class T>
bool Ordinal( const T &source, std::int32_t &destination, std::uint32_t maximum ) noexcept {
	static_assert( sizeof( T ) == sizeof( std::uint32_t ) );
	std::uint32_t bits;
	std::memcpy( &bits, &source, sizeof( bits ) );
	if ( bits > maximum ) return false;
	destination = static_cast<std::int32_t>( bits );
	return true;
}

} // namespace detail

inline bool FromInternal( const uiClientState_t &source, fntech3_rtcw_uiClientState_t &destination ) noexcept {
#define RTCW_CONNECTION_STATE(name) static_assert( static_cast<int>(name) == FNTECH3_RTCW_##name );
	RTCW_CONNECTION_STATE(CA_UNINITIALIZED)
	RTCW_CONNECTION_STATE(CA_DISCONNECTED)
	RTCW_CONNECTION_STATE(CA_AUTHORIZING)
	RTCW_CONNECTION_STATE(CA_CONNECTING)
	RTCW_CONNECTION_STATE(CA_CHALLENGING)
	RTCW_CONNECTION_STATE(CA_CONNECTED)
	RTCW_CONNECTION_STATE(CA_LOADING)
	RTCW_CONNECTION_STATE(CA_PRIMED)
	RTCW_CONNECTION_STATE(CA_ACTIVE)
	RTCW_CONNECTION_STATE(CA_CINEMATIC)
#undef RTCW_CONNECTION_STATE
	fntech3_rtcw_uiClientState_t result{};
	if ( !detail::Ordinal( source.connState, result.connState, FNTECH3_RTCW_CA_CINEMATIC ) ) return false;
	result.connectPacketCount = source.connectPacketCount;
	result.clientNum = source.clientNum;
	detail::Text( result.servername, source.servername );
	detail::Text( result.updateInfoString, source.updateInfoString );
	detail::Text( result.messageString, source.messageString );
	destination = result;
	return true;
}

inline bool FromInternal( const glconfig_t &source, fntech3_rtcw_glconfig_t &destination ) noexcept {
#define RTCW_RENDER_VALUE(name) static_assert( static_cast<int>(name) == FNTECH3_RTCW_##name );
	RTCW_RENDER_VALUE(GLDRV_ICD)
	RTCW_RENDER_VALUE(GLDRV_STANDALONE)
	RTCW_RENDER_VALUE(GLDRV_VOODOO)
	RTCW_RENDER_VALUE(GLHW_GENERIC)
	RTCW_RENDER_VALUE(GLHW_3DFX_2D3D)
	RTCW_RENDER_VALUE(GLHW_RIVA128)
	RTCW_RENDER_VALUE(GLHW_RAGEPRO)
	RTCW_RENDER_VALUE(GLHW_PERMEDIA2)
	RTCW_RENDER_VALUE(TC_NONE)
	RTCW_RENDER_VALUE(TC_S3TC)
#undef RTCW_RENDER_VALUE
	static_assert( static_cast<int>( TC_S3TC_ARB ) == FNTECH3_RTCW_TC_EXT_COMP_S3TC );
	fntech3_rtcw_glconfig_t result{};
	if ( !detail::Ordinal( source.driverType, result.driverType, FNTECH3_RTCW_GLDRV_VOODOO ) ||
		!detail::Ordinal( source.hardwareType, result.hardwareType, FNTECH3_RTCW_GLHW_PERMEDIA2 ) ||
		!detail::Ordinal( source.textureCompression, result.textureCompression, FNTECH3_RTCW_TC_EXT_COMP_S3TC ) ||
		!detail::Ordinal( source.deviceSupportsGamma, result.deviceSupportsGamma, 1 ) ||
		!detail::Ordinal( source.textureEnvAddAvailable, result.textureEnvAddAvailable, 1 ) ||
		!detail::Ordinal( source.isFullscreen, result.isFullscreen, 1 ) ||
		!detail::Ordinal( source.stereoEnabled, result.stereoEnabled, 1 ) ||
		!detail::Ordinal( source.smpActive, result.smpActive, 1 ) ) return false;
	detail::Text( result.renderer_string, source.renderer_string );
	detail::Text( result.vendor_string, source.vendor_string );
	detail::Text( result.version_string, source.version_string );
	detail::Text( result.extensions_string, source.extensions_string );
	result.maxTextureSize = source.maxTextureSize;
	result.maxActiveTextures = source.numTextureUnits;
	result.colorBits = source.colorBits;
	result.depthBits = source.depthBits;
	result.stencilBits = source.stencilBits;
	result.vidWidth = source.vidWidth;
	result.vidHeight = source.vidHeight;
	result.windowAspect = source.windowAspect;
	result.displayFrequency = source.displayFrequency;
	// The shared renderer contract does not report legacy vendor extensions or
	// anisotropy capabilities. Leave them unavailable until it supplies evidence.
	destination = result;
	return true;
}

inline bool FromInternal( const pc_token_t &source, fntech3_rtcw_pc_token_t &destination ) noexcept {
	static_assert( TT_STRING == FNTECH3_RTCW_TT_STRING && TT_LITERAL == FNTECH3_RTCW_TT_LITERAL &&
		TT_NUMBER == FNTECH3_RTCW_TT_NUMBER && TT_NAME == FNTECH3_RTCW_TT_NAME &&
		TT_PUNCTUATION == FNTECH3_RTCW_TT_PUNCTUATION );
	if ( source.type < FNTECH3_RTCW_TT_STRING || source.type > FNTECH3_RTCW_TT_PUNCTUATION ) return false;
	fntech3_rtcw_pc_token_t result{};
	result.type = source.type;
	result.subtype = source.subtype;
	result.intvalue = source.intvalue;
	result.floatvalue = source.floatvalue;
	detail::Text( result.string, source.string );
	destination = result;
	return true;
}

inline bool FromInternal( const fontInfo_t &source, fntech3_rtcw_fontInfo_t &destination ) noexcept {
	static_assert( GLYPHS_PER_FONT == 256 );
	fntech3_rtcw_fontInfo_t result{};
	for ( std::size_t index = 0; index < 256; ++index ) {
		const auto &input = source.glyphs[index];
		auto &output = result.glyphs[index];
		output.height = input.height;
		output.top = input.top;
		output.bottom = input.bottom;
		output.pitch = input.pitch;
		output.xSkip = input.xSkip;
		output.imageWidth = input.imageWidth;
		output.imageHeight = input.imageHeight;
		output.s = input.s; output.t = input.t; output.s2 = input.s2; output.t2 = input.t2;
		output.glyph = input.glyph;
		detail::Text( output.shaderName, input.shaderName );
	}
	result.glyphScale = source.glyphScale;
	detail::Text( result.name, source.name );
	destination = result;
	return true;
}

} // namespace fntech3::rtcw::client
#endif
