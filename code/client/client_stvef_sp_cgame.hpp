/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_SP_CGAME_HPP
#define FNTECH3_CLIENT_STVEF_SP_CGAME_HPP
#include "../compat/stvef_draw_adapter.hpp"
#include "client_stvef_sp_ambient.hpp"
#include "../compat/stvef_module_abi.h"
#include "../compat/stvef_state_adapter.hpp"
#include "client_stvef_records.hpp"

/*
 * Elite Force SP's cgame is the other half of efgamex86.dll, which the server
 * loads and attaches before the game's Init. Retail calls the cgame's vmMain
 * directly and answers its 71 imports in one dispatcher (stvoy.exe 0x402b20,
 * numbered as in Raven's SP SDK). Here the client drives the cgame through
 * cgvm: CL_STVEFSPCgameEntry turns the engine's cgame calls into vmMain calls,
 * and once the cgame has started its imports go through the shared cgame
 * dispatcher, which hands single player's numbers and records to
 * CL_STVEFSPCgameCall. The game half also calls cgame code while a level
 * spawns, before the cgame starts; CL_STVEFSPEarlyCgameCall answers those.
 */

/* The loading screen draws the kept picture of the screen, 256 pixels
 * square, where a level has no levelshot (CG_R_DRAWSCREENSHOT at 0x403006).
 * Retail draws whatever its one buffer holds, which is black until a
 * savegame or a level fills it, and takes no picture of its own here: a
 * frame being drawn is not one to keep. */
static void CL_STVEFSPDrawScreenshot( float x, float y, float w, float h ) {
	STVEFSPScreenshot &screenshot = CL_STVEFSPScreenshot();
	re.DrawSTVEFRaw( static_cast<int>( x ), static_cast<int>( y ), static_cast<int>( w ), static_cast<int>( h ),
		FNTECH3_STVEF_SP_SCREENSHOT_SIZE, FNTECH3_STVEF_SP_SCREENSHOT_SIZE, screenshot.rgba, 1.0f );
}

/* A snapshot in single player's record. Retail fills the flags, ping, time,
 * area mask, command number, player state, at most 256 entities and the
 * command sequence, and leaves the rest of the cgame's record alone
 * (CL_GetSnapshot 0x4026a0). A snapshot that cannot be expressed publishes
 * nothing.
 *
 * Retail's client goes active with its first snapshot, and single player's
 * snapshots have no inactive flag: their second bit reports dropped commands,
 * which the cgame never reads. This client waits for a snapshot of the
 * player in the world, so the ones before it are not shown to the cgame,
 * which would take one as its first while the client's clock still stood
 * before it. */
static qboolean CL_GetSTVEFSPSnapshot( int number, fntech3_stvef_sp_snapshot_t *destination ) {
	if ( !destination ) return qfalse;
	const clSnapshot_t *source = CL_FindSnapshot( number );
	if ( !source || ( source->snapFlags & SNAPFLAG_NOT_ACTIVE ) ) return qfalse;
	static fntech3_stvef_sp_snapshot_t result;
	result = *destination;
	result.snapFlags = source->snapFlags & SNAPFLAG_RATE_DELAYED;
	if ( source->snapFlags & SNAPFLAG_STVEF_SP_DROPPED_COMMANDS ) {
		result.snapFlags |= 2;
	}
	result.ping = source->ping;
	result.serverTime = source->serverTime;
	static_assert( sizeof( result.areamask ) == sizeof( source->areamask ) );
	std::memcpy( result.areamask, source->areamask, sizeof( result.areamask ) );
	result.cmdNum = source->cmdNum;
	if ( !fntech3::stvef::state::FromInternal( source->ps, result.ps ) ) return qfalse;
	int count = source->numEntities;
	if ( count > FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count, FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT );
		count = FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT;
	}
	result.numEntities = count;
	for ( int i = 0; i < count; ++i ) {
		const auto index = ( static_cast<std::uint32_t>( source->parseEntitiesNum ) + i ) & ( MAX_PARSE_ENTITIES - 1 );
		if ( !fntech3::stvef::state::FromInternal( cl.parseEntities[index], result.entities[i] ) ) return qfalse;
	}
	result.serverCommandSequence = source->serverCommandNum;
	*destination = result;
	return qtrue;
}

static qboolean CL_GetSTVEFSPUserCmd( int number, fntech3_stvef_sp_usercmd_t *destination ) {
	if ( !destination ) return qfalse;
	usercmd_t source;
	if ( !CL_GetUserCmd( number, &source ) ) return qfalse;
	return fntech3::stvef::state::FromInternal( source, *destination ) ? qtrue : qfalse;
}

/* Calls answered the same whether or not the cgame has started. */
static intptr_t CL_STVEFSPCgameCommonCall( fntech3::stvef::module::Call call, intptr_t *args, bool &handled ) {
	using fntech3::stvef::module::Call;
	handled = true;
	switch ( call ) {
	case Call::CG_ERROR:
		// Retail drops with the text in red (0x402b4f).
		Com_Error( ERR_DROP, "^1%s", reinterpret_cast<const char *>( args[1] ) );
		return 0;
	case Call::CG_FS_READ:
		// Nothing is read from a file that failed to open (FS_Read 0x414cf2).
		if ( !args[3] ) return 0;
		FS_VM_ReadFile( reinterpret_cast<void *>( args[1] ), static_cast<int>( args[2] ),
			static_cast<fileHandle_t>( args[3] ), H_CGAME );
		return 0;
	case Call::CG_S_REGISTERSOUND:
		// One argument; the sound system registers even while a level loads.
		return S_RegisterSound( reinterpret_cast<const char *>( args[1] ), qfalse );
	case Call::CG_FF_STARTFX:
		CL_STVEFSPFeedbackPlay( static_cast<int>( args[1] ), qfalse );
		return 0;
	case Call::CG_FF_ENSUREFX:
		CL_STVEFSPFeedbackPlay( static_cast<int>( args[1] ), qtrue );
		return 0;
	case Call::CG_FF_STOPFX:
		CL_STVEFSPFeedbackStop( static_cast<int>( args[1] ) );
		return 0;
	case Call::CG_FF_STOPALLFX:
		CL_STVEFSPFeedbackStopAll();
		return 0;
	case Call::CG_S_GETSAMPLELENGTH:
		return S_STVEFSoundLength( static_cast<sfxHandle_t>( args[1] ) );
	case Call::CG_S_UPDATEAMBIENTSET:
		// The room the player is in, played where they are. Retail plays
		// nothing before its sound system has started (0x402d83).
		if ( cls.soundRegistered ) {
			CL_STVEFSPAmbientUpdateSet( reinterpret_cast<const char *>( args[1] ),
				reinterpret_cast<const float *>( args[2] ) );
		}
		return 0;
	case Call::CG_AS_PARSESETS:
		CL_STVEFSPAmbientParseSets();
		return 0;
	case Call::CG_AS_ADDENTRY:
		CL_STVEFSPAmbientAddPrecacheEntry( reinterpret_cast<const char *>( args[1] ) );
		return 0;
	case Call::CG_S_ADDLOCALSET:
		// A set around a point, which answers when it last played; retail
		// answers the client's time for a set it does not know (0x41ddc2).
		return CL_STVEFSPAmbientAddLocalSet( reinterpret_cast<const char *>( args[1] ),
			reinterpret_cast<const float *>( args[2] ), reinterpret_cast<const float *>( args[3] ),
			static_cast<int>( args[4] ), static_cast<int>( args[5] ) );
	case Call::CG_AS_GETBMODELSOUND:
		// A mover's sound, or -1 where its set is not one this engine read
		// (0x41de1f). The sets are read as the cgame starts, so a level's
		// first movers ask before there are any, as they do in retail.
		return CL_STVEFSPAmbientBModelSound( reinterpret_cast<const char *>( args[1] ),
			static_cast<int>( args[2] ) );
	case Call::CG_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();
	default:
		handled = false;
		return 0;
	}
}

/* Imports while the cgame runs. Calls whose meaning and records are the
 * engine's continue into the shared dispatcher through operation. */
static bool CL_STVEFSPCgameCall( intptr_t *args, intptr_t &operation, intptr_t &result ) {
	using namespace fntech3::stvef::module;
	const Call call = Lookup( COM_GAME_STVEF_SP, Module::CGAME, Direction::IMPORT, args[0] );
	bool handled = false;
	result = CL_STVEFSPCgameCommonCall( call, args, handled );
	if ( handled ) return true;
	result = 0;
	switch ( call ) {
	case Call::CG_S_STARTSOUND:
		S_StartSound( VMA(1), static_cast<int>( args[2] ), CL_STVEFSPSoundChannel( static_cast<int>( args[3] ) ),
			static_cast<sfxHandle_t>( args[4] ) );
		return true;
	case Call::CG_S_STARTLOCALSOUND:
		S_StartLocalSound( static_cast<sfxHandle_t>( args[1] ), CL_STVEFSPSoundChannel( static_cast<int>( args[2] ) ) );
		return true;
	case Call::CG_S_CLEARLOOPINGSOUNDS:
		// No argument; the cgame clears the loops it adds again each frame.
		S_ClearLoopingSounds( qfalse );
		return true;
	case Call::CG_S_STARTBACKGROUNDTRACK: {
		// An empty intro stops the music (S_StartBackgroundTrack 0x421c40).
		const char *intro = VMA(1);
		if ( !intro || !*intro ) S_StopBackgroundTrack();
		else S_StartBackgroundTrack( intro, VMA(2) );
		return true;
	}
	case Call::CG_R_GETLIGHTING: {
		// The light grid at a point; retail leaves zeros where a map has none
		// (R_GetLighting 0x45bf10).
		float *ambient = VMA(2), *directed = VMA(3), *direction = VMA(4);
		if ( !re.LightForPoint( VMA(1), ambient, directed, direction ) ) {
			VectorClear( ambient );
			VectorClear( directed );
			VectorClear( direction );
		}
		return true;
	}
	case Call::CG_R_DRAWSCREENSHOT:
		CL_STVEFSPDrawScreenshot( VMF(1), VMF(2), VMF(3), VMF(4) );
		return true;
	case Call::CG_R_DRAWROTATEPIC: {
		float x = VMF(1), y = VMF(2), w = VMF(3), h = VMF(4);
		if ( !CL_HudAdjustPic( &x, &y, &w, &h, static_cast<qhandle_t>( args[10] ) ) ) return true;
		// Turned by an angle in degrees about its top-right corner.
		refDrawQuad_t quad;
		if ( !fntech3::stvef::render::RotatedQuad( x, y, w, h, VMF(5), VMF(6), VMF(7), VMF(8), VMF(9),
			static_cast<qhandle_t>( args[10] ), quad ) )
			Com_Error( ERR_DROP, "Invalid Elite Force rotated picture" );
		re.DrawQuad( &quad );
		return true;
	}
	case Call::CG_R_SCISSOR:
		re.SetSTVEFScissor( VMF(1), VMF(2), VMF(3), VMF(4) );
		return true;
	case Call::CG_GETGLCONFIG:
		CL_CopySTVEFSPGlconfig( VMA(1), cls.glconfig );
		return true;
	case Call::CG_GETGAMESTATE: {
		fntech3_stvef_gameState_t *destination = VMA(1);
		if ( !destination || !fntech3::stvef::state::FromInternal( cl.gameState, *destination ) ) {
			Com_Error( ERR_DROP, "Invalid Elite Force gamestate module transfer" );
		}
		return true;
	}
	case Call::CG_GETSNAPSHOT:
		result = CL_GetSTVEFSPSnapshot( static_cast<int>( args[1] ), VMA(2) );
		return true;
	case Call::CG_GETUSERCMD:
		result = CL_GetSTVEFSPUserCmd( static_cast<int>( args[1] ), VMA(2) );
		return true;
	default:
		operation = CL_STVEFCgameSharedCall( call );
		if ( operation < 0 ) {
			// Retail's default case: numbers beyond 70 (0x402b2a).
			Com_Error( ERR_DROP, "Bad cgame system trap: %ld", static_cast<long>( args[0] ) );
			return true;
		}
		return false;
	}
}

/* Imports before the cgame starts. The game half calls cgame code as a level
 * spawns: it precaches its characters' models, sounds and effects and asks
 * for movers' sounds. Retail's renderer is shut down then and refuses models
 * ("called before ready"); the cgame registers every character's media again
 * as it starts, so a registration here answers 0 until the renderer runs.
 * Sounds start only once the cgame has (retail tests cls.cgameStarted,
 * 0x402d5a). */
static intptr_t CL_STVEFSPEarlyCgameCall( intptr_t *args ) {
	using namespace fntech3::stvef::module;
	const Call call = Lookup( COM_GAME_STVEF_SP, Module::CGAME, Direction::IMPORT, args[0] );
	auto text = [args]( int index ) { return reinterpret_cast<const char *>( args[index] ); };
	auto pointer = [args]( int index ) { return reinterpret_cast<void *>( args[index] ); };
	auto vector = [args]( int index ) { return reinterpret_cast<float *>( args[index] ); };
	bool handled = false;
	const intptr_t common = CL_STVEFSPCgameCommonCall( call, args, handled );
	if ( handled ) return common;
	switch ( call ) {
	case Call::CG_PRINT:
		Com_Printf( "%s", text( 1 ) );
		return 0;
	case Call::CG_MILLISECONDS:
		return Sys_Milliseconds();
	case Call::CG_CVAR_REGISTER:
		Cvar_RegisterLegacy( pointer( 1 ), text( 2 ), text( 3 ), static_cast<int>( args[4] ), 0 );
		return 0;
	case Call::CG_CVAR_UPDATE:
		Cvar_UpdateLegacy( pointer( 1 ), 0 );
		return 0;
	case Call::CG_CVAR_SET:
		Cvar_SetSafe( text( 1 ), text( 2 ) );
		return 0;
	case Call::CG_ARGC:
		return Cmd_Argc();
	case Call::CG_ARGV:
		Cmd_ArgvBuffer( static_cast<int>( args[1] ), static_cast<char *>( pointer( 2 ) ), static_cast<int>( args[3] ) );
		return 0;
	case Call::CG_ARGS:
		Cmd_ArgsBuffer( static_cast<char *>( pointer( 1 ) ), static_cast<int>( args[2] ) );
		return 0;
	case Call::CG_FS_FOPENFILE:
		return FS_VM_OpenFile( text( 1 ), static_cast<fileHandle_t *>( pointer( 2 ) ),
			static_cast<fsMode_t>( args[3] ), H_CGAME );
	case Call::CG_FS_WRITE:
		FS_VM_WriteFile( pointer( 1 ), static_cast<int>( args[2] ), static_cast<fileHandle_t>( args[3] ), H_CGAME );
		return 0;
	case Call::CG_FS_FCLOSEFILE:
		FS_VM_CloseFile( static_cast<fileHandle_t>( args[1] ), H_CGAME );
		return 0;
	case Call::CG_SENDCONSOLECOMMAND:
		Cbuf_AddText( text( 1 ) );
		return 0;
	case Call::CG_CM_NUMINLINEMODELS:
		return CM_NumInlineModels();
	case Call::CG_CM_INLINEMODEL:
		return CM_InlineModel( static_cast<int>( args[1] ) );
	case Call::CG_CM_TEMPBOXMODEL:
		return CM_TempBoxModel( vector( 1 ), vector( 2 ), qfalse );
	case Call::CG_CM_POINTCONTENTS:
		return CM_PointContents( vector( 1 ), static_cast<clipHandle_t>( args[2] ) );
	case Call::CG_CM_TRANSFORMEDPOINTCONTENTS:
		return CM_TransformedPointContents( vector( 1 ), static_cast<clipHandle_t>( args[2] ), vector( 3 ), vector( 4 ) );
	case Call::CG_CM_BOXTRACE:
		CM_BoxTrace( static_cast<trace_t *>( pointer( 1 ) ), vector( 2 ), vector( 3 ), vector( 4 ), vector( 5 ),
			static_cast<clipHandle_t>( args[6] ), static_cast<int>( args[7] ), qfalse );
		return 0;
	case Call::CG_CM_TRANSFORMEDBOXTRACE:
		CM_TransformedBoxTrace( static_cast<trace_t *>( pointer( 1 ) ), vector( 2 ), vector( 3 ), vector( 4 ),
			vector( 5 ), static_cast<clipHandle_t>( args[6] ), static_cast<int>( args[7] ), vector( 8 ), vector( 9 ), qfalse );
		return 0;
	case Call::CG_R_REGISTERMODEL:
		return cls.rendererStarted ? re.RegisterModel( text( 1 ) ) : 0;
	case Call::CG_R_REGISTERSKIN:
		return cls.rendererStarted ? re.RegisterSkin( text( 1 ) ) : 0;
	case Call::CG_R_REGISTERSHADER:
		return cls.rendererStarted ? re.RegisterShader( text( 1 ) ) : 0;
	case Call::CG_R_REGISTERSHADERNOMIP:
		return cls.rendererStarted ? re.RegisterShaderNoMip( text( 1 ) ) : 0;
	case Call::Invalid:
		// Numbers beyond 70 (0x402b2a).
		Com_Error( ERR_DROP, "Bad cgame system trap: %ld", static_cast<long>( args[0] ) );
		return 0;
	default:
		// Sounds, the scene, snapshots and commands wait for the cgame.
		return 0;
	}
}

/* cgvm's entry point: the engine's cgame calls as calls on the cgame's
 * vmMain. Retail starts it with the command sequence the gamestate carried
 * (CL_InitCGame 0x4032cf) and draws with the server time and stereo frame
 * (0x403358); single player has no other exports the engine calls. */
static intptr_t QDECL CL_STVEFSPCgameEntry( int command, ... ) {
	std::int32_t arg[3] = {};
	va_list ap;
	va_start( ap, command );
	for ( std::int32_t &value : arg ) value = va_arg( ap, std::int32_t );
	va_end( ap );

	const vmMainFunc_t cgame = SV_STVEFSPCgameMain();
	if ( !cgame ) {
		Com_Error( ERR_DROP, "Elite Force SP cgame called without its game library" );
		return 0;
	}
	switch ( command ) {
	case CG_INIT:
		return cgame( FNTECH3_STVEF_SP_CG_INIT, clc.serverCommandSequence );
	case CG_SHUTDOWN:
		return cgame( FNTECH3_STVEF_SP_CG_SHUTDOWN );
	case CG_CONSOLE_COMMAND:
		return cgame( FNTECH3_STVEF_SP_CG_CONSOLE_COMMAND );
	case CG_DRAW_ACTIVE_FRAME:
		return cgame( FNTECH3_STVEF_SP_CG_DRAW_ACTIVE_FRAME, arg[0], arg[1], 0 );
	case CG_CROSSHAIR_PLAYER:
		return cgame( FNTECH3_STVEF_SP_CG_CROSSHAIR_PLAYER );
	case CG_LAST_ATTACKER:
		// vmMain's answer for an export it does not have.
		return -1;
	default:
		return 0;
	}
}

#endif
