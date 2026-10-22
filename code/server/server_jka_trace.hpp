/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_TRACE_HPP
#define FNTECH3_SERVER_JKA_TRACE_HPP

/* Jedi Academy multiplayer's server trace, imports 27 and 28 (see
 * compat/jka_trace_clip.hpp for the routine and ghoul2/ghoul2_collision.hpp
 * for the Ghoul2 test), and its collision imports 606 and 607, served from
 * this engine's collision model and the title's own Ghoul2 store. See
 * docs/fntech3/2026-10-15-jka-g2-trace.md and
 * docs/fntech3/2026-10-22-jka-g2-collision.md. */
#include "server_raven_assets.hpp"
#include "../compat/jka_game_abi.h"
#include "../compat/jka_trace_clip.hpp"
#include "../ghoul2/ghoul2_instance_collision.hpp"

/* Each mesh at each level of detail a test has used, read once from the
 * server store's files, which it keeps for the life of the process. */
static fntech3::ghoul2::collision::GeometryCache jkaTraceGeometry;

/* Why the server could not ready an instance's models, named as each
 * refusal of the trace and of the collision imports names it. */
static void SV_JKAGhoul2Unready( const char *what, int number,
	const fntech3::ghoul2::Instance &instance, const fntech3::ghoul2::collision::Prepared &prepared )
{
	using fntech3::ghoul2::collision::Readiness;
	const fntech3::ghoul2::Model *model = instance.At( prepared.failedModel );
	switch ( prepared.readiness ) {
	case Readiness::NoMesh:
		Com_Error( ERR_DROP, "%s: model %d of entity %d (%s) has no mesh to trace", what,
			prepared.failedModel, number, model ? model->name.c_str() : "?" );
		break;
	case Readiness::Geometry:
		Com_Error( ERR_DROP, "%s: %s: %s", what, model ? model->name.c_str() : "?",
			fntech3::ghoul2::geometry::ErrorName( prepared.error ) );
		break;
	default:
		Com_Error( ERR_DROP, "%s: model %d of entity %d is %s", what, prepared.failedModel, number,
			fntech3::ghoul2::PosingName( prepared.posing ) );
		break;
	}
}

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
	 * Ghoul2 clock the bolt queries pose at. Every model is traced in the
	 * instance's own order (0x4ec1c0), each from the skeleton its build left.
	 * The server's models hold no surface overrides (import 609 is not
	 * served), so each walks its file's own flags. */
	collision::Prepared prepared;
	if ( !collision::Prepare( store, *instance, sv.time, lod, jkaTraceGeometry,
		[]( int ) -> const collision::SurfaceOverrides * { return nullptr; }, prepared ) ) {
		SV_JKAGhoul2Unready( "Jedi Academy Ghoul2 trace", number, *instance, prepared );
	}
	const std::vector<collision::Model> &models = prepared.models;
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

/* Game imports 606 and 607 (jamp.exe 1.01 0x45cde5 and 0x45ce4d): the
 * module's sixteen records, its instance, the angles -- all three, unlike the
 * trace's yaw alone -- the position, the time to pose at, the entity number
 * the records name, the ray, the scale, trace flags, the level of detail and
 * the radius, handed to G2API_CollisionDetect (0x4e0170) or its cached twin
 * (0x4dfe00). Neither resets the records nor answers anything. A null
 * instance faults in retail's setup (0x4e0c50 reads through it), so it is
 * refused; one holding no models fails that setup, which returns at once and
 * leaves the records untouched. The trace flags reach the walk's state
 * (0x4e94e0) and change no answer measured (the record above). */
static void SV_JKACollide( void *recordsAddress, void *ghoul2, const float angles[3],
	const float position[3], int time, int entityNum, const float start[3], const float end[3],
	const float scale[3], int lod, float radius )
{
	using namespace fntech3::ghoul2;
	if ( !ghoul2 ) {
		Com_Error( ERR_DROP, "Jedi Academy Ghoul2 collision was handed no instance" );
	}
	Store &store = ravenGhoul2Academy;
	const Instance *instance = store.Resolve( ghoul2 );
	if ( !instance ) {
		Com_Error( ERR_DROP, "Jedi Academy Ghoul2 collision was handed an instance this engine never "
			"handed out" );
	}
	if ( !instance->HoldsAny() ) return;
	if ( !std::isfinite( radius ) ) {
		Com_Error( ERR_DROP, "Jedi Academy Ghoul2 collision was handed a radius that is not finite" );
	}
	collision::Prepared prepared;
	if ( !collision::Prepare( store, *instance, time, lod, jkaTraceGeometry,
		[]( int ) -> const collision::SurfaceOverrides * { return nullptr; }, prepared ) ) {
		SV_JKAGhoul2Unready( "Jedi Academy Ghoul2 collision", entityNum, *instance, prepared );
	}
	collision::Query query;
	query.entityNum = entityNum;
	query.radius = radius;
	for ( int axis = 0; axis < 3; ++axis ) {
		query.angles[std::size_t( axis )] = angles[axis];
		query.origin[std::size_t( axis )] = position[axis];
		query.scale[std::size_t( axis )] = scale[axis];
		query.start[std::size_t( axis )] = start[axis];
		query.end[std::size_t( axis )] = end[axis];
	}
	collision::Records records;
	collision::ReadRecords( recordsAddress, records );
	collision::Error error = collision::Error::None;
	collision::Collide( prepared.models, query, records, &error );
	if ( error != collision::Error::None ) {
		Com_Error( ERR_DROP, "Jedi Academy Ghoul2 collision: entity %d has a mesh it cannot trace", entityNum );
	}
	collision::WriteRecords( recordsAddress, records );
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
