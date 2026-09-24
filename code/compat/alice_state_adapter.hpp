/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_STATE_ADAPTER_HPP
#define FNTECH3_COMPAT_ALICE_STATE_ADAPTER_HPP

/* Between American McGee's Alice's records and fnTech3's own.
 *
 * Alice's entity state networks seventy-nine members and its player state
 * forty-nine. Thirty-six and twenty-two of them are this engine's own members
 * by name and meaning and cross into them; the rest cross into the extensions
 * in qcommon/alice_state.h, so that nothing the module sent is lost on the
 * way to a snapshot and nothing a snapshot carried is lost on the way back.
 *
 * Which member goes where is not decided here. compat/alice_state_carry.def
 * is generated from the delta tables together with the mapping, and its
 * generator refuses to emit it unless the mapped and carried sets cover a
 * table exactly -- so a member cannot be dropped by forgetting to write a
 * line, and the file below is a walk over that list rather than a list of
 * its own.
 *
 * Two correspondences that would be easy to make are deliberately not made.
 * Alice's `anim` is a whole-body animation with `torso_anim` overriding the
 * upper half, where this engine's pair is legs and torso; calling `anim` the
 * legs animation would be an interpretation rather than a measurement. Its
 * `skinNum` is not this engine's `modelindex2`. Both are carried under their
 * own names instead, which is exact.
 *
 * The member names are the retail engine's own, from the delta field tables
 * its binary carries. See docs/fntech3/2026-09-22-alice-wire.md. */

#include "alice_game_abi.h"

#include "../qcommon/q_shared.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace fntech3::alice::state {

namespace detail {

/* Both records keep their vectors as three floats, so one copy serves either
 * direction. */
inline void CopyVector( float destination[3], const float source[3] ) {
	destination[0] = source[0];
	destination[1] = source[1];
	destination[2] = source[2];
}

/* A mapped member crosses as a word, whatever its type: both sides hold the
 * same four bytes at their own offsets and this engine never reinterprets
 * one. A trajectory's `trType` is an enumeration on this side and a plain
 * word on Alice's, which is four bytes either way. */
template<class Destination, class Source>
inline void CopyWord( Destination &destination, const Source &source ) {
	static_assert( sizeof( Destination ) == 4 && sizeof( Source ) == 4 );
	std::memcpy( &destination, &source, 4 );
}

} // namespace detail

/* Alice's entity into this engine's. Members this engine has and Alice does
 * not are cleared, so nothing carries over from what the destination held. */
inline bool ToInternal( const fntech3_alice_entityState_t &source,
	entityState_t &destination ) {
	/* Clearing first is what makes a member this engine has and Alice does
	 * not read as zero rather than as whatever the destination held. The two
	 * trajectories come across member by member below, so the gravity term
	 * RTCW added to them -- which Alice's has no field for -- stays zero. */
	destination = {};
	destination.number = source.number;

#define FNTECH3_ALICE_ENTITY_MAP( external, internal ) \
	detail::CopyWord( destination.internal, source.external );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_ENTITY_MAP

#define FNTECH3_ALICE_ENTITY_CARRY( member ) \
	detail::CopyWord( destination.alice.member, source.member );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_ENTITY_CARRY

#define FNTECH3_ALICE_ENTITY_CARRY_ARRAY( member, count ) \
	for ( int i = 0; i < count; ++i ) \
		detail::CopyWord( destination.alice.member[i], source.member[i] );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_ENTITY_CARRY_ARRAY

	for ( int i = 0; i < 5; ++i ) {
		destination.alice.bonemod[i].tag = source.bonemod[i].tag;
		detail::CopyVector( destination.alice.bonemod[i].angles,
			source.bonemod[i].angles );
		destination.alice.bonemod[i].scale = source.bonemod[i].scale;
	}
	std::memcpy( destination.alice.tagged, source.tagged,
		sizeof( destination.alice.tagged ) );
	return true;
}

/* And back, onto the module's own bytes: the caller reads the record first
 * and passes it here, so every member this function does not touch -- the
 * entity state's own tail among them -- survives untouched. */
inline bool FromInternal( const entityState_t &source,
	fntech3_alice_entityState_t &destination ) {
	destination.number = source.number;

#define FNTECH3_ALICE_ENTITY_MAP( external, internal ) \
	detail::CopyWord( destination.external, source.internal );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_ENTITY_MAP

#define FNTECH3_ALICE_ENTITY_CARRY( member ) \
	detail::CopyWord( destination.member, source.alice.member );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_ENTITY_CARRY

#define FNTECH3_ALICE_ENTITY_CARRY_ARRAY( member, count ) \
	for ( int i = 0; i < count; ++i ) \
		detail::CopyWord( destination.member[i], source.alice.member[i] );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_ENTITY_CARRY_ARRAY

	for ( int i = 0; i < 5; ++i ) {
		destination.bonemod[i].tag = source.alice.bonemod[i].tag;
		detail::CopyVector( destination.bonemod[i].angles,
			source.alice.bonemod[i].angles );
		destination.bonemod[i].scale = source.alice.bonemod[i].scale;
	}
	std::memcpy( destination.tagged, source.alice.tagged,
		sizeof( destination.tagged ) );
	return true;
}

inline bool ToInternal( const fntech3_alice_playerState_t &source,
	playerState_t &destination ) {
	destination = {};

#define FNTECH3_ALICE_PLAYER_MAP( external, internal ) \
	detail::CopyWord( destination.internal, source.external );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_PLAYER_MAP

#define FNTECH3_ALICE_PLAYER_CARRY( member ) \
	detail::CopyWord( destination.alice.member, source.member );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_PLAYER_CARRY

#define FNTECH3_ALICE_PLAYER_CARRY_ARRAY( member, count ) \
	for ( int i = 0; i < count; ++i ) \
		detail::CopyWord( destination.alice.member[i], source.member[i] );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_PLAYER_CARRY_ARRAY

	std::memcpy( destination.alice.block, source.block,
		sizeof( destination.alice.block ) );
	destination.ping = source.ping;
	return true;
}

inline bool FromInternal( const playerState_t &source,
	fntech3_alice_playerState_t &destination ) {

#define FNTECH3_ALICE_PLAYER_MAP( external, internal ) \
	detail::CopyWord( destination.external, source.internal );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_PLAYER_MAP

#define FNTECH3_ALICE_PLAYER_CARRY( member ) \
	detail::CopyWord( destination.member, source.alice.member );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_PLAYER_CARRY

#define FNTECH3_ALICE_PLAYER_CARRY_ARRAY( member, count ) \
	for ( int i = 0; i < count; ++i ) \
		detail::CopyWord( destination.member[i], source.alice.member[i] );
#include "alice_state_carry.def"
#undef FNTECH3_ALICE_PLAYER_CARRY_ARRAY

	std::memcpy( destination.block, source.alice.block,
		sizeof( destination.block ) );
	destination.ping = source.ping;
	return true;
}

/* The user command. Its time, view angles, buttons and three movement bytes
 * are this engine's own members. The movement bytes are forward, right and up
 * in that order: retail's builder stores them at +14, +15 and +16
 * (0x00428287, 0x00428290, 0x0042829b), and fgamex86.dll's pmove reads +14
 * and +15 into its horizontal wish velocity and +16 as vertical
 * (0x10065319..0x10065355). The byte at +6 is carried as it is: the retail
 * client never fills it and the wire sends it, and what the game makes of it
 * is not established. The bytes at +4, +7 and +17..+19 are never sent and
 * stay zero. */
inline bool ToInternal( const fntech3_alice_usercmd_t &source,
	usercmd_t &destination ) {
	destination = {};
	destination.serverTime = source.serverTime;
	for ( int i = 0; i < 3; ++i ) destination.angles[i] = source.angles[i];
	destination.buttons = source.buttons;
	destination.forwardmove = static_cast<signed char>( source.move[0] );
	destination.rightmove = static_cast<signed char>( source.move[1] );
	destination.upmove = static_cast<signed char>( source.move[2] );
	destination.alice.unnamed = source.unnamed_6;
	return true;
}

inline bool FromInternal( const usercmd_t &source,
	fntech3_alice_usercmd_t &destination ) {
	destination = {};
	destination.serverTime = source.serverTime;
	for ( int i = 0; i < 3; ++i ) {
		destination.angles[i] = static_cast<std::int16_t>( source.angles[i] );
	}
	destination.buttons = static_cast<std::uint8_t>( source.buttons );
	destination.move[0] = static_cast<std::int8_t>( source.forwardmove );
	destination.move[1] = static_cast<std::int8_t>( source.rightmove );
	destination.move[2] = static_cast<std::int8_t>( source.upmove );
	destination.unnamed_6 = source.alice.unnamed;
	return true;
}

/* ---- what retail's entity reader adds for the client game ---------------
 *
 * Retail's MSG_ReadDeltaEntity (0x004460c0) does not stop at the networked
 * members. Its delta branch ends by deriving four more that the client game
 * reads (0x004463e5..0x0044644a): the origin (+96) from the net origin, the
 * angles (+132) from the net angles, the entity's rotation as a quaternion
 * (+460) from those angles, and, for each bone modifier whose tag is not
 * negative, that modifier's quaternion (+16 in it) from its own angles. The
 * retail client game interpolates the origin and slerps the quaternions
 * (cgamex86 0x30022655, 0x300226cd, 0x30022739), so a record without them puts
 * every lerped entity at the world's origin, unrotated.
 *
 * The arithmetic is retail's x87 code at double precision: each sine and
 * cosine, and each matrix and quaternion element, is kept as a float where
 * retail stores one. */

namespace reader {

inline constexpr double kDegreesToRadians = 0.017453292519943295; /* 0x004ffb90 */

/* AnglesToAxis (0x0044aa40): forward, left and up from pitch, yaw and roll
 * in degrees, row by row. */
inline void AnglesToAxis( const float angles[3], float axis[9] ) {
	const double yaw = static_cast<double>( angles[1] ) * kDegreesToRadians;
	const double pitch = static_cast<double>( angles[0] ) * kDegreesToRadians;
	const double roll = static_cast<double>( angles[2] ) * kDegreesToRadians;
	const double sy = static_cast<float>( std::sin( yaw ) );
	const double cy = static_cast<float>( std::cos( yaw ) );
	const double sp = static_cast<float>( std::sin( pitch ) );
	const double cp = static_cast<float>( std::cos( pitch ) );
	const double sr = static_cast<float>( std::sin( roll ) );
	const double cr = static_cast<float>( std::cos( roll ) );
	axis[0] = static_cast<float>( cp * cy );
	axis[1] = static_cast<float>( cp * sy );
	axis[2] = static_cast<float>( -sp );
	axis[3] = static_cast<float>( sr * sp * cy - cr * sy );
	axis[4] = static_cast<float>( sr * sp * sy + cr * cy );
	axis[5] = static_cast<float>( sr * cp );
	axis[6] = static_cast<float>( cr * sp * cy + sr * sy );
	axis[7] = static_cast<float>( cr * sp * sy - sr * cy );
	axis[8] = static_cast<float>( cr * cp );
}

/* MatToQuat (0x0044a3a0): x, y, z, then w. A positive trace takes the
 * direct form; otherwise the largest diagonal element leads, the next two
 * chosen from the table {1, 2, 0} at 0x005190f4. */
inline void AxisToQuat( const float m[9], float quat[4] ) {
	const double trace = static_cast<double>( m[4] ) + m[0] + m[8];
	if ( trace > 0.0 ) {
		double s = std::sqrt( trace + 1.0 );
		quat[3] = static_cast<float>( s * 0.5 );
		s = 0.5 / s;
		quat[0] = static_cast<float>( ( static_cast<double>( m[7] ) - m[5] ) * s );
		quat[1] = static_cast<float>( ( static_cast<double>( m[2] ) - m[6] ) * s );
		quat[2] = static_cast<float>( ( static_cast<double>( m[3] ) - m[1] ) * s );
		return;
	}
	static constexpr int next[3] = { 1, 2, 0 };
	int i = 0;
	if ( m[4] > m[0] ) i = 1;
	if ( m[8] > m[i * 4] ) i = 2;
	const int j = next[i];
	const int k = next[j];
	double s = std::sqrt( ( m[i * 4] - ( static_cast<double>( m[k * 4] ) + m[j * 4] ) ) + 1.0 );
	quat[i] = static_cast<float>( s * 0.5 );
	s = 0.5 / s;
	quat[3] = static_cast<float>( ( static_cast<double>( m[k * 3 + j] ) - m[j * 3 + k] ) * s );
	quat[j] = static_cast<float>( ( static_cast<double>( m[i * 3 + j] ) + m[j * 3 + i] ) * s );
	quat[k] = static_cast<float>( ( static_cast<double>( m[i * 3 + k] ) + m[k * 3 + i] ) * s );
}

/* EulerToQuat (0x0044a810): three words of zero -- tested as words, so a
 * negative zero is not one -- give the identity; anything else goes through
 * the axis. */
inline void EulerToQuat( const float angles[3], float quat[4] ) {
	std::uint32_t words[3];
	std::memcpy( words, angles, sizeof( words ) );
	if ( !words[0] && !words[1] && !words[2] ) {
		quat[0] = quat[1] = quat[2] = 0.0f;
		quat[3] = 1.0f;
		return;
	}
	float axis[9];
	AnglesToAxis( angles, axis );
	AxisToQuat( axis, quat );
}

} // namespace reader

/* The tail of retail's reader, on a record that already holds the networked
 * members. */
inline void DeriveReaderMembers( fntech3_alice_entityState_t &entity ) {
	static_assert( sizeof( entity.origin ) == sizeof( entity.netorigin ), "the origin is three words" );
	static_assert( sizeof( entity.angles_unnamed ) == sizeof( entity.netangles ), "the angles are three words" );
	static_assert( sizeof( entity.unnamed ) == 4 * sizeof( float ), "a quaternion is four floats" );
	static_assert( sizeof( entity.bonemod[0].unnamed ) == 4 * sizeof( float ), "so is a bone modifier's" );
	std::memcpy( entity.origin, entity.netorigin, sizeof( entity.origin ) );
	std::memcpy( entity.angles_unnamed, entity.netangles, sizeof( entity.angles_unnamed ) );
	float quat[4];
	reader::EulerToQuat( entity.netangles, quat );
	std::memcpy( entity.unnamed, quat, sizeof( quat ) );
	for ( fntech3_alice_bonemod_t &bonemod : entity.bonemod ) {
		if ( bonemod.tag >= 0 ) {
			reader::EulerToQuat( bonemod.angles, quat );
			std::memcpy( bonemod.unnamed, quat, sizeof( quat ) );
		}
	}
}

/* An entity as the client game receives it: the networked members, then what
 * the reader derives from them. Retail's delta branch writes only those into
 * a ring slot, so anything else there is whatever the slot last held; here it
 * is zero. */
inline void ForClientGame( const entityState_t &source,
	fntech3_alice_entityState_t &destination ) {
	destination = {};
	FromInternal( source, destination );
	DeriveReaderMembers( destination );
}

} // namespace fntech3::alice::state

#endif
