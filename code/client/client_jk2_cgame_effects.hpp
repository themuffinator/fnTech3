/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_CGAME_EFFECTS_HPP
#define FNTECH3_CLIENT_JK2_CGAME_EFFECTS_HPP

// Include after client_jka_cgame_host.hpp and client_jk2_cgame_services.hpp.
#include <cmath>
#include <cstddef>
#include <cstring>

/* Jedi Outcast's effects imports, handlers 0x402722..0x40289b in jk2mp.exe.
 * Its effects engine is the design Academy's grew from: a scheduler
 * (0x9e4710) and helper (0x9eb7b8) over a pool of 1800 primitives, the same
 * thirteen primitive kinds and the same registration rules (0x415f70: a
 * lower-cased cache key, ".efx" by default, "effects/" unless the name starts
 * with it, files under 65535 bytes). Fn plays Outcast's effect files through
 * the engine it built for Academy (client_jka_cgame_effects.hpp), which draws
 * them as Fn sprites, polygons and dynamic lights; this adapter supplies what
 * reaches Outcast's own module and view:
 *  - traces go to client-game export 16 through the shared buffer (0x4126a0;
 *    skip -1, solid mask 1). Fn traces a point, as it does for Academy;
 *    retail passes a particle's own bounds when it asks for them;
 *  - decals go to client-game export 23, the module's own impact marks
 *    (0x418989..0x418a0d), not to a renderer pool as Academy's do;
 *  - camera shakes do nothing: retail's helper (0x41ace0) measures the
 *    distance to the view and discards it;
 *  - sounds start at the spawn origin on no entity's automatic channel
 *    (0x418b26), as Academy's do;
 *  - the view is the origin import 265 hands over each frame (0x41ac60).
 * Outcast's effect files have not been compared against jk2mp.exe primitive
 * by primitive; they are read with Academy's measured rules. */
namespace {
fntech3::jka::effectplay::Vector jk2EffectsEye{};

static void CL_JK2CgameEffectsPlayable() {
	if ( !cgvm || cgvm->entryPoint || cgvm->dllExports || cgvm->index != VM_CGAME )
		Com_Error( ERR_DROP, "Jedi Outcast effects playback requires its QVM client game" );
}

static fntech3_jka_trace_t CL_JK2CgameEffectsTrace( const fntech3::jka::effectplay::Vector &start,
	const fntech3::jka::effectplay::Vector &end ) {
	const auto *owner = jkaCgameEffects.get();
	const auto revision = jkaCgameEffectsRevision;
	auto *const module = cgvm;
	const intptr_t shared = jk2CgameSharedAddress;
	if ( !shared ) Com_Error( ERR_DROP, "Jedi Outcast effects trace has no shared buffer" );
	void *destination = CL_JK2CgameSpan( shared, sizeof( fntech3_jk2_mp_cgame_trace_t ) );
	fntech3_jk2_mp_cgame_trace_t request{};
	std::memcpy( request.start, start.data(), sizeof( request.start ) );
	std::memcpy( request.end, end.data(), sizeof( request.end ) );
	request.skipEntity = -1;
	request.mask = 1;
	// Only the request crosses: retail's export reads it after the result.
	const auto offset = offsetof( fntech3_jk2_mp_cgame_trace_t, start );
	std::memcpy( static_cast<byte *>( destination ) + offset, reinterpret_cast<const byte *>( &request ) + offset,
		sizeof( request ) - offset );
	jkaCgameEffectsTracing = true;
	VM_Call( module, 0, FNTECH3_JK2_MP_CG_TRACE );
	jkaCgameEffectsTracing = false;
	if ( cgvm != module || jk2CgameSharedAddress != shared || jkaCgameEffects.get() != owner ||
		jkaCgameEffectsRevision != revision )
		Com_Error( ERR_DROP, "Jedi Outcast effects trace changed its owning context" );
	fntech3_jk2_mp_trace_t result;
	std::memcpy( &result, CL_JK2CgameSpan( shared, sizeof( result ) ), sizeof( result ) );
	if ( !fntech3::jka::effectplay::Finite( result.fraction ) || result.fraction < 0 || result.fraction > 1 )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast effects trace fraction" );
	for ( int n = 0; n < 3; ++n )
		if ( !fntech3::jka::effectplay::Finite( result.endpos[n] ) || std::fabs( result.endpos[n] ) > 10000000.0f )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast effects trace endpoint" );
	// The fields the engine reads, in Academy's record.
	fntech3_jka_trace_t output{};
	output.allsolid = result.allsolid ? 1 : 0;
	output.startsolid = result.startsolid ? 1 : 0;
	output.entityNum = static_cast<int16_t>( result.entityNum );
	output.fraction = result.fraction;
	std::memcpy( output.endpos, result.endpos, sizeof( output.endpos ) );
	std::memcpy( output.plane.normal, result.normal, sizeof( output.plane.normal ) );
	output.plane.dist = result.dist;
	output.plane.type = result.type;
	output.plane.signbits = result.signbits;
	output.surfaceFlags = result.surfaceFlags;
	output.contents = result.contents;
	return output;
}

static void CL_JK2CgameEffectsCameraShake( const fntech3::jka::effectplay::Spawn & ) {}

static void CL_JK2CgameEffectsSound( const fntech3::jka::effectplay::Vector &origin, sfxHandle_t sound ) {
	vec3_t at;
	std::memcpy( at, origin.data(), sizeof( at ) );
	S_StartSound( at, fntech3::jka::effectplay::SoundEntity, fntech3::jka::effectplay::SoundChannel, sound );
}

static int CL_JK2CgameEffectsDecal( qhandle_t shader, const fntech3::jka::effectplay::Vector &origin,
	const fntech3::jka::effectplay::Vector &direction, float rotation, const float ( &color )[4], float radius ) {
	auto *const module = cgvm;
	const intptr_t shared = jk2CgameSharedAddress;
	const auto *owner = jkaCgameEffects.get();
	const auto revision = jkaCgameEffectsRevision;
	if ( !shared ) Com_Error( ERR_DROP, "Jedi Outcast effects decal has no shared buffer" );
	fntech3_jk2_mp_cgame_impact_mark_t mark{};
	mark.shader = shader;
	std::memcpy( mark.origin, origin.data(), sizeof( mark.origin ) );
	std::memcpy( mark.direction, direction.data(), sizeof( mark.direction ) );
	mark.rotation = rotation;
	std::memcpy( mark.rgb, color, sizeof( mark.rgb ) );
	mark.alpha = color[3];
	mark.radius = radius;
	std::memcpy( CL_JK2CgameSpan( shared, sizeof( mark ) ), &mark, sizeof( mark ) );
	jkaCgameEffectsTracing = true;
	VM_Call( module, 0, FNTECH3_JK2_MP_CG_IMPACT_MARK );
	jkaCgameEffectsTracing = false;
	if ( cgvm != module || jk2CgameSharedAddress != shared || jkaCgameEffects.get() != owner ||
		jkaCgameEffectsRevision != revision )
		Com_Error( ERR_DROP, "Jedi Outcast effects decal changed its owning context" );
	return 0; // the module cuts and keeps its own marks
}

static fntech3::jka::effectplay::Vector CL_JK2CgameEffectsEye() {
	return jk2EffectsEye;
}

const RavenEffectsHost jk2EffectsHost{ "Jedi Outcast", CL_JK2CgameEffectsPlayable, CL_JK2CgameEffectsTrace,
	CL_JK2CgameEffectsCameraShake, CL_JK2CgameEffectsSound, CL_JK2CgameEffectsDecal, CL_JK2CgameEffectsEye };

// Import 263 -> FX_Init 0x41d910: everything FX_Free 0x41d8d0 releases,
// registered effects included (CFxScheduler::Clean( true, 0 ) at 0x41d8da),
// then a fresh clock (0x41ac20); it returns 1.
static void CL_JK2CgameEffectsInit() {
	CL_JKACgameEffectsFree();
	jk2EffectsEye = {};
	CL_JKACgameEffectsInit( 0 );
}

// Import 265 -> 0x41ac60: the view, then the clock; no pause rule.
static void CL_JK2CgameEffectsAdjustTime( const intptr_t *args ) {
	if ( args[1] < INT_MIN || args[1] > INT_MAX ) Com_Error( ERR_DROP, "Invalid Jedi Outcast effects clock word" );
	fntech3::jka::effectplay::Vector eye;
	std::memcpy( eye.data(), CL_JK2CgameSpan( args[2], sizeof( vec3_t ) ), sizeof( vec3_t ) );
	CL_JK2CgameSpan( args[3], sizeof( vec3_t[3] ) ); // the axes are kept by retail and read by no served family
	if ( !fntech3::jka::effectplay::Finite( eye ) ) Com_Error( ERR_DROP, "Invalid Jedi Outcast effects view origin" );
	++jkaCgameEffectsRevision;
	auto &state = CL_JKACgameEffectsState();
	jk2EffectsEye = eye;
	const int time = static_cast<int>( args[1] );
	const uint32_t delta = static_cast<uint32_t>( time ) - static_cast<uint32_t>( state.time );
	state.previousTime = state.time;
	state.time = time;
	std::memcpy( &state.frameTime, &delta, sizeof( delta ) ); // retail's subtraction (0x41acc9)
	volatile float milliseconds = static_cast<float>( state.frameTime );
	volatile float secondsPerMillisecond = 0.001f;
	state.realTime = milliseconds * secondsPerMillisecond;
}

/* Imports 259 (0x4166a0: an effect, origin and forward direction) and 260
 * (0x416fd0: an effect, origin, three axes, a bolt word and an entity). A
 * bolt word other than -1 bolts the spawns to a model; it is refused by name.
 * Every primitive adder returns before creating anything unless the clock
 * moved forward at least a millisecond (0x41dc76, 0x41e0c6 and the rest). */
static void CL_JK2CgameEffectsPlay( int id, const fntech3::jka::effectplay::Vector &origin,
	const fntech3::jka::effectplay::Axis &axis ) {
	using namespace fntech3::jka;
	CL_JK2CgameEffectsPlayable();
	if ( !jkaCgameEffects || id < 1 || id >= int( effects::MaxEffects ) || std::size_t( id ) > jkaCgameEffects->effects.size() ||
		!jkaCgameEffects->effects[id - 1]->ready ) {
		// Retail prints "invalid effect ID" (0xf2f08) and plays nothing.
		Com_Printf( "Jedi Outcast effects playback has no owned declaration for ID%d\n", id );
		return;
	}
	auto &state = *jkaCgameEffects;
	if ( !state.initialized ) Com_Error( ERR_DROP, "Jedi Outcast effects playback requires an initialized system" );
	if ( !effectplay::Finite( origin ) ) Com_Error( ERR_DROP, "Invalid Jedi Outcast effects placement" );
	CL_JKACgameEffectCompileGraph( state, id );
	if ( state.debug && state.debug->integer >= 2 )
		Com_Printf( "Outcast FX play: %s at %d\n", state.effects[id - 1]->name.file.c_str(), state.time );
	if ( state.frameTime < 1 ) return;
	CL_JKACgameEffectExpire( state );
	state.spawnWork.clear(); state.slotWork.clear();
	auto random = state.random;
	CL_JKACgameEffectSpawn( state, id, origin, axis, random, 0 );
	CL_JKACgameEffectPublish( state );
	state.random = random;
}

static void CL_JK2CgameEffectsPlayArguments( const intptr_t *args, bool entity ) {
	using namespace fntech3::jka;
	if ( args[1] < INT_MIN || args[1] > INT_MAX ) Com_Error( ERR_DROP, "Invalid Jedi Outcast effect ID" );
	effectplay::Vector origin;
	std::memcpy( origin.data(), CL_JK2CgameSpan( args[2], sizeof( origin ) ), sizeof( origin ) );
	effectplay::Axis axis;
	if ( entity ) {
		float words[9];
		std::memcpy( words, CL_JK2CgameSpan( args[3], sizeof( words ) ), sizeof( words ) );
		if ( !effectplay::EntityAxis( words, axis ) ) Com_Error( ERR_DROP, "Invalid Jedi Outcast entity effect axis" );
		if ( static_cast<int>( args[4] ) != effectplay::EntityUnbolted )
			Com_Error( ERR_DROP, "Jedi Outcast entity effect bolt word %d is not served yet", static_cast<int>( args[4] ) );
	} else {
		effectplay::Vector forward;
		std::memcpy( forward.data(), CL_JK2CgameSpan( args[3], sizeof( forward ) ), sizeof( forward ) );
		if ( !effectplay::Finite( forward ) || !effectplay::Basis( forward, axis ) )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast effects direction" );
	}
	CL_JK2CgameEffectsPlay( static_cast<int>( args[1] ), origin, axis );
}

static const char *CL_JK2CgameEffectName( intptr_t address ) {
	return CL_JK2CgameString( address, fntech3::jka::effects::NameBytes );
}
} // namespace

static bool CL_JK2CgameEffectsCall( intptr_t *args, intptr_t &result ) {
	if ( jkaCgameEffectsTracing )
		Com_Error( ERR_DROP, "Jedi Outcast effects trace or decal cannot reenter its effect context" );
	ravenEffectsHost = &jk2EffectsHost;
	result = 0;
	switch ( args[0] ) {
	case 254: // 0x402722 -> 0x411760 -> RegisterEffect( name, 1 ) 0x415f70
		result = CL_JKACgameEffectsRegister( CL_JK2CgameEffectName( args[1] ) );
		return true;
	case 256: { // 0x402764 -> 0x4117a0 -> PlayEffect( name, origin, forward ) 0x4173b0
		const intptr_t id = CL_JKACgameEffectsRegister( CL_JK2CgameEffectName( args[1] ) );
		intptr_t played[4] = { 259, id, args[2], args[3] };
		CL_JK2CgameEffectsPlayArguments( played, false );
		return true;
	}
	case 259: CL_JK2CgameEffectsPlayArguments( args, false ); return true;
	case 260: CL_JK2CgameEffectsPlayArguments( args, true ); return true;
	case 262: CL_JKACgameEffectDrawScheduled( false ); return true; // 0x40287a -> 0x417440
	case 263: CL_JK2CgameEffectsInit(); result = 1; return true;
	case 264: CL_JKACgameEffectsFree(); result = 1; return true; // 0x41d8d0 returns true
	case 265: CL_JK2CgameEffectsAdjustTime( args ); return true;
	case 266: case 267: case 268: case 269: case 270: {
		static const char *const names[] = { "polygon", "Bezier", "primitive", "sprite", "electricity" };
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import %ld: Jedi Outcast's direct effect %s is not served yet",
			static_cast<long>( args[0] ), names[args[0] - 266] );
		return true;
	}
	default:
		return false;
	}
}
#endif
