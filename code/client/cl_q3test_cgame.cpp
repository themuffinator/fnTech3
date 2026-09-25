/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* The Q3Test releases' client games.
 *
 * 1.05 to 1.07 export GetCGameAPI, which takes no argument and returns five
 * function pointers; the engine hands its import table to the first. 1.05's
 * table has 64 words and pushes configstrings and server commands into the
 * module; 1.06 and 1.07 share a 57-word table and let the module pull them.
 * 1.08 is dllEntry and vmMain with syscall numbers, the 1.07 slots less the
 * first and with one inserted. Every record crossing the boundary keeps the
 * release's own layout: snapshots, the gamestate, glconfig, the 108- or
 * 368-byte refdef, 140-byte entities, 56-byte traces and the usercmds.
 * Presentation is this engine's (AGENTS.md invariant 11): the procedural
 * beam, rail and lightning models become its own entity types, and marks
 * come from its renderer. Facts and addresses: docs/fntech3/Q3TEST.md. */
extern "C" {
#include "client.h"
}
#include "client_q3test.h"
#include "audio/snd_public.h"
#include <array>
#include <cstdarg>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace q3wire = fntech3::q3test::wire;
namespace q3cl = fntech3::q3test::client;

static_assert( sizeof( trace_t ) == 56, "Q3Test's trace_t is Quake III's" );
static_assert( offsetof( refEntity_t, rotation ) + sizeof( float ) == 140, "Q3Test's refEntity_t is Quake III's first 140 bytes" );
static_assert( offsetof( refdef_t, text ) + sizeof( ( (refdef_t *)nullptr )->text ) == 368, "Q3Test's refdef_t is Quake III's first 368 bytes" );
static_assert( sizeof( polyVert_t ) == 24, "Q3Test's polyVert_t" );

namespace {

enum class Q3TestCGameStyle { None, Table105, Table106, Syscalls108 };

// The procedural models 1.05 to 1.07 register by name (1.05 R_ModelInit
// 0x43e710, 1.06 0x436b30). Their handles here are past any renderer's.
enum : qhandle_t { Q3TEST_MODEL_BEAM = 0x7ff0, Q3TEST_MODEL_RAIL, Q3TEST_MODEL_RAIL_UNDERWATER, Q3TEST_MODEL_LIGHTNING };

// CM_LoadModel's MD3 records (1.05 0x40c890): clip handles 0x100 + k.
constexpr int Q3TEST_MD3_HANDLE = 0x100;
struct Q3TestMd3 {
	std::string name;
	int numFrames = 0, numTags = 0;
	std::vector<float> bounds;   // numFrames x 6
	std::vector<std::uint8_t> tags;  // numFrames x numTags x 112
};

struct Q3TestCGame {
	Q3TestCGameStyle style = Q3TestCGameStyle::None;
	const std::intptr_t *exports = nullptr;
	intptr_t ( QDECL *vmMain )( int, ... ) = nullptr;
	std::array<std::intptr_t, 64> imports{};
	int loopCount = 0;
};
Q3TestCGame q3cg;
/* The collision code's MD3 records, shared by the client game and the menus
 * and kept across client games, as retail's cache is (5.3). */
std::vector<Q3TestMd3> q3md3s;

int Q3TestProtocol() { return q3cl::Protocol(); }
const q3testRelease_t &Q3TestRelease() { return *Com_Q3TestRelease(); }

/* Q3Test's colour escape is 0x1B and a digit, not '^' (1.05 0x408030). */
std::string Q3TestText( const char *text ) {
	std::string out = text ? text : "";
	for ( std::size_t i = 0; i + 1 < out.size(); ++i ) {
		if ( out[i] == 0x1b && out[i + 1] >= '0' && out[i + 1] <= '7' ) out[i] = Q_COLOR_ESCAPE;
	}
	return out;
}

/* ---- common services --------------------------------------------------- */

void QDECL Q3CG_Print( const char *text ) { Com_Printf( "%s", Q3TestText( text ).c_str() ); }
void QDECL Q3CG_Error( const char *text ) { Com_Error( ERR_DROP, "%s", Q3TestText( text ).c_str() ); }
int QDECL Q3CG_Milliseconds() { return Sys_Milliseconds(); }

/* The cgame's flag words mean different things per release (5.12): 1.05
 * 0x40 cheat, 0x80 read only; 1.06 no cheat bit, but any cvar without one
 * of 0x15f is cheat protected; 1.07 and 1.08 0x200 cheat, 0x40 read only. */
int Q3TestCvarFlags( int flags ) {
	int out = 0;
	if ( flags & 0x001 ) out |= CVAR_ARCHIVE;
	if ( flags & 0x010 ) out |= CVAR_INIT;
	if ( flags & 0x020 ) out |= CVAR_LATCH;
	switch ( Q3TestRelease().id ) {
	case Q3TEST_RELEASE_1_05:
		if ( flags & 0x004 ) out |= CVAR_USERINFO;
		if ( flags & 0x008 ) out |= CVAR_SERVERINFO;
		if ( flags & 0x040 ) out |= CVAR_CHEAT;
		if ( flags & 0x080 ) out |= CVAR_ROM;
		break;
	case Q3TEST_RELEASE_1_06:
		if ( flags & 0x002 ) out |= CVAR_USERINFO;
		if ( flags & 0x004 ) out |= CVAR_SERVERINFO;
		if ( flags & 0x040 ) out |= CVAR_ROM;
		if ( flags & 0x100 ) out |= CVAR_TEMP;
		if ( !( flags & 0x15f ) ) out |= CVAR_CHEAT;
		break;
	default:
		if ( flags & 0x002 ) out |= CVAR_USERINFO;
		if ( flags & 0x004 ) out |= CVAR_SERVERINFO;
		if ( flags & 0x040 ) out |= CVAR_ROM;
		if ( flags & 0x200 ) out |= CVAR_CHEAT;
		break;
	}
	return out;
}
/* The module's vmCvar_t is Quake III's 272 bytes, without the flags word this
 * engine's record carries; it is copied through an engine record. */
struct Q3TestVmCvar {
	std::int32_t handle, modificationCount;
	float value;
	std::int32_t integer;
	char string[256];
};
static_assert( sizeof( Q3TestVmCvar ) == 272, "Q3Test's vmCvar_t" );
void Q3TestCvarOut( void *native, const vmCvar_t &engine ) {
	auto *out = static_cast<Q3TestVmCvar *>( native );
	out->handle = engine.handle;
	out->modificationCount = engine.modificationCount;
	out->value = engine.value;
	out->integer = engine.integer;
	Q_strncpyz( out->string, engine.string, sizeof( out->string ) );
}
void QDECL Q3CG_CvarRegister( void *native, const char *name, const char *value, int flags ) {
	vmCvar_t engine{};
	Cvar_Register( native ? &engine : nullptr, name, value, Q3TestCvarFlags( flags ), 0 );
	if ( native ) Q3TestCvarOut( native, engine );
}
void QDECL Q3CG_CvarUpdate( void *native ) {
	if ( !native ) return;
	const auto *in = static_cast<const Q3TestVmCvar *>( native );
	vmCvar_t engine{};
	engine.handle = in->handle;
	engine.modificationCount = in->modificationCount;
	engine.value = in->value;
	engine.integer = in->integer;
	Q_strncpyz( engine.string, in->string, sizeof( engine.string ) );
	Cvar_Update( &engine, 0 );
	Q3TestCvarOut( native, engine );
}
void QDECL Q3CG_CvarSet( const char *name, const char *value ) { Cvar_Set( name, value ); }
int QDECL Q3CG_Argc() { return Cmd_Argc(); }
void QDECL Q3CG_Argv( int n, char *buffer, int length ) { Cmd_ArgvBuffer( n, buffer, length ); }
void QDECL Q3CG_Args( char *buffer, int length ) { Cmd_ArgsBuffer( buffer, length ); }

/* The module's files are the engine's, owned by the client game so that its
 * shutdown closes what it left open. 1.05 opens for reading only and numbers
 * handles from 0 (0x412b60); 1.06 and later take a mode and keep 0 for none
 * (0x4139d0). */
int QDECL Q3CG_FOpen105( const char *qpath, fileHandle_t *f ) {
	fileHandle_t engine = FS_INVALID_HANDLE;
	const int length = FS_VM_OpenFile( qpath, &engine, FS_READ, H_CGAME );
	if ( f ) *f = engine ? engine - 1 : 0;
	return length;
}
int QDECL Q3CG_Read105( void *buffer, int length, fileHandle_t f ) { return FS_VM_ReadFile( buffer, length, f + 1, H_CGAME ); }
fileHandle_t QDECL Q3CG_FOpenWrite105( const char *qpath ) {
	fileHandle_t engine = FS_INVALID_HANDLE;
	FS_VM_OpenFile( qpath, &engine, FS_WRITE, H_CGAME );
	return engine ? engine - 1 : 0;
}
int QDECL Q3CG_Write105( const void *buffer, int length, fileHandle_t f ) {
	return FS_VM_WriteFile( const_cast<void *>( buffer ), length, f + 1, H_CGAME );
}
void QDECL Q3CG_FClose105( fileHandle_t f ) { FS_VM_CloseFile( f + 1, H_CGAME ); }
int QDECL Q3CG_FOpen( const char *qpath, fileHandle_t *f, int mode ) {
	if ( mode < FS_READ || mode > FS_APPEND ) Com_Error( ERR_DROP, "FSH_FOpenFile: bad mode" );
	return FS_VM_OpenFile( qpath, f, static_cast<fsMode_t>( mode ), H_CGAME );
}
int QDECL Q3CG_Read( void *buffer, int length, fileHandle_t f ) { return FS_VM_ReadFile( buffer, length, f, H_CGAME ); }
int QDECL Q3CG_Write( const void *buffer, int length, fileHandle_t f ) {
	return FS_VM_WriteFile( const_cast<void *>( buffer ), length, f, H_CGAME );
}
void QDECL Q3CG_FClose( fileHandle_t f ) { FS_VM_CloseFile( f, H_CGAME ); }
void QDECL Q3CG_AddText( const char *text ) { Cbuf_AddText( text ); }
void QDECL Q3CG_AddCommand( const char *name ) { Cmd_AddCommand( name, nullptr ); }
void QDECL Q3CG_ClientCommand( const char *text ) { CL_AddReliableCommand( text, qfalse ); }
void QDECL Q3CG_UpdateScreen() { SCR_UpdateScreen(); }

/* ---- collision ------------------------------------------------------------ */

void QDECL Q3CG_LoadMap105( const char *name, qboolean clientload, int *checksum ) {
	int local = 0;
	CM_LoadMap( name, clientload, checksum ? checksum : &local );
}
void QDECL Q3CG_LoadMap( const char *name ) {
	int checksum = 0;
	CM_LoadMap( name, qtrue, &checksum );
}
int QDECL Q3CG_NumInlineModels() { return CM_NumInlineModels(); }
clipHandle_t QDECL Q3CG_InlineModel( int index ) { return CM_InlineModel( index ); }
clipHandle_t QDECL Q3CG_TempBoxModel( const float *mins, const float *maxs ) { return CM_TempBoxModel( mins, maxs, qfalse ); }

const Q3TestMd3 *Q3TestMd3For( clipHandle_t handle ) {
	const int index = handle - Q3TEST_MD3_HANDLE;
	return index >= 0 && index < static_cast<int>( q3md3s.size() ) ? &q3md3s[static_cast<std::size_t>( index )] : nullptr;
}
bool Q3TestIsMd3Handle( clipHandle_t handle ) { return handle >= Q3TEST_MD3_HANDLE && handle < Q3TEST_MD3_HANDLE + 1024; }

/* CM_LoadModel (1.05 0x40c890, 1.06 0x40d130): an MD3's frame bounds and
 * tags, kept by the engine for bounds and tag queries; a known name (compared
 * exactly) returns its handle, and a failure returns 0x100, the first handle. */
clipHandle_t QDECL Q3CG_LoadModel( const char *name ) {
	if ( !name ) return Q3TEST_MD3_HANDLE;
	for ( std::size_t i = 0; i < q3md3s.size(); ++i ) {
		if ( q3md3s[i].name == name ) return Q3TEST_MD3_HANDLE + static_cast<int>( i );
	}
	if ( q3md3s.size() >= 1024 ) Com_Error( ERR_DROP, "CM_LoadModel: MAX_KNOWN_MODELS" );
	const std::size_t length = strlen( name );
	if ( length < 4 || Q_stricmp( name + length - 4, ".md3" ) ) return Q3TEST_MD3_HANDLE;
	void *buffer = nullptr;
	const int size = FS_ReadFile( name, &buffer );
	if ( !buffer ) return Q3TEST_MD3_HANDLE;
	const auto *data = static_cast<const std::uint8_t *>( buffer );
	auto read = [&]( int offset ) { return static_cast<int>( q3wire::Load( data + offset, 4 ) ); };
	Q3TestMd3 md3;
	md3.name = name;
	bool ok = size >= 108 && !memcmp( data, "IDP3", 4 ) && read( 4 ) == 15;
	if ( ok ) {
		md3.numFrames = read( 76 );
		md3.numTags = read( 80 );
		const int ofsFrames = read( 92 ), ofsTags = read( 96 );
		// Each table must lie inside the file; the offsets come from the
		// file, so nothing is added to one before it is known to fit.
		ok = md3.numFrames > 0 && md3.numFrames <= 1024 && md3.numTags >= 0 && md3.numTags <= 16
			&& ofsFrames >= 0 && ofsFrames <= size && md3.numFrames * 56 <= size - ofsFrames
			&& ofsTags >= 0 && ofsTags <= size && md3.numFrames * md3.numTags * 112 <= size - ofsTags;
		if ( ok ) {
			md3.bounds.resize( static_cast<std::size_t>( md3.numFrames ) * 6 );
			for ( int f = 0; f < md3.numFrames; ++f )
				std::memcpy( &md3.bounds[static_cast<std::size_t>( f ) * 6], data + ofsFrames + f * 56, 24 );
			md3.tags.assign( data + ofsTags, data + ofsTags + md3.numFrames * md3.numTags * 112 );
		}
	}
	FS_FreeFile( buffer );
	if ( !ok ) {
		Com_Printf( "CM_LoadModel: %s is not a version 15 MD3\n", name );
		return Q3TEST_MD3_HANDLE;
	}
	q3md3s.push_back( std::move( md3 ) );
	return Q3TEST_MD3_HANDLE + static_cast<int>( q3md3s.size() - 1 );
}

void QDECL Q3CG_ModelBounds( clipHandle_t handle, float *mins, float *maxs ) {
	if ( Q3TestIsMd3Handle( handle ) ) {
		const Q3TestMd3 *md3 = Q3TestMd3For( handle );
		if ( !md3 ) Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle );
		for ( int i = 0; i < 3; ++i ) { mins[i] = md3->bounds[i]; maxs[i] = md3->bounds[3 + i]; }
		return;
	}
	CM_ModelBounds( handle, mins, maxs );
}

/* CM_LerpTag (1.05 0x40cd90, 1.06 0x40d580): the frame clamps to the last, a
 * tag missing from either frame gives the identity, and each axis is
 * normalised. 1.05 fills a 112-byte md3Tag_t from +0x40, later releases a
 * 48-byte orientation_t. */
void Q3TestLerpTag( float *origin, float axis[3][3], clipHandle_t handle, int startFrame, int endFrame, float frac,
	const char *tagName ) {
	VectorClear( origin );
	AxisClear( axis );
	const Q3TestMd3 *md3 = Q3TestMd3For( handle );
	if ( !md3 ) Com_Error( ERR_DROP, "CM_LerpTag: bad handle %i", handle );
	if ( startFrame >= md3->numFrames ) startFrame = md3->numFrames - 1;
	if ( endFrame >= md3->numFrames ) endFrame = md3->numFrames - 1;
	if ( startFrame < 0 ) startFrame = 0;
	if ( endFrame < 0 ) endFrame = 0;
	const std::uint8_t *start = nullptr, *end = nullptr;
	for ( int t = 0; t < md3->numTags; ++t ) {
		const std::uint8_t *a = md3->tags.data() + ( startFrame * md3->numTags + t ) * 112;
		const std::uint8_t *b = md3->tags.data() + ( endFrame * md3->numTags + t ) * 112;
		if ( !strncmp( reinterpret_cast<const char *>( a ), tagName, 64 ) ) start = a;
		if ( !strncmp( reinterpret_cast<const char *>( b ), tagName, 64 ) ) end = b;
	}
	if ( !start || !end ) return;
	const float back = 1.0f - frac;
	auto load = []( const std::uint8_t *at ) { float value; std::memcpy( &value, at, sizeof( value ) ); return value; };
	for ( int i = 0; i < 3; ++i ) {
		origin[i] = load( start + 64 + 4 * i ) * back + load( end + 64 + 4 * i ) * frac;
		for ( int j = 0; j < 3; ++j ) {
			axis[i][j] = load( start + 76 + 12 * i + 4 * j ) * back + load( end + 76 + 12 * i + 4 * j ) * frac;
		}
		VectorNormalize( axis[i] );
	}
}
void QDECL Q3CG_LerpTag105( void *tag, clipHandle_t handle, int startFrame, int endFrame, float frac, const char *name ) {
	float origin[3], axis[3][3];
	Q3TestLerpTag( origin, axis, handle, startFrame, endFrame, frac, name );
	std::memcpy( static_cast<std::uint8_t *>( tag ) + 0x40, origin, 12 );
	std::memcpy( static_cast<std::uint8_t *>( tag ) + 0x4c, axis, 36 );
}
void QDECL Q3CG_LerpTag( orientation_t *tag, clipHandle_t handle, int startFrame, int endFrame, float frac, const char *name ) {
	float origin[3], axis[3][3];
	Q3TestLerpTag( origin, axis, handle, startFrame, endFrame, frac, name );
	VectorCopy( origin, tag->origin );
	std::memcpy( tag->axis, axis, sizeof( axis ) );
}

void Q3TestRefuseMd3( clipHandle_t handle, const char *what ) {
	if ( Q3TestIsMd3Handle( handle ) ) Com_Error( ERR_DROP, "%s called with an md3 model", what );
}
int QDECL Q3CG_PointContents( const float *p, clipHandle_t model ) {
	Q3TestRefuseMd3( model, "CM_PointContents" );
	return CM_PointContents( p, model );
}
int QDECL Q3CG_TransformedPointContents( const float *p, clipHandle_t model, const float *origin, const float *angles ) {
	Q3TestRefuseMd3( model, "CM_TransformedPointContents" );
	return CM_TransformedPointContents( p, model, origin, angles );
}
void QDECL Q3CG_BoxTrace( trace_t *results, const float *start, const float *end, const float *mins, const float *maxs,
	clipHandle_t model, int brushmask ) {
	Q3TestRefuseMd3( model, "CM_BoxTrace" );
	CM_BoxTrace( results, start, end, mins, maxs, model, brushmask, qfalse );
}
void QDECL Q3CG_TransformedBoxTrace( trace_t *results, const float *start, const float *end, const float *mins,
	const float *maxs, clipHandle_t model, int brushmask, const float *origin, const float *angles ) {
	Q3TestRefuseMd3( model, "CM_TransformedBoxTrace" );
	CM_TransformedBoxTrace( results, start, end, mins, maxs, model, brushmask, origin, angles, qfalse );
}
int QDECL Q3CG_BoxBrushes105( const float *mins, const float *maxs, void **list, int listsize, int *lastLeaf ) {
	return CM_Q3TestBoxBrushes( mins, maxs, list, listsize, lastLeaf );
}
/* 1.06 and later mark against collision brushes (0x40a5d0); this engine
 * marks against what its renderer draws, the same records in the same
 * buffers. */
int QDECL Q3CG_MarkFragments( int numPoints, const vec3_t *points, const float *projection, int maxPoints,
	vec3_t *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ) {
	return re.MarkFragments( numPoints, points, projection, maxPoints, pointBuffer[0], maxFragments, fragmentBuffer );
}

/* ---- sound ---------------------------------------------------------------- */

void QDECL Q3CG_StartSound( const float *origin, int entnum, int entchannel, sfxHandle_t sfx ) {
	if ( !origin && ( entnum < 0 || entnum > 1024 ) ) Com_Error( ERR_DROP, "S_StartSound: bad entitynum %i", entnum );
	S_StartSound( const_cast<float *>( origin ), entnum, entchannel, sfx );
}
// S_StartLocalSound takes a name and plays on the listener's channel 1 (1.05 0x418460).
void QDECL Q3CG_StartLocalSound( const char *name ) { S_StartLocalSound( S_RegisterSound( name, qfalse ), 1 ); }
/* The loops carry no entity and at most 32 are kept a frame (1.05 0x418550);
 * each takes its own loop slot in the engine's list. */
void QDECL Q3CG_ClearLoopingSounds() {
	S_ClearLoopingSounds( qfalse );
	q3cg.loopCount = 0;
}
void QDECL Q3CG_AddLoopingSound( const float *origin, const float *velocity, sfxHandle_t sfx ) {
	if ( q3cg.loopCount >= 32 ) return;
	S_AddLoopingSound( q3cg.loopCount++, origin, velocity, sfx );
}
void QDECL Q3CG_UpdateEntityPosition( int entnum, const float *origin ) {
	if ( entnum < 0 || entnum > 1024 ) Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entnum );
	S_UpdateEntityPosition( entnum, origin );
}
void QDECL Q3CG_Respatialize( int entnum, const float *origin, vec3_t axis[3] ) { S_Respatialize( entnum, origin, axis, 0 ); }
sfxHandle_t QDECL Q3CG_RegisterSound( const char *name ) {
	if ( Q3TestRelease().id >= Q3TEST_RELEASE_1_06 && name && strlen( name ) > 63 ) {
		Com_Printf( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}
	return S_RegisterSound( name, qfalse );
}
void QDECL Q3CG_StartBackgroundTrack( const char *name ) { S_StartBackgroundTrack( name ? name : "", name ? name : "" ); }
qboolean QDECL Q3CG_ChannelInUse( int ) { return qfalse; }

/* ---- renderer --------------------------------------------------------------- */

void QDECL Q3CG_LoadWorld( const char *name ) { re.LoadWorld( name ); }
qhandle_t QDECL Q3CG_RegisterModel( const char *name ) {
	if ( name && Q3TestRelease().id <= Q3TEST_RELEASE_1_07 ) {
		if ( !strcmp( name, "*beam" ) ) return Q3TEST_MODEL_BEAM;
		if ( !strcmp( name, "*rail" ) ) return Q3TEST_MODEL_RAIL;
		if ( !strcmp( name, "*lightning" ) ) return Q3TEST_MODEL_LIGHTNING;
		if ( !strcmp( name, "*railUnderwater" ) && Q3TestRelease().id >= Q3TEST_RELEASE_1_06 ) return Q3TEST_MODEL_RAIL_UNDERWATER;
	}
	return re.RegisterModel( name );
}
qhandle_t QDECL Q3CG_RegisterSkin( const char *name ) { return re.RegisterSkin( name ); }
qhandle_t QDECL Q3CG_RegisterShader( const char *name ) { return re.RegisterShader( name ); }
void QDECL Q3CG_ClearScene() { re.ClearScene(); }

/* The entity is Quake III's first 140 bytes. 1.05 to 1.07 skip renderfx
 * 0x20 (1.05 0x42ee4d, 1.06 0x4288d5) and draw only sprites specially,
 * their beam, rail and lightning being models of their own; 1.08 has
 * Quake III's types. */
void QDECL Q3CG_AddRefEntity( const void *native ) {
	refEntity_t entity{};
	std::memcpy( &entity, native, 140 );
	const q3testReleaseId_t release = Q3TestRelease().id;
	if ( release <= Q3TEST_RELEASE_1_07 ) {
		if ( release <= Q3TEST_RELEASE_1_06 && ( entity.renderfx & 0x20 ) ) return;
		const int type = entity.reType;
		entity.reType = type == RT_SPRITE ? RT_SPRITE : RT_MODEL;
		switch ( entity.hModel ) {
		case Q3TEST_MODEL_BEAM: entity.reType = RT_BEAM; entity.hModel = 0; break;
		case Q3TEST_MODEL_RAIL: entity.reType = RT_RAIL_CORE; entity.hModel = 0; break;
		case Q3TEST_MODEL_RAIL_UNDERWATER: entity.reType = RT_RAIL_RINGS; entity.hModel = 0; break;
		case Q3TEST_MODEL_LIGHTNING: entity.reType = RT_LIGHTNING; entity.hModel = 0; break;
		default: break;
		}
	} else if ( entity.reType < RT_MODEL || entity.reType > RT_PORTALSURFACE || entity.reType == RT_POLY ) {
		return;
	}
	re.AddRefEntityToScene( &entity, qfalse );
}
void QDECL Q3CG_AddPoly( qhandle_t shader, int numVerts, const polyVert_t *verts ) {
	if ( !shader ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: RE_AddPolyToScene: NULL poly shader\n" );
		return;
	}
	re.AddPolyToScene( shader, numVerts, verts, 1 );
}
void QDECL Q3CG_AddLight( const float *origin, float intensity, float r, float g, float b ) {
	if ( Q3TestRelease().id >= Q3TEST_RELEASE_1_06 && intensity <= 0 ) return;
	re.AddLightToScene( origin, intensity, r, g, b );
}
/* 1.05's refdef is 108 bytes and points at its area mask, whose set bits are
 * the visible areas (1.05 R_MarkLeaves 0x447c93); later releases carry
 * Quake III's 368 bytes, set bits hidden. */
void QDECL Q3CG_RenderScene( const void *native ) {
	refdef_t fd{};
	if ( Q3TestRelease().id == Q3TEST_RELEASE_1_05 ) {
		std::memcpy( &fd, native, 0x50 );
		const std::uint8_t *areamask = nullptr;
		std::memcpy( &areamask, static_cast<const std::uint8_t *>( native ) + 0x50, sizeof( areamask ) );
		for ( int i = 0; i < MAX_MAP_AREA_BYTES; ++i ) fd.areamask[i] = areamask ? static_cast<byte>( ~areamask[i] ) : 0;
	} else {
		std::memcpy( &fd, native, 368 );
	}
	re.RenderScene( &fd );
}
void QDECL Q3CG_SetColor( const float *rgba ) { re.SetColor( rgba ); }
void QDECL Q3CG_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}

/* ---- 1.05's own 2D helpers (slots 49-54, 1.05 0x407ba0..0x408030) --------- */

void QDECL Q3CG_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	const float scale = cls.glconfig.vidHeight / 480.0f;
	const float bias = cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640
		? ( cls.glconfig.vidWidth - cls.glconfig.vidHeight * 640.0f / 480.0f ) * 0.5f : 0.0f;
	if ( x ) *x = *x * scale + bias;
	if ( y ) *y *= scale;
	if ( w ) *w *= scale;
	if ( h ) *h *= scale;
}
void QDECL Q3CG_FillRect( float x, float y, float w, float h, const float *color ) {
	re.SetColor( color );
	Q3CG_AdjustFrom640( &x, &y, &w, &h );
	re.DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( nullptr );
}
void QDECL Q3CG_DrawPic( float x, float y, float w, float h, qhandle_t shader ) {
	Q3CG_AdjustFrom640( &x, &y, &w, &h );
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, shader );
}
int QDECL Q3CG_BigStringWidth( const char *s ) {
	int count = 0;
	for ( ; s && *s; ++s ) {
		if ( *s == 0x1b && s[1] >= '0' && s[1] <= '7' ) { ++s; continue; }
		++count;
	}
	return count * 16;
}
void Q3TestDrawBigChar( float x, float y, int ch ) {
	ch &= 255;
	if ( ch == ' ' ) return;
	float w = 16, h = 16;
	Q3CG_AdjustFrom640( &x, &y, &w, &h );
	const float row = ( ch >> 4 ) * 0.0625f, col = ( ch & 15 ) * 0.0625f;
	re.DrawStretchPic( x, y, w, h, col, row, col + 0.0625f, row + 0.0625f, cls.charSetShader );
}
void Q3TestDrawBigString( int x, int y, const char *s, const float *color, bool forceColor ) {
	if ( !s ) return;
	static const float black[4] = { 0, 0, 0, 1 };
	float shadow[4] = { 0, 0, 0, color[3] };
	(void)black;
	re.SetColor( shadow );
	int cx = x;
	for ( const char *p = s; *p; ++p ) {
		if ( *p == 0x1b && p[1] >= '0' && p[1] <= '7' ) { ++p; continue; }
		Q3TestDrawBigChar( static_cast<float>( cx + 2 ), static_cast<float>( y + 2 ), *p );
		cx += 16;
	}
	re.SetColor( color );
	cx = x;
	for ( const char *p = s; *p; ++p ) {
		if ( *p == 0x1b && p[1] >= '0' && p[1] <= '7' ) {
			if ( !forceColor ) {
				float c[4];
				Vector4Copy( g_color_table[ColorIndex( p[1] )], c );
				c[3] = color[3];
				re.SetColor( c );
			}
			++p;
			continue;
		}
		Q3TestDrawBigChar( static_cast<float>( cx ), static_cast<float>( y ), *p );
		cx += 16;
	}
	re.SetColor( nullptr );
}
void QDECL Q3CG_DrawBigString( int x, int y, const char *s, float alpha ) {
	const float color[4] = { 1, 1, 1, alpha };
	Q3TestDrawBigString( x, y, s, color, false );
}
void QDECL Q3CG_DrawBigStringColor( int x, int y, const char *s, const float *color ) {
	Q3TestDrawBigString( x, y, s, color, true );
}

/* ---- client data ------------------------------------------------------------ */

/* glconfig_t: Quake III's order with a 1024-byte extension string, 4160 bytes
 * in 1.05 and 4164 from 1.06 (smpActive). */
void QDECL Q3CG_GetGlconfig( void *native ) {
	const std::size_t bytes = Q3TestRelease().id == Q3TEST_RELEASE_1_05 ? 4160 : 4164;
	std::vector<std::uint8_t> out( bytes, 0 );
	std::memcpy( out.data(), cls.glconfig.renderer_string, 1023 );
	std::memcpy( out.data() + 0x400, cls.glconfig.vendor_string, 1023 );
	std::memcpy( out.data() + 0x800, cls.glconfig.version_string, 1023 );
	std::memcpy( out.data() + 0xc00, cls.glconfig.extensions_string, 1023 );
	const std::int32_t words[] = { cls.glconfig.maxTextureSize, cls.glconfig.numTextureUnits, cls.glconfig.colorBits,
		cls.glconfig.depthBits, cls.glconfig.stencilBits, cls.glconfig.driverType, cls.glconfig.hardwareType,
		cls.glconfig.deviceSupportsGamma, cls.glconfig.textureCompression, cls.glconfig.textureEnvAddAvailable,
		cls.glconfig.vidWidth, cls.glconfig.vidHeight };
	std::memcpy( out.data() + 0x1000, words, sizeof( words ) );
	std::memcpy( out.data() + 0x1030, &cls.glconfig.windowAspect, 4 );
	const std::int32_t tail[] = { cls.glconfig.displayFrequency, cls.glconfig.isFullscreen, cls.glconfig.stereoEnabled, 0 };
	std::memcpy( out.data() + 0x1034, tail, bytes - 0x1034 );
	std::memcpy( native, out.data(), bytes );
}
void QDECL Q3CG_GetGameState( void *native ) { q3cl::WriteGameState( native, 20100 ); }
void QDECL Q3CG_GetCurrentSnapshotNumber( int *number, int *serverTime ) {
	int n = 0, t = 0;
	q3cl::CurrentSnapshot( n, t );
	*number = n;
	*serverTime = t;
}

/* snapshot_t (5.2): 1.05 0xCDE8 bytes with serverTime at 0xC and the player at
 * 0x34; 1.06 and 1.07 0xCDF4, 1.08 0xCDFC, with serverTime at 8, the player
 * at 0x30 and the command sequence last. At most 256 entities. */
qboolean QDECL Q3CG_GetSnapshot( int number, void *native ) {
	const auto snap = q3cl::Snapshot( number );
	if ( !snap ) return qfalse;
	const int protocol = Q3TestProtocol();
	const bool v105 = protocol == 36;
	const std::size_t playerBytes = q3wire::PlayerBytes( protocol );
	const std::size_t playerAt = v105 ? 0x34 : 0x30;
	const std::size_t countAt = playerAt + playerBytes;
	auto *out = static_cast<std::uint8_t *>( native );
	std::memset( out, 0, countAt + 4 );
	q3wire::Store( out, 4, static_cast<std::uint32_t>( snap->snapFlags ) );
	q3wire::Store( out + 4, 4, static_cast<std::uint32_t>( q3cl::SnapshotPing( *snap ) ) );
	q3wire::Store( out + ( v105 ? 0xc : 8 ), 4, static_cast<std::uint32_t>( snap->serverTime ) );
	std::memcpy( out + ( v105 ? 0x10 : 0xc ), snap->areamask.data(), std::min<std::size_t>( snap->areamask.size(), 32 ) );
	q3wire::Store( out + ( v105 ? 0x30 : 0x2c ), 4, static_cast<std::uint32_t>( snap->lastUsercmd ) );
	std::memcpy( out + playerAt, snap->player.data(), playerBytes );
	std::size_t count = snap->entities.size();
	if ( count > 256 ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", static_cast<int>( count ), 256 );
		count = 256;
	}
	q3wire::Store( out + countAt, 4, static_cast<std::uint32_t>( count ) );
	for ( std::size_t i = 0; i < count; ++i )
		std::memcpy( out + countAt + 4 + i * q3wire::EntityBytes, snap->entities[i].data(), q3wire::EntityBytes );
	if ( !v105 ) {
		const std::size_t sequenceAt = countAt + 4 + 256 * q3wire::EntityBytes + 12;
		q3wire::Store( out + sequenceAt, 4, static_cast<std::uint32_t>( snap->commandSequence ) );
	}
	return qtrue;
}
qboolean QDECL Q3CG_GetServerCommand( int number ) { return q3cl::ServerCommand( number ) ? qtrue : qfalse; }
int QDECL Q3CG_GetCurrentCmdNumber() { return q3cl::CurrentCmdNumber(); }
qboolean QDECL Q3CG_GetUserCmd( int number, void *native ) {
	return q3cl::UserCmd( number, native, q3wire::UsercmdBytes( Q3TestProtocol() ) ) ? qtrue : qfalse;
}
void QDECL Q3CG_SetUserCmdValue105( int weapon ) { q3cl::SetUserCmdValue( weapon, 1.0f ); }
void QDECL Q3CG_SetUserCmdValue( int weapon, float sensitivity ) { q3cl::SetUserCmdValue( weapon, sensitivity ); }

/* 1.05 passes a format and its arguments to Printf and Error. */
void QDECL Q3CG_Printf105( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof( text ), fmt, args ); va_end( args );
	Q3CG_Print( text );
}
void QDECL Q3CG_Error105( int level, const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof( text ), fmt, args ); va_end( args );
	(void)level;
	Q3CG_Error( text );
}

template<class F> std::intptr_t Slot( F *function ) { return reinterpret_cast<std::intptr_t>( function ); }

void Q3TestFillImports105() {
	auto &t = q3cg.imports;
	t = {};
	t[1] = Slot( Q3CG_Printf105 ); t[2] = Slot( Q3CG_Error105 ); t[3] = Slot( Q3CG_Milliseconds );
	t[4] = Slot( Q3CG_CvarRegister ); t[5] = Slot( Q3CG_CvarUpdate ); t[6] = Slot( Q3CG_CvarSet );
	t[7] = Slot( Q3CG_Argc ); t[8] = Slot( Q3CG_Argv );
	t[9] = Slot( Q3CG_FOpen105 ); t[10] = Slot( Q3CG_Read105 ); t[11] = Slot( Q3CG_FOpenWrite105 );
	t[12] = Slot( Q3CG_Write105 ); t[13] = Slot( Q3CG_FClose105 );
	t[14] = Slot( Q3CG_AddText ); t[15] = Slot( Q3CG_AddCommand ); t[16] = Slot( Q3CG_ClientCommand );
	t[17] = Slot( Q3CG_LoadMap105 ); t[18] = Slot( Q3CG_NumInlineModels ); t[19] = Slot( Q3CG_BoxBrushes105 );
	t[20] = Slot( Q3CG_LerpTag105 ); t[21] = Slot( Q3CG_InlineModel ); t[22] = Slot( Q3CG_LoadModel );
	t[23] = Slot( Q3CG_TempBoxModel ); t[24] = Slot( Q3CG_ModelBounds ); t[25] = Slot( Q3CG_PointContents );
	t[26] = Slot( Q3CG_TransformedPointContents ); t[27] = Slot( Q3CG_BoxTrace ); t[28] = Slot( Q3CG_TransformedBoxTrace );
	t[29] = Slot( Q3CG_StartSound ); t[30] = Slot( Q3CG_StartLocalSound ); t[31] = Slot( Q3CG_ClearLoopingSounds );
	t[32] = Slot( Q3CG_AddLoopingSound ); t[33] = Slot( Q3CG_UpdateEntityPosition ); t[34] = Slot( Q3CG_Respatialize );
	t[35] = Slot( Q3CG_RegisterSound ); t[36] = Slot( Q3CG_StartBackgroundTrack );
	t[37] = Slot( Q3CG_LoadWorld ); t[38] = Slot( Q3CG_RegisterModel ); t[39] = Slot( Q3CG_RegisterSkin );
	t[40] = Slot( Q3CG_RegisterShader ); t[41] = Slot( Q3CG_ClearScene ); t[42] = Slot( Q3CG_AddRefEntity );
	t[43] = Slot( Q3CG_AddPoly ); t[44] = Slot( Q3CG_AddLight ); t[45] = Slot( Q3CG_RenderScene );
	t[46] = Slot( Q3CG_SetColor ); t[47] = Slot( Q3CG_DrawStretchPic ); t[48] = Slot( Q3CG_UpdateScreen );
	t[49] = Slot( Q3CG_BigStringWidth ); t[50] = Slot( Q3CG_AdjustFrom640 ); t[51] = Slot( Q3CG_FillRect );
	t[52] = Slot( Q3CG_DrawPic ); t[53] = Slot( Q3CG_DrawBigString ); t[54] = Slot( Q3CG_DrawBigStringColor );
	t[55] = Slot( Q3CG_GetGlconfig ); t[56] = Slot( Q3CG_GetGameState ); t[57] = Slot( Q3CG_GetCurrentSnapshotNumber );
	t[58] = Slot( Q3CG_GetSnapshot ); t[61] = Slot( Q3CG_GetCurrentCmdNumber ); t[62] = Slot( Q3CG_GetUserCmd );
	t[63] = Slot( Q3CG_SetUserCmdValue105 );
}

void Q3TestFillImports106() {
	auto &t = q3cg.imports;
	t = {};
	t[1] = Slot( Q3CG_Print ); t[2] = Slot( Q3CG_Error ); t[3] = Slot( Q3CG_Milliseconds );
	t[4] = Slot( Q3CG_CvarRegister ); t[5] = Slot( Q3CG_CvarUpdate ); t[6] = Slot( Q3CG_CvarSet );
	t[7] = Slot( Q3CG_Argc ); t[8] = Slot( Q3CG_Argv );
	t[9] = Slot( Q3CG_FOpen ); t[10] = Slot( Q3CG_Read ); t[11] = Slot( Q3CG_Write ); t[12] = Slot( Q3CG_FClose );
	t[13] = Slot( Q3CG_AddText ); t[14] = Slot( Q3CG_AddCommand ); t[15] = Slot( Q3CG_ClientCommand );
	t[16] = Slot( Q3CG_UpdateScreen ); t[17] = Slot( Q3CG_LoadMap ); t[18] = Slot( Q3CG_NumInlineModels );
	t[19] = Slot( Q3CG_LerpTag ); t[20] = Slot( Q3CG_InlineModel ); t[21] = Slot( Q3CG_LoadModel );
	t[22] = Slot( Q3CG_TempBoxModel ); t[23] = Slot( Q3CG_ModelBounds ); t[24] = Slot( Q3CG_PointContents );
	t[25] = Slot( Q3CG_TransformedPointContents ); t[26] = Slot( Q3CG_BoxTrace ); t[27] = Slot( Q3CG_TransformedBoxTrace );
	t[28] = Slot( Q3CG_MarkFragments ); t[29] = Slot( Q3CG_StartSound ); t[30] = Slot( Q3CG_StartLocalSound );
	t[31] = Slot( Q3CG_ClearLoopingSounds ); t[32] = Slot( Q3CG_AddLoopingSound ); t[33] = Slot( Q3CG_UpdateEntityPosition );
	t[34] = Slot( Q3CG_Respatialize ); t[35] = Slot( Q3CG_RegisterSound ); t[36] = Slot( Q3CG_StartBackgroundTrack );
	t[37] = Slot( Q3CG_ChannelInUse ); t[38] = Slot( Q3CG_LoadWorld ); t[39] = Slot( Q3CG_RegisterModel );
	t[40] = Slot( Q3CG_RegisterSkin ); t[41] = Slot( Q3CG_RegisterShader ); t[42] = Slot( Q3CG_ClearScene );
	t[43] = Slot( Q3CG_AddRefEntity ); t[44] = Slot( Q3CG_AddPoly ); t[45] = Slot( Q3CG_AddLight );
	t[46] = Slot( Q3CG_RenderScene ); t[47] = Slot( Q3CG_SetColor ); t[48] = Slot( Q3CG_DrawStretchPic );
	t[49] = Slot( Q3CG_GetGlconfig ); t[50] = Slot( Q3CG_GetGameState ); t[51] = Slot( Q3CG_GetCurrentSnapshotNumber );
	t[52] = Slot( Q3CG_GetSnapshot ); t[53] = Slot( Q3CG_GetServerCommand ); t[54] = Slot( Q3CG_GetCurrentCmdNumber );
	t[55] = Slot( Q3CG_GetUserCmd ); t[56] = Slot( Q3CG_SetUserCmdValue );
}

/* CL_CgameSystemCalls (1.08 0x401530): the 1.07 slots less the first, with
 * Cmd_ArgsBuffer at 8, and QVM math at 100-111. Unknown numbers return 0. */
float Q3TestArgFloat( intptr_t value ) {
	const auto bits = static_cast<std::uint32_t>( value );
	float result;
	std::memcpy( &result, &bits, sizeof( result ) );
	return result;
}
intptr_t Q3TestFloatResult( float value ) {
	std::uint32_t bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	return static_cast<intptr_t>( bits );
}
template<class T> T *Q3TestPtr( intptr_t value ) { return reinterpret_cast<T *>( value ); }

intptr_t Q3TestSyscall( const intptr_t *a ) {
	switch ( a[0] ) {
	case 0: Q3CG_Print( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 1: Q3CG_Error( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 2: return Q3CG_Milliseconds();
	case 3: Q3CG_CvarRegister( Q3TestPtr<void>( a[1] ), Q3TestPtr<const char>( a[2] ), Q3TestPtr<const char>( a[3] ), static_cast<int>( a[4] ) ); return 0;
	case 4: Q3CG_CvarUpdate( Q3TestPtr<void>( a[1] ) ); return 0;
	case 5: Q3CG_CvarSet( Q3TestPtr<const char>( a[1] ), Q3TestPtr<const char>( a[2] ) ); return 0;
	case 6: return Q3CG_Argc();
	case 7: Q3CG_Argv( static_cast<int>( a[1] ), Q3TestPtr<char>( a[2] ), static_cast<int>( a[3] ) ); return 0;
	case 8: Q3CG_Args( Q3TestPtr<char>( a[1] ), static_cast<int>( a[2] ) ); return 0;
	case 9: return Q3CG_FOpen( Q3TestPtr<const char>( a[1] ), Q3TestPtr<fileHandle_t>( a[2] ), static_cast<int>( a[3] ) );
	case 10: Q3CG_Read( Q3TestPtr<void>( a[1] ), static_cast<int>( a[2] ), static_cast<fileHandle_t>( a[3] ) ); return 0;
	case 11: Q3CG_Write( Q3TestPtr<const void>( a[1] ), static_cast<int>( a[2] ), static_cast<fileHandle_t>( a[3] ) ); return 0;
	case 12: Q3CG_FClose( static_cast<fileHandle_t>( a[1] ) ); return 0;
	case 13: Q3CG_AddText( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 14: Q3CG_AddCommand( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 15: Q3CG_ClientCommand( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 16: Q3CG_UpdateScreen(); return 0;
	case 17: Q3CG_LoadMap( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 18: return Q3CG_NumInlineModels();
	case 19: Q3CG_LerpTag( Q3TestPtr<orientation_t>( a[1] ), static_cast<clipHandle_t>( a[2] ), static_cast<int>( a[3] ),
		static_cast<int>( a[4] ), Q3TestArgFloat( a[5] ), Q3TestPtr<const char>( a[6] ) ); return 0;
	case 20: return Q3CG_InlineModel( static_cast<int>( a[1] ) );
	case 21: return Q3CG_LoadModel( Q3TestPtr<const char>( a[1] ) );
	case 22: return Q3CG_TempBoxModel( Q3TestPtr<const float>( a[1] ), Q3TestPtr<const float>( a[2] ) );
	case 23: Q3CG_ModelBounds( static_cast<clipHandle_t>( a[1] ), Q3TestPtr<float>( a[2] ), Q3TestPtr<float>( a[3] ) ); return 0;
	case 24: return Q3CG_PointContents( Q3TestPtr<const float>( a[1] ), static_cast<clipHandle_t>( a[2] ) );
	case 25: return Q3CG_TransformedPointContents( Q3TestPtr<const float>( a[1] ), static_cast<clipHandle_t>( a[2] ),
		Q3TestPtr<const float>( a[3] ), Q3TestPtr<const float>( a[4] ) );
	case 26: Q3CG_BoxTrace( Q3TestPtr<trace_t>( a[1] ), Q3TestPtr<const float>( a[2] ), Q3TestPtr<const float>( a[3] ),
		Q3TestPtr<const float>( a[4] ), Q3TestPtr<const float>( a[5] ), static_cast<clipHandle_t>( a[6] ), static_cast<int>( a[7] ) ); return 0;
	case 27: Q3CG_TransformedBoxTrace( Q3TestPtr<trace_t>( a[1] ), Q3TestPtr<const float>( a[2] ), Q3TestPtr<const float>( a[3] ),
		Q3TestPtr<const float>( a[4] ), Q3TestPtr<const float>( a[5] ), static_cast<clipHandle_t>( a[6] ), static_cast<int>( a[7] ),
		Q3TestPtr<const float>( a[8] ), Q3TestPtr<const float>( a[9] ) ); return 0;
	case 28: return Q3CG_MarkFragments( static_cast<int>( a[1] ), Q3TestPtr<const vec3_t>( a[2] ), Q3TestPtr<const float>( a[3] ),
		static_cast<int>( a[4] ), Q3TestPtr<vec3_t>( a[5] ), static_cast<int>( a[6] ), Q3TestPtr<markFragment_t>( a[7] ) );
	case 29: Q3CG_StartSound( Q3TestPtr<const float>( a[1] ), static_cast<int>( a[2] ), static_cast<int>( a[3] ), static_cast<sfxHandle_t>( a[4] ) ); return 0;
	case 30: Q3CG_StartLocalSound( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 31: Q3CG_ClearLoopingSounds(); return 0;
	case 32: Q3CG_AddLoopingSound( Q3TestPtr<const float>( a[1] ), Q3TestPtr<const float>( a[2] ), static_cast<sfxHandle_t>( a[3] ) ); return 0;
	case 33: Q3CG_UpdateEntityPosition( static_cast<int>( a[1] ), Q3TestPtr<const float>( a[2] ) ); return 0;
	case 34: Q3CG_Respatialize( static_cast<int>( a[1] ), Q3TestPtr<const float>( a[2] ), Q3TestPtr<vec3_t>( a[3] ) ); return 0;
	case 35: return Q3CG_RegisterSound( Q3TestPtr<const char>( a[1] ) );
	case 36: Q3CG_StartBackgroundTrack( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 37: return Q3CG_ChannelInUse( static_cast<int>( a[1] ) );
	case 38: Q3CG_LoadWorld( Q3TestPtr<const char>( a[1] ) ); return 0;
	case 39: return Q3CG_RegisterModel( Q3TestPtr<const char>( a[1] ) );
	case 40: return Q3CG_RegisterSkin( Q3TestPtr<const char>( a[1] ) );
	case 41: return Q3CG_RegisterShader( Q3TestPtr<const char>( a[1] ) );
	case 42: Q3CG_ClearScene(); return 0;
	case 43: Q3CG_AddRefEntity( Q3TestPtr<const void>( a[1] ) ); return 0;
	case 44: Q3CG_AddPoly( static_cast<qhandle_t>( a[1] ), static_cast<int>( a[2] ), Q3TestPtr<const polyVert_t>( a[3] ) ); return 0;
	case 45: Q3CG_AddLight( Q3TestPtr<const float>( a[1] ), Q3TestArgFloat( a[2] ), Q3TestArgFloat( a[3] ), Q3TestArgFloat( a[4] ),
		Q3TestArgFloat( a[5] ) ); return 0;
	case 46: Q3CG_RenderScene( Q3TestPtr<const void>( a[1] ) ); return 0;
	case 47: Q3CG_SetColor( Q3TestPtr<const float>( a[1] ) ); return 0;
	case 48: Q3CG_DrawStretchPic( Q3TestArgFloat( a[1] ), Q3TestArgFloat( a[2] ), Q3TestArgFloat( a[3] ), Q3TestArgFloat( a[4] ),
		Q3TestArgFloat( a[5] ), Q3TestArgFloat( a[6] ), Q3TestArgFloat( a[7] ), Q3TestArgFloat( a[8] ), static_cast<qhandle_t>( a[9] ) ); return 0;
	case 49: Q3CG_GetGlconfig( Q3TestPtr<void>( a[1] ) ); return 0;
	case 50: Q3CG_GetGameState( Q3TestPtr<void>( a[1] ) ); return 0;
	case 51: Q3CG_GetCurrentSnapshotNumber( Q3TestPtr<int>( a[1] ), Q3TestPtr<int>( a[2] ) ); return 0;
	case 52: return Q3CG_GetSnapshot( static_cast<int>( a[1] ), Q3TestPtr<void>( a[2] ) );
	case 53: return Q3CG_GetServerCommand( static_cast<int>( a[1] ) );
	case 54: return Q3CG_GetCurrentCmdNumber();
	case 55: return Q3CG_GetUserCmd( static_cast<int>( a[1] ), Q3TestPtr<void>( a[2] ) );
	case 56: Q3CG_SetUserCmdValue( static_cast<int>( a[1] ), Q3TestArgFloat( a[2] ) ); return 0;
	case 100: std::memset( Q3TestPtr<void>( a[1] ), static_cast<int>( a[2] ) & 0xff, static_cast<std::size_t>( a[3] ) ); return a[1];
	case 101: std::memcpy( Q3TestPtr<void>( a[1] ), Q3TestPtr<const void>( a[2] ), static_cast<std::size_t>( a[3] ) ); return a[1];
	case 102: return reinterpret_cast<intptr_t>( strncpy( Q3TestPtr<char>( a[1] ), Q3TestPtr<const char>( a[2] ), static_cast<std::size_t>( a[3] ) ) );
	case 103: return Q3TestFloatResult( sinf( Q3TestArgFloat( a[1] ) ) );
	case 104: return Q3TestFloatResult( cosf( Q3TestArgFloat( a[1] ) ) );
	case 105: return Q3TestFloatResult( atan2f( Q3TestArgFloat( a[1] ), Q3TestArgFloat( a[2] ) ) );
	case 106: return Q3TestFloatResult( sqrtf( Q3TestArgFloat( a[1] ) ) );
	case 107: MatrixMultiply( Q3TestPtr<float[3]>( a[1] ), Q3TestPtr<float[3]>( a[2] ), Q3TestPtr<float[3]>( a[3] ) ); return 0;
	case 108: AngleVectors( Q3TestPtr<const float>( a[1] ), Q3TestPtr<float>( a[2] ), Q3TestPtr<float>( a[3] ), Q3TestPtr<float>( a[4] ) ); return 0;
	case 109: PerpendicularVector( Q3TestPtr<float>( a[1] ), Q3TestPtr<const float>( a[2] ) ); return 0;
	case 110: return Q3TestFloatResult( floorf( Q3TestArgFloat( a[1] ) ) );
	case 111: return Q3TestFloatResult( ceilf( Q3TestArgFloat( a[1] ) ) );
	default: return 0;
	}
}

intptr_t QDECL Q3TestDllSyscall( intptr_t arg, ... ) {
	intptr_t args[10];
	args[0] = arg;
	va_list ap;
	va_start( ap, arg );
	for ( int i = 1; i < 10; ++i ) args[i] = va_arg( ap, intptr_t );
	va_end( ap );
	return Q3TestSyscall( args );
}

/* ---- exports ---------------------------------------------------------------- */

template<class... A> intptr_t Q3TestExport( int word, A... args ) {
	using Fn = intptr_t ( QDECL * )( A... );
	return reinterpret_cast<Fn>( q3cg.exports[word] )( args... );
}

/* The engine's calls, in its own numbering. 1.05 has no Shutdown and no
 * CrosshairPlayer; 1.06 and 1.07 report no target as 0 and 1.08 as -1; the
 * console command reaches the module only while active (1.05 0x4015b0). */
intptr_t QDECL CL_Q3TestCGameEntry( int command, ... ) {
	va_list args;
	va_start( args, command );
	const intptr_t a0 = va_arg( args, intptr_t ), a1 = va_arg( args, intptr_t ), a2 = va_arg( args, intptr_t );
	va_end( args );
	switch ( q3cg.style ) {
	case Q3TestCGameStyle::Syscalls108:
		if ( command == CG_CONSOLE_COMMAND && cls.state != CA_ACTIVE ) return 0;
		if ( command > CG_LAST_ATTACKER ) return 0;
		{
			const intptr_t result = q3cg.vmMain( command, a0, a1, a2, 0, 0, 0, 0, 0, 0 );
			if ( ( command == CG_CROSSHAIR_PLAYER || command == CG_LAST_ATTACKER ) && ( result < 0 || result >= 128 ) ) return -1;
			return result;
		}
	case Q3TestCGameStyle::Table106:
		switch ( command ) {
		case CG_SHUTDOWN: Q3TestExport( 1 ); return 0;
		case CG_CONSOLE_COMMAND: return cls.state == CA_ACTIVE ? Q3TestExport( 2 ) : 0;
		case CG_DRAW_ACTIVE_FRAME: Q3TestExport( 3, static_cast<int>( a0 ), static_cast<int>( a1 ), static_cast<int>( a2 ) ); return 0;
		case CG_CROSSHAIR_PLAYER: { const intptr_t result = Q3TestExport( 4 ); return result > 0 && result < 128 ? result : -1; }
		default: return command == CG_LAST_ATTACKER ? -1 : 0;
		}
	case Q3TestCGameStyle::Table105:
		switch ( command ) {
		case CG_CONSOLE_COMMAND: return cls.state == CA_ACTIVE ? Q3TestExport( 3 ) : 0;
		case CG_DRAW_ACTIVE_FRAME: Q3TestExport( 4, static_cast<int>( a0 ), static_cast<int>( a1 ), static_cast<int>( a2 ) ); return 0;
		case CG_CROSSHAIR_PLAYER: case CG_LAST_ATTACKER: return -1;
		default: return 0;
		}
	default:
		return 0;
	}
}

} // namespace

/* The collision, record and 2D services the menus share with the client game. */
namespace fntech3::q3test::client {

clipHandle_t CollisionModel( const char *name ) { return Q3CG_LoadModel( name ); }
void LerpTag( orientation_t *tag, clipHandle_t model, int startFrame, int endFrame, float frac, const char *name ) {
	Q3CG_LerpTag( tag, model, startFrame, endFrame, frac, name );
}
void WriteGlconfig( void *native ) { Q3CG_GetGlconfig( native ); }
void AddRefEntity( const void *native ) { Q3CG_AddRefEntity( native ); }
void RenderScene( const void *native ) { Q3CG_RenderScene( native ); }
void AdjustFrom640( float *x, float *y, float *w, float *h ) { Q3CG_AdjustFrom640( x, y, w, h ); }
void FillRect( float x, float y, float w, float h, const float *color ) { Q3CG_FillRect( x, y, w, h, color ); }
void DrawPic( float x, float y, float w, float h, qhandle_t shader ) { Q3CG_DrawPic( x, y, w, h, shader ); }
void DrawBigChar( float x, float y, int ch ) { Q3TestDrawBigChar( x, y, ch ); }
void DrawBigString( int x, int y, const char *s, const float *color, bool forceColor ) {
	Q3TestDrawBigString( x, y, s, color, forceColor );
}
int BigStringWidth( const char *s ) { return Q3CG_BigStringWidth( s ); }
std::string Text( const char *text ) { return Q3TestText( text ); }
int CvarFlags( int flags ) { return Q3TestCvarFlags( flags ); }

} // namespace fntech3::q3test::client

/* Loads and initialises the release's client game (CL_InitCGame, 1.05
 * 0x401280, 1.06 0x401530, 1.08 0x401fc0). */
vm_t *CL_Q3TestLoadCGame( void ) {
	const q3testRelease_t &release = Q3TestRelease();
	if ( release.cgameEntry == Q3TEST_CGAME_NONE ) {
		Com_Error( ERR_DROP, "Q3Test %s shipped no client game for Win32", release.version );
		return nullptr;
	}
	// The retail libraries are 32-bit; so must the process be that loads them.
	if ( sizeof( void * ) != 4 ) {
		Com_Error( ERR_DROP, "Q3Test's client games are Win32 x86 libraries; this build cannot load them" );
		return nullptr;
	}
	q3cg = Q3TestCGame{};
	void *symbol = nullptr;
	vm_t *vm = nullptr;
	if ( release.cgameEntry == Q3TEST_CGAME_SYSCALLS ) {
		vm = VM_CreateTable( VM_CGAME, "vmMain", CL_Q3TestCGameEntry, &symbol );
		if ( !vm ) return nullptr;
		auto dllEntry = reinterpret_cast<void ( QDECL * )( intptr_t ( QDECL * )( intptr_t, ... ) )>(
			Sys_LoadFunction( vm->dllHandle, "dllEntry" ) );
		if ( !dllEntry ) {
			VM_Free( vm );
			Com_Error( ERR_DROP, "Q3Test's cgamex86.dll has no dllEntry" );
			return nullptr;
		}
		q3cg.vmMain = reinterpret_cast<intptr_t ( QDECL * )( int, ... )>( symbol );
		q3cg.style = Q3TestCGameStyle::Syscalls108;
		dllEntry( Q3TestDllSyscall );
	} else {
		vm = VM_CreateTable( VM_CGAME, "GetCGameAPI", CL_Q3TestCGameEntry, &symbol );
		if ( !vm ) return nullptr;
		auto getter = reinterpret_cast<const std::intptr_t *( QDECL * )( void )>( symbol );
		q3cg.exports = getter();
		if ( !q3cg.exports ) {
			VM_Free( vm );
			Com_Error( ERR_FATAL, "Sys_GetCGameAPI failed" );
			return nullptr;
		}
		q3cg.style = release.id == Q3TEST_RELEASE_1_05 ? Q3TestCGameStyle::Table105 : Q3TestCGameStyle::Table106;
		if ( q3cg.style == Q3TestCGameStyle::Table105 ) Q3TestFillImports105();
		else Q3TestFillImports106();
	}
	return vm;
}

/* The module's Init: 1.05 the table, 1.06 and 1.07 the table and the
 * command sequence, 1.08 CG_INIT with the sequence. */
void CL_Q3TestInitCGameModule( void ) {
	switch ( q3cg.style ) {
	case Q3TestCGameStyle::Table105: Q3TestExport( 0, q3cg.imports.data() ); break;
	case Q3TestCGameStyle::Table106: Q3TestExport( 0, q3cg.imports.data(), clc.serverCommandSequence ); break;
	case Q3TestCGameStyle::Syscalls108: q3cg.vmMain( CG_INIT, clc.serverCommandSequence, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); break;
	default: break;
	}
}

void CL_Q3TestShutdownCGame( void ) {
	q3cg = Q3TestCGame{};
}

void CL_Q3TestCGameConfigstringModified( int index ) {
	if ( q3cg.style == Q3TestCGameStyle::Table105 ) Q3TestExport( 1, index );
}

void CL_Q3TestCGameServerCommand( void ) {
	if ( q3cg.style == Q3TestCGameStyle::Table105 ) Q3TestExport( 2 );
}
