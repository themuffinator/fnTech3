/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "server_game_data.h"

#include "../compat/alice_module_abi.h"
#include "../compat/alice_state_adapter.hpp"
#include "../compat/fakk2_convert.hpp"
#include "../compat/game_abi_adapter.h"
#include "../compat/q3_game_abi.h"
#include "../compat/ql_game_abi.h"
#include "../compat/jk2_game_abi.h"
#include "../compat/jka_game_abi.h"
#include "../compat/mohaa_game_abi.h"
#include "../compat/jka_state_adapter.hpp"
#include "../compat/jka_sp_state_adapter.hpp"
#include "../compat/jk2_state_adapter.hpp"
#include "../compat/q3_ihv_state_adapter.hpp"
#include "../compat/q3test_state_adapter.hpp"
#include "../compat/rtcw_state_adapter.hpp"
#include "../compat/stvef_state_adapter.hpp"
#include "../compat/wolfet_state_adapter.hpp"

#include <cstring>
#include <limits>

namespace fntech3::server {

namespace {
// A field the module's record does not have.
constexpr std::size_t kNoField = ~std::size_t{ 0 };
}

struct ServerGameDataView::Layout {
	comGameId_t profile;
	std::size_t entityPrefixBytes;
	std::size_t entityStateBytes;
	std::size_t playerStateBytes;
	std::size_t linkedOffset;
	std::size_t linkcountOffset;
	std::size_t svFlagsOffset;
	std::size_t singleClientOffset;
	std::size_t bmodelOffset;
	std::size_t minsOffset;
	std::size_t maxsOffset;
	std::size_t contentsOffset;
	std::size_t absminOffset;
	std::size_t absmaxOffset;
	std::size_t currentOriginOffset;
	std::size_t currentAnglesOffset;
	std::size_t ownerNumOffset;
	std::size_t sharedStateOrigin2Offset;
	std::size_t playerPingOffset;
	std::int32_t maxClients;
	// Elite Force SP names its owner by gentity pointer and reaches each
	// player state through its gentity's client pointer.
	bool ownerIsPointer = false;
	std::size_t clientPointerOffset = kNoField;
	std::int32_t firstClientEntity = 0;
	std::size_t entityNumberBytes = sizeof(std::int32_t);
};

namespace {

constexpr std::size_t kAbiAlignment = alignof( std::int32_t );

template<typename Destination, typename Source, std::size_t Count>
void CopyArray( Destination ( &destination )[Count],
	const Source ( &source )[Count] )
{
	for ( std::size_t index = 0; index < Count; ++index ) {
		destination[index] = static_cast<Destination>( source[index] );
	}
}

void InternalTrajectoryFromQl( trajectory_t &destination,
	const fntech3_ql_trajectory_t &source )
{
	destination.trType = static_cast<trType_t>( source.trType );
	destination.trTime = source.trTime;
	destination.trDuration = source.trDuration;
	CopyArray( destination.trBase, source.trBase );
	CopyArray( destination.trDelta, source.trDelta );
	destination.gravity = source.gravity;
}

void QlTrajectoryFromInternal( fntech3_ql_trajectory_t &destination,
	const trajectory_t &source )
{
	destination.trType = static_cast<std::int32_t>( source.trType );
	destination.trTime = source.trTime;
	destination.trDuration = source.trDuration;
	CopyArray( destination.trBase, source.trBase );
	CopyArray( destination.trDelta, source.trDelta );
	destination.gravity = source.gravity;
}

void InternalEntityStateFromQl( entityState_t &destination,
	const fntech3_ql_entityState_t &source )
{
	std::memset( &destination, 0, sizeof( destination ) );
	destination.number = source.number;
	destination.eType = source.eType;
	destination.eFlags = source.eFlags;
	InternalTrajectoryFromQl( destination.pos, source.pos );
	InternalTrajectoryFromQl( destination.apos, source.apos );
	destination.time = source.time;
	destination.time2 = source.time2;
	CopyArray( destination.origin, source.origin );
	CopyArray( destination.origin2, source.origin2 );
	CopyArray( destination.angles, source.angles );
	CopyArray( destination.angles2, source.angles2 );
	destination.otherEntityNum = source.otherEntityNum;
	destination.otherEntityNum2 = source.otherEntityNum2;
	destination.groundEntityNum = source.groundEntityNum;
	destination.constantLight = source.constantLight;
	destination.loopSound = source.loopSound;
	destination.modelindex = source.modelindex;
	destination.modelindex2 = source.modelindex2;
	destination.clientNum = source.clientNum;
	destination.frame = source.frame;
	destination.solid = source.solid;
	destination.event = source.event;
	destination.eventParm = source.eventParm;
	destination.powerups = source.powerups;
	destination.health = source.health;
	destination.armor = source.armor;
	destination.weapon = source.weapon;
	destination.location = source.location;
	destination.legsAnim = source.legsAnim;
	destination.torsoAnim = source.torsoAnim;
	destination.generic1 = source.generic1;
	destination.jumpTime = source.jumpTime;
	destination.doubleJumped = source.doubleJumped;
}

void QlEntityStateFromInternal( fntech3_ql_entityState_t &destination,
	const entityState_t &source )
{
	std::memset( &destination, 0, sizeof( destination ) );
	destination.number = source.number;
	destination.eType = source.eType;
	destination.eFlags = source.eFlags;
	QlTrajectoryFromInternal( destination.pos, source.pos );
	QlTrajectoryFromInternal( destination.apos, source.apos );
	destination.time = source.time;
	destination.time2 = source.time2;
	CopyArray( destination.origin, source.origin );
	CopyArray( destination.origin2, source.origin2 );
	CopyArray( destination.angles, source.angles );
	CopyArray( destination.angles2, source.angles2 );
	destination.otherEntityNum = source.otherEntityNum;
	destination.otherEntityNum2 = source.otherEntityNum2;
	destination.groundEntityNum = source.groundEntityNum;
	destination.constantLight = source.constantLight;
	destination.loopSound = source.loopSound;
	destination.modelindex = source.modelindex;
	destination.modelindex2 = source.modelindex2;
	destination.clientNum = source.clientNum;
	destination.frame = source.frame;
	destination.solid = source.solid;
	destination.event = source.event;
	destination.eventParm = source.eventParm;
	destination.powerups = source.powerups;
	destination.health = source.health;
	destination.armor = source.armor;
	destination.weapon = source.weapon;
	destination.location = source.location;
	destination.legsAnim = source.legsAnim;
	destination.torsoAnim = source.torsoAnim;
	destination.generic1 = source.generic1;
	destination.jumpTime = source.jumpTime;
	destination.doubleJumped = source.doubleJumped;
}

void InternalPlayerStateFromQl( playerState_t &destination,
	const fntech3_ql_playerState_t &source )
{
	std::memset( &destination, 0, sizeof( destination ) );
	destination.commandTime = source.commandTime;
	destination.pm_type = source.pm_type;
	destination.bobCycle = source.bobCycle;
	destination.pm_flags = source.pm_flags;
	destination.pm_time = source.pm_time;
	CopyArray( destination.origin, source.origin );
	CopyArray( destination.velocity, source.velocity );
	destination.weaponTime = source.weaponTime;
	destination.gravity = source.gravity;
	destination.speed = source.speed;
	CopyArray( destination.delta_angles, source.delta_angles );
	destination.groundEntityNum = source.groundEntityNum;
	destination.legsTimer = source.legsTimer;
	destination.legsAnim = source.legsAnim;
	destination.torsoTimer = source.torsoTimer;
	destination.torsoAnim = source.torsoAnim;
	destination.movementDir = source.movementDir;
	CopyArray( destination.grapplePoint, source.grapplePoint );
	destination.eFlags = source.eFlags;
	destination.eventSequence = source.eventSequence;
	CopyArray( destination.events, source.events );
	CopyArray( destination.eventParms, source.eventParms );
	destination.externalEvent = source.externalEvent;
	destination.externalEventParm = source.externalEventParm;
	destination.clientNum = source.clientNum;
	destination.location = source.location;
	destination.weapon = source.weapon;
	destination.weaponPrimary = source.weaponPrimary;
	destination.weaponstate = source.weaponstate;
	destination.fov = source.fov;
	CopyArray( destination.viewangles, source.viewangles );
	destination.viewheight = source.viewheight;
	destination.damageEvent = source.damageEvent;
	destination.damageYaw = source.damageYaw;
	destination.damagePitch = source.damagePitch;
	destination.damageCount = source.damageCount;
	CopyArray( destination.stats, source.stats );
	CopyArray( destination.persistant, source.persistant );
	CopyArray( destination.powerups, source.powerups );
	CopyArray( destination.ammo, source.ammo );
	destination.generic1 = source.generic1;
	destination.loopSound = source.loopSound;
	destination.jumppad_ent = source.jumppad_ent;
	destination.jumpTime = source.jumpTime;
	destination.doubleJumped = source.doubleJumped;
	destination.crouchTime = source.crouchTime;
	destination.crouchSlideTime = source.crouchSlideTime;
	destination.forwardmove = source.forwardmove;
	destination.rightmove = source.rightmove;
	destination.upmove = source.upmove;
	destination.commandMirrorPad = source.commandMirrorPad;
	destination.ping = source.ping;
	destination.pmove_framecount = source.pmove_framecount;
	destination.jumppad_frame = source.jumppad_frame;
	destination.entityEventSequence = source.entityEventSequence;
	destination.externalEventTime = source.externalEventTime;
	destination.armorTier = source.armorTier;
}

bool CheckedRequiredSpan( std::int32_t count, std::size_t stride,
	std::size_t &destination )
{
	const std::size_t unsignedCount = static_cast<std::size_t>( count );
	if ( stride > ( std::numeric_limits<std::size_t>::max )() / unsignedCount ) {
		return false;
	}
	destination = stride * unsignedCount;
	return true;
}

bool AddressRangeFits( const void *base, std::size_t span )
{
	const std::uintptr_t address = reinterpret_cast<std::uintptr_t>( base );
	return address <= ( std::numeric_limits<std::uintptr_t>::max )() - span;
}

} // namespace

bool ResolveQvmGameDataPointer( void *dataBase, std::size_t dataSpanBytes,
	std::intptr_t offset, void *&pointer, std::size_t &remainingSpanBytes )
{
	pointer = nullptr;
	remainingSpanBytes = 0;
	if ( !dataBase || offset <= 0 || !AddressRangeFits( dataBase, dataSpanBytes )
		|| static_cast<std::uintptr_t>( offset ) >= dataSpanBytes ) {
		return false;
	}
	const std::size_t position = static_cast<std::size_t>( offset );
	pointer = static_cast<unsigned char *>( dataBase ) + position;
	remainingSpanBytes = dataSpanBytes - position;
	return true;
}

namespace {
/* Set once by the Q3Test game loader from its pinned release. */
int q3testProtocol = 0;
}

bool ServerGameDataView::SelectQ3TestProtocol( int protocol )
{
	switch ( protocol ) {
	case 35: case 36: case 38: case 39: case 40:
		break;
	default:
		return false;
	}
	if ( q3testProtocol && q3testProtocol != protocol ) {
		return false;
	}
	q3testProtocol = protocol;
	return true;
}

const ServerGameDataView::Layout *ServerGameDataView::FindLayout(
	comGameId_t profile )
{
#define SHARED_OFFSET( prefix, member ) \
	( offsetof( prefix##_sharedEntity_t, r ) \
		+ offsetof( prefix##_entityShared_t, member ) )
#define SHARED_STATE_OFFSET( prefix, member ) \
	( offsetof( prefix##_sharedEntity_t, r ) \
		+ offsetof( prefix##_entityShared_t, s ) \
		+ offsetof( prefix##_entityState_t, member ) )
	static const Layout q3Layout = {
		COM_GAME_QUAKE3,
		sizeof( fntech3_q3_sharedEntity_t ),
		sizeof( fntech3_q3_entityState_t ),
		sizeof( fntech3_q3_playerState_t ),
		SHARED_OFFSET( fntech3_q3, linked ),
		SHARED_OFFSET( fntech3_q3, linkcount ),
		SHARED_OFFSET( fntech3_q3, svFlags ),
		SHARED_OFFSET( fntech3_q3, singleClient ),
		SHARED_OFFSET( fntech3_q3, bmodel ),
		SHARED_OFFSET( fntech3_q3, mins ),
		SHARED_OFFSET( fntech3_q3, maxs ),
		SHARED_OFFSET( fntech3_q3, contents ),
		SHARED_OFFSET( fntech3_q3, absmin ),
		SHARED_OFFSET( fntech3_q3, absmax ),
		SHARED_OFFSET( fntech3_q3, currentOrigin ),
		SHARED_OFFSET( fntech3_q3, currentAngles ),
		SHARED_OFFSET( fntech3_q3, ownerNum ),
		SHARED_STATE_OFFSET( fntech3_q3, origin2 ),
		offsetof( fntech3_q3_playerState_t, ping ),
		FNTECH3_Q3_MAX_CLIENTS
	};
	static const Layout qlLayout = {
		COM_GAME_QUAKELIVE,
		sizeof( fntech3_ql_sharedEntity_t ),
		sizeof( fntech3_ql_entityState_t ),
		sizeof( fntech3_ql_playerState_t ),
		SHARED_OFFSET( fntech3_ql, linked ),
		SHARED_OFFSET( fntech3_ql, linkcount ),
		SHARED_OFFSET( fntech3_ql, svFlags ),
		SHARED_OFFSET( fntech3_ql, singleClient ),
		SHARED_OFFSET( fntech3_ql, bmodel ),
		SHARED_OFFSET( fntech3_ql, mins ),
		SHARED_OFFSET( fntech3_ql, maxs ),
		SHARED_OFFSET( fntech3_ql, contents ),
		SHARED_OFFSET( fntech3_ql, absmin ),
		SHARED_OFFSET( fntech3_ql, absmax ),
		SHARED_OFFSET( fntech3_ql, currentOrigin ),
		SHARED_OFFSET( fntech3_ql, currentAngles ),
		SHARED_OFFSET( fntech3_ql, ownerNum ),
		SHARED_STATE_OFFSET( fntech3_ql, origin2 ),
		offsetof( fntech3_ql_playerState_t, ping ),
		FNTECH3_Q3_MAX_CLIENTS
	};
#undef SHARED_STATE_OFFSET
#undef SHARED_OFFSET

	// SP and MP agree on r's field offsets, including its second entityState;
	// their total prefixes and player-state tails remain distinct contracts.
	constexpr auto rtcwLayout = []( comGameId_t game, std::size_t entityBytes,
		std::size_t playerBytes, std::int32_t maxClients ) {
#define RTCW_R(member) offsetof( fntech3_rtcw_sp_sharedEntity_t, r.member )
		return Layout{ game, entityBytes, sizeof( fntech3_rtcw_entityState_t ), playerBytes,
			RTCW_R(linked), RTCW_R(linkcount), RTCW_R(svFlags), RTCW_R(singleClient),
			RTCW_R(bmodel), RTCW_R(mins), RTCW_R(maxs), RTCW_R(contents),
			RTCW_R(absmin), RTCW_R(absmax), RTCW_R(currentOrigin), RTCW_R(currentAngles),
			RTCW_R(ownerNum), RTCW_R(s.origin2),
			offsetof( fntech3_rtcw_playerStatePrefix_t, ping ), maxClients };
#undef RTCW_R
	};
	static const Layout spLayout = rtcwLayout( COM_GAME_RTCW_SP,
		sizeof( fntech3_rtcw_sp_sharedEntity_t ), sizeof( fntech3_rtcw_sp_playerState_t ),
		FNTECH3_RTCW_SP_MAX_CLIENTS );
	static const Layout mpLayout = rtcwLayout( COM_GAME_RTCW_MP,
		sizeof( fntech3_rtcw_mp_sharedEntity_t ), sizeof( fntech3_rtcw_mp_playerState_t ),
		FNTECH3_RTCW_MP_MAX_CLIENTS );

#define ET_R(member) offsetof(fntech3_wolfet_sharedEntity_t, r.member)
	static const Layout etLayout{
		COM_GAME_WOLFET, sizeof(fntech3_wolfet_sharedEntity_t),
		sizeof(fntech3_wolfet_entityState_t), sizeof(fntech3_wolfet_playerState_t),
		ET_R(linked), ET_R(linkcount), ET_R(svFlags), ET_R(singleClient),
		ET_R(bmodel), ET_R(mins), ET_R(maxs), ET_R(contents), ET_R(absmin), ET_R(absmax),
		ET_R(currentOrigin), ET_R(currentAngles), ET_R(ownerNum),
		offsetof(fntech3_wolfet_sharedEntity_t, s.origin2),
		offsetof(fntech3_wolfet_playerState_t, ping), FNTECH3_WOLFET_MAX_CLIENTS
	};
#undef ET_R

	// Holomatch's entityShared_t has no leading entity state: r follows s.
#define EF_R(member) offsetof(fntech3_stvef_mp_sharedEntity_t, r.member)
	static const Layout stvefLayout{
		COM_GAME_STVEF_MP, sizeof(fntech3_stvef_mp_sharedEntity_t),
		sizeof(fntech3_stvef_mp_entityState_t), sizeof(fntech3_stvef_mp_playerState_t),
		EF_R(linked), EF_R(linkcount), EF_R(svFlags), EF_R(singleClient),
		EF_R(bmodel), EF_R(mins), EF_R(maxs), EF_R(contents), EF_R(absmin), EF_R(absmax),
		EF_R(currentOrigin), EF_R(currentAngles), EF_R(ownerNum),
		offsetof(fntech3_stvef_mp_sharedEntity_t, s.origin2),
		offsetof(fntech3_stvef_mp_playerState_t, ping), FNTECH3_STVEF_MP_MAX_CLIENTS
	};
#undef EF_R

	// Elite Force SP's gentity: its entityState, then the engine's fields,
	// with client and owner pointers and no linkcount or singleClient.
#define EFSP(member) offsetof(fntech3_stvef_sp_gentity_t, member)
	static const Layout stvefSpLayout{
		COM_GAME_STVEF_SP, sizeof(fntech3_stvef_sp_gentity_t),
		sizeof(fntech3_stvef_sp_entityState_t), sizeof(fntech3_stvef_sp_playerState_t),
		EFSP(linked), kNoField, EFSP(svFlags), kNoField,
		EFSP(bmodel), EFSP(mins), EFSP(maxs), EFSP(contents), EFSP(absmin), EFSP(absmax),
		EFSP(currentOrigin), EFSP(currentAngles), EFSP(owner), EFSP(s.origin2),
		offsetof(fntech3_stvef_sp_playerState_t, ping), FNTECH3_STVEF_SP_MAX_CLIENTS,
		true, EFSP(client)
	};
#undef EFSP

	// Jedi Academy's shared entity carries Raven's own ROFF flag and broadcast
	// mask in the engine's half, and the ICARUS and navigation fields the engine
	// also reads after it, so the whole 864-byte record is the shared prefix.
#define JKA_R(member) offsetof(fntech3_jka_sharedEntity_t, r.member)
	static const Layout jkaLayout{
		COM_GAME_JKA_MP, sizeof(fntech3_jka_sharedEntity_t),
		sizeof(fntech3_jka_entityState_t), sizeof(fntech3_jka_playerState_t),
		JKA_R(linked), JKA_R(linkcount), JKA_R(svFlags), JKA_R(singleClient),
		JKA_R(bmodel), JKA_R(mins), JKA_R(maxs), JKA_R(contents), JKA_R(absmin), JKA_R(absmax),
		JKA_R(currentOrigin), JKA_R(currentAngles), JKA_R(ownerNum),
		offsetof(fntech3_jka_sharedEntity_t, s.origin2),
		offsetof(fntech3_jka_playerState_t, ping), FNTECH3_JKA_MAX_CLIENTS
	};
#undef JKA_R
	/* API-8 single player publishes a fixed 1252 * 1024 arena. It has a
	 * 272-byte state, pointer owners, and no module linkcount. Player-state
	 * size and ping are deliberately absent until independently frozen. */
#define JKASP(member) offsetof(fntech3_jka_sp_gentity_prefix_t, member)
	static const Layout jkaSpLayout{
		COM_GAME_JKA_SP, sizeof(fntech3_jka_sp_gentity_prefix_t),
		sizeof(fntech3_jka_sp_entityState_t), 0,
		JKASP(linked), kNoField, JKASP(svFlags), kNoField,
		JKASP(bmodel), JKASP(mins), JKASP(maxs), JKASP(contents), JKASP(absmin), JKASP(absmax),
		JKASP(currentOrigin), JKASP(currentAngles), JKASP(owner_address), JKASP(s.origin2),
		kNoField, 1, true, JKASP(client_address)
	};
#undef JKASP

	// Jedi Outcast's records were measured rather than read: no multiplayer
	// source was released for it. Its entity carries the engine's hundred-byte
	// half at 296, after a 296-byte state, inside an 828-byte record the module
	// states for itself. Its ping sits at 484, read out of the place the retail
	// engine writes it rather than guessed from Quake III's field order.
	static const Layout jk2Layout{
		COM_GAME_JK2_MP, FNTECH3_JK2_SHARED_ENTITY_BYTES,
		FNTECH3_JK2_ENTITY_STATE_BYTES, FNTECH3_JK2_PLAYER_STATE_BYTES,
		FNTECH3_JK2_R_LINKED, FNTECH3_JK2_R_LINKCOUNT, FNTECH3_JK2_R_SVFLAGS,
		FNTECH3_JK2_R_SINGLECLIENT, FNTECH3_JK2_R_BMODEL, FNTECH3_JK2_R_MINS,
		FNTECH3_JK2_R_MAXS, FNTECH3_JK2_R_CONTENTS, FNTECH3_JK2_R_ABSMIN,
		FNTECH3_JK2_R_ABSMAX, FNTECH3_JK2_R_CURRENTORIGIN,
		FNTECH3_JK2_R_CURRENTANGLES, FNTECH3_JK2_R_OWNERNUM,
		FNTECH3_JK2_S_ORIGIN2, FNTECH3_JK2_PS_PING, FNTECH3_JK2_MAX_CLIENTS
	};

#define IHV(member) offsetof(fntech3_q3_ihv_entity_t, member)
	static const Layout ihvLayout{
		COM_GAME_QUAKE3_IHV, sizeof(fntech3_q3_ihv_entity_t),
		FNTECH3_Q3_IHV_ENTITY_STATE_BYTES, FNTECH3_Q3_IHV_PLAYER_STATE_BYTES,
		IHV(linked), IHV(linkcount), IHV(svFlags), kNoField,
		IHV(bmodel), IHV(mins), IHV(maxs), IHV(contents), IHV(absmin), IHV(absmax),
		IHV(currentOrigin), IHV(currentAngles), IHV(owner), 100,
		FNTECH3_Q3_IHV_PLAYER_PING_OFFSET, 256, true, IHV(client), 1, 2
	};
#undef IHV

	/* American McGee's Alice's entity record, which has no declaration
	 * anywhere: every offset was read out of an instruction in retail
	 * alice.exe that touches it, and the two halves of the record meet
	 * exactly -- the delta field table ends at 456 and the snapshot walk's
	 * first engine-owned read is at 456. See code/compat/alice_module_abi.h.
	 *
	 * Three members this view expects are not in the record at all, and each
	 * is marked absent rather than pointed at a plausible offset. Neither the
	 * linker nor the unlinker touches a link count, so the view keeps its own
	 * as it does for Elite Force's single player. Nothing in the snapshot
	 * walk restricts an entity to one client, so there is no single-client
	 * member to read; that is measured by absence, from a walk whose every
	 * flag test is accounted for. The whole 732-byte record is the readable
	 * prefix because the module owns all of it and publishes nothing past it. */
	static const Layout aliceLayout{
		COM_GAME_ALICE, FNTECH3_ALICE_GENTITY_SIZE,
		FNTECH3_ALICE_ENTITY_STATE_BYTES,
		FNTECH3_ALICE_PLAYER_STATE_BYTES,
		FNTECH3_ALICE_R_LINKED, kNoField, FNTECH3_ALICE_R_SVFLAGS, kNoField,
		FNTECH3_ALICE_R_BMODEL, FNTECH3_ALICE_R_MINS, FNTECH3_ALICE_R_MAXS,
		FNTECH3_ALICE_R_CONTENTS, FNTECH3_ALICE_R_ABSMIN,
		FNTECH3_ALICE_R_ABSMAX, FNTECH3_ALICE_R_CURRENTORIGIN,
		FNTECH3_ALICE_R_CURRENTANGLES, FNTECH3_ALICE_S_OWNERNUM,
		FNTECH3_ALICE_S_ORIGIN2, FNTECH3_ALICE_PS_PING,
		FNTECH3_ALICE_MAX_CLIENTS
	};

	/* Heavy Metal: F.A.K.K.2's, measured in its retail modules: Alice's engine
	 * half in Alice's order, 24 bytes earlier, after F.A.K.K.2's shorter
	 * entity state. The same three members are absent for the same reasons. */
	static const Layout fakk2Layout{
		COM_GAME_FAKK2, FNTECH3_FAKK2_GENTITY_SIZE,
		FNTECH3_FAKK2_ENTITY_STATE_BYTES,
		FNTECH3_FAKK2_PLAYER_STATE_BYTES,
		FNTECH3_FAKK2_R_LINKED, kNoField, FNTECH3_FAKK2_R_SVFLAGS, kNoField,
		FNTECH3_FAKK2_R_BMODEL, FNTECH3_FAKK2_R_MINS, FNTECH3_FAKK2_R_MAXS,
		FNTECH3_FAKK2_R_CONTENTS, FNTECH3_FAKK2_R_ABSMIN,
		FNTECH3_FAKK2_R_ABSMAX, FNTECH3_FAKK2_R_CURRENTORIGIN,
		FNTECH3_FAKK2_R_CURRENTANGLES, FNTECH3_FAKK2_S_BINDPARENT,
		FNTECH3_FAKK2_S_ORIGIN2, FNTECH3_FAKK2_PS_PING,
		FNTECH3_FAKK2_MAX_CLIENTS
	};

	/* Medal of Honor: Allied Assault's entity record, measured in its retail
	 * modules (code/compat/mohaa_game_abi.h). 2015's shared half has no
	 * current origin: its engine links and sends an entity at its state's
	 * origin, so that is the member named here. The whole 852-byte record is
	 * the readable prefix, as for Alice, and the player state is the first
	 * 660 bytes of each 2,272-byte client. */
#define MOHAA_E(member) offsetof(fntech3::mohaa::GEntity, member)
	static const Layout mohaaLayout{
		COM_GAME_MOHAA, FNTECH3_MOHAA_GENTITY_BYTES,
		FNTECH3_MOHAA_ENTITY_STATE_BYTES, FNTECH3_MOHAA_PLAYER_STATE_BYTES,
		MOHAA_E(r.linked), MOHAA_E(r.linkcount), MOHAA_E(r.svFlags), MOHAA_E(r.singleClient),
		MOHAA_E(r.bmodel), MOHAA_E(r.mins), MOHAA_E(r.maxs), MOHAA_E(r.contents),
		MOHAA_E(r.absmin), MOHAA_E(r.absmax), MOHAA_E(s.origin), MOHAA_E(r.currentAngles),
		MOHAA_E(r.ownerNum), MOHAA_E(s.origin2),
		offsetof(fntech3::mohaa::PlayerState, ping), FNTECH3_MOHAA_MAX_CLIENTS
	};
#undef MOHAA_E

	/* Q3Test's entity is IHV's shape around a 204-byte state: the engine half
	 * follows at 0xcc, the client and owner are pointers, and the release
	 * decides the player record's size, its ping and where clients start:
	 * entity n + 1 up to 1.05, entity n from 1.06 (ABI record 5.1, 5.4, 6.1). */
#define Q3T(member) offsetof(fntech3_q3test_entity_t, member)
	static const Layout q3test35Layout{
		COM_GAME_Q3TEST, sizeof(fntech3_q3test_entity_t), FNTECH3_Q3TEST_ENTITY_STATE_BYTES, 276,
		Q3T(linked), Q3T(linkcount), Q3T(svFlags), kNoField, Q3T(bmodel), Q3T(mins), Q3T(maxs),
		Q3T(contents), Q3T(absmin), Q3T(absmax), Q3T(currentOrigin), Q3T(currentAngles), Q3T(owner),
		Q3T(s.origin2), 0x110, FNTECH3_Q3TEST_MAX_CLIENTS, true, Q3T(client), 1
	};
	static const Layout q3test36Layout{
		COM_GAME_Q3TEST, sizeof(fntech3_q3test_entity_t), FNTECH3_Q3TEST_ENTITY_STATE_BYTES, 416,
		Q3T(linked), Q3T(linkcount), Q3T(svFlags), kNoField, Q3T(bmodel), Q3T(mins), Q3T(maxs),
		Q3T(contents), Q3T(absmin), Q3T(absmax), Q3T(currentOrigin), Q3T(currentAngles), Q3T(owner),
		Q3T(s.origin2), 0x19c, FNTECH3_Q3TEST_MAX_CLIENTS, true, Q3T(client), 1
	};
	static const Layout q3test38Layout{
		COM_GAME_Q3TEST, sizeof(fntech3_q3test_entity_t), FNTECH3_Q3TEST_ENTITY_STATE_BYTES, 432,
		Q3T(linked), Q3T(linkcount), Q3T(svFlags), kNoField, Q3T(bmodel), Q3T(mins), Q3T(maxs),
		Q3T(contents), Q3T(absmin), Q3T(absmax), Q3T(currentOrigin), Q3T(currentAngles), Q3T(owner),
		Q3T(s.origin2), 0x1ac, FNTECH3_Q3TEST_MAX_CLIENTS, true, Q3T(client), 0
	};
	static const Layout q3test40Layout{
		COM_GAME_Q3TEST, sizeof(fntech3_q3test_entity_t), FNTECH3_Q3TEST_ENTITY_STATE_BYTES, 440,
		Q3T(linked), Q3T(linkcount), Q3T(svFlags), kNoField, Q3T(bmodel), Q3T(mins), Q3T(maxs),
		Q3T(contents), Q3T(absmin), Q3T(absmax), Q3T(currentOrigin), Q3T(currentAngles), Q3T(owner),
		Q3T(s.origin2), 0x1b4, FNTECH3_Q3TEST_MAX_CLIENTS, true, Q3T(client), 0
	};
#undef Q3T

	switch ( profile ) {
	case COM_GAME_Q3TEST: {
		switch ( q3testProtocol ) {
		case 35: return &q3test35Layout;
		case 36: return &q3test36Layout;
		case 38: case 39: return &q3test38Layout;
		case 40: return &q3test40Layout;
		default: return nullptr;
		}
	}
	case COM_GAME_MOHAA:
		return &mohaaLayout;
	case COM_GAME_ALICE:
		return &aliceLayout;
	case COM_GAME_FAKK2:
		return &fakk2Layout;
	case COM_GAME_QUAKE3:
		return &q3Layout;
	case COM_GAME_QUAKELIVE:
		return &qlLayout;
	case COM_GAME_RTCW_SP:
		return &spLayout;
	case COM_GAME_RTCW_MP:
		return &mpLayout;
	case COM_GAME_WOLFET:
		return &etLayout;
	case COM_GAME_STVEF_MP:
		return &stvefLayout;
	case COM_GAME_STVEF_SP:
		return &stvefSpLayout;
	case COM_GAME_QUAKE3_IHV:
		return &ihvLayout;
	case COM_GAME_JKA_MP:
		return &jkaLayout;
	case COM_GAME_JKA_SP:
		return &jkaSpLayout;
	case COM_GAME_JK2_MP:
		return &jk2Layout;
	default:
		return nullptr;
	}
}

const char *GameDataBindErrorName( GameDataBindError error )
{
	switch ( error ) {
	case GameDataBindError::None: return "none";
	case GameDataBindError::UnsupportedProfile: return "unsupported profile";
	case GameDataBindError::InvalidEntityCount: return "invalid entity count";
	case GameDataBindError::NullEntityBase: return "null entity base";
	case GameDataBindError::MisalignedEntityBase: return "misaligned entity base";
	case GameDataBindError::EntityStrideTooSmall: return "entity stride too small";
	case GameDataBindError::MisalignedEntityStride: return "misaligned entity stride";
	case GameDataBindError::EntitySpanOverflow: return "entity span overflow";
	case GameDataBindError::EntitySpanTooSmall: return "entity span too small";
	case GameDataBindError::EntityAddressOverflow: return "entity address overflow";
	case GameDataBindError::InvalidClientCount: return "invalid client count";
	case GameDataBindError::NullClientBase: return "null client base";
	case GameDataBindError::MisalignedClientBase: return "misaligned client base";
	case GameDataBindError::ClientStrideTooSmall: return "client stride too small";
	case GameDataBindError::MisalignedClientStride: return "misaligned client stride";
	case GameDataBindError::ClientSpanOverflow: return "client span overflow";
	case GameDataBindError::ClientSpanTooSmall: return "client span too small";
	case GameDataBindError::ClientAddressOverflow: return "client address overflow";
	case GameDataBindError::MissingClientValidation: return "missing native client allocation validator";
	}
	return "unknown";
}

GameDataBindError ServerGameDataView::BindNative( comGameId_t profile,
	const NativeGameDataBinding &binding )
{
	AdvanceGeneration();
	ClearBinding();
	const Layout *candidateLayout = FindLayout( profile );
	if ( !candidateLayout || ((profile == COM_GAME_QUAKE3_IHV || profile == COM_GAME_Q3TEST
		|| profile == COM_GAME_JKA_SP) && sizeof(void *) != 4) ) {
		return GameDataBindError::UnsupportedProfile;
	}
	if ( binding.entityCount <= 0
		|| binding.entityCount > FNTECH3_Q3_MAX_GENTITIES ) {
		return GameDataBindError::InvalidEntityCount;
	}
	const int entityCapacity = binding.entityCapacity ? binding.entityCapacity : binding.entityCount;
	// Alice joins the profiles whose published count is a high-water mark
	// rather than the array's bound: its LocateGameData reports the entities
	// in use at the moment it is called and the module then spawns a level's
	// worth past that, so the arena to bind is the capacity it publishes
	// separately. RTCW MP and ET join for the world record past the mark,
	// which their settled explosives take as owner and trace with.
	if ( entityCapacity < binding.entityCount || entityCapacity > FNTECH3_Q3_MAX_GENTITIES
		|| ( profile != COM_GAME_RTCW_SP && profile != COM_GAME_RTCW_MP && profile != COM_GAME_WOLFET
			&& profile != COM_GAME_STVEF_SP && profile != COM_GAME_QUAKE3_IHV
			&& profile != COM_GAME_ALICE && profile != COM_GAME_FAKK2 && profile != COM_GAME_MOHAA
			&& profile != COM_GAME_Q3TEST && profile != COM_GAME_JKA_SP && entityCapacity != binding.entityCount ) )
		return GameDataBindError::InvalidEntityCount;
	if ( !binding.entityBase ) {
		return GameDataBindError::NullEntityBase;
	}
	if ( reinterpret_cast<std::uintptr_t>( binding.entityBase )
		% kAbiAlignment != 0 ) {
		return GameDataBindError::MisalignedEntityBase;
	}
	if ( binding.entityStride < candidateLayout->entityPrefixBytes ) {
		return GameDataBindError::EntityStrideTooSmall;
	}
	if ( binding.entityStride % kAbiAlignment != 0 ) {
		return GameDataBindError::MisalignedEntityStride;
	}
	std::size_t requiredEntitySpan = 0;
	if ( !CheckedRequiredSpan( entityCapacity, binding.entityStride,
		requiredEntitySpan ) ) {
		return GameDataBindError::EntitySpanOverflow;
	}
	if ( binding.entitySpanBytes < requiredEntitySpan ) {
		return GameDataBindError::EntitySpanTooSmall;
	}
	if ( !AddressRangeFits( binding.entityBase, requiredEntitySpan ) ) {
		return GameDataBindError::EntityAddressOverflow;
	}

	if ( binding.clientCount <= 0
		|| binding.clientCount > candidateLayout->maxClients ) {
		return GameDataBindError::InvalidClientCount;
	}
	if ( candidateLayout->clientPointerOffset != kNoField ) {
		// IHV reserves entity zero for the world; EF SP starts at entity zero.
		if ( binding.clientCount > entityCapacity - candidateLayout->firstClientEntity ) {
			return GameDataBindError::InvalidClientCount;
		}
		if ( profile == COM_GAME_QUAKE3_IHV && !binding.validateClientRecord ) {
			return GameDataBindError::MissingClientValidation;
		}
		layout_ = candidateLayout;
		entityBase_ = static_cast<unsigned char *>( binding.entityBase );
		entitySpanBytes_ = requiredEntitySpan;
		entityCount_ = binding.entityCount;
		entityCapacity_ = entityCapacity;
		entityStride_ = binding.entityStride;
		clientCount_ = binding.clientCount;
		validateClientRecord_ = binding.validateClientRecord;
		clientValidationContext_ = binding.clientValidationContext;
		return GameDataBindError::None;
	}
	if ( !binding.clientBase ) {
		return GameDataBindError::NullClientBase;
	}
	if ( reinterpret_cast<std::uintptr_t>( binding.clientBase )
		% kAbiAlignment != 0 ) {
		return GameDataBindError::MisalignedClientBase;
	}
	if ( binding.clientStride < candidateLayout->playerStateBytes ) {
		return GameDataBindError::ClientStrideTooSmall;
	}
	if ( binding.clientStride % kAbiAlignment != 0 ) {
		return GameDataBindError::MisalignedClientStride;
	}
	std::size_t requiredClientSpan = 0;
	if ( !CheckedRequiredSpan( binding.clientCount, binding.clientStride,
		requiredClientSpan ) ) {
		return GameDataBindError::ClientSpanOverflow;
	}
	if ( binding.clientSpanBytes < requiredClientSpan ) {
		return GameDataBindError::ClientSpanTooSmall;
	}
	if ( !AddressRangeFits( binding.clientBase, requiredClientSpan ) ) {
		return GameDataBindError::ClientAddressOverflow;
	}

	layout_ = candidateLayout;
	entityBase_ = static_cast<unsigned char *>( binding.entityBase );
	entitySpanBytes_ = requiredEntitySpan;
	entityCount_ = binding.entityCount;
	entityCapacity_ = entityCapacity;
	entityStride_ = binding.entityStride;
	clientBase_ = static_cast<unsigned char *>( binding.clientBase );
	clientSpanBytes_ = requiredClientSpan;
	clientCount_ = binding.clientCount;
	clientStride_ = binding.clientStride;
	return GameDataBindError::None;
}

void ServerGameDataView::AdvanceGeneration()
{
	if ( generation_ == ( std::numeric_limits<std::uint64_t>::max )() ) {
		generation_ = 1;
	}
	else {
		++generation_;
	}
}

void ServerGameDataView::ClearBinding()
{
	layout_ = nullptr;
	entityBase_ = nullptr;
	entitySpanBytes_ = 0;
	entityCount_ = 0;
	entityCapacity_ = 0;
	entityStride_ = 0;
	clientBase_ = nullptr;
	clientSpanBytes_ = 0;
	clientCount_ = 0;
	clientStride_ = 0;
	validateClientRecord_ = nullptr;
	clientValidationContext_ = nullptr;
}

void ServerGameDataView::Invalidate()
{
	AdvanceGeneration();
	ClearBinding();
	std::memset( engineLinkcounts_, 0, sizeof( engineLinkcounts_ ) );
}

bool ServerGameDataView::IsBound() const { return layout_ != nullptr; }
comGameId_t ServerGameDataView::Profile() const
{
	return layout_ ? layout_->profile : COM_GAME_AUTO;
}
std::uint64_t ServerGameDataView::Generation() const { return generation_; }
std::int32_t ServerGameDataView::EntityCount() const { return entityCount_; }
std::int32_t ServerGameDataView::ClientCount() const { return clientCount_; }
std::size_t ServerGameDataView::EntityStride() const { return entityStride_; }
std::size_t ServerGameDataView::ClientStride() const { return clientStride_; }

std::size_t ServerGameDataView::EntityPrefixBytesForProfile(
	comGameId_t profile )
{
	const Layout *layout = FindLayout( profile );
	return layout ? layout->entityPrefixBytes : 0;
}

std::size_t ServerGameDataView::PlayerStateBytesForProfile(
	comGameId_t profile )
{
	const Layout *layout = FindLayout( profile );
	return layout ? layout->playerStateBytes : 0;
}

bool ServerGameDataView::EntityRefAt( std::int32_t index,
	GameEntityRef &destination ) const
{
	if ( !layout_ || index < 0 || index >= entityCapacity_ ) {
		return false;
	}
	destination.owner = this;
	destination.generation = generation_;
	destination.index = index;
	return true;
}

bool ServerGameDataView::EntityRefForPointer( const void *pointer,
	GameEntityRef &destination ) const
{
	if ( !layout_ || !pointer ) {
		return false;
	}
	const std::uintptr_t base = reinterpret_cast<std::uintptr_t>( entityBase_ );
	const std::uintptr_t value = reinterpret_cast<std::uintptr_t>( pointer );
	if ( value < base || value >= base + entitySpanBytes_ ) {
		return false;
	}
	const std::size_t difference = static_cast<std::size_t>( value - base );
	if ( difference % entityStride_ != 0 ) {
		return false;
	}
	const std::size_t index = difference / entityStride_;
	if ( index >= static_cast<std::size_t>( entityCapacity_ ) ) {
		return false;
	}
	return EntityRefAt( static_cast<std::int32_t>( index ), destination );
}

bool ServerGameDataView::IsCurrent( const GameEntityRef &reference ) const
{
	return layout_ && reference.owner == this
		&& reference.generation == generation_
		&& reference.index >= 0 && reference.index < entityCapacity_;
}

void *ServerGameDataView::EntityPointer( const GameEntityRef &reference )
{
	if ( !IsCurrent( reference ) ) {
		return nullptr;
	}
	return entityBase_ + static_cast<std::size_t>( reference.index )
		* entityStride_;
}

const void *ServerGameDataView::EntityPointer(
	const GameEntityRef &reference ) const
{
	if ( !IsCurrent( reference ) ) {
		return nullptr;
	}
	return entityBase_ + static_cast<std::size_t>( reference.index )
		* entityStride_;
}

unsigned char *ServerGameDataView::ClientRecord( std::int32_t clientIndex ) const
{
	if ( !layout_ || !layout_->playerStateBytes || clientIndex < 0 || clientIndex >= clientCount_ ) {
		return nullptr;
	}
	if ( layout_->clientPointerOffset == kNoField ) {
		return clientBase_ + static_cast<std::size_t>( clientIndex ) * clientStride_;
	}
	// The selected native layout owns the entity-to-client index mapping.
	void *pointer = nullptr;
	std::memcpy( &pointer, entityBase_ + static_cast<std::size_t>( clientIndex + layout_->firstClientEntity ) * entityStride_
		+ layout_->clientPointerOffset, sizeof( pointer ) );
	if ( reinterpret_cast<std::uintptr_t>( pointer ) % kAbiAlignment != 0 ) {
		return nullptr;
	}
	if ( layout_->profile == COM_GAME_QUAKE3_IHV && (!pointer || !validateClientRecord_
		|| !validateClientRecord_(clientValidationContext_, pointer, clientIndex, layout_->playerStateBytes)) ) {
		return nullptr;
	}
	return static_cast<unsigned char *>( pointer );
}

void *ServerGameDataView::ClientPointer( std::int32_t index )
{
	return ClientRecord( index );
}

const void *ServerGameDataView::ClientPointer( std::int32_t index ) const
{
	return ClientRecord( index );
}

bool ServerGameDataView::ReadEntityBytes( const GameEntityRef &reference,
	std::size_t offset, void *destination, std::size_t byteCount ) const
{
	if ( !destination || !IsCurrent( reference ) || offset > layout_->entityPrefixBytes
		|| byteCount > layout_->entityPrefixBytes - offset ) {
		return false;
	}
	const unsigned char *record = entityBase_
		+ static_cast<std::size_t>( reference.index ) * entityStride_;
	std::memcpy( destination, record + offset, byteCount );
	return true;
}

bool ServerGameDataView::WriteEntityBytes( const GameEntityRef &reference,
	std::size_t offset, const void *source, std::size_t byteCount )
{
	if ( !source || !IsCurrent( reference ) || offset > layout_->entityPrefixBytes
		|| byteCount > layout_->entityPrefixBytes - offset ) {
		return false;
	}
	unsigned char *record = entityBase_
		+ static_cast<std::size_t>( reference.index ) * entityStride_;
	std::memcpy( record + offset, source, byteCount );
	return true;
}

bool ServerGameDataView::ReadClientBytes( std::int32_t clientIndex,
	std::size_t offset, void *destination, std::size_t byteCount ) const
{
	if ( !destination || !layout_ || offset > layout_->playerStateBytes
		|| byteCount > layout_->playerStateBytes - offset ) {
		return false;
	}
	const unsigned char *record = ClientRecord( clientIndex );
	if ( !record ) {
		return false;
	}
	std::memcpy( destination, record + offset, byteCount );
	return true;
}

bool ServerGameDataView::WriteClientBytes( std::int32_t clientIndex,
	std::size_t offset, const void *source, std::size_t byteCount )
{
	if ( !source || !layout_ || offset > layout_->playerStateBytes
		|| byteCount > layout_->playerStateBytes - offset ) {
		return false;
	}
	unsigned char *record = ClientRecord( clientIndex );
	if ( !record ) {
		return false;
	}
	std::memcpy( record + offset, source, byteCount );
	return true;
}

bool ServerGameDataView::ReadEntityState( const GameEntityRef &reference,
	entityState_t &destination ) const
{
	if ( !layout_ ) {
		return false;
	}
	if ( layout_->profile == COM_GAME_QUAKE3 ) {
		fntech3_q3_entityState_t external{};
		if ( !ReadEntityBytes( reference, 0, &external, sizeof( external ) ) ) {
			return false;
		}
		FnTech3_InternalEntityStateFromQ3( &destination, &external );
		return true;
	}
	if ( layout_->profile == COM_GAME_QUAKE3_IHV ) {
		fntech3_q3_ihv_entityState_t external{};
		return ReadEntityState(reference, external) && q3ihv::state::ToInternal(external, destination);
	}
	if ( layout_->profile == COM_GAME_Q3TEST ) {
		fntech3_q3test_entityState_t external{};
		return ReadEntityBytes( reference, 0, &external, sizeof( external ) ) &&
			q3test::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_ALICE ) {
		fntech3_alice_entityState_t external{};
		return ReadEntityBytes( reference, 0, &external, sizeof( external ) ) &&
			alice::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_FAKK2 ) {
		/* F.A.K.K.2's record crosses into Alice's and from there into this
		 * engine's, losing nothing on the way (code/compat/fakk2_convert.hpp). */
		fntech3_fakk2_entityState_t external{};
		fntech3_alice_entityState_t alice{};
		if ( !ReadEntityBytes( reference, 0, &external, sizeof( external ) ) ) return false;
		fakk2::convert::EntityToAlice( external, alice );
		return alice::state::ToInternal( alice, destination );
	}
	if ( layout_->profile == COM_GAME_RTCW_SP || layout_->profile == COM_GAME_RTCW_MP ) {
		fntech3_rtcw_entityState_t external{};
		return ReadEntityState( reference, external ) && rtcw::state::ToInternal( external, destination );
	}
	if (layout_->profile == COM_GAME_WOLFET) {
		fntech3_wolfet_entityState_t external{};
		return ReadEntityBytes(reference, 0, &external, sizeof(external)) && wolfet::state::ToInternal(external, destination);
	}
	if ( layout_->profile == COM_GAME_JK2_MP ) {
		fntech3_jk2_entityState_t external{};
		return ReadEntityBytes( reference, 0, &external, sizeof( external ) ) &&
			jk2::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_JKA_MP ) {
		fntech3_jka_entityState_t external{};
		return ReadEntityBytes( reference, 0, &external, sizeof( external ) ) &&
			jka::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_JKA_SP ) {
		fntech3_jka_sp_entityState_t external{};
		return ReadEntityBytes(reference, 0, &external, sizeof(external))
			&& jka::sp::state::ToInternal(external, destination);
	}
	if ( layout_->profile == COM_GAME_STVEF_MP ) {
		fntech3_stvef_mp_entityState_t external{};
		return ReadEntityBytes( reference, 0, &external, sizeof( external ) ) &&
			stvef::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_STVEF_SP ) {
		fntech3_stvef_sp_entityState_t external{};
		return ReadEntityBytes( reference, 0, &external, sizeof( external ) ) &&
			stvef::state::ToInternal( external, destination );
	}
	if ( layout_->profile != COM_GAME_QUAKELIVE ) return false;
	fntech3_ql_entityState_t external{};
	if ( !ReadEntityBytes( reference, 0, &external, sizeof( external ) ) ) {
		return false;
	}
	InternalEntityStateFromQl( destination, external );
	return true;
}

bool ServerGameDataView::WriteEntityState( const GameEntityRef &reference,
	const entityState_t &source )
{
	if ( !layout_ ) {
		return false;
	}
	if ( layout_->profile == COM_GAME_QUAKE3_IHV ) {
		fntech3_q3_ihv_entityState_t external{};
		return ReadEntityState(reference, external) && q3ihv::state::FromInternal(source, external)
			&& WriteEntityState(reference, external);
	}
	if ( layout_->profile == COM_GAME_Q3TEST ) {
		fntech3_q3test_entityState_t external{};
		return q3test::state::FromInternal( source, external ) &&
			WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile == COM_GAME_ALICE ) {
		/* Read first, then put back only the members this engine owns: the
		 * forty-nine Ritual members it has nowhere to keep stay exactly as
		 * the module left them. */
		fntech3_alice_entityState_t external{};
		return ReadEntityBytes( reference, 0, &external, sizeof( external ) ) &&
			alice::state::FromInternal( source, external ) &&
			WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile == COM_GAME_FAKK2 ) {
		/* The same, through Alice's record: every F.A.K.K.2 member has a home
		 * there, so the ones this engine does not own come back unchanged. */
		fntech3_fakk2_entityState_t external{};
		fntech3_alice_entityState_t alice{};
		if ( !ReadEntityBytes( reference, 0, &external, sizeof( external ) ) ) return false;
		fakk2::convert::EntityToAlice( external, alice );
		if ( !alice::state::FromInternal( source, alice ) ) return false;
		fakk2::convert::EntityFromAlice( alice, external );
		return WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile == COM_GAME_QUAKE3 ) {
		fntech3_q3_entityState_t external{};
		FnTech3_Q3EntityStateFromInternal( &external, &source );
		return WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile == COM_GAME_RTCW_SP || layout_->profile == COM_GAME_RTCW_MP ) {
		fntech3_rtcw_entityState_t external{};
		return rtcw::state::FromInternal( source, external ) && WriteEntityState( reference, external );
	}
	if (layout_->profile == COM_GAME_WOLFET) {
		fntech3_wolfet_entityState_t external{};
		return wolfet::state::FromInternal(source, external) && WriteEntityBytes(reference, 0, &external, sizeof(external));
	}
	if ( layout_->profile == COM_GAME_JK2_MP ) {
		fntech3_jk2_entityState_t external{};
		return jk2::state::FromInternal( source, external ) &&
			WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile == COM_GAME_JKA_MP ) {
		fntech3_jka_entityState_t external{};
		return jka::state::FromInternal( source, external ) &&
			WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile == COM_GAME_JKA_SP ) {
		fntech3_jka_sp_entityState_t external{};
		return jka::sp::state::FromInternal(source, external)
			&& WriteEntityBytes(reference, 0, &external, sizeof(external));
	}
	if ( layout_->profile == COM_GAME_STVEF_MP ) {
		fntech3_stvef_mp_entityState_t external{};
		return stvef::state::FromInternal( source, external ) &&
			WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile == COM_GAME_STVEF_SP ) {
		fntech3_stvef_sp_entityState_t external{};
		return stvef::state::FromInternal( source, external ) &&
			WriteEntityBytes( reference, 0, &external, sizeof( external ) );
	}
	if ( layout_->profile != COM_GAME_QUAKELIVE ) return false;
	fntech3_ql_entityState_t external{};
	QlEntityStateFromInternal( external, source );
	return WriteEntityBytes( reference, 0, &external, sizeof( external ) );
}

bool ServerGameDataView::ReadEntityState( const GameEntityRef &reference,
	fntech3_rtcw_entityState_t &destination ) const
{
	return layout_ && ( layout_->profile == COM_GAME_RTCW_SP || layout_->profile == COM_GAME_RTCW_MP ) &&
		ReadEntityBytes( reference, 0, &destination, sizeof( destination ) );
}

bool ServerGameDataView::WriteEntityState( const GameEntityRef &reference,
	const fntech3_rtcw_entityState_t &source )
{
	return layout_ && ( layout_->profile == COM_GAME_RTCW_SP || layout_->profile == COM_GAME_RTCW_MP ) &&
		WriteEntityBytes( reference, 0, &source, sizeof( source ) );
}

bool ServerGameDataView::ReadEntityState(const GameEntityRef &reference,
	fntech3_q3_ihv_entityState_t &destination) const {
	return layout_ && layout_->profile == COM_GAME_QUAKE3_IHV
		&& ReadEntityBytes(reference, 0, &destination, sizeof(destination));
}
bool ServerGameDataView::WriteEntityState(const GameEntityRef &reference,
	const fntech3_q3_ihv_entityState_t &source) {
	return layout_ && layout_->profile == COM_GAME_QUAKE3_IHV
		&& WriteEntityBytes(reference, 0, &source, sizeof(source));
}
bool ServerGameDataView::GetInuse(const GameEntityRef &reference, std::int32_t &destination) const {
	if (layout_ && layout_->profile == COM_GAME_JKA_SP)
		return ReadEntityBytes(reference, offsetof(fntech3_jka_sp_gentity_prefix_t,inuse), &destination, sizeof(destination));
	if (layout_ && layout_->profile == COM_GAME_Q3TEST)
		return ReadEntityBytes(reference, offsetof(fntech3_q3test_entity_t,inuse), &destination, sizeof(destination));
	return layout_ && layout_->profile == COM_GAME_QUAKE3_IHV
		&& ReadEntityBytes(reference, offsetof(fntech3_q3_ihv_entity_t,inuse), &destination, sizeof(destination));
}

#define DEFINE_SCALAR_ACCESSORS( Name, layoutMember ) \
	bool ServerGameDataView::Get##Name( const GameEntityRef &reference, \
		std::int32_t &destination ) const \
	{ \
		return layout_ && ReadEntityBytes( reference, layout_->layoutMember, \
			&destination, sizeof( destination ) ); \
	} \
	bool ServerGameDataView::Set##Name( const GameEntityRef &reference, \
		std::int32_t value ) \
	{ \
		return layout_ && WriteEntityBytes( reference, layout_->layoutMember, \
			&value, sizeof( value ) ); \
	}

DEFINE_SCALAR_ACCESSORS( Linked, linkedOffset )
DEFINE_SCALAR_ACCESSORS( Bmodel, bmodelOffset )
DEFINE_SCALAR_ACCESSORS( Contents, contentsOffset )

#undef DEFINE_SCALAR_ACCESSORS

bool ServerGameDataView::GetSvFlags(const GameEntityRef &reference, std::int32_t &destination) const {
	if (!layout_ || !ReadEntityBytes(reference, layout_->svFlagsOffset, &destination, sizeof(destination))) return false;
	/* SP's other bits mean NPC, script and gameplay flags, not Q3's client
	 * masks/capsules. Only shared meanings enter the engine world view. */
	if (layout_->profile == COM_GAME_JKA_SP) destination &= 1|8|32|64|128;
	return true;
}
bool ServerGameDataView::SetSvFlags(const GameEntityRef &reference, std::int32_t value) {
	if (!layout_) return false;
	if (layout_->profile == COM_GAME_JKA_SP) {
		std::int32_t original;
		if (!ReadEntityBytes(reference, layout_->svFlagsOffset, &original, sizeof(original))) return false;
		constexpr int shared = 1|8|32|64|128;
		value = (original & ~shared) | (value & shared);
	}
	return WriteEntityBytes(reference, layout_->svFlagsOffset, &value, sizeof(value));
}

bool ServerGameDataView::GetLinkcount( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	if ( layout_ && layout_->linkcountOffset == kNoField ) {
		if ( !IsCurrent( reference ) ) return false;
		destination = engineLinkcounts_[reference.index];
		return true;
	}
	return layout_ && ReadEntityBytes( reference, layout_->linkcountOffset,
		&destination, sizeof( destination ) );
}

bool ServerGameDataView::SetLinkcount( const GameEntityRef &reference,
	std::int32_t value )
{
	if ( layout_ && layout_->linkcountOffset == kNoField ) {
		if ( !IsCurrent( reference ) ) return false;
		engineLinkcounts_[reference.index] = value;
		return true;
	}
	return layout_ && WriteEntityBytes( reference, layout_->linkcountOffset,
		&value, sizeof( value ) );
}

// A record without singleClient sends every entity to its one client.
bool ServerGameDataView::GetSingleClient( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	if ( layout_ && layout_->singleClientOffset == kNoField ) {
		if ( !IsCurrent( reference ) ) return false;
		destination = 0;
		return true;
	}
	return layout_ && ReadEntityBytes( reference, layout_->singleClientOffset,
		&destination, sizeof( destination ) );
}

bool ServerGameDataView::SetSingleClient( const GameEntityRef &reference,
	std::int32_t value )
{
	if ( layout_ && layout_->singleClientOffset == kNoField ) {
		return IsCurrent( reference );
	}
	return layout_ && WriteEntityBytes( reference, layout_->singleClientOffset,
		&value, sizeof( value ) );
}

/* Native pointer owners project to checked arena indices. IHV uses -1 for a
 * null owner because its entity 1023 is valid; malformed nonnull pointers fail.
 * Elite Force SP retains ENTITYNUM_NONE for null/unrecognized pointers. Writing
 * back the index the engine read leaves the original pointer untouched. */
bool ServerGameDataView::GetOwnerNum( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	if ( !layout_ ) return false;
	if ( !layout_->ownerIsPointer ) {
		return ReadEntityBytes( reference, layout_->ownerNumOffset,
			&destination, sizeof( destination ) );
	}
	void *owner = nullptr;
	if ( !ReadEntityBytes( reference, layout_->ownerNumOffset, &owner, sizeof( owner ) ) ) {
		return false;
	}
	GameEntityRef target;
	// Q3Test's owner is a pointer too, and its A/B entity 1023 is valid.
	const bool nativeOwner = layout_->profile == COM_GAME_QUAKE3_IHV || layout_->profile == COM_GAME_Q3TEST;
	if ((nativeOwner || layout_->profile == COM_GAME_JKA_SP) && owner && !EntityRefForPointer(owner,target)) return false;
	const int noOwner = nativeOwner ? -1 : ENTITYNUM_NONE;
	destination = owner && EntityRefForPointer( owner, target ) ? target.index : noOwner;
	return true;
}

bool ServerGameDataView::SetOwnerNum( const GameEntityRef &reference,
	std::int32_t value )
{
	if ( !layout_ ) return false;
	if ( !layout_->ownerIsPointer ) {
		return WriteEntityBytes( reference, layout_->ownerNumOffset,
			&value, sizeof( value ) );
	}
	std::int32_t current = 0;
	if ( !GetOwnerNum( reference, current ) ) return false;
	if ( value == current ) return true;
	void *owner = nullptr;
	const int noOwner = layout_->profile == COM_GAME_QUAKE3_IHV || layout_->profile == COM_GAME_Q3TEST ? -1 : ENTITYNUM_NONE;
	if ( value != noOwner ) {
		if ( value < 0 || value >= entityCapacity_ ) return false;
		owner = entityBase_ + static_cast<std::size_t>( value ) * entityStride_;
	}
	return WriteEntityBytes( reference, layout_->ownerNumOffset, &owner, sizeof( owner ) );
}

bool ServerGameDataView::GetWolfETSnapshotCallback( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	return layout_ && layout_->profile == COM_GAME_WOLFET &&
		ReadEntityBytes( reference, offsetof( fntech3_wolfet_sharedEntity_t, r.snapshotCallback ),
			&destination, sizeof( destination ) );
}

bool ServerGameDataView::GetRTCWMPWorldflags( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	return layout_ && layout_->profile == COM_GAME_RTCW_MP &&
		ReadEntityBytes( reference, offsetof( fntech3_rtcw_mp_sharedEntity_t, worldflags ),
			&destination, sizeof( destination ) );
}

bool ServerGameDataView::GetNumber( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	if (layout_ && layout_->entityNumberBytes == 2) {
		std::int16_t value;
		if(!ReadEntityBytes(reference,0,&value,sizeof(value)))return false;
		destination=value;return true;
	}
	return ReadEntityBytes( reference, 0, &destination, sizeof( destination ) );
}

bool ServerGameDataView::SetNumber( const GameEntityRef &reference,
	std::int32_t value )
{
	if (layout_ && layout_->entityNumberBytes == 2) {
		if(value < -32768 || value > 32767)return false;
		const auto native=static_cast<std::int16_t>(value);
		return WriteEntityBytes(reference,0,&native,sizeof(native));
	}
	return WriteEntityBytes( reference, 0, &value, sizeof( value ) );
}

#define DEFINE_VECTOR_ACCESSORS( Name, layoutMember ) \
	bool ServerGameDataView::Get##Name( const GameEntityRef &reference, \
		float destination[3] ) const \
	{ \
		return layout_ && ReadEntityBytes( reference, layout_->layoutMember, \
			destination, sizeof( float ) * 3 ); \
	} \
	bool ServerGameDataView::Set##Name( const GameEntityRef &reference, \
		const float value[3] ) \
	{ \
		return layout_ && WriteEntityBytes( reference, layout_->layoutMember, \
			value, sizeof( float ) * 3 ); \
	}

DEFINE_VECTOR_ACCESSORS( Mins, minsOffset )
DEFINE_VECTOR_ACCESSORS( Maxs, maxsOffset )
DEFINE_VECTOR_ACCESSORS( Absmin, absminOffset )
DEFINE_VECTOR_ACCESSORS( Absmax, absmaxOffset )
DEFINE_VECTOR_ACCESSORS( CurrentOrigin, currentOriginOffset )
DEFINE_VECTOR_ACCESSORS( CurrentAngles, currentAnglesOffset )
DEFINE_VECTOR_ACCESSORS( SharedStateOrigin2, sharedStateOrigin2Offset )

#undef DEFINE_VECTOR_ACCESSORS

bool ServerGameDataView::ReadPlayerState( std::int32_t clientIndex,
	playerState_t &destination ) const
{
	if ( !layout_ ) {
		return false;
	}
	if ( layout_->profile == COM_GAME_QUAKE3 ) {
		fntech3_q3_playerState_t external{};
		if ( !ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) ) {
			return false;
		}
		FnTech3_InternalPlayerStateFromQ3( &destination, &external );
		return true;
	}
	if ( layout_->profile == COM_GAME_RTCW_SP ) {
		fntech3_rtcw_sp_playerState_t external{};
		return ReadPlayerState( clientIndex, external ) && rtcw::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_RTCW_MP ) {
		fntech3_rtcw_mp_playerState_t external{};
		return ReadPlayerState( clientIndex, external ) && rtcw::state::ToInternal( external, destination );
	}
	if (layout_->profile == COM_GAME_WOLFET) {
		fntech3_wolfet_playerState_t external{};
		return ReadClientBytes(clientIndex, 0, &external, sizeof(external)) && wolfet::state::ToInternal(external, destination);
	}
	if ( layout_->profile == COM_GAME_JK2_MP ) {
		fntech3_jk2_playerState_t external{};
		return ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) &&
			jk2::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_JKA_MP ) {
		fntech3_jka_playerState_t external{};
		return ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) &&
			jka::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_STVEF_MP ) {
		fntech3_stvef_mp_playerState_t external{};
		return ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) &&
			stvef::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_STVEF_SP ) {
		fntech3_stvef_sp_playerState_t external{};
		return ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) &&
			stvef::state::ToInternal( external, destination );
	}
	if ( layout_->profile == COM_GAME_Q3TEST ) {
		const auto *player = q3test::state::PlayerLayoutForProtocol( q3testProtocol );
		std::uint8_t external[440];
		return player && player->bytes <= sizeof( external )
			&& ReadClientBytes( clientIndex, 0, external, player->bytes )
			&& q3test::state::PlayerToInternal( *player, external, destination );
	}
	/* F.A.K.K.2's player state is Alice's record, member for member. */
	if ( layout_->profile == COM_GAME_ALICE || layout_->profile == COM_GAME_FAKK2 ) {
		fntech3_alice_playerState_t external{};
		return ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) &&
			alice::state::ToInternal( external, destination );
	}
	/* Medal of Honor's record (mohaa_game_abi.h) keeps no persistant scores:
	 * a client's score is its kills stat, as 2015's status prints it (OpenMoHAA
	 * sv_ccmds.c). Only the members that mean the same are carried over. */
	if ( layout_->profile == COM_GAME_MOHAA ) {
		fntech3::mohaa::PlayerState external{};
		if ( !ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) ) {
			return false;
		}
		destination = playerState_t{};
		destination.commandTime = external.commandTime;
		destination.clientNum = external.clientNum;
		destination.ping = external.ping;
		for ( int i = 0; i < 3; i++ ) {
			destination.origin[i] = external.origin[i];
			destination.velocity[i] = external.velocity[i];
			destination.viewangles[i] = external.viewangles[i];
		}
		for ( int i = 0; i < MAX_STATS; i++ ) {
			destination.stats[i] = external.stats[i];
		}
		/* persistant[0] is PERS_SCORE, which the engine's status and
		 * getstatus read (game/bg_public.h: it must not change), and
		 * persistant[8] PERS_KILLED, the times the player died, which the
		 * GameSpy player list reports. */
		destination.persistant[0] = external.stats[fntech3::mohaa::kStatKills];
		destination.persistant[8] = external.stats[fntech3::mohaa::kStatDeaths];
		return true;
	}
	if ( layout_->profile != COM_GAME_QUAKELIVE ) return false;
	fntech3_ql_playerState_t external{};
	if ( !ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) ) {
		return false;
	}
	InternalPlayerStateFromQl( destination, external );
	return true;
}

bool ServerGameDataView::ReadPlayerState( std::int32_t clientIndex,
	fntech3_rtcw_sp_playerState_t &destination ) const
{
	return layout_ && layout_->profile == COM_GAME_RTCW_SP &&
		ReadClientBytes( clientIndex, 0, &destination, sizeof( destination ) );
}

bool ServerGameDataView::ReadPlayerState( std::int32_t clientIndex,
	fntech3_rtcw_mp_playerState_t &destination ) const
{
	return layout_ && layout_->profile == COM_GAME_RTCW_MP &&
		ReadClientBytes( clientIndex, 0, &destination, sizeof( destination ) );
}

bool ServerGameDataView::GetPlayerPing( std::int32_t clientIndex,
	std::int32_t &destination ) const
{
	return layout_ && ReadClientBytes( clientIndex, layout_->playerPingOffset,
		&destination, sizeof( destination ) );
}

bool ServerGameDataView::ReadPlayerState(std::int32_t clientIndex,
	fntech3_q3_ihv_playerState_t &destination) const {
	return layout_ && layout_->profile == COM_GAME_QUAKE3_IHV
		&& ReadClientBytes(clientIndex,0,&destination,sizeof(destination));
}

bool ServerGameDataView::ReadQ3TestEntityState( const GameEntityRef &reference,
	void *destination, std::size_t bytes ) const
{
	return layout_ && layout_->profile == COM_GAME_Q3TEST && bytes == layout_->entityStateBytes
		&& ReadEntityBytes( reference, 0, destination, bytes );
}

bool ServerGameDataView::ReadQ3TestPlayerState( std::int32_t clientIndex,
	void *destination, std::size_t bytes ) const
{
	return layout_ && layout_->profile == COM_GAME_Q3TEST && bytes == layout_->playerStateBytes
		&& ReadClientBytes( clientIndex, 0, destination, bytes );
}

bool ServerGameDataView::SetPlayerPing( std::int32_t clientIndex,
	std::int32_t value )
{
	/* A profile whose player record has no measured ping refuses to write one
	 * rather than putting it wherever the field would be in another game. */
	if ( !layout_ || layout_->playerPingOffset == kNoField ) return false;
	return WriteClientBytes( clientIndex, layout_->playerPingOffset,
		&value, sizeof( value ) );
}

} // namespace fntech3::server
