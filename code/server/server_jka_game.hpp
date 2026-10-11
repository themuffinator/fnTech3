/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_GAME_HPP
#define FNTECH3_SERVER_JKA_GAME_HPP

#include "../compat/jka_game_abi.h"
#include "../compat/jka_ghoul2_clock.hpp"
#include "../compat/jka_module_abi.h"
#include "../compat/jka_state_adapter.hpp"
#include "server_jka_icarus.hpp"
#include "server_jka_actions.hpp"
#include "server_jka_roff.hpp"
#include "server_world_bounds.hpp"
#include "../nav/nav_graph.hpp"

#include <cmath>
#include <cstring>
#include <climits>

/* The engine side of Jedi Academy multiplayer's game boundary, as far as it
 * reaches today. Raven numbers its imports its own way -- only the first three
 * and the shared arithmetic block sit where Quake III's do -- so this decodes
 * them from the frozen table in compat/jka_module_calls.def.
 *
 * Everything this file does not serve yet is refused by name. That is the
 * point of it: a developer probe then reports exactly which import the retail
 * module asked for next, instead of the engine handing it an answer it never
 * asked for and dying inside the module. See
 * docs/fntech3/COMPATIBILITY.md#jedi-academy-gate-matrix. */

/* The navigation graph this title's bots walk on. It is the engine's to hold
 * and the game's to fill; it lives as long as the map does. */
static fntech3::nav::Graph jkaNavigation;

/* How far this title's game asks the server to cull an entity at. Keeping the
 * number is the whole of that call; nothing reads it until snapshots do. */
static float jkaServerCullDistance = 0.0f;

/* The clock this title's server poses Ghoul2 bolts at. Retail jamp.exe 1.01's
 * bolt core (0x4dfa00) never reads the time word a module passes: it poses at
 * the engine's own Ghoul2 clock, the client's (0x10bcb78) when that is set
 * and the server's (0x10bcb74) otherwise. A dedicated server has no client
 * clock. The server sets its own to the level time before the game starts,
 * before each settling frame and after them, after the frame that follows
 * the baseline (0x45e9e8..0x45ecf0), and after each server frame's run of
 * game frames (0x460883) -- so a query from inside a game frame poses one or
 * more server frames behind the time that frame was given. The clip setter
 * (0x4df310) and the angle setter (0x4df460) keep the caller's time. Neither
 * global is reset with a level, here or in retail. A listen server shares
 * the same pair with its client game and UI. See
 * docs/fntech3/2026-10-07-jka-server-pose-clock.md. */
static int SV_JKAGhoul2Time() { return fntech3::jka::clock::shared.Time(); }
/* Import 589 sets the no-rotation selector before the shared core, which
 * clears it only when a query succeeds (0x45cb27, 0x4dfc07): a failed 589
 * leaves the next successful 587 or 588 unrotated. */
static auto &jkaServerNoRotationPending = fntech3::jka::clock::noRotationPending;

void SV_JKAGhoul2SetTime( int time )
{
	if ( Com_GameIsJKAMP() ) fntech3::jka::clock::shared.Set(0, time);
}

static void SV_JKAClearGame( void )
{
	jkaSharedBuffer = nullptr;
	jkaServerCullDistance = 0.0f;
	jkaNavigation.Clear();
	SV_JKAIcarusClear();
	SV_RavenAssetsClear();
}

/* Raven kept Quake III's cvar flags up to 0x400 and gave 0x800 and 0x1000
 * meanings of its own, where fnTech3 keeps engine-owned bits. A module may
 * only ask for the flags the two engines agree on; its own two are recorded
 * for a later slice rather than granted, and nothing above them is admitted. */
static int SV_JKACvarFlags( intptr_t requested )
{
	const uint32_t shared = CVAR_ARCHIVE | CVAR_USERINFO | CVAR_SERVERINFO
		| CVAR_SYSTEMINFO | CVAR_INIT | CVAR_LATCH | CVAR_ROM | CVAR_USER_CREATED
		| CVAR_TEMP | CVAR_CHEAT | CVAR_NORESTART;
	return static_cast<int>( static_cast<uint32_t>( requested ) & shared );
}

/* Jedi Academy's own cvar record has no flags member, so the engine's is
 * copied across member by member rather than shared. */
static void SV_JKACvarStore( void *record, const vmCvar_t &internal )
{
	fntech3_jka_vmCvar_t external{};
	external.handle = internal.handle;
	external.modificationCount = internal.modificationCount;
	external.value = internal.value;
	external.integer = internal.integer;
	Q_strncpyz( external.string, internal.string, sizeof( external.string ) );
	std::memcpy( record, &external, sizeof( external ) );
}

static void SV_JKACvarLoad( const void *record, vmCvar_t &internal )
{
	fntech3_jka_vmCvar_t external{};
	std::memcpy( &external, record, sizeof( external ) );
	internal = vmCvar_t{};
	internal.handle = external.handle;
	internal.modificationCount = external.modificationCount;
	internal.value = external.value;
	internal.integer = external.integer;
	Q_strncpyz( internal.string, external.string, sizeof( internal.string ) );
}

/* Raven's own clamp for an arc sine, which its engine keeps beside Q_acos. */
static float SV_JKAArcSine( float value )
{
	if ( value > 1.0f ) value = 1.0f;
	if ( value < -1.0f ) value = -1.0f;
	return asinf( value );
}

/* The module addresses an entity by a pointer into the array it published, so
 * the number is the distance from its base. Nothing is derived before that
 * array is bound, and nothing outside it is admitted. */
static fntech3::icarus::EntityId SV_JKAEntityNumber( intptr_t address )
{
	if ( !sv.gentities || sv.gentitySize <= 0 || !address ) return fntech3::icarus::NoEntity;
	const auto base = reinterpret_cast<std::uintptr_t>( sv.gentities );
	const auto here = static_cast<std::uintptr_t>( address );
	if ( here < base ) return fntech3::icarus::NoEntity;
	const std::uintptr_t offset = here - base;
	if ( offset % std::uintptr_t( sv.gentitySize ) != 0 ) return fntech3::icarus::NoEntity;
	const std::uintptr_t index = offset / std::uintptr_t( sv.gentitySize );
	if ( index >= std::uintptr_t( sv.num_entities ) ) return fntech3::icarus::NoEntity;
	return fntech3::icarus::EntityId( index );
}

/* An entity's own name, as a script addresses it. Jedi Academy shares the
 * targetname with the engine for exactly this reason. */
static const char *SV_JKAEntityName( intptr_t address )
{
	if ( SV_JKAEntityNumber( address ) == fntech3::icarus::NoEntity ) return nullptr;
	const auto *shared = reinterpret_cast<const fntech3_jka_sharedEntity_t *>( address );
	return shared->targetname;
}

/* Which entity a navigation call is about.
 *
 * Raven's names say some of these take an entity and some take its number,
 * and the module's wrappers say only how many arguments each one passes. So
 * this does not decide: a value that resolves to an entity in the array the
 * module published is keyed by that entity's number, and anything else is
 * keyed by itself. Either way the key is the same for the same entity every
 * time, which is all the bookkeeping below needs it to be. */
static int SV_JKANavEntity( intptr_t address )
{
	const fntech3::icarus::EntityId number = SV_JKAEntityNumber( address );
	if ( number != fntech3::icarus::NoEntity ) return int( number );
	return int( address );
}

/* An entity the module names by pointer, checked against the array it
 * published before anything is read through it. The world calls feed what
 * they are handed straight to the collision model, so when `geometry` is
 * asked for, the entity's own bounds are read through the checked view and
 * refused if they are not a box this engine can trace against. */
static sharedEntity_t *SV_JKAWorldEntity( intptr_t address, bool geometry )
{
	const fntech3::icarus::EntityId number = SV_JKAEntityNumber( address );
	if ( number == fntech3::icarus::NoEntity ) {
		Com_Error( ERR_DROP, "Jedi Academy world argument is not a registered entity" );
	}
	void *pointer = VM_ArgPtr( address );
	if ( geometry ) {
		sharedEntity_t entity{};
		if ( !SV_ReadGameEntity( pointer, &entity ) ) {
			Com_Error( ERR_DROP, "Cannot read Jedi Academy world entity %d", int( number ) );
		}
		if ( !SV_ValidWorldBounds( entity.r.mins, entity.r.maxs )
			|| !SV_FiniteWorldVector( entity.r.currentOrigin )
			|| !SV_FiniteWorldVector( entity.r.currentAngles ) ) {
			Com_Error( ERR_DROP, "Invalid Jedi Academy world entity geometry: entity %d "
				"type %d, bounds (%g %g %g)-(%g %g %g), origin (%g %g %g)",
				int( number ), entity.s.eType,
				entity.r.mins[0], entity.r.mins[1], entity.r.mins[2],
				entity.r.maxs[0], entity.r.maxs[1], entity.r.maxs[2],
				entity.r.currentOrigin[0], entity.r.currentOrigin[1], entity.r.currentOrigin[2] );
		}
	}
	return static_cast<sharedEntity_t *>( pointer );
}

/* A vector the module hands in. Nothing infinite reaches the collision model,
 * because a trace against one does not end. The refusal names the call and the
 * numbers, so that a game handing one in can be told apart from an engine
 * reading the wrong bytes.
 *
 * `optional` is for the two a trace may legitimately leave out: this lineage
 * has always read a missing box as a point, and the server's own trace
 * substitutes the origin for one. Refusing those would refuse every point
 * trace the game makes. */
static const float *SV_JKAWorldVector( void *address, const char *where,
	bool optional = false )
{
	const float *value = static_cast<const float *>( address );
	if ( !value ) {
		if ( optional ) return nullptr;
		Com_Error( ERR_DROP, "Jedi Academy %s was handed no vector at all",
			where ? where : "world call" );
	}
	if ( !SV_FiniteWorldVector( value ) ) {
		Com_Error( ERR_DROP, "Jedi Academy %s was handed a vector that is not "
			"finite: (%g %g %g)", where ? where : "world call",
			value[0], value[1], value[2] );
	}
	return value;
}

class ServerJKARoffHost final : public fntech3::jka::roff::MotionHost {
public:
	fntech3_jka_sharedEntity_t *Entity( int number ) override
	{
		if ( !sv.gentities || number < 0 || number >= sv.num_entities
			|| sv.gentitySize < int( sizeof( fntech3_jka_sharedEntity_t ) ) ) return nullptr;
		return reinterpret_cast<fntech3_jka_sharedEntity_t *>( SV_GentityNum( number ) );
	}
	void Basis( const float angles[3], float columns[3][3] ) override
	{
		vec3_t mutableAngles;
		VectorCopy( angles, mutableAngles );
		AnglesToAxis( mutableAngles, columns );
	}
	void Note( int entity, const char *text ) override
	{
		const auto *callback = fntech3::jka::module::Describe(
			fntech3::jka::module::Call::GAME_ROFF_NOTETRACK_CALLBACK );
		if ( !gvm || !callback ) Com_Error( ERR_DROP, "Jedi Academy ROFF callback is unavailable" );
		/* Retail ProcessNote VA 0x426d74-0x426d84 calls export 11 with two
		 * arguments, the entity number and the line; no shared scratch buffer. */
		VM_Call( gvm, 2, callback->ordinal, entity, text );
	}
	void Fail( const char *reason ) override { Com_Error( ERR_DROP, "%s", reason ); }
};

static ServerJKARoffHost jkaRoffHost;
static fntech3::jka::roff::ServerPlayer jkaRoffPlayer( ravenRoff, jkaRoffHost );

static bool SV_JKAGameCall( const intptr_t *args, intptr_t &operation, intptr_t &result )
{
	using namespace fntech3::jka::module;
	(void)operation;
	result = 0;
	/* This title's own model store: it serves the `*default` skeleton,
	 * attached models, retail's tag frame and transitions between clips,
	 * posed at this server's own clock (server_raven_assets.hpp). */
	fntech3::ghoul2::Store &ravenGhoul2 = ravenGhoul2Academy;

	/* The arithmetic and memory block at 100-113 is Quake III's own, in the
	 * same order, and Jedi Academy kept it; the shared dispatcher serves it. */
	if ( args[0] >= TRAP_MEMSET && args[0] <= TRAP_TESTPRINTFLOAT ) return false;

	const Call call = Lookup( COM_GAME_JKA_MP, Module::GAME, Direction::IMPORT, args[0] );
	const auto pointer = [args]( int index ) { return VM_ArgPtr( args[index] ); };
	const auto text = [&pointer]( int index ) {
		return static_cast<const char *>( pointer( index ) );
	};

	switch ( call ) {
	case Call::G_PRINT:
		Com_Printf( "%s", text( 1 ) );
		return true;
	case Call::G_ERROR:
		Com_Error( ERR_DROP, "%s", text( 1 ) );
		return true;
	case Call::G_MILLISECONDS:
		result = Sys_Milliseconds();
		return true;

	case Call::G_CVAR_REGISTER: {
		vmCvar_t internal{};
		internal.handle = -1;
		if ( args[1] ) VM_CHECKBOUNDS( gvm, args[1], sizeof( fntech3_jka_vmCvar_t ) );
		Cvar_Register( args[1] ? &internal : nullptr, text( 2 ), text( 3 ),
			SV_JKACvarFlags( args[4] ), gvm->privateFlag );
		if ( args[1] && internal.handle >= 0 ) SV_JKACvarStore( pointer( 1 ), internal );
		return true;
	}
	case Call::G_CVAR_UPDATE: {
		if ( !args[1] ) return true;
		VM_CHECKBOUNDS( gvm, args[1], sizeof( fntech3_jka_vmCvar_t ) );
		vmCvar_t internal{};
		SV_JKACvarLoad( pointer( 1 ), internal );
		Cvar_Update( &internal, gvm->privateFlag );
		SV_JKACvarStore( pointer( 1 ), internal );
		return true;
	}
	case Call::G_CVAR_SET:
		Cvar_SetSafe( text( 1 ), text( 2 ) );
		return true;
	case Call::G_CVAR_VARIABLE_INTEGER_VALUE:
		result = Cvar_VariableIntegerValue( text( 1 ) );
		return true;
	case Call::G_CVAR_VARIABLE_STRING_BUFFER:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		Cvar_VariableStringBuffer( text( 1 ), static_cast<char *>( pointer( 2 ) ),
			static_cast<int>( args[3] ) );
		return true;

	case Call::G_ARGC:
		result = Cmd_Argc();
		return true;
	case Call::G_ARGV:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		Cmd_ArgvBuffer( static_cast<int>( args[1] ), static_cast<char *>( pointer( 2 ) ),
			static_cast<int>( args[3] ) );
		return true;
	case Call::G_SEND_CONSOLE_COMMAND:
		/* Raven kept Quake III's three execution times, so the module's own
		 * number is the engine's; anything else is not one of them. */
		if ( args[1] < EXEC_NOW || args[1] > EXEC_APPEND ) {
			Com_Error( ERR_DROP, "Jedi Academy console command with an unknown execution time %d",
				static_cast<int>( args[1] ) );
		}
		Cbuf_ExecuteText( static_cast<cbufExec_t>( args[1] ), text( 2 ) );
		return true;

	case Call::G_FS_FOPEN_FILE:
		result = FS_VM_OpenFile( text( 1 ), static_cast<fileHandle_t *>( pointer( 2 ) ),
			static_cast<fsMode_t>( args[3] ), H_QAGAME );
		return true;
	case Call::G_FS_READ:
		if ( args[3] == 0 ) return true;
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		result = FS_VM_ReadFile( pointer( 1 ), static_cast<int>( args[2] ),
			static_cast<fileHandle_t>( args[3] ), H_QAGAME );
		return true;
	case Call::G_FS_WRITE:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		FS_VM_WriteFile( pointer( 1 ), static_cast<int>( args[2] ),
			static_cast<fileHandle_t>( args[3] ), H_QAGAME );
		return true;
	case Call::G_FS_FCLOSE_FILE:
		FS_VM_CloseFile( static_cast<fileHandle_t>( args[1] ), H_QAGAME );
		return true;
	case Call::G_FS_GETFILELIST:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		result = FS_GetFileList( text( 1 ), text( 2 ), static_cast<char *>( pointer( 3 ) ),
			static_cast<int>( args[4] ) );
		return true;

	case Call::G_SET_SHARED_BUFFER:
		if ( !args[1] ) {
			Com_Error( ERR_DROP, "Jedi Academy game lent the engine no shared buffer" );
		}
		jkaSharedBuffer = pointer( 1 );
		return true;

	case Call::G_LOCATE_GAME_DATA:
		SV_LocateGameData( args[1], static_cast<int>( args[2] ), static_cast<int>( args[3] ),
			args[4], static_cast<int>( args[5] ) );
		return true;

	case Call::G_SET_CONFIGSTRING:
		SV_SetConfigstring( static_cast<int>( args[1] ), text( 2 ) );
		return true;
	case Call::G_GET_CONFIGSTRING:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetConfigstring( static_cast<int>( args[1] ), static_cast<char *>( pointer( 2 ) ),
			static_cast<int>( args[3] ) );
		return true;
	case Call::G_GET_SERVERINFO:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		SV_GetServerinfo( static_cast<char *>( pointer( 1 ) ), static_cast<int>( args[2] ) );
		return true;

	case Call::G_GET_ENTITY_TOKEN: {
		const char *token = COM_Parse( &sv.entityParsePoint );
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		const int last = static_cast<int>( args[2] ) - 1;
		if ( last >= 0 ) {
			char *destination = static_cast<char *>( pointer( 1 ) );
			Q_strncpy( destination, const_cast<char *>( token ), last );
			destination[last] = '\0';
		}
		result = ( sv.entityParsePoint != nullptr || token[0] != 0 ) ? 1 : 0;
		return true;
	}

	/* Raven's qtime_t is Quake III's nine integers, in the same order. */
	case Call::G_REAL_TIME:
		if ( args[1] ) VM_CHECKBOUNDS( gvm, args[1], sizeof( qtime_t ) );
		result = Com_RealTime( args[1] ? static_cast<qtime_t *>( pointer( 1 ) ) : nullptr );
		return true;
	case Call::G_SNAPVECTOR:
		VM_CHECKBOUNDS( gvm, args[1], sizeof( vec3_t ) );
		Sys_SnapVector( static_cast<float *>( pointer( 1 ) ) );
		return true;

	/* Raven's game asks the engine to forget every entity-to-bone attachment
	 * as its first act, before a level exists, and again when one ends. This
	 * used to be answered by doing nothing, which was true only while no
	 * import could make an attachment. One now can, so this really clears
	 * what the engine holds, and the instances themselves survive it -- only
	 * the attachments go.
	 *
	 * An attachment is what lets the game ask where a bolt on one entity is
	 * from another. The engine remembers which instance belongs to which
	 * entity number and nothing else about it. */
	case Call::G_G2_CLEANENTATTACHMENTS:
		ravenGhoul2.ClearAttachments();
		return true;

	case Call::G_G2_ATTACHINSTANCETOENTNUM:
		/* The instance, the entity it belongs to, and a flag saying which
		 * side is asking. A dedicated server is always the server side, so
		 * the flag is recorded and not acted on. An instance this engine
		 * never handed out is refused rather than remembered. */
		result = ravenGhoul2.Attach( pointer( 1 ), static_cast<int>( args[2] ) ) ? 1 : 0;
		return true;
	case Call::G_G2_CLEARATTACHEDINSTANCE:
		ravenGhoul2.Detach( static_cast<int>( args[1] ) );
		return true;

	/* Linking is where a level stops being a list and becomes a world. The
	 * server reads the entity through its checked view, so this call is what
	 * the entity-state adapter was for: Academy's 532-byte record converted
	 * field by field, never cast.
	 *
	 * These two are served by name and never handed through. Quake III's 34
	 * and 35 are G_BOT_ALLOCATE_CLIENT and G_BOT_FREE_CLIENT; handing Raven's
	 * numbers to the shared dispatcher would free a bot client instead of
	 * unlinking an entity. */
	case Call::G_LINKENTITY:
		SV_LinkEntity( static_cast<sharedEntity_t *>( pointer( 1 ) ) );
		return true;
	case Call::G_UNLINKENTITY:
		SV_UnlinkEntity( static_cast<sharedEntity_t *>( pointer( 1 ) ) );
		return true;

	case Call::G_GET_USERCMD: {
		/* The input frame a client last sent, as this title's own record. Its
		 * first twenty-one bytes are Quake III's; the three that follow are
		 * Raven's, where Quake III keeps a second weapon and a field of view,
		 * so the movement bytes sit one byte later. The adapter places every
		 * one by name rather than copying the record across. */
		VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_jka_usercmd_t ) );
		usercmd_t internal{};
		SV_GetUsercmd( static_cast<int>( args[1] ), &internal );
		fntech3_jka_usercmd_t external{};
		if ( !fntech3::jka::state::FromInternal( internal, external ) ) {
			Com_Error( ERR_DROP, "Jedi Academy cannot be told this input frame" );
		}
		std::memcpy( pointer( 2 ), &external, sizeof( external ) );
		return true;
	}

	/* --- clients, by name and never by Quake III's number ---------------
	 *
	 * Every one of these carries only a client number and text, so nothing of
	 * this title's own layout is at stake; what is at stake is the number.
	 * Quake III puts the userinfo pair at 20 and 21 and the serverinfo at 22,
	 * and Raven puts them at 22, 23 and 24, so a hand-through would read a
	 * client's userinfo when the game asked to write it. */
	case Call::G_DROP_CLIENT:
		SV_GameDropClient( static_cast<int>( args[1] ), text( 2 ) );
		return true;
	case Call::G_SEND_SERVER_COMMAND:
		SV_GameSendServerCommand( static_cast<int>( args[1] ), text( 2 ) );
		return true;
	case Call::G_GET_USERINFO:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetUserinfo( static_cast<int>( args[1] ), static_cast<char *>( pointer( 2 ) ),
			static_cast<int>( args[3] ) );
		return true;
	case Call::G_SET_USERINFO:
		SV_SetUserinfo( static_cast<int>( args[1] ), text( 2 ) );
		return true;

	/* --- client slots for the game's own bots ---------------------------
	 *
	 * Served by name, like the link pair and for the same reason: Quake III's
	 * 38 and 39 are the two potentially-visible-set tests, so handing Raven's
	 * numbers through would answer a slot request with a visibility answer.
	 *
	 * Freeing a slot reads and writes the bot's entity through the checked
	 * view, which converts this title's own 532-byte state, so the flag it
	 * clears is cleared in Raven's record and not in a Quake III one. */
	case Call::G_BOT_ALLOCATE_CLIENT:
		result = SV_BotAllocateClient();
		return true;
	case Call::G_BOT_FREE_CLIENT:
		SV_BotFreeClient( static_cast<int>( args[1] ) );
		return true;

	/* --- the world: the geometry a level answers the game with -------- */

	case Call::G_SET_BRUSH_MODEL: {
		/* A brush model is named by its number with a star in front. The name
		 * is checked here rather than inside the parser, so a model this map
		 * does not have is a refusal instead of an index into nothing. */
		sharedEntity_t *entity = SV_JKAWorldEntity( args[1], true );
		const char *name = text( 2 );
		if ( !name || name[0] != '*' || name[1] < '0' || name[1] > '9' ) {
			Com_Error( ERR_DROP, "Invalid Jedi Academy brush model name" );
		}
		int model = 0;
		for ( int at = 1; name[at]; ++at ) {
			if ( name[at] < '0' || name[at] > '9'
				|| model > ( INT_MAX - ( name[at] - '0' ) ) / 10 ) {
				Com_Error( ERR_DROP, "Invalid Jedi Academy brush model index" );
			}
			model = model * 10 + name[at] - '0';
		}
		if ( model >= CM_NumInlineModels() ) {
			Com_Error( ERR_DROP, "Jedi Academy brush model %d is outside this map", model );
		}
		SV_SetBrushModel( entity, name );
		return true;
	}

	case Call::G_TRACE: {
		/* Raven's trap passes two more arguments than Quake III's, a zero and
		 * a ten, which steer Ghoul2 collision. This is the world trace, which
		 * has no Ghoul2 in it, so they are recorded here and not acted on;
		 * G_G2TRACE is where they would mean something. */
		trace_t traced{};
		SV_Trace( &traced, SV_JKAWorldVector( pointer( 2 ), "the trace start" ),
			SV_JKAWorldVector( pointer( 3 ), "the trace mins", true ),
			SV_JKAWorldVector( pointer( 4 ), "the trace maxs", true ),
			SV_JKAWorldVector( pointer( 5 ), "the trace end" ), static_cast<int>( args[6] ),
			static_cast<int>( args[7] ), qfalse );
		fntech3_jka_trace_t external{};
		if ( !fntech3::jka::state::FromInternal( traced, external ) ) {
			Com_Error( ERR_DROP, "Jedi Academy cannot be told this trace result" );
		}
		std::memcpy( pointer( 1 ), &external, sizeof( external ) );
		return true;
	}

	case Call::G_POINT_CONTENTS:
		result = SV_PointContents( SV_JKAWorldVector( pointer( 1 ), "the point-contents point" ),
			static_cast<int>( args[2] ) );
		return true;

	case Call::G_IN_PVS:
		result = SV_inPVS( SV_JKAWorldVector( pointer( 1 ), "a visibility point" ),
			SV_JKAWorldVector( pointer( 2 ), "a visibility point" ) );
		return true;
	case Call::G_IN_PVS_IGNORE_PORTALS:
		result = SV_inPVSIgnorePortals( SV_JKAWorldVector( pointer( 1 ), "a visibility point" ),
			SV_JKAWorldVector( pointer( 2 ), "a visibility point" ) );
		return true;

	case Call::G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( SV_JKAWorldEntity( args[1], false ), args[2] != 0 );
		return true;
	case Call::G_AREAS_CONNECTED:
		result = CM_AreasConnected( static_cast<int>( args[1] ), static_cast<int>( args[2] ) );
		return true;

	case Call::G_ENTITIES_IN_BOX:
	case Call::G_ENTITY_CONTACT:
	case Call::G_ENTITY_CONTACTCAPSULE: {
		const float *mins = SV_JKAWorldVector( pointer( 1 ), "a box corner" );
		const float *maxs = SV_JKAWorldVector( pointer( 2 ), "a box corner" );
		if ( !SV_ValidWorldBounds( mins, maxs ) ) {
			Com_Error( ERR_DROP, "Inverted Jedi Academy world bounds" );
		}
		if ( call != Call::G_ENTITIES_IN_BOX ) {
			result = SV_EntityContact( mins, maxs, SV_JKAWorldEntity( args[3], true ),
				call == Call::G_ENTITY_CONTACTCAPSULE );
			return true;
		}
		const int capacity = static_cast<int>( args[4] );
		if ( capacity < 0 || capacity > FNTECH3_JKA_MAX_GENTITIES ) {
			Com_Error( ERR_DROP, "Invalid Jedi Academy area entity capacity" );
		}
		if ( capacity == 0 ) return true;
		/* The query is answered into storage of this engine's own before any
		 * of it is published, so a list that overlaps its own inputs cannot
		 * change the box while the box is still being searched. */
		static int found[FNTECH3_JKA_MAX_GENTITIES];
		const int count = SV_AreaEntities( mins, maxs, found, capacity );
		if ( count < 0 || count > capacity ) {
			Com_Error( ERR_DROP, "Invalid Jedi Academy area entity result" );
		}
		std::memcpy( pointer( 3 ), found, std::size_t( count ) * sizeof( int ) );
		result = count;
		return true;
	}

	case Call::G_SET_SERVER_CULL:
		/* A distance the game chooses and the server keeps. Remembering it is
		 * the entire call: its own engine stores it in one global and reads it
		 * when it builds a snapshot, which nothing here does yet. */
		jkaServerCullDistance = VMF( 1 );
		return true;

	case Call::G_G2_SETSKIN: {
		/* The game puts a registered skin on one of an instance's models. The
		 * server keeps the handle; what it stands for is a renderer's business
		 * and no part of this. An instance this engine never handed out is
		 * refused, not written through. */
		const int customSkin = static_cast<int>( args[3] );
		result = ravenGhoul2.SetSkin( pointer( 1 ), static_cast<int>( args[2] ),
			customSkin ) ? 1 : 0;
		return true;
	}

	case Call::G_R_REGISTERSKIN:
		/* Raven's own engine asks its renderer for a *server* skin, separately
		 * from the one a client registers, because a server needs the handle
		 * and not the shaders: the game hands that handle back when it sets a
		 * skin on a model. The file is really read and its surfaces really
		 * remembered. A skin that is not there registers as no skin, which is
		 * zero, and zero is what this lineage has always meant by none. */
		result = ravenSkins.Register( text( 1 ) );
		return true;

	case Call::G_G2_ANGLEOVERRIDE:
		/* Bend one bone. Retail jamp.exe's handler reads eleven arguments and
		 * converts exactly three -- the third, the fourth and the ninth --
		 * which places the bone's name, the angles and the model list as the
		 * only pointers and fixes everything between them:
		 *
		 *     ghoul2, modelIndex, boneName, angles, flags,
		 *     up, right, forward, modelList, blendTime, currentTime
		 *
		 * The three orientations say which way round the angles are meant.
		 * The engine records them; turning them into a matrix is a renderer's
		 * business and is not measured here. The model list is a Ghoul2 handle
		 * array the game passes for its own bookkeeping and is not read.
		 * Academy's store matches names without ASCII case, as this core's
		 * find/add do (0x4e2be0/0x4e2c70); see the bone names record. */
		VM_CHECKBOUNDS( gvm, args[4], sizeof( vec3_t ) );
		result = ravenGhoul2.SetBoneAngles( pointer( 1 ), static_cast<int>( args[2] ),
			text( 3 ), static_cast<const float *>( pointer( 4 ) ),
			static_cast<int>( args[5] ), static_cast<int>( args[6] ),
			static_cast<int>( args[7] ), static_cast<int>( args[8] ),
			static_cast<int>( args[10] ), static_cast<int>( args[11] ) ) ? 1 : 0;
		return true;

	case Call::G_G2_PLAYANIM:
		/* Set one bone playing. Retail jamp.exe's own handler for this reads
		 * ten arguments and converts exactly one of them from a module
		 * address, which places the bone's name as the only pointer in the
		 * list and fixes the order of the rest:
		 *
		 *     ghoul2, modelIndex, boneName, startFrame, endFrame, flags,
		 *     animSpeed, currentTime, setFrame, blendTime
		 *
		 * The ninth argument is a floating frame seed, with -1.0f meaning the
		 * animation starts at its first frame. Retail's bone setup shifts the
		 * start clock for another seed, including a fractional one.
		 *
		 * The setter is the client game's (0x4df310, which its import 303
		 * reaches too), so its bounds are too: a first frame outside 0..99999
		 * becomes 0, a last frame outside 1..99999 becomes 1, and a seed below
		 * 0 other than -1, or above 100000, becomes 0 (0x4df326..0x4df36f). The
		 * clip starts at the caller's time, which the setter keeps.
		 *
		 * The setter marks the bone's record with 0x8000 before it takes the
		 * flags (0x4e334f), so that bit in the flags is no part of the clip:
		 * the retail game hands import 597's answer, mark and all, straight
		 * back here. The store keeps the mark in the bone's word itself.
		 * Its Academy name policy also finds differently cased prior clips
		 * for blending: docs/fntech3/2026-10-07-jka-bone-names.md. */
		{
			const int first = args[4] >= 0 && args[4] < 100000 ? static_cast<int>( args[4] ) : 0;
			const int last = args[5] > 0 && args[5] < 100000 ? static_cast<int>( args[5] ) : 1;
			float seed = VMF( 9 );
			if ( seed != -1.0f && ( seed < 0.0f || seed > 100000.0f ) ) seed = 0.0f;
			const int flags = static_cast<int>( args[6] & ~intptr_t( fntech3::ghoul2::BoneMarkFlag ) );
			result = ravenGhoul2.SetBoneAnim( pointer( 1 ), static_cast<int>( args[2] ),
				text( 3 ), first, last, flags, VMF( 7 ),
				static_cast<int>( args[8] ), seed, static_cast<int>( args[10] ) ) ? 1 : 0;
		}
		return true;

	case Call::G_G2_SETBOLTINFO:
		/* Which bolt a model is using. Retail jamp.exe's handler reads three
		 * arguments and converts none of them, so all three are numbers and
		 * the engine keeps the last one (0x4df940 stores it in the model's
		 * record). The model is then posed from that bolt of its parent. */
		result = ravenGhoul2.SetBoltInfo( pointer( 1 ), static_cast<int>( args[2] ),
			static_cast<int>( args[3] ) ) ? 1 : 0;
		return true;

	case Call::G_G2_GETBOLT:
	case Call::G_G2_GETBOLT_NOREC:
	case Call::G_G2_GETBOLT_NOREC_NOROT:
		/* Where a bolt is, in the world.
		 *
		 * The argument list is the retail engine's own: its handler for this
		 * reads nine and converts five of them from module addresses -- the
		 * matrix it writes, the angles, the position, the model list and the
		 * scale -- which leaves the instance, the model, the bolt and the
		 * time as numbers and fixes the order:
		 *
		 *     ghoul2, modelIndex, boltIndex, matrix, angles, position,
		 *     time, modelList, scale
		 *
		 * The time is not read: retail's core poses at the server's own clock
		 * (SV_JKAGhoul2Time above), and so does this.
		 *
		 * The store works the bolt out in the model's own space: a bolt that
		 * names a bone is that bone posed for the moment asked about, and one
		 * that names a surface is a tag, whose first three vertices, carried
		 * by the bones they are weighted to, give retail's frame. Retail's
		 * query builds the whole instance's skeleton first (jamp.exe 0x4dfa00
		 * into 0x49a050, the same core its client game reaches), so a saber or
		 * weapon attached to the body answers from where the body's bolt has
		 * put it, mid-transition included. This then scales its model-space
		 * translation, normalizes the rotation rows and applies the entity's
		 * angles and position.
		 *
		 * Retail's own failure -- a model or bolt the instance does not have
		 * -- writes the entity's placement times the fixed axes and answers
		 * 0 (0x4dfc1d), and so does this: the game reads the matrix whatever
		 * the answer. A bolt the store cannot answer for a reason of its own
		 * writes nothing and reports failure, because no retail answer is
		 * known to hand back: a model retail's model sort never reaches,
		 * which retail answers from whatever pose it last had; a link to a
		 * bolt its parent lacks, where retail reads past its list; and a pose
		 * outside the served clips. So does an instance this store never
		 * handed out, which retail would dereference. Retail also reuses a
		 * model's pose for a second query at the same clock even when another
		 * model of its instance has been bent in between (0x49a010); this
		 * poses afresh.
		 *
		 * The first two imports share placement; NOREC_NOROT additionally
		 * skips the MP API's rotation-only -90 degree correction, by a
		 * selector that only a successful query clears. */
		{
			if ( call == Call::G_G2_GETBOLT_NOREC_NOROT ) jkaServerNoRotationPending = true;
			VM_CHECKBOUNDS( gvm, args[4], sizeof( float ) * 12 );
			VM_CHECKBOUNDS( gvm, args[5], sizeof( vec3_t ) );
			VM_CHECKBOUNDS( gvm, args[6], sizeof( vec3_t ) );
			float *destination = static_cast<float *>( pointer( 4 ) );
			const float *angles = static_cast<const float *>( pointer( 5 ) );
			const float *position = static_cast<const float *>( pointer( 6 ) );
			if ( !destination || !angles || !position ) {
				result = 0;
				return true;
			}

			const int modelIndex = static_cast<int>( args[2] );
			const int boltIndex = static_cast<int>( args[3] );
			if ( ravenGhoul2.Resolve( pointer( 1 ) ) && !ravenGhoul2.BoltAt( pointer( 1 ), modelIndex, boltIndex ) ) {
				float fallback[12];
				if ( fntech3::ghoul2::pose::FailureBolt( angles, position, fallback ) )
					std::memcpy( destination, fallback, sizeof( fallback ) );
				result = 0;
				return true;
			}
			float bolt[12];
			if ( !ravenGhoul2.BoltMatrix( pointer( 1 ), modelIndex, boltIndex, SV_JKAGhoul2Time(), bolt ) ) {
				result = 0;
				return true;
			}

			const float *scale = nullptr;
			if ( args[9] ) {
				VM_CHECKBOUNDS( gvm, args[9], sizeof( vec3_t ) );
				scale = static_cast<const float *>( pointer( 9 ) );
			}
			/* Retail core0x4dfa00 scales the model-space translation before
			 * placement, normalizes rows and retains589's rotation distinction. */
			float world[12];
			if ( !fntech3::ghoul2::pose::PlaceBolt(bolt, angles, position, scale,
				jkaServerNoRotationPending, world) ) {
				result = 0;
				return true;
			}
			jkaServerNoRotationPending = false;
			std::memcpy(destination, world, sizeof(world));
			result = 1;
		}
		return true;

	case Call::G_G2_ADDBOLT:
		/* A bolt is a named place on a model -- a surface of the mesh or a
		 * bone of the skeleton -- that the game asks about by index later.
		 * Both files name their own, and the store read those names when the
		 * model loaded, so this is a lookup and not an invention. A name the
		 * model does not carry is reported as no bolt. Academy folds ASCII
		 * case for surfaces, bones and duplicate additions, as 0x4e22d0 does. */
		result = ravenGhoul2.AddBolt( pointer( 1 ), static_cast<int>( args[2] ), text( 3 ) );
		return true;

	case Call::G_G2_GETBONEANIM:
		/* What one bone is playing, and how far its clip has got. Retail
		 * jamp.exe 1.01's handler (0x45cc9c) reads ten arguments, converts
		 * the second and the fourth to ninth, and resolves the model itself:
		 *
		 *     ghoul2, boneName, time, currentFrame, startFrame, endFrame,
		 *     flags, animSpeed, modelList, modelIndex
		 *
		 * Its wrapper (0x4df400) replaces the time with the Ghoul2 clock, the
		 * one this server's bolts pose at (SV_JKAGhoul2Time above), and
		 * the model list is never read. A bone the skeleton lacks answers 0
		 * and writes nothing. A bone with no clip running answers 0 and
		 * writes frame 0, frames 0..1, no flags and no speed (0x4e3709). A
		 * running clip answers 1 with its frame at the clock, its range, its
		 * record's flags word -- the 0x8000 its setter marks it with, a
		 * blend's 0x80 for as long as the clip lasts, and any angle mode --
		 * and its speed (0x4e36af). Each writes in retail's order.
		 *
		 * Retail resolves a model index past the instance's end without a
		 * check (0x4116e0) and reads what lies beyond it; this answers 0 and
		 * writes nothing for one, as for a model the instance has removed.
		 * A clip the store does not time -- completing without loop or
		 * freeze, which retail's timing answers by clearing the word's
		 * animation bits, a reverse clip, or flag bits outside the clip's
		 * own -- is refused by name, as are an instance this engine never
		 * handed out and an output retail would write through null. See
		 * docs/fntech3/2026-10-07-jka-server-bone-anim.md. */
		{
			if ( !ravenGhoul2.Resolve( pointer( 1 ) ) ) {
				Com_Error( ERR_DROP, "Jedi Academy G_G2_GETBONEANIM: stale or foreign model handle" );
				return true;
			}
			fntech3::ghoul2::BoneClip clip;
			const auto answer = ravenGhoul2.BoneClipAt( pointer( 1 ), static_cast<int>( args[10] ),
				text( 2 ), SV_JKAGhoul2Time(), clip );
			if ( answer == fntech3::ghoul2::BoneClipAnswer::Unserved ) {
				Com_Error( ERR_DROP, "Jedi Academy G_G2_GETBONEANIM: bone %s of model %d plays a clip "
					"outside the served forward timing (flags 0x%x)", text( 2 ),
					static_cast<int>( args[10] ),
					static_cast<unsigned>( ravenGhoul2.BoneFlagsWord( pointer( 1 ),
						static_cast<int>( args[10] ), text( 2 ) ) ) );
				return true;
			}
			if ( answer != fntech3::ghoul2::BoneClipAnswer::Inactive
				&& answer != fntech3::ghoul2::BoneClipAnswer::Active ) {
				result = 0;
				return true;
			}
			void *outputs[5];
			for ( int index = 0; index < 5; ++index ) {
				VM_CHECKBOUNDS( gvm, args[4 + index], sizeof( float ) );
				outputs[index] = pointer( 4 + index );
				if ( !outputs[index] ) {
					Com_Error( ERR_DROP, "Jedi Academy G_G2_GETBONEANIM: null output" );
					return true;
				}
			}
			const std::uint32_t flags = clip.flags;
			if ( answer == fntech3::ghoul2::BoneClipAnswer::Inactive ) {
				std::memcpy( outputs[1], &clip.startFrame, sizeof( int ) );
				std::memcpy( outputs[2], &clip.endFrame, sizeof( int ) );
				std::memcpy( outputs[0], &clip.frame, sizeof( float ) );
				std::memcpy( outputs[3], &flags, sizeof( flags ) );
				std::memcpy( outputs[4], &clip.speed, sizeof( float ) );
				result = 0;
				return true;
			}
			std::memcpy( outputs[0], &clip.frame, sizeof( float ) );
			std::memcpy( outputs[1], &clip.startFrame, sizeof( int ) );
			std::memcpy( outputs[2], &clip.endFrame, sizeof( int ) );
			std::memcpy( outputs[3], &flags, sizeof( flags ) );
			std::memcpy( outputs[4], &clip.speed, sizeof( float ) );
			result = 1;
		}
		return true;

	case Call::G_G2_GETGLANAME:
		/* The skeleton a model names, which the store already read out of the
		 * mesh when it loaded it. The call carries no buffer length, so the
		 * engine writes at most one path's worth and always terminates it; a
		 * model this store does not hold writes nothing at all rather than
		 * leaving the game reading whatever was there. */
		{
			VM_CHECKBOUNDS( gvm, args[3], MAX_QPATH );
			char *destination = static_cast<char *>( pointer( 3 ) );
			const fntech3::ghoul2::Instance *instance = ravenGhoul2.Resolve( pointer( 1 ) );
			const fntech3::ghoul2::Model *model = instance
				? instance->At( static_cast<int>( args[2] ) ) : nullptr;
			if ( !model ) { destination[0] = '\0'; result = 0; return true; }
			Q_strncpyz( destination, model->skeletonName.c_str(), MAX_QPATH );
			result = 1;
		}
		return true;

	case Call::G_G2_COPYSPECIFICGHOUL2MODEL:
		/* One model out of one instance into a place in another, which is how
		 * a weapon moves from one holder to the next. Retail jamp.exe's
		 * handler reads four arguments and converts none of them, so both
		 * instances arrive as the handles this engine gave out and both are
		 * checked before either is touched. */
		result = ravenGhoul2.CopyModel( pointer( 1 ), static_cast<int>( args[2] ),
			pointer( 3 ), static_cast<int>( args[4] ) ) ? 1 : 0;
		return true;

	case Call::G_G2_DUPLICATEGHOUL2INSTANCE:
		/* One entity takes over another's models. The copy is its own from
		 * then on, and the pointer handed back is owned here like every
		 * other; a source this engine never gave out is refused rather than
		 * copied out of whatever it points at. */
		if ( !args[2] ) { result = 0; return true; }
		VM_CHECKBOUNDS( gvm, args[2], sizeof( void * ) );
		result = ravenGhoul2.Duplicate( pointer( 1 ),
			static_cast<void **>( pointer( 2 ) ) ) ? 1 : 0;
		return true;

	case Call::G_G2_OVERRIDESERVER:
		/* The game says this instance is the server's own. Raven's engine
		 * keeps such an instance out of its renderer; a dedicated server has
		 * no renderer, so the mark is recorded and nothing is done with it. */
		result = ravenGhoul2.OverrideServer( pointer( 1 ) ) ? 1 : 0;
		return true;

	case Call::G_G2_HAVEWEGHOULMODELS:
		/* Whether this instance holds a model at all. The game asks before it
		 * does anything else with one, and the answer is what the store knows:
		 * an instance this engine never handed out holds nothing. */
		{
			const fntech3::ghoul2::Instance *instance = ravenGhoul2.Resolve( pointer( 1 ) );
			result = instance && instance->HoldsAny() ? 1 : 0;
		}
		return true;

	case Call::G_G2_INITGHOUL2MODEL: {
		/* The game asks the server to hold a model for an entity and keeps the
		 * instance pointer in its own record. The model is really read, by the
		 * same bounded readers that swept both installations, so a model that
		 * would not read there does not load here. A model that does not load
		 * is reported as not loaded, which is what this call returns for one.
		 *
		 * The pointer handed back is owned here and checked on the way in; a
		 * pointer this engine did not give out is never followed. */
		if ( !args[1] ) { result = -1; return true; }
		VM_CHECKBOUNDS( gvm, args[1], sizeof( void * ) );
		auto **slot = static_cast<void **>( pointer( 1 ) );
		fntech3::ghoul2::LoadError error = fntech3::ghoul2::LoadError::None;
		const int index = ravenGhoul2.AddModel( slot, text( 2 ), error );
		if ( index < 0 ) {
			Com_Printf( "Ghoul2: %s: %s\n", text( 2 ),
				fntech3::ghoul2::LoadErrorName( error ) );
		}
		result = index;
		return true;
	}

	case Call::G_G2_CLEANMODELS: {
		/* Retail jampgamex86.dll 1.01 wrapper 0x200a6fb0 passes one argument:
		 * the address of its opaque pointer slot. Cleanup frees the instance
		 * and nulls that slot; an already-null handle is a genuine no-op. */
		if ( !args[1] ) {
			Com_Error( ERR_DROP, "Jedi Academy G_G2_CLEANMODELS: null model pointer slot" );
			return true;
		}
		VM_CHECKBOUNDS( gvm, args[1], sizeof( void * ) );
		if ( !ravenGhoul2.Clean( static_cast<void **>( pointer( 1 ) ) ) ) {
			Com_Error( ERR_DROP, "Jedi Academy G_G2_CLEANMODELS: stale or foreign model handle" );
		}
		return true;
	}

	/* --- navigation ------------------------------------------------------
	 *
	 * The game asks the engine to load a map's navigation graph and is told
	 * whether it got one. fnTech3 has no navigation graph and did not load
	 * one, so the answer is no. That is a true statement about this engine,
	 * not a made-up success: the call reports a result, and the result is the
	 * one that happened.
	 *
	 * It is only true while the engine holds no navigation at all. Every other
	 * navigation import is refused, and jka_module_refusal_tests.py fails if
	 * one is ever served while this still answers no -- at which point this
	 * has to start loading the file rather than declining it.
	 *
	 * A legitimate installation does ship these: 24 of them, one per
	 * multiplayer map, `maps/mp/ffa1.nav` among them. Declining is a statement
	 * about the engine, not about the installation. */
	/* --- navigation: Raven's own graph, not Quake III's areas ---------
	 *
	 * This title asks for twenty-three of these and never for `G_NAV_INIT` or
	 * `G_NAV_FREE`, so the graph is the engine's to own. How many arguments
	 * each call passes is measured from the module's own wrappers; what each
	 * argument means is read from Raven's name for the call, and where that
	 * reading could be wrong it is said so here rather than left implied. */

	case Call::G_NAV_ADDRAWPOINT:
		result = jkaNavigation.AddNode( SV_JKAWorldVector( pointer( 1 ), "a navigation point" ),
			static_cast<int>( args[2] ), static_cast<int>( args[3] ) );
		return true;
	case Call::G_NAV_HARDCONNECT:
		jkaNavigation.Connect( static_cast<int>( args[1] ), static_cast<int>( args[2] ) );
		return true;
	case Call::G_NAV_CALCULATEPATHS:
		/* The one argument this passes is not acted on: working the paths out
		 * again gives the same answer, so nothing it could say would change
		 * what happens. */
		jkaNavigation.CalculatePaths();
		return true;
	case Call::G_NAV_SETPATHSCALCULATED:
		jkaNavigation.SetPathsCalculated( args[1] != 0 );
		return true;

	case Call::G_NAV_GETNUMNODES:
		result = jkaNavigation.NodeCount();
		return true;
	case Call::G_NAV_GETNODENUMEDGES:
		result = jkaNavigation.EdgeCount( static_cast<int>( args[1] ) );
		return true;
	case Call::G_NAV_GETNODEEDGE:
		result = jkaNavigation.EdgeAt( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ) );
		return true;
	case Call::G_NAV_NODESARENEIGHBORS:
		result = jkaNavigation.AreNeighbors( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ) ) ? 1 : 0;
		return true;
	case Call::G_NAV_GETNODEPOSITION: {
		const fntech3::nav::Node *node = jkaNavigation.At( static_cast<int>( args[1] ) );
		if ( !node ) return true;      /* no node, and nothing written */
		std::memcpy( pointer( 2 ), node->origin, sizeof( node->origin ) );
		result = 1;
		return true;
	}
	case Call::G_NAV_GETNODERADIUS: {
		const fntech3::nav::Node *node = jkaNavigation.At( static_cast<int>( args[1] ) );
		result = node ? node->radius : 0;
		return true;
	}
	case Call::G_NAV_GETPATHCOST:
		result = jkaNavigation.PathCost( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ) );
		return true;

	case Call::G_NAV_GETNEARESTNODE: {
		/* The entity, the node it was last at, which nodes count, and the one
		 * it is heading for. Only the entity and the flags are acted on: what
		 * the other two are for is a search this engine does not do yet, and
		 * answering as though it did would be an invention. */
		sharedEntity_t entity{};
		if ( !SV_ReadGameEntity( SV_JKAWorldEntity( args[1], false ), &entity ) ) {
			result = fntech3::nav::NoNode;
			return true;
		}
		result = jkaNavigation.NearestNode( entity.r.currentOrigin,
			static_cast<int>( args[3] ), SV_JKANavEntity( args[1] ) );
		return true;
	}
	case Call::G_NAV_GETBESTNODE:
	case Call::G_NAV_GETBESTNODEALT2: {
		/* Which node to walk to next on the way from one to another, avoiding
		 * a third. The path is the one the graph worked out; a step onto the
		 * rejected node is no step at all. */
		const int step = jkaNavigation.NextOnPath( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ) );
		result = step == static_cast<int>( args[3] ) ? fntech3::nav::NoNode : step;
		return true;
	}
	case Call::G_NAV_GETBESTPATHBETWEENENTS: {
		sharedEntity_t from{}, to{};
		if ( !SV_ReadGameEntity( SV_JKAWorldEntity( args[1], false ), &from )
			|| !SV_ReadGameEntity( SV_JKAWorldEntity( args[2], false ), &to ) ) {
			result = fntech3::nav::NoNode;
			return true;
		}
		const int flags = static_cast<int>( args[3] );
		const int start = jkaNavigation.NearestNode( from.r.currentOrigin, flags,
			SV_JKANavEntity( args[1] ) );
		const int finish = jkaNavigation.NearestNode( to.r.currentOrigin, flags );
		result = jkaNavigation.NextOnPath( start, finish );
		return true;
	}

	case Call::G_NAV_ADDFAILEDNODE:
		jkaNavigation.AddFailedNode( SV_JKANavEntity( args[1] ),
			static_cast<int>( args[2] ) );
		return true;
	case Call::G_NAV_NODEFAILED:
		result = jkaNavigation.NodeFailed( SV_JKANavEntity( args[1] ),
			static_cast<int>( args[2] ) ) ? 1 : 0;
		return true;
	case Call::G_NAV_CHECKFAILEDNODES:
		/* Raven's name says these are looked at again. There is nothing here
		 * to look at them with -- no reachability test and no expiry -- so
		 * nothing is forgotten, and an entity keeps what it failed at until
		 * the map is cleared. That is a gap, stated, not a silent reset. */
		return true;

	case Call::G_NAV_ADDFAILEDEDGE:
		/* Three arguments: the name says the first is an entity and the other
		 * two are the edge. An edge between nodes the graph does not have is
		 * refused by the graph itself, so a wrong reading of the order cannot
		 * store a blockage that is not there. */
		jkaNavigation.AddFailedEdge( static_cast<int>( args[2] ),
			static_cast<int>( args[3] ) );
		return true;
	case Call::G_NAV_EDGEFAILED:
		result = jkaNavigation.EdgeFailed( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ) ) ? 1 : 0;
		return true;
	case Call::G_NAV_CLEARALLFAILEDEDGES:
		jkaNavigation.ClearAllFailedEdges();
		return true;
	case Call::G_NAV_CHECKALLFAILEDEDGES:
		/* Whether anything is standing in the way anywhere. */
		result = jkaNavigation.FailedEdgeCount() != 0 ? 1 : 0;
		return true;
	case Call::G_NAV_CHECKBLOCKEDEDGES:
		/* As with the failed nodes above: nothing here can re-test a blockage,
		 * so nothing is cleared. */
		return true;

	case Call::G_NAV_CLEARCHECKEDNODES:
		jkaNavigation.ClearCheckedNodes();
		return true;
	case Call::G_NAV_CHECKEDNODE:
		result = jkaNavigation.Checked( static_cast<int>( args[1] ) ) ? 1 : 0;
		return true;
	case Call::G_NAV_SETCHECKEDNODE:
		jkaNavigation.SetChecked( static_cast<int>( args[1] ), args[2] != 0 );
		return true;

	case Call::G_NAV_SHOWNODES:
	case Call::G_NAV_SHOWEDGES:
	case Call::G_NAV_SHOWPATH:
		/* Drawing the graph is a client's business, and a dedicated server has
		 * no client. Nothing is drawn and nothing is claimed to be. */
		return true;

	case Call::G_NAV_SAVE:
		/* Nothing is written. A `.nav` file is a cache of what the module just
		 * built, and this engine will not write one into an installation it
		 * was asked only to read. */
		result = 0;
		return true;

	case Call::G_NAV_LOAD:
		result = 0;
		return true;

	case Call::BOTLIB_GET_CONSOLE_MESSAGE:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		result = SV_BotGetConsoleMessage( static_cast<int>( args[1] ),
			static_cast<char *>( pointer( 2 ) ), static_cast<int>( args[3] ) );
		return true;

	case Call::BOTLIB_USER_COMMAND: {
		/* The input frame the game built for one of its bots, handed to the
		 * server as this title's own record and converted by the same adapter
		 * that hands one back. A frame that does not convert is refused: a
		 * bot moved by a record read at another game's offsets would move by
		 * nothing anyone chose. */
		const int clientNum = static_cast<int>( args[1] );
		if ( !SV_IsClientIndex( clientNum ) ) return true;
		VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_jka_usercmd_t ) );
		if ( !args[2] ) Com_Error( ERR_DROP, "Jedi Academy bot command is null" );
		fntech3_jka_usercmd_t external{};
		std::memcpy( &external, pointer( 2 ), sizeof( external ) );
		usercmd_t command{};
		if ( !fntech3::jka::state::ToInternal( external, command ) ) {
			Com_Error( ERR_DROP, "Jedi Academy bot command does not convert" );
		}
		SV_ClientThink( &SV_ClientForIndex( clientNum ), &command );
		return true;
	}

	/* --- how a bot presses its keys ------------------------------------
	 *
	 * Seventeen of these, and they are the whole of how this title's bots
	 * move: each one sets a flag or a value on the input frame the library
	 * builds, and `BOTLIB_EA_GET_INPUT` reads that frame back out.
	 *
	 * Raven's numbers here are not Quake III's. Raven inserted an alternate
	 * attack and a force power into the middle of the block, so everything
	 * after them sits two higher than Quake III puts it -- Quake III's 425 is
	 * the delayed jump, and this title's is the input reset. Each is served by
	 * name for that reason.
	 *
	 * Raven's alternate attack and force power use two otherwise unrelated
	 * Quake III action bits. Only this profile passes those bits on with their
	 * Raven meaning; the previous-jump marker is translated on the way out. */
	case Call::BOTLIB_EA_SAY:
		SV_JKABotActionInterface( args[1] ).EA_Say( SV_JKABotActionClient( args[1] ), text( 2 ) );
		return true;
	case Call::BOTLIB_EA_SAY_TEAM:
		SV_JKABotActionInterface( args[1] ).EA_SayTeam( SV_JKABotActionClient( args[1] ), text( 2 ) );
		return true;
	case Call::BOTLIB_EA_ATTACK:
		SV_JKABotActionInterface( args[1] ).EA_Attack( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_ALT_ATTACK:
		SV_JKABotActionInterface( args[1] ).EA_Action( SV_JKABotActionClient( args[1] ), JKAActionAltAttack );
		return true;
	case Call::BOTLIB_EA_FORCEPOWER:
		SV_JKABotActionInterface( args[1] ).EA_Action( SV_JKABotActionClient( args[1] ), JKAActionForcePower );
		return true;
	case Call::BOTLIB_EA_USE:
		SV_JKABotActionInterface( args[1] ).EA_Use( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_CROUCH:
		SV_JKABotActionInterface( args[1] ).EA_Crouch( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_MOVE_UP:
		SV_JKABotActionInterface( args[1] ).EA_MoveUp( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_MOVE_DOWN:
		SV_JKABotActionInterface( args[1] ).EA_MoveDown( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_MOVE_FORWARD:
		SV_JKABotActionInterface( args[1] ).EA_MoveForward( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_MOVE_BACK:
		SV_JKABotActionInterface( args[1] ).EA_MoveBack( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_MOVE_LEFT:
		SV_JKABotActionInterface( args[1] ).EA_MoveLeft( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_MOVE_RIGHT:
		SV_JKABotActionInterface( args[1] ).EA_MoveRight( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_SELECT_WEAPON:
		SV_JKABotActionInterface( args[1] ).EA_SelectWeapon( SV_JKABotActionClient( args[1] ),
			static_cast<int>( args[2] ) );
		return true;
	case Call::BOTLIB_EA_JUMP:
		SV_JKABotActionInterface( args[1] ).EA_Jump( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_DELAYED_JUMP:
		SV_JKABotActionInterface( args[1] ).EA_DelayedJump( SV_JKABotActionClient( args[1] ) );
		return true;
	case Call::BOTLIB_EA_MOVE:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		SV_JKABotActionInterface( args[1] ).EA_Move( SV_JKABotActionClient( args[1] ),
			static_cast<float *>( pointer( 2 ) ), VMF( 3 ) );
		return true;
	case Call::BOTLIB_EA_VIEW:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		SV_JKABotActionInterface( args[1] ).EA_View( SV_JKABotActionClient( args[1] ),
			static_cast<float *>( pointer( 2 ) ) );
		return true;
	case Call::BOTLIB_EA_END_REGULAR:
		SV_JKABotActionInterface( args[1] ).EA_EndRegular( SV_JKABotActionClient( args[1] ), VMF( 2 ) );
		return true;
	case Call::BOTLIB_EA_GET_INPUT: {
		/* The input frame the library built, read back into the module's own
		 * record. This one carries a record, so the record was measured
		 * rather than assumed: the module's single call site reads its action
		 * flags at offset 32, once as a byte and once as a word it clears
		 * bits in, which is where Quake III keeps them and is not where they
		 * would be had Raven moved anything before them. The layout matches,
		 * but Raven's previous-jump flag has a different value. */
		void *destination = SV_JKABotInputDestination( gvm, args[3] );
		bot_input_t input{};
		SV_JKABotActionInterface( args[1] ).EA_GetInput( SV_JKABotActionClient( args[1] ), VMF( 2 ), &input );
		const bot_input_t output = SV_JKABotActionOutput( input );
		std::memcpy( destination, &output, sizeof( output ) );
		return true;
	}

	case Call::BOTLIB_EA_RESET_INPUT:
		SV_JKABotActionInterface( args[1] ).EA_ResetInput( SV_JKABotActionClient( args[1] ) );
		return true;

	/* --- the state a bot keeps, at Raven's numbers ----------------------
	 *
	 * These eleven are the whole of what this title asks Quake III's bot AI
	 * for, measured from its own module: it allocates and resets a goal, move
	 * and weapon state per bot and never loads a character or a chat state.
	 * The library holds all three without any of the configuration files this
	 * game does not ship, which is why the profile starts none of them.
	 *
	 * Each is served by name. Raven's numbers here are not Quake III's: Quake
	 * III puts the goal-state pair at 496 and 497. */
	case Call::BOTLIB_AI_RESET_GOAL_STATE:
		botlib_export->ai.BotResetGoalState( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_RESET_AVOID_GOALS:
		botlib_export->ai.BotResetAvoidGoals( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_ALLOC_GOAL_STATE:
		result = botlib_export->ai.BotAllocGoalState( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_FREE_GOAL_STATE:
		botlib_export->ai.BotFreeGoalState( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_RESET_MOVE_STATE:
		botlib_export->ai.BotResetMoveState( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_RESET_AVOID_REACH:
		botlib_export->ai.BotResetAvoidReach( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_ALLOC_MOVE_STATE:
		result = botlib_export->ai.BotAllocMoveState();
		return true;
	case Call::BOTLIB_AI_FREE_MOVE_STATE:
		botlib_export->ai.BotFreeMoveState( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_ALLOC_WEAPON_STATE:
		result = botlib_export->ai.BotAllocWeaponState();
		return true;
	case Call::BOTLIB_AI_FREE_WEAPON_STATE:
		botlib_export->ai.BotFreeWeaponState( static_cast<int>( args[1] ) );
		return true;
	case Call::BOTLIB_AI_RESET_WEAPON_STATE:
		botlib_export->ai.BotResetWeaponState( static_cast<int>( args[1] ) );
		return true;

	/* --- the bot library, as far as this title uses it ----------------
	 *
	 * Measured, not assumed: the retail module carries a call thunk for 150
	 * of the 796 import numbers, and not one of them is in the AAS block at
	 * 300-318. Academy's bots do not navigate on Quake III's area awareness.
	 * They ask this library only to set itself up and shut itself down, to
	 * hand over console messages and user commands, and then drive movement
	 * through the elementary-action block at 400-425 and allocate goal, move
	 * and weapon states -- never a chat state and never a character, because
	 * Raven reads its own botfiles/*.jkb rather than Quake III's .c files.
	 * See docs/fntech3/2026-09-21-jka-runtime.md. */

	case Call::BOTLIB_SETUP:
		result = SV_BotLibSetup();
		return true;
	case Call::BOTLIB_SHUTDOWN:
		result = SV_BotLibShutdown();
		return true;

	/* --- recorded motion paths -----------------------------------------
	 *
	 * A `.rof` is a list of steps: how far an object moves and turns during
	 * each one. Retail jamp.exe's measured trajectory updates, millisecond
	 * scheduling, starting-angle translation and export-11 note callbacks are
	 * implemented by this profile's player. The game still evaluates and
	 * links those trajectories in its own frame loop. */

	case Call::G_ROFF_CLEAN:
		jkaRoffPlayer.Clear( sv.time );
		result = 1;
		return true;
	case Call::G_ROFF_CACHE:
		/* The handle is the game's to keep. A path that does not read is
		 * reported as not cached, not remembered as an empty one. */
		result = ravenRoff.Cache( text( 1 ) );
		return true;
	case Call::G_ROFF_PLAY:
		if ( args[1] < 0 || args[1] > INT_MAX || args[2] < 1 || args[2] > INT_MAX ) return true;
		result = jkaRoffPlayer.Play( static_cast<int>( args[1] ), static_cast<int>( args[2] ),
			args[3] != 0, sv.time ) ? 1 : 0;
		return true;
	case Call::G_ROFF_PURGE_ENT:
		if ( args[1] < 0 || args[1] > INT_MAX ) return true;
		result = jkaRoffPlayer.Stop( static_cast<int>( args[1] ), sv.time ) ? 1 : 0;
		return true;
	case Call::G_ROFF_UPDATE_ENTITIES:
		jkaRoffPlayer.Update( sv.time );
		return true;

	/* --- ICARUS: the script interpreter the game asks the engine for ---- */

	case Call::G_ICARUS_INIT:
		jkaIcarus.Initialize();
		return true;
	case Call::G_ICARUS_SHUTDOWN:
		SV_JKAIcarusClear();
		return true;
	case Call::G_ICARUS_ISINITIALIZED:
		result = jkaIcarus.Initialized() ? 1 : 0;
		return true;

	case Call::G_ICARUS_VALIDENT: {
		/* An entity can run scripts when the engine can name it and the game
		 * gave it a name to be known by. */
		const char *name = SV_JKAEntityName( args[1] );
		result = ( name && name[0] ) ? 1 : 0;
		return true;
	}
	case Call::G_ICARUS_INITENT: {
		const fntech3::icarus::EntityId entity = SV_JKAEntityNumber( args[1] );
		if ( entity != fntech3::icarus::NoEntity ) jkaIcarus.Bind( entity );
		return true;
	}
	case Call::G_ICARUS_FREEENT: {
		const fntech3::icarus::EntityId entity = SV_JKAEntityNumber( args[1] );
		if ( entity != fntech3::icarus::NoEntity ) jkaIcarus.Release( entity );
		return true;
	}
	case Call::G_ICARUS_ASSOCIATEENT: {
		/* This is how a script's names become entities: the game hands over an
		 * entity and the engine remembers what it is called. */
		const fntech3::icarus::EntityId entity = SV_JKAEntityNumber( args[1] );
		const char *name = SV_JKAEntityName( args[1] );
		if ( entity != fntech3::icarus::NoEntity && name && name[0] ) {
			jkaIcarusNames[name] = entity;
		}
		return true;
	}

	case Call::G_ICARUS_RUNSCRIPT: {
		const fntech3::icarus::EntityId entity = SV_JKAEntityNumber( args[1] );
		if ( entity == fntech3::icarus::NoEntity ) { result = 0; return true; }
		fntech3::icarus::LoadError error = fntech3::icarus::LoadError::None;
		result = jkaIcarus.RunScript( entity, text( 2 ), error ) ? 1 : 0;
		if ( !result ) {
			Com_Printf( "ICARUS: %s: %s\n", text( 2 ),
				fntech3::icarus::LoadErrorName( error ) );
		}
		return true;
	}
	case Call::G_ICARUS_REGISTERSCRIPT:
		/* Reading a script ahead of time only warms the cache, so a script
		 * that is not there is reported and nothing else happens. */
		result = 1;
		return true;

	case Call::G_ICARUS_MAINTAINTASKMANAGER:
		jkaIcarus.TickEntity( fntech3::icarus::EntityId( args[1] ) );
		return true;
	case Call::G_ICARUS_ISRUNNING: {
		const fntech3::icarus::Sequencer *sequencer =
			jkaIcarus.Find( fntech3::icarus::EntityId( args[1] ) );
		result = ( sequencer && sequencer->Busy() ) ? 1 : 0;
		return true;
	}

	case Call::G_ICARUS_TASKIDSET: {
		/* The game keeps the identifier the engine gave it in one of the
		 * entity's own slots, so that it can say later which one finished. */
		const fntech3::icarus::EntityId entity = SV_JKAEntityNumber( args[1] );
		const int slot = static_cast<int>( args[2] );
		if ( entity == fntech3::icarus::NoEntity || slot < 0
			|| slot >= FNTECH3_JKA_NUM_TASK_IDS ) return true;
		auto *shared = reinterpret_cast<fntech3_jka_sharedEntity_t *>( args[1] );
		shared->taskID[slot] = static_cast<int32_t>( args[3] );
		return true;
	}
	case Call::G_ICARUS_TASKIDCOMPLETE: {
		const fntech3::icarus::EntityId entity = SV_JKAEntityNumber( args[1] );
		const int slot = static_cast<int>( args[2] );
		if ( entity == fntech3::icarus::NoEntity || slot < 0
			|| slot >= FNTECH3_JKA_NUM_TASK_IDS ) return true;
		auto *shared = reinterpret_cast<fntech3_jka_sharedEntity_t *>( args[1] );
		jkaIcarus.CompleteTask( shared->taskID[slot] );
		shared->taskID[slot] = -1;
		return true;
	}
	case Call::G_ICARUS_TASKIDPENDING: {
		const fntech3::icarus::EntityId entity = SV_JKAEntityNumber( args[1] );
		const int slot = static_cast<int>( args[2] );
		if ( entity == fntech3::icarus::NoEntity || slot < 0
			|| slot >= FNTECH3_JKA_NUM_TASK_IDS ) { result = 0; return true; }
		const auto *shared = reinterpret_cast<const fntech3_jka_sharedEntity_t *>( args[1] );
		result = jkaIcarus.TaskPending( shared->taskID[slot] ) ? 1 : 0;
		return true;
	}

	case Call::G_ICARUS_SETVAR: {
		fntech3::icarus::HostValue value;
		value.kind = fntech3::icarus::ValueKind::Text;
		value.text = text( 4 );
		result = jkaIcarus.SetVariable( text( 3 ), value ) ? 1 : 0;
		return true;
	}
	case Call::G_ICARUS_VARIABLEDECLARED:
		result = jkaIcarus.Declared( text( 1 ) ) ? 1 : 0;
		return true;
	case Call::G_ICARUS_GETFLOATVARIABLE: {
		fntech3::icarus::HostValue value;
		if ( !jkaIcarus.GetVariable( text( 1 ),
			fntech3::icarus::ValueKind::Number, value ) ) { result = 0; return true; }
		VM_CHECKBOUNDS( gvm, args[2], sizeof( float ) );
		*static_cast<float *>( pointer( 2 ) ) = value.numbers[0];
		result = 1;
		return true;
	}
	case Call::G_ICARUS_GETVECTORVARIABLE: {
		fntech3::icarus::HostValue value;
		if ( !jkaIcarus.GetVariable( text( 1 ),
			fntech3::icarus::ValueKind::Vector, value ) ) { result = 0; return true; }
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		auto *out = static_cast<float *>( pointer( 2 ) );
		for ( int i = 0; i < 3; ++i ) out[i] = value.numbers[i];
		result = 1;
		return true;
	}
	case Call::G_ICARUS_GETSTRINGVARIABLE: {
		fntech3::icarus::HostValue value;
		if ( !jkaIcarus.GetVariable( text( 1 ),
			fntech3::icarus::ValueKind::Text, value ) ) { result = 0; return true; }
		VM_CHECKBOUNDS( gvm, args[2], FNTECH3_JKA_ICARUS_STRING_BYTES );
		Q_strncpyz( static_cast<char *>( pointer( 2 ) ), value.text ? value.text : "",
			FNTECH3_JKA_ICARUS_STRING_BYTES );
		result = 1;
		return true;
	}

	/* Two arithmetic calls of Raven's own, just past the shared block. */
	case Call::G_ACOS:
		result = FloatAsInt( Q_acos( VMF( 1 ) ) );
		return true;
	case Call::G_ASIN:
		result = FloatAsInt( SV_JKAArcSine( VMF( 1 ) ) );
		return true;

	default:
		break;
	}

	const Descriptor *descriptor = Describe( call );
	/* Say whether the module had already published its arrays when it asked,
	 * so a probe records how far initialization actually got rather than
	 * leaving it to be inferred from the order of Raven's own source. */
	Com_Error( ERR_DROP, "Jedi Academy game import %s (%d) is not served yet: "
		"its module boundary is still open. Its arrays %s. See "
		"docs/fntech3/COMPATIBILITY.md",
		descriptor ? descriptor->name : "unknown", static_cast<int>( args[0] ),
		sv.gentities ? "are bound" : "are not bound yet" );
	return true;
}

#endif
