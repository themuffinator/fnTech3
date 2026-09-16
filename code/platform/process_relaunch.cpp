/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "process_relaunch.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __linux__
#include <dirent.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace {
struct RelaunchPlan {
#ifdef _WIN32
	wchar_t image[32768];
	wchar_t command[32768];
#else
	char image[PATH_MAX];
	char command[SYS_RELAUNCH_COMMAND_BYTES];
#endif
};
RelaunchPlan pending;
bool prepared;

int Fail( char *error, unsigned capacity, const char *message, unsigned long code = 0 )
{
	if ( error && capacity ) {
		if ( code ) std::snprintf( error, capacity, "%s (system error %lu)", message, code );
		else std::snprintf( error, capacity, "%s", message );
	}
	return 0;
}

#ifndef _WIN32
bool CloseDescriptorOnExec( int descriptor )
{
	int flags;
	do { flags = fcntl( descriptor, F_GETFD ); } while ( flags < 0 && errno == EINTR );
	if ( flags < 0 ) return errno == EBADF;
	int result;
	do { result = fcntl( descriptor, F_SETFD, flags | FD_CLOEXEC ); } while ( result < 0 && errno == EINTR );
	return result >= 0 || errno == EBADF;
}

bool CloseDescriptorsOnExec()
{
	// Mark, rather than close, so a failed exec can still report its error.
	// Engine cleanup has already stopped workers and released owned resources.
#ifdef __linux__
	DIR *directory = opendir( "/proc/self/fd" );
	if ( !directory ) return false;
	bool ok = true;
	errno = 0;
	while ( const dirent *entry = readdir( directory ) ) {
		char *end;
		const long descriptor = strtol( entry->d_name, &end, 10 );
		if ( !*entry->d_name || *end || descriptor < 3 || descriptor > INT_MAX ) continue;
		if ( !CloseDescriptorOnExec( int( descriptor ) ) ) {
			ok = false;
			break;
		}
		errno = 0;
	}
	const int error = errno;
	closedir( directory );
	errno = error;
	return ok && error == 0;
#else
	const long limit = sysconf( _SC_OPEN_MAX );
	if ( limit < 0 || limit > INT_MAX ) return false;
	for ( int descriptor = 3; descriptor < limit; ++descriptor ) {
		if ( !CloseDescriptorOnExec( descriptor ) ) return false;
	}
	return true;
#endif
}
#endif
}

int Sys_PrepareRelaunch( const char *commandLine, char *error, unsigned errorCapacity )
{
	if ( prepared ) return Fail( error, errorCapacity, "An engine relaunch is already prepared" );
	if ( !commandLine || !commandLine[0] ) return Fail( error, errorCapacity, "The relaunch command is empty" );
	unsigned length = 0;
	for ( ; length < SYS_RELAUNCH_COMMAND_BYTES && commandLine[length]; ++length ) {
		const unsigned char ch = static_cast<unsigned char>( commandLine[length] );
		if ( ch < 32 || ch == 127 ) return Fail( error, errorCapacity, "The relaunch command contains a control character" );
	}
	if ( length == SYS_RELAUNCH_COMMAND_BYTES ) return Fail( error, errorCapacity, "The relaunch command is too long" );
	RelaunchPlan plan = {};
#ifdef _WIN32
	const DWORD imageLength = GetModuleFileNameW( nullptr, plan.image, 32768 );
	if ( !imageLength || imageLength >= 32768 )
		return Fail( error, errorCapacity, "Cannot resolve this engine executable", GetLastError() );
	wchar_t arguments[SYS_RELAUNCH_COMMAND_BYTES];
	const int argumentLength = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS,
		commandLine, -1, arguments, SYS_RELAUNCH_COMMAND_BYTES );
	if ( !argumentLength ) return Fail( error, errorCapacity, "The relaunch command is not valid UTF-8", GetLastError() );
	// CreateProcessW receives an explicit image path and a writable raw command
	// line. Quake, rather than the C runtime or a shell, parses the arguments.
	if ( imageLength + argumentLength + 3 > 32768 )
		return Fail( error, errorCapacity, "The executable path and relaunch command are too long" );
	plan.command[0] = L'"';
	std::memcpy( plan.command + 1, plan.image, imageLength * sizeof( wchar_t ) );
	plan.command[imageLength + 1] = L'"';
	plan.command[imageLength + 2] = L' ';
	std::memcpy( plan.command + imageLength + 3, arguments, argumentLength * sizeof( wchar_t ) );
#else
#ifdef __linux__
	const ssize_t imageLength = readlink( "/proc/self/exe", plan.image, sizeof( plan.image ) );
	if ( imageLength <= 0 || size_t( imageLength ) >= sizeof( plan.image ) )
		return Fail( error, errorCapacity, "Cannot resolve this engine executable", errno );
	plan.image[imageLength] = 0;
#elif defined( __APPLE__ )
	char image[PATH_MAX];
	uint32_t capacity = sizeof( image );
	if ( _NSGetExecutablePath( image, &capacity ) || !realpath( image, plan.image ) )
		return Fail( error, errorCapacity, "Cannot resolve this engine executable", errno );
#else
	return Fail( error, errorCapacity, "Mode relaunch is not implemented on this platform; restart this executable with the requested profile" );
#endif
	// C++ filesystem uses the platform's large-file stat contract, including
	// 64-bit inode numbers on an x86 process running on a modern filesystem.
	std::error_code pathError;
	if ( plan.image[0] != '/' || !std::filesystem::is_regular_file( plan.image, pathError ) || access( plan.image, X_OK ) )
		return Fail( error, errorCapacity, "This engine executable is no longer available for relaunch",
			pathError ? pathError.value() : errno );
	std::memcpy( plan.command, commandLine, length + 1 );
#endif
	pending = plan;
	prepared = true;
	if ( error && errorCapacity ) error[0] = 0;
	return 1;
}

int Sys_HasPendingRelaunch( void ) { return prepared; }

void Sys_CancelRelaunch( void )
{
	prepared = false;
	pending = {};
}

int Sys_LaunchRelaunch( char *error, unsigned errorCapacity )
{
	if ( !prepared ) return Fail( error, errorCapacity, "No engine relaunch is prepared" );
	prepared = false; // Consume before entering the OS; failures cannot retry on a later quit.
#ifdef _WIN32
	STARTUPINFOW startup = {};
	PROCESS_INFORMATION process = {};
	startup.cb = sizeof( startup );
	if ( !CreateProcessW( pending.image, pending.command, nullptr, nullptr, FALSE,
		0, nullptr, nullptr, &startup, &process ) ) {
		const DWORD code = GetLastError();
		Sys_CancelRelaunch();
		return Fail( error, errorCapacity, "Cannot restart this engine executable", code );
	}
	CloseHandle( process.hThread );
	CloseHandle( process.hProcess );
	Sys_CancelRelaunch();
	if ( error && errorCapacity ) error[0] = 0;
	return 1;
#else
	if ( !CloseDescriptorsOnExec() ) {
		const int code = errno;
		Sys_CancelRelaunch();
		return Fail( error, errorCapacity, "Cannot isolate descriptors for engine relaunch", code );
	}
	// Sys_BuildCommandLine preserves this already-quoted engine command verbatim.
	char *arguments[] = { pending.image, pending.command, nullptr };
	execv( pending.image, arguments );
	const int code = errno;
	Sys_CancelRelaunch();
	return Fail( error, errorCapacity, "Cannot restart this engine executable", code );
#endif
}

#ifdef _WIN32
int Sys_DecodeStartupCommandLine( const wchar_t *commandLine,
	char *destination, unsigned capacity )
{
	if ( !commandLine || !destination || !capacity || capacity > INT_MAX ) return 0;
	bool quoted = false;
	while ( *commandLine ) {
		if ( *commandLine == L'"' ) quoted = !quoted;
		else if ( *commandLine <= L' ' && !quoted ) break;
		++commandLine;
	}
	while ( *commandLine && *commandLine <= L' ' ) ++commandLine;
	const int bytes = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, commandLine,
		-1, nullptr, 0, nullptr, nullptr );
	if ( !bytes || unsigned( bytes ) > capacity ) return 0;
	return WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, commandLine,
		-1, destination, int( capacity ), nullptr, nullptr ) == bytes;
}
#endif
