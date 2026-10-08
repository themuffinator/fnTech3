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
#include "../compat/q3_ihv_netchan.hpp"
#include <algorithm>
#include <array>
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

/* The live connection (cl_main.cpp 0x405420, 0x403360): the build's own
 * channel, a ring of the packets sent with the command number each carried,
 * and the configstring sequence of the newest frame, which every move
 * acknowledges. */
struct IhvOutPacket {
	int realtime = 0;
	int cmdNumber = 0;
};
struct IhvLive {
	fntech3::q3ihv::Netchan chan;
	std::array<IhvOutPacket, 32> out{};
	int lastSent = -9999;
	int configAck = 0;
	int engineCmd = 0;   // the engine command last recorded for the client game
	fileHandle_t rawLog = 0;   // cl_ihvRawLog
};
IhvLive ihvLive;

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
	ihvLive.configAck = word( 336 );
	// A full frame starts a demo being recorded (0x406af4).
	if ( mirror.deltaNum <= 0 ) clc.demowaiting = qfalse;
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

/* A frame's ping (CL_ParseFrame 0x406ab2): the time since the newest of the
 * last 32 packets that carried the command the server names as its last,
 * 999 when none did; a demo's is 0. */
std::uint32_t CL_Q3IHVPing( const ihvwire::Snapshot &snapshot ) {
	if ( clc.demoplaying ) return 0;
	const int outgoing = ihvLive.chan.OutgoingSequence();
	for ( int i = 0; i < 32; ++i ) {
		const IhvOutPacket &packet = ihvLive.out[( outgoing - 1 - i ) & 31];
		if ( packet.cmdNumber == static_cast<int>( snapshot.commandSequence ) )
			return static_cast<std::uint32_t>( cls.realtime - packet.realtime );
	}
	return 999;
}

/* One server message through the session; the module sees its events in
 * wire order, after the whole message has been checked. */
void CL_Q3IHVParse( const std::uint8_t *data, std::size_t size ) {
	if ( !ihvSession ) CL_Q3IHVResetSession();
	const auto result = ihvSession->FeedWith( data, size, CL_Q3IHVPing, ihvEvents );
	if ( result.code == ihvcl::PacketResult::Delivered || result.code == ihvcl::PacketResult::Aborted ) return;
	Com_Error( ERR_DROP, "CL_ParseServerMessage: %s", CL_Q3IHVPacketError( result ) );
}

/* ---- demo recording ------------------------------------------------------- */

/* A record of a demo being recorded: the message's length, then its bytes
 * (CL_WriteDemoMessage 0x4044a0). */
void CL_Q3IHVDemoWrite( const void *data, std::size_t size ) {
	if ( !clc.demorecording || !clc.recordfile || !size ) return;
	const int length = LittleLong( static_cast<int>( size ) );
	FS_Write( &length, 4, clc.recordfile );
	FS_Write( data, static_cast<int>( size ), clc.recordfile );
}

/* The gamestate a recorded demo starts with (CL_Record_f 0x404650..0x404707):
 * the client's own configstrings and only the baselines that carry a model,
 * a narrower test than the server's (0x4187d1). */
bool CL_Q3IHVDemoGamestate( const ihvwire::GameState &game, ihvwire::MessageWriter &out ) {
	auto recorded = std::make_unique<ihvwire::GameState>( game );
	for ( auto &baseline : recorded->baselines ) {
		if ( !baseline[0x95] ) baseline = {};
	}
	return out.AddGameState( *recorded );
}

/* ---- the live connection ------------------------------------------------- */

bool CL_Q3IHVSendDatagram( void *, const unsigned char *data, std::size_t size ) {
	NET_SendPacket( NS_CLIENT, static_cast<int>( size ), data, &clc.netchan.remoteAddress );
	return true;
}

void CL_Q3IHVMirrorChannel() {
	clc.netchan.incomingSequence = ihvLive.chan.IncomingSequence();
	clc.netchan.outgoingSequence = ihvLive.chan.OutgoingSequence();
	clc.netchan.dropped = ihvLive.chan.Dropped();
	clc.serverMessageSequence = ihvLive.chan.IncomingSequence();
}

void CL_Q3IHVTransmit( const void *data, std::size_t length ) {
	if ( !ihvLive.chan.Ready() ) return;
	fntech3::q3ihv::NetchanSink sink{ nullptr, CL_Q3IHVSendDatagram };
	ihvLive.chan.Transmit( sink, static_cast<const unsigned char *>( data ), length );
	clc.netchan.lastSentTime = Sys_Milliseconds();
	clc.netchan.lastSentSize = static_cast<int>( length );
	CL_Q3IHVMirrorChannel();
	if ( ihvLive.chan.FatalError() ) Com_Error( ERR_DROP, "Netchan_Transmit: overflow" );
}

/* A client command is clc_stringcmd (3) and its text, in the reliable block. */
bool CL_Q3IHVQueueCommand( const char *text ) {
	std::array<std::uint8_t, 1100> buffer{};
	ihvwire::Writer out( buffer.data(), buffer.size() );
	if ( !out.Write( 3, 8 ) || !ihvwire::WriteString( out, text ) ) return false;
	return ihvLive.chan.QueueReliable( buffer.data(), out.Bytes() );
}

/* The engine's command as the build's 14 bytes (CL_FinishMove 0x402e40):
 * milliseconds, buttons with 0x80 for any key, the client game's weapon,
 * short angles and signed moves. */
fntech3_q3_ihv_usercmd_t CL_Q3IHVNativeCmd( int number, int weapon ) {
	const usercmd_t &cmd = cl.cmds[number & CMD_MASK];
	const usercmd_t &previous = cl.cmds[( number - 1 ) & CMD_MASK];
	fntech3_q3_ihv_usercmd_t out{};
	int msec = cmd.serverTime - previous.serverTime;
	if ( msec < 0 ) msec = 0;
	if ( msec > 250 ) msec = 100;
	out.bytes[0] = static_cast<std::uint8_t>( msec );
	out.bytes[1] = static_cast<std::uint8_t>( ( cmd.buttons & 0x7f ) | ( cmd.buttons & BUTTON_ANY ? 0x80 : 0 ) );
	out.bytes[2] = static_cast<std::uint8_t>( weapon );
	for ( int i = 0; i < 3; ++i ) ihvwire::Store( out.bytes + 4 + 2 * i, 2, static_cast<std::uint32_t>( cmd.angles[i] & 0xffff ) );
	out.bytes[10] = static_cast<std::uint8_t>( cmd.forwardmove );
	out.bytes[11] = static_cast<std::uint8_t>( cmd.rightmove );
	out.bytes[12] = static_cast<std::uint8_t>( cmd.upmove );
	return out;
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
	if ( ihvLive.rawLog ) FS_FCloseFile( ihvLive.rawLog );
	ihvLive = IhvLive{};
}

netchanWireProfile_t CL_Q3IHVWireProfile( void ) {
	return NETCHAN_WIRE_Q3_IHV;
}

/* CL_CheckForResend (0x405280): getchallenge while connecting and connect
 * while challenging, every three seconds; a local server needs no challenge. */
void CL_Q3IHVCheckForResend( void ) {
	if ( clc.demoplaying || ( cls.state != CA_CONNECTING && cls.state != CA_CHALLENGING ) ) return;
	if ( cls.state == CA_CONNECTING && NET_IsLocalAddress( &clc.serverAddress ) ) {
		cls.state = CA_CHALLENGING;
		clc.challenge = 0;
		clc.connectTime = -RETRANSMIT_TIMEOUT;
	}
	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) return;
	clc.connectTime = cls.realtime;
	clc.connectPacketCount++;
	if ( cls.state == CA_CONNECTING ) {
		NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "getchallenge" );
		return;
	}
	const int qport = Cvar_VariableIntegerValue( "net_qport" ) & 0xffff;
	NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "connect %i %i %i \"%s\"", 33, qport, clc.challenge,
		Cvar_InfoString( CVAR_USERINFO, nullptr ) );
	cvar_modifiedFlags &= ~CVAR_USERINFO;
}

/* CL_ConnectionlessPacket (0x405420). The build itself prints nothing for a
 * `print`, which its own server sends with a refusal; this engine shows it. */
static void CL_Q3IHVConnectionless( const netadr_t *from, msg_t *msg ) {
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );
	const char *line = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( line );
	const char *verb = Cmd_Argv( 0 );
	Com_DPrintf( "CL packet %s: %s\n", NET_AdrToString( from ), verb );
	if ( !strcmp( verb, "challenge" ) ) {
		if ( cls.state != CA_CONNECTING && cls.state != CA_CHALLENGING ) return;
		clc.challenge = atoi( Cmd_Argv( 1 ) );
		cls.state = CA_CHALLENGING;
		clc.connectPacketCount = 0;
		clc.connectTime = -99999;
		return;
	}
	if ( !strcmp( verb, "client_connect" ) ) {
		if ( cls.state >= CA_CONNECTED ) {
			Com_Printf( "Dup connect received.  Ignored.\n" );
			return;
		}
		if ( cls.state != CA_CHALLENGING ) {
			Com_Printf( "client_connect packet while not connecting.  Ignored.\n" );
			return;
		}
		if ( !NET_CompareBaseAdr( from, &clc.serverAddress ) ) {
			Com_Printf( "client_connect from a different address.  Ignored.\n" );
			Com_Printf( "%s should have been %s\n", NET_AdrToString( from ), NET_AdrToString( &clc.serverAddress ) );
			return;
		}
		const int qport = Cvar_VariableIntegerValue( "net_qport" ) & 0xffff;
		Netchan_Setup( NS_CLIENT, &clc.netchan, from, qport, clc.challenge, NETCHAN_WIRE_Q3_IHV );
		CL_Q3IHVResetSession();
		if ( ihvLive.rawLog ) FS_FCloseFile( ihvLive.rawLog );
		ihvLive = IhvLive{};
		ihvLive.chan.Setup( fntech3::q3ihv::NetchanSource::Client, qport );
		cls.state = CA_CONNECTED;
		ihvLive.lastSent = -9999;
		return;
	}
	if ( !strcmp( verb, "print" ) ) {
		const char *text = MSG_ReadString( msg );
		Q_strncpyz( clc.serverMessage, text, sizeof( clc.serverMessage ) );
		Com_Printf( "%s", text );
		return;
	}
	if ( !strcmp( verb, "disconnect" ) ) {
		if ( cls.state >= CA_CONNECTED && NET_CompareAdr( from, &clc.netchan.remoteAddress ) )
			Com_Error( ERR_SERVERDISCONNECT, "Server disconnected for unknown reason\n" );
		return;
	}
	if ( !strcmp( verb, "echo" ) ) {
		NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv( 1 ) );
		return;
	}
	if ( !strcmp( verb, "info" ) ) return;
	Com_DPrintf( "Unknown connectionless packet command.\n" );
}

/* CL_PacketEvent: sequenced packets come from the channel's address only. */
void CL_Q3IHVPacketEvent( const netadr_t *from, msg_t *msg ) {
	clc.lastPacketTime = cls.realtime;
	if ( msg->cursize >= 4 ) {
		std::int32_t marker;
		std::memcpy( &marker, msg->data, sizeof( marker ) );
		if ( marker == -1 ) {
			CL_Q3IHVConnectionless( from, msg );
			return;
		}
	}
	if ( cls.state < CA_CONNECTED || !ihvLive.chan.Ready() ) return;
	if ( msg->cursize < 8 ) {
		Com_Printf( "%s: Runt packet\n", NET_AdrToString( from ) );
		return;
	}
	if ( !NET_CompareAdr( from, &clc.netchan.remoteAddress ) ) {
		Com_DPrintf( "%s:sequenced packet without connection\n", NET_AdrToString( from ) );
		return;
	}
	std::size_t size = static_cast<std::size_t>( msg->cursize );
	const auto result = ihvLive.chan.Process( msg->data, size, static_cast<std::size_t>( msg->maxsize ) );
	CL_Q3IHVMirrorChannel();
	if ( result != fntech3::q3ihv::NetchanResult::Message ) return;
	// An empty message carries nothing: the original's parser reads its end
	// at once (0x4070cd).
	if ( !size ) return;
	// A developer log of every message of the connection, in .dm3 record
	// form, for replaying a session through the decoder offline.
	static cvar_t *rawLog;
	if ( !rawLog ) rawLog = Cvar_Get( "cl_ihvRawLog", "", CVAR_TEMP );
	if ( rawLog->string[0] && !ihvLive.rawLog ) ihvLive.rawLog = FS_FOpenFileWrite( rawLog->string );
	if ( ihvLive.rawLog ) {
		const int length = LittleLong( static_cast<int>( size ) );
		FS_Write( &length, 4, ihvLive.rawLog );
		FS_Write( msg->data, static_cast<int>( size ), ihvLive.rawLog );
		FS_Flush( ihvLive.rawLog );
	}
	CL_Q3IHVParse( msg->data, size );
	// The message after the channel's header goes to a demo being recorded
	// once a full frame has arrived (0x405812).
	if ( clc.demorecording && !clc.demowaiting ) CL_Q3IHVDemoWrite( msg->data, size );
}

/* CL_SendCmd (0x403360), after the engine has made the frame's command.
 * Client commands enter the reliable block. A connected client sends an empty
 * packet each second or when reliable data waits; a loading, primed or active
 * one records the frame's command and a provisional copy for the client game,
 * then, held to cl_maxpackets except over loopback, sends a move once two
 * commands are new: server id, time, delta frame, configstring sequence, last
 * finished command and up to 32 commands, each a delta from the one before. */
void CL_Q3IHVSendCmd( void ) {
	if ( cls.state < CA_CONNECTED || clc.demoplaying || cls.state == CA_CINEMATIC || !ihvLive.chan.Ready() ) return;
	while ( clc.reliableAcknowledge < clc.reliableSequence ) {
		++clc.reliableAcknowledge;
		if ( !CL_Q3IHVQueueCommand( clc.reliableCommands[clc.reliableAcknowledge & ( CL_ReliableCommandLimit() - 1 )] ) )
			Com_Error( ERR_DROP, "Client command overflow" );
	}
	if ( cls.state == CA_CONNECTED ) {
		const bool waiting = ihvLive.chan.PendingLength() || ihvLive.chan.ReliableLength();
		if ( !waiting && cls.realtime - ihvLive.lastSent <= 1000 ) return;
		ihvLive.lastSent = cls.realtime;
		CL_Q3IHVTransmit( nullptr, 0 );
		return;
	}
	ihvcl::ClientData *data = ihvSession ? const_cast<ihvcl::ClientData *>( &ihvSession->Data() ) : nullptr;
	if ( !data || !data->Ready() ) return;
	if ( cl.cmdNumber != ihvLive.engineCmd ) {
		ihvLive.engineCmd = cl.cmdNumber;
		const fntech3_q3_ihv_usercmd_t finished = CL_Q3IHVNativeCmd( cl.cmdNumber, CL_Q3IHVCGameWeapon() );
		fntech3_q3_ihv_usercmd_t provisional = finished;
		provisional.bytes[0] = 0;
		data->ReplaceNewestCommand( finished );
		data->AppendCommand( provisional );
	}
	const int maxpackets = std::clamp( Cvar_VariableIntegerValue( "cl_maxpackets" ), 15, 100 );
	const int outgoing = ihvLive.chan.OutgoingSequence();
	if ( clc.netchan.remoteAddress.type != NA_LOOPBACK
		&& cls.realtime - ihvLive.out[( outgoing - 1 ) & 31].realtime < 1000 / maxpackets ) return;
	const int packetdup = std::clamp( Cvar_VariableIntegerValue( "cl_packetdup" ), 0, 5 );
	const int cmdNumber = static_cast<int>( data->CurrentCommandNumber() );
	int count = cmdNumber - ihvLive.out[( outgoing - 1 - packetdup ) & 31].cmdNumber;
	if ( count > 32 ) {
		Com_Printf( "MAX_PACKET_USERCMDS\n" );
		count = 32;
	}
	if ( count < 2 ) return;
	std::array<std::uint8_t, 1024> buffer{};
	ihvwire::Writer out( buffer.data(), buffer.size() );
	const bool delta = !Cvar_VariableIntegerValue( "cl_nodelta" ) && cl.snap.valid && !clc.demowaiting;
	out.Write( 2, 8 );
	out.Write( static_cast<std::uint32_t>( cl.serverId ), 32 );
	out.Write( static_cast<std::uint32_t>( cls.realtime ), 32 );
	out.Write( static_cast<std::uint32_t>( delta ? cl.snap.messageNum : -1 ), 32 );
	out.Write( static_cast<std::uint32_t>( ihvLive.configAck ), 32 );
	out.Write( static_cast<std::uint32_t>( cmdNumber - 1 ), 32 );
	out.Write( static_cast<std::uint32_t>( count ), 8 );
	ihvwire::Command from{};
	for ( int number = cmdNumber - count + 1; number <= cmdNumber; ++number ) {
		fntech3_q3_ihv_usercmd_t record{};
		data->CopyCommand( static_cast<std::uint32_t>( number ), &record, sizeof( record ) );
		ihvwire::Command to{};
		std::memcpy( to.data(), record.bytes, to.size() );
		ihvwire::WriteCommand( out, from, to );
		from = to;
	}
	if ( !out.Good() ) Com_Error( ERR_DROP, "CL_SendCmd: move overflow" );
	IhvOutPacket &packet = ihvLive.out[outgoing & 31];
	packet.realtime = cls.realtime;
	packet.cmdNumber = cmdNumber - 1;
	ihvLive.lastSent = cls.realtime;
	CL_Q3IHVTransmit( buffer.data(), out.Bytes() );
}

/* CL_Disconnect: the command three times, unreliably, as Quake II sends it. */
void CL_Q3IHVDisconnect( void ) {
	if ( cls.state < CA_CONNECTED || clc.demoplaying || !ihvLive.chan.Ready() ) return;
	std::array<std::uint8_t, 64> buffer{};
	ihvwire::Writer out( buffer.data(), buffer.size() );
	out.Write( 3, 8 );
	ihvwire::WriteString( out, "disconnect" );
	for ( int i = 0; i < 3; ++i ) CL_Q3IHVTransmit( buffer.data(), out.Bytes() );
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

/* record <name> (CL_Record_f 0x404560): demos/<name>.dm3, its first record a
 * gamestate built from the client's own state, the rest the server's messages
 * from the next full frame, which the next move asks for. The original's
 * console command needs a name; without one this takes the first free name its
 * menu would (demos/demo%03d.dm3, 0x463a3c). */
void CL_Q3IHVRecord_f( void ) {
	if ( Cmd_Argc() > 2 ) {
		Com_Printf( "record <demoname>\n" );
		return;
	}
	if ( clc.demorecording ) {
		Com_Printf( "Already recording.\n" );
		return;
	}
	const auto game = ihvSession ? ihvSession->Decoder().CurrentGameState() : nullptr;
	if ( cls.state != CA_ACTIVE || !game ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}
	char name[MAX_OSPATH];
	if ( Cmd_Argc() == 2 ) {
		Com_sprintf( name, sizeof( name ), "demos/%s.dm3", Cmd_Argv( 1 ) );
	} else {
		int number = 0;
		for ( ; number <= 999; ++number ) {
			Com_sprintf( name, sizeof( name ), "demos/demo%03d.dm3", number );
			if ( !FS_FileExists( name ) ) break;
		}
		if ( number > 999 ) {
			Com_Printf( "ERROR: no free demo name\n" );
			return;
		}
	}
	auto writer = std::make_unique<ihvwire::MessageWriter>();
	if ( !CL_Q3IHVDemoGamestate( *game, *writer ) ) {
		Com_Printf( "ERROR: the gamestate does not fit a demo record\n" );
		return;
	}
	Com_Printf( "recording to %s.\n", name );
	clc.recordfile = FS_FOpenFileWrite( name );
	if ( !clc.recordfile ) {
		Com_Printf( "ERROR: couldn't open.\n" );
		return;
	}
	Q_strncpyz( clc.recordName, name, sizeof( clc.recordName ) );
	clc.demorecording = qtrue;
	clc.demowaiting = qtrue;
	CL_Q3IHVDemoWrite( writer->Data(), writer->Bytes() );
}

/* stoprecord (CL_StopRecord_f 0x4044f0): the -1 terminator. */
void CL_Q3IHVStopRecord_f( void ) {
	if ( !clc.demorecording ) {
		Com_Printf( "Not recording a demo.\n" );
		return;
	}
	const int end = LittleLong( -1 );
	FS_Write( &end, 4, clc.recordfile );
	FS_FCloseFile( clc.recordfile );
	clc.recordfile = 0;
	clc.demorecording = qfalse;
	clc.demowaiting = qfalse;
	Com_Printf( "Stopped demo.\n" );
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
