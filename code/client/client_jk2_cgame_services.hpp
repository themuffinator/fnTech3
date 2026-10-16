/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_CGAME_SERVICES_HPP
#define FNTECH3_CLIENT_JK2_CGAME_SERVICES_HPP

#include "../compat/jk2_client_abi.h"
#include "../compat/jk2_render_adapter.hpp"
#include "client_jka_command_values.hpp"
#include "client_jk2_strings.hpp"
#include <array>
#include <climits>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <utility>
#include <vector>

/* Jedi Outcast 1.02a's client-game services, each read from its handler in
 * jk2mp.exe's dispatcher (0x4017a0, index 0x4031a0, jump table 0x402f14;
 * .tmp/jk2-cgame holds the census and the disassembly). The retail client
 * game is a QVM, so every address it passes is an offset into its own data:
 * a record is bounds-checked whole and copied, never handed on as a pointer.
 * Its trace is 1080 bytes: Quake III's record, then a Ghoul2 collision map a
 * world trace leaves zero (jk2_client_abi.h). Its mark fragment, orientation
 * and polygon vertex are Quake III's (jk2_render_abi.h). Imports that need an
 * engine service Fn does not have for this title yet are refused by name.
 */
enum class JK2CgameService { None, Handled, Shared };

namespace {
// The module address import 300 registered (retail keeps it at 0x9d9e24).
intptr_t jk2CgameSharedAddress = 0;
static void CL_JKACgameEffectsClear();

// Declared beside CL_JKACgameClear in cl_cgame.cpp's own unnamed namespace.
static void CL_JK2CgameClear() {
	fntech3::jka::command::selections.Reset();
	jk2CgameSharedAddress = 0;
	CL_JKACgameEffectsClear(); // the effects engine Outcast shares (client_jk2_cgame_effects.hpp)
}
} // namespace

static bool CL_JK2CgameEffectsCall( intptr_t *args, intptr_t &result );

static void *CL_JK2CgameSpan( intptr_t address, std::size_t bytes ) {
	if ( !cgvm || !address || bytes > ( std::numeric_limits<std::uintptr_t>::max )() - std::uintptr_t( address ) )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame memory span" );
	// QVM arguments are offsets. Validate their full width before the shared
	// bounds check narrows them to unsigned; native modules use pointers.
	if ( !cgvm->entryPoint && !cgvm->dllExports
		&& ( !cgvm->dataBase || std::uintptr_t( address ) > ( std::numeric_limits<unsigned int>::max )()
			|| bytes > ( std::numeric_limits<unsigned int>::max )() ) )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame memory offset" );
	VM_CHECKBOUNDS( cgvm, static_cast<unsigned int>( address ), static_cast<unsigned int>( bytes ) );
	return VM_ExplicitArgPtr( cgvm, address );
}

static const float *CL_JK2CgameVector( intptr_t address, bool optional = false ) {
	if ( !address && optional ) return nullptr;
	return static_cast<const float *>( CL_JK2CgameSpan( address, sizeof( vec3_t ) ) );
}

// A module string that must end inside the module's data within limit bytes.
static const char *CL_JK2CgameString( intptr_t address, std::size_t limit = MAX_STRING_CHARS ) {
	const auto *text = static_cast<const char *>( CL_JK2CgameSpan( address, 1 ) );
	for ( std::size_t n = 0; n < limit; ++n ) {
		if ( std::uintptr_t( address ) > ( std::numeric_limits<std::uintptr_t>::max )() - n )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame string address" );
		if ( !*static_cast<const char *>( CL_JK2CgameSpan( address + static_cast<intptr_t>( n ), 1 ) ) ) return text;
	}
	Com_Error( ERR_DROP, "Jedi Outcast cgame string exceeds %u bytes", static_cast<unsigned>( limit - 1 ) );
	return "";
}

// A null name reads as empty; longer names are cut at 63 characters, as
// retail's Q_strncpyz( ..., 64 ) does for the music names (0x45e2e9/0x45e2f6).
static std::array<char, MAX_QPATH> CL_JK2CgameName( intptr_t address ) {
	std::array<char, MAX_QPATH> name{};
	if ( !address ) return name;
	for ( std::size_t n = 0; n + 1 < name.size(); ++n ) {
		if ( std::uintptr_t( address ) > ( std::numeric_limits<std::uintptr_t>::max )() - n )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame name address" );
		name[n] = *static_cast<const char *>( CL_JK2CgameSpan( address + static_cast<intptr_t>( n ), 1 ) );
		if ( !name[n] ) break;
	}
	return name;
}

/* Import 52, read-character 0x473e50: retail reads the module's string
 * pointer as a host pointer; from the QVM it is an offset into the module's
 * data, read here within it. Outside Asian languages retail returns the byte
 * and steps one byte (0x473eac..0x473eb2); hoststrings::Language refuses the
 * Asian ones. */
static intptr_t CL_JK2CgameReadChar( intptr_t cursorAddress ) {
	fntech3::jk2::hoststrings::Language();
	void *cursor = CL_JK2CgameSpan( cursorAddress, sizeof( std::int32_t ) );
	std::int32_t text;
	std::memcpy( &text, cursor, sizeof( text ) );
	const intptr_t character = *static_cast<const unsigned char *>( CL_JK2CgameSpan( text, 1 ) );
	++text;
	std::memcpy( cursor, &text, sizeof( text ) );
	return character;
}

// Quake III's 56 bytes, then the zero Ghoul2 collision map a world trace
// leaves (jk2_client_abi.h).
static fntech3_jk2_mp_trace_t CL_JK2CgameTraceRecord( const trace_t &traced ) {
	static_assert( sizeof( trace_t ) == offsetof( fntech3_jk2_mp_trace_t, ghoul2Collisions ),
		"Outcast's trace begins with Quake III's record" );
	fntech3_jk2_mp_trace_t record{};
	std::memcpy( &record, &traced, sizeof( traced ) );
	return record;
}

static void CL_JK2CgameTrace( const intptr_t *args, bool transformed, qboolean capsule ) {
	void *destination = CL_JK2CgameSpan( args[1], sizeof( fntech3_jk2_mp_trace_t ) );
	const float *start = CL_JK2CgameVector( args[2] ), *end = CL_JK2CgameVector( args[3] );
	const float *mins = CL_JK2CgameVector( args[4], true ), *maxs = CL_JK2CgameVector( args[5], true );
	trace_t traced{};
	if ( transformed )
		CM_TransformedBoxTrace( &traced, start, end, mins, maxs, static_cast<clipHandle_t>( args[6] ),
			static_cast<int>( args[7] ), CL_JK2CgameVector( args[8] ), CL_JK2CgameVector( args[9] ), capsule );
	else
		CM_BoxTrace( &traced, start, end, mins, maxs, static_cast<clipHandle_t>( args[6] ),
			static_cast<int>( args[7] ), capsule );
	const auto record = CL_JK2CgameTraceRecord( traced );
	std::memcpy( destination, &record, sizeof( record ) );
}

// Bounds the vector arguments of a shared sound import, which reads them
// through the module pointer itself.
static JK2CgameService CL_JK2CgameSharedSound( const intptr_t *args, intptr_t &operation, intptr_t shared,
	std::initializer_list<std::pair<int, std::size_t>> records ) {
	for ( const auto &record : records ) CL_JK2CgameSpan( args[record.first], record.second );
	operation = shared;
	return JK2CgameService::Shared;
}

static JK2CgameService CL_JK2CgameSharedString( const intptr_t *args, intptr_t &operation, intptr_t shared,
	std::initializer_list<int> strings ) {
	for ( const int index : strings ) CL_JK2CgameString( args[index] );
	operation = shared;
	return JK2CgameService::Shared;
}

/* Outcast's parser imports 238-241 reach botlib's load, free, read and
 * locate slots, +0x200/+0x204/+0x208/+0x20c (handlers 0x40243a, 0x40245b,
 * 0x402474, 0x402499), the slots its UI reaches at 64-67. Loading takes
 * Raven's @-name opt-in as the UI's does (client_jk2_ui.hpp). */
static void CL_JK2CgameParserCall( const intptr_t *args, intptr_t &result ) {
	if ( !botlib_export ) Com_Error( ERR_DROP, "Jedi Outcast cgame parser is unavailable" );
	if ( args[0] == 238 ) {
		const char *name = CL_JK2CgameString( args[1], MAX_QPATH );
		if ( !botlib_export->PC_LoadJKASourceHandle ) Com_Error( ERR_DROP, "Jedi Outcast cgame source parser is unavailable" );
		result = botlib_export->PC_LoadJKASourceHandle( name );
		return;
	}
	if ( args[1] < INT_MIN || args[1] > INT_MAX ) Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame parser handle" );
	if ( args[0] == 240 ) {
		void *destination = CL_JK2CgameSpan( args[2], sizeof( fntech3_jk2_mp_pc_token_t ) );
		if ( !botlib_export->PC_ReadTokenHandle ) Com_Error( ERR_DROP, "Jedi Outcast cgame token reader is unavailable" );
		pc_token_t internal{};
		result = botlib_export->PC_ReadTokenHandle( static_cast<int>( args[1] ), &internal );
		if ( result ) {
			const auto *end = static_cast<const char *>( std::memchr( internal.string, 0, sizeof( internal.string ) ) );
			if ( !end || end - internal.string >= 1024 ) Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame parser token" );
			fntech3_jk2_mp_pc_token_t external{};
			external.type = internal.type; external.subtype = internal.subtype; external.intvalue = internal.intvalue;
			external.floatvalue = internal.floatvalue;
			std::memcpy( external.string, internal.string, std::size_t( end - internal.string ) + 1 );
			std::memcpy( destination, &external, sizeof( external ) );
		}
		return;
	}
	void *filename = CL_JK2CgameSpan( args[2], FNTECH3_JK2_MP_SOURCE_FILENAME_CHARS );
	void *line = CL_JK2CgameSpan( args[3], sizeof( std::int32_t ) );
	if ( !botlib_export->PC_SourceFileAndLine ) Com_Error( ERR_DROP, "Jedi Outcast cgame source diagnostics are unavailable" );
	std::array<char, MAX_STRING_CHARS> internal{};
	int internalLine = 0;
	result = botlib_export->PC_SourceFileAndLine( static_cast<int>( args[1] ), internal.data(), &internalLine );
	if ( result ) {
		const auto *end = static_cast<const char *>( std::memchr( internal.data(), 0, internal.size() ) );
		if ( !end || end - internal.data() >= FNTECH3_JK2_MP_SOURCE_FILENAME_CHARS || internalLine < 0 )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame source diagnostic record" );
		const std::int32_t externalLine = internalLine;
		std::memcpy( filename, internal.data(), std::size_t( end - internal.data() ) + 1 );
		std::memcpy( line, &externalLine, sizeof( externalLine ) );
	}
}

static JK2CgameService CL_JK2CgameServiceCall( intptr_t *args, intptr_t &operation, intptr_t &result ) {
	switch ( args[0] ) {
	case 22: case 23: // 0x401a40/0x401a55: CM_TempBoxModel, box then capsule.
		result = CM_TempBoxModel( CL_JK2CgameVector( args[1] ), CL_JK2CgameVector( args[2] ), args[0] == 23 ? qtrue : qfalse );
		return JK2CgameService::Handled;
	case 24: // 0x401a7f
		result = CM_PointContents( CL_JK2CgameVector( args[1] ), static_cast<clipHandle_t>( args[2] ) );
		return JK2CgameService::Handled;
	case 25: // 0x401a9e
		result = CM_TransformedPointContents( CL_JK2CgameVector( args[1] ), static_cast<clipHandle_t>( args[2] ),
			CL_JK2CgameVector( args[3] ), CL_JK2CgameVector( args[4] ) );
		return JK2CgameService::Handled;
	case 26: case 27: case 28: case 29: // 0x401ad7/0x401b1b and 0x401b76/0x401b7a, returning 0.
		CL_JK2CgameTrace( args, args[0] >= 28, args[0] == 27 || args[0] == 29 ? qtrue : qfalse );
		return JK2CgameService::Handled;
	case 30: { // 0x401bef: the renderer's MarkFragments.
		// The renderer walks the polygon's points and edge normals in arrays
		// of MAX_VERTS_ON_POLY (64, tr_marks.c); Outcast's impact marks pass 4.
		if ( args[1] < 0 || args[1] > 64 || args[4] < 0 || args[4] > 0x10000 || args[6] < 0 || args[6] > 0x10000 )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame mark fragment counts" );
		const auto *points = static_cast<const vec3_t *>( CL_JK2CgameSpan( args[2], sizeof( vec3_t ) * std::size_t( args[1] ? args[1] : 1 ) ) );
		const float *projection = CL_JK2CgameVector( args[3] );
		auto *pointBuffer = static_cast<float *>( CL_JK2CgameSpan( args[5], sizeof( vec3_t ) * std::size_t( args[4] ? args[4] : 1 ) ) );
		auto *fragments = static_cast<markFragment_t *>( CL_JK2CgameSpan( args[7],
			sizeof( markFragment_t ) * std::size_t( args[6] ? args[6] : 1 ) ) );
		static_assert( sizeof( markFragment_t ) == 8, "Outcast's mark fragment is Quake III's" );
		result = re.MarkFragments( static_cast<int>( args[1] ), points, projection, static_cast<int>( args[4] ),
			pointBuffer, static_cast<int>( args[6] ), fragments );
		return JK2CgameService::Handled;
	}
	case 31: // S_MuteSound 0x45d200: retail skips a channel below 1.
		if ( args[2] >= 1 && args[2] <= INT_MAX && args[1] >= INT_MIN && args[1] <= INT_MAX )
			S_MuteJK2Sound( static_cast<int>( args[1] ), static_cast<int>( args[2] ) );
		return JK2CgameService::Handled;
	case 32: { // S_StartSound 0x45d230 with a nullable origin.
		vec3_t origin;
		if ( args[1] ) std::memcpy( origin, CL_JK2CgameVector( args[1] ), sizeof( origin ) );
		S_StartSound( args[1] ? origin : nullptr, static_cast<int>( args[2] ), static_cast<int>( args[3] ),
			static_cast<sfxHandle_t>( args[4] ) );
		return JK2CgameService::Handled;
	}
	case 33: operation = CG_S_STARTLOCALSOUND; return JK2CgameService::Shared;
	case 34: operation = CG_S_CLEARLOOPINGSOUNDS; return JK2CgameService::Shared;
	case 35: return CL_JK2CgameSharedSound( args, operation, CG_S_ADDLOOPINGSOUND, { { 2, sizeof( vec3_t ) }, { 3, sizeof( vec3_t ) } } );
	case 36: return CL_JK2CgameSharedSound( args, operation, CG_S_UPDATEENTITYPOSITION, { { 2, sizeof( vec3_t ) } } );
	case 37: return CL_JK2CgameSharedSound( args, operation, CG_S_ADDREALLOOPINGSOUND, { { 2, sizeof( vec3_t ) }, { 3, sizeof( vec3_t ) } } );
	case 38: operation = CG_S_STOPLOOPINGSOUND; return JK2CgameService::Shared;
	case 39: return CL_JK2CgameSharedSound( args, operation, CG_S_RESPATIALIZE, { { 2, sizeof( vec3_t ) }, { 3, sizeof( vec3_t[3] ) } } );
	case 40: // 0x401d73 reads only the name, never Quake III's compression flag.
		result = S_RegisterSound( CL_JK2CgameString( args[1] ), qfalse );
		return JK2CgameService::Handled;
	case 41: { // 0x401d8b -> 0x45e2c0( intro, loop, flag != 0 ).
		/* A null intro reads as empty and an empty loop as the intro. With the
		 * flag set retail only keeps both names for its snd_restart (0x40b05e
		 * -> 0x45e280) and returns; the client game sets it when the music
		 * configstring is read at start-up and not forced (CG_StartMusic).
		 * Fn's own restart replays the track it last started. */
		const auto intro = CL_JK2CgameName( args[1] );
		auto loop = CL_JK2CgameName( args[2] );
		if ( !loop[0] ) loop = intro;
		if ( !args[3] ) S_StartBackgroundTrack( intro.data(), loop.data() );
		return JK2CgameService::Handled;
	}
	case 42: return CL_JK2CgameSharedString( args, operation, CG_R_LOADWORLDMAP, { 1 } );
	case 43: return CL_JK2CgameSharedString( args, operation, CG_R_REGISTERMODEL, { 1 } );
	case 44: return CL_JK2CgameSharedString( args, operation, CG_R_REGISTERSKIN, { 1 } );
	case 45: return CL_JK2CgameSharedString( args, operation, CG_R_REGISTERSHADER, { 1 } );
	case 46: return CL_JK2CgameSharedString( args, operation, CG_R_REGISTERSHADERNOMIP, { 1 } );
	case 52: // 0x401eeb -> the renderer's read-character 0x473e50.
		result = CL_JK2CgameReadChar( args[1] );
		return JK2CgameService::Handled;
	case 200: operation = CG_R_CLEARSCENE; return JK2CgameService::Shared;
	case 201: { // 0x401f12: one 212-byte entity record (jk2_render_abi.h).
		fntech3_jk2_mp_refEntity_t source;
		std::memcpy( &source, CL_JK2CgameSpan( args[1], sizeof( source ) ), sizeof( source ) );
		refEntity_t value{};
		const auto status = fntech3::jk2::render::Entity( source, value );
		if ( status != fntech3::jk2::render::Status::Ok )
			Com_Error( ERR_DROP, "Jedi Outcast cgame %s (type=%d, flags=0x%x)", fntech3::jk2::render::Description( status ),
				static_cast<int>( source.reType ), static_cast<unsigned>( source.renderfx ) );
		operation = CG_R_ADDREFENTITYTOSCENE;
		return JK2CgameService::Shared; // the shared scene path reads it again through the same adapter
	}
	case 202: { // 0x401f2d: shader, vertex count and vertices; one polygon.
		if ( args[2] <= 0 || args[2] > 32768 ) Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame polygon vertex count" );
		const auto count = static_cast<std::size_t>( args[2] );
		const auto *source = static_cast<const byte *>( CL_JK2CgameSpan( args[3], count * sizeof( fntech3_jk2_mp_polyVert_t ) ) );
		std::vector<polyVert_t> vertices( count );
		for ( std::size_t index = 0; index < count; ++index )
			if ( !fntech3::jk2::render::PolyVertex( source + index * sizeof( fntech3_jk2_mp_polyVert_t ), vertices[index] ) )
				Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame polygon vertex" );
		re.AddPolyToScene( static_cast<qhandle_t>( args[1] ), static_cast<int>( count ), vertices.data(), 1 );
		return JK2CgameService::Handled;
	}
	case 204: { // 0x401f7f: the light at a point.
		vec3_t point, ambient, directed, direction;
		std::memcpy( point, CL_JK2CgameVector( args[1] ), sizeof( point ) );
		void *outputs[3] = { CL_JK2CgameSpan( args[2], sizeof( vec3_t ) ), CL_JK2CgameSpan( args[3], sizeof( vec3_t ) ),
			CL_JK2CgameSpan( args[4], sizeof( vec3_t ) ) };
		result = re.LightForPoint( point, ambient, directed, direction );
		std::memcpy( outputs[0], ambient, sizeof( ambient ) );
		std::memcpy( outputs[1], directed, sizeof( directed ) );
		std::memcpy( outputs[2], direction, sizeof( direction ) );
		return JK2CgameService::Handled;
	}
	case 205: { // 0x401fc2: origin and four float words.
		vec3_t origin;
		std::memcpy( origin, CL_JK2CgameVector( args[1] ), sizeof( origin ) );
		re.AddLightToScene( origin, _vmf( args[2] ), _vmf( args[3] ), _vmf( args[4] ), _vmf( args[5] ) );
		return JK2CgameService::Handled;
	}
	case 207: { // 0x40201e: one 368-byte view record (jk2_render_abi.h).
		fntech3_jk2_mp_refdef_t source;
		std::memcpy( &source, CL_JK2CgameSpan( args[1], sizeof( source ) ), sizeof( source ) );
		refdef_t value{};
		const auto status = fntech3::jk2::render::View( source, value );
		if ( status != fntech3::jk2::render::Status::Ok )
			Com_Error( ERR_DROP, "Jedi Outcast cgame %s (flags=0x%x)", fntech3::jk2::render::Description( status ),
				static_cast<unsigned>( source.rdflags ) );
		operation = CG_R_RENDERSCENE;
		return JK2CgameService::Shared; // HUD, capture, liquid and lighting paths read it again
	}
	case 208:
		if ( args[1] ) CL_JK2CgameSpan( args[1], sizeof( float[4] ) );
		operation = CG_R_SETCOLOR;
		return JK2CgameService::Shared;
	case 209: { // 0x402054: Raven's virtual 640x480 screen, as the fonts are.
		if ( CL_CaptureHidesHud() ) return JK2CgameService::Handled;
		float x = _vmf( args[1] ), y = _vmf( args[2] ), w = _vmf( args[3] ), h = _vmf( args[4] );
		CL_HudFromVirtual( &x, &y, &w, &h );
		CL_HudQueueStretchPic( x, y, w, h, _vmf( args[5] ), _vmf( args[6] ), _vmf( args[7] ), _vmf( args[8] ),
			static_cast<qhandle_t>( args[9] ) );
		return JK2CgameService::Handled;
	}
	case 210: { // 0x402089
		vec3_t mins, maxs;
		void *outputs[2] = { CL_JK2CgameSpan( args[2], sizeof( vec3_t ) ), CL_JK2CgameSpan( args[3], sizeof( vec3_t ) ) };
		re.ModelBounds( static_cast<qhandle_t>( args[1] ), mins, maxs );
		std::memcpy( outputs[0], mins, sizeof( mins ) );
		std::memcpy( outputs[1], maxs, sizeof( maxs ) );
		return JK2CgameService::Handled;
	}
	case 211: { // 0x4020b8: a 48-byte orientation (jk2_render_abi.h); returns found.
		void *destination = CL_JK2CgameSpan( args[1], sizeof( fntech3_jk2_mp_orientation_t ) );
		const char *tag = CL_JK2CgameString( args[6], MAX_QPATH );
		orientation_t canonical{};
		result = re.LerpTag( &canonical, static_cast<qhandle_t>( args[2] ), static_cast<int>( args[3] ),
			static_cast<int>( args[4] ), _vmf( args[5] ), tag );
		fntech3_jk2_mp_orientation_t external{};
		fntech3::jk2::render::Copy( external.origin, canonical.origin );
		fntech3::jk2::render::Copy( external.axis, canonical.axis );
		std::memcpy( destination, &external, sizeof( external ) );
		return JK2CgameService::Handled;
	}
	case 214: return CL_JK2CgameSharedString( args, operation, CG_R_REMAP_SHADER, { 1, 2, 3 } );
	case 226: { // 0x4011d0 stores weapon, sensitivity and both selections.
		const float sensitivity = _vmf( args[2] );
		if ( !fntech3::jk2::render::Finite( sensitivity ) ) Com_Error( ERR_DROP, "Invalid Jedi Outcast cgame sensitivity" );
		CL_SetUserCmdValue( static_cast<int>( args[1] ), 0, sensitivity, 0 );
		// CL_FinishMove 0x4076c0 sends their low bytes, as Academy's does.
		fntech3::jka::command::selections.Set( static_cast<int>( args[3] ), static_cast<int>( args[4] ) );
		return JK2CgameService::Handled;
	}
	case 229: // 0x40226f: VM_Call( uivm, 7, menu ), the number in the UI's own terms.
		if ( !uivm || !CL_UIModuleReady() ) Com_Error( ERR_DROP, "Jedi Outcast cgame menu request has no UI" );
		Com_DPrintf( "Outcast cgame requested UI menu: %ld\n", static_cast<long>( args[1] ) );
		VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, static_cast<int>( args[1] ) );
		return JK2CgameService::Handled;
	case 234: operation = CG_KEY_GETCATCHER; return JK2CgameService::Shared; // 0x40fb50
	case 235: operation = CG_KEY_SETCATCHER; return JK2CgameService::Shared; // 0x40fb60
	case 238: case 240: case 241:
		CL_JK2CgameParserCall( args, result );
		return JK2CgameService::Handled;
	case 239: operation = CG_PC_FREE_SOURCE; return JK2CgameService::Shared;
	case 246: return CL_JK2CgameSharedSound( args, operation, CG_SNAPVECTOR, { { 1, sizeof( vec3_t ) } } ); // 0x444640
	case 247: return CL_JK2CgameSharedString( args, operation, CG_CIN_PLAYCINEMATIC, { 1 } );
	case 248: operation = CG_CIN_STOPCINEMATIC; return JK2CgameService::Shared;
	case 249: operation = CG_CIN_RUNCINEMATIC; return JK2CgameService::Shared;
	case 250: operation = CG_CIN_DRAWCINEMATIC; return JK2CgameService::Shared;
	case 251: operation = CG_CIN_SETEXTENTS; return JK2CgameService::Shared;
	case 273: // 0x402a88 -> ROFF clean 0x4357c0 for the client lane, which returns 1.
		/* The client game never caches a ROFF (import 275 has no call site)
		 * and its playback (276) is refused, so this lane has no entries to
		 * drop and nothing of its own to unload; the server's ROFF storage is
		 * untouched, as retail clears only the client's reference (+0x5c). */
		result = 1;
		return JK2CgameService::Handled;
	case 274: // 0x402a9a -> 0x435a30 for the client lane; with no entries, no work.
		return JK2CgameService::Handled;
	case 300: // 0x402ee1 keeps the module's address (0x9d9e24) for later exports.
		CL_JK2CgameSpan( args[1], 1 );
		jk2CgameSharedAddress = args[1];
		return JK2CgameService::Handled;
	case 254: case 256: case 259: case 260: case 262: case 263: case 264: case 265:
	case 266: case 267: case 268: case 269: case 270:
		return CL_JK2CgameEffectsCall( args, result ) ? JK2CgameService::Handled : JK2CgameService::None;
	case 212: case 213:
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import %ld: Jedi Outcast's rotated pictures are not served yet",
			static_cast<long>( args[0] ) );
		return JK2CgameService::Handled;
	case 217:
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import 217: Jedi Outcast's brush-model vertices are not served yet" );
		return JK2CgameService::Handled;
	case 218: // 0x40266b hands fourteen words to the effects engine's line 0x41e0b0.
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import 218: Jedi Outcast's effects boundary is open." );
		return JK2CgameService::Handled;
	case 227: case 228:
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import %ld: Jedi Outcast's forced view %s is not served yet",
			static_cast<long>( args[0] ), args[0] == 227 ? "angle" : "turn extent" );
		return JK2CgameService::Handled;
	case 276:
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import 276: Jedi Outcast's client ROFF playback is not served yet" );
		return JK2CgameService::Handled;
	default:
		return JK2CgameService::None;
	}
}
#endif
