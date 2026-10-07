/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// The launcher reads the system's own interface typeface at run time --
// Segoe UI on Windows, the desktop's sans-serif on Linux, San Francisco on
// macOS -- so it ships no font of its own. Dear ImGui's built-in scalable
// font is the fallback when none is found.
#pragma once

#include "imgui.h"

#include <string>

namespace fntech3::launcher::gui {

struct Fonts {
	ImFont *regular = nullptr;
	ImFont *medium = nullptr;  // semibold where the system has it
	ImFont *bold = nullptr;
	// No heavier face was found: titles are drawn twice, a hair apart.
	bool syntheticBold = false;
	bool systemFont = false;
	std::string description; // which files were loaded, for the about text
};

// Call once after ImGui::CreateContext() and before the first frame.
Fonts LoadFonts();

// Adds the system's CJK face behind the regular one, once, when `text` holds
// characters the interface font lacks. Call between frames.
void EnsureFallbackFor( Fonts &fonts, const std::string &text );

} // namespace fntech3::launcher::gui
