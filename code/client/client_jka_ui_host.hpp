/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_UI_HOST_HPP
#define FNTECH3_CLIENT_JKA_UI_HOST_HPP
#include "client_jka_ui.hpp"
#include "client_jka_glconfig.hpp"
#include "client_jka_font.hpp"
#include "client_jka_strings.hpp"
#include "client_jka_sound.hpp"
#include "../compat/jka_render_adapter.hpp"
#include "../compat/jka_key_adapter.h"
#include <climits>
#include <limits>

static void *CL_JKAUINativeSpan( intptr_t address, std::size_t bytes ) {
	if ( !uivm || !uivm->entryPoint || uivm->dllExports )
		Com_Error( ERR_DROP, "Jedi Academy UI requires its legacy native module" );
	if ( !address || bytes > ( std::numeric_limits<std::uintptr_t>::max )() - std::uintptr_t( address ) )
		Com_Error( ERR_DROP, "Invalid Jedi Academy UI record address" );
	VM_CHECKBOUNDS( uivm, address, bytes );
	return VM_ExplicitArgPtr( uivm, address );
}

#include "client_jka_ui_ghoul2.hpp"
#include "client_jka_ui_render.hpp"

/* Included after the host helpers in cl_ui.cpp. All record transfers use
 * explicit JKA PODs; unsupported imports fail before the shared switch. */
static bool CL_JKAUICall( intptr_t *args, intptr_t &operation, intptr_t &result,
	std::array<intptr_t, 6> &browserArguments, intptr_t *&sharedArguments ) {
	using namespace fntech3::jka;
	using module::Call;
	const auto call = module::Lookup( COM_GAME_JKA_MP, module::Module::UI,
		module::Direction::IMPORT, args[0] );
	result = 0;
	if ( CL_JKAUIGhoul2Call( args, call, result ) ) return true;
	if ( CL_JKAFontCall( uivm, args, result, FNTECH3_JKA_UI_R_REGISTERFONT,
		[]( float x, float y, float w, float h, const fntech3::raven::font::Glyph &glyph, qhandle_t shader ) {
			CL_UIFromVirtual( &x, &y, &w, &h );
			re.DrawStretchPic( x, y, w, h, glyph.s, glyph.t, glyph.s2, glyph.t2, shader );
		} ) ) return true;
	switch ( call ) {
	case Call::UI_R_CLEARSCENE:
		CL_JKAUIRenderClearScene();
		operation = UI_R_CLEARSCENE;
		return false;
	case Call::UI_R_ADDREFENTITYTOSCENE: {
		fntech3_jka_mp_refEntity_t source;
		std::memcpy(&source, CL_JKAUINativeSpan(args[1], sizeof(source)), sizeof(source));
		if (source.ghoul2) {
			CL_JKAUIRenderQueue(source);
			return true;
		}
		refEntity_t value{};
		const auto status = render::Entity( source, value );
		if ( status != render::Status::Ok ) Com_Error( ERR_DROP, "Jedi Academy UI %s", render::StatusName( status ) );
		operation = UI_R_ADDREFENTITYTOSCENE;
		return false; // shared entity submission reads the same frozen JKA record
	}
	case Call::UI_R_RENDERSCENE: {
		refdef_t value{};
		const auto status = render::ReadView( CL_JKAUINativeSpan( args[1], sizeof( fntech3_jka_mp_refdef_t ) ), value );
		if ( status != render::Status::Ok ) Com_Error( ERR_DROP, "Jedi Academy UI %s", render::StatusName( status ) );
		CL_JKAUIRenderFlush(value.time);
		operation = UI_R_RENDERSCENE;
		return false; // shared menu viewport adjustment and renderer features apply
	}
	case Call::UI_R_ADDLIGHTTOSCENE: {
		// Retail0x4244ee passes an origin pointer and four float words.
		vec3_t origin;
		std::memcpy(origin, CL_JKAUINativeSpan(args[1], sizeof(origin)), sizeof(origin));
		float light[4];
		for (unsigned i = 0; i < 4; ++i) {
			const auto bits = static_cast<std::uint32_t>(args[i+2]);
			std::memcpy(&light[i], &bits, sizeof(bits));
			if (!render::Finite(light[i])) Com_Error(ERR_DROP, "Invalid Jedi Academy UI light");
		}
		if (!render::Finite(origin)) Com_Error(ERR_DROP, "Invalid Jedi Academy UI light origin");
		re.AddLightToScene(origin, light[0], light[1], light[2], light[3]);
		return true;
	}
	case Call::UI_KEY_GETBINDINGBUF: {
		if ( args[3] < 1 || args[3] > INT_MAX ) Com_Error( ERR_DROP, "Invalid Jedi Academy UI binding buffer size" );
		auto *output = static_cast<char *>( CL_JKAUINativeSpan( args[2], std::size_t( args[3] ) ) );
		const int key = FNTech3_JKAUIBindingKey( args[1] );
		const char *binding = key >= 0 ? Key_GetBinding( key ) : nullptr;
		Q_strncpyz( output, binding ? binding : "", static_cast<int>( args[3] ) );
		return true;
	}
	case Call::UI_PC_LOAD_SOURCE:
	case Call::UI_PC_LOAD_GLOBAL_DEFINES: {
		std::array<char, MAX_QPATH> filename{};
		for ( std::size_t n = 0; n < filename.size(); ++n ) {
			if ( std::uintptr_t( args[1] ) > ( std::numeric_limits<std::uintptr_t>::max )() - n )
				Com_Error( ERR_DROP, "Invalid Jedi Academy UI definition filename address" );
			filename[n] = *static_cast<const char *>( CL_JKAUINativeSpan( std::uintptr_t( args[1] ) + n, 1 ) );
			if ( !filename[n] ) {
				if ( !botlib_export ) Com_Error( ERR_DROP, "Jedi Academy UI parser is unavailable" );
				if ( call == Call::UI_PC_LOAD_SOURCE ) {
					if ( !botlib_export->PC_LoadJKASourceHandle ) Com_Error( ERR_DROP, "Jedi Academy UI source parser is unavailable" );
					result = botlib_export->PC_LoadJKASourceHandle( filename.data() );
				} else {
					if ( !botlib_export->PC_LoadGlobalDefines ) Com_Error( ERR_DROP, "Jedi Academy UI global-definition parser is unavailable" );
					result = botlib_export->PC_LoadGlobalDefines( filename.data() );
				}
				return true;
			}
		}
		Com_Error( ERR_DROP, "Jedi Academy UI definition filename exceeds MAX_QPATH" );
		return true;
	}
	case Call::UI_PC_REMOVE_ALL_GLOBAL_DEFINES:
		if ( !uivm || !uivm->entryPoint || uivm->dllExports ) Com_Error( ERR_DROP, "Jedi Academy UI requires its legacy native module" );
		if ( !botlib_export || !botlib_export->PC_RemoveAllGlobalDefines ) Com_Error( ERR_DROP, "Jedi Academy UI global-definition parser is unavailable" );
		botlib_export->PC_RemoveAllGlobalDefines();
		return true;
	case Call::UI_PC_SOURCE_FILE_AND_LINE: {
		auto *filename = static_cast<char *>( CL_JKAUINativeSpan( args[2], 128 ) );
		void *lineOutput = CL_JKAUINativeSpan( args[3], sizeof( std::int32_t ) );
		if ( args[1] < 0 || args[1] > INT_MAX || !botlib_export || !botlib_export->PC_SourceFileAndLine )
			Com_Error( ERR_DROP, "Invalid Jedi Academy UI parser diagnostic query" );
		std::array<char, 1024> internal{};
		int line = 0;
		result = botlib_export->PC_SourceFileAndLine( static_cast<int>( args[1] ), internal.data(), &line );
		if ( result ) {
			const auto *end = static_cast<const char *>( std::memchr( internal.data(), 0, internal.size() ) );
			if ( !end || end - internal.data() >= 128 || line < 0 ) Com_Error( ERR_DROP, "Invalid Jedi Academy UI parser diagnostic record" );
			Q_strncpyz( filename, internal.data(), 128 );
			const std::int32_t externalLine = line;
			std::memcpy( lineOutput, &externalLine, sizeof( externalLine ) );
		}
		return true;
	}
	case Call::UI_KEY_GETCATCHER:
		if ( !uivm || !uivm->entryPoint || uivm->dllExports ) Com_Error( ERR_DROP, "Jedi Academy UI requires its legacy native module" );
		result = Key_GetCatcher() & 15;
		return true;
	case Call::UI_KEY_SETCATCHER:
		if ( !uivm || !uivm->entryPoint || uivm->dllExports ) Com_Error( ERR_DROP, "Jedi Academy UI requires its legacy native module" );
		if ( args[1] < 0 || args[1] > 15 ) Com_Error( ERR_DROP, "Invalid Jedi Academy UI key catcher flags" );
		Key_SetCatcher( static_cast<int>( args[1] ) | ( Key_GetCatcher() & ( KEYCATCH_CONSOLE | KEYCATCH_BROWSER ) ) );
		return true;
	case Call::UI_LANGUAGE_ISASIAN:
		result = CL_JKAStringIsAsian();
		return true;
	case Call::UI_LANGUAGE_USESSPACES:
		result = CL_JKAStringUsesSpaces();
		return true;
	case Call::UI_ANYLANGUAGE_READCHARFROMSTRING:
		result = CL_JKAStringReadChar( uivm, args[1], args[2], args[3] );
		return true;
	case Call::UI_SP_GETNUMLANGUAGES:
		result = CL_JKAStringCount();
		return true;
	case Call::UI_SP_GETLANGUAGENAME:
		CL_JKAStringLanguage( uivm, static_cast<int>( args[1] ), args[2] );
		return true;
	case Call::UI_SP_GETSTRINGTEXTSTRING:
		result = CL_JKAStringText( uivm, args[1], args[2], static_cast<int>( args[3] ), false );
		return true;
	case Call::UI_GETGLCONFIG:
		CL_JKAGetGlconfig( uivm, args[1] );
		return true;
	case Call::UI_CVAR_REGISTER: {
		if (!uivm || !uivm->entryPoint || uivm->dllExports)
			Com_Error(ERR_DROP, "Jedi Academy UI requires its legacy native module");
		auto *output = args[1] ? static_cast<fntech3_jka_vmCvar_t *>(
			CL_JKAUINativeSpan(args[1], sizeof(fntech3_jka_vmCvar_t))) : nullptr;
		vmCvar_t value{};
		value.handle = -1;
		// Retail 0x43a350 registers the cvar before testing its nullable output;
		// UI_INIT uses that form for ui_name and debug_protocol.
		Cvar_Register( output ? &value : nullptr, VMA(2), VMA(3), ui::CvarFlags( static_cast<uint32_t>( args[4] ) ), uivm->privateFlag );
		if (output && value.handle >= 0) {
			fntech3_jka_vmCvar_t external{};
			ui::CvarToExternal(value, external);
			std::memcpy(output, &external, sizeof(external));
		}
		return true;
	}
	case Call::UI_CVAR_UPDATE: {
		auto *output = static_cast<fntech3_jka_vmCvar_t *>(
			CL_JKAUINativeSpan(args[1], sizeof(fntech3_jka_vmCvar_t)));
		vmCvar_t value{};
		fntech3_jka_vmCvar_t external;
		std::memcpy(&external, output, sizeof(external));
		ui::CvarToInternal( external, value );
		const auto previous = value;
		Cvar_Update( &value, uivm->privateFlag );
		if (!std::memcmp(&previous, &value, sizeof(value))) return true;
		ui::CvarToExternal( value, external );
		std::memcpy(output, &external, sizeof(external));
		return true;
	}
	case Call::UI_CVAR_CREATE:
		Cvar_Register( nullptr, VMA(1), VMA(2), ui::CvarFlags( static_cast<uint32_t>( args[3] ) ), uivm->privateFlag );
		return true;
	case Call::UI_CVAR_INFOSTRINGBUFFER:
		VM_CHECKBOUNDS( uivm, args[2], args[3] );
		Cvar_InfoStringBuffer( ui::CvarFlags( static_cast<uint32_t>( args[1] ) ), VMA(2), args[3] );
		return true;
	case Call::UI_GETCLIENTSTATE: {
		VM_CHECKBOUNDS( uivm, args[1], sizeof( fntech3_jka_mp_uiClientState_t ) );
		auto *output = static_cast<fntech3_jka_mp_uiClientState_t *>( static_cast<void *>( VMA(1) ) );
		if ( !output ) Com_Error( ERR_DROP, "Missing Jedi Academy UI client state" );
		uiClientState_t state{};
		state.connState = cls.state;
		state.connectPacketCount = clc.connectPacketCount;
		state.clientNum = cl.snap.ps.clientNum;
		Q_strncpyz( state.servername, cls.servername, sizeof( state.servername ) );
		Q_strncpyz( state.updateInfoString, cls.updateInfoString, sizeof( state.updateInfoString ) );
		Q_strncpyz( state.messageString, clc.serverMessage, sizeof( state.messageString ) );
		if ( !ui::ClientState( state, *output ) ) Com_Error( ERR_DROP, "Invalid Jedi Academy UI connection state" );
		return true;
	}
	case Call::UI_PC_READ_TOKEN: {
		VM_CHECKBOUNDS( uivm, args[2], sizeof( fntech3_jka_mp_pc_token_t ) );
		auto *output = static_cast<fntech3_jka_mp_pc_token_t *>( static_cast<void *>( VMA(2) ) );
		if ( !output ) Com_Error( ERR_DROP, "Missing Jedi Academy UI parser token" );
		pc_token_t token{};
		result = botlib_export->PC_ReadTokenHandle( args[1], &token );
		if ( result ) ui::Token( token, *output );
		return true;
	}
	case Call::UI_REAL_TIME: {
		VM_CHECKBOUNDS( uivm, args[1], sizeof( fntech3_jka_mp_qtime_t ) );
		auto *output = static_cast<fntech3_jka_mp_qtime_t *>( static_cast<void *>( VMA(1) ) );
		if ( !output ) Com_Error( ERR_DROP, "Missing Jedi Academy UI time output" );
		qtime_t time{};
		result = Com_RealTime( &time );
		ui::Time( time, *output );
		return true;
	}
	case Call::UI_S_REGISTERSOUND:
		result = S_RegisterSound( VMA(1), qfalse );
		return true;
	case Call::UI_S_STARTLOCALSOUND: {
		// Retail 0x424603 hands the handle and channel to S_StartLocalSound
		// 0x46aed0 exactly as client-game import 38 does, and returns zero.
		// Menu sounds take the same channel translation and level into Fn's
		// local-sound path, as interface audio: `play` names the automatic
		// channel, which would otherwise be the listener's own in the level.
		if ( args[1] < INT_MIN || args[1] > INT_MAX || args[2] < INT_MIN || args[2] > INT_MAX )
			Com_Error( ERR_DROP, "Invalid Academy UI local sound handle or channel" );
		const int channel = static_cast<int>( args[2] );
		S_StartInterfaceSound( static_cast<sfxHandle_t>( args[1] ), sound::EngineChannel( channel ),
			sound::ListenerVolume( channel ) );
		return true;
	}
	default:
		operation = ui::SharedImport( call );
		if ( operation >= 0 ) {
			if ( ui::BrowserArguments( call, args, browserArguments ) ) sharedArguments = browserArguments.data();
			return false;
		}
		const auto *description = module::Describe( call );
		Com_Error( ERR_DROP, "Jedi Academy multiplayer UI import %s (%ld) is not served yet: %s boundary is open",
			description ? description->name : "unknown", static_cast<long>( args[0] ), ui::OpenBoundary( call ) );
		return true;
	}
}
#endif
