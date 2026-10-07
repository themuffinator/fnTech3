/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_vdf.hpp"

#include "launcher_fs.hpp"

namespace fntech3::launcher::vdf {

namespace {

const std::string kEmpty;

enum class Token { End, Error, String, Open, Close };

class Lexer {
public:
	explicit Lexer( const std::string &text ) : text_( text ) {}

	Token Next( std::string &value )
	{
		value.clear();
		for ( ;; ) {
			while ( pos_ < text_.size() && static_cast<unsigned char>( text_[pos_] ) <= ' ' ) ++pos_;
			if ( pos_ + 1 < text_.size() && text_[pos_] == '/' && text_[pos_ + 1] == '/' ) {
				while ( pos_ < text_.size() && text_[pos_] != '\n' ) ++pos_;
				continue;
			}
			break;
		}
		if ( pos_ >= text_.size() ) return Token::End;
		const char c = text_[pos_];
		if ( c == '{' ) {
			++pos_;
			return Token::Open;
		}
		if ( c == '}' ) {
			++pos_;
			return Token::Close;
		}
		if ( c == '"' ) {
			++pos_;
			while ( pos_ < text_.size() && text_[pos_] != '"' ) {
				char ch = text_[pos_++];
				if ( ch == '\\' && pos_ < text_.size() ) {
					const char escaped = text_[pos_++];
					switch ( escaped ) {
					case 'n': ch = '\n'; break;
					case 't': ch = '\t'; break;
					case '\\': ch = '\\'; break;
					case '"': ch = '"'; break;
					default:
						// Steam writes Windows paths with doubled backslashes; an
						// unknown escape keeps both characters.
						value += '\\';
						ch = escaped;
						break;
					}
				}
				if ( value.size() >= kMaxTokenBytes ) return Token::Error;
				value += ch;
			}
			if ( pos_ >= text_.size() ) return Token::Error;
			++pos_;
			return Token::String;
		}
		while ( pos_ < text_.size() ) {
			const char ch = text_[pos_];
			if ( static_cast<unsigned char>( ch ) <= ' ' || ch == '{' || ch == '}' || ch == '"' ) break;
			if ( value.size() >= kMaxTokenBytes ) return Token::Error;
			value += ch;
			++pos_;
		}
		return Token::String;
	}

	// A platform conditional such as [$WIN32] after a value; it never names a
	// key, so it is skipped wherever one may follow.
	static bool IsConditional( const std::string &token )
	{
		return token.size() >= 2 && token.front() == '[' && token.back() == ']';
	}

private:
	const std::string &text_;
	std::size_t pos_ = 0;
};

bool ParseBlock( Lexer &lexer, std::vector<Node> &entries, int depth, bool topLevel )
{
	if ( depth > kMaxDepth ) return false;
	std::string token;
	for ( ;; ) {
		Token type = lexer.Next( token );
		if ( type == Token::String && Lexer::IsConditional( token ) ) type = lexer.Next( token );
		if ( type == Token::End ) return topLevel;
		if ( type == Token::Close ) return !topLevel;
		if ( type != Token::String ) return false;
		Node node;
		node.key = token;
		type = lexer.Next( token );
		if ( type == Token::String ) {
			node.value = token;
		} else if ( type == Token::Open ) {
			node.block = true;
			if ( !ParseBlock( lexer, node.children, depth + 1, false ) ) return false;
		} else {
			return false;
		}
		entries.push_back( std::move( node ) );
	}
}

} // namespace

const Node *Node::Find( const char *name ) const
{
	if ( !name ) return nullptr;
	for ( const Node &child : children ) {
		if ( fs::EqualsNoCase( child.key, name ) ) return &child;
	}
	return nullptr;
}

const std::string &Node::Get( const char *name ) const
{
	const Node *child = Find( name );
	return child && !child->block ? child->value : kEmpty;
}

bool Parse( const std::string &text, Node &root )
{
	root = Node();
	root.block = true;
	if ( text.size() > kMaxFileBytes ) return false;
	Lexer lexer( text );
	if ( !ParseBlock( lexer, root.children, 0, true ) ) {
		root = Node();
		return false;
	}
	return true;
}

bool ParseFile( const std::string &utf8Path, Node &root )
{
	std::string text;
	if ( !fs::ReadFile( fs::FromUtf8( utf8Path ), text, kMaxFileBytes ) ) {
		root = Node();
		return false;
	}
	return Parse( text, root );
}

} // namespace fntech3::launcher::vdf
