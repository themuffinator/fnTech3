/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "../code/server/server_game_data.h"

#include "../code/compat/alice_game_abi.h"
#include "../code/compat/alice_state_adapter.hpp"
#include "../code/compat/q3_game_abi.h"
#include "../code/compat/ql_game_abi.h"
#include "../code/compat/jka_game_abi.h"
#include "../code/compat/stvef_game_abi.h"
#include "../code/compat/wolfet_game_abi.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <type_traits>
#include <vector>

using fntech3::server::GameDataBindError;
using fntech3::server::GameEntityRef;
using fntech3::server::NativeGameDataBinding;
using fntech3::server::ServerGameDataView;

namespace {

constexpr std::size_t kGuardBytes = 16;
constexpr unsigned char kCanary = 0xa5;
int failures;

void Expect( bool condition, const char *message )
{
	if ( !condition ) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

struct Storage {
	std::vector<unsigned char> entities;
	std::vector<unsigned char> clients;
	std::size_t entityStride;
	std::size_t clientStride;
	std::int32_t entityCount;
	std::int32_t clientCount;

	Storage( std::size_t entityStrideValue, std::size_t clientStrideValue,
		std::int32_t entityCountValue = 2, std::int32_t clientCountValue = 2 )
		: entities( kGuardBytes + entityStrideValue
			* static_cast<std::size_t>( entityCountValue ) + kGuardBytes, kCanary ),
		clients( kGuardBytes + clientStrideValue
			* static_cast<std::size_t>( clientCountValue ) + kGuardBytes, kCanary ),
		entityStride( entityStrideValue ),
		clientStride( clientStrideValue ),
		entityCount( entityCountValue ),
		clientCount( clientCountValue )
	{
		Expect( reinterpret_cast<std::uintptr_t>( EntityBase() ) % 4 == 0,
			"test entity storage is four-byte aligned" );
		Expect( reinterpret_cast<std::uintptr_t>( ClientBase() ) % 4 == 0,
			"test client storage is four-byte aligned" );
	}

	unsigned char *EntityBase() { return entities.data() + kGuardBytes; }
	unsigned char *ClientBase() { return clients.data() + kGuardBytes; }

	NativeGameDataBinding Binding()
	{
		NativeGameDataBinding binding;
		binding.entityBase = EntityBase();
		binding.entitySpanBytes = entityStride
			* static_cast<std::size_t>( entityCount );
		binding.entityCount = entityCount;
		binding.entityStride = entityStride;
		binding.clientBase = ClientBase();
		binding.clientSpanBytes = clientStride
			* static_cast<std::size_t>( clientCount );
		binding.clientCount = clientCount;
		binding.clientStride = clientStride;
		return binding;
	}
};

bool RangeIsCanary( const std::vector<unsigned char> &bytes,
	std::size_t begin, std::size_t end )
{
	for ( std::size_t index = begin; index < end; ++index ) {
		if ( bytes[index] != kCanary ) {
			return false;
		}
	}
	return true;
}

void ExpectGuardsIntact( const Storage &storage, const char *message )
{
	Expect( RangeIsCanary( storage.entities, 0, kGuardBytes ), message );
	Expect( RangeIsCanary( storage.entities,
		storage.entities.size() - kGuardBytes, storage.entities.size() ), message );
	Expect( RangeIsCanary( storage.clients, 0, kGuardBytes ), message );
	Expect( RangeIsCanary( storage.clients,
		storage.clients.size() - kGuardBytes, storage.clients.size() ), message );
}

void ExpectOnlyRangeChanged( const std::vector<unsigned char> &before,
	const std::vector<unsigned char> &after, std::size_t changedBegin,
	std::size_t changedBytes, const char *message )
{
	Expect( before.size() == after.size(), "comparison buffers have equal size" );
	for ( std::size_t index = 0; index < before.size(); ++index ) {
		if ( ( index < changedBegin || index >= changedBegin + changedBytes )
			&& before[index] != after[index] ) {
			Expect( false, message );
			return;
		}
	}
}

/* `storesSingleClient` is false for a record that has no such member: the
 * view answers zero for it and writes nothing, which is the contract, not a
 * failure. */
void CheckFieldAccessors( ServerGameDataView &view,
	const GameEntityRef &reference, bool storesSingleClient = true )
{
	std::int32_t scalar = 0;
	Expect( view.SetNumber( reference, 41 ), "set state number" );
	Expect( view.GetNumber( reference, scalar ) && scalar == 41,
		"get state number" );
	Expect( view.SetLinked( reference, 1 ), "set linked" );
	Expect( view.GetLinked( reference, scalar ) && scalar == 1, "get linked" );
	Expect( view.SetLinkcount( reference, 2 ), "set linkcount" );
	Expect( view.GetLinkcount( reference, scalar ) && scalar == 2,
		"get linkcount" );
	Expect( view.SetSvFlags( reference, 3 ), "set svFlags" );
	Expect( view.GetSvFlags( reference, scalar ) && scalar == 3,
		"get svFlags" );
	Expect( view.SetSingleClient( reference, 4 ), "set singleClient" );
	Expect( view.GetSingleClient( reference, scalar )
		&& scalar == ( storesSingleClient ? 4 : 0 ), "get singleClient" );
	Expect( view.SetBmodel( reference, 1 ), "set bmodel" );
	Expect( view.GetBmodel( reference, scalar ) && scalar == 1, "get bmodel" );
	Expect( view.SetContents( reference, 5 ), "set contents" );
	Expect( view.GetContents( reference, scalar ) && scalar == 5,
		"get contents" );
	Expect( view.SetOwnerNum( reference, 6 ), "set ownerNum" );
	Expect( view.GetOwnerNum( reference, scalar ) && scalar == 6,
		"get ownerNum" );

	const float mins[3] = { -1.0f, -2.0f, -3.0f };
	const float maxs[3] = { 1.0f, 2.0f, 3.0f };
	const float absmin[3] = { -4.0f, -5.0f, -6.0f };
	const float absmax[3] = { 4.0f, 5.0f, 6.0f };
	const float origin[3] = { 7.0f, 8.0f, 9.0f };
	const float angles[3] = { 10.0f, 11.0f, 12.0f };
	const float portal[3] = { 13.0f, 14.0f, 15.0f };
	float actual[3] = {};
#define CHECK_VECTOR( Name, expected ) \
	Expect( view.Set##Name( reference, expected ), "set " #Name ); \
	Expect( view.Get##Name( reference, actual ) \
		&& std::memcmp( actual, expected, sizeof( actual ) ) == 0, "get " #Name )
	CHECK_VECTOR( Mins, mins );
	CHECK_VECTOR( Maxs, maxs );
	CHECK_VECTOR( Absmin, absmin );
	CHECK_VECTOR( Absmax, absmax );
	CHECK_VECTOR( CurrentOrigin, origin );
	CHECK_VECTOR( CurrentAngles, angles );
	CHECK_VECTOR( SharedStateOrigin2, portal );
#undef CHECK_VECTOR
}

void TestQ3ExactEntityRecord()
{
	Storage storage( sizeof( fntech3_q3_sharedEntity_t ),
		sizeof( fntech3_q3_playerState_t ) + 20 );
	ServerGameDataView view;
	Expect( view.BindNative( COM_GAME_QUAKE3, storage.Binding() )
		== GameDataBindError::None, "bind exact Q3 entity records" );
	Expect( view.EntityStride() == 516, "Q3 exact entity stride is 516" );

	fntech3_q3_entityState_t external{};
	external.number = 7;
	external.eType = 8;
	external.pos.trType = FNTECH3_Q3_TR_GRAVITY;
	external.pos.trBase[1] = 25.0f;
	external.event = 91;
	external.weapon = 4;
	std::memcpy( storage.EntityBase() + storage.entityStride, &external,
		sizeof( external ) );
	const std::vector<unsigned char> firstRecord(
		storage.EntityBase(), storage.EntityBase() + storage.entityStride );

	GameEntityRef reference;
	Expect( view.EntityRefAt( 1, reference ), "create Q3 entity reference" );
	entityState_t canonical{};
	Expect( view.ReadEntityState( reference, canonical ), "read Q3 state" );
	Expect( canonical.number == 7 && canonical.eType == 8
		&& canonical.pos.trBase[1] == 25.0f && canonical.event == 91,
		"Q3 state converts into canonical state" );
	Expect( canonical.pos.gravity == 0.0f && canonical.health == 0
		&& canonical.armor == 0 && canonical.location == 0,
		"Q3 state conversion clears QL-only fields" );

	canonical.number = 33;
	canonical.origin[2] = 44.0f;
	canonical.health = 999;
	Expect( view.WriteEntityState( reference, canonical ), "write Q3 state" );
	fntech3_q3_entityState_t written{};
	std::memcpy( &written, storage.EntityBase() + storage.entityStride,
		sizeof( written ) );
	Expect( written.number == 33 && written.origin[2] == 44.0f,
		"Q3 external state receives canonical fields" );
	CheckFieldAccessors( view, reference );
	Expect( std::memcmp( storage.EntityBase(), firstRecord.data(),
		firstRecord.size() ) == 0, "Q3 writes do not touch adjacent record" );
	ExpectGuardsIntact( storage, "Q3 exact-record canaries remain intact" );
}

void TestQlExactEntityRecord()
{
	Storage storage( sizeof( fntech3_ql_sharedEntity_t ),
		sizeof( fntech3_ql_playerState_t ) + 24 );
	ServerGameDataView view;
	Expect( view.BindNative( COM_GAME_QUAKELIVE, storage.Binding() )
		== GameDataBindError::None, "bind exact QL entity records" );
	Expect( view.EntityStride() == 572, "QL exact entity stride is 572" );

	fntech3_ql_entityState_t external{};
	external.number = 17;
	external.eType = 18;
	external.pos.trType = FNTECH3_QL_TR_ACCEL;
	external.pos.gravity = 777.0f;
	external.health = 125;
	external.armor = 75;
	external.location = 9;
	external.doubleJumped = 1;
	std::memcpy( storage.EntityBase() + storage.entityStride, &external,
		sizeof( external ) );
	const std::vector<unsigned char> firstRecord(
		storage.EntityBase(), storage.EntityBase() + storage.entityStride );

	GameEntityRef reference;
	Expect( view.EntityRefAt( 1, reference ), "create QL entity reference" );
	entityState_t canonical{};
	Expect( view.ReadEntityState( reference, canonical ), "read QL state" );
	Expect( canonical.number == 17 && canonical.eType == 18
		&& canonical.pos.trType == TR_QL_ACCEL
		&& canonical.pos.gravity == 777.0f && canonical.health == 125
		&& canonical.armor == 75 && canonical.location == 9
		&& canonical.doubleJumped == 1,
		"QL state converts into canonical state" );

	canonical.number = 43;
	canonical.jumpTime = 1234;
	Expect( view.WriteEntityState( reference, canonical ), "write QL state" );
	fntech3_ql_entityState_t written{};
	std::memcpy( &written, storage.EntityBase() + storage.entityStride,
		sizeof( written ) );
	Expect( written.number == 43 && written.jumpTime == 1234,
		"QL external state receives canonical fields" );
	CheckFieldAccessors( view, reference );
	Expect( std::memcmp( storage.EntityBase(), firstRecord.data(),
		firstRecord.size() ) == 0, "QL writes do not touch adjacent record" );
	ExpectGuardsIntact( storage, "QL exact-record canaries remain intact" );
}

void TestQ3PlayerPingAndPrivateTail()
{
	constexpr std::size_t privateBytes = 20;
	Storage storage( sizeof( fntech3_q3_sharedEntity_t ),
		sizeof( fntech3_q3_playerState_t ) + privateBytes );
	fntech3_q3_playerState_t external{};
	external.commandTime = 111;
	external.pm_flags = 0x12345;
	external.weapon = 8;
	external.externalEventTime = 222;
	external.ping = 30;
	std::memcpy( storage.ClientBase() + storage.clientStride, &external,
		sizeof( external ) );
	ServerGameDataView view;
	Expect( view.BindNative( COM_GAME_QUAKE3, storage.Binding() )
		== GameDataBindError::None, "bind Q3 clients" );
	playerState_t canonical{};
	Expect( view.ReadPlayerState( 1, canonical ), "read Q3 player state" );
	Expect( canonical.commandTime == 111 && canonical.pm_flags == 0x2345
		&& canonical.weapon == 8 && canonical.externalEventTime == 222
		&& canonical.weaponPrimary == 0 && canonical.fov == 0,
		"Q3 player state converts and masks profile-only values" );
	std::int32_t ping = 0;
	Expect( view.GetPlayerPing( 1, ping ) && ping == 30, "read Q3 ping" );
	const std::vector<unsigned char> before = storage.clients;
	Expect( view.SetPlayerPing( 1, 55 ), "write Q3 ping" );
	const std::size_t pingOffset = kGuardBytes + storage.clientStride
		+ offsetof( fntech3_q3_playerState_t, ping );
	ExpectOnlyRangeChanged( before, storage.clients, pingOffset,
		sizeof( std::int32_t ), "Q3 ping write changed bytes outside ping" );
	Expect( RangeIsCanary( storage.clients,
		kGuardBytes + storage.clientStride + sizeof( fntech3_q3_playerState_t ),
		kGuardBytes + storage.clientStride * 2 ),
		"Q3 client private tail remains untouched" );
	ExpectGuardsIntact( storage, "Q3 client canaries remain intact" );
}

void TestQlPlayerPingAndPrivateTail()
{
	constexpr std::size_t privateBytes = 24;
	Storage storage( sizeof( fntech3_ql_sharedEntity_t ),
		sizeof( fntech3_ql_playerState_t ) + privateBytes );
	fntech3_ql_playerState_t external{};
	external.commandTime = 311;
	external.pm_flags = 0x123456;
	external.weaponPrimary = 1;
	external.fov = 105;
	external.location = 6;
	external.externalEventTime = 422;
	external.armorTier = 2;
	external.ping = 40;
	std::memcpy( storage.ClientBase() + storage.clientStride, &external,
		sizeof( external ) );
	ServerGameDataView view;
	Expect( view.BindNative( COM_GAME_QUAKELIVE, storage.Binding() )
		== GameDataBindError::None, "bind QL clients" );
	playerState_t canonical{};
	Expect( view.ReadPlayerState( 1, canonical ), "read QL player state" );
	Expect( canonical.commandTime == 311 && canonical.pm_flags == 0x123456
		&& canonical.weaponPrimary == 1 && canonical.fov == 105
		&& canonical.location == 6 && canonical.externalEventTime == 422
		&& canonical.armorTier == 2,
		"QL player state converts into canonical state" );
	std::int32_t ping = 0;
	Expect( view.GetPlayerPing( 1, ping ) && ping == 40, "read QL ping" );
	const std::vector<unsigned char> before = storage.clients;
	Expect( view.SetPlayerPing( 1, 65 ), "write QL ping" );
	const std::size_t pingOffset = kGuardBytes + storage.clientStride
		+ offsetof( fntech3_ql_playerState_t, ping );
	ExpectOnlyRangeChanged( before, storage.clients, pingOffset,
		sizeof( std::int32_t ), "QL ping write changed bytes outside ping" );
	Expect( RangeIsCanary( storage.clients,
		kGuardBytes + storage.clientStride + sizeof( fntech3_ql_playerState_t ),
		kGuardBytes + storage.clientStride * 2 ),
		"QL client private tail remains untouched" );
	ExpectGuardsIntact( storage, "QL client canaries remain intact" );
}

void TestPointerConversionsAndGenerations()
{
	Storage q3( sizeof( fntech3_q3_sharedEntity_t ),
		sizeof( fntech3_q3_playerState_t ) );
	Storage ql( sizeof( fntech3_ql_sharedEntity_t ),
		sizeof( fntech3_ql_playerState_t ) );
	ServerGameDataView view;
	Expect( view.BindNative( COM_GAME_QUAKE3, q3.Binding() )
		== GameDataBindError::None, "bind Q3 for pointer tests" );
	GameEntityRef first;
	GameEntityRef second;
	Expect( view.EntityRefAt( 0, first ) && view.EntityRefAt( 1, second ),
		"make refs for exact indexes" );
	Expect( view.EntityPointer( second ) == q3.EntityBase() + q3.entityStride,
		"entity ref converts to exact record pointer" );
	GameEntityRef converted;
	Expect( view.EntityRefForPointer( q3.EntityBase() + q3.entityStride,
		converted ) && converted.index == 1,
		"exact record pointer converts to entity ref" );
	Expect( !view.EntityRefForPointer( q3.EntityBase() + 1, converted ),
		"interior pointer is rejected" );
	Expect( !view.EntityRefForPointer( q3.EntityBase()
		+ q3.entityStride * static_cast<std::size_t>( q3.entityCount ), converted ),
		"one-past entity pointer is rejected" );
	Expect( !view.EntityRefForPointer( q3.entities.data(), converted ),
		"pointer before entity span is rejected" );
	Expect( !view.EntityRefAt( -1, converted )
		&& !view.EntityRefAt( q3.entityCount, converted ),
		"out-of-range entity indexes are rejected" );
	Expect( view.ClientPointer( 1 ) == q3.ClientBase() + q3.clientStride,
		"client index converts to exact pointer" );
	Expect( view.ClientPointer( -1 ) == nullptr
		&& view.ClientPointer( q3.clientCount ) == nullptr,
		"out-of-range client indexes are rejected" );

	const std::uint64_t firstGeneration = first.generation;
	Expect( view.BindNative( COM_GAME_QUAKELIVE, ql.Binding() )
		== GameDataBindError::None, "rebind from Q3 to QL" );
	Expect( view.Generation() != firstGeneration && !view.IsCurrent( first )
		&& view.EntityPointer( first ) == nullptr,
		"successful rebind invalidates prior refs" );
	GameEntityRef qlReference;
	Expect( view.EntityRefAt( 0, qlReference ), "make ref after rebind" );
	NativeGameDataBinding invalid = ql.Binding();
	invalid.entityCount = 0;
	Expect( view.BindNative( COM_GAME_QUAKELIVE, invalid )
		== GameDataBindError::InvalidEntityCount,
		"invalid rebind reports the validation failure" );
	Expect( !view.IsBound() && !view.IsCurrent( qlReference ),
		"failed rebind invalidates current binding and refs" );

	Expect( view.BindNative( COM_GAME_QUAKE3, q3.Binding() )
		== GameDataBindError::None, "bind after failed rebind" );
	GameEntityRef beforeInvalidate;
	Expect( view.EntityRefAt( 0, beforeInvalidate ), "make ref before invalidate" );
	view.Invalidate();
	Expect( !view.IsBound() && !view.IsCurrent( beforeInvalidate ),
		"explicit invalidation clears binding and refs" );

	ServerGameDataView other;
	Expect( other.BindNative( COM_GAME_QUAKE3, q3.Binding() )
		== GameDataBindError::None, "bind second view" );
	GameEntityRef foreign;
	Expect( other.EntityRefAt( 0, foreign ) && !view.IsCurrent( foreign ),
		"entity refs cannot cross view owners" );
}

void TestBindingValidation()
{
	Storage storage( sizeof( fntech3_q3_sharedEntity_t ),
		sizeof( fntech3_q3_playerState_t ) );
	ServerGameDataView view;
	NativeGameDataBinding binding = storage.Binding();
	Expect( view.BindNative( COM_GAME_AUTO, binding )
		== GameDataBindError::UnsupportedProfile, "reject automatic ABI profile" );

	NativeGameDataBinding changed = binding;
	changed.entityCount = 0;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::InvalidEntityCount, "reject zero entities" );
	changed = binding;
	changed.entityCount = FNTECH3_Q3_MAX_GENTITIES + 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::InvalidEntityCount, "reject excessive entities" );
	changed = binding;
	changed.entityBase = nullptr;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::NullEntityBase, "reject null entity base" );
	changed = binding;
	changed.entityBase = storage.EntityBase() + 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::MisalignedEntityBase,
		"reject misaligned entity base" );
	changed = binding;
	changed.entityStride = sizeof( fntech3_q3_sharedEntity_t ) - 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::EntityStrideTooSmall,
		"reject undersized entity stride" );
	changed = binding;
	changed.entityStride = sizeof( fntech3_q3_sharedEntity_t ) + 2;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::MisalignedEntityStride,
		"reject misaligned entity stride" );
	changed = binding;
	changed.entitySpanBytes -= 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::EntitySpanTooSmall,
		"reject undersized entity span" );
	changed = binding;
	changed.entityCount = 2;
	changed.entityStride = ( std::numeric_limits<std::size_t>::max )()
		- ( std::numeric_limits<std::size_t>::max )() % 4;
	changed.entitySpanBytes = ( std::numeric_limits<std::size_t>::max )();
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::EntitySpanOverflow,
		"reject overflowing entity span" );
	changed = binding;
	changed.entityBase = reinterpret_cast<void *>(
		( std::numeric_limits<std::uintptr_t>::max )() - 3 );
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::EntityAddressOverflow,
		"reject overflowing entity address range" );

	changed = binding;
	changed.clientCount = 0;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::InvalidClientCount, "reject zero clients" );
	changed = binding;
	changed.clientCount = FNTECH3_Q3_MAX_CLIENTS + 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::InvalidClientCount, "reject excessive clients" );
	changed = binding;
	changed.clientBase = nullptr;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::NullClientBase, "reject null client base" );
	changed = binding;
	changed.clientBase = storage.ClientBase() + 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::MisalignedClientBase,
		"reject misaligned client base" );
	changed = binding;
	changed.clientStride = sizeof( fntech3_q3_playerState_t ) - 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::ClientStrideTooSmall,
		"reject undersized client stride" );
	changed = binding;
	changed.clientStride = sizeof( fntech3_q3_playerState_t ) + 2;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::MisalignedClientStride,
		"reject misaligned client stride" );
	changed = binding;
	changed.clientSpanBytes -= 1;
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::ClientSpanTooSmall,
		"reject undersized client span" );
	changed = binding;
	changed.clientCount = 2;
	changed.clientStride = ( std::numeric_limits<std::size_t>::max )()
		- ( std::numeric_limits<std::size_t>::max )() % 4;
	changed.clientSpanBytes = ( std::numeric_limits<std::size_t>::max )();
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::ClientSpanOverflow,
		"reject overflowing client span" );
	changed = binding;
	changed.clientBase = reinterpret_cast<void *>(
		( std::numeric_limits<std::uintptr_t>::max )() - 3 );
	Expect( view.BindNative( COM_GAME_QUAKE3, changed )
		== GameDataBindError::ClientAddressOverflow,
		"reject overflowing client address range" );

	Expect( std::strcmp( fntech3::server::GameDataBindErrorName(
		GameDataBindError::EntityStrideTooSmall ), "entity stride too small" ) == 0,
		"bind errors have stable diagnostic names" );
}

void TestQvmOffsetsAndArrayBounds()
{
	std::vector<unsigned char> segment( 8192, kCanary );
	void *pointer = nullptr;
	std::size_t remaining = 0;
	Expect( fntech3::server::ResolveQvmGameDataPointer( segment.data(),
		segment.size(), 16, pointer, remaining )
		&& pointer == segment.data() + 16 && remaining == segment.size() - 16,
		"QVM offset resolves without masking and preserves the available span" );

	const std::intptr_t invalidOffsets[] = { 0, -1, 8192, 8208,
		( std::numeric_limits<std::intptr_t>::max )() };
	for ( const std::intptr_t offset : invalidOffsets ) {
		pointer = segment.data();
		remaining = segment.size();
		Expect( !fntech3::server::ResolveQvmGameDataPointer( segment.data(),
			segment.size(), offset, pointer, remaining ),
			"reject null, negative, one-past, wrapped, and oversized QVM offsets" );
		Expect( !pointer && remaining == 0,
			"failed QVM resolution clears both outputs" );
	}
	Expect( !fntech3::server::ResolveQvmGameDataPointer( nullptr,
		segment.size(), 16, pointer, remaining ), "reject absent QVM memory" );
	Expect( !fntech3::server::ResolveQvmGameDataPointer(
		reinterpret_cast<void *>(
			( std::numeric_limits<std::uintptr_t>::max )() - 15 ),
		segment.size(), 16, pointer, remaining ),
		"reject overflowing QVM segment address range" );

	for ( const comGameId_t profile : { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE } ) {
		NativeGameDataBinding binding;
		binding.entityCount = 2;
		binding.entityStride = ServerGameDataView::EntityPrefixBytesForProfile( profile );
		binding.clientCount = 2;
		binding.clientStride = ServerGameDataView::PlayerStateBytesForProfile( profile );
		Expect( fntech3::server::ResolveQvmGameDataPointer( segment.data(),
			segment.size(), 16, binding.entityBase, binding.entitySpanBytes ),
			"resolve QVM entity records" );
		Expect( fntech3::server::ResolveQvmGameDataPointer( segment.data(),
			segment.size(), 2048, binding.clientBase, binding.clientSpanBytes ),
			"resolve QVM client records" );
		ServerGameDataView view;
		Expect( view.BindNative( profile, binding ) == GameDataBindError::None,
			"bind resolved QVM records for each profile" );
		GameEntityRef reference;
		Expect( view.EntityRefAt( 0, reference ), "retain QVM reference before bad rebind" );
		Expect( fntech3::server::ResolveQvmGameDataPointer( segment.data(),
			segment.size(), static_cast<std::intptr_t>( segment.size()
				- binding.clientStride ), binding.clientBase, binding.clientSpanBytes ),
			"a valid QVM offset can still leave an incomplete client array" );
		Expect( view.BindNative( profile, binding ) == GameDataBindError::ClientSpanTooSmall,
			"reject QVM client arrays extending outside the exact data segment" );
		Expect( !view.IsCurrent( reference ) && !view.IsBound(),
			"failed QVM record rebind invalidates the previous view" );
	}
	Expect( RangeIsCanary( segment, 0, segment.size() ),
		"QVM binding validation does not modify module memory" );
}

/* RTCW MP's SP_worldspawn copies the map's worldspawn spawnflags into the world
 * record's worldflags, which map_restart reads at +0x2a8 of retail's 1352-byte
 * record, just past the part r shares with SP. No other profile answers, even
 * where its own record reaches that far: SP's record has no such member, and
 * ET's map_restart never reads its own. */
void TestRTCWMPWorldflags()
{
	constexpr std::size_t stride = 1352, worldflags = 0x2a8;
	static_assert( offsetof( fntech3_rtcw_mp_sharedEntity_t, worldflags ) == worldflags );
	const std::size_t players = sizeof( fntech3_rtcw_sp_playerState_t );
	Storage storage( stride, players, 1024, 1 );
	const std::int32_t flags = 3;
	std::memcpy( storage.EntityBase() + ENTITYNUM_WORLD * stride + worldflags, &flags, sizeof( flags ) );
	const auto unchanged = storage.entities;
	ServerGameDataView view;
	GameEntityRef world;
	std::int32_t read = -1;
	Expect( view.BindNative( COM_GAME_RTCW_MP, storage.Binding() ) == GameDataBindError::None
		&& view.EntityRefAt( ENTITYNUM_WORLD, world ) && view.GetRTCWMPWorldflags( world, read )
		&& read == flags, "RTCW MP reads the world record's worldflags at byte 680" );
	view.Invalidate();
	read = -1;
	Expect( !view.GetRTCWMPWorldflags( world, read ) && read == -1,
		"an expired world reference reads no worldflags" );
	int others = 0;
	for ( int id = COM_GAME_QUAKE3; id < COM_GAME_COUNT; ++id ) {
		const auto profile = static_cast<comGameId_t>( id );
		const std::size_t prefix = ServerGameDataView::EntityPrefixBytesForProfile( profile );
		if ( profile == COM_GAME_RTCW_MP || prefix == 0 ) continue;
		const std::size_t player = ServerGameDataView::PlayerStateBytesForProfile( profile );
		Storage other( prefix > stride ? prefix : stride, player > players ? player : players, 1024, 1 );
		// A profile this plain binding cannot satisfy has no view to ask.
		if ( view.BindNative( profile, other.Binding() ) != GameDataBindError::None ) continue;
		++others;
		Expect( view.EntityRefAt( ENTITYNUM_WORLD, world ) && !view.GetRTCWMPWorldflags( world, read )
			&& read == -1, "no other profile reads RTCW MP's worldflags" );
		ExpectGuardsIntact( other, "refused worldflags reads preserve array guards" );
	}
	Expect( others >= 6, "Quake III, Quake Live, RTCW SP, ET and both Elite Force modes are asked" );
	Expect( storage.entities == unchanged, "reading worldflags changes no module memory" );
	ExpectGuardsIntact( storage, "worldflags reads preserve array guards" );
}

template<class Shared, class Player>
void TestRTCWRecords( comGameId_t profile, std::int32_t maxClients )
{
	Storage storage( sizeof( Shared ) + 20, sizeof( Player ) + 32 );
	ServerGameDataView view;
	Expect( view.BindNative( profile, storage.Binding() ) == GameDataBindError::None,
		"bind RTCW entity and player records with private tails" );
	Shared external{};
	external.s.number = 7;
	external.s.pos.trType = 7;
	external.s.pos.trBase[1] = 25;
	external.s.events[3] = 29;
	external.s.eventParms[3] = 153;
	external.s.animMovetype = 13;
	external.r.s.number = 912;
	external.r.eventTime = 4567;
	if constexpr ( std::is_same_v<Shared, fntech3_rtcw_mp_sharedEntity_t> ) external.worldflags = 0x76543210;
	std::memcpy( storage.EntityBase() + storage.entityStride, &external, sizeof( external ) );
	GameEntityRef reference;
	Expect( view.EntityRefAt( 1, reference ), "resolve RTCW entity reference" );
	fntech3_rtcw_entityState_t state{};
	Expect( view.ReadEntityState( reference, state ) &&
		std::memcmp( &state, &external.s, sizeof( state ) ) == 0, "read complete RTCW entity state" );
	state.effect3Time = 987654;
	auto before = storage.entities;
	Expect( view.WriteEntityState( reference, state ), "write RTCW entity state" );
	ExpectOnlyRangeChanged( before, storage.entities, kGuardBytes + storage.entityStride,
		sizeof( state ), "RTCW state writes preserve r and game-private data" );
	CheckFieldAccessors( view, reference );
	std::memcpy( &external, storage.EntityBase() + storage.entityStride, sizeof( external ) );
	Expect( external.r.linked == 1 && external.r.linkcount == 2 && external.r.contents == 5,
		"RTCW shared accessors use the linked flag at offset 576" );
	Expect( external.r.s.number == 912 && external.r.eventTime == 4567,
		"RTCW accessor writes preserve the embedded state and event time" );
	if constexpr ( std::is_same_v<Shared, fntech3_rtcw_mp_sharedEntity_t> )
		Expect( external.worldflags == 0x76543210, "MP worldflags are retained" );
	Expect( RangeIsCanary( storage.entities, kGuardBytes + storage.entityStride + sizeof( Shared ),
		kGuardBytes + 2 * storage.entityStride ), "RTCW entity private tail stays untouched" );

	Player player{};
	player.common.commandTime = 1234;
	player.common.ammo[63] = 4321;
	player.common.ammoclip[63] = 17;
	player.common.events[3] = 199;
	player.aiState = 3;
	if constexpr ( std::is_same_v<Player, fntech3_rtcw_sp_playerState_t> ) player.footstepCount = 1.5f;
	else player.identifyClient = 61;
	std::memcpy( storage.ClientBase() + storage.clientStride, &player, sizeof( player ) );
	Player copied{};
	Expect( view.ReadPlayerState( 1, copied ) && std::memcmp( &copied, &player, sizeof( player ) ) == 0,
		"read complete profile-specific RTCW player state" );
	before = storage.clients;
	Expect( view.SetPlayerPing( 1, 73 ), "write RTCW player ping" );
	ExpectOnlyRangeChanged( before, storage.clients,
		kGuardBytes + storage.clientStride + offsetof( Player, common.ping ), 4,
		"ping write preserves all other RTCW player and private bytes" );
	std::int32_t ping = 0;
	Expect( view.GetPlayerPing( 1, ping ) && ping == 73, "read RTCW player ping" );
	Expect( RangeIsCanary( storage.clients, kGuardBytes + storage.clientStride + sizeof( Player ),
		kGuardBytes + 2 * storage.clientStride ), "RTCW client private tail stays untouched" );
	using OtherPlayer = std::conditional_t<std::is_same_v<Player, fntech3_rtcw_sp_playerState_t>,
		fntech3_rtcw_mp_playerState_t, fntech3_rtcw_sp_playerState_t>;
	OtherPlayer wrongMode{};
	wrongMode.common.commandTime = 919;
	Expect( !view.ReadPlayerState( 1, wrongMode ) && wrongMode.common.commandTime == 919,
		"SP and MP player read overloads reject the other mode without writing" );
	entityState_t canonical{};
	playerState_t canonicalPlayer{};
	Expect( view.ReadEntityState( reference, canonical ) && canonical.rtcw.effect3Time == 987654 &&
		canonical.rtcw.animMovetype == 13 && canonical.health == 0,
		"RTCW entities use the complete canonical adapter" );
	canonical.number = 929;
	canonical.rtcw.events[3] = 241;
	before = storage.entities;
	Expect( view.WriteEntityState( reference, canonical ) && view.ReadEntityState( reference, state ) &&
		state.number == 929 && state.events[3] == 241 && state.effect3Time == 987654,
		"RTCW canonical writes preserve all event slots and extended fields" );
	ExpectOnlyRangeChanged( before, storage.entities, kGuardBytes + storage.entityStride,
		sizeof( state ), "canonical writes preserve shared and private RTCW data" );
	before = storage.entities;
	canonical.pos.trType = TR_QL_ACCEL;
	Expect( !view.WriteEntityState( reference, canonical ) && storage.entities == before,
		"RTCW write rejects a foreign trajectory without touching module state" );
	Expect( view.ReadPlayerState( 1, canonicalPlayer ) && canonicalPlayer.commandTime == 1234 &&
		canonicalPlayer.rtcw.ammo[47] == 4321 && canonicalPlayer.rtcw.ammoclip[63] == 17 &&
		canonicalPlayer.rtcw.events[1] == 199 && canonicalPlayer.rtcw.aiState == 3,
		"RTCW canonical player preserves high weapon slots and all four events" );
	Expect( !view.ReadPlayerState( -1, copied ) && !view.ReadPlayerState( 2, copied ),
		"RTCW player access checks both array bounds" );
	ExpectGuardsIntact( storage, "RTCW state access preserves record-array guards" );
	const auto unchangedEntities = storage.entities;
	const auto unchangedClients = storage.clients;
	view.Invalidate();
	Expect( !view.ReadEntityState( reference, state ) && !view.WriteEntityState( reference, state ) &&
		!view.ReadPlayerState( 0, copied ), "RTCW references expire on shutdown" );
	Expect( storage.entities == unchangedEntities && storage.clients == unchangedClients,
		"expired RTCW views do not touch module records" );

	Storage full( sizeof( Shared ), sizeof( Player ), 1, maxClients );
	Expect( view.BindNative( profile, full.Binding() ) == GameDataBindError::None,
		"bind the mode's complete client capacity" );
	Expect( view.ClientPointer( maxClients - 1 ) != nullptr && view.ClientPointer( maxClients ) == nullptr,
		"last RTCW client index obeys the mode's limit" );
	auto invalid = full.Binding();
	invalid.clientCount = maxClients + 1;
	Expect( view.BindNative( profile, invalid ) == GameDataBindError::InvalidClientCount,
		"reject counts beyond the SP/MP maximum" );
	invalid = full.Binding();
	invalid.entityStride = sizeof( Shared ) - 4;
	Expect( view.BindNative( profile, invalid ) == GameDataBindError::EntityStrideTooSmall,
		"reject a short RTCW shared-entity prefix" );
	invalid = full.Binding();
	invalid.clientStride = sizeof( Player ) - 4;
	Expect( view.BindNative( profile, invalid ) == GameDataBindError::ClientStrideTooSmall,
		"reject a short RTCW player record" );
	ExpectGuardsIntact( full, "invalid RTCW bindings preserve module memory" );
}

// Holomatch's shared entity is 204 bytes of state then 100 of r, with no
// leading state in r; its 468-byte player state carries four events.
// Jedi Academy's shared entity puts Raven's own ROFF flag and broadcast mask
// inside the engine's half and its ICARUS and navigation fields after it, so
// the engine's fields sit 560 bytes into an 864-byte record rather than at the
// end of one. Nothing converts its entity state yet, and the view says so
// rather than reading a Quake III state out of a Jedi Academy record.
void TestJKARecords()
{
	Expect( ServerGameDataView::EntityPrefixBytesForProfile( COM_GAME_JKA_MP ) == 864 &&
		ServerGameDataView::PlayerStateBytesForProfile( COM_GAME_JKA_MP ) == 1552,
		"Jedi Academy exposes its exact record prefixes" );
	Expect( ServerGameDataView::EntityPrefixBytesForProfile( COM_GAME_JKA_SP ) == 0,
		"Jedi Academy single player has no game-data layout yet" );

	Storage storage( sizeof( fntech3_jka_sharedEntity_t ) + 16,
		sizeof( fntech3_jka_playerState_t ) + 24 );
	ServerGameDataView view;
	Expect( view.BindNative( COM_GAME_JKA_MP, storage.Binding() ) == GameDataBindError::None,
		"bind Jedi Academy entity and player records with game-private tails" );
	Expect( view.EntityStride() == sizeof( fntech3_jka_sharedEntity_t ) + 16,
		"Jedi Academy keeps the stride its module published" );

	GameEntityRef reference;
	Expect( view.EntityRefAt( 1, reference ), "resolve a Jedi Academy entity reference" );
	CheckFieldAccessors( view, reference );

	fntech3_jka_sharedEntity_t external{};
	std::memcpy( &external, storage.EntityBase() + storage.entityStride, sizeof( external ) );
	Expect( external.r.linked == 1 && external.r.linkcount == 2 && external.r.contents == 5
		&& external.r.ownerNum == 6 && external.s.number == 41,
		"Jedi Academy shared accessors address r at offset 560" );
	// Raven's own shared fields still hold the fill this record was made with,
	// so nothing the engine writes has strayed into them.
	const std::uint32_t untouched = 0xa5a5a5a5u;
	Expect( external.r.mIsRoffing == static_cast<std::int32_t>( untouched )
		&& external.r.broadcastClients[0] == untouched
		&& external.r.broadcastClients[1] == untouched
		&& external.next_roff_time == static_cast<std::int32_t>( untouched ),
		"Raven's own shared fields are left to the game" );

	fntech3_jka_playerState_t player{};
	player.commandTime = 4321;
	player.clientNum = 9;
	std::memcpy( storage.ClientBase() + storage.clientStride, &player, sizeof( player ) );
	const auto before = storage.clients;
	Expect( view.SetPlayerPing( 1, 61 ), "write the Jedi Academy ping" );
	ExpectOnlyRangeChanged( before, storage.clients, kGuardBytes + storage.clientStride +
		offsetof( fntech3_jka_playerState_t, ping ), 4, "ping lands at offset 524" );

	// Every one of Academy's ninety-four entity fields is carried, and the
	// accounting is checked rather than claimed: a record with a distinct
	// value in every byte must come back byte for byte after a trip through
	// the engine's own state. A field left unmapped would come back zero.
	fntech3_jka_entityState_t filled{};
	unsigned char *fill = reinterpret_cast<unsigned char *>( &filled );
	for ( std::size_t at = 0; at < sizeof( filled ); ++at ) {
		fill[at] = static_cast<unsigned char>( 0x11 + ( at * 7 ) % 0xd7 );
	}
	filled.pos.trType = 6;     // Academy's gravity, which is Quake III's five
	filled.apos.trType = 4;    // Academy's own eased stop, which Quake III lacks
	filled.torsoAnim = 21;
	filled.saberMove = 33;
	filled.boneAngles4[2] = 45.0f;
	filled.userVec2[0] = -1.5f;
	std::memcpy( storage.EntityBase() + storage.entityStride, &filled, sizeof( filled ) );

	entityState_t state{};
	Expect( view.ReadEntityState( reference, state ),
		"read a Jedi Academy entity state through its adapter" );
	Expect( state.pos.trType == TR_GRAVITY && state.apos.trType == TR_JKA_NONLINEAR_STOP,
		"Academy's renumbered trajectory types arrive with their own meanings" );
	Expect( state.number == filled.number && state.torsoAnim == 21
		&& state.raven.saberMove == 33 && state.raven.boneAngles4[2] == 45.0f
		&& state.raven.userVec2[0] == -1.5f,
		"Quake III's fields and Academy's own each land where they belong" );

	auto entitiesBefore = storage.entities;
	Expect( view.WriteEntityState( reference, state ),
		"write the Jedi Academy entity state" );
	ExpectOnlyRangeChanged( entitiesBefore, storage.entities, kGuardBytes + storage.entityStride,
		sizeof( fntech3_jka_entityState_t ),
		"Academy state writes preserve r and game-private data" );
	fntech3_jka_entityState_t returned{};
	std::memcpy( &returned, storage.EntityBase() + storage.entityStride, sizeof( returned ) );
	Expect( std::memcmp( &returned, &filled, sizeof( filled ) ) == 0,
		"all ninety-four Academy entity fields survive the round trip" );

	// A trajectory this title does not have is refused in both directions
	// rather than converted into a meaning Academy would not mean.
	entitiesBefore = storage.entities;
	state.pos.trType = TR_QL_ACCEL;
	Expect( !view.WriteEntityState( reference, state ) && storage.entities == entitiesBefore,
		"Academy refuses a trajectory type it does not have" );
	state.pos.trType = TR_GRAVITY;
	state.pos.gravity = 2.0f;
	Expect( !view.WriteEntityState( reference, state ) && storage.entities == entitiesBefore,
		"Academy refuses a trajectory acceleration it cannot carry" );
	filled.pos.trType = 7;
	std::memcpy( storage.EntityBase() + storage.entityStride, &filled, sizeof( filled ) );
	Expect( !view.ReadEntityState( reference, state ),
		"a trajectory type past Academy's seven is refused, not guessed" );

	// The player state is accounted for the same way as the entity state, and
	// it is the larger record: 168 fields, of which 43 are Quake III's and the
	// rest are Raven's. A record with a distinct value in every one of its
	// 1,552 bytes must come back byte for byte, so a field left unmapped comes
	// back zero and fails here.
	fntech3_jka_playerState_t filledPlayer{};
	{
		unsigned char *bytes = reinterpret_cast<unsigned char *>( &filledPlayer );
		for ( std::size_t at = 0; at < sizeof( filledPlayer ); ++at ) {
			bytes[at] = static_cast<unsigned char>( 0x23 + ( at * 11 ) % 0xc9 );
		}
	}
	filledPlayer.clientNum = 5;
	filledPlayer.saberMove = 17;
	filledPlayer.ammo[18] = 99;          /* the nineteenth slot, past Quake III's */
	std::memcpy( storage.ClientBase() + storage.clientStride, &filledPlayer,
		sizeof( filledPlayer ) );

	playerState_t canonical{};
	Expect( view.ReadPlayerState( 1, canonical ),
		"read a Jedi Academy player state through its adapter" );
	Expect( canonical.clientNum == 5 && canonical.raven.saberMove == 17
		&& canonical.raven.ammoExtra[2] == 99,
		"Quake III's fields and Raven's each land where they belong" );

	Expect( view.SetPlayerPing( 1, 61 ), "write the Jedi Academy ping again" );
	fntech3_jka_playerState_t returnedPlayer{};
	std::memcpy( &returnedPlayer, storage.ClientBase() + storage.clientStride,
		sizeof( returnedPlayer ) );
	returnedPlayer.ping = filledPlayer.ping;   /* the one field just written */
	Expect( std::memcmp( &returnedPlayer, &filledPlayer, sizeof( filledPlayer ) ) == 0,
		"nothing but the ping is disturbed by writing it" );

	ExpectGuardsIntact( storage, "Jedi Academy access preserves record-array guards" );
}

void TestSTVEFRecords()
{
	Expect( ServerGameDataView::EntityPrefixBytesForProfile( COM_GAME_STVEF_MP ) == 304 &&
		ServerGameDataView::PlayerStateBytesForProfile( COM_GAME_STVEF_MP ) == 468,
		"Holomatch exposes its exact record prefixes" );
	Storage storage( sizeof( fntech3_stvef_mp_sharedEntity_t ) + 12,
		sizeof( fntech3_stvef_mp_playerState_t ) + 20 );
	ServerGameDataView view;
	Expect( view.BindNative( COM_GAME_STVEF_MP, storage.Binding() ) == GameDataBindError::None,
		"bind Holomatch entity and player records with private tails" );
	fntech3_stvef_mp_sharedEntity_t external{};
	external.s.number = 7;
	external.s.pos.trType = 5;
	external.s.pos.trBase[2] = 64.0f;
	external.s.solid = 0x102030;
	external.s.torsoAnim = 21;
	std::memcpy( storage.EntityBase() + storage.entityStride, &external, sizeof( external ) );
	GameEntityRef reference;
	Expect( view.EntityRefAt( 1, reference ), "resolve Holomatch entity reference" );
	entityState_t state{};
	Expect( view.ReadEntityState( reference, state ) && state.number == 7 &&
		state.pos.trType == TR_GRAVITY && state.pos.trBase[2] == 64.0f &&
		state.solid == 0x102030 && state.torsoAnim == 21 && state.generic1 == 0,
		"read the Holomatch entity state through its adapter" );
	CheckFieldAccessors( view, reference );
	std::memcpy( &external, storage.EntityBase() + storage.entityStride, sizeof( external ) );
	Expect( external.r.linked == 1 && external.r.linkcount == 2 && external.r.contents == 5,
		"Holomatch shared accessors address r at offset 204" );
	auto before = storage.entities;
	state.eFlags = 0x400000;
	Expect( view.WriteEntityState( reference, state ), "write the Holomatch entity state" );
	ExpectOnlyRangeChanged( before, storage.entities, kGuardBytes + storage.entityStride,
		sizeof( fntech3_stvef_mp_entityState_t ), "state writes preserve r and game-private data" );
	before = storage.entities;
	state.pos.trType = TR_QL_ACCEL;
	Expect( !view.WriteEntityState( reference, state ) && storage.entities == before,
		"Holomatch rejects a trajectory type it does not have" );

	fntech3_stvef_mp_playerState_t player{};
	player.commandTime = 4321;
	player.events[3] = 44;
	player.eventParms[2] = 7;
	player.damageShieldCount = 9;
	player.introTime = 1500;
	player.ammo[15] = 99;
	std::memcpy( storage.ClientBase() + storage.clientStride, &player, sizeof( player ) );
	playerState_t canonical{};
	Expect( view.ReadPlayerState( 1, canonical ) && canonical.commandTime == 4321 &&
		canonical.stvef.events[1] == 44 && canonical.stvef.eventParms[0] == 7 &&
		canonical.stvef.damageShieldCount == 9 && canonical.stvef.introTime == 1500 &&
		canonical.ammo[15] == 99, "read the Holomatch player state with its extension" );
	before = storage.clients;
	Expect( view.SetPlayerPing( 1, 61 ), "write the Holomatch ping" );
	ExpectOnlyRangeChanged( before, storage.clients, kGuardBytes + storage.clientStride +
		offsetof( fntech3_stvef_mp_playerState_t, ping ), 4, "ping lands at offset 460" );
	ExpectGuardsIntact( storage, "Holomatch access preserves record-array guards" );

	Storage full( sizeof( fntech3_stvef_mp_sharedEntity_t ), sizeof( fntech3_stvef_mp_playerState_t ), 1, 64 );
	Expect( view.BindNative( COM_GAME_STVEF_MP, full.Binding() ) == GameDataBindError::None,
		"bind 64 Holomatch clients" );
	auto invalid = full.Binding();
	invalid.clientCount = 65;
	Expect( view.BindNative( COM_GAME_STVEF_MP, invalid ) == GameDataBindError::InvalidClientCount,
		"reject more clients than the Holomatch game holds" );
	// Single player's gentity is longer than Holomatch's shared entity.
	Expect( view.BindNative( COM_GAME_STVEF_SP, full.Binding() ) == GameDataBindError::EntityStrideTooSmall,
		"single player refuses Holomatch's records" );
}

/* American McGee's Alice's record has no declaration anywhere: its layout is
 * read out of the retail engine's own delta table and its own entity code.
 * What is checked here is that the view addresses it where those readings
 * say, and -- the part that matters most -- that the forty-nine members this
 * engine has nowhere to keep survive a round trip through it untouched. */
void TestAliceRecords()
{
	Expect( ServerGameDataView::EntityPrefixBytesForProfile( COM_GAME_ALICE )
			== FNTECH3_ALICE_GENTITY_SIZE &&
		ServerGameDataView::PlayerStateBytesForProfile( COM_GAME_ALICE )
			== FNTECH3_ALICE_PLAYER_STATE_BYTES,
		"Alice exposes its 732-byte entity and 1,048-byte player prefixes" );

	Storage storage( FNTECH3_ALICE_GENTITY_SIZE, FNTECH3_ALICE_GCLIENT_SIZE );
	ServerGameDataView view;
	auto binding = storage.Binding();
	Expect( view.BindNative( COM_GAME_ALICE, binding ) == GameDataBindError::None,
		"bind Alice's entity and client arrays" );

	/* The count Alice publishes is a high-water mark, so a capacity larger
	 * than it is accepted where other profiles require the two to agree. */
	Storage arena( FNTECH3_ALICE_GENTITY_SIZE, FNTECH3_ALICE_GCLIENT_SIZE, 64, 8 );
	auto grown = arena.Binding();
	grown.entityCount = 8;
	Expect( view.BindNative( COM_GAME_ALICE, grown ) == GameDataBindError::None,
		"Alice binds an arena larger than the count in use" );
	grown.entityCapacity = 65;
	Expect( view.BindNative( COM_GAME_ALICE, grown ) == GameDataBindError::EntitySpanTooSmall,
		"Alice refuses a capacity its span does not cover" );
	Expect( view.BindNative( COM_GAME_ALICE, binding ) == GameDataBindError::None,
		"rebind the two-entity arena" );

	unsigned char *record = storage.EntityBase() + storage.entityStride;
	fntech3_alice_entityState_t external{};
	external.number = 7;
	external.eType = 3;
	external.pos.trType = 5;
	external.pos.trBase[2] = 64.0f;
	external.netorigin[1] = 128.0f;
	external.netangles[0] = 90.0f;
	external.origin2[2] = 12.0f;
	external.modelindex = 31;
	external.clientNum = 2;
	external.frame = 11;
	external.groundEntityNum = 1022;
	external.constantLight = 0x01020304;
	external.loopSound = 9;
	external.solid = 0xffffff;
	/* Ritual's own, which this engine has no member for. */
	external.skinNum = 4;
	external.anim = 77;
	external.torso_anim = 88;
	external.parent = 13;
	external.tag_num = 5;
	external.scale = 2.5f;
	external.alpha = 0.5f;
	external.renderfx = 0x80;
	external.renderfx2 = 0x40;
	external.bonemod[3].tag = 6;
	external.bonemod[3].scale = 1.5f;
	external.loopSoundMinDist = 30000.0f;
	std::memcpy( record, &external, sizeof( external ) );

	GameEntityRef reference;
	Expect( view.EntityRefAt( 1, reference ), "resolve an Alice entity reference" );
	entityState_t state{};
	Expect( view.ReadEntityState( reference, state ) && state.number == 7 &&
		state.eType == 3 && state.pos.trType == TR_GRAVITY &&
		state.pos.trBase[2] == 64.0f && state.origin[1] == 128.0f &&
		state.angles[0] == 90.0f && state.origin2[2] == 12.0f &&
		state.modelindex == 31 && state.clientNum == 2 && state.frame == 11 &&
		state.groundEntityNum == 1022 && state.constantLight == 0x01020304 &&
		state.loopSound == 9 && state.solid == 0xffffff,
		"read the thirty Alice members that have a counterpart" );
	/* Members Alice does not have must not arrive holding anything. */
	Expect( state.weapon == 0 && state.legsAnim == 0 && state.torsoAnim == 0 &&
		state.event == 0 && state.eventParm == 0 && state.powerups == 0 &&
		state.otherEntityNum == 0 && state.generic1 == 0 &&
		state.modelindex2 == 0 && state.time == 0 && state.time2 == 0,
		"members Alice has no field for read as zero" );

	CheckFieldAccessors( view, reference, false );
	std::memcpy( &external, record, sizeof( external ) );
	Expect( external.origin2[0] == 13.0f && external.origin2[1] == 14.0f &&
		external.origin2[2] == 15.0f,
		"the portal origin the view writes is the delta table's origin2 at 108" );

	/* A link count and a single-client member are absent from the record, so
	 * the view keeps the first itself and answers zero for the second --
	 * without touching the module's bytes for either. */
	auto before = storage.entities;
	Expect( view.SetLinkcount( reference, 9 ), "Alice link count is the view's own" );
	std::int32_t scalar = 0;
	Expect( view.GetLinkcount( reference, scalar ) && scalar == 9,
		"the view reads back the link count it kept" );
	Expect( view.SetSingleClient( reference, 3 ), "Alice has no single-client member" );
	Expect( view.GetSingleClient( reference, scalar ) && scalar == 0,
		"an absent single-client member answers zero" );
	Expect( before == storage.entities,
		"neither absent member writes into the module's record" );

	/* The round trip that matters: change one member this engine owns and
	 * every Ritual member must come back exactly as it was. */
	state.eFlags = 0x4000;
	before = storage.entities;
	Expect( view.WriteEntityState( reference, state ), "write the Alice entity state" );
	fntech3_alice_entityState_t after{};
	std::memcpy( &after, record, sizeof( after ) );
	Expect( after.eFlags == 0x4000, "the changed member lands" );
	Expect( after.skinNum == 4 && after.anim == 77 && after.torso_anim == 88 &&
		after.parent == 13 && after.tag_num == 5 && after.scale == 2.5f &&
		after.alpha == 0.5f && after.renderfx == 0x80 && after.renderfx2 == 0x40 &&
		after.bonemod[3].tag == 6 && after.bonemod[3].scale == 1.5f &&
		after.loopSoundMinDist == 30000.0f,
		"every Ritual member survives a write untouched" );
	ExpectOnlyRangeChanged( before, storage.entities,
		kGuardBytes + storage.entityStride,
		FNTECH3_ALICE_ENTITY_STATE_BYTES,
		"a state write stays inside the entity state" );

	/* The ping is the first word past that half. */
	before = storage.clients;
	Expect( view.SetPlayerPing( 1, 57 ), "write an Alice ping" );
	ExpectOnlyRangeChanged( before, storage.clients,
		kGuardBytes + storage.clientStride + FNTECH3_ALICE_PS_PING, 4,
		"the ping lands at 1,044, where the retail engine writes it" );
	ExpectGuardsIntact( storage, "Alice access preserves record-array guards" );

	/* The player state crosses too: twenty-two of its forty-nine networked
	 * members are this engine's own and the rest are carried, so a read
	 * brings the whole record over rather than only the part that happens
	 * to have a counterpart. */
	fntech3_alice_playerState_t player{};
	player.commandTime = 4321;
	player.origin[1] = 64.0f;
	player.viewheight = -8;
	player.clientNum = 1;
	player.reverb_type = 17;
	player.camera_flags = 9;
	player.blend[2] = 0.25f;
	player.block[4][11] = 777;
	player.ping = 61;
	std::memcpy( storage.ClientBase() + storage.clientStride, &player,
		sizeof( player ) );
	playerState_t canonical{};
	Expect( view.ReadPlayerState( 1, canonical ) &&
		canonical.commandTime == 4321 && canonical.origin[1] == 64.0f &&
		canonical.viewheight == -8 && canonical.clientNum == 1,
		"the mapped part of an Alice player state crosses" );
	Expect( canonical.alice.reverb_type == 17 &&
		canonical.alice.camera_flags == 9 &&
		canonical.alice.blend[2] == 0.25f &&
		canonical.alice.block[4][11] == 777,
		"and the part this engine has no member for is carried" );
	Expect( canonical.ping == 61, "the ping crosses with it" );
}

void TestRTCWSaveEntityCapacity()
{
	Storage storage( sizeof(fntech3_rtcw_sp_sharedEntity_t) + 16,
		sizeof(fntech3_rtcw_sp_playerState_t), 1024, 1 );
	ServerGameDataView view;
	auto binding = storage.Binding(); binding.entityCount = 128; binding.entityCapacity = 1024;
	Expect( view.BindNative(COM_GAME_RTCW_SP, binding) == GameDataBindError::None,
		"SP binds the full save-restore entity arena" );
	Expect( view.EntityCount() == 128, "capacity does not change the active entity count" );
	GameEntityRef ref;
	Expect( view.EntityRefAt(1023, ref) && view.SetNumber(ref, 1023),
		"SP restores entities beyond the initial count" );
	Expect( view.EntityRefForPointer(storage.EntityBase() + 1023 * storage.entityStride, ref),
		"SP links the last restored entity by checked pointer" );
	Expect( !view.EntityRefAt(1024, ref) && !view.EntityRefForPointer(
		storage.EntityBase() + 1024 * storage.entityStride, ref), "SP still rejects the arena endpoint" );
	Expect( !view.EntityRefForPointer(storage.EntityBase() + 128 * storage.entityStride + 1, ref),
		"SP still rejects misaligned restored handles" );
	for ( int capacity : {-1, 127, 1025} ) {
		binding.entityCapacity = capacity;
		Expect( view.BindNative(COM_GAME_RTCW_SP, binding) == GameDataBindError::InvalidEntityCount,
			"SP rejects an invalid capacity" );
	}
	binding.entityCapacity = 1024; --binding.entitySpanBytes;
	Expect( view.BindNative(COM_GAME_RTCW_SP, binding) == GameDataBindError::EntitySpanTooSmall,
		"capacity requires a complete native/QVM memory span" );
	++binding.entitySpanBytes;
	for ( auto profile : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_STVEF_MP,
		COM_GAME_JK2_MP, COM_GAME_JKA_MP} )
		Expect( view.BindNative(profile, binding) == GameDataBindError::InvalidEntityCount,
			"other profiles do not broaden their entity contracts" );
	ExpectGuardsIntact(storage, "SP save capacity preserves array guards");
}

/* RTCW MP's and ET's modules own a fixed 1024-record entity arena too and
 * publish only the count in use, which starts at 64. Dynamite that stops
 * moving takes the world, ENTITYNUM_WORLD, as its owner and traces with it as
 * the pass entity, so the server reads the world's record past that count.
 * The strides are the retail modules' own: 1352 bytes in RTCW MP's
 * qagame_mp_x86.dll and 1404 in ET 2.60b's. */
template<typename Entity, typename Player>
void TestWorldEntityArena( comGameId_t profile, std::size_t stride, const char *name )
{
	Storage storage( stride, sizeof(Player), 1024, 1 );
	Entity world{};
	world.s.number = ENTITYNUM_WORLD;
	std::memcpy( storage.EntityBase() + ENTITYNUM_WORLD * stride, &world, sizeof(world) );
	const auto check = [name]( bool condition, const char *message ) {
		if ( !condition ) std::cerr << name << ": ";
		Expect( condition, message );
	};
	ServerGameDataView view;
	auto binding = storage.Binding(); binding.entityCount = 64; binding.entityCapacity = 1024;
	check( view.BindNative(profile, binding) == GameDataBindError::None,
		"binds the module's whole entity arena" );
	check( view.EntityCount() == 64, "the arena leaves the active entity count alone" );
	GameEntityRef ref;
	std::int32_t number = -1, owner = -1;
	// Retail SP_worldspawn never sets the world's owner, so it keeps the zero
	// G_InitGame cleared the arena to; the server must read it, not assume it.
	check( view.EntityRefAt(ENTITYNUM_WORLD, ref) && view.GetNumber(ref, number)
		&& number == ENTITYNUM_WORLD && view.GetOwnerNum(ref, owner) && owner == 0,
		"the world's record past the published count is readable" );
	check( view.EntityRefForPointer(storage.EntityBase() + ENTITYNUM_WORLD * stride, ref)
		&& ref.index == ENTITYNUM_WORLD, "the world's record resolves by checked pointer" );
	check( !view.EntityRefAt(1024, ref) && !view.EntityRefForPointer(
		storage.EntityBase() + 1024 * stride, ref), "the arena endpoint is still rejected" );
	binding.entityCapacity = 1025;
	check( view.BindNative(profile, binding) == GameDataBindError::InvalidEntityCount,
		"a capacity past MAX_GENTITIES is rejected" );
	binding.entityCapacity = 1024; --binding.entitySpanBytes;
	check( view.BindNative(profile, binding) == GameDataBindError::EntitySpanTooSmall,
		"the arena requires a complete native/QVM memory span" );
	ExpectGuardsIntact( storage, "the world arena preserves array guards" );
}

} // namespace

int main()
{
	Expect( ServerGameDataView::EntityPrefixBytesForProfile( COM_GAME_QUAKE3 )
		== 516, "Q3 profile exposes exact shared-entity prefix" );
	Expect( ServerGameDataView::EntityPrefixBytesForProfile( COM_GAME_QUAKELIVE )
		== 572, "QL profile exposes exact shared-entity prefix" );
	Expect( ServerGameDataView::PlayerStateBytesForProfile( COM_GAME_QUAKE3 )
		== 468, "Q3 profile exposes exact player-state prefix" );
	Expect( ServerGameDataView::PlayerStateBytesForProfile( COM_GAME_QUAKELIVE )
		== 504, "QL profile exposes exact player-state prefix" );
	Expect( ServerGameDataView::EntityPrefixBytesForProfile( COM_GAME_AUTO ) == 0,
		"unsupported profile has no entity layout" );

	TestQ3ExactEntityRecord();
	TestQlExactEntityRecord();
	TestQ3PlayerPingAndPrivateTail();
	TestQlPlayerPingAndPrivateTail();
	TestPointerConversionsAndGenerations();
	TestBindingValidation();
	TestQvmOffsetsAndArrayBounds();
	TestAliceRecords();
	TestRTCWSaveEntityCapacity();
	TestWorldEntityArena<fntech3_rtcw_mp_sharedEntity_t, fntech3_rtcw_mp_playerState_t>(
		COM_GAME_RTCW_MP, 1352, "RTCW MP" );
	TestWorldEntityArena<fntech3_wolfet_sharedEntity_t, fntech3_wolfet_playerState_t>(
		COM_GAME_WOLFET, 1404, "ET" );
	TestRTCWRecords<fntech3_rtcw_sp_sharedEntity_t, fntech3_rtcw_sp_playerState_t>( COM_GAME_RTCW_SP, 128 );
	TestRTCWRecords<fntech3_rtcw_mp_sharedEntity_t, fntech3_rtcw_mp_playerState_t>( COM_GAME_RTCW_MP, 64 );
	TestRTCWMPWorldflags();
	TestSTVEFRecords();
	TestJKARecords();

	if ( failures != 0 ) {
		std::cerr << failures << " server game-data checks failed\n";
		return 1;
	}
	return 0;
}
