/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Enemy Territory's client-boundary records, renderer transfers, key numbers
 * and 2D geometry. Nothing here draws or runs a client; these are the
 * translations that would cross the boundary if one ran. */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include "../code/compat/render_record_adapter.hpp"
#include "../code/compat/wolfet_client_adapter.hpp"
#include "../code/compat/wolfet_draw_adapter.hpp"
#include "../code/compat/wolfet_key_adapter.h"
#include "../code/compat/wolfet_state_adapter.hpp"
#include <cassert>
#include <cstring>

// Copy a terminated string into a fixed field without linking the engine.
template<std::size_t N>
static void Fill( char ( &field )[N], const char *text ) {
	std::size_t index = 0;
	while ( index + 1 < N && text[index] ) { field[index] = text[index]; ++index; }
	field[index] = '\0';
}

/* ---------------------------------------------------------------- records */
static_assert( sizeof( fntech3_wolfet_glyphInfo_t ) == 80, "ET glyph" );
static_assert( sizeof( fntech3_wolfet_fontInfo_t ) == 20548, "ET font" );
static_assert( offsetof( fntech3_wolfet_fontInfo_t, glyphScale ) == 20480, "ET font scale" );
static_assert( sizeof( fntech3_wolfet_polyBuffer_t ) == 53312, "ET polygon buffer" );
static_assert( offsetof( fntech3_wolfet_polyBuffer_t, numVerts ) == 28700, "ET buffer count" );
static_assert( sizeof( fntech3_wolfet_uiClientState_t ) == 3084, "ET client state" );
static_assert( offsetof( fntech3_wolfet_uiClientState_t, servername ) == 12, "ET server name" );
/* ET's reference entity is its own: a model handle beside every frame index. */
static_assert( sizeof( fntech3_wolfet_refEntity_t ) == 236, "ET reference entity" );
static_assert( offsetof( fntech3_wolfet_refEntity_t, frameModel ) == 120, "ET frame model" );
static_assert( offsetof( fntech3_wolfet_refEntity_t, oldTorsoFrameModel ) == 156, "ET old torso model" );

static void Records() {
	assert( fntech3::render::EntityBytes( COM_GAME_WOLFET ) == sizeof( fntech3_wolfet_refEntity_t ) );
	assert( fntech3::render::RefdefBytes( COM_GAME_WOLFET ) == sizeof( fntech3_wolfet_refdef_t ) );
	// No other profile may answer with ET's sizes, and ET may not answer with theirs.
	assert( fntech3::render::EntityBytes( COM_GAME_QUAKE3 ) != sizeof( fntech3_wolfet_refEntity_t ) );
	assert( fntech3::render::EntityBytes( COM_GAME_RTCW_MP ) != sizeof( fntech3_wolfet_refEntity_t ) );
	assert( fntech3::render::EntityBytes( COM_GAME_AUTO ) == 0 );
}

/* ------------------------------------------------------- reference entity */
static void Entity() {
	fntech3_wolfet_refEntity_t source{};
	refEntity_t entity{};

	// ET's scene types are its own order: a splash is fourth and a tapered rail
	// core sits between the two rail types.
	const struct { int external; refEntityType_t internal; } types[] = {
		{ FNTECH3_WOLFET_RT_MODEL, RT_MODEL }, { FNTECH3_WOLFET_RT_POLY, RT_POLY },
		{ FNTECH3_WOLFET_RT_SPRITE, RT_SPRITE }, { FNTECH3_WOLFET_RT_SPLASH, RT_SPLASH },
		{ FNTECH3_WOLFET_RT_BEAM, RT_BEAM }, { FNTECH3_WOLFET_RT_RAIL_CORE, RT_RAIL_CORE },
		{ FNTECH3_WOLFET_RT_RAIL_CORE_TAPER, RT_RAIL_CORE_TAPER },
		{ FNTECH3_WOLFET_RT_RAIL_RINGS, RT_RAIL_RINGS },
		{ FNTECH3_WOLFET_RT_LIGHTNING, RT_LIGHTNING },
		{ FNTECH3_WOLFET_RT_PORTALSURFACE, RT_PORTALSURFACE }
	};
	for ( const auto &type : types ) {
		source = {};
		source.reType = type.external;
		assert( fntech3::render::ReadEntity( COM_GAME_WOLFET, &source, entity ) );
		assert( entity.reType == type.internal );
	}
	source = {};
	source.reType = 10;
	assert( !fntech3::render::ReadEntity( COM_GAME_WOLFET, &source, entity ) );
	source.reType = -1;
	assert( !fntech3::render::ReadEntity( COM_GAME_WOLFET, &source, entity ) );
	source.reType = FNTECH3_WOLFET_RT_MODEL;
	source.nonNormalizedAxes = 2;
	assert( !fntech3::render::ReadEntity( COM_GAME_WOLFET, &source, entity ) );

	// Highlight, blink and forced detail are ET's own and never become an
	// engine render flag; unknown bits acquire no effect at all.
	source = {};
	source.renderfx = FNTECH3_WOLFET_RF_HILIGHT | FNTECH3_WOLFET_RF_BLINK |
		FNTECH3_WOLFET_RF_FORCENOLOD | 0x40000000;
	assert( fntech3::render::ReadEntity( COM_GAME_WOLFET, &source, entity ) );
	assert( entity.renderfx == 0 );
	assert( entity.wolfet.highlight && entity.wolfet.blink && entity.wolfet.forceNoLod );
	assert( !entity.rtcw.highlight && !entity.rtcw.blink );

	source = {};
	source.renderfx = FNTECH3_WOLFET_RF_THIRD_PERSON | FNTECH3_WOLFET_RF_WRAP_FRAMES;
	assert( fntech3::render::ReadEntity( COM_GAME_WOLFET, &source, entity ) );
	assert( entity.renderfx == ( RF_THIRD_PERSON | RF_WRAP_FRAMES ) );

	// Every frame index carries its own model handle across.
	source = {};
	source.frame = 11; source.frameModel = 12;
	source.torsoFrame = 13; source.torsoFrameModel = 14;
	source.oldframe = 15; source.oldframeModel = 16;
	source.oldTorsoFrame = 17; source.oldTorsoFrameModel = 18;
	source.torsoBacklerp = 0.5f;
	assert( fntech3::render::ReadEntity( COM_GAME_WOLFET, &source, entity ) );
	assert( entity.frame == 11 && entity.wolfet.frameModel == 12 );
	assert( entity.wolfet.torsoFrame == 13 && entity.wolfet.torsoFrameModel == 14 );
	assert( entity.oldframe == 15 && entity.wolfet.oldframeModel == 16 );
	assert( entity.wolfet.oldTorsoFrame == 17 && entity.wolfet.oldTorsoFrameModel == 18 );
	assert( entity.wolfet.torsoBacklerp == 0.5f );

	// A tag lookup reads the pose alone. Scene type, flags and material fields
	// of an otherwise uninitialized record must not reach the renderer.
	source = {};
	source.reType = FNTECH3_WOLFET_RT_PORTALSURFACE;
	source.renderfx = FNTECH3_WOLFET_RF_DEPTHHACK;
	source.customShader = 9;
	source.radius = 64.0f;
	source.frame = 3; source.frameModel = 4;
	entity = {};
	assert( fntech3::render::ReadWolfETTagEntity( &source, entity ) );
	assert( entity.reType == RT_MODEL && entity.renderfx == 0 );
	assert( entity.customShader == 0 && entity.radius == 0.0f );
	assert( entity.frame == 3 && entity.wolfet.frameModel == 4 );
}

/* ------------------------------------------------------------ scene view */
static void Refdef() {
	fntech3_wolfet_refdef_t source{};
	refdef_t view{};
	source.glfog.registered = 2;
	assert( !fntech3::render::ReadRefdef( COM_GAME_WOLFET, &source, view ) );
	source.glfog.registered = 1;
	source.glfog.drawsky = -1;
	assert( !fntech3::render::ReadRefdef( COM_GAME_WOLFET, &source, view ) );

	source = {};
	// Bit 8 is a skybox portal in ET, not Quake's no-first-person, and must not
	// become a shared scene flag.
	source.rdflags = FNTECH3_WOLFET_RDF_SKYBOXPORTAL | FNTECH3_WOLFET_RDF_UNDERWATER |
		FNTECH3_WOLFET_RDF_DRAWINGSKY;
	assert( fntech3::render::ReadRefdef( COM_GAME_WOLFET, &source, view ) );
	assert( view.rdflags == 0 );
	assert( view.wolfet.skyboxPortal && view.wolfet.underwater && view.wolfet.drawingSky );

	source = {};
	source.rdflags = FNTECH3_WOLFET_RDF_NOWORLDMODEL | FNTECH3_WOLFET_RDF_HYPERSPACE;
	source.glfog.mode = 3; source.glfog.start = 8.0f; source.glfog.end = 900.0f;
	source.glfog.density = 0.5f; source.glfog.useEndForClip = 1;
	assert( fntech3::render::ReadRefdef( COM_GAME_WOLFET, &source, view ) );
	assert( view.rdflags == ( RDF_NOWORLDMODEL | RDF_HYPERSPACE ) );
	assert( view.wolfet.fog.mode == 3 && view.wolfet.fog.start == 8.0f );
	assert( view.wolfet.fog.end == 900.0f && view.wolfet.fog.density == 0.5f );
	assert( view.wolfet.fog.useEndForClip == qtrue );
	// ET's fog never reaches the other Wolfenstein profile's view state.
	assert( view.rtcw.fog.mode == 0 && !view.rtcw.skyboxPortal );
}

/* ------------------------------------------- polygons and tag orientation */
static void Geometry() {
	fntech3_wolfet_polyVert_t vertex{};
	vertex.xyz[0] = 1.0f; vertex.xyz[1] = 2.0f; vertex.xyz[2] = 3.0f;
	vertex.st[0] = 0.25f; vertex.st[1] = 0.75f;
	vertex.modulate[0] = 9; vertex.modulate[3] = 255;
	polyVert_t output{};
	assert( fntech3::render::ReadWolfETPolyVert( &vertex, output ) );
	assert( output.xyz[2] == 3.0f && output.st[1] == 0.75f );
	assert( output.modulate.rgba[0] == 9 && output.modulate.rgba[3] == 255 );
	assert( !fntech3::render::ReadWolfETPolyVert( nullptr, output ) );

	orientation_t tag{};
	tag.origin[1] = 5.0f;
	tag.axis[2][2] = 1.0f;
	fntech3_wolfet_orientation_t external{};
	assert( fntech3::render::WriteWolfETOrientation( &external, tag ) );
	assert( external.origin[1] == 5.0f && external.axis[2][2] == 1.0f );
	assert( !fntech3::render::WriteWolfETOrientation( nullptr, tag ) );
}

/* -------------------------------------------------------- 2D submissions */
static void Draw() {
	using namespace fntech3::wolfet::render;
	refDrawQuad_t quad{};
	const float color[4] = { 0.0f, 0.5f, 1.0f, 1.0f };
	assert( GradientQuad( 10, 20, 100, 50, 0, 0, 1, 1, 7, color, 0, quad ) );
	// ET colours the lower two corners only.
	assert( quad.colorMask == 12 );
	assert( quad.colors[2].rgba[2] == 255 && quad.colors[3].rgba[0] == 0 );
	assert( quad.xy[0][0] == 10.0f && quad.xy[2][1] == 70.0f );
	assert( quad.shader == 7 );
	const float infinite[4] = { 0.0f, 0.0f, 0.0f, INFINITY };
	assert( !GradientQuad( 10, 20, 100, 50, 0, 0, 1, 1, 7, infinite, 0, quad ) );
	assert( !GradientQuad( NAN, 20, 100, 50, 0, 0, 1, 1, 7, color, 0, quad ) );
	// A null colour is white, as in the original.
	assert( GradientQuad( 0, 0, 8, 8, 0, 0, 1, 1, 1, nullptr, 0, quad ) );
	assert( quad.colors[2].rgba[0] == 255 && quad.colors[3].rgba[3] == 255 );

	// A quarter turn puts the first corner where the second was, on a circle of
	// half-diagonal radius about the rectangle's centre.
	assert( RotatedQuad( 0, 0, 100, 100, 0, 0, 1, 1, 2, 0.0f, quad ) );
	const float radius = 50.0f * 1.41421356f;
	assert( std::fabs( quad.xy[0][0] - ( 50.0f + radius ) ) < 0.01f );
	assert( std::fabs( quad.xy[0][1] - 50.0f ) < 0.01f );
	refDrawQuad_t turned{};
	assert( RotatedQuad( 0, 0, 100, 100, 0, 0, 1, 1, 2, 0.25f, turned ) );
	assert( std::fabs( turned.xy[0][0] - quad.xy[1][0] ) < 0.01f );
	assert( std::fabs( turned.xy[0][1] - quad.xy[1][1] ) < 0.01f );
	assert( !RotatedQuad( 0, 0, 100, 100, 0, 0, 1, 1, 2, NAN, quad ) );
}

/* -------------------------------------------------------------- key numbers */
static void Keys() {
	// ET's vocabulary ends at K_AUX16; this engine numbers four more keys above
	// it, and none of them may be reached from a module number.
	assert( FNTECH3_WOLFET_K_AUX16 == 232 );
	assert( FnTech3_WolfETKeyToInternal( FNTECH3_WOLFET_K_AUX16 ) == K_AUX16 );
	assert( FnTech3_WolfETKeyToInternal( FNTECH3_WOLFET_K_AUX16 + 1 ) == -1 );
	assert( FnTech3_WolfETKeyToInternal( -1 ) == -1 );
	for ( int key = 0; key < 128; ++key ) assert( FnTech3_WolfETKeyToInternal( key ) == key );

	// The mouse wheel and every joystick and auxiliary key sit four below this
	// engine's numbers. Passing one through unchanged would bind the wrong key.
	assert( FNTECH3_WOLFET_K_MWHEELDOWN == 183 && K_MWHEELDOWN == 187 );
	assert( FnTech3_WolfETKeyToInternal( FNTECH3_WOLFET_K_MWHEELDOWN ) == K_MWHEELDOWN );
	assert( FnTech3_WolfETKeyFromInternal( K_MWHEELDOWN ) == FNTECH3_WOLFET_K_MWHEELDOWN );
	assert( FnTech3_WolfETKeyFromInternal( K_JOY1 ) == FNTECH3_WOLFET_K_JOY1 );

	// Round trips must be exact in both directions for the whole vocabulary.
#define FNTECH3_WOLFET_KEY(name, value) \
	assert( FnTech3_WolfETKeyToInternal( value ) == name ); \
	assert( FnTech3_WolfETKeyFromInternal( name ) == value );
#include "../code/compat/wolfet_keycodes.def"
#undef FNTECH3_WOLFET_KEY

	// Character events are a separate contract and keep ET's own flag.
	assert( FnTech3_WolfETKeyEventFromInternal( K_CHAR_FLAG | 'a' ) ==
		( FNTECH3_WOLFET_K_CHAR_FLAG | 'a' ) );
	assert( FnTech3_WolfETKeyEventFromInternal( K_ESCAPE ) == FNTECH3_WOLFET_K_ESCAPE );
}

/* ------------------------------------------------- renderer configuration */
static void Configuration() {
	glconfig_t internal{};
	fntech3_wolfet_glconfig_t external{};
	internal.driverType = GLDRV_ICD;
	internal.hardwareType = GLHW_GENERIC;
	internal.textureCompression = TC_NONE;
	internal.vidWidth = 1920; internal.vidHeight = 1080;
	internal.numTextureUnits = 8;
	internal.windowAspect = 1.777f;
	internal.isFullscreen = qtrue;
	Fill( internal.renderer_string, "fnTech3" );
	assert( fntech3::wolfet::client::FromInternal( internal, external ) );
	assert( external.vidWidth == 1920 && external.maxActiveTextures == 8 );
	assert( external.isFullscreen == 1 && !std::strcmp( external.renderer_string, "fnTech3" ) );
	// The shared renderer reports no anisotropy and no vendor capabilities.
	// Those stay zero rather than being invented.
	assert( external.anisotropicAvailable == 0 && external.maxAnisotropy == 0.0f );
	assert( external.NVFogAvailable == 0 && external.ATIMaxTruformTess == 0 );

	// An out-of-range enumerated value is refused, never truncated.
	internal.driverType = static_cast<glDriverType_t>( 99 );
	assert( !fntech3::wolfet::client::FromInternal( internal, external ) );

	// An unterminated engine string still leaves a terminated module string.
	glconfig_t unterminated{};
	std::memset( unterminated.vendor_string, 'v', sizeof( unterminated.vendor_string ) );
	assert( fntech3::wolfet::client::FromInternal( unterminated, external ) );
	assert( std::strlen( external.vendor_string ) == sizeof( external.vendor_string ) - 1 );
}

/* ---------------------------------------------------- parser and client state */
static void ClientState() {
	pc_token_t token{};
	fntech3_wolfet_pc_token_t external{};
	token.type = TT_NUMBER;
	token.subtype = 4;
	token.intvalue = 7;
	token.floatvalue = 7.5f;
	Fill( token.string, "7.5" );
	assert( fntech3::wolfet::client::FromInternal( token, external ) );
	assert( external.type == FNTECH3_WOLFET_TT_NUMBER && external.intvalue == 7 );
	assert( !std::strcmp( external.string, "7.5" ) );
	// ET's token also names a line and a newline count; the shared parser
	// reports neither, so both stay zero.
	assert( external.line == 0 && external.linescrossed == 0 );
	token.type = 0;
	assert( !fntech3::wolfet::client::FromInternal( token, external ) );
	token.type = FNTECH3_WOLFET_TT_PUNCTUATION + 1;
	assert( !fntech3::wolfet::client::FromInternal( token, external ) );

	uiClientState_t state{};
	fntech3_wolfet_uiClientState_t reported{};
	state.connState = CA_ACTIVE;
	state.clientNum = 3;
	Fill( state.servername, "localhost" );
	assert( fntech3::wolfet::client::FromInternal( state, reported ) );
	assert( reported.connState == FNTECH3_WOLFET_CA_ACTIVE && reported.clientNum == 3 );
	assert( !std::strcmp( reported.servername, "localhost" ) );
	state.connState = static_cast<connstate_t>( FNTECH3_WOLFET_CA_CINEMATIC + 1 );
	assert( !fntech3::wolfet::client::FromInternal( state, reported ) );

	fontInfo_t font{};
	font.glyphScale = 0.5f;
	font.glyphs[65].height = 16;
	font.glyphs[65].s2 = 0.25f;
	Fill( font.glyphs[65].shaderName, "fonts/font" );
	Fill( font.name, "fonts/ariblk" );
	auto *externalFont = new fntech3_wolfet_fontInfo_t{};
	assert( fntech3::wolfet::client::FromInternal( font, *externalFont ) );
	assert( externalFont->glyphScale == 0.5f && externalFont->glyphs[65].height == 16 );
	assert( externalFont->glyphs[65].s2 == 0.25f );
	assert( !std::strcmp( externalFont->glyphs[65].shaderName, "fonts/font" ) );
	assert( !std::strcmp( externalFont->name, "fonts/ariblk" ) );
	delete externalFont;
}

/* ------------------------------------------------------------- gamestate */
static void GameState() {
	auto *internal = new gameState_t{};
	auto *external = new fntech3_wolfet_gameState_t{};

	// A gamestate is published only when every offset addresses its own data.
	assert( !fntech3::wolfet::state::FromInternal( *internal, *external ) );
	internal->dataCount = 8;
	std::memcpy( internal->stringData, "\0abc\0de", 8 );
	internal->stringOffsets[2] = 1;
	assert( fntech3::wolfet::state::FromInternal( *internal, *external ) );
	assert( external->dataCount == 8 && external->stringOffsets[2] == 1 );
	assert( !std::strcmp( external->stringData + 1, "abc" ) );

	internal->stringOffsets[3] = internal->dataCount;
	assert( !fntech3::wolfet::state::FromInternal( *internal, *external ) );
	internal->stringOffsets[3] = -1;
	assert( !fntech3::wolfet::state::FromInternal( *internal, *external ) );
	internal->stringOffsets[3] = 0;

	// ET keeps 16,000 bytes where this engine reserves more; a larger count is
	// refused rather than truncated into ET's record.
	internal->dataCount = FNTECH3_WOLFET_MAX_GAMESTATE_CHARS + 1;
	assert( !fntech3::wolfet::state::FromInternal( *internal, *external ) );
	delete internal;
	delete external;
}

int main() {
	Records();
	Entity();
	Refdef();
	Geometry();
	Draw();
	Keys();
	Configuration();
	ClientState();
	GameState();
	return 0;
}
