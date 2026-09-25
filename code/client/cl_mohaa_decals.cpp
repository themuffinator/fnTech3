/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor: Allied Assault's decal editor

A level's permanent decals (code/qcommon/dcl_mohaa.h) are built into its
world by the renderers as it loads (code/renderercommon/tr_mohaa_world.c).
With `dcl_editmode` set before a level loads they are not: its decal file is
read as definitions instead, the client game cuts each into fragments (its
exports 16 to 18), and the fragments are drawn with the scene every frame,
so that the retail decal menu (Pak0.pk3 ui/decals.URC, bound to F6 in
development.cfg) can change them through the `dcl_*` cvars and the `dclc_*`
commands, and `dclc_save` can write the file back. 2015's renderer held the
editor; OpenMoHAA's renderergl1/tr_marks_permanent.c (GPL-2.0-or-later)
reconstructs it. This is written anew after that reconstruction's behaviour
and constants, none of its text taken, except where noted below.

  dclc_make         cut a new decal where the view looks, with the dcl_*
                    settings, and select it
  dclc_reapply      cut the selected decal again where it lies, with them
  dclc_delete       remove the selected decal
  dclc_selectnext   select the next or the previous decal; with
  dclc_selectprev   dcl_autogetinfo, read its settings into the cvars
  dclc_getinfo      read the selected decal's settings into the cvars
  dclc_shiftup      move the selected decal dcl_shiftstep units along its
  dclc_shiftdown    surface and cut it again
  dclc_shiftleft
  dclc_shiftright
  dclc_randomroll   a random dcl_rotation
  dclc_save         write the level's decal file

While a decal is selected, a change to its shader, size, rotation, colour or
lighting cvar cuts it again at once; a new shader resets its radius to 16
and its scales to its picture's size over 32. `dcl_pathmode` lays decals
along the player's path instead, a segment at a time, with the tread
exports. The selected decal's fragments are outlined over the view, with the
direction it was laid from and its width and height, while
`dcl_showcurrent` is set. The decals are drawn only while
`r_drawstaticdecals` is, as the built ones are.

A decal's coplanar fragments that share an edge are joined as 2015's
editor joins them, with its tests set right (MergeCoplanar). Where fnTech3
differs: a fragment on a brush model is not kept, as the renderers draw none
(the client game is handed none, see MarkFragmentsForInlineModel in
cl_mohaa_cgame.cpp); the decals keep the order the file lists them in,
where 2015's listed its newest first; the outline is drawn over everything,
as thin polygons facing the view rather than debug lines; and the file is
written as version 1, 1.00's own, with the brush model and leaf lists
1.00's renderer reads, each fragment listed in the open leafs a unit in
front of its points and their centre.
===========================================================================
*/

extern "C" {
#include "client.h"
}

#include "client_cpp.h"
#include "cl_mohaa.h"
#include "../qcommon/dcl_mohaa.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace mohaa = fntech3::mohaa;

/* The client game keeps at most eight points a fragment. */
constexpr int kFragmentPoints = 8;
/* A decal laid along a path is cut downwards, the way 2015's editor names
 * `vec_upwards`. */
const vec3_t kUpwards = { 0.0f, 0.0f, 1.0f };

cvar_t *dcl_editmode;
cvar_t *dcl_showcurrent;
cvar_t *dcl_autogetinfo;
cvar_t *dcl_shiftstep;
cvar_t *dcl_shader;
cvar_t *dcl_radius;
cvar_t *dcl_heightscale;
cvar_t *dcl_widthscale;
cvar_t *dcl_rotation;
cvar_t *dcl_r;
cvar_t *dcl_g;
cvar_t *dcl_b;
cvar_t *dcl_alpha;
cvar_t *dcl_dolighting;
cvar_t *dcl_doworld;
cvar_t *dcl_doterrain;
cvar_t *dcl_dobmodels;
cvar_t *dcl_dostring;
cvar_t *dcl_pathmode;
cvar_t *dcl_maxsegment;
cvar_t *dcl_minsegment;
cvar_t *dcl_maxoffset;
cvar_t *dcl_texturescale;

struct Fragment {
	int index = 0;					/* 0 the world, a terrain patch above 0 */
	std::vector<mohaa::PolyVert> points;
	std::vector<int> leafs;			/* the open leafs it is listed in */
};

struct Decal {
	std::string shader = "<default>";
	qhandle_t handle = 0;
	vec3_t position = { 0.0f, 0.0f, 0.0f };
	vec3_t projection = { 0.0f, 0.0f, -1.0f };
	float radius = 8.0f;
	float heightScale = 1.0f;
	float widthScale = 1.0f;
	float rotation = 0.0f;			/* -1 for one laid along a path */
	vec4_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool lighting = true;
	std::vector<Fragment> fragments;
};

struct Editor {
	bool active = false;
	bool pending = false;			/* read from the file, not cut yet */
	bool commands = false;
	std::string file;
	std::vector<std::unique_ptr<Decal>> decals;
	Decal *current = nullptr;
	bool autoApply = false;
	bool pathLaying = false;
	mohaa::TreadMark tread{};
	Decal *pathStart = nullptr;
	Decal *pathEnd = nullptr;
	vec3_t viewOrigin = { 0.0f, 0.0f, 0.0f };
	vec3_t viewForward = { 1.0f, 0.0f, 0.0f };
	bool viewSeen = false;
};
Editor editor;

float Clamp01( float value ) {
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

int IndexOf( const Decal *decal ) {
	for ( std::size_t i = 0; i < editor.decals.size(); i++ ) {
		if ( editor.decals[i].get() == decal ) {
			return static_cast<int>( i );
		}
	}
	return -1;
}

Decal *NewDecal() {
	editor.decals.push_back( std::make_unique<Decal>() );
	/* 2015's fresh definition leaves its colour unset (R_InitMarkDef) */
	return editor.decals.back().get();
}

/* The side a fragment's points wind clockwise around; false for points on
 * one line. */
bool FragmentNormal( const Fragment &fragment, vec3_t normal ) {
	const float *first = fragment.points[0].xyz;
	for ( std::size_t i = 1; i + 1 < fragment.points.size(); i++ ) {
		vec3_t e1, e2;
		VectorSubtract( fragment.points[i + 1].xyz, first, e1 );
		VectorSubtract( fragment.points[i].xyz, first, e2 );
		CrossProduct( e1, e2, normal );
		if ( VectorNormalize( normal ) > 0.0001f ) {
			return true;
		}
	}
	VectorClear( normal );
	return false;
}

/* Points this close are one: neighbouring fragments are cut along the same
 * edge of the world's surfaces. */
bool SamePoint( const float *a, const float *b ) {
	constexpr float kEpsilon = 0.01f;
	return std::fabs( a[0] - b[0] ) <= kEpsilon && std::fabs( a[1] - b[1] ) <= kEpsilon
		&& std::fabs( a[2] - b[2] ) <= kEpsilon;
}

/* A point on a straight run between its neighbours, or on top of one, is
 * dropped. */
void DropCollinear( std::vector<mohaa::PolyVert> &points ) {
	std::size_t k = 0;
	while ( points.size() > 3 && k < points.size() ) {
		const std::size_t n = points.size();
		vec3_t in, out;
		VectorSubtract( points[k].xyz, points[( k + n - 1 ) % n].xyz, in );
		VectorSubtract( points[( k + 1 ) % n].xyz, points[k].xyz, out );
		const float inLength = VectorNormalize( in ), outLength = VectorNormalize( out );
		if ( inLength < 0.01f || outLength < 0.01f || DotProduct( in, out ) > 0.999f ) {
			points.erase( points.begin() + static_cast<std::ptrdiff_t>( k ) );
		} else {
			k++;
		}
	}
}

/* Whether a polygon wound clockwise about `normal` is convex: no point lies
 * outside any of its edges. */
bool Convex( const std::vector<mohaa::PolyVert> &points, const vec3_t normal ) {
	const std::size_t n = points.size();
	for ( std::size_t k = 0; k < n; k++ ) {
		vec3_t edge;
		VectorSubtract( points[( k + 1 ) % n].xyz, points[k].xyz, edge );
		for ( std::size_t l = 0; l < n; l++ ) {
			vec3_t toPoint, cross;
			VectorSubtract( points[l].xyz, points[k].xyz, toPoint );
			CrossProduct( edge, toPoint, cross );
			if ( DotProduct( cross, normal ) > 0.01f ) {
				return false;
			}
		}
	}
	return true;
}

/*
==================
MergeCoplanar

2015's editor joins a decal's fragments that lie on one plane and share an
edge into one polygon, when the whole is convex and has at most eight
points (R_ApplyLevelDecal [O]); a terrain patch's fragments stay apart. As
OpenMoHAA writes it, its merge skips a pair on the same surface rather than
on different ones and tests convexity against the wrong vectors; here the
pair must lie on the same surface, and every point must lie inside every
edge. Each fragment is merged with the later ones in turn, as there.
==================
*/
void MergeCoplanar( std::vector<Fragment> &fragments ) {
	for ( std::size_t i = 0; i < fragments.size(); i++ ) {
		Fragment &a = fragments[i];
		vec3_t normal;
		if ( a.index != 0 || a.points.size() < 3 || !FragmentNormal( a, normal ) ) {
			continue;
		}
		for ( std::size_t j = i + 1; j < fragments.size(); j++ ) {
			Fragment &b = fragments[j];
			vec3_t other;
			if ( b.index != a.index || b.points.size() < 3 || !FragmentNormal( b, other )
				|| DotProduct( normal, other ) < 0.9999f
				|| std::fabs( DotProduct( normal, b.points[0].xyz ) - DotProduct( normal, a.points[0].xyz ) ) > 0.01f ) {
				continue;
			}
			/* an edge a walks one way and b the other */
			const std::size_t n = a.points.size(), m = b.points.size();
			std::size_t edgeA = n, edgeB = m;
			for ( std::size_t k = 0; k < n && edgeA == n; k++ ) {
				for ( std::size_t l = 0; l < m; l++ ) {
					if ( SamePoint( a.points[k].xyz, b.points[( l + 1 ) % m].xyz )
						&& SamePoint( a.points[( k + 1 ) % n].xyz, b.points[l].xyz ) ) {
						edgeA = k;
						edgeB = l;
						break;
					}
				}
			}
			if ( edgeA == n ) {
				continue;
			}
			/* a from the edge's far end round to its near one, then b past
			 * the edge round to it again */
			std::vector<mohaa::PolyVert> merged;
			for ( std::size_t k = 1; k < n; k++ ) {
				merged.push_back( a.points[( edgeA + k ) % n] );
			}
			for ( std::size_t k = 1; k < m; k++ ) {
				merged.push_back( b.points[( edgeB + k ) % m] );
			}
			DropCollinear( merged );
			if ( merged.size() > static_cast<std::size_t>( kFragmentPoints ) || !Convex( merged, normal ) ) {
				continue;
			}
			a.points = std::move( merged );
			b.points.clear();
		}
	}
	fragments.erase( std::remove_if( fragments.begin(), fragments.end(),
		[]( const Fragment &fragment ) { return fragment.points.size() < 3; } ), fragments.end() );
}

/* The open leafs a unit in front of a fragment's points and their centre,
 * where 2015's editor lists a fragment it has cut (R_ApplyLevelDecal). */
std::vector<int> FragmentLeafs( const Fragment &fragment ) {
	std::vector<int> leafs;
	vec3_t normal, centre = { 0.0f, 0.0f, 0.0f };
	FragmentNormal( fragment, normal );
	const auto add = [&leafs]( const vec3_t point ) {
		const int leaf = CM_PointLeafnum( point );
		if ( leaf >= 0 && leaf < CM_NumLeafs() && CM_LeafCluster( leaf ) >= 0
			&& std::find( leafs.begin(), leafs.end(), leaf ) == leafs.end() ) {
			leafs.push_back( leaf );
		}
	};
	for ( const mohaa::PolyVert &point : fragment.points ) {
		vec3_t front;
		VectorAdd( point.xyz, normal, front );
		add( front );
		VectorAdd( centre, front, centre );
	}
	VectorScale( centre, 1.0f / static_cast<float>( fragment.points.size() ), centre );
	add( centre );
	return leafs;
}

void SetShader( Decal &decal, const char *name ) {
	decal.shader = name && name[0] ? name : "<default>";
	decal.handle = re.RegisterShader( decal.shader.c_str() );
	if ( !decal.handle ) {
		Com_Printf( "Bad decal shader %s\n", decal.shader.c_str() );
	}
}

/*
==================
Apply

R_ApplyLevelDecal: have the client game cut a decal from a position along a
direction, with the target's own settings, the dcl_* cvars' or, along a
path, the tread's; the target, or a new decal when there is none, then holds
what it cut. Null, with the target untouched, when nothing was cut.
==================
*/
Decal *Apply( const vec3_t position, const vec3_t direction, Decal *target, bool targetSettings, int pathIndex ) {
	std::string shader;
	float radius, sScale, tScale, roll;
	vec4_t color;
	bool lighting;

	if ( pathIndex ) {
		shader = dcl_shader->string;
		radius = editor.tread.fWidth;
		sScale = tScale = 1.0f;
		Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
		lighting = target ? target->lighting : dcl_dolighting->integer != 0;
		roll = -1.0f;
	} else if ( target && targetSettings ) {
		shader = target->shader;
		radius = target->radius;
		sScale = target->widthScale;
		tScale = target->heightScale;
		Vector4Copy( target->color, color );
		lighting = target->lighting;
		roll = target->rotation;
	} else {
		shader = dcl_shader->string;
		radius = dcl_radius->value;
		sScale = dcl_widthscale->value;
		tScale = dcl_heightscale->value;
		Vector4Set( color, Clamp01( dcl_r->value ), Clamp01( dcl_g->value ), Clamp01( dcl_b->value ),
			Clamp01( dcl_alpha->value ) );
		lighting = dcl_dolighting->integer != 0;
		roll = AngleMod( dcl_rotation->value );
	}
	radius = radius < 0.0f ? 0.001f : radius;
	sScale = sScale < 0.0f ? 0.001f : sScale;
	tScale = tScale < 0.0f ? 0.001f : tScale;

	/* the client game leaves the hit point and its surface's normal here */
	vec3_t origin, projection;
	VectorCopy( position, origin );
	VectorNormalize2( direction, projection );

	static mohaa::MarkFragment fragments[FNTECH3_MOHAA_MARK_FRAGMENTS];
	static mohaa::PolyVert points[FNTECH3_MOHAA_MARK_POINTS];
	std::memset( fragments, 0, sizeof( fragments ) );
	std::memset( points, 0, sizeof( points ) );
	int count = pathIndex
		? CL_MOHAACGamePermanentTreadMarkDecal( &editor.tread, pathIndex == 1, lighting, fragments, points )
		: CL_MOHAACGamePermanentMark( origin, projection, AngleMod( roll + 90.0f ), sScale * radius,
			tScale * radius, color, lighting, fragments, points );
	if ( count <= 0 ) {
		return nullptr;
	}
	count = std::min( count, static_cast<int>( FNTECH3_MOHAA_MARK_FRAGMENTS ) );

	Decal *decal = target ? target : NewDecal();
	decal->fragments.clear();
	VectorCopy( origin, decal->position );
	VectorCopy( projection, decal->projection );
	SetShader( *decal, shader.c_str() );
	decal->radius = radius;
	decal->widthScale = sScale;
	decal->heightScale = tScale;
	Vector4Copy( color, decal->color );
	decal->lighting = lighting;
	decal->rotation = roll;

	if ( pathIndex ) {
		/* a segment is kept above the middle of its four corners */
		const float *corners[4];
		if ( pathIndex == 1 ) {
			corners[0] = editor.tread.vStartVerts[1];
			corners[1] = editor.tread.vStartVerts[0];
			corners[2] = editor.tread.vMidVerts[0];
			corners[3] = editor.tread.vMidVerts[1];
		} else {
			corners[0] = editor.tread.vMidVerts[1];
			corners[1] = editor.tread.vMidVerts[0];
			corners[2] = editor.tread.vEndVerts[0];
			corners[3] = editor.tread.vEndVerts[1];
		}
		VectorClear( decal->position );
		for ( const float *corner : corners ) {
			VectorAdd( decal->position, corner, decal->position );
		}
		VectorScale( decal->position, 0.25f, decal->position );
		decal->position[2] += 16.0f;
	}

	std::vector<Fragment> kept;
	for ( int i = 0; i < count; i++ ) {
		const mohaa::MarkFragment &cut = fragments[i];
		const int numPoints = std::min( cut.numPoints, kFragmentPoints );
		if ( numPoints < 3 || cut.firstPoint < 0 || cut.firstPoint + numPoints > FNTECH3_MOHAA_MARK_POINTS ) {
			continue;
		}
		/* the world's, and a terrain patch's with dcl_doterrain; a brush
		 * model's is not kept */
		if ( cut.iIndex < 0 || !dcl_doworld->integer || ( cut.iIndex > 0 && !dcl_doterrain->integer ) ) {
			continue;
		}
		Fragment fragment;
		fragment.index = cut.iIndex;
		fragment.points.assign( points + cut.firstPoint, points + cut.firstPoint + numPoints );
		kept.push_back( std::move( fragment ) );
	}
	MergeCoplanar( kept );
	for ( Fragment &fragment : kept ) {
		fragment.leafs = FragmentLeafs( fragment );
		decal->fragments.push_back( std::move( fragment ) );
	}
	return decal;
}

/* ---- the commands ------------------------------------------------------- */

void GetInfo_f() {
	Decal *decal = editor.current;
	if ( !decal ) {
		return;
	}
	Cvar_Set( "dcl_shader", decal->shader.c_str() );
	Cvar_SetValue( "dcl_radius", decal->radius );
	if ( decal->rotation == -1.0f ) {
		Cvar_SetValue( "dcl_widthscale", decal->widthScale );
	} else {
		Cvar_SetValue( "dcl_heightscale", decal->heightScale );
		Cvar_SetValue( "dcl_widthscale", decal->widthScale );
		Cvar_SetValue( "dcl_rotation", decal->rotation );
		Cvar_SetValue( "dcl_r", decal->color[0] );
		Cvar_SetValue( "dcl_g", decal->color[1] );
		Cvar_SetValue( "dcl_b", decal->color[2] );
	}
	Cvar_SetValue( "dcl_alpha", decal->color[3] );
	Cvar_SetValue( "dcl_dolighting", decal->lighting ? 1.0f : 0.0f );
	editor.autoApply = true;
}

void Make_f() {
	if ( !editor.viewSeen ) {
		return;
	}
	vec3_t projection;
	VectorNegate( editor.viewForward, projection );
	Decal *decal = Apply( editor.viewOrigin, projection, nullptr, false, 0 );
	if ( decal ) {
		editor.current = decal;
		editor.autoApply = true;
	}
}

void Reapply_f() {
	Decal *decal = editor.current;
	if ( !decal || decal->rotation == -1.0f ) {
		return;
	}
	vec3_t position, projection;
	VectorCopy( decal->position, position );
	VectorCopy( decal->projection, projection );
	if ( Apply( position, projection, decal, false, 0 ) ) {
		editor.autoApply = true;
	}
}

void Delete_f() {
	Decal *decal = editor.current;
	const int index = IndexOf( decal );
	if ( index < 0 ) {
		return;
	}
	if ( editor.pathStart == decal ) {
		editor.pathStart = nullptr;
	}
	if ( editor.pathEnd == decal ) {
		editor.pathEnd = nullptr;
	}
	editor.decals.erase( editor.decals.begin() + index );
	/* the one listed before it, else the one after, else none */
	editor.current = index > 0 ? editor.decals[index - 1].get()
		: !editor.decals.empty() ? editor.decals[0].get() : nullptr;
	if ( dcl_autogetinfo->integer ) {
		editor.autoApply = true;
		GetInfo_f();
	} else {
		editor.autoApply = false;
	}
}

void Select( int step ) {
	editor.autoApply = dcl_autogetinfo->integer != 0;
	if ( !editor.decals.empty() ) {
		const int count = static_cast<int>( editor.decals.size() );
		const int index = IndexOf( editor.current );
		editor.current = editor.decals[index < 0 ? ( step > 0 ? 0 : count - 1 ) : ( index + step + count ) % count].get();
	}
	if ( dcl_autogetinfo->integer ) {
		GetInfo_f();
	}
}

void SelectNext_f() {
	Select( 1 );
}

void SelectPrev_f() {
	Select( -1 );
}

/* Along the selected decal's surface, by the direction it was laid from. */
void Shift( float up, float right ) {
	Decal *decal = editor.current;
	if ( !decal || decal->rotation == -1.0f ) {
		return;
	}
	vec3_t angles, rightAxis, upAxis;
	vectoangles( decal->projection, angles );
	AngleVectors( angles, nullptr, rightAxis, upAxis );
	VectorMA( decal->position, up * dcl_shiftstep->value, upAxis, decal->position );
	VectorMA( decal->position, right * dcl_shiftstep->value, rightAxis, decal->position );
	Reapply_f();
}

void ShiftUp_f() {
	Shift( 1.0f, 0.0f );
}

void ShiftDown_f() {
	Shift( -1.0f, 0.0f );
}

void ShiftLeft_f() {
	Shift( 0.0f, -1.0f );
}

void ShiftRight_f() {
	Shift( 0.0f, 1.0f );
}

void RandomRoll_f() {
	Cvar_SetValue( "dcl_rotation", random() * 360.0f );
}

/* ---- the file ------------------------------------------------------------ */

struct Writer {
	std::vector<byte> data;
	void Int( int value ) {
		const int little = LittleLong( value );
		const byte *p = reinterpret_cast<const byte *>( &little );
		data.insert( data.end(), p, p + 4 );
	}
	void Float( float value ) {
		const float little = LittleFloat( value );
		const byte *p = reinterpret_cast<const byte *>( &little );
		data.insert( data.end(), p, p + 4 );
	}
	void Name( const std::string &name ) {
		char field[MOHAA_DCL_SHADER_BYTES] = {};
		Q_strncpyz( field, name.c_str(), sizeof( field ) );
		data.insert( data.end(), field, field + sizeof( field ) );
	}
};

/*
==================
Save_f

DCLC_Save: the header (version 1, checksum 0, as 1.00's files are), each
decal's definition, each fragment with its points, then for each brush
model and each leaf the fragments it lists.
==================
*/
void Save_f() {
	if ( editor.file.empty() ) {
		return;
	}
	int numFragments = 0;
	for ( const auto &decal : editor.decals ) {
		numFragments += static_cast<int>( decal->fragments.size() );
	}

	static const byte ident[4] = { 'D', 'C', 'L', ' ' };
	Writer out;
	out.data.insert( out.data.end(), ident, ident + 4 );
	out.Int( MOHAA_DCL_VERSION_1 );
	out.Int( 0 );
	out.Int( static_cast<int>( editor.decals.size() ) );
	out.Int( numFragments );
	for ( const auto &decal : editor.decals ) {
		out.Name( decal->shader );
		for ( int i = 0; i < 3; i++ ) {
			out.Float( decal->position[i] );
		}
		for ( int i = 0; i < 3; i++ ) {
			out.Float( decal->projection[i] );
		}
		out.Float( decal->radius );
		out.Float( decal->heightScale );
		out.Float( decal->widthScale );
		out.Float( decal->rotation );
		for ( int i = 0; i < 4; i++ ) {
			out.Float( decal->color[i] );
		}
		out.Int( decal->lighting ? 1 : 0 );
	}

	const int numLeafs = CM_NumLeafs();
	std::vector<std::vector<int>> leafFragments( static_cast<std::size_t>( std::max( numLeafs, 0 ) ) );
	int number = 0;
	for ( const auto &decal : editor.decals ) {
		for ( const Fragment &fragment : decal->fragments ) {
			out.Name( decal->shader );
			out.Int( 0 );
			out.Int( fragment.index );
			out.Int( static_cast<int>( fragment.points.size() ) );
			for ( const mohaa::PolyVert &point : fragment.points ) {
				for ( int i = 0; i < 3; i++ ) {
					out.Float( point.xyz[i] );
				}
				out.Float( point.st[0] );
				out.Float( point.st[1] );
				out.data.insert( out.data.end(), point.modulate, point.modulate + 4 );
			}
			for ( const int leaf : fragment.leafs ) {
				if ( leaf >= 0 && leaf < numLeafs ) {
					leafFragments[leaf].push_back( number );
				}
			}
			number++;
		}
	}
	for ( int i = 0; i < CM_NumInlineModels(); i++ ) {
		out.Int( 0 );
	}
	for ( const std::vector<int> &list : leafFragments ) {
		out.Int( static_cast<int>( list.size() ) );
		for ( const int fragment : list ) {
			out.Int( fragment );
		}
	}

	FS_WriteFile( editor.file.c_str(), out.data.data(), static_cast<int>( out.data.size() ) );
	Com_Printf( "Wrote %s: %i decals in %i fragments\n", editor.file.c_str(),
		static_cast<int>( editor.decals.size() ), numFragments );
}

const struct {
	const char *name;
	xcommand_t function;
} kCommands[] = {
	{ "dclc_make", Make_f },
	{ "dclc_reapply", Reapply_f },
	{ "dclc_delete", Delete_f },
	{ "dclc_selectnext", SelectNext_f },
	{ "dclc_selectprev", SelectPrev_f },
	{ "dclc_getinfo", GetInfo_f },
	{ "dclc_shiftup", ShiftUp_f },
	{ "dclc_shiftdown", ShiftDown_f },
	{ "dclc_shiftleft", ShiftLeft_f },
	{ "dclc_shiftright", ShiftRight_f },
	{ "dclc_randomroll", RandomRoll_f },
	{ "dclc_save", Save_f },
};

/* The definitions of the level's decal file, to be cut once the client game
 * draws its first frame. */
void ReadDefinitions() {
	void *data = nullptr;
	const int length = FS_ReadFile( editor.file.c_str(), &data );
	if ( length <= 0 || !data ) {
		Com_Printf( "Decal editor: %s not found; starting without decals\n", editor.file.c_str() );
		return;
	}
	dclLayoutMOHAA_t layout;
	const byte *bytes = static_cast<const byte *>( data );
	if ( !MOHAA_DCLLayout( bytes, length, &layout ) ) {
		Com_Printf( S_COLOR_YELLOW "Decal editor: %s is not a decal file this engine reads\n", editor.file.c_str() );
		FS_FreeFile( data );
		return;
	}
	for ( int i = 0; i < layout.numDecals; i++ ) {
		const byte *record = bytes + layout.decals + i * MOHAA_DCL_DECAL_BYTES;
		Decal *decal = NewDecal();
		char shader[MAX_QPATH];
		MOHAA_DCLShader( record, shader, sizeof( shader ) );
		decal->shader = shader;
		for ( int k = 0; k < 3; k++ ) {
			decal->position[k] = MOHAA_DCLFloat( record + MOHAA_DCL_DECAL_POSITION + k * 4 );
			decal->projection[k] = MOHAA_DCLFloat( record + MOHAA_DCL_DECAL_PROJECTION + k * 4 );
		}
		decal->radius = MOHAA_DCLFloat( record + MOHAA_DCL_DECAL_RADIUS );
		decal->heightScale = MOHAA_DCLFloat( record + MOHAA_DCL_DECAL_HEIGHT_SCALE );
		decal->widthScale = MOHAA_DCLFloat( record + MOHAA_DCL_DECAL_WIDTH_SCALE );
		decal->rotation = MOHAA_DCLFloat( record + MOHAA_DCL_DECAL_ROTATION );
		for ( int k = 0; k < 4; k++ ) {
			decal->color[k] = MOHAA_DCLFloat( record + MOHAA_DCL_DECAL_COLOR + k * 4 );
		}
		decal->lighting = MOHAA_DCLInt( record + MOHAA_DCL_DECAL_LIGHTING ) != 0;
	}
	FS_FreeFile( data );
	editor.pending = !editor.decals.empty();
	Com_Printf( "Decal editor: %s, %i decals to cut\n", editor.file.c_str(), layout.numDecals );
}

/* The loaded definitions cut where they lie with their own settings; one
 * the client game cuts nothing of is dropped (R_LevelMarksLoad). */
void CutDefinitions() {
	std::vector<std::unique_ptr<Decal>> loaded;
	loaded.swap( editor.decals );
	int dropped = 0;
	for ( auto &decal : loaded ) {
		editor.decals.push_back( std::move( decal ) );
		Decal *target = editor.decals.back().get();
		vec3_t position, projection;
		VectorCopy( target->position, position );
		VectorCopy( target->projection, projection );
		if ( !Apply( position, projection, target, true, 0 ) ) {
			editor.decals.pop_back();
			dropped++;
		}
	}
	editor.current = editor.decals.empty() ? nullptr : editor.decals.back().get();
	Com_Printf( "Decal editor: %i decals cut, %i cut nothing and were dropped\n",
		static_cast<int>( editor.decals.size() ), dropped );
}

/* ---- each frame ---------------------------------------------------------- */

/* dcl_pathmode: the client game follows the player and says when a segment
 * is laid (-1 none, 0 the last one grew, 1 a new one began); the editor
 * keeps the last segment, and the one before it once the path has three
 * points (R_UpdateLevelMarksSystem). */
void UpdatePath() {
	if ( !dcl_pathmode->integer ) {
		editor.pathLaying = false;
		return;
	}
	if ( !editor.pathLaying ) {
		editor.tread = mohaa::TreadMark{};
		editor.tread.iLastTime = -1000;
		editor.pathStart = editor.pathEnd = nullptr;
		editor.pathLaying = true;
	}
	editor.tread.hTreadShader = re.RegisterShader( dcl_shader->string );
	editor.tread.fWidth = Com_Clamp( 2.0f, 1024.0f, dcl_radius->value * dcl_widthscale->value );
	const float alpha = Clamp01( dcl_alpha->value );
	const float texScale = dcl_texturescale->value != 0.0f ? 1.0f / dcl_texturescale->value : 1.0f;
	const auto update = [alpha, texScale]() {
		return CL_MOHAACGamePermanentUpdateTreadMark( &editor.tread, alpha, dcl_minsegment->value,
			dcl_maxsegment->value, dcl_maxoffset->value, texScale );
	};
	const int result = update();
	if ( result == 1 ) {
		editor.pathStart = editor.pathEnd;
		editor.pathEnd = nullptr;
		update();
	}
	if ( result != -1 ) {
		if ( !editor.pathEnd ) {
			editor.pathEnd = NewDecal();
		}
		if ( !Apply( kUpwards, kUpwards, editor.pathEnd, true, 2 ) ) {
			editor.pathEnd->fragments.clear();
		}
		if ( editor.tread.iState == 3 ) {
			if ( !editor.pathStart ) {
				editor.pathStart = NewDecal();
			}
			if ( !Apply( kUpwards, kUpwards, editor.pathStart, true, 1 ) ) {
				editor.pathStart->fragments.clear();
			}
		}
	}
	if ( editor.pathEnd ) {
		editor.current = editor.pathEnd;
	} else if ( editor.pathStart ) {
		editor.current = editor.pathStart;
	}
}

bool TakeModified( cvar_t *cvar ) {
	const bool modified = cvar->modified != qfalse;
	cvar->modified = qfalse;
	return modified;
}

/* A setting changed in the menu cuts the selected decal again at once. */
void AutoApply() {
	Decal *decal = editor.current;
	if ( !editor.autoApply || !decal || decal->rotation == -1.0f ) {
		return;
	}
	bool changed = false;
	if ( TakeModified( dcl_shader ) ) {
		const qhandle_t handle = re.RegisterShader( dcl_shader->string );
		if ( handle && Q_stricmp( decal->shader.c_str(), dcl_shader->string ) ) {
			SetShader( *decal, dcl_shader->string );
			/* a new picture starts at radius 16, scaled to its size */
			int width = 0, height = 0;
			if ( re.GetShaderImageSize ) {
				re.GetShaderImageSize( handle, &width, &height );
			}
			Cvar_SetValue( "dcl_radius", 16.0f );
			dcl_radius->modified = qfalse;
			decal->radius = 16.0f;
			if ( width > 0 && height > 0 ) {
				Cvar_SetValue( "dcl_heightscale", height / 16.0f * 0.5f );
				Cvar_SetValue( "dcl_widthscale", width / 16.0f * 0.5f );
			}
			changed = true;
		}
	}
	const auto take = [&changed]( cvar_t *cvar, float value, float &field ) {
		if ( TakeModified( cvar ) && field != value ) {
			field = value;
			changed = true;
		}
	};
	take( dcl_radius, dcl_radius->value, decal->radius );
	take( dcl_heightscale, dcl_heightscale->value, decal->heightScale );
	take( dcl_widthscale, dcl_widthscale->value, decal->widthScale );
	take( dcl_rotation, AngleMod( dcl_rotation->value ), decal->rotation );
	take( dcl_r, Clamp01( dcl_r->value ), decal->color[0] );
	take( dcl_g, Clamp01( dcl_g->value ), decal->color[1] );
	take( dcl_b, Clamp01( dcl_b->value ), decal->color[2] );
	take( dcl_alpha, Clamp01( dcl_alpha->value ), decal->color[3] );
	if ( TakeModified( dcl_dolighting ) && decal->lighting != ( dcl_dolighting->integer != 0 ) ) {
		decal->lighting = dcl_dolighting->integer != 0;
		changed = true;
	}
	if ( changed ) {
		vec3_t position, projection;
		VectorCopy( decal->position, position );
		VectorCopy( decal->projection, projection );
		Apply( position, projection, decal, true, 0 );
	}
}

/* A line over the view: a quad a pixel and a half wide, facing it. */
void Line( const refdef_t &view, const vec3_t start, const vec3_t end, float r, float g, float b ) {
	vec3_t along, middle, toView, side;
	VectorSubtract( end, start, along );
	VectorAdd( start, end, middle );
	VectorScale( middle, 0.5f, middle );
	VectorSubtract( view.vieworg, middle, toView );
	CrossProduct( along, toView, side );
	if ( VectorNormalize( side ) < 0.0001f ) {
		return;
	}
	const float field = view.fov_x > 1.0f ? view.fov_x : 90.0f;
	const float pixel = 2.0f * VectorLength( toView ) * std::tan( DEG2RAD( field * 0.5f ) )
		/ static_cast<float>( std::max( view.width, 1 ) );
	VectorScale( side, pixel * 0.75f, side );

	/* clockwise seen from the view, as a polygon's front is */
	mohaa::PolyVert quad[4] = {};
	VectorAdd( start, side, quad[0].xyz );
	VectorSubtract( start, side, quad[1].xyz );
	VectorSubtract( end, side, quad[2].xyz );
	VectorAdd( end, side, quad[3].xyz );
	for ( mohaa::PolyVert &point : quad ) {
		point.modulate[0] = static_cast<byte>( Clamp01( r ) * 255.0f );
		point.modulate[1] = static_cast<byte>( Clamp01( g ) * 255.0f );
		point.modulate[2] = static_cast<byte>( Clamp01( b ) * 255.0f );
		point.modulate[3] = 255;
	}
	re.AddPolyToScene( cls.whiteShader, 4, reinterpret_cast<const polyVert_t *>( quad ), 1 );
}

/* The selected decal: its fragments' edges, each with a tick along the
 * direction it was laid from, and that direction, its width and its height
 * from its position, pulsing (R_UpdateLevelMarksSystem). */
void Outline( const refdef_t &view, const Decal &decal ) {
	vec3_t axis[3];
	VectorNormalize2( decal.projection, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], AngleMod( decal.rotation + 90.0f ) );
	CrossProduct( axis[0], axis[2], axis[1] );
	const float pulse = static_cast<float>( std::sin( view.time / 500.0 ) / 4.0 + 0.6 );

	for ( const Fragment &fragment : decal.fragments ) {
		const std::size_t count = fragment.points.size();
		for ( std::size_t j = 0; j < count; j++ ) {
			vec3_t a, b, tick;
			VectorMA( fragment.points[j].xyz, 0.1f, axis[0], a );
			VectorMA( fragment.points[( j + 1 ) % count].xyz, 0.1f, axis[0], b );
			Line( view, a, b, pulse, pulse, pulse );
			VectorMA( a, 8.0f, axis[0], tick );
			if ( fragment.index > 0 ) {
				Line( view, a, tick, pulse, 1.0f - pulse, 0.1f );
			} else {
				Line( view, a, tick, 0.1f, 1.0f - pulse, pulse );
			}
		}
	}

	vec3_t a, b;
	const float front = std::max( 4.0f, ( decal.widthScale + decal.heightScale ) * decal.radius * 0.75f );
	VectorMA( decal.position, front, axis[0], a );
	Line( view, decal.position, a, 0.1f, pulse, 1.0f - pulse );
	const float width = std::max( 1.0f, decal.radius * decal.widthScale );
	VectorMA( decal.position, width, axis[1], a );
	VectorMA( decal.position, -width, axis[1], b );
	Line( view, a, b, 0.1f, pulse, 1.0f - pulse );
	const float height = std::max( 1.0f, decal.radius * decal.heightScale );
	VectorMA( decal.position, height, axis[2], a );
	VectorMA( decal.position, -height, axis[2], b );
	Line( view, a, b, 0.1f, pulse, 1.0f - pulse );
}

void RemoveCommands() {
	if ( editor.commands ) {
		for ( const auto &command : kCommands ) {
			Cmd_RemoveCommand( command.name );
		}
		editor.commands = false;
	}
}

} // namespace

/* 2015's decal editor cvars, their defaults its renderer's
 * (R_LevelMarksInit). dcl_editmode takes effect as the next level loads. */
void CL_MOHAADecalsInit( void ) {
	dcl_editmode = Cvar_Get( "dcl_editmode", "0", CVAR_LATCH );
	Cvar_SetDescription( dcl_editmode, "Medal of Honor: from the next level loaded, edit its permanent decals "
		"(the decal menu, F6 in development.cfg) instead of drawing them from its file." );
	dcl_showcurrent = Cvar_Get( "dcl_showcurrent", "1", 0 );
	dcl_autogetinfo = Cvar_Get( "dcl_autogetinfo", "1", 0 );
	dcl_shiftstep = Cvar_Get( "dcl_shiftstep", "4", 0 );
	dcl_shader = Cvar_Get( "dcl_shader", "blastmark", 0 );
	dcl_radius = Cvar_Get( "dcl_radius", "16", 0 );
	dcl_heightscale = Cvar_Get( "dcl_heightscale", "1", 0 );
	dcl_widthscale = Cvar_Get( "dcl_widthscale", "1", 0 );
	dcl_rotation = Cvar_Get( "dcl_rotation", "0", 0 );
	dcl_r = Cvar_Get( "dcl_r", "1", 0 );
	dcl_g = Cvar_Get( "dcl_g", "1", 0 );
	dcl_b = Cvar_Get( "dcl_b", "1", 0 );
	dcl_alpha = Cvar_Get( "dcl_alpha", "1", 0 );
	dcl_dolighting = Cvar_Get( "dcl_dolighting", "1", 0 );
	dcl_doworld = Cvar_Get( "dcl_doworld", "1", 0 );
	dcl_doterrain = Cvar_Get( "dcl_doterrain", "1", 0 );
	dcl_dobmodels = Cvar_Get( "dcl_dobmodels", "1", 0 );
	dcl_dostring = Cvar_Get( "dcl_dostring", "apply to all", 0 );
	dcl_pathmode = Cvar_Get( "dcl_pathmode", "0", 0 );
	dcl_maxsegment = Cvar_Get( "dcl_maxsegment", "512", 0 );
	dcl_minsegment = Cvar_Get( "dcl_minsegment", "24", 0 );
	dcl_maxoffset = Cvar_Get( "dcl_maxoffset", "10", 0 );
	dcl_texturescale = Cvar_Get( "dcl_texturescale", "32", 0 );
}

/* As a level begins to load, before its world does: edit mode reads the
 * level's decal file, which the renderers then leave unbuilt. */
void CL_MOHAADecalsBeginLevel( const char *mapName ) {
	CL_MOHAADecalsShutdown();
	/* a value set since the last level takes effect now */
	dcl_editmode = Cvar_Get( "dcl_editmode", "0", CVAR_LATCH );
	if ( !dcl_editmode->integer || !mapName || !mapName[0] ) {
		return;
	}
	char name[MAX_QPATH];
	COM_StripExtension( mapName, name, sizeof( name ) );
	Q_strcat( name, sizeof( name ), ".dcl" );
	editor.active = true;
	editor.file = name;
	for ( const auto &command : kCommands ) {
		Cmd_AddCommand( command.name, command.function );
	}
	editor.commands = true;
	ReadDefinitions();
}

void CL_MOHAADecalsShutdown( void ) {
	RemoveCommands();
	editor = Editor();
}

/* Each frame of the world view, before it is drawn. */
void CL_MOHAADecalsFrame( const refdef_t *view ) {
	if ( !editor.active || !view ) {
		return;
	}
	VectorCopy( view->vieworg, editor.viewOrigin );
	VectorCopy( view->viewaxis[0], editor.viewForward );
	editor.viewSeen = true;
	if ( editor.pending ) {
		editor.pending = false;
		CutDefinitions();
	}
	UpdatePath();
	AutoApply();

	if ( Cvar_VariableIntegerValue( "r_drawstaticdecals" ) ) {
		for ( const auto &decal : editor.decals ) {
			if ( !decal->handle ) {
				continue;
			}
			for ( const Fragment &fragment : decal->fragments ) {
				re.AddPolyToScene( decal->handle, static_cast<int>( fragment.points.size() ),
					reinterpret_cast<const polyVert_t *>( fragment.points.data() ), 1 );
			}
		}
	}
	if ( dcl_showcurrent->integer && editor.current && !editor.current->fragments.empty() ) {
		Outline( *view, *editor.current );
	}
}
