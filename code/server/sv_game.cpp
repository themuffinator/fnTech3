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
// sv_game.cpp -- interface to the game dll

#include "server.h"
#include "server_cvar_compat.hpp"
#include "server_game_data.h"

#include "../botlib/botlib.h"
#include "../compat/alice_module_abi.h"
#include "../compat/game_abi_adapter.h"
#include "../compat/rtcw_module_abi.h"
#include "../compat/rtcw_state_adapter.hpp"
#include "../compat/rtcw_savegame.hpp"
#include "../compat/stvef_game_abi.h"
#include "../compat/q3test_game_abi.h"
#include "../qcommon/vm_rtcw_filesystem.hpp"
#define JSON_IMPLEMENTATION
#include "../qcommon/json.h"
#undef JSON_IMPLEMENTATION

#if defined(_WIN32)
#include <windows.h>
#endif

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>

botlib_export_t	*botlib_export;

namespace {

std::string serverEntityOverride;
fntech3::server::ServerGameDataView serverGameData;

struct ServerFlagMapping { uint32_t external, internal; };
constexpr ServerFlagMapping rtcwServerFlags[] = {
	{ FNTECH3_RTCW_SVF_NOCLIENT, SVF_NOCLIENT },
	{ FNTECH3_RTCW_SVF_VISDUMMY, SVF_RTCW_VISDUMMY },
	{ FNTECH3_RTCW_SVF_BOT, SVF_BOT },
	{ FNTECH3_RTCW_SVF_CASTAI, SVF_RTCW_CASTAI },
	{ FNTECH3_RTCW_SVF_BROADCAST, SVF_BROADCAST },
	{ FNTECH3_RTCW_SVF_PORTAL, SVF_PORTAL },
	{ FNTECH3_RTCW_SVF_USE_CURRENT_ORIGIN, SVF_USE_CURRENT_ORIGIN },
	{ FNTECH3_RTCW_SVF_NOFOOTSTEPS, SVF_RTCW_NOFOOTSTEPS },
	{ FNTECH3_RTCW_SVF_CAPSULE, SVF_CAPSULE },
	{ FNTECH3_RTCW_SVF_VISDUMMY_MULTIPLE, SVF_RTCW_VISDUMMY_MULTIPLE },
	{ FNTECH3_RTCW_SVF_SINGLECLIENT, SVF_SINGLECLIENT },
	{ FNTECH3_RTCW_SVF_NOSERVERINFO, SVF_NOSERVERINFO },
	{ FNTECH3_RTCW_SVF_NOTSINGLECLIENT, SVF_NOTSINGLECLIENT }
};
// Holomatch keeps Quake III's numbers except for its shield boxes at 0x2, which
// Quake III uses as SVF_CLIENTMASK. SVF_ELIMINATED (0x10) is game state the
// engine never reads, so it stays with the module.
constexpr ServerFlagMapping stvefServerFlags[] = {
	{ FNTECH3_STVEF_MP_SVF_NOCLIENT, SVF_NOCLIENT },
	{ FNTECH3_STVEF_MP_SVF_SHIELD_BBOX, SVF_STVEF_SHIELD_BBOX },
	{ FNTECH3_STVEF_MP_SVF_BOT, SVF_BOT },
	{ FNTECH3_STVEF_MP_SVF_BROADCAST, SVF_BROADCAST },
	{ FNTECH3_STVEF_MP_SVF_PORTAL, SVF_PORTAL },
	{ FNTECH3_STVEF_MP_SVF_USE_CURRENT_ORIGIN, SVF_USE_CURRENT_ORIGIN },
	{ FNTECH3_STVEF_MP_SVF_SINGLECLIENT, SVF_SINGLECLIENT },
	{ FNTECH3_STVEF_MP_SVF_NOSERVERINFO, SVF_NOSERVERINFO },
	{ FNTECH3_STVEF_MP_SVF_CAPSULE, SVF_CAPSULE },
	{ FNTECH3_STVEF_MP_SVF_NOTSINGLECLIENT, SVF_NOTSINGLECLIENT }
};
// Elite Force SP's engine reads four of its bits; its NPC, usable, trimodel
// and other bits are the game's own.
constexpr ServerFlagMapping stvefSpServerFlags[] = {
	{ FNTECH3_STVEF_SP_SVF_NOCLIENT, SVF_NOCLIENT },
	{ FNTECH3_STVEF_SP_SVF_BOT, SVF_BOT },
	{ FNTECH3_STVEF_SP_SVF_BROADCAST, SVF_BROADCAST },
	{ FNTECH3_STVEF_SP_SVF_PORTAL, SVF_PORTAL }
};
/* Alice's bits are not Quake III's. Its "do not send" happens to be bit 0 as
 * well, but its bot, broadcast and portal bits are 1, 2 and 3 where Quake
 * III's are 3, 5 and 6, so a record passed through untranslated would read as
 * a different set of flags entirely.
 *
 * Each of these is what the retail engine does with the bit rather than what
 * a neighbouring engine calls it. The snapshot walk at 0x004571e0 skips an
 * entity on bit 0, sends it past the visibility test on bit 2, and on bit 3
 * runs a second visibility pass from the entity's own `s.origin2`, which is
 * what a portal is; the ping loop at 0x00456550 forces a ping of zero on bit
 * 1, which is what this lineage does for a bot.
 *
 * Two tested bits are deliberately absent from this table. Bit 4 lets an
 * entity past the walk's requirement to carry a model index or a looping
 * sound, and this engine has no flag meaning that. Bit 14 reaches the same
 * unconditional-send arm as bit 2, so it could be folded into SVF_BROADCAST
 * on the way in -- but then the way out would set bit 2 on an entity whose
 * module had only ever set bit 14, which is a bit the module never wrote.
 * Leaving both unmapped drops them on the way in, where an unmapped bit only
 * makes an entity ordinary, and preserves them untouched on the way out. */
constexpr ServerFlagMapping aliceServerFlags[] = {
	{ FNTECH3_ALICE_SVF_NOCLIENT, SVF_NOCLIENT },
	{ FNTECH3_ALICE_SVF_BOT, SVF_BOT },
	{ FNTECH3_ALICE_SVF_BROADCAST, SVF_BROADCAST },
	{ FNTECH3_ALICE_SVF_PORTAL, SVF_PORTAL }
};
/* Q3Test's engines test four bits and nothing else (1.02 0x41d5bc,
 * 0x419c7b, 0x41d5ec, 0x41d6af), at Quake III's own positions
 * (q3test_game_abi.h, checked in server_q3test_game.hpp); any other bit is
 * the module's. */
constexpr ServerFlagMapping q3testServerFlags[] = {
	{ SVF_NOCLIENT, SVF_NOCLIENT },
	{ SVF_BOT, SVF_BOT },
	{ SVF_BROADCAST, SVF_BROADCAST },
	{ SVF_PORTAL, SVF_PORTAL }
};
constexpr ServerFlagMapping wolfetServerFlags[] = {
	{ FNTECH3_WOLFET_SVF_IGNOREBMODELEXTENTS, SVF_WOLFET_ORIGIN_ONLY },
	{ FNTECH3_WOLFET_SVF_SELF_PORTAL, SVF_WOLFET_SELF_PORTAL },
	{ FNTECH3_WOLFET_SVF_SELF_PORTAL_EXCLUSIVE, SVF_WOLFET_SELF_PORTAL_EXCLUSIVE }
};

int ServerFlagsToInternal( comGameId_t profile, int flags ) {
	if ( profile == COM_GAME_STVEF_MP ) {
		uint32_t result = 0;
		for ( const auto &bit : stvefServerFlags )
			if ( static_cast<uint32_t>( flags ) & bit.external ) result |= bit.internal;
		return static_cast<int>( result );
	}
	if ( profile == COM_GAME_STVEF_SP ) {
		uint32_t result = 0;
		for ( const auto &bit : stvefSpServerFlags )
			if ( static_cast<uint32_t>( flags ) & bit.external ) result |= bit.internal;
		return static_cast<int>( result );
	}
	/* F.A.K.K.2's bits are Alice's: its SDK header numbers the no-client,
	 * bot, broadcast, portal, send-by-PVS, send-once and sent flags 0 to 4, 14
	 * and 15, the positions alice.exe's walk tests. */
	if ( profile == COM_GAME_ALICE || profile == COM_GAME_FAKK2 ) {
		uint32_t result = 0;
		for ( const auto &bit : aliceServerFlags )
			if ( static_cast<uint32_t>( flags ) & bit.external ) result |= bit.internal;
		return static_cast<int>( result );
	}
	if ( profile == COM_GAME_Q3TEST ) {
		uint32_t result = 0;
		for ( const auto &bit : q3testServerFlags )
			if ( static_cast<uint32_t>( flags ) & bit.external ) result |= bit.internal;
		return static_cast<int>( result );
	}
	if ( profile != COM_GAME_RTCW_SP && profile != COM_GAME_RTCW_MP && profile != COM_GAME_WOLFET ) return flags;
	uint32_t result = 0;
	for ( const auto &bit : rtcwServerFlags ) {
		if (profile == COM_GAME_WOLFET && (bit.external == FNTECH3_RTCW_SVF_CASTAI ||
			bit.external == FNTECH3_RTCW_SVF_USE_CURRENT_ORIGIN)) continue;
		if ( static_cast<uint32_t>( flags ) & bit.external ) result |= bit.internal;
	}
	if ( profile == COM_GAME_WOLFET ) {
		for ( const auto &bit : wolfetServerFlags )
			if ( static_cast<uint32_t>( flags ) & bit.external ) result |= bit.internal;
	}
	return static_cast<int>( result );
}

int ServerFlagsFromInternal( comGameId_t profile, int flags, int previous ) {
	if ( profile == COM_GAME_STVEF_MP || profile == COM_GAME_STVEF_SP ) {
		// Engine updates must preserve module-owned bits the engine does not know.
		uint32_t result = static_cast<uint32_t>( previous );
		const auto apply = [&]( const auto &table ) {
			for ( const auto &bit : table ) {
				result &= ~bit.external;
				if ( static_cast<uint32_t>( flags ) & bit.internal ) result |= bit.external;
			}
		};
		if ( profile == COM_GAME_STVEF_SP ) apply( stvefSpServerFlags );
		else apply( stvefServerFlags );
		int output;
		std::memcpy( &output, &result, sizeof( output ) );
		return output;
	}
	if ( profile == COM_GAME_Q3TEST ) {
		uint32_t kept = static_cast<uint32_t>( previous );
		for ( const auto &bit : q3testServerFlags ) {
			kept &= ~bit.external;
			if ( static_cast<uint32_t>( flags ) & bit.internal ) kept |= bit.external;
		}
		int output;
		std::memcpy( &output, &kept, sizeof( output ) );
		return output;
	}
	if ( profile == COM_GAME_ALICE || profile == COM_GAME_FAKK2 ) {
		// Every bit this engine does not map stays exactly as the module left it.
		uint32_t kept = static_cast<uint32_t>( previous );
		for ( const auto &bit : aliceServerFlags ) {
			kept &= ~bit.external;
			if ( static_cast<uint32_t>( flags ) & bit.internal ) kept |= bit.external;
		}
		int output;
		std::memcpy( &output, &kept, sizeof( output ) );
		return output;
	}
	if ( profile != COM_GAME_RTCW_SP && profile != COM_GAME_RTCW_MP && profile != COM_GAME_WOLFET ) return flags;
	// Engine updates must preserve module-owned bits the engine does not know.
	uint32_t result = static_cast<uint32_t>( previous );
	for ( const auto &bit : rtcwServerFlags ) {
		if (profile == COM_GAME_WOLFET && (bit.external == FNTECH3_RTCW_SVF_CASTAI ||
			bit.external == FNTECH3_RTCW_SVF_USE_CURRENT_ORIGIN)) continue;
		result &= ~bit.external;
		if ( static_cast<uint32_t>( flags ) & bit.internal ) result |= bit.external;
	}
	if ( profile == COM_GAME_WOLFET ) {
		for ( const auto &bit : wolfetServerFlags ) {
			result &= ~bit.external;
			if ( static_cast<uint32_t>( flags ) & bit.internal ) result |= bit.external;
		}
	}
	int output;
	static_assert( sizeof( output ) == sizeof( result ) );
	std::memcpy( &output, &result, sizeof( output ) );
	return output;
}

void InvalidateServerGameData()
{
	serverGameData.Invalidate();
	sv.gentities = nullptr;
	sv.gentitySize = 0;
	sv.num_entities = 0;
	sv.gameClients = nullptr;
	sv.gameClientSize = 0;
}

std::string CurrentEntityMapName()
{
	std::array<char, MAX_QPATH> mapName{};
	const char *configured = sv_mapname ? sv_mapname->string
		: Cvar_VariableString( "mapname" );
	const char *baseName = configured;
	for ( const char *cursor = configured; cursor && *cursor; ++cursor ) {
		if ( *cursor == '/' || *cursor == '\\' ) baseName = cursor + 1;
	}
	Q_strncpyz( mapName.data(), baseName,
		static_cast<int>( mapName.size() ) );
	COM_StripExtension( mapName.data(), mapName.data(),
		static_cast<int>( mapName.size() ) );
	return mapName.data();
}

const char *SelectServerEntityString()
{
	serverEntityOverride.clear();
	const char *selected = CM_EntityString();
	if ( !selected ) selected = "";
	const std::string mapName = CurrentEntityMapName();

	if ( sv_altEntDir && sv_altEntDir->string[0] ) {
		const auto overridePath = fnql::server::cvars::BuildEntityFilePath(
			sv_altEntDir->string, mapName, MAX_QPATH );
		if ( !overridePath ) {
			Com_Printf( S_COLOR_YELLOW
				"Rejected unsafe or overlong sv_altEntDir entity path for map %s.\n",
				mapName.c_str() );
		}
		else {
			void *fileData = nullptr;
			const int fileLength = FS_ReadFile( overridePath->c_str(), &fileData );
			if ( fileLength >= 0 && fileData ) {
				const char *bytes = static_cast<const char *>( fileData );
				if ( std::memchr( bytes, '\0', static_cast<std::size_t>( fileLength ) ) ) {
					Com_Printf( S_COLOR_YELLOW
						"Ignored %s because its entity text contains an embedded NUL.\n",
						overridePath->c_str() );
				}
				else {
					serverEntityOverride.assign( bytes,
						static_cast<std::size_t>( fileLength ) );
					selected = serverEntityOverride.c_str();
					Com_Printf( "Loaded entity override %s (%d bytes).\n",
						overridePath->c_str(), fileLength );
				}
				FS_FreeFile( fileData );
			}
			else {
				Com_DPrintf( "No entity override found at %s; using BSP entities.\n",
					overridePath->c_str() );
			}
		}
	}

	if ( sv_dumpEntities && sv_dumpEntities->integer ) {
		const auto dumpPath = fnql::server::cvars::BuildEntityFilePath(
			"ents", mapName, MAX_QPATH );
		if ( dumpPath ) {
			const std::size_t length = std::strlen( selected );
			if ( length <= static_cast<std::size_t>(
				( std::numeric_limits<int>::max )() ) ) {
				FS_WriteFile( dumpPath->c_str(), selected, static_cast<int>( length ) );
				Com_Printf( "Dumped server entities to %s (%zu bytes).\n",
					dumpPath->c_str(), length );
			}
		}
	}

	return selected;
}

} // namespace

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int	SV_NumForGentity( sharedEntity_t *ent ) {
	fntech3::server::GameEntityRef reference;
	if ( !serverGameData.EntityRefForPointer( ent, reference ) ) {
		Com_Error( ERR_DROP, "%s: bad gEnt", __func__ );
		return 0;
	}
	return reference.index;
}


sharedEntity_t *SV_GentityNum( int num ) {
	fntech3::server::GameEntityRef reference;
	if ( !serverGameData.EntityRefAt( num, reference ) ) {
		/* An out-of-range number and an unbound view both land here, and they
		 * are different problems: the first is a bad index, the second is a
		 * profile whose entity record this engine has no layout for, so no
		 * index would have worked. Saying which saves reading the number for
		 * a fault that was never about the number. */
		if ( !serverGameData.IsBound() ) {
			Com_Error( ERR_DROP, "%s: the %s profile has no entity layout bound, "
				"so entity %i cannot be reached at all. The game module "
				"published where its entities live, but this engine has no "
				"record of what one of this title's entities contains.",
				__func__, Com_GameProfile()->displayName, num );
			return nullptr;
		}
		Com_Error( ERR_DROP, "%s: bad entity number %i", __func__, num );
		return nullptr;
	}
	return static_cast<sharedEntity_t *>( serverGameData.EntityPointer( reference ) );
}


// Original SP G_LoadGame restores the numeric episode from its header, then
// ReadEntity overwrites it with va("%s", a null missionLevel). Keep the valid
// value already supplied by the module; do not alter its proprietary payload.
static void SV_SetGameCvar( const char *name, const char *value ) {
	if ( Com_GameIsRTCWSP() && name && value && !Q_stricmp(name, "g_episode") &&
		!std::strcmp(value, "(null)") ) {
		Com_DPrintf("RTCW: retained saved episode after null mission-level assignment\n");
		return;
	}
	Cvar_SetSafe(name, value);
}

qboolean SV_ReadGameEntity( const void *entity,
	sharedEntity_t *destination ) {
	fntech3::server::GameEntityRef reference;
	if ( !destination
		|| !serverGameData.EntityRefForPointer( entity, reference ) ) {
		return qfalse;
	}

	*destination = {};
	int value = 0;
	return serverGameData.ReadEntityState( reference, destination->s )
		&& serverGameData.GetLinked( reference, value )
		&& ( destination->r.linked = SV_QBool( value != 0 ), true )
		&& serverGameData.GetLinkcount( reference, destination->r.linkcount )
		&& serverGameData.GetSvFlags( reference, value )
		&& ( destination->r.svFlags = ServerFlagsToInternal( serverGameData.Profile(), value ), true )
		&& serverGameData.GetSingleClient( reference, destination->r.singleClient )
		&& serverGameData.GetBmodel( reference, value )
		&& ( destination->r.bmodel = SV_QBool( value != 0 ), true )
		&& serverGameData.GetMins( reference, destination->r.mins )
		&& serverGameData.GetMaxs( reference, destination->r.maxs )
		&& serverGameData.GetContents( reference, destination->r.contents )
		&& serverGameData.GetAbsmin( reference, destination->r.absmin )
		&& serverGameData.GetAbsmax( reference, destination->r.absmax )
		&& serverGameData.GetCurrentOrigin( reference,
			destination->r.currentOrigin )
		&& serverGameData.GetCurrentAngles( reference,
			destination->r.currentAngles )
		&& serverGameData.GetOwnerNum( reference, destination->r.ownerNum )
		&& serverGameData.GetSharedStateOrigin2( reference,
			destination->r.s.origin2 ) ? qtrue : qfalse;
}


qboolean SV_ClientIsRTCWCastAI( const client_t *client ) {
	if ( !Com_GameIsRTCW() || !client->gentity ) return qfalse;
	sharedEntity_t entity;
	if ( !SV_ReadGameEntity( client->gentity, &entity ) ) {
		Com_Error( ERR_DROP, "%s: invalid client gentity", __func__ );
		return qfalse;
	}
	return SV_QBool( ( entity.r.svFlags & SVF_RTCW_CASTAI ) != 0 );
}

/* ET 2.60b leaves a client out of broadcast commands when its entity carries
 * SVF_BOT (8 in r.svFlags, +0x128 in the record): et.exe tests it at 0x448884
 * and etded.exe at 0x42f0ef. The entity's flag decides, not the client's
 * address type, as in retail. */
qboolean SV_ClientIsWolfETBot( const client_t *client ) {
	if ( !Com_GameIsWolfET() || !client->gentity ) return qfalse;
	sharedEntity_t entity;
	if ( !SV_ReadGameEntity( client->gentity, &entity ) ) {
		Com_Error( ERR_DROP, "%s: invalid client gentity", __func__ );
		return qfalse;
	}
	return SV_QBool( ( entity.r.svFlags & SVF_BOT ) != 0 );
}

bool SV_WolfETSnapshotCallback( int entityNum, int clientNum ) {
	if ( !Com_GameIsWolfET() ) return true;
	fntech3::server::GameEntityRef reference;
	int enabled = 0;
	if ( clientNum < 0 || clientNum >= serverGameData.ClientCount() ||
		!serverGameData.EntityRefAt( entityNum, reference ) ||
		!serverGameData.GetWolfETSnapshotCallback( reference, enabled ) ) {
		Com_Error( ERR_DROP, "Invalid ET snapshot callback entity/client %i/%i", entityNum, clientNum );
		return false;
	}
	if ( !enabled ) return true;
	if ( !gvm ) {
		Com_Error( ERR_DROP, "ET snapshot callback has no game module" );
		return false;
	}
	return VM_Call( gvm, 2, GAME_SNAPSHOT_CALLBACK, entityNum, clientNum ) != 0;
}

/* RTCW MP's SP_worldspawn copies the map's worldspawn spawnflags into the world
 * record's r.worldflags, which map_restart reads to keep g_gametype to a type
 * the map allows. Retail WolfMP.exe reads them at 0x432f0f..0x432f19, from a
 * record past the published entity count that SV_GameEntityArena binds. */
int SV_RTCWMPWorldflags( void ) {
	fntech3::server::GameEntityRef world;
	int flags = 0;
	if ( !serverGameData.EntityRefAt( ENTITYNUM_WORLD, world )
		|| !serverGameData.GetRTCWMPWorldflags( world, flags ) ) {
		Com_Error( ERR_DROP, "%s: the world record, entity %i, is not readable as "
			"RTCW MP's, so map_restart cannot check the game types the map allows",
			__func__, ENTITYNUM_WORLD );
		return 0;
	}
	return flags;
}


qboolean SV_WriteGameEntity( void *entity,
	const sharedEntity_t *source ) {
	fntech3::server::GameEntityRef reference;
	if ( !source || !serverGameData.EntityRefForPointer( entity, reference ) ) {
		return qfalse;
	}
	int previousFlags;
	if ( !serverGameData.GetSvFlags( reference, previousFlags ) ) return qfalse;

	return serverGameData.WriteEntityState( reference, source->s )
		&& serverGameData.SetLinked( reference, source->r.linked )
		&& serverGameData.SetLinkcount( reference, source->r.linkcount )
		&& serverGameData.SetSvFlags( reference, ServerFlagsFromInternal(
			serverGameData.Profile(), source->r.svFlags, previousFlags ) )
		&& serverGameData.SetSingleClient( reference, source->r.singleClient )
		&& serverGameData.SetBmodel( reference, source->r.bmodel )
		&& serverGameData.SetMins( reference, source->r.mins )
		&& serverGameData.SetMaxs( reference, source->r.maxs )
		&& serverGameData.SetContents( reference, source->r.contents )
		&& serverGameData.SetAbsmin( reference, source->r.absmin )
		&& serverGameData.SetAbsmax( reference, source->r.absmax )
		&& serverGameData.SetCurrentOrigin( reference,
			source->r.currentOrigin )
		&& serverGameData.SetCurrentAngles( reference,
			source->r.currentAngles )
		&& serverGameData.SetOwnerNum( reference, source->r.ownerNum )
		&& serverGameData.SetSharedStateOrigin2( reference,
			source->r.s.origin2 ) ? qtrue : qfalse;
}


qboolean SV_ReadGameEntityByNum( int num, sharedEntity_t *destination ) {
	fntech3::server::GameEntityRef reference;
	if ( !destination || !serverGameData.EntityRefAt( num, reference ) ) {
		return qfalse;
	}
	return SV_ReadGameEntity(
		static_cast<const sharedEntity_t *>( serverGameData.EntityPointer( reference ) ),
		destination );
}


qboolean SV_WriteGameEntityByNum( int num, const sharedEntity_t *source ) {
	fntech3::server::GameEntityRef reference;
	if ( !source || !serverGameData.EntityRefAt( num, reference ) ) {
		return qfalse;
	}
	return SV_WriteGameEntity(
		static_cast<sharedEntity_t *>( serverGameData.EntityPointer( reference ) ),
		source );
}


qboolean SV_ReadGameClient( int num, playerState_t *destination ) {
	return destination && serverGameData.ReadPlayerState( num, *destination )
		? qtrue : qfalse;
}


qboolean SV_SetGameClientPing( int num, int ping ) {
	return serverGameData.SetPlayerPing( num, ping ) ? qtrue : qfalse;
}


svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	fntech3::server::GameEntityRef reference;
	int number = -1;
	if ( !serverGameData.EntityRefForPointer( gEnt, reference )
		|| !serverGameData.GetNumber( reference, number )
		|| number < 0 || number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
		return nullptr;
	}
	return &sv.svEntities[ number ];
}


/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
static void SV_GameSendServerCommand( int clientNum, const char *text ) {
	if ( clientNum == -1 ) {
		SV_SendServerCommand( nullptr, "%s", text );
	} else {
		if ( clientNum < 0 || clientNum >= sv.maxclients ) {
			return;
		}
		SV_SendServerCommand( svs.clients + clientNum, "%s", text );
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
static void SV_GameDropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason );
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
static void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;
	sharedEntity_t	entity;

	if ( !name ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if ( name[0] != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}

	if ( !SV_ReadGameEntity( ent, &entity ) ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: bad gEnt" );
		return;
	}
	entity.s.modelindex = SV_ParseInt( name + 1 );

	h = CM_InlineModel( entity.s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy (mins, entity.r.mins);
	VectorCopy (maxs, entity.r.maxs);
	entity.r.bmodel = qtrue;

	entity.r.contents = -1;		// we don't know exactly what is in the brushes
	if ( !SV_WriteGameEntity( ent, &entity ) ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: game-data write failed" );
		return;
	}

	SV_LinkEntity( ent );		// FIXME: remove
}


/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS( const vec3_t p1, const vec3_t p2 )
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && ( cluster < 0 || cluster >= CM_NumClusters()
		|| !(mask[cluster>>3] & (1<<(cluster&7))) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks sight
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
static bool SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 )
{
	int		leafnum;
	int		cluster;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);

	if ( mask && ( cluster < 0 || cluster >= CM_NumClusters()
		|| !(mask[cluster>>3] & (1<<(cluster&7))) ) )
		return false;

	return true;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
static void SV_AdjustAreaPortalState( sharedEntity_t *ent, bool open ) {
	svEntity_t	*svEnt;

	svEnt = SV_SvEntityForGentity( ent );
	if ( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, SV_QBool( open ) );
}


/*
==================
SV_EntityContact
==================
*/
static bool SV_EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t *gEnt, bool capsule ) {
	clipHandle_t	ch;
	trace_t			trace;
	sharedEntity_t entity;

	// check for exact collision
	if ( !SV_ReadGameEntity( gEnt, &entity ) ) {
		Com_Error( ERR_DROP, "SV_EntityContact: bad gEnt" );
		return false;
	}

	ch = SV_ClipHandleForEntity( gEnt, SV_QBool( capsule ) );
	CM_TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs,
		ch, -1, entity.r.currentOrigin, entity.r.currentAngles,
		SV_QBool( capsule ) );

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo
===============
*/
static void SV_GetServerinfo( char *buffer, int bufferSize ) {

	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	if ( sv.state != SS_GAME || !sv.configstrings[ CS_SERVERINFO ] ) {
		Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO, nullptr ), bufferSize );
	} else {
		Q_strncpyz( buffer, sv.configstrings[ CS_SERVERINFO ], bufferSize );
	}
}


/*
===============
SV_GameEntityArena

How many records the located entity array holds when the module publishes
fewer, or zero when the published count is the array's bound.

Original RTCW and ET modules own a fixed g_entities[MAX_GENTITIES] arena and
publish only its high-water mark, which starts at MAX_CLIENTS. Two records
past that mark are reached anyway. SP's G_LoadGame links restored camera and
mover entities before its final LocateGameData call. In MP and ET, dynamite
that stops moving takes the world, ENTITYNUM_WORLD, as its owner, and the
missile's next trace passes that owner as the pass entity, whose own owner
the server then reads. In ET, landmines, satchels, tripmines and smoke do
the same. Retail qagame_mp_x86.dll (RTCW MP) clears 1024 records of 1352
bytes (0x20041d3f), stores the world owner at 0x20047d7c and traces with it
at 0x20048b0d. ET 2.60b's clears 1024 of 1404 (0x20051ff7) and stores it at
0x20057a4e and 0x2005a385.
===============
*/
static int SV_GameEntityArena( void ) {
	if ( Com_GameIsRTCW() ) return 1 << FNTECH3_RTCW_GENTITYNUM_BITS;
	if ( Com_GameIsWolfET() ) return 1 << FNTECH3_WOLFET_GENTITYNUM_BITS;
	return 0;
}


/*
===============
SV_LocateGameData

===============
*/
static void SV_LocateGameData( intptr_t entityAddress, int numGEntities, int sizeofGEntity_t, intptr_t clientAddress, int sizeofGameClient ) {
	// Rejecting a new binding must also discard the previous server metadata.
	InvalidateServerGameData();
	fntech3::server::NativeGameDataBinding binding;
	binding.entityCount = numGEntities;
	// Keep the active count for snapshots, but admit handles within the arena.
	binding.entityCapacity = SV_GameEntityArena();
	binding.entityStride = sizeofGEntity_t > 0
		? static_cast<std::size_t>( sizeofGEntity_t ) : 0;
	binding.clientCount = Com_GameClientLimit();
	binding.clientStride = sizeofGameClient > 0
		? static_cast<std::size_t>( sizeofGameClient ) : 0;

	const bool qvmBacked = !gvm->entryPoint && !gvm->dllExports;
	if ( qvmBacked ) {
		// Do not pass these offsets through VM_ArgPtr: masking there would
		// turn a forged out-of-range offset into an apparently valid binding.
		if ( !fntech3::server::ResolveQvmGameDataPointer( gvm->dataBase,
			gvm->exactDataLength, entityAddress, binding.entityBase,
			binding.entitySpanBytes )
			|| !fntech3::server::ResolveQvmGameDataPointer( gvm->dataBase,
				gvm->exactDataLength, clientAddress, binding.clientBase,
				binding.clientSpanBytes ) ) {
			Com_Error( ERR_DROP, "%s: game data located outside VM data segment",
				__func__ );
			return;
		}
	}
	else {
		binding.entityBase = reinterpret_cast<void *>( entityAddress );
		binding.clientBase = reinterpret_cast<void *>( clientAddress );
		const auto claimedSpan = []( int count, int stride ) -> std::size_t {
			if ( count <= 0 || stride <= 0 ) {
				return 0;
			}
			const std::size_t unsignedCount = static_cast<std::size_t>( count );
			const std::size_t unsignedStride = static_cast<std::size_t>( stride );
			if ( unsignedStride > ( std::numeric_limits<std::size_t>::max )()
				/ unsignedCount ) {
				return 0;
			}
			return unsignedStride * unsignedCount;
		};
		binding.entitySpanBytes = claimedSpan( binding.entityCapacity ? binding.entityCapacity : numGEntities,
			sizeofGEntity_t );
		binding.clientSpanBytes = claimedSpan( binding.clientCount,
			sizeofGameClient );
	}

	const fntech3::server::GameDataBindError bindError =
		serverGameData.BindNative( Com_GameProfile()->id, binding );
	if ( bindError != fntech3::server::GameDataBindError::None ) {
		Com_Error( ERR_DROP, "%s: invalid %s game-data binding: %s",
			__func__, Com_GameProfile()->displayName,
			fntech3::server::GameDataBindErrorName( bindError ) );
		return;
	}

	sv.gentities = binding.entityBase;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = binding.clientBase;
	sv.gameClientSize = sizeofGameClient;
}


/*
===============
SV_GetUsercmd
===============
*/
static void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( SV_IsClientIndex( clientNum ) ) {
		*cmd = SV_ClientForIndex( clientNum ).lastUsercmd;
	} else {
		Com_Error( ERR_DROP, "%s(): bad clientNum: %i", __func__, clientNum );
	}
}


static void SV_GetQ3Usercmd( int clientNum,
	fntech3_q3_usercmd_t *destination ) {
	usercmd_t internal;

	SV_GetUsercmd( clientNum, &internal );
	FnTech3_Q3UsercmdFromInternal( destination, &internal );
}

static void SV_GetQLUsercmd( int clientNum,
	fntech3_ql_usercmd_t *destination ) {
	usercmd_t internal;

	SV_GetUsercmd( clientNum, &internal );
	FnTech3_QLUsercmdFromInternal( destination, &internal );
}

template<typename ExternalCommand>
static void SV_GetRTCWUsercmd( int clientNum, ExternalCommand *destination ) {
	usercmd_t internal{};
	SV_GetUsercmd( clientNum, &internal );
	if ( !destination || !fntech3::rtcw::state::FromInternal( internal, *destination ) ) {
		Com_Error( ERR_DROP, "Invalid RTCW usercmd module transfer" );
	}
}


//==============================================

static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


/*
====================
VM_ArgPtr
====================
*/
static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || gvm == nullptr )
		return nullptr;

	if ( gvm->entryPoint || gvm->dllExports )
		return reinterpret_cast<void *>( intValue );
	else
		return gvm->dataBase + ( intValue & gvm->dataMask );
}

class VmArgPtr {
public:
	explicit VmArgPtr( void *ptr ) : ptr_( ptr ) {
	}

	template <typename T>
	operator T *() const {
		return static_cast<T *>( ptr_ );
	}

	operator void *() const {
		return ptr_;
	}

private:
	void *ptr_;
};

#undef VMA


/*
====================
GVM_ArgPtr

exported version
====================
*/
void *GVM_ArgPtr( intptr_t intValue ) 
{
	return VM_ArgPtr( intValue );
}


static bool SV_GetValue( char* value, int valueSize, const char* key )
{
	if ( !Q_stricmp( key, "SVF_SELF_PORTAL2_Q3E" ) )
	{
		Com_sprintf( value, valueSize, "%i", SVF_SELF_PORTAL2 );
		return true;
	}

	if ( !Q_stricmp( key, "trap_Cvar_SetDescription_Q3E" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_CVAR_SETDESCRIPTION );
		return true;
	}

	return false;
}

static constexpr const char *SV_GAME_RANKINGS_PROVIDER = "Build-disabled (FnQL engine-only)";
static constexpr const char *SV_GAME_RANKINGS_POLICY = "compatibility-disabled (no live rankings owner)";

static qboolean sv_gameRankStubAnnounced = qfalse;
static int sv_gameRankStubServerId = -1;

void SV_GameRefreshRankingsPolicyCvars( void ) {
	if ( !Com_GameIsQuakeLive() ) {
		return;
	}

	Cvar_Set( "sv_rankingsProvider", SV_GAME_RANKINGS_PROVIDER );
	Cvar_Set( "sv_rankingsPolicy", SV_GAME_RANKINGS_POLICY );
	Cvar_Set( "sv_rankingsActive", "0" );
}

static void SV_GameRankLogDisabledState( void ) {
	if ( sv_gameRankStubAnnounced ) {
		return;
	}

	Com_Printf( "Rankings disabled by FnQL engine policy; exposing retained compatibility surface only (%s [%s]).\n",
		SV_GAME_RANKINGS_PROVIDER, SV_GAME_RANKINGS_POLICY );
	sv_gameRankStubAnnounced = qtrue;
}

static void SV_GameRankBegin( const char *gameKey ) {
	(void)gameKey;
	SV_GameRankLogDisabledState();

	if ( sv_enableRankings && sv_enableRankings->integer != 0 ) {
		Com_Printf( "Rankings requested but disabled by FnQL engine policy; forcing sv_enableRankings back to 0 (%s [%s]).\n",
			SV_GAME_RANKINGS_PROVIDER, SV_GAME_RANKINGS_POLICY );
		Cvar_Set( "sv_enableRankings", "0" );
	}

	SV_GameRefreshRankingsPolicyCvars();
	sv_gameRankStubServerId = sv.serverId;
}

static void SV_GameRankPoll( void ) {
}

static qboolean SV_GameRankCheckInit( void ) {
	return sv_gameRankStubServerId == sv.serverId ? qtrue : qfalse;
}

static qboolean SV_GameRankActive( void ) {
	return qfalse;
}

static int SV_GameRankUserStatus( int index ) {
	(void)index;
	return 0;
}

static void SV_GameRankUserReset( int index ) {
	(void)index;
}

static void SV_GameRankReportInt( int index1, int index2, int key, int value, qboolean accum ) {
	(void)index1;
	(void)index2;
	(void)key;
	(void)value;
	(void)accum;
}

static void SV_GameRankReportStr( int index1, int index2, int key, const char *value ) {
	(void)index1;
	(void)index2;
	(void)key;
	(void)value;
}


/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
static void SV_RTCWTrace( fntech3_rtcw_trace_t *destination,
	const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
	int passEntityNum, int contentmask, qboolean capsule ) {
	if ( !destination || !start || !end ) {
		Com_Error( ERR_DROP, "Invalid RTCW game trace arguments" );
	}
	trace_t result{};
	SV_Trace( &result, start, mins, maxs, end, passEntityNum, contentmask, capsule );
	if ( !fntech3::rtcw::state::FromInternal( result, *destination ) ) {
		Com_Error( ERR_DROP, "Invalid RTCW game trace result" );
	}
}

#include "server_rtcw_world.hpp"
#include "server_rtcw_botlib.hpp"

// Original RTCW asks the local cgame for its current skeletal pose. Keep the
// server record separate from both the engine orientation and cgame's copy.
static intptr_t SV_RTCWGetTag(const vm_t *source, intptr_t client, intptr_t name, intptr_t output) {
	if (!Com_GameIsRTCW()) Com_Error(ERR_DROP, "RTCW tag lookup requires an RTCW profile");
#if defined(DEDICATED) || defined(USE_DEDICATED)
	return qfalse;
#else
	if (com_dedicated->integer) return qfalse;
	const int number = SV_RTCWInteger(client);
	if (!source || source->index != VM_GAME || number < 0 || number >= Com_GameProfile()->maxClients)
		Com_Error(ERR_DROP, "Invalid RTCW tag client");
	const auto tagName = SV_RTCWString<64>(source, name);
	void *destination = SV_RTCWPointer(source, output, sizeof(fntech3_rtcw_orientation_t));
	fntech3_rtcw_orientation_t external;
	std::memcpy(&external, destination, sizeof(external));
	orientation_t internal;
	std::memcpy(internal.origin, external.origin, sizeof(external.origin));
	std::memcpy(internal.axis, external.axis, sizeof(external.axis));
	const intptr_t result = CL_RTCWGetTag(number, tagName.data(), &internal);
	if (result && (!SV_FiniteWorldVector(internal.origin) ||
		!SV_FiniteWorldVector(internal.axis[0]) || !SV_FiniteWorldVector(internal.axis[1]) ||
		!SV_FiniteWorldVector(internal.axis[2])))
		Com_Error(ERR_DROP, "Invalid RTCW tag orientation");
	std::memcpy(external.origin, internal.origin, sizeof(external.origin));
	std::memcpy(external.axis, internal.axis, sizeof(external.axis));
	std::memcpy(destination, &external, sizeof(external));
	return result;
#endif
}

static intptr_t SV_RTCWGameSharedCall( fntech3::rtcw::module::Call call ) {
	using fntech3::rtcw::module::Call;
	switch ( call ) {
#define RTCW_SHARED_CALL(name) case Call::name: return name;
	RTCW_SHARED_CALL(G_PRINT)
	RTCW_SHARED_CALL(G_ERROR)
	RTCW_SHARED_CALL(G_MILLISECONDS)
	RTCW_SHARED_CALL(G_CVAR_SET)
	RTCW_SHARED_CALL(G_CVAR_VARIABLE_INTEGER_VALUE)
	RTCW_SHARED_CALL(G_CVAR_VARIABLE_STRING_BUFFER)
	RTCW_SHARED_CALL(G_ARGC)
	RTCW_SHARED_CALL(G_ARGV)
	RTCW_SHARED_CALL(G_SEND_CONSOLE_COMMAND)
	RTCW_SHARED_CALL(G_FS_FOPEN_FILE)
	RTCW_SHARED_CALL(G_FS_READ)
	RTCW_SHARED_CALL(G_FS_WRITE)
	RTCW_SHARED_CALL(G_FS_FCLOSE_FILE)
	RTCW_SHARED_CALL(G_FS_GETFILELIST)
	RTCW_SHARED_CALL(G_LOCATE_GAME_DATA)
	RTCW_SHARED_CALL(G_DROP_CLIENT)
	RTCW_SHARED_CALL(G_SEND_SERVER_COMMAND)
	RTCW_SHARED_CALL(G_SET_CONFIGSTRING)
	RTCW_SHARED_CALL(G_GET_CONFIGSTRING)
	RTCW_SHARED_CALL(G_SET_USERINFO)
	RTCW_SHARED_CALL(G_GET_USERINFO)
	RTCW_SHARED_CALL(G_GET_SERVERINFO)
	RTCW_SHARED_CALL(G_GET_ENTITY_TOKEN)
	RTCW_SHARED_CALL(G_POINT_CONTENTS)
	RTCW_SHARED_CALL(G_SNAPVECTOR)
#undef RTCW_SHARED_CALL
	default: return -1;
	}
}

#include "server_wolfet_game.hpp"
#include "server_jk2_game.hpp"
#include "server_jka_game.hpp"
#include "server_stvef_game.hpp"
#include "server_stvef_sp_game.hpp"
#include "server_stvef_sp_save.hpp"
#include "server_q3_ihv_game.hpp"
#include "server_alice_game.hpp"
#include "server_mohaa_game.hpp"
#include "server_q3test_game.hpp"
#include "server_q3test_net.hpp"

static intptr_t SV_GameSystemCalls( intptr_t *args ) {

	// detect infinite loops in QVM code by counting syscalls per VM_Call invocation
	// the stock id 1.32 qagame.qvm has a bug in ClientSpawn() where a do/while(1) loop
	// retrying spawn point selection can loop forever if all spawn points have FL_NO_BOTS
	// set, causing the server to hang at 100% CPU
	if ( gvm->syscallCount >= 1024 * 1024 ) {
		Com_Error( ERR_DROP, "game VM syscall overflow - Loss of control in VM" );
	}
	++gvm->syscallCount;

	const auto VMA = [args]( int index ) {
		return VmArgPtr( VM_ArgPtr( args[index] ) );
	};

	intptr_t operation = args[0];
	if (Com_GameIsWolfET()) {
		intptr_t result = 0;
		if (SV_WolfETGameCall(args, operation, result)) return result;
	}
	if ( Com_GameIsSTVEFMP() ) {
		intptr_t result = 0;
		if ( SV_STVEFGameCall( args, operation, result ) ) return result;
	}
	if ( Com_GameIsJKAMP() ) {
		intptr_t result = 0;
		if ( SV_JKAGameCall( args, operation, result ) ) return result;
	}
	if ( Com_GameIsJK2MP() ) {
		intptr_t result = 0;
		if ( SV_JK2GameCall( args, operation, result ) ) return result;
	}
	if ( Com_GameIsRTCW() ) {
		using namespace fntech3::rtcw::module;
		const auto call = Lookup( Com_GameProfile()->id, Module::GAME, Direction::IMPORT, args[0] );
		intptr_t worldResult = 0;
		if ( SV_RTCWWorldCall( gvm, call, args, worldResult ) ) return worldResult;
		if ( SV_RTCWBotLibCall( gvm, call, args, worldResult ) ) return worldResult;
		switch ( call ) {
		case Call::G_FS_RENAME:
			VM_RTCWRenameFile( gvm, args[1], args[2] );
			return 0;
		case Call::G_GETTAG:
			return SV_RTCWGetTag(gvm, args[1], args[2], args[3]);
		case Call::G_TRACE:
		case Call::G_TRACECAPSULE:
			VM_CHECKBOUNDS( gvm, args[1], sizeof( fntech3_rtcw_trace_t ) );
			for ( int index = 2; index <= 5; ++index ) {
				if ( args[index] ) VM_CHECKBOUNDS( gvm, args[index], sizeof( vec3_t ) );
			}
			SV_RTCWTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7],
				call == Call::G_TRACECAPSULE ? qtrue : qfalse );
			return 0;
		case Call::G_REAL_TIME:
			VM_CHECKBOUNDS( gvm, args[1], sizeof( fntech3_rtcw_qtime_t ) );
			return Com_RealTimeRTCW( VMA(1) );
		case Call::G_ENDGAME:
			// SP only (MP has no such ordinal). The final campaign script and
			// the end-of-level reload call it; original SP never returns from
			// it, ending the frame to open the endgame menu (sv_game.c).
			Com_Error( ERR_ENDGAME, "endgame" );
			return 0;
		case Call::G_CVAR_REGISTER:
			VM_CHECKBOUNDS( gvm, args[1], sizeof( fntech3_rtcw_vmCvar_t ) );
			Cvar_RegisterRTCW( VMA(1), VMA(2), VMA(3), args[4], gvm->privateFlag );
			return 0;
		case Call::G_CVAR_UPDATE:
			VM_CHECKBOUNDS( gvm, args[1], sizeof( fntech3_rtcw_vmCvar_t ) );
			Cvar_UpdateRTCW( VMA(1), gvm->privateFlag );
			return 0;
		case Call::G_GET_USERCMD:
			if ( Com_GameIsRTCWSP() ) {
				VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_rtcw_sp_usercmd_t ) );
				SV_GetRTCWUsercmd<fntech3_rtcw_sp_usercmd_t>( args[1], VMA(2) );
			} else {
				VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_rtcw_mp_usercmd_t ) );
				SV_GetRTCWUsercmd<fntech3_rtcw_mp_usercmd_t>( args[1], VMA(2) );
			}
			return 0;
		default:
			operation = SV_RTCWGameSharedCall( call );
			if ( operation < 0 ) {
				const auto *descriptor = Describe( call );
				Com_Error( ERR_DROP, "Unsupported %s game import %ld (%s)",
					Com_GameProfile()->key, static_cast<long>( args[0] ),
					descriptor ? descriptor->name : "unknown ordinal" );
				return 0;
			}
			break;
		}
	}
	switch( operation ) {
	case G_PRINT:
		Com_Printf( "%s", static_cast<const char *>( VMA(1) ) );
		return 0;
	case G_ERROR:
		Com_Error( ERR_DROP, "%s", static_cast<const char *>( VMA(1) ) );
		return 0;
	case G_MILLISECONDS:
		return Sys_Milliseconds();
	case G_CVAR_REGISTER:
		if ( gvm->dllExports ) {
			Cvar_Register( VMA(1), VMA(2), VMA(3), args[4], gvm->privateFlag );
		} else {
			Cvar_RegisterLegacy( VMA(1), VMA(2), VMA(3), args[4], gvm->privateFlag );
		}
		return 0;
	case G_CVAR_UPDATE:
		if ( gvm->dllExports ) {
			Cvar_Update( VMA(1), gvm->privateFlag );
		} else {
			Cvar_UpdateLegacy( VMA(1), gvm->privateFlag );
		}
		return 0;
	case G_CVAR_SET:
		SV_SetGameCvar( VMA(1), VMA(2) );
		return 0;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		return Cvar_VariableIntegerValue( VMA(1) );
	case G_CVAR_VARIABLE_STRING_BUFFER:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		Cvar_VariableStringBufferSafe( VMA(1), VMA(2), args[3], gvm->privateFlag );
		return 0;
	case G_ARGC:
		return Cmd_Argc();
	case G_ARGV:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case G_SEND_CONSOLE_COMMAND:
		if ( args[1] >= EXEC_NOW && args[1] <= EXEC_APPEND ) {
			Cbuf_ExecuteText( static_cast<cbufExec_t>( args[1] ), VMA(2) );
		} else {
			Cbuf_ExecuteText( EXEC_APPEND, VMA(1) );
		}
		return 0;

	case G_FS_FOPEN_FILE:
		return FS_VM_OpenFile( VMA(1), VMA(2), static_cast<fsMode_t>( args[3] ), H_QAGAME );
	case G_FS_READ:
		if ( args[3] == 0 ) // UrT may pass this with args[2]=-1 and cause false bounds check error
			return 0;
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		return FS_VM_ReadFile( VMA(1), args[2], args[3], H_QAGAME );
	case G_FS_WRITE:
		if ( Com_GameIsRTCW() ) return VM_RTCWWriteFile( gvm, args[1], args[2], args[3], H_QAGAME );
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		FS_VM_WriteFile( VMA(1), args[2], args[3], H_QAGAME );
		return 0;
	case G_FS_FCLOSE_FILE:
		FS_VM_CloseFile( args[1], H_QAGAME );
		return 0;
	case G_FS_SEEK:
		return FS_VM_SeekFile( args[1], args[2], static_cast<fsOrigin_t>( args[3] ), H_QAGAME );

	case G_FS_GETFILELIST:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		return FS_GetFileList( VMA(1), VMA(2), VMA(3), args[4] );

	case G_LOCATE_GAME_DATA:
		SV_LocateGameData( args[1], args[2], args[3], args[4], args[5] );
		return 0;
	case G_DROP_CLIENT:
		SV_GameDropClient( args[1], VMA(2) );
		return 0;
	case G_SEND_SERVER_COMMAND:
		SV_GameSendServerCommand( args[1], VMA(2) );
		return 0;
	case G_LINKENTITY:
		SV_LinkEntity( VMA(1) );
		return 0;
	case G_UNLINKENTITY:
		SV_UnlinkEntity( VMA(1) );
		return 0;
	case G_ENTITIES_IN_BOX:
		VM_CHECKBOUNDS( gvm, args[3], args[4] * sizeof( int ) );
		return SV_AreaEntities( VMA(1), VMA(2), VMA(3), args[4] );
	case G_ENTITY_CONTACT:
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), false );
	case G_ENTITY_CONTACTCAPSULE:
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), true );
	case G_TRACE:
		SV_Trace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse );
		return 0;
	case G_TRACECAPSULE:
		SV_Trace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue );
		return 0;
	case G_POINT_CONTENTS:
		return SV_PointContents( VMA(1), args[2] );
	case G_SET_BRUSH_MODEL:
		SV_SetBrushModel( VMA(1), VMA(2) );
		return 0;
	case G_IN_PVS:
		return SV_inPVS( VMA(1), VMA(2) );
	case G_IN_PVS_IGNORE_PORTALS:
		return SV_inPVSIgnorePortals( VMA(1), VMA(2) );

	case G_SET_CONFIGSTRING:
		SV_SetConfigstring( args[1], VMA(2) );
		return 0;
	case G_GET_CONFIGSTRING:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetConfigstring( args[1], VMA(2), args[3] );
		return 0;
	case G_SET_USERINFO:
		SV_SetUserinfo( args[1], VMA(2) );
		return 0;
	case G_GET_USERINFO:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetUserinfo( args[1], VMA(2), args[3] );
		return 0;
	case G_GET_SERVERINFO:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		SV_GetServerinfo( VMA(1), args[2] );
		return 0;
	case G_STEAMID_QUERY:
		return SV_ClientSteamId( args[1], static_cast<unsigned int *>( VMA(2) ), static_cast<unsigned int *>( VMA(3) ) );
	case G_STEAM_AUTH_VALIDATE:
		return SV_VerifyClientSteamAuth( args[1] );
	case G_RANK_BEGIN:
		SV_GameRankBegin( static_cast<const char *>( VMA(1) ) );
		return 0;
	case G_RANK_POLL:
		SV_GameRankPoll();
		return 0;
	case G_RANK_CHECK_INIT:
		return SV_GameRankCheckInit();
	case G_RANK_ACTIVE:
		return SV_GameRankActive();
	case G_RANK_USER_STATUS:
		return SV_GameRankUserStatus( args[1] );
	case G_RANK_USER_RESET:
		SV_GameRankUserReset( args[1] );
		return 0;
	case G_RANK_REPORT_INT:
		SV_GameRankReportInt( args[1], args[2], args[3], args[4], args[5] != 0 ? qtrue : qfalse );
		return 0;
	case G_RANK_REPORT_STR:
		SV_GameRankReportStr( args[1], args[2], args[3], static_cast<const char *>( VMA(4) ) );
		return 0;
	case G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( VMA(1), args[2] != 0 );
		return 0;
	case G_AREAS_CONNECTED:
		return CM_AreasConnected( args[1], args[2] );

	case G_BOT_ALLOCATE_CLIENT:
		return SV_BotAllocateClient();
	case G_BOT_FREE_CLIENT:
		SV_BotFreeClient( args[1] );
		return 0;

	case G_GET_USERCMD:
		if ( gvm->dllExports ) {
			VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_ql_usercmd_t ) );
			SV_GetQLUsercmd( args[1], VMA(2) );
		} else {
			VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_q3_usercmd_t ) );
			SV_GetQ3Usercmd( args[1], VMA(2) );
		}
		return 0;
	case G_GET_ENTITY_TOKEN:
		{
			const char *s = COM_Parse( &sv.entityParsePoint );
			VM_CHECKBOUNDS( gvm, args[1], args[2] );
			//Q_strncpyz( VMA(1), s, args[2] );
			// we can't use our optimized Q_strncpyz() function
			// because of uninitialized memory bug in defrag mod
			{
				char *dst = VMA(1);
				const int size = args[2]-1;
				if ( size >= 0 ) {
					Q_strncpy( dst, const_cast<char *>( s ), size );
					dst[size] = '\0';
				}
			}
			if ( !sv.entityParsePoint && s[0] == '\0' ) {
				return qfalse;
			} else {
				return qtrue;
			}
		}

	case G_DEBUG_POLYGON_CREATE:
		return BotImport_DebugPolygonCreate( args[1], args[2], VMA(3) );
	case G_DEBUG_POLYGON_DELETE:
		BotImport_DebugPolygonDelete( args[1] );
		return 0;
	case G_REAL_TIME:
		return Com_RealTime( VMA(1) );
	case G_SNAPVECTOR:
		Sys_SnapVector( VMA(1) );
		return 0;

		//====================================

	case BOTLIB_SETUP:
		return SV_BotLibSetup();
	case BOTLIB_SHUTDOWN:
		return SV_BotLibShutdown();
	case BOTLIB_LIBVAR_SET:
		return botlib_export->BotLibVarSet( VMA(1), VMA(2) );
	case BOTLIB_LIBVAR_GET:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		return botlib_export->BotLibVarGet( VMA(1), VMA(2), args[3] );

	case BOTLIB_PC_ADD_GLOBAL_DEFINE:
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case BOTLIB_PC_LOAD_SOURCE:
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case BOTLIB_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case BOTLIB_PC_READ_TOKEN:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( pc_token_t ) );
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case BOTLIB_PC_SOURCE_FILE_AND_LINE:
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case BOTLIB_START_FRAME:
		return botlib_export->BotLibStartFrame( VMF(1) );
	case BOTLIB_LOAD_MAP:
		return botlib_export->BotLibLoadMap( VMA(1) );
	case BOTLIB_UPDATENTITY:
		return botlib_export->BotLibUpdateEntity( args[1], VMA(2) );
	case BOTLIB_TEST:
		return botlib_export->Test( args[1], VMA(2), VMA(3), VMA(4) );

	case BOTLIB_GET_SNAPSHOT_ENTITY:
		return SV_BotGetSnapshotEntity( args[1], args[2] );
	case BOTLIB_GET_CONSOLE_MESSAGE:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		return SV_BotGetConsoleMessage( args[1], VMA(2), args[3] );
	case BOTLIB_USER_COMMAND:
		{
			const int clientNum = args[1];
			if ( SV_IsClientIndex( clientNum ) )
			{
				usercmd_t command{};
				if ( gvm->dllExports ) {
					VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_ql_usercmd_t ) );
				} else {
					VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_q3_usercmd_t ) );
				}
				if ( !VMA(2) ) {
					Com_Error( ERR_DROP, "BOTLIB_USER_COMMAND: null command" );
					return 0;
				}
				if ( gvm->dllExports ) {
					FnTech3_InternalUsercmdFromQL( &command, VMA(2) );
				} else {
					FnTech3_InternalUsercmdFromQ3( &command, VMA(2) );
				}
				SV_ClientThink( &SV_ClientForIndex( clientNum ), &command );
			}
		}
		return 0;

	case BOTLIB_AAS_BBOX_AREAS:
		return botlib_export->aas.AAS_BBoxAreas( VMA(1), VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_AREA_INFO:
		return botlib_export->aas.AAS_AreaInfo( args[1], VMA(2) );
	case BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
		return botlib_export->aas.AAS_AlternativeRouteGoals( VMA(1), args[2], VMA(3), args[4], args[5], VMA(6), args[7], args[8] );
	case BOTLIB_AAS_ENTITY_INFO:
		botlib_export->aas.AAS_EntityInfo( args[1], VMA(2) );
		return 0;

	case BOTLIB_AAS_INITIALIZED:
		return botlib_export->aas.AAS_Initialized();
	case BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
		botlib_export->aas.AAS_PresenceTypeBoundingBox( args[1], VMA(2), VMA(3) );
		return 0;
	case BOTLIB_AAS_TIME:
		return FloatAsInt( botlib_export->aas.AAS_Time() );

	case BOTLIB_AAS_POINT_AREA_NUM:
		return botlib_export->aas.AAS_PointAreaNum( VMA(1) );
	case BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
		return botlib_export->aas.AAS_PointReachabilityAreaIndex( VMA(1) );
	case BOTLIB_AAS_TRACE_AREAS:
		return botlib_export->aas.AAS_TraceAreas( VMA(1), VMA(2), VMA(3), VMA(4), args[5] );

	case BOTLIB_AAS_POINT_CONTENTS:
		return botlib_export->aas.AAS_PointContents( VMA(1) );
	case BOTLIB_AAS_NEXT_BSP_ENTITY:
		return botlib_export->aas.AAS_NextBSPEntity( args[1] );
	case BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		return botlib_export->aas.AAS_ValueForBSPEpairKey( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_VectorForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_FloatForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_IntForBSPEpairKey( args[1], VMA(2), VMA(3) );

	case BOTLIB_AAS_AREA_REACHABILITY:
		return botlib_export->aas.AAS_AreaReachability( args[1] );

	case BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
		return botlib_export->aas.AAS_AreaTravelTimeToGoalArea( args[1], VMA(2), args[3], args[4] );
	case BOTLIB_AAS_ENABLE_ROUTING_AREA:
		return botlib_export->aas.AAS_EnableRoutingArea( args[1], args[2] );
	case BOTLIB_AAS_PREDICT_ROUTE:
		return botlib_export->aas.AAS_PredictRoute( VMA(1), args[2], VMA(3), args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11] );

	case BOTLIB_AAS_SWIMMING:
		return botlib_export->aas.AAS_Swimming( VMA(1) );
	case BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
		return botlib_export->aas.AAS_PredictClientMovement( VMA(1), args[2], VMA(3), args[4], args[5],
			VMA(6), VMA(7), args[8], args[9], VMF(10), args[11], args[12], args[13] );

	case BOTLIB_EA_SAY:
		botlib_export->ea.EA_Say( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_SAY_TEAM:
		botlib_export->ea.EA_SayTeam( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_COMMAND:
		botlib_export->ea.EA_Command( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_ACTION:
		botlib_export->ea.EA_Action( args[1], args[2] );
		return 0;
	case BOTLIB_EA_GESTURE:
		botlib_export->ea.EA_Gesture( args[1] );
		return 0;
	case BOTLIB_EA_TALK:
		botlib_export->ea.EA_Talk( args[1] );
		return 0;
	case BOTLIB_EA_ATTACK:
		botlib_export->ea.EA_Attack( args[1] );
		return 0;
	case BOTLIB_EA_USE:
		botlib_export->ea.EA_Use( args[1] );
		return 0;
	case BOTLIB_EA_RESPAWN:
		botlib_export->ea.EA_Respawn( args[1] );
		return 0;
	case BOTLIB_EA_CROUCH:
		botlib_export->ea.EA_Crouch( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_UP:
		botlib_export->ea.EA_MoveUp( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_DOWN:
		botlib_export->ea.EA_MoveDown( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_FORWARD:
		botlib_export->ea.EA_MoveForward( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_BACK:
		botlib_export->ea.EA_MoveBack( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_LEFT:
		botlib_export->ea.EA_MoveLeft( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_RIGHT:
		botlib_export->ea.EA_MoveRight( args[1] );
		return 0;

	case BOTLIB_EA_SELECT_WEAPON:
		botlib_export->ea.EA_SelectWeapon( args[1], args[2] );
		return 0;
	case BOTLIB_EA_JUMP:
		botlib_export->ea.EA_Jump( args[1] );
		return 0;
	case BOTLIB_EA_DELAYED_JUMP:
		botlib_export->ea.EA_DelayedJump( args[1] );
		return 0;
	case BOTLIB_EA_MOVE:
		botlib_export->ea.EA_Move( args[1], VMA(2), VMF(3) );
		return 0;
	case BOTLIB_EA_VIEW:
		botlib_export->ea.EA_View( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_END_REGULAR:
		botlib_export->ea.EA_EndRegular( args[1], VMF(2) );
		return 0;
	case BOTLIB_EA_GET_INPUT:
		botlib_export->ea.EA_GetInput( args[1], VMF(2), VMA(3) );
		return 0;
	case BOTLIB_EA_RESET_INPUT:
		botlib_export->ea.EA_ResetInput( args[1] );
		return 0;

	case BOTLIB_AI_LOAD_CHARACTER:
		return botlib_export->ai.BotLoadCharacter( VMA(1), VMF(2) );
	case BOTLIB_AI_FREE_CHARACTER:
		botlib_export->ai.BotFreeCharacter( args[1] );
		return 0;
	case BOTLIB_AI_CHARACTERISTIC_FLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_Float( args[1], args[2] ) );
	case BOTLIB_AI_CHARACTERISTIC_BFLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_BFloat( args[1], args[2], VMF(3), VMF(4) ) );
	case BOTLIB_AI_CHARACTERISTIC_INTEGER:
		return botlib_export->ai.Characteristic_Integer( args[1], args[2] );
	case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
		return botlib_export->ai.Characteristic_BInteger( args[1], args[2], args[3], args[4] );
	case BOTLIB_AI_CHARACTERISTIC_STRING:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		botlib_export->ai.Characteristic_String( args[1], args[2], VMA(3), args[4] );
		return 0;

	case BOTLIB_AI_ALLOC_CHAT_STATE:
		return botlib_export->ai.BotAllocChatState();
	case BOTLIB_AI_FREE_CHAT_STATE:
		botlib_export->ai.BotFreeChatState( args[1] );
		return 0;
	case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
		botlib_export->ai.BotQueueConsoleMessage( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
		botlib_export->ai.BotRemoveConsoleMessage( args[1], args[2] );
		return 0;
	case BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNextConsoleMessage( args[1], VMA(2) );
	case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNumConsoleMessages( args[1] );
	case BOTLIB_AI_INITIAL_CHAT:
		botlib_export->ai.BotInitialChat( args[1], VMA(2), args[3], VMA(4), VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11) );
		return 0;
	case BOTLIB_AI_NUM_INITIAL_CHATS:
		return botlib_export->ai.BotNumInitialChats( args[1], VMA(2) );
	case BOTLIB_AI_REPLY_CHAT:
		return botlib_export->ai.BotReplyChat( args[1], VMA(2), args[3], args[4], VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11), VMA(12) );
	case BOTLIB_AI_CHAT_LENGTH:
		return botlib_export->ai.BotChatLength( args[1] );
	case BOTLIB_AI_ENTER_CHAT:
		botlib_export->ai.BotEnterChat( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_GET_CHAT_MESSAGE:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		botlib_export->ai.BotGetChatMessage( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_STRING_CONTAINS:
		return botlib_export->ai.StringContains( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_FIND_MATCH:
		return botlib_export->ai.BotFindMatch( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_MATCH_VARIABLE:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		botlib_export->ai.BotMatchVariable( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_UNIFY_WHITE_SPACES:
		botlib_export->ai.UnifyWhiteSpaces( VMA(1) );
		return 0;
	case BOTLIB_AI_REPLACE_SYNONYMS:
		botlib_export->ai.BotReplaceSynonyms( VMA(1), VM_DATA_GUARD_SIZE, args[2] );
		return 0;
	case BOTLIB_AI_LOAD_CHAT_FILE:
		return botlib_export->ai.BotLoadChatFile( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_SET_CHAT_GENDER:
		botlib_export->ai.BotSetChatGender( args[1], args[2] );
		return 0;
	case BOTLIB_AI_SET_CHAT_NAME:
		botlib_export->ai.BotSetChatName( args[1], VMA(2), args[3] );
		return 0;

	case BOTLIB_AI_RESET_GOAL_STATE:
		botlib_export->ai.BotResetGoalState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_AVOID_GOALS:
		botlib_export->ai.BotResetAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
		botlib_export->ai.BotRemoveFromAvoidGoals( args[1], args[2] );
		return 0;
	case BOTLIB_AI_PUSH_GOAL:
		botlib_export->ai.BotPushGoal( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_POP_GOAL:
		botlib_export->ai.BotPopGoal( args[1] );
		return 0;
	case BOTLIB_AI_EMPTY_GOAL_STACK:
		botlib_export->ai.BotEmptyGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_AVOID_GOALS:
		botlib_export->ai.BotDumpAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_GOAL_STACK:
		botlib_export->ai.BotDumpGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_GOAL_NAME:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		botlib_export->ai.BotGoalName( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_GET_TOP_GOAL:
		return botlib_export->ai.BotGetTopGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_SECOND_GOAL:
		return botlib_export->ai.BotGetSecondGoal( args[1], VMA(2) );
	case BOTLIB_AI_CHOOSE_LTG_ITEM:
		return botlib_export->ai.BotChooseLTGItem( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AI_CHOOSE_NBG_ITEM:
		return botlib_export->ai.BotChooseNBGItem( args[1], VMA(2), VMA(3), args[4], VMA(5), VMF(6) );
	case BOTLIB_AI_TOUCHING_GOAL:
		return botlib_export->ai.BotTouchingGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
		return botlib_export->ai.BotItemGoalInVisButNotVisible( args[1], VMA(2), VMA(3), VMA(4) );
	case BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
		return botlib_export->ai.BotGetLevelItemGoal( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
		return botlib_export->ai.BotGetNextCampSpotGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_MAP_LOCATION_GOAL:
		return botlib_export->ai.BotGetMapLocationGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_AVOID_GOAL_TIME:
		return FloatAsInt( botlib_export->ai.BotAvoidGoalTime( args[1], args[2] ) );
	case BOTLIB_AI_SET_AVOID_GOAL_TIME:
		botlib_export->ai.BotSetAvoidGoalTime( args[1], args[2], VMF(3));
		return 0;
	case BOTLIB_AI_INIT_LEVEL_ITEMS:
		botlib_export->ai.BotInitLevelItems();
		return 0;
	case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
		botlib_export->ai.BotUpdateEntityItems();
		return 0;
	case BOTLIB_AI_LOAD_ITEM_WEIGHTS:
		return botlib_export->ai.BotLoadItemWeights( args[1], VMA(2) );
	case BOTLIB_AI_FREE_ITEM_WEIGHTS:
		botlib_export->ai.BotFreeItemWeights( args[1] );
		return 0;
	case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotInterbreedGoalFuzzyLogic( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotSaveGoalFuzzyLogic( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotMutateGoalFuzzyLogic( args[1], VMF(2) );
		return 0;
	case BOTLIB_AI_ALLOC_GOAL_STATE:
		return botlib_export->ai.BotAllocGoalState( args[1] );
	case BOTLIB_AI_FREE_GOAL_STATE:
		botlib_export->ai.BotFreeGoalState( args[1] );
		return 0;

	case BOTLIB_AI_RESET_MOVE_STATE:
		botlib_export->ai.BotResetMoveState( args[1] );
		return 0;
	case BOTLIB_AI_ADD_AVOID_SPOT:
		botlib_export->ai.BotAddAvoidSpot( args[1], VMA(2), VMF(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_TO_GOAL:
		botlib_export->ai.BotMoveToGoal( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_IN_DIRECTION:
		return botlib_export->ai.BotMoveInDirection( args[1], VMA(2), VMF(3), args[4] );
	case BOTLIB_AI_RESET_AVOID_REACH:
		botlib_export->ai.BotResetAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_RESET_LAST_AVOID_REACH:
		botlib_export->ai.BotResetLastAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_REACHABILITY_AREA:
		return botlib_export->ai.BotReachabilityArea( VMA(1), args[2] );
	case BOTLIB_AI_MOVEMENT_VIEW_TARGET:
		return botlib_export->ai.BotMovementViewTarget( args[1], VMA(2), args[3], VMF(4), VMA(5) );
	case BOTLIB_AI_PREDICT_VISIBLE_POSITION:
		return botlib_export->ai.BotPredictVisiblePosition( VMA(1), args[2], VMA(3), args[4], VMA(5) );
	case BOTLIB_AI_ALLOC_MOVE_STATE:
		return botlib_export->ai.BotAllocMoveState();
	case BOTLIB_AI_FREE_MOVE_STATE:
		botlib_export->ai.BotFreeMoveState( args[1] );
		return 0;
	case BOTLIB_AI_INIT_MOVE_STATE:
		botlib_export->ai.BotInitMoveState( args[1], VMA(2) );
		return 0;

	case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
		return botlib_export->ai.BotChooseBestFightWeapon( args[1], VMA(2) );
	case BOTLIB_AI_GET_WEAPON_INFO:
		botlib_export->ai.BotGetWeaponInfo( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
		return botlib_export->ai.BotLoadWeaponWeights( args[1], VMA(2) );
	case BOTLIB_AI_ALLOC_WEAPON_STATE:
		return botlib_export->ai.BotAllocWeaponState();
	case BOTLIB_AI_FREE_WEAPON_STATE:
		botlib_export->ai.BotFreeWeaponState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_WEAPON_STATE:
		botlib_export->ai.BotResetWeaponState( args[1] );
		return 0;

	case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
		return botlib_export->ai.GeneticParentsAndChildSelection(args[1], VMA(2), VMA(3), VMA(4), VMA(5));

	// shared syscalls

	case TRAP_MEMSET:
		VM_CHECKBOUNDS( gvm, args[1], args[3] );
		Com_Memset( VMA(1), args[2], args[3] );
		return args[1];

	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( gvm, args[1], args[2], args[3] );
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( gvm, args[1], args[3] );
		Q_strncpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_SIN:
		return FloatAsInt( sin( VMF(1) ) );

	case TRAP_COS:
		return FloatAsInt( cos( VMF(1) ) );

	case TRAP_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );

	case TRAP_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case TRAP_MATRIXMULTIPLY:
		MatrixMultiply( VMA(1), VMA(2), VMA(3) );
		return 0;

	case TRAP_ANGLEVECTORS:
		AngleVectors( VMA(1), VMA(2), VMA(3), VMA(4) );
		return 0;

	case TRAP_PERPENDICULARVECTOR:
		PerpendicularVector( VMA(1), VMA(2) );
		return 0;

	case TRAP_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );

	case TRAP_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );

	case TRAP_TESTPRINTINT:
		return Com_sprintf( static_cast<char *>( VMA(1) ), MAX_STRING_CHARS, "%i", static_cast<int>( args[2] ) );

	case TRAP_TESTPRINTFLOAT:
		return Com_sprintf( static_cast<char *>( VMA(1) ), MAX_STRING_CHARS, "%f", VMF(2) );

	case G_CVAR_SETDESCRIPTION:
		Cvar_SetDescription2( VMA(1), VMA(2) );
		return 0;

	case G_TRAP_GETVALUE:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		return SV_GetValue( VMA(1), args[2], VMA(3) );

	default:
		Com_Error( ERR_DROP, "Bad game system trap: %ld", (long int) args[0] );
	}
	return 0;
}


/*
====================
SV_DllSyscall
====================
*/
static intptr_t QDECL SV_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	std::array<intptr_t, MAX_VMMAIN_CALL_ARGS> args{};
	va_list	ap;

	args[0] = arg;
	va_start( ap, arg );
	for ( std::size_t i = 1; i < args.size(); i++ ) {
		args[ i ] = va_arg( ap, intptr_t );
	}
	va_end( ap );

	return SV_GameSystemCalls( args.data() );
#else
	return SV_GameSystemCalls( &arg );
#endif
}

typedef void (QDECL *ql_import_f)( void );

static ql_import_f ql_game_imports[GAME_NATIVE_IMPORT_COUNT];
static_assert( G_QL_IMPORT_SUBMIT_MATCH_REPORT == 185, "QL qagame direct import slot 185 must remain MATCH_REPORT" );
static_assert( G_QL_IMPORT_REPORT_PLAYER_EVENT == 186, "QL qagame direct import slot 186 must remain PLAYER_EVENT" );
static_assert( G_QL_IMPORT_MARK_CLIENT_GOT_CP == 192, "QL qagame direct import slot 192 must remain MARK_CLIENT_GOT_CP" );
static_assert( G_QL_IMPORT_RETURN_ZERO_198 == 198, "QL qagame direct import slot 198 must remain an explicit zero stub" );
static_assert( G_QL_IMPORT_RETURN_ZERO_199 == 199, "QL qagame direct import slot 199 must remain an explicit zero stub" );
static_assert( G_QL_IMPORT_STEAMID_QUERY == 200, "QL qagame direct import slot 200 must remain STEAMID_QUERY" );
static_assert( G_QL_IMPORT_FACTORY_EXISTS == 206, "QL qagame direct import slot 206 must remain FACTORY_EXISTS" );
static_assert( G_QL_IMPORT_COUNT == 207, "QL qagame direct import slab size must preserve recovered retail gaps" );
static qboolean sv_gameCvarsRegistered = qfalse;

static void SV_ResetGameCvarRegistration( void ) {
	sv_gameCvarsRegistered = qfalse;
}

static qboolean SV_CallGameRegisterCvarsOnce( vm_t *vm );

template <typename T, std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, int> = 0>
static intptr_t SV_GameImportArg( T value ) {
	return static_cast<intptr_t>( value );
}

template <typename T>
static intptr_t SV_GameImportArg( T *value ) {
	return reinterpret_cast<intptr_t>( value );
}

template <typename... Args>
static intptr_t SV_GameImportCall( intptr_t callNum, Args... values ) {
	static_assert( sizeof...( values ) + 1 <= MAX_VMMAIN_CALL_ARGS,
		"qagame native import call exceeds VM argument buffer" );
	std::array<intptr_t, MAX_VMMAIN_CALL_ARGS> args{};
	std::size_t index = 1;

	args[0] = callNum;
	( ( args[index++] = SV_GameImportArg( values ) ), ... );

	return SV_GameSystemCalls( args.data() );
}

static float SV_GameImportFloat( intptr_t value ) {
	floatint_t fi;
	fi.i = static_cast<int>( value );
	return fi.f;
}

#define QL_IMPORT0( name, trap ) \
	static intptr_t QDECL name( void ) { return SV_GameImportCall( trap ); }
#define QL_IMPORT1( name, trap, t1 ) \
	static intptr_t QDECL name( t1 a1 ) { return SV_GameImportCall( trap, a1 ); }
#define QL_IMPORT2( name, trap, t1, t2 ) \
	static intptr_t QDECL name( t1 a1, t2 a2 ) { return SV_GameImportCall( trap, a1, a2 ); }
#define QL_IMPORT3( name, trap, t1, t2, t3 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3 ) { return SV_GameImportCall( trap, a1, a2, a3 ); }
#define QL_IMPORT4( name, trap, t1, t2, t3, t4 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3, t4 a4 ) { return SV_GameImportCall( trap, a1, a2, a3, a4 ); }
#define QL_IMPORT5( name, trap, t1, t2, t3, t4, t5 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3, t4 a4, t5 a5 ) { return SV_GameImportCall( trap, a1, a2, a3, a4, a5 ); }
#define QL_IMPORT6( name, trap, t1, t2, t3, t4, t5, t6 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6 ) { return SV_GameImportCall( trap, a1, a2, a3, a4, a5, a6 ); }
#define QL_IMPORT7( name, trap, t1, t2, t3, t4, t5, t6, t7 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7 ) { return SV_GameImportCall( trap, a1, a2, a3, a4, a5, a6, a7 ); }
#define QL_IMPORT8( name, trap, t1, t2, t3, t4, t5, t6, t7, t8 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8 ) { return SV_GameImportCall( trap, a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define QL_IMPORT11( name, trap, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9, t10 a10, t11 a11 ) { return SV_GameImportCall( trap, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 ); }
#define QL_IMPORT12( name, trap, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12 ) \
	static intptr_t QDECL name( t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9, t10 a10, t11 a11, t12 a12 ) { return SV_GameImportCall( trap, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12 ); }

QL_IMPORT1( QL_G_trap_Printf, G_PRINT, const char * )
QL_IMPORT1( QL_G_trap_Error, G_ERROR, const char * )
QL_IMPORT0( QL_G_trap_Milliseconds, G_MILLISECONDS )
QL_IMPORT1( QL_G_trap_RealTime, G_REAL_TIME, qtime_t * )
QL_IMPORT3( QL_G_trap_Argv, G_ARGV, int, char *, int )
QL_IMPORT3( QL_G_trap_FS_FOpenFile, G_FS_FOPEN_FILE, const char *, fileHandle_t *, fsMode_t )
QL_IMPORT3( QL_G_trap_FS_Read, G_FS_READ, void *, int, fileHandle_t )
QL_IMPORT3( QL_G_trap_FS_Write, G_FS_WRITE, const void *, int, fileHandle_t )
QL_IMPORT1( QL_G_trap_FS_FCloseFile, G_FS_FCLOSE_FILE, fileHandle_t )
QL_IMPORT4( QL_G_trap_FS_GetFileList, G_FS_GETFILELIST, const char *, const char *, char *, int )
QL_IMPORT3( QL_G_trap_FS_Seek, G_FS_SEEK, fileHandle_t, long, int )
QL_IMPORT2( QL_G_trap_SendConsoleCommand, G_SEND_CONSOLE_COMMAND, int, const char * )

static void QDECL QL_G_trap_SendConsoleCommandText( const char *text ) {
	SV_GameImportCall( G_SEND_CONSOLE_COMMAND, static_cast<int>( EXEC_APPEND ), text );
}

QL_IMPORT4( QL_G_trap_Cvar_Register, G_CVAR_REGISTER, vmCvar_t *, const char *, const char *, int )
QL_IMPORT1( QL_G_trap_Cvar_Update, G_CVAR_UPDATE, vmCvar_t * )
QL_IMPORT2( QL_G_trap_Cvar_Set, G_CVAR_SET, const char *, const char * )

// Retail qagame's direct import slot 11 returns the host cvar as a float.
// Keep this distinct from the legacy G_CVAR_VARIABLE_INTEGER_VALUE syscall:
// using its integer return through a native float ABI silently corrupts
// fractional values on 32-bit Windows.
static float QDECL QL_G_trap_Cvar_VariableValue( const char *varName ) {
	return Cvar_VariableValue( varName );
}

QL_IMPORT1( QL_G_trap_Cvar_VariableIntegerValue, G_CVAR_VARIABLE_INTEGER_VALUE, const char * )
QL_IMPORT3( QL_G_trap_Cvar_VariableStringBuffer, G_CVAR_VARIABLE_STRING_BUFFER, const char *, char *, int )
QL_IMPORT0( QL_G_trap_Argc, G_ARGC )
QL_IMPORT5( QL_G_trap_LocateGameData, G_LOCATE_GAME_DATA, sharedEntity_t *, int, int, playerState_t *, int )
QL_IMPORT2( QL_G_trap_DropClient, G_DROP_CLIENT, int, const char * )
QL_IMPORT2( QL_G_trap_SendServerCommand, G_SEND_SERVER_COMMAND, int, const char * )
QL_IMPORT2( QL_G_trap_SetConfigstring, G_SET_CONFIGSTRING, int, const char * )
QL_IMPORT3( QL_G_trap_GetConfigstring, G_GET_CONFIGSTRING, int, char *, int )
QL_IMPORT3( QL_G_trap_GetUserinfo, G_GET_USERINFO, int, char *, int )
QL_IMPORT2( QL_G_trap_SetUserinfo, G_SET_USERINFO, int, const char * )
QL_IMPORT2( QL_G_trap_GetServerinfo, G_GET_SERVERINFO, char *, int )
QL_IMPORT2( QL_G_trap_SetBrushModel, G_SET_BRUSH_MODEL, sharedEntity_t *, const char * )
QL_IMPORT7( QL_G_trap_Trace, G_TRACE, trace_t *, const float *, const float *, const float *, const float *, int, int )
QL_IMPORT7( QL_G_trap_TraceCapsule, G_TRACECAPSULE, trace_t *, const float *, const float *, const float *, const float *, int, int )
QL_IMPORT2( QL_G_trap_PointContents, G_POINT_CONTENTS, const float *, int )
QL_IMPORT2( QL_G_trap_InPVS, G_IN_PVS, const float *, const float * )
QL_IMPORT2( QL_G_trap_InPVSIgnorePortals, G_IN_PVS_IGNORE_PORTALS, const float *, const float * )
QL_IMPORT2( QL_G_trap_AdjustAreaPortalState, G_ADJUST_AREA_PORTAL_STATE, sharedEntity_t *, qboolean )
QL_IMPORT2( QL_G_trap_AreasConnected, G_AREAS_CONNECTED, int, int )
QL_IMPORT1( QL_G_trap_LinkEntity, G_LINKENTITY, sharedEntity_t * )
QL_IMPORT1( QL_G_trap_UnlinkEntity, G_UNLINKENTITY, sharedEntity_t * )
QL_IMPORT4( QL_G_trap_EntitiesInBox, G_ENTITIES_IN_BOX, const float *, const float *, int *, int )
QL_IMPORT3( QL_G_trap_EntityContact, G_ENTITY_CONTACT, const float *, const float *, const sharedEntity_t * )
QL_IMPORT3( QL_G_trap_EntityContactCapsule, G_ENTITY_CONTACTCAPSULE, const float *, const float *, const sharedEntity_t * )
QL_IMPORT0( QL_G_trap_BotAllocateClient, G_BOT_ALLOCATE_CLIENT )
QL_IMPORT1( QL_G_trap_BotFreeClient, G_BOT_FREE_CLIENT, int )
QL_IMPORT2( QL_G_trap_GetUsercmd, G_GET_USERCMD, int, fntech3_ql_usercmd_t * )
QL_IMPORT2( QL_G_trap_GetEntityToken, G_GET_ENTITY_TOKEN, char *, int )
QL_IMPORT3( QL_G_trap_DebugPolygonCreate, G_DEBUG_POLYGON_CREATE, int, int, vec3_t * )
QL_IMPORT1( QL_G_trap_DebugPolygonDelete, G_DEBUG_POLYGON_DELETE, int )
QL_IMPORT1( QL_G_trap_SnapVector, G_SNAPVECTOR, float * )

QL_IMPORT0( QL_G_trap_BotLibSetup, BOTLIB_SETUP )
QL_IMPORT0( QL_G_trap_BotLibShutdown, BOTLIB_SHUTDOWN )
QL_IMPORT2( QL_G_trap_BotLibVarSet, BOTLIB_LIBVAR_SET, const char *, const char * )
QL_IMPORT3( QL_G_trap_BotLibVarGet, BOTLIB_LIBVAR_GET, const char *, char *, int )
QL_IMPORT1( QL_G_trap_BotLibDefine, BOTLIB_PC_ADD_GLOBAL_DEFINE, const char * )
QL_IMPORT1( QL_G_trap_BotLibLoadMap, BOTLIB_LOAD_MAP, const char * )
QL_IMPORT2( QL_G_trap_BotLibUpdateEntity, BOTLIB_UPDATENTITY, int, void * )
QL_IMPORT4( QL_G_trap_BotLibTest, BOTLIB_TEST, int, char *, vec3_t, vec3_t )
QL_IMPORT2( QL_G_trap_BotGetSnapshotEntity, BOTLIB_GET_SNAPSHOT_ENTITY, int, int )
QL_IMPORT3( QL_G_trap_BotGetServerCommand, BOTLIB_GET_CONSOLE_MESSAGE, int, char *, int )
QL_IMPORT2( QL_G_trap_BotUserCommand, BOTLIB_USER_COMMAND, int, fntech3_ql_usercmd_t * )

QL_IMPORT4( QL_G_trap_AAS_BBoxAreas, BOTLIB_AAS_BBOX_AREAS, vec3_t, vec3_t, int *, int )
QL_IMPORT2( QL_G_trap_AAS_AreaInfo, BOTLIB_AAS_AREA_INFO, int, void * )
QL_IMPORT2( QL_G_trap_AAS_EntityInfo, BOTLIB_AAS_ENTITY_INFO, int, void * )
QL_IMPORT0( QL_G_trap_AAS_Initialized, BOTLIB_AAS_INITIALIZED )
QL_IMPORT3( QL_G_trap_AAS_PresenceTypeBoundingBox, BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX, int, vec3_t, vec3_t )
QL_IMPORT1( QL_G_trap_AAS_PointAreaNum, BOTLIB_AAS_POINT_AREA_NUM, vec3_t )
QL_IMPORT1( QL_G_trap_AAS_PointReachabilityAreaIndex, BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX, vec3_t )
QL_IMPORT5( QL_G_trap_AAS_TraceAreas, BOTLIB_AAS_TRACE_AREAS, vec3_t, vec3_t, int *, vec3_t *, int )
QL_IMPORT1( QL_G_trap_AAS_PointContents, BOTLIB_AAS_POINT_CONTENTS, vec3_t )
QL_IMPORT1( QL_G_trap_AAS_NextBSPEntity, BOTLIB_AAS_NEXT_BSP_ENTITY, int )
QL_IMPORT4( QL_G_trap_AAS_ValueForBSPEpairKey, BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY, int, const char *, char *, int )
QL_IMPORT3( QL_G_trap_AAS_VectorForBSPEpairKey, BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY, int, const char *, vec3_t )
QL_IMPORT3( QL_G_trap_AAS_FloatForBSPEpairKey, BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY, int, const char *, float * )
QL_IMPORT3( QL_G_trap_AAS_IntForBSPEpairKey, BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY, int, const char *, int * )
QL_IMPORT1( QL_G_trap_AAS_AreaReachability, BOTLIB_AAS_AREA_REACHABILITY, int )
QL_IMPORT4( QL_G_trap_AAS_AreaTravelTimeToGoalArea, BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA, int, vec3_t, int, int )
QL_IMPORT2( QL_G_trap_AAS_EnableRoutingArea, BOTLIB_AAS_ENABLE_ROUTING_AREA, int, int )
QL_IMPORT11( QL_G_trap_AAS_PredictRoute, BOTLIB_AAS_PREDICT_ROUTE, void *, int, vec3_t, int, int, int, int, int, int, int, int )
QL_IMPORT8( QL_G_trap_AAS_AlternativeRouteGoals, BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL, vec3_t, int, vec3_t, int, int, void *, int, int )
QL_IMPORT1( QL_G_trap_AAS_Swimming, BOTLIB_AAS_SWIMMING, vec3_t )

QL_IMPORT2( QL_G_trap_EA_Say, BOTLIB_EA_SAY, int, const char * )
QL_IMPORT2( QL_G_trap_EA_SayTeam, BOTLIB_EA_SAY_TEAM, int, const char * )
QL_IMPORT2( QL_G_trap_EA_Command, BOTLIB_EA_COMMAND, int, const char * )
QL_IMPORT2( QL_G_trap_EA_Action, BOTLIB_EA_ACTION, int, int )
QL_IMPORT1( QL_G_trap_EA_Gesture, BOTLIB_EA_GESTURE, int )
QL_IMPORT1( QL_G_trap_EA_Talk, BOTLIB_EA_TALK, int )
QL_IMPORT1( QL_G_trap_EA_Attack, BOTLIB_EA_ATTACK, int )
QL_IMPORT1( QL_G_trap_EA_Use, BOTLIB_EA_USE, int )
QL_IMPORT1( QL_G_trap_EA_Respawn, BOTLIB_EA_RESPAWN, int )
QL_IMPORT1( QL_G_trap_EA_Crouch, BOTLIB_EA_CROUCH, int )
QL_IMPORT1( QL_G_trap_EA_MoveUp, BOTLIB_EA_MOVE_UP, int )
QL_IMPORT1( QL_G_trap_EA_MoveDown, BOTLIB_EA_MOVE_DOWN, int )
QL_IMPORT1( QL_G_trap_EA_MoveForward, BOTLIB_EA_MOVE_FORWARD, int )
QL_IMPORT1( QL_G_trap_EA_MoveBack, BOTLIB_EA_MOVE_BACK, int )
QL_IMPORT1( QL_G_trap_EA_MoveLeft, BOTLIB_EA_MOVE_LEFT, int )
QL_IMPORT1( QL_G_trap_EA_MoveRight, BOTLIB_EA_MOVE_RIGHT, int )
QL_IMPORT2( QL_G_trap_EA_SelectWeapon, BOTLIB_EA_SELECT_WEAPON, int, int )
QL_IMPORT1( QL_G_trap_EA_Jump, BOTLIB_EA_JUMP, int )
QL_IMPORT1( QL_G_trap_EA_DelayedJump, BOTLIB_EA_DELAYED_JUMP, int )
QL_IMPORT2( QL_G_trap_EA_View, BOTLIB_EA_VIEW, int, vec3_t )
QL_IMPORT1( QL_G_trap_EA_ResetInput, BOTLIB_EA_RESET_INPUT, int )

QL_IMPORT1( QL_G_trap_BotFreeCharacter, BOTLIB_AI_FREE_CHARACTER, int )
QL_IMPORT2( QL_G_trap_Characteristic_Integer, BOTLIB_AI_CHARACTERISTIC_INTEGER, int, int )
QL_IMPORT4( QL_G_trap_Characteristic_BInteger, BOTLIB_AI_CHARACTERISTIC_BINTEGER, int, int, int, int )
QL_IMPORT4( QL_G_trap_Characteristic_String, BOTLIB_AI_CHARACTERISTIC_STRING, int, int, char *, int )
QL_IMPORT0( QL_G_trap_BotAllocChatState, BOTLIB_AI_ALLOC_CHAT_STATE )
QL_IMPORT1( QL_G_trap_BotFreeChatState, BOTLIB_AI_FREE_CHAT_STATE, int )
QL_IMPORT3( QL_G_trap_BotQueueConsoleMessage, BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, int, int, char * )
QL_IMPORT2( QL_G_trap_BotRemoveConsoleMessage, BOTLIB_AI_REMOVE_CONSOLE_MESSAGE, int, int )
QL_IMPORT2( QL_G_trap_BotNextConsoleMessage, BOTLIB_AI_NEXT_CONSOLE_MESSAGE, int, void * )
QL_IMPORT1( QL_G_trap_BotNumConsoleMessages, BOTLIB_AI_NUM_CONSOLE_MESSAGE, int )
QL_IMPORT11( QL_G_trap_BotInitialChat, BOTLIB_AI_INITIAL_CHAT, int, char *, int, char *, char *, char *, char *, char *, char *, char *, char * )
QL_IMPORT2( QL_G_trap_BotNumInitialChats, BOTLIB_AI_NUM_INITIAL_CHATS, int, char * )
QL_IMPORT12( QL_G_trap_BotReplyChat, BOTLIB_AI_REPLY_CHAT, int, char *, int, int, char *, char *, char *, char *, char *, char *, char *, char * )
QL_IMPORT1( QL_G_trap_BotChatLength, BOTLIB_AI_CHAT_LENGTH, int )
QL_IMPORT3( QL_G_trap_BotEnterChat, BOTLIB_AI_ENTER_CHAT, int, int, int )
QL_IMPORT3( QL_G_trap_BotGetChatMessage, BOTLIB_AI_GET_CHAT_MESSAGE, int, char *, int )
QL_IMPORT3( QL_G_trap_StringContains, BOTLIB_AI_STRING_CONTAINS, char *, char *, int )
QL_IMPORT3( QL_G_trap_BotFindMatch, BOTLIB_AI_FIND_MATCH, char *, void *, unsigned long )
QL_IMPORT4( QL_G_trap_BotMatchVariable, BOTLIB_AI_MATCH_VARIABLE, void *, int, char *, int )
QL_IMPORT1( QL_G_trap_UnifyWhiteSpaces, BOTLIB_AI_UNIFY_WHITE_SPACES, char * )
QL_IMPORT2( QL_G_trap_BotReplaceSynonyms, BOTLIB_AI_REPLACE_SYNONYMS, char *, unsigned long )
QL_IMPORT3( QL_G_trap_BotLoadChatFile, BOTLIB_AI_LOAD_CHAT_FILE, int, char *, char * )
QL_IMPORT2( QL_G_trap_BotSetChatGender, BOTLIB_AI_SET_CHAT_GENDER, int, int )
QL_IMPORT3( QL_G_trap_BotSetChatName, BOTLIB_AI_SET_CHAT_NAME, int, char *, int )
QL_IMPORT1( QL_G_trap_BotResetGoalState, BOTLIB_AI_RESET_GOAL_STATE, int )
QL_IMPORT2( QL_G_trap_BotRemoveFromAvoidGoals, BOTLIB_AI_REMOVE_FROM_AVOID_GOALS, int, int )
QL_IMPORT1( QL_G_trap_BotResetAvoidGoals, BOTLIB_AI_RESET_AVOID_GOALS, int )
QL_IMPORT2( QL_G_trap_BotPushGoal, BOTLIB_AI_PUSH_GOAL, int, void * )
QL_IMPORT1( QL_G_trap_BotPopGoal, BOTLIB_AI_POP_GOAL, int )
QL_IMPORT1( QL_G_trap_BotEmptyGoalStack, BOTLIB_AI_EMPTY_GOAL_STACK, int )
QL_IMPORT1( QL_G_trap_BotDumpAvoidGoals, BOTLIB_AI_DUMP_AVOID_GOALS, int )
QL_IMPORT1( QL_G_trap_BotDumpGoalStack, BOTLIB_AI_DUMP_GOAL_STACK, int )
QL_IMPORT3( QL_G_trap_BotGoalName, BOTLIB_AI_GOAL_NAME, int, char *, int )
QL_IMPORT2( QL_G_trap_BotGetTopGoal, BOTLIB_AI_GET_TOP_GOAL, int, void * )
QL_IMPORT2( QL_G_trap_BotGetSecondGoal, BOTLIB_AI_GET_SECOND_GOAL, int, void * )
QL_IMPORT4( QL_G_trap_BotChooseLTGItem, BOTLIB_AI_CHOOSE_LTG_ITEM, int, vec3_t, int *, int )
QL_IMPORT2( QL_G_trap_BotTouchingGoal, BOTLIB_AI_TOUCHING_GOAL, vec3_t, void * )
QL_IMPORT4( QL_G_trap_BotItemGoalInVisButNotVisible, BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, int, vec3_t, vec3_t, void * )
QL_IMPORT3( QL_G_trap_BotGetLevelItemGoal, BOTLIB_AI_GET_LEVEL_ITEM_GOAL, int, char *, void * )
QL_IMPORT2( QL_G_trap_BotGetNextCampSpotGoal, BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, int, void * )
QL_IMPORT2( QL_G_trap_BotGetMapLocationGoal, BOTLIB_AI_GET_MAP_LOCATION_GOAL, char *, void * )
QL_IMPORT0( QL_G_trap_BotInitLevelItems, BOTLIB_AI_INIT_LEVEL_ITEMS )
QL_IMPORT0( QL_G_trap_BotUpdateEntityItems, BOTLIB_AI_UPDATE_ENTITY_ITEMS )
QL_IMPORT2( QL_G_trap_BotLoadItemWeights, BOTLIB_AI_LOAD_ITEM_WEIGHTS, int, char * )
QL_IMPORT1( QL_G_trap_BotFreeItemWeights, BOTLIB_AI_FREE_ITEM_WEIGHTS, int )
QL_IMPORT3( QL_G_trap_BotInterbreedGoalFuzzyLogic, BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC, int, int, int )
QL_IMPORT2( QL_G_trap_BotSaveGoalFuzzyLogic, BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, int, char * )
QL_IMPORT1( QL_G_trap_BotAllocGoalState, BOTLIB_AI_ALLOC_GOAL_STATE, int )
QL_IMPORT1( QL_G_trap_BotFreeGoalState, BOTLIB_AI_FREE_GOAL_STATE, int )
QL_IMPORT1( QL_G_trap_BotResetMoveState, BOTLIB_AI_RESET_MOVE_STATE, int )
QL_IMPORT4( QL_G_trap_BotMoveToGoal, BOTLIB_AI_MOVE_TO_GOAL, void *, int, void *, int )
QL_IMPORT1( QL_G_trap_BotResetAvoidReach, BOTLIB_AI_RESET_AVOID_REACH, int )
QL_IMPORT1( QL_G_trap_BotResetLastAvoidReach, BOTLIB_AI_RESET_LAST_AVOID_REACH, int )
QL_IMPORT2( QL_G_trap_BotReachabilityArea, BOTLIB_AI_REACHABILITY_AREA, vec3_t, int )
QL_IMPORT3( QL_G_trap_BotPredictVisiblePosition, BOTLIB_AI_PREDICT_VISIBLE_POSITION, vec3_t, int, void * )
QL_IMPORT0( QL_G_trap_BotAllocMoveState, BOTLIB_AI_ALLOC_MOVE_STATE )
QL_IMPORT1( QL_G_trap_BotFreeMoveState, BOTLIB_AI_FREE_MOVE_STATE, int )
QL_IMPORT2( QL_G_trap_BotInitMoveState, BOTLIB_AI_INIT_MOVE_STATE, int, void * )
QL_IMPORT2( QL_G_trap_BotChooseBestFightWeapon, BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON, int, int * )
QL_IMPORT3( QL_G_trap_BotGetWeaponInfo, BOTLIB_AI_GET_WEAPON_INFO, int, int, void * )
QL_IMPORT2( QL_G_trap_BotLoadWeaponWeights, BOTLIB_AI_LOAD_WEAPON_WEIGHTS, int, char * )
QL_IMPORT0( QL_G_trap_BotAllocWeaponState, BOTLIB_AI_ALLOC_WEAPON_STATE )
QL_IMPORT1( QL_G_trap_BotFreeWeaponState, BOTLIB_AI_FREE_WEAPON_STATE, int )
QL_IMPORT1( QL_G_trap_BotResetWeaponState, BOTLIB_AI_RESET_WEAPON_STATE, int )
QL_IMPORT5( QL_G_trap_GeneticParentsAndChildSelection, BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, int, float *, int *, int *, int * )
QL_IMPORT1( QL_G_trap_PC_LoadSource, BOTLIB_PC_LOAD_SOURCE, const char * )
QL_IMPORT1( QL_G_trap_PC_FreeSource, BOTLIB_PC_FREE_SOURCE, int )
QL_IMPORT2( QL_G_trap_PC_ReadToken, BOTLIB_PC_READ_TOKEN, int, void * )
QL_IMPORT3( QL_G_trap_PC_SourceFileAndLine, BOTLIB_PC_SOURCE_FILE_AND_LINE, int, char *, int * )
QL_IMPORT2( QL_G_trap_Cvar_SetDescription, G_CVAR_SETDESCRIPTION, const char *, const char * )
QL_IMPORT3( QL_G_trap_GetValue, G_TRAP_GETVALUE, char *, int, const char * )

static intptr_t QDECL QL_G_trap_BotLibStartFrame( float time ) {
	return SV_GameImportCall( BOTLIB_START_FRAME, FloatAsInt( time ) );
}

static float QDECL QL_G_trap_AAS_Time( void ) {
	return SV_GameImportFloat( SV_GameImportCall( BOTLIB_AAS_TIME ) );
}

static intptr_t QDECL QL_G_trap_AAS_PredictClientMovement( void *move, int entnum, const vec3_t origin,
		int presencetype, int onground, const vec3_t velocity, const vec3_t cmdmove,
		int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize ) {
	return SV_GameImportCall( BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT, move, entnum, origin,
		presencetype, onground, velocity, cmdmove, cmdframes, maxframes, FloatAsInt( frametime ),
		stopevent, stopareanum, visualize );
}

static void QDECL QL_G_trap_EA_Walk( int client ) {
	botlib_export->ea.EA_Action( client, ACTION_WALK );
}

static void QDECL QL_G_trap_EA_Move( int client, vec3_t dir, float speed ) {
	SV_GameImportCall( BOTLIB_EA_MOVE, client, dir, FloatAsInt( speed ) );
}

static void QDECL QL_G_trap_EA_EndRegular( int client, float thinktime ) {
	SV_GameImportCall( BOTLIB_EA_END_REGULAR, client, FloatAsInt( thinktime ) );
}

static void QDECL QL_G_trap_EA_GetInput( int client, float thinktime, void *input ) {
	SV_GameImportCall( BOTLIB_EA_GET_INPUT, client, FloatAsInt( thinktime ), input );
}

static intptr_t QDECL QL_G_trap_BotLoadCharacter( char *charfile, float skill ) {
	return SV_GameImportCall( BOTLIB_AI_LOAD_CHARACTER, charfile, FloatAsInt( skill ) );
}

static float QDECL QL_G_trap_Characteristic_Float( int character, int index ) {
	return SV_GameImportFloat( SV_GameImportCall( BOTLIB_AI_CHARACTERISTIC_FLOAT, character, index ) );
}

static float QDECL QL_G_trap_Characteristic_BFloat( int character, int index, float min, float max ) {
	return SV_GameImportFloat( SV_GameImportCall( BOTLIB_AI_CHARACTERISTIC_BFLOAT, character, index,
		FloatAsInt( min ), FloatAsInt( max ) ) );
}

static intptr_t QDECL QL_G_trap_BotChooseNBGItem( int goalstate, vec3_t origin, int *inventory,
		int travelflags, void *ltg, float maxtime ) {
	return SV_GameImportCall( BOTLIB_AI_CHOOSE_NBG_ITEM, goalstate, origin, inventory, travelflags,
		ltg, FloatAsInt( maxtime ) );
}

static float QDECL QL_G_trap_BotAvoidGoalTime( int goalstate, int number ) {
	return SV_GameImportFloat( SV_GameImportCall( BOTLIB_AI_AVOID_GOAL_TIME, goalstate, number ) );
}

static void QDECL QL_G_trap_BotSetAvoidGoalTime( int goalstate, int number, float avoidtime ) {
	SV_GameImportCall( BOTLIB_AI_SET_AVOID_GOAL_TIME, goalstate, number, FloatAsInt( avoidtime ) );
}

static void QDECL QL_G_trap_BotMutateGoalFuzzyLogic( int goalstate, float range ) {
	SV_GameImportCall( BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, goalstate, FloatAsInt( range ) );
}

static void QDECL QL_G_trap_BotAddAvoidSpot( int movestate, vec3_t origin, float radius, int type ) {
	SV_GameImportCall( BOTLIB_AI_ADD_AVOID_SPOT, movestate, origin, FloatAsInt( radius ), type );
}

static intptr_t QDECL QL_G_trap_BotMoveInDirection( int movestate, vec3_t dir, float speed, int type ) {
	return SV_GameImportCall( BOTLIB_AI_MOVE_IN_DIRECTION, movestate, dir, FloatAsInt( speed ), type );
}

static intptr_t QDECL QL_G_trap_BotMovementViewTarget( int movestate, void *goal, int travelflags,
		float lookahead, vec3_t target ) {
	return SV_GameImportCall( BOTLIB_AI_MOVEMENT_VIEW_TARGET, movestate, goal, travelflags,
		FloatAsInt( lookahead ), target );
}

static void QDECL QL_G_trap_Cmd_TokenizeString( const char *text ) {
	Cmd_TokenizeString( text );
}

static void QDECL QL_G_trap_Args( char *buffer, int bufferLength ) {
	Cmd_ArgsBuffer( buffer, bufferLength );
}

static cvar_t *QDECL QL_G_trap_Cvar_SetValue( const char *var_name, float value ) {
	char val[32];

	if ( value == static_cast<int>( value ) ) {
		Com_sprintf( val, sizeof( val ), "%i", static_cast<int>( value ) );
	} else {
		Com_sprintf( val, sizeof( val ), "%f", value );
	}

	return Cvar_Set2( var_name, val, qtrue );
}

static cvar_t *QDECL QL_G_trap_Cvar_RegisterBounded( vmCvar_t *cvar, const char *var_name,
		const char *value, const char *minValue, const char *maxValue, int flags ) {
	return Cvar_RegisterBounded( cvar, var_name, value, minValue, maxValue, flags,
		gvm ? gvm->privateFlag : CVAR_PRIVATE );
}

static void SV_GameResendConfigstring( int index ) {
	const char *val;
	int len;
	const int maxChunkSize = MAX_STRING_CHARS - 24;

	if ( index < 0 || index >= Com_GameConfigstringLimit() ) {
		Com_Error( ERR_DROP, "SV_GameResendConfigstring: bad index %i\n", index );
	}

	val = sv.configstrings[index];
	if ( !val || ( sv.state != SS_GAME && !sv.restarting ) ) {
		return;
	}

	for ( client_t &client : SV_Clients() ) {
		if ( client.state < CS_PRIMED ) {
			continue;
		}

		if ( index == CS_SERVERINFO && client.gentity ) {
			sharedEntity_t entity;
			if ( !SV_ReadGameEntity( client.gentity, &entity ) ) {
				Com_Error( ERR_DROP, "%s: invalid client gentity", __func__ );
				return;
			}
			if ( entity.r.svFlags & SVF_NOSERVERINFO ) {
				continue;
			}
		}

		len = static_cast<int>( strlen( val ) );
		if ( len >= maxChunkSize ) {
			int sent = 0;
			int remaining = len;
			char buf[MAX_STRING_CHARS];

			while ( remaining > 0 ) {
				const char *cmd = sent == 0 ? "bcs0" : ( remaining < maxChunkSize ? "bcs2" : "bcs1" );
				Q_strncpyz( buf, &val[sent], maxChunkSize );
				SV_SendServerCommand( &client, "%s %i \"%s\"\n", cmd, index, buf );

				sent += ( maxChunkSize - 1 );
				remaining -= ( maxChunkSize - 1 );
			}
		} else {
			SV_SendServerCommand( &client, "cs %i \"%s\"\n", index, val );
		}
	}
}

static void QDECL QL_G_trap_ResendConfigstring( int index ) {
	SV_GameResendConfigstring( index );
}

static void *QDECL QL_G_trap_MarkClientGotCP( int clientNum ) {
	if ( !SV_IsClientIndex( clientNum ) ) {
		return reinterpret_cast<void *>( static_cast<intptr_t>( clientNum ) );
	}

	client_t &client = SV_ClientForIndex( clientNum );
	client.gotCP = qtrue;
	return &client;
}

static intptr_t QDECL QL_G_trap_ReturnZero( void ) {
	return 0;
}

static qboolean SV_ParseSteamIdString( const char *text, unsigned int *steamIdLow, unsigned int *steamIdHigh ) {
	unsigned long long value = 0;

	if ( !text ) {
		return qfalse;
	}

	while ( *text == ' ' || *text == '\t' ) {
		++text;
	}

	if ( !*text ) {
		return qfalse;
	}

	while ( *text && *text != ' ' && *text != '\t' ) {
		if ( *text < '0' || *text > '9' ) {
			return qfalse;
		}

		const unsigned int digit = static_cast<unsigned int>( *text - '0' );
		const unsigned long long maxValue = ~0ULL;
		if ( value > ( maxValue - digit ) / 10ULL ) {
			return qfalse;
		}

		value = value * 10ULL + digit;
		++text;
	}

	while ( *text == ' ' || *text == '\t' ) {
		++text;
	}

	if ( *text || value == 0ULL ) {
		return qfalse;
	}

	if ( steamIdLow ) {
		*steamIdLow = static_cast<unsigned int>( value & 0xffffffffULL );
	}
	if ( steamIdHigh ) {
		*steamIdHigh = static_cast<unsigned int>( value >> 32 );
	}

	return qtrue;
}

static qboolean SV_ClientSteamIdFromUserinfo( int clientNum, unsigned int *steamIdLow, unsigned int *steamIdHigh ) {
	if ( !SV_IsClientIndex( clientNum ) ) {
		return qfalse;
	}

	const client_t &client = SV_ClientForIndex( clientNum );
	if ( SV_ParseSteamIdString( client.platformSteamId, steamIdLow, steamIdHigh ) ) {
		return qtrue;
	}

	if ( SV_ParseSteamIdString( Info_ValueForKey( client.userinfo, "steamid" ), steamIdLow, steamIdHigh ) ) {
		return qtrue;
	}

	return SV_ParseSteamIdString( Info_ValueForKey( client.userinfo, "steam" ), steamIdLow, steamIdHigh );
}

qboolean SV_ClientSteamId( int clientNum, unsigned int *steamIdLow, unsigned int *steamIdHigh ) {
	return SV_ClientSteamIdFromUserinfo( clientNum, steamIdLow, steamIdHigh );
}

static qboolean QDECL QL_G_trap_GetSteamId( int clientNum, unsigned int *steamIdLow, unsigned int *steamIdHigh ) {
	unsigned int low = 0;
	unsigned int high = 0;

	if ( steamIdLow ) {
		*steamIdLow = 0;
	}
	if ( steamIdHigh ) {
		*steamIdHigh = 0;
	}

	if ( !SV_ClientSteamId( clientNum, &low, &high ) ) {
		return qfalse;
	}

	if ( steamIdLow ) {
		*steamIdLow = low;
	}
	if ( steamIdHigh ) {
		*steamIdHigh = high;
	}
	return qtrue;
}

// Retail native import slot 200 returns only the low SteamID dword.  The
// older compatibility syscall uses the three-argument output-pointer shape
// above; sharing one function between those ABIs corrupts the caller stack.
static unsigned int QDECL QL_G_trap_GetSteamIdLow( int clientNum ) {
	unsigned int low = 0;
	unsigned int high = 0;
	return SV_ClientSteamId( clientNum, &low, &high ) ? low : 0u;
}

static qboolean QDECL QL_G_trap_VerifySteamAuth( int clientNum ) {
	return SV_VerifyClientSteamAuth( clientNum );
}

// Retail slot 205 is invoked from GAME_CLIENT_CONNECT before the server moves
// the provisional slot from CS_FREE to CS_CONNECTED.  It asks whether the
// provider session was successfully started; final asynchronous rejection is
// handled by the provider event callback and drops the client.
static qboolean QDECL QL_G_trap_BeginSteamAuthSession( int clientNum ) {
	if ( !svs.clients || clientNum < 0 || clientNum >= sv.maxclients ) {
		return qfalse;
	}
	if ( SV_PlatformServiceAvailable( SV_PLATFORM_CAPABILITY_AUTH ) ) {
		return svs.clients[clientNum].platformAuthSession;
	}
	return qtrue;
}

static void QDECL QL_G_trap_RankBegin( char *gameKey ) {
	SV_GameRankBegin( gameKey );
}

static void QDECL QL_G_trap_RankPoll( void ) {
	SV_GameRankPoll();
}

static qboolean QDECL QL_G_trap_RankCheckInit( void ) {
	return SV_GameRankCheckInit();
}

static qboolean QDECL QL_G_trap_RankActive( void ) {
	return SV_GameRankActive();
}

static int QDECL QL_G_trap_RankUserStatus( int index ) {
	return SV_GameRankUserStatus( index );
}

static void QDECL QL_G_trap_RankUserReset( int index ) {
	SV_GameRankUserReset( index );
}

static void QDECL QL_G_trap_RankReportInt( int index1, int index2, int key, int value, qboolean accum ) {
	SV_GameRankReportInt( index1, index2, key, value, accum );
}

static void QDECL QL_G_trap_RankReportStr( int index1, int index2, int key, char *value ) {
	SV_GameRankReportStr( index1, index2, key, value );
}

static void SV_SubmitMatchReport( void *report ) {
	(void)SV_SteamStats_ProcessMatchReport( report, nullptr, 0 );
}

static void SV_ReportPlayerEvent( unsigned int steamIdLow, unsigned int steamIdHigh,
		const void *clientStats, const char *eventName, void *payload ) {
	SV_SteamStats_ProcessEvent( steamIdLow, steamIdHigh, clientStats, eventName, payload );
}

static void QDECL QL_G_trap_SubmitMatchReport( void *report ) {
	SV_SubmitMatchReport( report );
}

static void QDECL QL_G_trap_ReportPlayerEvent( unsigned int steamIdLow, unsigned int steamIdHigh,
		const void *clientStats, const char *eventName, void *payload ) {
	SV_ReportPlayerEvent( steamIdLow, steamIdHigh, clientStats, eventName, payload );
}

static void QDECL QL_G_trap_AddSteamStat( int clientNum, int statIndex, int delta ) {
	SV_SteamStats_AddFieldValue( clientNum, statIndex, delta );
}

typedef struct {
	std::uint32_t	data1;
	std::uint16_t	data2;
	std::uint16_t	data3;
	byte		data4[8];
} ql_guid_t;

static_assert( sizeof( ql_guid_t ) == 16, "QL match GUID layout must match Win32 GUID" );

#if defined(_WIN32)
typedef HRESULT (WINAPI *ql_co_create_guid_t)( ql_guid_t *guid );
#endif

static qboolean SV_CreateSystemGuid( ql_guid_t *guid ) {
#if defined(_WIN32)
	HMODULE ole32;
	ql_co_create_guid_t createGuid;
	HRESULT result;

	if ( !guid ) {
		return qfalse;
	}

	ole32 = LoadLibraryA( "ole32.dll" );
	if ( !ole32 ) {
		return qfalse;
	}

	createGuid = reinterpret_cast<ql_co_create_guid_t>( GetProcAddress( ole32, "CoCreateGuid" ) );
	if ( !createGuid ) {
		FreeLibrary( ole32 );
		return qfalse;
	}

	result = createGuid( guid );
	FreeLibrary( ole32 );

	return result >= 0 ? qtrue : qfalse;
#else
	(void)guid;
	return qfalse;
#endif
}

static void SV_CreateFallbackGuid( ql_guid_t *guid ) {
	byte randomBytes[16];

	Com_RandomBytes( randomBytes, sizeof( randomBytes ) );
	randomBytes[6] = static_cast<byte>( ( randomBytes[6] & 0x0f ) | 0x40 );
	randomBytes[8] = static_cast<byte>( ( randomBytes[8] & 0x3f ) | 0x80 );

	guid->data1 = ( static_cast<std::uint32_t>( randomBytes[0] ) << 24 )
		| ( static_cast<std::uint32_t>( randomBytes[1] ) << 16 )
		| ( static_cast<std::uint32_t>( randomBytes[2] ) << 8 )
		| static_cast<std::uint32_t>( randomBytes[3] );
	guid->data2 = static_cast<std::uint16_t>( ( randomBytes[4] << 8 ) | randomBytes[5] );
	guid->data3 = static_cast<std::uint16_t>( ( randomBytes[6] << 8 ) | randomBytes[7] );
	for ( int i = 0; i < 8; ++i ) {
		guid->data4[i] = randomBytes[i + 8];
	}
}

static void SV_FormatMatchGuid( const ql_guid_t *guid, char *buffer, int bufferSize ) {
	if ( !buffer || bufferSize <= 0 ) {
		return;
	}

	Com_sprintf(
		buffer,
		bufferSize,
		"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		static_cast<unsigned int>( guid->data1 ),
		static_cast<unsigned int>( guid->data2 ),
		static_cast<unsigned int>( guid->data3 ),
		static_cast<unsigned int>( guid->data4[0] ),
		static_cast<unsigned int>( guid->data4[1] ),
		static_cast<unsigned int>( guid->data4[2] ),
		static_cast<unsigned int>( guid->data4[3] ),
		static_cast<unsigned int>( guid->data4[4] ),
		static_cast<unsigned int>( guid->data4[5] ),
		static_cast<unsigned int>( guid->data4[6] ),
		static_cast<unsigned int>( guid->data4[7] )
	);
}

static void SV_GenerateMatchGuid( char *buffer, int bufferSize ) {
	ql_guid_t guid;

	if ( !buffer || bufferSize <= 0 ) {
		return;
	}

	if ( !SV_CreateSystemGuid( &guid ) ) {
		SV_CreateFallbackGuid( &guid );
	}

	SV_FormatMatchGuid( &guid, buffer, bufferSize );
}

static void QDECL QL_G_trap_GenerateMatchGuid( char *buffer, int bufferSize ) {
	if ( bufferSize <= 0 || bufferSize > GAME_MATCH_GUID_BUFFER_SIZE ) {
		bufferSize = GAME_MATCH_GUID_BUFFER_SIZE;
	}

	SV_GenerateMatchGuid( buffer, bufferSize );
}

static void QDECL QL_G_trap_UnlockSteamAchievement( int clientNum, int achievementId ) {
	SV_SteamStats_UnlockAchievement( clientNum, achievementId );
}

static qboolean QDECL QL_G_trap_HasSteamAchievement( int clientNum, int achievementId ) {
	return SV_SteamStats_HasAchievement( clientNum, achievementId );
}

static qboolean QDECL QL_G_trap_FactoryExists( const char *factoryName ) {
	return SV_FactoryExists( factoryName );
}

static void QDECL QL_G_trap_BotDrawDebugAreas( vec3_t origin, int enable, int areanum ) {
	botlib_export->ai.BotDrawDebugAreas( origin, enable, areanum );
}

static void QDECL QL_G_trap_BotDrawAvoidSpots( int movestate ) {
	botlib_export->ai.BotDrawAvoidSpots( movestate );
}

static void *QDECL QL_G_trap_Memset( void *dest, int c, int count ) {
	return Com_Memset( dest, c, count );
}

static void *QDECL QL_G_trap_Memcpy( void *dest, const void *src, int count ) {
	return Com_Memcpy( dest, src, count );
}

static char *QDECL QL_G_trap_Strncpy( char *dest, const char *src, int count ) {
	return Q_strncpy( dest, const_cast<char *>( src ), count );
}

static float QDECL QL_G_trap_Sin( float x ) {
	return sin( x );
}

static float QDECL QL_G_trap_Cos( float x ) {
	return cos( x );
}

static float QDECL QL_G_trap_Atan2( float y, float x ) {
	return atan2( y, x );
}

static float QDECL QL_G_trap_Sqrt( float x ) {
	return sqrt( x );
}

static float QDECL QL_G_trap_Floor( float x ) {
	return floor( x );
}

static float QDECL QL_G_trap_Ceil( float x ) {
	return ceil( x );
}

static void QDECL QL_G_trap_MatrixMultiply( float in1[3][3], float in2[3][3], float out[3][3] ) {
	MatrixMultiply( in1, in2, out );
}

static void QDECL QL_G_trap_AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up ) {
	AngleVectors( angles, forward, right, up );
}

static void QDECL QL_G_trap_PerpendicularVector( vec3_t dst, const vec3_t src ) {
	PerpendicularVector( dst, src );
}

static void QDECL QL_G_trap_TestPrintInt( int value ) {
	(void)value;
}

static void QDECL QL_G_trap_TestPrintFloat( float value ) {
	(void)value;
}

static void SV_InitGameImports( void ) {
#define QL_BIND( slot, fn ) ql_game_imports[slot] = (ql_import_f)fn
#define QL_BIND_COMPAT( slot, fn ) ql_game_imports[G_QL_IMPORT_COMPAT_BASE + slot] = (ql_import_f)fn
	Com_Memset( ql_game_imports, 0, sizeof( ql_game_imports ) );

	QL_BIND( G_QL_IMPORT_SEND_CONSOLE_COMMAND, QL_G_trap_SendConsoleCommandText );
	QL_BIND( G_QL_IMPORT_REAL_TIME, QL_G_trap_RealTime );
	QL_BIND( G_QL_IMPORT_PRINT, QL_G_trap_Printf );
	QL_BIND( G_QL_IMPORT_MILLISECONDS, QL_G_trap_Milliseconds );
	QL_BIND( G_QL_IMPORT_FS_WRITE, QL_G_trap_FS_Write );
	QL_BIND( G_QL_IMPORT_FS_SEEK, QL_G_trap_FS_Seek );
	QL_BIND( G_QL_IMPORT_FS_READ, QL_G_trap_FS_Read );
	QL_BIND( G_QL_IMPORT_FS_GETFILELIST, QL_G_trap_FS_GetFileList );
	QL_BIND( G_QL_IMPORT_FS_FOPEN_FILE, QL_G_trap_FS_FOpenFile );
	QL_BIND( G_QL_IMPORT_FS_FCLOSE_FILE, QL_G_trap_FS_FCloseFile );
	QL_BIND( G_QL_IMPORT_ERROR, QL_G_trap_Error );
	QL_BIND( G_QL_IMPORT_CVAR_VARIABLE_VALUE, QL_G_trap_Cvar_VariableValue );
	QL_BIND( G_QL_IMPORT_CVAR_UPDATE, QL_G_trap_Cvar_Update );
	QL_BIND( G_QL_IMPORT_CVAR_VARIABLE_STRING_BUFFER, QL_G_trap_Cvar_VariableStringBuffer );
	QL_BIND( G_QL_IMPORT_CVAR_SET_VALUE, QL_G_trap_Cvar_SetValue );
	QL_BIND( G_QL_IMPORT_CVAR_SET, QL_G_trap_Cvar_Set );
	QL_BIND( G_QL_IMPORT_CVAR_REGISTER_BOUNDED, QL_G_trap_Cvar_RegisterBounded );
	QL_BIND( G_QL_IMPORT_CVAR_REGISTER, QL_G_trap_Cvar_Register );
	QL_BIND( G_QL_IMPORT_ARGV, QL_G_trap_Argv );
	QL_BIND( G_QL_IMPORT_ARGS, QL_G_trap_Args );
	QL_BIND( G_QL_IMPORT_ARGC, QL_G_trap_Argc );
	QL_BIND( G_QL_IMPORT_CMD_TOKENIZE_STRING, QL_G_trap_Cmd_TokenizeString );
	QL_BIND( G_QL_IMPORT_LOCATE_GAME_DATA, QL_G_trap_LocateGameData );
	QL_BIND( G_QL_IMPORT_DROP_CLIENT, QL_G_trap_DropClient );
	QL_BIND( G_QL_IMPORT_SEND_SERVER_COMMAND, QL_G_trap_SendServerCommand );
	QL_BIND( G_QL_IMPORT_SET_CONFIGSTRING, QL_G_trap_SetConfigstring );
	QL_BIND( G_QL_IMPORT_GET_CONFIGSTRING, QL_G_trap_GetConfigstring );
	QL_BIND( G_QL_IMPORT_RESEND_CONFIGSTRING, QL_G_trap_ResendConfigstring );
	QL_BIND( G_QL_IMPORT_GET_USERINFO, QL_G_trap_GetUserinfo );
	QL_BIND( G_QL_IMPORT_SET_USERINFO, QL_G_trap_SetUserinfo );
	QL_BIND( G_QL_IMPORT_GET_SERVERINFO, QL_G_trap_GetServerinfo );
	QL_BIND( G_QL_IMPORT_SET_BRUSH_MODEL, QL_G_trap_SetBrushModel );
	QL_BIND( G_QL_IMPORT_TRACE, QL_G_trap_Trace );
	QL_BIND( G_QL_IMPORT_TRACECAPSULE, QL_G_trap_TraceCapsule );
	QL_BIND( G_QL_IMPORT_POINT_CONTENTS, QL_G_trap_PointContents );
	QL_BIND( G_QL_IMPORT_IN_PVS, QL_G_trap_InPVS );
	QL_BIND( G_QL_IMPORT_IN_PVS_IGNORE_PORTALS, QL_G_trap_InPVSIgnorePortals );
	QL_BIND( G_QL_IMPORT_ADJUST_AREA_PORTAL_STATE, QL_G_trap_AdjustAreaPortalState );
	QL_BIND( G_QL_IMPORT_AREAS_CONNECTED, QL_G_trap_AreasConnected );
	QL_BIND( G_QL_IMPORT_LINKENTITY, QL_G_trap_LinkEntity );
	QL_BIND( G_QL_IMPORT_UNLINK_ENTITY, QL_G_trap_UnlinkEntity );
	QL_BIND( G_QL_IMPORT_ENTITIES_IN_BOX, QL_G_trap_EntitiesInBox );
	QL_BIND( G_QL_IMPORT_ENTITY_CONTACT, QL_G_trap_EntityContact );
	QL_BIND( G_QL_IMPORT_BOT_ALLOCATE_CLIENT, QL_G_trap_BotAllocateClient );
	QL_BIND( G_QL_IMPORT_BOT_FREE_CLIENT, QL_G_trap_BotFreeClient );
	QL_BIND( G_QL_IMPORT_GET_USERCMD, QL_G_trap_GetUsercmd );
	QL_BIND( G_QL_IMPORT_GET_ENTITY_TOKEN, QL_G_trap_GetEntityToken );
	QL_BIND( G_QL_IMPORT_DEBUG_POLYGON_CREATE, QL_G_trap_DebugPolygonCreate );
	QL_BIND( G_QL_IMPORT_DEBUG_POLYGON_DELETE, QL_G_trap_DebugPolygonDelete );
	QL_BIND( G_QL_IMPORT_BOTLIB_SETUP, QL_G_trap_BotLibSetup );
	QL_BIND( G_QL_IMPORT_BOTLIB_SHUTDOWN, QL_G_trap_BotLibShutdown );
	QL_BIND( G_QL_IMPORT_BOTLIB_LIBVAR_SET, QL_G_trap_BotLibVarSet );
	QL_BIND( G_QL_IMPORT_BOTLIB_LIBVAR_GET, QL_G_trap_BotLibVarGet );
	QL_BIND( G_QL_IMPORT_BOTLIB_PC_ADD_GLOBAL_DEFINE, QL_G_trap_BotLibDefine );
	QL_BIND( G_QL_IMPORT_BOTLIB_START_FRAME, QL_G_trap_BotLibStartFrame );
	QL_BIND( G_QL_IMPORT_BOTLIB_LOAD_MAP, QL_G_trap_BotLibLoadMap );
	QL_BIND( G_QL_IMPORT_BOTLIB_UPDATE_ENTITY, QL_G_trap_BotLibUpdateEntity );
	QL_BIND( G_QL_IMPORT_BOTLIB_TEST, QL_G_trap_BotLibTest );
	QL_BIND( G_QL_IMPORT_BOTLIB_GET_SNAPSHOT_ENTITY, QL_G_trap_BotGetSnapshotEntity );
	QL_BIND( G_QL_IMPORT_BOTLIB_GET_CONSOLE_MESSAGE, QL_G_trap_BotGetServerCommand );
	QL_BIND( G_QL_IMPORT_BOTLIB_USER_COMMAND, QL_G_trap_BotUserCommand );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_BBOX_AREAS, QL_G_trap_AAS_BBoxAreas );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_AREA_INFO, QL_G_trap_AAS_AreaInfo );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_ENTITY_INFO, QL_G_trap_AAS_EntityInfo );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_INITIALIZED, QL_G_trap_AAS_Initialized );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX, QL_G_trap_AAS_PresenceTypeBoundingBox );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_TIME, QL_G_trap_AAS_Time );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_POINT_AREA_NUM, QL_G_trap_AAS_PointAreaNum );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX, QL_G_trap_AAS_PointReachabilityAreaIndex );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_TRACE_AREAS, QL_G_trap_AAS_TraceAreas );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_POINT_CONTENTS, QL_G_trap_AAS_PointContents );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_NEXT_BSP_ENTITY, QL_G_trap_AAS_NextBSPEntity );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_ValueForBSPEpairKey );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_VectorForBSPEpairKey );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_FloatForBSPEpairKey );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_IntForBSPEpairKey );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_AREA_REACHABILITY, QL_G_trap_AAS_AreaReachability );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA, QL_G_trap_AAS_AreaTravelTimeToGoalArea );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_ENABLE_ROUTING_AREA, QL_G_trap_AAS_EnableRoutingArea );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_PREDICT_ROUTE, QL_G_trap_AAS_PredictRoute );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL, QL_G_trap_AAS_AlternativeRouteGoals );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_SWIMMING, QL_G_trap_AAS_Swimming );
	QL_BIND( G_QL_IMPORT_BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT, QL_G_trap_AAS_PredictClientMovement );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_DRAW_DEBUG_AREAS, QL_G_trap_BotDrawDebugAreas );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_DRAW_AVOID_SPOTS, QL_G_trap_BotDrawAvoidSpots );

	QL_BIND( G_QL_IMPORT_BOTLIB_EA_SAY, QL_G_trap_EA_Say );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_SAY_TEAM, QL_G_trap_EA_SayTeam );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_COMMAND, QL_G_trap_EA_Command );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_ACTION, QL_G_trap_EA_Action );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_WALK, QL_G_trap_EA_Walk );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_GESTURE, QL_G_trap_EA_Gesture );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_TALK, QL_G_trap_EA_Talk );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_ATTACK, QL_G_trap_EA_Attack );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_USE, QL_G_trap_EA_Use );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_RESPAWN, QL_G_trap_EA_Respawn );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_CROUCH, QL_G_trap_EA_Crouch );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_MOVE_UP, QL_G_trap_EA_MoveUp );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_MOVE_DOWN, QL_G_trap_EA_MoveDown );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_MOVE_FORWARD, QL_G_trap_EA_MoveForward );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_MOVE_BACK, QL_G_trap_EA_MoveBack );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_MOVE_LEFT, QL_G_trap_EA_MoveLeft );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_MOVE_RIGHT, QL_G_trap_EA_MoveRight );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_SELECT_WEAPON, QL_G_trap_EA_SelectWeapon );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_JUMP, QL_G_trap_EA_Jump );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_DELAYED_JUMP, QL_G_trap_EA_DelayedJump );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_MOVE, QL_G_trap_EA_Move );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_VIEW, QL_G_trap_EA_View );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_END_REGULAR, QL_G_trap_EA_EndRegular );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_GET_INPUT, QL_G_trap_EA_GetInput );
	QL_BIND( G_QL_IMPORT_BOTLIB_EA_RESET_INPUT, QL_G_trap_EA_ResetInput );

	QL_BIND( G_QL_IMPORT_BOTLIB_AI_LOAD_CHARACTER, QL_G_trap_BotLoadCharacter );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_FREE_CHARACTER, QL_G_trap_BotFreeCharacter );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHARACTERISTIC_FLOAT, QL_G_trap_Characteristic_Float );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHARACTERISTIC_BFLOAT, QL_G_trap_Characteristic_BFloat );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHARACTERISTIC_INTEGER, QL_G_trap_Characteristic_Integer );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHARACTERISTIC_BINTEGER, QL_G_trap_Characteristic_BInteger );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHARACTERISTIC_STRING, QL_G_trap_Characteristic_String );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_ALLOC_CHAT_STATE, QL_G_trap_BotAllocChatState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_FREE_CHAT_STATE, QL_G_trap_BotFreeChatState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, QL_G_trap_BotQueueConsoleMessage );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_REMOVE_CONSOLE_MESSAGE, QL_G_trap_BotRemoveConsoleMessage );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_NEXT_CONSOLE_MESSAGE, QL_G_trap_BotNextConsoleMessage );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_NUM_CONSOLE_MESSAGE, QL_G_trap_BotNumConsoleMessages );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_INITIAL_CHAT, QL_G_trap_BotInitialChat );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_NUM_INITIAL_CHATS, QL_G_trap_BotNumInitialChats );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_REPLY_CHAT, QL_G_trap_BotReplyChat );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHAT_LENGTH, QL_G_trap_BotChatLength );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_ENTER_CHAT, QL_G_trap_BotEnterChat );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GET_CHAT_MESSAGE, QL_G_trap_BotGetChatMessage );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_STRING_CONTAINS, QL_G_trap_StringContains );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_FIND_MATCH, QL_G_trap_BotFindMatch );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_MATCH_VARIABLE, QL_G_trap_BotMatchVariable );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_UNIFY_WHITE_SPACES, QL_G_trap_UnifyWhiteSpaces );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_REPLACE_SYNONYMS, QL_G_trap_BotReplaceSynonyms );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_LOAD_CHAT_FILE, QL_G_trap_BotLoadChatFile );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_SET_CHAT_GENDER, QL_G_trap_BotSetChatGender );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_SET_CHAT_NAME, QL_G_trap_BotSetChatName );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_RESET_GOAL_STATE, QL_G_trap_BotResetGoalState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_REMOVE_FROM_AVOID_GOALS, QL_G_trap_BotRemoveFromAvoidGoals );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_RESET_AVOID_GOALS, QL_G_trap_BotResetAvoidGoals );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_PUSH_GOAL, QL_G_trap_BotPushGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_POP_GOAL, QL_G_trap_BotPopGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_EMPTY_GOAL_STACK, QL_G_trap_BotEmptyGoalStack );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_DUMP_AVOID_GOALS, QL_G_trap_BotDumpAvoidGoals );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_DUMP_GOAL_STACK, QL_G_trap_BotDumpGoalStack );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GOAL_NAME, QL_G_trap_BotGoalName );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GET_TOP_GOAL, QL_G_trap_BotGetTopGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GET_SECOND_GOAL, QL_G_trap_BotGetSecondGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHOOSE_LTG_ITEM, QL_G_trap_BotChooseLTGItem );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHOOSE_NBG_ITEM, QL_G_trap_BotChooseNBGItem );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_TOUCHING_GOAL, QL_G_trap_BotTouchingGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, QL_G_trap_BotItemGoalInVisButNotVisible );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, QL_G_trap_BotGetNextCampSpotGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GET_MAP_LOCATION_GOAL, QL_G_trap_BotGetMapLocationGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GET_LEVEL_ITEM_GOAL, QL_G_trap_BotGetLevelItemGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_AVOID_GOAL_TIME, QL_G_trap_BotAvoidGoalTime );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_SET_AVOID_GOAL_TIME, QL_G_trap_BotSetAvoidGoalTime );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_INIT_LEVEL_ITEMS, QL_G_trap_BotInitLevelItems );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_UPDATE_ENTITY_ITEMS, QL_G_trap_BotUpdateEntityItems );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_LOAD_ITEM_WEIGHTS, QL_G_trap_BotLoadItemWeights );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_FREE_ITEM_WEIGHTS, QL_G_trap_BotFreeItemWeights );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC, QL_G_trap_BotInterbreedGoalFuzzyLogic );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, QL_G_trap_BotSaveGoalFuzzyLogic );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, QL_G_trap_BotMutateGoalFuzzyLogic );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_ALLOC_GOAL_STATE, QL_G_trap_BotAllocGoalState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_FREE_GOAL_STATE, QL_G_trap_BotFreeGoalState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_RESET_MOVE_STATE, QL_G_trap_BotResetMoveState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_MOVE_TO_GOAL, QL_G_trap_BotMoveToGoal );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_MOVE_IN_DIRECTION, QL_G_trap_BotMoveInDirection );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_RESET_AVOID_REACH, QL_G_trap_BotResetAvoidReach );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_RESET_LAST_AVOID_REACH, QL_G_trap_BotResetLastAvoidReach );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_REACHABILITY_AREA, QL_G_trap_BotReachabilityArea );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_MOVEMENT_VIEW_TARGET, QL_G_trap_BotMovementViewTarget );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_PREDICT_VISIBLE_POSITION, QL_G_trap_BotPredictVisiblePosition );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_ALLOC_MOVE_STATE, QL_G_trap_BotAllocMoveState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_FREE_MOVE_STATE, QL_G_trap_BotFreeMoveState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_INIT_MOVE_STATE, QL_G_trap_BotInitMoveState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_ADD_AVOID_SPOT, QL_G_trap_BotAddAvoidSpot );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON, QL_G_trap_BotChooseBestFightWeapon );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GET_WEAPON_INFO, QL_G_trap_BotGetWeaponInfo );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_LOAD_WEAPON_WEIGHTS, QL_G_trap_BotLoadWeaponWeights );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_ALLOC_WEAPON_STATE, QL_G_trap_BotAllocWeaponState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_FREE_WEAPON_STATE, QL_G_trap_BotFreeWeaponState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_RESET_WEAPON_STATE, QL_G_trap_BotResetWeaponState );
	QL_BIND( G_QL_IMPORT_BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, QL_G_trap_GeneticParentsAndChildSelection );
	QL_BIND( G_QL_IMPORT_SUBMIT_MATCH_REPORT, QL_G_trap_SubmitMatchReport );
	QL_BIND( G_QL_IMPORT_REPORT_PLAYER_EVENT, QL_G_trap_ReportPlayerEvent );
	QL_BIND( G_QL_IMPORT_MARK_CLIENT_GOT_CP, QL_G_trap_MarkClientGotCP );
	QL_BIND( G_QL_IMPORT_RETURN_ZERO_198, QL_G_trap_ReturnZero );
	QL_BIND( G_QL_IMPORT_RETURN_ZERO_199, QL_G_trap_ReturnZero );
	QL_BIND( G_QL_IMPORT_STEAMID_QUERY, QL_G_trap_GetSteamIdLow );
	QL_BIND( G_QL_IMPORT_STEAM_STAT_ADD, QL_G_trap_AddSteamStat );
	QL_BIND( G_QL_IMPORT_GENERATE_MATCH_GUID, QL_G_trap_GenerateMatchGuid );
	QL_BIND( G_QL_IMPORT_STEAM_UNLOCK_ACHIEVEMENT, QL_G_trap_UnlockSteamAchievement );
	QL_BIND( G_QL_IMPORT_STEAM_HAS_ACHIEVEMENT, QL_G_trap_HasSteamAchievement );
	QL_BIND( G_QL_IMPORT_STEAM_AUTH_VALIDATE, QL_G_trap_BeginSteamAuthSession );
	QL_BIND( G_QL_IMPORT_FACTORY_EXISTS, QL_G_trap_FactoryExists );

	QL_BIND_COMPAT( G_PRINT, QL_G_trap_Printf );
	QL_BIND_COMPAT( G_ERROR, QL_G_trap_Error );
	QL_BIND_COMPAT( G_MILLISECONDS, QL_G_trap_Milliseconds );
	QL_BIND_COMPAT( G_CVAR_REGISTER, QL_G_trap_Cvar_Register );
	QL_BIND_COMPAT( G_CVAR_UPDATE, QL_G_trap_Cvar_Update );
	QL_BIND_COMPAT( G_CVAR_SET, QL_G_trap_Cvar_Set );
	QL_BIND_COMPAT( G_CVAR_VARIABLE_INTEGER_VALUE, QL_G_trap_Cvar_VariableIntegerValue );
	QL_BIND_COMPAT( G_CVAR_VARIABLE_STRING_BUFFER, QL_G_trap_Cvar_VariableStringBuffer );
	QL_BIND_COMPAT( G_ARGC, QL_G_trap_Argc );
	QL_BIND_COMPAT( G_ARGV, QL_G_trap_Argv );
	QL_BIND_COMPAT( G_FS_FOPEN_FILE, QL_G_trap_FS_FOpenFile );
	QL_BIND_COMPAT( G_FS_READ, QL_G_trap_FS_Read );
	QL_BIND_COMPAT( G_FS_WRITE, QL_G_trap_FS_Write );
	QL_BIND_COMPAT( G_FS_FCLOSE_FILE, QL_G_trap_FS_FCloseFile );
	QL_BIND_COMPAT( G_SEND_CONSOLE_COMMAND, QL_G_trap_SendConsoleCommand );
	QL_BIND_COMPAT( G_LOCATE_GAME_DATA, QL_G_trap_LocateGameData );
	QL_BIND_COMPAT( G_DROP_CLIENT, QL_G_trap_DropClient );
	QL_BIND_COMPAT( G_SEND_SERVER_COMMAND, QL_G_trap_SendServerCommand );
	QL_BIND_COMPAT( G_SET_CONFIGSTRING, QL_G_trap_SetConfigstring );
	QL_BIND_COMPAT( G_GET_CONFIGSTRING, QL_G_trap_GetConfigstring );
	QL_BIND_COMPAT( G_GET_USERINFO, QL_G_trap_GetUserinfo );
	QL_BIND_COMPAT( G_SET_USERINFO, QL_G_trap_SetUserinfo );
	QL_BIND_COMPAT( G_GET_SERVERINFO, QL_G_trap_GetServerinfo );
	QL_BIND_COMPAT( G_STEAMID_QUERY, QL_G_trap_GetSteamId );
	QL_BIND_COMPAT( G_STEAM_AUTH_VALIDATE, QL_G_trap_VerifySteamAuth );
	QL_BIND_COMPAT( G_RANK_BEGIN, QL_G_trap_RankBegin );
	QL_BIND_COMPAT( G_RANK_POLL, QL_G_trap_RankPoll );
	QL_BIND_COMPAT( G_RANK_CHECK_INIT, QL_G_trap_RankCheckInit );
	QL_BIND_COMPAT( G_RANK_ACTIVE, QL_G_trap_RankActive );
	QL_BIND_COMPAT( G_RANK_USER_STATUS, QL_G_trap_RankUserStatus );
	QL_BIND_COMPAT( G_RANK_USER_RESET, QL_G_trap_RankUserReset );
	QL_BIND_COMPAT( G_RANK_REPORT_INT, QL_G_trap_RankReportInt );
	QL_BIND_COMPAT( G_RANK_REPORT_STR, QL_G_trap_RankReportStr );
	QL_BIND_COMPAT( G_SET_BRUSH_MODEL, QL_G_trap_SetBrushModel );
	QL_BIND_COMPAT( G_TRACE, QL_G_trap_Trace );
	QL_BIND_COMPAT( G_POINT_CONTENTS, QL_G_trap_PointContents );
	QL_BIND_COMPAT( G_IN_PVS, QL_G_trap_InPVS );
	QL_BIND_COMPAT( G_IN_PVS_IGNORE_PORTALS, QL_G_trap_InPVSIgnorePortals );
	QL_BIND_COMPAT( G_ADJUST_AREA_PORTAL_STATE, QL_G_trap_AdjustAreaPortalState );
	QL_BIND_COMPAT( G_AREAS_CONNECTED, QL_G_trap_AreasConnected );
	QL_BIND_COMPAT( G_LINKENTITY, QL_G_trap_LinkEntity );
	QL_BIND_COMPAT( G_UNLINKENTITY, QL_G_trap_UnlinkEntity );
	QL_BIND_COMPAT( G_ENTITIES_IN_BOX, QL_G_trap_EntitiesInBox );
	QL_BIND_COMPAT( G_ENTITY_CONTACT, QL_G_trap_EntityContact );
	QL_BIND_COMPAT( G_BOT_ALLOCATE_CLIENT, QL_G_trap_BotAllocateClient );
	QL_BIND_COMPAT( G_BOT_FREE_CLIENT, QL_G_trap_BotFreeClient );
	QL_BIND_COMPAT( G_GET_USERCMD, QL_G_trap_GetUsercmd );
	QL_BIND_COMPAT( G_GET_ENTITY_TOKEN, QL_G_trap_GetEntityToken );
	QL_BIND_COMPAT( G_FS_GETFILELIST, QL_G_trap_FS_GetFileList );
	QL_BIND_COMPAT( G_DEBUG_POLYGON_CREATE, QL_G_trap_DebugPolygonCreate );
	QL_BIND_COMPAT( G_DEBUG_POLYGON_DELETE, QL_G_trap_DebugPolygonDelete );
	QL_BIND_COMPAT( G_REAL_TIME, QL_G_trap_RealTime );
	QL_BIND_COMPAT( G_SNAPVECTOR, QL_G_trap_SnapVector );
	QL_BIND_COMPAT( G_TRACECAPSULE, QL_G_trap_TraceCapsule );
	QL_BIND_COMPAT( G_ENTITY_CONTACTCAPSULE, QL_G_trap_EntityContactCapsule );
	QL_BIND_COMPAT( G_FS_SEEK, QL_G_trap_FS_Seek );
	QL_BIND_COMPAT( TRAP_MATRIXMULTIPLY, QL_G_trap_MatrixMultiply );
	QL_BIND_COMPAT( TRAP_ANGLEVECTORS, QL_G_trap_AngleVectors );
	QL_BIND_COMPAT( TRAP_PERPENDICULARVECTOR, QL_G_trap_PerpendicularVector );
	QL_BIND_COMPAT( TRAP_FLOOR, QL_G_trap_Floor );
	QL_BIND_COMPAT( TRAP_CEIL, QL_G_trap_Ceil );
	QL_BIND_COMPAT( TRAP_TESTPRINTINT, QL_G_trap_TestPrintInt );
	QL_BIND_COMPAT( TRAP_TESTPRINTFLOAT, QL_G_trap_TestPrintFloat );
	QL_BIND_COMPAT( BOTLIB_SETUP, QL_G_trap_BotLibSetup );
	QL_BIND_COMPAT( BOTLIB_SHUTDOWN, QL_G_trap_BotLibShutdown );
	QL_BIND_COMPAT( BOTLIB_LIBVAR_SET, QL_G_trap_BotLibVarSet );
	QL_BIND_COMPAT( BOTLIB_LIBVAR_GET, QL_G_trap_BotLibVarGet );
	QL_BIND_COMPAT( BOTLIB_PC_ADD_GLOBAL_DEFINE, QL_G_trap_BotLibDefine );
	QL_BIND_COMPAT( BOTLIB_START_FRAME, QL_G_trap_BotLibStartFrame );
	QL_BIND_COMPAT( BOTLIB_LOAD_MAP, QL_G_trap_BotLibLoadMap );
	QL_BIND_COMPAT( BOTLIB_UPDATENTITY, QL_G_trap_BotLibUpdateEntity );
	QL_BIND_COMPAT( BOTLIB_TEST, QL_G_trap_BotLibTest );
	QL_BIND_COMPAT( BOTLIB_GET_SNAPSHOT_ENTITY, QL_G_trap_BotGetSnapshotEntity );
	QL_BIND_COMPAT( BOTLIB_GET_CONSOLE_MESSAGE, QL_G_trap_BotGetServerCommand );
	QL_BIND_COMPAT( BOTLIB_USER_COMMAND, QL_G_trap_BotUserCommand );
	QL_BIND_COMPAT( BOTLIB_AAS_ENABLE_ROUTING_AREA, QL_G_trap_AAS_EnableRoutingArea );
	QL_BIND_COMPAT( BOTLIB_AAS_BBOX_AREAS, QL_G_trap_AAS_BBoxAreas );
	QL_BIND_COMPAT( BOTLIB_AAS_AREA_INFO, QL_G_trap_AAS_AreaInfo );
	QL_BIND_COMPAT( BOTLIB_AAS_ENTITY_INFO, QL_G_trap_AAS_EntityInfo );
	QL_BIND_COMPAT( BOTLIB_AAS_INITIALIZED, QL_G_trap_AAS_Initialized );
	QL_BIND_COMPAT( BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX, QL_G_trap_AAS_PresenceTypeBoundingBox );
	QL_BIND_COMPAT( BOTLIB_AAS_TIME, QL_G_trap_AAS_Time );
	QL_BIND_COMPAT( BOTLIB_AAS_POINT_AREA_NUM, QL_G_trap_AAS_PointAreaNum );
	QL_BIND_COMPAT( BOTLIB_AAS_TRACE_AREAS, QL_G_trap_AAS_TraceAreas );
	QL_BIND_COMPAT( BOTLIB_AAS_POINT_CONTENTS, QL_G_trap_AAS_PointContents );
	QL_BIND_COMPAT( BOTLIB_AAS_NEXT_BSP_ENTITY, QL_G_trap_AAS_NextBSPEntity );
	QL_BIND_COMPAT( BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_ValueForBSPEpairKey );
	QL_BIND_COMPAT( BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_VectorForBSPEpairKey );
	QL_BIND_COMPAT( BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_FloatForBSPEpairKey );
	QL_BIND_COMPAT( BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY, QL_G_trap_AAS_IntForBSPEpairKey );
	QL_BIND_COMPAT( BOTLIB_AAS_AREA_REACHABILITY, QL_G_trap_AAS_AreaReachability );
	QL_BIND_COMPAT( BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA, QL_G_trap_AAS_AreaTravelTimeToGoalArea );
	QL_BIND_COMPAT( BOTLIB_AAS_SWIMMING, QL_G_trap_AAS_Swimming );
	QL_BIND_COMPAT( BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT, QL_G_trap_AAS_PredictClientMovement );
	QL_BIND_COMPAT( BOTLIB_EA_SAY, QL_G_trap_EA_Say );
	QL_BIND_COMPAT( BOTLIB_EA_SAY_TEAM, QL_G_trap_EA_SayTeam );
	QL_BIND_COMPAT( BOTLIB_EA_COMMAND, QL_G_trap_EA_Command );
	QL_BIND_COMPAT( BOTLIB_EA_ACTION, QL_G_trap_EA_Action );
	QL_BIND_COMPAT( BOTLIB_EA_GESTURE, QL_G_trap_EA_Gesture );
	QL_BIND_COMPAT( BOTLIB_EA_TALK, QL_G_trap_EA_Talk );
	QL_BIND_COMPAT( BOTLIB_EA_ATTACK, QL_G_trap_EA_Attack );
	QL_BIND_COMPAT( BOTLIB_EA_USE, QL_G_trap_EA_Use );
	QL_BIND_COMPAT( BOTLIB_EA_RESPAWN, QL_G_trap_EA_Respawn );
	QL_BIND_COMPAT( BOTLIB_EA_CROUCH, QL_G_trap_EA_Crouch );
	QL_BIND_COMPAT( BOTLIB_EA_MOVE_UP, QL_G_trap_EA_MoveUp );
	QL_BIND_COMPAT( BOTLIB_EA_MOVE_DOWN, QL_G_trap_EA_MoveDown );
	QL_BIND_COMPAT( BOTLIB_EA_MOVE_FORWARD, QL_G_trap_EA_MoveForward );
	QL_BIND_COMPAT( BOTLIB_EA_MOVE_BACK, QL_G_trap_EA_MoveBack );
	QL_BIND_COMPAT( BOTLIB_EA_MOVE_LEFT, QL_G_trap_EA_MoveLeft );
	QL_BIND_COMPAT( BOTLIB_EA_MOVE_RIGHT, QL_G_trap_EA_MoveRight );
	QL_BIND_COMPAT( BOTLIB_EA_SELECT_WEAPON, QL_G_trap_EA_SelectWeapon );
	QL_BIND_COMPAT( BOTLIB_EA_JUMP, QL_G_trap_EA_Jump );
	QL_BIND_COMPAT( BOTLIB_EA_DELAYED_JUMP, QL_G_trap_EA_DelayedJump );
	QL_BIND_COMPAT( BOTLIB_EA_MOVE, QL_G_trap_EA_Move );
	QL_BIND_COMPAT( BOTLIB_EA_VIEW, QL_G_trap_EA_View );
	QL_BIND_COMPAT( BOTLIB_EA_END_REGULAR, QL_G_trap_EA_EndRegular );
	QL_BIND_COMPAT( BOTLIB_EA_GET_INPUT, QL_G_trap_EA_GetInput );
	QL_BIND_COMPAT( BOTLIB_EA_RESET_INPUT, QL_G_trap_EA_ResetInput );
	QL_BIND_COMPAT( BOTLIB_AI_LOAD_CHARACTER, QL_G_trap_BotLoadCharacter );
	QL_BIND_COMPAT( BOTLIB_AI_FREE_CHARACTER, QL_G_trap_BotFreeCharacter );
	QL_BIND_COMPAT( BOTLIB_AI_CHARACTERISTIC_FLOAT, QL_G_trap_Characteristic_Float );
	QL_BIND_COMPAT( BOTLIB_AI_CHARACTERISTIC_BFLOAT, QL_G_trap_Characteristic_BFloat );
	QL_BIND_COMPAT( BOTLIB_AI_CHARACTERISTIC_INTEGER, QL_G_trap_Characteristic_Integer );
	QL_BIND_COMPAT( BOTLIB_AI_CHARACTERISTIC_BINTEGER, QL_G_trap_Characteristic_BInteger );
	QL_BIND_COMPAT( BOTLIB_AI_CHARACTERISTIC_STRING, QL_G_trap_Characteristic_String );
	QL_BIND_COMPAT( BOTLIB_AI_ALLOC_CHAT_STATE, QL_G_trap_BotAllocChatState );
	QL_BIND_COMPAT( BOTLIB_AI_FREE_CHAT_STATE, QL_G_trap_BotFreeChatState );
	QL_BIND_COMPAT( BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, QL_G_trap_BotQueueConsoleMessage );
	QL_BIND_COMPAT( BOTLIB_AI_REMOVE_CONSOLE_MESSAGE, QL_G_trap_BotRemoveConsoleMessage );
	QL_BIND_COMPAT( BOTLIB_AI_NEXT_CONSOLE_MESSAGE, QL_G_trap_BotNextConsoleMessage );
	QL_BIND_COMPAT( BOTLIB_AI_NUM_CONSOLE_MESSAGE, QL_G_trap_BotNumConsoleMessages );
	QL_BIND_COMPAT( BOTLIB_AI_INITIAL_CHAT, QL_G_trap_BotInitialChat );
	QL_BIND_COMPAT( BOTLIB_AI_REPLY_CHAT, QL_G_trap_BotReplyChat );
	QL_BIND_COMPAT( BOTLIB_AI_CHAT_LENGTH, QL_G_trap_BotChatLength );
	QL_BIND_COMPAT( BOTLIB_AI_ENTER_CHAT, QL_G_trap_BotEnterChat );
	QL_BIND_COMPAT( BOTLIB_AI_STRING_CONTAINS, QL_G_trap_StringContains );
	QL_BIND_COMPAT( BOTLIB_AI_FIND_MATCH, QL_G_trap_BotFindMatch );
	QL_BIND_COMPAT( BOTLIB_AI_MATCH_VARIABLE, QL_G_trap_BotMatchVariable );
	QL_BIND_COMPAT( BOTLIB_AI_UNIFY_WHITE_SPACES, QL_G_trap_UnifyWhiteSpaces );
	QL_BIND_COMPAT( BOTLIB_AI_REPLACE_SYNONYMS, QL_G_trap_BotReplaceSynonyms );
	QL_BIND_COMPAT( BOTLIB_AI_LOAD_CHAT_FILE, QL_G_trap_BotLoadChatFile );
	QL_BIND_COMPAT( BOTLIB_AI_SET_CHAT_GENDER, QL_G_trap_BotSetChatGender );
	QL_BIND_COMPAT( BOTLIB_AI_SET_CHAT_NAME, QL_G_trap_BotSetChatName );
	QL_BIND_COMPAT( BOTLIB_AI_RESET_GOAL_STATE, QL_G_trap_BotResetGoalState );
	QL_BIND_COMPAT( BOTLIB_AI_RESET_AVOID_GOALS, QL_G_trap_BotResetAvoidGoals );
	QL_BIND_COMPAT( BOTLIB_AI_PUSH_GOAL, QL_G_trap_BotPushGoal );
	QL_BIND_COMPAT( BOTLIB_AI_POP_GOAL, QL_G_trap_BotPopGoal );
	QL_BIND_COMPAT( BOTLIB_AI_EMPTY_GOAL_STACK, QL_G_trap_BotEmptyGoalStack );
	QL_BIND_COMPAT( BOTLIB_AI_DUMP_AVOID_GOALS, QL_G_trap_BotDumpAvoidGoals );
	QL_BIND_COMPAT( BOTLIB_AI_DUMP_GOAL_STACK, QL_G_trap_BotDumpGoalStack );
	QL_BIND_COMPAT( BOTLIB_AI_GOAL_NAME, QL_G_trap_BotGoalName );
	QL_BIND_COMPAT( BOTLIB_AI_GET_TOP_GOAL, QL_G_trap_BotGetTopGoal );
	QL_BIND_COMPAT( BOTLIB_AI_GET_SECOND_GOAL, QL_G_trap_BotGetSecondGoal );
	QL_BIND_COMPAT( BOTLIB_AI_CHOOSE_LTG_ITEM, QL_G_trap_BotChooseLTGItem );
	QL_BIND_COMPAT( BOTLIB_AI_CHOOSE_NBG_ITEM, QL_G_trap_BotChooseNBGItem );
	QL_BIND_COMPAT( BOTLIB_AI_TOUCHING_GOAL, QL_G_trap_BotTouchingGoal );
	QL_BIND_COMPAT( BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, QL_G_trap_BotItemGoalInVisButNotVisible );
	QL_BIND_COMPAT( BOTLIB_AI_GET_LEVEL_ITEM_GOAL, QL_G_trap_BotGetLevelItemGoal );
	QL_BIND_COMPAT( BOTLIB_AI_AVOID_GOAL_TIME, QL_G_trap_BotAvoidGoalTime );
	QL_BIND_COMPAT( BOTLIB_AI_INIT_LEVEL_ITEMS, QL_G_trap_BotInitLevelItems );
	QL_BIND_COMPAT( BOTLIB_AI_UPDATE_ENTITY_ITEMS, QL_G_trap_BotUpdateEntityItems );
	QL_BIND_COMPAT( BOTLIB_AI_LOAD_ITEM_WEIGHTS, QL_G_trap_BotLoadItemWeights );
	QL_BIND_COMPAT( BOTLIB_AI_FREE_ITEM_WEIGHTS, QL_G_trap_BotFreeItemWeights );
	QL_BIND_COMPAT( BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, QL_G_trap_BotSaveGoalFuzzyLogic );
	QL_BIND_COMPAT( BOTLIB_AI_ALLOC_GOAL_STATE, QL_G_trap_BotAllocGoalState );
	QL_BIND_COMPAT( BOTLIB_AI_FREE_GOAL_STATE, QL_G_trap_BotFreeGoalState );
	QL_BIND_COMPAT( BOTLIB_AI_RESET_MOVE_STATE, QL_G_trap_BotResetMoveState );
	QL_BIND_COMPAT( BOTLIB_AI_MOVE_TO_GOAL, QL_G_trap_BotMoveToGoal );
	QL_BIND_COMPAT( BOTLIB_AI_MOVE_IN_DIRECTION, QL_G_trap_BotMoveInDirection );
	QL_BIND_COMPAT( BOTLIB_AI_RESET_AVOID_REACH, QL_G_trap_BotResetAvoidReach );
	QL_BIND_COMPAT( BOTLIB_AI_RESET_LAST_AVOID_REACH, QL_G_trap_BotResetLastAvoidReach );
	QL_BIND_COMPAT( BOTLIB_AI_REACHABILITY_AREA, QL_G_trap_BotReachabilityArea );
	QL_BIND_COMPAT( BOTLIB_AI_MOVEMENT_VIEW_TARGET, QL_G_trap_BotMovementViewTarget );
	QL_BIND_COMPAT( BOTLIB_AI_ALLOC_MOVE_STATE, QL_G_trap_BotAllocMoveState );
	QL_BIND_COMPAT( BOTLIB_AI_FREE_MOVE_STATE, QL_G_trap_BotFreeMoveState );
	QL_BIND_COMPAT( BOTLIB_AI_INIT_MOVE_STATE, QL_G_trap_BotInitMoveState );
	QL_BIND_COMPAT( BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON, QL_G_trap_BotChooseBestFightWeapon );
	QL_BIND_COMPAT( BOTLIB_AI_GET_WEAPON_INFO, QL_G_trap_BotGetWeaponInfo );
	QL_BIND_COMPAT( BOTLIB_AI_LOAD_WEAPON_WEIGHTS, QL_G_trap_BotLoadWeaponWeights );
	QL_BIND_COMPAT( BOTLIB_AI_ALLOC_WEAPON_STATE, QL_G_trap_BotAllocWeaponState );
	QL_BIND_COMPAT( BOTLIB_AI_FREE_WEAPON_STATE, QL_G_trap_BotFreeWeaponState );
	QL_BIND_COMPAT( BOTLIB_AI_RESET_WEAPON_STATE, QL_G_trap_BotResetWeaponState );
	QL_BIND_COMPAT( BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, QL_G_trap_GeneticParentsAndChildSelection );
	QL_BIND_COMPAT( BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC, QL_G_trap_BotInterbreedGoalFuzzyLogic );
	QL_BIND_COMPAT( BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, QL_G_trap_BotMutateGoalFuzzyLogic );
	QL_BIND_COMPAT( BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, QL_G_trap_BotGetNextCampSpotGoal );
	QL_BIND_COMPAT( BOTLIB_AI_GET_MAP_LOCATION_GOAL, QL_G_trap_BotGetMapLocationGoal );
	QL_BIND_COMPAT( BOTLIB_AI_NUM_INITIAL_CHATS, QL_G_trap_BotNumInitialChats );
	QL_BIND_COMPAT( BOTLIB_AI_GET_CHAT_MESSAGE, QL_G_trap_BotGetChatMessage );
	QL_BIND_COMPAT( BOTLIB_AI_REMOVE_FROM_AVOID_GOALS, QL_G_trap_BotRemoveFromAvoidGoals );
	QL_BIND_COMPAT( BOTLIB_AI_PREDICT_VISIBLE_POSITION, QL_G_trap_BotPredictVisiblePosition );
	QL_BIND_COMPAT( BOTLIB_AI_SET_AVOID_GOAL_TIME, QL_G_trap_BotSetAvoidGoalTime );
	QL_BIND_COMPAT( BOTLIB_AI_ADD_AVOID_SPOT, QL_G_trap_BotAddAvoidSpot );
	QL_BIND_COMPAT( BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL, QL_G_trap_AAS_AlternativeRouteGoals );
	QL_BIND_COMPAT( BOTLIB_AAS_PREDICT_ROUTE, QL_G_trap_AAS_PredictRoute );
	QL_BIND_COMPAT( BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX, QL_G_trap_AAS_PointReachabilityAreaIndex );
	QL_BIND_COMPAT( BOTLIB_PC_LOAD_SOURCE, QL_G_trap_PC_LoadSource );
	QL_BIND_COMPAT( BOTLIB_PC_FREE_SOURCE, QL_G_trap_PC_FreeSource );
	QL_BIND_COMPAT( BOTLIB_PC_READ_TOKEN, QL_G_trap_PC_ReadToken );
	QL_BIND_COMPAT( BOTLIB_PC_SOURCE_FILE_AND_LINE, QL_G_trap_PC_SourceFileAndLine );
	QL_BIND_COMPAT( G_CVAR_SETDESCRIPTION, QL_G_trap_Cvar_SetDescription );
	QL_BIND_COMPAT( G_TRAP_GETVALUE, QL_G_trap_GetValue );
	QL_BIND_COMPAT( TRAP_MEMSET, QL_G_trap_Memset );
	QL_BIND_COMPAT( TRAP_MEMCPY, QL_G_trap_Memcpy );
	QL_BIND_COMPAT( TRAP_STRNCPY, QL_G_trap_Strncpy );
	QL_BIND_COMPAT( TRAP_SIN, QL_G_trap_Sin );
	QL_BIND_COMPAT( TRAP_COS, QL_G_trap_Cos );
	QL_BIND_COMPAT( TRAP_ATAN2, QL_G_trap_Atan2 );
	QL_BIND_COMPAT( TRAP_SQRT, QL_G_trap_Sqrt );

#undef QL_BIND_COMPAT
#undef QL_BIND
}


/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	SV_JKAClearGame();
	if ( !gvm ) {
		if ( Com_GameIsQuake3IHV() ) SV_IHVClearGame();
		wolfETTagFiles.clear();
		InvalidateServerGameData();
		serverEntityOverride.clear();
		SV_ResetGameCvarRegistration();
		return;
	}
	VM_Call( gvm, 1, GAME_SHUTDOWN, qfalse );
	wolfETTagFiles.clear();
	InvalidateServerGameData();
	VM_Free( gvm );
	gvm = nullptr;
	serverEntityOverride.clear();
	SV_ResetGameCvarRegistration();
	FS_VM_CloseFiles( H_QAGAME );
}

static bool SV_GameNativeExportAvailable( int exportIndex ) {
	void **exports;

	if ( !gvm || !gvm->dllExports || exportIndex < 0 || exportIndex >= GAME_NATIVE_EXPORT_COUNT ) {
		return false;
	}

	exports = reinterpret_cast<void **>( gvm->dllExports );
	return exports[exportIndex] != nullptr;
}

qboolean SV_GameShouldSuppressVoiceToClient( int senderClientNum, int recipientClientNum ) {
	if ( !SV_GameNativeExportAvailable( GAME_NATIVE_EXPORT_SHOULD_SUPPRESS_VOICE_TO_CLIENT ) ) {
		return qfalse;
	}

	return SV_QBool( VM_Call( gvm, 2, GAME_SHOULD_SUPPRESS_VOICE_TO_CLIENT, senderClientNum, recipientClientNum ) != 0 );
}

qboolean SV_GameIsClientAdmin( int clientNum ) {
	if ( !SV_GameNativeExportAvailable( GAME_NATIVE_EXPORT_IS_CLIENT_ADMIN ) ) {
		return qfalse;
	}

	return SV_QBool( VM_Call( gvm, 1, GAME_IS_CLIENT_ADMIN, clientNum ) != 0 );
}

qboolean SV_GameAreEnemyClients( int clientNumA, int clientNumB ) {
	if ( !SV_GameNativeExportAvailable( GAME_NATIVE_EXPORT_ARE_ENEMY_CLIENTS ) ) {
		return qfalse;
	}

	return SV_QBool( VM_Call( gvm, 2, GAME_ARE_ENEMY_CLIENTS, clientNumA, clientNumB ) != 0 );
}

int SV_GameGetClientScore( int clientNum, int fallbackScore ) {
	if ( !SV_GameNativeExportAvailable( GAME_NATIVE_EXPORT_GET_CLIENT_SCORE ) ) {
		return fallbackScore;
	}

	return static_cast<int>( VM_Call( gvm, 1, GAME_GET_CLIENT_SCORE, clientNum ) );
}


/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM( bool restart ) {
	// start the entity parsing at the beginning
	sv.entityParsePoint = SelectServerEntityString();

	// clear all gentity pointers that might still be set from
	// a previous level
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
	// now done before GAME_INIT call
	for ( client_t &client : SV_Clients() ) {
		client.gentity = nullptr;
	}
	InvalidateServerGameData();
	
	// use the current msec count for a random seed
	// init for this gamestate
	VM_Call( gvm, 3, GAME_INIT, sv.time, Com_Milliseconds(), SV_QBool( restart ) );
}


/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gvm ) {
		InvalidateServerGameData();
		SV_ResetGameCvarRegistration();
		return;
	}
	if ( Com_GameIsQuake3IHV() ) {
		SV_ClearWorld();
		sv.time = 1000;
		SV_InitGameVM( true );
		return;
	}
	VM_Call( gvm, 1, GAME_SHUTDOWN, qtrue );
	// World links belong to the old module's entity binding. The new game
	// may expose fewer entities or query collision before it relinks them.
	SV_ClearWorld();
	InvalidateServerGameData();
	SV_ResetGameCvarRegistration();

	if ( Com_GameIsSTVEFSP() || Com_GameIsQ3Test() ) {
		// The function-table game reloads through GetGameAPI. Every Q3Test
		// engine unloads its library for a restart too, so its static data
		// starts afresh (1.02 restart 0x419e50, 1.06 map_restart 0x41add0).
		VM_Free( gvm );
		gvm = Com_GameIsQ3Test() ? SV_Q3TestLoadGame() : SV_STVEFSPLoadGame();
		if ( !gvm ) {
			Com_Error( ERR_DROP, "VM_Restart on game failed" );
		}
		SV_InitGameVM( true );
		return;
	}

	// do a restart instead of a free
	gvm = VM_Restart( gvm );
	if ( !gvm ) {
		Com_Error( ERR_DROP, "VM_Restart on game failed" );
	}

	SV_CallGameRegisterCvarsOnce( gvm );
	SV_InitGameVM( true );

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
=============
SV_LoadGameModule
=============
*/
static vm_t *SV_LoadGameModule( vmInterpret_t interpret ) {
	SV_InitGameImports();

	return VM_CreateNative( VM_GAME, SV_GameSystemCalls, SV_DllSyscall, interpret,
		ql_game_imports, interpret == VMI_NATIVE ? GAME_NATIVE_API_VERSION : GAME_API_VERSION );
}


/*
=============
SV_CallGameRegisterCvarsOnce
=============
*/
static qboolean SV_CallGameRegisterCvarsOnce( vm_t *vm ) {
	if ( sv_gameCvarsRegistered ) {
		return qtrue;
	}

	if ( VM_CallGameRegisterCvars( vm ) ) {
		sv_gameCvarsRegistered = qtrue;
		return qtrue;
	}

	return qfalse;
}


/*
=============
SV_RegisterGameCvars

Loads qagame early enough for retail native RegisterCvars when a compatible
module is available. FnQL remains engine-only, so missing startup modules defer
to the normal map-load failure path instead of aborting a plain client launch.
=============
*/
void SV_RegisterGameCvars( void ) {
	cvar_t		*var;
	vmInterpret_t	interpret;
	//FIXME these are temp while I make bots run in vm
	extern int	bot_enable;

	if ( !Com_GameIsQuakeLive() || sv_gameCvarsRegistered ) {
		return;
	}

	var = Cvar_Get( "bot_enable", Com_GameProfile()->bots.enabled, 0 );
	if ( var ) {
		bot_enable = var->integer;
	} else {
		bot_enable = 0;
	}

	if ( !gvm ) {
		interpret = static_cast<vmInterpret_t>( Cvar_VariableIntegerValue( "vm_game" ) );
		gvm = SV_LoadGameModule( interpret );
		if ( !gvm ) {
			Com_Printf( "SV_RegisterGameCvars: qagame unavailable; deferring cvar registration until game load.\n" );
			return;
		}
	}

	SV_CallGameRegisterCvarsOnce( gvm );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
	cvar_t	*var;
	if (Com_GameIsWolfET()) {
		Cvar_Get("sv_punkbuster", "0", CVAR_ROM);
		Cvar_Set2("sv_punkbuster", "0", qtrue);
		Com_Printf("WolfET: PunkBuster services are unavailable.\n");
	}
	//FIXME these are temp while I make bots run in vm
	extern int	bot_enable;

	var = Cvar_Get( "bot_enable", Com_GameProfile()->bots.enabled, CVAR_LATCH );
	if ( var ) {
		bot_enable = var->integer;
	}
	else {
		bot_enable = 0;
	}

	/* No Q3Test release shipped its bot library, qabotx86.dll, nor any bot
	 * data; with bot_enable 1, 1.06 to 1.08 stop fatally ("Couldn't load
	 * bot"), and 1.02 to 1.05 have no path that spawns one. Bots stay off. */
	if ( Com_GameIsQ3Test() && bot_enable ) {
		Com_Printf( S_COLOR_YELLOW "Couldn't load bot: Q3Test shipped no bot library (qabotx86.dll); bots stay off.\n" );
		bot_enable = 0;
	}

	/* Jedi Academy's single player hands the engine a function table rather
	 * than numbering its calls, and no side of that boundary exists yet.
	 * Multiplayer's numbered boundary is served as far as it reaches, and
	 * refuses the first import it has not built by name; see
	 * docs/fntech3/COMPATIBILITY.md#jedi-academy-gate-matrix. */
	if ( Com_GameIsJKASP() ) {
		Com_Error( ERR_DROP, "%s is not implemented: Jedi Academy's single-player module "
			"boundary is open. The profile loads its installation and maps, but no module runs yet.",
			Com_GameProfile()->key );
	}

	// load the dll or bytecode; Elite Force SP's and Alice's games are
	// function tables rather than a vmMain
	if ( Com_GameIsQuake3IHV() ) {
		if ( !gvm ) gvm = SV_IHVLoadGame();
	} else {
		gvm = Com_GameIsSTVEFSP() ? SV_STVEFSPLoadGame()
			: Com_GameIsQ3Test() ? SV_Q3TestLoadGame()
			: Com_GameIsRitual() ? SV_AliceLoadGame()
			: Com_GameIsMOHAA() ? SV_MOHAALoadGame()
			: SV_LoadGameModule( static_cast<vmInterpret_t>( Cvar_VariableIntegerValue( "vm_game" ) ) );
	}
	if ( !gvm ) {
		Com_Error( ERR_DROP, "VM_Create on game failed" );
	}

	SV_CallGameRegisterCvarsOnce( gvm );

	SV_InitGameVM( false );

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
====================
SV_STVEFSPAttachClient / SV_STVEFSPCgameMain

The client's side of Elite Force SP's cgame, which lives in the game library.
====================
*/
void SV_STVEFSPAttachClient( svSTVEFSPCgameSyscalls_t syscalls, void ( *detach )( void ) ) {
	stvefSpClientSyscalls = syscalls;
	stvefSpClientDetach = detach;
}

vmMainFunc_t SV_STVEFSPCgameMain( void ) {
	return stvefSpCgameMain;
}


/*
====================
SV_STVEFSPVoiceVolumes

How wide a mouth each entity holds. The client measures them as it plays and
hands them over; the game reads the array they land in through its S_Override
import, and a server with nothing playing leaves every mouth shut.
====================
*/
void SV_STVEFSPVoiceVolumes( const int *levels ) {
	if ( levels == NULL ) {
		return;
	}
	std::copy( levels, levels + FNTECH3_STVEF_SP_LIPSYNC_ENTITIES, stvefSpVoiceVolume );
}


/*
====================
SV_STVEFSPCameraOrigin

Elite Force SP's snapshots are seen from a running camera scene's position,
which the server asks the cgame for (stvoy.exe SV_BuildClientSnapshot
0x4283a3); false when no scene runs.
====================
*/
qboolean SV_STVEFSPCameraOrigin( vec3_t origin ) {
	if ( !stvefSpCgameMain ) {
		return qfalse;
	}
	vec3_t camera = { 0, 0, 0 };
	if ( !stvefSpCgameMain( FNTECH3_STVEF_SP_CG_CAMERA_POS, reinterpret_cast<intptr_t>( camera ) ) ) {
		return qfalse;
	}
	VectorCopy( camera, origin );
	return qtrue;
}


/*
====================
SV_STVEFSPGameAllowedToSaveHere

Retail SG_GameAllowedToSaveHere (stvoy.exe 0x4272e0). The SP menus ask it
before opening any menu with inCamera set, which asks only the game (it
refuses during its camera scenes) and allows a menu when no game runs.
Without inCamera it also requires a running level whose name does not start
with '_' and a living player.
====================
*/
qboolean SV_STVEFSPGameAllowedToSaveHere( qboolean inCamera ) {
	if ( !inCamera ) {
		if ( !com_sv_running || !com_sv_running->integer || sv.state != SS_GAME ) {
			return qfalse;
		}
		const char *mapname = Cvar_VariableString( "mapname" );
		if ( mapname[0] == '_' ) {
			return qfalse;
		}
		// Health is the first stat.
		playerState_t player{};
		if ( !SV_ReadGameClient( 0, &player ) || player.stats[0] <= 0 ) {
			return qfalse;
		}
	}
	if ( !stvefSpGame ) {
		return inCamera;
	}
	return stvefSpGame->GameAllowedToSaveHere() ? qtrue : qfalse;
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}
	if ( Com_GameIsRTCWSP() && !Q_stricmp(Cmd_Argv(0), "savegame") ) {
		fntech3_rtcw_sp_playerState_t player{};
		if ( !serverGameData.ReadPlayerState(0, player) ) {
			Com_Printf("Cannot save RTCW without an active player.\n"); return qtrue;
		}
		// The retail game's cinematic cameraPortal pointer is not serialized as
		// an entity index. Such a save can only reload at the same DLL address.
		if ( player.common.eFlags & fntech3::rtcw::savegame::ViewingCamera ) {
			Com_Printf("Cannot save RTCW during a cinematic; wait for gameplay to resume.\n");
			return qtrue;
		}
	}

	return SV_QBool( VM_Call( gvm, 0, GAME_CONSOLE_COMMAND ) != 0 );
}

// Original SP/MP cgame sends at most a 10,000-byte animation text buffer.
// The game owns parsing and applying the speeds; its synchronous callback
// receives an isolated copy, never an offset or a pointer into another VM.
void SV_RTCWSendMoveSpeeds( const vm_t *source, intptr_t entity, intptr_t text ) {
	if ( !Com_GameIsRTCW() ) Com_Error( ERR_DROP, "RTCW animation handoff requires an RTCW profile" );
	if ( !gvm ) return; // A remote client has no local game to update.
	if ( gvm->index != VM_GAME || !gvm->entryPoint || gvm->dllExports || sizeof( void * ) != sizeof( int32_t ) )
		Com_Error( ERR_DROP, "RTCW animation handoff requires a legacy x86 game module" );
	const int number = SV_RTCWInteger( entity );
	auto copy = SV_RTCWString<10000>( source, text );
	VM_Call( gvm, 2, GAME_RETRIEVE_MOVESPEEDS_FROM_CLIENT,
		number, static_cast<int32_t>( reinterpret_cast<intptr_t>( copy.data() ) ) );
}

// SP's game and cgame share a game-owned animation record, including nested
// pointers. Keep it opaque: only matching legacy native modules can exchange
// this pointer. Its lifetime remains that of the local game module/map.
intptr_t SV_RTCWGetModelInfo( const vm_t *source, intptr_t client, intptr_t name, intptr_t output ) {
	if ( Com_GameProfile()->id != COM_GAME_RTCW_SP )
		Com_Error( ERR_DROP, "RTCW model information requires the SP profile" );
	if ( !gvm ) return qfalse;
	if ( !source || source->index != VM_CGAME || !source->entryPoint || source->dllExports ||
		gvm->index != VM_GAME || !gvm->entryPoint || gvm->dllExports || sizeof( void * ) != sizeof( int32_t ) )
		Com_Error( ERR_DROP, "RTCW model information requires legacy x86 game and cgame modules" );
	const int number = SV_RTCWInteger( client );
	if ( number < 0 || number >= FNTECH3_RTCW_SP_MAX_CLIENTS )
		Com_Error( ERR_DROP, "Invalid RTCW animation client" );
	auto modelName = SV_RTCWString<64>( source, name ); // Original MAX_QPATH.
	void *destination = SV_RTCWPointer( source, output, sizeof( void * ) );
	void *modelInfo;
	std::memcpy( &modelInfo, destination, sizeof( modelInfo ) );
	const intptr_t result = VM_Call( gvm, 3, GAME_GETMODELINFO, number,
		static_cast<int32_t>( reinterpret_cast<intptr_t>( modelName.data() ) ),
		static_cast<int32_t>( reinterpret_cast<intptr_t>( &modelInfo ) ) );
	std::memcpy( destination, &modelInfo, sizeof( modelInfo ) );
	return result;
}
