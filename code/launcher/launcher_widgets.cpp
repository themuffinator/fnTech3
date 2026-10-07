/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_widgets.hpp"

#include "launcher_fonts.hpp"

#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace fntech3::launcher::gui {

namespace {

constexpr float kPi = 3.14159265358979f;

ImU32 Rgb( int r, int g, int b, int a = 255 )
{
	return IM_COL32( r, g, b, a );
}

ImVec4 Vec( ImU32 color )
{
	return ImGui::ColorConvertU32ToFloat4( color );
}

ImVec2 Point( ImVec2 center, float size, float x, float y )
{
	return ImVec2( center.x + x * size, center.y + y * size );
}

bool Continuation( char c )
{
	return ( static_cast<unsigned char>( c ) & 0xc0 ) == 0x80;
}

} // namespace

Palette DarkPalette()
{
	Palette p;
	p.dark = true;
	p.background = Rgb( 13, 15, 19 );
	p.backgroundRaised = Rgb( 18, 21, 27 );
	p.glow = Rgb( 255, 107, 26, 30 );
	p.glowCool = Rgb( 72, 112, 255, 16 );
	p.card = Rgb( 22, 25, 32 );
	p.cardHover = Rgb( 28, 32, 41 );
	p.cardSelected = Rgb( 37, 30, 31 );
	p.cardBorder = Rgb( 35, 40, 51 );
	p.cardBorderHover = Rgb( 50, 57, 72 );
	p.accent = Rgb( 255, 107, 26 );
	p.accentHover = Rgb( 255, 128, 56 );
	p.accentActive = Rgb( 232, 90, 12 );
	p.accentSoft = Rgb( 255, 107, 26, 40 );
	p.onAccent = Rgb( 255, 255, 255 );
	p.text = Rgb( 237, 239, 243 );
	p.textSecondary = Rgb( 163, 171, 185 );
	p.textFaint = Rgb( 112, 120, 136 );
	p.textDisabled = Rgb( 86, 93, 107 );
	p.field = Rgb( 17, 20, 26 );
	p.fieldHover = Rgb( 21, 24, 31 );
	p.fieldBorder = Rgb( 38, 43, 55 );
	p.fieldFocus = Rgb( 255, 107, 26, 190 );
	p.ghostHover = Rgb( 255, 255, 255, 14 );
	p.ghostActive = Rgb( 255, 255, 255, 26 );
	p.separator = Rgb( 255, 255, 255, 16 );
	p.shadow = Rgb( 0, 0, 0, 110 );
	p.overlay = Rgb( 6, 7, 10, 170 );
	p.popup = Rgb( 25, 29, 37 );
	p.popupBorder = Rgb( 45, 51, 64 );
	p.success = Rgb( 61, 214, 140 );
	p.warning = Rgb( 245, 185, 66 );
	p.danger = Rgb( 255, 99, 99 );
	p.info = Rgb( 110, 168, 255 );
	p.iconPlate = Rgb( 255, 255, 255, 10 );
	p.skeleton = Rgb( 29, 33, 42 );
	p.skeletonShine = Rgb( 41, 46, 58 );
	return p;
}

Palette LightPalette()
{
	Palette p;
	p.dark = false;
	p.background = Rgb( 243, 244, 247 );
	p.backgroundRaised = Rgb( 250, 250, 252 );
	p.glow = Rgb( 255, 107, 26, 24 );
	p.glowCool = Rgb( 72, 112, 255, 12 );
	p.card = Rgb( 255, 255, 255 );
	p.cardHover = Rgb( 252, 252, 253 );
	p.cardSelected = Rgb( 255, 247, 241 );
	p.cardBorder = Rgb( 225, 228, 234 );
	p.cardBorderHover = Rgb( 204, 209, 218 );
	p.accent = Rgb( 236, 88, 10 );
	p.accentHover = Rgb( 250, 104, 24 );
	p.accentActive = Rgb( 212, 76, 6 );
	p.accentSoft = Rgb( 236, 88, 10, 28 );
	p.onAccent = Rgb( 255, 255, 255 );
	p.text = Rgb( 20, 23, 28 );
	p.textSecondary = Rgb( 78, 86, 101 );
	p.textFaint = Rgb( 128, 136, 150 );
	p.textDisabled = Rgb( 168, 174, 186 );
	p.field = Rgb( 255, 255, 255 );
	p.fieldHover = Rgb( 250, 251, 252 );
	p.fieldBorder = Rgb( 212, 216, 224 );
	p.fieldFocus = Rgb( 236, 88, 10, 200 );
	p.ghostHover = Rgb( 0, 0, 0, 10 );
	p.ghostActive = Rgb( 0, 0, 0, 20 );
	p.separator = Rgb( 0, 0, 0, 18 );
	p.shadow = Rgb( 20, 26, 38, 34 );
	p.overlay = Rgb( 18, 22, 30, 96 );
	p.popup = Rgb( 255, 255, 255 );
	p.popupBorder = Rgb( 220, 224, 231 );
	p.success = Rgb( 22, 163, 74 );
	p.warning = Rgb( 196, 132, 4 );
	p.danger = Rgb( 220, 38, 38 );
	p.info = Rgb( 37, 99, 235 );
	p.iconPlate = Rgb( 0, 0, 0, 8 );
	p.skeleton = Rgb( 232, 235, 240 );
	p.skeletonShine = Rgb( 244, 246, 249 );
	return p;
}

ImU32 Mix( ImU32 a, ImU32 b, float t )
{
	t = std::clamp( t, 0.0f, 1.0f );
	auto channel = [t]( ImU32 x, ImU32 y, int shift ) {
		const float from = float( ( x >> shift ) & 0xff );
		const float to = float( ( y >> shift ) & 0xff );
		return ImU32( std::lround( from + ( to - from ) * t ) ) << shift;
	};
	return channel( a, b, IM_COL32_R_SHIFT ) | channel( a, b, IM_COL32_G_SHIFT )
		| channel( a, b, IM_COL32_B_SHIFT ) | channel( a, b, IM_COL32_A_SHIFT );
}

ImU32 WithAlpha( ImU32 color, float alpha )
{
	const float current = float( ( color >> IM_COL32_A_SHIFT ) & 0xff );
	const ImU32 value = ImU32( std::lround( std::clamp( current * alpha, 0.0f, 255.0f ) ) );
	return ( color & ~IM_COL32_A_MASK ) | ( value << IM_COL32_A_SHIFT );
}

void ApplyStyle( const Palette &palette, float scale )
{
	ImGuiStyle style;
	if ( palette.dark ) ImGui::StyleColorsDark( &style );
	else ImGui::StyleColorsLight( &style );
	style.WindowPadding = ImVec2( 22.0f, 20.0f );
	style.FramePadding = ImVec2( 11.0f, 8.0f );
	style.ItemSpacing = ImVec2( 10.0f, 9.0f );
	style.ItemInnerSpacing = ImVec2( 8.0f, 6.0f );
	style.CellPadding = ImVec2( 8.0f, 6.0f );
	style.WindowRounding = 16.0f;
	style.ChildRounding = 10.0f;
	style.FrameRounding = 9.0f;
	style.PopupRounding = 11.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 8.0f;
	style.TabRounding = 8.0f;
	style.WindowBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.ChildBorderSize = 0.0f;
	style.ScrollbarSize = 10.0f;
	style.GrabMinSize = 28.0f;
	style.DisabledAlpha = 0.45f;
	style.WindowTitleAlign = ImVec2( 0.5f, 0.5f );
	style.SelectableTextAlign = ImVec2( 0.0f, 0.5f );
	style.ScaleAllSizes( scale );
	style.FontSizeBase = 14.0f;
	style.FontScaleMain = 1.0f;
	style.FontScaleDpi = scale;

	ImVec4 *colors = style.Colors;
	colors[ImGuiCol_Text] = Vec( palette.text );
	colors[ImGuiCol_TextDisabled] = Vec( palette.textDisabled );
	colors[ImGuiCol_WindowBg] = Vec( palette.popup );
	colors[ImGuiCol_ChildBg] = ImVec4( 0, 0, 0, 0 );
	colors[ImGuiCol_PopupBg] = Vec( palette.popup );
	colors[ImGuiCol_Border] = Vec( palette.popupBorder );
	colors[ImGuiCol_BorderShadow] = ImVec4( 0, 0, 0, 0 );
	colors[ImGuiCol_FrameBg] = Vec( palette.field );
	colors[ImGuiCol_FrameBgHovered] = Vec( palette.fieldHover );
	colors[ImGuiCol_FrameBgActive] = Vec( palette.fieldHover );
	colors[ImGuiCol_TitleBg] = Vec( palette.popup );
	colors[ImGuiCol_TitleBgActive] = Vec( palette.popup );
	colors[ImGuiCol_TitleBgCollapsed] = Vec( palette.popup );
	colors[ImGuiCol_MenuBarBg] = Vec( palette.popup );
	colors[ImGuiCol_ScrollbarBg] = ImVec4( 0, 0, 0, 0 );
	colors[ImGuiCol_ScrollbarGrab] = Vec( WithAlpha( palette.text, 0.16f ) );
	colors[ImGuiCol_ScrollbarGrabHovered] = Vec( WithAlpha( palette.text, 0.28f ) );
	colors[ImGuiCol_ScrollbarGrabActive] = Vec( WithAlpha( palette.text, 0.36f ) );
	colors[ImGuiCol_CheckMark] = Vec( palette.accent );
	colors[ImGuiCol_SliderGrab] = Vec( palette.accent );
	colors[ImGuiCol_SliderGrabActive] = Vec( palette.accentActive );
	colors[ImGuiCol_Button] = Vec( palette.ghostHover );
	colors[ImGuiCol_ButtonHovered] = Vec( palette.ghostActive );
	colors[ImGuiCol_ButtonActive] = Vec( palette.ghostActive );
	colors[ImGuiCol_Header] = Vec( palette.accentSoft );
	colors[ImGuiCol_HeaderHovered] = Vec( palette.ghostActive );
	colors[ImGuiCol_HeaderActive] = Vec( palette.accentSoft );
	colors[ImGuiCol_Separator] = Vec( palette.separator );
	colors[ImGuiCol_SeparatorHovered] = Vec( palette.accent );
	colors[ImGuiCol_SeparatorActive] = Vec( palette.accent );
	colors[ImGuiCol_ResizeGrip] = ImVec4( 0, 0, 0, 0 );
	colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0, 0, 0, 0 );
	colors[ImGuiCol_ResizeGripActive] = ImVec4( 0, 0, 0, 0 );
	colors[ImGuiCol_TextSelectedBg] = Vec( WithAlpha( palette.accent, 0.32f ) );
	colors[ImGuiCol_NavCursor] = Vec( palette.accent );
	colors[ImGuiCol_ModalWindowDimBg] = Vec( palette.overlay );
	colors[ImGuiCol_InputTextCursor] = Vec( palette.accent );
	ImGui::GetStyle() = style;
}

void DrawIcon( ImDrawList *draw, Icon icon, ImVec2 c, float size, ImU32 color, float thickness )
{
	const float s = size;
	auto p = [c, s]( float x, float y ) { return Point( c, s, x, y ); };
	switch ( icon ) {
	case Icon::Play: {
		const ImVec2 a = p( -0.26f, -0.38f ), b = p( 0.40f, 0.0f ), d = p( -0.26f, 0.38f );
		draw->AddTriangleFilled( a, b, d, color );
		const ImVec2 points[] = { a, b, d };
		draw->AddPolyline( points, 3, color, thickness * 0.9f, ImDrawFlags_Closed );
		break;
	}
	case Icon::Plus:
		draw->AddLine( p( -0.36f, 0.0f ), p( 0.36f, 0.0f ), color, thickness );
		draw->AddLine( p( 0.0f, -0.36f ), p( 0.0f, 0.36f ), color, thickness );
		break;
	case Icon::Refresh: {
		const float radius = 0.34f * s;
		const float start = -kPi * 0.30f, end = start + kPi * 1.62f;
		draw->PathArcTo( c, radius, start, end, 28 );
		draw->PathStroke( color, thickness );
		const ImVec2 tip( c.x + std::cos( start ) * radius, c.y + std::sin( start ) * radius );
		const ImVec2 tangent( -std::sin( start ), std::cos( start ) );
		const ImVec2 normal( std::cos( start ), std::sin( start ) );
		const float head = 0.20f * s;
		const ImVec2 back( tip.x + tangent.x * head * 0.15f, tip.y + tangent.y * head * 0.15f );
		draw->AddTriangleFilled(
			ImVec2( back.x - tangent.x * head, back.y - tangent.y * head ),
			ImVec2( back.x + normal.x * head * 0.75f, back.y + normal.y * head * 0.75f ),
			ImVec2( back.x - normal.x * head * 0.75f, back.y - normal.y * head * 0.75f ), color );
		break;
	}
	case Icon::Settings: {
		const float rows[] = { -0.28f, 0.0f, 0.28f };
		const float knobs[] = { 0.16f, -0.18f, 0.06f };
		for ( int i = 0; i < 3; ++i ) {
			draw->AddLine( p( -0.38f, rows[i] ), p( 0.38f, rows[i] ), color, thickness );
			draw->AddCircleFilled( p( knobs[i], rows[i] ), 0.095f * s, color, 16 );
		}
		break;
	}
	case Icon::Search:
		draw->AddCircle( p( -0.07f, -0.07f ), 0.27f * s, color, 24, thickness );
		draw->AddLine( p( 0.13f, 0.13f ), p( 0.38f, 0.38f ), color, thickness * 1.15f );
		break;
	case Icon::Folder: {
		const ImVec2 points[] = { p( -0.40f, -0.30f ), p( -0.10f, -0.30f ), p( 0.02f, -0.18f ), p( 0.40f, -0.18f ),
			p( 0.40f, 0.30f ), p( -0.40f, 0.30f ) };
		draw->AddPolyline( points, 6, color, thickness, ImDrawFlags_Closed );
		draw->AddLine( p( -0.40f, -0.10f ), p( 0.40f, -0.10f ), color, thickness * 0.8f );
		break;
	}
	case Icon::Copy: {
		const ImVec2 back[] = { p( -0.36f, 0.14f ), p( -0.36f, -0.38f ), p( 0.14f, -0.38f ) };
		draw->AddPolyline( back, 3, color, thickness );
		draw->AddRect( p( -0.18f, -0.20f ), p( 0.36f, 0.38f ), color, 0.08f * s, thickness );
		break;
	}
	case Icon::Command: {
		draw->AddRect( p( -0.42f, -0.34f ), p( 0.42f, 0.34f ), color, 0.10f * s, thickness );
		const ImVec2 chevron[] = { p( -0.24f, -0.12f ), p( -0.08f, 0.0f ), p( -0.24f, 0.12f ) };
		draw->AddPolyline( chevron, 3, color, thickness );
		draw->AddLine( p( 0.02f, 0.14f ), p( 0.24f, 0.14f ), color, thickness );
		break;
	}
	case Icon::Edit: {
		const ImVec2 pencil[] = { p( -0.36f, 0.36f ), p( -0.27f, 0.10f ), p( 0.16f, -0.33f ), p( 0.33f, -0.16f ),
			p( -0.10f, 0.27f ) };
		draw->AddPolyline( pencil, 5, color, thickness, ImDrawFlags_Closed );
		draw->AddLine( p( 0.05f, -0.22f ), p( 0.22f, -0.05f ), color, thickness * 0.8f );
		break;
	}
	case Icon::Hide:
	case Icon::Show: {
		const float radius = 0.58f * s;
		draw->PathArcTo( p( 0.0f, 0.40f ), radius, -2.38f, -0.76f, 20 );
		draw->PathStroke( color, thickness );
		draw->PathArcTo( p( 0.0f, -0.40f ), radius, 0.76f, 2.38f, 20 );
		draw->PathStroke( color, thickness );
		draw->AddCircle( c, 0.11f * s, color, 16, thickness );
		if ( icon == Icon::Hide ) draw->AddLine( p( -0.36f, 0.36f ), p( 0.36f, -0.36f ), color, thickness );
		break;
	}
	case Icon::Trash: {
		draw->AddLine( p( -0.38f, -0.27f ), p( 0.38f, -0.27f ), color, thickness );
		const ImVec2 handle[] = { p( -0.11f, -0.27f ), p( -0.11f, -0.39f ), p( 0.11f, -0.39f ), p( 0.11f, -0.27f ) };
		draw->AddPolyline( handle, 4, color, thickness * 0.9f );
		const ImVec2 body[] = { p( -0.27f, -0.27f ), p( -0.21f, 0.38f ), p( 0.21f, 0.38f ), p( 0.27f, -0.27f ) };
		draw->AddPolyline( body, 4, color, thickness );
		break;
	}
	case Icon::Server:
		draw->AddRect( p( -0.38f, -0.36f ), p( 0.38f, -0.04f ), color, 0.08f * s, thickness );
		draw->AddRect( p( -0.38f, 0.04f ), p( 0.38f, 0.36f ), color, 0.08f * s, thickness );
		draw->AddCircleFilled( p( 0.22f, -0.20f ), 0.05f * s, color, 10 );
		draw->AddCircleFilled( p( 0.22f, 0.20f ), 0.05f * s, color, 10 );
		break;
	case Icon::Close:
		draw->AddLine( p( -0.28f, -0.28f ), p( 0.28f, 0.28f ), color, thickness );
		draw->AddLine( p( -0.28f, 0.28f ), p( 0.28f, -0.28f ), color, thickness );
		break;
	case Icon::ChevronDown: {
		const ImVec2 points[] = { p( -0.26f, -0.11f ), p( 0.0f, 0.15f ), p( 0.26f, -0.11f ) };
		draw->AddPolyline( points, 3, color, thickness );
		break;
	}
	case Icon::ChevronRight: {
		const ImVec2 points[] = { p( -0.11f, -0.26f ), p( 0.15f, 0.0f ), p( -0.11f, 0.26f ) };
		draw->AddPolyline( points, 3, color, thickness );
		break;
	}
	case Icon::Check: {
		const ImVec2 points[] = { p( -0.32f, 0.02f ), p( -0.09f, 0.25f ), p( 0.33f, -0.22f ) };
		draw->AddPolyline( points, 3, color, thickness );
		break;
	}
	case Icon::Warning: {
		const ImVec2 points[] = { p( 0.0f, -0.38f ), p( 0.42f, 0.34f ), p( -0.42f, 0.34f ) };
		draw->AddPolyline( points, 3, color, thickness, ImDrawFlags_Closed );
		draw->AddLine( p( 0.0f, -0.12f ), p( 0.0f, 0.10f ), color, thickness );
		draw->AddCircleFilled( p( 0.0f, 0.21f ), thickness * 0.65f, color, 8 );
		break;
	}
	case Icon::Info:
		draw->AddCircle( c, 0.40f * s, color, 24, thickness );
		draw->AddLine( p( 0.0f, -0.02f ), p( 0.0f, 0.20f ), color, thickness );
		draw->AddCircleFilled( p( 0.0f, -0.17f ), thickness * 0.65f, color, 8 );
		break;
	case Icon::More:
		for ( float x : { -0.28f, 0.0f, 0.28f } ) draw->AddCircleFilled( p( x, 0.0f ), 0.075f * s, color, 10 );
		break;
	case Icon::Sort: {
		draw->AddLine( p( -0.17f, 0.34f ), p( -0.17f, -0.30f ), color, thickness );
		const ImVec2 up[] = { p( -0.32f, -0.14f ), p( -0.17f, -0.32f ), p( -0.02f, -0.14f ) };
		draw->AddPolyline( up, 3, color, thickness );
		draw->AddLine( p( 0.17f, -0.34f ), p( 0.17f, 0.30f ), color, thickness );
		const ImVec2 down[] = { p( 0.02f, 0.14f ), p( 0.17f, 0.32f ), p( 0.32f, 0.14f ) };
		draw->AddPolyline( down, 3, color, thickness );
		break;
	}
	case Icon::Flame: {
		draw->PathLineTo( p( 0.0f, -0.44f ) );
		draw->PathBezierCubicCurveTo( p( 0.30f, -0.18f ), p( 0.40f, 0.04f ), p( 0.30f, 0.24f ), 16 );
		draw->PathBezierCubicCurveTo( p( 0.20f, 0.44f ), p( -0.20f, 0.44f ), p( -0.30f, 0.24f ), 16 );
		draw->PathBezierCubicCurveTo( p( -0.38f, 0.06f ), p( -0.26f, -0.08f ), p( -0.12f, -0.16f ), 16 );
		draw->PathBezierCubicCurveTo( p( -0.10f, -0.26f ), p( -0.04f, -0.36f ), p( 0.0f, -0.44f ), 16 );
		draw->PathStroke( color, thickness, ImDrawFlags_Closed );
		break;
	}
	}
}

Typeface Ui::Regular( float size ) const
{
	return { fonts->regular, size, false };
}

Typeface Ui::Medium( float size ) const
{
	return { fonts->medium, size, false };
}

Typeface Ui::Bold( float size ) const
{
	return { fonts->bold, size, fonts->syntheticBold };
}

ImVec2 Ui::Measure( const Typeface &face, const std::string &text ) const
{
	if ( text.empty() ) return ImVec2( 0.0f, face.size * scale );
	ImVec2 size = face.font->CalcTextSizeA( face.size * scale, FLT_MAX, 0.0f, text.c_str(), text.c_str() + text.size() );
	if ( face.synthetic ) size.x += 0.6f * scale;
	size.x = std::ceil( size.x );
	return size;
}

void Ui::Text( ImDrawList *draw, const Typeface &face, ImVec2 position, ImU32 color, const std::string &text ) const
{
	if ( text.empty() ) return;
	// Whole pixels keep glyph edges crisp under linear filtering.
	position.x = std::round( position.x );
	position.y = std::round( position.y );
	draw->AddText( face.font, face.size * scale, position, color, text.c_str(), text.c_str() + text.size() );
	if ( face.synthetic ) {
		draw->AddText( face.font, face.size * scale, ImVec2( position.x + 0.6f * scale, position.y ), color,
			text.c_str(), text.c_str() + text.size() );
	}
}

std::string Ui::FitEnd( const Typeface &face, const std::string &text, float width ) const
{
	if ( Measure( face, text ).x <= width ) return text;
	const std::string ellipsis = "\xe2\x80\xa6";
	std::size_t low = 0, high = text.size();
	while ( low < high ) {
		std::size_t middle = ( low + high + 1 ) / 2;
		while ( middle > 0 && middle < text.size() && Continuation( text[middle] ) ) --middle;
		if ( middle <= low ) {
			high = low;
			break;
		}
		if ( Measure( face, text.substr( 0, middle ) + ellipsis ).x <= width ) low = middle;
		else high = middle - 1;
	}
	while ( low > 0 && low < text.size() && Continuation( text[low] ) ) --low;
	std::string fitted = text.substr( 0, low );
	while ( !fitted.empty() && fitted.back() == ' ' ) fitted.pop_back();
	return fitted + ellipsis;
}

std::string Ui::FitPath( const Typeface &face, const std::string &path, float width ) const
{
	if ( Measure( face, path ).x <= width ) return path;
	const char separator = path.find( '\\' ) != std::string::npos ? '\\' : '/';
	std::vector<std::string> parts;
	std::string current;
	for ( const char c : path ) {
		if ( c == '\\' || c == '/' ) {
			parts.push_back( current );
			current.clear();
		} else {
			current += c;
		}
	}
	parts.push_back( current );
	// The root: everything through the first named component ("C:\", "/home/",
	// "\\server\").
	std::size_t first = 0;
	while ( first < parts.size() && parts[first].empty() ) ++first;
	if ( first + 2 >= parts.size() ) return FitEnd( face, path, width );
	std::string head;
	for ( std::size_t i = 0; i <= first; ++i ) head += parts[i] + separator;
	const std::string ellipsis = "\xe2\x80\xa6";
	for ( std::size_t keep = parts.size() - first - 2; keep >= 1; --keep ) {
		std::string tail;
		for ( std::size_t i = parts.size() - keep; i < parts.size(); ++i ) {
			if ( !tail.empty() ) tail += separator;
			tail += parts[i];
		}
		const std::string candidate = head + ellipsis + separator + tail;
		if ( Measure( face, candidate ).x <= width ) return candidate;
		if ( keep == 1 ) break;
	}
	return FitEnd( face, ellipsis + separator + parts.back(), width );
}

bool Ui::Approach( float &value, float target, float speed ) const
{
	const float step = 1.0f - std::exp( -speed * deltaTime );
	value += ( target - value ) * step;
	if ( std::fabs( target - value ) < 0.002f ) value = target;
	return value != target;
}

float Ui::Animate( ImGuiID id, float target, float speed, bool *moving )
{
	ImGuiStorage *storage = ImGui::GetStateStorage();
	float *value = storage->GetFloatRef( id, 0.0f );
	const bool stillMoving = Approach( *value, target, speed );
	if ( stillMoving ) animating = true;
	if ( moving ) *moving = stillMoving;
	return *value;
}

float Ui::PillWidth( const std::string &text, float height, bool dot ) const
{
	const float padding = height * 0.48f;
	const float dotSpace = dot ? height * 0.42f : 0.0f;
	return std::round( padding * 2.0f + dotSpace + Measure( Medium( 11.5f ), text ).x );
}

float Ui::Pill( ImDrawList *draw, ImVec2 position, const std::string &text, ImU32 foreground, ImU32 background,
	ImU32 border, float height, bool dot ) const
{
	const float width = PillWidth( text, height, dot );
	const ImVec2 max( position.x + width, position.y + height );
	draw->AddRectFilled( position, max, background, height * 0.5f );
	if ( ( border >> IM_COL32_A_SHIFT ) & 0xff ) draw->AddRect( position, max, border, height * 0.5f, S( 1.0f ) );
	const Typeface face = Medium( 11.5f );
	const ImVec2 textSize = Measure( face, text );
	float x = position.x + height * 0.48f;
	if ( dot ) {
		draw->AddCircleFilled( ImVec2( x + height * 0.12f, position.y + height * 0.5f ), height * 0.15f, foreground, 12 );
		x += height * 0.42f;
	}
	Text( draw, face, ImVec2( x, position.y + ( height - textSize.y ) * 0.5f ), foreground, text );
	return width;
}

float Ui::ButtonWidth( const std::string &label, float height, bool showIcon ) const
{
	const float padding = height * 0.52f;
	const float icon = showIcon ? height * 0.40f + ( label.empty() ? 0.0f : height * 0.22f ) : 0.0f;
	return std::round( padding * 2.0f + icon + Measure( Medium( 14.0f ), label ).x );
}

bool Ui::Button( const char *id, const std::string &label, ButtonKind kind, float height, float width, bool enabled,
	Icon icon, bool showIcon, bool *hoveredOut )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	if ( width <= 0.0f ) width = ButtonWidth( label, height, showIcon );
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const ImVec2 max( min.x + width, min.y + height );
	const bool clicked = ImGui::InvisibleButton( id, ImVec2( width, height ) ) && enabled;
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive() && enabled;
	if ( hoveredOut ) *hoveredOut = hovered;
	if ( hovered && enabled ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
	const float hover = Animate( ImGui::GetItemID(), hovered && enabled ? 1.0f : 0.0f, 16.0f, nullptr );
	const float rounding = kind == ButtonKind::Primary ? height * 0.5f : std::min( height * 0.5f, S( 9.0f ) );
	ImU32 text = palette.text;
	switch ( kind ) {
	case ButtonKind::Primary: {
		if ( !enabled ) {
			draw->AddRectFilled( min, max, palette.ghostHover, rounding );
			draw->AddRect( min, max, palette.cardBorder, rounding, S( 1.0f ) );
			text = palette.textDisabled;
			break;
		}
		if ( hover > 0.01f ) Shadow( draw, min, max, rounding, S( 10.0f ), WithAlpha( palette.accent, 0.45f * hover ) );
		const ImU32 base = held ? palette.accentActive : Mix( palette.accent, palette.accentHover, hover );
		const int first = draw->VtxBuffer.Size;
		draw->AddRectFilled( min, max, base, rounding );
		ImGui::ShadeVertsLinearColorGradientKeepAlpha( draw, first, draw->VtxBuffer.Size, min, ImVec2( min.x, max.y ),
			Mix( base, IM_COL32( 255, 255, 255, 255 ), held ? 0.02f : 0.14f ), Mix( base, IM_COL32( 0, 0, 0, 255 ), 0.06f ) );
		draw->AddRect( min, max, WithAlpha( IM_COL32( 255, 255, 255, 255 ), 0.10f ), rounding, S( 1.0f ) );
		text = palette.onAccent;
		break;
	}
	case ButtonKind::Secondary:
		draw->AddRectFilled( min, max, Mix( palette.field, palette.fieldHover, hover ), rounding );
		draw->AddRect( min, max, Mix( palette.fieldBorder, palette.cardBorderHover, hover ), rounding, S( 1.0f ) );
		if ( held ) draw->AddRectFilled( min, max, palette.ghostHover, rounding );
		text = enabled ? palette.text : palette.textDisabled;
		break;
	case ButtonKind::Ghost:
		if ( hover > 0.0f ) draw->AddRectFilled( min, max, WithAlpha( palette.ghostHover, hover ), rounding );
		if ( held ) draw->AddRectFilled( min, max, palette.ghostHover, rounding );
		text = enabled ? Mix( palette.textSecondary, palette.text, hover ) : palette.textDisabled;
		break;
	case ButtonKind::Subtle:
		draw->AddRectFilled( min, max, Mix( palette.ghostHover, palette.ghostActive, hover ), rounding );
		if ( held ) draw->AddRectFilled( min, max, palette.ghostHover, rounding );
		text = enabled ? palette.text : palette.textDisabled;
		break;
	case ButtonKind::Danger:
		if ( hover > 0.0f ) draw->AddRectFilled( min, max, WithAlpha( palette.danger, 0.12f * hover ), rounding );
		draw->AddRect( min, max, WithAlpha( palette.danger, 0.45f ), rounding, S( 1.0f ) );
		text = enabled ? palette.danger : palette.textDisabled;
		break;
	}
	const Typeface face = Medium( 14.0f );
	const ImVec2 textSize = Measure( face, label );
	const float iconSize = height * 0.40f;
	const float gap = label.empty() ? 0.0f : height * 0.22f;
	const float content = ( showIcon ? iconSize + gap : 0.0f ) + textSize.x;
	float x = min.x + ( width - content ) * 0.5f;
	if ( showIcon ) {
		const ImVec2 iconCenter( x + iconSize * 0.5f, min.y + height * 0.5f );
		if ( spinNextIcon ) {
			Spinner( draw, iconCenter, iconSize * 0.42f, text, S( 1.7f ) );
			animating = true;
		} else {
			DrawIcon( draw, icon, iconCenter, iconSize, text, S( 1.6f ) );
		}
		x += iconSize + gap;
	}
	spinNextIcon = false;
	Text( draw, face, ImVec2( x, min.y + ( height - textSize.y ) * 0.5f ), text, label );
	return clicked;
}

bool Ui::IconButton( const char *id, Icon icon, float diameter, const char *tooltip, bool enabled )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const bool clicked = ImGui::InvisibleButton( id, ImVec2( diameter, diameter ) ) && enabled;
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	if ( hovered && enabled ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
	const float hover = Animate( ImGui::GetItemID(), hovered && enabled ? 1.0f : 0.0f, 16.0f, nullptr );
	const ImVec2 center( min.x + diameter * 0.5f, min.y + diameter * 0.5f );
	if ( hover > 0.0f ) draw->AddCircleFilled( center, diameter * 0.5f, WithAlpha( palette.ghostActive, hover ), 32 );
	if ( held && enabled ) draw->AddCircleFilled( center, diameter * 0.5f, palette.ghostHover, 32 );
	const ImU32 color = enabled ? Mix( palette.textSecondary, palette.text, hover ) : palette.textDisabled;
	DrawIcon( draw, icon, center, diameter * 0.46f, color, S( 1.6f ) );
	if ( tooltip && ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) Tooltip( tooltip );
	return clicked;
}

float Ui::SegmentedWidth( const std::vector<std::string> &labels, float height ) const
{
	float width = S( 3.0f ) * 2.0f;
	for ( const std::string &label : labels ) width += Measure( Medium( 12.5f ), label ).x + height * 0.9f;
	return std::round( width );
}

bool Ui::Segmented( const char *id, const std::vector<std::string> &labels, int &selected, float height,
	const std::vector<bool> *enabled, const std::vector<std::string> *tooltips )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float inset = S( 3.0f );
	const float width = SegmentedWidth( labels, height );
	const ImVec2 max( origin.x + width, origin.y + height );
	draw->AddRectFilled( origin, max, palette.field, height * 0.5f );
	draw->AddRect( origin, max, palette.fieldBorder, height * 0.5f, S( 1.0f ) );
	ImGui::PushID( id );
	std::vector<float> starts, widths;
	float x = origin.x + inset;
	const Typeface face = Medium( 12.5f );
	for ( const std::string &label : labels ) {
		starts.push_back( x );
		widths.push_back( Measure( face, label ).x + height * 0.9f );
		x += widths.back();
	}
	// The selection slides between segments.
	const ImGuiID slideId = ImGui::GetID( "slide" );
	ImGuiStorage *storage = ImGui::GetStateStorage();
	const int current = std::clamp( selected, 0, int( labels.size() ) - 1 );
	float *slideX = storage->GetFloatRef( slideId, starts.empty() ? 0.0f : starts[std::size_t( current )] - origin.x );
	float *slideW = storage->GetFloatRef( ImGui::GetID( "slideWidth" ), widths.empty() ? 0.0f : widths[std::size_t( current )] );
	if ( !starts.empty() ) {
		if ( Approach( *slideX, starts[std::size_t( current )] - origin.x, 20.0f ) ) animating = true;
		if ( Approach( *slideW, widths[std::size_t( current )], 20.0f ) ) animating = true;
		const ImVec2 knobMin( origin.x + *slideX, origin.y + inset );
		const ImVec2 knobMax( knobMin.x + *slideW, max.y - inset );
		const float knobRounding = ( height - inset * 2.0f ) * 0.5f;
		if ( !palette.dark ) Shadow( draw, knobMin, knobMax, knobRounding, S( 3.0f ), palette.shadow );
		draw->AddRectFilled( knobMin, knobMax, palette.dark ? palette.cardBorderHover : palette.card, knobRounding );
	}
	bool changed = false;
	for ( std::size_t i = 0; i < labels.size(); ++i ) {
		const bool usable = !enabled || i >= enabled->size() || ( *enabled )[i];
		ImGui::SetCursorScreenPos( ImVec2( starts[i], origin.y ) );
		ImGui::PushID( int( i ) );
		const bool clicked = ImGui::InvisibleButton( "segment", ImVec2( widths[i], height ) );
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if ( hovered && usable && int( i ) != selected ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
		if ( clicked && usable && int( i ) != selected ) {
			selected = int( i );
			changed = true;
		}
		if ( tooltips && i < tooltips->size() && !( *tooltips )[i].empty()
			&& ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) {
			Tooltip( ( *tooltips )[i] );
		}
		const ImVec2 textSize = Measure( face, labels[i] );
		const ImU32 color = !usable ? palette.textDisabled
			: int( i ) == current ? palette.text : hovered ? palette.text : palette.textSecondary;
		Text( draw, face, ImVec2( starts[i] + ( widths[i] - textSize.x ) * 0.5f, origin.y + ( height - textSize.y ) * 0.5f ),
			color, labels[i] );
	}
	ImGui::PopID();
	ImGui::SetCursorScreenPos( ImVec2( max.x, origin.y ) );
	ImGui::Dummy( ImVec2( 0.0f, height ) );
	return changed;
}

bool Ui::Toggle( const char *id, bool &value )
{
	ImDrawList *draw = ImGui::GetWindowDrawList();
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const ImVec2 size( S( 38.0f ), S( 22.0f ) );
	const bool clicked = ImGui::InvisibleButton( id, size );
	if ( clicked ) value = !value;
	const bool hovered = ImGui::IsItemHovered();
	if ( hovered ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
	const float on = Animate( ImGui::GetItemID(), value ? 1.0f : 0.0f, 18.0f, nullptr );
	const ImVec2 max( min.x + size.x, min.y + size.y );
	const ImU32 off = palette.dark ? palette.cardBorderHover : palette.fieldBorder;
	draw->AddRectFilled( min, max, Mix( off, palette.accent, on ), size.y * 0.5f );
	const float radius = size.y * 0.5f - S( 3.0f );
	const float x = min.x + size.y * 0.5f + ( size.x - size.y ) * on;
	draw->AddCircleFilled( ImVec2( x, min.y + size.y * 0.5f ), radius, IM_COL32( 255, 255, 255, 255 ), 24 );
	return clicked;
}

void Ui::Spinner( ImDrawList *draw, ImVec2 center, float radius, ImU32 color, float thickness ) const
{
	const float start = time * 5.2f;
	const float sweep = 3.6f + std::sin( time * 2.6f ) * 0.9f;
	draw->PathArcTo( center, radius, start, start + sweep, 24 );
	draw->PathStroke( color, thickness );
}

void Ui::Shadow( ImDrawList *draw, ImVec2 min, ImVec2 max, float rounding, float spread, ImU32 color ) const
{
	// Stacked rings, each adding a little, so the sum fades smoothly outward
	// and falls a touch below the shape like light from above.
	const int layers = 8;
	const float alpha = float( ( color >> IM_COL32_A_SHIFT ) & 0xff ) / 255.0f;
	const float drop = spread * 0.30f;
	for ( int i = 0; i < layers; ++i ) {
		const float t = ( float( i ) + 1.0f ) / float( layers );
		const float grow = spread * t;
		const float layerAlpha = alpha * 0.22f * ( 1.0f - t * 0.85f );
		draw->AddRectFilled( ImVec2( min.x - grow * 0.5f, min.y - grow * 0.25f + drop ),
			ImVec2( max.x + grow * 0.5f, max.y + grow * 0.75f + drop ), WithAlpha( color | IM_COL32_A_MASK, layerAlpha ),
			rounding + grow * 0.5f );
	}
}

void Ui::Glow( ImDrawList *draw, ImVec2 center, float radius, ImU32 color ) const
{
	// A radial gradient as one mesh: rings whose alpha falls off smoothly
	// (quadratically) from the centre, so no bands show.
	const int segments = 72;
	const int rings = 10;
	const float alpha = float( ( color >> IM_COL32_A_SHIFT ) & 0xff ) / 255.0f;
	const ImU32 opaque = color | IM_COL32_A_MASK;
	const ImVec2 uv = draw->_Data->TexUvWhitePixel;
	draw->PrimReserve( segments * rings * 6, 1 + segments * rings );
	const unsigned first = draw->_VtxCurrentIdx;
	draw->PrimWriteVtx( center, uv, WithAlpha( opaque, alpha ) );
	for ( int ring = 1; ring <= rings; ++ring ) {
		const float t = float( ring ) / float( rings );
		const float falloff = ( 1.0f - t ) * ( 1.0f - t );
		const ImU32 ringColor = WithAlpha( opaque, alpha * falloff );
		for ( int i = 0; i < segments; ++i ) {
			const float angle = float( i ) / float( segments ) * 2.0f * kPi;
			draw->PrimWriteVtx( ImVec2( center.x + std::cos( angle ) * radius * t, center.y + std::sin( angle ) * radius * t ),
				uv, ringColor );
		}
	}
	for ( int i = 0; i < segments; ++i ) {
		const int next = ( i + 1 ) % segments;
		draw->PrimWriteIdx( ImDrawIdx( first ) );
		draw->PrimWriteIdx( ImDrawIdx( first + 1 + unsigned( i ) ) );
		draw->PrimWriteIdx( ImDrawIdx( first + 1 + unsigned( next ) ) );
	}
	for ( int ring = 1; ring < rings; ++ring ) {
		const unsigned inner = first + 1 + unsigned( ( ring - 1 ) * segments );
		const unsigned outer = inner + unsigned( segments );
		for ( int i = 0; i < segments; ++i ) {
			const unsigned next = unsigned( ( i + 1 ) % segments );
			draw->PrimWriteIdx( ImDrawIdx( inner + unsigned( i ) ) );
			draw->PrimWriteIdx( ImDrawIdx( outer + unsigned( i ) ) );
			draw->PrimWriteIdx( ImDrawIdx( outer + next ) );
			draw->PrimWriteIdx( ImDrawIdx( inner + unsigned( i ) ) );
			draw->PrimWriteIdx( ImDrawIdx( outer + next ) );
			draw->PrimWriteIdx( ImDrawIdx( inner + next ) );
		}
	}
	// PrimReserve asked for one fan and rings - 1 bands; give back the rest.
	draw->PrimUnreserve( segments * 6 - segments * 3, 0 );
}

void Ui::Tooltip( const std::string &text, float wrap ) const
{
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, S( 10.0f, 8.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_PopupRounding, S( 8.0f ) );
	if ( ImGui::BeginTooltip() ) {
		ImGui::PushTextWrapPos( S( wrap ) );
		ImGui::PushFont( fonts->regular, 13.0f );
		ImGui::TextUnformatted( text.c_str(), text.c_str() + text.size() );
		ImGui::PopFont();
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
	ImGui::PopStyleVar( 2 );
}

} // namespace fntech3::launcher::gui
