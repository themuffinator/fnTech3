/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_BOT_ACTIONS_HPP
#define FNTECH3_STVEF_BOT_ACTIONS_HPP

#include "stvef_game_abi.h"
#include "../botlib/botlib.h"

#include <cstdint>
#include <cstring>

/* Holomatch's qagame turns bot input into user commands with Quake III 1.17's
 * action bits, not 1.32's (BotInputToUserCommand in the retail 1.20 pak2
 * qagame tests 0x200 for a delayed jump, 0x2000 for alternate fire and so on).
 * The bit order below is that frozen contract; bit 13 is Elite Force's
 * alternate attack. The botlib keeps its canonical flags and they are
 * translated here, at the module boundary. */
namespace fntech3::stvef::bot {

inline constexpr unsigned ActionBits[] = {
	ACTION_ATTACK, ACTION_USE, ACTION_RESPAWN, ACTION_JUMP, ACTION_CROUCH,
	ACTION_MOVEFORWARD, ACTION_MOVEBACK, ACTION_MOVELEFT, ACTION_MOVERIGHT,
	ACTION_DELAYEDJUMP, ACTION_TALK, ACTION_GESTURE, ACTION_WALK,
	ACTION_STVEF_ALTATTACK
};

inline fntech3_stvef_bot_input_t ActionOutput( const bot_input_t &in ) {
	fntech3_stvef_bot_input_t out{};
	out.thinktime = in.thinktime;
	out.speed = in.speed;
	out.weapon = in.weapon;
	std::memcpy( out.dir, in.dir, sizeof( out.dir ) );
	std::memcpy( out.viewangles, in.viewangles, sizeof( out.viewangles ) );
	std::uint32_t flags = 0;
	for ( unsigned bit = 0; bit < sizeof( ActionBits ) / sizeof( ActionBits[0] ); ++bit ) {
		if ( static_cast<unsigned>( in.actionflags ) & ActionBits[bit] ) flags |= 1u << bit;
	}
	std::memcpy( &out.actionflags, &flags, sizeof( flags ) );
	return out;
}

} // namespace fntech3::stvef::bot
#endif
