/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_KEY_NAMES_HPP
#define FNTECH3_STVEF_KEY_NAMES_HPP

#include "rtcw_key_names.hpp"

namespace fntech3::stvef::keys {

/* Elite Force's translated key names are original RTCW's English, French and
 * German ones, with the same German Shift in each mode (UMSCHALT in stvoy.exe,
 * UMSCHALLT in stvoyHM.exe); scripts/probe_stvef_key_names.py compares them
 * with the retail tables. Retail picks the table by g_language, compared
 * without regard to ASCII case: deutsch German, francais French, anything
 * else English. */
inline bool IsLanguage( const char *value, const char *language ) {
	if ( !value ) {
		return false;
	}
	for ( ;; ++value, ++language ) {
		char a = *value, b = *language;
		if ( a >= 'a' && a <= 'z' ) a = static_cast<char>( a - 'a' + 'A' );
		if ( b >= 'a' && b <= 'z' ) b = static_cast<char>( b - 'a' + 'A' );
		if ( a != b ) {
			return false;
		}
		if ( !a ) {
			return true;
		}
	}
}

inline rtcw::keys::Column ColumnForLanguage( const char *gLanguage, bool multiplayer ) {
	if ( IsLanguage( gLanguage, "deutsch" ) ) {
		return multiplayer ? rtcw::keys::GermanMP : rtcw::keys::GermanSP;
	}
	return IsLanguage( gLanguage, "francais" ) ? rtcw::keys::French : rtcw::keys::English;
}

/* The name retail hands a module for its own key number: the translated
 * Key_KeynumToString (stvoyHM.exe 0x40b830, stvoy.exe 0x406bd0) that the UI's
 * key-name import uses. tiny must hold at least five characters. */
inline const char *NameForModule( int key, const char *gLanguage, bool multiplayer, char *tiny ) {
	return rtcw::keys::NameInColumn( key, ColumnForLanguage( gLanguage, multiplayer ), tiny );
}

} // namespace fntech3::stvef::keys

#endif
