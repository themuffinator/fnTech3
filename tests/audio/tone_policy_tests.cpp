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

#include "AudioTonePolicy.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

namespace tone = fnql_audio_tone;

bool Check( bool condition, const char *test, int line, const char *expression ) {
	if ( condition ) {
		return true;
	}
	std::fprintf( stderr, "%s:%d: check failed: %s\n", test, line, expression );
	return false;
}

#define CHECK( expression ) do { if ( !Check( ( expression ), __func__, __LINE__, #expression ) ) return false; } while ( 0 )

bool Near( float a, float b, float epsilon = 0.0001f ) {
	return std::fabs( a - b ) <= epsilon;
}

// Matches the "underwater" reverb preset the listener probe selects inside a
// liquid brush.
tone::ToneEnvironmentBands UnderwaterEnvironment() {
	tone::ToneEnvironmentBands environment;
	environment.directLF = 1.0f;
	environment.directHF = 0.25f;
	environment.wetLF = 1.0f;
	environment.wetHF = 0.30f;
	environment.underwater = true;
	return environment;
}

tone::ToneEnvironmentBands RoomEnvironment() {
	tone::ToneEnvironmentBands environment;
	environment.directLF = 1.0f;
	environment.directHF = 0.95f;
	environment.wetLF = 1.0f;
	environment.wetHF = 1.0f;
	environment.underwater = false;
	return environment;
}

// A looping weapon sound added on the listener's own entity, such as the
// gauntlet fire loop. Loops carry CHAN_AUTO, so the entity channel implies
// nothing about the class.
tone::ToneClassInputs SelfWeaponLoop() {
	tone::ToneClassInputs inputs;
	inputs.sampleName = "sound/weapons/melee/fstrun.wav";
	inputs.listenerAttached = true;
	return inputs;
}

bool ListenerAttachedLoopsAreDiegetic() {
	const tone::SoundToneClass soundClass = tone::ClassifyToneClass( SelfWeaponLoop() );

	CHECK( soundClass == tone::SoundToneClass::Self );
	CHECK( !tone::ToneClassIsNonDiegetic( soundClass ) );
	return true;
}

bool NonDiegeticAudioStaysOutsideTheLevel() {
	tone::ToneClassInputs uiSound;
	uiSound.localOnly = true;
	CHECK( tone::ClassifyToneClass( uiSound ) == tone::SoundToneClass::Local );

	tone::ToneClassInputs announcer;
	announcer.channelClass = tone::SoundToneClass::Announcer;
	CHECK( tone::ClassifyToneClass( announcer ) == tone::SoundToneClass::Announcer );

	// Feedback samples stay non-diegetic even when they ride the listener's own
	// entity on a world channel.
	tone::ToneClassInputs feedback;
	feedback.sampleName = "sound/feedback/hit.wav";
	feedback.listenerAttached = true;
	CHECK( tone::ClassifyToneClass( feedback ) == tone::SoundToneClass::Local );

	CHECK( tone::ToneClassIsNonDiegetic( tone::SoundToneClass::Local ) );
	CHECK( tone::ToneClassIsNonDiegetic( tone::SoundToneClass::Announcer ) );
	CHECK( !tone::ToneClassIsNonDiegetic( tone::SoundToneClass::Self ) );
	CHECK( !tone::ToneClassIsNonDiegetic( tone::SoundToneClass::World ) );
	CHECK( !tone::ToneClassIsNonDiegetic( tone::SoundToneClass::Weapon ) );
	CHECK( !tone::ToneClassIsNonDiegetic( tone::SoundToneClass::Underwater ) );
	return true;
}

bool WorldClassificationIsUnchanged() {
	tone::ToneClassInputs remoteWeapon;
	remoteWeapon.sampleName = "sound/weapons/rocket/rocklf1a.wav";
	remoteWeapon.positionalSource = true;
	CHECK( tone::ClassifyToneClass( remoteWeapon ) == tone::SoundToneClass::Weapon );

	tone::ToneClassInputs remoteItem;
	remoteItem.sampleName = "sound/items/respawn1.wav";
	remoteItem.positionalSource = true;
	CHECK( tone::ClassifyToneClass( remoteItem ) == tone::SoundToneClass::Item );

	tone::ToneClassInputs ambient;
	ambient.sampleName = "sound/world/water1.wav";
	ambient.positionalSource = true;
	CHECK( tone::ClassifyToneClass( ambient ) == tone::SoundToneClass::World );

	tone::ToneClassInputs submergedAmbient = ambient;
	submergedAmbient.underwater = true;
	CHECK( tone::ClassifyToneClass( submergedAmbient ) == tone::SoundToneClass::Underwater );

	tone::ToneClassInputs channelWeapon;
	channelWeapon.channelClass = tone::SoundToneClass::Weapon;
	channelWeapon.listenerAttached = true;
	CHECK( tone::ClassifyToneClass( channelWeapon ) == tone::SoundToneClass::Weapon );
	return true;
}

bool AuthoredStereoKeepsItsPathUntilItIsSpatialized() {
	tone::ToneClassInputs music;
	music.stereoSample = true;
	CHECK( tone::ClassifyToneClass( music ) == tone::SoundToneClass::Stereo );
	CHECK( tone::ToneClassKeepsAuthoredPath( tone::SoundToneClass::Stereo, false ) );

	tone::ToneClassInputs surround;
	surround.stereoSample = true;
	surround.multichannelSample = true;
	CHECK( tone::ClassifyToneClass( surround ) == tone::SoundToneClass::Multichannel );
	CHECK( tone::ToneClassKeepsAuthoredPath( tone::SoundToneClass::Multichannel, false ) );

	// s_alSpatializeStereo turns a two-channel world sample into a world source,
	// which has to follow the environment like any other world source.
	CHECK( !tone::ToneClassKeepsAuthoredPath( tone::SoundToneClass::Stereo, true ) );
	CHECK( !tone::ToneClassKeepsAuthoredPath( tone::SoundToneClass::World, false ) );
	CHECK( !tone::ToneClassKeepsAuthoredPath( tone::SoundToneClass::Self, false ) );
	return true;
}

bool SubmergedListenerMufflesItsOwnSounds() {
	const tone::ToneEnvironmentBands environment = UnderwaterEnvironment();
	const tone::ToneBands self = tone::ShapeToneBands( tone::SoundToneClass::Self, environment, environment.directHF, environment.wetHF, 0.0f );
	const tone::ToneBands world = tone::ShapeToneBands( tone::SoundToneClass::Underwater, environment, environment.directHF, environment.wetHF, 0.0f );

	CHECK( self.bandPass );
	CHECK( Near( self.directLF, 1.0f - tone::kToneUnderwaterLowCut ) );
	CHECK( Near( self.directHF, environment.directHF * ( 1.0f - tone::kToneUnderwaterHighCut ) ) );
	CHECK( self.directHF < 0.5f );
	CHECK( Near( self.sendLF, tone::kToneUnderwaterSendLowCeiling ) );
	CHECK( Near( self.sendHF, environment.wetHF * ( 1.0f - tone::kToneUnderwaterHighCut ) ) );

	// The listener's own audio is muffled no less than the world around it.
	CHECK( self.directHF <= world.directHF + 0.0001f );
	CHECK( self.directLF <= world.directLF + 0.0001f );
	return true;
}

bool SubmergedWeaponsAndSelfMatchTheMedium() {
	const tone::ToneEnvironmentBands environment = UnderwaterEnvironment();
	const tone::ToneBands ownFireLoop = tone::ShapeToneBands( tone::SoundToneClass::Self, environment, environment.directHF, environment.wetHF, 0.0f );
	const tone::ToneBands remoteFire = tone::ShapeToneBands( tone::SoundToneClass::Weapon, environment, environment.directHF, environment.wetHF, 0.0f );

	// The weapon class keeps its transient crack against the ambient HF cut, but
	// the medium still wins: both land on the same underwater ceiling.
	CHECK( Near( ownFireLoop.directHF, remoteFire.directHF ) );
	CHECK( ownFireLoop.bandPass && remoteFire.bandPass );
	return true;
}

bool NonDiegeticAudioIgnoresTheMediumAndOcclusion() {
	const tone::ToneEnvironmentBands environment = UnderwaterEnvironment();
	const tone::ToneBands local = tone::ShapeToneBands( tone::SoundToneClass::Local, environment, environment.directHF, environment.wetHF, 1.0f );
	const tone::ToneBands announcer = tone::ShapeToneBands( tone::SoundToneClass::Announcer, environment, environment.directHF, environment.wetHF, 1.0f );

	CHECK( !local.bandPass );
	CHECK( Near( local.directHF, 1.0f ) );
	CHECK( Near( local.directLF, 1.0f - tone::kToneLocalLowCut ) );
	CHECK( !announcer.bandPass );
	CHECK( Near( announcer.directHF, 1.0f ) );
	CHECK( Near( announcer.directLF, 1.0f - tone::kToneAnnouncerLowCut ) );
	return true;
}

bool DryRoomsLeaveFirstPersonAudioAlone() {
	const tone::ToneEnvironmentBands environment = RoomEnvironment();
	const tone::ToneBands self = tone::ShapeToneBands( tone::SoundToneClass::Self, environment, environment.directHF, environment.wetHF, 0.0f );
	const tone::ToneBands local = tone::ShapeToneBands( tone::SoundToneClass::Local, environment, environment.directHF, environment.wetHF, 0.0f );

	// Out of the water the listener's own sounds keep exactly the dry local
	// character they had before they became their own class.
	CHECK( Near( self.directLF, local.directLF ) );
	CHECK( Near( self.directHF, local.directHF ) );
	CHECK( Near( self.sendLF, local.sendLF ) );
	CHECK( Near( self.sendHF, local.sendHF ) );
	CHECK( !self.bandPass );
	return true;
}

bool StrongOcclusionStillThinsTheLowEnd() {
	const tone::ToneEnvironmentBands environment = RoomEnvironment();
	const tone::ToneBands open = tone::ShapeToneBands( tone::SoundToneClass::World, environment, environment.directHF, environment.wetHF, tone::kToneStrongOcclusionThreshold - 0.01f );
	const tone::ToneBands blocked = tone::ShapeToneBands( tone::SoundToneClass::World, environment, environment.directHF, environment.wetHF, 1.0f );

	CHECK( !open.bandPass );
	CHECK( Near( open.directLF, environment.directLF ) );
	CHECK( blocked.bandPass );
	CHECK( Near( blocked.directLF, 1.0f - tone::kToneStrongOcclusionLowCut ) );
	CHECK( Near( blocked.sendLF, 1.0f - tone::kToneStrongOcclusionSendLowCut ) );
	return true;
}

bool ClassNamesStayStableForDiagnostics() {
	CHECK( std::strcmp( tone::SoundToneClassName( tone::SoundToneClass::Self ), "self" ) == 0 );
	CHECK( std::strcmp( tone::SoundToneClassName( tone::SoundToneClass::Local ), "local" ) == 0 );
	CHECK( std::strcmp( tone::SoundToneClassName( tone::SoundToneClass::Underwater ), "underwater" ) == 0 );
	CHECK( std::strcmp( tone::SoundToneClassName( tone::SoundToneClass::Neutral ), "neutral" ) == 0 );
	return true;
}

struct TestCase {
	const char *name;
	bool ( *run )();
};

} // namespace

int main() {
	const TestCase tests[] = {
		{ "ListenerAttachedLoopsAreDiegetic", ListenerAttachedLoopsAreDiegetic },
		{ "NonDiegeticAudioStaysOutsideTheLevel", NonDiegeticAudioStaysOutsideTheLevel },
		{ "WorldClassificationIsUnchanged", WorldClassificationIsUnchanged },
		{ "AuthoredStereoKeepsItsPathUntilItIsSpatialized", AuthoredStereoKeepsItsPathUntilItIsSpatialized },
		{ "SubmergedListenerMufflesItsOwnSounds", SubmergedListenerMufflesItsOwnSounds },
		{ "SubmergedWeaponsAndSelfMatchTheMedium", SubmergedWeaponsAndSelfMatchTheMedium },
		{ "NonDiegeticAudioIgnoresTheMediumAndOcclusion", NonDiegeticAudioIgnoresTheMediumAndOcclusion },
		{ "DryRoomsLeaveFirstPersonAudioAlone", DryRoomsLeaveFirstPersonAudioAlone },
		{ "StrongOcclusionStillThinsTheLowEnd", StrongOcclusionStillThinsTheLowEnd },
		{ "ClassNamesStayStableForDiagnostics", ClassNamesStayStableForDiagnostics }
	};

	for ( const TestCase &test : tests ) {
		if ( !test.run() ) {
			std::fprintf( stderr, "%s failed\n", test.name );
			return 1;
		}
		std::printf( "%s passed\n", test.name );
	}
	return 0;
}
