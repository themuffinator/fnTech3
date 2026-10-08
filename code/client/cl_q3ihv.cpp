/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* The February 1999 IHV Test's client session: protocol-33 messages through
 * fntech3::q3ihv::client::ClientSession, each event pushed to the client game
 * as the original engine pushes it, its length-only .dm3 demos, the frame the
 * client game is asked for, and the engine's own screens in place of the
 * build's built-in menus. Facts and addresses: docs/fntech3/Q3_IHV.md. */
extern "C" {
#include "client.h"
}
#include "client_q3ihv.h"
#include "client_ihv_session.hpp"
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace ihvcl = fntech3::q3ihv::client;
namespace ihvwire = fntech3::q3ihv::wire;

namespace {

constexpr int Q3IHV_DEMO_MESSAGE_BYTES = static_cast<int>( ihvwire::MaxMessageBytes );

/* The session lives on the heap and is rebuilt for every connection: an
 * error raised inside a client-game callback leaves by longjmp, which does not
 * unwind the session's own frames, so a session that saw one is never reused. */
std::unique_ptr<ihvcl::ClientSession> ihvSession;

void CL_Q3IHVResetSession() {
	ihvSession = std::make_unique<ihvcl::ClientSession>();
}

/* The engine's own gameState mirrors the build's configstrings, rebuilt the
 * way the original rebuilds its record (code/compat/q3_ihv_configstrings.hpp),
 * so the map name, server info and console commands read what it holds. */
void CL_Q3IHVMirrorGameState( const ihvcl::ClientData &data ) {
	auto previous = std::make_unique<gameState_t>( cl.gameState );
	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
	cl.gameState.dataCount = 1;
	for ( std::uint32_t i = 0; i < FNTECH3_Q3_IHV_MAX_CONFIGSTRINGS && i < MAX_CONFIGSTRINGS; ++i ) {
		const std::string_view text = data.Configstrings().String( i );
		if ( text.empty() ) continue;
		const int length = static_cast<int>( text.size() );
		if ( length + 1 + cl.gameState.dataCount > static_cast<int>( sizeof( cl.gameState.stringData ) ) ) {
			cl.gameState = *previous;
			Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
		}
		cl.gameState.stringOffsets[i] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, text.data(), length );
		cl.gameState.stringData[cl.gameState.dataCount + length] = '\0';
		cl.gameState.dataCount += length + 1;
	}
	const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SYSTEMINFO];
	cl.serverId = atoi( Info_ValueForKey( info, "serverid" ) );
}

/* CL_ParseGamestate (0x406c40): the client state is cleared, the client game
 * loaded and initialised with the new record. */
void CL_Q3IHVStartCGame() {
	Con_Close();
	cls.state = CA_LOADING;
	Cvar_Set( "r_uiFullScreen", "0" );
	CL_FlushMemory();
	cls.cgameStarted = qtrue;
	CL_InitCGame();
}

/* CL_ParseServerCommand (0x406f60): the engine takes disconnect and
 * clientLevelShot, the client game the rest, tokenized. */
void CL_Q3IHVServerCommand( std::string_view command ) {
	const std::string text( command );
	if ( com_developer && com_developer->integer ) Com_Printf( "serverCommand: %s\n", text.c_str() );
	Cmd_TokenizeString( text.c_str() );
	if ( !strcmp( Cmd_Argv( 0 ), "disconnect" ) ) {
		Com_Error( ERR_SERVERDISCONNECT, "Server disconnected\n" );
	}
	if ( !strcmp( Cmd_Argv( 0 ), "clientLevelShot" ) ) {
		if ( !com_sv_running || !com_sv_running->integer ) return;
		Con_Close();
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
	}
	Cmd_TokenizeString( text.c_str() );
	CL_Q3IHVCGameServerCommand();
}

/* A valid frame makes the connection active (0x406c11) and goes to the
 * client game at once. The engine's own snapshot record keeps the times its
 * screen code reads. */
void CL_Q3IHVSnapshot( const fntech3_q3_ihv_snapshot_t &record ) {
	const auto word = [&]( unsigned offset ) { return static_cast<int>( ihvwire::Load( record.bytes + offset, 4 ) ); };
	clSnapshot_t mirror{};
	mirror.valid = qtrue;
	mirror.serverTime = word( 8 );
	mirror.cmdNum = word( 12 );
	mirror.messageNum = word( 16 );
	mirror.deltaNum = word( 20 );
	mirror.ping = word( 24 );
	cl.snap = mirror;
	cl.snapshots[mirror.messageNum & PACKET_MASK] = mirror;
	cl.serverTime = mirror.serverTime;
	cl.newSnapshots = qtrue;
	clc.lastPacketTime = cls.realtime;
	if ( cls.state == CA_PRIMED ) {
		cls.state = CA_ACTIVE;
		Com_Printf( "CL_FirstSnapshot: %s\n", cl.mapname );
		if ( cl_activeAction->string[0] ) {
			Cbuf_AddText( cl_activeAction->string );
			Cbuf_AddText( "\n" );
			Cvar_Set( "activeAction", "" );
		}
	}
	if ( cls.state == CA_ACTIVE ) CL_Q3IHVCGameSnapshot( &record );
}

class CL_Q3IHVEvents final : public ihvcl::ClientEvents {
public:
	void GameState( const ihvcl::ClientData &data ) override {
		CL_ClearState();
		CL_Q3IHVMirrorGameState( data );
		clc.clientNum = 0;
		clc.eventMask |= EM_GAMESTATE;
		Cvar_Set( "cl_paused", "0" );
		CL_Q3IHVStartCGame();
	}
	void Configstring( const ihvcl::ClientData &data, std::uint32_t index ) override {
		CL_Q3IHVMirrorGameState( data );
		CL_Q3IHVCGameConfigstring( static_cast<int>( index ) );
	}
	void ServerCommand( const ihvcl::ClientData &, std::string_view text ) override {
		CL_Q3IHVServerCommand( text );
	}
	void Snapshot( const ihvcl::ClientData &, const fntech3_q3_ihv_snapshot_t &snapshot ) override {
		CL_Q3IHVSnapshot( snapshot );
	}
};
CL_Q3IHVEvents ihvEvents;

const char *CL_Q3IHVPacketError( const ihvcl::PacketResult &result ) {
	switch ( result.code ) {
	case ihvcl::PacketResult::Rejected:
		switch ( result.message ) {
		case ihvwire::MessageResult::MissingGamestate: return "a frame before its gamestate";
		case ihvwire::MessageResult::MissingDelta: return "a frame whose delta is gone";
		case ihvwire::MessageResult::UnsupportedDownload: return "a download block";
		default: return "Illegible server message";
		}
	case ihvcl::PacketResult::StateMismatch: return "a record the client state refuses";
	case ihvcl::PacketResult::Reentrant: return "a message inside another";
	default: return "the session was cleared";
	}
}

/* One server message through the session; the module sees its events in
 * wire order, after the whole message has been checked. */
void CL_Q3IHVParse( const std::uint8_t *data, std::size_t size ) {
	if ( !ihvSession ) CL_Q3IHVResetSession();
	const auto result = ihvSession->Feed( data, size, 0, ihvEvents );
	if ( result.code == ihvcl::PacketResult::Delivered || result.code == ihvcl::PacketResult::Aborted ) return;
	Com_Error( ERR_DROP, "CL_ParseServerMessage: %s", CL_Q3IHVPacketError( result ) );
}

/* ---- the engine's screens ------------------------------------------------ */

const vec4_t ihvWhite = { 1, 1, 1, 1 };
const vec4_t ihvYellow = { 1, 1, 0, 1 };

void CL_Q3IHVCentred( int y, const char *text, const float *color ) {
	const int width = static_cast<int>( strlen( text ) ) * BIGCHAR_WIDTH;
	SCR_DrawStringExt( 320 - width / 2, y, BIGCHAR_WIDTH, text, color, qtrue, qfalse );
}

/* The build's own menus are not reproduced: its title screen is drawn on its
 * console background, and the console stands in for the menus. */
void CL_Q3IHVDrawTitle() {
	SCR_DrawPic( 0, 0, 640, 480, re.RegisterShaderNoMip( "gfx/2d/conback" ) );
	CL_Q3IHVCentred( 176, "Quake III Arena IHV Test", ihvWhite );
	CL_Q3IHVCentred( 224, "Open the console to play a demo", ihvYellow );
	CL_Q3IHVCentred( 248, "or connect to a server", ihvYellow );
}

void CL_Q3IHVDrawConnect( bool overlay ) {
	if ( !overlay ) SCR_DrawPic( 0, 0, 640, 480, re.RegisterShaderNoMip( "gfx/2d/conback" ) );
	CL_Q3IHVCentred( 16, va( "Connecting to %s", cls.servername ), ihvWhite );
	const char *line = "";
	switch ( cls.state ) {
	case CA_CONNECTING: line = "Awaiting challenge..."; break;
	case CA_CHALLENGING: line = "Awaiting connection..."; break;
	case CA_CONNECTED: line = "Awaiting gamestate..."; break;
	case CA_LOADING: line = "Loading..."; break;
	case CA_PRIMED: line = "Awaiting snapshot..."; break;
	default: break;
	}
	CL_Q3IHVCentred( 440, line, ihvWhite );
}

intptr_t QDECL CL_Q3IHVUIEntry( int command, ... ) {
	va_list args;
	va_start( args, command );
	const intptr_t a0 = va_arg( args, intptr_t );
	va_end( args );
	switch ( command ) {
	case UI_REFRESH:
		if ( ( Key_GetCatcher() & KEYCATCH_UI ) && cls.state <= CA_DISCONNECTED ) CL_Q3IHVDrawTitle();
		return 0;
	case UI_IS_FULLSCREEN:
		return ( Key_GetCatcher() & KEYCATCH_UI ) && cls.state <= CA_DISCONNECTED ? 1 : 0;
	case UI_SET_ACTIVE_MENU:
		if ( a0 == UIMENU_MAIN ) Key_SetCatcher( ( Key_GetCatcher() & ~7 ) | KEYCATCH_UI );
		else Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		return 0;
	case UI_KEY_EVENT:
		return 0;
	case UI_DRAW_CONNECT_SCREEN:
		CL_Q3IHVDrawConnect( a0 != 0 );
		return 0;
	default:
		return 0;
	}
}

} // namespace

namespace fntech3::q3ihv::client {
const ClientData *SessionData() { return ihvSession ? &ihvSession->Data() : nullptr; }
}

vm_t *CL_Q3IHVLoadUI( void ) {
	return VM_CreateHosted( VM_UI, CL_Q3IHVUIEntry );
}

/* Disconnecting ends the session and whatever it held. */
void CL_Q3IHVClearConnection( void ) {
	ihvSession.reset();
}

/* One record of the demo being played (CL_ReadDemoMessage 0x4047e0): a
 * length, then the message; -1, a short read or an empty record ends it. */
bool CL_Q3IHVReadDemoMessage( void ) {
	if ( !clc.demofile ) return false;
	int length = 0;
	if ( FS_Read( &length, 4, clc.demofile ) != 4 ) return false;
	length = LittleLong( length );
	if ( length == -1 ) return false;
	if ( length < 0 || length > Q3IHV_DEMO_MESSAGE_BYTES ) {
		Com_Error( ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );
		return false;
	}
	if ( !length ) return false;
	std::vector<std::uint8_t> data( static_cast<std::size_t>( length ) );
	if ( FS_Read( data.data(), length, clc.demofile ) != length ) {
		Com_Printf( "Demo file was truncated.\n" );
		return false;
	}
	clc.lastPacketTime = cls.realtime;
	CL_Q3IHVParse( data.data(), data.size() );
	return true;
}

/* demo <name> (0x404960): demos/<name>.dm3, read until the client game is
 * primed; the original reads its first two records there. */
void CL_Q3IHVPlayDemo_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "demo <demoname>\n" );
		return;
	}
	const char *arg = Cmd_Argv( 1 );
	char name[MAX_OSPATH];
	if ( !Q_stricmp( COM_GetExtension( arg ), "dm3" ) ) Com_sprintf( name, sizeof( name ), "demos/%s", arg );
	else Com_sprintf( name, sizeof( name ), "demos/%s.dm3", arg );
	fileHandle_t probe = 0;
	if ( FS_FOpenFileRead( name, &probe, qtrue ) < 0 || !probe ) {
		Com_Printf( S_COLOR_YELLOW "couldn't open %s\n", name );
		return;
	}
	FS_FCloseFile( probe );
	Cvar_Set( "sv_killserver", "2" );
	CL_Disconnect( qtrue );
	if ( FS_FOpenFileRead( name, &clc.demofile, qtrue ) == -1 || !clc.demofile ) {
		Com_Error( ERR_DROP, "couldn't open %s\n", name );
		return;
	}
	const char *slash = strrchr( name, '/' );
	const char *shortname = slash ? slash + 1 : name;
	Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );
	Con_Close();
	cls.state = CA_CONNECTED;
	clc.demoplaying = qtrue;
	Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );
	CL_Q3IHVResetSession();
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED ) {
		CL_ReadDemoMessage();
	}
}

/* CL_CgameFrame (0x4059b0): while active, the client game is asked each frame
 * whether it can draw. A demo is read a record at a time until it can
 * (0x405a40); a timed demo counts the frames that drew. */
void CL_Q3IHVSetCGameTime( void ) {
	// A demo's first frame follows its gamestate (the original reads both
	// before its first client frame, 0x4049e3/0x4049e8).
	while ( clc.demoplaying && cls.state == CA_PRIMED ) {
		CL_ReadDemoMessage();
	}
	if ( cls.state != CA_ACTIVE ) return;
	if ( !clc.demoplaying ) {
		CL_Q3IHVCGameFrame( cls.realtime, false );
		return;
	}
	for ( ;; ) {
		if ( CL_Q3IHVCGameFrame( cls.realtime, true ) ) break;
		CL_ReadDemoMessage();
		if ( cls.state != CA_ACTIVE ) return;
	}
	if ( com_timedemo->integer ) {
		if ( !clc.timeDemoStart ) clc.timeDemoStart = Sys_Milliseconds();
		clc.timeDemoFrames++;
	}
}
