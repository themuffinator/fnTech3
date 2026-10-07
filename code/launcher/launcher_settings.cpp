/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_settings.hpp"

#include "launcher_fs.hpp"
#include "launcher_json.hpp"

#include <cmath>
#include <system_error>

namespace fntech3::launcher {

namespace stdfs = std::filesystem;

namespace {

constexpr std::size_t kMaxString = 32 * 1024;
constexpr std::size_t kMaxManual = 512;
constexpr std::size_t kMaxSearchFolders = 64;
constexpr std::size_t kMaxInstallations = 2048;
constexpr std::size_t kMaxCache = 1024;

const char *ThemeKey( Theme theme )
{
	switch ( theme ) {
	case Theme::Dark: return "dark";
	case Theme::Light: return "light";
	default: return "system";
	}
}

const char *AfterLaunchKey( AfterLaunch after )
{
	switch ( after ) {
	case AfterLaunch::Minimize: return "minimize";
	case AfterLaunch::Close: return "close";
	default: return "keep-open";
	}
}

const char *SortKey( SortOrder sort )
{
	switch ( sort ) {
	case SortOrder::RecentlyPlayed: return "recent";
	case SortOrder::Location: return "location";
	default: return "name";
	}
}

bool ReadString( const json::Value &object, const char *key, std::string &out )
{
	const json::Value *value = object.Find( key );
	if ( !value || !value->IsString() || value->AsString().size() > kMaxString ) return false;
	out = value->AsString();
	return true;
}

void ReadBool( const json::Value &object, const char *key, bool &out )
{
	const json::Value *value = object.Find( key );
	if ( value && value->IsBool() ) out = value->AsBool();
}

void ReadInt( const json::Value &object, const char *key, int &out )
{
	const json::Value *value = object.Find( key );
	if ( !value || !value->IsNumber() ) return;
	const double number = value->AsNumber();
	if ( number >= double( INT_MIN ) && number <= double( INT_MAX ) ) out = int( std::floor( number ) );
}

void ReadInt64( const json::Value &object, const char *key, std::int64_t &out )
{
	const json::Value *value = object.Find( key );
	if ( !value || !value->IsNumber() ) return;
	const double number = value->AsNumber();
	if ( number >= 0.0 && number < 9.0e15 ) out = std::int64_t( number );
}

json::Value Strings( const std::vector<std::string> &values )
{
	json::Value array = json::Value::Array();
	for ( const std::string &value : values ) array.Push( json::Value::String( value ) );
	return array;
}

json::Value CacheEntry( const Installation &installation )
{
	json::Value entry = json::Value::Object();
	entry.Set( "root", json::Value::String( installation.rootUtf8 ) );
	entry.Set( "group", json::Value::String( installation.group ) );
	std::vector<std::string> modes;
	for ( const comGameProfile_t *profile : installation.modes ) modes.push_back( profile->key );
	entry.Set( "modes", Strings( modes ) );
	entry.Set( "markers", json::Value::Number( double( installation.markers ) ) );
	entry.Set( "store", json::Value::String( StoreKey( installation.store ) ) );
	entry.Set( "detail", json::Value::String( installation.detail ) );
	entry.Set( "storeTitle", json::Value::String( installation.storeTitle ) );
	entry.Set( "storeId", json::Value::String( installation.storeId ) );
	entry.Set( "release", json::Value::String( installation.release ) );
	entry.Set( "verified", json::Value::Boolean( installation.verified ) );
	entry.Set( "manual", json::Value::Boolean( installation.manual ) );
	entry.Set( "missing", json::Value::Boolean( installation.missing ) );
	entry.Set( "portable", json::Value::Boolean( installation.portable ) );
	return entry;
}

bool ReadCacheEntry( const json::Value &entry, Installation &installation )
{
	if ( !entry.IsObject() ) return false;
	std::string root, group, store;
	if ( !ReadString( entry, "root", root ) || root.empty() || !ReadString( entry, "group", group ) ) return false;
	const auto profiles = GroupProfiles( group );
	if ( profiles.empty() ) return false;
	installation = Installation();
	installation.root = fs::FromUtf8( root );
	if ( installation.root.empty() ) return false;
	installation.rootUtf8 = root;
	installation.group = group;
	installation.id = fs::IdentityKey( installation.root ) + "|" + group;
	installation.title = profiles.front()->titleId;
	if ( const json::Value *modes = entry.Find( "modes" ) ) {
		for ( const comGameProfile_t *profile : profiles ) {
			for ( const json::Value &mode : modes->Items() ) {
				if ( mode.AsString() == profile->key ) installation.modes.push_back( profile );
			}
		}
	}
	if ( installation.modes.empty() ) return false;
	int markers = 0;
	ReadInt( entry, "markers", markers );
	installation.markers = unsigned( markers );
	if ( ReadString( entry, "store", store ) ) StoreFromKey( store, installation.store );
	ReadString( entry, "detail", installation.detail );
	ReadString( entry, "storeTitle", installation.storeTitle );
	ReadString( entry, "storeId", installation.storeId );
	ReadString( entry, "release", installation.release );
	ReadBool( entry, "verified", installation.verified );
	ReadBool( entry, "manual", installation.manual );
	ReadBool( entry, "missing", installation.missing );
	ReadBool( entry, "portable", installation.portable );
	return true;
}

} // namespace

stdfs::path SettingsFile( const stdfs::path &preferenceFolder, const stdfs::path &launcherDirectory )
{
	if ( !launcherDirectory.empty() ) {
		const stdfs::path portable = launcherDirectory / "fntech3-launcher.json";
		if ( fs::IsRegularFile( portable ) ) return portable;
	}
	if ( preferenceFolder.empty() ) return launcherDirectory.empty() ? stdfs::path()
		: launcherDirectory / "fntech3-launcher.json";
	return preferenceFolder / "launcher.json";
}

std::string SerializeSettings( const Settings &settings )
{
	json::Value root = json::Value::Object();
	root.Set( "version", json::Value::Number( kSettingsVersion ) );
	root.Set( "theme", json::Value::String( ThemeKey( settings.theme ) ) );
	root.Set( "afterLaunch", json::Value::String( AfterLaunchKey( settings.afterLaunch ) ) );
	root.Set( "sort", json::Value::String( SortKey( settings.sort ) ) );
	root.Set( "showUnavailable", json::Value::Boolean( settings.showUnavailable ) );
	root.Set( "unavailableCollapsed", json::Value::Boolean( settings.unavailableCollapsed ) );
	root.Set( "showHidden", json::Value::Boolean( settings.showHidden ) );
	root.Set( "engine", json::Value::String( settings.enginePath ) );
	root.Set( "searchFolders", Strings( settings.searchFolders ) );

	json::Value manual = json::Value::Array();
	for ( const ManualEntry &entry : settings.manual ) {
		json::Value item = json::Value::Object();
		item.Set( "path", json::Value::String( entry.path ) );
		item.Set( "group", json::Value::String( entry.group ) );
		item.Set( "added", json::Value::Number( double( entry.added ) ) );
		manual.Push( std::move( item ) );
	}
	root.Set( "manual", std::move( manual ) );

	json::Value installations = json::Value::Object();
	for ( const auto &[id, prefs] : settings.installations ) {
		json::Value item = json::Value::Object();
		if ( !prefs.name.empty() ) item.Set( "name", json::Value::String( prefs.name ) );
		if ( !prefs.mode.empty() ) item.Set( "mode", json::Value::String( prefs.mode ) );
		if ( !prefs.mod.empty() ) item.Set( "mod", json::Value::String( prefs.mod ) );
		if ( !prefs.arguments.empty() ) item.Set( "arguments", json::Value::String( prefs.arguments ) );
		if ( prefs.hidden ) item.Set( "hidden", json::Value::Boolean( true ) );
		if ( prefs.lastPlayed ) item.Set( "lastPlayed", json::Value::Number( double( prefs.lastPlayed ) ) );
		if ( prefs.playCount ) item.Set( "playCount", json::Value::Number( double( prefs.playCount ) ) );
		if ( item.Members().empty() ) continue;
		installations.Set( id, std::move( item ) );
	}
	root.Set( "installations", std::move( installations ) );

	json::Value window = json::Value::Object();
	if ( settings.window.x != INT_MIN && settings.window.y != INT_MIN ) {
		window.Set( "x", json::Value::Number( settings.window.x ) );
		window.Set( "y", json::Value::Number( settings.window.y ) );
	}
	if ( settings.window.width > 0 && settings.window.height > 0 ) {
		window.Set( "width", json::Value::Number( settings.window.width ) );
		window.Set( "height", json::Value::Number( settings.window.height ) );
	}
	window.Set( "maximized", json::Value::Boolean( settings.window.maximized ) );
	root.Set( "window", std::move( window ) );

	json::Value cache = json::Value::Array();
	for ( const Installation &installation : settings.cache ) cache.Push( CacheEntry( installation ) );
	root.Set( "cache", std::move( cache ) );
	return json::Serialize( root );
}

bool ParseSettings( const std::string &text, Settings &settings, std::string *error )
{
	settings = Settings();
	json::Value root;
	std::string parseError;
	if ( !json::Parse( text, root, &parseError ) ) {
		if ( error ) *error = "the settings file is not valid JSON (" + parseError + ")";
		return false;
	}
	if ( !root.IsObject() ) {
		if ( error ) *error = "the settings file does not hold a JSON object";
		return false;
	}
	int version = 0;
	ReadInt( root, "version", version );
	if ( version > kSettingsVersion ) {
		// A newer launcher wrote it; read what this one understands.
		if ( error ) *error = "the settings file was written by a newer launcher";
	}
	std::string choice;
	if ( ReadString( root, "theme", choice ) ) {
		settings.theme = choice == "dark" ? Theme::Dark : choice == "light" ? Theme::Light : Theme::System;
	}
	if ( ReadString( root, "afterLaunch", choice ) ) {
		settings.afterLaunch = choice == "minimize" ? AfterLaunch::Minimize
			: choice == "close" ? AfterLaunch::Close : AfterLaunch::KeepOpen;
	}
	if ( ReadString( root, "sort", choice ) ) {
		settings.sort = choice == "recent" ? SortOrder::RecentlyPlayed
			: choice == "location" ? SortOrder::Location : SortOrder::Name;
	}
	ReadBool( root, "showUnavailable", settings.showUnavailable );
	ReadBool( root, "unavailableCollapsed", settings.unavailableCollapsed );
	ReadBool( root, "showHidden", settings.showHidden );
	ReadString( root, "engine", settings.enginePath );

	if ( const json::Value *folders = root.Find( "searchFolders" ) ) {
		for ( const json::Value &folder : folders->Items() ) {
			if ( settings.searchFolders.size() >= kMaxSearchFolders ) break;
			if ( folder.IsString() && !folder.AsString().empty() && folder.AsString().size() <= kMaxString ) {
				settings.searchFolders.push_back( folder.AsString() );
			}
		}
	}
	if ( const json::Value *manual = root.Find( "manual" ) ) {
		for ( const json::Value &item : manual->Items() ) {
			if ( settings.manual.size() >= kMaxManual ) break;
			ManualEntry entry;
			if ( !item.IsObject() || !ReadString( item, "path", entry.path ) || entry.path.empty() ) continue;
			ReadString( item, "group", entry.group );
			if ( !entry.group.empty() && GroupProfiles( entry.group ).empty() ) entry.group.clear();
			ReadInt64( item, "added", entry.added );
			settings.manual.push_back( std::move( entry ) );
		}
	}
	if ( const json::Value *installations = root.Find( "installations" ) ) {
		for ( const auto &[id, item] : installations->Members() ) {
			if ( settings.installations.size() >= kMaxInstallations ) break;
			if ( id.empty() || id.size() > kMaxString || !item.IsObject() ) continue;
			InstallationPrefs prefs;
			ReadString( item, "name", prefs.name );
			ReadString( item, "mode", prefs.mode );
			ReadString( item, "mod", prefs.mod );
			ReadString( item, "arguments", prefs.arguments );
			ReadBool( item, "hidden", prefs.hidden );
			ReadInt64( item, "lastPlayed", prefs.lastPlayed );
			std::int64_t count = 0;
			ReadInt64( item, "playCount", count );
			prefs.playCount = count > 0 && count < 1000000000 ? unsigned( count ) : 0u;
			settings.installations[id] = std::move( prefs );
		}
	}
	if ( const json::Value *window = root.Find( "window" ) ) {
		if ( window->IsObject() ) {
			ReadInt( *window, "x", settings.window.x );
			ReadInt( *window, "y", settings.window.y );
			ReadInt( *window, "width", settings.window.width );
			ReadInt( *window, "height", settings.window.height );
			ReadBool( *window, "maximized", settings.window.maximized );
			if ( settings.window.width < 0 || settings.window.width > 32768
				|| settings.window.height < 0 || settings.window.height > 32768 ) {
				settings.window.width = settings.window.height = 0;
			}
		}
	}
	if ( const json::Value *cache = root.Find( "cache" ) ) {
		for ( const json::Value &entry : cache->Items() ) {
			if ( settings.cache.size() >= kMaxCache ) break;
			Installation installation;
			if ( ReadCacheEntry( entry, installation ) ) settings.cache.push_back( std::move( installation ) );
		}
		MergeInstallations( settings.cache );
	}
	return true;
}

bool LoadSettings( const stdfs::path &file, Settings &settings, std::string *error )
{
	settings = Settings();
	if ( error ) error->clear();
	if ( file.empty() || !fs::IsRegularFile( file ) ) return true;
	std::string text;
	if ( !fs::ReadFile( file, text, kMaxSettingsBytes ) ) {
		if ( error ) *error = "the settings file could not be read or is too large";
		return false;
	}
	// An empty file is how a player asks for a portable settings file.
	if ( text.find_first_not_of( " \t\r\n" ) == std::string::npos ) return true;
	std::string parseError;
	if ( ParseSettings( text, settings, &parseError ) ) {
		if ( error ) *error = parseError;
		return true;
	}
	// Keep the damaged file for the player to inspect; the next save starts afresh.
	std::error_code code;
	stdfs::path backup = file;
	backup += ".bad";
	stdfs::copy_file( file, backup, stdfs::copy_options::overwrite_existing, code );
	if ( error ) *error = parseError;
	settings = Settings();
	return false;
}

bool SaveSettings( const stdfs::path &file, const Settings &settings, std::string *error )
{
	return fs::WriteFileAtomic( file, SerializeSettings( settings ), error );
}

} // namespace fntech3::launcher
