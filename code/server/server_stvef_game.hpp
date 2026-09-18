/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_STVEF_GAME_HPP
#define FNTECH3_SERVER_STVEF_GAME_HPP
#include "../compat/stvef_module_abi.h"
#include "../compat/stvef_state_adapter.hpp"
#include "../compat/stvef_bot_actions.hpp"
#include "../compat/stvef_game_imports.hpp"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_weap.h"
#include "../botlib/be_ea.h"

#include <algorithm>

/* Holomatch's qagame numbers its imports as the Quake III 1.17 engine did;
 * stvef_game_imports.hpp says which keep Quake III 1.32's contract. Those
 * continue into the shared dispatcher and everything else is decoded here
 * (stvoyHM.exe's game dispatcher at 0x431030 handles the same set). */

static int SV_STVEFBotClient( intptr_t value ) {
	if ( value < 0 || value >= Com_GameProfile()->maxClients ||
		!EA_ClientInputAvailable( static_cast<int>( value ) ) ) {
		Com_Error( ERR_DROP, "Elite Force bot action for a client without input state" );
	}
	return static_cast<int>( value );
}

static float SV_STVEFFloat( intptr_t value ) {
	const std::int32_t bits = static_cast<std::int32_t>( value );
	float result;
	std::memcpy( &result, &bits, sizeof( result ) );
	return result;
}

static bool SV_STVEFGameCall( const intptr_t *args, intptr_t &operation, intptr_t &result ) {
	using namespace fntech3::stvef::module;
	const Call call = Lookup( COM_GAME_STVEF_MP, Module::GAME, Direction::IMPORT, args[0] );
	const auto pointer = [args]( int n ) { return VM_ArgPtr( args[n] ); };
	result = 0;
	switch ( call ) {
	case Call::G_GET_USERCMD: {
		VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_stvef_mp_usercmd_t ) );
		usercmd_t internal{};
		SV_GetUsercmd( args[1], &internal );
		fntech3_stvef_mp_usercmd_t external{};
		if ( !fntech3::stvef::state::FromInternal( internal, external ) ) {
			Com_Error( ERR_DROP, "Invalid Elite Force user command" );
		}
		std::memcpy( pointer( 2 ), &external, sizeof( external ) );
		return true;
	}
	case Call::BOTLIB_USER_COMMAND: {
		const int clientNum = static_cast<int>( args[1] );
		if ( !SV_IsClientIndex( clientNum ) ) return true;
		VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_stvef_mp_usercmd_t ) );
		fntech3_stvef_mp_usercmd_t external;
		std::memcpy( &external, pointer( 2 ), sizeof( external ) );
		usercmd_t command{};
		if ( !fntech3::stvef::state::ToInternal( external, command ) ) {
			Com_Error( ERR_DROP, "Invalid Elite Force bot user command" );
		}
		SV_ClientThink( &SV_ClientForIndex( clientNum ), &command );
		return true;
	}
	case Call::G_ENTITIES_IN_BOX: {
		// Holomatch's game passes MAX_GENTITIES as the capacity of smaller
		// lists on its stack; retail writes only the entities it finds. Clamp
		// the capacity to the data segment instead of refusing the call.
		VM_CHECKBOUNDS( gvm, args[1], sizeof( vec3_t ) );
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		if ( args[4] < 0 ) Com_Error( ERR_DROP, "Elite Force entity list with a negative capacity" );
		int capacity = static_cast<int>( std::min<intptr_t>( args[4], MAX_GENTITIES ) );
		if ( !gvm->entryPoint && !gvm->dllExports ) {
			const std::uint32_t start = static_cast<std::uint32_t>( args[3] ) & gvm->dataMask;
			capacity = static_cast<int>( std::min<std::uint32_t>( static_cast<std::uint32_t>( capacity ),
				( gvm->dataMask + 1u - start ) / sizeof( int ) ) );
		}
		result = SV_AreaEntities( static_cast<const float *>( pointer( 1 ) ),
			static_cast<const float *>( pointer( 2 ) ), static_cast<int *>( pointer( 3 ) ), capacity );
		return true;
	}
	case Call::TRAP_PERPENDICULARVECTOR:
		VM_CHECKBOUNDS( gvm, args[1], sizeof( vec3_t ) );
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		fntech3::stvef::game::PerpendicularVector(
			static_cast<float *>( pointer( 1 ) ), static_cast<const float *>( pointer( 2 ) ) );
		return true;

	// Retail's dispatcher has no case for these three EF-only AAS queries.
	case Call::BOTLIB_AAS_ENTITY_VISIBLE:
	case Call::BOTLIB_AAS_IN_FIELD_OF_VISION:
	case Call::BOTLIB_AAS_VISIBLE_CLIENTS:
		Com_Error( ERR_DROP, "Bad game system trap: %ld", static_cast<long>( args[0] ) );
		return true;

	// The skill is an integer, and the chat name call has no client: the chat
	// state speaks as the client its caller names, with no private messages.
	case Call::BOTLIB_AI_LOAD_CHARACTER:
		result = botlib_export->ai.BotLoadCharacter( static_cast<char *>( pointer( 1 ) ),
			static_cast<float>( static_cast<std::int32_t>( args[2] ) ) );
		return true;
	case Call::BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
		// The 1.20 game passes a third argument that the botlib uses.
		result = BotChooseBestFightWeaponSTVEF( static_cast<int>( args[1] ),
			static_cast<int *>( pointer( 2 ) ), static_cast<int>( args[3] ) );
		return true;
	case Call::BOTLIB_AI_SET_CHAT_NAME:
		BotSetChatNameLegacy( static_cast<int>( args[1] ), static_cast<const char *>( pointer( 2 ) ) );
		return true;
	case Call::BOTLIB_AI_ENTER_CHAT:
		BotEnterChatLegacy( static_cast<int>( args[1] ), static_cast<int>( args[2] ),
			static_cast<int>( args[3] ) );
		return true;

	case Call::BOTLIB_EA_SAY: case Call::BOTLIB_EA_SAY_TEAM: case Call::BOTLIB_EA_COMMAND:
	case Call::BOTLIB_EA_USE_ITEM: case Call::BOTLIB_EA_DROP_ITEM:
	case Call::BOTLIB_EA_USE_INV: case Call::BOTLIB_EA_DROP_INV: {
		const int client = SV_STVEFBotClient( args[1] );
		const char *text = static_cast<const char *>( pointer( 2 ) );
		auto &ea = botlib_export->ea;
		if ( call == Call::BOTLIB_EA_SAY ) ea.EA_Say( client, text );
		else if ( call == Call::BOTLIB_EA_SAY_TEAM ) ea.EA_SayTeam( client, text );
		else if ( call == Call::BOTLIB_EA_COMMAND ) ea.EA_Command( client, text );
		else {
			const char *verb = call == Call::BOTLIB_EA_USE_ITEM ? "use" :
				call == Call::BOTLIB_EA_DROP_ITEM ? "drop" :
				call == Call::BOTLIB_EA_USE_INV ? "invuse" : "invdrop";
			ea.EA_Command( client, va( "%s %s", verb, text ) );
		}
		return true;
	}
	case Call::BOTLIB_EA_GESTURE: botlib_export->ea.EA_Gesture( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_SELECT_WEAPON:
		botlib_export->ea.EA_SelectWeapon( SV_STVEFBotClient( args[1] ), static_cast<int>( args[2] ) );
		return true;
	case Call::BOTLIB_EA_TALK: botlib_export->ea.EA_Talk( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_ATTACK: botlib_export->ea.EA_Attack( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_ALT_ATTACK:
		// Retail calls its own alternate-attack slot here (0x431d79), not EA_Attack.
		botlib_export->ea.EA_Action( SV_STVEFBotClient( args[1] ), ACTION_STVEF_ALTATTACK );
		return true;
	case Call::BOTLIB_EA_USE: botlib_export->ea.EA_Use( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_RESPAWN: botlib_export->ea.EA_Respawn( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_JUMP: botlib_export->ea.EA_Jump( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_DELAYED_JUMP: botlib_export->ea.EA_DelayedJump( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_CROUCH: botlib_export->ea.EA_Crouch( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_MOVE_UP: botlib_export->ea.EA_MoveUp( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_MOVE_DOWN: botlib_export->ea.EA_MoveDown( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_MOVE_FORWARD: botlib_export->ea.EA_MoveForward( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_MOVE_BACK: botlib_export->ea.EA_MoveBack( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_MOVE_LEFT: botlib_export->ea.EA_MoveLeft( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_MOVE_RIGHT: botlib_export->ea.EA_MoveRight( SV_STVEFBotClient( args[1] ) ); return true;
	case Call::BOTLIB_EA_MOVE: {
		const int client = SV_STVEFBotClient( args[1] );
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		botlib_export->ea.EA_Move( client, static_cast<float *>( pointer( 2 ) ), SV_STVEFFloat( args[3] ) );
		return true;
	}
	case Call::BOTLIB_EA_VIEW: {
		const int client = SV_STVEFBotClient( args[1] );
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		botlib_export->ea.EA_View( client, static_cast<float *>( pointer( 2 ) ) );
		return true;
	}
	case Call::BOTLIB_EA_END_REGULAR:
		botlib_export->ea.EA_EndRegular( SV_STVEFBotClient( args[1] ), SV_STVEFFloat( args[2] ) );
		return true;
	case Call::BOTLIB_EA_GET_INPUT: {
		const int client = SV_STVEFBotClient( args[1] );
		VM_CHECKBOUNDS( gvm, args[3], sizeof( fntech3_stvef_bot_input_t ) );
		bot_input_t input{};
		botlib_export->ea.EA_GetInput( client, SV_STVEFFloat( args[2] ), &input );
		const fntech3_stvef_bot_input_t output = fntech3::stvef::bot::ActionOutput( input );
		std::memcpy( pointer( 3 ), &output, sizeof( output ) );
		return true;
	}
	case Call::BOTLIB_EA_RESET_INPUT:
		botlib_export->ea.EA_ResetInput( SV_STVEFBotClient( args[1] ) );
		return true;

	default:
		if ( call == Call::Invalid || !fntech3::stvef::game::SharesQuake3Import( args[0] ) ) {
			Com_Error( ERR_DROP, "Bad game system trap: %ld", static_cast<long>( args[0] ) );
			return true;
		}
		operation = args[0];
		return false;
	}
}
#endif
