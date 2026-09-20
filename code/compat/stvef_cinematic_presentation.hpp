/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_CINEMATIC_PRESENTATION_HPP
#define FNTECH3_STVEF_CINEMATIC_PRESENTATION_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>

namespace fntech3::stvef::bink {

/* Retail stvoy.exe 1.20, 0x403790/0x4036b0: hold the first decoded frame
 * during a one-second fade-in, and retain the last during fade-out. */
class Presentation {
public:
	void Open( bool fadeUp, bool fadeDown, float quality ) {
		Reset();
		up_ = fadeUp; down_ = fadeDown; crop_ = quality >= 2.0f;
		alpha_ = up_ ? 0.0f : 1.0f;
	}
	void Reset() {
		phase_ = Phase::First; up_ = down_ = crop_ = false;
		origin_ = 0; alpha_ = 1.0f;
		std::vector<std::uint8_t>().swap( cropPixels_ );
	}
	void Update( std::uint32_t now ) {
		const auto elapsed = now - origin_;
		if ( phase_ == Phase::Up ) {
			alpha_ = elapsed < 1000 ? elapsed * 0.001f : 1.0f;
			if ( elapsed >= 1000 ) phase_ = Phase::Playing;
		} else if ( phase_ == Phase::Down ) {
			alpha_ = elapsed < 1000 ? 1.0f - elapsed * 0.001f : 0.0f;
			if ( elapsed >= 1000 ) phase_ = Phase::Done;
		}
	}
	void Frame( std::uint32_t now ) {
		if ( phase_ != Phase::First ) return;
		origin_ = now;
		phase_ = up_ ? Phase::Up : Phase::Playing;
	}
	bool End( std::uint32_t now ) {
		if ( !down_ || phase_ == Phase::First ) { phase_ = Phase::Done; return false; }
		origin_ = now; alpha_ = 1.0f; phase_ = Phase::Down;
		return true;
	}
	bool Pause() const { return phase_ == Phase::Up; }
	bool Mute() const { return ( phase_ == Phase::First && up_ ) || Pause() || Ending(); }
	bool Ending() const { return phase_ == Phase::Down || phase_ == Phase::Done; }
	bool Done() const { return phase_ == Phase::Done; }
	float Alpha() const { return alpha_; }

	struct Image {
		const std::uint8_t *pixels = nullptr;
		int width = 0, height = 0;
		float heightScale = 1.0f;
	};
	Image Pixels( const std::uint8_t *source, std::size_t bytes, int width, int height, bool dirty ) {
		if ( !source || width <= 0 || height <= 0 || width > 2048 || height > 2048 ||
			bytes < std::size_t( width ) * height * 4 ) return {};
		// Quality 0/1 retain the complete image. Retail quality 1 tiles it
		// through small textures; a modern native texture needs no tiling.
		if ( !crop_ || width < 256 || height < 192 ) return { source, width, height, 1.0f };
		// Quality 2 copies a central 256x192 region into a 256x256 texture,
		// then draws a 4:3 taller rectangle, clipped by the game viewport.
		// Clear the unused rows instead of exposing retail's uninitialized pad.
		if ( cropPixels_.empty() ) { cropPixels_.resize( 256 * 256 * 4 ); dirty = true; }
		if ( dirty ) {
			const int left = width / 2 - 128, top = height / 2 - 96;
			for ( int y = 0; y < 192; ++y ) {
				std::memcpy( cropPixels_.data() + y * 256 * 4,
					source + ( std::size_t( top + y ) * width + left ) * 4, 256 * 4 );
			}
		}
		return { cropPixels_.data(), 256, 256, 4.0f / 3.0f };
	}

private:
	enum class Phase { First, Up, Playing, Down, Done };
	Phase phase_ = Phase::First;
	bool up_ = false, down_ = false, crop_ = false;
	std::uint32_t origin_ = 0;
	float alpha_ = 1.0f;
	std::vector<std::uint8_t> cropPixels_;
};
}
#endif
