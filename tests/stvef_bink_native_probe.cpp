/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Optional Windows x86 fixture probe. Feed one legitimate BIKi file on stdin;
 * argv[1] is the absolute path to the user's installed Bink 1 DLL, argv[2]
 * may be "de". No retail inputs or proprietary headers are distributed.
 * Timing/audio are tested in the engine. This file-only probe disables Bink's
 * wait callback and sound to enumerate every decoded frame without pacing. */
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <io.h>
#include <fcntl.h>

static int __stdcall NoWait( void * ) { return 0; }
static void *Sys_LoadLibrary( const char *name ) { return LoadLibraryA( name ); }
static void *Sys_LoadFunction( void *library, const char *name ) {
	if ( !strcmp( name, "_BinkWait@4" ) ) return reinterpret_cast<void *>( NoWait );
	return reinterpret_cast<void *>( GetProcAddress( static_cast<HMODULE>( library ), name ) );
}
static void Sys_UnloadLibrary( void *library ) { FreeLibrary( static_cast<HMODULE>( library ) ); }
#include "code/client/cinematic_bink.hpp"

int main( int argc, char **argv ) {
	using namespace fntech3::stvef::bink;
	if ( argc < 2 || argc > 3 ) return 64;
	_setmode( _fileno( stdin ), _O_BINARY );
	std::vector<std::uint8_t> bytes;
	char block[65536];
	while ( std::cin ) {
		std::cin.read( block, sizeof( block ) );
		const auto count = static_cast<std::size_t>( std::cin.gcount() );
		if ( count > MaxFileBytes - bytes.size() ) return 65;
		bytes.insert( bytes.end(), block, block + count );
	}
	Movie movie;
	if ( !movie.Open( std::move( bytes ), argv[1], argc == 3 && !strcmp( argv[2], "de" ), false ) ) {
		std::cerr << movie.Error() << '\n';
		return 1;
	}
	const auto &info = movie.Info();
	std::uint64_t hash = 1469598103934665603ull;
	unsigned frames = 0;
	for ( std::uint32_t tick = 0; tick <= info.frames + 1; ++tick ) {
		const auto result = movie.Step( tick * 1000 );
		if ( result == Movie::Result::End ) break;
		if ( result == Movie::Result::Frame ) {
			++frames;
			for ( unsigned i = 0; i < info.width * info.height * 4; ++i )
				hash = ( hash ^ movie.Pixels()[i] ) * 1099511628211ull;
		}
	}
	std::cout << "{\"width\":" << info.width << ",\"height\":" << info.height
		<< ",\"frames\":" << frames << ",\"expected\":" << info.frames
		<< ",\"track\":" << info.soundTrack << ",\"hash\":\"" << hash << "\"}\n";
	return frames == info.frames ? 0 : 2;
}
