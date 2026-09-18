/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_WOLFET_RENDER_COMPAT_HPP
#define FNTECH3_CLIENT_WOLFET_RENDER_COMPAT_HPP

#include "client_render_compat.hpp"
#include "client_rtcw_render_memory.hpp"
#include "../compat/wolfet_module_abi.h"
#include "../compat/wolfet_draw_adapter.hpp"
#include "../renderercommon/tr_rtcw_corona.h"
#include <climits>
#include <limits>
#include <memory>

inline const char *CL_WolfETRenderName( vm_t *vm, intptr_t address ) {
	if ( !address ) Com_Error( ERR_DROP, "Null ET renderer resource name" );
	VM_CHECKBOUNDS( vm, address, 1 );
	const char *name = static_cast<const char *>( VM_ExplicitArgPtr( vm, address ) );
	if ( !name ) Com_Error( ERR_DROP, "Invalid ET renderer resource pointer" );
	return name;
}

inline void CL_WolfETRenderPolys( vm_t *vm, qhandle_t shader, intptr_t numVerts,
	intptr_t address, intptr_t numPolys ) {
	if ( numVerts < 0 || numVerts > INT_MAX || numPolys < 0 || numPolys > INT_MAX )
		Com_Error( ERR_DROP, "Invalid ET polygon count" );
	// Empty submissions have no input range, and the original ignores shader 0.
	if ( !numVerts || !numPolys || !shader ) return;
	constexpr std::size_t maxVertices = INT_MAX /
		( sizeof( polyVert_t ) > sizeof( fntech3_wolfet_polyVert_t ) ?
		  sizeof( polyVert_t ) : sizeof( fntech3_wolfet_polyVert_t ) );
	if ( static_cast<std::size_t>( numVerts ) > maxVertices / static_cast<std::size_t>( numPolys ) )
		Com_Error( ERR_DROP, "ET polygon buffer is too large" );
	const auto count = static_cast<std::size_t>( numVerts ) * static_cast<std::size_t>( numPolys );
	if ( !address ) Com_Error( ERR_DROP, "Null ET polygon vertices" );
	VM_CHECKBOUNDS( vm, address, count * sizeof( fntech3_wolfet_polyVert_t ) );
	const auto *source = static_cast<const unsigned char *>( VM_ExplicitArgPtr( vm, address ) );
	if ( !source ) Com_Error( ERR_DROP, "Invalid ET polygon vertices" );

	// Small submissions need no allocation. Larger batches use the engine's
	// temporary hunk, which ERR_DROP and CL_FlushMemory also reclaim. No module
	// memory is retained by the renderer.
	polyVert_t local[256];
	std::unique_ptr<polyVert_t, decltype( &Hunk_FreeTempMemory )> staging( nullptr, Hunk_FreeTempMemory );
	polyVert_t *vertices = local;
	if ( count > std::size( local ) ) {
		staging.reset( static_cast<polyVert_t *>( Hunk_AllocateTempMemory(
			static_cast<int>( count * sizeof( polyVert_t ) ) ) ) );
		vertices = staging.get();
	}
	for ( std::size_t i = 0; i < count; ++i )
		fntech3::render::ReadWolfETPolyVert( source + i * sizeof( fntech3_wolfet_polyVert_t ), vertices[i] );
	re.AddPolyToScene( shader, static_cast<int>( numVerts ), vertices, static_cast<int>( numPolys ) );
}

// Enemy Territory's renderer imports, for its cgame and its UI alike. False
// means the caller must continue its own dispatch. Imports ET declares that the
// shared renderer has no service for are not handled here and are refused by
// name where the dispatcher runs out of cases, so nothing is silently dropped.
inline bool CL_WolfETRenderCall( vm_t *vm, fntech3::wolfet::module::Call call,
	intptr_t *args, intptr_t &result ) {
	using fntech3::wolfet::module::Call;
	if ( !Com_GameIsWolfET() ) return false;
	if ( !vm ) Com_Error( ERR_DROP, "ET renderer import requires an initialized module" );
	switch ( call ) {
	case Call::CG_R_LOADWORLDMAP:
		re.LoadWorld( CL_WolfETRenderName( vm, args[1] ) );
		break;
	case Call::CG_R_REGISTERMODEL:
	case Call::UI_R_REGISTERMODEL: {
		// ET returns handle 0 for a null or empty model name rather than
		// reporting a missing model, so absent names never reach the shared
		// renderer's diagnostic.
		const char *name = args[1] ? CL_WolfETRenderName( vm, args[1] ) : nullptr;
		result = name && name[0] ? re.RegisterModel( name ) : 0;
		return true;
	}
	case Call::CG_R_REGISTERSKIN:
	case Call::UI_R_REGISTERSKIN:
		result = args[1] ? re.RegisterSkin( CL_WolfETRenderName( vm, args[1] ) ) : 0;
		return true;
	case Call::CG_R_REGISTERSHADER: {
		const char *name = CL_WolfETRenderName( vm, args[1] );
		result = re.RegisterShader( name );
		CL_HudRegisterShaderName( static_cast<qhandle_t>( result ), name );
		return true;
	}
	case Call::CG_R_REGISTERSHADERNOMIP:
	case Call::UI_R_REGISTERSHADERNOMIP: {
		const char *name = CL_WolfETRenderName( vm, args[1] );
		result = re.RegisterShaderNoMip( name );
		if ( call == Call::CG_R_REGISTERSHADERNOMIP )
			CL_HudRegisterShaderName( static_cast<qhandle_t>( result ), name );
		return true;
	}
	case Call::CG_R_GETSKINMODEL: {
		if ( !args[3] ) Com_Error( ERR_DROP, "Null ET skin model output" );
		VM_CHECKBOUNDS( vm, args[3], FNTECH3_WOLFET_MAX_QPATH );
		void *output = VM_ExplicitArgPtr( vm, args[3] );
		if ( !output || args[1] < INT_MIN || args[1] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid ET skin model arguments" );
		const char *type = CL_WolfETRenderName( vm, args[2] );
		char name[FNTECH3_WOLFET_MAX_QPATH]{};
		const qboolean found = re.GetSkinModel( static_cast<int>( args[1] ), type, name );
		if ( found ) {
			const char *end = static_cast<const char *>( std::memchr( name, '\0', sizeof( name ) ) );
			if ( !end ) Com_Error( ERR_DROP, "Unterminated ET skin model result" );
			std::memcpy( output, name, static_cast<std::size_t>( end - name ) + 1 );
		}
		result = found;
		return true;
	}
	case Call::CG_R_GETMODELSHADER:
		for ( int i = 1; i <= 3; ++i )
			if ( args[i] < INT_MIN || args[i] > INT_MAX )
				Com_Error( ERR_DROP, "Invalid ET model shader arguments" );
		result = re.GetShaderFromModel( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ), static_cast<int>( args[3] ) );
		return true;
	case Call::CG_R_CLEARSCENE:
	case Call::UI_R_CLEARSCENE:
		re.ClearScene();
		break;
	case Call::CG_R_ADDREFENTITYTOSCENE:
	case Call::UI_R_ADDREFENTITYTOSCENE: {
		const refEntity_t entity = CL_ModuleRefEntity( vm, args[1] );
		re.AddRefEntityToScene( &entity, qfalse );
		break;
	}
	case Call::CG_R_ADDPOLYTOSCENE:
	case Call::UI_R_ADDPOLYTOSCENE:
		CL_WolfETRenderPolys( vm, args[1], args[2], args[3], 1 );
		break;
	case Call::CG_R_ADDPOLYSTOSCENE:
	case Call::UI_R_ADDPOLYSTOSCENE:
		CL_WolfETRenderPolys( vm, args[1], args[2], args[3], args[4] );
		break;
	case Call::CG_R_ADDCORONATOSCENE:
	case Call::UI_R_ADDCORONATOSCENE: {
		if ( args[6] < INT_MIN || args[6] > INT_MAX || args[7] < INT_MIN || args[7] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid ET corona identity or visibility" );
		fnRTCWCorona_t corona{};
		std::memcpy( corona.origin, CL_RTCWRenderSpan( vm, args[1], sizeof( corona.origin ) ),
			sizeof( corona.origin ) );
		corona.color[0] = VMF(2); corona.color[1] = VMF(3); corona.color[2] = VMF(4);
		corona.scale = VMF(5);
		corona.id = static_cast<int>( args[6] );
		// ET's last word is a visibility flag, where RTCW names the same word
		// a flag set. Only zero and one are meaningful to either.
		corona.flags = static_cast<int>( args[7] );
		if ( !FnRTCWCoronaValid( &corona ) ) Com_Error( ERR_DROP, "Invalid ET corona values" );
		re.AddRTCWCoronaToScene( &corona );
		break;
	}
	case Call::CG_R_PROJECTDECAL: {
		// A shader, a footprint of one, three or four points, a direction and
		// depth, a colour, and how long the mark lasts and how long it fades.
		// ET ignores a footprint of any other size before reading it.
		if ( args[2] < INT_MIN || args[2] > INT_MAX || args[6] < INT_MIN || args[6] > INT_MAX ||
			args[7] < INT_MIN || args[7] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid ET decal request" );
		const int numPoints = static_cast<int>( args[2] );
		if ( numPoints != 1 && numPoints != 3 && numPoints != 4 ) break;
		vec3_t points[4];
		vec4_t projection, color;
		const auto pointBytes = sizeof( vec3_t ) * static_cast<std::size_t>( numPoints );
		std::memcpy( points, CL_RTCWRenderSpan( vm, args[3], pointBytes ), pointBytes );
		std::memcpy( projection, CL_RTCWRenderSpan( vm, args[4], sizeof( projection ) ), sizeof( projection ) );
		std::memcpy( color, CL_RTCWRenderSpan( vm, args[5], sizeof( color ) ), sizeof( color ) );
		if ( re.ProjectWolfETDecal ) {
			re.ProjectWolfETDecal( static_cast<qhandle_t>( args[1] ), numPoints, points, projection, color,
				static_cast<int>( args[6] ), static_cast<int>( args[7] ) );
		}
		break;
	}
	case Call::CG_R_CLEARDECALS:
		if ( re.ClearWolfETDecals ) re.ClearWolfETDecals();
		break;
	case Call::CG_R_ADDLIGHTTOSCENE:
	case Call::UI_R_ADDLIGHTTOSCENE: {
		// ET sends a radius and a brightness separately, then a shader handle
		// and its own flag word. Every one of them crosses.
		if ( args[7] < INT_MIN || args[7] > INT_MAX || args[8] < INT_MIN || args[8] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid ET light shader or flags" );
		vec3_t origin;
		std::memcpy( origin, CL_RTCWRenderSpan( vm, args[1], sizeof( origin ) ), sizeof( origin ) );
		re.AddWolfETLightToScene( origin, VMF(2), VMF(3), VMF(4), VMF(5), VMF(6),
			static_cast<qhandle_t>( args[7] ), static_cast<int>( args[8] ) );
		break;
	}
	case Call::CG_R_DRAW2DPOLYS:
	case Call::UI_R_DRAW2DPOLYS: {
		// ET submits a screen-space fan: points, count, shader.
		if ( args[2] < 0 || args[2] > INT_MAX ) Com_Error( ERR_DROP, "Invalid ET 2D polygon count" );
		const auto count = static_cast<std::size_t>( args[2] );
		if ( !count ) break;
		if ( count > INT_MAX / sizeof( fntech3_wolfet_polyVert_t ) )
			Com_Error( ERR_DROP, "ET 2D polygon batch is too large" );
		const auto *source = static_cast<const unsigned char *>(
			CL_RTCWRenderSpan( vm, args[1], count * sizeof( fntech3_wolfet_polyVert_t ) ) );
		polyVert_t local[256];
		std::unique_ptr<polyVert_t, decltype( &Hunk_FreeTempMemory )> staging( nullptr, Hunk_FreeTempMemory );
		polyVert_t *vertices = local;
		if ( count > std::size( local ) ) {
			staging.reset( static_cast<polyVert_t *>( Hunk_AllocateTempMemory(
				static_cast<int>( count * sizeof( polyVert_t ) ) ) ) );
			vertices = staging.get();
		}
		for ( std::size_t i = 0; i < count; ++i )
			fntech3::render::ReadWolfETPolyVert( source + i * sizeof( fntech3_wolfet_polyVert_t ), vertices[i] );
		re.AddWolfET2DPolys( vertices, static_cast<int>( count ), args[3] );
		break;
	}
	case Call::CG_R_SAVEVIEWPARMS:
		re.SaveWolfETViewParms();
		break;
	case Call::CG_R_RESTOREVIEWPARMS:
		re.RestoreWolfETViewParms();
		break;
	case Call::CG_R_FINISH:
		// ET queues a pipeline finish for the frame it is drawing; the engine's
		// own render synchronisation is the same wait.
		re.SyncRender();
		break;
	case Call::CG_R_ADDPOLYBUFFERTOSCENE: {
		// ET submits an indexed batch and expects the renderer to hold on to
		// the module's own buffer until the frame is drawn. Retaining module
		// memory is not something this engine does, so the batch is expanded
		// into its triangles here and copied into the engine's own scene. The
		// triangles, their vertices and their shader are identical; only the
		// fog volume is chosen per triangle rather than once for the batch.
		VM_CHECKBOUNDS( vm, args[1], sizeof( fntech3_wolfet_polyBuffer_t ) );
		const auto *buffer = static_cast<const fntech3_wolfet_polyBuffer_t *>(
			VM_ExplicitArgPtr( vm, args[1] ) );
		if ( !buffer ) Com_Error( ERR_DROP, "Invalid ET polygon buffer" );
		const int vertices = buffer->numVerts;
		const int indices = buffer->numIndicies;
		if ( vertices < 0 || vertices > FNTECH3_WOLFET_MAX_PB_VERTS ||
			indices < 0 || indices > FNTECH3_WOLFET_MAX_PB_INDICIES )
			Com_Error( ERR_DROP, "Invalid ET polygon buffer counts" );
		// A batch that is not whole triangles, or names a vertex it does not
		// carry, is refused rather than drawn with whatever happened to be there.
		if ( indices % 3 || !indices || !vertices ) break;
		for ( int i = 0; i < indices; ++i ) {
			if ( buffer->indicies[i] < 0 || buffer->indicies[i] >= vertices )
				Com_Error( ERR_DROP, "ET polygon buffer index out of range" );
		}
		const auto count = static_cast<std::size_t>( indices );
		std::unique_ptr<polyVert_t, decltype( &Hunk_FreeTempMemory )> staging(
			static_cast<polyVert_t *>( Hunk_AllocateTempMemory(
				static_cast<int>( count * sizeof( polyVert_t ) ) ) ), Hunk_FreeTempMemory );
		polyVert_t *expanded = staging.get();
		if ( !expanded ) Com_Error( ERR_DROP, "ET polygon buffer allocation failed" );
		for ( std::size_t i = 0; i < count; ++i ) {
			const int index = buffer->indicies[i];
			polyVert_t &vertex = expanded[i];
			vertex = {};
			for ( int axis = 0; axis < 3; ++axis ) vertex.xyz[axis] = buffer->xyz[index][axis];
			vertex.st[0] = buffer->st[index][0];
			vertex.st[1] = buffer->st[index][1];
			for ( int channel = 0; channel < 4; ++channel )
				vertex.modulate.rgba[channel] = buffer->color[index][channel];
		}
		re.AddPolyToScene( buffer->shader, 3, expanded, indices / 3 );
		break;
	}
	case Call::CG_R_LOADDYNAMICSHADER: {
		// ET defines a shader at runtime instead of shipping it in a file.
		// Either argument may be absent: neither empties the list, a name
		// alone removes one definition.
		const char *name = args[1] ? CL_WolfETRenderName( vm, args[1] ) : nullptr;
		const char *text = args[2] ? CL_WolfETRenderName( vm, args[2] ) : nullptr;
		result = re.LoadWolfETDynamicShader( name, text );
		return true;
	}
	case Call::CG_R_GETTEXTUREID:
		result = re.GetWolfETTextureId( CL_WolfETRenderName( vm, args[1] ) );
		return true;
	case Call::CG_R_SETGLOBALFOG:
		// ET's map compiles one fog volume with no brush of its own, covering
		// the whole world; this retunes that one, optionally over a duration.
		if ( args[2] < INT_MIN || args[2] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid ET global fog duration" );
		re.SetWolfETGlobalFog( args[1] ? qtrue : qfalse, static_cast<int>( args[2] ),
			VMF(3), VMF(4), VMF(5), VMF(6) );
		break;
	case Call::CG_R_SETFOG:
		for ( int i = 1; i <= 3; ++i )
			if ( args[i] < INT_MIN || args[i] > INT_MAX )
				Com_Error( ERR_DROP, "Invalid ET fog integer argument" );
		// The engine's Wolfenstein fog command carries ET's parameters exactly:
		// ET's fog description is RTCW's without the private dirty flag.
		re.SetRTCWFog( static_cast<int>( args[1] ), static_cast<int>( args[2] ),
			static_cast<int>( args[3] ), VMF(4), VMF(5), VMF(6), VMF(7) );
		break;
	case Call::CG_R_RENDERSCENE:
	case Call::UI_R_RENDERSCENE: {
		refdef_t view = CL_ModuleRefdef( vm, args[1] );
		if ( call == Call::CG_R_RENDERSCENE ) {
			CL_HudAdjustRefdef( &view );
		} else {
			view.rdflags |= RDF_NOFOVCORRECTION;
		}
		re.RenderScene( &view );
		break;
	}
	case Call::CG_R_SETCOLOR:
	case Call::UI_R_SETCOLOR:
		if ( args[1] ) {
			VM_CHECKBOUNDS( vm, args[1], sizeof( vec4_t ) );
			vec4_t color;
			std::memcpy( color, VM_ExplicitArgPtr( vm, args[1] ), sizeof( color ) );
			re.SetColor( color );
		} else {
			re.SetColor( nullptr );
		}
		break;
	case Call::CG_R_DRAWSTRETCHPIC:
		CL_HudDrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		break;
	case Call::UI_R_DRAWSTRETCHPIC:
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		break;
	case Call::CG_R_DRAWSTRETCHPIC_GRADIENT: {
		vec4_t color;
		if ( args[10] ) {
			VM_CHECKBOUNDS( vm, args[10], sizeof( color ) );
			std::memcpy( color, VM_ExplicitArgPtr( vm, args[10] ), sizeof( color ) );
		}
		refDrawQuad_t quad;
		if ( !fntech3::wolfet::render::GradientQuad( VMF(1), VMF(2), VMF(3), VMF(4),
			VMF(5), VMF(6), VMF(7), VMF(8), args[9], args[10] ? color : nullptr, args[11], quad ) )
			Com_Error( ERR_DROP, "Invalid ET gradient picture" );
		float x = VMF(1), y = VMF(2), w = VMF(3), h = VMF(4);
		if ( !CL_HudAdjustPic( &x, &y, &w, &h, args[9] ) ) break;
		if ( !fntech3::wolfet::render::GradientQuad( x, y, w, h,
			VMF(5), VMF(6), VMF(7), VMF(8), args[9], args[10] ? color : nullptr, args[11], quad ) )
			Com_Error( ERR_DROP, "Invalid corrected ET gradient picture" );
		re.DrawQuad( &quad );
		break;
	}
	case Call::CG_R_DRAWROTATEDPIC:
	case Call::UI_R_DRAWROTATEDPIC: {
		refDrawQuad_t quad;
		if ( !fntech3::wolfet::render::RotatedQuad( VMF(1), VMF(2), VMF(3), VMF(4),
			VMF(5), VMF(6), VMF(7), VMF(8), args[9], VMF(10), quad ) )
			Com_Error( ERR_DROP, "Invalid ET rotated picture" );
		if ( call == Call::CG_R_DRAWROTATEDPIC ) {
			float x = VMF(1), y = VMF(2), w = VMF(3), h = VMF(4);
			if ( !CL_HudAdjustPic( &x, &y, &w, &h, args[9] ) ) break;
			if ( !fntech3::wolfet::render::RotatedQuad( x, y, w, h,
				VMF(5), VMF(6), VMF(7), VMF(8), args[9], VMF(10), quad ) )
				Com_Error( ERR_DROP, "Invalid corrected ET rotated picture" );
		}
		re.DrawQuad( &quad );
		break;
	}
	case Call::CG_R_LERPTAG:
	case Call::UI_CM_LERPTAG: {
		if ( !args[1] ) Com_Error( ERR_DROP, "Null ET tag output" );
		VM_CHECKBOUNDS( vm, args[1], sizeof( fntech3_wolfet_orientation_t ) );
		void *output = VM_ExplicitArgPtr( vm, args[1] );
		if ( !output ) Com_Error( ERR_DROP, "Invalid ET tag output" );
		if ( !args[2] ) Com_Error( ERR_DROP, "Null ET tag entity" );
		VM_CHECKBOUNDS( vm, args[2], sizeof( fntech3_wolfet_refEntity_t ) );
		refEntity_t entity{};
		if ( !fntech3::render::ReadWolfETTagEntity( VM_ExplicitArgPtr( vm, args[2] ), entity ) )
			Com_Error( ERR_DROP, "Invalid ET tag entity" );
		const char *name = CL_WolfETRenderName( vm, args[3] );
		if ( args[4] < INT_MIN || args[4] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid ET tag start index" );
		orientation_t tag{};
		const int index = re.LerpTagEntity( &tag, &entity, name, static_cast<int>( args[4] ) );
		fntech3::render::WriteWolfETOrientation( output, tag );
		result = index;
		return true;
	}
	case Call::CG_R_MODELBOUNDS:
	case Call::UI_R_MODELBOUNDS: {
		if ( !args[2] || !args[3] ) Com_Error( ERR_DROP, "Null ET model bounds output" );
		VM_CHECKBOUNDS( vm, args[2], sizeof( vec3_t ) );
		VM_CHECKBOUNDS( vm, args[3], sizeof( vec3_t ) );
		vec3_t mins{}, maxs{};
		re.ModelBounds( args[1], mins, maxs );
		std::memcpy( VM_ExplicitArgPtr( vm, args[2] ), mins, sizeof( mins ) );
		std::memcpy( VM_ExplicitArgPtr( vm, args[3] ), maxs, sizeof( maxs ) );
		break;
	}
	case Call::CG_R_REMAP_SHADER:
	case Call::UI_R_REMAP_SHADER: {
		const char *oldName = CL_WolfETRenderName( vm, args[1] );
		const char *newName = CL_WolfETRenderName( vm, args[2] );
		const char *time = args[3] ? CL_WolfETRenderName( vm, args[3] ) : nullptr;
		re.RemapShader( oldName, newName, time );
		break;
	}
	case Call::CG_R_LIGHTFORPOINT: {
		for ( int index = 1; index <= 4; ++index )
			VM_CHECKBOUNDS( vm, args[index], sizeof( vec3_t ) );
		vec3_t point, ambient{}, directed{}, direction{};
		std::memcpy( point, VM_ExplicitArgPtr( vm, args[1] ), sizeof( point ) );
		result = re.LightForPoint( point, ambient, directed, direction );
		std::memcpy( VM_ExplicitArgPtr( vm, args[2] ), ambient, sizeof( ambient ) );
		std::memcpy( VM_ExplicitArgPtr( vm, args[3] ), directed, sizeof( directed ) );
		std::memcpy( VM_ExplicitArgPtr( vm, args[4] ), direction, sizeof( direction ) );
		return true;
	}
	case Call::CG_R_INPVS: {
		vec3_t first, second;
		std::memcpy( first, CL_RTCWRenderSpan( vm, args[1], sizeof( first ) ), sizeof( first ) );
		std::memcpy( second, CL_RTCWRenderSpan( vm, args[2], sizeof( second ) ), sizeof( second ) );
		result = re.inPVS( first, second );
		return true;
	}
	case Call::CG_GET_ENTITY_TOKEN: {
		if ( args[2] < 1 || args[2] > INT_MAX || !args[1] )
			Com_Error( ERR_DROP, "Invalid ET entity token buffer" );
		if ( static_cast<std::uintptr_t>( args[2] ) > (std::numeric_limits<std::uintptr_t>::max)()
			- static_cast<std::uintptr_t>( args[1] ) )
			Com_Error( ERR_DROP, "ET entity token address overflow" );
		VM_CHECKBOUNDS( vm, args[1], args[2] );
		auto *destination = static_cast<char *>( VM_ExplicitArgPtr( vm, args[1] ) );
		if ( !destination ) Com_Error( ERR_DROP, "Invalid ET entity token pointer" );
		char token[MAX_TOKEN_CHARS]{};
		const int capacity = args[2] < MAX_TOKEN_CHARS ? static_cast<int>( args[2] ) : MAX_TOKEN_CHARS;
		result = re.GetEntityToken( token, capacity );
		token[capacity - 1] = '\0';
		std::memcpy( destination, token, std::strlen( token ) + 1 );
		return true;
	}
	case Call::CG_CM_MARKFRAGMENTS: {
		// ET's mark request is Quake's, and its fragment record is the same two
		// integers, so the points and fragments cross unchanged. The counts are
		// still bounded before either buffer is sized.
		if ( args[1] < 0 || args[1] > INT_MAX || args[4] < 0 || args[4] > INT_MAX ||
			args[6] < 0 || args[6] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid ET mark fragment counts" );
		const auto points = static_cast<std::size_t>( args[1] );
		const auto maxPoints = static_cast<std::size_t>( args[4] );
		const auto maxFragments = static_cast<std::size_t>( args[6] );
		const void *input = CL_RTCWRenderSpan( vm, args[2], points * sizeof( vec3_t ) );
		const void *projection = CL_RTCWRenderSpan( vm, args[3], sizeof( vec3_t ) );
		void *pointOutput = CL_RTCWRenderSpan( vm, args[5], maxPoints * sizeof( vec3_t ) );
		void *fragmentOutput = CL_RTCWRenderSpan( vm, args[7],
			maxFragments * sizeof( fntech3_wolfet_markFragment_t ) );
		static_assert( sizeof( markFragment_t ) == sizeof( fntech3_wolfet_markFragment_t ) );
		vec3_t plane;
		std::memcpy( plane, projection, sizeof( plane ) );
		result = re.MarkFragments( static_cast<int>( points ),
			static_cast<const vec3_t *>( input ), plane,
			static_cast<int>( maxPoints ), static_cast<float *>( pointOutput ),
			static_cast<int>( maxFragments ), static_cast<markFragment_t *>( fragmentOutput ) );
		return true;
	}
	default:
		return false;
	}
	result = 0;
	return true;
}

#endif
