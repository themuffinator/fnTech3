/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_ALICE_SNAPSHOT_HPP
#define FNTECH3_SERVER_ALICE_SNAPSHOT_HPP

/* American McGee's Alice's snapshot walk, as retail alice.exe runs it.
 *
 * Ritual's walk is not Quake III's with a few flags renamed. Read from
 * SV_AddEntitiesVisibleFromPoint (0x004571e0..0x00457612) and the
 * SV_AddEntToSnapshot it calls (0x00457150..0x004571d7):
 *
 *  - An entity needs its module-owned in-use word (+0x204) to be looked at,
 *    and every one that has it is marked sent (svFlags bit 15, `or dh, 0x80`
 *    at 0x00457282) before the linked and no-client tests.
 *  - An attached or bound entity is judged by its parent: `s.parent` (+0xa0),
 *    or when that is none the bind parent at +0x1c8 (0x0045729d..0x004572b5).
 *    A child whose parent is not sent (no-client, 0x004572ca) or not drawn
 *    (RF_DONTDRAW, 0x004572d7) is not sent either; a child whose parent is
 *    already in this snapshot is sent without a test (0x00457367..0x0045737e);
 *    and otherwise the child is tested against the parent's areas and
 *    clusters, and the far plane against the parent's sphere.
 *  - A sky origin entity (RF_SKYORIGIN, 0x004572f9) is sent without a test
 *    when the level has a sky portal and the walk is the main view's, and
 *    once the main view is done the walk runs again from its `s.origin`
 *    (0x004575cf..0x00457605) with the sky argument set. A second one is an
 *    error (0x0045732b).
 *  - Broadcast entities, and those with svFlags bit 14, are sent without a
 *    test (0x0045734c); an entity with no model, no looping sound and no
 *    svFlags bit 4 is not sent at all (0x00457394); and a looping sound whose
 *    minimum distance is exactly 30,000 is sent without a test (0x004573ba).
 *  - The level's far plane, squared, culls by the distance to the entity's
 *    centroid less its radius (0x004573fe..0x00457486).
 *  - The already-added test comes after the visibility tests (0x00457527):
 *    an entity seen a second time is marked RF_DUALENTITY and loses
 *    RF_PORTALENTITY. A portal entity seen for the first time opens a walk
 *    from its `s.origin2` with itself as the portal (0x00457599), and a portal
 *    does not recurse into itself.
 *
 * SV_AddEntToSnapshot rewrites the entity's own `renderfx` (+0x1c0) on every
 * add, so the game module sees what the last client's walk decided, and the
 * states are copied into the client's frame only after the walk
 * (SV_BuildClientSnapshot 0x0045709d..0x004570f9).
 *
 * Everything here works on the module's own 732-byte records through byte
 * offsets, the way the retail engine does; a record is never converted. The
 * walk is a template over the world it reads so that it can be exercised
 * without a server (tests/alice_snapshot_tests.cpp). See
 * docs/fntech3/2026-09-22-alice-wire.md. */

#include "../compat/alice_game_abi.h"
#include "../compat/alice_module_abi.h"
#include "../compat/alice_render_abi.h"
#include "../compat/fakk2_module_abi.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3::alice::snapshot {

enum : int {
	/* The snapshot's entity bound, compared at 0x00457189. */
	kEntityLimit = 0x400,
	kNoEntity = 0x3ff,
	/* SV_SvEntityForGentity (0x004533d0) refuses a number outside this. */
	kSvEntities = 0x400
};

/* Offsets in the 732-byte record. The ones in the networked half are the
 * delta table's; the rest are the engine's own reads, each cited. */
enum : std::size_t {
	kNumber = offsetof( fntech3_alice_entityState_t, number ),
	/* `s.origin`, the sky pass's viewpoint (`add eax, 0x60` at 0x004575e7). */
	kOrigin = offsetof( fntech3_alice_entityState_t, origin ),
	/* `s.origin2`, a portal's viewpoint (`add esi, 0x6c` at 0x00457594). */
	kOrigin2 = offsetof( fntech3_alice_entityState_t, origin2 ),
	kLoopSound = offsetof( fntech3_alice_entityState_t, loopSound ),
	kLoopSoundMinDist = offsetof( fntech3_alice_entityState_t, loopSoundMinDist ),
	kParent = offsetof( fntech3_alice_entityState_t, parent ),
	kModelIndex = offsetof( fntech3_alice_entityState_t, modelindex ),
	kRenderfx = offsetof( fntech3_alice_entityState_t, renderfx ),
	/* Ritual's bind parent. fgamex86.dll's Entity::bind stores the master's
	 * number here (0x1009c35e) and its unbind stores 0x3ff (0x1009c502); this
	 * tree names the offset FNTECH3_ALICE_S_OWNERNUM. */
	kBindParent = FNTECH3_ALICE_S_OWNERNUM,
	kInuse = FNTECH3_ALICE_R_INUSE,
	kLinked = FNTECH3_ALICE_R_LINKED,
	kSvFlags = FNTECH3_ALICE_R_SVFLAGS,
	/* The far plane's sphere: a radius and the centroid after it
	 * (`fld [ebp+0x24c]` at 0x0045742d, `fld [ebp+0x250]` at 0x00457413). */
	kRadius = 0x24c,
	kCentroid = 0x250
};

static_assert( kOrigin == 0x60 && kOrigin2 == 0x6c && kParent == 0xa0
	&& kLoopSound == 0x94 && kLoopSoundMinDist == 0x9c && kModelIndex == 0xb8
	&& kRenderfx == 0x1c0 && kBindParent == 0x1c8 && kInuse == 0x204
	&& kLinked == 0x208 && kSvFlags == 0x210,
	"the offsets retail's walk reads" );
static_assert( kCentroid + 12 <= FNTECH3_ALICE_R_CURRENTORIGIN,
	"the sphere sits between the absolute bounds and the current origin" );

/* The same members in the running title's record. Heavy Metal: F.A.K.K.2's
 * walk reads its own entity, whose state is 24 bytes shorter than Alice's:
 * everything up to the model index is at Alice's offsets, and the render
 * flags, the bind parent and the engine's half each sit earlier. The walk
 * itself is taken to be Alice's, whose engine descends from F.A.K.K.2's --
 * F.A.K.K.2's own engine is encrypted on disk and could not be read. Every
 * function below takes the offsets last and defaults to Alice's. */
struct Offsets {
	std::size_t number, origin, origin2, loopSound, loopSoundMinDist, parent,
		modelIndex, renderfx, bindParent, inuse, linked, svFlags, radius, centroid;
};

inline constexpr Offsets kAliceOffsets{ kNumber, kOrigin, kOrigin2, kLoopSound,
	kLoopSoundMinDist, kParent, kModelIndex, kRenderfx, kBindParent, kInuse, kLinked,
	kSvFlags, kRadius, kCentroid };

inline constexpr Offsets kFAKK2Offsets{ FNTECH3_FAKK2_S_NUMBER, FNTECH3_FAKK2_S_ORIGIN,
	FNTECH3_FAKK2_S_ORIGIN2, FNTECH3_FAKK2_S_LOOPSOUND, FNTECH3_FAKK2_S_LOOPSOUNDMINDIST,
	FNTECH3_FAKK2_S_PARENT, FNTECH3_FAKK2_S_MODELINDEX, FNTECH3_FAKK2_S_RENDERFX,
	FNTECH3_FAKK2_S_BINDPARENT, FNTECH3_FAKK2_R_INUSE, FNTECH3_FAKK2_R_LINKED,
	FNTECH3_FAKK2_R_SVFLAGS, FNTECH3_FAKK2_R_RADIUS, FNTECH3_FAKK2_R_CENTROID };

static_assert( kFAKK2Offsets.origin == kAliceOffsets.origin && kFAKK2Offsets.parent == kAliceOffsets.parent
	&& kFAKK2Offsets.modelIndex == kAliceOffsets.modelIndex,
	"the members before the controllers are at the same offsets" );
static_assert( kFAKK2Offsets.renderfx + 20 == kAliceOffsets.renderfx
	&& kFAKK2Offsets.bindParent + 24 == kAliceOffsets.bindParent
	&& kFAKK2Offsets.inuse + 24 == kAliceOffsets.inuse
	&& kFAKK2Offsets.centroid + 24 == kAliceOffsets.centroid,
	"and the rest are 20 or 24 bytes earlier" );

enum : std::uint32_t {
	/* 30,000.0f, compared as bits at 0x004573ba. */
	kGlobalLoopSoundBits = 0x46ea6000u,
	/* SV_BuildClientSnapshot tests the player state's pm_flags for this at
	 * 0x00456fbb and then sees from camera_origin (+0x3dc). */
	kCameraViewFlag = 0x100u,
	/* SV_AddEntToSnapshot clears these three on a first add (0x004571a0). */
	kViewFlags = FNTECH3_ALICE_RF_PORTALENTITY | FNTECH3_ALICE_RF_DUALENTITY
		| FNTECH3_ALICE_RF_SKYENTITY
};

inline std::int32_t ReadInt( const std::uint8_t *record, std::size_t offset ) {
	std::int32_t value;
	std::memcpy( &value, record + offset, sizeof( value ) );
	return value;
}

inline std::uint32_t ReadBits( const std::uint8_t *record, std::size_t offset ) {
	std::uint32_t value;
	std::memcpy( &value, record + offset, sizeof( value ) );
	return value;
}

inline float ReadFloat( const std::uint8_t *record, std::size_t offset ) {
	float value;
	std::memcpy( &value, record + offset, sizeof( value ) );
	return value;
}

inline void WriteInt( std::uint8_t *record, std::size_t offset, std::int32_t value ) {
	std::memcpy( record + offset, &value, sizeof( value ) );
}

inline void ReadVector( const std::uint8_t *record, std::size_t offset, float out[3] ) {
	std::memcpy( out, record + offset, 3 * sizeof( float ) );
}

/* The entity numbers a snapshot carries, in the order they were added. */
struct Numbers {
	int count = 0;
	int numbers[kEntityLimit];
};

/* An entity already in this snapshot is seen again: from a portal, from the
 * sky, or from the main view after being added first. It is then drawn both
 * in a portal and outside it (0x00457162 and 0x00457536). */
inline void MarkDual( std::uint8_t *entity, const Offsets &o = kAliceOffsets ) {
	const std::uint32_t renderfx = ReadBits( entity, o.renderfx );
	WriteInt( entity, o.renderfx, static_cast<std::int32_t>(
		( renderfx & ~static_cast<std::uint32_t>( FNTECH3_ALICE_RF_PORTALENTITY ) )
		| FNTECH3_ALICE_RF_DUALENTITY ) );
}

/*
SV_AddEntToSnapshot (0x00457150). The counter is set before the bound is
checked, and a full snapshot leaves the flags alone, as retail does.
*/
template<class SvEntity>
void AddEntToSnapshot( SvEntity &svEntity, std::uint8_t *entity, Numbers &numbers,
	int counter, bool portal, bool sky, const Offsets &o = kAliceOffsets ) {
	if ( svEntity.snapshotCounter == counter ) {
		MarkDual( entity, o );
		return;
	}
	svEntity.snapshotCounter = counter;
	if ( numbers.count == kEntityLimit ) {
		return;
	}
	std::uint32_t renderfx = ReadBits( entity, o.renderfx ) & ~kViewFlags;
	if ( portal ) {
		renderfx |= FNTECH3_ALICE_RF_PORTALENTITY;
	} else if ( sky ) {
		renderfx |= FNTECH3_ALICE_RF_SKYENTITY;
	}
	WriteInt( entity, o.renderfx, static_cast<std::int32_t>( renderfx ) );
	numbers.numbers[numbers.count++] = ReadInt( entity, o.number );
}

/* SV_SvEntityForGentity (0x004533d0): by the record's own number, refused
 * outside the server's entity table. */
template<class World>
typename World::SvEntity *SvEntityFor( World &world, const std::uint8_t *entity,
	const Offsets &o = kAliceOffsets ) {
	const int number = ReadInt( entity, o.number );
	if ( number < 0 || number >= kSvEntities ) {
		world.Fatal( "SV_SvEntityForGentity: bad gEnt" );
		return nullptr;
	}
	return &world.Sv( number );
}

inline bool ClusterVisible( const std::uint8_t *pvs, int cluster ) {
	return ( pvs[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) != 0;
}

/* The PVS test at 0x0045748c..0x00457521, against the clusters of the entity
 * whose visibility decides -- the parent's for a child. Its overflow scan runs
 * up to the entity's own last cluster and is then compared with the deciding
 * one's, as retail's registers have it; the two are the same for an entity
 * judged by itself. */
template<class SvEntity>
bool PvsVisible( const std::uint8_t *pvs, const SvEntity &visibility, const SvEntity &own ) {
	if ( !pvs ) {
		return false;
	}
	int i = 0;
	int l = 0;
	for ( ; i < visibility.numClusters; ++i ) {
		l = visibility.clusternums[i];
		if ( ClusterVisible( pvs, l ) ) {
			return true;
		}
	}
	const int last = visibility.lastCluster;
	if ( !last ) {
		return false;
	}
	for ( ; l <= own.lastCluster; ++l ) {
		if ( ClusterVisible( pvs, l ) ) {
			break;
		}
	}
	return l != last;
}

/* The far plane at 0x004573fe..0x00457486, in the double precision retail's
 * x87 code runs at: the squared distance from the view to the centroid, less
 * the squared radius, against the squared plane distance the SetFarPlane
 * import stored. An entity is culled only when it is beyond it. */
inline bool BeyondFarPlane( const std::uint8_t *sphere, const float origin[3], int farPlane,
	const Offsets &o = kAliceOffsets ) {
	const double dx = static_cast<double>( ReadFloat( sphere, o.centroid ) ) - origin[0];
	const double dy = static_cast<double>( ReadFloat( sphere, o.centroid + 4 ) ) - origin[1];
	const double dz = static_cast<double>( ReadFloat( sphere, o.centroid + 8 ) ) - origin[2];
	const double radius = ReadFloat( sphere, o.radius );
	const double reach = ( dz * dz + dy * dy + dx * dx ) - radius * radius;
	return reach > static_cast<double>( farPlane );
}

/* The SetFarPlane import (0x004537d0) stores zero, or the plane distance
 * plus 32, squared, in 32 bits. */
inline int SquaredFarPlane( int farPlane ) {
	if ( !farPlane ) {
		return 0;
	}
	const std::uint32_t reach = static_cast<std::uint32_t>( farPlane ) + 32u;
	return static_cast<int>( reach * reach );
}

/*
SV_AddEntitiesVisibleFromPoint (0x004571e0). `portal` is the portal whose view
this is, or null; `sky` is set for the sky portal's view.

World supplies:
  SvEntity                 the server's per-entity record (numClusters,
                           clusternums, lastCluster, areanum, areanum2,
                           snapshotCounter)
  bool Dead()              the server is not running (sv.state == SS_DEAD)
  void View( origin, area, pvs )
                           the view's area and PVS row, with the area bits
                           OR'd into the frame
  int NumEntities()
  std::uint8_t *Entity( n )  the module's record, or null past its arena
  SvEntity &Sv( n )
  int Counter()            sv.snapshotCounter
  bool AreasConnected( a, b )
  int FarPlane()           the squared value, or zero
  bool SkyPortal()
  void Fatal( message )    ERR_DROP; does not return in the engine
  void FixedNumber( n )    a record's number was repaired
*/
template<class World>
void AddEntitiesVisibleFromPoint( World &world, const float viewOrigin[3], Numbers &numbers,
	const typename World::SvEntity *portal, bool sky, const Offsets &o = kAliceOffsets ) {
	using SvEntity = typename World::SvEntity;

	float origin[3] = { viewOrigin[0], viewOrigin[1], viewOrigin[2] };
	std::uint8_t *skyEntity = nullptr;

	for ( ;; ) {
		if ( world.Dead() ) {
			return;
		}
		int area = 0;
		const std::uint8_t *pvs = nullptr;
		world.View( origin, area, pvs );
		const int counter = world.Counter();

		for ( int e = 0; e < world.NumEntities(); ++e ) {
			std::uint8_t *entity = world.Entity( e );
			if ( !entity ) {
				world.Fatal( "SV_AddEntitiesVisibleFromPoint: entity outside the game's arena" );
				return;
			}
			if ( !ReadInt( entity, o.inuse ) ) {
				continue;
			}
			if ( ReadInt( entity, o.number ) != e ) {
				world.FixedNumber( e );
				WriteInt( entity, o.number, e );
			}
			const std::uint32_t svFlags = ReadBits( entity, o.svFlags )
				| FNTECH3_ALICE_SVF_ENGINE_VISITED;
			WriteInt( entity, o.svFlags, static_cast<std::int32_t>( svFlags ) );
			if ( !ReadInt( entity, o.linked ) || ( svFlags & FNTECH3_ALICE_SVF_NOCLIENT ) ) {
				continue;
			}

			int parentNumber = ReadInt( entity, o.parent );
			if ( parentNumber == kNoEntity ) {
				parentNumber = ReadInt( entity, o.bindParent );
			}
			const std::uint8_t *parent = nullptr;
			if ( parentNumber != kNoEntity ) {
				parent = world.Entity( parentNumber );
				if ( !parent ) {
					world.Fatal( "SV_AddEntitiesVisibleFromPoint: parent outside the game's arena" );
					return;
				}
				if ( ReadBits( parent, o.svFlags ) & FNTECH3_ALICE_SVF_NOCLIENT ) {
					continue;
				}
				if ( ReadBits( parent, o.renderfx ) & FNTECH3_ALICE_RF_DONTDRAW ) {
					continue;
				}
			}

			SvEntity *svEntity = SvEntityFor( world, entity, o );
			if ( !svEntity ) {
				return;
			}

			if ( ReadBits( entity, o.renderfx ) & FNTECH3_ALICE_RF_SKYORIGIN ) {
				if ( !world.SkyPortal() || sky || portal ) {
					continue;
				}
				if ( skyEntity ) {
					world.Fatal( "SV_AddEntitiesVisibleFromPoint: duplicate sky origin" );
					return;
				}
				skyEntity = entity;
				AddEntToSnapshot( *svEntity, entity, numbers, counter, false, false, o );
				continue;
			}

			if ( svFlags & ( FNTECH3_ALICE_SVF_BROADCAST | FNTECH3_ALICE_SVF_BROADCAST_ALT ) ) {
				AddEntToSnapshot( *svEntity, entity, numbers, counter, portal != nullptr, sky, o );
				continue;
			}

			const SvEntity *visibility = svEntity;
			if ( parent ) {
				const SvEntity *parentEntity = SvEntityFor( world, parent, o );
				if ( !parentEntity ) {
					return;
				}
				if ( parentEntity->snapshotCounter == counter ) {
					AddEntToSnapshot( *svEntity, entity, numbers, counter, portal != nullptr, sky, o );
					continue;
				}
				visibility = parentEntity;
			}

			const std::int32_t loopSound = ReadInt( entity, o.loopSound );
			if ( !( svFlags & FNTECH3_ALICE_SVF_ALWAYS_RELEVANT )
				&& !ReadInt( entity, o.modelIndex ) && !loopSound ) {
				continue;
			}
			if ( loopSound && ReadBits( entity, o.loopSoundMinDist ) == kGlobalLoopSoundBits ) {
				AddEntToSnapshot( *svEntity, entity, numbers, counter, portal != nullptr, sky, o );
				continue;
			}

			if ( !world.AreasConnected( area, visibility->areanum )
				&& !world.AreasConnected( area, visibility->areanum2 ) ) {
				continue;
			}

			const int farPlane = world.FarPlane();
			if ( farPlane && BeyondFarPlane( parent ? parent : entity, origin, farPlane, o ) ) {
				continue;
			}

			if ( !PvsVisible( pvs, *visibility, *svEntity ) ) {
				continue;
			}

			if ( svEntity->snapshotCounter == counter ) {
				MarkDual( entity, o );
				continue;
			}

			AddEntToSnapshot( *svEntity, entity, numbers, counter, portal != nullptr, sky, o );

			if ( ( ReadBits( entity, o.svFlags ) & FNTECH3_ALICE_SVF_PORTAL ) && svEntity != portal ) {
				float portalOrigin[3];
				ReadVector( entity, o.origin2, portalOrigin );
				AddEntitiesVisibleFromPoint( world, portalOrigin, numbers, svEntity, false, o );
			}
		}

		if ( sky || !skyEntity || portal ) {
			return;
		}
		ReadVector( skyEntity, o.origin, origin );
		sky = true;
		portal = nullptr;
		skyEntity = nullptr;
	}
}

/* Where the client sees from (SV_BuildClientSnapshot 0x00456fbb..0x00457002):
 * a camera's origin while the player state says the view is a camera's, and
 * otherwise the eye. */
inline void ViewOrigin( std::int32_t pmFlags, const float playerOrigin[3], int viewHeight,
	const float cameraOrigin[3], float out[3] ) {
	if ( static_cast<std::uint32_t>( pmFlags ) & kCameraViewFlag ) {
		out[0] = cameraOrigin[0];
		out[1] = cameraOrigin[1];
		out[2] = cameraOrigin[2];
		return;
	}
	out[0] = playerOrigin[0];
	out[1] = playerOrigin[1];
	out[2] = static_cast<float>( static_cast<double>( viewHeight ) + playerOrigin[2] );
}

/* The qsort at 0x0045705f and its comparison at 0x00457110, which refuses a
 * number seen twice. Returns false on a duplicate. */
inline bool SortNumbers( Numbers &numbers ) {
	std::sort( numbers.numbers, numbers.numbers + numbers.count );
	return std::adjacent_find( numbers.numbers, numbers.numbers + numbers.count )
		== numbers.numbers + numbers.count;
}

} // namespace fntech3::alice::snapshot

#endif
