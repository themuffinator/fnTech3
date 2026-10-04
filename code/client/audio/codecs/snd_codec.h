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

#ifndef _SND_CODEC_H_
#define _SND_CODEC_H_

#include "../../../qcommon/q_shared.h"
#include "../../../qcommon/qcommon.h"

struct snd_info_t {
	int rate;
	int width;
	int channels;
	int samples;
	int size;
	int dataofs;
	unsigned int speakerMask; // Internal codec metadata; zero keeps legacy channel order.
	// The loudest byte the sound reaches, which Elite Force measures its
	// lip-sync against; zero where the file reports none.
	float peak = 0.0f;
};

struct snd_codec_t;

struct snd_stream_t {
	snd_codec_t *codec;
	fileHandle_t file;
	snd_info_t info;
	int length;
	int pos;
	void *ptr;
};

// Codec functions
typedef void *(*CODEC_LOAD)(const char *filename, snd_info_t *info);
typedef snd_stream_t *(*CODEC_OPEN)(const char *filename);
typedef int (*CODEC_READ)(snd_stream_t *stream, int bytes, void *buffer);
typedef void (*CODEC_CLOSE)(snd_stream_t *stream);

// Codec data structure
struct snd_codec_t {
	const char *ext;
	CODEC_LOAD load;
	CODEC_OPEN open;
	CODEC_READ read;
	CODEC_CLOSE close;
	snd_codec_t *next;
};

// Decoded PCM16 is native-endian, for both whole sounds and streams. File
// codecs own byte-order conversion; mixers and OpenAL must not swap it again.
// PCM8 WAV output remains unsigned (converted multichannel WAV is PCM16).
// Codec management
void S_CodecInit( void );
void S_CodecShutdown( void );
void S_CodecPreloadJKAMusic( const char *intro, const char *loop );
void S_ClearJKAMusicPreload( void );
void *S_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_CodecOpenStream(const char *filename);
void S_CodecCloseStream(snd_stream_t *stream);
int S_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

// Util functions (used by codecs)
snd_stream_t *S_CodecUtilOpen(const char *filename, snd_codec_t *codec);
void S_CodecUtilClose(snd_stream_t **stream);

// WAV Codec
extern snd_codec_t wav_codec;
void *S_WAV_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_WAV_CodecOpenStream(const char *filename);
void S_WAV_CodecCloseStream(snd_stream_t *stream);
int S_WAV_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

// Ogg Vorbis codec
#ifdef USE_OGG_VORBIS
extern snd_codec_t ogg_codec;
void *S_OGG_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_OGG_CodecOpenStream(const char *filename);
void S_OGG_CodecCloseStream(snd_stream_t *stream);
int S_OGG_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);
#endif // USE_OGG_VORBIS

// MP3 codec: registered only for Star Trek Voyager: Elite Force profiles and
// American McGee's Alice.
#ifdef USE_CODEC_MP3
extern snd_codec_t mp3_codec;
void *S_MP3_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_MP3_CodecOpenStream(const char *filename);
void S_MP3_CodecCloseStream(snd_stream_t *stream);
int S_MP3_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

// American McGee's Alice decodes its songs and cinematic tracks from file
// images it reads once and keeps, and starts or loops them at a byte offset
// into the image, as Miles does for it. The image stays the caller's.
struct snd_mp3_memory_t;
snd_mp3_memory_t *S_MP3_OpenMemory( const byte *image, int length, snd_info_t *info );
// Continue at the first frame at or after `offset` bytes into the image;
// returns the samples per channel of the frames passed over.
int S_MP3_SeekFileOffset( snd_mp3_memory_t *memory, int offset );
int S_MP3_MemoryStartOffset( const snd_mp3_memory_t *memory );
int S_MP3_ReadMemory( snd_mp3_memory_t *memory, int bytes, void *pcm );
void S_MP3_CloseMemory( snd_mp3_memory_t *memory );
#endif // USE_CODEC_MP3

#endif // !_SND_CODEC_H_
