/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_FAKK2_CONVERT_HPP
#define FNTECH3_COMPAT_FAKK2_CONVERT_HPP

/* Between Heavy Metal: F.A.K.K.2's records and Alice's.
 *
 * Alice's engine is F.A.K.K.2's with a few members added or regrouped, and
 * fnTech3 already carries Alice's records through every Ritual path: the
 * snapshot walk, the wire codec, the render front end. So F.A.K.K.2's records
 * cross into Alice's at the module boundary and back out of them on the way
 * to a F.A.K.K.2 module, and nothing past the boundary needs a second shape.
 *
 * A crossing loses nothing F.A.K.K.2 has:
 *
 *  - an entity state's three controller arrays become Alice's five records --
 *    tag and angles in the networked members, the module's own quaternion in
 *    the sixteen bytes Alice never sends -- and come back the same way;
 *  - a render entity's two controller arrays become Alice's five records with
 *    a scale of one, which the pose multiplies in as nothing;
 *  - the words Alice added (a render entity's +0x08 and trailing angles, a
 *    view's depth flag, an entity's second render-flags word) are zero on the
 *    way in and dropped on the way out.
 *
 * Nothing here interprets a member. See code/compat/fakk2_game_abi.h for the
 * records and docs/fntech3/2026-09-24-fakk2-contracts.md for the evidence. */

#include "fakk2_game_abi.h"

#include <cstring>

namespace fntech3::fakk2::convert {

static_assert( FNTECH3_FAKK2_BONE_CONTROLLERS == FNTECH3_ALICE_BONE_CONTROLLERS,
	"both titles carry five controllers" );
static_assert( sizeof( fntech3_alice_bonemod_t::unnamed ) == sizeof( float[4] ),
	"Alice keeps sixteen unsent bytes between a controller's angles and scale" );

/* The members both entity states keep at the same offsets: everything from the
 * number to the torso's crossblend time, before the controllers begin. */
constexpr std::size_t kSharedEntityPrefix = offsetof( fntech3_fakk2_entityState_t, bone_tag );
static_assert( kSharedEntityPrefix == offsetof( fntech3_alice_entityState_t, bonemod ),
	"the controllers begin at 216 in both" );

inline void EntityToAlice( const fntech3_fakk2_entityState_t &from, fntech3_alice_entityState_t &to ) {
	std::memset( &to, 0, sizeof( to ) );
	std::memcpy( &to, &from, kSharedEntityPrefix );
	for ( int i = 0; i < FNTECH3_FAKK2_BONE_CONTROLLERS; ++i ) {
		to.bonemod[i].tag = from.bone_tag[i];
		std::memcpy( to.bonemod[i].angles, from.bone_angles[i], sizeof( to.bonemod[i].angles ) );
		std::memcpy( to.bonemod[i].unnamed, from.bone_quat[i], sizeof( to.bonemod[i].unnamed ) );
	}
	std::memcpy( to.tagged, from.surfaces, sizeof( to.tagged ) );
	to.clientNum = from.clientNum;
	to.groundEntityNum = from.groundEntityNum;
	to.solid = from.solid;
	to.scale = from.scale;
	to.alpha = from.alpha;
	to.renderfx = from.renderfx;
	to.ownerNum = from.bindparent;
	std::memcpy( to.unnamed, from.quat, sizeof( to.unnamed ) );
	std::memcpy( to.axis, from.mat, sizeof( to.axis ) );
}

/* Back onto a F.A.K.K.2 record. The caller passes the module's own bytes when
 * it has them, so the one member Alice's record cannot hold a value for --
 * none: every F.A.K.K.2 member has an Alice home -- is simply overwritten. */
inline void EntityFromAlice( const fntech3_alice_entityState_t &from, fntech3_fakk2_entityState_t &to ) {
	std::memcpy( &to, &from, kSharedEntityPrefix );
	for ( int i = 0; i < FNTECH3_FAKK2_BONE_CONTROLLERS; ++i ) {
		to.bone_tag[i] = from.bonemod[i].tag;
		std::memcpy( to.bone_angles[i], from.bonemod[i].angles, sizeof( to.bone_angles[i] ) );
		std::memcpy( to.bone_quat[i], from.bonemod[i].unnamed, sizeof( to.bone_quat[i] ) );
	}
	std::memcpy( to.surfaces, from.tagged, sizeof( to.surfaces ) );
	to.clientNum = from.clientNum;
	to.groundEntityNum = from.groundEntityNum;
	to.solid = from.solid;
	to.scale = from.scale;
	to.alpha = from.alpha;
	to.renderfx = from.renderfx;
	to.bindparent = from.ownerNum;
	std::memcpy( to.quat, from.unnamed, sizeof( to.quat ) );
	std::memcpy( to.mat, from.axis, sizeof( to.mat ) );
}

/* F.A.K.K.2's two controller arrays as Alice's five records. A null array
 * leaves every record untagged, which the pose skips. */
inline void ControllersToAlice( const int32_t *boneTag, const float ( *boneQuat )[4],
	fntech3_alice_bonectrl_t out[FNTECH3_ALICE_BONE_CONTROLLERS] ) {
	std::memset( out, 0, sizeof( fntech3_alice_bonectrl_t ) * FNTECH3_ALICE_BONE_CONTROLLERS );
	for ( int i = 0; i < FNTECH3_ALICE_BONE_CONTROLLERS; ++i ) {
		out[i].tag = boneTag ? boneTag[i] : -1;
		if ( boneQuat ) {
			std::memcpy( out[i].quat, boneQuat[i], sizeof( out[i].quat ) );
		}
		out[i].scale = 1.0f;
	}
}

/* The members before Alice's inserted word, and the run after it up to the
 * controllers, which moved four bytes. */
constexpr std::size_t kRefHead = offsetof( fntech3_fakk2_refEntity_t, hModel );
constexpr std::size_t kRefRun = offsetof( fntech3_fakk2_refEntity_t, bone_tag ) - kRefHead;
static_assert( offsetof( fntech3_alice_refEntity_t, hModel ) == kRefHead + 4, "Alice's +0x08" );
static_assert( offsetof( fntech3_alice_refEntity_t, bonectrl ) == kRefHead + 4 + kRefRun,
	"and the run after it ends at the controllers" );

inline void RefEntityToAlice( const fntech3_fakk2_refEntity_t &from, fntech3_alice_refEntity_t &to ) {
	std::memset( &to, 0, sizeof( to ) );
	std::memcpy( &to, &from, kRefHead );
	std::memcpy( reinterpret_cast<unsigned char *>( &to ) + kRefHead + 4,
		reinterpret_cast<const unsigned char *>( &from ) + kRefHead, kRefRun );
	ControllersToAlice( from.bone_tag, from.bone_quat, to.bonectrl );
	std::memcpy( to.rendererOnly, from.rendererOnly, sizeof( to.rendererOnly ) );
	to.radius = from.radius;
	to.rotation = from.rotation;
}

inline void RefEntityFromAlice( const fntech3_alice_refEntity_t &from, fntech3_fakk2_refEntity_t &to ) {
	std::memset( &to, 0, sizeof( to ) );
	std::memcpy( &to, &from, kRefHead );
	std::memcpy( reinterpret_cast<unsigned char *>( &to ) + kRefHead,
		reinterpret_cast<const unsigned char *>( &from ) + kRefHead + 4, kRefRun );
	for ( int i = 0; i < FNTECH3_FAKK2_BONE_CONTROLLERS; ++i ) {
		to.bone_tag[i] = from.bonectrl[i].tag;
		std::memcpy( to.bone_quat[i], from.bonectrl[i].quat, sizeof( to.bone_quat[i] ) );
	}
	std::memcpy( to.rendererOnly, from.rendererOnly, sizeof( to.rendererOnly ) );
	to.radius = from.radius;
	to.rotation = from.rotation;
}

inline void RefdefToAlice( const fntech3_fakk2_refdef_t &from, fntech3_alice_refdef_t &to ) {
	std::memset( &to, 0, sizeof( to ) );
	std::memcpy( &to, &from, sizeof( from ) );
}

/* A model definition's command list, into F.A.K.K.2's smaller record: at
 * most 128 commands of at most ten arguments. The argument pointers are the
 * definition's own strings and are copied as they are. Returns false when a
 * command or an argument had to be left out, which F.A.K.K.2's own data never
 * needs -- no retail definition has a frame command longer than ten words. */
inline bool CommandsFromAlice( const fntech3_alice_tiki_cmd_t &from, fntech3_fakk2_tiki_cmd_t &to ) {
	bool whole = true;
	int count = from.num_cmds;
	if ( count < 0 ) {
		count = 0;
	}
	if ( count > FNTECH3_FAKK2_MAX_TIKI_CMDS ) {
		count = FNTECH3_FAKK2_MAX_TIKI_CMDS;
		whole = false;
	}
	to.num_cmds = count;
	for ( int i = 0; i < count; ++i ) {
		int args = from.cmds[i].num_args;
		if ( args < 0 ) {
			args = 0;
		}
		if ( args > FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS ) {
			args = FNTECH3_FAKK2_MAX_TIKI_CMD_ARGS;
			whole = false;
		}
		to.cmds[i].num_args = args;
		for ( int a = 0; a < args; ++a ) {
			to.cmds[i].args[a] = from.cmds[i].args[a];
		}
	}
	return whole;
}

/* Mark fragments back into Quake III's eight-byte record: the plane Alice's
 * engine adds is dropped. */
inline void FragmentsFromAlice( const fntech3_alice_markFragment_t *from, int count,
	fntech3_fakk2_markFragment_t *to ) {
	for ( int i = 0; i < count; ++i ) {
		to[i].firstPoint = from[i].firstPoint;
		to[i].numPoints = from[i].numPoints;
	}
}

} // namespace fntech3::fakk2::convert

#endif
