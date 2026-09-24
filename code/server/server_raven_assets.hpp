/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RAVEN_ASSETS_HPP
#define FNTECH3_SERVER_RAVEN_ASSETS_HPP

/* The asset stores both Raven titles share.
 *
 * Jedi Outcast and Jedi Academy read the same formats -- Ghoul2 meshes and
 * skeletons, skins, and recorded motion paths -- and both ask the server to
 * hold them. What they do not share is the numbering they ask through, so
 * each title has its own dispatcher and neither is run through the other's.
 * These stores sit under both, and are cleared when a map is. */

#include "../ghoul2/ghoul2_server.hpp"
#include "server_raven_roff.hpp"

#include <cstring>

namespace {

/* The models a Raven game asks this server to hold. Both titles need them
 * before a level finishes spawning. */
class ServerGhoul2Files final : public fntech3::ghoul2::Files {
public:
	std::size_t Read( const char *path, void *destination, std::size_t capacity ) override
	{
		void *bytes = nullptr;
		const int length = FS_ReadFile( path, &bytes );
		if ( length <= 0 || !bytes ) {
			if ( bytes ) FS_FreeFile( bytes );
			return 0;
		}
		const std::size_t size = std::size_t( length );
		if ( destination && capacity >= size ) std::memcpy( destination, bytes, size );
		FS_FreeFile( bytes );
		return size;
	}
};

/* Motion paths are read the same way and from the same filesystem. */
class ServerRoffFiles final : public fntech3::roff::Files {
public:
	std::size_t Read( const char *path, void *destination, std::size_t capacity ) override
	{
		void *bytes = nullptr;
		const int length = FS_ReadFile( path, &bytes );
		if ( length <= 0 || !bytes ) {
			if ( bytes ) FS_FreeFile( bytes );
			return 0;
		}
		const std::size_t size = std::size_t( length );
		if ( destination && capacity >= size ) std::memcpy( destination, bytes, size );
		FS_FreeFile( bytes );
		return size;
	}
};

ServerRoffFiles ravenRoffFiles;
fntech3::roff::Store ravenRoff( ravenRoffFiles );

ServerGhoul2Files ravenGhoul2Files;
fntech3::ghoul2::Store ravenGhoul2( ravenGhoul2Files );
fntech3::ghoul2::SkinRegistry ravenSkins( ravenGhoul2Files );

static void SV_RavenAssetsClear()
{
	ravenGhoul2.Clear();
	ravenSkins.Clear();
	ravenRoff.Clear();
}

} // namespace

#endif
