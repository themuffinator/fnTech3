/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CINEMATIC_BINK_HPP
#define FNTECH3_CINEMATIC_BINK_HPP

#include "../compat/stvef_bink_file.hpp"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace fntech3::stvef::bink {

/* Independently declared interoperability boundary for the Bink 1 runtime
 * installed beside retail stvoy.exe. No SDK header or runtime is distributed.
 * The SP modules themselves already require this Windows x86 platform. */
#if defined(_WIN32) && ( defined(_M_IX86) || defined(__i386__) )
#define FN_BINK_CALL __stdcall
#define FN_BINK_NATIVE 1
#else
#define FN_BINK_CALL
#define FN_BINK_NATIVE 0
#endif

struct NativePrefix {
	std::uint32_t width, height, frames, frame, lastFrame, rate, rateDivisor;
};
static_assert( sizeof( NativePrefix ) == 28, "Bink 1 public header prefix" );

class Movie {
public:
	enum class Result { Waiting, Frame, End };
	Movie() = default;
	Movie( const Movie & ) = delete;
	Movie &operator=( const Movie & ) = delete;
	~Movie() { Close(); }

	bool Open( std::vector<std::uint8_t> bytes, const char *runtimePath, bool german, bool sound ) {
		Close();
		error_.clear();
		if ( !Inspect( bytes.data(), bytes.size(), german, info_ ) ) {
			error_ = "invalid or unsupported BIKi movie framing";
			return false;
		}
#if FN_BINK_NATIVE
		library_ = Sys_LoadLibrary( runtimePath );
		if ( !library_ ) {
			error_ = "the installation's binkw32.dll could not be loaded";
			return false;
		}
		if ( !Load( open_, "_BinkOpen@8" ) || !Load( close_, "_BinkClose@4" ) ||
			!Load( wait_, "_BinkWait@4" ) || !Load( decode_, "_BinkDoFrame@4" ) ||
			!Load( next_, "_BinkNextFrame@4" ) || !Load( copy_, "_BinkCopyToBuffer@28" ) ||
			!Load( track_, "_BinkSetSoundTrack@4" ) || !Load( soundSystem_, "_BinkSetSoundSystem@8" ) ||
			!Load( directSound_, "_BinkOpenDirectSound@4" ) || !Load( volume_, "_BinkSetVolume@8" ) ||
			!Load( soundState_, "_BinkSetSoundOnOff@8" ) ||
			!Load( pause_, "_BinkPause@8" ) ||
			!Load( errorText_, "_BinkGetError@0" ) ) {
			error_ = "binkw32.dll does not expose Elite Force's Bink 1 interface";
			Close();
			return false;
		}
		sound_ = sound && info_.tracks && soundSystem_( directSound_, 0 ) != 0;
		bytes_ = std::move( bytes );
		pixels_.resize( std::size_t( info_.width ) * info_.height * 4 );
		return Rewind();
#else
		(void)runtimePath;
		(void)sound;
		error_ = "Elite Force's installed Bink runtime requires Windows x86";
		return false;
#endif
	}

	bool Rewind() {
		CloseNative();
		if ( !library_ || bytes_.empty() ) return false;
		track_( sound_ ? info_.soundTrack : 0xffffffffu );
		// Retail uses 0x4000 to select the sound track. Memory input is tested
		// at binkw32.dll 0x10007f30; unlike file handles it needs no extraction.
		native_ = open_( bytes_.data(), 0x04004000u );
		if ( !native_ ) {
			const char *detail = errorText_();
			error_ = detail && *detail ? detail : "BinkOpen failed";
			return false;
		}
		if ( native_->width != info_.width || native_->height != info_.height ||
			native_->frames != info_.frames || native_->frame != 1 ||
			native_->rate != info_.rate || native_->rateDivisor != info_.rateDivisor ) {
			error_ = "Bink runtime returned inconsistent movie metadata";
			CloseNative();
			return false;
		}
		started_ = last_ = ready_ = paused_ = false;
		return true;
	}

	Result Step( std::uint32_t now ) {
		if ( !native_ ) return Result::End;
		if ( paused_ ) return Result::Waiting;
		if ( last_ ) {
			const std::uint64_t duration = ( std::uint64_t( info_.rateDivisor ) * 1000 + info_.rate - 1 ) / info_.rate;
			return std::uint32_t( now - lastTime_ ) >= duration ? Result::End : Result::Waiting;
		}
		if ( started_ && wait_( native_ ) ) return Result::Waiting;
		started_ = true;
		// Retail copies only frames Bink actually decoded (stvoy.exe 0x43092a).
		const bool decoded = decode_( native_ ) == 0;
		if ( !decoded && !ready_ ) {
			error_ = "Bink did not decode the first frame";
			CloseNative();
			return Result::End;
		}
		if ( decoded ) {
			copy_( native_, pixels_.data(), info_.width * 4, info_.height, 0, 0, 0x70000004u );
			ready_ = true;
		}
		if ( native_->frame >= native_->frames ) {
			last_ = true;
			lastTime_ = now;
		} else {
			next_( native_ );
		}
		return decoded ? Result::Frame : Result::Waiting;
	}

	void Volume( float volume ) {
		if ( native_ && volume_ ) {
			if ( !( volume >= 0.0f ) ) volume = 0.0f;
			volume_( native_, static_cast<std::int32_t>( std::min( volume, 1.0f ) * 32768 ) );
		}
	}

	void Pause( bool paused, std::uint32_t now ) {
		if ( !native_ || !pause_ || paused == paused_ ) return;
		pause_( native_, paused ? 1 : 0 );
		if ( paused ) pauseTime_ = now;
		else if ( last_ ) lastTime_ += now - pauseTime_;
		paused_ = paused;
	}

	const FileInfo &Info() const { return info_; }
	const char *Error() const { return error_.c_str(); }
	bool Sound() const {
		// Factory initialization can succeed even when opening the movie's
		// sound buffer fails. In the installed Bink 1 runtime (0x1000b260),
		// -1 queries the current sound state without turning it on or off.
		return sound_ && native_ && soundState_( native_, -1 ) != 0;
	}
	std::uint8_t *Pixels() { return ready_ ? pixels_.data() : nullptr; }

	void Close() {
		CloseNative();
#if FN_BINK_NATIVE
		if ( library_ ) Sys_UnloadLibrary( library_ );
#endif
		library_ = nullptr;
		std::vector<std::uint8_t>().swap( bytes_ );
		std::vector<std::uint8_t>().swap( pixels_ );
		sound_ = started_ = last_ = ready_ = paused_ = false;
	}

private:
	template <typename T> bool Load( T &fn, const char *name ) {
#if FN_BINK_NATIVE
		fn = reinterpret_cast<T>( Sys_LoadFunction( library_, name ) );
		return fn != nullptr;
#else
		(void)fn; (void)name; return false;
#endif
	}
	void CloseNative() {
		if ( native_ && close_ ) close_( native_ );
		native_ = nullptr;
	}

	void *library_ = nullptr;
	NativePrefix *native_ = nullptr;
	NativePrefix *(FN_BINK_CALL *open_)( const void *, std::uint32_t ) = nullptr;
	void (FN_BINK_CALL *close_)( NativePrefix * ) = nullptr;
	std::int32_t (FN_BINK_CALL *wait_)( NativePrefix * ) = nullptr;
	std::int32_t (FN_BINK_CALL *decode_)( NativePrefix * ) = nullptr;
	void (FN_BINK_CALL *next_)( NativePrefix * ) = nullptr;
	void (FN_BINK_CALL *copy_)( NativePrefix *, void *, std::int32_t, std::uint32_t,
		std::uint32_t, std::uint32_t, std::uint32_t ) = nullptr;
	void (FN_BINK_CALL *track_)( std::uint32_t ) = nullptr;
	using SoundFactory = void *(FN_BINK_CALL *)( std::uint32_t );
	std::int32_t (FN_BINK_CALL *soundSystem_)( SoundFactory, std::uint32_t ) = nullptr;
	SoundFactory directSound_ = nullptr;
	void (FN_BINK_CALL *volume_)( NativePrefix *, std::int32_t ) = nullptr;
	std::int32_t (FN_BINK_CALL *soundState_)( NativePrefix *, std::int32_t ) = nullptr;
	std::int32_t (FN_BINK_CALL *pause_)( NativePrefix *, std::int32_t ) = nullptr;
	const char *(FN_BINK_CALL *errorText_)() = nullptr;
	FileInfo info_{};
	std::vector<std::uint8_t> bytes_, pixels_;
	std::string error_;
	std::uint32_t lastTime_ = 0, pauseTime_ = 0;
	bool sound_ = false, started_ = false, last_ = false, ready_ = false, paused_ = false;
};

#undef FN_BINK_NATIVE
#undef FN_BINK_CALL
} // namespace fntech3::stvef::bink
#endif
