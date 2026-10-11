/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_WOLFET_AUDIO_COMPAT_HPP
#define FNTECH3_CLIENT_WOLFET_AUDIO_COMPAT_HPP
#include "../compat/wolfet_module_abi.h"
#include "client_rtcw_render_memory.hpp"
#include <array>
#include <climits>
#include <cstring>
#include <limits>

// Enemy Territory's sound imports, for its cgame and its UI alike. Every value
// ET submits is carried: the volume on each one-shot, the start time on each
// looping sound, and the flag deciding whether a fade also stops what it faded.
// False means the caller must continue its own dispatch.

inline int CL_WolfETAudioInteger( intptr_t value ) {
	if ( value < INT_MIN || value > INT_MAX ) Com_Error( ERR_DROP, "Invalid ET audio integer" );
	return static_cast<int>( value );
}

inline int CL_WolfETAudioEntity( intptr_t value ) {
	if ( value < 0 || value >= ( 1 << FNTECH3_WOLFET_GENTITYNUM_BITS ) )
		Com_Error( ERR_DROP, "Invalid ET audio entity" );
	return static_cast<int>( value );
}

template<std::size_t Count>
inline std::array<float, Count> CL_WolfETAudioFloats( vm_t *vm, intptr_t address ) {
	constexpr std::size_t bytes = Count * sizeof( float );
	if ( !vm || !address || static_cast<std::uintptr_t>( address ) >
		(std::numeric_limits<std::uintptr_t>::max)() - bytes )
		Com_Error( ERR_DROP, "Invalid ET spatial audio pointer" );
	VM_CHECKBOUNDS( vm, address, bytes );
	const void *source = VM_ExplicitArgPtr( vm, address );
	if ( !source ) Com_Error( ERR_DROP, "Invalid ET spatial audio buffer" );
	std::array<float, Count> result;
	std::memcpy( result.data(), source, bytes );
	for ( float value : result ) {
		std::uint32_t bits;
		std::memcpy( &bits, &value, sizeof( bits ) );
		if ( ( bits & 0x7f800000u ) == 0x7f800000u )
			Com_Error( ERR_DROP, "Non-finite ET spatial audio value" );
	}
	return result;
}

// A sound or music name is read one byte at a time so an unterminated module
// string can never be walked past the memory it owns.
inline std::array<char, 64> CL_WolfETAudioName( vm_t *vm, intptr_t address ) {
	std::array<char, 64> name{};
	if ( !address ) return name;
	if ( !vm || static_cast<std::uintptr_t>( address ) >
		(std::numeric_limits<std::uintptr_t>::max)() - name.size() )
		Com_Error( ERR_DROP, "Invalid ET audio name pointer" );
	VM_CHECKBOUNDS( vm, address, 1 );
	const auto *source = static_cast<const char *>( VM_ExplicitArgPtr( vm, address ) );
	if ( !source ) Com_Error( ERR_DROP, "Invalid ET audio name" );
	for ( std::size_t i = 0; i + 1 < name.size(); ++i ) {
		VM_CHECKBOUNDS( vm, address, i + 1 );
		name[i] = source[i];
		if ( !name[i] ) break;
	}
	return name;
}

inline float CL_WolfETAudioFade( intptr_t word ) {
	// ET passes a float in an integer word. A non-finite encoding is refused
	// rather than reaching the mixer's volume.
	const auto bits = static_cast<std::uint32_t>( word );
	float volume;
	std::memcpy( &volume, &bits, sizeof( volume ) );
	if ( ( bits & 0x7f800000u ) == 0x7f800000u ) Com_Error( ERR_DROP, "Non-finite ET fade volume" );
	return volume;
}

inline bool CL_WolfETAudioCall( vm_t *vm, fntech3::wolfet::module::Call call,
	intptr_t *args, intptr_t &result ) {
	using fntech3::wolfet::module::Call;
	if ( !Com_GameIsWolfET() ) return false;
	if ( !vm ) Com_Error( ERR_DROP, "ET sound import requires an initialized module" );
	result = 0;
	switch ( call ) {
	case Call::CG_S_STARTSOUND:
	case Call::CG_S_STARTSOUNDEX: {
		const bool extended = call == Call::CG_S_STARTSOUNDEX;
		const int entity = args[1] ? CL_WolfETAudioInteger( args[2] ) : CL_WolfETAudioEntity( args[2] );
		const int channel = CL_WolfETAudioInteger( args[3] );
		const int handle = CL_WolfETAudioInteger( args[4] );
		const int flags = extended ? CL_WolfETAudioInteger( args[5] ) : 0;
		const int volume = CL_WolfETAudioInteger( extended ? args[6] : args[5] );
		if ( args[1] ) {
			const auto origin = CL_WolfETAudioFloats<3>( vm, args[1] );
			S_StartWolfETSound( origin.data(), entity, channel, handle, flags, volume );
		} else {
			S_StartWolfETSound( nullptr, entity, channel, handle, flags, volume );
		}
		return true;
	}
	case Call::CG_S_STARTLOCALSOUND:
		S_StartWolfETLocalSound( CL_WolfETAudioInteger( args[1] ),
			CL_WolfETAudioInteger( args[2] ), CL_WolfETAudioInteger( args[3] ) );
		return true;
	case Call::UI_S_STARTLOCALSOUND:
		// The menus' own sound, at its volume, played as interface audio.
		S_StartWolfETInterfaceSound( CL_WolfETAudioInteger( args[1] ),
			CL_WolfETAudioInteger( args[2] ), CL_WolfETAudioInteger( args[3] ) );
		return true;
	case Call::CG_S_CLEARLOOPINGSOUNDS:
		// ET resubmits its whole loop set every frame and passes no argument.
		S_ClearWolfETLoopingSounds();
		return true;
	case Call::CG_S_CLEARSOUNDS:
		S_ClearWolfETSounds( args[1] ? qtrue : qfalse );
		return true;
	case Call::CG_S_ADDLOOPINGSOUND:
	case Call::CG_S_ADDREALLOOPINGSOUND: {
		// ET's two loop imports differ only in that the ordinary one always
		// submits a range of 1250; both carry a volume and a start time.
		const int range = CL_WolfETAudioInteger( args[3] );
		const int handle = CL_WolfETAudioInteger( args[4] );
		const int volume = CL_WolfETAudioInteger( args[5] );
		const int soundTime = CL_WolfETAudioInteger( args[6] );
		if ( range < 0 ) Com_Error( ERR_DROP, "Invalid ET looping sound range" );
		const auto origin = CL_WolfETAudioFloats<3>( vm, args[1] );
		const auto velocity = CL_WolfETAudioFloats<3>( vm, args[2] );
		S_AddWolfETLoopingSound( origin.data(), velocity.data(), range, handle, volume, soundTime );
		return true;
	}
	case Call::CG_S_UPDATEENTITYPOSITION: {
		const int entity = CL_WolfETAudioEntity( args[1] );
		const auto origin = CL_WolfETAudioFloats<3>( vm, args[2] );
		S_UpdateEntityPosition( entity, origin.data() );
		return true;
	}
	case Call::CG_S_GETVOICEAMPLITUDE:
		result = S_WolfETVoiceAmplitude( CL_WolfETAudioInteger( args[1] ) );
		return true;
	case Call::CG_S_RESPATIALIZE: {
		const int entity = CL_WolfETAudioEntity( args[1] );
		const auto origin = CL_WolfETAudioFloats<3>( vm, args[2] );
		const auto orientation = CL_WolfETAudioFloats<9>( vm, args[3] );
		const int inwater = CL_WolfETAudioInteger( args[4] );
		float axis[3][3];
		std::memcpy( axis, orientation.data(), sizeof( axis ) );
		S_Respatialize( entity, origin.data(), axis, inwater );
		return true;
	}
	case Call::CG_S_REGISTERSOUND:
	case Call::UI_S_REGISTERSOUND: {
		// Both of ET's modules force the compression argument false before the
		// call, so a compressed variant is never requested.
		const auto name = CL_WolfETAudioName( vm, args[1] );
		result = S_RegisterSound( name.data(), qfalse );
		return true;
	}
	case Call::CG_S_STARTBACKGROUNDTRACK:
	case Call::UI_S_STARTBACKGROUNDTRACK: {
		const auto intro = CL_WolfETAudioName( vm, args[1] );
		const auto loop = CL_WolfETAudioName( vm, args[2] );
		S_StartWolfETBackgroundTrack( intro.data(), loop.data(), CL_WolfETAudioInteger( args[3] ) );
		return true;
	}
	case Call::CG_S_STOPBACKGROUNDTRACK:
	case Call::UI_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return true;
	case Call::CG_S_FADESTREAMINGSOUND:
	case Call::UI_S_FADESTREAMINGSOUND:
		S_FadeWolfETStreamingSound( CL_WolfETAudioFade( args[1] ),
			CL_WolfETAudioInteger( args[2] ), CL_WolfETAudioInteger( args[3] ) );
		return true;
	case Call::CG_S_FADEALLSOUNDS:
	case Call::UI_S_FADEALLSOUNDS:
		S_FadeWolfETAllSounds( CL_WolfETAudioFade( args[1] ),
			CL_WolfETAudioInteger( args[2] ), args[3] ? qtrue : qfalse );
		return true;
	case Call::CG_S_STARTSTREAMINGSOUND: {
		const int entity = CL_WolfETAudioInteger( args[3] );
		if ( entity >= ( 1 << FNTECH3_WOLFET_GENTITYNUM_BITS ) )
			Com_Error( ERR_DROP, "Invalid ET streaming entity" );
		const int channel = CL_WolfETAudioInteger( args[4] );
		const int attenuation = CL_WolfETAudioInteger( args[5] );
		const auto intro = CL_WolfETAudioName( vm, args[1] );
		const auto loop = CL_WolfETAudioName( vm, args[2] );
		result = S_StartWolfETEntityStream( intro.data(), loop.data(), entity, channel, attenuation );
		return true;
	}
	case Call::CG_S_STOPSTREAMINGSOUND: {
		const int entity = CL_WolfETAudioInteger( args[1] );
		if ( entity >= ( 1 << FNTECH3_WOLFET_GENTITYNUM_BITS ) )
			Com_Error( ERR_DROP, "Invalid ET streaming entity" );
		S_StopWolfETEntityStream( entity );
		return true;
	}
	case Call::CG_S_GETSOUNDLENGTH:
		result = S_WolfETSoundLength( CL_WolfETAudioInteger( args[1] ) );
		return true;
	case Call::CG_S_GETCURRENTSOUNDTIME:
		result = S_WolfETCurrentSoundTime();
		return true;
	default:
		return false;
	}
}

#endif
