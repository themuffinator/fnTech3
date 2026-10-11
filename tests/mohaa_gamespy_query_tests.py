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
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from compiler_support import compiler_target_args
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

/* ---- a browser's side ---- */

static gs::ReplyPacket R( const std::string &text ) {
	const std::optional<gs::ReplyPacket> packet = gs::ParseReplyPacket( text.data(), text.size() );
	assert( packet );
	return *packet;
}

static void ReplyParsing() {
	gs::ReplyPacket p = R( S( "|gamename|mohaa|gamever|1.00|final||queryid|3.1" ) );
	assert( p.final && p.pairs.size() == 2 );
	assert( p.pairs[0].first == "gamename" && p.pairs[0].second == "mohaa" );
	assert( p.pairs[1].first == "gamever" && p.pairs[1].second == "1.00" );
	/* a packet before the last: no \final\, its queryid left out */
	p = R( S( "|player_0|Alpha|frags_0|3|queryid|3.1" ) );
	assert( !p.final && p.pairs.size() == 2 && p.pairs[1].second == "3" );
	/* empty values, a key with nothing after it, a trailing backslash */
	p = R( S( "|hostname||hostport|12203" ) );
	assert( p.pairs.size() == 2 && p.pairs[0].second.empty() && p.pairs[1].second == "12203" );
	p = R( S( "|a|1|b" ) );
	assert( p.pairs.size() == 2 && p.pairs[1].first == "b" && p.pairs[1].second.empty() );
	p = R( S( "|a|1|" ) );
	assert( p.pairs.size() == 1 && !p.final );
	p = R( S( "|final|" ) );
	assert( p.final && p.pairs.empty() );
	/* text ends at its first NUL */
	const char nul[] = "\\a\\1\0\\final\\";
	const std::optional<gs::ReplyPacket> cut = gs::ParseReplyPacket( nul, sizeof( nul ) - 1 );
	assert( cut && cut->pairs.size() == 1 && !cut->final );
	/* not an answer: null, empty, not starting with a backslash, too long */
	assert( !gs::ParseReplyPacket( nullptr, 4 ) );
	assert( !gs::ParseReplyPacket( "", 0 ) );
	const std::string engine = "\xff\xff\xff\xff" "infoResponse";
	assert( !gs::ParseReplyPacket( engine.data(), engine.size() ) );
	std::string longest = S( "|k|" ) + std::string( gs::MaxReplyBytes - 3, 'v' );
	assert( gs::ParseReplyPacket( longest.data(), longest.size() ) );
	longest.push_back( 'v' );
	assert( !gs::ParseReplyPacket( longest.data(), longest.size() ) );
}

static const gs::LanAnswers::Answer *Add( gs::LanAnswers &answers, const std::string &source, const std::string &text ) {
	return answers.Add( source, text.data(), text.size() );
}

static void Gathering() {
	gs::LanAnswers answers;
	/* an answer is whole at its \final\, gathered over its packets */
	assert( !Add( answers, "a", S( "|gamename|mohaa|gamever|1.00|hostport|12203|player_0|Alpha|frags_0|3|queryid|9.1" ) ) );
	const gs::LanAnswers::Answer *answer = Add( answers, "a", S( "|player_1|Bravo|deaths_1|2|final||queryid|9.2" ) );
	assert( answer && answer->size() == 3 && answer->at( "hostport" ) == "12203" && answer->at( "gamever" ) == "1.00" );
	/* a player's keys are not kept */
	assert( !answer->count( "player_0" ) && !answer->count( "frags_0" ) && !answer->count( "deaths_1" ) );
	assert( gs::LanAnswers::PlayerKey( "player_12" ) && gs::LanAnswers::PlayerKey( "ping_0" ) );
	assert( !gs::LanAnswers::PlayerKey( "gametype_i" ) && !gs::LanAnswers::PlayerKey( "player_" ) );
	assert( !gs::LanAnswers::PlayerKey( "hostname" ) && !gs::LanAnswers::PlayerKey( "k7" ) );
	/* sources stay apart, and a later value replaces an earlier one */
	answer = Add( answers, "b", S( "|gamename|mohaa|hostport|12204|final|" ) );
	assert( answer && answer->at( "hostport" ) == "12204" && !answer->count( "gamever" ) );
	answer = Add( answers, "a", S( "|hostport|12205|final|" ) );
	assert( answer && answer->at( "hostport" ) == "12205" && answer->at( "gamever" ) == "1.00" );
	/* what is not an answer changes nothing */
	assert( !Add( answers, "a", "hello" ) );
	assert( !Add( answers, "c", "" ) );
	answers.Clear();
	answer = Add( answers, "a", S( "|final|" ) );
	assert( answer && answer->empty() );
	/* at most 128 sources, and 64 keys from one */
	gs::LanAnswers bounded;
	for ( int i = 0; i < 128; i++ ) {
		assert( Add( bounded, std::to_string( i ), S( "|final|" ) ) );
	}
	assert( !Add( bounded, "one more", S( "|final|" ) ) );
	std::string many;
	for ( int i = 0; i < 100; i++ ) {
		many += S( "|k" ) + std::to_string( i ) + S( "|v" );
	}
	answer = Add( bounded, "5", many + S( "|final|" ) );
	assert( answer && answer->size() == gs::LanAnswers::MaxKeys && answer->count( "k0" ) && !answer->count( "k99" ) );
	/* a key it has already is still replaced */
	answer = Add( bounded, "5", S( "|k0|w|final|" ) );
	assert( answer && answer->size() == gs::LanAnswers::MaxKeys && answer->at( "k0" ) == "w" );
}

static gs::mohaa::LanVerdict J( const gs::LanAnswers::Answer &answer, gs::mohaa::LanServer &server ) {
	return gs::mohaa::JudgeLanAnswer( answer, 12203, server );
}

static void Judging() {
	using V = gs::mohaa::LanVerdict;
	const gs::LanAnswers::Answer full = { { "gamename", "mohaa" }, { "gamever", "1.00" }, { "hostname", "LAN game" },
		{ "hostport", "12210" }, { "mapname", "mohdm1" }, { "gametype", "Team-Match" }, { "gametype_i", "2" },
		{ "numplayers", "3" }, { "maxplayers", "16" } };
	gs::mohaa::LanServer server;
	assert( J( full, server ) == V::Listed );
	assert( server.hostPort == 12210 && server.hostName == "LAN game" && server.mapName == "mohdm1" );
	assert( server.gameType == 2 && server.players == 3 && server.maxPlayers == 16 && server.version == "1.00" );
	/* another game, by GameSpy's name for it, or no name */
	gs::LanAnswers::Answer other;
	for ( const char *name : { "mohaas", "mohaab", "quake3", "" } ) {
		other = full;
		other["gamename"] = name;
		other["gamever"] = "2.15";
		/* its version is read all the same, for the browser to say */
		assert( J( other, server ) == V::OtherGame && server.version == "2.15" );
	}
	other = full;
	other.erase( "gamename" );
	assert( J( other, server ) == V::OtherGame );
	other = full;
	other["gamename"] = "MOHAA";
	assert( J( other, server ) == V::Listed );
	/* a version within a tenth of 1.00, as OpenMoHAA's browser allows, and
	 * none at all, are 1.00's; a demo's is not */
	for ( const char *version : { "1.00", "1.0", "1.05", "0.95" } ) {
		other = full;
		other["gamever"] = version;
		assert( J( other, server ) == V::Listed && server.version == version );
	}
	for ( const char *version : { "1.11", "1.10", "1.12+0.82.0", "2.15", "2.40", "", "x" } ) {
		other = full;
		other["gamever"] = version;
		assert( J( other, server ) == V::OtherVersion );
	}
	other = full;
	other["gamever"] = "d1.00";
	assert( J( other, server ) == V::Demo );
	other = full;
	other.erase( "gamever" );
	assert( J( other, server ) == V::Listed && server.version == "1.00" );
	/* the game port: hostport, read as atoi reads it, or the default */
	other = full;
	other.erase( "hostport" );
	assert( J( other, server ) == V::Listed && server.hostPort == 12203 );
	other["hostport"] = "12215x";
	assert( J( other, server ) == V::Listed && server.hostPort == 12215 );
	for ( const char *port : { "0", "65536", "-1", "abc", "" } ) {
		other = full;
		other["hostport"] = port;
		assert( J( other, server ) == V::NoPort );
	}
	/* the game type: the number when it is one of 1.00's, else the name */
	other = full;
	other.erase( "gametype_i" );
	assert( J( other, server ) == V::Listed && server.gameType == 2 );
	other["gametype"] = "objective-match";
	assert( J( other, server ) == V::Listed && server.gameType == 4 );
	other["gametype"] = "Capture-The-Flag";
	assert( J( other, server ) == V::Listed && server.gameType == -1 );
	other = full;
	other["gametype_i"] = "9";
	assert( J( other, server ) == V::Listed && server.gameType == 2 );
	/* counts missing or below nought are nought; names missing are empty */
	other = full;
	other.erase( "numplayers" );
	other["maxplayers"] = "-4";
	other.erase( "hostname" );
	other.erase( "mapname" );
	assert( J( other, server ) == V::Listed && server.players == 0 && server.maxPlayers == 0 );
	assert( server.hostName.empty() && server.mapName.empty() );
	for ( V verdict : { V::Listed, V::OtherGame, V::Demo, V::OtherVersion, V::NoPort } ) {
		assert( std::strlen( gs::mohaa::LanVerdictName( verdict ) ) > 0 );
	}
}

static void RoundTrip() {
	/* the search's query is the SDK browser's eight bytes, over its ports, for
	 * its three seconds */
	assert( std::string( gs::StatusQuery ) == S( "|status|" ) && std::strlen( gs::StatusQuery ) == 8 );
	assert( gs::LanSearchMilliseconds == 3000 );
	assert( gs::mohaa::LanSearchFirstPort == 12300 && gs::mohaa::LanSearchLastPort == 12316 );
	/* fnTech3's own server, its 66 players' answer in several packets, is
	 * listed from them at its game port */
	gs::mohaa::Server sample = Sample();
	sample.hostPort = 12210;
	for ( int i = 0; i < 64; i++ ) {
		sample.players.push_back( { "Soldier_" + std::to_string( i ) + std::string( 20, 'n' ), i, 0, 50 } );
	}
	const std::vector<std::string> packets = gs::Reply( Q( gs::StatusQuery ), gs::mohaa::Answer( sample ), 21 );
	assert( packets.size() >= 3 );
	gs::LanAnswers answers;
	const gs::LanAnswers::Answer *answer = nullptr;
	for ( std::size_t i = 0; i < packets.size(); i++ ) {
		answer = Add( answers, "127.0.0.1:12300", packets[i] );
		assert( ( answer != nullptr ) == ( i + 1 == packets.size() ) );
	}
	gs::mohaa::LanServer server;
	assert( gs::mohaa::JudgeLanAnswer( *answer, 12203, server ) == gs::mohaa::LanVerdict::Listed );
	assert( server.hostPort == 12210 && server.hostName == "fnTech3 LAN" && server.mapName == "mohdm1" );
	assert( server.gameType == 1 && server.players == 66 && server.maxPlayers == 8 && server.version == "1.00" );
}

int main() {
	Parsing();
	Pairs();
	MohaaAnswers();
	Replies();
	Splitting();
	ReplyParsing();
	Gathering();
	Judging();
	RoundTrip();
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


SOCKETS = r'''
/* net_ip.c itself, its query and search sockets carrying datagrams over the
 * loopback address, with the little of the engine it calls stubbed here. */
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/net_query.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cvar_t *com_dedicated;
cvar_t *com_sv_running;
static int clockMs = 100000;
static int gamePackets;

int Sys_Milliseconds( void ) { return clockMs; }

static char *Copy( const char *text ) {
	const size_t size = strlen( text ) + 1;
	char *copy = (char *)malloc( size );
	memcpy( copy, text, size );
	return copy;
}

static cvar_t cvars[64];
static int cvarCount;

static cvar_t *FindCvar( const char *name ) {
	int i;
	for ( i = 0; i < cvarCount; i++ ) {
		if ( !strcmp( cvars[i].name, name ) ) {
			return &cvars[i];
		}
	}
	return NULL;
}

static void SetCvar( cvar_t *cv, const char *value ) {
	free( cv->string );
	cv->string = Copy( value );
	cv->integer = atoi( value );
	cv->value = (float)atof( value );
	cv->modified = qtrue;
}

cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	cvar_t *cv = FindCvar( name );
	if ( cv ) {
		return cv;
	}
	if ( cvarCount == 64 ) {
		puts( "too many cvars" );
		exit( 4 );
	}
	cv = &cvars[cvarCount++];
	cv->name = Copy( name );
	cv->flags = flags;
	/* IPv4 alone, on the loopback address, the game's port the system's */
	if ( !strcmp( name, "net_enabled" ) ) value = "1";
	else if ( !strcmp( name, "net_ip" ) ) value = "127.0.0.1";
	else if ( !strcmp( name, "net_port" ) ) value = "0";
	SetCvar( cv, value ? value : "" );
	return cv;
}

void Cvar_CheckRange( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type ) {
	(void)cv; (void)minVal; (void)maxVal; (void)type;
}
void Cvar_SetDescription( cvar_t *var, const char *description ) { (void)var; (void)description; }
void Cvar_SetIntegerValue( const char *name, int value ) {
	cvar_t *cv = FindCvar( name );
	char text[32];
	if ( cv ) {
		snprintf( text, sizeof( text ), "%d", value );
		SetCvar( cv, text );
	}
}
void Cmd_AddCommand( const char *name, xcommand_t function ) { (void)name; (void)function; }
void QDECL Com_Printf( const char *fmt, ... ) {
	va_list ap;
	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
}
void QDECL Com_DPrintf( const char *fmt, ... ) {
	va_list ap;
	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
}
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
	va_list ap;
	(void)level;
	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
	exit( 2 );
}
int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	va_list ap;
	int length;
	va_start( ap, fmt );
	length = vsnprintf( dest, (size_t)size, fmt, ap );
	va_end( ap );
	return length;
}
const char *QDECL va( const char *format, ... ) {
	static char buffers[4][1024];
	static int index;
	char *buffer = buffers[index++ & 3];
	va_list ap;
	va_start( ap, format );
	vsnprintf( buffer, sizeof( buffers[0] ), format, ap );
	va_end( ap );
	return buffer;
}
void Q_strncpyz( char *dest, const char *src, int destsize ) {
	strncpy( dest, src, (size_t)destsize - 1 );
	dest[destsize - 1] = '\0';
}
int Q_CountChar( const char *string, char tocount ) {
	int count = 0;
	for ( ; *string; string++ ) {
		count += *string == tocount;
	}
	return count;
}
void MSG_Init( msg_t *buf, byte *data, int length ) {
	memset( buf, 0, sizeof( *buf ) );
	buf->data = data;
	buf->maxsize = length;
}
int Com_GameDefaultServerPort( void ) { return 27960; }
void Com_RunAndTimeServerPacket( const netadr_t *from, msg_t *msg ) { (void)from; (void)msg; gamePackets++; }
void CL_PacketEvent( const netadr_t *from, msg_t *msg ) { (void)from; (void)msg; gamePackets++; }

/* ---- the checks ---- */

#define CHECK( x ) do { if ( !( x ) ) { printf( "check failed, line %d: %s\n", __LINE__, #x ); exit( 1 ); } } while ( 0 )

static const char kAnswer[] = "\\gamename\\mohaa\\final\\\\queryid\\1.1";
static int queryDatagrams, searchDatagrams, lastQueryPort, lastAnswerPort;
static char lastAnswer[MAX_PACKETLEN + 1];

static int PortOf( const netadr_t *adr ) {
	const byte *b = (const byte *)&adr->port;
	return b[0] << 8 | b[1];
}

static netadr_t Loopback( int port ) {
	netadr_t adr;
	byte *b;
	memset( &adr, 0, sizeof( adr ) );
	CHECK( Sys_StringToAdr( "127.0.0.1", &adr, NA_IP ) );
	b = (byte *)&adr.port;
	b[0] = (byte)( port >> 8 );
	b[1] = (byte)( port & 0xff );
	return adr;
}

/* a server's side: `\status\` is answered from the query socket */
static void QueryHandler( const netadr_t *from, const byte *data, int length ) {
	queryDatagrams++;
	lastQueryPort = PortOf( from );
	if ( length == 8 && !memcmp( data, "\\status\\", 8 ) ) {
		NET_SendQueryPacket( (int)sizeof( kAnswer ) - 1, kAnswer, from );
	}
}

static void SearchHandler( const netadr_t *from, const byte *data, int length ) {
	searchDatagrams++;
	lastAnswerPort = PortOf( from );
	if ( length > MAX_PACKETLEN ) {
		length = MAX_PACKETLEN;
	}
	memcpy( lastAnswer, data, (size_t)length );
	lastAnswer[length] = '\0';
}

/* Waits on the sockets as frames do, until `counter` reaches `wanted` or
 * two seconds pass. */
static void Pump( const int *counter, int wanted ) {
	int i;
	for ( i = 0; i < 200 && *counter < wanted; i++ ) {
		NET_Sleep( 10 * 1000 );
	}
}

int main( void ) {
	int queryPort, searchPort, i, before;
	netadr_t to;
	cvar_t *enabled;

	com_dedicated = Cvar_Get( "dedicated", "1", 0 );
	com_sv_running = Cvar_Get( "sv_running", "0", 0 );
	NET_Init();

	/* nothing opens without a handler, a run of ports or a lifetime */
	CHECK( !NET_OpenQuerySocket( 47300, 100, NULL ) && !NET_OpenQuerySocket( 47300, 0, QueryHandler ) );
	CHECK( !NET_OpenSearchSocket( 3000, NULL ) && !NET_OpenSearchSocket( 0, SearchHandler ) );

	queryPort = NET_OpenQuerySocket( 47300, 100, QueryHandler );
	CHECK( queryPort >= 47300 && queryPort < 47400 && NET_QuerySocketPort() == queryPort );
	searchPort = NET_OpenSearchSocket( 3000, SearchHandler );
	CHECK( searchPort > 0 && searchPort != queryPort );

	/* the search's query reaches the query socket from the search's port, and
	 * the answer comes back from the query port; the game's socket sees none */
	to = Loopback( queryPort );
	NET_SendSearchPacket( 8, "\\status\\", &to );
	Pump( &searchDatagrams, 1 );
	CHECK( queryDatagrams == 1 && lastQueryPort == searchPort );
	CHECK( searchDatagrams == 1 && lastAnswerPort == queryPort && !strcmp( lastAnswer, kAnswer ) );
	CHECK( gamePackets == 0 );

	/* a broadcast to the query port: told, not checked, as whether one from
	 * the loopback address reaches it is the system's to say */
	to.type = NA_BROADCAST;
	NET_SendSearchPacket( 8, "\\status\\", &to );
	Pump( &searchDatagrams, 2 );
	for ( i = 0; i < 20; i++ ) {
		NET_Sleep( 10 * 1000 );
	}
	printf( "loopback broadcast answered %d time(s)\n", searchDatagrams - 1 );

	/* at most 64 datagrams a wake, the rest at the next */
	before = queryDatagrams;
	to = Loopback( queryPort );
	for ( i = 0; i < 100; i++ ) {
		NET_SendSearchPacket( 4, "ping", &to );
	}
	NET_Sleep( 100 * 1000 );
	CHECK( queryDatagrams - before == 64 );
	Pump( &queryDatagrams, before + 100 );
	CHECK( queryDatagrams - before == 100 );

	/* the search ends by itself at its lifetime: open a moment before, shut
	 * at it, and then neither reads nor sends */
	to = Loopback( searchPort );
	clockMs += 2999;
	before = searchDatagrams;
	NET_SendQueryPacket( 5, "early", &to );
	Pump( &searchDatagrams, before + 1 );
	CHECK( searchDatagrams == before + 1 && !strcmp( lastAnswer, "early" ) );
	clockMs += 1;
	NET_Sleep( 0 );
	NET_SendQueryPacket( 4, "late", &to );
	for ( i = 0; i < 20; i++ ) {
		NET_Sleep( 10 * 1000 );
	}
	CHECK( searchDatagrams == before + 1 );
	before = queryDatagrams;
	to = Loopback( queryPort );
	NET_SendSearchPacket( 8, "\\status\\", &to );
	for ( i = 0; i < 20; i++ ) {
		NET_Sleep( 10 * 1000 );
	}
	CHECK( queryDatagrams == before );

	/* another search opens; without IPv4 none does, and the open one closes */
	searchPort = NET_OpenSearchSocket( 3000, SearchHandler );
	CHECK( searchPort > 0 );
	enabled = Cvar_Get( "net_enabled", "", 0 );
	enabled->integer = 2;
	CHECK( !NET_OpenSearchSocket( 3000, SearchHandler ) );
	enabled->integer = 1;
	before = queryDatagrams;
	NET_SendSearchPacket( 8, "\\status\\", &to );
	for ( i = 0; i < 20; i++ ) {
		NET_Sleep( 10 * 1000 );
	}
	CHECK( queryDatagrams == before );
	searchPort = NET_OpenSearchSocket( 3000, SearchHandler );
	CHECK( searchPort > 0 );

	/* the network's end closes both */
	NET_Shutdown();
	CHECK( NET_QuerySocketPort() == 0 );
	puts( "net query socket checks passed" );
	return 0;
}
'''


def build_net_and_run(test, name, fixture):
    """The fixture with net_ip.c, built for the engine's 32-bit target where
    the toolchain gives one, and run."""
    compiler = shutil.which('clang') or (shutil.which('gcc') if os.name != 'nt' else None)
    if not compiler:
        test.skipTest('no C compiler')
    target = compiler_target_args(compiler)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else []
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as directory:
        source = Path(directory) / f'{name}.c'
        source.write_text(fixture, encoding='utf-8')
        exe = Path(directory) / f'{name}.exe'
        command = [compiler, *target, *abi, '-std=gnu11', '-I', str(ROOT), str(source),
                   str(ROOT / 'code' / 'qcommon' / 'net_ip.c'), '-o', str(exe)]
        command += ['-lws2_32'] if os.name == 'nt' else ['-lm']
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        run = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=60, **options)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return run.stdout


class MohaaGameSpyQueryTests(unittest.TestCase):
    def test_the_query_and_search_sockets_carry_datagrams(self):
        output = build_net_and_run(self, 'net_query_sockets', SOCKETS)
        self.assertIn('net query socket checks passed', output)

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
        reader = function(net, 'static void NET_ReadQuerySocket( const SOCKET *sock, const netQueryHandler_t *handler, '
                               'const fd_set *fdr ) {')
        self.assertIn('i < 64', reader)
        self.assertIn('byte		data[ MAX_PACKETLEN ];', reader)
        event = function(net, 'static void NET_QueryEvent( const fd_set *fdr ) {')
        self.assertIn('NET_ReadQuerySocket( &query_socket, &query_handler, fdr );', event)
        self.assertIn('NET_ReadQuerySocket( &search_socket, &search_handler, fdr );', event)

    def test_the_search_socket_broadcasts_and_ends_by_itself(self):
        net = read('code/qcommon/net_ip.c')
        opener = function(net, 'int NET_OpenSearchSocket( int lifetime, netQueryHandler_t handler ) {')
        # The query socket's conditions, a port the system picks, broadcasts
        # allowed, and an end.
        for condition in ('!( net_enabled->integer & NET_ENABLEV4 )', 'ip_socket == INVALID_SOCKET', 'usingSocks',
                          'lifetime <= 0', 'NET_QueryIPSocket( 0 )', 'SO_BROADCAST',
                          'search_ends = Sys_Milliseconds() + lifetime;'):
            self.assertIn(condition, opener)
        sender = function(net, 'void NET_SendSearchPacket( int length, const void *data, const netadr_t *to ) {')
        self.assertIn('to->type != NA_IP && to->type != NA_BROADCAST', sender)
        sleep = function(net, 'qboolean NET_Sleep( int timeout )')
        # the search closes once over, before the wait, else it is waited on
        self.assertLess(sleep.index('Sys_Milliseconds() - search_ends >= 0'), sleep.index('FD_SET( search_socket, &fdr );'))
        self.assertLess(sleep.index('FD_SET( search_socket, &fdr );'), sleep.index('select( highestfd + 1'))
        config = function(net, 'static void NET_Config( qboolean enableNetworking ) {')
        stop = config[config.index('if( stop ) {'):]
        self.assertLess(stop.index('NET_CloseSearchSocket();'), stop.index('closesocket( ip_socket );'))

    def test_only_medal_of_honors_lan_list_makes_gamespys_search(self):
        main = read('code/client/cl_main.cpp')
        local = function(main, 'static void CL_LocalServers_f( void ) {')
        self.assertIn('if ( Com_GameIsMOHAA() ) {\n\t\tCL_MOHAAGameSpyLanSearch();', local)
        # after the game ports' getinfo, which it adds to
        self.assertLess(local.index('NET_SendPacket( NS_CLIENT, n, message, &to );'),
                        local.index('CL_MOHAAGameSpyLanSearch();'))
        info = function(main, 'static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg ) {')
        duplicate = info[info.index('// avoid duplicate'):info.index('MAX_OTHER_SERVERS hit')]
        self.assertIn('if ( Com_GameIsMOHAA() && CL_MOHAAGameSpyTakeOver( i ) ) {\n'
                      '\t\t\t\tCL_SetServerInfo( &cls.localServers[i], infoString, ', duplicate)
        glue = read('code/client/cl_mohaa.cpp')
        answer = function(glue, 'void GameSpyLanAnswer( const netadr_t *from, const byte *data, int length ) {')
        self.assertIn('!Com_GameIsMOHAA() || cls.pingUpdateSource != AS_LOCAL', answer)
        self.assertLess(answer.index('mohaa::JudgeLanAnswer('), answer.index('cls.numlocalservers = i + 1;'))
        self.assertIn('if ( NET_CompareAdr( &address, &cls.localServers[i].adr ) ) {\n\t\t\treturn;', answer)
        search = function(glue, 'void CL_MOHAAGameSpyLanSearch( void ) {')
        self.assertIn('NET_OpenSearchSocket( gamespy::LanSearchMilliseconds, GameSpyLanAnswer )', search)
        self.assertIn('NET_SendSearchPacket( length, gamespy::StatusQuery, &to );', search)
        # Nothing goes to a master, or out of the game's own socket.
        section = glue[glue.index("GameSpy's LAN search\n"):]
        for call in ('NET_SendPacket', 'NET_OutOfBandPrint', 'heartbeat'):
            self.assertNotIn(call, section)

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
