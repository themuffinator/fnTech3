/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

A map's entity text: the `{ "key" "value" ... }` blocks of its BSP entity
lump, or of a `.ent` file that overrides that lump.

The engine hands this text to the game modules a token at a time through
COM_ParseExt (code/qcommon/q_shared.c), and the Quake III line's game
modules read those tokens as their G_ParseSpawnVars reads them: an opening
brace, then a key and a value until a token that begins with a closing
brace. The tokenizer and parser below follow both exactly, so that what the
entity editor shows and what a `.ent` override is checked against are what
a game module would be handed:

  - every byte up to ' ' is whitespace, compared as a plain `char`, as
    SkipWhitespace compares it (where `char` is signed, a byte of 0x80 or
    above outside quotes is whitespace too);
  - a comment that opens with two slashes runs to the end of its line, one
    that opens with slash-star to the next star-slash, and neither is read
    inside quotes;
  - a quoted token runs to the next quote or the end of the text, keeps
    line breaks, and has no escapes; an unquoted token runs to the next
    whitespace byte;
  - a token keeps at most MAX_TOKEN_CHARS - 1 bytes (1023), the rest is
    read and dropped;
  - the text ends at its first NUL.

Ritual's and 2015's game modules (Alice, F.A.K.K.2, Medal of Honor) are
handed the whole text and parse it themselves; text written by Serialize
reads the same under their parsers, as every key and value is quoted and
every brace stands alone on its line, as their map compilers wrote them.

This header depends on the standard library alone, so that tests can
compile it without the engine.
===========================================================================
*/
#ifndef FNTECH3_QCOMMON_ENTITY_TEXT_HPP
#define FNTECH3_QCOMMON_ENTITY_TEXT_HPP

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace fntech3::entities {

/* MAX_TOKEN_CHARS: the engine's token buffer, and every Quake III-line game
 * module's G_ParseSpawnVars buffer. */
inline constexpr std::size_t kTokenBytes = 1024;

struct Token {
	std::string text;
	std::size_t offset = 0;		/* where the token begins in the text */
	int line = 1;				/* the line it begins on, from 1 */
	bool quoted = false;
	bool truncated = false;		/* longer than the kTokenBytes - 1 kept */
};

/* COM_ParseExt( &text, qtrue ), one token per call. It reads the text where
 * it lies, so the text must outlive it. */
class Tokenizer {
public:
	explicit Tokenizer( std::string_view text ) noexcept : text_( text ) {}

	/* False at the end of the text, as the engine's entity-token trap
	 * returns qfalse there; an empty quoted token is still a token. */
	bool Next( Token &token )
	{
		token = Token{};
		if ( ended_ ) {
			return false;
		}

		int c = 0;
		for ( ;; ) {
			while ( ( c = At( position_ ) ) <= ' ' ) {
				if ( !c ) {
					ended_ = true;
					return false;
				}
				if ( c == '\n' ) {
					line_++;
				}
				position_++;
			}
			if ( c == '/' && At( position_ + 1 ) == '/' ) {
				position_ += 2;
				while ( At( position_ ) && At( position_ ) != '\n' ) {
					position_++;
				}
			} else if ( c == '/' && At( position_ + 1 ) == '*' ) {
				position_ += 2;
				while ( At( position_ ) && ( At( position_ ) != '*' || At( position_ + 1 ) != '/' ) ) {
					if ( At( position_ ) == '\n' ) {
						line_++;
					}
					position_++;
				}
				if ( At( position_ ) ) {
					position_ += 2;
				}
			} else {
				break;
			}
		}

		token.offset = position_;
		token.line = line_;
		if ( c == '"' ) {
			token.quoted = true;
			position_++;
			for ( ;; ) {
				c = At( position_ );
				if ( c == '"' || c == 0 ) {
					if ( c == '"' ) {
						position_++;
					}
					return true;
				}
				position_++;
				if ( c == '\n' ) {
					line_++;
				}
				Append( token, c );
			}
		}

		do {
			Append( token, c );
			position_++;
			c = At( position_ );
		} while ( c > ' ' );
		return true;
	}

	[[nodiscard]] std::size_t Offset() const noexcept { return position_; }
	[[nodiscard]] int Line() const noexcept { return line_; }

private:
	/* The byte as the engine reads it: a `char` widened to int. */
	[[nodiscard]] int At( std::size_t index ) const noexcept
	{
		return index < text_.size() ? static_cast<int>( text_[index] ) : 0;
	}

	static void Append( Token &token, int c )
	{
		if ( token.text.size() < kTokenBytes - 1 ) {
			token.text.push_back( static_cast<char>( c ) );
		} else {
			token.truncated = true;
		}
	}

	std::string_view text_;
	std::size_t position_ = 0;
	int line_ = 1;
	bool ended_ = false;
};

/* Q_stricmp's equality: ASCII letters fold, every other byte must match. */
[[nodiscard]] inline bool EqualsNoCase( std::string_view a, std::string_view b ) noexcept
{
	if ( a.size() != b.size() ) {
		return false;
	}
	for ( std::size_t i = 0; i < a.size(); i++ ) {
		char x = a[i], y = b[i];
		if ( x >= 'A' && x <= 'Z' ) x = static_cast<char>( x + ( 'a' - 'A' ) );
		if ( y >= 'A' && y <= 'Z' ) y = static_cast<char>( y + ( 'a' - 'A' ) );
		if ( x != y ) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] inline bool StartsWithNoCase( std::string_view text, std::string_view prefix ) noexcept
{
	return text.size() >= prefix.size() && EqualsNoCase( text.substr( 0, prefix.size() ), prefix );
}

struct KeyValue {
	std::string key;
	std::string value;

	friend bool operator==( const KeyValue &a, const KeyValue &b )
	{
		return a.key == b.key && a.value == b.value;
	}
	friend bool operator!=( const KeyValue &a, const KeyValue &b ) { return !( a == b ); }
};

struct Entity {
	std::vector<KeyValue> pairs;
	int line = 0;				/* the line of its opening brace; 0 for a new one */

	/* The game modules' G_SpawnString finds a key's first occurrence,
	 * ignoring case. */
	[[nodiscard]] int IndexOf( std::string_view key ) const noexcept
	{
		for ( std::size_t i = 0; i < pairs.size(); i++ ) {
			if ( EqualsNoCase( pairs[i].key, key ) ) {
				return static_cast<int>( i );
			}
		}
		return -1;
	}

	[[nodiscard]] bool Has( std::string_view key ) const noexcept { return IndexOf( key ) >= 0; }

	[[nodiscard]] std::string_view Value( std::string_view key, std::string_view fallback = {} ) const noexcept
	{
		const int index = IndexOf( key );
		return index >= 0 ? std::string_view( pairs[static_cast<std::size_t>( index )].value ) : fallback;
	}

	/* The first occurrence takes the value; a new key is appended. */
	void Set( std::string_view key, std::string_view value )
	{
		const int index = IndexOf( key );
		if ( index >= 0 ) {
			pairs[static_cast<std::size_t>( index )].value.assign( value );
		} else {
			pairs.push_back( KeyValue{ std::string( key ), std::string( value ) } );
		}
	}

	/* Every occurrence; how many there were. */
	std::size_t Remove( std::string_view key )
	{
		std::size_t removed = 0;
		for ( std::size_t i = 0; i < pairs.size(); ) {
			if ( EqualsNoCase( pairs[i].key, key ) ) {
				pairs.erase( pairs.begin() + static_cast<std::ptrdiff_t>( i ) );
				removed++;
			} else {
				i++;
			}
		}
		return removed;
	}

	[[nodiscard]] std::string_view Classname() const noexcept { return Value( "classname" ); }

	friend bool operator==( const Entity &a, const Entity &b ) { return a.pairs == b.pairs; }
	friend bool operator!=( const Entity &a, const Entity &b ) { return !( a == b ); }
};

struct ParseResult {
	std::vector<Entity> entities;
	/* False where a Quake III-line game module would stop the level with
	 * G_Error; the entities read before that point are kept. */
	bool ok = true;
	std::string error;
	int errorLine = 0;
	/* Some token was longer than the 1023 bytes a game module is handed. */
	bool truncated = false;
};

/*
G_SpawnEntitiesFromString's reading: entity after entity, each an opening
brace token, then key and value tokens until a key that begins with a
closing brace. A key or value token is compared by its first byte only,
quoted or not, as the game modules compare it.
*/
[[nodiscard]] inline ParseResult Parse( std::string_view text )
{
	ParseResult result;
	Tokenizer tokenizer( text );
	Token token;
	const auto fail = [&result]( const Token &at, std::string message ) {
		result.ok = false;
		result.error = std::move( message );
		result.errorLine = at.line;
	};

	while ( tokenizer.Next( token ) ) {
		result.truncated = result.truncated || token.truncated;
		if ( token.text.empty() || token.text[0] != '{' ) {
			fail( token, "found \"" + token.text + "\" where an entity should begin with {" );
			return result;
		}
		Entity entity;
		entity.line = token.line;
		Token key, value;
		for ( ;; ) {
			if ( !tokenizer.Next( key ) ) {
				fail( token, "the entity begun here has no closing brace" );
				result.entities.push_back( std::move( entity ) );
				return result;
			}
			result.truncated = result.truncated || key.truncated;
			if ( !key.text.empty() && key.text[0] == '}' ) {
				break;
			}
			if ( !tokenizer.Next( value ) ) {
				fail( token, "the entity begun here has no closing brace" );
				result.entities.push_back( std::move( entity ) );
				return result;
			}
			result.truncated = result.truncated || value.truncated;
			if ( !value.text.empty() && value.text[0] == '}' ) {
				fail( value, "a closing brace in place of the value of \"" + key.text + "\"" );
				result.entities.push_back( std::move( entity ) );
				return result;
			}
			entity.pairs.push_back( KeyValue{ std::move( key.text ), std::move( value.text ) } );
		}
		result.entities.push_back( std::move( entity ) );
	}
	return result;
}

/* The text as q3map and its successors write an entity lump: braces on
 * their own lines, each pair quoted on its own line, LF line ends. */
[[nodiscard]] inline std::string Serialize( const std::vector<Entity> &entities )
{
	std::size_t bytes = 0;
	for ( const Entity &entity : entities ) {
		bytes += 4;
		for ( const KeyValue &pair : entity.pairs ) {
			bytes += pair.key.size() + pair.value.size() + 6;
		}
	}
	std::string text;
	text.reserve( bytes );
	for ( const Entity &entity : entities ) {
		text += "{\n";
		for ( const KeyValue &pair : entity.pairs ) {
			text += '"';
			text += pair.key;
			text += "\" \"";
			text += pair.value;
			text += "\"\n";
		}
		text += "}\n";
	}
	return text;
}

enum class TextProblem {
	None,
	Empty,				/* a key with no name */
	Quote,				/* a quote would end the token early */
	Nul,				/* the text would end there */
	ClosingBrace,		/* a token that begins with '}' ends the entity */
	TooLong				/* a game module keeps only the first 1023 bytes */
};

/* Whether a key or a value can be written so that it reads back the same. */
[[nodiscard]] inline TextProblem CheckToken( std::string_view text, bool isKey ) noexcept
{
	if ( isKey && text.empty() ) {
		return TextProblem::Empty;
	}
	for ( const char c : text ) {
		if ( c == '"' ) return TextProblem::Quote;
		if ( c == '\0' ) return TextProblem::Nul;
	}
	if ( !text.empty() && text[0] == '}' ) {
		return TextProblem::ClosingBrace;
	}
	if ( text.size() > kTokenBytes - 1 ) {
		return TextProblem::TooLong;
	}
	return TextProblem::None;
}

[[nodiscard]] inline const char *DescribeProblem( TextProblem problem ) noexcept
{
	switch ( problem ) {
	case TextProblem::None: return "";
	case TextProblem::Empty: return "a key needs a name";
	case TextProblem::Quote: return "a double quote cannot appear in entity text";
	case TextProblem::Nul: return "a NUL byte cannot appear in entity text";
	case TextProblem::ClosingBrace: return "a key or value cannot begin with '}'";
	case TextProblem::TooLong: return "game modules keep only the first 1023 bytes";
	}
	return "";
}

/* G_SpawnVector's sscanf( "%f %f %f" ): the components read before the
 * first that does not parse; the rest are left as they were. */
inline int ParseVector( std::string_view text, float out[3] )
{
	std::string copy( text );
	const char *cursor = copy.c_str();
	int count = 0;
	while ( count < 3 ) {
		char *end = nullptr;
		const double value = std::strtod( cursor, &end );
		if ( end == cursor ) {
			break;
		}
		out[count++] = static_cast<float>( value );
		cursor = end;
	}
	return count;
}

/* A number as a mapper would type it: no exponent, no trailing zeros, at
 * most three decimals, and no "-0". */
[[nodiscard]] inline std::string FormatNumber( double value )
{
	char buffer[64];
	std::snprintf( buffer, sizeof( buffer ), "%.3f", value );
	std::string text( buffer );
	const std::size_t point = text.find_first_of( ".," );
	if ( point != std::string::npos ) {
		while ( !text.empty() && text.back() == '0' ) {
			text.pop_back();
		}
		if ( !text.empty() && ( text.back() == '.' || text.back() == ',' ) ) {
			text.pop_back();
		}
	}
	if ( text == "-0" ) {
		text = "0";
	}
	return text;
}

[[nodiscard]] inline std::string FormatVector( const float value[3] )
{
	return FormatNumber( value[0] ) + " " + FormatNumber( value[1] ) + " " + FormatNumber( value[2] );
}

} // namespace fntech3::entities

#endif // FNTECH3_QCOMMON_ENTITY_TEXT_HPP
