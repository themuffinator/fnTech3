/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RTCW_AUDIO_COMPAT_HPP
#define FNTECH3_CLIENT_RTCW_AUDIO_COMPAT_HPP
#include "../compat/rtcw_module_abi.h"
#include "../compat/rtcw_game_abi.h"
#include <array>
#include <climits>
#include <cstring>
#include <limits>

template<std::size_t Count>
inline std::array<float, Count> CL_RTCWAudioFloats( vm_t *vm, intptr_t address ) {
	constexpr std::size_t bytes = Count * sizeof( float );
	if ( !vm || !address || static_cast<std::uintptr_t>( address ) >
		(std::numeric_limits<std::uintptr_t>::max)() - bytes )
		Com_Error( ERR_DROP, "Invalid RTCW spatial audio pointer" );
	VM_CHECKBOUNDS( vm, address, bytes );
	const void *source = VM_ExplicitArgPtr( vm, address );
	if ( !source ) Com_Error( ERR_DROP, "Invalid RTCW spatial audio buffer" );
	std::array<float, Count> result;
	std::memcpy( result.data(), source, bytes );
	for ( float value : result ) {
		std::uint32_t bits;
		std::memcpy( &bits, &value, sizeof( bits ) );
		if ( ( bits & 0x7f800000u ) == 0x7f800000u )
			Com_Error( ERR_DROP, "Non-finite RTCW spatial audio value" );
	}
	return result;
}

inline int CL_RTCWAudioEntity( intptr_t value ) {
	if ( value < 0 || value >= ( 1 << FNTECH3_RTCW_GENTITYNUM_BITS ) )
		Com_Error( ERR_DROP, "Invalid RTCW audio entity" );
	return static_cast<int>( value );
}

inline int CL_RTCWAudioInteger( intptr_t value ) {
	if ( value < INT_MIN || value > INT_MAX ) Com_Error( ERR_DROP, "Invalid RTCW audio integer" );
	return static_cast<int>( value );
}

inline std::array<char, 64> CL_RTCWAudioName( vm_t *vm, intptr_t address ) {
	std::array<char, 64> name{};
	if ( !address ) return name;
	if ( !vm || static_cast<std::uintptr_t>( address ) >
		(std::numeric_limits<std::uintptr_t>::max)() - name.size() )
		Com_Error( ERR_DROP, "Invalid RTCW audio name pointer" );
	VM_CHECKBOUNDS( vm, address, 1 );
	const auto *source = static_cast<const char *>( VM_ExplicitArgPtr( vm, address ) );
	if ( !source ) Com_Error( ERR_DROP, "Invalid RTCW music name" );
	for ( std::size_t i = 0; i + 1 < name.size(); ++i ) {
		VM_CHECKBOUNDS( vm, address, i + 1 );
		name[i] = source[i];
		if ( !name[i] ) break;
	}
	return name;
}

inline bool CL_RTCWAudioCall( vm_t *vm, fntech3::rtcw::module::Call call, intptr_t *args ) {
	using fntech3::rtcw::module::Call;
	if ( !Com_GameIsRTCW() ) return false;
	switch ( call ) {
	case Call::CG_S_STARTSTREAMINGSOUND: {
		if ( !vm ) Com_Error( ERR_DROP, "RTCW streaming import requires an initialized module" );
		const int entity = CL_RTCWAudioInteger( args[3] );
		if ( entity >= ( 1 << FNTECH3_RTCW_GENTITYNUM_BITS ) ) Com_Error( ERR_DROP, "Invalid RTCW streaming entity" );
		const int channel = CL_RTCWAudioInteger( args[4] );
		const int attenuation = CL_RTCWAudioInteger( args[5] );
		const auto intro = CL_RTCWAudioName( vm, args[1] );
		const auto loop = CL_RTCWAudioName( vm, args[2] );
		S_StartRTCWEntityStream( intro.data(), loop.data(), entity, channel, attenuation );
		return true;
	}
	case Call::CG_S_STOPSTREAMINGSOUND: {
		if ( !Com_GameIsRTCWSP() ) return false;
		const int entity = CL_RTCWAudioInteger( args[1] );
		if ( entity >= ( 1 << FNTECH3_RTCW_GENTITYNUM_BITS ) ) Com_Error( ERR_DROP, "Invalid RTCW streaming entity" );
		S_StopRTCWEntityStream( entity );
		return true;
	}
	case Call::CG_S_STARTSOUND:
	case Call::CG_S_STARTSOUNDEX: {
		const int entity = args[1] ? CL_RTCWAudioInteger(args[2]) : CL_RTCWAudioEntity(args[2]);
		const int channel = CL_RTCWAudioInteger(args[3]);
		const int handle = CL_RTCWAudioInteger(args[4]);
		const int flags = call == Call::CG_S_STARTSOUNDEX ? CL_RTCWAudioInteger(args[5]) : 0;
		if ( args[1] ) {
			const auto origin = CL_RTCWAudioFloats<3>(vm, args[1]);
			S_StartRTCWSound(origin.data(), entity, channel, handle, flags);
		} else S_StartRTCWSound(nullptr, entity, channel, handle, flags);
		return true;
	}
	case Call::CG_S_STARTLOCALSOUND:
		S_StartRTCWLocalSound(CL_RTCWAudioInteger(args[1]), CL_RTCWAudioInteger(args[2]));
		return true;
	case Call::UI_S_STARTLOCALSOUND:
		// The menus' own sound: RTCW's local sound, played as interface audio.
		S_StartInterfaceSound(CL_RTCWAudioInteger(args[1]), CL_RTCWAudioInteger(args[2]), 1.0f);
		return true;
	case Call::CG_S_ADDLOOPINGSOUND:
	case Call::CG_S_ADDREALLOOPINGSOUND: {
		// Original SP has no real-loop import. MP's two imports are identical.
		if ( Com_GameIsRTCWSP() && call == Call::CG_S_ADDREALLOOPINGSOUND ) return false;
		CL_RTCWAudioInteger( args[1] ); // Both original profiles ignore the entity number.
		const int range = CL_RTCWAudioInteger( args[4] );
		const int handle = CL_RTCWAudioInteger( args[5] );
		const int volume = CL_RTCWAudioInteger( args[6] );
		if ( range < 0 ) Com_Error( ERR_DROP, "Invalid RTCW looping sound range" );
		const auto origin = CL_RTCWAudioFloats<3>( vm, args[2] );
		const auto velocity = CL_RTCWAudioFloats<3>( vm, args[3] );
		S_AddRTCWLoopingSound( origin.data(), velocity.data(), range, handle, volume );
		return true;
	}
	case Call::CG_S_UPDATEENTITYPOSITION: {
		const int entity = CL_RTCWAudioEntity( args[1] );
		const auto origin = CL_RTCWAudioFloats<3>( vm, args[2] );
		S_UpdateEntityPosition( entity, origin.data() );
		return true;
	}
	case Call::CG_S_RESPATIALIZE: {
		const int entity = CL_RTCWAudioEntity( args[1] );
		const auto origin = CL_RTCWAudioFloats<3>( vm, args[2] );
		const auto orientation = CL_RTCWAudioFloats<9>( vm, args[3] );
		const int inwater = CL_RTCWAudioInteger( args[4] );
		float axis[3][3];
		std::memcpy( axis, orientation.data(), sizeof( axis ) );
		S_Respatialize( entity, origin.data(), axis, inwater );
		return true;
	}
	case Call::CG_S_CLEARLOOPINGSOUNDS:
		if ( Com_GameIsRTCWSP() ) S_ClearRTCWLoopingSounds(CL_RTCWAudioInteger(args[1]));
		else S_ClearLoopingSoundsFrame(); // MP ignores its legacy killall word.
		return true;
	case Call::CG_S_STOPLOOPINGSOUND:
		// Both original dispatchers intentionally perform no operation.
		return true;
	case Call::UI_S_STARTBACKGROUNDTRACK:
	case Call::CG_S_STARTBACKGROUNDTRACK: {
		if ( !vm ) Com_Error( ERR_DROP, "RTCW music import requires an initialized module" );
		const auto intro = CL_RTCWAudioName( vm, args[1] );
		const auto loop = CL_RTCWAudioName( vm, args[2] );
		if ( Com_GameIsRTCWSP() ) S_StartRTCWBackgroundTrack( intro.data(), loop.data(), CL_RTCWAudioInteger( args[3] ) );
		else S_StartBackgroundTrack( intro.data(), loop.data() ); // MP supplies exactly two names.
		return true;
	}
	case Call::UI_S_STOPBACKGROUNDTRACK:
	case Call::CG_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return true;
	case Call::UI_S_FADESTREAMINGSOUND:
	case Call::CG_S_FADESTREAMINGSOUND:
	case Call::UI_S_FADEALLSOUNDS:
	case Call::CG_S_FADEALLSOUNDS: {
		if ( !Com_GameIsRTCWSP() ) return false;
		const auto bits = static_cast<std::uint32_t>( args[1] );
		float volume;
		std::memcpy( &volume, &bits, sizeof( volume ) );
		const int time = CL_RTCWAudioInteger( args[2] );
		if ( call == Call::UI_S_FADEALLSOUNDS || call == Call::CG_S_FADEALLSOUNDS )
			S_FadeRTCWAllSounds( volume, time );
		else S_FadeRTCWStreamingSound( volume, time, CL_RTCWAudioInteger( args[3] ) );
		return true;
	}
	default: return false;
	}
}
#endif
