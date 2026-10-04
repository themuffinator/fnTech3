/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q_shared.h"
#include "../code/compat/jk2_sp_state_adapter.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
using namespace fntech3::jk2::sp::state;
#define FNTECH3_JK2_SP_ENTITY_FIELD(member, offset, bits) \
	static_assert( offsetof( fntech3_jk2_sp_entityState_t, member ) == offset );
#include "../code/compat/jk2_sp_wire_layout.def"
#undef FNTECH3_JK2_SP_ENTITY_FIELD
int main() {
	fntech3_jk2_sp_entityState_t external{};
	// Exercise every measured four-byte field, including all SP extensions.
	auto *bytes = reinterpret_cast<unsigned char *>( &external );
	for ( std::size_t n = 0; n < sizeof( external ); ++n ) bytes[n] = static_cast<unsigned char>( n * 7 + 3 );
	for ( int type = 0; type < 7; ++type ) {
		external.pos.trType = type; external.apos.trType = ( type + 3 ) % 7;
		entityState_t internal{};
		assert( ToInternal( external, internal ) );
		assert( internal.raven.sp.vehicleModel == external.vehicleModel );
		assert( internal.raven.sp.boltInfo == external.boltInfo );
		assert( internal.raven.sp.modelindex3 == external.modelindex3 );
		assert( internal.raven.sp.vehicleArmor == 0 );
		assert( internal.raven.sp.vehicleAngles[0] == 0 );
		assert( internal.raven.modelGhoul2 == 0 && internal.stvef.modelindex3 == 0 );
		fntech3_jk2_sp_entityState_t restored{};
		assert( FromInternal( internal, restored ) );
		assert( !std::memcmp( &external, &restored, sizeof( external ) ) );
		if ( type == 4 ) assert( internal.pos.trType == TR_JKA_NONLINEAR_STOP );
		if ( type == 5 ) assert( internal.pos.trType == TR_SINE );
		if ( type == 6 ) assert( internal.pos.trType == TR_GRAVITY );
		// Foreign canonical trajectory contracts fail without a partial write.
		fntech3_jk2_sp_entityState_t untouched = restored;
		internal.pos.trType = TR_QL_ACCEL;
		assert( !FromInternal( internal, restored ) && !std::memcmp( &restored, &untouched, sizeof( restored ) ) );
		internal.pos.trType = TR_LINEAR; internal.pos.gravity = 800;
		assert( !FromInternal( internal, restored ) );
	}
	entityState_t destination{}; destination.number = 37;
	external.pos.trType = -1;
	assert( !ToInternal( external, destination ) && destination.number == 37 );
	external.pos.trType = 7;
	assert( !ToInternal( external, destination ) && destination.number == 37 );
	external.pos.trType = 0; external.apos.trType = 7;
	assert( !ToInternal( external, destination ) && destination.number == 37 );

	int index = -1;
	constexpr std::uint32_t base = 0x10000000, stride = 1288;
	assert( OwnerIndex( 0, base, stride, 1024, index ) && index == 1023 );
	assert( OwnerIndex( base + 5 * stride, base, stride, 1024, index ) && index == 5 );
	assert( OwnerIndex( base + 900 * stride, base, stride, 1024, index ) && index == 900 );
	index = 29;
	for ( auto bad : { base - 1, base + 1, base + 1024 * stride, UINT32_MAX } )
		assert( !OwnerIndex( bad, base, stride, 1024, index ) && index == 29 );
	assert( !OwnerIndex( base, base, 351, 1024, index ) );
	assert( !OwnerIndex( base, base, stride, -1, index ) );
	assert( !OwnerIndex( base, base, stride, 1025, index ) );
	assert( !OwnerIndex( UINT32_MAX, 0xfffff000, stride, 1024, index ) );
	assert( !OwnerIndex( base, 0, stride, 1024, index ) );
}
