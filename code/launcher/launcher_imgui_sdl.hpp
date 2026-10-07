/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// Dear ImGui on SDL3 for the launcher: SDL events become ImGui input, and
// ImGui's triangles are drawn with SDL_Renderer, which picks Direct3D, Metal,
// Vulkan or OpenGL for the platform. One window, one renderer, no viewports.
// Written for this program against the two libraries' public interfaces.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

#include "imgui.h"

namespace fntech3::launcher::gui {

bool PlatformInit( SDL_Window *window );
void PlatformShutdown();
// Feeds one event to ImGui. True when it may change what is drawn.
bool PlatformProcessEvent( const SDL_Event &event );
// Display size, framebuffer scale, frame time, mouse cursor.
void PlatformNewFrame();

bool RendererInit( SDL_Renderer *renderer );
void RendererShutdown();
// Honours ImGui's texture requests, then draws the frame into the renderer's
// current target.
void RenderDrawData( ImDrawData *drawData );

// A straight-alpha RGBA image as a texture ImGui can draw; nullptr on failure.
SDL_Texture *CreateImageTexture( SDL_Renderer *renderer, const unsigned char *rgba, int width, int height );

inline ImTextureID TextureId( SDL_Texture *texture )
{
	return static_cast<ImTextureID>( reinterpret_cast<std::uintptr_t>( texture ) );
}

} // namespace fntech3::launcher::gui
