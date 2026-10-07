/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// Where the launcher looks for installations. Sources only nominate folders;
// the engine's probe decides what each holds (launcher_catalog.hpp), so a
// stale registry entry or an unrelated Steam app costs a directory listing
// and nothing else. Nothing here writes outside the launcher's own settings.
#pragma once

#include "launcher_catalog.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace fntech3::launcher {

struct SteamApp {
	std::string appId;
	std::string name;
	std::filesystem::path folder; // <library>/steamapps/common/<installdir>
};

// The Steam root itself and every library its libraryfolders.vdf lists that
// exists, without duplicates.
std::vector<std::filesystem::path> SteamLibraries( const std::filesystem::path &steamRoot );
// Installed apps in one library, from its appmanifest_*.acf files.
std::vector<SteamApp> SteamApps( const std::filesystem::path &library );

struct HeroicGame {
	std::string title;
	std::filesystem::path folder;
};
// Heroic Games Launcher's gog_store/installed.json.
std::vector<HeroicGame> ParseHeroicInstalled( const std::string &text );

// Install paths named in a Lutris game configuration (YAML lines such as
// "  exe: /path/game.exe" or "  prefix: /path"); executables give their folder.
std::vector<std::filesystem::path> ParseLutrisPaths( const std::string &text );

// The folders below a Wine prefix where Windows installers put games.
std::vector<std::filesystem::path> WinePrefixGameFolders( const std::filesystem::path &prefix );

struct ManualFolder {
	std::filesystem::path folder;
	std::string group; // what it was added as; used when it cannot be recognized
};

struct DiscoveryOptions {
	// Folders the player asked to search: each child is a candidate.
	std::vector<std::filesystem::path> searchFolders;
	// Folders the player added as installations.
	std::vector<ManualFolder> manual;
	bool scanStores = true;        // Steam, GOG, Heroic, Lutris, registry
	bool scanCommonFolders = true; // Games folders, Program Files, /opt...
	// Where the launcher runs from, for a portable copy inside a game folder.
	std::filesystem::path launcherDirectory;
};

struct DiscoveryProgress {
	std::atomic<unsigned> candidates{ 0 };
	std::atomic<unsigned> probed{ 0 };
	std::atomic<unsigned> found{ 0 };
};

struct DiscoveryResult {
	std::vector<Installation> installations;
	unsigned foldersProbed = 0;
	double seconds = 0.0;
	bool cancelled = false;
};

// Every candidate folder the platform's stores and usual locations name.
std::vector<Candidate> GatherCandidates( const DiscoveryOptions &options );

// Gathers, probes and merges; safe on a worker thread. `cancel` is polled
// between folders and `progress` (when given) counts as it goes.
DiscoveryResult Discover( const DiscoveryOptions &options,
	const std::atomic<bool> *cancel = nullptr, DiscoveryProgress *progress = nullptr );

// The installations in one folder the player picked (or dropped), searched
// as ManualProbeOrder describes. Empty when nothing there is recognized.
std::vector<Installation> ProbeManualFolder( const std::filesystem::path &folder );

} // namespace fntech3::launcher
