/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2023 the OpenMoHAA team
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor: Allied Assault 1.00's client side of protocol 6

A 1.00 server sends its client the game's own records: a 596-byte entity
state, a 660-byte player state, up to 64 sounds a snapshot, and three
opcodes past Quake III's snapshot -- a centre print (8), a located print (9)
and a client-game message (10), which only the client game can read to its
end -- before its own end byte, 11. The client answers each move with its
eye position and 1.00's keyed command.

None of that fits the engine's own snapshot, so the frames, the entity ring
and the baselines here hold the game's records, and the engine's cl.snap
keeps only what the rest of the client times itself by: the server time, the
flags, the message numbers, the ping and the command time. The client game
reads its snapshots through CL_MOHAAGetSnapshot.

The parse order and the parent-first entity order are OpenMoHAA's
(code/client/cl_parse.cpp and cl_cgame.cpp, GPL-2.0-or-later), whose
protocol-6 client these follow; the codecs are code/qcommon/mohaa_msg.cpp.
See docs/fntech3/2026-09-24-mohaa-contracts.md.
===========================================================================
*/

extern "C" {
#include "client.h"
}

#include "client_cpp.h"
#include "cl_mohaa.h"
#include "../compat/gamespy_query.hpp"
#include "../compat/mohaa_message_codec.hpp"
#include "../qcommon/net_query.h"
#include "../qcommon/netchan_safety.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstring>
#include <memory>
#include <string>

namespace {

using fntech3::mohaa::EntityState;
using fntech3::mohaa::PlayerState;
using fntech3::mohaa::ServerSound;
using fntech3::mohaa::Snapshot;
using fntech3::mohaa::UserCmd;
using fntech3::mohaa::UserEyes;
namespace wire = fntech3::mohaa::wire;

/* The entity ring holds eight full snapshots; a delta from a frame whose
 * entities have been overwritten is refused, as Quake III refuses one. */
constexpr int kParseEntities = 8 * FNTECH3_MOHAA_MAX_SNAPSHOT_ENTITIES;
constexpr int kParseEntityMask = kParseEntities - 1;
static_assert( ( kParseEntities & kParseEntityMask ) == 0, "the entity ring is a power of two" );

constexpr int kSnapflagServerCount = 4;
constexpr int kSvcEntityEnd = wire::kEntityNumEnd;

struct Frame {
	bool valid = false;
	int snapFlags = 0;
	int serverTime = 0;
	int serverTimeResidual = 0;
	int messageNum = 0;
	int deltaNum = -1;
	int ping = 0;
	int serverCommandNum = 0;
	std::uint8_t areamask[FNTECH3_MOHAA_MAX_MAP_AREA_BYTES] = {};
	PlayerState ps{};
	int numEntities = 0;
	int parseEntitiesNum = 0;
	int numSounds = 0;
	std::array<ServerSound, FNTECH3_MOHAA_MAX_SERVER_SOUNDS> sounds{};
};

/* The print the server last sent: centred, or at a point of the 640 by 480
 * screen. 1.00's view keeps one, so each replaces the other. */
struct Print {
	char text[FNTECH3_MOHAA_MAX_STRING_CHARS] = {};
	bool located = false;
	int x = 0;
	int y = 0;
	int time = 0;
};

struct State {
	std::array<Frame, PACKET_BACKUP> frames{};
	Frame snap{};
	std::unique_ptr<EntityState[]> parseEntities{ new EntityState[kParseEntities] };
	int parseEntitiesNum = 0;
	std::unique_ptr<EntityState[]> baselines{ new EntityState[FNTECH3_MOHAA_MAX_GENTITIES] };
	/* The baselines the gamestate named, which a demo's gamestate names again. */
	std::array<bool, FNTECH3_MOHAA_MAX_GENTITIES> baselineUsed{};
	int serverStartTime = 0;
	std::array<UserEyes, PACKET_BACKUP> sentEyes{};
	msg_t *currentMessage = nullptr;
	Print print;
};

State &Client() {
	static State *state = new State;
	return *state;
}

EntityState &ParseEntity( int index ) {
	return Client().parseEntities[index & kParseEntityMask];
}

/* A string of 1.00's own length: 2,048 characters, the high bytes kept and
 * a percent sign turned into a full stop, as its client reads one. */
const char *ReadString( msg_t *msg, char *buffer, std::size_t size ) {
	std::size_t length = 0;
	for ( ;; ) {
		const int c = MSG_ReadByte( msg );
		if ( c <= 0 ) {
			break;
		}
		if ( length + 1 < size ) {
			buffer[length++] = c == '%' ? '.' : static_cast<char>( c );
		}
	}
	buffer[length] = '\0';
	return buffer;
}

/* Keep one entity: the one read, or the old one unchanged. */
void DeltaEntity( msg_t *msg, Frame &frame, int number, const EntityState *old, bool unchanged ) {
	State &s = Client();
	EntityState &state = ParseEntity( s.parseEntitiesNum );

	if ( unchanged ) {
		state = *old;
	} else {
		wire::ReadDeltaEntity( msg, old, &state, number );
	}
	if ( state.number == kSvcEntityEnd ) {
		return;		// removed
	}
	s.parseEntitiesNum++;
	frame.numEntities++;
}

void ParsePacketEntities( msg_t *msg, const Frame *oldframe, Frame &newframe ) {
	State &s = Client();
	int oldindex = 0;
	const EntityState *oldstate = nullptr;
	int oldnum = 99999;

	newframe.parseEntitiesNum = s.parseEntitiesNum;
	newframe.numEntities = 0;

	const auto nextOld = [&]() {
		if ( !oldframe || oldindex >= oldframe->numEntities ) {
			oldstate = nullptr;
			oldnum = 99999;
		} else {
			oldstate = &ParseEntity( oldframe->parseEntitiesNum + oldindex );
			oldnum = oldstate->number;
		}
	};
	nextOld();

	for ( ;; ) {
		const int newnum = wire::ReadEntityNum( msg );
		if ( newnum == kSvcEntityEnd ) {
			break;
		}
		if ( msg->readcount > msg->cursize ) {
			Com_Error( ERR_DROP, "CL_ParsePacketEntities: end of message" );
		}
		if ( newnum < 0 || newnum >= FNTECH3_MOHAA_MAX_GENTITIES ) {
			Com_Error( ERR_DROP, "CL_ParsePacketEntities: entity %i out of range", newnum );
		}

		while ( oldnum < newnum ) {
			// one or more entities from the old packet are unchanged
			DeltaEntity( msg, newframe, oldnum, oldstate, true );
			oldindex++;
			nextOld();
		}
		if ( oldnum == newnum ) {
			// delta from the previous state
			DeltaEntity( msg, newframe, newnum, oldstate, false );
			oldindex++;
			nextOld();
			continue;
		}
		// oldnum > newnum: delta from the baseline
		DeltaEntity( msg, newframe, newnum, &s.baselines[newnum], false );
	}

	// any remaining entities in the old frame are copied over
	while ( oldnum != 99999 ) {
		DeltaEntity( msg, newframe, oldnum, oldstate, true );
		oldindex++;
		nextOld();
	}
}

/* The engine's snapshot keeps the timing and nothing of the game's. */
void MirrorEngineSnapshot( const Frame &frame ) {
	clSnapshot_t &g = cl.snap;
	g.valid = frame.valid ? qtrue : qfalse;
	g.snapFlags = frame.snapFlags;
	g.serverTime = frame.serverTime;
	g.messageNum = frame.messageNum;
	g.deltaNum = frame.deltaNum;
	g.ping = frame.ping;
	g.serverCommandNum = frame.serverCommandNum;
	g.areabytes = static_cast<int>( sizeof( frame.areamask ) );
	static_assert( sizeof( g.areamask ) >= sizeof( frame.areamask ), "area mask" );
	std::memcpy( g.areamask, frame.areamask, sizeof( frame.areamask ) );
	g.ps.commandTime = frame.ps.commandTime;
	g.ps.clientNum = frame.ps.clientNum;
	VectorCopy( frame.ps.origin, g.ps.origin );
	VectorCopy( frame.ps.viewangles, g.ps.viewangles );
	g.numEntities = 0;
	g.parseEntitiesNum = cl.parseEntitiesNum;
	cl.snapshots[frame.messageNum & PACKET_MASK] = g;
}

/* The view shows a print four seconds: it fades in over the first 750
 * milliseconds and out over the last 750 (OpenMoHAA client/cl_uiview3d.cpp
 * View3D::CenterPrint, reference only). */
void DrawPrint( const Print &print ) {
	if ( !print.text[0] ) {
		return;
	}
	const int age = cls.realtime - print.time;
	if ( age < 0 || age >= 4000 ) {
		return;
	}
	const float remaining = 4000.0f - age;
	float alpha = remaining > 3250.0f ? 1.0f - ( remaining - 3250.0f ) / 750.0f
		: remaining >= 750.0f ? 1.0f : remaining / 750.0f;
	alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	CL_MOHAADrawViewPrint( print.text, print.located, print.x, print.y, alpha );
}

} // namespace

void CL_MOHAAClearState( void ) {
	State &s = Client();
	for ( Frame &frame : s.frames ) {
		frame = Frame{};
	}
	s.snap = Frame{};
	s.parseEntitiesNum = 0;
	EntityState nullstate;
	wire::NullEntityState( nullstate );
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_GENTITIES; i++ ) {
		s.baselines[i] = nullstate;
	}
	s.baselineUsed.fill( false );
	s.serverStartTime = 0;
	s.sentEyes.fill( UserEyes{} );
	s.currentMessage = nullptr;
	s.print = Print{};
}

void CL_MOHAAParseBaseline( msg_t *msg ) {
	const int number = wire::ReadEntityNum( msg );
	if ( number < 0 || number >= FNTECH3_MOHAA_MAX_GENTITIES || number == kSvcEntityEnd ) {
		Com_Error( ERR_DROP, "CL_ParseGamestate: baseline number out of range: %i", number );
	}
	EntityState nullstate;
	wire::NullEntityState( nullstate );
	wire::ReadDeltaEntity( msg, &nullstate, &Client().baselines[number], number );
	Client().baselineUsed[static_cast<std::size_t>( number )] = true;
}

/* A demo's gamestate: each baseline the server named, as the same delta
 * from the null entity it was read from. */
void CL_MOHAAWriteBaselines( msg_t *msg ) {
	State &s = Client();
	EntityState nullstate;
	wire::NullEntityState( nullstate );
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_GENTITIES; i++ ) {
		if ( !s.baselineUsed[static_cast<std::size_t>( i )] ) {
			continue;
		}
		MSG_WriteByte( msg, svc_baseline );
		wire::WriteDeltaEntity( msg, &nullstate, &s.baselines[i], true );
	}
}

/*
==================
CL_MOHAAParseSnapshot

1.00's snapshot: the server time and its residual, the delta frame, the
flags, the area mask, the player state, the entities and the sounds. A
snapshot delta'd from a frame the client no longer holds is read to its end
and dropped, and the next move asks for an uncompressed one.
==================
*/
void CL_MOHAAParseSnapshot( msg_t *msg ) {
	State &s = Client();
	std::unique_ptr<Frame> parsed( new Frame );
	Frame &newSnap = *parsed;
	const Frame *old = nullptr;

	newSnap.serverCommandNum = clc.serverCommandSequence;
	newSnap.serverTime = MSG_ReadLong( msg );
	newSnap.serverTimeResidual = MSG_ReadByte( msg );

	// if we were just unpaused, we can only *now* really let the
	// change come into effect or the client hangs.
	cl_paused->modified = qfalse;

	newSnap.messageNum = clc.serverMessageSequence;
	const int deltaNum = MSG_ReadByte( msg );
	newSnap.deltaNum = deltaNum
		? fnql::net::RetreatSequence( newSnap.messageNum, static_cast<std::uint32_t>( deltaNum ) )
		: -1;
	newSnap.snapFlags = MSG_ReadByte( msg );

	if ( !deltaNum ) {
		newSnap.valid = true;	// uncompressed frame
		clc.demowaiting = qfalse;
	} else {
		old = &s.frames[newSnap.deltaNum & PACKET_MASK];
		if ( !old->valid ) {
			Com_Printf( "Delta from invalid frame (not supposed to happen!).\n" );
			Com_DPrintf( "  snapshot %i deltas from %i; that frame holds %i\n", newSnap.messageNum,
				newSnap.deltaNum, old->messageNum );
		} else if ( old->messageNum != newSnap.deltaNum ) {
			Com_Printf( "Delta frame too old.\n" );
			Com_DPrintf( "  snapshot %i deltas from %i; that frame holds %i\n", newSnap.messageNum,
				newSnap.deltaNum, old->messageNum );
		} else if ( s.parseEntitiesNum - old->parseEntitiesNum > kParseEntities - FNTECH3_MOHAA_MAX_SNAPSHOT_ENTITIES ) {
			Com_Printf( "Delta parseEntitiesNum too old.\n" );
		} else {
			newSnap.valid = true;
		}
	}

	const int areabytes = MSG_ReadByte( msg );
	if ( areabytes < 0 || areabytes > static_cast<int>( sizeof( newSnap.areamask ) ) ) {
		Com_Error( ERR_DROP, "CL_ParseSnapshot: Invalid size %d for areamask", areabytes );
		return;
	}
	MSG_ReadData( msg, newSnap.areamask, areabytes );

	wire::ReadDeltaPlayerstate( msg, old ? &old->ps : nullptr, &newSnap.ps );
	ParsePacketEntities( msg, old, newSnap );
	if ( !wire::ReadSounds( msg, newSnap.sounds.data(), newSnap.numSounds ) ) {
		Com_Error( ERR_DROP, "CL_ParseSnapshot: an illegible sound block" );
		return;
	}
	if ( msg->readcount > msg->cursize ) {
		Com_Error( ERR_DROP, "CL_ParseSnapshot: read past the end of the message" );
		return;
	}

	if ( !newSnap.valid ) {
		return;
	}

	// clear the valid flags of any snapshots between the last received and
	// this one, so a dropped packet cannot look like something to delta from
	const std::uint32_t gap = fnql::net::SequenceDistance( newSnap.messageNum, s.snap.messageNum );
	const int missing = static_cast<int>( std::min<std::uint32_t>( gap > 0 ? gap - 1u : 0u, PACKET_BACKUP - 1u ) );
	for ( int i = 1; i <= missing; i++ ) {
		s.frames[fnql::net::RetreatSequence( newSnap.messageNum, static_cast<std::uint32_t>( i ) ) & PACKET_MASK].valid = false;
	}

	if ( s.snap.valid && ( ( newSnap.snapFlags ^ s.snap.snapFlags ) & kSnapflagServerCount ) ) {
		s.serverStartTime = newSnap.serverTime;
	}

	s.snap = newSnap;
	s.snap.ping = 999;
	for ( int i = 0; i < PACKET_BACKUP; i++ ) {
		const int packetNum = fnql::net::RetreatSequence(
			clc.netchan.outgoingSequence, static_cast<std::uint32_t>( i + 1 ) ) & PACKET_MASK;
		if ( s.snap.ps.commandTime - cl.outPackets[packetNum].p_serverTime >= 0 ) {
			s.snap.ping = cls.realtime - cl.outPackets[packetNum].p_realtime;
			break;
		}
	}
	s.frames[s.snap.messageNum & PACKET_MASK] = s.snap;

	if ( cl_shownet->integer == 3 ) {
		Com_Printf( "   snapshot:%i  delta:%i  ping:%i\n", s.snap.messageNum, s.snap.deltaNum, s.snap.ping );
	}

	MirrorEngineSnapshot( s.snap );
	cl.newSnapshots = qtrue;
	clc.eventMask |= EM_SNAPSHOT;
}

void CL_MOHAAParseCenterprint( msg_t *msg ) {
	Print &print = Client().print;
	ReadString( msg, print.text, sizeof( print.text ) );
	print.located = false;
	print.time = cls.realtime;
}

void CL_MOHAAParseLocationprint( msg_t *msg ) {
	Print &print = Client().print;
	print.x = MSG_ReadShort( msg );
	print.y = MSG_ReadShort( msg );
	ReadString( msg, print.text, sizeof( print.text ) );
	print.located = true;
	print.time = cls.realtime;
}

/* Only the client game knows where a client-game message ends, so it reads
 * the message itself through the MSG_Read imports. */
void CL_MOHAAParseCGameMessage( msg_t *msg ) {
	if ( !cgvm ) {
		Com_Error( ERR_DROP, "CL_ParseServerMessage: tried to parse cg message without cgame loaded" );
		return;
	}
	State &s = Client();
	s.currentMessage = msg;
	CL_MOHAACGameParseMessage();
	s.currentMessage = nullptr;
}

msg_t *CL_MOHAACurrentMessage( void ) {
	return Client().currentMessage;
}

/*
==================
CL_MOHAAGetSnapshot

A frame the client still holds, handed to the client game with each
attached entity after its parent, as OpenMoHAA's CL_GetSnapshot orders them:
the client game attaches a child to its parent's pose as it walks the list.
==================
*/
bool CL_MOHAAGetSnapshot( int snapshotNumber, Snapshot *snapshot ) {
	State &s = Client();

	if ( !snapshot ) {
		return false;
	}
	if ( snapshotNumber - s.snap.messageNum > 0 ) {
		Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.snapshot.messageNum" );
		return false;
	}
	// if the frame has fallen out of the circular buffer, we can't return it
	if ( s.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return false;
	}
	const Frame &frame = s.frames[snapshotNumber & PACKET_MASK];
	if ( !frame.valid ) {
		return false;
	}
	if ( s.parseEntitiesNum - frame.parseEntitiesNum >= kParseEntities ) {
		return false;
	}

	snapshot->snapFlags = frame.snapFlags;
	snapshot->serverCommandSequence = frame.serverCommandNum;
	snapshot->ping = frame.ping;
	snapshot->serverTime = frame.serverTime;
	std::memcpy( snapshot->areamask, frame.areamask, sizeof( snapshot->areamask ) );
	snapshot->ps = frame.ps;
	snapshot->numServerCommands = 0;

	const int sounds = std::clamp( frame.numSounds, 0, static_cast<int>( FNTECH3_MOHAA_MAX_SERVER_SOUNDS ) );
	snapshot->number_of_sounds = sounds;
	std::copy_n( frame.sounds.begin(), sounds, snapshot->sounds );

	const int count = std::min( frame.numEntities, static_cast<int>( FNTECH3_MOHAA_MAX_SNAPSHOT_ENTITIES ) );
	if ( frame.numEntities > count ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i visible entities to %i\n", frame.numEntities, count );
	}

	/* -2 once an entity is out, its parent's number while it waits. */
	std::array<int, FNTECH3_MOHAA_MAX_GENTITIES> parents;
	parents.fill( -1 );
	snapshot->numEntities = 0;
	for ( int i = 0; i < count; i++ ) {
		const EntityState &entity = ParseEntity( frame.parseEntitiesNum + i );
		const int number = entity.number & ( FNTECH3_MOHAA_MAX_GENTITIES - 1 );
		const int parent = entity.parent;
		if ( parent == FNTECH3_MOHAA_ENTITYNUM_NONE || parent < 0 || parent >= FNTECH3_MOHAA_MAX_GENTITIES ) {
			parents[number] = -2;
		} else if ( parents[parent] == -2 ) {
			parents[number] = -2;
		} else {
			parents[number] = parent;
			continue;
		}
		snapshot->entities[snapshot->numEntities++] = entity;
	}
	for ( int pass = 0; pass < 8 && snapshot->numEntities != count; pass++ ) {
		for ( int i = 0; i < count; i++ ) {
			const EntityState &entity = ParseEntity( frame.parseEntitiesNum + i );
			const int number = entity.number & ( FNTECH3_MOHAA_MAX_GENTITIES - 1 );
			const int parent = parents[number];
			if ( parent >= 0 && parents[parent] == -2 ) {
				parents[number] = -2;
				snapshot->entities[snapshot->numEntities++] = entity;
			}
		}
	}
	if ( snapshot->numEntities != count ) {
		/* An entity whose parent is not in the frame follows the rest. */
		Com_DPrintf( "CL_GetSnapshot: Not all children could find their parents.\n" );
		for ( int i = 0; i < count && snapshot->numEntities < count; i++ ) {
			const EntityState &entity = ParseEntity( frame.parseEntitiesNum + i );
			const int number = entity.number & ( FNTECH3_MOHAA_MAX_GENTITIES - 1 );
			if ( parents[number] >= 0 ) {
				parents[number] = -2;
				snapshot->entities[snapshot->numEntities++] = entity;
			}
		}
	}
	return true;
}

void CL_MOHAACurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	const State &s = Client();
	if ( snapshotNumber ) {
		*snapshotNumber = s.snap.messageNum;
	}
	if ( serverTime ) {
		*serverTime = s.snap.serverTime;
	}
}

int CL_MOHAAServerStartTime( void ) {
	return Client().serverStartTime;
}

const PlayerState *CL_MOHAAPlayerState( void ) {
	const State &s = Client();
	return s.snap.valid ? &s.snap.ps : nullptr;
}

/*
==================
CL_MOHAAWriteEyeInfo

1.00's move carries where the player's eyes are: the client game's eye
offset, rounded into a signed byte a component, and its eye pitch and yaw
(OpenMoHAA CL_EyeInfo). Before the client game is up the view angles stand
in.

The server reads it as a delta from the last it received, and 1.00's client
writes it as a delta from the packet it duplicates from, so the two bases
part whenever a packet is dropped or duplicated. Every member is therefore
written whole: the delta is taken from a base that differs from the record
in each member, which any 1.00 reader decodes to the record itself.
==================
*/
void CL_MOHAAWriteEyeInfo( msg_t *msg ) {
	UserEyes eyes{};
	if ( cgvm ) {
		float offset[3], angles[3];
		CL_MOHAACGameEyeOffset( offset );
		CL_MOHAACGameEyeAngles( angles );
		for ( int i = 0; i < 3; i++ ) {
			const float rounded = std::clamp( offset[i] + 0.5f, -127.0f, 128.0f );
			eyes.ofs[i] = static_cast<std::int8_t>( static_cast<int>( rounded ) );
		}
		eyes.angles[0] = angles[0];
		eyes.angles[1] = angles[1];
	} else {
		eyes.angles[0] = cl.viewangles[PITCH];
		eyes.angles[1] = cl.viewangles[YAW];
	}

	UserEyes base = eyes;
	for ( int i = 0; i < 3; i++ ) {
		base.ofs[i] = static_cast<std::int8_t>( eyes.ofs[i] ^ 1 );
	}
	base.angles[0] = eyes.angles[0] + 1.0f;
	base.angles[1] = eyes.angles[1] + 1.0f;
	wire::WriteDeltaEyeInfo( msg, base, eyes );
	Client().sentEyes[clc.netchan.outgoingSequence & PACKET_MASK] = eyes;
}

void CL_MOHAACommandFromEngine( const usercmd_t &in, UserCmd &out ) {
	wire::CommandFromEngine( in, out );
}

void CL_MOHAAWriteDeltaUsercmdKey( msg_t *msg, int key, const usercmd_t &from, const usercmd_t &to ) {
	UserCmd base, sent;
	wire::CommandFromEngine( from, base );
	wire::CommandFromEngine( to, sent );
	wire::WriteDeltaUsercmdKey( msg, key, base, sent );
}

void CL_MOHAADrawPrints( void ) {
	DrawPrint( Client().print );
}

/*
==================
CL_MOHAADrawFades

The screen's fade and a cinematic's letterbox, which 2015's engine draws over
the view and under the client game's 2D (OpenMoHAA View3D::Draw2D, reference
only): the player state's blend colour across the screen, added when the
add-fade stat is set, then black bars at the top and the bottom, each the
letterbox stat's share of the screen's height. A level's `fadeout`, `fadein`
and `letterbox` reach the screen only through these.
==================
*/
void CL_MOHAADrawFades( void ) {
	const PlayerState *ps = CL_MOHAAPlayerState();
	if ( !ps ) {
		return;
	}
	const float width = static_cast<float>( cls.glconfig.vidWidth );
	const float height = static_cast<float>( cls.glconfig.vidHeight );
	if ( ps->blend[3] > 0.0f ) {
		float color[4];
		for ( int i = 0; i < 4; i++ ) {
			color[i] = std::clamp( ps->blend[i], 0.0f, 1.0f );
		}
		/* 2015's own "white" is additive (scripts/sprites.shader). */
		const qhandle_t shader = ps->stats[fntech3::mohaa::kStatAddFade]
			? re.RegisterShaderNoMip( "white" ) : cls.whiteShader;
		re.SetColor( color );
		re.DrawStretchPic( 0.0f, 0.0f, width, height, 0.0f, 0.0f, 1.0f, 1.0f, shader );
	}
	const float fraction = std::clamp( static_cast<float>( ps->stats[fntech3::mohaa::kStatLetterbox] )
		/ static_cast<float>( fntech3::mohaa::kLetterboxScale ), 0.0f, 1.0f );
	if ( fraction > 0.0f ) {
		static const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		const float bar = height * fraction;
		re.SetColor( black );
		re.DrawStretchPic( 0.0f, 0.0f, width, bar, 0.0f, 0.0f, 1.0f, 1.0f, cls.whiteShader );
		re.DrawStretchPic( 0.0f, height - bar, width, bar, 0.0f, 0.0f, 1.0f, 1.0f, cls.whiteShader );
	}
	re.SetColor( nullptr );
}

bool CL_MOHAALetterboxActive( void ) {
	const PlayerState *ps = CL_MOHAAPlayerState();
	return ps && ps->stats[fntech3::mohaa::kStatLetterbox] > 0;
}

/*
==================
GameSpy's LAN search

1.00's Join LAN Game list asks the game ports with `getinfo` (its Refresh
runs `localservers`, ui/joinlangame.urc), and so finds a server whose game
port is one of the four the engine scans. Beside that, fnTech3's list makes
GameSpy's LAN search as the SDK's browser made it **[O]**
(code/compat/gamespy_query.hpp): `\status\` broadcast once to each port from
12300 to 12316, from a socket of its own, the answers taken for three
seconds. 1.00's servers answered GameSpy's browsers there (Start New
Server's GameSpy box, `ui_gamespy`), so a server is found either way, and
one whose game port lies beyond the four is found as well. An answer is
judged as OpenMoHAA's browser judges one **[O]** and listed only when 1.00
can join it: GameSpy's `mohaa` at 1.00, at its `hostport`. A server that
answers on its game port is listed from that answer, whichever comes first,
so GameSpy's answers only add. No master is asked anything: GameSpy's are
gone.
==================
*/
namespace {

fntech3::gamespy::LanAnswers lanAnswers;
/* The local servers a GameSpy answer listed. */
std::bitset<MAX_OTHER_SERVERS> lanListedByGameSpy;
int lanSearchStarted;

void GameSpyLanAnswer( const netadr_t *from, const byte *data, int length ) {
	namespace mohaa = fntech3::gamespy::mohaa;
	if ( !Com_GameIsMOHAA() || cls.pingUpdateSource != AS_LOCAL || !from || from->type != NA_IP || !data
		|| length <= 0 ) {
		return;
	}
	const std::string source = NET_AdrToStringwPort( from );
	const fntech3::gamespy::LanAnswers::Answer *answer =
		lanAnswers.Add( source, reinterpret_cast<const char *>( data ), static_cast<std::size_t>( length ) );
	if ( !answer ) {
		return;
	}
	mohaa::LanServer server;
	const mohaa::LanVerdict verdict = mohaa::JudgeLanAnswer( *answer, Com_GameDefaultServerPort(), server );
	if ( verdict != mohaa::LanVerdict::Listed ) {
		Com_DPrintf( "GameSpy LAN answer from %s not listed: %s (gamever %s)\n", source.c_str(),
			mohaa::LanVerdictName( verdict ), server.version.c_str() );
		return;
	}
	netadr_t address = *from;
	address.port = BigShort( static_cast<short>( server.hostPort ) );
	int i;
	for ( i = 0; i < MAX_OTHER_SERVERS; i++ ) {
		if ( cls.localServers[i].adr.port == 0 ) {
			break;
		}
		if ( NET_CompareAdr( &address, &cls.localServers[i].adr ) ) {
			return;
		}
	}
	if ( i == MAX_OTHER_SERVERS ) {
		Com_DPrintf( "MAX_OTHER_SERVERS hit, dropping GameSpy answer\n" );
		return;
	}
	/* As CL_InitServerInfo and CL_SetServerInfo leave a getinfo answer's
	 * entry, its visibility untouched. */
	serverInfo_t &entry = cls.localServers[i];
	entry.adr = address;
	Q_strncpyz( entry.hostName, server.hostName.c_str(), sizeof( entry.hostName ) );
	Q_strncpyz( entry.mapName, server.mapName.c_str(), sizeof( entry.mapName ) );
	entry.game[0] = '\0';
	entry.netType = 0;
	entry.gameType = server.gameType;
	entry.clients = server.players;
	entry.maxClients = server.maxPlayers;
	entry.minPing = 0;
	entry.maxPing = 0;
	entry.ping = Sys_Milliseconds() - lanSearchStarted;
	entry.punkbuster = 0;
	entry.g_humanplayers = 0;
	entry.g_needpass = 0;
	cls.numlocalservers = i + 1;
	lanListedByGameSpy.set( static_cast<std::size_t>( i ) );
	Com_DPrintf( "%s listed from GameSpy's answer from %s: %s\n", NET_AdrToStringwPort( &address ), source.c_str(),
		entry.hostName );
}

} // namespace

void CL_MOHAAGameSpyLanSearch( void ) {
	namespace gamespy = fntech3::gamespy;
	lanAnswers.Clear();
	lanListedByGameSpy.reset();
	const int port = NET_OpenSearchSocket( gamespy::LanSearchMilliseconds, GameSpyLanAnswer );
	if ( !port ) {
		Com_DPrintf( "No GameSpy LAN search: no IPv4 socket for it.\n" );
		return;
	}
	lanSearchStarted = Sys_Milliseconds();
	netadr_t to{};
	to.type = NA_BROADCAST;
	const int length = static_cast<int>( std::strlen( gamespy::StatusQuery ) );
	for ( int queryPort = gamespy::mohaa::LanSearchFirstPort; queryPort <= gamespy::mohaa::LanSearchLastPort;
		queryPort++ ) {
		to.port = BigShort( static_cast<short>( queryPort ) );
		NET_SendSearchPacket( length, gamespy::StatusQuery, &to );
	}
	Com_DPrintf( "GameSpy LAN search from UDP port %i to ports %i-%i\n", port, gamespy::mohaa::LanSearchFirstPort,
		gamespy::mohaa::LanSearchLastPort );
}

bool CL_MOHAAGameSpyTakeOver( int index ) {
	if ( index < 0 || index >= MAX_OTHER_SERVERS || !lanListedByGameSpy.test( static_cast<std::size_t>( index ) ) ) {
		return false;
	}
	lanListedByGameSpy.reset( static_cast<std::size_t>( index ) );
	return true;
}
