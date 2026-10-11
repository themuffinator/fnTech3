/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_PICKER_HPP
#define FNTECH3_COMPAT_ALICE_PICKER_HPP

/* The rules of the file pickers Ritual's engine UI opens for its developer
 * menus, as alice.exe (and fakk2.exe 1.02, which has the same five classes)
 * keeps them: FilePickerClass (0x00417fd0) and its subclasses MapRunnerClass
 * (0x0041c1c0, `maplist`), ViewSpawnerClass (0x0041c930, `viewspawnlist`) and
 * SoundPickerClass (0x0041d830, `soundpicker`). A picker lists one folder of
 * the game's files -- its parent, its folders, then its files -- and hands on
 * what is chosen as a console command or a cvar. The window, the list and the
 * input are the hosted UI's (code/client/cl_alice_ui.cpp); this header holds
 * what can be tested on its own. Read from the retail binaries; nothing from
 * a reconstruction is incorporated. */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

namespace fntech3 {
namespace alice {
namespace picker {

enum class Kind { MapRunner, ViewSpawner, SoundPicker };

/* A row: its three cells -- File Name, Date, Size (0x004183ba..0x00418415) --
 * and what it is. */
enum class RowType { Parent, Folder, File };
struct Row {
	std::string cells[3];
	RowType type = RowType::File;
	std::string name;   // the folder's or file's own name, without brackets
};

/* The three columns: titles, widths in 640x480 units and the numeric one. */
constexpr const char *ColumnTitles[3] = { "File Name", "Date", "Size" };
constexpr float ColumnWidths[3] = { 175.0f, 125.0f, 100.0f };
constexpr bool ColumnNumeric[3] = { false, false, true };

/* The window (0x004182dc..0x00418335) and its parts (0x004c2170). */
constexpr float WindowX = 100.0f, WindowY = 50.0f, WindowW = 400.0f, WindowH = 300.0f;
constexpr float TitleHeight = 20.0f;      // 0x00500388
constexpr float TitleGrab = 18.0f;        // a press at or above it drags (0x004c293c)
constexpr float ButtonSize = 16.0f;
constexpr float CloseRight = 18.0f, MinimizeRight = 36.0f;   // 0x004ff9bc, 0x0050063c
constexpr float ClientX = 2.0f, ClientY = 20.0f;             // the child space
constexpr float ClientShrinkW = 4.0f, ClientShrinkH = 22.0f; // 0x004ffa88, 0x00500638
constexpr float HeaderPad = 2.0f;         // 0x00aa7064
constexpr float EdgeGrab = 2.0f;          // a header press this near an edge resizes
constexpr float MaxColumnWidth = 600.0f;  // 0x004ceecc..0x004ceee2
constexpr int DoubleClickMs = 500;        // 0x004ceb7f..0x004cebe0
constexpr float DoubleClickSlop = 2.0f;   // 0x00500640

/* Setup's rule (0x004186b3..0x004186f5): a folder longer than one character
 * that does not end in '/' gets one. */
inline std::string WithSlash( std::string folder ) {
	if ( folder.size() > 1 && folder.back() != '/' ) folder += '/';
	return folder;
}

/* GotoParentDirectory (0x00418840): nothing at the root, compared exactly;
 * otherwise cut after the '/' before the last one. */
inline std::string ParentOf( const std::string &current, const std::string &root ) {
	if ( current == root || current.size() < 2 ) return current;
	for ( std::size_t i = current.size() - 2; i > 0; --i ) {
		if ( current[i] == '/' ) return current.substr( 0, i + 1 );
	}
	return current;
}

/* The folder a sound picker opens in (the constructor 0x0041d900): the
 * folder of ui_pickedsound up to its last '/', else the root. Retail takes a
 * value outside the root as it is (0x0041dadd..0x0041db3a); here such a
 * value, one with "..", or none opens the root. */
inline std::string StartFolder( const std::string &picked, const std::string &root ) {
	const std::size_t slash = picked.rfind( '/' );
	if ( picked.empty() || slash == std::string::npos || picked.find( ".." ) != std::string::npos ) return root;
	const std::string folder = picked.substr( 0, slash + 1 );
	if ( folder.size() < root.size() ) return root;
	for ( std::size_t i = 0; i < root.size(); ++i ) {
		const char a = static_cast<char>( std::tolower( static_cast<unsigned char>( folder[i] ) ) );
		const char b = static_cast<char>( std::tolower( static_cast<unsigned char>( root[i] ) ) );
		if ( a != b ) return root;
	}
	return folder;
}

/* ViewSpawnerClass::FileChosen (0x0041c9f0): the full path, unquoted. */
inline std::string ViewSpawnCommand( const std::string &full ) {
	return "viewspawn " + full + "\n";
}

/* MapRunnerClass::FileChosen (0x0041c280): the path without `maps/` and its
 * four-letter extension. A name with nothing between the two runs nothing,
 * where retail would run `map` with an empty name. */
inline std::string MapCommand( const std::string &full ) {
	if ( full.size() <= 9 ) return std::string();
	return "map " + full.substr( 5, full.size() - 9 ) + "\n";
}

/* FileInfo's date (0x0043ec00, "%2d/%02d/%04d %2d:%02d%c" at 0x00512b8c):
 * month, day, year, the hour as (hour + 12) % 12 -- noon and midnight print
 * 0 -- the minute, and `a` before noon or `p` after it. */
inline std::string DateText( const std::tm &when ) {
	char text[32];
	std::snprintf( text, sizeof( text ), "%2d/%02d/%04d %2d:%02d%c", when.tm_mon + 1, when.tm_mday,
		when.tm_year + 1900, ( when.tm_hour + 12 ) % 12, when.tm_min, when.tm_hour < 12 ? 'a' : 'p' );
	return text;
}

inline bool SameName( const std::string &a, const std::string &b ) {
	if ( a.size() != b.size() ) return false;
	for ( std::size_t i = 0; i < a.size(); ++i ) {
		if ( std::tolower( static_cast<unsigned char>( a[i] ) ) != std::tolower( static_cast<unsigned char>( b[i] ) ) ) {
			return false;
		}
	}
	return true;
}

/* The names a folder listing gives that a row is made of: one trailing '/'
 * off a folder's name; nothing empty, nothing starting '.' (SetupFiles skips
 * those, 0x00418cff) and nothing still deeper than this folder; each name
 * once. Retail's own pak listing drops a folder's direct files and garbles
 * deeper names (0x0043cd98); this engine's listing is used instead. */
inline std::vector<std::string> CleanNames( const std::vector<std::string> &names, bool folders ) {
	std::vector<std::string> kept;
	for ( std::string name : names ) {
		if ( folders ) {
			while ( !name.empty() && ( name.back() == '/' || name.back() == '\\' ) ) name.pop_back();
		}
		if ( name.empty() || name[0] == '.' || name.find_first_of( "/\\" ) != std::string::npos ) continue;
		const bool known = std::any_of( kept.begin(), kept.end(), [&]( const std::string &k ) { return SameName( k, name ); } );
		if ( !known ) kept.push_back( name );
	}
	return kept;
}

/* SetupFiles (0x00418b10): `..` unless at the root (a case-sensitive compare,
 * 0x00418ba3), then the folders as `[name]`, then the files, in the order
 * they were listed; a file's date and size come from the caller, empty for a
 * file in a pak (0x0043ec00 finds only loose files). */
inline std::vector<Row> BuildRows( const std::string &current, const std::string &root,
	const std::vector<std::string> &folders, const std::vector<std::string> &files,
	const std::vector<std::string> &fileDates, const std::vector<std::string> &fileSizes ) {
	std::vector<Row> rows;
	if ( current != root ) {
		Row parent;
		parent.cells[0] = "..";
		parent.type = RowType::Parent;
		rows.push_back( parent );
	}
	for ( const std::string &folder : folders ) {
		Row row;
		row.cells[0] = "[" + folder + "]";
		row.type = RowType::Folder;
		row.name = folder;
		rows.push_back( row );
	}
	for ( std::size_t i = 0; i < files.size(); ++i ) {
		Row row;
		row.cells[0] = files[i];
		row.cells[1] = i < fileDates.size() ? fileDates[i] : std::string();
		row.cells[2] = i < fileSizes.size() ? fileSizes[i] : std::string();
		row.type = RowType::File;
		row.name = files[i];
		rows.push_back( row );
	}
	return rows;
}

/* SortByColumn (0x004cf0e0): a text column by Q_stricmp's order, a numeric
 * one by atoi, ascending, rows with equal keys in no promised order; the
 * selected row (1-based, 0 for none) is found again by identity. */
inline void SortRows( std::vector<Row> &rows, int column, int &selected ) {
	if ( column < 0 || column > 2 ) return;
	std::vector<std::size_t> order( rows.size() );
	for ( std::size_t i = 0; i < order.size(); ++i ) order[i] = i;
	const auto lower = []( const std::string &s ) {
		std::string l = s;
		for ( char &c : l ) c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
		return l;
	};
	std::stable_sort( order.begin(), order.end(), [&]( std::size_t a, std::size_t b ) {
		const std::string &x = rows[a].cells[column], &y = rows[b].cells[column];
		if ( ColumnNumeric[column] ) return std::atoi( x.c_str() ) < std::atoi( y.c_str() );
		return lower( x ) < lower( y );
	} );
	std::vector<Row> sorted;
	sorted.reserve( rows.size() );
	int moved = 0;
	for ( std::size_t i = 0; i < order.size(); ++i ) {
		sorted.push_back( rows[order[i]] );
		if ( selected > 0 && order[i] == static_cast<std::size_t>( selected - 1 ) ) moved = static_cast<int>( i ) + 1;
	}
	rows.swap( sorted );
	selected = moved;
}

/* A row press (0x004ceb3f..0x004ceb74): the first row under the header is
 * top + 1. */
inline int RowAt( float local, float header, float line, int top ) {
	if ( line <= 0.0f || local < header ) return 0;
	return top + 1 + static_cast<int>( ( local - header ) / line );
}

/* The same row pressed again within half a second and two units in each
 * direction (0x004ceb7f..0x004cebe0). */
inline bool DoubleClick( int now, int lastTime, int row, int lastRow, float x, float y, float lastX, float lastY ) {
	return lastTime + DoubleClickMs > now && row == lastRow && std::abs( y - lastY ) <= DoubleClickSlop
		&& std::abs( x - lastX ) <= DoubleClickSlop;
}

} // namespace picker
} // namespace alice
} // namespace fntech3

#endif
