// SPDX-License-Identifier: GPL-2.0-or-later
// Engine-owned save selection; serialization remains in the installed game.
#ifndef FNTECH3_SERVER_RTCW_SAVE_HPP
#define FNTECH3_SERVER_RTCW_SAVE_HPP
#include "../compat/rtcw_savegame.hpp"
#include <vector>

static bool SV_RTCWReadSaveHeader( const char *filename,
	fntech3::rtcw::savegame::Header &header ) {
	using namespace fntech3::rtcw::savegame;
	fileHandle_t file = 0;
	const int length = FS_FOpenFileRead(filename, &file, qtrue);
	unsigned char prefix[PrefixBytes]{};
	const bool read = file && length > static_cast<int>(PrefixBytes) && length <= MaxBytes &&
		FS_Read(prefix, static_cast<int>(PrefixBytes), file) == static_cast<int>(PrefixBytes);
	if ( file ) FS_FCloseFile(file);
	if ( !read || !Parse(prefix, sizeof(prefix), header) ) {
		Com_Printf("Cannot load RTCW save %s: missing, truncated or invalid save header.\n", filename);
		return false;
	}
	return true;
}

static bool SV_RTCWStageSave( const char *filename,
	const fntech3::rtcw::savegame::Header &expected ) {
	using namespace fntech3::rtcw::savegame;
	// Release memory before the filesystem error boundary. Never truncate the
	// previous current save if writing or replacing the temporary file fails.
	{
		fileHandle_t source = 0;
		const int length = FS_FOpenFileRead(filename, &source, qtrue);
		if ( !source ) return false;
		if ( length <= static_cast<int>(PrefixBytes) || length > MaxBytes ) {
			FS_FCloseFile(source); return false;
		}
		std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
		const bool read = FS_Read(bytes.data(), length, source) == length;
		FS_FCloseFile(source);
		Header header;
		if ( !read || !Parse(bytes.data(), bytes.size(), header) || header.time != expected.time ||
			header.version != expected.version || std::strcmp(header.map, expected.map) ) return false;
		fileHandle_t destination = FS_FOpenFileWrite("save/fntech3-load.tmp");
		if ( !destination ) return false;
		const bool written = FS_Write(bytes.data(), length, destination) == length;
		FS_FCloseFile(destination);
		if ( !written ) return false;
	}
	FS_VM_RenameFile("save/fntech3-load.tmp", "save/current.svg");
	return true;
}

static void SV_RTCWClearSaveRequest() {
	Cvar_Set("savegame_loading", "0"); Cvar_Set("savegame_filename", "");
}

// After clearing level state, before GAME_INIT. This engine keeps the network
// clock separately; the save timestamp belongs to the game/level clock.
bool SV_RTCWRestoreSaveTime() {
	if ( !Com_GameIsRTCWSP() || !Cvar_VariableIntegerValue("savegame_loading") ) return true;
	fntech3::rtcw::savegame::Header header;
	if ( !SV_RTCWReadSaveHeader("save/current.svg", header) ) {
		SV_RTCWClearSaveRequest(); return false;
	}
	sv.time = header.time; return true;
}
#endif
