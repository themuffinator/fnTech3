/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_KEY_NAMES_HPP
#define FNTECH3_RTCW_KEY_NAMES_HPP

#include "rtcw_game_abi.h"

namespace fntech3::rtcw::keys {

struct KeyName {
	int key;
	const char *names[6];	// English, French, German SP, German MP, Italian, Spanish
};

inline constexpr KeyName keyNames[] = {
#define FNTECH3_RTCW_KEYNAME(key, english, french, germanSP, germanMP, italian, spanish) \
	{ FNTECH3_RTCW_##key, { english, french, germanSP, germanMP, italian, spanish } },
#include "rtcw_keynames.def"
#undef FNTECH3_RTCW_KEYNAME
};

/* The name original RTCW hands a module for a key number: its translated
 * Key_KeynumToString, which the UI and cgame key-name imports use (cl_keys.c,
 * cl_ui.c). A printable character is itself, ';' included; otherwise the name
 * table for cl_language (1 French, 2 German, 3 Italian, 4 Spanish, anything
 * else English); otherwise "0x" and two lowercase hex digits. tiny must hold
 * at least five characters and is returned for the single-character and hex
 * forms. */
inline const char *NameForModule( int key, int clLanguage, bool multiplayer, char *tiny ) {
	if ( key == -1 ) {
		return "<KEY NOT FOUND>";
	}
	if ( key < 0 || key > 255 ) {
		return "<OUT OF RANGE>";
	}
	if ( key > 32 && key < 127 && key != '"' ) {
		tiny[0] = static_cast<char>( key );
		tiny[1] = '\0';
		return tiny;
	}
	int column = 0;
	switch ( static_cast<long long>( clLanguage ) - 1 ) {
	case 0: column = 1; break;
	case 1: column = multiplayer ? 3 : 2; break;
	case 2: column = 4; break;
	case 3: column = 5; break;
	default: break;
	}
	for ( const KeyName &entry : keyNames ) {
		if ( entry.key == key ) {
			return entry.names[column];
		}
	}
	const int high = key >> 4, low = key & 15;
	tiny[0] = '0';
	tiny[1] = 'x';
	tiny[2] = static_cast<char>( high > 9 ? high - 10 + 'a' : high + '0' );
	tiny[3] = static_cast<char>( low > 9 ? low - 10 + 'a' : low + '0' );
	tiny[4] = '\0';
	return tiny;
}

} // namespace fntech3::rtcw::keys

#endif
