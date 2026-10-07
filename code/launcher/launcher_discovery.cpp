/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_discovery.hpp"

#include "launcher_fs.hpp"
#include "launcher_json.hpp"
#include "launcher_vdf.hpp"

#include <algorithm>
#include <chrono>
#include <cwchar>
#include <set>
#include <system_error>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fntech3::launcher {

namespace stdfs = std::filesystem;

namespace {

// Folders whose children are games rather than games themselves: publisher
// and launcher folders under Program Files, a Games folder, GOG's own.
const char *const kContainerFolders[] = {
	"games", "gog games", "gog galaxy", "gog.com",
	"activision", "activision value", "aspyr", "aspyr media", "atari",
	"bethesda softworks", "disney interactive", "ea games", "electronic arts",
	"gathering of developers", "god games", "gray matter", "id", "id software",
	"infogrames", "lucasarts", "lucasarts entertainment company",
	"lucasarts entertainment company llc", "origin games", "raven",
	"raven software", "ritual entertainment", "rogue entertainment",
	"splash damage", "2015", "2015 games"
};

bool IsContainerFolder( const stdfs::path &folder )
{
	const std::string name = fs::Lower( fs::ToUtf8( folder.filename() ) );
	for ( const char *container : kContainerFolders ) {
		if ( name == container ) return true;
	}
	return false;
}

class CandidateList {
public:
	void Add( const stdfs::path &path, Store store, const std::string &detail,
		const std::string &title = {}, const std::string &id = {} )
	{
		if ( path.empty() ) return;
		Candidate candidate;
		candidate.path = path;
		candidate.store = store;
		candidate.detail = detail;
		candidate.storeTitle = title;
		candidate.storeId = id;
		items_.push_back( std::move( candidate ) );
	}

	// Each child of a container; publisher and launcher folders below it are
	// opened up to `depth` more levels.
	void AddChildren( const stdfs::path &container, Store store, const std::string &detail, int depth = 2 )
	{
		if ( !fs::IsDirectory( container ) ) return;
		for ( const stdfs::path &child : fs::Subdirectories( container, 2048 ) ) {
			Add( child, store, detail );
			if ( depth > 0 && IsContainerFolder( child ) ) AddChildren( child, store, detail, depth - 1 );
		}
	}

	std::vector<Candidate> Take() { return std::move( items_ ); }

private:
	std::vector<Candidate> items_;
};

bool AllDigits( const std::string &text )
{
	if ( text.empty() ) return false;
	return std::all_of( text.begin(), text.end(), []( char c ) { return c >= '0' && c <= '9'; } );
}

bool SingleComponent( const std::string &name )
{
	if ( name.empty() || name == "." || name == ".." ) return false;
	return name.find_first_of( "/\\:" ) == std::string::npos;
}

std::string Trim( const std::string &text )
{
	const std::size_t first = text.find_first_not_of( " \t\r\n" );
	if ( first == std::string::npos ) return {};
	const std::size_t last = text.find_last_not_of( " \t\r\n" );
	return text.substr( first, last - first + 1 );
}

std::string Unquote( std::string text )
{
	text = Trim( text );
	if ( text.size() >= 2 && ( ( text.front() == '"' && text.back() == '"' )
		|| ( text.front() == '\'' && text.back() == '\'' ) ) ) {
		text = text.substr( 1, text.size() - 2 );
	}
	return text;
}

void AddSteamRoot( CandidateList &list, std::set<std::string> &libraries, const stdfs::path &root, Store store )
{
	if ( !fs::IsDirectory( root ) ) return;
	for ( const stdfs::path &library : SteamLibraries( root ) ) {
		if ( !libraries.insert( fs::IdentityKey( library ) ).second ) continue;
		const std::string detail = "Steam library " + fs::ToUtf8( library );
		for ( const SteamApp &app : SteamApps( library ) ) {
			list.Add( app.folder, store, detail, app.name, app.appId );
		}
	}
}

#ifdef _WIN32

std::string Narrow( const std::wstring &wide )
{
	return fs::ToUtf8( stdfs::path( wide ) );
}

// A registry string as a folder: quotes and an icon index removed, a file
// replaced by its folder, environment references expanded.
stdfs::path RegistryFolder( std::wstring value )
{
	auto trim = []( std::wstring &text ) {
		while ( !text.empty() && ( text.back() == L' ' || text.back() == L'\t' ) ) text.pop_back();
		std::size_t first = 0;
		while ( first < text.size() && ( text[first] == L' ' || text[first] == L'\t' ) ) ++first;
		text.erase( 0, first );
	};
	trim( value );
	if ( value.empty() ) return {};
	if ( value.front() == L'"' ) {
		const std::size_t close = value.find( L'"', 1 );
		value = value.substr( 1, close == std::wstring::npos ? std::wstring::npos : close - 1 );
	} else {
		const std::size_t comma = value.rfind( L',' );
		if ( comma != std::wstring::npos && comma + 1 < value.size()
			&& value.find_first_not_of( L"-0123456789", comma + 1 ) == std::wstring::npos ) {
			value.erase( comma );
		}
	}
	trim( value );
	if ( value.find( L'%' ) != std::wstring::npos ) {
		wchar_t expanded[4096];
		const DWORD length = ExpandEnvironmentStringsW( value.c_str(), expanded, 4096 );
		if ( length && length <= 4096 ) value = expanded;
	}
	if ( value.size() < 3 || ( value[1] != L':' && value.rfind( L"\\\\", 0 ) != 0 ) ) return {};
	stdfs::path path( value );
	const std::string extension = fs::Lower( fs::ToUtf8( path.extension() ) );
	if ( fs::IsRegularFile( path ) || extension == ".exe" || extension == ".ico" || extension == ".dll" ) {
		path = path.parent_path();
	}
	return path;
}

class RegistryKey {
public:
	RegistryKey( HKEY root, const std::wstring &path, REGSAM view )
	{
		if ( RegOpenKeyExW( root, path.c_str(), 0, KEY_READ | view, &key_ ) != ERROR_SUCCESS ) key_ = nullptr;
	}
	~RegistryKey()
	{
		if ( key_ ) RegCloseKey( key_ );
	}
	RegistryKey( const RegistryKey & ) = delete;
	RegistryKey &operator=( const RegistryKey & ) = delete;

	explicit operator bool() const { return key_ != nullptr; }

	std::vector<std::wstring> Subkeys( std::size_t limit = 8192 ) const
	{
		std::vector<std::wstring> names;
		if ( !key_ ) return names;
		for ( DWORD index = 0; names.size() < limit; ++index ) {
			wchar_t name[256];
			DWORD length = 256;
			const LSTATUS status = RegEnumKeyExW( key_, index, name, &length, nullptr, nullptr, nullptr, nullptr );
			if ( status == ERROR_NO_MORE_ITEMS ) break;
			if ( status == ERROR_SUCCESS ) names.emplace_back( name, length );
		}
		return names;
	}

	std::wstring String( const wchar_t *name ) const
	{
		if ( !key_ ) return {};
		DWORD type = 0, size = 0;
		if ( RegQueryValueExW( key_, name, nullptr, &type, nullptr, &size ) != ERROR_SUCCESS
			|| ( type != REG_SZ && type != REG_EXPAND_SZ ) || size < sizeof( wchar_t ) || size > 65536 ) {
			return {};
		}
		std::wstring data( size / sizeof( wchar_t ) + 1, L'\0' );
		if ( RegQueryValueExW( key_, name, nullptr, &type, reinterpret_cast<BYTE *>( data.data() ), &size ) != ERROR_SUCCESS ) {
			return {};
		}
		data.resize( wcsnlen( data.c_str(), data.size() ) );
		return data;
	}

	struct NamedString {
		std::wstring name;
		std::wstring data;
	};

	std::vector<NamedString> Strings( std::size_t limit = 256 ) const
	{
		std::vector<NamedString> values;
		if ( !key_ ) return values;
		for ( DWORD index = 0; values.size() < limit; ++index ) {
			wchar_t name[512];
			DWORD nameLength = 512, type = 0, size = 0;
			const LSTATUS status = RegEnumValueW( key_, index, name, &nameLength, nullptr, &type, nullptr, &size );
			if ( status == ERROR_NO_MORE_ITEMS ) break;
			if ( status != ERROR_SUCCESS || ( type != REG_SZ && type != REG_EXPAND_SZ ) ) continue;
			NamedString value;
			value.name.assign( name, nameLength );
			value.data = String( value.name.c_str() );
			if ( !value.data.empty() ) values.push_back( std::move( value ) );
		}
		return values;
	}

private:
	HKEY key_ = nullptr;
};

const REGSAM kViews[] = { KEY_WOW64_64KEY, KEY_WOW64_32KEY };

bool InstallValueName( const std::wstring &wide )
{
	const std::string name = fs::Lower( Narrow( wide ) );
	if ( name == "path" || name == "gamepath" || name == "gamedir" || name == "exestring" ) return true;
	if ( name.find( "install" ) == std::string::npos ) return false;
	return name.find( "path" ) != std::string::npos || name.find( "dir" ) != std::string::npos
		|| name.find( "folder" ) != std::string::npos || name.find( "location" ) != std::string::npos;
}

void AddPublisherKeys( CandidateList &list, HKEY root, const std::wstring &path, REGSAM view, int depth )
{
	RegistryKey key( root, path, view );
	if ( !key ) return;
	for ( const auto &value : key.Strings() ) {
		if ( !InstallValueName( value.name ) ) continue;
		list.Add( RegistryFolder( value.data ), Store::Retail, "Registered by its installer" );
	}
	if ( depth <= 0 ) return;
	for ( const std::wstring &child : key.Subkeys( 256 ) ) {
		AddPublisherKeys( list, root, path + L"\\" + child, view, depth - 1 );
	}
}

void GatherWindows( CandidateList &list, std::set<std::string> &libraries, const DiscoveryOptions &options )
{
	if ( options.scanStores ) {
		// Steam: the client's own registration, then its usual folders.
		std::vector<stdfs::path> steamRoots;
		{
			RegistryKey user( HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0 );
			const std::wstring path = user.String( L"SteamPath" );
			if ( !path.empty() ) steamRoots.emplace_back( path );
		}
		for ( REGSAM view : kViews ) {
			RegistryKey machine( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", view );
			const std::wstring path = machine.String( L"InstallPath" );
			if ( !path.empty() ) steamRoots.emplace_back( path );
		}
		for ( const char *variable : { "ProgramFiles(x86)", "ProgramW6432", "ProgramFiles" } ) {
			const std::string folder = fs::Environment( variable );
			if ( !folder.empty() ) steamRoots.push_back( fs::FromUtf8( folder ) / "Steam" );
		}
		for ( const stdfs::path &root : steamRoots ) AddSteamRoot( list, libraries, root, Store::Steam );

		// GOG Galaxy and GOG's offline installers both register here.
		for ( REGSAM view : kViews ) {
			RegistryKey games( HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games", view );
			for ( const std::wstring &id : games.Subkeys() ) {
				RegistryKey game( HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games\\" + id, view );
				std::wstring path = game.String( L"path" );
				if ( path.empty() ) path = game.String( L"PATH" );
				std::wstring name = game.String( L"gameName" );
				if ( name.empty() ) name = game.String( L"GAMENAME" );
				list.Add( RegistryFolder( path ), Store::Gog, "Registered by GOG", Narrow( name ), Narrow( id ) );
			}
		}

		// Publishers' own keys from disc installers, which often name only
		// an InstallPath (sometimes in 8.3 form).
		static const wchar_t *const publishers[] = {
			L"Activision", L"Aspyr", L"Electronic Arts", L"EA Games", L"Gathering of Developers",
			L"GODGames", L"Gray Matter", L"Id", L"id Software", L"LucasArts",
			L"LucasArts Entertainment Company", L"LucasArts Entertainment Company LLC",
			L"Raven", L"Raven Software", L"Ritual Entertainment", L"Rogue Entertainment",
			L"Splash Damage", L"2015", L"Disney Interactive"
		};
		for ( HKEY hive : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER } ) {
			for ( REGSAM view : kViews ) {
				for ( const wchar_t *publisher : publishers ) {
					AddPublisherKeys( list, hive, std::wstring( L"SOFTWARE\\" ) + publisher, view, 3 );
				}
			}
		}

		// Installed-programs entries: retail installers, GOG, the EA app.
		// Steam's own entries are skipped; its library manifests are authoritative
		// and its uninstall entries can name a stale library.
		const std::wstring uninstallPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
		for ( HKEY hive : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER } ) {
			for ( REGSAM view : kViews ) {
				RegistryKey uninstall( hive, uninstallPath, view );
				for ( const std::wstring &name : uninstall.Subkeys() ) {
					if ( name.rfind( L"Steam App ", 0 ) == 0 ) continue;
					RegistryKey entry( hive, uninstallPath + L"\\" + name, view );
					const std::wstring publisher = entry.String( L"Publisher" );
					const std::string title = Narrow( entry.String( L"DisplayName" ) );
					const bool gog = fs::ContainsNoCase( Narrow( publisher ), "gog" );
					const Store store = gog ? Store::Gog : Store::Retail;
					const char *detail = gog ? "Registered by GOG" : "Listed in Installed apps";
					stdfs::path folder = RegistryFolder( entry.String( L"InstallLocation" ) );
					if ( folder.empty() ) folder = RegistryFolder( entry.String( L"DisplayIcon" ) );
					if ( folder.empty() ) {
						const stdfs::path uninstaller = RegistryFolder( entry.String( L"UninstallString" ) );
						if ( !fs::ContainsNoCase( fs::ToUtf8( uninstaller ), "installshield installation information" ) ) {
							folder = uninstaller;
						}
					}
					list.Add( folder, store, detail, title );
				}
			}
		}
	}

	if ( options.scanCommonFolders ) {
		const DWORD drives = GetLogicalDrives();
		for ( int letter = 0; letter < 26; ++letter ) {
			if ( !( drives & ( 1u << letter ) ) ) continue;
			const wchar_t rootName[] = { wchar_t( L'A' + letter ), L':', L'\\', L'\0' };
			if ( GetDriveTypeW( rootName ) != DRIVE_FIXED ) continue;
			const stdfs::path root( rootName );
			list.AddChildren( root, Store::Local, "Found in a games folder", 1 );
			list.AddChildren( root / "Program Files", Store::Local, "Found in Program Files", 2 );
			list.AddChildren( root / "Program Files (x86)", Store::Local, "Found in Program Files", 2 );
			list.AddChildren( root / "GOG Games", Store::Gog, "Found in a GOG Games folder", 1 );
		}
		const stdfs::path home = fs::HomeDirectory();
		if ( !home.empty() ) list.AddChildren( home / "Games", Store::Local, "Found in a games folder", 2 );
	}
}

#else // !_WIN32

stdfs::path DataHome( const stdfs::path &home )
{
	const std::string xdg = fs::Environment( "XDG_DATA_HOME" );
	if ( !xdg.empty() && xdg[0] == '/' ) return fs::FromUtf8( xdg );
	return home.empty() ? stdfs::path() : home / ".local" / "share";
}

stdfs::path ConfigHome( const stdfs::path &home )
{
	const std::string xdg = fs::Environment( "XDG_CONFIG_HOME" );
	if ( !xdg.empty() && xdg[0] == '/' ) return fs::FromUtf8( xdg );
	return home.empty() ? stdfs::path() : home / ".config";
}

void AddWinePrefix( CandidateList &list, const stdfs::path &prefix, Store store, const std::string &detail )
{
	for ( const stdfs::path &container : WinePrefixGameFolders( prefix ) ) {
		list.AddChildren( container, store, detail, 2 );
	}
}

#ifndef __APPLE__
void AddLutrisConfigs( CandidateList &list, const stdfs::path &folder )
{
	if ( !fs::IsDirectory( folder ) ) return;
	try {
		std::error_code error;
		stdfs::directory_iterator it( folder, stdfs::directory_options::skip_permission_denied, error );
		const stdfs::directory_iterator end;
		for ( unsigned seen = 0; !error && it != end && seen < 4096; it.increment( error ), ++seen ) {
			if ( !fs::EqualsNoCase( fs::ToUtf8( it->path().extension() ), ".yml" ) ) continue;
			std::string text;
			if ( !fs::ReadFile( it->path(), text, 256 * 1024 ) ) continue;
			for ( const stdfs::path &path : ParseLutrisPaths( text ) ) {
				list.Add( path, Store::Lutris, "Installed with Lutris" );
				AddWinePrefix( list, path, Store::Lutris, "Installed with Lutris" );
			}
		}
	} catch ( const std::exception & ) {
	}
}
#endif

void GatherPosix( CandidateList &list, std::set<std::string> &libraries, const DiscoveryOptions &options )
{
	const stdfs::path home = fs::HomeDirectory();
	const stdfs::path data = DataHome( home );
	const stdfs::path config = ConfigHome( home );
	(void)data;
	(void)config;
	if ( options.scanStores ) {
#ifdef __APPLE__
		if ( !home.empty() ) {
			AddSteamRoot( list, libraries, home / "Library" / "Application Support" / "Steam", Store::Steam );
		}
#else
		for ( const char *variable : { "STEAM_DIR", "STEAM_HOME" } ) {
			const std::string root = fs::Environment( variable );
			if ( !root.empty() && root[0] == '/' ) AddSteamRoot( list, libraries, fs::FromUtf8( root ), Store::Steam );
		}
		if ( !data.empty() ) AddSteamRoot( list, libraries, data / "Steam", Store::Steam );
		if ( !home.empty() ) {
			AddSteamRoot( list, libraries, home / ".steam" / "steam", Store::Steam );
			AddSteamRoot( list, libraries, home / ".steam" / "root", Store::Steam );
			AddSteamRoot( list, libraries, home / ".local" / "share" / "Steam", Store::Steam );
			const stdfs::path flatpak = home / ".var" / "app" / "com.valvesoftware.Steam";
			AddSteamRoot( list, libraries, flatpak / ".local" / "share" / "Steam", Store::SteamFlatpak );
			AddSteamRoot( list, libraries, flatpak / "data" / "Steam", Store::SteamFlatpak );
			AddSteamRoot( list, libraries, home / "snap" / "steam" / "common" / ".local" / "share" / "Steam",
				Store::SteamSnap );
		}

		// Heroic Games Launcher (GOG) keeps a list of what it installed.
		std::vector<stdfs::path> heroicConfigs;
		if ( !config.empty() ) heroicConfigs.push_back( config / "heroic" );
		if ( !home.empty() ) {
			heroicConfigs.push_back( home / ".var" / "app" / "com.heroicgameslauncher.hgl" / "config" / "heroic" );
		}
		for ( const stdfs::path &folder : heroicConfigs ) {
			std::string text;
			if ( !fs::ReadFile( folder / "gog_store" / "installed.json", text, 4 * 1024 * 1024 ) ) continue;
			for ( const HeroicGame &game : ParseHeroicInstalled( text ) ) {
				list.Add( game.folder, Store::Heroic, "Installed with Heroic", game.title );
			}
		}
		if ( !home.empty() ) list.AddChildren( home / "Games" / "Heroic", Store::Heroic, "Installed with Heroic", 0 );

		// Lutris's game configurations name each game's folder or prefix.
		if ( !config.empty() ) AddLutrisConfigs( list, config / "lutris" / "games" );
		if ( !data.empty() ) AddLutrisConfigs( list, data / "lutris" / "games" );
#endif
	}

	if ( options.scanCommonFolders ) {
		if ( !home.empty() ) {
			list.AddChildren( home / "Games", Store::Local, "Found in a games folder", 2 );
			list.AddChildren( home / "games", Store::Local, "Found in a games folder", 2 );
			list.AddChildren( home / "GOG Games", Store::Gog, "Found in a GOG Games folder", 1 );
		}
#ifdef __APPLE__
		list.AddChildren( "/Applications", Store::Local, "Found in Applications", 2 );
		if ( !home.empty() ) {
			list.AddChildren( home / "Applications", Store::Local, "Found in Applications", 2 );
			const stdfs::path bottles = home / "Library" / "Application Support" / "CrossOver" / "Bottles";
			for ( const stdfs::path &bottle : fs::Subdirectories( bottles, 256 ) ) {
				AddWinePrefix( list, bottle, Store::Local, "Found in a CrossOver bottle" );
			}
		}
#else
		for ( const char *folder : { "/usr/share/games", "/usr/local/share/games", "/usr/local/games",
				"/usr/games", "/opt" } ) {
			list.AddChildren( folder, Store::Local, "Found in a system games folder", 1 );
		}
		// Wine prefixes: the default one, the player's own, Bottles and
		// PlayOnLinux, and Lutris's default games folder.
		const std::string winePrefix = fs::Environment( "WINEPREFIX" );
		if ( !winePrefix.empty() && winePrefix[0] == '/' ) {
			AddWinePrefix( list, fs::FromUtf8( winePrefix ), Store::Local, "Found in a Wine prefix" );
		}
		if ( !home.empty() ) {
			AddWinePrefix( list, home / ".wine", Store::Local, "Found in a Wine prefix" );
			for ( const stdfs::path &prefix : fs::Subdirectories( home / "Games", 512 ) ) {
				AddWinePrefix( list, prefix, Store::Lutris, "Installed with Lutris" );
			}
			for ( const stdfs::path &prefix : fs::Subdirectories( home / ".PlayOnLinux" / "wineprefix", 256 ) ) {
				AddWinePrefix( list, prefix, Store::Local, "Found in a PlayOnLinux prefix" );
			}
			const stdfs::path flatpakBottles = home / ".var" / "app" / "com.usebottles.bottles" / "data"
				/ "bottles" / "bottles";
			for ( const stdfs::path &bottle : fs::Subdirectories( flatpakBottles, 256 ) ) {
				AddWinePrefix( list, bottle, Store::Local, "Found in a Bottles bottle" );
			}
		}
		if ( !data.empty() ) {
			for ( const stdfs::path &bottle : fs::Subdirectories( data / "bottles" / "bottles", 256 ) ) {
				AddWinePrefix( list, bottle, Store::Local, "Found in a Bottles bottle" );
			}
		}
#endif
	}
}

#endif // _WIN32

} // namespace

std::vector<stdfs::path> SteamLibraries( const stdfs::path &steamRoot )
{
	std::vector<stdfs::path> libraries;
	std::set<std::string> seen;
	auto add = [&libraries, &seen]( const stdfs::path &candidate ) {
		const stdfs::path library = fs::Canonical( candidate );
		if ( library.empty() || fs::FindChild( library, "steamapps" ).empty() ) return;
		if ( seen.insert( fs::IdentityKey( library ) ).second ) libraries.push_back( library );
	};
	add( steamRoot );
	const stdfs::path steamapps = fs::FindChild( steamRoot, "steamapps" );
	if ( steamapps.empty() ) return libraries;
	vdf::Node document;
	const stdfs::path file = fs::FindChild( steamapps, "libraryfolders.vdf" );
	if ( file.empty() || !vdf::ParseFile( fs::ToUtf8( file ), document ) ) return libraries;
	const vdf::Node *folders = document.Find( "libraryfolders" );
	if ( !folders || !folders->block ) return libraries;
	for ( const vdf::Node &entry : folders->children ) {
		if ( !AllDigits( entry.key ) ) continue;
		const std::string path = entry.block ? entry.Get( "path" ) : entry.value;
		if ( !path.empty() ) add( fs::FromUtf8( path ) );
	}
	return libraries;
}

std::vector<SteamApp> SteamApps( const stdfs::path &library )
{
	std::vector<SteamApp> apps;
	const stdfs::path steamapps = fs::FindChild( library, "steamapps" );
	if ( steamapps.empty() ) return apps;
	const stdfs::path common = fs::FindChild( steamapps, "common" );
	try {
		std::error_code error;
		stdfs::directory_iterator it( steamapps, stdfs::directory_options::skip_permission_denied, error );
		const stdfs::directory_iterator end;
		for ( unsigned seen = 0; !error && it != end && seen < 16384; it.increment( error ), ++seen ) {
			const std::string leaf = fs::Lower( fs::ToUtf8( it->path().filename() ) );
			if ( leaf.rfind( "appmanifest_", 0 ) != 0 || leaf.size() < 17
				|| leaf.compare( leaf.size() - 4, 4, ".acf" ) != 0 ) continue;
			vdf::Node document;
			if ( !vdf::ParseFile( fs::ToUtf8( it->path() ), document ) ) continue;
			const vdf::Node *state = document.Find( "AppState" );
			if ( !state || !state->block ) continue;
			SteamApp app;
			app.appId = state->Get( "appid" );
			app.name = state->Get( "name" );
			const std::string installDir = state->Get( "installdir" );
			if ( !AllDigits( app.appId ) || !SingleComponent( installDir ) || common.empty() ) continue;
			app.folder = fs::FindChild( common, installDir.c_str() );
			if ( app.folder.empty() || !fs::IsDirectory( app.folder ) ) continue;
			apps.push_back( std::move( app ) );
		}
	} catch ( const std::exception & ) {
	}
	std::sort( apps.begin(), apps.end(), []( const SteamApp &a, const SteamApp &b ) {
		return a.folder < b.folder;
	} );
	return apps;
}

std::vector<HeroicGame> ParseHeroicInstalled( const std::string &text )
{
	std::vector<HeroicGame> games;
	json::Value document;
	if ( !json::Parse( text, document ) ) return games;
	const json::Value *list = document.IsArray() ? &document : document.Find( "installed" );
	if ( !list ) return games;
	for ( const json::Value &item : list->Items() ) {
		const json::Value *path = item.Find( "install_path" );
		if ( !path || path->AsString().empty() ) continue;
		HeroicGame game;
		game.folder = fs::FromUtf8( path->AsString() );
		if ( const json::Value *title = item.Find( "title" ) ) game.title = title->AsString();
		if ( !game.folder.empty() ) games.push_back( std::move( game ) );
	}
	return games;
}

std::vector<stdfs::path> ParseLutrisPaths( const std::string &text )
{
	std::vector<stdfs::path> paths;
	std::size_t start = 0;
	while ( start < text.size() ) {
		std::size_t end = text.find( '\n', start );
		if ( end == std::string::npos ) end = text.size();
		const std::string line = Trim( text.substr( start, end - start ) );
		start = end + 1;
		const std::size_t colon = line.find( ':' );
		if ( colon == std::string::npos ) continue;
		const std::string key = Trim( line.substr( 0, colon ) );
		if ( key != "exe" && key != "prefix" && key != "game_path" && key != "working_dir" ) continue;
		const std::string value = Unquote( line.substr( colon + 1 ) );
		if ( value.empty() || value[0] != '/' ) continue;
		stdfs::path path = fs::FromUtf8( value );
		if ( key == "exe" ) path = path.parent_path();
		if ( !path.empty() && std::find( paths.begin(), paths.end(), path ) == paths.end() ) paths.push_back( path );
	}
	return paths;
}

std::vector<stdfs::path> WinePrefixGameFolders( const stdfs::path &prefix )
{
	std::vector<stdfs::path> folders;
	const stdfs::path drive = fs::FindChild( prefix, "drive_c" );
	if ( drive.empty() || !fs::IsDirectory( drive ) ) return folders;
	for ( const char *name : { "Program Files (x86)", "Program Files", "GOG Games", "Games" } ) {
		const stdfs::path folder = fs::FindChild( drive, name );
		if ( !folder.empty() && fs::IsDirectory( folder ) ) folders.push_back( folder );
	}
	return folders;
}

std::vector<Candidate> GatherCandidates( const DiscoveryOptions &options )
{
	CandidateList list;
	std::set<std::string> libraries;
#ifdef _WIN32
	GatherWindows( list, libraries, options );
#else
	GatherPosix( list, libraries, options );
#endif
	for ( const stdfs::path &folder : options.searchFolders ) {
		list.Add( folder, Store::Local, "In a folder you search" );
		list.AddChildren( folder, Store::Local, "In a folder you search", 2 );
	}
	if ( !options.launcherDirectory.empty() ) {
		list.Add( options.launcherDirectory, Store::Portable, "fnTech3 is installed in this folder" );
		if ( options.launcherDirectory.has_parent_path() ) {
			list.Add( options.launcherDirectory.parent_path(), Store::Portable, "fnTech3 is installed in this folder" );
		}
	}
	return list.Take();
}

std::vector<Installation> ProbeManualFolder( const stdfs::path &folder )
{
	std::vector<Installation> result;
	for ( const stdfs::path &path : ManualProbeOrder( folder ) ) {
		Candidate candidate;
		candidate.path = path;
		candidate.store = Store::Manual;
		candidate.detail = "Added by you";
		for ( const ProbedRoot &probe : ProbeFolder( path ) ) {
			for ( Installation &installation : InstallationsFromProbe( probe, candidate ) ) {
				installation.manual = true;
				result.push_back( std::move( installation ) );
			}
		}
		if ( !result.empty() ) break;
	}
	MergeInstallations( result );
	return result;
}

DiscoveryResult Discover( const DiscoveryOptions &options, const std::atomic<bool> *cancel,
	DiscoveryProgress *progress )
{
	const auto started = std::chrono::steady_clock::now();
	DiscoveryResult result;
	const std::vector<Candidate> candidates = GatherCandidates( options );
	if ( progress ) progress->candidates = unsigned( candidates.size() + options.manual.size() );
	std::unordered_map<std::string, std::vector<ProbedRoot>> probes;
	std::set<std::string> found;
	std::vector<Installation> installations;
	auto collect = [&]( std::vector<Installation> &&batch ) {
		for ( Installation &installation : batch ) {
			found.insert( installation.id );
			installations.push_back( std::move( installation ) );
		}
		if ( progress ) progress->found = unsigned( found.size() );
	};
	for ( const Candidate &candidate : candidates ) {
		if ( cancel && cancel->load() ) {
			result.cancelled = true;
			break;
		}
		if ( progress ) ++progress->probed;
		const stdfs::path folder = fs::Canonical( candidate.path );
		if ( folder.empty() ) continue;
		const std::string key = fs::IdentityKey( folder );
		auto probe = probes.find( key );
		if ( probe == probes.end() ) {
			probe = probes.emplace( key, ProbeFolder( folder ) ).first;
			++result.foldersProbed;
		}
		for ( const ProbedRoot &root : probe->second ) collect( InstallationsFromProbe( root, candidate ) );
	}
	for ( const ManualFolder &manual : options.manual ) {
		if ( cancel && cancel->load() ) {
			result.cancelled = true;
			break;
		}
		if ( progress ) ++progress->probed;
		std::vector<Installation> batch;
		if ( fs::IsDirectory( manual.folder ) ) {
			batch = ProbeManualFolder( manual.folder );
			if ( batch.empty() && !manual.group.empty() ) {
				batch.push_back( UnverifiedInstallation( manual.folder, manual.group, false ) );
			}
		} else if ( !manual.group.empty() ) {
			batch.push_back( UnverifiedInstallation( manual.folder, manual.group, true ) );
		}
		collect( std::move( batch ) );
	}
	MergeInstallations( installations );
	result.installations = std::move( installations );
	result.seconds = std::chrono::duration<double>( std::chrono::steady_clock::now() - started ).count();
	return result;
}

} // namespace fntech3::launcher
