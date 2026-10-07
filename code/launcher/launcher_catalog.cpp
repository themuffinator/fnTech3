/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_catalog.hpp"

#include "launcher_fs.hpp"
#include "launcher_json.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <system_error>

namespace fntech3::launcher {

namespace stdfs = std::filesystem;

namespace {

struct StoreInfo {
	Store store;
	const char *name;
	const char *key;
	int rank;
};

const StoreInfo kStores[] = {
	{ Store::Local, "Local folder", "local", 30 },
	{ Store::Manual, "Added manually", "manual", 40 },
	{ Store::Portable, "Portable", "portable", 50 },
	{ Store::Retail, "Retail", "retail", 60 },
	{ Store::Steam, "Steam", "steam", 100 },
	{ Store::SteamFlatpak, "Steam (Flatpak)", "steam-flatpak", 99 },
	{ Store::SteamSnap, "Steam (Snap)", "steam-snap", 98 },
	{ Store::Gog, "GOG", "gog", 90 },
	{ Store::Heroic, "Heroic", "heroic", 85 },
	{ Store::Lutris, "Lutris", "lutris", 80 },
	{ Store::EaApp, "EA app", "ea", 75 },
};
static_assert( sizeof( kStores ) / sizeof( kStores[0] ) == std::size_t( Store::Count ),
	"Each store needs one row" );

const StoreInfo &Info( Store store )
{
	for ( const StoreInfo &info : kStores ) {
		if ( info.store == store ) return info;
	}
	return kStores[0];
}

// The host the engine runs on, in the engine's own module-name terms: the
// same ARCH_STRING and DLL_EXT q_platform.h gives the client built beside us.
#if defined( _WIN32 )
const char *const kHostExtension = ".dll";
#if defined( _M_IX86 ) || defined( __i386__ )
const char *const kHostArchitecture = "x86";
#elif defined( _M_AMD64 ) || defined( __x86_64__ )
const char *const kHostArchitecture = "x86_64";
#elif defined( _M_ARM64 ) || defined( __aarch64__ )
const char *const kHostArchitecture = "arm64";
#else
const char *const kHostArchitecture = "unknown";
#endif
#else
const char *const kHostExtension = ".so";
#if defined( __i386__ )
const char *const kHostArchitecture = "i386";
#elif defined( __x86_64__ ) || defined( __amd64__ )
const char *const kHostArchitecture = "x86_64";
#elif defined( __aarch64__ )
const char *const kHostArchitecture = "aarch64";
#elif defined( __arm__ )
const char *const kHostArchitecture = "arm";
#else
const char *const kHostArchitecture = "unknown";
#endif
#endif

bool HasExtension( const stdfs::path &path, const char *extension )
{
	return fs::EqualsNoCase( fs::ToUtf8( path.extension() ), extension );
}

std::string ReleaseVersion( const stdfs::path &root )
{
	const q3testReleaseId_t id = Com_Q3TestDetectRelease( fs::ToUtf8( root ).c_str() );
	const q3testRelease_t *release = Com_Q3TestReleaseById( id );
	return release && release->version ? release->version : "";
}

bool ReadGogInfo( const stdfs::path &folder, std::string &name, std::string &id )
{
	try {
		std::error_code error;
		stdfs::directory_iterator it( folder, stdfs::directory_options::skip_permission_denied, error );
		const stdfs::directory_iterator end;
		for ( unsigned seen = 0; !error && it != end && seen < 4096; it.increment( error ), ++seen ) {
			const std::string leaf = fs::Lower( fs::ToUtf8( it->path().filename() ) );
			if ( leaf.rfind( "goggame-", 0 ) != 0 || leaf.size() < 13
				|| leaf.compare( leaf.size() - 5, 5, ".info" ) != 0 ) continue;
			std::string text;
			json::Value document;
			if ( !fs::ReadFile( it->path(), text, 256 * 1024 ) || !json::Parse( text, document )
				|| !document.IsObject() ) continue;
			if ( const json::Value *value = document.Find( "name" ) ) name = value->AsString();
			if ( const json::Value *value = document.Find( "gameId" ) ) id = value->AsString();
			return true;
		}
	} catch ( const std::exception & ) {
	}
	return false;
}

} // namespace

const char *StoreName( Store store ) { return Info( store ).name; }
const char *StoreKey( Store store ) { return Info( store ).key; }
int StoreRank( Store store ) { return Info( store ).rank; }

bool StoreFromKey( const std::string &key, Store &store )
{
	for ( const StoreInfo &info : kStores ) {
		if ( key == info.key ) {
			store = info.store;
			return true;
		}
	}
	return false;
}

std::string GroupKey( const comGameProfile_t *profile )
{
	if ( !profile ) return {};
	if ( profile->titleId == COM_GAME_TITLE_QUAKE3 ) return profile->key;
	const comGameTitle_t *title = Com_GameManagerTitleById( profile->titleId );
	return title ? title->key : profile->key;
}

std::vector<const comGameProfile_t *> GroupProfiles( const std::string &group )
{
	std::vector<const comGameProfile_t *> profiles;
	for ( unsigned i = 0; i < Com_GameManagerProfileCount(); ++i ) {
		const comGameProfile_t *profile = Com_GameManagerProfileAt( i );
		if ( profile && GroupKey( profile ) == group ) profiles.push_back( profile );
	}
	return profiles;
}

std::string GroupName( const std::string &group )
{
	const auto profiles = GroupProfiles( group );
	if ( profiles.empty() ) return group;
	if ( profiles.front()->titleId == COM_GAME_TITLE_QUAKE3 ) return profiles.front()->displayName;
	const comGameTitle_t *title = Com_GameManagerTitleById( profiles.front()->titleId );
	return title ? title->displayName : profiles.front()->displayName;
}

std::string ModeLabel( const comGameProfile_t *profile )
{
	if ( !profile || GroupProfiles( GroupKey( profile ) ).size() < 2 ) return {};
	const std::string name = profile->displayName;
	const std::size_t open = name.rfind( '(' );
	const std::size_t close = name.rfind( ')' );
	if ( open == std::string::npos || close == std::string::npos || close < open + 2 ) return {};
	return name.substr( open + 1, close - open - 1 );
}

int GroupOrder( const std::string &group )
{
	const auto profiles = GroupProfiles( group );
	if ( profiles.empty() ) return 1 << 20;
	int titleIndex = 0;
	for ( unsigned i = 0; i < Com_GameManagerTitleCount(); ++i ) {
		if ( Com_GameManagerTitleAt( i )->id == profiles.front()->titleId ) titleIndex = int( i );
	}
	int profileIndex = 0;
	for ( unsigned i = 0; i < Com_GameManagerTitleProfileCount( profiles.front()->titleId ); ++i ) {
		if ( Com_GameManagerTitleProfileAt( profiles.front()->titleId, i ) == profiles.front() ) {
			profileIndex = int( i );
		}
	}
	return titleIndex * 100 + profileIndex;
}

const std::vector<std::string> &BaseDirectoryNames()
{
	static const std::vector<std::string> names = [] {
		std::vector<std::string> result;
		for ( unsigned i = 0; i < Com_GameManagerProfileCount(); ++i ) {
			const comGameProfile_t *profile = Com_GameManagerProfileAt( i );
			if ( !profile || !profile->baseGame ) continue;
			const std::string name = fs::Lower( profile->baseGame );
			if ( std::find( result.begin(), result.end(), name ) == result.end() ) result.push_back( name );
		}
		return result;
	}();
	return names;
}

bool MightHoldGame( const stdfs::path &folder )
{
	if ( folder.empty() ) return false;
	const auto &names = BaseDirectoryNames();
	try {
		std::error_code error;
		stdfs::directory_iterator it( folder, stdfs::directory_options::skip_permission_denied, error );
		const stdfs::directory_iterator end;
		for ( unsigned seen = 0; !error && it != end && seen < 4096; it.increment( error ), ++seen ) {
			const std::string leaf = fs::Lower( fs::ToUtf8( it->path().filename() ) );
			if ( std::find( names.begin(), names.end(), leaf ) == names.end() ) continue;
			std::error_code typeError;
			if ( it->is_directory( typeError ) && !typeError ) return true;
		}
	} catch ( const std::exception & ) {
	}
	return false;
}

std::vector<ProbedRoot> ProbeFolder( const stdfs::path &folder )
{
	std::vector<ProbedRoot> result;
	if ( !fs::IsDirectory( folder ) ) return result;
	std::vector<stdfs::path> roots = { folder };
	for ( const char *nested : { "GameData", "game" } ) {
		const stdfs::path child = fs::FindChild( folder, nested );
		if ( !child.empty() && fs::IsDirectory( child ) ) roots.push_back( child );
	}
	if ( fs::EqualsNoCase( fs::ToUtf8( folder.extension() ), ".app" ) ) {
		roots.push_back( folder / "Contents" / "Resources" );
		roots.push_back( folder / "Contents" / "Resources" / "game" );
	}
	std::vector<std::string> seen;
	for ( const stdfs::path &candidate : roots ) {
		const stdfs::path root = fs::Canonical( candidate );
		if ( root.empty() || !fs::IsDirectory( root ) ) continue;
		const std::string key = fs::IdentityKey( root );
		if ( std::find( seen.begin(), seen.end(), key ) != seen.end() ) continue;
		seen.push_back( key );
		if ( !MightHoldGame( root ) ) continue;
		const std::string utf8 = fs::ToUtf8( root );
		if ( utf8.empty() ) continue;
		const unsigned markers = Com_ProbeGameProfileMarkers( utf8.c_str() );
		if ( markers & ~unsigned( COM_GAME_MARKER_UNRECOGNIZED_PAK ) ) result.push_back( { root, markers } );
	}
	return result;
}

std::vector<Installation> InstallationsFromProbe( const ProbedRoot &probe, const Candidate &candidate )
{
	std::vector<Installation> result;
	const std::string rootUtf8 = fs::ToUtf8( probe.root );
	const std::string identity = fs::IdentityKey( probe.root );
	for ( unsigned i = 0; i < Com_GameManagerProfileCount(); ++i ) {
		const comGameProfile_t *profile = Com_GameManagerProfileAt( i );
		if ( !profile || !( probe.markers & profile->installMarker ) ) continue;
		const std::string group = GroupKey( profile );
		auto existing = std::find_if( result.begin(), result.end(),
			[&group]( const Installation &installation ) { return installation.group == group; } );
		if ( existing != result.end() ) {
			existing->modes.push_back( profile );
			continue;
		}
		Installation installation;
		installation.id = identity + "|" + group;
		installation.root = probe.root;
		installation.rootUtf8 = rootUtf8;
		installation.group = group;
		installation.title = profile->titleId;
		installation.modes.push_back( profile );
		installation.markers = probe.markers;
		installation.store = candidate.store;
		installation.detail = candidate.detail;
		installation.storeTitle = candidate.storeTitle;
		installation.storeId = candidate.storeId;
		installation.manual = candidate.store == Store::Manual;
		installation.portable = candidate.store == Store::Portable;
		if ( profile->id == COM_GAME_Q3TEST ) installation.release = ReleaseVersion( probe.root );
		RefineStore( installation, candidate.path );
		result.push_back( std::move( installation ) );
	}
	return result;
}

Installation UnverifiedInstallation( const stdfs::path &folder, const std::string &group, bool missing )
{
	Installation installation;
	stdfs::path root = missing ? stdfs::path() : fs::Canonical( folder );
	if ( root.empty() ) root = folder.lexically_normal();
	installation.root = root;
	installation.rootUtf8 = fs::ToUtf8( root );
	installation.group = group;
	installation.id = fs::IdentityKey( root ) + "|" + group;
	installation.modes = GroupProfiles( group );
	if ( !installation.modes.empty() ) installation.title = installation.modes.front()->titleId;
	installation.store = Store::Manual;
	installation.verified = false;
	installation.manual = true;
	installation.missing = missing;
	return installation;
}

void RefineStore( Installation &installation, const stdfs::path &candidateFolder )
{
	if ( installation.store != Store::Local && installation.store != Store::Manual
		&& installation.store != Store::Retail && installation.store != Store::Portable ) return;
	std::vector<stdfs::path> folders = { installation.root };
	if ( !candidateFolder.empty() ) folders.push_back( candidateFolder );
	if ( installation.root.has_parent_path() ) folders.push_back( installation.root.parent_path() );
	for ( const stdfs::path &folder : folders ) {
		std::string name, id;
		if ( ReadGogInfo( folder, name, id ) ) {
			installation.store = Store::Gog;
			if ( !name.empty() ) installation.storeTitle = name;
			if ( !id.empty() ) installation.storeId = id;
			if ( installation.detail.empty() ) installation.detail = "GOG installation";
			return;
		}
	}
	for ( const stdfs::path &folder : folders ) {
		const stdfs::path installer = fs::FindChild( folder, "__Installer" );
		if ( !installer.empty() && fs::IsDirectory( installer ) ) {
			installation.store = Store::EaApp;
			if ( installation.detail.empty() ) installation.detail = "EA app installation";
			return;
		}
	}
}

void MergeInstallations( std::vector<Installation> &installations )
{
	std::map<std::string, std::size_t> index;
	std::vector<Installation> merged;
	merged.reserve( installations.size() );
	for ( Installation &installation : installations ) {
		const auto found = index.find( installation.id );
		if ( found == index.end() ) {
			index.emplace( installation.id, merged.size() );
			merged.push_back( std::move( installation ) );
			continue;
		}
		Installation &kept = merged[found->second];
		const bool keptVerified = kept.verified;
		if ( StoreRank( installation.store ) > StoreRank( kept.store ) ) {
			kept.store = installation.store;
			kept.detail = installation.detail;
			if ( !installation.storeTitle.empty() ) kept.storeTitle = installation.storeTitle;
			if ( !installation.storeId.empty() ) kept.storeId = installation.storeId;
		} else {
			if ( kept.storeTitle.empty() ) kept.storeTitle = installation.storeTitle;
			if ( kept.storeId.empty() ) kept.storeId = installation.storeId;
			if ( kept.detail.empty() ) kept.detail = installation.detail;
		}
		if ( installation.verified && !keptVerified ) {
			kept.modes = installation.modes;
			kept.root = installation.root;
			kept.rootUtf8 = installation.rootUtf8;
		}
		kept.verified = keptVerified || installation.verified;
		kept.manual = kept.manual || installation.manual;
		kept.portable = kept.portable || installation.portable;
		kept.missing = kept.missing && installation.missing;
		kept.markers |= installation.markers;
		if ( kept.release.empty() ) kept.release = installation.release;
	}
	std::stable_sort( merged.begin(), merged.end(), []( const Installation &a, const Installation &b ) {
		const int orderA = GroupOrder( a.group ), orderB = GroupOrder( b.group );
		if ( orderA != orderB ) return orderA < orderB;
		return fs::Lower( a.rootUtf8 ) < fs::Lower( b.rootUtf8 );
	} );
	installations = std::move( merged );
}

std::vector<stdfs::path> ManualProbeOrder( const stdfs::path &picked )
{
	std::vector<stdfs::path> order;
	if ( picked.empty() ) return order;
	stdfs::path folder = picked;
	if ( fs::IsRegularFile( folder ) ) folder = folder.parent_path();
	order.push_back( folder );
	const std::string leaf = fs::Lower( fs::ToUtf8( folder.filename() ) );
	const auto &names = BaseDirectoryNames();
	const bool baseDirectory = std::find( names.begin(), names.end(), leaf ) != names.end();
	if ( ( baseDirectory || leaf == "gamedata" || leaf == "game" ) && folder.has_parent_path()
		&& folder.parent_path() != folder ) {
		order.push_back( folder.parent_path() );
	}
	return order;
}

std::vector<std::string> FindMods( const Installation &installation,
	const comGameProfile_t *profile, std::size_t limit )
{
	std::vector<std::string> mods;
	if ( !profile || !profile->baseGame || installation.missing ) return mods;
	// Quake Live has no mod folders of its own; everything it loads arrives
	// through its base directory and Steam Workshop.
	if ( profile->titleId == COM_GAME_TITLE_QUAKELIVE ) return mods;
	const std::string base = fs::Lower( profile->baseGame );
	for ( const stdfs::path &folder : fs::Subdirectories( installation.root, 1024 ) ) {
		const std::string name = fs::ToUtf8( folder.filename() );
		const std::string lower = fs::Lower( name );
		if ( name.empty() || name[0] == '.' || lower == base ) continue;
		// Medal of Honor's expansions install their own base directories,
		// which run a different engine release, not a mod of this one.
		if ( profile->titleId == COM_GAME_TITLE_MOHAA && ( lower == "mainta" || lower == "maintt" ) ) continue;
		// Another catalogue profile's base directory is never a mod.
		const auto &names = BaseDirectoryNames();
		if ( std::find( names.begin(), names.end(), lower ) != names.end() ) continue;
		bool archive = false;
		try {
			std::error_code error;
			stdfs::directory_iterator it( folder, stdfs::directory_options::skip_permission_denied, error );
			const stdfs::directory_iterator end;
			for ( unsigned seen = 0; !error && it != end && seen < 4096 && !archive; it.increment( error ), ++seen ) {
				archive = HasExtension( it->path(), ".pk3" );
			}
		} catch ( const std::exception & ) {
		}
		if ( archive ) mods.push_back( name );
		if ( mods.size() >= limit ) break;
	}
	return mods;
}

std::string ModDisplayName( const std::string &folder )
{
	if ( fs::EqualsNoCase( folder, "missionpack" ) ) return "Team Arena";
	return folder;
}

bool ProfileStartsInThisBuild( const comGameProfile_t *profile )
{
	return ProfileUnavailableReason( profile ).empty();
}

std::string ProfileUnavailableReason( const comGameProfile_t *profile )
{
	if ( !profile ) return "This game is not part of fnTech3's catalogue.";
	if ( !Com_GameManagerCanActivate( profile ) ) {
#if defined( FNTECH3_Q3_ONLY )
		if ( profile->id != COM_GAME_QUAKE3 ) {
			return "This Quake III-only build of fnTech3 starts Quake III Arena alone.";
		}
#endif
		return "Support for this game is still being built. It starts only in developer builds of fnTech3.";
	}
	// Retail modules that exist only as Windows x86 libraries decide the host
	// before the engine ever starts: ask the engine's own module naming.
	if ( profile->titleId == COM_GAME_TITLE_QUAKELIVE ) {
#if !defined( _WIN32 )
		return "Quake Live's own menus and game code exist only for Windows. On this system fnTech3 can host its servers.";
#endif
	} else if ( !( profile->allowedModuleApis & COM_MODULE_API_QVM ) ) {
		char module[64];
		if ( !Com_GameManagerNativeModuleFilename( profile, "qagame", kHostArchitecture,
				kHostExtension, 0, module, sizeof( module ) ) ) {
			return "This game's own code exists only for 32-bit Windows, so it starts only in the Windows version of fnTech3.";
		}
	}
	return {};
}

const char *ProfileDevelopmentOption( const comGameProfile_t *profile )
{
	if ( !profile ) return "";
	switch ( profile->id ) {
	case COM_GAME_QUAKE3_IHV: return "q3-ihv-development";
	case COM_GAME_Q3TEST: return "q3test-development";
	case COM_GAME_WOLFET: return "wolfet-development";
	default: break;
	}
	switch ( profile->titleId ) {
	case COM_GAME_TITLE_RTCW: return "rtcw-development";
	case COM_GAME_TITLE_STVEF: return "stvef-development";
	case COM_GAME_TITLE_JKA: return "jka-development";
	case COM_GAME_TITLE_JK2: return "jk2-development";
	case COM_GAME_TITLE_ALICE: return "alice-development";
	case COM_GAME_TITLE_FAKK2: return "fakk2-development";
	case COM_GAME_TITLE_MOHAA: return "mohaa-development";
	default: return "";
	}
}

} // namespace fntech3::launcher
