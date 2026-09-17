/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RTCW_RENDER_MEMORY_HPP
#define FNTECH3_CLIENT_RTCW_RENDER_MEMORY_HPP
#include <cstdint>
#include <cstddef>
#include <limits>

inline void *CL_RTCWRenderSpan( vm_t *vm, intptr_t address, std::size_t bytes ) {
	if ( !address || bytes > (std::numeric_limits<std::uintptr_t>::max)() -
		static_cast<std::uintptr_t>( address ) )
		Com_Error( ERR_DROP, "Invalid RTCW renderer buffer address" );
	VM_CHECKBOUNDS( vm, address, bytes );
	void *memory = VM_ExplicitArgPtr( vm, address );
	if ( !memory ) Com_Error( ERR_DROP, "Invalid RTCW renderer buffer" );
	return memory;
}
#endif
