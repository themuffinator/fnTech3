/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_MOHAA_LOAD_RECORD_HPP
#define FNTECH3_COMPAT_MOHAA_LOAD_RECORD_HPP

/* Medal of Honor: Allied Assault's load records, maps/<map>.min, which drive
 * the loading screen's bar.
 *
 * A record is text, one item to a line: its version, 3; the number of
 * resources and the bytes their names take, each counted with its end; then
 * each resource's name -- its kind in the first letter -- and the time it
 * took to load, in the engine's own units. Retail's reader refuses a record
 * whose version differs or whose names do not add up to the byte count
 * (OpenMoHAA UI_ArchiveLoadMapinfo and UI_ParseLoadMapinfo, reference only).
 * As a load reaches a resource, the bar advances by that resource's time, out
 * of the time of every resource the load will reach
 * (UI_RegisterLoadResource). */

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace fntech3::mohaa::loadrecord {

constexpr int Version = 3;
/* More would be no map's record: the 54 retail ones name 53 (briefing1) to
 * 942 (m4l3), and every one's names add up to its byte count. */
constexpr int MaxResources = 65536;

struct Record {
	/* Sorted by name, lower case; a resource already counted, or never to be
	 * counted, has no time left. */
	std::vector<std::pair<std::string, int>> entries;
	long long total = 0;
	long long current = 0;
	bool valid = false;
};

inline std::string Lower( const std::string &name ) {
	std::string lower( name );
	for ( char &c : lower ) {
		c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
	}
	return lower;
}

/* The record in `text`. `counted( name )` says whether this load will report
 * a resource at all -- a kind the engine reports and not already cached --
 * and only those count towards the total. */
template<typename Counted>
Record Parse( const char *text, int length, Counted counted ) {
	Record record;
	if ( !text || length <= 0 ) {
		return record;
	}
	std::vector<std::string> lines;
	std::string line;
	for ( int i = 0; i <= length; i++ ) {
		const char c = i < length ? text[i] : '\n';
		if ( c == '\n' ) {
			while ( !line.empty() && ( line.back() == '\r' || line.back() == ' ' || line.back() == '\t' ) ) {
				line.pop_back();
			}
			std::size_t start = 0;
			while ( start < line.size() && ( line[start] == ' ' || line[start] == '\t' ) ) {
				start++;
			}
			if ( start < line.size() ) {
				lines.push_back( line.substr( start ) );
			}
			line.clear();
		} else if ( c != '\0' ) {
			line += c;
		}
	}
	if ( lines.size() < 2 || std::atoi( lines[0].c_str() ) != Version ) {
		return record;
	}
	const char *counts = lines[1].c_str();
	char *after = nullptr;
	const long count = std::strtol( counts, &after, 10 );
	const bool haveCount = after != counts;
	counts = after;
	const long bytes = std::strtol( counts, &after, 10 );
	if ( !haveCount || after == counts || count <= 0 || count > MaxResources
		|| lines.size() < 2 + 2 * static_cast<std::size_t>( count ) ) {
		return record;
	}
	long long nameBytes = 0;
	for ( int i = 0; i < count; i++ ) {
		const std::string &name = lines[2 + 2 * static_cast<std::size_t>( i )];
		nameBytes += static_cast<long long>( name.size() ) + 1;
		const int time = std::atoi( lines[3 + 2 * static_cast<std::size_t>( i )].c_str() );
		record.entries.emplace_back( Lower( name ), time > 0 ? time : 0 );
	}
	if ( nameBytes != bytes ) {
		record.entries.clear();
		return record;
	}
	std::sort( record.entries.begin(), record.entries.end() );
	for ( auto &entry : record.entries ) {
		if ( !counted( entry.first ) ) {
			entry.second = 0;
		}
		record.total += entry.second;
	}
	record.valid = record.total > 0;
	return record;
}

/* A resource the load has reached: its time is counted once, and true is
 * returned when the bar moved. */
inline bool Reach( Record &record, const std::string &name ) {
	if ( !record.valid ) {
		return false;
	}
	const std::string key = Lower( name );
	auto found = std::lower_bound( record.entries.begin(), record.entries.end(), key,
		[]( const std::pair<std::string, int> &entry, const std::string &value ) { return entry.first < value; } );
	if ( found == record.entries.end() || found->first != key || found->second <= 0 ) {
		return false;
	}
	record.current += found->second;
	found->second = 0;
	return true;
}

inline float Fraction( const Record &record ) {
	if ( !record.valid || record.total <= 0 ) {
		return 0.0f;
	}
	return static_cast<float>( std::min( record.current, record.total ) ) / static_cast<float>( record.total );
}

} // namespace fntech3::mohaa::loadrecord

#endif
