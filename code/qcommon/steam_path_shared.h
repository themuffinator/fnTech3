/*
===========================================================================
Shared Steam library discovery helpers for fnTech3 profile-aware platform bootstrap.
Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
===========================================================================
*/

#ifndef FNQL_STEAM_PATH_SHARED_H
#define FNQL_STEAM_PATH_SHARED_H

#ifndef SYS_STEAM_PATH_SEPARATOR_CHAR
#error "SYS_STEAM_PATH_SEPARATOR_CHAR must be defined before including steam_path_shared.h"
#endif

#ifndef SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR
#error "SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR must be defined before including steam_path_shared.h"
#endif

typedef enum {
	SYS_VDF_TOKEN_NONE,
	SYS_VDF_TOKEN_ERROR,
	SYS_VDF_TOKEN_STRING,
	SYS_VDF_TOKEN_OPEN_BRACE,
	SYS_VDF_TOKEN_CLOSE_BRACE
} sysVdfToken_t;


static qboolean Sys_SteamPathSeparator( char c )
{
	return ( c == SYS_STEAM_PATH_SEPARATOR_CHAR || c == SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR ) ? qtrue : qfalse;
}


static void Sys_NormalizeSteamPath( char *path )
{
	char *cursor;
	size_t len;

	if ( !path || !path[0] ) {
		return;
	}

	for ( cursor = path; *cursor; cursor++ ) {
		if ( *cursor == SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR ) {
			*cursor = SYS_STEAM_PATH_SEPARATOR_CHAR;
		}
	}

	len = strlen( path );
	while ( len > 1 && Sys_SteamPathSeparator( path[len - 1] )
		&& !( len == 3 && path[1] == ':' ) ) {
		path[--len] = '\0';
	}
}


static qboolean Sys_SteamJoinPath( char *out, int outSize, const char *base, const char *leaf )
{
	char separator[2];

	if ( !out || outSize <= 0 ) {
		return qfalse;
	}

	out[0] = '\0';
	if ( !base || !base[0] ) {
		return qfalse;
	}
	const size_t baseLength = strlen( base );
	const size_t leafLength = leaf ? strlen( leaf ) : 0;
	const size_t separatorLength = leafLength && !Sys_SteamPathSeparator( base[baseLength - 1] ) ? 1 : 0;
	if ( baseLength + leafLength + separatorLength + 1 > (size_t)outSize ) {
		return qfalse;
	}

	Q_strncpyz( out, base, outSize );
	Sys_NormalizeSteamPath( out );

	if ( leaf && leaf[0] ) {
		if ( out[0] && !Sys_SteamPathSeparator( out[strlen( out ) - 1] ) ) {
			separator[0] = SYS_STEAM_PATH_SEPARATOR_CHAR;
			separator[1] = '\0';
			Q_strcat( out, outSize, separator );
		}
		Q_strcat( out, outSize, leaf );
		Sys_NormalizeSteamPath( out );
	}
	return qtrue;
}


static void Sys_SteamJoinSteamAppsPath( char *out, int outSize, const char *libraryPath, const char *leaf )
{
	char steamAppsPath[MAX_OSPATH];

	Sys_SteamJoinPath( steamAppsPath, sizeof( steamAppsPath ), libraryPath, "steamapps" );
	Sys_SteamJoinPath( out, outSize, steamAppsPath, leaf );
}


static qboolean Sys_SteamInstallIsValid( const comGameProfile_t *profile, const char *installPath )
{
	char basePath[MAX_OSPATH];
	char assetPath[MAX_OSPATH];

	Sys_SteamJoinPath( basePath, sizeof( basePath ), installPath, profile->baseGame );
	Sys_SteamJoinPath( assetPath, sizeof( assetPath ), basePath, profile->basePakName );

	return Sys_RegularFileExists( assetPath );
}


static qboolean Sys_CopyValidSteamInstall( const comGameProfile_t *profile, const char *path, char *out, int outSize )
{
	char normalized[MAX_OSPATH];

	if ( !path || !path[0] || !out || outSize <= 0
		|| strlen( path ) >= sizeof( normalized ) || strlen( path ) >= (size_t)outSize ) {
		return qfalse;
	}

	Q_strncpyz( normalized, path, sizeof( normalized ) );
	Sys_NormalizeSteamPath( normalized );

	if ( !Sys_SteamInstallIsValid( profile, normalized ) ) {
		return qfalse;
	}

	Q_strncpyz( out, normalized, outSize );
	return qtrue;
}


static char *Sys_ReadTextFile( const char *path )
{
	FILE *file;
	long length;
	char *buffer;
	size_t bytesRead;

	file = fopen( path, "rb" );
	if ( !file ) {
		return NULL;
	}

	if ( fseek( file, 0, SEEK_END ) != 0 ) {
		fclose( file );
		return NULL;
	}

	length = ftell( file );
	/* Steam metadata is small. Bound allocations before parsing untrusted text. */
	if ( length < 0 || length > 1024 * 1024 ) {
		fclose( file );
		return NULL;
	}

	if ( fseek( file, 0, SEEK_SET ) != 0 ) {
		fclose( file );
		return NULL;
	}

	buffer = (char *)malloc( (size_t)length + 1 );
	if ( !buffer ) {
		fclose( file );
		return NULL;
	}

	bytesRead = fread( buffer, 1, (size_t)length, file );
	fclose( file );
	if ( bytesRead != (size_t)length || memchr( buffer, '\0', bytesRead ) ) {
		free( buffer );
		return NULL;
	}
	buffer[bytesRead] = '\0';
	return buffer;
}


/* Reject truncated tokens instead of treating their prefixes as paths/keys. */
static sysVdfToken_t Sys_ReadVdfToken( const char **input, char *token, int tokenSize )
{
	const char *cursor = *input;
	int len = 0;
	qboolean quoted;
	if ( tokenSize <= 0 ) {
		return SYS_VDF_TOKEN_ERROR;
	}
	token[0] = '\0';
	for ( ;; ) {
		while ( *cursor && (unsigned char)*cursor <= ' ' ) {
			cursor++;
		}
		if ( cursor[0] != '/' || cursor[1] != '/' ) {
			break;
		}
		while ( *cursor && *cursor != '\n' ) {
			cursor++;
		}
	}
	*input = cursor;
	if ( !*cursor ) {
		return SYS_VDF_TOKEN_NONE;
	}
	if ( *cursor == '{' || *cursor == '}' ) {
		*input = cursor + 1;
		return *cursor == '{' ? SYS_VDF_TOKEN_OPEN_BRACE : SYS_VDF_TOKEN_CLOSE_BRACE;
	}
	quoted = *cursor == '"' ? qtrue : qfalse;
	if ( quoted ) {
		cursor++;
	}
	while ( *cursor ) {
		char value = *cursor;
		if ( quoted ? value == '"' : ( (unsigned char)value <= ' ' || value == '{' || value == '}' ) ) {
			break;
		}
		if ( quoted && value == '\\' && ( cursor[1] == '\\' || cursor[1] == '"' ) ) {
			value = *++cursor;
		}
		if ( len >= tokenSize - 1 ) {
			return SYS_VDF_TOKEN_ERROR;
		}
		token[len++] = value;
		cursor++;
	}
	if ( quoted ) {
		if ( *cursor != '"' ) {
			return SYS_VDF_TOKEN_ERROR;
		}
		cursor++;
	}
	token[len] = '\0';
	*input = cursor;
	return SYS_VDF_TOKEN_STRING;
}

static qboolean Sys_SkipVdfObject( const char **cursor, int depth )
{
	char token[MAX_OSPATH];
	sysVdfToken_t type;
	if ( depth > 16 ) {
		return qfalse;
	}
	for ( ;; ) {
		type = Sys_ReadVdfToken( cursor, token, sizeof( token ) );
		if ( type == SYS_VDF_TOKEN_CLOSE_BRACE ) {
			return qtrue;
		}
		if ( type != SYS_VDF_TOKEN_STRING ) {
			return qfalse;
		}
		type = Sys_ReadVdfToken( cursor, token, sizeof( token ) );
		if ( type == SYS_VDF_TOKEN_OPEN_BRACE ) {
			if ( !Sys_SkipVdfObject( cursor, depth + 1 ) ) {
				return qfalse;
			}
		} else if ( type != SYS_VDF_TOKEN_STRING ) {
			return qfalse;
		}
	}
}

static qboolean Sys_VdfHeader( const char **cursor, const char *name )
{
	char token[MAX_OSPATH];
	return Sys_ReadVdfToken( cursor, token, sizeof( token ) ) == SYS_VDF_TOKEN_STRING
		&& !Q_stricmp( token, name )
		&& Sys_ReadVdfToken( cursor, token, sizeof( token ) ) == SYS_VDF_TOKEN_OPEN_BRACE ? qtrue : qfalse;
}

static qboolean Sys_SteamInstallDirIsValid( const char *name )
{
	/* installdir is one child of steamapps/common, never an arbitrary path. */
	const size_t len = strlen( name );
	for ( const unsigned char *c = (const unsigned char *)name; *c; ++c ) {
		if ( *c < 32 ) {
			return qfalse;
		}
	}
	return len && strcmp( name, "." ) && strcmp( name, ".." )
		&& !strpbrk( name, "/\\:*?\"<>|\r\n\t" )
		&& name[len - 1] != '.' && name[len - 1] != ' ' ? qtrue : qfalse;
}

static qboolean Sys_SteamManifestInstallDir( const comGameProfile_t *profile,
	const char *libraryPath, char *installDir, int installDirSize )
{
	char manifestPath[MAX_OSPATH], manifestName[64], key[MAX_OSPATH], value[MAX_OSPATH];
	char *text;
	const char *cursor;
	qboolean appSeen = qfalse, dirSeen = qfalse, valid = qfalse;
	sysVdfToken_t type;
	if ( !installDir || installDirSize <= 0 ) {
		return qfalse;
	}
	installDir[0] = '\0';
	if ( !profile || !libraryPath || !libraryPath[0] ) {
		return qfalse;
	}
	snprintf( manifestName, sizeof( manifestName ), "appmanifest_%s.acf", profile->steamAppId );
	Sys_SteamJoinSteamAppsPath( manifestPath, sizeof( manifestPath ), libraryPath, manifestName );
	text = Sys_ReadTextFile( manifestPath );
	if ( !text ) {
		return qfalse;
	}
	cursor = text;
	if ( !Sys_VdfHeader( &cursor, "AppState" ) ) {
		free( text );
		return qfalse;
	}
	for ( ;; ) {
		type = Sys_ReadVdfToken( &cursor, key, sizeof( key ) );
		if ( type == SYS_VDF_TOKEN_CLOSE_BRACE ) {
			valid = appSeen && dirSeen
				&& Sys_ReadVdfToken( &cursor, key, sizeof( key ) ) == SYS_VDF_TOKEN_NONE ? qtrue : qfalse;
			break;
		}
		if ( type != SYS_VDF_TOKEN_STRING ) {
			break;
		}
		type = Sys_ReadVdfToken( &cursor, value, sizeof( value ) );
		if ( !Q_stricmp( key, "appid" ) ) {
			if ( appSeen || type != SYS_VDF_TOKEN_STRING || strcmp( value, profile->steamAppId ) ) {
				break;
			}
			appSeen = qtrue;
		} else if ( !Q_stricmp( key, "installdir" ) ) {
			if ( dirSeen || type != SYS_VDF_TOKEN_STRING || !Sys_SteamInstallDirIsValid( value )
				|| strlen( value ) >= (size_t)installDirSize ) {
				break;
			}
			Q_strncpyz( installDir, value, installDirSize );
			dirSeen = qtrue;
		} else if ( type == SYS_VDF_TOKEN_OPEN_BRACE ) {
			if ( !Sys_SkipVdfObject( &cursor, 1 ) ) {
				break;
			}
		} else if ( type != SYS_VDF_TOKEN_STRING ) {
			break;
		}
	}
	free( text );
	if ( !valid ) {
		installDir[0] = '\0';
	}
	return valid;
}

static qboolean Sys_SteamAppPathFromLibrary( const comGameProfile_t *profile,
	const char *libraryPath, char *out, int outSize )
{
	char installDir[MAX_OSPATH], commonPath[MAX_OSPATH], candidate[MAX_OSPATH];
	if ( !libraryPath || !libraryPath[0] ) {
		return qfalse;
	}
	Sys_SteamJoinSteamAppsPath( commonPath, sizeof( commonPath ), libraryPath, "common" );
	if ( Sys_SteamManifestInstallDir( profile, libraryPath, installDir, sizeof( installDir ) )
		&& Sys_SteamJoinPath( candidate, sizeof( candidate ), commonPath, installDir )
		&& Sys_CopyValidSteamInstall( profile, candidate, out, outSize ) ) {
		return qtrue;
	}
	return Sys_SteamJoinPath( candidate, sizeof( candidate ), commonPath, profile->steamInstallName )
		&& Sys_CopyValidSteamInstall( profile, candidate, out, outSize ) ? qtrue : qfalse;
}

static qboolean Sys_SteamLibraryIndex( const char *key )
{
	if ( !*key ) {
		return qfalse;
	}
	for ( ; *key; ++key ) {
		if ( *key < '0' || *key > '9' ) {
			return qfalse;
		}
	}
	return qtrue;
}

static qboolean Sys_SteamAppPathFromLibraryFolders( const comGameProfile_t *profile,
	const char *steamRoot, char *out, int outSize )
{
	char libraryFile[MAX_OSPATH], libraryPath[MAX_OSPATH], selected[MAX_OSPATH] = "";
	char key[MAX_OSPATH], value[MAX_OSPATH];
	char *text;
	const char *cursor;
	qboolean valid = qfalse;
	sysVdfToken_t type;
	Sys_SteamJoinSteamAppsPath( libraryFile, sizeof( libraryFile ), steamRoot, "libraryfolders.vdf" );
	text = Sys_ReadTextFile( libraryFile );
	if ( !text ) {
		return qfalse;
	}
	cursor = text;
	if ( !Sys_VdfHeader( &cursor, "libraryfolders" ) ) {
		free( text );
		return qfalse;
	}
	for ( ;; ) {
		qboolean index;
		type = Sys_ReadVdfToken( &cursor, key, sizeof( key ) );
		if ( type == SYS_VDF_TOKEN_CLOSE_BRACE ) {
			valid = Sys_ReadVdfToken( &cursor, key, sizeof( key ) ) == SYS_VDF_TOKEN_NONE ? qtrue : qfalse;
			break;
		}
		if ( type != SYS_VDF_TOKEN_STRING ) {
			break;
		}
		index = Sys_SteamLibraryIndex( key );
		type = Sys_ReadVdfToken( &cursor, value, sizeof( value ) );
		libraryPath[0] = '\0';
		if ( type == SYS_VDF_TOKEN_STRING ) {
			if ( index ) {
				Q_strncpyz( libraryPath, value, sizeof( libraryPath ) );
			}
		} else if ( type == SYS_VDF_TOKEN_OPEN_BRACE && index ) {
			qboolean pathSeen = qfalse;
			for ( ;; ) {
				type = Sys_ReadVdfToken( &cursor, key, sizeof( key ) );
				if ( type != SYS_VDF_TOKEN_STRING ) {
					break;
				}
				type = Sys_ReadVdfToken( &cursor, value, sizeof( value ) );
				if ( !Q_stricmp( key, "path" ) ) {
					if ( pathSeen || type != SYS_VDF_TOKEN_STRING ) {
						type = SYS_VDF_TOKEN_ERROR;
						break;
					}
					Q_strncpyz( libraryPath, value, sizeof( libraryPath ) );
					pathSeen = qtrue;
				} else if ( type == SYS_VDF_TOKEN_OPEN_BRACE ) {
					if ( !Sys_SkipVdfObject( &cursor, 1 ) ) {
						type = SYS_VDF_TOKEN_ERROR;
						break;
					}
				} else if ( type != SYS_VDF_TOKEN_STRING ) {
					break;
				}
			}
			if ( type != SYS_VDF_TOKEN_CLOSE_BRACE ) {
				break;
			}
		} else if ( type == SYS_VDF_TOKEN_OPEN_BRACE ) {
			if ( !Sys_SkipVdfObject( &cursor, 1 ) ) {
				break;
			}
		} else {
			break;
		}
		if ( libraryPath[0] && !selected[0] ) {
			Sys_NormalizeSteamPath( libraryPath );
			Sys_SteamAppPathFromLibrary( profile, libraryPath, selected, sizeof( selected ) );
		}
	}
	free( text );
	return valid && selected[0] && Sys_CopyValidSteamInstall( profile, selected, out, outSize ) ? qtrue : qfalse;
}

static qboolean Sys_SteamAppPathFromRoot( const comGameProfile_t *profile,
	const char *steamRoot, char *out, int outSize )
{
	if ( !out || outSize <= 0 ) {
		return qfalse;
	}
	out[0] = '\0';
	if ( !profile || !steamRoot || !steamRoot[0] ) {
		return qfalse;
	}
	return Sys_SteamAppPathFromLibraryFolders( profile, steamRoot, out, outSize )
		|| Sys_SteamAppPathFromLibrary( profile, steamRoot, out, outSize ) ? qtrue : qfalse;
}

#endif // FNQL_STEAM_PATH_SHARED_H
