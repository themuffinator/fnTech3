/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* Alice's lip-sync files, served to both of its modules.
 *
 * The game's SoundLength and SoundAmplitudes imports and the client game's
 * get_lip_length and get_lip_amplitudes are the same two engine functions in
 * retail alice.exe (0x0043f100 and 0x0043f170). Each looks the sound's name up
 * in a ring of sixty-four records -- the ring runs from 0x0068a500 to its
 * count at 0x00693700 in 584-byte steps -- and on a miss reads the `.lip`
 * file that sits beside the sound, replacing the sound's four-character
 * extension. A record is the name, the length as a float, the amplitude
 * count, and 512 amplitude bytes.
 *
 * The file is text: the spoken length in seconds on the first line, the
 * number of amplitudes on the second, then one amplitude a line. Retail
 * warns "Too many lip sync amplitudes" past 512 and keeps the first 512. */

#include "alice_runtime.h"
#include "qcommon.h"

#include <cstring>

namespace {

constexpr int kRingSize = 64;

struct LipRecord {
	char name[MAX_QPATH];
	float length;
	int count;
	byte amplitudes[ALICE_LIP_MAX_AMPLITUDES];
};

LipRecord lipRing[kRingSize];
int lipUsed;
int lipNext;

const LipRecord *Alice_FindLip( const char *soundName ) {
	for ( int i = 0; i < lipUsed; i++ ) {
		if ( !Q_stricmp( lipRing[i].name, soundName ) ) {
			return &lipRing[i];
		}
	}
	return nullptr;
}

const LipRecord *Alice_LoadLip( const char *soundName ) {
	char path[MAX_QPATH];
	union {
		char *c;
		void *v;
	} buffer;

	/* Retail swaps the last four characters, whatever they are, for ".lip". */
	const size_t length = strlen( soundName );
	if ( length < 5 || length >= sizeof( path ) ) {
		return nullptr;
	}
	Q_strncpyz( path, soundName, (int)( length - 3 ) );
	Q_strcat( path, sizeof( path ), ".lip" );

	const int bytes = FS_ReadFile( path, &buffer.v );
	if ( bytes <= 0 || !buffer.c ) {
		return nullptr;
	}

	LipRecord &record = lipRing[lipNext];
	lipNext = ( lipNext + 1 ) % kRingSize;
	if ( lipUsed < kRingSize ) {
		lipUsed++;
	}
	record = {};
	Q_strncpyz( record.name, soundName, sizeof( record.name ) );

	const char *text = buffer.c;
	const char *token = COM_Parse( &text );
	record.length = Q_atof( token );
	token = COM_Parse( &text );
	int count = atoi( token );
	if ( count < 0 ) {
		count = 0;
	}
	if ( count > ALICE_LIP_MAX_AMPLITUDES ) {
		Com_Printf( "Too many lip sync amplitudes in %s\n", path );
		count = ALICE_LIP_MAX_AMPLITUDES;
	}
	for ( int i = 0; i < count; i++ ) {
		token = COM_Parse( &text );
		if ( !token[0] ) {
			count = i;
			break;
		}
		const int amplitude = atoi( token );
		record.amplitudes[i] = (byte)( amplitude < 0 ? 0 : amplitude > 255 ? 255 : amplitude );
	}
	record.count = count;
	FS_FreeFile( buffer.v );
	return &record;
}

const LipRecord *Alice_Lip( const char *soundName ) {
	if ( !soundName || !soundName[0] ) {
		return nullptr;
	}
	const LipRecord *record = Alice_FindLip( soundName );
	return record ? record : Alice_LoadLip( soundName );
}

} // namespace

float Alice_LipLength( const char *soundName ) {
	const LipRecord *record = Alice_Lip( soundName );
	return record ? record->length : -1.0f;
}

const byte *Alice_LipAmplitudes( const char *soundName, int *count ) {
	const LipRecord *record = Alice_Lip( soundName );
	if ( count ) {
		*count = record ? record->count : 0;
	}
	return record ? record->amplitudes : nullptr;
}

void Alice_LipClear( void ) {
	lipUsed = 0;
	lipNext = 0;
}
