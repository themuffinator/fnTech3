/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_app.hpp"

#include "launcher_fs.hpp"
#include "launcher_imgui_sdl.hpp"

#include "../platform/application_icon_data.h"
#include "../platform/game_icon_builtin.hpp"
#include "../../version/fntech3_version.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace fntech3::launcher {

namespace stdfs = std::filesystem;
using gui::Icon;
using ButtonKind = gui::Ui::ButtonKind;
using gui::Mix;
using gui::WithAlpha;

namespace {

constexpr float kSidePadding = 24.0f;
constexpr float kHeaderHeight = 90.0f;
constexpr float kToolbarHeight = 58.0f;
constexpr float kFooterHeight = 38.0f;
constexpr float kRowHeight = 80.0f;
constexpr float kRowGap = 10.0f;

enum PickKind { kPickGame = 0, kPickEngine = 1, kPickSearch = 2 };

// Dialog answers can arrive on any thread and after the window is gone, so
// they wait here rather than in the App.
std::mutex pickMutex;
std::vector<std::pair<int, std::string>> pickQueue;
std::atomic<Uint32> pickWakeEvent{ 0 };

void SDLCALL PickCallback( void *userdata, const char *const *files, int )
{
	const int kind = int( reinterpret_cast<std::intptr_t>( userdata ) );
	if ( files && files[0] ) {
		std::lock_guard<std::mutex> lock( pickMutex );
		pickQueue.emplace_back( kind, files[0] );
	}
	if ( const Uint32 wake = pickWakeEvent.load() ) {
		SDL_Event event;
		SDL_zero( event );
		event.type = wake;
		SDL_PushEvent( &event );
	}
}

std::int64_t Now()
{
	return std::int64_t( std::time( nullptr ) );
}

std::string FileUrl( const stdfs::path &folder )
{
	std::string path = fs::ToUtf8( folder );
	std::replace( path.begin(), path.end(), '\\', '/' );
	std::string url;
	if ( path.rfind( "//", 0 ) == 0 ) url = "file:";
	else url = path.empty() || path[0] != '/' ? "file:///" : "file://";
	static const char hex[] = "0123456789ABCDEF";
	for ( const char ch : path ) {
		const unsigned char c = static_cast<unsigned char>( ch );
		if ( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' )
			|| std::strchr( "-._~/:", c ) ) {
			url += char( c );
		} else {
			url += '%';
			url += hex[c >> 4];
			url += hex[c & 15];
		}
	}
	return url;
}

std::string Upper( std::string text )
{
	for ( char &c : text ) {
		if ( c >= 'a' && c <= 'z' ) c = char( c - 'a' + 'A' );
	}
	return text;
}

std::string Plural( std::size_t count, const char *one, const char *many )
{
	return std::to_string( count ) + " " + ( count == 1 ? one : many );
}

std::string PlayedWhen( std::int64_t then )
{
	if ( then <= 0 ) return "Not played from the launcher yet";
	const std::int64_t seconds = std::max<std::int64_t>( 0, Now() - then );
	if ( seconds < 90 ) return "Played just now";
	if ( seconds < 3600 ) return "Played " + Plural( std::size_t( seconds / 60 ), "minute", "minutes" ) + " ago";
	if ( seconds < 86400 ) return "Played " + Plural( std::size_t( seconds / 3600 ), "hour", "hours" ) + " ago";
	if ( seconds < 86400 * 45 ) return "Played " + Plural( std::size_t( seconds / 86400 ), "day", "days" ) + " ago";
	return "Played " + Plural( std::size_t( seconds / ( 86400 * 30 ) ), "month", "months" ) + " ago";
}

ImU32 StoreColor( const gui::Palette &palette, Store store )
{
	const bool dark = palette.dark;
	switch ( store ) {
	case Store::Steam:
	case Store::SteamFlatpak:
	case Store::SteamSnap: return dark ? IM_COL32( 110, 168, 255, 255 ) : IM_COL32( 29, 95, 201, 255 );
	case Store::Gog: return dark ? IM_COL32( 192, 146, 255, 255 ) : IM_COL32( 124, 58, 237, 255 );
	case Store::Heroic: return dark ? IM_COL32( 94, 214, 200, 255 ) : IM_COL32( 13, 148, 136, 255 );
	case Store::Lutris: return dark ? IM_COL32( 255, 170, 90, 255 ) : IM_COL32( 194, 98, 10, 255 );
	case Store::EaApp: return dark ? IM_COL32( 255, 122, 122, 255 ) : IM_COL32( 200, 40, 60, 255 );
	case Store::Retail: return dark ? IM_COL32( 236, 190, 100, 255 ) : IM_COL32( 161, 98, 7, 255 );
	case Store::Portable: return dark ? IM_COL32( 110, 220, 150, 255 ) : IM_COL32( 21, 128, 61, 255 );
	case Store::Manual: return dark ? IM_COL32( 160, 178, 206, 255 ) : IM_COL32( 71, 85, 105, 255 );
	default: return palette.textSecondary;
	}
}

std::vector<std::string> AllGroups()
{
	std::vector<std::string> groups;
	for ( unsigned i = 0; i < Com_GameManagerProfileCount(); ++i ) {
		const std::string group = GroupKey( Com_GameManagerProfileAt( i ) );
		if ( std::find( groups.begin(), groups.end(), group ) == groups.end() ) groups.push_back( group );
	}
	std::stable_sort( groups.begin(), groups.end(), []( const std::string &a, const std::string &b ) {
		return GroupOrder( a ) < GroupOrder( b );
	} );
	return groups;
}

std::vector<std::string> Words( const std::string &text, const gui::Ui &ui, const gui::Typeface &face, float width )
{
	std::vector<std::string> lines;
	std::string line, word;
	auto flushWord = [&]() {
		if ( word.empty() ) return;
		const std::string candidate = line.empty() ? word : line + " " + word;
		if ( !line.empty() && ui.Measure( face, candidate ).x > width ) {
			lines.push_back( line );
			line = word;
		} else {
			line = candidate;
		}
		word.clear();
	};
	for ( const char c : text ) {
		if ( c == ' ' ) flushWord();
		else word += c;
	}
	flushWord();
	if ( !line.empty() ) lines.push_back( line );
	return lines;
}

// Wraps text that may hold long paths: at a space or a path separator where
// it can, and between characters where a single word is wider than the line.
std::vector<std::string> WrapAnywhere( const std::string &text, const gui::Ui &ui, const gui::Typeface &face, float width )
{
	std::vector<std::string> lines;
	std::string line;
	for ( std::size_t i = 0; i < text.size(); ) {
		std::size_t next = i + 1;
		while ( next < text.size() && ( static_cast<unsigned char>( text[next] ) & 0xc0 ) == 0x80 ) ++next;
		const std::string character = text.substr( i, next - i );
		if ( !line.empty() && ui.Measure( face, line + character ).x > width ) {
			const std::size_t cut = line.find_last_of( " \\/" );
			if ( cut != std::string::npos && cut > 0 && cut + 1 < line.size() ) {
				lines.push_back( line.substr( 0, cut + 1 ) );
				line = line.substr( cut + 1 );
			} else {
				lines.push_back( line );
				line.clear();
			}
		}
		line += character;
		i = next;
	}
	if ( !line.empty() ) lines.push_back( line );
	return lines;
}

} // namespace

App::App( SDL_Window *window, SDL_Renderer *renderer, AppOptions options, Settings settings )
	: window_( window ), renderer_( renderer ), options_( std::move( options ) ), settings_( std::move( settings ) )
{
	wakeEvent_ = SDL_RegisterEvents( 1 );
	pickWakeEvent = wakeEvent_;
	installations_ = settings_.cache;
	ResolveEngine();
}

App::~App()
{
	scanCancel_ = true;
	if ( scanThread_.joinable() ) scanThread_.join();
	pickWakeEvent = 0;
	for ( auto &[key, set] : icons_ ) {
		if ( set.frame64 ) SDL_DestroyTexture( set.frame64 );
		if ( set.frame128 ) SDL_DestroyTexture( set.frame128 );
	}
	if ( appIcon_ ) SDL_DestroyTexture( appIcon_ );
}

void App::Initialize( gui::Fonts *fonts )
{
	fonts_ = fonts;
	ui_.fonts = fonts;
	appIcon_ = gui::CreateImageTexture( renderer_, FNTECH3_ICON_RGBA, int( FNTECH3_ICON_SIZE ), int( FNTECH3_ICON_SIZE ) );
	ResolveTheme();
	if ( options_.synchronousScan ) {
		DiscoveryResult result = Discover( MakeDiscoveryOptions(), nullptr, &progress_ );
		lastScanSeconds_ = result.seconds;
		ApplyInstallations( std::move( result.installations ) );
		scannedOnce_ = true;
	} else {
		StartScan();
	}
}

// ---------------------------------------------------------------------------
// Work
// ---------------------------------------------------------------------------

DiscoveryOptions App::MakeDiscoveryOptions() const
{
	DiscoveryOptions discovery;
	discovery.scanStores = options_.scanStores;
	discovery.scanCommonFolders = options_.scanCommonFolders;
	for ( const std::string &folder : settings_.searchFolders ) discovery.searchFolders.push_back( fs::FromUtf8( folder ) );
	for ( const stdfs::path &folder : options_.extraSearchFolders ) discovery.searchFolders.push_back( folder );
	for ( const ManualEntry &entry : settings_.manual ) discovery.manual.push_back( { fs::FromUtf8( entry.path ), entry.group } );
	discovery.launcherDirectory = options_.launcherDirectory;
	return discovery;
}

void App::StartScan()
{
	if ( scanRunning_ ) return;
	if ( scanThread_.joinable() ) scanThread_.join();
	scanCancel_ = false;
	scanRunning_ = true;
	progress_.candidates = 0;
	progress_.probed = 0;
	progress_.found = 0;
	const DiscoveryOptions discovery = MakeDiscoveryOptions();
	const Uint32 wake = wakeEvent_;
	scanThread_ = std::thread( [this, discovery, wake]() {
		DiscoveryResult result = Discover( discovery, &scanCancel_, &progress_ );
		{
			std::lock_guard<std::mutex> lock( scanMutex_ );
			scanResult_ = std::move( result );
		}
		scanRunning_ = false;
		SDL_Event event;
		SDL_zero( event );
		event.type = wake;
		SDL_PushEvent( &event );
	} );
}

void App::PollScan()
{
	std::optional<DiscoveryResult> result;
	{
		std::lock_guard<std::mutex> lock( scanMutex_ );
		result.swap( scanResult_ );
	}
	if ( !result ) return;
	if ( scanThread_.joinable() ) scanThread_.join();
	if ( result->cancelled ) return;
	lastScanSeconds_ = result->seconds;
	scannedOnce_ = true;
	ApplyInstallations( std::move( result->installations ) );
}

void App::ApplyInstallations( std::vector<Installation> installations )
{
	installations_ = std::move( installations );
	if ( !options_.readOnlySettings ) {
		// Rewrite the settings file only when the scan found something new.
		auto signature = []( const std::vector<Installation> &list ) {
			std::string text;
			for ( const Installation &installation : list ) {
				text += installation.id + '|' + StoreKey( installation.store ) + '|' + installation.detail + '|'
					+ installation.storeTitle + '|' + std::to_string( installation.modes.size() )
					+ ( installation.verified ? "v" : "" ) + ( installation.manual ? "m" : "" )
					+ ( installation.missing ? "x" : "" ) + ( installation.portable ? "p" : "" ) + '\n';
			}
			return text;
		};
		if ( signature( settings_.cache ) != signature( installations_ ) ) {
			settings_.cache = installations_;
			MarkDirty();
		}
	}
	fontsChecked_ = false;
	if ( !FindInstallation( selectedId_ ) ) {
		selectedId_.clear();
		const View view = BuildView();
		if ( !view.order.empty() ) selectedId_ = view.order.front()->id;
	}
}

void App::PollRunning()
{
	for ( auto it = running_.begin(); it != running_.end(); ) {
		if ( it->process->Running() ) {
			++it;
			continue;
		}
		const int code = it->process->ExitCode();
		if ( code != 0 && it->process->Seconds() < 20.0 ) {
			Notify( it->name + " closed right after starting (exit code " + std::to_string( code )
				+ "). fnTech3 shows why in its own window or its console log.", 2 );
		}
		it = running_.erase( it );
	}
	if ( minimizedForGame_ && running_.empty() ) {
		minimizedForGame_ = false;
		SDL_RestoreWindow( window_ );
		SDL_RaiseWindow( window_ );
	}
}

void App::PollFolderPicks()
{
	std::vector<std::pair<int, std::string>> picks;
	{
		std::lock_guard<std::mutex> lock( pickMutex );
		picks.swap( pickQueue );
	}
	for ( const auto &[kind, path] : picks ) {
		if ( kind == kPickGame ) {
			AddFolder( path );
		} else if ( kind == kPickEngine ) {
			settings_.enginePath = path;
			ResolveEngine();
			MarkDirty();
			if ( !engineFound_ ) Notify( "That file could not be found.", 2 );
		} else if ( kind == kPickSearch ) {
			if ( std::find( settings_.searchFolders.begin(), settings_.searchFolders.end(), path ) == settings_.searchFolders.end() ) {
				settings_.searchFolders.push_back( path );
				MarkDirty();
				StartScan();
			}
		}
	}
}

void App::AddFolder( const std::string &path )
{
	if ( path.empty() ) return;
	std::vector<Installation> found = ProbeManualFolder( fs::FromUtf8( path ) );
	if ( found.empty() ) {
		add_ = AddState();
		add_.path = path;
		openAdd_ = true;
		return;
	}
	std::string names;
	bool added = false;
	for ( const Installation &installation : found ) {
		const std::string key = fs::IdentityKey( installation.root );
		bool known = false;
		for ( const ManualEntry &entry : settings_.manual ) {
			const stdfs::path root = fs::Canonical( fs::FromUtf8( entry.path ) );
			if ( !root.empty() && fs::IdentityKey( root ) == key && ( entry.group.empty() || entry.group == installation.group ) ) {
				known = true;
			}
		}
		const bool listed = FindInstallation( installation.id ) != nullptr;
		if ( !known ) {
			settings_.manual.push_back( { installation.rootUtf8, installation.group, Now() } );
			added = true;
		}
		if ( InstallationPrefs *prefs = settings_.installations.count( installation.id )
				? &settings_.installations[installation.id] : nullptr ) {
			prefs->hidden = false;
		}
		if ( !names.empty() ) names += ", ";
		names += GroupName( installation.group );
		if ( !listed ) added = true;
	}
	std::vector<Installation> merged = installations_;
	for ( Installation &installation : found ) merged.push_back( installation );
	MergeInstallations( merged );
	installations_ = std::move( merged );
	if ( !options_.readOnlySettings ) settings_.cache = installations_;
	fontsChecked_ = false;
	selectedId_ = found.front().id;
	ensureVisible_ = selectedId_;
	MarkDirty();
	Notify( added ? "Added " + names + "." : names + " is already in your list.", added ? 1 : 0 );
}

void App::AddUnverified( const std::string &path, const std::string &group )
{
	settings_.manual.push_back( { path, group, Now() } );
	const stdfs::path folder = fs::FromUtf8( path );
	Installation installation = UnverifiedInstallation( folder, group, !fs::IsDirectory( folder ) );
	const std::string id = installation.id;
	installations_.push_back( std::move( installation ) );
	MergeInstallations( installations_ );
	if ( !options_.readOnlySettings ) settings_.cache = installations_;
	fontsChecked_ = false;
	selectedId_ = id;
	ensureVisible_ = id;
	MarkDirty();
	Notify( "Added " + GroupName( group ) + ". fnTech3 could not confirm its data, so it may not start.", 0 );
}

void App::RemoveManual( const Installation &installation )
{
	const std::string key = fs::IdentityKey( installation.root );
	const std::string name = DisplayName( installation );
	const std::string id = installation.id;
	const bool detectedElsewhere = installation.store != Store::Manual;
	auto &manual = settings_.manual;
	manual.erase( std::remove_if( manual.begin(), manual.end(), [&]( const ManualEntry &entry ) {
		stdfs::path root = fs::Canonical( fs::FromUtf8( entry.path ) );
		if ( root.empty() ) root = fs::FromUtf8( entry.path ).lexically_normal();
		return fs::IdentityKey( root ) == key && ( entry.group.empty() || entry.group == installation.group );
	} ), manual.end() );
	if ( detectedElsewhere ) {
		for ( Installation &item : installations_ ) {
			if ( item.id == id ) item.manual = false;
		}
		Notify( name + " is no longer remembered as added by you; fnTech3 still finds it by itself.", 0 );
	} else {
		installations_.erase( std::remove_if( installations_.begin(), installations_.end(),
			[&id]( const Installation &item ) { return item.id == id; } ), installations_.end() );
		settings_.installations.erase( id );
		Notify( "Removed " + name + " from the list. Its files were not touched.", 0 );
	}
	if ( !options_.readOnlySettings ) settings_.cache = installations_;
	MarkDirty();
}

bool App::BuildLaunch( const Installation &installation, const comGameProfile_t *profile, bool dedicated,
	stdfs::path &engine, std::vector<std::string> &arguments, std::string &error ) const
{
	LaunchRequest request;
	request.engine = dedicated ? dedicatedPath_ : enginePath_;
	request.profile = profile;
	request.basePath = installation.rootUtf8;
	if ( const InstallationPrefs *prefs = FindPrefs( installation.id ) ) {
		request.mod = prefs->mod;
		request.arguments = prefs->arguments;
	}
	engine = request.engine;
	return BuildArguments( request, arguments, &error );
}

void App::Launch( const Installation &installation, const comGameProfile_t *profile, bool dedicated )
{
	if ( !profile ) return;
	const std::string name = DisplayName( installation );
	if ( installation.missing ) {
		Notify( name + "'s folder is no longer there. Reconnect its drive, or remove it from the list.", 2 );
		return;
	}
	const std::string reason = ProfileUnavailableReason( profile );
	if ( !reason.empty() ) {
		Notify( reason, 2 );
		return;
	}
	if ( !( dedicated ? dedicatedFound_ : engineFound_ ) ) {
		Notify( std::string( "fnTech3's " ) + ( dedicated ? "dedicated server" : "engine" )
			+ " was not found next to the launcher. Choose it in Settings.", 2 );
		return;
	}
	stdfs::path engine;
	std::vector<std::string> arguments;
	std::string error;
	if ( !BuildLaunch( installation, profile, dedicated, engine, arguments, error ) ) {
		Notify( error, 2 );
		return;
	}
	auto process = std::make_unique<GameProcess>();
	if ( !process->Start( engine, arguments, &error ) ) {
		Notify( error, 2 );
		return;
	}
	InstallationPrefs &prefs = Prefs( installation.id );
	prefs.lastPlayed = Now();
	++prefs.playCount;
	if ( !dedicated && installation.modes.size() > 1 ) prefs.mode = profile->key;
	MarkDirty();
	running_.push_back( { std::move( process ), installation.id, name, dedicated } );
	const std::string mode = installation.modes.size() > 1 ? " (" + ModeLabel( profile ) + ")" : "";
	Notify( dedicated ? "Started a " + name + " dedicated server." : "Starting " + name + mode + "\xe2\x80\xa6", 1 );
	if ( dedicated ) return;
	if ( settings_.afterLaunch == AfterLaunch::Minimize ) {
		minimizedForGame_ = true;
		SDL_MinimizeWindow( window_ );
	} else if ( settings_.afterLaunch == AfterLaunch::Close ) {
		quit_ = true;
	}
}

void App::ResolveEngine()
{
	if ( !settings_.enginePath.empty() ) {
		enginePath_ = fs::FromUtf8( settings_.enginePath );
		dedicatedPath_ = DefaultEnginePath( enginePath_.parent_path(), true );
	} else {
		enginePath_ = DefaultEnginePath( options_.launcherDirectory, false );
		dedicatedPath_ = DefaultEnginePath( options_.launcherDirectory, true );
	}
	engineFound_ = fs::IsRegularFile( enginePath_ );
	dedicatedFound_ = fs::IsRegularFile( dedicatedPath_ );
}

void App::ShowFolderPicker()
{
	SDL_ShowOpenFolderDialog( PickCallback, reinterpret_cast<void *>( std::intptr_t( kPickGame ) ), window_, nullptr, false );
}

void App::ShowEnginePicker()
{
#ifdef _WIN32
	static const SDL_DialogFileFilter filters[] = { { "fnTech3 engine", "exe" } };
	SDL_ShowOpenFileDialog( PickCallback, reinterpret_cast<void *>( std::intptr_t( kPickEngine ) ), window_,
		filters, 1, nullptr, false );
#else
	SDL_ShowOpenFileDialog( PickCallback, reinterpret_cast<void *>( std::intptr_t( kPickEngine ) ), window_,
		nullptr, 0, nullptr, false );
#endif
}

void App::ShowSearchFolderPicker()
{
	SDL_ShowOpenFolderDialog( PickCallback, reinterpret_cast<void *>( std::intptr_t( kPickSearch ) ), window_, nullptr, false );
}

void App::OpenFolder( const stdfs::path &folder )
{
	if ( !SDL_OpenURL( FileUrl( folder ).c_str() ) ) Notify( "The folder could not be opened.", 2 );
}

void App::Notify( const std::string &text, int kind )
{
	Toast toast;
	toast.text = text;
	toast.kind = kind;
	toast.life = kind == 2 ? 8.0f : 4.5f;
	toasts_.push_back( toast );
	if ( toasts_.size() > 4 ) toasts_.erase( toasts_.begin() );
}

void App::MarkDirty()
{
	if ( options_.readOnlySettings ) return;
	if ( !dirty_ ) dirtySince_ = SDL_GetTicks();
	dirty_ = true;
}

void App::Persist( bool force )
{
	if ( options_.readOnlySettings || options_.settingsFile.empty() ) return;
	if ( !dirty_ && !force ) return;
	if ( !force && SDL_GetTicks() - dirtySince_ < 700 ) return;
	std::string error;
	if ( !launcher::SaveSettings( options_.settingsFile, settings_, &error ) ) {
		Notify( "Settings could not be saved: " + error, 2 );
	}
	dirty_ = false;
}

// ---------------------------------------------------------------------------
// Model helpers
// ---------------------------------------------------------------------------

InstallationPrefs &App::Prefs( const std::string &id )
{
	return settings_.installations[id];
}

const InstallationPrefs *App::FindPrefs( const std::string &id ) const
{
	const auto found = settings_.installations.find( id );
	return found == settings_.installations.end() ? nullptr : &found->second;
}

std::string App::DisplayName( const Installation &installation ) const
{
	const InstallationPrefs *prefs = FindPrefs( installation.id );
	return prefs && !prefs->name.empty() ? prefs->name : GroupName( installation.group );
}

const comGameProfile_t *App::SelectedMode( const Installation &installation ) const
{
	const InstallationPrefs *prefs = FindPrefs( installation.id );
	const comGameProfile_t *stored = nullptr;
	if ( prefs && !prefs->mode.empty() ) {
		for ( const comGameProfile_t *profile : installation.modes ) {
			if ( prefs->mode == profile->key ) stored = profile;
		}
	}
	if ( stored && ProfileStartsInThisBuild( stored ) ) return stored;
	for ( const comGameProfile_t *profile : installation.modes ) {
		if ( ProfileStartsInThisBuild( profile ) ) return profile;
	}
	if ( stored ) return stored;
	return installation.modes.empty() ? nullptr : installation.modes.front();
}

bool App::Playable( const Installation &installation ) const
{
	if ( installation.missing ) return false;
	for ( const comGameProfile_t *profile : installation.modes ) {
		if ( ProfileStartsInThisBuild( profile ) ) return true;
	}
	return false;
}

bool App::IsRunning( const std::string &id ) const
{
	for ( const RunningGame &game : running_ ) {
		if ( game.id == id && !game.dedicated ) return true;
	}
	return false;
}

const Installation *App::FindInstallation( const std::string &id ) const
{
	if ( id.empty() ) return nullptr;
	for ( const Installation &installation : installations_ ) {
		if ( installation.id == id ) return &installation;
	}
	return nullptr;
}

bool App::Matches( const Installation &installation, const std::string &query ) const
{
	if ( query.empty() ) return true;
	std::string haystack = DisplayName( installation ) + " " + GroupName( installation.group ) + " "
		+ StoreName( installation.store ) + " " + installation.rootUtf8 + " " + installation.storeTitle + " "
		+ installation.release;
	for ( const comGameProfile_t *profile : installation.modes ) haystack += " " + ModeLabel( profile ) + " " + profile->key;
	if ( const InstallationPrefs *prefs = FindPrefs( installation.id ) ) haystack += " " + prefs->mod;
	haystack = fs::Lower( haystack );
	std::size_t start = 0;
	const std::string lowered = fs::Lower( query );
	while ( start < lowered.size() ) {
		std::size_t end = lowered.find( ' ', start );
		if ( end == std::string::npos ) end = lowered.size();
		const std::string word = lowered.substr( start, end - start );
		if ( !word.empty() && haystack.find( word ) == std::string::npos ) return false;
		start = end + 1;
	}
	return true;
}

App::View App::BuildView() const
{
	View view;
	std::vector<const Installation *> all;
	for ( const Installation &installation : installations_ ) {
		if ( Matches( installation, search_ ) ) all.push_back( &installation );
	}
	auto name = [this]( const Installation *installation ) { return fs::Lower( DisplayName( *installation ) ); };
	auto played = [this]( const Installation *installation ) {
		const InstallationPrefs *prefs = FindPrefs( installation->id );
		return prefs ? prefs->lastPlayed : std::int64_t( 0 );
	};
	std::stable_sort( all.begin(), all.end(), [&]( const Installation *a, const Installation *b ) {
		if ( settings_.sort == SortOrder::RecentlyPlayed ) {
			const std::int64_t left = played( a ), right = played( b );
			if ( left != right ) return left > right;
		} else if ( settings_.sort == SortOrder::Location ) {
			const std::string left = StoreName( a->store ), right = StoreName( b->store );
			if ( left != right ) return left < right;
		}
		const std::string left = name( a ), right = name( b );
		if ( left != right ) return left < right;
		if ( a->store != b->store ) return StoreRank( a->store ) > StoreRank( b->store );
		return fs::Lower( a->rootUtf8 ) < fs::Lower( b->rootUtf8 );
	} );
	for ( const Installation *installation : all ) {
		const InstallationPrefs *prefs = FindPrefs( installation->id );
		if ( prefs && prefs->hidden ) {
			if ( settings_.showHidden ) view.hidden.push_back( installation );
		} else if ( Playable( *installation ) || installation->missing ) {
			view.ready.push_back( installation );
		} else if ( settings_.showUnavailable ) {
			view.unavailable.push_back( installation );
		}
	}
	view.order = view.ready;
	if ( !settings_.unavailableCollapsed ) {
		view.order.insert( view.order.end(), view.unavailable.begin(), view.unavailable.end() );
	}
	view.order.insert( view.order.end(), view.hidden.begin(), view.hidden.end() );
	return view;
}

SDL_Texture *App::IconTexture( const comGameProfile_t *profile, float pixels )
{
	if ( !profile ) return nullptr;
	IconSet &set = icons_[profile->key];
	if ( !set.loaded ) {
		set.loaded = true;
		std::size_t count = 0;
		const icon::EncodedImage *images = icon::EmbeddedImages( count );
		for ( std::size_t i = 0; i < count; ++i ) {
			if ( std::strcmp( images[i].profile, profile->key ) || ( images[i].size != 64 && images[i].size != 128 ) ) continue;
			icon::Image image;
			if ( !icon::DecodePng( images[i].png, images[i].length, image ) ) continue;
			SDL_Texture *texture = gui::CreateImageTexture( renderer_, image.rgba.data(), int( image.width ), int( image.height ) );
			( images[i].size == 64 ? set.frame64 : set.frame128 ) = texture;
		}
	}
	if ( pixels <= 64.0f && set.frame64 ) return set.frame64;
	return set.frame128 ? set.frame128 : set.frame64;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void App::ResolveTheme()
{
	const Theme theme = options_.theme ? *options_.theme : settings_.theme;
	const bool dark = theme == Theme::Dark || ( theme == Theme::System && SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT );
	if ( dark != darkTheme_ ) styleDirty_ = true;
	darkTheme_ = dark;
}

void App::HandleEvent( const SDL_Event &event )
{
	switch ( event.type ) {
	case SDL_EVENT_QUIT:
		quit_ = true;
		break;
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		if ( event.window.windowID == SDL_GetWindowID( window_ ) ) quit_ = true;
		break;
	case SDL_EVENT_DROP_BEGIN:
	case SDL_EVENT_DROP_POSITION:
		dropActive_ = true;
		break;
	case SDL_EVENT_DROP_FILE:
		dropActive_ = false;
		if ( event.drop.data ) AddFolder( event.drop.data );
		break;
	case SDL_EVENT_DROP_COMPLETE:
		dropActive_ = false;
		break;
	case SDL_EVENT_MOUSE_MOTION:
		// No motion arrives during a drag, so a drag that left the window
		// without a drop on a platform that does not report it ends here.
		dropActive_ = false;
		break;
	case SDL_EVENT_SYSTEM_THEME_CHANGED:
		ResolveTheme();
		break;
	default:
		break;
	}
}

bool App::Busy() const
{
	return scanRunning_ || ui_.animating || !toasts_.empty() || dropActive_;
}

int App::IdleWaitMilliseconds() const
{
	if ( dirty_ ) return 250;
	return running_.empty() ? 1000 : 400;
}

void App::BeforeFrame()
{
	PollScan();
	PollFolderPicks();
	PollRunning();
	float scale = options_.scale;
	if ( scale <= 0.0f ) {
		const float density = SDL_GetWindowPixelDensity( window_ );
		scale = SDL_GetWindowDisplayScale( window_ ) / ( density > 0.0f ? density : 1.0f );
	}
	scale = std::clamp( scale > 0.0f ? scale : 1.0f, 0.75f, 4.0f );
	if ( std::fabs( scale - scale_ ) > 0.01f ) {
		scale_ = scale;
		styleDirty_ = true;
	}
	ResolveTheme();
	if ( styleDirty_ ) {
		ui_.palette = darkTheme_ ? gui::DarkPalette() : gui::LightPalette();
		ui_.scale = scale_;
		gui::ApplyStyle( ui_.palette, scale_ );
		styleDirty_ = false;
	}
	if ( !fontsChecked_ && fonts_ ) {
		std::string text = search_;
		for ( const Installation &installation : installations_ ) text += installation.rootUtf8 + DisplayName( installation );
		for ( const std::string &folder : settings_.searchFolders ) text += folder;
		gui::EnsureFallbackFor( *fonts_, text );
		fontsChecked_ = true;
	}
	Persist( false );
}

void App::Frame()
{
	ImGuiIO &io = ImGui::GetIO();
	ui_.deltaTime = io.DeltaTime;
	time_ += io.DeltaTime;
	ui_.time = time_;
	ui_.animating = false;
	for ( Toast &toast : toasts_ ) toast.age += io.DeltaTime;
	toasts_.erase( std::remove_if( toasts_.begin(), toasts_.end(),
		[]( const Toast &toast ) { return toast.age >= toast.life; } ), toasts_.end() );

	if ( !previewApplied_ && scannedOnce_ ) {
		previewApplied_ = true;
		if ( !options_.selectGroup.empty() ) {
			for ( const Installation *installation : BuildView().order ) {
				if ( installation->group == options_.selectGroup ) {
					selectedId_ = installation->id;
					ensureVisible_ = selectedId_;
					jumpScroll_ = true;
					break;
				}
			}
		}
		const std::string &preview = options_.preview;
		if ( preview == "menu" && !selectedId_.empty() ) {
			menuId_ = selectedId_;
			previewMenu_ = true;
		} else if ( preview == "settings" ) {
			openSettings_ = true;
		} else if ( preview == "edit" && FindInstallation( selectedId_ ) ) {
			const Installation &installation = *FindInstallation( selectedId_ );
			edit_ = EditState();
			edit_.id = installation.id;
			edit_.mods = FindMods( installation, SelectedMode( installation ) );
			openEdit_ = true;
		} else if ( preview == "add" ) {
			add_ = AddState();
			add_.path = fs::ToUtf8( options_.launcherDirectory );
			openAdd_ = true;
		} else if ( preview == "toast" ) {
			Notify( "Starting Return to Castle Wolfenstein (Multiplayer)\xe2\x80\xa6", 1 );
			Notify( "Added Quake III Arena.", 1 );
		} else if ( preview == "drop" ) {
			dropActive_ = true;
		} else if ( preview.rfind( "search:", 0 ) == 0 ) {
			std::snprintf( search_, sizeof( search_ ), "%s", preview.c_str() + 7 );
		}
	}

	if ( previewMenu_ && rowMenuAnchors_.count( menuId_ ) ) {
		previewMenu_ = false;
		const ImVec4 anchor = rowMenuAnchors_[menuId_];
		menuAnchor_ = ImVec2( anchor.x, anchor.y );
		menuFlipY_ = anchor.w;
		menuPivot_ = ImVec2( 1.0f, 0.0f );
		openMenu_ = true;
	}
	const ImVec2 size = io.DisplaySize;
	ImGui::SetNextWindowPos( ImVec2( 0.0f, 0.0f ) );
	ImGui::SetNextWindowSize( size );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
	ImGui::Begin( "##launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse
		| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavFocus );
	ImGui::PopStyleVar( 3 );
	ImDrawList *draw = ImGui::GetWindowDrawList();
	DrawBackground( draw, size );
	DrawHeader( size.x );
	DrawToolbar( size.x );
	const View view = BuildView();
	const float listTop = ui_.S( kHeaderHeight + kToolbarHeight );
	const float footerTop = size.y - ui_.S( kFooterHeight );
	DrawList( ImVec2( 0.0f, listTop ), ImVec2( size.x, std::max( footerTop - listTop, 1.0f ) ), view );
	DrawFooter( size.x, footerTop );
	HandleKeyboard( view );
	DrawRowMenu();
	DrawSortMenu();
	ImGui::End();
	DrawEditDialog();
	DrawSettingsDialog();
	DrawAddDialog();
	DrawToasts( size );
	DrawDropOverlay( size );
}

void App::Shutdown()
{
	scanCancel_ = true;
	if ( scanThread_.joinable() ) scanThread_.join();
	PollScan();
	if ( !options_.readOnlySettings ) {
		const SDL_WindowFlags flags = SDL_GetWindowFlags( window_ );
		settings_.window.maximized = ( flags & SDL_WINDOW_MAXIMIZED ) != 0;
		if ( !( flags & ( SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN ) ) ) {
			SDL_GetWindowPosition( window_, &settings_.window.x, &settings_.window.y );
			SDL_GetWindowSize( window_, &settings_.window.width, &settings_.window.height );
		}
		dirty_ = true;
		Persist( true );
	}
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void App::DrawBackground( ImDrawList *draw, ImVec2 size )
{
	const gui::Palette &palette = ui_.palette;
	draw->AddRectFilled( ImVec2( 0.0f, 0.0f ), size, palette.background );
	// A warm light behind the mark and a cool one across the far corner.
	ui_.Glow( draw, ImVec2( ui_.S( 70.0f ), ui_.S( 30.0f ) ), ui_.S( 460.0f ), palette.glow );
	ui_.Glow( draw, ImVec2( size.x + ui_.S( 60.0f ), size.y + ui_.S( 80.0f ) ), ui_.S( 560.0f ), palette.glowCool );
}

void App::DrawHeader( float width )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const gui::Palette &palette = ui_.palette;
	const float pad = ui_.S( kSidePadding );
	const float mark = ui_.S( 46.0f );
	const ImVec2 markMin( pad, ui_.S( 22.0f ) );
	if ( appIcon_ ) {
		draw->AddImage( gui::TextureId( appIcon_ ), markMin, ImVec2( markMin.x + mark, markMin.y + mark ) );
	}
	const float textX = markMin.x + mark + ui_.S( 14.0f );
	const gui::Typeface title = ui_.Bold( 21.0f );
	ui_.Text( draw, title, ImVec2( textX, ui_.S( 22.0f ) ), palette.text, "fnTech3" );
	const float titleWidth = ui_.Measure( title, "fnTech3" ).x;
	ui_.Text( draw, ui_.Regular( 21.0f ), ImVec2( textX + titleWidth + ui_.S( 7.0f ), ui_.S( 22.0f ) ),
		palette.textSecondary, "Launcher" );

	std::size_t ready = 0;
	for ( const Installation &installation : installations_ ) {
		const InstallationPrefs *prefs = FindPrefs( installation.id );
		if ( Playable( installation ) && !( prefs && prefs->hidden ) ) ++ready;
	}
	std::string status;
	float statusX = textX;
	if ( scanRunning_ ) {
		ui_.Spinner( draw, ImVec2( textX + ui_.S( 6.0f ), ui_.S( 59.5f ) ), ui_.S( 5.5f ), palette.accent, ui_.S( 1.8f ) );
		ui_.animating = true;
		statusX += ui_.S( 18.0f );
		status = "Looking for games\xe2\x80\xa6";
		if ( progress_.candidates.load() ) {
			status += "  " + std::to_string( progress_.probed.load() ) + " of " + std::to_string( progress_.candidates.load() )
				+ " places checked";
		}
	} else if ( installations_.empty() ) {
		status = scannedOnce_ ? "No games found yet" : "Getting ready\xe2\x80\xa6";
	} else {
		status = Plural( ready, "game", "games" ) + " ready to play";
		const std::size_t others = installations_.size() - ready;
		if ( others ) status += "  \xc2\xb7  " + std::to_string( others ) + " more found";
	}
	ui_.Text( draw, ui_.Regular( 13.5f ), ImVec2( statusX, ui_.S( 51.0f ) ), palette.textSecondary, status );

	// Actions, right to left.
	const bool compact = width < ui_.S( 760.0f );
	const float height = ui_.S( 38.0f );
	const float y = ui_.S( 26.0f );
	float right = width - pad;
	right -= height;
	ImGui::SetCursorScreenPos( ImVec2( right, y ) );
	if ( ui_.IconButton( "##settings", Icon::Settings, height, "Settings  (Ctrl+,)" ) ) openSettings_ = true;
	const std::string addLabel = compact ? "" : "Add game";
	const float addWidth = compact ? height : ui_.ButtonWidth( addLabel, height, true );
	right -= ui_.S( 10.0f ) + addWidth;
	ImGui::SetCursorScreenPos( ImVec2( right, y ) );
	bool hovered = false;
	if ( ui_.Button( "##add", addLabel, ButtonKind::Secondary, height, addWidth, true, Icon::Plus, true, &hovered ) ) {
		ShowFolderPicker();
	}
	if ( hovered && ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) {
		ui_.Tooltip( "Add a game from a folder fnTech3 did not search. You can also drop the folder on this window.  (Ctrl+O)" );
	}
	const std::string scanLabel = compact ? "" : scanRunning_ ? "Scanning" : "Rescan";
	const float scanWidth = compact ? height : ui_.ButtonWidth( "Scanning", height, true );
	right -= ui_.S( 6.0f ) + scanWidth;
	ImGui::SetCursorScreenPos( ImVec2( right, y ) );
	// The refresh arrow turns into a spinner while the scan runs.
	ui_.spinNextIcon = scanRunning_;
	if ( ui_.Button( "##rescan", scanLabel, ButtonKind::Ghost, height, scanWidth, !scanRunning_, Icon::Refresh, true,
			&hovered ) ) {
		StartScan();
	}
	if ( !scanRunning_ && hovered && ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) {
		ui_.Tooltip( "Look for games again  (F5)" );
	}
}

void App::DrawToolbar( float width )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const gui::Palette &palette = ui_.palette;
	const float pad = ui_.S( kSidePadding );
	const float y = ui_.S( kHeaderHeight );
	const float height = ui_.S( 42.0f );
	const char *sortNames[] = { "Name", "Recently played", "Location" };
	const std::string sortLabel = sortNames[int( settings_.sort )];
	const float sortWidth = ui_.ButtonWidth( sortLabel, height, true );
	const float searchWidth = std::max( width - pad * 2.0f - sortWidth - ui_.S( 10.0f ), ui_.S( 120.0f ) );

	ImGui::PushFont( fonts_->regular, 14.5f );
	const float fontSize = ImGui::GetFontSize();
	ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, ui_.S( 11.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( ui_.S( 42.0f ), ( height - fontSize ) * 0.5f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, ui_.S( 1.0f ) );
	ImGui::PushStyleColor( ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4( palette.field ) );
	ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4( palette.fieldHover ) );
	ImGui::PushStyleColor( ImGuiCol_FrameBgActive, ImGui::ColorConvertU32ToFloat4( palette.field ) );
	ImGui::PushStyleColor( ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4( palette.fieldBorder ) );
	ImGui::PushStyleColor( ImGuiCol_TextDisabled, ImGui::ColorConvertU32ToFloat4( palette.textFaint ) );
	ImGui::SetCursorScreenPos( ImVec2( pad, y ) );
	if ( focusSearch_ ) {
		ImGui::SetKeyboardFocusHere();
		focusSearch_ = false;
	}
	ImGui::SetNextItemWidth( searchWidth );
	ImGui::SetNextItemAllowOverlap();
	ImGui::InputTextWithHint( "##search", "Search games, stores and folders", search_, sizeof( search_ ) );
	const bool active = ImGui::IsItemActive();
	const ImVec2 fieldMin = ImGui::GetItemRectMin(), fieldMax = ImGui::GetItemRectMax();
	if ( ImGui::IsItemEdited() ) fontsChecked_ = false;
	ImGui::PopStyleColor( 5 );
	ImGui::PopStyleVar( 3 );
	ImGui::PopFont();
	if ( active ) {
		draw->AddRect( ImVec2( fieldMin.x - ui_.S( 1.0f ), fieldMin.y - ui_.S( 1.0f ) ),
			ImVec2( fieldMax.x + ui_.S( 1.0f ), fieldMax.y + ui_.S( 1.0f ) ), WithAlpha( palette.accent, 0.35f ),
			ui_.S( 12.0f ), ui_.S( 3.0f ) );
		draw->AddRect( fieldMin, fieldMax, palette.fieldFocus, ui_.S( 11.0f ), ui_.S( 1.2f ) );
	}
	gui::DrawIcon( draw, Icon::Search, ImVec2( fieldMin.x + ui_.S( 22.0f ), ( fieldMin.y + fieldMax.y ) * 0.5f ), ui_.S( 18.0f ),
		active ? palette.accent : palette.textFaint, ui_.S( 1.7f ) );
	if ( search_[0] ) {
		const float button = ui_.S( 28.0f );
		ImGui::SetCursorScreenPos( ImVec2( fieldMax.x - button - ui_.S( 8.0f ), ( fieldMin.y + fieldMax.y - button ) * 0.5f ) );
		if ( ui_.IconButton( "##clear", Icon::Close, button, "Clear the search  (Esc)" ) ) search_[0] = '\0';
	} else if ( !active ) {
		const std::string hint = "Ctrl+F";
		const gui::Typeface face = ui_.Medium( 11.5f );
		const float hintWidth = ui_.Measure( face, hint ).x + ui_.S( 14.0f );
		const ImVec2 hintMin( fieldMax.x - hintWidth - ui_.S( 10.0f ), ( fieldMin.y + fieldMax.y ) * 0.5f - ui_.S( 11.0f ) );
		const ImVec2 hintMax( hintMin.x + hintWidth, hintMin.y + ui_.S( 22.0f ) );
		draw->AddRect( hintMin, hintMax, palette.fieldBorder, ui_.S( 6.0f ), ui_.S( 1.0f ) );
		ui_.Text( draw, face, ImVec2( hintMin.x + ui_.S( 7.0f ), hintMin.y + ( ui_.S( 22.0f ) - ui_.Measure( face, hint ).y ) * 0.5f ),
			palette.textFaint, hint );
	}
	ImGui::SetCursorScreenPos( ImVec2( pad + searchWidth + ui_.S( 10.0f ), y ) );
	bool hovered = false;
	if ( ui_.Button( "##sort", sortLabel, ButtonKind::Secondary, height, sortWidth, true, Icon::Sort, true, &hovered ) ) {
		ImGui::OpenPopup( "##sort-menu" );
	}
	if ( hovered && ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) ui_.Tooltip( "Sort and filter the list" );
}

float App::DrawSection( ImDrawList *draw, float x, float y, float width, const std::string &label, std::size_t count,
	bool collapsible, bool *collapsed )
{
	const gui::Palette &palette = ui_.palette;
	const float height = ui_.S( 30.0f );
	bool hovered = false;
	if ( collapsible && collapsed ) {
		ImGui::SetCursorScreenPos( ImVec2( x, y ) );
		ImGui::PushID( label.c_str() );
		if ( ImGui::InvisibleButton( "##section", ImVec2( width, height ) ) ) {
			*collapsed = !*collapsed;
			MarkDirty();
		}
		hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if ( hovered ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
	}
	const ImU32 color = hovered ? palette.textSecondary : palette.textFaint;
	float textX = x + ui_.S( 4.0f );
	if ( collapsible && collapsed ) {
		gui::DrawIcon( draw, *collapsed ? Icon::ChevronRight : Icon::ChevronDown,
			ImVec2( textX + ui_.S( 6.0f ), y + height * 0.5f ), ui_.S( 12.0f ), color, ui_.S( 1.6f ) );
		textX += ui_.S( 18.0f );
	}
	const gui::Typeface face = ui_.Medium( 11.5f );
	const std::string text = Upper( label ) + "   " + std::to_string( count );
	const ImVec2 textSize = ui_.Measure( face, text );
	ui_.Text( draw, face, ImVec2( textX, y + ( height - textSize.y ) * 0.5f ), color, text );
	const float lineX = textX + textSize.x + ui_.S( 12.0f );
	if ( lineX < x + width ) {
		draw->AddLine( ImVec2( lineX, y + height * 0.5f ), ImVec2( x + width, y + height * 0.5f ), palette.separator, ui_.S( 1.0f ) );
	}
	return y + height;
}

void App::DrawList( ImVec2 origin, ImVec2 size, const View &view )
{
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetCursorScreenPos( origin );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ScrollbarSize, ui_.S( 9.0f ) );
	ImGui::BeginChild( "##list", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground );
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const float pad = ui_.S( kSidePadding );
	const float rowHeight = ui_.S( kRowHeight );
	const float gap = ui_.S( kRowGap );
	const ImVec2 windowPos = ImGui::GetWindowPos();
	const float scrollY = ImGui::GetScrollY();
	const float viewHeight = ImGui::GetWindowHeight();
	const float scrollbar = ImGui::GetScrollMaxY() > 0.0f ? ui_.S( 6.0f ) : 0.0f;
	const float left = windowPos.x + pad;
	const float width = ImGui::GetWindowWidth() - pad * 2.0f - scrollbar;
	float y = ui_.S( 6.0f );
	auto screenY = [&]( float contentY ) { return windowPos.y + contentY - scrollY; };
	auto visible = [&]( float contentY, float height ) {
		return contentY + height >= scrollY - rowHeight && contentY <= scrollY + viewHeight + rowHeight;
	};

	if ( installations_.empty() ) {
		if ( scanRunning_ || !scannedOnce_ ) DrawSkeleton( draw, ImVec2( left, screenY( y ) ), width );
		else DrawEmpty( windowPos, ImVec2( ImGui::GetWindowWidth(), viewHeight ) );
	} else if ( view.ready.empty() && view.unavailable.empty() && view.hidden.empty() ) {
		DrawEmpty( windowPos, ImVec2( ImGui::GetWindowWidth(), viewHeight ) );
	} else {
		rowPositions_.clear();
		rowMenuAnchors_.clear();
		auto rows = [&]( const std::vector<const Installation *> &list, bool unavailable ) {
			for ( const Installation *installation : list ) {
				rowPositions_[installation->id] = y;
				if ( visible( y, rowHeight ) ) {
					DrawRow( *installation, ImVec2( left, screenY( y ) ), ImVec2( left + width, screenY( y ) + rowHeight ),
						unavailable );
				}
				y += rowHeight + gap;
			}
		};
		const bool sections = !view.unavailable.empty() || !view.hidden.empty();
		if ( !view.ready.empty() ) {
			if ( sections ) y = DrawSection( draw, left, screenY( y ), width, "Ready to play", view.ready.size(), false, nullptr )
				- windowPos.y + scrollY;
			rows( view.ready, false );
		}
		if ( !view.unavailable.empty() ) {
			if ( !view.ready.empty() ) y += ui_.S( 8.0f );
			y = DrawSection( draw, left, screenY( y ), width, "Not playable in this build", view.unavailable.size(), true,
				&settings_.unavailableCollapsed ) - windowPos.y + scrollY;
			if ( !settings_.unavailableCollapsed ) rows( view.unavailable, true );
		}
		if ( !view.hidden.empty() ) {
			y += ui_.S( 8.0f );
			y = DrawSection( draw, left, screenY( y ), width, "Hidden", view.hidden.size(), false, nullptr )
				- windowPos.y + scrollY;
			rows( view.hidden, true );
		}
		y += ui_.S( 14.0f );
	}
	ImGui::SetCursorPos( ImVec2( 0.0f, y ) );
	ImGui::Dummy( ImVec2( 1.0f, 1.0f ) );

	// Smooth scrolling: the wheel moves a target the view eases toward.
	const float maxScroll = ImGui::GetScrollMaxY();
	if ( std::fabs( scrollY - listScroll_ ) > 1.0f ) {
		// Something else moved the view, such as the scroll bar.
		listScroll_ = scrollY;
		scrollTarget_ = scrollY;
	}
	if ( ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows ) && io.MouseWheel != 0.0f
		&& !ImGui::IsPopupOpen( nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) ) {
		scrollTarget_ -= io.MouseWheel * ( rowHeight + gap ) * 1.15f;
	}
	// The first frame of new content does not know its scroll range yet.
	const bool layoutKnown = maxScroll > 0.0f || y <= viewHeight;
	if ( !ensureVisible_.empty() && layoutKnown ) {
		const auto found = rowPositions_.find( ensureVisible_ );
		if ( found != rowPositions_.end() ) {
			if ( found->second < scrollTarget_ + ui_.S( 6.0f ) ) scrollTarget_ = found->second - ui_.S( 12.0f );
			else if ( found->second + rowHeight > scrollTarget_ + viewHeight - ui_.S( 6.0f ) ) {
				scrollTarget_ = found->second + rowHeight - viewHeight + ui_.S( 14.0f );
			}
		}
		ensureVisible_.clear();
	}
	scrollTarget_ = std::clamp( scrollTarget_, 0.0f, std::max( maxScroll, 0.0f ) );
	if ( jumpScroll_ && layoutKnown ) {
		listScroll_ = scrollTarget_;
		jumpScroll_ = false;
	}
	if ( ui_.Approach( listScroll_, scrollTarget_, 18.0f ) ) ui_.animating = true;
	ImGui::SetScrollY( listScroll_ );
	ImGui::EndChild();
	ImGui::PopStyleVar( 2 );
}

void App::DrawRow( const Installation &installation, ImVec2 min, ImVec2 max, bool unavailable )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const gui::Palette &palette = ui_.palette;
	ImGuiIO &io = ImGui::GetIO();
	ImGui::PushID( installation.id.c_str() );
	const ImVec2 size( max.x - min.x, max.y - min.y );
	ImGui::SetCursorScreenPos( min );
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton( "##row", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight );
	const bool rowHovered = ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenOverlappedByItem );
	const bool rowItself = ImGui::IsItemHovered();
	if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) || ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
		selectedId_ = installation.id;
	}
	if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
		menuId_ = installation.id;
		menuAnchor_ = io.MousePos;
		menuFlipY_ = io.MousePos.y;
		menuPivot_ = ImVec2( 0.0f, 0.0f );
		openMenu_ = true;
	}
	const comGameProfile_t *mode = SelectedMode( installation );
	const bool playable = Playable( installation );
	const bool running = IsRunning( installation.id );
	if ( rowItself && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) && playable && !running ) {
		Launch( installation, mode, false );
	}
	const bool selected = selectedId_ == installation.id;
	const float hover = ui_.Animate( ImGui::GetID( "hover" ), rowHovered ? 1.0f : 0.0f, 14.0f, nullptr );
	const float chosen = ui_.Animate( ImGui::GetID( "selected" ), selected ? 1.0f : 0.0f, 14.0f, nullptr );
	const float lift = ui_.S( 1.0f ) * hover;
	const ImVec2 a( min.x, min.y - lift ), b( max.x, max.y - lift );
	const float radius = ui_.S( 14.0f );

	if ( !palette.dark ) ui_.Shadow( draw, a, b, radius, ui_.S( 6.0f + 6.0f * hover ), WithAlpha( palette.shadow, 0.7f + 0.3f * hover ) );
	else if ( hover > 0.0f ) ui_.Shadow( draw, a, b, radius, ui_.S( 12.0f ), WithAlpha( palette.shadow, 0.55f * hover ) );
	if ( chosen > 0.0f ) ui_.Shadow( draw, a, b, radius, ui_.S( 9.0f ), WithAlpha( palette.accent, ( palette.dark ? 0.22f : 0.16f ) * chosen ) );
	ImU32 fill = Mix( palette.card, palette.cardHover, hover );
	fill = Mix( fill, palette.cardSelected, chosen );
	draw->AddRectFilled( a, b, fill, radius );
	ImU32 border = Mix( palette.cardBorder, palette.cardBorderHover, hover );
	border = Mix( border, WithAlpha( palette.accent, 0.85f ), chosen );
	draw->AddRect( a, b, border, radius, ui_.S( 1.0f ) + ui_.S( 0.5f ) * chosen );
	const float dim = unavailable ? 0.5f : 1.0f;

	// Game icon.
	const float iconSize = ui_.S( 54.0f );
	const ImVec2 iconMin( a.x + ui_.S( 13.0f ), a.y + ( size.y - iconSize ) * 0.5f );
	const ImVec2 iconMax( iconMin.x + iconSize, iconMin.y + iconSize );
	draw->AddRectFilled( ImVec2( iconMin.x - ui_.S( 1.0f ), iconMin.y - ui_.S( 1.0f ) ),
		ImVec2( iconMax.x + ui_.S( 1.0f ), iconMax.y + ui_.S( 1.0f ) ), palette.iconPlate, ui_.S( 13.0f ) );
	if ( SDL_Texture *texture = IconTexture( mode, iconSize * io.DisplayFramebufferScale.x ) ) {
		draw->AddImageRounded( gui::TextureId( texture ), iconMin, iconMax, ImVec2( 0.0f, 0.0f ), ImVec2( 1.0f, 1.0f ),
			WithAlpha( IM_COL32( 255, 255, 255, 255 ), unavailable ? 0.42f : 1.0f ), ui_.S( 12.0f ) );
	}
	if ( running ) {
		const ImVec2 badge( iconMax.x - ui_.S( 3.0f ), iconMin.y + ui_.S( 3.0f ) );
		draw->AddCircleFilled( badge, ui_.S( 7.0f ), fill, 20 );
		const float pulse = 0.5f + 0.5f * std::sin( time_ * 3.0f );
		draw->AddCircleFilled( badge, ui_.S( 4.5f ) + ui_.S( 1.0f ) * pulse, palette.success, 20 );
		ui_.animating = true;
	}

	// Controls on the right, laid out right to left.
	const InstallationPrefs *prefs = FindPrefs( installation.id );
	float right = b.x - ui_.S( 14.0f );
	const float moreSize = ui_.S( 34.0f );
	const ImVec2 moreMin( right - moreSize, a.y + ( size.y - moreSize ) * 0.5f );
	right = moreMin.x - ui_.S( 8.0f );

	std::string playLabel;
	ButtonKind playKind = ButtonKind::Primary;
	bool playEnabled = true, playIcon = true;
	std::string playTip;
	if ( installation.missing ) {
		playLabel = "Missing";
		playKind = ButtonKind::Subtle;
		playEnabled = false;
		playIcon = false;
		playTip = "This folder is no longer there. Reconnect its drive, or remove it from the list.";
	} else if ( unavailable && !playable ) {
		playLabel = "Not yet";
		playKind = ButtonKind::Subtle;
		playEnabled = false;
		playIcon = false;
		playTip = ProfileUnavailableReason( mode );
		if ( const char *option = ProfileDevelopmentOption( mode ); *option ) {
			playTip += std::string( "\n\nDevelopers: build with -D" ) + option + "=true to try it.";
		}
	} else if ( running ) {
		playLabel = "Running";
		playKind = ButtonKind::Subtle;
		playIcon = false;
		playTip = "Already running. Use the menu to start another copy.";
	} else if ( !playable ) {
		playLabel = "Hidden";
		playKind = ButtonKind::Subtle;
		playEnabled = false;
		playIcon = false;
	} else {
		playLabel = "Play";
	}
	const float playHeight = ui_.S( 38.0f );
	const float playWidth = std::max( ui_.S( 106.0f ), ui_.ButtonWidth( playLabel, playHeight, playIcon ) );
	const ImVec2 playMin( right - playWidth, a.y + ( size.y - playHeight ) * 0.5f );
	right = playMin.x - ui_.S( 14.0f );

	const float textLeft = iconMax.x + ui_.S( 16.0f );
	std::vector<std::string> modeLabels;
	std::vector<bool> modeEnabled;
	std::vector<std::string> modeTips;
	int modeIndex = 0;
	bool showModes = installation.modes.size() > 1 && !installation.missing && playable;
	const float modeHeight = ui_.S( 32.0f );
	float modeWidth = 0.0f;
	if ( showModes ) {
		for ( std::size_t i = 0; i < installation.modes.size(); ++i ) {
			const comGameProfile_t *profile = installation.modes[i];
			modeLabels.push_back( ModeLabel( profile ) );
			const std::string reason = ProfileUnavailableReason( profile );
			modeEnabled.push_back( reason.empty() );
			modeTips.push_back( reason );
			if ( profile == mode ) modeIndex = int( i );
		}
		modeWidth = ui_.SegmentedWidth( modeLabels, modeHeight );
		// Too narrow for the switch: the mode moves into the row's tags and the menu.
		if ( right - modeWidth - textLeft < ui_.S( 230.0f ) ) showModes = false;
	}
	const float textRight = showModes ? right - modeWidth - ui_.S( 16.0f ) : right;
	const float textWidth = std::max( textRight - textLeft, ui_.S( 40.0f ) );

	// Title and tags.
	struct Tag {
		std::string text;
		ImU32 color;
		bool dot;
	};
	std::vector<Tag> tags;
	tags.push_back( { StoreName( installation.store ), StoreColor( palette, installation.store ), false } );
	if ( running ) tags.push_back( { "Running", palette.success, true } );
	if ( installation.missing ) tags.push_back( { "Folder not found", palette.danger, false } );
	if ( !installation.verified && !installation.missing ) tags.push_back( { "Unverified", palette.warning, false } );
	if ( mode && mode->runtimeBlocker && ProfileStartsInThisBuild( mode ) ) tags.push_back( { "Experimental", palette.warning, false } );
	if ( !installation.release.empty() ) tags.push_back( { "Release " + installation.release, palette.textSecondary, false } );
	if ( installation.modes.size() > 1 && !showModes && mode ) tags.push_back( { ModeLabel( mode ), palette.textSecondary, false } );
	if ( prefs && !prefs->mod.empty() ) tags.push_back( { "Mod: " + ModDisplayName( prefs->mod ), palette.textSecondary, false } );
	const float tagHeight = ui_.S( 21.0f );
	const float tagGap = ui_.S( 6.0f );
	auto tagsWidth = [&]() {
		float total = 0.0f;
		for ( const Tag &tag : tags ) total += ui_.PillWidth( tag.text, tagHeight, tag.dot ) + tagGap;
		return total;
	};
	while ( tags.size() > 1 && textWidth - tagsWidth() - ui_.S( 10.0f ) < ui_.S( 150.0f ) ) tags.pop_back();
	const gui::Typeface titleFace = ui_.Medium( 16.5f );
	const std::string fullTitle = DisplayName( installation );
	const std::string title = ui_.FitEnd( titleFace, fullTitle, std::max( textWidth - tagsWidth() - ui_.S( 10.0f ), ui_.S( 60.0f ) ) );
	const ImVec2 titleSize = ui_.Measure( titleFace, title );
	const float titleY = a.y + ui_.S( 16.0f );
	ui_.Text( draw, titleFace, ImVec2( textLeft, titleY ), WithAlpha( palette.text, dim ), title );
	float tagX = textLeft + titleSize.x + ui_.S( 10.0f );
	const float tagY = titleY + ( titleSize.y - tagHeight ) * 0.5f + ui_.S( 0.5f );
	for ( const Tag &tag : tags ) {
		const ImU32 color = WithAlpha( tag.color, unavailable ? 0.7f : 1.0f );
		tagX += ui_.Pill( draw, ImVec2( tagX, tagY ), tag.text, color, WithAlpha( tag.color, palette.dark ? 0.13f : 0.09f ),
			WithAlpha( tag.color, palette.dark ? 0.20f : 0.22f ), tagHeight, tag.dot ) + tagGap;
	}

	// The faint path underneath.
	const gui::Typeface pathFace = ui_.Regular( 13.0f );
	const std::string path = ui_.FitPath( pathFace, installation.rootUtf8, textWidth );
	const float pathY = a.y + ui_.S( 46.0f );
	const ImVec2 pathSize = ui_.Measure( pathFace, path );
	ui_.Text( draw, pathFace, ImVec2( textLeft, pathY ), WithAlpha( palette.textFaint, unavailable ? 0.8f : 1.0f ), path );
	const ImVec2 mouse = io.MousePos;
	if ( rowItself && mouse.x >= textLeft && mouse.x <= textLeft + std::max( pathSize.x, titleSize.x )
		&& mouse.y >= titleY && mouse.y <= pathY + pathSize.y && ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) {
		std::string tip = fullTitle != GroupName( installation.group ) ? GroupName( installation.group ) + "\n" : "";
		tip += installation.rootUtf8;
		if ( !installation.detail.empty() ) tip += "\n" + installation.detail;
		if ( !installation.storeTitle.empty() && installation.storeTitle != fullTitle ) tip += " (\"" + installation.storeTitle + "\")";
		tip += "\n" + PlayedWhen( prefs ? prefs->lastPlayed : 0 );
		ui_.Tooltip( tip, 520.0f );
	}

	// Mode switch, play button and menu: submitted after the row so they win the mouse.
	if ( showModes ) {
		ImGui::SetCursorScreenPos( ImVec2( right - modeWidth, a.y + ( size.y - modeHeight ) * 0.5f ) );
		if ( ui_.Segmented( "##modes", modeLabels, modeIndex, modeHeight, &modeEnabled, &modeTips ) ) {
			Prefs( installation.id ).mode = installation.modes[std::size_t( modeIndex )]->key;
			selectedId_ = installation.id;
			MarkDirty();
		}
	}
	ImGui::SetCursorScreenPos( playMin );
	bool playHovered = false;
	if ( ui_.Button( "##play", playLabel, playKind, playHeight, playWidth, playEnabled && !running, Icon::Play, playIcon,
			&playHovered ) ) {
		selectedId_ = installation.id;
		Launch( installation, mode, false );
	}
	if ( playHovered && !playTip.empty() && ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) ui_.Tooltip( playTip );
	ImGui::SetCursorScreenPos( moreMin );
	const ImVec2 menuCorner( moreMin.x + moreSize, moreMin.y + moreSize + ui_.S( 6.0f ) );
	rowMenuAnchors_[installation.id] = ImVec4( menuCorner.x, menuCorner.y, moreMin.x, moreMin.y - ui_.S( 6.0f ) );
	if ( ui_.IconButton( "##more", Icon::More, moreSize, "More" ) ) {
		selectedId_ = installation.id;
		menuId_ = installation.id;
		menuAnchor_ = menuCorner;
		menuFlipY_ = moreMin.y - ui_.S( 6.0f );
		menuPivot_ = ImVec2( 1.0f, 0.0f );
		openMenu_ = true;
	}
	ImGui::PopID();
}

void App::DrawSkeleton( ImDrawList *draw, ImVec2 min, float width )
{
	const gui::Palette &palette = ui_.palette;
	const float rowHeight = ui_.S( kRowHeight ), gap = ui_.S( kRowGap );
	for ( int i = 0; i < 4; ++i ) {
		const ImVec2 a( min.x, min.y + float( i ) * ( rowHeight + gap ) );
		const ImVec2 b( a.x + width, a.y + rowHeight );
		draw->AddRectFilled( a, b, palette.card, ui_.S( 14.0f ) );
		draw->AddRect( a, b, palette.cardBorder, ui_.S( 14.0f ), ui_.S( 1.0f ) );
		const float pulse = 0.5f + 0.5f * std::sin( time_ * 3.2f - float( i ) * 0.7f );
		const ImU32 shade = Mix( palette.skeleton, palette.skeletonShine, pulse );
		const ImVec2 icon( a.x + ui_.S( 13.0f ), a.y + ( rowHeight - ui_.S( 54.0f ) ) * 0.5f );
		draw->AddRectFilled( icon, ImVec2( icon.x + ui_.S( 54.0f ), icon.y + ui_.S( 54.0f ) ), shade, ui_.S( 12.0f ) );
		const float textLeft = icon.x + ui_.S( 70.0f );
		draw->AddRectFilled( ImVec2( textLeft, a.y + ui_.S( 20.0f ) ),
			ImVec2( textLeft + width * ( 0.26f + 0.05f * float( i % 2 ) ), a.y + ui_.S( 33.0f ) ), shade, ui_.S( 6.0f ) );
		draw->AddRectFilled( ImVec2( textLeft, a.y + ui_.S( 46.0f ) ),
			ImVec2( textLeft + width * ( 0.42f - 0.06f * float( i % 3 ) ), a.y + ui_.S( 57.0f ) ),
			Mix( shade, palette.card, 0.35f ), ui_.S( 6.0f ) );
		draw->AddRectFilled( ImVec2( b.x - ui_.S( 162.0f ), a.y + ( rowHeight - ui_.S( 38.0f ) ) * 0.5f ),
			ImVec2( b.x - ui_.S( 56.0f ), a.y + ( rowHeight + ui_.S( 38.0f ) ) * 0.5f ), shade, ui_.S( 19.0f ) );
	}
	ui_.animating = true;
}

void App::DrawEmpty( ImVec2 origin, ImVec2 size )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const gui::Palette &palette = ui_.palette;
	const bool searching = search_[0] != '\0' && !installations_.empty();
	const float column = std::min( size.x - ui_.S( 48.0f ), ui_.S( 460.0f ) );
	const float centerX = origin.x + size.x * 0.5f;
	float y = origin.y + std::max( size.y * 0.5f - ui_.S( 150.0f ), ui_.S( 24.0f ) );
	const float mark = ui_.S( 76.0f );
	if ( searching ) {
		const ImVec2 center( centerX, y + mark * 0.5f );
		draw->AddCircleFilled( center, mark * 0.5f, palette.iconPlate, 48 );
		gui::DrawIcon( draw, Icon::Search, center, mark * 0.46f, palette.textFaint, ui_.S( 2.2f ) );
	} else {
		ui_.Glow( draw, ImVec2( centerX, y + mark * 0.5f ), mark * 1.6f, WithAlpha( palette.accent, 0.5f ) );
		if ( appIcon_ ) {
			draw->AddImage( gui::TextureId( appIcon_ ), ImVec2( centerX - mark * 0.5f, y ), ImVec2( centerX + mark * 0.5f, y + mark ) );
		}
	}
	y += mark + ui_.S( 22.0f );
	const std::string heading = searching ? "No games match \xe2\x80\x9c" + std::string( search_ ) + "\xe2\x80\x9d" : "No games found yet";
	const gui::Typeface headingFace = ui_.Bold( 20.0f );
	const std::string fittedHeading = ui_.FitEnd( headingFace, heading, column );
	const ImVec2 headingSize = ui_.Measure( headingFace, fittedHeading );
	ui_.Text( draw, headingFace, ImVec2( centerX - headingSize.x * 0.5f, y ), palette.text, fittedHeading );
	y += headingSize.y + ui_.S( 10.0f );
	const std::string body = searching
		? "Try another word: a game's name, a store such as Steam or GOG, or part of its folder."
		: "fnTech3 looked in Steam, GOG, the usual game folders and the installers' registrations. "
		  "If your game is somewhere else, add its folder, or drop the folder on this window.";
	const gui::Typeface bodyFace = ui_.Regular( 14.0f );
	for ( const std::string &line : Words( body, ui_, bodyFace, column ) ) {
		const ImVec2 lineSize = ui_.Measure( bodyFace, line );
		ui_.Text( draw, bodyFace, ImVec2( centerX - lineSize.x * 0.5f, y ), palette.textSecondary, line );
		y += lineSize.y + ui_.S( 4.0f );
	}
	y += ui_.S( 18.0f );
	const float height = ui_.S( 40.0f );
	if ( searching ) {
		const float width = ui_.ButtonWidth( "Clear the search", height, true );
		ImGui::SetCursorScreenPos( ImVec2( centerX - width * 0.5f, y ) );
		if ( ui_.Button( "##empty-clear", "Clear the search", ButtonKind::Secondary, height, width, true, Icon::Close, true ) ) {
			search_[0] = '\0';
		}
		return;
	}
	const float addWidth = ui_.ButtonWidth( "Add a game folder", height, true );
	const float scanWidth = ui_.ButtonWidth( "Search again", height, true );
	float x = centerX - ( addWidth + scanWidth + ui_.S( 10.0f ) ) * 0.5f;
	ImGui::SetCursorScreenPos( ImVec2( x, y ) );
	if ( ui_.Button( "##empty-add", "Add a game folder", ButtonKind::Primary, height, addWidth, true, Icon::Folder, true ) ) {
		ShowFolderPicker();
	}
	x += addWidth + ui_.S( 10.0f );
	ImGui::SetCursorScreenPos( ImVec2( x, y ) );
	if ( ui_.Button( "##empty-scan", "Search again", ButtonKind::Secondary, height, scanWidth, !scanRunning_, Icon::Refresh, true ) ) {
		StartScan();
	}
}

void App::DrawFooter( float width, float y )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const gui::Palette &palette = ui_.palette;
	const float pad = ui_.S( kSidePadding );
	const float height = ui_.S( kFooterHeight );
	draw->AddLine( ImVec2( 0.0f, y ), ImVec2( width, y ), palette.separator, ui_.S( 1.0f ) );
	const gui::Typeface face = ui_.Regular( 12.5f );
	const float textY = y + ( height - ui_.Measure( face, "Ag" ).y ) * 0.5f;
	const float iconY = y + height * 0.5f;
	std::string engine;
	ImU32 engineColor = palette.textFaint;
	if ( engineFound_ ) {
		gui::DrawIcon( draw, Icon::Check, ImVec2( pad + ui_.S( 6.0f ), iconY ), ui_.S( 13.0f ), palette.success, ui_.S( 1.7f ) );
		engine = std::string( "fnTech3 " FNTECH3_VERSION_STRING "  \xc2\xb7  " ) + fs::ToUtf8( enginePath_.filename() );
	} else {
		gui::DrawIcon( draw, Icon::Warning, ImVec2( pad + ui_.S( 6.0f ), iconY ), ui_.S( 14.0f ), palette.warning, ui_.S( 1.6f ) );
		engine = "fnTech3's engine was not found next to the launcher. Choose it in Settings.";
		engineColor = palette.warning;
	}
	const float engineX = pad + ui_.S( 18.0f );
	const std::string engineText = ui_.FitEnd( face, engine, width * 0.55f );
	ui_.Text( draw, face, ImVec2( engineX, textY ), engineColor, engineText );
	if ( !engineFound_ ) {
		ImGui::SetCursorScreenPos( ImVec2( engineX, y ) );
		if ( ImGui::InvisibleButton( "##engine-warning", ImVec2( ui_.Measure( face, engineText ).x, height ) ) ) openSettings_ = true;
		if ( ImGui::IsItemHovered() ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
	}

	std::string summary;
	if ( scanRunning_ ) {
		summary = "Checking " + std::to_string( progress_.probed.load() ) + " of " + std::to_string( progress_.candidates.load() )
			+ " places\xe2\x80\xa6";
	} else if ( scannedOnce_ ) {
		char seconds[32];
		std::snprintf( seconds, sizeof( seconds ), "%.1f", lastScanSeconds_ );
		summary = "Scanned " + std::to_string( progress_.probed.load() ) + " places in " + seconds + " s";
	}
	std::size_t hidden = 0;
	for ( const auto &[id, prefs] : settings_.installations ) {
		if ( prefs.hidden && FindInstallation( id ) ) ++hidden;
	}
	float right = width - pad;
	if ( hidden ) {
		const std::string link = settings_.showHidden ? "Hide hidden games" : "Show " + Plural( hidden, "hidden game", "hidden games" );
		const gui::Typeface linkFace = ui_.Medium( 12.5f );
		const float linkWidth = ui_.Measure( linkFace, link ).x;
		right -= linkWidth;
		ImGui::SetCursorScreenPos( ImVec2( right, y ) );
		if ( ImGui::InvisibleButton( "##hidden-link", ImVec2( linkWidth, height ) ) ) {
			settings_.showHidden = !settings_.showHidden;
			MarkDirty();
		}
		const bool hovered = ImGui::IsItemHovered();
		if ( hovered ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
		ui_.Text( draw, linkFace, ImVec2( right, textY ), hovered ? palette.accentHover : palette.accent, link );
		right -= ui_.S( 18.0f );
	}
	if ( !summary.empty() ) {
		const float summaryWidth = ui_.Measure( face, summary ).x;
		if ( right - summaryWidth > engineX + ui_.Measure( face, engineText ).x + ui_.S( 24.0f ) ) {
			ui_.Text( draw, face, ImVec2( right - summaryWidth, textY ), palette.textFaint, summary );
		}
	}
}

bool App::MenuItem( const char *id, Icon icon, const std::string &label, bool enabled, bool danger )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const gui::Palette &palette = ui_.palette;
	const float height = ui_.S( 34.0f );
	const float width = std::max( ImGui::GetContentRegionAvail().x, ui_.S( 260.0f ) );
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const bool clicked = ImGui::InvisibleButton( id, ImVec2( width, height ) ) && enabled;
	const bool hovered = ImGui::IsItemHovered() && enabled;
	if ( hovered ) {
		draw->AddRectFilled( min, ImVec2( min.x + width, min.y + height ),
			danger ? WithAlpha( palette.danger, 0.12f ) : palette.ghostActive, ui_.S( 8.0f ) );
		ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
	}
	const ImU32 color = !enabled ? palette.textDisabled : danger ? palette.danger : hovered ? palette.text : palette.textSecondary;
	gui::DrawIcon( draw, icon, ImVec2( min.x + ui_.S( 18.0f ), min.y + height * 0.5f ), ui_.S( 16.0f ), color, ui_.S( 1.6f ) );
	const gui::Typeface face = ui_.Regular( 14.0f );
	ui_.Text( draw, face, ImVec2( min.x + ui_.S( 38.0f ), min.y + ( height - ui_.Measure( face, label ).y ) * 0.5f ),
		!enabled ? palette.textDisabled : danger ? palette.danger : palette.text, label );
	if ( clicked ) ImGui::CloseCurrentPopup();
	return clicked;
}

void App::DrawRowMenu()
{
	if ( openMenu_ ) {
		ImGui::OpenPopup( "##row-menu" );
		ImGui::SetNextWindowPos( menuAnchor_, ImGuiCond_Always, menuPivot_ );
		openMenu_ = false;
	}
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ui_.S( 6.0f, 6.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ui_.S( 0.0f, 1.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_PopupRounding, ui_.S( 12.0f ) );
	if ( ImGui::BeginPopup( "##row-menu" ) ) {
		// Near the window's bottom edge the menu opens upward instead.
		const ImVec2 position = ImGui::GetWindowPos(), extent = ImGui::GetWindowSize();
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		if ( position.y + extent.y > display.y - ui_.S( 8.0f ) && menuFlipY_ > 0.0f ) {
			ImGui::SetWindowPos( ImVec2( position.x, std::max( ui_.S( 8.0f ), menuFlipY_ - extent.y ) ) );
		}
		const Installation *found = FindInstallation( menuId_ );
		if ( !found ) {
			ImGui::CloseCurrentPopup();
		} else {
			const Installation installation = *found;
			ImDrawList *draw = ImGui::GetWindowDrawList();
			const gui::Typeface face = ui_.Medium( 12.5f );
			const ImVec2 at = ImGui::GetCursorScreenPos();
			const std::string heading = ui_.FitEnd( face, DisplayName( installation ), ui_.S( 240.0f ) );
			ui_.Text( draw, face, ImVec2( at.x + ui_.S( 12.0f ), at.y + ui_.S( 6.0f ) ), ui_.palette.textFaint, Upper( heading ) );
			ImGui::Dummy( ImVec2( ui_.S( 260.0f ), ui_.S( 28.0f ) ) );
			const bool canPlay = !installation.missing;
			for ( std::size_t i = 0; i < installation.modes.size(); ++i ) {
				const comGameProfile_t *profile = installation.modes[i];
				const std::string label = installation.modes.size() > 1 ? "Play " + ModeLabel( profile ) : "Play";
				ImGui::PushID( int( i ) );
				if ( MenuItem( "##play", Icon::Play, label, canPlay && ProfileStartsInThisBuild( profile ) ) ) {
					Launch( installation, profile, false );
				}
				ImGui::PopID();
			}
#ifdef _WIN32
			// Windows gives the dedicated server a console window of its own;
			// elsewhere it would run unseen, with nowhere to type commands.
			if ( dedicatedFound_ ) {
				const comGameProfile_t *mode = SelectedMode( installation );
				if ( MenuItem( "##server", Icon::Server, "Start a dedicated server",
						canPlay && mode && ProfileStartsInThisBuild( mode ) ) ) {
					Launch( installation, mode, true );
				}
			}
#endif
			ImGui::Separator();
			if ( MenuItem( "##open", Icon::Folder, "Open the game's folder", canPlay ) ) OpenFolder( installation.root );
			if ( MenuItem( "##copy-path", Icon::Copy, "Copy the folder's path" ) ) {
				SDL_SetClipboardText( installation.rootUtf8.c_str() );
				Notify( "Copied the folder's path.", 0 );
			}
			if ( MenuItem( "##copy-command", Icon::Command, "Copy the launch command", canPlay ) ) {
				stdfs::path engine;
				std::vector<std::string> arguments;
				std::string error;
				if ( BuildLaunch( installation, SelectedMode( installation ), false, engine, arguments, error ) ) {
					SDL_SetClipboardText( DisplayCommand( engine, arguments ).c_str() );
					Notify( "Copied a command that starts this game without the launcher.", 0 );
				} else {
					Notify( error, 2 );
				}
			}
			ImGui::Separator();
			if ( MenuItem( "##edit", Icon::Edit, "Name, mod and options\xe2\x80\xa6" ) ) {
				edit_ = EditState();
					edit_.id = installation.id;
				openEdit_ = true;
			}
			const InstallationPrefs *prefs = FindPrefs( installation.id );
			if ( prefs && prefs->hidden ) {
				if ( MenuItem( "##show", Icon::Show, "Show in the list" ) ) {
					Prefs( installation.id ).hidden = false;
					MarkDirty();
				}
			} else if ( MenuItem( "##hide", Icon::Hide, "Hide from the list" ) ) {
				Prefs( installation.id ).hidden = true;
				MarkDirty();
				Notify( DisplayName( installation ) + " is hidden. Show it again from the bottom of the window.", 0 );
			}
			if ( installation.manual ) {
				if ( MenuItem( "##remove", Icon::Trash, "Remove from the list", true, true ) ) RemoveManual( installation );
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar( 3 );
}

void App::DrawSortMenu()
{
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ui_.S( 6.0f, 6.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ui_.S( 0.0f, 1.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_PopupRounding, ui_.S( 12.0f ) );
	if ( ImGui::BeginPopup( "##sort-menu" ) ) {
		ImDrawList *draw = ImGui::GetWindowDrawList();
		const ImVec2 at = ImGui::GetCursorScreenPos();
		ui_.Text( draw, ui_.Medium( 12.5f ), ImVec2( at.x + ui_.S( 12.0f ), at.y + ui_.S( 6.0f ) ), ui_.palette.textFaint, "SORT BY" );
		ImGui::Dummy( ImVec2( ui_.S( 260.0f ), ui_.S( 28.0f ) ) );
		const char *labels[] = { "Name", "Recently played", "Location" };
		for ( int i = 0; i < 3; ++i ) {
			ImGui::PushID( i );
			const bool current = int( settings_.sort ) == i;
			if ( MenuItem( "##order", current ? Icon::Check : Icon::Sort, labels[i] ) ) {
				settings_.sort = SortOrder( i );
				MarkDirty();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		if ( MenuItem( "##unavailable", settings_.showUnavailable ? Icon::Check : Icon::Hide,
				"Show games this build can't start" ) ) {
			settings_.showUnavailable = !settings_.showUnavailable;
			MarkDirty();
		}
		if ( MenuItem( "##hidden", settings_.showHidden ? Icon::Check : Icon::Hide, "Show hidden games" ) ) {
			settings_.showHidden = !settings_.showHidden;
			MarkDirty();
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar( 3 );
}

bool App::BeginComboField( const char *id, const std::string &preview, float width )
{
	// ImGui's arrow box replaced by a chevron inside the field.
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const float height = ImGui::GetFrameHeight();
	ImGui::SetNextItemWidth( width );
	const bool open = ImGui::BeginCombo( id, preview.c_str(), ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge );
	gui::DrawIcon( draw, open ? Icon::ChevronRight : Icon::ChevronDown,
		ImVec2( min.x + width - ui_.S( 20.0f ), min.y + height * 0.5f ), ui_.S( 14.0f ), ui_.palette.textSecondary, ui_.S( 1.6f ) );
	return open;
}

void App::DrawEditDialog()
{
	const Installation *found = FindInstallation( edit_.id );
	if ( openEdit_ ) {
		openEdit_ = false;
		if ( found ) {
			const InstallationPrefs *prefs = FindPrefs( found->id );
			std::snprintf( edit_.name, sizeof( edit_.name ), "%s", prefs ? prefs->name.c_str() : "" );
			std::snprintf( edit_.arguments, sizeof( edit_.arguments ), "%s", prefs ? prefs->arguments.c_str() : "" );
			edit_.mod = prefs ? prefs->mod : "";
			const comGameProfile_t *mode = SelectedMode( *found );
			edit_.mode = 0;
			for ( std::size_t i = 0; i < found->modes.size(); ++i ) {
				if ( found->modes[i] == mode ) edit_.mode = int( i );
			}
			if ( edit_.mods.empty() ) edit_.mods = FindMods( *found, mode );
			ImGui::OpenPopup( "##edit" );
		}
	}
	const gui::Palette &palette = ui_.palette;
	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos( center, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
	ImGui::SetNextWindowSize( ImVec2( std::min( ui_.S( 580.0f ), ImGui::GetIO().DisplaySize.x - ui_.S( 32.0f ) ), 0.0f ) );
	if ( !ImGui::BeginPopupModal( "##edit", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings ) ) {
		return;
	}
	if ( !found ) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	const Installation installation = *found;
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const float width = ImGui::GetContentRegionAvail().x;
	// Heading: icon, name and where it lives.
	{
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const float iconSize = ui_.S( 44.0f );
		const comGameProfile_t *mode = installation.modes.empty() ? nullptr
			: installation.modes[std::size_t( std::clamp( edit_.mode, 0, int( installation.modes.size() ) - 1 ) )];
		if ( SDL_Texture *texture = IconTexture( mode, iconSize * ImGui::GetIO().DisplayFramebufferScale.x ) ) {
			draw->AddImageRounded( gui::TextureId( texture ), at, ImVec2( at.x + iconSize, at.y + iconSize ),
				ImVec2( 0.0f, 0.0f ), ImVec2( 1.0f, 1.0f ), IM_COL32( 255, 255, 255, 255 ), ui_.S( 10.0f ) );
		}
		const float textX = at.x + iconSize + ui_.S( 14.0f );
		ui_.Text( draw, ui_.Bold( 18.0f ), ImVec2( textX, at.y + ui_.S( 1.0f ) ), palette.text,
			ui_.FitEnd( ui_.Bold( 18.0f ), GroupName( installation.group ), width - iconSize - ui_.S( 14.0f ) ) );
		const std::string where = std::string( StoreName( installation.store ) ) + "  \xc2\xb7  " + installation.rootUtf8;
		ui_.Text( draw, ui_.Regular( 13.0f ), ImVec2( textX, at.y + ui_.S( 26.0f ) ), palette.textFaint,
			ui_.FitPath( ui_.Regular( 13.0f ), where, width - iconSize - ui_.S( 14.0f ) ) );
		ImGui::Dummy( ImVec2( width, iconSize + ui_.S( 12.0f ) ) );
	}
	auto label = [&]( const char *text ) {
		ImGui::Dummy( ImVec2( 0.0f, ui_.S( 4.0f ) ) );
		const ImVec2 at = ImGui::GetCursorScreenPos();
		ui_.Text( draw, ui_.Medium( 13.0f ), at, palette.textSecondary, text );
		ImGui::Dummy( ImVec2( 0.0f, ui_.Measure( ui_.Medium( 13.0f ), text ).y + ui_.S( 2.0f ) ) );
	};
	ImGui::PushFont( fonts_->regular, 14.0f );
	label( "Name in the list" );
	ImGui::SetNextItemWidth( width );
	ImGui::InputTextWithHint( "##name", GroupName( installation.group ).c_str(), edit_.name, sizeof( edit_.name ) );
	if ( installation.modes.size() > 1 ) {
		label( "Game mode" );
		std::vector<std::string> labels;
		std::vector<bool> enabled;
		std::vector<std::string> tips;
		for ( const comGameProfile_t *profile : installation.modes ) {
			labels.push_back( ModeLabel( profile ) );
			tips.push_back( ProfileUnavailableReason( profile ) );
			enabled.push_back( tips.back().empty() );
		}
		ui_.Segmented( "##edit-mode", labels, edit_.mode, ui_.S( 34.0f ), &enabled, &tips );
	}
	label( "Mod" );
	const std::string current = edit_.mod.empty() ? "None: play the game itself" : ModDisplayName( edit_.mod )
		+ ( ModDisplayName( edit_.mod ) != edit_.mod ? "  (" + edit_.mod + ")" : "" );
	if ( BeginComboField( "##mod", current, width ) ) {
		if ( ImGui::Selectable( "None: play the game itself", edit_.mod.empty() ) ) edit_.mod.clear();
		for ( const std::string &mod : edit_.mods ) {
			const std::string text = ModDisplayName( mod ) + ( ModDisplayName( mod ) != mod ? "  (" + mod + ")" : "" );
			if ( ImGui::Selectable( text.c_str(), edit_.mod == mod ) ) edit_.mod = mod;
		}
		ImGui::EndCombo();
	}
	if ( edit_.mods.empty() ) {
		ImGui::PushStyleColor( ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4( palette.textFaint ) );
		ImGui::TextUnformatted( "No mod folders were found beside this game's data." );
		ImGui::PopStyleColor();
	}
	label( "Extra command line" );
	ImGui::SetNextItemWidth( width );
	ImGui::InputTextWithHint( "##arguments", "For example: +set r_mode -1 +connect example.com", edit_.arguments,
		sizeof( edit_.arguments ) );

	// What will run, checked the way the launch itself checks it.
	LaunchRequest request;
	request.engine = enginePath_;
	request.profile = installation.modes.empty() ? nullptr
		: installation.modes[std::size_t( std::clamp( edit_.mode, 0, int( installation.modes.size() ) - 1 ) )];
	request.basePath = installation.rootUtf8;
	request.mod = edit_.mod;
	request.arguments = edit_.arguments;
	std::vector<std::string> arguments;
	std::string error;
	const bool valid = BuildArguments( request, arguments, &error );
	label( "Launch command" );
	{
		const std::string command = valid ? DisplayCommand( enginePath_, arguments ) : error;
		const gui::Typeface face = ui_.Regular( 12.5f );
		const float boxWidth = width;
		const float innerWidth = boxWidth - ui_.S( 52.0f );
		const std::vector<std::string> lines = WrapAnywhere( command, ui_, face, innerWidth );
		const float lineHeight = ui_.Measure( face, "Ag" ).y + ui_.S( 2.0f );
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const float boxHeight = std::max( float( lines.size() ) * lineHeight + ui_.S( 20.0f ), ui_.S( 44.0f ) );
		draw->AddRectFilled( at, ImVec2( at.x + boxWidth, at.y + boxHeight ), palette.field, ui_.S( 9.0f ) );
		draw->AddRect( at, ImVec2( at.x + boxWidth, at.y + boxHeight ), valid ? palette.fieldBorder : WithAlpha( palette.danger, 0.6f ),
			ui_.S( 9.0f ), ui_.S( 1.0f ) );
		float y = at.y + ui_.S( 10.0f );
		for ( const std::string &text : lines ) {
			ui_.Text( draw, face, ImVec2( at.x + ui_.S( 12.0f ), y ), valid ? palette.textSecondary : palette.danger, text );
			y += lineHeight;
		}
		if ( valid ) {
			ImGui::SetCursorScreenPos( ImVec2( at.x + boxWidth - ui_.S( 38.0f ), at.y + ui_.S( 6.0f ) ) );
			if ( ui_.IconButton( "##copy-command", Icon::Copy, ui_.S( 32.0f ), "Copy the command" ) ) {
				SDL_SetClipboardText( command.c_str() );
				Notify( "Copied the launch command.", 0 );
			}
		}
		ImGui::SetCursorScreenPos( ImVec2( at.x, at.y + boxHeight ) );
		ImGui::Dummy( ImVec2( boxWidth, ui_.S( 2.0f ) ) );
	}
	if ( !installation.verified ) {
		ImGui::Dummy( ImVec2( 0.0f, ui_.S( 4.0f ) ) );
		const ImVec2 at = ImGui::GetCursorScreenPos();
		gui::DrawIcon( draw, Icon::Warning, ImVec2( at.x + ui_.S( 8.0f ), at.y + ui_.S( 9.0f ) ), ui_.S( 15.0f ), palette.warning, ui_.S( 1.5f ) );
		ui_.Text( draw, ui_.Regular( 13.0f ), ImVec2( at.x + ui_.S( 24.0f ), at.y ), palette.warning,
			"fnTech3 could not confirm this folder holds the game's retail data." );
		ImGui::Dummy( ImVec2( 0.0f, ui_.S( 20.0f ) ) );
	}
	ImGui::PopFont();

	// Buttons.
	ImGui::Dummy( ImVec2( 0.0f, ui_.S( 10.0f ) ) );
	const float height = ui_.S( 38.0f );
	const ImVec2 row = ImGui::GetCursorScreenPos();
	if ( installation.manual ) {
		const float removeWidth = ui_.ButtonWidth( "Remove", height, true );
		if ( ui_.Button( "##edit-remove", "Remove", ButtonKind::Danger, height, removeWidth, true, Icon::Trash, true ) ) {
			RemoveManual( installation );
			ImGui::CloseCurrentPopup();
		}
	}
	const float saveWidth = std::max( ui_.ButtonWidth( "Save", height, false ), ui_.S( 96.0f ) );
	const float cancelWidth = std::max( ui_.ButtonWidth( "Cancel", height, false ), ui_.S( 96.0f ) );
	ImGui::SetCursorScreenPos( ImVec2( row.x + width - saveWidth - cancelWidth - ui_.S( 10.0f ), row.y ) );
	if ( ui_.Button( "##edit-cancel", "Cancel", ButtonKind::Secondary, height, cancelWidth )
		|| ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::SetCursorScreenPos( ImVec2( row.x + width - saveWidth, row.y ) );
	if ( ui_.Button( "##edit-save", "Save", ButtonKind::Primary, height, saveWidth, valid ) ) {
		InstallationPrefs &prefs = Prefs( installation.id );
		prefs.name = edit_.name;
		while ( !prefs.name.empty() && prefs.name.back() == ' ' ) prefs.name.pop_back();
		if ( prefs.name == GroupName( installation.group ) ) prefs.name.clear();
		prefs.mod = edit_.mod;
		prefs.arguments = edit_.arguments;
		if ( installation.modes.size() > 1 && request.profile ) prefs.mode = request.profile->key;
		MarkDirty();
		fontsChecked_ = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void App::DrawSettingsDialog()
{
	if ( openSettings_ ) {
		openSettings_ = false;
		ImGui::OpenPopup( "##settings" );
	}
	const gui::Palette &palette = ui_.palette;
	ImGui::SetNextWindowPos( ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
	ImGui::SetNextWindowSize( ImVec2( std::min( ui_.S( 600.0f ), ImGui::GetIO().DisplaySize.x - ui_.S( 32.0f ) ), 0.0f ) );
	ImGui::SetNextWindowSizeConstraints( ImVec2( 0.0f, 0.0f ), ImVec2( FLT_MAX, ImGui::GetIO().DisplaySize.y - ui_.S( 40.0f ) ) );
	if ( !ImGui::BeginPopupModal( "##settings", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings ) ) {
		return;
	}
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const float width = ImGui::GetContentRegionAvail().x;
	{
		const ImVec2 at = ImGui::GetCursorScreenPos();
		ui_.Text( draw, ui_.Bold( 19.0f ), at, palette.text, "Settings" );
		ImGui::Dummy( ImVec2( width, ui_.S( 30.0f ) ) );
	}
	auto heading = [&]( const char *text ) {
		ImGui::Dummy( ImVec2( 0.0f, ui_.S( 4.0f ) ) );
		const ImVec2 at = ImGui::GetCursorScreenPos();
		ui_.Text( draw, ui_.Medium( 11.5f ), at, palette.textFaint, Upper( text ) );
		const float textWidth = ui_.Measure( ui_.Medium( 11.5f ), Upper( text ) ).x;
		draw->AddLine( ImVec2( at.x + textWidth + ui_.S( 10.0f ), at.y + ui_.S( 8.0f ) ), ImVec2( at.x + width, at.y + ui_.S( 8.0f ) ),
			palette.separator, ui_.S( 1.0f ) );
		ImGui::Dummy( ImVec2( 0.0f, ui_.S( 16.0f ) ) );
	};
	auto caption = [&]( const std::string &text, float indent = 0.0f ) {
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const gui::Typeface face = ui_.Regular( 13.5f );
		float y = at.y;
		for ( const std::string &line : Words( text, ui_, face, width - indent ) ) {
			ui_.Text( draw, face, ImVec2( at.x + indent, y ), palette.textSecondary, line );
			y += ui_.Measure( face, line ).y + ui_.S( 2.0f );
		}
		ImGui::Dummy( ImVec2( width, y - at.y + ui_.S( 6.0f ) ) );
	};

	heading( "Appearance" );
	int theme = int( settings_.theme );
	if ( ui_.Segmented( "##theme", { "Match the system", "Dark", "Light" }, theme, ui_.S( 34.0f ) ) ) {
		settings_.theme = Theme( theme );
		ResolveTheme();
		MarkDirty();
	}

	heading( "When a game starts" );
	int after = int( settings_.afterLaunch );
	if ( ui_.Segmented( "##after", { "Keep the launcher open", "Minimize it", "Close it" }, after, ui_.S( 34.0f ) ) ) {
		settings_.afterLaunch = AfterLaunch( after );
		MarkDirty();
	}

	heading( "Game list" );
	auto toggleRow = [&]( const char *id, const char *text, bool &value ) {
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const gui::Typeface face = ui_.Regular( 14.0f );
		ui_.Text( draw, face, ImVec2( at.x, at.y + ( ui_.S( 22.0f ) - ui_.Measure( face, text ).y ) * 0.5f ), palette.text, text );
		ImGui::SetCursorScreenPos( ImVec2( at.x + width - ui_.S( 38.0f ), at.y ) );
		if ( ui_.Toggle( id, value ) ) MarkDirty();
		ImGui::SetCursorScreenPos( ImVec2( at.x, at.y + ui_.S( 32.0f ) ) );
	};
	toggleRow( "##show-unavailable", "Show games this build can't start yet", settings_.showUnavailable );
	toggleRow( "##show-hidden", "Show games you have hidden", settings_.showHidden );

	heading( "Also search these folders" );
	caption( "fnTech3 always looks in Steam, GOG, Heroic and Lutris and in the usual game folders. Add a folder "
		"that holds several games, such as D:\\Games, to have it searched too." );
	std::size_t removeIndex = settings_.searchFolders.size();
	for ( std::size_t i = 0; i < settings_.searchFolders.size(); ++i ) {
		ImGui::PushID( int( i ) );
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const float rowHeight = ui_.S( 36.0f );
		draw->AddRectFilled( at, ImVec2( at.x + width, at.y + rowHeight ), palette.field, ui_.S( 8.0f ) );
		gui::DrawIcon( draw, Icon::Folder, ImVec2( at.x + ui_.S( 18.0f ), at.y + rowHeight * 0.5f ), ui_.S( 16.0f ),
			palette.textSecondary, ui_.S( 1.5f ) );
		const gui::Typeface face = ui_.Regular( 13.5f );
		const std::string text = ui_.FitPath( face, settings_.searchFolders[i], width - ui_.S( 84.0f ) );
		ui_.Text( draw, face, ImVec2( at.x + ui_.S( 36.0f ), at.y + ( rowHeight - ui_.Measure( face, text ).y ) * 0.5f ),
			palette.text, text );
		ImGui::SetCursorScreenPos( ImVec2( at.x + width - ui_.S( 34.0f ), at.y + ui_.S( 3.0f ) ) );
		if ( ui_.IconButton( "##remove-folder", Icon::Close, ui_.S( 30.0f ), "Stop searching this folder" ) ) removeIndex = i;
		ImGui::SetCursorScreenPos( ImVec2( at.x, at.y + rowHeight + ui_.S( 6.0f ) ) );
		ImGui::PopID();
	}
	if ( removeIndex < settings_.searchFolders.size() ) {
		settings_.searchFolders.erase( settings_.searchFolders.begin() + std::ptrdiff_t( removeIndex ) );
		MarkDirty();
		StartScan();
	}
	{
		const float height = ui_.S( 36.0f );
		if ( ui_.Button( "##add-search", "Add a folder to search", ButtonKind::Secondary, height, 0.0f, true, Icon::Plus, true ) ) {
			ShowSearchFolderPicker();
		}
	}

	heading( "Engine" );
	{
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const float rowHeight = ui_.S( 40.0f );
		draw->AddRectFilled( at, ImVec2( at.x + width, at.y + rowHeight ), palette.field, ui_.S( 8.0f ) );
		gui::DrawIcon( draw, engineFound_ ? Icon::Check : Icon::Warning, ImVec2( at.x + ui_.S( 18.0f ), at.y + rowHeight * 0.5f ),
			ui_.S( 15.0f ), engineFound_ ? palette.success : palette.warning, ui_.S( 1.7f ) );
		const gui::Typeface face = ui_.Regular( 13.5f );
		const std::string text = ui_.FitPath( face, fs::ToUtf8( enginePath_ ), width - ui_.S( 52.0f ) );
		ui_.Text( draw, face, ImVec2( at.x + ui_.S( 36.0f ), at.y + ( rowHeight - ui_.Measure( face, text ).y ) * 0.5f ),
			engineFound_ ? palette.text : palette.warning, text );
		ImGui::Dummy( ImVec2( width, rowHeight + ui_.S( 6.0f ) ) );
	}
	caption( settings_.enginePath.empty()
		? "The launcher starts the fnTech3 that sits beside it. Choose another only to try a different build."
		: "You chose this engine. The dedicated server is expected in the same folder." );
	{
		const float height = ui_.S( 36.0f );
		if ( ui_.Button( "##choose-engine", "Choose\xe2\x80\xa6", ButtonKind::Secondary, height, 0.0f, true, Icon::Folder, true ) ) {
			ShowEnginePicker();
		}
		if ( !settings_.enginePath.empty() ) {
			ImGui::SameLine( 0.0f, ui_.S( 8.0f ) );
			if ( ui_.Button( "##default-engine", "Use the one beside the launcher", ButtonKind::Ghost, height ) ) {
				settings_.enginePath.clear();
				ResolveEngine();
				MarkDirty();
			}
		}
	}

	// About and close.
	ImGui::Dummy( ImVec2( 0.0f, ui_.S( 10.0f ) ) );
	const ImVec2 at = ImGui::GetCursorScreenPos();
	const float height = ui_.S( 38.0f );
	const float doneWidth = std::max( ui_.ButtonWidth( "Done", height, false ), ui_.S( 104.0f ) );
	const gui::Typeface face = ui_.Regular( 12.5f );
	const std::string where = "fnTech3 " FNTECH3_VERSION_STRING "  \xc2\xb7  settings in " + fs::ToUtf8( options_.settingsFile );
	const std::string fitted = ui_.FitPath( face, where, width - doneWidth - ui_.S( 12.0f ) );
	ui_.Text( draw, face, ImVec2( at.x, at.y + ( height - ui_.Measure( face, fitted ).y ) * 0.5f ), palette.textFaint, fitted );
	ImGui::SetCursorScreenPos( at );
	ImGui::InvisibleButton( "##settings-file", ImVec2( std::max( ui_.Measure( face, fitted ).x, 1.0f ), height ) );
	if ( ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) {
		ui_.Tooltip( "Settings file: " + fs::ToUtf8( options_.settingsFile ) + "\nInterface font: "
			+ ( fonts_ ? fonts_->description : std::string() ), 520.0f );
	}
	ImGui::SetCursorScreenPos( ImVec2( at.x + width - doneWidth, at.y ) );
	if ( ui_.Button( "##settings-done", "Done", ButtonKind::Primary, height, doneWidth ) || ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void App::DrawAddDialog()
{
	if ( openAdd_ ) {
		openAdd_ = false;
		ImGui::OpenPopup( "##add" );
	}
	const gui::Palette &palette = ui_.palette;
	ImGui::SetNextWindowPos( ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
	ImGui::SetNextWindowSize( ImVec2( std::min( ui_.S( 540.0f ), ImGui::GetIO().DisplaySize.x - ui_.S( 32.0f ) ), 0.0f ) );
	if ( !ImGui::BeginPopupModal( "##add", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings ) ) {
		return;
	}
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const float width = ImGui::GetContentRegionAvail().x;
	{
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const float badge = ui_.S( 40.0f );
		draw->AddCircleFilled( ImVec2( at.x + badge * 0.5f, at.y + badge * 0.5f ), badge * 0.5f, WithAlpha( palette.warning, 0.14f ), 32 );
		gui::DrawIcon( draw, Icon::Warning, ImVec2( at.x + badge * 0.5f, at.y + badge * 0.5f ), badge * 0.5f, palette.warning, ui_.S( 1.8f ) );
		const float textX = at.x + badge + ui_.S( 14.0f );
		ui_.Text( draw, ui_.Bold( 17.5f ), ImVec2( textX, at.y + ui_.S( 1.0f ) ), palette.text,
			ui_.FitEnd( ui_.Bold( 17.5f ), "fnTech3 didn't find a game it knows here", width - badge - ui_.S( 14.0f ) ) );
		ui_.Text( draw, ui_.Regular( 13.0f ), ImVec2( textX, at.y + ui_.S( 25.0f ) ), palette.textFaint,
			ui_.FitPath( ui_.Regular( 13.0f ), add_.path, width - badge - ui_.S( 14.0f ) ) );
		ImGui::Dummy( ImVec2( width, badge + ui_.S( 12.0f ) ) );
	}
	{
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const gui::Typeface face = ui_.Regular( 14.0f );
		float y = at.y;
		const std::string body = "Choose the game's own folder: the one that holds its data folder, such as baseq3 for "
			"Quake III Arena, main for Return to Castle Wolfenstein or base for Jedi Academy. If you are sure this is "
			"the game, you can add it anyway; it may not start.";
		for ( const std::string &line : Words( body, ui_, face, width ) ) {
			ui_.Text( draw, face, ImVec2( at.x, y ), palette.textSecondary, line );
			y += ui_.Measure( face, line ).y + ui_.S( 3.0f );
		}
		ImGui::Dummy( ImVec2( width, y - at.y + ui_.S( 8.0f ) ) );
	}
	const std::vector<std::string> groups = AllGroups();
	add_.group = std::clamp( add_.group, 0, int( groups.size() ) - 1 );
	ImGui::PushFont( fonts_->regular, 14.0f );
	{
		const ImVec2 at = ImGui::GetCursorScreenPos();
		ui_.Text( draw, ui_.Medium( 13.0f ), at, palette.textSecondary, "Add it as" );
		ImGui::Dummy( ImVec2( 0.0f, ui_.S( 20.0f ) ) );
	}
	if ( BeginComboField( "##add-group", GroupName( groups[std::size_t( add_.group )] ), width ) ) {
		for ( std::size_t i = 0; i < groups.size(); ++i ) {
			if ( ImGui::Selectable( GroupName( groups[i] ).c_str(), int( i ) == add_.group ) ) add_.group = int( i );
		}
		ImGui::EndCombo();
	}
	ImGui::PopFont();
	ImGui::Dummy( ImVec2( 0.0f, ui_.S( 14.0f ) ) );
	const float height = ui_.S( 38.0f );
	const ImVec2 row = ImGui::GetCursorScreenPos();
	const float chooseWidth = ui_.ButtonWidth( "Choose another folder", height, true );
	if ( ui_.Button( "##add-choose", "Choose another folder", ButtonKind::Ghost, height, chooseWidth, true, Icon::Folder, true ) ) {
		ImGui::CloseCurrentPopup();
		ShowFolderPicker();
	}
	const float addWidth = ui_.ButtonWidth( "Add anyway", height, false );
	const float cancelWidth = std::max( ui_.ButtonWidth( "Cancel", height, false ), ui_.S( 96.0f ) );
	ImGui::SetCursorScreenPos( ImVec2( row.x + width - addWidth - cancelWidth - ui_.S( 10.0f ), row.y ) );
	if ( ui_.Button( "##add-cancel", "Cancel", ButtonKind::Secondary, height, cancelWidth ) || ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::SetCursorScreenPos( ImVec2( row.x + width - addWidth, row.y ) );
	if ( ui_.Button( "##add-anyway", "Add anyway", ButtonKind::Primary, height, addWidth ) ) {
		AddUnverified( add_.path, groups[std::size_t( add_.group )] );
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void App::DrawToasts( ImVec2 size )
{
	if ( toasts_.empty() ) return;
	const gui::Palette &palette = ui_.palette;
	const gui::Typeface face = ui_.Regular( 14.0f );
	const float width = std::min( ui_.S( 380.0f ), size.x - ui_.S( 48.0f ) );
	const float textWidth = width - ui_.S( 62.0f );
	const float lineHeight = ui_.Measure( face, "Ag" ).y + ui_.S( 2.0f );
	struct Layout {
		std::vector<std::string> lines;
		float height;
	};
	std::vector<Layout> layouts;
	float total = 0.0f;
	for ( const Toast &toast : toasts_ ) {
		Layout layout;
		layout.lines = Words( toast.text, ui_, face, textWidth );
		layout.height = float( layout.lines.size() ) * lineHeight + ui_.S( 26.0f );
		total += layout.height + ui_.S( 10.0f );
		layouts.push_back( layout );
	}
	total -= ui_.S( 10.0f );
	const ImVec2 origin( size.x - width - ui_.S( 24.0f ), size.y - ui_.S( kFooterHeight ) - ui_.S( 16.0f ) - total );
	// An invisible window over the stack takes its clicks, so a click that
	// dismisses a notification never reaches a Play button underneath.
	ImGui::SetNextWindowPos( origin );
	ImGui::SetNextWindowSize( ImVec2( width, total ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
	ImGui::Begin( "##toasts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
		| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	ImGui::PopStyleVar( 2 );
	const bool hovered = ImGui::IsWindowHovered();
	ImGui::End();
	ImDrawList *draw = ImGui::GetForegroundDrawList();
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	float y = origin.y;
	for ( std::size_t i = 0; i < toasts_.size(); ++i ) {
		Toast &toast = toasts_[i];
		const Layout &layout = layouts[i];
		const float appear = std::min( toast.age / 0.22f, 1.0f );
		const float vanish = std::min( ( toast.life - toast.age ) / 0.4f, 1.0f );
		const float alpha = std::clamp( std::min( appear, vanish ), 0.0f, 1.0f );
		const float ease = 1.0f - ( 1.0f - appear ) * ( 1.0f - appear );
		const float slide = ( 1.0f - ease ) * ui_.S( 16.0f );
		const ImVec2 min( origin.x + slide, y ), max( origin.x + width + slide, y + layout.height );
		const bool over = hovered && mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y;
		if ( over ) {
			// Hovering holds a notification; a click dismisses it.
			toast.age = std::min( toast.age, toast.life - 1.0f );
			if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) toast.age = toast.life - 0.4f;
		}
		ui_.Shadow( draw, min, max, ui_.S( 12.0f ), ui_.S( 14.0f ), WithAlpha( palette.shadow, alpha ) );
		draw->AddRectFilled( min, max, WithAlpha( palette.popup, alpha ), ui_.S( 12.0f ) );
		draw->AddRect( min, max, WithAlpha( over ? palette.cardBorderHover : palette.popupBorder, alpha ), ui_.S( 12.0f ), ui_.S( 1.0f ) );
		const ImU32 tint = toast.kind == 2 ? palette.danger : toast.kind == 1 ? palette.success : palette.info;
		const ImVec2 iconCenter( min.x + ui_.S( 27.0f ), min.y + ui_.S( 13.0f ) + lineHeight * 0.5f );
		draw->AddCircleFilled( iconCenter, ui_.S( 13.0f ), WithAlpha( tint, 0.16f * alpha ), 24 );
		gui::DrawIcon( draw, toast.kind == 2 ? Icon::Warning : toast.kind == 1 ? Icon::Check : Icon::Info, iconCenter,
			ui_.S( 14.0f ), WithAlpha( tint, alpha ), ui_.S( 1.7f ) );
		float lineY = min.y + ui_.S( 13.0f );
		for ( const std::string &line : layout.lines ) {
			ui_.Text( draw, face, ImVec2( min.x + ui_.S( 50.0f ), lineY ), WithAlpha( palette.text, alpha ), line );
			lineY += lineHeight;
		}
		y += layout.height + ui_.S( 10.0f );
	}
	ui_.animating = true;
}

void App::DrawDropOverlay( ImVec2 size )
{
	if ( !dropActive_ ) return;
	const gui::Palette &palette = ui_.palette;
	ImDrawList *draw = ImGui::GetForegroundDrawList();
	draw->AddRectFilled( ImVec2( 0.0f, 0.0f ), size, WithAlpha( palette.background | IM_COL32_A_MASK, 0.86f ) );
	const float inset = ui_.S( 22.0f );
	const ImVec2 min( inset, inset ), max( size.x - inset, size.y - inset );
	draw->AddRectFilled( min, max, WithAlpha( palette.accent, 0.06f ), ui_.S( 18.0f ) );
	// A dashed outline.
	const float dash = ui_.S( 12.0f ), space = ui_.S( 8.0f );
	const float offset = std::fmod( time_ * ui_.S( 24.0f ), dash + space );
	auto dashed = [&]( ImVec2 from, ImVec2 to ) {
		const float length = std::hypot( to.x - from.x, to.y - from.y );
		const ImVec2 direction( ( to.x - from.x ) / length, ( to.y - from.y ) / length );
		for ( float t = -offset; t < length; t += dash + space ) {
			const float start = std::max( t, 0.0f ), end = std::min( t + dash, length );
			if ( end <= start ) continue;
			draw->AddLine( ImVec2( from.x + direction.x * start, from.y + direction.y * start ),
				ImVec2( from.x + direction.x * end, from.y + direction.y * end ), palette.accent, ui_.S( 2.0f ) );
		}
	};
	const float r = ui_.S( 18.0f );
	dashed( ImVec2( min.x + r, min.y ), ImVec2( max.x - r, min.y ) );
	dashed( ImVec2( max.x, min.y + r ), ImVec2( max.x, max.y - r ) );
	dashed( ImVec2( max.x - r, max.y ), ImVec2( min.x + r, max.y ) );
	dashed( ImVec2( min.x, max.y - r ), ImVec2( min.x, min.y + r ) );
	const float quarter = 3.14159265f * 0.5f;
	const ImVec2 corners[] = { ImVec2( min.x + r, min.y + r ), ImVec2( max.x - r, min.y + r ), ImVec2( max.x - r, max.y - r ),
		ImVec2( min.x + r, max.y - r ) };
	for ( int i = 0; i < 4; ++i ) {
		draw->PathArcTo( corners[i], r, quarter * float( i + 2 ), quarter * float( i + 3 ), 10 );
		draw->PathStroke( palette.accent, ui_.S( 2.0f ) );
	}
	const ImVec2 center( size.x * 0.5f, size.y * 0.5f - ui_.S( 20.0f ) );
	draw->AddCircleFilled( center, ui_.S( 44.0f ), WithAlpha( palette.accent, 0.14f ), 48 );
	gui::DrawIcon( draw, Icon::Folder, center, ui_.S( 44.0f ), palette.accent, ui_.S( 2.4f ) );
	const gui::Typeface face = ui_.Bold( 19.0f );
	const std::string text = "Drop a game folder to add it";
	const ImVec2 textSize = ui_.Measure( face, text );
	ui_.Text( draw, face, ImVec2( center.x - textSize.x * 0.5f, center.y + ui_.S( 60.0f ) ), palette.text, text );
	ui_.animating = true;
}

void App::HandleKeyboard( const View &view )
{
	if ( ImGui::IsPopupOpen( nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) ) return;
	ImGuiIO &io = ImGui::GetIO();
	if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F ) ) focusSearch_ = true;
	if ( !io.WantTextInput && ImGui::IsKeyPressed( ImGuiKey_Slash, false ) ) focusSearch_ = true;
	if ( ImGui::IsKeyPressed( ImGuiKey_F5, false ) || ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_R ) ) StartScan();
	if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_O ) ) ShowFolderPicker();
	if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_Comma ) ) openSettings_ = true;
	if ( ImGui::IsKeyPressed( ImGuiKey_Escape, false ) && search_[0] ) search_[0] = '\0';
	if ( view.order.empty() ) return;
	int index = -1;
	for ( std::size_t i = 0; i < view.order.size(); ++i ) {
		if ( view.order[i]->id == selectedId_ ) index = int( i );
	}
	if ( index < 0 ) {
		// The search hid the selection: Enter and the arrows start from the
		// first game still shown, never from one the player cannot see.
		selectedId_ = view.order.front()->id;
		index = 0;
	}
	const int last = int( view.order.size() ) - 1;
	int next = index;
	if ( ImGui::IsKeyPressed( ImGuiKey_DownArrow, true ) ) next = index < 0 ? 0 : std::min( index + 1, last );
	if ( ImGui::IsKeyPressed( ImGuiKey_UpArrow, true ) ) next = index < 0 ? 0 : std::max( index - 1, 0 );
	if ( ImGui::IsKeyPressed( ImGuiKey_PageDown, true ) ) next = std::min( std::max( index, 0 ) + 5, last );
	if ( ImGui::IsKeyPressed( ImGuiKey_PageUp, true ) ) next = std::max( index - 5, 0 );
	if ( !io.WantTextInput && ImGui::IsKeyPressed( ImGuiKey_Home, false ) ) next = 0;
	if ( !io.WantTextInput && ImGui::IsKeyPressed( ImGuiKey_End, false ) ) next = last;
	if ( next != index && next >= 0 ) {
		selectedId_ = view.order[std::size_t( next )]->id;
		ensureVisible_ = selectedId_;
	}
	const Installation *selected = FindInstallation( selectedId_ );
	if ( !selected ) return;
	if ( ImGui::IsKeyPressed( ImGuiKey_Enter, false ) || ImGui::IsKeyPressed( ImGuiKey_KeypadEnter, false ) ) {
		if ( Playable( *selected ) && !IsRunning( selected->id ) ) Launch( *selected, SelectedMode( *selected ), false );
	}
	if ( ImGui::IsKeyPressed( ImGuiKey_Menu, false ) || ImGui::IsKeyChordPressed( ImGuiMod_Shift | ImGuiKey_F10 ) ) {
		menuId_ = selectedId_;
		const auto anchor = rowMenuAnchors_.find( selectedId_ );
		if ( anchor != rowMenuAnchors_.end() ) {
			menuAnchor_ = ImVec2( anchor->second.x, anchor->second.y );
			menuFlipY_ = anchor->second.w;
			menuPivot_ = ImVec2( 1.0f, 0.0f );
		} else {
			menuAnchor_ = ImGui::GetMainViewport()->GetCenter();
			menuFlipY_ = -1.0f;
			menuPivot_ = ImVec2( 0.5f, 0.5f );
		}
		openMenu_ = true;
	}
}

void App::EnsureVisible( const std::string &id )
{
	ensureVisible_ = id;
}

} // namespace fntech3::launcher
