/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RTCW_SOUND_NAME_HPP
#define FNTECH3_CLIENT_RTCW_SOUND_NAME_HPP
#include <array>
#include <cstdint>
#include <limits>

inline sfxHandle_t CL_RegisterRTCWSound( const vm_t *vm, intptr_t address ) {
	if ( !vm || !address ) Com_Error( ERR_DROP, "Null RTCW sound name" );
	const bool native = vm->entryPoint || vm->dllExports;
	const auto offset = static_cast<std::uintptr_t>( address );
	std::array<char, MAX_QPATH> name{};
	for ( std::size_t i = 0; i < name.size(); ++i ) {
		if ( native ) {
			if ( offset > (std::numeric_limits<std::uintptr_t>::max)() - i - 1 )
				Com_Error( ERR_DROP, "RTCW sound name address overflow" );
			name[i] = reinterpret_cast<const char *>( address )[i];
		} else {
			if ( !vm->dataBase || address < 0 || vm->exactDataLength <= 0
				|| offset >= static_cast<std::size_t>( vm->exactDataLength )
				|| i >= static_cast<std::size_t>( vm->exactDataLength ) - offset )
				Com_Error( ERR_DROP, "RTCW sound name outside VM data" );
			name[i] = static_cast<char>( vm->dataBase[offset + i] );
		}
		if ( !name[i] ) return S_RegisterSound( name.data(), qfalse );
	}
	// The sound backends also reject paths of MAX_QPATH bytes or longer.
	// Stop at that limit without searching untrusted memory for a terminator.
	return 0;
}
#endif
