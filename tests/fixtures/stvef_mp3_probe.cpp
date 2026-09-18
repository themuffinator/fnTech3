/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Decode every MP3 path read from stdin through the same frame walker and
 * minimp3 configuration as code/client/audio/codecs/snd_codec_mp3.cpp and
 * report one line per file:
 *   <path> <rate> <channels> <monoEffectBytes> <streamBytes> <status>
 * Used by scripts/probe_stvef_mp3.py against a legitimate Elite Force install;
 * no retail data is stored in the repository. */
#if defined( _MSC_VER ) && !defined( __clang__ )
#pragma warning( push, 0 )
#endif
#define MINIMP3_IMPLEMENTATION
#include <minimp3.h>
#if defined( _MSC_VER ) && !defined( __clang__ )
#pragma warning( pop )
#endif

#include "../../code/client/audio/codecs/snd_mp3_frames.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {
struct MiniMp3 {
	mp3dec_t decoder;
	static int Decode( void *self, const std::uint8_t *data, int bytes, short *pcm,
		int *frameBytes, int *frameOffset, int *channels, int *rate ) {
		mp3dec_frame_info_t info{};
		const int samples = mp3dec_decode_frame( &static_cast<MiniMp3 *>( self )->decoder,
			data, bytes, pcm, &info );
		*frameBytes = info.frame_bytes;
		*frameOffset = info.frame_offset;
		*channels = info.channels;
		*rate = info.hz;
		return samples;
	}
};
} // namespace

int main()
{
	std::string path;
	while ( std::getline( std::cin, path ) ) {
		if ( path.empty() ) continue;
		std::ifstream file( path, std::ios::binary );
		const std::vector<unsigned char> bytes( ( std::istreambuf_iterator<char>( file ) ),
			std::istreambuf_iterator<char>() );
		MiniMp3 codec{};
		auto frames = std::make_unique<fntech3::audio::Mp3FrameDecoder>();
		mp3dec_init( &codec.decoder );
		frames->Reset( bytes.data(), static_cast<int>( bytes.size() ) );
		const auto info = frames->Scan( &codec, &MiniMp3::Decode );
		if ( !info.valid ) {
			std::printf( "%s 0 0 0 scan-failed\n", path.c_str() );
			continue;
		}
		mp3dec_init( &codec.decoder );
		frames->Reset( bytes.data(), static_cast<int>( bytes.size() ) );
		// Effects unpack as mono; streams keep the source channels.
		long long totals[2] = {};
		for ( int pass = 0; pass < 2; ++pass ) {
			const int outputChannels = pass ? info.channels : 1;
			mp3dec_init( &codec.decoder );
			frames->Reset( bytes.data(), static_cast<int>( bytes.size() ) );
			std::vector<short> pcm( static_cast<std::size_t>( info.frames ) * outputChannels + 16 );
			// Read in odd-sized chunks to exercise partial-frame copies.
			for ( int chunk = 4093;; chunk = chunk == 4093 ? 1000 : 4093 ) {
				const int got = frames->Read( &codec, &MiniMp3::Decode, info.channels, info.rate,
					outputChannels, pcm.data() + totals[pass] / 2, chunk * 2 );
				totals[pass] += got;
				if ( got <= 0 ) break;
			}
		}
		const bool complete = totals[0] == static_cast<long long>( info.frames ) * 2
			&& totals[1] == static_cast<long long>( info.frames ) * info.channels * 2;
		std::printf( "%s %d %d %lld %lld %s\n", path.c_str(), info.rate, info.channels, totals[0],
			totals[1], complete ? "ok" : "short" );
	}
	return 0;
}
