/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_launch.hpp"

#include "launcher_fs.hpp"

#include <cerrno>
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
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef FNTECH3_LAUNCHER_CLIENT_NAME
#define FNTECH3_LAUNCHER_CLIENT_NAME "fntech3"
#endif
#ifndef FNTECH3_LAUNCHER_SERVER_NAME
#define FNTECH3_LAUNCHER_SERVER_NAME "fntech3.ded"
#endif

namespace fntech3::launcher {

namespace stdfs = std::filesystem;

namespace {

bool Fail( std::string *error, const std::string &message )
{
	if ( error ) *error = message;
	return false;
}

// unix_main.cpp's Sys_CommandLineArgNeedsQuotes, kept identical so a Windows
// line and a POSIX argv reach the engine's tokenizer as the same text.
bool NeedsQuotes( const std::string &argument )
{
	if ( argument.empty() ) return true;
	for ( std::size_t i = 0; i < argument.size(); ++i ) {
		const unsigned char c = static_cast<unsigned char>( argument[i] );
		if ( c <= ' ' || c == ';' || ( c == '+' && i != 0 ) ) return true;
	}
	return false;
}

bool HasControl( const std::string &text )
{
	for ( const char ch : text ) {
		const unsigned char c = static_cast<unsigned char>( ch );
		if ( c < 32 || c == 127 ) return true;
	}
	return false;
}

// The engine's tokenizer starts a comment at // or /* outside quotes, which
// would cut a value such as a URL short. Quoting it on both platforms keeps
// it whole: Windows passes the line raw, and the POSIX entry point leaves an
// argument that already holds quotes as it is.
std::string ProtectComment( const std::string &token )
{
	if ( token.find( "//" ) == std::string::npos && token.find( "/*" ) == std::string::npos ) return token;
	return "\"" + token + "\"";
}

#ifdef _WIN32
std::wstring Widen( const std::string &utf8 )
{
	if ( utf8.empty() ) return {};
	const int length = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), int( utf8.size() ), nullptr, 0 );
	if ( length <= 0 ) return {};
	std::wstring wide( std::size_t( length ), L'\0' );
	MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), int( utf8.size() ), wide.data(), length );
	return wide;
}

std::string SystemMessage( DWORD code )
{
	wchar_t *buffer = nullptr;
	const DWORD length = FormatMessageW( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t *>( &buffer ), 0, nullptr );
	std::wstring message = length && buffer ? std::wstring( buffer, length ) : std::wstring();
	if ( buffer ) LocalFree( buffer );
	while ( !message.empty() && ( message.back() == L'\n' || message.back() == L'\r' || message.back() == L'.' ) ) {
		message.pop_back();
	}
	std::string text = fs::ToUtf8( stdfs::path( message ) );
	return text.empty() ? "system error " + std::to_string( code ) : text;
}
#endif

} // namespace

bool SplitArguments( const std::string &text, std::vector<std::string> &tokens, std::string *error )
{
	tokens.clear();
	std::size_t pos = 0;
	while ( pos < text.size() ) {
		const unsigned char c = static_cast<unsigned char>( text[pos] );
		if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) {
			++pos;
			continue;
		}
		if ( c < 32 || c == 127 ) return Fail( error, "The extra command line contains a control character." );
		std::string token;
		if ( c == '"' ) {
			const std::size_t close = text.find( '"', pos + 1 );
			if ( close == std::string::npos ) {
				tokens.clear();
				return Fail( error, "The extra command line has a quote that is never closed." );
			}
			token = text.substr( pos + 1, close - pos - 1 );
			if ( HasControl( token ) ) {
				tokens.clear();
				return Fail( error, "The extra command line contains a control character." );
			}
			pos = close + 1;
		} else {
			// As in the engine's tokenizer, a quote ends an unquoted token.
			while ( pos < text.size() ) {
				const unsigned char ch = static_cast<unsigned char>( text[pos] );
				if ( ch <= ' ' || ch == '"' ) break;
				if ( ch == 127 ) {
					tokens.clear();
					return Fail( error, "The extra command line contains a control character." );
				}
				token += char( ch );
				++pos;
			}
		}
		tokens.push_back( std::move( token ) );
	}
	if ( error ) error->clear();
	return true;
}

bool BuildArguments( const LaunchRequest &request, std::vector<std::string> &arguments, std::string *error )
{
	arguments.clear();
	if ( !request.profile || !request.profile->key ) return Fail( error, "No game mode was chosen." );
	if ( request.basePath.empty() ) return Fail( error, "The installation has no folder." );
	if ( HasControl( request.basePath ) || request.basePath.find( '"' ) != std::string::npos ) {
		return Fail( error, "The installation's folder name contains a quote or a control character, "
			"which the engine's command line cannot carry. Move or rename the folder." );
	}
	std::string basePath = request.basePath;
	// A trailing separator would sit before the closing quote; the engine never
	// needs it, and a drive root keeps its own.
	while ( basePath.size() > 1 && ( basePath.back() == '/' || basePath.back() == '\\' )
		&& !( basePath.size() == 3 && basePath[1] == ':' ) ) {
		basePath.pop_back();
	}
	arguments = { "+set", "com_gameProfile", request.profile->key, "+set", "fs_basepath", basePath };
	if ( !request.mod.empty() ) {
		if ( HasControl( request.mod ) || request.mod.find_first_of( "\"/\\:" ) != std::string::npos
			|| request.mod == "." || request.mod == ".." ) {
			arguments.clear();
			return Fail( error, "The mod folder's name is not one the engine accepts." );
		}
		arguments.insert( arguments.end(), { "+set", "fs_game", request.mod } );
	}
	std::vector<std::string> extra;
	if ( !SplitArguments( request.arguments, extra, error ) ) {
		arguments.clear();
		return false;
	}
	for ( std::string &token : extra ) arguments.push_back( ProtectComment( token ) );
	int commands = 0;
	for ( const std::string &argument : arguments ) {
		if ( !argument.empty() && argument[0] == '+' ) ++commands;
	}
	if ( commands > kMaxStartupCommands ) {
		arguments.clear();
		return Fail( error, "The command line has more than 32 startup commands. Put the extra settings in a "
			"config file and run it with +exec instead." );
	}
	if ( JoinCommandLine( arguments ).size() > kMaxCommandLineBytes ) {
		arguments.clear();
		return Fail( error, "The command line is longer than the engine accepts. Shorten the extra command "
			"line, or move the installation to a shorter folder path." );
	}
	if ( error ) error->clear();
	return true;
}

std::string JoinCommandLine( const std::vector<std::string> &arguments )
{
	std::string line;
	for ( const std::string &argument : arguments ) {
		if ( !line.empty() ) line += ' ';
		const bool quote = NeedsQuotes( argument ) && argument.find( '"' ) == std::string::npos;
		if ( quote ) line += '"';
		line += argument;
		if ( quote ) line += '"';
	}
	return line;
}

std::string DisplayCommand( const stdfs::path &engine, const std::vector<std::string> &arguments )
{
#ifdef _WIN32
	std::string command = "\"" + fs::ToUtf8( engine ) + "\"";
	const std::string line = JoinCommandLine( arguments );
	if ( !line.empty() ) command += " " + line;
	return command;
#else
	auto shellQuote = []( const std::string &text ) {
		bool plain = !text.empty();
		for ( const char c : text ) {
			if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' )
				|| std::strchr( "+-_./:=@%,", c ) ) ) {
				plain = false;
				break;
			}
		}
		if ( plain ) return text;
		std::string quoted = "'";
		for ( const char c : text ) {
			if ( c == '\'' ) quoted += "'\\''";
			else quoted += c;
		}
		return quoted + "'";
	};
	std::string command = shellQuote( fs::ToUtf8( engine ) );
	for ( const std::string &argument : arguments ) command += " " + shellQuote( argument );
	return command;
#endif
}

stdfs::path DefaultEnginePath( const stdfs::path &launcherDirectory, bool dedicated )
{
	std::string name = dedicated ? FNTECH3_LAUNCHER_SERVER_NAME : FNTECH3_LAUNCHER_CLIENT_NAME;
#ifdef _WIN32
	name += ".exe";
#endif
	return launcherDirectory / fs::FromUtf8( name );
}

GameProcess::~GameProcess()
{
	Close();
}

double GameProcess::Seconds() const
{
	if ( !started_ ) return 0.0;
	return std::chrono::duration<double>( std::chrono::steady_clock::now() - startedAt_ ).count();
}

void GameProcess::Close()
{
#ifdef _WIN32
	if ( process_ ) {
		CloseHandle( static_cast<HANDLE>( process_ ) );
		process_ = nullptr;
	}
#endif
}

#ifdef _WIN32

bool GameProcess::Start( const stdfs::path &engine, const std::vector<std::string> &arguments, std::string *error )
{
	if ( started_ ) return Fail( error, "This process was already started." );
	if ( !fs::IsRegularFile( engine ) ) {
		return Fail( error, "The engine was not found at " + fs::ToUtf8( engine ) + "." );
	}
	const std::wstring image = engine.wstring();
	std::wstring command = L"\"" + image + L"\"";
	const std::string line = JoinCommandLine( arguments );
	if ( !line.empty() ) {
		const std::wstring wide = Widen( line );
		if ( wide.empty() ) return Fail( error, "The command line is not valid UTF-8." );
		command += L" " + wide;
	}
	std::vector<wchar_t> buffer( command.begin(), command.end() );
	buffer.push_back( L'\0' );
	const std::wstring directory = engine.parent_path().wstring();
	STARTUPINFOW startup = {};
	startup.cb = sizeof( startup );
	PROCESS_INFORMATION info = {};
	if ( !CreateProcessW( image.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
			directory.empty() ? nullptr : directory.c_str(), &startup, &info ) ) {
		return Fail( error, "Windows could not start the engine: " + SystemMessage( GetLastError() ) + "." );
	}
	CloseHandle( info.hThread );
	process_ = info.hProcess;
	started_ = true;
	finished_ = false;
	startedAt_ = std::chrono::steady_clock::now();
	if ( error ) error->clear();
	return true;
}

bool GameProcess::Running()
{
	if ( !started_ || finished_ ) return false;
	if ( WaitForSingleObject( static_cast<HANDLE>( process_ ), 0 ) == WAIT_TIMEOUT ) return true;
	DWORD code = 0;
	if ( GetExitCodeProcess( static_cast<HANDLE>( process_ ), &code ) ) exitCode_ = int( code );
	finished_ = true;
	Close();
	return false;
}

#else

bool GameProcess::Start( const stdfs::path &engine, const std::vector<std::string> &arguments, std::string *error )
{
	if ( started_ ) return Fail( error, "This process was already started." );
	const std::string image = engine.native();
	if ( !fs::IsRegularFile( engine ) || access( image.c_str(), X_OK ) != 0 ) {
		return Fail( error, "The engine was not found, or is not executable, at " + image + "." );
	}
	// Everything the child needs is prepared before fork(): after it, only
	// async-signal-safe calls run until exec.
	const std::string directory = engine.parent_path().native();
	std::vector<std::string> storage;
	storage.reserve( arguments.size() + 1 );
	storage.push_back( image );
	storage.insert( storage.end(), arguments.begin(), arguments.end() );
	std::vector<char *> argv;
	for ( std::string &argument : storage ) argv.push_back( argument.data() );
	argv.push_back( nullptr );
	long limit = sysconf( _SC_OPEN_MAX );
	if ( limit < 0 || limit > 65536 ) limit = 65536;
	const int descriptorLimit = int( limit );

	int report[2];
	if ( pipe( report ) != 0 ) return Fail( error, std::string( "Cannot start the engine: " ) + std::strerror( errno ) + "." );
	fcntl( report[0], F_SETFD, FD_CLOEXEC );
	fcntl( report[1], F_SETFD, FD_CLOEXEC );

	const pid_t pid = fork();
	if ( pid < 0 ) {
		const int code = errno;
		close( report[0] );
		close( report[1] );
		return Fail( error, std::string( "Cannot start the engine: " ) + std::strerror( code ) + "." );
	}
	if ( pid == 0 ) {
		sigset_t none;
		sigemptyset( &none );
		sigprocmask( SIG_SETMASK, &none, nullptr );
		struct sigaction standard;
		std::memset( &standard, 0, sizeof( standard ) );
		standard.sa_handler = SIG_DFL;
		const int signals[] = { SIGPIPE, SIGINT, SIGTERM, SIGHUP, SIGCHLD };
		for ( const int number : signals ) sigaction( number, &standard, nullptr );
		// A session of its own: the game outlives the launcher and its terminal.
		setsid();
		int code = 0;
		if ( !directory.empty() && chdir( directory.c_str() ) != 0 ) {
			code = errno;
		} else {
			for ( int descriptor = 3; descriptor < descriptorLimit; ++descriptor ) {
				if ( descriptor != report[1] ) fcntl( descriptor, F_SETFD, FD_CLOEXEC );
			}
			execv( image.c_str(), argv.data() );
			code = errno;
		}
		const ssize_t written = write( report[1], &code, sizeof( code ) );
		(void)written;
		_exit( 127 );
	}
	close( report[1] );
	int childError = 0;
	ssize_t received;
	do {
		received = read( report[0], &childError, sizeof( childError ) );
	} while ( received < 0 && errno == EINTR );
	close( report[0] );
	if ( received == ssize_t( sizeof( childError ) ) ) {
		int status = 0;
		waitpid( pid, &status, 0 );
		return Fail( error, std::string( "Cannot start the engine: " ) + std::strerror( childError ) + "." );
	}
	pid_ = long( pid );
	started_ = true;
	finished_ = false;
	startedAt_ = std::chrono::steady_clock::now();
	if ( error ) error->clear();
	return true;
}

bool GameProcess::Running()
{
	if ( !started_ || finished_ ) return false;
	int status = 0;
	const pid_t result = waitpid( pid_t( pid_ ), &status, WNOHANG );
	if ( result == 0 ) return true;
	if ( result == pid_t( pid_ ) ) {
		exitCode_ = WIFEXITED( status ) ? WEXITSTATUS( status ) : WIFSIGNALED( status ) ? 128 + WTERMSIG( status ) : 0;
	}
	finished_ = true;
	return false;
}

#endif

} // namespace fntech3::launcher
