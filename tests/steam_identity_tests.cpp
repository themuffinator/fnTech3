#include <cstring>
#include <iostream>

#include "../code/qcommon/steam_identity.hpp"

namespace {

int failures;

void Check( bool condition, const char* expression, int line )
{
	if ( condition ) {
		return;
	}
	std::cerr << "line " << line << ": check failed: " << expression << '\n';
	++failures;
}

#define CHECK(expression) Check( ( expression ), #expression, __LINE__ )

// The engine bound the published name shares with cl->name.
constexpr std::size_t kNameCapacity = 32;

void TestPersonaPassesThroughUnchanged()
{
	using fnql::identity::NormalizePersonaName;

	char name[kNameCapacity];
	auto result = NormalizePersonaName( "themuffinator", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( !result.adjusted );
	CHECK( result.length == 13 );
	CHECK( std::strcmp( name, "themuffinator" ) == 0 );

	// Retail strips color escapes at render time, not at the identity source,
	// so an interior escape survives normalization.
	result = NormalizePersonaName( "^1red^7name", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( !result.adjusted );
	CHECK( std::strcmp( name, "^1red^7name" ) == 0 );

	// Printable punctuation that infostrings accept is not identity-hostile.
	result = NormalizePersonaName( "[FnQL] p!ayer_1-%", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( !result.adjusted );
	CHECK( std::strcmp( name, "[FnQL] p!ayer_1-%" ) == 0 );
}

void TestInfostringHostileBytesAreRemoved()
{
	using fnql::identity::NormalizePersonaName;

	char name[kNameCapacity];

	// Info_ValidateKeyValue() rejects these, which would drop the whole "name"
	// key out of userinfo rather than publish a broken value.
	auto result = NormalizePersonaName( "ba\\ck\"quote;semi", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( result.adjusted );
	CHECK( std::strcmp( name, "backquotesemi" ) == 0 );

	result = NormalizePersonaName( "tab\there\nnewline\x7f", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( result.adjusted );
	CHECK( std::strcmp( name, "tabherenewline" ) == 0 );
}

void TestWhitespaceIsTrimmed()
{
	using fnql::identity::NormalizePersonaName;

	char name[kNameCapacity];
	auto result = NormalizePersonaName( "   spaced out   ", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( result.adjusted );
	CHECK( std::strcmp( name, "spaced out" ) == 0 );

	// Interior spacing is part of the persona and is preserved.
	result = NormalizePersonaName( "two  inner  gaps", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( !result.adjusted );
	CHECK( std::strcmp( name, "two  inner  gaps" ) == 0 );
}

void TestUnusablePersonaFallsBack()
{
	using fnql::identity::NormalizePersonaName;

	char name[kNameCapacity];
	auto result = NormalizePersonaName( "", name, sizeof( name ) );
	CHECK( !result.usable );
	CHECK( name[0] == '\0' );

	result = NormalizePersonaName( "     ", name, sizeof( name ) );
	CHECK( !result.usable );
	CHECK( name[0] == '\0' );

	result = NormalizePersonaName( ";;;\\\"", name, sizeof( name ) );
	CHECK( !result.usable );
	CHECK( name[0] == '\0' );

	result = NormalizePersonaName( nullptr, name, sizeof( name ) );
	CHECK( !result.usable );
	CHECK( name[0] == '\0' );

	// A degenerate buffer must not be written past.
	char single[1] = { 'x' };
	result = NormalizePersonaName( "player", single, sizeof( single ) );
	CHECK( !result.usable );
	CHECK( single[0] == '\0' );
	CHECK( !NormalizePersonaName( "player", nullptr, 0 ).usable );
}

void TestTruncationRespectsTheEngineBound()
{
	using fnql::identity::NormalizePersonaName;

	char name[kNameCapacity];
	auto result = NormalizePersonaName(
		"0123456789012345678901234567890123456789", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( result.adjusted );
	CHECK( result.length == kNameCapacity - 1 );
	CHECK( std::strcmp( name, "0123456789012345678901234567890" ) == 0 );

	// Exactly filling the buffer is not a truncation.
	result = NormalizePersonaName( "0123456789012345678901234567890", name,
		sizeof( name ) );
	CHECK( result.usable );
	CHECK( !result.adjusted );
	CHECK( result.length == kNameCapacity - 1 );

	// Truncation must not expose trailing whitespace.
	result = NormalizePersonaName( "012345678901234567890123456789 x", name,
		sizeof( name ) );
	CHECK( result.usable );
	CHECK( result.adjusted );
	CHECK( std::strcmp( name, "012345678901234567890123456789" ) == 0 );
}

void TestTruncationNeverSplitsUtf8()
{
	using fnql::identity::NormalizePersonaName;
	using fnql::identity::Utf8SequenceLength;

	CHECK( Utf8SequenceLength( 'a' ) == 1 );
	CHECK( Utf8SequenceLength( 0xc3u ) == 2 );
	CHECK( Utf8SequenceLength( 0xe3u ) == 3 );
	CHECK( Utf8SequenceLength( 0xf0u ) == 4 );
	CHECK( Utf8SequenceLength( 0x80u ) == 0 );
	CHECK( Utf8SequenceLength( 0xffu ) == 0 );

	char name[kNameCapacity];

	// A complete multi-byte persona is published byte for byte.
	auto result = NormalizePersonaName( "caf\xc3\xa9", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( !result.adjusted );
	CHECK( std::strcmp( name, "caf\xc3\xa9" ) == 0 );

	// 30 ASCII bytes plus a two-byte sequence leaves room for only its lead
	// byte, so the whole sequence is dropped instead of published half written.
	result = NormalizePersonaName( "012345678901234567890123456789\xc3\xa9",
		name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( result.adjusted );
	CHECK( std::strcmp( name, "012345678901234567890123456789" ) == 0 );

	// A four-byte sequence that only partly fits is dropped whole.
	result = NormalizePersonaName( "01234567890123456789012345678\xf0\x9f\x98\x80",
		name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( std::strcmp( name, "01234567890123456789012345678" ) == 0 );

	// A persona that is nothing but continuation bytes is not usable.
	result = NormalizePersonaName( "\x80\x81", name, sizeof( name ) );
	CHECK( !result.usable );
}

void TestTrailingColorEscapeIsRemoved()
{
	using fnql::identity::NormalizePersonaName;
	using fnql::identity::TrimTrailingColorEscape;

	CHECK( TrimTrailingColorEscape( "name", 4 ) == 4 );
	CHECK( TrimTrailingColorEscape( "name^", 5 ) == 4 );
	CHECK( TrimTrailingColorEscape( "name^^", 6 ) == 4 );
	CHECK( TrimTrailingColorEscape( nullptr, 3 ) == 0 );

	char name[kNameCapacity];

	// A rendered name is always concatenated with surrounding text, where a
	// trailing caret would consume the next character.
	auto result = NormalizePersonaName( "player^", name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( result.adjusted );
	CHECK( std::strcmp( name, "player" ) == 0 );

	// Truncation must not manufacture one either.
	result = NormalizePersonaName( "0123456789012345678901234567890^1tail",
		name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( std::strcmp( name, "0123456789012345678901234567890" ) == 0 );

	result = NormalizePersonaName( "012345678901234567890123456789^1tail",
		name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( std::strcmp( name, "012345678901234567890123456789" ) == 0 );

	result = NormalizePersonaName( "^^^", name, sizeof( name ) );
	CHECK( !result.usable );
}

void TestCountryCodeNormalization()
{
	using fnql::identity::NormalizeCountryCode;

	char country[3];
	CHECK( NormalizeCountryCode( "GB", country, sizeof( country ) ) );
	CHECK( std::strcmp( country, "GB" ) == 0 );

	CHECK( NormalizeCountryCode( "us", country, sizeof( country ) ) );
	CHECK( std::strcmp( country, "US" ) == 0 );

	// Anything that is not exactly two letters is not a country code.
	CHECK( !NormalizeCountryCode( "", country, sizeof( country ) ) );
	CHECK( country[0] == '\0' );
	CHECK( !NormalizeCountryCode( "G", country, sizeof( country ) ) );
	CHECK( country[0] == '\0' );
	CHECK( !NormalizeCountryCode( "GBR", country, sizeof( country ) ) );
	CHECK( country[0] == '\0' );
	CHECK( !NormalizeCountryCode( "G1", country, sizeof( country ) ) );
	CHECK( !NormalizeCountryCode( "\\\"", country, sizeof( country ) ) );
	CHECK( !NormalizeCountryCode( nullptr, country, sizeof( country ) ) );

	char tooSmall[2] = { 'x', 'y' };
	CHECK( !NormalizeCountryCode( "GB", tooSmall, sizeof( tooSmall ) ) );
	CHECK( tooSmall[0] == '\0' );
	CHECK( !NormalizeCountryCode( "GB", nullptr, 0 ) );
}

void TestRetailDefaultIsTheFallbackName()
{
	using fnql::identity::NormalizePersonaName;

	// The retail executable registers "name" with this default, so a persona
	// that normalizes away publishes the retail value rather than a local one.
	CHECK( std::strcmp( fnql::identity::kUnnamedPlayer,
		"UnnamedPlayer" ) == 0 );

	char name[kNameCapacity];
	const auto result = NormalizePersonaName(
		fnql::identity::kUnnamedPlayer, name, sizeof( name ) );
	CHECK( result.usable );
	CHECK( !result.adjusted );
	CHECK( std::strcmp( name, "UnnamedPlayer" ) == 0 );
}

} // namespace

int main()
{
	TestPersonaPassesThroughUnchanged();
	TestInfostringHostileBytesAreRemoved();
	TestWhitespaceIsTrimmed();
	TestUnusablePersonaFallsBack();
	TestTruncationRespectsTheEngineBound();
	TestTruncationNeverSplitsUtf8();
	TestTrailingColorEscapeIsRemoved();
	TestCountryCodeNormalization();
	TestRetailDefaultIsTheFallbackName();
	return failures == 0 ? 0 : 1;
}
