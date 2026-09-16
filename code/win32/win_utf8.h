/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WIN_UTF8_H
#define FNTECH3_WIN_UTF8_H

#include <windows.h>
#include <cerrno>
#include <cstddef>
#include <climits>

namespace fntech3::win {

template<std::size_t N>
bool ToWide( const char *source, wchar_t ( &destination )[N] ) noexcept {
	static_assert( N <= INT_MAX );
	if ( !source ) { errno = EINVAL; return false; }
	const int count = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, source, -1, nullptr, 0 );
	if ( !count ) { errno = EILSEQ; return false; }
	if ( std::size_t( count ) > N ) { errno = ENAMETOOLONG; return false; }
	return MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, source, -1, destination, int( N ) ) == count;
}

template<std::size_t N>
bool ToUtf8( const wchar_t *source, char ( &destination )[N] ) noexcept {
	static_assert( N <= INT_MAX );
	if ( !source ) { errno = EINVAL; return false; }
	// Some legacy engine headers still select a pre-Vista SDK contract. Check
	// UTF-16 explicitly instead of relying on its newer conversion flag.
	for ( const wchar_t *cursor = source; *cursor; ++cursor ) {
		if ( *cursor >= 0xd800 && *cursor <= 0xdbff ) {
			++cursor;
			if ( *cursor < 0xdc00 || *cursor > 0xdfff ) { errno = EILSEQ; return false; }
		} else if ( *cursor >= 0xdc00 && *cursor <= 0xdfff ) { errno = EILSEQ; return false; }
	}
	const int count = WideCharToMultiByte( CP_UTF8, 0, source, -1, nullptr, 0, nullptr, nullptr );
	if ( !count ) { errno = EILSEQ; return false; }
	if ( std::size_t( count ) > N ) { errno = ENAMETOOLONG; return false; }
	return WideCharToMultiByte( CP_UTF8, 0, source, -1, destination, int( N ), nullptr, nullptr ) == count;
}

/* Larger than all current engine OS-path/search buffers. Conversion never
 * truncates into a different path, allocates, or changes the process locale. */
class Path {
	wchar_t text[1024]{};
	bool valid;
public:
	explicit Path( const char *source ) noexcept : valid( ToWide( source, text ) ) {}
	explicit operator bool() const noexcept { return valid; }
	const wchar_t *c_str() const noexcept { return text; }
};

} // namespace fntech3::win
#endif
