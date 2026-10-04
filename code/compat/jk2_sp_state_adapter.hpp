/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JK2_SP_STATE_ADAPTER_HPP
#define FNTECH3_COMPAT_JK2_SP_STATE_ADAPTER_HPP
#include "jk2_sp_game_abi.h"
#include "rtcw_state_adapter.hpp"
#include <cstdint>
#include <cstring>

namespace fntech3::jk2::sp::state {
namespace detail {
using rtcw::state::detail::Field;
/* Independently measured from the retail SP module's EvaluateTrajectory at
 * 0x2000ccc0 and its seven-way table at 0x2000ce5c. Type 4 follows Raven's
 * nonlinear stop, 5 the two-pi sine and 6 gravity; none is QL acceleration. */
inline constexpr trType_t TrajectoryTypes[] = {
	TR_STATIONARY, TR_INTERPOLATE, TR_LINEAR, TR_LINEAR_STOP,
	TR_JKA_NONLINEAR_STOP, TR_SINE, TR_GRAVITY
};
template<bool Out, class External, class Internal>
bool Trajectory( External &external, Internal &internal ) noexcept {
	if constexpr ( Out ) {
		if ( internal.gravity != 0.0f ) return false;
		int index = 0;
		while ( index < 7 && internal.trType != TrajectoryTypes[index] ) ++index;
		if ( index == 7 ) return false;
		external.trType = index;
	} else {
		if ( external.trType < 0 || external.trType >= 7 ) return false;
		internal.trType = TrajectoryTypes[external.trType]; internal.gravity = 0.0f;
	}
	Field<Out>( external.trTime, internal.trTime );
	Field<Out>( external.trDuration, internal.trDuration );
	Field<Out>( external.trBase, internal.trBase );
	Field<Out>( external.trDelta, internal.trDelta );
	return true;
}
template<bool Out, class External, class Internal>
bool Entity( External &external, Internal &internal ) noexcept {
	if ( !Trajectory<Out>( external.pos, internal.pos ) || !Trajectory<Out>( external.apos, internal.apos ) ) return false;
#define JK2_SP_COMMON(member) Field<Out>( external.member, internal.member )
	JK2_SP_COMMON( number ); JK2_SP_COMMON( eType ); JK2_SP_COMMON( eFlags );
	JK2_SP_COMMON( time ); JK2_SP_COMMON( time2 );
	JK2_SP_COMMON( origin ); JK2_SP_COMMON( origin2 ); JK2_SP_COMMON( angles ); JK2_SP_COMMON( angles2 );
	JK2_SP_COMMON( otherEntityNum ); JK2_SP_COMMON( otherEntityNum2 ); JK2_SP_COMMON( groundEntityNum );
	JK2_SP_COMMON( constantLight ); JK2_SP_COMMON( loopSound );
	JK2_SP_COMMON( modelindex ); JK2_SP_COMMON( modelindex2 ); JK2_SP_COMMON( clientNum ); JK2_SP_COMMON( frame );
	JK2_SP_COMMON( solid ); JK2_SP_COMMON( event ); JK2_SP_COMMON( eventParm );
	JK2_SP_COMMON( powerups ); JK2_SP_COMMON( weapon );
	JK2_SP_COMMON( legsAnim ); JK2_SP_COMMON( torsoAnim );
#undef JK2_SP_COMMON
	Field<Out>( external.saberInFlight, internal.raven.saberInFlight );
#define JK2_SP_EXTRA(member) Field<Out>( external.member, internal.raven.sp.member )
	JK2_SP_EXTRA( modelindex3 ); JK2_SP_EXTRA( legsAnimTimer ); JK2_SP_EXTRA( torsoAnimTimer ); JK2_SP_EXTRA( scale );
	JK2_SP_EXTRA( saberActive ); JK2_SP_EXTRA( vehicleModel ); JK2_SP_EXTRA( modelScale );
	JK2_SP_EXTRA( radius ); JK2_SP_EXTRA( boltInfo );
#undef JK2_SP_EXTRA
	return true;
}
}
inline bool ToInternal( const fntech3_jk2_sp_entityState_t &source, entityState_t &destination ) noexcept {
	entityState_t temporary{};
	if ( !detail::Entity<false>( source, temporary ) ) return false;
	destination = temporary; return true;
}
inline bool FromInternal( const entityState_t &source, fntech3_jk2_sp_entityState_t &destination ) noexcept {
	fntech3_jk2_sp_entityState_t temporary{};
	if ( !detail::Entity<true>( temporary, source ) ) return false;
	destination = temporary; return true;
}

/* Resolve a fixed-width native owner address by arithmetic against a checked
 * published array. Never dereference the owner to discover its number. A
 * caller must preserve a foreign raw owner on entity write-back, or refuse
 * the operation, rather than silently treating that raw address as an index. */
inline bool OwnerIndex( std::uint32_t owner, std::uint32_t base, std::size_t stride,
	int capacity, int &index ) noexcept {
	if ( !base || stride < FNTECH3_JK2_SP_GENTITY_PREFIX_BYTES ||
		capacity < 1 || capacity > FNTECH3_JK2_SP_MAX_GENTITIES ) return false;
	const std::uint64_t span = std::uint64_t( stride ) * std::uint64_t( capacity );
	if ( stride > UINT32_MAX || std::uint64_t( base ) + span > UINT64_C(0x100000000) ) return false;
	if ( !owner ) { index = FNTECH3_JK2_SP_MAX_GENTITIES - 1; return true; }
	if ( owner < base ) return false;
	const std::uint64_t delta = std::uint64_t( owner ) - base;
	if ( delta >= span || delta % stride ) return false;
	index = static_cast<int>( delta / stride ); return true;
}
}
#endif
