/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

The in-game entity editor

`entityeditor` opens an editor of the running map's entities over the game.
Every entity that stands somewhere is marked where it stands -- the nearest
as boxes in the colour of their kind with their classnames over them, the
rest as dots, a brush entity by its model's bounds -- and a line runs from
each entity to those it targets. Its interface is Dear ImGui's: a menu bar,
a list beside the view that filters and selects entities, one or many, a
panel that edits the selected entity's keys and values, and a status line.
Entities are moved by dragging them or their axis handles, alone or as a
selection, made, copied, pasted and deleted, and every change can be undone.
The result is written as the map's entity override, maps/<map>.ent, which
the server reads in place of the BSP's own entities (sv_entityOverrides,
sv_game.cpp); with that server in this process, Apply saves and restarts
the level, so that the game spawns what was edited.

While it is open the editor flies a camera of its own, and the world is
drawn from there (CL_EntityEditorAdjustView, which every profile's scene
adapter calls before it hands the renderer its view), so that neither
cheats nor the player's position limit where it looks; and at a field of
view of its own, which the renderer draws as given, so that its markers,
projected with the same view, lie on what they mark. What it draws over
the view, and its panels, are ImGui's triangles turned into the renderers'
2D quads and pictures (entity_editor_ui.hpp), so they look the same under
every renderer. It takes the keyboard and the mouse through a catcher bit
of its own, which no module can clear (Key_SetCatcher), and hands them to
ImGui; its own keys act when no ImGui text field has the keyboard.

The document and the entity text are kept apart from the engine
(entity_editor_model.hpp, entity_text.hpp), so that tests drive them alone.

When the server in this process runs the client's map with an override, the
client game's own entity tokens (the cgames that spawn entities of their
own, as Enemy Territory's and Jedi Academy's do) are read from the same
text (CL_GetEntityToken), so that both halves of the game see one level.
===========================================================================
*/

extern "C" {
#include "client.h"
}

#include "client_cpp.h"
#include "../qcommon/entity_text.hpp"

#include <string>

/* The client game's entity tokens, which builds without the editor's
 * interface keep too. */
namespace {

std::string cgameEntityText;
const char *cgameEntityParse = nullptr;
bool cgameEntityOverride = false;

} // namespace

#if defined( FNTECH3_ENTITY_EDITOR_UI )

#include "entity_editor_model.hpp"
#include "entity_editor_ui.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace ee = fntech3::entity_editor;
namespace ui = fntech3::entity_editor::ui;
namespace et = fntech3::entities;

using ee::Category;

cvar_t *cl_entityEditorGrid;
cvar_t *cl_entityEditorSnap;
cvar_t *cl_entityEditorLabels;
cvar_t *cl_entityEditorLabelDistance;
cvar_t *cl_entityEditorLinks;
cvar_t *cl_entityEditorPause;
cvar_t *cl_entityEditorSpeed;
cvar_t *cl_entityEditorScale;
cvar_t *cl_entityEditorFov;

/* Every line, fill and glyph the editor draws is a command in the renderer's
 * buffer (MAX_RENDER_COMMANDS, 512 KiB), which the game's frame and the
 * console fill too: a line is a 2D quad of up to 136 bytes, a fill or a glyph
 * a stretched picture of 48, a colour change 24 (64-bit sizes; 32-bit ones
 * are smaller). The view's overlay is bounded by count, about 220 KiB at
 * most however many entities it shows, and the editor's whole frame by
 * kFrameBytes: past it the overlay's links and farthest markers go first. */
constexpr int kQuadBytes = 136;
constexpr int kPicBytes = 48;
constexpr int kColorBytes = 24;
constexpr int kFrameBytes = 320 * 1024;
constexpr int kMaxBoxes = 48;		/* the nearest entities, as boxes */
constexpr int kMaxDots = 384;		/* the next, as dots */
constexpr int kMaxLabels = 32;
constexpr std::size_t kLabelChars = 40;
constexpr int kMaxLinks = 48;
/* Each upload of an ImGui picture spends one of the renderer's 2,048 image
 * slots until it restarts: the font's glyphs are baked before they are
 * first drawn, so a session needs a few, and this many stops a runaway. */
constexpr int kMaxTextureUploads = 32;
constexpr float kNear = 4.0f;
constexpr float kDragThreshold = 4.0f;
constexpr float kGridSizes[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f };

/* ---- the view ------------------------------------------------------------ */

struct View {
	bool valid = false;
	vec3_t origin = { 0.0f, 0.0f, 0.0f };
	vec3_t axis[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	float tanX = 1.0f;
	float tanY = 0.75f;
	float x = 0.0f, y = 0.0f, width = 640.0f, height = 480.0f;
};

void ViewFromRefdef( const refdef_t &refdef, View &view )
{
	view.valid = true;
	VectorCopy( refdef.vieworg, view.origin );
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( refdef.viewaxis[i], view.axis[i] );
	}
	const float fovX = refdef.fov_x > 1.0f && refdef.fov_x < 179.0f ? refdef.fov_x : 90.0f;
	const float fovY = refdef.fov_y > 1.0f && refdef.fov_y < 179.0f ? refdef.fov_y : 73.74f;
	view.tanX = std::tan( DEG2RAD( fovX ) * 0.5f );
	view.tanY = std::tan( DEG2RAD( fovY ) * 0.5f );
	view.x = static_cast<float>( refdef.x );
	view.y = static_cast<float>( refdef.y );
	view.width = static_cast<float>( std::max( refdef.width, 1 ) );
	view.height = static_cast<float>( std::max( refdef.height, 1 ) );
}

/* The editor's field of view, which the renderer is told to draw as it is
 * (RDF_NOFOVCORRECTION), so that what the overlay projects with it lands on
 * what the renderer draws: the game's own, which r_fovCorrection then widens
 * after the editor has projected with it, and which a zoom or an underwater
 * warp changes, is not the editor's. cl_entityEditorFov is its width on a 4:3
 * screen; on any other, its height is kept and its width fitted, as
 * r_fovCorrection fits a game's (ee::FitFieldOfView). The shape fitted is the
 * one the screen shows: a render stretched to the window shows the window's
 * (glconfig.windowAspect); r_renderScale 2 and 4 keep the render's own. */
void EditorFov( int width, int height, float &fovX, float &fovY )
{
	const float configured = cl_entityEditorFov ? cl_entityEditorFov->value : 90.0f;
	const double horizontal = std::clamp( std::isfinite( configured ) ? configured : 90.0f, 10.0f, 140.0f );
	double aspect = static_cast<double>( std::max( width, 1 ) ) / static_cast<double>( std::max( height, 1 ) );
	const int renderScale = Cvar_VariableIntegerValue( "r_renderScale" );
	const bool keepsShape = Cvar_VariableIntegerValue( "r_fbo" ) && ( renderScale == 2 || renderScale == 4 );
	const double windowAspect = cls.glconfig.windowAspect;
	if ( !keepsShape && std::isfinite( windowAspect ) && windowAspect > 0.0 && cls.glconfig.vidWidth > 0
		&& cls.glconfig.vidHeight > 0 ) {
		aspect *= windowAspect * static_cast<double>( cls.glconfig.vidHeight ) / static_cast<double>( cls.glconfig.vidWidth );
	}
	ee::FitFieldOfView( horizontal, aspect, fovX, fovY );
}

struct ScreenPoint {
	float x = 0.0f, y = 0.0f, depth = 0.0f;
};

bool Project( const View &view, const vec3_t point, ScreenPoint &out )
{
	vec3_t local;
	VectorSubtract( point, view.origin, local );
	const float depth = DotProduct( local, view.axis[0] );
	if ( !( depth >= kNear - 0.01f ) ) {
		return false;
	}
	const float side = DotProduct( local, view.axis[1] );
	const float up = DotProduct( local, view.axis[2] );
	out.x = view.x + view.width * 0.5f * ( 1.0f - side / ( depth * view.tanX ) );
	out.y = view.y + view.height * 0.5f * ( 1.0f - up / ( depth * view.tanY ) );
	out.depth = depth;
	if ( !std::isfinite( out.x ) || !std::isfinite( out.y ) ) {
		return false;
	}
	/* far off the screen still draws as a finite quad */
	out.x = std::clamp( out.x, -65536.0f, 65536.0f );
	out.y = std::clamp( out.y, -65536.0f, 65536.0f );
	return true;
}

/* A segment cut at the near plane, then projected. */
bool ProjectSegment( const View &view, const vec3_t a, const vec3_t b, ScreenPoint &pa, ScreenPoint &pb )
{
	vec3_t la, lb;
	VectorSubtract( a, view.origin, la );
	VectorSubtract( b, view.origin, lb );
	const float da = DotProduct( la, view.axis[0] ), db = DotProduct( lb, view.axis[0] );
	if ( da < kNear && db < kNear ) {
		return false;
	}
	vec3_t ca, cb;
	VectorCopy( a, ca );
	VectorCopy( b, cb );
	if ( da < kNear ) {
		const float t = ( kNear - da ) / ( db - da );
		for ( int i = 0; i < 3; i++ ) ca[i] = a[i] + ( b[i] - a[i] ) * t;
	} else if ( db < kNear ) {
		const float t = ( kNear - db ) / ( da - db );
		for ( int i = 0; i < 3; i++ ) cb[i] = b[i] + ( a[i] - b[i] ) * t;
	}
	return Project( view, ca, pa ) && Project( view, cb, pb );
}

/* The direction from the view's origin through a pixel. */
void PixelRay( const View &view, float sx, float sy, vec3_t direction )
{
	const float nx = ( sx - view.x - view.width * 0.5f ) / ( view.width * 0.5f );
	const float ny = ( sy - view.y - view.height * 0.5f ) / ( view.height * 0.5f );
	VectorCopy( view.axis[0], direction );
	VectorMA( direction, -nx * view.tanX, view.axis[1], direction );
	VectorMA( direction, -ny * view.tanY, view.axis[2], direction );
	VectorNormalize( direction );
}

bool RayBox( const vec3_t start, const vec3_t direction, const vec3_t mins, const vec3_t maxs, float &distance )
{
	float nearest = 0.0f, farthest = 1.0e9f;
	for ( int i = 0; i < 3; i++ ) {
		if ( std::fabs( direction[i] ) < 1.0e-6f ) {
			if ( start[i] < mins[i] || start[i] > maxs[i] ) {
				return false;
			}
			continue;
		}
		float t1 = ( mins[i] - start[i] ) / direction[i];
		float t2 = ( maxs[i] - start[i] ) / direction[i];
		if ( t1 > t2 ) {
			std::swap( t1, t2 );
		}
		nearest = std::max( nearest, t1 );
		farthest = std::min( farthest, t2 );
		if ( nearest > farthest ) {
			return false;
		}
	}
	distance = nearest;
	return true;
}

/* How far a pixel is from a segment on the screen. */
float SegmentDistance( float px, float py, const ScreenPoint &a, const ScreenPoint &b )
{
	const float dx = b.x - a.x, dy = b.y - a.y;
	const float lengthSquared = dx * dx + dy * dy;
	float t = lengthSquared > 0.0f ? ( ( px - a.x ) * dx + ( py - a.y ) * dy ) / lengthSquared : 0.0f;
	t = std::clamp( t, 0.0f, 1.0f );
	const float cx = a.x + dx * t - px, cy = a.y + dy * t - py;
	return std::sqrt( cx * cx + cy * cy );
}

/* ---- markers: what the view shows of each entity -------------------------- */

struct Marker {
	std::uint32_t id = 0;
	int index = 0;
	bool placed = false;		/* stands somewhere: an origin, or a brush model */
	bool brush = false;
	vec3_t point = { 0.0f, 0.0f, 0.0f };	/* its origin, or its model's centre */
	vec3_t offset = { 0.0f, 0.0f, 0.0f };	/* a brush entity's origin key */
	vec3_t mins = { 0.0f, 0.0f, 0.0f };	/* its box in the world */
	vec3_t maxs = { 0.0f, 0.0f, 0.0f };
	Category category = Category::Other;
	ui::Color color;
	bool hasYaw = false;
	float yaw = 0.0f;
	int cluster = -1;
	std::string classname;
	std::string targetname;
};

ui::Color CategoryColor( Category category )
{
	switch ( category ) {
	case Category::World: return { 0.70f, 0.70f, 0.70f, 1.0f };
	case Category::Spawn: return { 0.35f, 0.95f, 0.45f, 1.0f };
	case Category::Light: return { 1.00f, 0.86f, 0.32f, 1.0f };
	case Category::Item: return { 0.32f, 0.86f, 1.00f, 1.0f };
	case Category::Trigger: return { 1.00f, 0.56f, 0.22f, 1.0f };
	case Category::Logic: return { 0.86f, 0.48f, 1.00f, 1.0f };
	case Category::Brush: return { 0.46f, 0.62f, 1.00f, 1.0f };
	case Category::Misc: return { 0.88f, 0.88f, 0.88f, 1.0f };
	case Category::Other: return { 0.66f, 0.66f, 0.66f, 1.0f };
	}
	return { 0.66f, 0.66f, 0.66f, 1.0f };
}

/* How the running game stands an entity about its origin. */
ee::Lineage GameLineage()
{
	if ( Com_GameIsRitual() || Com_GameIsMOHAA() ) {
		return ee::Lineage::Ritual;
	}
	if ( Com_GameIsRTCW() || Com_GameIsWolfET() ) {
		return ee::Lineage::Wolfenstein;
	}
	return ee::Lineage::Quake3;
}

/* A point entity's box about its origin, as its game stands it
 * (ee::PointExtent). */
void MarkerExtent( const et::Entity &entity, vec3_t mins, vec3_t maxs )
{
	const ee::Extent extent = ee::PointExtent( GameLineage(), entity );
	VectorCopy( extent.mins, mins );
	VectorCopy( extent.maxs, maxs );
}

bool Finite( const vec3_t v )
{
	return std::isfinite( v[0] ) && std::isfinite( v[1] ) && std::isfinite( v[2] );
}

/* ---- the editor ------------------------------------------------------------ */

enum class Drag { None, AxisX, AxisY, AxisZ, Plane, PendingPlane };
enum class StatusKind { Info, Good, Warning, Error };

struct Layout {
	ui::Rect screen, menu, status, list, props, viewport;
};

/* A selected entity a drag moves, as it stood when the drag began. */
struct DragMember {
	std::uint32_t id = 0;
	bool brush = false;
	vec3_t point = { 0.0f, 0.0f, 0.0f };
	vec3_t offset = { 0.0f, 0.0f, 0.0f };
};

/* A key pressed for the editor's own keys, which act once ImGui has had the
 * frame's input and wants no text. */
struct Shortcut {
	int key = 0;
	bool shift = false;
	bool ctrl = false;
};

struct Editor {
	bool active = false;
	/* the document */
	ee::Document doc;
	std::string mapPath;			/* maps/<map>, the document's map */
	std::string savePath;			/* the override it is written to */
	std::string source;				/* where it was read from */
	bool local = false;				/* the server in this process runs the map */
	int generation = -1;
	std::string parseNote;
	std::string savedText;			/* what the last save wrote */
	/* the markers, for the document's revision */
	std::uint64_t markersRevision = ~0ull;
	std::vector<Marker> markers;
	std::unordered_map<std::uint32_t, int> markerOf;
	std::vector<std::pair<int, int>> links;
	/* the list */
	std::string filter;
	std::string filterUsed;
	std::uint64_t filterRevision = ~0ull;
	std::vector<std::uint32_t> filtered;
	std::uint32_t anchorId = 0;		/* where a Shift+click range starts */
	bool scrollToSelection = false;
	bool focusFilter = false;
	bool showList = true;
	bool showProps = true;
	/* the camera */
	vec3_t cameraOrigin = { 0.0f, 0.0f, 0.0f };
	vec3_t cameraAngles = { 0.0f, 0.0f, 0.0f };
	bool cameraSet = false;
	std::string cameraMap;			/* the map the camera was placed in */
	float speedScale = 1.0f;
	bool looking = false;			/* the right button turns the view */
	bool flying = false;			/* Tab: the mouse turns it until Tab again */
	int cameraFrame = -1;
	int cameraTime = 0;
	std::array<bool, MAX_KEYS> keyDown{};
	View view;						/* the view as drawn, from the camera */
	View playerView;				/* the view as the game last set it */
	/* the pointer and the editor's keys */
	float cursorX = 0.0f, cursorY = 0.0f;
	bool cursorSet = false;
	std::vector<Shortcut> shortcuts;
	/* a drag in the view, and a box drawn to select */
	Drag drag = Drag::None;
	std::uint32_t dragId = 0;
	vec3_t dragStart = { 0.0f, 0.0f, 0.0f };	/* the dragged marker's point when it began */
	vec3_t dragGrab = { 0.0f, 0.0f, 0.0f };
	float dragParam = 0.0f;
	float dragPressX = 0.0f, dragPressY = 0.0f;
	std::vector<DragMember> dragMembers;
	bool pressSelectsAlone = false;	/* a plain press on one of several selected: alone unless dragged */
	bool boxPending = false;		/* a press on empty view, not yet a box */
	bool boxing = false;
	bool boxAdds = false;			/* Ctrl or Shift held: the box adds */
	float boxX0 = 0.0f, boxY0 = 0.0f, boxX1 = 0.0f, boxY1 = 0.0f;
	std::uint32_t hoverId = 0;
	int hoverAxis = -1;				/* 0..2 an axis handle, 3 the centre */
	/* panels and prompts */
	bool help = false;
	bool openNewPrompt = false;
	std::string newClassname;
	std::string newKey;
	std::string newValue;
	std::string allKey;				/* a key set on every selected entity */
	std::string allValue;
	bool focusNewKey = false;
	/* the edit box with the keyboard: its text, and what keeps it */
	ImGuiID fieldId = 0;
	std::string fieldBuffer;
	std::function<void( const std::string & )> fieldCommit;
	bool fieldSeen = false;
	std::string fieldScratch;
	/* what the panels asked for, done once they are drawn */
	std::vector<std::function<void()>> actions;
	/* status line */
	std::string status;
	StatusKind statusKind = StatusKind::Info;
	int statusTime = 0;
	/* pause and apply */
	bool pausedByEditor = false;
	bool applyPending = false;
	int applyGeneration = 0;
	std::string applyPath;
	int repauseAt = 0;
	/* a reload asked of another server over rcon, and what its replies said */
	bool remotePending = false;
	int remoteAskedAt = 0;
	std::string remotePath;
	std::string remoteHeard;
	/* ImGui */
	ImGuiContext *imgui = nullptr;
	float uiScale = 0.0f;			/* the scale the style was built for */
	bool prewarm = true;			/* the font's glyphs are baked next frame */
	int frameTime = 0;
	int textureUploads = 0;			/* this renderer's */
	int textureSerial = 0;
	bool textureLimitNoted = false;
	std::string clipboard;
	/* rendering state */
	float lastColor[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
	ui::Rect viewport;				/* what the view's overlay is drawn within */
	ImDrawList *overlay = nullptr;	/* the frame's background list, under the panels */
	ui::DrawList viewDraw;			/* the overlay, drawn before the panels */
	ui::DrawList panelDraw;
	int frameBytes = 0;				/* the last frame's commands, estimated */
	int frameCommands = 0;
	int frameDropped = 0;			/* what kFrameBytes left out of it */
	int markersShown[2] = {};		/* boxes and dots in the last frame */
};

Editor editor;

/* ---- helpers --------------------------------------------------------------- */

void SetStatus( StatusKind kind, const std::string &message )
{
	editor.status = message;
	editor.statusKind = kind;
	editor.statusTime = cls.realtime;
	if ( kind == StatusKind::Error || kind == StatusKind::Warning ) {
		Com_Printf( "%sEntity editor: %s\n", kind == StatusKind::Error ? S_COLOR_RED : S_COLOR_YELLOW,
			message.c_str() );
	} else {
		Com_Printf( "Entity editor: %s\n", message.c_str() );
	}
}

std::string Format( const char *format, ... ) FORMAT_PRINTF( 1, 2 );
std::string Format( const char *format, ... )
{
	char buffer[1024];
	va_list args;
	va_start( args, format );
	Q_vsnprintf( buffer, sizeof( buffer ), format, args );
	va_end( args );
	return buffer;
}

float Grid()
{
	return cl_entityEditorGrid ? std::max( 0.0f, cl_entityEditorGrid->value ) : 8.0f;
}

bool Snapping()
{
	return cl_entityEditorSnap && cl_entityEditorSnap->integer && Grid() > 0.0f;
}

float SnapValue( float value )
{
	return Snapping() ? ee::SnapToGrid( value, Grid() ) : std::floor( value * 8.0f + 0.5f ) / 8.0f;
}

/* What a module limits an entity to, where its source is public:
 * MAX_SPAWN_VARS and MAX_SPAWN_VARS_CHARS of id's Quake III and its
 * successors' g_local.h (Return to Castle Wolfenstein, Enemy Territory,
 * Elite Force, and OpenJK's Jedi games). Ritual's and 2015's modules parse
 * with no such table. */
ee::SpawnLimits Limits()
{
	const comGameProfile_t *profile = Com_GameProfile();
	if ( !profile ) {
		return {};
	}
	switch ( profile->id ) {
	case COM_GAME_QUAKE3:
	case COM_GAME_QUAKELIVE:
	case COM_GAME_JKA_MP:
		return { 64, 4096 };
	case COM_GAME_RTCW_SP:
	case COM_GAME_RTCW_MP:
	case COM_GAME_WOLFET:
	case COM_GAME_STVEF_SP:
	case COM_GAME_STVEF_MP:
	case COM_GAME_JKA_SP:
	case COM_GAME_JK2_SP:
		return { 64, 2048 };
	default:
		return {};
	}
}

/* The map the client holds: maps/<map>, from its BSP's name. */
std::string ClientMapPath()
{
	std::string path = cl.mapname;
	if ( path.size() > 4 && !Q_stricmp( path.c_str() + path.size() - 4, ".bsp" ) ) {
		path.resize( path.size() - 4 );
	}
	return path;
}

/* maps/<map>.ent for the server's map or, with no server here, for the
 * client's; empty when its name makes no safe path. */
std::string OverridePathFor( const std::string &mapPath )
{
	if ( mapPath.empty() || mapPath.find( ".." ) != std::string::npos || mapPath.find( ':' ) != std::string::npos
		|| mapPath.find( '\\' ) != std::string::npos || mapPath.size() + 4 >= MAX_QPATH ) {
		return {};
	}
	for ( const char c : mapPath ) {
		if ( static_cast<unsigned char>( c ) < 0x20u || c == 0x7f ) {
			return {};
		}
	}
	return mapPath + ".ent";
}

bool ServerRunsMap()
{
	char path[MAX_QPATH];
	return com_sv_running && com_sv_running->integer && SV_EntityOverrideFile( path, sizeof( path ) );
}

/* maps/<map> for the level running here: the local server's, else the
 * client's. */
std::string CurrentMapPath()
{
	char path[MAX_QPATH];
	if ( com_sv_running && com_sv_running->integer && SV_EntityOverrideFile( path, sizeof( path ) ) ) {
		std::string map = path;
		map.resize( map.size() - 4 );
		return map;
	}
	return ClientMapPath();
}

/* The edit box being typed in keeps its text now, as a save, an apply or a
 * console command must see it. */
void CommitField()
{
	if ( editor.fieldId && editor.fieldCommit ) {
		const std::function<void( const std::string & )> commit = editor.fieldCommit;
		commit( editor.fieldBuffer );
	}
}

/* ---- the document -------------------------------------------------------- */

void ApplyParsed( const et::ParseResult &parsed, const std::string &from )
{
	editor.doc.Load( parsed.entities );
	editor.parseNote.clear();
	if ( !parsed.ok ) {
		editor.parseNote = Format( "%s, line %d: %s", from.c_str(), parsed.errorLine, parsed.error.c_str() );
	} else if ( parsed.truncated ) {
		editor.parseNote = from + ": a token is longer than the 1023 bytes games keep";
	}
	editor.markersRevision = ~0ull;
	editor.filterRevision = ~0ull;
	editor.anchorId = 0;
}

/* The running level's entities: the local server's text, or with no server
 * here, the client's map's override file, else its BSP's own. */
void LoadDocument()
{
	editor.local = ServerRunsMap();
	std::string text;
	if ( editor.local ) {
		char path[MAX_QPATH];
		SV_EntityOverrideFile( path, sizeof( path ) );
		const char *overridePath = SV_EntityOverridePath();
		editor.savePath = overridePath[0] ? overridePath : path;
		editor.mapPath = path;
		if ( editor.mapPath.size() > 4 ) {
			editor.mapPath.resize( editor.mapPath.size() - 4 );
		}
		editor.source = overridePath[0] ? overridePath : "the BSP";
		text = SV_EntityText();
		editor.generation = SV_EntityTextGeneration();
	} else {
		editor.mapPath = ClientMapPath();
		editor.savePath = OverridePathFor( editor.mapPath );
		editor.source = "the BSP";
		void *data = nullptr;
		const int length = editor.savePath.empty() ? -1 : FS_ReadFile( editor.savePath.c_str(), &data );
		if ( length > 0 && data ) {
			const std::string_view file( static_cast<const char *>( data ), static_cast<std::size_t>( length ) );
			if ( file.find( '\0' ) == std::string_view::npos && et::Parse( file ).ok ) {
				text.assign( file );
				editor.source = editor.savePath;
			}
		}
		if ( data ) {
			FS_FreeFile( data );
		}
		if ( editor.source != editor.savePath ) {
			const char *bsp = CM_EntityString();
			text = bsp ? bsp : "";
		}
		editor.generation = -1;
	}
	ApplyParsed( et::Parse( text ), editor.source );
	editor.savedText = editor.doc.Text();
	editor.drag = Drag::None;
	editor.boxPending = editor.boxing = false;
	editor.hoverId = 0;
}

/* The text written whole into the home directory, or false. */
bool WriteText( const std::string &path, const std::string &text )
{
	const fileHandle_t file = FS_FOpenFileWrite( path.c_str() );
	if ( file == FS_INVALID_HANDLE ) {
		return false;
	}
	const int length = static_cast<int>( text.size() );
	const int written = length ? FS_Write( text.data(), length, file ) : 0;
	FS_FCloseFile( file );
	return written == length;
}

bool Save()
{
	/* a box still being typed in counts */
	CommitField();
	if ( editor.savePath.empty() ) {
		SetStatus( StatusKind::Error, "this map's name makes no safe path for an override" );
		return false;
	}
	const std::string text = editor.doc.Text();
	if ( !WriteText( editor.savePath, text ) ) {
		SetStatus( StatusKind::Error, Format( "could not write %s; nothing was saved", editor.savePath.c_str() ) );
		return false;
	}
	editor.savedText = text;
	editor.doc.MarkSaved();
	int errors = 0;
	const ee::SpawnLimits limits = Limits();
	for ( std::size_t i = 0; i < editor.doc.Count(); i++ ) {
		for ( const ee::Finding &finding : ee::CheckEntity( editor.doc.Items()[i].entity, limits, i == 0 ) ) {
			errors += finding.error ? 1 : 0;
		}
	}
	if ( errors ) {
		SetStatus( StatusKind::Warning, Format( "saved %s (%d bytes); %d problem%s a game would refuse",
			editor.savePath.c_str(), static_cast<int>( text.size() ), errors, errors == 1 ? "" : "s" ) );
	} else {
		SetStatus( StatusKind::Good, Format( "saved %s (%d bytes, %d entities)", editor.savePath.c_str(),
			static_cast<int>( text.size() ), static_cast<int>( editor.doc.Count() ) ) );
	}
	return true;
}

/* The command that loads the running map again from its start, as the
 * server started it (SV_ReloadMapCommand). */
std::string ReloadCommand()
{
	char command[MAX_QPATH + 32];
	if ( SV_ReloadMapCommand( command, sizeof( command ) ) ) {
		return command;
	}
	return Format( "%s %s\n", Cvar_VariableIntegerValue( "sv_cheats" ) ? "devmap" : "map",
		Cvar_VariableString( "mapname" ) );
}

/* Whether the restart is Quake III's map_restart, which respawns the level's
 * entities while the client game runs on; Ritual's and Medal of Honor's
 * restart and Elite Force's single player's map spawn the level anew. Q3Test's
 * retail engine has map_restart from 1.06 (its quake3.exe and qagamex86.dll
 * name it from then); before, its modules never met one. */
bool RestartKeepsClientGame()
{
	if ( Com_GameIsQ3Test() ) {
		const q3testRelease_t *release = Com_Q3TestRelease();
		return release && release->id >= Q3TEST_RELEASE_1_06;
	}
	return !Com_GameIsRitual() && !Com_GameIsMOHAA() && !Com_GameIsSTVEFSP();
}

/* The command that respawns the level's entities from the text the server
 * selects: Quake III's map_restart, Ritual's and Medal of Honor's restart,
 * and the map again for Elite Force's single player and Q3Test before 1.06,
 * which have neither. */
std::string RestartCommand()
{
	if ( Com_GameIsRitual() || Com_GameIsMOHAA() ) {
		return "restart\n";
	}
	if ( !RestartKeepsClientGame() ) {
		return ReloadCommand();
	}
	return "map_restart 0\n";
}

void Unpause()
{
	if ( editor.pausedByEditor ) {
		if ( cl_paused && cl_paused->integer ) {
			Cvar_Set( "cl_paused", "0" );
		}
		editor.pausedByEditor = false;
	}
}

void Pause()
{
	if ( !editor.pausedByEditor && cl_entityEditorPause && cl_entityEditorPause->integer
		&& com_sv_running && com_sv_running->integer && cl_paused && !cl_paused->integer
		&& Cvar_VariableIntegerValue( "sv_maxclients" ) <= 1 ) {
		Cvar_Set( "cl_paused", "1" );
		editor.pausedByEditor = true;
	}
}

/* Another server can be asked to apply only over rcon: this client holds its
 * password, and the server is the one played on, not a demo's. */
bool RemoteApplyAvailable()
{
	return !editor.local && cls.state == CA_ACTIVE && !clc.demoplaying
		&& Cvar_VariableString( "rconPassword" )[0] != '\0';
}

/* What another server is asked to run: the map again with cheats, as the
 * editor needs them there (map_restart would keep a client game's per-entity
 * state, so a full load is the one safe for every edit), or Ritual's and
 * Medal of Honor's restart, which loads the level anew. Empty for a map name
 * that would not survive the trip as one argument. */
std::string RemoteReloadCommand()
{
	if ( Com_GameIsRitual() || Com_GameIsMOHAA() ) {
		return "restart";
	}
	std::string map = editor.mapPath;
	if ( map.size() > 5 && !Q_stricmpn( map.c_str(), "maps/", 5 ) ) {
		map.erase( 0, 5 );
	}
	if ( map.empty() || map.find_first_not_of( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-/." )
		!= std::string::npos ) {
		return std::string();
	}
	return "devmap " + map;
}

/* On another server the editor's own map command cannot run there; with its
 * rcon password the server is asked to load the map again, and its reply says
 * whether it read the saved file -- which it does when it shares this home,
 * or once the file is copied to its own (CL_EntityEditorServerPrint). */
void StartRemoteLevelChange()
{
	if ( !RemoteApplyAvailable() ) {
		SetStatus( StatusKind::Error, "applying needs the map running on this machine's server (map or devmap), "
			"or rconPassword for the server you are on" );
		return;
	}
	const std::string command = RemoteReloadCommand();
	if ( command.empty() ) {
		SetStatus( StatusKind::Error, "this map's name cannot be sent to the server as one argument" );
		return;
	}
	if ( !Save() ) {
		return;
	}
	editor.remotePending = true;
	editor.remoteAskedAt = cls.realtime;
	editor.remotePath = editor.savePath;
	editor.remoteHeard.clear();
	SetStatus( StatusKind::Info, Format( "asked the server over rcon: %s", command.c_str() ) );
	Com_Printf( "Entity editor: asked the server over rcon: %s\n", command.c_str() );
	/* next, before anything a script queued after the command that asked */
	Cbuf_ExecuteText( EXEC_INSERT, ( "rcon " + command + "\n" ).c_str() );
}

void StartLevelChange( const std::string &command, const char *what )
{
	if ( !editor.local ) {
		StartRemoteLevelChange();
		return;
	}
	if ( !Save() ) {
		return;
	}
	const bool fromOverrideDir = SV_EntityOverridePath()[0] && editor.savePath == SV_EntityOverridePath();
	if ( !fromOverrideDir && !Cvar_VariableIntegerValue( "sv_entityOverrides" ) ) {
		SetStatus( StatusKind::Warning, "saved, but sv_entityOverrides is 0, so the server reads the BSP's entities" );
		return;
	}
	const bool wasPaused = editor.pausedByEditor;
	Unpause();
	editor.applyPending = true;
	editor.applyGeneration = SV_EntityTextGeneration();
	editor.applyPath = editor.savePath;
	editor.repauseAt = wasPaused ? -1 : 0;
	SetStatus( StatusKind::Info, Format( "%s: %s", what, command.substr( 0, command.size() - 1 ).c_str() ) );
	/* next, before anything a script queued after the command that asked */
	Cbuf_ExecuteText( EXEC_INSERT, command.c_str() );
}

void Apply()
{
	/* map_restart keeps the client game's state for each entity number,
	 * which entities added, removed, reordered or spawned differently leave
	 * stale -- Return to Castle Wolfenstein's single player then spins in its
	 * animation code -- so such a change loads the map again */
	CommitField();
	if ( editor.local && RestartKeepsClientGame()
		&& !ee::SpawnsAlike( et::Parse( SV_EntityText() ).entities, editor.doc.Entities() ) ) {
		StartLevelChange( ReloadCommand(), "the entities to spawn changed, so the map loads again" );
		return;
	}
	StartLevelChange( RestartCommand(), "restarting the level" );
}

void ReloadMap()
{
	StartLevelChange( ReloadCommand(), "reloading the map" );
}

/* Back to what the override or the BSP held when last read: one change. */
void Revert()
{
	std::string text;
	if ( editor.local ) {
		text = SV_EntityText();
	} else {
		text = editor.savedText;
	}
	const et::ParseResult parsed = et::Parse( text );
	editor.doc.Replace( parsed.entities, "revert" );
	editor.markersRevision = ~0ull;
	SetStatus( StatusKind::Info, "reverted to the level's entities (Ctrl+Z undoes it)" );
}

/* The BSP's own entities in place of the override's: one change. */
void LoadBspEntities()
{
	const char *bsp = CM_EntityString();
	const et::ParseResult parsed = et::Parse( bsp ? bsp : "" );
	editor.doc.Replace( parsed.entities, "BSP entities" );
	editor.markersRevision = ~0ull;
	SetStatus( StatusKind::Info, "the BSP's own entities are loaded; Save writes them as the override (Ctrl+Z undoes it)" );
}

/* The server chose its text again: a restart, a map change, or the apply
 * this editor started. */
void FollowServer()
{
	if ( !editor.local ) {
		if ( cls.state == CA_ACTIVE && ClientMapPath() != editor.mapPath ) {
			LoadDocument();
			editor.cameraSet = false;	/* from the new map's player view */
		}
		return;
	}
	if ( !ServerRunsMap() ) {
		return;
	}
	const int generation = SV_EntityTextGeneration();
	if ( generation == editor.generation ) {
		return;
	}
	editor.generation = generation;
	char path[MAX_QPATH];
	SV_EntityOverrideFile( path, sizeof( path ) );
	std::string mapPath = path;
	if ( mapPath.size() > 4 ) {
		mapPath.resize( mapPath.size() - 4 );
	}

	if ( editor.applyPending ) {
		editor.applyPending = false;
		const char *loaded = SV_EntityOverridePath();
		if ( editor.applyPath == loaded ) {
			SetStatus( StatusKind::Good, Format( "the level restarted with %s", loaded ) );
		} else {
			SetStatus( StatusKind::Error, Format( "the server did not load %s; see the console", editor.applyPath.c_str() ) );
		}
		if ( editor.repauseAt < 0 ) {
			editor.repauseAt = cls.realtime + 750;
		}
	}

	if ( mapPath != editor.mapPath ) {
		if ( editor.doc.Dirty() && !editor.savePath.empty() ) {
			const std::string recovery = editor.savePath + ".unsaved";
			if ( WriteText( recovery, editor.doc.Text() ) ) {
				SetStatus( StatusKind::Warning, Format( "the map changed; unsaved edits were written to %s", recovery.c_str() ) );
			} else {
				SetStatus( StatusKind::Error, Format( "the map changed and %s could not be written: the edits are lost",
					recovery.c_str() ) );
			}
		}
		LoadDocument();
		editor.cameraSet = false;	/* from the new map's player view */
		return;
	}
	const char *overridePath = SV_EntityOverridePath();
	editor.source = overridePath[0] ? overridePath : "the BSP";
	if ( overridePath[0] ) {
		editor.savePath = overridePath;
	}
	/* the same level again: keep the edits and their history unless the
	 * text it now runs differs and nothing here is unsaved */
	const et::ParseResult parsed = et::Parse( SV_EntityText() );
	if ( !editor.doc.Holds( parsed.entities ) && !editor.doc.Dirty() ) {
		std::vector<int> selectedIndexes;
		for ( const std::uint32_t id : editor.doc.Selection() ) {
			selectedIndexes.push_back( editor.doc.IndexOf( id ) );
		}
		const int primary = editor.doc.IndexOf( editor.doc.Selected() );
		ApplyParsed( parsed, editor.source );
		std::vector<std::uint32_t> ids;
		for ( const int index : selectedIndexes ) {
			if ( index >= 0 && index < static_cast<int>( editor.doc.Count() ) ) {
				ids.push_back( editor.doc.Items()[static_cast<std::size_t>( index )].id );
			}
		}
		editor.doc.SetSelection( ids, primary >= 0 && primary < static_cast<int>( editor.doc.Count() )
			? editor.doc.Items()[static_cast<std::size_t>( primary )].id : 0 );
		editor.savedText = editor.doc.Text();
	}
}

/* ---- markers --------------------------------------------------------------- */

void RebuildMarkers()
{
	if ( editor.markersRevision == editor.doc.Revision() && !editor.markers.empty() ) {
		return;
	}
	editor.markersRevision = editor.doc.Revision();
	editor.markers.clear();
	editor.markerOf.clear();
	editor.links.clear();
	const int numModels = CM_NumInlineModels();
	std::unordered_map<std::string, std::vector<int>> named;
	const auto &items = editor.doc.Items();
	editor.markers.reserve( items.size() );
	for ( std::size_t i = 0; i < items.size(); i++ ) {
		const et::Entity &entity = items[i].entity;
		Marker marker;
		marker.id = items[i].id;
		marker.index = static_cast<int>( i );
		marker.category = ee::Classify( entity );
		marker.classname.assign( entity.Classname() );
		marker.targetname.assign( entity.Value( "targetname" ) );
		marker.color = CategoryColor( marker.category );
		vec3_t origin;
		/* an origin no number holds (nan, inf) stands nowhere */
		const bool hasOrigin = ee::EntityOrigin( entity, origin ) && Finite( origin );
		const int model = ee::BrushModelNumber( entity );
		if ( model > 0 && model < numModels && ( hasOrigin || !entity.Has( "origin" ) ) ) {
			vec3_t mins, maxs;
			CM_ModelBounds( CM_InlineModel( model ), mins, maxs );
			/* the collision map spreads each brush model's bounds by a unit
			 * (CMod_LoadSubmodels); the box is the brushes' own */
			for ( int axis = 0; axis < 3; axis++ ) {
				if ( maxs[axis] - mins[axis] > 2.0f ) {
					mins[axis] += 1.0f;
					maxs[axis] -= 1.0f;
				}
			}
			if ( hasOrigin ) {
				VectorCopy( origin, marker.offset );
			}
			VectorAdd( mins, marker.offset, marker.mins );
			VectorAdd( maxs, marker.offset, marker.maxs );
			for ( int axis = 0; axis < 3; axis++ ) {
				marker.point[axis] = ( marker.mins[axis] + marker.maxs[axis] ) * 0.5f;
			}
			marker.brush = true;
			marker.placed = Finite( marker.mins ) && Finite( marker.maxs );
		} else if ( hasOrigin && i > 0 ) {
			vec3_t mins, maxs;
			MarkerExtent( entity, mins, maxs );
			VectorCopy( origin, marker.point );
			VectorAdd( origin, mins, marker.mins );
			VectorAdd( origin, maxs, marker.maxs );
			marker.placed = Finite( marker.mins ) && Finite( marker.maxs );
		}
		if ( marker.category == Category::Light ) {
			/* a light is drawn in its own colour */
			float color[3] = { 1.0f, 1.0f, 1.0f };
			const int colorKey = entity.IndexOf( "_color" );
			if ( colorKey >= 0 && et::ParseVector( entity.pairs[static_cast<std::size_t>( colorKey )].value, color ) == 3 ) {
				const float brightest = std::max( { color[0], color[1], color[2] } );
				if ( brightest > 0.0f ) {
					marker.color = { 0.25f + 0.75f * color[0] / brightest, 0.25f + 0.75f * color[1] / brightest,
						0.25f + 0.75f * color[2] / brightest, 1.0f };
				}
			}
		}
		marker.hasYaw = ee::EntityYaw( entity, marker.yaw ) && !marker.brush;
		if ( marker.placed ) {
			marker.cluster = CM_LeafCluster( CM_PointLeafnum( marker.point ) );
		}
		if ( !marker.targetname.empty() ) {
			named[marker.targetname].push_back( static_cast<int>( i ) );
		}
		editor.markerOf[marker.id] = static_cast<int>( i );
		editor.markers.push_back( std::move( marker ) );
	}
	for ( std::size_t i = 0; i < items.size(); i++ ) {
		for ( const et::KeyValue &pair : items[i].entity.pairs ) {
			if ( pair.value.empty() || !ee::IsTargetKey( pair.key ) ) {
				continue;
			}
			const auto found = named.find( pair.value );
			if ( found == named.end() ) {
				continue;
			}
			for ( const int target : found->second ) {
				if ( target != static_cast<int>( i ) ) {
					editor.links.emplace_back( static_cast<int>( i ), target );
				}
			}
		}
	}
}

const Marker *MarkerFor( std::uint32_t id )
{
	const auto found = editor.markerOf.find( id );
	return found != editor.markerOf.end() ? &editor.markers[static_cast<std::size_t>( found->second )] : nullptr;
}

/* The selected entities that stand somewhere, the primary one first. */
std::vector<const Marker *> PlacedSelection()
{
	std::vector<const Marker *> placed;
	const Marker *primary = MarkerFor( editor.doc.Selected() );
	if ( primary && primary->placed ) {
		placed.push_back( primary );
	}
	for ( const std::uint32_t id : editor.doc.Selection() ) {
		const Marker *marker = MarkerFor( id );
		if ( marker && marker->placed && marker != primary ) {
			placed.push_back( marker );
		}
	}
	return placed;
}

/* ---- the camera ------------------------------------------------------------ */

void CameraAxis( vec3_t axis[3] )
{
	AnglesToAxis( editor.cameraAngles, axis );
}

void CameraFromPlayer()
{
	if ( !editor.playerView.valid ) {
		return;
	}
	VectorCopy( editor.playerView.origin, editor.cameraOrigin );
	vectoangles( editor.playerView.axis[0], editor.cameraAngles );
	if ( editor.cameraAngles[PITCH] > 180.0f ) {
		editor.cameraAngles[PITCH] -= 360.0f;
	}
	editor.cameraAngles[ROLL] = 0.0f;
	editor.cameraSet = true;
	editor.cameraMap = editor.mapPath;
}

/* Whether a key steers the camera that way: a fixed key, or one bound to
 * the game's own move. */
bool KeyMoves( int key, char fixed, const char *binding )
{
	if ( key == fixed ) {
		return true;
	}
	if ( !binding[0] || key < 0 || key >= MAX_KEYS ) {
		return false;
	}
	const char *bound = Key_GetBinding( key );
	return bound && !Q_stricmp( bound, binding );
}

float MoveAxis( char positive, const char *positiveBinding, char negative, const char *negativeBinding )
{
	float amount = 0.0f;
	for ( int key = 0; key < MAX_KEYS; key++ ) {
		if ( !editor.keyDown[static_cast<std::size_t>( key )] ) {
			continue;
		}
		if ( KeyMoves( key, positive, positiveBinding ) ) amount += 1.0f;
		if ( KeyMoves( key, negative, negativeBinding ) ) amount -= 1.0f;
	}
	return std::clamp( amount, -1.0f, 1.0f );
}

void UpdateCamera()
{
	if ( editor.cameraFrame == cls.framecount ) {
		return;
	}
	editor.cameraFrame = cls.framecount;
	const int now = cls.realtime;
	const float seconds = editor.cameraTime ? std::clamp( static_cast<float>( now - editor.cameraTime ) * 0.001f, 0.0f, 0.1f ) : 0.0f;
	editor.cameraTime = now;
	if ( !( editor.looking || editor.flying ) || seconds <= 0.0f ) {
		return;
	}
	const float forward = MoveAxis( 'w', "+forward", 's', "+back" );
	const float right = MoveAxis( 'd', "+moveright", 'a', "+moveleft" );
	const float up = MoveAxis( 'e', "+moveup", 'q', "+movedown" ) + MoveAxis( ' ', "", 'c', "" );
	float speed = ( cl_entityEditorSpeed ? cl_entityEditorSpeed->value : 400.0f ) * editor.speedScale;
	if ( editor.keyDown[K_SHIFT] ) speed *= 3.0f;
	if ( editor.keyDown[K_CTRL] ) speed *= 0.3f;
	vec3_t forwardAxis, rightAxis;
	AngleVectors( editor.cameraAngles, forwardAxis, rightAxis, nullptr );
	VectorMA( editor.cameraOrigin, forward * speed * seconds, forwardAxis, editor.cameraOrigin );
	VectorMA( editor.cameraOrigin, right * speed * seconds, rightAxis, editor.cameraOrigin );
	editor.cameraOrigin[2] += std::clamp( up, -1.0f, 1.0f ) * speed * seconds;
}

/* Frames an entity: the camera backs off from its box's middle along the
 * way it looks (an entity standing on its origin rises above it). */
void FocusOn( std::uint32_t id )
{
	const Marker *marker = MarkerFor( id );
	if ( !marker || !marker->placed ) {
		return;
	}
	vec3_t size, middle;
	VectorSubtract( marker->maxs, marker->mins, size );
	VectorAdd( marker->mins, marker->maxs, middle );
	VectorScale( middle, 0.5f, middle );
	const float distance = std::clamp( VectorLength( size ) * 1.5f + 96.0f, 96.0f, 4096.0f );
	vec3_t forward;
	AngleVectors( editor.cameraAngles, forward, nullptr, nullptr );
	VectorMA( middle, -distance, forward, editor.cameraOrigin );
	editor.cameraSet = true;
}

/* Where an entity whose box about its point is mins to maxs stands when put
 * where the ray through a pixel meets the world: against the surface met,
 * its box clear of it -- across the surface, at the grid step nearest it on
 * its near side, or flush with it when not snapping -- and on the grid along
 * it. The point itself goes on the grid or, given `from` (a brush entity's
 * point, which keeps its brushes' place on the grid), the move from there.
 * With nothing met, a step ahead, on the grid. */
void PlaceAtAim( float sx, float sy, const vec3_t mins, const vec3_t maxs, const float *from, vec3_t point )
{
	vec3_t direction, end;
	PixelRay( editor.view, sx, sy, direction );
	VectorMA( editor.view.origin, 8192.0f, direction, end );
	trace_t trace;
	CM_BoxTrace( &trace, editor.view.origin, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
	const bool hit = trace.fraction < 1.0f && !trace.allsolid;
	vec3_t normal = { 0.0f, 0.0f, 0.0f };
	if ( hit ) {
		/* the box's corner deepest into the surface, as far behind it as
		 * the point stands before it; the trace stops short of the surface
		 * (SURFACE_CLIP_EPSILON), the box rests on it */
		VectorCopy( trace.plane.normal, normal );
		float clearance = 0.0f;
		for ( int axis = 0; axis < 3; axis++ ) {
			clearance -= normal[axis] * ( normal[axis] > 0.0f ? mins[axis] : maxs[axis] );
		}
		const float gap = std::clamp( DotProduct( trace.endpos, normal ) - trace.plane.dist, 0.0f, 1.0f );
		VectorMA( trace.endpos, clearance - gap, normal, point );
	} else {
		VectorMA( editor.view.origin, 128.0f, direction, point );
	}
	/* across the surface, on the grid on its near side, never into it; or,
	 * not snapping, flush with it */
	const auto across = []( float value, float towards ) {
		if ( !Snapping() ) {
			return std::floor( value * 8.0f + 0.5f ) / 8.0f;
		}
		const float grid = Grid();
		return towards > 0.0f ? std::ceil( value / grid - 0.001f ) * grid : std::floor( value / grid + 0.001f ) * grid;
	};
	for ( int axis = 0; axis < 3; axis++ ) {
		const bool acrossSurface = std::fabs( normal[axis] ) > 0.7f;
		const float base = from ? from[axis] : 0.0f;
		point[axis] = base + ( acrossSurface ? across( point[axis] - base, normal[axis] ) : SnapValue( point[axis] - base ) );
	}
}

/* Where a new or pasted entity stands: put at the view's centre. */
void PlaceEntityAtAim( const et::Entity &entity, vec3_t point )
{
	vec3_t mins, maxs;
	MarkerExtent( entity, mins, maxs );
	PlaceAtAim( editor.view.x + editor.view.width * 0.5f, editor.view.y + editor.view.height * 0.5f, mins, maxs,
		nullptr, point );
}

/* ---- edits through the editor ---------------------------------------------- */

/* A marker moved so that its point is `point`: a point entity's origin, or a
 * brush entity's origin key by as much as its centre moved. */
void PlaceMarkerWithoutCheckpoint( const Marker &marker, const vec3_t point )
{
	vec3_t origin;
	if ( marker.brush ) {
		vec3_t moved;
		VectorSubtract( point, marker.point, moved );
		VectorAdd( marker.offset, moved, origin );
	} else {
		VectorCopy( point, origin );
	}
	editor.doc.PlaceWithoutCheckpoint( marker.id, origin );
}

/* Every selected entity that stands somewhere moved by `delta`, as one
 * change; how many moved. */
int MoveSelectionBy( const vec3_t delta, const char *label )
{
	const std::vector<const Marker *> placed = PlacedSelection();
	if ( placed.empty() ) {
		return 0;
	}
	/* copies: placing one rebuilds nothing, but the document changes */
	std::vector<Marker> moving;
	for ( const Marker *marker : placed ) {
		moving.push_back( *marker );
	}
	editor.doc.Checkpoint( label );
	for ( const Marker &marker : moving ) {
		vec3_t point;
		VectorAdd( marker.point, delta, point );
		PlaceMarkerWithoutCheckpoint( marker, point );
	}
	return static_cast<int>( moving.size() );
}

void Nudge( float dx, float dy, float dz )
{
	const float step = Grid() > 0.0f ? Grid() : 1.0f;
	/* along the camera's facing, turned to the nearest axis */
	const float yaw = DEG2RAD( editor.cameraAngles[YAW] );
	float fx = std::cos( yaw ), fy = std::sin( yaw );
	if ( std::fabs( fx ) >= std::fabs( fy ) ) {
		fx = fx >= 0.0f ? 1.0f : -1.0f;
		fy = 0.0f;
	} else {
		fy = fy >= 0.0f ? 1.0f : -1.0f;
		fx = 0.0f;
	}
	vec3_t delta;
	delta[0] = ( fx * dy + fy * dx ) * step;
	delta[1] = ( fy * dy - fx * dx ) * step;
	delta[2] = dz * step;
	MoveSelectionBy( delta, "move" );
}

/* How far a trace that stopped short of a floor (SURFACE_CLIP_EPSILON) must
 * still fall for a box about its end, mins to maxs, to rest on the floor's
 * plane. */
float FallToPlane( const trace_t &trace, const vec3_t mins, const vec3_t maxs )
{
	const float *normal = trace.plane.normal;
	if ( normal[2] < 0.1f ) {
		return 0.0f;
	}
	/* the box's corner deepest into the floor */
	float reach = DotProduct( trace.endpos, normal ) - trace.plane.dist;
	for ( int axis = 0; axis < 3; axis++ ) {
		reach += normal[axis] * ( normal[axis] > 0.0f ? mins[axis] : maxs[axis] );
	}
	return std::clamp( reach / normal[2], 0.0f, 1.0f );
}

/* Each selected entity set on the floor below it: its box swept down until
 * any part of it rests on the world, or, where the box already overlaps the
 * world, the middle of its bottom set on the floor below that. */
void DropToFloor()
{
	const std::vector<const Marker *> placed = PlacedSelection();
	if ( placed.empty() ) {
		return;
	}
	std::vector<std::pair<Marker, float>> drops;
	for ( const Marker *marker : placed ) {
		vec3_t mins, maxs, start, end;
		VectorSubtract( marker->mins, marker->point, mins );
		VectorSubtract( marker->maxs, marker->point, maxs );
		VectorCopy( marker->point, start );
		VectorCopy( marker->point, end );
		end[2] -= 8192.0f;
		trace_t trace;
		CM_BoxTrace( &trace, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse );
		if ( !trace.startsolid ) {
			if ( trace.fraction < 1.0f ) {
				const float z = trace.endpos[2] - FallToPlane( trace, mins, maxs );
				drops.emplace_back( *marker, std::floor( z * 8.0f + 0.5f ) / 8.0f );
			}
			continue;
		}
		const float below = marker->point[2] - marker->mins[2];
		start[2] = marker->mins[2] + 1.0f;
		end[2] = start[2] - 8192.0f;
		CM_BoxTrace( &trace, start, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
		if ( trace.fraction < 1.0f && !trace.startsolid ) {
			const float z = trace.endpos[2] - FallToPlane( trace, vec3_origin, vec3_origin ) + below;
			drops.emplace_back( *marker, std::floor( z * 8.0f + 0.5f ) / 8.0f );
		}
	}
	if ( drops.empty() ) {
		SetStatus( StatusKind::Warning, "no floor below the selection" );
		return;
	}
	editor.doc.Checkpoint( "drop" );
	for ( const auto &drop : drops ) {
		vec3_t point;
		VectorCopy( drop.first.point, point );
		point[2] = drop.second;
		PlaceMarkerWithoutCheckpoint( drop.first, point );
	}
	if ( drops.size() < placed.size() ) {
		SetStatus( StatusKind::Warning, Format( "%d of %d had a floor below", static_cast<int>( drops.size() ),
			static_cast<int>( placed.size() ) ) );
	}
}

/* The primary entity put where the pixel's ray meets the world, its box
 * resting on the surface there (PlaceAtAim), the rest of the selection
 * keeping their places about it. */
void MoveToAim( float sx, float sy )
{
	const Marker *primary = MarkerFor( editor.doc.Selected() );
	if ( !primary || !primary->placed ) {
		return;
	}
	vec3_t mins, maxs, point;
	VectorSubtract( primary->mins, primary->point, mins );
	VectorSubtract( primary->maxs, primary->point, maxs );
	PlaceAtAim( sx, sy, mins, maxs, primary->brush ? primary->point : nullptr, point );
	vec3_t delta;
	VectorSubtract( point, primary->point, delta );
	MoveSelectionBy( delta, "move" );
}

void Rotate( float degrees )
{
	if ( !editor.doc.RotateSelection( degrees ) ) {
		const ee::Item *item = editor.doc.SelectedItem();
		if ( item && editor.doc.IndexOf( item->id ) > 0 ) {
			SetStatus( StatusKind::Warning, "an \"angle\" of -1 or -2 points up or down, not a way to turn: it stays" );
		}
		return;
	}
	float yaw = 0.0f;
	if ( const ee::Item *item = editor.doc.SelectedItem() ) {
		ee::EntityYaw( item->entity, yaw );
	}
	SetStatus( StatusKind::Info, editor.doc.SelectionCount() > 1
		? Format( "turned %d by %s", static_cast<int>( editor.doc.SelectionCount() ), et::FormatNumber( degrees ).c_str() )
		: Format( "facing %s", et::FormatNumber( yaw ).c_str() ) );
}

void Create( const std::string &classname )
{
	if ( et::CheckToken( classname, true ) != et::TextProblem::None ) {
		SetStatus( StatusKind::Error, Format( "\"%s\" cannot be a classname", classname.c_str() ) );
		return;
	}
	et::Entity made;
	made.Set( "classname", classname );
	vec3_t point;
	PlaceEntityAtAim( made, point );
	const std::uint32_t id = editor.doc.Create( classname, point );
	if ( id ) {
		editor.anchorId = id;
		editor.scrollToSelection = true;
		SetStatus( StatusKind::Info, Format( "made a %s at %s", classname.c_str(), et::FormatVector( point ).c_str() ) );
	}
}

void Duplicate()
{
	const float step = Grid() > 0.0f ? std::max( Grid(), 8.0f ) : 16.0f;
	const float offset[3] = { step, step, 0.0f };
	const std::size_t made = editor.doc.DuplicateSelection( offset );
	if ( !made ) {
		if ( editor.doc.SelectionCount() ) {
			SetStatus( StatusKind::Warning, "a brush entity's model belongs to it alone, and the worldspawn to the map: "
				"nothing to duplicate" );
		}
		return;
	}
	editor.anchorId = editor.doc.Selected();
	editor.scrollToSelection = true;
	SetStatus( StatusKind::Info, made == 1 ? std::string( "duplicated" ) : Format( "duplicated %d", static_cast<int>( made ) ) );
}

void Delete();

/* The selected entities as entity text, a paste for this map or another. */
void CopyEntities( bool cut )
{
	const std::vector<std::uint32_t> ids = editor.doc.Selection();
	if ( ids.empty() ) {
		return;
	}
	std::vector<et::Entity> list;
	for ( const std::uint32_t id : ids ) {
		const ee::Item *item = editor.doc.Find( id );
		if ( cut && ee::IsBrushModel( item->entity ) ) {
			SetStatus( StatusKind::Warning, "a brush entity cannot be pasted back, so nothing is cut; Delete removes it" );
			return;
		}
		list.push_back( item->entity );
	}
	Sys_SetClipboardData( et::Serialize( list ).c_str() );
	if ( cut ) {
		Delete();
	} else {
		SetStatus( StatusKind::Info, list.size() == 1 ? std::string( "copied the entity's text to the clipboard" )
			: Format( "copied %d entities' text to the clipboard", static_cast<int>( list.size() ) ) );
	}
}

std::string ClipboardText()
{
	std::string text;
	if ( char *data = Sys_GetClipboardData() ) {
		text = data;
		Z_Free( data );
	}
	return text;
}

/* Entity text from the clipboard, listed last; a lone entity with an origin
 * is put where the view's centre meets the world, as a new one is. */
void PasteEntities()
{
	const et::ParseResult parsed = et::Parse( ClipboardText() );
	if ( parsed.entities.empty() ) {
		SetStatus( StatusKind::Warning, "the clipboard holds no entity text" );
		return;
	}
	const std::size_t count = editor.doc.Paste( parsed.entities, nullptr );
	if ( !count ) {
		SetStatus( StatusKind::Warning, "nothing pasted: a worldspawn or a brush entity belongs to its own map" );
		return;
	}
	const ee::Item *pasted = editor.doc.SelectedItem();
	float origin[3];
	if ( count == 1 && pasted && ee::EntityOrigin( pasted->entity, origin ) ) {
		/* part of the paste's change: its checkpoint is taken */
		vec3_t at;
		PlaceEntityAtAim( pasted->entity, at );
		editor.doc.PlaceWithoutCheckpoint( pasted->id, at );
	}
	editor.anchorId = editor.doc.Selected();
	editor.scrollToSelection = true;
	SetStatus( StatusKind::Info, Format( "pasted %d entit%s", static_cast<int>( count ), count == 1 ? "y" : "ies" ) );
}

void Delete()
{
	const ee::Item *item = editor.doc.SelectedItem();
	if ( !item ) {
		return;
	}
	if ( editor.doc.SelectionCount() == 1 && editor.doc.IndexOf( item->id ) == 0 ) {
		SetStatus( StatusKind::Warning, "the worldspawn stays: every level needs it" );
		return;
	}
	const std::string name( item->entity.Classname() );
	const std::size_t deleted = editor.doc.DeleteSelection();
	if ( deleted == 1 ) {
		SetStatus( StatusKind::Info, Format( "deleted %s", name.c_str() ) );
	} else if ( deleted ) {
		SetStatus( StatusKind::Info, Format( "deleted %d entities", static_cast<int>( deleted ) ) );
	}
	editor.anchorId = editor.doc.Selected();
}

void Undo()
{
	const std::string label( editor.doc.UndoLabel() );
	if ( editor.doc.Undo() ) {
		SetStatus( StatusKind::Info, "undid " + label );
	}
}

void Redo()
{
	const std::string label( editor.doc.RedoLabel() );
	if ( editor.doc.Redo() ) {
		SetStatus( StatusKind::Info, "redid " + label );
	}
}

void CycleGrid( int step )
{
	const float grid = Grid();
	int index = 3;
	for ( int i = 0; i < static_cast<int>( std::size( kGridSizes ) ); i++ ) {
		if ( std::fabs( kGridSizes[i] - grid ) < 0.01f ) {
			index = i;
		}
	}
	index = std::clamp( index + step, 0, static_cast<int>( std::size( kGridSizes ) ) - 1 );
	Cvar_Set( "cl_entityEditorGrid", et::FormatNumber( kGridSizes[index] ).c_str() );
	SetStatus( StatusKind::Info, Format( "grid %s", et::FormatNumber( kGridSizes[index] ).c_str() ) );
}

void UpdateFilter()
{
	if ( editor.filterRevision == editor.doc.Revision() && editor.filterUsed == editor.filter ) {
		return;
	}
	editor.filterRevision = editor.doc.Revision();
	editor.filterUsed = editor.filter;
	editor.filtered.clear();
	for ( const ee::Item &item : editor.doc.Items() ) {
		if ( editor.filter.empty() || ee::Document::Matches( item.entity, editor.filter ) ) {
			editor.filtered.push_back( item.id );
		}
	}
}

/* The entity listed before or after the selected one, in the filtered list. */
void SelectNeighbour( int step )
{
	if ( editor.filtered.empty() ) {
		return;
	}
	const auto at = std::find( editor.filtered.begin(), editor.filtered.end(), editor.doc.Selected() );
	int index = at == editor.filtered.end() ? ( step > 0 ? -1 : 0 ) : static_cast<int>( at - editor.filtered.begin() );
	const int count = static_cast<int>( editor.filtered.size() );
	index = ( index + step + count ) % count;
	editor.doc.Select( editor.filtered[static_cast<std::size_t>( index )] );
	editor.anchorId = editor.doc.Selected();
	editor.scrollToSelection = true;
}

/* Every entity the list shows. */
void SelectListed()
{
	UpdateFilter();
	editor.doc.SetSelection( editor.filtered, editor.doc.IsSelected( editor.doc.Selected() ) ? editor.doc.Selected()
		: editor.filtered.empty() ? 0 : editor.filtered.back() );
	SetStatus( StatusKind::Info, Format( "%d selected", static_cast<int>( editor.doc.SelectionCount() ) ) );
}

/* ---- pause, open and close ---------------------------------------------------- */

void Close();
void StartLooking( bool look );

bool CanOpen( bool quiet )
{
	if ( cls.state != CA_ACTIVE ) {
		if ( !quiet ) Com_Printf( "The entity editor needs a map running.\n" );
		return false;
	}
	/* A free camera on someone else's server is a view the player was not
	 * given; there it needs that server's cheats, as a demo needs nothing. */
	if ( !( com_sv_running && com_sv_running->integer ) && !clc.demoplaying && !cl_connectedToCheatServer ) {
		if ( !quiet ) Com_Printf( "The entity editor needs a local server, a demo or a server with sv_cheats 1.\n" );
		return false;
	}
	return true;
}

void Open()
{
	if ( editor.active || !CanOpen( false ) ) {
		return;
	}
	/* edits left unsaved on this map are taken up again */
	if ( !editor.doc.Dirty() || editor.doc.Count() == 0 || editor.mapPath != CurrentMapPath() ) {
		LoadDocument();
	}
	editor.active = true;
	editor.help = false;
	editor.openNewPrompt = false;
	editor.looking = editor.flying = false;
	editor.keyDown.fill( false );
	editor.shortcuts.clear();
	editor.actions.clear();
	if ( !editor.cameraSet || editor.cameraMap != editor.mapPath ) {
		CameraFromPlayer();
	}
	editor.cameraTime = cls.realtime;
	if ( !editor.cursorSet ) {
		editor.cursorX = static_cast<float>( cls.glconfig.vidWidth ) * 0.5f;
		editor.cursorY = static_cast<float>( cls.glconfig.vidHeight ) * 0.5f;
		editor.cursorSet = true;
	}
	Key_ClearStates();
	Key_SetEntityEditorCatcher( qtrue );
	if ( editor.imgui ) {
		/* what was queued while the editor was shut is not its */
		ImGuiIO &io = ImGui::GetIO();
		io.ClearEventsQueue();
		io.ClearInputKeys();
		io.ClearInputMouse();
		io.AddFocusEvent( true );
	}
	editor.frameTime = cls.realtime;
	Pause();
	const std::string note = editor.parseNote.empty() ? std::string() : " (" + editor.parseNote + ")";
	SetStatus( editor.parseNote.empty() ? StatusKind::Info : StatusKind::Warning,
		Format( "%d entities from %s%s. Right mouse: look and fly. F1: help.",
			static_cast<int>( editor.doc.Count() ), editor.source.c_str(), note.c_str() ) );
}

void Close()
{
	if ( !editor.active ) {
		return;
	}
	CommitField();
	editor.fieldId = 0;
	editor.fieldCommit = nullptr;
	editor.active = false;
	editor.looking = editor.flying = false;
	editor.drag = Drag::None;
	editor.boxPending = editor.boxing = false;
	editor.keyDown.fill( false );
	Key_SetEntityEditorCatcher( qfalse );
	Unpause();
	if ( editor.doc.Dirty() ) {
		Com_Printf( S_COLOR_YELLOW "Entity editor: closed with unsaved changes; `entityeditor` reopens them.\n" );
	}
}

/* ---- input --------------------------------------------------------------------- */

bool KeyIsDown( int key )
{
	return key >= 0 && key < MAX_KEYS && editor.keyDown[static_cast<std::size_t>( key )];
}

/* The pointer is ImGui's while the camera does not turn. */
void PointerToImGui()
{
	if ( !editor.imgui ) {
		return;
	}
	ImGuiIO &io = ImGui::GetIO();
	if ( editor.looking || editor.flying ) {
		io.AddMousePosEvent( -FLT_MAX, -FLT_MAX );
	} else {
		io.AddMousePosEvent( editor.cursorX, editor.cursorY );
	}
}

void StartLooking( bool look )
{
	if ( editor.looking == look ) {
		return;
	}
	editor.looking = look;
	if ( look ) {
		CommitField();
		editor.cameraTime = cls.realtime;
	}
	PointerToImGui();
}

void SetFlying( bool fly )
{
	editor.flying = fly;
	editor.cameraTime = cls.realtime;
	PointerToImGui();
}

/* An engine key as ImGui names it; ImGuiKey_None for one it does not. */
ImGuiKey ImGuiKeyFor( int key )
{
	if ( key >= 'a' && key <= 'z' ) {
		return static_cast<ImGuiKey>( ImGuiKey_A + ( key - 'a' ) );
	}
	if ( key >= '0' && key <= '9' ) {
		return static_cast<ImGuiKey>( ImGuiKey_0 + ( key - '0' ) );
	}
	if ( key >= K_F1 && key <= K_F12 ) {
		return static_cast<ImGuiKey>( ImGuiKey_F1 + ( key - K_F1 ) );
	}
	switch ( key ) {
	case K_TAB: return ImGuiKey_Tab;
	case K_ENTER: return ImGuiKey_Enter;
	case K_KP_ENTER: return ImGuiKey_KeypadEnter;
	case K_ESCAPE: return ImGuiKey_Escape;
	case K_SPACE: return ImGuiKey_Space;
	case K_BACKSPACE: return ImGuiKey_Backspace;
	case K_DEL: case K_KP_DEL: return ImGuiKey_Delete;
	case K_INS: case K_KP_INS: return ImGuiKey_Insert;
	case K_HOME: case K_KP_HOME: return ImGuiKey_Home;
	case K_END: case K_KP_END: return ImGuiKey_End;
	case K_PGUP: case K_KP_PGUP: return ImGuiKey_PageUp;
	case K_PGDN: case K_KP_PGDN: return ImGuiKey_PageDown;
	case K_LEFTARROW: case K_KP_LEFTARROW: return ImGuiKey_LeftArrow;
	case K_RIGHTARROW: case K_KP_RIGHTARROW: return ImGuiKey_RightArrow;
	case K_UPARROW: case K_KP_UPARROW: return ImGuiKey_UpArrow;
	case K_DOWNARROW: case K_KP_DOWNARROW: return ImGuiKey_DownArrow;
	case K_CTRL: return ImGuiKey_LeftCtrl;
	case K_SHIFT: return ImGuiKey_LeftShift;
	case K_ALT: return ImGuiKey_LeftAlt;
	case K_SUPER: case K_COMMAND: return ImGuiKey_LeftSuper;
	case K_MENU: return ImGuiKey_Menu;
	case K_CAPSLOCK: return ImGuiKey_CapsLock;
	case K_KP_5: return ImGuiKey_Keypad5;
	case K_KP_SLASH: return ImGuiKey_KeypadDivide;
	case K_KP_STAR: return ImGuiKey_KeypadMultiply;
	case K_KP_MINUS: return ImGuiKey_KeypadSubtract;
	case K_KP_PLUS: return ImGuiKey_KeypadAdd;
	case K_KP_EQUALS: return ImGuiKey_KeypadEqual;
	case '\'': return ImGuiKey_Apostrophe;
	case ',': return ImGuiKey_Comma;
	case '-': return ImGuiKey_Minus;
	case '.': return ImGuiKey_Period;
	case '/': return ImGuiKey_Slash;
	case ';': return ImGuiKey_Semicolon;
	case '=': return ImGuiKey_Equal;
	case '[': return ImGuiKey_LeftBracket;
	case '\\': return ImGuiKey_Backslash;
	case ']': return ImGuiKey_RightBracket;
	case '`': return ImGuiKey_GraveAccent;
	default: return ImGuiKey_None;
	}
}

/* A key going down or up, to ImGui; a modifier carries its flag too. */
void KeyToImGui( int key, bool down )
{
	if ( !editor.imgui ) {
		return;
	}
	ImGuiIO &io = ImGui::GetIO();
	switch ( key ) {
	case K_MOUSE1: io.AddMouseButtonEvent( 0, down ); return;
	case K_MOUSE2: io.AddMouseButtonEvent( 1, down ); return;
	case K_MOUSE3: io.AddMouseButtonEvent( 2, down ); return;
	case K_CTRL: io.AddKeyEvent( ImGuiMod_Ctrl, down ); break;
	case K_SHIFT: io.AddKeyEvent( ImGuiMod_Shift, down ); break;
	case K_ALT: io.AddKeyEvent( ImGuiMod_Alt, down ); break;
	case K_SUPER: case K_COMMAND: io.AddKeyEvent( ImGuiMod_Super, down ); break;
	default: break;
	}
	const ImGuiKey named = ImGuiKeyFor( key );
	if ( named != ImGuiKey_None ) {
		io.AddKeyEvent( named, down );
	}
}

/* A key for the editor's own keys: the keypad's navigation keys as the
 * arrows and pages they stand for. */
void QueueShortcut( int key )
{
	switch ( key ) {
	case K_KP_UPARROW: key = K_UPARROW; break;
	case K_KP_DOWNARROW: key = K_DOWNARROW; break;
	case K_KP_LEFTARROW: key = K_LEFTARROW; break;
	case K_KP_RIGHTARROW: key = K_RIGHTARROW; break;
	case K_KP_HOME: key = K_HOME; break;
	case K_KP_END: key = K_END; break;
	case K_KP_PGUP: key = K_PGUP; break;
	case K_KP_PGDN: key = K_PGDN; break;
	case K_KP_DEL: key = K_DEL; break;
	case K_KP_ENTER: key = K_ENTER; break;
	default: break;
	}
	if ( key >= 'A' && key <= 'Z' ) {
		key = key - 'A' + 'a';
	}
	editor.shortcuts.push_back( Shortcut{ key, KeyIsDown( K_SHIFT ), KeyIsDown( K_CTRL ) } );
}

/* ---- drawing -------------------------------------------------------------------- */

void SetRenderColor( const ui::Color &color )
{
	const float rgba[4] = { color.r, color.g, color.b, color.a };
	if ( std::memcmp( rgba, editor.lastColor, sizeof( rgba ) ) != 0 ) {
		re.SetColor( rgba );
		std::memcpy( editor.lastColor, rgba, sizeof( rgba ) );
	}
}

byte ColorByte( float value )
{
	return static_cast<byte>( std::clamp( value, 0.0f, 1.0f ) * 255.0f + 0.5f );
}

/* What a command takes of the renderer's buffer, at most: a colour change
 * for a picture. */
int CommandBytes( const ui::DrawCommand &command )
{
	return command.kind == ui::DrawKind::Quad ? kQuadBytes : kPicBytes + kColorBytes;
}

int ListBytes( const ui::DrawList &list )
{
	int bytes = 0;
	for ( const ui::DrawCommand &command : list.Commands() ) {
		bytes += CommandBytes( command );
	}
	return bytes;
}

/* A list's commands to the renderer, the first `skip` bytes of them left
 * out, and none past the frame's kFrameBytes. */
void Render( const ui::DrawList &list, int skip = 0 )
{
	for ( const ui::DrawCommand &command : list.Commands() ) {
		const int bytes = CommandBytes( command );
		if ( skip > 0 || editor.frameBytes + bytes > kFrameBytes ) {
			skip -= bytes;
			editor.frameDropped += bytes;
			continue;
		}
		const qhandle_t shader = static_cast<qhandle_t>( command.texture );
		if ( !shader ) {
			continue;
		}
		editor.frameBytes += bytes;
		editor.frameCommands++;
		if ( command.kind == ui::DrawKind::Pic ) {
			SetRenderColor( command.colors[0] );
			re.DrawStretchPic( command.points[0][0], command.points[0][1], command.points[1][0], command.points[1][1],
				command.st[0][0], command.st[0][1], command.st[1][0], command.st[1][1], shader );
			continue;
		}
		if ( !re.DrawQuad ) {
			continue;
		}
		refDrawQuad_t quad{};
		for ( int i = 0; i < 4; i++ ) {
			quad.xy[i][0] = command.points[i][0];
			quad.xy[i][1] = command.points[i][1];
			quad.st[i][0] = command.st[i][0];
			quad.st[i][1] = command.st[i][1];
			quad.colors[i].rgba[0] = ColorByte( command.colors[i].r );
			quad.colors[i].rgba[1] = ColorByte( command.colors[i].g );
			quad.colors[i].rgba[2] = ColorByte( command.colors[i].b );
			quad.colors[i].rgba[3] = ColorByte( command.colors[i].a );
		}
		quad.colorMask = 15u;
		quad.shader = shader;
		re.DrawQuad( &quad );
	}
}

/* ---- ImGui's pictures ------------------------------------------------------------ */

/* A picture ImGui asked for, uploaded whole under a name of its own (the
 * renderer answers a name it knows with the shader it has, and cannot
 * write into an image it made). */
qhandle_t UploadTexture( ImTextureData &texture )
{
	if ( editor.textureUploads >= kMaxTextureUploads ) {
		if ( !editor.textureLimitNoted ) {
			editor.textureLimitNoted = true;
			Com_Printf( S_COLOR_YELLOW "Entity editor: %d interface pictures uploaded under this renderer; no more are, "
				"to spare its image slots (vid_restart frees them).\n", kMaxTextureUploads );
		}
		return 0;
	}
	if ( texture.Width <= 0 || texture.Height <= 0 || texture.Width > 4096 || texture.Height > 4096 ) {
		return 0;
	}
	std::vector<byte> rgba( static_cast<std::size_t>( texture.Width ) * static_cast<std::size_t>( texture.Height ) * 4u );
	const unsigned char *pixels = static_cast<const unsigned char *>( texture.GetPixels() );
	if ( texture.Format == ImTextureFormat_Alpha8 ) {
		for ( std::size_t i = 0; i < rgba.size() / 4u; i++ ) {
			rgba[i * 4u + 0] = rgba[i * 4u + 1] = rgba[i * 4u + 2] = 255;
			rgba[i * 4u + 3] = pixels[i];
		}
	} else {
		std::memcpy( rgba.data(), pixels, rgba.size() );
	}
	editor.textureUploads++;
	const std::string name = Format( "*fntech3/entityeditor/imgui%d", ++editor.textureSerial );
	return re.RegisterShaderFromRGBA( name.c_str(), rgba.data(), texture.Width, texture.Height );
}

/* ImGui's requests for its pictures, honoured before its triangles are
 * drawn. The renderer frees an image only when it restarts. */
void UpdateTextures( ImDrawData &data )
{
	if ( !data.Textures ) {
		return;
	}
	for ( ImTextureData *texture : *data.Textures ) {
		if ( texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates ) {
			const qhandle_t shader = UploadTexture( *texture );
			if ( shader ) {
				texture->SetTexID( static_cast<ImTextureID>( shader ) );
			}
			texture->SetStatus( ImTextureStatus_OK );
		} else if ( texture->Status == ImTextureStatus_WantDestroy && texture->UnusedFrames > 0 ) {
			texture->SetTexID( ImTextureID_Invalid );
			texture->SetStatus( ImTextureStatus_Destroyed );
		}
	}
}

/* ---- ImGui's style ---------------------------------------------------------------- */

float UiScale()
{
	const float configured = cl_entityEditorScale ? cl_entityEditorScale->value : 0.0f;
	const float scale = configured > 0.0f ? configured : static_cast<float>( cls.glconfig.vidHeight ) / 900.0f;
	return std::clamp( std::floor( scale * 4.0f + 0.5f ) / 4.0f, 0.75f, 4.0f );
}

float TextSize() { return std::round( 14.0f * editor.uiScale ); }
float LabelSize() { return std::round( 12.0f * editor.uiScale ); }
float Px( float pixels ) { return std::round( pixels * editor.uiScale ); }

/* ImGui's dark style, square and thin-lined, for this scale. */
void ApplyStyle( float scale )
{
	ImGuiStyle style;
	ImGui::StyleColorsDark( &style );
	style.WindowRounding = style.ChildRounding = style.FrameRounding = style.PopupRounding = 0.0f;
	style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 0.0f;
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowPadding = ImVec2( 6.0f, 6.0f );
	style.FramePadding = ImVec2( 5.0f, 3.0f );
	style.ItemSpacing = ImVec2( 6.0f, 4.0f );
	/* the renderers' 2D path draws each anti-aliased line as one textured
	 * quad; filled shapes' feathering would cost a ring of quads each */
	style.AntiAliasedLines = true;
	style.AntiAliasedLinesUseTex = true;
	style.AntiAliasedFill = false;
	ImVec4 *colors = style.Colors;
	colors[ImGuiCol_WindowBg] = ImVec4( 0.06f, 0.07f, 0.09f, 0.95f );
	colors[ImGuiCol_PopupBg] = ImVec4( 0.07f, 0.08f, 0.10f, 0.97f );
	colors[ImGuiCol_MenuBarBg] = ImVec4( 0.09f, 0.10f, 0.13f, 0.97f );
	colors[ImGuiCol_Border] = ImVec4( 0.24f, 0.28f, 0.36f, 1.0f );
	colors[ImGuiCol_Header] = ImVec4( 0.20f, 0.36f, 0.62f, 0.80f );
	colors[ImGuiCol_HeaderHovered] = ImVec4( 0.24f, 0.42f, 0.70f, 0.80f );
	colors[ImGuiCol_HeaderActive] = ImVec4( 0.28f, 0.48f, 0.78f, 1.0f );
	style.ScaleAllSizes( scale );
	style.FontSizeBase = std::round( 14.0f * scale );
	style.FontScaleMain = 1.0f;
	ImGui::GetStyle() = style;
}

/* The glyphs the editor draws, baked before they are first drawn, so the
 * font's picture is uploaded once rather than as each glyph first shows. */
void PrewarmGlyphs()
{
	ImFont *font = ImGui::GetFont();
	if ( !font ) {
		return;
	}
	for ( const float size : { TextSize(), LabelSize() } ) {
		ImFontBaked *baked = font->GetFontBaked( size );
		if ( !baked ) {
			continue;
		}
		for ( ImWchar c = 32; c < 127; c++ ) {
			baked->FindGlyph( c );
		}
		baked->FindGlyph( static_cast<ImWchar>( 0x2026 ) );	/* the ellipsis of a cut text */
		baked->FindGlyph( static_cast<ImWchar>( 0xFFFD ) );	/* what a byte UTF-8 has no reading of shows */
	}
}

/* ---- the view's overlay ----------------------------------------------------- */

float Rounded( float width )
{
	return std::max( 1.0f, std::round( width ) );
}

/* A line of the view's overlay. ImGui's clip rectangle cuts it to the
 * view's part of the screen. */
void ViewLine( float x0, float y0, float x1, float y1, float width, const ui::Color &color )
{
	if ( !std::isfinite( x0 ) || !std::isfinite( y0 ) || !std::isfinite( x1 ) || !std::isfinite( y1 ) ) {
		return;
	}
	editor.overlay->AddLine( ImVec2( x0, y0 ), ImVec2( x1, y1 ), color.Packed(), Rounded( width ) );
}

void Line3D( const vec3_t a, const vec3_t b, float width, const ui::Color &color )
{
	ScreenPoint pa, pb;
	if ( ProjectSegment( editor.view, a, b, pa, pb ) ) {
		ViewLine( pa.x, pa.y, pb.x, pb.y, width, color );
	}
}

void Box3D( const vec3_t mins, const vec3_t maxs, float width, const ui::Color &color )
{
	vec3_t corners[8];
	for ( int i = 0; i < 8; i++ ) {
		corners[i][0] = ( i & 1 ) ? maxs[0] : mins[0];
		corners[i][1] = ( i & 2 ) ? maxs[1] : mins[1];
		corners[i][2] = ( i & 4 ) ? maxs[2] : mins[2];
	}
	static const int edges[12][2] = {
		{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};
	for ( const auto &edge : edges ) {
		Line3D( corners[edge[0]], corners[edge[1]], width, color );
	}
}

/* An arrow head at `tip`, pointing away from `tail`, on the screen. */
void ArrowHead( const ScreenPoint &tail, const ScreenPoint &tip, float size, float width, const ui::Color &color )
{
	const float dx = tip.x - tail.x, dy = tip.y - tail.y;
	const float length = std::sqrt( dx * dx + dy * dy );
	if ( length < 1.0f ) {
		return;
	}
	const float ux = dx / length, uy = dy / length;
	const float bx = tip.x - ux * size, by = tip.y - uy * size;
	ViewLine( tip.x, tip.y, bx - uy * size * 0.5f, by + ux * size * 0.5f, width, color );
	ViewLine( tip.x, tip.y, bx + uy * size * 0.5f, by - ux * size * 0.5f, width, color );
}

float GizmoLength( const vec3_t point )
{
	vec3_t toPoint;
	VectorSubtract( point, editor.view.origin, toPoint );
	return std::clamp( DotProduct( toPoint, editor.view.axis[0] ) * 0.16f * editor.view.tanY, 8.0f, 1024.0f );
}

/* The gizmo's handles on the screen: three axes and the centre. */
bool GizmoScreen( const Marker &marker, ScreenPoint &centre, ScreenPoint tips[3] )
{
	if ( !Project( editor.view, marker.point, centre ) ) {
		return false;
	}
	const float length = GizmoLength( marker.point );
	for ( int axis = 0; axis < 3; axis++ ) {
		vec3_t tip;
		VectorCopy( marker.point, tip );
		tip[axis] += length;
		ScreenPoint start;
		if ( !ProjectSegment( editor.view, marker.point, tip, start, tips[axis] ) ) {
			tips[axis] = centre;
		}
	}
	return true;
}

/* The handle under the pointer: 0..2 an axis, 3 the centre, -1 none. */
int GizmoHandleAt( const Marker &marker, float px, float py )
{
	ScreenPoint centre, tips[3];
	if ( !GizmoScreen( marker, centre, tips ) ) {
		return -1;
	}
	const float reach = Px( 7.0f );
	if ( std::fabs( px - centre.x ) <= reach && std::fabs( py - centre.y ) <= reach ) {
		return 3;
	}
	int best = -1;
	float bestDistance = reach;
	for ( int axis = 0; axis < 3; axis++ ) {
		const float distance = SegmentDistance( px, py, centre, tips[axis] );
		if ( distance < bestDistance ) {
			bestDistance = distance;
			best = axis;
		}
	}
	return best;
}

void DrawGizmo( const Marker &marker, int highlighted )
{
	ScreenPoint centre, tips[3];
	if ( !GizmoScreen( marker, centre, tips ) ) {
		return;
	}
	static const ui::Color axisColors[3] = {
		{ 1.0f, 0.32f, 0.30f, 1.0f }, { 0.36f, 0.92f, 0.38f, 1.0f }, { 0.36f, 0.56f, 1.0f, 1.0f }
	};
	const ui::Color hot = { 1.0f, 0.92f, 0.30f, 1.0f };
	const float width = Px( 2.5f );
	for ( int axis = 0; axis < 3; axis++ ) {
		const ui::Color color = highlighted == axis ? hot : axisColors[axis];
		ViewLine( centre.x, centre.y, tips[axis].x, tips[axis].y, width, color );
		ArrowHead( centre, tips[axis], Px( 10.0f ), width, color );
	}
	const float half = Px( 5.0f );
	const ui::Color middle = highlighted == 3 ? hot : ui::Color{ 0.95f, 0.95f, 0.95f, 1.0f };
	const ImVec2 min( centre.x - half, centre.y - half ), max( centre.x + half, centre.y + half );
	editor.overlay->AddRectFilled( min, max, ui::Color{ 0.0f, 0.0f, 0.0f, 0.7f }.Packed() );
	editor.overlay->AddRect( min, max, middle.Packed(), 0.0f, Rounded( width * 0.6f ) );
}

/* How a marker is drawn over the view: its box, a dot, or not at all. */
enum class Shape : std::uint8_t { None, Box, Dot };

struct Visible {
	int marker;
	float distance;
	ScreenPoint screen;
	bool inPvs;
	bool ahead;			/* its point is before the camera */
	Shape shape;
};

/* The nearest markers before the camera, or around it, are boxes, then brush
 * entities behind it whose boxes may reach into the view; the next markers
 * before it are dots, and the rest are not drawn. */
void AssignShapes( std::vector<Visible> &visible )
{
	int boxes = 0, dots = 0;
	for ( int pass = 0; pass < 2; pass++ ) {
		for ( Visible &entry : visible ) {
			if ( boxes >= kMaxBoxes ) {
				break;
			}
			if ( entry.shape == Shape::None && entry.ahead == ( pass == 0 ) ) {
				entry.shape = Shape::Box;
				boxes++;
			}
		}
	}
	for ( Visible &entry : visible ) {
		if ( dots >= kMaxDots ) {
			break;
		}
		if ( entry.shape == Shape::None && entry.screen.x > -50000.0f ) {
			entry.shape = Shape::Dot;
			dots++;
		}
	}
}

/* The markers before the camera, nearest first, each with whether the
 * camera's cluster can see its own and how it is drawn. */
std::vector<Visible> VisibleMarkers()
{
	std::vector<Visible> visible;
	const View &view = editor.view;
	const int cameraLeaf = CM_PointLeafnum( view.origin );
	const int cameraCluster = CM_LeafCluster( cameraLeaf );
	const byte *pvs = cameraCluster >= 0 ? CM_ClusterPVS( cameraCluster ) : nullptr;
	const float margin = view.width;
	for ( std::size_t i = 0; i < editor.markers.size(); i++ ) {
		const Marker &marker = editor.markers[i];
		if ( !marker.placed ) {
			continue;
		}
		ScreenPoint screen;
		const bool inside = marker.brush && marker.mins[0] <= view.origin[0] && view.origin[0] <= marker.maxs[0]
			&& marker.mins[1] <= view.origin[1] && view.origin[1] <= marker.maxs[1]
			&& marker.mins[2] <= view.origin[2] && view.origin[2] <= marker.maxs[2];
		bool ahead = true;
		if ( !Project( view, marker.point, screen ) ) {
			if ( !marker.brush ) {
				continue;
			}
			screen = ScreenPoint{ -100000.0f, -100000.0f, 0.0f };
			ahead = false;
		} else if ( !marker.brush && ( screen.x < view.x - margin || screen.x > view.x + view.width + margin
			|| screen.y < view.y - margin || screen.y > view.y + view.height + margin ) ) {
			continue;
		}
		vec3_t toPoint;
		VectorSubtract( marker.point, view.origin, toPoint );
		Visible entry;
		entry.marker = static_cast<int>( i );
		entry.distance = VectorLength( toPoint );
		entry.screen = screen;
		entry.inPvs = inside || !pvs || marker.cluster < 0 || ( pvs[marker.cluster >> 3] & ( 1 << ( marker.cluster & 7 ) ) );
		entry.ahead = ahead || inside;
		entry.shape = Shape::None;
		visible.push_back( entry );
	}
	std::sort( visible.begin(), visible.end(), []( const Visible &a, const Visible &b ) { return a.distance < b.distance; } );
	AssignShapes( visible );
	return visible;
}

/* The drawn marker the ray through a pixel meets first. */
std::uint32_t PickAt( const std::vector<Visible> &visible, float px, float py )
{
	vec3_t direction;
	PixelRay( editor.view, px, py, direction );
	std::uint32_t best = 0;
	float bestDistance = 1.0e9f;
	const float reach = Px( 6.0f );
	for ( std::size_t i = 0; i < visible.size(); i++ ) {
		const Marker &marker = editor.markers[static_cast<std::size_t>( visible[i].marker )];
		if ( visible[i].shape == Shape::None && !editor.doc.IsSelected( marker.id ) ) {
			continue;
		}
		float distance;
		bool hit = RayBox( editor.view.origin, direction, marker.mins, marker.maxs, distance );
		if ( !hit && !marker.brush && std::fabs( visible[i].screen.x - px ) <= reach
			&& std::fabs( visible[i].screen.y - py ) <= reach ) {
			hit = true;		/* a far marker smaller than the pointer */
			distance = visible[i].distance;
		}
		/* a brush model the camera is inside is picked last */
		if ( hit && marker.brush && distance <= 0.0f ) {
			distance = 1.0e8f;
		}
		if ( hit && distance < bestDistance ) {
			bestDistance = distance;
			best = marker.id;
		}
	}
	return best;
}

bool LinkInvolves( const std::pair<int, int> &link, int index )
{
	return link.first == index || link.second == index;
}

void DrawViewOverlay( const std::vector<Visible> &visible )
{
	const std::uint32_t selected = editor.doc.Selected();
	const Marker *selectedMarker = MarkerFor( selected );
	const int selectedIndex = selectedMarker ? selectedMarker->index : -1;
	const float thin = Rounded( Px( 1.0f ) );
	const float thick = Px( 2.5f );

	/* links: the selected entity's always, drawn over the rest, which are
	 * drawn when asked and as many as the selected entity's leave room for */
	const int linkMode = cl_entityEditorLinks ? cl_entityEditorLinks->integer : 1;
	const auto drawLink = [&]( const std::pair<int, int> &link, bool mine ) {
		const Marker &from = editor.markers[static_cast<std::size_t>( link.first )];
		const Marker &to = editor.markers[static_cast<std::size_t>( link.second )];
		ScreenPoint a, b;
		if ( !from.placed || !to.placed || !ProjectSegment( editor.view, from.point, to.point, a, b ) ) {
			return false;
		}
		const ui::Color color = mine ? ( link.first == selectedIndex ? ui::Color{ 1.0f, 0.62f, 0.25f, 1.0f }
			: ui::Color{ 0.35f, 0.85f, 1.0f, 1.0f } ) : ui::Color{ 0.80f, 0.80f, 0.85f, 0.35f };
		ViewLine( a.x, a.y, b.x, b.y, mine ? thick * 0.8f : thin, color );
		ArrowHead( a, b, Px( mine ? 10.0f : 7.0f ), mine ? thick * 0.8f : thin, color );
		return true;
	};
	int mineCount = 0;
	if ( selectedIndex >= 0 ) {
		for ( const auto &link : editor.links ) {
			mineCount += LinkInvolves( link, selectedIndex ) ? 1 : 0;
		}
	}
	if ( linkMode >= 1 ) {
		int othersDrawn = 0;
		const int othersAllowed = kMaxLinks - std::min( mineCount, kMaxLinks );
		for ( const auto &link : editor.links ) {
			if ( othersDrawn >= othersAllowed ) {
				break;
			}
			if ( !( selectedIndex >= 0 && LinkInvolves( link, selectedIndex ) ) && drawLink( link, false ) ) {
				othersDrawn++;
			}
		}
	}
	int mineDrawn = 0;
	for ( const auto &link : editor.links ) {
		if ( mineDrawn >= kMaxLinks || selectedIndex < 0 ) {
			break;
		}
		if ( LinkInvolves( link, selectedIndex ) && drawLink( link, true ) ) {
			mineDrawn++;
		}
	}

	/* markers, the farthest first so that nearer ones lie over them; the
	 * selected and the hovered entities are boxes wherever they are */
	editor.markersShown[0] = editor.markersShown[1] = 0;
	const auto drawMarker = [&]( const Visible &entry, bool isSelected, bool isPrimary, bool isHovered ) {
		const Marker &marker = editor.markers[static_cast<std::size_t>( entry.marker )];
		ui::Color color = marker.color.Alpha( entry.inPvs ? 1.0f : 0.35f );
		if ( isPrimary ) {
			color = { 1.0f, 0.95f, 0.45f, 1.0f };
		} else if ( isSelected ) {
			color = { 1.0f, 0.80f, 0.35f, 1.0f };
		} else if ( isHovered ) {
			color = marker.color.Lighten( 0.5f );
		}
		const float width = isSelected || isHovered ? thick : thin;
		if ( entry.shape == Shape::Box || isSelected || isHovered ) {
			editor.markersShown[0]++;
			Box3D( marker.mins, marker.maxs, width, color );
			if ( marker.hasYaw ) {
				vec3_t tip;
				const float length = std::max( 16.0f, ( marker.maxs[0] - marker.mins[0] ) );
				const float yaw = DEG2RAD( marker.yaw );
				VectorSet( tip, marker.point[0] + std::cos( yaw ) * length, marker.point[1] + std::sin( yaw ) * length,
					marker.point[2] );
				ScreenPoint a, b;
				if ( ProjectSegment( editor.view, marker.point, tip, a, b ) ) {
					ViewLine( a.x, a.y, b.x, b.y, width, color );
					ArrowHead( a, b, Px( 7.0f ), width, color );
				}
			}
		} else if ( entry.screen.x > -50000.0f ) {
			/* one fill, where a box is twelve lines */
			const float s = std::max( 1.0f, std::floor( Px( 2.0f ) ) );
			const float x = std::floor( entry.screen.x ), y = std::floor( entry.screen.y );
			if ( editor.viewport.Contains( x - s, y - s ) && editor.viewport.Contains( x + s, y + s ) ) {
				editor.overlay->AddRectFilled( ImVec2( x - s, y - s ), ImVec2( x + s, y + s ), color.Packed() );
				editor.markersShown[1]++;
			}
		}
	};
	const Visible *hoveredEntry = nullptr;
	const Visible *primaryEntry = nullptr;
	std::vector<const Visible *> selectedEntries;
	for ( int i = static_cast<int>( visible.size() ) - 1; i >= 0; i-- ) {
		const Visible &entry = visible[static_cast<std::size_t>( i )];
		const std::uint32_t id = editor.markers[static_cast<std::size_t>( entry.marker )].id;
		if ( id == selected ) {
			primaryEntry = &entry;
		} else if ( editor.doc.IsSelected( id ) ) {
			selectedEntries.push_back( &entry );
		} else if ( id == editor.hoverId ) {
			hoveredEntry = &entry;
		} else if ( entry.shape != Shape::None ) {
			drawMarker( entry, false, false, false );
		}
	}
	/* the hovered and the selected entities last: over the rest, and the
	 * last a crowded frame would leave out */
	if ( hoveredEntry ) {
		drawMarker( *hoveredEntry, false, false, true );
	}
	for ( const Visible *entry : selectedEntries ) {
		drawMarker( *entry, true, false, editor.markers[static_cast<std::size_t>( entry->marker )].id == editor.hoverId );
	}
	if ( primaryEntry ) {
		drawMarker( *primaryEntry, true, true, false );
	}

	/* labels, the nearest first, none over another */
	const int labelMode = cl_entityEditorLabels ? cl_entityEditorLabels->integer : 1;
	const float labelDistance = cl_entityEditorLabelDistance ? cl_entityEditorLabelDistance->value : 1536.0f;
	std::vector<ui::Rect> placed;
	int labels = 0;
	ImFont *font = ImGui::GetFont();
	const float size = LabelSize();
	const auto label = [&]( const Visible &entry, bool force ) {
		const Marker &marker = editor.markers[static_cast<std::size_t>( entry.marker )];
		vec3_t top;
		VectorCopy( marker.point, top );
		top[2] = marker.maxs[2];
		ScreenPoint at;
		if ( !Project( editor.view, top, at ) ) {
			return;
		}
		std::string text = marker.classname.empty() ? std::string( "(no classname)" ) : marker.classname;
		if ( !marker.targetname.empty() ) {
			text += " \"" + marker.targetname + "\"";
		}
		if ( text.size() > kLabelChars ) {
			text.resize( kLabelChars - 2 );
			text += "..";
		}
		const ImVec2 extent = font->CalcTextSizeA( size, FLT_MAX, 0.0f, text.c_str() );
		const float width = extent.x + Px( 6.0f );
		const ui::Rect box = { std::floor( at.x - width * 0.5f ), std::floor( at.y - size - Px( 8.0f ) ), width,
			size + Px( 4.0f ) };
		const ui::Rect inside = box.Intersect( editor.viewport );
		if ( inside.w < box.w || inside.h < box.h ) {
			return;
		}
		if ( !force ) {
			for ( const ui::Rect &other : placed ) {
				const ui::Rect overlap = other.Intersect( box );
				if ( overlap.w * overlap.h > box.w * box.h * 0.3f ) {
					return;
				}
			}
		}
		placed.push_back( box );
		const bool isSelected = editor.doc.IsSelected( marker.id );
		const float alpha = isSelected || force ? 1.0f : entry.inPvs ? 0.9f : 0.45f;
		editor.overlay->AddRectFilled( ImVec2( box.x, box.y ), ImVec2( box.Right(), box.Bottom() ),
			ui::Color{ 0.0f, 0.0f, 0.0f, 0.55f * alpha }.Packed() );
		editor.overlay->AddText( font, size, ImVec2( box.x + Px( 3.0f ), box.y + Px( 2.0f ) ),
			( isSelected ? ui::Color{ 1.0f, 0.95f, 0.45f, 1.0f } : marker.color.Lighten( 0.35f ) ).Alpha( alpha ).Packed(),
			text.c_str() );
		labels++;
	};
	for ( const Visible &entry : visible ) {
		const Marker &marker = editor.markers[static_cast<std::size_t>( entry.marker )];
		if ( marker.id == selected || marker.id == editor.hoverId ) {
			label( entry, true );
		}
	}
	if ( labelMode > 0 ) {
		for ( const Visible &entry : visible ) {
			if ( labels >= kMaxLabels ) {
				break;
			}
			const Marker &marker = editor.markers[static_cast<std::size_t>( entry.marker )];
			if ( marker.id == selected || marker.id == editor.hoverId || entry.shape == Shape::None ) {
				continue;
			}
			if ( labelMode < 2 && entry.distance > labelDistance ) {
				break;
			}
			label( entry, false );
		}
	}

	if ( selectedMarker && selectedMarker->placed ) {
		const int highlighted = editor.drag == Drag::AxisX ? 0 : editor.drag == Drag::AxisY ? 1
			: editor.drag == Drag::AxisZ ? 2 : editor.drag == Drag::Plane ? 3 : editor.hoverAxis;
		DrawGizmo( *selectedMarker, highlighted );
	}

	/* the box being drawn to select */
	if ( editor.boxing ) {
		const ImVec2 min( std::min( editor.boxX0, editor.boxX1 ), std::min( editor.boxY0, editor.boxY1 ) );
		const ImVec2 max( std::max( editor.boxX0, editor.boxX1 ), std::max( editor.boxY0, editor.boxY1 ) );
		editor.overlay->AddRectFilled( min, max, ui::Color{ 0.35f, 0.55f, 1.0f, 0.12f }.Packed() );
		editor.overlay->AddRect( min, max, ui::Color{ 0.45f, 0.65f, 1.0f, 0.9f }.Packed(), 0.0f, thin );
	}

	/* the crosshair the view picks with while it flies */
	if ( editor.flying || editor.looking ) {
		const float cx = editor.view.x + editor.view.width * 0.5f, cy = editor.view.y + editor.view.height * 0.5f;
		const float s = Px( 7.0f );
		const ui::Color white = { 1.0f, 1.0f, 1.0f, 0.85f };
		ViewLine( cx - s, cy, cx + s, cy, thin, white );
		ViewLine( cx, cy - s, cx, cy + s, thin, white );
	}
}

/* ---- dragging and boxing in the view ------------------------------------------------ */

/* A drag of the marker under the pointer, which moves every selected
 * entity with it. */
void BeginDrag( Drag kind, const Marker &marker, float px, float py )
{
	editor.drag = kind;
	editor.dragId = marker.id;
	VectorCopy( marker.point, editor.dragStart );
	editor.dragPressX = px;
	editor.dragPressY = py;
	editor.pressSelectsAlone = false;
	editor.dragMembers.clear();
	for ( const Marker *member : PlacedSelection() ) {
		DragMember entry;
		entry.id = member->id;
		entry.brush = member->brush;
		VectorCopy( member->point, entry.point );
		VectorCopy( member->offset, entry.offset );
		editor.dragMembers.push_back( entry );
	}
	if ( std::none_of( editor.dragMembers.begin(), editor.dragMembers.end(),
		[&marker]( const DragMember &member ) { return member.id == marker.id; } ) ) {
		DragMember entry;
		entry.id = marker.id;
		entry.brush = marker.brush;
		VectorCopy( marker.point, entry.point );
		VectorCopy( marker.offset, entry.offset );
		editor.dragMembers.push_back( entry );
	}
	vec3_t direction;
	PixelRay( editor.view, px, py, direction );
	if ( kind == Drag::AxisX || kind == Drag::AxisY || kind == Drag::AxisZ ) {
		const int axis = kind == Drag::AxisX ? 0 : kind == Drag::AxisY ? 1 : 2;
		/* the axis point nearest the pointer's ray, as a distance along it */
		vec3_t axisDir = { 0.0f, 0.0f, 0.0f };
		axisDir[axis] = 1.0f;
		vec3_t w0;
		VectorSubtract( editor.dragStart, editor.view.origin, w0 );
		const float b = DotProduct( axisDir, direction );
		const float d = DotProduct( axisDir, w0 ), e = DotProduct( direction, w0 );
		const float denominator = 1.0f - b * b;
		editor.dragParam = denominator > 1.0e-5f ? ( b * e - d ) / denominator : 0.0f;
	} else {
		/* the plane through the marker, level */
		if ( std::fabs( direction[2] ) > 1.0e-4f ) {
			const float s = ( editor.dragStart[2] - editor.view.origin[2] ) / direction[2];
			vec3_t hit;
			VectorMA( editor.view.origin, s, direction, hit );
			VectorSubtract( hit, editor.dragStart, editor.dragGrab );
		} else {
			VectorClear( editor.dragGrab );
		}
	}
}

void UpdateDrag( float px, float py )
{
	const Marker *marker = MarkerFor( editor.dragId );
	if ( !marker ) {
		editor.drag = Drag::None;
		return;
	}
	if ( editor.drag == Drag::PendingPlane ) {
		if ( std::fabs( px - editor.dragPressX ) + std::fabs( py - editor.dragPressY ) < kDragThreshold ) {
			return;
		}
		editor.doc.Checkpoint( editor.dragMembers.size() > 1 ? "move " + std::to_string( editor.dragMembers.size() )
			: std::string( "move" ) );
		editor.drag = Drag::Plane;
	}
	vec3_t direction;
	PixelRay( editor.view, px, py, direction );
	vec3_t point;
	VectorCopy( editor.dragStart, point );
	if ( editor.drag == Drag::Plane ) {
		if ( std::fabs( direction[2] ) < 1.0e-4f ) {
			return;
		}
		const float s = ( editor.dragStart[2] - editor.view.origin[2] ) / direction[2];
		if ( !( s > 0.0f ) || s > 65536.0f ) {
			return;
		}
		vec3_t hit;
		VectorMA( editor.view.origin, s, direction, hit );
		point[0] = hit[0] - editor.dragGrab[0];
		point[1] = hit[1] - editor.dragGrab[1];
		if ( marker->brush ) {
			point[0] = editor.dragStart[0] + SnapValue( point[0] - editor.dragStart[0] );
			point[1] = editor.dragStart[1] + SnapValue( point[1] - editor.dragStart[1] );
		} else {
			point[0] = SnapValue( point[0] );
			point[1] = SnapValue( point[1] );
		}
	} else {
		const int axis = editor.drag == Drag::AxisX ? 0 : editor.drag == Drag::AxisY ? 1 : 2;
		vec3_t axisDir = { 0.0f, 0.0f, 0.0f };
		axisDir[axis] = 1.0f;
		vec3_t w0;
		VectorSubtract( editor.dragStart, editor.view.origin, w0 );
		const float b = DotProduct( axisDir, direction );
		const float d = DotProduct( axisDir, w0 ), e = DotProduct( direction, w0 );
		const float denominator = 1.0f - b * b;
		if ( denominator < 1.0e-5f ) {
			return;
		}
		const float t = std::clamp( ( b * e - d ) / denominator - editor.dragParam, -65536.0f, 65536.0f );
		point[axis] = marker->brush ? editor.dragStart[axis] + SnapValue( t ) : SnapValue( editor.dragStart[axis] + t );
	}
	/* every member moves as far as the dragged marker did from where the drag
	 * began; a brush entity's origin key moves with its centre */
	vec3_t moved;
	VectorSubtract( point, editor.dragStart, moved );
	for ( const DragMember &member : editor.dragMembers ) {
		vec3_t origin;
		VectorAdd( member.brush ? member.offset : member.point, moved, origin );
		editor.doc.PlaceWithoutCheckpoint( member.id, origin );
	}
}

void EndDrag()
{
	if ( editor.drag == Drag::None ) {
		return;
	}
	if ( editor.drag != Drag::PendingPlane ) {
		editor.doc.DropCheckpointIfUnchanged();
		if ( const ee::Item *item = editor.doc.Find( editor.dragId ) ) {
			SetStatus( StatusKind::Info, editor.dragMembers.size() > 1
				? Format( "moved %d entities", static_cast<int>( editor.dragMembers.size() ) )
				: Format( "moved to %s", std::string( item->entity.Value( "origin" ) ).c_str() ) );
		}
	} else if ( editor.pressSelectsAlone ) {
		/* pressed and released without moving: a click, which selects one */
		editor.doc.Select( editor.dragId );
	}
	editor.drag = Drag::None;
	editor.dragMembers.clear();
	editor.pressSelectsAlone = false;
}

/* The drawn markers within the box, selected -- added to the selection
 * with Ctrl or Shift. */
void FinishBox( const std::vector<Visible> &visible )
{
	const float x0 = std::min( editor.boxX0, editor.boxX1 ), x1 = std::max( editor.boxX0, editor.boxX1 );
	const float y0 = std::min( editor.boxY0, editor.boxY1 ), y1 = std::max( editor.boxY0, editor.boxY1 );
	std::vector<std::uint32_t> ids;
	for ( const Visible &entry : visible ) {
		if ( entry.shape == Shape::None || entry.screen.x < x0 || entry.screen.x > x1
			|| entry.screen.y < y0 || entry.screen.y > y1 ) {
			continue;
		}
		ids.push_back( editor.markers[static_cast<std::size_t>( entry.marker )].id );
	}
	if ( editor.boxAdds ) {
		for ( const std::uint32_t id : ids ) {
			editor.doc.AddToSelection( id );
		}
	} else {
		editor.doc.SetSelection( ids, ids.empty() ? 0 : ids.front() );
	}
	editor.anchorId = editor.doc.Selected();
	editor.scrollToSelection = true;
	SetStatus( StatusKind::Info, Format( "%d selected", static_cast<int>( editor.doc.SelectionCount() ) ) );
}

/* ---- panels ---------------------------------------------------------------------- */

Layout ComputeLayout()
{
	Layout layout;
	const float width = static_cast<float>( cls.glconfig.vidWidth );
	const float height = static_cast<float>( cls.glconfig.vidHeight );
	const float bar = ImGui::GetFrameHeight();
	layout.screen = { 0.0f, 0.0f, width, height };
	layout.menu = { 0.0f, 0.0f, width, bar };
	layout.status = { 0.0f, height - bar, width, bar };
	const float top = layout.menu.Bottom(), bottom = layout.status.y;
	const float listWidth = editor.showList ? std::min( Px( 300.0f ), width * 0.28f ) : 0.0f;
	const float propsWidth = editor.showProps ? std::min( Px( 420.0f ), width * 0.34f ) : 0.0f;
	layout.list = { 0.0f, top, listWidth, bottom - top };
	layout.props = { width - propsWidth, top, propsWidth, bottom - top };
	layout.viewport = { listWidth, top, width - listWidth - propsWidth, bottom - top };
	return layout;
}

std::string RowText( const ee::Item &item, int index )
{
	std::string text = Format( "%4d  ", index );
	const std::string_view classname = item.entity.Classname();
	text += classname.empty() ? "(no classname)" : std::string( classname );
	const std::string_view targetname = item.entity.Value( "targetname" );
	if ( !targetname.empty() ) {
		text += "  \"" + std::string( targetname ) + "\"";
	}
	return text;
}

ImVec4 StatusColor( StatusKind kind )
{
	switch ( kind ) {
	case StatusKind::Good: return ImVec4( 0.45f, 0.92f, 0.50f, 1.0f );
	case StatusKind::Warning: return ImVec4( 1.0f, 0.82f, 0.35f, 1.0f );
	case StatusKind::Error: return ImVec4( 1.0f, 0.42f, 0.38f, 1.0f );
	case StatusKind::Info: break;
	}
	return ImVec4( 0.85f, 0.87f, 0.90f, 1.0f );
}

/* A panel fixed in its place: no title bar to drag, no edge to resize. */
bool BeginPanel( const char *name, const ui::Rect &rect, ImGuiWindowFlags extra = 0 )
{
	ImGui::SetNextWindowPos( ImVec2( rect.x, rect.y ), ImGuiCond_Always );
	ImGui::SetNextWindowSize( ImVec2( rect.w, rect.h ), ImGuiCond_Always );
	return ImGui::Begin( name, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
		| extra );
}

/* An edit box over a value: what is typed is kept when the box lets go --
 * Enter, Tab, a click elsewhere -- and Escape takes it back. The box being
 * typed in keeps its own text from frame to frame. */
void EditField( const char *id, const std::string &value, const char *hint,
	std::function<void( const std::string & )> commit )
{
	const ImGuiID widget = ImGui::GetID( id );
	const bool mine = editor.fieldId == widget;
	std::string &buffer = mine ? editor.fieldBuffer : editor.fieldScratch;
	if ( !mine ) {
		buffer = value;
	}
	ImGui::SetNextItemWidth( -FLT_MIN );
	if ( hint ) {
		ImGui::InputTextWithHint( id, hint, &buffer );
	} else {
		ImGui::InputText( id, &buffer );
	}
	if ( ImGui::IsItemActivated() ) {
		editor.fieldId = widget;
		editor.fieldBuffer = buffer;
		editor.fieldCommit = commit;
	}
	if ( editor.fieldId == widget ) {
		editor.fieldSeen = true;
		if ( ImGui::IsItemDeactivatedAfterEdit() ) {
			const std::string text = editor.fieldBuffer;
			editor.fieldId = 0;
			editor.fieldCommit = nullptr;
			commit( text );
		} else if ( !ImGui::IsItemActive() ) {
			editor.fieldId = 0;
			editor.fieldCommit = nullptr;
		}
	}
}

void Act( std::function<void()> action )
{
	editor.actions.push_back( std::move( action ) );
}

void DrawMenuBar( const Layout &layout )
{
	if ( !ImGui::BeginMainMenuBar() ) {
		return;
	}
	const bool any = editor.doc.SelectionCount() > 0;
	if ( ImGui::BeginMenu( "File" ) ) {
		if ( ImGui::MenuItem( "Save", "Ctrl+S" ) ) Act( [] { Save(); } );
		ImGui::SetItemTooltip( "Write the entities to %s", editor.savePath.empty() ? "the map's .ent" : editor.savePath.c_str() );
		const bool canApply = editor.local || RemoteApplyAvailable();
		if ( ImGui::MenuItem( "Apply", "F5", false, canApply ) ) Act( [] { Apply(); } );
		ImGui::SetItemTooltip( editor.local ? "Save, and restart the level so the game spawns the edits"
			: "Save, and ask the server over rcon to load the map again" );
		if ( ImGui::MenuItem( "Reload map", "Ctrl+F5", false, canApply ) ) Act( [] { ReloadMap(); } );
		ImGui::SetItemTooltip( "Save, and load the map again from its start" );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Revert" ) ) Act( [] { Revert(); } );
		ImGui::SetItemTooltip( "Back to the entities the level runs (undoable)" );
		if ( ImGui::MenuItem( "BSP entities" ) ) Act( [] { LoadBspEntities(); } );
		ImGui::SetItemTooltip( "The map's own entities, ignoring the override (undoable)" );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Close", "Esc" ) ) Act( [] { Close(); } );
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Edit" ) ) {
		const std::string undo = editor.doc.CanUndo() ? "Undo " + std::string( editor.doc.UndoLabel() ) : "Undo";
		const std::string redo = editor.doc.CanRedo() ? "Redo " + std::string( editor.doc.RedoLabel() ) : "Redo";
		if ( ImGui::MenuItem( undo.c_str(), "Ctrl+Z", false, editor.doc.CanUndo() ) ) Act( [] { Undo(); } );
		if ( ImGui::MenuItem( redo.c_str(), "Ctrl+Y", false, editor.doc.CanRedo() ) ) Act( [] { Redo(); } );
		ImGui::Separator();
		if ( ImGui::MenuItem( "New entity...", "N" ) ) editor.openNewPrompt = true;
		ImGui::SetItemTooltip( "Make one where the centre of the view meets the world" );
		if ( ImGui::MenuItem( "Duplicate", "Ctrl+D", false, any ) ) Act( [] { Duplicate(); } );
		ImGui::SetItemTooltip( "Copy the selection a grid step aside; the copies become the selection" );
		if ( ImGui::MenuItem( "Copy", "Ctrl+C", false, any ) ) Act( [] { CopyEntities( false ); } );
		ImGui::SetItemTooltip( "Put the selection on the clipboard as entity text" );
		if ( ImGui::MenuItem( "Cut", "Ctrl+X", false, any ) ) Act( [] { CopyEntities( true ); } );
		ImGui::SetItemTooltip( "Copy the selection as entity text, then delete it" );
		if ( ImGui::MenuItem( "Paste", "Ctrl+V" ) ) Act( [] { PasteEntities(); } );
		ImGui::SetItemTooltip( "Add the entity text on the clipboard, from this map, another or a text editor" );
		if ( ImGui::MenuItem( "Delete", "Del", false, any ) ) Act( [] { Delete(); } );
		ImGui::SetItemTooltip( "Delete the selection; the worldspawn stays" );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Drop to floor", "End", false, any ) ) Act( [] { DropToFloor(); } );
		ImGui::SetItemTooltip( "Set each selected entity on the floor below it" );
		if ( ImGui::MenuItem( "Move to the view's centre", "M", false, any ) ) {
			Act( [] { MoveToAim( editor.view.x + editor.view.width * 0.5f, editor.view.y + editor.view.height * 0.5f ); } );
		}
		ImGui::SetItemTooltip( "Move the primary entity where the centre of the view meets the world, the rest with it" );
		if ( ImGui::MenuItem( "Turn 15 clockwise", "Q", false, any ) ) Act( [] { Rotate( -15.0f ); } );
		if ( ImGui::MenuItem( "Turn 15 anticlockwise", "E", false, any ) ) Act( [] { Rotate( 15.0f ); } );
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Select" ) ) {
		if ( ImGui::MenuItem( "All listed", "Ctrl+A" ) ) Act( [] { SelectListed(); } );
		ImGui::SetItemTooltip( "Select every entity the list shows: filter it first" );
		if ( ImGui::MenuItem( "None", "Esc", false, any ) ) Act( [] { editor.doc.Select( 0 ); } );
		if ( ImGui::MenuItem( "Previous", "," ) ) Act( [] { SelectNeighbour( -1 ); } );
		if ( ImGui::MenuItem( "Next", "." ) ) Act( [] { SelectNeighbour( 1 ); } );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Frame the selected entity", "F", false, any ) ) Act( [] { FocusOn( editor.doc.Selected() ); } );
		if ( ImGui::MenuItem( "Filter the list", "Ctrl+F" ) ) {
			editor.showList = true;
			editor.focusFilter = true;
		}
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "View" ) ) {
		bool snap = Snapping();
		if ( ImGui::MenuItem( "Snap to the grid", "G", &snap ) ) Cvar_SetIntegerValue( "cl_entityEditorSnap", snap ? 1 : 0 );
		if ( ImGui::BeginMenu( "Grid" ) ) {
			for ( const float size : kGridSizes ) {
				const std::string name = et::FormatNumber( size );
				if ( ImGui::MenuItem( name.c_str(), nullptr, std::fabs( Grid() - size ) < 0.01f ) ) {
					Cvar_Set( "cl_entityEditorGrid", name.c_str() );
				}
			}
			ImGui::EndMenu();
		}
		const int labels = cl_entityEditorLabels ? cl_entityEditorLabels->integer : 1;
		if ( ImGui::BeginMenu( "Labels" ) ) {
			static const char *names[3] = { "None", "Near", "All" };
			for ( int mode = 0; mode < 3; mode++ ) {
				if ( ImGui::MenuItem( names[mode], mode == 0 ? "L" : nullptr, labels == mode ) ) {
					Cvar_SetIntegerValue( "cl_entityEditorLabels", mode );
				}
			}
			ImGui::EndMenu();
		}
		bool links = !cl_entityEditorLinks || cl_entityEditorLinks->integer;
		if ( ImGui::MenuItem( "Every target link", "K", &links ) ) Cvar_SetIntegerValue( "cl_entityEditorLinks", links ? 1 : 0 );
		ImGui::SetItemTooltip( "Draw every entity's links to its targets, not only the selected one's" );
		ImGui::Separator();
		ImGui::MenuItem( "Entity list", nullptr, &editor.showList );
		ImGui::MenuItem( "Keys panel", nullptr, &editor.showProps );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Back to the player's view", "Home" ) ) Act( [] { CameraFromPlayer(); } );
		if ( ImGui::MenuItem( "Fly", "Tab", editor.flying ) ) Act( [] { SetFlying( !editor.flying ); } );
		ImGui::SetItemTooltip( "The mouse turns the view without the right button; W A S D fly" );
		float speed = cl_entityEditorSpeed ? cl_entityEditorSpeed->value : 400.0f;
		ImGui::SetNextItemWidth( Px( 160.0f ) );
		if ( ImGui::SliderFloat( "Speed", &speed, 16.0f, 4096.0f, "%.0f", ImGuiSliderFlags_Logarithmic ) ) {
			Cvar_SetValue( "cl_entityEditorSpeed", speed );
		}
		ImGui::SetItemTooltip( "How fast the camera flies, in units a second" );
		float fov = cl_entityEditorFov ? cl_entityEditorFov->value : 90.0f;
		ImGui::SetNextItemWidth( Px( 160.0f ) );
		if ( ImGui::SliderFloat( "Field of view", &fov, 10.0f, 140.0f, "%.0f" ) ) {
			Cvar_SetValue( "cl_entityEditorFov", fov );
		}
		ImGui::SetItemTooltip( "The camera's field of view across a 4:3 screen; a wider screen keeps its height" );
		float scale = cl_entityEditorScale ? cl_entityEditorScale->value : 0.0f;
		ImGui::SetNextItemWidth( Px( 160.0f ) );
		if ( ImGui::SliderFloat( "Panel size", &scale, 0.0f, 4.0f, scale > 0.0f ? "%.2f" : "auto" ) ) {
			Cvar_SetValue( "cl_entityEditorScale", scale < 0.5f ? 0.0f : scale );
		}
		ImGui::SetItemTooltip( "The panels' and text's size; auto follows the screen's height" );
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Help" ) ) {
		if ( ImGui::MenuItem( "Keys and mouse", "F1", editor.help ) ) editor.help = !editor.help;
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if ( ImGui::MenuItem( "Save" ) ) Act( [] { Save(); } );
	ImGui::SetItemTooltip( "Write the entities to %s (Ctrl+S)", editor.savePath.empty() ? "the map's .ent" : editor.savePath.c_str() );
	if ( ImGui::MenuItem( "Apply", nullptr, false, editor.local || RemoteApplyAvailable() ) ) Act( [] { Apply(); } );
	ImGui::SetItemTooltip( editor.local ? "Save, and restart the level (F5)"
		: RemoteApplyAvailable() ? "Save, and ask the server over rcon to load the map again (F5)"
		: "A map on this machine's server restarts; another server needs its rconPassword set" );
	/* the map's name, and whether it is saved, at the bar's end */
	const std::string title = editor.mapPath + ( editor.doc.Dirty() ? "  *" : "" );
	const float titleWidth = ImGui::CalcTextSize( title.c_str() ).x;
	const float right = layout.menu.w - titleWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f;
	if ( right > ImGui::GetCursorPosX() ) {
		ImGui::SetCursorPosX( right );
		ImGui::TextColored( ImVec4( 0.55f, 0.75f, 1.0f, 1.0f ), "%s", title.c_str() );
		ImGui::SetItemTooltip( "Saves to %s", editor.savePath.empty() ? "(no path)" : editor.savePath.c_str() );
	}
	ImGui::EndMainMenuBar();
}

void DrawEntityList( const ui::Rect &panel )
{
	if ( panel.w <= 0.0f ) {
		return;
	}
	UpdateFilter();
	const std::string title = Format( "Entities  %d/%d###entities", static_cast<int>( editor.filtered.size() ),
		static_cast<int>( editor.doc.Count() ) );
	if ( !BeginPanel( title.c_str(), panel ) ) {
		ImGui::End();
		return;
	}
	if ( editor.focusFilter ) {
		ImGui::SetKeyboardFocusHere();
		editor.focusFilter = false;
	}
	ImGui::SetNextItemWidth( -FLT_MIN );
	ImGui::InputTextWithHint( "##filter", "filter: light, team_, model=*3", &editor.filter );
	ImGui::SetItemTooltip( "Each word must appear in a key or value; key=value matches one pair" );
	UpdateFilter();

	if ( ImGui::BeginChild( "rows", ImVec2( 0.0f, 0.0f ), ImGuiChildFlags_None ) ) {
		const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
		const int count = static_cast<int>( editor.filtered.size() );
		if ( editor.scrollToSelection ) {
			editor.scrollToSelection = false;
			const auto at = std::find( editor.filtered.begin(), editor.filtered.end(), editor.doc.Selected() );
			if ( at != editor.filtered.end() ) {
				const float y = rowHeight * static_cast<float>( at - editor.filtered.begin() );
				const float visibleHeight = ImGui::GetWindowHeight();
				if ( y < ImGui::GetScrollY() || y + rowHeight > ImGui::GetScrollY() + visibleHeight ) {
					ImGui::SetScrollY( std::max( 0.0f, y - visibleHeight * 0.5f ) );
				}
			}
		}
		ImGuiListClipper clipper;
		clipper.Begin( count, rowHeight );
		while ( clipper.Step() ) {
			for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ ) {
				const std::uint32_t id = editor.filtered[static_cast<std::size_t>( row )];
				const int index = editor.doc.IndexOf( id );
				if ( index < 0 ) {
					ImGui::Dummy( ImVec2( 1.0f, rowHeight ) );
					continue;
				}
				const ee::Item &item = editor.doc.Items()[static_cast<std::size_t>( index )];
				const Marker *marker = MarkerFor( id );
				const ui::Color swatch = marker ? marker->color : CategoryColor( ee::Classify( item.entity ) );
				ImGui::PushID( static_cast<int>( id ) );
				/* the colour of its kind, a short bar before the row */
				const ImVec2 at = ImGui::GetCursorScreenPos();
				const float bar = Px( 4.0f );
				ImGui::GetWindowDrawList()->AddRectFilled( ImVec2( at.x, at.y + rowHeight * 0.2f ),
					ImVec2( at.x + bar, at.y + rowHeight * 0.75f ), swatch.Packed() );
				ImGui::SetCursorScreenPos( ImVec2( at.x + bar + Px( 4.0f ), at.y ) );
				const bool placed = marker && marker->placed;
				if ( !placed ) {
					ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled ) );
				}
				const std::string text = RowText( item, index );
				if ( ImGui::Selectable( text.c_str(), editor.doc.IsSelected( id ), ImGuiSelectableFlags_AllowDoubleClick ) ) {
					const ImGuiIO &io = ImGui::GetIO();
					if ( io.KeyShift && editor.anchorId ) {
						if ( !io.KeyCtrl ) {
							editor.doc.Select( 0 );
						}
						editor.doc.SelectRange( editor.anchorId, id );
					} else if ( io.KeyCtrl ) {
						editor.doc.ToggleSelection( id );
						editor.anchorId = id;
					} else {
						editor.doc.Select( id );
						editor.anchorId = id;
					}
					if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
						const std::uint32_t focus = id;
						Act( [focus] { FocusOn( focus ); } );
					}
				}
				if ( !placed ) {
					ImGui::PopStyleColor();
					ImGui::SetItemTooltip( "Stands nowhere: no origin, no brush model" );
				}
				ImGui::PopID();
			}
		}
	}
	ImGui::EndChild();
	ImGui::End();
}

void DrawKeys( const ui::Rect &panel )
{
	if ( panel.w <= 0.0f ) {
		return;
	}
	if ( !BeginPanel( "Keys###keys", panel ) ) {
		ImGui::End();
		return;
	}
	const ee::Item *item = editor.doc.SelectedItem();
	if ( !item ) {
		ImGui::TextUnformatted( "No entity selected" );
		ImGui::Spacing();
		ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled ) );
		ImGui::TextWrapped( "Click an entity in the view or the list; Ctrl+click or Shift+click adds to the selection, "
			"and a box dragged over empty view selects all in it. Hold the right mouse button to look, and fly with "
			"W A S D, Q and E. Drag an entity, or its arrows, to move it. N makes one; Ctrl+S saves; F5 applies. "
			"F1 lists every key." );
		ImGui::Spacing();
		ImGui::TextWrapped( "Read from: %s", editor.source.c_str() );
		ImGui::TextWrapped( "Saves to: %s", editor.savePath.empty() ? "(no path)" : editor.savePath.c_str() );
		ImGui::PopStyleColor();
		if ( !editor.parseNote.empty() ) {
			ImGui::PushStyleColor( ImGuiCol_Text, StatusColor( StatusKind::Warning ) );
			ImGui::TextWrapped( "%s", editor.parseNote.c_str() );
			ImGui::PopStyleColor();
		}
		ImGui::End();
		return;
	}

	const std::uint32_t id = item->id;
	const int index = editor.doc.IndexOf( id );
	const std::size_t selectedCount = editor.doc.SelectionCount();
	const std::string classname( item->entity.Classname() );
	ImGui::TextColored( ImVec4( 0.55f, 0.75f, 1.0f, 1.0f ), "#%d  %s", index,
		classname.empty() ? "(no classname)" : classname.c_str() );
	if ( selectedCount > 1 ) {
		ImGui::SameLine();
		ImGui::TextDisabled( "(and %d more selected)", static_cast<int>( selectedCount - 1 ) );
	}

	/* what a game would make of it */
	for ( const ee::Finding &finding : ee::CheckEntity( item->entity, Limits(), index == 0 ) ) {
		ImGui::PushStyleColor( ImGuiCol_Text, StatusColor( finding.error ? StatusKind::Error : StatusKind::Warning ) );
		ImGui::TextWrapped( "%s", finding.text.c_str() );
		ImGui::PopStyleColor();
	}

	/* the actions, on the whole selection */
	const Marker *marker = MarkerFor( id );
	const bool placed = marker && marker->placed;
	/* buttons in rows as wide as the panel */
	float rowLeft = ImGui::GetContentRegionAvail().x;
	bool rowStarted = false;
	const auto action = [&]( const char *label, const char *tip, bool enabled ) {
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float width = ImGui::CalcTextSize( label ).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		if ( rowStarted && width + spacing <= rowLeft ) {
			ImGui::SameLine();
			rowLeft -= width + spacing;
		} else {
			rowLeft = ImGui::GetContentRegionAvail().x - width;
			rowStarted = true;
		}
		ImGui::BeginDisabled( !enabled );
		const bool clicked = ImGui::Button( label );
		ImGui::EndDisabled();
		ImGui::SetItemTooltip( "%s", tip );
		return clicked;
	};
	if ( action( "Look at", "Frame it in the view (F)", placed ) ) Act( [id] { FocusOn( id ); } );
	if ( action( "Move here", "Put it where the view's centre meets the world (M)", placed ) ) {
		Act( [] { MoveToAim( editor.view.x + editor.view.width * 0.5f, editor.view.y + editor.view.height * 0.5f ); } );
	}
	if ( action( "Drop", "Set the selection on the floor below (End)", placed ) ) Act( [] { DropToFloor(); } );
	if ( action( "Turn -15", "Turn the selection 15 degrees clockwise (Q; Shift: 90)", index > 0 ) ) Act( [] { Rotate( -15.0f ); } );
	if ( action( "Turn +15", "Turn the selection 15 degrees anticlockwise (E; Shift: 90)", index > 0 ) ) Act( [] { Rotate( 15.0f ); } );
	if ( action( "Duplicate", "Copy the selection a grid step aside (Ctrl+D)", index > 0 ) ) Act( [] { Duplicate(); } );
	if ( action( "Delete", "Delete the selection (Del)", index > 0 || selectedCount > 1 ) ) Act( [] { Delete(); } );
	ImGui::Separator();

	/* a key set on all of them */
	if ( selectedCount > 1 ) {
		ImGui::TextDisabled( "On all %d selected:", static_cast<int>( selectedCount ) );
		const float half = ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f - Px( 40.0f ) ) * 0.5f;
		ImGui::SetNextItemWidth( half );
		ImGui::InputTextWithHint( "##allkey", "key", &editor.allKey );
		ImGui::SameLine();
		ImGui::SetNextItemWidth( half );
		const bool entered = ImGui::InputTextWithHint( "##allvalue", "value", &editor.allValue,
			ImGuiInputTextFlags_EnterReturnsTrue );
		ImGui::SameLine();
		ImGui::BeginDisabled( editor.allKey.empty() );
		const bool set = ImGui::Button( "Set" );
		ImGui::EndDisabled();
		ImGui::SetItemTooltip( "Set this key on every selected entity" );
		if ( ( entered || set ) && !editor.allKey.empty() ) {
			if ( editor.doc.SetValueOnSelection( editor.allKey, editor.allValue ) ) {
				SetStatus( StatusKind::Info, Format( "set %s on %d", editor.allKey.c_str(), static_cast<int>( selectedCount ) ) );
			} else {
				SetStatus( StatusKind::Error, Format( "\"%s\" \"%s\" cannot be written", editor.allKey.c_str(),
					editor.allValue.c_str() ) );
			}
		}
		ImGui::Separator();
	}

	/* the keys and values, and below them the links, scrolled together */
	std::vector<std::uint32_t> targets, sources;
	editor.doc.Links( id, targets, sources );
	ImGui::BeginChild( "pairsarea", ImVec2( 0.0f, 0.0f ) );
	if ( ImGui::BeginTable( "pairs", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg ) ) {
		ImGui::TableSetupColumn( "key", ImGuiTableColumnFlags_WidthStretch, 0.38f );
		ImGui::TableSetupColumn( "value", ImGuiTableColumnFlags_WidthStretch, 0.62f );
		ImGui::TableSetupColumn( "x", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight() );
		ImGui::PushID( static_cast<int>( id ) );
		for ( std::size_t pair = 0; pair < item->entity.pairs.size(); pair++ ) {
			const et::KeyValue current = item->entity.pairs[pair];
			ImGui::PushID( static_cast<int>( pair ) );
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			EditField( "##key", current.key, nullptr, [id, pair]( const std::string &text ) {
				if ( !editor.doc.RenameKey( id, pair, text ) ) {
					SetStatus( StatusKind::Error, Format( "\"%s\": %s", text.c_str(),
						et::DescribeProblem( et::CheckToken( text, true ) ) ) );
				}
			} );
			ImGui::TableNextColumn();
			const bool linkKey = ee::IsTargetKey( current.key ) || et::EqualsNoCase( current.key, "targetname" );
			if ( linkKey ) {
				ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.78f, 0.45f, 1.0f ) );
			}
			EditField( "##value", current.value, nullptr, [id, pair]( const std::string &text ) {
				if ( !editor.doc.SetPairValue( id, pair, text ) ) {
					SetStatus( StatusKind::Error, Format( "%s", et::DescribeProblem( et::CheckToken( text, false ) ) ) );
				}
			} );
			if ( linkKey ) {
				ImGui::PopStyleColor();
			}
			ImGui::TableNextColumn();
			if ( ImGui::Button( "x", ImVec2( ImGui::GetFrameHeight(), 0.0f ) ) ) {
				Act( [id, pair] { editor.doc.RemovePair( id, pair ); } );
			}
			ImGui::SetItemTooltip( "Remove this key" );
			ImGui::PopID();
		}
		/* a new key */
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if ( editor.focusNewKey ) {
			ImGui::SetKeyboardFocusHere();
			editor.focusNewKey = false;
		}
		ImGui::SetNextItemWidth( -FLT_MIN );
		ImGui::InputTextWithHint( "##newkey", "new key", &editor.newKey );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( -FLT_MIN );
		const bool entered = ImGui::InputTextWithHint( "##newvalue", "value", &editor.newValue,
			ImGuiInputTextFlags_EnterReturnsTrue );
		ImGui::TableNextColumn();
		ImGui::BeginDisabled( editor.newKey.empty() );
		const bool add = ImGui::Button( "+", ImVec2( ImGui::GetFrameHeight(), 0.0f ) );
		ImGui::EndDisabled();
		ImGui::SetItemTooltip( "Add this key (Enter in the value)" );
		if ( ( entered || add ) && !editor.newKey.empty() ) {
			if ( editor.doc.AddPair( id, editor.newKey, editor.newValue ) ) {
				SetStatus( StatusKind::Info, Format( "added %s", editor.newKey.c_str() ) );
				editor.newKey.clear();
				editor.newValue.clear();
				editor.focusNewKey = true;
			} else {
				SetStatus( StatusKind::Error, Format( "\"%s\" \"%s\" cannot be written", editor.newKey.c_str(),
					editor.newValue.c_str() ) );
			}
		}
		ImGui::PopID();
		ImGui::EndTable();
	}
	/* what it targets and what targets it, the panel's width; a click
	 * selects one */
	const auto linkList = [&]( const char *title, const std::vector<std::uint32_t> &ids ) {
		if ( ids.empty() ) {
			return;
		}
		ImGui::Spacing();
		ImGui::TextDisabled( "%s", title );
		for ( const std::uint32_t other : ids ) {
			const int otherIndex = editor.doc.IndexOf( other );
			if ( otherIndex < 0 ) {
				continue;
			}
			ImGui::PushID( static_cast<int>( other ) );
			const ee::Item &linked = editor.doc.Items()[static_cast<std::size_t>( otherIndex )];
			if ( ImGui::Selectable( RowText( linked, otherIndex ).c_str(), false ) ) {
				Act( [other] {
					editor.doc.Select( other );
					editor.anchorId = other;
					editor.scrollToSelection = true;
				} );
			}
			ImGui::PopID();
		}
	};
	linkList( "Targets:", targets );
	linkList( "Targeted by:", sources );
	ImGui::EndChild();
	ImGui::End();
}

void DrawStatus( const ui::Rect &bar )
{
	if ( !BeginPanel( "##status", bar, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoScrollWithMouse ) ) {
		ImGui::End();
		return;
	}
	const std::string right = Format( "%s   camera %s   speed %s", editor.local ? "local server" : RemoteApplyAvailable() ? "server by rcon" : "save only",
		et::FormatVector( editor.cameraOrigin ).c_str(),
		et::FormatNumber( ( cl_entityEditorSpeed ? cl_entityEditorSpeed->value : 400.0f ) * editor.speedScale ).c_str() );
	const float rightWidth = ImGui::CalcTextSize( right.c_str() ).x;
	const std::string message = editor.looking || editor.flying
		? std::string( "Flying: W A S D move, Q and E down and up, Shift faster, wheel sets speed" ) : editor.status;
	ImGui::PushClipRect( ImGui::GetCursorScreenPos(),
		ImVec2( bar.Right() - rightWidth - ImGui::GetStyle().ItemSpacing.x * 3.0f, bar.Bottom() ), true );
	ImGui::TextColored( editor.looking || editor.flying ? StatusColor( StatusKind::Info ) : StatusColor( editor.statusKind ),
		"%s", message.c_str() );
	ImGui::PopClipRect();
	ImGui::SameLine( bar.w - rightWidth - ImGui::GetStyle().WindowPadding.x );
	ImGui::TextDisabled( "%s", right.c_str() );
	ImGui::End();
}

void DrawNewPrompt( const Layout &layout )
{
	if ( editor.openNewPrompt ) {
		editor.openNewPrompt = false;
		editor.newClassname.clear();
		ImGui::OpenPopup( "New entity" );
	}
	ImGui::SetNextWindowPos( ImVec2( layout.viewport.x + layout.viewport.w * 0.5f, layout.viewport.y + Px( 40.0f ) ),
		ImGuiCond_Appearing, ImVec2( 0.5f, 0.0f ) );
	ImGui::SetNextWindowSize( ImVec2( std::min( Px( 420.0f ), layout.viewport.w - Px( 20.0f ) ), 0.0f ) );
	if ( !ImGui::BeginPopupModal( "New entity", nullptr, ImGuiWindowFlags_NoSavedSettings ) ) {
		return;
	}
	ImGui::TextUnformatted( "Classname" );
	if ( ImGui::IsWindowAppearing() ) {
		ImGui::SetKeyboardFocusHere();
	}
	ImGui::SetNextItemWidth( -FLT_MIN );
	bool create = ImGui::InputTextWithHint( "##classname", "info_player_deathmatch", &editor.newClassname,
		ImGuiInputTextFlags_EnterReturnsTrue );
	/* the map's own, the most used first */
	int shown = 0;
	for ( const std::string &name : editor.doc.Classnames() ) {
		if ( !editor.newClassname.empty() && !ee::ContainsNoCase( name, editor.newClassname ) ) {
			continue;
		}
		if ( ImGui::Selectable( name.c_str() ) ) {
			editor.newClassname = name;
			create = true;
		}
		if ( ++shown >= 12 ) {
			break;
		}
	}
	ImGui::Separator();
	ImGui::BeginDisabled( editor.newClassname.empty() );
	if ( ImGui::Button( "Create" ) ) {
		create = true;
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip( "Make it where the view's centre meets the world (Enter)" );
	ImGui::SameLine();
	if ( ImGui::Button( "Cancel" ) || ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
		ImGui::CloseCurrentPopup();
	} else if ( create && !editor.newClassname.empty() ) {
		const std::string classname = editor.newClassname;
		Act( [classname] { Create( classname ); } );
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void DrawHelp( const Layout &layout )
{
	if ( !editor.help ) {
		return;
	}
	ImGui::SetNextWindowPos( ImVec2( layout.viewport.x + layout.viewport.w * 0.5f, layout.viewport.y + layout.viewport.h * 0.5f ),
		ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );
	if ( !ImGui::Begin( "Keys and mouse", &editor.help, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoSavedSettings ) ) {
		ImGui::End();
		return;
	}
	static const char *const rows[][2] = {
		{ "Left click", "select; Ctrl+click adds or takes away, Shift+click adds (a range in the list)" },
		{ "Left drag", "move the selection level; on an arrow, along that axis" },
		{ "Drag on empty view", "a box: select what is in it (Ctrl or Shift: add)" },
		{ "Double click", "frame the entity" },
		{ "Right button held", "look; W A S D, Q E, Space, C fly; wheel sets the speed" },
		{ "Wheel", "move forward and back" },
		{ "Tab", "fly without holding the button" },
		{ "F", "frame the selected entity" },
		{ "Arrows, Page Up, Page Down", "nudge the selection a grid step (Shift: four)" },
		{ "Q, E", "turn the selection 15 degrees (Shift: 90)" },
		{ "M", "move the selection to the point under the cursor" },
		{ "End", "set the selection on the floor" },
		{ "N", "make an entity" },
		{ "Ctrl+D", "duplicate the selection a grid step aside" },
		{ "Ctrl+C, X, V", "copy, cut and paste entities as text" },
		{ "Del", "delete the selection" },
		{ "Ctrl+Z, Ctrl+Y", "undo, redo" },
		{ "Ctrl+A", "select every entity the list shows" },
		{ ", and .", "the previous and the next entity in the list" },
		{ "G, [ and ]", "grid on and off, smaller and larger" },
		{ "L, K", "labels, links" },
		{ "Ctrl+F", "filter the list" },
		{ "Home", "back to the player's view" },
		{ "Ctrl+S", "save maps/<map>.ent" },
		{ "F5, Ctrl+F5", "save and restart the level; save and reload the map" },
		{ "Esc", "leave a box, stop flying, deselect, then close" },
	};
	if ( ImGui::BeginTable( "keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit ) ) {
		for ( const auto &row : rows ) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextColored( ImVec4( 0.55f, 0.75f, 1.0f, 1.0f ), "%s", row[0] );
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( row[1] );
		}
		ImGui::EndTable();
	}
	ImGui::Spacing();
	ImGui::TextDisabled( "The server reads maps/<map>.ent in place of the BSP's entities while sv_entityOverrides is 1." );
	ImGui::TextDisabled( "The console command entityeditor does all of this too: entityeditor help lists it." );
	ImGui::End();
}

/* ---- the frame's keys ---------------------------------------------------------- */

void HandleShortcuts( const Layout &layout )
{
	const ImGuiIO &io = ImGui::GetIO();
	std::vector<Shortcut> shortcuts;
	shortcuts.swap( editor.shortcuts );
	/* a box being typed in, or a prompt up, has the keys */
	if ( io.WantTextInput || ImGui::IsPopupOpen( "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) ) {
		return;
	}
	for ( const Shortcut &press : shortcuts ) {
		const int key = press.key;
		if ( key == K_ESCAPE ) {
			if ( editor.help ) {
				editor.help = false;
			} else if ( editor.flying ) {
				SetFlying( false );
			} else if ( editor.doc.SelectionCount() ) {
				editor.doc.Select( 0 );
			} else {
				Close();
				return;
			}
			continue;
		}
		if ( key == K_TAB ) {
			SetFlying( !editor.flying );
			continue;
		}
		if ( key == K_F1 ) {
			editor.help = !editor.help;
			continue;
		}
		if ( key == K_F5 ) {
			press.ctrl ? ReloadMap() : Apply();
			continue;
		}
		if ( press.ctrl ) {
			switch ( key ) {
			case 'z': press.shift ? Redo() : Undo(); break;
			case 'y': Redo(); break;
			case 's': Save(); break;
			case 'd': Duplicate(); break;
			case 'c': CopyEntities( false ); break;
			case 'x': CopyEntities( true ); break;
			case 'v': PasteEntities(); break;
			case 'a': SelectListed(); break;
			case 'f': editor.showList = true; editor.focusFilter = true; break;
			default: break;
			}
			continue;
		}
		const float step = press.shift ? 4.0f : 1.0f;
		switch ( key ) {
		case K_DEL: Delete(); break;
		case K_UPARROW: Nudge( 0.0f, step, 0.0f ); break;
		case K_DOWNARROW: Nudge( 0.0f, -step, 0.0f ); break;
		case K_LEFTARROW: Nudge( -step, 0.0f, 0.0f ); break;
		case K_RIGHTARROW: Nudge( step, 0.0f, 0.0f ); break;
		case K_PGUP: Nudge( 0.0f, 0.0f, step ); break;
		case K_PGDN: Nudge( 0.0f, 0.0f, -step ); break;
		case K_END: DropToFloor(); break;
		case K_HOME: CameraFromPlayer(); break;
		case 'f': FocusOn( editor.doc.Selected() ); break;
		case 'q': Rotate( press.shift ? -90.0f : -15.0f ); break;
		case 'e': Rotate( press.shift ? 90.0f : 15.0f ); break;
		case 'm': {
			const bool pointer = !( editor.looking || editor.flying ) && io.MousePos.x > -FLT_MAX * 0.5f;
			MoveToAim( pointer ? io.MousePos.x : layout.viewport.x + layout.viewport.w * 0.5f,
				pointer ? io.MousePos.y : layout.viewport.y + layout.viewport.h * 0.5f );
			break;
		}
		case 'n': editor.openNewPrompt = true; break;
		case ',': SelectNeighbour( -1 ); break;
		case '.': SelectNeighbour( 1 ); break;
		case 'g': Cvar_SetIntegerValue( "cl_entityEditorSnap", Snapping() ? 0 : 1 ); break;
		case '[': CycleGrid( -1 ); break;
		case ']': CycleGrid( 1 ); break;
		case 'l': Cvar_SetIntegerValue( "cl_entityEditorLabels",
			( ( cl_entityEditorLabels ? cl_entityEditorLabels->integer : 1 ) + 1 ) % 3 ); break;
		case 'k': Cvar_SetIntegerValue( "cl_entityEditorLinks",
			( !cl_entityEditorLinks || cl_entityEditorLinks->integer ) ? 0 : 1 ); break;
		default: break;
		}
	}
}

/* ---- the view under the pointer --------------------------------------------------- */

void ViewInteraction( const std::vector<Visible> &visible )
{
	const ImGuiIO &io = ImGui::GetIO();
	const bool aiming = editor.flying || editor.looking;
	const float px = aiming ? editor.view.x + editor.view.width * 0.5f : io.MousePos.x;
	const float py = aiming ? editor.view.y + editor.view.height * 0.5f : io.MousePos.y;
	const bool overView = aiming || ( !io.WantCaptureMouse && editor.viewport.Contains( px, py ) );

	if ( editor.drag != Drag::None ) {
		if ( io.MouseDown[ImGuiMouseButton_Left] ) {
			UpdateDrag( px, py );
		} else {
			EndDrag();
		}
		editor.hoverId = editor.dragId;
		return;
	}
	if ( editor.boxPending || editor.boxing ) {
		editor.boxX1 = px;
		editor.boxY1 = py;
		if ( editor.boxPending && std::fabs( px - editor.boxX0 ) + std::fabs( py - editor.boxY0 ) >= kDragThreshold ) {
			editor.boxPending = false;
			editor.boxing = true;
		}
		if ( !io.MouseDown[ImGuiMouseButton_Left] ) {
			if ( editor.boxing ) {
				FinishBox( visible );
			} else if ( !editor.boxAdds ) {
				editor.doc.Select( 0 );		/* a click on empty view */
			}
			editor.boxPending = editor.boxing = false;
		}
		return;
	}
	editor.hoverId = 0;
	editor.hoverAxis = -1;
	if ( !overView ) {
		return;
	}
	const Marker *primary = MarkerFor( editor.doc.Selected() );
	if ( primary && primary->placed && !aiming ) {
		editor.hoverAxis = GizmoHandleAt( *primary, px, py );
	}
	if ( editor.hoverAxis < 0 ) {
		editor.hoverId = PickAt( visible, px, py );
	}

	if ( io.MouseWheel != 0.0f && !aiming ) {
		vec3_t forward;
		AngleVectors( editor.cameraAngles, forward, nullptr, nullptr );
		VectorMA( editor.cameraOrigin, io.MouseWheel * 64.0f * editor.speedScale, forward, editor.cameraOrigin );
	}

	if ( !io.MouseClicked[ImGuiMouseButton_Left] ) {
		return;
	}
	CommitField();
	if ( editor.hoverAxis >= 0 && primary ) {
		const Drag kinds[4] = { Drag::AxisX, Drag::AxisY, Drag::AxisZ, Drag::Plane };
		editor.doc.Checkpoint( "move" );
		BeginDrag( kinds[editor.hoverAxis], *primary, px, py );
		return;
	}
	if ( editor.hoverId ) {
		const std::uint32_t hit = editor.hoverId;
		bool alone = false;
		if ( io.KeyCtrl ) {
			editor.doc.ToggleSelection( hit );
		} else if ( io.KeyShift || editor.doc.IsSelected( hit ) ) {
			/* a selected entity pressed keeps the selection, to drag it; let
			 * go without dragging, the press selects it alone */
			alone = !io.KeyShift && editor.doc.SelectionCount() > 1;
			editor.doc.AddToSelection( hit );
		} else {
			editor.doc.Select( hit );
		}
		editor.anchorId = hit;
		editor.scrollToSelection = true;
		const Marker *marker = MarkerFor( hit );
		if ( io.MouseDoubleClicked[ImGuiMouseButton_Left] ) {
			FocusOn( hit );
		} else if ( marker && marker->placed && !aiming && editor.doc.IsSelected( hit ) ) {
			BeginDrag( Drag::PendingPlane, *marker, px, py );
			editor.pressSelectsAlone = alone;
		} else if ( alone ) {
			editor.doc.Select( hit );
		}
		return;
	}
	if ( aiming ) {
		editor.doc.Select( 0 );
		return;
	}
	editor.boxPending = true;
	editor.boxAdds = io.KeyCtrl || io.KeyShift;
	editor.boxX0 = editor.boxX1 = px;
	editor.boxY0 = editor.boxY1 = py;
}

/* ---- the frame -------------------------------------------------------------------- */

void Frame()
{
	ImGuiIO &io = ImGui::GetIO();
	io.DisplaySize = ImVec2( static_cast<float>( cls.glconfig.vidWidth ), static_cast<float>( cls.glconfig.vidHeight ) );
	io.DeltaTime = std::clamp( static_cast<float>( cls.realtime - editor.frameTime ) * 0.001f, 0.001f, 0.25f );
	editor.frameTime = cls.realtime;
	const float scale = UiScale();
	if ( scale != editor.uiScale ) {
		editor.uiScale = scale;
		ApplyStyle( scale );
		editor.prewarm = true;
	}

	ImGui::NewFrame();
	if ( editor.prewarm ) {
		editor.prewarm = false;
		PrewarmGlyphs();
	}
	RebuildMarkers();
	const Layout layout = ComputeLayout();
	editor.viewport = layout.viewport;
	editor.fieldSeen = false;

	HandleShortcuts( layout );
	if ( editor.active ) {
		DrawMenuBar( layout );
		DrawEntityList( layout.list );
		DrawKeys( layout.props );
		DrawStatus( layout.status );
		DrawNewPrompt( layout );
		DrawHelp( layout );
	}
	/* the box being typed in went with what showed it: what it held is kept */
	if ( editor.fieldId && !editor.fieldSeen ) {
		CommitField();
		editor.fieldId = 0;
		editor.fieldCommit = nullptr;
	}
	std::vector<std::function<void()>> actions;
	actions.swap( editor.actions );
	for ( const std::function<void()> &action : actions ) {
		if ( editor.active ) {
			action();
		}
	}

	editor.overlay = ImGui::GetBackgroundDrawList();
	editor.overlay->PushClipRect( ImVec2( layout.viewport.x, layout.viewport.y ),
		ImVec2( layout.viewport.Right(), layout.viewport.Bottom() ), false );
	if ( editor.active && editor.view.valid ) {
		RebuildMarkers();
		const std::vector<Visible> visible = VisibleMarkers();
		ViewInteraction( visible );
		RebuildMarkers();
		DrawViewOverlay( visible );
	}
	editor.overlay->PopClipRect();
	ImGui::Render();

	ImDrawData *data = ImGui::GetDrawData();
	if ( !data ) {
		return;
	}
	UpdateTextures( *data );
	if ( !editor.active ) {
		return;
	}
	editor.viewDraw.Clear();
	editor.panelDraw.Clear();
	for ( const ImDrawList *list : data->CmdLists ) {
		ui::AppendDrawList( *list, list == editor.overlay ? editor.viewDraw : editor.panelDraw );
	}

	/* the view's overlay lies under the panels and the prompts; what the
	 * frame cannot hold is taken from its start, the links and the farthest
	 * markers, never from the panels */
	const int panelBytes = ListBytes( editor.panelDraw );
	const int viewBytes = ListBytes( editor.viewDraw );
	editor.frameBytes = editor.frameCommands = editor.frameDropped = 0;
	editor.lastColor[0] = -1.0f;
	Render( editor.viewDraw, std::max( 0, viewBytes - std::max( 0, kFrameBytes - panelBytes ) ) );
	Render( editor.panelDraw );
	re.SetColor( nullptr );
	editor.lastColor[0] = -1.0f;
}

/* ---- console commands ---------------------------------------------------------- */

std::uint32_t FindEntity( const char *what )
{
	if ( !what || !what[0] ) {
		return 0;
	}
	char *end = nullptr;
	const long number = std::strtol( what, &end, 10 );
	if ( end && !*end && number >= 0 && number < static_cast<long>( editor.doc.Count() ) ) {
		return editor.doc.Items()[static_cast<std::size_t>( number )].id;
	}
	for ( const ee::Item &item : editor.doc.Items() ) {
		if ( et::EqualsNoCase( item.entity.Value( "targetname" ), what ) ) {
			return item.id;
		}
	}
	return 0;
}

void PrintEntity( const ee::Item &item )
{
	Com_Printf( "#%d {\n", editor.doc.IndexOf( item.id ) );
	for ( const et::KeyValue &pair : item.entity.pairs ) {
		Com_Printf( "  \"%s\" \"%s\"\n", pair.key.c_str(), pair.value.c_str() );
	}
	Com_Printf( "}\n" );
}

void Command_f()
{
	const char *action = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "toggle";
	const auto is = [action]( const char *name ) { return !Q_stricmp( action, name ); };
	if ( is( "help" ) ) {
		Com_Printf(
			"entityeditor                      open or close the editor\n"
			"entityeditor open | close\n"
			"entityeditor save | apply | reload save; save and restart the level; save and load the map again\n"
			"entityeditor revert | bsp         back to the level's entities, or the BSP's own (undoable)\n"
			"entityeditor undo | redo\n"
			"entityeditor select <n|name>...   select by list number or targetname, one or many\n"
			"entityeditor select all | none    every entity the list shows, or none\n"
			"entityeditor next | prev\n"
			"entityeditor set <key> <value>    set a key on the selected entities\n"
			"entityeditor unset <key>          remove a key from them\n"
			"entityeditor new <classname>      make one where the view looks\n"
			"entityeditor duplicate | delete | drop | look\n"
			"entityeditor move <x> <y> <z>     place the primary entity there, the rest about it\n"
			"entityeditor nudge <x> <y> <z>    move the selection by that much\n"
			"entityeditor turn <degrees>\n"
			"entityeditor find <text>          filter the list\n"
			"entityeditor print                print the selected entities\n"
			"entityeditor list [text]          print the entities, or those matching\n"
			"entityeditor camera <x> <y> <z> [pitch yaw]  place the editor's camera\n"
			"entityeditor stats                what the last frame drew\n" );
		return;
	}
	if ( is( "stats" ) ) {
		Com_Printf( "Entity editor, last frame: %d boxes and %d dots over the view; %d renderer commands, about "
			"%d of %d KiB, %d KiB left out; %d interface picture%s uploaded under this renderer.\n",
			editor.markersShown[0], editor.markersShown[1], editor.frameCommands, ( editor.frameBytes + 1023 ) / 1024,
			kFrameBytes / 1024, ( editor.frameDropped + 1023 ) / 1024, editor.textureUploads,
			editor.textureUploads == 1 ? "" : "s" );
		return;
	}
	if ( is( "toggle" ) ) {
		editor.active ? Close() : Open();
		return;
	}
	if ( is( "open" ) ) {
		Open();
		return;
	}
	if ( is( "close" ) ) {
		Close();
		return;
	}
	if ( !editor.active ) {
		if ( !CanOpen( false ) ) {
			return;
		}
		/* the editing commands work on a document even while it is shut */
		if ( editor.doc.Count() == 0 || editor.mapPath != CurrentMapPath() ) {
			LoadDocument();
		}
	}
	CommitField();
	const ee::Item *selected = editor.doc.SelectedItem();
	const std::uint32_t id = selected ? selected->id : 0;
	if ( is( "save" ) ) {
		Save();
	} else if ( is( "apply" ) ) {
		Apply();
	} else if ( is( "reload" ) ) {
		ReloadMap();
	} else if ( is( "revert" ) ) {
		Revert();
	} else if ( is( "bsp" ) ) {
		LoadBspEntities();
	} else if ( is( "undo" ) ) {
		Undo();
	} else if ( is( "redo" ) ) {
		Redo();
	} else if ( is( "select" ) ) {
		if ( !Q_stricmp( Cmd_Argv( 2 ), "all" ) ) {
			SelectListed();
		} else if ( !Q_stricmp( Cmd_Argv( 2 ), "none" ) ) {
			editor.doc.Select( 0 );
		} else {
			std::vector<std::uint32_t> ids;
			for ( int arg = 2; arg < Cmd_Argc(); arg++ ) {
				const std::uint32_t found = FindEntity( Cmd_Argv( arg ) );
				if ( found ) {
					ids.push_back( found );
				} else {
					Com_Printf( "No entity %s.\n", Cmd_Argv( arg ) );
				}
			}
			editor.doc.SetSelection( ids, ids.empty() ? 0 : ids.front() );
			editor.anchorId = editor.doc.Selected();
			editor.scrollToSelection = true;
			if ( ids.size() == 1 ) {
				PrintEntity( *editor.doc.SelectedItem() );
			} else if ( ids.size() > 1 ) {
				Com_Printf( "%d selected.\n", static_cast<int>( editor.doc.SelectionCount() ) );
			}
		}
	} else if ( is( "next" ) || is( "prev" ) ) {
		UpdateFilter();
		SelectNeighbour( is( "next" ) ? 1 : -1 );
	} else if ( is( "set" ) && Cmd_Argc() >= 4 && id ) {
		if ( !editor.doc.SetValueOnSelection( Cmd_Argv( 2 ), Cmd_ArgsFrom( 3 ) ) ) {
			Com_Printf( "That key or value cannot be written.\n" );
		}
	} else if ( is( "unset" ) && Cmd_Argc() >= 3 && id ) {
		editor.doc.RemoveKeyFromSelection( Cmd_Argv( 2 ) );
	} else if ( is( "new" ) && Cmd_Argc() >= 3 ) {
		Create( Cmd_Argv( 2 ) );
	} else if ( is( "duplicate" ) || is( "copy" ) ) {
		Duplicate();
	} else if ( is( "delete" ) ) {
		Delete();
	} else if ( is( "drop" ) ) {
		RebuildMarkers();
		DropToFloor();
	} else if ( is( "look" ) ) {
		RebuildMarkers();
		FocusOn( id );
	} else if ( ( is( "move" ) || is( "nudge" ) ) && Cmd_Argc() >= 5 && id ) {
		RebuildMarkers();
		const Marker *marker = MarkerFor( id );
		if ( marker && marker->placed ) {
			vec3_t delta;
			for ( int axis = 0; axis < 3; axis++ ) {
				const float value = static_cast<float>( std::atof( Cmd_Argv( 2 + axis ) ) );
				delta[axis] = is( "move" ) ? value - marker->point[axis] : value;
			}
			MoveSelectionBy( delta, "move" );
		} else {
			Com_Printf( "The selected entity stands nowhere.\n" );
		}
	} else if ( is( "turn" ) && Cmd_Argc() >= 3 ) {
		Rotate( static_cast<float>( std::atof( Cmd_Argv( 2 ) ) ) );
	} else if ( is( "find" ) ) {
		editor.filter = Cmd_ArgsFrom( 2 );
		UpdateFilter();
		Com_Printf( "%d entities match.\n", static_cast<int>( editor.filtered.size() ) );
	} else if ( is( "print" ) ) {
		for ( const std::uint32_t member : editor.doc.Selection() ) {
			PrintEntity( *editor.doc.Find( member ) );
		}
	} else if ( is( "list" ) ) {
		const std::string text = Cmd_ArgsFrom( 2 );
		int shown = 0;
		for ( std::size_t i = 0; i < editor.doc.Count(); i++ ) {
			const ee::Item &item = editor.doc.Items()[i];
			if ( text.empty() || ee::Document::Matches( item.entity, text ) ) {
				Com_Printf( "%s\n", RowText( item, static_cast<int>( i ) ).c_str() );
				shown++;
			}
		}
		Com_Printf( "%d of %d entities%s.\n", shown, static_cast<int>( editor.doc.Count() ),
			editor.doc.Dirty() ? ", unsaved changes" : "" );
	} else if ( is( "camera" ) && Cmd_Argc() >= 5 ) {
		for ( int axis = 0; axis < 3; axis++ ) {
			editor.cameraOrigin[axis] = static_cast<float>( std::atof( Cmd_Argv( 2 + axis ) ) );
		}
		if ( Cmd_Argc() >= 7 ) {
			editor.cameraAngles[PITCH] = static_cast<float>( std::atof( Cmd_Argv( 5 ) ) );
			editor.cameraAngles[YAW] = static_cast<float>( std::atof( Cmd_Argv( 6 ) ) );
			editor.cameraAngles[ROLL] = 0.0f;
		}
		editor.cameraSet = true;
	} else {
		Com_Printf( "Unknown or incomplete: entityeditor %s. Try entityeditor help.\n", Cmd_ArgsFrom( 1 ) );
	}
}

} // namespace

/* ---- the client's calls ----------------------------------------------------------- */

void CL_EntityEditorInit( void )
{
	cl_entityEditorGrid = Cvar_Get( "cl_entityEditorGrid", "8", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorGrid, "0", "1024", CV_FLOAT );
	Cvar_SetDescription( cl_entityEditorGrid, "Entity editor: the grid moved entities snap to, in units." );
	cl_entityEditorSnap = Cvar_Get( "cl_entityEditorSnap", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorSnap, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_entityEditorSnap, "Entity editor: snap moved entities to cl_entityEditorGrid." );
	cl_entityEditorLabels = Cvar_Get( "cl_entityEditorLabels", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorLabels, "0", "2", CV_INTEGER );
	Cvar_SetDescription( cl_entityEditorLabels, "Entity editor: classnames over entities, 0 none, 1 near ones, 2 all." );
	cl_entityEditorLabelDistance = Cvar_Get( "cl_entityEditorLabelDistance", "1536", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorLabelDistance, "64", "65536", CV_FLOAT );
	Cvar_SetDescription( cl_entityEditorLabelDistance, "Entity editor: how near an entity is labelled, in units." );
	cl_entityEditorLinks = Cvar_Get( "cl_entityEditorLinks", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorLinks, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_entityEditorLinks, "Entity editor: draw every target link, not only the selected entity's." );
	cl_entityEditorPause = Cvar_Get( "cl_entityEditorPause", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorPause, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_entityEditorPause, "Entity editor: pause a single-player game while it is open." );
	cl_entityEditorSpeed = Cvar_Get( "cl_entityEditorSpeed", "400", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorSpeed, "16", "8192", CV_FLOAT );
	Cvar_SetDescription( cl_entityEditorSpeed, "Entity editor: the camera's flying speed, in units a second." );
	cl_entityEditorScale = Cvar_Get( "cl_entityEditorScale", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorScale, "0", "4", CV_FLOAT );
	Cvar_SetDescription( cl_entityEditorScale, "Entity editor: the size of its panels and text; 0 follows the screen." );
	cl_entityEditorFov = Cvar_Get( "cl_entityEditorFov", "90", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_entityEditorFov, "10", "140", CV_FLOAT );
	Cvar_SetDescription( cl_entityEditorFov, "Entity editor: the camera's field of view across a 4:3 screen, its "
		"height kept on wider and narrower ones." );
	Cmd_AddCommand( "entityeditor", Command_f );

	if ( !editor.imgui ) {
		IMGUI_CHECKVERSION();
		editor.imgui = ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		io.IniFilename = nullptr;		/* nothing written beside the executable */
		io.LogFilename = nullptr;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
		io.BackendPlatformName = "fntech3";
		io.BackendRendererName = "fntech3 2D";
		io.Fonts->AddFontDefaultVector();
		ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
		platform.Platform_GetClipboardTextFn = []( ImGuiContext * ) -> const char * {
			editor.clipboard = ClipboardText();
			return editor.clipboard.c_str();
		};
		platform.Platform_SetClipboardTextFn = []( ImGuiContext *, const char *text ) {
			Sys_SetClipboardData( text ? text : "" );
		};
		io.AddMousePosEvent( -FLT_MAX, -FLT_MAX );
		editor.uiScale = 0.0f;
	}
}

void CL_EntityEditorShutdown( void )
{
	Close();
	Cmd_RemoveCommand( "entityeditor" );
	editor.doc.Load( {} );
	cgameEntityText.clear();
	cgameEntityParse = nullptr;
	cgameEntityOverride = false;
	if ( editor.imgui ) {
		ImGui::DestroyContext( editor.imgui );
		editor.imgui = nullptr;
		editor.overlay = nullptr;
	}
}

void CL_EntityEditorRendererStarted( void )
{
	/* every picture belonged to the renderer before: ImGui uploads its
	 * pixels again (a texture left Destroyed with pixels asks to be made) */
	editor.textureUploads = 0;
	editor.textureLimitNoted = false;
	if ( !editor.imgui ) {
		return;
	}
	for ( ImTextureData *texture : ImGui::GetPlatformIO().Textures ) {
		texture->SetTexID( ImTextureID_Invalid );
		texture->SetStatus( ImTextureStatus_Destroyed );
	}
}

qboolean CL_EntityEditorActive( void )
{
	return editor.active ? qtrue : qfalse;
}

/* Text the server printed back, rcon replies among it: after a reload asked
 * over rcon, whether the server loaded the saved override or set it aside.
 * A reply can arrive in pieces, so the last of it is kept and searched. */
void CL_EntityEditorServerPrint( const char *text )
{
	if ( !editor.remotePending || !text || !*text ) {
		return;
	}
	editor.remoteHeard += text;
	if ( editor.remoteHeard.size() > 4096 ) {
		editor.remoteHeard.erase( 0, editor.remoteHeard.size() - 4096 );
	}
	const std::string loaded = "Loaded entity override " + editor.remotePath;
	const std::string ignored = "Ignored " + editor.remotePath;
	const std::size_t found = editor.remoteHeard.find( ignored );
	if ( editor.remoteHeard.find( loaded ) != std::string::npos ) {
		editor.remotePending = false;
		SetStatus( StatusKind::Good, Format( "the server loaded %s", editor.remotePath.c_str() ) );
		Com_Printf( "Entity editor: the server loaded %s\n", editor.remotePath.c_str() );
	} else if ( found != std::string::npos ) {
		const std::size_t end = editor.remoteHeard.find( '\n', found );
		const std::string line = editor.remoteHeard.substr( found, end == std::string::npos ? std::string::npos : end - found );
		editor.remotePending = false;
		SetStatus( StatusKind::Error, Format( "the server set the file aside: %s", line.c_str() ) );
		Com_Printf( "Entity editor: the server set %s aside\n", editor.remotePath.c_str() );
	}
}

int CL_EntityEditorPointerMask( void )
{
	return editor.active && !( editor.looking || editor.flying ) && ( Key_GetCatcher() & KEYCATCH_ENTITYEDITOR )
		? KEYCATCH_ENTITYEDITOR : 0;
}

qboolean CL_EntityEditorKeyEvent( int key, qboolean down, unsigned time )
{
	(void)time;
	/* a release always lets go, wherever it is routed: a key or button held
	 * when the console opened must not fly the camera or drag on after it,
	 * and ImGui hears it too */
	if ( !down && key >= 0 && key < MAX_KEYS ) {
		editor.keyDown[static_cast<std::size_t>( key )] = false;
		if ( key == K_MOUSE2 ) {
			StartLooking( false );
		}
		KeyToImGui( key, false );
	}
	const int catcher = Key_GetCatcher();
	if ( !editor.active || !( catcher & KEYCATCH_ENTITYEDITOR ) || ( catcher & KEYCATCH_CONSOLE ) ) {
		return qfalse;
	}
	/* the console key and Shift+Escape stay the console's, Alt+Enter the
	 * window's */
	if ( key == K_CONSOLE || ( key == K_ESCAPE && Key_IsDown( K_SHIFT ) )
		|| ( key == K_ENTER && Key_IsDown( K_ALT ) ) ) {
		return qfalse;
	}
	if ( key < 0 || key >= MAX_KEYS || !down ) {
		return qtrue;
	}
	editor.keyDown[static_cast<std::size_t>( key )] = true;
	ImGuiIO &io = ImGui::GetIO();
	switch ( key ) {
	case K_MOUSE1:
	case K_MOUSE3:
		KeyToImGui( key, true );
		return qtrue;
	case K_MOUSE2:
		/* the right button looks around wherever it is pressed over the view */
		if ( editor.looking || editor.flying || !io.WantCaptureMouse ) {
			StartLooking( true );
		} else {
			KeyToImGui( key, true );
		}
		return qtrue;
	case K_MWHEELUP:
	case K_MWHEELDOWN:
		if ( editor.looking || editor.flying ) {
			editor.speedScale = std::clamp( editor.speedScale * ( key == K_MWHEELUP ? 1.25f : 0.8f ), 0.05f, 20.0f );
		} else {
			io.AddMouseWheelEvent( 0.0f, key == K_MWHEELUP ? 1.0f : -1.0f );
		}
		return qtrue;
	default:
		break;
	}
	if ( editor.looking || editor.flying ) {
		/* flying keys steer; Escape and Tab still act */
		if ( key == K_ESCAPE || key == K_TAB ) {
			QueueShortcut( key );
		}
		return qtrue;
	}
	KeyToImGui( key, true );
	QueueShortcut( key );
	return qtrue;
}

qboolean CL_EntityEditorCharEvent( int ch )
{
	const int catcher = Key_GetCatcher();
	if ( !editor.active || !( catcher & KEYCATCH_ENTITYEDITOR ) || ( catcher & KEYCATCH_CONSOLE ) ) {
		return qfalse;
	}
	if ( !( editor.looking || editor.flying ) && ch >= 32 && ch < 127 ) {
		ImGui::GetIO().AddInputCharacter( static_cast<unsigned int>( ch ) );
	}
	return qtrue;
}

qboolean CL_EntityEditorMouseEvent( int dx, int dy )
{
	const int catcher = Key_GetCatcher();
	if ( !editor.active || !( catcher & KEYCATCH_ENTITYEDITOR ) || ( catcher & KEYCATCH_CONSOLE ) ) {
		return qfalse;
	}
	if ( editor.looking || editor.flying ) {
		float sensitivity = Cvar_VariableValue( "sensitivity" );
		if ( !( sensitivity > 0.0f ) ) {
			sensitivity = 5.0f;
		}
		float yawScale = Cvar_VariableValue( "m_yaw" ), pitchScale = Cvar_VariableValue( "m_pitch" );
		if ( yawScale == 0.0f ) yawScale = 0.022f;
		if ( pitchScale == 0.0f ) pitchScale = 0.022f;
		editor.cameraAngles[YAW] -= static_cast<float>( dx ) * sensitivity * yawScale;
		editor.cameraAngles[PITCH] = std::clamp( editor.cameraAngles[PITCH] + static_cast<float>( dy ) * sensitivity * pitchScale,
			-89.0f, 89.0f );
		editor.cameraAngles[YAW] = AngleMod( editor.cameraAngles[YAW] );
		return qtrue;
	}
	/* relative motion where no absolute pointer is offered (a fullscreen
	 * window without one) moves the editor's own pointer */
	editor.cursorX = std::clamp( editor.cursorX + static_cast<float>( dx ), 0.0f, static_cast<float>( cls.glconfig.vidWidth - 1 ) );
	editor.cursorY = std::clamp( editor.cursorY + static_cast<float>( dy ), 0.0f, static_cast<float>( cls.glconfig.vidHeight - 1 ) );
	PointerToImGui();
	return qtrue;
}

qboolean CL_EntityEditorMouseAbsoluteEvent( int x, int y )
{
	const int catcher = Key_GetCatcher();
	if ( !editor.active || !( catcher & KEYCATCH_ENTITYEDITOR ) || ( catcher & KEYCATCH_CONSOLE ) ) {
		return qfalse;
	}
	if ( !( editor.looking || editor.flying ) ) {
		editor.cursorX = static_cast<float>( x );
		editor.cursorY = static_cast<float>( y );
		PointerToImGui();
	}
	return qtrue;
}

void CL_EntityEditorAdjustView( refdef_t *refdef )
{
	if ( !refdef || ( refdef->rdflags & RDF_NOWORLDMODEL ) ) {
		return;
	}
	/* Enemy Territory's, Return to Castle Wolfenstein's and Jedi Academy's
	 * sky portals are views of their own, drawn first: they turn with the
	 * camera and keep their origin */
	const bool portal = refdef->rtcw.skyboxPortal || refdef->wolfet.skyboxPortal ||
		refdef->jka.skyboxPortal;
	if ( !portal ) {
		ViewFromRefdef( *refdef, editor.playerView );
	}
	if ( !editor.active || cls.state != CA_ACTIVE ) {
		if ( !portal ) {
			ViewFromRefdef( *refdef, editor.view );
		}
		return;
	}
	/* the whole screen, at the editor's own field of view, drawn as given: a
	 * game that draws its world into part of it -- Enemy Territory's limbo
	 * camera, a smaller viewsize -- still gives the editor all of it; a sky
	 * portal's view is fitted alike, to stay behind it */
	const int screenWidth = cls.glconfig.vidWidth, screenHeight = cls.glconfig.vidHeight;
	if ( screenWidth > 0 && screenHeight > 0 ) {
		refdef->x = 0;
		refdef->y = 0;
		refdef->width = screenWidth;
		refdef->height = screenHeight;
		EditorFov( screenWidth, screenHeight, refdef->fov_x, refdef->fov_y );
		refdef->rdflags |= RDF_NOFOVCORRECTION;
	}
	if ( !editor.cameraSet ) {
		/* the player's view this once, until the camera takes it up */
		if ( !portal ) {
			ViewFromRefdef( *refdef, editor.view );
		}
		return;
	}
	UpdateCamera();
	vec3_t axis[3];
	CameraAxis( axis );
	for ( int i = 0; i < 3; i++ ) {
		VectorCopy( axis[i], refdef->viewaxis[i] );
	}
	if ( portal ) {
		return;
	}
	VectorCopy( editor.cameraOrigin, refdef->vieworg );
	refdef->rdflags |= RDF_NOFIRSTPERSON;
	/* every area: the player's closed doors do not hide the camera's view */
	std::memset( refdef->areamask, 0, sizeof( refdef->areamask ) );
	const bool water = ( CM_PointContents( editor.cameraOrigin, 0 ) & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) != 0;
	refdef->rtcw.underwater = water ? qtrue : qfalse;
	refdef->wolfet.underwater = water ? qtrue : qfalse;
	if ( refdef->alice.skyPortal ) {
		for ( int i = 0; i < 3; i++ ) {
			VectorCopy( axis[i], refdef->alice.skyAxis[i] );
		}
	}
	ViewFromRefdef( *refdef, editor.view );
}

void CL_EntityEditorDraw( void )
{
	if ( !editor.active || !editor.imgui ) {
		return;
	}
	/* a level loading, a cinematic or a disconnect: the editor waits */
	if ( cls.state != CA_ACTIVE ) {
		if ( Key_GetCatcher() & KEYCATCH_ENTITYEDITOR ) {
			editor.looking = editor.flying = false;
			Key_SetEntityEditorCatcher( qfalse );
		}
		return;
	}
	if ( !( Key_GetCatcher() & KEYCATCH_ENTITYEDITOR ) ) {
		Key_SetEntityEditorCatcher( qtrue );
	}
	/* a server asked over rcon that never named the file: it read its own
	 * home's, or the BSP's */
	if ( editor.remotePending && cls.realtime - editor.remoteAskedAt > 20000 ) {
		editor.remotePending = false;
		SetStatus( StatusKind::Warning, Format( "the server did not say it loaded %s: it reads its own home, so copy the "
			"file there", editor.remotePath.c_str() ) );
		Com_Printf( "Entity editor: the server did not report loading %s\n", editor.remotePath.c_str() );
	}
	if ( editor.repauseAt > 0 && cls.realtime >= editor.repauseAt ) {
		editor.repauseAt = 0;
		Pause();
	}
	FollowServer();
	if ( !editor.cameraSet ) {
		CameraFromPlayer();
	}
	Frame();
}

#else /* FNTECH3_ENTITY_EDITOR_UI */

/* A build without Dear ImGui has no editor's interface; overrides and the
 * client game's entity tokens work as they do with it. */
namespace {

void Command_f()
{
	Com_Printf( "This build has no entity editor: Dear ImGui was not found when it was built "
		"(meson subprojects download imgui).\n" );
}

} // namespace

void CL_EntityEditorInit( void ) { Cmd_AddCommand( "entityeditor", Command_f ); }
void CL_EntityEditorShutdown( void )
{
	Cmd_RemoveCommand( "entityeditor" );
	cgameEntityText.clear();
	cgameEntityParse = nullptr;
	cgameEntityOverride = false;
}
void CL_EntityEditorRendererStarted( void ) {}
qboolean CL_EntityEditorActive( void ) { return qfalse; }
int CL_EntityEditorPointerMask( void ) { return 0; }
qboolean CL_EntityEditorKeyEvent( int, qboolean, unsigned ) { return qfalse; }
qboolean CL_EntityEditorCharEvent( int ) { return qfalse; }
qboolean CL_EntityEditorMouseEvent( int, int ) { return qfalse; }
qboolean CL_EntityEditorMouseAbsoluteEvent( int, int ) { return qfalse; }
void CL_EntityEditorAdjustView( refdef_t * ) {}
void CL_EntityEditorDraw( void ) {}
void CL_EntityEditorServerPrint( const char * ) {}

#endif /* FNTECH3_ENTITY_EDITOR_UI */

/* ---- the cgame's entity tokens ---------------------------------------------------- */

namespace {

/* maps/<map> of the client's map, from its BSP's name. */
std::string CGameMapPath()
{
	std::string path = cl.mapname;
	if ( path.size() > 4 && !Q_stricmp( path.c_str() + path.size() - 4, ".bsp" ) ) {
		path.resize( path.size() - 4 );
	}
	return path;
}

} // namespace

void CL_EntityOverrideCGameInit( void )
{
	cgameEntityText.clear();
	cgameEntityParse = nullptr;
	cgameEntityOverride = false;
	char path[MAX_QPATH];
	if ( !com_sv_running || !com_sv_running->integer || !SV_EntityOverrideFile( path, sizeof( path ) )
		|| !SV_EntityOverridePath()[0] ) {
		return;
	}
	/* only for the map the server runs */
	std::string serverMap = path;
	serverMap.resize( serverMap.size() - 4 );
	if ( Q_stricmp( serverMap.c_str(), CGameMapPath().c_str() ) ) {
		return;
	}
	cgameEntityText = SV_EntityText();
	cgameEntityParse = cgameEntityText.c_str();
	cgameEntityOverride = true;
	Com_DPrintf( "Client game entity tokens follow %s.\n", SV_EntityOverridePath() );
}

void CL_EntityOverrideCGameShutdown( void )
{
	cgameEntityText.clear();
	cgameEntityParse = nullptr;
	cgameEntityOverride = false;
}

qboolean CL_GetEntityToken( char *buffer, int size )
{
	if ( !cgameEntityOverride ) {
		return re.GetEntityToken( buffer, size );
	}
	/* the renderer's iterator, over the override's text: Academy resets on
	 * -1 and does not rewind at the end; the others rewind there */
	if ( Com_GameIsJKAMP() ) {
		if ( size == -1 ) {
			cgameEntityParse = cgameEntityText.c_str();
			return qtrue;
		}
		if ( !buffer || size <= 0 ) {
			Com_Error( ERR_DROP, "Invalid Academy entity token buffer" );
		}
		const char *token = COM_Parse( &cgameEntityParse );
		Q_strncpyz( buffer, token, size );
		return cgameEntityParse && token[0] ? qtrue : qfalse;
	}
	if ( !buffer || size <= 0 ) {
		return qfalse;
	}
	const char *token = COM_Parse( &cgameEntityParse );
	Q_strncpyz( buffer, token, size );
	if ( !cgameEntityParse && !token[0] ) {
		cgameEntityParse = cgameEntityText.c_str();
		return qfalse;
	}
	return qtrue;
}
