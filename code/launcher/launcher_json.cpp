/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fntech3::launcher::json {

namespace {

const std::string kEmptyString;
const std::vector<Value> kEmptyItems;
const std::vector<std::pair<std::string, Value>> kEmptyMembers;

class Parser {
public:
	explicit Parser( const std::string &text ) : text_( text ) {}

	bool Document( Value &out )
	{
		SkipSpace();
		if ( !ParseValue( out, 0 ) ) return false;
		SkipSpace();
		if ( pos_ != text_.size() ) return Fail( "unexpected text after the document" );
		return true;
	}

	std::string Error() const
	{
		return error_ + " at byte " + std::to_string( errorPos_ );
	}

private:
	bool Fail( const char *message )
	{
		if ( error_.empty() ) {
			error_ = message;
			errorPos_ = pos_;
		}
		return false;
	}

	void SkipSpace()
	{
		while ( pos_ < text_.size() ) {
			const char c = text_[pos_];
			if ( c != ' ' && c != '\t' && c != '\n' && c != '\r' ) break;
			++pos_;
		}
	}

	bool Literal( const char *word )
	{
		const std::size_t length = std::strlen( word );
		if ( text_.compare( pos_, length, word ) != 0 ) return Fail( "invalid literal" );
		pos_ += length;
		return true;
	}

	bool ParseValue( Value &out, int depth )
	{
		if ( depth > kMaxDepth ) return Fail( "document is nested too deeply" );
		if ( pos_ >= text_.size() ) return Fail( "unexpected end of document" );
		switch ( text_[pos_] ) {
		case '{': return ParseObject( out, depth );
		case '[': return ParseArray( out, depth );
		case '"': {
			std::string value;
			if ( !ParseString( value ) ) return false;
			out = Value::String( std::move( value ) );
			return true;
		}
		case 't':
			if ( !Literal( "true" ) ) return false;
			out = Value::Boolean( true );
			return true;
		case 'f':
			if ( !Literal( "false" ) ) return false;
			out = Value::Boolean( false );
			return true;
		case 'n':
			if ( !Literal( "null" ) ) return false;
			out = Value();
			return true;
		default:
			return ParseNumber( out );
		}
	}

	bool ParseObject( Value &out, int depth )
	{
		++pos_; // '{'
		Value object = Value::Object();
		SkipSpace();
		if ( pos_ < text_.size() && text_[pos_] == '}' ) {
			++pos_;
			out = std::move( object );
			return true;
		}
		for ( ;; ) {
			SkipSpace();
			if ( pos_ >= text_.size() || text_[pos_] != '"' ) return Fail( "expected a member name" );
			std::string key;
			if ( !ParseString( key ) ) return false;
			SkipSpace();
			if ( pos_ >= text_.size() || text_[pos_] != ':' ) return Fail( "expected ':' after a member name" );
			++pos_;
			SkipSpace();
			Value member;
			if ( !ParseValue( member, depth + 1 ) ) return false;
			object.Set( key, std::move( member ) );
			SkipSpace();
			if ( pos_ >= text_.size() ) return Fail( "unterminated object" );
			if ( text_[pos_] == ',' ) {
				++pos_;
				continue;
			}
			if ( text_[pos_] == '}' ) {
				++pos_;
				out = std::move( object );
				return true;
			}
			return Fail( "expected ',' or '}' in an object" );
		}
	}

	bool ParseArray( Value &out, int depth )
	{
		++pos_; // '['
		Value array = Value::Array();
		SkipSpace();
		if ( pos_ < text_.size() && text_[pos_] == ']' ) {
			++pos_;
			out = std::move( array );
			return true;
		}
		for ( ;; ) {
			SkipSpace();
			Value item;
			if ( !ParseValue( item, depth + 1 ) ) return false;
			array.Push( std::move( item ) );
			SkipSpace();
			if ( pos_ >= text_.size() ) return Fail( "unterminated array" );
			if ( text_[pos_] == ',' ) {
				++pos_;
				continue;
			}
			if ( text_[pos_] == ']' ) {
				++pos_;
				out = std::move( array );
				return true;
			}
			return Fail( "expected ',' or ']' in an array" );
		}
	}

	static int HexDigit( char c )
	{
		if ( c >= '0' && c <= '9' ) return c - '0';
		if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
		if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
		return -1;
	}

	bool Hex4( unsigned &value )
	{
		if ( text_.size() - pos_ < 4 ) return Fail( "truncated \\u escape" );
		value = 0;
		for ( int i = 0; i < 4; ++i ) {
			const int digit = HexDigit( text_[pos_ + i] );
			if ( digit < 0 ) return Fail( "invalid \\u escape" );
			value = value * 16u + unsigned( digit );
		}
		pos_ += 4;
		return true;
	}

	static void AppendUtf8( std::string &out, unsigned codepoint )
	{
		if ( codepoint < 0x80 ) {
			out += char( codepoint );
		} else if ( codepoint < 0x800 ) {
			out += char( 0xc0 | ( codepoint >> 6 ) );
			out += char( 0x80 | ( codepoint & 0x3f ) );
		} else if ( codepoint < 0x10000 ) {
			out += char( 0xe0 | ( codepoint >> 12 ) );
			out += char( 0x80 | ( ( codepoint >> 6 ) & 0x3f ) );
			out += char( 0x80 | ( codepoint & 0x3f ) );
		} else {
			out += char( 0xf0 | ( codepoint >> 18 ) );
			out += char( 0x80 | ( ( codepoint >> 12 ) & 0x3f ) );
			out += char( 0x80 | ( ( codepoint >> 6 ) & 0x3f ) );
			out += char( 0x80 | ( codepoint & 0x3f ) );
		}
	}

	bool ParseString( std::string &out )
	{
		++pos_; // opening quote
		out.clear();
		while ( pos_ < text_.size() ) {
			const unsigned char c = static_cast<unsigned char>( text_[pos_] );
			if ( c == '"' ) {
				++pos_;
				return true;
			}
			if ( c < 0x20 ) return Fail( "control character in a string" );
			if ( c != '\\' ) {
				out += char( c );
				++pos_;
				continue;
			}
			if ( ++pos_ >= text_.size() ) break;
			const char escape = text_[pos_++];
			switch ( escape ) {
			case '"': out += '"'; break;
			case '\\': out += '\\'; break;
			case '/': out += '/'; break;
			case 'b': out += '\b'; break;
			case 'f': out += '\f'; break;
			case 'n': out += '\n'; break;
			case 'r': out += '\r'; break;
			case 't': out += '\t'; break;
			case 'u': {
				unsigned codepoint;
				if ( !Hex4( codepoint ) ) return false;
				if ( codepoint >= 0xd800 && codepoint <= 0xdbff ) {
					unsigned low;
					if ( text_.compare( pos_, 2, "\\u" ) != 0 ) return Fail( "unpaired surrogate" );
					pos_ += 2;
					if ( !Hex4( low ) ) return false;
					if ( low < 0xdc00 || low > 0xdfff ) return Fail( "unpaired surrogate" );
					codepoint = 0x10000 + ( ( codepoint - 0xd800 ) << 10 ) + ( low - 0xdc00 );
				} else if ( codepoint >= 0xdc00 && codepoint <= 0xdfff ) {
					return Fail( "unpaired surrogate" );
				}
				AppendUtf8( out, codepoint );
				break;
			}
			default:
				return Fail( "invalid escape" );
			}
		}
		return Fail( "unterminated string" );
	}

	bool ParseNumber( Value &out )
	{
		const std::size_t start = pos_;
		auto digit = [this]() {
			return pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9';
		};
		if ( pos_ < text_.size() && text_[pos_] == '-' ) ++pos_;
		if ( !digit() ) return Fail( "unexpected character" );
		if ( text_[pos_] == '0' ) {
			++pos_;
		} else {
			while ( digit() ) ++pos_;
		}
		if ( pos_ < text_.size() && text_[pos_] == '.' ) {
			++pos_;
			if ( !digit() ) return Fail( "invalid number" );
			while ( digit() ) ++pos_;
		}
		if ( pos_ < text_.size() && ( text_[pos_] == 'e' || text_[pos_] == 'E' ) ) {
			++pos_;
			if ( pos_ < text_.size() && ( text_[pos_] == '+' || text_[pos_] == '-' ) ) ++pos_;
			if ( !digit() ) return Fail( "invalid number" );
			while ( digit() ) ++pos_;
		}
		const std::string literal = text_.substr( start, pos_ - start );
		const double value = std::strtod( literal.c_str(), nullptr );
		if ( !std::isfinite( value ) ) return Fail( "number out of range" );
		out = Value::Number( value );
		return true;
	}

	const std::string &text_;
	std::size_t pos_ = 0;
	std::string error_;
	std::size_t errorPos_ = 0;
};

void Indent( std::string &out, int depth )
{
	out.append( std::size_t( depth ) * 2, ' ' );
}

void WriteString( std::string &out, const std::string &value )
{
	out += '"';
	for ( const char ch : value ) {
		const unsigned char c = static_cast<unsigned char>( ch );
		switch ( c ) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if ( c < 0x20 ) {
				char escaped[8];
				std::snprintf( escaped, sizeof( escaped ), "\\u%04x", unsigned( c ) );
				out += escaped;
			} else {
				out += char( c );
			}
		}
	}
	out += '"';
}

void WriteNumber( std::string &out, double value )
{
	char buffer[40];
	// Integers within double precision print exactly, without a fraction.
	if ( std::fabs( value ) < 9007199254740992.0 && value == std::floor( value ) ) {
		std::snprintf( buffer, sizeof( buffer ), "%.0f", value );
	} else {
		std::snprintf( buffer, sizeof( buffer ), "%.17g", value );
	}
	out += buffer;
}

void Write( std::string &out, const Value &value, bool pretty, int depth )
{
	switch ( value.GetType() ) {
	case Value::Type::Null: out += "null"; return;
	case Value::Type::Bool: out += value.AsBool() ? "true" : "false"; return;
	case Value::Type::Number: WriteNumber( out, value.AsNumber() ); return;
	case Value::Type::String: WriteString( out, value.AsString() ); return;
	case Value::Type::Array: {
		const auto &items = value.Items();
		if ( items.empty() ) {
			out += "[]";
			return;
		}
		out += '[';
		for ( std::size_t i = 0; i < items.size(); ++i ) {
			if ( i ) out += ',';
			if ( pretty ) {
				out += '\n';
				Indent( out, depth + 1 );
			}
			Write( out, items[i], pretty, depth + 1 );
		}
		if ( pretty ) {
			out += '\n';
			Indent( out, depth );
		}
		out += ']';
		return;
	}
	case Value::Type::Object: {
		const auto &members = value.Members();
		if ( members.empty() ) {
			out += "{}";
			return;
		}
		out += '{';
		for ( std::size_t i = 0; i < members.size(); ++i ) {
			if ( i ) out += ',';
			if ( pretty ) {
				out += '\n';
				Indent( out, depth + 1 );
			}
			WriteString( out, members[i].first );
			out += pretty ? ": " : ":";
			Write( out, members[i].second, pretty, depth + 1 );
		}
		if ( pretty ) {
			out += '\n';
			Indent( out, depth );
		}
		out += '}';
		return;
	}
	}
}

} // namespace

Value Value::Boolean( bool value )
{
	Value result;
	result.type_ = Type::Bool;
	result.bool_ = value;
	return result;
}

Value Value::Number( double value )
{
	Value result;
	result.type_ = Type::Number;
	result.number_ = std::isfinite( value ) ? value : 0.0;
	return result;
}

Value Value::String( std::string value )
{
	Value result;
	result.type_ = Type::String;
	result.string_ = std::move( value );
	return result;
}

Value Value::Array()
{
	Value result;
	result.type_ = Type::Array;
	return result;
}

Value Value::Object()
{
	Value result;
	result.type_ = Type::Object;
	return result;
}

const std::string &Value::AsString() const
{
	return type_ == Type::String ? string_ : kEmptyString;
}

std::string Value::AsString( const char *fallback ) const
{
	return type_ == Type::String ? string_ : std::string( fallback ? fallback : "" );
}

const std::vector<Value> &Value::Items() const
{
	return type_ == Type::Array ? items_ : kEmptyItems;
}

void Value::Push( Value value )
{
	if ( type_ == Type::Null ) type_ = Type::Array;
	if ( type_ == Type::Array ) items_.push_back( std::move( value ) );
}

const std::vector<std::pair<std::string, Value>> &Value::Members() const
{
	return type_ == Type::Object ? members_ : kEmptyMembers;
}

const Value *Value::Find( const char *key ) const
{
	if ( type_ != Type::Object || !key ) return nullptr;
	for ( auto it = members_.rbegin(); it != members_.rend(); ++it ) {
		if ( it->first == key ) return &it->second;
	}
	return nullptr;
}

Value &Value::Set( const std::string &key, Value value )
{
	if ( type_ == Type::Null ) type_ = Type::Object;
	for ( auto &member : members_ ) {
		if ( member.first == key ) {
			member.second = std::move( value );
			return member.second;
		}
	}
	members_.emplace_back( key, std::move( value ) );
	return members_.back().second;
}

bool Parse( const std::string &text, Value &out, std::string *error )
{
	out = Value();
	Parser parser( text );
	Value parsed;
	if ( !parser.Document( parsed ) ) {
		if ( error ) *error = parser.Error();
		return false;
	}
	out = std::move( parsed );
	if ( error ) error->clear();
	return true;
}

std::string Serialize( const Value &value, bool pretty )
{
	std::string out;
	Write( out, value, pretty, 0 );
	if ( pretty ) out += '\n';
	return out;
}

} // namespace fntech3::launcher::json
