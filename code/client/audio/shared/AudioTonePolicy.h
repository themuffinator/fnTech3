/*
===========================================================================
Copyright (C) 2026

This file is part of FnQL.

FnQL is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
===========================================================================
*/

#pragma once

#include <algorithm>
#include <cstring>

// Per-voice tone policy for the spatial audio path. The backend decides which
// EFX filter to attach; this header decides which source class a voice belongs
// to and how the environment, the medium the listener is in, and occlusion
// shape its low/high bands. It is deliberately free of engine and OpenAL types
// so the policy can be exercised on its own.
namespace fnql_audio_tone {

// Bands within this much of unity are not worth a filter attachment.
constexpr float kToneNeutralThreshold = 0.985f;

constexpr float kToneStrongOcclusionThreshold = 0.68f;
constexpr float kToneStrongOcclusionLowCut = 0.26f;
constexpr float kToneStrongOcclusionSendLowCut = 0.12f;

constexpr float kToneUnderwaterLowCut = 0.30f;
constexpr float kToneUnderwaterHighCut = 0.18f;
constexpr float kToneUnderwaterSendLowCeiling = 0.82f;

constexpr float kToneAnnouncerLowCut = 0.10f;
constexpr float kToneLocalLowCut = 0.08f;
constexpr float kToneItemLowCut = 0.06f;
constexpr float kToneVoiceLowCut = 0.08f;
constexpr float kToneVoiceHighCeiling = 0.96f;
constexpr float kToneBodyHighCut = 0.06f;
constexpr float kToneWeaponLowCeiling = 0.98f;
constexpr float kToneWeaponHighFloor = 0.97f;

enum class SoundToneClass {
	Neutral,
	World,
	Weapon,
	Voice,
	Item,
	Body,
	// Emitted by the listener's own entity: non-positional, but still inside
	// the level rather than on top of it.
	Self,
	// Non-diegetic: UI, feedback, and chat that plays outside the level.
	Local,
	Announcer,
	Stereo,
	Multichannel,
	Underwater
};

inline const char *SoundToneClassName( SoundToneClass soundClass ) {
	switch ( soundClass ) {
	case SoundToneClass::World:
		return "world";
	case SoundToneClass::Weapon:
		return "weapon";
	case SoundToneClass::Voice:
		return "voice";
	case SoundToneClass::Item:
		return "item";
	case SoundToneClass::Body:
		return "body";
	case SoundToneClass::Self:
		return "self";
	case SoundToneClass::Local:
		return "local";
	case SoundToneClass::Announcer:
		return "announcer";
	case SoundToneClass::Stereo:
		return "stereo";
	case SoundToneClass::Multichannel:
		return "multichannel";
	case SoundToneClass::Underwater:
		return "underwater";
	default:
		return "neutral";
	}
}

// Announcer, UI, and feedback audio is not emitted inside the level, so the
// listener's environment must never colour it. Everything else is diegetic and
// follows the room, the medium, and occlusion.
inline bool ToneClassIsNonDiegetic( SoundToneClass soundClass ) {
	return soundClass == SoundToneClass::Announcer || soundClass == SoundToneClass::Local;
}

// Authored stereo, surround, music, and UI content keeps the direct path it was
// mixed for. A two-channel world sample that opted into positional playback is
// a world source and is shaped like one.
inline bool ToneClassKeepsAuthoredPath( SoundToneClass soundClass, bool positionalSource ) {
	if ( positionalSource ) {
		return false;
	}
	return soundClass == SoundToneClass::Stereo || soundClass == SoundToneClass::Multichannel;
}

struct ToneClassInputs {
	// Class implied by the voice's entity channel, or Neutral when the channel
	// implies nothing.
	SoundToneClass channelClass = SoundToneClass::Neutral;
	const char *sampleName = nullptr;
	bool stereoSample = false;
	bool multichannelSample = false;
	// The voice is attached to the listener's own entity.
	bool listenerAttached = false;
	// The voice plays on a local-only channel and never reaches the world mix.
	bool localOnly = false;
	bool positionalSource = false;
	bool underwater = false;
};

inline bool SampleNameHasPrefix( const char *name, const char *prefix ) {
	if ( name == nullptr || prefix == nullptr ) {
		return false;
	}
	const size_t prefixLength = std::strlen( prefix );
	return std::strncmp( name, prefix, prefixLength ) == 0;
}

inline SoundToneClass ClassifyToneClass( const ToneClassInputs &inputs ) {
	if ( inputs.stereoSample ) {
		return inputs.multichannelSample ? SoundToneClass::Multichannel : SoundToneClass::Stereo;
	}
	if ( inputs.channelClass != SoundToneClass::Neutral ) {
		return inputs.channelClass;
	}
	if ( inputs.localOnly ) {
		return SoundToneClass::Local;
	}
	if ( SampleNameHasPrefix( inputs.sampleName, "sound/feedback/" ) ) {
		return SoundToneClass::Local;
	}
	// Sounds emitted by the listener's own entity, such as a held weapon's fire
	// loop, keep the dry first-person character of local audio but are not
	// non-diegetic: the medium the listener is standing in still has to reach
	// them.
	if ( inputs.listenerAttached ) {
		return SoundToneClass::Self;
	}
	if ( SampleNameHasPrefix( inputs.sampleName, "sound/weapons/" ) ) {
		return SoundToneClass::Weapon;
	}
	if ( SampleNameHasPrefix( inputs.sampleName, "sound/items/" ) ) {
		return SoundToneClass::Item;
	}
	if ( SampleNameHasPrefix( inputs.sampleName, "sound/player/" ) ) {
		return SoundToneClass::Body;
	}
	if ( inputs.underwater && inputs.positionalSource ) {
		return SoundToneClass::Underwater;
	}
	if ( inputs.positionalSource ) {
		return SoundToneClass::World;
	}
	return SoundToneClass::Neutral;
}

// Listener environment bands, before per-voice shaping.
struct ToneEnvironmentBands {
	float directLF = 1.0f;
	float directHF = 1.0f;
	float wetLF = 1.0f;
	float wetHF = 1.0f;
	bool underwater = false;
};

struct ToneBands {
	float directLF = 1.0f;
	float directHF = 1.0f;
	float sendLF = 1.0f;
	float sendHF = 1.0f;
	bool bandPass = false;
};

inline float ClampBand( float value ) {
	return ( std::max )( 0.0f, ( std::min )( value, 1.0f ) );
}

// occlusionDirectHF and occlusionSendHF are the environment high bands after
// the occlusion curve has already been applied to them.
inline ToneBands ShapeToneBands( SoundToneClass soundClass, const ToneEnvironmentBands &environment,
	float occlusionDirectHF, float occlusionSendHF, float occlusion ) {
	ToneBands bands;
	bands.directLF = ClampBand( environment.directLF );
	bands.directHF = ClampBand( occlusionDirectHF );
	bands.sendLF = ClampBand( environment.wetLF );
	bands.sendHF = ClampBand( occlusionSendHF );

	switch ( soundClass ) {
	case SoundToneClass::Announcer:
		bands.directLF = ( std::min )( bands.directLF, 1.0f - kToneAnnouncerLowCut );
		bands.directHF = 1.0f;
		break;
	case SoundToneClass::Local:
	case SoundToneClass::Self:
		bands.directLF = ( std::min )( bands.directLF, 1.0f - kToneLocalLowCut );
		bands.directHF = 1.0f;
		break;
	case SoundToneClass::Item:
		bands.directLF = ( std::min )( bands.directLF, 1.0f - kToneItemLowCut );
		break;
	case SoundToneClass::Voice:
		bands.directLF = ( std::min )( bands.directLF, 1.0f - kToneVoiceLowCut );
		bands.directHF = ( std::min )( bands.directHF, kToneVoiceHighCeiling );
		break;
	case SoundToneClass::Body:
		bands.directHF = ( std::min )( bands.directHF, 1.0f - kToneBodyHighCut );
		break;
	case SoundToneClass::Weapon:
		bands.directLF = ( std::min )( bands.directLF, kToneWeaponLowCeiling );
		// Preserve the transient crack of gunfire against the ambient
		// environment HF cut; occlusion and underwater still shape it below.
		bands.directHF = ( std::max )( bands.directHF, kToneWeaponHighFloor );
		break;
	default:
		break;
	}

	if ( environment.underwater && !ToneClassIsNonDiegetic( soundClass ) ) {
		bands.directLF = ( std::min )( bands.directLF, 1.0f - kToneUnderwaterLowCut );
		bands.directHF = ( std::min )( bands.directHF, ClampBand( environment.directHF ) * ( 1.0f - kToneUnderwaterHighCut ) );
		bands.sendLF = ( std::min )( bands.sendLF, kToneUnderwaterSendLowCeiling );
		bands.sendHF = ( std::min )( bands.sendHF, ClampBand( environment.wetHF ) * ( 1.0f - kToneUnderwaterHighCut ) );
		bands.bandPass = true;
	}

	if ( occlusion >= kToneStrongOcclusionThreshold && !ToneClassIsNonDiegetic( soundClass ) ) {
		const float occlusionBlend = ClampBand( ( occlusion - kToneStrongOcclusionThreshold ) /
			( 1.0f - kToneStrongOcclusionThreshold ) );
		bands.directLF = ( std::min )( bands.directLF, 1.0f - occlusionBlend * kToneStrongOcclusionLowCut );
		bands.sendLF = ( std::min )( bands.sendLF, 1.0f - occlusionBlend * kToneStrongOcclusionSendLowCut );
		bands.bandPass = true;
	}

	return bands;
}

} // namespace fnql_audio_tone
