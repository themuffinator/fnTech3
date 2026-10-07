/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// Starting the engine for one installation. The launcher always names the
// profile and the installation explicitly (+set com_gameProfile, +set
// fs_basepath), so the engine selects exactly one contract before anything
// else starts, as AGENTS.md's first invariant asks. No shell is involved.
#pragma once

#include "../qcommon/game_profile.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fntech3::launcher {

struct LaunchRequest {
	std::filesystem::path engine;
	const comGameProfile_t *profile = nullptr;
	std::string basePath;  // UTF-8 installation root
	std::string mod;       // fs_game folder; empty for none
	std::string arguments; // the player's extra command line
};

// The engine reads its command line with Quake's tokenizer: whitespace
// separates tokens, double quotes group, and nothing is escaped. Unbalanced
// quotes and control characters are refused.
bool SplitArguments( const std::string &text, std::vector<std::string> &tokens, std::string *error = nullptr );

// Startup commands the engine accepts (common.c's MAX_CONSOLE_LINES) and the
// command line its Windows entry point keeps (MAX_STRING_CHARS).
inline constexpr int kMaxStartupCommands = 32;
inline constexpr std::size_t kMaxCommandLineBytes = 1023;

// The engine arguments: the profile, the installation, the mod, then extras.
bool BuildArguments( const LaunchRequest &request, std::vector<std::string> &arguments,
	std::string *error = nullptr );

// The raw command line the engine reads on Windows. Tokens are quoted exactly
// where unix_main.cpp's Sys_BuildCommandLine quotes POSIX argv entries, so
// both platforms hand the engine the same line.
std::string JoinCommandLine( const std::vector<std::string> &arguments );

// A command the player can paste into a terminal or a shortcut.
std::string DisplayCommand( const std::filesystem::path &engine, const std::vector<std::string> &arguments );

// The engine executable this launcher was built beside.
std::filesystem::path DefaultEnginePath( const std::filesystem::path &launcherDirectory, bool dedicated );

// One started engine process. Destroying it never stops the game.
class GameProcess {
public:
	GameProcess() = default;
	~GameProcess();
	GameProcess( const GameProcess & ) = delete;
	GameProcess &operator=( const GameProcess & ) = delete;

	// Starts the executable from its own folder without inheriting the
	// launcher's handles or descriptors.
	bool Start( const std::filesystem::path &engine, const std::vector<std::string> &arguments,
		std::string *error = nullptr );
	// Non-blocking; false once the process has ended (or was never started).
	bool Running();
	bool Started() const { return started_; }
	int ExitCode() const { return exitCode_; }
	double Seconds() const;

private:
	void Close();

	bool started_ = false;
	bool finished_ = false;
	int exitCode_ = 0;
	std::chrono::steady_clock::time_point startedAt_{};
#ifdef _WIN32
	void *process_ = nullptr;
#else
	long pid_ = -1;
#endif
};

} // namespace fntech3::launcher
