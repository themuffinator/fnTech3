/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// UTF-8 paths and small file helpers shared by the launcher's discovery,
// settings and launch code. Every function is exception-free: a path the
// platform cannot represent behaves like a missing file.
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace fntech3::launcher::fs {

std::filesystem::path FromUtf8( const std::string &utf8 );
std::string ToUtf8( const std::filesystem::path &path );

bool IsDirectory( const std::filesystem::path &path );
bool IsRegularFile( const std::filesystem::path &path );

// The canonical form of an existing directory with symbolic links, "..",
// short (8.3) names and redundant separators resolved; empty on failure.
std::filesystem::path Canonical( const std::filesystem::path &path );

// A key that is equal for two spellings of the same directory: the canonical
// path, compared case-insensitively where the platform's file names are.
std::string IdentityKey( const std::filesystem::path &canonical );

// Immediate subdirectories, sorted by name; at most `limit` of them.
std::vector<std::filesystem::path> Subdirectories( const std::filesystem::path &path,
	std::size_t limit = 4096 );

// A direct child whose name matches case-insensitively (Linux installations
// copied from Windows keep their original capitalisation), or empty.
std::filesystem::path FindChild( const std::filesystem::path &parent, const char *name );

// Reads a whole file of at most `maxBytes`; false when missing or larger.
bool ReadFile( const std::filesystem::path &path, std::string &out, std::size_t maxBytes );

// Writes through a temporary sibling and renames it over the target, so a
// crash never leaves a half-written file behind.
bool WriteFileAtomic( const std::filesystem::path &path, const std::string &data,
	std::string *error = nullptr );

// UTF-8 value of an environment variable; empty when unset.
std::string Environment( const char *name );

// The current user's home folder; empty when it cannot be determined.
std::filesystem::path HomeDirectory();

// The folder holding this executable.
std::filesystem::path ExecutableDirectory();

// ASCII-only case folding; UTF-8 bytes above 0x7f are left alone.
std::string Lower( std::string text );
bool EqualsNoCase( const std::string &left, const std::string &right );
bool ContainsNoCase( const std::string &haystack, const std::string &needle );

} // namespace fntech3::launcher::fs
