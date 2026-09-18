/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_SERVER_CACHE_HPP
#define FNTECH3_RTCW_SERVER_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3::rtcw::server_cache {

/* Original WolfMP keeps its server browser cache in <fs_homepath>/servercache.dat,
 * written by its x86 LAN_SaveServersToCache (cl_ui.c): the global, mplayer and
 * favorite counts, the total size of the three arrays, then 2048 global, 128
 * mplayer and 128 favorite serverInfo_t records of 204 bytes. A record starts
 * with its netadr_t (type, ip[4], ipx[10], port in network order) and hostName. */
inline constexpr const char *LegacyFileName = "servercache.dat";
inline constexpr std::size_t RecordBytes = 204;
inline constexpr std::size_t GlobalRecords = 2048;
inline constexpr std::size_t OtherRecords = 128;
inline constexpr std::size_t HeaderBytes = 4 * sizeof( std::int32_t );
inline constexpr std::size_t ArrayBytes = ( GlobalRecords + 2 * OtherRecords ) * RecordBytes;
inline constexpr std::size_t FileBytes = HeaderBytes + ArrayBytes;
inline constexpr std::size_t FavoritesOffset = HeaderBytes + ( GlobalRecords + OtherRecords ) * RecordBytes;
inline constexpr std::size_t AddressTypeOffset = 0;
inline constexpr std::size_t AddressIPOffset = 4;
inline constexpr std::size_t AddressPortOffset = 18;
inline constexpr std::size_t HostNameOffset = 20;
inline constexpr std::size_t NameBytes = 32;
inline constexpr std::int32_t AddressIP = 4; // NA_BOT, NA_BAD, NA_LOOPBACK, NA_BROADCAST, NA_IP, ...

struct Favorite {
	unsigned char ip[4];
	unsigned short port;
	char hostName[NameBytes];
};

inline std::int32_t ReadInt( const unsigned char *data ) {
	return static_cast<std::int32_t>( static_cast<std::uint32_t>( data[0] ) |
		( static_cast<std::uint32_t>( data[1] ) << 8 ) | ( static_cast<std::uint32_t>( data[2] ) << 16 ) |
		( static_cast<std::uint32_t>( data[3] ) << 24 ) );
}

/* Copies the IPv4 favorites of a complete WolfMP cache to out, which holds
 * OtherRecords entries, and returns how many; -1 when the data is not one.
 * WolfMP itself only checks the stored array size; this also needs the exact
 * file length and a favorite count within the array. Other address types (the
 * retired IPX among them) are skipped, and host names are cut to fit. */
inline int ReadFavorites( const unsigned char *data, std::size_t bytes, Favorite *out ) {
	if ( !data || bytes != FileBytes || ReadInt( data + 12 ) != static_cast<std::int32_t>( ArrayBytes ) ) {
		return -1;
	}
	const std::int32_t count = ReadInt( data + 8 );
	if ( count < 0 || count > static_cast<std::int32_t>( OtherRecords ) ) {
		return -1;
	}
	int found = 0;
	for ( std::int32_t i = 0; i < count; ++i ) {
		const unsigned char *record = data + FavoritesOffset + static_cast<std::size_t>( i ) * RecordBytes;
		if ( ReadInt( record + AddressTypeOffset ) != AddressIP ) {
			continue;
		}
		Favorite &favorite = out[found++];
		std::memcpy( favorite.ip, record + AddressIPOffset, sizeof( favorite.ip ) );
		favorite.port = static_cast<unsigned short>( ( record[AddressPortOffset] << 8 ) | record[AddressPortOffset + 1] );
		std::memcpy( favorite.hostName, record + HostNameOffset, NameBytes );
		favorite.hostName[NameBytes - 1] = '\0';
	}
	return found;
}

} // namespace fntech3::rtcw::server_cache

#endif
