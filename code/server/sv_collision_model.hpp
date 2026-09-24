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

/*
 * Retail WolfMP.exe and et.exe give the temporary box brush a traced entity's
 * own r.contents when the entity's clip handle is that box, so the trace mask
 * tests the entity by those contents and the trace reports them. Their
 * SV_ClipMoveToEntities sets them just before the sweep and puts CONTENTS_BODY
 * back after it (0x43d339..0x43d3f0 and 0x449e61..0x449f1c), and nothing else
 * in either engine changes them, so SV_ClipToEntity, SV_PointContents and the
 * client game always see CONTENTS_BODY. Capsule and inline-model handles are
 * left alone. WolfSP.exe, Quake III and Quake Live never change the box's
 * CONTENTS_BODY. A Com_Error longjmp may skip the server's restore, but it also
 * stops the server, and the next map load builds the box again.
 */
[[nodiscard]] constexpr bool UseEntityBoxContents(
	comGameEntityBoxContentsPolicy_t policy, bool clipsTemporaryBox ) noexcept
{
	return clipsTemporaryBox && policy == COM_ENTITY_BOX_CONTENTS_ENTITY;
}

} // namespace fnql::server::collision

#endif
