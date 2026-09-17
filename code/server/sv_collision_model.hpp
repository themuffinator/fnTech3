/*
===========================================================================
Copyright (C) 2026 FnQL contributors

This file is part of FnQL.

FnQL is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.
===========================================================================
*/

#ifndef FNQL_SERVER_SV_COLLISION_MODEL_HPP
#define FNQL_SERVER_SV_COLLISION_MODEL_HPP

#include "../qcommon/game_profile.h"

namespace fnql::server::collision {

/*
 * QLSRP records the retail QL contract: SVF_CAPSULE changes an entity hull
 * only for capsule traces.  Point/box traces, including hitscan weapon fire,
 * continue to test the entity's axis-aligned box. Original RTCW SP/MP
 * sv_world.c selects the entity's flagged capsule for every trace shape.
 * The immutable profile owns this difference.
 */
[[nodiscard]] constexpr bool UseCapsuleEntityModel(
	comGameEntityHullPolicy_t policy, bool traceUsesCapsule, bool entitySupportsCapsule ) noexcept
{
	return entitySupportsCapsule
		&& ( traceUsesCapsule || policy == COM_ENTITY_HULL_ENTITY_FLAG );
}

} // namespace fnql::server::collision

#endif
