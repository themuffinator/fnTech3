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
#include "server_cvar_compat.hpp"
#include "../qcommon/netchan_safety.hpp"
#include "../compat/wolfet_gameinfo.hpp"
#include "server_mohaa_gamespy.hpp"

#include <algorithm>
#include <array>
#include <utility>

serverStatic_t	svs;				// persistant server info
server_t		sv;					// local server
vm_t			*gvm = nullptr;		// game virtual machine

cvar_t	*sv_fps;				// time rate for running non-clients
cvar_t	*sv_audioPVS;			// expanded sound emitter snapshots
cvar_t	*sv_audioPVSRange;		// max distance for expanded sound emitter snapshots
cvar_t	*sv_audioPVSMaxEntities;	// max expanded sound emitters per snapshot
cvar_t	*sv_timeout;			// seconds without any message
cvar_t	*sv_zombietime;			// seconds to sink messages after disconnect
cvar_t	*sv_rconPassword;		// password for remote server commands
cvar_t	*sv_privatePassword;	// password for the privateClient slots
cvar_t	*sv_allowDownload;
cvar_t	*sv_maxclients;
cvar_t	*sv_maxclientsPerIP;
cvar_t	*sv_clientTLD;

cvar_t	*sv_privateClients;		// number of clients reserved for password
cvar_t	*sv_hostname;
cvar_t	*sv_tags;
cvar_t	*sv_masterAdvertise;
cvar_t	*sv_master[MAX_MASTER_SERVERS];		// master server ip address
cvar_t	*sv_reconnectlimit;		// minimum seconds between connect messages
cvar_t	*sv_tempbanmessage;	// ET temporary-ban rejection text
cvar_t	*sv_wwwDownload;		// ET redirected downloads
cvar_t	*sv_wwwBaseURL;
cvar_t	*sv_wwwDlDisconnected;
cvar_t	*sv_wwwFallbackURL;
cvar_t	*sv_padPackets;			// add nop bytes to messages
cvar_t	*sv_killserver;			// menu system can set to 1 to shut server down
cvar_t	*sv_mapname;
cvar_t	*sv_mapChecksum;
cvar_t	*sv_referencedPakNames;
cvar_t	*sv_serverid;
cvar_t	*sv_minRate;
cvar_t	*sv_maxRate;
cvar_t	*sv_dlRate;
cvar_t	*sv_gametype;
cvar_t	*sv_ammoPack;
cvar_t	*sv_pure;
cvar_t	*sv_floodProtect;
cvar_t	*sv_enableRankings;
cvar_t	*sv_rankingsActive;
cvar_t	*sv_lanForceRate; // dedicated 1 (LAN) server forces local client rates to 99999 (bug #491)
cvar_t	*sv_autoRecordDemos;
cvar_t	*sv_cheats;
cvar_t	*sv_gtid;
cvar_t	*sv_idleRestart;
cvar_t	*sv_idleExit;
cvar_t	*sv_errorExit;
cvar_t	*sv_quitOnEmpty;
cvar_t	*sv_quitOnExitLevel;
cvar_t	*sv_altEntDir;
cvar_t	*sv_dumpEntities;
cvar_t	*sv_entityOverrides;
cvar_t	*sv_cylinderScale;
cvar_t	*sv_vac;
cvar_t	*sv_showloss;

cvar_t *sv_levelTimeReset;
cvar_t *sv_filter;

namespace {

fnql::server::cvars::InactivityTimer idleExitTimer;
fnql::server::cvars::InactivityTimer emptyServerTimer;

bool SV_HasHumanClients()
{
	if ( !svs.clients || !sv_maxclients ) {
		return false;
	}
	for ( const client_t &client : SV_Clients() ) {
		if ( client.state >= CS_CONNECTED
			&& client.netchan.remoteAddress.type != NA_BOT ) {
			return true;
		}
	}
	return false;
}

} // namespace

void SV_ResetServerCvarRuntime( void )
{
	idleExitTimer.Reset();
	emptyServerTimer.Reset();
}

qboolean SV_ShouldErrorExit( errorParm_t code )
{
	const bool recoverable = code == ERR_DROP || code == ERR_DISCONNECT;
	const int policy = sv_errorExit ? sv_errorExit->integer : 0;
	const bool running = com_sv_running && com_sv_running->integer;
	return fnql::server::cvars::ShouldEscalateError(
		policy, running, recoverable ) ? qtrue : qfalse;
}

qboolean SV_HandleQuitOnExitLevel( const char *context )
{
	using fnql::server::cvars::ExitLevelAction;
	const int policy = sv_quitOnExitLevel ? sv_quitOnExitLevel->integer : 0;
	switch ( fnql::server::cvars::SelectExitLevelAction( policy ) ) {
		case ExitLevelAction::Continue:
			return qfalse;
		case ExitLevelAction::Shutdown:
			Com_Printf( "sv_quitOnExitLevel %d: shutting down after %s\n",
				policy, context ? context : "level exit" );
			SV_Shutdown( "Server stopped by sv_quitOnExitLevel" );
			return qtrue;
		case ExitLevelAction::Quit:
			Com_Printf( "sv_quitOnExitLevel %d: exiting after %s\n",
				policy, context ? context : "level exit" );
			Cbuf_AddText( "quit\n" );
			return qtrue;
	}
	return qfalse;
}

#ifdef USE_BANS
cvar_t	*sv_banFile;
serverBan_t serverBans[SERVER_MAXBANS];
int serverBansCount = 0;
#endif

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
===============
SV_ExpandNewlines

Converts newlines to "\n" so a line prints nicer

RTCW MP and ET also leave out their "[lon]" and "[lof]" localization tokens,
as WolfMP.exe (0x439e00) and ET 2.60b's et.exe (0x447ce0) do. Only the
console echo changes: clients still receive the tokens.
===============
*/
static const char *SV_ExpandNewlines( const char *in ) {
	static std::array<char, MAX_STRING_CHARS * 2> string{};
	const bool stripLocalization = Com_GameIsRTCWMP() || Com_GameIsWolfET();
	int		l;

	l = 0;
	while ( *in && l < SV_ArraySize(string) - 3 ) {
		if ( *in == '\n' ) {
			string[l++] = '\\';
			string[l++] = 'n';
		} else if ( stripLocalization &&
			( !strncmp( in, "[lon]", 5 ) || !strncmp( in, "[lof]", 5 ) ) ) {
			in += 5;
			continue;
		} else {
			string[l++] = *in;
		}
		in++;
	}
	string[l] = '\0';

	return string.data();
}


/*
======================
SV_ReplacePendingServerCommands

FIXME: This is ugly
======================
*/
#if 0 // unused
static int SV_ReplacePendingServerCommands( client_t *client, const char *cmd ) {
	int i, index, csnum1, csnum2;

	for ( i = client->reliableSent+1; i <= client->reliableSequence; i++ ) {
		index = i & ( SV_ReliableCommandLimit( client ) - 1 );
		//
		if ( !Q_strncmp(cmd, client->reliableCommands[ index ], strlen("cs")) ) {
			sscanf(cmd, "cs %i", &csnum1);
			sscanf(client->reliableCommands[ index ], "cs %i", &csnum2);
			if ( csnum1 == csnum2 ) {
			Q_strncpyz( client->reliableCommands[ index ], cmd, SV_ArraySize( client->reliableCommands[ index ] ) );
				/*
				if ( client->netchan.remoteAddress.type != NA_BOT ) {
					Com_Printf( "WARNING: client %i removed double pending config string %i: %s\n", client-svs.clients, csnum1, cmd );
				}
				*/
				return qtrue;
			}
		}
	}
	return qfalse;
}
#endif


/*
======================
SV_AddServerCommand

The given command will be transmitted to the client, and is guaranteed to
not have future snapshot_t executed before it is executed
======================
*/
void SV_AddServerCommand( client_t *client, const char *cmd ) {
	int		index;
	std::uint32_t pendingCount = 0;

	// this is very ugly but it's also a waste to for instance send multiple config string updates
	// for the same config string index in one snapshot
//	if ( SV_ReplacePendingServerCommands( client, cmd ) ) {
//		return;
//	}

	// Downloads return to CS_CONNECTED after receiving a gamestate. Their
	// engine-side disconnect must still arrive if the transfer fails.
	const bool downloadDisconnect = client->state == CS_CONNECTED &&
		client->downloading && !strncmp( cmd, "disconnect ", 11 );
	if ( client->state < CS_PRIMED && !downloadDisconnect )
		return;

	client->reliableSequence = fnql::net::NextCounter( client->reliableSequence );
	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	if ( !fnql::net::PendingCounterCount( client->reliableSequence,
		client->reliableAcknowledge, pendingCount ) ||
		pendingCount >= SV_ReliableCommandLimit( client ) + 1u ) {
		Com_Printf( "===== pending server commands =====\n" );
		const int printableCount = static_cast<int>( std::min<std::uint32_t>(
			pendingCount, SV_ReliableCommandLimit( client ) ) );
		for ( int i : SV_Indices( printableCount ) ) {
			const int idx = fnql::net::CounterAdd( client->reliableAcknowledge,
				static_cast<std::uint32_t>( i + 1 ) );
			Com_Printf( "cmd %5d: %s\n", i, client->reliableCommands[ idx & ( SV_ReliableCommandLimit( client ) - 1 ) ] );
		}
		Com_Printf( "cmd %5u: %s\n", pendingCount, cmd );
		// SV_DropClient() broadcasts a "print" reason to every client, which
		// re-enters this function for this very client while it is still
		// CS_ACTIVE. Collapse the reliable window first so the re-entrant add
		// takes the normal path instead of recursing until the stack is gone.
		// Only the local bookkeeping field is touched: the wire-visible
		// acknowledge is read from the client's own packets, and
		// reliableSequence must stay monotonic, so neither is disturbed.
		client->reliableAcknowledge = client->reliableSequence;
		SV_DropClient( client, "Server command overflow" );
		return;
	}
	index = client->reliableSequence & ( SV_ReliableCommandLimit( client ) - 1 );
	char *stored = client->reliableCommands[ index ];
	const std::size_t capacity = SV_ArraySize( client->reliableCommands[ index ] );
	if ( client->netchan.wireProfile == NETCHAN_WIRE_QL_RETAIL ) {
		std::size_t offset = 0;
		for ( ; cmd[offset] != '\0' && offset + 1 < capacity; ++offset ) {
			// Retail readers expose '%' as '.', so storing and transmitting the
			// safe byte also keeps the client's usercmd hash aligned with ours.
			stored[offset] = cmd[offset] == '%' ? '.' : cmd[offset];
		}
		stored[offset] = '\0';
	} else {
		Q_strncpyz( stored, cmd, static_cast<int>( capacity ) );
	}
}


/*
=================
SV_SendServerCommand

Sends a reliable command string to be interpreted by 
the client game module: "cp", "print", "chat", etc
A NULL client will broadcast to all clients
=================
*/
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) {
	va_list		argptr;
	std::array<char, MAX_STRING_CHARS + 128> message{}; // slightly larger than allowed, to detect overflows
	int			len;
	
	va_start( argptr, fmt );
	len = Q_vsnprintf( message.data(), SV_ArraySize(message), fmt, argptr );
	va_end( argptr );

	// Q3Test's commands travel each release's own way.
	if ( Com_GameIsQ3Test() ) {
		SV_Q3TestSendServerCommand( cl, message.data() );
		return;
	}

	if ( cl != nullptr ) {
		// outdated clients can't properly decode 1023-chars-long strings
		// http://aluigi.altervista.org/adv/q3msgboom-adv.txt
		if ( len <= 1022 || cl->longstr ) {
			SV_AddServerCommand( cl, message.data() );
		}
		return;
	}

	// hack to echo broadcast prints to console
	if ( com_dedicated->integer && !strncmp( message.data(), "print", 5 ) ) {
		Com_Printf( "broadcast: %s\n", SV_ExpandNewlines( message.data() ) );
	}

	// send the data to all relevant clients
	for ( client_t &client : SV_Clients() ) {
		// RTCW cast AI and ET bots do not consume the broadcast command
		// stream: WolfMP.exe and WolfSP.exe skip SVF_CASTAI, et.exe and
		// etded.exe SVF_BOT. Other profiles' bots still receive it, and
		// explicitly addressed commands stay intact.
		if ( client.state >= CS_PRIMED && ( SV_ClientIsRTCWCastAI( &client )
			|| SV_ClientIsWolfETBot( &client ) ) ) {
			continue;
		}
		if ( len <= 1022 || client.longstr ) {
			SV_AddServerCommand( &client, message.data() );
		}
	}
}


/*
==============================================================================

MASTER SERVER FUNCTIONS

==============================================================================
*/

/*
================
SV_MasterHeartbeat

Send a message to the masters every few minutes to
let it know we are alive, and log information.
We will also have a heartbeat sent when a server
changes from empty to non-empty, and full to non-full,
but not on every player enter or exit.
================
*/
static constexpr int HEARTBEAT_MSEC = 300 * 1000;
static constexpr int MASTERDNS_MSEC = 24 * 60 * 60 * 1000;

// [2] for v4 and v6 address for the same address string.
static std::array<std::array<netadr_t, 2>, MAX_MASTER_SERVERS> sv_masterAddresses{};

/*
================
SV_MasterPublishing

Masters hear only from a public dedicated server ("dedicated 2"; "dedicated 1"
is for LAN play) with a network open, and only while the sv_master gate is on.
================
*/
static bool SV_MasterPublishing( int netenabled )
{
	if ( !com_dedicated || com_dedicated->integer != 2 || !( netenabled & ( NET_ENABLEV4 | NET_ENABLEV6 ) ) )
		return false;
	if ( sv_masterAdvertise && !sv_masterAdvertise->integer )
		return false;	// retail sv_master is the publication gate
	return true;
}

/*
================
SV_ResolveMaster

Resolves sv_master<i + 1> when it changed or when its last answer is over a
day old -- resolving usually causes hitches on win95, so only do it when
needed -- and reports whether either family has an address to send to.
================
*/
static bool SV_ResolveMaster( int i, int netenabled )
{
	auto &adr = sv_masterAddresses[i];
	int res;

	if ( sv_master[i]->modified || svs.time - svs.masterResolveTime[i] > 0 )
	{
		sv_master[i]->modified = qfalse;
		svs.masterResolveTime[i] = svs.time + MASTERDNS_MSEC;

		if(netenabled & NET_ENABLEV4)
		{
			Com_Printf("Resolving %s (IPv4)\n", sv_master[i]->string);
			res = NET_StringToAdr(sv_master[i]->string, &adr[0], NA_IP);

			if(res == 2)
			{
				// if no port was specified, use the profile's master port
				adr[0].port = BigShort(Com_GameMasterPort());
			}

			if(res)
				Com_Printf( "%s resolved to %s\n", sv_master[i]->string, NET_AdrToStringwPort( &adr[0] ) );
			else
				Com_Printf( "%s has no IPv4 address.\n", sv_master[i]->string );
		}
#ifdef USE_IPV6
		if(netenabled & NET_ENABLEV6)
		{
			Com_Printf("Resolving %s (IPv6)\n", sv_master[i]->string);
			res = NET_StringToAdr(sv_master[i]->string, &adr[1], NA_IP6);

			if(res == 2)
			{
				// if no port was specified, use the profile's master port
				adr[1].port = BigShort(Com_GameMasterPort());
			}

			if(res)
				Com_Printf( "%s resolved to %s\n", sv_master[i]->string, NET_AdrToStringwPort( &adr[1] ) );
			else
				Com_Printf( "%s has no IPv6 address.\n", sv_master[i]->string );
		}
#endif
	}

	return adr[0].type != NA_BAD || adr[1].type != NA_BAD;
}

static void SV_MasterHeartbeat( const char *message, bool stopping = false )
{
	const auto &adr = sv_masterAddresses;
	int			i;
	int			netenabled;

	netenabled = Cvar_VariableIntegerValue("net_enabled");

	if ( !SV_MasterPublishing( netenabled ) )
		return;		// only public dedicated servers send heartbeats

	// if not time yet, don't send anything
	if ( svs.nextHeartbeatTime - svs.time > 0 )
		return;

	svs.nextHeartbeatTime = svs.time + HEARTBEAT_MSEC;

	// send to group masters
	for (i = 0; i < MAX_MASTER_SERVERS; i++)
	{
		if(!sv_master[i]->string[0])
			continue;

		if ( !SV_ResolveMaster( i, netenabled ) )
		{
			continue;
		}


		Com_Printf ("Sending heartbeat to %s\n", sv_master[i]->string );

		// this command should be changed if the server info / status format
		// ever changes incompatibly

		if ( Com_GameIsSTVEFMP() ) {
			// Raven's Elite Force masters read key-value heartbeats and then
			// poll getstatus: retail stvoyHM.exe 1.20 sends
			// \heartbeat\<port>\gamename\STEF1\, and \heartstop\ in the same
			// shape as it shuts down (0x434604).
			const char *kind = stopping ? "heartstop" : "heartbeat";
			if(adr[i][0].type != NA_BAD)
				NET_OutOfBandPrint( NS_SERVER, &adr[i][0], "\\%s\\%d\\gamename\\%s\\",
					kind, Cvar_VariableIntegerValue( "net_port" ), message );
			if(adr[i][1].type != NA_BAD)
				NET_OutOfBandPrint( NS_SERVER, &adr[i][1], "\\%s\\%d\\gamename\\%s\\",
					kind, Cvar_VariableIntegerValue( "net_port6" ), message );
			continue;
		}
		if(adr[i][0].type != NA_BAD)
			NET_OutOfBandPrint( NS_SERVER, &adr[i][0], "heartbeat %s\n", message);
		if(adr[i][1].type != NA_BAD)
			NET_OutOfBandPrint( NS_SERVER, &adr[i][1], "heartbeat %s\n", message);
	}
}


/*
=================
SV_MasterShutdown

Informs all masters that this server is going down
=================
*/
void SV_MasterShutdown( void )
{
	// send a heartbeat right now
	svs.nextHeartbeatTime = svs.time;
	SV_MasterHeartbeat( Com_GameFlatlineName() );

	// Wolfenstein's engines name a dead game and send it once (WolfMP.exe
	// 1.4 0x43a2e0, etded.exe 2.60b 0x42eec0); Quake III's send it again
	// to minimize the chance of a drop, and Elite Force's second says it
	// stops (stvoyHM.exe 1.20 0x4345f0).
	if ( Com_GameIsRTCWMP() || Com_GameIsWolfET() )
		return;
	svs.nextHeartbeatTime = svs.time;
	SV_MasterHeartbeat( Com_GameFlatlineName(), Com_GameIsSTVEFMP() != 0 );

	// when the master tries to poll the server, it won't respond, so
	// it will be removed from the list
}


/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

// This is deliberately quite large to make it more of an effort to DoS
static constexpr int MAX_BUCKETS = 16384;
static constexpr int MAX_HASHES = 1024;

static std::array<leakyBucket_t, MAX_BUCKETS> buckets{};
static std::array<leakyBucket_t *, MAX_HASHES> bucketHashes{};
static rateLimit_t outboundRateLimit;

/*
================
SVC_HashForAddress
================
*/
static int SVC_HashForAddress( const netadr_t *address ) {
	const byte	*ip = nullptr;
	int			size = 0;
	int			hash = 0;

	switch ( address->type ) {
		case NA_IP:  ip = address->ipv._4; size = 4;  break;
#ifdef USE_IPV6
		case NA_IP6: ip = address->ipv._6; size = 16; break;
#endif
		default: break;
	}

	for ( int i : SV_Indices( size ) ) {
		hash += static_cast<int>( ip[ i ] ) * ( i + 119 );
	}

	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
	hash &= ( MAX_HASHES - 1 );

	return hash;
}


/*
================
SVC_RelinkToHead
================
*/
static void SVC_RelinkToHead( leakyBucket_t *bucket, int hash ) {

	if ( bucket->prev != nullptr ) {
		bucket->prev->next = bucket->next;
	} else {
		return;
	}

	if ( bucket->next != nullptr ) {
		bucket->next->prev = bucket->prev;
	}

	bucket->next = bucketHashes[ hash ];
	if ( bucketHashes[ hash ] != nullptr ) {
		bucketHashes[ hash ]->prev = bucket;
	}

	bucket->prev = nullptr;
	bucketHashes[ hash ] = bucket;
}

/*
================
SVC_UnlinkBucket
================
*/
static void SVC_UnlinkBucket( leakyBucket_t *bucket ) {
	if ( bucket->prev != nullptr ) {
		bucket->prev->next = bucket->next;
	} else {
		bucketHashes[ bucket->hash ] = bucket->next;
	}

	if ( bucket->next != nullptr ) {
		bucket->next->prev = bucket->prev;
	}
}

/*
================
SVC_BucketAddressMatches
================
*/
static bool SVC_BucketAddressMatches( const leakyBucket_t &bucket, const netadr_t &address ) {
	switch ( bucket.type ) {
		case NA_IP:
			return std::equal( bucket.ipv._4, bucket.ipv._4 + 4, address.ipv._4 );
#ifdef USE_IPV6
		case NA_IP6:
			return std::equal( bucket.ipv._6, bucket.ipv._6 + 16, address.ipv._6 );
#endif
		default:
			return false;
	}
}


/*
================
SVC_BucketForAddress

Find or allocate a bucket for an address
================
*/
static leakyBucket_t *SVC_BucketForAddress( const netadr_t *address, int burst, int period ) {
	static leakyBucket_t dummy = {};
	static int		start = 0;
	const int		hash = SVC_HashForAddress( address );
	const int		now = Sys_Milliseconds();
	leakyBucket_t	*bucket;
	int				n;

	for ( bucket = bucketHashes[ hash ], n = 0; bucket; bucket = bucket->next, n++ ) {
		if ( bucket->type != address->type ) {
			if ( bucket->type == NA_BAD ) {
				return &dummy;
			}
			continue;
		}

		if ( SVC_BucketAddressMatches( *bucket, *address ) ) {
			if ( n > 8 ) {
				SVC_RelinkToHead( bucket, hash );
			}
			return bucket;
		}
	}

	for ( int scanned : SV_Indices( MAX_BUCKETS ) ) {
		int interval;
		(void)scanned;

		if ( start >= MAX_BUCKETS )
			start = 0;
		bucket = &buckets[ start++ ];
		interval = now - bucket->rate.lastTime;

		// Reclaim expired buckets
		if ( bucket->type != NA_BAD && static_cast<unsigned>( interval ) > static_cast<unsigned>( bucket->rate.burst * period ) ) {
			SVC_UnlinkBucket( bucket );
			bucket->type = NA_BAD;
		}

		if ( bucket->type == NA_BAD ) {
			bucket->type = address->type;
			switch ( address->type ) {
				case NA_IP:
					std::copy_n( address->ipv._4, 4, bucket->ipv._4 );
					break;
#ifdef USE_IPV6
				case NA_IP6:
					std::copy_n( address->ipv._6, 16, bucket->ipv._6 );
					break;
#endif
				default: break;
			}

			bucket->rate.lastTime = now;
			bucket->rate.burst = 0;
			bucket->hash = hash;
			bucket->toxic = 0;

			// Add to the head of the relevant hash chain
			bucket->next = bucketHashes[ hash ];
			if ( bucketHashes[ hash ] != nullptr ) {
				bucketHashes[ hash ]->prev = bucket;
			}

			bucket->prev = nullptr;
			bucketHashes[ hash ] = bucket;

			return bucket;
		}
	}

	// Couldn't allocate a bucket for this address
	return nullptr;
}


/*
================
SVC_RateLimit
================
*/
qboolean SVC_RateLimit( rateLimit_t *bucket, int burst, int period ) {
	int now = Sys_Milliseconds();
	int interval = now - bucket->lastTime;
	int expired = interval / period;
	int expiredRemainder = interval % period;

	if ( expired > bucket->burst || interval < 0 ) {
		bucket->burst = 0;
		bucket->lastTime = now;
	} else {
		bucket->burst -= expired;
		bucket->lastTime = now - expiredRemainder;
	}

	if ( bucket->burst < burst ) {
		bucket->burst++;
		return SV_QBool( false );
	}

	return SV_QBool( true );
}


/*
================
SVC_RateDrop
================
*/
static void SVC_RateDrop( leakyBucket_t *bucket, int burst ) {
	if ( bucket != nullptr ) {
		if ( bucket->toxic < 10000 )
			++bucket->toxic;
		bucket->rate.burst = burst * bucket->toxic;
		bucket->rate.lastTime = Sys_Milliseconds();
	}
}


/*
================
SVC_RateRestoreBurst
================
*/
static void SVC_RateRestoreBurst( leakyBucket_t *bucket ) {
	if ( bucket != nullptr ) {
		if ( bucket->rate.burst > 0 ) {
			bucket->rate.burst--;
		}
	}
}


/*
================
SVC_RateRestoreToxic
================
*/
static void SVC_RateRestoreToxic( leakyBucket_t *bucket ) {
	if ( bucket != nullptr ) {
		if ( bucket->toxic > 0 ) {
			bucket->toxic--;
		}
	}
}


/*
================
SVC_RateLimitAddress

Rate limit for a particular address
================
*/
static fntech3::wolfet::gameinfo::GameInfo svWolfETGameInfo;
static bool svWolfETGameInfoLoaded;

void SV_WolfETLoadGameInfo( void ) {
	void *text = nullptr;
	const int length = FS_ReadFile( "gameinfo.dat", &text );
	svWolfETGameInfo = length > 0 && text
		? fntech3::wolfet::gameinfo::Parse( static_cast<const char *>( text ), static_cast<size_t>( length ) )
		: fntech3::wolfet::gameinfo::GameInfo{};
	if ( text ) {
		FS_FreeFile( text );
	}
	svWolfETGameInfoLoaded = true;
}

static const fntech3::wolfet::gameinfo::GameInfo &SV_WolfETGameInfo( void ) {
	if ( !svWolfETGameInfoLoaded ) {
		SV_WolfETLoadGameInfo();
	}
	return svWolfETGameInfo;
}

// Quake 3's engine knows one single-player game type; Enemy Territory's
// numbers its game types differently and lists its single-player ones.
// Medal of Honor's single player is 0, where Quake III's 2 is its team
// match (1.00's gamex86.dll names g_gametype 0 "Single-Player" and 2
// "Team-Match" at 0x310e5a50); a listen server hosting a team match had
// refused every remote query and connection.
// Raven's Jedi games count single player after their own modes, where
// Quake III's 2 is Jedi Master: each engine's spmap sets Jedi Academy's 5
// (jamp.exe 1.01 SV_Map_f, 0x45617f) and Jedi Outcast's 4 (jk2mp.exe 1.02a,
// 0x4479c0).
qboolean SV_GameTypeIsSinglePlayer( int gameType ) {
	if ( Com_GameIsWolfET() ) {
		return fntech3::wolfet::gameinfo::IsSinglePlayer( SV_WolfETGameInfo(), gameType ) ? qtrue : qfalse;
	}
	if ( Com_GameIsMOHAA() ) {
		return gameType == 0 ? qtrue : qfalse;
	}
	if ( Com_GameIsJKAMP() ) {
		return gameType == SV_JKA_GT_SINGLE_PLAYER ? qtrue : qfalse;
	}
	if ( Com_GameIsJK2MP() ) {
		return gameType == SV_JK2_GT_SINGLE_PLAYER ? qtrue : qfalse;
	}
	return gameType == GT_SINGLE_PLAYER ? qtrue : qfalse;
}

/*
==================
SV_GameTypeHidesListenServer

Whether a listen server running this game type answers no remote getinfo,
getstatus or getchallenge. Quake III's engine keeps its single-player game
to itself, and Jedi Outcast's does too, by its own number: jk2mp.exe 1.02a's
SVC_Info (0x44e7b0), SVC_Status (0x44e5f0) and SV_GetChallenge (0x448750)
each return at g_gametype 4. Jedi Academy's never asks the game type:
jamp.exe 1.01's SVC_Info (0x45fa10) and SV_GetChallenge (0x457080) read
ui_singlePlayerActive alone and its SVC_Status (0x45f830) reads neither, so
its single-player game answers as any other does.
==================
*/
qboolean SV_GameTypeHidesListenServer( int gameType ) {
	if ( Com_GameIsJKAMP() || Com_GameIsQuakeLive() ) {
		return qfalse;
	}
	return SV_GameTypeIsSinglePlayer( gameType );
}

/*
==================
SV_ServerTypeDropsPacket

Quake Live keeps a server to itself by its server type, not by its game type:
quakelive_steam.exe's SV_PacketEvent (0x4e4500) drops every packet from
anywhere but its own client while sv_serverType is 0, offline, the default and
the Start Match menu's offline choice. Neither it nor SV_GetChallenge reads
g_gametype or ui_singlePlayerActive, which its map command sets for a local
match (0x4ddfff..0x4de00f), so Race, game type 2, hides nothing. Only the
client executable was read, so a dedicated build keeps answering.
==================
*/
qboolean SV_ServerTypeDropsPacket( const netadr_t *from ) {
#ifdef DEDICATED
	(void)from;
	return qfalse;
#else
	return Com_GameIsQuakeLive() && from->type != NA_LOOPBACK
		&& Cvar_VariableIntegerValue( "sv_serverType" ) == 0 ? qtrue : qfalse;
#endif
}

/*
==================
SV_ServerTypeRefusesChallenge

A Quake Live LAN server, sv_serverType 1, refuses a challenge from outside the
LAN (quakelive_steam.exe SV_GetChallenge 0x4df430..0x4df4a7).
==================
*/
qboolean SV_ServerTypeRefusesChallenge( const netadr_t *from ) {
	return Com_GameIsQuakeLive() && Cvar_VariableIntegerValue( "sv_serverType" ) == 1
		&& !Sys_IsLANAddress( from ) ? qtrue : qfalse;
}

qboolean SV_WolfETGameTypeIsCoop( int gameType ) {
	return fntech3::wolfet::gameinfo::IsCoop( SV_WolfETGameInfo(), gameType ) ? qtrue : qfalse;
}

qboolean SV_WolfETSinglePlayerEnabled( void ) {
	return SV_WolfETGameInfo().spEnabled ? qtrue : qfalse;
}

int SV_WolfETDefaultSinglePlayerGameType( void ) {
	return SV_WolfETGameInfo().defaultSPGameType;
}

qboolean SVC_RateLimitAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	return bucket ? SVC_RateLimit( &bucket->rate, burst, period ) : SV_QBool( true );
}


/*
================
SVC_RateRestoreAddress

Decrease burst rate
================
*/
void SVC_RateRestoreBurstAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	SVC_RateRestoreBurst( bucket );
}


/*
================
SVC_RateRestoreToxicAddress

Decrease toxicity
================
*/
void SVC_RateRestoreToxicAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	SVC_RateRestoreToxic( bucket );
}


/*
================
SVC_RateDropAddress
================
*/
void SVC_RateDropAddress( const netadr_t *from, int burst, int period ) {
	leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );

	SVC_RateDrop( bucket, burst );
}


cvar_t *sv_minPing;
cvar_t *sv_maxPing;

/*
================
SV_ProfileHasPingLimits

quake3.exe 1.32 registers sv_minPing and sv_maxPing archived and in its
serverinfo (0x4391d8), publishes each once it is set and refuses a client
whose challenge round trip falls outside them; WolfMP.exe 1.4, etded.exe
2.60b, stvoyHM.exe 1.20, jamp.exe 1.01 and jk2mp.exe 1.02a do the same.
Quake Live's retail engine has neither, nor do the other profiles.
================
*/
bool SV_ProfileHasPingLimits( void ) {
	return Com_GameIsQuake3() || Com_GameIsRTCWMP() || Com_GameIsWolfET() ||
		Com_GameIsSTVEFMP() || Com_GameIsJKAMP() || Com_GameIsJK2MP();
}

int SV_PingLimitVerdict( int ping, int minPing, int maxPing ) {
	if ( minPing && ping < minPing ) {
		return 1;
	}
	if ( maxPing && ping > maxPing ) {
		return 2;
	}
	return 0;
}

/*
================
SV_MasterGameCompleteStatus

Wolfenstein's game modules end a match with the console command
gameCompleteStatus, and their engines then send every master the server's
status under that name (WolfMP.exe 1.4 0x43a2a2, etded.exe 2.60b 0x42ed17):
the serverinfo and one "score ping name" line per connected client, as a
status reply has, without a challenge to echo. Only a public dedicated
server publishes, as it does its heartbeats.
================
*/
void SV_MasterGameCompleteStatus( void ) {
	std::array<char, MAX_NAME_LENGTH + 32> player{}; // score + ping + name
	std::array<char, MAX_PACKETLEN> status{};
	std::array<char, MAX_INFO_STRING + 160> infostring{};
	playerState_t	ps;
	char	*s;
	int		statusLength;
	int		playerLength;
	const int	netenabled = Cvar_VariableIntegerValue( "net_enabled" );

	if ( !SV_MasterPublishing( netenabled ) ) {
		return;
	}
	if ( SV_GameTypeIsSinglePlayer( Cvar_VariableIntegerValue( "g_gametype" ) ) ) {
		return;
	}

	Q_strncpyz( infostring.data(), Cvar_InfoString( CVAR_SERVERINFO, nullptr ), SV_ArraySize( infostring ) );

	s = status.data();
	statusLength = strlen( infostring.data() ) + 20; // strlen( "gameCompleteStatus\n\n" )
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		if ( slot.client.state < CS_CONNECTED || !SV_ReadGameClient( slot.index, &ps ) ) {
			continue;
		}
		playerLength = Com_sprintf( player.data(), SV_ArraySize( player ), "%i %i \"%s\"\n",
			ps.persistant[ PERS_SCORE ], slot.client.ping, slot.client.name );
		if ( statusLength + playerLength >= MAX_PACKETLEN - 4 ) {
			break; // can't hold any more
		}
		s = Q_stradd( s, player.data() );
		statusLength += playerLength;
	}

	for ( int i = 0; i < MAX_MASTER_SERVERS; i++ ) {
		if ( !sv_master[i]->string[0] || !SV_ResolveMaster( i, netenabled ) ) {
			continue;
		}
		Com_Printf( "Sending gameCompleteStatus to %s\n", sv_master[i]->string );
		for ( const auto &address : sv_masterAddresses[i] ) {
			if ( address.type != NA_BAD ) {
				NET_OutOfBandPrint( NS_SERVER, &address, "gameCompleteStatus\n%s\n%s",
					infostring.data(), status.data() );
			}
		}
	}
}


/*
================
SVC_Status

Responds with all the info that qplug or qspy can see about the server
and all connected players.  Used for getting detailed information after
the simple info query.
================
*/
static void SVC_Status( const netadr_t *from ) {
	std::array<char, MAX_NAME_LENGTH + 32> player{}; // score + ping + name
	std::array<char, MAX_PACKETLEN> status{};
	char	*s;
	playerState_t	ps;
	int		statusLength;
	int		playerLength;
	std::array<char, MAX_INFO_STRING + 160> infostring{}; // add some space for challenge string

	// ignore if we are in single player
#ifndef DEDICATED
	if ( SV_GameTypeHidesListenServer( Cvar_VariableIntegerValue( "g_gametype" ) ) || Cvar_VariableIntegerValue("ui_singlePlayerActive")) {
		return;
	}
#endif

	// Prevent using getstatus as an amplifier
	if ( SVC_RateLimitAddress( from, 10, 1000 ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "SVC_Status: rate limit from %s exceeded, dropping request\n",
				NET_AdrToString( from ) );
		}
		return;
	}

	// Allow getstatus to be DoSed relatively easily, but prevent
	// excess outbound bandwidth usage when being flooded inbound
	if ( SVC_RateLimit( &outboundRateLimit, 10, 100 ) ) {
		Com_DPrintf( "SVC_Status: rate limit exceeded, dropping request\n" );
		return;
	}

	// A maximum challenge length of 128 should be more than plenty.
	if ( strlen( Cmd_Argv( 1 ) ) > 128 )
		return;

	Q_strncpyz( infostring.data(), Cvar_InfoString( CVAR_SERVERINFO, nullptr ), SV_ArraySize(infostring) );

	// echo back the parameter to status. so master servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring.data(), "challenge", Cmd_Argv( 1 ) );
	if ( Com_GameIsSTVEFMP() ) {
		// Retail publishes protocol in its serverinfo, and Raven's master
		// lists only servers whose status carries it.
		Info_SetValueForKey( infostring.data(), "protocol", va( "%i", com_protocol->integer ) );
	}

	s = status.data();
	status[0] = '\0';
	statusLength = strlen( infostring.data() ) + 16; // strlen( "statusResponse\n\n" )

	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		if ( slot.client.state >= CS_CONNECTED ) {

			if ( !SV_ReadGameClient( slot.index, &ps ) ) {
				continue;
			}
			playerLength = Com_sprintf( player.data(), SV_ArraySize(player), "%i %i \"%s\"\n", 
				ps.persistant[ PERS_SCORE ], slot.client.ping, slot.client.name );
			
			if ( statusLength + playerLength >= MAX_PACKETLEN-4 )
				break; // can't hold any more
			
			s = Q_stradd( s, player.data() );
			statusLength += playerLength;
		}
	}

	NET_OutOfBandPrint( NS_SERVER, from, "statusResponse\n%s\n%s", infostring.data(), status.data() );
}


/*
================
SV_RetailInfoKeys

The keys a profile's retail engine adds to its info reply beyond Quake III's,
which that game's own browser shows and filters on. A retail RTCW client
drops any reply without gamename "wolfmp" (WolfMP.exe 1.4
CL_ServerInfoPacket 0x40ee19), so for that game they decide whether the
server is listed at all. Enemy Territory's are written with its others.
================
*/
static void SV_RetailInfoKeys( char *info ) {
	if ( Com_GameIsRTCWMP() ) {
		// WolfMP.exe 1.4 SVC_Info 0x43a878-0x43a994, in its order.
		Info_SetValueForKey( info, "sv_allowAnonymous", va( "%i", Cvar_VariableIntegerValue( "sv_allowAnonymous" ) ) );
		Info_SetValueForKey( info, "gameskill", va( "%i", Cvar_VariableIntegerValue( "g_gameskill" ) ) );
		Info_SetValueForKey( info, "friendlyFire", va( "%i", Cvar_VariableIntegerValue( "g_friendlyFire" ) ) );
		Info_SetValueForKey( info, "maxlives", Cvar_VariableIntegerValue( "g_maxlives" ) ? "1" : "0" );
		Info_SetValueForKey( info, "tourney", va( "%i", Cvar_VariableIntegerValue( "g_noTeamSwitching" ) ) );
		// No anti-cheat service runs in this engine.
		Info_SetValueForKey( info, "punkbuster", "0" );
		Info_SetValueForKey( info, "gamename", "wolfmp" );
		Info_SetValueForKey( info, "g_antilag", Cvar_VariableString( "g_antilag" ) );
	} else if ( Com_GameIsJKAMP() ) {
		// jamp.exe 1.01 SVC_Info 0x45fb57-0x45fc1e: a duel (3) or power
		// duel (4) publishes its own weapon set.
		const int gametype = sv_gametype->integer;
		Info_SetValueForKey( info, "needpass", va( "%i", Cvar_VariableIntegerValue( "g_needpass" ) ) );
		Info_SetValueForKey( info, "truejedi", va( "%i", Cvar_VariableIntegerValue( "g_jediVmerc" ) ) );
		Info_SetValueForKey( info, "wdisable", va( "%i", Cvar_VariableIntegerValue(
			gametype == 3 || gametype == 4 ? "g_duelWeaponDisable" : "g_weaponDisable" ) ) );
		Info_SetValueForKey( info, "fdisable", va( "%i", Cvar_VariableIntegerValue( "g_forcePowerDisable" ) ) );
	} else if ( Com_GameIsJK2MP() ) {
		// jk2mp.exe 1.02a SVC_Info 0x44e9a7.
		Info_SetValueForKey( info, "sv_allowAnonymous", va( "%i", Cvar_VariableIntegerValue( "sv_allowAnonymous" ) ) );
	} else if ( Com_GameIsQuake3() ) {
		// quake3.exe 1.32 SVC_Info 0x439dad; no anti-cheat service runs here.
		Info_SetValueForKey( info, "punkbuster", "0" );
	}
}

/*
================
SVC_Info

Responds with a short info message that should be enough to determine
if a user is interested in a server to do a full status
================
*/
static void SVC_Info( const netadr_t *from ) {
	int		count, humans;
	const char	*gamedir;
	std::array<char, MAX_INFO_STRING> infostring{};

	// ignore if we are in single player
#ifndef DEDICATED
	if ( SV_GameTypeHidesListenServer( Cvar_VariableIntegerValue( "g_gametype" ) ) || Cvar_VariableIntegerValue("ui_singlePlayerActive")) {
		return;
	}
#endif

	// Prevent using getinfo as an amplifier
	if ( SVC_RateLimitAddress( from, 10, 1000 ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "SVC_Info: rate limit from %s exceeded, dropping request\n",
				NET_AdrToString( from ) );
		}
		return;
	}

	// Allow getinfo to be DoSed relatively easily, but prevent
	// excess outbound bandwidth usage when being flooded inbound
	if ( SVC_RateLimit( &outboundRateLimit, 10, 100 ) ) {
		Com_DPrintf( "SVC_Info: rate limit exceeded, dropping request\n" );
		return;
	}

	/*
	 * Check whether Cmd_Argv(1) has a sane length. This was not done in the original Quake3 version which led
	 * to the Infostring bug discovered by Luigi Auriemma. See http://aluigi.altervista.org/ for the advisory.
	 */

	// A maximum challenge length of 128 should be more than plenty.
	if ( strlen( Cmd_Argv( 1 ) ) > 128 )
		return;

	// don't count privateclients
	count = humans = 0;
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		if ( slot.index < sv_privateClients->integer ) {
			continue;
		}
		if ( slot.client.state >= CS_CONNECTED ) {
			count++;
			if (slot.client.netchan.remoteAddress.type != NA_BOT) {
				humans++;
			}
		}
	}

	// echo back the parameter to status. so servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring.data(), "challenge", Cmd_Argv(1) );

	Info_SetValueForKey( infostring.data(), "protocol", va( "%i", com_protocol->integer ) );
	Info_SetValueForKey( infostring.data(), "hostname", sv_hostname->string );
	Info_SetValueForKey( infostring.data(), "mapname", sv_mapname->string );
	Info_SetValueForKey( infostring.data(), "clients", va("%i", count) );
	Info_SetValueForKey( infostring.data(), "g_humanplayers", va( "%i", humans ) );
	Info_SetValueForKey( infostring.data(), "sv_maxclients", va( "%i", sv.maxclients - sv_privateClients->integer ) );
	Info_SetValueForKey( infostring.data(), "gametype", va( "%i", sv_gametype->integer ) );
	Info_SetValueForKey( infostring.data(), "pure", va( "%i", sv.pure ) );
	Info_SetValueForKey( infostring.data(), "g_needpass", va( "%d", Cvar_VariableIntegerValue( "g_needpass" ) ) );
	if (Com_GameIsWolfET()) {
		// ET browsers filter on this identity and use their own rule-key names.
		Info_SetValueForKey(infostring.data(), "gamename", "et");
		Info_SetValueForKey(infostring.data(), "serverload", "-1"); // No load measurement available.
		for (const auto &rule : std::array<std::pair<const char *, const char *>, 5>{{
			{"sv_allowAnonymous", "sv_allowAnonymous"}, {"friendlyFire", "g_friendlyFire"},
			{"g_antilag", "g_antilag"}, {"weaprestrict", "g_heavyWeaponRestriction"},
			{"balancedteams", "g_balancedteams"}}}) {
			Info_SetValueForKey(infostring.data(), rule.first, va("%d", Cvar_VariableIntegerValue(rule.second)));
		}
		Info_SetValueForKey(infostring.data(), "maxlives", Cvar_VariableIntegerValue("g_maxlives") ? "1" : "0");
		Info_SetValueForKey(infostring.data(), "needpass", Cvar_VariableIntegerValue("g_needpass") ? "1" : "0");
	}
	if ( sv_minPing && sv_minPing->integer ) {
		Info_SetValueForKey( infostring.data(), "minPing", va( "%i", sv_minPing->integer ) );
	}
	if ( sv_maxPing && sv_maxPing->integer ) {
		Info_SetValueForKey( infostring.data(), "maxPing", va( "%i", sv_maxPing->integer ) );
	}
	SV_RetailInfoKeys( infostring.data() );
	gamedir = Cvar_VariableString( "fs_game" );
	if ( *gamedir != '\0' ) {
		Info_SetValueForKey( infostring.data(), "game", gamedir );
	}
	// Retail marks a server running an Expansion Pack mode (SVC_Info 0x4349ff).
	if ( SV_STVEFModeActive() ) {
		Info_SetValueForKey( infostring.data(), "vv", "1" );
	}

	if ( Com_GameIsSTVEFMP() ) {
		// Retail: infoResponse "<info>" on one line (SVC_Info 0x434800).
		NET_OutOfBandPrint( NS_SERVER, from, "infoResponse \"%s\"", infostring.data() );
		return;
	}
	NET_OutOfBandPrint( NS_SERVER, from, "infoResponse\n%s", infostring.data() );
}


/*
================
SV_FlushRedirect
================
*/
static netadr_t redirectAddress; // for rcon return messages

static void SV_FlushRedirect( const char *outputbuf )
{
	if ( *outputbuf )
	{
		NET_OutOfBandPrint( NS_SERVER, &redirectAddress, "print\n%s", outputbuf );
	}
}


/*
===============
SVC_RemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
static void SVC_RemoteCommand( const netadr_t *from ) {
	static rateLimit_t bucket;
	bool		valid;
	// TTimo - scaled down to accumulate, but not overflow anything network wise, print wise etc.
	// (OOB messages are the bottleneck here)
	std::array<char, 1024 - 16> sv_outputbuf{};
	const char	*cmd_aux, *pw;

	// Prevent using rcon as an amplifier and make dictionary attacks impractical
	if ( SVC_RateLimitAddress( from, 10, 1000 ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "SVC_RemoteCommand: rate limit from %s exceeded, dropping request\n",
				NET_AdrToString( from ) );
		}
		return;
	}

	pw = Cmd_Argv( 1 );
	// Q3Test 1.02 names its password rcon_password.
	const char *const rconPassword = Com_GameIsQ3Test() ? SV_Q3TestRconPassword() : sv_rconPassword->string;
	if ( ( rconPassword[0] && strcmp( pw, rconPassword ) == 0 ) ||
		( rconPassword2[0] && strcmp( pw, rconPassword2 ) == 0 ) ) {
		valid = true;
		Com_Printf( "Rcon from %s: %s\n", NET_AdrToString( from ), Cmd_ArgsFrom( 2 ) );
	} else {
		// Make DoS via rcon impractical
		if ( SVC_RateLimit( &bucket, 10, 1000 ) ) {
			Com_DPrintf( "SVC_RemoteCommand: rate limit exceeded, dropping request\n" );
			return;
		}

		valid = false;
		Com_Printf( "Bad rcon from %s: %s\n", NET_AdrToString( from ), Cmd_ArgsFrom( 2 ) );
	}

	// start redirecting all print outputs to the packet
	redirectAddress = *from;
	Com_BeginRedirect( sv_outputbuf.data(), SV_ArraySize(sv_outputbuf), SV_FlushRedirect );
	const auto endRedirect = SV_MakeScopeExit( []() { Com_EndRedirect(); } );

	if ( !sv_rconPassword->string[0] && !rconPassword2[0] ) {
		Com_Printf( "No rconpassword set on the server.\n" );
	} else if ( !valid ) {
		Com_Printf( "Bad rconpassword.\n" );
	} else {
		// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
		// get the command directly, "rcon <pass> <command>" to avoid quoting issues
		// extract the command by walking
		// since the cmd formatting can fuckup (amount of spaces), using a dumb step by step parsing
		cmd_aux = Cmd_Cmd();
		while ( *cmd_aux && *cmd_aux <= ' ' ) // skip whitespace
			cmd_aux++;
		cmd_aux += 4; // "rcon"
		while ( *cmd_aux == ' ' )
			cmd_aux++;
		if ( *cmd_aux == '"' ) {
			cmd_aux++;
			while ( *cmd_aux && *cmd_aux != '"' ) // quoted password
				cmd_aux++;
			if ( *cmd_aux == '"' )
				cmd_aux++;
		} else {
			while ( *cmd_aux && *cmd_aux != ' ' ) // password
				cmd_aux++;
		}
		while ( *cmd_aux == ' ' )
			cmd_aux++;

		// Run a copy, as Quake III 1.32 did: Cmd_ExecuteString tokenizes into
		// the buffer cmd_aux points into, so a command nothing here knows was
		// forwarded to the server as that buffer's shifted tail, short by the
		// length of "rcon <password> ".
		std::array<char, BIG_INFO_STRING> command{};
		Q_strncpyz( command.data(), cmd_aux, SV_ArraySize( command ) );
		Cmd_ExecuteString( command.data() );
	}
}


void SV_RemoteCommandPacket( const netadr_t *from ) {
	SVC_RemoteCommand( from );
}


/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
static void SV_ConnectionlessPacket( const netadr_t *from, msg_t *msg ) {
	static constexpr char CONNECT_PREFIX[] = "connect ";
	struct connectionlessCommand_t {
		const char *name;
		void ( *func )( const netadr_t *from );
	};
	static constexpr std::array<connectionlessCommand_t, 3> commands = {{
		{ "getstatus", SVC_Status },
		{ "getinfo", SVC_Info },
		{ "connect", SV_DirectConnect },
	}};
	const char *s;
	const char *c;

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );		// skip the -1 marker
	// Medal of Honor's packets carry a direction byte after the marker, and
	// its connect text starts one byte later.
	const int header = Com_GameIsMOHAA() ? 5 : 4;
	if ( header == 5 ) {
		MSG_ReadByte( msg );
	}

	if ( msg->cursize >= header + 8 && std::equal( CONNECT_PREFIX, CONNECT_PREFIX + sizeof( CONNECT_PREFIX ) - 1,
			reinterpret_cast<const char *>( msg->data + header ) ) ) {
		if ( msg->cursize > MAX_INFO_STRING*2 ) { // if we assume 200% compression ratio on userinfo
			if ( com_developer->integer ) {
				Com_Printf( "%s : connect packet is too long - %i\n", NET_AdrToString( from ), msg->cursize );
			}
			return;
		}
		// Elite Force clients send connect as plain text, as Ritual's engine
		// does: fakk2.exe 1.02 prints it (0x00421686) and its server reads the
		// line with no Huffman stage (0x00447100).
		if ( !Com_GameIsSTVEFMP() && !Com_GameIsRitual() ) {
			if ( !Huff_Decompress( msg, header + 8 ) ) {
				return;
			}
		}
	}

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	c = Cmd_Argv(0);

	if ( com_developer->integer ) {
		Com_Printf( "SV packet %s : %s\n", NET_AdrToString( from ), c );
	}

	if ( !Q_stricmp(c, "rcon") ) {
		SVC_RemoteCommand( from );
		return;
	}

	if ( !com_sv_running->integer ) {
		return;
	}

	if ( !Q_stricmp( c, "getchallenge" ) ) {
		SV_GetChallenge( from, msg );
		return;
	}

	for ( const auto &command : commands ) {
		if ( !Q_stricmp( c, command.name ) ) {
			command.func( from );
			return;
		}
	}
#ifndef STANDALONE
	if ( !Q_stricmp( c, "ipAuthorize" ) ) {
		// removed from codebase since stateless challenges
		return;
	}
#endif
	if ( !Q_stricmp( c, "disconnect" ) ) {
		// if a client starts up a local server, we may see some spurious
		// server disconnect messages when their new server sees our final
		// sequenced messages to the old client
		return;
	}
	if ( com_developer->integer ) {
		Com_Printf( "bad connectionless packet from %s:\n%s\n",
			NET_AdrToString( from ), s );
	}
}

//============================================================================

/*
=================
SV_PacketEvent
=================
*/
void SV_PacketEvent( const netadr_t *from, msg_t *msg ) {
	// Dedicated IHV developer probes exercise native game imports only. Its
	// protocol-33 envelopes must never enter the retail packet dispatcher.
	if ( Com_GameIsQuake3IHV() ) return;
	// Q3Test's packets are its own sessions' (server_q3test_net.hpp).
	if ( Com_GameIsQ3Test() ) {
		SV_Q3TestPacketEvent( from, msg );
		return;
	}
	if ( SV_ServerTypeDropsPacket( from ) ) {
		return;
	}
	int			qport;
	SV_SteamHandleIncomingPacket( from, msg );

	if ( msg->cursize < 6 ) // too short for anything
		return;

	// check for connectionless packet (0xffffffff) first
	int32_t packetMarker = 0;
	Com_Memcpy( &packetMarker, msg->data, sizeof( packetMarker ) );
	if ( packetMarker == -1 ) {
		SV_ConnectionlessPacket( from, msg );
		return;
	}

	if ( sv.state == SS_DEAD ) {
		return;
	}

	// read the qport out of the message so we can fix up
	// stupid address translating routers
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg ); // sequence number
	qport = MSG_ReadShort( msg ) & 0xffff;

	// find which client the message is from
	for ( client_t &client : SV_Clients() ) {
		if ( client.state == CS_FREE ) {
			continue;
		}
		if ( !NET_CompareBaseAdr( from, &client.netchan.remoteAddress ) ) {
			continue;
		}
		// it is possible to have multiple clients from a single IP
		// address, so they are differentiated by the qport variable
		if ( client.netchan.qport != qport ) {
			continue;
		}

		// make sure it is a valid, in sequence packet
		if ( SV_Netchan_Process( &client, msg ) ) {
			// the IP port can't be used to differentiate clients, because
			// some address translating routers periodically change UDP
			// port assignments
			if ( client.netchan.remoteAddress.port != from->port ) {
				Com_Printf( "SV_PacketEvent: fixing up a translated port\n" );
				client.netchan.remoteAddress.port = from->port;
			}
			// zombie clients still need to do the Netchan_Process
			// to make sure they don't need to retransmit the final
			// reliable message, but they don't do any other processing
			if ( client.state != CS_ZOMBIE ) {
				client.lastPacketTime = svs.time;	// don't timeout
				SV_ExecuteClientMessage( &client, msg );
			}
			return;
		}
	}
}


/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
static void SV_CalcPings( void ) {
	int			j;
	int			total, count;
	int			delta;

	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		client_t &client = slot.client;
		if ( client.state != CS_ACTIVE ) {
			client.ping = 999;
			continue;
		}
		if ( !client.gentity ) {
			client.ping = 999;
			continue;
		}
		if ( client.netchan.remoteAddress.type == NA_BOT ) {
			client.ping = 0;
			continue;
		}

		total = 0;
		count = 0;
		for ( j = 0 ; j < PACKET_BACKUP ; j++ ) {
			if ( client.frames[j].messageAcked == 0 ) {
				continue;
			}
			delta = client.frames[j].messageAcked - client.frames[j].messageSent;
			count++;
			total += delta;
		}
		if (!count) {
			client.ping = 999;
		} else {
			client.ping = total/count;
			if ( client.ping > 999 ) {
				client.ping = 999;
			}
		}

		// let the game dll know about the ping
		if ( !SV_SetGameClientPing( slot.index, client.ping ) ) {
			Com_Error( ERR_DROP, "%s: invalid game client %i", __func__,
				slot.index );
			return;
		}
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->integer 
seconds, drop the connection.  Server time is used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static void SV_CheckTimeouts( void ) {
	int			droppoint;
	int			zombiepoint;

	droppoint = svs.time - 1000 * sv_timeout->integer;
	zombiepoint = svs.time - 1000 * sv_zombietime->integer;

	for ( client_t &client : SV_Clients() ) {
		if ( client.state == CS_FREE ) {
			continue;
		}
		if ( client.platformAuthSession && !client.platformAuthValidated &&
			static_cast<uint32_t>( svs.time ) - client.platformAuthStartedTime > 10000u ) {
			SV_DropClient( &client, "Platform authentication timed out" );
			continue;
		}
		// message times may be wrong across a changelevel
		if ( client.lastPacketTime - svs.time > 0 ) {
			client.lastPacketTime = svs.time;
		}

		if ( client.state == CS_ZOMBIE && client.lastPacketTime - zombiepoint < 0 ) {
			// using the client id cause the cl->name is empty at this point
			SV_PrintClientStateChange( &client, CS_FREE );
			client.state = CS_FREE;	// can now be reused
			continue;
		}
		// Campaign actors have no packet source. Keep clock correction and
		// zombie reclamation above this exemption.
		if ( Com_GameIsRTCWSP() && SV_ClientIsRTCWCastAI( &client ) ) {
			continue;
		}
		if ( client.justConnected && svs.time - client.lastPacketTime > 4000 ) {
			// for real client 4 seconds is more than enough to respond
			SVC_RateDropAddress( &client.netchan.remoteAddress, 10, 1000 ); // enforce burst with progressive multiplier
			SV_DropClient( &client, nullptr ); // drop silently
			client.state = CS_FREE;
			continue;
		}
		// SP starts normal packet timeout accounting once its entity exists.
		// The admission deadline above still reclaims abandoned connections.
		if ( Com_GameIsRTCWSP() && !client.gentity ) {
			continue;
		}
		if ( client.state >= CS_CONNECTED && client.lastPacketTime - droppoint < 0 ) {
			// wait several frames so a debugger session doesn't
			// cause a timeout
			if ( ++client.timeoutCount > 5 ) {
				SV_DropClient( &client, "timed out" );
				client.state = CS_FREE;	// don't bother with zombie state
			}
		} else {
			client.timeoutCount = 0;
		}
	}

	if ( emptyServerTimer.Poll( !SV_HasHumanClients(),
		static_cast<uint32_t>( svs.time ),
		sv_quitOnEmpty ? sv_quitOnEmpty->integer : 0 ) ) {
		Com_Printf( "Server has been empty for %d seconds; quitting.\n",
			sv_quitOnEmpty->integer );
		Cbuf_AddText( "quit\n" );
	}
}


/*
==================
SV_CheckPaused
==================
*/
static bool SV_CheckPaused( void ) {

#ifdef DEDICATED
	// can't pause on dedicated servers
	return false;
#else
	int	count;

	if ( !cl_paused->integer ) {
		return false;
	}

	// only pause if there is just a single client connected
	count = 0;
	for ( const client_t &client : SV_Clients() ) {
		if ( client.state >= CS_CONNECTED && client.netchan.remoteAddress.type != NA_BOT ) {
			count++;
		}
	}

	if ( count > 1 ) {
		// don't pause
		if (sv_paused->integer)
			Cvar_Set("sv_paused", "0");
		return false;
	}

	if (!sv_paused->integer)
		Cvar_Set("sv_paused", "1");

	return true;
#endif // !DEDICATED
}


/*
==================
SV_FrameMsec
Return time in millseconds until processing of the next server frame.
==================
*/
int SV_FrameMsec( void )
{
	if ( sv_fps )
	{
		int frameMsec;
		
		frameMsec = 1000.0f / sv_fps->value;
		
		if ( frameMsec < sv.timeResidual )
			return 0;
		else
			return frameMsec - sv.timeResidual;
	}
	else
		return 1;
}


/*
==================
SV_TrackCvarChanges
==================
*/
void SV_TrackCvarChanges( void )
{
	if ( sv_maxRate->integer && sv_maxRate->integer < 1000 ) {
		Cvar_Set( "sv_maxRate", "1000" );
		Com_DPrintf( "sv_maxRate adjusted to 1000\n" );
	}

	if ( sv_minRate->integer && sv_minRate->integer < 1000 ) {
		Cvar_Set( "sv_minRate", "1000" );
		Com_DPrintf( "sv_minRate adjusted to 1000\n" );
	}

	Cvar_ResetGroup( CVG_SERVER, qfalse );

	if ( sv.state == SS_DEAD || !svs.clients )
		return;

	for ( client_t &client : SV_Clients() ) {
		if ( client.state >= CS_CONNECTED ) {
			SV_UserinfoChanged( &client, qfalse, qfalse ); // do not update userinfo, do not run filter
		}
	}
}


/*
==================
SV_Restart
==================
*/
static void SV_Restart( const char *reason ) {
	bool sv_shutdown = false;
	std::array<char, MAX_CVAR_VALUE_STRING> mapName{};

	if ( SV_HandleQuitOnExitLevel( reason ) ) {
		return;
	}

	if ( svs.clients ) {
		// check if we can reset map time without full server shutdown
		for ( const client_t &client : SV_Clients() ) {
			if ( client.state >= CS_CONNECTED ) {
				sv_shutdown = true;
				break;
			}
		}
	}

	sv.time = 0; // force level time reset
	sv.restartTime = 0;
	
	Cvar_VariableStringBuffer( "mapname", mapName.data(), SV_ArraySize(mapName) );
	
	if ( sv_shutdown ) {
		SV_Shutdown( reason );
	}

	Cbuf_AddText( va( "map %s\n", mapName.data() ) );
}


/*
==================
SV_Frame

Player movement occurs as a result of packet events, which
happen before SV_Frame is called
==================
*/
void SV_Frame( int msec, int realMsec ) {
	int		frameMsec;
	int		startTime;

	if ( Cvar_CheckGroup( CVG_SERVER ) )
		SV_TrackCvarChanges(); // update rate settings, etc.

	// the menu kills the server with this cvar
	if ( sv_killserver->integer ) {
		SV_Shutdown( "Server was killed" );
		Cvar_Set( "sv_killserver", "0" );
		return;
	}

	if ( !com_sv_running->integer )
	{
		if ( com_dedicated->integer )
		{
			if ( idleExitTimer.Poll( true,
				static_cast<uint32_t>( Sys_Milliseconds() ),
				sv_idleExit ? sv_idleExit->integer : 0 ) ) {
				Com_Error( ERR_FATAL,
					"Dedicated server remained idle for sv_idleExit (%d) seconds",
					sv_idleExit->integer );
			}
			// A finite sleep is required while the retail idle-exit policy is
			// armed; the disabled path retains FnQL's event-driven blocking.
			Sys_Sleep( sv_idleExit && sv_idleExit->integer > 0 ? 50 : -1 );
		}
		else {
			idleExitTimer.Reset();
		}
		return;
	}
	idleExitTimer.Reset();

	// Medal of Honor answers GameSpy's queries on a port of its own.
	SV_MOHAAGameSpyFrame();

	Zmq_UpdatePasswords();
	Zmq_PumpRcon();

	svs.time += realMsec;
	SV_SteamP2PFrame();

	// allow pause if only the local client is connected
	if ( SV_CheckPaused() ) {
		return;
	}

	// if it isn't time for the next frame, do nothing
	frameMsec = 0;
	const bool stvefSP = Com_GameIsSTVEFSP();
	if ( stvefSP ) {
		// Retail SP keeps fixed simulation ticks when timescale changes
		// (stvoy.exe 1.20, SV_Frame 0x4265c0). Com_ModifyMsec has already
		// scaled the elapsed game time; sv_fps is range-checked at startup.
		frameMsec = 1000 / sv_fps->integer;
		if ( msec > 1000 ) {
			msec = 1000;
		}
	} else if ( com_timescale->value > 0.0f ) {
		frameMsec = static_cast<int>( 1000.0f / sv_fps->value * com_timescale->value );
		if ( frameMsec < 1 ) {
			frameMsec = 1;
		}
	}

	sv.timeResidual += msec;
	if ( stvefSP && sv.timeResidual < frameMsec ) {
		// SP publishes one snapshot after its new tick(s), without
		// duplicating game state on intervening render frames.
		return;
	}

	if ( !com_dedicated->integer )
		SV_BotFrame( sv.time + sv.timeResidual );

	// if time is about to hit the 32nd bit, kick all clients
	// and clear sv.time, rather
	// than checking for negative time wraparound everywhere.
	// 2giga-milliseconds = 23 days, so it won't be too often
	if ( sv.time > 0x78000000 ) {
		SV_Restart( "Restarting server due to time wrapping" );
		return;
	}

	// Retail recycles an unattended server after a day.  Bots do not keep a
	// host alive for this policy, and the inherited 12-hour time-maintenance
	// restart below remains intact as an independent FnQL safeguard.
	if ( sv_idleRestart && sv_idleRestart->integer && svs.time > 24 * 60 * 60 * 1000
		&& !SV_HasHumanClients() ) {
		if ( SV_HandleQuitOnExitLevel( "idle server restart" ) ) {
			return;
		}
		SV_Shutdown( "Restarting idle server" );
		Cbuf_AddText( "vstr nextmap\n" );
		return;
	}

	// try to do silent restart earlier if possible
	if ( sv.time > (12*3600*1000) && ( sv_levelTimeReset->integer == 0 || sv.time > 0x40000000 ) ) {
		if ( svs.clients ) {
			bool hasRealClient = false;
			for ( const client_t &client : SV_Clients() ) {
				// FIXME: deal with bots (reconnect?)
				if ( client.state != CS_FREE && client.netchan.remoteAddress.type != NA_BOT ) {
					hasRealClient = true;
					break;
				}
			}
			if ( !hasRealClient ) {
				SV_Restart( "Restarting server" );
				return;
			}
		}
	}

	if ( sv.restartTime && sv.time - sv.restartTime >= 0 ) {
		sv.restartTime = 0;
		if ( SV_HandleQuitOnExitLevel( "scheduled map_restart" ) ) {
			return;
		}
		Cbuf_AddText( "map_restart 0\n" );
		return;
	}

	// update infostrings if anything has been changed
	if ( cvar_modifiedFlags & CVAR_SERVERINFO ) {
		SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO, nullptr ) );
		cvar_modifiedFlags &= ~CVAR_SERVERINFO;
	}
	if ( cvar_modifiedFlags & CVAR_SYSTEMINFO ) {
		SV_UpdateSysteminfoConfigstring();
	}
	SV_UpdateQLCvarConfigstrings( qfalse );
	SV_UpdateRTCWCvarConfigstrings( qfalse );

	if ( com_speeds->integer ) {
		startTime = Sys_Milliseconds();
	} else {
		startTime = 0;	// quite a compiler warning
	}

	// update ping based on the all received frames
	SV_CalcPings();

	if (com_dedicated->integer) SV_BotFrame (sv.time);

	// run the game simulation in chunks
	while ( frameMsec > 0 && sv.timeResidual >= frameMsec ) {
		sv.timeResidual -= frameMsec;
		sv.time += frameMsec;

		// let everything in the world think and move
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
	}
	// Jedi Academy's Ghoul2 clock catches up after the frames, not before
	// each (jamp.exe 0x460883): they pose one or more frames behind.
	SV_JKAGhoul2SetTime( sv.time );

	if ( com_speeds->integer ) {
		time_game = Sys_Milliseconds () - startTime;
	}

	// Elite Force SP runs the game's saving code without a file while
	// sv_testsave is set.
	if ( Com_GameIsSTVEFSP() ) {
		SV_STVEFSPTestSave();
	}

	// check timeouts
	SV_CheckTimeouts();

	// reset current and build new snapshot on first query
	SV_IssueNewSnapshot();

	// IHV's native clients keep their own snapshot history, built here in the
	// original's position (0x41ac87) rather than by the retail snapshot pass.
	if ( Com_GameIsQuake3IHV() ) {
		SV_IHVServerFrame();
	}

	// send messages back to the clients
	SV_SendClientMessages();

	// send a heartbeat to the master if needed
	SV_MasterHeartbeat( Com_GameHeartbeatName() );
}


/*
====================
SV_RateMsec

Return the number of msec until another message can be sent to
a client based on its rate settings
====================
*/

static constexpr int UDPIP_HEADER_SIZE = 28;
static constexpr int UDPIP6_HEADER_SIZE = 48;

int SV_RateMsec( const client_t *client )
{
	int rate, rateMsec;
	int messageSize;
	
	if ( !client->rate )
		return 0;

	messageSize = client->netchan.lastSentSize;

#ifdef USE_IPV6
	if ( client->netchan.remoteAddress.type == NA_IP6 )
		messageSize += UDPIP6_HEADER_SIZE;
	else
#endif
		messageSize += UDPIP_HEADER_SIZE;
		
	rateMsec = messageSize * 1000 / client->rate;
	rate = Sys_Milliseconds() - client->netchan.lastSentTime;
	
	if ( rate > rateMsec )
		return 0;
	else
		return rateMsec - rate;
}


/*
====================
SV_SendQueuedPackets

Send download messages and queued packets in the time that we're idle, i.e.
not computing a server frame or sending client snapshots.
Return the time in msec until we expect to be called next
====================
*/
int SV_SendQueuedPackets( void )
{
	int downloadBytes;
	int dlStart, deltaT, delayT;
	static int dlNextRound = 0;
	int timeVal = INT_MAX;

	// Send out fragmented packets now that we're idle
	delayT = SV_SendQueuedMessages();
	if(delayT >= 0)
		timeVal = delayT;

	if(sv_dlRate->integer)
	{
		// Rate limiting. This is very imprecise for high
		// download rates due to millisecond timedelta resolution
		dlStart = Sys_Milliseconds();
		deltaT = dlNextRound - dlStart;

		if(deltaT > 0)
		{
			if(deltaT < timeVal)
				timeVal = deltaT + 1;
		}
		else
		{
			downloadBytes = SV_SendDownloadMessages();

			if(downloadBytes)
			{
				// There are active downloads
				deltaT = Sys_Milliseconds() - dlStart;

				delayT = 1000 * downloadBytes;
				delayT /= sv_dlRate->integer * 1024;

				if(delayT <= deltaT + 1)
				{
					// Sending the last round of download messages
					// took too long for given rate, don't wait for
					// next round, but always enforce a 1ms delay
					// between DL message rounds so we don't hog
					// all of the bandwidth. This will result in an
					// effective maximum rate of 1MB/s per user, but the
					// low download window size limits this anyways.
					if(timeVal > 2)
						timeVal = 2;

					dlNextRound = dlStart + deltaT + 1;
				}
				else
				{
					dlNextRound = dlStart + delayT;
					delayT -= deltaT;

					if(delayT < timeVal)
						timeVal = delayT;
				}
			}
		}
	}
	else
	{
		if(SV_SendDownloadMessages())
			timeVal = 0;
	}

	return timeVal;
}
