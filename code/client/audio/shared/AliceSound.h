/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Independently authored from retail American McGee's Alice observations. */
#ifndef FNTECH3_AUDIO_ALICE_SOUND_H
#define FNTECH3_AUDIO_ALICE_SOUND_H

/*
 * American McGee's Alice's sound effects, as retail alice.exe plays them
 * through Miles (client-game imports 30 to 41).
 *
 *   S_StartSound 0x0046fa50      PickChannel 0x0046f7c0   StartDialog 0x0046f860
 *   S_StopSound 0x00471340       AddLoopingSound 0x0046fe70
 *   S_Respatialize 0x00470790    loop mixer 0x00470010    S_Update 0x00471090
 *   StopAllSounds 0x004713f0     sound infos 0x0044da50   S_FadeSound 0x00472530
 *
 * Thirty-two voices carry the one-shots and the looping groups alike; four
 * more carry sounds whose names say `.mp3`, in two dimensions. A one-shot
 * waits for the next respatialize to start, and is dropped there if it is
 * already too far away to hear. Looping sounds are submitted afresh every
 * frame, merged by sound into one voice placed where the group is loudest,
 * and reaped once the client game stops submitting them.
 *
 * Nothing here touches a device: an `Engine` decides what plays where and how
 * loud, and hands each decision to a `Device` the backend supplies. See
 * docs/fntech3/2026-09-23-alice-audio.md.
 */

#include "AliceMusic.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace fntech3::audio::alice::sfx {

/* 32 3-D handles at 0x00aaa280, 0x44 bytes apart (0x0046e685). */
inline constexpr int Voices = 32;
/* Four 2-D dialog handles at 0x00aa8f10, 0x1c bytes apart. */
inline constexpr int DialogVoices = 4;
inline constexpr int AllVoices = Voices + DialogVoices;
/* 64 loop records at 0x00aa8f80, 0x4c bytes apart. */
inline constexpr int Loops = 64;
/* Retail's own sfx cap (0x0044ca45); fnTech3 keeps its larger table. */
inline constexpr int MaxSfx = 700;
/* The sound-info table's count stops at 1,000 records, defaults included
 * (0x0044db71). */
inline constexpr int MaxInfos = 1000;
inline constexpr float DefaultMinDist = 160.0f;
inline constexpr float DefaultVolume = 1.0f;
inline constexpr int DefaultMaxNumber = 5;
inline constexpr float DefaultMaxFactor = -1.0f;
/* max distance = min distance x 64 (0x00500288). */
inline constexpr float MaxDistanceScale = 64.0f;
/* A pending one-shot starts only inside a quarter of its max distance
 * (0x005002c0, 0x00471510): 16 x min distance. */
inline constexpr float StartCullScale = 0.25f;
/* A min distance of exactly this plays at the listener (0x0050028c). */
inline constexpr float LocalMinDist = 30101.0f;
/* CHAN_MENU: local, starts at once, ignores the fade. */
inline constexpr int MenuChannel = 9;
/* A one-shot is ended this long after its own length (0x00500278). */
inline constexpr int EndMarginMs = 250;
/* AIL_set_3D_sample_obstruction's one value (0x00470cb4). */
inline constexpr float ObstructionGain = 0.5f;
/* A sound 90 units straight below the listener, from the listener's own
 * entity, is the listener (0x005002b8, 0x005002b0). */
inline constexpr float ListenerSnapLow = 89.9f;
inline constexpr float ListenerSnapHigh = 90.1f;
/* S_Update halves the ambient loops while a key catcher is up (0x0047109d). */
inline constexpr float KeyCatcherAmbient = 0.5f;
/* Entity numbers 1023 (ENTITYNUM_NONE) and -1 mean a fixed origin. */
inline constexpr int FixedOriginEntity = 0x3ff;
/* The entity sound table has 1024 records; import 39 accepts 1024 too. */
inline constexpr int Entities = 1024;
/* The 2-D pan of a dialog at the listener (0x00470e70). */
inline constexpr int CentrePan = 64;
inline constexpr int ReverbTypes = 26;
/* The EAX 1.0 environment volumes, 26 floats at 0x0051f944, which the
 * software provider scales a reverb level by (0x00470f50). */
inline constexpr float kEaxRoomVolume[ReverbTypes] = {
	0.5f, 0.25f, 0.417f, 0.653f, 0.208f, 0.5f, 0.403f, 0.5f, 0.5f, 0.361f, 0.5f, 0.153f, 0.361f,
	0.444f, 0.25f, 0.111f, 0.111f, 0.194f, 1.0f, 0.097f, 0.208f, 0.652f, 1.0f, 0.875f, 0.139f, 0.486f
};

/* The nearest of the OpenAL backend's own reverb presets for each EAX
 * environment -- GENERIC, PADDEDCELL, ROOM, BATHROOM, LIVINGROOM, STONEROOM,
 * AUDITORIUM, CONCERTHALL, CAVE, ARENA, HANGAR, CARPETEDHALLWAY, HALLWAY,
 * STONECORRIDOR, ALLEY, FOREST, CITY, MOUNTAINS, QUARRY, PLAIN, PARKINGLOT,
 * SEWERPIPE, UNDERWATER, DRUGGED, DIZZY, PSYCHOTIC -- chosen by the kind of
 * space. An approximation: the tree carries seven presets, not EAX's 26. */
inline const char *NearestReverbPreset( int type ) {
	static constexpr const char *kPresets[ReverbTypes] = {
		"room", "small-room", "small-room", "stone-room", "small-room", "stone-room",
		"hall", "hall", "hall", "hall", "hall", "hallway", "hallway",
		"hallway", "outdoors", "outdoors", "outdoors", "outdoors", "outdoors", "outdoors", "outdoors",
		"hallway", "underwater", "hall", "hall", "hall"
	};
	return type >= 0 && type < ReverbTypes ? kPresets[type] : nullptr;
}

/* The software provider's reverb send for one environment (0x00470f50). */
inline float ReverbLevel( int type, float level ) {
	return type >= 0 && type < ReverbTypes ? level * kEaxRoomVolume[type] : 0.0f;
}

using Vec3 = std::array<float, 3>;

inline Vec3 ToVec3( const float *v ) {
	return v ? Vec3{ v[0], v[1], v[2] } : Vec3{};
}

inline float Distance( const Vec3 &a, const Vec3 &b ) {
	const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
	return std::sqrt( x * x + y * y + z * z );
}

/* ---- sound infos ------------------------------------------------------ */

/* One record of 0x00b84ec0, 92 bytes. */
struct SoundInfo {
	std::string name;
	float minDist = DefaultMinDist;
	float volume = DefaultVolume;
	/* The loop block, bytes into the sample's data (+0x48, +0x4c): a
	 * loopstart of -1 plays the sound once; see ResolveLoopBlock. */
	int loopStart = -1;
	int loopEnd = -1;
	/* Negative is unlimited. */
	int maxNumber = DefaultMaxNumber;
	/* Used only at 1 or more. */
	float maxFactor = DefaultMaxFactor;
	bool local = false;
};

struct InfoLog {
	virtual ~InfoLog() = default;
	virtual void Print( const std::string &text ) = 0;
	virtual void Developer( const std::string &text ) = 0;
};

/* 0x0044da50: global/sound0.txt to global/sound9.txt, read as one run. The
 * defaults and the record count carry from file to file; a keyword before a
 * file's first `sound` sets the record then current, which is record 0 until
 * a `sound` appears; and once the last file is read, record 0 takes the
 * default min distance and volume as they stand and loses its name. */
class SoundInfoTable {
public:
	SoundInfoTable() {
		Begin();
		End();
	}

	void Begin() {
		infos_.assign( 1, SoundInfo{} );
		index_.clear();
		defaultMinDist_ = DefaultMinDist;
		defaultVolume_ = DefaultVolume;
	}

	/* One file. Its tokens are read with Ritual's rules, crossing lines;
	 * an unknown word is passed over. The retail loader's own Script class
	 * was not read, so its comment and quoting rules are taken to be the
	 * engine tokenizer's. */
	void Parse( const char *text, InfoLog &log ) {
		Tokens tokens( text ? text : "" );
		const auto next = [&tokens]( std::string &out ) {
			out = tokens.Next( true );
			return !out.empty();
		};
		std::string word, argument;
		while ( next( word ) ) {
			const char *w = word.c_str();
			if ( !CompareNoCase( w, "sound" ) ) {
				if ( !next( argument ) ) continue;
				if ( static_cast<int>( infos_.size() ) >= MaxInfos ) {
					/* The rest of this file is abandoned (0x0044e04c). */
					log.Developer( "Too many sound infos specified\n" );
					return;
				}
				SoundInfo info;
				info.name = argument.size() > 63 ? argument.substr( 0, 63 ) : argument;
				info.minDist = defaultMinDist_;
				info.volume = defaultVolume_;
				index_.emplace( Lower( info.name ), static_cast<int>( infos_.size() ) );
				infos_.push_back( info );
			} else if ( !CompareNoCase( w, "mindist" ) ) {
				if ( !next( argument ) ) continue;
				const double value = std::atof( argument.c_str() );
				Current().minDist = static_cast<float>( value );
				if ( value < 0.0 ) {
					Current().minDist = defaultMinDist_;
					log.Developer( Format( "Minimum distance (%f) out of range for sound %s\n", value, Current().name.c_str() ) );
				}
			} else if ( !CompareNoCase( w, "maxdist" ) || !CompareNoCase( w, "defaultmaxdist" ) ) {
				/* Read and thrown away (0x0044dca7, 0x0044df90). */
				next( argument );
			} else if ( !CompareNoCase( w, "volume" ) ) {
				if ( !next( argument ) ) continue;
				const double value = std::atof( argument.c_str() );
				Current().volume = static_cast<float>( value );
				if ( value < 0.0 || value > 1.0 ) {
					Current().volume = defaultVolume_;
					log.Developer( Format( "Volume (%f) out of range for sound %s\n", value, Current().name.c_str() ) );
				}
			} else if ( !CompareNoCase( w, "loopstart" ) ) {
				if ( next( argument ) ) Current().loopStart = std::atoi( argument.c_str() );
			} else if ( !CompareNoCase( w, "loopend" ) ) {
				if ( next( argument ) ) Current().loopEnd = std::atoi( argument.c_str() );
			} else if ( !CompareNoCase( w, "maxnumber" ) ) {
				if ( next( argument ) ) Current().maxNumber = std::atoi( argument.c_str() );
			} else if ( !CompareNoCase( w, "maxfactor" ) ) {
				if ( next( argument ) ) Current().maxFactor = static_cast<float>( std::atof( argument.c_str() ) );
			} else if ( !CompareNoCase( w, "localsound" ) ) {
				Current().local = true;
			} else if ( !CompareNoCase( w, "defaultmindist" ) ) {
				if ( !next( argument ) ) continue;
				const double value = std::atof( argument.c_str() );
				defaultMinDist_ = static_cast<float>( value );
				if ( value < 0.0 ) {
					log.Developer( Format( "Default minimum distance (%f) out of range\n", value, "" ) );
					defaultMinDist_ = DefaultMinDist;
				}
			} else if ( !CompareNoCase( w, "defaultvolume" ) ) {
				if ( !next( argument ) ) continue;
				const double value = std::atof( argument.c_str() );
				defaultVolume_ = static_cast<float>( value );
				if ( value < 0.0 || value > 1.0 ) {
					log.Developer( Format( "Default volume (%f) out of range\n", value, "" ) );
					defaultVolume_ = DefaultVolume;
				}
			}
		}
	}

	void End() {
		infos_[0].minDist = defaultMinDist_;
		infos_[0].volume = defaultVolume_;
		infos_[0].name.clear();
	}

	/* 0x0044cbd2: the first record whose name matches, ignoring case, or
	 * record 0. */
	int Find( const char *name ) const {
		if ( !name || !*name ) return 0;
		const auto found = index_.find( Lower( name ) );
		return found != index_.end() ? found->second : 0;
	}

	const SoundInfo &At( int index ) const {
		return index >= 0 && index < static_cast<int>( infos_.size() ) ? infos_[static_cast<std::size_t>( index )] : infos_[0];
	}

	const SoundInfo &For( const char *name ) const { return At( Find( name ) ); }
	int Count() const { return static_cast<int>( infos_.size() ); }

private:
	SoundInfo &Current() { return infos_.back(); }

	static std::string Lower( const std::string &text ) {
		std::string out = text;
		for ( char &c : out ) {
			if ( c >= 'A' && c <= 'Z' ) c = static_cast<char>( c - 'A' + 'a' );
		}
		return out;
	}

	static std::string Format( const char *format, double value, const char *name ) {
		char buffer[256];
		std::snprintf( buffer, sizeof( buffer ), format, value, name );
		return buffer;
	}

	std::vector<SoundInfo> infos_;
	std::unordered_map<std::string, int> index_;
	float defaultMinDist_ = DefaultMinDist;
	float defaultVolume_ = DefaultVolume;
};

/* ---- the retail arithmetic -------------------------------------------- */

/* The engine's own loop gain (0x00470010): the volume inside the min
 * distance, falling as min over distance, and nothing at 64 times it. Miles
 * attenuated one-shots itself with the same two distances and no rolloff
 * call; this curve stands in for its own. */
inline float Gain( float distance, float minDist, float volume ) {
	if ( distance <= minDist ) return volume;
	if ( distance >= minDist * MaxDistanceScale ) return 0.0f;
	return volume * minDist / distance;
}

/* A pending one-shot that far away is dropped rather than started. */
inline bool CullAtStart( float distance, float maxDist ) {
	return !( distance < maxDist * StartCullScale );
}

inline bool IsLocal( int channel, float minDist, const SoundInfo &info ) {
	return info.local || minDist == LocalMinDist || channel == MenuChannel;
}

/* A one-shot takes its record's value for any negative argument; a loop
 * only for exactly -1. */
inline float ResolveOneShot( float value, float fallback ) { return value < 0.0f ? fallback : value; }
inline float ResolveLoop( float value, float fallback ) { return value == -1.0f ? fallback : value; }

/* The client game builds the listener axis from (pitch, -yaw, roll) and the
 * engine reads its columns as forward and up (0x00470816): its transpose is
 * the listener the backends understand, exact for yaw and approximate in
 * pitch as retail is. */
inline void RetailListenerAxis( const float in[3][3], float out[3][3] ) {
	for ( int i = 0; i < 3; ++i ) {
		for ( int j = 0; j < 3; ++j ) out[i][j] = in[j][i];
	}
}

inline bool SnapToListener( int entnum, int listener, const Vec3 &position, const Vec3 &at ) {
	if ( entnum != listener || position[0] != at[0] || position[1] != at[1] ) return false;
	const float below = at[2] - position[2];
	return below > ListenerSnapLow && below < ListenerSnapHigh;
}

/* A loop starts where it would be had it played since it was added,
 * counted in the sample's own frames (retail counts in the output's). */
inline int LoopPhase( int elapsedMs, int rate, int frames ) {
	if ( elapsedMs <= 0 || rate <= 0 || frames <= 0 ) return 0;
	return static_cast<int>( ( static_cast<long long>( elapsedMs ) * rate / 1000 ) % frames );
}

/* 0x00472530 and the factor S_Update keeps (0x004710ef). */
struct Fade {
	bool active = false;
	float factor = 1.0f;
	float start = 0.0f;
	float end = 0.0f;

	void Start( float ms, int now ) {
		if ( ms == 0.0f ) {
			active = false;
			return;
		}
		active = true;
		factor = 1.0f;
		start = static_cast<float>( now );
		end = start + ms;
	}
	void Update( int now ) {
		if ( !active ) return;
		factor = 1.0f - ( static_cast<float>( now ) - start ) / ( end - start );
		if ( factor < 0.0f ) factor = 0.0f;
	}
	/* 0x00472580. */
	float Master( float volume ) const { return active ? factor * volume : volume; }
};

/* The 2-D pan of a dialog: 0 left, 127 right, spread by s_separation
 * (0x004724a0). */
inline int DialogPan( const Vec3 &position, const Vec3 &listener, const float right[3], float separation ) {
	Vec3 direction{ position[0] - listener[0], position[1] - listener[1], position[2] - listener[2] };
	const float length = Distance( position, listener );
	if ( length > 0.0f ) {
		for ( float &c : direction ) c /= length;
	}
	const float dot = direction[0] * right[0] + direction[1] * right[1] + direction[2] * right[2];
	float scale = dot * ( 1.0f - separation ) + separation;
	if ( scale < 0.0f ) scale = 0.0f;
	return static_cast<int>( scale * 127.0f );
}

/* ---- the engine --------------------------------------------------------- */

struct SampleFacts {
	/* The sample has data (retail +0x0c). */
	bool loaded = false;
	/* Its frames (+0x04) as the device plays them; a loop of none is
	 * refused. */
	int frames = 0;
	int rate = 0;
	/* Its length in milliseconds (+0x58). */
	float durationMs = 0.0f;
	std::string name;
	/* The file's own frames, which a resampling device has more or fewer
	 * of, and the bytes each takes in the file (its sample width times its
	 * channels): a loop block counts bytes of the file. */
	int sourceFrames = 0;
	int frameBytes = 0;
};

/* A stretch of a sample's frames, [start, end). */
struct LoopBlock {
	int start = 0;
	int end = 0;
};

/* A sound-info record's loop block as S_StartSound hands it to Miles
 * (0x0046fd79 to 0x0046fd9d): the sample's offset goes to 0, then
 * AIL_set_3D_sample_loop_block takes the record's loopstart and loopend and a
 * loop count of 0 follows, so the sample plays from its start to the block's
 * end and then repeats the block for as long as it sounds. The two are byte
 * offsets into the sample's data. Miles's software provider
 * (SndDrivers/mssfast.m3d) rounds the start to the nearest whole sample
 * (0x22601fb0), and treats an end of -1, or any end at or past the data's
 * end, as the end of the sample (an unsigned compare, 0x22601b07 to
 * 0x22601b11); setting a sample's data resets the block to the whole sample
 * (0x22602b28 to 0x22602b2e).
 *
 * Here both ends go to the nearest whole frame of the file, then to the
 * device's frames. What retail's provider would read outside the data for is
 * bounded instead: a negative start is the sample's start, and a block that
 * is empty or inverted once rounded -- which the provider would repeat at
 * zero length for ever, or read past the data for -- is the whole sample.
 * True when that leaves a block smaller than the whole sample. */
inline bool ResolveLoopBlock( int startBytes, int endBytes, const SampleFacts &facts, LoopBlock &block ) {
	block = LoopBlock{ 0, facts.frames };
	if ( facts.frameBytes <= 0 || facts.sourceFrames <= 0 || facts.frames <= 0 ) return false;
	const long long frameBytes = facts.frameBytes;
	const long long total = facts.sourceFrames;
	const auto nearest = [frameBytes]( int bytes ) {
		return ( static_cast<long long>( bytes ) + frameBytes / 2 ) / frameBytes;
	};
	const long long start = startBytes < 0 ? 0 : nearest( startBytes );
	const long long end = endBytes < 0 ? total : ( std::min )( nearest( endBytes ), total );
	if ( start >= end ) return false;
	const long long first = start * facts.frames / total;
	const long long last = end * facts.frames / total;
	if ( first >= last ) return false;
	block.start = static_cast<int>( first );
	block.end = static_cast<int>( last );
	return block.start > 0 || block.end < facts.frames;
}

/* Where a voice looping a block [blockStart, blockEnd) is once `elapsed` of
 * its frames have played: through its start to the block's end, then round
 * the block. The block must be well formed (0 <= blockStart < blockEnd). */
inline int LoopBlockOffset( unsigned elapsed, int blockStart, int blockEnd ) {
	const unsigned end = static_cast<unsigned>( blockEnd );
	if ( elapsed < end ) return static_cast<int>( elapsed );
	return blockStart + static_cast<int>( ( elapsed - end ) % static_cast<unsigned>( blockEnd - blockStart ) );
}

/* What a backend does for the engine. Voices 0 to 31 are the 3-D ones and 32
 * to 35 the dialog ones. */
class Device {
public:
	virtual ~Device() = default;
	/* False for a handle the backend never registered. */
	virtual bool Sample( int sfx, SampleFacts &facts ) const = 0;
	/* AIL's "playing" status: false once a sample has ended or is paused. */
	virtual bool Playing( int voice ) const = 0;
	/* Hands the voice a sample (AIL_set_3D_sample_file); false on failure.
	 * Any loop block the voice had goes with it. */
	virtual bool Prepare( int voice, int sfx ) = 0;
	/* The prepared voice, once started looping, plays to `endFrame` and then
	 * repeats [startFrame, endFrame) (AIL_set_3D_sample_loop_block), in the
	 * frames SampleFacts gives; called only for a block smaller than the
	 * whole sample. A device that cannot loops the whole sample instead. */
	virtual void LoopBlock( int voice, int startFrame, int endFrame ) = 0;
	virtual void Start( int voice, bool loop, int offsetFrames ) = 0;
	virtual void Stop( int voice ) = 0;
	virtual void Pause( int voice ) = 0;
	virtual void Resume( int voice ) = 0;
	/* Where a voice is heard and how loud: `gain` includes the master volume
	 * and the distance, `direct` is the obstruction of the direct path, and
	 * `pan` is a dialog's 2-D pan (0 to 127) or -1 for a 3-D voice. */
	virtual void Place( int voice, const float *position, float gain, float direct, int pan ) = 0;
};

/* The one trace the engine makes: from a sound to the listener, a point, the
 * world model, CONTENTS_SOLID. True when it is blocked. */
struct World {
	virtual ~World() = default;
	virtual bool Blocked( const float *from, const float *to, int listener ) = 0;
};

struct Log {
	virtual ~Log() = default;
	virtual void Print( const std::string &text ) = 0;
	virtual void Developer( const std::string &text ) = 0;
};

/* What the engine reads of the client and the cvars at each call. */
struct Frame {
	int serverTime = 0;     /* cl.serverTime (0x00c309cc) */
	int realtime = 0;       /* cls.realtime (0x00c239e8) */
	float volume = 1.0f;    /* s_volume */
	float master = 1.0f;    /* s_volume, times the fade while one runs */
	float ambient = 1.0f;   /* 0x00519564 */
	float separation = 0.3f;
	bool developer = false;
	bool showSounds = false;
};

enum class VoiceState { Free = 0, Pending = 1, Started = 2, Paused = 3 };

/* One 3-D channel, 0x44 bytes at 0x00aaa280. Positions are game axes;
 * retail keeps Miles's, which only reorder and negate them. */
struct Voice {
	VoiceState state = VoiceState::Free;
	int sfx = 0;
	/* Stored as 0 for a fixed-origin sound. */
	int entnum = 0;
	int entchannel = 0;
	bool fixed = false;
	Vec3 position{};
	/* +0x24: the volume factor, 0.8 where the start volume was 0. */
	float factor = 0.0f;
	/* The volume Miles holds, over 127. */
	float applied = 0.0f;
	float maxDist = 0.0f;
	float time = 0.0f;
	bool loopOwned = false;
	bool looping = false;
	float nextOcclusion = 0.0f;
	int endTime = 0;
	bool local = false;
	float durationMs = 0.0f;
	/* Retail never takes the obstruction off a handle again; here it lasts
	 * for one sound. */
	bool obstructed = false;
	/* Where the last respatialize heard it and how loud. */
	Vec3 heard{};
	float gain = 0.0f;
};

/* One 2-D dialog channel, 0x1c bytes at 0x00aa8f10. */
struct DialogVoice {
	VoiceState state = VoiceState::Free;
	int sfx = 0;
	int entnum = 0;
	int entchannel = 0;
	float volume = 0.0f;
	float minDist = 0.0f;
	bool local = false;
};

/* One loop record, 0x4c bytes at 0x00aa8f80. */
struct LoopRecord {
	Vec3 origin{};
	Vec3 velocity{};
	int sfx = 0;
	bool playing = false;
	int voice = -1;
	bool processed = false;
	float gain = 0.0f;
	int time = 0;
	bool added = false;
	bool stationary = false;
	float volume = -1.0f;
	float minDist = -1.0f;
};

/* One entity's record in the sound table at 0x00b5c460. */
struct EntitySound {
	Vec3 origin{};
	Vec3 velocity{};
	float time = 0.0f;
	bool useListener = false;
};

class Engine {
public:
	void SetInfos( const SoundInfoTable *infos ) { infos_ = infos; }

	/* The sound system starting or stopping: every record goes, and the
	 * volume the next s_volume change is measured against is the cvar's
	 * (0x0046e5cd). */
	void Reset( float volume ) {
		voices_ = {};
		dialogs_ = {};
		loops_ = {};
		entities_.assign( Entities, EntitySound{} );
		listener_ = 0;
		listenerOrigin_ = {};
		paused_ = false;
		masterOld_ = volume;
		nextTooManyLoops_ = 0;
	}

	int Listener() const { return listener_; }
	const Vec3 &ListenerOrigin() const { return listenerOrigin_; }
	const Voice &VoiceAt( int i ) const { return voices_[static_cast<std::size_t>( i )]; }
	const DialogVoice &DialogAt( int i ) const { return dialogs_[static_cast<std::size_t>( i )]; }
	const LoopRecord &LoopAt( int i ) const { return loops_[static_cast<std::size_t>( i )]; }

	/* 0x0046fa50, behind import 30's range check. */
	void StartSound( Device &device, Log &log, const float *origin, int entnum, int entchannel,
		int sfx, float volume, float minDist, const Frame &frame ) {
		SampleFacts facts;
		if ( !device.Sample( sfx, facts ) || !facts.loaded ) return;
		const SoundInfo &info = Info( facts.name );
		if ( facts.name.find( ".mp3" ) != std::string::npos ) {
			StartDialog( device, log, entnum, entchannel, sfx, facts, info, volume, minDist, frame );
			return;
		}
		/* 0x00471460: no more than maxnumber of one sound at once, pending
		 * ones included. */
		if ( info.maxNumber >= 0 ) {
			int count = 0;
			for ( int i = 0; i < Voices; ++i ) {
				if ( voices_[static_cast<std::size_t>( i )].sfx == sfx && Busy( device, i ) ) ++count;
			}
			if ( count >= info.maxNumber ) return;
		}
		const bool fixed = entnum == FixedOriginEntity || entnum == -1;
		/* Retail reads a null fixed origin and faults. */
		if ( fixed && !origin ) return;
		const int channel = Pick( device, entnum, entchannel );
		if ( channel < 0 ) {
			if ( frame.developer ) {
				log.Developer( "No free channel for " + facts.name + "\n" );
				DumpTo( device, log );
			}
			return;
		}
		Voice &voice = voices_[static_cast<std::size_t>( channel )];
		voice.entchannel = entchannel;
		if ( device.Playing( channel ) ) device.Stop( channel );
		voice.state = VoiceState::Pending;
		voice.sfx = sfx;
		voice.loopOwned = false;
		voice.looping = false;
		voice.local = IsLocal( entchannel, minDist, info );
		voice.nextOcclusion = 0.0f;
		voice.obstructed = false;
		voice.durationMs = facts.durationMs;
		if ( !device.Prepare( channel, sfx ) ) {
			log.Developer( "Set sample error - " + facts.name + "\n" );
			voice.state = VoiceState::Free;
			return;
		}
		const float resolvedMinDist = ResolveOneShot( minDist, info.minDist );
		voice.maxDist = resolvedMinDist * MaxDistanceScale;
		const float resolvedVolume = ResolveOneShot( volume, info.volume );
		const float startVolume = ( entchannel == MenuChannel ? frame.volume : frame.master ) * resolvedVolume;
		voice.applied = MilesVolume( startVolume );
		voice.factor = startVolume == 0.0f ? 0.8f : startVolume;
		if ( frame.showSounds ) {
			char line[128];
			std::snprintf( line, sizeof( line ), "%d - %s (vol %f, mindist %f)\n", frame.serverTime,
				facts.name.c_str(), static_cast<double>( startVolume ), static_cast<double>( resolvedMinDist ) );
			log.Print( line );
		}
		if ( fixed ) {
			voice.position = ToVec3( origin );
			voice.fixed = true;
			voice.entnum = 0;
		} else {
			voice.fixed = false;
			voice.entnum = entnum;
			if ( origin ) {
				voice.position = ToVec3( origin );
				voice.time = static_cast<float>( frame.serverTime - 1 > 0 ? frame.serverTime - 1 : 0 );
			} else {
				voice.time = 0.0f;
			}
		}
		if ( info.loopStart != -1 ) {
			/* Only a loopstart of exactly -1 plays once (0x0046fd84); any
			 * other hands the voice the record's block (0x0046fd8f) and
			 * loops it. */
			voice.looping = true;
			SampleFacts prepared;
			LoopBlock block;
			if ( device.Sample( sfx, prepared ) && ResolveLoopBlock( info.loopStart, info.loopEnd, prepared, block ) ) {
				device.LoopBlock( channel, block.start, block.end );
			}
		}
		if ( entchannel == MenuChannel ) {
			/* Menu sounds start now rather than at the next respatialize,
			 * which the menus may never reach. They are local, so they are
			 * placed at the listener. */
			voice.state = VoiceState::Started;
			voice.heard = listenerOrigin_;
			voice.gain = voice.applied;
			device.Place( channel, voice.heard.data(), voice.gain, 1.0f, -1 );
			device.Start( channel, voice.looping, 0 );
			voice.endTime = frame.serverTime + static_cast<int>( voice.durationMs ) + EndMarginMs;
		}
	}

	/* 0x00471340: the first 3-D channel with that entity and channel, playing
	 * or not, else the first dialog. Loops are never touched here. */
	void StopSound( Device &device, Log &log, int entnum, int channel, const Frame &frame ) {
		for ( int i = 0; i < Voices; ++i ) {
			Voice &voice = voices_[static_cast<std::size_t>( i )];
			if ( voice.entnum != entnum || voice.entchannel != channel ) continue;
			device.Stop( i );
			voice.state = VoiceState::Free;
			if ( frame.showSounds ) {
				SampleFacts facts;
				device.Sample( voice.sfx, facts );
				log.Print( std::to_string( frame.serverTime ) + " - stopped sound - " + facts.name + "\n" );
			}
			return;
		}
		for ( int i = 0; i < DialogVoices; ++i ) {
			DialogVoice &dialog = dialogs_[static_cast<std::size_t>( i )];
			if ( dialog.entnum != entnum || dialog.entchannel != channel ) continue;
			device.Stop( Voices + i );
			dialog.state = VoiceState::Free;
			return;
		}
	}

	/* 0x0046fe30: only the frame's marks go; nothing stops. */
	void ClearLoopingSounds() {
		for ( LoopRecord &loop : loops_ ) loop.added = false;
	}

	/* 0x0046fe70. A record is the sound's, not an entity's. */
	void AddLoopingSound( Device &device, Log &log, const float *origin, const float *velocity,
		int sfx, float volume, float minDist, const Frame &frame ) {
		SampleFacts facts;
		if ( !device.Sample( sfx, facts ) || facts.frames == 0 ) return;
		int found = -1;
		for ( int i = 0; i < Loops && found < 0; ++i ) {
			if ( loops_[static_cast<std::size_t>( i )].sfx == sfx && !loops_[static_cast<std::size_t>( i )].added ) found = i;
		}
		for ( int i = 0; i < Loops && found < 0; ++i ) {
			LoopRecord &loop = loops_[static_cast<std::size_t>( i )];
			if ( loop.sfx == 0 && !loop.added ) {
				loop.playing = false;
				found = i;
			}
		}
		if ( found < 0 ) {
			if ( frame.realtime >= nextTooManyLoops_ ) {
				log.Developer( "Too many looping sounds\n" );
				nextTooManyLoops_ = frame.realtime + 1000;
			}
			return;
		}
		LoopRecord &loop = loops_[static_cast<std::size_t>( found )];
		loop.origin = ToVec3( origin );
		loop.velocity = ToVec3( velocity );
		loop.sfx = sfx;
		loop.added = true;
		/* Written on every submission (0x0046ff7a), so a group's phase is
		 * the time of its leader's latest submission. */
		loop.time = frame.realtime;
		loop.volume = volume;
		loop.minDist = minDist;
		loop.stationary = loop.velocity[0] == 0.0f && loop.velocity[1] == 0.0f && loop.velocity[2] == 0.0f;
		if ( !loop.stationary ) return;
		/* A stationary record joins its sound's running group. */
		for ( const LoopRecord &other : loops_ ) {
			if ( other.sfx != sfx || !other.added ) continue;
			loop.time = other.time;
			if ( other.playing ) {
				loop.playing = true;
				loop.voice = other.voice;
			}
		}
	}

	/* 0x0044cd20 after its range check; 1024 is accepted and kept nowhere. */
	void UpdateEntity( int entnum, const float *origin, const float *velocity, bool useListener, int serverTime ) {
		if ( entnum < 0 || entnum >= Entities ) return;
		EntitySound &entity = entities_[static_cast<std::size_t>( entnum )];
		entity.origin = ToVec3( origin );
		entity.velocity = ToVec3( velocity );
		entity.time = static_cast<float>( serverTime );
		entity.useListener = useListener;
	}

	/* 0x00470790. `axis` is the listener axis as the backends take it (see
	 * RetailListenerAxis); its second row is the listener's left. */
	void Respatialize( Device &device, World &world, Log &log, int listener, const float *origin,
		const float axis[3][3], const Frame &frame ) {
		listener_ = listener;
		listenerOrigin_ = ToVec3( origin );
		const Vec3 &at = listenerOrigin_;
		for ( int i = 0; i < Voices; ++i ) {
			Voice &voice = voices_[static_cast<std::size_t>( i )];
			if ( !voice.loopOwned && !voice.looping && voice.endTime < frame.serverTime && device.Playing( i ) ) {
				device.Stop( i );
			}
			if ( !device.Playing( i ) && voice.state != VoiceState::Pending ) continue;
			if ( voice.loopOwned ) continue;
			Vec3 heard;
			if ( voice.fixed ) {
				heard = voice.position;
			} else if ( voice.local ) {
				heard = at;
			} else {
				const EntitySound *entity = voice.entnum >= 0 && voice.entnum < Entities
					? &entities_[static_cast<std::size_t>( voice.entnum )] : nullptr;
				if ( entity && entity->time > voice.time ) {
					voice.position = entity->origin;
					voice.time = entity->time;
				} else if ( voice.looping ) {
					/* A looping one-shot whose entity is no longer updated. */
					voice.state = VoiceState::Free;
					device.Stop( i );
					continue;
				}
				heard = entity && entity->useListener ? at : voice.position;
			}
			if ( SnapToListener( voice.entnum, listener_, heard, at ) ) heard = at;
			/* At most once a whole second of server time (0x00470cc0). */
			const float seconds = static_cast<float>( frame.serverTime / 1000 );
			if ( seconds > voice.nextOcclusion ) {
				if ( world.Blocked( heard.data(), at.data(), listener_ ) ) voice.obstructed = true;
				voice.nextOcclusion = seconds + 0.5f;
			}
			const float distance = Distance( heard, at );
			voice.heard = heard;
			voice.gain = voice.applied * Gain( distance, voice.maxDist / MaxDistanceScale, 1.0f );
			if ( voice.state == VoiceState::Pending ) {
				if ( CullAtStart( distance, voice.maxDist ) ) {
					voice.state = VoiceState::Free;
					device.Stop( i );
					continue;
				}
				voice.state = VoiceState::Started;
				device.Place( i, heard.data(), voice.gain, voice.obstructed ? ObstructionGain : 1.0f, -1 );
				device.Start( i, voice.looping, 0 );
				voice.endTime = frame.serverTime + static_cast<int>( voice.durationMs ) + EndMarginMs;
				continue;
			}
			device.Place( i, heard.data(), voice.gain, voice.obstructed ? ObstructionGain : 1.0f, -1 );
		}

		const float right[3] = { -axis[1][0], -axis[1][1], -axis[1][2] };
		for ( int i = 0; i < DialogVoices; ++i ) {
			DialogVoice &dialog = dialogs_[static_cast<std::size_t>( i )];
			const int voice = Voices + i;
			if ( dialog.state != VoiceState::Pending && !device.Playing( voice ) ) continue;
			const Vec3 position = dialog.entnum >= 0 && dialog.entnum < Entities
				? entities_[static_cast<std::size_t>( dialog.entnum )].origin : at;
			const int pan = dialog.local ? CentrePan : DialogPan( position, at, right, frame.separation );
			float gain = frame.master * dialog.volume;
			const float distance = Distance( position, at );
			if ( !dialog.local && distance > dialog.minDist ) gain *= dialog.minDist / distance;
			device.Place( voice, dialog.local ? at.data() : position.data(), MilesVolume( gain ), 1.0f, pan );
			if ( dialog.state == VoiceState::Pending ) {
				dialog.state = VoiceState::Started;
				device.Start( voice, false, 0 );
			}
		}

		MixLoops( device, log, frame );
	}

	/* The part of S_Update (0x00471090) the engine owns: pausing, s_volume
	 * rescaling the one-shots, and reaping the loops the frame dropped. */
	void Update( Device &device, Log &log, bool paused, bool volumeModified, const Frame &frame ) {
		if ( paused && !paused_ ) {
			/* 0x0046f6c0: what is playing stops where it is. */
			for ( int i = 0; i < Voices; ++i ) {
				if ( device.Playing( i ) ) {
					device.Pause( i );
					voices_[static_cast<std::size_t>( i )].state = VoiceState::Paused;
				}
			}
			for ( int i = 0; i < DialogVoices; ++i ) {
				if ( device.Playing( Voices + i ) ) {
					device.Pause( Voices + i );
					dialogs_[static_cast<std::size_t>( i )].state = VoiceState::Paused;
				}
			}
			paused_ = true;
		} else if ( !paused && paused_ ) {
			/* 0x0046f740. */
			for ( int i = 0; i < Voices; ++i ) {
				if ( voices_[static_cast<std::size_t>( i )].state == VoiceState::Paused ) {
					device.Resume( i );
					voices_[static_cast<std::size_t>( i )].state = VoiceState::Started;
				}
			}
			for ( int i = 0; i < DialogVoices; ++i ) {
				if ( dialogs_[static_cast<std::size_t>( i )].state == VoiceState::Paused ) {
					device.Resume( Voices + i );
					dialogs_[static_cast<std::size_t>( i )].state = VoiceState::Started;
				}
			}
			paused_ = false;
		}

		if ( volumeModified ) {
			/* 0x00471145: only an s_volume change rescales a playing
			 * one-shot, by the master volume against the one last used. */
			for ( int i = 0; i < Voices; ++i ) {
				Voice &voice = voices_[static_cast<std::size_t>( i )];
				if ( !device.Playing( i ) && voice.state != VoiceState::Pending ) continue;
				if ( masterOld_ > 0.0f ) voice.factor = frame.master / masterOld_ * voice.factor;
				voice.applied = MilesVolume( voice.factor );
			}
			masterOld_ = frame.master;
		}

		/* 0x0047125a: a record not submitted this frame is reaped if it was
		 * playing; one that was not stays until a submission reuses it. */
		for ( LoopRecord &loop : loops_ ) {
			if ( loop.added || !loop.playing ) continue;
			bool sibling = false;
			if ( loop.stationary ) {
				for ( const LoopRecord &other : loops_ ) {
					if ( other.sfx == loop.sfx && other.added ) sibling = true;
				}
			}
			if ( !sibling && loop.voice >= 0 && loop.voice < Voices ) {
				Voice &voice = voices_[static_cast<std::size_t>( loop.voice )];
				if ( voice.sfx == loop.sfx ) {
					if ( frame.showSounds ) {
						SampleFacts facts;
						device.Sample( loop.sfx, facts );
						log.Print( std::to_string( frame.serverTime ) + " - stopped loop - " + facts.name + "\n" );
					}
					device.Stop( loop.voice );
					voice.sfx = 0;
					voice.state = VoiceState::Free;
				}
			}
			loop.sfx = 0;
			loop.playing = false;
		}
	}

	/* 0x004713f0 with 0x0046fe10: every loop record goes, and every voice
	 * ends. */
	void StopAll( Device &device ) {
		for ( LoopRecord &loop : loops_ ) {
			loop.added = false;
			loop.playing = false;
			loop.sfx = 0;
		}
		for ( int i = 0; i < Voices; ++i ) {
			device.Stop( i );
			voices_[static_cast<std::size_t>( i )].state = VoiceState::Free;
		}
		for ( int i = 0; i < DialogVoices; ++i ) {
			device.Stop( Voices + i );
			dialogs_[static_cast<std::size_t>( i )].state = VoiceState::Free;
		}
	}

	/* sounddump (0x004716f0). */
	std::vector<std::string> Dump( const Device &device ) const {
		std::vector<std::string> lines;
		for ( int i = 0; i < Voices; ++i ) {
			SampleFacts facts;
			const int sfx = voices_[static_cast<std::size_t>( i )].sfx;
			if ( sfx ) device.Sample( sfx, facts );
			lines.push_back( "channel " + std::to_string( i ) + " - " + facts.name + "\n" );
		}
		return lines;
	}

private:
	/* Miles holds a volume of 0 to 127 made with `_ftol`; above 127 it is
	 * taken to clamp. */
	static float MilesVolume( float volume ) {
		const double scaled = static_cast<double>( volume ) * 127.0;
		if ( !( scaled > 0.0 ) ) return 0.0f;
		if ( scaled >= 127.0 ) return 1.0f;
		return static_cast<float>( static_cast<int>( scaled ) ) / 127.0f;
	}

	const SoundInfo &Info( const std::string &name ) const {
		static const SoundInfo defaults;
		return infos_ ? infos_->For( name.c_str() ) : defaults;
	}

	bool Busy( const Device &device, int i ) const {
		return device.Playing( i ) || voices_[static_cast<std::size_t>( i )].state == VoiceState::Pending;
	}

	/* 0x0046f7c0: a non-zero channel takes over the same entity's channel
	 * whatever it holds; channel 0 never does. Nothing is stolen: with no
	 * free voice the sound is dropped. */
	int Pick( const Device &device, int entnum, int entchannel ) const {
		int firstFree = -1;
		for ( int i = 0; i < Voices; ++i ) {
			const Voice &voice = voices_[static_cast<std::size_t>( i )];
			if ( entchannel != 0 && voice.entnum == entnum && voice.entchannel == entchannel ) return i;
			if ( !Busy( device, i ) && firstFree < 0 ) firstFree = i;
		}
		return firstFree;
	}

	void DumpTo( const Device &device, Log &log ) const {
		for ( const std::string &line : Dump( device ) ) log.Developer( line );
	}

	/* 0x0046f860. The volume branch reads the channel the dialog voice held
	 * before, not the one asked for. */
	void StartDialog( Device &device, Log &log, int entnum, int entchannel, int sfx,
		const SampleFacts &facts, const SoundInfo &info, float volume, float minDist, const Frame &frame ) {
		if ( entchannel == MenuChannel ) {
			for ( int i = 0; i < DialogVoices; ++i ) {
				if ( dialogs_[static_cast<std::size_t>( i )].entchannel == MenuChannel && device.Playing( Voices + i ) ) {
					device.Pause( Voices + i );
				}
			}
		}
		int found = -1;
		for ( int i = 0; i < DialogVoices && found < 0; ++i ) {
			if ( !device.Playing( Voices + i ) && dialogs_[static_cast<std::size_t>( i )].state != VoiceState::Pending ) found = i;
		}
		if ( found < 0 ) {
			log.Developer( "Could not find a free dialog channel\n" );
			return;
		}
		DialogVoice &dialog = dialogs_[static_cast<std::size_t>( found )];
		float startVolume;
		if ( dialog.entchannel == MenuChannel ) {
			dialog.volume = info.volume;
			startVolume = frame.volume * info.volume;
		} else if ( volume < 0.0f ) {
			dialog.volume = info.volume;
			startVolume = frame.master * info.volume;
		} else {
			dialog.volume = volume;
			startVolume = frame.master * volume;
		}
		dialog.local = info.local;
		dialog.minDist = ResolveOneShot( minDist, info.minDist );
		const int voice = Voices + found;
		if ( !device.Prepare( voice, sfx ) ) {
			log.Print( "Could not setup dialog file!\n" );
			return;
		}
		device.Place( voice, listenerOrigin_.data(), MilesVolume( startVolume ), 1.0f, CentrePan );
		dialog.state = VoiceState::Pending;
		dialog.sfx = sfx;
		dialog.entnum = entnum;
		dialog.entchannel = entchannel;
		if ( entchannel == MenuChannel ) {
			dialog.state = VoiceState::Started;
			device.Start( voice, false, 0 );
		}
		if ( frame.showSounds ) {
			char line[128];
			std::snprintf( line, sizeof( line ), "%d - %s (vol %f)\n", frame.serverTime,
				facts.name.c_str(), static_cast<double>( volume ) );
			log.Print( line );
		}
	}

	/* 0x00470010. */
	void MixLoops( Device &device, Log &log, const Frame &frame ) {
		const Vec3 &at = listenerOrigin_;
		for ( LoopRecord &loop : loops_ ) loop.processed = false;
		for ( int r = 0; r < Loops; ++r ) {
			LoopRecord &leader = loops_[static_cast<std::size_t>( r )];
			if ( leader.processed || !leader.sfx ) continue;
			SampleFacts facts;
			device.Sample( leader.sfx, facts );
			const SoundInfo &info = Info( facts.name );
			const float minDist = ResolveLoop( leader.minDist, info.minDist );
			const float volume = frame.ambient * ResolveLoop( leader.volume, info.volume );
			float sum = 0.0f, loudest = 0.0f;
			Vec3 direction{};
			if ( leader.stationary ) {
				/* Every record of the sound, stale ones included, at the
				 * leader's volume and distance. */
				std::array<float, Loops> gains{};
				for ( int j = 0; j < Loops; ++j ) {
					LoopRecord &member = loops_[static_cast<std::size_t>( j )];
					if ( member.sfx != leader.sfx ) continue;
					gains[static_cast<std::size_t>( j )] = Gain( Distance( member.origin, at ), minDist, volume );
					member.processed = true;
					sum += gains[static_cast<std::size_t>( j )];
					loudest = ( std::max )( loudest, gains[static_cast<std::size_t>( j )] );
				}
				if ( sum > 0.0f ) {
					for ( int j = 0; j < Loops; ++j ) {
						const LoopRecord &member = loops_[static_cast<std::size_t>( j )];
						if ( member.sfx != leader.sfx || gains[static_cast<std::size_t>( j )] <= 0.0f ) continue;
						Vec3 toward{ member.origin[0] - at[0], member.origin[1] - at[1], member.origin[2] - at[2] };
						const float length = Distance( member.origin, at );
						if ( length > 0.0f ) {
							for ( float &c : toward ) c /= length;
						}
						for ( std::size_t c = 0; c < 3; ++c ) direction[c] += gains[static_cast<std::size_t>( j )] / sum * toward[c];
					}
				}
			} else {
				sum = Gain( Distance( leader.origin, at ), minDist, volume );
				loudest = sum;
				leader.processed = true;
			}
			if ( info.maxFactor >= 1.0f ) sum = ( std::min )( sum, info.maxFactor * loudest );
			leader.gain = sum;
			if ( sum <= 0.0f ) {
				if ( leader.playing ) {
					if ( leader.voice >= 0 && leader.voice < Voices ) {
						if ( frame.showSounds ) {
							log.Print( std::to_string( frame.serverTime ) + " - stopped loop - " + facts.name + "\n" );
						}
						device.Stop( leader.voice );
						voices_[static_cast<std::size_t>( leader.voice )].sfx = 0;
						voices_[static_cast<std::size_t>( leader.voice )].state = VoiceState::Free;
					}
					leader.playing = false;
					if ( leader.stationary ) SetGroupPlaying( leader.sfx, false, -1 );
				}
				continue;
			}
			Vec3 position;
			float gain;
			if ( leader.stationary ) {
				const float length = std::sqrt( direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2] );
				if ( length > 0.0f ) {
					for ( float &c : direction ) c /= length;
				}
				/* Half the min distance away: inside it, so the provider
				 * only pans. */
				for ( std::size_t c = 0; c < 3; ++c ) position[c] = at[c] + direction[c] * minDist * 0.5f;
				gain = MilesVolume( frame.master * ( std::min )( sum, 1.0f ) );
			} else {
				/* A moving loop plays at the master volume from its own
				 * origin, and the provider attenuates it; its own volume and
				 * the ambient level go unused. No shipped loop moves. */
				position = leader.origin;
				gain = MilesVolume( frame.master ) * Gain( Distance( leader.origin, at ), minDist, 1.0f );
			}
			if ( !leader.playing ) {
				const int channel = Pick( device, 0, 0 );
				if ( channel < 0 ) {
					if ( frame.developer ) {
						log.Developer( "No free loop channel for " + facts.name + "\n" );
						DumpTo( device, log );
					}
					continue;
				}
				Voice &voice = voices_[static_cast<std::size_t>( channel )];
				voice.entnum = 0;
				voice.entchannel = 0;
				if ( device.Playing( channel ) ) device.Stop( channel );
				if ( !device.Prepare( channel, leader.sfx ) ) return;
				voice.sfx = leader.sfx;
				voice.fixed = true;
				voice.loopOwned = true;
				voice.looping = true;
				voice.state = VoiceState::Started;
				voice.heard = position;
				voice.gain = gain;
				device.Place( channel, position.data(), gain, 1.0f, -1 );
				device.Start( channel, true, LoopPhase( frame.realtime - leader.time, facts.rate, facts.frames ) );
				leader.playing = true;
				leader.voice = channel;
				if ( leader.stationary ) SetGroupPlaying( leader.sfx, true, channel );
				continue;
			}
			if ( leader.voice < 0 || leader.voice >= Voices || voices_[static_cast<std::size_t>( leader.voice )].sfx != leader.sfx ) {
				/* Its voice was taken; it starts again next frame. */
				leader.playing = false;
				continue;
			}
			Voice &voice = voices_[static_cast<std::size_t>( leader.voice )];
			voice.heard = position;
			voice.gain = gain;
			device.Place( leader.voice, position.data(), gain, 1.0f, -1 );
		}
	}

	void SetGroupPlaying( int sfx, bool playing, int voice ) {
		for ( LoopRecord &loop : loops_ ) {
			if ( loop.sfx != sfx ) continue;
			loop.playing = playing;
			if ( playing ) loop.voice = voice;
		}
	}

	const SoundInfoTable *infos_ = nullptr;
	std::array<Voice, Voices> voices_{};
	std::array<DialogVoice, DialogVoices> dialogs_{};
	std::array<LoopRecord, Loops> loops_{};
	std::vector<EntitySound> entities_ = std::vector<EntitySound>( Entities );
	int listener_ = 0;
	Vec3 listenerOrigin_{};
	bool paused_ = false;
	/* 0x00aaab00. */
	float masterOld_ = 1.0f;
	int nextTooManyLoops_ = 0;
};

} // namespace fntech3::audio::alice::sfx

#endif
