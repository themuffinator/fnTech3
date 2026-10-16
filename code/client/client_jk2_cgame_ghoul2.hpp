/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_CGAME_GHOUL2_HPP
#define FNTECH3_CLIENT_JK2_CGAME_GHOUL2_HPP

// Include after client_jka_cgame_host.hpp (its Ghoul2 file reader) and
// client_jk2_cgame_services.hpp (its module spans).
#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

/* Jedi Outcast's client-game Ghoul2 imports 280-299, handlers 0x402b3f..
 * 0x402e45 in jk2mp.exe. Twelve reach the cores its game imports reach, which
 * the server already serves through the same store (server_jk2_game.hpp):
 *
 *   client  game  core       client  game  core
 *   281     584   0x4a7890   289     593   0x4a7d80
 *   283     586   0x4a7620   291     595   0x4a7bf0
 *   284     587   0x4a7620   292     596   0x4a7ce0
 *   285     588   0x4a6e90   295     589   0x4a75a0
 *   286     599   0x4a6e00   296     590   0x4a75f0
 *   287     591   0x4a7520
 *   288     592   0x4a7440
 *
 * The client owns its own store, with the capabilities the server's Outcast
 * store opts into (server_raven_assets.hpp) and retail's case-blind names
 * (0x4e2480). A QVM field holds a 32-bit word, and every word a module passes
 * back is the handle itself: retail converts none of them. 285's handler
 * clears the byte 588's sets (0x5084a0), so the instances are the client's. */
namespace {
std::unique_ptr<fntech3::ghoul2::Store> jk2CgameGhoul2Store;
constexpr std::size_t jk2CgameGhoul2FieldBytes = 4;

/* Entities with an instance wait for their view, as retail's drawing pass
 * poses them at the view's time (0x478704..0x478718). Each model keeps one
 * runtime registration, the shape Academy's client uses
 * (client_jka_cgame_render.hpp). */
struct JK2CgameGhoul2Scene {
	std::map<fntech3::jka::cgame::scene::Key, fntech3::jka::cgame::scene::Runtime> models;
	std::vector<fntech3_jk2_mp_refEntity_t> pending;
	std::vector<refEntity_t> prepared;
	std::vector<int> visible, traversal, order;
	std::vector<fntech3::ghoul2::pose::Palette> palettes;
	std::vector<unsigned char> bytes;
	std::vector<float> bones;
};
JK2CgameGhoul2Scene jk2CgameGhoul2Scene;

static fntech3::ghoul2::Store &CL_JK2CgameGhoul2Store() {
	if ( !jk2CgameGhoul2Store )
		jk2CgameGhoul2Store = std::make_unique<fntech3::ghoul2::Store>( jkaCgameGhoul2Files, true, false, true, true,
			fntech3::ghoul2::NameMatch::AsciiFolded );
	return *jk2CgameGhoul2Store;
}

static void CL_JK2CgameGhoul2Clear() {
	jk2CgameGhoul2Scene = JK2CgameGhoul2Scene{};
	jk2CgameGhoul2Store.reset();
}

static void *CL_JK2CgameGhoul2Handle( intptr_t word ) {
	return reinterpret_cast<void *>( std::uintptr_t( std::uint32_t( word ) ) );
}

/* Import 282 -> 0x4a78e0: one column of a 3x4 bolt matrix, by Raven's
 * orientation number (jump table 0x4a79bc): 0 the origin, 1 to 3 the x, z
 * and y axes, 4 to 6 the same negated. Any other number writes nothing. */
static void CL_JK2CgameGhoul2Vector( const intptr_t *args ) {
	float matrix[12];
	std::memcpy( matrix, CL_JK2CgameSpan( args[1], sizeof( matrix ) ), sizeof( matrix ) );
	void *destination = CL_JK2CgameSpan( args[3], sizeof( vec3_t ) );
	static constexpr int columns[7] = { 3, 0, 2, 1, 0, 2, 1 };
	if ( args[2] < 0 || args[2] > 6 ) return;
	const int column = columns[args[2]];
	const float sign = args[2] >= 4 ? -1.0f : 1.0f;
	const float vector[3] = { sign * matrix[column], sign * matrix[4 + column], sign * matrix[8 + column] };
	std::memcpy( destination, vector, sizeof( vector ) );
}

/* One runtime registration per model of an instance, rebuilt when its mesh
 * or visible surfaces change. A surface the model file switches off (0x2)
 * is not drawn, and one that stops its descendants (0x100) hides them too:
 * the flags Academy's client honours from the same file format. */
static fntech3::jka::cgame::scene::Runtime &CL_JK2CgameGhoul2Geometry( std::uint32_t identity, int index,
	const fntech3::ghoul2::Model &model ) {
	auto &scene = jk2CgameGhoul2Scene;
	for ( auto at = scene.models.begin(); at != scene.models.end(); ) {
		const auto *owner = jk2CgameGhoul2Store->Resolve( CL_JK2CgameGhoul2Handle( at->first.first ) );
		if ( !owner || !owner->At( at->first.second ) ) at = scene.models.erase( at );
		else ++at;
	}
	const fntech3::jka::cgame::scene::Key key{ identity, index };
	auto at = scene.models.find( key );
	if ( at != scene.models.end() && at->second.meshBytes != model.meshBytes ) {
		scene.models.erase( at );
		at = scene.models.end();
	}
	if ( at == scene.models.end() ) {
		if ( scene.models.size() >= fntech3::jka::cgame::scene::MaxModels )
			Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 renderer cache is full" );
		at = scene.models.emplace( key, fntech3::jka::cgame::scene::Runtime{} ).first;
		const auto error = fntech3::ghoul2::geometry::Load( model.meshBytes->data(), model.meshBytes->size(),
			model.skeleton.numBones, 0, at->second.geometry );
		if ( error != fntech3::ghoul2::geometry::Error::None )
			Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 %s (%s)", fntech3::ghoul2::geometry::ErrorName( error ),
				model.name.c_str() );
		at->second.meshBytes = model.meshBytes;
	}
	auto &runtime = at->second;
	scene.visible.clear();
	scene.traversal.clear();
	for ( auto root = runtime.geometry.roots.rbegin(); root != runtime.geometry.roots.rend(); ++root )
		scene.traversal.push_back( *root );
	while ( !scene.traversal.empty() ) {
		const int surfaceIndex = scene.traversal.back();
		scene.traversal.pop_back();
		const auto &surface = runtime.geometry.surfaces[std::size_t( surfaceIndex )];
		const auto flags = surface.metadata.flags;
		if ( flags & ~std::uint32_t( 0x103 ) )
			Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 surface flags 0x%x are not served yet (%s)",
				static_cast<unsigned>( flags ), model.name.c_str() );
		if ( !flags && !surface.triangles.empty() ) scene.visible.push_back( surfaceIndex );
		if ( !( flags & 0x100 ) )
			for ( auto child = surface.metadata.children.rbegin(); child != surface.metadata.children.rend(); ++child )
				scene.traversal.push_back( *child );
	}
	if ( runtime.visible != scene.visible ) {
		runtime.visible = scene.visible;
		runtime.handle = 0;
		runtime.owner = 0;
	}
	return runtime;
}
} // namespace

/* Import 201 with an instance (the record's word at 208). Retail's drawing
 * pass (0x478320) places the instance from the record's angles and origin
 * (G2_GenerateWorldMatrix 0x4abdb0), not its axes, culls it by its largest
 * model scale (0x478dc0) and picks each model's material: the record's
 * custom shader (+0x4c), else the model's own skin from its init (+0x38),
 * else the record's skin (+0x88). */
static void CL_JK2CgameGhoul2Queue( const fntech3_jk2_mp_refEntity_t &source ) {
	auto plain = source;
	plain.ghoul2 = 0;
	refEntity_t canonical{};
	const auto status = fntech3::jk2::render::Entity( plain, canonical );
	if ( status != fntech3::jk2::render::Status::Ok )
		Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 entity: %s (type=%d, flags=0x%x)",
			fntech3::jk2::render::Description( status ), static_cast<int>( source.reType ),
			static_cast<unsigned>( source.renderfx ) );
	if ( !fntech3::jk2::render::Finite( source.angles ) || !fntech3::jk2::render::Finite( source.modelScale ) )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame Ghoul2 placement" );
	for ( const float scale : source.modelScale )
		if ( scale != 0 && scale != 1 ) Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 model scale is not served yet" );
	if ( !CL_JK2CgameGhoul2Store().Resolve( CL_JK2CgameGhoul2Handle( source.ghoul2 ) ) )
		Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 entity names no instance the client made" );
	if ( jk2CgameGhoul2Scene.pending.size() >= fntech3::jka::cgame::scene::MaxModels )
		Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 scene exceeds its entity budget" );
	jk2CgameGhoul2Scene.pending.push_back( source );
}

static void CL_JK2CgameGhoul2DropScene() {
	jk2CgameGhoul2Scene.pending.clear();
}

/* Import 207, before the view is drawn: every waiting instance is posed at
 * the view's time, as retail's pass rebuilds a model whose last build time
 * is not the scene's (0x478704). Each model becomes its own Fn skeletal
 * entity, so Fn's lighting, shadows and materials apply to it. Retail's
 * frame-to-frame bone smoothing (0x478978..0x478a6a) is not served. */
static void CL_JK2CgameGhoul2Flush( int time ) {
	auto &scene = jk2CgameGhoul2Scene;
	if ( scene.pending.empty() ) return;
	if ( !re.RegisterRuntimeMDR || !re.UpdateRuntimeMDRPose )
		Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 needs Fn's skeletal renderer" );
	auto &store = CL_JK2CgameGhoul2Store();
	scene.prepared.clear();
	for ( const auto &source : scene.pending ) {
		const auto identity = static_cast<std::uint32_t>( source.ghoul2 );
		const fntech3::ghoul2::Instance *instance = store.Resolve( CL_JK2CgameGhoul2Handle( identity ) );
		if ( !instance ) Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 instance went away before its view" );
		int failed = -1;
		const auto posing = store.InstancePalettes( *instance, time, scene.order, scene.palettes, failed );
		if ( posing != fntech3::ghoul2::Posing::Served )
			Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 model %d is %s", failed, fntech3::ghoul2::PosingName( posing ) );
		auto plain = source;
		plain.ghoul2 = 0;
		refEntity_t canonical{};
		if ( fntech3::jk2::render::Entity( plain, canonical ) != fntech3::jk2::render::Status::Ok )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame Ghoul2 entity" );
		vec3_t angles;
		std::memcpy( angles, source.angles, sizeof( angles ) );
		AnglesToAxis( angles, canonical.axis );
		canonical.nonNormalizedAxes = qfalse;
		canonical.frame = canonical.oldframe = 0;
		canonical.backlerp = 0;
		canonical.customShader = 0;
		canonical.customSkin = 0;
		for ( std::size_t at = 0; at < scene.order.size(); ++at ) {
			const int index = scene.order[at];
			const auto &model = *instance->At( index );
			if ( !model.meshBytes || !model.skeletonBytes )
				Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 model %d (%s) has no paired skeleton", index, model.name.c_str() );
			auto &runtime = CL_JK2CgameGhoul2Geometry( identity, index, model );
			if ( runtime.visible.empty() ) continue;
			const auto &pose = scene.palettes[at];
			if ( !runtime.handle ) {
				scene.bytes.clear();
				const auto error = fntech3::ghoul2::mdr::Build( runtime.geometry, runtime.visible, pose.deformation, scene.bytes );
				if ( error != fntech3::ghoul2::mdr::Error::None )
					Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 %s", fntech3::ghoul2::mdr::ErrorName( error ) );
				const auto owner = fntech3::jka::mdr::NextOwner();
				if ( !owner ) Com_Error( ERR_DROP, "Jedi Outcast Ghoul2 renderer identity space is exhausted" );
				const auto handle = re.RegisterRuntimeMDR( owner, scene.bytes.data(), static_cast<int>( scene.bytes.size() ) );
				if ( !handle ) Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 Fn skeletal registration failed" );
				runtime.owner = owner;
				runtime.handle = handle;
			} else {
				fntech3::ghoul2::geometry::Vector mins, maxs;
				const auto error = fntech3::ghoul2::mdr::Bounds( runtime.geometry, runtime.visible, pose.deformation, mins, maxs );
				if ( error != fntech3::ghoul2::mdr::Error::None )
					Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 %s", fntech3::ghoul2::mdr::ErrorName( error ) );
				scene.bones.clear();
				scene.bones.reserve( pose.deformation.size() * 12 );
				for ( const auto &bone : pose.deformation ) scene.bones.insert( scene.bones.end(), bone.begin(), bone.end() );
				if ( !re.UpdateRuntimeMDRPose( runtime.handle, runtime.owner, scene.bones.data(),
					static_cast<int>( pose.deformation.size() ), mins.data(), maxs.data() ) )
					Com_Error( ERR_DROP, "Jedi Outcast cgame Ghoul2 pose is stale or invalid" );
			}
			auto entity = canonical;
			entity.hModel = runtime.handle;
			if ( source.customShader ) entity.customShader = source.customShader;
			else entity.customSkin = model.customSkin ? model.customSkin : source.customSkin;
			scene.prepared.push_back( entity );
		}
	}
	// Publish only once every waiting entity has a validated pose.
	for ( const auto &entity : scene.prepared ) CL_AddEnemyHighlightRefEntity( &entity, qfalse );
	scene.pending.clear();
	scene.prepared.clear();
}

static bool CL_JK2CgameGhoul2Call( intptr_t *args, intptr_t &result ) {
	if ( args[0] < 278 || args[0] > 299 ) return false;
	fntech3::ghoul2::Store &store = CL_JK2CgameGhoul2Store();
	result = 0;
	switch ( args[0] ) {
	case 280: // 0x402b3f -> 0x4a78d0, which only returns.
		return true;
	case 281: { // 0x402b2d -> 0x4a7890: whether any model is still there.
		const fntech3::ghoul2::Instance *instance = store.Resolve( CL_JK2CgameGhoul2Handle( args[1] ) );
		result = instance && instance->HoldsAny() ? 1 : 0;
		return true;
	}
	case 282:
		CL_JK2CgameGhoul2Vector( args );
		return true;
	case 283: case 284: {
		/* 0x402b77; 284 (0x402b6d) sets the no-rebuild flag first. Nine
		 * arguments, the game's order: ghoul2, model, bolt, matrix, angles,
		 * position, time, model list, scale. A bolt the store cannot answer
		 * writes nothing and returns 0 (server_jk2_game.hpp, 586). */
		if ( !args[4] || !args[5] || !args[6] ) return true;
		void *destination = CL_JK2CgameSpan( args[4], sizeof( float ) * 12 );
		float angles[3], position[3], scale[3];
		std::memcpy( angles, CL_JK2CgameSpan( args[5], sizeof( angles ) ), sizeof( angles ) );
		std::memcpy( position, CL_JK2CgameSpan( args[6], sizeof( position ) ), sizeof( position ) );
		if ( args[9] ) std::memcpy( scale, CL_JK2CgameSpan( args[9], sizeof( scale ) ), sizeof( scale ) );
		float kept[12];
		if ( store.KeptBolt( CL_JK2CgameGhoul2Handle( args[1] ), static_cast<int>( args[2] ),
			static_cast<int>( args[3] ), static_cast<int>( args[7] ), args[0] == 284, kept ) != fntech3::ghoul2::Posing::Served )
			return true;
		float world[12];
		if ( !fntech3::ghoul2::pose::PlaceOutcastBolt( kept, angles, position, args[9] ? scale : nullptr, world ) )
			return true;
		std::memcpy( destination, world, sizeof( world ) );
		result = 1;
		return true;
	}
	case 285: { // 0x402bd6 -> 0x4a6e90: the module's field and a model name.
		if ( !args[1] ) { result = -1; return true; }
		auto **slot = static_cast<void **>( CL_JK2CgameSpan( args[1], jk2CgameGhoul2FieldBytes ) );
		const char *name = CL_JK2CgameString( args[2], MAX_QPATH );
		fntech3::ghoul2::LoadError error = fntech3::ghoul2::LoadError::None;
		const int index = store.AddModel( slot, name, error, jk2CgameGhoul2FieldBytes );
		if ( index < 0 ) Com_Printf( "Ghoul2: %s: %s\n", name, fntech3::ghoul2::LoadErrorName( error ) );
		// The core keeps its fourth word, a skin handle, as the model's own
		// skin (0x4a7051, +0x38), which the drawing pass prefers.
		else if ( args[4] > 0 && args[4] <= INT_MAX )
			store.SetSkin( fntech3::ghoul2::ReadField( slot, jk2CgameGhoul2FieldBytes ), index, static_cast<int>( args[4] ) );
		result = index;
		return true;
	}
	case 286: // 0x402c6e -> 0x4a6e00: empty a field; returns 0.
		if ( args[1] ) store.CleanField( CL_JK2CgameSpan( args[1], jk2CgameGhoul2FieldBytes ), jk2CgameGhoul2FieldBytes );
		return true;
	case 287: { // 0x402c19 -> 0x4a7520: eleven arguments, the game's 591 order.
		float angles[3];
		std::memcpy( angles, CL_JK2CgameSpan( args[4], sizeof( angles ) ), sizeof( angles ) );
		result = store.SetBoneAngles( CL_JK2CgameGhoul2Handle( args[1] ), static_cast<int>( args[2] ),
			CL_JK2CgameString( args[3], MAX_QPATH ), angles, static_cast<int>( args[5] ), static_cast<int>( args[6] ),
			static_cast<int>( args[7] ), static_cast<int>( args[8] ), static_cast<int>( args[10] ),
			static_cast<int>( args[11] ) ) ? 1 : 0;
		return true;
	}
	case 288: // 0x402c88 -> 0x4a7440: ten arguments, the game's 592 order.
		result = store.SetBoneAnim( CL_JK2CgameGhoul2Handle( args[1] ), static_cast<int>( args[2] ),
			CL_JK2CgameString( args[3], MAX_QPATH ), static_cast<int>( args[4] ), static_cast<int>( args[5] ),
			static_cast<int>( args[6] ), _vmf( args[7] ), static_cast<int>( args[8] ), _vmf( args[9] ),
			static_cast<int>( args[10] ) ) ? 1 : 0;
		return true;
	case 289: { // 0x402cc7 -> 0x4a7d80: the skeleton's name, copied whole; returns 0.
		const char *name = store.GLAName( CL_JK2CgameGhoul2Handle( args[1] ), static_cast<int>( args[2] ) );
		if ( name ) {
			const std::size_t bytes = std::strlen( name ) + 1;
			std::memcpy( CL_JK2CgameSpan( args[3], bytes ), name, bytes );
		}
		return true;
	}
	case 291: // 0x402d1c -> 0x4a7bf0: one model into a place in another; returns 0.
		store.CopyModel( CL_JK2CgameGhoul2Handle( args[1] ), static_cast<int>( args[2] ),
			CL_JK2CgameGhoul2Handle( args[3] ), static_cast<int>( args[4] ) );
		return true;
	case 292: // 0x402d3c -> 0x4a7ce0: a whole instance into an empty field; returns 0.
		if ( args[2] )
			store.DuplicateInto( CL_JK2CgameGhoul2Handle( args[1] ), CL_JK2CgameSpan( args[2], jk2CgameGhoul2FieldBytes ),
				jk2CgameGhoul2FieldBytes );
		return true;
	case 293: { // 0x402d5a -> 0x4a72a0: a field, and whether its model there is present.
		const void *field = CL_JK2CgameSpan( args[1], jk2CgameGhoul2FieldBytes );
		const fntech3::ghoul2::Instance *instance =
			store.Resolve( fntech3::ghoul2::ReadField( field, jk2CgameGhoul2FieldBytes ) );
		result = instance && instance->At( static_cast<int>( args[2] ) ) ? 1 : 0;
		return true;
	}
	case 295: // 0x402d98 -> 0x4a75a0: a surface or bone by name.
		result = store.AddBolt( CL_JK2CgameGhoul2Handle( args[1] ), static_cast<int>( args[2] ),
			CL_JK2CgameString( args[3], MAX_QPATH ) );
		return true;
	case 296: // 0x402db8 -> 0x4a75f0; returns 0.
		store.SetBoltInfo( CL_JK2CgameGhoul2Handle( args[1] ), static_cast<int>( args[2] ), static_cast<int>( args[3] ) );
		return true;
	case 294: case 297: case 298: case 299: {
		static const char *const names[] = { "model removal", "", "", "root surface", "surface visibility", "new origin" };
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import %ld: Jedi Outcast's Ghoul2 %s is not served yet",
			static_cast<long>( args[0] ), names[args[0] - 294] );
		return true;
	}
	default: // 278-279 list surfaces and bones, 290 copies an instance: no call site.
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import %ld: Jedi Outcast's Ghoul2 boundary is open.",
			static_cast<long>( args[0] ) );
		return true;
	}
}
#endif
