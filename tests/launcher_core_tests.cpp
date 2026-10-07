// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
// The launcher's core without its interface: settings, Steam's files, the
// engine probe over synthetic installations, and the engine command line.
#include "../code/launcher/launcher_catalog.hpp"
#include "../code/launcher/launcher_discovery.hpp"
#include "../code/launcher/launcher_fs.hpp"
#include "../code/launcher/launcher_json.hpp"
#include "../code/launcher/launcher_launch.hpp"
#include "../code/launcher/launcher_settings.hpp"
#include "../code/launcher/launcher_vdf.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#define CHECK( expression ) do { if ( !( expression ) ) { \
	std::fprintf( stderr, "line %d: %s\n", __LINE__, #expression ); std::exit( 1 ); \
} } while ( 0 )

namespace stdfs = std::filesystem;
using namespace fntech3::launcher;

namespace {

void Put16( std::string &out, unsigned value )
{
	out += char( value & 0xff );
	out += char( ( value >> 8 ) & 0xff );
}

void Put32( std::string &out, std::uint32_t value )
{
	Put16( out, value & 0xffff );
	Put16( out, value >> 16 );
}

struct PakEntry {
	const char *name;
	std::uint32_t length;
	std::uint32_t crc;
};

// Synthetic ZIP directory metadata only, as tests/profile_marker_filesystem_
// tests.py builds it: names, sizes and CRCs, with no retail payload.
void WriteMetadataPak( const stdfs::path &path, const std::vector<PakEntry> &entries )
{
	std::string local, directory;
	for ( const PakEntry &entry : entries ) {
		const std::uint32_t offset = std::uint32_t( local.size() );
		const unsigned nameLength = unsigned( std::strlen( entry.name ) );
		Put32( local, 0x04034b50u );
		Put16( local, 20 ); Put16( local, 0 ); Put16( local, 0 ); Put16( local, 0 ); Put16( local, 0 );
		Put32( local, entry.crc ); Put32( local, 1 ); Put32( local, entry.length );
		Put16( local, nameLength ); Put16( local, 0 );
		local += entry.name;
		local += 'x';
		Put32( directory, 0x02014b50u );
		Put16( directory, 20 ); Put16( directory, 20 ); Put16( directory, 0 ); Put16( directory, 0 );
		Put16( directory, 0 ); Put16( directory, 0 );
		Put32( directory, entry.crc ); Put32( directory, 1 ); Put32( directory, entry.length );
		Put16( directory, nameLength ); Put16( directory, 0 ); Put16( directory, 0 ); Put16( directory, 0 );
		Put16( directory, 0 ); Put32( directory, 0 ); Put32( directory, offset );
		directory += entry.name;
	}
	std::string eocd;
	Put32( eocd, 0x06054b50u );
	Put16( eocd, 0 ); Put16( eocd, 0 );
	Put16( eocd, unsigned( entries.size() ) ); Put16( eocd, unsigned( entries.size() ) );
	Put32( eocd, std::uint32_t( directory.size() ) ); Put32( eocd, std::uint32_t( local.size() ) );
	Put16( eocd, 0 );
	stdfs::create_directories( path.parent_path() );
	std::ofstream file( path, std::ios::binary );
	file << local << directory << eocd;
}

void WriteText( const stdfs::path &path, const std::string &text )
{
	stdfs::create_directories( path.parent_path() );
	std::ofstream file( path, std::ios::binary );
	file << text;
}

void MakeQuake3( const stdfs::path &root )
{
	WriteMetadataPak( root / "baseq3" / "pak0.pk3", {
		{ "default.cfg", 1737, 0x9a18b2fdu }, { "maps/q3dm6.bsp", 4408212, 0x9fef9a4bu } } );
}

void MakeQuakeLive( const stdfs::path &root )
{
	WriteMetadataPak( root / "baseq3" / "pak00.pk3", {
		{ "default.cfg", 1580, 0xc6c9f233u }, { "maps/campgrounds.bsp", 2924532, 0x0ddc0f1du } } );
}

void MakeRtcw( const stdfs::path &root, bool singlePlayer, bool multiplayer )
{
	if ( singlePlayer ) {
		WriteMetadataPak( root / "main" / "pak0.pk3", {
			{ "default.cfg", 2237, 0x844222c3u }, { "maps/escape1.bsp", 12366352, 0x5f08b87cu } } );
	}
	if ( multiplayer ) {
		WriteMetadataPak( root / "main" / "mp_pak0.pk3", {
			{ "default.cfg", 1372, 0x91dfaca0u }, { "maps/mp_beach.bsp", 7343512, 0x77533d56u } } );
	}
}

void MakeJediAcademy( const stdfs::path &root )
{
	WriteMetadataPak( root / "base" / "assets2.pk3", {
		{ "default.cfg", 2178, 0xfb11a25bu }, { "mpdefault.cfg", 2845, 0x4999454au } } );
}

const Installation *FindGroup( const std::vector<Installation> &list, const char *group )
{
	for ( const Installation &installation : list ) {
		if ( installation.group == group ) return &installation;
	}
	return nullptr;
}

void TestJson()
{
	json::Value value;
	std::string error;
	CHECK( json::Parse( "{\"a\": [1, 2.5, -3e2, true, false, null], \"b\": {\"c\": \"d\\\\e\\\"f\\u00e9\\ud83d\\ude00\"}}",
		value, &error ) );
	CHECK( value.IsObject() && value.Find( "a" )->Items().size() == 6 );
	CHECK( value.Find( "a" )->Items()[1].AsNumber() == 2.5 );
	CHECK( value.Find( "a" )->Items()[2].AsNumber() == -300.0 );
	CHECK( value.Find( "b" )->Find( "c" )->AsString() == "d\\e\"f\xc3\xa9\xf0\x9f\x98\x80" );
	for ( const char *bad : { "", "{", "[1,]", "{\"a\":1,}", "01", "1.", "-", "\"\x01\"", "\"\\x\"",
			"\"\\ud800\"", "{\"a\" 1}", "[1] 2", "nul", "1e", "\"abc" } ) {
		CHECK( !json::Parse( bad, value, &error ) && value.IsNull() && !error.empty() );
	}
	std::string deep( 70, '[' );
	deep += std::string( 70, ']' );
	CHECK( !json::Parse( deep, value ) );
	std::string shallow( 60, '[' );
	shallow += std::string( 60, ']' );
	CHECK( json::Parse( shallow, value ) );

	json::Value document = json::Value::Object();
	document.Set( "path", json::Value::String( "C:\\Games\\Quake III Arena\n\x02" ) );
	document.Set( "time", json::Value::Number( 1787055256.0 ) );
	document.Set( "list", json::Value::Array() );
	document.Set( "path", json::Value::String( "replaced" ) );
	const std::string text = json::Serialize( document );
	CHECK( text.find( "1787055256" ) != std::string::npos && text.find( "e+" ) == std::string::npos );
	json::Value back;
	CHECK( json::Parse( text, back ) );
	CHECK( back.Members().size() == 3 && back.Find( "path" )->AsString() == "replaced" );
	document.Set( "path", json::Value::String( "C:\\Games\\Quake III Arena\n\x02" ) );
	CHECK( json::Parse( json::Serialize( document, false ), back ) );
	CHECK( back.Find( "path" )->AsString() == "C:\\Games\\Quake III Arena\n\x02" );
}

void TestVdf()
{
	vdf::Node root;
	CHECK( vdf::Parse( "// comment\n\"libraryfolders\"\n{\n \"0\"\n {\n  \"path\" \"C:\\\\Program Files (x86)\\\\Steam\"\n"
		"  \"apps\" { \"2200\" \"1\" }\n }\n \"1\" \"D:\\\\Old\\\\Library\"\n \"contentstatsid\" \"7\"\n}\n", root ) );
	const vdf::Node *folders = root.Find( "LibraryFolders" );
	CHECK( folders && folders->block && folders->children.size() == 3 );
	CHECK( folders->children[0].Get( "PATH" ) == "C:\\Program Files (x86)\\Steam" );
	CHECK( folders->children[0].Find( "apps" )->block );
	CHECK( folders->children[1].value == "D:\\Old\\Library" );
	CHECK( vdf::Parse( "\"a\" \"b\" [$WIN32]\n\"c\" { \"d\" \"e\" [$OSX] }", root ) && root.children.size() == 2 );
	CHECK( vdf::Parse( "\"unknown\" \"C:\\Steam\"", root ) && root.Get( "unknown" ) == "C:\\Steam" );
	for ( const char *bad : { "\"a\" {", "\"a\" }", "{", "\"a\"", "\"a\" \"b", "}" } ) {
		CHECK( !vdf::Parse( bad, root ) && root.children.empty() );
	}
	std::string deep;
	for ( int i = 0; i < 40; ++i ) deep += "\"k\" { ";
	for ( int i = 0; i < 40; ++i ) deep += "} ";
	CHECK( !vdf::Parse( deep, root ) );
}

void TestCatalogNames()
{
	const comGameProfile_t *rtcwSp = Com_GameManagerFindProfile( "rtcw-sp" );
	const comGameProfile_t *rtcwMp = Com_GameManagerFindProfile( "rtcw-mp" );
	const comGameProfile_t *quake3 = Com_GameManagerFindProfile( "quake3" );
	const comGameProfile_t *ihv = Com_GameManagerFindProfile( "q3-ihv" );
	const comGameProfile_t *alice = Com_GameManagerFindProfile( "alice" );
	const comGameProfile_t *holomatch = Com_GameManagerFindProfile( "stvef-mp" );
	CHECK( rtcwSp && rtcwMp && quake3 && ihv && alice && holomatch );
	CHECK( GroupKey( rtcwSp ) == "rtcw" && GroupKey( rtcwMp ) == "rtcw" );
	CHECK( GroupKey( quake3 ) == "quake3" && GroupKey( ihv ) == "q3-ihv" );
	CHECK( GroupProfiles( "rtcw" ).size() == 2 && GroupProfiles( "quake3" ).size() == 1 );
	CHECK( GroupName( "rtcw" ) == "Return to Castle Wolfenstein" );
	CHECK( GroupName( "q3-ihv" ) == "Quake III Arena (IHV Test)" );
	CHECK( ModeLabel( rtcwSp ) == "Single Player" && ModeLabel( rtcwMp ) == "Multiplayer" );
	CHECK( ModeLabel( holomatch ) == "Holomatch" );
	CHECK( ModeLabel( quake3 ).empty() && ModeLabel( alice ).empty() && ModeLabel( ihv ).empty() );
	CHECK( GroupOrder( "quake3" ) < GroupOrder( "q3-ihv" ) && GroupOrder( "q3-ihv" ) < GroupOrder( "quakelive" ) );
	CHECK( GroupOrder( "quakelive" ) < GroupOrder( "rtcw" ) && GroupOrder( "rtcw" ) < GroupOrder( "mohaa" ) );
	const auto &bases = BaseDirectoryNames();
	for ( const char *base : { "baseq3", "main", "etmain", "baseef", "base", "fakk", "demoq3" } ) {
		CHECK( std::find( bases.begin(), bases.end(), base ) != bases.end() );
	}
	for ( unsigned i = 0; i < Com_GameManagerProfileCount(); ++i ) {
		const comGameProfile_t *profile = Com_GameManagerProfileAt( i );
		if ( !Com_GameManagerCanActivate( profile ) ) {
			CHECK( !ProfileStartsInThisBuild( profile ) && !ProfileUnavailableReason( profile ).empty() );
			CHECK( profile->id == COM_GAME_QUAKE3 || profile->id == COM_GAME_QUAKELIVE
				|| *ProfileDevelopmentOption( profile ) );
		}
	}
	static_assert( Store( 0 ) == Store::Local, "Store keys start at Local" );
	for ( unsigned i = 0; i < unsigned( Store::Count ); ++i ) {
		Store parsed = Store::Count;
		CHECK( StoreFromKey( StoreKey( Store( i ) ), parsed ) && parsed == Store( i ) );
		CHECK( *StoreName( Store( i ) ) );
	}
	Store unknown = Store::Steam;
	CHECK( !StoreFromKey( "itch", unknown ) && unknown == Store::Steam );
	CHECK( StoreRank( Store::Steam ) > StoreRank( Store::Gog ) && StoreRank( Store::Gog ) > StoreRank( Store::Retail ) );
	CHECK( StoreRank( Store::Retail ) > StoreRank( Store::Manual ) && StoreRank( Store::Manual ) > StoreRank( Store::Local ) );
	CHECK( ModDisplayName( "missionpack" ) == "Team Arena" && ModDisplayName( "cpma" ) == "cpma" );
}

void TestProbe( const stdfs::path &scratch )
{
	const stdfs::path games = scratch / "Games";
	const stdfs::path q3 = games / "Quake III Arena";
	MakeQuake3( q3 );
	stdfs::create_directories( q3 / "missionpack" );
	WriteText( q3 / "missionpack" / "pak0.pk3", "not read" );
	stdfs::create_directories( q3 / "emptymod" );
	stdfs::create_directories( q3 / ".hidden" );
	WriteText( q3 / ".hidden" / "x.pk3", "" );
	const stdfs::path rtcw = games / "Return to Castle Wolfenstein";
	MakeRtcw( rtcw, true, true );
	const stdfs::path jka = games / "Jedi Academy";
	MakeJediAcademy( jka / "GameData" );
	const stdfs::path junk = games / "Not a game";
	stdfs::create_directories( junk / "baseq3" );
	WriteText( junk / "baseq3" / "pak0.pk3", "PK\x03\x04 not a real archive" );
	stdfs::create_directories( games / "Empty" );

	CHECK( MightHoldGame( q3 ) && MightHoldGame( rtcw ) && MightHoldGame( junk ) );
	CHECK( !MightHoldGame( games / "Empty" ) && !MightHoldGame( jka ) && !MightHoldGame( scratch / "missing" ) );
	CHECK( ProbeFolder( junk ).empty() && ProbeFolder( games / "Empty" ).empty() );

	const auto q3Probe = ProbeFolder( q3 );
	CHECK( q3Probe.size() == 1 && q3Probe[0].markers == COM_GAME_MARKER_QUAKE3 );
	const auto rtcwProbe = ProbeFolder( rtcw );
	CHECK( rtcwProbe.size() == 1
		&& rtcwProbe[0].markers == ( COM_GAME_MARKER_RTCW_SP | COM_GAME_MARKER_RTCW_MP ) );
	const auto jkaProbe = ProbeFolder( jka );
	CHECK( jkaProbe.size() == 1 && fs::EqualsNoCase( fs::ToUtf8( jkaProbe[0].root.filename() ), "GameData" ) );

	Candidate candidate;
	candidate.path = rtcw;
	candidate.store = Store::Local;
	candidate.detail = "test";
	auto installations = InstallationsFromProbe( rtcwProbe[0], candidate );
	CHECK( installations.size() == 1 && installations[0].group == "rtcw" && installations[0].modes.size() == 2 );
	CHECK( installations[0].modes[0]->id == COM_GAME_RTCW_SP && installations[0].modes[1]->id == COM_GAME_RTCW_MP );
	CHECK( installations[0].verified && !installations[0].manual && installations[0].store == Store::Local );
	CHECK( installations[0].id == fs::IdentityKey( rtcwProbe[0].root ) + "|rtcw" );

	// A GOG installation names itself in a goggame-*.info file, here in the
	// folder above Jedi Academy's GameData as GOG lays it out.
	WriteText( jka / "goggame-1428935002.info", "{\"gameId\": \"1428935002\", \"name\": \"Star Wars Jedi Knight: Jedi Academy\"}" );
	candidate.path = jka;
	installations = InstallationsFromProbe( jkaProbe[0], candidate );
	CHECK( installations.size() == 1 && installations[0].group == "jka" && installations[0].modes.size() == 2 );
	CHECK( installations[0].store == Store::Gog && installations[0].storeId == "1428935002" );
	CHECK( installations[0].storeTitle == "Star Wars Jedi Knight: Jedi Academy" );

	candidate.path = q3;
	const auto mods = FindMods( InstallationsFromProbe( q3Probe[0], candidate )[0],
		Com_GameManagerFindProfile( "quake3" ) );
	CHECK( mods.size() == 1 && mods[0] == "missionpack" );

	// Manual picks of a base directory or a nested root reach the installation.
	auto order = ManualProbeOrder( q3 / "baseq3" );
	CHECK( order.size() == 2 && order[1] == q3 );
	order = ManualProbeOrder( q3 / "baseq3" / "pak0.pk3" );
	CHECK( order.size() == 2 && order[0] == q3 / "baseq3" && order[1] == q3 );
	CHECK( ManualProbeOrder( q3 ).size() == 1 );
	auto manual = ProbeManualFolder( q3 / "baseq3" );
	CHECK( manual.size() == 1 && manual[0].group == "quake3" && manual[0].manual && manual[0].store == Store::Manual );
	CHECK( ProbeManualFolder( junk ).empty() );

	// The same folder found by two sources is one row named by the better one.
	std::vector<Installation> merged = InstallationsFromProbe( q3Probe[0], candidate );
	candidate.store = Store::Steam;
	candidate.storeTitle = "Quake III Arena";
	candidate.storeId = "2200";
	for ( Installation &installation : InstallationsFromProbe( q3Probe[0], candidate ) ) merged.push_back( installation );
	merged.push_back( manual[0] );
	merged.push_back( UnverifiedInstallation( junk, "rtcw", false ) );
	merged.push_back( UnverifiedInstallation( scratch / "gone", "quake3", true ) );
	MergeInstallations( merged );
	CHECK( merged.size() == 3 );
	const Installation *quake = FindGroup( merged, "quake3" );
	CHECK( quake && quake->store == Store::Steam && quake->manual && quake->verified && quake->storeId == "2200" );
	CHECK( merged[0].group == "quake3" && merged[2].group == "rtcw" );
	CHECK( !merged[2].verified && merged[2].manual && !merged[2].missing && merged[2].modes.size() == 2 );
	CHECK( merged[1].missing && !merged[1].verified );

	DiscoveryOptions options;
	options.scanStores = false;
	options.scanCommonFolders = false;
	options.searchFolders = { games };
	options.manual = { { junk, "rtcw" }, { scratch / "unplugged", "quakelive" }, { scratch / "unplugged2", "" } };
	DiscoveryProgress progress;
	const DiscoveryResult result = Discover( options, nullptr, &progress );
	CHECK( !result.cancelled && result.foldersProbed > 0 );
	CHECK( result.installations.size() == 5 );
	CHECK( progress.found == 5 && progress.probed == progress.candidates );
	CHECK( FindGroup( result.installations, "quake3" ) && FindGroup( result.installations, "jka" ) );
	CHECK( FindGroup( result.installations, "quakelive" )->missing );
	const std::atomic<bool> cancel{ true };
	CHECK( Discover( options, &cancel ).cancelled );

	// A portable copy of fnTech3 inside a game folder names that game.
	const stdfs::path ql = scratch / "Quake Live";
	MakeQuakeLive( ql );
	options = DiscoveryOptions();
	options.scanStores = false;
	options.scanCommonFolders = false;
	options.launcherDirectory = ql / "fnTech3";
	stdfs::create_directories( options.launcherDirectory );
	const DiscoveryResult portable = Discover( options );
	CHECK( portable.installations.size() == 1 && portable.installations[0].portable );
	CHECK( portable.installations[0].store == Store::Portable && portable.installations[0].group == "quakelive" );
}

// Steam writes a path's backslashes doubled.
std::string VdfPath( const stdfs::path &path )
{
	std::string escaped;
	for ( const char c : fs::ToUtf8( path ) ) {
		if ( c == '\\' ) escaped += '\\';
		escaped += c;
	}
	return escaped;
}

void TestSteam( const stdfs::path &scratch )
{
	const stdfs::path steam = scratch / "Steam";
	const stdfs::path second = scratch / "Library Two";
	WriteText( steam / "steamapps" / "libraryfolders.vdf",
		"\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + VdfPath( steam ) + "\"\n\t}\n"
		"\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" + VdfPath( second ) + "\"\n\t}\n"
		"\t\"2\"\n\t{\n\t\t\"path\"\t\t\"" + VdfPath( scratch / "absent" ) + "\"\n\t}\n}\n" );
	stdfs::create_directories( second / "steamapps" );
	WriteText( second / "steamapps" / "appmanifest_9010.acf",
		"\"AppState\"\n{\n\t\"appid\"\t\t\"9010\"\n\t\"name\"\t\t\"Return to Castle Wolfenstein\"\n"
		"\t\"installdir\"\t\t\"Return to Castle Wolfenstein\"\n\t\"UserConfig\" { \"language\" \"english\" }\n}\n" );
	WriteText( second / "steamapps" / "appmanifest_666.acf",
		"\"AppState\" { \"appid\" \"666\" \"name\" \"Escape\" \"installdir\" \"..\" }" );
	WriteText( second / "steamapps" / "appmanifest_667.acf", "\"AppState\" { \"appid\" \"667\"" );
	WriteText( second / "steamapps" / "appmanifest_668.acf.tmp", "ignored" );
	MakeRtcw( second / "steamapps" / "common" / "Return to Castle Wolfenstein", false, true );

	const auto libraries = SteamLibraries( steam );
	CHECK( libraries.size() == 2 );
	CHECK( SteamApps( libraries[0] ).empty() );
	const auto apps = SteamApps( libraries[1] );
	CHECK( apps.size() == 1 && apps[0].appId == "9010" && apps[0].name == "Return to Castle Wolfenstein" );
	CHECK( fs::ToUtf8( apps[0].folder.filename() ) == "Return to Castle Wolfenstein" );
	CHECK( SteamLibraries( scratch / "no steam here" ).empty() );

	CHECK( ParseHeroicInstalled( "{\"installed\": [{\"install_path\": \"/games/Quake III Arena\", \"title\": \"Quake III\"},"
		" {\"install_path\": \"\"}, {\"platform\": \"windows\"}]}" ).size() == 1 );
	CHECK( ParseHeroicInstalled( "[{\"install_path\": \"/a\"}, {\"install_path\": \"/b\"}]" ).size() == 2 );
	CHECK( ParseHeroicInstalled( "not json" ).empty() );
	const auto lutris = ParseLutrisPaths( "game:\n  exe: /home/p/Games/q3/drive_c/Quake III Arena/quake3.exe\n"
		"  prefix: '/home/p/Games/q3'\n  args: +set fs_game cpma\nsystem:\n  game_path: relative/path\n" );
	CHECK( lutris.size() == 2 && lutris[0] == stdfs::path( "/home/p/Games/q3/drive_c/Quake III Arena" ) );
	CHECK( lutris[1] == stdfs::path( "/home/p/Games/q3" ) );

	const stdfs::path prefix = scratch / "prefix";
	stdfs::create_directories( prefix / "drive_c" / "Program Files (x86)" );
	stdfs::create_directories( prefix / "drive_c" / "GOG Games" );
	CHECK( WinePrefixGameFolders( prefix ).size() == 2 );
	CHECK( WinePrefixGameFolders( scratch ).empty() );
}

void TestArguments()
{
	std::vector<std::string> tokens;
	std::string error;
	CHECK( SplitArguments( "  +set fs_game cpma +connect \"my server\" \"\" a\"b c\"  ", tokens, &error ) );
	CHECK( tokens.size() == 8 && tokens[3] == "+connect" && tokens[4] == "my server" && tokens[5].empty() );
	CHECK( tokens[6] == "a" && tokens[7] == "b c" );
	CHECK( SplitArguments( "a\"b c\"", tokens ) && tokens.size() == 2 && tokens[0] == "a" && tokens[1] == "b c" );
	CHECK( !SplitArguments( "+set name \"unclosed", tokens, &error ) && tokens.empty() && !error.empty() );
	CHECK( !SplitArguments( "+set name \x01", tokens, &error ) );
	CHECK( !SplitArguments( "\"a\x02\"", tokens, &error ) );
	CHECK( SplitArguments( "", tokens ) && tokens.empty() );

	CHECK( JoinCommandLine( { "+set", "fs_basepath", "C:\\Games\\Quake III Arena" } )
		== "+set fs_basepath \"C:\\Games\\Quake III Arena\"" );
	CHECK( JoinCommandLine( { "a;b", "x+y", "+map", "" } ) == "\"a;b\" \"x+y\" +map \"\"" );
	CHECK( JoinCommandLine( { "\"already quoted\"" } ) == "\"already quoted\"" );

	LaunchRequest request;
	request.profile = Com_GameManagerFindProfile( "quake3" );
	request.basePath = "C:\\Games\\Quake III Arena\\";
	request.mod = "missionpack";
	request.arguments = "+set sv_dlURL http://example.com/maps +set r_fullscreen 0";
	std::vector<std::string> arguments;
	CHECK( BuildArguments( request, arguments, &error ) );
	CHECK( arguments.size() == 15 && arguments[2] == "quake3" && arguments[5] == "C:\\Games\\Quake III Arena" );
	CHECK( arguments[8] == "missionpack" && arguments[11] == "\"http://example.com/maps\"" );
	CHECK( JoinCommandLine( arguments ) == "+set com_gameProfile quake3 +set fs_basepath \"C:\\Games\\Quake III Arena\" "
		"+set fs_game missionpack +set sv_dlURL \"http://example.com/maps\" +set r_fullscreen 0" );
	request.basePath = "D:\\";
	request.mod.clear();
	request.arguments.clear();
	CHECK( BuildArguments( request, arguments ) && arguments.size() == 6 && arguments[5] == "D:\\" );
	request.basePath = "/";
	CHECK( BuildArguments( request, arguments ) && arguments[5] == "/" );

	request.basePath = "C:\\Games\\\"Quake\"";
	CHECK( !BuildArguments( request, arguments, &error ) && arguments.empty() && !error.empty() );
	request.basePath = "C:\\Games\\Q3";
	for ( const char *mod : { "../x", "a/b", "a\\b", ".", "..", "c:d", "q\"3" } ) {
		request.mod = mod;
		CHECK( !BuildArguments( request, arguments ) );
	}
	request.mod.clear();
	request.arguments = "+set a \"open";
	CHECK( !BuildArguments( request, arguments ) );
	std::string many;
	for ( int i = 0; i < 29; ++i ) many += "+wait ";
	request.arguments = many;
	CHECK( BuildArguments( request, arguments ) );
	request.arguments = many + "+wait +wait";
	CHECK( !BuildArguments( request, arguments, &error ) && error.find( "32" ) != std::string::npos );
	request.arguments = "+set long \"" + std::string( 1000, 'x' ) + "\"";
	CHECK( !BuildArguments( request, arguments ) );
	request.profile = nullptr;
	request.arguments.clear();
	CHECK( !BuildArguments( request, arguments ) );

#ifdef _WIN32
	CHECK( DisplayCommand( "C:\\fnTech3\\fntech3.exe", { "+set", "fs_basepath", "C:\\A B" } )
		== "\"C:\\fnTech3\\fntech3.exe\" +set fs_basepath \"C:\\A B\"" );
	CHECK( fs::ToUtf8( DefaultEnginePath( "C:\\fnTech3", false ).filename() ) == "fntech3.exe" );
#else
	CHECK( DisplayCommand( "/opt/fn tech3/fntech3", { "+set", "fs_basepath", "/a b/it's" } )
		== "'/opt/fn tech3/fntech3' +set fs_basepath '/a b/it'\\''s'" );
	CHECK( fs::ToUtf8( DefaultEnginePath( "/opt/fntech3", true ).filename() ) == "fntech3.ded" );
#endif
}

void TestProcess( const stdfs::path &scratch )
{
	GameProcess missing;
	std::string error;
	CHECK( !missing.Start( scratch / "no-engine-here", {}, &error ) && !error.empty() && !missing.Running() );
#ifdef _WIN32
	std::string shell = fs::Environment( "ComSpec" );
	if ( shell.empty() ) shell = "C:\\Windows\\System32\\cmd.exe";
	const std::vector<std::string> arguments = { "/c", "exit 3" };
#else
	const std::string shell = "/bin/sh";
	const std::vector<std::string> arguments = { "-c", "exit 3" };
#endif
	GameProcess process;
	CHECK( process.Start( fs::FromUtf8( shell ), arguments, &error ) );
	CHECK( process.Started() );
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 20 );
	while ( process.Running() && std::chrono::steady_clock::now() < deadline ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
	}
	CHECK( !process.Running() && process.ExitCode() == 3 );
	CHECK( !process.Start( fs::FromUtf8( shell ), arguments, &error ) );
}

void TestSettings( const stdfs::path &scratch )
{
	Settings settings;
	settings.theme = Theme::Light;
	settings.afterLaunch = AfterLaunch::Minimize;
	settings.sort = SortOrder::RecentlyPlayed;
	settings.showUnavailable = false;
	settings.unavailableCollapsed = true;
	settings.enginePath = "C:\\dev\\fntech3.exe";
	settings.searchFolders = { "D:\\Games", "/home/p/more games" };
	settings.manual = { { "E:\\Games\\Quake III Arena", "quake3", 1787055256 }, { "/x", "", 0 } };
	InstallationPrefs prefs;
	prefs.name = "Clan night";
	prefs.mode = "rtcw-mp";
	prefs.mod = "osp";
	prefs.arguments = "+set name \"Player One\"";
	prefs.hidden = true;
	prefs.lastPlayed = 1787000000;
	prefs.playCount = 12;
	settings.installations["c:\\games\\rtcw|rtcw"] = prefs;
	settings.installations["empty"] = InstallationPrefs();
	settings.window = { 100, -20, 1100, 700, true };
	Installation cached = UnverifiedInstallation( scratch / "cached", "rtcw", true );
	cached.store = Store::Steam;
	cached.storeId = "9010";
	cached.modes.pop_back();
	settings.cache.push_back( cached );

	const stdfs::path file = scratch / "settings" / "launcher.json";
	std::string error;
	CHECK( SaveSettings( file, settings, &error ) );
	Settings loaded;
	CHECK( LoadSettings( file, loaded, &error ) && error.empty() );
	CHECK( loaded.theme == Theme::Light && loaded.afterLaunch == AfterLaunch::Minimize );
	CHECK( loaded.sort == SortOrder::RecentlyPlayed && !loaded.showUnavailable && loaded.unavailableCollapsed );
	CHECK( loaded.enginePath == settings.enginePath && loaded.searchFolders == settings.searchFolders );
	CHECK( loaded.manual.size() == 2 && loaded.manual[0].group == "quake3" && loaded.manual[0].added == 1787055256 );
	CHECK( loaded.installations.size() == 1 );
	const InstallationPrefs &read = loaded.installations["c:\\games\\rtcw|rtcw"];
	CHECK( read.name == "Clan night" && read.mode == "rtcw-mp" && read.mod == "osp" && read.hidden );
	CHECK( read.arguments == prefs.arguments && read.lastPlayed == 1787000000 && read.playCount == 12 );
	CHECK( loaded.window.x == 100 && loaded.window.y == -20 && loaded.window.width == 1100 && loaded.window.maximized );
	CHECK( loaded.cache.size() == 1 && loaded.cache[0].store == Store::Steam && loaded.cache[0].storeId == "9010" );
	CHECK( loaded.cache[0].modes.size() == 1 && loaded.cache[0].modes[0]->id == COM_GAME_RTCW_SP );
	CHECK( loaded.cache[0].missing && !loaded.cache[0].verified && loaded.cache[0].id == cached.id );

	// Unknown values fall back, unknown groups are dropped, nothing crashes.
	CHECK( ParseSettings( "{\"version\": 99, \"theme\": \"neon\", \"manual\": [{\"path\": \"/a\", \"group\": \"doom\"},"
		" {\"group\": \"quake3\"}, 7], \"installations\": {\"x\": 5}, \"window\": {\"width\": -5},"
		" \"cache\": [{\"root\": \"/r\", \"group\": \"quake3\", \"modes\": [\"rtcw-sp\"]}]}", loaded, &error ) );
	CHECK( !error.empty() && loaded.theme == Theme::System && loaded.manual.size() == 1 && loaded.manual[0].group.empty() );
	CHECK( loaded.installations.empty() && loaded.window.width == 0 && loaded.cache.empty() );

	WriteText( file, "{ broken" );
	CHECK( !LoadSettings( file, loaded, &error ) && !error.empty() && loaded.manual.empty() );
	stdfs::path backup = file;
	backup += ".bad";
	CHECK( fs::IsRegularFile( backup ) );
	CHECK( LoadSettings( scratch / "absent.json", loaded, &error ) && error.empty() );
	// An empty file is a portable copy's request for defaults, not damage.
	WriteText( scratch / "empty" / "fntech3-launcher.json", " \r\n" );
	CHECK( LoadSettings( scratch / "empty" / "fntech3-launcher.json", loaded, &error ) && error.empty() );
	CHECK( !fs::IsRegularFile( scratch / "empty" / "fntech3-launcher.json.bad" ) );

	const stdfs::path portable = scratch / "portable";
	stdfs::create_directories( portable );
	CHECK( SettingsFile( scratch / "prefs", portable ) == scratch / "prefs" / "launcher.json" );
	WriteText( portable / "fntech3-launcher.json", "{}" );
	CHECK( SettingsFile( scratch / "prefs", portable ) == portable / "fntech3-launcher.json" );
}

void TestFilesystem( const stdfs::path &scratch )
{
	const stdfs::path folder = scratch / "Mixed Case";
	stdfs::create_directories( folder / "BaseQ3" );
	CHECK( !fs::FindChild( folder, "baseq3" ).empty() );
	CHECK( fs::FindChild( folder, "missing" ).empty() && fs::FindChild( {}, "x" ).empty() );
	CHECK( fs::Canonical( folder / "BaseQ3" / ".." ) == fs::Canonical( folder ) );
	CHECK( fs::Canonical( scratch / "nope" ).empty() );
	CHECK( fs::IdentityKey( fs::Canonical( folder ) ) == fs::IdentityKey( fs::Canonical( folder / "." ) ) );
	CHECK( fs::Lower( "AbC\xc3\x89" ) == "abc\xc3\x89" && fs::EqualsNoCase( "BaseQ3", "baseq3" ) );
	CHECK( fs::ContainsNoCase( "Program Files (x86)", "files" ) && !fs::ContainsNoCase( "abc", "abd" ) );
	CHECK( fs::FromUtf8( "caf\xc3\xa9" ).filename() == fs::FromUtf8( "caf\xc3\xa9" ) );
	CHECK( fs::ToUtf8( fs::FromUtf8( "caf\xc3\xa9" ) ) == "caf\xc3\xa9" );
	std::string text;
	CHECK( fs::WriteFileAtomic( scratch / "w" / "file.txt", "one" ) && fs::ReadFile( scratch / "w" / "file.txt", text, 16 ) );
	CHECK( text == "one" && fs::WriteFileAtomic( scratch / "w" / "file.txt", "two" ) );
	CHECK( fs::ReadFile( scratch / "w" / "file.txt", text, 16 ) && text == "two" );
	CHECK( !fs::ReadFile( scratch / "w" / "file.txt", text, 2 ) && text.empty() );
	CHECK( !fs::ExecutableDirectory().empty() );
}

} // namespace

int main()
{
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const stdfs::path scratch = stdfs::temp_directory_path() / ( "fntech3-launcher-tests-" + std::to_string( stamp ) );
	stdfs::create_directories( scratch );
	TestJson();
	TestVdf();
	TestCatalogNames();
	TestFilesystem( scratch );
	TestArguments();
	TestSettings( scratch );
	TestSteam( scratch );
	TestProbe( scratch );
	TestProcess( scratch );
	std::error_code error;
	stdfs::remove_all( scratch, error );
	std::printf( "launcher core tests passed\n" );
	return 0;
}
