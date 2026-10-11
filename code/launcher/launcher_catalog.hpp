/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// What the launcher lists: game installations recognized by the engine's own
// startup probe (Com_ProbeGameProfileMarkers), so a row appears only where
// fntech3 itself would find the game's data. Store labels describe where an
// installation came from; they never change which profile it runs.
#pragma once

#include "../qcommon/game_manager.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fntech3::launcher {

enum class Store : std::uint8_t {
	Local,        // found in a usual games folder, with no store evidence
	Manual,       // the player added the folder
	Portable,     // fnTech3 itself sits inside the game's folder
	Retail,       // registered by a disc or installer (Windows registry)
	Steam,
	SteamFlatpak,
	SteamSnap,
	Gog,
	Heroic,
	Lutris,
	EaApp,
	Count
};

// The name a row shows: "Steam", "GOG", "Retail", "Added manually"...
const char *StoreName( Store store );
// The stable name the settings file stores.
const char *StoreKey( Store store );
bool StoreFromKey( const std::string &key, Store &store );
// When one folder is found by several sources, the higher rank names it.
int StoreRank( Store store );

// A folder some source says may hold a game, before anything is read from it.
struct Candidate {
	std::filesystem::path path;
	Store store = Store::Local;
	std::string detail;     // where it was found, for the details panel
	std::string storeTitle; // the store's own name for the game, when known
	std::string storeId;    // a Steam app or GOG product id, when known
};

struct Installation {
	// Stable identity: the root's identity key and the group key. Settings
	// remember per-installation choices under it.
	std::string id;
	std::filesystem::path root; // canonical; the engine's fs_basepath
	std::string rootUtf8;
	std::string group;          // GroupKey of its profiles
	comGameTitleId_t title = COM_GAME_TITLE_QUAKE3;
	// The profiles this installation's data supports, in catalogue order.
	std::vector<const comGameProfile_t *> modes;
	unsigned markers = 0;
	Store store = Store::Local;
	std::string detail;
	std::string storeTitle;
	std::string storeId;
	std::string release;        // a Q3Test release such as "1.08"
	bool verified = true;       // its data matched a known retail release
	bool manual = false;        // the player added this folder
	bool missing = false;       // a manual folder that is no longer there
	bool portable = false;      // fnTech3 is installed inside it
};

// Q3's IHV and Q3Test installations are separate products under the Quake III
// title, so each is its own group; every other title groups its modes.
std::string GroupKey( const comGameProfile_t *profile );
// The profiles of one group, in catalogue order.
std::vector<const comGameProfile_t *> GroupProfiles( const std::string &group );
// The name a row shows for a group: the title, or the test release's profile.
std::string GroupName( const std::string &group );
// "Single Player", "Multiplayer", "Holomatch"; empty for one-mode titles.
std::string ModeLabel( const comGameProfile_t *profile );
// Catalogue position of a group, for a stable default order.
int GroupOrder( const std::string &group );

// Every base directory a catalogue profile reads ("baseq3", "main", ...).
const std::vector<std::string> &BaseDirectoryNames();

// A cheap filter by immediate children alone: could this folder hold any
// profile's data? The engine's probe reads archives, so it runs only after.
bool MightHoldGame( const std::filesystem::path &folder );

struct ProbedRoot {
	std::filesystem::path root; // canonical
	unsigned markers = 0;
};

// The folder itself and the nested roots stores install into (Jedi Knight's
// GameData, GOG's Linux game/), each probed by the engine's own reader.
// Only roots with recognized data are returned.
std::vector<ProbedRoot> ProbeFolder( const std::filesystem::path &folder );

// One installation per group found in a probe, carrying the candidate's facts.
std::vector<Installation> InstallationsFromProbe( const ProbedRoot &probe,
	const Candidate &candidate );

// An installation for a folder the player chose to add as a given group even
// though its data was not recognized, or that is currently missing.
Installation UnverifiedInstallation( const std::filesystem::path &folder,
	const std::string &group, bool missing );

// Store evidence inside an installation (a goggame-*.info file, the EA app's
// __Installer folder) for one found in a plain folder.
void RefineStore( Installation &installation, const std::filesystem::path &candidateFolder );

// Merges installations found more than once (same root and group), keeping
// the highest-ranked store and the union of flags, then sorts them by
// catalogue order and path.
void MergeInstallations( std::vector<Installation> &installations );

// The folders to probe for a folder the player picked: the folder, then its
// parent when the pick was a base directory or a nested root.
std::vector<std::filesystem::path> ManualProbeOrder( const std::filesystem::path &picked );

// Mod folders beside the profile's base directory: those holding a .pk3 and
// not owned by the game itself. Sorted, at most `limit`.
std::vector<std::string> FindMods( const Installation &installation,
	const comGameProfile_t *profile, std::size_t limit = 64 );
// A friendlier name for well-known mod folders ("Team Arena"), else the folder.
std::string ModDisplayName( const std::string &folder );

// Whether this build of the engine starts the profile, and a player-facing
// reason when it does not.
bool ProfileStartsInThisBuild( const comGameProfile_t *profile );
std::string ProfileUnavailableReason( const comGameProfile_t *profile );
// The Meson option a developer build enables the profile with, or empty.

} // namespace fntech3::launcher
