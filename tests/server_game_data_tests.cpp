/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "../code/server/server_game_data.h"

#include "../code/compat/q3_game_abi.h"
#include "../code/compat/ql_game_abi.h"

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

void CheckFieldAccessors( ServerGameDataView &view,
	const GameEntityRef &reference )
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
	Expect( view.GetSingleClient( reference, scalar ) && scalar == 4,
		"get singleClient" );
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
	TestRTCWRecords<fntech3_rtcw_sp_sharedEntity_t, fntech3_rtcw_sp_playerState_t>( COM_GAME_RTCW_SP, 128 );
	TestRTCWRecords<fntech3_rtcw_mp_sharedEntity_t, fntech3_rtcw_mp_playerState_t>( COM_GAME_RTCW_MP, 64 );

	if ( failures != 0 ) {
		std::cerr << failures << " server game-data checks failed\n";
		return 1;
	}
	return 0;
}
