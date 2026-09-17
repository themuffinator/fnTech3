/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RTCW_RENDER_COMPAT_HPP
#define FNTECH3_CLIENT_RTCW_RENDER_COMPAT_HPP

#include "client_render_compat.hpp"
#include "../compat/rtcw_module_abi.h"
#include "../compat/rtcw_draw_adapter.hpp"
#include <climits>
#include <limits>
#include <memory>
#include "client_rtcw_marks.hpp"
#include "../renderercommon/tr_rtcw_corona.h"

inline const char *CL_RTCWRenderName( vm_t *vm, intptr_t address ) {
	if ( !address ) Com_Error( ERR_DROP, "Null RTCW renderer resource name" );
	VM_CHECKBOUNDS( vm, address, 1 );
	const char *name = static_cast<const char *>( VM_ExplicitArgPtr( vm, address ) );
	if ( !name ) Com_Error( ERR_DROP, "Invalid RTCW renderer resource pointer" );
	return name;
}

inline void CL_RTCWRenderPolys( vm_t *vm, qhandle_t shader, intptr_t numVerts,
	intptr_t address, intptr_t numPolys ) {
	if ( numVerts < 0 || numVerts > INT_MAX || numPolys < 0 || numPolys > INT_MAX )
		Com_Error( ERR_DROP, "Invalid RTCW polygon count" );
	// Empty submissions have no input range. Original RTCW also ignores shader 0.
	if ( !numVerts || !numPolys || !shader ) return;
	constexpr std::size_t maxVertices = INT_MAX /
		( sizeof( polyVert_t ) > sizeof( fntech3_rtcw_polyVert_t ) ?
		  sizeof( polyVert_t ) : sizeof( fntech3_rtcw_polyVert_t ) );
	if ( static_cast<std::size_t>( numVerts ) > maxVertices / static_cast<std::size_t>( numPolys ) )
		Com_Error( ERR_DROP, "RTCW polygon buffer is too large" );
	const auto count = static_cast<std::size_t>( numVerts ) * static_cast<std::size_t>( numPolys );
	if ( !address ) Com_Error( ERR_DROP, "Null RTCW polygon vertices" );
	VM_CHECKBOUNDS( vm, address, count * sizeof( fntech3_rtcw_polyVert_t ) );
	const auto *source = static_cast<const unsigned char *>( VM_ExplicitArgPtr( vm, address ) );
	if ( !source ) Com_Error( ERR_DROP, "Invalid RTCW polygon vertices" );

	// Common small submissions need no allocation. Larger batches use the
	// engine's temporary hunk, which is also reclaimed by ERR_DROP/CL_FlushMemory.
	// No module memory is retained by the renderer.
	polyVert_t local[256];
	std::unique_ptr<polyVert_t, decltype( &Hunk_FreeTempMemory )> staging( nullptr, Hunk_FreeTempMemory );
	polyVert_t *vertices = local;
	if ( count > std::size( local ) ) {
		staging.reset( static_cast<polyVert_t *>( Hunk_AllocateTempMemory( static_cast<int>( count * sizeof( polyVert_t ) ) ) ) );
		vertices = staging.get();
	}
	for ( std::size_t i = 0; i < count; ++i )
		fntech3::render::ReadRTCWPolyVert( source + i * sizeof( fntech3_rtcw_polyVert_t ), vertices[i] );
	re.AddPolyToScene( shader, static_cast<int>( numVerts ), vertices, static_cast<int>( numPolys ) );
}

// Original SP/MP UI and cgame contracts. Keeping the common renderer calls
// together avoids routing RTCW through QL Steam/menu/highlighting behavior.
// False means that the caller must continue its module-specific dispatch.
inline bool CL_RTCWRenderCall( vm_t *vm, fntech3::rtcw::module::Call call,
	intptr_t *args, intptr_t &result ) {
	using fntech3::rtcw::module::Call;
	if ( !Com_GameIsRTCW() ) return false;
	if ( !vm ) Com_Error( ERR_DROP, "RTCW renderer import requires an initialized module" );
	switch ( call ) {
	case Call::CG_R_ADDCORONATOSCENE:
	case Call::UI_R_ADDCORONATOSCENE: {
		if ( args[6] < INT_MIN || args[6] > INT_MAX || args[7] < INT_MIN || args[7] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid RTCW corona identity or flags" );
		fnRTCWCorona_t corona{};
		std::memcpy( corona.origin, CL_RTCWRenderSpan( vm, args[1], sizeof( corona.origin ) ), sizeof( corona.origin ) );
		corona.color[0] = VMF(2); corona.color[1] = VMF(3); corona.color[2] = VMF(4);
		corona.scale = VMF(5); corona.id = static_cast<int>( args[6] ); corona.flags = static_cast<int>( args[7] );
		if ( !FnRTCWCoronaValid( &corona ) ) Com_Error( ERR_DROP, "Invalid RTCW corona values" );
		re.AddRTCWCoronaToScene( &corona );
		break;
	}
	case Call::CG_CM_MARKFRAGMENTS:
		result = CL_RTCWMarkFragments( vm, args );
		return true;
	case Call::CG_RB_ZOMBIEFXADDNEWHIT:
		if ( Com_GameProfile()->id != COM_GAME_RTCW_SP ) return false;
		// Original SP disables RB_ZombieFXAddNewHit before examining any
		// arguments, including the (-1, NULL, NULL) initialization call.
		// iortcw enables this effect; that is a different renderer contract.
		break;
	case Call::CG_R_SETFOG:
		for ( int i = 1; i <= 3; ++i )
			if ( args[i] < INT_MIN || args[i] > INT_MAX )
				Com_Error( ERR_DROP, "Invalid RTCW fog integer argument" );
		re.SetRTCWFog( static_cast<int>( args[1] ), static_cast<int>( args[2] ),
			static_cast<int>( args[3] ), VMF(4), VMF(5), VMF(6), VMF(7) );
		break;
	case Call::CG_GET_ENTITY_TOKEN: {
		if ( args[2] < 1 || args[2] > INT_MAX || !args[1] )
			Com_Error( ERR_DROP, "Invalid RTCW entity token buffer" );
		if ( static_cast<std::uintptr_t>( args[2] ) > (std::numeric_limits<std::uintptr_t>::max)()
			- static_cast<std::uintptr_t>( args[1] ) )
			Com_Error( ERR_DROP, "RTCW entity token address overflow" );
		VM_CHECKBOUNDS( vm, args[1], args[2] );
		auto *destination = static_cast<char *>( VM_ExplicitArgPtr( vm, args[1] ) );
		if ( !destination ) Com_Error( ERR_DROP, "Invalid RTCW entity token pointer" );
		char token[MAX_TOKEN_CHARS]{};
		const int capacity = args[2] < MAX_TOKEN_CHARS ? static_cast<int>( args[2] ) : MAX_TOKEN_CHARS;
		result = re.GetEntityToken( token, capacity );
		token[capacity - 1] = '\0';
		std::memcpy( destination, token, std::strlen( token ) + 1 );
		return true;
	}
	case Call::CG_R_LOADWORLDMAP:
		re.LoadWorld( CL_RTCWRenderName( vm, args[1] ) );
		break;
	case Call::CG_R_REGISTERMODEL:
	case Call::UI_R_REGISTERMODEL: {
		// Original RTCW silently returns handle 0 for null and empty model
		// names. Absent models must not reach the shared renderer's diagnostic.
		const char *name = args[1] ? CL_RTCWRenderName( vm, args[1] ) : nullptr;
		result = name && name[0] ? re.RegisterModel( name ) : 0;
		return true;
	}
	case Call::CG_R_REGISTERSKIN:
	case Call::UI_R_REGISTERSKIN:
		result = args[1] ? re.RegisterSkin( CL_RTCWRenderName( vm, args[1] ) ) : 0;
		return true;
	case Call::CG_R_REGISTERSHADER: {
		const char *name = CL_RTCWRenderName( vm, args[1] );
		result = re.RegisterShader( name );
		CL_HudRegisterShaderName( static_cast<qhandle_t>( result ), name );
		return true;
	}
	case Call::CG_R_GETSKINMODEL: {
		if ( !args[3] ) Com_Error( ERR_DROP, "Null RTCW skin model output" );
		VM_CHECKBOUNDS( vm, args[3], MAX_QPATH );
		void *output = VM_ExplicitArgPtr( vm, args[3] );
		if ( !output || args[1] < INT_MIN || args[1] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid RTCW skin model arguments" );
		const char *type = CL_RTCWRenderName( vm, args[2] );
		char name[MAX_QPATH]{};
		const qboolean found = re.GetSkinModel( static_cast<int>( args[1] ), type, name );
		if ( found ) {
			const char *end = static_cast<const char *>( std::memchr( name, '\0', sizeof( name ) ) );
			if ( !end ) Com_Error( ERR_DROP, "Unterminated RTCW skin model result" );
			std::memcpy( output, name, static_cast<std::size_t>( end - name ) + 1 );
		}
		result = found;
		return true;
	}
	case Call::CG_R_GETMODELSHADER:
		for ( int i = 1; i <= 3; ++i )
			if ( args[i] < INT_MIN || args[i] > INT_MAX )
				Com_Error( ERR_DROP, "Invalid RTCW model shader arguments" );
		result = re.GetShaderFromModel( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ), static_cast<int>( args[3] ) );
		return true;
	case Call::CG_R_REGISTERSHADERNOMIP:
	case Call::UI_R_REGISTERSHADERNOMIP: {
		const char *name = CL_RTCWRenderName( vm, args[1] );
		result = re.RegisterShaderNoMip( name );
		if ( call == Call::CG_R_REGISTERSHADERNOMIP ) CL_HudRegisterShaderName( static_cast<qhandle_t>( result ), name );
		return true;
	}
	case Call::CG_R_CLEARSCENE:
	case Call::UI_R_CLEARSCENE:
		re.ClearScene();
		break;
	case Call::CG_R_ADDPOLYTOSCENE:
	case Call::UI_R_ADDPOLYTOSCENE:
		CL_RTCWRenderPolys( vm, args[1], args[2], args[3], 1 );
		break;
	case Call::CG_R_ADDPOLYSTOSCENE:
	case Call::UI_R_ADDPOLYSTOSCENE:
		CL_RTCWRenderPolys( vm, args[1], args[2], args[3], args[4] );
		break;
	case Call::CG_R_ADDREFENTITYTOSCENE:
	case Call::UI_R_ADDREFENTITYTOSCENE: {
		const refEntity_t entity = CL_ModuleRefEntity( vm, args[1] );
		re.AddRefEntityToScene( &entity, qfalse );
		break;
	}
	case Call::CG_R_ADDLIGHTTOSCENE:
	case Call::UI_R_ADDLIGHTTOSCENE: {
		if ( !args[1] || args[6] < INT_MIN || args[6] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid RTCW light arguments" );
		VM_CHECKBOUNDS( vm, args[1], sizeof( vec3_t ) );
		const void *source = VM_ExplicitArgPtr( vm, args[1] );
		if ( !source ) Com_Error( ERR_DROP, "Invalid RTCW light origin" );
		vec3_t origin;
		std::memcpy( origin, source, sizeof( origin ) );
		re.AddRTCWLightToScene( origin, VMF(2), VMF(3), VMF(4), VMF(5), static_cast<int>( args[6] ) );
		break;
	}
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
		if ( !fntech3::rtcw::render::GradientQuad( VMF(1), VMF(2), VMF(3), VMF(4),
			VMF(5), VMF(6), VMF(7), VMF(8), args[9], args[10] ? color : nullptr, args[11], quad ) )
			Com_Error( ERR_DROP, "Invalid RTCW gradient picture" );
		float x = VMF(1), y = VMF(2), w = VMF(3), h = VMF(4);
		if ( !CL_HudAdjustPic( &x, &y, &w, &h, args[9] ) ) break;
		if ( !fntech3::rtcw::render::GradientQuad( x, y, w, h,
			VMF(5), VMF(6), VMF(7), VMF(8), args[9], args[10] ? color : nullptr, args[11], quad ) )
			Com_Error( ERR_DROP, "Invalid corrected RTCW gradient picture" );
		re.DrawQuad( &quad );
		break;
	}
	case Call::CG_R_DRAWROTATEDPIC: {
		if ( Com_GameProfile()->id != COM_GAME_RTCW_MP ) return false;
		refDrawQuad_t quad;
		if ( !fntech3::rtcw::render::RotatedQuad( VMF(1), VMF(2), VMF(3), VMF(4),
			VMF(5), VMF(6), VMF(7), VMF(8), args[9], VMF(10), quad ) )
			Com_Error( ERR_DROP, "Invalid RTCW rotated picture" );
		float x = VMF(1), y = VMF(2), w = VMF(3), h = VMF(4);
		if ( !CL_HudAdjustPic( &x, &y, &w, &h, args[9] ) ) break;
		if ( !fntech3::rtcw::render::RotatedQuad( x, y, w, h,
			VMF(5), VMF(6), VMF(7), VMF(8), args[9], VMF(10), quad ) )
			Com_Error( ERR_DROP, "Invalid corrected RTCW rotated picture" );
		re.DrawQuad( &quad );
		break;
	}
	case Call::CG_R_LERPTAG:
	case Call::UI_CM_LERPTAG: {
		if ( !args[1] ) Com_Error( ERR_DROP, "Null RTCW tag output" );
		VM_CHECKBOUNDS( vm, args[1], sizeof( fntech3_rtcw_orientation_t ) );
		void *output = VM_ExplicitArgPtr( vm, args[1] );
		if ( !output ) Com_Error( ERR_DROP, "Invalid RTCW tag output" );
		const refEntity_t entity = CL_ModuleRTCWTagEntity( vm, args[2] );
		const char *name = CL_RTCWRenderName( vm, args[3] );
		if ( args[4] < INT_MIN || args[4] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid RTCW tag start index" );
		orientation_t tag{};
		const int index = re.LerpTagEntity( &tag, &entity, name, static_cast<int>( args[4] ) );
		fntech3::render::WriteRTCWOrientation( output, tag );
		result = index;
		return true;
	}
	case Call::CG_R_MODELBOUNDS:
	case Call::UI_R_MODELBOUNDS: {
		if ( !args[2] || !args[3] ) Com_Error( ERR_DROP, "Null RTCW model bounds output" );
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
		const char *oldName = CL_RTCWRenderName( vm, args[1] );
		const char *newName = CL_RTCWRenderName( vm, args[2] );
		const char *time = args[3] ? CL_RTCWRenderName( vm, args[3] ) : nullptr;
		re.RemapShader( oldName, newName, time );
		break;
	}
	default:
		return false;
	}
	result = 0;
	return true;
}

#endif
