/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// Steam's text KeyValues ("VDF") files, as libraryfolders.vdf and the
// appmanifest_*.acf files write them. The engine's own startup reader
// (code/qcommon/steam_path_shared.h) answers one profile's question; the
// launcher needs every library and every installed app, so it keeps a small
// tree. Input is untrusted and bounded like the engine's.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fntech3::launcher::vdf {

struct Node {
	std::string key;
	std::string value;          // empty for a block
	std::vector<Node> children; // a block's entries, in file order
	bool block = false;

	// The first direct child with this key, compared case-insensitively.
	const Node *Find( const char *name ) const;
	// That child's string value, or an empty string.
	const std::string &Get( const char *name ) const;
};

inline constexpr std::size_t kMaxFileBytes = 4u * 1024u * 1024u;
inline constexpr std::size_t kMaxTokenBytes = 64u * 1024u;
inline constexpr int kMaxDepth = 32;

// Parses a whole file's text into its top-level entries. Returns false, with
// `root` cleared, for malformed or oversized input.
bool Parse( const std::string &text, Node &root );

// Reads and parses a file; false when it is missing, too large or malformed.
bool ParseFile( const std::string &utf8Path, Node &root );

} // namespace fntech3::launcher::vdf
