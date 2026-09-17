/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_WOLFET_BOTLIB_HPP
#define FNTECH3_SERVER_WOLFET_BOTLIB_HPP
#include "server_rtcw_args.hpp"
#include "../compat/wolfet_bot_actions.hpp"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/rtcw_navigation.h"
#include "../botlib/wolfet_navigation.h"
#include "../botlib/wolfet_prediction_record.h"
#include "../botlib/be_ai_move.h"
#include "../compat/wolfet_state_adapter.hpp"
#include "../botlib/be_ai_weap.h"
#include "../botlib/be_aas.h"
#include <array>
#include <cstddef>
#include <cstdint>


/* ET's prediction result reaches the module as its own record. Every scalar is
 * checked first, and the surface name is always terminated. Both sides are
 * frozen: the world trace is 84 bytes and the result 128. */
static bool SV_WolfETPredictionOutput( const aas_wolfet_clientmove_t &source,
	fntech3_wolfet_aas_clientmove_t &destination ) {
	static_assert( sizeof( fntech3_wolfet_bsp_trace_t ) == 84,
		"ET world trace is a frozen 84-byte record" );
	static_assert( sizeof( fntech3_wolfet_aas_clientmove_t ) == 128,
		"ET prediction result is a frozen 128-byte record" );
	const auto finite = []( float value ) {
		std::uint32_t bits;
		std::memcpy( &bits, &value, sizeof( bits ) );
		return ( bits & 0x7f800000u ) != 0x7f800000u;
	};
	const auto vector = [&finite]( const float *values ) {
		return finite( values[0] ) && finite( values[1] ) && finite( values[2] );
	};
	if ( !vector( source.endpos ) || !vector( source.velocity )
		|| !vector( source.trace.endpos ) || !vector( source.trace.plane.normal )
		|| !finite( source.trace.fraction ) || !finite( source.trace.plane.dist )
		|| !finite( source.trace.exp_dist ) || !finite( source.endcontents )
		|| !finite( source.time ) ) {
		return false;
	}
	destination = {};
	VectorCopy( source.endpos, destination.endpos );
	VectorCopy( source.velocity, destination.velocity );
	destination.trace.allsolid = source.trace.allsolid;
	destination.trace.startsolid = source.trace.startsolid;
	destination.trace.fraction = source.trace.fraction;
	VectorCopy( source.trace.endpos, destination.trace.endpos );
	VectorCopy( source.trace.plane.normal, destination.trace.plane.normal );
	destination.trace.plane.dist = source.trace.plane.dist;
	destination.trace.plane.type = source.trace.plane.type;
	destination.trace.plane.signbits = source.trace.plane.signbits;
	destination.trace.exp_dist = source.trace.exp_dist;
	destination.trace.sidenum = source.trace.sidenum;
	std::size_t length = 0;
	while ( length + 1 < sizeof( destination.trace.surface.name )
		&& length < sizeof( source.trace.surface.name )
		&& source.trace.surface.name[length] ) {
		destination.trace.surface.name[length] = source.trace.surface.name[length];
		++length;
	}
	destination.trace.surface.name[length] = '\0';
	destination.trace.surface.flags = source.trace.surface.flags;
	destination.trace.surface.value = source.trace.surface.value;
	destination.trace.contents = source.trace.contents;
	destination.trace.ent = source.trace.ent;
	destination.presencetype = source.presencetype;
	destination.stopevent = source.stopevent;
	destination.endcontents = source.endcontents;
	destination.time = source.time;
	destination.frames = source.frames;
	return true;
}

/* ET elementary-action and AI-state imports. ET repeats RTCW MP's argument
 * contracts for these ordinals and adds EA_Walk, EA_Prone and the fifteenth
 * ACTION_PRONE input bit. Records with ET-specific layouts (goals, movement
 * results, entity info, routes) are not admitted here; they stay explicit
 * failures until their frozen ET records exist. Reference: Enemy-Territory
 * g_syscalls.c and server/sv_game.c at
 * 40342a9e3690cb5b627a433d4d5cbf30e3c57698. */
static bool SV_WolfETActionCall( const vm_t *vm, fntech3::wolfet::module::Call call,
	const intptr_t *args, intptr_t &result ) {
	using fntech3::wolfet::module::Call;
	switch ( call ) {
	case Call::BOTLIB_EA_SAY: case Call::BOTLIB_EA_SAY_TEAM:
	case Call::BOTLIB_EA_USE_ITEM: case Call::BOTLIB_EA_DROP_ITEM:
	case Call::BOTLIB_EA_USE_INV: case Call::BOTLIB_EA_DROP_INV:
	case Call::BOTLIB_EA_GESTURE: case Call::BOTLIB_EA_COMMAND:
	case Call::BOTLIB_EA_SELECT_WEAPON: case Call::BOTLIB_EA_TALK:
	case Call::BOTLIB_EA_ATTACK: case Call::BOTLIB_EA_RELOAD:
	case Call::BOTLIB_EA_USE: case Call::BOTLIB_EA_RESPAWN:
	case Call::BOTLIB_EA_JUMP: case Call::BOTLIB_EA_DELAYED_JUMP:
	case Call::BOTLIB_EA_CROUCH: case Call::BOTLIB_EA_WALK:
	case Call::BOTLIB_EA_PRONE: case Call::BOTLIB_EA_MOVE_UP:
	case Call::BOTLIB_EA_MOVE_DOWN: case Call::BOTLIB_EA_MOVE_FORWARD:
	case Call::BOTLIB_EA_MOVE_BACK: case Call::BOTLIB_EA_MOVE_LEFT:
	case Call::BOTLIB_EA_MOVE_RIGHT: case Call::BOTLIB_EA_MOVE:
	case Call::BOTLIB_EA_VIEW: case Call::BOTLIB_EA_END_REGULAR:
	case Call::BOTLIB_EA_GET_INPUT: case Call::BOTLIB_EA_RESET_INPUT:
		break;
	default: return false;
	}
	if ( !botlib_export ) Com_Error( ERR_DROP, "WolfET bot library interface is unavailable" );
	const int client = SV_RTCWInteger( args[1] );
	if ( client < 0 || client >= Com_GameProfile()->maxClients || !EA_ClientInputAvailable( client ) )
		Com_Error( ERR_DROP, "WolfET action client has no input state" );
	auto &ea = botlib_export->ea;
	switch ( call ) {
	case Call::BOTLIB_EA_SAY: case Call::BOTLIB_EA_SAY_TEAM:
	case Call::BOTLIB_EA_USE_ITEM: case Call::BOTLIB_EA_DROP_ITEM:
	case Call::BOTLIB_EA_USE_INV: case Call::BOTLIB_EA_DROP_INV:
	case Call::BOTLIB_EA_COMMAND: {
		const auto text = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		if ( call == Call::BOTLIB_EA_SAY ) ea.EA_Say( client, text.data() );
		else if ( call == Call::BOTLIB_EA_SAY_TEAM ) ea.EA_SayTeam( client, text.data() );
		else ea.EA_Command( client, text.data() );
		break;
	}
	case Call::BOTLIB_EA_SELECT_WEAPON: ea.EA_SelectWeapon( client, SV_RTCWInteger( args[2] ) ); break;
	case Call::BOTLIB_EA_GESTURE: ea.EA_Gesture( client ); break;
	case Call::BOTLIB_EA_TALK: ea.EA_Talk( client ); break;
	case Call::BOTLIB_EA_ATTACK: ea.EA_Attack( client ); break;
	case Call::BOTLIB_EA_RELOAD: ea.EA_Action( client, ACTION_RTCW_RELOAD ); break;
	case Call::BOTLIB_EA_PRONE: ea.EA_Action( client, ACTION_WOLFET_PRONE ); break;
	case Call::BOTLIB_EA_USE: ea.EA_Use( client ); break;
	case Call::BOTLIB_EA_RESPAWN: ea.EA_Respawn( client ); break;
	case Call::BOTLIB_EA_JUMP: ea.EA_Jump( client ); break;
	case Call::BOTLIB_EA_DELAYED_JUMP: ea.EA_DelayedJump( client ); break;
	case Call::BOTLIB_EA_CROUCH: ea.EA_Crouch( client ); break;
	// The bot export has no EA_Walk entry; ET walks through the action bit.
	case Call::BOTLIB_EA_WALK: ea.EA_Action( client, ACTION_WALK ); break;
	case Call::BOTLIB_EA_MOVE_UP: ea.EA_MoveUp( client ); break;
	case Call::BOTLIB_EA_MOVE_DOWN: ea.EA_MoveDown( client ); break;
	case Call::BOTLIB_EA_MOVE_FORWARD: ea.EA_MoveForward( client ); break;
	case Call::BOTLIB_EA_MOVE_BACK: ea.EA_MoveBack( client ); break;
	case Call::BOTLIB_EA_MOVE_LEFT: ea.EA_MoveLeft( client ); break;
	case Call::BOTLIB_EA_MOVE_RIGHT: ea.EA_MoveRight( client ); break;
	case Call::BOTLIB_EA_MOVE: {
		auto dir = SV_RTCWVector( vm, args[2] );
		ea.EA_Move( client, dir.data(), SV_RTCWFloat( args[3] ) );
		break;
	}
	case Call::BOTLIB_EA_VIEW: {
		auto angles = SV_RTCWVector( vm, args[2] );
		ea.EA_View( client, angles.data() );
		break;
	}
	case Call::BOTLIB_EA_END_REGULAR: ea.EA_EndRegular( client, SV_RTCWFloat( args[2] ) ); break;
	case Call::BOTLIB_EA_GET_INPUT: {
		const float time = SV_RTCWFloat( args[2] );
		void *destination = SV_RTCWPointer( vm, args[3], sizeof( fntech3_wolfet_bot_input_t ) );
		bot_input_t input;
		ea.EA_GetInput( client, time, &input );
		const auto output = fntech3::wolfet::bot::ActionOutput( input, EA_ExtraInputFlags( client ) );
		std::memcpy( destination, &output, sizeof( output ) );
		break;
	}
	case Call::BOTLIB_EA_RESET_INPUT: {
		// ET's own bot setup passes a null record to clear the pending input.
		if ( !args[2] ) { EA_RestoreInput( client, nullptr, 0 ); break; }
		fntech3_wolfet_bot_input_t input;
		std::memcpy( &input, SV_RTCWPointer( vm, args[2], sizeof( input ) ), sizeof( input ) );
		bot_input_t converted;
		if ( !fntech3::wolfet::bot::ActionInput( input, converted ) )
			Com_Error( ERR_DROP, "Invalid WolfET action input" );
		EA_RestoreInput( client, &converted,
			static_cast<unsigned>( input.actionflags ) & ~fntech3::wolfet::bot::KnownActionBits );
		break;
	}
	default: return false;
	}
	result = 0;
	return true;
}

/* Character, chat, goal, movement and weapon state handles. ET repeats RTCW's
 * scalar and string contracts for every ordinal admitted here. */
static bool SV_WolfETAIStateCall( const vm_t *vm, fntech3::wolfet::module::Call call,
	const intptr_t *args, intptr_t &result ) {
	using fntech3::wolfet::module::Call;
	switch ( call ) {
	case Call::BOTLIB_AI_LOAD_CHARACTER: case Call::BOTLIB_AI_FREE_CHARACTER:
	case Call::BOTLIB_AI_CHARACTERISTIC_FLOAT: case Call::BOTLIB_AI_CHARACTERISTIC_BFLOAT:
	case Call::BOTLIB_AI_CHARACTERISTIC_INTEGER: case Call::BOTLIB_AI_CHARACTERISTIC_BINTEGER:
	case Call::BOTLIB_AI_CHARACTERISTIC_STRING:
	case Call::BOTLIB_AI_ALLOC_CHAT_STATE: case Call::BOTLIB_AI_FREE_CHAT_STATE:
	case Call::BOTLIB_AI_LOAD_CHAT_FILE: case Call::BOTLIB_AI_SET_CHAT_GENDER:
	case Call::BOTLIB_AI_SET_CHAT_NAME: case Call::BOTLIB_AI_ENTER_CHAT:
	case Call::BOTLIB_AI_ALLOC_GOAL_STATE: case Call::BOTLIB_AI_FREE_GOAL_STATE:
	case Call::BOTLIB_AI_LOAD_ITEM_WEIGHTS: case Call::BOTLIB_AI_FREE_ITEM_WEIGHTS:
	case Call::BOTLIB_AI_ALLOC_MOVE_STATE: case Call::BOTLIB_AI_FREE_MOVE_STATE:
	case Call::BOTLIB_AI_RESET_MOVE_STATE: case Call::BOTLIB_AI_RESET_AVOID_REACH:
	case Call::BOTLIB_AI_RESET_LAST_AVOID_REACH:
	case Call::BOTLIB_AI_ALLOC_WEAPON_STATE: case Call::BOTLIB_AI_FREE_WEAPON_STATE:
	case Call::BOTLIB_AI_RESET_WEAPON_STATE: case Call::BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
	case Call::BOTLIB_AI_RESET_GOAL_STATE: case Call::BOTLIB_AI_RESET_AVOID_GOALS:
	case Call::BOTLIB_AI_EMPTY_GOAL_STACK: case Call::BOTLIB_AI_POP_GOAL:
	case Call::BOTLIB_AI_DUMP_AVOID_GOALS: case Call::BOTLIB_AI_DUMP_GOAL_STACK:
		break;
	default: return false;
	}
	if ( !botlib_export ) Com_Error( ERR_DROP, "WolfET bot library interface is unavailable" );
	auto &ai = botlib_export->ai;
	switch ( call ) {
	case Call::BOTLIB_AI_LOAD_CHARACTER: {
		const auto file = SV_RTCWString<MAX_QPATH>( vm, args[1] );
		result = ai.BotLoadCharacter( file.data(), static_cast<float>( SV_RTCWInteger( args[2] ) ) );
		return true;
	}
	case Call::BOTLIB_AI_FREE_CHARACTER: ai.BotFreeCharacter( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_CHARACTERISTIC_FLOAT: {
		const float value = ai.Characteristic_Float( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) );
		int bits;
		std::memcpy( &bits, &value, sizeof( bits ) );
		result = bits;
		return true;
	}
	case Call::BOTLIB_AI_CHARACTERISTIC_BFLOAT: {
		const float value = ai.Characteristic_BFloat( SV_RTCWInteger( args[1] ),
			SV_RTCWInteger( args[2] ), SV_RTCWFloat( args[3] ), SV_RTCWFloat( args[4] ) );
		int bits;
		std::memcpy( &bits, &value, sizeof( bits ) );
		result = bits;
		return true;
	}
	case Call::BOTLIB_AI_CHARACTERISTIC_INTEGER:
		result = ai.Characteristic_Integer( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) );
		return true;
	case Call::BOTLIB_AI_CHARACTERISTIC_BINTEGER:
		result = ai.Characteristic_BInteger( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ),
			SV_RTCWFloat( args[3] ), SV_RTCWFloat( args[4] ) );
		return true;
	case Call::BOTLIB_AI_CHARACTERISTIC_STRING: {
		const int size = SV_RTCWInteger( args[4] );
		if ( size <= 0 ) Com_Error( ERR_DROP, "Invalid WolfET characteristic buffer size" );
		auto *buffer = static_cast<char *>( SV_RTCWPointer( vm, args[3], static_cast<std::size_t>( size ) ) );
		ai.Characteristic_String( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ), buffer, size );
		break;
	}
	case Call::BOTLIB_AI_ALLOC_CHAT_STATE: result = ai.BotAllocChatState(); return true;
	case Call::BOTLIB_AI_FREE_CHAT_STATE: ai.BotFreeChatState( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_LOAD_CHAT_FILE: {
		const auto file = SV_RTCWString<MAX_QPATH>( vm, args[2] );
		const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[3] );
		result = ai.BotLoadChatFile( SV_RTCWInteger( args[1] ), file.data(), name.data() );
		return true;
	}
	case Call::BOTLIB_AI_SET_CHAT_GENDER:
		ai.BotSetChatGender( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) );
		break;
	case Call::BOTLIB_AI_SET_CHAT_NAME: {
		// ET stores the name only; its BotEnterChat carries the speaking client.
		const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		BotSetChatNameLegacy( SV_RTCWInteger( args[1] ), name.data() );
		break;
	}
	case Call::BOTLIB_AI_ENTER_CHAT:
		BotEnterChatLegacy( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ),
			SV_RTCWInteger( args[3] ) );
		break;
	case Call::BOTLIB_AI_ALLOC_GOAL_STATE: result = ai.BotAllocGoalState( SV_RTCWInteger( args[1] ) ); return true;
	case Call::BOTLIB_AI_FREE_GOAL_STATE: ai.BotFreeGoalState( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_RESET_GOAL_STATE: ai.BotResetGoalState( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_RESET_AVOID_GOALS: ai.BotResetAvoidGoals( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_EMPTY_GOAL_STACK: ai.BotEmptyGoalStack( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_POP_GOAL: ai.BotPopGoal( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_DUMP_AVOID_GOALS: ai.BotDumpAvoidGoals( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_DUMP_GOAL_STACK: ai.BotDumpGoalStack( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_LOAD_ITEM_WEIGHTS: {
		const auto file = SV_RTCWString<MAX_QPATH>( vm, args[2] );
		result = ai.BotLoadItemWeights( SV_RTCWInteger( args[1] ), file.data() );
		return true;
	}
	case Call::BOTLIB_AI_FREE_ITEM_WEIGHTS: ai.BotFreeItemWeights( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_ALLOC_MOVE_STATE: result = ai.BotAllocMoveState(); return true;
	case Call::BOTLIB_AI_FREE_MOVE_STATE: ai.BotFreeMoveState( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_RESET_MOVE_STATE: ai.BotResetMoveState( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_RESET_AVOID_REACH: ai.BotResetAvoidReach( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_RESET_LAST_AVOID_REACH: ai.BotResetLastAvoidReach( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_ALLOC_WEAPON_STATE: result = ai.BotAllocWeaponState(); return true;
	case Call::BOTLIB_AI_FREE_WEAPON_STATE: ai.BotFreeWeaponState( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_RESET_WEAPON_STATE: ai.BotResetWeaponState( SV_RTCWInteger( args[1] ) ); break;
	case Call::BOTLIB_AI_LOAD_WEAPON_WEIGHTS: {
		const auto file = SV_RTCWString<MAX_QPATH>( vm, args[2] );
		result = ai.BotLoadWeaponWeights( SV_RTCWInteger( args[1] ), file.data() );
		return true;
	}
	default: return false;
	}
	result = 0;
	return true;
}


/* Bot-library lifetime, world updates and the AAS queries whose ET argument
 * contract is scalars, vectors or records that match the engine's own. Imports
 * carrying ET-specific goal, movement or route records are deliberately absent:
 * they fail explicitly until their frozen ET records exist. ET's own engine also
 * leaves BOTLIB_AAS_ENTITY_VISIBLE, BOTLIB_AAS_IN_FIELD_OF_VISION,
 * BOTLIB_AAS_VISIBLE_CLIENTS, BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX,
 * BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL and BOTLIB_AAS_PREDICT_ROUTE unhandled, so
 * refusing them here reproduces the original engine rather than a shortfall. */
static bool SV_WolfETBotLibCall( const vm_t *vm, fntech3::wolfet::module::Call call,
	const intptr_t *args, intptr_t &result ) {
	using fntech3::wolfet::module::Call;
	const auto library = []() -> botlib_export_t & {
		if ( !botlib_export ) Com_Error( ERR_DROP, "WolfET bot library interface is unavailable" );
		return *botlib_export;
	};
	const auto area = [args]( int index ) {
		const int value = SV_RTCWInteger( args[index] );
		if ( value < 0 ) Com_Error( ERR_DROP, "Invalid WolfET navigation area" );
		return value;
	};
	const auto buffer = [vm, args]( int index, int size ) {
		if ( size <= 0 ) Com_Error( ERR_DROP, "Invalid WolfET output buffer size" );
		return static_cast<char *>( SV_RTCWPointer( vm, args[index], static_cast<std::size_t>( size ) ) );
	};
	switch ( call ) {
	case Call::BOTLIB_SETUP: result = SV_BotLibSetup(); return true;
	case Call::BOTLIB_SHUTDOWN: result = SV_BotLibShutdown(); return true;
	case Call::BOTLIB_LIBVAR_SET: {
		const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[1] );
		const auto value = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		result = library().BotLibVarSet( name.data(), value.data() );
		return true;
	}
	case Call::BOTLIB_LIBVAR_GET: {
		auto &exports = library();
		const int size = SV_RTCWInteger( args[3] );
		const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[1] );
		result = exports.BotLibVarGet( name.data(), buffer( 2, size ), size );
		return true;
	}
	case Call::BOTLIB_START_FRAME:
		result = library().BotLibStartFrame( SV_RTCWFloat( args[1] ) );
		return true;
	case Call::BOTLIB_LOAD_MAP: {
		auto &exports = library();
		// Null is the library's no-op update, not an empty map name.
		if ( !args[1] ) { result = exports.BotLibLoadMap( nullptr ); return true; }
		const auto map = SV_RTCWString<MAX_QPATH>( vm, args[1] );
		result = exports.BotLibLoadMap( map.data() );
		return true;
	}
	case Call::BOTLIB_UPDATENTITY: {
		auto &exports = library();
		const int entity = SV_RTCWInteger( args[1] );
		if ( entity < 0 || entity >= ( 1 << FNTECH3_WOLFET_GENTITYNUM_BITS ) )
			Com_Error( ERR_DROP, "WolfET bot entity outside profile" );
		fntech3_wolfet_bot_entitystate_t input;
		std::memcpy( &input, SV_RTCWPointer( vm, args[2], sizeof( input ) ), sizeof( input ) );
		bot_entitystate_t state;
		if ( !fntech3::wolfet::bot::EntityState( input, state ) )
			Com_Error( ERR_DROP, "Invalid WolfET bot entity state" );
		result = exports.BotLibUpdateEntity( entity, &state );
		return true;
	}
	case Call::BOTLIB_TEST: {
		auto &exports = library();
		auto *text = args[2]
			? static_cast<char *>( SV_RTCWPointer( vm, args[2], MAX_QPATH ) ) : nullptr;
		if ( text && !std::memchr( text, 0, MAX_QPATH ) )
			Com_Error( ERR_DROP, "Unterminated WolfET library test argument" );
		auto first = SV_RTCWVector( vm, args[3] );
		auto second = SV_RTCWVector( vm, args[4] );
		result = exports.Test( SV_RTCWInteger( args[1] ), text, first.data(), second.data() );
		return true;
	}
	case Call::BOTLIB_GET_SNAPSHOT_ENTITY:
		result = SV_BotGetSnapshotEntity( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) );
		return true;
	case Call::BOTLIB_GET_CONSOLE_MESSAGE: {
		const int size = SV_RTCWInteger( args[3] );
		result = SV_BotGetConsoleMessage( SV_RTCWInteger( args[1] ), buffer( 2, size ), size );
		return true;
	}
	case Call::BOTLIB_USER_COMMAND: {
		const int client = SV_RTCWInteger( args[1] );
		if ( !svs.clients || !SV_IsClientIndex( client ) || client >= Com_GameProfile()->maxClients )
			Com_Error( ERR_DROP, "WolfET bot command client outside server" );
		fntech3_wolfet_usercmd_t input;
		std::memcpy( &input, SV_RTCWPointer( vm, args[2], sizeof( input ) ), sizeof( input ) );
		usercmd_t converted;
		if ( !fntech3::wolfet::state::ToInternal( input, converted ) )
			Com_Error( ERR_DROP, "Invalid WolfET bot command" );
		SV_ClientThink( &SV_ClientForIndex( client ), &converted );
		result = 0;
		return true;
	}
	case Call::BOTLIB_AAS_INITIALIZED: result = library().aas.AAS_Initialized(); return true;
	case Call::BOTLIB_AAS_TIME: {
		const float time = library().aas.AAS_Time();
		int bits;
		std::memcpy( &bits, &time, sizeof( bits ) );
		result = bits;
		return true;
	}
	case Call::BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX: {
		auto &exports = library();
		auto *mins = static_cast<float *>( SV_RTCWPointer( vm, args[2], sizeof( vec3_t ) ) );
		auto *maxs = static_cast<float *>( SV_RTCWPointer( vm, args[3], sizeof( vec3_t ) ) );
		exports.aas.AAS_PresenceTypeBoundingBox( SV_RTCWInteger( args[1] ), mins, maxs );
		result = 0;
		return true;
	}
	case Call::BOTLIB_AAS_POINT_AREA_NUM: {
		auto point = SV_RTCWVector( vm, args[1] );
		result = library().aas.AAS_PointAreaNum( point.data() );
		return true;
	}
	case Call::BOTLIB_AAS_POINT_CONTENTS: {
		auto point = SV_RTCWVector( vm, args[1] );
		result = library().aas.AAS_PointContents( point.data() );
		return true;
	}
	case Call::BOTLIB_AAS_SWIMMING: {
		auto origin = SV_RTCWVector( vm, args[1] );
		result = library().aas.AAS_Swimming( origin.data() );
		return true;
	}
	case Call::BOTLIB_AAS_TRACE_AREAS: {
		auto &exports = library();
		const int count = SV_RTCWInteger( args[5] );
		if ( count <= 0 || count > FNTECH3_WOLFET_MAX_AAS_QUERY )
			Com_Error( ERR_DROP, "Invalid WolfET trace area count" );
		auto start = SV_RTCWVector( vm, args[1] );
		auto end = SV_RTCWVector( vm, args[2] );
		auto *areas = static_cast<int *>( SV_RTCWPointer( vm, args[3],
			static_cast<std::size_t>( count ) * sizeof( int ) ) );
		auto *points = args[4] ? static_cast<vec_t *>( SV_RTCWPointer( vm, args[4],
			static_cast<std::size_t>( count ) * sizeof( vec3_t ) ) ) : nullptr;
		result = exports.aas.AAS_TraceAreas( start.data(), end.data(), areas,
			reinterpret_cast<vec3_t *>( points ), count );
		return true;
	}
	case Call::BOTLIB_AAS_BBOX_AREAS: {
		auto &exports = library();
		const int count = SV_RTCWInteger( args[4] );
		if ( count <= 0 || count > FNTECH3_WOLFET_MAX_AAS_QUERY )
			Com_Error( ERR_DROP, "Invalid WolfET bounding-box area count" );
		auto mins = SV_RTCWVector( vm, args[1] );
		auto maxs = SV_RTCWVector( vm, args[2] );
		if ( !SV_ValidWorldBounds( mins.data(), maxs.data() ) )
			Com_Error( ERR_DROP, "Inverted WolfET bounding-box area query" );
		auto *areas = static_cast<int *>( SV_RTCWPointer( vm, args[3],
			static_cast<std::size_t>( count ) * sizeof( int ) ) );
		result = exports.aas.AAS_BBoxAreas( mins.data(), maxs.data(), areas, count );
		return true;
	}
	case Call::BOTLIB_AAS_NEXT_BSP_ENTITY:
		result = library().aas.AAS_NextBSPEntity( SV_RTCWInteger( args[1] ) );
		return true;
	case Call::BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY: {
		auto &exports = library();
		const int size = SV_RTCWInteger( args[4] );
		const auto key = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		result = exports.aas.AAS_ValueForBSPEpairKey( SV_RTCWInteger( args[1] ), key.data(),
			buffer( 3, size ), size );
		return true;
	}
	case Call::BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY: {
		auto &exports = library();
		const auto key = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		auto *value = static_cast<float *>( SV_RTCWPointer( vm, args[3], sizeof( vec3_t ) ) );
		result = exports.aas.AAS_VectorForBSPEpairKey( SV_RTCWInteger( args[1] ), key.data(), value );
		return true;
	}
	case Call::BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY: {
		auto &exports = library();
		const auto key = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		auto *value = static_cast<float *>( SV_RTCWPointer( vm, args[3], sizeof( float ) ) );
		result = exports.aas.AAS_FloatForBSPEpairKey( SV_RTCWInteger( args[1] ), key.data(), value );
		return true;
	}
	case Call::BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY: {
		auto &exports = library();
		const auto key = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		auto *value = static_cast<int *>( SV_RTCWPointer( vm, args[3], sizeof( int ) ) );
		result = exports.aas.AAS_IntForBSPEpairKey( SV_RTCWInteger( args[1] ), key.data(), value );
		return true;
	}
	case Call::BOTLIB_AAS_AREA_REACHABILITY:
		result = library().aas.AAS_AreaReachability( area( 1 ) );
		return true;
	case Call::BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA: {
		auto &exports = library();
		auto origin = SV_RTCWVector( vm, args[2] );
		result = exports.aas.AAS_AreaTravelTimeToGoalArea( area( 1 ), origin.data(),
			area( 3 ), SV_RTCWInteger( args[4] ) );
		return true;
	}
	case Call::BOTLIB_AAS_ENTITY_INFO: {
		auto &exports = library();
		// ET repeats the engine's aas_entityinfo_t layout field for field.
		static_assert( sizeof( aas_entityinfo_t ) == 140,
			"ET entity info is the engine's frozen 140-byte record" );
		auto *info = static_cast<aas_entityinfo_t *>(
			SV_RTCWPointer( vm, args[2], sizeof( aas_entityinfo_t ) ) );
		exports.aas.AAS_EntityInfo( SV_RTCWInteger( args[1] ), info );
		result = 0;
		return true;
	}
	default: return false;
	}
}


/* ET goal, movement and weapon imports. Goals travel through ET's own record so
 * urgency and the goal end time survive; movement results and console messages
 * are narrowed to ET's field set rather than copied from engine storage.
 * BOTLIB_AI_FIND_MATCH and BOTLIB_AI_MATCH_VARIABLE stay unhandled: ET's
 * bot_match_t stores raw module pointers where this engine stores offsets, and
 * that conversion has no evidence yet. BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT is
 * also absent because ET's aas_clientmove_t embeds a bsp_trace_t this engine
 * does not produce. */
static bool SV_WolfETGoalCall( const vm_t *vm, fntech3::wolfet::module::Call call,
	const intptr_t *args, intptr_t &result ) {
	using fntech3::wolfet::module::Call;
	namespace bot = fntech3::wolfet::bot;
	switch ( call ) {
	case Call::BOTLIB_AI_PUSH_GOAL: case Call::BOTLIB_AI_GET_TOP_GOAL:
	case Call::BOTLIB_AI_GET_SECOND_GOAL: case Call::BOTLIB_AI_CHOOSE_LTG_ITEM:
	case Call::BOTLIB_AI_CHOOSE_NBG_ITEM: case Call::BOTLIB_AI_TOUCHING_GOAL:
	case Call::BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
	case Call::BOTLIB_AI_GET_LEVEL_ITEM_GOAL: case Call::BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
	case Call::BOTLIB_AI_GET_MAP_LOCATION_GOAL: case Call::BOTLIB_AI_AVOID_GOAL_TIME: case Call::BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
	case Call::BOTLIB_AI_GOAL_NAME: case Call::BOTLIB_AI_INIT_LEVEL_ITEMS:
	case Call::BOTLIB_AI_UPDATE_ENTITY_ITEMS: case Call::BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
	case Call::BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC: case Call::BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
	case Call::BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
	case Call::BOTLIB_AI_INIT_MOVE_STATE: case Call::BOTLIB_AI_MOVE_TO_GOAL:
	case Call::BOTLIB_AI_MOVE_IN_DIRECTION: case Call::BOTLIB_AI_INIT_AVOID_REACH:
	case Call::BOTLIB_AI_REACHABILITY_AREA: case Call::BOTLIB_AI_MOVEMENT_VIEW_TARGET:
	case Call::BOTLIB_AI_PREDICT_VISIBLE_POSITION:
	case Call::BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON: case Call::BOTLIB_AI_GET_WEAPON_INFO:
	case Call::BOTLIB_AI_QUEUE_CONSOLE_MESSAGE: case Call::BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
	case Call::BOTLIB_AI_NEXT_CONSOLE_MESSAGE: case Call::BOTLIB_AI_NUM_CONSOLE_MESSAGE:
	case Call::BOTLIB_AI_CHAT_LENGTH: case Call::BOTLIB_AI_GET_CHAT_MESSAGE:
	case Call::BOTLIB_AI_FIND_MATCH: case Call::BOTLIB_AI_MATCH_VARIABLE:
	case Call::BOTLIB_AI_STRING_CONTAINS: case Call::BOTLIB_AI_UNIFY_WHITE_SPACES:
	case Call::BOTLIB_AI_REPLACE_SYNONYMS: case Call::BOTLIB_AI_INITIAL_CHAT:
	case Call::BOTLIB_AI_NUM_INITIAL_CHATS: case Call::BOTLIB_AI_REPLY_CHAT:
		break;
	default: return false;
	}
	if ( !botlib_export ) Com_Error( ERR_DROP, "WolfET bot library interface is unavailable" );
	auto &ai = botlib_export->ai;
	const auto readGoal = [vm, args, &ai]( int index, bot_goal_t &goal ) {
		(void)ai;
		fntech3_wolfet_bot_goal_t input;
		std::memcpy( &input, SV_RTCWPointer( vm, args[index], sizeof( input ) ), sizeof( input ) );
		if ( !bot::GoalInput( input, goal ) ) Com_Error( ERR_DROP, "Invalid WolfET bot goal" );
	};
	const auto writeGoal = [vm, args]( int index, const bot_goal_t &goal ) {
		auto *destination = SV_RTCWPointer( vm, args[index], sizeof( fntech3_wolfet_bot_goal_t ) );
		fntech3_wolfet_bot_goal_t stored;
		std::memcpy( &stored, destination, sizeof( stored ) );
		// The module's urgency and end time are its own; the library never sets them.
		const auto output = bot::GoalOutput( goal, stored.urgency, stored.goalEndTime );
		std::memcpy( destination, &output, sizeof( output ) );
	};
	const auto text = [vm, args]( int index, int size ) {
		if ( size <= 0 ) Com_Error( ERR_DROP, "Invalid WolfET output buffer size" );
		return static_cast<char *>( SV_RTCWPointer( vm, args[index], static_cast<std::size_t>( size ) ) );
	};
	const auto scalar = []( float value, intptr_t &out ) {
		int bits;
		std::memcpy( &bits, &value, sizeof( bits ) );
		out = bits;
	};
	switch ( call ) {
	case Call::BOTLIB_AI_PUSH_GOAL: {
		bot_goal_t goal;
		readGoal( 2, goal );
		ai.BotPushGoal( SV_RTCWInteger( args[1] ), &goal );
		break;
	}
	case Call::BOTLIB_AI_GET_TOP_GOAL: case Call::BOTLIB_AI_GET_SECOND_GOAL: {
		bot_goal_t goal{};
		result = call == Call::BOTLIB_AI_GET_TOP_GOAL
			? ai.BotGetTopGoal( SV_RTCWInteger( args[1] ), &goal )
			: ai.BotGetSecondGoal( SV_RTCWInteger( args[1] ), &goal );
		if ( result ) writeGoal( 2, goal );
		return true;
	}
	case Call::BOTLIB_AI_CHOOSE_LTG_ITEM:
		result = ai.BotChooseLTGItem( SV_RTCWInteger( args[1] ),
			SV_RTCWVector( vm, args[2] ).data(),
			static_cast<int *>( SV_RTCWPointer( vm, args[3], sizeof( int ) ) ),
			SV_RTCWInteger( args[4] ) );
		return true;
	case Call::BOTLIB_AI_CHOOSE_NBG_ITEM: {
		bot_goal_t goal;
		readGoal( 5, goal );
		result = ai.BotChooseNBGItem( SV_RTCWInteger( args[1] ),
			SV_RTCWVector( vm, args[2] ).data(),
			static_cast<int *>( SV_RTCWPointer( vm, args[3], sizeof( int ) ) ),
			SV_RTCWInteger( args[4] ), &goal, SV_RTCWFloat( args[6] ) );
		return true;
	}
	case Call::BOTLIB_AI_TOUCHING_GOAL: {
		bot_goal_t goal;
		readGoal( 2, goal );
		result = ai.BotTouchingGoal( SV_RTCWVector( vm, args[1] ).data(), &goal );
		return true;
	}
	case Call::BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE: {
		bot_goal_t goal;
		readGoal( 4, goal );
		result = ai.BotItemGoalInVisButNotVisible( SV_RTCWInteger( args[1] ),
			SV_RTCWVector( vm, args[2] ).data(), SV_RTCWVector( vm, args[3] ).data(), &goal );
		return true;
	}
	case Call::BOTLIB_AI_GET_LEVEL_ITEM_GOAL: {
		bot_goal_t goal{};
		const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		result = ai.BotGetLevelItemGoal( SV_RTCWInteger( args[1] ), name.data(), &goal );
		if ( result >= 0 ) writeGoal( 3, goal );
		return true;
	}
	case Call::BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL: {
		bot_goal_t goal{};
		result = ai.BotGetNextCampSpotGoal( SV_RTCWInteger( args[1] ), &goal );
		if ( result >= 0 ) writeGoal( 2, goal );
		return true;
	}
	case Call::BOTLIB_AI_GET_MAP_LOCATION_GOAL: {
		bot_goal_t goal{};
		const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[1] );
		result = ai.BotGetMapLocationGoal( name.data(), &goal );
		if ( result >= 0 ) writeGoal( 2, goal );
		return true;
	}
	case Call::BOTLIB_AI_AVOID_GOAL_TIME:
		scalar( ai.BotAvoidGoalTime( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) ), result );
		return true;
	case Call::BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
		ai.BotRemoveFromAvoidGoals( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) );
		break;
	case Call::BOTLIB_AI_GOAL_NAME: {
		const int size = SV_RTCWInteger( args[3] );
		ai.BotGoalName( SV_RTCWInteger( args[1] ), text( 2, size ), size );
		break;
	}
	case Call::BOTLIB_AI_INIT_LEVEL_ITEMS: ai.BotInitLevelItems(); break;
	case Call::BOTLIB_AI_UPDATE_ENTITY_ITEMS: ai.BotUpdateEntityItems(); break;
	case Call::BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC: {
		const auto file = SV_RTCWString<MAX_QPATH>( vm, args[2] );
		ai.BotSaveGoalFuzzyLogic( SV_RTCWInteger( args[1] ), file.data() );
		break;
	}
	case Call::BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
		ai.BotInterbreedGoalFuzzyLogic( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ),
			SV_RTCWInteger( args[3] ) );
		break;
	case Call::BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
		ai.BotMutateGoalFuzzyLogic( SV_RTCWInteger( args[1] ), SV_RTCWFloat( args[2] ) );
		break;
	case Call::BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION: {
		const int count = SV_RTCWInteger( args[1] );
		if ( count <= 0 || count > FNTECH3_WOLFET_MAX_AAS_QUERY )
			Com_Error( ERR_DROP, "Invalid WolfET genetic selection count" );
		auto *ranks = static_cast<float *>( SV_RTCWPointer( vm, args[2],
			static_cast<std::size_t>( count ) * sizeof( float ) ) );
		result = ai.GeneticParentsAndChildSelection( count, ranks,
			static_cast<int *>( SV_RTCWPointer( vm, args[3], sizeof( int ) ) ),
			static_cast<int *>( SV_RTCWPointer( vm, args[4], sizeof( int ) ) ),
			static_cast<int *>( SV_RTCWPointer( vm, args[5], sizeof( int ) ) ) );
		return true;
	}
	case Call::BOTLIB_AI_INIT_MOVE_STATE: {
		fntech3_wolfet_bot_initmove_t input;
		std::memcpy( &input, SV_RTCWPointer( vm, args[2], sizeof( input ) ), sizeof( input ) );
		bot_initmove_t converted;
		if ( !bot::InitMoveInput( input, converted ) )
			Com_Error( ERR_DROP, "Invalid WolfET movement setup" );
		ai.BotInitMoveState( SV_RTCWInteger( args[1] ), &converted );
		break;
	}
	case Call::BOTLIB_AI_MOVE_TO_GOAL: {
		bot_goal_t goal;
		readGoal( 3, goal );
		bot_moveresult_t moveresult{};
		auto *destination = SV_RTCWPointer( vm, args[1], sizeof( fntech3_wolfet_bot_moveresult_t ) );
		ai.BotMoveToGoal( &moveresult, SV_RTCWInteger( args[2] ), &goal, SV_RTCWInteger( args[4] ) );
		const auto output = bot::MoveResultOutput( moveresult );
		std::memcpy( destination, &output, sizeof( output ) );
		break;
	}
	case Call::BOTLIB_AI_INIT_AVOID_REACH:
		// Clearing the avoid-reach arrays is engine-internal; the frozen bot
		// export has no entry for it.
		BotInitAvoidReach( SV_RTCWInteger( args[1] ) );
		break;
	case Call::BOTLIB_AI_MOVE_IN_DIRECTION:
		result = ai.BotMoveInDirection( SV_RTCWInteger( args[1] ),
			SV_RTCWVector( vm, args[2] ).data(), SV_RTCWFloat( args[3] ),
			SV_RTCWInteger( args[4] ) );
		return true;
	case Call::BOTLIB_AI_REACHABILITY_AREA:
		result = ai.BotReachabilityArea( SV_RTCWVector( vm, args[1] ).data(),
			SV_RTCWInteger( args[2] ) );
		return true;
	case Call::BOTLIB_AI_MOVEMENT_VIEW_TARGET: {
		bot_goal_t goal;
		readGoal( 2, goal );
		auto *target = static_cast<float *>( SV_RTCWPointer( vm, args[5], sizeof( vec3_t ) ) );
		result = ai.BotMovementViewTarget( SV_RTCWInteger( args[1] ), &goal,
			SV_RTCWInteger( args[3] ), SV_RTCWFloat( args[4] ), target );
		return true;
	}
	case Call::BOTLIB_AI_PREDICT_VISIBLE_POSITION: {
		bot_goal_t goal;
		readGoal( 3, goal );
		auto *target = static_cast<float *>( SV_RTCWPointer( vm, args[5], sizeof( vec3_t ) ) );
		result = ai.BotPredictVisiblePosition( SV_RTCWVector( vm, args[1] ).data(),
			SV_RTCWInteger( args[2] ), &goal, SV_RTCWInteger( args[4] ), target );
		return true;
	}
	case Call::BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
		result = ai.BotChooseBestFightWeapon( SV_RTCWInteger( args[1] ),
			static_cast<int *>( SV_RTCWPointer( vm, args[2], sizeof( int ) ) ) );
		return true;
	case Call::BOTLIB_AI_GET_WEAPON_INFO: {
		// ET repeats the engine's weaponinfo_t and projectileinfo_t layouts.
		static_assert( sizeof( weaponinfo_t ) == 552, "ET weapon info is 552 bytes" );
		auto *info = static_cast<weaponinfo_t *>(
			SV_RTCWPointer( vm, args[3], sizeof( weaponinfo_t ) ) );
		ai.BotGetWeaponInfo( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ), info );
		break;
	}
	case Call::BOTLIB_AI_QUEUE_CONSOLE_MESSAGE: {
		const auto message = SV_RTCWString<MAX_STRING_CHARS>( vm, args[3] );
		ai.BotQueueConsoleMessage( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ),
			message.data() );
		break;
	}
	case Call::BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
		ai.BotRemoveConsoleMessage( SV_RTCWInteger( args[1] ), SV_RTCWInteger( args[2] ) );
		break;
	case Call::BOTLIB_AI_NUM_CONSOLE_MESSAGE:
		result = ai.BotNumConsoleMessages( SV_RTCWInteger( args[1] ) );
		return true;
	case Call::BOTLIB_AI_NEXT_CONSOLE_MESSAGE: {
		bot_consolemessage_t message{};
		auto *destination = SV_RTCWPointer( vm, args[2],
			sizeof( fntech3_wolfet_bot_consolemessage_t ) );
		result = ai.BotNextConsoleMessage( SV_RTCWInteger( args[1] ), &message );
		if ( result ) {
			const auto output = bot::ConsoleMessageOutput( message );
			std::memcpy( destination, &output, sizeof( output ) );
		}
		return true;
	}
	case Call::BOTLIB_AI_CHAT_LENGTH:
		result = ai.BotChatLength( SV_RTCWInteger( args[1] ) );
		return true;
	case Call::BOTLIB_AI_GET_CHAT_MESSAGE: {
		const int size = SV_RTCWInteger( args[3] );
		ai.BotGetChatMessage( SV_RTCWInteger( args[1] ), text( 2, size ), size );
		break;
	}
	case Call::BOTLIB_AI_FIND_MATCH: {
		/* The engine records each variable as an offset into its own 256-byte
		 * copy; ET expects a pointer into the module's 150-byte copy. Translate
		 * both, and drop a variable whose text does not survive the shorter
		 * string rather than handing the module a pointer past its own record. */
		const auto subject = SV_RTCWString<MAX_MESSAGE_SIZE>( vm, args[1] );
		auto *destination = static_cast<fntech3_wolfet_bot_match_t *>(
			SV_RTCWPointer( vm, args[2], sizeof( fntech3_wolfet_bot_match_t ) ) );
		bot_match_t internal{};
		result = ai.BotFindMatch( subject.data(), &internal,
			static_cast<unsigned long>( args[3] ) );
		if ( !result ) return true;
		fntech3_wolfet_bot_match_t output{};
		output.type = internal.type;
		output.subtype = internal.subtype;
		std::size_t length = 0;
		while ( length + 1 < sizeof( output.string ) && length < sizeof( internal.string )
			&& internal.string[length] ) {
			output.string[length] = internal.string[length];
			++length;
		}
		output.string[length] = '\0';
		const auto base = reinterpret_cast<std::uintptr_t>( destination )
			+ offsetof( fntech3_wolfet_bot_match_t, string );
		// ET's record holds a 32-bit address because its modules are x86.
		if ( base + sizeof( output.string ) > 0xffffffffu )
			Com_Error( ERR_DROP, "WolfET match record outside the module address space" );
		for ( int index = 0; index < FNTECH3_WOLFET_MAX_MATCHVARIABLES; ++index ) {
			const int offset = internal.variables[index].offset;
			const int size = internal.variables[index].length;
			if ( offset < 0 || size < 0 || static_cast<std::size_t>( offset ) >= length
				|| static_cast<std::size_t>( offset + size ) > length ) {
				continue;
			}
			output.variables[index].ptr = static_cast<std::uint32_t>( base + offset );
			output.variables[index].length = size;
		}
		std::memcpy( destination, &output, sizeof( output ) );
		return true;
	}
	case Call::BOTLIB_AI_MATCH_VARIABLE: {
		const int variable = SV_RTCWInteger( args[2] );
		const int size = SV_RTCWInteger( args[4] );
		auto *buffer = text( 3, size );
		buffer[0] = '\0';
		if ( variable < 0 || variable >= FNTECH3_WOLFET_MAX_MATCHVARIABLES ) {
			// The original prints and returns an empty string for this.
			result = 0;
			return true;
		}
		fntech3_wolfet_bot_match_t input;
		auto *record = SV_RTCWPointer( vm, args[1], sizeof( input ) );
		std::memcpy( &input, record, sizeof( input ) );
		const auto base = reinterpret_cast<std::uintptr_t>( record )
			+ offsetof( fntech3_wolfet_bot_match_t, string );
		const std::uintptr_t pointer = input.variables[variable].ptr;
		const int stored = input.variables[variable].length;
		// A variable must point inside the record the module handed us.
		if ( !pointer || stored < 0 || pointer < base
			|| pointer + static_cast<std::uintptr_t>( stored ) >= base + sizeof( input.string ) ) {
			result = 0;
			return true;
		}
		int copy = stored < size ? stored + 1 : size;
		Q_strncpyz( buffer, reinterpret_cast<const char *>( pointer ), copy );
		result = 0;
		return true;
	}
	case Call::BOTLIB_AI_STRING_CONTAINS: {
		const auto first = SV_RTCWString<MAX_STRING_CHARS>( vm, args[1] );
		const auto second = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		result = ai.StringContains( first.data(), second.data(), SV_RTCWInteger( args[3] ) );
		return true;
	}
	case Call::BOTLIB_AI_UNIFY_WHITE_SPACES: {
		// Same bounded round trip: ET gives no size for this in-place rewrite.
		auto chat = SV_RTCWString<FNTECH3_WOLFET_MAX_MESSAGE_SIZE>( vm, args[1] );
		std::array<char, MAX_MESSAGE_SIZE> working{};
		Q_strncpyz( working.data(), chat.data(), static_cast<int>( working.size() ) );
		ai.UnifyWhiteSpaces( working.data() );
		auto *destination = text( 1, FNTECH3_WOLFET_MAX_MESSAGE_SIZE );
		Q_strncpyz( destination, working.data(), FNTECH3_WOLFET_MAX_MESSAGE_SIZE );
		break;
	}
	case Call::BOTLIB_AI_REPLACE_SYNONYMS: {
		// ET supplies no buffer size. Work in engine-sized storage and return at
		// most ET's own 150-byte chat limit so a longer synonym cannot overrun
		// the module's record.
		auto chat = SV_RTCWString<FNTECH3_WOLFET_MAX_MESSAGE_SIZE>( vm, args[1] );
		std::array<char, MAX_MESSAGE_SIZE> working{};
		Q_strncpyz( working.data(), chat.data(), static_cast<int>( working.size() ) );
		ai.BotReplaceSynonyms( working.data(), static_cast<int>( working.size() ),
			static_cast<unsigned long>( args[2] ) );
		auto *destination = text( 1, FNTECH3_WOLFET_MAX_MESSAGE_SIZE );
		Q_strncpyz( destination, working.data(), FNTECH3_WOLFET_MAX_MESSAGE_SIZE );
		break;
	}
	case Call::BOTLIB_AI_NUM_INITIAL_CHATS: {
		const auto type = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		result = ai.BotNumInitialChats( SV_RTCWInteger( args[1] ), type.data() );
		return true;
	}
	case Call::BOTLIB_AI_INITIAL_CHAT: case Call::BOTLIB_AI_REPLY_CHAT: {
		// Eight optional substitution strings follow the fixed arguments.
		const int first = call == Call::BOTLIB_AI_INITIAL_CHAT ? 4 : 5;
		std::array<std::array<char, MAX_STRING_CHARS>, 8> variables{};
		std::array<char *, 8> pointers{};
		for ( int index = 0; index < 8; ++index ) {
			if ( !args[first + index] ) continue;
			variables[index] = SV_RTCWString<MAX_STRING_CHARS>( vm, args[first + index] );
			pointers[index] = variables[index].data();
		}
		const auto name = SV_RTCWString<MAX_STRING_CHARS>( vm, args[2] );
		if ( call == Call::BOTLIB_AI_INITIAL_CHAT ) {
			ai.BotInitialChat( SV_RTCWInteger( args[1] ), name.data(), SV_RTCWInteger( args[3] ),
				pointers[0], pointers[1], pointers[2], pointers[3], pointers[4], pointers[5],
				pointers[6], pointers[7] );
			break;
		}
		result = ai.BotReplyChat( SV_RTCWInteger( args[1] ), name.data(), SV_RTCWInteger( args[3] ),
			SV_RTCWInteger( args[4] ), pointers[0], pointers[1], pointers[2], pointers[3],
			pointers[4], pointers[5], pointers[6], pointers[7] );
		return true;
	}
	default: return false;
	}
	result = 0;
	return true;
}

/* Navigation queries whose ET argument contract is plain scalars and vectors. */
static bool SV_WolfETNavigationCall( const vm_t *vm, fntech3::wolfet::module::Call call,
	const intptr_t *args, intptr_t &result ) {
	using fntech3::wolfet::module::Call;
	switch ( call ) {
	case Call::BOTLIB_AAS_SETCURRENTWORLD:
		if ( !AAS_SetCurrentWorld( SV_RTCWInteger( args[1] ) ) )
			Com_Error( ERR_DROP, "WolfET navigation world outside profile" );
		result = 0;
		return true;
	case Call::BOTLIB_AAS_SETAASBLOCKINGENTITY: {
		const auto mins = SV_RTCWVector( vm, args[1] );
		const auto maxs = SV_RTCWVector( vm, args[2] );
		if ( !SV_ValidWorldBounds( mins.data(), maxs.data() ) )
			Com_Error( ERR_DROP, "Inverted WolfET navigation bounds" );
		// ET's own reset sentinel, bit policy and mover retry, not RTCW's.
		AAS_WolfETSetBlockingEntity( mins.data(), maxs.data(), SV_RTCWInteger( args[3] ) );
		result = 0;
		return true;
	}
	case Call::BOTLIB_AAS_AREA_CENTER: {
		auto *center = static_cast<float *>( SV_RTCWPointer( vm, args[2], sizeof( vec3_t ) ) );
		// The original leaves the caller's vector alone for an invalid area.
		AAS_WolfETAreaCenter( SV_RTCWInteger( args[1] ), center );
		result = 0;
		return true;
	}
	case Call::BOTLIB_AAS_AREA_WAYPOINT: {
		auto *center = static_cast<float *>( SV_RTCWPointer( vm, args[2], sizeof( vec3_t ) ) );
		result = AAS_WolfETAreaWaypoint( SV_RTCWInteger( args[1] ), center ) ? qtrue : qfalse;
		return true;
	}
	case Call::BOTLIB_AAS_AREA_LADDER:
		result = AAS_WolfETAreaLadder( SV_RTCWInteger( args[1] ) );
		return true;
	case Call::BOTLIB_AAS_RT_SHOWROUTE:
		// The release body is entirely inside #ifdef DEBUG, so it draws nothing
		// and touches no navigation data. Arguments are still validated.
		SV_RTCWVector( vm, args[1] );
		SV_RTCWInteger( args[2] );
		SV_RTCWInteger( args[3] );
		result = 0;
		return true;
	case Call::BOTLIB_AAS_LISTAREASINRANGE: {
		const int count = SV_RTCWInteger( args[6] );
		if ( count <= 0 || count > FNTECH3_WOLFET_MAX_AAS_QUERY )
			Com_Error( ERR_DROP, "Invalid WolfET area-range count" );
		auto source = SV_RTCWVector( vm, args[1] );
		auto *areas = static_cast<vec_t *>( SV_RTCWPointer( vm, args[5],
			static_cast<std::size_t>( count ) * sizeof( vec3_t ) ) );
		result = AAS_WolfETListAreasInRange( source.data(), SV_RTCWInteger( args[2] ),
			SV_RTCWFloat( args[3] ), SV_RTCWInteger( args[4] ),
			reinterpret_cast<vec3_t *>( areas ), count );
		return true;
	}
	case Call::BOTLIB_AAS_AVOIDDANGERAREA: {
		auto source = SV_RTCWVector( vm, args[1] );
		auto danger = SV_RTCWVector( vm, args[3] );
		result = AAS_WolfETAvoidDangerArea( source.data(), SV_RTCWInteger( args[2] ),
			danger.data(), SV_RTCWInteger( args[4] ), SV_RTCWFloat( args[5] ),
			SV_RTCWInteger( args[6] ) );
		return true;
	}
	case Call::BOTLIB_AAS_ALTROUTEGOALS: {
		const int count = SV_RTCWInteger( args[5] );
		if ( count <= 0 || count > FNTECH3_WOLFET_MAX_AAS_QUERY )
			Com_Error( ERR_DROP, "Invalid WolfET alternative route count" );
		auto start = SV_RTCWVector( vm, args[1] );
		auto goal = SV_RTCWVector( vm, args[2] );
		auto *goals = static_cast<aas_altroutegoal_t *>( SV_RTCWPointer( vm, args[4],
			static_cast<std::size_t>( count ) * sizeof( aas_altroutegoal_t ) ) );
		// ET's sixth argument is a debug colour its release body never uses.
		SV_RTCWInteger( args[6] );
		static_assert( sizeof( aas_altroutegoal_t ) == 24,
			"ET alternative route goal is a frozen 24-byte record" );
		result = AAS_WolfETAlternativeRouteGoals( start.data(), goal.data(),
			SV_RTCWInteger( args[3] ), goals, count );
		return true;
	}
	case Call::BOTLIB_AAS_NEARESTHIDEAREA: {
		auto origin = SV_RTCWVector( vm, args[2] );
		auto enemy = SV_RTCWVector( vm, args[5] );
		// The last argument is optional: without it there is no distance limit.
		std::array<float, 3> reference{};
		if ( args[9] ) reference = SV_RTCWVector( vm, args[9] );
		result = AAS_WolfETNearestHideArea( SV_RTCWInteger( args[1] ), origin.data(),
			SV_RTCWInteger( args[3] ), SV_RTCWInteger( args[4] ), enemy.data(),
			SV_RTCWInteger( args[6] ), SV_RTCWInteger( args[7] ), SV_RTCWFloat( args[8] ),
			args[9] ? reference.data() : nullptr );
		if ( result < 0 ) Com_Error( ERR_DROP, "Invalid WolfET hide area" );
		return true;
	}
	case Call::BOTLIB_AAS_RETREAT: {
		const int spots = SV_RTCWInteger( args[2] );
		if ( spots < 0 || spots > FNTECH3_WOLFET_MAX_AAS_QUERY )
			Com_Error( ERR_DROP, "Invalid WolfET danger spot count" );
		auto *list = spots ? static_cast<int *>( SV_RTCWPointer( vm, args[1],
			static_cast<std::size_t>( spots ) * sizeof( int ) ) ) : nullptr;
		auto source = SV_RTCWVector( vm, args[3] );
		auto danger = SV_RTCWVector( vm, args[5] );
		result = AAS_WolfETRetreat( list, spots, source.data(), SV_RTCWInteger( args[4] ),
			danger.data(), SV_RTCWInteger( args[6] ), SV_RTCWFloat( args[7] ),
			SV_RTCWFloat( args[8] ), SV_RTCWInteger( args[9] ) );
		if ( result < 0 ) Com_Error( ERR_DROP, "Invalid WolfET retreat area" );
		return true;
	}
	case Call::BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT: {
		auto *destination = SV_RTCWPointer( vm, args[1],
			sizeof( fntech3_wolfet_aas_clientmove_t ) );
		auto origin = SV_RTCWVector( vm, args[3] );
		auto velocity = SV_RTCWVector( vm, args[6] );
		auto command = SV_RTCWVector( vm, args[7] );
		// ET names the fourth argument presencetype in its trap but passes it
		// as the entity its hit-entity stop watches for.
		aas_wolfet_clientmove_t predicted{};
		result = AAS_WolfETPredictClientMovement( &predicted, SV_RTCWInteger( args[2] ),
			origin.data(), SV_RTCWInteger( args[4] ), SV_RTCWInteger( args[5] ),
			velocity.data(), command.data(), SV_RTCWInteger( args[8] ),
			SV_RTCWInteger( args[9] ), SV_RTCWFloat( args[10] ),
			SV_RTCWInteger( args[11] ), SV_RTCWInteger( args[12] ) );
		// The thirteenth argument only drives the original's debug drawing.
		SV_RTCWInteger( args[13] );
		fntech3_wolfet_aas_clientmove_t output{};
		if ( !SV_WolfETPredictionOutput( predicted, output ) )
			Com_Error( ERR_DROP, "Invalid WolfET movement prediction" );
		std::memcpy( destination, &output, sizeof( output ) );
		return true;
	}
	case Call::BOTLIB_AAS_RECORDTEAMDEATHAREA:
		// The published body opens with an unconditional return; nothing is
		// recorded in the shipped engine.
		SV_RTCWVector( vm, args[1] );
		for ( int index = 2; index <= 5; ++index ) SV_RTCWInteger( args[index] );
		result = 0;
		return true;
	default: return false;
	}
}
#endif
