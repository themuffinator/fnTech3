/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RTCW_CINEMATIC_COMPAT_HPP
#define FNTECH3_CLIENT_RTCW_CINEMATIC_COMPAT_HPP
#include "../compat/rtcw_module_abi.h"
#include <array>
#include <climits>
#include <cstring>
#include <limits>

inline int CL_RTCWCinematicInteger( intptr_t value ) {
	if ( value < INT_MIN || value > INT_MAX )
		Com_Error( ERR_DROP, "Invalid RTCW cinematic integer" );
	return static_cast<int>( value );
}

inline std::array<char, 256> CL_RTCWCinematicName( vm_t *vm, intptr_t address ) {
	if ( !vm || !address ) Com_Error( ERR_DROP, "Null RTCW cinematic name" );
	// Both original modes format the filename into a 256-byte pathname.
	// Bound the read and retain their truncation, independent of host PATH_MAX.
	std::array<char, 256> name{};
	for ( std::size_t i = 0; i + 1 < name.size(); ++i ) {
		if ( static_cast<std::uintptr_t>( address ) >
			(std::numeric_limits<std::uintptr_t>::max)() - i - 1 )
			Com_Error( ERR_DROP, "RTCW cinematic name address overflow" );
		VM_CHECKBOUNDS( vm, address, i + 1 );
		const char *source = static_cast<const char *>( VM_ExplicitArgPtr( vm, address ) );
		if ( !source ) Com_Error( ERR_DROP, "Invalid RTCW cinematic name" );
		name[i] = source[i];
		if ( !name[i] ) break;
	}
	if ( !std::strchr( name.data(), '/' ) && !std::strchr( name.data(), '\\' ) ) {
		std::array<char, 256> qualified{};
		std::memcpy( qualified.data(), "video/", 6 );
		std::memcpy( qualified.data() + 6, name.data(), qualified.size() - 7 );
		return qualified;
	}
	return name;
}

inline bool CL_RTCWCinematicCall( vm_t *vm, fntech3::rtcw::module::Call call,
	intptr_t *args, intptr_t &result ) {
	using fntech3::rtcw::module::Call;
	if ( !Com_GameIsRTCW() ) return false;
	static_assert( CIN_system == 1 && CIN_loop == 2 && CIN_hold == 4 &&
		CIN_silent == 8 && CIN_shader == 16, "RTCW common cinematic flags" );
	static_assert( FMV_IDLE == 0 && FMV_PLAY == 1 && FMV_EOF == 2 &&
		FMV_ID_BLT == 3 && FMV_ID_IDLE == 4 && FMV_LOOPED == 5 && FMV_ID_WAIT == 6,
		"RTCW cinematic result encodings" );
	switch ( call ) {
	case Call::CG_CIN_PLAYCINEMATIC:
	case Call::UI_CIN_PLAYCINEMATIC: {
		std::array<int, 5> values;
		for ( std::size_t i = 0; i < values.size(); ++i )
			values[i] = CL_RTCWCinematicInteger( args[i + 2] );
		if ( Com_GameIsRTCWSP() && ( values[4] & 0x20 ) )
			Com_Error( ERR_DROP, "RTCW SP letterboxed cinematic presentation is not implemented" );
		const auto name = CL_RTCWCinematicName( vm, args[1] );
		result = CIN_PlayCinematic( name.data(), values[0], values[1], values[2], values[3], values[4] );
		return true;
	}
	case Call::CG_CIN_STOPCINEMATIC:
	case Call::UI_CIN_STOPCINEMATIC:
		result = CIN_StopCinematic( CL_RTCWCinematicInteger( args[1] ) );
		return true;
	case Call::CG_CIN_RUNCINEMATIC:
	case Call::UI_CIN_RUNCINEMATIC:
		result = CIN_RunCinematic( CL_RTCWCinematicInteger( args[1] ) );
		return true;
	case Call::CG_CIN_DRAWCINEMATIC:
	case Call::UI_CIN_DRAWCINEMATIC:
		// Original RTCW UI and cgame both use independent 640x480 axis scaling.
		CIN_DrawCinematic( CL_RTCWCinematicInteger( args[1] ) );
		break;
	case Call::CG_CIN_SETEXTENTS:
	case Call::UI_CIN_SETEXTENTS: {
		std::array<int, 5> values;
		for ( std::size_t i = 0; i < values.size(); ++i )
			values[i] = CL_RTCWCinematicInteger( args[i + 1] );
		CIN_SetExtents( values[0], values[1], values[2], values[3], values[4] );
		break;
	}
	default: return false;
	}
	result = 0;
	return true;
}
#endif
