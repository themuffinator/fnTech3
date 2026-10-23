/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_ALICE_CGAME_HPP
#define FNTECH3_CLIENT_ALICE_CGAME_HPP

/* Alice's client game boundary, as far as it reaches.
 *
 * `base/cgamex86.dll` is a function table like the game library, and a
 * stranger one. Its entry point `GetCGameAPI` takes no argument at all: it
 * writes seven function pointers into a table of its own and returns that
 * address. The engine's import table arrives through the *first of those
 * exports* instead, which copies 125 slots with one `rep movsd`, clears its
 * own state, and then immediately calls the last slot it was given.
 *
 * So the order here is not the game's. There, the engine handed over imports
 * and received exports in one call; here it receives exports first and hands
 * the imports to one of them. That is why this file exists separately rather
 * than reusing the game's loader.
 *
 * Every one of the 125 slots is filled before that call, because the module
 * calls slot 124 before it does anything else and a null there would fault
 * inside the module with nothing to say which slot was missing. Slots this
 * engine can serve get a service; the rest get a thunk that drops by name.
 *
 * Twenty-one of the slots hold an engine function the game import table also
 * holds, so the game map names them by address -- the printf family, the cvar
 * pair, the command accessors, the file calls and the global alias block.
 * Twenty-two more name themselves in their own bodies. See
 * docs/fntech3/2026-09-22-alice-module-abi.md; none of that naming is assumed
 * from position. */

#include "../compat/alice_game_abi.h"
#include "../compat/alice_module_abi.h"
#include "../compat/alice_state_adapter.hpp"
#include "../compat/alice_render_abi.h"
#include "../compat/fakk2_convert.hpp"
#include "../qcommon/alice_runtime.h"
#include "../qcommon/alice_tiki.h"

#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace {

using fntech3::alice::module::CGameImportTable;

CGameImportTable aliceCGameImport;
CGameImportTable aliceCGameUnserved;
const std::intptr_t *aliceCGameExports;

/*
==================
CL_AliceUnservedCGameImport

Every client-game slot this engine does not serve. As with the game's, the
point is that the module runs until it needs something real and then the log
names it, rather than faulting inside the module on a null.

The slot numbers here are the client game's own and do not correspond to the
game table's: the two tables are different lengths and different orders.
==================
*/
template<int Slot>
intptr_t QDECL CL_AliceUnservedCGameImport() {
	const bool fakk2 = Com_GameIsFAKK2() != 0;
	Com_Error( ERR_DROP,
		"%s client game called import %i, which fnTech3 does not serve. "
		"The module ran to this point; this call is the next thing it needs. "
		"See docs/fntech3/COMPATIBILITY.md#%s.", fakk2 ? "F.A.K.K.2's" : "Alice's", Slot,
		fakk2 ? "fakk2-gate-matrix" : "alice-gate-matrix" );
	return 0;
}

template<std::size_t... Slots>
void CL_AliceFillUnservedCGame( std::index_sequence<Slots...> ) {
	( ( aliceCGameUnserved.slot[Slots] = reinterpret_cast<std::intptr_t>(
		&CL_AliceUnservedCGameImport<static_cast<int>( Slots )> ) ), ... );
	aliceCGameImport = aliceCGameUnserved;
}

int CL_AliceServedCGameImports() {
	int served = 0;

	for ( int i = 0; i < FNTECH3_ALICE_CGAME_IMPORT_SLOTS; i++ ) {
		served += aliceCGameImport.slot[i] != aliceCGameUnserved.slot[i];
	}
	return served;
}

/* ---- the slots this engine serves --------------------------------------- */

/* Slots 1, 2 and 3 hold the same three engine functions the game table's
 * printf family holds, by address, and slot 6 holds its error call. */
void QDECL CL_AlicePrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_Printf( "%s", text );
}

void QDECL CL_AliceDPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_DPrintf( "%s", text );
}

void QDECL CL_AliceError( int level, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_Error( ERR_DROP, "Alice client game error (level %i): %s", level, text );
}

/* Slots 4 and 5, the allocator pair. The retail engine fills them with two
 * one-line wrappers: the first calls its zone allocator with the size and a
 * fixed tag, the second calls its free with the pointer. Paired, so what one
 * hands out the other has to take back, and both go through the allocator the
 * game module's own pair uses. */
void *QDECL CL_AliceMalloc( int size ) {
	if ( size < 0 ) {
		Com_Error( ERR_DROP, "Alice's client game asked for %i bytes", size );
		return nullptr;
	}
	return Z_TagMalloc( size ? size : 1, TAG_GENERAL );
}

void QDECL CL_AliceFree( void *block ) {
	if ( block ) {
		Z_Free( block );
	}
}

/* Slots 8 and 9, the cvar pair, at the argument counts the client game's own
 * call sites give: three to fetch, two to set. The record handed back is the
 * one the game table's slot 7 hands back, and the same measurement covers it. */
cvar_t *QDECL CL_AliceCvar( const char *name, const char *value, int flags ) {
	if ( !name || !value ) {
		return nullptr;
	}
	return Cvar_Get( name, value, flags );
}

void QDECL CL_AliceCvarSet( const char *name, const char *value ) {
	if ( name && value ) {
		Cvar_Set( name, value );
	}
}

/* Slots 10 to 12, the command accessors. */
int QDECL CL_AliceArgc() {
	return Cmd_Argc();
}

const char *QDECL CL_AliceArgv( int arg ) {
	return Cmd_Argv( arg );
}

const char *QDECL CL_AliceArgs() {
	return Cmd_ArgsFrom( 1 );
}

/* Slot 13. The retail engine fills it with a routine that calls its
 * `Cmd_AddCommand` with the name and a null handler -- it walks the command
 * list comparing names, and links a three-word node when it finds none. A
 * null handler is how this lineage says "the module will answer this", and
 * fnTech3's own `Cmd_AddCommand` already takes one. */
void QDECL CL_AliceAddCommand( const char *name ) {
	if ( name && name[0] ) {
		Cmd_AddCommand( name, nullptr );
	}
}

/* Slots 14 to 17, the file calls the client game shares with the game. The
 * read's third argument is retail's `quiet`, which keeps back the open's
 * developer line for a missing file (0x0043c9c8, 0x0043c1fa..0x0043c204). */
int QDECL CL_AliceReadFile( const char *path, void **buffer, int quiet ) {
	const int length = FS_ReadFile( path, buffer );
	if ( length < 0 && !quiet && path ) {
		Com_DPrintf( "Can't find %s\n", path );
	}
	return length;
}

void QDECL CL_AliceFreeFile( void *buffer ) {
	if ( buffer ) {
		FS_FreeFile( buffer );
	}
}

void QDECL CL_AliceWriteFile( const char *path, const void *buffer, int size ) {
	if ( path && buffer && size >= 0 ) {
		FS_WriteFile( path, buffer, size );
	}
}

/* Slot 18, the console command, appended rather than run where it stands. */
void QDECL CL_AliceSendConsoleCommand( const char *text ) {
	if ( text && text[0] ) {
		Cbuf_AddText( text );
	}
}

/* Slot 21, the collision load. The retail engine's wrapper calls its own
 * loader with the name, a one, and the address of its own argument slot as
 * the place to put the checksum -- which is this lineage's
 * `CM_LoadMap( name, clientload, &checksum )` with the client flag set. The
 * checksum it collects goes nowhere the module can see, so it is collected
 * and dropped here too. */
void QDECL CL_AliceLoadMap( const char *name ) {
	int checksum = 0;

	if ( !name || !name[0] ) {
		Com_Error( ERR_DROP, "Alice's client game asked to load a map with "
			"no name" );
		return;
	}
	CM_LoadMap( name, qtrue, &checksum );
}

/* Slot 74, the game state: the configstrings the client game reads the level
 * out of. The retail engine copies 21,316 bytes into the module's buffer,
 * which is 1,328 offsets, 16,000 bytes of string data and the count of what
 * is used -- the same three members this engine keeps, at Alice's own
 * configstring count rather than the 2,048 this engine sizes its own array
 * for. The string data and the count cross unchanged, because the offsets
 * index into that data and stay valid; only the array of offsets is shorter,
 * and a configstring past its end could not have been set, since the profile
 * bounds the module to 1,328 as well. */
void QDECL CL_AliceGetGameState( fntech3_alice_gamestate_t *state ) {
	if ( !state ) {
		Com_Error( ERR_DROP, "Alice's client game asked for the game state "
			"with nowhere to put it" );
		return;
	}
	/* The engine's store is sized for the largest profile's game state
	 * (Medal of Honor's); Alice's parser holds hers to 16,000 bytes, so the
	 * first 16,000 are all of it and the count is checked before it crosses. */
	static_assert( sizeof( state->stringData ) <= sizeof( cl.gameState.stringData ),
		"Alice's string data fits in this engine's" );
	if ( cl.gameState.dataCount < 0 ||
		cl.gameState.dataCount > static_cast<int>( sizeof( state->stringData ) ) ) {
		Com_Error( ERR_DROP, "Alice's game state holds %i bytes of strings, "
			"past the %i this title has", cl.gameState.dataCount,
			static_cast<int>( sizeof( state->stringData ) ) );
		return;
	}
	*state = {};

	for ( int i = 0; i < FNTECH3_ALICE_MAX_CONFIGSTRINGS; i++ ) {
		state->stringOffsets[i] = cl.gameState.stringOffsets[i];
	}
	/* A configstring this engine holds past Alice's count would leave the
	 * module reading a level it cannot see all of, so it stops here instead. */
	for ( int i = FNTECH3_ALICE_MAX_CONFIGSTRINGS;
		i < MAX_GAME_CONFIGSTRINGS; i++ ) {
		if ( cl.gameState.stringOffsets[i] ) {
			Com_Error( ERR_DROP, "Alice's game state holds configstring %i, "
				"past the %i this title has", i,
				FNTECH3_ALICE_MAX_CONFIGSTRINGS );
			return;
		}
	}
	std::memcpy( state->stringData, cl.gameState.stringData,
		sizeof( state->stringData ) );
	state->dataCount = cl.gameState.dataCount;
}

/* Slot 77, the graphics configuration, which the client game asks for before
 * it draws anything. The retail engine fills it by copying 5,188 bytes out of
 * its own; this engine's record has the same members in the same order with
 * larger string buffers, so each one is copied across and each string is
 * truncated into the room Alice's record has for it.
 *
 * The three members whose positions are anchored -- the renderer name at the
 * front, the width and height the engine pushes into `glViewport`, and the
 * flag its screen update tests for stereo -- are the ones a client game would
 * notice first if any of this were placed wrongly. */
void QDECL CL_AliceGetGlconfig( fntech3_alice_glconfig_t *config ) {
	if ( !config ) {
		Com_Error( ERR_DROP, "Alice's client game asked for the graphics "
			"configuration with nowhere to put it" );
		return;
	}
	const glconfig_t &from = cls.glconfig;
	*config = {};

	Q_strncpyz( config->renderer_string, from.renderer_string,
		sizeof( config->renderer_string ) );
	Q_strncpyz( config->vendor_string, from.vendor_string,
		sizeof( config->vendor_string ) );
	Q_strncpyz( config->version_string, from.version_string,
		sizeof( config->version_string ) );
	Q_strncpyz( config->extensions_string, from.extensions_string,
		sizeof( config->extensions_string ) );

	config->maxTextureSize = from.maxTextureSize;
	config->maxActiveTextures = from.numTextureUnits;
	config->colorBits = from.colorBits;
	config->depthBits = from.depthBits;
	config->stencilBits = from.stencilBits;
	config->driverType = (int)from.driverType;
	config->hardwareType = (int)from.hardwareType;
	config->deviceSupportsGamma = from.deviceSupportsGamma != qfalse;
	config->textureCompression = (int)from.textureCompression;
	config->textureEnvAddAvailable = from.textureEnvAddAvailable != qfalse;
	config->vidWidth = from.vidWidth;
	config->vidHeight = from.vidHeight;
	config->windowAspect = from.windowAspect;
	config->displayFrequency = from.displayFrequency;
	config->isFullscreen = from.isFullscreen != qfalse;
	config->stereoEnabled = from.stereoEnabled != qfalse;
	config->smpActive = from.smpActive != qfalse;
}

/* Slot 7. The retail target reads the journal-aware millisecond clock. */
int QDECL CL_AliceMilliseconds() {
	return Sys_Milliseconds();
}

/* Slot 17. Retail opens the file for text output and writes the buffer; the
 * module writes its own console log and bindings through it. */
void QDECL CL_AliceWriteTextFile( const char *path, const void *buffer, int size ) {
	if ( path && buffer && size >= 0 ) {
		FS_WriteFile( path, buffer, size );
	}
}

/* Slot 19. Retail's target is its screen update, which the module calls while
 * it registers media so the loading screen keeps moving. */
void QDECL CL_AliceUpdateLoadingScreen() {
	CL_AliceLoadingScreenRefresh();
}

/* Slot 20, the reliable client command. */
void QDECL CL_AliceSendClientCommand( const char *text ) {
	if ( text ) {
		CL_AddReliableCommand( text, qfalse );
	}
}

/* Slots 22 to 25 and 28, the collision queries the client game makes against
 * the map it loaded through slot 21. The retail targets are the engine's own
 * collision routines, which fnTech3's share: the handles are the ones this
 * engine's collision model hands out. */
clipHandle_t QDECL CL_AliceInlineModel( int index ) {
	return CM_InlineModel( index );
}

int QDECL CL_AliceNumInlineModels() {
	return CM_NumInlineModels();
}

int QDECL CL_AlicePointContents( const vec3_t point, int headnode ) {
	return CM_PointContents( point, headnode );
}

int QDECL CL_AliceTransformedPointContents( const vec3_t point, int headnode,
	const vec3_t origin, const vec3_t angles ) {
	return CM_TransformedPointContents( point, headnode, origin, angles );
}

/* Slots 50, 52, 55, 56 and 61: the renderer entry points that take no record
 * of Alice's own. Retail copies them straight out of its renderer's export
 * block, so the module calls the renderer directly; here they reach this
 * engine's renderer through its own export table. */
extern int aliceNumSceneEntities;

void QDECL CL_AliceClearScene() {
	aliceNumSceneEntities = 0;
	re.ClearScene();
}

void QDECL CL_AliceLoadWorldMap( const char *name ) {
	if ( !name || !name[0] ) {
		Com_Error( ERR_DROP, "Alice's client game asked for a world with no name" );
		return;
	}
	re.LoadWorld( name );
}

qhandle_t QDECL CL_AliceRegisterShader( const char *name ) {
	return name && name[0] ? re.RegisterShader( name ) : 0;
}

qhandle_t QDECL CL_AliceRegisterShaderNoMip( const char *name ) {
	return name && name[0] ? re.RegisterShaderNoMip( name ) : 0;
}

/* Slot 53. An inline brush model -- `*1` and up -- is the world's own
 * geometry; a `.tik` is a model definition the renderer draws from the
 * engine's shared cache, and a `.spr` a sprite. The renderer tells them
 * apart by name, as retail's does (0x004895f0). */
qhandle_t QDECL CL_AliceRegisterModel( const char *name ) {
	if ( !name || !name[0] ) {
		return 0;
	}
	return re.RegisterModel( name );
}

/* Slots 64 and 65, a registered model's extent, at the three and one
 * arguments the module's sites push. The radius is the one Quake III's
 * renderer derives from the same bounds: the length of the larger corner. */
void QDECL CL_AliceModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs ) {
	if ( mins && maxs ) {
		re.ModelBounds( model, mins, maxs );
	}
}

/* Slot 65. Retail's R_ModelRadius (0x0048a980) answers a
 * definition's own radius and a sprite's half size; other models, and a
 * renderer that cannot say, fall back to the radius of the model's bounds. */
float QDECL CL_AliceModelRadius( qhandle_t model ) {
	vec3_t mins, maxs;

	if ( re.GetAliceModelRadius ) {
		const float radius = re.GetAliceModelRadius( model );
		if ( radius >= 0.0f ) {
			return radius;
		}
	}
	re.ModelBounds( model, mins, maxs );
	return RadiusFromBounds( mins, maxs );
}

void QDECL CL_AliceSetColor( const float *rgba ) {
	re.SetColor( rgba );
}

/* Slot 62, at the nine arguments both of the module's call sites push. */
void QDECL CL_AliceDrawStretchPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}

/* Slots 76, 79 and 80, the client's own record of where it is. */
void QDECL CL_AliceGetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	if ( snapshotNumber && serverTime ) {
		CL_GetCurrentSnapshotNumber( snapshotNumber, serverTime );
	}
}

int QDECL CL_AliceGetCurrentCmdNumber() {
	return CL_GetCurrentCmdNumber();
}

/* The command is Alice's twenty-byte record, converted from this engine's own
 * by the adapter the wire codec uses for the same record. */
qboolean QDECL CL_AliceGetUserCmd( int cmdNumber, fntech3_alice_usercmd_t *command ) {
	usercmd_t internal;

	if ( !command || !CL_GetUserCmd( cmdNumber, &internal ) ) {
		return qfalse;
	}
	return fntech3::alice::state::FromInternal( internal, *command ) ? qtrue : qfalse;
}

/*
==================
CL_AliceGetServerCommand

Slot 81. Retail's target at 0x00424300 bounds the number against the last
128 commands, tokenizes the command and hands it to the routine at 0x00424100,
which claims six commands before the module sees them:

  disconnect       drops the connection
  cs               updates the configstring, re-tokenizes, and passes it on
  fadesound        fades the sound system and swallows the command
  updatemenus      refreshes the menus and swallows it
  restart          clears the client's per-level record and passes it on
  clientLevelShot  queues a level shot and swallows it

fnTech3's own accessor already bounds the ring, drops on `disconnect` and
updates configstrings on `cs`, so it is used for those; the other four are
handled here.
==================
*/
qboolean QDECL CL_AliceGetServerCommand( int serverCommandNumber ) {
	if ( !CL_GetServerCommand( serverCommandNumber ) ) {
		return qfalse;
	}

	const char *cmd = Cmd_Argv( 0 );
	if ( !strcmp( cmd, "fadesound" ) ) {
		/* S_FadeSound( atof( argv1 ) ) at 0x004241f6: the sound system fades
		 * out over that many milliseconds and stays silent until the fade is
		 * cancelled at the next disconnect or level. */
		S_AliceFadeSound( static_cast<float>( atof( Cmd_Argv( 1 ) ) ) );
		return qfalse;
	}
	if ( !strcmp( cmd, "updatemenus" ) ) {
		return qfalse;
	}
	if ( !strcmp( cmd, "clientLevelShot" ) ) {
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		return qfalse;
	}
	return qtrue;
}

/* ---- the scene -------------------------------------------------------- */

/* The entities the module submitted this frame, in its own 460-byte layout,
 * because slot 63 hands one back: retail's renderer searches its entity list
 * by entity number and returns its copy of the record (0x00491600). The list
 * is a fixed array, as retail's is, so a record handed back stays where it is
 * until the scene is cleared. */
std::array<fntech3_alice_refEntity_t, FNTECH3_ALICE_MAX_RENDER_ENTITIES> aliceSceneEntities;
int aliceNumSceneEntities;

/* Alice's render flags, at its own bit positions, onto this engine's. The
 * ones with no counterpart here -- the lens flares, the sky entity, the
 * shadows -- are not mapped; the entity carries Alice's own flags too, and
 * the renderer reads those for them. */
int CL_AliceRenderFx( int aliceFx ) {
	int fx = 0;
	if ( aliceFx & FNTECH3_ALICE_RF_THIRD_PERSON ) fx |= RF_THIRD_PERSON;
	if ( aliceFx & FNTECH3_ALICE_RF_FIRST_PERSON ) fx |= RF_FIRST_PERSON;
	if ( aliceFx & FNTECH3_ALICE_RF_DEPTHHACK ) fx |= RF_DEPTHHACK;
	if ( aliceFx & FNTECH3_ALICE_RF_MINLIGHT ) fx |= RF_MINLIGHT;
	if ( aliceFx & FNTECH3_ALICE_RF_LIGHTING_ORIGIN ) fx |= RF_LIGHTING_ORIGIN;
	if ( aliceFx & FNTECH3_ALICE_RF_SHADOW_PLANE ) fx |= RF_SHADOW_PLANE;
	return fx;
}

/* The common part of an entity: placement, lighting, skin and colour. */
void CL_AliceBaseEntity( const fntech3_alice_refEntity_t &from, refEntity_t &to ) {
	to = {};
	to.renderfx = CL_AliceRenderFx( from.renderfx );
	to.alice.renderfx = from.renderfx;
	to.hModel = from.hModel;
	VectorCopy( from.lightingOrigin, to.lightingOrigin );
	to.shadowPlane = from.shadowPlane;
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( from.axis[i], to.axis[i] );
	}
	to.nonNormalizedAxes = from.nonNormalizedAxes ? qtrue : qfalse;
	VectorCopy( from.origin, to.origin );
	VectorCopy( from.oldorigin, to.oldorigin );
	to.frame = from.frame;
	to.oldframe = from.oldframe;
	to.backlerp = from.backlerp;
	to.skinNum = from.skinNum;
	to.customSkin = from.customSkin;
	to.customShader = from.customShader;
	std::memcpy( &to.shader, from.shaderRGBA, sizeof( from.shaderRGBA ) );
	to.shaderTexCoord[0] = from.shaderTexCoord[0];
	to.shaderTexCoord[1] = from.shaderTexCoord[1];
	to.shaderTime.f = from.shaderTime;
	to.radius = from.radius;
	to.rotation = from.rotation;
	to.alice.scale = from.scale > 0.0f ? from.scale : 1.0f;
}

/* Slot 57. Retail drops the session for a type at or past eight
 * (0x00491675, `bad reType`) and, at draw time, for type 1; a sprite submitted
 * here (type 2) is drawn as a sprite after a developer warning (0x00478c92).
 * Types 3 to 6 are Quake III's beam, rail core, rail rings and lightning,
 * which retail's surface table dispatches one by one (0x004af410) -- the names
 * are inferred from that order, which is Quake III's own.
 *
 * The client game suppresses hidden network models before this call
 * (cgamex86.dll 0x300263da). Submitted effects can inherit RF_DONTDRAW;
 * retail copies them without another visibility test (0x00491650). */
void QDECL CL_AliceAddRefEntityToScene( const fntech3_alice_refEntity_t *e ) {
	if ( !e ) {
		return;
	}
	if ( e->reType < 0 || e->reType >= FNTECH3_ALICE_RT_MAX ) {
		Com_Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", e->reType );
		return;
	}
	if ( e->reType == 1 ) {
		Com_Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );
		return;
	}
	if ( aliceNumSceneEntities >= FNTECH3_ALICE_MAX_RENDER_ENTITIES ) {
		return;
	}
	aliceSceneEntities[aliceNumSceneEntities++] = *e;

	refEntity_t ent;
	CL_AliceBaseEntity( *e, ent );
	switch ( e->reType ) {
	case FNTECH3_ALICE_RT_SPRITE:
		Com_DPrintf( "Sprite being added to entities, should be added to sprite list\n" );
		ent.reType = RT_ALICE_SPRITE;
		ent.alice.scale = e->scale;
		break;
	case FNTECH3_ALICE_RT_BEAM:
		ent.reType = RT_BEAM;
		break;
	case FNTECH3_ALICE_RT_BEAM + 1:
		ent.reType = RT_RAIL_CORE;
		break;
	case FNTECH3_ALICE_RT_BEAM + 2:
		ent.reType = RT_RAIL_RINGS;
		break;
	case FNTECH3_ALICE_RT_BEAM + 3:
		ent.reType = RT_LIGHTNING;
		break;
	case FNTECH3_ALICE_RT_PORTALSURFACE:
		ent.reType = RT_PORTALSURFACE;
		break;
	default:
		ent.reType = RT_MODEL;
		break;
	}

	aliceRenderEntity_t &a = ent.alice;
	a.anim = e->anim;
	a.oldanim = e->oldanim;
	a.frame = e->frame;
	a.oldframe = e->oldframe;
	a.backlerp = e->backlerp;
	a.torsoAnim = e->torso_anim;
	a.torsoOldanim = e->torso_oldanim;
	a.torsoFrame = e->torso_frame;
	a.torsoOldframe = e->torso_oldframe;
	a.torsoBacklerp = e->torso_backlerp;
	a.crossblendAnim = e->crossblend_anim;
	a.crossblendFrame = e->crossblend_frame;
	a.crossblendLerp = e->crossblend_lerp;
	a.torsoCrossblendAnim = e->torso_crossblend_anim;
	a.torsoCrossblendFrame = e->torso_crossblend_frame;
	a.torsoCrossblendLerp = e->torso_crossblend_lerp;
	a.useLegs = e->uselegs ? qtrue : qfalse;
	a.useTorso = e->usetorso ? qtrue : qfalse;
	a.useControllers = e->useAngles ? qtrue : qfalse;
	a.customShaderPass = ( e->renderfx & FNTECH3_ALICE_RF_CUSTOMSHADERPASS ) ? qtrue : qfalse;
	std::memcpy( a.surfaces, e->surfaces, sizeof( a.surfaces ) );
	a.shaderData[0] = e->shader_data[0];
	a.shaderData[1] = e->shader_data[1];
	for ( int i = 0; i < FNTECH3_ALICE_BONE_CONTROLLERS; i++ ) {
		a.controllers[i].bone = e->bonectrl[i].tag;
		std::memcpy( a.controllers[i].quat, e->bonectrl[i].quat, sizeof( a.controllers[i].quat ) );
		a.controllers[i].scale = e->bonectrl[i].scale;
	}
	re.AddRefEntityToScene( &ent, qfalse );
}

/* Slot 58. Both Ritual engines accept sprites from hidden emitters:
 * Alice's RE_AddRefSpriteToScene (0x00491700..0x004917d2), like F.A.K.K.2's
 * (1.02, 0x0047b760), copies RF_DONTDRAW without testing it. The client game
 * already hides network sprites (Alice 0x3002208a), but its particles inherit
 * the carrier's flags. Rejecting those removes Alice's targeting glow. */
void QDECL CL_AliceAddRefSpriteToScene( const fntech3_alice_refEntity_t *e ) {
	if ( !e ) {
		return;
	}
	refEntity_t ent;
	CL_AliceBaseEntity( *e, ent );
	ent.reType = RT_ALICE_SPRITE;
	ent.alice.scale = e->scale; // zero-sized particles stay zero-sized
	ent.customShader = 0;
	re.AddRefEntityToScene( &ent, qfalse );
}

/* Slot 63. */
fntech3_alice_refEntity_t *QDECL CL_AliceGetRenderEntity( int entityNumber ) {
	for ( int i = 0; i < aliceNumSceneEntities; i++ ) {
		if ( aliceSceneEntities[i].entityNumber == entityNumber ) {
			return &aliceSceneEntities[i];
		}
	}
	return nullptr;
}

/* Slot 59, a dynamic light. Its last argument is a set of flags -- a lens
 * flare, a view lens flare, additive -- which the renderer's Alice path takes
 * whole (retail 0x004a8808 gives bits 1 and 2 a flare); without it, only the
 * additive bit is kept. */
void QDECL CL_AliceAddLightToScene( const float *origin, float intensity, float r, float g,
	float b, int type ) {
	if ( !origin ) {
		return;
	}
	if ( re.AddAliceLightToScene ) {
		re.AddAliceLightToScene( origin, intensity, r, g, b, type );
		return;
	}
	if ( type & 4 ) {
		re.AddAdditiveLightToScene( origin, intensity, r, g, b );
	} else {
		re.AddLightToScene( origin, intensity, r, g, b );
	}
}

/* Slot 60. Alice's polygon vertex is Quake III's 24 bytes, so the vertices
 * go across as they are. */
void QDECL CL_AliceAddPolyToScene( qhandle_t shader, int numVerts,
	const fntech3_alice_polyVert_t *verts, int renderfx ) {
	(void)renderfx;
	static_assert( sizeof( fntech3_alice_polyVert_t ) == sizeof( polyVert_t ),
		"Alice's polygon vertex is this engine's" );
	if ( !verts || numVerts < 3 || !shader ) {
		return;
	}
	re.AddPolyToScene( shader, numVerts, reinterpret_cast<const polyVert_t *>( verts ), 1 );
}

/* Slot 51, the view: Quake III's own, and Ritual's far plane and sky portal
 * (retail's RE_RenderScene 0x00491870 copies them into its view), which the
 * renderer draws under this profile. */
void QDECL CL_AliceRenderScene( const fntech3_alice_refdef_t *fd ) {
	if ( !fd ) {
		return;
	}
	refdef_t view{};
	view.x = fd->x;
	view.y = fd->y;
	view.width = fd->width;
	view.height = fd->height;
	view.fov_x = fd->fov_x;
	view.fov_y = fd->fov_y;
	VectorCopy( fd->vieworg, view.vieworg );
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( fd->viewaxis[i], view.viewaxis[i] );
	}
	view.time = fd->time;
	view.rdflags = fd->rdflags & ( RDF_NOWORLDMODEL | RDF_HYPERSPACE );
	static_assert( sizeof( fd->areamask ) <= sizeof( view.areamask ), "area mask" );
	std::memcpy( view.areamask, fd->areamask, sizeof( fd->areamask ) );
	static_assert( sizeof( fd->text ) == sizeof( view.text ), "view text" );
	std::memcpy( view.text, fd->text, sizeof( view.text ) );
	aliceRenderView_t &alice = view.alice;
	alice.farplaneDistance = fd->farplane_distance;
	VectorCopy( fd->farplane_color, alice.farplaneColor );
	alice.farplaneCull = fd->farplane_cull ? qtrue : qfalse;
	alice.skyPortal = fd->sky_portal ? qtrue : qfalse;
	alice.skyAlpha = fd->sky_alpha;
	VectorCopy( fd->sky_origin, alice.skyOrigin );
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( fd->sky_axis[i], alice.skyAxis[i] );
	}
	alice.keepDepth = fd->keepDepth ? qtrue : qfalse;
	CL_EntityEditorAdjustView( &view );
	CL_LiquidFeedView( &view );
	re.RenderScene( &view );
}

/* Slots 71 and 72. */
int QDECL CL_AliceGetShaderWidth( qhandle_t shader ) {
	int width = 0, height = 0;
	if ( re.GetShaderImageSize ) re.GetShaderImageSize( shader, &width, &height );
	return width;
}

int QDECL CL_AliceGetShaderHeight( qhandle_t shader ) {
	int width = 0, height = 0;
	if ( re.GetShaderImageSize ) re.GetShaderImageSize( shader, &width, &height );
	return height;
}

/* Slot 86. */
int QDECL CL_AliceTikiGetHandle( qhandle_t model ) {
	return re.GetAliceTikiHandle ? re.GetAliceTikiHandle( model ) : -1;
}

/* ---- collision -------------------------------------------------------- */

/* Alice's trace is Quake III's first 56 bytes and a game-side entity pointer
 * the client never fills. */
void CL_AliceCopyTrace( fntech3_alice_trace_t *out, const trace_t &trace ) {
	static_assert( sizeof( trace_t ) <= sizeof( fntech3_alice_trace_t ), "trace fits" );
	std::memset( out, 0, sizeof( *out ) );
	std::memcpy( out, &trace, sizeof( trace ) );
}

/* Slots 26 and 27. The last argument asks for a capsule rather than a box:
 * retail's CM_BoxTrace (0x00437130) and its transformed form (0x004375b0)
 * both take it, as Quake III 1.32's do, and the game's trace passes the same
 * flag on the server, so prediction and the game agree only when both honour
 * it. */
void QDECL CL_AliceBoxTrace( fntech3_alice_trace_t *results, const float *start, const float *end,
	const float *mins, const float *maxs, int headnode, int brushmask, int cylinder ) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	trace_t trace;
	if ( !results || !start || !end ) {
		return;
	}
	CM_BoxTrace( &trace, start, end, mins ? mins : zero, maxs ? maxs : zero, headnode, brushmask,
		cylinder ? qtrue : qfalse );
	CL_AliceCopyTrace( results, trace );
}

void QDECL CL_AliceTransformedBoxTrace( fntech3_alice_trace_t *results, const float *start,
	const float *end, const float *mins, const float *maxs, int headnode, int brushmask,
	const float *origin, const float *angles, int cylinder ) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	trace_t trace;
	if ( !results || !start || !end || !origin || !angles ) {
		return;
	}
	CM_TransformedBoxTrace( &trace, start, end, mins ? mins : zero, maxs ? maxs : zero,
		headnode, brushmask, origin, angles, cylinder ? qtrue : qfalse );
	CL_AliceCopyTrace( results, trace );
}

/* Slot 28, retail's own CM_TempBoxModel (alice.exe 0x00430f50), which writes
 * the contents it is given into the box brush. The server's clips build the
 * same shared box with each entity's contents, so the box keeps none of its
 * own between calls. */
clipHandle_t QDECL CL_AliceTempBoxModel( const float *mins, const float *maxs, int contents ) {
	if ( !mins || !maxs ) {
		return 0;
	}
	const clipHandle_t box = CM_TempBoxModel( mins, maxs, qfalse );
	CM_SetTempBoxModelContents( contents );
	return box;
}

/* Slot 29. Alice's fragment carries the plane its points lie in, which the
 * renderer's own marking routine computes; this engine's carries only the
 * points, so the plane is taken from the first three of them. */
int QDECL CL_AliceMarkFragments( int numPoints, const vec3_t *points, const float *projection,
	int maxPoints, float *pointBuffer, int maxFragments, fntech3_alice_markFragment_t *fragments ) {
	if ( !points || !projection || !pointBuffer || !fragments || maxFragments <= 0 ) {
		return 0;
	}
	std::vector<markFragment_t> own( static_cast<std::size_t>( maxFragments ) );
	const int count = re.MarkFragments( numPoints, points, projection, maxPoints,
		pointBuffer, maxFragments, own.data() );
	for ( int i = 0; i < count; i++ ) {
		fntech3_alice_markFragment_t &f = fragments[i];
		f.firstPoint = own[i].firstPoint;
		f.numPoints = own[i].numPoints;
		VectorClear( f.normal );
		f.dist = 0.0f;
		if ( f.numPoints >= 3 ) {
			const float *p0 = pointBuffer + 3 * f.firstPoint;
			vec3_t a, b;
			VectorSubtract( p0 + 3, p0, a );
			VectorSubtract( p0 + 6, p0, b );
			CrossProduct( b, a, f.normal );
			VectorNormalize( f.normal );
			f.dist = DotProduct( p0, f.normal );
		}
	}
	return count;
}

/* ---- the client's own state ------------------------------------------- */

/* Slot 75. The snapshot in Alice's own layout: its player and entity states
 * converted back from this engine's, and the server's sounds for the frame.
 * Retail's accessor at 0x00423c40 also orders the entities so each attached
 * child follows its parent, and drops a child whose parent is not there. */
qboolean QDECL CL_AliceGetSnapshot( int snapshotNumber, fntech3_alice_snapshot_t *snapshot ) {
	if ( !snapshot ) {
		return qfalse;
	}
	const clSnapshot_t *source = CL_FindSnapshot( snapshotNumber );
	if ( !source ) {
		return qfalse;
	}
	snapshot->snapFlags = source->snapFlags;
	snapshot->ping = source->ping;
	snapshot->serverTime = source->serverTime;
	std::memcpy( snapshot->areamask, source->areamask, sizeof( snapshot->areamask ) );
	fntech3::alice::state::FromInternal( source->ps, snapshot->ps );

	int count = source->numEntities;
	if ( count > FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count,
			FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES );
		count = FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES;
	}
	static fntech3_alice_entityState_t unordered[FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES];
	for ( int i = 0; i < count; i++ ) {
		const entityState_t &state = cl.parseEntities[
			( static_cast<std::uint32_t>( source->parseEntitiesNum ) + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];
		fntech3::alice::state::ForClientGame( state, unordered[i] );
	}

	/* Parents first: one pass in snapshot order, then up to eight more for
	 * the children still waiting (retail's loop at 0x00423c40). */
	static bool copied[MAX_GENTITIES];
	static bool placed[FNTECH3_ALICE_MAX_SNAPSHOT_ENTITIES];
	std::memset( copied, 0, sizeof( copied ) );
	std::memset( placed, 0, sizeof( placed ) );
	int out = 0;
	for ( int pass = 0; pass < 9; pass++ ) {
		bool progress = false;
		for ( int i = 0; i < count; i++ ) {
			if ( placed[i] ) continue;
			const int parent = unordered[i].parent;
			if ( parent == ENTITYNUM_NONE || ( parent >= 0 && parent < MAX_GENTITIES && copied[parent] ) ) {
				snapshot->entities[out++] = unordered[i];
				placed[i] = true;
				if ( unordered[i].number >= 0 && unordered[i].number < MAX_GENTITIES ) {
					copied[unordered[i].number] = true;
				}
				progress = true;
			}
		}
		if ( !progress ) break;
	}
	if ( out != count ) {
		Com_DPrintf( "CL_GetSnapshot: Not all children could find their parents.\n" );
	}
	snapshot->numEntities = out;
	snapshot->numServerCommands = 0;
	snapshot->serverCommandSequence = source->serverCommandNum;

	snapshot->number_of_sounds = source->aliceNumSounds;
	for ( int i = 0; i < source->aliceNumSounds; i++ ) {
		const aliceServerSound_t &from = source->aliceSounds[i];
		fntech3_alice_server_sound_t &to = snapshot->sounds[i];
		std::memset( &to, 0, sizeof( to ) );
		VectorCopy( from.origin, to.origin );
		to.entity_number = from.entityNumber;
		to.channel = from.channel;
		to.sound_index = static_cast<int16_t>( from.soundIndex );
		to.volume = from.volume;
		to.min_dist = from.minDist;
		to.stop_flag = from.stop ? 1 : 0;
	}
	return qtrue;
}

/* Slot 78 (0x00423bc0): a number the client has not parsed yet drops the
 * session, one that has left retail's ring of 2,048 is refused, and anything
 * else is the whole record, as the reader left it. */
qboolean QDECL CL_AliceGetParseEntityState( int parseEntityNumber, fntech3_alice_entityState_t *state ) {
	constexpr int retailParseEntities = 0x800;
	static_assert( MAX_PARSE_ENTITIES >= retailParseEntities, "the client keeps at least retail's ring" );
	if ( parseEntityNumber >= cl.parseEntitiesNum ) {
		Com_Error( ERR_DROP, "CL_GetParseEntityState: %i >= %i", parseEntityNumber, cl.parseEntitiesNum );
	}
	if ( !state || parseEntityNumber <= cl.parseEntitiesNum - retailParseEntities ) {
		return qfalse;
	}
	const entityState_t &source = cl.parseEntities[parseEntityNumber & ( MAX_PARSE_ENTITIES - 1 )];
	fntech3::alice::state::ForClientGame( source, *state );
	return qtrue;
}

/* ---- model definitions -------------------------------------------------- */

/* Slots 87 to 123: the definition queries, answered by the engine's one TIKI
 * cache, which the game module's slots reach too. */
int QDECL CL_AliceNumAnims( int tiki ) { return AliceTiki_NumAnims( tiki ); }
int QDECL CL_AliceNumSurfaces( int tiki ) { return AliceTiki_NumSurfaces( tiki ); }
int QDECL CL_AliceNumTags( int tiki ) { return AliceTiki_NumTags( tiki ); }
qboolean QDECL CL_AliceInitCommands( int tiki, fntech3_alice_tiki_cmd_t *commands ) {
	return AliceTiki_InitCommands( tiki, qtrue, commands );
}
void QDECL CL_AliceCalculateBounds( int tiki, float scale, float *mins, float *maxs ) {
	if ( mins && maxs ) AliceTiki_CalculateBounds( tiki, scale, mins, maxs );
}
void QDECL CL_AliceFlushAll() {}
const char *QDECL CL_AliceTikiNameForNum( int tiki ) { return AliceTiki_Name( tiki ); }
const char *QDECL CL_AliceAnimNameForNum( int tiki, int anim ) { return AliceTiki_AnimName( tiki, anim ); }
int QDECL CL_AliceAnimNumForName( int tiki, const char *name ) { return AliceTiki_AnimNumForName( tiki, name ); }
int QDECL CL_AliceAnimRandom( int tiki, const char *name ) { return AliceTiki_AnimRandom( tiki, name ); }
int QDECL CL_AliceAnimNumFrames( int tiki, int anim ) { return AliceTiki_AnimNumFrames( tiki, anim ); }
float QDECL CL_AliceAnimTime( int tiki, int anim ) { return AliceTiki_AnimTime( tiki, anim ); }
void QDECL CL_AliceAnimDelta( int tiki, int anim, float *delta ) {
	if ( delta ) AliceTiki_AnimDelta( tiki, anim, delta );
}
int QDECL CL_AliceAnimFlags( int tiki, int anim ) { return AliceTiki_AnimFlags( tiki, anim ); }
int QDECL CL_AliceAnimCrossblendTime( int tiki, int anim ) { return AliceTiki_AnimCrossblendTime( tiki, anim ); }
qboolean QDECL CL_AliceAnimHasCommands( int tiki, int anim ) {
	return AliceTiki_AnimHasCommands( tiki, anim, qtrue );
}
qboolean QDECL CL_AliceFrameCommands( int tiki, int anim, int frame, fntech3_alice_tiki_cmd_t *commands ) {
	return AliceTiki_FrameCommands( tiki, anim, frame, qtrue, commands );
}
void QDECL CL_AliceFrameDelta( int tiki, int anim, int frame, float *delta ) {
	if ( delta ) AliceTiki_FrameDelta( tiki, anim, frame, delta );
}
float QDECL CL_AliceFrameTime( int tiki, int anim, int frame ) { return AliceTiki_FrameTime( tiki, anim, frame ); }
void QDECL CL_AliceFrameBounds( int tiki, int anim, int frame, float scale, float *mins, float *maxs ) {
	if ( mins && maxs ) AliceTiki_FrameBounds( tiki, anim, frame, scale, mins, maxs );
}
float QDECL CL_AliceFrameRadius( int tiki, int anim, int frame ) { return AliceTiki_FrameRadius( tiki, anim, frame ); }
int QDECL CL_AliceSurfaceNameToNum( int tiki, const char *name ) { return AliceTiki_SurfaceNameToNum( tiki, name ); }
const char *QDECL CL_AliceSurfaceNumToName( int tiki, int surface ) { return AliceTiki_SurfaceNumToName( tiki, surface ); }
/* Retail's slot 111 answers with the first surface's flags whichever surface
 * it is asked about, a defect kept so the module sees what it saw. */
int QDECL CL_AliceSurfaceFlags( int tiki, int surface ) {
	return AliceTiki_NumSurfaces( tiki ) > surface && surface >= 0 ? AliceTiki_SurfaceFlags( tiki, 0 ) : 0;
}
int QDECL CL_AliceSurfaceNumSkins( int tiki, int surface ) { return AliceTiki_SurfaceNumSkins( tiki, surface ); }
int QDECL CL_AliceTagNumForName( int tiki, const char *name ) { return AliceTiki_TagNumForName( tiki, name ); }
const char *QDECL CL_AliceTagNameForNum( int tiki, int tag ) { return AliceTiki_TagNameForNum( tiki, tag ); }

struct AliceCGameOrientation {
	float origin[3];
	float axis[3][3];
};

AliceCGameOrientation *QDECL CL_AliceTagOrientation( AliceCGameOrientation *out, int tiki, int anim,
	int frame, int tag, float scale, const fntech3_alice_bonectrl_t *controllers ) {
	AliceTiki_TagOrientation( tiki, anim, frame, tag, scale, controllers, out->origin, out->axis );
	return out;
}

/* Slot 117 (0x0045fbc0): the legs alone, with every crossblend and torso
 * argument zero and both lerps 0.0 (0x0045fbd1..0x0045fbe7). */
AliceCGameOrientation *QDECL CL_AliceTagNamedOrientation( AliceCGameOrientation *out, int tiki, int anim,
	int frame, float scale, const char *tagName, const fntech3_alice_bonectrl_t *controllers ) {
	const int tag = AliceTiki_TagNumForName( tiki, tagName );
	AliceTiki_TagOrientationEx( tiki, anim, frame, tag, scale, controllers, 0, 0, 0.0f,
		qtrue, qfalse, 0, 0, 0, 0, 0.0f, out->origin, out->axis );
	return out;
}

AliceCGameOrientation *QDECL CL_AliceTagLerpedOrientation( AliceCGameOrientation *out, int tiki,
	const fntech3_alice_refEntity_t *entity, int tag ) {
	AliceTiki_TagLerpedOrientation( tiki, entity, tag, out->origin, out->axis );
	return out;
}

/* A model's own list: retail's slots 119 to 123 resolve the handle through
 * TIKI_GetTiki (0x0045c910), which prints and does nothing for a handle that
 * names no model -- where this runtime's -1 would mean the global list. */
bool CL_AliceTikiHandleValid( int tiki ) {
	if ( tiki < 0 ) {
		Com_Printf( "TIKI_GetTiki: handle %d out of range\n", tiki );
		return false;
	}
	return true;
}

qboolean QDECL CL_AliceTikiAliasAdd( int tiki, const char *alias, const char *name, const char *parameters ) {
	return CL_AliceTikiHandleValid( tiki ) ? AliceAlias_Add( tiki, alias, name, parameters ) : qfalse;
}
const char *QDECL CL_AliceTikiAliasFindRandom( int tiki, const char *alias ) {
	return CL_AliceTikiHandleValid( tiki ) ? AliceAlias_FindRandom( tiki, alias ) : nullptr;
}
void QDECL CL_AliceTikiAliasDump( int tiki ) {
	if ( CL_AliceTikiHandleValid( tiki ) ) AliceAlias_Dump( tiki );
}
void QDECL CL_AliceTikiAliasClear( int tiki ) {
	if ( CL_AliceTikiHandleValid( tiki ) ) AliceAlias_Clear( tiki );
}
const char *QDECL CL_AliceTikiAliasFindDialog( int tiki, const char *alias, int random, int entityNumber ) {
	return CL_AliceTikiHandleValid( tiki ) ? AliceAlias_FindDialog( tiki, alias, random, entityNumber ) : nullptr;
}

/* Slots 82 to 85, the engine's global alias list the game's 89 to 92 share. */
qboolean QDECL CL_AliceGlobalAliasAdd( const char *alias, const char *name, const char *parameters ) {
	return AliceAlias_Add( -1, alias, name, parameters );
}
const char *QDECL CL_AliceGlobalAliasFindRandom( const char *alias ) { return AliceAlias_FindRandom( -1, alias ); }
void QDECL CL_AliceGlobalAliasDump() { AliceAlias_Dump( -1 ); }
void QDECL CL_AliceGlobalAliasClear() { AliceAlias_Clear( -1 ); }

/* Slots 30 to 41, the sound system (retail's wrappers 0x0044cdb0 to
 * 0x0044cfa0 in front of its Miles backend). The audio module holds Alice's
 * own voices, loop groups, distances and fade; these only change the calling
 * convention. See docs/fntech3/2026-09-23-alice-audio.md. */
void QDECL CL_AliceStartSound( const float *origin, int entnum, int entchannel, sfxHandle_t sfx,
	float volume, float minDist ) {
	S_AliceStartSound( origin, entnum, entchannel, sfx, volume, minDist );
}
void QDECL CL_AliceStartLocalSound( const char *name ) { S_AliceStartLocalSound( name ); }
void QDECL CL_AliceStopSound( int entnum, int channel ) { S_AliceStopSound( entnum, channel ); }
void QDECL CL_AliceClearLoopingSounds() { S_AliceClearLoopingSounds(); }
void QDECL CL_AliceAddLoopingSound( const float *origin, const float *velocity, sfxHandle_t sfx,
	float volume, float minDist ) {
	S_AliceAddLoopingSound( origin, velocity, sfx, volume, minDist );
}
void QDECL CL_AliceRespatialize( int entnum, const float *origin, vec3_t axis[3] ) {
	S_AliceRespatialize( entnum, origin, axis );
}
void QDECL CL_AliceBeginSoundRegistration() { S_AliceBeginRegistration(); }
sfxHandle_t QDECL CL_AliceRegisterSound( const char *name ) { return S_AliceRegisterSound( name ); }
void QDECL CL_AliceEndSoundRegistration() { S_AliceEndRegistration(); }
void QDECL CL_AliceUpdateEntity( int entnum, const float *origin, const float *velocity,
	qboolean useListener ) {
	S_AliceUpdateEntity( entnum, origin, velocity, useListener );
	/* What the player carries comes with no position of its own, to be heard
	 * at the listener (F.A.K.K.2 SDK CG_SetEntitySoundPosition); the rest are
	 * where they stand. */
	if ( origin && !useListener ) {
		CL_LiquidFeedEntity( entnum, origin );
	}
}
void QDECL CL_AliceSetReverb( int type, float level ) { S_AliceSetReverb( type, level ); }
void QDECL CL_AliceSetGlobalAmbientVolumeLevel( float volume ) {
	S_AliceSetGlobalAmbientVolumeLevel( volume );
}

/* Slots 42 to 44, the soundtrack manager: a soundtrack's name from the
 * level's configstring, and the moods and music volume every snapshot's
 * player state carries. */
void QDECL CL_AliceMusicNewSoundtrack( const char *name ) { S_AliceMusicNewSoundtrack( name ); }
void QDECL CL_AliceMusicUpdateMood( int current, int fallback ) { S_AliceMusicUpdateMood( current, fallback ); }
void QDECL CL_AliceMusicUpdateVolume( float volume, float fadeTime ) { S_AliceMusicUpdateVolume( volume, fadeTime ); }

/* Slot 47, the camera look: whether the look button is held, whether a
 * centre-view asked for a reset (cleared by asking), and the offset the mouse
 * has accumulated since the process began (0x0042a1f0). */
float *QDECL CL_AliceCameraOffset( qboolean *lookActive, qboolean *resetView ) {
	return CL_AliceGetCameraOffset( lookActive, resetView );
}

/* Slots 45 and 46, the lip-sync pair the game's 97 and 98 share. */
float QDECL CL_AliceLipLength( const char *name ) { return Alice_LipLength( name ); }
const byte *QDECL CL_AliceLipAmplitudes( const char *name, int *count ) {
	return Alice_LipAmplitudes( name, count );
}

/* Slot 124, the one the module calls before anything else: the retail engine
 * fills it with a routine whose whole body returns the address of a fixed
 * 60-byte block (0x0042c490), the same block the game table's slot 107 and the
 * renderer are handed, so the camera the client game writes there reaches the
 * game and the HUD state the game writes reaches the client. */
void *QDECL CL_AliceSharedBlock() {
	return Alice_SharedBlock();
}

/* Slot 54, a Quake III skin file, which retail's renderer loads with Quake
 * III's own routine. */
qhandle_t QDECL CL_AliceRegisterSkin( const char *name ) {
	return name && name[0] ? re.RegisterSkin( name ) : 0;
}

/* Slot 66, the renderer's noise, used to shake beams. Its table is retail's own,
 * built from a fixed seed, so the same beam shakes the same way every run. */
float QDECL CL_AliceNoise( float x, float y, float z, float t ) {
	return Alice_Noise( x, y, z, t );
}

/* Slot 67, appended to the array the game module publishes; drawing it is a
 * renderer feature this engine does not yet have. */
void QDECL CL_AliceDebugLine( const float *start, const float *end, float r, float g, float b,
	float alpha ) {
	Alice_AddDebugLine( start, end, r, g, b, alpha );
}

/* Slots 68 to 70, a swipe: the trail a swung weapon leaves. The renderer keeps
 * the points and draws the strip; the end call does nothing in retail
 * (0x00497f60). */
void QDECL CL_AliceSwipeBegin( float thistime, float life, qhandle_t shader ) {
	if ( re.AliceSwipeBegin ) re.AliceSwipeBegin( thistime, life, shader );
}

void QDECL CL_AliceSwipePoint( const float *p1, const float *p2, float time ) {
	if ( p1 && p2 && re.AliceSwipePoint ) re.AliceSwipePoint( p1, p2, time );
}

void QDECL CL_AliceSwipeEnd() {
}

/*
==================
CL_AliceFillCGameImports

Fill every slot, then replace the ones this engine serves.
==================
*/
void CL_AliceFillCGameImports() {
	CL_AliceFillUnservedCGame(
		std::make_index_sequence<FNTECH3_ALICE_CGAME_IMPORT_SLOTS>{} );

/* A function whose shape is not the slot's frozen one
 * (alice_cgame_import_sigs.def) does not compile. */
#define SERVE_CGAME( index, fn ) \
	static_assert( fntech3::alice::module::Fits< \
		fntech3::alice::module::CGameImportSig<index>, decltype( &fn )>(), \
		#fn " does not have the frozen shape of client-game import " #index ); \
	aliceCGameImport.slot[index] = reinterpret_cast<std::intptr_t>( &fn )

	SERVE_CGAME( FNTECH3_ALICE_CG_Printf, CL_AlicePrintf );
	SERVE_CGAME( FNTECH3_ALICE_CG_DPrintf, CL_AliceDPrintf );
	SERVE_CGAME( FNTECH3_ALICE_CG_DebugPrintf, CL_AliceDPrintf );
	SERVE_CGAME( FNTECH3_ALICE_CG_Malloc, CL_AliceMalloc );
	SERVE_CGAME( FNTECH3_ALICE_CG_Free, CL_AliceFree );
	SERVE_CGAME( FNTECH3_ALICE_CG_Error, CL_AliceError );
	SERVE_CGAME( FNTECH3_ALICE_CG_Milliseconds, CL_AliceMilliseconds );
	SERVE_CGAME( FNTECH3_ALICE_CG_Cvar_Get, CL_AliceCvar );
	SERVE_CGAME( FNTECH3_ALICE_CG_Cvar_Set, CL_AliceCvarSet );
	SERVE_CGAME( FNTECH3_ALICE_CG_Argc, CL_AliceArgc );
	SERVE_CGAME( FNTECH3_ALICE_CG_Argv, CL_AliceArgv );
	SERVE_CGAME( FNTECH3_ALICE_CG_Args, CL_AliceArgs );
	SERVE_CGAME( FNTECH3_ALICE_CG_AddCommand, CL_AliceAddCommand );
	SERVE_CGAME( FNTECH3_ALICE_CG_FS_ReadFile, CL_AliceReadFile );
	SERVE_CGAME( FNTECH3_ALICE_CG_FS_FreeFile, CL_AliceFreeFile );
	SERVE_CGAME( FNTECH3_ALICE_CG_FS_WriteFile, CL_AliceWriteFile );
	SERVE_CGAME( FNTECH3_ALICE_CG_FS_WriteTextFile, CL_AliceWriteTextFile );
	SERVE_CGAME( FNTECH3_ALICE_CG_SendConsoleCommand, CL_AliceSendConsoleCommand );
	SERVE_CGAME( FNTECH3_ALICE_CG_UpdateLoadingScreen, CL_AliceUpdateLoadingScreen );
	SERVE_CGAME( FNTECH3_ALICE_CG_SendClientCommand, CL_AliceSendClientCommand );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_LoadMap, CL_AliceLoadMap );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_InlineModel, CL_AliceInlineModel );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_NumInlineModels, CL_AliceNumInlineModels );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_PointContents, CL_AlicePointContents );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_TransformedPointContents, CL_AliceTransformedPointContents );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_ClearScene, CL_AliceClearScene );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_LoadWorldMap, CL_AliceLoadWorldMap );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_RegisterModel, CL_AliceRegisterModel );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_RegisterShader, CL_AliceRegisterShader );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_ModelBounds, CL_AliceModelBounds );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_ModelRadius, CL_AliceModelRadius );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_RegisterShaderNoMip, CL_AliceRegisterShaderNoMip );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_SetColor, CL_AliceSetColor );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_DrawStretchPic, CL_AliceDrawStretchPic );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetGameState, CL_AliceGetGameState );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetCurrentSnapshotNumber, CL_AliceGetCurrentSnapshotNumber );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetGlconfig, CL_AliceGetGlconfig );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetCurrentCmdNumber, CL_AliceGetCurrentCmdNumber );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetUserCmd, CL_AliceGetUserCmd );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetServerCommand, CL_AliceGetServerCommand );
	SERVE_CGAME( FNTECH3_ALICE_CG_SharedBlock, CL_AliceSharedBlock );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_RegisterSkin, CL_AliceRegisterSkin );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_Noise, CL_AliceNoise );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_DebugLine, CL_AliceDebugLine );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_SwipeBegin, CL_AliceSwipeBegin );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_SwipePoint, CL_AliceSwipePoint );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_SwipeEnd, CL_AliceSwipeEnd );

	SERVE_CGAME( FNTECH3_ALICE_CG_CM_BoxTrace, CL_AliceBoxTrace );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_TransformedBoxTrace, CL_AliceTransformedBoxTrace );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_TempBoxModel, CL_AliceTempBoxModel );
	SERVE_CGAME( FNTECH3_ALICE_CG_CM_MarkFragments, CL_AliceMarkFragments );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_StartSound, CL_AliceStartSound );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_StartLocalSound, CL_AliceStartLocalSound );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_StopSound, CL_AliceStopSound );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_ClearLoopingSounds, CL_AliceClearLoopingSounds );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_AddLoopingSound, CL_AliceAddLoopingSound );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_Respatialize, CL_AliceRespatialize );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_BeginRegistration, CL_AliceBeginSoundRegistration );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_RegisterSound, CL_AliceRegisterSound );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_EndRegistration, CL_AliceEndSoundRegistration );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_UpdateEntity, CL_AliceUpdateEntity );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_SetReverb, CL_AliceSetReverb );
	SERVE_CGAME( FNTECH3_ALICE_CG_S_SetGlobalAmbientVolumeLevel, CL_AliceSetGlobalAmbientVolumeLevel );
	SERVE_CGAME( FNTECH3_ALICE_CG_MUSIC_NewSoundtrack, CL_AliceMusicNewSoundtrack );
	SERVE_CGAME( FNTECH3_ALICE_CG_MUSIC_UpdateMood, CL_AliceMusicUpdateMood );
	SERVE_CGAME( FNTECH3_ALICE_CG_MUSIC_UpdateVolume, CL_AliceMusicUpdateVolume );
	SERVE_CGAME( FNTECH3_ALICE_CG_get_camera_offset, CL_AliceCameraOffset );
	SERVE_CGAME( FNTECH3_ALICE_CG_get_lip_length, CL_AliceLipLength );
	SERVE_CGAME( FNTECH3_ALICE_CG_get_lip_amplitudes, CL_AliceLipAmplitudes );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_RenderScene, CL_AliceRenderScene );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_AddRefEntityToScene, CL_AliceAddRefEntityToScene );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_AddRefSpriteToScene, CL_AliceAddRefSpriteToScene );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_AddLightToScene, CL_AliceAddLightToScene );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_AddPolyToScene, CL_AliceAddPolyToScene );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_GetRenderEntity, CL_AliceGetRenderEntity );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_GetShaderWidth, CL_AliceGetShaderWidth );
	SERVE_CGAME( FNTECH3_ALICE_CG_R_GetShaderHeight, CL_AliceGetShaderHeight );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetSnapshot, CL_AliceGetSnapshot );
	SERVE_CGAME( FNTECH3_ALICE_CG_GetParseEntityState, CL_AliceGetParseEntityState );
	SERVE_CGAME( FNTECH3_ALICE_CG_Alias_Add, CL_AliceGlobalAliasAdd );
	SERVE_CGAME( FNTECH3_ALICE_CG_Alias_FindRandom, CL_AliceGlobalAliasFindRandom );
	SERVE_CGAME( FNTECH3_ALICE_CG_Alias_Dump, CL_AliceGlobalAliasDump );
	SERVE_CGAME( FNTECH3_ALICE_CG_Alias_Clear, CL_AliceGlobalAliasClear );
	SERVE_CGAME( FNTECH3_ALICE_CG_TIKI_GetHandle, CL_AliceTikiGetHandle );
	SERVE_CGAME( FNTECH3_ALICE_CG_NumAnims, CL_AliceNumAnims );
	SERVE_CGAME( FNTECH3_ALICE_CG_NumSurfaces, CL_AliceNumSurfaces );
	SERVE_CGAME( FNTECH3_ALICE_CG_NumTags, CL_AliceNumTags );
	SERVE_CGAME( FNTECH3_ALICE_CG_InitCommands, CL_AliceInitCommands );
	SERVE_CGAME( FNTECH3_ALICE_CG_CalculateBounds, CL_AliceCalculateBounds );
	SERVE_CGAME( FNTECH3_ALICE_CG_FlushAll, CL_AliceFlushAll );
	SERVE_CGAME( FNTECH3_ALICE_CG_TIKI_NameForNum, CL_AliceTikiNameForNum );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_NameForNum, CL_AliceAnimNameForNum );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_NumForName, CL_AliceAnimNumForName );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_Random, CL_AliceAnimRandom );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_NumFrames, CL_AliceAnimNumFrames );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_Time, CL_AliceAnimTime );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_Delta, CL_AliceAnimDelta );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_Flags, CL_AliceAnimFlags );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_CrossblendTime, CL_AliceAnimCrossblendTime );
	SERVE_CGAME( FNTECH3_ALICE_CG_Anim_HasCommands, CL_AliceAnimHasCommands );
	SERVE_CGAME( FNTECH3_ALICE_CG_Frame_Commands, CL_AliceFrameCommands );
	SERVE_CGAME( FNTECH3_ALICE_CG_Frame_Delta, CL_AliceFrameDelta );
	SERVE_CGAME( FNTECH3_ALICE_CG_Frame_Time, CL_AliceFrameTime );
	SERVE_CGAME( FNTECH3_ALICE_CG_Frame_Bounds, CL_AliceFrameBounds );
	SERVE_CGAME( FNTECH3_ALICE_CG_Frame_Radius, CL_AliceFrameRadius );
	SERVE_CGAME( FNTECH3_ALICE_CG_Surface_NameToNum, CL_AliceSurfaceNameToNum );
	SERVE_CGAME( FNTECH3_ALICE_CG_Surface_NumToName, CL_AliceSurfaceNumToName );
	SERVE_CGAME( FNTECH3_ALICE_CG_Surface_Flags, CL_AliceSurfaceFlags );
	SERVE_CGAME( FNTECH3_ALICE_CG_Surface_NumSkins, CL_AliceSurfaceNumSkins );
	SERVE_CGAME( FNTECH3_ALICE_CG_Tag_NumForName, CL_AliceTagNumForName );
	SERVE_CGAME( FNTECH3_ALICE_CG_Tag_NameForNum, CL_AliceTagNameForNum );
	SERVE_CGAME( FNTECH3_ALICE_CG_Tag_Orientation, CL_AliceTagOrientation );
	SERVE_CGAME( FNTECH3_ALICE_CG_Tag_NamedOrientation, CL_AliceTagNamedOrientation );
	SERVE_CGAME( FNTECH3_ALICE_CG_Tag_LerpedOrientation, CL_AliceTagLerpedOrientation );
	SERVE_CGAME( FNTECH3_ALICE_CG_TIKI_Alias_Add, CL_AliceTikiAliasAdd );
	SERVE_CGAME( FNTECH3_ALICE_CG_TIKI_Alias_FindRandom, CL_AliceTikiAliasFindRandom );
	SERVE_CGAME( FNTECH3_ALICE_CG_TIKI_Alias_Dump, CL_AliceTikiAliasDump );
	SERVE_CGAME( FNTECH3_ALICE_CG_TIKI_Alias_Clear, CL_AliceTikiAliasClear );
	SERVE_CGAME( FNTECH3_ALICE_CG_TIKI_Alias_FindDialog, CL_AliceTikiAliasFindDialog );

#undef SERVE_CGAME
}

/* ---- Heavy Metal: F.A.K.K.2's client game -------------------------------- */

/* F.A.K.K.2's cgamex86.dll is the parent of Alice's and is served from the
 * same functions: its 124 slots are the first 124 of Alice's table, member for
 * member (code/compat/fakk2_module_abi.h). Where a slot hands over a record
 * whose shape the two titles do not share -- a render entity, the view, a
 * snapshot's entities, the game state, a command list, a mark fragment, the
 * bone controllers -- the F.A.K.K.2 record crosses into Alice's first
 * (code/compat/fakk2_convert.hpp). */
std::intptr_t fakk2CGameImport[FNTECH3_FAKK2_CGAME_IMPORT_SLOTS];

bool CL_RitualIsFAKK2() {
	return Com_GameIsFAKK2() != 0;
}

/* The render entities as F.A.K.K.2 submitted them, beside Alice's copies at
 * the same indices, so slot 63 can hand one back in the module's own layout. */
std::array<fntech3_fakk2_refEntity_t, FNTECH3_ALICE_MAX_RENDER_ENTITIES> fakk2SceneEntities;

void QDECL CL_FAKK2AddRefEntityToScene( const fntech3_fakk2_refEntity_t *e ) {
	if ( !e ) {
		return;
	}
	fntech3_alice_refEntity_t alice;
	fntech3::fakk2::convert::RefEntityToAlice( *e, alice );
	const int index = aliceNumSceneEntities;
	CL_AliceAddRefEntityToScene( &alice );
	if ( aliceNumSceneEntities > index ) {
		fakk2SceneEntities[index] = *e;
	}
}

void QDECL CL_FAKK2AddRefSpriteToScene( const fntech3_fakk2_refEntity_t *e ) {
	if ( !e ) {
		return;
	}
	fntech3_alice_refEntity_t alice;
	fntech3::fakk2::convert::RefEntityToAlice( *e, alice );
	CL_AliceAddRefSpriteToScene( &alice );
}

fntech3_fakk2_refEntity_t *QDECL CL_FAKK2GetRenderEntity( int entityNumber ) {
	for ( int i = 0; i < aliceNumSceneEntities; i++ ) {
		if ( aliceSceneEntities[i].entityNumber == entityNumber ) {
			return &fakk2SceneEntities[i];
		}
	}
	return nullptr;
}

void QDECL CL_FAKK2RenderScene( const fntech3_fakk2_refdef_t *fd ) {
	if ( !fd ) {
		return;
	}
	fntech3_alice_refdef_t alice;
	fntech3::fakk2::convert::RefdefToAlice( *fd, alice );
	CL_AliceRenderScene( &alice );
}

/* Slot 73, which F.A.K.K.2's client game calls at four arguments -- both of
 * its sites, 0x3001f65e and 0x3001f793, clean up four -- and Alice's never
 * does: a box of the colour R_SetColor set, filled with the renderer's white
 * image, which is what the SDK's declaration of it as x, y, width and height
 * describes. */
void QDECL CL_FAKK2DrawBox( float x, float y, float w, float h ) {
	re.DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, cls.whiteShader );
}

/* Slot 74: F.A.K.K.2's 1,200 configstrings over the same string data. */
void QDECL CL_FAKK2GetGameState( fntech3_fakk2_gamestate_t *state ) {
	if ( !state ) {
		Com_Error( ERR_DROP, "F.A.K.K.2's client game asked for the game state "
			"with nowhere to put it" );
		return;
	}
	static_assert( sizeof( state->stringData ) <= sizeof( cl.gameState.stringData ),
		"F.A.K.K.2's string data fits in this engine's" );
	if ( cl.gameState.dataCount < 0 ||
		cl.gameState.dataCount > static_cast<int>( sizeof( state->stringData ) ) ) {
		Com_Error( ERR_DROP, "F.A.K.K.2's game state holds %i bytes of strings, "
			"past the %i this title has", cl.gameState.dataCount,
			static_cast<int>( sizeof( state->stringData ) ) );
		return;
	}
	std::memset( state, 0, sizeof( *state ) );
	for ( int i = 0; i < FNTECH3_FAKK2_MAX_CONFIGSTRINGS; i++ ) {
		state->stringOffsets[i] = cl.gameState.stringOffsets[i];
	}
	for ( int i = FNTECH3_FAKK2_MAX_CONFIGSTRINGS; i < MAX_GAME_CONFIGSTRINGS; i++ ) {
		if ( cl.gameState.stringOffsets[i] ) {
			Com_Error( ERR_DROP, "F.A.K.K.2's game state holds configstring %i, "
				"past the %i this title has", i, FNTECH3_FAKK2_MAX_CONFIGSTRINGS );
			return;
		}
	}
	std::memcpy( state->stringData, cl.gameState.stringData, sizeof( state->stringData ) );
	state->dataCount = cl.gameState.dataCount;
}

/* Slot 75: Alice's snapshot, built as her accessor builds it -- parents before
 * their attached children -- and then carried into F.A.K.K.2's record. */
fntech3_alice_snapshot_t fakk2SnapshotScratch;

qboolean QDECL CL_FAKK2GetSnapshot( int snapshotNumber, fntech3_fakk2_snapshot_t *snapshot ) {
	if ( !snapshot || !CL_AliceGetSnapshot( snapshotNumber, &fakk2SnapshotScratch ) ) {
		return qfalse;
	}
	const fntech3_alice_snapshot_t &from = fakk2SnapshotScratch;
	snapshot->snapFlags = from.snapFlags;
	snapshot->ping = from.ping;
	snapshot->serverTime = from.serverTime;
	std::memcpy( snapshot->areamask, from.areamask, sizeof( snapshot->areamask ) );
	snapshot->ps = from.ps;
	snapshot->numEntities = from.numEntities;
	for ( int i = 0; i < from.numEntities; i++ ) {
		fntech3::fakk2::convert::EntityFromAlice( from.entities[i], snapshot->entities[i] );
	}
	snapshot->numServerCommands = from.numServerCommands;
	snapshot->serverCommandSequence = from.serverCommandSequence;
	snapshot->number_of_sounds = from.number_of_sounds;
	std::memcpy( snapshot->sounds, from.sounds, sizeof( snapshot->sounds ) );
	return qtrue;
}

qboolean QDECL CL_FAKK2GetParseEntityState( int parseEntityNumber, fntech3_fakk2_entityState_t *state ) {
	fntech3_alice_entityState_t alice;
	if ( !state || !CL_AliceGetParseEntityState( parseEntityNumber, &alice ) ) {
		return qfalse;
	}
	fntech3::fakk2::convert::EntityFromAlice( alice, *state );
	return qtrue;
}

/* Slots 91 and 104, into F.A.K.K.2's 128-command, ten-argument record. */
fntech3_alice_tiki_cmd_t fakk2CGameCommandScratch;

qboolean CL_FAKK2Commands( qboolean found, fntech3_fakk2_tiki_cmd_t *commands ) {
	if ( !commands ) {
		return qfalse;
	}
	if ( !found ) {
		commands->num_cmds = 0;
		return qfalse;
	}
	if ( !fntech3::fakk2::convert::CommandsFromAlice( fakk2CGameCommandScratch, *commands ) ) {
		Com_DPrintf( "a command list longer than F.A.K.K.2's 128 commands of ten arguments was cut short\n" );
	}
	return qtrue;
}

qboolean QDECL CL_FAKK2InitCommands( int tiki, fntech3_fakk2_tiki_cmd_t *commands ) {
	std::memset( &fakk2CGameCommandScratch, 0, sizeof( fakk2CGameCommandScratch ) );
	return CL_FAKK2Commands( CL_AliceInitCommands( tiki, &fakk2CGameCommandScratch ), commands );
}

qboolean QDECL CL_FAKK2FrameCommands( int tiki, int anim, int frame, fntech3_fakk2_tiki_cmd_t *commands ) {
	std::memset( &fakk2CGameCommandScratch, 0, sizeof( fakk2CGameCommandScratch ) );
	return CL_FAKK2Commands( CL_AliceFrameCommands( tiki, anim, frame, &fakk2CGameCommandScratch ),
		commands );
}

/* Slot 29, into Quake III's eight-byte fragments. */
int QDECL CL_FAKK2MarkFragments( int numPoints, const vec3_t *points, const float *projection,
	int maxPoints, float *pointBuffer, int maxFragments, fntech3_fakk2_markFragment_t *fragments ) {
	if ( !fragments || maxFragments <= 0 ) {
		return 0;
	}
	std::vector<fntech3_alice_markFragment_t> alice( static_cast<std::size_t>( maxFragments ) );
	const int count = CL_AliceMarkFragments( numPoints, points, projection, maxPoints,
		pointBuffer, maxFragments, alice.data() );
	fntech3::fakk2::convert::FragmentsFromAlice( alice.data(), count, fragments );
	return count;
}

/* Slots 115 to 118, with the bone controllers as the two arrays F.A.K.K.2
 * keeps them in, and slot 118's entity in F.A.K.K.2's layout. */
AliceCGameOrientation *QDECL CL_FAKK2TagOrientation( AliceCGameOrientation *out, int tiki, int anim,
	int frame, int tag, float scale, const int32_t *boneTag, const float ( *boneQuat )[4] ) {
	fntech3_alice_bonectrl_t controllers[FNTECH3_ALICE_BONE_CONTROLLERS];
	fntech3::fakk2::convert::ControllersToAlice( boneTag, boneQuat, controllers );
	return CL_AliceTagOrientation( out, tiki, anim, frame, tag, scale, controllers );
}

AliceCGameOrientation *QDECL CL_FAKK2TagOrientationEx( AliceCGameOrientation *out, int tiki, int anim,
	int frame, int tag, float scale, const int32_t *boneTag, const float ( *boneQuat )[4],
	int crossblendAnim, int crossblendFrame, float crossblendLerp, int useLegs, int useTorso,
	int torsoAnim, int torsoFrame, int torsoCrossblendAnim, int torsoCrossblendFrame,
	float torsoCrossblendLerp ) {
	fntech3_alice_bonectrl_t controllers[FNTECH3_ALICE_BONE_CONTROLLERS];
	fntech3::fakk2::convert::ControllersToAlice( boneTag, boneQuat, controllers );
	AliceTiki_TagOrientationEx( tiki, anim, frame, tag, scale, controllers, crossblendAnim,
		crossblendFrame, crossblendLerp, useLegs ? qtrue : qfalse, useTorso ? qtrue : qfalse,
		torsoAnim, torsoFrame, torsoCrossblendAnim, torsoCrossblendFrame, torsoCrossblendLerp,
		out->origin, out->axis );
	return out;
}

AliceCGameOrientation *QDECL CL_FAKK2TagNamedOrientation( AliceCGameOrientation *out, int tiki,
	int anim, int frame, float scale, const char *tagName, const int32_t *boneTag,
	const float ( *boneQuat )[4] ) {
	fntech3_alice_bonectrl_t controllers[FNTECH3_ALICE_BONE_CONTROLLERS];
	fntech3::fakk2::convert::ControllersToAlice( boneTag, boneQuat, controllers );
	return CL_AliceTagNamedOrientation( out, tiki, anim, frame, scale, tagName, controllers );
}

AliceCGameOrientation *QDECL CL_FAKK2TagLerpedOrientation( AliceCGameOrientation *out, int tiki,
	const fntech3_fakk2_refEntity_t *entity, int tag ) {
	fntech3_alice_refEntity_t alice;
	if ( !entity ) {
		return CL_AliceTagLerpedOrientation( out, tiki, nullptr, tag );
	}
	fntech3::fakk2::convert::RefEntityToAlice( *entity, alice );
	return CL_AliceTagLerpedOrientation( out, tiki, &alice, tag );
}

/* Build F.A.K.K.2's table from Alice's: the same services at the same
 * positions, and F.A.K.K.2's own records where the two differ. Slot 0 is the
 * import table's version, CGAME_IMPORT_API_VERSION 3 in the SDK's header;
 * the retail module never reads it (no instruction in cgamex86.dll addresses
 * its copy at 0x301a3a20), and it is filled rather than left null. */
void CL_FAKK2FillCGameImports() {
	for ( int slot = 0; slot < FNTECH3_FAKK2_CGAME_IMPORT_SLOTS; slot++ ) {
		fakk2CGameImport[slot] = aliceCGameImport.slot[fntech3::fakk2::module::AliceCgameSlot( slot )];
	}
	fakk2CGameImport[0] = 3;

#define SERVE_FAKK2( index, fn ) \
	fakk2CGameImport[index] = reinterpret_cast<std::intptr_t>( &fn )

	SERVE_FAKK2( FNTECH3_FAKK2_CG_CM_MarkFragments, CL_FAKK2MarkFragments );
	SERVE_FAKK2( FNTECH3_ALICE_CG_R_RenderScene, CL_FAKK2RenderScene );
	SERVE_FAKK2( FNTECH3_ALICE_CG_R_AddRefEntityToScene, CL_FAKK2AddRefEntityToScene );
	SERVE_FAKK2( FNTECH3_ALICE_CG_R_AddRefSpriteToScene, CL_FAKK2AddRefSpriteToScene );
	SERVE_FAKK2( FNTECH3_ALICE_CG_R_GetRenderEntity, CL_FAKK2GetRenderEntity );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_R_DrawBox, CL_FAKK2DrawBox );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_GetGameState, CL_FAKK2GetGameState );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_GetSnapshot, CL_FAKK2GetSnapshot );
	SERVE_FAKK2( FNTECH3_ALICE_CG_GetParseEntityState, CL_FAKK2GetParseEntityState );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_InitCommands, CL_FAKK2InitCommands );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_Frame_Commands, CL_FAKK2FrameCommands );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_Tag_Orientation, CL_FAKK2TagOrientation );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_Tag_OrientationEx, CL_FAKK2TagOrientationEx );
	SERVE_FAKK2( FNTECH3_FAKK2_CG_Tag_NamedOrientation, CL_FAKK2TagNamedOrientation );
	SERVE_FAKK2( FNTECH3_ALICE_CG_Tag_LerpedOrientation, CL_FAKK2TagLerpedOrientation );

#undef SERVE_FAKK2
}

int CL_FAKK2ServedCGameImports() {
	int served = 0;
	for ( int slot = 1; slot < FNTECH3_FAKK2_CGAME_IMPORT_SLOTS; slot++ ) {
		served += fakk2CGameImport[slot]
			!= aliceCGameUnserved.slot[fntech3::fakk2::module::AliceCgameSlot( slot )];
	}
	return served;
}

/* The seven exports, at the argument counts their own bodies read and the
 * retail engine's own call sites push. */
typedef void ( QDECL *AliceCGameInit_t )( CGameImportTable *imports,
	int serverMessageNum, int serverCommandSequence );
typedef void ( QDECL *AliceCGameVoid_t )();
typedef void ( QDECL *AliceCGameDrawActiveFrame_t )( int serverTime,
	int stereoView, int demoPlayback );
typedef int ( QDECL *AliceCGameConsoleCommand_t )();
/* Export 6, which retail's client collision calls (0x00431261) and this
 * engine's does not. */
typedef void ( QDECL *AliceCGameEntityOrientation_t )( int entityNum, float *origin, float *angles );

/* Every type an export is called through has its frozen row's shape
 * (alice_cgame_export_sigs.def). */
#define FNTECH3_ALICE_CGAME_EXPORT_FITS( member, type ) \
	static_assert( fntech3::alice::module::Fits<fntech3::alice::module::CGameExportSig< \
		FNTECH3_ALICE_CGE_##member>, type>(), #type " is not the frozen shape of client-game export " #member )
FNTECH3_ALICE_CGAME_EXPORT_FITS( Init, AliceCGameInit_t );
FNTECH3_ALICE_CGAME_EXPORT_FITS( Shutdown, AliceCGameVoid_t );
FNTECH3_ALICE_CGAME_EXPORT_FITS( DrawActiveFrame, AliceCGameDrawActiveFrame_t );
FNTECH3_ALICE_CGAME_EXPORT_FITS( ConsoleCommand, AliceCGameConsoleCommand_t );
FNTECH3_ALICE_CGAME_EXPORT_FITS( GetRendererConfig, AliceCGameVoid_t );
FNTECH3_ALICE_CGAME_EXPORT_FITS( Draw2D, AliceCGameVoid_t );
FNTECH3_ALICE_CGAME_EXPORT_FITS( EntityOrientation, AliceCGameEntityOrientation_t );
#undef FNTECH3_ALICE_CGAME_EXPORT_FITS

template<typename T>
T CL_AliceCGameExport( int slot ) {
	return aliceCGameExports
		? reinterpret_cast<T>( aliceCGameExports[slot] ) : nullptr;
}

/*
==================
CL_AliceCgameEntry

The engine's client-game calls, routed to Alice's exports as retail alice.exe
routes them:

  CG_INIT              `CL_InitCGame` at 0x004248d0 hands export 0 the import
                       table, clc.serverMessageSequence and the last executed
                       server command -- three arguments, which the export's
                       own body reads and stores.
  CG_SHUTDOWN          export 1, at 0x004243e3.
  CG_CONSOLE_COMMAND   export 3, tail-called at 0x0042498a; its answer is
                       whether the module claimed the command.
  CG_DRAW_ACTIVE_FRAME export 2 with the server time, the stereo field and the
                       demo flag, at 0x004249a7; then export 5, the 2D pass,
                       which retail calls from its in-game view once the 3D
                       frame is drawn (0x0041fe4e).

The Quake III calls Alice's module has no counterpart for -- crosshair and
attacker queries, key and mouse events -- answer nothing.
==================
*/
intptr_t QDECL CL_AliceCgameEntry( int command, ... ) {
	va_list ap;
	int args[3] = {};

	va_start( ap, command );
	for ( int &arg : args ) {
		arg = va_arg( ap, int );
	}
	va_end( ap );

	switch ( command ) {
	case CG_INIT: {
		auto init = CL_AliceCGameExport<AliceCGameInit_t>( FNTECH3_ALICE_CGE_Init );
		/* F.A.K.K.2's initializer copies its own 124 slots from the same
		 * position and reads the same two sequence numbers (0x30022470). */
		init( CL_RitualIsFAKK2() ? reinterpret_cast<CGameImportTable *>( fakk2CGameImport )
			: &aliceCGameImport, args[0], args[1] );
		Com_Printf( "%s client game initialized.\n", CL_RitualIsFAKK2() ? "F.A.K.K.2" : "Alice" );
		return 0;
	}
	case CG_SHUTDOWN: {
		auto shutdown = CL_AliceCGameExport<AliceCGameVoid_t>( FNTECH3_ALICE_CGE_Shutdown );
		if ( shutdown ) {
			shutdown();
		}
		aliceCGameExports = nullptr;
		return 0;
	}
	case CG_CONSOLE_COMMAND: {
		auto consoleCommand = CL_AliceCGameExport<AliceCGameConsoleCommand_t>(
			FNTECH3_ALICE_CGE_ConsoleCommand );
		return consoleCommand ? consoleCommand() : 0;
	}
	case CG_DRAW_ACTIVE_FRAME: {
		/* The 3D view alone: the module's 2D layer is drawn by the in-game
		 * view over the view, the fade, the letterbox and the frame counter,
		 * through CL_AliceCGameDraw2D. */
		auto drawActiveFrame = CL_AliceCGameExport<AliceCGameDrawActiveFrame_t>(
			FNTECH3_ALICE_CGE_DrawActiveFrame );
		drawActiveFrame( args[0], args[1], args[2] );
		return 0;
	}
	default:
		return 0;
	}
}

/*
==================
CL_AliceCGameRendererRestarted

Retail tells the client game the renderer came back after a video restart
(0x0042c68f), and the module re-reads the graphics configuration.
==================
*/
void CL_AliceCGameRendererRestarted() {
	auto getRendererConfig = CL_AliceCGameExport<AliceCGameVoid_t>(
		FNTECH3_ALICE_CGE_GetRendererConfig );
	if ( getRendererConfig ) {
		getRendererConfig();
	}
}

/*
==================
CL_AliceLoadCGame

Take the module's export table and build the imports. The imports are handed
over by CG_INIT, as retail does, because the initializer takes the two
sequence numbers alongside them.
==================
*/
vm_t *CL_AliceLoadCGame() {
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_CGAME, "GetCGameAPI", CL_AliceCgameEntry, &symbol );

	if ( !vm ) {
		return nullptr;
	}

	typedef void *( QDECL *AliceGetCGameAPI_t )();
	auto getCGameAPI = reinterpret_cast<AliceGetCGameAPI_t>( symbol );

	const std::intptr_t *exports =
		static_cast<const std::intptr_t *>( getCGameAPI() );
	const bool fakk2 = CL_RitualIsFAKK2();
	const char *title = fakk2 ? "F.A.K.K.2's" : "Alice's";
	if ( !exports ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "%s client game published no export table", title );
		return nullptr;
	}
	/* F.A.K.K.2's GetCGameAPI (0x300224e0) writes the first six of Alice's
	 * seven, in the same order; the seventh is Alice's own. */
	const int exportSlots = fakk2 ? FNTECH3_FAKK2_CGAME_EXPORT_SLOTS : FNTECH3_ALICE_CGAME_EXPORT_SLOTS;
	for ( int i = 0; i < exportSlots; i++ ) {
		if ( !exports[i] ) {
			VM_Free( vm );
			Com_Error( ERR_DROP, "%s client game left its %s export empty", title,
				fntech3::alice::module::kCGameExports[i].name );
			return nullptr;
		}
	}

	aliceCGameExports = exports;
	CL_AliceFillCGameImports();
	if ( fakk2 ) {
		CL_FAKK2FillCGameImports();
		Com_Printf( "F.A.K.K.2 client game loaded: table at %p\n", (const void *)exports );
		Com_Printf( "  %i of %i client-game imports are served; the rest drop by name.\n",
			CL_FAKK2ServedCGameImports(), FNTECH3_FAKK2_CGAME_IMPORT_SLOTS - 1 );
		return vm;
	}

	Com_Printf( "Alice client game loaded: table at %p\n", (const void *)exports );
	Com_Printf( "  %i of %i client-game imports are served; the rest drop by name.\n",
		CL_AliceServedCGameImports(), FNTECH3_ALICE_CGAME_IMPORT_SLOTS );
	return vm;
}

} // namespace

/*
==================
CL_AliceCGameDraw2D

The client game's 2D layer, export 5. Retail's in-game view calls it after
drawing the view (0x0041fe4e), and only once the client is in the game.
==================
*/
void CL_AliceCGameDraw2D( void ) {
	auto draw2D = CL_AliceCGameExport<AliceCGameVoid_t>( FNTECH3_ALICE_CGE_Draw2D );
	if ( draw2D && cls.state == CA_ACTIVE ) {
		draw2D();
	}
}

#endif
