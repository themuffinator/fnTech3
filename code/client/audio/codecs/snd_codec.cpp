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
#include "../shared/JKAMusicPreload.h"

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

	if ( !quiet && ( !info || S_SoundRegistrationWarningsEnabled() ) ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: Failed to %s sound %s!\n", info ? "load" : "open", filename );
	}

	return nullptr;
}

fntech3::audio::jka::music::Cache<snd_stream_t> jkaMusicPreload;
bool jkaMusicOpening=false;
struct JKAMusicIO {
	snd_stream_t *Open(const char *name) {
		jkaMusicOpening=true;
		try {
			auto *stream=static_cast<snd_stream_t *>(S_CodecGetSound(name,nullptr));
			jkaMusicOpening=false;
			return stream;
		} catch (...) { jkaMusicOpening=false; throw; }
	}
    void Close(snd_stream_t *stream) { S_CodecCloseStream(stream); }
};

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

/*
=================
S_JKAForeignVoice

Jedi Academy ships German, French and Spanish recordings of its voices under
sound/chr_d, sound/chr_f and sound/chr_e. Both retail loaders lowercase a
sound's name first (jamp.exe 0x472726, jasp.exe 0x42dfc0); when s_language
is DEUTSCH, FRANCAIS or ESPANOL, in any case, they turn its first "chars"
into "chr_d", "chr_f" or "chr_e" and read that file, then its .mp3; only when
neither exists do they read the English .wav, then .mp3 (jamp.exe 0x4723e0,
jasp.exe 0x42dc80). Here "chars" is found in any case and the rest of the
name keeps its own.
=================
*/
bool S_JKAForeignVoice( const char *english, std::array<char, MAX_QPATH> &foreign ) {
	if ( !Com_GameIsJKASP() && !Com_GameIsJKAMP() ) {
		return false;
	}

	const char *language = Cvar_VariableString( "s_language" );
	const char *folder;
	if ( !Q_stricmp( language, "DEUTSCH" ) ) {
		folder = "chr_d";
	} else if ( !Q_stricmp( language, "FRANCAIS" ) ) {
		folder = "chr_f";
	} else if ( !Q_stricmp( language, "ESPANOL" ) ) {
		folder = "chr_e";
	} else {
		return false;
	}

	Q_strncpyz( foreign.data(), english, static_cast<int>( foreign.size() ) );
	char *voice = const_cast<char *>( Q_stristr( foreign.data(), "chars" ) );
	if ( voice == nullptr ) {
		return false;
	}

	memcpy( voice, folder, 5 );
	return true;
}

/*
=================
S_JK2SoundFile

Jedi Outcast ignores the extension a module asks for. Both retail engines
keep a sound's name cut at its first '.' (S_FindName, jk2mp.exe 0x45cc00 and
jk2sp.exe 0x423930), and S_LoadSound reads "<name>.wav" and only then
"<name>.mp3" (jk2mp.exe 0x45f050 and 0x45f520, jk2sp.exe 0x42a910 and
0x42adb0), so a .wav beside an .mp3 always wins, whichever one the name
says. Loading "<name>.wav" gives the codec list that order. The cut is made
in the file's own name: retail's would break a name with a dotted folder.
=================
*/
bool S_JK2SoundFile( const char *filename, std::array<char, MAX_QPATH> &wav ) {
	if ( !Com_GameIsJK2() ) {
		return false;
	}

	const char *leaf = filename;
	for ( const char *at = filename; *at; ++at ) {
		if ( *at == '/' || *at == '\\' ) {
			leaf = at + 1;
		}
	}
	const char *dot = strchr( leaf, '.' );
	const size_t length = dot ? static_cast<size_t>( dot - filename ) : strlen( filename );
	if ( length + sizeof( ".wav" ) > wav.size() ) {
		return false;
	}

	memcpy( wav.data(), filename, length );
	memcpy( wav.data() + length, ".wav", sizeof( ".wav" ) );
	return true;
}

/*
=================
S_JK2ForeignVoice

Jedi Outcast ships German and French recordings of its voices under
sound/chr_d and sound/chr_f. When s_language is DEUTSCH or FRANCAIS, both
retail file loaders (jk2mp.exe 0x45f520, jk2sp.exe 0x42adb0) turn the first
"chars" in a sound's name into "chr_d" or "chr_f" and read that .wav, then
.mp3; only when neither exists do they read the English ones. Multiplayer
looks for "chars" as the module wrote it; single player lowercases the name
first (jk2sp.exe 0x42a96a), so it finds any case.
=================
*/
bool S_JK2ForeignVoice( const char *english, std::array<char, MAX_QPATH> &foreign ) {
	const char *language = Cvar_VariableString( "s_language" );
	const char *folder;
	if ( !Q_stricmp( language, "DEUTSCH" ) ) {
		folder = "chr_d";
	} else if ( !Q_stricmp( language, "FRANCAIS" ) ) {
		folder = "chr_f";
	} else {
		return false;
	}

	Q_strncpyz( foreign.data(), english, static_cast<int>( foreign.size() ) );
	char *voice = Com_GameIsJK2SP() ? const_cast<char *>( Q_stristr( foreign.data(), "chars" ) )
		: strstr( foreign.data(), "chars" );
	if ( voice == nullptr ) {
		return false;
	}

	memcpy( voice, folder, 5 );
	return true;
}

} // namespace

/*
=================
S_CodecInit
=================
*/
extern "C" void S_ClearJKAMusicPreload( void ) {
	jkaMusicOpening=false;
	JKAMusicIO io;jkaMusicPreload.Clear(io);
}

extern "C" void S_CodecPreloadJKAMusic( const char *intro, const char *loop ) {
	if ( !Com_GameIsJKAMP() ) Com_Error(ERR_DROP,"Academy music preload requires its MP profile");
	fntech3::audio::jka::music::Names names;
	if ( fntech3::audio::jka::music::PrepareNames(intro,loop,names)!=fntech3::audio::jka::music::Error::None )
		Com_Error(ERR_DROP,"Invalid Academy explicit music preload");
	JKAMusicIO io;
	try { jkaMusicPreload.Prepare(io,names); }
	catch (...) {
		S_ClearJKAMusicPreload();
		Com_Error(ERR_DROP,"Academy decoder exception during music preload");
	}
	Com_DPrintf("Academy music preloaded: %d decoder(s)\n",jkaMusicPreload.Count());
}

extern "C" void S_CodecInit( void ) {
	S_ClearJKAMusicPreload();
	codecs = nullptr;

#ifdef USE_OGG_VORBIS
	S_CodecRegister( &ogg_codec );
#endif

#ifdef USE_CODEC_MP3
	// Elite Force stores most sounds and all music as MP3, and American
	// McGee's Alice all of its music; Medal of Honor its music, stereo
	// ambience and briefings. Academy also stores ambient waves, speech
	// and music as MP3, and Outcast 7,987 of its 8,296 sounds and all of
	// its music. Retail Quake and Wolfenstein engines never decode MP3, so
	// their extension fallback must not start finding .mp3 files; the
	// codec joins only those titles.
	if ( Com_GameIsSTVEF() || Com_GameIsRitual() || Com_GameIsMOHAA() ||
		Com_GameIsJKASP() || Com_GameIsJKAMP() || Com_GameIsJK2() ) {
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
	S_ClearJKAMusicPreload();
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

	std::array<char, MAX_QPATH> outcast;
	if ( S_JK2SoundFile( filename, outcast ) ) {
		std::array<char, MAX_QPATH> foreign;
		if ( S_JK2ForeignVoice( outcast.data(), foreign ) ) {
			void *rtn = S_CodecGetSound( foreign.data(), info, true );
			if ( rtn ) {
				return rtn;
			}
		}
		return S_CodecGetSound( outcast.data(), info );
	}

	std::array<char, MAX_QPATH> academy;
	if ( S_JKAForeignVoice( filename, academy ) ) {
		void *rtn = S_CodecGetSound( academy.data(), info, true );
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
	if ( Com_GameIsJKAMP() ) {
		if ( auto *stream=jkaMusicPreload.Take(filename) ) return stream;
	}
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
	if ( jkaMusicOpening && length>64*1024*1024 ) {
		Com_DPrintf("Academy music preload exceeds the 64 MiB file bound: %s\n",filename);
		return nullptr;
	}
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
