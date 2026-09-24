/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's sound-effect engine against the behaviour read out of retail
 * alice.exe (code/client/audio/shared/AliceSound.h names each routine). A
 * fake device stands in for the backends and records what the engine asks of
 * it; every sound-info file here is written by the test, and no retail file
 * is read. */

#include "../code/client/audio/shared/AliceSound.h"

#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace sfx = fntech3::audio::alice::sfx;

namespace {

bool Near( float a, float b, float epsilon = 1e-4f ) { return std::fabs( a - b ) < epsilon; }

struct FakeLog : sfx::Log, sfx::InfoLog {
	std::vector<std::string> lines;
	void Print( const std::string &text ) override { lines.push_back( text ); }
	void Developer( const std::string &text ) override { lines.push_back( text ); }
	bool Said( const char *part ) const {
		for ( const auto &l : lines ) if ( l.find( part ) != std::string::npos ) return true;
		return false;
	}
};

struct FakeDevice : sfx::Device {
	struct SampleRecord {
		std::string name;
		bool loaded = true;
		int frames = 22050;
		int rate = 22050;
		/* The file's own frames and bytes a frame, which a resampling
		 * device's frames above need not match. */
		int sourceFrames = 22050;
		int frameBytes = 2;
	};
	struct VoiceRecord {
		int sfx = 0;
		bool playing = false;
		bool paused = false;
		bool loop = false;
		int offset = 0;
		int starts = 0;
		int stops = 0;
		float position[3] = { 0, 0, 0 };
		float gain = -1.0f;
		float direct = 1.0f;
		int pan = -1;
		/* The loop block the voice holds, or -1 without one; how many were
		 * handed over; and whether the last came before the voice started. */
		int blockStart = -1;
		int blockEnd = -1;
		int blocks = 0;
		bool blockBeforeStart = false;
	};
	std::vector<SampleRecord> samples;
	std::array<VoiceRecord, sfx::AllVoices> voices{};
	bool refusePrepare = false;

	FakeDevice() {
		samples.push_back( { "***DEFAULT***", false, 0, 0, 0, 0 } );
	}
	int Add( const std::string &name, int frames = 22050, int rate = 22050 ) {
		samples.push_back( { name, true, frames, rate, frames, 2 } );
		return static_cast<int>( samples.size() - 1 );
	}
	/* A file the device resampled: it plays `frames` of the file's
	 * `sourceFrames`. */
	int AddResampled( const std::string &name, int frames, int rate, int sourceFrames, int frameBytes ) {
		samples.push_back( { name, true, frames, rate, sourceFrames, frameBytes } );
		return static_cast<int>( samples.size() - 1 );
	}

	bool Sample( int sfx, sfx::SampleFacts &facts ) const override {
		if ( sfx < 0 || sfx >= static_cast<int>( samples.size() ) ) return false;
		const SampleRecord &s = samples[static_cast<std::size_t>( sfx )];
		facts.loaded = s.loaded;
		facts.frames = s.frames;
		facts.rate = s.rate;
		facts.durationMs = s.rate > 0 ? static_cast<float>( s.frames ) * 1000.0f / static_cast<float>( s.rate ) : 0.0f;
		facts.name = s.name;
		facts.sourceFrames = s.sourceFrames;
		facts.frameBytes = s.frameBytes;
		return true;
	}
	bool Playing( int voice ) const override {
		return voices[static_cast<std::size_t>( voice )].playing && !voices[static_cast<std::size_t>( voice )].paused;
	}
	bool Prepare( int voice, int sfx ) override {
		if ( refusePrepare ) return false;
		VoiceRecord &v = voices[static_cast<std::size_t>( voice )];
		v.sfx = sfx;
		v.playing = false;
		v.paused = false;
		/* A new sample drops the block, as AIL_set_3D_sample_file does. */
		v.blockStart = v.blockEnd = -1;
		return true;
	}
	void LoopBlock( int voice, int startFrame, int endFrame ) override {
		VoiceRecord &v = voices[static_cast<std::size_t>( voice )];
		v.blockStart = startFrame;
		v.blockEnd = endFrame;
		v.blockBeforeStart = !v.playing;
		++v.blocks;
	}
	void Start( int voice, bool loop, int offset ) override {
		VoiceRecord &v = voices[static_cast<std::size_t>( voice )];
		v.playing = true;
		v.loop = loop;
		v.offset = offset;
		++v.starts;
	}
	void Stop( int voice ) override {
		VoiceRecord &v = voices[static_cast<std::size_t>( voice )];
		if ( v.playing || v.paused ) ++v.stops;
		v.playing = false;
		v.paused = false;
	}
	void Pause( int voice ) override {
		if ( Playing( voice ) ) voices[static_cast<std::size_t>( voice )].paused = true;
	}
	void Resume( int voice ) override { voices[static_cast<std::size_t>( voice )].paused = false; }
	void Place( int voice, const float *position, float gain, float direct, int pan ) override {
		VoiceRecord &v = voices[static_cast<std::size_t>( voice )];
		std::memcpy( v.position, position, sizeof( v.position ) );
		v.gain = gain;
		v.direct = direct;
		v.pan = pan;
	}
	/* A sample running out by itself. */
	void Finish( int voice ) { voices[static_cast<std::size_t>( voice )].playing = false; }
};

struct FakeWorld : sfx::World {
	bool blocked = false;
	int traces = 0;
	bool Blocked( const float *, const float *, int ) override {
		++traces;
		return blocked;
	}
};

constexpr float kIdentity[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
constexpr float kOrigin[3] = { 0, 0, 0 };

sfx::Frame At( int serverTime, int realtime = 0 ) {
	sfx::Frame frame;
	frame.serverTime = serverTime;
	frame.realtime = realtime;
	return frame;
}

int VoiceOf( const sfx::Engine &engine, int sfxHandle ) {
	for ( int i = 0; i < sfx::Voices; ++i ) {
		if ( engine.VoiceAt( i ).sfx == sfxHandle ) return i;
	}
	return -1;
}

int VoiceOnChannel( const sfx::Engine &engine, int entchannel ) {
	for ( int i = 0; i < sfx::Voices; ++i ) {
		if ( engine.VoiceAt( i ).entchannel == entchannel ) return i;
	}
	return -1;
}

/* ---- sound infos -------------------------------------------------------- */

void CheckSoundInfos() {
	FakeLog log;
	sfx::SoundInfoTable table;
	table.Begin();
	table.Parse(
		"defaultmindist 200\n"
		"defaultvolume 0.5\n"
		"sound test/a.wav\n"
		"mindist\t100\n"
		"maxdist 9000\n"
		"sound test/b.wav volume 0.25 loopstart 10 loopend 20 maxnumber -1 maxfactor 2 localsound\n"
		"sound test/c.wav mindist -3 volume 1.5\n"
		"unknownword sound test/d.wav\n", log );
	table.End();
	CHECK( table.Count() == 5 );
	const sfx::SoundInfo &a = table.For( "TEST/A.WAV" );
	CHECK( a.name == "test/a.wav" && Near( a.minDist, 100.0f ) && Near( a.volume, 0.5f ) );
	CHECK( a.maxNumber == sfx::DefaultMaxNumber && a.loopStart == -1 && !a.local );
	const sfx::SoundInfo &b = table.For( "test/b.wav" );
	CHECK( Near( b.minDist, 200.0f ) && Near( b.volume, 0.25f ) && b.loopStart == 10 && b.loopEnd == 20 );
	CHECK( b.maxNumber == -1 && Near( b.maxFactor, 2.0f ) && b.local );
	/* Out-of-range values fall back to the defaults then current. */
	const sfx::SoundInfo &c = table.For( "test/c.wav" );
	CHECK( Near( c.minDist, 200.0f ) && Near( c.volume, 0.5f ) );
	CHECK( log.Said( "Minimum distance (-3.000000) out of range for sound test/c.wav" ) );
	CHECK( log.Said( "Volume (1.500000) out of range for sound test/c.wav" ) );
	/* An unknown word is passed over. */
	CHECK( table.Find( "test/d.wav" ) == 4 );
	/* Anything else is record 0, which ends with the defaults as they stand
	 * and no name. */
	CHECK( table.Find( "other.wav" ) == 0 && table.Find( "" ) == 0 && table.Find( nullptr ) == 0 );
	CHECK( Near( table.At( 0 ).minDist, 200.0f ) && Near( table.At( 0 ).volume, 0.5f ) && table.At( 0 ).name.empty() );

	/* A keyword before the first `sound` sets record 0; its min distance and
	 * volume are then overwritten, and the rest stays. Defaults out of range
	 * go back to retail's own. */
	FakeLog log2;
	sfx::SoundInfoTable first;
	first.Begin();
	first.Parse( "localsound maxnumber 2 mindist 50 defaultmindist -1 defaultvolume 3\n", log2 );
	first.End();
	CHECK( first.At( 0 ).local && first.At( 0 ).maxNumber == 2 );
	CHECK( Near( first.At( 0 ).minDist, 160.0f ) && Near( first.At( 0 ).volume, 1.0f ) );
	CHECK( log2.Said( "Default minimum distance (-1.000000) out of range" ) && log2.Said( "Default volume (3.000000) out of range" ) );

	/* The defaults and the count carry from one file to the next; the first
	 * match wins. */
	sfx::SoundInfoTable files;
	files.Begin();
	files.Parse( "defaultmindist 80 sound x.wav\n", log );
	files.Parse( "sound y.wav sound X.WAV mindist 5\n", log );
	files.End();
	CHECK( Near( files.For( "y.wav" ).minDist, 80.0f ) && Near( files.For( "x.wav" ).minDist, 80.0f ) );

	/* A thousand records at most, record 0 included; the rest of that file
	 * is abandoned, the next file still read. */
	std::string many;
	for ( int i = 0; i < 1005; ++i ) many += "sound s" + std::to_string( i ) + ".wav\n";
	FakeLog log3;
	sfx::SoundInfoTable full;
	full.Begin();
	full.Parse( many.c_str(), log3 );
	full.Parse( "sound late.wav\n", log3 );
	full.End();
	CHECK( full.Count() == sfx::MaxInfos && log3.Said( "Too many sound infos specified" ) );
	CHECK( full.Find( "s998.wav" ) == 999 && full.Find( "s999.wav" ) == 0 && full.Find( "late.wav" ) == 0 );
}

/* ---- the arithmetic ----------------------------------------------------- */

void CheckArithmetic() {
	/* Full volume inside the min distance, min over distance to 64 times it,
	 * silence from there. */
	CHECK( Near( sfx::Gain( 160.0f, 160.0f, 1.0f ), 1.0f ) );
	CHECK( Near( sfx::Gain( 320.0f, 160.0f, 0.8f ), 0.4f ) );
	CHECK( sfx::Gain( 160.0f * 64.0f - 1.0f, 160.0f, 1.0f ) > 0.0f );
	CHECK( sfx::Gain( 160.0f * 64.0f, 160.0f, 1.0f ) == 0.0f );
	/* A pending one-shot is dropped at 16 x its min distance. */
	CHECK( !sfx::CullAtStart( 160.0f * 16.0f - 0.5f, 160.0f * 64.0f ) );
	CHECK( sfx::CullAtStart( 160.0f * 16.0f, 160.0f * 64.0f ) );

	sfx::SoundInfo plain, local;
	local.local = true;
	CHECK( sfx::IsLocal( sfx::MenuChannel, -1.0f, plain ) );
	CHECK( sfx::IsLocal( 6, sfx::LocalMinDist, plain ) );
	CHECK( sfx::IsLocal( 1, -1.0f, local ) );
	CHECK( !sfx::IsLocal( 1, 30100.0f, plain ) );
	/* Any negative argument is a one-shot's default; only -1 is a loop's. */
	CHECK( sfx::ResolveOneShot( -0.5f, 7.0f ) == 7.0f && sfx::ResolveLoop( -0.5f, 7.0f ) == -0.5f );
	CHECK( sfx::ResolveLoop( -1.0f, 7.0f ) == 7.0f );

	/* The client game's listener axis is AnglesToAxis(pitch, -yaw, roll);
	 * transposed it is the true forward, left and up for any yaw. */
	const float yaw = 0.7f;
	const float cgame[3][3] = {
		{ std::cos( -yaw ), std::sin( -yaw ), 0.0f },
		{ -std::sin( -yaw ), std::cos( -yaw ), 0.0f },
		{ 0.0f, 0.0f, 1.0f } };
	float listener[3][3];
	sfx::RetailListenerAxis( cgame, listener );
	CHECK( Near( listener[0][0], std::cos( yaw ) ) && Near( listener[0][1], std::sin( yaw ) ) );
	CHECK( Near( listener[1][0], -std::sin( yaw ) ) && Near( listener[1][1], std::cos( yaw ) ) );
	CHECK( Near( listener[2][2], 1.0f ) );

	/* A sound 90 units under the listener from its own entity is the
	 * listener. */
	const sfx::Vec3 at{ 10, 20, 100 };
	CHECK( sfx::SnapToListener( 3, 3, sfx::Vec3{ 10, 20, 10 }, at ) );
	CHECK( !sfx::SnapToListener( 4, 3, sfx::Vec3{ 10, 20, 10 }, at ) );
	CHECK( !sfx::SnapToListener( 3, 3, sfx::Vec3{ 10.5f, 20, 10 }, at ) );
	CHECK( !sfx::SnapToListener( 3, 3, sfx::Vec3{ 10, 20, 0 }, at ) );

	/* The fade: from one to nothing over its length, then held; 0 ends it. */
	sfx::Fade fade;
	CHECK( Near( fade.Master( 0.8f ), 0.8f ) );
	fade.Start( 1000.0f, 5000 );
	fade.Update( 5500 );
	CHECK( Near( fade.Master( 0.8f ), 0.4f ) );
	fade.Update( 9000 );
	CHECK( Near( fade.Master( 0.8f ), 0.0f ) );
	fade.Start( 0.0f, 9000 );
	CHECK( Near( fade.Master( 0.8f ), 0.8f ) );

	CHECK( sfx::LoopPhase( 500, 22050, 22050 ) == 11025 && sfx::LoopPhase( 1500, 22050, 22050 ) == 11025 );
	CHECK( sfx::LoopPhase( 0, 22050, 100 ) == 0 && sfx::LoopPhase( -5, 22050, 100 ) == 0 );

	/* The EAX 1.0 environment volumes the software provider scales by. */
	CHECK( Near( sfx::kEaxRoomVolume[0], 0.5f ) && Near( sfx::kEaxRoomVolume[18], 1.0f ) && Near( sfx::kEaxRoomVolume[25], 0.486f ) );
	CHECK( Near( sfx::ReverbLevel( 2, 0.5f ), 0.2085f ) && sfx::ReverbLevel( 26, 1.0f ) == 0.0f && sfx::ReverbLevel( -1, 1.0f ) == 0.0f );
	CHECK( !std::strcmp( sfx::NearestReverbPreset( 22 ), "underwater" ) && !std::strcmp( sfx::NearestReverbPreset( 5 ), "stone-room" ) );
	CHECK( sfx::NearestReverbPreset( 26 ) == nullptr );
}

/* ---- one-shots ---------------------------------------------------------- */

void CheckOneShots() {
	FakeDevice device;
	FakeWorld world;
	FakeLog log;
	sfx::SoundInfoTable infos;
	sfx::Engine engine;
	engine.SetInfos( &infos );
	engine.Reset( 1.0f );
	/* Ten seconds long. */
	const int step = device.Add( "sound/step.wav", 220500 );
	const float far[3] = { 3000.0f, 0.0f, 0.0f };
	const float near[3] = { 320.0f, 0.0f, 0.0f };

	/* A world sound waits for the next respatialize, then starts where it
	 * can be heard: at 320 units, half of a min distance of 160. */
	engine.StartSound( device, log, near, 1023, 1, step, -1.0f, -1.0f, At( 1000 ) );
	const int v = VoiceOf( engine, step );
	CHECK( v == 0 && engine.VoiceAt( v ).state == sfx::VoiceState::Pending && device.voices[0].starts == 0 );
	/* A fixed origin is kept as entity 0. */
	CHECK( engine.VoiceAt( v ).fixed && engine.VoiceAt( v ).entnum == 0 );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 1000 ) );
	CHECK( device.voices[0].starts == 1 && device.voices[0].playing );
	/* Miles holds 127 and the distance halves it. */
	CHECK( Near( device.voices[0].gain, 0.5f ) );
	/* At the listener there is nothing in the way; the one trace a second
	 * was made. */
	CHECK( world.traces == 1 && Near( device.voices[0].direct, 1.0f ) );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 1500 ) );
	CHECK( world.traces == 1 );
	world.blocked = true;
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 2000 ) );
	CHECK( world.traces == 2 && Near( device.voices[0].direct, sfx::ObstructionGain ) );
	/* Once obstructed, the sound stays so. */
	world.blocked = false;
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 3000 ) );
	CHECK( Near( device.voices[0].direct, sfx::ObstructionGain ) );

	/* Too far away when it would start: dropped, never heard. */
	engine.StartSound( device, log, far, 1023, 2, step, -1.0f, -1.0f, At( 3000 ) );
	CHECK( engine.VoiceAt( 1 ).state == sfx::VoiceState::Pending );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 3000 ) );
	CHECK( engine.VoiceAt( 1 ).state == sfx::VoiceState::Free && device.voices[1].starts == 0 );

	/* A new sound on the same voice is not obstructed until measured. */
	device.Finish( 0 );
	engine.StartSound( device, log, near, 1023, 1, step, -1.0f, -1.0f, At( 3100 ) );
	CHECK( VoiceOf( engine, step ) == 0 && !engine.VoiceAt( 0 ).obstructed );

	/* Past its length and a quarter second a one-shot is ended. */
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 3100 ) );
	CHECK( device.voices[0].playing && engine.VoiceAt( 0 ).endTime == 3100 + 10000 + sfx::EndMarginMs );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 13350 ) );
	CHECK( device.voices[0].playing );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 13351 ) );
	CHECK( !device.voices[0].playing );
}

void CheckChannels() {
	FakeDevice device;
	FakeWorld world;
	FakeLog log;
	sfx::SoundInfoTable infos;
	sfx::Engine engine;
	engine.SetInfos( &infos );
	engine.Reset( 1.0f );
	std::vector<int> sounds;
	for ( int i = 0; i < 40; ++i ) sounds.push_back( device.Add( "sound/s" + std::to_string( i ) + ".wav" ) );
	const float here[3] = { 10.0f, 0.0f, 0.0f };

	/* A non-zero channel takes over the same entity's channel. */
	engine.StartSound( device, log, here, 5, 3, sounds[0], -1.0f, -1.0f, At( 0 ) );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 0 ) );
	engine.StartSound( device, log, here, 5, 3, sounds[1], -1.0f, -1.0f, At( 0 ) );
	CHECK( engine.VoiceAt( 0 ).sfx == sounds[1] && device.voices[0].stops == 1 );
	/* Channel 0 never does. */
	engine.StartSound( device, log, here, 5, 0, sounds[2], -1.0f, -1.0f, At( 0 ) );
	engine.StartSound( device, log, here, 5, 0, sounds[3], -1.0f, -1.0f, At( 0 ) );
	CHECK( engine.VoiceAt( 1 ).sfx == sounds[2] && engine.VoiceAt( 2 ).sfx == sounds[3] );

	/* A fixed-origin sound is stored as entity 0, so entity 0 on the same
	 * channel takes it over and stopping entity 0's channel stops it. */
	engine.StartSound( device, log, here, 1023, 4, sounds[4], -1.0f, -1.0f, At( 0 ) );
	const int fixedVoice = VoiceOf( engine, sounds[4] );
	engine.StartSound( device, log, here, 0, 4, sounds[5], -1.0f, -1.0f, At( 0 ) );
	CHECK( VoiceOf( engine, sounds[5] ) == fixedVoice );
	engine.StopSound( device, log, 0, 4, At( 0 ) );
	CHECK( engine.VoiceAt( fixedVoice ).state == sfx::VoiceState::Free );

	/* Nothing is stolen: with every voice busy a sound is dropped. */
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 0 ) );
	for ( int i = 6; i < 40; ++i ) engine.StartSound( device, log, here, 100 + i, 1, sounds[static_cast<std::size_t>( i )], -1.0f, -1.0f, At( 0 ) );
	int busy = 0;
	for ( int i = 0; i < sfx::Voices; ++i ) busy += engine.VoiceAt( i ).state != sfx::VoiceState::Free;
	CHECK( busy == sfx::Voices );
	CHECK( VoiceOf( engine, sounds[39] ) < 0 );

	/* No more than maxnumber of one sound, pending ones counted. */
	FakeDevice capped;
	sfx::Engine limited;
	limited.SetInfos( &infos );
	limited.Reset( 1.0f );
	const int loud = capped.Add( "sound/loud.wav" );
	for ( int i = 0; i < 8; ++i ) limited.StartSound( capped, log, here, 200 + i, 1, loud, -1.0f, -1.0f, At( 0 ) );
	int copies = 0;
	for ( int i = 0; i < sfx::Voices; ++i ) copies += limited.VoiceAt( i ).sfx == loud;
	CHECK( copies == sfx::DefaultMaxNumber );

	/* The menu channel starts at once, at the listener, at s_volume without
	 * the fade. */
	FakeDevice menu;
	sfx::Engine ui;
	ui.SetInfos( &infos );
	ui.Reset( 1.0f );
	const int click = menu.Add( "sound/menu/click.wav" );
	sfx::Frame frame = At( 0 );
	frame.volume = 0.8f;
	frame.master = 0.4f;
	ui.StartSound( menu, log, nullptr, 0, sfx::MenuChannel, click, -1.0f, -1.0f, frame );
	CHECK( menu.voices[0].starts == 1 && Near( menu.voices[0].gain, 101.0f / 127.0f ) );
	/* Any other channel starts at the faded master volume. */
	const int bang = menu.Add( "sound/bang.wav" );
	ui.StartSound( menu, log, nullptr, 0, 1, bang, -1.0f, -1.0f, frame );
	CHECK( Near( ui.VoiceAt( 1 ).applied, 50.0f / 127.0f ) );
	/* A null fixed origin, which retail faults on, is dropped. */
	ui.StartSound( menu, log, nullptr, 1023, 1, bang, -1.0f, -1.0f, frame );
	CHECK( ui.VoiceAt( 2 ).state == sfx::VoiceState::Free );
}

void CheckEntitiesAndVolume() {
	FakeDevice device;
	FakeWorld world;
	FakeLog log;
	sfx::SoundInfoTable infos;
	infos.Begin();
	infos.Parse( "sound sound/hum.wav loopstart 0 loopend 10\n", log );
	infos.End();
	sfx::Engine engine;
	engine.SetInfos( &infos );
	engine.Reset( 1.0f );
	const int step = device.Add( "sound/step.wav" );
	const int hum = device.Add( "sound/hum.wav" );
	const float given[3] = { 100.0f, 0.0f, 0.0f };
	const float moved[3] = { 200.0f, 0.0f, 0.0f };

	/* An entity's sound follows the entity once the entity is newer than the
	 * origin it was started with. */
	engine.StartSound( device, log, given, 7, 1, step, -1.0f, -1.0f, At( 1000 ) );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 1000 ) );
	CHECK( Near( device.voices[0].position[0], 100.0f ) );
	engine.UpdateEntity( 7, moved, kOrigin, false, 1000 );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 1000 ) );
	CHECK( Near( device.voices[0].position[0], 200.0f ) );
	/* An entity heard at the listener plays there. */
	engine.UpdateEntity( 7, moved, kOrigin, true, 1100 );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 1100 ) );
	CHECK( Near( device.voices[0].position[0], 0.0f ) && Near( device.voices[0].gain, 1.0f ) );

	/* A one-shot with a loop block stops once its entity goes stale. */
	engine.StartSound( device, log, given, 8, 1, hum, -1.0f, -1.0f, At( 1200 ) );
	const int looped = VoiceOf( engine, hum );
	CHECK( engine.VoiceAt( looped ).looping );
	engine.UpdateEntity( 8, given, kOrigin, false, 1200 );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 1200 ) );
	CHECK( device.voices[static_cast<std::size_t>( looped )].playing && device.voices[static_cast<std::size_t>( looped )].loop );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 1300 ) );
	CHECK( !device.voices[static_cast<std::size_t>( looped )].playing );

	/* s_volume changing rescales what plays by the master volume against the
	 * last one; the fade alone does not. */
	sfx::Frame frame = At( 2000 );
	engine.StartSound( device, log, given, 1023, 2, step, 0.5f, -1.0f, frame );
	const int half = VoiceOnChannel( engine, 2 );
	CHECK( half >= 0 );
	CHECK( Near( engine.VoiceAt( half ).applied, 63.0f / 127.0f ) );
	frame.master = 0.5f;
	engine.Update( device, log, false, false, frame );
	CHECK( Near( engine.VoiceAt( half ).applied, 63.0f / 127.0f ) );
	frame.volume = 0.5f;
	engine.Update( device, log, false, true, frame );
	CHECK( Near( engine.VoiceAt( half ).factor, 0.25f ) && Near( engine.VoiceAt( half ).applied, 31.0f / 127.0f ) );
	/* A start at zero volume keeps 0.8 as its factor. */
	frame.volume = frame.master = 0.0f;
	engine.StartSound( device, log, given, 1023, 3, step, -1.0f, -1.0f, frame );
	const int silent = VoiceOnChannel( engine, 3 );
	CHECK( silent >= 0 && Near( engine.VoiceAt( silent ).factor, 0.8f ) && engine.VoiceAt( silent ).applied == 0.0f );

	/* Pausing stops what plays where it is; unpausing resumes only that. */
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 2000 ) );
	engine.Update( device, log, true, false, At( 2000 ) );
	CHECK( engine.VoiceAt( half ).state == sfx::VoiceState::Paused && device.voices[static_cast<std::size_t>( half )].paused );
	engine.Update( device, log, false, false, At( 2000 ) );
	CHECK( engine.VoiceAt( half ).state == sfx::VoiceState::Started && !device.voices[static_cast<std::size_t>( half )].paused );

	/* Stopping everything ends every voice. */
	engine.StopAll( device );
	for ( int i = 0; i < sfx::Voices; ++i ) CHECK( !device.voices[static_cast<std::size_t>( i )].playing );
}

/* ---- loops -------------------------------------------------------------- */

void CheckLoops() {
	FakeDevice device;
	FakeWorld world;
	FakeLog log;
	sfx::SoundInfoTable infos;
	infos.Begin();
	infos.Parse( "sound sound/fire.wav maxfactor 1.5\n", log );
	infos.End();
	sfx::Engine engine;
	engine.SetInfos( &infos );
	engine.Reset( 1.0f );
	const int water = device.Add( "sound/water.wav" );
	const int fire = device.Add( "sound/fire.wav" );
	const float still[3] = { 0, 0, 0 };
	const float left[3] = { 0.0f, 320.0f, 0.0f };
	const float ahead[3] = { 160.0f, 0.0f, 0.0f };

	/* Two emitters of one sound are one group on one voice: the leader's
	 * volume and distance serve both, the gains add, and the voice sits at
	 * half the min distance toward the louder. */
	engine.ClearLoopingSounds();
	engine.AddLoopingSound( device, log, ahead, still, water, 0.5f, -1.0f, At( 0, 100 ) );
	engine.AddLoopingSound( device, log, left, still, water, 1.0f, 40.0f, At( 0, 100 ) );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 0, 100 ) );
	const int voice = engine.LoopAt( 0 ).voice;
	CHECK( voice >= 0 && engine.LoopAt( 0 ).playing && engine.LoopAt( 1 ).playing && engine.LoopAt( 1 ).voice == voice );
	CHECK( device.voices[static_cast<std::size_t>( voice )].loop && device.voices[static_cast<std::size_t>( voice )].starts == 1 );
	/* 0.5 at 160 units and 0.5 x 160/320 = 0.25 at 320: 0.75 in all. */
	CHECK( Near( engine.LoopAt( 0 ).gain, 0.75f ) );
	CHECK( Near( device.voices[static_cast<std::size_t>( voice )].gain, 95.0f / 127.0f ) );
	const float *p = device.voices[static_cast<std::size_t>( voice )].position;
	CHECK( Near( std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] ), 80.0f, 1e-3f ) );
	CHECK( p[0] > p[1] && p[1] > 0.0f );
	/* The voice is entity 0, channel 0, so no one-shot takes it over. */
	CHECK( engine.VoiceAt( voice ).entnum == 0 && engine.VoiceAt( voice ).entchannel == 0 && engine.VoiceAt( voice ).loopOwned );

	/* The ambient level scales stationary loops. */
	sfx::Frame quiet = At( 0, 200 );
	quiet.ambient = 0.5f;
	engine.ClearLoopingSounds();
	engine.AddLoopingSound( device, log, ahead, still, water, 0.5f, -1.0f, quiet );
	engine.AddLoopingSound( device, log, left, still, water, 1.0f, 40.0f, quiet );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, quiet );
	CHECK( Near( engine.LoopAt( 0 ).gain, 0.375f ) );

	/* Water, not submitted this frame, is reaped: its voice ends. */
	engine.ClearLoopingSounds();
	engine.Update( device, log, false, false, At( 0, 300 ) );
	CHECK( !device.voices[static_cast<std::size_t>( voice )].playing );
	CHECK( engine.LoopAt( 0 ).sfx == 0 && !engine.LoopAt( 0 ).playing && engine.LoopAt( 1 ).sfx == 0 );

	/* maxfactor caps a group at that many times its loudest member. */
	for ( int i = 0; i < 3; ++i ) engine.AddLoopingSound( device, log, still, still, fire, -1.0f, -1.0f, At( 0, 300 ) );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 0, 300 ) );
	int fireRecord = -1;
	for ( int i = 0; i < sfx::Loops; ++i ) if ( engine.LoopAt( i ).sfx == fire && fireRecord < 0 ) fireRecord = i;
	CHECK( fireRecord >= 0 && Near( engine.LoopAt( fireRecord ).gain, 1.5f ) );
	/* A group at the listener is placed on it, at full volume. */
	const int fireVoice = engine.LoopAt( fireRecord ).voice;
	CHECK( fireVoice >= 0 && Near( device.voices[static_cast<std::size_t>( fireVoice )].gain, 1.0f ) );
	CHECK( Near( device.voices[static_cast<std::size_t>( fireVoice )].position[0], 0.0f ) );

	/* A group out of reach is not started; a record that was never playing
	 * stays until a submission reuses it. */
	FakeDevice device2;
	sfx::Engine far;
	far.SetInfos( &infos );
	far.Reset( 1.0f );
	const int drip = device2.Add( "sound/drip.wav" );
	const float away[3] = { 160.0f * 64.0f + 1.0f, 0.0f, 0.0f };
	far.ClearLoopingSounds();
	far.AddLoopingSound( device2, log, away, still, drip, -1.0f, -1.0f, At( 0 ) );
	far.Respatialize( device2, world, log, 0, kOrigin, kIdentity, At( 0 ) );
	CHECK( !far.LoopAt( 0 ).playing && device2.voices[0].starts == 0 );
	far.ClearLoopingSounds();
	far.Update( device2, log, false, false, At( 0 ) );
	CHECK( far.LoopAt( 0 ).sfx == drip );
	/* Stopping everything clears the records. */
	far.StopAll( device2 );
	CHECK( far.LoopAt( 0 ).sfx == 0 );

	/* A sample with no frames is never a loop. */
	FakeDevice empty;
	sfx::Engine none;
	none.Reset( 1.0f );
	const int silent = empty.Add( "sound/silent.wav", 0 );
	none.AddLoopingSound( empty, log, still, still, silent, -1.0f, -1.0f, At( 0 ) );
	CHECK( none.LoopAt( 0 ).sfx == 0 );

	/* Past 64 records a sound is refused, said once a second. */
	FakeDevice crowded;
	sfx::Engine busy;
	busy.Reset( 1.0f );
	FakeLog crowdLog;
	for ( int i = 0; i < 70; ++i ) {
		const int handle = crowded.Add( "sound/l" + std::to_string( i ) + ".wav" );
		busy.AddLoopingSound( crowded, crowdLog, still, still, handle, -1.0f, -1.0f, At( 0, 5000 ) );
	}
	int said = 0;
	for ( const auto &line : crowdLog.lines ) said += line == "Too many looping sounds\n";
	CHECK( said == 1 );
}

/* ---- loop blocks -------------------------------------------------------- */

void CheckLoopBlockArithmetic() {
	/* A thousand frames of 16-bit mono: two bytes a frame. */
	sfx::SampleFacts pcm16;
	pcm16.frames = pcm16.sourceFrames = 1000;
	pcm16.frameBytes = 2;
	sfx::LoopBlock block;
	CHECK( sfx::ResolveLoopBlock( 200, 1200, pcm16, block ) && block.start == 100 && block.end == 600 );
	/* Each end goes to the nearest whole frame, halves up, as Miles's
	 * software provider rounds the start. */
	CHECK( sfx::ResolveLoopBlock( 201, 1201, pcm16, block ) && block.start == 101 && block.end == 601 );
	CHECK( sfx::ResolveLoopBlock( 199, 1199, pcm16, block ) && block.start == 100 && block.end == 600 );
	/* An end of -1, any other negative one, and any at or past the data's
	 * end are the end of the sample. */
	CHECK( sfx::ResolveLoopBlock( 200, -1, pcm16, block ) && block.start == 100 && block.end == 1000 );
	CHECK( sfx::ResolveLoopBlock( 200, -7, pcm16, block ) && block.end == 1000 );
	CHECK( sfx::ResolveLoopBlock( 200, 1999, pcm16, block ) && block.end == 1000 );
	CHECK( sfx::ResolveLoopBlock( 200, INT_MAX, pcm16, block ) && block.end == 1000 );
	/* A block that is the whole sample is no block at all. */
	CHECK( !sfx::ResolveLoopBlock( 0, -1, pcm16, block ) && block.start == 0 && block.end == 1000 );
	CHECK( !sfx::ResolveLoopBlock( 0, 2000, pcm16, block ) );
	/* A negative start is the sample's start. */
	CHECK( sfx::ResolveLoopBlock( -5, 600, pcm16, block ) && block.start == 0 && block.end == 300 );
	CHECK( sfx::ResolveLoopBlock( INT_MIN, 600, pcm16, block ) && block.start == 0 && block.end == 300 );
	/* A block that is empty or inverted once rounded, or starts past the
	 * end -- which retail's provider would repeat at no length or read past
	 * the data for -- is the whole sample. */
	CHECK( !sfx::ResolveLoopBlock( 600, 600, pcm16, block ) && block.start == 0 && block.end == 1000 );
	CHECK( !sfx::ResolveLoopBlock( 601, 600, pcm16, block ) );
	CHECK( !sfx::ResolveLoopBlock( 800, 400, pcm16, block ) );
	CHECK( !sfx::ResolveLoopBlock( 2500, -1, pcm16, block ) );
	CHECK( !sfx::ResolveLoopBlock( INT_MAX, INT_MAX, pcm16, block ) );
	/* An 8-bit file counts a byte a frame. */
	sfx::SampleFacts pcm8 = pcm16;
	pcm8.frameBytes = 1;
	CHECK( sfx::ResolveLoopBlock( 201, 601, pcm8, block ) && block.start == 201 && block.end == 601 );
	/* A device that resampled the file counts its own frames: a 44.1 kHz file
	 * on a 22.05 kHz mixer has half as many, and one at 11.025 kHz twice. */
	sfx::SampleFacts halved = pcm16;
	halved.frames = 500;
	CHECK( sfx::ResolveLoopBlock( 200, 1200, halved, block ) && block.start == 50 && block.end == 300 );
	sfx::SampleFacts doubled = pcm16;
	doubled.frames = 2000;
	CHECK( sfx::ResolveLoopBlock( 201, 1201, doubled, block ) && block.start == 202 && block.end == 1202 );
	/* A block that vanishes at the device's rate is the whole sample. */
	sfx::SampleFacts tiny = pcm16;
	tiny.frames = 1;
	CHECK( !sfx::ResolveLoopBlock( 200, 1200, tiny, block ) && block.start == 0 && block.end == 1 );
	/* Without the file's layout nothing can be counted. */
	sfx::SampleFacts unknown = pcm16;
	unknown.frameBytes = 0;
	CHECK( !sfx::ResolveLoopBlock( 200, 1200, unknown, block ) );
	unknown = pcm16;
	unknown.sourceFrames = 0;
	CHECK( !sfx::ResolveLoopBlock( 200, 1200, unknown, block ) );

	/* Through the start to the block's end, then round the block. */
	CHECK( sfx::LoopBlockOffset( 0, 100, 600 ) == 0 && sfx::LoopBlockOffset( 599, 100, 600 ) == 599 );
	CHECK( sfx::LoopBlockOffset( 600, 100, 600 ) == 100 && sfx::LoopBlockOffset( 1099, 100, 600 ) == 599 );
	CHECK( sfx::LoopBlockOffset( 1100, 100, 600 ) == 100 && sfx::LoopBlockOffset( 1234, 100, 600 ) == 234 );
	CHECK( sfx::LoopBlockOffset( 7, 0, 7 ) == 0 && sfx::LoopBlockOffset( 9, 6, 7 ) == 6 );
	/* The mixer's clock is unsigned; the far end of it still lands inside. */
	CHECK( sfx::LoopBlockOffset( 0xffffffffu, 3, 10 ) == 3 + static_cast<int>( ( 0xffffffffull - 10 ) % 7 ) );
}

void CheckLoopBlocks() {
	FakeDevice device;
	FakeWorld world;
	FakeLog log;
	sfx::SoundInfoTable infos;
	infos.Begin();
	infos.Parse(
		"sound sound/hum.wav loopstart 2000 loopend 6000\n"
		"sound sound/whole.wav loopstart 0\n"
		"sound sound/broken.wav loopstart 900 loopend 100\n"
		"sound sound/far.wav loopstart 2000 loopend 6000\n", log );
	infos.End();
	sfx::Engine engine;
	engine.SetInfos( &infos );
	engine.Reset( 1.0f );
	const int hum = device.Add( "sound/hum.wav" );
	const int whole = device.Add( "sound/whole.wav" );
	const int broken = device.Add( "sound/broken.wav" );
	const int plain = device.Add( "sound/plain.wav" );
	/* 44.1 kHz in the file, played at 22.05 kHz. */
	const int far = device.AddResampled( "sound/far.wav", 11025, 22050, 22050, 2 );
	const float here[3] = { 10.0f, 0.0f, 0.0f };

	/* The record's block reaches the voice before it starts, in the sample's
	 * frames, and the voice then loops from its first frame. */
	engine.StartSound( device, log, here, 1023, 1, hum, -1.0f, -1.0f, At( 0 ) );
	const int humVoice = VoiceOf( engine, hum );
	CHECK( humVoice >= 0 && engine.VoiceAt( humVoice ).looping );
	const FakeDevice::VoiceRecord &h = device.voices[static_cast<std::size_t>( humVoice )];
	CHECK( h.blocks == 1 && h.blockStart == 1000 && h.blockEnd == 3000 && h.blockBeforeStart && h.starts == 0 );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 0 ) );
	CHECK( h.starts == 1 && h.loop && h.offset == 0 );

	/* A block that is the whole sample, and one that is inverted, loop the
	 * whole sample with no block handed over. */
	engine.StartSound( device, log, here, 1023, 2, whole, -1.0f, -1.0f, At( 0 ) );
	engine.StartSound( device, log, here, 1023, 3, broken, -1.0f, -1.0f, At( 0 ) );
	for ( const int handle : { whole, broken } ) {
		const int voice = VoiceOf( engine, handle );
		CHECK( voice >= 0 && engine.VoiceAt( voice ).looping && device.voices[static_cast<std::size_t>( voice )].blocks == 0 );
	}
	/* Without a loopstart the sound plays once, with no block. */
	engine.StartSound( device, log, here, 1023, 4, plain, -1.0f, -1.0f, At( 0 ) );
	const int plainVoice = VoiceOf( engine, plain );
	CHECK( plainVoice >= 0 && !engine.VoiceAt( plainVoice ).looping && device.voices[static_cast<std::size_t>( plainVoice )].blocks == 0 );

	/* A resampled file's block is counted in the device's frames. */
	engine.StartSound( device, log, here, 1023, 5, far, -1.0f, -1.0f, At( 0 ) );
	const int farVoice = VoiceOf( engine, far );
	CHECK( farVoice >= 0 );
	const FakeDevice::VoiceRecord &f = device.voices[static_cast<std::size_t>( farVoice )];
	CHECK( f.blocks == 1 && f.blockStart == 500 && f.blockEnd == 1500 );

	/* A fixed origin is stored as entity 0, so entity 0's channel 1 takes the
	 * voice over; with the new sample the block goes. */
	engine.StartSound( device, log, here, 0, 1, plain, -1.0f, -1.0f, At( 0 ) );
	CHECK( h.sfx == plain && h.blockStart == -1 && h.blocks == 1 );
	CHECK( engine.VoiceAt( humVoice ).sfx == plain && !engine.VoiceAt( humVoice ).looping );

	/* The menu channel starts at once, its block already handed over. */
	FakeDevice menu;
	sfx::Engine ui;
	ui.SetInfos( &infos );
	ui.Reset( 1.0f );
	const int menuHum = menu.Add( "sound/hum.wav" );
	ui.StartSound( menu, log, nullptr, 0, sfx::MenuChannel, menuHum, -1.0f, -1.0f, At( 0 ) );
	const FakeDevice::VoiceRecord &m = menu.voices[0];
	CHECK( m.blocks == 1 && m.blockBeforeStart && m.starts == 1 && m.loop && m.blockStart == 1000 );

	/* A loop group loops the whole sample: retail's loop mixer sets up its
	 * handle with no block (0x004704e4, 0x00470619). */
	FakeDevice loops;
	sfx::Engine grouped;
	grouped.SetInfos( &infos );
	grouped.Reset( 1.0f );
	const int loopHum = loops.Add( "sound/hum.wav" );
	const float still[3] = { 0, 0, 0 };
	grouped.ClearLoopingSounds();
	grouped.AddLoopingSound( loops, log, here, still, loopHum, -1.0f, -1.0f, At( 0 ) );
	grouped.Respatialize( loops, world, log, 0, kOrigin, kIdentity, At( 0 ) );
	const int groupVoice = grouped.LoopAt( 0 ).voice;
	CHECK( groupVoice >= 0 && loops.voices[static_cast<std::size_t>( groupVoice )].loop );
	CHECK( loops.voices[static_cast<std::size_t>( groupVoice )].blocks == 0 );
}

/* ---- dialogs ------------------------------------------------------------ */

void CheckDialogs() {
	FakeDevice device;
	FakeWorld world;
	FakeLog log;
	sfx::SoundInfoTable infos;
	sfx::Engine engine;
	engine.SetInfos( &infos );
	engine.Reset( 1.0f );
	const int line = device.Add( "sound/dialog/line.mp3" );
	const float speaker[3] = { 320.0f, 0.0f, 0.0f };
	engine.UpdateEntity( 4, speaker, kOrigin, false, 0 );

	/* A sound whose name says .mp3 takes a dialog voice, and its distance
	 * over the min distance scales it with no far limit. */
	engine.StartSound( device, log, nullptr, 4, 6, line, -1.0f, -1.0f, At( 0 ) );
	CHECK( engine.DialogAt( 0 ).state == sfx::VoiceState::Pending && VoiceOf( engine, line ) < 0 );
	engine.Respatialize( device, world, log, 0, kOrigin, kIdentity, At( 0 ) );
	CHECK( device.voices[sfx::Voices].starts == 1 && Near( device.voices[sfx::Voices].gain, 63.0f / 127.0f ) );
	CHECK( device.voices[sfx::Voices].pan >= 0 );
	/* Four at most. */
	for ( int i = 0; i < 4; ++i ) engine.StartSound( device, log, nullptr, 4, 6, line, -1.0f, -1.0f, At( 0 ) );
	CHECK( log.Said( "Could not find a free dialog channel" ) );
}

} // namespace

int main() {
	CheckSoundInfos();
	CheckArithmetic();
	CheckOneShots();
	CheckChannels();
	CheckEntitiesAndVolume();
	CheckLoops();
	CheckLoopBlockArithmetic();
	CheckLoopBlocks();
	CheckDialogs();
	std::cout << "alice sound tests passed\n";
	return 0;
}
