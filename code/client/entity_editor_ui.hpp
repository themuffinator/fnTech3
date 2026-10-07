/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

The entity editor's drawing. Its interface is Dear ImGui's, and this turns
ImGui's draw lists into what the engine's renderers draw in 2D: a stretched
picture for each upright rectangle of one colour (a glyph, a filled box),
and a quad of four textured, coloured corners for the rest. The renderers'
2D path has no scissor, so ImGui's clip rectangles are applied here, on the
vertices; and it draws only the face that runs clockwise on the screen and
splits a quad along its first diagonal, so each quad is wound and ordered
for that. It knows nothing of the engine: tests drive it with ImGui frames
of their own.
===========================================================================
*/
#ifndef FNTECH3_CLIENT_ENTITY_EDITOR_UI_HPP
#define FNTECH3_CLIENT_ENTITY_EDITOR_UI_HPP

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fntech3::entity_editor::ui {

struct Color {
	float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

	[[nodiscard]] constexpr Color Alpha( float scale ) const noexcept { return { r, g, b, a * scale }; }
	[[nodiscard]] constexpr Color Lighten( float amount ) const noexcept
	{
		return { r + ( 1.0f - r ) * amount, g + ( 1.0f - g ) * amount, b + ( 1.0f - b ) * amount, a };
	}
	[[nodiscard]] ImU32 Packed() const noexcept
	{
		const auto byte = []( float value ) {
			return static_cast<ImU32>( std::clamp( value, 0.0f, 1.0f ) * 255.0f + 0.5f );
		};
		return IM_COL32( byte( r ), byte( g ), byte( b ), byte( a ) );
	}
	[[nodiscard]] static Color Unpacked( ImU32 packed ) noexcept
	{
		return { static_cast<float>( ( packed >> IM_COL32_R_SHIFT ) & 0xFFu ) / 255.0f,
			static_cast<float>( ( packed >> IM_COL32_G_SHIFT ) & 0xFFu ) / 255.0f,
			static_cast<float>( ( packed >> IM_COL32_B_SHIFT ) & 0xFFu ) / 255.0f,
			static_cast<float>( ( packed >> IM_COL32_A_SHIFT ) & 0xFFu ) / 255.0f };
	}
	[[nodiscard]] bool operator==( const Color &o ) const noexcept
	{
		return r == o.r && g == o.g && b == o.b && a == o.a;
	}
};

struct Rect {
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

	[[nodiscard]] constexpr bool Contains( float px, float py ) const noexcept
	{
		return px >= x && py >= y && px < x + w && py < y + h;
	}
	[[nodiscard]] constexpr float Right() const noexcept { return x + w; }
	[[nodiscard]] constexpr float Bottom() const noexcept { return y + h; }
	[[nodiscard]] constexpr Rect Intersect( const Rect &o ) const noexcept
	{
		const float left = std::max( x, o.x ), top = std::max( y, o.y );
		const float right = std::min( Right(), o.Right() ), bottom = std::min( Bottom(), o.Bottom() );
		return { left, top, std::max( 0.0f, right - left ), std::max( 0.0f, bottom - top ) };
	}
};

/* ---- what the renderers draw ---------------------------------------------- */

enum class DrawKind { Pic, Quad };

struct DrawCommand {
	DrawKind kind = DrawKind::Pic;
	/* Pic: x, y in [0], w, h in [1]; Quad: its corners, clockwise */
	float points[4][2] = {};
	/* Pic: s1, t1 in [0], s2, t2 in [1]; Quad: each corner's */
	float st[4][2] = {};
	/* Pic: [0]; Quad: each corner's */
	Color colors[4];
	/* the ImGui texture sampled: in the engine, a shader handle */
	ImTextureID texture = ImTextureID_Invalid;
};

class DrawList {
public:
	void Clear() noexcept { commands_.clear(); }
	[[nodiscard]] const std::vector<DrawCommand> &Commands() const noexcept { return commands_; }

	void Pic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const Color &color,
		ImTextureID texture )
	{
		if ( !( w > 0.0f ) || !( h > 0.0f ) || color.a <= 0.0f ) {
			return;
		}
		DrawCommand command;
		command.kind = DrawKind::Pic;
		command.points[0][0] = x;
		command.points[0][1] = y;
		command.points[1][0] = w;
		command.points[1][1] = h;
		command.st[0][0] = s1;
		command.st[0][1] = t1;
		command.st[1][0] = s2;
		command.st[1][1] = t2;
		command.colors[0] = color;
		command.texture = texture;
		commands_.push_back( command );
	}

	/* Four corners in either winding, each with its texture coordinates and
	 * colour, stored clockwise on the screen (y down), the order kept around
	 * so that the diagonal from corner 0 to corner 2 stays the one the
	 * renderers split along. Nothing is stored for a quad with no area. */
	void Quad( const ImVec2 xy[4], const ImVec2 uv[4], const ImU32 col[4], ImTextureID texture )
	{
		float area = 0.0f;
		for ( int i = 0; i < 4; i++ ) {
			const int next = ( i + 1 ) & 3;
			area += xy[i].x * xy[next].y - xy[next].x * xy[i].y;
		}
		if ( !( std::fabs( area ) > 1.0e-6f ) || !std::isfinite( area ) ) {
			return;
		}
		static const int clockwise[4] = { 0, 1, 2, 3 };
		static const int reversed[4] = { 0, 3, 2, 1 };
		const int *order = area > 0.0f ? clockwise : reversed;
		DrawCommand command;
		command.kind = DrawKind::Quad;
		for ( int i = 0; i < 4; i++ ) {
			command.points[i][0] = xy[order[i]].x;
			command.points[i][1] = xy[order[i]].y;
			command.st[i][0] = uv[order[i]].x;
			command.st[i][1] = uv[order[i]].y;
			command.colors[i] = Color::Unpacked( col[order[i]] );
		}
		command.texture = texture;
		commands_.push_back( command );
	}

private:
	std::vector<DrawCommand> commands_;
};

/* ---- ImGui's draw lists ------------------------------------------------- */

namespace detail {

struct Vertex {
	ImVec2 pos, uv;
	float col[4];
};

inline Vertex FromImGui( const ImDrawVert &v ) noexcept
{
	const Color c = Color::Unpacked( v.col );
	return Vertex{ v.pos, v.uv, { c.r, c.g, c.b, c.a } };
}

inline ImU32 Packed( const Vertex &v ) noexcept
{
	return Color{ v.col[0], v.col[1], v.col[2], v.col[3] }.Packed();
}

inline Vertex Lerp( const Vertex &a, const Vertex &b, float t ) noexcept
{
	Vertex out;
	out.pos = ImVec2( a.pos.x + ( b.pos.x - a.pos.x ) * t, a.pos.y + ( b.pos.y - a.pos.y ) * t );
	out.uv = ImVec2( a.uv.x + ( b.uv.x - a.uv.x ) * t, a.uv.y + ( b.uv.y - a.uv.y ) * t );
	for ( int i = 0; i < 4; i++ ) {
		out.col[i] = a.col[i] + ( b.col[i] - a.col[i] ) * t;
	}
	return out;
}

inline bool Inside( const ImVec4 &clip, const ImVec2 &p ) noexcept
{
	return p.x >= clip.x && p.x <= clip.z && p.y >= clip.y && p.y <= clip.w;
}

constexpr int kMaxClipped = 16;

/* Sutherland and Hodgman's clip of a convex polygon to an upright
 * rectangle; how many corners are left. */
inline int ClipPolygon( const Vertex *in, int count, const ImVec4 &clip, Vertex *out )
{
	Vertex buffers[2][kMaxClipped];
	int counts[2] = { std::min( count, kMaxClipped ), 0 };
	std::copy( in, in + counts[0], buffers[0] );
	int current = 0;
	for ( int edge = 0; edge < 4; edge++ ) {
		const auto distance = [edge, &clip]( const Vertex &v ) {
			switch ( edge ) {
			case 0: return v.pos.x - clip.x;
			case 1: return clip.z - v.pos.x;
			case 2: return v.pos.y - clip.y;
			default: return clip.w - v.pos.y;
			}
		};
		const Vertex *source = buffers[current];
		Vertex *target = buffers[current ^ 1];
		int written = 0;
		const int n = counts[current];
		for ( int i = 0; i < n && written < kMaxClipped - 1; i++ ) {
			const Vertex &a = source[i], &b = source[( i + 1 ) % n];
			const float da = distance( a ), db = distance( b );
			if ( da >= 0.0f ) {
				target[written++] = a;
			}
			if ( ( da >= 0.0f ) != ( db >= 0.0f ) ) {
				target[written++] = Lerp( a, b, da / ( da - db ) );
			}
		}
		counts[current ^ 1] = written;
		current ^= 1;
		if ( written < 3 ) {
			return 0;
		}
	}
	std::copy( buffers[current], buffers[current] + counts[current], out );
	return counts[current];
}

/* A convex polygon as a fan of quads from its first corner: corners 0, 1,
 * 2, 3, then 0, 3, 4, 5, and a last odd triangle with a corner doubled. */
inline void EmitFan( const Vertex *poly, int count, ImTextureID texture, DrawList &out )
{
	for ( int first = 1; first + 1 < count; first += 2 ) {
		const Vertex &a = poly[0], &b = poly[first], &c = poly[first + 1];
		const Vertex &d = first + 2 < count ? poly[first + 2] : c;
		const ImVec2 xy[4] = { a.pos, b.pos, c.pos, d.pos };
		const ImVec2 uv[4] = { a.uv, b.uv, c.uv, d.uv };
		const ImU32 col[4] = { Packed( a ), Packed( b ), Packed( c ), Packed( d ) };
		out.Quad( xy, uv, col, texture );
	}
}

/* A triangle, clipped. */
inline void EmitTriangle( const ImDrawVert &a, const ImDrawVert &b, const ImDrawVert &c, const ImVec4 &clip,
	ImTextureID texture, DrawList &out )
{
	const Vertex tri[3] = { FromImGui( a ), FromImGui( b ), FromImGui( c ) };
	Vertex clipped[kMaxClipped];
	const bool inside = Inside( clip, a.pos ) && Inside( clip, b.pos ) && Inside( clip, c.pos );
	const int count = inside ? 3 : ClipPolygon( tri, 3, clip, clipped );
	EmitFan( inside ? tri : clipped, count, texture, out );
}

/* Whether four corners make an upright rectangle of one colour whose
 * texture runs straight along it: a picture, then, of these bounds. */
inline bool UprightPicture( const ImDrawVert *const v[4], ImVec4 &bounds, ImVec4 &texture )
{
	float x0 = v[0]->pos.x, x1 = x0, y0 = v[0]->pos.y, y1 = y0;
	for ( int i = 1; i < 4; i++ ) {
		if ( v[i]->col != v[0]->col ) {
			return false;
		}
		x0 = std::min( x0, v[i]->pos.x );
		x1 = std::max( x1, v[i]->pos.x );
		y0 = std::min( y0, v[i]->pos.y );
		y1 = std::max( y1, v[i]->pos.y );
	}
	if ( !( x1 > x0 ) || !( y1 > y0 ) ) {
		return false;
	}
	bool corner[4] = {};
	float u[2] = {}, w[2] = {};
	bool seenU[2] = {}, seenW[2] = {};
	for ( int i = 0; i < 4; i++ ) {
		const bool left = v[i]->pos.x == x0, right = v[i]->pos.x == x1;
		const bool top = v[i]->pos.y == y0, bottom = v[i]->pos.y == y1;
		if ( !( left || right ) || !( top || bottom ) ) {
			return false;
		}
		const int column = right ? 1 : 0, row = bottom ? 1 : 0;
		corner[row * 2 + column] = true;
		if ( seenU[column] && u[column] != v[i]->uv.x ) {
			return false;
		}
		if ( seenW[row] && w[row] != v[i]->uv.y ) {
			return false;
		}
		u[column] = v[i]->uv.x;
		w[row] = v[i]->uv.y;
		seenU[column] = seenW[row] = true;
	}
	if ( !( corner[0] && corner[1] && corner[2] && corner[3] ) ) {
		return false;
	}
	bounds = ImVec4( x0, y0, x1, y1 );
	texture = ImVec4( u[0], w[0], u[1], w[1] );
	return true;
}

/* A picture cut to the clip rectangle, its texture cut with it. */
inline void EmitPicture( const ImVec4 &bounds, const ImVec4 &texture, ImU32 col, const ImVec4 &clip,
	ImTextureID id, DrawList &out )
{
	const float x0 = std::max( bounds.x, clip.x ), y0 = std::max( bounds.y, clip.y );
	const float x1 = std::min( bounds.z, clip.z ), y1 = std::min( bounds.w, clip.w );
	if ( !( x1 > x0 ) || !( y1 > y0 ) ) {
		return;
	}
	const float width = bounds.z - bounds.x, height = bounds.w - bounds.y;
	const auto s = [&]( float x ) { return texture.x + ( texture.z - texture.x ) * ( x - bounds.x ) / width; };
	const auto t = [&]( float y ) { return texture.y + ( texture.w - texture.y ) * ( y - bounds.y ) / height; };
	out.Pic( x0, y0, x1 - x0, y1 - y0, s( x0 ), t( y0 ), s( x1 ), t( y1 ), Color::Unpacked( col ), id );
}

} // namespace detail

/* One of ImGui's draw lists appended as pictures and quads, in its order.
 * Two triangles that share an edge are one quad -- each rectangle, glyph
 * and line ImGui draws -- and an upright rectangle of one colour a
 * picture; whatever crosses its clip rectangle is cut to it. */
inline void AppendDrawList( const ImDrawList &list, DrawList &out )
{
	for ( const ImDrawCmd &cmd : list.CmdBuffer ) {
		if ( cmd.UserCallback || cmd.ElemCount < 3 ) {
			continue;
		}
		const ImVec4 &clip = cmd.ClipRect;
		if ( !( clip.z > clip.x ) || !( clip.w > clip.y ) ) {
			continue;
		}
		const ImTextureID texture = cmd.GetTexID();
		const ImDrawIdx *idx = list.IdxBuffer.Data + cmd.IdxOffset;
		const ImDrawVert *vtx = list.VtxBuffer.Data + cmd.VtxOffset;
		unsigned i = 0;
		while ( i + 2 < cmd.ElemCount ) {
			const unsigned a[3] = { idx[i], idx[i + 1], idx[i + 2] };
			if ( i + 5 < cmd.ElemCount ) {
				const unsigned b[3] = { idx[i + 3], idx[i + 4], idx[i + 5] };
				/* the corners both triangles have, and the one each has alone */
				int shared[2] = { 0, 0 }, count = 0, aloneA = -1, aloneB = -1;
				for ( int m = 0; m < 3; m++ ) {
					const bool inB = a[m] == b[0] || a[m] == b[1] || a[m] == b[2];
					if ( inB && count < 2 ) {
						shared[count++] = m;
					} else if ( !inB ) {
						aloneA = m;
					}
				}
				for ( int m = 0; m < 3; m++ ) {
					if ( b[m] != a[0] && b[m] != a[1] && b[m] != a[2] ) {
						aloneB = m;
					}
				}
				if ( count == 2 && aloneA >= 0 && aloneB >= 0 ) {
					/* corners 0 and 2 the shared edge, so the renderers split
					 * the quad into these same two triangles */
					const ImDrawVert *quad[4] = { &vtx[a[shared[0]]], &vtx[a[aloneA]], &vtx[a[shared[1]]],
						&vtx[b[aloneB]] };
					ImVec4 bounds, coords;
					if ( detail::UprightPicture( quad, bounds, coords ) ) {
						detail::EmitPicture( bounds, coords, quad[0]->col, clip, texture, out );
					} else if ( detail::Inside( clip, quad[0]->pos ) && detail::Inside( clip, quad[1]->pos )
						&& detail::Inside( clip, quad[2]->pos ) && detail::Inside( clip, quad[3]->pos ) ) {
						const ImVec2 xy[4] = { quad[0]->pos, quad[1]->pos, quad[2]->pos, quad[3]->pos };
						const ImVec2 uv[4] = { quad[0]->uv, quad[1]->uv, quad[2]->uv, quad[3]->uv };
						const ImU32 col[4] = { quad[0]->col, quad[1]->col, quad[2]->col, quad[3]->col };
						out.Quad( xy, uv, col, texture );
					} else {
						detail::EmitTriangle( vtx[a[0]], vtx[a[1]], vtx[a[2]], clip, texture, out );
						detail::EmitTriangle( vtx[b[0]], vtx[b[1]], vtx[b[2]], clip, texture, out );
					}
					i += 6;
					continue;
				}
			}
			detail::EmitTriangle( vtx[a[0]], vtx[a[1]], vtx[a[2]], clip, texture, out );
			i += 3;
		}
	}
}

} // namespace fntech3::entity_editor::ui

#endif
