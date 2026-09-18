/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

// Decodes retail Elite Force Holomatch demos (protocol 24 .efdemo) with the
// engine's message layer, then re-encodes every player and entity delta and
// compares the bits with the recording. A delta that differs is classified:
// "hidden" when retail flagged extra fields whose values decode equal to the
// baseline (bits above a field's width, or -0.0 against 0.0, which the
// sender saw but no reader can), "escape" when a pre-release encoder sent a
// raw vector the dictionary holds, otherwise "other". Paths arrive one per
// line on stdin; one JSON object per demo is printed.
// scripts/probe_stvef_demos.py drives it.

extern "C" {
#include "q_shared.h"
#include "qcommon.h"
}

#include "../../code/compat/stvef_message_codec.hpp"
#include "../../code/compat/stvef_state_adapter.hpp"

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

extern "C" void QDECL Com_Printf( const char *, ... ) {
}

extern "C" void QDECL Com_Error( errorParm_t, const char *format, ... ) {
	va_list args;
	va_start( args, format );
	std::vfprintf( stderr, format, args );
	va_end( args );
	std::fputc( '\n', stderr );
	std::exit( 3 );
}

namespace {

constexpr netchanWireProfile_t kWire = NETCHAN_WIRE_STVEF_HM;
constexpr int kPacketBackup = 32;
constexpr int kMaxMessage = 16384;

struct Frame {
	bool valid = false;
	int messageNum = 0;
	playerState_t ps{};
	std::map<int, entityState_t> entities;
};

struct Result {
	int records = 0, gamestates = 0, snapshots = 0, serverCommands = 0;
	int configstrings = 0, baselines = 0, entityDeltas = 0;
	struct Mismatches { int hidden = 0, escape = 0, other = 0; };
	Mismatches player, entity, baseline;
	int invalidDeltaFrames = 0;
	std::array<int, 8> leftoverBits{};
	std::array<int, 32> vectorSymbols{};
	std::string version;
	std::string error;
};

bool SameBits( const byte *a, int aStart, const byte *b, int bStart, int count ) {
	for ( int i = 0; i < count; ++i ) {
		const int x = ( a[( aStart + i ) >> 3] >> ( ( aStart + i ) & 7 ) ) & 1;
		const int y = ( b[( bStart + i ) >> 3] >> ( ( bStart + i ) & 7 ) ) & 1;
		if ( x != y ) return false;
	}
	return true;
}

int BitsAt( const byte *data, int start, int count ) {
	int value = 0;
	for ( int i = 0; i < count; ++i )
		value |= ( ( data[( start + i ) >> 3] >> ( ( start + i ) & 7 ) ) & 1 ) << i;
	return value;
}

// Walks recorded bits without decoding values.
struct BitWalker {
	const byte *data;
	int bit;
	int Take( int count ) { const int value = BitsAt( data, bit, count ); bit += count; return value; }
	void SkipScalar( int width ) {
		if ( width ) { bit += width < 0 ? -width : width; return; }
		bit += Take( 1 ) ? 32 : fntech3::stvef::message::kFloatIntBits;
	}
};

using fntech3::stvef::message::ChangeVector;

int FirstSymbol( const ChangeVector &vector ) {
	const auto &table = fntech3::stvef::message::ChangeVectors;
	for ( std::size_t i = 0; i < table.size(); ++i ) if ( table[i] == vector ) return static_cast<int>( i );
	return static_cast<int>( fntech3::stvef::message::kRawVectorSymbol );
}

bool Subset( const ChangeVector &part, const ChangeVector &whole ) {
	for ( std::size_t i = 0; i < part.size(); ++i ) if ( part[i] & ~whole[i] ) return false;
	return true;
}

class Probe {
public:
	explicit Probe( Result &result ) : result_( result ) {}

	bool Record( int sequence, std::vector<byte> &data ) {
		msg_t msg{};
		MSG_Init( &msg, data.data(), static_cast<int>( data.size() ) );
		msg.cursize = static_cast<int>( data.size() );
		MSG_BeginReading( &msg );
		MSG_STVEFBitstream( &msg );
		// Messages have no leading acknowledgement and no EOF opcode.
		for ( ;; ) {
			const int before = msg.bit;
			const int command = MSG_ReadByte( &msg );
			if ( command == -1 ) {
				// Fewer than eight bits remain after the last command.
				const int leftover = msg.cursize * 8 - before;
				if ( leftover < 0 || leftover >= 8 ) return Fail( "unread data at end of message" );
				++result_.leftoverBits[leftover];
				break;
			}
			bool parsed = false;
			switch ( command ) {
			case 1: parsed = true; break;
			case 2: parsed = Gamestate( msg ); break;
			case 5: parsed = ServerCommand( msg ); break;
			case 7: parsed = Snapshot( msg, sequence ); break;
			default:
				return Fail( "illegible command " + std::to_string( command ) );
			}
			if ( !parsed ) return false;
			if ( msg.readcount > msg.cursize ) return Fail( "read past end" );
		}
		++result_.records;
		return true;
	}

private:
	bool Fail( const std::string &error ) {
		result_.error = error;
		return false;
	}

	bool Gamestate( msg_t &msg ) {
		++result_.gamestates;
		MSG_ReadLong( &msg );
		for ( ;; ) {
			const int command = MSG_ReadByte( &msg );
			if ( command <= 0 ) break;
			if ( command == 3 ) {
				const int index = MSG_ReadShort( &msg );
				if ( index < 0 || index >= MAX_CONFIGSTRINGS ) return Fail( "configstring index" );
				const std::string text = MSG_ReadBigStringForWireProfile( &msg, kWire );
				if ( index == 0 ) {
					const std::string key = "\\version\\";
					const auto at = text.find( key );
					if ( at != std::string::npos ) {
						const auto begin = at + key.size();
						result_.version = text.substr( begin, text.find( '\\', begin ) - begin );
					}
				}
				++result_.configstrings;
			} else if ( command == 4 ) {
				const int start = msg.bit;
				const int number = MSG_ReadEntitynum( &msg );
				if ( number < 0 || number >= MAX_GENTITIES ) return Fail( "baseline number" );
				entityState_t empty{};
				if ( !MSG_ReadDeltaEntityForWireProfile( &msg, &empty, &baselines_[number], number, kWire ) )
					return Fail( "baseline delta" );
				CountSymbol( msg, start );
				if ( !Reencode( msg, start, &empty, &baselines_[number], true ) )
					ClassifyEntity( msg, start, empty, baselines_[number], result_.baseline );
				++result_.baselines;
			} else {
				return Fail( "gamestate command " + std::to_string( command ) );
			}
		}
		// A live gamestate carries the checksum feed; the recorded one does not.
		if ( msg.cursize - msg.readcount >= 4 ) MSG_ReadLong( &msg );
		for ( auto &frame : frames_ ) frame.valid = false;
		return true;
	}

	bool ServerCommand( msg_t &msg ) {
		MSG_ReadLong( &msg );
		MSG_ReadStringForWireProfile( &msg, kWire );
		++result_.serverCommands;
		return msg.readcount <= msg.cursize;
	}

	void CountSymbol( const msg_t &msg, int start ) {
		// number:10, removed:1, changed:1, then the vector symbol.
		if ( start + 17 > msg.bit ) return;
		if ( BitsAt( msg.data, start + 10, 1 ) || !BitsAt( msg.data, start + 11, 1 ) ) return;
		++result_.vectorSymbols[BitsAt( msg.data, start + 12, 5 )];
	}

	bool Reencode( const msg_t &msg, int start, const entityState_t *from,
		const entityState_t *to, bool force ) {
		std::array<byte, kMaxMessage> scratch{};
		msg_t writer{};
		MSG_Init( &writer, scratch.data(), static_cast<int>( scratch.size() ) );
		MSG_STVEFBitstream( &writer );
		if ( !MSG_WriteDeltaEntityForWireProfile( &writer, from, to, force ? qtrue : qfalse, kWire ) )
			return false;
		return writer.bit == msg.bit - start &&
			SameBits( msg.data, start, scratch.data(), 0, writer.bit );
	}

	void Report( const char *kind, int start ) {
		if ( reported_++ < 8 ) std::fprintf( stderr, "%s mismatch at record %d bit %d\n", kind, result_.records, start );
	}

	void ClassifyEntity( const msg_t &msg, int start, const entityState_t &fromState,
		const entityState_t &toState, Result::Mismatches &count ) {
		using namespace fntech3::stvef;
		message::Entity from{}, to{};
		BitWalker walk{ msg.data, start + 10 };
		if ( !state::FromInternal( fromState, from ) || !state::FromInternal( toState, to ) ||
			walk.Take( 1 ) || !walk.Take( 1 ) ) {
			++count.other; Report( "entity", start ); return;
		}
		const int symbol = walk.Take( 5 );
		ChangeVector recorded{};
		if ( symbol == static_cast<int>( message::kRawVectorSymbol ) ) {
			for ( std::size_t i = 0; i < 6; ++i ) recorded[i] = static_cast<std::uint8_t>( walk.Take( 8 ) );
			recorded[6] = static_cast<std::uint8_t>( walk.Take( 2 ) );
		} else {
			recorded = message::ChangeVectors[symbol];
		}
		ChangeVector ours{};
		for ( std::size_t i = 0; i < message::EntityFields.size(); ++i ) {
			const auto offset = message::EntityFields[i].offset;
			if ( message::Word( from, offset ) != message::Word( to, offset ) )
				ours[i >> 3] |= static_cast<std::uint8_t>( 1u << ( i & 7 ) );
		}
		if ( symbol != FirstSymbol( recorded ) ) {
			if ( symbol == static_cast<int>( message::kRawVectorSymbol ) && recorded == ours ) ++count.escape;
			else { ++count.other; Report( "entity symbol", start ); }
		} else if ( recorded != ours && Subset( ours, recorded ) ) {
			++count.hidden;
		} else {
			++count.other; Report( "entity", start );
		}
	}

	void ClassifyPlayer( const msg_t &msg, int start, const playerState_t *fromState,
		const playerState_t &toState ) {
		using namespace fntech3::stvef;
		message::Player from{}, to{};
		if ( ( fromState && !state::FromInternal( *fromState, from ) ) || !state::FromInternal( toState, to ) ) {
			++result_.player.other; Report( "player", start ); return;
		}
		BitWalker walk{ msg.data, start };
		bool hidden = false;
		for ( const auto &field : message::PlayerFields ) {
			const bool differs = message::Word( from, field.offset ) != message::Word( to, field.offset );
			const bool flagged = walk.Take( 1 ) != 0;
			if ( differs && !flagged ) { ++result_.player.other; Report( "player field", start ); return; }
			if ( flagged && !differs ) hidden = true;
			if ( flagged ) walk.SkipScalar( field.bits );
		}
		const std::int32_t *arrays[4][2] = {
			{ from.stats, to.stats }, { from.persistant, to.persistant },
			{ from.ammo, to.ammo }, { from.powerups, to.powerups } };
		for ( int block = 0; block < 4; ++block ) {
			const std::uint32_t ours = message::ArrayChanges( arrays[block][0], arrays[block][1] );
			const std::uint32_t recorded = walk.Take( 1 ) ? static_cast<std::uint32_t>( walk.Take( 16 ) ) : 0u;
			if ( ours & ~recorded ) { ++result_.player.other; Report( "player array", start ); return; }
			if ( recorded != ours ) hidden = true;
			for ( int i = 0; i < 16; ++i ) if ( recorded & ( 1u << i ) ) walk.bit += block == 3 ? 32 : 16;
		}
		if ( hidden && walk.bit == msg.bit ) ++result_.player.hidden;
		else { ++result_.player.other; Report( "player length", start ); }
	}

	bool Snapshot( msg_t &msg, int sequence ) {
		++result_.snapshots;
		MSG_ReadLong( &msg );
		MSG_ReadLong( &msg );
		const int deltaNum = MSG_ReadByte( &msg );
		MSG_ReadByte( &msg );
		const int areaBytes = MSG_ReadByte( &msg );
		if ( areaBytes < 0 || areaBytes > MAX_MAP_AREA_BYTES ) return Fail( "areamask" );
		byte areamask[MAX_MAP_AREA_BYTES];
		MSG_ReadData( &msg, areamask, areaBytes );

		Frame next;
		next.messageNum = sequence;
		const Frame *old = nullptr;
		if ( deltaNum ) {
			old = &frames_[( sequence - deltaNum ) & ( kPacketBackup - 1 )];
			if ( !old->valid || old->messageNum != sequence - deltaNum ) {
				++result_.invalidDeltaFrames;
				return Fail( "delta from a frame that is not held" );
			}
		}

		const int playerStart = msg.bit;
		if ( !MSG_ReadDeltaPlayerstateForWireProfile( &msg, old ? &old->ps : nullptr, &next.ps, kWire ) )
			return Fail( "player delta" );
		{
			std::array<byte, kMaxMessage> scratch{};
			msg_t writer{};
			MSG_Init( &writer, scratch.data(), static_cast<int>( scratch.size() ) );
			MSG_STVEFBitstream( &writer );
			if ( !MSG_WriteDeltaPlayerstateForWireProfile( &writer, old ? &old->ps : nullptr, &next.ps, kWire ) ||
				writer.bit != msg.bit - playerStart ||
				!SameBits( msg.data, playerStart, scratch.data(), 0, writer.bit ) ) {
				ClassifyPlayer( msg, playerStart, old ? &old->ps : nullptr, next.ps );
			}
		}

		auto oldEntity = old ? old->entities.begin() : next.entities.end();
		const auto oldEnd = old ? old->entities.end() : next.entities.end();
		for ( ;; ) {
			const int start = msg.bit;
			const int number = MSG_ReadEntitynum( &msg );
			if ( number < 0 ) return Fail( "entity list truncated" );
			if ( number == MAX_GENTITIES - 1 ) break;
			while ( oldEntity != oldEnd && oldEntity->first < number ) {
				next.entities.insert( *oldEntity );
				++oldEntity;
			}
			entityState_t decoded{};
			const entityState_t *from = nullptr;
			bool force = false;
			if ( oldEntity != oldEnd && oldEntity->first == number ) {
				from = &oldEntity->second;
				++oldEntity;
			} else {
				from = &baselines_[number];
				force = true;
			}
			if ( !MSG_ReadDeltaEntityForWireProfile( &msg, from, &decoded, number, kWire ) )
				return Fail( "entity delta" );
			CountSymbol( msg, start );
			const bool removed = decoded.number == MAX_GENTITIES - 1;
			// An unchanged entity is only ever sent when forced.
			if ( msg.bit - start == 12 ) force = true;
			if ( !Reencode( msg, start, from, removed ? nullptr : &decoded, force ) ) {
				if ( removed ) { ++result_.entity.other; Report( "removal", start ); }
				else ClassifyEntity( msg, start, *from, decoded, result_.entity );
			}
			if ( !removed ) next.entities[number] = decoded;
			++result_.entityDeltas;
		}
		for ( ; oldEntity != oldEnd; ++oldEntity ) next.entities.insert( *oldEntity );
		next.valid = true;
		frames_[sequence & ( kPacketBackup - 1 )] = next;
		return true;
	}

	Result &result_;
	int reported_ = 0;
	std::array<Frame, kPacketBackup> frames_;
	std::array<entityState_t, MAX_GENTITIES> baselines_{};
};

std::string Escape( const std::string &text ) {
	std::string out;
	for ( const char c : text ) {
		if ( c == '"' || c == '\\' ) out += '\\';
		if ( static_cast<unsigned char>( c ) >= 0x20 ) out += c;
	}
	return out;
}

void ProbeDemo( const std::string &path ) {
	Result result;
	Probe probe( result );
	FILE *file = std::fopen( path.c_str(), "rb" );
	if ( !file ) {
		result.error = "cannot open";
	} else {
		for ( ;; ) {
			std::int32_t header[2];
			if ( std::fread( header, sizeof( header ), 1, file ) != 1 ) {
				result.error = "truncated file";
				break;
			}
			if ( header[1] == -1 ) break;
			if ( header[1] < 0 || header[1] > kMaxMessage ) {
				result.error = "bad record length";
				break;
			}
			std::vector<byte> data( static_cast<std::size_t>( header[1] ) );
			if ( !data.empty() && std::fread( data.data(), data.size(), 1, file ) != 1 ) {
				result.error = "truncated record";
				break;
			}
			if ( !probe.Record( header[0], data ) ) {
				result.error += " at record " + std::to_string( result.records );
				break;
			}
		}
		std::fclose( file );
	}
	std::cout << "{\"file\":\"" << Escape( path ) << "\",\"version\":\"" << Escape( result.version )
		<< "\",\"records\":" << result.records << ",\"gamestates\":" << result.gamestates
		<< ",\"snapshots\":" << result.snapshots << ",\"serverCommands\":" << result.serverCommands
		<< ",\"configstrings\":" << result.configstrings << ",\"baselines\":" << result.baselines
		<< ",\"entityDeltas\":" << result.entityDeltas
		<< ",\"reencodeMismatches\":{";
	const std::pair<const char *, const Result::Mismatches *> kinds[] = {
		{ "player", &result.player }, { "entity", &result.entity }, { "baseline", &result.baseline } };
	for ( std::size_t i = 0; i < 3; ++i ) {
		std::cout << ( i ? "," : "" ) << "\"" << kinds[i].first << "\":{\"hidden\":" << kinds[i].second->hidden
			<< ",\"escape\":" << kinds[i].second->escape << ",\"other\":" << kinds[i].second->other << "}";
	}
	std::cout << "}"
		<< ",\"vectorSymbols\":[";
	for ( std::size_t i = 0; i < result.vectorSymbols.size(); ++i )
		std::cout << ( i ? "," : "" ) << result.vectorSymbols[i];
	std::cout << "],\"leftoverBits\":[";
	for ( std::size_t i = 0; i < result.leftoverBits.size(); ++i )
		std::cout << ( i ? "," : "" ) << result.leftoverBits[i];
	std::cout << "],\"error\":\"" << Escape( result.error ) << "\"}" << std::endl;
}

} // namespace

int main() {
	std::string path;
	while ( std::getline( std::cin, path ) ) {
		if ( !path.empty() ) ProbeDemo( path );
	}
	return 0;
}
