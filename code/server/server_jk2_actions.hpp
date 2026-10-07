/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JK2_ACTIONS_HPP
#define FNTECH3_SERVER_JK2_ACTIONS_HPP

#include "../botlib/be_ea.h"
#include "server_game_data.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>

/* Retail jk2mp.exe 1.02a: the action table's slots 0x98/0x9c reach
 * 0x4b1570/0x4b1590 (bindings 0x4b1194/0x4b119b); they OR 0x200000 and
 * 0x100000 into offset 32 of a 40-byte input. 0x4b1820 copies that record;
 * 0x4b1850 resets it, carrying a jump as 0x800000 at 0x4b1880.
 * The QVM converts those attack/force bits to buttons 128/512 at 7540/7637.
 * These bits have Raven meanings only at this profile's module boundary.
 * See docs/fntech3/2026-10-07-jk2-bot-combat.md. */
inline constexpr int JK2ActionForcePower = 0x00100000;
inline constexpr int JK2ActionAltAttack = 0x00200000;
inline constexpr int JK2ActionJumpedLastFrame = 0x00800000;

static int SV_JK2BotActionClient( intptr_t requested )
{
	if ( requested < 0 || requested > INT_MAX || !Com_GameProfile()
		|| requested >= Com_GameProfile()->maxClients || !botlib_export
		|| !EA_ClientInputAvailable( static_cast<int>( requested ) ) ) {
		Com_Error( ERR_DROP, "Jedi Outcast bot action client has no input state" );
	}
	return static_cast<int>( requested );
}

static ea_export_s &SV_JK2BotActionInterface( intptr_t requested )
{
	SV_JK2BotActionClient( requested );
	return botlib_export->ea;
}

static bot_input_t SV_JK2BotActionOutput( const bot_input_t &input )
{
	static_assert( sizeof( bot_input_t ) == 40, "Jedi Outcast bot input size" );
	static_assert( offsetof( bot_input_t, actionflags ) == 32,
		"Jedi Outcast bot input action flag offset" );
	bot_input_t output = input;
	output.actionflags &= ~ACTION_JUMPEDLASTFRAME;
	if ( input.actionflags & ACTION_JUMPEDLASTFRAME ) {
		output.actionflags |= JK2ActionJumpedLastFrame;
	}
	return output;
}

static void *SV_JK2BotInputDestination( const vm_t *vm, intptr_t address )
{
	if ( !vm || !address ) Com_Error( ERR_DROP, "Jedi Outcast bot input destination is null" );
	if ( !vm->entryPoint && !vm->dllExports ) {
		/* The retail QVM puts this output on its program stack, beyond the
		 * file's exact data length. Check the full VM arena without masking an
		 * invalid address into it (vm.c allocates dataMask + 1 bytes). */
		if ( !vm->dataBase || address < 0
			|| static_cast<std::uint64_t>( address ) + sizeof( bot_input_t )
				> static_cast<std::uint64_t>( vm->dataMask ) + 1 ) {
			Com_Error( ERR_DROP, "Jedi Outcast bot input destination is outside VM data" );
		}
		return vm->dataBase + address;
	}
	if ( sizeof( bot_input_t ) > ( std::numeric_limits<std::uintptr_t>::max )()
		- static_cast<std::uintptr_t>( address ) ) {
		Com_Error( ERR_DROP, "Jedi Outcast bot input destination address overflows" );
	}
	return reinterpret_cast<void *>( address );
}

#endif
