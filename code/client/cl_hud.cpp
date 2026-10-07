/*
 * HUD rules, layout and draw-group diagnostics adapted from FnQuake3
 * at 7d8c9629cdc7979f09697266470359101ffd9eda (GPL-2.0-or-later).
 * Copyright (C) FnQuake3 contributors; fnTech3 adaptations (C) 2026 contributors.
 * See CREDITS.md and THIRD_PARTY_NOTICES.md.
 */
extern "C" {
#include "client.h"

#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define JSON_IMPLEMENTATION
#include "../qcommon/json.h"
#undef JSON_IMPLEMENTATION
#if defined( __clang__ )
#pragma clang diagnostic pop
#elif defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif
}

#include "client_cpp.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>

using fnql::FileWrite;
using fnql::ScopedFileHandle;
using fnql::ScopedReadFile;

namespace {

constexpr char kHudScriptFile[] = "fntech3-hud.json";
constexpr char kLegacyHudScriptFile[] = "fnq3-hud.json";
constexpr char kHudDumpFile[] = "fntech3-hud-dump.json";
constexpr int kHudMaxScriptBytes = 256 * 1024;

// Selected once at client startup for every game whose HUD assumes a 4:3
// screen. QL's and MoHAA's modules place their own HUDs for any screen, even
// if a config happens to contain these cvars or a legacy HUD script. Alice's
// and F.A.K.K.2's HUDs are drawn by the engine, which routes them through
// here.
bool hudSupported = false;
cvar_t *cl_hudAspect = nullptr;
cvar_t *cl_hudDump = nullptr;

constexpr int kHudMaxRules = 256;
constexpr int kHudMaxShaderNames = 1024;
constexpr int kHudMaxFrameDraws = 4096;
constexpr int kHudMaxFrameGroups = 512;
constexpr int kHudMaxDumpGroups = 1024;

enum class HudTransformMode {
	Stretch,
	Uniform,
	// Uniform, then each edge that touches the 640x480 frame reaches the
	// screen's edge: a mask beside a centred picture, such as a scope's bars.
	Extend
};

enum class HudAlignX {
	Left,
	Center,
	Right
};

enum class HudAlignY {
	Top,
	Middle,
	Bottom
};

// The point of a draw (or of its text line) that a rule's region must hold.
enum class HudMatchEdge {
	Left,
	Center,
	Right
};

struct HudShaderName {
	qhandle_t handle = 0;
	std::array<char, MAX_QPATH> name{};
};

struct HudRule {
	std::array<char, 64> name{};
	bool hasShader = false;
	std::array<char, MAX_QPATH> shader{};
	bool hasTextLike = false;
	bool textLike = false;
	bool hasRefdef = false;
	bool refdef = false;
	bool hasRegion = false;
	float regionX = 0.0f;
	float regionY = 0.0f;
	float regionW = 0.0f;
	float regionH = 0.0f;
	HudMatchEdge edge = HudMatchEdge::Center;
	bool hasMinHeight = false;
	float minHeight = 0.0f;
	bool hasMaxHeight = false;
	float maxHeight = 0.0f;
	// A screen of the game's own begins with this draw: the rules pause for
	// the rest of the frame, as for HUD_PANEL_CENTERED.
	bool panel = false;
	bool hasAlignX = false;
	HudAlignX alignX = HudAlignX::Center;
	bool hasAlignY = false;
	HudAlignY alignY = HudAlignY::Middle;
	HudTransformMode mode = HudTransformMode::Uniform;
};

struct HudDrawCapture {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float s1 = 0.0f;
	float t1 = 0.0f;
	float s2 = 0.0f;
	float t2 = 0.0f;
	qhandle_t shader = 0;
	std::array<char, MAX_QPATH> shaderName{};
	// The module's own name for the shader, when shaderName is the client's
	// alias for it. Points into hudShaderNames; valid for this draw only.
	const char *moduleShaderName = "";
	bool textLike = false;
	bool refdef = false;
	// The line of text this glyph belongs to, which the rules test in its
	// place when they place text by line ("textRuns").
	bool inLine = false;
	float lineX1 = 0.0f;
	float lineY1 = 0.0f;
	float lineX2 = 0.0f;
	float lineY2 = 0.0f;
};

// A client game's picture or colour change held back until the line of text
// it belongs to is complete (CL_HudQueueStretchPic).
struct HudQueuedOp {
	bool color = false;
	bool colorSet = false;
	std::array<float, 4> rgba{};
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float s1 = 0.0f;
	float t1 = 0.0f;
	float s2 = 0.0f;
	float t2 = 0.0f;
	qhandle_t shader = 0;
	HudDrawCapture draw;
};

struct HudDumpGroup {
	float x1 = 0.0f;
	float y1 = 0.0f;
	float x2 = 0.0f;
	float y2 = 0.0f;
	std::array<char, MAX_QPATH> shaderName{};
	bool textLike = false;
	bool refdef = false;
	int drawCount = 0;
	int samples = 0;
	bool likelyAligned = false;
	HudTransformMode mode = HudTransformMode::Uniform;
	HudAlignX alignX = HudAlignX::Center;
	HudAlignY alignY = HudAlignY::Middle;
};

std::array<HudShaderName, kHudMaxShaderNames> hudShaderNames{};
int hudNumShaderNames = 0;

std::array<HudRule, kHudMaxRules> hudRules{};
int hudNumRules = 0;
bool hudScriptLoaded = false;
bool hudScriptWarned = false;
int hudLastAspectMode = 0;

// Script options. With textRuns a glyph is matched by the line of text it
// belongs to, so a line is never split between two placements. A shader in
// widthScaledShaders was scaled vertically by the screen's width, not its
// height, by the module (Quake III's proportional font), which is undone.
bool hudTextRuns = false;
constexpr int kHudMaxWidthScaled = 16;
std::array<std::array<char, MAX_QPATH>, kHudMaxWidthScaled> hudWidthScaled{};
int hudNumWidthScaled = 0;

// Set by a rule's panel for the rest of the client's frame.
bool hudFramePanel = false;

// Glyphs of the current line, and the colour changes between them.
constexpr int kHudMaxQueue = 512;
std::array<HudQueuedOp, kHudMaxQueue> hudQueue{};
int hudQueueCount = 0;
// The line a flushed glyph belongs to, while CL_HudAdjustPic places it.
bool hudLineActive = false;
float hudLineX1 = 0.0f;
float hudLineY1 = 0.0f;
float hudLineX2 = 0.0f;
float hudLineY2 = 0.0f;

bool hudFrameActive = false;
std::array<HudDrawCapture, kHudMaxFrameDraws> hudFrameDraws{};
int hudFrameDrawCount = 0;

std::array<HudDumpGroup, kHudMaxDumpGroups> hudDumpGroups{};
int hudDumpGroupCount = 0;
bool hudDumpDirty = false;
int hudDumpPrevValue = 0;

// A screen of the game's own drawn in place of its HUD or over it, as the
// client's frame reports it (CL_HudSetGamePanel). Centered pauses the rules,
// so the screen stays one 4:3 composite; Stretched draws it as retail did,
// for one that holds world views of its own.
int hudGamePanel = HUD_PANEL_NONE;

// The screen the client game was told it draws on. It is the real screen
// unless the game was given the 4:3 frame (CL_HudModuleScreen), in which
// case its own views and projections are 4:3 ones.
int hudModuleWidth = 0;
int hudModuleHeight = 0;

bool CL_HudModuleFramed() {
	return hudModuleWidth > 0 && hudModuleHeight > 0
		&& ( hudModuleWidth != cls.glconfig.vidWidth || hudModuleHeight != cls.glconfig.vidHeight );
}

const HudRule *CL_HudFindRule( const HudDrawCapture *draw );

qboolean QBool( bool value ) {
	return value ? qtrue : qfalse;
}

const char *CL_HudAlignXName( HudAlignX align ) {
	switch ( align ) {
	case HudAlignX::Left:
		return "left";
	case HudAlignX::Right:
		return "right";
	default:
		return "center";
	}
}

const char *CL_HudAlignYName( HudAlignY align ) {
	switch ( align ) {
	case HudAlignY::Top:
		return "top";
	case HudAlignY::Bottom:
		return "bottom";
	default:
		return "center";
	}
}

qboolean CL_HudParseAlignX( const char *value, HudAlignX *align ) {
	if ( !value || !*value ) {
		return qfalse;
	}

	if ( !Q_stricmp( value, "left" ) ) {
		*align = HudAlignX::Left;
		return qtrue;
	}

	if ( !Q_stricmp( value, "right" ) ) {
		*align = HudAlignX::Right;
		return qtrue;
	}

	if ( !Q_stricmp( value, "center" ) || !Q_stricmp( value, "middle" ) ) {
		*align = HudAlignX::Center;
		return qtrue;
	}

	return qfalse;
}

qboolean CL_HudParseAlignY( const char *value, HudAlignY *align ) {
	if ( !value || !*value ) {
		return qfalse;
	}

	if ( !Q_stricmp( value, "top" ) ) {
		*align = HudAlignY::Top;
		return qtrue;
	}

	if ( !Q_stricmp( value, "bottom" ) ) {
		*align = HudAlignY::Bottom;
		return qtrue;
	}

	if ( !Q_stricmp( value, "center" ) || !Q_stricmp( value, "middle" ) ) {
		*align = HudAlignY::Middle;
		return qtrue;
	}

	return qfalse;
}

qboolean CL_HudParseMode( const char *value, HudTransformMode *mode ) {
	if ( !value || !*value ) {
		return qfalse;
	}

	if ( !Q_stricmp( value, "stretch" ) ) {
		*mode = HudTransformMode::Stretch;
		return qtrue;
	}

	if ( !Q_stricmp( value, "uniform" ) ) {
		*mode = HudTransformMode::Uniform;
		return qtrue;
	}

	if ( !Q_stricmp( value, "extend" ) ) {
		*mode = HudTransformMode::Extend;
		return qtrue;
	}

	return qfalse;
}

qboolean CL_HudParseEdge( const char *value, HudMatchEdge *edge ) {
	if ( !value || !*value ) {
		return qfalse;
	}

	if ( !Q_stricmp( value, "left" ) ) {
		*edge = HudMatchEdge::Left;
		return qtrue;
	}

	if ( !Q_stricmp( value, "right" ) ) {
		*edge = HudMatchEdge::Right;
		return qtrue;
	}

	if ( !Q_stricmp( value, "center" ) || !Q_stricmp( value, "middle" ) ) {
		*edge = HudMatchEdge::Center;
		return qtrue;
	}

	return qfalse;
}

// A finite, non-negative JSON number, as the region's coordinates are read.
bool CL_HudReadSize( const char *valueJson, const char *jsonEnd, float *size ) {
	if ( !valueJson || JSON_ValueGetType( valueJson, jsonEnd ) != JSONTYPE_VALUE
		|| !( *valueJson >= '0' && *valueJson <= '9' ) ) {
		return false;
	}
	*size = JSON_ValueGetFloat( valueJson, jsonEnd );
	return std::isfinite( *size ) && *size >= 0.0f;
}

qboolean CL_HudShaderMatches( const char *pattern, const char *shaderName ) {
	if ( !pattern || !*pattern || !shaderName ) {
		return qfalse;
	}

	const char *wildcard = std::strchr( pattern, '*' );
	if ( !wildcard || wildcard[1] != '\0' ) {
		return QBool( !Q_stricmp( pattern, shaderName ) );
	}

	return QBool( !Q_stricmpn( pattern, shaderName, static_cast<int>( wildcard - pattern ) ) );
}

void CL_HudJsonWriteString( fileHandle_t file, const char *text ) {
	FileWrite( file, "\"", 1 );

	for ( const char *s = text; s && *s; s++ ) {
		switch ( *s ) {
		case '\\':
		case '"': {
			const char ch = '\\';
			FileWrite( file, &ch, 1 );
			FileWrite( file, s, 1 );
			break;
		}
		case '\n':
			FileWrite( file, "\\n", 2 );
			break;
		case '\r':
			FileWrite( file, "\\r", 2 );
			break;
		case '\t':
			FileWrite( file, "\\t", 2 );
			break;
		default:
			if ( static_cast<unsigned char>( *s ) < 32 ) FS_Printf( file, "\\u%04x", static_cast<unsigned char>( *s ) );
			else FileWrite( file, s, 1 );
			break;
		}
	}

	FileWrite( file, "\"", 1 );
}

void CL_HudClearRules() {
	hudNumRules = 0;
	hudScriptLoaded = false;
	hudTextRuns = false;
	hudNumWidthScaled = 0;
}

// json.h is a value lookup helper, not a validator. Bound nesting and validate
// the complete document before it can recurse over a user-supplied HUD file.
class HudJsonValidator {
	const char *cursor;
	const char *end;
	void space() {
		while ( cursor < end && ( *cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n' ) ) ++cursor;
	}
	bool take( char ch ) {
		space();
		if ( cursor == end || *cursor != ch ) return false;
		++cursor;
		return true;
	}
	bool string() {
		if ( !take( '"' ) ) return false;
		while ( cursor < end ) {
			const unsigned char ch = static_cast<unsigned char>( *cursor++ );
			if ( ch == '"' ) return true;
			if ( ch < 32 ) return false;
			if ( ch != '\\' ) continue;
			if ( cursor == end ) return false;
			const char escape = *cursor++;
			if ( escape == 'u' ) {
				for ( int i = 0; i < 4; ++i ) {
					if ( cursor == end ) return false;
					const char digit = *cursor++;
					if ( !( ( digit >= '0' && digit <= '9' ) || ( digit >= 'a' && digit <= 'f' ) || ( digit >= 'A' && digit <= 'F' ) ) ) return false;
				}
			} else if ( !escape || !std::strchr( "\"\\/bfnrt", escape ) ) return false;
		}
		return false;
	}
	bool digits() {
		const char *start = cursor;
		while ( cursor < end && *cursor >= '0' && *cursor <= '9' ) ++cursor;
		return cursor != start;
	}
	bool value( int depth ) {
		space();
		if ( cursor == end || depth > 16 ) return false;
		if ( *cursor == '"' ) return string();
		if ( *cursor == '{' || *cursor == '[' ) {
			const bool object = *cursor++ == '{';
			const char close = object ? '}' : ']';
			if ( take( close ) ) return true;
			do {
				if ( object && ( !string() || !take( ':' ) ) ) return false;
				if ( !value( depth + 1 ) ) return false;
				if ( take( close ) ) return true;
			} while ( take( ',' ) );
			return false;
		}
		for ( const char *literal : { "true", "false", "null" } ) {
			const auto length = std::strlen( literal );
			if ( static_cast<std::size_t>( end - cursor ) >= length && !std::strncmp( cursor, literal, length ) ) {
				cursor += length;
				return true;
			}
		}
		if ( *cursor == '-' ) ++cursor;
		if ( cursor == end ) return false;
		if ( *cursor == '0' ) ++cursor;
		else if ( !digits() ) return false;
		if ( cursor < end && *cursor == '.' ) { ++cursor; if ( !digits() ) return false; }
		if ( cursor < end && ( *cursor == 'e' || *cursor == 'E' ) ) {
			++cursor;
			if ( cursor < end && ( *cursor == '+' || *cursor == '-' ) ) ++cursor;
			if ( !digits() ) return false;
		}
		return true;
	}
public:
	HudJsonValidator( const char *begin, const char *limit ) : cursor( begin ), end( limit ) {}
	bool valid() { if ( !value( 0 ) ) return false; space(); return cursor == end; }
};

qboolean CL_HudLoadRules( qboolean verbose ) {
	CL_HudClearRules();
	hudScriptLoaded = true;
	if ( !hudSupported ) return qfalse;

	const char *scriptName = kHudScriptFile;
	int length = FS_ReadFile( scriptName, nullptr );
	if ( length < 0 ) {
		scriptName = kLegacyHudScriptFile;
		length = FS_ReadFile( scriptName, nullptr );
	}
	ScopedReadFile scriptFile;
	if ( length > 0 && length <= kHudMaxScriptBytes ) scriptFile = ScopedReadFile::Read( scriptName );
	if ( !scriptFile ) {
		if ( verbose || ( !hudScriptWarned && cl_hudAspect && cl_hudAspect->integer > 0 ) ) {
			Com_Printf( S_COLOR_YELLOW "HUD: no usable script in %s (legacy %s), using centered uniform placement.\n", kHudScriptFile, kLegacyHudScriptFile );
			hudScriptWarned = true;
		}
		return qfalse;
	}

	const char *json = scriptFile.as<const char>();
	const char *jsonEnd = json + scriptFile.length();
	const char *rulesJson = nullptr;
	if ( scriptFile.length() > kHudMaxScriptBytes || !HudJsonValidator( json, jsonEnd ).valid() ) {
		if ( verbose || !hudScriptWarned ) Com_Printf( S_COLOR_YELLOW "HUD: invalid JSON in %s, using centered uniform placement.\n", scriptName );
		hudScriptWarned = true;
		return qfalse;
	}
	while ( json < jsonEnd && ( *json == ' ' || *json == '\t' || *json == '\r' || *json == '\n' ) ) ++json;

	if ( JSON_ValueGetType( json, jsonEnd ) == JSONTYPE_OBJECT ) {
		rulesJson = JSON_ObjectGetNamedValue( json, jsonEnd, "rules" );
	} else if ( JSON_ValueGetType( json, jsonEnd ) == JSONTYPE_ARRAY ) {
		rulesJson = json;
	}

	if ( !rulesJson || JSON_ValueGetType( rulesJson, jsonEnd ) != JSONTYPE_ARRAY ) {
		hudScriptLoaded = true;
		if ( verbose || !hudScriptWarned ) {
			Com_Printf( S_COLOR_YELLOW "HUD: invalid script format in %s.\n", scriptName );
			hudScriptWarned = true;
		}
		return qfalse;
	}

	// Options of the object form; a malformed one is ignored.
	if ( rulesJson != json ) {
		const char *optionJson = JSON_ObjectGetNamedValue( json, jsonEnd, "textRuns" );
		hudTextRuns = optionJson && JSON_ValueGetType( optionJson, jsonEnd ) == JSONTYPE_VALUE
			&& JSON_ValueGetDouble( optionJson, jsonEnd ) != 0;
		optionJson = JSON_ObjectGetNamedValue( json, jsonEnd, "widthScaledShaders" );
		if ( optionJson && JSON_ValueGetType( optionJson, jsonEnd ) == JSONTYPE_ARRAY ) {
			for ( const char *patternJson = JSON_ArrayGetFirstValue( optionJson, jsonEnd );
				patternJson && hudNumWidthScaled < kHudMaxWidthScaled;
				patternJson = JSON_ArrayGetNextValue( patternJson, jsonEnd ) ) {
				auto &pattern = hudWidthScaled[hudNumWidthScaled];
				if ( JSON_ValueGetType( patternJson, jsonEnd ) == JSONTYPE_STRING
					&& JSON_ValueGetString( patternJson, jsonEnd, pattern.data(), static_cast<int>( pattern.size() ) )
					&& pattern[0] ) {
					hudNumWidthScaled++;
				}
			}
		}
	}

	for ( const char *ruleJson = JSON_ArrayGetFirstValue( rulesJson, jsonEnd );
		ruleJson && hudNumRules < kHudMaxRules;
		ruleJson = JSON_ArrayGetNextValue( ruleJson, jsonEnd ) ) {
		const char *valueJson;
		const char *matchJson;
		const char *regionJson;
		const char *alignJson;
		std::array<char, 128> text;

		if ( JSON_ValueGetType( ruleJson, jsonEnd ) != JSONTYPE_OBJECT ) {
			continue;
		}

		HudRule *rule = &hudRules[hudNumRules];
		*rule = HudRule{};

		valueJson = JSON_ObjectGetNamedValue( ruleJson, jsonEnd, "name" );
		if ( valueJson ) {
			JSON_ValueGetString( valueJson, jsonEnd, rule->name.data(), static_cast<int>( rule->name.size() ) );
		}

		valueJson = JSON_ObjectGetNamedValue( ruleJson, jsonEnd, "mode" );
		if ( valueJson ) {
			if ( !JSON_ValueGetString( valueJson, jsonEnd, text.data(), static_cast<int>( text.size() ) )
				|| !CL_HudParseMode( text.data(), &rule->mode ) ) continue;
		}

		valueJson = JSON_ObjectGetNamedValue( ruleJson, jsonEnd, "panel" );
		if ( valueJson ) {
			if ( JSON_ValueGetType( valueJson, jsonEnd ) != JSONTYPE_STRING
				|| !JSON_ValueGetString( valueJson, jsonEnd, text.data(), static_cast<int>( text.size() ) )
				|| Q_stricmp( text.data(), "centered" ) ) continue;
			rule->panel = true;
		}

		alignJson = JSON_ObjectGetNamedValue( ruleJson, jsonEnd, "align" );
		if ( alignJson && JSON_ValueGetType( alignJson, jsonEnd ) != JSONTYPE_OBJECT ) continue;
		if ( alignJson && JSON_ValueGetType( alignJson, jsonEnd ) == JSONTYPE_OBJECT ) {
			valueJson = JSON_ObjectGetNamedValue( alignJson, jsonEnd, "x" );
			if ( valueJson && JSON_ValueGetString( valueJson, jsonEnd, text.data(), static_cast<int>( text.size() ) ) ) {
				rule->hasAlignX = CL_HudParseAlignX( text.data(), &rule->alignX );
			}
			if ( valueJson && !rule->hasAlignX ) continue;

			valueJson = JSON_ObjectGetNamedValue( alignJson, jsonEnd, "y" );
			if ( valueJson && JSON_ValueGetString( valueJson, jsonEnd, text.data(), static_cast<int>( text.size() ) ) ) {
				rule->hasAlignY = CL_HudParseAlignY( text.data(), &rule->alignY );
			}
			if ( valueJson && !rule->hasAlignY ) continue;
		}

		matchJson = JSON_ObjectGetNamedValue( ruleJson, jsonEnd, "match" );
		if ( matchJson && JSON_ValueGetType( matchJson, jsonEnd ) != JSONTYPE_OBJECT ) continue;
		if ( matchJson && JSON_ValueGetType( matchJson, jsonEnd ) == JSONTYPE_OBJECT ) {
			valueJson = JSON_ObjectGetNamedValue( matchJson, jsonEnd, "shader" );
			if ( valueJson && JSON_ValueGetString( valueJson, jsonEnd, rule->shader.data(), static_cast<int>( rule->shader.size() ) ) ) {
				rule->hasShader = true;
			}
			if ( valueJson && ( JSON_ValueGetType( valueJson, jsonEnd ) != JSONTYPE_STRING || !rule->hasShader ) ) continue;

			valueJson = JSON_ObjectGetNamedValue( matchJson, jsonEnd, "textLike" );
			if ( valueJson ) {
				rule->hasTextLike = true;
				if ( JSON_ValueGetType( valueJson, jsonEnd ) != JSONTYPE_VALUE ) continue;
				rule->textLike = JSON_ValueGetDouble( valueJson, jsonEnd ) != 0;
			}

			valueJson = JSON_ObjectGetNamedValue( matchJson, jsonEnd, "refdef" );
			if ( valueJson ) {
				rule->hasRefdef = true;
				if ( JSON_ValueGetType( valueJson, jsonEnd ) != JSONTYPE_VALUE ) continue;
				rule->refdef = JSON_ValueGetDouble( valueJson, jsonEnd ) != 0;
			}

			valueJson = JSON_ObjectGetNamedValue( matchJson, jsonEnd, "edge" );
			if ( valueJson ) {
				if ( JSON_ValueGetType( valueJson, jsonEnd ) != JSONTYPE_STRING
					|| !JSON_ValueGetString( valueJson, jsonEnd, text.data(), static_cast<int>( text.size() ) )
					|| !CL_HudParseEdge( text.data(), &rule->edge ) ) continue;
			}

			valueJson = JSON_ObjectGetNamedValue( matchJson, jsonEnd, "minHeight" );
			if ( valueJson ) {
				if ( !CL_HudReadSize( valueJson, jsonEnd, &rule->minHeight ) ) continue;
				rule->hasMinHeight = true;
			}

			valueJson = JSON_ObjectGetNamedValue( matchJson, jsonEnd, "maxHeight" );
			if ( valueJson ) {
				if ( !CL_HudReadSize( valueJson, jsonEnd, &rule->maxHeight ) ) continue;
				rule->hasMaxHeight = true;
			}

			regionJson = JSON_ObjectGetNamedValue( matchJson, jsonEnd, "region" );
			if ( regionJson && JSON_ValueGetType( regionJson, jsonEnd ) != JSONTYPE_OBJECT ) continue;
			if ( regionJson && JSON_ValueGetType( regionJson, jsonEnd ) == JSONTYPE_OBJECT ) {
				bool validRegion = true;
				for ( const char *axis : { "x", "y", "w", "h" } ) {
					const char *coordinate = JSON_ObjectGetNamedValue( regionJson, jsonEnd, axis );
					if ( !coordinate || !( *coordinate == '-' || ( *coordinate >= '0' && *coordinate <= '9' ) ) ) validRegion = false;
				}
				if ( !validRegion ) continue;
				rule->regionX = JSON_ValueGetFloat( JSON_ObjectGetNamedValue( regionJson, jsonEnd, "x" ), jsonEnd );
				rule->regionY = JSON_ValueGetFloat( JSON_ObjectGetNamedValue( regionJson, jsonEnd, "y" ), jsonEnd );
				rule->regionW = JSON_ValueGetFloat( JSON_ObjectGetNamedValue( regionJson, jsonEnd, "w" ), jsonEnd );
				rule->regionH = JSON_ValueGetFloat( JSON_ObjectGetNamedValue( regionJson, jsonEnd, "h" ), jsonEnd );
				if ( !std::isfinite( rule->regionX ) || !std::isfinite( rule->regionY )
					|| !std::isfinite( rule->regionW ) || !std::isfinite( rule->regionH )
					|| !std::isfinite( rule->regionX + rule->regionW ) || !std::isfinite( rule->regionY + rule->regionH )
					|| rule->regionW < 0 || rule->regionH < 0 ) continue;
				rule->hasRegion = true;
			}
		}

		hudNumRules++;
	}

	hudScriptLoaded = true;
	hudScriptWarned = false;
	if ( verbose ) {
		Com_Printf( "HUD: loaded %i rule%s from %s.\n", hudNumRules, hudNumRules == 1 ? "" : "s", scriptName );
	}
	return qtrue;
}

void CL_HudEnsureRules() {
	if ( !hudSupported || !cl_hudAspect || cl_hudAspect->integer <= 0 ) {
		return;
	}

	if ( hudLastAspectMode != cl_hudAspect->integer ) {
		hudLastAspectMode = cl_hudAspect->integer;
		hudScriptLoaded = false;
		hudScriptWarned = false;
	}

	if ( !hudScriptLoaded ) {
		CL_HudLoadRules( qfalse );
	}
}

void HudReload() {
	if ( !hudSupported ) return;
	hudLastAspectMode = -1;
	hudScriptWarned = false;
	CL_HudLoadRules( qtrue );
}

const char *CL_HudModuleShaderName( qhandle_t shader ) {
	for ( int i = 0; i < hudNumShaderNames; i++ ) {
		if ( hudShaderNames[i].handle == shader ) {
			return hudShaderNames[i].name.data();
		}
	}

	return "";
}

// The client's own shaders keep fixed names, so "charset" means the console
// font under every profile. A module that registers one of them by its real
// name gets the same handle: Q3's cgame registers the client's
// gfx/2d/bigchars, and RTCW MP's the client's gfx/2d/hudchars. Such a draw is
// reported by the client's name and matches a rule naming either one.
const char *CL_HudClientShaderName( qhandle_t shader ) {
	if ( shader == cls.charSetShader ) {
		return "charset";
	}

	if ( shader == cls.whiteShader ) {
		return "white";
	}

	if ( shader == cls.consoleShader ) {
		return "console";
	}

	return "";
}

qboolean CL_HudIsTextLike( qhandle_t shader, const char *shaderName, float w, float h ) {
	if ( shader == cls.charSetShader ) {
		return qtrue;
	}

	if ( shaderName && *shaderName && !Q_stricmp( shaderName, "charset" ) ) {
		return qtrue;
	}
	// The shipped FnQuake3 rules classify icon/number atlases as pictures.
	// Keep those matches stable when small quads are measured in 640x480 units.
	if ( shaderName && ( !Q_stricmpn( shaderName, "icons/", 6 )
		|| !Q_stricmpn( shaderName, "gfx/2d/numbers/", 15 ) ) ) {
		return qfalse;
	}

	return QBool( w <= BIGCHAR_WIDTH * 2.0f && h <= GIANTCHAR_HEIGHT * 1.5f );
}

qboolean CL_HudIsFullscreenLike( float x, float y, float w, float h ) {
	return QBool( x <= 1.0f && y <= 1.0f && w >= SCREEN_WIDTH - 1.0f && h >= SCREEN_HEIGHT - 1.0f );
}

void CL_HudApplyStretch( float *x, float *y, float *w, float *h ) {
	const float xscale = cls.glconfig.vidWidth / 640.0f;
	const float yscale = cls.glconfig.vidHeight / 480.0f;

	*x *= xscale;
	*y *= yscale;
	*w *= xscale;
	*h *= yscale;
}

// From the pixels of the screen the client game draws on to its 640x480 units.
// widthScaled: the module scaled this draw vertically by the horizontal scale.
void CL_HudUnstretch( float x, float y, float w, float h, float *outX, float *outY, float *outW, float *outH,
	bool widthScaled = false ) {
	const bool framed = CL_HudModuleFramed();
	const float xscale = ( framed ? hudModuleWidth : cls.glconfig.vidWidth ) / 640.0f;
	const float yscale = widthScaled ? xscale : ( framed ? hudModuleHeight : cls.glconfig.vidHeight ) / 480.0f;

	if ( xscale == 0.0f || yscale == 0.0f ) {
		*outX = x;
		*outY = y;
		*outW = w;
		*outH = h;
		return;
	}

	*outX = x / xscale;
	*outY = y / yscale;
	*outW = w / xscale;
	*outH = h / yscale;
}

void CL_HudApplyUniform( float *x, float *y, float *w, float *h, HudAlignX alignX, HudAlignY alignY ) {
	float originX;
	float originY;

	switch ( alignX ) {
	case HudAlignX::Left:
		originX = 0.0f;
		break;
	case HudAlignX::Right:
		originX = cls.biasX * 2.0f;
		break;
	default:
		originX = cls.biasX;
		break;
	}

	switch ( alignY ) {
	case HudAlignY::Top:
		originY = 0.0f;
		break;
	case HudAlignY::Bottom:
		originY = cls.biasY * 2.0f;
		break;
	default:
		originY = cls.biasY;
		break;
	}

	*x = *x * cls.scale + originX;
	*y = *y * cls.scale + originY;
	*w *= cls.scale;
	*h *= cls.scale;
}

// Each edge of the 640x480 draw that touches the frame's is carried out to the
// screen's. Flipped draws keep their uniform placement.
void CL_HudExtendToEdges( const HudDrawCapture *draw, float *x, float *y, float *w, float *h ) {
	if ( !( draw->w > 0.0f ) || !( draw->h > 0.0f ) ) {
		return;
	}
	if ( draw->x <= 0.0f ) {
		*w += *x;
		*x = 0.0f;
	}
	if ( draw->x + draw->w >= SCREEN_WIDTH ) {
		*w = cls.glconfig.vidWidth - *x;
	}
	if ( draw->y <= 0.0f ) {
		*h += *y;
		*y = 0.0f;
	}
	if ( draw->y + draw->h >= SCREEN_HEIGHT ) {
		*h = cls.glconfig.vidHeight - *y;
	}
}

void CL_HudTransformRect( const HudDrawCapture *draw, float *x, float *y, float *w, float *h ) {
	HudTransformMode mode = CL_HudIsFullscreenLike( draw->x, draw->y, draw->w, draw->h ) ? HudTransformMode::Stretch : HudTransformMode::Uniform;
	HudAlignX alignX = HudAlignX::Center;
	HudAlignY alignY = HudAlignY::Middle;

	if ( hudScriptLoaded && hudNumRules > 0 && hudGamePanel != HUD_PANEL_CENTERED && !hudFramePanel ) {
		const HudRule *rule = CL_HudFindRule( draw );
		if ( rule ) {
			mode = rule->mode;
			if ( rule->hasAlignX ) {
				alignX = rule->alignX;
			}
			if ( rule->hasAlignY ) {
				alignY = rule->alignY;
			}
			// The draws after this one belong to the game's screen.
			if ( rule->panel && hudFrameActive ) {
				hudFramePanel = true;
			}
		}
	}

	*x = draw->x;
	*y = draw->y;
	*w = draw->w;
	*h = draw->h;

	if ( mode == HudTransformMode::Stretch ) {
		CL_HudApplyStretch( x, y, w, h );
	} else {
		CL_HudApplyUniform( x, y, w, h, alignX, alignY );
		if ( mode == HudTransformMode::Extend ) {
			CL_HudExtendToEdges( draw, x, y, w, h );
		}
	}
}

const HudRule *CL_HudFindRule( const HudDrawCapture *draw ) {
	// The area the rules test: the draw's own, or its whole line of text.
	float x1 = std::min( draw->x, draw->x + draw->w );
	float x2 = std::max( draw->x, draw->x + draw->w );
	float y1 = std::min( draw->y, draw->y + draw->h );
	float y2 = std::max( draw->y, draw->y + draw->h );
	if ( draw->inLine ) {
		x1 = draw->lineX1;
		x2 = draw->lineX2;
		y1 = draw->lineY1;
		y2 = draw->lineY2;
	}
	const float cy = ( y1 + y2 ) * 0.5f;
	const float height = std::fabs( draw->h );

	for ( int i = 0; i < hudNumRules; i++ ) {
		const HudRule *rule = &hudRules[i];
		const float cx = rule->edge == HudMatchEdge::Left ? x1
			: rule->edge == HudMatchEdge::Right ? x2 : ( x1 + x2 ) * 0.5f;

		if ( rule->hasShader && !CL_HudShaderMatches( rule->shader.data(), draw->shaderName.data() )
			&& !CL_HudShaderMatches( rule->shader.data(), draw->moduleShaderName ) ) {
			continue;
		}

		if ( rule->hasTextLike && rule->textLike != draw->textLike ) {
			continue;
		}

		if ( rule->hasRefdef && rule->refdef != draw->refdef ) {
			continue;
		}

		if ( ( rule->hasMinHeight && height < rule->minHeight ) || ( rule->hasMaxHeight && height > rule->maxHeight ) ) {
			continue;
		}

		if ( rule->hasRegion ) {
			if ( cx < rule->regionX || cy < rule->regionY ||
				cx > rule->regionX + rule->regionW || cy > rule->regionY + rule->regionH ) {
				continue;
			}
		}

		return rule;
	}

	return nullptr;
}

void CL_HudCaptureDraw( const HudDrawCapture *draw ) {
	if ( !hudFrameActive || !cl_hudDump || !cl_hudDump->integer ) {
		return;
	}

	if ( hudFrameDrawCount >= kHudMaxFrameDraws ) {
		return;
	}

	hudFrameDraws[hudFrameDrawCount++] = *draw;
}

bool CL_HudDrawLess( const HudDrawCapture &a, const HudDrawCapture &b ) {
	if ( a.y < b.y ) {
		return true;
	}
	if ( a.y > b.y ) {
		return false;
	}
	if ( a.x < b.x ) {
		return true;
	}
	if ( a.x > b.x ) {
		return false;
	}
	return Q_stricmp( a.shaderName.data(), b.shaderName.data() ) < 0;
}

qboolean CL_HudCanMergeGroup( const HudDrawCapture *draw, const HudDumpGroup *group ) {
	const float gapX = draw->textLike ? 24.0f : 8.0f;
	const float gapY = draw->textLike ? 6.0f : 8.0f;
	const float drawX2 = draw->x + draw->w;
	const float drawY2 = draw->y + draw->h;

	if ( draw->textLike != group->textLike ) {
		return qfalse;
	}

	if ( draw->refdef != group->refdef ) {
		return qfalse;
	}

	if ( Q_stricmp( draw->shaderName.data(), group->shaderName.data() ) ) {
		return qfalse;
	}

	if ( drawY2 < group->y1 - gapY || draw->y > group->y2 + gapY ) {
		return qfalse;
	}

	if ( drawX2 < group->x1 - gapX || draw->x > group->x2 + gapX ) {
		return qfalse;
	}

	return qtrue;
}

void CL_HudSuggestPlacement( HudDumpGroup *group ) {
	const float width = group->x2 - group->x1;
	const float height = group->y2 - group->y1;
	const float centerX = group->x1 + width * 0.5f;
	const float centerY = group->y1 + height * 0.5f;

	if ( CL_HudIsFullscreenLike( group->x1, group->y1, width, height ) ) {
		group->mode = HudTransformMode::Stretch;
		group->alignX = HudAlignX::Center;
		group->alignY = HudAlignY::Middle;
		group->likelyAligned = false;
		return;
	}

	group->mode = HudTransformMode::Uniform;

	if ( group->x1 <= 96.0f || centerX <= 160.0f ) {
		group->alignX = HudAlignX::Left;
	} else if ( group->x2 >= SCREEN_WIDTH - 96.0f || centerX >= SCREEN_WIDTH - 160.0f ) {
		group->alignX = HudAlignX::Right;
	} else {
		group->alignX = HudAlignX::Center;
	}

	if ( group->y1 <= 72.0f || centerY <= 120.0f ) {
		group->alignY = HudAlignY::Top;
	} else if ( group->y2 >= SCREEN_HEIGHT - 72.0f || centerY >= SCREEN_HEIGHT - 120.0f ) {
		group->alignY = HudAlignY::Bottom;
	} else {
		group->alignY = HudAlignY::Middle;
	}

	group->likelyAligned = group->alignX != HudAlignX::Center || group->alignY != HudAlignY::Middle;
}

qboolean CL_HudSameDumpGroup( const HudDumpGroup *left, const HudDumpGroup *right ) {
	const float leftW = left->x2 - left->x1;
	const float leftH = left->y2 - left->y1;
	const float rightW = right->x2 - right->x1;
	const float rightH = right->y2 - right->y1;
	const float leftCx = left->x1 + leftW * 0.5f;
	const float leftCy = left->y1 + leftH * 0.5f;
	const float rightCx = right->x1 + rightW * 0.5f;
	const float rightCy = right->y1 + rightH * 0.5f;

	if ( left->textLike != right->textLike ) {
		return qfalse;
	}

	if ( left->refdef != right->refdef ) {
		return qfalse;
	}

	if ( left->mode != right->mode || left->alignX != right->alignX || left->alignY != right->alignY ) {
		return qfalse;
	}

	if ( Q_stricmp( left->shaderName.data(), right->shaderName.data() ) ) {
		return qfalse;
	}

	if ( std::fabs( leftCx - rightCx ) > 24.0f || std::fabs( leftCy - rightCy ) > 24.0f ) {
		return qfalse;
	}

	if ( std::fabs( leftW - rightW ) > 96.0f || std::fabs( leftH - rightH ) > 48.0f ) {
		return qfalse;
	}

	return qtrue;
}

void CL_HudAccumulateDumpGroup( const HudDumpGroup *group ) {
	for ( int i = 0; i < hudDumpGroupCount; i++ ) {
		HudDumpGroup *existing = &hudDumpGroups[i];

		if ( !CL_HudSameDumpGroup( existing, group ) ) {
			continue;
		}

		if ( group->x1 < existing->x1 ) {
			existing->x1 = group->x1;
		}
		if ( group->y1 < existing->y1 ) {
			existing->y1 = group->y1;
		}
		if ( group->x2 > existing->x2 ) {
			existing->x2 = group->x2;
		}
		if ( group->y2 > existing->y2 ) {
			existing->y2 = group->y2;
		}

		existing->drawCount += group->drawCount;
		existing->samples++;
		hudDumpDirty = true;
		return;
	}

	if ( hudDumpGroupCount >= kHudMaxDumpGroups ) {
		return;
	}

	hudDumpGroups[hudDumpGroupCount] = *group;
	hudDumpGroups[hudDumpGroupCount].samples = 1;
	hudDumpGroupCount++;
	hudDumpDirty = true;
}

void CL_HudWriteDumpFile() {
	if ( !hudDumpDirty ) {
		return;
	}

	ScopedFileHandle file( FS_FOpenFileWrite( kHudDumpFile ) );
	if ( !file ) {
		Com_Printf( S_COLOR_YELLOW "HUD: failed to write dump file %s.\n", kHudDumpFile );
		return;
	}
	const fileHandle_t dumpFile = file.get();

	FS_Printf( dumpFile, "{\n  \"version\": 1,\n  \"rules\": [\n" );

	for ( int i = 0; i < hudDumpGroupCount; i++ ) {
		const HudDumpGroup *group = &hudDumpGroups[i];

		if ( i ) {
			FS_Printf( dumpFile, ",\n" );
		}

		FS_Printf( dumpFile, "    {\n" );
		FS_Printf( dumpFile, "      \"name\": " );
		CL_HudJsonWriteString( dumpFile, va( "hud_%04i", i ) );
		FS_Printf( dumpFile, ",\n      \"match\": {\n" );

		if ( group->shaderName[0] ) {
			FS_Printf( dumpFile, "        \"shader\": " );
			CL_HudJsonWriteString( dumpFile, group->shaderName.data() );
			FS_Printf( dumpFile, ",\n" );
		}

		if ( group->refdef ) {
			FS_Printf( dumpFile, "        \"refdef\": 1,\n" );
		}

		FS_Printf( dumpFile, "        \"textLike\": %i,\n", group->textLike ? 1 : 0 );
		FS_Printf( dumpFile, "        \"region\": { \"x\": %.3f, \"y\": %.3f, \"w\": %.3f, \"h\": %.3f }\n",
			group->x1, group->y1, group->x2 - group->x1, group->y2 - group->y1 );
		FS_Printf( dumpFile, "      },\n" );

		FS_Printf( dumpFile, "      \"mode\": " );
		CL_HudJsonWriteString( dumpFile, group->mode == HudTransformMode::Stretch ? "stretch" : "uniform" );
		FS_Printf( dumpFile, ",\n      \"align\": {\n        \"x\": " );
		CL_HudJsonWriteString( dumpFile, CL_HudAlignXName( group->alignX ) );
		FS_Printf( dumpFile, ",\n        \"y\": " );
		CL_HudJsonWriteString( dumpFile, CL_HudAlignYName( group->alignY ) );
		FS_Printf( dumpFile, "\n      },\n" );
		FS_Printf( dumpFile, "      \"likelyAligned\": %i,\n", group->likelyAligned ? 1 : 0 );
		FS_Printf( dumpFile, "      \"samples\": %i,\n", group->samples );
		FS_Printf( dumpFile, "      \"drawCount\": %i\n", group->drawCount );
		FS_Printf( dumpFile, "    }" );
	}

	FS_Printf( dumpFile, "\n  ]\n}\n" );
	hudDumpDirty = false;
}

void CL_HudDumpFrame() {
	if ( hudFrameDrawCount <= 0 ) {
		return;
	}

	std::sort( hudFrameDraws.begin(), hudFrameDraws.begin() + hudFrameDrawCount, CL_HudDrawLess );

	std::array<HudDumpGroup, kHudMaxFrameGroups> frameGroups{};
	int frameGroupCount = 0;

	for ( int i = 0; i < hudFrameDrawCount; i++ ) {
		const HudDrawCapture *draw = &hudFrameDraws[i];
		const float drawX2 = draw->x + draw->w;
		const float drawY2 = draw->y + draw->h;
		bool merged = false;

		for ( int j = 0; j < frameGroupCount; j++ ) {
			HudDumpGroup *group = &frameGroups[j];

			if ( !CL_HudCanMergeGroup( draw, group ) ) {
				continue;
			}

			if ( draw->x < group->x1 ) {
				group->x1 = draw->x;
			}
			if ( draw->y < group->y1 ) {
				group->y1 = draw->y;
			}
			if ( drawX2 > group->x2 ) {
				group->x2 = drawX2;
			}
			if ( drawY2 > group->y2 ) {
				group->y2 = drawY2;
			}
			group->drawCount++;
			merged = true;
			break;
		}

		if ( merged || frameGroupCount >= kHudMaxFrameGroups ) {
			continue;
		}

		HudDumpGroup *group = &frameGroups[frameGroupCount];
		*group = HudDumpGroup{};
		group->x1 = draw->x;
		group->y1 = draw->y;
		group->x2 = drawX2;
		group->y2 = drawY2;
		Q_strncpyz( group->shaderName.data(), draw->shaderName.data(), static_cast<int>( group->shaderName.size() ) );
		group->textLike = draw->textLike;
		group->refdef = draw->refdef;
		group->drawCount = 1;
		frameGroupCount++;
	}

	for ( int i = 0; i < frameGroupCount; i++ ) {
		CL_HudSuggestPlacement( &frameGroups[i] );
		CL_HudAccumulateDumpGroup( &frameGroups[i] );
	}

	CL_HudWriteDumpFile();
}

bool CL_HudWidthScaled( const char *shaderName ) {
	for ( int i = 0; i < hudNumWidthScaled; i++ ) {
		if ( CL_HudShaderMatches( hudWidthScaled[i].data(), shaderName ) ) {
			return true;
		}
	}
	return false;
}

// A module's picture in its 640x480 units, named and classified.
void CL_HudDescribeDraw( float x, float y, float w, float h, qhandle_t shader, HudDrawCapture *draw ) {
	const char *clientName = CL_HudClientShaderName( shader );
	const char *moduleName = CL_HudModuleShaderName( shader );
	const bool widthScaled = cl_hudAspect && cl_hudAspect->integer > 0 && CL_HudWidthScaled( moduleName );

	CL_HudUnstretch( x, y, w, h, &draw->x, &draw->y, &draw->w, &draw->h, widthScaled );
	draw->shader = shader;
	Q_strncpyz( draw->shaderName.data(), *clientName ? clientName : moduleName, static_cast<int>( draw->shaderName.size() ) );
	if ( *clientName ) {
		draw->moduleShaderName = moduleName;
	}
	draw->textLike = CL_HudIsTextLike( shader, draw->shaderName.data(), std::fabs( draw->w ), std::fabs( draw->h ) );
}

// Whether a client game's glyphs may be held until their line is complete:
// the rules place text by line and would place this frame's draws. A held
// glyph is placed by CL_HudAdjustPic later, so whatever hides the HUD then
// still hides it.
bool CL_HudQueueing() {
	return hudSupported && hudFrameActive && hudScriptLoaded && hudTextRuns && cl_hudAspect && cl_hudAspect->integer > 0
		&& hudGamePanel == HUD_PANEL_NONE && !hudFramePanel
		&& cls.glconfig.vidWidth > 0 && cls.glconfig.vidHeight > 0 && cls.scale > 0.0f;
}

// The next glyph of a line: the same font and size on the same baseline,
// further right by at most four glyph heights of space.
bool CL_HudContinuesLine( const HudDrawCapture &last, const HudDrawCapture &next ) {
	if ( next.shader != last.shader || !( last.w > 0.0f ) || !( next.w > 0.0f ) || !( next.h > 0.0f )
		|| std::fabs( next.h - last.h ) > 0.01f || std::fabs( next.y - last.y ) > 0.01f ) {
		return false;
	}
	const float gap = next.x - ( last.x + last.w );
	return gap >= -0.01f && gap <= last.h * 4.0f + 0.01f;
}

const HudQueuedOp *CL_HudLastQueuedGlyph() {
	for ( int i = hudQueueCount - 1; i >= 0; i-- ) {
		if ( !hudQueue[i].color ) {
			return &hudQueue[i];
		}
	}
	return nullptr;
}

} // namespace

extern "C" void CL_HudReload_f( void ) {
	HudReload();
}

extern "C" void CL_HudInit( void ) {
	hudSupported = Com_GameIsQuake3() || Com_GameIsRTCW() || Com_GameIsWolfET() || Com_GameIsSTVEF()
		|| Com_GameIsRitual();
	cl_hudAspect = nullptr;
	cl_hudDump = nullptr;
	if ( !hudSupported ) return;
	cl_hudAspect = Cvar_Get( "cl_hudAspect", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_hudAspect, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_hudAspect,
		"HUD aspect correction: 0 stretch, 1 uniform 4:3 with optional fntech3-hud.json rules (legacy fnq3-hud.json). Inactive in Quake Live and MoHAA, whose modules place their own HUDs." );
	cl_hudDump = Cvar_Get( "cl_hudDump", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_hudDump, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_hudDump, "Write deduplicated HUD draw groups to fntech3-hud-dump.json in the active writable game directory. Inactive in Quake Live and MoHAA." );
	hudLastAspectMode = -1;
	hudDumpPrevValue = cl_hudDump ? cl_hudDump->integer : 0;
	hudNumShaderNames = 0;
	hudNumRules = 0;
	hudScriptLoaded = false;
	hudScriptWarned = false;
	hudFrameActive = false;
	hudFrameDrawCount = 0;
	hudDumpGroupCount = 0;
	hudDumpDirty = false;
	hudGamePanel = HUD_PANEL_NONE;
	hudModuleWidth = 0;
	hudModuleHeight = 0;
	hudQueueCount = 0;
	hudFramePanel = false;
	hudTextRuns = false;
	hudNumWidthScaled = 0;
	Cmd_AddCommand( "hud_reload", CL_HudReload_f );
}

extern "C" void CL_HudShutdown( void ) {
	if ( !hudSupported ) return;
	CL_HudWriteDumpFile();
	Cmd_RemoveCommand( "hud_reload" );
	hudQueueCount = 0;
	hudFramePanel = false;
	hudSupported = false;
	cl_hudAspect = nullptr;
	cl_hudDump = nullptr;
}

extern "C" void CL_HudResetCGame( void ) {
	if ( !hudSupported ) return;
	CL_HudWriteDumpFile();
	hudNumShaderNames = 0;
	hudFrameActive = false;
	hudFrameDrawCount = 0;
	hudDumpGroupCount = 0;
	hudDumpDirty = false;
	hudScriptWarned = false;
	hudGamePanel = HUD_PANEL_NONE;
	hudModuleWidth = 0;
	hudModuleHeight = 0;
	hudQueueCount = 0;
	hudFramePanel = false;
	CL_HudClearRules();
}

extern "C" void CL_HudBeginFrame( void ) {
	if ( !hudSupported ) return;
	CL_HudFlushQueue();
	hudFramePanel = false;
	if ( cl_hudDump && cl_hudDump->integer != hudDumpPrevValue ) {
		if ( cl_hudDump->integer ) {
			hudDumpGroupCount = 0;
			hudDumpDirty = true;
		}
		hudDumpPrevValue = cl_hudDump->integer;
	}

	hudFrameActive = true;
	hudFrameDrawCount = 0;
	CL_HudEnsureRules();
}

extern "C" void CL_HudEndFrame( void ) {
	if ( !hudSupported ) return;
	CL_HudFlushQueue();
	if ( hudFrameActive && cl_hudDump && cl_hudDump->integer ) {
		CL_HudDumpFrame();
	}

	hudFrameActive = false;
	hudFramePanel = false;
	hudFrameDrawCount = 0;
}

extern "C" void CL_HudRegisterShaderName( qhandle_t shader, const char *name ) {
	if ( !hudSupported || shader <= 0 || !name || !*name ) {
		return;
	}

	for ( int i = 0; i < hudNumShaderNames; i++ ) {
		if ( hudShaderNames[i].handle == shader ) {
			Q_strncpyz( hudShaderNames[i].name.data(), name, static_cast<int>( hudShaderNames[i].name.size() ) );
			return;
		}
	}

	if ( hudNumShaderNames >= kHudMaxShaderNames ) {
		return;
	}

	hudShaderNames[hudNumShaderNames].handle = shader;
	Q_strncpyz( hudShaderNames[hudNumShaderNames].name.data(), name, static_cast<int>( hudShaderNames[hudNumShaderNames].name.size() ) );
	hudNumShaderNames++;
}

extern "C" void CL_HudSetGamePanel( int panel ) {
	hudGamePanel = hudSupported ? panel : HUD_PANEL_NONE;
}

extern "C" void CL_HudModuleScreen( int *width, int *height ) {
	hudModuleWidth = 0;
	hudModuleHeight = 0;
	if ( !hudSupported || !width || !height || !cl_hudAspect || cl_hudAspect->integer <= 0
		|| !( cls.scale > 0.0f ) || cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0 ) {
		return;
	}
	const int frameWidth = static_cast<int>( std::lround( 640.0 * cls.scale ) );
	const int frameHeight = static_cast<int>( std::lround( 480.0 * cls.scale ) );
	if ( frameWidth <= 0 || frameHeight <= 0 ) {
		return;
	}
	hudModuleWidth = frameWidth;
	hudModuleHeight = frameHeight;
	*width = frameWidth;
	*height = frameHeight;
}

extern "C" void CL_HudAdjustRefdef( refdef_t *refdef ) {
	HudDrawCapture draw;
	float x;
	float y;
	float w;
	float h;

	if ( !hudSupported || !refdef || refdef->width <= 0 || refdef->height <= 0
		|| cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0 ) {
		return;
	}
	// The world view is the game's own unless the game was given the 4:3 frame.
	const bool world = ( refdef->rdflags & RDF_NOWORLDMODEL ) == 0;
	if ( world && !CL_HudModuleFramed() ) {
		return;
	}

	CL_HudUnstretch( static_cast<float>( refdef->x ), static_cast<float>( refdef->y ),
		static_cast<float>( refdef->width ), static_cast<float>( refdef->height ),
		&draw.x, &draw.y, &draw.w, &draw.h );
	draw.textLike = false;
	draw.refdef = true;

	if ( !world ) {
		CL_HudCaptureDraw( &draw );
		refdef->rdflags |= RDF_NOFOVCORRECTION;
	}

	x = draw.x;
	y = draw.y;
	w = draw.w;
	h = draw.h;
	if ( !cl_hudAspect || cl_hudAspect->integer <= 0 || hudGamePanel == HUD_PANEL_STRETCHED ) {
		// Retail's stretch: the game's own pixels, or its 4:3 frame stretched.
		if ( !CL_HudModuleFramed() ) return;
		CL_HudApplyStretch( &x, &y, &w, &h );
	} else if ( world ) {
		// A world view filling the frame fills the screen, and the renderer
		// widens its field of view (r_fovCorrection); a smaller one the game cut
		// from its 4:3 frame keeps that shape and the fields of view it was given.
		if ( CL_HudIsFullscreenLike( draw.x, draw.y, draw.w, draw.h ) ) {
			CL_HudApplyStretch( &x, &y, &w, &h );
		} else {
			CL_HudApplyUniform( &x, &y, &w, &h, HudAlignX::Center, HudAlignY::Middle );
			refdef->rdflags |= RDF_NOFOVCORRECTION;
		}
	} else {
		CL_HudEnsureRules();
		CL_HudTransformRect( &draw, &x, &y, &w, &h );
	}
	// A bad viewport must not cause undefined float-to-int conversions.
	const double rounded[] = { std::round( static_cast<double>( x ) ), std::round( static_cast<double>( y ) ),
		std::round( static_cast<double>( w ) ), std::round( static_cast<double>( h ) ) };
	for ( double coordinate : rounded ) {
		if ( !std::isfinite( coordinate ) || coordinate < INT_MIN || coordinate > INT_MAX ) return;
	}
	if ( rounded[2] < 1 || rounded[3] < 1 ) return;
	refdef->x = static_cast<int>( rounded[0] );
	refdef->y = static_cast<int>( rounded[1] );
	refdef->width = static_cast<int>( rounded[2] );
	refdef->height = static_cast<int>( rounded[3] );
}

extern "C" qboolean CL_HudAdjustPic( float *x, float *y, float *w, float *h, qhandle_t shader ) {
	HudDrawCapture draw;
	// The entity editor's view has no HUD over it, in any profile.
	if ( CL_EntityEditorActive() ) return qfalse;
	if ( !hudSupported ) return qtrue;

	if ( cl_captureActive && cl_captureActive->integer && r_levelshotHideHud && r_levelshotHideHud->integer ) {
		return qfalse;
	}
	if ( !x || !y || !w || !h || !std::isfinite( *x ) || !std::isfinite( *y )
		|| !std::isfinite( *w ) || !std::isfinite( *h ) ) return qfalse;
	if ( cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0 || cls.scale <= 0.0f ) return qtrue;

	CL_HudEnsureRules();
	CL_HudDescribeDraw( *x, *y, *w, *h, shader, &draw );
	if ( hudLineActive ) {
		draw.inLine = true;
		draw.lineX1 = hudLineX1;
		draw.lineY1 = hudLineY1;
		draw.lineX2 = hudLineX2;
		draw.lineY2 = hudLineY2;
	}
	CL_HudCaptureDraw( &draw );

	if ( !cl_hudAspect || cl_hudAspect->integer <= 0 || hudGamePanel == HUD_PANEL_STRETCHED ) {
		// Retail's stretch: the game's own pixels, or its 4:3 frame stretched.
		if ( !CL_HudModuleFramed() ) return qtrue;
		*x = draw.x;
		*y = draw.y;
		*w = draw.w;
		*h = draw.h;
		CL_HudApplyStretch( x, y, w, h );
	} else {
		CL_HudEnsureRules();
		CL_HudTransformRect( &draw, x, y, w, h );
	}
	return QBool( std::isfinite( *x ) && std::isfinite( *y ) && std::isfinite( *w ) && std::isfinite( *h ) );
}

extern "C" void CL_HudDrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader ) {
	if ( CL_HudAdjustPic( &x, &y, &w, &h, shader ) ) {
		re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
	}
}

// Places the held line as one: each glyph is matched by the whole line.
extern "C" void CL_HudFlushQueue( void ) {
	if ( hudQueueCount <= 0 ) {
		return;
	}
	float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
	bool first = true;
	for ( int i = 0; i < hudQueueCount; i++ ) {
		const HudDrawCapture &draw = hudQueue[i].draw;
		if ( hudQueue[i].color ) {
			continue;
		}
		x1 = first ? draw.x : std::min( x1, draw.x );
		y1 = first ? draw.y : std::min( y1, draw.y );
		x2 = first ? draw.x + draw.w : std::max( x2, draw.x + draw.w );
		y2 = first ? draw.y + draw.h : std::max( y2, draw.y + draw.h );
		first = false;
	}
	const int count = hudQueueCount;
	hudQueueCount = 0;
	for ( int i = 0; i < count; i++ ) {
		const HudQueuedOp &op = hudQueue[i];
		if ( op.color ) {
			re.SetColor( op.colorSet ? op.rgba.data() : nullptr );
			continue;
		}
		hudLineActive = true;
		hudLineX1 = x1;
		hudLineY1 = y1;
		hudLineX2 = x2;
		hudLineY2 = y2;
		CL_HudDrawStretchPic( op.x, op.y, op.w, op.h, op.s1, op.t1, op.s2, op.t2, op.shader );
		hudLineActive = false;
	}
}

// The client game's own pictures. When the rules place text by line, a glyph
// is held until its line is complete, with the colour changes between its
// glyphs (CL_HudQueueColor); anything else first submits the held line.
extern "C" void CL_HudQueueStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader ) {
	if ( CL_HudQueueing() && std::isfinite( x ) && std::isfinite( y ) && std::isfinite( w ) && std::isfinite( h ) ) {
		HudQueuedOp op;
		CL_HudDescribeDraw( x, y, w, h, shader, &op.draw );
		if ( op.draw.textLike && op.draw.w > 0.0f && op.draw.h > 0.0f ) {
			const HudQueuedOp *last = CL_HudLastQueuedGlyph();
			if ( hudQueueCount >= kHudMaxQueue || ( last && !CL_HudContinuesLine( last->draw, op.draw ) ) ) {
				CL_HudFlushQueue();
			}
			op.x = x;
			op.y = y;
			op.w = w;
			op.h = h;
			op.s1 = s1;
			op.t1 = t1;
			op.s2 = s2;
			op.t2 = t2;
			op.shader = shader;
			hudQueue[hudQueueCount++] = op;
			return;
		}
	}
	CL_HudFlushQueue();
	CL_HudDrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}

extern "C" void CL_HudQueueColor( const float *rgba ) {
	if ( hudQueueCount <= 0 ) {
		re.SetColor( rgba );
		return;
	}
	if ( hudQueueCount >= kHudMaxQueue ) {
		CL_HudFlushQueue();
		re.SetColor( rgba );
		return;
	}
	HudQueuedOp &op = hudQueue[hudQueueCount++];
	op = HudQueuedOp{};
	op.color = true;
	op.colorSet = rgba != nullptr;
	if ( rgba ) {
		std::copy( rgba, rgba + 4, op.rgba.begin() );
	}
}
