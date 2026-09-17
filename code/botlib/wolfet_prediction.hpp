/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_PREDICTION_HPP
#define FNTECH3_WOLFET_PREDICTION_HPP

/* ET's client movement prediction. It is not Quake's with different settings:
 * it traces the world directly rather than client bounding boxes, so its result
 * carries a full trace with a plane, a surface and contents; it unsticks the
 * start; it adds a move-towards-a-point mode, a hit-entity stop and a stuck
 * stop; and it has no jump-pad, teleporter or cluster-portal stops. Its
 * stop-event bits also differ above 1024. Observations:
 * docs/fntech3/2026-09-17-wolfet-prediction.md. */

namespace fntech3 {
namespace wolfet {
namespace prediction {

// ET's own trace contents, with bodies excluded.
inline constexpr int TraceMask = ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP ) & ~CONTENTS_BODY;
// The original gives up after this many clip iterations inside one frame.
inline constexpr int MaxClips = 20;
// It also refuses a start it cannot lift clear within this height.
inline constexpr float MaxUnstick = 16;

inline void Finish( aas_wolfet_clientmove_t &move, const bsp_trace_t &trace,
	const vec3_t endpos, const vec3_t velocity, float frametime, int event, int area,
	int frames, float contents ) {
	VectorCopy( endpos, move.endpos );
	VectorScale( velocity, 1 / frametime, move.velocity );
	move.trace = trace;
	move.stopevent = event;
	move.presencetype = ( area > 0 && area < aasworld.numareasettings && aasworld.areasettings )
		? aasworld.areasettings[area].presencetype : 0;
	move.endcontents = contents;
	move.time = frames * frametime;
	move.frames = frames;
}

} // namespace prediction
} // namespace wolfet
} // namespace fntech3

/*
===========================================================================
AAS_WolfETPredictClientMovement
===========================================================================
*/
int AAS_WolfETPredictClientMovement( aas_wolfet_clientmove_t *move, int entnum,
	vec3_t origin, int hitent, int onground, vec3_t velocity, vec3_t cmdmove,
	int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum ) {
	using namespace fntech3::wolfet::prediction;

	if ( !move || !Com_GameIsWolfET() || !aasworld.loaded || !aasworld.areasettings ) return qfalse;

	if ( stopareanum == 0 ) stopevent &= ~( SE_ENTERAREA | SE_HITGROUNDAREA );
	if ( frametime <= 0 ) frametime = 0.1f;

	const float friction0 = aassettings.phys_friction;
	const float stopspeed = aassettings.phys_stopspeed;
	const float gravity0 = aassettings.phys_gravity;
	const float waterfriction = aassettings.phys_waterfriction;
	const float watergravity = aassettings.phys_watergravity;
	const float maxwalk = aassettings.phys_maxwalkvelocity;
	const float maxcrouch = aassettings.phys_maxcrouchvelocity;
	const float maxswim = aassettings.phys_maxswimvelocity;
	const float walkaccel = aassettings.phys_walkaccelerate;
	const float airaccel = aassettings.phys_airaccelerate;
	const float swimaccel = aassettings.phys_swimaccelerate;
	const float maxstep = aassettings.phys_maxstep;
	const float maxsteepness = aassettings.phys_maxsteepness;
	const float jumpvel = aassettings.phys_jumpvel * frametime;

	*move = {};
	bsp_trace_t trace{};
	vec3_t mins, maxs, up = { 0, 0, 1 };
	AAS_PresenceTypeBoundingBox( PRESENCE_NORMAL, mins, maxs );

	vec3_t org;
	VectorCopy( origin, org );
	org[2] += 0.25f;
	// Lift a start that begins inside the world until it is clear, or refuse.
	trace = AAS_Trace( org, mins, maxs, org, entnum, TraceMask );
	while ( trace.startsolid ) {
		org[2] += 8;
		trace = AAS_Trace( org, mins, maxs, org, entnum, TraceMask );
		if ( trace.startsolid && ( org[2] - origin[2] > MaxUnstick ) ) {
			move->stopevent = SE_NONE;
			return qfalse;
		}
	}

	vec3_t frame_test_vel;
	VectorScale( velocity, frametime, frame_test_vel );
	int jump_frame = -1;
	cplane_t *lplane = nullptr;
	int n = 0;

	for ( ; n < maxframes; n++ ) {
		const int swimming = AAS_Swimming( org );
		const float gravity = swimming ? watergravity : gravity0;
		frame_test_vel[2] = frame_test_vel[2] - ( gravity * 0.1f * frametime );

		if ( onground || swimming ) {
			// ET selects the pair the opposite way round from Quake.
			const float friction = swimming ? friction0 : waterfriction;
			VectorScale( frame_test_vel, 1 / frametime, frame_test_vel );
			AAS_ApplyFriction( frame_test_vel, friction, stopspeed, frametime );
			VectorScale( frame_test_vel, frametime, frame_test_vel );
		}

		int crouch = qfalse;
		vec3_t wishdir;
		if ( cmdframes < 0 ) {
			// Negative frames mean "walk towards this point", keeping any
			// vertical velocity unless swimming.
			vec3_t savevel;
			VectorSubtract( cmdmove, org, wishdir );
			VectorNormalize( wishdir );
			VectorScale( wishdir, maxwalk, wishdir );
			VectorCopy( frame_test_vel, savevel );
			VectorScale( wishdir, frametime, frame_test_vel );
			if ( !swimming ) frame_test_vel[2] = savevel[2];
		} else if ( n < cmdframes ) {
			float maxvel = maxwalk;
			float accelerate = airaccel;
			VectorCopy( cmdmove, wishdir );
			if ( onground ) {
				if ( cmdmove[2] < -300 ) {
					crouch = qtrue;
					maxvel = maxcrouch;
				}
				if ( !swimming && cmdmove[2] > 1 ) {
					frame_test_vel[2] = jumpvel - ( gravity * 0.1f * frametime ) + 5;
					jump_frame = n;
					accelerate = airaccel;
				} else {
					accelerate = walkaccel;
				}
			}
			if ( swimming ) {
				maxvel = maxswim;
				accelerate = swimaccel;
			} else {
				wishdir[2] = 0;
			}
			float wishspeed = VectorNormalize( wishdir );
			if ( wishspeed > maxvel ) wishspeed = maxvel;
			VectorScale( frame_test_vel, 1 / frametime, frame_test_vel );
			AAS_Accelerate( frame_test_vel, frametime, wishdir, wishspeed, accelerate );
			VectorScale( frame_test_vel, frametime, frame_test_vel );
		}
		(void)crouch;

		vec3_t lastorg, left_test_vel, end;
		VectorCopy( org, lastorg );
		VectorCopy( frame_test_vel, left_test_vel );
		int clips = 0;
		do {
			VectorAdd( org, left_test_vel, end );
			trace = AAS_Trace( org, mins, maxs, end, entnum, TraceMask );

			if ( ( stopevent & SE_WOLFET_HITENT ) && trace.fraction < 1.0f && trace.ent == hitent ) {
				Finish( *move, trace, org, frame_test_vel, frametime, SE_WOLFET_HITENT,
					AAS_PointAreaNum( org ), n, 0 );
				return qtrue;
			}

			if ( stopevent & SE_ENTERAREA ) {
				int areas[20];
				vec3_t points[20];
				const int numareas = AAS_TraceAreas( org, trace.endpos, areas, points, 20 );
				for ( int i = 0; i < numareas; ++i ) {
					if ( areas[i] != stopareanum ) continue;
					Finish( *move, trace, points[i], frame_test_vel, frametime,
						SE_ENTERAREA, areas[i], n, 0 );
					return qtrue;
				}
			}

			if ( ( stopevent & SE_WOLFET_STUCK ) && trace.fraction < 1.0f ) {
				vec3_t heading;
				VectorNormalize2( frame_test_vel, heading );
				if ( DotProduct( trace.plane.normal, heading ) < -0.8f ) {
					Finish( *move, trace, org, frame_test_vel, frametime, SE_WOLFET_STUCK,
						AAS_PointAreaNum( org ), n, 0 );
					return qtrue;
				}
			}

			VectorCopy( trace.endpos, org );
			if ( trace.fraction < 1.0f ) {
				cplane_t *plane = &trace.plane;
				if ( ( stopevent & SE_HITGROUNDAREA )
					&& DotProduct( plane->normal, up ) > maxsteepness ) {
					vec3_t start;
					VectorCopy( org, start );
					start[2] += 0.5f;
					const int at = AAS_PointAreaNum( start );
					if ( ( stopareanum < 0 && at ) || at == stopareanum ) {
						Finish( *move, trace, start, frame_test_vel, frametime,
							SE_HITGROUNDAREA, stopareanum, n, 0 );
						return qtrue;
					}
				}

				int step = qfalse;
				if ( plane->normal[2] == 0 && ( jump_frame < 0 || n - jump_frame > 2 ) ) {
					vec3_t start, stepend;
					VectorMA( org, -0.25f, plane->normal, start );
					VectorCopy( start, stepend );
					start[2] += maxstep;
					const bsp_trace_t steptrace =
						AAS_Trace( start, mins, maxs, stepend, entnum, TraceMask );
					if ( !steptrace.startsolid ) {
						const cplane_t *plane2 = &steptrace.plane;
						if ( DotProduct( plane2->normal, up ) > maxsteepness ) {
							VectorSubtract( end, steptrace.endpos, left_test_vel );
							left_test_vel[2] = 0;
							frame_test_vel[2] = 0;
							org[2] = steptrace.endpos[2];
							step = qtrue;
						}
					}
				}

				if ( !step ) {
					vec3_t old_frame_test_vel;
					VectorMA( left_test_vel, -DotProduct( left_test_vel, plane->normal ),
						plane->normal, left_test_vel );
					// Two near-identical planes in a row would trap the slide.
					if ( lplane && DotProduct( lplane->normal, plane->normal ) > 0.99f ) {
						VectorAdd( plane->normal, left_test_vel, left_test_vel );
					}
					lplane = plane;
					VectorCopy( frame_test_vel, old_frame_test_vel );
					VectorMA( frame_test_vel, -DotProduct( frame_test_vel, plane->normal ),
						plane->normal, frame_test_vel );
					if ( DotProduct( plane->normal, up ) > maxsteepness ) onground = qtrue;

					if ( stopevent & SE_HITGROUNDDAMAGE ) {
						float delta = 0;
						if ( old_frame_test_vel[2] < 0
							&& frame_test_vel[2] > old_frame_test_vel[2] && !onground ) {
							delta = old_frame_test_vel[2];
						} else if ( onground ) {
							delta = frame_test_vel[2] - old_frame_test_vel[2];
						}
						if ( delta ) {
							delta = delta * 10;
							delta = delta * delta * 0.0001f;
							if ( swimming ) delta = 0;
							if ( delta > 40 ) {
								// The original reports the frame velocity here
								// rather than scaling it back to a rate.
								VectorCopy( org, move->endpos );
								VectorCopy( frame_test_vel, move->velocity );
								move->trace = trace;
								move->stopevent = SE_HITGROUNDDAMAGE;
								const int area = AAS_PointAreaNum( org );
								move->presencetype = area ? aasworld.areasettings[area].presencetype : 0;
								move->endcontents = 0;
								move->time = n * frametime;
								move->frames = n;
								return qtrue;
							}
						}
					}
				}
			}
			if ( ++clips > MaxClips ) return qfalse;
		} while ( trace.fraction < 1.0f );

		if ( frame_test_vel[2] <= 10 ) {
			vec3_t feet;
			VectorCopy( org, feet );
			feet[2] -= 22;
			const int pc = AAS_PointContents( feet );
			int event = SE_NONE;
			if ( pc & CONTENTS_LAVA ) event |= SE_ENTERLAVA;
			if ( pc & CONTENTS_SLIME ) event |= SE_ENTERSLIME;
			if ( pc & CONTENTS_WATER ) event |= SE_ENTERWATER;
			const int area = AAS_PointAreaNum( org );
			if ( area > 0 && area < aasworld.numareasettings ) {
				const int contents = aasworld.areasettings[area].contents;
				if ( contents & AREACONTENTS_LAVA ) event |= SE_ENTERLAVA;
				if ( contents & AREACONTENTS_SLIME ) event |= SE_ENTERSLIME;
				if ( contents & AREACONTENTS_WATER ) event |= SE_ENTERWATER;
			}
			if ( event & stopevent ) {
				// The original leaves the trace untouched for this one.
				VectorCopy( org, move->endpos );
				VectorScale( frame_test_vel, 1 / frametime, move->velocity );
				move->stopevent = event & stopevent;
				move->presencetype = ( area > 0 && area < aasworld.numareasettings )
					? aasworld.areasettings[area].presencetype : 0;
				move->endcontents = static_cast<float>( pc );
				move->time = n * frametime;
				move->frames = n;
				return qtrue;
			}
		}

		onground = AAS_OnGround( org, PRESENCE_NORMAL, entnum );
		if ( onground ) {
			if ( stopevent & SE_HITGROUND ) {
				Finish( *move, trace, org, frame_test_vel, frametime, SE_HITGROUND,
					AAS_PointAreaNum( org ), n, 0 );
				return qtrue;
			}
		} else if ( stopevent & SE_LEAVEGROUND ) {
			Finish( *move, trace, org, frame_test_vel, frametime, SE_LEAVEGROUND,
				AAS_PointAreaNum( org ), n, 0 );
			return qtrue;
		} else if ( stopevent & SE_GAP ) {
			vec3_t start, gapend;
			VectorCopy( org, start );
			VectorCopy( start, gapend );
			gapend[2] -= 48 + aassettings.phys_maxbarrier;
			const bsp_trace_t gaptrace =
				AAS_Trace( start, mins, maxs, gapend, -1, TraceMask );
			if ( !gaptrace.startsolid
				&& gaptrace.endpos[2] < org[2] - aassettings.phys_maxstep - 1
				&& !( AAS_PointContents( gapend ) & ( CONTENTS_WATER | CONTENTS_SLIME ) ) ) {
				// The gap is reported at the position before this frame's move.
				Finish( *move, trace, lastorg, frame_test_vel, frametime, SE_GAP,
					AAS_PointAreaNum( org ), n, 0 );
				return qtrue;
			}
		}
	}

	const int area = AAS_PointAreaNum( org );
	VectorCopy( org, move->endpos );
	VectorScale( frame_test_vel, 1 / frametime, move->velocity );
	move->stopevent = SE_NONE;
	move->presencetype = ( area > 0 && area < aasworld.numareasettings )
		? aasworld.areasettings[area].presencetype : 0;
	move->endcontents = 0;
	move->time = n * frametime;
	move->frames = n;
	return qtrue;
}

#endif
