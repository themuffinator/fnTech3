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

using fnql::CloseFile;
using fnql::AllocateZoneMemory;
using fnql::OpenFileRead;
using fnql::ScopedFileHandle;
using fnql::ScopedZoneMemory;

namespace {

snd_codec_t *codecs = nullptr;

void S_CodecRegister( snd_codec_t *codec ) {
	codec->next = codecs;
	codecs = codec;
}

/*
=================
S_CodecGetSound

Opens/loads a sound, tries codec based on the sound's file extension
then tries all supported codecs.
=================
*/
void *S_CodecGetSound( const char *filename, snd_info_t *info, bool quiet = false ) {
	snd_codec_t *codec;
	snd_codec_t *orgCodec = nullptr;
	bool orgNameFailed = false;
	std::array<char, MAX_QPATH> localName;
	const char *ext;
	std::array<char, MAX_QPATH> altName;
	void *rtn = nullptr;

	Q_strncpyz( localName.data(), filename, static_cast<int>( localName.size() ) );

	ext = COM_GetExtension( localName.data() );

	if ( *ext ) {
		// Look for the correct loader and use it.
		for ( codec = codecs; codec; codec = codec->next ) {
			if ( !Q_stricmp( ext, codec->ext ) ) {
				if ( info ) {
					rtn = codec->load( localName.data(), info );
				} else {
					rtn = codec->open( localName.data() );
				}
				break;
			}
		}

		if ( codec ) {
			if ( !rtn ) {
				// Loader failed, most likely because the file is not present;
				// try again without the extension.
				orgNameFailed = true;
				orgCodec = codec;
				COM_StripExtension( filename, localName.data(), static_cast<int>( localName.size() ) );
			} else {
				return rtn;
			}
		}
	}

	// Try all supported codecs.
	for ( codec = codecs; codec; codec = codec->next ) {
		if ( codec == orgCodec ) {
			continue;
		}

		Com_sprintf( altName.data(), static_cast<int>( altName.size() ), "%s.%s", localName.data(), codec->ext );

		if ( info ) {
			rtn = codec->load( altName.data(), info );
		} else {
			rtn = codec->open( altName.data() );
		}

		if ( rtn ) {
			if ( orgNameFailed ) {
				Com_DPrintf( S_COLOR_CYAN "Sound fallback: %s -> %s\n",
					filename, altName.data() );
			}

			return rtn;
		}
	}

	if ( !quiet ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: Failed to %s sound %s!\n", info ? "load" : "open", filename );
	}

	return nullptr;
}

/*
=================
S_STVEFGermanVoice

Elite Force ships German recordings of its voices under sound/vox_d. When
s_language is DEUTSCH, retail S_LoadSound (stvoyHM.exe 0x42c690, stvoy.exe
0x4232f3) turns the first "voice/" in a sound's name into "vox_d/" and loads
that .wav or .mp3; only when neither exists does it load the name as given.
=================
*/
bool S_STVEFGermanVoice( const char *filename, std::array<char, MAX_QPATH> &german ) {
	if ( !Com_GameIsSTVEF() || Q_stricmp( Cvar_VariableString( "s_language" ), "DEUTSCH" ) ) {
		return false;
	}

	Q_strncpyz( german.data(), filename, static_cast<int>( german.size() ) );
	char *voice = const_cast<char *>( Q_stristr( german.data(), "voice/" ) );
	if ( voice == nullptr ) {
		return false;
	}

	memcpy( voice, "vox_d", 5 );
	return true;
}

} // namespace

/*
=================
S_CodecInit
=================
*/
extern "C" void S_CodecInit( void ) {
	codecs = nullptr;

#ifdef USE_OGG_VORBIS
	S_CodecRegister( &ogg_codec );
#endif

#ifdef USE_CODEC_MP3
	// Elite Force stores most sounds and all music as MP3, and American
	// McGee's Alice all of its music; Medal of Honor its music, stereo
	// ambience and briefings. Retail Quake and Wolfenstein engines never
	// decode MP3, so their extension fallback must not start finding .mp3
	// files; the codec joins only those titles.
	if ( Com_GameIsSTVEF() || Com_GameIsRitual() || Com_GameIsMOHAA() ) {
		S_CodecRegister( &mp3_codec );
	}
#endif

	// Register wav last so it is tried first when a file extension was not found.
	S_CodecRegister( &wav_codec );
}

/*
=================
S_CodecShutdown
=================
*/
extern "C" void S_CodecShutdown( void ) {
	codecs = nullptr;
}

/*
=================
S_CodecLoad
=================
*/
extern "C" void *S_CodecLoad( const char *filename, snd_info_t *info ) {
	std::array<char, MAX_QPATH> german;
	if ( S_STVEFGermanVoice( filename, german ) ) {
		void *rtn = S_CodecGetSound( german.data(), info, true );
		if ( rtn ) {
			return rtn;
		}
	}

	return S_CodecGetSound( filename, info );
}

/*
=================
S_CodecOpenStream
=================
*/
extern "C" snd_stream_t *S_CodecOpenStream( const char *filename ) {
	return static_cast<snd_stream_t *>( S_CodecGetSound( filename, nullptr ) );
}

/*
=================
S_CodecCloseStream
=================
*/
extern "C" void S_CodecCloseStream( snd_stream_t *stream ) {
	stream->codec->close( stream );
}

/*
=================
S_CodecReadStream
=================
*/
extern "C" int S_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer ) {
	return stream->codec->read( stream, bytes, buffer );
}

//=======================================================================
// Util functions (used by codecs)

/*
=================
S_CodecUtilOpen
=================
*/
extern "C" snd_stream_t *S_CodecUtilOpen( const char *filename, snd_codec_t *codec ) {
	ScopedFileHandle file;
	const int length = OpenFileRead( filename, file, qtrue );
	if ( !file ) {
		return nullptr;
	}

	ScopedZoneMemory streamStorage = AllocateZoneMemory( sizeof( snd_stream_t ), "snd_stream_t", __FILE__, __LINE__ );
	auto *stream = streamStorage.as<snd_stream_t>();
	if ( !stream ) {
		return nullptr;
	}

	stream->codec = codec;
	stream->file = file.release();
	stream->length = length;
	return static_cast<snd_stream_t *>( streamStorage.release() );
}

/*
=================
S_CodecUtilClose
=================
*/
extern "C" void S_CodecUtilClose( snd_stream_t **stream ) {
	CloseFile( ( *stream )->file );
	Z_Free( *stream );
	*stream = nullptr;
}
