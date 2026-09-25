/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_URC_HPP
#define FNTECH3_COMPAT_ALICE_URC_HPP

/* American McGee's Alice's menu resources: the `ui/*.urc` files its engine
 * reads at start-up and turns into menus. This header only reads them -- it
 * turns a file's text into a plain tree of containers, widgets and their
 * property lines -- so the grammar can be tested apart from the engine that
 * gives the properties meaning.
 *
 * The rules, from retail alice.exe (image base 0x400000):
 *
 *   Script tokenizer   0x00420eb0, 0x00421100, 0x00420d20, 0x00420d70
 *     Whitespace is every byte at or below a space, and the comma. Outside
 *     quotes `//` and `;` start a comment that runs to the end of the line.
 *     A quoted token cannot hold a raw newline. Escapes work in and out of
 *     quotes: \" \' \\ keep the character, \n and \r give a line feed, and
 *     any other escaped byte is itself. A token is at most 255 bytes.
 *   UILayout parse     0x004cbc50
 *     `end.` (case-sensitive) stops the file. `include <file>` reads another
 *     file in place. `resource` reads one widget: a class token, `{`, one
 *     property per line until `}`. Anything else is an event, applied to the
 *     current menu, whose arguments are every remaining token on its line.
 *     `menu` starts a new menu; its header is `menu "<name>" w h dir [time]`.
 *   Widget classes     0x0050ba68 (translation), 0x004cc7a0 (menu header)
 *     `Label` becomes FakkLabel; every other class is "UI" plus the token,
 *     looked up without regard to case. An unknown class is reported by name.
 *
 * Nothing here decides what a property means: an unknown keyword is kept with
 * its line so the engine can report it the way retail does ("UI: Invalid token
 * '%s' on line %d of %s"). */

#include <cstddef>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace fntech3 {
namespace alice {
namespace urc {

inline constexpr std::size_t MaxTokenBytes = 255;
/* Retail bounds no include: a cycle there recurses until the stack runs out.
 * Here a file nests at most MaxIncludeDepth deep, and one parse reads at most
 * MaxIncludes included files holding MaxIncludedBytes in all, so a file that
 * includes itself many times over cannot multiply the work. An include past
 * any of them is refused with a diagnostic. */
inline constexpr int MaxIncludeDepth = 8;
inline constexpr int MaxIncludes = 64;
inline constexpr std::size_t MaxIncludedBytes = std::size_t( 4 ) << 20;
/* The diagnostics one parse keeps; any further ones are only counted. */
inline constexpr std::size_t MaxDiagnostics = 256;

struct Token {
	std::string text;
	int line = 0;
	bool quoted = false;
};

/* One keyword and the rest of its line. */
struct Property {
	std::string name;
	std::vector<std::string> args;
	int line = 0;
};

struct Widget {
	std::string classToken;     /* as written: "Label", "Button" */
	std::vector<Property> properties;
	int line = 0;
};

struct Menu {
	Property header;            /* the `menu` line itself */
	std::vector<Property> events;
	std::vector<Widget> widgets;
};

struct Diagnostic {
	std::string message;
	int line = 0;
};

struct Layout {
	std::vector<Menu> menus;
	std::vector<Diagnostic> diagnostics;
	std::size_t diagnosticsDropped = 0;   /* past MaxDiagnostics */
	bool ended = false;          /* an `end.` was reached */
};

namespace detail {

inline char Lower( char c ) { return c >= 'A' && c <= 'Z' ? static_cast<char>( c - 'A' + 'a' ) : c; }

inline bool IsWhitespace( unsigned char c ) { return c <= ' ' || c == ','; }

class Tokenizer {
public:
	Tokenizer( const char *text, std::size_t length ) : cursor_( text ), end_( text + length ) {}

	/* The next token, or false at the end of the text. */
	bool Next( Token &token ) {
		SkipSpaceAndComments();
		if ( cursor_ >= end_ ) return false;
		token = Token();
		token.line = line_;
		if ( *cursor_ == '"' ) {
			token.quoted = true;
			++cursor_;
			while ( cursor_ < end_ && *cursor_ != '"' && *cursor_ != '\n' ) {
				Append( token, ReadCharacter() );
			}
			if ( cursor_ < end_ && *cursor_ == '"' ) ++cursor_;
			return true;
		}
		while ( cursor_ < end_ && !IsWhitespace( static_cast<unsigned char>( *cursor_ ) ) ) {
			if ( *cursor_ == ';' || ( *cursor_ == '/' && cursor_ + 1 < end_ && cursor_[1] == '/' ) ) break;
			Append( token, ReadCharacter() );
		}
		return true;
	}

	/* Whether another token starts on the current line. */
	bool TokenOnLine() {
		SkipSpaceAndComments( false );
		return cursor_ < end_ && *cursor_ != '\n';
	}

	int Line() const { return line_; }

private:
	void Append( Token &token, char c ) {
		if ( token.text.size() < MaxTokenBytes ) token.text += c;
	}

	char ReadCharacter() {
		char c = *cursor_++;
		if ( c == '\\' && cursor_ < end_ ) {
			const char escaped = *cursor_++;
			switch ( escaped ) {
			case 'n': case 'r': return '\n';
			default: return escaped;
			}
		}
		return c;
	}

	void SkipSpaceAndComments( bool crossLines = true ) {
		while ( cursor_ < end_ ) {
			const unsigned char c = static_cast<unsigned char>( *cursor_ );
			if ( c == '\n' ) {
				if ( !crossLines ) return;
				++line_;
				++cursor_;
				continue;
			}
			if ( IsWhitespace( c ) ) {
				++cursor_;
				continue;
			}
			if ( c == ';' || ( c == '/' && cursor_ + 1 < end_ && cursor_[1] == '/' ) ) {
				while ( cursor_ < end_ && *cursor_ != '\n' ) ++cursor_;
				continue;
			}
			return;
		}
	}

	const char *cursor_;
	const char *end_;
	int line_ = 1;
};

} // namespace detail

inline bool EqualsNoCase( const std::string &a, const char *b ) {
	std::size_t i = 0;
	for ( ; i < a.size() && b[i]; ++i ) {
		if ( detail::Lower( a[i] ) != detail::Lower( b[i] ) ) return false;
	}
	return i == a.size() && !b[i];
}

/* The class a `resource` token instantiates, or an empty string for a token
 * naming no widget class this client serves. Retail puts "UI" before any
 * token and takes whatever registered class answers to it, case aside
 * (0x004cc327, getClass 0x0042e9d0) -- about 35 classes; these are the ones
 * Alice's and Heavy Metal: F.A.K.K.2's menus use. `Label` alone is translated,
 * and exactly (0x0040e240): a lowercase `label` is a plain UILabel, which
 * draws as a FakkLabel without its keywords. */
inline std::string CanonicalClass( const std::string &token ) {
	static const char *const kClasses[][2] = {
		{ "Label", "FakkLabel" },
		{ "Button", "UIButton" },
		{ "CheckBox", "UICheckBox" },
		{ "Slider", "UISlider" },
		{ "Field", "UIField" },
		{ "List", "UIList" },
		{ "FakkBindList", "UIFakkBindList" },
		{ "FakkLoadGameClass", "UIFAKKLoadGameClass" },
		{ "AutoScroll", "UIAutoScroll" },
		{ "Alice3D", "UIAlice3D" },
		{ "PulldownMenuContainer", "UIPulldownMenuContainer" },
		{ "MultiLineEdit", "UIMultiLineEdit" },
	};
	for ( const auto &entry : kClasses ) {
		if ( EqualsNoCase( token, entry[0] ) ) return entry[1];
	}
	return std::string();
}

/* Reads a file included by `include`; answers false for a file that cannot be
 * read. */
using IncludeReader = std::function<bool( const std::string &name, std::string &text )>;

namespace detail {

/* What one parse has spent on includes so far. */
struct IncludeBudget {
	int includes = 0;
	std::size_t bytes = 0;
};

inline void Report( Layout &layout, std::string message, int line ) {
	if ( layout.diagnostics.size() < MaxDiagnostics ) {
		layout.diagnostics.push_back( { std::move( message ), line } );
	} else {
		++layout.diagnosticsDropped;
	}
}

inline void ReadRestOfLine( Tokenizer &tokens, std::vector<std::string> &args ) {
	Token token;
	while ( tokens.TokenOnLine() && tokens.Next( token ) ) args.push_back( token.text );
}

inline void ParseInto( Layout &layout, const char *text, std::size_t length, const IncludeReader &include,
	int depth, IncludeBudget &budget ) {
	Tokenizer tokens( text, length );
	Token token;
	while ( !layout.ended && tokens.Next( token ) ) {
		if ( !token.quoted && token.text == "end." ) {
			layout.ended = true;
			return;
		}
		if ( !token.quoted && EqualsNoCase( token.text, "include" ) ) {
			Token name;
			if ( !tokens.TokenOnLine() || !tokens.Next( name ) ) {
				Report( layout, "include without a file name", token.line );
				continue;
			}
			std::string included;
			if ( depth >= MaxIncludeDepth ) {
				Report( layout, "include nested too deeply: " + name.text, token.line );
			} else if ( budget.includes >= MaxIncludes ) {
				Report( layout, "too many includes: " + name.text, token.line );
			} else {
				// A file that cannot be read still spends an include.
				++budget.includes;
				if ( !include || !include( name.text, included ) ) {
					Report( layout, "couldn't include " + name.text, token.line );
				} else if ( included.size() > MaxIncludedBytes - budget.bytes ) {
					Report( layout, "included text too large: " + name.text, token.line );
				} else {
					budget.bytes += included.size();
					ParseInto( layout, included.data(), included.size(), include, depth + 1, budget );
					// An included file's `end.` ends that file, not its includer.
					layout.ended = false;
				}
			}
			continue;
		}
		if ( !token.quoted && EqualsNoCase( token.text, "resource" ) ) {
			Token classToken;
			if ( !tokens.Next( classToken ) ) {
				Report( layout, "resource without a class", token.line );
				return;
			}
			Widget widget;
			widget.classToken = classToken.text;
			widget.line = classToken.line;
			Token open;
			if ( !tokens.Next( open ) || open.text != "{" ) {
				Report( layout, "expected '{' after resource " + classToken.text, classToken.line );
				return;
			}
			bool closed = false;
			Token keyword;
			while ( tokens.Next( keyword ) ) {
				if ( !keyword.quoted && keyword.text == "}" ) {
					closed = true;
					break;
				}
				Property property;
				property.name = keyword.text;
				property.line = keyword.line;
				ReadRestOfLine( tokens, property.args );
				widget.properties.push_back( std::move( property ) );
			}
			if ( !closed ) {
				Report( layout, "resource " + classToken.text + " has no closing '}'", widget.line );
				return;
			}
			if ( CanonicalClass( widget.classToken ).empty() ) {
				Report( layout, "Tried to instantiate a widget that doesn't exist: "
					+ widget.classToken, widget.line );
			}
			if ( layout.menus.empty() ) {
				Report( layout, "resource outside a menu", widget.line );
				continue;
			}
			layout.menus.back().widgets.push_back( std::move( widget ) );
			continue;
		}

		Property event;
		event.name = token.text;
		event.line = token.line;
		ReadRestOfLine( tokens, event.args );
		if ( EqualsNoCase( event.name, "menu" ) ) {
			Menu menu;
			menu.header = std::move( event );
			layout.menus.push_back( std::move( menu ) );
		} else if ( layout.menus.empty() ) {
			Report( layout, "event outside a menu: " + event.name, event.line );
		} else {
			layout.menus.back().events.push_back( std::move( event ) );
		}
	}
}

} // namespace detail

inline Layout Parse( const char *text, std::size_t length, const IncludeReader &include = IncludeReader() ) {
	Layout layout;
	detail::IncludeBudget budget;
	if ( text ) detail::ParseInto( layout, text, length, include, 0, budget );
	return layout;
}

/* ---- menu headers (0x004cc7a0) ------------------------------------------ */

enum class Direction { None = 0, FromLeft = 1, FromRight = 2, FromBottom = 3, FromTop = 4, FromPoint = 5 };

/* A `direction` or menu-header direction word. from_point only exists for
 * widgets; a menu header treats it, like any unknown word, as none. */
inline Direction DirectionFromName( const std::string &name, bool widget ) {
	if ( EqualsNoCase( name, "from_left" ) ) return Direction::FromLeft;
	if ( EqualsNoCase( name, "from_right" ) ) return Direction::FromRight;
	if ( EqualsNoCase( name, "from_bottom" ) ) return Direction::FromBottom;
	if ( EqualsNoCase( name, "from_top" ) ) return Direction::FromTop;
	if ( widget && EqualsNoCase( name, "from_point" ) ) return Direction::FromPoint;
	return Direction::None;
}

struct MenuHeader {
	std::string name;
	float width = 0.0f, height = 0.0f;
	Direction direction = Direction::None;
	float motionTime = 0.0f;
	bool hasMotionTime = false;
};

/* `menu "<name>" <w> <h> <dir> [<time>]`: the time is read only when all five
 * arguments are there (0x004cc7a0). Fewer than four arguments name no menu. */
inline bool ReadMenuHeader( const Property &header, MenuHeader &out ) {
	if ( header.args.size() < 4 ) return false;
	out = MenuHeader();
	out.name = header.args[0];
	out.width = static_cast<float>( std::atof( header.args[1].c_str() ) );
	out.height = static_cast<float>( std::atof( header.args[2].c_str() ) );
	out.direction = DirectionFromName( header.args[3], false );
	if ( header.args.size() == 5 ) {
		out.motionTime = static_cast<float>( std::atof( header.args[4].c_str() ) );
		out.hasMotionTime = true;
	}
	return true;
}

/* `fontjustify <h> <v>` (0x004be500): center, left, right horizontally and
 * top, center, bottom vertically; an unknown word leaves that axis as it was. */
inline void ReadJustify( const std::vector<std::string> &args, int &horizontal, int &vertical ) {
	if ( args.size() > 0 ) {
		if ( EqualsNoCase( args[0], "center" ) ) horizontal = 0;
		else if ( EqualsNoCase( args[0], "left" ) ) horizontal = 1;
		else if ( EqualsNoCase( args[0], "right" ) ) horizontal = 2;
	}
	if ( args.size() > 1 ) {
		if ( EqualsNoCase( args[1], "top" ) ) vertical = 0;
		else if ( EqualsNoCase( args[1], "center" ) ) vertical = 1;
		else if ( EqualsNoCase( args[1], "bottom" ) ) vertical = 2;
	}
}

/* `borderstyle`: NONE, 3D_BORDER and INDENT_BORDER; anything else leaves the
 * style as it was (the default is INDENT_BORDER). */
inline bool ReadBorderStyle( const std::string &word, int &style ) {
	if ( EqualsNoCase( word, "NONE" ) ) { style = 0; return true; }
	if ( EqualsNoCase( word, "3D_BORDER" ) ) { style = 1; return true; }
	if ( EqualsNoCase( word, "INDENT_BORDER" ) ) { style = 2; return true; }
	return false;
}

/* The words `title` and `stuffcommand`-style events join with spaces. */
inline std::string Join( const std::vector<std::string> &args, std::size_t first = 0 ) {
	std::string joined;
	for ( std::size_t i = first; i < args.size(); ++i ) {
		if ( i > first ) joined += ' ';
		joined += args[i];
	}
	return joined;
}

} // namespace urc
} // namespace alice
} // namespace fntech3

#endif
