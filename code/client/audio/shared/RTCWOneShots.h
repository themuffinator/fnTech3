/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_RTCW_ONE_SHOTS_H
#define FNTECH3_AUDIO_RTCW_ONE_SHOTS_H

#include <array>

namespace fntech3::audio::rtcw {

// Engine-owned policy records. Flags and channel numbers are RTCW ABI facts;
// no original sound implementation or module-owned memory is retained here.
struct OneShot {
	int entity = 0, channel = 0, handle = 0, flags = 0, allocated = 0;
	bool active = false;
};

class OneShots {
public:
	static constexpr int Capacity = 96;
	static constexpr int Auto = 0, Weapon = 2, Announcer = 7;
	static constexpr int OkToCut = 1, RequestCut = 2, CutOff = 4, CutOffAll = 8, NoCut = 16;
	OneShots() { Reset(); }
	void Reset() {
		voices_ = {};
		available_ = Capacity;
		head_ = 0;
		for ( int i = 0; i < Capacity; ++i ) free_[i] = Capacity - 1 - i;
	}
	const OneShot &operator[]( int index ) const { return voices_[index]; }
	void Release( int index ) {
		if ( index < 0 || index >= Capacity || !voices_[index].active ) return;
		voices_[index].active = false;
		free_[(head_ + available_++) % Capacity] = index;
	}

	// The callback releases backend resources only. Slot ownership stays here,
	// including FIFO free order, restart age, and the original strict age test.
	template<class Stop>
	int Start( bool singlePlayer, int clientLimit, int listener, int now,
		OneShot request, Stop stop ) {
		for ( int i = 0; i < Capacity; ++i ) {
			const auto &voice = voices_[i];
			if ( !voice.active || voice.entity != request.entity || voice.channel != request.channel ) continue;
			const bool replaceClient = singlePlayer && request.entity < clientLimit &&
				request.channel != Auto && request.channel != Weapon;
			const bool cut = (request.flags & CutOffAll) || (!(voice.flags & NoCut) &&
				(replaceClient || (voice.flags & OkToCut) || ((request.flags & CutOff) && (voice.flags & RequestCut))));
			if ( cut ) { stop(i); Release(i); }
		}
		int selected = -1;
		if ( !singlePlayer || request.channel != Auto ) {
			for ( int i = 0; i < Capacity; ++i ) {
				const auto &voice = voices_[i];
				if ( voice.active && voice.entity == request.entity && voice.channel == request.channel &&
					voice.handle == request.handle && !(voice.flags & NoCut) ) { selected = i; break; }
			}
		}
		request.allocated = now;
		if ( selected >= 0 ) request.allocated = voices_[selected].allocated;
		else if ( available_ > (singlePlayer ? 1 : 0) ) {
			// SP deliberately retains its final free-list node as a sentinel.
			selected = free_[head_]; head_ = (head_ + 1) % Capacity; --available_;
		}
		else {
			int oldest = now;
			for ( int i = 0; i < Capacity; ++i ) {
				const auto &voice = voices_[i];
				if ( !voice.active ) continue;
				if ( voice.entity != request.entity ) continue;
				if ( voice.handle == request.handle ) { selected = i; break; }
				if ( voice.entity != listener && voice.channel != Announcer && voice.allocated < oldest ) {
					selected = i; oldest = voice.allocated;
				}
			}
			if ( selected < 0 ) {
				for ( int i = 0; i < Capacity; ++i ) {
					const auto &voice = voices_[i];
					if ( !voice.active ) continue;
					if ( voice.entity != listener && voice.channel != Announcer && voice.allocated < oldest ) {
						selected = i; oldest = voice.allocated;
					}
				}
			}
			// Original SP/MP read beyond their channel arrays in the remaining
			// fallback. Drop safely when no defined replacement is available.
			if ( selected < 0 ) return -1;
		}
		request.active = true;
		voices_[selected] = request;
		return selected;
	}

private:
	std::array<OneShot, Capacity> voices_{};
	std::array<int, Capacity> free_{};
	int available_ = 0, head_ = 0;
};

} // namespace fntech3::audio::rtcw
#endif
