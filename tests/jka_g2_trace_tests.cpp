/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Jedi Academy multiplayer's server trace (imports 27 and 28): the Ghoul2
 * segment and radius tests, retail's record sort, and the entity loop's owner
 * rules, merge and Ghoul2 step, against synthetic meshes and a fake world.
 * See docs/fntech3/2026-10-15-jka-g2-trace.md. */
#include "../code/ghoul2/ghoul2_collision.hpp"
#include "../code/ghoul2/ghoul2_instance_collision.hpp"
#include "../code/compat/jka_trace_clip.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

namespace collision = fntech3::ghoul2::collision;
namespace geometry = fntech3::ghoul2::geometry;
namespace trace = fntech3::jka::trace;

static int failures = 0;
#define CHECK( condition ) do { if ( !( condition ) ) { \
	std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition ); ++failures; } } while ( 0 )

static bool Near( float a, float b, float tolerance = 1e-4f ) { return std::fabs( a - b ) <= tolerance; }

/* A surface of `quads` unit squares in the plane x = `depth`, each two
 * triangles, spanning y and z from -1 to 1 shifted along y by 2 per quad. */
static geometry::DeformedSurface Wall( int index, float depth, int quads, std::uint32_t flags = 0,
	std::vector<int> children = {} )
{
	geometry::DeformedSurface surface;
	surface.metadata.index = index;
	surface.metadata.flags = flags;
	surface.metadata.children = std::move( children );
	for ( int q = 0; q < quads; ++q ) {
		const float y = float( q ) * 2.0f;
		const std::uint32_t base = std::uint32_t( surface.vertices.size() );
		const float corners[4][2] = { { y - 1, -1 }, { y + 1, -1 }, { y + 1, 1 }, { y - 1, 1 } };
		for ( const auto &corner : corners ) {
			geometry::Vertex vertex;
			vertex.position = { depth, corner[0], corner[1] };
			surface.vertices.push_back( vertex );
		}
		surface.triangles.push_back( { base, base + 1, base + 2 } );
		surface.triangles.push_back( { base, base + 2, base + 3 } );
	}
	return surface;
}

static collision::Query Ray( float fromX, float toX, float y = 0.25f, float z = 0.5f )
{
	collision::Query query;
	query.entityNum = 7;
	query.start = { fromX, y, z };
	query.end = { toX, y, z };
	return query;
}

static void SegmentTests()
{
	geometry::DeformedGeometry mesh;
	mesh.surfaces.push_back( Wall( 0, 10.0f, 1, 0, { 1, 2 } ) );
	mesh.surfaces.push_back( Wall( 1, 20.0f, 1 ) );
	mesh.surfaces.push_back( Wall( 2, 30.0f, 1, 0x2 ) );
	const std::vector<collision::Model> models{ { 3, &mesh } };
	collision::Records records;

	/* Two walls in the way, both faces counted; the off surface is not. */
	int count = collision::Detect( models, Ray( 0.0f, 40.0f ), records );
	CHECK( count == 2 );
	CHECK( records[0].entityNum == 7 && records[0].modelIndex == 3 && records[0].surfaceIndex == 0 );
	CHECK( Near( records[0].distance, 10.0f ) && Near( records[0].position[0], 10.0f ) );
	CHECK( records[1].surfaceIndex == 1 && Near( records[1].distance, 20.0f ) );
	CHECK( records[2].entityNum == -1 );
	/* The wall's normal (B - A) x (C - A) faces +x; a ray along +x meets its
	 * back (facing > 0 is retail's front flag 1). */
	CHECK( records[0].flags == collision::FrontFace );
	CHECK( Near( records[0].normal[0], 1.0f ) && Near( records[0].normal[1], 0.0f ) );
	CHECK( Near( records[0].barycentricI, 0.25f ) && Near( records[0].barycentricJ, 0.625f ) );
	count = collision::Detect( models, Ray( 40.0f, 0.0f ), records );
	CHECK( count == 2 && records[0].surfaceIndex == 1 && records[0].flags == collision::BackFace );

	/* Stopping short of a wall, parallel to it, or beside it: no hit. */
	CHECK( collision::Detect( models, Ray( 0.0f, 9.0f ), records ) == 0 );
	CHECK( collision::Detect( models, Ray( 0.0f, 40.0f, 5.0f ), records ) == 0 );
	collision::Query parallel = Ray( 10.0f, 10.0f );
	parallel.end = { 10.0f, 0.5f, 0.75f };
	CHECK( collision::Detect( models, parallel, records ) == 0 );

	/* No descendants: surface 0 with 0x100 stops before 1. A surface flag
	 * other than zero leaves the surface itself out. */
	mesh.surfaces[0].metadata.flags = 0x100;
	CHECK( collision::Detect( models, Ray( 0.0f, 40.0f ), records ) == 0 );
	mesh.surfaces[0].metadata.flags = 0x1;
	count = collision::Detect( models, Ray( 0.0f, 40.0f ), records );
	CHECK( count == 1 && records[0].surfaceIndex == 1 );
	mesh.surfaces[0].metadata.flags = 0;

	/* The entity frame: yaw 90 turns the model's +x to the world's +y, and the
	 * origin moves it; the scale stretches the model before either. */
	collision::Query turned = Ray( 0.0f, 0.0f );
	turned.angles = { 0.0f, 90.0f, 0.0f };
	turned.origin = { 100.0f, 200.0f, 0.0f };
	turned.start = { 99.75f, 200.0f, 0.5f };
	turned.end = { 99.75f, 250.0f, 0.5f };
	count = collision::Detect( models, turned, records );
	CHECK( count == 2 && Near( records[0].position[1], 210.0f ) && Near( records[0].normal[1], 1.0f ) );
	turned.scale = { 2.0f, 0.0f, 0.0f };
	count = collision::Detect( models, turned, records );
	CHECK( count == 2 && Near( records[0].position[1], 220.0f ) && Near( records[0].distance, 20.0f )
		&& Near( records[1].position[1], 240.0f ) );

	/* Every model is walked in the instance's order; a null mesh is refused. */
	geometry::DeformedGeometry near;
	near.surfaces.push_back( Wall( 0, 5.0f, 1 ) );
	const std::vector<collision::Model> both{ { 0, &mesh }, { 1, &near } };
	count = collision::Detect( both, Ray( 0.0f, 40.0f ), records );
	CHECK( count == 3 && records[0].modelIndex == 1 && Near( records[0].distance, 5.0f ) );
	collision::Error error = collision::Error::None;
	CHECK( collision::Detect( { { 0, nullptr } }, Ray( 0.0f, 40.0f ), records, &error ) == 0
		&& error == collision::Error::Mesh );
	geometry::DeformedGeometry broken;
	broken.surfaces.push_back( Wall( 0, 5.0f, 1 ) );
	broken.surfaces[0].triangles[0][2] = 99;
	CHECK( collision::Detect( { { 0, &broken } }, Ray( 0.0f, 40.0f ), records, &error ) == 0
		&& error == collision::Error::Mesh );
}

static void CapTests()
{
	/* Twenty walls in a row: sixteen records fill, the seventeenth hit stops
	 * the whole walk, and the nearest sixteen remain, sorted. Walked deepest
	 * first, the cap keeps the farthest instead -- the walk's order decides
	 * which hits survive, as it does in retail. */
	geometry::DeformedGeometry mesh;
	for ( int i = 0; i < 20; ++i ) {
		std::vector<int> children;
		if ( i + 1 < 20 ) children.push_back( i + 1 );
		mesh.surfaces.push_back( Wall( i, 10.0f + float( i ), 1, 0, children ) );
	}
	collision::Records records;
	int count = collision::Detect( { { 0, &mesh } }, Ray( 0.0f, 100.0f ), records );
	CHECK( count == collision::MaxRecords );
	CHECK( Near( records[0].distance, 10.0f ) && Near( records[15].distance, 25.0f ) );
	for ( int i = 1; i < count; ++i ) CHECK( records[i - 1].distance <= records[i].distance );
	count = collision::Detect( { { 0, &mesh } }, Ray( 100.0f, 0.0f ), records );
	CHECK( count == collision::MaxRecords && Near( records[0].distance, 100.0f - 25.0f )
		&& Near( records[15].distance, 100.0f - 10.0f ) );
}

static void RadiusTests()
{
	geometry::DeformedGeometry mesh;
	mesh.surfaces.push_back( Wall( 0, 10.0f, 3 ) );
	collision::Records records;
	/* A point ray between the quads' centres misses the middle quad's
	 * triangles beside it, but a radius sweeps every triangle whose corners
	 * do not all lie beyond one face of the box around the line. */
	collision::Query query = Ray( 0.0f, 20.0f, 0.0f, 0.0f );
	int count = collision::Detect( { { 0, &mesh } }, query, records );
	CHECK( count == 2 );
	query.radius = 0.09f;   /* still a point: |radius| < 0.1 */
	CHECK( collision::Detect( { { 0, &mesh } }, query, records ) == 2 );
	query.radius = 0.5f;
	count = collision::Detect( { { 0, &mesh } }, query, records );
	CHECK( count == 2 );
	CHECK( records[0].flags == collision::FrontFace && Near( records[0].position[0], 10.0f )
		&& Near( records[0].distance, 10.0f ) && records[0].barycentricI == 0.0f );
	query.radius = 1.5f;    /* reaches the next quad's near corners */
	count = collision::Detect( { { 0, &mesh } }, query, records );
	CHECK( count == 4 );
	/* The box ends at the line's two ends: a ray stopping short sees nothing. */
	query.end = { 9.0f, 0.0f, 0.0f };
	CHECK( collision::Detect( { { 0, &mesh } }, query, records ) == 0 );
	/* Vertical rays take the other basis. */
	geometry::DeformedGeometry floor;
	geometry::DeformedSurface flat;
	flat.metadata.index = 0;
	for ( const auto &p : { collision::Vector{ -1, -1, 0 }, collision::Vector{ 1, -1, 0 },
		collision::Vector{ 1, 1, 0 } } ) {
		geometry::Vertex vertex; vertex.position = p; flat.vertices.push_back( vertex );
	}
	flat.triangles.push_back( { 0, 1, 2 } );
	floor.surfaces.push_back( flat );
	collision::Query down;
	down.entityNum = 1;
	down.radius = 1.0f;
	down.start = { 0.5f, -0.25f, 10.0f };
	down.end = { 0.5f, -0.25f, -10.0f };
	count = collision::Detect( { { 0, &floor } }, down, records );
	CHECK( count == 1 && Near( records[0].position[2], 0.0f ) && Near( records[0].normal[2], 1.0f )
		&& Near( records[0].distance, 10.0f ) );
}

#if defined( _MSC_VER )
static int CrtOrder( const void *a, const void *b )
{
	return static_cast<const collision::Record *>( a )->distance
		< static_cast<const collision::Record *>( b )->distance ? -1 : 1;
}
#endif

static void SortTests()
{
	/* Retail's order never answers "equal", so equally distant records come
	 * out in whatever order the runtime's algorithm leaves them. The port is
	 * compared with the C runtime's own qsort where this build has it. */
	unsigned seed = 12345u;
	const auto next = [&seed]() { seed = seed * 1103515245u + 12345u; return ( seed >> 16 ) & 0x7fffu; };
	for ( int round = 0; round < 4000; ++round ) {
		const int count = 1 + int( next() % 16u );
		collision::Records records;
		for ( int i = 0; i < count; ++i ) {
			records[std::size_t( i )].distance = float( next() % 5u );
			records[std::size_t( i )].polyIndex = i;
		}
		collision::Records ported = records;
		collision::Sort( ported.data(), count );
		for ( int i = 1; i < count; ++i ) CHECK( ported[std::size_t( i - 1 )].distance <= ported[std::size_t( i )].distance );
		std::map<int, int> seen;
		for ( int i = 0; i < count; ++i ) ++seen[ported[std::size_t( i )].polyIndex];
		CHECK( int( seen.size() ) == count );
#if defined( _MSC_VER )
		collision::Records crt = records;
		std::qsort( crt.data(), std::size_t( count ), sizeof( collision::Record ), CrtOrder );
		for ( int i = 0; i < count; ++i ) CHECK( crt[std::size_t( i )].polyIndex == ported[std::size_t( i )].polyIndex );
#endif
	}
}

/* --- the entity loop --------------------------------------------------- */

struct FakeTrace {
	int allsolid = 0, startsolid = 0;
	float fraction = 1.0f;
	float endpos[3] = {};
	struct { float normal[3] = {}; float dist = 0; } plane;
	int surfaceFlags = 0, contents = 0, entityNum = -99;
};

struct FakeEntity {
	trace::Entity entity;
	FakeTrace swept;          /* what its box sweep answers */
	bool hits = false;        /* whether its Ghoul2 mesh is struck */
	int surface = 0;
};

struct FakeWorld {
	FakeTrace world;
	std::vector<int> touching;
	std::map<int, FakeEntity> entities;
	std::vector<int> clipped, traced;
	float lastRadius = -1.0f;

	void Sweep( FakeTrace &out, const float *, const float *, const float *, const float *, int ) { out = world; }
	int Touching( const float *, const float *, int *list, int capacity ) {
		int n = 0;
		for ( int number : touching ) if ( n < capacity ) list[n++] = number;
		return n;
	}
	bool Read( int number, trace::Entity &out ) {
		const auto found = entities.find( number );
		if ( found == entities.end() ) return false;
		out = found->second.entity;
		return true;
	}
	void Clip( int number, FakeTrace &out, const float *, const float *, const float *, const float *, int ) {
		clipped.push_back( number );
		out = entities[number].swept;
		out.entityNum = 0;
	}
	bool Ghoul2( int number, float radius, trace::Ghoul2Hit &hit ) {
		traced.push_back( number );
		lastRadius = radius;
		const FakeEntity &entity = entities[number];
		if ( !entity.hits ) return false;
		hit.entityNum = entity.entity.number;
		hit.surfaceIndex = entity.surface;
		hit.position[0] = 1.0f; hit.position[1] = 2.0f; hit.position[2] = 3.0f;
		hit.normal[2] = 1.0f;
		return true;
	}
};

static FakeEntity Body( int number, float fraction, std::int32_t contents = 0x100 )
{
	FakeEntity body;
	body.entity.number = number;
	body.entity.contents = contents;
	body.entity.ghoul2 = true;
	body.swept.fraction = fraction;
	body.swept.endpos[0] = fraction * 100.0f;
	body.swept.surfaceFlags = 0x55;
	return body;
}

static const float start[3] = { 0, 0, 0 }, end[3] = { 100, 0, 0 };

static void LoopTests()
{
	/* The world blocks at once: nothing else is looked at. */
	{
		FakeWorld world;
		world.world.fraction = 0.0f;
		world.touching = { 5 };
		FakeTrace out;
		CHECK( trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 1, 0 ) );
		CHECK( out.entityNum == trace::EntityWorld && world.clipped.empty() );
	}
	/* A clear world reports 1023; a body in the way is struck by its box,
	 * and with flag 1 its mesh decides: a miss puts the trace back. */
	{
		FakeWorld world;
		world.touching = { 5 };
		world.entities[5] = Body( 5, 0.5f );
		FakeTrace out;
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, 0 );
		CHECK( out.entityNum == 5 && Near( out.fraction, 0.5f ) && world.traced.empty() );
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, trace::DoGhoul2 );
		CHECK( out.entityNum == trace::EntityNone && Near( out.fraction, 1.0f ) && world.traced.size() == 1 );
		CHECK( world.lastRadius == 0.0f );
		world.entities[5].hits = true;
		world.entities[5].surface = 9;
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, trace::DoGhoul2 );
		CHECK( out.entityNum == 5 && Near( out.fraction, 0.5f ) && out.endpos[1] == 2.0f
			&& out.plane.normal[2] == 1.0f && out.surfaceFlags == 0x55 );
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100,
			trace::DoGhoul2 | trace::SurfaceIndex );
		CHECK( out.surfaceFlags == 9 );
		/* A dead body is left to its box unless corpses are asked for. */
		world.entities[5].entity.eFlags = trace::FlagDead;
		world.traced.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, trace::DoGhoul2 );
		CHECK( world.traced.empty() && out.endpos[1] == 0.0f );
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100,
			trace::DoGhoul2 | trace::HitCorpses );
		CHECK( world.traced.size() == 1 && out.endpos[1] == 2.0f );
		/* No instance, no step. */
		world.entities[5].entity.ghoul2 = false;
		world.traced.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100,
			trace::DoGhoul2 | trace::HitCorpses );
		CHECK( world.traced.empty() );
	}
	/* A box traces half its width; flag 8 makes that at least one. */
	{
		FakeWorld world;
		world.touching = { 5 };
		world.entities[5] = Body( 5, 0.5f );
		const float mins[3] = { -0.25f, -3, -3 }, maxs[3] = { 0.25f, 3, 3 };
		FakeTrace out;
		trace::Run( world, out, start, mins, maxs, end, trace::EntityNone, 0x100, trace::DoGhoul2 );
		CHECK( world.lastRadius == 0.25f );
		trace::Run( world, out, start, mins, maxs, end, trace::EntityNone, 0x100, trace::DoGhoul2 | trace::Thick );
		CHECK( world.lastRadius == 1.0f );
		const float wideMins[3] = { -4, -4, -4 }, wideMaxs[3] = { 4, 4, 4 };
		trace::Run( world, out, start, wideMins, wideMaxs, end, trace::EntityNone, 0x100, trace::DoGhoul2 | trace::Thick );
		CHECK( world.lastRadius == 4.0f );
	}
	/* Only the entity the box sweep made the trace's own gets the step; a
	 * farther body behind a nearer one does not. Retail's one exception:
	 * entity 0, whose number matches the zeroed record a missed sweep leaves. */
	{
		FakeWorld world;
		world.touching = { 4, 6, 0 };
		world.entities[4] = Body( 4, 0.3f );
		world.entities[6] = Body( 6, 0.6f );
		world.entities[0] = Body( 0, 1.0f );
		world.entities[0].hits = true;
		FakeTrace out;
		/* Each body's mesh missed: the trace went back each time, so the
		 * next body's box made it its own and was traced in turn. */
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, trace::DoGhoul2 );
		CHECK( ( world.traced == std::vector<int>{ 4, 6, 0 } ) );
		CHECK( out.entityNum == trace::EntityNone && Near( out.fraction, 1.0f ) && out.endpos[1] == 2.0f );
		/* The nearer body struck: the farther one's box does not take the
		 * trace, so it is not traced; entity 0 still is. */
		world.entities[4].hits = true;
		world.traced.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, trace::DoGhoul2 );
		CHECK( ( world.traced == std::vector<int>{ 4, 0 } ) );
		CHECK( out.entityNum == 4 && Near( out.fraction, 0.3f ) && out.endpos[1] == 2.0f );
	}
	/* Raven's owner rules. */
	{
		FakeWorld world;
		world.touching = { 1, 2, 3, 4, 8 };
		world.entities[1] = Body( 1, 0.9f );           /* the pass entity */
		world.entities[1].entity.ownerNum = 8;
		world.entities[2] = Body( 2, 0.2f );           /* the pass entity's missile */
		world.entities[2].entity.ownerNum = 1;
		world.entities[3] = Body( 3, 0.3f );           /* a sibling from the same owner */
		world.entities[3].entity.ownerNum = 8;
		world.entities[4] = Body( 4, 0.4f, 0x200100 ); /* not to be shot */
		world.entities[8] = Body( 8, 0.8f );           /* the owner */
		FakeTrace out;
		trace::Run( world, out, start, nullptr, nullptr, end, 1, 0x100, 0 );
		CHECK( ( world.clipped == std::vector<int>{ 4, 8 } ) && out.entityNum == 4 );
		world.clipped.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, 1, trace::MaskShot, 0 );
		CHECK( ( world.clipped == std::vector<int>{ 8 } ) );
		/* A not-shared missile is hit by its owner's shot, and only by a shot. */
		world.entities[2].entity.svFlags = trace::OwnerNotShared;
		world.clipped.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, 1, trace::MaskShotSaber, 0 );
		CHECK( ( world.clipped == std::vector<int>{ 2, 8 } ) && out.entityNum == 2 );
		world.clipped.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, 1, 0x100, 0 );
		CHECK( ( world.clipped == std::vector<int>{ 4, 8 } ) );
		/* A not-shared pass entity collides with its owner's other things. */
		world.entities[1].entity.svFlags = trace::OwnerNotShared;
		world.clipped.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, 1, 0x100, 0 );
		CHECK( ( world.clipped == std::vector<int>{ 3, 4, 8 } ) );
		/* ...but never with a shared missile of that owner. */
		world.entities[3].entity.eType = trace::TypeMissile;
		world.clipped.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, 1, 0x100, 0 );
		CHECK( ( world.clipped == std::vector<int>{ 4, 8 } ) );
		/* An unreadable pass entity is a refusal. */
		CHECK( !trace::Run( world, out, start, nullptr, nullptr, end, 77, 0x100, 0 ) );
	}
	/* -1, the module's "ignore nothing", is no pass entity: everything in
	 * the way is swept, the things owned by client 0 or by -1 too (retail
	 * read record -1 and, at g_forceBasedTeams 0, skipped both). Any other
	 * number outside the records is still refused. */
	{
		FakeWorld world;
		world.touching = { 0, 2, 3, 5 };
		world.entities[0] = Body( 0, 0.9f );           /* client 0 */
		world.entities[2] = Body( 2, 0.6f );           /* owned by client 0 */
		world.entities[2].entity.ownerNum = 0;
		world.entities[3] = Body( 3, 0.4f );           /* owned by -1 */
		world.entities[3].entity.ownerNum = -1;
		world.entities[5] = Body( 5, 0.7f );
		FakeTrace none, nothing;
		CHECK( trace::Run( world, none, start, nullptr, nullptr, end, trace::EntityNone, 0x100, 0 ) );
		const std::vector<int> swept = world.clipped;
		world.clipped.clear();
		CHECK( trace::Run( world, nothing, start, nullptr, nullptr, end, trace::PassNothing, 0x100, 0 ) );
		CHECK( ( world.clipped == std::vector<int>{ 0, 2, 3, 5 } ) && world.clipped == swept );
		CHECK( nothing.entityNum == 3 && Near( nothing.fraction, 0.4f ) );
		CHECK( none.entityNum == nothing.entityNum && Near( none.fraction, nothing.fraction ) );
		world.clipped.clear();
		CHECK( trace::Run( world, nothing, start, nullptr, nullptr, end, trace::PassNothing,
			trace::MaskShot, trace::DoGhoul2 ) );
		CHECK( ( world.clipped == std::vector<int>{ 0, 2, 3, 5 } ) );
		FakeTrace refused;
		CHECK( !trace::Run( world, refused, start, nullptr, nullptr, end, -2, 0x100, 0 ) );
		CHECK( !trace::Run( world, refused, start, nullptr, nullptr, end, trace::MaxEntities, 0x100, 0 ) );
		CHECK( !trace::Run( world, refused, start, nullptr, nullptr, end, -1024, 0x100, 0 ) );
	}
	/* Starting inside an entity names it at once; being wholly inside one
	 * stops the loop. */
	{
		FakeWorld world;
		world.touching = { 2, 3 };
		world.entities[2] = Body( 2, 1.0f );
		world.entities[2].swept.startsolid = 1;
		world.entities[3] = Body( 3, 0.5f );
		FakeTrace out;
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, 0 );
		CHECK( out.startsolid && out.entityNum == 3 && Near( out.fraction, 0.5f ) );
		world.entities[3].swept.fraction = 1.0f;
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, 0 );
		CHECK( out.startsolid && out.entityNum == 2 );
		world.entities[2].swept.allsolid = 1;
		world.entities[2].swept.fraction = 0.0f;
		world.clipped.clear();
		trace::Run( world, out, start, nullptr, nullptr, end, trace::EntityNone, 0x100, 0 );
		CHECK( out.allsolid && ( world.clipped == std::vector<int>{ 2 } ) );
	}
}

/* Imports 606 and 299 hand the module's own records to G2API_CollisionDetect
 * (0x4e0170): nothing is reset, a free place is entity -1, the leading
 * records in use are counted and sorted, and a model's override list stands
 * in for its file's surface flags. */
static void CollideTests()
{
	geometry::DeformedGeometry mesh;
	mesh.surfaces.push_back( Wall( 0, 10.0f, 1, 0, { 1, 2 } ) );
	mesh.surfaces.push_back( Wall( 1, 20.0f, 1 ) );
	mesh.surfaces.push_back( Wall( 2, 30.0f, 1, 0x2 ) );
	collision::Records records;

	/* A record the caller left in place 0 stays, counted and sorted with the hits. */
	records[0].entityNum = 77;
	records[0].distance = 15.0f;
	int count = collision::Collide( { { 0, &mesh } }, Ray( 0.0f, 40.0f ), records );
	CHECK( count == 3 );
	CHECK( records[0].entityNum == 7 && Near( records[0].distance, 10.0f ) );
	CHECK( records[1].entityNum == 77 && Near( records[1].distance, 15.0f ) );
	CHECK( records[2].entityNum == 7 && Near( records[2].distance, 20.0f ) && records[3].entityNum == -1 );

	/* One free place among used ones: the first hit takes it, the second finds
	 * none and stops the walk, and the count runs past it to the end. */
	for ( auto &record : records ) {
		record = collision::Record{};
		record.entityNum = 50;
		record.distance = 99.0f;
	}
	records[1].entityNum = -1;
	count = collision::Collide( { { 0, &mesh } }, Ray( 0.0f, 40.0f ), records );
	CHECK( count == collision::MaxRecords );
	CHECK( records[0].entityNum == 7 && Near( records[0].distance, 10.0f ) && records[0].surfaceIndex == 0 );
	for ( int i = 1; i < collision::MaxRecords; ++i ) CHECK( records[std::size_t( i )].entityNum == 50 );

	/* A used record before the first free place hides nothing after it from the
	 * walk, but the count stops at the free place. */
	for ( auto &record : records ) record = collision::Record{};
	records[2].entityNum = 60;
	count = collision::Collide( { { 0, &mesh } }, Ray( 0.0f, 40.0f ), records );
	CHECK( count == 3 && records[2].entityNum == 7 && Near( records[2].distance, 20.0f ) );

	/* The override list: surface 2 switched on, surface 1 off. */
	for ( auto &record : records ) record = collision::Record{};
	collision::SurfaceOverrides overrides{ { 1, 0x2u }, { 2, 0u } };
	const collision::Model switched{ 0, &mesh, &overrides };
	count = collision::Collide( { switched }, Ray( 0.0f, 40.0f ), records );
	CHECK( count == 2 && records[0].surfaceIndex == 0 && records[1].surfaceIndex == 2
		&& Near( records[1].distance, 30.0f ) );
	/* 0x100 in an override leaves everything under the surface out too. */
	overrides = { { 0, 0x100u } };
	for ( auto &record : records ) record = collision::Record{};
	CHECK( collision::Collide( { switched }, Ray( 0.0f, 40.0f ), records ) == 0 );
	/* Detect still resets what it is handed. */
	records[0].entityNum = 77;
	CHECK( collision::Detect( { { 0, &mesh } }, Ray( 0.0f, 40.0f ), records ) == 2
		&& records[0].entityNum == 7 && records[2].entityNum == -1 );

	/* A mesh that cannot be walked leaves every record as it was. */
	for ( auto &record : records ) record = collision::Record{};
	records[0].entityNum = 77;
	collision::Error error = collision::Error::None;
	CHECK( collision::Collide( { { 0, nullptr } }, Ray( 0.0f, 40.0f ), records, &error ) == 0
		&& error == collision::Error::Mesh && records[0].entityNum == 77 );

	/* The module's records are retail's bytes, field for field. */
	unsigned char bytes[1024];
	for ( std::size_t i = 0; i < sizeof( bytes ); ++i ) bytes[i] = static_cast<unsigned char>( i * 7 );
	collision::ReadRecords( bytes, records );
	collision::Records copy = records;
	unsigned char back[1024] = {};
	collision::WriteRecords( back, copy );
	CHECK( std::memcmp( bytes, back, sizeof( bytes ) ) == 0 );
	int entity = 0;
	std::memcpy( &entity, bytes + 64 * 3 + 4, sizeof( entity ) );
	CHECK( records[3].entityNum == entity );
}

/* A geometry cache follows the store its files came from: each store is told
 * apart for the life of the process. */
struct NoFiles final : fntech3::ghoul2::Files {
	std::size_t Read( const char *, void *, std::size_t ) override { return 0; }
};
static void StoreSerialTests()
{
	NoFiles files;
	fntech3::ghoul2::Store first( files ), second( files );
	CHECK( first.Serial() != 0 && second.Serial() != 0 && first.Serial() != second.Serial() );
	fntech3::ghoul2::Store third( files );
	CHECK( third.Serial() != first.Serial() && third.Serial() != second.Serial() );
}

int main()
{
	CollideTests();
	StoreSerialTests();
	SegmentTests();
	CapTests();
	RadiusTests();
	SortTests();
	LoopTests();
	if ( failures ) {
		std::printf( "%d failure(s)\n", failures );
		return 1;
	}
	std::printf( "jka_g2_trace_tests: all passed\n" );
	return 0;
}
