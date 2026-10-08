/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_Q3_IHV_NET_HPP
#define FNTECH3_SERVER_Q3_IHV_NET_HPP
#include "../compat/q3_ihv_netchan.hpp"
#include "../compat/q3_ihv_server_session.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

/* The IHV Test's clients, served by sessions of their own. A protocol-33
 * client speaks the February 1999 quake3.exe server's contract exactly:
 * Quake II's challenge and connect, its channel (q3_ihv_netchan.hpp), one
 * reliable message carrying the gamestate and every server command, and
 * datagrams that repeat each changed configstring ahead of the snapshot until
 * the client acknowledges its sequence. The native game's client exports are
 * called directly with the entity and command records it expects. The engine's
 * client_t slot mirrors a network client for the shared server commands
 * (status, kick, timeouts); the native client record (q3_ihv_server_clients.hpp)
 * holds what the snapshot builder and writer read, bots included. Addresses
 * are in docs/fntech3/Q3_IHV.md (protocol-33 server sessions). */
namespace {
namespace ihvsv = fntech3::q3ihv::server;

constexpr int IHV_PACKET_FRAMES = 32;
// SV_SendClientDatagram's message (0x41b555); bit writes stop four bytes short.
constexpr std::size_t IHV_DATAGRAM_BYTES = 1400;

struct IhvSession {
	fntech3::q3ihv::Netchan chan;
	// The channel's waiting reliable message, written the original's way.
	ihvsv::OriginalMessage message{ fntech3::q3ihv::FNTECH3_Q3_IHV_NETCHAN_BYTES };
	int configAck = 0;          // client +0x408
	int lastUsercmd = 0;        // +0x40c
	int deltaMessage = -1;      // +0x440
	bool rateDelayed = false;   // +0x450
	// Server-time stamps, as the original keeps them in sv.time.
	int lastPacketTime = 0;     // +0x444
	int lastConnectTime = 0;    // +0x448
	int nextSnapshotTime = 0;   // +0x44c
	std::array<int, IHV_PACKET_FRAMES> sent{}, acked{};  // frames +0x598, +0x59c
};

struct IhvChallenge {
	netadr_t adr{};
	int challenge = 0;
	int time = 0;
	bool used = false;
};

struct IhvNet {
	std::vector<std::unique_ptr<IhvSession>> sessions;
	std::array<IhvChallenge, 1024> challenges{};
	// SV_SetConfigstring 0x419655 stamps each change with the new sequence.
	std::array<int, 1024> csStamps{};
};
IhvNet ihvNet;

IhvSession *SV_IHVSession( int slot ) {
	if ( slot < 0 || slot >= static_cast<int>( ihvNet.sessions.size() ) ) return nullptr;
	return ihvNet.sessions[static_cast<std::size_t>( slot )].get();
}

IhvSession &SV_IHVNewSession( int slot ) {
	if ( slot < 0 || slot >= sv.maxclients ) Com_Error( ERR_DROP, "IHV session for a bad slot %i", slot );
	if ( ihvNet.sessions.size() < static_cast<std::size_t>( sv.maxclients ) )
		ihvNet.sessions.resize( static_cast<std::size_t>( sv.maxclients ) );
	auto &session = ihvNet.sessions[static_cast<std::size_t>( slot )];
	session = std::make_unique<IhvSession>();
	return *session;
}

void SV_IHVEndSession( int slot ) {
	if ( slot >= 0 && slot < static_cast<int>( ihvNet.sessions.size() ) )
		ihvNet.sessions[static_cast<std::size_t>( slot )].reset();
	ihvGameClientRecords.SetRemote( slot, false );
}

// The engine's slot and the native record move together; their numbering agrees.
void SV_IHVSetState( client_t *client, clientState_t state ) {
	client->state = state;
	ihvGameClientRecords.SetState( SV_ClientIndex( client ), static_cast<int>( state ) );
}

/* ---- the game's client exports ------------------------------------------ */

fntech3_q3_ihv_entity_t *SV_IHVSlotEntity( int slot ) {
	SV_IHVBindEntities();
	auto *entity = ihvGameEntities.At( slot + 1 );
	if ( !entity ) Com_Error( ERR_DROP, "IHV client %i has no entity", slot );
	return entity;
}

template<class Export> Export SV_IHVExport( Export exported, const char *name ) {
	if ( !ihvGame || !ihvGameInitialized || !exported ) Com_Error( ERR_DROP, "IHV game has no %s export", name );
	SV_IHVSyncCvars();
	return exported;
}

const char *SV_IHVGameClientConnect( int slot, char *userinfo ) {
	auto *entity = SV_IHVSlotEntity( slot );
	const char *denied = SV_IHVExport( ihvGame ? ihvGame->ClientConnect : nullptr, "ClientConnect" )( entity, userinfo );
	SV_IHVBindEntities();
	return denied;
}

void SV_IHVGameClientBegin( int slot, const fntech3::q3ihv::wire::Command &command ) {
	auto *entity = SV_IHVSlotEntity( slot );
	fntech3_q3_ihv_usercmd_t cmd;
	std::memcpy( cmd.bytes, command.data(), sizeof( cmd.bytes ) );
	SV_IHVExport( ihvGame ? ihvGame->ClientBegin : nullptr, "ClientBegin" )( entity, &cmd );
	SV_IHVBindEntities();
}

// SV_ClientThink 0x418c90: only an active client's entity thinks.
void SV_IHVGameClientThink( int slot, const fntech3::q3ihv::wire::Command &command ) {
	if ( ihvGameClientRecords.State( slot ) != fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_ACTIVE ) return;
	auto *entity = SV_IHVSlotEntity( slot );
	fntech3_q3_ihv_usercmd_t cmd;
	std::memcpy( cmd.bytes, command.data(), sizeof( cmd.bytes ) );
	SV_IHVExport( ihvGame ? ihvGame->ClientThink : nullptr, "ClientThink" )( entity, &cmd );
	SV_IHVBindEntities();
}

void SV_IHVGameClientAfterCommands( int slot, const fntech3::q3ihv::wire::Command &command ) {
	auto *entity = SV_IHVSlotEntity( slot );
	fntech3_q3_ihv_usercmd_t cmd;
	std::memcpy( cmd.bytes, command.data(), sizeof( cmd.bytes ) );
	SV_IHVExport( ihvGame ? ihvGame->ClientAfterCommands : nullptr, "ClientAfterCommands" )( entity, &cmd );
	SV_IHVBindEntities();
}

void SV_IHVGameClientDisconnect( int slot ) {
	auto *entity = SV_IHVSlotEntity( slot );
	SV_IHVExport( ihvGame ? ihvGame->ClientDisconnect : nullptr, "ClientDisconnect" )( entity );
	SV_IHVBindEntities();
}

/* ---- channel ---------------------------------------------------------------- */

bool SV_IHVSendDatagram( void *context, const unsigned char *data, std::size_t size ) {
	const auto *client = static_cast<const client_t *>( context );
	NET_SendPacket( NS_SERVER, static_cast<int>( size ), data, &client->netchan.remoteAddress );
	return true;
}

/* Netchan_Transmit 0x4127b0 promotes the waiting message only into an empty
 * reliable slot (Netchan_NeedReliable 0x412730), so it is handed over just
 * then and cleared, as the original clears its own. */
void SV_IHVTransmit( client_t *client, IhvSession &session, const void *data, std::size_t length ) {
	if ( !session.chan.Ready() ) return;
	if ( session.message.Size() && !session.chan.ReliableLength() && !session.chan.PendingLength() ) {
		session.chan.QueueReliable( session.message.Data(), session.message.Size() );
		session.message.Clear();
	}
	fntech3::q3ihv::NetchanSink sink{ client, SV_IHVSendDatagram };
	session.chan.Transmit( sink, static_cast<const unsigned char *>( data ), length );
	client->netchan.outgoingSequence = session.chan.OutgoingSequence();
	client->netchan.incomingSequence = session.chan.IncomingSequence();
	client->netchan.dropped = session.chan.Dropped();
	client->netchan.lastSentTime = Sys_Milliseconds();
	client->netchan.lastSentSize = static_cast<int>( length );
	ihvGameClientRecords.SetOutgoingSequence( SV_ClientIndex( client ), session.chan.OutgoingSequence() );
}

/* ---- server commands ------------------------------------------------------- */

/* SV_SendServerCommand 0x419e90 prefixes the text with the command byte 5 and
 * copies it, with its terminator, onto the reliable message: to one client
 * whatever its state, or to every client from primed on. A bot's slot has no
 * channel, and the original never sends a bot's message. */
void SV_IHVAddCommand( int slot, const char *text ) {
	IhvSession *session = SV_IHVSession( slot );
	if ( !session ) return;
	std::string command( 1, '\x05' );
	command.append( text, strnlen( text, MAX_STRING_CHARS ) );
	session->message.WriteData( command.c_str(), command.size() + 1 );
}

void SV_IHVBroadcastCommand( const char *text ) {
	// A dedicated server echoes broadcast prints, newlines escaped (0x419f29).
	if ( com_dedicated->integer && !strncmp( text, "print", 5 ) ) {
		std::string shown;
		for ( const char *c = text; *c; ++c ) {
			if ( *c == '\n' ) shown += "\\n";
			else shown += *c;
		}
		Com_Printf( "broadcast: %s\n", shown.c_str() );
	}
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		if ( slot.client.state >= CS_PRIMED ) SV_IHVAddCommand( slot.index, text );
	}
}

/* ---- client messages ------------------------------------------------------- */

// SV_UserinfoChanged 0x418ab0: name, rate and snapshot interval.
void SV_IHVUserinfoChanged( client_t *client, int slot ) {
	const char *stored = ihvGameClientRecords.Userinfo( slot );
	char name[MAX_INFO_VALUE], rate[MAX_INFO_VALUE], snaps[MAX_INFO_VALUE];
	Q_strncpyz( name, Info_ValueForKey( stored, "name" ), sizeof( name ) );
	Q_strncpyz( rate, Info_ValueForKey( stored, "rate" ), sizeof( rate ) );
	Q_strncpyz( snaps, Info_ValueForKey( stored, "snaps" ), sizeof( snaps ) );
	if ( !ihvGameClientRecords.ApplyUserinfo( slot, name, rate, snaps ) )
		Com_Error( ERR_DROP, "IHV client %i cannot record its userinfo", slot );
	Q_strncpyz( client->userinfo, stored, sizeof( client->userinfo ) );
	Q_strncpyz( client->name, ihvGameClientRecords.Name( slot ), sizeof( client->name ) );
	client->rate = ihvGameClientRecords.Rate( slot );
	client->snapshotMsec = ihvGameClientRecords.SnapshotMsec( slot );
}

/* SV_SendClientGameState 0x418750: every set configstring, every baseline
 * with a visible field, then a zero byte; the client is primed and is taken
 * to hold every configstring change so far. */
void SV_IHVSendGamestate( client_t *client, IhvSession &session ) {
	Com_DPrintf( "SV_SendGameState() for %s\n", client->name );
	SV_IHVSetState( client, CS_PRIMED );
	ihvsv::OriginalMessage &message = session.message;
	message.WriteByte( 2 );
	const int configstrings = std::min( 1024, Com_GameConfigstringLimit() );
	for ( int index = 0; index < configstrings; ++index ) {
		const char *text = sv.configstrings[index];
		if ( !text || !text[0] ) continue;
		message.WriteByte( 3 );
		message.WriteShort( index );
		message.WriteString( text );
	}
	const fntech3::q3ihv::wire::Entity empty{};
	for ( const auto &baseline : ihvGameState.baselines ) {
		if ( !baseline[2] && !baseline[0x95] && !baseline[0x94] && !baseline[4] && !baseline[5] ) continue;
		std::array<std::uint8_t, 512> bits{};
		fntech3::q3ihv::wire::Writer out( bits.data(), bits.size() );
		if ( !fntech3::q3ihv::wire::WriteEntity( out, empty, &baseline, true ) )
			Com_Error( ERR_DROP, "IHV baseline cannot be encoded" );
		message.WriteByte( 4 );
		message.AppendBits( bits.data(), out.Position() );
	}
	message.WriteByte( 0 );
	session.configAck = sv.configstringSequence;
}

/* SV_ClientEnterWorld 0x418840: the client's entity, numbered by its slot,
 * begins with the first command of the move that named the current level. */
void SV_IHVEnterWorld( client_t *client, IhvSession &session, const fntech3::q3ihv::wire::Command &command ) {
	const int slot = SV_ClientIndex( client );
	Com_DPrintf( "Begin() from %s\n", client->name );
	SV_IHVSetState( client, CS_ACTIVE );
	if ( !fntech3::q3ihv::SetEntityNumber( *SV_IHVSlotEntity( slot ), slot + 1 ) )
		Com_Error( ERR_DROP, "IHV client entity %i cannot hold its number", slot + 1 );
	session.deltaMessage = -1;
	session.lastUsercmd = 0;
	SV_IHVGameClientBegin( slot, command );
}

/* SV_UserMove 0x418cc0. A move naming another level runs nothing; the first
 * one naming this level enters the world. The commands before the provisional
 * last one run in order, a gap filled first, and the provisional one goes to
 * the game's after-commands export unless sv_nopredict is set. The entity's
 * state time then takes the server time (0x418ede). */
void SV_IHVUserMove( client_t *client, IhvSession &session, ihvsv::OriginalReader &input ) {
	const int slot = SV_ClientIndex( client );
	ihvsv::Move move;
	const ihvsv::MoveStatus status = ihvsv::ReadMove( input, move );
	session.deltaMessage = move.deltaMessage;
	if ( move.configAck != -1 ) session.configAck = move.configAck;
	if ( status == ihvsv::MoveStatus::CountLow ) {
		Com_Printf( "cmdCount < 2\n" );
		return;
	}
	if ( status == ihvsv::MoveStatus::CountHigh ) {
		Com_Printf( "cmdCount > MAX_PACKET_USERCMDS\n" );
		return;
	}
	if ( move.serverId != sv.serverId ) return;
	if ( client->state == CS_PRIMED ) SV_IHVEnterWorld( client, session, move.commands[0] );
	if ( client->state != CS_ACTIVE ) {
		session.deltaMessage = -1;
		return;
	}
	const ihvsv::MovePlan plan = ihvsv::PlanMove( session.lastUsercmd, move.lastCommand, move.count );
	if ( plan.lost && sv_showloss && sv_showloss->integer )
		Com_Printf( "Lost %i usercmds from %s\n", plan.lost, client->name );
	for ( int i = 0; i < plan.replays; ++i ) SV_IHVGameClientThink( slot, move.commands[0] );
	for ( int i = plan.begin; i < plan.end; ++i ) SV_IHVGameClientThink( slot, move.commands[static_cast<std::size_t>( i )] );
	session.lastUsercmd = move.lastCommand;
	if ( !Cvar_VariableIntegerValue( "sv_nopredict" ) )
		SV_IHVGameClientAfterCommands( slot, move.commands[static_cast<std::size_t>( move.count - 1 )] );
	const std::int32_t time = sv.time;
	std::memcpy( SV_IHVSlotEntity( slot )->state + 80, &time, sizeof( time ) );
}

// SV_UpdateUserinfo 0x418bb0: the game sees the new userinfo before the engine.
void SV_IHVUpdateUserinfo( client_t *client ) {
	const int slot = SV_ClientIndex( client );
	if ( !ihvGameClientRecords.CopyUserinfo( slot, Cmd_Argv( 1 ) ) )
		Com_Error( ERR_DROP, "IHV client %i cannot copy its userinfo", slot );
	auto *entity = SV_IHVSlotEntity( slot );
	SV_IHVExport( ihvGame ? ihvGame->ClientUserinfoChanged : nullptr, "ClientUserinfoChanged" )(
		entity, ihvGameClientRecords.MutableUserinfo( slot ) );
	SV_IHVBindEntities();
	SV_IHVUserinfoChanged( client, slot );
}

/* SV_ExecuteClientCommand 0x418bf0: the engine's userinfo, disconnect,
 * download and nextdl (table 0x4623a0), then the game's while the level runs.
 * fnTech3 serves no download: it answers one with the original's refusal, a
 * download message of length -1 (0x418a63). */
void SV_IHVClientCommand( client_t *client, IhvSession &session, const std::string &text ) {
	Cmd_TokenizeString( text.c_str() );
	const char *verb = Cmd_Argv( 0 );
	if ( !strcmp( verb, "userinfo" ) ) {
		SV_IHVUpdateUserinfo( client );
		return;
	}
	if ( !strcmp( verb, "disconnect" ) ) {
		SV_DropClient( client, "disconnected" );
		return;
	}
	if ( !strcmp( verb, "download" ) ) {
		session.message.WriteByte( 6 );
		session.message.WriteShort( -1 );
		session.message.WriteByte( 0 );
		return;
	}
	if ( !strcmp( verb, "nextdl" ) ) return;
	if ( sv.state == SS_GAME ) {
		auto *entity = SV_IHVSlotEntity( SV_ClientIndex( client ) );
		SV_IHVExport( ihvGame ? ihvGame->ClientCommand : nullptr, "ClientCommand" )( entity );
		SV_IHVBindEntities();
	}
}

/* SV_ExecuteClientMessage 0x418ef0: a connected client's first message brings
 * it the gamestate; then nops, moves and at most seven client commands, until
 * the message ends or a command leaves the client a zombie. */
void SV_IHVExecuteMessage( client_t *client, IhvSession &session, const void *data, std::size_t size ) {
	if ( client->state == CS_CONNECTED ) SV_IHVSendGamestate( client, session );
	ihvsv::OriginalReader input( data, size );
	int commands = 0;
	for ( ;; ) {
		const int c = input.ReadByte();
		if ( c == -1 ) return;
		if ( c == 1 ) continue;
		if ( c == 2 ) {
			SV_IHVUserMove( client, session, input );
			continue;
		}
		if ( c == 3 ) {
			const std::string text = input.ReadString();
			if ( ++commands <= ihvsv::ClientMessageCommands ) SV_IHVClientCommand( client, session, text );
			if ( client->state == CS_ZOMBIE || client->state == CS_FREE ) return;
			continue;
		}
		SV_DropClient( client, "had an unknown command char" );
		return;
	}
}

/* ---- snapshots ------------------------------------------------------------- */

/* SV_SendClientDatagram 0x41b520: build the client's snapshot, write the
 * configstrings changed since its acknowledged sequence (0x41af60) and the
 * snapshot into one 1400-byte message, send it and schedule the next. An
 * overflowed message goes out empty, with the original's warning. fnTech3
 * counts a message as overflowed once it passes 1396 bytes, where the
 * original's bit writes stop. */
void SV_IHVSendClientDatagram( client_t *client, IhvSession &session ) {
	const int slot = SV_ClientIndex( client );
	ihvGameClientRecords.SetDeltaMessage( slot, session.deltaMessage );
	ihvGameClientRecords.SetSequences( slot, session.configAck, session.lastUsercmd, session.rateDelayed ? 1 : 0 );
	SV_IHVBuildClientSnapshot( slot );
	static fntech3::q3ihv::wire::MessageWriter writer;
	writer.Clear();
	bool overflowed = false;
	if ( sv.configstringSequence > session.configAck ) {
		for ( int index = 0; index < static_cast<int>( ihvNet.csStamps.size() ) && !overflowed; ++index ) {
			if ( ihvNet.csStamps[static_cast<std::size_t>( index )] <= session.configAck ) continue;
			const char *text = sv.configstrings[index] ? sv.configstrings[index] : "";
			overflowed = !writer.AddConfigstring( static_cast<std::uint32_t>( index ), text )
				|| writer.Bytes() > IHV_DATAGRAM_BYTES - 4;
		}
	}
	if ( !overflowed ) {
		SV_IHVWriteSnapshotToClient( slot, writer );
		// The entity list ends with a ten-bit zero, the message's last bit write.
		overflowed = ( writer.Bits() - 10 + 7 ) / 8 > IHV_DATAGRAM_BYTES - 4;
	}
	const std::size_t bytes = overflowed ? 0 : writer.Bytes();
	if ( overflowed ) Com_Printf( "WARNING: msg overflowed for %s\n", client->name );
	const int frame = session.chan.OutgoingSequence() & ( IHV_PACKET_FRAMES - 1 );
	session.sent[static_cast<std::size_t>( frame )] = sv.time;
	SV_IHVTransmit( client, session, writer.Data(), bytes );
	const ihvsv::NextSnapshot next = ihvsv::ScheduleSnapshot( sv.time,
		client->netchan.remoteAddress.type == NA_LOOPBACK, session.lastPacketTime, static_cast<int>( bytes ),
		ihvGameClientRecords.Rate( slot ), ihvGameClientRecords.SnapshotMsec( slot ) );
	session.nextSnapshotTime = next.time;
	session.rateDelayed = next.rateDelayed;
}

/* ---- connectionless -------------------------------------------------------- */

/* SV_GetChallenge 0x4180e0: 1024 entries keyed by address alone, never
 * refreshed; an unknown address takes the oldest entry. Retail rand() is
 * MSVC's fifteen bits. */
void SV_IHVGetChallenge( const netadr_t *from ) {
	IhvChallenge *found = nullptr;
	IhvChallenge *oldest = &ihvNet.challenges[0];
	int oldestTime = 0x7fffffff;
	for ( IhvChallenge &entry : ihvNet.challenges ) {
		if ( entry.used && NET_CompareBaseAdr( from, &entry.adr ) ) {
			found = &entry;
			break;
		}
		if ( entry.time < oldestTime ) {
			oldestTime = entry.time;
			oldest = &entry;
		}
	}
	if ( !found ) {
		found = oldest;
		found->challenge = ( ( rand() & 0x7fff ) << 16 ) ^ ( rand() & 0x7fff );
		found->adr = *from;
		found->time = sv.time;
		found->used = true;
	}
	NET_OutOfBandPrint( NS_SERVER, from, "challenge %i", found->challenge );
}

/* SV_DirectConnect 0x4181d0: "connect 33 <qport> <challenge> <userinfo>". The
 * rcon password, when set, opens the private slots (0x418298). A client from
 * the same address and qport or port takes its own slot again after the
 * reconnect limit, with no disconnect. The game sees the new userinfo before
 * the slot is set up, and a refusal leaves the slot free. */
void SV_IHVDirectConnect( const netadr_t *from ) {
	Com_DPrintf( "SVC_DirectConnect ()\n" );
	const int version = atoi( Cmd_Argv( 1 ) );
	if ( version != 33 ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\nServer is version %4.2f.\n", 4.01 );
		Com_DPrintf( "    rejected connect from version %i\n", version );
		return;
	}
	const int qport = atoi( Cmd_Argv( 2 ) );
	const int challenge = atoi( Cmd_Argv( 3 ) );
	char userinfo[MAX_INFO_STRING];
	Q_strncpyz( userinfo, Cmd_Argv( 4 ), std::min<int>( sizeof( userinfo ), fntech3::q3ihv::FNTECH3_Q3_IHV_USERINFO_BYTES ) );
	const char *rcon = Cvar_VariableString( "rconpassword" );
	const bool privileged = rcon[0] && !strcmp( Info_ValueForKey( userinfo, "password" ), rcon );
	if ( !NET_IsLocalAddress( from ) ) {
		const IhvChallenge *entry = nullptr;
		for ( const IhvChallenge &candidate : ihvNet.challenges ) {
			if ( candidate.used && NET_CompareBaseAdr( from, &candidate.adr ) ) {
				entry = &candidate;
				break;
			}
		}
		if ( !entry ) {
			NET_OutOfBandPrint( NS_SERVER, from, "print\nNo challenge for address.\n" );
			return;
		}
		if ( entry->challenge != challenge ) {
			NET_OutOfBandPrint( NS_SERVER, from, "print\nBad challenge.\n" );
			return;
		}
		Info_SetValueForKey( userinfo, "ip", NET_AdrToString( from ) );
	} else {
		Info_SetValueForKey( userinfo, "ip", "localhost" );
	}

	int slot = -1;
	for ( SV_ClientSlot candidate : SV_ClientSlots() ) {
		IhvSession *session = SV_IHVSession( candidate.index );
		if ( candidate.client.state == CS_FREE || !session ) continue;
		const netadr_t &address = candidate.client.netchan.remoteAddress;
		if ( !NET_CompareBaseAdr( from, &address ) || ( candidate.client.netchan.qport != qport && address.port != from->port ) )
			continue;
		if ( sv.time - session->lastConnectTime < sv_reconnectlimit->integer * 1000 ) {
			Com_DPrintf( "%s:reconnect rejected : too soon\n", NET_AdrToString( from ) );
			return;
		}
		Com_Printf( "%s:reconnect\n", NET_AdrToString( from ) );
		slot = candidate.index;
		break;
	}
	if ( slot < 0 ) {
		const int start = privileged ? 0 : std::max( 0, Cvar_VariableIntegerValue( "privateclients" ) );
		for ( int index = start; index < sv.maxclients; ++index ) {
			if ( SV_ClientForIndex( index ).state == CS_FREE
				&& ihvGameClientRecords.State( index ) == fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_FREE ) {
				slot = index;
				break;
			}
		}
		if ( slot < 0 ) {
			NET_OutOfBandPrint( NS_SERVER, from, "print\nServer is full.\n" );
			Com_DPrintf( "Rejected a connection.\n" );
			return;
		}
	}

	client_t &client = SV_ClientForIndex( slot );
	SV_IHVEndSession( slot );
	ihvGameClientRecords.Free( slot );
	SV_FreeClient( &client );
	client.state = CS_FREE;
	client.name[0] = '\0';
	client.gentity = nullptr;
	client.ping = 0;
	client.timeoutCount = 0;
	client.justConnected = qfalse;
	const char *denied = SV_IHVGameClientConnect( slot, userinfo );
	if ( denied ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\n%s\n", denied );
		Com_DPrintf( "Game rejected a connection: %s.\n", denied );
		return;
	}
	if ( !ihvGameClientRecords.Connect( slot, sv.time ) || !ihvGameClientRecords.SetRemote( slot, true )
		|| !ihvGameClientRecords.CopyUserinfo( slot, userinfo ) )
		Com_Error( ERR_DROP, "IHV client %i cannot be admitted", slot );
	SV_IHVUserinfoChanged( &client, slot );
	NET_OutOfBandPrint( NS_SERVER, from, "client_connect" );
	Netchan_Setup( NS_SERVER, &client.netchan, from, qport, challenge, NETCHAN_WIRE_Q3_IHV );
	IhvSession &session = SV_IHVNewSession( slot );
	if ( !session.chan.Setup( fntech3::q3ihv::NetchanSource::Server, qport ) )
		Com_Error( ERR_DROP, "IHV client %i has no channel", slot );
	ihvGameClientRecords.SetRemote( slot, true );
	ihvGameClientRecords.SetOutgoingSequence( slot, session.chan.OutgoingSequence() );
	SV_IHVSetState( &client, CS_CONNECTED );
	session.nextSnapshotTime = session.lastPacketTime = session.lastConnectTime = sv.time;
	client.lastPacketTime = client.lastConnectTime = svs.time;
}

int SV_IHVConnectedClients() {
	int count = 0;
	for ( int slot = 0; slot < ihvGameClientRecords.Count(); ++slot )
		if ( ihvGameClientRecords.State( slot ) >= fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_CONNECTED ) ++count;
	return count;
}

// A client's score, the signed word at 0x84 of its player state (0x41a170).
int SV_IHVScore( int slot ) {
	fntech3_q3_ihv_playerState_t player{};
	if ( !serverGameData.ReadPlayerState( slot, player ) ) return 0;
	std::int16_t score = 0;
	std::memcpy( &score, player.bytes + 0x84, sizeof( score ) );
	return score;
}

void SV_IHVConnectionless( const netadr_t *from, msg_t *msg ) {
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );
	const char *line = MSG_ReadStringLine( msg );
	char text[MAX_STRING_CHARS];
	Q_strncpyz( text, line, sizeof( text ) );
	Cmd_TokenizeString( text );
	const char *verb = Cmd_Argv( 0 );
	Com_DPrintf( "SV packet %s : %s\n", NET_AdrToString( from ), verb );
	if ( !strcmp( verb, "getstatus" ) ) {
		std::vector<ihvsv::StatusPlayer> players;
		for ( int slot = 0; slot < ihvGameClientRecords.Count() && slot < sv.maxclients; ++slot ) {
			if ( ihvGameClientRecords.State( slot ) < fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_CONNECTED ) continue;
			const IhvSession *session = SV_IHVSession( slot );
			players.push_back( { SV_IHVScore( slot ), session ? SV_ClientForIndex( slot ).ping : 0,
				ihvGameClientRecords.Name( slot ) } );
		}
		const std::string reply = ihvsv::StatusReply( Cmd_Argv( 1 ), SV_IHVServerinfo(), players );
		NET_OutOfBandPrint( NS_SERVER, from, "%s", reply.c_str() );
	} else if ( !strcmp( verb, "getinfo" ) ) {
		const std::string reply = ihvsv::InfoReply( Cvar_VariableString( "hostname" ), sv_mapname->string,
			sv_maxclients->integer, SV_IHVConnectedClients() );
		NET_OutOfBandPrint( NS_SERVER, from, "%s", reply.c_str() );
	} else if ( !strcmp( verb, "getchallenge" ) ) {
		SV_IHVGetChallenge( from );
	} else if ( !strcmp( verb, "connect" ) ) {
		SV_IHVDirectConnect( from );
	} else if ( !strcmp( verb, "rcon" ) ) {
		SV_RemoteCommandPacket( from );
	} else if ( strcmp( verb, "disconnect" ) ) {
		Com_Printf( "bad connectionless packet from %s:\n%s\n", NET_AdrToString( from ), text );
	}
}

/* A slot the engine freed (a timeout, or a zombie's time passing) gives up its
 * channel and native record; a released bot's zombie record is reclaimed the
 * same way once sv_zombietime passes (SV_CheckTimeouts 0x41aa63). */
void SV_IHVReclaimSlots() {
	const int zombiepoint = sv.time - 1000 * ( sv_zombietime ? sv_zombietime->integer : 2 );
	for ( int slot = 0; slot < ihvGameClientRecords.Count(); ++slot ) {
		const int state = ihvGameClientRecords.State( slot );
		if ( state == fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_FREE ) continue;
		if ( ihvGameClientRecords.Remote( slot ) ) {
			if ( slot < sv.maxclients && SV_ClientForIndex( slot ).state == CS_FREE ) {
				SV_IHVEndSession( slot );
				ihvGameClientRecords.Free( slot );
			}
		} else if ( state == fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_ZOMBIE
			&& ihvGameClientRecords.LastPacketTime( slot ) < zombiepoint ) {
			ihvGameClientRecords.Free( slot );
		}
	}
}

} // namespace

/* ---- engine hooks ---------------------------------------------------------- */

/* SV_PacketEvent 0x41a790: a client is found by base address and the qport in
 * bytes 8 and 9; a changed UDP port is adopted; every accepted packet restamps
 * the frame it acknowledges; a sequenced packet from nobody is answered with a
 * disconnect. */
void SV_IHVNetPacketEvent( const netadr_t *from, msg_t *msg ) {
	if ( msg->cursize >= 4 ) {
		std::int32_t marker;
		std::memcpy( &marker, msg->data, sizeof( marker ) );
		if ( marker == -1 ) {
			if ( com_sv_running->integer ) SV_IHVConnectionless( from, msg );
			return;
		}
	}
	if ( sv.state == SS_DEAD || msg->cursize < 10 ) return;
	const int qport = msg->data[8] | ( msg->data[9] << 8 );
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		client_t &client = slot.client;
		IhvSession *session = SV_IHVSession( slot.index );
		if ( client.state == CS_FREE || !session || !NET_CompareBaseAdr( from, &client.netchan.remoteAddress )
			|| client.netchan.qport != qport ) continue;
		if ( client.netchan.remoteAddress.port != from->port ) {
			Com_Printf( "SV_ReadPackets: fixing up a translated port\n" );
			client.netchan.remoteAddress.port = from->port;
		}
		std::size_t size = static_cast<std::size_t>( msg->cursize );
		const auto result = session->chan.Process( msg->data, size, static_cast<std::size_t>( msg->maxsize ) );
		client.netchan.incomingSequence = session->chan.IncomingSequence();
		client.netchan.dropped = session->chan.Dropped();
		if ( result != fntech3::q3ihv::NetchanResult::Message || client.state == CS_ZOMBIE ) return;
		session->lastPacketTime = sv.time;
		client.lastPacketTime = svs.time;
		session->acked[static_cast<std::size_t>( session->chan.IncomingAcknowledged() & ( IHV_PACKET_FRAMES - 1 ) )] = sv.time;
		SV_IHVExecuteMessage( &client, *session, msg->data, size );
		return;
	}
	NET_OutOfBandPrint( NS_SERVER, from, "disconnect" );
}

/* SV_SendClientMessages 0x41b6a0: an overflowed reliable message drops its
 * client; an active client gets a datagram when its time comes; every other
 * slot, zombies included, gets an empty message whenever reliable data waits
 * or once a second. */
void SV_IHVNetSendClientMessages( void ) {
	SV_IHVReclaimSlots();
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		client_t &client = slot.client;
		IhvSession *session = SV_IHVSession( slot.index );
		if ( client.state == CS_FREE || !session ) continue;
		if ( session->message.Overflowed() ) {
			session->message.Clear();
			SV_DropClient( &client, "overflowed" );
			continue;
		}
		if ( client.state != CS_ACTIVE ) {
			if ( session->message.Size() || sv.time >= session->nextSnapshotTime ) {
				SV_IHVTransmit( &client, *session, nullptr, 0 );
				session->nextSnapshotTime = sv.time + 1000;
			}
			continue;
		}
		if ( sv.time >= session->nextSnapshotTime ) SV_IHVSendClientDatagram( &client, *session );
	}
}

void SV_IHVNetServerCommand( client_t *client, const char *text ) {
	if ( client ) SV_IHVAddCommand( SV_ClientIndex( client ), text );
	else SV_IHVBroadcastCommand( text );
}

void SV_IHVNetConfigstringChanged( int index ) {
	if ( index >= 0 && index < static_cast<int>( ihvNet.csStamps.size() ) )
		ihvNet.csStamps[static_cast<std::size_t>( index )] = sv.configstringSequence;
}

// SV_SpawnServer clears the configstring sequence and its stamps with the level.
void SV_IHVNetSpawnReset( void ) {
	ihvNet.csStamps.fill( 0 );
}

/* SV_SpawnServer 0x419966: every slot's times start over, and every client
 * from connected on, bots included, is offered to the game again. A refused
 * one is dropped; the rest are connected and take the new gamestate with their
 * next message, or, for a bot, begin again from the scheduler. */
void SV_IHVNetReconnectClients( void ) {
	for ( int slot = 0; slot < ihvGameClientRecords.Count(); ++slot ) {
		IhvSession *session = SV_IHVSession( slot );
		if ( session ) session->lastPacketTime = session->lastConnectTime = session->nextSnapshotTime = 0;
		if ( ihvGameClientRecords.State( slot ) < fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_CONNECTED ) continue;
		const char *denied = SV_IHVGameClientConnect( slot, ihvGameClientRecords.MutableUserinfo( slot ) );
		client_t *client = slot < sv.maxclients ? &SV_ClientForIndex( slot ) : nullptr;
		if ( denied ) {
			if ( client ) SV_DropClient( client, denied );
			continue;
		}
		ihvGameClientRecords.SetState( slot, fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_CONNECTED );
		if ( client && session ) {
			client->state = CS_CONNECTED;
			client->lastPacketTime = svs.time;
		}
	}
}

/* SV_DropClient 0x4186e0: everyone primed hears why, the game lets the entity
 * go, the client is told to disconnect and its slot waits as a zombie. A bot's
 * slot has no channel. fnTech3 does not drop a slot twice. */
void SV_IHVNetDropClient( client_t *drop, const char *reason ) {
	const int slot = SV_ClientIndex( drop );
	const int state = ihvGameClientRecords.State( slot );
	if ( state <= fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_ZOMBIE ) return;
	char name[MAX_NAME_LENGTH];
	Q_strncpyz( name, ihvGameClientRecords.Name( slot ), sizeof( name ) );
	if ( reason ) SV_IHVBroadcastCommand( va( "print \"%s %s\n\"", name, reason ) );
	SV_IHVGameClientDisconnect( slot );
	SV_IHVAddCommand( slot, "disconnect" );
	ihvGameClientRecords.Release( slot );
	if ( SV_IHVSession( slot ) ) {
		drop->name[0] = '\0';
		drop->state = CS_ZOMBIE;
	}
}

/* SV_FinalMessage 0x419cd0: one message with the print and the disconnect,
 * sent twice to every connected client, unreliable and immediately. */
void SV_IHVNetFinalMessage( const char *message ) {
	ihvsv::OriginalMessage closing( IHV_DATAGRAM_BYTES );
	closing.WriteByte( 5 );
	closing.WriteString( va( "print \"%s\"", message ) );
	closing.WriteByte( 5 );
	closing.WriteString( "disconnect" );
	for ( int pass = 0; pass < 2; ++pass ) {
		for ( SV_ClientSlot slot : SV_ClientSlots() ) {
			IhvSession *session = SV_IHVSession( slot.index );
			if ( slot.client.state >= CS_CONNECTED && session )
				SV_IHVTransmit( &slot.client, *session, closing.Data(), closing.Size() );
		}
	}
}

void SV_IHVNetClearSessions( void ) {
	ihvNet.sessions.clear();
	ihvNet.challenges = {};
	ihvNet.csStamps.fill( 0 );
}

/* SV_CalcPings 0x41a910, before the snapshots: an active network client's
 * mean round trip goes to the client and its player state's ping word. */
void SV_IHVNetCalcPings( void ) {
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		IhvSession *session = SV_IHVSession( slot.index );
		if ( slot.client.state != CS_ACTIVE || !session ) continue;
		slot.client.ping = ihvsv::Ping( session->sent.data(), session->acked.data(), IHV_PACKET_FRAMES );
		SV_SetGameClientPing( slot.index, slot.client.ping );
	}
}

int SV_IHVNetClientScore( int slot ) {
	return SV_IHVScore( slot );
}

/* The original's own server cvars that this engine names differently:
 * hostname, privateclients and rconpassword (SV_Init 0x419a40), and
 * sv_nopredict. The first two are serverinfo there (flags 9 and 8). */
void SV_IHVNetRegisterCvars( void ) {
	Cvar_Get( "hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE );
	Cvar_Get( "privateclients", "0", CVAR_SERVERINFO );
	Cvar_Get( "rconpassword", "", CVAR_TEMP );
	Cvar_Get( "sv_nopredict", "0", 0 );
}

/* The original's serverinfo: what its SV_Init (0x419a40) registers with the
 * serverinfo bit (8) -- timelimit, cheats, protocol, mapname, privateclients,
 * hostname and sv_maxclients -- with the version, and the game's serverinfo
 * cvars. This engine's own serverinfo cvars (sv_hostname, sv_privateClients,
 * sv_allowDownload, sv_dlRate, sv_floodProtect, sv_minRate, sv_maxRate,
 * sv_playingDemo, g_gametype) have no IHV counterpart, and the original keeps
 * its download switch out (0x419b66). Keys come in the order the original's
 * cvar list gives them with the build's game; the version is this engine's. */
const char *SV_IHVServerinfo( void ) {
	static char info[MAX_INFO_STRING];
	static const char *const engine[] = { "sv_maxclients", "privateclients", "mapname", "protocol",
		"timelimit", "version", "hostname", "cheats" };
	static const char *const order[] = { "teamflags", "maxclients", "gamedate", "gamename",
		"sv_maxclients", "privateclients", "mapname", "protocol", "timelimit", "fraglimit", "dmflags",
		"version", "hostname", "cheats" };
	const auto game = []( const char *key ) {
		for ( const IhvGameCvar *node = ihvGameCvars; node; node = node->next )
			if ( ( fntech3::q3ihv::CvarFlagsToEngine( node->flags ) & CVAR_SERVERINFO ) && !Q_stricmp( node->name, key ) )
				return true;
		return false;
	};
	const auto published = [&game]( const char *key ) {
		for ( const char *name : engine ) if ( !Q_stricmp( name, key ) ) return true;
		return game( key );
	};
	info[0] = '\0';
	for ( const char *key : order )
		if ( published( key ) ) Info_SetValueForKey( info, key, Cvar_VariableString( key ) );
	for ( const IhvGameCvar *node = ihvGameCvars; node; node = node->next )
		if ( game( node->name ) && !Info_ValueForKey( info, node->name )[0] )
			Info_SetValueForKey( info, node->name, Cvar_VariableString( node->name ) );
	return info;
}

const char *SV_IHVNetRconPassword( void ) {
	return Cvar_VariableString( "rconpassword" );
}

#endif
