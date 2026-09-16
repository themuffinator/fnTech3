/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Independently authored from original RTCW SP music-contract observations. */
#ifndef FNTECH3_AUDIO_RTCW_MUSIC_H
#define FNTECH3_AUDIO_RTCW_MUSIC_H

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace fntech3::audio::rtcw {

using SampleTime = std::int64_t;

class SampleClock {
public:
	SampleTime Advance( SampleTime microseconds, bool deviceClock, int rate ) {
		if ( rate <= 0 || rate > 192000 || microseconds < 0 ) return samples_;
		if ( !initialized_ || deviceClock != deviceClock_ || microseconds < previous_ ) {
			previous_ = microseconds;
			deviceClock_ = deviceClock;
			initialized_ = true;
			fraction_ = 0;
			return samples_;
		}
		const auto elapsed = static_cast<std::uint64_t>( microseconds - previous_ );
		previous_ = microseconds;
		const auto whole = elapsed / 1000000;
		const auto part = ( elapsed % 1000000 ) * rate + fraction_;
		fraction_ = part % 1000000;
		const auto room = static_cast<std::uint64_t>( ( std::numeric_limits<SampleTime>::max )() - samples_ );
		if ( part / 1000000 > room || whole > room / rate || whole * rate > room - part / 1000000 ) {
			samples_ = ( std::numeric_limits<SampleTime>::max )();
		} else samples_ += whole * rate + part / 1000000;
		return samples_;
	}
	void Pause() { initialized_ = false; }
private:
	SampleTime samples_ = 0, previous_ = 0;
	std::uint64_t fraction_ = 0;
	bool initialized_ = false, deviceClock_ = false;
};

// RTCW's stream fades measure duration in source samples, against the output
// sample clock. Preserve its binary32 endpoint calculation, with defined
// behavior when a malformed duration or a long-running clock exceeds int32.
inline SampleTime FadeEnd( SampleTime now, int rate, int milliseconds ) {
	// Force the original binary32 endpoint before conversion. An x87 temporary
	// otherwise retains excess precision and can truncate one sample early.
	volatile float endpoint = static_cast<float>( now ) +
		( static_cast<float>( rate ) / 1000.0f ) * milliseconds;
	const double end = endpoint;
	if ( end >= static_cast<double>( ( std::numeric_limits<SampleTime>::max )() ) )
		return ( std::numeric_limits<SampleTime>::max )();
	if ( end <= static_cast<double>( ( std::numeric_limits<SampleTime>::min )() ) )
		return ( std::numeric_limits<SampleTime>::min )();
	return static_cast<SampleTime>( end );
}

inline bool ValidFadeVolume( float value ) {
	// Stock fades use [0,1]. Keep finite amplification/negative stop targets,
	// while bounding PCM conversion and backend gain calculations.
	return std::isfinite( value ) && value >= -16.0f && value <= 16.0f;
}

inline float FadeFraction( SampleTime now, SampleTime start, SampleTime end ) {
	// Widen before subtraction so corrupted/very old clocks cannot overflow.
	return static_cast<float>( static_cast<double>( now ) - static_cast<double>( start ) ) /
		static_cast<float>( static_cast<double>( end ) - static_cast<double>( start ) );
}

struct StreamFade {
	SampleTime start = 0, end = 0;
	float from = 0.0f, target = 0.0f;

	void Start( SampleTime now, int rate, int milliseconds ) {
		*this = {};
		if ( milliseconds ) {
			start = now;
			end = FadeEnd( now, rate, milliseconds );
			target = 1.0f;
		}
	}

	bool Set( float volume, int milliseconds, SampleTime now, int rate ) {
		if ( !ValidFadeVolume( volume ) ) return false;
		float initial = 1.0f;
		if ( start ) {
			initial = end <= now ? target : FadeFraction( now, start, end );
		}
		// Original SP restarts an interrupted fade from its elapsed fraction,
		// rather than from the interpolated gain. This is observable in mu_fade.
		from = initial;
		start = now;
		end = FadeEnd( now, rate, milliseconds );
		target = volume;
		return true;
	}

	float Value( SampleTime now, bool &stop ) const {
		if ( !start ) return 1.0f; // Original zero-clock sentinel.
		if ( end <= now ) {
			if ( target <= 0.0f ) stop = true;
			return target;
		}
		const float fraction = FadeFraction( now, start, end );
		return ( 1.0f - fraction ) * from + fraction * target;
	}
};

struct MasterFade {
	SampleTime start = 0, end = 0;
	float from = 1.0f, target = 1.0f, current = 1.0f;

	bool Set( float volume, int milliseconds, SampleTime now, int outputRate ) {
		if ( !ValidFadeVolume( volume ) ) return false;
		from = current;
		target = volume;
		start = now;
		end = FadeEnd( now, outputRate, milliseconds );
		if ( !milliseconds ) {
			from = target = current = volume;
			start = end = 0;
		}
		return true;
	}

	float Value( SampleTime now ) {
		if ( now >= end ) current = target;
		else if ( now <= start ) current = from;
		else {
			const float fraction = FadeFraction( now, start, end );
			current = static_cast<float>( ( 1.0 - fraction ) * from + fraction * target );
		}
		return current;
	}
};

inline std::string MusicPath( const char *text, std::size_t limit = 63 ) {
	std::size_t length = 0;
	if ( text ) while ( length < limit && text[length] ) ++length;
	return text ? std::string( text, length ) : std::string{};
}

inline bool OneTimeOnly( const std::string &text ) {
	constexpr char marker[] = "onetimeonly";
	if ( text.size() != sizeof( marker ) - 1 ) return false;
	for ( std::size_t i = 0; i < text.size(); ++i ) {
		const unsigned char c = static_cast<unsigned char>( text[i] );
		if ( ( c >= 'A' && c <= 'Z' ? c + ( 'a' - 'A' ) : c ) != marker[i] ) return false;
	}
	return true;
}

// IO owns codec/device resources. Close(false) preserves already-buffered PCM;
// Open returns the decoded source rate (zero on failure). All state transitions
// live here, so the OpenAL and legacy backends cannot diverge in queue policy.
class Music {
public:
	template<class IO>
	void Start( IO &io, const char *intro, const char *loop, int milliseconds ) {
		const std::string requested = MusicPath( intro );
		if ( milliseconds < 0 ) {
			queued_ = requested;
			queueType_ = requested.empty() ? 0 : milliseconds;
			if ( requested.empty() ) loop_.clear();
			else if ( milliseconds == -2 ) io.CurrentMusic( queued_.c_str() );
			return;
		}
		if ( !io.Ready() ) return;
		// Snapshot both names before changing state; the caller may pass Loop().
		std::string nextLoop = MusicPath( loop );
		if ( nextLoop.empty() ) nextLoop = requested;
		if ( OneTimeOnly( nextLoop ) ) nextLoop = loop_;
		io.CurrentMusic( "" );
		loop_ = nextLoop.substr( 0, 59 );
		name_ = requested.substr( 0, 59 );
		const auto slash = name_.find_last_of( '/' );
		const auto dot = name_.find_last_of( '.' );
		if ( dot == std::string::npos || ( slash != std::string::npos && dot < slash ) ) name_ += ".wav";
		io.Close( false );
		active_ = false;
		if ( requested.empty() ) return;
		rate_ = io.Open( name_.c_str() );
		if ( rate_ <= 0 ) { io.Close( false ); return; }
		active_ = true;
		stop_ = false;
		looped_ = false;
		fade_.Start( io.Clock(), rate_, milliseconds );
		io.CurrentMusic( nextLoop.c_str() );
	}

	void Stop() { if ( active_ ) stop_ = true; }
	void Fade( float volume, int milliseconds, SampleTime now ) {
		if ( active_ && !stop_ ) fade_.Set( volume, milliseconds, now, rate_ );
	}
	float Gain( SampleTime now ) { return fade_.Value( now, stop_ ); }

	template<class IO>
	bool BeginService( IO &io, float musicVolume ) {
		if ( !io.Ready() ) return false;
		if ( stop_ && active_ ) {
			io.Close( true );
			active_ = stop_ = false;
			return false;
		}
		if ( musicVolume <= 0.0f ) return false;
		if ( active_ ) return true;
		if ( !queued_.empty() ) {
			const std::string next = queued_;
			if ( queueType_ == -1 ) Start( io, next.c_str(), name_.c_str(), 0 );
			else if ( queueType_ != -3 ) Start( io, next.c_str(), next.c_str(), 0 );
			// Original SP retains the queued name as a restart fallback. In
			// particular, -3 suppresses one service before the fallback resumes.
			queueType_ = 0;
		}
		return false; // Newly opened music starts decoding on the next service.
	}

	template<class IO>
	void EndOfTrack( IO &io ) {
		if ( queueType_ ) {
			io.Close( true );
			active_ = false;
		} else if ( !loop_.empty() ) {
			if ( !looped_ ) {
				Start( io, loop_.c_str(), loop_.c_str(), 0 );
				looped_ = true;
			} else {
				io.Close( false );
				rate_ = io.Open( name_.c_str() );
				active_ = rate_ > 0;
				if ( !active_ ) io.Close( false );
			}
		} else {
			Stop();
			io.CurrentMusic( "" );
		}
	}

	template<class IO>
	void Clear( IO &io ) {
		io.Close( true );
		queued_.clear();
		queueType_ = 0;
		active_ = stop_ = false;
		// S_ClearSounds clears playback and queued music, retaining the last
		// track/loop for a subsequent onetimeonly request. S_Init resets all.
	}

	bool Active() const { return active_; }
	const std::string &Name() const { return name_; }
	const std::string &Loop() const { return loop_; }
	const std::string &Queued() const { return queued_; }
	int QueueType() const { return queueType_; }
	const StreamFade &FadeState() const { return fade_; }

private:
	std::string name_, loop_, queued_;
	int queueType_ = 0, rate_ = 0;
	bool active_ = false, stop_ = false, looped_ = false;
	StreamFade fade_;
};

} // namespace fntech3::audio::rtcw
#endif
