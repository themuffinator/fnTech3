/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_STATE_ADAPTER_HPP
#define FNTECH3_COMPAT_JKA_SP_STATE_ADAPTER_HPP
#include "jka_sp_game_abi.h"
#include "jka_state_adapter.hpp"

/* Whole SP entity-state transfers. Raven's SP and MP trajectory enumerations
 * have the same seven entries in the released sources; their entity records
 * differ completely and never share a cast. Every SP byte has named storage. */
namespace fntech3::jka::sp::state {
namespace detail {
template<bool Out, class External, class Internal>
bool Entity(External &external, Internal &internal) noexcept {
	using rtcw::state::detail::Field;
	if (!jka::state::detail::Trajectory<Out>(external.pos, internal.pos)
		|| !jka::state::detail::Trajectory<Out>(external.apos, internal.apos)) return false;
#define JKA_SP_SHARED(name) Field<Out>(external.name, internal.name)
	JKA_SP_SHARED(number); JKA_SP_SHARED(eType); JKA_SP_SHARED(eFlags);
	JKA_SP_SHARED(time); JKA_SP_SHARED(time2); JKA_SP_SHARED(origin); JKA_SP_SHARED(origin2);
	JKA_SP_SHARED(angles); JKA_SP_SHARED(angles2); JKA_SP_SHARED(otherEntityNum);
	JKA_SP_SHARED(otherEntityNum2); JKA_SP_SHARED(groundEntityNum); JKA_SP_SHARED(constantLight);
	JKA_SP_SHARED(loopSound); JKA_SP_SHARED(modelindex); JKA_SP_SHARED(modelindex2);
	JKA_SP_SHARED(clientNum); JKA_SP_SHARED(frame); JKA_SP_SHARED(solid);
	JKA_SP_SHARED(event); JKA_SP_SHARED(eventParm); JKA_SP_SHARED(powerups); JKA_SP_SHARED(weapon);
	JKA_SP_SHARED(legsAnim); JKA_SP_SHARED(torsoAnim);
#undef JKA_SP_SHARED
#define JKA_SP_OWN(name) Field<Out>(external.name, internal.raven.sp.name)
	JKA_SP_OWN(modelindex3); JKA_SP_OWN(legsAnimTimer); JKA_SP_OWN(torsoAnimTimer);
	JKA_SP_OWN(scale); JKA_SP_OWN(saberActive); JKA_SP_OWN(vehicleAngles); JKA_SP_OWN(vehicleArmor);
	JKA_SP_OWN(modelScale); JKA_SP_OWN(radius); JKA_SP_OWN(boltInfo);
#undef JKA_SP_OWN
	Field<Out>(external.saberInFlight, internal.raven.saberInFlight);
	Field<Out>(external.vehicleNum, internal.raven.m_iVehicleNum);
	Field<Out>(external.isPortalEnt, internal.raven.isPortalEnt);
	return true;
}
}
inline bool ToInternal(const fntech3_jka_sp_entityState_t &source, entityState_t &destination) noexcept {
	entityState_t temporary{};
	if (!detail::Entity<false>(source, temporary)) return false;
	destination = temporary; return true;
}
inline bool FromInternal(const entityState_t &source, fntech3_jka_sp_entityState_t &destination) noexcept {
	fntech3_jka_sp_entityState_t temporary{};
	if (!detail::Entity<true>(temporary, source)) return false;
	destination = temporary; return true;
}
}
#endif
