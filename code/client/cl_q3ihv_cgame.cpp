/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* The February 1999 IHV Test's client game, cgamex86.dll.
 *
 * GetCGameAPI takes no argument and returns eight function pointers; the
 * engine hands its 62-word import table to the first (CL_InitCGame,
 * quake3.exe 0x405e30). The exports are Init, the snapshot, configstring and
 * server-command pushes, the console command, the frame (time and whether a
 * demo is playing; it answers whether it has something to draw), the draw
 * and the loading screen. Every record crossing the boundary keeps the
 * build's own layout: the 20,100-byte gamestate, the 348-byte snapshot,
 * 164-byte entities, 14-byte commands, 32-byte cvars, 56-byte traces returned
 * by value, 112-byte collision models and the 152-byte scene. The
 * collision calls are Quake II's: a model is a pointer to its record and a
 * trace takes the headnode the record holds, which here is the engine's clip
 * handle. Presentation is this engine's (AGENTS.md invariant 11): scenes go
 * to its renderers through their IHV entity and particle paths, sounds to
 * its audio pipeline, text and pictures through its 640x480 screen. Facts
 * and addresses: docs/fntech3/Q3_IHV.md. */
extern "C" {
#include "client.h"
}
#include "client_q3ihv.h"
#include "audio/snd_public.h"
#include "../compat/q3_ihv_client_data.hpp"
#include "../compat/q3_ihv_cvars.hpp"
#include "../compat/q3_ihv_model_tags.hpp"
#include "../compat/q3_ihv_native_boundary.hpp"
#include "../compat/q3_ihv_scene.hpp"
#include <array>
#include <cstdarg>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ihv = fntech3::q3ihv;
namespace ihvcl = fntech3::q3ihv::client;
namespace ihvrender = fntech3::q3ihv::render;

namespace {

constexpr int Q3IHV_IMPORTS = 62;   // CL_InitCGame's table, quake3.exe 0x405e59
static_assert( offsetof( fntech3_q3_ihv_collision_model_t, headnode ) == 24, "IHV model record headnode" );
constexpr int Q3IHV_EXPORTS = 8;    // GetCGameAPI, cgamex86.dll 0x10009590

/* The engine's cvar under a module's 32-byte record (Cvar_Get 0x40de80). The
 * record keeps its address; values are copied in before every call into the
 * module, as the server's game mirrors are (server_q3_ihv_game.hpp). */
struct IhvCvar {
	ihv::CvarMirror mirror;
	char name[256]{}, reset[1024]{};
	cvar_t *source = nullptr;
	int flags = 0, serial = -1;
	IhvCvar *next = nullptr;
};

/* Resources the module registers come back as tokens naming their kind, so
 * a scene's model, skin and shader fields resolve only to their own kind. A
 * failed registration is 0, as the original's null pointer was. */
enum class IhvKind : std::uint32_t { Model = 1, Skin = 2, Shader = 3, Sound = 4 };
std::uint32_t IhvToken( IhvKind kind, int handle ) {
	if ( handle < 0 || handle >= 0x0fffffff ) return 0;
	return ( static_cast<std::uint32_t>( kind ) << 28 ) | static_cast<std::uint32_t>( handle + 1 );
}
bool IhvHandle( IhvKind kind, std::uint32_t token, int &handle ) {
	if ( !token || ( token >> 28 ) != static_cast<std::uint32_t>( kind ) ) return false;
	handle = static_cast<int>( token & 0x0fffffff ) - 1;
	return true;
}

struct IhvCGame {
	const std::intptr_t *exports = nullptr;
	std::array<std::intptr_t, Q3IHV_IMPORTS> imports{};
	bool initialized = false;
	// Import 61: the word the engine copies into each command's weapon byte
	// (CL_CreateCmd 0x402e91); the module sets it.
	std::int32_t weapon = 0;
	IhvCvar *cvars = nullptr;
	// Inline models' 112-byte records, built when the module loads the map.
	std::vector<fntech3_q3_ihv_collision_model_t> inlineModels;
	std::map<std::string, std::unique_ptr<ihv::NativeModelMetadata>> md3s;
	fntech3_q3_ihv_tag_t tag{};
	std::string argv, args;
	// The sound positions the module's callback reports (S_Respatialize 0x416200).
	std::map<int, int> soundEntities;   // entity -> time of its last sound
	int loopCount = 0;
	int listener = 0;
};
IhvCGame ihvcg;

/* ---- common services ---------------------------------------------------- */

/* IHV's colour escape is 0x1B and a digit, as Q3Test's (0x407640). */
std::string IhvText( const char *text ) {
	std::string out = text ? text : "";
	for ( std::size_t i = 0; i + 1 < out.size(); ++i ) {
		if ( out[i] == 0x1b && out[i + 1] >= '0' && out[i + 1] <= '7' ) out[i] = Q_COLOR_ESCAPE;
	}
	return out;
}

void QDECL IhvPrint( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof( text ), fmt, args ); va_end( args );
	Com_Printf( "%s", IhvText( text ).c_str() );
}
void QDECL IhvError( int level, const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args; va_start( args, fmt ); Q_vsnprintf( text, sizeof( text ), fmt, args ); va_end( args );
	// Com_Error 0x40cca0: 1 drops the session, 0 is fatal, 2 a disconnect.
	Com_Error( level == 0 ? ERR_FATAL : level == 2 ? ERR_SERVERDISCONNECT : ERR_DROP, "%s", IhvText( text ).c_str() );
}
int QDECL IhvMilliseconds() { return Sys_Milliseconds(); }

void IhvSyncCvar( IhvCvar *node ) {
	if ( !node->source || !node->source->name || Q_stricmp( node->name, node->source->name ) )
		node->source = Cvar_Get( node->name, node->reset, ihv::CvarFlagsToEngine( node->flags ) );
	const cvar_t *v = node->source;
	const int modified = node->serial == v->modificationCount ? node->mirror.Record()->modified : 1;
	if ( !node->mirror.Assign( v->name, v->string, v->resetString, v->latchedString,
		ihv::CvarFlagsToNative( v->flags, node->flags ), modified, v->value,
		node->next ? static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( node->next->mirror.Record() ) ) : 0 ) )
		Com_Error( ERR_DROP, "IHV cvar record cannot represent %s", node->name );
	node->serial = v->modificationCount;
}
void IhvSyncCvars() {
	for ( IhvCvar *node = ihvcg.cvars; node; node = node->next ) IhvSyncCvar( node );
}
fntech3_q3_ihv_cvar_t *QDECL IhvCvarGet( const char *name, const char *value, int flags ) {
	if ( !name || !*name || !value || strlen( name ) >= sizeof( IhvCvar::name ) || strlen( value ) >= sizeof( IhvCvar::reset ) )
		Com_Error( ERR_DROP, "Invalid IHV cvar registration" );
	if ( Cvar_Flags( name ) & CVAR_PRIVATE ) Com_Error( ERR_DROP, "IHV client game requested private cvar %s", name );
	IhvCvar *node = ihvcg.cvars;
	while ( node && Q_stricmp( node->name, name ) ) node = node->next;
	if ( !node ) {
		node = new IhvCvar;
		Q_strncpyz( node->name, name, sizeof( node->name ) );
		Q_strncpyz( node->reset, value, sizeof( node->reset ) );
		node->next = ihvcg.cvars;
		ihvcg.cvars = node;
	}
	node->flags |= flags;
	node->source = Cvar_Get( name, value, ihv::CvarFlagsToEngine( flags ) );
	IhvSyncCvar( node );
	return node->mirror.Record();
}
void QDECL IhvCvarSet( const char *name, const char *value ) {
	if ( !name || !value ) Com_Error( ERR_DROP, "Invalid IHV cvar assignment" );
	Cvar_Set2( name, value, qtrue );   // Cvar_Set 0x40e2b0 forces
	IhvSyncCvars();
}
int QDECL IhvArgc() { return Cmd_Argc(); }
char *QDECL IhvArgv( int n ) { ihvcg.argv = Cmd_Argv( n ); return const_cast<char *>( ihvcg.argv.c_str() ); }
char *QDECL IhvArgs() { ihvcg.args = Cmd_ArgsFrom( 1 ); return const_cast<char *>( ihvcg.args.c_str() ); }

/* FS_ReadFile 0x40f000: the module owns the buffer until it frees it. */
ihv::TaggedMemory ihvFiles( 64u * 1024u * 1024u );
int QDECL IhvReadFile( const char *name, void **out ) {
	if ( out ) *out = nullptr;
	if ( !name || !*name ) Com_Error( ERR_DROP, "Invalid IHV file read" );
	void *data = nullptr;
	const int length = FS_ReadFile( name, &data );
	if ( length < 0 || !data ) return -1;
	if ( !out ) { FS_FreeFile( data ); return length; }
	void *copy = ihvFiles.Allocate( length + 1, 0 );
	if ( !copy ) { FS_FreeFile( data ); Com_Error( ERR_DROP, "IHV file allocation rejected: %s", name ); }
	std::memcpy( copy, data, static_cast<std::size_t>( length ) );
	FS_FreeFile( data );
	*out = copy;
	return length;
}
void QDECL IhvFreeFile( void *data ) {
	if ( data && !ihvFiles.Free( data ) ) Com_Error( ERR_DROP, "IHV client game freed an unowned file" );
}
void QDECL IhvWriteFile( const char *name, const void *data, int length ) {
	if ( !name || length < 0 || ( length && !data ) ) Com_Error( ERR_DROP, "Invalid IHV file write" );
	FS_WriteFile( name, data, length );
}
void QDECL IhvAddCommandText( const char *text ) { if ( text ) Cbuf_AddText( text ); }
void QDECL IhvDebugGraph( float value, int ) { SCR_DebugGraph( value ); }
/* 0x404aa0: a client command is a reliable string (clc_stringcmd, 3). */
void QDECL IhvClientCommand( const char *text ) {
	if ( !text || clc.demoplaying ) return;
	CL_AddReliableCommand( text, qfalse );
}

/* ---- collision ----------------------------------------------------------- */

/* A trace's headnode is the engine's clip handle: 0 the world, 1.. the
 * inline models, the temporary box its own. The records hold it at word 0. */
fntech3_q3_ihv_collision_model_t *IhvInlineRecord( int index ) {
	if ( index < 0 || index >= static_cast<int>( ihvcg.inlineModels.size() ) )
		Com_Error( ERR_DROP, "CM_InlineModel: bad number" );
	return &ihvcg.inlineModels[static_cast<std::size_t>( index )];
}
void IhvBuildInlineModels() {
	ihvcg.inlineModels.clear();
	const int count = CM_NumInlineModels();
	ihvcg.inlineModels.resize( count > 0 ? static_cast<std::size_t>( count ) : 0 );
	for ( int i = 0; i < count; ++i ) {
		auto &record = ihvcg.inlineModels[static_cast<std::size_t>( i )];
		const clipHandle_t handle = CM_InlineModel( i );
		record.headnode = handle;
		vec3_t mins, maxs;
		CM_ModelBounds( handle, mins, maxs );
		VectorCopy( mins, record.mins );
		VectorCopy( maxs, record.maxs );
		Com_sprintf( record.name, sizeof( record.name ), "*%i", i );
	}
}
void QDECL IhvLoadMap( const char *name, int clientLoad, unsigned *checksum ) {
	if ( !name || !*name ) Com_Error( ERR_DROP, "Invalid IHV map load" );
	int sum = 0;
	CM_LoadMap( name, clientLoad ? qtrue : qfalse, &sum );
	if ( checksum ) *checksum = static_cast<unsigned>( sum );
	IhvBuildInlineModels();
}
int QDECL IhvMapLoaded( const char *name ) {
	return name && !Q_stricmp( name, cl.mapname ) ? 1 : 0;
}
fntech3_q3_ihv_collision_model_t *QDECL IhvInlineModel( int index ) { return IhvInlineRecord( index ); }
int QDECL IhvNumInlineModels() { return static_cast<int>( ihvcg.inlineModels.size() ); }
/* CM_RegisterModel 0x40a140: an MD3's frames and tags, which the module reads
 * through the record's pointers; any other name is an inline model. */
const fntech3_q3_ihv_collision_model_t *QDECL IhvCollisionModel( const char *name ) {
	if ( !name || !*name ) return nullptr;
	if ( name[0] == '*' ) {
		const int index = atoi( name + 1 );
		return index >= 0 && index < static_cast<int>( ihvcg.inlineModels.size() ) ? IhvInlineRecord( index ) : nullptr;
	}
	const std::string key( name );
	const auto found = ihvcg.md3s.find( key );
	if ( found != ihvcg.md3s.end() ) return found->second->Record();
	void *data = nullptr;
	const int length = FS_ReadFile( name, &data );
	if ( length < 0 || !data ) return nullptr;
	auto model = std::make_unique<ihv::NativeModelMetadata>();
	const bool ok = model->Load( key, data, static_cast<std::size_t>( length ) );
	FS_FreeFile( data );
	if ( !ok ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: IHV collision model %s rejected\n", name );
		return nullptr;
	}
	const auto *record = model->Record();
	ihvcg.md3s.emplace( key, std::move( model ) );
	return record;
}
/* CM_LerpTag 0x40a570: the full 112-byte tag, its name untouched, or null. */
fntech3_q3_ihv_tag_t *QDECL IhvLerpTag( const void *model, int start, int end, float fraction, const char *name ) {
	if ( !model || !name ) return nullptr;
	for ( const auto &entry : ihvcg.md3s ) {
		if ( entry.second->Record() != model ) continue;
		return entry.second->Metadata().Lerp( start, end, fraction, name, ihvcg.tag ) ? &ihvcg.tag : nullptr;
	}
	return nullptr;
}
int QDECL IhvTempBox( const float *mins, const float *maxs ) {
	if ( !mins || !maxs ) Com_Error( ERR_DROP, "Invalid IHV temporary box" );
	return CM_TempBoxModel( mins, maxs, qfalse );
}
/* CM_BoxBrushes 0x40aa90: pointers to the brushes the box touches, which the
 * module clips its marks against. They are read in the original's 40-byte
 * shape -- contents, bounds, side count at 0x1c, sides at 0x20, each a plane
 * pointer and surface flags (cgamex86.dll 0x1000b2c6) -- the shape Q3Test 1.05
 * kept, so the same mirror serves both (cm_test.c). */
int QDECL IhvBoxBrushes( const float *mins, const float *maxs, void **list, int capacity, int *topnode ) {
	if ( !mins || !maxs || capacity < 0 || ( capacity && !list ) ) Com_Error( ERR_DROP, "Invalid IHV brush query" );
	int top = -1;
	const int count = CM_Q3TestBoxBrushes( mins, maxs, list, capacity, &top );
	if ( topnode ) *topnode = top;
	return count;
}
int QDECL IhvPointContents( const float *point, int headnode ) {
	if ( !point ) Com_Error( ERR_DROP, "Invalid IHV point contents" );
	return CM_PointContents( point, headnode );
}
int QDECL IhvTransformedContents( const float *point, int headnode, const float *origin, const float *angles ) {
	if ( !point || !origin || !angles ) Com_Error( ERR_DROP, "Invalid IHV point contents" );
	return CM_TransformedPointContents( point, headnode, origin, angles );
}
fntech3_q3_ihv_trace_t IhvTrace( const trace_t &trace ) {
	fntech3_q3_ihv_trace_t result{};
	result.allsolid = trace.allsolid; result.startsolid = trace.startsolid; result.fraction = trace.fraction;
	VectorCopy( trace.endpos, result.endpos ); VectorCopy( trace.plane.normal, result.planeNormal );
	result.planeDist = trace.plane.dist; result.planeType = trace.plane.type; result.planeSignbits = trace.plane.signbits;
	result.surfaceFlags = trace.surfaceFlags; result.contents = trace.contents; result.entityNum = trace.entityNum;
	return result;
}
const float *IhvVector( const float *value ) {
	static const vec3_t zero = { 0, 0, 0 };
	return value ? value : zero;
}
/* CM_BoxTrace 0x40ba80: null extents are a point, its record returned by value. */
fntech3_q3_ihv_trace_t QDECL IhvBoxTrace( const float *start, const float *end, const float *mins, const float *maxs,
	int headnode, int mask ) {
	if ( !start || !end ) Com_Error( ERR_DROP, "Invalid IHV trace" );
	trace_t trace{};
	CM_BoxTrace( &trace, start, end, IhvVector( mins ), IhvVector( maxs ), headnode, mask, qfalse );
	return IhvTrace( trace );
}
fntech3_q3_ihv_trace_t QDECL IhvTransformedTrace( const float *start, const float *end, const float *mins,
	const float *maxs, int headnode, int mask, const float *origin, const float *angles ) {
	if ( !start || !end || !origin || !angles ) Com_Error( ERR_DROP, "Invalid IHV trace" );
	trace_t trace{};
	CM_TransformedBoxTrace( &trace, start, end, IhvVector( mins ), IhvVector( maxs ), headnode, mask, origin, angles, qfalse );
	return IhvTrace( trace );
}

/* ---- sound ----------------------------------------------------------------- */

std::uint32_t IhvSoundToken( sfxHandle_t sfx ) { return IhvToken( IhvKind::Sound, sfx ); }
bool IhvSound( std::uintptr_t token, sfxHandle_t &sfx ) {
	int handle = 0;
	if ( !IhvHandle( IhvKind::Sound, static_cast<std::uint32_t>( token ), handle ) ) return false;
	sfx = handle;
	return true;
}
void QDECL IhvStartSound( const float *origin, int entity, int channel, std::uintptr_t token ) {
	sfxHandle_t sfx;
	if ( !IhvSound( token, sfx ) ) return;
	if ( entity < 0 || entity >= MAX_GENTITIES ) return;
	ihvcg.soundEntities[entity] = cls.realtime;
	S_StartSound( origin ? const_cast<float *>( origin ) : nullptr, entity, channel, sfx );
}
void QDECL IhvLocalSound( const char *name ) {
	if ( !name || !*name ) return;
	S_StartLocalSound( S_RegisterSound( name, qfalse ), CHAN_LOCAL_SOUND );
}
void QDECL IhvClearLoops() {
	ihvcg.loopCount = 0;
	S_ClearLoopingSounds( qfalse );
}
/* Each loop is a position of its own (S_AddLoopingSound 0x415d10); the
 * engine's loops are kept per entity, so each takes the next number. */
void QDECL IhvAddLoop( const float *origin, const float *velocity, std::uintptr_t token ) {
	sfxHandle_t sfx;
	if ( !origin || !IhvSound( token, sfx ) || ihvcg.loopCount >= MAX_GENTITIES ) return;
	S_AddLoopingSound( ihvcg.loopCount++, origin, IhvVector( velocity ), sfx );
}
/* The listener, then every sounding entity's position from the module's own
 * callback, as the original walks its channels (0x416200). */
void QDECL IhvRespatialize( int entity, const float *origin, const float *axis,
	void ( QDECL *position )( int, float * ) ) {
	if ( !origin || !axis ) return;
	vec3_t view[3];
	std::memcpy( view, axis, sizeof( view ) );
	ihvcg.listener = entity;
	S_Respatialize( entity, origin, view, 0 );
	if ( !position ) return;
	for ( auto it = ihvcg.soundEntities.begin(); it != ihvcg.soundEntities.end(); ) {
		if ( cls.realtime - it->second > 30000 ) { it = ihvcg.soundEntities.erase( it ); continue; }
		vec3_t where{};
		position( it->first, where );
		S_UpdateEntityPosition( it->first, where );
		++it;
	}
}
void QDECL IhvSoundBegin() { S_BeginRegistration(); }
std::uintptr_t QDECL IhvRegisterSound( const char *name ) {
	if ( !name || !*name ) return 0;
	return IhvSoundToken( S_RegisterSound( name, qfalse ) );
}
void QDECL IhvSoundEnd() {}
void QDECL IhvMusic( const char *name ) {
	S_StartBackgroundTrack( name ? name : "", name ? name : "" );
}

/* ---- renderer and screen ------------------------------------------------- */

void QDECL IhvLoadWorld( const char *name ) {
	if ( !name || !*name ) Com_Error( ERR_DROP, "Invalid IHV world" );
	re.LoadWorld( name );
}
std::map<int, std::string> ihvModelNames;   // for cl_ihvSceneDump
std::uintptr_t QDECL IhvRegisterModel( const char *name ) {
	if ( !name || !*name ) return 0;
	const qhandle_t handle = re.RegisterModel( name );
	if ( handle ) ihvModelNames[handle] = name;
	return handle ? IhvToken( IhvKind::Model, handle ) : 0;
}
std::uintptr_t QDECL IhvRegisterSkin( const char *name ) {
	if ( !name || !*name ) return 0;
	const qhandle_t handle = re.RegisterSkin( name );
	return handle ? IhvToken( IhvKind::Skin, handle ) : 0;
}
std::uintptr_t QDECL IhvRegisterShader( const char *name ) {
	if ( !name || !*name ) return 0;
	return IhvToken( IhvKind::Shader, re.RegisterShader( name ) );
}
qhandle_t IhvShader( std::uintptr_t token ) {
	int handle = 0;
	return IhvHandle( IhvKind::Shader, static_cast<std::uint32_t>( token ), handle ) ? handle : 0;
}

/* The module's scene is read where it lies, every array checked against the
 * memory it names before a byte of it is used. */
struct IhvSceneMemory final : ihvrender::Memory {
	bool Read( std::uint32_t address, void *output, std::size_t bytes ) const override {
		const auto *data = reinterpret_cast<const unsigned char *>( static_cast<std::uintptr_t>( address ) );
#ifdef _WIN32
		std::size_t done = 0;
		while ( done < bytes ) {
			MEMORY_BASIC_INFORMATION region{};
			const unsigned char *at = data + done;
			if ( VirtualQuery( at, &region, sizeof( region ) ) != sizeof( region ) || region.State != MEM_COMMIT
				|| ( region.Protect & ( PAGE_GUARD | PAGE_NOACCESS ) ) ) return false;
			const auto offset = static_cast<std::size_t>( at - static_cast<const unsigned char *>( region.BaseAddress ) );
			if ( offset >= region.RegionSize ) return false;
			const std::size_t length = ( std::min )( bytes - done, static_cast<std::size_t>( region.RegionSize - offset ) );
			std::memcpy( static_cast<unsigned char *>( output ) + done, at, length );
			done += length;
		}
		return true;
#else
		if ( !data ) return false;
		std::memcpy( output, data, bytes );
		return true;
#endif
	}
};
struct IhvSceneResources final : ihvrender::Resources {
	bool Resolve( ihvrender::ResourceKind kind, std::uint32_t token, std::int32_t &handle ) const override {
		const IhvKind ours = kind == ihvrender::ResourceKind::Model ? IhvKind::Model
			: kind == ihvrender::ResourceKind::Skin ? IhvKind::Skin : IhvKind::Shader;
		int value = 0;
		if ( !IhvHandle( ours, token, value ) ) return false;
		handle = value;
		return true;
	}
};

/* R_RenderScene 0x42cce0: one complete scene a call. Its view's bit 0 lets it
 * draw without a world (0x42cd11); its area bits admit an area where retail's
 * block one. The overlay quad (0x442700) is not drawn yet. */
void QDECL IhvRenderScene( const void *native ) {
	if ( !native ) Com_Error( ERR_DROP, "IHV scene is null" );
	ihvrender::Scene scene;
	const auto result = ihvrender::Capture( static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( native ) ),
		IhvSceneMemory{}, IhvSceneResources{}, scene );
	if ( result != ihvrender::Result::Ok ) {
		Com_DPrintf( S_COLOR_YELLOW "IHV scene refused (%i)\n", static_cast<int>( result ) );
		return;
	}
	static cvar_t *dump;
	if ( !dump ) dump = Cvar_Get( "cl_ihvSceneDump", "0", CVAR_TEMP );
	if ( dump->integer > 0 ) {
		Cvar_SetIntegerValue( "cl_ihvSceneDump", dump->integer - 1 );
		Com_Printf( "IHV scene %ix%i fov %.1f flags %u: %i entities, %i lights, %i particles, %i polys\n",
			scene.view.width, scene.view.height, scene.view.fovX, scene.view.flags,
			static_cast<int>( scene.entities.size() ), static_cast<int>( scene.lights.size() ),
			static_cast<int>( scene.particles.size() ), static_cast<int>( scene.polygons.size() ) );
		for ( const auto &e : scene.entities ) {
			const auto name = ihvModelNames.find( e.model );
			Com_Printf( "  model %i %s skin %i shader %i flags 0x%x frame %i rgba %i %i %i %i at %.0f %.0f %.0f\n", e.model,
				name != ihvModelNames.end() ? name->second.c_str() : "-", e.skin, e.shader, e.flags, e.frame,
				e.rgba[0], e.rgba[1], e.rgba[2], e.rgba[3], e.origin[0], e.origin[1], e.origin[2] );
		}
		for ( std::size_t i = 0; i < scene.particles.size() && i < 4; ++i ) {
			const auto &p = scene.particles[i];
			Com_Printf( "  particle %.0f %.0f %.0f rgba %i %i %i %i\n", p.origin[0], p.origin[1], p.origin[2],
				p.rgba[0], p.rgba[1], p.rgba[2], p.rgba[3] );
		}
	}
	re.ClearScene();
	if ( !scene.entities.empty() ) {
		std::vector<fnQ3IHVEntity_t> entities( scene.entities.size() );
		for ( std::size_t i = 0; i < entities.size(); ++i ) {
			const auto &from = scene.entities[i];
			auto &to = entities[i];
			to.model = from.model; to.skin = from.skin; to.shader = from.shader;
			std::memcpy( to.axis, from.axis, sizeof( to.axis ) );
			VectorCopy( from.origin, to.origin ); VectorCopy( from.oldorigin, to.oldorigin );
			to.backlerp = from.backlerp; to.frame = from.frame; to.oldframe = from.oldframe; to.skinNum = from.skinNum;
			to.nonNormalizedAxes = from.nonNormalizedAxes ? 1 : 0;
			to.flags = from.flags;
			std::memcpy( to.rgba, from.rgba.data(), 4 );
		}
		if ( !re.AddIHVEntitiesToScene( static_cast<int>( entities.size() ), entities.data() ) )
			Com_DPrintf( S_COLOR_YELLOW "IHV entities refused by the renderer\n" );
	}
	if ( !scene.particles.empty() ) {
		std::vector<fnQ3IHVParticle_t> particles( scene.particles.size() );
		for ( std::size_t i = 0; i < particles.size(); ++i ) {
			VectorCopy( scene.particles[i].origin, particles[i].origin );
			std::memcpy( particles[i].rgba, scene.particles[i].rgba.data(), 4 );
		}
		if ( !re.AddIHVParticlesToScene( static_cast<int>( particles.size() ), particles.data() ) )
			Com_DPrintf( S_COLOR_YELLOW "IHV particles refused by the renderer\n" );
	}
	for ( const auto &light : scene.lights ) {
		re.AddLightToScene( light.origin, light.radius, light.color[0], light.color[1], light.color[2] );
	}
	for ( const auto &polygon : scene.polygons ) {
		std::vector<polyVert_t> vertices( polygon.vertices.size() );
		for ( std::size_t v = 0; v < vertices.size(); ++v ) {
			VectorCopy( polygon.vertices[v].xyz, vertices[v].xyz );
			vertices[v].st[0] = polygon.vertices[v].st[0];
			vertices[v].st[1] = polygon.vertices[v].st[1];
			static_assert( sizeof( vertices[v].modulate ) == 4, "polyVert_t colour" );
			std::memcpy( &vertices[v].modulate, polygon.vertices[v].rgba.data(), 4 );
		}
		re.AddPolyToScene( polygon.shader, static_cast<int>( vertices.size() ), vertices.data(), 1 );
	}
	refdef_t view{};
	view.x = scene.view.x; view.y = scene.view.y;
	view.width = scene.view.width; view.height = scene.view.height;
	view.fov_x = scene.view.fovX; view.fov_y = scene.view.fovY;
	VectorCopy( scene.view.origin, view.vieworg );
	std::memcpy( view.viewaxis, scene.view.axis, sizeof( view.viewaxis ) );
	view.time = scene.view.time;
	if ( scene.view.flags & 1 ) view.rdflags |= RDF_NOWORLDMODEL;
	const auto blocked = scene.view.BlockedAreas();
	std::memcpy( view.areamask, blocked.data(), ( std::min )( sizeof( view.areamask ), blocked.size() ) );
	re.RenderScene( &view );
}
void QDECL IhvSetColor( const float *rgba ) { re.SetColor( rgba ); }
void QDECL IhvStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, std::uintptr_t shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, IhvShader( shader ) );
}
/* Only the module's refresh timing calls these two (cgamex86.dll 0x1000969a). */
void QDECL IhvBeginFrame() { re.BeginFrame( STEREO_CENTER ); }
void QDECL IhvEndFrame() { re.EndFrame( nullptr, nullptr ); }
void QDECL IhvUpdateScreen() { SCR_UpdateScreen(); }
/* 0x407640/0x407620: printable characters, sixteen pixels each. */
int IhvVisibleLength( const char *text ) {
	int length = 0;
	for ( const char *s = text ? text : ""; *s; ) {
		if ( s[0] == 0x1b && s[1] >= '0' && s[1] <= '7' ) { s += 2; continue; }
		++length; ++s;
	}
	return length;
}
int QDECL IhvBigStringWidth( const char *text ) { return IhvVisibleLength( text ) * BIGCHAR_WIDTH; }
/* 0x407670: eight pixels a character, in the screen's own units. */
int QDECL IhvSmallStringWidth( const char *text ) {
	return cls.glconfig.vidWidth > 0 ? IhvVisibleLength( text ) * 8 * 640 / cls.glconfig.vidWidth : 0;
}
void QDECL IhvAdjustFrom640( float *x, float *y, float *w, float *h ) { SCR_AdjustFrom640( x, y, w, h ); }
void QDECL IhvFillRect( float x, float y, float w, float h, const float *color ) { SCR_FillRect( x, y, w, h, color ); }
void QDECL IhvDrawPic( float x, float y, float w, float h, std::uintptr_t shader ) { SCR_DrawPic( x, y, w, h, IhvShader( shader ) ); }
void QDECL IhvBigString( int x, int y, const char *text, float alpha ) {
	SCR_DrawBigString( x, y, IhvText( text ).c_str(), alpha, qfalse );
}
void QDECL IhvBigStringColor( int x, int y, const char *text, const float *color ) {
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, IhvText( text ).c_str(), color, qtrue, qfalse );
}

/* ---- the session's records ------------------------------------------------- */

const ihvcl::ClientData &IhvData() {
	const ihvcl::ClientData *data = ihvcl::SessionData();
	if ( !data || !data->Ready() ) Com_Error( ERR_DROP, "IHV client game has no gamestate" );
	return *data;
}
void QDECL IhvGameState( void *out ) {
	if ( !out || !IhvData().CopyGameState( out, sizeof( fntech3_q3_ihv_gameState_t ) ) )
		Com_Error( ERR_DROP, "IHV gamestate copy failed" );
}
/* 0x405dd0: an entity of the history the snapshots filled; a future one is an error. */
int QDECL IhvEntity( int number, void *out ) {
	const auto result = IhvData().CopyEntity( static_cast<std::uint32_t>( number ), out, sizeof( fntech3_q3_ihv_entityState_t ) );
	if ( result == ihvcl::ReadResult::FutureEntity ) Com_Error( ERR_DROP, "CL_GetParseEntityState: %i >= %i", number,
		static_cast<int>( IhvData().EntitySequence() ) );
	if ( result == ihvcl::ReadResult::InvalidOutput ) Com_Error( ERR_DROP, "IHV entity copy has no destination" );
	return result == ihvcl::ReadResult::Ok ? 1 : 0;
}
int QDECL IhvCurrentCommand() { return static_cast<int>( IhvData().CurrentCommandNumber() ); }
int QDECL IhvCommand( int number, void *out ) {
	return IhvData().CopyCommand( static_cast<std::uint32_t>( number ), out, sizeof( fntech3_q3_ihv_usercmd_t ) ) == ihvcl::ReadResult::Ok ? 1 : 0;
}

template<class F> std::intptr_t Slot( F *function ) { return reinterpret_cast<std::intptr_t>( function ); }

/* The table CL_InitCGame builds (0x405e30..0x4060a1). Words 0 and 57 are
 * null there too; 37 and 38 are the video width and height themselves. */
void IhvFillImports() {
	auto &t = ihvcg.imports;
	t = {};
	t[1] = Slot( IhvPrint ); t[2] = Slot( IhvError ); t[3] = Slot( IhvMilliseconds );
	t[4] = Slot( IhvCvarGet ); t[5] = Slot( IhvCvarSet ); t[6] = Slot( IhvArgc ); t[7] = Slot( IhvArgv );
	t[8] = Slot( IhvArgs ); t[9] = Slot( IhvReadFile ); t[10] = Slot( IhvFreeFile ); t[11] = Slot( IhvWriteFile );
	t[12] = Slot( IhvAddCommandText ); t[13] = Slot( IhvDebugGraph ); t[14] = Slot( IhvClientCommand );
	t[15] = Slot( IhvTempBox ); t[16] = Slot( IhvBoxBrushes ); t[17] = Slot( IhvLerpTag ); t[18] = Slot( IhvLoadMap );
	t[19] = Slot( IhvMapLoaded ); t[20] = Slot( IhvInlineModel ); t[21] = Slot( IhvCollisionModel );
	t[22] = Slot( IhvNumInlineModels ); t[23] = Slot( IhvPointContents ); t[24] = Slot( IhvTransformedContents );
	t[25] = Slot( IhvBoxTrace ); t[26] = Slot( IhvTransformedTrace );
	t[27] = Slot( IhvStartSound ); t[28] = Slot( IhvLocalSound ); t[29] = Slot( IhvClearLoops ); t[30] = Slot( IhvAddLoop );
	t[31] = Slot( IhvRespatialize ); t[32] = Slot( IhvSoundBegin ); t[33] = Slot( IhvRegisterSound );
	t[34] = Slot( IhvRegisterSound ); t[35] = Slot( IhvSoundEnd ); t[36] = Slot( IhvMusic );
	t[37] = cls.glconfig.vidWidth; t[38] = cls.glconfig.vidHeight;
	t[39] = Slot( IhvLoadWorld ); t[40] = Slot( IhvRegisterModel ); t[41] = Slot( IhvRegisterSkin );
	t[42] = Slot( IhvRegisterShader ); t[43] = Slot( IhvRenderScene ); t[44] = Slot( IhvSetColor );
	t[45] = Slot( IhvStretchPic ); t[46] = Slot( IhvBeginFrame ); t[47] = Slot( IhvEndFrame );
	t[48] = Slot( IhvUpdateScreen ); t[49] = Slot( IhvBigStringWidth ); t[50] = Slot( IhvSmallStringWidth );
	t[51] = Slot( IhvAdjustFrom640 ); t[52] = Slot( IhvFillRect ); t[53] = Slot( IhvDrawPic );
	t[54] = Slot( IhvBigString ); t[55] = Slot( IhvBigStringColor ); t[56] = Slot( IhvGameState );
	t[58] = Slot( IhvEntity ); t[59] = Slot( IhvCurrentCommand ); t[60] = Slot( IhvCommand );
	t[61] = reinterpret_cast<std::intptr_t>( &ihvcg.weapon );
}

/* ---- exports --------------------------------------------------------------- */

template<class R, class... A> R IhvExport( int word, A... args ) {
	using Fn = R ( QDECL * )( A... );
	return reinterpret_cast<Fn>( ihvcg.exports[word] )( args... );
}

/* The engine's calls in its own numbering. The draw is export 6 once the
 * level is up and export 7, the loading screen, before (SCR_UpdateScreen's
 * states 5-7, 0x4079c1/0x407a11); the console command reaches the module
 * only while active (0x4060e0). There is no shutdown export. */
intptr_t QDECL CL_Q3IHVCGameEntry( int command, ... ) {
	va_list args;
	va_start( args, command );
	const intptr_t a0 = va_arg( args, intptr_t ), a1 = va_arg( args, intptr_t );
	va_end( args );
	(void)a0;
	if ( !ihvcg.exports || !ihvcg.initialized ) return 0;
	switch ( command ) {
	case CG_CONSOLE_COMMAND:
		if ( cls.state != CA_ACTIVE ) return 0;
		IhvSyncCvars();
		return IhvExport<int>( 4 ) ? 1 : 0;
	case CG_DRAW_ACTIVE_FRAME:
		IhvSyncCvars();
		if ( cls.state == CA_ACTIVE ) {
			const stereoFrame_t stereo = static_cast<stereoFrame_t>( a1 );
			const float separation = stereo == STEREO_LEFT ? -1.0f : stereo == STEREO_RIGHT ? 1.0f : 0.0f;
			IhvExport<void>( 6, separation );
		} else {
			IhvExport<void>( 7 );
		}
		return 0;
	case CG_CROSSHAIR_PLAYER:
	case CG_LAST_ATTACKER:
		return -1;
	default:
		return 0;
	}
}

void IhvClear() {
	while ( ihvcg.cvars ) { IhvCvar *node = ihvcg.cvars; ihvcg.cvars = node->next; delete node; }
	ihvcg = IhvCGame{};
	ihvFiles.Clear();
}

} // namespace

/* Loads the client game (Sys_GetCGameAPI, 0x425cb0). */
vm_t *CL_Q3IHVLoadCGame( void ) {
	// The supplied library is 32-bit; so must the process be that loads it.
	if ( sizeof( void * ) != 4 ) {
		Com_Error( ERR_DROP, "The IHV client game is a Win32 x86 library; this build cannot load it" );
		return nullptr;
	}
	IhvClear();
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_CGAME, "GetCGameAPI", CL_Q3IHVCGameEntry, &symbol );
	if ( !vm ) return nullptr;
	auto getter = reinterpret_cast<const std::intptr_t *( QDECL * )( void )>( symbol );
	ihvcg.exports = getter();
	if ( !ihvcg.exports ) {
		VM_Free( vm );
		Com_Error( ERR_FATAL, "Sys_GetCGameAPI failed" );
		return nullptr;
	}
	IhvFillImports();
	return vm;
}

/* Export 0 with the table (0x4060cc); the module registers its cvars, loads
 * the map's collision and world, and its sounds, models and pictures. */
void CL_Q3IHVInitCGameModule( void ) {
	if ( !ihvcg.exports ) return;
	ihvcg.initialized = true;
	IhvExport<void>( 0, ihvcg.imports.data() );
	IhvSyncCvars();
}

void CL_Q3IHVShutdownCGame( void ) {
	IhvClear();
}

bool CL_Q3IHVCGameReady( void ) {
	return ihvcg.exports && ihvcg.initialized;
}

/* Export 1 with the frame record (0x406c2f), which stays where it is until the next. */
void CL_Q3IHVCGameSnapshot( const fntech3_q3_ihv_snapshot_t *snapshot ) {
	if ( !CL_Q3IHVCGameReady() || !snapshot ) return;
	IhvSyncCvars();
	IhvExport<void>( 1, snapshot );
}

/* Export 2 with the changed index (0x406f4d). */
void CL_Q3IHVCGameConfigstring( int index ) {
	if ( !CL_Q3IHVCGameReady() ) return;
	IhvSyncCvars();
	IhvExport<void>( 2, index );
}

/* Export 3 with the command already tokenized (0x40705f). */
void CL_Q3IHVCGameServerCommand( void ) {
	if ( !CL_Q3IHVCGameReady() ) return;
	IhvSyncCvars();
	IhvExport<void>( 3 );
}

/* Export 5 (CL_CgameFrame, 0x4059b0): the time, and 1 while a demo plays. */
bool CL_Q3IHVCGameFrame( int realtime, bool demo ) {
	if ( !CL_Q3IHVCGameReady() ) return true;
	IhvSyncCvars();
	return IhvExport<int>( 5, realtime, demo ? 1 : 0 ) != 0;
}

int CL_Q3IHVCGameWeapon( void ) {
	return ihvcg.weapon;
}
