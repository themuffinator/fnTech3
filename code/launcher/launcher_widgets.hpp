/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// The launcher's look: two palettes, vector icons drawn with ImGui paths (so
// no icon artwork ships and every size stays sharp), and the few custom
// widgets the list and dialogs use. Sizes are in points before UI scaling.
#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace fntech3::launcher::gui {

struct Fonts;

struct Palette {
	bool dark = true;
	ImU32 background, backgroundRaised, glow, glowCool;
	ImU32 card, cardHover, cardSelected, cardBorder, cardBorderHover;
	ImU32 accent, accentHover, accentActive, accentSoft, onAccent;
	ImU32 text, textSecondary, textFaint, textDisabled;
	ImU32 field, fieldHover, fieldBorder, fieldFocus;
	ImU32 ghostHover, ghostActive, separator, shadow, overlay, popup, popupBorder;
	ImU32 success, warning, danger, info;
	ImU32 iconPlate, skeleton, skeletonShine;
};

Palette DarkPalette();
Palette LightPalette();
// ImGui's own widgets (inputs, popups, combo boxes, scroll bars) in the palette.
void ApplyStyle( const Palette &palette, float scale );

ImU32 Mix( ImU32 a, ImU32 b, float t );
ImU32 WithAlpha( ImU32 color, float alpha );

enum class Icon {
	Play, Plus, Refresh, Settings, Search, Folder, Copy, Command, Edit, Hide, Show,
	Trash, Server, Close, ChevronDown, ChevronRight, Check, Warning, Info, More, Sort, Flame
};
void DrawIcon( ImDrawList *draw, Icon icon, ImVec2 center, float size, ImU32 color, float thickness );

// Text drawn with a font at a size in points, honouring the UI scale.
struct Typeface {
	ImFont *font = nullptr;
	float size = 14.0f;
	bool synthetic = false; // double-struck for a missing bold face
};

class Ui {
public:
	Fonts *fonts = nullptr;
	Palette palette;
	float scale = 1.0f;
	float time = 0.0f;
	float deltaTime = 0.0f;

	float S( float value ) const { return value * scale; }
	ImVec2 S( float x, float y ) const { return ImVec2( x * scale, y * scale ); }

	Typeface Regular( float size ) const;
	Typeface Medium( float size ) const;
	Typeface Bold( float size ) const;

	ImVec2 Measure( const Typeface &face, const std::string &text ) const;
	void Text( ImDrawList *draw, const Typeface &face, ImVec2 position, ImU32 color, const std::string &text ) const;
	// Shortens text from its end with an ellipsis to fit `width`.
	std::string FitEnd( const Typeface &face, const std::string &text, float width ) const;
	// Shortens a path in its middle, keeping its root and its last folders.
	std::string FitPath( const Typeface &face, const std::string &path, float width ) const;

	// Eases a value toward a target, frame-rate independent. Returns true
	// while still moving, so the caller keeps drawing.
	bool Approach( float &value, float target, float speed ) const;
	// A per-id animated value kept in ImGui's state storage.
	float Animate( ImGuiID id, float target, float speed, bool *moving );

	// A rounded pill with text; returns its width.
	float Pill( ImDrawList *draw, ImVec2 position, const std::string &text, ImU32 foreground, ImU32 background,
		ImU32 border, float height, bool dot = false ) const;
	float PillWidth( const std::string &text, float height, bool dot = false ) const;

	enum class ButtonKind { Primary, Secondary, Ghost, Danger, Subtle };
	// A button with an optional icon; `width` 0 sizes it to its label.
	bool Button( const char *id, const std::string &label, ButtonKind kind, float height, float width = 0.0f,
		bool enabled = true, Icon icon = Icon::Play, bool showIcon = false, bool *hoveredOut = nullptr );
	float ButtonWidth( const std::string &label, float height, bool showIcon ) const;
	// A round icon-only button.
	bool IconButton( const char *id, Icon icon, float diameter, const char *tooltip = nullptr, bool enabled = true );
	// A row of mutually exclusive choices. Returns true when `selected` changed.
	bool Segmented( const char *id, const std::vector<std::string> &labels, int &selected, float height,
		const std::vector<bool> *enabled = nullptr, const std::vector<std::string> *tooltips = nullptr );
	float SegmentedWidth( const std::vector<std::string> &labels, float height ) const;
	bool Toggle( const char *id, bool &value );
	void Spinner( ImDrawList *draw, ImVec2 center, float radius, ImU32 color, float thickness ) const;
	void Shadow( ImDrawList *draw, ImVec2 min, ImVec2 max, float rounding, float spread, ImU32 color ) const;
	void Glow( ImDrawList *draw, ImVec2 center, float radius, ImU32 color ) const;
	void Tooltip( const std::string &text, float wrap = 360.0f ) const;

	bool animating = false;     // set by any widget still animating this frame
	bool spinNextIcon = false;  // the next Button shows a spinner in place of its icon
};

} // namespace fntech3::launcher::gui
