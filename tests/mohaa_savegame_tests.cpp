/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Medal of Honor: Allied Assault's savegame byte rules
 * (code/compat/mohaa_savegame.hpp): names and folders, the .ssv header and
 * portal block, and the load list's text. The layout is fnTech3's own, as
 * 1.00's engine cannot be read; no retail file is read here. */

#include "../code/compat/mohaa_savegame.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK( condition ) do { if ( !( condition ) ) { \
	std::fprintf( stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition ); \
	++failures; } } while ( 0 )

using namespace fntech3::mohaa::savegame;

namespace {

void CheckFolders() {
	char folder[NameBytes];
	Folder( "unnamedsoldier.cfg", folder );
	CHECK( !std::strcmp( folder, "unnamedsoldier" ) );
	/* The extension is the last dot's, as COM_StripExtension cuts it. */
	Folder( "my.soldier.cfg", folder );
	CHECK( !std::strcmp( folder, "my.soldier" ) );
	/* A directory never reaches the folder. */
	Folder( "configs/unnamedsoldier.cfg", folder );
	CHECK( !std::strcmp( folder, "unnamedsoldier" ) );
	Folder( "a\\b\\pvt.cfg", folder );
	CHECK( !std::strcmp( folder, "pvt" ) );
	/* Nothing left, or something that could climb, is the default. */
	for ( const char *config : { "", ".cfg", "..", "...", "configs/", "c:" } ) {
		Folder( config, folder );
		CHECK( !std::strcmp( folder, DefaultFolder ) );
	}
	Folder( nullptr, folder );
	CHECK( !std::strcmp( folder, DefaultFolder ) );
	Folder( std::string( NameBytes, 'x' ).c_str(), folder );
	CHECK( !std::strcmp( folder, DefaultFolder ) );
	Folder( std::string( NameBytes - 1, 'x' ).c_str(), folder );
	CHECK( std::strlen( folder ) == NameBytes - 1 );

	char path[PathBytes];
	FolderPath( "", path );
	CHECK( !std::strcmp( path, "save/unnamedsoldier" ) );
	CHECK( ArchivePath( "", "m1l10003", ServerExtension, path ) );
	CHECK( !std::strcmp( path, "save/unnamedsoldier/m1l10003.ssv" ) );
	CHECK( ArchivePath( "pvt.cfg", CurrentName, PersistentExtension, path ) );
	CHECK( !std::strcmp( path, "save/pvt/current.spv" ) );
	/* Refusals leave nothing behind. */
	for ( const char *name : { "", "../x", "a/b", "a\\b", "c:x", ".." } ) {
		CHECK( !ArchivePath( "", name, LevelExtension, path ) && !path[0] );
	}
	CHECK( !ArchivePath( "", nullptr, LevelExtension, path ) );
	CHECK( !ArchivePath( "", "x", "", path ) );
	CHECK( !ArchivePath( "", "x", "../sav", path ) );
	CHECK( !ArchivePath( "", std::string( NameBytes, 'n' ).c_str(), LevelExtension, path ) );
	CHECK( ArchivePath( "", std::string( NameBytes - 1, 'n' ).c_str(), LevelExtension, path ) );
}

void CheckNames() {
	char prefix[NameBytes], name[NameBytes];
	AutosavePrefix( "m1l1", prefix );
	CHECK( !std::strcmp( prefix, "m1l1" ) );
	AutosavePrefix( "obj/obj_team1$start", prefix );
	CHECK( !std::strcmp( prefix, "obj_obj_team1_start" ) );
	/* Room is left for the four digits. */
	AutosavePrefix( std::string( 100, 'm' ).c_str(), prefix );
	CHECK( std::strlen( prefix ) == NameBytes - 5 );
	CHECK( AutosaveName( prefix, 9999, name ) && std::strlen( name ) == NameBytes - 1 );
	CHECK( AutosaveName( "m1l1", 3, name ) && !std::strcmp( name, "m1l10003" ) );
	CHECK( AutosaveName( "m2l1", 0, name ) && !std::strcmp( name, "m2l10000" ) );
	CHECK( !AutosaveName( "m1l1", AutosaveNames, name ) && !name[0] );
	CHECK( !AutosaveName( "m1l1", -1, name ) );
	CHECK( !AutosaveName( nullptr, 0, name ) );

	CHECK( IsQuickSave( "quick" ) );
	CHECK( IsQuickSave( "m1l1quick" ) );
	CHECK( !IsQuickSave( "quicksave" ) );
	CHECK( !IsQuickSave( "uick" ) );
	CHECK( !IsQuickSave( nullptr ) );

	CHECK( NameAllowed( "quick" ) );
	CHECK( NameAllowed( "my save" ) );
	CHECK( !NameAllowed( "" ) );
	CHECK( !NameAllowed( "a/b" ) );
	CHECK( !NameAllowed( "a\"b" ) );
	CHECK( !NameAllowed( "a;quit" ) );
	CHECK( !NameAllowed( std::string( NameBytes, 'n' ).c_str() ) );
}

void CheckComments() {
	char comment[FieldBytes];
	Comment( "Lighthouse", "", true, false, comment );
	CHECK( !std::strcmp( comment, "Lighthouse - Starting" ) );
	Comment( "Lighthouse", "Radar", false, false, comment );
	CHECK( !std::strcmp( comment, "Lighthouse - Radar" ) );
	Comment( "Lighthouse", nullptr, false, false, comment );
	CHECK( !std::strcmp( comment, "Lighthouse" ) );
	Comment( "Lighthouse", "", false, true, comment );
	CHECK( !std::strcmp( comment, "QuickSave - Lighthouse" ) );
	Comment( nullptr, nullptr, false, false, comment );
	CHECK( !comment[0] );
	/* Always terminated, however long the message. */
	Comment( std::string( 200, 'm' ).c_str(), "x", false, true, comment );
	CHECK( std::strlen( comment ) == FieldBytes - 1 );
}

void CheckHeader() {
	Header header;
	header.saveTime = 0x5f5e1000;
	header.mapTime = 3723000;
	std::snprintf( header.comment, sizeof( header.comment ), "%s", "Lighthouse - Starting" );
	std::snprintf( header.mapName, sizeof( header.mapName ), "%s", "m1l1" );
	std::snprintf( header.saveName, sizeof( header.saveName ), "%s", "m1l10000" );
	header.musicLoops = -1;
	header.musicOffset = 12345;

	std::uint8_t bytes[HeaderBytes];
	std::memset( bytes, 0xcc, sizeof( bytes ) );
	Encode( header, bytes );
	/* The version, little-endian: "\x01MNF". */
	CHECK( bytes[0] == 0x01 && bytes[1] == 0x4d && bytes[2] == 0x4e && bytes[3] == 0x46 );
	CHECK( bytes[HeaderMapTime] == 0xf8 && bytes[HeaderMapTime + 1] == 0xce && bytes[HeaderMapTime + 2] == 0x38 );
	CHECK( !std::memcmp( bytes + HeaderMap, "m1l1\0\0\0\0", 8 ) );
	/* Unused bytes of each field are zero, however the buffer began. */
	CHECK( bytes[HeaderComment + FieldBytes - 1] == 0 && bytes[HeaderMusic] == 0 );
	CHECK( bytes[HeaderMusicLoops] == 0xff && bytes[HeaderMusicLoops + 3] == 0xff );

	Header back;
	CHECK( Decode( bytes, sizeof( bytes ), back ) );
	CHECK( back.saveTime == header.saveTime && back.mapTime == header.mapTime );
	CHECK( !std::strcmp( back.comment, header.comment ) && !std::strcmp( back.mapName, "m1l1" ) );
	CHECK( !std::strcmp( back.saveName, "m1l10000" ) && !back.musicFile[0] );
	CHECK( back.musicLoops == -1 && back.musicOffset == 12345 );

	/* A field written full has no terminator in the file and reads whole. */
	std::memset( header.mapName, 'q', FieldBytes );
	header.mapName[FieldBytes] = '\0';
	Encode( header, bytes );
	CHECK( bytes[HeaderMap + FieldBytes - 1] == 'q' && bytes[HeaderName] == 'm' );
	CHECK( Decode( bytes, sizeof( bytes ), back ) && std::strlen( back.mapName ) == FieldBytes );

	/* Short files and other versions are refused. */
	CHECK( !Decode( bytes, HeaderBytes - 1, back ) );
	CHECK( !Decode( nullptr, HeaderBytes, back ) );
	bytes[0] = 0x02;
	CHECK( !Decode( bytes, sizeof( bytes ), back ) );
}

void CheckPortals() {
	std::size_t bytes = 0;
	CHECK( PortalBytes( 1, bytes ) && bytes == 8 );
	CHECK( PortalBytes( 3, bytes ) && bytes == 40 );
	CHECK( PortalBytes( MaxAreas, bytes ) && bytes == 4 + 4 * 256 * 256 );
	CHECK( !PortalBytes( 0, bytes ) && !bytes );
	CHECK( !PortalBytes( MaxAreas + 1, bytes ) );
	CHECK( !PortalBytes( -1, bytes ) );

	std::vector<std::uint8_t> file( HeaderBytes + 4 + 4 * 4 );
	WriteInt32( file.data() + HeaderBytes, 2 );
	for ( int i = 0; i < 4; ++i ) WriteInt32( file.data() + HeaderBytes + 4 + i * 4, i * 10 - 1 );
	std::int32_t counts[4] = {};
	int areas = -1;
	CHECK( ReadPortals( file.data(), file.size(), areas, counts, 4 ) );
	CHECK( areas == 2 && counts[0] == -1 && counts[3] == 29 );
	/* Too little room, a short block, no block and an impossible count. */
	CHECK( !ReadPortals( file.data(), file.size(), areas, counts, 3 ) && areas == 0 );
	CHECK( !ReadPortals( file.data(), file.size() - 1, areas, counts, 4 ) );
	CHECK( !ReadPortals( file.data(), HeaderBytes, areas, counts, 4 ) );
	WriteInt32( file.data() + HeaderBytes, MaxAreas + 1 );
	CHECK( !ReadPortals( file.data(), file.size(), areas, counts, 4 ) );
}

void CheckListText() {
	char text[32];
	ElapsedText( 0, text );
	CHECK( !std::strcmp( text, "0:00:00" ) );
	ElapsedText( 3723999, text );
	CHECK( !std::strcmp( text, "1:02:03" ) );
	ElapsedText( 59999, text );
	CHECK( !std::strcmp( text, "0:00:59" ) );
	ElapsedText( -5000, text );
	CHECK( !std::strcmp( text, "0:00:00" ) );
	ElapsedText( 0x7fffffff, text );
	CHECK( !std::strcmp( text, "596:31:23" ) );
}

} // namespace

int main() {
	CheckFolders();
	CheckNames();
	CheckComments();
	CheckHeader();
	CheckPortals();
	CheckListText();
	if ( failures ) {
		std::fprintf( stderr, "%d failure(s)\n", failures );
		return 1;
	}
	std::puts( "mohaa savegame tests passed" );
	return 0;
}
