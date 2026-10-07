/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// The launcher's own settings file. It belongs to the launcher alone: the
// engine never reads it, and nothing in it changes a profile's contract --
// the launcher only chooses which profile and installation to start.
#pragma once

#include "launcher_catalog.hpp"

#include <climits>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fntech3::launcher {

enum class Theme { System, Dark, Light };
enum class AfterLaunch { KeepOpen, Minimize, Close };
enum class SortOrder { Name, RecentlyPlayed, Location };

struct InstallationPrefs {
	std::string name;      // the player's own name for the row; empty for the default
	std::string mode;      // profile key last chosen
	std::string mod;       // fs_game folder; empty for none
	std::string arguments; // extra engine command line
	bool hidden = false;
	std::int64_t lastPlayed = 0; // seconds since 1970
	unsigned playCount = 0;
};

struct ManualEntry {
	std::string path;
	std::string group; // what it was added as
	std::int64_t added = 0;
};

struct WindowPlacement {
	int x = INT_MIN;
	int y = INT_MIN;
	int width = 0;
	int height = 0;
	bool maximized = false;
};

struct Settings {
	Theme theme = Theme::System;
	AfterLaunch afterLaunch = AfterLaunch::KeepOpen;
	SortOrder sort = SortOrder::Name;
	bool showUnavailable = true;
	bool unavailableCollapsed = false;
	bool showHidden = false;
	std::string enginePath; // empty: the client beside the launcher
	std::vector<std::string> searchFolders;
	std::vector<ManualEntry> manual;
	std::map<std::string, InstallationPrefs> installations;
	WindowPlacement window;
	// The last scan's result, shown at once on the next start while a fresh
	// scan runs. Rebuilt from catalogue keys, so it can never name a profile
	// this build does not have.
	std::vector<Installation> cache;
};

inline constexpr int kSettingsVersion = 1;
inline constexpr std::size_t kMaxSettingsBytes = 4u * 1024u * 1024u;

// The settings file: fntech3-launcher.json beside the launcher when that file
// exists (a portable copy), otherwise launcher.json in `preferenceFolder`.
std::filesystem::path SettingsFile( const std::filesystem::path &preferenceFolder,
	const std::filesystem::path &launcherDirectory );

// Missing files give defaults and succeed. A damaged file gives defaults,
// is kept beside itself as .bad, and reports why in `error`.
bool LoadSettings( const std::filesystem::path &file, Settings &settings, std::string *error = nullptr );
bool SaveSettings( const std::filesystem::path &file, const Settings &settings, std::string *error = nullptr );

std::string SerializeSettings( const Settings &settings );
bool ParseSettings( const std::string &text, Settings &settings, std::string *error = nullptr );

} // namespace fntech3::launcher
