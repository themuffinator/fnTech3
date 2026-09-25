/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_Q3TEST_NET_HPP
#define FNTECH3_SERVER_Q3TEST_NET_HPP
#include "../compat/q3test_netchan.hpp"
#include "../qcommon/q3test_wire.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

/* The Q3Test releases' clients, served by their own sessions. Retail Q3Test
 * clients speak a release's protocol exactly: connectionless verbs and the
 * connect packet of that release, one of two netchans, and message bodies with
 * no Huffman stage (q3test_wire.hpp). 1.02 to 1.05 (protocols 35 and 36) carry
 * the gamestate and every server command in the channel's one reliable block
 * and repeat changed configstrings in each snapshot until acknowledged; 1.06
 * to 1.08 (38 to 40) keep Quake III's command rings, acknowledged in the
 * message bodies, and send the gamestate as its own message when a move names
 * a stale server id. Everything here follows the retail servers; the facts and
 * addresses are in docs/fntech3/Q3TEST.md. The game module, the world and the
 * shared client slots are the engine's; the session owns only what the wire
 * needs besides. */
namespace {
namespace q3wire = fntech3::q3test::wire;
using fntech3::q3test::FNTECH3_Q3TEST_PACKET_BYTES;

constexpr int Q3TEST_PACKET_BACKUP = 32;
constexpr int Q3TEST_COMMAND_RING = 64;
// SV_AddEntToSnapshot stops at 1024 entities (1.02 0x41d4e0).
constexpr int Q3TEST_SNAPSHOT_ENTITIES = 1024;
// A client runs at most seven commands a message (35/36) or a server frame (38-40).
constexpr int Q3TEST_COMMANDS_PER_BATCH = 7;

struct Q3TestFrame {
	int messageNum = -1;
	q3wire::Snapshot snap;
};

/* 35/36's pending reliable message. Retail writes the gamestate and every
 * server command into one msg_t, and its client reads the reliable block and
 * what follows as one bit stream, so they are written here by one continuous
 * writer too, and handed to the channel only when its reliable slot is free,
 * as Netchan_NeedReliable would promote them (1.02 0x414a90). */
struct Q3TestPending {
	std::vector<std::uint8_t> data;
	q3wire::Writer out;
	void Reset() {
		data.assign( fntech3::q3ihv::FNTECH3_Q3_IHV_NETCHAN_BYTES, 0 );
		out = q3wire::Writer( data.data(), data.size() );
	}
};

struct Q3TestSession {
	fntech3::q3test::Netchan chan;
	Q3TestPending pending;
	std::array<Q3TestFrame, Q3TEST_PACKET_BACKUP> frames;
	int csAck = 0;              // 35/36: the configstring sequence the client holds
	int lastUsercmd = 0;        // the last command number run
	bool lostCommands = false;  // snapFlags bit 1 (36-40)
	bool rateDelayed = false;
	bool overflowed = false;    // 35/36: the pending reliable message filled up
	int nextSnapshotTime = 0;
	int commandsThisFrame = 0;  // 38-40: client commands run this server frame
	int commandFrame = -1;
};

struct Q3TestChallenge {
	netadr_t adr{};
	int challenge = 0;
	int time = 0;
	bool used = false;
};

struct Q3TestNet {
	std::vector<std::unique_ptr<Q3TestSession>> sessions;
	std::unique_ptr<q3wire::Baselines> baselines = std::make_unique<q3wire::Baselines>();
	std::array<int, 1024> csStamps{};
	int csSequence = 0;         // 35/36: bumped by every configstring change
	std::array<Q3TestChallenge, 1024> challenges{};
	int frameCount = 0;
	int snapshotCounter = 0;
};
Q3TestNet q3net;

const q3testRelease_t &SV_Q3TestNetRelease();

} // namespace

/* The remote-console password: rcon_password in 1.02, rconPassword from 1.03
 * (SVC_RemoteCommand, 1.02 0x41c880, 1.06 0x41d950). */
const char *SV_Q3TestRconPassword( void ) {
	const q3testRelease_t *release = Com_Q3TestRelease();
	return Cvar_VariableString( release && release->id == Q3TEST_RELEASE_1_02 ? "rcon_password" : "rconPassword" );
}

namespace {

const q3testRelease_t &SV_Q3TestNetRelease() {
	const q3testRelease_t *release = Com_Q3TestRelease();
	if ( !release ) Com_Error( ERR_DROP, "Q3Test has no pinned release" );
	return *release;
}
int SV_Q3TestNetProtocol() { return SV_Q3TestNetRelease().protocol; }
bool SV_Q3TestLegacyWire() { return q3wire::IsLegacy( SV_Q3TestNetProtocol() ); }

Q3TestSession &SV_Q3TestSessionFor( const client_t *client ) {
	const int index = SV_ClientIndex( client );
	if ( index < 0 || index >= sv.maxclients ) Com_Error( ERR_DROP, "Q3Test session for a bad client" );
	if ( q3net.sessions.size() < static_cast<std::size_t>( sv.maxclients ) ) q3net.sessions.resize( static_cast<std::size_t>( sv.maxclients ) );
	auto &session = q3net.sessions[static_cast<std::size_t>( index )];
	if ( !session ) {
		session = std::make_unique<Q3TestSession>();
		session->pending.Reset();
	}
	return *session;
}

/* ---- channel --------------------------------------------------------------- */

bool SV_Q3TestSendDatagram( void *context, const unsigned char *data, std::size_t size ) {
	const auto *client = static_cast<const client_t *>( context );
	if ( Cvar_VariableIntegerValue( "q3test_nettrace" ) ) {
		std::uint32_t first = 0, second = 0;
		std::memcpy( &first, data, 4 ); std::memcpy( &second, data + 4, 4 );
		Com_Printf( "Q3TNET send %i bytes seq %u%s ack %u to %s state %i\n", static_cast<int>( size ), first & 0x7fffffffu,
			first & 0x80000000u ? " (flag)" : "", second, NET_AdrToString( &client->netchan.remoteAddress ), client->state );
	}
	NET_SendPacket( NS_SERVER, static_cast<int>( size ), data, &client->netchan.remoteAddress );
	return true;
}

// Shared engine code reads the channel's sequences from netchan_t.
void SV_Q3TestMirrorChannel( client_t *client, const Q3TestSession &session ) {
	client->netchan.incomingSequence = session.chan.IncomingSequence();
	client->netchan.outgoingSequence = session.chan.OutgoingSequence();
	client->netchan.dropped = session.chan.Dropped();
}

void SV_Q3TestTransmit( client_t *client, const void *data, std::size_t length ) {
	auto &session = SV_Q3TestSessionFor( client );
	if ( client->netchan.remoteAddress.type == NA_BOT || !session.chan.Ready() ) return;
	if ( session.chan.Legacy() && session.pending.out.Size() && !session.chan.ReliableInFlight()
		&& !session.chan.PendingReliable() ) {
		if ( !session.pending.out.Good() || !session.chan.QueueReliable( session.pending.data.data(), session.pending.out.Size() ) )
			session.overflowed = true;
		session.pending.Reset();
	}
	fntech3::q3test::NetchanSink sink{ client, SV_Q3TestSendDatagram };
	session.chan.Transmit( sink, static_cast<const unsigned char *>( data ), length );
	client->netchan.lastSentTime = Sys_Milliseconds();
	client->netchan.lastSentSize = static_cast<int>( length );
	SV_Q3TestMirrorChannel( client, session );
}

/* ---- server commands and configstrings ------------------------------------ */

/* SV_SendServerCommand stores 0x05 in front of the text in every release
 * (1.02 0x41c372, 1.06 0x41d431): 35/36 append that whole buffer to the
 * reliable message, the 5 being the opcode; 38-40 keep it as the command,
 * so the string on the wire starts with 0x05. */
void SV_Q3TestAddCommand( client_t *client, const char *text ) {
	auto &session = SV_Q3TestSessionFor( client );
	if ( client->netchan.remoteAddress.type == NA_BOT ) return;
	std::string command( 1, '\x05' );
	command.append( text, strnlen( text, q3wire::MaxStringChars - 1 ) );
	if ( SV_Q3TestLegacyWire() ) {
		/* The opcode continues the stream and the string aligns, as a retail
		 * client reads them; retail's one aligned write of 0x05 and the text
		 * is the same bytes whenever the message is aligned (1.05 0x41c777). */
		if ( !q3wire::WriteServerCommand( session.pending.out, SV_Q3TestNetProtocol(), 0, command.c_str() + 1 ) )
			session.overflowed = true;
		return;
	}
	client->reliableSequence++;
	// 1.06 SV_AddServerCommand 0x41d3b6
	if ( client->reliableSequence - client->reliableAcknowledge > Q3TEST_COMMAND_RING ) {
		Com_Printf( "===== pending server commands =====\n" );
		client->reliableAcknowledge = client->reliableSequence;
		SV_DropClient( client, "Server command overflow" );
		return;
	}
	Q_strncpyz( client->reliableCommands[client->reliableSequence & ( Q3TEST_COMMAND_RING - 1 )], command.c_str(),
		MAX_STRING_CHARS );
}

void SV_Q3TestBroadcastCommand( const char *text ) {
	// A dedicated server echoes broadcast prints (1.02 0x41c3c5, 1.06 0x41d46d).
	if ( com_dedicated->integer && !strncmp( text, "print", 5 ) ) {
		Com_Printf( "broadcast: %s\n", text );
	}
	for ( client_t &client : SV_Clients() ) {
		if ( client.state >= CS_PRIMED ) SV_Q3TestAddCommand( &client, text );
	}
}

/* ---- snapshots ------------------------------------------------------------- */

const fntech3::q3test::state::PlayerLayout &SV_Q3TestPlayerLayout() {
	const auto *layout = fntech3::q3test::state::PlayerLayoutForProtocol( SV_Q3TestNetProtocol() );
	if ( !layout ) Com_Error( ERR_DROP, "Q3Test has no player layout" );
	return *layout;
}

const q3tEnt *SV_Q3TestNativeEntity( int number ) {
	void *base; int stride, count, capacity;
	SV_Q3TestEntityArray( base, stride, count, capacity );
	if ( !base || number < 0 || number >= count ) return nullptr;
	return reinterpret_cast<const q3tEnt *>( static_cast<const byte *>( base ) + static_cast<std::size_t>( number ) * stride );
}

void SV_Q3TestAddEntity( int number, svEntity_t &svEnt, std::vector<int> &numbers ) {
	if ( svEnt.snapshotCounter == q3net.snapshotCounter ) return;
	svEnt.snapshotCounter = q3net.snapshotCounter;
	if ( static_cast<int>( numbers.size() ) != Q3TEST_SNAPSHOT_ENTITIES ) numbers.push_back( number );
}

/* SV_AddEntitiesVisibleFromPoint (1.02 0x41d520, 1.06 0x41e5d0): entities in
 * use and linked, not NOCLIENT and not yet added; BROADCAST ones always, the
 * rest when their area connects with the viewer's and a cluster -- or, for an
 * entity in more clusters than it records, its head node -- is in the PVS; a
 * PORTAL recurses from its origin2. 1.02-1.05 start at entity 1. */
void SV_Q3TestAddVisible( const vec3_t origin, q3wire::Snapshot &snap, std::vector<int> &numbers, bool portal ) {
	const int leafnum = CM_PointLeafnum( origin );
	const int clientarea = CM_LeafArea( leafnum );
	const int clientcluster = CM_LeafCluster( leafnum );
	if ( !portal ) {
		byte areabits[MAX_MAP_AREA_BYTES] = {};
		const int bytes = CM_WriteAreaBits( areabits, clientarea );
		snap.areamask.assign( areabits, areabits + std::clamp( bytes, 0, static_cast<int>( q3wire::MaxAreaBytes ) ) );
	}
	const byte *pvs = CM_ClusterPVS( clientcluster );
	void *base; int stride, count, capacity;
	SV_Q3TestEntityArray( base, stride, count, capacity );
	for ( int e = SV_Q3TestNumberedClients() ? 0 : 1; e < count && e < MAX_GENTITIES; ++e ) {
		auto *ent = reinterpret_cast<q3tEnt *>( static_cast<byte *>( base ) + static_cast<std::size_t>( e ) * stride );
		if ( !ent->inuse ) continue;
		if ( ent->s.number != e ) {
			Com_DPrintf( "FIXING ENT->S.NUMBER!!!\n" );
			ent->s.number = e;
		}
		if ( !ent->linked || ( ent->svFlags & FNTECH3_Q3TEST_SVF_NOCLIENT ) ) continue;
		svEntity_t &svEnt = sv.svEntities[e];
		if ( svEnt.snapshotCounter == q3net.snapshotCounter ) continue;
		if ( ent->svFlags & FNTECH3_Q3TEST_SVF_BROADCAST ) {
			SV_Q3TestAddEntity( e, svEnt, numbers );
			continue;
		}
		if ( !CM_AreasConnected( clientarea, svEnt.areanum ) && !CM_AreasConnected( clientarea, svEnt.areanum2 ) ) continue;
		bool visible = false;
		for ( int i = 0; i < svEnt.numClusters && !visible; ++i ) {
			const int cluster = svEnt.clusternums[i];
			visible = pvs && ( pvs[cluster >> 3] & ( 1 << ( cluster & 7 ) ) );
		}
		if ( !visible && svEnt.lastCluster ) {
			/* An entity in more clusters than it records: retail tests the
			 * node that splits its box, Quake II's head node. */
			int leafs[64], topnode = 0;
			vec3_t mins, maxs;
			VectorCopy( ent->absmin, mins );
			VectorCopy( ent->absmax, maxs );
			CM_BoxLeafnumsTopnode( mins, maxs, leafs, ARRAY_LEN( leafs ), &topnode );
			visible = pvs && CM_HeadnodeVisible( topnode, pvs );
		}
		if ( !visible ) continue;
		SV_Q3TestAddEntity( e, svEnt, numbers );
		if ( ent->svFlags & FNTECH3_Q3TEST_SVF_PORTAL ) SV_Q3TestAddVisible( ent->s.origin2, snap, numbers, true );
	}
}

/* SV_BuildClientSnapshot (1.02 0x41d6f0, 1.06 0x41e7a0): the viewer is marked
 * sent, its player state copied whole, the walk runs from the view point and
 * the numbers are sorted; 38-40 send the area mask inverted, and a 38-40
 * client without an entity gets an empty frame. */
void SV_Q3TestBuildSnapshot( client_t *client, Q3TestFrame &frame ) {
	const int protocol = SV_Q3TestNetProtocol();
	const int index = SV_ClientIndex( client );
	frame.snap = q3wire::Snapshot{};
	++q3net.snapshotCounter;
	void *entity = SV_Q3TestEntityForClient( index );
	if ( !entity || ( SV_Q3TestNumberedClients() && !client->gentity ) ) return;
	if ( !serverGameData.ReadQ3TestPlayerState( index, frame.snap.player.data(), q3wire::PlayerBytes( protocol ) ) ) return;
	const auto &layout = SV_Q3TestPlayerLayout();
	int viewer = SV_Q3TestClientEntity( index );
	if ( SV_Q3TestNumberedClients() ) {
		viewer = static_cast<int>( fntech3::q3test::state::LoadInt( frame.snap.player.data() + layout.clientNum ) );
		if ( viewer < 0 || viewer > 1023 ) Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	sv.svEntities[viewer].snapshotCounter = q3net.snapshotCounter;
	vec3_t origin;
	for ( int i = 0; i < 3; ++i ) origin[i] = fntech3::q3test::state::LoadFloat( frame.snap.player.data() + layout.origin + 4 * i );
	origin[2] += static_cast<float>( fntech3::q3test::state::LoadInt( frame.snap.player.data() + layout.viewheight ) );
	std::vector<int> numbers;
	numbers.reserve( 256 );
	SV_Q3TestAddVisible( origin, frame.snap, numbers, false );
	std::sort( numbers.begin(), numbers.end() );
	if ( !q3wire::IsLegacy( protocol ) ) {
		for ( auto &byteValue : frame.snap.areamask ) byteValue = static_cast<std::uint8_t>( ~byteValue );
	}
	frame.snap.entities.reserve( numbers.size() );
	for ( int number : numbers ) {
		const q3tEnt *ent = SV_Q3TestNativeEntity( number );
		if ( !ent || !q3wire::ValidEntityNumber( protocol, number ) ) continue;
		q3wire::Entity state{};
		std::memcpy( state.data(), &ent->s, q3wire::EntityBytes );
		frame.snap.entities.push_back( state );
	}
}

/* The frame a move's deltaMessage names, or none: out of the ring, not the
 * frame sent under that number, or not active (1.05 0x41d700). A number not
 * yet sent -- the frame being built carries the next one -- names none:
 * retail would delta from that frame and tell the client it had none. */
const Q3TestFrame *SV_Q3TestDeltaFrame( const client_t *client, const Q3TestSession &session ) {
	if ( client->state != CS_ACTIVE || client->deltaMessage <= 0 ) return nullptr;
	const int outgoing = session.chan.OutgoingSequence();
	if ( client->deltaMessage >= outgoing ) return nullptr;
	if ( outgoing - client->deltaMessage >= Q3TEST_PACKET_BACKUP - 3 ) {
		Com_DPrintf( "%s: Delta request from out of date packet.\n", client->name );
		return nullptr;
	}
	const Q3TestFrame &old = session.frames[client->deltaMessage & ( Q3TEST_PACKET_BACKUP - 1 )];
	return old.messageNum == client->deltaMessage ? &old : nullptr;
}

/* The next message time (1.02 0x41da00, 1.06 0x41ea80): a loopback client
 * every frame; otherwise the rate or the snapshot interval, whichever is
 * longer, and at least a second while the client is silent or not active. */
void SV_Q3TestScheduleNext( client_t *client, Q3TestSession &session, std::size_t messageBytes ) {
	if ( client->netchan.remoteAddress.type == NA_LOOPBACK || Sys_IsLANAddress( &client->netchan.remoteAddress ) && !client->rate ) {
		session.nextSnapshotTime = svs.time - 1;
		return;
	}
	const bool silent = svs.time - client->lastPacketTime > 1000;
	if ( SV_Q3TestLegacyWire() && silent ) {
		session.nextSnapshotTime = svs.time + 1000;
		return;
	}
	const int rate = client->rate > 0 ? client->rate : 3000;
	const int rateMsec = static_cast<int>( ( messageBytes + 48 ) * 1000 / static_cast<std::size_t>( rate ) );
	const int snapshotMsec = client->snapshotMsec > 0 ? client->snapshotMsec : 50;
	session.nextSnapshotTime = svs.time + std::max( rateMsec, snapshotMsec );
	session.rateDelayed = rateMsec >= snapshotMsec;
	if ( !SV_Q3TestLegacyWire() && ( silent || client->state != CS_ACTIVE ) )
		session.nextSnapshotTime = std::max( session.nextSnapshotTime, svs.time + 1000 );
}

/* SV_SendClientSnapshot: 35/36 put the unacknowledged configstrings before
 * the snapshot in 1400 bytes and stop the entities past 1350 (1.05
 * 0x41de50); 38-40 put every unacknowledged server command first, in 16384
 * bytes (1.06 0x41ea80). Either may end with sv_padPackets nops. */
void SV_Q3TestSendSnapshot( client_t *client ) {
	auto &session = SV_Q3TestSessionFor( client );
	const int protocol = SV_Q3TestNetProtocol();
	const bool legacy = q3wire::IsLegacy( protocol );
	const int messageNum = session.chan.OutgoingSequence();
	Q3TestFrame &frame = session.frames[messageNum & ( Q3TEST_PACKET_BACKUP - 1 )];
	SV_Q3TestBuildSnapshot( client, frame );
	frame.messageNum = messageNum;
	if ( client->netchan.remoteAddress.type == NA_BOT ) return;
	const Q3TestFrame *old = SV_Q3TestDeltaFrame( client, session );
	q3wire::Snapshot &snap = frame.snap;
	snap.serverTime = sv.time;
	snap.messageNum = messageNum;
	snap.deltaOffset = old ? messageNum - old->messageNum : 0;
	snap.csSequence = q3net.csSequence;
	snap.reliableAcknowledge = client->lastClientCommand;
	snap.lastUsercmd = session.lastUsercmd;
	snap.snapFlags = ( session.rateDelayed ? 1 : 0 ) | ( protocol != 35 && session.lostCommands ? 2 : 0 );
	session.lostCommands = false;

	std::vector<std::uint8_t> buffer( legacy ? FNTECH3_Q3TEST_PACKET_BYTES : q3wire::MaxMessageBytes );
	q3wire::Writer out( buffer.data(), buffer.size() );
	const q3wire::Snapshot *from = old ? &old->snap : nullptr;
	std::size_t written = snap.entities.size();
	const auto finish = [&]( q3wire::Writer &w ) {
		return q3wire::WriteSnapshot( w, protocol, from, snap, *q3net.baselines, legacy, &written );
	};
	/* A message that fits is written as retail writes it. Records that do not
	 * all fit before the snapshot go in as far as they leave room for it and
	 * the rest follow in later messages: 35/36 send the oldest changes first
	 * and stamp the snapshot with the newest change they carry, which is all
	 * the client then acknowledges; 38-40's commands go in sequence. */
	bool fits = true;
	if ( legacy ) {
		std::vector<int> changed;
		for ( int index = 0; index < 1024; ++index ) {
			if ( q3net.csStamps[static_cast<std::size_t>( index )] > session.csAck ) changed.push_back( index );
		}
		const auto item = [&]( q3wire::Writer &w, int i ) {
			const int index = changed[static_cast<std::size_t>( i )];
			return q3wire::WriteConfigstring( w, protocol, index, sv.configstrings[index] ? sv.configstrings[index] : "" );
		};
		const int count = static_cast<int>( changed.size() );
		if ( q3wire::MessageFits( out, count, item, finish ) ) {
			for ( int i = 0; i < count && fits; ++i ) fits = item( out, i );
		} else {
			const auto stamp = []( int index ) { return q3net.csStamps[static_cast<std::size_t>( index )]; };
			std::stable_sort( changed.begin(), changed.end(), [&]( int a, int b ) { return stamp( a ) < stamp( b ); } );
			const int sent = q3wire::WriteFittingPrefix( out, count, item, finish );
			fits = sent >= 0;
			if ( fits && sent < count ) snap.csSequence = sent ? stamp( changed[static_cast<std::size_t>( sent - 1 )] ) : session.csAck;
		}
	} else {
		const int first = client->reliableAcknowledge + 1;
		const int count = std::max( 0, client->reliableSequence - client->reliableAcknowledge );
		const auto item = [&]( q3wire::Writer &w, int i ) {
			const int sequence = first + i;
			return q3wire::WriteServerCommand( w, protocol, sequence,
				client->reliableCommands[sequence & ( Q3TEST_COMMAND_RING - 1 )] );
		};
		if ( q3wire::MessageFits( out, count, item, finish ) ) {
			for ( int i = 0; i < count && fits; ++i ) fits = item( out, i );
		} else {
			fits = q3wire::WriteFittingPrefix( out, count, item, finish ) >= 0;
		}
	}
	if ( !fits || !finish( out ) ) {
		Com_Printf( "WARNING: snapshot overflowed for %s\n", client->name );
		frame.messageNum = -1;
		return;
	}
	// 35/36's cap keeps only the entities it emitted in the server's frame.
	if ( written < snap.entities.size() ) snap.entities.resize( written );
	for ( int pad = 0; sv_padPackets && pad < sv_padPackets->integer; ++pad ) {
		if ( !q3wire::WriteNop( out ) ) break;
	}
	client->frames[messageNum & PACKET_MASK].messageSent = svs.time;
	client->frames[messageNum & PACKET_MASK].messageAcked = 0;
	SV_Q3TestTransmit( client, buffer.data(), out.Size() );
	SV_Q3TestScheduleNext( client, session, out.Size() );
}

/* ---- gamestate ------------------------------------------------------------- */

/* SV_SendClientGameState (1.05 0x41b060, 1.06 0x41bba0): every configstring
 * and the baselines retail selects. 35/36 queue it on the reliable message
 * and take its configstring sequence as acknowledged; 38-40 restart the
 * command ring at 0 and send it as its own message, recording the sequence
 * it goes out under. */
void SV_Q3TestSendGamestate( client_t *client ) {
	auto &session = SV_Q3TestSessionFor( client );
	const int protocol = SV_Q3TestNetProtocol();
	const bool legacy = q3wire::IsLegacy( protocol );
	client->state = CS_PRIMED;
	auto configstrings = std::make_unique<q3wire::Configstrings>();
	for ( int index = 0; index < 1024 && index < Com_GameConfigstringLimit(); ++index ) {
		if ( sv.configstrings[index] ) ( *configstrings )[static_cast<std::size_t>( index )] = sv.configstrings[index];
	}
	std::vector<std::uint8_t> buffer( q3wire::MaxMessageBytes );
	q3wire::Writer own( buffer.data(), buffer.size() );
	q3wire::Writer &out = legacy ? session.pending.out : own;
	if ( !legacy ) {
		client->reliableSequence = 0;
		client->reliableAcknowledge = 0;
	}
	if ( !q3wire::WriteServerGamestate( out, protocol, legacy ? q3net.csSequence : client->reliableSequence,
		*configstrings, *q3net.baselines ) ) {
		SV_DropClient( client, "gamestate overflowed" );
		return;
	}
	Com_DPrintf( "Sending gamestate to %s\n", client->name );
	if ( legacy ) {
		session.csAck = q3net.csSequence;
		return;
	}
	client->gamestateMessageNum = session.chan.OutgoingSequence();
	SV_Q3TestTransmit( client, buffer.data(), out.Size() );
}

/* ---- client messages ------------------------------------------------------- */

void SV_Q3TestUserinfoChanged( client_t *client ) {
	// SV_UserinfoChanged (1.02 0x41b000, 1.06 0x41bd90)
	const char *value = Info_ValueForKey( client->userinfo, "rate" );
	if ( Sys_IsLANAddress( &client->netchan.remoteAddress ) && com_dedicated->integer != 2 ) {
		client->rate = 99999;
	} else {
		client->rate = value[0] ? std::clamp( atoi( value ), 1000, 90000 ) : 3000;
	}
	value = Info_ValueForKey( client->userinfo, "snaps" );
	if ( !value[0] ) {
		client->snapshotMsec = 50;
	} else {
		const int snaps = std::clamp( atoi( value ), 1, 30 );
		client->snapshotMsec = 1000 / snaps;
	}
	Q_strncpyz( client->name, Info_ValueForKey( client->userinfo, "name" ), sizeof( client->name ) );
}

/* SV_ClientEnterWorld (1.05 0x41b160, 1.06 0x41bcd0): the client's entity,
 * numbered by its slot, takes the first command of the move that named the
 * current server id. */
void SV_Q3TestEnterWorld( client_t *client, Q3TestSession &session, const q3wire::Usercmd &cmd ) {
	const int index = SV_ClientIndex( client );
	Com_DPrintf( "Going from CS_PRIMED to CS_ACTIVE for %s\n", client->name );
	client->state = CS_ACTIVE;
	auto *entity = static_cast<q3tEnt *>( SV_Q3TestEntityForClient( index ) );
	entity->s.number = SV_Q3TestClientEntity( index );
	client->gentity = entity;
	client->deltaMessage = -1;
	session.lastUsercmd = 0;
	session.nextSnapshotTime = svs.time;
	std::memcpy( SV_Q3TestCommand( index ), cmd.data(), SV_Q3TestUsercmdBytes() );
	VM_Call( gvm, 1, GAME_CLIENT_BEGIN, index );
}

// SV_ClientThink (1.05 0x41b5e0): only an active client's entity thinks.
void SV_Q3TestClientThink( client_t *client, const q3wire::Usercmd &cmd ) {
	const int index = SV_ClientIndex( client );
	std::memcpy( SV_Q3TestCommand( index ), cmd.data(), SV_Q3TestUsercmdBytes() );
	if ( client->state == CS_ACTIVE ) VM_Call( gvm, 1, GAME_CLIENT_THINK, index );
}

/* SV_UserMove (1.05 0x41b620, 1.06 0x41c0c0). Moves naming another server id
 * run nothing; 38-40 then resend the gamestate once the client has
 * acknowledged a message sent after it. Lost commands are replayed with the
 * oldest received, at most five. 35/36 carry a provisional command last, for
 * the game's own export unless sv_nopredict is set. */
void SV_Q3TestUserMove( client_t *client, Q3TestSession &session, const q3wire::Move &move ) {
	const int protocol = SV_Q3TestNetProtocol();
	const bool legacy = q3wire::IsLegacy( protocol );
	const int count = static_cast<int>( move.cmds.size() );
	if ( Cvar_VariableIntegerValue( "q3test_nettrace" ) > 2 ) {
		char hex[3 * q3wire::MaxUsercmdBytes + 1] = "";
		for ( std::size_t i = 0; i < SV_Q3TestUsercmdBytes(); ++i )
			Q_strcat( hex, sizeof( hex ), va( "%02x ", move.cmds.back()[i] ) );
		Com_Printf( "Q3TNET move id %i last %i count %i delta %i cs %i/%i cmd %s\n", move.serverId, move.lastCmd, count,
			move.deltaMessage, move.csAck, move.serverCommandSequence, hex );
	}
	if ( legacy ) {
		client->deltaMessage = move.deltaMessage;
		if ( move.csAck != -1 ) session.csAck = move.csAck;
	} else {
		// Retail stores whatever the client sends; a value outside the window
		// would index commands that were never queued.
		if ( move.serverCommandSequence <= client->reliableSequence
			&& move.serverCommandSequence >= client->reliableSequence - Q3TEST_COMMAND_RING )
			client->reliableAcknowledge = move.serverCommandSequence;
		client->deltaMessage = move.deltaMessage;
	}
	if ( move.serverId != sv.serverId ) {
		if ( !legacy && client->gamestateMessageNum < session.chan.IncomingAcknowledged() ) {
			Com_DPrintf( "%s : dropped gamestate, resending\n", client->name );
			SV_Q3TestSendGamestate( client );
		}
		return;
	}
	if ( client->state == CS_PRIMED ) SV_Q3TestEnterWorld( client, session, move.cmds.front() );
	if ( client->state != CS_ACTIVE ) {
		client->deltaMessage = -1;
		return;
	}
	const int real = legacy ? count - 1 : count;
	const int first = move.lastCmd - real + 1;
	if ( session.lastUsercmd < first - 1 ) {
		session.lostCommands = true;
		if ( sv_showloss && sv_showloss->integer )
			Com_Printf( "Lost %i usercmds from %s\n", first - 1 - session.lastUsercmd, client->name );
		if ( session.lastUsercmd < first - 6 ) session.lastUsercmd = first - 6;
		while ( session.lastUsercmd < first - 1 ) {
			++session.lastUsercmd;
			SV_Q3TestClientThink( client, move.cmds.front() );
		}
	}
	for ( int i = std::max( 0, session.lastUsercmd - first + 1 ); i < real; ++i ) {
		SV_Q3TestClientThink( client, move.cmds[static_cast<std::size_t>( i )] );
	}
	session.lastUsercmd = move.lastCmd;
	if ( legacy && !Cvar_VariableIntegerValue( "sv_nopredict" ) && client->state == CS_ACTIVE )
		SV_Q3TestClientAfterCommands( SV_ClientIndex( client ), move.cmds.back().data() );
}

/* SV_ExecuteClientCommand: the engine's own userinfo and disconnect, then the
 * game's while the server is in the game. 1.02-1.05's download and nextdl are
 * not served: no Q3Test client can take a download. */
void SV_Q3TestClientCommand( client_t *client, const std::string &text ) {
	Cmd_TokenizeString( text.c_str() );
	const char *verb = Cmd_Argv( 0 );
	if ( !strcmp( verb, "userinfo" ) ) {
		Q_strncpyz( client->userinfo, Cmd_Argv( 1 ), sizeof( client->userinfo ) );
		Info_SetValueForKey( client->userinfo, "ip", NET_IsLocalAddress( &client->netchan.remoteAddress )
			? "localhost" : NET_AdrToString( &client->netchan.remoteAddress ) );
		SV_Q3TestUserinfoChanged( client );
		VM_Call( gvm, 1, GAME_CLIENT_USERINFO_CHANGED, SV_ClientIndex( client ) );
		return;
	}
	if ( !strcmp( verb, "disconnect" ) ) {
		SV_DropClient( client, "disconnected" );
		return;
	}
	if ( !strcmp( verb, "download" ) || !strcmp( verb, "nextdl" ) ) return;
	if ( sv.state == SS_GAME ) VM_Call( gvm, 1, GAME_CLIENT_COMMAND, SV_ClientIndex( client ) );
}

/* SV_ExecuteClientMessage (1.05 0x41b830, 1.06 0x41c2c0). */
void SV_Q3TestExecuteMessage( client_t *client, Q3TestSession &session, const void *data, std::size_t size ) {
	const int protocol = SV_Q3TestNetProtocol();
	const bool legacy = q3wire::IsLegacy( protocol );
	q3wire::ClientMessage message;
	std::string error;
	if ( !q3wire::ParseClientMessage( protocol, data, size, message, &error ) ) {
		SV_DropClient( client, error == "unknown client command" ? "had an unknown command char" : "had a badread" );
		return;
	}
	// 35/36 answer a connected client's first message with the gamestate.
	if ( legacy && client->state == CS_CONNECTED ) SV_Q3TestSendGamestate( client );
	int legacyCommands = 0;
	for ( const q3wire::ClientOp &op : message.ops ) {
		if ( client->state == CS_ZOMBIE || client->state == CS_FREE ) return;
		if ( const auto *command = std::get_if<q3wire::ClientCommand>( &op ) ) {
			if ( legacy ) {
				if ( ++legacyCommands <= Q3TEST_COMMANDS_PER_BATCH ) SV_Q3TestClientCommand( client, command->text );
				continue;
			}
			if ( command->sequence <= client->lastClientCommand ) continue;
			if ( command->sequence > client->lastClientCommand + 1 ) {
				Com_Printf( "Client %s lost %i clientCommands\n", client->name, command->sequence - client->lastClientCommand + 1 );
			}
			client->lastClientCommand = command->sequence;
			if ( session.commandFrame != q3net.frameCount ) {
				session.commandFrame = q3net.frameCount;
				session.commandsThisFrame = 0;
			}
			if ( ++session.commandsThisFrame <= Q3TEST_COMMANDS_PER_BATCH ) SV_Q3TestClientCommand( client, command->text );
		} else if ( const auto *move = std::get_if<q3wire::Move>( &op ) ) {
			SV_Q3TestUserMove( client, session, *move );
		}
	}
}

/* ---- connectionless -------------------------------------------------------- */

// Retail's rand() is MSVC's fifteen bits.
int SV_Q3TestRand15() { return rand() & 0x7fff; }

/* SV_GetChallenge (identical in every release, 1.02 0x41a690): 1024 entries
 * keyed by IP alone, never refreshed or expired; an unknown address takes the
 * oldest entry. */
void SV_Q3TestGetChallenge( const netadr_t *from ) {
	Q3TestChallenge *oldest = &q3net.challenges[0];
	for ( Q3TestChallenge &entry : q3net.challenges ) {
		if ( entry.used && NET_CompareBaseAdr( from, &entry.adr ) ) {
			NET_OutOfBandPrint( NS_SERVER, from, "challengeResponse %i", entry.challenge );
			return;
		}
		if ( !entry.used || entry.time < oldest->time ) oldest = &entry;
		if ( !oldest->used ) break;
	}
	oldest->used = true;
	oldest->adr = *from;
	oldest->challenge = ( SV_Q3TestRand15() << 16 ) ^ SV_Q3TestRand15();
	oldest->time = svs.time;
	NET_OutOfBandPrint( NS_SERVER, from, "challengeResponse %i", oldest->challenge );
}

const Q3TestChallenge *SV_Q3TestFindChallenge( const netadr_t *from ) {
	for ( const Q3TestChallenge &entry : q3net.challenges ) {
		if ( entry.used && NET_CompareBaseAdr( from, &entry.adr ) ) return &entry;
	}
	return nullptr;
}

void SV_Q3TestResetSession( client_t *client, int qport ) {
	const int index = SV_ClientIndex( client );
	SV_Q3TestSessionFor( client );
	q3net.sessions[static_cast<std::size_t>( index )] = std::make_unique<Q3TestSession>();
	auto &session = *q3net.sessions[static_cast<std::size_t>( index )];
	session.pending.Reset();
	session.chan.Setup( SV_Q3TestLegacyWire(), fntech3::q3test::NetchanSource::Server, qport );
}

/* SV_DirectConnect (1.02 0x41a780, 1.05 0x41a3c0-ish, 1.06 0x41b630). */
void SV_Q3TestDirectConnect( const netadr_t *from ) {
	const q3testRelease_t &release = SV_Q3TestNetRelease();
	const int protocol = release.protocol;
	char userinfo[MAX_INFO_STRING];
	int version, qport, challenge;
	if ( protocol == 35 ) {
		version = atoi( Cmd_Argv( 1 ) );
		qport = atoi( Cmd_Argv( 2 ) );
		challenge = atoi( Cmd_Argv( 3 ) );
		Q_strncpyz( userinfo, Cmd_Argv( 4 ), sizeof( userinfo ) );
	} else {
		Q_strncpyz( userinfo, Cmd_Argv( 1 ), sizeof( userinfo ) );
		version = atoi( Info_ValueForKey( userinfo, "protocol" ) );
		qport = atoi( Info_ValueForKey( userinfo, protocol == 36 ? "port" : "qport" ) );
		challenge = atoi( Info_ValueForKey( userinfo, "challenge" ) );
	}
	if ( version != protocol ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\nServer uses protocol version %i.\n", protocol );
		Com_DPrintf( "    rejected connect from version %i\n", version );
		return;
	}
	if ( !NET_IsLocalAddress( from ) ) {
		const Q3TestChallenge *entry = SV_Q3TestFindChallenge( from );
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

	client_t *newcl = nullptr;
	bool reconnect = false;
	// A reconnect from the same address and qport or port takes its old slot
	// in place, with no drop (1.02 0x41aa21..0x41aa9f).
	for ( client_t &client : SV_Clients() ) {
		if ( client.state == CS_FREE ) continue;
		if ( NET_CompareBaseAdr( from, &client.netchan.remoteAddress )
			&& ( client.netchan.qport == qport || from->port == client.netchan.remoteAddress.port ) ) {
			if ( svs.time - client.lastConnectTime < sv_reconnectlimit->integer * 1000 ) {
				Com_DPrintf( "%s:reconnect rejected : too soon\n", NET_AdrToString( from ) );
				return;
			}
			Com_Printf( "%s:reconnect\n", NET_AdrToString( from ) );
			newcl = &client;
			reconnect = true;
			break;
		}
	}
	if ( !newcl ) {
		/* 1.02 reserves its private slots with the rcon password, and only
		 * when one is set (0x41a837); 1.03 and later compare with
		 * sv_privatePassword, set or not (1.03 0x41af21). */
		const char *password = Info_ValueForKey( userinfo, "password" );
		bool privileged;
		if ( release.id == Q3TEST_RELEASE_1_02 ) {
			const char *rcon = SV_Q3TestRconPassword();
			privileged = rcon[0] && !strcmp( password, rcon );
		} else {
			privileged = !strcmp( password, sv_privatePassword->string );
		}
		const int reserved = release.id == Q3TEST_RELEASE_1_02 ? Cvar_VariableIntegerValue( "privateclients" )
			: sv_privateClients->integer;
		const int start = privileged ? 0 : reserved;
		for ( int i = std::max( 0, start ); i < sv.maxclients; ++i ) {
			client_t &client = SV_ClientForIndex( i );
			if ( client.state == CS_FREE ) {
				newcl = &client;
				break;
			}
		}
		if ( !newcl ) {
			NET_OutOfBandPrint( NS_SERVER, from, "print\nServer is full.\n" );
			Com_DPrintf( "Rejected a connection.\n" );
			return;
		}
	}
	(void)reconnect;

	const int index = SV_ClientIndex( newcl );
	// The slot starts over; its command strings are left as they were.
	newcl->state = CS_FREE;
	newcl->reliableSequence = newcl->reliableAcknowledge = 0;
	newcl->lastClientCommand = 0;
	newcl->gamestateMessageNum = -1;
	newcl->deltaMessage = -1;
	newcl->timeoutCount = 0;
	newcl->ping = 0;
	newcl->gentity = nullptr;
	newcl->name[0] = '\0';
	for ( auto &frame : newcl->frames ) {
		frame.messageSent = frame.messageAcked = 0;
	}
	Q_strncpyz( newcl->userinfo, userinfo, sizeof( newcl->userinfo ) );
	Netchan_Setup( NS_SERVER, &newcl->netchan, from, qport, challenge,
		SV_Q3TestLegacyWire() ? NETCHAN_WIRE_Q3TEST_A : NETCHAN_WIRE_Q3TEST_B );
	SV_Q3TestResetSession( newcl, qport );

	auto admit = [&]() -> bool {
		const char *denied = static_cast<const char *>( GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, index, qtrue, qfalse ) ) );
		if ( !denied ) return true;
		NET_OutOfBandPrint( NS_SERVER, from, "print\n%s\n", denied );
		Com_DPrintf( "Game rejected a connection: %s.\n", denied );
		newcl->state = CS_FREE;
		newcl->netchan.remoteAddress.type = NA_BAD;
		return false;
	};
	/* 1.02-1.05 let the game decide before the channel exists (1.02
	 * 0x41aab4..0x41aad6); 1.06 and later set the channel and userinfo up
	 * first (0x41b9b5). */
	if ( SV_Q3TestLegacyWire() && !admit() ) return;
	newcl->state = CS_CONNECTED;
	newcl->lastPacketTime = svs.time;
	newcl->lastConnectTime = svs.time;
	SV_Q3TestSessionFor( newcl ).nextSnapshotTime = svs.time;
	SV_Q3TestUserinfoChanged( newcl );
	if ( !SV_Q3TestLegacyWire() && !admit() ) return;
	Com_DPrintf( "Going from CS_FREE to CS_CONNECTED for %s\n", newcl->name );
	NET_OutOfBandPrint( NS_SERVER, from, "connectResponse" );
}

/* SVC_Info (1.02 0x41c750, 1.03 0x41cce0): 1.02 counts every connected slot
 * and names its maximum as set; 1.03 and later count and offer only the
 * public ones. */
void SV_Q3TestInfo( const netadr_t *from ) {
	const q3testRelease_t &release = SV_Q3TestNetRelease();
	const bool v102 = release.id == Q3TEST_RELEASE_1_02;
	const int privateClients = v102 ? 0 : sv_privateClients->integer;
	int count = 0;
	for ( int i = privateClients; i < sv.maxclients; ++i ) {
		if ( SV_ClientForIndex( i ).state >= CS_CONNECTED ) ++count;
	}
	char info[MAX_INFO_STRING] = "";
	if ( Cmd_Argc() > 1 ) Info_SetValueForKey( info, "challenge", Cmd_Argv( 1 ) );
	Info_SetValueForKey( info, "protocol", va( "%i", release.protocol ) );
	Info_SetValueForKey( info, "hostname", v102 ? Cvar_VariableString( "hostname" ) : sv_hostname->string );
	Info_SetValueForKey( info, "mapname", sv_mapname->string );
	Info_SetValueForKey( info, "clients", va( "%i", count ) );
	Info_SetValueForKey( info, "sv_maxclients", va( "%i", sv_maxclients->integer - privateClients ) );
	NET_OutOfBandPrint( NS_SERVER, from, "infoResponse\n%s", info );
}

/* SVC_Status (1.02 0x41c5e0, 1.08 0x41dfe0): the server info with the
 * challenge, then score, ping and name of every connected client. */
void SV_Q3TestStatus( const netadr_t *from ) {
	char info[BIG_INFO_STRING];
	Q_strncpyz( info, SV_Q3TestServerinfo(), sizeof( info ) );
	if ( Cmd_Argc() > 1 ) Info_SetValueForKey( info, "challenge", Cmd_Argv( 1 ) );
	std::string players;
	const std::size_t limit = SV_Q3TestLegacyWire() ? 1400 : 16384;
	const auto &layout = SV_Q3TestPlayerLayout();
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		if ( slot.client.state < CS_CONNECTED ) continue;
		int score = 0;
		std::array<std::uint8_t, q3wire::MaxPlayerBytes> player{};
		if ( slot.client.gentity && serverGameData.ReadQ3TestPlayerState( slot.index, player.data(), layout.bytes ) ) {
			if ( layout.shortScore ) {
				std::int16_t value; std::memcpy( &value, player.data() + layout.score, 2 ); score = value;
			} else {
				score = fntech3::q3test::state::LoadInt( player.data() + layout.score );
			}
		}
		const std::string line = va( "%i %i \"%s\"\n", score, slot.client.ping, slot.client.name );
		if ( players.size() + line.size() >= limit ) break;
		players += line;
	}
	NET_OutOfBandPrint( NS_SERVER, from, "statusResponse\n%s\n%s", info, players.c_str() );
}

/* SV_ConnectionlessPacket (identical in every release, 1.02 0x41ca70): the
 * first line's first token, compared exactly. */
void SV_Q3TestConnectionless( const netadr_t *from, msg_t *msg ) {
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );
	const char *line = MSG_ReadStringLine( msg );
	char text[MAX_STRING_CHARS];
	Q_strncpyz( text, line, sizeof( text ) );
	Cmd_TokenizeString( text );
	const char *verb = Cmd_Argv( 0 );
	Com_DPrintf( "SV packet %s : %s\n", NET_AdrToString( from ), verb );
	if ( !strcmp( verb, "getstatus" ) ) SV_Q3TestStatus( from );
	else if ( !strcmp( verb, "getinfo" ) ) SV_Q3TestInfo( from );
	else if ( !strcmp( verb, "getchallenge" ) ) SV_Q3TestGetChallenge( from );
	else if ( !strcmp( verb, "connect" ) ) SV_Q3TestDirectConnect( from );
	else if ( !strcmp( verb, "rcon" ) ) SV_RemoteCommandPacket( from );
	else if ( !strcmp( verb, "disconnect" ) ) {}
	else Com_Printf( "bad connectionless packet from %s:\n%s\n", NET_AdrToString( from ), text );
}

} // namespace

/* ---- engine hooks ---------------------------------------------------------- */

/* SV_PacketEvent (1.02 0x41ccd0, 1.06 0x41dda0): the client is found by base
 * address and the qport in bytes 8 and 9; a changed UDP port is adopted; a
 * sequenced packet from nobody is answered with a disconnect. */
void SV_Q3TestPacketEvent( const netadr_t *from, msg_t *msg ) {
	if ( msg->cursize >= 4 ) {
		int32_t marker;
		std::memcpy( &marker, msg->data, sizeof( marker ) );
		if ( marker == -1 ) {
			if ( com_sv_running->integer ) SV_Q3TestConnectionless( from, msg );
			return;
		}
	}
	if ( sv.state == SS_DEAD || msg->cursize < 10 ) return;
	const int qport = msg->data[8] | ( msg->data[9] << 8 );
	for ( client_t &client : SV_Clients() ) {
		if ( client.state == CS_FREE || !NET_CompareBaseAdr( from, &client.netchan.remoteAddress )
			|| client.netchan.qport != qport ) continue;
		if ( client.netchan.remoteAddress.port != from->port ) {
			Com_Printf( "SV_ReadPackets: fixing up a translated port\n" );
			client.netchan.remoteAddress.port = from->port;
		}
		auto &session = SV_Q3TestSessionFor( &client );
		std::size_t size = static_cast<std::size_t>( msg->cursize );
		const auto result = session.chan.Process( msg->data, size, static_cast<std::size_t>( msg->maxsize ) );
		SV_Q3TestMirrorChannel( &client, session );
		if ( Cvar_VariableIntegerValue( "q3test_nettrace" ) ) {
			Com_Printf( "Q3TNET recv %i bytes result %i in %i ack %i -> %i bytes\n", msg->cursize, static_cast<int>( result ),
				session.chan.IncomingSequence(), session.chan.IncomingAcknowledged(), static_cast<int>( size ) );
		}
		if ( result != fntech3::q3test::NetchanResult::Message ) return;
		if ( client.state == CS_ZOMBIE ) return;
		client.lastPacketTime = svs.time;
		// Every packet restamps the frame it acknowledges (1.02 0x41cd6a).
		client.frames[session.chan.IncomingAcknowledged() & PACKET_MASK].messageAcked = svs.time;
		SV_Q3TestExecuteMessage( &client, session, msg->data, size );
		return;
	}
	NET_OutOfBandPrint( NS_SERVER, from, "disconnect" );
}

/* SV_SendClientMessages (1.02 0x41db80, 1.06 0x41eb20): active clients get a
 * snapshot when due. 35/36 keep every other slot, zombies included, alive
 * with an empty message whenever reliable data waits or once a second, and
 * drop one whose reliable message overflowed; 38-40 send connected and primed
 * clients an empty message once a second and zombies nothing. */
void SV_Q3TestSendClientMessages() {
	++q3net.frameCount;
	const bool legacy = SV_Q3TestLegacyWire();
	for ( client_t &client : SV_Clients() ) {
		if ( client.state == CS_FREE || client.netchan.remoteAddress.type == NA_BOT ) continue;
		auto &session = SV_Q3TestSessionFor( &client );
		if ( legacy && ( session.overflowed || session.chan.FatalError() ) ) {
			session.overflowed = false;
			SV_DropClient( &client, "overflowed" );
			continue;
		}
		if ( Cvar_VariableIntegerValue( "q3test_nettrace" ) > 1 ) {
			Com_Printf( "Q3TNET frame %i client state %i time %i next %i\n", q3net.frameCount, client.state, svs.time,
				session.nextSnapshotTime );
		}
		if ( client.state == CS_ACTIVE ) {
			if ( svs.time - session.nextSnapshotTime >= 0 ) SV_Q3TestSendSnapshot( &client );
			continue;
		}
		if ( legacy ) {
			if ( session.pending.out.Size() || session.chan.PendingReliable() || svs.time - session.nextSnapshotTime >= 0 ) {
				SV_Q3TestTransmit( &client, nullptr, 0 );
				session.nextSnapshotTime = svs.time + 1000;
			}
		} else if ( client.state != CS_ZOMBIE && svs.time - session.nextSnapshotTime >= 0 ) {
			SV_Q3TestTransmit( &client, nullptr, 0 );
			session.nextSnapshotTime = svs.time + 1000;
		}
	}
}

void SV_Q3TestSendServerCommand( client_t *client, const char *text ) {
	if ( client ) SV_Q3TestAddCommand( client, text );
	else SV_Q3TestBroadcastCommand( text );
}

/* 35/36 stamp each change with a new sequence and repeat it in snapshots
 * (1.05 SV_SetConfigstring 0x41be60); 38-40 broadcast `cs` while the server
 * is in the game (1.06 0x41c97d). */
void SV_Q3TestConfigstringChanged( int index ) {
	if ( index < 0 || index >= 1024 ) return;
	if ( SV_Q3TestLegacyWire() ) {
		q3net.csStamps[static_cast<std::size_t>( index )] = ++q3net.csSequence;
		return;
	}
	if ( sv.state == SS_GAME ) {
		SV_Q3TestBroadcastCommand( va( "cs %i \"%s\"\n", index, sv.configstrings[index] ? sv.configstrings[index] : "" ) );
	}
}

/* The game's cvars that carry the release's SERVERINFO or SYSTEMINFO bit, as
 * they would be in the retail cvar list. */
static void SV_Q3TestAddGameInfo( char *info, int engineFlag ) {
	for ( const auto &node : q3testCvars ) {
		if ( node && node->source && ( node->source->flags & engineFlag ) && node->source->string && node->source->string[0] )
			Info_SetValueForKey( info, node->name, node->source->string );
	}
}

/* Each release's systeminfo (SV_Init 1.02 0x41bed0, 1.06 0x41cf90, 1.07
 * 0x41d030): sv_serverid, timescale up to 1.06, cheats from 1.06, and the
 * game's systeminfo cvars (g_syncronousClients in every release but 1.05).
 * A retail client applies every key it finds (CL_SystemInfoChanged), so
 * nothing of this engine's own is published. */
const char *SV_Q3TestSysteminfo() {
	static char info[MAX_INFO_STRING];
	info[0] = '\0';
	const q3testReleaseId_t id = SV_Q3TestNetRelease().id;
	SV_Q3TestAddGameInfo( info, CVAR_SYSTEMINFO );
	Info_SetValueForKey( info, "sv_serverid", va( "%i", sv.serverId ) );
	if ( id >= Q3TEST_RELEASE_1_06 ) Info_SetValueForKey( info, "cheats", sv_cheats && sv_cheats->integer ? "1" : "0" );
	if ( id <= Q3TEST_RELEASE_1_06 ) Info_SetValueForKey( info, "timescale", com_timescale->string );
	return info;
}

/* Each release's serverinfo: the engine's own serverinfo cvars (SV_Init;
 * net_transport.md 5.6) and the game's, not every cvar this engine marks
 * serverinfo -- sv_dlRate, sv_floodProtect and the rest have no Q3Test
 * counterpart. 1.02 names two of them hostname and privateclients; 1.02 to
 * 1.05 publish cheats here, 1.06 and later g_gametype and sv_zone (1.06,
 * 1.07) or sv_keywords (1.08). The version is this engine's. */
const char *SV_Q3TestServerinfo() {
	static char info[MAX_INFO_STRING];
	info[0] = '\0';
	const q3testReleaseId_t id = SV_Q3TestNetRelease().id;
	auto add = [&]( const char *key, const char *value ) {
		if ( value && value[0] ) Info_SetValueForKey( info, key, value );
	};
	SV_Q3TestAddGameInfo( info, CVAR_SERVERINFO );
	if ( id == Q3TEST_RELEASE_1_02 ) {
		add( "hostname", Cvar_VariableString( "hostname" ) );
		add( "privateclients", Cvar_VariableString( "privateclients" ) );
	} else {
		add( "sv_hostname", sv_hostname->string );
		add( "sv_privateClients", sv_privateClients->string );
	}
	add( "mapname", sv_mapname->string );
	add( "protocol", va( "%i", SV_Q3TestNetRelease().protocol ) );
	if ( id >= Q3TEST_RELEASE_1_06 ) {
		add( id >= Q3TEST_RELEASE_1_08 ? "sv_keywords" : "sv_zone",
			Cvar_VariableString( id >= Q3TEST_RELEASE_1_08 ? "sv_keywords" : "sv_zone" ) );
		add( "g_gametype", Cvar_VariableString( "g_gametype" ) );
	}
	add( "timelimit", Cvar_VariableString( "timelimit" ) );
	add( "fraglimit", Cvar_VariableString( "fraglimit" ) );
	add( "dmflags", Cvar_VariableString( "dmflags" ) );
	add( "version", Cvar_VariableString( "version" ) );
	add( "sv_maxclients", sv_maxclients->string );
	if ( id <= Q3TEST_RELEASE_1_05 ) add( "cheats", sv_cheats && sv_cheats->integer ? "1" : "0" );
	return info;
}

/* The engine cvars a release has and this engine does not: 1.02's hostname,
 * privateclients and rcon_password, 1.06 and 1.07's sv_zone. */
void SV_Q3TestRegisterCvars() {
	const q3testReleaseId_t id = SV_Q3TestNetRelease().id;
	if ( id == Q3TEST_RELEASE_1_02 ) {
		Cvar_Get( "hostname", "noname", 0 );
		Cvar_Get( "privateclients", "0", 0 );
		Cvar_Get( "rcon_password", "", CVAR_TEMP );
	}
	if ( id == Q3TEST_RELEASE_1_06 || id == Q3TEST_RELEASE_1_07 ) Cvar_Get( "sv_zone", "default", 0 );
}

/* SV_CreateBaseline (every release, entities 1 to n - 1): a linked entity's
 * state, numbered, is its baseline. */
void SV_Q3TestCreateBaselines() {
	q3net.baselines = std::make_unique<q3wire::Baselines>();
	void *base; int stride, count, capacity;
	SV_Q3TestEntityArray( base, stride, count, capacity );
	for ( int e = 1; e < count && e < 1024; ++e ) {
		auto *ent = reinterpret_cast<q3tEnt *>( static_cast<byte *>( base ) + static_cast<std::size_t>( e ) * stride );
		if ( !ent->linked ) continue;
		ent->s.number = e;
		std::memcpy( ( *q3net.baselines )[static_cast<std::size_t>( e )].data(), &ent->s, q3wire::EntityBytes );
	}
}

/* A new level clears the configstring sequence and every frame; 35/36
 * clients then take the gamestate with their next message, 38-40 clients
 * with their next move under the old server id. */
void SV_Q3TestSpawnReset() {
	q3net.csStamps.fill( 0 );
	q3net.csSequence = 0;
	q3net.baselines = std::make_unique<q3wire::Baselines>();
	Cvar_Get( "sv_nopredict", "0", 0 );
	for ( auto &session : q3net.sessions ) {
		if ( !session ) continue;
		for ( auto &frame : session->frames ) frame = Q3TestFrame{};
		session->csAck = 0;
		session->lastUsercmd = 0;
	}
}

/* SV_DropClient (1.02 0x41ac40, 1.06 0x41bb00). 1.02-1.05 announce the drop
 * to everyone, the dropped client included, then tell the game and queue the
 * disconnect; 1.06 and later make the slot a zombie first, so their
 * announcement skips it, and never transmit to zombies. */
void SV_Q3TestDropClient( client_t *drop, const char *reason ) {
	if ( drop->state == CS_ZOMBIE || drop->state == CS_FREE ) return;
	const int index = SV_ClientIndex( drop );
	const bool bot = drop->netchan.remoteAddress.type == NA_BOT;
	char name[MAX_NAME_LENGTH];
	Q_strncpyz( name, drop->name, sizeof( name ) );
	if ( SV_Q3TestLegacyWire() ) {
		if ( reason ) SV_Q3TestBroadcastCommand( va( "print \"%s %s\n\"", name, reason ) );
		VM_Call( gvm, 1, GAME_CLIENT_DISCONNECT, index );
		SV_Q3TestAddCommand( drop, "disconnect" );
		drop->name[0] = '\0';
		drop->state = CS_ZOMBIE;
	} else {
		drop->state = CS_ZOMBIE;
		if ( bot ) SV_BotFreeClient( index );
		else VM_Call( gvm, 1, GAME_CLIENT_DISCONNECT, index );
		if ( reason ) SV_Q3TestBroadcastCommand( va( "print \"%s %s\n\"", name, reason ) );
		SV_Q3TestAddCommand( drop, "disconnect" );
	}
	drop->gentity = nullptr;
	if ( bot ) drop->state = CS_FREE;
}

/* SV_FinalMessage (1.02 0x41c170, 1.06 0x41d220): 35/36 send every connected
 * client, twice, an unreliable message with the print and the disconnect;
 * 38-40 broadcast both and send two snapshots with the rate disarmed. */
void SV_Q3TestFinalMessage( const char *message ) {
	const int protocol = SV_Q3TestNetProtocol();
	if ( q3wire::IsLegacy( protocol ) ) {
		for ( int pass = 0; pass < 2; ++pass ) {
			for ( client_t &client : SV_Clients() ) {
				if ( client.state < CS_CONNECTED || client.netchan.remoteAddress.type == NA_BOT ) continue;
				std::uint8_t buffer[FNTECH3_Q3TEST_PACKET_BYTES];
				q3wire::Writer out( buffer, sizeof( buffer ) );
				q3wire::WriteServerCommand( out, protocol, 0, va( "print \"%s\"", message ) );
				q3wire::WriteServerCommand( out, protocol, 0, "disconnect" );
				SV_Q3TestTransmit( &client, buffer, out.Size() );
			}
		}
		return;
	}
	SV_Q3TestBroadcastCommand( va( "print \"%s\"", message ) );
	SV_Q3TestBroadcastCommand( "disconnect" );
	for ( int pass = 0; pass < 2; ++pass ) {
		for ( client_t &client : SV_Clients() ) {
			if ( client.state < CS_CONNECTED || client.netchan.remoteAddress.type == NA_BOT ) continue;
			SV_Q3TestSendSnapshot( &client );
		}
	}
}

/* Every slot's channel, when the server shuts down; a new connection
 * replaces its slot's channel (SV_Q3TestResetSession). */
void SV_Q3TestClearSessions() {
	q3net.sessions.clear();
}

#endif
