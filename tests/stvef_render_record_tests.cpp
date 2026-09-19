/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Elite Force render records: Holomatch's reference entity with its effect
 * union, single player's Quake III 1.17 entity, the view both modes share and
 * each mode's glconfig. Nothing here draws; these are the translations the
 * client applies at the module boundary. */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include "../code/compat/render_record_adapter.hpp"
#include "../code/client/client_stvef_records.hpp"
#include "../code/compat/stvef_draw_adapter.hpp"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

// The copier's one engine dependency, without linking the engine.
void Q_strncpyz( char *dest, const char *src, int destsize ) {
	int index = 0;
	while ( index + 1 < destsize && src[index] ) { dest[index] = src[index]; ++index; }
	dest[index] = '\0';
}

namespace {

int failures = 0;

#define CHECK( condition ) do { \
	if ( !( condition ) ) { \
		std::fprintf( stderr, "%s:%d: CHECK( %s ) failed\n", __FILE__, __LINE__, #condition ); \
		++failures; \
	} \
} while ( 0 )

using fntech3::render::ReadEntity;
using fntech3::render::ReadRefdef;

static_assert( sizeof( fntech3_stvef_mp_refEntity_t ) == 160, "Holomatch refEntity_t" );
static_assert( offsetof( fntech3_stvef_mp_refEntity_t, data ) == 132, "Holomatch effect union" );
static_assert( sizeof( fntech3_stvef_mp_refEntityData_t ) == 28, "Holomatch union size" );
static_assert( sizeof( fntech3_stvef_refdef_t ) == 112, "Holomatch refdef_t" );
static_assert( sizeof( fntech3_stvef_sp_refEntity_t ) == 152, "SP refEntity_t" );

void TestRecordSizes() {
	CHECK( fntech3::render::EntityBytes( COM_GAME_STVEF_MP ) == sizeof( fntech3_stvef_mp_refEntity_t ) );
	CHECK( fntech3::render::RefdefBytes( COM_GAME_STVEF_MP ) == sizeof( fntech3_stvef_refdef_t ) );
	// Single player's view is Holomatch's; its entity is its own.
	CHECK( fntech3::render::EntityBytes( COM_GAME_STVEF_SP ) == sizeof( fntech3_stvef_sp_refEntity_t ) );
	CHECK( fntech3::render::RefdefBytes( COM_GAME_STVEF_SP ) == sizeof( fntech3_stvef_refdef_t ) );
}

fntech3_stvef_mp_refEntity_t Filled() {
	fntech3_stvef_mp_refEntity_t source{};
	source.hModel = 83; source.shadowPlane = -17; source.frame = 19; source.oldframe = 41;
	source.backlerp = 0.25f; source.skinNum = 71; source.customSkin = 73; source.customShader = 79;
	for ( int i = 0; i < 3; ++i ) {
		source.lightingOrigin[i] = 10.0f + i; source.origin[i] = 20.0f + i; source.oldorigin[i] = 30.0f + i;
		for ( int j = 0; j < 3; ++j ) source.axis[i][j] = 50.0f + i * 3 + j;
	}
	for ( int i = 0; i < 4; ++i ) source.shaderRGBA[i] = static_cast<uint8_t>( 100 + i );
	source.shaderTexCoord[0] = 0.125f; source.shaderTexCoord[1] = 0.625f;
	source.shaderTime = -0.5f;
	return source;
}

void TestCommonFields() {
	fntech3_stvef_mp_refEntity_t source = Filled();
	source.nonNormalizedAxes = 1;
	refEntity_t entity;
	std::memset( &entity, 0xa5, sizeof( entity ) );
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.reType == RT_MODEL && entity.hModel == 83 && entity.nonNormalizedAxes == qtrue );
	CHECK( entity.shadowPlane == -17 && entity.frame == 19 && entity.oldframe == 41 && entity.backlerp == 0.25f );
	CHECK( entity.skinNum == 71 && entity.customSkin == 73 && entity.customShader == 79 );
	for ( int i = 0; i < 3; ++i ) {
		CHECK( entity.lightingOrigin[i] == 10.0f + i && entity.origin[i] == 20.0f + i );
		CHECK( entity.oldorigin[i] == 30.0f + i );
		for ( int j = 0; j < 3; ++j ) CHECK( entity.axis[i][j] == 50.0f + i * 3 + j );
	}
	for ( int i = 0; i < 4; ++i ) CHECK( entity.shader.rgba[i] == 100 + i );
	CHECK( entity.shaderTexCoord[0] == 0.125f && entity.shaderTexCoord[1] == 0.625f );
	CHECK( entity.shaderTime.f == -0.5f );
	// Nothing another title owns is carried over from the poisoned output.
	const rtcwRenderEntity_t rtcw{};
	CHECK( !std::memcmp( &entity.rtcw, &rtcw, sizeof( rtcw ) ) );
	const wolfetRenderEntity_t wolfet{};
	CHECK( !std::memcmp( &entity.wolfet, &wolfet, sizeof( wolfet ) ) );
	CHECK( !entity.renderfx && !entity.stvef.fullbright && !entity.stvef.darken && !entity.stvef.forceAlpha );
}

void TestTypes() {
	// Raven's ordinals: the three sprite-like types follow the sprite, and the
	// effect primitives follow the portal.
	const refEntityType_t expected[] = { RT_MODEL, RT_SPRITE, RT_STVEF_ORIENTEDSPRITE,
		RT_STVEF_ALPHAVERTPOLY, RT_BEAM, RT_RAIL_CORE, RT_RAIL_RINGS, RT_LIGHTNING,
		RT_PORTALSURFACE, RT_STVEF_LINE, RT_STVEF_ORIENTEDLINE, RT_STVEF_LINE2,
		RT_STVEF_BEZIER, RT_STVEF_CYLINDER, RT_STVEF_ELECTRICITY };
	fntech3_stvef_mp_refEntity_t source = Filled();
	for ( int type = 0; type < FNTECH3_STVEF_MP_RT_MAX; ++type ) {
		source.reType = type;
		refEntity_t entity{};
		CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
		CHECK( entity.reType == expected[type] );
	}
	// Retail's RE_AddRefEntityToScene refuses 15 and above; so does the client,
	// and a refused record leaves the output untouched.
	refEntity_t entity;
	std::memset( &entity, 0xb7, sizeof( entity ) );
	refEntity_t saved;
	std::memcpy( &saved, &entity, sizeof( entity ) );
	for ( int type : { -1, static_cast<int>( FNTECH3_STVEF_MP_RT_MAX ), INT_MAX, INT_MIN } ) {
		source.reType = type;
		CHECK( !ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
		CHECK( !std::memcmp( &entity, &saved, sizeof( entity ) ) );
	}
	source.reType = FNTECH3_STVEF_MP_RT_MODEL;
	for ( int boolean : { -1, 2, INT_MAX } ) {
		source.nonNormalizedAxes = boolean;
		CHECK( !ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
		CHECK( !std::memcmp( &entity, &saved, sizeof( entity ) ) );
	}
	CHECK( !ReadEntity( COM_GAME_STVEF_MP, nullptr, entity ) );
	CHECK( !std::memcmp( &entity, &saved, sizeof( entity ) ) );
}

void TestFlags() {
	const struct { int external, internal; } shared[] = {
		{ FNTECH3_STVEF_MP_RF_THIRD_PERSON, RF_THIRD_PERSON },
		{ FNTECH3_STVEF_MP_RF_FIRST_PERSON, RF_FIRST_PERSON },
		{ FNTECH3_STVEF_MP_RF_DEPTHHACK, RF_DEPTHHACK },
		{ FNTECH3_STVEF_MP_RF_NOSHADOW, RF_NOSHADOW },
		{ FNTECH3_STVEF_MP_RF_LIGHTING_ORIGIN, RF_LIGHTING_ORIGIN },
		{ FNTECH3_STVEF_MP_RF_SHADOW_PLANE, RF_SHADOW_PLANE },
		{ FNTECH3_STVEF_MP_RF_WRAP_FRAMES, RF_WRAP_FRAMES },
	};
	fntech3_stvef_mp_refEntity_t source = Filled();
	refEntity_t entity{};
	int all = 0;
	for ( const auto &flag : shared ) {
		source.renderfx = flag.external;
		CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
		CHECK( entity.renderfx == flag.internal );
		CHECK( !entity.stvef.fullbright && !entity.stvef.darken && !entity.stvef.forceAlpha );
		all |= flag.internal;
	}
	// 0x10 brightens and 0x1 darkens; with both, retail tests 0x10 first.
	source.renderfx = FNTECH3_STVEF_MP_RF_FULLBRIGHT;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( !entity.renderfx && entity.stvef.fullbright && !entity.stvef.darken );
	source.renderfx = FNTECH3_STVEF_MP_RF_MINLIGHT;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( !entity.renderfx && !entity.stvef.fullbright && entity.stvef.darken );
	source.renderfx = FNTECH3_STVEF_MP_RF_MINLIGHT | FNTECH3_STVEF_MP_RF_FULLBRIGHT;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.stvef.fullbright && !entity.stvef.darken );
	source.renderfx = FNTECH3_STVEF_MP_RF_FORCE_ENT_ALPHA;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( !entity.renderfx && entity.stvef.forceAlpha );
	// Every bit at once: the shared flags, and nothing for the bits retail's
	// renderer never reads.
	source.renderfx = -1;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.renderfx == all );
	CHECK( entity.stvef.fullbright && !entity.stvef.darken && entity.stvef.forceAlpha );
	source.renderfx = 0x20 | 0x400 | 0x1000 | INT_MIN;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( !entity.renderfx && !entity.stvef.fullbright && !entity.stvef.darken && !entity.stvef.forceAlpha );
}

void TestUnion() {
	fntech3_stvef_mp_refEntity_t source = Filled();
	refEntity_t entity{};

	// Every type reads the union's first two words as a sprite's rotation and
	// radius; retail's fog test reads the radius for every generated type.
	source.reType = FNTECH3_STVEF_MP_RT_SPRITE;
	source.data.sprite.rotation = 45.0f; source.data.sprite.radius = 12.5f;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.rotation == 45.0f && entity.radius == 12.5f );

	source.reType = FNTECH3_STVEF_MP_RT_ALPHAVERTPOLY;
	for ( int corner = 0; corner < 4; ++corner )
		for ( int channel = 0; channel < 4; ++channel )
			source.data.sprite.vertRGBA[corner][channel] = static_cast<uint8_t>( corner * 16 + channel + 1 );
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.reType == RT_STVEF_ALPHAVERTPOLY && entity.radius == 12.5f );
	for ( int corner = 0; corner < 4; ++corner )
		for ( int channel = 0; channel < 4; ++channel )
			CHECK( entity.stvef.vertexColors[corner].rgba[channel] == corner * 16 + channel + 1 );

	for ( int type : { FNTECH3_STVEF_MP_RT_LINE, FNTECH3_STVEF_MP_RT_ORIENTEDLINE, FNTECH3_STVEF_MP_RT_LINE2 } ) {
		source = Filled();
		source.reType = type;
		source.data.line.width = 6.4f; source.data.line.width2 = 2.5f; source.data.line.stscale = -48.0f;
		CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
		CHECK( entity.stvef.width == 6.4f && entity.stvef.width2 == 2.5f && entity.stvef.stscale == -48.0f );
		CHECK( entity.rotation == 6.4f && entity.radius == 2.5f );
	}

	source = Filled();
	source.reType = FNTECH3_STVEF_MP_RT_BEZIER;
	source.data.bezier.width = 4.0f;
	for ( int i = 0; i < 3; ++i ) {
		source.data.bezier.control1[i] = 100.0f + i;
		source.data.bezier.control2[i] = -200.0f - i;
	}
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.stvef.width == 4.0f );
	for ( int i = 0; i < 3; ++i )
		CHECK( entity.stvef.control1[i] == 100.0f + i && entity.stvef.control2[i] == -200.0f - i );

	source = Filled();
	source.reType = FNTECH3_STVEF_MP_RT_CYLINDER;
	source.data.cylinder.width = 10.0f; source.data.cylinder.width2 = 34.0f;
	source.data.cylinder.stscale = 1.0f; source.data.cylinder.height = 96.0f;
	source.data.cylinder.bias = 0.75f; source.data.cylinder.wrap = 7;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.stvef.width == 10.0f && entity.stvef.width2 == 34.0f && entity.stvef.stscale == 1.0f );
	CHECK( entity.stvef.height == 96.0f && entity.stvef.bias == 0.75f && entity.stvef.wrap == qtrue );
	source.data.cylinder.wrap = 0;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.stvef.wrap == qfalse );

	source = Filled();
	source.reType = FNTECH3_STVEF_MP_RT_ELECTRICITY;
	source.data.electricity.width = 3.0f; source.data.electricity.deviation = 0.3f;
	source.data.electricity.stscale = 2.0f; source.data.electricity.wrap = -1;
	source.data.electricity.taper = 1;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.stvef.width == 3.0f && entity.stvef.deviation == 0.3f && entity.stvef.stscale == 2.0f );
	CHECK( entity.stvef.wrap == qtrue && entity.stvef.taper == qtrue );

	// A model leaves every shape field clear whatever the union holds.
	source.reType = FNTECH3_STVEF_MP_RT_MODEL;
	CHECK( ReadEntity( COM_GAME_STVEF_MP, &source, entity ) );
	CHECK( entity.stvef.width == 0.0f && entity.stvef.deviation == 0.0f && !entity.stvef.wrap && !entity.stvef.taper );
}

fntech3_stvef_sp_refEntity_t FilledSP() {
	fntech3_stvef_sp_refEntity_t source{};
	source.hModel = 83; source.shadowPlane = -17; source.frame = 19; source.oldframe = 41;
	source.backlerp = 0.25f; source.skinNum = 71; source.customSkin = 73; source.customShader = 79;
	for ( int i = 0; i < 3; ++i ) {
		source.lightingOrigin[i] = 10.0f + i; source.origin[i] = 20.0f + i; source.oldorigin[i] = 30.0f + i;
		source.lightDir[i] = -1.0f - i;
		for ( int j = 0; j < 3; ++j ) source.axis[i][j] = 50.0f + i * 3 + j;
	}
	for ( int i = 0; i < 4; ++i ) source.shaderRGBA[i] = static_cast<uint8_t>( 100 + i );
	source.shaderTexCoord[0] = 0.125f; source.shaderTexCoord[1] = 0.625f;
	source.shaderTime = -0.5f;
	source.radius = 12.5f; source.rotation = 45.0f;
	return source;
}

void TestSPEntity() {
	fntech3_stvef_sp_refEntity_t source = FilledSP();
	source.nonNormalizedAxes = 1;
	refEntity_t entity;
	std::memset( &entity, 0xa5, sizeof( entity ) );
	CHECK( ReadEntity( COM_GAME_STVEF_SP, &source, entity ) );
	CHECK( entity.reType == RT_MODEL && entity.hModel == 83 && entity.nonNormalizedAxes == qtrue );
	CHECK( entity.shadowPlane == -17 && entity.frame == 19 && entity.oldframe == 41 && entity.backlerp == 0.25f );
	CHECK( entity.skinNum == 71 && entity.customSkin == 73 && entity.customShader == 79 );
	for ( int i = 0; i < 3; ++i ) {
		CHECK( entity.lightingOrigin[i] == 10.0f + i && entity.origin[i] == 20.0f + i );
		CHECK( entity.oldorigin[i] == 30.0f + i );
		for ( int j = 0; j < 3; ++j ) CHECK( entity.axis[i][j] == 50.0f + i * 3 + j );
	}
	for ( int i = 0; i < 4; ++i ) CHECK( entity.shader.rgba[i] == 100 + i );
	CHECK( entity.shaderTexCoord[0] == 0.125f && entity.shaderTexCoord[1] == 0.625f );
	CHECK( entity.shaderTime.f == -0.5f );
	// A sprite's radius and rotation have their own fields in single player.
	CHECK( entity.radius == 12.5f && entity.rotation == 45.0f );
	// Nothing of Holomatch's lighting or shapes, and nothing another title owns.
	CHECK( !entity.renderfx && !entity.stvef.fullbright && !entity.stvef.darken && !entity.stvef.forceAlpha );
	CHECK( entity.stvef.width == 0.0f && !entity.stvef.wrap && !entity.stvef.taper );
	const rtcwRenderEntity_t rtcw{};
	CHECK( !std::memcmp( &entity.rtcw, &rtcw, sizeof( rtcw ) ) );

	// Quake III's eight types, with Quake III's numbers.
	const refEntityType_t expected[] = { RT_MODEL, RT_POLY, RT_SPRITE, RT_BEAM, RT_RAIL_CORE,
		RT_RAIL_RINGS, RT_LIGHTNING, RT_PORTALSURFACE };
	static_assert( sizeof( expected ) / sizeof( expected[0] ) == FNTECH3_STVEF_SP_RT_MAX );
	for ( int type = 0; type < FNTECH3_STVEF_SP_RT_MAX; ++type ) {
		source.reType = type;
		CHECK( ReadEntity( COM_GAME_STVEF_SP, &source, entity ) );
		CHECK( entity.reType == expected[type] );
	}
	// Holomatch's extra types do not exist here; a refused record leaves the
	// output untouched.
	refEntity_t saved;
	std::memset( &entity, 0xb7, sizeof( entity ) );
	std::memcpy( &saved, &entity, sizeof( entity ) );
	for ( int type : { -1, static_cast<int>( FNTECH3_STVEF_SP_RT_MAX ), 14, INT_MAX, INT_MIN } ) {
		source.reType = type;
		CHECK( !ReadEntity( COM_GAME_STVEF_SP, &source, entity ) );
		CHECK( !std::memcmp( &entity, &saved, sizeof( entity ) ) );
	}
	source.reType = RT_MODEL;
	for ( int boolean : { -1, 2, INT_MAX } ) {
		source.nonNormalizedAxes = boolean;
		CHECK( !ReadEntity( COM_GAME_STVEF_SP, &source, entity ) );
		CHECK( !std::memcmp( &entity, &saved, sizeof( entity ) ) );
	}
	CHECK( !ReadEntity( COM_GAME_STVEF_SP, nullptr, entity ) );
}

void TestSPFlags() {
	// Quake III's flags keep their numbers, minimum light included: retail's
	// renderer lights every entity alike, so none of them darkens anything.
	// Holding a one-shot animation's last frame is 0x400, as in the engine.
	const int shared[] = { RF_MINLIGHT, RF_THIRD_PERSON, RF_FIRST_PERSON, RF_DEPTHHACK, RF_NOSHADOW,
		RF_LIGHTING_ORIGIN, RF_SHADOW_PLANE, RF_WRAP_FRAMES, RF_CAP_FRAMES };
	fntech3_stvef_sp_refEntity_t source = FilledSP();
	refEntity_t entity{};
	int all = 0;
	for ( int flag : shared ) {
		source.renderfx = flag;
		CHECK( ReadEntity( COM_GAME_STVEF_SP, &source, entity ) );
		CHECK( entity.renderfx == flag );
		CHECK( !entity.stvef.fullbright && !entity.stvef.darken && !entity.stvef.forceAlpha );
		all |= flag;
	}
	// Bits the engine gives other meanings (0x10 is its crosshair) stay clear.
	source.renderfx = -1;
	CHECK( ReadEntity( COM_GAME_STVEF_SP, &source, entity ) );
	CHECK( entity.renderfx == all && !( entity.renderfx & RF_CROSSHAIR ) );
	CHECK( !entity.stvef.fullbright && !entity.stvef.darken && !entity.stvef.forceAlpha );
}

bool Near( float value, float expected ) { return std::fabs( value - expected ) < 1e-4f; }

void TestSPRotatedPicture() {
	using fntech3::stvef::render::RotatedQuad;
	refDrawQuad_t quad;
	// Unturned: the rectangle, corners from the top-left clockwise.
	CHECK( RotatedQuad( 10, 20, 30, 40, 0.25f, 0.5f, 0.75f, 1.0f, 0, 7, quad ) );
	const float plain[4][2] = { { 10, 20 }, { 40, 20 }, { 40, 60 }, { 10, 60 } };
	for ( int i = 0; i < 4; ++i ) CHECK( Near( quad.xy[i][0], plain[i][0] ) && Near( quad.xy[i][1], plain[i][1] ) );
	CHECK( quad.st[0][0] == 0.25f && quad.st[0][1] == 0.5f && quad.st[2][0] == 0.75f && quad.st[2][1] == 1.0f );
	CHECK( quad.shader == 7 && !quad.colorMask );
	// A quarter turn swings the picture about its top-right corner, which stays.
	CHECK( RotatedQuad( 10, 20, 30, 40, 0, 0, 1, 1, 90, 7, quad ) );
	const float turned[4][2] = { { 40, -10 }, { 40, 20 }, { 0, 20 }, { 0, -10 } };
	for ( int i = 0; i < 4; ++i ) CHECK( Near( quad.xy[i][0], turned[i][0] ) && Near( quad.xy[i][1], turned[i][1] ) );
	// A non-finite value draws nothing and leaves the output alone.
	refDrawQuad_t saved = quad;
	CHECK( !RotatedQuad( 10, 20, 30, 40, 0, 0, 1, 1, NAN, 7, quad ) );
	CHECK( !RotatedQuad( INFINITY, 20, 30, 40, 0, 0, 1, 1, 0, 7, quad ) );
	CHECK( !std::memcmp( &quad, &saved, sizeof( quad ) ) );
}

void TestSPSoundChannels() {
	// The engine's channels keep their meaning, local ones included.
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_AUTO ) == CHAN_AUTO );
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_LOCAL ) == CHAN_LOCAL );
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_WEAPON ) == CHAN_WEAPON );
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_VOICE ) == CHAN_VOICE );
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_ITEM ) == CHAN_ITEM );
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_BODY ) == CHAN_BODY );
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_LOCAL_SOUND ) == CHAN_LOCAL_SOUND );
	CHECK( CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_ANNOUNCER ) == CHAN_ANNOUNCER );
	// Single player's own channels are distinct from each other and from the
	// engine's, so none plays as local or replaces another's sound.
	const int own[] = { FNTECH3_STVEF_SP_CHAN_VOICE_ATTEN, FNTECH3_STVEF_SP_CHAN_AMBIENT,
		FNTECH3_STVEF_SP_CHAN_MENU1, FNTECH3_STVEF_SP_CHAN_MENU2, 12, 40 };
	for ( int i = 0; i < 6; ++i ) {
		CHECK( CL_STVEFSPSoundChannel( own[i] ) > CHAN_ANNOUNCER );
		for ( int j = 0; j < i; ++j ) CHECK( CL_STVEFSPSoundChannel( own[i] ) != CL_STVEFSPSoundChannel( own[j] ) );
	}
}

void TestRefdef() {
	fntech3_stvef_refdef_t source{};
	source.x = -10; source.y = 6; source.width = 1024; source.height = 768;
	source.fov_x = 90.0f; source.fov_y = 73.74f; source.time = INT_MAX;
	for ( int i = 0; i < 3; ++i ) {
		source.vieworg[i] = 1.0f + i;
		for ( int j = 0; j < 3; ++j ) source.viewaxis[i][j] = 0.5f * ( i * 3 + j );
	}
	for ( int i = 0; i < FNTECH3_STVEF_MAX_MAP_AREA_BYTES; ++i ) source.areamask[i] = static_cast<uint8_t>( i * 7 + 1 );
	for ( int flags = 0; flags < 256; ++flags ) {
		source.rdflags = flags;
		refdef_t view;
		std::memset( &view, 0xa5, sizeof( view ) );
		CHECK( ReadRefdef( COM_GAME_STVEF_MP, &source, view ) );
		CHECK( view.x == -10 && view.y == 6 && view.width == 1024 && view.height == 768 );
		CHECK( view.fov_x == 90.0f && view.fov_y == 73.74f && view.time == INT_MAX );
		for ( int i = 0; i < 3; ++i ) {
			CHECK( view.vieworg[i] == 1.0f + i );
			for ( int j = 0; j < 3; ++j ) CHECK( view.viewaxis[i][j] == 0.5f * ( i * 3 + j ) );
		}
		CHECK( !std::memcmp( view.areamask, source.areamask, sizeof( source.areamask ) ) );
		// Only no-world and hyperspace carry meaning; the view has no text.
		CHECK( view.rdflags == ( ( flags & 1 ? RDF_NOWORLDMODEL : 0 ) | ( flags & 4 ? RDF_HYPERSPACE : 0 ) ) );
		CHECK( !view.text[0][0] );
		// Single player reads the same view.
		refdef_t spView;
		std::memset( &spView, 0x5a, sizeof( spView ) );
		CHECK( ReadRefdef( COM_GAME_STVEF_SP, &source, spView ) );
		CHECK( !std::memcmp( &spView, &view, sizeof( view ) ) );
	}
	refdef_t view{};
	CHECK( !ReadRefdef( COM_GAME_STVEF_MP, nullptr, view ) );
	CHECK( !ReadRefdef( COM_GAME_STVEF_SP, nullptr, view ) );
}

void TestGlconfig() {
	glconfig_t source{};
	Q_strncpyz( source.renderer_string, "fnTech3 renderer", sizeof( source.renderer_string ) );
	Q_strncpyz( source.vendor_string, "vendor", sizeof( source.vendor_string ) );
	Q_strncpyz( source.version_string, "4.6", sizeof( source.version_string ) );
	// Longer than Holomatch's 2 KB extension list: it is cut, and terminated.
	std::memset( source.extensions_string, 'x', sizeof( source.extensions_string ) - 1 );
	source.extensions_string[sizeof( source.extensions_string ) - 1] = '\0';
	source.maxTextureSize = 16384; source.numTextureUnits = 4;
	source.colorBits = 32; source.depthBits = 24; source.stencilBits = 8;
	source.driverType = GLDRV_ICD; source.hardwareType = GLHW_GENERIC;
	source.deviceSupportsGamma = qtrue; source.textureCompression = TC_S3TC;
	source.textureEnvAddAvailable = qtrue;
	source.vidWidth = 1024; source.vidHeight = 768; source.windowAspect = 4.0f / 3.0f;
	source.displayFrequency = 60; source.isFullscreen = qfalse; source.stereoEnabled = qfalse;
	source.smpActive = qfalse;

	struct { uint32_t before; fntech3_stvef_mp_glconfig_t config; uint32_t after; } guarded;
	std::memset( &guarded, 0xa5, sizeof( guarded ) );
	CL_CopySTVEFGlconfig( &guarded.config, source );
	CHECK( guarded.before == 0xa5a5a5a5u && guarded.after == 0xa5a5a5a5u );
	const auto &config = guarded.config;
	CHECK( !std::strcmp( config.renderer_string, "fnTech3 renderer" ) );
	CHECK( !std::strcmp( config.vendor_string, "vendor" ) && !std::strcmp( config.version_string, "4.6" ) );
	CHECK( std::strlen( config.extensions_string ) == sizeof( config.extensions_string ) - 1 );
	CHECK( config.maxTextureSize == 16384 && config.numTextureUnits == 4 );
	CHECK( config.colorBits == 32 && config.depthBits == 24 && config.stencilBits == 8 );
	CHECK( config.driverType == GLDRV_ICD && config.hardwareType == GLHW_GENERIC );
	CHECK( config.deviceSupportsGamma == 1 && config.textureCompression == TC_S3TC );
	CHECK( config.textureEnvAddAvailable == 1 && config.textureFilterAnisotropicAvailable == 1 );
	CHECK( config.vidWidth == 1024 && config.vidHeight == 768 && config.windowAspect == 4.0f / 3.0f );
	CHECK( config.displayFrequency == 60 && !config.isFullscreen && !config.stereoEnabled && !config.smpActive );

	// Single player's names its texture units maxActiveTextures and adds
	// clamp-to-edge after anisotropy.
	struct { uint32_t before; fntech3_stvef_sp_glconfig_t config; uint32_t after; } spGuarded;
	std::memset( &spGuarded, 0xa5, sizeof( spGuarded ) );
	CL_CopySTVEFSPGlconfig( &spGuarded.config, source );
	CHECK( spGuarded.before == 0xa5a5a5a5u && spGuarded.after == 0xa5a5a5a5u );
	const auto &sp = spGuarded.config;
	CHECK( !std::strcmp( sp.renderer_string, "fnTech3 renderer" ) && !std::strcmp( sp.version_string, "4.6" ) );
	CHECK( std::strlen( sp.extensions_string ) == sizeof( sp.extensions_string ) - 1 );
	CHECK( sp.maxTextureSize == 16384 && sp.maxActiveTextures == 4 );
	CHECK( sp.colorBits == 32 && sp.depthBits == 24 && sp.stencilBits == 8 );
	CHECK( sp.driverType == GLDRV_ICD && sp.hardwareType == GLHW_GENERIC );
	CHECK( sp.deviceSupportsGamma == 1 && sp.textureCompression == TC_S3TC && sp.textureEnvAddAvailable == 1 );
	CHECK( sp.textureFilterAnisotropicAvailable == 1 && sp.clampToEdgeAvailable == 1 );
	CHECK( sp.vidWidth == 1024 && sp.vidHeight == 768 && sp.windowAspect == 4.0f / 3.0f );
	CHECK( sp.displayFrequency == 60 && !sp.isFullscreen && !sp.stereoEnabled && !sp.smpActive );
}

} // namespace

int main() {
	TestRecordSizes();
	TestCommonFields();
	TestTypes();
	TestFlags();
	TestUnion();
	TestSPEntity();
	TestSPFlags();
	TestSPRotatedPicture();
	TestSPSoundChannels();
	TestRefdef();
	TestGlconfig();
	if ( failures ) {
		std::fprintf( stderr, "%d Elite Force render record check(s) failed\n", failures );
		return 1;
	}
	std::printf( "Elite Force render records: all checks passed\n" );
	return 0;
}
