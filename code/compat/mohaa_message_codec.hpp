/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_MOHAA_MESSAGE_CODEC_HPP
#define FNTECH3_COMPAT_MOHAA_MESSAGE_CODEC_HPP

/* Medal of Honor: Allied Assault 1.00's protocol-6 message records.
 *
 * The retail engine is SafeDisc-wrapped and could not be read, so the field
 * orders, widths and packings here are OpenMoHAA's (code/qcommon/msg.cpp,
 * GPL-2.0-or-later), whose protocol-6 tables it keeps for 1.00 servers and
 * clients; the implementation is code/qcommon/mohaa_msg.cpp. They act on the
 * module records of code/compat/mohaa_game_abi.h, which are what 1.00's
 * modules and its wire both carry, so nothing is converted on the way.
 *
 * Every function writes to or reads from a Huffman-coded message: 1.00's
 * gamestates, snapshots and client moves are not out-of-band. */

#include "mohaa_game_abi.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#ifdef __cplusplus

namespace fntech3::mohaa::wire {

enum {
	kEntityNumBits = 10,
	/* The entity number that ends a snapshot's entity list and marks a
	 * removed entity: MAX_GENTITIES - 1. */
	kEntityNumEnd = FNTECH3_MOHAA_MAX_GENTITIES - 1,
	/* A snapshot carries at most 64 sounds, counted in seven bits. */
	kMaxSnapshotSounds = FNTECH3_MOHAA_MAX_SERVER_SOUNDS,
	kMaxPacketUsercmds = 32
};

/* The state an entity's first delta starts from (OpenMoHAA
 * MSG_GetNullEntityState): opaque, full size, no parent, a white constant
 * light, render flag 16 and no bone controllers. */
void NullEntityState( EntityState &state );

void WriteEntityNum( msg_t *msg, int number );
int ReadEntityNum( msg_t *msg );

/* A delta from `from` to `to`, or, when `to` is null, the entity's removal.
 * Nothing is written for an unchanged entity unless `force` is set. */
void WriteDeltaEntity( msg_t *msg, const EntityState *from, const EntityState *to, bool force );

/* The delta for entity `number`, whose number has been read already. A
 * removed entity comes back numbered kEntityNumEnd. The client-side members
 * the wire does not carry are rebuilt as 1.00's client does: the origin and
 * angles from their networked copies, and the quaternions from the angles. */
void ReadDeltaEntity( msg_t *msg, const EntityState *from, EntityState *to, int number );

void WriteDeltaPlayerstate( msg_t *msg, const PlayerState *from, const PlayerState *to );
void ReadDeltaPlayerstate( msg_t *msg, const PlayerState *from, PlayerState *to );

void WriteSounds( msg_t *msg, const ServerSound *sounds, int count );
/* False if the count is out of range; `count` is then zero. */
bool ReadSounds( msg_t *msg, ServerSound *sounds, int &count );

/* A client's move, keyed as Quake III 1.32 keys it: the checksum feed, the
 * acknowledged message and the hash of the last acknowledged server command,
 * and each command's time. */
void WriteDeltaUsercmdKey( msg_t *msg, int key, const UserCmd &from, const UserCmd &to );
void ReadDeltaUsercmdKey( msg_t *msg, int key, const UserCmd &from, UserCmd &to );

void WriteDeltaEyeInfo( msg_t *msg, const UserEyes &from, const UserEyes &to );
void ReadDeltaEyeInfo( msg_t *msg, const UserEyes &from, UserEyes &to );

/* The engine's command as 1.00 sends it: its sixteen button bits and short
 * angles, no weapon byte, and a zero msec, which 1.00's client never sets
 * and the protocol never carries. */
inline void CommandFromEngine( const usercmd_t &in, UserCmd &out ) {
	out = UserCmd{};
	out.serverTime = in.serverTime;
	out.buttons = static_cast<std::uint16_t>( in.buttons );
	for ( int i = 0; i < 3; i++ ) {
		out.angles[i] = static_cast<std::int16_t>( in.angles[i] );
	}
	out.forwardmove = in.forwardmove;
	out.rightmove = in.rightmove;
	out.upmove = in.upmove;
}

/* The number of fields the tables carry, for tests and diagnostics. */
int EntityFieldCount();
int PlayerFieldCount();

} // namespace fntech3::mohaa::wire

#endif

#endif
