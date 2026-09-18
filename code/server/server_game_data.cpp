/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "server_game_data.h"

#include "../compat/game_abi_adapter.h"
#include "../compat/q3_game_abi.h"
#include "../compat/ql_game_abi.h"
#include "../compat/rtcw_state_adapter.hpp"
#include "../compat/stvef_state_adapter.hpp"
#include "../compat/wolfet_state_adapter.hpp"

#include <cstring>
#include <limits>

namespace fntech3::server {

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

	switch ( profile ) {
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
	}
	return "unknown";
}

GameDataBindError ServerGameDataView::BindNative( comGameId_t profile,
	const NativeGameDataBinding &binding )
{
	AdvanceGeneration();
	ClearBinding();
	const Layout *candidateLayout = FindLayout( profile );
	if ( !candidateLayout ) {
		return GameDataBindError::UnsupportedProfile;
	}
	if ( binding.entityCount <= 0
		|| binding.entityCount > FNTECH3_Q3_MAX_GENTITIES ) {
		return GameDataBindError::InvalidEntityCount;
	}
	const int entityCapacity = binding.entityCapacity ? binding.entityCapacity : binding.entityCount;
	if ( entityCapacity < binding.entityCount || entityCapacity > FNTECH3_Q3_MAX_GENTITIES
		|| ( profile != COM_GAME_RTCW_SP && entityCapacity != binding.entityCount ) )
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
}

void ServerGameDataView::Invalidate()
{
	AdvanceGeneration();
	ClearBinding();
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

void *ServerGameDataView::ClientPointer( std::int32_t index )
{
	if ( !layout_ || index < 0 || index >= clientCount_ ) {
		return nullptr;
	}
	return clientBase_ + static_cast<std::size_t>( index ) * clientStride_;
}

const void *ServerGameDataView::ClientPointer( std::int32_t index ) const
{
	if ( !layout_ || index < 0 || index >= clientCount_ ) {
		return nullptr;
	}
	return clientBase_ + static_cast<std::size_t>( index ) * clientStride_;
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
	if ( !destination || !layout_ || clientIndex < 0 || clientIndex >= clientCount_
		|| offset > layout_->playerStateBytes
		|| byteCount > layout_->playerStateBytes - offset ) {
		return false;
	}
	const unsigned char *record = clientBase_
		+ static_cast<std::size_t>( clientIndex ) * clientStride_;
	std::memcpy( destination, record + offset, byteCount );
	return true;
}

bool ServerGameDataView::WriteClientBytes( std::int32_t clientIndex,
	std::size_t offset, const void *source, std::size_t byteCount )
{
	if ( !source || !layout_ || clientIndex < 0 || clientIndex >= clientCount_
		|| offset > layout_->playerStateBytes
		|| byteCount > layout_->playerStateBytes - offset ) {
		return false;
	}
	unsigned char *record = clientBase_
		+ static_cast<std::size_t>( clientIndex ) * clientStride_;
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
	if ( layout_->profile == COM_GAME_RTCW_SP || layout_->profile == COM_GAME_RTCW_MP ) {
		fntech3_rtcw_entityState_t external{};
		return ReadEntityState( reference, external ) && rtcw::state::ToInternal( external, destination );
	}
	if (layout_->profile == COM_GAME_WOLFET) {
		fntech3_wolfet_entityState_t external{};
		return ReadEntityBytes(reference, 0, &external, sizeof(external)) && wolfet::state::ToInternal(external, destination);
	}
	if ( layout_->profile == COM_GAME_STVEF_MP ) {
		fntech3_stvef_mp_entityState_t external{};
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
	if ( layout_->profile == COM_GAME_STVEF_MP ) {
		fntech3_stvef_mp_entityState_t external{};
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
DEFINE_SCALAR_ACCESSORS( Linkcount, linkcountOffset )
DEFINE_SCALAR_ACCESSORS( SvFlags, svFlagsOffset )
DEFINE_SCALAR_ACCESSORS( SingleClient, singleClientOffset )
DEFINE_SCALAR_ACCESSORS( Bmodel, bmodelOffset )
DEFINE_SCALAR_ACCESSORS( Contents, contentsOffset )
DEFINE_SCALAR_ACCESSORS( OwnerNum, ownerNumOffset )

#undef DEFINE_SCALAR_ACCESSORS

bool ServerGameDataView::GetWolfETSnapshotCallback( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	return layout_ && layout_->profile == COM_GAME_WOLFET &&
		ReadEntityBytes( reference, offsetof( fntech3_wolfet_sharedEntity_t, r.snapshotCallback ),
			&destination, sizeof( destination ) );
}

bool ServerGameDataView::GetNumber( const GameEntityRef &reference,
	std::int32_t &destination ) const
{
	return ReadEntityBytes( reference, 0, &destination, sizeof( destination ) );
}

bool ServerGameDataView::SetNumber( const GameEntityRef &reference,
	std::int32_t value )
{
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
	if ( layout_->profile == COM_GAME_STVEF_MP ) {
		fntech3_stvef_mp_playerState_t external{};
		return ReadClientBytes( clientIndex, 0, &external, sizeof( external ) ) &&
			stvef::state::ToInternal( external, destination );
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

bool ServerGameDataView::SetPlayerPing( std::int32_t clientIndex,
	std::int32_t value )
{
	return layout_ && WriteClientBytes( clientIndex, layout_->playerPingOffset,
		&value, sizeof( value ) );
}

} // namespace fntech3::server
