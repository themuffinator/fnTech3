/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_TRACE_CLIP_HPP
#define FNTECH3_COMPAT_JKA_TRACE_CLIP_HPP

/* Jedi Academy multiplayer's server trace: the one routine behind both game
 * import 27 (G_TRACE) and import 28 (G_G2TRACE), measured from retail
 * jamp.exe 1.01. Independently written; OpenJK 1a6a643427aa347553e9073dac55
 * 70b33337c4d9 (codemp/server/sv_world.cpp) only located the routine.
 *
 *   import 27  0x45abd7  the trace with Ghoul2 flags 0 and the module's
 *                        tenth word as the level of detail
 *   import 28  0x45ac34  the same with the module's ninth word as the flags
 *   trace      0x462920  a missing box is the origin; the world first
 *                        (capsule 0), entity 1022 for a hit and 1023 for none;
 *                        a fraction of 0 ends it there
 *   entities   0x462490  Raven's owner rules, the box sweep, the merge, and
 *                        then the Ghoul2 step below
 *
 * The step is what import 28 adds. When the trace asked for Ghoul2 collision
 * (flag 1) and the entity just swept is the one the entity's own sweep names,
 * and it carries a Ghoul2 instance, and it is alive or the trace takes
 * corpses too (flag 2): its models are traced, and if no triangle is hit the
 * whole trace goes back to what it was before this entity -- a shot through
 * the gaps of a body's box passes on. A hit keeps the fraction and the entity
 * of the box and takes the hit's position and normal; with flag 4 the hit
 * surface's index replaces the surface flags. A trace with a box traces a
 * radius of half its width, and flag 8 makes that at least one unit.
 *
 * One retail detail is kept on purpose: a box sweep that misses leaves the
 * entity word of its record 0 (the collision model zeroes the record,
 * 0x435990), so entity 0 -- the first client -- is given the Ghoul2 step even
 * when the box missed it. A hit then moves the end of a trace that belongs to
 * whatever it last struck.
 *
 * A pass entity of -1 is read as no pass entity. The retail game module
 * passes -1 for "ignore nothing" (seven 1.01 sites, MASK_SOLID: the bot
 * melee and saber ground checks and the seeker drone's line of fire), while
 * retail's loop compares the pass entity only with 1023 (0x4624f8) and so
 * reads record -1, the bytes in front of the module's entity array: with the
 * 1.01 module those are its own cvars, the owner the float bits of
 * g_forceBasedTeams' value and the flags a tail of g_podiumDist's string.
 * That is the module's memory layout, not a contract, so -1 is given the one
 * reading it was written for, as Jedi Outcast's trace does; any other number
 * outside the game's records is still refused.
 * See docs/fntech3/2026-10-16-jka-trace-pass-none.md. */
#include <cstdint>
#include <cstring>

namespace fntech3::jka::trace {
inline constexpr int EntityWorld = 1022;
inline constexpr int EntityNone = 1023;
inline constexpr int PassNothing = -1;      /* the module's "ignore nothing" */
inline constexpr int MaxEntities = 1024;   /* the touch list's capacity, 0x4624d9 */
inline constexpr int MaxClients = 32;      /* 0x462793 */
inline constexpr std::int32_t OwnerNotShared = 0x1000;  /* r.svFlags */
inline constexpr std::int32_t MaskShot = 0x1301;
inline constexpr std::int32_t MaskShotSaber = 0x41301;
inline constexpr std::int32_t ContentsNoShot = 0x200000;
inline constexpr std::int32_t TypeMissile = 3;
inline constexpr std::int32_t FlagDead = 2;
enum : int { DoGhoul2 = 1, HitCorpses = 2, SurfaceIndex = 4, Thick = 8 };

/* What the routine reads of one entity (its retail offsets are in
 * docs/fntech3/2026-10-15-jka-g2-trace.json). */
struct Entity {
	int number = 0;
	std::int32_t eType = 0;
	std::int32_t eFlags = 0;
	std::int32_t svFlags = 0;
	std::int32_t contents = 0;
	std::int32_t ownerNum = EntityNone;
	bool ghoul2 = false;
};

struct Ghoul2Hit {
	int entityNum = -1;
	int surfaceIndex = 0;
	float position[3] = {};
	float normal[3] = {};
};

/* The trace record a World works in: the engine's own trace, whose field
 * names this lineage shares. `World` serves the collision model:
 *
 *   void Sweep( Trace &, start, end, mins, maxs, mask )   the world, capsule 0
 *   int  Touching( boxmins, boxmaxs, int *list, int capacity )
 *   bool Read( int number, Entity & )                     false refuses it
 *   void Clip( int number, Trace &, start, end, mins, maxs, mask )
 *   bool Ghoul2( int number, float radius, Ghoul2Hit & )  true for a hit
 *
 * Clip leaves the record's entity at 0, as retail's collision model does. */
template<class Trace, class World>
bool Run( World &world, Trace &out, const float start[3], const float minsIn[3],
	const float maxsIn[3], const float end[3], int passEntityNum, int contentmask,
	int traceFlags )
{
	static const float origin[3] = { 0.0f, 0.0f, 0.0f };
	const float *mins = minsIn ? minsIn : origin;
	const float *maxs = maxsIn ? maxsIn : origin;
	using Solid = decltype( out.allsolid );

	Trace clip{};
	world.Sweep( clip, start, end, mins, maxs, contentmask );
	clip.entityNum = double( clip.fraction ) != 1.0 ? EntityWorld : EntityNone;
	if ( clip.fraction == 0.0f ) {
		out = clip;
		return true;
	}

	float boxmins[3], boxmaxs[3];
	for ( int i = 0; i < 3; ++i ) {
		if ( end[i] > start[i] ) {
			boxmins[i] = start[i] + mins[i] - 1.0f;
			boxmaxs[i] = end[i] + maxs[i] + 1.0f;
		} else {
			boxmins[i] = end[i] + mins[i] - 1.0f;
			boxmaxs[i] = start[i] + maxs[i] + 1.0f;
		}
	}
	static int touch[MaxEntities];
	const int count = world.Touching( boxmins, boxmaxs, touch, MaxEntities );
	if ( count < 0 || count > MaxEntities ) return false;

	if ( passEntityNum == PassNothing ) passEntityNum = EntityNone;
	int passOwnerNum = -1;
	bool passShared = true;
	if ( passEntityNum != EntityNone ) {
		Entity pass;
		if ( !world.Read( passEntityNum, pass ) ) return false;
		if ( pass.ownerNum != EntityNone ) passOwnerNum = pass.ownerNum;
		if ( pass.svFlags & OwnerNotShared ) passShared = false;
	}
	const bool shot = contentmask == MaskShot || contentmask == MaskShotSaber;

	Trace oldTrace{};
	for ( int i = 0; i < count; ++i ) {
		if ( clip.allsolid ) break;
		/* The touch list's number picks the record and meets the pass
		 * entity; the record's own number is what the trace reports. */
		const int touched = touch[i];
		Entity entity;
		if ( !world.Read( touched, entity ) ) return false;
		const int number = entity.number;
		if ( passEntityNum != EntityNone ) {
			if ( touched == passEntityNum ) continue;
			if ( entity.ownerNum == passEntityNum ) {
				/* An owner's own missile, unless it is not shared and this
				 * is a shot: a laser can hit its owner's other "missile". */
				if ( !( entity.svFlags & OwnerNotShared ) || !shot ) continue;
			}
			if ( entity.ownerNum == passOwnerNum && !( entity.svFlags & OwnerNotShared )
				&& passShared ) continue;
			if ( entity.eType == TypeMissile && !( entity.svFlags & OwnerNotShared )
				&& entity.ownerNum == passOwnerNum ) continue;
		}
		if ( !( contentmask & entity.contents ) ) continue;
		if ( shot && entity.contents > 0 && ( entity.contents & ContentsNoShot ) ) continue;

		Trace swept{};
		world.Clip( touched, swept, start, end, mins, maxs, contentmask );
		if ( traceFlags & DoGhoul2 ) oldTrace = clip;
		if ( swept.allsolid ) {
			clip.allsolid = Solid( 1 );
			swept.entityNum = number;
		} else if ( swept.startsolid ) {
			clip.startsolid = Solid( 1 );
			swept.entityNum = number;
			clip.entityNum = number;
		}
		if ( swept.fraction < clip.fraction ) {
			const bool oldStart = clip.startsolid != Solid( 0 );
			swept.entityNum = number;
			clip = swept;
			if ( oldStart ) clip.startsolid = Solid( 1 );
		}

		if ( !( traceFlags & DoGhoul2 ) || swept.entityNum != number || !entity.ghoul2
			|| ( !( traceFlags & HitCorpses ) && ( entity.eFlags & FlagDead ) ) ) continue;
		float radius = 0.0f;
		if ( mins[0] != 0.0f || maxs[0] != 0.0f ) radius = ( maxs[0] - mins[0] ) / 2.0f;
		if ( ( traceFlags & Thick ) && radius < 1.0f ) radius = 1.0f;
		Ghoul2Hit hit;
		if ( !world.Ghoul2( touched, radius, hit ) || hit.entityNum != number ) {
			clip = oldTrace;
			continue;
		}
		for ( int axis = 0; axis < 3; ++axis ) {
			clip.endpos[axis] = hit.position[axis];
			clip.plane.normal[axis] = hit.normal[axis];
		}
		if ( ( traceFlags & SurfaceIndex ) && clip.entityNum == hit.entityNum ) {
			clip.surfaceFlags = hit.surfaceIndex;
		}
	}
	out = clip;
	return true;
}
} // namespace fntech3::jka::trace
#endif
