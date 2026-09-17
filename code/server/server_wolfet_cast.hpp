/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_WOLFET_CAST_HPP
#define FNTECH3_SERVER_WOLFET_CAST_HPP
#include "../botlib/wolfet_navigation.h"
#include "../compat/wolfet_module_abi.h"
#include "../compat/rtcw_routing.hpp"
#include <array>
#include <cstring>

/* ET's hide search asks the game module whether one position can see another.
 * The original routes that through botlib_import.BotVisibleFromPos, which calls
 * the module's BOT_VISIBLEFROMPOS export. fnTech3 keeps it off the frozen
 * botlib import table and registers it as an engine callback instead, so the
 * bot library never holds a pointer into a game module. */

static bool SV_WolfETCastAvailable() {
	if ( !Com_GameIsWolfET() || !gvm ) return false;
	if ( gvm->index != VM_GAME || !gvm->entryPoint || gvm->dllExports
		|| sizeof( void * ) != sizeof( int32_t ) ) {
		Com_Error( ERR_DROP, "WolfET visibility queries require a legacy x86 game module" );
	}
	return true;
}

static std::array<float, 3> SV_WolfETCastPosition( const void *source ) {
	if ( !source ) Com_Error( ERR_DROP, "Missing WolfET visibility position" );
	std::array<float, 3> position;
	// Read bytes without assuming the caller's vector is aligned.
	std::memcpy( position.data(), source, sizeof( position ) );
	for ( float value : position ) {
		if ( !fntech3::rtcw::routing::Coordinate( value ) ) {
			Com_Error( ERR_DROP, "Invalid WolfET visibility position" );
		}
	}
	return position;
}

static void SV_WolfETCastEntities( int source, int destination ) {
	constexpr int limit = 1 << FNTECH3_WOLFET_GENTITYNUM_BITS;
	if ( source < 0 || source >= limit || destination < 0 || destination >= limit ) {
		Com_Error( ERR_DROP, "WolfET visibility entity outside game bounds" );
	}
}

static int SV_WolfETCastVisible( const float *source, int sourceEntity,
	const float *destination, int destinationEntity, int update ) {
	if ( !SV_WolfETCastAvailable() ) return qfalse;
	SV_WolfETCastEntities( sourceEntity, destinationEntity );
	if ( update != 0 && update != 1 ) {
		Com_Error( ERR_DROP, "Invalid WolfET visibility update flag" );
	}
	auto from = SV_WolfETCastPosition( source );
	auto to = SV_WolfETCastPosition( destination );
	// The native module may write to its arguments; never expose botlib storage.
	const intptr_t result = VM_Call( gvm, 5, FNTECH3_WOLFET_BOT_VISIBLEFROMPOS,
		static_cast<int32_t>( reinterpret_cast<intptr_t>( from.data() ) ), sourceEntity,
		static_cast<int32_t>( reinterpret_cast<intptr_t>( to.data() ) ), destinationEntity,
		update );
	if ( result != qfalse && result != qtrue ) {
		Com_Error( ERR_DROP, "Invalid WolfET visibility result" );
	}
	return static_cast<int>( result );
}

#endif
