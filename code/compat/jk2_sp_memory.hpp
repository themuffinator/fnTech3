/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JK2_SP_MEMORY_HPP
#define FNTECH3_COMPAT_JK2_SP_MEMORY_HPP
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace fntech3::jk2::sp {
/* Native module memory lives outside the engine's tagged zone. Raven's tags
 * belong to Raven's contract, not to the shared engine's memtag_t enum. Keep
 * the requested tag as metadata and reclaim every block after a failed Init
 * or an unload, including blocks whose game shutdown was never reached. */
class AllocationPool {
	struct alignas( std::max_align_t ) Block {
		Block *next;
		void *allocation;
		std::size_t bytes;
		std::int32_t tag;
	};
	Block *head_ = nullptr;
	std::size_t count_ = 0;
	void *( *allocate_ )( std::size_t ) = std::malloc;
	void ( *release_ )( void * ) = std::free;
public:
	AllocationPool() = default;
	AllocationPool( void *( *allocate )( std::size_t ), void ( *release )( void * ) )
		: allocate_( allocate ), release_( release ) {}
	AllocationPool( const AllocationPool & ) = delete;
	AllocationPool &operator=( const AllocationPool & ) = delete;
	~AllocationPool() { Reset(); }
	void *Allocate( std::int32_t bytes, std::int32_t tag, bool zero ) {
		constexpr std::size_t overhead = sizeof( Block ) + alignof( Block ) - 1;
		if ( bytes < 0 || !allocate_ || !release_ ||
			static_cast<std::size_t>( bytes ) > std::numeric_limits<std::size_t>::max() - overhead ) return nullptr;
		void *allocation = allocate_( overhead + static_cast<std::size_t>( bytes ) );
		if ( !allocation ) return nullptr;
		// Win32 malloc can provide less alignment than MinGW's max_align_t.
		// Align both the header and payload, but free the allocator's pointer.
		const auto address = reinterpret_cast<std::uintptr_t>( allocation );
		const auto padding = ( alignof( Block ) - address % alignof( Block ) ) % alignof( Block );
		auto *block = new ( static_cast<unsigned char *>( allocation ) + padding )
			Block{ head_, allocation, static_cast<std::size_t>( bytes ), tag };
		head_ = block; ++count_;
		void *data = block + 1;
		if ( zero ) std::memset( data, 0, block->bytes );
		return data;
	}
	bool Free( void *data ) {
		if ( !data ) return true;
		Block **link = &head_;
		while ( *link && ( *link ) + 1 != data ) link = &( *link )->next;
		if ( !*link ) return false;
		Block *block = *link; *link = block->next; --count_; release_( block->allocation );
		return true;
	}
	void Reset() {
		while ( head_ ) { Block *block = head_; head_ = block->next; release_( block->allocation ); }
		count_ = 0;
	}
	std::size_t Count() const { return count_; }
};
}
#endif
