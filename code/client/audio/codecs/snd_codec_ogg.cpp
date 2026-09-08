/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)
Copyright (C) 2005-2006 Joerg Dietrich <dietrich_joerg@gmx.de>

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

// OGG support is enabled by this define
#ifdef USE_OGG_VORBIS

// includes for the Q3 sound system
extern "C" {
#include "../../client.h"
#include "snd_codec.h"
}

#include "../../client_cpp.h"
#include "../shared/AudioVorbisLayout.h"

// includes for the OGG codec
#include <cerrno>
#include <limits>
#include <memory>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

using fnql::AllocateZoneMemory;
using fnql::FileRead;
using fnql::ScopedTempMemory;
using fnql::ScopedZoneMemory;

// The OGG codec can return the samples in a number of different formats,
// we use the standard signed short format.
namespace {

constexpr int OGG_SAMPLEWIDTH = 2;

struct OggStreamDeleter {
	void operator()( snd_stream_t *stream ) const
	{
		if ( stream ) {
			S_OGG_CodecCloseStream( stream );
		}
	}
};

} // namespace

// Q3 OGG codec
snd_codec_t ogg_codec =
{
	"ogg",
	S_OGG_CodecLoad,
	S_OGG_CodecOpenStream,
	S_OGG_CodecReadStream,
	S_OGG_CodecCloseStream,
	nullptr
};

// callbacks for vobisfile

// fread() replacement
size_t S_OGG_Callback_read( void *ptr, size_t size, size_t nmemb, void *datasource )
{
	if ( !size || !nmemb ) {
		errno = 0;
		return 0;
	}
	if ( !ptr || !datasource ) {
		errno = !ptr ? EFAULT : EBADF;
		return 0;
	}
	const size_t maximum = static_cast<size_t>( ( std::numeric_limits<int>::max )() );
	if ( size > maximum || nmemb > maximum / size ) {
		errno = EINVAL;
		return 0;
	}
	auto *stream = static_cast<snd_stream_t *>( datasource );
	if ( stream->pos < 0 || stream->pos > stream->length ) {
		errno = EIO;
		return 0;
	}
	int bytes = static_cast<int>( size * nmemb );
	const int remaining = stream->length - stream->pos;
	if ( bytes > remaining ) bytes = remaining;
	if ( !bytes ) return 0;
	const int count = FileRead( stream->file, ptr, bytes );
	if ( count < 0 ) {
		errno = EIO;
		return 0;
	}
	stream->pos += count;
	// fread reports complete elements; a trailing partial element is consumed
	// but must not be advertised as initialized output.
	return static_cast<size_t>( count ) / size;
}

// fseek() replacement
int S_OGG_Callback_seek( void *datasource, ogg_int64_t offset, int whence )
{
	if ( !datasource ) {
		errno = EBADF;
		return -1;
	}
	auto *stream = static_cast<snd_stream_t *>( datasource );
	ogg_int64_t base;
	switch ( whence ) {
		case SEEK_SET: base = 0; break;
		case SEEK_CUR: base = FS_FTell( stream->file ); break;
		case SEEK_END: base = stream->length; break;
		default: errno = EINVAL; return -1;
	}
	// Compare before adding or narrowing, including extreme 64-bit offsets.
	if ( base < 0 || base > stream->length
		|| offset < -base || offset > stream->length - base ) {
		errno = EINVAL;
		return -1;
	}
	const int target = static_cast<int>( base + offset );
	if ( FS_Seek( stream->file, static_cast<long>( target ), FS_SEEK_SET ) < 0 ) {
		errno = EIO;
		return -1;
	}
	// Packed-file seeks may return a positive success value. Verify the real
	// cursor rather than assuming either zero-only success or a requested move.
	const int actual = FS_FTell( stream->file );
	if ( actual >= 0 && actual <= stream->length ) stream->pos = actual;
	if ( actual != target ) {
		errno = EIO;
		return -1;
	}
	return 0;
}

// fclose() replacement
int S_OGG_Callback_close(void *datasource)
{
	// we do nothing here and close all things manually in S_OGG_CodecCloseStream()
	return 0;
}

// ftell() replacement
long S_OGG_Callback_tell(void *datasource)
{
	snd_stream_t   *stream;

	// check if input is valid
	if (!datasource)
	{
		errno = EBADF;
		return -1;
	}

	// snd_stream_t in the generic pointer
	stream = static_cast<snd_stream_t *>( datasource );

	return static_cast<long>( FS_FTell(stream->file) );
}

// the callback structure
const ov_callbacks S_OGG_Callbacks =
{
	&S_OGG_Callback_read,
	&S_OGG_Callback_seek,
	&S_OGG_Callback_close,
	&S_OGG_Callback_tell
};

/*
=================
S_OGG_CodecOpenStream
=================
*/
snd_stream_t *S_OGG_CodecOpenStream(const char *filename)
{
	snd_stream_t *stream;

	// OGG codec control structure
	OggVorbis_File *vf;

	// some variables used to get informations about the OGG 
	vorbis_info *OGGInfo;
	ogg_int64_t numSamples;

	// check if input is valid
	if (!filename)
	{
		return nullptr;
	}

	// Open the stream
	stream = S_CodecUtilOpen(filename, &ogg_codec);
	if (!stream)
	{
		return nullptr;
	}

	// alloctate the OggVorbis_File
	ScopedZoneMemory vfStorage = AllocateZoneMemory( sizeof( OggVorbis_File ), "OggVorbis_File", __FILE__, __LINE__ );
	vf = vfStorage.as<OggVorbis_File>();
	if (!vf)
	{
		S_CodecUtilClose(&stream);

		return nullptr;
	}

	// open the codec with our callbacks and stream as the generic pointer
	if (ov_open_callbacks(stream, vf, nullptr, 0, S_OGG_Callbacks) != 0)
	{
		S_CodecUtilClose(&stream);

		return nullptr;
	}

	// the stream must be seekable
	if (!ov_seekable(vf))
	{
		ov_clear(vf);

		S_CodecUtilClose(&stream);

		return nullptr;  
	}
 
	// we only support OGGs with one substream
	if (ov_streams(vf) != 1)
	{
		ov_clear(vf);

		S_CodecUtilClose(&stream);

		return nullptr;  
	}

	// get the info about channels and rate
	OGGInfo = ov_info(vf, 0);
	if (!OGGInfo)
	{
		ov_clear(vf);

		S_CodecUtilClose(&stream);

		return nullptr;  
	}

	// get the number of sample-frames in the OGG
	numSamples = ov_pcm_total(vf, 0);
	const int maxDecodedBytes = ( std::numeric_limits<int>::max )();
	if ( OGGInfo->rate <= 0 || OGGInfo->rate > maxDecodedBytes
		|| OGGInfo->channels <= 0 || OGGInfo->channels > maxDecodedBytes / OGG_SAMPLEWIDTH
		|| numSamples < 0
		|| numSamples > maxDecodedBytes / ( OGGInfo->channels * OGG_SAMPLEWIDTH ) ) {
		ov_clear(vf);
		S_CodecUtilClose(&stream);
		return nullptr;
	}

	// fill in the info-structure in the stream
	stream->info.rate = OGGInfo->rate;
	stream->info.width = OGG_SAMPLEWIDTH;
	stream->info.channels = OGGInfo->channels;
	stream->info.samples = numSamples;
	stream->info.size = stream->info.samples * stream->info.channels * stream->info.width;
	stream->info.dataofs = 0;
	stream->info.speakerMask = fntech3::audio::VorbisHasEncodedFieldTag( filename, stream->info.channels ) ? 0 :
		fntech3::audio::VorbisSpeakerMask( stream->info.channels );

	// The callbacks retain the actual compressed-file position established
	// by ov_open_callbacks; do not reset it without seeking the file.
	
	// We use the generic pointer in stream for the OGG codec control structure
	stream->ptr = vfStorage.release();

	return stream;
}

/*
=================
S_OGG_CodecCloseStream
=================
*/
void S_OGG_CodecCloseStream(snd_stream_t *stream)
{
	// check if input is valid
	if (!stream)
	{
		return;
	}
	
	// let the OGG codec cleanup its stuff
	ov_clear(static_cast<OggVorbis_File *>( stream->ptr ));

	// free the OGG codec control struct
	Z_Free(stream->ptr);

	// close the stream
	S_CodecUtilClose(&stream);
}

/*
=================
S_OGG_CodecReadStream
=================
*/
int S_OGG_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer)
{
	// buffer handling
	int bytesRead, bytesLeft, c;
	char *bufPtr;
	
	// Bitstream for the decoder
	int BS = 0;

	// big endian machines want their samples in big endian order
	int IsBigEndian = 0;

#	ifdef Q3_BIG_ENDIAN
	IsBigEndian = 1;
#	endif // Q3_BIG_ENDIAN

	// check if input is valid
	if (!(stream && buffer))
	{
		return 0;
	}

	if (bytes <= 0)
	{
		return 0;
	}

	bytesRead = 0;
	bytesLeft = bytes;
	bufPtr = static_cast<char *>( buffer );

	// cycle until we have the requested or all available bytes read
	while (-1)
	{
		// read some bytes from the OGG codec
		c = ov_read(static_cast<OggVorbis_File *>( stream->ptr ), bufPtr, bytesLeft, IsBigEndian, OGG_SAMPLEWIDTH, 1, &BS);
		
		// no more bytes are left
		if (c <= 0)
		{
			break;
		}

		if ( stream->info.speakerMask ) {
			fntech3::audio::ReorderVorbisFrames( bufPtr, c, stream->info.channels );
		}
		bytesRead += c;
		bytesLeft -= c;
		bufPtr += c;
  
		// we have enough bytes
		if (bytesLeft <= 0)
		{
			break;
		}
	}

	return bytesRead;
}

/*
=====================================================================
S_OGG_CodecLoad

We handle S_OGG_CodecLoad as a special case of the streaming functions 
where we read the whole stream at once.
======================================================================
*/
void *S_OGG_CodecLoad(const char *filename, snd_info_t *info)
{
	int bytesRead;
	
	// check if input is valid
	if (!(filename && info))
	{
		return nullptr;
	}
	
	// open the file as a stream
	std::unique_ptr<snd_stream_t, OggStreamDeleter> stream( S_OGG_CodecOpenStream(filename) );
	if (!stream)
	{
		return nullptr;
	}
	
	// allocate a buffer
	// this buffer must be free-ed by the caller of this function
	ScopedTempMemory bufferStorage = ScopedTempMemory::Allocate(stream->info.size);
	byte *buffer = bufferStorage.as<byte>();
	if (!buffer)
	{
		return nullptr;	
	}

	// fill the buffer
	bytesRead = S_OGG_CodecReadStream(stream.get(), stream->info.size, buffer);
	
	// Never publish a buffer whose advertised tail was not decoded.
	if (bytesRead <= 0 || bytesRead != stream->info.size)
	{
		return nullptr;	
	}

	*info = stream->info;
	return bufferStorage.release();
}

#endif // USE_OGG_VORBIS
