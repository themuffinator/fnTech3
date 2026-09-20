/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_SP_AMBIENT_HPP
#define FNTECH3_STVEF_SP_AMBIENT_HPP

#include <cstddef>
#include <cstdio>
#include <cstring>

/*
 * Elite Force SP's ambient sound sets, sound/sound.txt (stvoy.exe 0x41c960
 * to 0x41d7e0). The file begins with "type ambientset" and a few directories
 * the tools used, and then names sets of three kinds: a generalSet is the
 * room tone of wherever the player is, a localSet plays around a point
 * within its radius, and a bmodelSet holds the sounds a mover starts, runs
 * and stops with.
 *
 * Each set holds the seconds between its sub-waves, the sub-waves themselves
 * (a directory and up to eight names), one looping wave and the volume range
 * its sub-waves play at. Retail reads the file once for each kind of set, and
 * each kind takes only the keywords that belong to it, so a keyword that
 * belongs to another kind ends the set it appears in.
 */

namespace fntech3::stvef::ambient {

inline constexpr int MaxSubWaves = 8;
inline constexpr int NameBytes = 64;
inline constexpr int PathBytes = 256;

enum class SetType { General = 0, Local = 1, BModel = 2 };
inline constexpr int SetTypeCount = 3;

inline const char *SetTypeKeyword( SetType type ) {
	switch ( type ) {
	case SetType::General: return "generalSet";
	case SetType::Local: return "localSet";
	default: return "bmodelSet";
	}
}

struct Set {
	char name[NameBytes];
	SetType type;
	// Seconds between sub-waves, and the volumes they play at.
	int timeStart, timeEnd;
	int volumeMin, volumeMax;
	// How far a local set is heard.
	int radius;
	char loopedWave[PathBytes];
	char subWaves[MaxSubWaves][PathBytes];
	int numSubWaves;
};

enum class Warning { SwappedTimes, SwappedVolumes, TooManySubWaves, UnknownKeyword };

/* Retail's words for what it corrects or refuses (0x41cdf0, 0x41ce60,
 * 0x41d100, 0x41d1a0). The name is the set or keyword the warning is about. */
inline void WarningText( Warning warning, const char *name, char *text, std::size_t size ) {
	switch ( warning ) {
	case Warning::SwappedTimes:
		std::snprintf( text, size,
			"^3WARNING: Corrected swapped start / end times in a \"timeBetweenWaves\" keyword\n" );
		break;
	case Warning::SwappedVolumes:
		std::snprintf( text, size,
			"^3WARNING: Corrected swapped min / max range in a \"volRange\" keyword\n" );
		break;
	case Warning::TooManySubWaves:
		std::snprintf( text, size, "^3WARNING: Too many subwaves on set \"%s\"\n", name ? name : "" );
		break;
	default:
		std::snprintf( text, size, "^3WARNING: Unknown ambient set keyword \"%s\"\n", name ? name : "" );
		break;
	}
}

/* A wave's file, as retail builds it: the sound directory, the name the file
 * gives and the extension it leaves out (0x41d030, 0x41ce60). */
inline void WavePath( const char *directory, const char *name, char *path, std::size_t size ) {
	if ( directory && directory[0] ) {
		std::snprintf( path, size, "sound/%s/%s.wav", directory, name ? name : "" );
	} else {
		std::snprintf( path, size, "sound/%s.wav", name ? name : "" );
	}
}

namespace detail {

inline bool IsSpace( char c ) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

class Scanner {
public:
	explicit Scanner( const char *text ) : text_( text ? text : "" ), offset_( 0 ) {}

	std::size_t Offset() const { return offset_; }
	void Seek( std::size_t offset ) { offset_ = offset; }

	// The next token, with a comment running to the end of its line.
	bool Next( char *token, std::size_t size ) {
		for ( ;; ) {
			while ( text_[offset_] && IsSpace( text_[offset_] ) ) ++offset_;
			if ( !text_[offset_] ) return false;
			if ( text_[offset_] != ';' ) break;
			SkipLine();
		}
		std::size_t length = 0;
		while ( text_[offset_] && !IsSpace( text_[offset_] ) ) {
			if ( length + 1 < size ) token[length++] = text_[offset_];
			++offset_;
		}
		token[length] = '\0';
		return true;
	}

	// Whether the line the last token was on has ended.
	bool AtLineEnd() const {
		for ( std::size_t i = offset_; text_[i]; ++i ) {
			if ( text_[i] == '\n' || text_[i] == '\r' ) return true;
			if ( text_[i] == ';' ) return true;
			if ( !IsSpace( text_[i] ) ) return false;
		}
		return true;
	}

	void SkipLine() {
		while ( text_[offset_] && text_[offset_] != '\n' ) ++offset_;
		if ( text_[offset_] ) ++offset_;
	}

private:
	const char *text_;
	std::size_t offset_;
};

inline bool Equal( const char *left, const char *right ) {
	return left && right && std::strcmp( left, right ) == 0;
}

inline int Number( const char *token ) {
	int value = 0;
	bool negative = false;
	if ( !token ) return 0;
	if ( *token == '-' ) { negative = true; ++token; }
	for ( ; *token >= '0' && *token <= '9'; ++token ) {
		if ( value > ( 1 << 24 ) ) break;
		value = value * 10 + ( *token - '0' );
	}
	return negative ? -value : value;
}

// The keywords a set of each kind takes, in retail's order (0x41d1a0,
// 0x41d260, 0x41d330).
inline bool TakesKeyword( SetType type, const char *keyword ) {
	if ( Equal( keyword, "subWaves" ) ) return true;
	if ( type == SetType::BModel ) return false;
	if ( Equal( keyword, "timeBetweenWaves" ) || Equal( keyword, "loopedWave" )
		|| Equal( keyword, "volRange" ) ) return true;
	return type == SetType::Local && Equal( keyword, "radius" );
}

inline bool IsKeyword( const char *token ) {
	return Equal( token, "timeBetweenWaves" ) || Equal( token, "subWaves" )
		|| Equal( token, "loopedWave" ) || Equal( token, "volRange" )
		|| Equal( token, "radius" ) || Equal( token, "type" )
		|| Equal( token, "generalSet" ) || Equal( token, "localSet" )
		|| Equal( token, "bmodelSet" ) || Equal( token, "amsdir" )
		|| Equal( token, "outdir" ) || Equal( token, "basedir" );
}

} // namespace detail

/* Every set the file names, in retail's order: the general sets, then the
 * local ones, then the mover sets. Retail reads the file once for each kind,
 * taking a line at a time and leaving the lines that begin with anything else
 * (0x41d3c0). onSet takes each finished set and onWarning each correction or
 * unknown keyword. Returns how many sets were read. */
template <typename OnSet, typename OnWarning>
int Parse( const char *text, OnSet onSet, OnWarning onWarning ) {
	int count = 0;
	for ( int kind = 0; kind < SetTypeCount; ++kind ) {
		const SetType type = static_cast<SetType>( kind );
		const char *keyword = SetTypeKeyword( type );
		detail::Scanner scanner( text );
		char token[PathBytes];
		while ( scanner.Next( token, sizeof( token ) ) ) {
			if ( !detail::Equal( token, keyword ) ) {
				scanner.SkipLine();
				continue;
			}
			if ( !scanner.Next( token, sizeof( token ) ) ) break;
			Set set{};
			set.type = type;
			std::snprintf( set.name, sizeof( set.name ), "%s", token );
			scanner.SkipLine();
			for ( ;; ) {
				const std::size_t mark = scanner.Offset();
				if ( !scanner.Next( token, sizeof( token ) ) ) break;
				if ( !detail::TakesKeyword( type, token ) ) {
					if ( detail::IsKeyword( token ) ) {
						// The set ends where a keyword it does not take begins.
						scanner.Seek( mark );
						break;
					}
					onWarning( Warning::UnknownKeyword, token );
					scanner.SkipLine();
					continue;
				}
				if ( detail::Equal( token, "timeBetweenWaves" ) ) {
					char start[PathBytes], end[PathBytes];
					if ( !scanner.Next( start, sizeof( start ) ) || !scanner.Next( end, sizeof( end ) ) ) break;
					set.timeStart = detail::Number( start );
					set.timeEnd = detail::Number( end );
					if ( set.timeStart > set.timeEnd ) {
						const int keep = set.timeStart;
						set.timeStart = set.timeEnd;
						set.timeEnd = keep;
						onWarning( Warning::SwappedTimes, set.name );
					}
				} else if ( detail::Equal( token, "volRange" ) ) {
					char low[PathBytes], high[PathBytes];
					if ( !scanner.Next( low, sizeof( low ) ) || !scanner.Next( high, sizeof( high ) ) ) break;
					set.volumeMin = detail::Number( low );
					set.volumeMax = detail::Number( high );
					if ( set.volumeMin > set.volumeMax ) {
						const int keep = set.volumeMin;
						set.volumeMin = set.volumeMax;
						set.volumeMax = keep;
						onWarning( Warning::SwappedVolumes, set.name );
					}
				} else if ( detail::Equal( token, "radius" ) ) {
					char radius[PathBytes];
					if ( !scanner.Next( radius, sizeof( radius ) ) ) break;
					set.radius = detail::Number( radius );
				} else if ( detail::Equal( token, "loopedWave" ) ) {
					char wave[PathBytes];
					if ( !scanner.Next( wave, sizeof( wave ) ) ) break;
					WavePath( nullptr, wave, set.loopedWave, sizeof( set.loopedWave ) );
				} else {
					// subWaves: a directory, then the names on its line.
					char directory[PathBytes];
					if ( !scanner.Next( directory, sizeof( directory ) ) ) break;
					while ( !scanner.AtLineEnd() && scanner.Next( token, sizeof( token ) ) ) {
						if ( set.numSubWaves >= MaxSubWaves ) {
							onWarning( Warning::TooManySubWaves, set.name );
							continue;
						}
						WavePath( directory, token, set.subWaves[set.numSubWaves],
							sizeof( set.subWaves[set.numSubWaves] ) );
						++set.numSubWaves;
					}
				}
				scanner.SkipLine();
			}
			onSet( set );
			++count;
		}
	}
	return count;
}

} // namespace fntech3::stvef::ambient

#endif
