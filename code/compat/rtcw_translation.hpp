/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_TRANSLATION_HPP
#define FNTECH3_RTCW_TRANSLATION_HPP

#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

// Independently implemented from original RTCW MP's translation dictionary
// and UI/cgame string contract; see docs/fntech3/2026-09-14-rtcw-contracts.md.
namespace fntech3::rtcw::translation {
inline constexpr std::size_t OutputBytes = 32000;
inline constexpr std::size_t TokenBytes = 1024;
inline constexpr std::size_t FileBytes = 4 * 1024 * 1024;
inline constexpr std::size_t MaxEntries = 16384;

inline char Lower( char value ) {
	return value >= 'A' && value <= 'Z' ? static_cast<char>( value + 'a' - 'A' ) : value;
}
inline std::string Key( std::string_view text ) {
	std::string key( text );
	for ( char &value : key ) value = Lower( value );
	return key;
}

// COM_Parse-style quoting preserves literal backslashes. Empty quoted strings
// are valid. Bounds, embedded NULs and unfinished comments/quotes fail closed.
class Reader {
public:
	explicit Reader( std::string_view source ) : source_( source ) {}
	bool Next( std::string &token ) {
		token.clear();
		for ( ;; ) {
			while ( position_ < source_.size() && static_cast<unsigned char>( source_[position_] ) <= 32 ) ++position_;
			if ( position_ == source_.size() ) return false;
			if ( source_.substr( position_, 2 ) == "//" ) {
				const auto end = source_.find( '\n', position_ + 2 );
				position_ = end == std::string_view::npos ? source_.size() : end + 1;
			} else if ( source_.substr( position_, 2 ) == "/*" ) {
				const auto end = source_.find( "*/", position_ + 2 );
				if ( end == std::string_view::npos ) { failed_ = true; return false; }
				position_ = end + 2;
			} else break;
		}
		const bool quoted = source_[position_] == '"';
		if ( quoted ) ++position_;
		const auto start = position_;
		while ( position_ < source_.size() && ( quoted ? source_[position_] != '"'
			: static_cast<unsigned char>( source_[position_] ) > 32 ) ) ++position_;
		if ( position_ - start >= TokenBytes || ( quoted && position_ == source_.size() ) ) {
			failed_ = true; return false;
		}
		token.assign( source_.substr( start, position_ - start ) );
		if ( quoted ) ++position_;
		return true;
	}
	bool Failed() const { return failed_; }
	std::size_t Position() const { return position_; }
private:
	std::string_view source_;
	std::size_t position_ = 0;
	bool failed_ = false;
};

// Retain the reference's exact formatting sequence requirement, and check the
// complete conversion so e.g. %02d cannot become %02f with a different ABI.
inline bool FormatSignature( std::string_view text, std::string &signature ) {
	signature.clear();
	for ( std::size_t i = 0; i < text.size(); ++i ) if ( text[i] == '%' ) {
		signature += i + 1 < text.size() ? text[i + 1] : '\0';
		signature += '|';
	}
	signature += ':';
	for ( std::size_t i = 0; i < text.size(); ++i ) {
		if ( text[i] != '%' ) continue;
		const auto start = i++;
		if ( i == text.size() ) { signature += "%|"; break; }
		if ( text[i] != '%' ) {
			while ( i < text.size() && std::string_view( "#0- +'" ).find( text[i] ) != std::string_view::npos ) ++i;
			if ( i < text.size() && text[i] == '*' ) ++i;
			while ( i < text.size() && text[i] >= '0' && text[i] <= '9' ) ++i;
			if ( i < text.size() && text[i] == '.' ) {
				++i;
				if ( i < text.size() && text[i] == '*' ) ++i;
				while ( i < text.size() && text[i] >= '0' && text[i] <= '9' ) ++i;
			}
			if ( i < text.size() && std::string_view( "hlLjzt" ).find( text[i] ) != std::string_view::npos ) {
				const auto length = text[i++];
				if ( i < text.size() && ( length == 'h' || length == 'l' ) && text[i] == length ) ++i;
			}
			if ( i < text.size() && ( text[i] == 'n' || text[i] == '$' ) ) return false;
		}
		if ( i >= text.size() ) return false;
		signature.append( text.substr( start, i - start + 1 ) );
		signature += '|';
	}
	return true;
}

struct Entry {
	std::array<std::string, 4> languages;
	float x = 0, y = 0;
};
struct LoadResult { bool accepted; std::size_t records, offset; };

class Catalog {
public:
	LoadResult Load( std::string_view text ) {
		if ( text.size() > FileBytes || text.find( '\0' ) != std::string_view::npos ) return { false, 0, 0 };
		Reader reader( text );
		auto staged = entries_;
		std::string token, original, signature, translatedSignature;
		std::size_t count = 0;
		auto reject = [&] { return LoadResult{ false, count, reader.Position() }; };
		while ( reader.Next( token ) ) {
			if ( Key( token ) == "#version" ) {
				if ( !reader.Next( token ) ) return reject();
				continue;
			}
			if ( token != "{" || !reader.Next( token ) || Key( token ) != "english"
				|| !reader.Next( original ) || !FormatSignature( original, signature ) ) return reject();
			Entry entry;
			const auto previous = staged.find( Key( original ) );
			if ( previous != staged.end() ) {
				entry.x = previous->second.x;
				entry.y = previous->second.y;
			}
			constexpr const char *names[] = { "french", "german", "italian", "spanish" };
			for ( std::size_t i = 0; i < entry.languages.size(); ++i ) {
				if ( !reader.Next( token ) || Key( token ) != names[i] || !reader.Next( entry.languages[i] ) ) return reject();
				if ( !entry.languages[i].empty() && ( !FormatSignature( entry.languages[i], translatedSignature )
					|| signature != translatedSignature ) ) return reject();
			}
			if ( !reader.Next( token ) ) return reject();
			if ( Key( token ) == "offset" ) {
				for ( float *axis : { &entry.x, &entry.y } ) {
					if ( !reader.Next( token ) || token.empty() ) return reject();
					char *end;
					*axis = std::strtof( token.c_str(), &end );
					if ( *end || !std::isfinite( *axis ) ) return reject();
				}
				if ( !reader.Next( token ) ) return reject();
			}
			if ( token != "}" ) return reject();
			staged[Key( original )] = std::move( entry );
			if ( staged.size() > MaxEntries ) return reject();
			++count;
		}
		if ( reader.Failed() ) return reject();
		entries_.swap( staged );
		return { true, count, reader.Position() };
	}
	const Entry *Find( std::string_view english ) const {
		const auto found = entries_.find( Key( english ) );
		return found == entries_.end() ? nullptr : &found->second;
	}
	std::string Translate( std::string_view original, int language, int debug ) const {
		if ( language < 1 || language > 4 || original.empty() ) return std::string( original );
		std::string lookup;
		for ( const char value : original ) if ( value != '\n' ) lookup += value;
		const auto *entry = Find( lookup );
		std::string result( original );
		const bool translated = entry && !entry->languages[language - 1].empty();
		if ( translated ) {
			result = entry->languages[language - 1];
		}
		if ( debug >= 1 ) {
			const bool newline = !translated && !result.empty() && result.back() == '\n';
			if ( newline ) result.pop_back();
			result = "^1[" + result + "]^7";
			if ( newline ) result += '\n';
		}
		if ( translated && original.back() == '\n' ) result += '\n';
		return result;
	}
	std::size_t Size() const { return entries_.size(); }
	void Clear() { entries_.clear(); }
private:
	std::unordered_map<std::string, Entry> entries_;
};
} // namespace fntech3::rtcw::translation
#endif
