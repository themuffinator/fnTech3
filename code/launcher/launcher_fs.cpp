/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_fs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <climits>
#include <pwd.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace fntech3::launcher::fs {

namespace stdfs = std::filesystem;

stdfs::path FromUtf8( const std::string &utf8 )
{
	try {
		return stdfs::u8path( utf8 );
	} catch ( const std::exception & ) {
		return {};
	}
}

std::string ToUtf8( const stdfs::path &path )
{
	try {
		return path.u8string();
	} catch ( const std::exception & ) {
		return {};
	}
}

bool IsDirectory( const stdfs::path &path )
{
	if ( path.empty() ) return false;
	std::error_code error;
	const bool directory = stdfs::is_directory( path, error );
	return directory && !error;
}

bool IsRegularFile( const stdfs::path &path )
{
	if ( path.empty() ) return false;
	std::error_code error;
	const bool regular = stdfs::is_regular_file( path, error );
	return regular && !error;
}

stdfs::path Canonical( const stdfs::path &path )
{
	if ( path.empty() ) return {};
	try {
		std::error_code error;
		stdfs::path canonical = stdfs::canonical( path, error );
		if ( error || canonical.empty() ) return {};
#ifdef _WIN32
		// Final-path queries can answer in the \\?\ namespace; keep the plain
		// drive or UNC spelling the engine and the player expect to see.
		const std::wstring text = canonical.native();
		if ( text.rfind( L"\\\\?\\UNC\\", 0 ) == 0 ) {
			canonical = stdfs::path( L"\\\\" + text.substr( 8 ) );
		} else if ( text.rfind( L"\\\\?\\", 0 ) == 0 ) {
			canonical = stdfs::path( text.substr( 4 ) );
		}
#endif
		return canonical;
	} catch ( const std::exception & ) {
		return {};
	}
}

std::string IdentityKey( const stdfs::path &canonical )
{
#ifdef _WIN32
	std::wstring text = canonical.native();
	std::replace( text.begin(), text.end(), L'/', L'\\' );
	while ( text.size() > 3 && text.back() == L'\\' ) text.pop_back();
	if ( !text.empty() ) {
		CharLowerBuffW( text.data(), DWORD( text.size() ) );
	}
	return ToUtf8( stdfs::path( text ) );
#else
	std::string text = ToUtf8( canonical );
	while ( text.size() > 1 && text.back() == '/' ) text.pop_back();
	return text;
#endif
}

std::vector<stdfs::path> Subdirectories( const stdfs::path &path, std::size_t limit )
{
	std::vector<stdfs::path> result;
	if ( path.empty() ) return result;
	try {
		std::error_code error;
		stdfs::directory_iterator it( path, stdfs::directory_options::skip_permission_denied, error );
		const stdfs::directory_iterator end;
		for ( ; !error && it != end && result.size() < limit; it.increment( error ) ) {
			std::error_code typeError;
			if ( it->is_directory( typeError ) && !typeError ) result.push_back( it->path() );
		}
	} catch ( const std::exception & ) {
		// A name the platform cannot convert ends the listing early.
	}
	std::sort( result.begin(), result.end() );
	return result;
}

stdfs::path FindChild( const stdfs::path &parent, const char *name )
{
	if ( parent.empty() || !name || !*name ) return {};
	std::error_code error;
	const stdfs::path exact = parent / name;
	if ( stdfs::exists( exact, error ) && !error ) return exact;
#ifndef _WIN32
	try {
		const std::string wanted = Lower( name );
		stdfs::directory_iterator it( parent, stdfs::directory_options::skip_permission_denied, error );
		const stdfs::directory_iterator end;
		for ( ; !error && it != end; it.increment( error ) ) {
			if ( Lower( ToUtf8( it->path().filename() ) ) == wanted ) return it->path();
		}
	} catch ( const std::exception & ) {
	}
#endif
	return {};
}

bool ReadFile( const stdfs::path &path, std::string &out, std::size_t maxBytes )
{
	out.clear();
	if ( path.empty() ) return false;
	try {
		std::error_code error;
		const auto size = stdfs::file_size( path, error );
		if ( error || size > maxBytes ) return false;
		std::ifstream file( path, std::ios::binary );
		if ( !file ) return false;
		out.resize( std::size_t( size ) );
		if ( size && !file.read( &out[0], std::streamsize( size ) ) ) {
			out.clear();
			return false;
		}
		return true;
	} catch ( const std::exception & ) {
		out.clear();
		return false;
	}
}

bool WriteFileAtomic( const stdfs::path &path, const std::string &data, std::string *error )
{
	auto fail = [error]( const std::string &message ) {
		if ( error ) *error = message;
		return false;
	};
	if ( path.empty() ) return fail( "no file name" );
	try {
		std::error_code code;
		if ( path.has_parent_path() ) stdfs::create_directories( path.parent_path(), code );
		stdfs::path temporary = path;
		temporary += ".tmp";
		{
			std::ofstream file( temporary, std::ios::binary | std::ios::trunc );
			if ( !file ) return fail( "cannot create " + ToUtf8( temporary ) );
			file.write( data.data(), std::streamsize( data.size() ) );
			file.flush();
			if ( !file ) {
				file.close();
				stdfs::remove( temporary, code );
				return fail( "cannot write " + ToUtf8( temporary ) );
			}
		}
		stdfs::rename( temporary, path, code );
		if ( code ) {
			// Some file systems refuse to replace an existing file by rename.
			std::error_code removeError;
			stdfs::remove( path, removeError );
			code.clear();
			stdfs::rename( temporary, path, code );
		}
		if ( code ) {
			stdfs::remove( temporary, code );
			return fail( "cannot replace " + ToUtf8( path ) );
		}
		if ( error ) error->clear();
		return true;
	} catch ( const std::exception &exception ) {
		return fail( exception.what() );
	}
}

std::string Environment( const char *name )
{
	if ( !name || !*name ) return {};
#ifdef _WIN32
	const int length = MultiByteToWideChar( CP_UTF8, 0, name, -1, nullptr, 0 );
	if ( length <= 0 ) return {};
	std::wstring wideName( std::size_t( length ), L'\0' );
	MultiByteToWideChar( CP_UTF8, 0, name, -1, wideName.data(), length );
	const DWORD size = GetEnvironmentVariableW( wideName.c_str(), nullptr, 0 );
	if ( !size ) return {};
	std::wstring value( size, L'\0' );
	const DWORD written = GetEnvironmentVariableW( wideName.c_str(), value.data(), size );
	if ( !written || written >= size ) return {};
	value.resize( written );
	return ToUtf8( stdfs::path( value ) );
#else
	const char *value = std::getenv( name );
	return value ? std::string( value ) : std::string();
#endif
}

stdfs::path HomeDirectory()
{
#ifdef _WIN32
	const std::string profile = Environment( "USERPROFILE" );
	return profile.empty() ? stdfs::path() : FromUtf8( profile );
#else
	const std::string home = Environment( "HOME" );
	if ( !home.empty() && home[0] == '/' ) return FromUtf8( home );
	if ( const passwd *entry = getpwuid( getuid() ) ) {
		if ( entry->pw_dir && entry->pw_dir[0] == '/' ) return stdfs::path( entry->pw_dir );
	}
	return {};
#endif
}

stdfs::path ExecutableDirectory()
{
#ifdef _WIN32
	std::wstring buffer( 32768, L'\0' );
	const DWORD length = GetModuleFileNameW( nullptr, buffer.data(), DWORD( buffer.size() ) );
	if ( !length || length >= buffer.size() ) return {};
	buffer.resize( length );
	return stdfs::path( buffer ).parent_path();
#elif defined( __linux__ )
	char buffer[PATH_MAX];
	const ssize_t length = readlink( "/proc/self/exe", buffer, sizeof( buffer ) - 1 );
	if ( length <= 0 ) return {};
	buffer[length] = '\0';
	return stdfs::path( buffer ).parent_path();
#elif defined( __APPLE__ )
	char image[PATH_MAX];
	uint32_t capacity = sizeof( image );
	char resolved[PATH_MAX];
	if ( _NSGetExecutablePath( image, &capacity ) || !realpath( image, resolved ) ) return {};
	return stdfs::path( resolved ).parent_path();
#else
	return {};
#endif
}

std::string Lower( std::string text )
{
	for ( char &c : text ) {
		if ( c >= 'A' && c <= 'Z' ) c = char( c - 'A' + 'a' );
	}
	return text;
}

bool EqualsNoCase( const std::string &left, const std::string &right )
{
	if ( left.size() != right.size() ) return false;
	for ( std::size_t i = 0; i < left.size(); ++i ) {
		char a = left[i], b = right[i];
		if ( a >= 'A' && a <= 'Z' ) a = char( a - 'A' + 'a' );
		if ( b >= 'A' && b <= 'Z' ) b = char( b - 'A' + 'a' );
		if ( a != b ) return false;
	}
	return true;
}

bool ContainsNoCase( const std::string &haystack, const std::string &needle )
{
	if ( needle.empty() ) return true;
	return Lower( haystack ).find( Lower( needle ) ) != std::string::npos;
}

} // namespace fntech3::launcher::fs
