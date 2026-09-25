/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor: Allied Assault 1.00's client-game boundary

main/cgamex86.dll exports GetCGameAPI, which takes no argument and returns a
table of thirty members; the engine's 168 imports reach the module through
the first of them, CG_Init, with the two sequence numbers and the client
number. Like the game library, it imports its allocator from the
installation's system86.dll, which is loaded from there first.

Every import slot is filled. One the engine does not serve drops by name
when called, so the log names the next thing the module needs. The records
the module hands over or has filled are 1.00's own
(code/compat/mohaa_cgame_abi.h); each is carried into or out of the engine's
at the boundary.

Model handles are this file's own: a TIKI definition is the engine's, shared
with the game, and its handle names the definition as well as whatever the
renderer draws it with. Where a slot's rule is 2015's engine -- which model
registrations run a definition's init commands, how a server model is let
go, how a server command is interpreted -- it follows OpenMoHAA's reading of
that engine (code/client/cl_cgame.cpp and renderergl1/tr_model.cpp,
GPL-2.0-or-later), checked against the retail module's own call sites.
See docs/fntech3/2026-09-24-mohaa-contracts.md.
===========================================================================
*/

extern "C" {
#include "client.h"
}

#include "client_cpp.h"
#include "cl_mohaa.h"
#include "../mohaa/mohaa_tiki.h"
#include "../mohaa/mohaa_system86.h"
#include "../compat/alice_ritualfont.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace mohaa = fntech3::mohaa;

enum ImportSlot : int {
#define FNTECH3_MOHAA_CGAME_IMPORT( slot, name, args, evidence, sites ) CI_##name = slot,
#include "../compat/mohaa_cgame_imports.def"
#undef FNTECH3_MOHAA_CGAME_IMPORT
};

const char *const kImportNames[FNTECH3_MOHAA_CGAME_IMPORT_SLOTS] = {
#define FNTECH3_MOHAA_CGAME_IMPORT( slot, name, args, evidence, sites ) #name,
#include "../compat/mohaa_cgame_imports.def"
#undef FNTECH3_MOHAA_CGAME_IMPORT
};

enum ExportSlot : int {
#define FNTECH3_MOHAA_CGAME_EXPORT( slot, name, kind, evidence ) CE_##name = slot,
#include "../compat/mohaa_cgame_exports.def"
#undef FNTECH3_MOHAA_CGAME_EXPORT
	CE_COUNT
};
static_assert( static_cast<int>( CE_COUNT ) == FNTECH3_MOHAA_CGAME_EXPORT_SLOTS, "thirty export slots" );

std::intptr_t imports[FNTECH3_MOHAA_CGAME_IMPORT_SLOTS];
std::intptr_t unserved[FNTECH3_MOHAA_CGAME_IMPORT_SLOTS];
const std::intptr_t *exports;

template<int Slot>
intptr_t QDECL UnservedImport() {
	Com_Error( ERR_DROP,
		"Medal of Honor client game called import %i (%s), which fnTech3 does not "
		"serve. The module ran to this point; this call is the next thing it needs. "
		"See docs/fntech3/COMPATIBILITY.md.", Slot, kImportNames[Slot] );
	return 0;
}

template<std::size_t... Slots>
void FillUnserved( std::index_sequence<Slots...> ) {
	( ( unserved[Slots] = reinterpret_cast<std::intptr_t>( &UnservedImport<static_cast<int>( Slots )> ) ), ... );
	std::memcpy( imports, unserved, sizeof( imports ) );
}

int ServedImports() {
	int served = 0;
	for ( int i = 0; i < FNTECH3_MOHAA_CGAME_IMPORT_SLOTS; i++ ) {
		served += imports[i] != unserved[i];
	}
	return served;
}

template<typename T>
T Export( int slot ) {
	return exports ? reinterpret_cast<T>( exports[slot] ) : nullptr;
}

template<typename T>
T *Pointer( std::uint32_t address ) {
	return reinterpret_cast<T *>( static_cast<std::uintptr_t>( address ) );
}

/* ---- the data the engine keeps for the client game ---------------------- */

/* Slots 164 to 166: the HUD elements the server's messages fill, the view
 * model's animation state and the stopwatch. The engine keeps them so they
 * outlive a client-game restart, and sets them up as OpenMoHAA's
 * CL_InitClientSavedData does. */
std::array<mohaa::HudDrawElement, FNTECH3_MOHAA_MAX_HUDDRAW_ELEMENTS> hudDrawElements;
mohaa::ClientAnim clientAnim;
mohaa::StopWatch stopWatch;

void InitClientSavedData() {
	clientAnim = mohaa::ClientAnim{};
	clientAnim.g_iLastVMAnim = -1;
	clientAnim.g_iLastVMAnimChanged = -1;
	clientAnim.g_iCurrentVMAnimSlot = 0;
	clientAnim.g_iCurrentVMDuration = 9999;
	clientAnim.g_bCrossblending = 0;
	clientAnim.g_iLastEquippedWeaponStat = -1;
	clientAnim.g_szLastActiveItem[0] = '\0';
	clientAnim.g_iLastAnimPrefixIndex = 0;

	for ( mohaa::HudDrawElement &element : hudDrawElements ) {
		element = mohaa::HudDrawElement{};
		element.vColor[0] = element.vColor[1] = element.vColor[2] = 1.0f;
		element.vColor[3] = 0.0f;
		element.iHorizontalAlign = 1;
		element.iVerticalAlign = 1;
	}
	stopWatch = mohaa::StopWatch{};
}

/* ---- printing, errors, time, memory (slots 1..8) ------------------------ */

void QDECL Printf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	/* A line the game module coded for the HUD's message boxes goes there,
	 * and to the console without its code (UI_PrintConsole). */
	if ( text[0] > 0 && text[0] < 6 ) {
		CL_MOHAAUIPrint( text );
		Com_Printf( "%s", text + 1 );
		return;
	}
	Com_Printf( "%s", text );
}

void QDECL DPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_DPrintf( "%s", text );
}

/* Slot 3, the second developer channel, printed at developer 2. */
void QDECL DebugPrintf( const char *fmt, ... ) {
	if ( Cvar_VariableIntegerValue( "developer" ) < 2 ) {
		return;
	}
	char text[MAXPRINTMSG];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_Printf( "%s", text );
}

void QDECL Error( int level, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	/* A module's fatal error is not the engine's: it drops the session. */
	if ( level == ERR_FATAL ) {
		Com_Error( ERR_DROP, "Medal of Honor client game fatal error: %s", text );
	}
	Com_Error( ERR_DROP, "%s", text );
}

int QDECL Milliseconds() {
	return Sys_Milliseconds();
}

/* Slot 8: the localisation table is not loaded yet, so English passes as it
 * is, as the game's slot does. */
const char *QDECL LV_ConvertString( const char *string ) {
	return string;
}

/* Slots 4 and 5, and the file buffers of slot 18: the installation's own
 * allocator, as for the game (code/mohaa/mohaa_system86.h), since the client
 * game imports MemoryMalloc and MemoryFree from it as well. */
void *QDECL Malloc( int size ) {
	if ( size < 0 ) {
		Com_Error( ERR_DROP, "Medal of Honor client game asked for %i bytes", size );
		return nullptr;
	}
	return MOHAA_ModuleMalloc( size );
}

void QDECL Free( void *block ) {
	MOHAA_ModuleFree( block );
}

/* ---- console variables and commands (slots 9..17) ------------------------ */

/* 2015's flags are Quake III's up to 0x400; its own above that are dropped,
 * as the game's slot drops them. */
constexpr int kSharedCvarFlags = CVAR_ARCHIVE | CVAR_USERINFO | CVAR_SERVERINFO | CVAR_SYSTEMINFO
	| CVAR_INIT | CVAR_LATCH | CVAR_ROM | CVAR_TEMP | CVAR_CHEAT | CVAR_NORESTART;

cvar_t *QDECL CvarGet( const char *name, const char *value, int flags ) {
	if ( !name || !value ) {
		Com_Error( ERR_DROP, "Medal of Honor client game asked for a cvar with no name" );
		return nullptr;
	}
	return Cvar_Get( name, value, flags & kSharedCvarFlags );
}

void QDECL CvarSet( const char *name, const char *value ) {
	if ( name && value ) {
		Cvar_Set( name, value );
	}
}

int QDECL Argc() {
	return Cmd_Argc();
}

const char *QDECL Argv( int arg ) {
	return Cmd_Argv( arg );
}

const char *QDECL Args() {
	return Cmd_ArgsFrom( 1 );
}

/* Slot 14: a name alone, which the engine routes back to the module through
 * the console-command export, as Quake III's client does. */
void QDECL AddCommand( const char *name ) {
	if ( name && name[0] ) {
		Cmd_AddCommand( name, nullptr );
	}
}

void QDECL CmdStuff( const char *text ) {
	if ( text ) {
		Cbuf_AddText( text );
	}
}

/* Slot 16: 2015's execution times are Quake III's: now, insert, append. */
void QDECL CmdExecute( int when, const char *text ) {
	if ( !text ) {
		return;
	}
	if ( when < EXEC_NOW || when > EXEC_APPEND ) {
		when = EXEC_APPEND;
	}
	Cbuf_ExecuteText( static_cast<cbufExec_t>( when ), text );
}

void QDECL CmdTokenizeString( const char *text ) {
	Cmd_TokenizeString( text ? text : "" );
}

/* ---- files (slots 18..21) --------------------------------------------- */

int QDECL ReadFile( const char *path, void **buffer, qboolean quiet ) {
	const int length = MOHAA_ModuleReadFile( path, buffer );
	if ( length < 0 && path && !quiet ) {
		Com_DPrintf( "Medal of Honor client game: %s not found\n", path );
	}
	return length;
}

void QDECL FreeFile( void *buffer ) {
	MOHAA_ModuleFree( buffer );
}

int QDECL WriteFile( const char *path, const void *buffer, int size ) {
	if ( path && buffer && size >= 0 ) {
		FS_WriteFile( path, buffer, size );
		return size;
	}
	return 0;
}

void QDECL WriteTextFile( const char *path, const void *buffer, int size ) {
	if ( path && buffer && size >= 0 ) {
		FS_WriteFile( path, buffer, size );
	}
}

/* ---- the client-game message (slots 23..36) ------------------------------ */

msg_t *Message() {
	msg_t *msg = CL_MOHAACurrentMessage();
	if ( !msg ) {
		Com_Error( ERR_DROP, "Medal of Honor client game read a server message outside one" );
	}
	return msg;
}

int QDECL MsgReadBits( int bits ) {
	return MSG_ReadBitsExternal( Message(), bits );
}

int QDECL MsgReadChar() {
	return MSG_ReadChar( Message() );
}

int QDECL MsgReadByte() {
	return MSG_ReadByte( Message() );
}

int QDECL MsgReadShort() {
	return MSG_ReadShort( Message() );
}

int QDECL MsgReadLong() {
	return MSG_ReadLong( Message() );
}

float QDECL MsgReadFloat() {
	return MSG_ReadFloat( Message() );
}

/* 1.00's strings: 2,048 characters, the high bytes kept and a percent sign
 * turned into a full stop; a line stops at a newline too. */
const char *ReadMessageString( bool line ) {
	static char text[FNTECH3_MOHAA_MAX_STRING_CHARS];
	msg_t *msg = Message();
	std::size_t length = 0;
	for ( ;; ) {
		const int c = MSG_ReadByte( msg );
		if ( c <= 0 || ( line && c == '\n' ) ) {
			break;
		}
		if ( length + 1 < sizeof( text ) ) {
			text[length++] = c == '%' ? '.' : static_cast<char>( c );
		}
	}
	text[length] = '\0';
	return text;
}

const char *QDECL MsgReadString() {
	return ReadMessageString( false );
}

const char *QDECL MsgReadStringLine() {
	return ReadMessageString( true );
}

/* The game writes an 8-bit angle as `f * 256 / 360` in eight bits and a
 * 16-bit one as a short; each is read back at the width it was written. */
float QDECL MsgReadAngle8() {
	return MSG_ReadByte( Message() ) * ( 360.0f / 256.0f );
}

float QDECL MsgReadAngle16() {
	return SHORT2ANGLE( MSG_ReadShort( Message() ) );
}

void QDECL MsgReadData( void *data, int length ) {
	if ( data && length > 0 ) {
		MSG_ReadData( Message(), data, length );
	}
}

/* A coordinate: nineteen bits, the top one the sign, in sixteenths. */
float QDECL MsgReadCoord() {
	int read = MSG_ReadBitsExternal( Message(), 19 );
	const float sign = ( read & 0x40000 ) ? -1.0f : 1.0f;
	read &= ~0x40000;
	return sign * static_cast<float>( read ) / 16.0f;
}

void QDECL MsgReadDir( vec3_t dir ) {
	if ( dir ) {
		ByteToDir( MSG_ReadByte( Message() ), dir );
	}
}

void QDECL SendClientCommand( const char *command ) {
	if ( command ) {
		CL_AddReliableCommand( command, qfalse );
	}
}

/* ---- collision (slots 38..48) ----------------------------------------- */

/* Slot 38 takes the map's name alone. Loading a map empties the global alias
 * list (OpenMoHAA cm_load.c CM_LoadMap), and the client game registers the
 * map's aliases straight after: CG_Init is its only caller (cgamex86.dll
 * 0x300235e9, 0x300235f7). So the list is emptied here even for the map the
 * client already holds, which only registers the same aliases again.
 * Playing on this process's own server it stays: that server's spawn emptied
 * it for this map, and in multiplayer its game has registered into it since. */
void QDECL LoadMap( const char *name ) {
	const bool ownServer = com_sv_running && com_sv_running->integer && !clc.demoplaying
		&& clc.serverAddress.type == NA_LOOPBACK;
	if ( !ownServer ) {
		MOHAA_GlobalAliasClear();
	}
	int checksum;
	CM_LoadMap( name, qtrue, &checksum );
}

clipHandle_t QDECL InlineModel( int index ) {
	if ( index < 0 || index >= CM_NumInlineModels() ) {
		Com_Error( ERR_DROP, "Medal of Honor client game asked for inline model %i of %i", index,
			CM_NumInlineModels() );
		return 0;
	}
	return CM_InlineModel( index );
}

int QDECL NumInlineModels() {
	return CM_NumInlineModels();
}

int QDECL PointContents( const vec3_t point, int headnode ) {
	return CM_PointContents( point, headnode );
}

int QDECL TransformedPointContents( const vec3_t point, clipHandle_t model, const vec3_t origin,
	const vec3_t angles ) {
	return CM_TransformedPointContents( point, model, origin, angles );
}

/* The engine's trace, in 2015's record: its hit shader follows the surface
 * flags, and a hit location and the entity pointer follow the entity. The
 * collision model does not name the shader it hit, so it is the map's first;
 * nothing on this side resolves a hit to a body location. */
void ToModuleTrace( const trace_t &in, mohaa::Trace &out ) {
	out = mohaa::Trace{};
	out.allsolid = in.allsolid;
	out.startsolid = in.startsolid;
	out.fraction = in.fraction;
	VectorCopy( in.endpos, out.endpos );
	VectorCopy( in.plane.normal, out.plane.normal );
	out.plane.dist = in.plane.dist;
	out.plane.type = in.plane.type;
	out.plane.signbits = in.plane.signbits;
	out.surfaceFlags = in.surfaceFlags;
	out.shaderNum = 0;
	out.contents = in.contents;
	out.entityNum = in.entityNum;
	out.location = -1;
	out.ent = 0;
}

/* 2015's `cylinder` traces a vertical cylinder rather than a box; the
 * collision model here traces the box that bounds it. */
void QDECL BoxTrace( mohaa::Trace *results, const vec3_t start, const vec3_t end, const vec3_t mins,
	const vec3_t maxs, int headnode, int brushmask, qboolean cylinder ) {
	(void)cylinder;
	if ( !results ) {
		return;
	}
	trace_t trace;
	CM_BoxTrace( &trace, start, end, mins, maxs, headnode, brushmask, qfalse );
	ToModuleTrace( trace, *results );
}

void QDECL TransformedBoxTrace( mohaa::Trace *results, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs, int headnode, int brushmask, const vec3_t origin,
	const vec3_t angles, qboolean cylinder ) {
	(void)cylinder;
	if ( !results ) {
		return;
	}
	trace_t trace;
	CM_TransformedBoxTrace( &trace, start, end, mins, maxs, headnode, brushmask, origin, angles, qfalse );
	ToModuleTrace( trace, *results );
}

clipHandle_t QDECL TempBoxModel( const vec3_t mins, const vec3_t maxs, int contents ) {
	const clipHandle_t handle = CM_TempBoxModel( mins, maxs, qfalse );
	CM_SetTempBoxModelContents( contents );
	return handle;
}

void QDECL PrintBSPFileSizes() {
	Com_Printf( "%i inline models\n", CM_NumInlineModels() );
}

qboolean QDECL LeafInPVS( int leaf1, int leaf2 ) {
	const int cluster1 = CM_LeafCluster( leaf1 );
	const int cluster2 = CM_LeafCluster( leaf2 );
	if ( cluster1 < 0 || cluster2 < 0 ) {
		return qfalse;
	}
	const byte *vis = CM_ClusterPVS( cluster1 );
	return ( vis[cluster2 >> 3] & ( 1 << ( cluster2 & 7 ) ) ) ? qtrue : qfalse;
}

int QDECL PointLeafnum( const vec3_t point ) {
	return CM_PointLeafnum( point );
}

/* ---- decals and lighting queries (slots 49..54) ---------------------- */

/* The renderer's fragments are Quake III's, eight bytes; 1.00's carry a
 * third word naming what the fragment lies on: a terrain patch's number
 * plus one, which the client game draws through slot 92, or 0 for the
 * world's own surfaces and for a brush model's, whose fragments the client
 * game names itself (tr_mohaa_marks_surface.h). */
int CutFragments( int model, const vec3_t angles, const vec3_t origin, int numPoints, const vec3_t *points,
	const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments,
	mohaa::MarkFragment *fragmentBuffer, float radiusSquared ) {
	if ( !re.MarkMOHAAFragments || !points || !projection || !pointBuffer || !fragmentBuffer || maxPoints <= 0
		|| maxFragments <= 0 ) {
		return 0;
	}
	std::vector<markFragment_t> fragments( static_cast<std::size_t>( maxFragments ) );
	std::vector<int> patches( static_cast<std::size_t>( maxFragments ), 0 );
	const int count = re.MarkMOHAAFragments( model, angles, origin, numPoints, points, projection, maxPoints,
		pointBuffer, maxFragments, fragments.data(), patches.data(), radiusSquared );
	for ( int i = 0; i < count; i++ ) {
		fragmentBuffer[i].firstPoint = fragments[i].firstPoint;
		fragmentBuffer[i].numPoints = fragments[i].numPoints;
		fragmentBuffer[i].iIndex = patches[i];
	}
	return count;
}

/* Slot 49: the world's, its terrain's among them unless the decal is too
 * small for terrain (ter_minMarkRadius). */
int QDECL MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints,
	vec3_t pointBuffer, int maxFragments, mohaa::MarkFragment *fragmentBuffer, float radiusSquared ) {
	return CutFragments( 0, vec3_origin, vec3_origin, numPoints, points, projection, maxPoints, pointBuffer,
		maxFragments, fragmentBuffer, radiusSquared );
}

/* Slot 50: a brush model's, cut and returned in the model's space, which
 * the client game carries with the model as it moves. The model is the
 * collision model's inline model, the renderer's brush model of the same
 * number. */
int QDECL MarkFragmentsForInlineModel( clipHandle_t model, const vec3_t angles, const vec3_t origin,
	int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer,
	int maxFragments, mohaa::MarkFragment *fragmentBuffer, float radiusSquared ) {
	if ( model <= 0 || model >= CM_NumInlineModels() || !angles || !origin ) {
		return 0;
	}
	return CutFragments( model, angles, origin, numPoints, points, projection, maxPoints, pointBuffer,
		maxFragments, fragmentBuffer, radiusSquared );
}

void QDECL GetInlineModelBounds( int index, vec3_t mins, vec3_t maxs ) {
	if ( !mins || !maxs ) {
		return;
	}
	if ( index < 0 || index >= CM_NumInlineModels() ) {
		VectorClear( mins );
		VectorClear( maxs );
		return;
	}
	CM_ModelBounds( CM_InlineModel( index ), mins, maxs );
}

/* Slots 52 and 53: the light at a point, from the renderer's light grid, on
 * a byte's scale with its hue kept under 255 (OpenMoHAA
 * R_GetLightingForDecal, reference only); full light without a grid. */
void LightAt( vec3_t light, const vec3_t origin ) {
	vec3_t ambient, directed, direction;
	vec3_t point;
	VectorCopy( origin, point );
	if ( re.LightForPoint && re.LightForPoint( point, ambient, directed, direction ) ) {
		for ( int i = 0; i < 3; i++ ) {
			light[i] = ambient[i] + directed[i];
		}
		const float peak = std::max( light[0], std::max( light[1], light[2] ) );
		if ( peak > 255.0f ) {
			VectorScale( light, 255.0f / peak, light );
		}
		return;
	}
	VectorSet( light, 255.0f, 255.0f, 255.0f );
}

/* A decal's light stays on that scale: every caller stores it in a vertex
 * colour's byte as it is or times its own 0..1 colour (CG_ImpactMark at
 * 0x300251f1, the permanent marks at 0x300272c4 and 0x300274cf, the tread
 * marks at 0x30027bec and 0x30027ea9). */
void QDECL GetLightingForDecal( vec3_t light, const vec3_t facing, const vec3_t origin ) {
	if ( !light || !origin ) {
		return;
	}
	LightAt( light, origin );
	/* the renderers' grid reads a point whose neighbours all lie in walls
	 * as unlit, which a thin wall's face can be: look out from the face,
	 * the way it faces */
	if ( facing && light[0] + light[1] + light[2] <= 0.0f ) {
		for ( const float step : { 16.0f, 48.0f } ) {
			vec3_t out;
			VectorMA( origin, step, facing, out );
			LightAt( light, out );
			if ( light[0] + light[1] + light[2] > 0.0f ) {
				break;
			}
		}
	}
}

/* A smoke puff's is the same light over 255 (OpenMoHAA
 * R_GetLightingForSmoke). */
void QDECL GetLightingForSmoke( vec3_t light, const vec3_t origin ) {
	if ( light && origin ) {
		LightAt( light, origin );
		VectorScale( light, 1.0f / 255.0f, light );
	}
}

/* The areas the last view drawn could see: a set bit is one it could not. */
byte sceneAreamask[MAX_MAP_AREA_BYTES];
bool sceneAreamaskSet = false;

/* Slot 54: the lights reaching a point, the sun first, which the client game
 * casts a character's foot shadows from under "Complex" shadows (cg_shadows
 * 2; its one call, 0x30029b98). The map's sphere lights, as the collision
 * model keeps them (cm_mohaa_lights.c), seen from the last view drawn, as
 * 2015's renderer reads them with its back end's area mask (OpenMoHAA
 * tr_sphere_shade.cpp R_GatherLightSources). */
int QDECL GatherLightSources( const vec3_t position, vec3_t *lightPositions, vec3_t *lightIntensities,
	int maxLights ) {
	if ( !position || !lightPositions || !lightIntensities || maxLights <= 0 ) {
		return 0;
	}
	return CM_MOHAAGatherLights( position, sceneAreamaskSet ? sceneAreamask : nullptr, lightPositions,
		lightIntensities, maxLights );
}

/* ---- sound (slots 55..73) ---------------------------------------------- */

/* 2015's channels (OpenMoHAA's CHAN_ list) onto Quake III's. */
int EngineChannel( int channel ) {
	switch ( channel ) {
	case 1: return CHAN_BODY;
	case 2: return CHAN_ITEM;
	case 3: return CHAN_WEAPON;	// weapon idle
	case 4: return CHAN_VOICE;
	case 5: return CHAN_LOCAL;
	case 6: return CHAN_WEAPON;
	case 7: return CHAN_VOICE;	// secondary dialog
	case 8: return CHAN_VOICE;	// dialog
	case 9: return CHAN_LOCAL_SOUND;	// menu
	default: return CHAN_AUTO;
	}
}

/* The names the client game registered its sounds by, for the sounds a
 * level's script waits on (cl_mohaa_sound.cpp). */
std::map<sfxHandle_t, std::string> soundNames;

/* A sound the level's script waits on carries this on its entity number, in
 * the snapshot's eleven bits (OpenMoHAA S_FLAG_DO_CALLBACK). */
constexpr int kSoundWaitedFlag = 0x400;

/* A sound with its alias's volume, distances and pitch; a value below zero is
 * the alias's "none", which the backend fills in. Streaming is the backend's
 * own choice. A waited sound is its entity's like any other, and the local
 * server's game is told which channel took it. */
void QDECL StartSound( const vec3_t origin, int entityNumber, int channel, sfxHandle_t sfx, float volume,
	float minDist, float pitch, float maxDist, int streamed ) {
	(void)streamed;
	if ( sfx <= 0 ) {
		return;
	}
	const bool waited = entityNumber >= 0 && ( entityNumber & kSoundWaitedFlag );
	if ( waited ) {
		entityNumber &= ~kSoundWaitedFlag;
	}
	if ( entityNumber < 0 || entityNumber >= MAX_GENTITIES ) {
		entityNumber = ENTITYNUM_WORLD;
	}
	S_StartMOHAASound( origin, entityNumber, EngineChannel( channel ), sfx, volume, minDist, maxDist, pitch, channel );
	const auto name = soundNames.find( sfx );
	const char *sound = name != soundNames.end() ? name->second.c_str() : "";
	const int id = CL_MOHAASoundStarted( entityNumber, channel, sound, pitch, waited );
	if ( waited && id >= 0 && com_sv_running && com_sv_running->integer ) {
		SV_MOHAASoundCallback( entityNumber, id, sound );
	}
}

/* The sound on an entity's channel ends (OpenMoHAA S_StopSound, reference
 * only). 2015's loops belong to no entity, so none of them goes with it. */
void QDECL StopSound( int entityNumber, int channel ) {
	if ( entityNumber >= 0 && entityNumber < MAX_GENTITIES ) {
		S_StopMOHAASound( entityNumber, channel );
		CL_MOHAASoundStopped( entityNumber, channel );
	}
}

/* 2015's looping sounds belong to no entity: the client game hands over the
 * whole set each frame. Each takes a voice numbered down from the top of the
 * entity range, and keeps last frame's while its sound and place are the
 * same, so a loop plays on unbroken when others come and go. */
constexpr int kLoopSlots = MAX_GENTITIES / 2;

struct LoopSlot {
	sfxHandle_t sfx = 0;
	vec3_t origin = { 0.0f, 0.0f, 0.0f };
	bool used = false;
};
LoopSlot previousLoops[kLoopSlots];
LoopSlot currentLoops[kLoopSlots];

void QDECL ClearLoopingSounds() {
	S_ClearLoopingSounds( qfalse );
	std::memcpy( previousLoops, currentLoops, sizeof( previousLoops ) );
	for ( LoopSlot &slot : currentLoops ) {
		slot.used = false;
	}
}

int LoopSlotFor( sfxHandle_t sfx, const vec3_t origin ) {
	for ( int i = 0; i < kLoopSlots; i++ ) {
		const LoopSlot &last = previousLoops[i];
		if ( last.used && !currentLoops[i].used && last.sfx == sfx
			&& DistanceSquared( last.origin, origin ) < 16.0f * 16.0f ) {
			return i;
		}
	}
	for ( int i = 0; i < kLoopSlots; i++ ) {
		if ( !currentLoops[i].used && !previousLoops[i].used ) {
			return i;
		}
	}
	for ( int i = 0; i < kLoopSlots; i++ ) {
		if ( !currentLoops[i].used ) {
			return i;
		}
	}
	return -1;
}

void QDECL AddLoopingSound( const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx, float volume,
	float minDist, float maxDist, float pitch, int flags ) {
	(void)flags;
	if ( !origin || sfx <= 0 ) {
		return;
	}
	const int slot = LoopSlotFor( sfx, origin );
	if ( slot < 0 ) {
		return;
	}
	LoopSlot &taken = currentLoops[slot];
	taken.used = true;
	taken.sfx = sfx;
	VectorCopy( origin, taken.origin );
	const vec3_t still = { 0.0f, 0.0f, 0.0f };
	S_AddMOHAALoopingSound( MAX_GENTITIES - 2 - slot, origin, velocity ? velocity : still, sfx, volume,
		minDist, maxDist, pitch );
}

void QDECL Respatialize( int entityNumber, const vec3_t origin, vec3_t axis[3] ) {
	if ( origin && axis ) {
		S_Respatialize( entityNumber, origin, axis, 0 );
	}
}

sfxHandle_t QDECL RegisterSound( const char *name, int streamed ) {
	(void)streamed;
	if ( !name || !name[0] ) {
		return 0;
	}
	const sfxHandle_t sfx = S_RegisterSound( name, qfalse );
	if ( sfx > 0 ) {
		soundNames[sfx] = name;
	}
	/* A sound is one of a level's load record's resources (kind k). */
	MOHAA_LoadResource( va( "k%s", name ) );
	return sfx;
}

void QDECL UpdateEntity( int entityNumber, const vec3_t origin, const vec3_t velocity, qboolean useListener ) {
	(void)velocity; (void)useListener;
	if ( origin && entityNumber >= 0 && entityNumber < MAX_GENTITIES ) {
		S_UpdateEntityPosition( entityNumber, origin );
	}
}

/* Slot 65: the room the player is in, from the player state (the level's
 * script sets it), for the audio's environment. */
void QDECL SetReverb( int type, float level ) {
	S_MOHAASetReverb( type, level );
}

int QDECL ChannelNameToNum( const char *name ) {
	static const char *const channels[] = {
		"auto", "body", "item", "weaponidle", "voice", "local", "weapon",
		"dialog_secondary", "dialog", "menu"
	};
	if ( !name ) {
		return -1;
	}
	for ( int i = 0; i < static_cast<int>( ARRAY_LEN( channels ) ); i++ ) {
		if ( !Q_stricmp( name, channels[i] ) ) {
			return i;
		}
	}
	return -1;
}

void QDECL MusicNewSoundtrack( const char *name ) {
	CL_MOHAAMusicNewSoundtrack( name );
}

void QDECL MusicUpdateMood( int current, int fallback ) {
	CL_MOHAAMusicUpdateMood( current, fallback );
}

void QDECL MusicUpdateVolume( float volume, float fadeTime ) {
	CL_MOHAAMusicUpdateVolume( volume, fadeTime );
}

/* Slot 74: the camera look the input layer drives; nothing drives it. */
float cameraOffset[3];

float *QDECL GetCameraOffset( qboolean *lookActive, qboolean *resetView ) {
	if ( lookActive ) {
		*lookActive = qfalse;
	}
	if ( resetView ) {
		*resetView = qfalse;
	}
	return cameraOffset;
}

/* ---- models (slots 81..84, 97..99, 130) ---------------------------------- */

struct Model {
	std::string name;
	qhandle_t renderer = 0;
	dtiki_t *tiki = nullptr;
	bool serverOnly = false;
	bool bad = false;
	bool sprite = false;
};

std::vector<Model> models( 1 );

const Model *ModelFor( qhandle_t handle ) {
	if ( handle <= 0 || handle >= static_cast<qhandle_t>( models.size() ) ) {
		return nullptr;
	}
	const Model &model = models[handle];
	return model.name.empty() || model.bad ? nullptr : &model;
}

typedef void ( QDECL *ProcessInitCommands_t )( dtiki_t *tiki, mohaa::RefEntity *entity );
typedef void ( QDECL *EndTiki_t )( dtiki_t *tiki );

/* The renderer keeps 63 characters of a model's name, and an actor's name
 * is longer: it leads with the weapon, head and skin it wears
 * (`weapon|m1 garand|headmodel|head4|headskin|us_j|models/human/...tik`),
 * each combination its own definition. Such a model is known to the
 * renderer by the end of its name and a hash of all of it. */
std::string RendererModelName( const char *name ) {
	const std::size_t length = std::strlen( name );
	if ( length < MAX_QPATH ) {
		return name;
	}
	std::uint32_t hash = 2166136261u;
	for ( const char *c = name; *c; c++ ) {
		hash = ( hash ^ static_cast<std::uint32_t>( std::tolower( static_cast<unsigned char>( *c ) ) ) ) * 16777619u;
	}
	char suffix[16];
	std::snprintf( suffix, sizeof( suffix ), "#%08x", hash );
	const std::size_t keep = MAX_QPATH - 1 - std::strlen( suffix );
	return std::string( name + length - keep ) + suffix;
}

/* A model's first registration, as 2015's R_RegisterModelInternal does it: a
 * definition is registered with the engine's cache and, when the call asks
 * for it, the client game runs its init commands with no entity. */
qhandle_t RegisterModelInternal( const char *name, bool beginTiki, bool use ) {
	if ( !name || !name[0] ) {
		Com_Printf( "RE_RegisterModel: NULL name\n" );
		return 0;
	}
	if ( std::strlen( name ) >= 128 ) {
		Com_Printf( "Model name exceeds MAX_MODEL_NAME\n" );
		return 0;
	}
	for ( std::size_t i = 1; i < models.size(); i++ ) {
		if ( !models[i].name.empty() && !Q_stricmp( models[i].name.c_str(), name ) ) {
			return models[i].bad ? 0 : static_cast<qhandle_t>( i );
		}
	}

	Model model;
	model.name = name;
	const char *extension = COM_GetExtension( name );
	if ( !Q_stricmp( extension, "tik" ) ) {
		model.tiki = TIKI_RegisterTikiFlags( name, use ? qtrue : qfalse );
		if ( !model.tiki ) {
			model.bad = true;
		} else if ( re.RegisterMOHAATiki ) {
			model.renderer = re.RegisterMOHAATiki( RendererModelName( name ).c_str(), model.tiki );
		}
	} else if ( !Q_stricmp( extension, "spr" ) ) {
		model.renderer = re.RegisterModel( name );
		model.bad = model.renderer == 0;
		model.sprite = !model.bad;
	} else {
		model.renderer = re.RegisterModel( name );
		model.bad = model.renderer == 0;
	}

	models.push_back( model );
	const qhandle_t handle = static_cast<qhandle_t>( models.size() - 1 );
	if ( model.bad ) {
		Com_Printf( S_COLOR_YELLOW "RE_RegisterModel: Registration failed for '%s'\n", name );
		return 0;
	}
	if ( model.tiki && beginTiki ) {
		const auto processInitCommands = Export<ProcessInitCommands_t>( CE_ProcessInitCommands );
		if ( processInitCommands ) {
			processInitCommands( model.tiki, nullptr );
		}
	}
	return handle;
}

qhandle_t QDECL RegisterModel( const char *name ) {
	const qhandle_t handle = RegisterModelInternal( name, true, true );
	if ( handle ) {
		models[handle].serverOnly = false;
	}
	return handle;
}

/* Slot 83: a model the server names, kept only while the server does. */
qhandle_t QDECL RegisterServerModel( const char *name ) {
	const std::size_t before = models.size();
	const qhandle_t handle = RegisterModelInternal( name, true, false );
	if ( handle && models.size() > before ) {
		models[handle].serverOnly = true;
	}
	return handle;
}

void QDECL UnregisterServerModel( qhandle_t handle ) {
	if ( handle <= 0 || handle >= static_cast<qhandle_t>( models.size() ) ) {
		return;
	}
	Model &model = models[handle];
	if ( !model.serverOnly ) {
		return;
	}
	if ( model.tiki ) {
		const auto endTiki = Export<EndTiki_t>( CE_EndTiki );
		if ( endTiki ) {
			endTiki( model.tiki );
		}
	}
	model = Model{};
}

/* Slot 82: an effect's model, whose init commands run on an entity at the
 * given place and orientation. */
qhandle_t QDECL SpawnEffectModel( const char *name, vec3_t position, vec3_t axis[3] ) {
	mohaa::RefEntity entity{};
	std::memset( entity.shaderRGBA, 255, sizeof( entity.shaderRGBA ) );
	if ( position ) {
		VectorCopy( position, entity.origin );
	}
	entity.scale = 1.0f;
	if ( axis ) {
		for ( int i = 0; i < 3; i++ ) {
			VectorCopy( axis[i], entity.axis[i] );
		}
	}
	entity.hModel = RegisterModelInternal( name, false, true );
	if ( entity.hModel ) {
		Model &model = models[entity.hModel];
		model.serverOnly = false;
		if ( model.tiki ) {
			entity.tiki = static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( model.tiki ) );
			const auto processInitCommands = Export<ProcessInitCommands_t>( CE_ProcessInitCommands );
			if ( processInitCommands ) {
				processInitCommands( model.tiki, &entity );
			}
		}
	}
	return entity.hModel;
}

dtiki_t *QDECL ModelGetHandle( qhandle_t handle ) {
	const Model *model = ModelFor( handle );
	return model ? model->tiki : nullptr;
}

void QDECL ModelBounds( clipHandle_t handle, vec3_t mins, vec3_t maxs ) {
	if ( !mins || !maxs ) {
		return;
	}
	const Model *model = ModelFor( handle );
	if ( model && model->tiki ) {
		TIKI_CalculateBounds( model->tiki, 1.0f, mins, maxs );
		return;
	}
	if ( model && model->renderer ) {
		re.ModelBounds( model->renderer, mins, maxs );
		return;
	}
	VectorClear( mins );
	VectorClear( maxs );
}

float QDECL ModelRadius( clipHandle_t handle ) {
	const Model *model = ModelFor( handle );
	if ( model && model->tiki ) {
		return TIKI_GlobalRadius( model->tiki );
	}
	if ( model && model->sprite && re.GetAliceModelRadius ) {
		/* Half the sprite's larger side, which its flat bounds would not
		 * give (OpenMoHAA's R_ModelRadius). */
		return re.GetAliceModelRadius( model->renderer );
	}
	vec3_t mins, maxs;
	ModelBounds( handle, mins, maxs );
	return RadiusFromBounds( mins, maxs );
}

/* ---- shaders and 2D (slots 86, 87, 93..96, 106..108) --------------------- */

qhandle_t QDECL RegisterShader( const char *name ) {
	return name && name[0] ? re.RegisterShader( name ) : 0;
}

qhandle_t QDECL RegisterShaderNoMip( const char *name ) {
	return name && name[0] ? re.RegisterShaderNoMip( name ) : 0;
}

void QDECL SetColor( const float *rgba ) {
	re.SetColor( rgba );
}

void QDECL DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2,
	qhandle_t shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}

int QDECL GetShaderWidth( qhandle_t shader ) {
	int width = 0, height = 0;
	if ( re.GetShaderImageSize ) {
		re.GetShaderImageSize( shader, &width, &height );
	}
	return width;
}

int QDECL GetShaderHeight( qhandle_t shader ) {
	int width = 0, height = 0;
	if ( re.GetShaderImageSize ) {
		re.GetShaderImageSize( shader, &width, &height );
	}
	return height;
}

void QDECL DrawBox( float x, float y, float w, float h ) {
	re.DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, cls.whiteShader );
}

/* Slots 95 and 96: 2015's fonts are Ritual's `.RitualFont` glyph tables over a
 * picture, `gfx/fonts/<name>`, read by code/compat/alice_ritualfont.hpp. The
 * client game keeps the pointer it is handed and passes it back to draw with;
 * it reads nothing inside. A font that does not load is a null pointer, as
 * retail's R_LoadFont answers.
 *
 * The scripts give every font `rgbGen identity`, yet the text takes the colour
 * the client game set, as retail's font shader is forced to (OpenMoHAA
 * renderergl1/tr_font.cpp R_LoadFontShader); the picture is registered as a
 * plain 2D image, which takes the colour and blends by alpha as the font stage
 * does. */
namespace ritualfont = fntech3::alice::ritualfont;

struct Font {
	char name[FNTECH3_MOHAA_MAX_QPATH];
	ritualfont::Font metrics;
	qhandle_t shader = 0;
	int epoch = -1;
};
std::vector<std::unique_ptr<Font>> fonts;
/* Bumped when the renderer restarts: a font's picture is registered again. */
int fontEpoch = 0;

void *QDECL LoadFont( const char *name ) {
	if ( !name || !name[0] ) {
		return nullptr;
	}
	for ( const std::unique_ptr<Font> &font : fonts ) {
		if ( !Q_stricmp( font->name, name ) ) {
			return font.get();
		}
	}
	void *buffer = nullptr;
	const int length = FS_ReadFile( va( "fonts/%s.RitualFont", name ), &buffer );
	if ( length <= 0 || !buffer ) {
		if ( buffer ) {
			FS_FreeFile( buffer );
		}
		Com_Printf( "LoadFont: Couldn't load font %s\n", name );
		return nullptr;
	}
	auto font = std::make_unique<Font>();
	Q_strncpyz( font->name, name, sizeof( font->name ) );
	std::string error;
	const bool parsed = ritualfont::Parse( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ),
		font->metrics, &error );
	FS_FreeFile( buffer );
	if ( !parsed ) {
		Com_DPrintf( "font %s: %s\n", name, error.c_str() );
		Com_Printf( "WARNING: Error parsing font %s.\n", name );
		return nullptr;
	}
	fonts.push_back( std::move( font ) );
	return fonts.back().get();
}

qhandle_t FontShader( Font &font ) {
	if ( font.epoch == fontEpoch ) {
		return font.shader;
	}
	font.epoch = fontEpoch;
	font.shader = 0;
	if ( re.RegisterShaderFromRGBA && re.LoadImagePixels ) {
		int width = 0, height = 0;
		byte *pixels = re.LoadImagePixels( va( "gfx/fonts/%s.tga", font.name ), &width, &height );
		if ( pixels && width > 0 && height > 0 ) {
			font.shader = re.RegisterShaderFromRGBA( va( "*fntech3/mohaa/font/%s", font.name ), pixels, width, height );
		}
		if ( pixels ) {
			Z_Free( pixels );
		}
	}
	if ( !font.shader ) {
		font.shader = re.RegisterShaderNoMip( va( "gfx/fonts/%s", font.name ) );
	}
	if ( !font.shader ) {
		Com_Printf( "Could not load font shader for %s\n", font.name );
	}
	return font.shader;
}

/* Glyphs left to right from (x, y) in the colour last set, every position and
 * size times (scaleX, scaleY): a tab three spaces wide, a newline back to x one
 * font height down, a carriage return back to x; a byte the font has no glyph
 * for draws its '?' (OpenMoHAA R_DrawString_sgl). */
void DrawStringScaled( void *font, const char *text, float x, float y, int maxLength, float scaleX, float scaleY ) {
	Font *f = static_cast<Font *>( font );
	if ( !f || !text ) {
		return;
	}
	const qhandle_t shader = FontShader( *f );
	if ( !shader ) {
		return;
	}
	const ritualfont::Font &metrics = f->metrics;
	const float charHeight = metrics.height;
	const float startX = x;
	float startY = y;
	for ( int i = 0; text[i]; i++ ) {
		if ( maxLength != -1 && i >= maxLength ) {
			break;
		}
		const unsigned char c = static_cast<unsigned char>( text[i] );
		if ( c == '\t' ) {
			const int space = metrics.indirection[' '];
			if ( space >= 0 && space < ritualfont::GlyphCount ) {
				x += metrics.locations[space].width * 256.0f * 3.0f;
			}
			continue;
		}
		if ( c == '\n' ) {
			startY += charHeight;
			x = startX;
			y = startY;
			continue;
		}
		if ( c == '\r' ) {
			x = startX;
			continue;
		}
		int index = metrics.indirection[c];
		if ( index < 0 || index >= ritualfont::GlyphCount ) {
			index = metrics.indirection['?'];
			if ( index < 0 || index >= ritualfont::GlyphCount ) {
				continue;
			}
		}
		const ritualfont::Glyph &glyph = metrics.locations[index];
		const float width = glyph.width * 256.0f;
		re.DrawStretchPic( x * scaleX, y * scaleY, width * scaleX, charHeight * scaleY,
			glyph.s, glyph.t, glyph.s + glyph.width, glyph.t + glyph.height, shader );
		x += width;
	}
}

/* R_DrawString (slot 96): 1.00's last argument is a flag, not the later
 * games' pair of scale factors -- its HUD elements pass their virtual-screen
 * word (+0x6c of the element, cgamex86.dll 0x3001f0fd) and every other site
 * pushes 0 -- and a set flag takes the positions and sizes as 640x480 ones. */
void QDECL DrawString( void *font, const char *text, float x, float y, int maxLength, int virtualScreen ) {
	if ( virtualScreen ) {
		DrawStringScaled( font, text, x, y, maxLength, cls.glconfig.vidWidth / 640.0f, cls.glconfig.vidHeight / 480.0f );
	} else {
		DrawStringScaled( font, text, x, y, maxLength, 1.0f, 1.0f );
	}
}

/* ---- the scene (slots 75..77, 88..92, 97, 100..105) ----------------------- */

/* 2015's render flags onto the engine's Quake III ones. */
int EngineRenderFx( int renderfx ) {
	int out = 0;
	if ( renderfx & ( 1 << 0 ) ) out |= RF_THIRD_PERSON;
	if ( renderfx & ( 1 << 1 ) ) out |= RF_FIRST_PERSON;
	if ( renderfx & ( 1 << 2 ) ) out |= RF_DEPTHHACK;
	if ( renderfx & ( 1 << 17 ) ) out |= RF_MINLIGHT;
	if ( renderfx & ( 1 << 19 ) ) out |= RF_LIGHTING_ORIGIN;
	if ( renderfx & ( 1 << 20 ) ) out |= RF_SHADOW_PLANE;
	if ( renderfx & ( 1 << 21 ) ) out |= RF_WRAP_FRAMES;
	return out;
}

constexpr int kRfDontDraw = 1 << 7;

/* Every entity added this frame, as the module submitted it, which slot 97
 * finds by number whether or not it is drawn. */
std::vector<mohaa::RefEntity> sceneEntities;
constexpr std::size_t kMaxSceneEntities = 4096;

void QDECL ClearScene() {
	sceneEntities.clear();
	re.ClearScene();
}

void BaseEntity( const mohaa::RefEntity &in, refEntity_t &out ) {
	out = refEntity_t{};
	out.renderfx = EngineRenderFx( in.renderfx );
	VectorCopy( in.lightingOrigin, out.lightingOrigin );
	out.shadowPlane = in.shadowPlane;
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( in.axis[i], out.axis[i] );
	}
	out.nonNormalizedAxes = in.nonNormalizedAxes ? qtrue : qfalse;
	VectorCopy( in.origin, out.origin );
	VectorCopy( in.oldorigin, out.oldorigin );
	out.skinNum = in.skinNum;
	out.customShader = in.customShader;
	std::memcpy( out.shader.rgba, in.shaderRGBA, sizeof( out.shader.rgba ) );
	out.shaderTexCoord[0] = in.shaderTexCoord[0];
	out.shaderTexCoord[1] = in.shaderTexCoord[1];
	out.shaderTime.f = in.shaderTime;
	out.radius = in.radius;
	out.rotation = in.rotation;
	/* 2015's own flags ride along for the renderer's view tests
	 * (R_MOHAAEntityInView); a TIKI entity's pose record keeps them too. */
	out.mohaa.renderfx = in.renderfx;
}

/* A TIKI entity's pose and surface state, for the renderer to ask the engine
 * to pose it by (code/renderercommon/tr_mohaa_render.h). The bone
 * controllers are the entity's own arrays, which the client game keeps
 * while it draws. */
void TikiEntity( const mohaa::RefEntity &in, dtiki_t *tiki, mohaaRenderEntity_t &out ) {
	static_assert( sizeof( out.pose.frameInfo ) == sizeof( in.frameInfo ), "sixteen frame infos" );
	out = mohaaRenderEntity_t{};
	out.pose.tiki = tiki;
	out.pose.entityNumber = in.entityNumber;
	std::memcpy( out.pose.frameInfo, in.frameInfo, sizeof( out.pose.frameInfo ) );
	const int *tags = Pointer<const int>( in.bone_tag );
	const float *quats = Pointer<const float>( in.bone_quat );
	for ( int i = 0; i < FNTECH3_MOHAA_NUM_BONE_CONTROLLERS; i++ ) {
		out.pose.boneTag[i] = tags ? tags[i] : -1;
		for ( int j = 0; j < 4; j++ ) {
			out.pose.boneQuat[i][j] = quats ? quats[i * 4 + j] : ( j == 3 ? 1.0f : 0.0f );
		}
	}
	out.pose.actionWeight = in.actionWeight;
	out.scale = in.scale;
	out.renderfx = in.renderfx;
	std::memcpy( out.surfaces, in.surfaces, sizeof( out.surfaces ) );
	out.shaderData[0] = in.shader_data[0];
	out.shaderData[1] = in.shader_data[1];
	out.bones = nullptr;
	/* unmeasured until a view draws it: whole */
	out.lod = -1.0f;
}

/* Slot 88. The parent's number rides beside the entity: it is the one the
 * entity's pose hangs from, which the renderer's own TIKI path reads. */
void QDECL AddRefEntityToScene( const mohaa::RefEntity *entity, int parentEntityNumber ) {
	(void)parentEntityNumber;
	if ( !entity ) {
		return;
	}
	if ( entity->reType < 0 || entity->reType >= mohaa::kRtMax ) {
		Com_Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", entity->reType );
		return;
	}
	if ( sceneEntities.size() < kMaxSceneEntities ) {
		sceneEntities.push_back( *entity );
	}
	if ( entity->renderfx & kRfDontDraw ) {
		return;
	}

	refEntity_t ent;
	BaseEntity( *entity, ent );
	switch ( entity->reType ) {
	case mohaa::kRtModel: {
		const Model *model = ModelFor( entity->hModel );
		if ( !model || !model->renderer ) {
			return;
		}
		ent.reType = RT_MODEL;
		ent.hModel = model->renderer;
		if ( model->tiki ) {
			TikiEntity( *entity, model->tiki, ent.mohaa );
		}
		break;
	}
	case mohaa::kRtSprite:
		ent.reType = RT_SPRITE;
		ent.customShader = entity->customShader;
		break;
	case mohaa::kRtBeam:
		ent.reType = RT_BEAM;
		break;
	case mohaa::kRtRailCore:
		ent.reType = RT_RAIL_CORE;
		break;
	case mohaa::kRtRailRings:
		ent.reType = RT_RAIL_RINGS;
		break;
	case mohaa::kRtLightning:
		ent.reType = RT_LIGHTNING;
		break;
	case mohaa::kRtPortalSurface:
		ent.reType = RT_PORTALSURFACE;
		break;
	default:
		return;
	}
	re.AddRefEntityToScene( &ent, qfalse );
}

/* Slot 89: a sprite, whose model names its shader. It is drawn at the
 * entity's origin, as large as the shader's image times the shader's
 * `spritescale` and the entity's scale, turned by the shader's `spritegen`
 * (code/renderer/tr_alice.c), and tinted by the entity's colour. */
void QDECL AddRefSpriteToScene( const mohaa::RefEntity *entity ) {
	if ( !entity || ( entity->renderfx & kRfDontDraw ) ) {
		return;
	}
	const Model *model = ModelFor( entity->hModel );
	if ( !model || !model->renderer ) {
		return;
	}
	refEntity_t ent;
	BaseEntity( *entity, ent );
	ent.reType = RT_ALICE_SPRITE;
	ent.hModel = model->renderer;
	ent.mohaa.scale = entity->scale;
	re.AddRefEntityToScene( &ent, qfalse );
}

mohaa::RefEntity *QDECL GetRenderEntity( int entityNumber ) {
	for ( mohaa::RefEntity &entity : sceneEntities ) {
		if ( entity.entityNumber == entityNumber ) {
			return &entity;
		}
	}
	return nullptr;
}

/* Slot 90: 2015's type bits name a lens flare (1), a view lens flare (2)
 * and an additive light (4), which the renderers read as Alice's light's
 * (RE_AddAliceLightToScene). */
void QDECL AddLightToScene( const vec3_t origin, float intensity, float r, float g, float b, int type ) {
	if ( !origin ) {
		return;
	}
	re.AddAliceLightToScene( origin, intensity, r, g, b, type );
}

qboolean QDECL AddPolyToScene( qhandle_t shader, int numVerts, const mohaa::PolyVert *verts, int renderfx ) {
	(void)renderfx;
	static_assert( sizeof( mohaa::PolyVert ) == sizeof( polyVert_t ), "2015's polygon vertex is this engine's" );
	if ( !verts || numVerts < 3 || !shader ) {
		return qfalse;
	}
	re.AddPolyToScene( shader, numVerts, reinterpret_cast<const polyVert_t *>( verts ), 1 );
	return qtrue;
}

void QDECL AddTerrainMarkToScene( int terrainIndex, qhandle_t shader, int numVerts, const mohaa::PolyVert *verts,
	int renderfx ) {
	(void)terrainIndex;
	AddPolyToScene( shader, numVerts, verts, renderfx );
}

/* Slot 76: 1.00's view has no deform text; its far plane and sky portal
 * are not drawn yet. */
void QDECL RenderScene( const mohaa::Refdef *fd ) {
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
	if ( !( view.rdflags & RDF_NOWORLDMODEL ) ) {
		static_assert( sizeof( fd->areamask ) <= sizeof( sceneAreamask ), "scene area mask" );
		std::memset( sceneAreamask, 0, sizeof( sceneAreamask ) );
		std::memcpy( sceneAreamask, fd->areamask, sizeof( fd->areamask ) );
		sceneAreamaskSet = true;
	}
	/* The far plane and sky portal are the Ritual view's, which the
	 * renderers draw for this profile too (R_GAME_HAS_RITUAL_VIEW). */
	aliceRenderView_t &ritual = view.alice;
	ritual.farplaneDistance = fd->farplane_distance;
	VectorCopy( fd->farplane_color, ritual.farplaneColor );
	ritual.farplaneCull = fd->farplane_cull ? qtrue : qfalse;
	ritual.skyPortal = fd->sky_portal ? qtrue : qfalse;
	ritual.skyAlpha = fd->sky_alpha;
	VectorCopy( fd->sky_origin, ritual.skyOrigin );
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( fd->sky_axis[i], ritual.skyAxis[i] );
	}
	/* 2015's renderer ran its level-of-detail tool and decal editor as it
	 * drew the world (OpenMoHAA RE_RenderScene, reference only) */
	if ( !( view.rdflags & RDF_NOWORLDMODEL ) ) {
		MOHAA_LodToolFrame( view.vieworg, view.viewaxis, view.fov_x );
		CL_MOHAADecalsFrame( &view );
	}
	re.RenderScene( &view );
}

void QDECL LoadWorldMap( const char *name ) {
	if ( name ) {
		/* the decal editor takes the level's decals before the renderers
		 * would build them (cl_mohaa_decals.cpp) */
		CL_MOHAADecalsBeginLevel( name );
	sceneAreamaskSet = false;
		re.LoadWorld( name );
	}
}

void QDECL RendererPrintBSPFileSizes() {
}

/* Slot 100: Quake III's four-dimensional noise, which its renderer builds
 * its wave tables from and 2015's client game samples for its effects. */
float QDECL Noise( float x, float y, float z, float t ) {
	/* A small value-noise lattice, deterministic in its inputs. */
	const auto hash = []( int a, int b, int c, int d ) {
		std::uint32_t h = static_cast<std::uint32_t>( a ) * 73856093u ^ static_cast<std::uint32_t>( b ) * 19349663u
			^ static_cast<std::uint32_t>( c ) * 83492791u ^ static_cast<std::uint32_t>( d ) * 2654435761u;
		h ^= h >> 13;
		h *= 0x5bd1e995u;
		h ^= h >> 15;
		return static_cast<float>( h & 0xffff ) / 32767.5f - 1.0f;
	};
	const int ix = static_cast<int>( std::floor( x ) ), iy = static_cast<int>( std::floor( y ) );
	const int iz = static_cast<int>( std::floor( z ) ), it = static_cast<int>( std::floor( t ) );
	const float fx = x - ix, fy = y - iy, fz = z - iz, ft = t - it;
	float value = 0.0f;
	for ( int corner = 0; corner < 16; corner++ ) {
		const int dx = corner & 1, dy = ( corner >> 1 ) & 1, dz = ( corner >> 2 ) & 1, dt = ( corner >> 3 ) & 1;
		const float weight = ( dx ? fx : 1.0f - fx ) * ( dy ? fy : 1.0f - fy ) * ( dz ? fz : 1.0f - fz )
			* ( dt ? ft : 1.0f - ft );
		value += weight * hash( ix + dx, iy + dy, iz + dz, it + dt );
	}
	return value;
}

void QDECL DebugLine( const vec3_t start, const vec3_t end, float r, float g, float b, float alpha ) {
	(void)start; (void)end; (void)r; (void)g; (void)b; (void)alpha;
}

/* Slot 102: a map shader's name and flags by number. The collision model's
 * traces name none (see ToModuleTrace), so the first is all there is. */
mohaa::BaseShader shaderRecord;

mohaa::BaseShader *QDECL GetShader( int shaderNum ) {
	(void)shaderNum;
	shaderRecord = mohaa::BaseShader{};
	return &shaderRecord;
}

/* Slots 103..105: a swipe, the ribbon a TIKI `swipe` or `trail` leaves
 * behind between two tags. The client game opens one each frame at its time
 * with the swipe's life and shader, hands its (p1, p2) pairs oldest first,
 * each with the time it was taken, and closes it; a negative shader, and no
 * life, clears. The strip between successive pairs fades out, colour and
 * alpha alike, over the life (OpenMoHAA's RB_DrawSwipeSurface [O]), and is
 * drawn through the scene's polygons, which every renderer takes. The
 * texture runs along the strip from its oldest pair, p1 its top edge and p2
 * its bottom [I]: OpenMoHAA gives both edges the same row. Retail 1.00's
 * data names one swipe, in a TIKI nothing uses. */
constexpr std::size_t kMaxSwipePoints = 1024;

struct SwipePointRecord {
	vec3_t p1, p2;
	float time;
};

struct SwipeState {
	bool open = false;
	float time = 0.0f;
	float life = 0.0f;
	qhandle_t shader = 0;
	std::vector<SwipePointRecord> points;
} swipe;

void QDECL SwipeBegin( float thistime, float life, qhandle_t shader ) {
	swipe.points.clear();
	swipe.open = shader > 0 && life > 0.0f;
	swipe.time = thistime;
	swipe.life = life;
	swipe.shader = shader;
}

void QDECL SwipePoint( vec3_t p1, vec3_t p2, float time ) {
	if ( !swipe.open || !p1 || !p2 || swipe.points.size() >= kMaxSwipePoints ) {
		return;
	}
	SwipePointRecord point;
	VectorCopy( p1, point.p1 );
	VectorCopy( p2, point.p2 );
	point.time = time;
	swipe.points.push_back( point );
}

void QDECL SwipeEnd() {
	if ( !swipe.open ) {
		return;
	}
	swipe.open = false;
	const std::size_t count = swipe.points.size();
	if ( count < 2 ) {
		return;
	}
	const auto fade = []( float time ) {
		const float f = 1.0f - ( swipe.time - time ) / swipe.life;
		return f < 0.0f ? 0.0f : ( f > 1.0f ? 1.0f : f );
	};
	const auto vertex = [&]( polyVert_t &out, const vec3_t xyz, float s, float t, float alpha ) {
		VectorCopy( xyz, out.xyz );
		out.st[0] = s;
		out.st[1] = t;
		const byte value = static_cast<byte>( alpha * 255.0f );
		out.modulate.rgba[0] = out.modulate.rgba[1] = out.modulate.rgba[2] = out.modulate.rgba[3] = value;
	};
	for ( std::size_t i = 0; i + 1 < count; i++ ) {
		const SwipePointRecord &a = swipe.points[i];
		const SwipePointRecord &b = swipe.points[i + 1];
		const float fa = fade( a.time ), fb = fade( b.time );
		if ( fa <= 0.0f && fb <= 0.0f ) {
			continue;
		}
		const float sa = static_cast<float>( i ) / static_cast<float>( count - 1 );
		const float sb = static_cast<float>( i + 1 ) / static_cast<float>( count - 1 );
		polyVert_t quad[4];
		vertex( quad[0], a.p1, sa, 0.0f, fa );
		vertex( quad[1], a.p2, sa, 1.0f, fa );
		vertex( quad[2], b.p2, sb, 1.0f, fb );
		vertex( quad[3], b.p1, sb, 0.0f, fb );
		re.AddPolyToScene( swipe.shader, 4, quad, 1 );
	}
}

/* ---- the client's own state (slots 109..118) ------------------------------ */

void QDECL GetGameState( mohaa::GameState *state ) {
	if ( !state ) {
		Com_Error( ERR_DROP, "Medal of Honor client game asked for the game state with nowhere to put it" );
		return;
	}
	static_assert( sizeof( state->stringOffsets ) == sizeof( cl.gameState.stringOffsets ),
		"the engine keeps 1.00's 2,736 configstrings" );
	static_assert( sizeof( state->stringData ) == sizeof( cl.gameState.stringData ),
		"the engine's string data is 1.00's 32,000 bytes" );
	std::memcpy( state->stringOffsets, cl.gameState.stringOffsets, sizeof( state->stringOffsets ) );
	std::memcpy( state->stringData, cl.gameState.stringData, sizeof( state->stringData ) );
	state->dataCount = cl.gameState.dataCount;
}

qboolean QDECL GetSnapshot( int snapshotNumber, mohaa::Snapshot *snapshot ) {
	return CL_MOHAAGetSnapshot( snapshotNumber, snapshot ) ? qtrue : qfalse;
}

int QDECL GetServerStartTime() {
	return CL_MOHAAServerStartTime();
}

void QDECL SetTime( int time ) {
	cl.serverTime = time;
}

void QDECL GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	CL_MOHAACurrentSnapshotNumber( snapshotNumber, serverTime );
}

void QDECL GetGlconfig( mohaa::GlConfig *config ) {
	if ( !config ) {
		return;
	}
	*config = mohaa::GlConfig{};
	Q_strncpyz( config->renderer_string, cls.glconfig.renderer_string, sizeof( config->renderer_string ) );
	Q_strncpyz( config->vendor_string, cls.glconfig.vendor_string, sizeof( config->vendor_string ) );
	Q_strncpyz( config->version_string, cls.glconfig.version_string, sizeof( config->version_string ) );
	Q_strncpyz( config->extensions_string, cls.glconfig.extensions_string, sizeof( config->extensions_string ) );
	config->maxTextureSize = cls.glconfig.maxTextureSize;
	config->numTextureUnits = cls.glconfig.numTextureUnits;
	config->colorBits = cls.glconfig.colorBits;
	config->depthBits = cls.glconfig.depthBits;
	config->stencilBits = cls.glconfig.stencilBits;
	config->driverType = cls.glconfig.driverType;
	config->hardwareType = cls.glconfig.hardwareType;
	config->deviceSupportsGamma = cls.glconfig.deviceSupportsGamma;
	config->textureCompression = cls.glconfig.textureCompression;
	config->textureEnvAddAvailable = cls.glconfig.textureEnvAddAvailable;
	config->vidWidth = cls.glconfig.vidWidth;
	config->vidHeight = cls.glconfig.vidHeight;
	config->windowAspect = cls.glconfig.windowAspect;
	config->displayFrequency = cls.glconfig.displayFrequency;
	config->isFullscreen = cls.glconfig.isFullscreen;
	config->stereoEnabled = cls.glconfig.stereoEnabled;
	config->smpActive = cls.glconfig.smpActive;
}

int QDECL GetCurrentCmdNumber() {
	return cl.cmdNumber;
}

qboolean QDECL GetUserCmd( int cmdNumber, mohaa::UserCmd *command ) {
	if ( !command ) {
		return qfalse;
	}
	if ( cmdNumber - cl.cmdNumber > 0 ) {
		Com_Error( ERR_DROP, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber );
		return qfalse;
	}
	if ( cl.cmdNumber - cmdNumber >= CMD_BACKUP ) {
		return qfalse;
	}
	CL_MOHAACommandFromEngine( cl.cmds[cmdNumber & CMD_MASK], *command );
	return qtrue;
}

/* Slot 118. 1.00 passes a second argument, whether the command comes from a
 * server the client has since left; the engine's reading of the command
 * does not depend on it. */
qboolean QDECL GetServerCommand( int serverCommandNumber, qboolean differentServer ) {
	(void)differentServer;
	return CL_GetServerCommandForProfile( serverCommandNumber );
}

/* ---- aliases (slots 119..127) ------------------------------------------ */

qboolean QDECL AliasAdd( const char *alias, const char *name, const char *parameters ) {
	return MOHAA_GlobalAliasAdd( alias, name, parameters );
}

qboolean QDECL AliasListAdd( void *list, const char *alias, const char *name, const char *parameters ) {
	return MOHAA_AliasListAdd( list, alias, name, parameters );
}

const char *QDECL AliasFindRandom( const char *alias, AliasListNode_t **ret ) {
	return MOHAA_GlobalAliasFindRandom( alias, ret );
}

const char *QDECL AliasListFindRandom( void *list, const char *alias, AliasListNode_t **ret ) {
	return MOHAA_AliasListFindRandom( list, alias, ret );
}

void *QDECL AliasListNew( const char *name ) {
	return MOHAA_AliasListNew( name );
}

void QDECL AliasListFindRandomRange( void *list, const char *alias, int *minIndex, int *maxIndex,
	float *totalWeight ) {
	MOHAA_AliasListFindRandomRange( list, alias, minIndex, maxIndex, totalWeight );
}

void *QDECL AliasGetGlobalList() {
	return MOHAA_AliasGetGlobalList();
}

/* ---- the engine's menus (slots 128, 129, 155..159) ----------------------- */

void QDECL ShowMenu( const char *name, qboolean force ) {
	CL_MOHAAUIShowMenu( name, force != qfalse );
}

void QDECL HideMenu( const char *name, qboolean force ) {
	CL_MOHAAUIHideMenu( name, force != qfalse );
}

void QDECL ShowScoreBoard( const char *menuName ) {
	CL_MOHAAUIShowScoreboard( menuName );
}

void QDECL HideScoreBoard() {
	CL_MOHAAUIHideScoreboard();
}

void QDECL SetScoreBoardItem( int item, const char *data1, const char *data2, const char *data3,
	const char *data4, const char *data5, const char *data6, const char *data7, const char *data8,
	const vec4_t textColor, const vec4_t backColor, qboolean isHeader ) {
	const char *const data[8] = { data1, data2, data3, data4, data5, data6, data7, data8 };
	CL_MOHAAUISetScoreboardItem( item, data, textColor, backColor, isHeader != qfalse );
}

void QDECL DeleteScoreBoardItems( int maxIndex ) {
	CL_MOHAAUIDeleteScoreboardItems( maxIndex );
}

/* Slot 159: the deathmatch chat, only in multiplayer. The client game passes
 * 300, 100 and 200 from three commands and a client's number, 1 to 64, from
 * a fourth -- messagemode, messagemode_all, messagemode_team and
 * messagemode_private in OpenMoHAA's cg_consolecmds.c, 300 being the kind
 * last opened -- and -100, -200 and minus a client's number, for one
 * message, from three more given nothing to say (say, sayteam and
 * sayprivate there); given words, those send "dmmessage <target> <words>"
 * themselves, 0 to all, -1 to the team (0x30046490..0x300466db). 2015's
 * engine opened a chat window with its own
 * history (OpenMoHAA cl_ui.cpp UI_ToggleDMConsole); fnTech3 opens its chat
 * line, whose line goes out as that dmmessage (cl_keys.cpp), the game's chat
 * landing in the deathmatch box as before. The same kind again, or 300,
 * closes it. */
int chatKind = 100;

void QDECL ToggleDMMessageConsole( int mode ) {
	int kind = mode < 0 ? -mode : mode;
	if ( kind == 300 ) {
		kind = chatKind;
	}
	if ( kind != 100 && kind != 200 && ( kind < 1 || kind > MAX_CLIENTS ) ) {
		return;
	}
	const int target = kind == 100 ? 0 : kind == 200 ? -1 : kind;
	if ( ( Key_GetCatcher() & KEYCATCH_MESSAGE ) && ( mode == 300 || target == chat_dmTarget ) ) {
		Con_CloseChat();
		return;
	}
	chatKind = kind;
	chat_dmTarget = target;
	Con_OpenChat( kind == 200 ? qtrue : qfalse );
}

/* ---- model definitions (slots 130..154, 160..162) ------------------------ */

dtiki_t *TikiOf( const mohaa::RefEntity *model ) {
	return model ? Pointer<dtiki_t>( model->tiki ) : nullptr;
}

/* The pose a render entity asks for, set on its skeleton before any tag is
 * read from it (OpenMoHAA R_UpdatePoseInternal). */
void UpdatePose( mohaa::RefEntity *model ) {
	dtiki_t *tiki = TikiOf( model );
	if ( !tiki ) {
		return;
	}
	static const int noTags[FNTECH3_MOHAA_NUM_BONE_CONTROLLERS] = { -1, -1, -1, -1, -1 };
	static const vec4_t noQuats[FNTECH3_MOHAA_NUM_BONE_CONTROLLERS] = {};
	const int *tags = model->bone_tag ? Pointer<const int>( model->bone_tag ) : noTags;
	const vec4_t *quats = model->bone_quat ? Pointer<const vec4_t>( model->bone_quat ) : noQuats;
	TIKI_SetPoseInternal( TIKI_GetSkeletor( tiki, model->entityNumber ),
		reinterpret_cast<const frameInfo_t *>( model->frameInfo ), tags, quats, model->actionWeight );
}

void QDECL CalculateBounds( dtiki_t *tiki, float scale, vec3_t mins, vec3_t maxs ) {
	if ( tiki && mins && maxs ) {
		TIKI_CalculateBounds( tiki, scale, mins, maxs );
	}
}

const char *QDECL TikiName( dtiki_t *tiki ) {
	return tiki ? TIKI_Name( tiki ) : "";
}

void QDECL SetEyeTargetPos( dtiki_t *tiki, int entityNumber, vec3_t position ) {
	if ( tiki && position ) {
		TIKI_SetEyeTargetPos( tiki, entityNumber, position );
	}
}

const char *QDECL AnimNameForNum( dtiki_t *tiki, int anim ) {
	return tiki ? TIKI_Anim_NameForNum( tiki, anim ) : nullptr;
}

int QDECL AnimNumForName( dtiki_t *tiki, const char *name ) {
	return tiki && name ? TIKI_Anim_NumForName( tiki, name ) : -1;
}

int QDECL AnimNumFrames( dtiki_t *tiki, int anim ) {
	return tiki ? TIKI_Anim_NumFrames( tiki, anim ) : 0;
}

float QDECL AnimTime( dtiki_t *tiki, int anim ) {
	return tiki ? TIKI_Anim_Time( tiki, anim ) : 0.0f;
}

float QDECL AnimFrametime( dtiki_t *tiki, int anim ) {
	return tiki ? TIKI_Anim_Frametime( tiki, anim ) : 0.0f;
}

int QDECL AnimFlags( dtiki_t *tiki, int anim ) {
	return tiki ? TIKI_Anim_Flags( tiki, anim ) : 0;
}

float QDECL AnimCrossblendTime( dtiki_t *tiki, int anim ) {
	return tiki ? TIKI_Anim_CrossblendTime( tiki, anim ) : 0.0f;
}

qboolean QDECL AnimHasCommands( dtiki_t *tiki, int anim ) {
	return tiki ? TIKI_Anim_HasClientCommands( tiki, anim ) : qfalse;
}

qboolean QDECL FrameCommands( dtiki_t *tiki, int anim, int frame, tiki_cmd_t *command ) {
	return tiki && command ? TIKI_Frame_Commands_Client( tiki, anim, frame, command ) : qfalse;
}

qboolean QDECL FrameCommandsTime( dtiki_t *tiki, int anim, float start, float end, tiki_cmd_t *command ) {
	return tiki && command ? TIKI_Frame_Commands_ClientTime( tiki, anim, start, end, command ) : qfalse;
}

int QDECL SurfaceNameToNum( dtiki_t *tiki, const char *name ) {
	return tiki && name ? TIKI_Surface_NameToNum( tiki, name ) : -1;
}

int QDECL TagNumForName( dtiki_t *tiki, const char *name ) {
	return tiki && name ? TIKI_Tag_NameToNum( tiki, name ) : -1;
}

const char *QDECL TagNameForNum( dtiki_t *tiki, int number ) {
	return tiki ? TIKI_Tag_NumToName( tiki, number ) : nullptr;
}

void QDECL ForceUpdatePose( mohaa::RefEntity *model ) {
	UpdatePose( model );
}

/* Slot 153 returns the orientation by value: 1.00's caller passes the
 * record's address first, as every x86 caller of a 48-byte return does. */
orientation_t QDECL TikiOrientation( mohaa::RefEntity *model, int tagNum ) {
	orientation_t orientation;
	std::memset( &orientation, 0, sizeof( orientation ) );
	AxisClear( orientation.axis );
	dtiki_t *tiki = TikiOf( model );
	if ( !tiki ) {
		return orientation;
	}
	UpdatePose( model );
	return TIKI_OrientationInternal( tiki, model->entityNumber, tagNum, model->scale );
}

qboolean QDECL TikiIsOnGround( mohaa::RefEntity *model, int tagNum, float threshold ) {
	dtiki_t *tiki = TikiOf( model );
	if ( !tiki ) {
		return qfalse;
	}
	UpdatePose( model );
	return TIKI_IsOnGroundInternal( tiki, model->entityNumber, tagNum, threshold );
}

dtiki_t *QDECL FindTiki( const char *path ) {
	return path ? TIKI_FindTiki( path ) : nullptr;
}

/* The client game's own share of the loading screen's progress: the sound
 * scripts it parses (cl_mohaa_ui.cpp counts them). */
void QDECL LoadResource( const char *name ) {
	MOHAA_LoadResource( name );
}

void QDECL CanonicalFilename( char *name ) {
	if ( name ) {
		FS_CanonicalFilename( name );
	}
}

/* ---- the table ------------------------------------------------------- */

cvar_t *fsDebug;

void FillImports() {
	FillUnserved( std::make_index_sequence<FNTECH3_MOHAA_CGAME_IMPORT_SLOTS>{} );

#define SERVE( slot, fn ) imports[CI_##slot] = reinterpret_cast<std::intptr_t>( &fn )
	/* Slot 0 is the interface version, which no instruction in 1.00's
	 * client game reads; OpenMoHAA's is 3. */
	imports[CI_apiversion] = 3;
	SERVE( Printf, Printf );
	SERVE( DPrintf, DPrintf );
	SERVE( DebugPrintf, DebugPrintf );
	SERVE( Malloc, Malloc );
	SERVE( Free, Free );
	SERVE( Error, Error );
	SERVE( Milliseconds, Milliseconds );
	SERVE( LV_ConvertString, LV_ConvertString );
	SERVE( Cvar_Get, CvarGet );
	SERVE( Cvar_Set, CvarSet );
	SERVE( Argc, Argc );
	SERVE( Argv, Argv );
	SERVE( Args, Args );
	SERVE( AddCommand, AddCommand );
	SERVE( Cmd_Stuff, CmdStuff );
	SERVE( Cmd_Execute, CmdExecute );
	SERVE( Cmd_TokenizeString, CmdTokenizeString );
	SERVE( FS_ReadFile, ReadFile );
	SERVE( FS_FreeFile, FreeFile );
	SERVE( FS_WriteFile, WriteFile );
	SERVE( FS_WriteTextFile, WriteTextFile );
	SERVE( MSG_ReadBits, MsgReadBits );
	SERVE( MSG_ReadChar, MsgReadChar );
	SERVE( MSG_ReadByte, MsgReadByte );
	SERVE( MSG_ReadSVC, MsgReadByte );
	SERVE( MSG_ReadShort, MsgReadShort );
	SERVE( MSG_ReadLong, MsgReadLong );
	SERVE( MSG_ReadFloat, MsgReadFloat );
	SERVE( MSG_ReadString, MsgReadString );
	SERVE( MSG_ReadStringLine, MsgReadStringLine );
	SERVE( MSG_ReadAngle8, MsgReadAngle8 );
	SERVE( MSG_ReadAngle16, MsgReadAngle16 );
	SERVE( MSG_ReadData, MsgReadData );
	SERVE( MSG_ReadCoord, MsgReadCoord );
	SERVE( MSG_ReadDir, MsgReadDir );
	SERVE( SendClientCommand, SendClientCommand );
	SERVE( CM_LoadMap, LoadMap );
	SERVE( CM_InlineModel, InlineModel );
	SERVE( CM_NumInlineModels, NumInlineModels );
	SERVE( CM_PointContents, PointContents );
	SERVE( CM_TransformedPointContents, TransformedPointContents );
	SERVE( CM_BoxTrace, BoxTrace );
	SERVE( CM_TransformedBoxTrace, TransformedBoxTrace );
	SERVE( CM_TempBoxModel, TempBoxModel );
	SERVE( CM_PrintBSPFileSizes, PrintBSPFileSizes );
	SERVE( CM_LeafInPVS, LeafInPVS );
	SERVE( CM_PointLeafnum, PointLeafnum );
	SERVE( R_MarkFragments, MarkFragments );
	SERVE( R_MarkFragmentsForInlineModel, MarkFragmentsForInlineModel );
	SERVE( R_GetInlineModelBounds, GetInlineModelBounds );
	SERVE( R_GetLightingForDecal, GetLightingForDecal );
	SERVE( R_GetLightingForSmoke, GetLightingForSmoke );
	SERVE( R_GatherLightSources, GatherLightSources );
	SERVE( S_StartSound, StartSound );
	SERVE( S_StopSound, StopSound );
	SERVE( S_ClearLoopingSounds, ClearLoopingSounds );
	SERVE( S_AddLoopingSound, AddLoopingSound );
	SERVE( S_Respatialize, Respatialize );
	SERVE( S_RegisterSound, RegisterSound );
	SERVE( S_UpdateEntity, UpdateEntity );
	SERVE( S_SetReverb, SetReverb );
	SERVE( S_ChannelNameToNum, ChannelNameToNum );
	SERVE( MUSIC_NewSoundtrack, MusicNewSoundtrack );
	SERVE( MUSIC_UpdateMood, MusicUpdateMood );
	SERVE( MUSIC_UpdateVolume, MusicUpdateVolume );
	SERVE( get_camera_offset, GetCameraOffset );
	SERVE( R_ClearScene, ClearScene );
	SERVE( R_RenderScene, RenderScene );
	SERVE( R_LoadWorldMap, LoadWorldMap );
	SERVE( R_PrintBSPFileSizes, RendererPrintBSPFileSizes );
	SERVE( R_RegisterModel, RegisterModel );
	SERVE( R_SpawnEffectModel, SpawnEffectModel );
	SERVE( R_RegisterServerModel, RegisterServerModel );
	SERVE( R_UnregisterServerModel, UnregisterServerModel );
	SERVE( R_RegisterShader, RegisterShader );
	SERVE( R_RegisterShaderNoMip, RegisterShaderNoMip );
	SERVE( R_AddRefEntityToScene, AddRefEntityToScene );
	SERVE( R_AddRefSpriteToScene, AddRefSpriteToScene );
	SERVE( R_AddLightToScene, AddLightToScene );
	SERVE( R_AddPolyToScene, AddPolyToScene );
	SERVE( R_AddTerrainMarkToScene, AddTerrainMarkToScene );
	SERVE( R_SetColor, SetColor );
	SERVE( R_DrawStretchPic, DrawStretchPic );
	SERVE( R_LoadFont, LoadFont );
	SERVE( R_DrawString, DrawString );
	SERVE( R_GetRenderEntity, GetRenderEntity );
	SERVE( R_ModelBounds, ModelBounds );
	SERVE( R_ModelRadius, ModelRadius );
	SERVE( R_Noise, Noise );
	SERVE( R_DebugLine, DebugLine );
	SERVE( GetShader, GetShader );
	SERVE( R_SwipeBegin, SwipeBegin );
	SERVE( R_SwipePoint, SwipePoint );
	SERVE( R_SwipeEnd, SwipeEnd );
	SERVE( R_GetShaderWidth, GetShaderWidth );
	SERVE( R_GetShaderHeight, GetShaderHeight );
	SERVE( R_DrawBox, DrawBox );
	SERVE( GetGameState, GetGameState );
	SERVE( GetSnapshot, GetSnapshot );
	SERVE( GetServerStartTime, GetServerStartTime );
	SERVE( SetTime, SetTime );
	SERVE( GetCurrentSnapshotNumber, GetCurrentSnapshotNumber );
	SERVE( GetGlconfig, GetGlconfig );
	SERVE( GetCurrentCmdNumber, GetCurrentCmdNumber );
	SERVE( GetUserCmd, GetUserCmd );
	SERVE( GetServerCommand, GetServerCommand );
	SERVE( Alias_Add, AliasAdd );
	SERVE( Alias_ListAdd, AliasListAdd );
	SERVE( Alias_FindRandom, AliasFindRandom );
	SERVE( Alias_ListFindRandom, AliasListFindRandom );
	SERVE( AliasList_New, AliasListNew );
	SERVE( Alias_ListFindRandomRange, AliasListFindRandomRange );
	SERVE( Alias_GetGlobalList, AliasGetGlobalList );
	SERVE( UI_ShowMenu, ShowMenu );
	SERVE( UI_HideMenu, HideMenu );
	SERVE( R_Model_GetHandle, ModelGetHandle );
	SERVE( TIKI_CalculateBounds, CalculateBounds );
	SERVE( TIKI_Name, TikiName );
	SERVE( TIKI_SetEyeTargetPos, SetEyeTargetPos );
	SERVE( Anim_NameForNum, AnimNameForNum );
	SERVE( Anim_NumForName, AnimNumForName );
	SERVE( Anim_NumFrames, AnimNumFrames );
	SERVE( Anim_Time, AnimTime );
	SERVE( Anim_Frametime, AnimFrametime );
	SERVE( Anim_Flags, AnimFlags );
	SERVE( Anim_CrossblendTime, AnimCrossblendTime );
	SERVE( Anim_HasCommands, AnimHasCommands );
	SERVE( Frame_Commands, FrameCommands );
	SERVE( Frame_CommandsTime, FrameCommandsTime );
	SERVE( Surface_NameToNum, SurfaceNameToNum );
	SERVE( Tag_NumForName, TagNumForName );
	SERVE( Tag_NameForNum, TagNameForNum );
	SERVE( ForceUpdatePose, ForceUpdatePose );
	SERVE( TIKI_Orientation, TikiOrientation );
	SERVE( TIKI_IsOnGround, TikiIsOnGround );
	SERVE( UI_ShowScoreBoard, ShowScoreBoard );
	SERVE( UI_HideScoreBoard, HideScoreBoard );
	SERVE( UI_SetScoreBoardItem, SetScoreBoardItem );
	SERVE( UI_DeleteScoreBoardItems, DeleteScoreBoardItems );
	SERVE( UI_ToggleDMMessageConsole, ToggleDMMessageConsole );
	SERVE( TIKI_FindTiki, FindTiki );
	SERVE( LoadResource, LoadResource );
	SERVE( FS_CanonicalFilename, CanonicalFilename );
#undef SERVE

	/* The four pointers to data, and 1.00's force-feedback interface, which
	 * the module tests for null before every use. */
	fsDebug = Cvar_Get( "fs_debug", "0", 0 );
	imports[CI_fsDebug] = reinterpret_cast<std::intptr_t>( fsDebug );
	imports[CI_HudDrawElements] = reinterpret_cast<std::intptr_t>( hudDrawElements.data() );
	imports[CI_anim] = reinterpret_cast<std::intptr_t>( &clientAnim );
	imports[CI_stopWatch] = reinterpret_cast<std::intptr_t>( &stopWatch );
	imports[CI_forceFeedback] = 0;
}

/* ---- the exports --------------------------------------------------------- */

typedef void ( QDECL *Init_t )( std::intptr_t *imports, int serverMessageNum, int serverCommandSequence,
	int clientNum );
typedef void ( QDECL *Void_t )();
typedef void ( QDECL *DrawActiveFrame_t )( int serverTime, int frameTime, int stereoView, int demoPlayback );
typedef qboolean ( QDECL *ConsoleCommand_t )();
typedef void ( QDECL *Vector_t )( float *out );
typedef float ( QDECL *Float_t )();
typedef int ( QDECL *Int_t )();
typedef qboolean ( QDECL *CheckCaptureKey_t )( int key, qboolean down );

/*
==================
Entry

The engine's client-game calls, routed to 1.00's exports:

  CG_INIT              TIKI's per-level state is started again unless the
                       local server already did it; then export 0 with the
                       import table, the message and command sequences and
                       the client number, and the definitions' load is
                       finished (OpenMoHAA CL_InitCGame).
  CG_SHUTDOWN          export 1; the models go with it.
  CG_CONSOLE_COMMAND   export 3.
  CG_DRAW_ACTIVE_FRAME export 2 with the server time, the frame's length,
                       the stereo field and the demo flag, then the 2D layer
                       (export 5) and the server's prints over it, once the
                       client is in the game.
==================
*/
intptr_t QDECL Entry( int command, ... ) {
	va_list ap;
	int args[3] = {};

	va_start( ap, command );
	for ( int &arg : args ) {
		arg = va_arg( ap, int );
	}
	va_end( ap );

	switch ( command ) {
	case CG_INIT: {
		const auto init = Export<Init_t>( CE_Init );
		if ( !com_sv_running->integer ) {
			TIKI_End();
			TIKI_Begin();
			TIKI_FreeAll();
			InitClientSavedData();
		}
		init( imports, args[0], args[1], args[2] );
		TIKI_FinishLoad();
		Com_Printf( "Medal of Honor client game initialized.\n" );
		return 0;
	}
	case CG_SHUTDOWN: {
		const auto shutdown = Export<Void_t>( CE_Shutdown );
		CL_MOHAADecalsShutdown();
		if ( shutdown ) {
			shutdown();
		}
		exports = nullptr;
		CL_MOHAACGameClearModels();
		CL_MOHAAMusicFreeAll();
		return 0;
	}
	case CG_CONSOLE_COMMAND: {
		const auto consoleCommand = Export<ConsoleCommand_t>( CE_ConsoleCommand );
		return consoleCommand ? consoleCommand() : 0;
	}
	case CG_DRAW_ACTIVE_FRAME: {
		if ( cls.state != CA_ACTIVE ) {
			return 0;
		}
		const auto drawActiveFrame = Export<DrawActiveFrame_t>( CE_DrawActiveFrame );
		/* A save's picture is an extra frame inside this one: the view again,
		 * no time spent, nothing over it. */
		const bool viewOnly = CL_MOHAAViewOnlyFrame();
		drawActiveFrame( args[0], viewOnly ? 0 : cls.frametime, args[1], args[2] );
		if ( viewOnly ) {
			return 0;
		}
		/* 2015's order over the view: the fade and the letterbox, the client
		 * game's 2D, the prints, then the engine's HUD and menus (OpenMoHAA
		 * View3D::Draw2D). */
		CL_MOHAADrawFades();
		CL_MOHAACGameDraw2D();
		CL_MOHAADrawPrints();
		CL_MOHAAUIDrawHud();
		return 0;
	}
	default:
		return 0;
	}
}

} // namespace

/* ---- the engine's side -------------------------------------------------- */

/* The local player's bindings, which a listen server's game reads through its
 * imports 153..155 -- a level's `getboundkey1 "+scores"` names the key its
 * hint tells the player to press (maps/m1l2b.scr; gamex86.dll 0x31051622
 * names the first key only when its number is not 0). An unbound command's
 * keys are -1 and read back as "Not Bound" (OpenMoHAA Key_GetKeysForCommand
 * and Key_KeynumToBindString, reference only). */
int CL_MOHAAKeyStringToKeynum( const char *name ) {
	return name ? Key_StringToKeynum( name ) : -1;
}

const char *CL_MOHAAKeyBindString( int keynum ) {
	if ( keynum == -1 ) {
		return "Not Bound";
	}
	if ( keynum < 0 || keynum >= MAX_KEYS ) {
		return "Out of Range";
	}
	return Key_KeynumToString( keynum );
}

void CL_MOHAAKeysForCommand( const char *command, int *key1, int *key2 ) {
	int first = -1, second = -1;
	for ( int key = 0; command && key < MAX_KEYS; key++ ) {
		const char *binding = Key_GetBinding( key );
		if ( !binding || Q_stricmp( binding, command ) ) {
			continue;
		}
		if ( first < 0 ) {
			first = key;
		} else {
			second = key;
			break;
		}
	}
	if ( key1 ) *key1 = first;
	if ( key2 ) *key2 = second;
}

vm_t *CL_MOHAALoadCGame( void ) {
	MOHAA_LoadSystem86();

	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_CGAME, "GetCGameAPI", Entry, &symbol );
	if ( !vm ) {
		return nullptr;
	}

	typedef const std::intptr_t *( QDECL *GetCGameAPI_t )();
	const std::intptr_t *table = reinterpret_cast<GetCGameAPI_t>( symbol )();
	if ( !table ) {
		VM_Free( vm );
		Com_Error( ERR_DROP, "Medal of Honor client game published no export table" );
		return nullptr;
	}
	for ( int i = 0; i < FNTECH3_MOHAA_CGAME_EXPORT_SLOTS; i++ ) {
		/* 1.00 writes its profiling pointer as null; every other slot is a
		 * function. */
		if ( i != CE_profStruct && !table[i] ) {
			VM_Free( vm );
			Com_Error( ERR_DROP, "Medal of Honor client game left export %i empty", i );
			return nullptr;
		}
	}

	exports = table;
	FillImports();
	CL_MOHAACGameClearModels();
	/* the renderer's level-of-detail and decal editor cvars, which 2015's
	 * renderer registered and the engine keeps here */
	MOHAA_LodRegister();
	CL_MOHAADecalsInit();
	Com_Printf( "Medal of Honor client game loaded: %i of %i imports are served\n", ServedImports(),
		FNTECH3_MOHAA_CGAME_IMPORT_SLOTS );
	return vm;
}

void CL_MOHAACGameParseMessage( void ) {
	const auto parse = Export<Void_t>( CE_ParseCGMessage );
	if ( parse ) {
		parse();
	}
}

/* Exports 16 to 18, which 1.00's decal editor called: fourteen, five and six
 * arguments (0x30026dbe, 0x300275d6 and 0x30028061 read [ebp+8] to [ebp+0x3c],
 * [ebp+0x18] and [ebp+0x1c]). The permanent mark writes its trace's end and
 * plane normal back through its first two (0x30026e54..0x30026e85). */
typedef int ( QDECL *PermanentMark_t )( float *origin, float *direction, float orientation, float sScale,
	float tScale, float red, float green, float blue, float alpha, qboolean doLighting, float sCenter,
	float tCenter, mohaa::MarkFragment *fragments, mohaa::PolyVert *points );
typedef int ( QDECL *PermanentTreadMarkDecal_t )( mohaa::TreadMark *tread, qboolean startSegment,
	qboolean doLighting, mohaa::MarkFragment *fragments, mohaa::PolyVert *points );
typedef int ( QDECL *PermanentUpdateTreadMark_t )( mohaa::TreadMark *tread, float alpha, float minSegment,
	float maxSegment, float maxOffset, float texScale );

int CL_MOHAACGamePermanentMark( vec3_t origin, vec3_t direction, float orientation, float sScale, float tScale,
	const vec4_t color, bool doLighting, mohaa::MarkFragment *fragments, mohaa::PolyVert *points ) {
	const auto mark = Export<PermanentMark_t>( CE_PermanentMark );
	if ( !mark || !origin || !direction || !color || !fragments || !points ) {
		return 0;
	}
	/* the decal centred on the point, as 2015's editor asks */
	return mark( origin, direction, orientation, sScale, tScale, color[0], color[1], color[2], color[3],
		doLighting ? qtrue : qfalse, 0.5f, 0.5f, fragments, points );
}

int CL_MOHAACGamePermanentTreadMarkDecal( mohaa::TreadMark *tread, bool startSegment, bool doLighting,
	mohaa::MarkFragment *fragments, mohaa::PolyVert *points ) {
	const auto decal = Export<PermanentTreadMarkDecal_t>( CE_PermanentTreadMarkDecal );
	if ( !decal || !tread || !fragments || !points ) {
		return 0;
	}
	return decal( tread, startSegment ? qtrue : qfalse, doLighting ? qtrue : qfalse, fragments, points );
}

int CL_MOHAACGamePermanentUpdateTreadMark( mohaa::TreadMark *tread, float alpha, float minSegment,
	float maxSegment, float maxOffset, float texScale ) {
	const auto update = Export<PermanentUpdateTreadMark_t>( CE_PermanentUpdateTreadMark );
	if ( !update || !tread ) {
		return -1;
	}
	return update( tread, alpha, minSegment, maxSegment, maxOffset, texScale );
}

void CL_MOHAACGameDraw2D( void ) {
	const auto draw2D = Export<Void_t>( CE_Draw2D );
	if ( draw2D && cls.state == CA_ACTIVE ) {
		draw2D();
	}
}

void CL_MOHAACGameEyeOffset( float out[3] ) {
	const auto eyeOffset = Export<Vector_t>( CE_EyeOffset );
	VectorClear( out );
	if ( eyeOffset ) {
		eyeOffset( out );
	}
}

void CL_MOHAACGameEyeAngles( float out[3] ) {
	const auto eyeAngles = Export<Vector_t>( CE_EyeAngles );
	VectorCopy( cl.viewangles, out );
	if ( eyeAngles ) {
		eyeAngles( out );
	}
}

/* The mouse's scale, as 2015's CL_MouseMove takes it (OpenMoHAA
 * cl_input.cpp): 1 while a menu holds the mouse or with no client game,
 * otherwise export 9's answer unless that is negative (or not a number). */
float CL_MOHAACGameSensitivityScale( void ) {
	const auto scale = Export<Float_t>( CE_SensitivityScale );
	if ( !scale || ( Key_GetCatcher() & KEYCATCH_UI ) ) {
		return 1.0f;
	}
	const float value = scale();
	return value >= 0.0f ? value : 1.0f;
}

int CL_MOHAACGameWeaponCommandButtonBits( void ) {
	const auto bits = Export<Int_t>( CE_WeaponCommandButtonBits );
	return bits ? bits() : 0;
}

qboolean CL_MOHAACGameCheckCaptureKey( int key, qboolean down ) {
	const auto check = Export<CheckCaptureKey_t>( CE_CheckCaptureKey );
	return check ? check( key, down ) : qfalse;
}

/* The scoreboard the client game describes (exports 21..25, OpenMoHAA
 * cg_public.h): where its list goes in 640x480, its colours, whether it draws
 * a header row, and each column's name and width. */
bool CL_MOHAACGameScoreboardLayout( float rect[4], float color[4], float fontColor[4], bool *drawHeader ) {
	typedef void ( QDECL *Four_t )( float *a, float *b, float *c, float *d );
	const auto position = Export<Four_t>( CE_GetScoreBoardPosition );
	const auto back = Export<Four_t>( CE_GetScoreBoardColor );
	const auto text = Export<Four_t>( CE_GetScoreBoardFontColor );
	const auto header = Export<Int_t>( CE_GetScoreBoardDrawHeader );
	if ( !position || !back || !text || !header ) {
		return false;
	}
	position( &rect[0], &rect[1], &rect[2], &rect[3] );
	back( &color[0], &color[1], &color[2], &color[3] );
	text( &fontColor[0], &fontColor[1], &fontColor[2], &fontColor[3] );
	*drawHeader = header() != 0;
	return true;
}

/* The game module's HUD elements on a local server (its slots 158..165): the
 * local client's own array, which the client game draws. A shader or a font
 * is handed to the client game to register (exports 12 and 13), as
 * OpenMoHAA's SV_HudDraw* do; a string or a shader clears the other. */
namespace {

mohaa::HudDrawElement *HudElement( int info ) {
	return info >= 0 && info < FNTECH3_MOHAA_MAX_HUDDRAW_ELEMENTS ? &hudDrawElements[static_cast<std::size_t>( info )]
		: nullptr;
}

} // namespace

void CL_MOHAAHudDrawShader( int info, const char *name ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( !element ) return;
	Q_strncpyz( element->shaderName, name ? name : "", sizeof( element->shaderName ) );
	element->string[0] = '\0';
	element->pFont = 0;
	element->fontName[0] = '\0';
	typedef void ( QDECL *Info_t )( int info );
	const auto registerShader = Export<Info_t>( CE_HudDrawShader );
	if ( registerShader ) registerShader( info );
}

void CL_MOHAAHudDrawAlign( int info, int horizontal, int vertical ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( !element ) return;
	element->iHorizontalAlign = horizontal;
	element->iVerticalAlign = vertical;
}

void CL_MOHAAHudDrawRect( int info, int x, int y, int width, int height ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( !element ) return;
	element->iX = x;
	element->iY = y;
	element->iWidth = width;
	element->iHeight = height;
}

void CL_MOHAAHudDrawVirtualSize( int info, bool virtualScreen ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( element ) element->bVirtualScreen = virtualScreen ? 1 : 0;
}

void CL_MOHAAHudDrawColor( int info, const float *color ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( element && color ) VectorCopy( color, element->vColor );
}

void CL_MOHAAHudDrawAlpha( int info, float alpha ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( element ) element->vColor[3] = alpha;
}

void CL_MOHAAHudDrawString( int info, const char *string ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( !element ) return;
	element->hShader = 0;
	Q_strncpyz( element->string, string ? string : "", sizeof( element->string ) );
}

void CL_MOHAAHudDrawFont( int info, const char *name ) {
	mohaa::HudDrawElement *element = HudElement( info );
	if ( !element ) return;
	Q_strncpyz( element->fontName, name ? name : "", sizeof( element->fontName ) );
	element->hShader = 0;
	element->shaderName[0] = '\0';
	typedef void ( QDECL *Info_t )( int info );
	const auto registerFont = Export<Info_t>( CE_HudDrawFont );
	if ( registerFont ) registerFont( info );
}

/* The client state a savegame carries (server_mohaa_save.hpp): the HUD
 * elements, the view model's animation and the stopwatch; the HUD elements'
 * pictures and fonts are registered again once they are read back (export
 * 11). */
mohaa::HudDrawElement *CL_MOHAAHudElements( int *count ) {
	if ( count ) *count = FNTECH3_MOHAA_MAX_HUDDRAW_ELEMENTS;
	return hudDrawElements.data();
}

mohaa::ClientAnim *CL_MOHAAClientAnim( void ) {
	return &clientAnim;
}

mohaa::StopWatch *CL_MOHAAStopWatch( void ) {
	return &stopWatch;
}

void CL_MOHAARefreshHudElements( void ) {
	const auto refresh = Export<Void_t>( CE_RefreshHudDrawElements );
	if ( refresh ) {
		refresh();
	}
}

/* How far the objectives overlay is up (export 15), 0 without it. */
float CL_MOHAACGameObjectiveAlpha( void ) {
	const auto alpha = Export<Float_t>( CE_GetObjectiveAlpha );
	return alpha ? alpha() : 0.0f;
}

const char *CL_MOHAACGameScoreboardColumn( int column, int *width ) {
	typedef const char *( QDECL *ColumnName_t )( int column, int *width );
	const auto name = Export<ColumnName_t>( CE_GetColumnName );
	return name ? name( column, width ) : nullptr;
}

/* The fonts, for the engine's menus (cl_mohaa_ui.cpp): the same cache and
 * drawing the client game's R_LoadFont and R_DrawString use. */
void *CL_MOHAAFontLoad( const char *name ) {
	return LoadFont( name );
}

void CL_MOHAAFontDraw( void *font, const char *text, float x, float y, int maxLength, const float *scale ) {
	DrawStringScaled( font, text, x, y, maxLength, scale ? scale[0] : 1.0f, scale ? scale[1] : 1.0f );
}

int CL_MOHAAFontWidth( void *font, const char *text, int maxLength ) {
	return font ? ritualfont::StringWidth( &static_cast<Font *>( font )->metrics, text, maxLength ) : 0;
}

int CL_MOHAAFontHeight( void *font ) {
	return ritualfont::LineHeight( font ? &static_cast<Font *>( font )->metrics : nullptr );
}

/*
====================
CL_MOHAADrawViewPrint

The server's centre and located prints, as 1.00's full-screen view widget
draws them in its font, facfont-20 (OpenMoHAA client/cl_uiview3d.cpp
View3D::CenterPrint and LocationPrint, uilib/uifont.cpp PrintJustified): white
over a black copy one pixel down and right, each line justified on its own. A
centre print is centred on the screen; a located one is left and top aligned at
its point of the 640 by 480 screen measured up from the bottom, an axis given
as -1 centring it. A centre print naming `@material` draws that material
centred instead.
====================
*/
void CL_MOHAADrawViewPrint( const char *text, bool located, int x, int y, float alpha ) {
	if ( !text || !text[0] ) {
		return;
	}
	const float screenWidth = static_cast<float>( cls.glconfig.vidWidth );
	const float screenHeight = static_cast<float>( cls.glconfig.vidHeight );
	if ( !located && text[0] == '@' ) {
		const qhandle_t material = re.RegisterShaderNoMip( text + 1 );
		int width = 0, height = 0;
		if ( !material || !re.GetShaderImageSize ) {
			return;
		}
		re.GetShaderImageSize( material, &width, &height );
		const vec4_t color = { alpha, alpha, alpha, alpha };
		re.SetColor( color );
		re.DrawStretchPic( ( screenWidth - width ) * 0.5f, ( screenHeight - height ) * 0.5f,
			static_cast<float>( width ), static_cast<float>( height ), 0.0f, 0.0f, 1.0f, 1.0f, material );
		re.SetColor( nullptr );
		return;
	}
	Font *font = static_cast<Font *>( LoadFont( "facfont-20" ) );
	if ( !font ) {
		return;
	}
	const char *p = LV_ConvertString( text );
	const int lineHeight = static_cast<int>( font->metrics.height );
	int horizontal = 0, vertical = 1;	// centre, centre
	float frameX = 0.0f, frameY = 0.0f;
	if ( located ) {
		horizontal = 1;					// left
		vertical = 0;					// top
		frameX = x / 640.0f * screenWidth;
		frameY = ( 480 - lineHeight - y ) / 480.0f * screenHeight;
		if ( x == -1 ) {
			horizontal = 0;
			frameX = 0.0f;
		}
		if ( y == -1 ) {
			vertical = 1;
			frameY = 0.0f;
		}
	}
	int lines = 1;
	for ( const char *c = p; *c; c++ ) {
		if ( *c == '\n' ) {
			lines++;
		}
	}
	const int textHeight = lineHeight * lines;
	float lineY = vertical == 1 ? frameY + ( screenHeight - textHeight ) * 0.5f
		: vertical == 2 ? frameY + screenHeight - textHeight : frameY;
	const float colors[2][4] = { { 0.0f, 0.0f, 0.0f, alpha }, { 1.0f, 1.0f, 1.0f, alpha } };
	for ( const char *line = p; *line; ) {
		while ( *line == '\n' ) {
			line++;
			lineY += lineHeight;
		}
		if ( !*line ) {
			break;
		}
		char buffer[FNTECH3_MOHAA_MAX_STRING_CHARS];
		int length = 0;
		while ( line[length] && line[length] != '\n' && length < static_cast<int>( sizeof( buffer ) ) - 1 ) {
			buffer[length] = line[length];
			length++;
		}
		buffer[length] = '\0';
		const int textWidth = ritualfont::StringWidth( &font->metrics, buffer );
		const float lineX = horizontal == 0 ? frameX + ( screenWidth - textWidth ) * 0.5f
			: horizontal == 2 ? frameX + screenWidth - textWidth : frameX;
		for ( int pass = 0; pass < 2; pass++ ) {
			re.SetColor( colors[pass] );
			DrawStringScaled( font, buffer, lineX + ( pass ? 0.0f : 1.0f ), lineY + ( pass ? 0.0f : 1.0f ), -1, 1.0f, 1.0f );
		}
		line += length;
		lineY += lineHeight;
	}
	re.SetColor( nullptr );
}

void CL_MOHAACGameRendererRestarted( void ) {
	fontEpoch++;
	const auto getRendererConfig = Export<Void_t>( CE_GetRendererConfig );
	if ( getRendererConfig ) {
		getRendererConfig();
	}
	const auto refresh = Export<Void_t>( CE_RefreshHudDrawElements );
	if ( refresh ) {
		refresh();
	}
}

void CL_MOHAACGameClearModels( void ) {
	models.clear();
	models.resize( 1 );
	sceneEntities.clear();
	/* Nor is any sound it registered or started waited on any more. */
	soundNames.clear();
	CL_MOHAASoundsClear();
	/* A swipe left open names a shader the renderer has dropped. */
	swipe = SwipeState();
	/* The fonts stay: the engine's menus hold them for the process, as
	 * retail's font cache does. Their pictures are registered again. */
	fontEpoch++;
}

/* A model for the engine's menus (a label's `rendermodel`), registered as
 * the client game's are, with no init commands run: its renderer handle and
 * its definition, the definition null for a model that is not a TIKI. */
bool CL_MOHAAUIModel( const char *name, qhandle_t *renderer, void **tiki ) {
	const qhandle_t handle = name && name[0] ? RegisterModelInternal( name, false, false ) : 0;
	const Model *model = ModelFor( handle );
	if ( !model || !model->renderer ) {
		return false;
	}
	*renderer = model->renderer;
	*tiki = model->tiki;
	return true;
}
