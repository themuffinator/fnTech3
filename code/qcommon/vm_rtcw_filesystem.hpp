/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_VM_RTCW_FILESYSTEM_HPP
#define FNTECH3_VM_RTCW_FILESYSTEM_HPP
#include <array>
#include <climits>
#include <cstdint>
#include <limits>

// RTCW game and cgame writes return a byte count. Quake's void traps retain
// their own result policy; never infer success from the requested length.
inline void *VM_RTCWFilePointer( const vm_t *vm, intptr_t address, std::size_t bytes ) {
	if ( !vm || !address ) Com_Error( ERR_DROP, "Null RTCW file buffer" );
	if ( !vm->entryPoint && !vm->dllExports ) {
		if ( address < 0 || vm->exactDataLength <= 0 ||
			static_cast<std::uintptr_t>( address ) >= static_cast<std::uintptr_t>( vm->exactDataLength ) ||
			bytes > static_cast<std::size_t>( vm->exactDataLength ) - static_cast<std::uintptr_t>( address ) )
			Com_Error( ERR_DROP, "RTCW file buffer outside VM data" );
	} else if ( bytes > (std::numeric_limits<std::uintptr_t>::max)() - static_cast<std::uintptr_t>( address ) ) {
		Com_Error( ERR_DROP, "RTCW file buffer address overflow" );
	}
	void *result = VM_ExplicitArgPtr( vm, address );
	if ( !result ) Com_Error( ERR_DROP, "Invalid RTCW file buffer" );
	return result;
}

inline int VM_RTCWWriteFile( const vm_t *vm, intptr_t address, intptr_t length,
	intptr_t handle, handleOwner_t owner ) {
	if ( !vm || length < 0 || length > INT_MAX || handle < INT_MIN || handle > INT_MAX )
		Com_Error( ERR_DROP, "Invalid RTCW file write arguments" );
	void *buffer = length ? VM_RTCWFilePointer( vm, address, static_cast<std::size_t>( length ) ) : nullptr;
	return FS_VM_WriteFile( buffer, static_cast<int>( length ), static_cast<int>( handle ), owner );
}

inline std::array<char, MAX_QPATH> VM_RTCWFileName( const vm_t *vm, intptr_t address ) {
	std::array<char, MAX_QPATH> result{};
	for ( std::size_t i = 0; i < result.size(); ++i ) {
		result[i] = static_cast<const char *>( VM_RTCWFilePointer( vm, address, i + 1 ) )[i];
		if ( !result[i] ) return result;
	}
	Com_Error( ERR_DROP, "Unterminated RTCW file name" );
	return result;
}

inline void VM_RTCWRenameFile( const vm_t *vm, intptr_t from, intptr_t to ) {
	const auto source = VM_RTCWFileName( vm, from );
	const auto destination = VM_RTCWFileName( vm, to );
	FS_VM_RenameFile( source.data(), destination.data() );
}
#endif
