/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// The launcher's window: the installation list, its dialogs, and the work
// behind them -- a background scan, the settings file and started games.
#pragma once

#include "launcher_discovery.hpp"
#include "launcher_fonts.hpp"
#include "launcher_launch.hpp"
#include "launcher_settings.hpp"
#include "launcher_widgets.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fntech3::launcher {

struct AppOptions {
	std::filesystem::path settingsFile;
	std::filesystem::path launcherDirectory;
	bool scanStores = true;
	bool scanCommonFolders = true;
	// Folders searched for this run only (--search-folder); never saved.
	std::vector<std::filesystem::path> extraSearchFolders;
	// Screenshot runs scan before the first frame and never write settings.
	bool synchronousScan = false;
	bool readOnlySettings = false;
	std::optional<Theme> theme;
	float scale = 0.0f;
	std::string selectGroup;
	std::string preview;
};

class App {
public:
	App( SDL_Window *window, SDL_Renderer *renderer, AppOptions options, Settings settings );
	~App();
	App( const App & ) = delete;
	App &operator=( const App & ) = delete;

	// Once ImGui exists, before the first frame.
	void Initialize( gui::Fonts *fonts );
	void HandleEvent( const SDL_Event &event );
	// Theme, scale and font changes, which ImGui must not see mid-frame.
	void BeforeFrame();
	void Frame();
	bool QuitRequested() const { return quit_; }
	// True while something animates or a scan runs: keep drawing every frame.
	bool Busy() const;
	// How long the loop may sleep waiting for input when not busy.
	int IdleWaitMilliseconds() const;
	Uint32 WakeEvent() const { return wakeEvent_; }
	void Shutdown();
	// A notification at the corner of the window: 0 information, 1 success, 2 error.
	void Notify( const std::string &text, int kind );

private:
	struct IconSet {
		SDL_Texture *frame64 = nullptr;
		SDL_Texture *frame128 = nullptr;
		bool loaded = false;
	};
	struct RunningGame {
		std::unique_ptr<GameProcess> process;
		std::string id;
		std::string name;
		bool dedicated = false;
	};
	struct Toast {
		std::string text;
		int kind = 0; // 0 info, 1 success, 2 error
		float age = 0.0f;
		float life = 4.5f;
	};
	struct View {
		std::vector<const Installation *> ready, unavailable, hidden;
		std::vector<const Installation *> order; // what Up and Down walk through
	};
	struct EditState {
		std::string id;
		char name[256] = {};
		char arguments[1024] = {};
		std::string mod;
		int mode = 0;
		std::vector<std::string> mods;
	};
	struct AddState {
		std::string path;
		int group = 0;
	};

	// Work.
	void StartScan();
	void PollScan();
	DiscoveryOptions MakeDiscoveryOptions() const;
	void ApplyInstallations( std::vector<Installation> installations );
	void PollRunning();
	void PollFolderPicks();
	void AddFolder( const std::string &path );
	void AddUnverified( const std::string &path, const std::string &group );
	void RemoveManual( const Installation &installation );
	void Launch( const Installation &installation, const comGameProfile_t *profile, bool dedicated );
	bool BuildLaunch( const Installation &installation, const comGameProfile_t *profile, bool dedicated,
		std::filesystem::path &engine, std::vector<std::string> &arguments, std::string &error ) const;
	void ResolveEngine();
	void ShowFolderPicker();
	void ShowEnginePicker();
	void ShowSearchFolderPicker();
	void OpenFolder( const std::filesystem::path &folder );
	void MarkDirty();
	void Persist( bool force );

	// Model helpers.
	InstallationPrefs &Prefs( const std::string &id );
	const InstallationPrefs *FindPrefs( const std::string &id ) const;
	std::string DisplayName( const Installation &installation ) const;
	const comGameProfile_t *SelectedMode( const Installation &installation ) const;
	bool Playable( const Installation &installation ) const;
	bool IsRunning( const std::string &id ) const;
	View BuildView() const;
	bool Matches( const Installation &installation, const std::string &query ) const;
	SDL_Texture *IconTexture( const comGameProfile_t *profile, float pixels );
	const Installation *FindInstallation( const std::string &id ) const;

	// Drawing.
	void ResolveTheme();
	void DrawBackground( ImDrawList *draw, ImVec2 size );
	void DrawHeader( float width );
	void DrawToolbar( float width );
	void DrawList( ImVec2 origin, ImVec2 size, const View &view );
	float DrawSection( ImDrawList *draw, float x, float y, float width, const std::string &label, std::size_t count,
		bool collapsible, bool *collapsed );
	void DrawRow( const Installation &installation, ImVec2 min, ImVec2 max, bool unavailable );
	void DrawSkeleton( ImDrawList *draw, ImVec2 min, float width );
	void DrawEmpty( ImVec2 origin, ImVec2 size );
	void DrawFooter( float width, float y );
	void DrawRowMenu();
	void DrawSortMenu();
	void DrawEditDialog();
	void DrawSettingsDialog();
	void DrawAddDialog();
	void DrawToasts( ImVec2 size );
	void DrawDropOverlay( ImVec2 size );
	bool MenuItem( const char *id, gui::Icon icon, const std::string &label, bool enabled = true, bool danger = false );
	bool BeginComboField( const char *id, const std::string &preview, float width );
	void HandleKeyboard( const View &view );
	void EnsureVisible( const std::string &id );

	SDL_Window *window_;
	SDL_Renderer *renderer_;
	AppOptions options_;
	Settings settings_;
	gui::Fonts *fonts_ = nullptr;
	gui::Ui ui_;
	bool styleDirty_ = true;
	bool darkTheme_ = true;
	float scale_ = 1.0f;
	bool quit_ = false;
	Uint32 wakeEvent_ = 0;

	std::vector<Installation> installations_;
	bool fontsChecked_ = false;
	std::string selectedId_;
	char search_[256] = {};
	bool focusSearch_ = false;
	std::string menuId_;
	bool openMenu_ = false;
	bool previewMenu_ = false;
	ImVec2 menuAnchor_;
	ImVec2 menuPivot_;
	float menuFlipY_ = -1.0f; // where an upward menu ends, or negative
	// Each visible row's menu corner (x, y) and the top of its button (z, w).
	std::map<std::string, ImVec4> rowMenuAnchors_;
	EditState edit_;
	AddState add_;
	bool openSettings_ = false;
	bool openEdit_ = false;
	bool openAdd_ = false;
	std::vector<Toast> toasts_;
	bool dropActive_ = false;
	std::string ensureVisible_;
	float scrollTarget_ = 0.0f;
	float listScroll_ = 0.0f;
	bool jumpScroll_ = false;
	std::map<std::string, float> rowPositions_; // content offsets from the last frame

	// Background scan.
	std::thread scanThread_;
	std::atomic<bool> scanCancel_{ false };
	std::atomic<bool> scanRunning_{ false };
	DiscoveryProgress progress_;
	std::mutex scanMutex_;
	std::optional<DiscoveryResult> scanResult_;
	bool scannedOnce_ = false;
	double lastScanSeconds_ = 0.0;

	std::vector<RunningGame> running_;
	bool minimizedForGame_ = false;
	std::map<std::string, IconSet> icons_;
	SDL_Texture *appIcon_ = nullptr;

	std::filesystem::path enginePath_;
	std::filesystem::path dedicatedPath_;
	bool engineFound_ = false;
	bool dedicatedFound_ = false;

	bool dirty_ = false;
	Uint64 dirtySince_ = 0;
	float time_ = 0.0f;
	bool previewApplied_ = false;
};

} // namespace fntech3::launcher
