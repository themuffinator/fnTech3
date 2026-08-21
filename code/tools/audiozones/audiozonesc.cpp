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

#include "../../client/audio/shared/AudioZoneFormat.h"
#include "../../client/audio/shared/AudioZoneRuntime.h"
#include "../../qcommon/surfaceflags.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace azfmt = fnql_audiozones;
namespace azrt = fnql_audiozones_runtime;

namespace {

struct Vec3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct Portal {
	std::uint32_t targetZone = 0;
	std::string targetName;
	Vec3 mins;
	Vec3 maxs;
	float openness = 0.0f;
	float blendDistance = azfmt::kDefaultPortalBlendDistance;
	float minimumBlend = azfmt::kDefaultPortalMinimumBlend;
	float maximumBlend = azfmt::kDefaultPortalMaximumBlend;
	std::uint8_t blendCurve = static_cast<std::uint8_t>( azfmt::PortalBlendCurve::Smooth );
	bool haveTarget = false;
	bool haveMins = false;
	bool haveMaxs = false;
};

struct Zone {
	std::string name;
	Vec3 mins;
	Vec3 maxs;
	bool haveMins = false;
	bool haveMaxs = false;
	std::uint32_t preset = static_cast<std::uint32_t>( azfmt::Preset::SmallRoom );
	float reverbGain = 1.0f;
	float occlusionMultiplier = 1.0f;
	float directLF = 1.0f;
	float directHF = 1.0f;
	float wetLF = 1.0f;
	float wetHF = 1.0f;
	std::uint32_t transitionMs = azfmt::kDefaultTransitionMs;
	std::int32_t priority = 0;
	std::uint8_t materialClass = static_cast<std::uint8_t>( azfmt::MaterialClass::Unknown );
	std::uint8_t flags = 0;
	std::vector<Portal> portals;
};

enum class TokenKind {
	End,
	Invalid,
	Word,
	String,
	LeftBrace,
	RightBrace
};

struct Token {
	TokenKind kind = TokenKind::End;
	std::string text;
	int line = 1;
	int column = 1;
};

static std::string Lowercase( std::string value ) {
	std::transform( value.begin(), value.end(), value.begin(), []( unsigned char c ) {
		if ( c == '_' ) {
			return '-';
		}
		return static_cast<char>( std::tolower( c ) );
	} );
	return value;
}

static bool ParseFloatToken( const Token &token, float &out, std::string &error ) {
	char *end = nullptr;
	errno = 0;
	const float value = std::strtof( token.text.c_str(), &end );
	if ( token.text.empty() || end == token.text.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite( value ) ) {
		error = "expected floating-point value at line " + std::to_string( token.line );
		return false;
	}
	out = value;
	return true;
}

static bool ParseIntToken( const Token &token, int &out, std::string &error ) {
	char *end = nullptr;
	errno = 0;
	const long value = std::strtol( token.text.c_str(), &end, 10 );
	if ( token.text.empty() || end == token.text.c_str() || *end != '\0' || errno == ERANGE ||
		value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max() ) {
		error = "expected integer value at line " + std::to_string( token.line );
		return false;
	}
	out = static_cast<int>( value );
	return true;
}

static bool ParsePresetName( const std::string &text, std::uint32_t &preset ) {
	const std::string normalized = Lowercase( text );
	for ( std::uint32_t i = 0; i < static_cast<std::uint32_t>( azfmt::Preset::Count ); ++i ) {
		if ( normalized == azfmt::kPresetNames[i] ) {
			preset = i;
			return true;
		}
	}
	if ( normalized == "smallroom" ) {
		preset = static_cast<std::uint32_t>( azfmt::Preset::SmallRoom );
		return true;
	}
	if ( normalized == "stoneroom" ) {
		preset = static_cast<std::uint32_t>( azfmt::Preset::StoneRoom );
		return true;
	}
	return false;
}

static bool ParseMaterialName( const std::string &text, std::uint8_t &materialClass ) {
	const std::string normalized = Lowercase( text );
	for ( std::uint32_t i = 0; i < static_cast<std::uint32_t>( azfmt::MaterialClass::Count ); ++i ) {
		if ( normalized == azfmt::kMaterialClassNames[i] ) {
			materialClass = static_cast<std::uint8_t>( i );
			return true;
		}
	}
	return false;
}

static bool ParseZoneFlagName( const std::string &text, std::uint8_t &flag ) {
	const std::string normalized = Lowercase( text );
	if ( normalized == "generated" ) {
		flag = azfmt::ZoneFlagGenerated;
		return true;
	}
	if ( normalized == "outdoor" || normalized == "outdoors" ) {
		flag = azfmt::ZoneFlagOutdoor;
		return true;
	}
	if ( normalized == "underwater" || normalized == "water" ) {
		flag = azfmt::ZoneFlagUnderwater;
		return true;
	}
	return false;
}

static bool ParsePortalBlendCurveName( const std::string &text, std::uint8_t &curve ) {
	const std::string normalized = Lowercase( text );
	for ( std::uint32_t i = 0; i < static_cast<std::uint32_t>( azfmt::PortalBlendCurve::Count ); ++i ) {
		if ( normalized == azfmt::kPortalBlendCurveNames[i] ) {
			curve = static_cast<std::uint8_t>( i );
			return true;
		}
	}
	if ( normalized == "easein" ) {
		curve = static_cast<std::uint8_t>( azfmt::PortalBlendCurve::EaseIn );
		return true;
	}
	if ( normalized == "easeout" ) {
		curve = static_cast<std::uint8_t>( azfmt::PortalBlendCurve::EaseOut );
		return true;
	}
	return false;
}

class Tokenizer {
public:
	explicit Tokenizer( std::string source ) : source_( std::move( source ) ) {}

	Token Next() {
		SkipWhitespaceAndComments();
		Token token;
		token.line = line_;
		token.column = column_;
		if ( !error_.empty() ) {
			token.kind = TokenKind::Invalid;
			token.text = error_;
			token.line = errorLine_;
			token.column = errorColumn_;
			error_.clear();
			return token;
		}

		if ( index_ >= source_.size() ) {
			token.kind = TokenKind::End;
			return token;
		}

		const char c = source_[index_];
		if ( c == '{' ) {
			Advance();
			token.kind = TokenKind::LeftBrace;
			token.text = "{";
			return token;
		}
		if ( c == '}' ) {
			Advance();
			token.kind = TokenKind::RightBrace;
			token.text = "}";
			return token;
		}
		if ( c == '"' ) {
			return ReadString();
		}
		return ReadWord();
	}

private:
	void Advance() {
		if ( index_ >= source_.size() ) {
			return;
		}
		if ( source_[index_] == '\n' ) {
			++line_;
			column_ = 1;
		} else {
			++column_;
		}
		++index_;
	}

	void SkipWhitespaceAndComments() {
		for (;;) {
			while ( index_ < source_.size() && std::isspace( static_cast<unsigned char>( source_[index_] ) ) ) {
				Advance();
			}
			if ( index_ >= source_.size() ) {
				return;
			}
			if ( source_[index_] == '#' ) {
				while ( index_ < source_.size() && source_[index_] != '\n' ) {
					Advance();
				}
				continue;
			}
			if ( source_[index_] == '/' && index_ + 1 < source_.size() && source_[index_ + 1] == '/' ) {
				while ( index_ < source_.size() && source_[index_] != '\n' ) {
					Advance();
				}
				continue;
			}
			if ( source_[index_] == '/' && index_ + 1 < source_.size() && source_[index_ + 1] == '*' ) {
				const int commentLine = line_;
				const int commentColumn = column_;
				Advance();
				Advance();
				while ( index_ + 1 < source_.size() && !( source_[index_] == '*' && source_[index_ + 1] == '/' ) ) {
					Advance();
				}
				if ( index_ + 1 < source_.size() ) {
					Advance();
					Advance();
				} else {
					error_ = "unterminated block comment";
					errorLine_ = commentLine;
					errorColumn_ = commentColumn;
				}
				continue;
			}
			return;
		}
	}

	Token ReadString() {
		Token token;
		token.kind = TokenKind::String;
		token.line = line_;
		token.column = column_;
		Advance();
		while ( index_ < source_.size() ) {
			const char c = source_[index_];
			if ( c == '"' ) {
				Advance();
				return token;
			}
			if ( c == '\\' && index_ + 1 < source_.size() ) {
				Advance();
				const char escaped = source_[index_];
				switch ( escaped ) {
				case 'n':
					token.text.push_back( '\n' );
					break;
				case 't':
					token.text.push_back( '\t' );
					break;
				default:
					token.text.push_back( escaped );
					break;
				}
				Advance();
				continue;
			}
			token.text.push_back( c );
			Advance();
		}
		token.kind = TokenKind::Invalid;
		token.text = "unterminated string";
		return token;
	}

	Token ReadWord() {
		Token token;
		token.kind = TokenKind::Word;
		token.line = line_;
		token.column = column_;
		while ( index_ < source_.size() ) {
			const char c = source_[index_];
			if ( std::isspace( static_cast<unsigned char>( c ) ) || c == '{' || c == '}' || c == '"' ) {
				break;
			}
			if ( c == '#' ) {
				break;
			}
			if ( c == '/' && index_ + 1 < source_.size() && ( source_[index_ + 1] == '/' || source_[index_ + 1] == '*' ) ) {
				break;
			}
			token.text.push_back( c );
			Advance();
		}
		return token;
	}

	std::string source_;
	std::string error_;
	std::size_t index_ = 0;
	int line_ = 1;
	int column_ = 1;
	int errorLine_ = 1;
	int errorColumn_ = 1;
};

class Parser {
public:
	explicit Parser( std::string source ) : tokenizer_( std::move( source ) ) {
		current_ = tokenizer_.Next();
	}

	bool Parse( std::vector<Zone> &zones, std::string &error ) {
		if ( current_.kind == TokenKind::Word && Lowercase( current_.text ) == "audiozones" ) {
			Advance();
			if ( current_.kind == TokenKind::Invalid ) {
				error = current_.text + " at line " + std::to_string( current_.line );
				return false;
			}
			if ( current_.kind != TokenKind::Word || current_.text != "1" ) {
				error = "unsupported or missing audiozones text version at line " + std::to_string( current_.line );
				return false;
			}
			Advance();
		}

		while ( current_.kind != TokenKind::End ) {
			if ( current_.kind == TokenKind::Invalid ) {
				error = current_.text + " at line " + std::to_string( current_.line );
				return false;
			}
			if ( current_.kind != TokenKind::Word || Lowercase( current_.text ) != "zone" ) {
				error = "expected 'zone' at line " + std::to_string( current_.line );
				return false;
			}
			if ( zones.size() >= azfmt::kMaxZones ) {
				error = "too many zones; maximum is " + std::to_string( azfmt::kMaxZones );
				return false;
			}
			Zone zone;
			if ( !ParseZone( zone, error ) ) {
				return false;
			}
			zones.push_back( zone );
		}

		if ( zones.empty() ) {
			error = "no zones were defined";
			return false;
		}
		if ( !ResolvePortalTargets( zones, error ) ) {
			return false;
		}
		return true;
	}

private:
	void Advance() {
		current_ = tokenizer_.Next();
	}

	bool ExpectWordLike( Token &token, const char *what, std::string &error ) {
		if ( current_.kind == TokenKind::Invalid ) {
			error = current_.text + " at line " + std::to_string( current_.line );
			return false;
		}
		if ( current_.kind != TokenKind::Word && current_.kind != TokenKind::String ) {
			error = std::string( "expected " ) + what + " at line " + std::to_string( current_.line );
			return false;
		}
		token = current_;
		Advance();
		return true;
	}

	bool ExpectLeftBrace( std::string &error ) {
		if ( current_.kind == TokenKind::Invalid ) {
			error = current_.text + " at line " + std::to_string( current_.line );
			return false;
		}
		if ( current_.kind != TokenKind::LeftBrace ) {
			error = "expected '{' at line " + std::to_string( current_.line );
			return false;
		}
		Advance();
		return true;
	}

	bool ParseVec3( Vec3 &value, std::string &error ) {
		Token token;
		if ( !ExpectWordLike( token, "x value", error ) || !ParseFloatToken( token, value.x, error ) ) {
			return false;
		}
		if ( !ExpectWordLike( token, "y value", error ) || !ParseFloatToken( token, value.y, error ) ) {
			return false;
		}
		if ( !ExpectWordLike( token, "z value", error ) || !ParseFloatToken( token, value.z, error ) ) {
			return false;
		}
		return true;
	}

	bool ParseFloatProperty( float &value, float minimum, float maximum, std::string &error ) {
		Token token;
		if ( !ExpectWordLike( token, "floating-point value", error ) || !ParseFloatToken( token, value, error ) ) {
			return false;
		}
		if ( value < minimum || value > maximum ) {
			std::ostringstream message;
			message << "value at line " << token.line << " is outside allowed range " << minimum << ".." << maximum;
			error = message.str();
			return false;
		}
		return true;
	}

	bool ParseBoolProperty( bool &value, std::string &error ) {
		Token token;
		if ( !ExpectWordLike( token, "boolean value", error ) ) {
			return false;
		}
		const std::string text = Lowercase( token.text );
		if ( text == "1" || text == "true" || text == "yes" || text == "on" ) {
			value = true;
			return true;
		}
		if ( text == "0" || text == "false" || text == "no" || text == "off" ) {
			value = false;
			return true;
		}
		error = "expected boolean value at line " + std::to_string( token.line );
		return false;
	}

	bool ParsePortalTarget( Portal &portal, const Token &token, std::string &error ) {
		int numericTarget = -1;
		std::string numericError;
		if ( token.kind == TokenKind::Word && ParseIntToken( token, numericTarget, numericError ) ) {
			if ( numericTarget < 0 || numericTarget >= static_cast<int>( azfmt::kMaxZones ) ) {
				error = "portal targetZone at line " + std::to_string( token.line ) + " is outside allowed range 0.." + std::to_string( azfmt::kMaxZones - 1u );
				return false;
			}
			portal.targetZone = static_cast<std::uint32_t>( numericTarget );
			portal.targetName.clear();
			portal.haveTarget = true;
			return true;
		}
		if ( token.text.empty() || token.text.size() > azfmt::kMaxNameBytes ) {
			error = "portal target name at line " + std::to_string( token.line ) + " must be 1.." + std::to_string( azfmt::kMaxNameBytes ) + " bytes";
			return false;
		}
		portal.targetName = token.text;
		portal.haveTarget = true;
		return true;
	}

	bool ParsePortal( Zone &zone, std::string &error ) {
		if ( zone.portals.size() >= azfmt::kMaxZonePortals ) {
			error = "zone '" + zone.name + "' has too many portals; maximum is " + std::to_string( azfmt::kMaxZonePortals );
			return false;
		}

		Portal portal;
		if ( current_.kind == TokenKind::Word || current_.kind == TokenKind::String ) {
			Token targetToken;
			if ( !ExpectWordLike( targetToken, "portal target", error ) || !ParsePortalTarget( portal, targetToken, error ) ) {
				return false;
			}
		}
		if ( !ExpectLeftBrace( error ) ) {
			return false;
		}

		while ( current_.kind != TokenKind::End && current_.kind != TokenKind::RightBrace ) {
			if ( current_.kind == TokenKind::Invalid ) {
				error = current_.text + " at line " + std::to_string( current_.line );
				return false;
			}
			if ( current_.kind != TokenKind::Word ) {
				error = "expected portal property at line " + std::to_string( current_.line );
				return false;
			}

			const Token propertyToken = current_;
			const std::string property = Lowercase( current_.text );
			Advance();

			if ( property == "target" || property == "zone" ) {
				Token targetToken;
				if ( !ExpectWordLike( targetToken, "portal target", error ) || !ParsePortalTarget( portal, targetToken, error ) ) {
					return false;
				}
			} else if ( property == "targetzone" || property == "targetindex" ) {
				Token targetToken;
				if ( !ExpectWordLike( targetToken, "portal target index", error ) ) {
					return false;
				}
				int numericTarget = -1;
				if ( !ParseIntToken( targetToken, numericTarget, error ) || numericTarget < 0 || numericTarget >= static_cast<int>( azfmt::kMaxZones ) ) {
					error = "portal targetZone at line " + std::to_string( targetToken.line ) + " is outside allowed range 0.." + std::to_string( azfmt::kMaxZones - 1u );
					return false;
				}
				portal.targetZone = static_cast<std::uint32_t>( numericTarget );
				portal.targetName.clear();
				portal.haveTarget = true;
			} else if ( property == "bounds" ) {
				if ( !ParseVec3( portal.mins, error ) || !ParseVec3( portal.maxs, error ) ) {
					return false;
				}
				portal.haveMins = true;
				portal.haveMaxs = true;
			} else if ( property == "mins" || property == "min" ) {
				if ( !ParseVec3( portal.mins, error ) ) {
					return false;
				}
				portal.haveMins = true;
			} else if ( property == "maxs" || property == "max" ) {
				if ( !ParseVec3( portal.maxs, error ) ) {
					return false;
				}
				portal.haveMaxs = true;
			} else if ( property == "openness" || property == "open" || property == "blend" ) {
				if ( !ParseFloatProperty( portal.openness, 0.0f, 1.0f, error ) ) {
					return false;
				}
			} else if ( property == "blenddistance" || property == "blend-distance" || property == "distance" || property == "radius" || property == "blendradius" || property == "blend-radius" ) {
				if ( !ParseFloatProperty( portal.blendDistance, azfmt::kMinimumPortalBlendDistance, azfmt::kMaximumPortalBlendDistance, error ) ) {
					return false;
				}
			} else if ( property == "minimumblend" || property == "minimum-blend" || property == "minblend" || property == "min-blend" ) {
				if ( !ParseFloatProperty( portal.minimumBlend, 0.0f, 1.0f, error ) ) {
					return false;
				}
			} else if ( property == "maximumblend" || property == "maximum-blend" || property == "maxblend" || property == "max-blend" ) {
				if ( !ParseFloatProperty( portal.maximumBlend, 0.0f, 1.0f, error ) ) {
					return false;
				}
			} else if ( property == "blendcurve" || property == "blend-curve" || property == "curve" ) {
				Token token;
				if ( !ExpectWordLike( token, "portal blend curve", error ) || !ParsePortalBlendCurveName( token.text, portal.blendCurve ) ) {
					error = "unknown portal blend curve at line " + std::to_string( token.line );
					return false;
				}
			} else {
				error = "unknown portal property '" + propertyToken.text + "' at line " + std::to_string( propertyToken.line );
				return false;
			}
		}

		if ( current_.kind != TokenKind::RightBrace ) {
			error = "unterminated portal in zone '" + zone.name + "'";
			return false;
		}
		Advance();

		if ( !portal.haveTarget ) {
			error = "portal in zone '" + zone.name + "' is missing target";
			return false;
		}
		if ( !portal.haveMins || !portal.haveMaxs ) {
			error = "portal in zone '" + zone.name + "' is missing bounds";
			return false;
		}
		NormalizePortalBounds( portal );
		portal.openness = ( std::max )( 0.0f, ( std::min )( 1.0f, portal.openness ) );
		if ( portal.minimumBlend > portal.maximumBlend ) {
			error = "portal in zone '" + zone.name + "' has minimumBlend greater than maximumBlend";
			return false;
		}
		zone.portals.push_back( portal );
		return true;
	}

	bool ParseZone( Zone &zone, std::string &error ) {
		Advance();
		Token nameToken;
		if ( !ExpectWordLike( nameToken, "zone name", error ) ) {
			return false;
		}
		zone.name = nameToken.text;
		if ( zone.name.empty() || zone.name.size() > azfmt::kMaxNameBytes ) {
			error = "zone name at line " + std::to_string( nameToken.line ) + " must be 1.." + std::to_string( azfmt::kMaxNameBytes ) + " bytes";
			return false;
		}
		if ( !ExpectLeftBrace( error ) ) {
			return false;
		}

		while ( current_.kind != TokenKind::End && current_.kind != TokenKind::RightBrace ) {
			if ( current_.kind == TokenKind::Invalid ) {
				error = current_.text + " at line " + std::to_string( current_.line );
				return false;
			}
			if ( current_.kind != TokenKind::Word ) {
				error = "expected zone property at line " + std::to_string( current_.line );
				return false;
			}

			const Token propertyToken = current_;
			const std::string property = Lowercase( current_.text );
			Advance();

			if ( property == "bounds" ) {
				if ( !ParseVec3( zone.mins, error ) || !ParseVec3( zone.maxs, error ) ) {
					return false;
				}
				zone.haveMins = true;
				zone.haveMaxs = true;
			} else if ( property == "mins" || property == "min" ) {
				if ( !ParseVec3( zone.mins, error ) ) {
					return false;
				}
				zone.haveMins = true;
			} else if ( property == "maxs" || property == "max" ) {
				if ( !ParseVec3( zone.maxs, error ) ) {
					return false;
				}
				zone.haveMaxs = true;
			} else if ( property == "environment" || property == "preset" ) {
				Token token;
				if ( !ExpectWordLike( token, "environment preset", error ) || !ParsePresetName( token.text, zone.preset ) ) {
					error = "unknown environment preset at line " + std::to_string( token.line );
					return false;
				}
			} else if ( property == "reverbgain" || property == "wetgain" ) {
				if ( !ParseFloatProperty( zone.reverbGain, 0.0f, 4.0f, error ) ) {
					return false;
				}
			} else if ( property == "occlusion" || property == "occlusionmultiplier" ) {
				if ( !ParseFloatProperty( zone.occlusionMultiplier, 0.0f, 4.0f, error ) ) {
					return false;
				}
			} else if ( property == "directlf" ) {
				if ( !ParseFloatProperty( zone.directLF, 0.0f, 1.0f, error ) ) {
					return false;
				}
			} else if ( property == "directhf" ) {
				if ( !ParseFloatProperty( zone.directHF, 0.0f, 1.0f, error ) ) {
					return false;
				}
			} else if ( property == "wetlf" ) {
				if ( !ParseFloatProperty( zone.wetLF, 0.0f, 1.0f, error ) ) {
					return false;
				}
			} else if ( property == "wethf" ) {
				if ( !ParseFloatProperty( zone.wetHF, 0.0f, 1.0f, error ) ) {
					return false;
				}
			} else if ( property == "lpfbias" ) {
				float value = 1.0f;
				if ( !ParseFloatProperty( value, 0.0f, 1.0f, error ) ) {
					return false;
				}
				zone.directHF = value;
				zone.wetHF = value;
			} else if ( property == "hpfbias" ) {
				float value = 1.0f;
				if ( !ParseFloatProperty( value, 0.0f, 1.0f, error ) ) {
					return false;
				}
				zone.directLF = value;
				zone.wetLF = value;
			} else if ( property == "transition" || property == "transitionms" ) {
				int value = 0;
				Token token;
				if ( !ExpectWordLike( token, "transition milliseconds", error ) || !ParseIntToken( token, value, error ) ) {
					return false;
				}
				if ( value < 0 || value > 10000 ) {
					error = "transition at line " + std::to_string( token.line ) + " is outside allowed range 0..10000";
					return false;
				}
				zone.transitionMs = static_cast<std::uint32_t>( value );
			} else if ( property == "priority" ) {
				int value = 0;
				Token token;
				if ( !ExpectWordLike( token, "priority", error ) || !ParseIntToken( token, value, error ) ) {
					return false;
				}
				zone.priority = static_cast<std::int32_t>( value );
			} else if ( property == "material" || property == "materialclass" ) {
				Token token;
				if ( !ExpectWordLike( token, "material class", error ) || !ParseMaterialName( token.text, zone.materialClass ) ) {
					error = "unknown material class at line " + std::to_string( token.line );
					return false;
				}
			} else if ( property == "flag" || property == "flags" ) {
				Token token;
				std::uint8_t flag = 0;
				if ( !ExpectWordLike( token, "zone flag", error ) || !ParseZoneFlagName( token.text, flag ) ) {
					error = "unknown zone flag at line " + std::to_string( token.line );
					return false;
				}
				zone.flags = static_cast<std::uint8_t>( zone.flags | flag );
			} else if ( property == "outdoor" || property == "outdoors" ) {
				bool enabled = false;
				if ( !ParseBoolProperty( enabled, error ) ) {
					return false;
				}
				if ( enabled ) {
					zone.flags = static_cast<std::uint8_t>( zone.flags | azfmt::ZoneFlagOutdoor );
				} else {
					zone.flags = static_cast<std::uint8_t>( zone.flags & ~azfmt::ZoneFlagOutdoor );
				}
			} else if ( property == "underwater" ) {
				bool enabled = false;
				if ( !ParseBoolProperty( enabled, error ) ) {
					return false;
				}
				if ( enabled ) {
					zone.flags = static_cast<std::uint8_t>( zone.flags | azfmt::ZoneFlagUnderwater );
				} else {
					zone.flags = static_cast<std::uint8_t>( zone.flags & ~azfmt::ZoneFlagUnderwater );
				}
			} else if ( property == "portal" ) {
				if ( !ParsePortal( zone, error ) ) {
					return false;
				}
			} else {
				error = "unknown zone property '" + propertyToken.text + "' at line " + std::to_string( propertyToken.line );
				return false;
			}
		}

		if ( current_.kind != TokenKind::RightBrace ) {
			error = "unterminated zone '" + zone.name + "'";
			return false;
		}
		Advance();

		if ( !zone.haveMins || !zone.haveMaxs ) {
			error = "zone '" + zone.name + "' is missing bounds";
			return false;
		}
		NormalizeBounds( zone );
		if ( zone.mins.x == zone.maxs.x || zone.mins.y == zone.maxs.y || zone.mins.z == zone.maxs.z ) {
			error = "zone '" + zone.name + "' has zero-volume bounds";
			return false;
		}
		return true;
	}

	static void NormalizeBounds( Zone &zone ) {
		if ( zone.mins.x > zone.maxs.x ) {
			std::swap( zone.mins.x, zone.maxs.x );
		}
		if ( zone.mins.y > zone.maxs.y ) {
			std::swap( zone.mins.y, zone.maxs.y );
		}
		if ( zone.mins.z > zone.maxs.z ) {
			std::swap( zone.mins.z, zone.maxs.z );
		}
	}

	static void NormalizePortalBounds( Portal &portal ) {
		if ( portal.mins.x > portal.maxs.x ) {
			std::swap( portal.mins.x, portal.maxs.x );
		}
		if ( portal.mins.y > portal.maxs.y ) {
			std::swap( portal.mins.y, portal.maxs.y );
		}
		if ( portal.mins.z > portal.maxs.z ) {
			std::swap( portal.mins.z, portal.maxs.z );
		}
	}

	static bool ResolvePortalTargets( std::vector<Zone> &zones, std::string &error ) {
		for ( std::size_t zoneIndex = 0; zoneIndex < zones.size(); ++zoneIndex ) {
			for ( Portal &portal : zones[zoneIndex].portals ) {
				if ( portal.targetName.empty() ) {
					if ( portal.targetZone >= zones.size() ) {
						error = "portal in zone '" + zones[zoneIndex].name + "' targets zone index " + std::to_string( portal.targetZone ) + ", but only " + std::to_string( zones.size() ) + " zones are defined";
						return false;
					}
					continue;
				}

				int found = -1;
				for ( std::size_t candidate = 0; candidate < zones.size(); ++candidate ) {
					if ( zones[candidate].name != portal.targetName ) {
						continue;
					}
					if ( found >= 0 ) {
						error = "portal target '" + portal.targetName + "' in zone '" + zones[zoneIndex].name + "' is ambiguous; zone names used by portals must be unique";
						return false;
					}
					found = static_cast<int>( candidate );
				}
				if ( found < 0 ) {
					error = "portal in zone '" + zones[zoneIndex].name + "' targets unknown zone '" + portal.targetName + "'";
					return false;
				}
				portal.targetZone = static_cast<std::uint32_t>( found );
			}
		}
		return true;
	}

	Tokenizer tokenizer_;
	Token current_;
};

static void WriteU8( std::vector<std::uint8_t> &out, std::uint8_t value ) {
	out.push_back( value );
}

static void WriteU16( std::vector<std::uint8_t> &out, std::uint16_t value ) {
	out.push_back( static_cast<std::uint8_t>( value & 0xffu ) );
	out.push_back( static_cast<std::uint8_t>( ( value >> 8u ) & 0xffu ) );
}

static void WriteU32( std::vector<std::uint8_t> &out, std::uint32_t value ) {
	out.push_back( static_cast<std::uint8_t>( value & 0xffu ) );
	out.push_back( static_cast<std::uint8_t>( ( value >> 8u ) & 0xffu ) );
	out.push_back( static_cast<std::uint8_t>( ( value >> 16u ) & 0xffu ) );
	out.push_back( static_cast<std::uint8_t>( ( value >> 24u ) & 0xffu ) );
}

static void WriteI32( std::vector<std::uint8_t> &out, std::int32_t value ) {
	WriteU32( out, static_cast<std::uint32_t>( value ) );
}

static void WriteF32( std::vector<std::uint8_t> &out, float value ) {
	static_assert( sizeof( float ) == sizeof( std::uint32_t ), "FnQL audio zone files require 32-bit floats" );
	std::uint32_t bits = 0;
	std::memcpy( &bits, &value, sizeof( bits ) );
	WriteU32( out, bits );
}

static void WriteVec3( std::vector<std::uint8_t> &out, const Vec3 &value ) {
	WriteF32( out, value.x );
	WriteF32( out, value.y );
	WriteF32( out, value.z );
}

static bool IsSupportedSidecarVersion( std::uint32_t version ) {
	return version == azfmt::kLegacyVersion ||
		version == azfmt::kMetadataVersion ||
		version == azfmt::kPortalTuningVersion;
}

static void NormalizePortalTuning( Portal &portal ) {
	portal.openness = ( std::max )( 0.0f, ( std::min )( 1.0f, portal.openness ) );
	portal.blendDistance = ( std::max )( azfmt::kMinimumPortalBlendDistance, ( std::min )( azfmt::kMaximumPortalBlendDistance, portal.blendDistance ) );
	portal.minimumBlend = ( std::max )( 0.0f, ( std::min )( 1.0f, portal.minimumBlend ) );
	portal.maximumBlend = ( std::max )( 0.0f, ( std::min )( 1.0f, portal.maximumBlend ) );
	if ( portal.minimumBlend > portal.maximumBlend ) {
		std::swap( portal.minimumBlend, portal.maximumBlend );
	}
}

static std::vector<std::uint8_t> BuildBinary( const std::vector<Zone> &zones ) {
	std::vector<std::uint8_t> binary;
	binary.reserve( 12u + zones.size() * 112u );
	binary.insert( binary.end(), std::begin( azfmt::kMagic ), std::end( azfmt::kMagic ) );
	WriteU32( binary, azfmt::kVersion );
	WriteU32( binary, static_cast<std::uint32_t>( zones.size() ) );

	for ( const Zone &zone : zones ) {
		WriteVec3( binary, zone.mins );
		WriteVec3( binary, zone.maxs );
		WriteU32( binary, zone.preset );
		WriteF32( binary, zone.reverbGain );
		WriteF32( binary, zone.occlusionMultiplier );
		WriteF32( binary, zone.directLF );
		WriteF32( binary, zone.directHF );
		WriteF32( binary, zone.wetLF );
		WriteF32( binary, zone.wetHF );
		WriteU32( binary, zone.transitionMs );
		WriteI32( binary, zone.priority );
		WriteU8( binary, static_cast<std::uint8_t>( zone.name.size() ) );
		binary.insert( binary.end(), zone.name.begin(), zone.name.end() );
		WriteU8( binary, zone.materialClass );
		WriteU8( binary, zone.flags );
		WriteU16( binary, static_cast<std::uint16_t>( zone.portals.size() ) );
		for ( const Portal &portal : zone.portals ) {
			WriteU32( binary, portal.targetZone );
			WriteVec3( binary, portal.mins );
			WriteVec3( binary, portal.maxs );
			WriteF32( binary, portal.openness );
			WriteF32( binary, portal.blendDistance );
			WriteF32( binary, portal.minimumBlend );
			WriteF32( binary, portal.maximumBlend );
			WriteU8( binary, portal.blendCurve );
		}
	}

	return binary;
}

static bool ReadWholeFile( const std::filesystem::path &path, std::string &contents, std::string &error ) {
	std::error_code ec;
	const std::uintmax_t size = std::filesystem::file_size( path, ec );
	if ( !ec && size > azfmt::kMaxFileBytes ) {
		error = "source file is too large";
		return false;
	}

	std::ifstream file( path, std::ios::binary );
	if ( !file ) {
		error = "could not open " + path.string();
		return false;
	}
	std::ostringstream stream;
	stream << file.rdbuf();
	contents = stream.str();
	if ( contents.size() > azfmt::kMaxFileBytes ) {
		error = "source file is too large";
		return false;
	}
	return true;
}

static bool WriteWholeFile( const std::filesystem::path &path, const std::vector<std::uint8_t> &contents, std::string &error ) {
	if ( path.has_parent_path() ) {
		std::error_code ec;
		std::filesystem::create_directories( path.parent_path(), ec );
		if ( ec ) {
			error = "could not create directory " + path.parent_path().string() + ": " + ec.message();
			return false;
		}
	}

	std::ofstream file( path, std::ios::binary );
	if ( !file ) {
		error = "could not write " + path.string();
		return false;
	}
	file.write( reinterpret_cast<const char *>( contents.data() ), static_cast<std::streamsize>( contents.size() ) );
	if ( !file ) {
		error = "failed while writing " + path.string();
		return false;
	}
	return true;
}

static std::filesystem::path DefaultOutputPath( std::filesystem::path input ) {
	input.replace_extension( ".azb" );
	return input;
}

static bool ReadU8( const std::vector<std::uint8_t> &data, std::size_t &offset, std::uint8_t &out ) {
	if ( offset >= data.size() ) {
		return false;
	}
	out = data[offset++];
	return true;
}

static bool ReadU16( const std::vector<std::uint8_t> &data, std::size_t &offset, std::uint16_t &out ) {
	if ( offset > data.size() || data.size() - offset < 2u ) {
		return false;
	}
	out = static_cast<std::uint16_t>( data[offset] ) |
		( static_cast<std::uint16_t>( data[offset + 1u] ) << 8u );
	offset += 2u;
	return true;
}

static bool ReadU32( const std::vector<std::uint8_t> &data, std::size_t &offset, std::uint32_t &out ) {
	if ( offset > data.size() || data.size() - offset < 4u ) {
		return false;
	}
	out = static_cast<std::uint32_t>( data[offset] ) |
		( static_cast<std::uint32_t>( data[offset + 1u] ) << 8u ) |
		( static_cast<std::uint32_t>( data[offset + 2u] ) << 16u ) |
		( static_cast<std::uint32_t>( data[offset + 3u] ) << 24u );
	offset += 4u;
	return true;
}

static bool ReadI32( const std::vector<std::uint8_t> &data, std::size_t &offset, std::int32_t &out ) {
	std::uint32_t value = 0;
	if ( !ReadU32( data, offset, value ) ) {
		return false;
	}
	out = static_cast<std::int32_t>( value );
	return true;
}

static bool ReadF32( const std::vector<std::uint8_t> &data, std::size_t &offset, float &out ) {
	std::uint32_t bits = 0;
	if ( !ReadU32( data, offset, bits ) ) {
		return false;
	}
	std::memcpy( &out, &bits, sizeof( out ) );
	return std::isfinite( out );
}

static bool ReadVec3( const std::vector<std::uint8_t> &data, std::size_t &offset, Vec3 &out ) {
	return ReadF32( data, offset, out.x ) &&
		ReadF32( data, offset, out.y ) &&
		ReadF32( data, offset, out.z );
}

static bool LoadBinary( const std::filesystem::path &path, std::vector<Zone> &zones, std::string &error, std::uint32_t *versionOut = nullptr ) {
	std::error_code ec;
	const std::uintmax_t size = std::filesystem::file_size( path, ec );
	if ( !ec && size > azfmt::kMaxFileBytes ) {
		error = "file is too large";
		return false;
	}

	std::ifstream file( path, std::ios::binary );
	if ( !file ) {
		error = "could not open " + path.string();
		return false;
	}
	const std::vector<std::uint8_t> data( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
	if ( data.size() > azfmt::kMaxFileBytes ) {
		error = "file is too large";
		return false;
	}
	if ( data.size() < 12u || !std::equal( std::begin( azfmt::kMagic ), std::end( azfmt::kMagic ), data.begin() ) ) {
		error = "bad audio-zone magic";
		return false;
	}

	std::size_t offset = 4u;
	std::uint32_t version = 0;
	std::uint32_t zoneCount = 0;
	if ( !ReadU32( data, offset, version ) || !ReadU32( data, offset, zoneCount ) ) {
		error = "truncated audio-zone header";
		return false;
	}
	if ( !IsSupportedSidecarVersion( version ) ) {
		error = "unsupported audio-zone version " + std::to_string( version );
		return false;
	}
	if ( versionOut != nullptr ) {
		*versionOut = version;
	}
	if ( zoneCount > azfmt::kMaxZones ) {
		error = "too many zones in file";
		return false;
	}
	if ( zoneCount == 0 ) {
		error = "no zones in file";
		return false;
	}

	zones.clear();
	zones.reserve( zoneCount );
	for ( std::uint32_t i = 0; i < zoneCount; ++i ) {
		Zone zone;
		std::uint8_t nameLength = 0;
		if ( !ReadVec3( data, offset, zone.mins ) ||
			!ReadVec3( data, offset, zone.maxs ) ||
			!ReadU32( data, offset, zone.preset ) ||
			!ReadF32( data, offset, zone.reverbGain ) ||
			!ReadF32( data, offset, zone.occlusionMultiplier ) ||
			!ReadF32( data, offset, zone.directLF ) ||
			!ReadF32( data, offset, zone.directHF ) ||
			!ReadF32( data, offset, zone.wetLF ) ||
			!ReadF32( data, offset, zone.wetHF ) ||
			!ReadU32( data, offset, zone.transitionMs ) ||
			!ReadI32( data, offset, zone.priority ) ||
			!ReadU8( data, offset, nameLength ) ) {
			error = "truncated zone record";
			return false;
		}
		if ( zone.preset >= static_cast<std::uint32_t>( azfmt::Preset::Count ) ) {
			error = "zone record has unknown preset index";
			return false;
		}
		if ( nameLength == 0 || nameLength > azfmt::kMaxNameBytes ||
			offset > data.size() || data.size() - offset < nameLength ) {
			error = "zone record has invalid name length";
			return false;
		}
		zone.name.assign( reinterpret_cast<const char *>( data.data() + offset ), nameLength );
		offset += nameLength;

		if ( version >= azfmt::kMetadataVersion ) {
			std::uint16_t portalCount = 0;
			if ( !ReadU8( data, offset, zone.materialClass ) ||
				!ReadU8( data, offset, zone.flags ) ||
				!ReadU16( data, offset, portalCount ) ) {
				error = "truncated zone metadata";
				return false;
			}
			if ( zone.materialClass >= static_cast<std::uint8_t>( azfmt::MaterialClass::Count ) ) {
				error = "zone record has unknown material class";
				return false;
			}
			if ( portalCount > azfmt::kMaxZonePortals ) {
				error = "zone record has too many portals";
				return false;
			}
			zone.portals.reserve( portalCount );
			for ( std::uint16_t portalIndex = 0; portalIndex < portalCount; ++portalIndex ) {
				Portal portal;
				if ( !ReadU32( data, offset, portal.targetZone ) ||
					!ReadVec3( data, offset, portal.mins ) ||
					!ReadVec3( data, offset, portal.maxs ) ||
					!ReadF32( data, offset, portal.openness ) ) {
					error = "truncated zone portal";
					return false;
				}
				if ( version >= azfmt::kPortalTuningVersion ) {
					if ( !ReadF32( data, offset, portal.blendDistance ) ||
						!ReadF32( data, offset, portal.minimumBlend ) ||
						!ReadF32( data, offset, portal.maximumBlend ) ||
						!ReadU8( data, offset, portal.blendCurve ) ) {
						error = "truncated zone portal tuning";
						return false;
					}
				}
				if ( portal.targetZone >= zoneCount ) {
					error = "zone portal target is out of range";
					return false;
				}
				if ( !std::isfinite( portal.openness ) ||
					!std::isfinite( portal.blendDistance ) ||
					!std::isfinite( portal.minimumBlend ) ||
					!std::isfinite( portal.maximumBlend ) ||
					portal.blendCurve >= static_cast<std::uint8_t>( azfmt::PortalBlendCurve::Count ) ) {
					error = "zone portal tuning is invalid";
					return false;
				}
				NormalizePortalTuning( portal );
				zone.portals.push_back( portal );
			}
		}
		zones.push_back( zone );
	}
	if ( offset != data.size() ) {
		error = "audio-zone file has trailing bytes";
		return false;
	}
	return true;
}

static bool ReadWholeBinaryFile( const std::filesystem::path &path, std::vector<std::uint8_t> &contents, std::string &error ) {
	std::error_code ec;
	const std::uintmax_t size = std::filesystem::file_size( path, ec );
	if ( ec ) {
		error = "could not stat " + path.string() + ": " + ec.message();
		return false;
	}
	if ( size > static_cast<std::uintmax_t>( std::numeric_limits<std::size_t>::max() ) ) {
		error = "file is too large";
		return false;
	}

	std::ifstream file( path, std::ios::binary );
	if ( !file ) {
		error = "could not open " + path.string();
		return false;
	}
	contents.assign( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
	return true;
}

constexpr std::int32_t kBspIdent = ( 'P' << 24 ) + ( 'S' << 16 ) + ( 'B' << 8 ) + 'I';
constexpr std::int32_t kBspVersionQ3 = 46;
constexpr std::int32_t kBspVersionQL = 47;
constexpr std::size_t kBspHeaderLumps = 17;
constexpr std::size_t kBspHeaderSize = 8u + kBspHeaderLumps * 8u;
constexpr std::size_t kBspMaxQpath = 64u;
constexpr float kMinimumLeafExtent = 4.0f;
constexpr float kPortalMinimumOpenness = 0.04f;
constexpr std::uint8_t kGeneratedEnvironmentFlags = azfmt::ZoneFlagOutdoor | azfmt::ZoneFlagUnderwater;

// Shader evidence weights by reference role. Visible draw surfaces are what the
// listener actually stands in front of, so they outrank brush bodies, and brush
// sides only count as weak supporting evidence: a single leaf routinely
// references several times more brush sides than draw surfaces.
constexpr float kDrawSurfaceRoleWeight = 4.0f;
constexpr float kBrushBodyRoleWeight = 1.0f;
constexpr float kBrushSideRoleWeight = 0.25f;

// A brush only contributes liquid contents when it actually contains the leaf.
// Leaf brush lists also name brushes that merely touch the leaf, which used to
// drown dry rooms next to water in the underwater preset.
constexpr float kBrushContainmentEpsilon = 0.25f;

// Fraction of a volume's draw-surface evidence that has to be sky before it is
// treated as open air, and the relaxed fraction used once a volume is large
// enough that it cannot be an enclosed room.
constexpr double kOutdoorSkyFraction = 0.10;
constexpr double kOpenAirSkyFraction = 0.01;
constexpr float kOpenAirVolume = 256.0f * 1024.0f * 1024.0f;

// Two leaf boxes are connected when they touch within this distance. Anything
// further apart has solid geometry between it, so zones never merge across a
// wall no matter how far the rest of the tuning is relaxed.
constexpr float kLeafAdjacencyGapEpsilon = 8.0f;

// Merge tuning schedules. The quality schedule always runs: BSP leaves are
// fragments of rooms, so recovering room-scale volumes is what makes the preset
// classification meaningful, not just what keeps the zone count down. The
// coarsen schedule only runs while the zone count is still over the cap.
//
// A room a BSP plane cut in half recombines with all three numbers at almost
// exactly 1.0, because the two halves tile the merged box perfectly. That is why
// the first pass can afford to be strict, and why every later pass only has to
// give up a little accuracy at a time.
//
// maximumVolumeInflation bounds one merge against the two boxes going into it.
// maximumFillInflation bounds the merged box against the leaf volume it actually
// contains, which is what stops a long chain of individually cheap merges from
// ratcheting a zone up to the size of the map.
struct MergeTuning {
	float minimumFaceCoverage;
	float maximumVolumeInflation;
	float maximumFillInflation;
};

constexpr MergeTuning kQualityMergeSchedule[] = {
	{ 0.90f, 1.02f, 1.06f },
	{ 0.75f, 1.06f, 1.15f },
	{ 0.60f, 1.12f, 1.30f },
	{ 0.48f, 1.18f, 1.45f },
	{ 0.40f, 1.24f, 1.60f }
};

constexpr MergeTuning kCoarsenMergeSchedule[] = {
	{ 0.35f, 1.40f, 2.00f },
	{ 0.28f, 1.65f, 2.75f },
	{ 0.20f, 2.00f, 4.00f },
	{ 0.12f, 2.60f, 6.50f },
	{ 0.06f, 3.50f, 12.0f }
};

// Generated priorities are rank based so that overlapping generated zones never
// tie: the runtime resolves ties by volume anyway, and encoding that ordering in
// the priority makes every lookup deterministic. Both bands stay negative so
// hand-authored zones at the default priority 0 still win.
constexpr std::int32_t kGeneratedLiquidPriorityBase = -600;
constexpr std::int32_t kGeneratedPriorityBase = -2048;

// Portal blend tuning derived from the opening geometry.
constexpr float kPortalBlendDistanceScale = 1.5f;
constexpr float kMinimumGeneratedPortalBlendDistance = 64.0f;
constexpr float kMaximumGeneratedPortalBlendDistance = 512.0f;
constexpr float kMinimumGeneratedPortalMaxBlend = 0.15f;
constexpr float kMaximumGeneratedPortalMaxBlend = 0.55f;
constexpr float kWidePortalOpenness = 0.75f;
constexpr float kNarrowPortalOpenness = 0.25f;

enum BspLumpIndex {
	BspLumpEntities = 0,
	BspLumpShaders = 1,
	BspLumpPlanes = 2,
	BspLumpNodes = 3,
	BspLumpLeafs = 4,
	BspLumpLeafSurfaces = 5,
	BspLumpLeafBrushes = 6,
	BspLumpModels = 7,
	BspLumpBrushes = 8,
	BspLumpBrushSides = 9,
	BspLumpDrawVerts = 10,
	BspLumpDrawIndexes = 11,
	BspLumpFogs = 12,
	BspLumpSurfaces = 13,
	BspLumpLightmaps = 14,
	BspLumpLightGrid = 15,
	BspLumpVisibility = 16
};

struct BspLump {
	std::size_t offset = 0;
	std::size_t length = 0;
};

struct BspShader {
	std::string name;
	std::int32_t surfaceFlags = 0;
	std::int32_t contentFlags = 0;
};

struct BspLeaf {
	std::int32_t cluster = -1;
	std::int32_t area = -1;
	Vec3 mins;
	Vec3 maxs;
	std::int32_t firstLeafSurface = 0;
	std::int32_t numLeafSurfaces = 0;
	std::int32_t firstLeafBrush = 0;
	std::int32_t numLeafBrushes = 0;
};

struct BspBrush {
	std::int32_t firstSide = 0;
	std::int32_t numSides = 0;
	std::int32_t shaderNum = -1;
	std::int32_t contentFlags = 0;
};

struct BspBrushSide {
	std::int32_t planeNum = 0;
	std::int32_t shaderNum = -1;
	std::int32_t surfaceFlags = 0;
};

struct BspPlane {
	Vec3 normal;
	float distance = 0.0f;
};

struct BspSurface {
	std::int32_t shaderNum = -1;
	std::int32_t surfaceType = 0;
};

struct MaterialOverrideRule {
	std::string pattern;
	std::uint8_t materialClass = static_cast<std::uint8_t>( azfmt::MaterialClass::Unknown );
	int preset = -1;
	std::uint8_t flags = 0;
	float weight = 4.0f;
	bool wildcard = false;
};

struct BspMap {
	std::int32_t version = 0;
	Vec3 worldMins;
	Vec3 worldMaxs;
	bool haveWorldBounds = false;
	std::vector<BspShader> shaders;
	std::vector<BspPlane> planes;
	std::vector<BspLeaf> leafs;
	std::vector<std::int32_t> leafSurfaces;
	std::vector<std::int32_t> leafBrushes;
	std::vector<BspBrush> brushes;
	std::vector<BspBrushSide> brushSides;
	std::vector<BspSurface> surfaces;
};

struct LeafAcoustics {
	// Contents of brushes that actually contain the volume, not of every brush
	// the leaf happens to reference.
	std::int32_t contents = 0;
	std::array<double, static_cast<std::size_t>( azfmt::MaterialClass::Count )> materialVotes = {};
	std::array<double, static_cast<std::size_t>( azfmt::Preset::Count )> presetVotes = {};
	double drawSurfaceWeight = 0.0;
	double skyDrawSurfaceWeight = 0.0;
	std::uint8_t overrideFlags = 0;
};

struct GeneratedZone {
	Zone zone;
	int area = -1;
	int cluster = -1;
	float weight = 1.0f;
	LeafAcoustics acoustics;
};

static std::int32_t ReadBspI32At( const std::vector<std::uint8_t> &data, std::size_t offset ) {
	if ( offset > data.size() || data.size() - offset < 4u ) {
		return 0;
	}
	const std::uint32_t value = static_cast<std::uint32_t>( data[offset] ) |
		( static_cast<std::uint32_t>( data[offset + 1u] ) << 8u ) |
		( static_cast<std::uint32_t>( data[offset + 2u] ) << 16u ) |
		( static_cast<std::uint32_t>( data[offset + 3u] ) << 24u );
	return static_cast<std::int32_t>( value );
}

static float ReadBspF32At( const std::vector<std::uint8_t> &data, std::size_t offset ) {
	const std::uint32_t bits = static_cast<std::uint32_t>( ReadBspI32At( data, offset ) );
	float value = 0.0f;
	std::memcpy( &value, &bits, sizeof( value ) );
	return std::isfinite( value ) ? value : 0.0f;
}

static std::string ReadBspStringAt( const std::vector<std::uint8_t> &data, std::size_t offset, std::size_t maxLength ) {
	std::string text;
	if ( offset >= data.size() ) {
		return text;
	}
	const std::size_t available = ( std::min )( maxLength, data.size() - offset );
	for ( std::size_t i = 0; i < available && data[offset + i] != 0; ++i ) {
		text.push_back( static_cast<char>( data[offset + i] ) );
	}
	return text;
}

static bool ReadBspLumps( const std::vector<std::uint8_t> &data, std::array<BspLump, kBspHeaderLumps> &lumps, std::int32_t &version, std::string &error ) {
	if ( data.size() < kBspHeaderSize ) {
		error = "BSP header is truncated";
		return false;
	}
	const std::int32_t ident = ReadBspI32At( data, 0u );
	version = ReadBspI32At( data, 4u );
	if ( ident != kBspIdent ) {
		error = "not an IBSP file";
		return false;
	}
	if ( version != kBspVersionQ3 && version != kBspVersionQL ) {
		error = "unsupported BSP version " + std::to_string( version ) + "; automatic audio-zone generation supports IBSP 46/47";
		return false;
	}

	for ( std::size_t i = 0; i < kBspHeaderLumps; ++i ) {
		const std::size_t offset = 8u + i * 8u;
		const std::int32_t fileOffset = ReadBspI32At( data, offset );
		const std::int32_t fileLength = ReadBspI32At( data, offset + 4u );
		if ( fileOffset < 0 || fileLength < 0 ) {
			error = "BSP has a negative lump offset or length";
			return false;
		}
		const std::size_t begin = static_cast<std::size_t>( fileOffset );
		const std::size_t length = static_cast<std::size_t>( fileLength );
		if ( begin > data.size() || length > data.size() - begin ) {
			error = "BSP has an out-of-range lump";
			return false;
		}
		lumps[i].offset = begin;
		lumps[i].length = length;
	}
	return true;
}

static bool ValidateLumpRecordSize( const BspLump &lump, std::size_t recordSize, const char *name, std::string &error ) {
	if ( recordSize == 0u || lump.length % recordSize != 0u ) {
		error = std::string( "BSP " ) + name + " lump has a malformed size";
		return false;
	}
	return true;
}

static Vec3 ReadBspBoundsVec3I32( const std::vector<std::uint8_t> &data, std::size_t offset ) {
	Vec3 value;
	value.x = static_cast<float>( ReadBspI32At( data, offset ) );
	value.y = static_cast<float>( ReadBspI32At( data, offset + 4u ) );
	value.z = static_cast<float>( ReadBspI32At( data, offset + 8u ) );
	return value;
}

static void NormalizeVecBounds( Vec3 &mins, Vec3 &maxs ) {
	if ( mins.x > maxs.x ) {
		std::swap( mins.x, maxs.x );
	}
	if ( mins.y > maxs.y ) {
		std::swap( mins.y, maxs.y );
	}
	if ( mins.z > maxs.z ) {
		std::swap( mins.z, maxs.z );
	}
}

static bool LoadBspMap( const std::filesystem::path &path, BspMap &map, std::string &error ) {
	std::vector<std::uint8_t> data;
	if ( !ReadWholeBinaryFile( path, data, error ) ) {
		return false;
	}

	std::array<BspLump, kBspHeaderLumps> lumps;
	if ( !ReadBspLumps( data, lumps, map.version, error ) ) {
		return false;
	}

	if ( !ValidateLumpRecordSize( lumps[BspLumpShaders], 72u, "shader", error ) ||
		!ValidateLumpRecordSize( lumps[BspLumpPlanes], 16u, "plane", error ) ||
		!ValidateLumpRecordSize( lumps[BspLumpLeafs], 48u, "leaf", error ) ||
		!ValidateLumpRecordSize( lumps[BspLumpLeafSurfaces], 4u, "leaf-surface", error ) ||
		!ValidateLumpRecordSize( lumps[BspLumpLeafBrushes], 4u, "leaf-brush", error ) ||
		!ValidateLumpRecordSize( lumps[BspLumpBrushes], 12u, "brush", error ) ||
		!ValidateLumpRecordSize( lumps[BspLumpBrushSides], 8u, "brush-side", error ) ||
		!ValidateLumpRecordSize( lumps[BspLumpSurfaces], 104u, "surface", error ) ) {
		return false;
	}

	const BspLump &shaderLump = lumps[BspLumpShaders];
	map.shaders.reserve( shaderLump.length / 72u );
	for ( std::size_t offset = shaderLump.offset; offset < shaderLump.offset + shaderLump.length; offset += 72u ) {
		BspShader shader;
		shader.name = ReadBspStringAt( data, offset, kBspMaxQpath );
		shader.surfaceFlags = ReadBspI32At( data, offset + 64u );
		shader.contentFlags = ReadBspI32At( data, offset + 68u );
		map.shaders.push_back( shader );
	}

	// The world model bounds are the extent of the actual map. BSP leaves outside
	// the sealed hull can carry the engine's world-coordinate limits instead, and
	// one such leaf is large enough to swallow the whole map during merging.
	const BspLump &modelLump = lumps[BspLumpModels];
	if ( modelLump.length >= 40u ) {
		Vec3 mins;
		Vec3 maxs;
		mins.x = ReadBspF32At( data, modelLump.offset );
		mins.y = ReadBspF32At( data, modelLump.offset + 4u );
		mins.z = ReadBspF32At( data, modelLump.offset + 8u );
		maxs.x = ReadBspF32At( data, modelLump.offset + 12u );
		maxs.y = ReadBspF32At( data, modelLump.offset + 16u );
		maxs.z = ReadBspF32At( data, modelLump.offset + 20u );
		NormalizeVecBounds( mins, maxs );
		if ( maxs.x - mins.x >= kMinimumLeafExtent &&
			maxs.y - mins.y >= kMinimumLeafExtent &&
			maxs.z - mins.z >= kMinimumLeafExtent ) {
			map.worldMins = mins;
			map.worldMaxs = maxs;
			map.haveWorldBounds = true;
		}
	}

	const BspLump &planeLump = lumps[BspLumpPlanes];
	map.planes.reserve( planeLump.length / 16u );
	for ( std::size_t offset = planeLump.offset; offset < planeLump.offset + planeLump.length; offset += 16u ) {
		BspPlane plane;
		plane.normal.x = ReadBspF32At( data, offset );
		plane.normal.y = ReadBspF32At( data, offset + 4u );
		plane.normal.z = ReadBspF32At( data, offset + 8u );
		plane.distance = ReadBspF32At( data, offset + 12u );
		map.planes.push_back( plane );
	}

	const BspLump &leafLump = lumps[BspLumpLeafs];
	map.leafs.reserve( leafLump.length / 48u );
	for ( std::size_t offset = leafLump.offset; offset < leafLump.offset + leafLump.length; offset += 48u ) {
		BspLeaf leaf;
		leaf.cluster = ReadBspI32At( data, offset );
		leaf.area = ReadBspI32At( data, offset + 4u );
		leaf.mins = ReadBspBoundsVec3I32( data, offset + 8u );
		leaf.maxs = ReadBspBoundsVec3I32( data, offset + 20u );
		NormalizeVecBounds( leaf.mins, leaf.maxs );
		leaf.firstLeafSurface = ReadBspI32At( data, offset + 32u );
		leaf.numLeafSurfaces = ReadBspI32At( data, offset + 36u );
		leaf.firstLeafBrush = ReadBspI32At( data, offset + 40u );
		leaf.numLeafBrushes = ReadBspI32At( data, offset + 44u );
		map.leafs.push_back( leaf );
	}

	const BspLump &leafSurfaceLump = lumps[BspLumpLeafSurfaces];
	map.leafSurfaces.reserve( leafSurfaceLump.length / 4u );
	for ( std::size_t offset = leafSurfaceLump.offset; offset < leafSurfaceLump.offset + leafSurfaceLump.length; offset += 4u ) {
		map.leafSurfaces.push_back( ReadBspI32At( data, offset ) );
	}

	const BspLump &leafBrushLump = lumps[BspLumpLeafBrushes];
	map.leafBrushes.reserve( leafBrushLump.length / 4u );
	for ( std::size_t offset = leafBrushLump.offset; offset < leafBrushLump.offset + leafBrushLump.length; offset += 4u ) {
		map.leafBrushes.push_back( ReadBspI32At( data, offset ) );
	}

	const BspLump &brushLump = lumps[BspLumpBrushes];
	map.brushes.reserve( brushLump.length / 12u );
	for ( std::size_t offset = brushLump.offset; offset < brushLump.offset + brushLump.length; offset += 12u ) {
		BspBrush brush;
		brush.firstSide = ReadBspI32At( data, offset );
		brush.numSides = ReadBspI32At( data, offset + 4u );
		brush.shaderNum = ReadBspI32At( data, offset + 8u );
		if ( brush.shaderNum >= 0 && static_cast<std::size_t>( brush.shaderNum ) < map.shaders.size() ) {
			brush.contentFlags = map.shaders[static_cast<std::size_t>( brush.shaderNum )].contentFlags;
		}
		map.brushes.push_back( brush );
	}

	const BspLump &brushSideLump = lumps[BspLumpBrushSides];
	map.brushSides.reserve( brushSideLump.length / 8u );
	for ( std::size_t offset = brushSideLump.offset; offset < brushSideLump.offset + brushSideLump.length; offset += 8u ) {
		BspBrushSide side;
		side.planeNum = ReadBspI32At( data, offset );
		side.shaderNum = ReadBspI32At( data, offset + 4u );
		if ( side.shaderNum >= 0 && static_cast<std::size_t>( side.shaderNum ) < map.shaders.size() ) {
			side.surfaceFlags = map.shaders[static_cast<std::size_t>( side.shaderNum )].surfaceFlags;
		}
		map.brushSides.push_back( side );
	}

	const BspLump &surfaceLump = lumps[BspLumpSurfaces];
	map.surfaces.reserve( surfaceLump.length / 104u );
	for ( std::size_t offset = surfaceLump.offset; offset < surfaceLump.offset + surfaceLump.length; offset += 104u ) {
		BspSurface surface;
		surface.shaderNum = ReadBspI32At( data, offset );
		surface.surfaceType = ReadBspI32At( data, offset + 8u );
		map.surfaces.push_back( surface );
	}

	if ( map.leafs.empty() ) {
		error = "BSP has no leaves";
		return false;
	}
	return true;
}

static float AxisOverlap( float aMin, float aMax, float bMin, float bMax ) {
	return ( std::min )( aMax, bMax ) - ( std::max )( aMin, bMin );
}

static float BoundsVolume( const Vec3 &mins, const Vec3 &maxs ) {
	return ( std::max )( 1.0f, maxs.x - mins.x ) *
		( std::max )( 1.0f, maxs.y - mins.y ) *
		( std::max )( 1.0f, maxs.z - mins.z );
}

static float ExtentX( const Zone &zone ) {
	return zone.maxs.x - zone.mins.x;
}

static float ExtentY( const Zone &zone ) {
	return zone.maxs.y - zone.mins.y;
}

static float ExtentZ( const Zone &zone ) {
	return zone.maxs.z - zone.mins.z;
}

static float ZoneVolume( const Zone &zone ) {
	return ( std::max )( 1.0f, ExtentX( zone ) ) *
		( std::max )( 1.0f, ExtentY( zone ) ) *
		( std::max )( 1.0f, ExtentZ( zone ) );
}

static void ExpandZoneBounds( Zone &zone, const Vec3 &mins, const Vec3 &maxs ) {
	if ( !zone.haveMins || !zone.haveMaxs ) {
		zone.mins = mins;
		zone.maxs = maxs;
		zone.haveMins = true;
		zone.haveMaxs = true;
		return;
	}
	zone.mins.x = ( std::min )( zone.mins.x, mins.x );
	zone.mins.y = ( std::min )( zone.mins.y, mins.y );
	zone.mins.z = ( std::min )( zone.mins.z, mins.z );
	zone.maxs.x = ( std::max )( zone.maxs.x, maxs.x );
	zone.maxs.y = ( std::max )( zone.maxs.y, maxs.y );
	zone.maxs.z = ( std::max )( zone.maxs.z, maxs.z );
}

static std::string LowercasePathToken( std::string value ) {
	for ( char &ch : value ) {
		if ( ch == '\\' ) {
			ch = '/';
		} else {
			ch = static_cast<char>( std::tolower( static_cast<unsigned char>( ch ) ) );
		}
	}
	return value;
}

static bool ContainsAnyToken( const std::string &text, const std::vector<std::string> &tokens ) {
	for ( const std::string &token : tokens ) {
		if ( text.find( token ) != std::string::npos ) {
			return true;
		}
	}
	return false;
}

static std::string TrimAscii( const std::string &text ) {
	std::size_t begin = 0;
	while ( begin < text.size() && std::isspace( static_cast<unsigned char>( text[begin] ) ) ) {
		++begin;
	}
	std::size_t end = text.size();
	while ( end > begin && std::isspace( static_cast<unsigned char>( text[end - 1u] ) ) ) {
		--end;
	}
	return text.substr( begin, end - begin );
}

static bool ParsePositiveFloatText( const std::string &text, float minimum, float maximum, float &out ) {
	char *end = nullptr;
	errno = 0;
	const float value = std::strtof( text.c_str(), &end );
	if ( text.empty() || end == text.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite( value ) ||
		value < minimum || value > maximum ) {
		return false;
	}
	out = value;
	return true;
}

static bool LoadMaterialOverrideMap( const std::filesystem::path &path, std::vector<MaterialOverrideRule> &rules, std::string &error ) {
	std::string source;
	if ( !ReadWholeFile( path, source, error ) ) {
		return false;
	}

	rules.clear();
	std::istringstream stream( source );
	std::string line;
	int lineNumber = 0;
	while ( std::getline( stream, line ) ) {
		++lineNumber;
		std::size_t comment = line.find( '#' );
		const std::size_t slashComment = line.find( "//" );
		if ( slashComment != std::string::npos && ( comment == std::string::npos || slashComment < comment ) ) {
			comment = slashComment;
		}
		if ( comment != std::string::npos ) {
			line.erase( comment );
		}
		line = TrimAscii( line );
		if ( line.empty() ) {
			continue;
		}

		std::istringstream fields( line );
		std::string pattern;
		std::string materialName;
		if ( !( fields >> pattern >> materialName ) ) {
			error = path.string() + ":" + std::to_string( lineNumber ) + ": expected '<shader-pattern> <material>'";
			return false;
		}

		MaterialOverrideRule rule;
		rule.pattern = LowercasePathToken( pattern );
		rule.wildcard = rule.pattern.find( '*' ) != std::string::npos || rule.pattern.find( '?' ) != std::string::npos;
		if ( !ParseMaterialName( materialName, rule.materialClass ) ) {
			error = path.string() + ":" + std::to_string( lineNumber ) + ": unknown material class '" + materialName + "'";
			return false;
		}
		if ( rule.pattern.empty() ) {
			error = path.string() + ":" + std::to_string( lineNumber ) + ": empty shader pattern";
			return false;
		}

		std::string property;
		while ( fields >> property ) {
			const std::string normalized = Lowercase( property );
			if ( normalized == "preset" || normalized == "environment" ) {
				std::string presetName;
				std::uint32_t preset = 0;
				if ( !( fields >> presetName ) || !ParsePresetName( presetName, preset ) ) {
					error = path.string() + ":" + std::to_string( lineNumber ) + ": expected environment preset after '" + property + "'";
					return false;
				}
				rule.preset = static_cast<int>( preset );
			} else if ( normalized == "flag" ) {
				std::string flagName;
				std::uint8_t flag = 0;
				if ( !( fields >> flagName ) || !ParseZoneFlagName( flagName, flag ) ) {
					error = path.string() + ":" + std::to_string( lineNumber ) + ": expected zone flag after 'flag'";
					return false;
				}
				rule.flags = static_cast<std::uint8_t>( rule.flags | flag );
			} else if ( normalized == "outdoor" || normalized == "outdoors" ) {
				rule.flags = static_cast<std::uint8_t>( rule.flags | azfmt::ZoneFlagOutdoor );
			} else if ( normalized == "underwater" ) {
				rule.flags = static_cast<std::uint8_t>( rule.flags | azfmt::ZoneFlagUnderwater );
			} else if ( normalized == "weight" ) {
				std::string weightText;
				if ( !( fields >> weightText ) || !ParsePositiveFloatText( weightText, 0.1f, 64.0f, rule.weight ) ) {
					error = path.string() + ":" + std::to_string( lineNumber ) + ": weight must be in range 0.1..64";
					return false;
				}
			} else {
				error = path.string() + ":" + std::to_string( lineNumber ) + ": unknown material override property '" + property + "'";
				return false;
			}
		}
		rules.push_back( rule );
	}

	if ( rules.empty() ) {
		error = path.string() + ": material override map did not contain any rules";
		return false;
	}
	return true;
}

static bool WildcardMatch( const char *pattern, const char *text ) {
	const char *star = nullptr;
	const char *starText = nullptr;
	while ( *text != '\0' ) {
		if ( *pattern == '?' || *pattern == *text ) {
			++pattern;
			++text;
			continue;
		}
		if ( *pattern == '*' ) {
			star = pattern++;
			starText = text;
			continue;
		}
		if ( star != nullptr ) {
			pattern = star + 1;
			text = ++starText;
			continue;
		}
		return false;
	}
	while ( *pattern == '*' ) {
		++pattern;
	}
	return *pattern == '\0';
}

static bool ShaderNameMatchesRule( const std::string &shaderName, const MaterialOverrideRule &rule ) {
	if ( rule.wildcard ) {
		return WildcardMatch( rule.pattern.c_str(), shaderName.c_str() );
	}
	return shaderName.find( rule.pattern ) != std::string::npos;
}

static void AddMaterialVote( LeafAcoustics &acoustics, std::uint8_t materialClass, double weight ) {
	if ( materialClass >= static_cast<std::uint8_t>( azfmt::MaterialClass::Count ) ||
		materialClass == static_cast<std::uint8_t>( azfmt::MaterialClass::Unknown ) ) {
		return;
	}
	acoustics.materialVotes[materialClass] += weight;
}

static void MergeLeafAcoustics( LeafAcoustics &target, const LeafAcoustics &source ) {
	target.contents |= source.contents;
	for ( std::size_t i = 0; i < target.materialVotes.size(); ++i ) {
		target.materialVotes[i] += source.materialVotes[i];
	}
	for ( std::size_t i = 0; i < target.presetVotes.size(); ++i ) {
		target.presetVotes[i] += source.presetVotes[i];
	}
	target.drawSurfaceWeight += source.drawSurfaceWeight;
	target.skyDrawSurfaceWeight += source.skyDrawSurfaceWeight;
	target.overrideFlags = static_cast<std::uint8_t>( target.overrideFlags | source.overrideFlags );
}

static double SkyDrawSurfaceFraction( const LeafAcoustics &acoustics ) {
	if ( acoustics.drawSurfaceWeight <= 0.0 ) {
		return 0.0;
	}
	return acoustics.skyDrawSurfaceWeight / acoustics.drawSurfaceWeight;
}

// Open air is decided by how much of what the volume can see is sky. A huge
// volume needs proportionally less of it: the void of a space map sees a lot of
// platform geometry and only a little skybox, but it is still open air rather
// than a reverberant hall.
static bool AcousticsAreOutdoor( const LeafAcoustics &acoustics, const Vec3 &mins, const Vec3 &maxs ) {
	if ( ( acoustics.overrideFlags & azfmt::ZoneFlagOutdoor ) != 0 ) {
		return true;
	}
	if ( acoustics.skyDrawSurfaceWeight <= 0.0 ) {
		return false;
	}
	const float volume = BoundsVolume( mins, maxs );
	const double required = volume >= kOpenAirVolume ? kOpenAirSkyFraction : kOutdoorSkyFraction;
	return SkyDrawSurfaceFraction( acoustics ) >= required;
}

static bool AcousticsAreUnderwater( const LeafAcoustics &acoustics ) {
	return ( acoustics.contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) != 0 ||
		( acoustics.overrideFlags & azfmt::ZoneFlagUnderwater ) != 0;
}

static int DominantPresetOverride( const LeafAcoustics &acoustics ) {
	int bestPreset = -1;
	double bestWeight = 0.0;
	for ( std::size_t i = 0; i < acoustics.presetVotes.size(); ++i ) {
		if ( acoustics.presetVotes[i] > bestWeight ) {
			bestWeight = acoustics.presetVotes[i];
			bestPreset = static_cast<int>( i );
		}
	}
	return bestWeight > 0.0 ? bestPreset : -1;
}

static void AccumulateShaderAcoustics( const BspShader &shader, const std::vector<MaterialOverrideRule> &overrides, float roleWeight, LeafAcoustics &acoustics ) {
	const double weight = static_cast<double>( roleWeight );
	if ( shader.surfaceFlags & SURF_METALSTEPS ) {
		AddMaterialVote( acoustics, static_cast<std::uint8_t>( azfmt::MaterialClass::Metal ), weight );
	}
	if ( shader.surfaceFlags & SURF_FLESH ) {
		AddMaterialVote( acoustics, static_cast<std::uint8_t>( azfmt::MaterialClass::Soft ), weight );
	}

	const std::string name = LowercasePathToken( shader.name );
	if ( ( shader.surfaceFlags & SURF_SKY ) != 0 || ContainsAnyToken( name, { "skies/", "/sky" } ) ) {
		AddMaterialVote( acoustics, static_cast<std::uint8_t>( azfmt::MaterialClass::Sky ), weight );
	}
	if ( ContainsAnyToken( name, { "water", "slime", "lava", "liquid", "pool", "slosh" } ) ) {
		AddMaterialVote( acoustics, static_cast<std::uint8_t>( azfmt::MaterialClass::Liquid ), weight );
	}
	if ( ContainsAnyToken( name, { "stone", "rock", "gothic", "brick", "concrete", "cement", "base_wall", "marble", "tile" } ) ) {
		AddMaterialVote( acoustics, static_cast<std::uint8_t>( azfmt::MaterialClass::Stone ), weight );
	}
	if ( ContainsAnyToken( name, { "metal", "grate", "proto", "base_trim", "clang", "steel", "iron" } ) ) {
		AddMaterialVote( acoustics, static_cast<std::uint8_t>( azfmt::MaterialClass::Metal ), weight );
	}
	if ( ContainsAnyToken( name, { "flesh", "organic", "cloth", "terrain", "grass", "sand", "dirt", "carpet", "wood" } ) ) {
		AddMaterialVote( acoustics, static_cast<std::uint8_t>( azfmt::MaterialClass::Soft ), weight );
	}

	for ( const MaterialOverrideRule &rule : overrides ) {
		if ( !ShaderNameMatchesRule( name, rule ) ) {
			continue;
		}
		const double weighted = weight * static_cast<double>( rule.weight );
		AddMaterialVote( acoustics, rule.materialClass, weighted );
		if ( rule.preset >= 0 && rule.preset < static_cast<int>( acoustics.presetVotes.size() ) ) {
			acoustics.presetVotes[static_cast<std::size_t>( rule.preset )] += weighted;
		}
		acoustics.overrideFlags = static_cast<std::uint8_t>( acoustics.overrideFlags | rule.flags );
	}
}

// A BSP leaf is convex, so testing its centre against the brush half-spaces is
// enough to tell "the leaf is inside this brush" from "the leaf merely lists a
// neighbouring brush".
static bool BrushContainsPoint( const BspMap &map, const BspBrush &brush, const Vec3 &point ) {
	if ( brush.numSides <= 0 || brush.firstSide < 0 ||
		static_cast<std::size_t>( brush.firstSide ) >= map.brushSides.size() ) {
		return false;
	}
	const std::size_t begin = static_cast<std::size_t>( brush.firstSide );
	if ( static_cast<std::size_t>( brush.numSides ) > map.brushSides.size() - begin ) {
		return false;
	}
	for ( std::size_t i = 0; i < static_cast<std::size_t>( brush.numSides ); ++i ) {
		const BspBrushSide &side = map.brushSides[begin + i];
		if ( side.planeNum < 0 || static_cast<std::size_t>( side.planeNum ) >= map.planes.size() ) {
			return false;
		}
		const BspPlane &plane = map.planes[static_cast<std::size_t>( side.planeNum )];
		const float distance = point.x * plane.normal.x + point.y * plane.normal.y + point.z * plane.normal.z - plane.distance;
		if ( distance > kBrushContainmentEpsilon ) {
			return false;
		}
	}
	return true;
}

static LeafAcoustics AnalyzeLeafAcoustics( const BspMap &map, const BspLeaf &leaf, const Vec3 &center, const std::vector<MaterialOverrideRule> &overrides ) {
	LeafAcoustics acoustics;

	if ( leaf.firstLeafSurface >= 0 && leaf.numLeafSurfaces > 0 &&
		static_cast<std::size_t>( leaf.firstLeafSurface ) <= map.leafSurfaces.size() ) {
		const std::size_t begin = static_cast<std::size_t>( leaf.firstLeafSurface );
		const std::size_t count = ( std::min )( static_cast<std::size_t>( leaf.numLeafSurfaces ), map.leafSurfaces.size() - begin );
		for ( std::size_t i = 0; i < count; ++i ) {
			const std::int32_t surfaceIndex = map.leafSurfaces[begin + i];
			if ( surfaceIndex < 0 || static_cast<std::size_t>( surfaceIndex ) >= map.surfaces.size() ) {
				continue;
			}
			const std::int32_t shaderNum = map.surfaces[static_cast<std::size_t>( surfaceIndex )].shaderNum;
			if ( shaderNum < 0 || static_cast<std::size_t>( shaderNum ) >= map.shaders.size() ) {
				continue;
			}
			const BspShader &shader = map.shaders[static_cast<std::size_t>( shaderNum )];
			acoustics.drawSurfaceWeight += static_cast<double>( kDrawSurfaceRoleWeight );
			if ( shader.surfaceFlags & SURF_SKY ) {
				acoustics.skyDrawSurfaceWeight += static_cast<double>( kDrawSurfaceRoleWeight );
			}
			AccumulateShaderAcoustics( shader, overrides, kDrawSurfaceRoleWeight, acoustics );
		}
	}

	if ( leaf.firstLeafBrush >= 0 && leaf.numLeafBrushes > 0 &&
		static_cast<std::size_t>( leaf.firstLeafBrush ) <= map.leafBrushes.size() ) {
		const std::size_t begin = static_cast<std::size_t>( leaf.firstLeafBrush );
		const std::size_t count = ( std::min )( static_cast<std::size_t>( leaf.numLeafBrushes ), map.leafBrushes.size() - begin );
		for ( std::size_t i = 0; i < count; ++i ) {
			const std::int32_t brushIndex = map.leafBrushes[begin + i];
			if ( brushIndex < 0 || static_cast<std::size_t>( brushIndex ) >= map.brushes.size() ) {
				continue;
			}
			const BspBrush &brush = map.brushes[static_cast<std::size_t>( brushIndex )];
			if ( ( brush.contentFlags & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) != 0 &&
				BrushContainsPoint( map, brush, center ) ) {
				acoustics.contents |= brush.contentFlags;
			}
			if ( brush.shaderNum >= 0 && static_cast<std::size_t>( brush.shaderNum ) < map.shaders.size() ) {
				AccumulateShaderAcoustics( map.shaders[static_cast<std::size_t>( brush.shaderNum )], overrides, kBrushBodyRoleWeight, acoustics );
			}
			if ( brush.firstSide >= 0 && brush.numSides > 0 &&
				static_cast<std::size_t>( brush.firstSide ) <= map.brushSides.size() ) {
				const std::size_t sideBegin = static_cast<std::size_t>( brush.firstSide );
				const std::size_t sideCount = ( std::min )( static_cast<std::size_t>( brush.numSides ), map.brushSides.size() - sideBegin );
				for ( std::size_t sideIndex = 0; sideIndex < sideCount; ++sideIndex ) {
					const BspBrushSide &side = map.brushSides[sideBegin + sideIndex];
					if ( side.surfaceFlags & ( SURF_NODRAW | SURF_SKIP | SURF_HINT ) ) {
						continue;
					}
					if ( side.shaderNum >= 0 && static_cast<std::size_t>( side.shaderNum ) < map.shaders.size() ) {
						AccumulateShaderAcoustics( map.shaders[static_cast<std::size_t>( side.shaderNum )], overrides, kBrushSideRoleWeight, acoustics );
					}
				}
			}
		}
	}
	return acoustics;
}

static std::uint8_t MaterialFromAcoustics( const LeafAcoustics &acoustics ) {
	const bool underwater = AcousticsAreUnderwater( acoustics );
	if ( underwater ) {
		return static_cast<std::uint8_t>( azfmt::MaterialClass::Liquid );
	}

	std::uint8_t best = static_cast<std::uint8_t>( azfmt::MaterialClass::Neutral );
	double bestVote = 0.0;
	for ( std::size_t i = 0; i < acoustics.materialVotes.size(); ++i ) {
		// Dry volumes next to water still see the liquid surface, but that is a
		// neighbouring material rather than the medium the listener is in.
		if ( i == static_cast<std::size_t>( azfmt::MaterialClass::Liquid ) ||
			i == static_cast<std::size_t>( azfmt::MaterialClass::Unknown ) ) {
			continue;
		}
		if ( acoustics.materialVotes[i] > bestVote ) {
			bestVote = acoustics.materialVotes[i];
			best = static_cast<std::uint8_t>( i );
		}
	}
	return bestVote > 0.0 ? best : static_cast<std::uint8_t>( azfmt::MaterialClass::Neutral );
}

static std::uint8_t GeneratedFlagsFromAcoustics( const LeafAcoustics &acoustics, const Vec3 &mins, const Vec3 &maxs ) {
	std::uint8_t flags = static_cast<std::uint8_t>( acoustics.overrideFlags & kGeneratedEnvironmentFlags );
	if ( AcousticsAreUnderwater( acoustics ) ) {
		flags = static_cast<std::uint8_t>( flags | azfmt::ZoneFlagUnderwater );
	}
	if ( AcousticsAreOutdoor( acoustics, mins, maxs ) ) {
		flags = static_cast<std::uint8_t>( flags | azfmt::ZoneFlagOutdoor );
	}
	return flags;
}

// Room scale and shape pick the preset; the acoustic material only decides
// whether a room-sized volume is the hard-walled stone-room variant. Letting the
// material short-circuit the size test collapsed almost every zone in a stock
// Quake map into stone-room, because stock Quake surfaces are nearly all stone
// or metal.
static std::uint32_t PresetFromBounds( const Vec3 &mins, const Vec3 &maxs, const LeafAcoustics &acoustics, std::uint8_t material ) {
	const int overridePreset = DominantPresetOverride( acoustics );
	if ( overridePreset >= 0 ) {
		return static_cast<std::uint32_t>( overridePreset );
	}
	if ( AcousticsAreUnderwater( acoustics ) ) {
		return static_cast<std::uint32_t>( azfmt::Preset::Underwater );
	}

	const float x = ( std::max )( kMinimumLeafExtent, maxs.x - mins.x );
	const float y = ( std::max )( kMinimumLeafExtent, maxs.y - mins.y );
	const float z = ( std::max )( kMinimumLeafExtent, maxs.z - mins.z );
	const float horizontalMin = ( std::min )( x, y );
	const float horizontalMax = ( std::max )( x, y );
	const float horizontalRatio = horizontalMax / ( std::max )( kMinimumLeafExtent, horizontalMin );
	const float volume = x * y * z;

	if ( AcousticsAreOutdoor( acoustics, mins, maxs ) && ( horizontalMax >= 256.0f || z >= 192.0f ) ) {
		return static_cast<std::uint32_t>( azfmt::Preset::Outdoors );
	}
	if ( volume > 96.0f * 1024.0f * 1024.0f || horizontalMax > 1536.0f || z > 768.0f ) {
		return static_cast<std::uint32_t>( azfmt::Preset::Hall );
	}
	if ( horizontalRatio > 2.8f && horizontalMin < 384.0f && horizontalMax >= 256.0f && z <= 512.0f ) {
		return static_cast<std::uint32_t>( azfmt::Preset::Hallway );
	}

	const bool hardSurfaced = material == static_cast<std::uint8_t>( azfmt::MaterialClass::Stone ) ||
		material == static_cast<std::uint8_t>( azfmt::MaterialClass::Metal );
	if ( volume > 8.0f * 1024.0f * 1024.0f || horizontalMax > 512.0f || z > 320.0f ) {
		return hardSurfaced
			? static_cast<std::uint32_t>( azfmt::Preset::StoneRoom )
			: static_cast<std::uint32_t>( azfmt::Preset::Room );
	}
	return static_cast<std::uint32_t>( azfmt::Preset::SmallRoom );
}

static void ApplyGeneratedTuning( Zone &zone ) {
	zone.flags |= azfmt::ZoneFlagGenerated;
	zone.transitionMs = azfmt::kDefaultTransitionMs;
	zone.occlusionMultiplier = 1.0f;
	zone.reverbGain = 1.0f;
	zone.directLF = 1.0f;
	zone.directHF = 0.96f;
	zone.wetLF = 1.0f;
	zone.wetHF = 0.92f;

	switch ( static_cast<azfmt::Preset>( zone.preset ) ) {
	case azfmt::Preset::Underwater:
		zone.flags |= azfmt::ZoneFlagUnderwater;
		zone.reverbGain = 0.85f;
		zone.occlusionMultiplier = 1.15f;
		zone.directLF = 0.86f;
		zone.directHF = 0.58f;
		zone.wetLF = 0.82f;
		zone.wetHF = 0.50f;
		zone.transitionMs = 500u;
		break;
	case azfmt::Preset::Outdoors:
		zone.flags |= azfmt::ZoneFlagOutdoor;
		zone.reverbGain = 0.55f;
		zone.occlusionMultiplier = 0.80f;
		zone.directHF = 1.0f;
		zone.wetHF = 0.88f;
		zone.transitionMs = 900u;
		break;
	case azfmt::Preset::Hall:
		zone.reverbGain = 1.15f;
		zone.occlusionMultiplier = 0.90f;
		zone.wetHF = 0.95f;
		zone.transitionMs = 800u;
		break;
	case azfmt::Preset::Hallway:
		zone.reverbGain = 1.05f;
		zone.occlusionMultiplier = 1.05f;
		zone.directHF = 0.94f;
		zone.wetHF = 0.88f;
		break;
	case azfmt::Preset::StoneRoom:
		zone.reverbGain = 1.08f;
		zone.directHF = 0.93f;
		zone.wetHF = 0.86f;
		break;
	case azfmt::Preset::Room:
		zone.reverbGain = 0.96f;
		break;
	case azfmt::Preset::SmallRoom:
	default:
		zone.reverbGain = 0.82f;
		zone.transitionMs = 450u;
		break;
	}

	if ( zone.materialClass == static_cast<std::uint8_t>( azfmt::MaterialClass::Metal ) ) {
		zone.reverbGain = ( std::min )( 1.30f, zone.reverbGain + 0.12f );
		zone.directHF = ( std::min )( zone.directHF, 0.92f );
		zone.wetHF = ( std::min )( zone.wetHF, 0.82f );
	} else if ( zone.materialClass == static_cast<std::uint8_t>( azfmt::MaterialClass::Soft ) ) {
		zone.reverbGain = ( std::max )( 0.45f, zone.reverbGain - 0.18f );
		zone.occlusionMultiplier = ( std::max )( 0.65f, zone.occlusionMultiplier - 0.10f );
		zone.wetHF = ( std::min )( zone.wetHF, 0.78f );
	}
}

// Clips a leaf to the world model bounds and reports whether anything usable is
// left. Opaque leaves and slivers are rejected outright.
static bool ClipLeafBounds( const BspMap &map, const BspLeaf &leaf, Vec3 &mins, Vec3 &maxs ) {
	if ( leaf.cluster < 0 ) {
		return false;
	}
	mins = leaf.mins;
	maxs = leaf.maxs;
	if ( map.haveWorldBounds ) {
		mins.x = ( std::max )( mins.x, map.worldMins.x );
		mins.y = ( std::max )( mins.y, map.worldMins.y );
		mins.z = ( std::max )( mins.z, map.worldMins.z );
		maxs.x = ( std::min )( maxs.x, map.worldMaxs.x );
		maxs.y = ( std::min )( maxs.y, map.worldMaxs.y );
		maxs.z = ( std::min )( maxs.z, map.worldMaxs.z );
	}
	const float x = maxs.x - mins.x;
	const float y = maxs.y - mins.y;
	const float z = maxs.z - mins.z;
	return std::isfinite( x ) && std::isfinite( y ) && std::isfinite( z ) &&
		x >= kMinimumLeafExtent && y >= kMinimumLeafExtent && z >= kMinimumLeafExtent;
}

static void FinalizeGeneratedZone( GeneratedZone &generated ) {
	generated.zone.materialClass = MaterialFromAcoustics( generated.acoustics );
	generated.zone.preset = PresetFromBounds( generated.zone.mins, generated.zone.maxs, generated.acoustics, generated.zone.materialClass );
	generated.zone.flags = static_cast<std::uint8_t>(
		azfmt::ZoneFlagGenerated | GeneratedFlagsFromAcoustics( generated.acoustics, generated.zone.mins, generated.zone.maxs ) );
	ApplyGeneratedTuning( generated.zone );
}

static float ZoneOverlapVolume( const Zone &a, const Zone &b ) {
	const float x = ( std::max )( 0.0f, AxisOverlap( a.mins.x, a.maxs.x, b.mins.x, b.maxs.x ) );
	const float y = ( std::max )( 0.0f, AxisOverlap( a.mins.y, a.maxs.y, b.mins.y, b.maxs.y ) );
	const float z = ( std::max )( 0.0f, AxisOverlap( a.mins.z, a.maxs.z, b.mins.z, b.maxs.z ) );
	return x * y * z;
}

static float ZoneFaceArea( const Zone &zone, int axis ) {
	if ( axis == 0 ) {
		return ( std::max )( 1.0f, ExtentY( zone ) ) * ( std::max )( 1.0f, ExtentZ( zone ) );
	}
	if ( axis == 1 ) {
		return ( std::max )( 1.0f, ExtentX( zone ) ) * ( std::max )( 1.0f, ExtentZ( zone ) );
	}
	return ( std::max )( 1.0f, ExtentX( zone ) ) * ( std::max )( 1.0f, ExtentY( zone ) );
}

static void MergeGeneratedZoneInto( GeneratedZone &target, const GeneratedZone &source ) {
	const float targetWeight = ( std::max )( 1.0f, target.weight );
	const float sourceWeight = ( std::max )( 1.0f, source.weight );
	const float totalWeight = targetWeight + sourceWeight;
	ExpandZoneBounds( target.zone, source.zone.mins, source.zone.maxs );
	if ( target.area != source.area ) {
		target.area = -1;
	}
	if ( target.cluster != source.cluster ) {
		target.cluster = -1;
	}
	MergeLeafAcoustics( target.acoustics, source.acoustics );
	target.weight = totalWeight;
	FinalizeGeneratedZone( target );
}

// The open contact area between two volumes, accumulated per axis along with the
// bounds of the contact rectangles.
//
// This is the difference between "one room a BSP plane happened to cut in half"
// and "two rooms sharing a wall with a door in it". Both look identical when you
// only compare bounding boxes: in each case the two boxes sit face to face and
// cover each other completely. Only the real leaf-to-leaf contact area tells
// them apart, because the door contributes a fraction of the wall area while the
// BSP cut contributes all of it.
struct ZoneAperture {
	std::array<double, 3> areaByAxis = {};
	Vec3 mins;
	Vec3 maxs;
	bool haveBounds = false;

	double TotalArea() const {
		return areaByAxis[0] + areaByAxis[1] + areaByAxis[2];
	}

	int DominantAxis() const {
		int axis = 0;
		for ( int i = 1; i < 3; ++i ) {
			if ( areaByAxis[i] > areaByAxis[axis] ) {
				axis = i;
			}
		}
		return axis;
	}
};

static void ExpandApertureBounds( ZoneAperture &aperture, const Vec3 &mins, const Vec3 &maxs ) {
	if ( !aperture.haveBounds ) {
		aperture.mins = mins;
		aperture.maxs = maxs;
		aperture.haveBounds = true;
		return;
	}
	aperture.mins.x = ( std::min )( aperture.mins.x, mins.x );
	aperture.mins.y = ( std::min )( aperture.mins.y, mins.y );
	aperture.mins.z = ( std::min )( aperture.mins.z, mins.z );
	aperture.maxs.x = ( std::max )( aperture.maxs.x, maxs.x );
	aperture.maxs.y = ( std::max )( aperture.maxs.y, maxs.y );
	aperture.maxs.z = ( std::max )( aperture.maxs.z, maxs.z );
}

static void MergeApertures( ZoneAperture &target, const ZoneAperture &source ) {
	for ( int i = 0; i < 3; ++i ) {
		target.areaByAxis[i] += source.areaByAxis[i];
	}
	if ( source.haveBounds ) {
		ExpandApertureBounds( target, source.mins, source.maxs );
	}
}

// Measures where two leaf boxes touch. Leaves partition space, so a shared face
// means the volumes are directly connected; a gap means solid geometry sits
// between them.
static bool MeasureLeafAperture( const Zone &a, const Zone &b, float gapEpsilon, ZoneAperture &aperture ) {
	const std::array<float, 3> aMins = { a.mins.x, a.mins.y, a.mins.z };
	const std::array<float, 3> aMaxs = { a.maxs.x, a.maxs.y, a.maxs.z };
	const std::array<float, 3> bMins = { b.mins.x, b.mins.y, b.mins.z };
	const std::array<float, 3> bMaxs = { b.maxs.x, b.maxs.y, b.maxs.z };
	std::array<float, 3> overlap = {};
	for ( int axis = 0; axis < 3; ++axis ) {
		overlap[axis] = AxisOverlap( aMins[axis], aMaxs[axis], bMins[axis], bMaxs[axis] );
		if ( overlap[axis] < -gapEpsilon ) {
			return false;
		}
	}

	int bestAxis = -1;
	float bestArea = 0.0f;
	for ( int axis = 0; axis < 3; ++axis ) {
		const int axisA = ( axis + 1 ) % 3;
		const int axisB = ( axis + 2 ) % 3;
		if ( overlap[axisA] <= 0.0f || overlap[axisB] <= 0.0f ) {
			continue;
		}
		const float area = overlap[axisA] * overlap[axisB];
		// Prefer the axis the two boxes actually meet on. When they interpenetrate
		// on every axis, the thinnest overlap is the contact direction.
		const bool contact = overlap[axis] <= gapEpsilon;
		const bool bestIsContact = bestAxis >= 0 && overlap[bestAxis] <= gapEpsilon;
		if ( bestAxis < 0 || ( contact && !bestIsContact ) ||
			( contact == bestIsContact && area > bestArea ) ) {
			bestAxis = axis;
			bestArea = area;
		}
	}
	if ( bestAxis < 0 ) {
		return false;
	}

	const int axisA = ( bestAxis + 1 ) % 3;
	const int axisB = ( bestAxis + 2 ) % 3;
	std::array<float, 3> mins = {};
	std::array<float, 3> maxs = {};
	mins[axisA] = ( std::max )( aMins[axisA], bMins[axisA] );
	maxs[axisA] = ( std::min )( aMaxs[axisA], bMaxs[axisA] );
	mins[axisB] = ( std::max )( aMins[axisB], bMins[axisB] );
	maxs[axisB] = ( std::min )( aMaxs[axisB], bMaxs[axisB] );
	const float plane = overlap[bestAxis] < 0.0f
		? ( ( aMaxs[bestAxis] <= bMins[bestAxis] )
			? ( aMaxs[bestAxis] + bMins[bestAxis] ) * 0.5f
			: ( bMaxs[bestAxis] + aMins[bestAxis] ) * 0.5f )
		: ( ( std::max )( aMins[bestAxis], bMins[bestAxis] ) + ( std::min )( aMaxs[bestAxis], bMaxs[bestAxis] ) ) * 0.5f;
	mins[bestAxis] = plane;
	maxs[bestAxis] = plane;

	aperture = ZoneAperture();
	aperture.areaByAxis[bestAxis] = static_cast<double>( bestArea );
	aperture.mins = { mins[0], mins[1], mins[2] };
	aperture.maxs = { maxs[0], maxs[1], maxs[2] };
	aperture.haveBounds = true;
	return true;
}

// Region adjacency graph over the generated zones. adjacency[i][j] holds the
// accumulated open contact between zone i and zone j.
using ZoneAdjacencyGraph = std::vector<std::map<std::size_t, ZoneAperture>>;

// How open the connection feels from inside the smaller volume: a doorway fills
// the whole wall of the corridor behind it even though it is a slot in the wall
// of the hall in front of it. This is the right question for a portal hint.
static float ApertureOpenness( const std::vector<GeneratedZone> &zones, std::size_t a, std::size_t b, const ZoneAperture &aperture ) {
	const int axis = aperture.DominantAxis();
	const float faceAreaA = ZoneFaceArea( zones[a].zone, axis );
	const float faceAreaB = ZoneFaceArea( zones[b].zone, axis );
	const float reference = ( std::max )( 1.0f, ( std::min )( faceAreaA, faceAreaB ) );
	return ( std::min )( 1.0f, static_cast<float>( aperture.TotalArea() ) / reference );
}

// Merging asks a different question: are these two halves of one volume?
//
// A BSP plane cutting a room in two leaves both halves with the same
// cross-section, so the opening covers *both* faces completely. An alcove, a
// doorway or a corridor mouth covers its own face completely and only a slice of
// the face it opens onto. Scoring against the geometric mean of the two faces
// separates the two cases; scoring against the smaller face alone reads them as
// identical, and the merge then walks from room to room until the map is a
// single box.
static float ApertureMergeCoverage( const std::vector<GeneratedZone> &zones, std::size_t a, std::size_t b, const ZoneAperture &aperture ) {
	const int axis = aperture.DominantAxis();
	const float faceAreaA = ( std::max )( 1.0f, ZoneFaceArea( zones[a].zone, axis ) );
	const float faceAreaB = ( std::max )( 1.0f, ZoneFaceArea( zones[b].zone, axis ) );
	const float reference = std::sqrt( faceAreaA * faceAreaB );
	return ( std::min )( 1.0f, static_cast<float>( aperture.TotalArea() ) / reference );
}

static ZoneAdjacencyGraph BuildLeafAdjacencyGraph( const std::vector<GeneratedZone> &zones, float gapEpsilon ) {
	ZoneAdjacencyGraph adjacency( zones.size() );
	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		for ( std::size_t j = i + 1u; j < zones.size(); ++j ) {
			ZoneAperture aperture;
			if ( !MeasureLeafAperture( zones[i].zone, zones[j].zone, gapEpsilon, aperture ) ) {
				continue;
			}
			adjacency[i][j] = aperture;
			adjacency[j][i] = aperture;
		}
	}
	return adjacency;
}

struct SplitMergeCandidate {
	std::size_t a = 0;
	std::size_t b = 0;
	float score = 0.0f;
};

// Water and air are different media, so they never share a zone. Everything else
// is free to merge: the preset, material and outdoor flag are all recomputed from
// the combined evidence, which is what lets a courtyard chopped into sky-lit and
// shaded leaves come back together as one outdoor volume.
static bool GeneratedZonesCanMerge( const GeneratedZone &a, const GeneratedZone &b ) {
	const std::uint8_t aUnderwater = static_cast<std::uint8_t>( a.zone.flags & azfmt::ZoneFlagUnderwater );
	const std::uint8_t bUnderwater = static_cast<std::uint8_t>( b.zone.flags & azfmt::ZoneFlagUnderwater );
	return aUnderwater == bUnderwater;
}

static bool BuildSplitMergeCandidate( const std::vector<GeneratedZone> &zones, std::size_t a, std::size_t b, const ZoneAperture &aperture, const MergeTuning &tuning, SplitMergeCandidate &candidate ) {
	const GeneratedZone &zoneA = zones[a];
	const GeneratedZone &zoneB = zones[b];
	if ( !GeneratedZonesCanMerge( zoneA, zoneB ) ) {
		return false;
	}

	const float coverage = ApertureMergeCoverage( zones, a, b, aperture );
	if ( coverage < tuning.minimumFaceCoverage ) {
		return false;
	}

	const Vec3 combinedMins = {
		( std::min )( zoneA.zone.mins.x, zoneB.zone.mins.x ),
		( std::min )( zoneA.zone.mins.y, zoneB.zone.mins.y ),
		( std::min )( zoneA.zone.mins.z, zoneB.zone.mins.z )
	};
	const Vec3 combinedMaxs = {
		( std::max )( zoneA.zone.maxs.x, zoneB.zone.maxs.x ),
		( std::max )( zoneA.zone.maxs.y, zoneB.zone.maxs.y ),
		( std::max )( zoneA.zone.maxs.z, zoneB.zone.maxs.z )
	};
	const float combinedVolume = BoundsVolume( combinedMins, combinedMaxs );
	const float unionVolume = ZoneVolume( zoneA.zone ) + ZoneVolume( zoneB.zone ) - ZoneOverlapVolume( zoneA.zone, zoneB.zone );
	if ( unionVolume <= 0.0f || combinedVolume > unionVolume * tuning.maximumVolumeInflation ) {
		return false;
	}

	// BSP leaves partition open space, so the accumulated leaf volume is how
	// much of the merged box is actually room rather than solid or a different
	// room entirely. Without this a long chain of individually cheap merges
	// ratchets a zone up to the size of the map.
	const float containedVolume = ( std::max )( 1.0f, zoneA.weight ) + ( std::max )( 1.0f, zoneB.weight );
	if ( combinedVolume > containedVolume * tuning.maximumFillInflation ) {
		return false;
	}

	candidate.a = a;
	candidate.b = b;
	candidate.score = coverage * 3.0f - ( combinedVolume / unionVolume - 1.0f );
	return true;
}

static void RelinkMergedAdjacency( ZoneAdjacencyGraph &adjacency, std::size_t target, std::size_t source ) {
	adjacency[target].erase( source );
	for ( const auto &entry : adjacency[source] ) {
		const std::size_t other = entry.first;
		adjacency[other].erase( source );
		if ( other == target ) {
			continue;
		}
		MergeApertures( adjacency[target][other], entry.second );
		adjacency[other][target] = adjacency[target][other];
	}
	adjacency[source].clear();
}

// Every round merges all mutually disjoint best-scoring pairs at once, so the
// zone count falls geometrically instead of one pair per sweep.
static void MergeAdjacentGeneratedZones( std::vector<GeneratedZone> &zones, ZoneAdjacencyGraph &adjacency, std::vector<bool> &alive, std::size_t &aliveCount, const MergeTuning &tuning, std::size_t limit ) {
	while ( aliveCount > ( std::max )( std::size_t{ 1 }, limit ) ) {
		std::vector<SplitMergeCandidate> candidates;
		for ( std::size_t i = 0; i < zones.size(); ++i ) {
			if ( !alive[i] ) {
				continue;
			}
			for ( const auto &entry : adjacency[i] ) {
				if ( entry.first <= i || !alive[entry.first] ) {
					continue;
				}
				SplitMergeCandidate candidate;
				if ( BuildSplitMergeCandidate( zones, i, entry.first, entry.second, tuning, candidate ) ) {
					candidates.push_back( candidate );
				}
			}
		}
		if ( candidates.empty() ) {
			return;
		}

		std::sort( candidates.begin(), candidates.end(), []( const SplitMergeCandidate &a, const SplitMergeCandidate &b ) {
			if ( a.score != b.score ) {
				return a.score > b.score;
			}
			if ( a.a != b.a ) {
				return a.a < b.a;
			}
			return a.b < b.b;
		} );

		std::vector<bool> used( zones.size(), false );
		bool mergedAny = false;
		for ( const SplitMergeCandidate &candidate : candidates ) {
			if ( used[candidate.a] || used[candidate.b] ) {
				continue;
			}
			MergeGeneratedZoneInto( zones[candidate.a], zones[candidate.b] );
			RelinkMergedAdjacency( adjacency, candidate.a, candidate.b );
			used[candidate.a] = true;
			used[candidate.b] = true;
			alive[candidate.b] = false;
			mergedAny = true;
			if ( --aliveCount <= limit ) {
				break;
			}
		}
		if ( !mergedAny ) {
			return;
		}
	}
}

static void CompactGeneratedZones( std::vector<GeneratedZone> &zones, ZoneAdjacencyGraph &adjacency, const std::vector<bool> &alive ) {
	std::vector<std::size_t> remap( zones.size(), std::numeric_limits<std::size_t>::max() );
	std::vector<GeneratedZone> compactZones;
	compactZones.reserve( zones.size() );
	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		if ( alive[i] ) {
			remap[i] = compactZones.size();
			compactZones.push_back( zones[i] );
		}
	}

	ZoneAdjacencyGraph compactAdjacency( compactZones.size() );
	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		if ( remap[i] == std::numeric_limits<std::size_t>::max() ) {
			continue;
		}
		for ( const auto &entry : adjacency[i] ) {
			if ( remap[entry.first] == std::numeric_limits<std::size_t>::max() ) {
				continue;
			}
			compactAdjacency[remap[i]][remap[entry.first]] = entry.second;
		}
	}

	zones.swap( compactZones );
	adjacency.swap( compactAdjacency );
}

static GeneratedZone CoarsenGroup( const std::vector<GeneratedZone> &zones, const std::vector<std::size_t> &indices ) {
	GeneratedZone merged = zones[indices.front()];
	for ( std::size_t i = 1; i < indices.size(); ++i ) {
		MergeGeneratedZoneInto( merged, zones[indices[i]] );
	}
	merged.cluster = -1;
	return merged;
}

template<typename KeyFn>
static std::vector<GeneratedZone> CoarsenGeneratedZonesByKey( const std::vector<GeneratedZone> &zones, KeyFn keyFn ) {
	std::map<std::string, std::vector<std::size_t>> groups;
	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		groups[keyFn( zones[i] )].push_back( i );
	}

	std::vector<GeneratedZone> coarsened;
	coarsened.reserve( groups.size() );
	for ( const auto &entry : groups ) {
		coarsened.push_back( CoarsenGroup( zones, entry.second ) );
	}
	return coarsened;
}

// Generated priorities are assigned by ascending volume so the tightest zone
// containing the listener always wins, and no two generated zones ever tie.
static void AssignGeneratedZoneNames( std::vector<GeneratedZone> &zones, ZoneAdjacencyGraph &adjacency ) {
	std::vector<std::size_t> order( zones.size() );
	for ( std::size_t i = 0; i < order.size(); ++i ) {
		order[i] = i;
	}
	std::sort( order.begin(), order.end(), [&zones]( std::size_t a, std::size_t b ) {
		const float volumeA = ZoneVolume( zones[a].zone );
		const float volumeB = ZoneVolume( zones[b].zone );
		if ( volumeA != volumeB ) {
			return volumeA < volumeB;
		}
		if ( zones[a].zone.mins.x != zones[b].zone.mins.x ) {
			return zones[a].zone.mins.x < zones[b].zone.mins.x;
		}
		if ( zones[a].zone.mins.y != zones[b].zone.mins.y ) {
			return zones[a].zone.mins.y < zones[b].zone.mins.y;
		}
		if ( zones[a].zone.mins.z != zones[b].zone.mins.z ) {
			return zones[a].zone.mins.z < zones[b].zone.mins.z;
		}
		return a < b;
	} );

	std::vector<std::size_t> remap( zones.size(), 0 );
	std::vector<GeneratedZone> sorted;
	sorted.reserve( zones.size() );
	for ( std::size_t i = 0; i < order.size(); ++i ) {
		remap[order[i]] = i;
		sorted.push_back( zones[order[i]] );
	}

	ZoneAdjacencyGraph sortedAdjacency( zones.size() );
	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		for ( const auto &entry : adjacency[i] ) {
			sortedAdjacency[remap[i]][remap[entry.first]] = entry.second;
		}
	}
	zones.swap( sorted );
	adjacency.swap( sortedAdjacency );

	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		const char *presetName = zones[i].zone.preset < static_cast<std::uint32_t>( azfmt::Preset::Count )
			? azfmt::kPresetNames[zones[i].zone.preset]
			: "zone";
		std::ostringstream name;
		name << "auto-a" << zones[i].area << "-" << presetName << "-" << i;
		zones[i].zone.name = name.str().substr( 0u, azfmt::kMaxNameBytes );

		const std::int32_t base = ( zones[i].zone.flags & azfmt::ZoneFlagUnderwater ) != 0
			? kGeneratedLiquidPriorityBase
			: kGeneratedPriorityBase;
		zones[i].zone.priority = base - static_cast<std::int32_t>( i );
	}
}

// Zones start out as BSP leaves, which are fragments of rooms rather than rooms.
// The quality schedule always runs so that presets are classified from
// room-scale bounds; the coarsen schedule only runs while the zone count is
// still over the cap, and each pass relaxes the adjacency bar a little rather
// than collapsing the map into a handful of map-spanning boxes.
static void CoarsenGeneratedZonesToLimit( std::vector<GeneratedZone> &zones, ZoneAdjacencyGraph &adjacency, std::size_t limit ) {
	std::vector<bool> alive( zones.size(), true );
	std::size_t aliveCount = zones.size();
	for ( const MergeTuning &tuning : kQualityMergeSchedule ) {
		MergeAdjacentGeneratedZones( zones, adjacency, alive, aliveCount, tuning, 1u );
	}
	for ( const MergeTuning &tuning : kCoarsenMergeSchedule ) {
		if ( aliveCount <= limit ) {
			break;
		}
		MergeAdjacentGeneratedZones( zones, adjacency, alive, aliveCount, tuning, limit );
	}
	CompactGeneratedZones( zones, adjacency, alive );
	if ( zones.size() <= limit ) {
		return;
	}

	// Last resort for pathological maps that stay fragmented even after the
	// relaxed passes: group by classification, then keep the largest volumes.
	zones = CoarsenGeneratedZonesByKey( zones, []( const GeneratedZone &zone ) {
		return std::to_string( zone.area ) + ":" + std::to_string( zone.zone.preset ) + ":" + std::to_string( zone.zone.materialClass );
	} );
	if ( zones.size() > limit ) {
		std::sort( zones.begin(), zones.end(), []( const GeneratedZone &a, const GeneratedZone &b ) {
			return ZoneVolume( a.zone ) > ZoneVolume( b.zone );
		} );
		zones.resize( limit );
	}
	adjacency = BuildLeafAdjacencyGraph( zones, kLeafAdjacencyGapEpsilon );
}

static bool BuildGeneratedZonesFromBsp( const BspMap &map, std::size_t limit, const std::vector<MaterialOverrideRule> &materialOverrides, std::vector<GeneratedZone> &zones, ZoneAdjacencyGraph &adjacency, std::string &error ) {
	zones.clear();
	for ( const BspLeaf &leaf : map.leafs ) {
		Vec3 mins;
		Vec3 maxs;
		if ( !ClipLeafBounds( map, leaf, mins, maxs ) ) {
			continue;
		}
		GeneratedZone leafZone;
		leafZone.area = leaf.area;
		leafZone.cluster = leaf.cluster;
		leafZone.zone.haveMins = true;
		leafZone.zone.haveMaxs = true;
		leafZone.zone.mins = mins;
		leafZone.zone.maxs = maxs;
		leafZone.weight = ZoneVolume( leafZone.zone );
		const Vec3 center = {
			( mins.x + maxs.x ) * 0.5f,
			( mins.y + maxs.y ) * 0.5f,
			( mins.z + maxs.z ) * 0.5f
		};
		leafZone.acoustics = AnalyzeLeafAcoustics( map, leaf, center, materialOverrides );
		FinalizeGeneratedZone( leafZone );
		zones.push_back( leafZone );
	}

	if ( zones.empty() ) {
		error = "BSP did not contain any non-opaque leaf clusters suitable for audio zones";
		return false;
	}

	adjacency = BuildLeafAdjacencyGraph( zones, kLeafAdjacencyGapEpsilon );
	CoarsenGeneratedZonesToLimit( zones, adjacency, limit );
	AssignGeneratedZoneNames( zones, adjacency );
	return true;
}

struct GeneratedPortalCandidate {
	std::size_t a = 0;
	std::size_t b = 0;
	Portal aToB;
	Portal bToA;
	float score = 0.0f;
};

// Portals come straight from the accumulated open contact between two zones, so
// a hint only exists where the volumes are genuinely connected, its quad covers
// the real opening, and its openness is the fraction of the smaller zone's face
// that is actually open rather than the fraction of two bounding boxes that
// happen to line up.
static void BuildGeneratedPortals( std::vector<Zone> &zones, const std::vector<GeneratedZone> &generated, const ZoneAdjacencyGraph &adjacency ) {
	const std::size_t generatedCount = generated.size();
	for ( std::size_t i = 0; i < generatedCount; ++i ) {
		zones[i].portals.clear();
	}

	std::vector<GeneratedPortalCandidate> candidates;
	for ( std::size_t i = 0; i < generatedCount; ++i ) {
		for ( const auto &entry : adjacency[i] ) {
			const std::size_t j = entry.first;
			if ( j <= i || j >= generatedCount || !entry.second.haveBounds ) {
				continue;
			}
			const float openness = ApertureOpenness( generated, i, j, entry.second );
			if ( openness < kPortalMinimumOpenness ) {
				continue;
			}

			Portal portal;
			portal.mins = entry.second.mins;
			portal.maxs = entry.second.maxs;
			portal.openness = openness;

			// A wide opening bleeds the neighbouring environment further into the
			// room and starts doing so sooner; a vent-sized gap only matters up
			// close.
			const float openingSize = std::sqrt( ( std::max )( 1.0f, static_cast<float>( entry.second.TotalArea() ) ) );
			portal.blendDistance = ( std::max )( kMinimumGeneratedPortalBlendDistance,
				( std::min )( kMaximumGeneratedPortalBlendDistance, openingSize * kPortalBlendDistanceScale ) );
			portal.maximumBlend = kMinimumGeneratedPortalMaxBlend +
				( kMaximumGeneratedPortalMaxBlend - kMinimumGeneratedPortalMaxBlend ) * openness;
			portal.minimumBlend = azfmt::kDefaultPortalMinimumBlend;
			if ( openness >= kWidePortalOpenness ) {
				portal.blendCurve = static_cast<std::uint8_t>( azfmt::PortalBlendCurve::EaseOut );
			} else if ( openness <= kNarrowPortalOpenness ) {
				portal.blendCurve = static_cast<std::uint8_t>( azfmt::PortalBlendCurve::EaseIn );
			} else {
				portal.blendCurve = static_cast<std::uint8_t>( azfmt::PortalBlendCurve::Smooth );
			}

			GeneratedPortalCandidate candidate;
			candidate.a = i;
			candidate.b = j;
			candidate.aToB = portal;
			candidate.aToB.targetZone = static_cast<std::uint32_t>( j );
			candidate.bToA = portal;
			candidate.bToA.targetZone = static_cast<std::uint32_t>( i );
			candidate.score = openness * static_cast<float>( entry.second.TotalArea() );
			candidates.push_back( candidate );
		}
	}

	std::sort( candidates.begin(), candidates.end(), []( const GeneratedPortalCandidate &a, const GeneratedPortalCandidate &b ) {
		if ( a.score != b.score ) {
			return a.score > b.score;
		}
		if ( a.a != b.a ) {
			return a.a < b.a;
		}
		return a.b < b.b;
	} );

	for ( const GeneratedPortalCandidate &candidate : candidates ) {
		if ( zones[candidate.a].portals.size() >= azfmt::kMaxZonePortals ||
			zones[candidate.b].portals.size() >= azfmt::kMaxZonePortals ) {
			continue;
		}
		zones[candidate.a].portals.push_back( candidate.aToB );
		zones[candidate.b].portals.push_back( candidate.bToA );
	}
}

static bool LoadTextZones( const std::filesystem::path &path, std::vector<Zone> &zones, std::string &error ) {
	std::string source;
	if ( !ReadWholeFile( path, source, error ) ) {
		return false;
	}
	Parser parser( source );
	return parser.Parse( zones, error );
}

static int FromBspCommand( int argc, char **argv ) {
	std::filesystem::path input;
	std::filesystem::path output;
	std::filesystem::path overridePath;
	std::filesystem::path materialMapPath;
	std::size_t maxZones = azfmt::kMaxZones;

	for ( int i = 2; i < argc; ++i ) {
		const std::string arg = argv[i];
		if ( arg == "-o" || arg == "--output" ) {
			if ( i + 1 >= argc ) {
				std::cerr << "missing output path after " << arg << "\n";
				return 2;
			}
			output = argv[++i];
		} else if ( arg == "--merge" || arg == "--override" || arg == "--overrides" ) {
			if ( i + 1 >= argc ) {
				std::cerr << "missing .audiozones path after " << arg << "\n";
				return 2;
			}
			overridePath = argv[++i];
		} else if ( arg == "--material-map" || arg == "--materials" || arg == "--material-overrides" ) {
			if ( i + 1 >= argc ) {
				std::cerr << "missing material override map after " << arg << "\n";
				return 2;
			}
			materialMapPath = argv[++i];
		} else if ( arg == "--max-zones" ) {
			if ( i + 1 >= argc ) {
				std::cerr << "missing zone count after " << arg << "\n";
				return 2;
			}
			char *end = nullptr;
			const long value = std::strtol( argv[++i], &end, 10 );
			if ( end == argv[i] || *end != '\0' || value < 1 || value > static_cast<long>( azfmt::kMaxZones ) ) {
				std::cerr << "--max-zones must be in range 1.." << azfmt::kMaxZones << "\n";
				return 2;
			}
			maxZones = static_cast<std::size_t>( value );
		} else if ( arg == "-h" || arg == "--help" ) {
			return 2;
		} else if ( input.empty() ) {
			input = arg;
		} else {
			std::cerr << "unexpected argument: " << arg << "\n";
			return 2;
		}
	}

	if ( input.empty() ) {
		return 2;
	}
	if ( output.empty() ) {
		output = DefaultOutputPath( input );
	}

	std::string error;
	std::vector<MaterialOverrideRule> materialOverrides;
	if ( !materialMapPath.empty() ) {
		if ( !LoadMaterialOverrideMap( materialMapPath, materialOverrides, error ) ) {
			std::cerr << error << "\n";
			return 1;
		}
	}

	BspMap map;
	if ( !LoadBspMap( input, map, error ) ) {
		std::cerr << input.string() << ": " << error << "\n";
		return 1;
	}

	std::vector<Zone> overrideZones;
	if ( !overridePath.empty() ) {
		if ( !LoadTextZones( overridePath, overrideZones, error ) ) {
			std::cerr << overridePath.string() << ": " << error << "\n";
			return 1;
		}
		if ( overrideZones.size() >= maxZones ) {
			std::cerr << overridePath.string() << ": override zones leave no room for generated zones under --max-zones\n";
			return 1;
		}
		for ( Zone &zone : overrideZones ) {
			zone.flags &= ~azfmt::ZoneFlagGenerated;
		}
	}

	std::vector<GeneratedZone> generatedZones;
	ZoneAdjacencyGraph generatedAdjacency;
	const std::size_t generatedLimit = maxZones - overrideZones.size();
	if ( !BuildGeneratedZonesFromBsp( map, generatedLimit, materialOverrides, generatedZones, generatedAdjacency, error ) ) {
		std::cerr << input.string() << ": " << error << "\n";
		return 1;
	}

	std::vector<Zone> zones;
	zones.reserve( generatedZones.size() + overrideZones.size() );
	for ( const GeneratedZone &generated : generatedZones ) {
		zones.push_back( generated.zone );
	}
	const std::size_t generatedCount = zones.size();
	for ( Zone &zone : overrideZones ) {
		for ( Portal &portal : zone.portals ) {
			if ( portal.targetZone >= overrideZones.size() ) {
				std::cerr << overridePath.string() << ": override portal target " << portal.targetZone
					<< " is outside the override zone set\n";
				return 1;
			}
			portal.targetZone += static_cast<std::uint32_t>( generatedCount );
		}
	}
	zones.insert( zones.end(), overrideZones.begin(), overrideZones.end() );
	BuildGeneratedPortals( zones, generatedZones, generatedAdjacency );

	const std::vector<std::uint8_t> binary = BuildBinary( zones );
	if ( binary.size() > azfmt::kMaxFileBytes ) {
		std::cerr << "generated audio-zone file would exceed " << azfmt::kMaxFileBytes << " bytes\n";
		return 1;
	}
	if ( !WriteWholeFile( output, binary, error ) ) {
		std::cerr << error << "\n";
		return 1;
	}

	std::cout << "generated " << generatedCount << " BSP audio zone" << ( generatedCount == 1u ? "" : "s" );
	if ( !overrideZones.empty() ) {
		std::cout << " plus " << overrideZones.size() << " override zone" << ( overrideZones.size() == 1u ? "" : "s" );
	}
	if ( !materialOverrides.empty() ) {
		std::cout << " using " << materialOverrides.size() << " material rule" << ( materialOverrides.size() == 1u ? "" : "s" );
	}
	std::cout << " to " << output.string() << "\n";
	return 0;
}

static int CompileCommand( int argc, char **argv ) {
	std::filesystem::path input;
	std::filesystem::path output;

	for ( int i = 1; i < argc; ++i ) {
		const std::string arg = argv[i];
		if ( arg == "-o" || arg == "--output" ) {
			if ( i + 1 >= argc ) {
				std::cerr << "missing output path after " << arg << "\n";
				return 2;
			}
			output = argv[++i];
		} else if ( arg == "-h" || arg == "--help" ) {
			return 2;
		} else if ( input.empty() ) {
			input = arg;
		} else {
			std::cerr << "unexpected argument: " << arg << "\n";
			return 2;
		}
	}

	if ( input.empty() ) {
		return 2;
	}
	if ( output.empty() ) {
		output = DefaultOutputPath( input );
	}

	std::string source;
	std::string error;
	if ( !ReadWholeFile( input, source, error ) ) {
		std::cerr << error << "\n";
		return 1;
	}

	std::vector<Zone> zones;
	Parser parser( source );
	if ( !parser.Parse( zones, error ) ) {
		std::cerr << input.string() << ": " << error << "\n";
		return 1;
	}

	const std::vector<std::uint8_t> binary = BuildBinary( zones );
	if ( binary.size() > azfmt::kMaxFileBytes ) {
		std::cerr << "compiled audio-zone file would exceed " << azfmt::kMaxFileBytes << " bytes\n";
		return 1;
	}
	if ( !WriteWholeFile( output, binary, error ) ) {
		std::cerr << error << "\n";
		return 1;
	}

	std::cout << "wrote " << zones.size() << " audio zone" << ( zones.size() == 1u ? "" : "s" )
		<< " to " << output.string() << "\n";
	return 0;
}

static int DumpCommand( int argc, char **argv ) {
	if ( argc != 3 ) {
		return 2;
	}
	std::vector<Zone> zones;
	std::string error;
	std::uint32_t version = 0;
	if ( !LoadBinary( argv[2], zones, error, &version ) ) {
		std::cerr << argv[2] << ": " << error << "\n";
		return 1;
	}
	std::cout << "audiozones version " << version << ", zones " << zones.size() << "\n";
	for ( const Zone &zone : zones ) {
		const char *presetName = zone.preset < static_cast<std::uint32_t>( azfmt::Preset::Count )
			? azfmt::kPresetNames[zone.preset]
			: "unknown";
		const char *materialName = zone.materialClass < static_cast<std::uint8_t>( azfmt::MaterialClass::Count )
			? azfmt::kMaterialClassNames[zone.materialClass]
			: "unknown";
		std::cout << "zone \"" << zone.name << "\""
			<< " preset " << presetName
			<< " material " << materialName
			<< " bounds " << zone.mins.x << ' ' << zone.mins.y << ' ' << zone.mins.z
			<< " -> " << zone.maxs.x << ' ' << zone.maxs.y << ' ' << zone.maxs.z
			<< " priority " << zone.priority
			<< " transition " << zone.transitionMs
			<< " reverbGain " << zone.reverbGain
			<< " occlusion " << zone.occlusionMultiplier
			<< " directLF/HF " << zone.directLF << '/' << zone.directHF
			<< " wetLF/HF " << zone.wetLF << '/' << zone.wetHF
			<< " flags " << static_cast<int>( zone.flags )
			<< " portals " << zone.portals.size()
			<< "\n";
		for ( const Portal &portal : zone.portals ) {
			const char *targetName = portal.targetZone < zones.size() ? zones[portal.targetZone].name.c_str() : "out-of-range";
			std::cout << "  portal target " << portal.targetZone
				<< " \"" << targetName << "\""
				<< " bounds " << portal.mins.x << ' ' << portal.mins.y << ' ' << portal.mins.z
				<< " -> " << portal.maxs.x << ' ' << portal.maxs.y << ' ' << portal.maxs.z
				<< " openness " << portal.openness
				<< " blendDistance " << portal.blendDistance
				<< " minBlend " << portal.minimumBlend
				<< " maxBlend " << portal.maximumBlend
				<< " curve " << azfmt::kPortalBlendCurveNames[portal.blendCurve]
				<< "\n";
		}
	}
	return 0;
}

static std::uint32_t SidecarVersionFromBytes( const std::vector<std::uint8_t> &data ) {
	if ( data.size() < 8u ) {
		return 0;
	}
	return static_cast<std::uint32_t>( data[4] ) |
		( static_cast<std::uint32_t>( data[5] ) << 8u ) |
		( static_cast<std::uint32_t>( data[6] ) << 16u ) |
		( static_cast<std::uint32_t>( data[7] ) << 24u );
}

static float RuntimeZoneVolume( const azrt::AudioZone &zone ) {
	return ( zone.maxs.v[0] - zone.mins.v[0] ) *
		( zone.maxs.v[1] - zone.mins.v[1] ) *
		( zone.maxs.v[2] - zone.mins.v[2] );
}

static bool RuntimeZoneBoundsOverlap( const azrt::AudioZone &a, const azrt::AudioZone &b ) {
	return a.mins.v[0] < b.maxs.v[0] && a.maxs.v[0] > b.mins.v[0] &&
		a.mins.v[1] < b.maxs.v[1] && a.maxs.v[1] > b.mins.v[1] &&
		a.mins.v[2] < b.maxs.v[2] && a.maxs.v[2] > b.mins.v[2];
}

static bool RuntimeZoneHasPortalTo( const std::vector<azrt::AudioZone> &zones, std::size_t from, std::size_t to ) {
	if ( from >= zones.size() || to >= zones.size() ) {
		return false;
	}
	for ( const azrt::AudioZonePortal &portal : zones[from].portals ) {
		if ( portal.targetZone == to ) {
			return true;
		}
	}
	return false;
}

static void PrintCountList( const char *label, const char *const *names, const int *counts, int count ) {
	std::cout << label << ':';
	bool any = false;
	for ( int i = 0; i < count; ++i ) {
		if ( counts[i] <= 0 ) {
			continue;
		}
		std::cout << ' ' << names[i] << '=' << counts[i];
		any = true;
	}
	if ( !any ) {
		std::cout << " none";
	}
	std::cout << "\n";
}

// A zone bigger than this share of the sidecar bounds is not a place, it is the
// map. A zone at or under specificFraction is treated as a specific volume.
constexpr double kAuditSpanningZoneFraction = 0.50;
constexpr double kAuditSpecificZoneFraction = 0.10;
constexpr std::size_t kAuditMinimumZonesForScaleChecks = 8u;

static double ClampAuditScore( double value ) {
	if ( value < 0.0 ) {
		return 0.0;
	}
	if ( value > 1.0 ) {
		return 1.0;
	}
	return value;
}

static const char *AuditConfidenceGrade( double score ) {
	if ( score >= 0.90 ) {
		return "excellent";
	}
	if ( score >= 0.75 ) {
		return "good";
	}
	if ( score >= 0.60 ) {
		return "fair";
	}
	return "poor";
}

struct AuditOptions {
	std::filesystem::path input;
	int samples = 4096;
	bool strict = false;
};

static bool ParseAuditOptions( int argc, char **argv, AuditOptions &options, std::string &error ) {
	if ( argc < 3 ) {
		return false;
	}

	for ( int i = 2; i < argc; ++i ) {
		const std::string arg = argv[i];
		if ( arg == "--strict" ) {
			options.strict = true;
			continue;
		}
		if ( arg == "--samples" ) {
			if ( i + 1 >= argc ) {
				error = "missing sample count after --samples";
				return false;
			}
			char *end = nullptr;
			errno = 0;
			const long value = std::strtol( argv[++i], &end, 10 );
			if ( argv[i][0] == '\0' || end == argv[i] || *end != '\0' || errno == ERANGE || value < 1 || value > 1000000 ) {
				error = "--samples expects an integer from 1 to 1000000";
				return false;
			}
			options.samples = static_cast<int>( value );
			continue;
		}
		if ( !options.input.empty() ) {
			error = "multiple input files were provided";
			return false;
		}
		options.input = arg;
	}

	if ( options.input.empty() ) {
		error = "missing input.azb";
		return false;
	}
	return true;
}

static int AuditCommand( int argc, char **argv ) {
	AuditOptions options;
	std::string error;
	if ( !ParseAuditOptions( argc, argv, options, error ) ) {
		if ( !error.empty() ) {
			std::cerr << error << "\n";
		}
		return 2;
	}

	std::vector<std::uint8_t> data;
	if ( !ReadWholeBinaryFile( options.input, data, error ) ) {
		std::cerr << options.input.string() << ": " << error << "\n";
		return 1;
	}

	std::vector<azrt::AudioZone> zones;
	if ( !azrt::ParseAudioZoneBinary( data.data(), data.size(), zones, error ) ) {
		std::cerr << options.input.string() << ": " << error << "\n";
		return 1;
	}

	const std::uint32_t version = SidecarVersionFromBytes( data );
	std::array<int, static_cast<std::size_t>( azfmt::Preset::Count )> presetCounts = {};
	std::array<int, static_cast<std::size_t>( azfmt::MaterialClass::Count )> materialCounts = {};
	int generatedCount = 0;
	int outdoorFlagCount = 0;
	int underwaterFlagCount = 0;
	int totalPortals = 0;
	int maxPortals = 0;
	int selfPortals = 0;
	int oneWayPortals = 0;
	float minOpenness = 1.0f;
	float maxOpenness = 0.0f;
	float sumOpenness = 0.0f;
	float minBlendDistance = azfmt::kMaximumPortalBlendDistance;
	float maxBlendDistance = 0.0f;
	float sumBlendDistance = 0.0f;
	float minMaxBlend = 1.0f;
	float maxMaxBlend = 0.0f;
	float sumMaxBlend = 0.0f;
	std::array<int, static_cast<std::size_t>( azfmt::PortalBlendCurve::Count )> curveCounts = {};
	float totalVolume = 0.0f;
	azrt::Vec3f mins = zones.front().mins;
	azrt::Vec3f maxs = zones.front().maxs;
	std::vector<double> zoneVolumes;
	zoneVolumes.reserve( zones.size() );

	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		const azrt::AudioZone &zone = zones[i];
		if ( zone.presetIndex >= 0 && zone.presetIndex < static_cast<int>( presetCounts.size() ) ) {
			++presetCounts[static_cast<std::size_t>( zone.presetIndex )];
		}
		if ( zone.materialClass < materialCounts.size() ) {
			++materialCounts[zone.materialClass];
		}
		if ( ( zone.flags & azfmt::ZoneFlagGenerated ) != 0 ) {
			++generatedCount;
		}
		if ( ( zone.flags & azfmt::ZoneFlagOutdoor ) != 0 ) {
			++outdoorFlagCount;
		}
		if ( ( zone.flags & azfmt::ZoneFlagUnderwater ) != 0 ) {
			++underwaterFlagCount;
		}
		totalVolume += RuntimeZoneVolume( zone );
		zoneVolumes.push_back( static_cast<double>( RuntimeZoneVolume( zone ) ) );
		for ( int axis = 0; axis < 3; ++axis ) {
			mins.v[axis] = ( std::min )( mins.v[axis], zone.mins.v[axis] );
			maxs.v[axis] = ( std::max )( maxs.v[axis], zone.maxs.v[axis] );
		}

		totalPortals += static_cast<int>( zone.portals.size() );
		maxPortals = ( std::max )( maxPortals, static_cast<int>( zone.portals.size() ) );
		for ( const azrt::AudioZonePortal &portal : zone.portals ) {
			if ( portal.targetZone == i ) {
				++selfPortals;
			}
			if ( !RuntimeZoneHasPortalTo( zones, portal.targetZone, i ) ) {
				++oneWayPortals;
			}
			minOpenness = ( std::min )( minOpenness, portal.openness );
			maxOpenness = ( std::max )( maxOpenness, portal.openness );
			sumOpenness += portal.openness;
			minBlendDistance = ( std::min )( minBlendDistance, portal.blendDistance );
			maxBlendDistance = ( std::max )( maxBlendDistance, portal.blendDistance );
			sumBlendDistance += portal.blendDistance;
			minMaxBlend = ( std::min )( minMaxBlend, portal.maximumBlend );
			maxMaxBlend = ( std::max )( maxMaxBlend, portal.maximumBlend );
			sumMaxBlend += portal.maximumBlend;
			if ( portal.blendCurve < curveCounts.size() ) {
				++curveCounts[portal.blendCurve];
			}
		}
	}

	int overlapPairs = 0;
	int equalPriorityOverlapPairs = 0;
	for ( std::size_t i = 0; i < zones.size(); ++i ) {
		for ( std::size_t j = i + 1u; j < zones.size(); ++j ) {
			if ( !RuntimeZoneBoundsOverlap( zones[i], zones[j] ) ) {
				continue;
			}
			++overlapPairs;
			if ( zones[i].priority == zones[j].priority ) {
				++equalPriorityOverlapPairs;
			}
		}
	}

	// Zone scale relative to the sidecar bounds. A generated sidecar whose zones
	// each cover a large share of the map has lost its spatial resolution, which
	// is exactly what a collapsed coarsening pass produces.
	const double boundsVolume = ( std::max )( 1.0,
		static_cast<double>( maxs.v[0] - mins.v[0] ) *
		static_cast<double>( maxs.v[1] - mins.v[1] ) *
		static_cast<double>( maxs.v[2] - mins.v[2] ) );
	std::vector<double> sortedVolumes = zoneVolumes;
	std::sort( sortedVolumes.begin(), sortedVolumes.end() );
	const double medianZoneFraction = sortedVolumes.empty() ? 0.0 : sortedVolumes[sortedVolumes.size() / 2u] / boundsVolume;
	const double maxZoneFraction = sortedVolumes.empty() ? 0.0 : sortedVolumes.back() / boundsVolume;
	// With N zones a zone would nominally cover 1/N of the map; allow generous
	// slack so small hand-authored sidecars are not judged by a scale rule aimed
	// at generated ones.
	const double specificFraction = ( std::min )( 1.0,
		( std::max )( kAuditSpecificZoneFraction, 4.0 / static_cast<double>( zones.size() ) ) );
	int spanningZones = 0;
	if ( zones.size() >= kAuditMinimumZonesForScaleChecks ) {
		for ( double volume : zoneVolumes ) {
			if ( volume / boundsVolume > kAuditSpanningZoneFraction ) {
				++spanningZones;
			}
		}
	}

	const int axisSamples = ( std::max )( 1, static_cast<int>( std::ceil( std::pow( static_cast<double>( options.samples ), 1.0 / 3.0 ) ) ) );
	int sampleCount = 0;
	int zoneHits = 0;
	int specificHits = 0;
	int portalBlendHits = 0;
	double blendSum = 0.0;
	std::uint64_t checksum = 1469598103934665603ull;
	const auto profileStart = std::chrono::steady_clock::now();
	for ( int z = 0; z < axisSamples && sampleCount < options.samples; ++z ) {
		for ( int y = 0; y < axisSamples && sampleCount < options.samples; ++y ) {
			for ( int x = 0; x < axisSamples && sampleCount < options.samples; ++x ) {
				float sample[3];
				const int coord[3] = { x, y, z };
				for ( int axis = 0; axis < 3; ++axis ) {
					const float span = maxs.v[axis] - mins.v[axis];
					sample[axis] = mins.v[axis] + span * ( static_cast<float>( coord[axis] ) + 0.5f ) / static_cast<float>( axisSamples );
				}
				const azrt::AudioZone *zone = azrt::FindAudioZone( zones, sample );
				if ( zone != nullptr ) {
					++zoneHits;
					if ( static_cast<double>( RuntimeZoneVolume( *zone ) ) / boundsVolume <= specificFraction ) {
						++specificHits;
					}
					const std::size_t zoneIndex = static_cast<std::size_t>( zone - zones.data() );
					const azrt::AudioZonePortalBlend blend = azrt::FindAudioZonePortalBlend( zones, *zone, sample );
					if ( blend.target != nullptr ) {
						++portalBlendHits;
						blendSum += blend.blend;
					}
					checksum ^= static_cast<std::uint64_t>( zoneIndex + 1u ) + ( static_cast<std::uint64_t>( sampleCount + 1 ) << 32u );
					checksum *= 1099511628211ull;
				}
				++sampleCount;
			}
		}
	}
	const auto profileEnd = std::chrono::steady_clock::now();
	const double elapsedMs = std::chrono::duration<double, std::milli>( profileEnd - profileStart ).count();
	const double zoneCountDouble = static_cast<double>( zones.size() );
	const double knownMaterialZones =
		zoneCountDouble -
		static_cast<double>( materialCounts[static_cast<std::size_t>( azfmt::MaterialClass::Unknown )] ) -
		static_cast<double>( materialCounts[static_cast<std::size_t>( azfmt::MaterialClass::Neutral )] );
	const double neutralMaterialZones = static_cast<double>( materialCounts[static_cast<std::size_t>( azfmt::MaterialClass::Neutral )] );
	const double materialConfidence = ClampAuditScore( ( knownMaterialZones + neutralMaterialZones * 0.5 ) / zoneCountDouble );
	const double portalConfidence = totalPortals == 0
		? ( generatedCount > 1 ? 0.0 : 1.0 )
		: ClampAuditScore( 1.0 - static_cast<double>( oneWayPortals + selfPortals ) / static_cast<double>( totalPortals ) );
	const double lookupCoverage = sampleCount > 0
		? ClampAuditScore( static_cast<double>( zoneHits ) / static_cast<double>( sampleCount ) )
		: 0.0;
	// Coverage alone rewards the wrong thing: a single map-sized box scores 100%.
	// What matters is that a resolved zone is a specific place rather than the
	// whole level, so score the share of hits that land in a zone of sane scale.
	const double lookupConfidence = zoneHits > 0
		? ClampAuditScore( static_cast<double>( specificHits ) / static_cast<double>( zoneHits ) )
		: 0.0;
	const double overlapConfidence = overlapPairs > 0
		? ClampAuditScore( 1.0 - static_cast<double>( equalPriorityOverlapPairs ) / static_cast<double>( overlapPairs ) )
		: 1.0;
	const double overallConfidence = ClampAuditScore(
		materialConfidence * 0.30 +
		portalConfidence * 0.25 +
		lookupConfidence * 0.25 +
		overlapConfidence * 0.20 );
	double anomalyPenalty = 0.0;
	if ( version == azfmt::kLegacyVersion ) {
		anomalyPenalty += 0.15;
	}
	if ( zones.size() > azfmt::kMaxZones * 9u / 10u ) {
		anomalyPenalty += 0.10;
	}
	if ( generatedCount > 1 && totalPortals == 0 ) {
		anomalyPenalty += 0.15;
	}
	if ( selfPortals > 0 ) {
		anomalyPenalty += 0.10;
	}
	if ( oneWayPortals > 0 ) {
		anomalyPenalty += 0.10;
	}
	if ( equalPriorityOverlapPairs > 0 ) {
		anomalyPenalty += 0.08;
	}
	if ( zoneHits == 0 ) {
		anomalyPenalty += 0.25;
	}
	if ( spanningZones > 0 ) {
		anomalyPenalty += 0.20;
	}
	const double anomalyScore = ClampAuditScore( ( 1.0 - overallConfidence ) * 0.75 + anomalyPenalty );

	std::vector<std::string> warnings;
	if ( version == azfmt::kLegacyVersion ) {
		warnings.push_back( "legacy v1 sidecar has no material or portal metadata" );
	}
	if ( zones.size() > azfmt::kMaxZones * 9u / 10u ) {
		warnings.push_back( "zone count is close to the format limit" );
	}
	if ( generatedCount > 1 && totalPortals == 0 ) {
		warnings.push_back( "generated sidecar has multiple zones but no portal hints" );
	}
	if ( selfPortals > 0 ) {
		warnings.push_back( "self-targeting portals are ignored at runtime" );
	}
	if ( oneWayPortals > 0 ) {
		warnings.push_back( "one-way portal hints were found; generated portals are normally reciprocal" );
	}
	if ( equalPriorityOverlapPairs > 0 ) {
		warnings.push_back( "equal-priority overlapping zones rely on smaller-volume tie-breaks" );
	}
	if ( zoneHits == 0 ) {
		warnings.push_back( "deterministic lookup samples did not hit any zone" );
	}
	if ( spanningZones > 0 ) {
		warnings.push_back( "zones cover more than half of the sidecar bounds; generated volumes may have collapsed" );
	}
	if ( materialConfidence < 0.35 ) {
		warnings.push_back( "low material confidence; add material overrides or authored material classes" );
	}
	if ( portalConfidence < 0.50 ) {
		warnings.push_back( "low portal confidence; add reciprocal portals or inspect generated transitions" );
	}
	if ( lookupConfidence < 0.60 ) {
		warnings.push_back( "most lookups resolve to map-scale zones instead of specific volumes" );
	}
	if ( lookupCoverage < 0.10 ) {
		warnings.push_back( "lookup coverage is low across the sampled sidecar bounds" );
	}
	if ( anomalyScore >= 0.50 ) {
		warnings.push_back( "audio-zone anomaly score is high; inspect this map before release" );
	}

	std::cout << std::fixed << std::setprecision( 3 );
	std::cout << "audio-zone audit " << options.input.string() << "\n";
	std::cout << "version " << version << ", bytes " << data.size() << ", zones " << zones.size() << "\n";
	std::cout << "bounds " << mins.v[0] << ' ' << mins.v[1] << ' ' << mins.v[2]
		<< " -> " << maxs.v[0] << ' ' << maxs.v[1] << ' ' << maxs.v[2]
		<< ", summed volume " << totalVolume << "\n";
	PrintCountList( "presets", azfmt::kPresetNames, presetCounts.data(), static_cast<int>( presetCounts.size() ) );
	PrintCountList( "materials", azfmt::kMaterialClassNames, materialCounts.data(), static_cast<int>( materialCounts.size() ) );
	std::cout << "flags generated=" << generatedCount
		<< " outdoor=" << outdoorFlagCount
		<< " underwater=" << underwaterFlagCount << "\n";
	if ( totalPortals > 0 ) {
		std::cout << "portals total=" << totalPortals
			<< " maxPerZone=" << maxPortals
			<< " openness min/avg/max=" << minOpenness << '/'
			<< ( sumOpenness / static_cast<float>( totalPortals ) ) << '/'
			<< maxOpenness
			<< " self=" << selfPortals
			<< " oneWay=" << oneWayPortals << "\n";
		std::cout << "portal tuning distance min/avg/max=" << minBlendDistance << '/'
			<< ( sumBlendDistance / static_cast<float>( totalPortals ) ) << '/'
			<< maxBlendDistance
			<< " maxBlend min/avg/max=" << minMaxBlend << '/'
			<< ( sumMaxBlend / static_cast<float>( totalPortals ) ) << '/'
			<< maxMaxBlend << "\n";
		PrintCountList( "portal curves", azfmt::kPortalBlendCurveNames, curveCounts.data(), static_cast<int>( curveCounts.size() ) );
	} else {
		std::cout << "portals total=0\n";
	}
	std::cout << "overlaps total=" << overlapPairs
		<< " equalPriority=" << equalPriorityOverlapPairs << "\n";
	std::cout << "scale boundsVolume=" << boundsVolume
		<< " medianZoneFraction=" << medianZoneFraction
		<< " maxZoneFraction=" << maxZoneFraction
		<< " specificFraction=" << specificFraction
		<< " spanning=" << spanningZones << "\n";
	std::cout << "lookup profile samples=" << sampleCount
		<< " hits=" << zoneHits
		<< " specificHits=" << specificHits
		<< " coverage=" << lookupCoverage
		<< " portalBlends=" << portalBlendHits
		<< " avgBlend=" << ( portalBlendHits > 0 ? blendSum / portalBlendHits : 0.0 )
		<< " elapsedMs=" << elapsedMs
		<< " nsPerSample=" << ( elapsedMs * 1000000.0 / static_cast<double>( sampleCount ) )
		<< " checksum=" << checksum << "\n";
	std::cout << "confidence material=" << materialConfidence
		<< " portal=" << portalConfidence
		<< " lookup=" << lookupConfidence
		<< " overlap=" << overlapConfidence
		<< " overall=" << overallConfidence
		<< " anomaly=" << anomalyScore
		<< " grade=" << AuditConfidenceGrade( overallConfidence ) << "\n";

	if ( warnings.empty() ) {
		std::cout << "warnings none\n";
		return 0;
	}
	for ( const std::string &warning : warnings ) {
		std::cout << "warning: " << warning << "\n";
	}
	return options.strict ? 1 : 0;
}

static void PrintUsage( const char *argv0 ) {
	std::cerr
		<< "Usage:\n"
		<< "  " << argv0 << " [-o output.azb] input.audiozones\n"
		<< "  " << argv0 << " --from-bsp [-o output.azb] [--merge overrides.audiozones] [--material-map audio-materials.txt] [--max-zones N] input.bsp\n"
		<< "  " << argv0 << " --dump input.azb\n"
		<< "  " << argv0 << " --audit [--strict] [--samples N] input.azb\n\n"
		<< "BSP generation reads IBSP v46/v47 leaves, clusters, areas, surfaces, brushes, and shaders.\n"
		<< "Material maps use: shader/pattern material [preset name] [flag outdoor] [weight N].\n"
		<< "Generated zones use negative priorities, so merged manual override zones win naturally.\n\n"
		<< "Text format:\n"
		<< "  audiozones 1\n"
		<< "  zone \"atrium\" {\n"
		<< "    bounds -512 -512 -64 512 512 384\n"
		<< "    environment hall\n"
		<< "    reverbGain 1.1\n"
		<< "    occlusionMultiplier 0.85\n"
		<< "    lpfBias 0.95\n"
		<< "    hpfBias 1.0\n"
		<< "    transitionMs 900\n"
		<< "    priority 10\n"
		<< "    portal \"hallway\" { bounds 512 -128 -64 512 128 192 openness 0.8 blendDistance 192 maxBlend 0.45 curve smooth }\n"
		<< "  }\n";
}

} // namespace

int main( int argc, char **argv ) {
	if ( argc >= 2 && std::string( argv[1] ) == "--dump" ) {
		const int result = DumpCommand( argc, argv );
		if ( result == 2 ) {
			PrintUsage( argv[0] );
		}
		return result;
	}
	if ( argc >= 2 && std::string( argv[1] ) == "--audit" ) {
		const int result = AuditCommand( argc, argv );
		if ( result == 2 ) {
			PrintUsage( argv[0] );
		}
		return result;
	}
	if ( argc >= 2 && std::string( argv[1] ) == "--from-bsp" ) {
		const int result = FromBspCommand( argc, argv );
		if ( result == 2 ) {
			PrintUsage( argv[0] );
		}
		return result;
	}

	const int result = CompileCommand( argc, argv );
	if ( result == 2 ) {
		PrintUsage( argv[0] );
	}
	return result;
}
