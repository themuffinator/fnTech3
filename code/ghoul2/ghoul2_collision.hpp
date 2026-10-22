/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GHOUL2_COLLISION_HPP
#define FNTECH3_GHOUL2_COLLISION_HPP

/* Independently authored segment and swept-radius tests against posed Ghoul2
 * meshes: what Jedi Academy multiplayer's server does for a trace that asks
 * for Ghoul2 collision (game import 28). Measured from retail jamp.exe 1.01;
 * OpenJK 1a6a643427aa347553e9073dac5570b33337c4d9 (codemp/rd-vanilla
 * G2_misc.cpp and G2_API.cpp) only located the routines and named them.
 *
 *   collision detect   0x4e0170  world matrix from angles and origin
 *                                (0x4ec580, inverse 0x4ec620), the ray moved
 *                                into model space, models traced, then the
 *                                records counted and qsorted (0x52bc40) by
 *                                distance (0x4dfd10)
 *   models             0x4ec1c0  each model in order; one with model index -1,
 *                                not valid, or flagged 1 (no collision) is
 *                                skipped; a stop ends the walk
 *   surfaces           0x4ec0a0  depth first from the root surface, children
 *                                in file order; any nonzero surface flag
 *                                leaves the surface out, and 0x100 its
 *                                descendants too; |radius| < 0.1 (double,
 *                                0x565fb0) is a segment test, else a radius
 *                                test
 *   segment test       0x4eb1c0  every triangle, both faces (0x4eb234), with
 *                                the 1e-10 parallel bound of 0x4ea120
 *   radius test        0x4eb4a0  a vertex box test around the swept line
 *   records            16 of 64 bytes (0x1308780..0x1308b80); a hit that
 *                      finds none free stops the whole walk
 *
 * Everything here works on vertices already deformed in the model's own space,
 * so the pose is the caller's business, as is choosing the level of detail.
 * Nothing here reads a file or a module's memory. */
#include "ghoul2_geometry.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace fntech3::ghoul2::collision {
using Vector = std::array<float, 3>;
using Matrix = std::array<float, 12>; /* rows of three axes and a translation */

inline constexpr int MaxRecords = 16;
inline constexpr double PointRadius = 0.1;      /* 0x565fb0 */
inline constexpr float ParallelBound = 1e-10f;  /* 0x54e3b4 */
inline constexpr std::uint32_t NoDescendants = 0x100u;
inline constexpr int FrontFace = 1, BackFace = 0;

/* Retail's collision record, field for field (64 bytes each). The server reads
 * the entity, surface, position and normal; the rest is kept because it costs
 * nothing and keeps the record the record retail fills. */
struct Record {
	float distance = 0.0f;
	int entityNum = -1;
	int modelIndex = 0;
	int polyIndex = 0;
	int surfaceIndex = 0;
	Vector position{};
	Vector normal{};
	int flags = 0;
	int material = 0;
	int location = 0;
	float barycentricI = 0.0f;
	float barycentricJ = 0.0f;
};
using Records = std::array<Record, MaxRecords>;

/* Surface flags a model holds in place of its file's own, by surface index:
 * retail's one override list per model (jamp.exe 1.01 0x4ee560 finds a
 * surface's entry), which a skin and a surface switch both write. */
using SurfaceOverrides = std::map<int, std::uint32_t>;

/* One model of an instance, deformed for the moment of the trace. `index` is
 * its place in the instance, which a record names; `overrides` its override
 * list, or null for none. */
struct Model {
	int index = 0;
	const geometry::DeformedGeometry *mesh = nullptr;
	const SurfaceOverrides *overrides = nullptr;
};

struct Query {
	Vector angles{};
	Vector origin{};
	Vector scale{};     /* the entity's model scale; a zero component is 1 */
	Vector start{};     /* the ray, in the world */
	Vector end{};
	int entityNum = -1;
	float radius = 0.0f;
};

inline Vector Subtract( const Vector &a, const Vector &b ) {
	return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}
inline float Dot( const Vector &a, const Vector &b ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline Vector Cross( const Vector &a, const Vector &b ) {
	return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}
inline float Length( const Vector &v ) {
	return float( std::sqrt( double( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] ) ) );
}
/* id's VectorNormalize: a zero vector stays zero. */
inline void Normalise( Vector &v ) {
	const float length = Length( v );
	if ( length ) {
		const float inverse = 1.0f / length;
		v[0] *= inverse; v[1] *= inverse; v[2] *= inverse;
	}
}

/* The world matrix of an entity: its angles as id's axes (forward, left, up)
 * standing as the matrix's columns, and its origin as the translation. */
inline Matrix WorldMatrix( const Vector &angles, const Vector &origin ) {
	constexpr double degrees = 3.14159265358979323846 * 2.0 / 360.0;
	const float pitch = float( angles[0] * degrees ), yaw = float( angles[1] * degrees ),
		roll = float( angles[2] * degrees );
	const float sp = float( std::sin( double( pitch ) ) ), cp = float( std::cos( double( pitch ) ) );
	const float sy = float( std::sin( double( yaw ) ) ), cy = float( std::cos( double( yaw ) ) );
	const float sr = float( std::sin( double( roll ) ) ), cr = float( std::cos( double( roll ) ) );
	const Vector forward{ cp * cy, cp * sy, -sp };
	const Vector right{ -sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp };
	const Vector up{ cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };
	const Vector left{ -right[0], -right[1], -right[2] };
	return { forward[0], left[0], up[0], origin[0],
		forward[1], left[1], up[1], origin[1],
		forward[2], left[2], up[2], origin[2] };
}
/* A rigid matrix's inverse: the rotation transposed, the translation undone. */
inline Matrix Inverse( const Matrix &m ) {
	Matrix out{};
	for ( int row = 0; row < 3; ++row ) {
		for ( int column = 0; column < 3; ++column ) out[row * 4 + column] = m[column * 4 + row];
	}
	for ( int row = 0; row < 3; ++row ) {
		float t = 0.0f;
		for ( int k = 0; k < 3; ++k ) t -= out[row * 4 + k] * m[k * 4 + 3];
		out[row * 4 + 3] = t;
	}
	return out;
}
inline Vector Rotate( const Matrix &m, const Vector &v ) {
	return { m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
		m[4] * v[0] + m[5] * v[1] + m[6] * v[2],
		m[8] * v[0] + m[9] * v[1] + m[10] * v[2] };
}
inline Vector Place( const Matrix &m, const Vector &v ) {
	Vector out = Rotate( m, v );
	out[0] += m[3]; out[1] += m[7]; out[2] += m[11];
	return out;
}

/* Retail's segment-triangle test (0x4ea120), accepting both faces. `normal` is
 * the unnormalised (B - A) x (C - A), and `facing` its dot with the ray. */
inline bool SegmentTriangle( const Vector &start, const Vector &end, const Vector &a,
	const Vector &b, const Vector &c, Vector &hit, Vector &normal, float &facing )
{
	normal = Cross( Subtract( b, a ), Subtract( c, a ) );
	Vector ray = Subtract( end, start );
	facing = Dot( ray, normal );
	if ( std::fabs( facing ) < ParallelBound ) return false;
	const float t = Dot( Subtract( a, start ), normal ) / facing;
	if ( t < 0.0f || t > 1.0f ) return false;
	hit = { ray[0] * t + start[0], ray[1] * t + start[1], ray[2] * t + start[2] };
	const Vector pa = Subtract( a, hit ), pb = Subtract( b, hit ), pc = Subtract( c, hit );
	if ( Dot( Cross( pa, pb ), normal ) < 0.0f ) return false;
	if ( Dot( Cross( pc, pa ), normal ) < 0.0f ) return false;
	if ( Dot( Cross( pb, pc ), normal ) < 0.0f ) return false;
	return true;
}

inline float TriangleArea( const Vector &a, const Vector &b, const Vector &c ) {
	return Length( Cross( Subtract( a, b ), Subtract( c, b ) ) );
}

/* Retail's sort of the records (0x52bc40, the C runtime's qsort, with
 * 0x4dfd10 as the order): the median-of-three quicksort that leaves runs of
 * eight or fewer to a selection sort. Retail's order answers "before" for a
 * smaller distance and "after" for anything else, equal included, so which
 * of two equally distant hits comes first is decided by this algorithm and
 * nothing else -- two triangles sharing the edge a ray passes through are
 * exactly that. Ported step for step for that reason. */
inline int Order( const Record &a, const Record &b ) { return a.distance < b.distance ? -1 : 1; }
inline void SelectionSort( Record *lo, Record *hi ) {
	while ( hi > lo ) {
		Record *max = lo;
		for ( Record *p = lo + 1; p <= hi; ++p ) if ( Order( *p, *max ) > 0 ) max = p;
		std::swap( *max, *hi );
		--hi;
	}
}
inline void Sort( Record *base, int count ) {
	if ( count < 2 ) return;
	Record *loStack[30], *hiStack[30];
	int stack = 0;
	Record *lo = base, *hi = base + ( count - 1 );
	for ( ;; ) {
		const std::ptrdiff_t size = hi - lo + 1;
		if ( size <= 8 ) {
			SelectionSort( lo, hi );
		} else {
			Record *mid = lo + size / 2;
			if ( Order( *lo, *mid ) > 0 ) std::swap( *lo, *mid );
			if ( Order( *lo, *hi ) > 0 ) std::swap( *lo, *hi );
			if ( Order( *mid, *hi ) > 0 ) std::swap( *mid, *hi );
			Record *loGuy = lo, *hiGuy = hi;
			for ( ;; ) {
				if ( mid > loGuy ) {
					do { ++loGuy; } while ( loGuy < mid && Order( *loGuy, *mid ) <= 0 );
				}
				if ( mid <= loGuy ) {
					do { ++loGuy; } while ( loGuy <= hi && Order( *loGuy, *mid ) <= 0 );
				}
				do { --hiGuy; } while ( hiGuy > mid && Order( *hiGuy, *mid ) > 0 );
				if ( hiGuy < loGuy ) break;
				std::swap( *loGuy, *hiGuy );
				if ( mid == hiGuy ) mid = loGuy;
			}
			++hiGuy;
			if ( mid < hiGuy ) {
				do { --hiGuy; } while ( hiGuy > mid && Order( *hiGuy, *mid ) == 0 );
			}
			if ( mid >= hiGuy ) {
				do { --hiGuy; } while ( hiGuy > lo && Order( *hiGuy, *mid ) == 0 );
			}
			if ( hiGuy - lo >= hi - loGuy ) {
				if ( lo < hiGuy ) { loStack[stack] = lo; hiStack[stack] = hiGuy; ++stack; }
				if ( loGuy < hi ) { lo = loGuy; continue; }
			} else {
				if ( loGuy < hi ) { loStack[stack] = loGuy; hiStack[stack] = hi; ++stack; }
				if ( lo < hiGuy ) { hi = hiGuy; continue; }
			}
		}
		if ( --stack < 0 ) return;
		lo = loStack[stack]; hi = hiStack[stack];
	}
}

enum class Error { None, Mesh, Numeric };

namespace detail {
struct Walk {
	const Query *query = nullptr;
	Matrix world{};
	Vector start{}, end{};   /* the ray in model space */
	Vector scale{};
	Records *records = nullptr;
	int modelIndex = 0;
	const SurfaceOverrides *overrides = nullptr;
	bool stop = false;
	std::vector<Vector> scaled;
	std::vector<int> outside;

	Record *Free() {
		for ( Record &record : *records ) if ( record.entityNum == -1 ) return &record;
		stop = true;
		return nullptr;
	}
	void Fill( Record &record, int surface, int poly ) const {
		record.polyIndex = poly;
		record.entityNum = query->entityNum;
		record.surfaceIndex = surface;
		record.modelIndex = modelIndex;
	}

	/* 0x4eb1c0: the point trace. */
	void Segment( const geometry::DeformedSurface &surface ) {
		for ( std::size_t t = 0; t < surface.triangles.size() && !stop; ++t ) {
			const auto &corners = surface.triangles[t];
			const Vector &a = scaled[corners[0]], &b = scaled[corners[1]], &c = scaled[corners[2]];
			Vector hit{}, normal{};
			float facing = 0.0f;
			if ( !SegmentTriangle( start, end, a, b, c, hit, normal, facing ) ) continue;
			Record *record = Free();
			if ( !record ) return;
			Fill( *record, surface.metadata.index, int( t ) );
			record->flags = facing > 0.0f ? FrontFace : BackFace;
			record->distance = Length( Subtract( hit, start ) );
			record->position = Place( world, hit );
			record->normal = Rotate( world, normal );
			Normalise( record->normal );
			record->material = record->location = 0;
			const float area = TriangleArea( a, b, c );
			record->barycentricI = TriangleArea( hit, b, c ) / area;
			record->barycentricJ = TriangleArea( a, hit, c ) / area;
		}
	}

	/* 0x4eb4a0: the radius trace. Each vertex is placed in a box around the
	 * swept line -- across it by the radius, along it from start to end --
	 * and a triangle is a hit unless all three of its corners lie beyond one
	 * and the same face of that box. */
	void Radius( const geometry::DeformedSurface &surface ) {
		Vector ray = Subtract( end, start );
		Vector up{ 0.0f, 0.0f, 1.0f };
		Vector across = Cross( ray, up );
		if ( Dot( across, across ) < 0.1f ) {
			up = { 0.0f, 1.0f, 0.0f };
			across = Cross( ray, up );
		}
		up = Cross( ray, across );
		Normalise( across );
		Normalise( up );
		const float half = 0.5f / query->radius;
		const Vector tAxis{ across[0] * half, across[1] * half, across[2] * half };
		const Vector sAxis{ up[0] * half, up[1] * half, up[2] * half };
		const float lengthSquared = Dot( ray, ray );
		ray = { ray[0] / lengthSquared, ray[1] / lengthSquared, ray[2] / lengthSquared };
		outside.assign( surface.vertices.size(), 0 );
		int all = 63;
		for ( std::size_t v = 0; v < surface.vertices.size(); ++v ) {
			const Vector delta = Subtract( scaled[v], start );
			const float s = Dot( delta, sAxis ) + 0.5f;
			const float t = Dot( delta, tAxis ) + 0.5f;
			const float u = Dot( delta, ray );
			int inside = 0;
			if ( s > 0.0f ) inside |= 1;
			if ( s < 1.0f ) inside |= 2;
			if ( t > 0.0f ) inside |= 4;
			if ( t < 1.0f ) inside |= 8;
			if ( u > 0.0f ) inside |= 16;
			if ( u < 1.0f ) inside |= 32;
			outside[v] = ~inside;
			all &= ~inside;
		}
		if ( all ) return;
		const Vector direction = Subtract( end, start );
		for ( std::size_t t = 0; t < surface.triangles.size() && !stop; ++t ) {
			const auto &corners = surface.triangles[t];
			if ( 63 & outside[corners[0]] & outside[corners[1]] & outside[corners[2]] ) continue;
			Record *record = Free();
			if ( !record ) return;
			Fill( *record, surface.metadata.index, int( t ) );
			record->flags = FrontFace;
			const Vector &a = scaled[corners[0]], &b = scaled[corners[1]], &c = scaled[corners[2]];
			const Vector normal = Cross( Subtract( b, a ), Subtract( c, a ) );
			record->normal = Rotate( world, normal );
			Normalise( record->normal );
			record->material = record->location = 0;
			/* Where the swept line meets the triangle's plane. */
			const float third = -( a[0] * ( b[1] * c[2] - c[1] * b[2] )
				+ b[0] * ( c[1] * a[2] - a[1] * c[2] ) + c[0] * ( a[1] * b[2] - b[1] * a[2] ) );
			const float side = normal[0] * start[0] + normal[1] * start[1] + normal[2] * start[2] + third;
			const float along = Dot( normal, direction );
			const float distance = side / along;
			const Vector hit{ start[0] - distance * direction[0], start[1] - distance * direction[1],
				start[2] - distance * direction[2] };
			record->distance = Length( Subtract( hit, start ) );
			record->position = Place( world, hit );
			record->barycentricI = record->barycentricJ = 0.0f;
		}
	}

	void Surfaces( const geometry::DeformedGeometry &mesh, int index ) {
		if ( stop ) return;
		const auto &surface = mesh.surfaces[std::size_t( index )];
		std::uint32_t flags = surface.metadata.flags;
		if ( overrides ) {
			const auto listed = overrides->find( int( surface.metadata.index ) );
			if ( listed != overrides->end() ) flags = listed->second;
		}
		if ( !flags ) {
			scaled.resize( surface.vertices.size() );
			for ( std::size_t v = 0; v < surface.vertices.size(); ++v ) {
				const auto &p = surface.vertices[v].position;
				scaled[v] = { p[0] * scale[0], p[1] * scale[1], p[2] * scale[2] };
			}
			if ( std::fabs( double( query->radius ) ) < PointRadius ) Segment( surface );
			else Radius( surface );
		}
		if ( flags & NoDescendants ) return;
		for ( int child : surface.metadata.children ) {
			if ( stop ) return;
			Surfaces( mesh, child );
		}
	}
};
} // namespace detail

/* Trace one instance's models into records the caller keeps, as
 * G2API_CollisionDetect (0x4e0170) does for the game's import 606 and the
 * client game's 299: nothing is reset, a record whose entity is -1 is a free
 * place, and what the caller left in the others stays. Then the records are
 * counted from the first up to the first free one, and that many are sorted
 * nearest first (0x4e0320..0x4e0395); the count is returned. A mesh that
 * cannot be walked leaves every record as it was. */
inline int Collide( const std::vector<Model> &models, const Query &query, Records &records,
	Error *error = nullptr )
{
	if ( error ) *error = Error::None;
	detail::Walk walk;
	walk.query = &query;
	walk.records = &records;
	walk.world = WorldMatrix( query.angles, query.origin );
	const Matrix inverse = Inverse( walk.world );
	walk.start = Place( inverse, query.start );
	walk.end = Place( inverse, query.end );
	for ( int axis = 0; axis < 3; ++axis ) {
		walk.scale[std::size_t( axis )] = query.scale[std::size_t( axis )] ? query.scale[std::size_t( axis )] : 1.0f;
	}
	for ( const Model &model : models ) {
		if ( !model.mesh || model.mesh->surfaces.empty() ) {
			if ( error ) *error = Error::Mesh;
			return 0;
		}
		for ( const auto &surface : model.mesh->surfaces ) {
			for ( const auto &corners : surface.triangles ) {
				for ( auto corner : corners ) {
					if ( corner >= surface.vertices.size() ) {
						if ( error ) *error = Error::Mesh;
						return 0;
					}
				}
			}
		}
	}
	for ( const Model &model : models ) {
		walk.modelIndex = model.index;
		walk.overrides = model.overrides;
		walk.Surfaces( *model.mesh, 0 );
		if ( walk.stop ) break;
	}
	int count = 0;
	while ( count < MaxRecords && records[std::size_t( count )].entityNum != -1 ) ++count;
	Sort( records.data(), count );
	return count;
}

/* Trace one instance's models with every record reset first, as the server
 * trace resets its array before each entity (0x1308780..0x1308b80). Returns
 * the number of records filled, sorted nearest first. */
inline int Detect( const std::vector<Model> &models, const Query &query, Records &records,
	Error *error = nullptr )
{
	for ( Record &record : records ) record = Record{};
	return Collide( models, query, records, error );
}
} // namespace fntech3::ghoul2::collision
#endif
