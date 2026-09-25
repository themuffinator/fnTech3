/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* The Q3Test releases' client session: each release's connectionless
 * handshake, one of Q3Test's two netchans, message bodies through
 * q3test_wire.hpp, and the native records the retail client game reads.
 * Retail's client state machine and send rules are followed throughout; the
 * facts and addresses are in docs/fntech3/Q3TEST.md. */
extern "C" {
#include "client.h"
}
#include "client_q3test.h"
#include "../compat/q3test_netchan.hpp"
#include "../compat/q3test_state_adapter.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace q3wire = fntech3::q3test::wire;

namespace {

// Retail's gameState_t holds 16000 characters (1.05 0x407800, 1.06 0x401230).
constexpr int Q3TEST_GAMESTATE_CHARS = 16000;
constexpr int Q3TEST_CONFIGSTRINGS = 1024;
constexpr int Q3TEST_COMMANDS = 64;
constexpr int Q3TEST_DEMO_MESSAGE_BYTES = 16384;

/* 35/36's reliable message: client commands written by one continuous writer
 * and handed to the channel when its reliable slot is free, as the server's
 * are (server_q3test_net.hpp). */
struct Q3TestClientPending {
	std::vector<std::uint8_t> data;
	q3wire::Writer out;
	void Reset() {
		data.assign( fntech3::q3ihv::FNTECH3_Q3_IHV_NETCHAN_BYTES, 0 );
		out = q3wire::Writer( data.data(), data.size() );
	}
};

struct Q3TestClientSession {
	fntech3::q3test::Netchan chan;
	std::unique_ptr<q3wire::ClientState> state;
	Q3TestClientPending pending;
	int csAck = -1;         // 35/36: the configstring sequence the client acknowledges
	bool overflowed = false;
};
Q3TestClientSession q3c;

const q3testRelease_t &CL_Q3TestRelease() {
	const q3testRelease_t *release = Com_Q3TestRelease();
	if ( !release ) Com_Error( ERR_FATAL, "Q3Test has no pinned release" );
	return *release;
}
int CL_Q3TestProtocol() { return CL_Q3TestRelease().protocol; }
bool CL_Q3TestLegacy() { return q3wire::IsLegacy( CL_Q3TestProtocol() ); }

bool CL_Q3TestSendDatagram( void *, const unsigned char *data, std::size_t size ) {
	NET_SendPacket( NS_CLIENT, static_cast<int>( size ), data, &clc.netchan.remoteAddress );
	return true;
}

void CL_Q3TestMirrorChannel() {
	clc.netchan.incomingSequence = q3c.chan.IncomingSequence();
	clc.netchan.outgoingSequence = q3c.chan.OutgoingSequence();
	clc.netchan.dropped = q3c.chan.Dropped();
	clc.serverMessageSequence = q3c.chan.IncomingSequence();
}

void CL_Q3TestTransmit( const void *data, std::size_t length ) {
	if ( !q3c.chan.Ready() ) return;
	if ( q3c.chan.Legacy() && q3c.pending.out.Size() && !q3c.chan.ReliableInFlight() && !q3c.chan.PendingReliable() ) {
		if ( !q3c.pending.out.Good() || !q3c.chan.QueueReliable( q3c.pending.data.data(), q3c.pending.out.Size() ) )
			q3c.overflowed = true;
		q3c.pending.Reset();
	}
	fntech3::q3test::NetchanSink sink{ nullptr, CL_Q3TestSendDatagram };
	q3c.chan.Transmit( sink, static_cast<const unsigned char *>( data ), length );
	clc.netchan.lastSentTime = Sys_Milliseconds();
	clc.netchan.lastSentSize = static_cast<int>( length );
	CL_Q3TestMirrorChannel();
	if ( q3c.overflowed || q3c.chan.FatalError() ) Com_Error( ERR_DROP, "Client command overflow" );
}

/* ---- configstrings --------------------------------------------------------- */

/* CL_SystemInfoChanged (1.02 0x406ec0, 1.06 0x4082f0, 1.08 0x408c20): the
 * server id, then every key applied to the client's cvars -- timescale up to
 * 1.06, the game's g_syncronousClients -- as retail's Cvar_Set did. `cheats`
 * drives this engine's cheat switch, and 1.06 and later reset the cheat
 * cvars while it is off. A cvar a server may not set here keeps its value, as
 * for every other profile (CL_SystemInfoChanged). */
void CL_Q3TestSystemInfoChanged() {
	const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SYSTEMINFO];
	cl.serverId = atoi( Info_ValueForKey( info, "sv_serverid" ) );
	if ( clc.demoplaying ) return;
	char key[BIG_INFO_KEY], value[BIG_INFO_VALUE];
	const char *s = info;
	do {
		s = Info_NextPair( s, key, value );
		if ( !key[0] ) break;
		if ( !Q_stricmp( key, "sv_serverid" ) ) continue;
		if ( !Q_stricmp( key, "cheats" ) ) {
			Cvar_Set( "sv_cheats", value );
			if ( !atoi( value ) && CL_Q3TestRelease().id >= Q3TEST_RELEASE_1_06 && !com_sv_running->integer ) Cvar_SetCheatState();
			continue;
		}
		const unsigned flags = Cvar_Flags( key );
		if ( flags == CVAR_NONEXISTENT ) {
			Cvar_Get( key, value, CVAR_SERVER_CREATED | CVAR_ROM );
		} else if ( flags & ( CVAR_SYSTEMINFO | CVAR_SERVER_CREATED | CVAR_USER_CREATED ) ) {
			Cvar_SetSafe( key, value );
		} else {
			Com_Printf( S_COLOR_YELLOW "WARNING: server is not allowed to set %s=%s\n", key, value );
		}
	} while ( *s );
}

/* CL_ConfigstringModified (1.06 0x401230) and 1.05's CL_ParseConfigString
 * (0x407800): the whole block is rebuilt under retail's 16000 characters. */
void CL_Q3TestSetConfigstring( int index, const char *value ) {
	if ( index < 0 || index >= Q3TEST_CONFIGSTRINGS ) Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
	const char *old = cl.gameState.stringData + cl.gameState.stringOffsets[index];
	if ( !strcmp( old, value ) ) return;
	auto previous = std::make_unique<gameState_t>( cl.gameState );
	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
	cl.gameState.dataCount = 1;
	for ( int i = 0; i < Q3TEST_CONFIGSTRINGS; ++i ) {
		const char *s = i == index ? value : previous->stringData + previous->stringOffsets[i];
		if ( !s[0] ) continue;
		const int length = static_cast<int>( strlen( s ) );
		if ( length + 1 + cl.gameState.dataCount > Q3TEST_GAMESTATE_CHARS ) Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
		cl.gameState.stringOffsets[i] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, s, length + 1 );
		cl.gameState.dataCount += length + 1;
	}
	if ( index == CS_SYSTEMINFO ) CL_Q3TestSystemInfoChanged();
}

/* ---- server messages ------------------------------------------------------- */

void CL_Q3TestGamestate( const q3wire::Gamestate &game ) {
	Con_Close();
	CL_Q3TestUIConnectCount( "" );
	clc.connectPacketCount = 0;
	CL_ClearState();
	const bool legacy = CL_Q3TestLegacy();
	if ( !legacy ) {
		clc.serverCommandSequence = game.sequence;
		clc.lastExecutedServerCommand = game.sequence;
	}
	// 36 acknowledges the gamestate's sequence, 35 nothing until a snapshot (1.02 0x4070c3)
	q3c.csAck = CL_Q3TestProtocol() == 36 ? game.sequence : -1;
	cl.gameState.dataCount = 1;
	for ( const q3wire::GamestateEntry &entry : game.entries ) {
		if ( entry.command != 3 ) continue;
		const int length = static_cast<int>( entry.text.size() );
		if ( length + 1 + cl.gameState.dataCount > Q3TEST_GAMESTATE_CHARS ) Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
		cl.gameState.stringOffsets[entry.index] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, entry.text.c_str(), length + 1 );
		cl.gameState.dataCount += length + 1;
	}
	clc.clientNum = 0;
	CL_Q3TestSystemInfoChanged();
	clc.eventMask |= EM_GAMESTATE;
	Cvar_Set( "cl_paused", "0" );
	CL_Q3TestStartCGame();
}

/* 35/36 run each command as it is parsed (1.05 CL_ParseServerCommand
 * 0x407980): the engine takes disconnect and clientLevelShot, the client game
 * the rest. */
void CL_Q3TestLegacyCommand( const std::string &text ) {
	Cmd_TokenizeString( text.c_str() );
	const char *verb = Cmd_Argv( 0 );
	if ( !strcmp( verb, "disconnect" ) ) Com_Error( ERR_SERVERDISCONNECT, "Server disconnected\n" );
	if ( !strcmp( verb, "clientLevelShot" ) ) {
		if ( com_sv_running->integer ) {
			Con_Close();
			Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		}
		return;
	}
	if ( cgvm && cls.state >= CA_LOADING ) {
		Cmd_TokenizeString( text.c_str() );
		CL_Q3TestCGameServerCommand();
	}
}

void CL_Q3TestSnapshot( const q3wire::Snapshot &snap ) {
	if ( !snap.Valid() ) return;
	const bool legacy = CL_Q3TestLegacy();
	clSnapshot_t mirror{};
	mirror.valid = qtrue;
	// Only the rate bit means the same to this engine; bit 1 is Q3Test's lost-commands flag.
	mirror.snapFlags = snap.snapFlags & SNAPFLAG_RATE_DELAYED;
	mirror.serverTime = snap.serverTime;
	mirror.messageNum = snap.messageNum;
	mirror.deltaNum = snap.DeltaNum();
	mirror.cmdNum = snap.lastUsercmd;
	mirror.areabytes = static_cast<int>( snap.areamask.size() );
	std::memcpy( mirror.areamask, snap.areamask.data(), snap.areamask.size() );
	mirror.numEntities = static_cast<int>( snap.entities.size() );
	if ( const auto *layout = fntech3::q3test::state::PlayerLayoutForProtocol( CL_Q3TestProtocol() ) )
		fntech3::q3test::state::PlayerToInternal( *layout, snap.player.data(), mirror.ps );
	// The ping is from the packet that carried the last command the server ran (1.05 0x407568).
	mirror.ping = 999;
	for ( int i = 0; i < PACKET_BACKUP; ++i ) {
		const int packet = ( clc.netchan.outgoingSequence - 1 - i ) & PACKET_MASK;
		if ( snap.lastUsercmd >= cl.outPackets[packet].p_cmdNumber ) {
			mirror.ping = std::min( cls.realtime - cl.outPackets[packet].p_realtime, 999 );
			break;
		}
	}
	mirror.ps.ping = mirror.ping;
	cl.snap = mirror;
	cl.snapshots[snap.messageNum & PACKET_MASK] = mirror;
	cl.newSnapshots = qtrue;
	if ( snap.DeltaNum() < 1 ) clc.demowaiting = qfalse;
	if ( legacy ) {
		q3c.csAck = snap.csSequence;
	} else if ( snap.reliableAcknowledge <= clc.reliableSequence
		&& snap.reliableAcknowledge >= clc.reliableSequence - Q3TEST_COMMANDS ) {
		clc.reliableAcknowledge = snap.reliableAcknowledge;
	}
	clc.eventMask |= EM_SNAPSHOT;
}

void CL_Q3TestParse( const std::uint8_t *data, std::size_t size ) {
	q3wire::ServerMessage message;
	std::string error;
	if ( !q3wire::ParseServerMessage( CL_Q3TestProtocol(), data, size, *q3c.state, message, &error ) ) {
		Com_Error( ERR_DROP, "CL_ParseServerMessage: %s", error.c_str() );
	}
	for ( const q3wire::ServerOp &op : message.ops ) {
		if ( const auto *game = std::get_if<q3wire::Gamestate>( &op ) ) {
			CL_Q3TestGamestate( *game );
		} else if ( const auto *update = std::get_if<q3wire::ConfigstringUpdate>( &op ) ) {
			const char *old = cl.gameState.stringData + cl.gameState.stringOffsets[update->index];
			if ( strcmp( old, update->text.c_str() ) ) {
				CL_Q3TestSetConfigstring( update->index, update->text.c_str() );
				if ( cgvm && cls.state >= CA_LOADING ) CL_Q3TestCGameConfigstringModified( update->index );
			}
		} else if ( const auto *command = std::get_if<q3wire::ServerCommand>( &op ) ) {
			if ( CL_Q3TestLegacy() ) {
				CL_Q3TestLegacyCommand( command->text );
			} else if ( command->fresh ) {
				clc.serverCommandSequence = command->sequence;
				Q_strncpyz( clc.serverCommands[command->sequence & ( Q3TEST_COMMANDS - 1 )], command->text.c_str(),
					sizeof( clc.serverCommands[0] ) );
				clc.eventMask |= EM_COMMAND;
			}
		} else if ( const auto *snap = std::get_if<q3wire::Snapshot>( &op ) ) {
			CL_Q3TestSnapshot( *snap );
		}
	}
}

/* ---- server lists ------------------------------------------------------------ */

/* The LAN list (16 records), the master's list (256) and the ping queue (16
 * slots), each an address and the info string retail stored with it
 * (net_transport.md 5.5, 5.10, 5.11). */
struct Q3TestServer {
	netadr_t adr{};
	std::string info;
};
struct Q3TestPingSlot {
	netadr_t adr{};
	int start = 0;
	int time = 0;
};
struct Q3TestBrowser {
	std::vector<Q3TestServer> local;
	int localCount = 0;
	std::vector<Q3TestServer> global;
	int globalCount = 0;
	std::array<Q3TestPingSlot, 16> pings{};
};
Q3TestBrowser q3b;

bool CL_Q3TestSlotLive( const Q3TestPingSlot &slot ) { return slot.adr.port != 0; }

/* CL_ServerInfoPacket (1.02 0x4067b0, 1.06 0x407830): a pending ping takes the
 * reply first (1.06 and later); anything else joins the LAN list unless its
 * address is there. The challenge is never checked. 1.06 counts the index it
 * just filled, one short; 1.07 and 1.08 count the records. */
void CL_Q3TestServerInfo( const netadr_t *from, msg_t *msg ) {
	const q3testReleaseId_t release = CL_Q3TestRelease().id;
	if ( release >= Q3TEST_RELEASE_1_06 ) {
		for ( Q3TestPingSlot &slot : q3b.pings ) {
			if ( CL_Q3TestSlotLive( slot ) && !slot.time && NET_CompareAdr( from, &slot.adr ) ) {
				slot.time = cls.realtime - slot.start + ( release >= Q3TEST_RELEASE_1_08 ? 1 : 0 );
				Com_DPrintf( "ping time %dms from %s\n", slot.time, NET_AdrToString( from ) );
				return;
			}
		}
	}
	for ( const Q3TestServer &server : q3b.local ) {
		if ( NET_CompareAdr( from, &server.adr ) ) return;
	}
	if ( q3b.local.size() >= 16 ) {
		Com_DPrintf( "MAX_LOCAL_SERVERS hit, dropping infoResponse\n" );
		return;
	}
	Q3TestServer server;
	server.adr = *from;
	server.info = MSG_ReadString( msg );
	q3b.local.push_back( server );
	const int index = static_cast<int>( q3b.local.size() ) - 1;
	q3b.localCount = release == Q3TEST_RELEASE_1_06 ? index : index + 1;
	Com_Printf( "%s: %s\n", NET_AdrToString( from ), server.info.c_str() );
}

/* CL_ServersResponsePacket (1.06 0x406680, 1.08 0x406f90): binary records
 * after the first backslash, each ip[4] port[2] ':' map ':' clients maxclients
 * ':' hostname, stored as info strings. */
void CL_Q3TestServersResponse( const msg_t *msg ) {
	if ( q3b.globalCount < 0 ) q3b.globalCount = 0;
	const unsigned char *data = msg->data;
	const int size = msg->cursize;
	int at = 1;
	while ( at < size && data[at] != '\\' ) ++at;
	auto clean = []( std::string text ) {
		for ( char &c : text ) {
			if ( static_cast<unsigned char>( c ) < 0x20 || static_cast<unsigned char>( c ) > 0x7e ) c = ' ';
		}
		return text;
	};
	while ( at < size && q3b.global.size() < 256 ) {
		if ( data[at] != '\\' ) break;
		++at;
		if ( at + 3 <= size && !memcmp( data + at, "EOT", 3 ) ) break;
		if ( at + 7 > size ) break;
		const unsigned char *ip = data + at;
		const int port = ( data[at + 4] << 8 ) | data[at + 5];
		at += 6;
		if ( data[at] != ':' ) break;
		++at;
		std::string map;
		while ( at < size && data[at] != ':' && map.size() < 16 ) map.push_back( static_cast<char>( data[at++] ) );
		if ( map.size() >= 16 || at >= size ) break;
		++at;
		if ( at + 3 > size ) break;
		const int clients = data[at], maxclients = data[at + 1];
		at += 2;
		if ( data[at] != ':' ) break;
		++at;
		std::string host;
		while ( at < size && data[at] != '\\' ) {
			if ( host.size() < 31 ) host.push_back( static_cast<char>( data[at] ) );
			++at;
		}
		Q3TestServer server;
		char info[MAX_INFO_STRING] = "";
		Info_SetValueForKey( info, "ip", va( "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port ) );
		Info_SetValueForKey( info, "hostname", clean( host ).c_str() );
		Info_SetValueForKey( info, "map", clean( map ).c_str() );
		Info_SetValueForKey( info, "clients", va( "%d", clients ) );
		Info_SetValueForKey( info, "maxclients", va( "%d", maxclients ) );
		server.info = info;
		Com_Printf( "server: %d ip: %d.%d.%d.%d:%d map: %s host: %s clients: %d/%d\n", static_cast<int>( q3b.global.size() ),
			ip[0], ip[1], ip[2], ip[3], port, map.c_str(), host.c_str(), clients, maxclients );
		q3b.global.push_back( server );
		q3b.globalCount = static_cast<int>( q3b.global.size() );
	}
}

/* ---- demos -------------------------------------------------------------------- */

/* A .dm3 record: a little-endian length and the message after its netchan
 * header (CL_WriteDemoMessage 1.05 0x404db0, 1.06 0x405300). */
void CL_Q3TestDemoWrite( const void *data, std::size_t size ) {
	if ( !clc.demorecording || !clc.recordfile || !size ) return;
	const int length = LittleLong( static_cast<int>( size ) );
	FS_Write( &length, 4, clc.recordfile );
	FS_Write( data, static_cast<int>( size ), clc.recordfile );
}

/* ---- connectionless -------------------------------------------------------- */

void CL_Q3TestResetSession() {
	q3c.chan = fntech3::q3test::Netchan{};
	q3c.state = std::make_unique<q3wire::ClientState>( CL_Q3TestProtocol() );
	q3c.pending.Reset();
	q3c.csAck = -1;
	q3c.overflowed = false;
}

/* CL_ConnectionlessPacket (1.05 0x406140, 1.06 0x4069b0). */
void CL_Q3TestConnectionless( const netadr_t *from, msg_t *msg ) {
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );
	const char *line = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( line );
	const char *verb = Cmd_Argv( 0 );
	Com_DPrintf( "CL packet %s: %s\n", NET_AdrToString( from ), verb );
	const bool legacy = CL_Q3TestLegacy();
	if ( !strcmp( verb, "challengeResponse" ) ) {
		// 1.06 and later take one only while connecting, and adopt its sender
		if ( cls.state != CA_CONNECTING ) {
			if ( !legacy ) {
				Com_DPrintf( "Unwanted challenge response received.  Ignored.\n" );
				return;
			}
			if ( cls.state != CA_CHALLENGING ) return;
		}
		clc.challenge = atoi( Cmd_Argv( 1 ) );
		if ( !legacy ) clc.serverAddress = *from;
		cls.state = CA_CHALLENGING;
		clc.connectPacketCount = 0;
		clc.connectTime = -99999;
		return;
	}
	if ( !strcmp( verb, "connectResponse" ) ) {
		if ( cls.state >= CA_CONNECTED ) {
			Com_Printf( "Dup connect received.  Ignored.\n" );
			return;
		}
		if ( cls.state != CA_CHALLENGING ) {
			Com_Printf( "connectResponse packet while not connecting.  Ignored.\n" );
			return;
		}
		if ( !NET_CompareBaseAdr( from, &clc.serverAddress ) ) {
			Com_Printf( "connectResponse from a different address.  Ignored.\n" );
			Com_Printf( "%s should have been %s\n", NET_AdrToString( from ), NET_AdrToString( &clc.serverAddress ) );
			return;
		}
		const int qport = Cvar_VariableIntegerValue( "net_qport" ) & 0xffff;
		Netchan_Setup( NS_CLIENT, &clc.netchan, from, qport, clc.challenge, CL_Q3TestWireProfile() );
		CL_Q3TestResetSession();
		q3c.chan.Setup( legacy, fntech3::q3test::NetchanSource::Client, qport );
		cls.state = CA_CONNECTED;
		clc.lastPacketSentTime = -9999;
		return;
	}
	// 1.02 has no print handler (net_transport.md 5.8)
	if ( !strcmp( verb, "print" ) && CL_Q3TestRelease().id >= Q3TEST_RELEASE_1_03 ) {
		const char *text = MSG_ReadString( msg );
		Q_strncpyz( clc.serverMessage, text, sizeof( clc.serverMessage ) );
		CL_Q3TestUIServerMessage( text );
		Com_Printf( "%s", text );
		return;
	}
	if ( !strcmp( verb, "infoResponse" ) ) {
		CL_Q3TestServerInfo( from, msg );
		return;
	}
	if ( !strcmp( verb, "getserversResponse\\" ) && CL_Q3TestRelease().id >= Q3TEST_RELEASE_1_06 ) {
		CL_Q3TestServersResponse( msg );
		return;
	}
	if ( !strcmp( verb, "echo" ) ) {
		NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv( 1 ) );
		return;
	}
	/* Retail never honours an out-of-band disconnect: the packet itself
	 * refreshes the timeout its check requires (1.02 0x405580). */
	if ( !strcmp( verb, "disconnect" ) || !strcmp( verb, "motd" ) ) return;
	Com_DPrintf( "Unknown connectionless packet command.\n" );
}

/* ---- usercmds -------------------------------------------------------------- */

/* The engine's command as the release's record: 20 bytes with msec (36-39,
 * 1.05 CL_FinishMove 0x403660), 24 with serverTime (40, 0x404370), 14 with
 * 16-bit angles (35). Buttons keep bits 0-6 and set 0x80 for any key. */
q3wire::Usercmd CL_Q3TestNativeCmd( int number ) {
	const usercmd_t &cmd = cl.cmds[number & CMD_MASK];
	const usercmd_t &previous = cl.cmds[( number - 1 ) & CMD_MASK];
	q3wire::Usercmd out{};
	const int protocol = CL_Q3TestProtocol();
	const auto buttons = static_cast<std::uint8_t>( ( cmd.buttons & 0x7f ) | ( cmd.buttons & BUTTON_ANY ? 0x80 : 0 ) );
	int msec = cmd.serverTime - previous.serverTime;
	if ( msec < 0 ) msec = 0;
	if ( msec > 250 ) msec = 100;
	auto store = []( std::uint8_t *at, unsigned bytes, std::uint32_t value ) { q3wire::Store( at, bytes, value ); };
	if ( protocol == 40 ) {
		store( out.data(), 4, static_cast<std::uint32_t>( cmd.serverTime ) );
		out[4] = buttons;
		out[5] = cmd.weapon;
		for ( int i = 0; i < 3; ++i ) store( out.data() + 8 + 4 * i, 4, static_cast<std::uint32_t>( cmd.angles[i] & 0xffff ) );
		out[20] = static_cast<std::uint8_t>( cmd.forwardmove );
		out[21] = static_cast<std::uint8_t>( cmd.rightmove );
		out[22] = static_cast<std::uint8_t>( cmd.upmove );
	} else if ( protocol == 35 ) {
		out[0] = static_cast<std::uint8_t>( msec );
		out[1] = buttons;
		out[2] = cmd.weapon;
		for ( int i = 0; i < 3; ++i ) store( out.data() + 4 + 2 * i, 2, static_cast<std::uint32_t>( cmd.angles[i] & 0xffff ) );
		out[10] = static_cast<std::uint8_t>( cmd.forwardmove );
		out[11] = static_cast<std::uint8_t>( cmd.rightmove );
		out[12] = static_cast<std::uint8_t>( cmd.upmove );
	} else {
		out[0] = static_cast<std::uint8_t>( msec );
		out[1] = buttons;
		out[2] = cmd.weapon;
		for ( int i = 0; i < 3; ++i ) store( out.data() + 4 + 4 * i, 4, static_cast<std::uint32_t>( cmd.angles[i] & 0xffff ) );
		out[16] = static_cast<std::uint8_t>( cmd.forwardmove );
		out[17] = static_cast<std::uint8_t>( cmd.rightmove );
		out[18] = static_cast<std::uint8_t>( cmd.upmove );
	}
	return out;
}

/* CL_WritePacket (1.05 0x4038e0, 1.06 0x403d80). 35/36 put client commands
 * in the reliable message and the move after them; 38-40 repeat every
 * unacknowledged command before the move. A move names the newest command:
 * 35/36 also carry a provisional one last, which this engine sends as a copy
 * of the newest with no time of its own. */
void CL_Q3TestWritePacket( bool withMove ) {
	const int protocol = CL_Q3TestProtocol();
	const bool legacy = q3wire::IsLegacy( protocol );
	std::array<std::uint8_t, q3wire::MaxMessageBytes> buffer{};
	q3wire::Writer out( buffer.data(), legacy ? 1024 : buffer.size() );
	const int limit = Q3TEST_COMMANDS;
	if ( legacy ) {
		while ( clc.reliableAcknowledge < clc.reliableSequence ) {
			++clc.reliableAcknowledge;
			if ( !q3wire::WriteClientCommand( q3c.pending.out, protocol, 0, clc.reliableCommands[clc.reliableAcknowledge & ( limit - 1 )] ) )
				q3c.overflowed = true;
		}
	} else {
		for ( int sequence = clc.reliableAcknowledge + 1; sequence <= clc.reliableSequence; ++sequence ) {
			if ( !q3wire::WriteClientCommand( out, protocol, sequence, clc.reliableCommands[sequence & ( limit - 1 )] ) ) break;
		}
	}
	if ( withMove ) {
		const int packetdup = std::clamp( Cvar_VariableIntegerValue( "cl_packetdup" ), 0, 5 );
		const int oldPacket = ( clc.netchan.outgoingSequence - 1 - packetdup ) & PACKET_MASK;
		int count = cl.cmdNumber - cl.outPackets[oldPacket].p_cmdNumber;
		const int maximum = legacy ? static_cast<int>( q3wire::MaxMoveCommands ) - 1 : static_cast<int>( q3wire::MaxMoveCommands );
		if ( count > maximum ) count = maximum;
		if ( count >= 1 ) {
			q3wire::Move move;
			move.serverCommandSequence = clc.serverCommandSequence;
			move.serverId = cl.serverId;
			move.clientTime = cls.realtime;
			move.deltaMessage = ( Cvar_VariableIntegerValue( "cl_nodelta" ) || !cl.snap.valid || clc.demowaiting )
				? -1 : cl.snap.messageNum;
			move.csAck = q3c.csAck;
			move.lastCmd = cl.cmdNumber;
			for ( int number = cl.cmdNumber - count + 1; number <= cl.cmdNumber; ++number )
				move.cmds.push_back( CL_Q3TestNativeCmd( number ) );
			if ( legacy ) {
				q3wire::Usercmd partial = move.cmds.back();
				if ( protocol != 40 ) partial[0] = 0;
				move.cmds.push_back( partial );
			}
			q3wire::WriteMove( out, protocol, move );
		}
	}
	const int packet = clc.netchan.outgoingSequence & PACKET_MASK;
	cl.outPackets[packet].p_realtime = cls.realtime;
	cl.outPackets[packet].p_serverTime = cl.cmds[cl.cmdNumber & CMD_MASK].serverTime;
	cl.outPackets[packet].p_cmdNumber = cl.cmdNumber;
	clc.lastPacketSentTime = cls.realtime;
	CL_Q3TestTransmit( buffer.data(), out.Size() );
}

} // namespace

/* ---- hooks ------------------------------------------------------------------ */

netchanWireProfile_t CL_Q3TestWireProfile( void ) {
	return CL_Q3TestLegacy() ? NETCHAN_WIRE_Q3TEST_A : NETCHAN_WIRE_Q3TEST_B;
}

/* CL_CheckForResend (1.05 0x405de0, 1.06 0x406310): getchallenge while
 * connecting, connect while challenging, every three seconds; a loopback
 * server needs no challenge. */
void CL_Q3TestCheckForResend( void ) {
	if ( clc.demoplaying || ( cls.state != CA_CONNECTING && cls.state != CA_CHALLENGING ) ) return;
	if ( cls.state == CA_CONNECTING && NET_IsLocalAddress( &clc.serverAddress ) ) {
		cls.state = CA_CHALLENGING;
		clc.challenge = 0;
		clc.connectTime = -99999;
	}
	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) return;
	clc.connectTime = cls.realtime;
	clc.connectPacketCount++;
	CL_Q3TestUIConnectCount( va( "(%i)", clc.connectPacketCount ) );
	if ( cls.state == CA_CONNECTING ) {
		NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "getchallenge" );
		return;
	}
	const int protocol = CL_Q3TestProtocol();
	const int qport = Cvar_VariableIntegerValue( "net_qport" ) & 0xffff;
	char info[MAX_INFO_STRING];
	Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO, nullptr ), sizeof( info ) );
	if ( protocol == 35 ) {
		NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "connect %i %i %i \"%s\"", protocol, qport, clc.challenge, info );
	} else {
		// 1.05 names the qport "port" (0x405fc9); 1.06 and later "qport"
		Info_SetValueForKey( info, "protocol", va( "%i", protocol ) );
		Info_SetValueForKey( info, protocol == 36 ? "port" : "qport", va( "%i", qport ) );
		Info_SetValueForKey( info, "challenge", va( "%i", clc.challenge ) );
		NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "connect \"%s\"", info );
	}
	cvar_modifiedFlags &= ~CVAR_USERINFO;
}

/* CL_PacketEvent (every release, 1.02 0x405900): every packet refreshes the
 * timeout; sequenced ones must come from the channel's exact address. */
void CL_Q3TestPacketEvent( const netadr_t *from, msg_t *msg ) {
	clc.lastPacketTime = cls.realtime;
	if ( msg->cursize >= 4 ) {
		int32_t marker;
		std::memcpy( &marker, msg->data, sizeof( marker ) );
		if ( marker == -1 ) {
			CL_Q3TestConnectionless( from, msg );
			return;
		}
	}
	if ( cls.state < CA_CONNECTED || !q3c.state ) return;
	if ( msg->cursize < 8 ) {
		Com_Printf( "%s: Runt packet\n", NET_AdrToString( from ) );
		return;
	}
	if ( !NET_CompareAdr( from, &clc.netchan.remoteAddress ) ) {
		Com_DPrintf( "%s:sequenced packet without connection\n", NET_AdrToString( from ) );
		return;
	}
	std::size_t size = static_cast<std::size_t>( msg->cursize );
	const auto result = q3c.chan.Process( msg->data, size, static_cast<std::size_t>( msg->maxsize ) );
	CL_Q3TestMirrorChannel();
	if ( result != fntech3::q3test::NetchanResult::Message ) return;
	CL_Q3TestParse( msg->data, size );
	if ( clc.demorecording && !clc.demowaiting ) CL_Q3TestDemoWrite( msg->data, size );
}

/* CL_SendCmd (1.05 0x4038e0, 1.06 0x403fb0), after this engine has created
 * the frame's command. Connected and loading clients send once a second:
 * 35/36 only while reliable data waits or the second has passed, 38-40 a move
 * with the stale server id that asks for the gamestate. Primed and active
 * clients are held to cl_maxpackets, except to loopback, and 38-40 LAN
 * servers; 35/36 need two commands for a move. */
void CL_Q3TestSendCmd( void ) {
	if ( cls.state < CA_CONNECTED || clc.demoplaying || cls.state == CA_CINEMATIC || !q3c.state ) return;
	const bool legacy = CL_Q3TestLegacy();
	if ( cls.state < CA_PRIMED ) {
		const bool waiting = legacy && ( clc.reliableAcknowledge < clc.reliableSequence || q3c.pending.out.Size() );
		if ( !waiting && cls.realtime - clc.lastPacketSentTime < ( legacy ? 1001 : 1000 ) ) return;
		if ( !legacy ) {
			// retail creates a command every frame whatever the state (1.06 0x403c30)
			cl.cmdNumber++;
			cl.cmds[cl.cmdNumber & CMD_MASK] = usercmd_t{};
			cl.cmds[cl.cmdNumber & CMD_MASK].serverTime = cl.serverTime;
		}
		CL_Q3TestWritePacket( !legacy );
		return;
	}
	const int maxpackets = std::clamp( Cvar_VariableIntegerValue( "cl_maxpackets" ), 5, 100 );
	const bool unlimited = clc.netchan.remoteAddress.type == NA_LOOPBACK
		|| ( !legacy && Sys_IsLANAddress( &clc.netchan.remoteAddress ) );
	if ( !unlimited ) {
		const int oldPacket = ( clc.netchan.outgoingSequence - 1 ) & PACKET_MASK;
		if ( cls.realtime - cl.outPackets[oldPacket].p_realtime < 1000 / maxpackets ) return;
	}
	if ( legacy ) {
		const int packetdup = std::clamp( Cvar_VariableIntegerValue( "cl_packetdup" ), 0, 5 );
		const int oldPacket = ( clc.netchan.outgoingSequence - 1 - packetdup ) & PACKET_MASK;
		if ( cl.cmdNumber - cl.outPackets[oldPacket].p_cmdNumber < 1 ) return;
	}
	CL_Q3TestWritePacket( true );
}

/* CL_Disconnect (1.02 0x404c50, 1.06 0x405a70): 35/36 send the command three
 * times unreliably; 38-40 queue it and write three packets. */
void CL_Q3TestDisconnect( void ) {
	if ( cls.state < CA_CONNECTED || clc.demoplaying || !q3c.state || !q3c.chan.Ready() ) return;
	const int protocol = CL_Q3TestProtocol();
	if ( q3wire::IsLegacy( protocol ) ) {
		std::uint8_t buffer[64];
		for ( int i = 0; i < 3; ++i ) {
			q3wire::Writer out( buffer, sizeof( buffer ) );
			q3wire::WriteClientCommand( out, protocol, 0, "disconnect" );
			fntech3::q3test::NetchanSink sink{ nullptr, CL_Q3TestSendDatagram };
			q3c.chan.Transmit( sink, buffer, out.Size() );
		}
		return;
	}
	CL_AddReliableCommand( "disconnect", qtrue );
	for ( int i = 0; i < 3; ++i ) CL_Q3TestWritePacket( false );
}

void CL_Q3TestClearConnection( void ) {
	q3c.chan = fntech3::q3test::Netchan{};
	q3c.state.reset();
	q3c.pending.Reset();
	q3c.csAck = -1;
	q3c.overflowed = false;
}

/* record [name] (CL_Record_f, 1.02 0x404680, 1.05 0x404f20, 1.06 0x405470,
 * 1.08 0x405d80): demos/<name>.dm3 or the first free demos/demoNNNN.dm3, whose
 * first record is a gamestate built from the client's own configstrings and
 * baselines. Recording waits for the next full snapshot, which the next move
 * asks for. 1.05 and later refused unless g_syncronousClients was set; this
 * engine only warns. */
void CL_Q3TestRecord_f( void ) {
	if ( Cmd_Argc() > 2 ) {
		Com_Printf( "record <demoname>\n" );
		return;
	}
	if ( clc.demorecording ) {
		Com_Printf( "Already recording.\n" );
		return;
	}
	if ( cls.state != CA_ACTIVE || !q3c.state ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}
	if ( CL_Q3TestRelease().id >= Q3TEST_RELEASE_1_05 && !Cvar_VariableValue( "g_syncronousClients" ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: the server does not have 'g_syncronousClients 1' set, which retail required for demos\n" );
	}
	char name[MAX_OSPATH];
	if ( Cmd_Argc() == 2 ) {
		Com_sprintf( name, sizeof( name ), "demos/%s.dm3", Cmd_Argv( 1 ) );
	} else {
		int number = 0;
		for ( ; number <= 9999; ++number ) {
			Com_sprintf( name, sizeof( name ), "demos/demo%04i.dm3", number );
			if ( !FS_FileExists( name ) ) break;
		}
		if ( number > 9999 ) {
			Com_Printf( "ERROR: no free demo name\n" );
			return;
		}
	}
	const int protocol = CL_Q3TestProtocol();
	auto strings = std::make_unique<q3wire::Configstrings>();
	for ( int i = 0; i < Q3TEST_CONFIGSTRINGS; ++i ) {
		if ( cl.gameState.stringOffsets[i] ) ( *strings )[static_cast<std::size_t>( i )] = cl.gameState.stringData + cl.gameState.stringOffsets[i];
	}
	auto zero = std::make_unique<q3wire::Baselines>();
	const q3wire::Baselines &baselines = q3c.state->baselines ? *q3c.state->baselines : *zero;
	std::vector<std::uint8_t> buffer( q3wire::MaxMessageBytes );
	q3wire::Writer out( buffer.data(), buffer.size() );
	if ( !q3wire::WriteDemoGamestate( out, protocol, protocol == 36 ? -1 : clc.serverCommandSequence, *strings, baselines ) ) {
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
	CL_Q3TestDemoWrite( buffer.data(), out.Size() );
}

/* stoprecord (1.02 0x404560, 1.06 0x405350): the -1 terminator. */
void CL_Q3TestStopRecord_f( void ) {
	if ( !clc.demorecording ) {
		Com_Printf( "Not recording a demo.\n" );
		return;
	}
	const int end = LittleLong( -1 );
	FS_Write( &end, 4, clc.recordfile );
	FS_FCloseFile( clc.recordfile );
	clc.recordfile = 0;
	clc.demorecording = qfalse;
	Com_Printf( "Stopped demo.\n" );
}

/* One record of the demo being played (CL_ReadDemoMessage 1.05 0x405230,
 * 1.06 0x405780): false once it has ended, is truncated or is -1. */
bool CL_Q3TestReadDemoMessage( void ) {
	if ( !clc.demofile ) return false;
	int length = 0;
	if ( FS_Read( &length, 4, clc.demofile ) != 4 ) return false;
	length = LittleLong( length );
	if ( length == -1 ) return false;
	if ( length < 0 || length > Q3TEST_DEMO_MESSAGE_BYTES ) {
		Com_Error( ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );
		return false;
	}
	std::vector<std::uint8_t> data( static_cast<std::size_t>( length ) + 1 );
	if ( !length || FS_Read( data.data(), length, clc.demofile ) != length ) {
		Com_Printf( "Demo file was truncated.\n" );
		return false;
	}
	clc.lastPacketTime = cls.realtime;
	if ( !q3c.state ) CL_Q3TestResetSession();
	CL_Q3TestParse( data.data(), static_cast<std::size_t>( length ) );
	return true;
}

/* demo <name> (CL_PlayDemo_f): demos/<name>.dm3, read until the client game
 * is primed. The session state starts empty; the demo's first record is its
 * gamestate. */
void CL_Q3TestPlayDemo_f( void ) {
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
	CL_Q3TestResetSession();
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED ) {
		CL_ReadDemoMessage();
	}
	clc.firstDemoFrameSkipped = qfalse;
}

/* scanservers / localservers (1.02 0x406890, 1.06 0x4079b0): two passes of
 * `getinfo xxx` to ports 27960-27963 on the broadcast address. */
void CL_Q3TestLocalServers_f( void ) {
	Com_Printf( "Scanning for servers on the local network...\n" );
	q3b.local.clear();
	q3b.localCount = 0;
	for ( int pass = 0; pass < 2; ++pass ) {
		for ( int port = 0; port < 4; ++port ) {
			netadr_t to{};
			to.type = NA_BROADCAST;
			to.port = BigShort( static_cast<short>( PORT_SERVER + port ) );
			NET_SendPacket( NS_CLIENT, 15, "\xff\xff\xff\xffgetinfo xxx", &to );
		}
	}
}

/* globalservers <protocol> [zone|keyword] (1.06 0x407a90, 1.08 0x4083d0):
 * `getservers` to the master the release names. 1.08 shows -1 until the first
 * response. */
void CL_Q3TestGlobalServers_f( void ) {
	if ( CL_Q3TestRelease().id < Q3TEST_RELEASE_1_06 ) {
		Com_Printf( "Unknown command \"%s\"\n", Cmd_Argv( 0 ) );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: globalservers <protocol> [%s]\n", CL_Q3TestRelease().id >= Q3TEST_RELEASE_1_08 ? "keyword" : "zone" );
		return;
	}
	netadr_t to{};
	if ( !NET_StringToAdr( "master3.idsoftware.com:27950", &to, NA_IP ) ) {
		Com_Printf( "Couldn't resolve address: master3.idsoftware.com\n" );
		return;
	}
	q3b.global.clear();
	q3b.globalCount = CL_Q3TestRelease().id >= Q3TEST_RELEASE_1_08 ? -1 : 0;
	if ( Cmd_Argc() >= 3 ) NET_OutOfBandPrint( NS_SERVER, &to, "getservers %s %s", Cmd_Argv( 1 ), Cmd_Argv( 2 ) );
	else NET_OutOfBandPrint( NS_SERVER, &to, "getservers %s", Cmd_Argv( 1 ) );
}

/* CL_GetFreePing (1.06 0x407c70): an empty slot, or one pending or answered for
 * 500 ms or more, else the oldest. */
static Q3TestPingSlot &CL_Q3TestFreePing() {
	Q3TestPingSlot *oldest = &q3b.pings[0];
	for ( Q3TestPingSlot &slot : q3b.pings ) {
		if ( !CL_Q3TestSlotLive( slot ) ) return slot;
		if ( cls.realtime - slot.start >= 500 ) return slot;
		if ( slot.start - oldest->start < 0 ) oldest = &slot;
	}
	return *oldest;
}

/* ping <server> (1.06 0x407cf0). */
void CL_Q3TestPing_f( void ) {
	if ( CL_Q3TestRelease().id < Q3TEST_RELEASE_1_06 ) {
		Com_Printf( "Unknown command \"%s\"\n", Cmd_Argv( 0 ) );
		return;
	}
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: ping [server]\n" );
		return;
	}
	netadr_t to{};
	if ( !NET_StringToAdr( Cmd_Argv( 1 ), &to, NA_IP ) ) return;
	to.type = NA_IP;
	Q3TestPingSlot &slot = CL_Q3TestFreePing();
	slot.adr = to;
	slot.start = cls.realtime;
	slot.time = 0;
	NET_OutOfBandPrint( NS_CLIENT, &to, "getinfo xxx" );
}

/* ---- services for the client game ------------------------------------------ */

namespace fntech3::q3test::client {

int LocalServerCount() { return q3b.localCount; }
/* The stored string with `nettype` added (1.06 0x409140). */
std::string LocalServerInfo( int n ) {
	if ( n < 0 || n >= static_cast<int>( q3b.local.size() ) ) return {};
	const Q3TestServer &server = q3b.local[static_cast<std::size_t>( n )];
	const netadrtype_t type = server.adr.type;
	return server.info + "\\nettype\\" + ( type == NA_IP || type == NA_BROADCAST ? "udp" : "???" );
}
std::string LocalServerAddress( int n ) {
	if ( n < 0 || n >= static_cast<int>( q3b.local.size() ) ) return {};
	return NET_AdrToString( &q3b.local[static_cast<std::size_t>( n )].adr );
}
int GlobalServerCount() { return q3b.globalCount; }
std::string GlobalServerInfo( int n ) {
	if ( n < 0 || n >= static_cast<int>( q3b.global.size() ) ) return {};
	return q3b.global[static_cast<std::size_t>( n )].info;
}
/* Retail never filled this address (5.7); the stored ip is the same. */
std::string GlobalServerAddress( int n ) {
	if ( n < 0 || n >= static_cast<int>( q3b.global.size() ) ) return {};
	return Info_ValueForKey( q3b.global[static_cast<std::size_t>( n )].info.c_str(), "ip" );
}
int PingQueueCount() {
	int count = 0;
	for ( const Q3TestPingSlot &slot : q3b.pings ) count += CL_Q3TestSlotLive( slot ) ? 1 : 0;
	return count;
}
void ClearPing( int n ) {
	if ( n >= 0 && n < static_cast<int>( q3b.pings.size() ) ) q3b.pings[static_cast<std::size_t>( n )] = Q3TestPingSlot{};
}
/* An answered slot gives its time, an unanswered one 0 until 500 ms have
 * passed and then the time waited (1.06 0x409300). */
void GetPing( int n, char *address, int size, int *time ) {
	*time = 0;
	if ( address && size > 0 ) address[0] = '\0';
	if ( n < 0 || n >= static_cast<int>( q3b.pings.size() ) ) return;
	const Q3TestPingSlot &slot = q3b.pings[static_cast<std::size_t>( n )];
	if ( !CL_Q3TestSlotLive( slot ) ) return;
	if ( address && size > 0 ) Q_strncpyz( address, NET_AdrToString( &slot.adr ), size );
	if ( slot.time ) *time = slot.time;
	else if ( cls.realtime - slot.start >= 500 ) *time = cls.realtime - slot.start;
}
void LocalServersScan() { CL_Q3TestLocalServers_f(); }

int Protocol() { return CL_Q3TestProtocol(); }
bool Legacy() { return CL_Q3TestLegacy(); }

bool CurrentSnapshot( int &messageNum, int &serverTime ) {
	messageNum = cl.snap.messageNum;
	serverTime = cl.snap.serverTime;
	return cl.snap.valid != qfalse;
}

/* CL_GetSnapshot (1.05 0x401130, 1.06 0x4010c0). */
std::shared_ptr<const wire::Snapshot> Snapshot( int messageNum ) {
	if ( messageNum > cl.snap.messageNum ) Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.frame.messageNum" );
	if ( !q3c.state || cl.snap.messageNum - messageNum >= PACKET_BACKUP ) return nullptr;
	auto frame = q3c.state->Frame( messageNum );
	if ( !frame || q3c.state->parseEntitiesNum - frame->parseEntitiesNum >= 2048 ) return nullptr;
	return frame;
}

int SnapshotPing( const wire::Snapshot &snap ) {
	return cl.snapshots[snap.messageNum & PACKET_MASK].messageNum == snap.messageNum
		? cl.snapshots[snap.messageNum & PACKET_MASK].ping : 999;
}

void WriteGameState( void *out, std::size_t bytes ) {
	struct Native {
		int32_t stringOffsets[1024];
		char stringData[Q3TEST_GAMESTATE_CHARS];
		int32_t dataCount;
	};
	static_assert( sizeof( Native ) == 20100, "Q3Test gameState_t" );
	if ( !out || bytes < sizeof( Native ) ) return;
	auto native = std::make_unique<Native>();
	std::memset( native.get(), 0, sizeof( Native ) );
	native->dataCount = 1;
	for ( int i = 0; i < Q3TEST_CONFIGSTRINGS; ++i ) {
		const char *s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		if ( !s[0] ) continue;
		const int length = static_cast<int>( strlen( s ) );
		if ( native->dataCount + length + 1 > Q3TEST_GAMESTATE_CHARS ) break;
		native->stringOffsets[i] = native->dataCount;
		std::memcpy( native->stringData + native->dataCount, s, static_cast<std::size_t>( length ) + 1 );
		native->dataCount += length + 1;
	}
	std::memcpy( out, native.get(), sizeof( Native ) );
}

const char *Configstring( int index ) {
	if ( index < 0 || index >= Q3TEST_CONFIGSTRINGS ) return "";
	return cl.gameState.stringData + cl.gameState.stringOffsets[index];
}

/* CL_GetServerCommand (1.06 0x401390, instruction-identical to 1.08's). */
bool ServerCommand( int number ) {
	if ( number <= clc.serverCommandSequence - Q3TEST_COMMANDS ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return false;
	}
	if ( number > clc.serverCommandSequence ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
		return false;
	}
	const char *text = clc.serverCommands[number & ( Q3TEST_COMMANDS - 1 )];
	clc.lastExecutedServerCommand = number;
	Com_DPrintf( "serverCommand: %i : %s\n", number, text );
	Cmd_TokenizeString( text );
	const char *verb = Cmd_Argv( 0 );
	if ( !strcmp( verb, "disconnect" ) ) {
		Com_Error( ERR_SERVERDISCONNECT, "Server disconnected\n" );
		return false;
	}
	if ( !strcmp( verb, "cs" ) ) {
		const int index = atoi( Cmd_Argv( 1 ) );
		if ( index < 0 || index >= Q3TEST_CONFIGSTRINGS ) Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
		CL_Q3TestSetConfigstring( index, Cmd_Argv( 2 ) );
		return true;
	}
	if ( !strcmp( verb, "clientLevelShot" ) ) {
		if ( !com_sv_running->integer ) return false;
		Con_Close();
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		return false;
	}
	return true;
}

int ServerCommandSequence() { return clc.serverCommandSequence; }
int CurrentCmdNumber() { return cl.cmdNumber; }

/* CL_GetUserCmd: 1.05 answers a future command with 0, 1.06 and later drop
 * (0x401040); a command older than the ring is 0 in both. */
bool UserCmd( int number, void *out, std::size_t bytes ) {
	if ( number > cl.cmdNumber ) {
		if ( !Legacy() ) Com_Error( ERR_DROP, "CL_GetUserCmd: %i >= %i", number, cl.cmdNumber );
		return false;
	}
	if ( number <= cl.cmdNumber - CMD_BACKUP ) return false;
	const wire::Usercmd native = CL_Q3TestNativeCmd( number );
	std::memcpy( out, native.data(), std::min( bytes, wire::UsercmdBytes( Protocol() ) ) );
	return true;
}

void SetUserCmdValue( int weapon, float sensitivityScale ) {
	cl.cgameUserCmdValue = weapon;
	cl.cgameSensitivity = sensitivityScale;
}

} // namespace fntech3::q3test::client
