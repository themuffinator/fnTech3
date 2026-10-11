/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_GAMESPY_QUERY_HPP
#define FNTECH3_COMPAT_GAMESPY_QUERY_HPP

/* GameSpy's first query protocol, which Medal of Honor: Allied Assault's
 * servers answer on a port of their own beside the game's, for GameSpy's
 * browsers, the later games' in-game lists and the third-party tools that
 * speak it.
 *
 * fnTech3's own reading of the protocol, written from reference observation
 * of OpenMoHAA (ab43a0de, code/gamespy): its sv_gqueryreporting.c and
 * gserverlist.c are GameSpy's own SDK code and no text of them is used here.
 * What they show **[O]**:
 *
 * - A query is one datagram of text, read up to 255 bytes, naming what it
 *   wants as backslash-framed words: `\status\`, or any of `\basic\`,
 *   `\info\`, `\rules\` and `\players\`, and `\echo\<text>`, which is sent
 *   back. A query whose first byte is ';' belongs to GameSpy's CD-key
 *   service. `\secure\<challenge>` asks a server to prove itself to a master
 *   with a key of GameSpy's, and `\packets\` was the SDK's demonstration of
 *   a reply in several packets.
 * - The reply is the same text, `\key\value` after `\key\value`, in
 *   datagrams of at most 1,400 bytes, each ending in `\queryid\<id>.<n>`:
 *   the server's count of queries and the packet's number from 1. The last
 *   packet carries `\final\` before that, and a browser takes a server's
 *   answer as whole when it sees `\final\`. Each part of an answer stays in
 *   one packet when it fits in one.
 * - Whatever the order the query names them in, the parts are answered
 *   basic, info, rules, players (all four for `\status\`), then the echo.
 *
 * This server answers the parts a browser lists servers by. It does not
 * answer the CD-key service, has no master's key to answer `\secure\` with
 * (so it proves nothing to a master, which none here asks of it), and gives
 * the demonstration query no answer of its own. A datagram naming nothing
 * it answers gets no reply at all, where the SDK sent a lone `\final\`.
 *
 * The browser's side, below the server's, reads such answers for fnTech3's
 * own Medal of Honor LAN list: a LAN search as the SDK's browser made it,
 * and each answer judged as OpenMoHAA's browser judges one. */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fntech3::gamespy {

/* The SDK read a query into 256 bytes, one kept for the end. */
constexpr std::size_t MaxQueryBytes = 255;
/* A reply datagram's limit, and the room kept in it for its queryid. */
constexpr std::size_t MaxPacketBytes = 1400;
constexpr std::size_t PacketReserve = 50;

struct Query {
	/* The CD-key service's: answered by none here. */
	bool cdkey = false;
	bool basic = false;
	bool info = false;
	bool rules = false;
	bool players = false;
	bool status = false;
	bool echo = false;
	std::string echoText;
	/* A master's challenge, which this server cannot answer. */
	bool secure = false;

	bool Answerable() const {
		return !cdkey && ( basic || info || rules || players || status || echo );
	}
};

/* The value after `\key\` in `text`, up to the next backslash or the end;
 * nothing when the key is not there framed by backslashes on both sides. */
inline std::optional<std::string> FramedValue( std::string_view text, std::string_view key ) {
	std::string framed;
	framed.reserve( key.size() + 2 );
	framed += '\\';
	framed.append( key.data(), key.size() );
	framed += '\\';
	const std::size_t at = text.find( framed );
	if ( at == std::string_view::npos ) {
		return std::nullopt;
	}
	const std::size_t start = at + framed.size();
	const std::size_t end = std::min( text.find( '\\', start ), text.size() );
	return std::string( text.substr( start, end - start ) );
}

/* A datagram as the server reads it. Longer than a query may be, or empty,
 * it names nothing; it ends at its first NUL, as the SDK read text. */
inline Query ParseQuery( const char *data, std::size_t length ) {
	Query query;
	if ( !data || length == 0 || length > MaxQueryBytes ) {
		return query;
	}
	const std::string_view text( data, static_cast<std::size_t>( std::find( data, data + length, '\0' ) - data ) );
	if ( text.empty() ) {
		return query;
	}
	if ( text[0] == ';' ) {
		query.cdkey = true;
		return query;
	}
	query.basic = FramedValue( text, "basic" ).has_value();
	query.info = FramedValue( text, "info" ).has_value();
	query.rules = FramedValue( text, "rules" ).has_value();
	query.players = FramedValue( text, "players" ).has_value();
	query.status = FramedValue( text, "status" ).has_value();
	query.secure = FramedValue( text, "secure" ).has_value();
	if ( std::optional<std::string> echo = FramedValue( text, "echo" ) ) {
		query.echo = true;
		query.echoText = std::move( *echo );
	}
	return query;
}

/* One `\key\value` pair onto an answer. A backslash in either would end it
 * early, so each becomes '_'. Unless `keepEmpty`, an empty value leaves the
 * key out, as the engines' info strings do. */
inline void AppendPair( std::string &out, std::string_view key, std::string_view value, bool keepEmpty = false ) {
	if ( key.empty() || ( value.empty() && !keepEmpty ) ) {
		return;
	}
	const auto append = [&out]( std::string_view text ) {
		out += '\\';
		for ( const char c : text ) {
			out += c == '\\' ? '_' : c;
		}
	};
	append( key );
	append( value );
}

/* The four parts of a server's answer, each a run of `\key\value` pairs. */
struct Answers {
	std::string basic;
	std::string info;
	std::string rules;
	std::string players;
};

/* Packs one query's reply into datagrams. */
class ReplyPackets {
public:
	explicit ReplyPackets( int queryId ) : queryId_( queryId ) {}

	/* A part of the answer: whole in the packet being filled when it fits,
	 * else whole in the next; longer than a packet, it is cut between its
	 * pairs, and a single pair longer than a packet is left out. */
	void Add( std::string_view part ) {
		if ( part.empty() ) {
			return;
		}
		if ( buffer_.size() + part.size() < Limit ) {
			buffer_.append( part.data(), part.size() );
			return;
		}
		if ( part.size() <= Limit ) {
			Flush();
			buffer_.assign( part.data(), part.size() );
			return;
		}
		std::size_t start = 0;
		while ( start < part.size() ) {
			const std::size_t end = PairEnd( part, start );
			const std::string_view pair = part.substr( start, end - start );
			start = end;
			if ( pair.size() > Limit ) {
				continue;
			}
			if ( buffer_.size() + pair.size() >= Limit ) {
				Flush();
			}
			buffer_.append( pair.data(), pair.size() );
		}
	}

	/* The datagrams, the last ending in `\final\`. */
	std::vector<std::string> Finish() {
		Add( "\\final\\" );
		Flush();
		return std::move( packets_ );
	}

private:
	static constexpr std::size_t Limit = MaxPacketBytes - PacketReserve;

	/* Where the pair starting at `start` (a backslash) ends: at the third
	 * backslash from it, or the end. */
	static std::size_t PairEnd( std::string_view text, std::size_t start ) {
		std::size_t at = start;
		for ( int slashes = 0; at < text.size(); at++ ) {
			if ( text[at] == '\\' && ++slashes == 3 ) {
				break;
			}
		}
		return at;
	}

	void Flush() {
		if ( buffer_.empty() ) {
			return;
		}
		buffer_ += "\\queryid\\" + std::to_string( queryId_ ) + "." + std::to_string( ++packetNumber_ );
		packets_.push_back( std::move( buffer_ ) );
		buffer_.clear();
	}

	int queryId_;
	int packetNumber_ = 0;
	std::string buffer_;
	std::vector<std::string> packets_;
};

/* The datagrams answering `query`, numbered `queryId`; none when it names
 * nothing this server answers. */
inline std::vector<std::string> Reply( const Query &query, const Answers &answers, int queryId ) {
	if ( !query.Answerable() ) {
		return {};
	}
	ReplyPackets packets( queryId );
	if ( query.basic ) packets.Add( answers.basic );
	if ( query.info ) packets.Add( answers.info );
	if ( query.rules ) packets.Add( answers.rules );
	if ( query.players ) packets.Add( answers.players );
	if ( query.status ) {
		packets.Add( answers.basic );
		packets.Add( answers.info );
		packets.Add( answers.rules );
		packets.Add( answers.players );
	}
	if ( query.echo ) {
		std::string echo;
		AppendPair( echo, "echo", query.echoText, true );
		packets.Add( echo );
	}
	return packets.Finish();
}

/* ---- a browser's side --------------------------------------------------
 *
 * GameSpy's LAN search, as the SDK's browser made it (gserverlist.c,
 * reference only **[O]**): from a socket of its own, allowed to broadcast,
 * the eight bytes `\status\` once to each port of a run; every source whose
 * answer holds `\final\` is a server; three seconds on, the search is over
 * and its socket shut. A browser then lists each server from its answer's
 * pairs. */

constexpr const char *StatusQuery = "\\status\\";
constexpr int LanSearchMilliseconds = 3000;
/* The SDK's browser read a datagram into 1,500 bytes, one kept for the end. */
constexpr std::size_t MaxReplyBytes = 1499;

/* One datagram of an answer, as a browser reads it: its pairs in order, the
 * `\final\` that ends an answer noted, the `\queryid\` that numbers it left
 * out. */
struct ReplyPacket {
	std::vector<std::pair<std::string, std::string>> pairs;
	bool final = false;
};

/* Text up to its first NUL, `\key\value` after `\key\value`; a key with
 * nothing after it has an empty value. Empty, longer than a browser reads,
 * or not starting with a backslash, it is no answer. */
inline std::optional<ReplyPacket> ParseReplyPacket( const char *data, std::size_t length ) {
	if ( !data || length == 0 || length > MaxReplyBytes ) {
		return std::nullopt;
	}
	const std::string_view text( data, static_cast<std::size_t>( std::find( data, data + length, '\0' ) - data ) );
	if ( text.empty() || text[0] != '\\' ) {
		return std::nullopt;
	}
	ReplyPacket packet;
	std::size_t at = 1;
	while ( at <= text.size() ) {
		const std::size_t keyEnd = std::min( text.find( '\\', at ), text.size() );
		const std::string_view key = text.substr( at, keyEnd - at );
		std::string_view value;
		at = keyEnd + 1;
		if ( keyEnd < text.size() ) {
			const std::size_t valueEnd = std::min( text.find( '\\', at ), text.size() );
			value = text.substr( at, valueEnd - at );
			at = valueEnd + 1;
		}
		if ( key == "final" ) {
			packet.final = true;
		} else if ( !key.empty() && key != "queryid" ) {
			packet.pairs.emplace_back( std::string( key ), std::string( value ) );
		}
	}
	return packet;
}

/* A LAN search's answers, each source's gathered over its datagrams until one
 * holds `\final\`, as the SDK's browser took a server only then. A player's
 * keys, numbered after their last underscore (`player_3`, `frags_3`), are
 * not kept, as no list here shows them; nor more than `MaxSources` sources
 * or `MaxKeys` keys from one. */
class LanAnswers {
public:
	using Answer = std::map<std::string, std::string>;
	static constexpr std::size_t MaxSources = 128;
	static constexpr std::size_t MaxKeys = 64;

	void Clear() {
		answers_.clear();
	}

	/* A datagram from `source`: that source's whole answer when this
	 * datagram ends it, else null. A later value of a key replaces an
	 * earlier one. */
	const Answer *Add( const std::string &source, const char *data, std::size_t length ) {
		const std::optional<ReplyPacket> packet = ParseReplyPacket( data, length );
		if ( !packet ) {
			return nullptr;
		}
		auto found = answers_.find( source );
		if ( found == answers_.end() ) {
			if ( answers_.size() >= MaxSources ) {
				return nullptr;
			}
			found = answers_.emplace( source, Answer() ).first;
		}
		Answer &answer = found->second;
		for ( const auto &pair : packet->pairs ) {
			if ( PlayerKey( pair.first ) || ( answer.size() >= MaxKeys && !answer.count( pair.first ) ) ) {
				continue;
			}
			answer[pair.first] = pair.second;
		}
		return packet->final ? &answer : nullptr;
	}

	static bool PlayerKey( std::string_view key ) {
		const std::size_t underscore = key.rfind( '_' );
		if ( underscore == std::string_view::npos || underscore + 1 == key.size() ) {
			return false;
		}
		for ( std::size_t i = underscore + 1; i < key.size(); i++ ) {
			if ( key[i] < '0' || key[i] > '9' ) {
				return false;
			}
		}
		return true;
	}

private:
	std::map<std::string, Answer> answers_;
};

/* Medal of Honor: Allied Assault's answers. The keys are those OpenMoHAA's
 * server reports (sv_gamespy.c's basic, info, rules and players callbacks)
 * **[O]**, less the ones it reports for the later games or added itself:
 * `sprinton` (sv_sprinton) and `realism` (g_realismmode), cvars 1.00's game
 * module never registers; `allowlean`, OpenMoHAA's dmflags bit 18, whose
 * meaning in 1.00 is not established; and `minplayers` and `botskill`, its
 * own additions for its bots. */
namespace mohaa {

constexpr const char *GameName = "mohaa";
/* The version a 1.00 server reports. OpenMoHAA's reports its target game's,
 * "1.12", then "+" and its own; that 1.00's reported "1.00" is **[I]**. A
 * browser compares the number with its own version. */
constexpr const char *GameVersion = "1.00";
/* GameSpy's state for a server in play; OpenMoHAA reports "exiting" only to
 * the masters as it stops. */
constexpr const char *GameMode = "openplaying";
/* The query port: the first free one from 12300, of the hundred the SDK
 * tried; the LAN search covers 12300 to 12316. */
constexpr int DefaultQueryPort = 12300;
constexpr int QueryPortsTried = 100;
constexpr int LanSearchFirstPort = 12300;
constexpr int LanSearchLastPort = 12316;

struct Player {
	std::string name;
	int kills = 0;
	int deaths = 0;
	int ping = 0;
};

struct Server {
	int location = 1;
	std::string hostName;
	int hostPort = 0;
	std::string mapName;
	/* g_gametypestring and g_gametype. */
	std::string gameTypeName;
	int gameType = 0;
	int maxPlayers = 0;
	bool dedicated = false;
	bool pure = false;
	/* The rules' cvars as text: timelimit, fraglimit, g_rankedserver. */
	std::string timeLimit;
	std::string fragLimit;
	std::string rankedServer;
	/* Everyone in a slot, numbered in the reply from 0 as listed: OpenMoHAA
	 * numbers them so, where it reads 2015's engine as numbering them by
	 * slot, which leaves gaps a browser stops at. */
	std::vector<Player> players;
};

/* A browser shows a map by the name after its folder: "dm/mohdm1" is
 * "mohdm1" (OpenMoHAA ConvertMapFilename, everything after the first '/'). */
inline std::string BrowserMapName( std::string_view map ) {
	const std::size_t slash = map.find( '/' );
	return std::string( slash == std::string_view::npos ? map : map.substr( slash + 1 ) );
}

inline Answers Answer( const Server &server ) {
	Answers answers;
	AppendPair( answers.basic, "gamename", GameName );
	AppendPair( answers.basic, "gamever", GameVersion );
	AppendPair( answers.basic, "location", std::to_string( server.location ) );

	AppendPair( answers.info, "hostname", server.hostName );
	AppendPair( answers.info, "hostport", std::to_string( server.hostPort ) );
	AppendPair( answers.info, "mapname", BrowserMapName( server.mapName ) );
	AppendPair( answers.info, "gametype", server.gameTypeName );
	AppendPair( answers.info, "numplayers", std::to_string( server.players.size() ) );
	AppendPair( answers.info, "maxplayers", std::to_string( server.maxPlayers ) );
	AppendPair( answers.info, "gamemode", GameMode );
	AppendPair( answers.info, "gametype_i", std::to_string( server.gameType ) );
	AppendPair( answers.info, "dedicated", server.dedicated ? "1" : "0" );
	AppendPair( answers.info, "pure", server.pure ? "1" : "0" );

	AppendPair( answers.rules, "timelimit", server.timeLimit );
	AppendPair( answers.rules, "fraglimit", server.fragLimit );
	AppendPair( answers.rules, "rankedserver", server.rankedServer );

	for ( std::size_t i = 0; i < server.players.size(); i++ ) {
		const Player &player = server.players[i];
		const std::string index = std::to_string( i );
		AppendPair( answers.players, "player_" + index, player.name, true );
		AppendPair( answers.players, "frags_" + index, std::to_string( player.kills ) );
		AppendPair( answers.players, "deaths_" + index, std::to_string( player.deaths ) );
		AppendPair( answers.players, "ping_" + index, std::to_string( player.ping ) );
	}
	return answers;
}

/* ---- a 1.00 browser's LAN list -------------------------------------------- */

/* A server as a LAN list shows it, from its GameSpy answer. */
struct LanServer {
	/* The game port, which a player joins. */
	int hostPort = 0;
	std::string hostName;
	std::string mapName;
	/* g_gametype; -1 when the answer names none 1.00 knows. */
	int gameType = -1;
	int players = 0;
	int maxPlayers = 0;
	std::string version;
};

enum class LanVerdict {
	Listed,
	/* GameSpy names another game: Spearhead's servers answer `mohaas` and
	 * Breakthrough's `mohaab` (OpenMoHAA sv_gamespy.c **[O]**). */
	OtherGame,
	/* A demo's server, its version starting with `d` **[O]**. */
	Demo,
	/* Another version than 1.00, which speaks another protocol. */
	OtherVersion,
	/* No game port to join it at. */
	NoPort,
};

inline const char *LanVerdictName( LanVerdict verdict ) {
	switch ( verdict ) {
	case LanVerdict::Listed: return "listed";
	case LanVerdict::OtherGame: return "another game";
	case LanVerdict::Demo: return "a demo's server";
	case LanVerdict::OtherVersion: return "another version";
	case LanVerdict::NoPort: return "no game port";
	}
	return "";
}

inline bool EqualsIgnoringCase( std::string_view a, std::string_view b ) {
	if ( a.size() != b.size() ) {
		return false;
	}
	for ( std::size_t i = 0; i < a.size(); i++ ) {
		const char x = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>( a[i] - 'A' + 'a' ) : a[i];
		const char y = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char>( b[i] - 'A' + 'a' ) : b[i];
		if ( x != y ) {
			return false;
		}
	}
	return true;
}

/* g_gametypestring's values, which 1.00's game module sets from g_gametype 0
 * to 4 (gamex86.dll 0x310e5a50); -1 for any other name. */
inline int GameTypeFromName( std::string_view name ) {
	static constexpr const char *names[] = { "Single-Player", "Free-For-All", "Team-Match", "Round-Based-Match",
		"Objective-Match" };
	for ( int i = 0; i < 5; i++ ) {
		if ( EqualsIgnoringCase( name, names[i] ) ) {
			return i;
		}
	}
	return -1;
}

/* A number as a browser reads one, its leading digits as atoi's; `fallback`
 * when there are none. */
inline int LeadingInt( const std::string &text, int fallback ) {
	const char *start = text.c_str();
	char *end = nullptr;
	const long value = std::strtol( start, &end, 10 );
	if ( end == start ) {
		return fallback;
	}
	return static_cast<int>( std::max( -2147483647L, std::min( 2147483647L, value ) ) );
}

/* What an answer says of a server, judged as OpenMoHAA's browser judges one
 * (cl_uiserverlist.cpp, cl_instantAction.cpp **[O]**): an answer naming no
 * version is taken for 1.00's, a version starting with `d` is a demo's, and
 * one more than a tenth from the browser's own is another version's; the
 * game port is `hostport`, or the game's default without one. A 1.00
 * browser lists only what it can join: the game GameSpy names `mohaa` (it
 * asks `\status\`, whose basic part names it), at 1.00 or within a tenth. */
inline LanVerdict JudgeLanAnswer( const LanAnswers::Answer &answer, int defaultPort, LanServer &server ) {
	const auto value = [&answer]( const char *key ) -> const std::string * {
		const auto found = answer.find( key );
		return found == answer.end() ? nullptr : &found->second;
	};
	const std::string *version = value( "gamever" );
	server.version = version ? *version : GameVersion;
	const std::string *gameName = value( "gamename" );
	if ( !gameName || !EqualsIgnoringCase( *gameName, GameName ) ) {
		return LanVerdict::OtherGame;
	}
	if ( !server.version.empty() && server.version[0] == 'd' ) {
		return LanVerdict::Demo;
	}
	if ( std::fabs( std::strtof( server.version.c_str(), nullptr ) - std::strtof( GameVersion, nullptr ) ) > 0.1f ) {
		return LanVerdict::OtherVersion;
	}
	const std::string *port = value( "hostport" );
	server.hostPort = port ? LeadingInt( *port, 0 ) : defaultPort;
	if ( server.hostPort < 1 || server.hostPort > 65535 ) {
		return LanVerdict::NoPort;
	}
	const std::string *hostName = value( "hostname" );
	const std::string *mapName = value( "mapname" );
	server.hostName = hostName ? *hostName : std::string();
	server.mapName = mapName ? *mapName : std::string();
	// the number where it is one of 1.00's, else the name g_gametypestring
	// gives it
	const std::string *typeNumber = value( "gametype_i" );
	server.gameType = typeNumber ? LeadingInt( *typeNumber, -1 ) : -1;
	if ( server.gameType < 0 || server.gameType > 4 ) {
		const std::string *typeName = value( "gametype" );
		server.gameType = typeName ? GameTypeFromName( *typeName ) : -1;
	}
	const std::string *players = value( "numplayers" );
	const std::string *maxPlayers = value( "maxplayers" );
	server.players = std::max( 0, players ? LeadingInt( *players, 0 ) : 0 );
	server.maxPlayers = std::max( 0, maxPlayers ? LeadingInt( *maxPlayers, 0 ) : 0 );
	return LanVerdict::Listed;
}

} // namespace mohaa

} // namespace fntech3::gamespy

#endif
