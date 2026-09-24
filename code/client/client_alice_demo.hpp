/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_ALICE_DEMO_HPP
#define FNTECH3_CLIENT_ALICE_DEMO_HPP

/* American McGee's Alice's client demo names, as retail alice.exe builds them.
 *
 * Retail registers `record` (CL_Record_f 0x0042a410), `demo` (CL_PlayDemo_f
 * 0x0042a890) and `stoprecord` (CL_StopRecord_f 0x0042a2e0) at 0x0042cbaa,
 * 0x0042cbb9 and 0x0042cbd7. Every file they name carries one fixed extension,
 * from the format `demos/%s.dm3` at 0x0050f314, where Quake III 1.27 and later
 * name a demo after its protocol (`.dm_NN`). The file inside is Quake III's
 * container: a gamestate record the recorder synthesizes, then every message
 * the client parses, each as a little-endian sequence, a little-endian length
 * and the bytes, and two -1 words to close it.
 *
 * Nothing here touches the filesystem; the caller passes how a name is
 * probed, so the rules can be held to in a test. See
 * docs/fntech3/2026-09-22-alice-wire.md. */

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace fntech3::alice::demo {

/* The one extension, for recording and playback alike. */
inline constexpr const char *kExtension = "dm3";

/* clc.demoName runs from 0x00c178a4 to clc.demorecording at 0x00c178e4, and
 * CL_Record_f copies the argument into a buffer of the same 64 bytes
 * (Q_strncpyz( .., .., 0x40 ) at 0x0042a4be) before it formats a path. */
inline constexpr std::size_t kNameBytes = 64;

/* Every path is formatted into MAX_OSPATH, 0x80 bytes, in this engine. */
inline constexpr std::size_t kPathBytes = 128;

/* The automatic name search stops at 9,999 (cmp esi, 0x270f at 0x0042a51f). */
inline constexpr int kLastAutomaticNumber = 9999;

/* CL_DemoFilename at 0x0042a360: four decimal digits after "demo", and
 * "demo9999.tga" for a number out of range -- a leftover of the screenshot
 * namer the search below never reaches. */
inline void AutomaticName( int number, char *out, std::size_t size ) {
	if ( number < 0 || number > kLastAutomaticNumber ) {
		std::snprintf( out, size, "demo9999.tga" );
		return;
	}
	std::snprintf( out, size, "demo%i%i%i%i", number / 1000, number / 100 % 10,
		number / 10 % 10, number % 10 );
}

/* The recorder's path, "demos/%s.dm3" (0x0042a4c8 and 0x0042a4f3). */
inline void RecordPath( const char *name, char *out, std::size_t size ) {
	std::snprintf( out, size, "demos/%s.%s", name, kExtension );
}

/* An explicit `record <name>` keeps the name as typed, cut to 63 characters
 * like Q_strncpyz into 64 bytes; retail strips no extension, so
 * `record x.dm3` writes demos/x.dm3.dm3 (0x0042a4a5..0x0042a4df). */
inline void ExplicitName( const char *argument, char name[kNameBytes] ) {
	std::snprintf( name, kNameBytes, "%s", argument );
}

/* `record` with no name takes the first demoNNNN whose file does not read
 * back as longer than nothing (FS_ReadFile( name, NULL ) <= 0 at 0x0042a50e).
 * When every number is taken the loop leaves demo9999, which the recorder
 * then overwrites (0x0042a4e1..0x0042a525). */
template<class Length>
void AutomaticRecordName( Length length, char name[kNameBytes], char *path,
	std::size_t pathSize ) {
	for ( int number = 0; number <= kLastAutomaticNumber; ++number ) {
		AutomaticName( number, name, kNameBytes );
		RecordPath( name, path, pathSize );
		if ( length( path ) <= 0 ) {
			return;
		}
	}
}

/* CL_PlayDemo_f: an argument that contains ".dm3" anywhere names the file
 * itself, otherwise the extension is appended -- strstr( arg, ".dm3" ) at
 * 0x0042a8dd, then "demos/%s" (0x0050f444) or "demos/%s.dm3". The test is
 * case sensitive, as strstr is. */
inline void PlaybackPath( const char *argument, char *out, std::size_t size ) {
	if ( std::strstr( argument, ".dm3" ) ) {
		std::snprintf( out, size, "demos/%s", argument );
	} else {
		std::snprintf( out, size, "demos/%s.%s", argument, kExtension );
	}
}

/* Retail refuses to record unless this cvar is set, and spells it without the
 * h (0x0050f360, read at 0x0042a476); the cgame registers it under the same
 * spelling with "0" (cgamex86.dll 0x30023a7c). It is a refusal, not Quake III
 * 1.32's warning. */
inline constexpr const char *kSynchronousClientsCvar = "g_syncronousClients";
inline constexpr const char *kSynchronousClientsRefusal =
	"The server must have 'g_syncronousClients 1' set for demos\n";

} // namespace fntech3::alice::demo

#endif
