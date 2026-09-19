/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_UI_HPP
#define FNTECH3_CLIENT_STVEF_UI_HPP
#include "../compat/stvef_module_abi.h"
#include "client_stvef_records.hpp"

/* Holomatch's UI (API 4) keeps Quake III's imports up to 45, puts its two
 * server lists at 46-49 and the ping queue at 50-53, then moves cvar
 * registration, memory, the CD key and model bounds to 54-58 (stvoyHM.exe's UI
 * dispatcher at 0x4125a0 handles 0-29, 31-58 and 100-108). */

static intptr_t CL_STVEFUISharedCall( fntech3::stvef::module::Call call ) {
	using fntech3::stvef::module::Call;
	switch ( call ) {
#define STVEF_SHARED_CALL(name) case Call::name: return name;
	STVEF_SHARED_CALL(UI_ERROR)
	STVEF_SHARED_CALL(UI_PRINT)
	STVEF_SHARED_CALL(UI_MILLISECONDS)
	STVEF_SHARED_CALL(UI_CVAR_SET)
	STVEF_SHARED_CALL(UI_CVAR_VARIABLEVALUE)
	STVEF_SHARED_CALL(UI_CVAR_VARIABLESTRINGBUFFER)
	STVEF_SHARED_CALL(UI_CVAR_SETVALUE)
	STVEF_SHARED_CALL(UI_CVAR_RESET)
	STVEF_SHARED_CALL(UI_CVAR_CREATE)
	STVEF_SHARED_CALL(UI_CVAR_INFOSTRINGBUFFER)
	STVEF_SHARED_CALL(UI_ARGC)
	STVEF_SHARED_CALL(UI_ARGV)
	STVEF_SHARED_CALL(UI_CMD_EXECUTETEXT)
	STVEF_SHARED_CALL(UI_FS_FOPENFILE)
	STVEF_SHARED_CALL(UI_FS_WRITE)
	STVEF_SHARED_CALL(UI_FS_FCLOSEFILE)
	STVEF_SHARED_CALL(UI_FS_GETFILELIST)
	STVEF_SHARED_CALL(UI_R_REGISTERMODEL)
	STVEF_SHARED_CALL(UI_R_REGISTERSKIN)
	STVEF_SHARED_CALL(UI_R_REGISTERSHADERNOMIP)
	STVEF_SHARED_CALL(UI_R_CLEARSCENE)
	// The shared handlers read the Elite Force entity and view records
	// through the render record adapter.
	STVEF_SHARED_CALL(UI_R_ADDREFENTITYTOSCENE)
	STVEF_SHARED_CALL(UI_R_ADDPOLYTOSCENE)
	STVEF_SHARED_CALL(UI_R_ADDLIGHTTOSCENE)
	STVEF_SHARED_CALL(UI_R_RENDERSCENE)
	STVEF_SHARED_CALL(UI_R_SETCOLOR)
	STVEF_SHARED_CALL(UI_R_DRAWSTRETCHPIC)
	STVEF_SHARED_CALL(UI_UPDATESCREEN)
	STVEF_SHARED_CALL(UI_CM_LERPTAG)
	STVEF_SHARED_CALL(UI_S_STARTLOCALSOUND)
	STVEF_SHARED_CALL(UI_KEY_GETOVERSTRIKEMODE)
	STVEF_SHARED_CALL(UI_KEY_SETOVERSTRIKEMODE)
	STVEF_SHARED_CALL(UI_KEY_CLEARSTATES)
	STVEF_SHARED_CALL(UI_KEY_GETCATCHER)
	STVEF_SHARED_CALL(UI_KEY_SETCATCHER)
	STVEF_SHARED_CALL(UI_GETCLIPBOARDDATA)
	STVEF_SHARED_CALL(UI_GETCLIENTSTATE)
	STVEF_SHARED_CALL(UI_GETCONFIGSTRING)
	STVEF_SHARED_CALL(UI_LAN_GETPINGQUEUECOUNT)
	STVEF_SHARED_CALL(UI_LAN_CLEARPING)
	STVEF_SHARED_CALL(UI_LAN_GETPING)
	STVEF_SHARED_CALL(UI_LAN_GETPINGINFO)
	STVEF_SHARED_CALL(UI_CVAR_REGISTER)
	STVEF_SHARED_CALL(UI_CVAR_UPDATE)
	STVEF_SHARED_CALL(UI_MEMORY_REMAINING)
	STVEF_SHARED_CALL(UI_R_MODELBOUNDS)
	STVEF_SHARED_CALL(UI_MEMSET)
	STVEF_SHARED_CALL(UI_MEMCPY)
	STVEF_SHARED_CALL(UI_STRNCPY)
	STVEF_SHARED_CALL(UI_SIN)
	STVEF_SHARED_CALL(UI_COS)
	STVEF_SHARED_CALL(UI_ATAN2)
	STVEF_SHARED_CALL(UI_SQRT)
	STVEF_SHARED_CALL(UI_FLOOR)
	STVEF_SHARED_CALL(UI_CEIL)
#undef STVEF_SHARED_CALL
	default: return -1;
	}
}

static bool CL_STVEFUICall( intptr_t *args, intptr_t &operation, intptr_t &result ) {
	using namespace fntech3::stvef::module;
	const Call call = Lookup( COM_GAME_STVEF_MP, Module::UI, Direction::IMPORT, args[0] );
	result = 0;
	switch ( call ) {
	case Call::UI_FS_READ:
		// Retail reads nothing from a file that failed to open, before it looks
		// at the length (FS_Read 0x41f072).
		if ( !args[3] ) return true;
		VM_CHECKBOUNDS( uivm, args[1], args[2] );
		FS_VM_ReadFile( VMA(1), args[2], args[3], H_Q3UI );
		return true;
	case Call::UI_S_REGISTERSOUND:
		// One argument; a sound that cannot be loaded is handle 0, silence.
		result = S_RegisterSound( VMA(1), qfalse );
		return true;
	case Call::UI_GETGLCONFIG:
		VM_CHECKBOUNDS( uivm, args[1], sizeof( fntech3_stvef_mp_glconfig_t ) );
		CL_CopySTVEFGlconfig( VMA(1), *re.GetConfig() );
		return true;
	// Each server list has its own count and address import.
	case Call::UI_LAN_GETLOCALSERVERCOUNT:
		result = LAN_GetServerCount( AS_LOCAL );
		return true;
	case Call::UI_LAN_GETGLOBALSERVERCOUNT:
		result = LAN_GetServerCount( AS_GLOBAL );
		return true;
	case Call::UI_LAN_GETLOCALSERVERADDRESSSTRING:
	case Call::UI_LAN_GETGLOBALSERVERADDRESSSTRING:
		if ( args[3] <= 0 ) Com_Error( ERR_DROP, "Invalid Elite Force server address buffer" );
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		LAN_GetServerAddressString( call == Call::UI_LAN_GETLOCALSERVERADDRESSSTRING ? AS_LOCAL : AS_GLOBAL,
			static_cast<int>( args[1] ), VMA(2), static_cast<int>( args[3] ) );
		return true;
	case Call::UI_SET_CDKEY:
		// Raven's CD-key check and authorize service are not part of this
		// engine; no key is accepted or stored.
		result = qfalse;
		return true;
	// Key numbers are Elite Force's own (stvef_key_adapter.h), and a key's
	// name is the translated one retail gives its modules.
	case Call::UI_KEY_KEYNUMTOSTRINGBUF:
	case Call::UI_KEY_GETBINDINGBUF:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		CL_STVEFKeyString( static_cast<int>( args[1] ), VMA(2), static_cast<int>( args[3] ),
			call == Call::UI_KEY_GETBINDINGBUF ? qtrue : qfalse );
		return true;
	case Call::UI_KEY_SETBINDING:
		CL_STVEFSetBinding( static_cast<int>( args[1] ), VMA(2) );
		return true;
	case Call::UI_KEY_ISDOWN:
		result = CL_STVEFKeyIsDown( static_cast<int>( args[1] ) );
		return true;
	default:
		operation = CL_STVEFUISharedCall( call );
		if ( operation < 0 ) {
			// Retail's default case: UI_CM_LOADMODEL (30) and unknown numbers.
			Com_Error( ERR_DROP, "Bad UI system trap: %ld", static_cast<long>( args[0] ) );
			return true;
		}
		return false;
	}
}
#endif
