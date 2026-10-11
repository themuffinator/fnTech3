/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JK2_UI_HPP
#define FNTECH3_CLIENT_JK2_UI_HPP

#include "../compat/jk2_client_abi.h"
#include "client_jk2_font.hpp"
#include "client_jk2_strings.hpp"
#include <array>
#include <climits>
#include <limits>

static void *CL_JK2UISpan( intptr_t address, std::size_t bytes ) {
	if ( !uivm || !address || bytes > ( std::numeric_limits<std::uintptr_t>::max )() - std::uintptr_t( address ) )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast UI memory span" );
	// QVM arguments are offsets. Validate their full width before the shared
	// bounds check narrows them to unsigned; native modules use pointers.
	if ( !uivm->entryPoint && !uivm->dllExports
		&& ( !uivm->dataBase || std::uintptr_t( address ) > ( std::numeric_limits<unsigned int>::max )() ) )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast UI memory offset" );
	VM_CHECKBOUNDS( uivm, address, bytes );
	return VM_ExplicitArgPtr( uivm, address );
}

#include "client_jk2_ui_render.hpp"

/* Retail 1.02a UI imports63/68/69 use botlib slots0x1fc/0x210/0x214
 * (jk2mp.exe 0x410460/0x410502/0x410520). Its export initializer at
 * 0x4b1072/0x4b107c installs definition-file loading0x4b59f0 and clearing
 * 0x4b38e0. Use the engine's bounded parser with owned input text. */
static bool CL_JK2UIDefinitionCall( intptr_t *args, intptr_t &result ) {
	if ( args[0] != 63 && args[0] != 64 && args[0] != 68 && args[0] != 69 ) return false;
	if ( !uivm ) Com_Error( ERR_DROP, "Jedi Outcast UI definition call has no module" );
	if ( !botlib_export ) Com_Error( ERR_DROP, "Jedi Outcast UI definition parser is unavailable" );
	if ( args[0] == 69 ) {
		if ( !botlib_export->PC_RemoveAllGlobalDefines )
			Com_Error( ERR_DROP, "Jedi Outcast UI definition parser is unavailable" );
		botlib_export->PC_RemoveAllGlobalDefines();
		result = 0;
		return true;
	}
	std::array<char, MAX_TOKEN_CHARS> text{};
	const std::size_t limit = args[0] == 63 ? text.size() : MAX_QPATH;
	for ( std::size_t n = 0; n < limit; ++n ) {
		if ( !args[1] || std::uintptr_t( args[1] ) >= ( std::numeric_limits<std::uintptr_t>::max )() - n )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast UI definition address" );
		const auto address = std::uintptr_t( args[1] ) + n;
		text[n] = *static_cast<const char *>( CL_JK2UISpan( address, 1 ) );
		if ( !text[n] ) {
			if ( args[0] == 64 ) {
				// Retail0x4d40f5 accepts leading@ into0x4d39a0, which
				// returns TT_NAME4 and subtype=length including@. This is
				// the existing Raven opt-in; ordinary Q3 sources stay unchanged.
				if ( !botlib_export->PC_LoadJKASourceHandle )
					Com_Error( ERR_DROP, "Jedi Outcast UI source parser is unavailable" );
				result = botlib_export->PC_LoadJKASourceHandle( text.data() );
			} else if ( args[0] == 68 ) {
				if ( !botlib_export->PC_LoadGlobalDefines )
					Com_Error( ERR_DROP, "Jedi Outcast UI definition parser is unavailable" );
				result = botlib_export->PC_LoadGlobalDefines( text.data() );
			} else {
				if ( !botlib_export->PC_AddGlobalDefine )
					Com_Error( ERR_DROP, "Jedi Outcast UI definition parser is unavailable" );
				result = botlib_export->PC_AddGlobalDefine( text.data() );
			}
			return true;
		}
	}
	Com_Error( ERR_DROP, "Jedi Outcast UI definition input exceeds %u bytes", static_cast<unsigned>( limit - 1 ) );
	return true;
}

/* UI66/67 dispatch to botlib+0x208/+0x20c (0x4104b0/0x4104d1).
 * Marshal the measured PODs and stage both diagnostic outputs before writing.
 * Failed reads leave caller storage unchanged instead of exposing scratch. */
static bool CL_JK2UIRecordCall( intptr_t *args, intptr_t &result ) {
	if ( args[0] != 66 && args[0] != 67 && args[0] != 72 ) return false;
	if ( !uivm ) Com_Error( ERR_DROP, "Jedi Outcast UI record call has no module" );
	if ( args[0] == 72 ) {
		// Retail0x428770 accepts null: return epoch seconds without a record.
		void *destination = args[1] ? CL_JK2UISpan( args[1], sizeof( fntech3_jk2_mp_qtime_t ) ) : nullptr;
		qtime_t internal{};
		internal.tm_mon = -1;
		result = Com_RealTime( destination ? &internal : nullptr );
		if ( destination && internal.tm_mon >= 0 ) {
			const fntech3_jk2_mp_qtime_t external{ internal.tm_sec, internal.tm_min, internal.tm_hour,
				internal.tm_mday, internal.tm_mon, internal.tm_year, internal.tm_wday, internal.tm_yday, internal.tm_isdst };
			std::memcpy( destination, &external, sizeof( external ) );
		}
		return true;
	}
	if ( args[1] < INT_MIN || args[1] > INT_MAX || !botlib_export )
		Com_Error( ERR_DROP, "Invalid Jedi Outcast UI parser handle or unavailable parser" );
	if ( args[0] == 66 ) {
		void *destination = CL_JK2UISpan( args[2], sizeof( fntech3_jk2_mp_pc_token_t ) );
		if ( !botlib_export->PC_ReadTokenHandle ) Com_Error( ERR_DROP, "Jedi Outcast UI token reader is unavailable" );
		pc_token_t internal{};
		result = botlib_export->PC_ReadTokenHandle( static_cast<int>( args[1] ), &internal );
		if ( result ) {
			const auto *end = static_cast<const char *>( std::memchr( internal.string, 0, sizeof( internal.string ) ) );
			if ( !end || end - internal.string >= 1024 ) Com_Error( ERR_DROP, "Invalid Jedi Outcast UI parser token" );
			fntech3_jk2_mp_pc_token_t external{};
			external.type = internal.type; external.subtype = internal.subtype; external.intvalue = internal.intvalue;
			external.floatvalue = internal.floatvalue;
			std::memcpy( external.string, internal.string, std::size_t( end - internal.string ) + 1 );
			std::memcpy( destination, &external, sizeof( external ) );
		}
		return true;
	}
	void *filename = CL_JK2UISpan( args[2], FNTECH3_JK2_MP_SOURCE_FILENAME_CHARS );
	void *line = CL_JK2UISpan( args[3], sizeof( std::int32_t ) );
	if ( !botlib_export->PC_SourceFileAndLine ) Com_Error( ERR_DROP, "Jedi Outcast UI source diagnostics are unavailable" );
	std::array<char, MAX_STRING_CHARS> internal{};
	int internalLine = 0;
	result = botlib_export->PC_SourceFileAndLine( static_cast<int>( args[1] ), internal.data(), &internalLine );
	if ( result ) {
		const auto *end = static_cast<const char *>( std::memchr( internal.data(), 0, internal.size() ) );
		if ( !end || end - internal.data() >= FNTECH3_JK2_MP_SOURCE_FILENAME_CHARS || internalLine < 0 )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast UI source diagnostic record" );
		const std::int32_t externalLine = internalLine;
		std::memcpy( filename, internal.data(), std::size_t( end - internal.data() ) + 1 );
		std::memcpy( line, &externalLine, sizeof( externalLine ) );
	}
	return true;
}

/* jk2mp.exe 0x40fb70, index0x410894, jump table0x4106fc. Its UI's
 * glconfig is43 and client state44 (opposite Quake III's ordering); its
 * cvar register/update are50/51. Only record-free services cross into the
 * shared dispatcher. Authored fonts use the bounded engine adapter; Ghoul2
 * remains an explicit refusal; StripEd packages use their own adapter.
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
	case 37: return UI_KEY_GETOVERSTRIKEMODE;
	case 38: return UI_KEY_SETOVERSTRIKEMODE;
	case 39: return UI_KEY_CLEARSTATES;
	case 40: return UI_KEY_GETCATCHER;
	case 41: return UI_KEY_SETCATCHER;
	case 45: return UI_GETCONFIGSTRING;
	case 50: return UI_CVAR_REGISTER;
	case 51: return UI_CVAR_UPDATE;
	case 52: return UI_MEMORY_REMAINING;
	case 65: return UI_PC_FREE_SOURCE;
	// Retail0x410530/0x41053a: no arguments, then intro/loop names.
	case 70: return UI_S_STOPBACKGROUNDTRACK;
	case 71: return UI_S_STARTBACKGROUNDTRACK;
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
	if ( CL_JK2UIRenderCall( args, result ) ) return true;
	if ( CL_JK2UIDefinitionCall( args, result ) ) return true;
	if ( CL_JK2UIRecordCall( args, result ) ) return true;
	if ( CL_JK2FontCall( uivm, args, result, 56,
		[]( float x, float y, float w, float h, const fntech3::jk2::font::Glyph &glyph, qhandle_t shader ) {
			CL_UIAdjustStretchPic( &x, &y, &w, &h );
			re.DrawStretchPic( x, y, w, h, glyph.s, glyph.t, glyph.s2, glyph.t2, shader );
		} ) ) return true;
	switch ( args[0] ) {
	case 33:
	case 34:
		// Retail 0x411520 / 0x411550 take original Q3 key numbers.
		if ( !args[2] || args[3] <= 0 || args[3] > 65536 )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast key string output" );
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		CL_Q3KeyString( args[1], VMA(2), args[3], args[0] == 34 ? qtrue : qfalse );
		return true;
	case 35:
		CL_Q3SetBinding( args[1], VMA(2) );
		return true;
	case 36:
		result = CL_Q3KeyIsDown( args[1] );
		return true;
	case 200:
		result = CL_JK2StringRegister( uivm, args[1], 4 );
		return true;
	case 201:
		result = CL_JK2StringText( uivm, args[1], args[2], args[3], false );
		return true;
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
	case 32:
		// Retail 0x40ffb0 hands the handle and channel to S_StartLocalSound
		// 0x45d370 as client-game import 33 does, and returns zero. Outcast
		// numbers its channels as Quake III does (its client game plays chat
		// on 6 and its announcer on 7), and its S_StartSound gives every
		// one-shot one level (0x45d315), so both arrive unchanged at Fn's
		// entry for a menu's sound, which plays it as interface audio.
		if ( args[1] < INT_MIN || args[1] > INT_MAX || args[2] < INT_MIN || args[2] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid Jedi Outcast UI local sound handle or channel" );
		S_StartInterfaceSound( static_cast<sfxHandle_t>( args[1] ), static_cast<int>( args[2] ), 1.0f );
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
