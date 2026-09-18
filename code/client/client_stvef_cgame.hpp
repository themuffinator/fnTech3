/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_CGAME_HPP
#define FNTECH3_CLIENT_STVEF_CGAME_HPP
#include "../compat/stvef_module_abi.h"
#include "../compat/stvef_state_adapter.hpp"
#include "client_stvef_records.hpp"

/* Holomatch's cgame numbers its imports as Quake III 1.32 does from 0 to 58 and
 * for the math traps, adds 59 (3D shaders), 60 (a quiet cvar set) and prints
 * at 109 and 110, and passes its own records. Calls whose meaning and records
 * are this engine's own continue into the shared dispatcher; everything else
 * is decoded here (stvoyHM.exe's cgame dispatcher at 0x402ed0 handles the same
 * set: 0-20, 22-60 and 100-110). */

static intptr_t CL_STVEFCgameSharedCall( fntech3::stvef::module::Call call ) {
	using fntech3::stvef::module::Call;
	switch ( call ) {
#define STVEF_SHARED_CALL(name) case Call::name: return name;
	STVEF_SHARED_CALL(CG_PRINT)
	STVEF_SHARED_CALL(CG_ERROR)
	STVEF_SHARED_CALL(CG_MILLISECONDS)
	STVEF_SHARED_CALL(CG_CVAR_REGISTER)
	STVEF_SHARED_CALL(CG_CVAR_UPDATE)
	STVEF_SHARED_CALL(CG_CVAR_SET)
	STVEF_SHARED_CALL(CG_CVAR_VARIABLESTRINGBUFFER)
	STVEF_SHARED_CALL(CG_ARGC)
	STVEF_SHARED_CALL(CG_ARGV)
	STVEF_SHARED_CALL(CG_ARGS)
	STVEF_SHARED_CALL(CG_FS_FOPENFILE)
	STVEF_SHARED_CALL(CG_FS_WRITE)
	STVEF_SHARED_CALL(CG_FS_FCLOSEFILE)
	STVEF_SHARED_CALL(CG_SENDCONSOLECOMMAND)
	STVEF_SHARED_CALL(CG_ADDCOMMAND)
	STVEF_SHARED_CALL(CG_SENDCLIENTCOMMAND)
	STVEF_SHARED_CALL(CG_UPDATESCREEN)
	STVEF_SHARED_CALL(CG_CM_LOADMAP)
	STVEF_SHARED_CALL(CG_CM_NUMINLINEMODELS)
	STVEF_SHARED_CALL(CG_CM_INLINEMODEL)
	STVEF_SHARED_CALL(CG_CM_TEMPBOXMODEL)
	STVEF_SHARED_CALL(CG_CM_POINTCONTENTS)
	STVEF_SHARED_CALL(CG_CM_TRANSFORMEDPOINTCONTENTS)
	STVEF_SHARED_CALL(CG_CM_BOXTRACE)
	STVEF_SHARED_CALL(CG_CM_TRANSFORMEDBOXTRACE)
	STVEF_SHARED_CALL(CG_CM_MARKFRAGMENTS)
	STVEF_SHARED_CALL(CG_S_STARTSOUND)
	STVEF_SHARED_CALL(CG_S_STARTLOCALSOUND)
	STVEF_SHARED_CALL(CG_S_ADDLOOPINGSOUND)
	STVEF_SHARED_CALL(CG_S_UPDATEENTITYPOSITION)
	STVEF_SHARED_CALL(CG_S_RESPATIALIZE)
	STVEF_SHARED_CALL(CG_R_LOADWORLDMAP)
	STVEF_SHARED_CALL(CG_R_REGISTERMODEL)
	STVEF_SHARED_CALL(CG_R_REGISTERSKIN)
	STVEF_SHARED_CALL(CG_R_REGISTERSHADER)
	STVEF_SHARED_CALL(CG_R_CLEARSCENE)
	// The shared handlers read the Elite Force entity and view records
	// through the render record adapter.
	STVEF_SHARED_CALL(CG_R_ADDREFENTITYTOSCENE)
	STVEF_SHARED_CALL(CG_R_ADDPOLYTOSCENE)
	STVEF_SHARED_CALL(CG_R_ADDLIGHTTOSCENE)
	STVEF_SHARED_CALL(CG_R_RENDERSCENE)
	STVEF_SHARED_CALL(CG_R_SETCOLOR)
	STVEF_SHARED_CALL(CG_R_DRAWSTRETCHPIC)
	STVEF_SHARED_CALL(CG_R_MODELBOUNDS)
	STVEF_SHARED_CALL(CG_R_LERPTAG)
	STVEF_SHARED_CALL(CG_GETCURRENTSNAPSHOTNUMBER)
	STVEF_SHARED_CALL(CG_GETSERVERCOMMAND)
	STVEF_SHARED_CALL(CG_GETCURRENTCMDNUMBER)
	STVEF_SHARED_CALL(CG_SETUSERCMDVALUE)
	STVEF_SHARED_CALL(CG_R_REGISTERSHADERNOMIP)
	STVEF_SHARED_CALL(CG_MEMORY_REMAINING)
	STVEF_SHARED_CALL(CG_MEMSET)
	STVEF_SHARED_CALL(CG_MEMCPY)
	STVEF_SHARED_CALL(CG_STRNCPY)
	STVEF_SHARED_CALL(CG_SIN)
	STVEF_SHARED_CALL(CG_COS)
	STVEF_SHARED_CALL(CG_ATAN2)
	STVEF_SHARED_CALL(CG_SQRT)
	STVEF_SHARED_CALL(CG_FLOOR)
	STVEF_SHARED_CALL(CG_CEIL)
#undef STVEF_SHARED_CALL
	default: return -1;
	}
}

static qboolean CL_GetSTVEFSnapshot( int number, fntech3_stvef_mp_snapshot_t *destination ) {
	if ( !destination ) return qfalse;
	const clSnapshot_t *source = CL_FindSnapshot( number );
	if ( !source ) return qfalse;
	fntech3_stvef_mp_snapshot_t result{};
	result.snapFlags = source->snapFlags;
	result.ping = source->ping;
	result.serverTime = source->serverTime;
	static_assert( sizeof( result.areamask ) == sizeof( source->areamask ) );
	std::memcpy( result.areamask, source->areamask, sizeof( result.areamask ) );
	if ( !fntech3::stvef::state::FromInternal( source->ps, result.ps ) ) return qfalse;
	result.numEntities = std::min( source->numEntities, (int)FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT );
	for ( int i = 0; i < result.numEntities; ++i ) {
		const auto index = ( static_cast<std::uint32_t>( source->parseEntitiesNum ) + i ) & ( MAX_PARSE_ENTITIES - 1 );
		if ( !fntech3::stvef::state::FromInternal( cl.parseEntities[index], result.entities[i] ) ) return qfalse;
	}
	// The reliable command boundary is carried by serverCommandSequence alone.
	result.serverCommandSequence = source->serverCommandNum;
	*destination = result;
	return qtrue;
}

static qboolean CL_GetSTVEFUserCmd( int number, fntech3_stvef_mp_usercmd_t *destination ) {
	if ( !destination ) return qfalse;
	usercmd_t source;
	if ( !CL_GetUserCmd( number, &source ) ) return qfalse;
	return fntech3::stvef::state::FromInternal( source, *destination ) ? qtrue : qfalse;
}

// Retail Cvar_SetNoModify (0x41cf50): the cgame sets "model" with it so that no
// userinfo update is announced. An existing cvar ends up with CVAR_USERINFO set
// and its modified flag clear; one that did not exist is simply created.
static void CL_STVEFCvarSetNoModify( const char *name, const char *value ) {
	const bool existed = Cvar_Flags( name ) != CVAR_NONEXISTENT;
	const int userinfoPending = cvar_modifiedFlags & CVAR_USERINFO;
	cvar_t *variable = Cvar_Set2( name, value, qtrue );
	if ( !userinfoPending ) cvar_modifiedFlags &= ~CVAR_USERINFO;
	if ( variable && existed ) {
		variable->flags |= CVAR_USERINFO;
		variable->modified = qfalse;
	}
}

static bool CL_STVEFCgameCall( intptr_t *args, intptr_t &operation, intptr_t &result ) {
	using namespace fntech3::stvef::module;
	const Call call = Lookup( COM_GAME_STVEF_MP, Module::CGAME, Direction::IMPORT, args[0] );
	result = 0;
	switch ( call ) {
	case Call::CG_FS_READ:
		// Retail reads nothing from a file that failed to open, before it looks
		// at the length (FS_Read 0x41f072); the cgame reads -1 bytes from one.
		if ( !args[3] ) return true;
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		FS_VM_ReadFile( VMA(1), args[2], args[3], H_CGAME );
		return true;
	case Call::CG_S_CLEARLOOPINGSOUNDS:
		// The cgame passes no argument; it clears the loops it re-adds each frame.
		S_ClearLoopingSounds( qfalse );
		return true;
	case Call::CG_S_REGISTERSOUND:
		// One argument; a sound that cannot be loaded is handle 0, silence.
		result = S_RegisterSound( VMA(1), qfalse );
		return true;
	case Call::CG_S_STARTBACKGROUNDTRACK: {
		// Elite Force has no stop trap: an empty intro stops the music.
		const char *intro = VMA(1);
		if ( !intro || !*intro ) S_StopBackgroundTrack();
		else S_StartBackgroundTrack( intro, VMA(2) );
		return true;
	}
	case Call::CG_GETGLCONFIG:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( fntech3_stvef_mp_glconfig_t ) );
		CL_CopySTVEFGlconfig( VMA(1), cls.glconfig );
		return true;
	case Call::CG_GETGAMESTATE: {
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( fntech3_stvef_gameState_t ) );
		fntech3_stvef_gameState_t *destination = VMA(1);
		if ( !destination || !fntech3::stvef::state::FromInternal( cl.gameState, *destination ) ) {
			Com_Error( ERR_DROP, "Invalid Elite Force gamestate module transfer" );
		}
		return true;
	}
	case Call::CG_GETSNAPSHOT:
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( fntech3_stvef_mp_snapshot_t ) );
		result = CL_GetSTVEFSnapshot( static_cast<int>( args[1] ), VMA(2) );
		return true;
	case Call::CG_GETUSERCMD:
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( fntech3_stvef_mp_usercmd_t ) );
		result = CL_GetSTVEFUserCmd( static_cast<int>( args[1] ), VMA(2) );
		return true;
	case Call::CG_R_REGISTERSHADER3D: {
		const qhandle_t shader = re.RegisterShader3D ? re.RegisterShader3D( VMA(1) ) : 0;
		CL_HudRegisterShaderName( shader, VMA(1) );
		result = shader;
		return true;
	}
	case Call::CG_CVAR_SET_NO_MODIFY:
		CL_STVEFCvarSetNoModify( VMA(1), VMA(2) );
		return true;
	case Call::CG_TESTPRINTINT:
		Com_Printf( "%s%i\n", VMA(1).String(), static_cast<int>( args[2] ) );
		return true;
	case Call::CG_TESTPRINTFLOAT:
		Com_Printf( "%s%f\n", VMA(1).String(), VMF(2) );
		return true;
	default:
		operation = CL_STVEFCgameSharedCall( call );
		if ( operation < 0 ) {
			// Retail's default case: CG_CM_LOADMODEL (21) and unknown numbers.
			Com_Error( ERR_DROP, "Bad cgame system trap: %ld", static_cast<long>( args[0] ) );
			return true;
		}
		return false;
	}
}
#endif
