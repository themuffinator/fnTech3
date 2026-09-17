/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Offline synthetic codec oracle driver. Compile separately with fnTech3 or
 * the user's original WolfET msg.c/huffman.c. No game module or asset is loaded. */
extern "C" {
#include "q_shared.h"
#include "qcommon.h"
}
#undef vsnprintf
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

#ifdef PROBE_REFERENCE
using ProbeCommand = usercmd_t;
using ProbeEntity = entityState_t;
using ProbePlayer = playerState_t;
extern "C" {
struct ProbeField { const char *name; int offset, bits, used; };
extern ProbeField entityStateFields[71];
extern ProbeField playerStateFields[77];
cvar_t *cl_shownet = nullptr;
void QDECL Com_Printf( const char *, ... ) {}
void QDECL Com_Error( int, const char *format, ... ) {
	va_list args; va_start( args, format ); std::vfprintf( stderr, format, args ); va_end( args );
	std::exit( 9 );
}
void Com_Memcpy( void *to, const void *from, size_t count ) { std::memcpy( to, from, count ); }
void Com_Memset( void *to, int value, size_t count ) { std::memset( to, value, count ); }
short LittleShort( short value ) { return value; }
int LittleLong( int value ) { return value; }
float LittleFloat( float value ) { return value; }
void Q_strncpyz( char *to, const char *from, int size ) {
	if ( size <= 0 ) std::abort();
	std::snprintf( to, static_cast<size_t>( size ), "%s", from );
}
}
#else
#include "../../code/compat/wolfet_message_codec.hpp"
using namespace fntech3::wolfet::message;
using ProbeCommand = Command;
using ProbePlayer = Player;
using ProbeEntity = Entity;
extern "C" void QDECL Com_Printf( const char *, ... ) {}
extern "C" void QDECL Com_Error( errorParm_t, const char *format, ... ) {
	va_list args; va_start( args, format ); std::vfprintf( stderr, format, args ); va_end( args );
	std::exit( 9 );
}
#endif

struct Record { ProbeCommand command{}; ProbeEntity entity{}; ProbePlayer player{}; };
struct Request { std::uint32_t kind, key, flags, bits, packetBytes, offset; };
struct Reply { std::uint32_t bits, packetBytes, readBits, recordBytes; };

static std::size_t Size( unsigned kind ) {
	return kind == 0 ? sizeof( ProbeCommand ) : kind == 1 ? sizeof( ProbeEntity ) : sizeof( ProbePlayer );
}

static void *Data( Record &record, unsigned kind ) {
	if ( kind == 0 ) return &record.command;
	if ( kind == 1 ) return &record.entity;
	return &record.player;
}

static void Encode( msg_t *msg, const Request &request, Record &from, Record &to ) {
	const qboolean force = ( request.flags & 1 ) ? qtrue : qfalse;
	auto *entity = ( request.flags & 2 ) ? nullptr : &to.entity;
#ifdef PROBE_REFERENCE
	switch ( request.kind ) {
	case 0: MSG_WriteDeltaUsercmdKey( msg, request.key, &from.command, &to.command ); break;
	case 1: MSG_WriteDeltaEntity( msg, &from.entity, entity, force ); break;
	case 2: MSG_WriteDeltaPlayerstate( msg, &from.player, &to.player ); break;
	}
#else
	bool ok = false;
	switch ( request.kind ) {
	case 0: ok = MSG_WriteDeltaWolfETUsercmdKey( msg, request.key, &from.command, &to.command ); break;
	case 2: ok = MSG_WriteDeltaWolfETPlayerstate( msg, &from.player, &to.player ); break;
	case 1: ok = MSG_WriteDeltaWolfETEntity( msg, &from.entity, entity, force ); break;
	}
	if ( !ok ) std::exit( 10 );
#endif
}

static void Decode( msg_t *msg, const Request &request, Record &from, Record &to ) {
#ifdef PROBE_REFERENCE
	switch ( request.kind ) {
	case 0: MSG_ReadDeltaUsercmdKey( msg, request.key, &from.command, &to.command ); break;
	case 1: { const int number = MSG_ReadBits( msg, 10 );
		MSG_ReadDeltaEntity( msg, &from.entity, &to.entity, number ); break; }
	case 2: MSG_ReadDeltaPlayerstate( msg, &from.player, &to.player ); break;
	}
#else
	bool ok = false;
	switch ( request.kind ) {
	case 0: ok = MSG_ReadDeltaWolfETUsercmdKey( msg, request.key, &from.command, &to.command ); break;
	case 2: ok = MSG_ReadDeltaWolfETPlayerstate( msg, &from.player, &to.player ); break;
	case 1: { const int number = MSG_ReadEntitynum( msg );
		ok = MSG_ReadDeltaWolfETEntity( msg, &from.entity, &to.entity, number ); break; }
	}
	if ( !ok ) std::exit( 11 );
#endif
}

int main( int argc, char **argv ) {
	if ( argc == 2 && std::strcmp( argv[1], "metadata" ) == 0 ) {
		std::printf( "sizes %zu %zu %zu\n", sizeof( ProbeCommand ), sizeof( ProbeEntity ), sizeof( ProbePlayer ) );
#ifdef PROBE_REFERENCE
		for ( auto field : entityStateFields ) std::printf( "entity %d %d\n", field.offset, field.bits );
		for ( auto field : playerStateFields ) std::printf( "player %d %d\n", field.offset, field.bits );
#else
		for ( auto field : entityFields )
			std::printf( "entity %zu %d\n", field.offset, field.bits );
		for ( auto field : playerFields ) std::printf( "player %zu %d\n", field.offset, field.bits );
#endif
		return 0;
	}
	if ( argc != 4 ) return 2;
	const bool encode = std::strcmp( argv[1], "encode" ) == 0;
	if ( !encode && std::strcmp( argv[1], "decode" ) != 0 ) return 2;
	FILE *input = std::fopen( argv[2], "rb" );
	FILE *output = std::fopen( argv[3], "wb" );
	if ( !input || !output ) return 3;
	Request request{};
	for ( ;; ) {
		const auto headerBytes = std::fread( &request, 1, sizeof( request ), input );
		if ( headerBytes == 0 ) break;
		if ( headerBytes != sizeof( request ) ) return 4;
		if ( request.kind > 2 || request.packetBytes > 32768 || request.offset > 7 ) return 4;
		Record from{}, to{}, decoded{};
		const auto size = Size( request.kind );
		if ( std::fread( Data( from, request.kind ), 1, size, input ) != size ||
			std::fread( Data( to, request.kind ), 1, size, input ) != size ) return 4;
		byte packet[32768]{};
		if ( std::fread( packet, 1, request.packetBytes, input ) != request.packetBytes ) return 4;
		msg_t msg{};
		MSG_Init( &msg, packet, sizeof( packet ) );
		if ( encode ) {
			if ( request.offset ) MSG_WriteBits( &msg, 0x55, request.offset );
			Encode( &msg, request, from, to );
			if ( msg.overflowed ) return 5;
		} else {
			msg.cursize = request.packetBytes;
			msg.bit = request.bits;
		}
		Reply reply{ static_cast<std::uint32_t>( msg.bit ), static_cast<std::uint32_t>( msg.cursize ),
			0, static_cast<std::uint32_t>( size ) };
		MSG_BeginReading( &msg );
		msg.bit = request.offset;
		msg.readcount = request.offset ? 1 : 0;
		decoded = from;
		if ( reply.bits > request.offset ) Decode( &msg, request, from, decoded );
		reply.readBits = msg.bit;
		if ( std::fwrite( &reply, 1, sizeof( reply ), output ) != sizeof( reply ) ||
			std::fwrite( packet, 1, reply.packetBytes, output ) != reply.packetBytes ||
			std::fwrite( Data( decoded, request.kind ), 1, size, output ) != size ) return 6;
	}
	const bool failed = std::ferror( input ) || std::ferror( output );
	std::fclose( input );
	if ( std::fclose( output ) != 0 ) return 7;
	return failed ? 8 : 0;
}
