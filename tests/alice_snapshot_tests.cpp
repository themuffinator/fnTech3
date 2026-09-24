/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* American McGee's Alice's snapshot walk (code/server/server_alice_snapshot.hpp)
 * run over a fake world of 732-byte records, pinned against the retail
 * alice.exe reading in docs/fntech3/2026-09-22-alice-wire.md. No retail file
 * is read here. */

#include "../code/server/server_alice_snapshot.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace snapshot = fntech3::alice::snapshot;
using snapshot::ReadBits;
using snapshot::ReadInt;
using snapshot::WriteInt;

enum : std::uint32_t {
	NOCLIENT = FNTECH3_ALICE_SVF_NOCLIENT,
	BROADCAST = FNTECH3_ALICE_SVF_BROADCAST,
	PORTAL = FNTECH3_ALICE_SVF_PORTAL,
	RELEVANT = FNTECH3_ALICE_SVF_ALWAYS_RELEVANT,
	BROADCAST_ALT = FNTECH3_ALICE_SVF_BROADCAST_ALT,
	SENT = FNTECH3_ALICE_SVF_ENGINE_VISITED,
	RF_PORTAL = FNTECH3_ALICE_RF_PORTALENTITY,
	RF_DUAL = FNTECH3_ALICE_RF_DUALENTITY,
	RF_SKY = FNTECH3_ALICE_RF_SKYENTITY,
	RF_SKYORIGIN = FNTECH3_ALICE_RF_SKYORIGIN,
	RF_DONTDRAW = FNTECH3_ALICE_RF_DONTDRAW
};

struct FakeSvEntity {
	int numClusters = 0;
	int clusternums[16] = {};
	int lastCluster = 0;
	int areanum = 0;
	int areanum2 = -1;
	int snapshotCounter = 0;
};

struct Fatal : std::runtime_error {
	using std::runtime_error::runtime_error;
};

/* Views are keyed by the x of their origin: each is an area and a PVS row. A
 * cluster is visible from a view when its bit is set in that view's row; two
 * areas are connected when they are equal, or when a test says so. */
struct FakeWorld {
	using SvEntity = FakeSvEntity;
	struct ViewRow {
		float x;
		int area;
		std::array<std::uint8_t, 8> pvs;
	};

	std::vector<std::array<std::uint8_t, FNTECH3_ALICE_GENTITY_SIZE>> records;
	std::vector<FakeSvEntity> sv;
	std::vector<ViewRow> rows;
	std::vector<float> viewed;
	int counter = 1;
	int farPlane = 0;
	bool skyPortal = false;
	bool dead = false;
	bool nullPvs = false;
	std::vector<std::pair<int, int>> connected;
	int fixed = 0;

	explicit FakeWorld( int count ) : records( count ), sv( snapshot::kSvEntities ) {
		for ( int i = 0; i < count; ++i ) {
			records[i].fill( 0 );
			WriteInt( Record( i ), snapshot::kNumber, i );
			WriteInt( Record( i ), snapshot::kParent, snapshot::kNoEntity );
			WriteInt( Record( i ), snapshot::kBindParent, snapshot::kNoEntity );
			WriteInt( Record( i ), snapshot::kInuse, 1 );
			WriteInt( Record( i ), snapshot::kLinked, 1 );
			WriteInt( Record( i ), snapshot::kModelIndex, 1 );
			sv[i].numClusters = 1;
			sv[i].clusternums[0] = 1;
		}
		rows.push_back( { 0.0f, 0, { 0x02, 0, 0, 0, 0, 0, 0, 0 } } );
	}

	std::uint8_t *Record( int number ) { return records[number].data(); }
	std::uint32_t Flags( int number, std::size_t offset ) { return ReadBits( Record( number ), offset ); }
	void Set( int number, std::size_t offset, std::uint32_t value ) {
		WriteInt( Record( number ), offset, static_cast<std::int32_t>( value ) );
	}
	void SetVector( int number, std::size_t offset, float x, float y, float z ) {
		const float v[3] = { x, y, z };
		std::memcpy( Record( number ) + offset, v, sizeof( v ) );
	}

	bool Dead() const { return dead; }
	void View( const float origin[3], int &area, const std::uint8_t *&pvs ) {
		viewed.push_back( origin[0] );
		for ( const ViewRow &row : rows ) {
			if ( row.x == origin[0] ) {
				area = row.area;
				pvs = nullPvs ? nullptr : row.pvs.data();
				return;
			}
		}
		throw ::Fatal( "no such view" );
	}
	int NumEntities() const { return static_cast<int>( records.size() ); }
	std::uint8_t *Entity( int number ) {
		return number >= 0 && number < NumEntities() ? Record( number ) : nullptr;
	}
	FakeSvEntity &Sv( int number ) { return sv[number]; }
	int Counter() const { return counter; }
	bool AreasConnected( int a, int b ) const {
		if ( a == b ) return true;
		for ( const auto &pair : connected ) {
			if ( ( pair.first == a && pair.second == b ) || ( pair.first == b && pair.second == a ) ) return true;
		}
		return false;
	}
	int FarPlane() const { return farPlane; }
	bool SkyPortal() const { return skyPortal; }
	void Fatal( const char *message ) { throw ::Fatal( message ); }
	void FixedNumber( int ) { ++fixed; }
};

static std::vector<int> Walk( FakeWorld &world, float x = 0.0f ) {
	snapshot::Numbers numbers;
	const float origin[3] = { x, 0.0f, 0.0f };
	snapshot::AddEntitiesVisibleFromPoint( world, origin, numbers, nullptr, false );
	return std::vector<int>( numbers.numbers, numbers.numbers + numbers.count );
}

/* SV_AddEntToSnapshot, 0x00457150. */
static void CheckAdd() {
	FakeWorld world( 3 );
	snapshot::Numbers numbers;
	world.Set( 0, snapshot::kRenderfx, RF_PORTAL | RF_DUAL | RF_SKY | 0x80u | 0x2000u );
	snapshot::AddEntToSnapshot( world.sv[0], world.Record( 0 ), numbers, 7, false, false );
	CHECK( numbers.count == 1 && numbers.numbers[0] == 0 && world.sv[0].snapshotCounter == 7 );
	CHECK( world.Flags( 0, snapshot::kRenderfx ) == ( 0x80u | 0x2000u ) );

	/* Portal wins over sky. */
	snapshot::AddEntToSnapshot( world.sv[1], world.Record( 1 ), numbers, 7, true, true );
	CHECK( world.Flags( 1, snapshot::kRenderfx ) == RF_PORTAL );
	snapshot::AddEntToSnapshot( world.sv[2], world.Record( 2 ), numbers, 7, false, true );
	CHECK( world.Flags( 2, snapshot::kRenderfx ) == RF_SKY );
	CHECK( numbers.count == 3 );

	/* A second add marks it dual, drops the portal flag and appends nothing. */
	snapshot::AddEntToSnapshot( world.sv[1], world.Record( 1 ), numbers, 7, false, false );
	CHECK( world.Flags( 1, snapshot::kRenderfx ) == RF_DUAL && numbers.count == 3 );
	world.Set( 2, snapshot::kRenderfx, RF_SKY | 1u );
	snapshot::AddEntToSnapshot( world.sv[2], world.Record( 2 ), numbers, 7, true, false );
	CHECK( world.Flags( 2, snapshot::kRenderfx ) == ( RF_SKY | RF_DUAL | 1u ) );

	/* Full: the counter is taken, the flags are not touched, nothing appended. */
	numbers.count = snapshot::kEntityLimit;
	world.sv[0].snapshotCounter = 0;
	world.Set( 0, snapshot::kRenderfx, RF_PORTAL );
	snapshot::AddEntToSnapshot( world.sv[0], world.Record( 0 ), numbers, 8, false, true );
	CHECK( numbers.count == snapshot::kEntityLimit && world.sv[0].snapshotCounter == 8 );
	CHECK( world.Flags( 0, snapshot::kRenderfx ) == RF_PORTAL );
}

/* The gates before any visibility test: in use, sent, linked, no-client and
 * the number repair (0x0045724a..0x00457297). */
static void CheckGates() {
	FakeWorld world( 5 );
	world.Set( 1, snapshot::kInuse, 0 );
	world.Set( 2, snapshot::kLinked, 0 );
	world.Set( 3, snapshot::kSvFlags, NOCLIENT );
	world.Set( 4, snapshot::kNumber, 99 );
	const std::vector<int> sent = Walk( world );
	CHECK( ( sent == std::vector<int>{ 0, 4 } ) );
	CHECK( world.fixed == 1 && ReadInt( world.Record( 4 ), snapshot::kNumber ) == 4 );
	/* Every entity in use is marked sent, linked or not; one not in use is not. */
	CHECK( !( world.Flags( 1, snapshot::kSvFlags ) & SENT ) );
	CHECK( world.Flags( 2, snapshot::kSvFlags ) & SENT );
	CHECK( world.Flags( 3, snapshot::kSvFlags ) == ( NOCLIENT | SENT ) );

	FakeWorld dead( 2 );
	dead.dead = true;
	CHECK( Walk( dead ).empty() && dead.viewed.empty() );
}

/* What makes an entity worth testing, and what skips the test
 * (0x0045734c..0x004573c4). */
static void CheckRelevance() {
	FakeWorld world( 7 );
	for ( int i = 1; i < 7; ++i ) {
		world.Set( i, snapshot::kModelIndex, 0 );
		world.sv[i].clusternums[0] = 5; /* not in the view's PVS */
	}
	world.Set( 1, snapshot::kSvFlags, RELEVANT );           /* tested, invisible */
	world.Set( 2, snapshot::kSvFlags, BROADCAST );          /* sent untested */
	world.Set( 3, snapshot::kSvFlags, BROADCAST_ALT );      /* sent untested */
	world.Set( 4, snapshot::kLoopSound, 3 );                /* tested, invisible */
	world.Set( 5, snapshot::kLoopSound, 3 );                /* global loop sound */
	world.Set( 5, snapshot::kLoopSoundMinDist, 0x46ea6000u );
	/* 6: no model, no sound, no flag -- never sent even when visible. */
	world.sv[6].clusternums[0] = 1;
	const std::vector<int> sent = Walk( world );
	CHECK( ( sent == std::vector<int>{ 0, 2, 3, 5 } ) );

	/* Once visible, a bit-4 entity and a looping sound go. */
	world.counter++;
	world.sv[1].clusternums[0] = 1;
	world.sv[4].clusternums[0] = 1;
	CHECK( ( Walk( world ) == std::vector<int>{ 0, 1, 2, 3, 4, 5 } ) );
}

/* The area and PVS tests (0x004573ca..0x00457521). */
static void CheckAreasAndClusters() {
	FakeWorld world( 5 );
	world.sv[1].areanum = 3;                                /* blocked */
	world.sv[2].areanum = 3;
	world.sv[2].areanum2 = 0;                               /* the second area opens it */
	world.sv[3].numClusters = 0;                            /* overflowed clusters */
	world.sv[3].lastCluster = 4;
	world.rows[0].pvs[0] = 0x02 | 0x08;                     /* clusters 1 and 3 */
	/* The overflow scan starts at the last stored cluster; finding a visible
	 * cluster exactly at the last one counts as not visible, as in retail. */
	world.sv[4].clusternums[0] = 2;
	world.sv[4].lastCluster = 3;
	const std::vector<int> sent = Walk( world );
	CHECK( ( sent == std::vector<int>{ 0, 2, 3 } ) );
	world.counter++;
	world.sv[4].lastCluster = 4;
	CHECK( ( Walk( world ) == std::vector<int>{ 0, 2, 3, 4 } ) );

	world.counter++;
	world.nullPvs = true;
	CHECK( Walk( world ).empty() );
}

/* A child is judged by its parent (0x0045729d..0x0045738a). */
static void CheckParents() {
	{
		/* A child of a hidden or undrawn parent is never sent, however it is flagged. */
		FakeWorld world( 5 );
		world.Set( 0, snapshot::kSvFlags, NOCLIENT );
		world.Set( 1, snapshot::kRenderfx, RF_DONTDRAW );
		world.Set( 2, snapshot::kParent, 0 );
		world.Set( 2, snapshot::kSvFlags, BROADCAST );
		world.Set( 3, snapshot::kParent, 1 );
		world.Set( 4, snapshot::kBindParent, 1 );
		CHECK( ( Walk( world ) == std::vector<int>{ 1 } ) );
	}
	{
		/* A parent already added sends its child untested. */
		FakeWorld world( 2 );
		world.Set( 1, snapshot::kParent, 0 );
		world.sv[1].areanum = 9;
		world.sv[1].clusternums[0] = 6;
		CHECK( ( Walk( world ) == std::vector<int>{ 0, 1 } ) );
	}
	{
		/* A parent later in the walk lends its areas and clusters. */
		FakeWorld world( 4 );
		world.Set( 0, snapshot::kParent, 2 );                /* own cluster invisible, parent's visible */
		world.sv[0].clusternums[0] = 6;
		world.Set( 1, snapshot::kBindParent, 3 );            /* own cluster visible, parent blocked */
		world.sv[3].areanum = 9;
		CHECK( ( Walk( world ) == std::vector<int>{ 0, 2 } ) );
		CHECK( world.sv[1].snapshotCounter != world.counter );
	}
	{
		/* s.parent wins over the bind parent. */
		FakeWorld world( 4 );
		world.Set( 3, snapshot::kParent, 1 );
		world.Set( 3, snapshot::kBindParent, 2 );
		world.Set( 2, snapshot::kSvFlags, NOCLIENT );
		CHECK( ( Walk( world ) == std::vector<int>{ 0, 1, 3 } ) );
	}
	{
		/* A parent outside the arena, or with a number the server has no
		 * record for, is refused rather than read. */
		FakeWorld world( 2 );
		world.Set( 1, snapshot::kParent, 50 );
		bool refused = false;
		try { Walk( world ); } catch ( const Fatal & ) { refused = true; }
		CHECK( refused );
		FakeWorld bad( 2 );
		bad.Set( 1, snapshot::kParent, 0 );
		bad.Set( 0, snapshot::kNumber, 0x400 );
		bad.Set( 0, snapshot::kInuse, 0 );
		refused = false;
		try { Walk( bad ); } catch ( const Fatal &e ) { refused = std::string( e.what() ) == "SV_SvEntityForGentity: bad gEnt"; }
		CHECK( refused );
	}
}

/* The far plane (0x004573fe..0x00457486), squared as SetFarPlane stores it. */
static void CheckFarPlane() {
	CHECK( snapshot::SquaredFarPlane( 0 ) == 0 );
	CHECK( snapshot::SquaredFarPlane( 10000 ) == 100641024 );
	CHECK( snapshot::SquaredFarPlane( 68 ) == 10000 );

	FakeWorld world( 5 );
	world.farPlane = snapshot::SquaredFarPlane( 68 );         /* 100 units */
	world.SetVector( 1, snapshot::kCentroid, 120.0f, 0.0f, 0.0f );
	world.Set( 1, snapshot::kModelIndex, 0 );                 /* a parent that is not sent itself */
	world.SetVector( 2, snapshot::kCentroid, 100.0f, 0.0f, 0.0f ); /* exactly on the plane */
	world.Set( 3, snapshot::kParent, 1 );                     /* near, but its parent's sphere decides */
	world.SetVector( 4, snapshot::kCentroid, 120.0f, 0.0f, 0.0f );
	world.Set( 4, snapshot::kRadius, 0x41a00000u );           /* 20: 14,400 - 400 is still beyond */
	CHECK( ( Walk( world ) == std::vector<int>{ 0, 2 } ) );
	world.counter++;
	world.farPlane = 0;                                       /* no plane, no cull */
	CHECK( ( Walk( world ) == std::vector<int>{ 0, 2, 3, 4 } ) );
}

/* A portal's view (0x0045757e..0x00457599). */
static void CheckPortals() {
	FakeWorld world( 6 );
	world.rows.push_back( { 500.0f, 1, { 0x04, 0, 0, 0, 0, 0, 0, 0 } } );
	world.rows.push_back( { 700.0f, 2, { 0x08, 0, 0, 0, 0, 0, 0, 0 } } );
	world.sv[0].numClusters = 2;                              /* both, added from the main view first */
	world.sv[0].clusternums[1] = 2;
	world.sv[0].areanum2 = 1;
	world.Set( 1, snapshot::kSvFlags, PORTAL );
	world.SetVector( 1, snapshot::kOrigin2, 500.0f, 0.0f, 0.0f );
	world.sv[1].clusternums[0] = 1;
	world.sv[1].areanum2 = 1;                                 /* seen from both views */
	world.sv[1].numClusters = 2;
	world.sv[1].clusternums[1] = 2;
	world.sv[2].clusternums[0] = 2;                           /* only through the portal */
	world.sv[2].areanum = 1;
	world.sv[3].numClusters = 2;                              /* both, reached through the portal first */
	world.sv[3].clusternums[1] = 2;
	world.sv[3].areanum2 = 1;
	world.sv[4].clusternums[0] = 3;                           /* only through the second portal */
	world.sv[4].areanum = 2;
	world.Set( 5, snapshot::kSvFlags, PORTAL );               /* a second portal, seen through the first */
	world.SetVector( 5, snapshot::kOrigin2, 700.0f, 0.0f, 0.0f );
	world.sv[5].clusternums[0] = 2;
	world.sv[5].areanum = 1;
	const std::vector<int> sent = Walk( world );
	CHECK( ( sent == std::vector<int>{ 0, 1, 2, 3, 5, 4 } ) );
	CHECK( ( world.viewed == std::vector<float>{ 0.0f, 500.0f, 700.0f } ) );
	CHECK( world.Flags( 0, snapshot::kRenderfx ) == RF_DUAL );    /* main view first, then the portal */
	CHECK( world.Flags( 1, snapshot::kRenderfx ) == RF_DUAL );    /* a portal does not open itself twice */
	CHECK( world.Flags( 2, snapshot::kRenderfx ) == RF_PORTAL );
	CHECK( world.Flags( 5, snapshot::kRenderfx ) == RF_PORTAL );
	CHECK( world.Flags( 4, snapshot::kRenderfx ) == RF_PORTAL );
	/* 3 was reached through the portal first, then again from the main view. */
	CHECK( world.Flags( 3, snapshot::kRenderfx ) == RF_DUAL );
}

/* The sky portal (0x004572f9..0x00457347, 0x004575cf..0x00457605). */
static void CheckSky() {
	{
		FakeWorld world( 4 );
		world.rows.push_back( { 900.0f, 4, { 0x10, 0, 0, 0, 0, 0, 0, 0 } } );
		world.Set( 1, snapshot::kRenderfx, RF_SKYORIGIN );
		world.SetVector( 1, snapshot::kOrigin, 900.0f, 0.0f, 0.0f );
		world.sv[1].clusternums[0] = 7;                       /* sent untested */
		world.sv[2].clusternums[0] = 4;                       /* in the sky box */
		world.sv[2].areanum = 4;
		world.sv[3].numClusters = 2;                          /* seen by both */
		world.sv[3].clusternums[1] = 4;
		world.sv[3].areanum2 = 4;

		/* Without the level's sky portal the origin is not sent and the sky is not walked. */
		CHECK( ( Walk( world ) == std::vector<int>{ 0, 3 } ) );
		CHECK( ( world.viewed == std::vector<float>{ 0.0f } ) );

		world.counter++;
		world.viewed.clear();
		world.skyPortal = true;
		const std::vector<int> sent = Walk( world );
		CHECK( ( sent == std::vector<int>{ 0, 1, 3, 2 } ) );
		CHECK( ( world.viewed == std::vector<float>{ 0.0f, 900.0f } ) );
		CHECK( world.Flags( 1, snapshot::kRenderfx ) == RF_SKYORIGIN );
		CHECK( world.Flags( 2, snapshot::kRenderfx ) == RF_SKY );
		CHECK( world.Flags( 3, snapshot::kRenderfx ) == RF_DUAL );
		CHECK( world.Flags( 0, snapshot::kRenderfx ) == 0 );
	}
	{
		/* Two sky origins are an error. */
		FakeWorld world( 3 );
		world.skyPortal = true;
		world.Set( 1, snapshot::kRenderfx, RF_SKYORIGIN );
		world.Set( 2, snapshot::kRenderfx, RF_SKYORIGIN );
		bool refused = false;
		try { Walk( world ); } catch ( const Fatal &e ) {
			refused = std::string( e.what() ) == "SV_AddEntitiesVisibleFromPoint: duplicate sky origin";
		}
		CHECK( refused );
	}
	{
		/* A portal's view skips a sky origin outright: it is taken by the main
		 * view afterwards, as a sky origin and nothing else. */
		FakeWorld world( 3 );
		world.skyPortal = true;
		world.rows.push_back( { 500.0f, 1, { 0x04, 0, 0, 0, 0, 0, 0, 0 } } );
		world.Set( 0, snapshot::kSvFlags, PORTAL );
		world.SetVector( 0, snapshot::kOrigin2, 500.0f, 0.0f, 0.0f );
		world.Set( 2, snapshot::kRenderfx, RF_SKYORIGIN );
		const std::vector<int> sent = Walk( world );
		CHECK( ( sent == std::vector<int>{ 0, 1, 2 } ) );
		CHECK( ( world.viewed == std::vector<float>{ 0.0f, 500.0f, 0.0f } ) );
		CHECK( world.Flags( 2, snapshot::kRenderfx ) == RF_SKYORIGIN );
		CHECK( world.Flags( 1, snapshot::kRenderfx ) == RF_DUAL );
	}
}

/* The snapshot's bound (0x00457189). */
static void CheckLimit() {
	FakeWorld world( 10 );
	snapshot::Numbers numbers;
	numbers.count = snapshot::kEntityLimit - 4;
	const float origin[3] = { 0.0f, 0.0f, 0.0f };
	snapshot::AddEntitiesVisibleFromPoint( world, origin, numbers, nullptr, false );
	CHECK( numbers.count == snapshot::kEntityLimit );
	CHECK( numbers.numbers[snapshot::kEntityLimit - 1] == 3 );
	/* The rest were counted as added, and kept their flags. */
	CHECK( world.sv[9].snapshotCounter == world.counter );
}

/* The view origin (0x00456fbb..0x00457002) and the sort (0x0045705f). */
static void CheckViewAndSort() {
	const float player[3] = { 1.0f, 2.0f, 3.5f };
	const float camera[3] = { -7.0f, 8.0f, 9.0f };
	float out[3];
	snapshot::ViewOrigin( 0, player, 22, camera, out );
	CHECK( out[0] == 1.0f && out[1] == 2.0f && out[2] == 25.5f );
	snapshot::ViewOrigin( 0x100 | 0x1, player, 22, camera, out );
	CHECK( out[0] == -7.0f && out[1] == 8.0f && out[2] == 9.0f );
	snapshot::ViewOrigin( 0xfeff, player, -4, camera, out );
	CHECK( out[2] == -0.5f );

	snapshot::Numbers numbers;
	numbers.count = 4;
	numbers.numbers[0] = 9; numbers.numbers[1] = 2; numbers.numbers[2] = 5; numbers.numbers[3] = 0;
	CHECK( snapshot::SortNumbers( numbers ) );
	CHECK( numbers.numbers[0] == 0 && numbers.numbers[1] == 2 && numbers.numbers[2] == 5 && numbers.numbers[3] == 9 );
	numbers.numbers[2] = 2;
	CHECK( !snapshot::SortNumbers( numbers ) );
	numbers.count = 0;
	CHECK( snapshot::SortNumbers( numbers ) );
}

int main() {
	CheckAdd();
	CheckGates();
	CheckRelevance();
	CheckAreasAndClusters();
	CheckParents();
	CheckFarPlane();
	CheckPortals();
	CheckSky();
	CheckLimit();
	CheckViewAndSort();
	std::puts( "alice snapshot tests passed" );
	return 0;
}
