/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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

extern "C" {
#include "../../client.h"
#include "snd_codec.h"
}

#include "../../client_cpp.h"

#include <array>
#include <cstring>

using fnql::AllocateZoneMemory;
using fnql::FileRead;
using fnql::OpenFileRead;
using fnql::ScopedFileHandle;
using fnql::ScopedTempMemory;
using fnql::ScopedZoneMemory;

namespace {

// The rest of the sound pipeline only consumes 8/16-bit integer PCM, so
// 24-bit integer, 32-bit float and multichannel unsigned PCM8 sources
// are converted to 16-bit here.
constexpr int kWavFormatPCM = 0x0001;
constexpr int kWavFormatIEEEFloat = 0x0003;
constexpr int kWavFormatExtensible = 0xFFFE;

struct WavSourceFormat {
	int format = kWavFormatPCM;		// resolved format tag (extensible unwrapped)
	int width = 0;					// bytes per sample as stored in the file
	bool needsConversion = false;	// decoded to 16-bit PCM on read
};

// Stream-side conversion state stashed in snd_stream_t::ptr; the field stays
// null for sources that can be read straight through.
struct WavStreamState {
	int format;
	int width;
};

// File lengths and filesystem read/seek counts use signed int. Keep each
// chunk within both the physical file and its declared RIFF container.
unsigned int WavLittleLong( const byte *p ) {
	return static_cast<unsigned int>( p[0] ) |
		( static_cast<unsigned int>( p[1] ) << 8 ) |
		( static_cast<unsigned int>( p[2] ) << 16 ) |
		( static_cast<unsigned int>( p[3] ) << 24 );
}

int WavLittleShort( const byte *p ) {
	return p[0] | ( static_cast<int>( p[1] ) << 8 );
}

struct WavReader {
	fileHandle_t file;
	int remaining;

	bool Read( void *buffer, int bytes ) {
		if ( bytes < 0 || bytes > remaining || FileRead( file, buffer, bytes ) != bytes ) {
			return false;
		}
		remaining -= bytes;
		return true;
	}

	bool Skip( int bytes ) {
		if ( bytes < 0 || bytes > remaining ) {
			return false;
		}
		if ( bytes ) {
			const int before = FS_FTell( file );
			// Loose files return zero on success; ZIP seeks return the offset.
			// Verify the resulting position as ZIP seeks can hide short reads.
			if ( before < 0 || before > 0x7fffffff - bytes ||
				FS_Seek( file, bytes, FS_SEEK_CUR ) < 0 || FS_FTell( file ) != before + bytes ) {
				return false;
			}
		}
		remaining -= bytes;
		return true;
	}
};

int S_FindRIFFChunk( WavReader &reader, const char *chunk ) {
	std::array<byte, 8> header{};
	while ( reader.remaining >= 8 ) {
		if ( !reader.Read( header.data(), 8 ) ) {
			return -1;
		}
		const unsigned int length = WavLittleLong( header.data() + 4 );
		if ( length > static_cast<unsigned int>( reader.remaining ) ) {
			return -1;
		}
		const int len = static_cast<int>( length );
		if ( std::memcmp( header.data(), chunk, 4 ) == 0 ) {
			// A final data payload may omit its unused pad byte. Padding is
			// required only when advancing to another chunk (including fmt).
			return len;
		}
		const int padding = len & 1;
		if ( padding > reader.remaining - len ) {
			return -1;
		}
		if ( !reader.Skip( len + padding ) ) {
			return -1;
		}
	}
	return -1;
}

/*
=================
S_ByteSwapRawSamples

If raw data has been loaded in little endian binary form, this must be done.
If raw data was calculated, as with ADPCM, this should not be called.
=================
*/
void S_ByteSwapRawSamples( int samples, int width, int s_channels, byte *data ) {
	if ( width != 2 ) {
		return;
	}
	if ( LittleShort( 256 ) == 256 ) {
		return;
	}

	// Header validation bounds the decoded byte count, including all channels.
	samples *= s_channels;

	auto *sampleData = reinterpret_cast<short *>( data );
	for ( int i = 0; i < samples; i++ ) {
		sampleData[i] = LittleShort( sampleData[i] );
	}
}

short ConvertWavSampleToPCM16( const byte *src, const WavSourceFormat &source ) {
	if ( source.format == kWavFormatPCM && source.width == 1 ) {
		return static_cast<short>( ( static_cast<int>( src[0] ) - 128 ) * 256 );
	}
	if ( source.format == kWavFormatIEEEFloat ) {
		unsigned int bits = static_cast<unsigned int>( src[0] ) |
			( static_cast<unsigned int>( src[1] ) << 8 ) |
			( static_cast<unsigned int>( src[2] ) << 16 ) |
			( static_cast<unsigned int>( src[3] ) << 24 );
		float value;
		std::memcpy( &value, &bits, sizeof( value ) );
		if ( !( value >= -1.0f ) ) {	// also catches NaN
			value = ( value < 0.0f ) ? -1.0f : 0.0f;
		} else if ( value > 1.0f ) {
			value = 1.0f;
		}
		return static_cast<short>( value * 32767.0f );
	}

	// 24-bit little-endian signed PCM: keep the top 16 bits.
	int value = static_cast<int>( src[0] ) |
		( static_cast<int>( src[1] ) << 8 ) |
		( static_cast<int>( src[2] ) << 16 );
	if ( value & 0x800000 ) {
		value -= 0x1000000;
	}
	return static_cast<short>( value >> 8 );
}

void ConvertWavBlockToPCM16( short *dst, const byte *src, int sampleCount, const WavSourceFormat &source ) {
	for ( int i = 0; i < sampleCount; ++i ) {
		dst[i] = ConvertWavSampleToPCM16( src + static_cast<size_t>( i ) * static_cast<size_t>( source.width ), source );
	}
}

/*
=================
S_ReadRIFFHeader

Fills info with the decoded (8/16-bit PCM) stream description and source
with the on-disk sample format.
=================
*/
qboolean S_ReadRIFFHeader( fileHandle_t file, int fileLength, snd_info_t *info, WavSourceFormat *source ) {
	WavReader reader{ file, fileLength };
	std::array<byte, 12> riff{};
	if ( !reader.Read( riff.data(), 12 ) ||
		std::memcmp( riff.data(), "RIFF", 4 ) != 0 ||
		std::memcmp( riff.data() + 8, "WAVE", 4 ) != 0 ) {
		return qfalse;
	}
	const unsigned int riffLength = WavLittleLong( riff.data() + 4 );
	if ( riffLength < 4 || riffLength - 4 > static_cast<unsigned int>( reader.remaining ) ) {
		return qfalse;
	}
	reader.remaining = static_cast<int>( riffLength - 4 );

	const int fmtlen = S_FindRIFFChunk( reader, "fmt " );
	std::array<byte, 40> fmt{};
	if ( fmtlen < 16 || !reader.Read( fmt.data(), 16 ) ) {
		return qfalse;
	}
	int format = WavLittleShort( fmt.data() );
	const int channels = WavLittleShort( fmt.data() + 2 );
	const unsigned int rate = WavLittleLong( fmt.data() + 4 );
	const int bits = WavLittleShort( fmt.data() + 14 );
	int consumed = 16;
	unsigned int speakerMask = 0;
	if ( format == kWavFormatExtensible ) {
		if ( fmtlen < 40 || !reader.Read( fmt.data() + 16, 24 ) ) {
			return qfalse;
		}
		const int extensionBytes = WavLittleShort( fmt.data() + 16 );
		if ( extensionBytes < 22 || extensionBytes > fmtlen - 18 ) {
			return qfalse;
		}
		// PCM/IEEE float GUIDs share this suffix after the two-byte format
		// tag: xxxxxxxx-0000-0010-8000-00aa00389b71 (Windows SDK ksmedia.h).
		constexpr std::array<byte, 14> waveGuidSuffix{
			0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71
		};
		if ( std::memcmp( fmt.data() + 26, waveGuidSuffix.data(), waveGuidSuffix.size() ) != 0 ) {
			return qfalse;
		}
		// WAVEFORMATEXTENSIBLE precision cannot exceed its sample container
		// (Microsoft ksmedia.h contract). Reduced PCM precision is left-aligned,
		// so the existing container-based conversion retains the correct scale.
		if ( WavLittleShort( fmt.data() + 18 ) > bits ) {
			return qfalse;
		}
		format = WavLittleShort( fmt.data() + 24 );
		speakerMask = WavLittleLong( fmt.data() + 20 );
		consumed = 40;
	}
	if ( !reader.Skip( fmtlen - consumed + ( fmtlen & 1 ) ) ) {
		return qfalse;
	}
	if ( ( format == kWavFormatPCM && bits != 8 && bits != 16 && bits != 24 ) ||
		( format == kWavFormatIEEEFloat && bits != 32 ) ||
		( format != kWavFormatPCM && format != kWavFormatIEEEFloat ) ||
		channels <= 0 || channels > 32767 || rate == 0 || rate > 0x7fffffffu ) {
		return qfalse;
	}

	const int dataSize = S_FindRIFFChunk( reader, "data" );
	if ( dataSize < 0 ) {
		return qfalse;
	}
	WavSourceFormat parsedSource;
	parsedSource.format = format;
	parsedSource.width = bits / 8;
	// RawSamples retains its legacy signed PCM8 stereo contract. WAV PCM8
	// is unsigned for every channel count, so normalize multichannel WAVs
	// here instead of changing the game-module/raw boundary.
	parsedSource.needsConversion = ( format == kWavFormatIEEEFloat || bits == 24 || ( bits == 8 && channels > 1 ) );
	snd_info_t parsed{};
	parsed.channels = channels;
	parsed.speakerMask = speakerMask;
	parsed.rate = static_cast<int>( rate );
	parsed.samples = dataSize / ( parsedSource.width * channels );
	parsed.width = parsedSource.needsConversion ? 2 : parsedSource.width;
	// PCM8 normalization expands samples; check before multiplying signed sizes.
	if ( parsed.samples > 0x7fffffff / ( parsed.width * channels ) ) {
		return qfalse;
	}
	parsed.size = parsed.samples * parsed.width * channels;
	*info = parsed;
	*source = parsedSource;
	return qtrue;
}

} // namespace

// WAV codec
snd_codec_t wav_codec = {
	"wav",
	S_WAV_CodecLoad,
	S_WAV_CodecOpenStream,
	S_WAV_CodecReadStream,
	S_WAV_CodecCloseStream,
	nullptr
};

/*
=================
S_WAV_CodecLoad
=================
*/
extern "C" void *S_WAV_CodecLoad( const char *filename, snd_info_t *info ) {
	ScopedFileHandle file;
	const int fileLength = OpenFileRead( filename, file, qtrue );
	if ( !file ) {
		return nullptr;
	}

	WavSourceFormat source;
	if ( !S_ReadRIFFHeader( file.get(), fileLength, info, &source ) ) {
		Com_Printf( S_COLOR_RED "ERROR: Incorrect/unsupported format in \"%s\"\n",
			filename );
		return nullptr;
	}

	// Temp hunk memory is LIFO: allocate the returned buffer first so the
	// source scratch buffer can be released before this function returns.
	auto bufferStorage = ScopedTempMemory::Allocate( info->size );
	byte *buffer = bufferStorage.as<byte>();
	if ( !buffer ) {
		Com_Printf( S_COLOR_RED "ERROR: Out of memory reading \"%s\"\n",
			filename );
		return nullptr;
	}

	if ( !source.needsConversion ) {
		if ( FileRead( file.get(), buffer, info->size ) != info->size ) {
			return nullptr;
		}
		S_ByteSwapRawSamples( info->samples, info->width, info->channels, buffer );
		return bufferStorage.release();
	}

	const int sampleCount = info->samples * info->channels;
	const int sourceBytes = sampleCount * source.width;
	auto sourceStorage = ScopedTempMemory::Allocate( sourceBytes );
	byte *sourceBuffer = sourceStorage.as<byte>();
	if ( !sourceBuffer ) {
		Com_Printf( S_COLOR_RED "ERROR: Out of memory reading \"%s\"\n",
			filename );
		return nullptr;
	}

	const int bytesRead = FileRead( file.get(), sourceBuffer, sourceBytes );
	if ( bytesRead != sourceBytes ) {
		return nullptr;
	}
	ConvertWavBlockToPCM16( reinterpret_cast<short *>( buffer ), sourceBuffer, sampleCount, source );

	return bufferStorage.release();
}

/*
=================
S_WAV_CodecOpenStream
=================
*/
extern "C" snd_stream_t *S_WAV_CodecOpenStream( const char *filename ) {
	snd_stream_t *rv = S_CodecUtilOpen( filename, &wav_codec );
	if ( !rv ) {
		return nullptr;
	}

	WavSourceFormat source;
	if ( !S_ReadRIFFHeader( rv->file, rv->length, &rv->info, &source ) ) {
		S_CodecUtilClose( &rv );
		return nullptr;
	}

	if ( source.needsConversion ) {
		ScopedZoneMemory stateStorage = AllocateZoneMemory( sizeof( WavStreamState ), "WavStreamState", __FILE__, __LINE__ );
		auto *state = stateStorage.as<WavStreamState>();
		if ( !state ) {
			S_CodecUtilClose( &rv );
			return nullptr;
		}
		state->format = source.format;
		state->width = source.width;
		rv->ptr = stateStorage.release();
	}

	return rv;
}

/*
=================
S_WAV_CodecCloseStream
=================
*/
extern "C" void S_WAV_CodecCloseStream( snd_stream_t *stream ) {
	if ( stream->ptr != nullptr ) {
		Z_Free( stream->ptr );
		stream->ptr = nullptr;
	}
	S_CodecUtilClose( &stream );
}

/*
=================
S_WAV_CodecReadStream
=================
*/
extern "C" int S_WAV_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer ) {
	if ( bytes <= 0 ) {
		return 0;
	}
	const int remaining = stream->info.size - stream->pos;
	if ( remaining <= 0 ) {
		return 0;
	}
	if ( bytes > remaining ) {
		bytes = remaining;
	}

	const auto *state = static_cast<const WavStreamState *>( stream->ptr );
	if ( state == nullptr ) {
		const int bytesRead = FileRead( stream->file, buffer, bytes );
		if ( bytesRead <= 0 ) {
			return 0;
		}
		stream->pos += bytesRead;
		// Swap every complete scalar sample, including an incomplete final frame.
		const int samples = bytesRead / stream->info.width;
		S_ByteSwapRawSamples( samples, stream->info.width, 1, static_cast<byte *>( buffer ) );
		return bytesRead;
	}

	// Decode converted source samples to 16-bit through a bounded
	// scratch buffer; stream->pos tracks decoded bytes.
	WavSourceFormat source;
	source.format = state->format;
	source.width = state->width;
	source.needsConversion = true;

	std::array<byte, 12288> sourceChunk;
	const int requestedSamples = bytes / static_cast<int>( sizeof( short ) );
	auto *out = static_cast<short *>( buffer );
	int decodedSamples = 0;

	while ( decodedSamples < requestedSamples ) {
		const int samplesThisPass = ( std::min )( requestedSamples - decodedSamples,
			static_cast<int>( sourceChunk.size() ) / source.width );
		const int sourceBytes = samplesThisPass * source.width;
		const int bytesRead = FileRead( stream->file, sourceChunk.data(), sourceBytes );
		const int samplesRead = bytesRead / source.width;
		if ( samplesRead <= 0 ) {
			break;
		}
		ConvertWavBlockToPCM16( out + decodedSamples, sourceChunk.data(), samplesRead, source );
		decodedSamples += samplesRead;
		if ( samplesRead < samplesThisPass ) {
			break;
		}
	}

	const int decodedBytes = decodedSamples * static_cast<int>( sizeof( short ) );
	stream->pos += decodedBytes;
	return decodedBytes;
}
