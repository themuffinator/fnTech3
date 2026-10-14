/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_MOHAA_FADE_H
#define FNTECH3_AUDIO_MOHAA_FADE_H
#include <cmath>

namespace fntech3::audio::mohaa {

/* Medal of Honor's `fadesound <ms>` server command. Retail 1.00's game sends
 * its seconds times 1,000 (gamex86.dll 0x310e38e0 and 0x310e3910, the
 * constant at 0x3119f4f0) and `fadesound 0.00` where a fade must end
 * (0x310e3a10); its client game has no handler, so the engine takes the
 * command. The whole mix falls in a straight line from full to silent over
 * that much real time and stays silent until the fade is cancelled
 * (OpenMoHAA's S_FadeSound and S_Update [O]; alice.exe, the parent engine,
 * takes the command the same way at 0x004241f6). A non-finite time is
 * treated as the cancel a malformed command must not outlast. */
class Fade {
public:
	void Start( float ms, int now ) noexcept {
		if ( !( ms > 0.0f ) || !std::isfinite( ms ) ) {
			Cancel();
			return;
		}
		active_ = true;
		start_ = now;
		length_ = ms;
	}
	void Cancel() noexcept {
		active_ = false;
		start_ = 0;
		length_ = 0.0f;
	}
	bool Active() const noexcept { return active_; }
	/* The share of the mix still heard at `now`, from 1 down to 0. */
	float Gain( int now ) const noexcept {
		if ( !active_ ) {
			return 1.0f;
		}
		const float gain = 1.0f - static_cast<float>( static_cast<double>( now ) - start_ ) / length_;
		return gain < 0.0f ? 0.0f : gain > 1.0f ? 1.0f : gain;
	}

private:
	bool active_ = false;
	int start_ = 0;
	float length_ = 0.0f;
};

}
#endif
