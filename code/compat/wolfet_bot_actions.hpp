/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_BOT_ACTIONS_HPP
#define FNTECH3_WOLFET_BOT_ACTIONS_HPP
#include "wolfet_game_abi.h"
#include "rtcw_bot_entity.hpp"
#include "../botlib/botlib.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_chat.h"
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace wolfet {
namespace bot {

/* ET's external ACTION_* order, from its game/botlib.h: the RTCW sequence with
 * ACTION_PRONE added at bit fourteen. Engine bit values differ from the module
 * contract, so every transfer is translated rather than copied. */
inline constexpr unsigned ActionBits[] = {
	ACTION_ATTACK, ACTION_USE, ACTION_RESPAWN, ACTION_JUMP, ACTION_CROUCH,
	ACTION_MOVEFORWARD, ACTION_MOVEBACK, ACTION_MOVELEFT, ACTION_MOVERIGHT,
	ACTION_DELAYEDJUMP, ACTION_TALK, ACTION_GESTURE, ACTION_WALK,
	ACTION_RTCW_RELOAD, ACTION_WOLFET_PRONE
};
inline constexpr unsigned ActionBitCount = 15;
inline constexpr unsigned KnownActionBits = 0x7fffu;

inline bool ActionInput( const fntech3_wolfet_bot_input_t &in, bot_input_t &out ) {
	static_assert( sizeof( in ) == 40 && alignof( fntech3_wolfet_bot_input_t ) == 4,
		"ET bot input is a frozen 40-byte record" );
	std::uint32_t time, speed;
	std::memcpy( &time, &in.thinktime, sizeof( time ) );
	std::memcpy( &speed, &in.speed, sizeof( speed ) );
	if ( ( time & 0x7f800000u ) == 0x7f800000u || ( speed & 0x7f800000u ) == 0x7f800000u
		|| !fntech3::rtcw::bot::FiniteVector( in.dir )
		|| !fntech3::rtcw::bot::FiniteVector( in.viewangles ) ) {
		return false;
	}
	out = {};
	out.thinktime = in.thinktime;
	out.speed = in.speed;
	out.weapon = in.weapon;
	VectorCopy( in.dir, out.dir );
	VectorCopy( in.viewangles, out.viewangles );
	for ( unsigned bit = 0; bit < ActionBitCount; ++bit ) {
		if ( static_cast<unsigned>( in.actionflags ) & ( 1u << bit ) ) {
			out.actionflags |= ActionBits[bit];
		}
	}
	return true;
}

inline fntech3_wolfet_bot_input_t ActionOutput( const bot_input_t &in, unsigned extraFlags ) {
	fntech3_wolfet_bot_input_t out{};
	out.thinktime = in.thinktime;
	out.speed = in.speed;
	out.weapon = in.weapon;
	VectorCopy( in.dir, out.dir );
	VectorCopy( in.viewangles, out.viewangles );
	unsigned flags = extraFlags & ~KnownActionBits;
	for ( unsigned bit = 0; bit < ActionBitCount; ++bit ) {
		if ( static_cast<unsigned>( in.actionflags ) & ActionBits[bit] ) {
			flags |= 1u << bit;
		}
	}
	std::memcpy( &out.actionflags, &flags, sizeof( flags ) );
	return out;
}


inline bool EntityState( const fntech3_wolfet_bot_entitystate_t &in, bot_entitystate_t &out ) {
	static_assert( sizeof( in ) == 112 && alignof( fntech3_wolfet_bot_entitystate_t ) == 4,
		"ET bot entity state is a frozen 112-byte record" );
	using fntech3::rtcw::bot::FiniteVector;
	if ( !FiniteVector( in.origin ) || !FiniteVector( in.angles ) || !FiniteVector( in.old_origin )
		|| !FiniteVector( in.mins ) || !FiniteVector( in.maxs ) ) {
		return false;
	}
	// SOLID_BBOX is 2 in ET's game ABI. BSP bounds come from the map instead.
	if ( in.solid == 2 && !fntech3::rtcw::bot::ValidBounds( in.origin, in.mins, in.maxs ) ) {
		return false;
	}
	out = {};
	out.type = in.type; out.flags = in.flags;
	VectorCopy( in.origin, out.origin ); VectorCopy( in.angles, out.angles );
	VectorCopy( in.old_origin, out.old_origin );
	VectorCopy( in.mins, out.mins ); VectorCopy( in.maxs, out.maxs );
	out.groundent = in.groundent; out.solid = in.solid;
	out.modelindex = in.modelindex; out.modelindex2 = in.modelindex2;
	out.frame = in.frame; out.event = in.event; out.eventParm = in.eventParm;
	out.powerups = in.powerups; out.weapon = in.weapon;
	out.legsAnim = in.legsAnim; out.torsoAnim = in.torsoAnim;
	return true;
}


/* ET goals carry urgency and an end time that the engine's goal record has no
 * field for. They round-trip through the module's record rather than the
 * engine's, so a goal the module stored keeps both values. */
inline bool GoalInput( const fntech3_wolfet_bot_goal_t &in, bot_goal_t &out ) {
	static_assert( sizeof( in ) == 64 && alignof( fntech3_wolfet_bot_goal_t ) == 4,
		"ET bot goal is a frozen 64-byte record" );
	using fntech3::rtcw::bot::FiniteVector;
	if ( !FiniteVector( in.origin ) || !FiniteVector( in.mins ) || !FiniteVector( in.maxs ) ) {
		return false;
	}
	out = {};
	VectorCopy( in.origin, out.origin );
	VectorCopy( in.mins, out.mins );
	VectorCopy( in.maxs, out.maxs );
	out.areanum = in.areanum; out.entitynum = in.entitynum;
	out.number = in.number; out.flags = in.flags; out.iteminfo = in.iteminfo;
	return true;
}

inline fntech3_wolfet_bot_goal_t GoalOutput( const bot_goal_t &in,
	int urgency = 0, int goalEndTime = 0 ) {
	fntech3_wolfet_bot_goal_t out{};
	VectorCopy( in.origin, out.origin );
	VectorCopy( in.mins, out.mins );
	VectorCopy( in.maxs, out.maxs );
	out.areanum = in.areanum; out.entitynum = in.entitynum;
	out.number = in.number; out.flags = in.flags; out.iteminfo = in.iteminfo;
	out.urgency = urgency; out.goalEndTime = goalEndTime;
	return out;
}

inline bool InitMoveInput( const fntech3_wolfet_bot_initmove_t &in, bot_initmove_t &out ) {
	static_assert( sizeof( in ) == 72 && alignof( fntech3_wolfet_bot_initmove_t ) == 4,
		"ET bot movement setup is a frozen 72-byte record" );
	using fntech3::rtcw::bot::FiniteVector;
	std::uint32_t think;
	std::memcpy( &think, &in.thinktime, sizeof( think ) );
	if ( ( think & 0x7f800000u ) == 0x7f800000u || !FiniteVector( in.origin )
		|| !FiniteVector( in.velocity ) || !FiniteVector( in.viewoffset )
		|| !FiniteVector( in.viewangles ) ) {
		return false;
	}
	out = {};
	VectorCopy( in.origin, out.origin );
	VectorCopy( in.velocity, out.velocity );
	VectorCopy( in.viewoffset, out.viewoffset );
	VectorCopy( in.viewangles, out.viewangles );
	out.entitynum = in.entitynum; out.client = in.client;
	out.thinktime = in.thinktime; out.presencetype = in.presencetype;
	out.or_moveflags = in.or_moveflags;
	return true;
}

inline fntech3_wolfet_bot_moveresult_t MoveResultOutput( const bot_moveresult_t &in ) {
	static_assert( sizeof( fntech3_wolfet_bot_moveresult_t ) == 52
		&& alignof( fntech3_wolfet_bot_moveresult_t ) == 4,
		"ET bot movement result is a frozen 52-byte record" );
	fntech3_wolfet_bot_moveresult_t out{};
	out.failure = in.failure; out.type = in.type; out.blocked = in.blocked;
	out.blockentity = in.blockentity; out.traveltype = in.traveltype;
	out.flags = in.flags; out.weapon = in.weapon;
	VectorCopy( in.movedir, out.movedir );
	VectorCopy( in.ideal_viewangles, out.ideal_viewangles );
	return out;
}

/* The engine keeps 256-byte messages; ET's record holds 150 and is always
 * terminated. The list pointers stay zero: they are engine-owned storage. */
inline fntech3_wolfet_bot_consolemessage_t ConsoleMessageOutput( const bot_consolemessage_t &in ) {
	static_assert( sizeof( fntech3_wolfet_bot_consolemessage_t ) == 172
		&& alignof( fntech3_wolfet_bot_consolemessage_t ) == 4,
		"ET console message is a frozen 172-byte record" );
	fntech3_wolfet_bot_consolemessage_t out{};
	out.handle = in.handle; out.time = in.time; out.type = in.type;
	std::size_t length = 0;
	while ( length + 1 < sizeof( out.message ) && length < sizeof( in.message )
		&& in.message[length] ) {
		out.message[length] = in.message[length];
		++length;
	}
	out.message[length] = '\0';
	return out;
}

} // namespace bot
} // namespace wolfet
} // namespace fntech3
#endif
