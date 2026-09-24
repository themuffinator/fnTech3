/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_TIKI_FORMAT_HPP
#define FNTECH3_COMPAT_ALICE_TIKI_FORMAT_HPP

/* Bounded reader for Alice's `.tik` model definitions.
 *
 * A definition is the index that ties this title's models together: it names
 * the directory the binary geometry lives in, maps each surface to a shader,
 * and maps each animation name to the `.tan` or `.ska` file that holds it.
 * Nothing else in the model pipeline can be resolved without it.
 *
 * It is a brace-nested token format with three wrinkles a reader has to carry,
 * each of which the 801 installed definitions demonstrate:
 *
 *   - a file may open with C-style `#define` lines before the `TIKI` magic;
 *   - `$define name value` declares a macro that `$name$` expands, and the
 *     sound paths in the animation command blocks need it;
 *   - one shipped definition never closes its last block, so a reader that
 *     requires balanced braces refuses an asset the game ships.
 *
 * This reader extracts structure, not meaning: it locates the blocks and the
 * name/value pairs inside them and leaves every command uninterpreted. It
 * allocates nothing, holding views into the caller's buffer.
 *
 * See docs/fntech3/2026-09-22-alice-models.md. */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace tiki {

inline constexpr std::size_t MaxSurfaces = 64;
inline constexpr std::size_t MaxAnimations = 512;
inline constexpr std::size_t MaxMacros = 32;
/* A definition is a small text file; the largest installed one is under 16 KB. */
inline constexpr std::size_t MaxBytes = 1u << 20;

enum class Error {
	None, Size, Magic, Depth, Truncated, TooMany
};

inline const char *ErrorName( Error error ) {
	switch ( error ) {
	case Error::None: return "none";
	case Error::Size: return "file empty or too large";
	case Error::Magic: return "no TIKI magic";
	case Error::Depth: return "a closing brace with no opening brace";
	case Error::Truncated: return "a value with no token after it";
	case Error::TooMany: return "more records than a definition may hold";
	}
	return "unknown";
}

/* A view into the caller's buffer. Definitions are read once and discarded,
 * so nothing is copied out of them. */
struct Token {
	const char *text;
	std::size_t length;

	bool Equals( const char *other ) const {
		const std::size_t n = std::strlen( other );
		return n == length && !std::memcmp( text, other, n );
	}

	bool EqualsNoCase( const char *other ) const {
		if ( std::strlen( other ) != length ) return false;
		for ( std::size_t i = 0; i < length; ++i ) {
			char a = text[i], b = other[i];
			if ( a >= 'A' && a <= 'Z' ) a = char( a - 'A' + 'a' );
			if ( b >= 'A' && b <= 'Z' ) b = char( b - 'A' + 'a' );
			if ( a != b ) return false;
		}
		return true;
	}

	bool EndsWithNoCase( const char *suffix ) const {
		const std::size_t n = std::strlen( suffix );
		if ( n > length ) return false;
		for ( std::size_t i = 0; i < n; ++i ) {
			char a = text[length - n + i], b = suffix[i];
			if ( a >= 'A' && a <= 'Z' ) a = char( a - 'A' + 'a' );
			if ( b >= 'A' && b <= 'Z' ) b = char( b - 'A' + 'a' );
			if ( a != b ) return false;
		}
		return true;
	}
};

struct Surface {
	Token name;
	Token shader;
};

struct Animation {
	Token name;
	Token file;       /* a .tan, .ska or .skb leaf, relative to the path */
	bool hasCommands; /* a block of timed commands follows the file */
};

struct Macro {
	Token name;       /* without the surrounding dollars */
	Token value;
};

struct Definition {
	Token path;                       /* the directory the binaries live in */
	bool hasPath;
	bool hasSetup;
	bool hasInit;
	bool hasAnimations;
	/* True when the file ends inside a block. One shipped definition does,
	 * and the retail engine accepts it, so this is reported and not refused. */
	bool unterminated;
	std::size_t surfaces;
	std::size_t animations;
	std::size_t macros;
	Surface surface[MaxSurfaces];
	Animation animation[MaxAnimations];
	Macro macro[MaxMacros];
};

class Lexer {
public:
	Lexer( const char *data, std::size_t bytes )
		: at_( data ), end_( data + bytes ) {}

	/* The next token, with comments and whitespace skipped. A quoted string
	 * yields its contents; a brace yields itself. */
	bool Next( Token &out ) {
		for ( ;; ) {
			while ( at_ < end_ && ( unsigned char )( *at_ ) <= ' ' ) ++at_;
			if ( at_ + 1 < end_ && at_[0] == '/' && at_[1] == '/' ) {
				while ( at_ < end_ && *at_ != '\n' ) ++at_;
				continue;
			}
			if ( at_ + 1 < end_ && at_[0] == '/' && at_[1] == '*' ) {
				at_ += 2;
				while ( at_ + 1 < end_ && !( at_[0] == '*' && at_[1] == '/' ) ) ++at_;
				at_ = at_ + 1 < end_ ? at_ + 2 : end_;
				continue;
			}
			break;
		}
		if ( at_ >= end_ ) return false;
		if ( *at_ == '"' ) {
			++at_;
			const char *start = at_;
			while ( at_ < end_ && *at_ != '"' ) ++at_;
			out = Token{ start, std::size_t( at_ - start ) };
			if ( at_ < end_ ) ++at_;
			return true;
		}
		if ( *at_ == '{' || *at_ == '}' ) {
			out = Token{ at_, 1 };
			++at_;
			return true;
		}
		const char *start = at_;
		while ( at_ < end_ && ( unsigned char )( *at_ ) > ' '
			&& *at_ != '{' && *at_ != '}' && *at_ != '"' ) {
			++at_;
		}
		out = Token{ start, std::size_t( at_ - start ) };
		return true;
	}

	/* Drop the rest of the current line. A `#define` preamble is not part
	 * of the token grammar, so it is skipped a line at a time. */
	void SkipLine() {
		while ( at_ < end_ && *at_ != '\n' ) ++at_;
	}

private:
	const char *at_;
	const char *end_;
};

/* Resolve `$name$` against the macros a definition declared. An unknown macro
 * is left alone rather than dropped, because that is what the caller sees in
 * the file. Returns false when the token is not a macro reference. */
inline bool ExpandMacro( const Definition &definition, const Token &token, Token &out ) {
	if ( token.length < 3 || token.text[0] != '$' ) return false;
	std::size_t close = 1;
	while ( close < token.length && token.text[close] != '$' ) ++close;
	if ( close >= token.length ) return false;
	const Token name{ token.text + 1, close - 1 };
	for ( std::size_t i = 0; i < definition.macros; ++i ) {
		if ( definition.macro[i].name.length == name.length
			&& !std::memcmp( definition.macro[i].name.text, name.text, name.length ) ) {
			out = definition.macro[i].value;
			return true;
		}
	}
	return false;
}

/*
==================
ParseDefinition

Read a `.tik` definition's structure. Blocks and their name/value pairs are
located; no command is interpreted and no file it names is opened.
==================
*/
inline Error ParseDefinition( const void *data, std::size_t bytes, Definition &out ) {
	out = Definition{};
	if ( !data || !bytes || bytes > MaxBytes ) return Error::Size;
	const char *text = static_cast<const char *>( data );

	Lexer lexer( text, bytes );
	Token token{};

	/* A definition may open with C preprocessor lines. Two installed ones do,
	 * so the magic is looked for rather than demanded first. */
	bool magic = false;
	while ( lexer.Next( token ) ) {
		if ( token.Equals( "TIKI" ) ) {
			magic = true;
			break;
		}
		if ( token.length && token.text[0] == '#' ) {
			/* A preprocessor line carries tokens of its own; none of them is
			 * the magic, so the whole line goes. */
			lexer.SkipLine();
			continue;
		}
		/* Anything else before the magic is not a definition. */
		return Error::Magic;
	}
	if ( !magic ) return Error::Magic;

	int depth = 0;
	Token blockName{ nullptr, 0 };
	Token pending{ nullptr, 0 };
	bool havePending = false;
	bool inSetup = false, inAnimations = false;
	int animationDepth = -1;

	while ( lexer.Next( token ) ) {
		if ( token.Equals( "{" ) ) {
			if ( depth == 0 && blockName.length ) {
				if ( blockName.EqualsNoCase( "setup" ) ) { out.hasSetup = true; inSetup = true; }
				else if ( blockName.EqualsNoCase( "init" ) ) out.hasInit = true;
				else if ( blockName.EqualsNoCase( "animations" ) ) {
					out.hasAnimations = true;
					inAnimations = true;
				}
			}
			/* A brace right after an animation's file opens its command block. */
			if ( inAnimations && depth == 1 && out.animations
				&& animationDepth == int( out.animations ) ) {
				out.animation[out.animations - 1].hasCommands = true;
			}
			++depth;
			blockName = Token{ nullptr, 0 };
			havePending = false;
			continue;
		}
		if ( token.Equals( "}" ) ) {
			if ( --depth < 0 ) return Error::Depth;
			if ( depth == 0 ) { inSetup = false; inAnimations = false; }
			havePending = false;
			continue;
		}

		/* `$define name value` is the only statement that changes how later
		 * tokens read, and the installed definitions put it at file scope,
		 * above the first block, so it is handled before the depth split. */
		if ( token.EqualsNoCase( "$define" ) ) {
			Token name{}, value{};
			if ( !lexer.Next( name ) || !lexer.Next( value ) ) return Error::Truncated;
			if ( out.macros == MaxMacros ) return Error::TooMany;
			out.macro[out.macros++] = Macro{ name, value };
			havePending = false;
			continue;
		}

		if ( depth == 0 ) {
			blockName = token;
			continue;
		}

		if ( inSetup && depth == 1 ) {
			if ( token.EqualsNoCase( "path" ) ) {
				if ( !lexer.Next( token ) ) return Error::Truncated;
				out.path = token;
				out.hasPath = true;
				havePending = false;
				continue;
			}
			if ( token.EqualsNoCase( "surface" ) ) {
				Token name{}, keyword{}, shader{};
				if ( !lexer.Next( name ) ) return Error::Truncated;
				/* `surface <name> shader <image>` is the form that names an
				 * image; a surface line may carry other keywords instead. */
				if ( !lexer.Next( keyword ) ) return Error::Truncated;
				if ( keyword.EqualsNoCase( "shader" ) ) {
					if ( !lexer.Next( shader ) ) return Error::Truncated;
					if ( out.surfaces == MaxSurfaces ) return Error::TooMany;
					out.surface[out.surfaces++] = Surface{ name, shader };
				}
				havePending = false;
				continue;
			}
		}

		if ( inAnimations && depth == 1 ) {
			/* An entry is a name followed by a file. Anything else at this
			 * depth is a keyword this reader does not interpret. */
			if ( havePending ) {
				if ( token.EndsWithNoCase( ".tan" ) || token.EndsWithNoCase( ".ska" )
					|| token.EndsWithNoCase( ".skb" ) ) {
					if ( out.animations == MaxAnimations ) return Error::TooMany;
					out.animation[out.animations++] = Animation{ pending, token, false };
					animationDepth = int( out.animations );
					havePending = false;
					continue;
				}
				/* Not a file: the previous token was a keyword, so the entry is
				 * still open and no command block can belong to the last one. */
				pending = token;
				animationDepth = -1;
				continue;
			}
			pending = token;
			havePending = true;
			animationDepth = -1;
			continue;
		}
		havePending = false;
	}

	/* The file ended. One shipped definition ends inside its animations block,
	 * and the retail engine loads it, so this is reported rather than refused. */
	out.unterminated = depth != 0;
	return Error::None;
}

} // namespace tiki
} // namespace alice
} // namespace fntech3

#endif
