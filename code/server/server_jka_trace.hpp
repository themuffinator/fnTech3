/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_TRACE_HPP
#define FNTECH3_SERVER_JKA_TRACE_HPP

/* Jedi Academy multiplayer's server trace, imports 27 and 28 (see
 * compat/jka_trace_clip.hpp for the routine and ghoul2/ghoul2_collision.hpp
 * for the Ghoul2 test), served from this engine's collision model and the
 * title's own Ghoul2 store. See docs/fntech3/2026-10-15-jka-g2-trace.md. */
#include "server_raven_assets.hpp"
#include "../compat/jka_game_abi.h"
#include "../compat/jka_trace_clip.hpp"
#include "../ghoul2/ghoul2_collision.hpp"

#include <map>
#include <utility>
#include <vector>

/* Each mesh at each level of detail a trace has used, read once. The store
 * keeps the files themselves for the life of the process and never lets one
 * go (Store::Keep), so the key stays good and this grows no further than
 * the store's own files do. */
static std::map<std::pair<const std::vector<unsigned char> *, int>,
	fntech3::ghoul2::geometry::Geometry> jkaTraceGeometry;

static const fntech3_jka_sharedEntity_t *SV_JKATraceEntity( int number )
{
	if ( number < 0 || number >= sv.num_entities || !sv.gentities
		|| sv.gentitySize < int( sizeof( fntech3_jka_sharedEntity_t ) ) ) return nullptr;
	return reinterpret_cast<const fntech3_jka_sharedEntity_t *>( SV_GentityNum( number ) );
}

/* The Ghoul2 step for one entity (jamp.exe 1.01 0x4626f1..0x4628fd, then
 * G2API_CollisionDetect 0x4e0170): its instance posed at the server's level
 * time, each model at the level of detail asked for, traced in the entity's
 * frame. The frame is its origin and its yaw alone -- a client's from its
 * state's angle base, anything else's from its current angles (0x462790). */
static bool SV_JKATraceGhoul2( const fntech3_jka_sharedEntity_t &entity, int lod,
	const float start[3], const float end[3], float radius,
	fntech3::jka::trace::Ghoul2Hit &hit )
{
	using namespace fntech3::ghoul2;
	Store &store = ravenGhoul2Academy;
	const int number = entity.s.number;
	const Instance *instance = store.Resolve( entity.ghoul2 );
	if ( !instance ) {
		Com_Error( ERR_DROP, "Jedi Academy Ghoul2 trace: entity %d holds an instance this "
			"engine never handed out", number );
	}
	/* Retail's skeleton build (0x49a050) is handed the server's time word
	 * (0xc317c4) itself, the level time its game frames run at -- not the
	 * Ghoul2 clock the bolt queries pose at. */
	std::vector<int> order;
	std::vector<pose::Palette> palettes;
	int failed = -1;
	const Posing posing = store.InstancePalettes( *instance, sv.time, order, palettes, failed );
	if ( posing != Posing::Served ) {
		Com_Error( ERR_DROP, "Jedi Academy Ghoul2 trace: model %d of entity %d is %s",
			failed, number, PosingName( posing ) );
	}
	/* Retail traces every model in the instance's own order (0x4ec1c0), each
	 * from the skeleton its build left; a present model the build never
	 * reaches has no pose of this build to trace, so it is refused. */
	std::vector<geometry::DeformedGeometry> deformed( std::size_t( instance->Count() ) );
	std::vector<collision::Model> models;
	for ( int index = 0; index < instance->Count(); ++index ) {
		const Model *model = instance->At( index );
		if ( !model ) continue;
		std::size_t at = 0;
		while ( at < order.size() && order[at] != index ) ++at;
		if ( at == order.size() ) {
			Com_Error( ERR_DROP, "Jedi Academy Ghoul2 trace: model %d of entity %d is %s",
				index, number, PosingName( Posing::Unposed ) );
		}
		if ( !model->meshBytes || model->mesh.numLODs <= 0 ) {
			Com_Error( ERR_DROP, "Jedi Academy Ghoul2 trace: model %d of entity %d (%s) has "
				"no mesh to trace", index, number, model->name.c_str() );
		}
		/* G2_DecideTraceLod's rule: the level asked for, raised to the
		 * model's bias (never set on a server here, so 0) and capped at its
		 * last level. */
		int level = lod < 0 ? 0 : lod;
		if ( level >= model->mesh.numLODs ) level = model->mesh.numLODs - 1;
		const auto key = std::make_pair( model->meshBytes, level );
		auto found = jkaTraceGeometry.find( key );
		if ( found == jkaTraceGeometry.end() ) {
			geometry::Geometry loaded;
			const auto error = geometry::Load( model->meshBytes->data(), model->meshBytes->size(),
				model->skeleton.numBones, level, loaded );
			if ( error != geometry::Error::None ) {
				Com_Error( ERR_DROP, "Jedi Academy Ghoul2 trace: %s: %s", model->name.c_str(),
					geometry::ErrorName( error ) );
			}
			found = jkaTraceGeometry.emplace( key, std::move( loaded ) ).first;
		}
		const auto error = geometry::Deform( found->second, palettes[at].deformation,
			deformed[std::size_t( index )] );
		if ( error != geometry::Error::None ) {
			Com_Error( ERR_DROP, "Jedi Academy Ghoul2 trace: %s: %s", model->name.c_str(),
				geometry::ErrorName( error ) );
		}
		models.push_back( { index, &deformed[std::size_t( index )] } );
	}
	if ( models.empty() ) return false;

	collision::Query query;
	query.entityNum = number;
	query.radius = radius;
	query.angles[1] = number < fntech3::jka::trace::MaxClients ? entity.s.apos.trBase[1]
		: entity.r.currentAngles[1];
	for ( int axis = 0; axis < 3; ++axis ) {
		query.origin[std::size_t( axis )] = entity.r.currentOrigin[axis];
		query.scale[std::size_t( axis )] = entity.modelScale[axis];
		query.start[std::size_t( axis )] = start[axis];
		query.end[std::size_t( axis )] = end[axis];
	}
	collision::Records records;
	collision::Error error = collision::Error::None;
	const int count = collision::Detect( models, query, records, &error );
	if ( error != collision::Error::None ) {
		Com_Error( ERR_DROP, "Jedi Academy Ghoul2 trace: entity %d has a mesh it cannot trace", number );
	}
	if ( !count ) return false;
	const collision::Record &best = records[0];
	hit.entityNum = best.entityNum;
	hit.surfaceIndex = best.surfaceIndex;
	for ( int axis = 0; axis < 3; ++axis ) {
		hit.position[axis] = best.position[std::size_t( axis )];
		hit.normal[axis] = best.normal[std::size_t( axis )];
	}
	return true;
}

class ServerJKATraceWorld {
public:
	ServerJKATraceWorld( const float *start, const float *end, int lod )
		: start_( start ), end_( end ), lod_( lod ) {}

	void Sweep( trace_t &trace, const float *start, const float *end, const float *mins,
		const float *maxs, int mask )
	{
		CM_BoxTrace( &trace, start, end, mins, maxs, 0, mask, qfalse );
	}
	int Touching( const float *mins, const float *maxs, int *list, int capacity )
	{
		return SV_AreaEntities( mins, maxs, list, capacity );
	}
	bool Read( int number, fntech3::jka::trace::Entity &out )
	{
		const fntech3_jka_sharedEntity_t *entity = SV_JKATraceEntity( number );
		if ( !entity ) return false;
		out.number = entity->s.number;
		out.eType = entity->s.eType;
		out.eFlags = entity->s.eFlags;
		out.svFlags = entity->r.svFlags;
		out.contents = entity->r.contents;
		out.ownerNum = entity->r.ownerNum;
		out.ghoul2 = entity->ghoul2 != nullptr;
		return true;
	}
	void Clip( int number, trace_t &trace, const float *start, const float *end,
		const float *mins, const float *maxs, int mask )
	{
		const fntech3_jka_sharedEntity_t *entity = SV_JKATraceEntity( number );
		if ( !entity ) Com_Error( ERR_DROP, "Jedi Academy trace touched entity %d outside the game", number );
		const clipHandle_t model = SV_ClipHandleForEntity( SV_GentityNum( number ), qfalse );
		CM_TransformedBoxTrace( &trace, start, end, mins, maxs, model, mask,
			entity->r.currentOrigin, entity->r.bmodel ? entity->r.currentAngles : vec3_origin, qfalse );
		trace.entityNum = 0;
	}
	bool Ghoul2( int number, float radius, fntech3::jka::trace::Ghoul2Hit &hit )
	{
		const fntech3_jka_sharedEntity_t *entity = SV_JKATraceEntity( number );
		if ( !entity ) return false;
		return SV_JKATraceGhoul2( *entity, lod_, start_, end_, radius, hit );
	}

private:
	const float *start_;
	const float *end_;
	int lod_;
};

/* One trace for the game: `ghoul2Flags` 0 is import 27's. */
static void SV_JKATrace( trace_t &out, const float *start, const float *mins,
	const float *maxs, const float *end, int passEntityNum, int contentmask,
	int ghoul2Flags, int lod )
{
	ServerJKATraceWorld world( start, end, lod );
	if ( !fntech3::jka::trace::Run( world, out, start, mins, maxs, end, passEntityNum,
		contentmask, ghoul2Flags ) ) {
		Com_Error( ERR_DROP, "Jedi Academy trace cannot read pass entity %d or the entities "
			"it touched", passEntityNum );
	}
}
#endif
