/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_VISIBILITY_HPP
#define FNTECH3_WOLFET_VISIBILITY_HPP

/* ET's per-area waypoints and area-to-area visibility, computed from the loaded
 * AAS world exactly as the original derives them, then kept in engine-internal
 * storage. The original compresses each visibility row and caches it in its
 * route table; fnTech3 keeps an equivalent bit matrix instead, which no module
 * or file ever observes: the only contract is what AAS_WolfETAreaVisible and
 * AAS_WolfETAreaWaypoint answer. Observations:
 * docs/fntech3/2026-09-17-wolfet-visibility.md. */

namespace fntech3 {
namespace wolfet {
namespace visibility {

// One row of bits per area. A map beyond this needs the original's compression.
inline constexpr std::size_t MaxMatrixBytes = 64u * 1024u * 1024u;

inline std::size_t Stride( int areas ) {
	return ( static_cast<std::size_t>( areas ) + 7u ) / 8u;
}

inline bool Reachable( int area ) {
	return AAS_AreaReachability( area ) != 0;
}

/* The original drops a player-sized box from the area centre and keeps the
 * landing point when it stays inside the same area. A blocking entity is
 * retried once as that entity's own pass entity. */
inline bool Waypoint( int area, vec3_t waypoint ) {
	vec3_t mins, maxs, end;
	VectorCopy( aasworld.areas[area].center, end );
	end[2] -= 256;
	AAS_PresenceTypeBoundingBox( PRESENCE_NORMAL, mins, maxs );
	constexpr int mask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP;
	bsp_trace_t trace = AAS_Trace( aasworld.areas[area].center, mins, maxs, end, -1, mask );
	if ( trace.startsolid && trace.ent < ENTITYNUM_WORLD ) {
		trace = AAS_Trace( aasworld.areas[area].center, mins, maxs, end, trace.ent, mask );
	}
	if ( trace.startsolid || trace.fraction >= 1
		|| AAS_PointAreaNum( trace.endpos ) != area ) {
		VectorClear( waypoint );
		return false;
	}
	VectorCopy( trace.endpos, waypoint );
	return true;
}

} // namespace visibility
} // namespace wolfet
} // namespace fntech3

void AAS_WolfETFreeVisibility( void ) {
	if ( aasworld.wolfetWaypoints ) FreeMemory( aasworld.wolfetWaypoints );
	if ( aasworld.wolfetVisibility ) FreeMemory( aasworld.wolfetVisibility );
	if ( aasworld.wolfetDangerDistance ) FreeMemory( aasworld.wolfetDangerDistance );
	if ( aasworld.wolfetHideVisibility ) FreeMemory( aasworld.wolfetHideVisibility );
	aasworld.wolfetWaypoints = nullptr;
	aasworld.wolfetVisibility = nullptr;
	aasworld.wolfetDangerDistance = nullptr;
	aasworld.wolfetHideVisibility = nullptr;
	aasworld.wolfetVisibilityStride = 0;
	aasworld.wolfetVisibilityBuilt = qfalse;
}

/*
===========================================================================
AAS_WolfETBuildVisibility

Derives the waypoints first, then the symmetric visibility between every pair
of areas that produced one. The original checks the potentially visible set
before tracing and mirrors the result into the lower triangle; both keep this
from becoming a full O(n^2) trace sweep.
===========================================================================
*/
void AAS_WolfETBuildVisibility( void ) {
	using namespace fntech3::wolfet::visibility;

	AAS_WolfETFreeVisibility();
	if ( !Com_GameIsWolfET() || !aasworld.loaded || aasworld.numareas <= 1 ) return;

	const int areas = aasworld.numareas;
	const std::size_t stride = Stride( areas );
	if ( stride && static_cast<std::size_t>( areas ) > MaxMatrixBytes / stride ) {
		botimport.Print( PRT_WARNING,
			"WolfET visibility needs more than %u MiB for %i areas; areas stay invisible\n",
			static_cast<unsigned>( MaxMatrixBytes / ( 1024u * 1024u ) ), areas );
		return;
	}

	aasworld.wolfetWaypoints = static_cast<vec3_t *>(
		GetClearedMemory( static_cast<int>( areas * sizeof( vec3_t ) ) ) );
	std::vector<byte> valid( static_cast<std::size_t>( areas ), 0 );
	for ( int area = 1; area < areas; ++area ) {
		if ( !Reachable( area ) ) continue;
		if ( Waypoint( area, aasworld.wolfetWaypoints[area] ) ) valid[area] = 1;
	}

	aasworld.wolfetVisibility = static_cast<byte *>(
		GetClearedMemory( static_cast<int>( static_cast<std::size_t>( areas ) * stride ) ) );
	aasworld.wolfetVisibilityStride = static_cast<int>( stride );

	const auto set = [stride]( int from, int to ) {
		aasworld.wolfetVisibility[static_cast<std::size_t>( from ) * stride + ( to >> 3 )]
			|= static_cast<byte>( 1 << ( to & 7 ) );
	};
	for ( int from = 1; from < areas; ++from ) {
		if ( !valid[from] ) continue;
		set( from, from );
		for ( int to = from + 1; to < areas; ++to ) {
			if ( !valid[to] ) continue;
			if ( !AAS_inPVS( aasworld.wolfetWaypoints[from], aasworld.wolfetWaypoints[to] ) ) continue;
			const bsp_trace_t trace = AAS_Trace( aasworld.wolfetWaypoints[from], nullptr, nullptr,
				aasworld.wolfetWaypoints[to], -1, CONTENTS_SOLID );
			if ( trace.fraction < 1 ) continue;
			set( from, to );
			set( to, from );
		}
	}
	aasworld.wolfetVisibilityBuilt = qtrue;
	botimport.Print( PRT_MESSAGE, "WolfET visibility built for %i areas\n", areas );
}

int AAS_WolfETAreaVisible( int srcarea, int destarea ) {
	if ( !Com_GameIsWolfET() || !aasworld.wolfetVisibilityBuilt || !aasworld.wolfetVisibility
		|| srcarea < 1 || srcarea >= aasworld.numareas
		|| destarea < 1 || destarea >= aasworld.numareas ) {
		return qfalse;
	}
	const auto stride = static_cast<std::size_t>( aasworld.wolfetVisibilityStride );
	return ( aasworld.wolfetVisibility[static_cast<std::size_t>( srcarea ) * stride + ( destarea >> 3 )]
		& ( 1 << ( destarea & 7 ) ) ) ? qtrue : qfalse;
}

const float *AAS_WolfETWaypoint( int area ) {
	if ( !Com_GameIsWolfET() || !aasworld.wolfetWaypoints
		|| area < 1 || area >= aasworld.numareas ) {
		return nullptr;
	}
	const float *waypoint = aasworld.wolfetWaypoints[area];
	// A cleared waypoint means the area produced none, as in the original.
	if ( waypoint[0] == 0 && waypoint[1] == 0 && waypoint[2] == 0 ) return nullptr;
	return waypoint;
}

#endif
