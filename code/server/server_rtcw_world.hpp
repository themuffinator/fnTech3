/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_WORLD_HPP
#define FNTECH3_SERVER_RTCW_WORLD_HPP
#include "server_world_bounds.hpp"
#include "server_rtcw_args.hpp"
#include <array>
#include <climits>
#include <limits>

// Included after the server's private world functions. Module entity pointers
// are opaque handles into ServerGameDataView, never canonical sharedEntity_t.
static sharedEntity_t *SV_RTCWWorldEntity( const vm_t *vm, intptr_t address, bool geometry ) {
	auto *pointer = SV_RTCWPointer( vm, address, 1 );
	fntech3::server::GameEntityRef reference;
	if ( serverGameData.Profile() != Com_GameProfile()->id
		|| !serverGameData.EntityRefForPointer( pointer, reference ) )
		Com_Error( ERR_DROP, "RTCW world argument is not a registered entity" );
	if ( geometry ) {
		sharedEntity_t entity{};
		if ( !SV_ReadGameEntity( pointer, &entity ) )
			Com_Error( ERR_DROP, "Cannot read RTCW world entity %d", reference.index );
		if ( !SV_ValidWorldBounds( entity.r.mins, entity.r.maxs )
			|| !SV_FiniteWorldVector( entity.r.currentOrigin )
			|| !SV_FiniteWorldVector( entity.r.currentAngles ) )
			Com_Error( ERR_DROP, "Invalid RTCW world entity geometry: entity %d type %d, "
				"bounds (%g %g %g)-(%g %g %g), origin (%g %g %g), angles (%g %g %g)",
				reference.index, entity.s.eType,
				entity.r.mins[0], entity.r.mins[1], entity.r.mins[2],
				entity.r.maxs[0], entity.r.maxs[1], entity.r.maxs[2],
				entity.r.currentOrigin[0], entity.r.currentOrigin[1], entity.r.currentOrigin[2],
				entity.r.currentAngles[0], entity.r.currentAngles[1], entity.r.currentAngles[2] );
	}
	return static_cast<sharedEntity_t *>( pointer );
}

static bool SV_RTCWWorldCall( const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result ) {
	using fntech3::rtcw::module::Call;
	if ( !Com_GameIsRTCW() ) return false;
	switch ( call ) {
	case Call::G_LINKENTITY:
		SV_LinkEntity( SV_RTCWWorldEntity( vm, args[1], true ) ); result = 0; return true;
	case Call::G_UNLINKENTITY:
		SV_UnlinkEntity( SV_RTCWWorldEntity( vm, args[1], false ) ); result = 0; return true;
	case Call::G_SET_BRUSH_MODEL: {
		auto *entity = SV_RTCWWorldEntity( vm, args[1], true );
		const auto name = SV_RTCWString<MAX_QPATH>( vm, args[2] );
		if ( name[0] != '*' || name[1] < '0' || name[1] > '9' )
			Com_Error( ERR_DROP, "Invalid RTCW brush model name" );
		int model = 0;
		for ( std::size_t i = 1; name[i]; ++i ) {
			if ( name[i] < '0' || name[i] > '9' || model > (INT_MAX - (name[i] - '0')) / 10 )
				Com_Error( ERR_DROP, "Invalid RTCW brush model index" );
			model = model * 10 + name[i] - '0';
		}
		if ( model >= CM_NumInlineModels() ) Com_Error( ERR_DROP, "RTCW brush model outside map" );
		SV_SetBrushModel( entity, name.data() ); result = 0; return true;
	}
	case Call::G_ENTITIES_IN_BOX:
	case Call::G_ENTITY_CONTACT:
	case Call::G_ENTITY_CONTACTCAPSULE: {
		const auto mins = SV_RTCWVector( vm, args[1] );
		const auto maxs = SV_RTCWVector( vm, args[2] );
		if ( !SV_ValidWorldBounds( mins.data(), maxs.data() ) ) Com_Error( ERR_DROP, "Inverted RTCW world bounds" );
		if ( call != Call::G_ENTITIES_IN_BOX ) {
			result = SV_EntityContact( mins.data(), maxs.data(), SV_RTCWWorldEntity( vm, args[3], true ),
				call == Call::G_ENTITY_CONTACTCAPSULE );
			return true;
		}
		const int count = SV_RTCWInteger( args[4] );
		if ( count < 0 || count > MAX_GENTITIES ) Com_Error( ERR_DROP, "Invalid RTCW area entity capacity" );
		if ( !count ) { result = 0; return true; }
		void *destination = SV_RTCWPointer( vm, args[3], static_cast<std::size_t>( count ) * sizeof( int ) );
		// Local aligned storage handles unaligned QVM outputs and input/output
		// aliases without changing a query while it is still being evaluated.
		std::array<int, MAX_GENTITIES> entities;
		const int found = SV_AreaEntities( mins.data(), maxs.data(), entities.data(), count );
		if ( found < 0 || found > count ) Com_Error( ERR_DROP, "Invalid RTCW area entity result" );
		std::memcpy( destination, entities.data(), static_cast<std::size_t>( found ) * sizeof( int ) );
		result = found; return true;
	}
	case Call::G_IN_PVS:
	case Call::G_IN_PVS_IGNORE_PORTALS: {
		const auto from = SV_RTCWVector( vm, args[1] );
		const auto to = SV_RTCWVector( vm, args[2] );
		result = call == Call::G_IN_PVS ? SV_inPVS( from.data(), to.data() )
			: SV_inPVSIgnorePortals( from.data(), to.data() );
		return true;
	}
	case Call::G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( SV_RTCWWorldEntity( vm, args[1], false ), SV_RTCWInteger( args[2] ) != 0 );
		result = 0; return true;
	case Call::G_AREAS_CONNECTED:
		result = CM_AreasConnected( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) ); return true;
	default: return false;
	}
}
#endif
