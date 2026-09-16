/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RENDER_RECORD_ADAPTER_HPP
#define FNTECH3_RENDER_RECORD_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "../qcommon/game_profile.h"
#include "../renderercommon/tr_types.h"
#include "q3_game_abi.h"
#include "ql_game_abi.h"
#include "rtcw_game_abi.h"
#include <cstddef>
#include <cstring>
#include <iterator>
#include <type_traits>

namespace fntech3::render {
namespace detail {

template<class D, class S>
void Bytes( D &destination, const S &source ) noexcept {
	static_assert( sizeof( D ) == sizeof( S ) );
	static_assert( std::is_trivially_copyable_v<D> && std::is_trivially_copyable_v<S> );
	std::memcpy( &destination, &source, sizeof( destination ) );
}

template<class T>
bool LegacyEntity( const T &source, refEntity_t &destination ) noexcept {
	// These are Q3/QL module ordinals, independent of the canonical enum order.
	constexpr refEntityType_t types[] = { RT_MODEL, RT_POLY, RT_SPRITE, RT_BEAM,
		RT_RAIL_CORE, RT_RAIL_RINGS, RT_LIGHTNING, RT_PORTALSURFACE };
	if ( source.reType < 0 || static_cast<std::size_t>( source.reType ) >= std::size( types ) ||
		( source.nonNormalizedAxes != 0 && source.nonNormalizedAxes != 1 ) ) return false;
	refEntity_t result{};
	result.reType = types[source.reType];
	result.renderfx = source.renderfx;
	result.hModel = source.hModel;
	Bytes( result.lightingOrigin, source.lightingOrigin );
	result.shadowPlane = source.shadowPlane;
	Bytes( result.axis, source.axis );
	result.nonNormalizedAxes = source.nonNormalizedAxes ? qtrue : qfalse;
	Bytes( result.origin, source.origin );
	result.frame = source.frame;
	Bytes( result.oldorigin, source.oldorigin );
	result.oldframe = source.oldframe;
	result.backlerp = source.backlerp;
	result.skinNum = source.skinNum;
	result.customSkin = source.customSkin;
	result.customShader = source.customShader;
	Bytes( result.shader, source.shaderRGBA );
	Bytes( result.shaderTexCoord, source.shaderTexCoord );
	Bytes( result.shaderTime, source.shaderTimeBits );
	result.radius = source.radius;
	result.rotation = source.rotation;
	destination = result;
	return true;
}

template<class T>
bool LegacyRefdef( const T &source, refdef_t &destination ) noexcept {
	refdef_t result{};
	result.x = source.x; result.y = source.y;
	result.width = source.width; result.height = source.height;
	result.fov_x = source.fov_x; result.fov_y = source.fov_y;
	Bytes( result.vieworg, source.vieworg );
	Bytes( result.viewaxis, source.viewaxis );
	result.time = source.time;
	result.rdflags = source.rdflags;
	Bytes( result.areamask, source.areamask );
	Bytes( result.text, source.text );
	destination = result;
	return true;
}

template<class T>
bool RTCWEntity( const T &source, refEntity_t &destination ) noexcept {
	const auto &common = source.common;
	const auto &appearance = source.appearance;
	constexpr refEntityType_t types[] = { RT_MODEL, RT_POLY, RT_SPRITE, RT_SPLASH,
		RT_BEAM, RT_RAIL_CORE, RT_RAIL_CORE_TAPER, RT_RAIL_RINGS, RT_LIGHTNING, RT_PORTALSURFACE };
	if ( common.reType < 0 || static_cast<std::size_t>( common.reType ) >= std::size( types ) ||
		( common.nonNormalizedAxes != 0 && common.nonNormalizedAxes != 1 ) ) return false;
	refEntity_t result{};
	result.reType = types[common.reType];
	// RTCW aliases highlight/shadow-plane and blink/wrap-frames. Preserve both
	// meanings without allowing unknown external bits to acquire engine effects.
	constexpr struct { int external, internal; } flags[] = {
		{ FNTECH3_RTCW_RF_MINLIGHT, RF_MINLIGHT }, { FNTECH3_RTCW_RF_THIRD_PERSON, RF_THIRD_PERSON },
		{ FNTECH3_RTCW_RF_FIRST_PERSON, RF_FIRST_PERSON }, { FNTECH3_RTCW_RF_DEPTHHACK, RF_DEPTHHACK },
		{ FNTECH3_RTCW_RF_NOSHADOW, RF_NOSHADOW }, { FNTECH3_RTCW_RF_LIGHTING_ORIGIN, RF_LIGHTING_ORIGIN },
		{ FNTECH3_RTCW_RF_SHADOW_PLANE, RF_SHADOW_PLANE }, { FNTECH3_RTCW_RF_WRAP_FRAMES, RF_WRAP_FRAMES }
	};
	for ( const auto &flag : flags ) if ( common.renderfx & flag.external ) result.renderfx |= flag.internal;
	result.rtcw.highlight = common.renderfx & FNTECH3_RTCW_RF_HILIGHT ? qtrue : qfalse;
	result.rtcw.blink = common.renderfx & FNTECH3_RTCW_RF_BLINK ? qtrue : qfalse;
	result.hModel = common.hModel;
	Bytes( result.lightingOrigin, common.lightingOrigin );
	result.shadowPlane = common.shadowPlane;
	Bytes( result.axis, common.axis );
	Bytes( result.rtcw.torsoAxis, common.torsoAxis );
	result.nonNormalizedAxes = common.nonNormalizedAxes ? qtrue : qfalse;
	Bytes( result.origin, common.origin );
	result.frame = common.frame;
	result.rtcw.torsoFrame = common.torsoFrame;
	if constexpr ( std::is_same_v<T, fntech3_rtcw_sp_refEntity_t> ) {
		Bytes( result.rtcw.scale, source.scale );
	} else {
		for ( float &axis : result.rtcw.scale ) axis = 1.0f;
	}
	Bytes( result.oldorigin, appearance.oldorigin );
	result.oldframe = appearance.oldframe;
	result.rtcw.oldTorsoFrame = appearance.oldTorsoFrame;
	result.backlerp = appearance.backlerp;
	result.rtcw.torsoBacklerp = appearance.torsoBacklerp;
	result.skinNum = appearance.skinNum;
	result.customSkin = appearance.customSkin;
	result.customShader = appearance.customShader;
	Bytes( result.shader, appearance.shaderRGBA );
	Bytes( result.shaderTexCoord, appearance.shaderTexCoord );
	Bytes( result.shaderTime, appearance.shaderTime );
	result.radius = appearance.radius;
	result.rotation = appearance.rotation;
	Bytes( result.rtcw.fireRiseDir, appearance.fireRiseDir );
	result.rtcw.fadeStartTime = appearance.fadeStartTime;
	result.rtcw.fadeEndTime = appearance.fadeEndTime;
	result.rtcw.hilightIntensity = appearance.hilightIntensity;
	// Effect flags belong exclusively to this tail; Q3/QL never inherit them.
	result.rtcw.reFlags = appearance.reFlags;
	result.rtcw.entityNum = appearance.entityNum;
	destination = result;
	return true;
}

template<class T>
bool RTCWTagEntity( const T &source, refEntity_t &destination ) noexcept {
	// CG_CalcMoveSpeeds initializes only model and leg frames for foot tags.
	// Scene type, render flags, bounds and material fields are not tag inputs.
	refEntity_t result{};
	result.reType = RT_MODEL;
	result.hModel = source.common.hModel;
	result.frame = source.common.frame;
	result.oldframe = source.appearance.oldframe;
	Bytes( result.backlerp, source.appearance.backlerp );
	result.rtcw.torsoFrame = source.common.torsoFrame;
	result.rtcw.oldTorsoFrame = source.appearance.oldTorsoFrame;
	Bytes( result.rtcw.torsoBacklerp, source.appearance.torsoBacklerp );
	Bytes( result.rtcw.torsoAxis, source.common.torsoAxis );
	destination = result;
	return true;
}

template<class T>
bool RTCWRefdef( const T &source, refdef_t &destination ) noexcept {
	const auto &fog = source.glfog;
	for ( int flag : { fog.useEndForClip, fog.registered, fog.drawsky, fog.clearscreen } )
		if ( flag != 0 && flag != 1 ) return false;
	refdef_t result{};
	LegacyRefdef( source.common, result );
	// RTCW bit 8 denotes a skybox portal, not RDF_NOFIRSTPERSON. Bit 16 is
	// both underwater and (in SP) draw-skybox; the original contracts alias it.
	const auto flags = source.common.rdflags;
	result.rdflags = ( flags & FNTECH3_RTCW_RDF_NOWORLDMODEL ? RDF_NOWORLDMODEL : 0 ) |
		( flags & FNTECH3_RTCW_RDF_HYPERSPACE ? RDF_HYPERSPACE : 0 );
	result.rtcw.skyboxPortal = flags & FNTECH3_RTCW_RDF_SKYBOXPORTAL ? qtrue : qfalse;
	result.rtcw.underwater = flags & FNTECH3_RTCW_RDF_UNDERWATER ? qtrue : qfalse;
	result.rtcw.drawingSky = flags & FNTECH3_RTCW_RDF_DRAWINGSKY ? qtrue : qfalse;
	result.rtcw.snooperView = flags & FNTECH3_RTCW_RDF_SNOOPERVIEW ? qtrue : qfalse;
	if constexpr ( std::is_same_v<T, fntech3_rtcw_sp_refdef_t> ) {
		result.rtcw.drawSkybox = flags & FNTECH3_RTCW_SP_RDF_DRAWSKYBOX ? qtrue : qfalse;
		result.rtcw.fog.dirty = source.fogDirty;
	}
	auto &output = result.rtcw.fog;
	output.mode = fog.mode; output.hint = fog.hint;
	output.startTime = fog.startTime; output.finishTime = fog.finishTime;
	Bytes( output.color, fog.color );
	output.start = fog.start; output.end = fog.end;
	output.useEndForClip = fog.useEndForClip ? qtrue : qfalse;
	output.density = fog.density;
	output.registered = fog.registered ? qtrue : qfalse;
	output.drawsky = fog.drawsky ? qtrue : qfalse;
	output.clearscreen = fog.clearscreen ? qtrue : qfalse;
	destination = result;
	return true;
}

template<class External, class Internal, class Adapter>
bool Read( const void *source, Internal &destination, Adapter adapter ) noexcept {
	if ( !source ) return false;
	External record;
	std::memcpy( &record, source, sizeof( record ) );
	return adapter( record, destination );
}

} // namespace detail

constexpr std::size_t EntityBytes( comGameId_t profile ) noexcept {
	switch ( profile ) {
	case COM_GAME_QUAKE3: return sizeof( fntech3_q3_refEntity_t );
	case COM_GAME_QUAKELIVE: return sizeof( fntech3_ql_refEntity_t );
	case COM_GAME_RTCW_SP: return sizeof( fntech3_rtcw_sp_refEntity_t );
	case COM_GAME_RTCW_MP: return sizeof( fntech3_rtcw_mp_refEntity_t );
	default: return 0;
	}
}

constexpr std::size_t RefdefBytes( comGameId_t profile ) noexcept {
	switch ( profile ) {
	case COM_GAME_QUAKE3: return sizeof( fntech3_q3_refdef_t );
	case COM_GAME_QUAKELIVE: return sizeof( fntech3_ql_refdef_t );
	case COM_GAME_RTCW_SP: return sizeof( fntech3_rtcw_sp_refdef_t );
	case COM_GAME_RTCW_MP: return sizeof( fntech3_rtcw_mp_refdef_t );
	default: return 0;
	}
}

inline bool ReadEntity( comGameId_t profile, const void *source, refEntity_t &destination ) noexcept {
	switch ( profile ) {
	case COM_GAME_QUAKE3:
		return detail::Read<fntech3_q3_refEntity_t>( source, destination, detail::LegacyEntity<fntech3_q3_refEntity_t> );
	case COM_GAME_QUAKELIVE:
		return detail::Read<fntech3_ql_refEntity_t>( source, destination, detail::LegacyEntity<fntech3_ql_refEntity_t> );
	case COM_GAME_RTCW_SP:
		return detail::Read<fntech3_rtcw_sp_refEntity_t>( source, destination, detail::RTCWEntity<fntech3_rtcw_sp_refEntity_t> );
	case COM_GAME_RTCW_MP:
		return detail::Read<fntech3_rtcw_mp_refEntity_t>( source, destination, detail::RTCWEntity<fntech3_rtcw_mp_refEntity_t> );
	default: return false;
	}
}

inline bool ReadRTCWTagEntity( comGameId_t profile, const void *source, refEntity_t &destination ) noexcept {
	switch ( profile ) {
	case COM_GAME_RTCW_SP:
		return detail::Read<fntech3_rtcw_sp_refEntity_t>( source, destination, detail::RTCWTagEntity<fntech3_rtcw_sp_refEntity_t> );
	case COM_GAME_RTCW_MP:
		return detail::Read<fntech3_rtcw_mp_refEntity_t>( source, destination, detail::RTCWTagEntity<fntech3_rtcw_mp_refEntity_t> );
	default: return false;
	}
}

inline bool ReadRefdef( comGameId_t profile, const void *source, refdef_t &destination ) noexcept {
	switch ( profile ) {
	case COM_GAME_QUAKE3:
		return detail::Read<fntech3_q3_refdef_t>( source, destination, detail::LegacyRefdef<fntech3_q3_refdef_t> );
	case COM_GAME_QUAKELIVE:
		return detail::Read<fntech3_ql_refdef_t>( source, destination, detail::LegacyRefdef<fntech3_ql_refdef_t> );
	case COM_GAME_RTCW_SP:
		return detail::Read<fntech3_rtcw_sp_refdef_t>( source, destination, detail::RTCWRefdef<fntech3_rtcw_sp_refdef_t> );
	case COM_GAME_RTCW_MP:
		return detail::Read<fntech3_rtcw_mp_refdef_t>( source, destination, detail::RTCWRefdef<fntech3_rtcw_mp_refdef_t> );
	default: return false;
	}
}

inline bool ReadRTCWPolyVert( const void *source, polyVert_t &destination ) noexcept {
	if ( !source ) return false;
	fntech3_rtcw_polyVert_t record;
	std::memcpy( &record, source, sizeof( record ) );
	polyVert_t result{};
	detail::Bytes( result.xyz, record.xyz );
	detail::Bytes( result.st, record.st );
	detail::Bytes( result.modulate, record.modulate );
	destination = result;
	return true;
}

inline bool WriteRTCWOrientation( void *destination, const orientation_t &source ) noexcept {
	if ( !destination ) return false;
	fntech3_rtcw_orientation_t record{};
	detail::Bytes( record.origin, source.origin );
	detail::Bytes( record.axis, source.axis );
	std::memcpy( destination, &record, sizeof( record ) );
	return true;
}

} // namespace fntech3::render
#endif
