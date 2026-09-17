/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_BOTLIB_HPP
#define FNTECH3_SERVER_RTCW_BOTLIB_HPP
#include "server_rtcw_args.hpp"
#include "server_rtcw_bsp_query.hpp"
#include "server_rtcw_aas_query.hpp"
#include "server_rtcw_ai_state.hpp"
#include "server_rtcw_actions.hpp"
#include "server_rtcw_movement_view.hpp"
#include "server_rtcw_move_goal.hpp"
#include "../botlib/rtcw_navigation.h"
#include "../compat/rtcw_bot_entity.hpp"
#include "../compat/rtcw_state_adapter.hpp"

template<class Command>
static void SV_RTCWBotUserCommand(const vm_t *vm, const intptr_t *args) {
	const int client = SV_RTCWInteger(args[1]);
	if (!svs.clients || !SV_IsClientIndex(client) || client >= Com_GameProfile()->maxClients)
		Com_Error(ERR_DROP, "RTCW bot command client outside server");
	Command input;
	std::memcpy(&input, SV_RTCWPointer(vm, args[2], sizeof(input)), sizeof(input));
	usercmd_t converted;
	if (!fntech3::rtcw::state::ToInternal(input, converted))
		Com_Error(ERR_DROP, "Invalid RTCW bot command");
	SV_ClientThink(&SV_ClientForIndex(client), &converted);
}

// Only imports with established argument contracts belong here. In particular,
// RTCW navigation and AI records must not enter Quake's generic POD dispatch.
static bool SV_RTCWBotLibCall( const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result ) {
	using fntech3::rtcw::module::Call;
	if ( !Com_GameIsRTCW() ) return false;
	if ( SV_RTCWBSPQueryCall( vm, call, args, result ) ) return true;
	if ( SV_RTCWAASQueryCall( vm, call, args, result ) ) return true;
	if ( SV_RTCWAIStateCall( vm, call, args, result ) ) return true;
	if ( SV_RTCWActionCall( vm, call, args, result ) ) return true;
	if ( SV_RTCWMovementViewCall( vm, call, args, result ) ) return true;
	if ( SV_RTCWMoveToGoalCall( vm, call, args, result ) ) return true;
	switch ( call ) {
	case Call::BOTLIB_AI_MOVE_IN_DIRECTION: {
		if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
		const int state = SV_RTCWInteger(args[1]);
		auto direction = SV_RTCWVector(vm, args[2]);
		const float speed = SV_RTCWFloat(args[3]);
		const int type = SV_RTCWInteger(args[4]);
		if (!fntech3::rtcw::movement::Position(direction.data()) || !fntech3::rtcw::routing::Coordinate(speed))
			Com_Error(ERR_DROP, "Invalid RTCW directional movement");
		result = botlib_export->ai.BotMoveInDirection(state, direction.data(), speed, type);
		if (result != 0 && result != 1) Com_Error(ERR_DROP, "Invalid RTCW directional movement result");
		return true;
	}
	case Call::BOTLIB_AI_REACHABILITY_AREA: {
		if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
		auto origin = SV_RTCWVector(vm, args[1]);
		const int entity = SV_RTCWInteger(args[2]);
		// The historical parameter is named client, but is the trace's ignored
		// entity. Cast actors may lie outside the human client slots.
		if (!fntech3::rtcw::movement::Position(origin.data())
			|| entity < -1 || entity >= (1 << FNTECH3_RTCW_GENTITYNUM_BITS))
			Com_Error(ERR_DROP, "Invalid RTCW reachability-area arguments");
		result = botlib_export->ai.BotReachabilityArea(origin.data(), entity);
		if (result < 0 || result >= 65535) Com_Error(ERR_DROP, "Invalid RTCW reachability area");
		return true;
	}
	case Call::BOTLIB_USER_COMMAND:
		if (Com_GameProfile()->id == COM_GAME_RTCW_SP)
			SV_RTCWBotUserCommand<fntech3_rtcw_sp_usercmd_t>(vm, args);
		else SV_RTCWBotUserCommand<fntech3_rtcw_mp_usercmd_t>(vm, args);
		result = 0; return true;
	case Call::G_BOT_ALLOCATE_CLIENT: result = SV_BotAllocateClient(); return true;
	case Call::G_BOT_FREE_CLIENT:
		SV_BotFreeClient(SV_RTCWInteger(args[1])); result = 0; return true;
	case Call::BOTLIB_SETUP: result = SV_BotLibSetup(); return true;
	case Call::BOTLIB_SHUTDOWN: result = SV_BotLibShutdown(); return true;
	case Call::BOTLIB_START_FRAME:
		if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
		result = botlib_export->BotLibStartFrame(SV_RTCWFloat(args[1]));
		return true;
	case Call::BOTLIB_UPDATENTITY: {
		if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
		const int entity = SV_RTCWInteger(args[1]);
		if (entity < 0 || entity >= (1 << FNTECH3_RTCW_GENTITYNUM_BITS))
			Com_Error(ERR_DROP, "RTCW bot entity outside profile");
		fntech3_rtcw_bot_entitystate_t input;
		std::memcpy(&input, SV_RTCWPointer(vm, args[2], sizeof(input)), sizeof(input));
		bot_entitystate_t state;
		if (!fntech3::rtcw::bot::EntityState(input, state))
			Com_Error(ERR_DROP, "Invalid RTCW bot entity state");
		result = botlib_export->BotLibUpdateEntity(entity, &state);
		return true;
	}
	case Call::BOTLIB_LOAD_MAP: {
		if ( !botlib_export ) Com_Error( ERR_DROP, "RTCW bot library interface is unavailable" );
		// Null is the library's no-op update, not an empty map name.
		if ( !args[1] ) { result = botlib_export->BotLibLoadMap( nullptr ); return true; }
		const auto map = SV_RTCWString<MAX_QPATH>( vm, args[1] );
		result = botlib_export->BotLibLoadMap( map.data() );
		return true;
	}
	case Call::BOTLIB_AAS_INITIALIZED:
		if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
		result = botlib_export->aas.AAS_Initialized();
		return true;
	case Call::BOTLIB_AAS_TIME: {
		if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
		const float time = botlib_export->aas.AAS_Time();
		int bits;
		std::memcpy(&bits, &time, sizeof(bits));
		result = bits;
		return true;
	}
	case Call::BOTLIB_AI_UPDATE_ENTITY_ITEMS:
		if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
		// RTCW does not initialize goal-item configuration. The real routine
		// preserves that empty state; it also owns any item-expiration work.
		botlib_export->ai.BotUpdateEntityItems();
		result = 0;
		return true;
	case Call::BOTLIB_AAS_SETCURRENTWORLD:
		if ( !AAS_SetCurrentWorld( SV_RTCWInteger( args[1] ) ) )
			Com_Error( ERR_DROP, "RTCW navigation world outside profile" );
		result = 0; return true;
	case Call::BOTLIB_AAS_SETAASBLOCKINGENTITY: {
		const auto mins = SV_RTCWVector( vm, args[1] );
		const auto maxs = SV_RTCWVector( vm, args[2] );
		if ( !SV_ValidWorldBounds( mins.data(), maxs.data() ) )
			Com_Error( ERR_DROP, "Inverted RTCW navigation bounds" );
		AAS_SetBlockingEntity( mins.data(), maxs.data(), SV_RTCWInteger( args[3] ) );
		result = 0; return true;
	}
	case Call::BOTLIB_LIBVAR_SET:
	case Call::BOTLIB_LIBVAR_GET:
	case Call::BOTLIB_PC_ADD_GLOBAL_DEFINE:
		break;
	default: return false;
	}
	if ( !botlib_export ) Com_Error( ERR_DROP, "RTCW bot library interface is unavailable" );
	const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[1] );
	if ( call == Call::BOTLIB_LIBVAR_SET ) {
		const auto value = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		result = botlib_export->BotLibVarSet( name.data(), value.data() );
	} else if ( call == Call::BOTLIB_LIBVAR_GET ) {
		const int capacity = SV_RTCWInteger( args[3] );
		if ( capacity < 0 || capacity > MAX_STRING_CHARS )
			Com_Error( ERR_DROP, "Invalid RTCW bot variable capacity" );
		if ( !capacity ) { result = 0; return true; }
		void *destination = SV_RTCWPointer( vm, args[2], static_cast<std::size_t>( capacity ) );
		std::array<char, MAX_STRING_CHARS> value{};
		result = botlib_export->BotLibVarGet( name.data(), value.data(), capacity );
		value[capacity - 1] = '\0';
		// Copy the defined string only; caller guards and trailing bytes survive.
		std::memcpy( destination, value.data(), std::strlen( value.data() ) + 1 );
	} else {
		result = botlib_export->PC_AddGlobalDefine( name.data() );
	}
	return true;
}
#endif
