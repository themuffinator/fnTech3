/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) the OpenMoHAA team
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3. Its snapshot walk, sound queue and client-game
message buffers are adapted from OpenMoHAA's code/server/sv_snapshot.c,
sv_snd.c and sv_game.c (https://github.com/openmoh/openmohaa), which are
distributed under the GNU General Public License, version 2 or (at your
option) any later version, and remain under that licence here.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

fnTech3 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.
===========================================================================
*/
#ifndef FNTECH3_SERVER_MOHAA_NET_HPP
#define FNTECH3_SERVER_MOHAA_NET_HPP

/* Medal of Honor: Allied Assault's server half of protocol 6.
 *
 * Its clients are sent the game's own entity and player records, delta-coded
 * by code/compat/mohaa_message_codec.hpp, so the snapshots are kept here in
 * that form rather than in the server's Quake III frames: each client's last
 * PACKET_BACKUP frames and a ring of the entity states they name. Beside
 * them are what the game queues for a client between snapshots -- sounds, a
 * centre or located print, and client-game messages -- which go out with the
 * next snapshot and are then cleared, as 2015's server does.
 *
 * Which entities a client sees follows OpenMoHAA's walk, without the later
 * expansions' additions (the far-plane cull, the radar and the visibility
 * traces that hide players behind walls): 1.00's rules are the entity
 * flags, its parent's, the areas and the PVS, the sky portal, and portal
 * entities' second views. */

#include "../compat/mohaa_message_codec.hpp"
#include "../qcommon/netchan_safety.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace mohaa_net {

using fntech3::mohaa::EntityState;
using fntech3::mohaa::GEntity;
using fntech3::mohaa::PlayerState;
using fntech3::mohaa::ServerSound;
using fntech3::mohaa::UserCmd;
using fntech3::mohaa::UserEyes;

/* 2015's server-only entity flags (OpenMoHAA g_public.h). */
constexpr int kSvfNoClient = 1 << 0;
constexpr int kSvfBroadcast = 1 << 2;
constexpr int kSvfPortal = 1 << 3;
constexpr int kSvfSendPvs = 1 << 4;
constexpr int kSvfNotSingleClient = 1 << 6;
constexpr int kSvfSingleClient = 1 << 8;
constexpr int kSvfSendOnce = 1 << 14;
constexpr int kSvfSent = 1 << 15;

/* Render flags the walk writes into a state (OpenMoHAA q_shared.h). */
constexpr int kRfSkyOrigin = 1 << 13;
constexpr int kRfSkyEntity = 1 << 14;
constexpr int kRfShadowPlane = 1 << 20;
constexpr int kRfWrapFrames = 1 << 21;
constexpr int kRfAlwaysDraw = 1 << 26;

/* A loop sound this close carries across the whole level. */
constexpr float kLevelWideMinDist = 10000.0f;

/* 1.00's camera-view movement flag: 2.0's bit 7, which protocol 6 carries
 * two places higher (OpenMoHAA bg_compat.cpp). */
constexpr int kPmfCameraView = 1 << 9;

constexpr int kSnapflagRateDelayed = 1;
constexpr int kSnapflagNotActive = 2;

constexpr int kMaxSnapshotEntities = FNTECH3_MOHAA_MAX_SNAPSHOT_ENTITIES;

/* The opcodes past Quake III's (OpenMoHAA qcommon.h svc_ops_e). */
constexpr int kSvcSnapshot = 7;
constexpr int kSvcCenterprint = 8;
constexpr int kSvcLocprint = 9;
constexpr int kSvcCgameMessage = 10;
constexpr int kSvcNop = 1;

struct Frame {
	PlayerState ps;
	int numEntities;
	int firstEntity;
	int areabytes;
	byte areabits[MAX_MAP_AREA_BYTES];
};

/* A client-game message buffer: the values the game wrote and how many bits
 * each takes, replayed into the snapshot's message (OpenMoHAA cgm_t). */
constexpr int kCgmDataSize = 4096;
constexpr int kCgmDatatypesSize = 8192;
/* Past this much a buffer takes no new message (OpenMoHAA's 3,968). */
constexpr int kCgmFull = 3968;

struct ClientState {
	Frame frames[PACKET_BACKUP];
	UserCmd lastCmd;
	UserEyes lastEyes;
	ServerSound sounds[FNTECH3_MOHAA_MAX_SERVER_SOUNDS];
	int numSounds;
	char print[256];
	bool locationPrint;
	int printX, printY;
	byte cgmData[kCgmDataSize];
	byte cgmTypes[kCgmDatatypesSize];
	int cgmSize;
	int cgmTypesUsed;
	bool cgmReceive;
	int cgmOverflowReport;
};

std::vector<ClientState> clients;
std::vector<EntityState> snapshotEntities;
int nextSnapshotEntities;

ClientState *State( int clientNum ) {
	if ( clientNum < 0 || clientNum >= (int)clients.size() ) {
		return nullptr;
	}
	return &clients[(std::size_t)clientNum];
}

ClientState *State( const client_t *client ) {
	return State( SV_ClientIndex( client ) );
}

/* Sized for this level's client slots; every client starts with nothing
 * queued and no frame to delta from. */
void Reset() {
	const int count = sv_maxclients ? sv_maxclients->integer : 1;
	clients.assign( (std::size_t)( count > 0 ? count : 1 ), ClientState{} );
	const std::size_t ring = (std::size_t)( count > 0 ? count : 1 ) * PACKET_BACKUP * 256;
	snapshotEntities.assign( ring, EntityState{} );
	nextSnapshotEntities = 0;
}

void ResetClient( int clientNum ) {
	ClientState *state = State( clientNum );
	if ( state ) {
		*state = ClientState{};
	}
}

/* ---- sounds and prints ------------------------------------------------- */

/* OpenMoHAA sv_snd.c SV_Sound: every active client hears it; where it plays
 * is the client game's to decide. */
void QueueSound( const float *origin, int entnum, int channel, int soundIndex, float volume, float minDist,
	float pitch, float maxDist, int streamed ) {
	for ( int i = 0; i < (int)clients.size() && i < sv.maxclients; i++ ) {
		if ( SV_ClientForIndex( i ).state != CS_ACTIVE ) {
			continue;
		}
		ClientState &state = clients[(std::size_t)i];
		if ( state.numSounds >= FNTECH3_MOHAA_MAX_SERVER_SOUNDS ) {
			continue;
		}
		ServerSound &sound = state.sounds[state.numSounds++];
		std::memset( &sound, 0, sizeof( sound ) );
		sound.stop_flag = qfalse;
		sound.entity_number = entnum;
		if ( origin ) {
			VectorCopy( origin, sound.origin );
		}
		sound.channel = channel;
		sound.volume = volume;
		sound.min_dist = minDist;
		sound.pitch = pitch;
		sound.maxDist = maxDist;
		sound.sound_index = (std::int16_t)soundIndex;
		sound.streamed = streamed;
	}
}

void QueueStopSound( int entnum, int channel ) {
	for ( int i = 0; i < (int)clients.size() && i < sv.maxclients; i++ ) {
		if ( SV_ClientForIndex( i ).state != CS_ACTIVE ) {
			continue;
		}
		ClientState &state = clients[(std::size_t)i];
		if ( state.numSounds >= FNTECH3_MOHAA_MAX_SERVER_SOUNDS ) {
			continue;
		}
		ServerSound &sound = state.sounds[state.numSounds++];
		std::memset( &sound, 0, sizeof( sound ) );
		sound.stop_flag = qtrue;
		sound.entity_number = entnum;
		sound.channel = channel;
	}
}

void QueuePrint( int clientNum, const char *text, bool location, int x, int y ) {
	ClientState *state = State( clientNum );
	if ( !state || !text ) {
		return;
	}
	Q_strncpyz( state->print, text, sizeof( state->print ) );
	state->locationPrint = location;
	state->printX = x;
	state->printY = y;
}

/* ---- client-game messages ---------------------------------------------- */

void ReportCGMOverflow( ClientState &state, int clientNum, bool datum ) {
	if ( !state.cgmOverflowReport || state.cgmOverflowReport < sv.time ) {
		Com_DPrintf( datum ? "CGM buffer for client %i overflowed number of datum\n"
			: "CGM buffer for client %i overflowed size of data\n", clientNum );
		state.cgmOverflowReport = sv.time + 5000;
	}
}

void WriteCGMBits( ClientState &state, int clientNum, int value, int bits ) {
	if ( kCgmDataSize - state.cgmSize < 4 ) {
		ReportCGMOverflow( state, clientNum, false );
		return;
	}
	if ( kCgmDatatypesSize - state.cgmTypesUsed < 1 ) {
		ReportCGMOverflow( state, clientNum, true );
		return;
	}
	if ( bits > 32 ) {
		Com_Error( ERR_DROP, "PF_MSG_WriteBits: bad bits %i", bits );
		return;
	}
	if ( bits < 0 ) {
		bits = -bits;
	}

	if ( bits <= 8 ) {
		state.cgmData[state.cgmSize++] = (byte)value;
	} else if ( bits <= 16 ) {
		const short v = (short)value;
		std::memcpy( &state.cgmData[state.cgmSize], &v, sizeof( v ) );
		state.cgmSize += 2;
	} else {
		std::memcpy( &state.cgmData[state.cgmSize], &value, sizeof( value ) );
		state.cgmSize += 4;
	}
	state.cgmTypes[state.cgmTypesUsed++] = (byte)bits;
}

/* Every selected client's buffer takes the value. */
void CGMWrite( int value, int bits ) {
	for ( int i = 0; i < (int)clients.size(); i++ ) {
		ClientState &state = clients[(std::size_t)i];
		if ( state.cgmReceive ) {
			WriteCGMBits( state, i, value, bits );
		}
	}
}

/* OpenMoHAA PF_MSG_StartCGM: a block begins with the opcode, and each
 * further message in it with a one bit; then the message's type in six. */
void CGMStart( int type ) {
	for ( int i = 0; i < (int)clients.size(); i++ ) {
		ClientState &state = clients[(std::size_t)i];
		if ( !state.cgmReceive ) {
			continue;
		}
		if ( state.cgmSize <= 0 ) {
			WriteCGMBits( state, i, kSvcCgameMessage, 8 );
		} else {
			WriteCGMBits( state, i, 1, 1 );
		}
		WriteCGMBits( state, i, type, 6 );
	}
}

void CGMEnd() {
	for ( ClientState &state : clients ) {
		state.cgmReceive = false;
	}
}

void CGMSetClient( int clientNum ) {
	CGMEnd();
	ClientState *state = State( clientNum );
	if ( state && state->cgmSize <= kCgmFull - 1 ) {
		state->cgmReceive = true;
	}
}

bool ClientCanReceive( int i, ClientState &state ) {
	const client_t &client = SV_ClientForIndex( i );
	if ( client.state == CS_FREE || !client.gentity ) {
		return false;
	}
	const GEntity *ent = mohaa_world::Entity( i );
	if ( !ent || !ent->inuse ) {
		return false;
	}
	if ( state.cgmSize >= kCgmFull ) {
		ReportCGMOverflow( state, i, false );
		return false;
	}
	return true;
}

void CGMBroadcastAll() {
	for ( int i = 0; i < (int)clients.size() && i < sv.maxclients; i++ ) {
		ClientState &state = clients[(std::size_t)i];
		if ( ClientCanReceive( i, state ) ) {
			state.cgmReceive = true;
		}
	}
}

bool ClusterVisible( const byte *pvs, int cluster ) {
	return !pvs || cluster < 0 || ( pvs[cluster >> 3] & ( 1 << ( cluster & 7 ) ) );
}

/* OpenMoHAA MSG_SetBroadcastVisible: every client that can see either point. */
void CGMBroadcastVisible( const float *pos, const float *posB ) {
	if ( !pos ) {
		return;
	}
	const int posLeaf = CM_PointLeafnum( pos );
	const int posArea = CM_LeafArea( posLeaf );
	const int posCluster = CM_LeafCluster( posLeaf );
	int posBArea = 0, posBCluster = 0;
	if ( posB ) {
		const int leaf = CM_PointLeafnum( posB );
		posBArea = CM_LeafArea( leaf );
		posBCluster = CM_LeafCluster( leaf );
	}

	for ( int i = 0; i < (int)clients.size() && i < sv.maxclients; i++ ) {
		ClientState &state = clients[(std::size_t)i];
		const client_t &client = SV_ClientForIndex( i );
		if ( client.state == CS_FREE || !client.gentity ) {
			continue;
		}
		const GEntity *ent = mohaa_world::Entity( i );
		if ( !ent || !ent->inuse ) {
			continue;
		}
		const int leaf = CM_PointLeafnum( ent->s.origin );
		const int area = CM_LeafArea( leaf );
		const byte *pvs = CM_ClusterPVS( CM_LeafCluster( leaf ) );

		if ( !CM_AreasConnected( area, posArea ) && ( !posB || !CM_AreasConnected( area, posBArea ) ) ) {
			continue;
		}
		if ( !ClusterVisible( pvs, posCluster ) && ( !posB || !ClusterVisible( pvs, posBCluster ) ) ) {
			continue;
		}
		if ( state.cgmSize >= kCgmFull ) {
			ReportCGMOverflow( state, i, false );
			continue;
		}
		state.cgmReceive = true;
	}
}

/* Replayed into the client's message after its snapshot, closed by a zero
 * bit (OpenMoHAA SV_WriteCGMToClient). */
void WriteCGMToClient( ClientState &state, int clientNum, msg_t *msg ) {
	if ( state.cgmSize <= 0 ) {
		return;
	}
	WriteCGMBits( state, clientNum, 0, 1 );

	const byte *data = state.cgmData;
	for ( int i = 0; i < state.cgmTypesUsed; i++ ) {
		const int bits = state.cgmTypes[i];
		if ( bits <= 8 ) {
			MSG_WriteBits( msg, *data, bits );
			data += 1;
		} else if ( bits <= 16 ) {
			short v;
			std::memcpy( &v, data, sizeof( v ) );
			MSG_WriteBits( msg, v, bits );
			data += 2;
		} else {
			int v;
			std::memcpy( &v, data, sizeof( v ) );
			MSG_WriteBits( msg, v, bits );
			data += 4;
		}
	}
	state.cgmSize = 0;
	state.cgmTypesUsed = 0;
}

/* ---- the snapshot walk ------------------------------------------------- */

struct EntityNumbers {
	int count = 0;
	int numbers[kMaxSnapshotEntities];
};

void AddEntity( svEntity_t *svEnt, GEntity *ent, EntityNumbers &numbers, bool portal, bool portalSky ) {
	// if we have already added this entity to this snapshot, don't add again
	if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
		ent->s.renderfx &= ~( kRfShadowPlane | kRfWrapFrames );
		ent->s.renderfx |= kRfWrapFrames;
		return;
	}
	svEnt->snapshotCounter = sv.snapshotCounter;

	// if we are full, silently discard entities
	if ( numbers.count == kMaxSnapshotEntities ) {
		return;
	}

	ent->s.renderfx &= ~( kRfShadowPlane | kRfWrapFrames | kRfSkyEntity );
	if ( portal ) {
		ent->s.renderfx |= kRfShadowPlane;
	} else if ( portalSky ) {
		ent->s.renderfx |= kRfSkyEntity;
	}

	ent->r.lastNetTime = sv.time;
	numbers.numbers[numbers.count++] = ent->s.number;
}

void AddEntitiesVisibleFromPoint( const vec3_t origin, Frame &frame, EntityNumbers &numbers, svEntity_t *portalEnt,
	bool portalSky ) {
	if ( !sv.state ) {
		return;
	}

	const int leafnum = CM_PointLeafnum( origin );
	const int clientarea = CM_LeafArea( leafnum );
	const int clientcluster = CM_LeafCluster( leafnum );

	// calculate the visible areas
	frame.areabytes = CM_WriteAreaBits( frame.areabits, clientarea );

	const byte *clientpvs = CM_ClusterPVS( clientcluster );
	const int count = mohaa_game::exports ? (int)mohaa_game::exports[mohaa_game::GE_num_entities] : 0;
	GEntity *skyorigin = nullptr;

	for ( int e = 0; e < count && e < MAX_GENTITIES; e++ ) {
		GEntity *ent = mohaa_world::Entity( e );
		if ( !ent || !ent->inuse ) {
			continue;
		}

		// mark the entity as sent
		ent->r.svFlags |= kSvfSent;

		if ( !ent->r.linked ) {
			continue;
		}
		if ( ent->r.svFlags & kSvfNoClient ) {
			continue;
		}
		if ( ( ent->r.svFlags & kSvfSingleClient ) && ent->r.singleClient != frame.ps.clientNum ) {
			continue;
		}
		if ( ( ent->r.svFlags & kSvfNotSingleClient ) && ent->r.singleClient == frame.ps.clientNum ) {
			continue;
		}

		GEntity *parentEnt = nullptr;
		if ( ent->s.parent != FNTECH3_MOHAA_ENTITYNUM_NONE ) {
			parentEnt = mohaa_world::Entity( ent->s.parent );
			// parents that will not send to clients will be skipped
			if ( parentEnt && ( parentEnt->r.svFlags & kSvfNoClient ) ) {
				continue;
			}
		}

		svEntity_t *svEnt = &sv.svEntities[e];

		// don't double add an entity through portals
		if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
			continue;
		}

		if ( ent->s.renderfx & kRfSkyOrigin ) {
			if ( mohaa_game::skyPortal && !portalSky && !portalEnt ) {
				if ( skyorigin ) {
					Com_Error( ERR_DROP, "SV_AddEntitiesVisibleFromPoint: duplicate sky origin" );
					return;
				}
				skyorigin = ent;
				AddEntity( svEnt, ent, numbers, false, false );
			}
			continue;
		}

		// broadcast entities are always sent, and those sent once
		if ( ( ent->r.svFlags & kSvfBroadcast ) || ( ent->r.svFlags & kSvfSendOnce ) ) {
			AddEntity( svEnt, ent, numbers, portalEnt != nullptr, portalSky );
			continue;
		}

		svEntity_t *svCheckEnt = svEnt;
		if ( parentEnt ) {
			svEntity_t *parentSvEnt = &sv.svEntities[ent->s.parent];
			if ( parentSvEnt->snapshotCounter == sv.snapshotCounter ) {
				AddEntity( svEnt, ent, numbers, portalEnt != nullptr, portalSky );
				continue;
			}
			svCheckEnt = parentSvEnt;
		}

		if ( !( ent->r.svFlags & kSvfSendPvs ) && !ent->s.modelindex && !ent->s.loopSound ) {
			// don't send entities that have nothing to draw
			continue;
		}

		if ( ( ent->s.loopSound && ent->s.loopSoundMinDist == kLevelWideMinDist )
			|| ( ent->s.renderfx & kRfAlwaysDraw ) ) {
			// loop sound entities are sent regardless
			AddEntity( svEnt, ent, numbers, portalEnt != nullptr, portalSky );
			continue;
		}

		// check area
		if ( !CM_AreasConnected( clientarea, svCheckEnt->areanum ) ) {
			// doors can legally straddle two areas, so
			// we may need to check another one
			if ( !CM_AreasConnected( clientarea, svCheckEnt->areanum2 ) ) {
				continue; // blocked by a door
			}
		}

		// check individual leafs
		if ( !svEnt->numClusters ) {
			continue;
		}

		int l = 0;
		int i;
		for ( i = 0; i < svCheckEnt->numClusters; i++ ) {
			l = svCheckEnt->clusternums[i];
			if ( ClusterVisible( clientpvs, l ) ) {
				break;
			}
		}

		// if we haven't found it to be visible,
		// check overflow clusters that couldn't be stored
		if ( i == svCheckEnt->numClusters ) {
			if ( svCheckEnt->lastCluster ) {
				for ( ; l <= svCheckEnt->lastCluster; l++ ) {
					if ( ClusterVisible( clientpvs, l ) ) {
						break;
					}
				}
				if ( l == svCheckEnt->lastCluster ) {
					continue; // not visible
				}
			} else {
				continue;
			}
		}

		// add it
		AddEntity( svEnt, ent, numbers, portalEnt != nullptr, portalSky );

		// if it's a portal entity, add everything visible from its camera position
		if ( ( ent->r.svFlags & kSvfPortal ) && svEnt != portalEnt ) {
			AddEntitiesVisibleFromPoint( ent->s.origin2, frame, numbers, svEnt, false );
		}
	}

	if ( !portalSky && skyorigin && !portalEnt ) {
		AddEntitiesVisibleFromPoint( skyorigin->s.origin, frame, numbers, nullptr, true );
	}
}

void BuildClientSnapshot( client_t *client, ClientState &state ) {
	const int clientNum = SV_ClientIndex( client );
	Frame &frame = state.frames[client->netchan.outgoingSequence & PACKET_MASK];

	// bump the counter used to prevent double adding
	sv.snapshotCounter++;

	std::memset( frame.areabits, 0, sizeof( frame.areabits ) );
	frame.areabytes = 0;
	frame.numEntities = 0;
	frame.firstEntity = nextSnapshotEntities;

	GEntity *clent = mohaa_world::Entity( clientNum );
	if ( !clent || !client->gentity || client->state == CS_ZOMBIE || !mohaa_game::gameData.clients ) {
		std::memset( &frame.ps, 0, sizeof( frame.ps ) );
		return;
	}

	// grab the current player state
	const PlayerState *ps = reinterpret_cast<const PlayerState *>(
		static_cast<const byte *>( mohaa_game::gameData.clients ) + (std::ptrdiff_t)clientNum * mohaa_game::gameData.clientBytes );
	frame.ps = *ps;

	const int viewClient = frame.ps.clientNum;
	if ( viewClient < 0 || viewClient >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
		return;
	}

	// find the client's viewpoint
	vec3_t org;
	if ( ps->pm_flags & kPmfCameraView ) {
		VectorCopy( ps->camera_origin, org );
	} else {
		VectorCopy( ps->origin, org );
		org[2] += ps->viewheight;
	}

	EntityNumbers numbers;
	AddEntity( &sv.svEntities[clientNum], clent, numbers, false, false );

	// add all the entities directly visible to the eye, which
	// may include portal entities that merge other viewpoints
	AddEntitiesVisibleFromPoint( org, frame, numbers, nullptr, false );

	// a portal may have added entities out of order
	std::sort( numbers.numbers, numbers.numbers + numbers.count );
	for ( int i = 1; i < numbers.count; i++ ) {
		if ( numbers.numbers[i] == numbers.numbers[i - 1] ) {
			Com_Error( ERR_DROP, "SV_QsortEntityStates: duplicated entity" );
			return;
		}
	}

	// invert the areabits to make a mask vector, which is what the renderer wants
	for ( int i = 0; i < MAX_MAP_AREA_BYTES / 4; i++ ) {
		int word;
		std::memcpy( &word, frame.areabits + i * 4, sizeof( word ) );
		word ^= -1;
		std::memcpy( frame.areabits + i * 4, &word, sizeof( word ) );
	}

	// copy the entity states out
	const int ring = (int)snapshotEntities.size();
	for ( int i = 0; i < numbers.count; i++ ) {
		const GEntity *ent = mohaa_world::Entity( numbers.numbers[i] );
		EntityState &out = snapshotEntities[(std::size_t)( nextSnapshotEntities % ring )];
		out = ent->s;
		out.number = numbers.numbers[i];
		nextSnapshotEntities++;
		if ( nextSnapshotEntities >= 0x7FFFFFFE ) {
			Com_Error( ERR_FATAL, "svs.nextSnapshotEntities wrapped" );
		}
		frame.numEntities++;
	}
}

void EmitPacketEntities( const Frame *from, const Frame &to, msg_t *msg ) {
	const int ring = (int)snapshotEntities.size();
	const int fromCount = from ? from->numEntities : 0;
	int newindex = 0, oldindex = 0;

	while ( newindex < to.numEntities || oldindex < fromCount ) {
		const EntityState *newent = nullptr;
		const EntityState *oldent = nullptr;
		int newnum = 9999, oldnum = 9999;

		if ( newindex < to.numEntities ) {
			newent = &snapshotEntities[(std::size_t)( ( to.firstEntity + newindex ) % ring )];
			newnum = newent->number;
		}
		if ( oldindex < fromCount ) {
			oldent = &snapshotEntities[(std::size_t)( ( from->firstEntity + oldindex ) % ring )];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			// delta update from old position; nothing is written if unchanged
			fntech3::mohaa::wire::WriteDeltaEntity( msg, oldent, newent, false );
			oldindex++;
			newindex++;
			continue;
		}
		if ( newnum < oldnum ) {
			// a new entity, sent from its baseline
			EntityState base;
			if ( newnum >= 0 && newnum < FNTECH3_MOHAA_MAX_GENTITIES && mohaa_game::baselineUsed[newnum] ) {
				base = mohaa_game::baselines[newnum];
			} else {
				fntech3::mohaa::wire::NullEntityState( base );
			}
			fntech3::mohaa::wire::WriteDeltaEntity( msg, &base, newent, true );
			newindex++;
			continue;
		}
		// the old entity isn't present in the new message
		fntech3::mohaa::wire::WriteDeltaEntity( msg, oldent, nullptr, true );
		oldindex++;
	}

	fntech3::mohaa::wire::WriteEntityNum( msg, fntech3::mohaa::wire::kEntityNumEnd );
}

void WriteSnapshotToClient( client_t *client, ClientState &state, msg_t *msg ) {
	const Frame &frame = state.frames[client->netchan.outgoingSequence & PACKET_MASK];
	const Frame *oldframe = nullptr;
	int lastframe = 0;

	// try to use a previous frame as the source for delta compressing the
	// snapshot; sequence numbers wrap, so their distance is measured as the
	// engine's own snapshot writer measures it
	const std::uint32_t deltaDistance = fnql::net::SequenceDistance(
		client->netchan.outgoingSequence, client->deltaMessage );
	if ( client->deltaMessage < 0 || client->state != CS_ACTIVE ) {
		// client is asking for a retransmit
	} else if ( !fnql::net::IsNewerSequence( client->netchan.outgoingSequence, client->deltaMessage )
		|| deltaDistance >= ( PACKET_BACKUP - 3u ) ) {
		if ( client->deltaMessage != fnql::net::RetreatSequence( client->netchan.outgoingSequence,
			PACKET_BACKUP + 1u ) ) {
			Com_DPrintf( "%s: Delta request from out of date packet.\n", client->name );
		}
	} else {
		oldframe = &state.frames[client->deltaMessage & PACKET_MASK];
		lastframe = static_cast<int>( deltaDistance );

		// the snapshot's entities may still have rolled off the buffer, though
		if ( oldframe->firstEntity <= nextSnapshotEntities - (int)snapshotEntities.size() ) {
			Com_DPrintf( "%s: Delta request from out of date entities.\n", client->name );
			oldframe = nullptr;
			lastframe = 0;
		}
	}

	MSG_WriteByte( msg, kSvcSnapshot );

	// the server's time, so the client can drift its view of time to match
	MSG_WriteLong( msg, sv.time + client->oldServerTime );
	MSG_WriteByte( msg, sv.timeResidual > 254 ? 255 : sv.timeResidual < 0 ? 0 : sv.timeResidual );

	// what we are delta'ing from
	MSG_WriteByte( msg, lastframe );

	int snapFlags = svs.snapFlagServerBit;
	if ( client->rateDelayed ) {
		snapFlags |= kSnapflagRateDelayed;
	}
	if ( client->state != CS_ACTIVE ) {
		snapFlags |= kSnapflagNotActive;
	}
	MSG_WriteByte( msg, snapFlags );

	// the areabits
	MSG_WriteByte( msg, frame.areabytes );
	MSG_WriteData( msg, frame.areabits, frame.areabytes );

	// delta encode the playerstate
	fntech3::mohaa::wire::WriteDeltaPlayerstate( msg, oldframe ? &oldframe->ps : nullptr, &frame.ps );

	// delta encode the entities
	EmitPacketEntities( oldframe, frame, msg );

	fntech3::mohaa::wire::WriteSounds( msg, state.sounds, state.numSounds );

	if ( state.print[0] ) {
		if ( state.locationPrint ) {
			MSG_WriteByte( msg, kSvcLocprint );
			MSG_WriteShort( msg, state.printX );
			MSG_WriteShort( msg, state.printY );
			MSG_WriteString( msg, state.print );
		} else {
			MSG_WriteByte( msg, kSvcCenterprint );
			MSG_WriteString( msg, state.print );
		}
	}
}

void SendClientSnapshot( client_t *client ) {
	ClientState *state = State( client );
	if ( !state ) {
		return;
	}

	BuildClientSnapshot( client, *state );

	if ( client->netchan.remoteAddress.type == NA_BOT ) {
		return;
	}

	std::array<byte, MAX_GAME_MSGLEN_BUF> buffer{};
	msg_t msg;
	MSG_Init( &msg, buffer.data(), Netchan_MessageLimit( client->netchan.wireProfile ) );
	msg.allowoverflow = qtrue;

	// every server message acknowledges the client's reliable commands
	MSG_WriteLong( &msg, client->lastClientCommand );

	// (re)send any reliable server commands
	SV_UpdateServerCommandsToClient( client, &msg );

	// the snapshot, then what the game queued since the last one
	WriteSnapshotToClient( client, *state, &msg );
	state->numSounds = 0;
	state->print[0] = '\0';
	state->locationPrint = false;
	WriteCGMToClient( *state, SV_ClientIndex( client ), &msg );

	// the transmit closes the message with the profile's end byte
	if ( msg.overflowed ) {
		Com_Printf( "WARNING: msg overflowed for %s\n", client->name );
		MSG_Clear( &msg );
	}

	SV_SendMessageToClient( &msg, client );
}

/* ---- the gamestate's baselines ------------------------------------------ */

void WriteBaselines( msg_t *msg ) {
	EntityState nullstate;
	fntech3::mohaa::wire::NullEntityState( nullstate );
	for ( int i = 0; i < FNTECH3_MOHAA_MAX_GENTITIES; i++ ) {
		if ( !mohaa_game::baselineUsed[i] ) {
			continue;
		}
		MSG_WriteByte( msg, 4 ); // svc_baseline
		fntech3::mohaa::wire::WriteDeltaEntity( msg, &nullstate, &mohaa_game::baselines[i], true );
	}
}

} // namespace mohaa_net

#endif
