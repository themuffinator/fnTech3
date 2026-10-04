/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

// MPEG-1/2 Layer III sound and music through the shared Fn audio pipeline.
// Retail Elite Force stores its effects, speech and music as MP3 frames with a
// trailing Raven ID3v1 tag ("#UNCOMP", "#MAXVOL"); see
// docs/fntech3/2026-09-18-stvef-contracts.md. Raven's "#UNCOMP" byte count
// includes every MPEG frame, so this codec decodes frame by frame with
// minimp3 (CC0) and never drops a leading Xing/Info frame as tag metadata.
// The codec joins Elite Force, Alice, F.A.K.K.2, Medal of Honor and Jedi
// Academy profiles, which supply MP3 assets; other profiles keep their
// existing extension fallback behaviour.

#ifdef USE_CODEC_MP3

extern "C" {
#include "../../client.h"
#include "snd_codec.h"
}

#include "../../client_cpp.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

// The decoder is third-party code; keep strict project warnings on our own
// translation unit without applying them to minimp3's implementation.
#if defined( _MSC_VER ) && !defined( __clang__ )
#pragma warning( push, 0 )
#elif defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
#define MINIMP3_IMPLEMENTATION
#include <minimp3.h>
#if defined( _MSC_VER ) && !defined( __clang__ )
#pragma warning( pop )
#elif defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic pop
#endif

#include "snd_mp3_frames.h"

using fnql::AllocateZoneMemory;
using fnql::FileRead;
using fnql::ScopedTempMemory;
using fnql::ScopedZoneMemory;

namespace {

using fntech3::audio::Mp3FrameDecoder;
using fntech3::audio::Mp3StreamInfo;
using fntech3::audio::Mp3RavenPeak;

// minimp3 adapter for the frame walker in snd_mp3_frames.h.
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

static_assert( sizeof( mp3d_sample_t ) == sizeof( short ),
	"the engine mixer consumes signed 16-bit samples" );

// Streams own their compressed bytes, the minimp3 state and one decoded frame.
struct Mp3Stream {
	byte *compressed;
	MiniMp3 codec;
	Mp3FrameDecoder frames;
};

// Effects are unpacked as mono, as retail's decoder does; streams keep the
// source channels.
bool S_MP3_Info( const Mp3StreamInfo &scan, int outputChannels, snd_info_t *info ) {
	if ( !scan.valid || outputChannels < 1 || outputChannels > scan.channels ) {
		return false;
	}
	info->rate = scan.rate;
	info->width = 2;
	info->channels = outputChannels;
	info->samples = scan.frames;
	info->size = scan.frames * outputChannels * 2;
	info->dataofs = 0;
	info->speakerMask = 0;
	return true;
}

} // namespace

snd_codec_t mp3_codec =
{
	"mp3",
	S_MP3_CodecLoad,
	S_MP3_CodecOpenStream,
	S_MP3_CodecReadStream,
	S_MP3_CodecCloseStream,
	nullptr
};

/*
=================
S_MP3_CodecOpenStream

Music tracks are a few megabytes; keep the compressed file in zone memory and
decode one MPEG frame at a time as the mixer asks for samples.
=================
*/
snd_stream_t *S_MP3_CodecOpenStream( const char *filename )
{
	if ( !filename ) {
		return nullptr;
	}
	snd_stream_t *stream = S_CodecUtilOpen( filename, &mp3_codec );
	if ( !stream ) {
		return nullptr;
	}
	if ( stream->length <= 0 ) {
		S_CodecUtilClose( &stream );
		return nullptr;
	}

	ScopedZoneMemory stateStorage = AllocateZoneMemory( sizeof( Mp3Stream ), "Mp3Stream", __FILE__, __LINE__ );
	ScopedZoneMemory compressed = AllocateZoneMemory( stream->length, "compressed mp3", __FILE__, __LINE__ );
	if ( !stateStorage.get() || !compressed.get() ) {
		S_CodecUtilClose( &stream );
		return nullptr;
	}
	auto *state = new ( stateStorage.get() ) Mp3Stream{};

	int filled = 0;
	while ( filled < stream->length ) {
		const int count = FileRead( stream->file, compressed.as<byte>() + filled, stream->length - filled );
		if ( count <= 0 ) {
			break;
		}
		filled += count;
	}
	if ( filled != stream->length ) {
		S_CodecUtilClose( &stream );
		return nullptr;
	}
	stream->pos = filled;

	mp3dec_init( &state->codec.decoder );
	state->frames.Reset( compressed.as<std::uint8_t>(), filled );
	const Mp3StreamInfo scan = state->frames.Scan( &state->codec, &MiniMp3::Decode );
	if ( !S_MP3_Info( scan, scan.channels, &stream->info ) ) {
		S_CodecUtilClose( &stream );
		return nullptr;
	}
	mp3dec_init( &state->codec.decoder );
	state->frames.Reset( compressed.as<std::uint8_t>(), filled );

	state->compressed = static_cast<byte *>( compressed.release() );
	stream->ptr = stateStorage.release();
	return stream;
}

/*
=================
S_MP3_CodecCloseStream
=================
*/
void S_MP3_CodecCloseStream( snd_stream_t *stream )
{
	if ( !stream ) {
		return;
	}
	auto *state = static_cast<Mp3Stream *>( stream->ptr );
	if ( state ) {
		if ( state->compressed ) {
			Z_Free( state->compressed );
		}
		Z_Free( state );
		stream->ptr = nullptr;
	}
	S_CodecUtilClose( &stream );
}

/*
=================
S_MP3_CodecReadStream
=================
*/
int S_MP3_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer )
{
	if ( !stream || !stream->ptr || !buffer || bytes <= 0 ) {
		return 0;
	}
	auto *state = static_cast<Mp3Stream *>( stream->ptr );
	const int read = state->frames.Read( &state->codec, &MiniMp3::Decode,
		stream->info.channels, stream->info.rate, stream->info.channels,
		static_cast<short *>( buffer ), bytes );
	// minimp3 writes native short values, as the WAV/Vorbis codecs do.
	return read;
}

/*
=================
S_MP3_CodecLoad

Effects decode completely into temporary hunk memory, as the WAV and Vorbis
loaders do. A buffer whose advertised tail was not decoded is never returned.
=================
*/
void *S_MP3_CodecLoad( const char *filename, snd_info_t *info )
{
	if ( !filename || !info ) {
		return nullptr;
	}

	ScopedZoneMemory compressed;
	int length;
	{
		auto file = fnql::ScopedReadFile::Read( filename );
		if ( !file ) {
			return nullptr;
		}
		length = file.length();
		if ( length <= 0 ) {
			return nullptr;
		}
		compressed = AllocateZoneMemory( length, "compressed mp3", __FILE__, __LINE__ );
		if ( !compressed.get() ) {
			return nullptr;
		}
		std::memcpy( compressed.get(), file.get(), length );
	} // Release the file's temporary hunk before allocating decoded samples.

	ScopedZoneMemory stateStorage = AllocateZoneMemory( sizeof( Mp3Stream ), "Mp3Stream", __FILE__, __LINE__ );
	if ( !stateStorage.get() ) {
		return nullptr;
	}
	auto *state = new ( stateStorage.get() ) Mp3Stream{};
	mp3dec_init( &state->codec.decoder );
	state->frames.Reset( compressed.as<std::uint8_t>(), length );
	snd_info_t decodedInfo{};
	const Mp3StreamInfo scan = state->frames.Scan( &state->codec, &MiniMp3::Decode );
	if ( !S_MP3_Info( scan, 1, &decodedInfo ) ) {
		return nullptr;
	}
	ScopedTempMemory buffer = ScopedTempMemory::Allocate( decodedInfo.size );
	if ( !buffer.get() ) {
		return nullptr;
	}
	mp3dec_init( &state->codec.decoder );
	state->frames.Reset( compressed.as<std::uint8_t>(), length );
	const int bytesRead = state->frames.Read( &state->codec, &MiniMp3::Decode,
		scan.channels, scan.rate, 1, buffer.as<short>(), decodedInfo.size );
	if ( bytesRead <= 0 || bytesRead != decodedInfo.size ) {
		return nullptr;
	}
	decodedInfo.peak = Mp3RavenPeak( compressed.as<std::uint8_t>(), length );
	*info = decodedInfo;
	return buffer.release();
}

/*
=================
In-memory decoders

American McGee's Alice reads every song of a soundtrack once, when the
soundtrack loads, and keeps the file images (alice.exe 0x0044fad0); Miles then
starts and loops each song at a byte offset into its image (0x00472147,
0x00472170). The decoder here reads such an image in place and never owns it.
Elite Force never reaches these.
=================
*/
struct snd_mp3_memory_t {
	MiniMp3 codec;
	Mp3FrameDecoder frames;
	snd_info_t info;
};

snd_mp3_memory_t *S_MP3_OpenMemory( const byte *image, int length, snd_info_t *info )
{
	if ( !image || length <= 0 ) {
		return nullptr;
	}
	auto *memory = new ( std::nothrow ) snd_mp3_memory_t{};
	if ( !memory ) {
		return nullptr;
	}
	mp3dec_init( &memory->codec.decoder );
	memory->frames.Reset( reinterpret_cast<const std::uint8_t *>( image ), length );
	const Mp3StreamInfo scan = memory->frames.Scan( &memory->codec, &MiniMp3::Decode );
	if ( !S_MP3_Info( scan, scan.channels, &memory->info ) ) {
		delete memory;
		return nullptr;
	}
	mp3dec_init( &memory->codec.decoder );
	memory->frames.Reset( reinterpret_cast<const std::uint8_t *>( image ), length );
	if ( info ) {
		*info = memory->info;
	}
	return memory;
}

int S_MP3_SeekFileOffset( snd_mp3_memory_t *memory, int offset )
{
	if ( !memory ) {
		return 0;
	}
	mp3dec_init( &memory->codec.decoder );
	const std::int64_t skipped = memory->frames.SamplesBefore( &memory->codec, &MiniMp3::Decode, offset );
	// minimp3 resyncs from any byte once its state is reset.
	mp3dec_init( &memory->codec.decoder );
	memory->frames.SeekFileOffset( offset );
	return skipped > ( std::numeric_limits<int>::max )() ? ( std::numeric_limits<int>::max )()
		: static_cast<int>( skipped );
}

int S_MP3_MemoryStartOffset( const snd_mp3_memory_t *memory )
{
	return memory ? memory->frames.StartOffset() : 0;
}

int S_MP3_ReadMemory( snd_mp3_memory_t *memory, int bytes, void *pcm )
{
	if ( !memory || !pcm || bytes <= 0 ) {
		return 0;
	}
	const int read = memory->frames.Read( &memory->codec, &MiniMp3::Decode,
		memory->info.channels, memory->info.rate, memory->info.channels,
		static_cast<short *>( pcm ), bytes );
	return read;
}

void S_MP3_CloseMemory( snd_mp3_memory_t *memory )
{
	delete memory;
}

#endif // USE_CODEC_MP3
