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
/* Each title's server keeps its models in a store of its own, because each
 * store serves only what its own retail engine was measured to do and the
 * two engines were not measured alike. Each dispatcher binds the name
 * `ravenGhoul2` to its title's store.
 *
 * Jedi Academy's: retail jamp.exe 1.01's game imports reach the same Ghoul2
 * cores as its client game's -- 587 to 589 the bolt query 0x4dfa00, 594 the
 * link store 0x4df940, 600 the model copy 0x4e04b0 -- so the server answers a
 * bolt query as the client game does: the engine-provided `*default`
 * skeleton every weapon and saber names, models attached to another posed
 * from its parent's bolt, and retail's tag frame. Its own synthetic game
 * module, run inside retail jamp.exe's dedicated server, gives the answers
 * this store matches. Animation blends stay off: they were measured through
 * the client game only. See docs/fntech3/2026-10-06-jedi-server-attachments.md.
 *
 * Jedi Outcast's keeps the earlier behaviour. Its engine builds and caches
 * bolts differently (jk2mp.exe 1.02a 0x4790f0), and no retail answer was
 * observed for it. */
fntech3::ghoul2::Store ravenGhoul2Academy( ravenGhoul2Files, true, false, true );
fntech3::ghoul2::Store ravenGhoul2Outcast( ravenGhoul2Files );
fntech3::ghoul2::SkinRegistry ravenSkins( ravenGhoul2Files );

static void SV_RavenAssetsClear()
{
	ravenGhoul2Academy.Clear();
	ravenGhoul2Outcast.Clear();
	ravenSkins.Clear();
	ravenRoff.Clear();
}

} // namespace

#endif
