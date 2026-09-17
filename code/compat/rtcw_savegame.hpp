// SPDX-License-Identifier: GPL-2.0-or-later
// Independently implemented engine-side RTCW SP save envelope. The native
// game owns the payload. Contract observations: id-Software/RTCW-SP,
// src/game/g_save.c and src/server/sv_ccmds.c (GPLv3 reference only).
#ifndef FNTECH3_RTCW_SAVEGAME_HPP
#define FNTECH3_RTCW_SAVEGAME_HPP
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace fntech3::rtcw::savegame {
constexpr std::size_t PathBytes = 64;
constexpr std::size_t PrefixBytes = 4 + PathBytes + 4;
constexpr int MaxBytes = 64 * 1024 * 1024;
// Original SP bg_public.h: EF_VIEWING_CAMERA (alias of EF_STAND_IDLE2).
constexpr int ViewingCamera = 0x00040000;
struct Header { char map[PathBytes]{}; int time = 0; int version = 0; };
inline bool NameCharacter( unsigned char c ) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '_' || c == '-';
}
inline bool SafeComponents( const std::string &name, bool spaces ) {
	bool component = false;
	for ( unsigned char c : name ) {
		if ( c == '/' ) { if ( !component ) return false; component = false; }
		else if ( NameCharacter(c) || (spaces && c == ' ') ) component = true;
		else return false;
	}
	return component;
}
inline bool Filename( const char *argument, char (&result)[PathBytes] ) {
	if ( !argument ) return false;
	std::string name;
	for ( std::size_t i = 0; argument[i]; ++i ) {
		if ( i >= PathBytes ) return false;
		const char c = argument[i]; name += c == '\\' ? '/' : c;
	}
	if ( name.size() >= 5 ) {
		std::string prefix = name.substr(0, 5);
		for ( char &c : prefix ) if ( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
		if ( prefix == "save/" ) name.erase(0, 5);
	}
	if ( name.size() >= 4 ) {
		std::string extension = name.substr(name.size() - 4);
		for ( char &c : extension ) if ( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
		if ( extension == ".svg" ) name.resize(name.size() - 4);
	}
	if ( !SafeComponents(name, true) || name.size() + 9 >= PathBytes ) return false;
	name = "save/" + name + ".svg";
	std::memcpy(result, name.c_str(), name.size() + 1); return true;
}
inline std::uint32_t Read32( const unsigned char *p ) {
	return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
		(std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
inline bool Parse( const void *data, std::size_t bytes, Header &result ) {
	if ( !data || bytes < PrefixBytes ) return false;
	const auto *p = static_cast<const unsigned char *>(data);
	const auto version = Read32(p), time = Read32(p + 4 + PathBytes);
	if ( version != 13 && version != 14 && version != 15 && version != 17 && version != 18 ) return false;
	if ( time > 0x7fffffffU - 10000U ) return false;
	const auto *end = static_cast<const unsigned char *>(std::memchr(p + 4, 0, PathBytes));
	if ( !end ) return false;
	const std::string map(reinterpret_cast<const char *>(p + 4), std::size_t(end - p - 4));
	if ( !SafeComponents(map, false) || map.size() + 9 >= PathBytes ) return false;
	Header parsed; std::memcpy(parsed.map, map.c_str(), map.size() + 1);
	parsed.time = static_cast<int>(time); parsed.version = static_cast<int>(version);
	result = parsed; return true;
}
}
#endif
