/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_CGAME_HPP
#define FNTECH3_CLIENT_JK2_CGAME_HPP

#include "../compat/jk2_client_adapter.hpp"
#include "client_jk2_font.hpp"
#include "client_jk2_strings.hpp"

/* The low services were identified by jk2mp.exe's own dispatch targets at
 * 0x4017a0 (index 0x4031a0, jump table 0x402f14). Never fall through by number:
 * its remove-command is16, send-client-command17 and collision load19.
 * Math floor/ceil are110/111, whereas Quake III's are107/108.
 * Unimplemented Raven renderer, FX, ambient and Ghoul2 calls are refused.
 */
static intptr_t CL_JK2CgameSharedCall( intptr_t ordinal ) {
	switch ( ordinal ) {
	case 0: return CG_PRINT;
	case 1: return CG_ERROR;
	case 2: return CG_MILLISECONDS;
	case 3: return CG_CVAR_REGISTER;
	case 4: return CG_CVAR_UPDATE;
	case 5: return CG_CVAR_SET;
	case 6: return CG_CVAR_VARIABLESTRINGBUFFER;
	case 7: return CG_ARGC;
	case 8: return CG_ARGV;
	case 9: return CG_ARGS;
	case 10: return CG_FS_FOPENFILE;
	case 11: return CG_FS_READ;
	case 12: return CG_FS_WRITE;
	case 13: return CG_FS_FCLOSEFILE;
	case 14: return CG_SENDCONSOLECOMMAND;
	case 15: return CG_ADDCOMMAND;
	case 16: return CG_REMOVECOMMAND;
	case 17: return CG_SENDCLIENTCOMMAND;
	case 18: return CG_UPDATESCREEN;
	case 19: return CG_CM_LOADMAP;
	case 20: return CG_CM_NUMINLINEMODELS;
	case 21: return CG_CM_INLINEMODEL;
	case 221: return CG_GETCURRENTSNAPSHOTNUMBER;
	case 223: return CG_GETSERVERCOMMAND;
	case 224: return CG_GETCURRENTCMDNUMBER;
	case 232: return CG_MEMORY_REMAINING;
	case 100: return CG_MEMSET;
	case 101: return CG_MEMCPY;
	case 102: return CG_STRNCPY;
	case 103: return CG_SIN;
	case 104: return CG_COS;
	case 105: return CG_ATAN2;
	case 106: return CG_SQRT;
	case 110: return CG_FLOOR;
	case 111: return CG_CEIL;
	default: return -1;
	}
}

static bool CL_JK2CgameCall( intptr_t *args, intptr_t &operation, intptr_t &result ) {
	result = 0;
	if ( CL_JK2FontCall( cgvm, args, result, 47,
		[]( float x, float y, float w, float h, const fntech3::jk2::font::Glyph &glyph, qhandle_t shader ) {
			// Raven's virtual 640x480 screen, stretched as the original drew it.
			CL_HudFromVirtual( &x, &y, &w, &h );
			CL_HudDrawStretchPic( x, y, w, h, glyph.s, glyph.t, glyph.s2, glyph.t2, shader );
		} ) ) return true;
	switch ( args[0] ) {
	case 271:
		result = CL_JK2StringText( cgvm, args[1], args[2], args[3], true );
		return true;
	case 272:
		result = CL_JK2StringRegister( cgvm, args[1], 1 );
		return true;
	case 3:
		VM_CHECKBOUNDS( cgvm, args[1], 272 );
		Cvar_RegisterLegacy( VMA(1), VMA(2), VMA(3), args[4], cgvm->privateFlag );
		return true;
	case 4:
		VM_CHECKBOUNDS( cgvm, args[1], 272 );
		Cvar_UpdateLegacy( VMA(1), cgvm->privateFlag );
		return true;
	case 10:
		if ( args[2] ) VM_CHECKBOUNDS( cgvm, args[2], sizeof( int32_t ) );
		operation = CG_FS_FOPENFILE;
		return false;
	case 219: { // 0x401020 copies 11332 bytes.
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( fntech3_jk2_mp_glconfig_t ) );
		static_assert( sizeof( fntech3_jk2_mp_glconfig_t ) == sizeof( fntech3_q3_glconfig_t ) );
		CL_CopyQ3Glconfig( VMA(1), &cls.glconfig );
		return true;
	}
	case 220: {
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( fntech3_jk2_mp_gameState_t ) );
		auto *destination = static_cast<fntech3_jk2_mp_gameState_t *>( VMA(1) );
		if ( !destination || !fntech3::jk2::client::GameState( cl.gameState, *destination ) )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast multiplayer gamestate transfer" );
		return true;
	}
	case 222: {
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( fntech3_jk2_mp_snapshot_t ) );
		auto *destination = static_cast<fntech3_jk2_mp_snapshot_t *>( VMA(2) );
		const auto *source = CL_FindSnapshot( static_cast<int>( args[1] ) );
		result = destination && source && fntech3::jk2::client::SnapshotTransfer(
			*source, cl.parseEntities, MAX_PARSE_ENTITIES, *destination ) ? qtrue : qfalse;
		return true;
	}
	case 225: {
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( fntech3_jk2_usercmd_t ) );
		auto *destination = static_cast<fntech3_jk2_usercmd_t *>( VMA(2) );
		usercmd_t source{};
		result = destination && CL_GetUserCmd( static_cast<int>( args[1] ), &source ) &&
			fntech3::jk2::state::FromInternal( source, *destination ) ? qtrue : qfalse;
		return true;
	}
	default:
		operation = CL_JK2CgameSharedCall( args[0] );
		if ( operation >= 0 ) return false;
		const char *boundary = args[0] >= 278 && args[0] <= 299 ? "Ghoul2"
			: args[0] >= 254 && args[0] <= 270 ? "effects"
			: args[0] >= 31 && args[0] <= 41 ? "audio channel"
			: args[0] >= 42 && args[0] <= 218 ? "renderer/font"
			: "client service";
		Com_Error( ERR_DROP, "Unsupported jk2-mp cgame import %ld: Jedi Outcast's %s boundary is open.",
			static_cast<long>( args[0] ), boundary );
		return true;
	}
}
#endif
