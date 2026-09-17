/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_ARGS_HPP
#define FNTECH3_SERVER_RTCW_ARGS_HPP
#include "server_game_data.h"
#include "server_world_bounds.hpp"
#include <array>
#include <climits>
#include <cstring>
#include <limits>

// Shared by every legacy-native profile (RTCW SP/MP and ET); diagnostics name
// the active profile. Native modules own their allocations. Reject null/address
// wrap here; registered entity membership is a separate ServerGameDataView check.
static void *SV_RTCWPointer( const vm_t *vm, intptr_t address, std::size_t bytes ) {
	if ( !vm || !address ) Com_Error( ERR_DROP, "Null %s game argument", Com_GameProfile()->key );
	if ( !vm->entryPoint && !vm->dllExports ) {
		void *pointer;
		std::size_t remaining;
		if ( !fntech3::server::ResolveQvmGameDataPointer( vm->dataBase, vm->exactDataLength,
			address, pointer, remaining ) || bytes > remaining )
			Com_Error( ERR_DROP, "%s game argument outside VM data", Com_GameProfile()->key );
		return pointer;
	}
	if ( bytes > ( std::numeric_limits<std::uintptr_t>::max )() - static_cast<std::uintptr_t>( address ) )
		Com_Error( ERR_DROP, "%s game argument address overflow", Com_GameProfile()->key );
	return reinterpret_cast<void *>( address );
}

static int SV_RTCWInteger( intptr_t value ) {
	if ( value < INT_MIN || value > INT_MAX ) Com_Error( ERR_DROP, "Invalid %s game integer", Com_GameProfile()->key );
	return static_cast<int>( value );
}

static float SV_RTCWFloat( intptr_t value ) {
	const int bits = SV_RTCWInteger(value);
	if ((static_cast<unsigned>(bits) & 0x7f800000u) == 0x7f800000u)
		Com_Error(ERR_DROP, "Invalid %s game scalar", Com_GameProfile()->key);
	float result;
	std::memcpy(&result, &bits, sizeof(result));
	return result;
}

static std::array<float, 3> SV_RTCWVector( const vm_t *vm, intptr_t address ) {
	std::array<float, 3> result;
	std::memcpy( result.data(), SV_RTCWPointer( vm, address, sizeof( result ) ), sizeof( result ) );
	if ( !SV_FiniteWorldVector( result.data() ) ) Com_Error( ERR_DROP, "Invalid %s game vector", Com_GameProfile()->key );
	return result;
}

template<std::size_t Capacity>
static std::array<char, Capacity> SV_RTCWString( const vm_t *vm, intptr_t address ) {
	std::array<char, Capacity> text{};
	for ( std::size_t i = 0; i < Capacity; ++i ) {
		text[i] = static_cast<const char *>( SV_RTCWPointer( vm, address, i + 1 ) )[i];
		if ( !text[i] ) return text;
	}
	Com_Error( ERR_DROP, "Unterminated %s game string", Com_GameProfile()->key );
	return text;
}
#endif
