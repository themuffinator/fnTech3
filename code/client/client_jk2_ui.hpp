/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_UI_HPP
#define FNTECH3_CLIENT_JK2_UI_HPP

#include "../compat/jk2_client_abi.h"
#include "client_jk2_font.hpp"

/* jk2mp.exe 0x40fb70, index0x410894, jump table0x4106fc. Its UI's
 * glconfig is43 and client state44 (opposite Quake III's ordering); its
 * cvar register/update are50/51. Only record-free services cross into the
 * shared dispatcher. Authored fonts use the bounded engine adapter; Ghoul2
 * and localization services remain explicit refusals.
 */
static intptr_t CL_JK2UISharedCall( intptr_t ordinal ) {
	switch ( ordinal ) {
	case 0: return UI_ERROR;
	case 1: return UI_PRINT;
	case 2: return UI_MILLISECONDS;
	case 3: return UI_CVAR_SET;
	case 4: return UI_CVAR_VARIABLEVALUE;
	case 5: return UI_CVAR_VARIABLESTRINGBUFFER;
	case 6: return UI_CVAR_SETVALUE;
	case 7: return UI_CVAR_RESET;
	case 8: return UI_CVAR_CREATE;
	case 9: return UI_CVAR_INFOSTRINGBUFFER;
	case 10: return UI_ARGC;
	case 11: return UI_ARGV;
	case 12: return UI_CMD_EXECUTETEXT;
	case 13: return UI_FS_FOPENFILE;
	case 14: return UI_FS_READ;
	case 15: return UI_FS_WRITE;
	case 16: return UI_FS_FCLOSEFILE;
	case 17: return UI_FS_GETFILELIST;
	case 20: return UI_R_REGISTERSHADERNOMIP;
	case 26: return UI_R_SETCOLOR;
	case 27: return UI_R_DRAWSTRETCHPIC;
	case 28: return UI_UPDATESCREEN;
	case 45: return UI_GETCONFIGSTRING;
	case 50: return UI_CVAR_REGISTER;
	case 51: return UI_CVAR_UPDATE;
	case 52: return UI_MEMORY_REMAINING;
	case 64: return UI_PC_LOAD_SOURCE;
	case 65: return UI_PC_FREE_SOURCE;
	case 73: return UI_LAN_GETSERVERCOUNT;
	case 74: return UI_LAN_GETSERVERADDRESSSTRING;
	case 75: return UI_LAN_GETSERVERINFO;
	case 76: return UI_LAN_MARKSERVERVISIBLE;
	case 77: return UI_LAN_UPDATEVISIBLEPINGS;
	case 78: return UI_LAN_RESETPINGS;
	case 79: return UI_LAN_LOADCACHEDSERVERS;
	case 80: return UI_LAN_SAVECACHEDSERVERS;
	case 81: return UI_LAN_ADDSERVER;
	case 82: return UI_LAN_REMOVESERVER;
	case 89: return UI_LAN_SERVERSTATUS;
	case 90: return UI_LAN_GETSERVERPING;
	case 91: return UI_LAN_SERVERISVISIBLE;
	case 92: return UI_LAN_COMPARESERVERS;
	case 100: return UI_MEMSET;
	case 101: return UI_MEMCPY;
	case 102: return UI_STRNCPY;
	case 103: return UI_SIN;
	case 104: return UI_COS;
	case 105: return UI_ATAN2;
	case 106: return UI_SQRT;
	/* Retail's table has floor107 and ceil110, with108/109 unhandled. */
	case 107: return UI_FLOOR;
	case 110: return UI_CEIL;
	default: return -1;
	}
}

static bool CL_JK2UICall( intptr_t *args, intptr_t &operation, intptr_t &result ) {
	result = 0;
	if ( CL_JK2FontCall( uivm, args, result, 56,
		[]( float x, float y, float w, float h, const fntech3::jk2::font::Glyph &glyph, qhandle_t shader ) {
			CL_UIAdjustStretchPic( &x, &y, &w, &h );
			re.DrawStretchPic( x, y, w, h, glyph.s, glyph.t, glyph.s2, glyph.t2, shader );
		} ) ) return true;
	switch ( args[0] ) {
	case 13:
		if ( args[2] ) VM_CHECKBOUNDS( uivm, args[2], sizeof( int32_t ) );
		operation = UI_FS_FOPENFILE;
		return false;
	case 50:
		VM_CHECKBOUNDS( uivm, args[1], 272 );
		Cvar_RegisterLegacy( VMA(1), VMA(2), VMA(3), args[4], uivm->privateFlag );
		return true;
	case 51:
		VM_CHECKBOUNDS( uivm, args[1], 272 );
		Cvar_UpdateLegacy( VMA(1), uivm->privateFlag );
		return true;
	case 31: // One argument; no Quake III compression flag follows it.
		result = S_RegisterSound( VMA(1), qfalse );
		return true;
	case 43:
		VM_CHECKBOUNDS( uivm, args[1], sizeof( fntech3_jk2_mp_glconfig_t ) );
		static_assert( sizeof( fntech3_jk2_mp_glconfig_t ) == sizeof( fntech3_q3_glconfig_t ) );
		CL_CopyQ3Glconfig( VMA(1), re.GetConfig() );
		return true;
	case 44: {
		VM_CHECKBOUNDS( uivm, args[1], sizeof( fntech3_jk2_mp_uiClientState_t ) );
		fntech3_jk2_mp_uiClientState_t record{};
		record.connState = cls.state;
		record.connectPacketCount = clc.connectPacketCount;
		record.clientNum = cl.snap.ps.clientNum;
		Q_strncpyz( record.servername, cls.servername, sizeof( record.servername ) );
		Q_strncpyz( record.updateInfoString, cls.updateInfoString, sizeof( record.updateInfoString ) );
		Q_strncpyz( record.messageString, clc.serverMessage, sizeof( record.messageString ) );
		void *destination = VMA(1);
		if ( !destination ) Com_Error( ERR_DROP, "Missing Jedi Outcast UI client state output" );
		std::memcpy( destination, &record, sizeof( record ) );
		return true;
	}
	default:
		operation = CL_JK2UISharedCall( args[0] );
		if ( operation >= 0 ) return false;
		const char *boundary = args[0] >= 56 && args[0] <= 61 ? "font"
			: args[0] >= 200 ? "localization/Ghoul2" : "menu service";
		Com_Error( ERR_DROP, "Unsupported jk2-mp UI import %ld: Jedi Outcast's %s boundary is open.",
			static_cast<long>( args[0] ), boundary );
		return true;
	}
}
#endif
