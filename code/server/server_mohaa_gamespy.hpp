/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_MOHAA_GAMESPY_HPP
#define FNTECH3_SERVER_MOHAA_GAMESPY_HPP

/* Medal of Honor's GameSpy status queries, included by sv_main.cpp.
 *
 * Beside its game port a Medal of Honor server answers GameSpy's queries on
 * a port of its own, the first free one from 12300 (OpenMoHAA
 * SV_InitGamespy and its query SDK, reference only **[O]**). GameSpy's
 * browsers, the later games' in-game lists and third-party tools find and
 * list servers there: a LAN search broadcasts `\status\` to ports 12300 to
 * 12316 and lists whoever answers. 1.00's own Join LAN Game list does not
 * use it -- it asks the game port with `getinfo` (ui/joinlangame.urc's
 * `localservers`) -- and MOHAA.exe cannot be read or run here, so retail's
 * acceptance of these answers is unchecked.
 *
 * Like OpenMoHAA's, the server answers while it runs a multiplayer game --
 * dedicated or listen -- and `sv_gamespy` is set; `net_gamespy_port` is the
 * first port it tries, of a hundred. It answers the LAN's questions only:
 * GameSpy's masters are gone, so it tells none about itself, and it holds no
 * key to prove itself to one (code/compat/gamespy_query.hpp). */

#include "../compat/gamespy_query.hpp"
#include "../qcommon/net_query.h"

static cvar_t *sv_gamespy;
static cvar_t *net_gamespy_port;
static cvar_t *sv_location;

static struct {
	// the net_gamespy_port the open socket was searched from
	int searchedFrom;
	// after a failed search, when to search again, and whether it was said
	int retryTime;
	bool failureReported;
	int queryId = 1;
	rateLimit_t outbound;
} svMOHAAGameSpy;

/* The answers' facts, read as the query arrives. */
static fntech3::gamespy::mohaa::Server SV_MOHAAGameSpyServer( void ) {
	fntech3::gamespy::mohaa::Server server;
	server.location = sv_location ? sv_location->integer : 1;
	server.hostName = sv_hostname->string;
	server.hostPort = Cvar_VariableIntegerValue( "net_port" );
	server.mapName = sv_mapname->string;
	// the game module registers both, and names its game type in the first
	server.gameTypeName = Cvar_VariableString( "g_gametypestring" );
	server.gameType = Cvar_VariableIntegerValue( "g_gametype" );
	server.maxPlayers = std::max( 0, sv.maxclients - sv_privateClients->integer );
	server.dedicated = com_dedicated->integer != 0;
	server.pure = sv.pure != 0;
	server.timeLimit = Cvar_VariableString( "timelimit" );
	server.fragLimit = Cvar_VariableString( "fraglimit" );
	server.rankedServer = Cvar_VariableString( "g_rankedserver" );
	// every connected client, private slots included as OpenMoHAA lists
	// them, with the kills and deaths the game keeps
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		if ( slot.client.state < CS_CONNECTED ) {
			continue;
		}
		fntech3::gamespy::mohaa::Player player;
		player.name = slot.client.name;
		player.ping = slot.client.ping;
		playerState_t ps;
		if ( SV_ReadGameClient( slot.index, &ps ) ) {
			player.kills = ps.persistant[ PERS_SCORE ];
			player.deaths = ps.persistant[ PERS_KILLED ];
		}
		server.players.push_back( std::move( player ) );
	}
	return server;
}

/* A datagram on the query port (net_ip.c's NET_QueryEvent). */
static void SV_MOHAAGameSpyPacket( const netadr_t *from, const byte *data, int length ) {
	if ( !Com_GameIsMOHAA() || !com_sv_running || !com_sv_running->integer || sv.state != SS_GAME
		|| !from || !data || length <= 0 ) {
		return;
	}
	const fntech3::gamespy::Query query =
		fntech3::gamespy::ParseQuery( reinterpret_cast<const char *>( data ), static_cast<std::size_t>( length ) );
	if ( !query.Answerable() ) {
		return;
	}
	// A status answer is many times its query: never an amplifier.
	if ( SVC_RateLimitAddress( from, 10, 1000 ) ) {
		Com_DPrintf( "SV_MOHAAGameSpyPacket: rate limit from %s exceeded, dropping request\n", NET_AdrToString( from ) );
		return;
	}
	if ( SVC_RateLimit( &svMOHAAGameSpy.outbound, 10, 100 ) ) {
		Com_DPrintf( "SV_MOHAAGameSpyPacket: rate limit exceeded, dropping request\n" );
		return;
	}
	const std::vector<std::string> packets = fntech3::gamespy::Reply( query,
		fntech3::gamespy::mohaa::Answer( SV_MOHAAGameSpyServer() ), ++svMOHAAGameSpy.queryId );
	for ( const std::string &packet : packets ) {
		NET_SendQueryPacket( static_cast<int>( packet.size() ), packet.data(), from );
	}
}

/* Stops answering, as the server stops (SV_Shutdown) or stops wanting to. */
void SV_MOHAAGameSpyStop( void ) {
	if ( NET_QuerySocketPort() ) {
		NET_CloseQuerySocket();
	}
	svMOHAAGameSpy.searchedFrom = 0;
	svMOHAAGameSpy.retryTime = 0;
	svMOHAAGameSpy.failureReported = false;
}

/* Each server frame: answer while a multiplayer game runs and sv_gamespy is
 * set, and open the port again after a network restart or a change of
 * net_gamespy_port. */
void SV_MOHAAGameSpyFrame( void ) {
	if ( !Com_GameIsMOHAA() ) {
		return;
	}
	if ( !sv_gamespy ) {
		sv_gamespy = Cvar_Get( "sv_gamespy", "1", 0 );
		Cvar_CheckRange( sv_gamespy, "0", "1", CV_INTEGER );
		Cvar_SetDescription( sv_gamespy, "Answer GameSpy's LAN status queries on the GameSpy query port while a "
			"multiplayer game runs. No GameSpy master is told of the server." );
		net_gamespy_port = Cvar_Get( "net_gamespy_port",
			va( "%i", fntech3::gamespy::mohaa::DefaultQueryPort ), 0 );
		Cvar_CheckRange( net_gamespy_port, "1", "65535", CV_INTEGER );
		Cvar_SetDescription( net_gamespy_port, "The first UDP port tried for GameSpy queries; the first free one "
			"of the next hundred is used. GameSpy LAN searches look from 12300 to 12316." );
		sv_location = Cvar_Get( "sv_location", "1", CVAR_ARCHIVE_ND );
		Cvar_CheckRange( sv_location, "0", nullptr, CV_INTEGER );
		Cvar_SetDescription( sv_location, "The region number GameSpy's basic answer reports." );
	}
	const bool wanted = sv_gamespy->integer && !SV_GameTypeIsSinglePlayer( Cvar_VariableIntegerValue( "g_gametype" ) );
	const int port = NET_QuerySocketPort();
	if ( !wanted || ( port && svMOHAAGameSpy.searchedFrom != net_gamespy_port->integer ) ) {
		SV_MOHAAGameSpyStop();
		if ( !wanted ) {
			return;
		}
	} else if ( port ) {
		return;
	}
	const int now = Sys_Milliseconds();
	if ( svMOHAAGameSpy.retryTime && now - svMOHAAGameSpy.retryTime < 0 ) {
		return;
	}
	const int first = net_gamespy_port->integer;
	const int opened = NET_OpenQuerySocket( first, fntech3::gamespy::mohaa::QueryPortsTried, SV_MOHAAGameSpyPacket );
	if ( opened ) {
		Com_Printf( "GameSpy LAN queries answered on UDP port %i\n", opened );
		svMOHAAGameSpy.searchedFrom = first;
		svMOHAAGameSpy.retryTime = 0;
		svMOHAAGameSpy.failureReported = false;
		return;
	}
	svMOHAAGameSpy.retryTime = now + 10000;
	// networking without IPv4 answers nothing, and says so only to a developer
	if ( !svMOHAAGameSpy.failureReported && ( Cvar_VariableIntegerValue( "net_enabled" ) & NET_ENABLEV4 ) ) {
		Com_Printf( S_COLOR_YELLOW "No UDP port from %i to %i is free for GameSpy queries; trying again in 10 "
			"seconds.\n", first, first + fntech3::gamespy::mohaa::QueryPortsTried - 1 );
	} else if ( !svMOHAAGameSpy.failureReported ) {
		Com_DPrintf( "GameSpy queries are not answered while IPv4 networking is off.\n" );
	}
	svMOHAAGameSpy.failureReported = true;
}

#endif
