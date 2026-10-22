/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GHOUL2_INSTANCE_COLLISION_HPP
#define FNTECH3_GHOUL2_INSTANCE_COLLISION_HPP

/* An instance's models made ready for a collision test, as Jedi Academy
 * multiplayer's G2API_CollisionDetect (jamp.exe 1.01 0x4e0170) readies them
 * before its walk: the skeleton built at the time it is handed (0x49a050,
 * which reads no clock of its own), each model's mesh deformed at its level of
 * detail (0x4e9ce0), and the models walked in the instance's own order
 * (0x4ec1c0). Measured from the retail executable; OpenJK
 * 1a6a643427aa347553e9073dac5570b33337c4d9 (codemp/rd-vanilla/G2_API.cpp,
 * G2_misc.cpp) only named the routines.
 *
 * The server trace (game import 28), the game's collision imports 606 and
 * 607 (handlers 0x45cde5 and 0x45ce4d) and the client game's 299 and 300
 * (0x4139ce and 0x413a36) all come here. 606 and 299 hand the routine the
 * module's own sixteen records; 607 and 300 hand the same words to its cached
 * twin (0x4dfe00), which keeps each model's deformed mesh for reuse while the
 * Ghoul2 clock stands. That reuse is served as the uncached test: the two
 * answer alike for the same arguments, which is measured, not inferred, in
 * docs/fntech3/2026-10-22-jka-g2-collision.md. */
#include "ghoul2_collision.hpp"
#include "ghoul2_server.hpp"

#include <cstring>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace fntech3::ghoul2::collision {

/* The module's record array: sixteen of retail's 64-byte records, the
 * collision imports' first argument. */
static_assert( sizeof( Record ) == 64 && sizeof( Records ) == 1024, "retail's collision records" );
static_assert( std::is_trivially_copyable_v<Record>, "records are copied as bytes" );
inline void ReadRecords( const void *from, Records &records )
{
	std::memcpy( records.data(), from, sizeof( Records ) );
}
inline void WriteRecords( void *to, const Records &records )
{
	std::memcpy( to, records.data(), sizeof( Records ) );
}

/* Each mesh at each level of detail a test has used, read once. A store
 * keeps its files for its own life (Store::Keep), so an entry is good while
 * the store it was read from is the one asking; any other store starts the
 * cache over. */
class GeometryCache {
public:
	const geometry::Geometry *Get( const Store &store, const ghoul2::Model &model, int level,
		geometry::Error &error )
	{
		if ( serial_ != store.Serial() ) {
			meshes_.clear();
			serial_ = store.Serial();
		}
		const auto key = std::make_pair( model.meshBytes, level );
		auto found = meshes_.find( key );
		if ( found == meshes_.end() ) {
			geometry::Geometry loaded;
			error = geometry::Load( model.meshBytes->data(), model.meshBytes->size(),
				model.skeleton.numBones, level, loaded );
			if ( error != geometry::Error::None ) return nullptr;
			found = meshes_.emplace( key, std::move( loaded ) ).first;
		}
		error = geometry::Error::None;
		return &found->second;
	}
	void Clear() { meshes_.clear(); serial_ = 0; }

private:
	std::uint64_t serial_ = 0;
	std::map<std::pair<const std::vector<unsigned char> *, int>, geometry::Geometry> meshes_;
};

/* Why an instance's models could not be made ready. */
enum class Readiness { Ready, Unposed, NoMesh, Geometry };

/* The models of one test. `models` points into `deformed`: keep this where
 * Prepare filled it. */
struct Prepared {
	std::vector<geometry::DeformedGeometry> deformed;
	std::vector<Model> models;
	Readiness readiness = Readiness::Ready;
	int failedModel = -1;
	Posing posing = Posing::Served;
	geometry::Error error = geometry::Error::None;
};

/* Pose every model of `instance` at `now` and deform each at `lod`, raised
 * to the model's bias -- which this engine's stores never set, so 0 -- and
 * capped at its last level (0x4ec3e2..0x4ec411), listing them in the
 * instance's order with `overrides( index )`, the model's surface override
 * list or null. A present model the store's pose order never reaches has no
 * pose of this build -- retail would walk whatever skeleton an earlier build
 * left in it -- so it is refused rather than guessed. */
template<class Overrides>
inline bool Prepare( const Store &store, const Instance &instance, int now, int lod,
	GeometryCache &cache, Overrides overrides, Prepared &out )
{
	out = Prepared{};
	std::vector<int> order;
	std::vector<pose::Palette> palettes;
	int failed = -1;
	out.posing = store.InstancePalettes( instance, now, order, palettes, failed );
	if ( out.posing != Posing::Served ) {
		out.readiness = Readiness::Unposed;
		out.failedModel = failed;
		return false;
	}
	out.deformed.resize( std::size_t( instance.Count() ) );
	for ( int index = 0; index < instance.Count(); ++index ) {
		const ghoul2::Model *model = instance.At( index );
		if ( !model ) continue;
		out.failedModel = index;
		std::size_t at = 0;
		while ( at < order.size() && order[at] != index ) ++at;
		if ( at == order.size() ) {
			out.readiness = Readiness::Unposed;
			out.posing = Posing::Unposed;
			return false;
		}
		if ( !model->meshBytes || model->mesh.numLODs <= 0 ) {
			out.readiness = Readiness::NoMesh;
			return false;
		}
		int level = lod < 0 ? 0 : lod;
		if ( level >= model->mesh.numLODs ) level = model->mesh.numLODs - 1;
		const geometry::Geometry *mesh = cache.Get( store, *model, level, out.error );
		if ( !mesh ) {
			out.readiness = Readiness::Geometry;
			return false;
		}
		out.error = geometry::Deform( *mesh, palettes[at].deformation, out.deformed[std::size_t( index )] );
		if ( out.error != geometry::Error::None ) {
			out.readiness = Readiness::Geometry;
			return false;
		}
		out.models.push_back( { index, &out.deformed[std::size_t( index )], overrides( index ) } );
	}
	out.failedModel = -1;
	return true;
}
} // namespace fntech3::ghoul2::collision
#endif
