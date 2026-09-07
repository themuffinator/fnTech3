#include "../code/qcommon/cm_game_contract.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

int failures;

void Check( bool condition, const char *expression, int line )
{
	if ( condition ) {
		return;
	}
	std::cerr << "line " << line << ": check failed: " << expression << '\n';
	++failures;
}

#define CHECK(expression) Check( ( expression ), #expression, __LINE__ )

bool NearlyEqual( float value, float expected )
{
	return std::fabs( value - expected ) < 0.0001f;
}

void TestVerticalCylinderRadiusIsProfileOwned()
{
	const float radius = 30.0f;
	const float nan = std::numeric_limits<float>::quiet_NaN();

	CHECK( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKE3, radius, 1.1f ) == radius );
	CHECK( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKE3, radius, 16.0f ) == radius );
	CHECK( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKE3, radius, nan ) == radius );
	CHECK( CM_VerticalCylinderRadiusForGame(
		COM_GAME_AUTO, radius, 1.1f ) == radius );

	CHECK( NearlyEqual( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKELIVE, radius, 1.1f ), 33.0f ) );
	CHECK( NearlyEqual( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKELIVE, radius, 1.25f ), 37.5f ) );
	CHECK( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKELIVE, radius, 0.0f ) == 0.0f );
	CHECK( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKELIVE, radius, 16.0f ) == 480.0f );

	/* FnQL falls back to 1.1 for every non-finite/out-of-range value. */
	CHECK( NearlyEqual( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKELIVE, radius, -0.01f ), 33.0f ) );
	CHECK( NearlyEqual( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKELIVE, radius, 16.01f ), 33.0f ) );
	CHECK( NearlyEqual( CM_VerticalCylinderRadiusForGame(
		COM_GAME_QUAKELIVE, radius, nan ), 33.0f ) );
}

void TestEntityClassnameIsProfileOwned()
{
	static const char source[] =
		"{\n"
		"\"classname\" \"advertisement\"\n"
		"\"targetname\" \"advertisement\"\n"
		"}\n"
		"{\n"
		"\"CLASSNAME\" \"ADVERTISEMENT\"\n"
		"}\n";
	static const char quake3Expected[] =
		"{\n"
		"\"classname\" \"func_static\"\n"
		"\"targetname\" \"advertisement\"\n"
		"}\n"
		"{\n"
		"\"CLASSNAME\" \"func_static\"\n"
		"}\n";
	char output[sizeof( source )];
	int outputLength;

	outputLength = CM_CopyEntityStringForGame( COM_GAME_QUAKE3,
		source, (int)sizeof( source ) - 1, output );
	CHECK( outputLength == (int)sizeof( quake3Expected ) - 1 );
	CHECK( !std::strcmp( output, quake3Expected ) );

	outputLength = CM_CopyEntityStringForGame( COM_GAME_QUAKELIVE,
		source, (int)sizeof( source ) - 1, output );
	CHECK( outputLength == (int)sizeof( source ) - 1 );
	CHECK( !std::memcmp( output, source, sizeof( source ) ) );

	/* An inactive profile preserves data; startup owns rejecting that state. */
	outputLength = CM_CopyEntityStringForGame( COM_GAME_AUTO,
		source, (int)sizeof( source ) - 1, output );
	CHECK( outputLength == (int)sizeof( source ) - 1 );
	CHECK( !std::memcmp( output, source, sizeof( source ) ) );
}

void TestBaselineEmbeddedNulBehavior()
{
	const char source[] = { 'x', '\0', 'y' };
	char quake3Output[sizeof( source ) + 1] = {};
	char quakeLiveOutput[sizeof( source ) + 1] = {};

	CHECK( CM_CopyEntityStringForGame( COM_GAME_QUAKE3,
		source, (int)sizeof( source ), quake3Output ) == 1 );
	CHECK( quake3Output[0] == 'x' && quake3Output[1] == '\0' );

	CHECK( CM_CopyEntityStringForGame( COM_GAME_QUAKELIVE,
		source, (int)sizeof( source ), quakeLiveOutput )
		== (int)sizeof( source ) );
	CHECK( !std::memcmp( source, quakeLiveOutput, sizeof( source ) ) );
	CHECK( quakeLiveOutput[sizeof( source )] == '\0' );
}

} // namespace

int main()
{
	TestVerticalCylinderRadiusIsProfileOwned();
	TestEntityClassnameIsProfileOwned();
	TestBaselineEmbeddedNulBehavior();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
