"""Medal of Honor's GameSpy status queries.

A Medal of Honor server answers GameSpy's first query protocol on a port of
its own, the first free one from 12300 (code/compat/gamespy_query.hpp,
code/server/server_mohaa_gamespy.hpp, net_ip.c's query socket). The format
is fnTech3's reading of OpenMoHAA's server and of the GameSpy SDK files it
carries, reference only **[O]**: backslash key/value text, the parts answered
basic, info, rules and players, packets of at most 1,400 bytes each ending in
`\\queryid\\<id>.<n>`, the last carrying `\\final\\`. No retail client runs
here, so no retail browser has read these answers.

The fixtures compile the protocol header itself, and the engine's
single-player rule sliced from sv_main.cpp; the rest checks that only the
mohaa profile opens the port, and where it opens and closes."""
import unittest
from pathlib import Path

from mohaa_permanent_decal_tests import build_and_run, function, read

ROOT = Path(__file__).resolve().parents[1]

PROTOCOL = r'''
#include "code/compat/gamespy_query.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gs = fntech3::gamespy;

/* '|' stands for a backslash, to keep the expected text readable. */
static std::string S( const char *text ) {
	std::string out( text );
	for ( char &c : out ) {
		if ( c == '|' ) c = '\\';
	}
	return out;
}

static gs::Query Q( const std::string &text ) {
	return gs::ParseQuery( text.data(), text.size() );
}

static gs::mohaa::Server Sample() {
	gs::mohaa::Server server;
	server.location = 1;
	server.hostName = "fnTech3 LAN";
	server.hostPort = 12203;
	server.mapName = "dm/mohdm1";
	server.gameTypeName = "Free-For-All";
	server.gameType = 1;
	server.maxPlayers = 8;
	server.dedicated = true;
	server.pure = false;
	server.timeLimit = "20";
	server.fragLimit = "0";
	server.rankedServer = "0";
	server.players = { { "Alpha", 3, 1, 50 }, { "Bravo", 0, 2, 0 } };
	return server;
}

static const char *const kBasic = "|gamename|mohaa|gamever|1.00|location|1";
static const char *const kInfo = "|hostname|fnTech3 LAN|hostport|12203|mapname|mohdm1|gametype|Free-For-All"
	"|numplayers|2|maxplayers|8|gamemode|openplaying|gametype_i|1|dedicated|1|pure|0";
static const char *const kRules = "|timelimit|20|fraglimit|0|rankedserver|0";
static const char *const kPlayers = "|player_0|Alpha|frags_0|3|deaths_0|1|ping_0|50"
	"|player_1|Bravo|frags_1|0|deaths_1|2|ping_1|0";

static void Parsing() {
	gs::Query q = Q( S( "|status|" ) );
	assert( q.status && !q.basic && !q.info && !q.rules && !q.players && !q.echo && q.Answerable() );
	q = Q( S( "|basic||info|" ) );
	assert( q.basic && q.info && !q.status );
	q = Q( S( "|info||rules|" ) );
	assert( q.info && q.rules && !q.basic );
	q = Q( S( "|echo|hello" ) );
	assert( q.echo && q.echoText == "hello" && q.Answerable() );
	q = Q( S( "|echo|abc|final|" ) );
	assert( q.echo && q.echoText == "abc" );
	q = Q( S( "|echo||basic|" ) );
	assert( q.echo && q.echoText.empty() && q.basic );
	/* a key must be framed on both sides */
	assert( !Q( S( "|statusx|" ) ).Answerable() );
	assert( !Q( S( "|status" ) ).Answerable() );
	assert( !Q( "status" ).Answerable() );
	/* the CD-key service's queries, a master's challenge and the SDK's
	 * demonstration are not answered */
	q = Q( S( ";|status|" ) );
	assert( q.cdkey && !q.Answerable() );
	q = Q( S( "|secure|ABCDEF" ) );
	assert( q.secure && !q.Answerable() );
	assert( !Q( S( "|packets|" ) ).Answerable() );
	q = Q( S( "|secure|ABCDEF|basic|" ) );
	assert( q.secure && q.basic && q.Answerable() );
	/* text ends at its first NUL */
	const char nul[] = "\\status\\\0\\basic\\";
	q = gs::ParseQuery( nul, sizeof( nul ) - 1 );
	assert( q.status && !q.basic );
	/* empty, null and longer than 255 bytes: nothing */
	assert( !gs::ParseQuery( nullptr, 8 ).Answerable() );
	assert( !gs::ParseQuery( "", 0 ).Answerable() );
	std::string longest = S( "|status|" );
	longest.resize( 255, 'x' );
	assert( Q( longest ).Answerable() );
	longest.push_back( 'x' );
	assert( !Q( longest ).Answerable() );
}

static void Pairs() {
	std::string out;
	gs::AppendPair( out, "a", "" );
	assert( out.empty() );
	gs::AppendPair( out, "a", "", true );
	assert( out == S( "|a|" ) );
	out.clear();
	gs::AppendPair( out, "host", "one\\two" );
	assert( out == S( "|host|one_two" ) );
	out.clear();
	gs::AppendPair( out, "", "value" );
	assert( out.empty() );
	assert( gs::mohaa::BrowserMapName( "dm/mohdm1" ) == "mohdm1" );
	assert( gs::mohaa::BrowserMapName( "obj/obj_team1" ) == "obj_team1" );
	assert( gs::mohaa::BrowserMapName( "m1l1" ) == "m1l1" );
	assert( gs::mohaa::BrowserMapName( "a/b/c" ) == "b/c" );
}

static void MohaaAnswers() {
	const gs::Answers answers = gs::mohaa::Answer( Sample() );
	assert( answers.basic == S( kBasic ) );
	assert( answers.info == S( kInfo ) );
	assert( answers.rules == S( kRules ) );
	assert( answers.players == S( kPlayers ) );
	/* keys OpenMoHAA reports for the later games or added itself */
	const std::string all = answers.basic + answers.info + answers.rules + answers.players;
	for ( const char *key : { "|sprinton|", "|realism|", "|allowlean|", "|minplayers|", "|botskill|" } ) {
		assert( all.find( S( key ) ) == std::string::npos );
	}
	/* an empty cvar leaves its key out; an empty name is still a player */
	gs::mohaa::Server server = Sample();
	server.hostName.clear();
	server.rankedServer.clear();
	server.players = { { "", 0, 0, 999 } };
	const gs::Answers empty = gs::mohaa::Answer( server );
	assert( empty.info.find( S( "|hostname|" ) ) == std::string::npos );
	assert( empty.info.find( S( "|numplayers|1|" ) ) != std::string::npos );
	assert( empty.rules == S( "|timelimit|20|fraglimit|0" ) );
	assert( empty.players == S( "|player_0||frags_0|0|deaths_0|0|ping_0|999" ) );
}

static void Replies() {
	const gs::Answers answers = gs::mohaa::Answer( Sample() );
	/* the LAN search's \status\: one packet, the four parts in order */
	std::vector<std::string> packets = gs::Reply( Q( S( "|status|" ) ), answers, 2 );
	assert( packets.size() == 1 );
	assert( packets[0] == S( kBasic ) + S( kInfo ) + S( kRules ) + S( kPlayers ) + S( "|final||queryid|2.1" ) );
	/* parts in the protocol's order, whatever the query's */
	packets = gs::Reply( Q( S( "|players||basic|" ) ), answers, 7 );
	assert( packets.size() == 1 && packets[0] == S( kBasic ) + S( kPlayers ) + S( "|final||queryid|7.1" ) );
	packets = gs::Reply( Q( S( "|rules|" ) ), answers, 3 );
	assert( packets.size() == 1 && packets[0] == S( kRules ) + S( "|final||queryid|3.1" ) );
	packets = gs::Reply( Q( S( "|info|" ) ), answers, 4 );
	assert( packets.size() == 1 && packets[0] == S( kInfo ) + S( "|final||queryid|4.1" ) );
	/* the echo, after the parts */
	packets = gs::Reply( Q( S( "|echo|ping123|basic|" ) ), answers, 5 );
	assert( packets.size() == 1 && packets[0] == S( kBasic ) + S( "|echo|ping123|final||queryid|5.1" ) );
	packets = gs::Reply( Q( S( "|echo|" ) ), answers, 6 );
	assert( packets.size() == 1 && packets[0] == S( "|echo||final||queryid|6.1" ) );
	/* nothing answerable, nothing sent */
	assert( gs::Reply( Q( S( ";|status|" ) ), answers, 8 ).empty() );
	assert( gs::Reply( Q( S( "|secure|ABCDEF" ) ), answers, 8 ).empty() );
	assert( gs::Reply( Q( "hello" ), answers, 8 ).empty() );
	/* an empty part sends nothing of its own */
	gs::Answers bare;
	packets = gs::Reply( Q( S( "|players|" ) ), bare, 9 );
	assert( packets.size() == 1 && packets[0] == S( "|final||queryid|9.1" ) );
}

/* The content of the packets, without each one's queryid. */
static std::string Joined( const std::vector<std::string> &packets, int queryId ) {
	std::string all;
	for ( std::size_t i = 0; i < packets.size(); i++ ) {
		const std::string tail = S( "|queryid|" ) + std::to_string( queryId ) + "." + std::to_string( i + 1 );
		assert( packets[i].size() <= gs::MaxPacketBytes );
		assert( packets[i].size() > tail.size() );
		assert( packets[i].compare( packets[i].size() - tail.size(), tail.size(), tail ) == 0 );
		const bool last = i + 1 == packets.size();
		assert( ( packets[i].find( S( "|final|" ) ) != std::string::npos ) == last );
		all += packets[i].substr( 0, packets[i].size() - tail.size() );
	}
	return all;
}

static void Splitting() {
	/* a full server: 64 players with long names spill across packets, each
	 * pair whole, numbered from 1, \final\ in the last alone */
	gs::mohaa::Server server = Sample();
	server.players.clear();
	for ( int i = 0; i < 64; i++ ) {
		server.players.push_back( { "Soldier_" + std::to_string( i ) + std::string( 20, 'n' ), i, 64 - i, 100 + i } );
	}
	const gs::Answers answers = gs::mohaa::Answer( server );
	assert( answers.players.size() > 3 * ( gs::MaxPacketBytes - gs::PacketReserve ) );
	const std::vector<std::string> packets = gs::Reply( Q( S( "|status|" ) ), answers, 11 );
	assert( packets.size() >= 4 );
	const std::string all = Joined( packets, 11 );
	assert( all == answers.basic + answers.info + answers.rules + answers.players + S( "|final|" ) );
	/* no packet starts or ends inside a pair: each holds an even number of
	 * backslashes before its queryid, and starts with one */
	for ( std::size_t i = 0; i < packets.size(); i++ ) {
		const std::string body = packets[i].substr( 0, packets[i].rfind( S( "|queryid|" ) ) );
		assert( !body.empty() && body[0] == '\\' );
		std::size_t slashes = 0;
		for ( char c : body ) slashes += c == '\\';
		if ( i + 1 < packets.size() ) {
			assert( slashes % 2 == 0 );
		}
	}
	/* basic, info and rules share the first, the players following */
	assert( packets[0].find( answers.basic + answers.info + answers.rules ) == 0 );

	/* a part that fits a packet goes whole into the next one rather than
	 * being cut */
	gs::Answers halves;
	gs::AppendPair( halves.basic, "a", std::string( 800, 'a' ) );
	gs::AppendPair( halves.info, "b", std::string( 800, 'b' ) );
	const std::vector<std::string> two = gs::Reply( Q( S( "|basic||info|" ) ), halves, 14 );
	assert( two.size() == 2 );
	assert( two[0] == halves.basic + S( "|queryid|14.1" ) );
	assert( two[1] == halves.info + S( "|final||queryid|14.2" ) );

	/* a single pair longer than a packet is left out; the rest goes */
	gs::Answers huge;
	huge.basic = S( "|a|1" );
	gs::AppendPair( huge.info, "big", std::string( 1400, 'x' ) );
	huge.info += S( "|b|2" );
	const std::vector<std::string> cut = gs::Reply( Q( S( "|basic||info|" ) ), huge, 12 );
	const std::string kept = Joined( cut, 12 );
	assert( kept == S( "|a|1|b|2|final|" ) );
	/* a part of exactly a packet's room still goes whole */
	gs::Answers exact;
	exact.basic = S( "|k|" ) + std::string( gs::MaxPacketBytes - gs::PacketReserve - 3, 'v' );
	assert( exact.basic.size() == gs::MaxPacketBytes - gs::PacketReserve );
	const std::vector<std::string> whole = gs::Reply( Q( S( "|basic|" ) ), exact, 13 );
	assert( whole.size() == 2 && Joined( whole, 13 ) == exact.basic + S( "|final|" ) );
}

int main() {
	Parsing();
	Pairs();
	MohaaAnswers();
	Replies();
	Splitting();
	std::puts( "gamespy query checks passed" );
	return 0;
}
'''

SINGLE_PLAYER = r'''
#include "code/compat/wolfet_gameinfo.hpp"
#include <cassert>
#include <cstdio>
typedef int qboolean;
enum { qfalse, qtrue };
enum { GT_FFA, GT_TOURNAMENT, GT_SINGLE_PLAYER };
enum { SV_JKA_GT_SINGLE_PLAYER = 5, SV_JK2_GT_SINGLE_PLAYER = 4 };
/* 0 Quake III, 1 Enemy Territory, 2 Medal of Honor, 3 Jedi Academy MP,
 * 4 Jedi Outcast MP */
static int game;
static int Com_GameIsWolfET() { return game == 1; }
static int Com_GameIsMOHAA() { return game == 2; }
static int Com_GameIsJKAMP() { return game == 3; }
static int Com_GameIsJK2MP() { return game == 4; }
static fntech3::wolfet::gameinfo::GameInfo etInfo;
static const fntech3::wolfet::gameinfo::GameInfo &SV_WolfETGameInfo() { return etInfo; }
'''

SINGLE_PLAYER_CHECKS = r'''
int main() {
	/* Quake III's single player is 2 */
	game = 0;
	assert( SV_GameTypeIsSinglePlayer( 2 ) && !SV_GameTypeIsSinglePlayer( 0 ) && !SV_GameTypeIsSinglePlayer( 1 ) );
	/* Medal of Honor's is 0; 2 is its team match, 1 to 4 its multiplayer */
	game = 2;
	assert( SV_GameTypeIsSinglePlayer( 0 ) );
	for ( int type = 1; type <= 4; type++ ) {
		assert( !SV_GameTypeIsSinglePlayer( type ) );
	}
	/* Jedi Academy's is 5 and Jedi Outcast's 4; 2 is Jedi Master in both */
	for ( game = 3; game <= 4; game++ ) {
		for ( int type = 0; type <= 9; type++ ) {
			assert( !SV_GameTypeIsSinglePlayer( type ) == ( type != ( game == 3 ? 5 : 4 ) ) );
		}
	}
	std::puts( "single-player rule checks passed" );
	return 0;
}
'''


class MohaaGameSpyQueryTests(unittest.TestCase):
    def test_queries_and_their_answers_follow_the_protocol(self):
        output = build_and_run(self, 'gamespy_query', [PROTOCOL], cpp=True)
        self.assertIn('gamespy query checks passed', output)

    def test_medal_of_honors_single_player_is_game_type_zero(self):
        main = read('code/server/sv_main.cpp')
        rule = function(main, 'qboolean SV_GameTypeIsSinglePlayer( int gameType ) {')
        output = build_and_run(self, 'gamespy_single_player', [SINGLE_PLAYER + rule + SINGLE_PLAYER_CHECKS], cpp=True)
        self.assertIn('single-player rule checks passed', output)

    def test_only_a_medal_of_honor_multiplayer_server_opens_the_port(self):
        glue = read('code/server/server_mohaa_gamespy.hpp')
        frame = function(glue, 'void SV_MOHAAGameSpyFrame( void ) {')
        # The profile gate comes before anything else, cvars included.
        self.assertLess(frame.index('if ( !Com_GameIsMOHAA() ) {\n\t\treturn;'), frame.index('Cvar_Get'))
        self.assertIn('sv_gamespy->integer && !SV_GameTypeIsSinglePlayer( Cvar_VariableIntegerValue( "g_gametype" ) )',
                      frame)
        self.assertIn('NET_OpenQuerySocket( first, fntech3::gamespy::mohaa::QueryPortsTried, SV_MOHAAGameSpyPacket )',
                      frame)
        packet = function(glue, 'static void SV_MOHAAGameSpyPacket(')
        self.assertIn('!Com_GameIsMOHAA() || !com_sv_running || !com_sv_running->integer || sv.state != SS_GAME',
                      packet)
        # Rate limits come before any answer is built.
        self.assertLess(packet.index('SVC_RateLimitAddress( from, 10, 1000 )'), packet.index('fntech3::gamespy::Reply'))
        self.assertLess(packet.index('SVC_RateLimit( &svMOHAAGameSpy.outbound, 10, 100 )'),
                        packet.index('fntech3::gamespy::Reply'))
        self.assertIn('NET_SendQueryPacket(', packet)
        # Nothing is told to a master.
        self.assertNotIn('heartbeat', glue.lower())
        self.assertNotIn('NET_SendPacket', glue)

        main = read('code/server/sv_main.cpp')
        self.assertIn('#include "server_mohaa_gamespy.hpp"', main)
        frame = function(main, 'void SV_Frame( int msec, int realMsec ) {')
        self.assertLess(frame.index('if ( !com_sv_running->integer )'), frame.index('SV_MOHAAGameSpyFrame();'))
        self.assertLess(frame.index('SV_MOHAAGameSpyFrame();'), frame.index('SV_CheckPaused()'))
        init = read('code/server/sv_init.cpp')
        shutdown = function(init, 'void SV_Shutdown( const char *finalmsg ) {')
        self.assertLess(shutdown.index('SV_MOHAAGameSpyStop();'), shutdown.index('SV_ShutdownGameProgs();'))

    def test_the_query_socket_is_read_and_closed_with_the_network(self):
        net = read('code/qcommon/net_ip.c')
        sleep = function(net, 'qboolean NET_Sleep( int timeout )')
        self.assertIn('FD_SET( query_socket, &fdr );', sleep)
        self.assertLess(sleep.index('NET_Event( &fdr );'), sleep.index('NET_QueryEvent( &fdr );'))
        config = function(net, 'static void NET_Config( qboolean enableNetworking ) {')
        stop = config[config.index('if( stop ) {'):]
        self.assertLess(stop.index('NET_CloseQuerySocket();'), stop.index('closesocket( ip_socket );'))
        opener = function(net, 'int NET_OpenQuerySocket( int firstPort, int count, netQueryHandler_t handler ) {')
        # IPv4 only, never through a SOCKS relay, on net_ip's interface.
        for condition in ('!( net_enabled->integer & NET_ENABLEV4 )', 'ip_socket == INVALID_SOCKET', 'usingSocks'):
            self.assertIn(condition, opener)
        socket = function(net, 'static SOCKET NET_QueryIPSocket( int port ) {')
        self.assertIn('Sys_StringToSockaddr( net_ip->string', socket)
        event = function(net, 'static void NET_QueryEvent( const fd_set *fdr ) {')
        self.assertIn('i < 64', event)
        self.assertIn('byte		data[ MAX_PACKETLEN ];', event)

    def test_the_kills_and_deaths_reach_the_answer(self):
        data = read('code/server/server_game_data.cpp')
        self.assertIn('destination.persistant[8] = external.stats[fntech3::mohaa::kStatDeaths];', data)
        abi = read('code/compat/mohaa_game_abi.h')
        self.assertIn('constexpr int kStatKills = 21;\nconstexpr int kStatDeaths = 22;', abi)
        glue = read('code/server/server_mohaa_gamespy.hpp')
        self.assertIn('player.kills = ps.persistant[ PERS_SCORE ];', glue)
        self.assertIn('player.deaths = ps.persistant[ PERS_KILLED ];', glue)

    def test_the_start_server_menus_gamespy_box_sets_sv_gamespy(self):
        ui = read('code/client/cl_mohaa_ui.cpp')
        start = function(ui, 'void StartDMMap_f() {')
        self.assertIn('const int gameSpy = ClampedCvar( "ui_gamespy", 0, 1 );', start)
        self.assertIn('set sv_gamespy %i;', start)
        self.assertLess(start.index('set sv_gamespy %i;'), start.index('map \\"%s\\"'))


if __name__ == '__main__':
    unittest.main()
