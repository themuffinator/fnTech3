/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_ACTIONS_HPP
#define FNTECH3_SERVER_JKA_ACTIONS_HPP

#include "../botlib/be_ea.h"
#include "server_game_data.h"

#include <climits>
#include <cstring>
#include <limits>

/* Retail jamp.exe, SHA256
 * e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b:
 * VA 0x509c00/0x509c20 set alternate attack/force power at input offset 32;
 * VA 0x509eb0 copies the 40-byte input; VA 0x509ee0 resets it and carries a
 * previous jump as 0x800000 (the write at 0x509f10). These are module contract
 * bits, not changes to the shared Quake III bot library's action meanings. */
inline constexpr int JKAActionForcePower = 0x00100000;
inline constexpr int JKAActionAltAttack = 0x00200000;
inline constexpr int JKAActionJumpedLastFrame = 0x00800000;

static int SV_JKABotActionClient( intptr_t requested )
{
	if ( requested < 0 || requested > INT_MAX || !Com_GameProfile()
		|| requested >= Com_GameProfile()->maxClients || !botlib_export
		|| !EA_ClientInputAvailable( static_cast<int>( requested ) ) ) {
		Com_Error( ERR_DROP, "Jedi Academy bot action client has no input state" );
	}
	return static_cast<int>( requested );
}

static ea_export_s &SV_JKABotActionInterface( intptr_t requested )
{
	SV_JKABotActionClient( requested );
	return botlib_export->ea;
}

static bot_input_t SV_JKABotActionOutput( const bot_input_t &input )
{
	static_assert( sizeof( bot_input_t ) == 40, "Jedi Academy bot input size" );
	static_assert( offsetof( bot_input_t, actionflags ) == 32,
		"Jedi Academy bot input action flag offset" );
	bot_input_t output = input;
	output.actionflags &= ~ACTION_JUMPEDLASTFRAME;
	if ( input.actionflags & ACTION_JUMPEDLASTFRAME ) {
		output.actionflags |= JKAActionJumpedLastFrame;
	}
	return output;
}

static void *SV_JKABotInputDestination( const vm_t *vm, intptr_t address )
{
	if ( !vm || !address ) Com_Error( ERR_DROP, "Jedi Academy bot input destination is null" );
	if ( !vm->entryPoint && !vm->dllExports ) {
		void *pointer;
		std::size_t remaining;
		if ( !fntech3::server::ResolveQvmGameDataPointer( vm->dataBase, vm->exactDataLength,
			address, pointer, remaining ) || remaining < sizeof( bot_input_t ) ) {
			Com_Error( ERR_DROP, "Jedi Academy bot input destination is outside VM data" );
		}
		return pointer;
	}
	if ( sizeof( bot_input_t ) > ( std::numeric_limits<std::uintptr_t>::max )()
		- static_cast<std::uintptr_t>( address ) ) {
		Com_Error( ERR_DROP, "Jedi Academy bot input destination address overflows" );
	}
	return reinterpret_cast<void *>( address );
}

#endif
