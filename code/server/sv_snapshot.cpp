/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "server.h"
#include "server_world_bounds.hpp"
#include "server_alice_snapshot.hpp"
#include "server_jedi_sp_snapshot.hpp"
#include "../qcommon/netchan_safety.hpp"
#include "../compat/jka_state_adapter.hpp"
#include "../compat/rtcw_game_abi.h"

#include <algorithm>
#include <array>


/*
=============================================================================

Delta encode a client frame onto the network channel

A normal server packet will look like:

4	sequence number (high bit set if an oversize fragment)
<optional reliable commands>
1	svc_snapshot
4	last client reliable command
4	serverTime
1	lastframe for delta compression
1	snapFlags
1	areaBytes
<areabytes>
<playerstate>
<packetentities>

=============================================================================
*/

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entityState_t list to the message.
=============
*/
static void SV_EmitPacketEntities( const clientSnapshot_t *from,
	const clientSnapshot_t *to, msg_t *msg,
	netchanWireProfile_t profile ) {
	entityState_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;

	// generate the delta update
	if ( !from ) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->num_entities;
	}

	newent = nullptr;
	oldent = nullptr;
	newindex = 0;
	oldindex = 0;
	while ( newindex < to->num_entities || oldindex < from_num_entities ) {
		if ( newindex >= to->num_entities ) {
			newnum = MAX_GENTITIES+1;
		} else {
			newent = to->ents[ newindex ];
			newnum = newent->number;
		}

		if ( oldindex >= from_num_entities ) {
			oldnum = MAX_GENTITIES+1;
		} else {
			oldent = from->ents[ oldindex ];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			// delta update from old position
			// because the force parm is qfalse, this will not result
			// in any bytes being emitted if the entity has not changed at all
			if ( !MSG_WriteDeltaEntityForWireProfile(
				msg, oldent, newent, qfalse, profile ) ) {
				SV_ErrorForWireProfile( __func__, profile );
				return;
			}
			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum ) {
			// this is a new entity, send it from the baseline
			if ( !MSG_WriteDeltaEntityForWireProfile( msg,
				&sv.svEntities[newnum].baseline, newent, qtrue, profile ) ) {
				SV_ErrorForWireProfile( __func__, profile );
				return;
			}
			newindex++;
			continue;
		}

		if ( newnum > oldnum ) {
			// the old entity isn't present in the new message
			if ( !MSG_WriteDeltaEntityForWireProfile(
				msg, oldent, nullptr, qtrue, profile ) ) {
				SV_ErrorForWireProfile( __func__, profile );
				return;
			}
			oldindex++;
			continue;
		}
	}

	MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );	// end of packetentities
}


/*
==================
SV_WriteRitualTimeResidual

Ritual's snapshots follow the server time with the milliseconds the server
has run past its last frame, clamped to a byte: fakk2.exe 1.02 writes
min( sv.timeResidual, 255 ) there (SV_WriteSnapshotToClient 0x00447b13,
the residual SV_Frame 0x00447650 keeps at 0x009cbc74), and both retail
clients read it before the delta byte (fakk2.exe 0x0042385e, alice.exe
0x0042dc6e). Without it a retail reader takes the delta byte for it and the
flags for the delta.
==================
*/
static void SV_WriteRitualTimeResidual( const client_t *client, msg_t *msg ) {
	if ( client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {
		MSG_WriteByte( msg, std::clamp( sv.timeResidual, 0, 255 ) );
	}
}


/*
==================
SV_WriteAliceSnapshotTail

What American McGee's Alice sends after its packet entities, as retail
alice.exe does at 0x00456cff: the sounds the game queued, in the block its
writer at 0x00446d00 produces, and then a pending centre or located print as
its own opcode.

The sound block is a presence bit, a six-bit count, and for each sound either
a stop -- one bit, the entity in ten bits and the channel in six -- or a start:
one bit, an origin only when it is not all zero, the entity and channel, the
sound index in eight bits, and the volume and minimum distance each only when
it is not -1.
==================
*/
static void SV_WriteAliceSnapshotTail( const client_t *client, msg_t *msg ) {
	MSG_WriteAliceSounds( msg, client->aliceSounds, client->aliceNumSounds );

	if ( client->aliceCenterPrint[0] ) {
		if ( client->aliceLocationPrint ) {
			MSG_WriteByte( msg, 9 );	// svc_locprint
			MSG_WriteShort( msg, client->aliceLocationX );
			MSG_WriteShort( msg, client->aliceLocationY );
		} else {
			MSG_WriteByte( msg, 8 );	// svc_centerprint
		}
		MSG_WriteStringForWireProfile( msg, client->aliceCenterPrint,
			client->netchan.wireProfile );
	}
}

/*
==================
SV_WriteSnapshotToClient
==================
*/
static void SV_WriteSnapshotToClient( const client_t *client, msg_t *msg ) {
	const clientSnapshot_t	*oldframe;
	const clientSnapshot_t	*frame;
	int					lastframe;
	int					i;
	int					snapFlags;
	const std::uint32_t deltaDistance = fnql::net::SequenceDistance(
		client->netchan.outgoingSequence, client->deltaMessage );

	// this is the snapshot we are creating
	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];

	// try to use a previous frame as the source for delta compressing the snapshot
	if ( /* client->deltaMessage <= 0 || */ client->state != CS_ACTIVE ) {
		// client is asking for a retransmit
		oldframe = nullptr;
		lastframe = 0;
	} else if ( !fnql::net::IsNewerSequence( client->netchan.outgoingSequence,
		client->deltaMessage ) || deltaDistance >= ( PACKET_BACKUP - 3u ) ) {
		// client hasn't gotten a good message through in a long time
		if ( com_developer->integer ) {
			if ( client->deltaMessage != fnql::net::RetreatSequence(
				client->netchan.outgoingSequence, PACKET_BACKUP + 1u ) ) {
				Com_Printf( "%s: Delta request from out of date packet.\n", client->name );
			}
		}
		oldframe = nullptr;
		lastframe = 0;
	} else {
		// we have a valid snapshot to delta from
		oldframe = &client->frames[ client->deltaMessage & PACKET_MASK ];
		lastframe = static_cast<int>( deltaDistance );
		// we may refer on outdated frame
		if ( oldframe->frameNum - svs.lastValidFrame < 0 ) {
			Com_DPrintf( "%s: Delta request from out of date frame.\n", client->name );
			oldframe = nullptr;
			lastframe = 0;
		}
	}

	MSG_WriteByte( msg, svc_snapshot );

	// NOTE, MRE: now sent at the start of every message from server to client
	// let the client know which reliable clientCommands we have received
	//MSG_WriteLong( msg, client->lastClientCommand );
	// Holomatch still sends it here, in every snapshot.
	if ( SV_LegacyFraming( client ) ) {
		MSG_WriteLong( msg, client->lastClientCommand );
	}

	// send over the current server time so the client can drift
	// its view of time to try to match
	if ( client->oldServerTime ) {
		// The server has not yet got an acknowledgement of the
		// new gamestate from this client, so continue to send it
		// a time as if the server has not restarted. Note from
		// the client's perspective this time is strictly speaking
		// incorrect, but since it'll be busy loading a map at
		// the time it doesn't really matter.
		MSG_WriteLong( msg, sv.time + client->oldServerTime );
	} else {
		MSG_WriteLong( msg, sv.time );
	}
	SV_WriteRitualTimeResidual( client, msg );

	// what we are delta'ing from
	MSG_WriteByte( msg, lastframe );
	if ( Com_GameIsSTVEFSP() ) {
		// The game can clamp/rewind command times; publish the actual ordinal.
		MSG_WriteLong( msg, client->stvefSpCommandNumber );
	}

	snapFlags = svs.snapFlagServerBit;
	if ( client->rateDelayed ) {
		snapFlags |= SNAPFLAG_RATE_DELAYED;
	}
	if ( client->state != CS_ACTIVE ) {
		snapFlags |= SNAPFLAG_NOT_ACTIVE;
	}
	if ( Com_GameIsSTVEFSP() && client->stvefSpDroppedCommands ) {
		snapFlags |= SNAPFLAG_STVEF_SP_DROPPED_COMMANDS;
	}

	MSG_WriteByte (msg, snapFlags);

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	MSG_WriteData (msg, frame->areabits, frame->areabytes);

	// don't send any changes to zombies
	if ( client->state <= CS_ZOMBIE ) {
		// playerstate: every field and array flagged unchanged, in the
		// client's own encoding. Quake III's zero count and one zero bit are
		// not enough for RTCW or ET, whose readers take a bit for ammo and
		// four for ammo in the clip after them. SV_FinalMessage sends every
		// client this snapshot too.
		static const playerState_t unchanged{};
		if ( !MSG_WriteDeltaPlayerstateForWireProfile( msg, &unchanged, &unchanged,
			client->netchan.wireProfile ) ) {
			Com_Error( ERR_DROP, "%s: invalid wire profile %i",
				__func__, static_cast<int>( client->netchan.wireProfile ) );
		}
		// packet entities
		MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );
		// Alice's client reads its sound block after every snapshot's
		// entities; retail's writer has no shortcut for a zombie and always
		// ends with it (SV_WriteSnapshotToClient 0x00456cff).
		if ( client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {
			SV_WriteAliceSnapshotTail( client, msg );
		}
		return;
	}

	// delta encode the playerstate
	const playerState_t *oldPlayerState = oldframe ? &oldframe->ps : nullptr;
	if ( !MSG_WriteDeltaPlayerstateForWireProfile( msg, oldPlayerState,
		&frame->ps, client->netchan.wireProfile ) ) {
		SV_ErrorForWireProfile( __func__, client->netchan.wireProfile );
		return;
	}
	if ( client->netchan.wireProfile == NETCHAN_WIRE_JKA_MP
		&& frame->ps.raven.m_iVehicleNum ) {
		fntech3_jka_playerState_t vehicle{}, oldVehicle{};
		const bool hasBase = oldframe && oldframe->ps.raven.m_iVehicleNum != 0;
		if ( !fntech3::jka::state::FromInternal( frame->jkaVehiclePs, vehicle )
			|| ( hasBase && !fntech3::jka::state::FromInternal( oldframe->jkaVehiclePs, oldVehicle ) )
			|| !MSG_WriteDeltaJKAPlayerstate( msg, hasBase ? &oldVehicle : nullptr,
				&vehicle, qtrue ) ) {
			SV_ErrorForWireProfile( __func__, client->netchan.wireProfile );
			return;
		}
	}

	// delta encode the entities
	SV_EmitPacketEntities(
		oldframe, frame, msg, client->netchan.wireProfile );

	if ( client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {
		SV_WriteAliceSnapshotTail( client, msg );
	}

	// padding for rate debugging
	if ( sv_padPackets->integer ) {
		for ( i = 0 ; i < sv_padPackets->integer ; i++ ) {
			MSG_WriteByte (msg, svc_nop);
		}
	}
}


/*
==================
SV_WriteLegacyDownloadSnapshot

Holomatch clients learn which reliable commands arrived only from snapshots.
A message carrying download blocks therefore opens with an empty one, as
Lilium Voyager's server does; retail appends its blocks to regular snapshots.
==================
*/
void SV_WriteLegacyDownloadSnapshot( const client_t *client, msg_t *msg ) {
	static const playerState_t empty{};
	int snapFlags = svs.snapFlagServerBit;

	MSG_WriteByte( msg, svc_snapshot );
	MSG_WriteLong( msg, client->lastClientCommand );
	MSG_WriteLong( msg, client->oldServerTime ? sv.time + client->oldServerTime : sv.time );
	SV_WriteRitualTimeResidual( client, msg );
	MSG_WriteByte( msg, 0 ); // not delta compressed
	if ( client->rateDelayed ) {
		snapFlags |= SNAPFLAG_RATE_DELAYED;
	}
	if ( client->state != CS_ACTIVE ) {
		snapFlags |= SNAPFLAG_NOT_ACTIVE;
	}
	MSG_WriteByte( msg, snapFlags );
	MSG_WriteByte( msg, 0 ); // no areabits
	MSG_WriteData( msg, nullptr, 0 );
	MSG_WriteDeltaPlayerstateForWireProfile( msg, &empty, &empty, client->netchan.wireProfile );
	MSG_WriteBits( msg, MAX_GENTITIES - 1, GENTITYNUM_BITS );
	// Alice's client reads a sound block after every snapshot's entities
	// (retail 0x00456cff); an empty one is its single zero bit. The client's
	// queued sounds stay for its next real snapshot.
	if ( client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {
		MSG_WriteAliceSounds( msg, nullptr, 0 );
	}
}


/*
==================
SV_UpdateServerCommandsToClient

(re)send all server commands the client hasn't acknowledged yet
==================
*/
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) {
	// write any unacknowledged serverCommands
	std::uint32_t pending = 0;
	if ( !fnql::net::PendingCounterCount( client->reliableSequence,
		client->reliableAcknowledge, pending ) || pending > SV_ReliableCommandLimit( client ) ) {
		Com_Error( ERR_DROP, "SV_UpdateServerCommandsToClient: invalid reliable window" );
		return;
	}
	const int n = static_cast<int>( pending );

	for ( int i : SV_Indices( n ) ) {
		const int index = fnql::net::CounterAdd( client->reliableAcknowledge,
			static_cast<std::uint32_t>( i + 1 ) );
		MSG_WriteByte( msg, svc_serverCommand );
		MSG_WriteLong( msg, index );
		MSG_WriteStringForWireProfile( msg,
			client->reliableCommands[ index & ( SV_ReliableCommandLimit( client ) - 1 ) ],
			client->netchan.wireProfile );
	}
}

/*
=============================================================================

Build a client snapshot structure

=============================================================================
*/


using SnapshotEntityIndex = int;

struct SnapshotEntityNumbers {
	int count = 0;
	std::array<SnapshotEntityIndex, MAX_SERVER_SNAPSHOT_ENTITIES> indices{};
	bool unordered = false;
	int clientNum = 0;
	bool wolfetSelfPortalAdded = false;
	// Raven's single-player engines add up to 1,024 entities, and on an odd
	// snapshot one fewer than their state ring holds, which the builder sets
	// (fntech3::jedi::sp::snapshot).
	const int limit = Com_GameIsWolfET() || Com_GameIsJKASP() || Com_GameIsJK2SP()
		? MAX_SERVER_SNAPSHOT_ENTITIES : MAX_SNAPSHOT_ENTITIES;
	int oddLimit = -1;
	static_assert( MAX_SERVER_SNAPSHOT_ENTITIES == fntech3::jedi::sp::snapshot::kSnapshotEntities );
	// The walk's copies of the record it is reading and of an ET proxy's
	// target. They live here rather than in the walk's frame, which recurses
	// once per portal: retail recurses to the last portal entity on 0x40-byte
	// frames, and a record is about 2 KB.
	sharedEntity_t entity{}, target{};

	void Add( svEntity_t &svEnt, SnapshotEntityIndex index ) {
		const bool wolfet = Com_GameIsWolfET() != 0;
		if ( wolfet && svEnt.snapshotCounter == sv.snapshotCounter ) return;
		svEnt.snapshotCounter = sv.snapshotCounter;

		// if we are full, silently discard entities
		if ( count >= limit || count == oddLimit ) {
			return;
		}
		if ( wolfet ) {
			if ( !svs.currFrame || index < 0 || index >= svs.currFrame->count || !svs.currFrame->ents[index] ) {
				Com_Error( ERR_DROP, "Invalid ET snapshot storage index %i", index );
				return;
			}
			if ( !SV_WolfETSnapshotCallback( svs.currFrame->ents[index]->number, clientNum ) ) return;
		}

		indices[count++] = index;
	}

	void MarkUnordered() {
		unordered = true;
	}
};

// ET visibility proxies select stored entities without requiring the target's
// own PVS to overlap the client. Single-target and fan-out proxies have distinct
// NOCLIENT rules in the original contract; keep their selection explicit.
static bool SV_AddWolfETVisibilityTargets( const sharedEntity_t &dummy, SnapshotEntityNumbers &numbers ) {
	if ( !Com_GameIsWolfET() ) return false;
	const bool single = ( dummy.r.svFlags & SVF_RTCW_VISDUMMY ) != 0;
	if ( !single && !( dummy.r.svFlags & SVF_RTCW_VISDUMMY_MULTIPLE ) ) return false;
	if ( single && ( dummy.s.otherEntityNum < 0 || dummy.s.otherEntityNum >= sv.num_entities ) ) {
		Com_Error( ERR_DROP, "Invalid ET visibility target %i", dummy.s.otherEntityNum );
		return true;
	}
	for ( int index = 0; index < svs.currFrame->count; ++index ) {
		const auto &state = *svs.currFrame->ents[index];
		if ( single ? state.number != dummy.s.otherEntityNum :
			( state.number == dummy.s.number || state.otherEntityNum != dummy.s.number ) ) continue;
		sharedEntity_t &target = numbers.target;
		if ( !SV_ReadGameEntityByNum( state.number, &target ) ) {
			Com_Error( ERR_DROP, "Invalid ET visibility entity %i", state.number );
			return true;
		}
		if ( !target.r.linked || ( !single && ( target.r.svFlags & SVF_NOCLIENT ) ) ) continue;
		numbers.Add( sv.svEntities[state.number], index );
		numbers.MarkUnordered();
		if ( single ) break;
	}
	return true;
}


/*
=============
SV_SortEntityNumbers

Insertion sort is about 10 times faster than quicksort for our task
=============
*/
static void SV_SortEntityNumbers( SnapshotEntityNumbers &numbers ) {
	auto &num = numbers.indices;
	for ( int i : SV_Indices( 1, numbers.count ) ) {
		int d = i;
		while ( d > 0 && num[d] < num[d-1] ) {
			std::swap( num[d], num[d-1] );
			d--;
		}
	}
#ifdef _DEBUG
	// consistency check for delta encoding
	for ( int i : SV_Indices( 1, numbers.count ) ) {
		if ( num[i-1] >= num[i] ) {
			Com_Error( ERR_DROP, "%s: invalid entity number %i", __func__, num[ i ] );
		}
	}
#endif
}


/*
===============
SV_EntityPassesSnapshotClientFlags
===============
*/
static bool SV_EntityPassesSnapshotClientFlags( const sharedEntity_t &entity, int clientNum ) {
	if ( entity.r.svFlags & SVF_NOCLIENT ) return false;
	// Raven's single-player walks test no other client flag (jasp.exe
	// 0x43c7d7, jk2sp.exe 0x43a21e): their games' 0x2, 0x100 and 0x800 are
	// SVF_INACTIVE, SVF_TRIMODEL and SVF_NPC_PRECACHE
	if ( Com_GameIsJKASP() || Com_GameIsJK2SP() ) return true;
	// entities can be flagged to be sent to only one client
	if ( entity.r.svFlags & SVF_SINGLECLIENT ) {
		if ( entity.r.singleClient != clientNum ) {
			return false;
		}
	}
	// entities can be flagged to be sent to everyone but one client
	if ( entity.r.svFlags & SVF_NOTSINGLECLIENT ) {
		if ( entity.r.singleClient == clientNum ) {
			return false;
		}
	}
	// entities can be flagged to be sent to a given mask of clients; the
	// Jedi engines read no such bit (jamp.exe 0x460fae, jk2mp.exe 0x44fd80)
	if ( !Com_GameIsJKAMP() && !Com_GameIsJK2MP() && ( entity.r.svFlags & SVF_CLIENTMASK ) ) {
		if ( clientNum >= 32 ) {
			Com_Error( ERR_DROP, "SVF_CLIENTMASK: clientNum >= 32" );
		}
		if ( ~entity.r.singleClient & ( 1u << clientNum ) ) {
			return false;
		}
	}

	return true;
}

/*
===============
SV_GameForcesSnapshotEntity

Mirrors the retail qagame visibility exports that can include entities before
the normal area and PVS pass.  Keep this native-only so legacy QVM/classic DLLs
are not asked for QL-only exports they may not implement.
===============
*/
static bool SV_GameForcesSnapshotEntity( int viewerClientNum, int entityNum ) {
	if ( !gvm || !gvm->dllExports ) {
		return false;
	}

	if ( entityNum >= 0 && entityNum < Com_GameClientLimit() ) {
		return VM_Call( gvm, 2, GAME_CAN_CLIENT_SEE_CLIENT, viewerClientNum, entityNum ) != 0;
	}

	if ( VM_Call( gvm, 2, GAME_FREEZE_CAN_SEE_THAW_PROGRESS_EVENT, viewerClientNum, entityNum ) ) {
		return true;
	}

	return VM_Call( gvm, 1, GAME_IS_OBJECTIVE_ENTITY, entityNum ) != 0;
}

/*
===============
SV_AreasVisibleForSnapshot
===============
*/
static bool SV_AreasVisibleForSnapshot( int clientarea, const svEntity_t *svEnt ) {
	if ( CM_AreasConnected( clientarea, svEnt->areanum ) ) {
		return true;
	}
	// doors can legally straddle two areas, so we may need to check another one
	if ( CM_AreasConnected( clientarea, svEnt->areanum2 ) ) {
		return true;
	}
	return false;
}

/*
===============
SV_AudioOnlyEventEntity
===============
*/
static bool SV_AudioOnlyEventEntity( const entityState_t *es ) {
	int event;

	if ( es->eType < ET_EVENTS ) {
		return false;
	}

	event = es->eType - ET_EVENTS;
	return event == EV_GENERAL_SOUND || event == EV_GLOBAL_SOUND || event == EV_GLOBAL_TEAM_SOUND;
}

/*
===============
SV_AudioOnlyEntity
===============
*/
static bool SV_AudioOnlyEntity( const entityState_t *es ) {
	if ( SV_AudioOnlyEventEntity( es ) ) {
		return true;
	}
	if ( es->eType == ET_SPEAKER ) {
		return true;
	}
	if ( !es->loopSound ) {
		return false;
	}
	if ( es->eType != ET_GENERAL && es->eType != ET_INVISIBLE ) {
		return false;
	}
	if ( es->modelindex || es->modelindex2 || es->constantLight || es->solid ) {
		return false;
	}
	if ( es->event || es->eventParm ) {
		return false;
	}
	return true;
}

/*
===============
SV_AudioReachEntity
===============
*/
static bool SV_AudioReachEntity( const entityState_t *es ) {
	const int mode = sv_audioPVS ? sv_audioPVS->integer : 0;

	if ( mode <= 0 ) {
		return false;
	}
	if ( mode == 1 ) {
		return SV_AudioOnlyEntity( es );
	}
	return SV_AudioOnlyEventEntity( es ) || es->loopSound || es->eType == ET_SPEAKER;
}

/*
===============
SV_AddAudioReachableEntities

Opt-in, protocol-neutral expansion for occluded sound emitters. This preserves
normal visual PVS and only sends extra entityStates that the client already
knows how to consume for sound.
===============
*/
static void SV_AddAudioReachableEntities( const vec3_t origin, int clientarea, clientSnapshot_t *frame,
									SnapshotEntityNumbers &eNums ) {
	int e;
	int added;
	float range;
	float rangeSquared;

	if ( !sv_audioPVS || sv_audioPVS->integer <= 0 || !sv_audioPVSRange ||
		!sv_audioPVSMaxEntities || sv_audioPVSRange->value <= 0.0f ||
		sv_audioPVSMaxEntities->integer <= 0 ) {
		return;
	}

	added = 0;
	range = sv_audioPVSRange->value;
	rangeSquared = range * range;

	for ( e = 0 ; e < svs.currFrame->count ; e++ ) {
		entityState_t *es;
		sharedEntity_t entity;
		svEntity_t *svEnt;

		if ( added >= sv_audioPVSMaxEntities->integer ) {
			break;
		}

		es = svs.currFrame->ents[ e ];
		if ( !SV_AudioReachEntity( es ) ) {
			continue;
		}

		if ( !SV_ReadGameEntityByNum( es->number, &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid entity %i", __func__, es->number );
			return;
		}
		if ( !SV_EntityPassesSnapshotClientFlags( entity, frame->ps.clientNum ) ) {
			continue;
		}
		qboolean jediBroadcast = qfalse;
		if ( ( Com_GameIsJKAMP() || Com_GameIsJK2MP() ) && !SV_JediSnapshotBroadcast( es->number,
			frame->ps.clientNum, &jediBroadcast ) ) continue;

		svEnt = &sv.svEntities[ es->number ];
		if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
			continue;
		}
		if ( !SV_AreasVisibleForSnapshot( clientarea, svEnt ) ) {
			continue;
		}
		if ( DistanceSquared( origin, entity.r.currentOrigin ) > rangeSquared ) {
			continue;
		}

		eNums.Add( *svEnt, e );
		added++;
		eNums.MarkUnordered();
	}
}


/*
===============
SV_AddEntitiesVisibleFromPoint

`portal` is set in a pass seen from somewhere other than the viewer's eye.
Every retail engine that shares this walk recurses into a portal seen
through another portal, bounded only by the already-added test (quake3.exe
1.32 0x43b972, quakelive_steam.exe 0x4e565d, WolfSP.exe 0x439489, WolfMP.exe
0x43c749, etded.exe 0x42f8b5, stvoyHM.exe 0x435cc8, stvoy.exe 0x42876f,
jamp.exe 0x4613a7, jk2mp.exe 0x44ff65, jasp.exe 0x43c941, jk2sp.exe 0x43a33a),
so the depth is at most the number of portal entities. Three passes set the
flag: RTCW's camera passes, walked like any other; Quake3e's second view
(SVF_SELF_PORTAL2), which adds no portal's view and no second view of its own;
and the Jedi single-player games' portal passes, as their engines set it.

Raven's single-player walks (jasp.exe 0x43c710, jk2sp.exe 0x43a170) also
always send the player's entity 0 (0x43c80a, 0x43a25b); Academy's sends an
entity seen from the sky portal (0x43c80e) and, in the eye's pass while Force
Sight is on, anything the power shows (0x43c762, 0x43c82a). They leave out an
entity not in use, and Academy's a permanent one, which SV_BuildCommonSnapshot
does for every pass.
===============
*/
static void SV_AddEntitiesVisibleFromPoint( const vec3_t origin, clientSnapshot_t *frame,
									SnapshotEntityNumbers &eNums, bool portal ) {
	int		e, i;
	svEntity_t	*svEnt;
	entityState_t  *es;
	int		l;
	int		clientarea, clientcluster;
	int		leafnum;
	byte	*clientpvs;
	byte	*bitvector;
	vec3_t	view;
	// Read into the numbers' buffer: each recursion below copies the
	// viewpoint it needs first, and nothing reads the buffer after one.
	sharedEntity_t &entity = eNums.entity;
	const bool secondView = portal && ( Com_GameIsQuake3() || Com_GameIsQuakeLive() );
	const bool jediSP = Com_GameIsJKASP() || Com_GameIsJK2SP();
	// Academy single player's Force Sight, read from the eye's pass only
	const bool forceSight = Com_GameIsJKASP() && !portal &&
		( static_cast<std::uint32_t>( frame->ps.raven.fd_forcePowersActive ) &
		fntech3::jedi::sp::snapshot::kAcademyForceSight );

	// during an error shutdown message we may need to transmit
	// the shutdown message after the server has shutdown, so
	// specifically check for it
	if ( sv.state == SS_DEAD ) {
		return;
	}
	if ( Com_GameIsWolfET() && !SV_FiniteWorldVector( origin ) ) {
		Com_Error( ERR_DROP, "Invalid ET snapshot viewpoint" );
		return;
	}

	leafnum = CM_PointLeafnum (origin);
	clientarea = CM_LeafArea (leafnum);
	clientcluster = CM_LeafCluster (leafnum);

	// calculate the visible areas
	frame->areabytes = CM_WriteAreaBits( frame->areabits, clientarea );

	clientpvs = CM_ClusterPVS (clientcluster);

	// Expand an ET player's second viewpoint once per client snapshot. Keeping
	// the visit state with the snapshot also terminates self-referential portals.
	if ( Com_GameIsWolfET() && !eNums.wolfetSelfPortalAdded ) {
		eNums.wolfetSelfPortalAdded = true;
		if ( !SV_ReadGameEntityByNum( frame->ps.clientNum, &entity ) ) {
			Com_Error( ERR_DROP, "Invalid ET snapshot viewer %i", frame->ps.clientNum );
			return;
		}
		if ( entity.r.svFlags & SVF_WOLFET_SELF_PORTAL ) {
			eNums.MarkUnordered();
			VectorCopy( entity.s.origin2, view );
			SV_AddEntitiesVisibleFromPoint( view, frame, eNums, portal );
		}
	}

	// An RTCW viewer watching a scripted camera is sent only the portal
	// entities and what each one's camera sees, which its own pass then adds
	// through portals as usual (WolfSP.exe 0x4391de..0x43921d, WolfMP.exe
	// 0x43c49e..0x43c4dd). ET's engine has no such test.
	bool rtcwCamera = false;
	if ( Com_GameIsRTCW() && !portal ) {
		if ( !SV_ReadGameEntityByNum( frame->ps.clientNum, &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid client entity %i", __func__, frame->ps.clientNum );
			return;
		}
		rtcwCamera = ( entity.s.eFlags & FNTECH3_RTCW_EF_VIEWING_CAMERA ) != 0;
	}

	for ( e = 0 ; e < svs.currFrame->count; e++ ) {
		es = svs.currFrame->ents[ e ];
		if ( !SV_ReadGameEntityByNum( es->number, &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid entity %i", __func__, es->number );
			return;
		}

		if ( !SV_EntityPassesSnapshotClientFlags( entity, frame->ps.clientNum ) ) {
			continue;
		}

		svEnt = &sv.svEntities[ es->number ];

		// don't double add an entity through portals
		qboolean jediBroadcast = qfalse;
		if ( ( Com_GameIsJKAMP() || Com_GameIsJK2MP() ) && !SV_JediSnapshotBroadcast( es->number,
			frame->ps.clientNum, &jediBroadcast ) ) continue;
		if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
			continue;
		}

		if ( rtcwCamera ) {
			if ( entity.r.svFlags & SVF_PORTAL ) {
				eNums.Add( *svEnt, e );
				eNums.MarkUnordered();
				VectorCopy( entity.s.origin2, view );
				SV_AddEntitiesVisibleFromPoint( view, frame, eNums, true );
			}
			continue;
		}

		// broadcast entities are always sent
		if ( ( entity.r.svFlags & SVF_BROADCAST ) || jediBroadcast ) {
			eNums.Add( *svEnt, e );
			continue;
		}

		// so are the Jedi single-player games' player, Academy's sky-portal
		// entities and, under Force Sight, what the power shows
		if ( jediSP && ( es->number == 0 || ( Com_GameIsJKASP() && ( entity.s.raven.isPortalEnt ||
			( forceSight && SV_JKASPForceSightSees( entity.r.currentOrigin, entity.s.eFlags,
			frame->ps.raven.fd_forcePowerLevel_FP_SEE ) ) ) ) ) ) {
			eNums.Add( *svEnt, e );
			continue;
		}

		if ( SV_GameForcesSnapshotEntity( frame->ps.clientNum, es->number ) ) {
			eNums.Add( *svEnt, e );
			continue;
		}

		// ET's origin-only brush rule bypasses both extent and area tests. A
		// solid/out-of-range origin must never index the PVS or reveal the brush.
		if ( Com_GameIsWolfET() && ( entity.r.svFlags & SVF_WOLFET_ORIGIN_ONLY ) ) {
			const int cluster = svEnt->wolfetOriginCluster;
			if ( clientpvs && cluster >= 0 && cluster < CM_NumClusters() &&
				( clientpvs[cluster >> 3] & ( 1u << ( cluster & 7 ) ) ) ) {
				eNums.Add( *svEnt, e );
			}
			continue;
		}

		// ignore if not touching a PV leaf
		// check area
		if ( !SV_AreasVisibleForSnapshot( clientarea, svEnt ) ) {
			continue;		// blocked by a door
		}

		bitvector = clientpvs;

		// check individual leafs
		if ( !svEnt->numClusters ) {
			continue;
		}
		l = 0;
		for ( i=0 ; i < svEnt->numClusters ; i++ ) {
			l = svEnt->clusternums[i];
			if ( bitvector[l >> 3] & (1 << (l&7) ) ) {
				break;
			}
		}

		// if we haven't found it to be visible,
		// check overflow clusters that couldn't be stored
		if ( i == svEnt->numClusters ) {
			if ( svEnt->lastCluster ) {
				for ( ; l <= svEnt->lastCluster ; l++ ) {
					if ( bitvector[l >> 3] & (1 << (l&7) ) ) {
						break;
					}
				}
				if ( l == svEnt->lastCluster ) {
					continue;	// not visible
				}
			} else {
				continue;
			}
		}

		if ( SV_AddWolfETVisibilityTargets( entity, eNums ) ) continue;

		// add it
		eNums.Add( *svEnt, e );

		// if it's a portal entity, add everything visible from its camera
		// position; only Quake III, Quake Live and the Jedi multiplayer games
		// carry a range in generic1, and the other profiles' records leave it
		// 0, as their engines test none
		if ( entity.r.svFlags & SVF_PORTAL && !secondView ) {
			if ( entity.s.generic1 ) {
				vec3_t dir;
				VectorSubtract(entity.s.origin, origin, dir);
				if ( VectorLengthSquared(dir) > static_cast<float>( entity.s.generic1 ) * entity.s.generic1 ) {
					continue;
				}
			}
			eNums.MarkUnordered();
			VectorCopy( entity.s.origin2, view );
			SV_AddEntitiesVisibleFromPoint( view, frame, eNums, portal || jediSP );
		}
	}

	if ( !SV_ReadGameEntityByNum( frame->ps.clientNum, &entity ) ) {
		Com_Error( ERR_DROP, "%s: invalid client entity %i",
			__func__, frame->ps.clientNum );
		return;
	}
	// extension: merge second PVS at entity.r.s.origin2. Only Quake III and
	// Quake Live publish it (SV_GetValue's SVF_SELF_PORTAL2_Q3E); no retail
	// walk reads the bit, which other games' modules may use as their own.
	if ( ( Com_GameIsQuake3() || Com_GameIsQuakeLive() ) &&
		entity.r.svFlags & SVF_SELF_PORTAL2 && !portal ) {
		VectorCopy( entity.r.s.origin2, view );
		SV_AddEntitiesVisibleFromPoint( view, frame, eNums, true );
		eNums.MarkUnordered();
	}
}


/*
===============
SV_InitSnapshotStorage
===============
*/
void SV_InitSnapshotStorage( void ) 
{
	// initialize snapshot storage
	for ( auto &frame : svs.snapFrames ) {
		frame = {};
	}
	svs.freeStorageEntities = svs.numSnapshotEntities;
	svs.currentStoragePosition = 0;

	svs.snapshotFrame = 0;
	svs.currentSnapshotFrame = 0;
	svs.lastValidFrame = 0;

	svs.currFrame = nullptr;
}


/*
===============
SV_IssueNewSnapshot

This should be called before any new client snaphot built
===============
*/
void SV_IssueNewSnapshot( void ) 
{
	svs.currFrame = nullptr;
	
	// value that clients can use even for their empty frames
	// as it will not increment on new snapshot built
	svs.currentSnapshotFrame = svs.snapshotFrame;
}


/*
===============
SV_BuildCommonSnapshot

This always allocates new common snapshot frame
===============
*/
static void SV_BuildCommonSnapshot( void ) 
{
	std::array<int, MAX_GENTITIES> list{};
	
	snapshotFrame_t	*tmp;
	snapshotFrame_t	*sf;

	int count = 0;
	int index;

	// gather all linked entities
	if ( sv.state != SS_DEAD ) {
		// Raven's single-player walks leave out an entity not in use and, in
		// Academy, a permanent one, then repair the number of what is left,
		// linked or not (jasp.exe 0x43c7a3, 0x43c7a9, 0x43c7b6; jk2sp.exe
		// 0x43a1f1, 0x43a1fd). Every pass would test them alike.
		const bool jediSP = Com_GameIsJKASP() || Com_GameIsJK2SP();
		for ( int num : SV_Indices( sv.num_entities ) ) {
			sharedEntity_t entity;
			if ( !SV_ReadGameEntityByNum( num, &entity ) ) {
				Com_Error( ERR_DROP, "%s: invalid entity %i", __func__, num );
				return;
			}

			if ( jediSP ) {
				qboolean inuse = qfalse;
				if ( !SV_ReadGameEntityInuse( num, &inuse ) ) {
					Com_Error( ERR_DROP, "%s: invalid entity %i", __func__, num );
					return;
				}
				if ( !inuse || ( Com_GameIsJKASP() && ( static_cast<std::uint32_t>( entity.s.eFlags ) &
					fntech3::jedi::sp::snapshot::kAcademyPermanent ) ) ) {
					continue;
				}
			}

			// never send entities that aren't linked in
			if ( !entity.r.linked && !jediSP ) {
				continue;
			}
	
			if ( entity.s.number != num ) {
				Com_DPrintf( "FIXING ENT->S.NUMBER %i => %i\n", entity.s.number, num );
				entity.s.number = num;
				if ( !SV_WriteGameEntityByNum( num, &entity ) ) {
					Com_Error( ERR_DROP, "%s: failed to repair entity %i", __func__, num );
					return;
				}
			}
			if ( !entity.r.linked ) {
				continue;
			}

			// entities can be flagged to explicitly not be sent to the client
			// ET single-target proxies may explicitly reveal a NOCLIENT master.
			// Keep it in common storage, then filter normal per-client candidates.
			if ( ( entity.r.svFlags & SVF_NOCLIENT ) && !Com_GameIsWolfET() ) {
				continue;
			}

			list[ count++ ] = num;
			sv.svEntities[ num ].snapshotCounter = -1;
		}
	}

	sv.snapshotCounter = -1;

	sf = &svs.snapFrames[ svs.snapshotFrame % NUM_SNAPSHOT_FRAMES ];
	
	// track last valid frame
	if ( svs.snapshotFrame - svs.lastValidFrame > (NUM_SNAPSHOT_FRAMES-1) ) {
		svs.lastValidFrame = svs.snapshotFrame - (NUM_SNAPSHOT_FRAMES-1);
		// release storage
		svs.freeStorageEntities += sf->count;
		sf->count = 0;
	}

	// release more frames if needed
	while ( svs.freeStorageEntities < count && svs.lastValidFrame != svs.snapshotFrame ) {
		tmp = &svs.snapFrames[ svs.lastValidFrame % NUM_SNAPSHOT_FRAMES ];
		svs.lastValidFrame++;
		// release storage
		svs.freeStorageEntities += tmp->count;
		tmp->count = 0;
	}

	// should never happen but anyway
	if ( svs.freeStorageEntities < count ) {
		Com_Error( ERR_DROP, "Not enough snapshot storage: %i < %i", svs.freeStorageEntities, count );
	}

	// allocate storage
	sf->count = count;
	svs.freeStorageEntities -= count;

	sf->start = svs.currentStoragePosition; 
	svs.currentStoragePosition = ( svs.currentStoragePosition + count ) % svs.numSnapshotEntities;

	sf->frameNum = svs.snapshotFrame;
	svs.snapshotFrame++;

	svs.currFrame = sf; // clients can refer to this

	// setup start index
	index = sf->start;
	for ( int i : SV_Indices( count ) ) {
		sharedEntity_t entity;
		if ( !SV_ReadGameEntityByNum( list[ i ], &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid entity %i", __func__, list[ i ] );
			return;
		}
		//index %= svs.numSnapshotEntities;
		svs.snapshotEntities[ index ] = entity.s;
		sf->ents[ i ] = &svs.snapshotEntities[ index ];
		index = (index+1) % svs.numSnapshotEntities;
	}
}


// ET's connected client owns the exclusive camera, even when its playerstate
// follows another client. Lean uses the playerstate's rolled right vector.
// Elite Force SP sees from a running camera scene's position, which its cgame
// supplies, and otherwise from the eye moved by its own lean (stvoy.exe
// SV_BuildClientSnapshot 0x4283a3). Academy SP sees from the camera position
// its client game reports, when it reports one (jasp.exe 0x43c9d3); its lean
// waits for a served player state.
static void SV_SnapshotViewOrigin( const client_t *client, const playerState_t &state, vec3_t origin ) {
	if ( Com_GameIsJKASP() && SV_JKASPCameraView( origin, nullptr ) ) {
		if ( !SV_FiniteWorldVector( origin ) ) Com_Error( ERR_DROP, "Invalid Jedi Academy camera origin" );
		return;
	}
	if ( Com_GameIsSTVEFSP() ) {
		if ( SV_STVEFSPCameraOrigin( origin ) ) {
			if ( !SV_FiniteWorldVector( origin ) ) Com_Error( ERR_DROP, "Invalid Elite Force camera origin" );
			return;
		}
		VectorCopy( state.origin, origin );
		origin[2] += state.viewheight;
		if ( state.stvef.leanofs ) {
			vec3_t angles, right;
			VectorCopy( state.viewangles, angles );
			angles[ROLL] += state.stvef.leanofs * 0.5f;
			AngleVectors( angles, nullptr, right, nullptr );
			for ( int axis = 0; axis < 3; ++axis ) origin[axis] += state.stvef.leanofs * right[axis];
		}
		if ( !SV_FiniteWorldVector( origin ) ) Com_Error( ERR_DROP, "Invalid Elite Force snapshot origin" );
		return;
	}
	VectorCopy( state.origin, origin );
	if ( Com_GameIsWolfET() ) {
		sharedEntity_t camera;
		if ( !SV_ReadGameEntity( client->gentity, &camera ) ) {
			Com_Error( ERR_DROP, "Invalid ET snapshot camera entity" );
			return;
		}
		if ( camera.r.svFlags & SVF_WOLFET_SELF_PORTAL_EXCLUSIVE )
			VectorCopy( camera.s.origin2, origin );
	}
	origin[2] += state.viewheight;
	if ( !Com_GameIsWolfET() ) return;
	if ( state.rtcw.leanf != 0.0f ) {
		vec3_t angles, right;
		VectorCopy( state.viewangles, angles );
		angles[ROLL] += state.rtcw.leanf * 0.5f;
		if ( !SV_FiniteWorldVector( angles ) ) {
			Com_Error( ERR_DROP, "Invalid ET snapshot lean angles" );
			return;
		}
		AngleVectors( angles, nullptr, right, nullptr );
		for ( int axis = 0; axis < 3; ++axis ) origin[axis] += state.rtcw.leanf * right[axis];
	}
	if ( !SV_FiniteWorldVector( origin ) ) Com_Error( ERR_DROP, "Invalid ET snapshot camera origin" );
}

/*
=============================================================================

American McGee's Alice

Retail alice.exe builds a snapshot from its game's own records rather than
from a frame shared by every client: its walk rewrites each entity's render
flags for the view it was reached from, and the states are copied only once
the walk is done. See server_alice_snapshot.hpp.

=============================================================================
*/

namespace {

// The walk's view of this server: the server state, the collision map and
// the module's records, as server_alice_snapshot.hpp describes each member.
struct AliceSnapshotWorld {
	using SvEntity = svEntity_t;
	clientSnapshot_t *frame;

	bool Dead() const { return sv.state == SS_DEAD; }
	void View( const float origin[3], int &area, const std::uint8_t *&pvs ) {
		const int leaf = CM_PointLeafnum( origin );
		area = CM_LeafArea( leaf );
		const int cluster = CM_LeafCluster( leaf );
		frame->areabytes = CM_WriteAreaBits( frame->areabits, area );
		pvs = CM_ClusterPVS( cluster );
	}
	int NumEntities() const { return sv.num_entities; }
	std::uint8_t *Entity( int number ) { return SV_AliceGameEntity( number ); }
	svEntity_t &Sv( int number ) { return sv.svEntities[ number ]; }
	int Counter() const { return sv.snapshotCounter; }
	bool AreasConnected( int area1, int area2 ) const { return CM_AreasConnected( area1, area2 ) != 0; }
	int FarPlane() const { return sv.aliceFarPlane; }
	bool SkyPortal() const { return sv.aliceSkyPortal != qfalse; }
	void Fatal( const char *message ) { Com_Error( ERR_DROP, "%s", message ); }
	void FixedNumber( int ) { Com_DPrintf( "FIXING ENT->S.NUMBER!!!\n" ); }
};

}

/*
===============
SV_StoreAliceSnapshotFrame

A frame of storage holding exactly the states one client's walk chose, read
from the game after the walk as retail copies them (0x0045709d..0x004570f9),
so each carries the render flags that walk wrote. The ring and its release
rule are SV_BuildCommonSnapshot's.
===============
*/
static void SV_StoreAliceSnapshotFrame( clientSnapshot_t *frame,
	const fntech3::alice::snapshot::Numbers &numbers ) {
	const int count = numbers.count;
	snapshotFrame_t *sf = &svs.snapFrames[ svs.snapshotFrame % NUM_SNAPSHOT_FRAMES ];

	if ( svs.snapshotFrame - svs.lastValidFrame > ( NUM_SNAPSHOT_FRAMES - 1 ) ) {
		svs.lastValidFrame = svs.snapshotFrame - ( NUM_SNAPSHOT_FRAMES - 1 );
		svs.freeStorageEntities += sf->count;
		sf->count = 0;
	}
	while ( svs.freeStorageEntities < count && svs.lastValidFrame != svs.snapshotFrame ) {
		snapshotFrame_t *oldest = &svs.snapFrames[ svs.lastValidFrame % NUM_SNAPSHOT_FRAMES ];
		svs.lastValidFrame++;
		svs.freeStorageEntities += oldest->count;
		oldest->count = 0;
	}
	if ( svs.freeStorageEntities < count ) {
		Com_Error( ERR_DROP, "Not enough snapshot storage: %i < %i", svs.freeStorageEntities, count );
		return;
	}

	sf->count = count;
	svs.freeStorageEntities -= count;
	sf->start = svs.currentStoragePosition;
	svs.currentStoragePosition = ( svs.currentStoragePosition + count ) % svs.numSnapshotEntities;
	sf->frameNum = svs.snapshotFrame;
	svs.snapshotFrame++;

	int index = sf->start;
	for ( int i : SV_Indices( count ) ) {
		sharedEntity_t entity;
		if ( !SV_ReadGameEntityByNum( numbers.numbers[ i ], &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid entity %i", __func__, numbers.numbers[ i ] );
			return;
		}
		svs.snapshotEntities[ index ] = entity.s;
		sf->ents[ i ] = &svs.snapshotEntities[ index ];
		frame->ents[ i ] = sf->ents[ i ];
		index = ( index + 1 ) % svs.numSnapshotEntities;
	}
	frame->num_entities = count;
	frame->frameNum = sf->frameNum;
}

/*
=============
SV_BuildAliceClientSnapshot

Retail's SV_BuildClientSnapshot (0x00456f20) past its client checks. The
counter goes up first (0x00456f2b); the client's own entity is added before
anything is tested, because Alice is seen in the third person and carries her
weapons and effects (0x00457024..0x00457037); then everything visible from a
camera's origin or the eye (0x00456fbb), which takes in the sky portal's view
and any portal's; the numbers are sorted with a duplicate refused
(0x0045705f), the area bits inverted, and the states copied last.

One client's snapshot differs from another's by the render flags its own walk
wrote, so each Alice client stores a frame of its own instead of the frame
the other profiles share, which is also what retail's copy after each walk
amounts to. The flags themselves stay on the game's entities as retail leaves
them, so the game sees the last walk's. The profile runs one client and a
loaded game refuses more; a second is still served this way, not refused.
=============
*/
static void SV_BuildAliceClientSnapshot( client_t *client, clientSnapshot_t *frame ) {
	namespace snapshot = fntech3::alice::snapshot;

	sv.snapshotCounter++;

	vec3_t origin;
	snapshot::ViewOrigin( frame->ps.pm_flags, frame->ps.origin, frame->ps.viewheight,
		frame->ps.alice.camera_origin, origin );

	/* Heavy Metal: F.A.K.K.2's records are walked the same way at their own
	 * offsets: its engine is Alice's ancestor and could not be read. */
	const snapshot::Offsets &offsets = Com_GameIsFAKK2()
		? snapshot::kFAKK2Offsets : snapshot::kAliceOffsets;
	AliceSnapshotWorld world{ frame };
	snapshot::Numbers numbers;
	const int self = SV_ClientIndex( client );
	std::uint8_t *own = world.Entity( self );
	if ( !own ) {
		Com_Error( ERR_DROP, "%s: client %i has no entity", __func__, self );
		return;
	}
	svEntity_t *ownEntity = snapshot::SvEntityFor( world, own, offsets );
	if ( !ownEntity ) {
		return;
	}
	snapshot::AddEntToSnapshot( *ownEntity, own, numbers, sv.snapshotCounter, false, false, offsets );
	snapshot::AddEntitiesVisibleFromPoint( world, origin, numbers, nullptr, false, offsets );

	if ( !snapshot::SortNumbers( numbers ) ) {
		Com_Error( ERR_DROP, "SV_QsortEntityStates: duplicated entity" );
		return;
	}

	int *areaWords = reinterpret_cast<int *>( frame->areabits );
	for ( int areaWord : SV_Indices( MAX_MAP_AREA_BYTES / static_cast<int>( sizeof( int ) ) ) ) {
		areaWords[areaWord] ^= -1;
	}

	SV_StoreAliceSnapshotFrame( frame, numbers );
}

/*
=============
SV_BuildClientSnapshot

Decides which entities are going to be visible to the client, and
copies off the playerstate and areabits.

This properly handles multiple recursive portals, but the render
currently doesn't.

For viewing through other player's eyes, clent can be something other than client->gentity
=============
*/
static void SV_BuildClientSnapshot( client_t *client ) {
	vec3_t						org;
	clientSnapshot_t			*frame;
	SnapshotEntityNumbers entityNumbers;
	int							cl;
	int							clientLeaf;
	int							clientArea;
	svEntity_t					*svEnt;
	int							clientNum;
	playerState_t				playerState;

	// this is the frame we are creating
	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];
	cl = SV_ClientIndex( client );

	// clear everything in this snapshot
	for ( auto &areabit : frame->areabits ) {
		areabit = 0;
	}
	frame->areabytes = 0;

	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=62
	frame->num_entities = 0;
	frame->frameNum = svs.currentSnapshotFrame;
	
	if ( client->state == CS_ZOMBIE )
		return;

	// grab the current playerState_t
	if ( !SV_ReadGameClient( cl, &playerState ) ) {
		Com_Error( ERR_DROP, "%s: invalid game client %i", __func__, cl );
		return;
	}
	frame->ps = playerState;
	frame->jkaVehiclePs = playerState_t{};
	if ( Com_GameIsJKAMP() && playerState.raven.m_iVehicleNum ) {
		const int vehicleNum = playerState.raven.m_iVehicleNum;
		if ( vehicleNum < 0 || vehicleNum >= sv.num_entities ) {
			Com_Error( ERR_DROP, "Jedi Academy snapshot vehicle %d is out of range", vehicleNum );
			return;
		}
		if ( !SV_ReadJKAVehicleState( vehicleNum, &frame->jkaVehiclePs ) ) {
			Com_Error( ERR_DROP, "Jedi Academy snapshot vehicle %d has no valid player state", vehicleNum );
			return;
		}
	}

	clientNum = frame->ps.clientNum;
	entityNumbers.clientNum = clientNum;
	if ( clientNum < 0 || clientNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}

	// we set client->gentity only after sending gamestate
	// so don't send any packetentities changes until CS_PRIMED
	// because new gamestate will invalidate them anyway
	if ( !client->gentity ) {
		return;
	}

	// American McGee's Alice and Heavy Metal: F.A.K.K.2 walk the game's own
	// records and store what each client's walk chose
	// (SV_BuildAliceClientSnapshot).
	if ( Com_GameIsRitual() ) {
		SV_BuildAliceClientSnapshot( client, frame );
		return;
	}

	if ( svs.currFrame == nullptr ) {
		// this will always success and setup current frame
		SV_BuildCommonSnapshot();
	}

	// bump the counter used to prevent double adding
	sv.snapshotCounter++;

	frame->frameNum = svs.currFrame->frameNum;

	// SP's cgame visits the player through the packet-entity list to update
	// animation state shared with its game half. Retail SP includes that
	// entity when visible (stvoy.exe 0x428330, visibility walk 0x4285a0).
	// The Jedi SP walks always send it (jasp.exe 0x43c80a, jk2sp.exe
	// 0x43a25b). Other profiles regenerate it from playerstate without
	// sending it.
	const bool jediSP = Com_GameIsJKASP() || Com_GameIsJK2SP();
	if ( !Com_GameIsSTVEFSP() && !jediSP ) {
		svEnt = &sv.svEntities[ clientNum ];
		svEnt->snapshotCounter = sv.snapshotCounter;
	}
	// The Jedi SP engines count every snapshot of the level, and an odd one
	// holds one fewer entity than their ring of states (jasp.exe 0x43c97d,
	// 0x43c5dd; jk2sp.exe 0x439f1a, 0x43a3a5).
	if ( jediSP && ( ++sv.jediSPSnapshotCounter & 1 ) ) {
		entityNumbers.oddLimit = fntech3::jedi::sp::snapshot::OddSnapshotLimit( Com_GameIsJKASP() != 0 );
	}

	// find the client's viewpoint
	SV_SnapshotViewOrigin( client, playerState, org );
	clientLeaf = CM_PointLeafnum( org );
	clientArea = CM_LeafArea( clientLeaf );

	// add all the entities directly visible to the eye, which
	// may include portal entities that merge other viewpoints
	SV_AddEntitiesVisibleFromPoint( org, frame, entityNumbers, false );
	SV_AddAudioReachableEntities( org, clientArea, frame, entityNumbers );

	// if there were portals visible, there may be out of order entities
	// in the list which will need to be resorted for the delta compression
	// to work correctly.  This also catches the error condition
	// of an entity being included twice.
	if ( entityNumbers.unordered ) {
		SV_SortEntityNumbers( entityNumbers );
	}

	// now that all viewpoint's areabits have been OR'd together, invert
	// all of them to make it a mask vector, which is what the renderer wants
	int *areaWords = reinterpret_cast<int *>( frame->areabits );
	for ( int areaWord : SV_Indices( MAX_MAP_AREA_BYTES / static_cast<int>( sizeof( int ) ) ) ) {
		areaWords[areaWord] ^= -1;
	}

	frame->num_entities = entityNumbers.count;
	// get pointers from common snapshot
	for ( int index : SV_Indices( entityNumbers.count ) )	{
		frame->ents[ index ] = svs.currFrame->ents[ entityNumbers.indices[ index ] ];
	}
}


/*
=======================
SV_HolomatchSnapshotDelay

How long Holomatch holds a client's next snapshot after a message, as
stvoyHM.exe 1.20's SV_SendMessageToClient decides (0x435478..0x43550d): a
local client gets one every frame; anyone else waits the longer of its
snapshot interval and the time its rate takes to carry the message, at most
1,500 bytes and 48 header bytes, at the rate held under sv_maxRate. The next
snapshot after a rate-held wait is flagged as delayed.
=======================
*/
static void SV_HolomatchSnapshotDelay( client_t *client, int messageSize )
{
	if ( client->netchan.remoteAddress.type == NA_LOOPBACK ) {
		client->snapshotDelayMsec = 0;
		return;
	}
	if ( messageSize > 1500 ) {
		messageSize = 1500;
	}
	int rate = client->rate;
	if ( sv_maxRate->integer && sv_maxRate->integer < rate ) {
		rate = sv_maxRate->integer;
	}
	const int rateMsec = rate > 0 ? ( messageSize + 48 ) * 1000 / rate : 0;
	if ( rateMsec < client->snapshotMsec ) {
		client->snapshotDelayMsec = client->snapshotMsec;
		client->rateDelayed = qfalse;
	} else {
		client->snapshotDelayMsec = rateMsec;
		client->rateDelayed = qtrue;
	}
}


/*
=======================
SV_SendMessageToClient

Called by SV_SendClientSnapshot and SV_SendClientGameState
=======================
*/
void SV_SendMessageToClient( msg_t *msg, client_t *client )
{
	// record information about the message
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = svs.msgTime;
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = 0;

	SV_RecordDemoMessage( client, msg );

	// send the datagram
	SV_Netchan_Transmit( client, msg );

	if ( Com_GameIsSTVEFMP() ) {
		SV_HolomatchSnapshotDelay( client, msg->cursize );
	}
}


/*
=======================
SV_SendClientSnapshot

Also called by SV_FinalMessage

=======================
*/
void SV_SendClientSnapshot( client_t *client ) {
	// Campaign cast AI use game-side perception, without a bot snapshot.
	if ( Com_GameIsRTCWSP() && SV_ClientIsRTCWCastAI( client ) ) {
		return;
	}
	// Medal of Honor's clients are sent the game's own records.
	if ( Com_GameIsMOHAA() ) {
		SV_MOHAASendClientSnapshot( client );
		return;
	}

	std::array<byte, MAX_GAME_MSGLEN_BUF> msg_buf{};
	msg_t		msg;

	// build the snapshot
	SV_BuildClientSnapshot( client );

	// bots need to have their snapshots build, but
	// the query them directly without needing to be sent
	if ( client->netchan.remoteAddress.type == NA_BOT ) {
		return;
	}

	MSG_Init( &msg, msg_buf.data(), Netchan_MessageLimit( client->netchan.wireProfile ) );
	msg.allowoverflow = qtrue;

	if ( SV_LegacyFraming( client ) ) {
		MSG_STVEFBitstream( &msg );
	} else {
		// NOTE, MRE: all server->client messages now acknowledge
		// let the client know which reliable clientCommands we have received
		MSG_WriteLong( &msg, client->lastClientCommand );
	}

	// (re)send any reliable server commands
	SV_UpdateServerCommandsToClient( client, &msg );

	// send over all the relevant entityState_t
	// and the playerState_t
	SV_WriteSnapshotToClient( client, &msg );
	if ( Com_GameIsSTVEFSP() ) {
		client->stvefSpDroppedCommands = qfalse;
	}

	// Alice's queued sounds and print went with this snapshot (retail clears
	// the list at 0x00457730 once it is sent).
	client->aliceNumSounds = 0;
	client->aliceCenterPrint[0] = '\0';
	client->aliceLocationPrint = qfalse;

	// check for overflow
	if ( msg.overflowed ) {
		Com_Printf( "WARNING: msg overflowed for %s\n", client->name );
		MSG_Clear( &msg );
	}

	SV_SendMessageToClient( &msg, client );
}


/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void )
{
	svs.msgTime = Sys_Milliseconds();

	if ( Com_GameIsQ3Test() ) {
		SV_Q3TestSendClientMessages();
		return;
	}
	if ( Com_GameIsQuake3IHV() ) {
		SV_IHVNetSendClientMessages();
		return;
	}

	// send a message to each connected client
	for ( client_t &client : SV_Clients() )
	{
		if ( client.state == CS_FREE )
			continue;		// not connected

		//if ( *client.downloadName )
		//	continue;		// Client is downloading, don't send snapshots

		if ( client.state == CS_CONNECTED )
			continue;		// Client is downloading, don't send snapshots

		//if ( !client.gamestateAcked )
		//	continue;		// waiting usercmd/downloading

		// Retail Elite Force SP sends a local snapshot after each new
		// simulation tick (0x4287e0, loopback deadline at 0x427f16).
		// SV_Frame gates these calls on tick progress. Keep the real-time
		// schedule for other profiles and remote clients, and preserve the
		// queue/rate guards below so pending deltas cannot be overwritten.
		const bool stvefLocal = Com_GameIsSTVEFSP()
			&& client.netchan.remoteAddress.type == NA_LOOPBACK;
		// Holomatch waits out the deadline its last snapshot left on its game
		// clock, which already holds the client's rate (stvoyHM.exe
		// SV_SendClientMessages 0x435d25); a bot's is never set, so its
		// snapshot is built every frame, as retail's is.
		const bool holomatch = Com_GameIsSTVEFMP() != 0;
		if ( holomatch ) {
			if ( svs.time - client.lastSnapshotTime < client.snapshotDelayMsec )
				continue;
		} else if ( !stvefLocal && svs.time - client.lastSnapshotTime < client.snapshotMsec )
			continue;		// It's not time yet

		if ( client.netchan.unsentFragments || client.netchan_start_queue )
		{
			if ( !holomatch ) {
				client.rateDelayed = qtrue;
			}
			continue;		// Drop this snapshot if the packet queue is still full or delta compression will break
		}

		if ( !holomatch && SV_RateMsec( &client ) > 0 )
		{
			// Not enough time since last packet passed through the line
			client.rateDelayed = qtrue;
			continue;
		}

		// generate and send a new message
		SV_SendClientSnapshot( &client );
		client.lastSnapshotTime = svs.time;
		if ( !holomatch ) {
			client.rateDelayed = qfalse;
		}
	}
}
