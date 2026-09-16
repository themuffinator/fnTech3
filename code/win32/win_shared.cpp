/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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

#ifdef __cplusplus
extern "C" {
#endif
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "win_local.h"
#ifdef __cplusplus
}
#endif
#include "win_raii.h"
#include "win_utf8.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <intrin.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds( void )
{
	static qboolean	initialized = qfalse;
	static DWORD sys_timeBase;
	int	sys_curtime;

	if ( !initialized ) {
		sys_timeBase = timeGetTime();
		initialized = qtrue;
	}

	sys_curtime = timeGetTime() - sys_timeBase;

	return sys_curtime;
}


/*
================
Sys_RandomBytes
================
*/
qboolean Sys_RandomBytes( byte *string, int len )
{
	fnql::win::ScopedCryptProvider prov;

	if( !CryptAcquireContext( prov.receive(), NULL, NULL,
		PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) )  {

		return qfalse;
	}

	if( !CryptGenRandom( prov.get(), len, (BYTE *)string ) )  {
		return qfalse;
	}
	return qtrue;
}


#ifdef UNICODE
LPWSTR AtoW( const char *s ) 
{
	static WCHAR buffer[MAXPRINTMSG*2];
	MultiByteToWideChar( CP_ACP, 0, s, strlen( s ) + 1, (LPWSTR) buffer, ARRAYSIZE( buffer ) );
	return buffer;
}

const char *WtoA( const LPWSTR s ) 
{
	static char buffer[MAXPRINTMSG*2];
	WideCharToMultiByte( CP_ACP, 0, s, -1, buffer, ARRAYSIZE( buffer ), NULL, NULL );
	return buffer;
}
#endif


/*
================
Sys_DefaultHomePath
================
*/
const char *Sys_DefaultHomePath( void ) 
{
#ifdef USE_PROFILES
	wchar_t szPath[MAX_PATH];
	static char path[MAX_OSPATH];
	using SHGetFolderPathWProc = HRESULT ( WINAPI * )( HWND, int, HANDLE, DWORD, LPWSTR );
	fnql::win::ScopedLibrary shfolder( LoadLibraryA( "shfolder.dll" ) );
	
	if ( !shfolder ) {
		Com_Printf("Unable to load SHFolder.dll\n");
		return NULL;
	}

	const auto qSHGetFolderPath = reinterpret_cast<SHGetFolderPathWProc>( GetProcAddress( shfolder.get(), "SHGetFolderPathW" ) );
	if ( qSHGetFolderPath == nullptr )
	{
		Com_Printf("Unable to find SHGetFolderPath in SHFolder.dll\n");
		return NULL;
	}

	if( !SUCCEEDED( qSHGetFolderPath( NULL, CSIDL_APPDATA,
		NULL, 0, szPath ) ) )
	{
		Com_Printf("Unable to detect CSIDL_APPDATA\n");
		return NULL;
	}
	if ( !fntech3::win::ToUtf8( szPath, path ) ) return NULL;
	if ( strlen( path ) + strlen( "\\Quake3" ) >= sizeof( path ) ) return NULL;
	Q_strcat( path, sizeof(path), "\\Quake3" );
	const fntech3::win::Path widePath( path );
	if ( !widePath ) return NULL;
	if( !CreateDirectoryW( widePath.c_str(), NULL ) )
	{
		if( GetLastError() != ERROR_ALREADY_EXISTS )
		{
			Com_Printf("Unable to create directory \"%s\"\n", path);
			return NULL;
		}
	}
	return path;
#else
    return NULL;
#endif
}


/*
================
Sys_SteamPath
================
*/
static qboolean Sys_RegularFileExists( const char *path )
{
	DWORD attributes;

	if ( !path || !path[0] ) {
		return qfalse;
	}

	const fntech3::win::Path widePath( path );
	if ( !widePath ) return qfalse;
	attributes = GetFileAttributesW( widePath.c_str() );
	return ( attributes != INVALID_FILE_ATTRIBUTES && !( attributes & FILE_ATTRIBUTE_DIRECTORY ) ) ? qtrue : qfalse;
}


#define SYS_STEAM_PATH_SEPARATOR_CHAR '\\'
#define SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR '/'
#define SYS_STEAM_OPEN_FILE Sys_FOpen
#include "../qcommon/steam_path_shared.h"
#undef SYS_STEAM_OPEN_FILE
#undef SYS_STEAM_PATH_SEPARATOR_CHAR
#undef SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR


static qboolean Sys_QueryRegistryString( HKEY root, const char *keyPath, REGSAM viewFlags, const char *valueName, char *out, int outSize )
{
	fnql::win::ScopedRegistryKey key;
	DWORD type;
	DWORD dataSize;
	wchar_t buffer[MAX_OSPATH] = {};
	const fntech3::win::Path wideKey( keyPath ), wideValue( valueName );
	if ( !wideKey || !wideValue || !out || outSize <= 0 ) return qfalse;

	if ( RegOpenKeyExW( root, wideKey.c_str(), 0, KEY_QUERY_VALUE | viewFlags, key.receive() ) != ERROR_SUCCESS ) {
		return qfalse;
	}

	dataSize = sizeof( buffer );
	if ( RegQueryValueExW( key.get(), wideValue.c_str(), NULL, &type, (LPBYTE)buffer, &dataSize ) != ERROR_SUCCESS ) {
		return qfalse;
	}

	if ( type != REG_SZ && type != REG_EXPAND_SZ ) {
		return qfalse;
	}

	if ( dataSize % sizeof( wchar_t ) || dataSize > sizeof( buffer ) ) return qfalse;
	if ( dataSize == sizeof( buffer ) && buffer[ARRAY_LEN( buffer ) - 1] ) return qfalse;
	if ( type == REG_EXPAND_SZ ) {
		wchar_t expanded[MAX_OSPATH];
		DWORD expandedSize;

		expandedSize = ExpandEnvironmentStringsW( buffer, expanded, ARRAY_LEN( expanded ) );
		if ( !expandedSize || expandedSize > ARRAY_LEN( expanded ) ) return qfalse;
		memcpy( buffer, expanded, expandedSize * sizeof( wchar_t ) );
	}

	char utf8[MAX_OSPATH];
	if ( !fntech3::win::ToUtf8( buffer, utf8 ) || strlen( utf8 ) >= (size_t)outSize ) return qfalse;
	Q_strncpyz( out, utf8, outSize );
	Sys_NormalizeSteamPath( out );
	return out[0] ? qtrue : qfalse;
}


static qboolean Sys_FindSteamAppInstallFromRegistry( const comGameProfile_t *profile, char *out, int outSize )
{
	char appKey[128];
	const HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
	const REGSAM views[] = { KEY_WOW64_32KEY, KEY_WOW64_64KEY, 0 };
	size_t rootIndex;
	size_t viewIndex;
	char candidate[MAX_OSPATH];

	snprintf( appKey, sizeof( appKey ), "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App %s", profile->steamAppId );

	for ( rootIndex = 0; rootIndex < ARRAY_LEN( roots ); rootIndex++ ) {
		for ( viewIndex = 0; viewIndex < ARRAY_LEN( views ); viewIndex++ ) {
			if ( Sys_QueryRegistryString( roots[rootIndex], appKey, views[viewIndex], "InstallLocation", candidate, sizeof( candidate ) )
				&& Sys_CopyValidSteamInstall( profile, candidate, out, outSize ) ) {
				return qtrue;
			}
		}
	}

	return qfalse;
}


static qboolean Sys_FindSteamRootFromRegistry( char *out, int outSize )
{
	const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
	const REGSAM views[] = { 0, KEY_WOW64_32KEY, KEY_WOW64_64KEY };
	const char *values[] = { "SteamPath", "InstallPath" };
	size_t rootIndex;
	size_t viewIndex;
	size_t valueIndex;

	for ( rootIndex = 0; rootIndex < ARRAY_LEN( roots ); rootIndex++ ) {
		for ( viewIndex = 0; viewIndex < ARRAY_LEN( views ); viewIndex++ ) {
			for ( valueIndex = 0; valueIndex < ARRAY_LEN( values ); valueIndex++ ) {
				if ( Sys_QueryRegistryString( roots[rootIndex], "Software\\Valve\\Steam", views[viewIndex], values[valueIndex], out, outSize ) ) {
					return qtrue;
				}
			}
		}
	}

	return qfalse;
}


static qboolean Sys_FindSteamRootFromKnownPaths( const comGameProfile_t *profile, char *out, int outSize )
{
	const wchar_t *variables[] = { L"ProgramFiles(x86)", L"ProgramFiles" };
	char candidate[MAX_OSPATH];

	for ( const wchar_t *variable : variables ) {
		wchar_t widePath[MAX_OSPATH];
		char path[MAX_OSPATH];
		const DWORD count = GetEnvironmentVariableW( variable, widePath, ARRAY_LEN( widePath ) );
		if ( !count || count >= ARRAY_LEN( widePath ) || !fntech3::win::ToUtf8( widePath, path ) ) continue;
		Sys_SteamJoinPath( candidate, sizeof( candidate ), path, "Steam" );
		if ( Sys_SteamAppPathFromRoot( profile, candidate, out, outSize ) ) {
			return qtrue;
		}
	}

	return Sys_SteamAppPathFromRoot( profile, "C:\\Program Files (x86)\\Steam", out, outSize );
}


const char *Sys_SteamPath( comGameId_t game )
{
	static char steamPaths[COM_GAME_COUNT][MAX_OSPATH];
	const comGameProfile_t *profile = Com_GameProfileById( game );
	char *steamPath;
	char steamRoot[MAX_OSPATH];

	if ( !profile ) {
		return "";
	}
	steamPath = steamPaths[profile->id];

	if ( steamPath[0] ) {
		return steamPath;
	}

	if ( Sys_FindSteamAppInstallFromRegistry( profile, steamPath, MAX_OSPATH ) ) {
		return steamPath;
	}

	if ( Sys_FindSteamRootFromRegistry( steamRoot, sizeof( steamRoot ) )
		&& Sys_SteamAppPathFromRoot( profile, steamRoot, steamPath, MAX_OSPATH ) ) {
		return steamPath;
	}

	Sys_FindSteamRootFromKnownPaths( profile, steamPath, MAX_OSPATH );
	return steamPath;
}


/*
================
Sys_QuakeLiveProfilePath

Retail Quake Live stores mutable files and extracted native modules below a
SteamID-named directory in the install root. Resolve the active account from
Steam's registry state without making Steamworks a filesystem dependency.
================
*/
const char *Sys_QuakeLiveProfilePath( const char *installPath )
{
	static char profilePath[MAX_OSPATH];
	fnql::win::ScopedRegistryKey key;
	DWORD activeUser;
	DWORD activeUserSize;
	DWORD type;
	unsigned long long steamId;
	char profileName[32];

	profilePath[0] = '\0';
	if ( !installPath || !installPath[0] ) {
		return profilePath;
	}

	if ( RegOpenKeyExA( HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess",
		0, KEY_QUERY_VALUE, key.receive() ) != ERROR_SUCCESS ) {
		return profilePath;
	}

	activeUser = 0;
	activeUserSize = sizeof( activeUser );
	type = 0;
	if ( RegQueryValueExA( key.get(), "ActiveUser", NULL, &type,
		reinterpret_cast<LPBYTE>( &activeUser ), &activeUserSize ) != ERROR_SUCCESS ||
		type != REG_DWORD || activeUserSize != sizeof( activeUser ) || activeUser == 0 ) {
		return profilePath;
	}

	/* SteamID64 individual accounts use universe/type/instance 0x01100001
	 * followed by the 32-bit account ID exposed as ActiveUser. */
	steamId = 76561197960265728ull + (unsigned long long)activeUser;
	Com_sprintf( profileName, sizeof( profileName ), "%llu", steamId );
	Sys_SteamJoinPath( profilePath, sizeof( profilePath ), installPath, profileName );
	return profilePath;
}


/*
================
Sys_SetAffinityMask
================
*/
#ifdef USE_AFFINITY_MASK
static HANDLE hCurrentProcess = 0;

uint64_t Sys_GetAffinityMask( void )
{
	DWORD_PTR dwProcessAffinityMask;
	DWORD_PTR dwSystemAffinityMask;

	if ( hCurrentProcess == 0 )	{
		hCurrentProcess = GetCurrentProcess();
	}

	if ( GetProcessAffinityMask( hCurrentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask ) )	{
		return (uint64_t)dwProcessAffinityMask;
	}

	return 0;
}


qboolean Sys_SetAffinityMask( const uint64_t mask )
{
	DWORD_PTR dwProcessAffinityMask = (DWORD_PTR)mask;

	if ( hCurrentProcess == 0 ) {
		hCurrentProcess = GetCurrentProcess();
	}

	if ( SetProcessAffinityMask( hCurrentProcess, dwProcessAffinityMask ) )	{
		//Sleep( 0 );
		return qtrue;
	}

	return qfalse;
}
#endif // USE_AFFINITY_MASK

#ifdef __cplusplus
}
#endif
