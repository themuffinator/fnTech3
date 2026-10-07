/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
#include "launcher_imgui_sdl.hpp"

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace fntech3::launcher::gui {

namespace {

struct Platform {
	SDL_Window *window = nullptr;
	Uint64 lastTicks = 0;
	SDL_Cursor *cursors[ImGuiMouseCursor_COUNT] = {};
	ImGuiMouseCursor shownCursor = ImGuiMouseCursor_COUNT;
	bool cursorHidden = false;
	std::string clipboard;
	bool textInput = false;
	int buttonsDown = 0;
	bool captured = false;
};

struct Renderer {
	SDL_Renderer *renderer = nullptr;
	std::vector<float> positions;
	std::vector<SDL_FColor> colors;
};

Platform platform;
Renderer renderer;

const char *GetClipboard( ImGuiContext * )
{
	char *text = SDL_GetClipboardText();
	platform.clipboard = text ? text : "";
	SDL_free( text );
	return platform.clipboard.c_str();
}

void SetClipboard( ImGuiContext *, const char *text )
{
	SDL_SetClipboardText( text ? text : "" );
}

bool OpenInShell( ImGuiContext *, const char *path )
{
	return path && SDL_OpenURL( path );
}

void SetImeData( ImGuiContext *, ImGuiViewport *, ImGuiPlatformImeData *data )
{
	if ( !platform.window ) return;
	const bool wanted = data->WantVisible || data->WantTextInput;
	if ( wanted ) {
		SDL_Rect area;
		area.x = int( data->InputPos.x );
		area.y = int( data->InputPos.y );
		area.w = 1;
		area.h = int( data->InputLineHeight );
		SDL_SetTextInputArea( platform.window, &area, 0 );
		if ( !platform.textInput ) {
			SDL_StartTextInput( platform.window );
			platform.textInput = true;
		}
	} else if ( platform.textInput ) {
		SDL_StopTextInput( platform.window );
		platform.textInput = false;
	}
}

ImGuiKey KeypadKey( SDL_Scancode scancode )
{
	switch ( scancode ) {
	case SDL_SCANCODE_KP_0: return ImGuiKey_Keypad0;
	case SDL_SCANCODE_KP_1: return ImGuiKey_Keypad1;
	case SDL_SCANCODE_KP_2: return ImGuiKey_Keypad2;
	case SDL_SCANCODE_KP_3: return ImGuiKey_Keypad3;
	case SDL_SCANCODE_KP_4: return ImGuiKey_Keypad4;
	case SDL_SCANCODE_KP_5: return ImGuiKey_Keypad5;
	case SDL_SCANCODE_KP_6: return ImGuiKey_Keypad6;
	case SDL_SCANCODE_KP_7: return ImGuiKey_Keypad7;
	case SDL_SCANCODE_KP_8: return ImGuiKey_Keypad8;
	case SDL_SCANCODE_KP_9: return ImGuiKey_Keypad9;
	case SDL_SCANCODE_KP_PERIOD: return ImGuiKey_KeypadDecimal;
	case SDL_SCANCODE_KP_DIVIDE: return ImGuiKey_KeypadDivide;
	case SDL_SCANCODE_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
	case SDL_SCANCODE_KP_MINUS: return ImGuiKey_KeypadSubtract;
	case SDL_SCANCODE_KP_PLUS: return ImGuiKey_KeypadAdd;
	case SDL_SCANCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
	case SDL_SCANCODE_KP_EQUALS: return ImGuiKey_KeypadEqual;
	default: return ImGuiKey_None;
	}
}

ImGuiKey MapKey( SDL_Keycode key, SDL_Scancode scancode )
{
	const ImGuiKey keypad = KeypadKey( scancode );
	if ( keypad != ImGuiKey_None ) return keypad;
	if ( key >= SDLK_A && key <= SDLK_Z ) return ImGuiKey( ImGuiKey_A + int( key - SDLK_A ) );
	if ( key >= SDLK_0 && key <= SDLK_9 ) return ImGuiKey( ImGuiKey_0 + int( key - SDLK_0 ) );
	if ( key >= SDLK_F1 && key <= SDLK_F12 ) return ImGuiKey( ImGuiKey_F1 + int( key - SDLK_F1 ) );
	switch ( key ) {
	case SDLK_TAB: return ImGuiKey_Tab;
	case SDLK_LEFT: return ImGuiKey_LeftArrow;
	case SDLK_RIGHT: return ImGuiKey_RightArrow;
	case SDLK_UP: return ImGuiKey_UpArrow;
	case SDLK_DOWN: return ImGuiKey_DownArrow;
	case SDLK_PAGEUP: return ImGuiKey_PageUp;
	case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
	case SDLK_HOME: return ImGuiKey_Home;
	case SDLK_END: return ImGuiKey_End;
	case SDLK_INSERT: return ImGuiKey_Insert;
	case SDLK_DELETE: return ImGuiKey_Delete;
	case SDLK_BACKSPACE: return ImGuiKey_Backspace;
	case SDLK_SPACE: return ImGuiKey_Space;
	case SDLK_RETURN: return ImGuiKey_Enter;
	case SDLK_ESCAPE: return ImGuiKey_Escape;
	case SDLK_APOSTROPHE: return ImGuiKey_Apostrophe;
	case SDLK_COMMA: return ImGuiKey_Comma;
	case SDLK_MINUS: return ImGuiKey_Minus;
	case SDLK_PERIOD: return ImGuiKey_Period;
	case SDLK_SLASH: return ImGuiKey_Slash;
	case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
	case SDLK_EQUALS: return ImGuiKey_Equal;
	case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
	case SDLK_BACKSLASH: return ImGuiKey_Backslash;
	case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
	case SDLK_GRAVE: return ImGuiKey_GraveAccent;
	case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
	case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
	case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
	case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
	case SDLK_PAUSE: return ImGuiKey_Pause;
	case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
	case SDLK_LSHIFT: return ImGuiKey_LeftShift;
	case SDLK_LALT: return ImGuiKey_LeftAlt;
	case SDLK_LGUI: return ImGuiKey_LeftSuper;
	case SDLK_RCTRL: return ImGuiKey_RightCtrl;
	case SDLK_RSHIFT: return ImGuiKey_RightShift;
	case SDLK_RALT: return ImGuiKey_RightAlt;
	case SDLK_RGUI: return ImGuiKey_RightSuper;
	case SDLK_APPLICATION: return ImGuiKey_Menu;
	default: return ImGuiKey_None;
	}
}

void UpdateModifiers( SDL_Keymod modifiers )
{
	ImGuiIO &io = ImGui::GetIO();
	io.AddKeyEvent( ImGuiMod_Ctrl, ( modifiers & SDL_KMOD_CTRL ) != 0 );
	io.AddKeyEvent( ImGuiMod_Shift, ( modifiers & SDL_KMOD_SHIFT ) != 0 );
	io.AddKeyEvent( ImGuiMod_Alt, ( modifiers & SDL_KMOD_ALT ) != 0 );
	io.AddKeyEvent( ImGuiMod_Super, ( modifiers & SDL_KMOD_GUI ) != 0 );
}

// Drawing code asks for the default state with this callback; there is no
// state beyond the viewport and clip rectangle to put back.
void ResetRenderState( const ImDrawList *, const ImDrawCmd * )
{
}

void UpdateTexture( ImTextureData *texture )
{
	if ( texture->Status == ImTextureStatus_WantCreate ) {
		// The atlas is RGBA unless an application asks otherwise; this one does not.
		SDL_Texture *created = SDL_CreateTexture( renderer.renderer, SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STATIC, texture->Width, texture->Height );
		if ( !created ) return;
		SDL_UpdateTexture( created, nullptr, texture->GetPixels(), texture->GetPitch() );
		SDL_SetTextureBlendMode( created, SDL_BLENDMODE_BLEND );
		SDL_SetTextureScaleMode( created, SDL_SCALEMODE_LINEAR );
		texture->SetTexID( TextureId( created ) );
		texture->BackendUserData = created;
		texture->SetStatus( ImTextureStatus_OK );
	} else if ( texture->Status == ImTextureStatus_WantUpdates ) {
		SDL_Texture *existing = static_cast<SDL_Texture *>( texture->BackendUserData );
		if ( existing ) {
			for ( const ImTextureRect &update : texture->Updates ) {
				const SDL_Rect area = { update.x, update.y, update.w, update.h };
				SDL_UpdateTexture( existing, &area, texture->GetPixelsAt( update.x, update.y ), texture->GetPitch() );
			}
		}
		texture->SetStatus( ImTextureStatus_OK );
	} else if ( texture->Status == ImTextureStatus_WantDestroy ) {
		if ( SDL_Texture *existing = static_cast<SDL_Texture *>( texture->BackendUserData ) ) SDL_DestroyTexture( existing );
		texture->BackendUserData = nullptr;
		texture->SetTexID( ImTextureID_Invalid );
		texture->SetStatus( ImTextureStatus_Destroyed );
	}
}

} // namespace

bool PlatformInit( SDL_Window *window )
{
	ImGuiIO &io = ImGui::GetIO();
	io.BackendPlatformName = "fntech3_launcher_sdl3";
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	platform = Platform();
	platform.window = window;
	ImGuiPlatformIO &platformIo = ImGui::GetPlatformIO();
	platformIo.Platform_GetClipboardTextFn = GetClipboard;
	platformIo.Platform_SetClipboardTextFn = SetClipboard;
	platformIo.Platform_OpenInShellFn = OpenInShell;
	platformIo.Platform_SetImeDataFn = SetImeData;
	const SDL_SystemCursor shapes[ImGuiMouseCursor_COUNT] = {
		SDL_SYSTEM_CURSOR_DEFAULT,     // Arrow
		SDL_SYSTEM_CURSOR_TEXT,        // TextInput
		SDL_SYSTEM_CURSOR_MOVE,        // ResizeAll
		SDL_SYSTEM_CURSOR_NS_RESIZE,   // ResizeNS
		SDL_SYSTEM_CURSOR_EW_RESIZE,   // ResizeEW
		SDL_SYSTEM_CURSOR_NESW_RESIZE, // ResizeNESW
		SDL_SYSTEM_CURSOR_NWSE_RESIZE, // ResizeNWSE
		SDL_SYSTEM_CURSOR_POINTER,     // Hand
		SDL_SYSTEM_CURSOR_WAIT,        // Wait
		SDL_SYSTEM_CURSOR_PROGRESS,    // Progress
		SDL_SYSTEM_CURSOR_NOT_ALLOWED  // NotAllowed
	};
	for ( int i = 0; i < ImGuiMouseCursor_COUNT; ++i ) platform.cursors[i] = SDL_CreateSystemCursor( shapes[i] );
	platform.lastTicks = SDL_GetTicksNS();
	return true;
}

void PlatformShutdown()
{
	if ( platform.captured ) SDL_CaptureMouse( false );
	if ( platform.textInput && platform.window ) SDL_StopTextInput( platform.window );
	for ( SDL_Cursor *&cursor : platform.cursors ) {
		if ( cursor ) SDL_DestroyCursor( cursor );
		cursor = nullptr;
	}
	ImGuiPlatformIO &platformIo = ImGui::GetPlatformIO();
	platformIo.Platform_GetClipboardTextFn = nullptr;
	platformIo.Platform_SetClipboardTextFn = nullptr;
	platformIo.Platform_OpenInShellFn = nullptr;
	platformIo.Platform_SetImeDataFn = nullptr;
	ImGuiIO &io = ImGui::GetIO();
	io.BackendPlatformName = nullptr;
	io.BackendFlags &= ~ImGuiBackendFlags_HasMouseCursors;
	platform = Platform();
}

bool PlatformProcessEvent( const SDL_Event &event )
{
	ImGuiIO &io = ImGui::GetIO();
	switch ( event.type ) {
	case SDL_EVENT_MOUSE_MOTION:
		io.AddMouseSourceEvent( event.motion.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse );
		io.AddMousePosEvent( event.motion.x, event.motion.y );
		return true;
	case SDL_EVENT_MOUSE_WHEEL:
		io.AddMouseSourceEvent( event.wheel.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse );
		io.AddMouseWheelEvent( -event.wheel.x, event.wheel.y );
		return true;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		int button = -1;
		switch ( event.button.button ) {
		case SDL_BUTTON_LEFT: button = 0; break;
		case SDL_BUTTON_RIGHT: button = 1; break;
		case SDL_BUTTON_MIDDLE: button = 2; break;
		case SDL_BUTTON_X1: button = 3; break;
		case SDL_BUTTON_X2: button = 4; break;
		default: break;
		}
		if ( button < 0 ) return false;
		const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		io.AddMouseSourceEvent( event.button.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse );
		io.AddMouseButtonEvent( button, down );
		platform.buttonsDown = down ? ( platform.buttonsDown | ( 1 << button ) ) : ( platform.buttonsDown & ~( 1 << button ) );
		return true;
	}
	case SDL_EVENT_TEXT_INPUT:
		io.AddInputCharactersUTF8( event.text.text );
		return true;
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		UpdateModifiers( event.key.mod );
		const ImGuiKey key = MapKey( event.key.key, event.key.scancode );
		io.AddKeyEvent( key, event.type == SDL_EVENT_KEY_DOWN );
		io.SetKeyEventNativeData( key, int( event.key.key ), int( event.key.scancode ), int( event.key.scancode ) );
		return true;
	}
	case SDL_EVENT_WINDOW_MOUSE_LEAVE:
		io.AddMousePosEvent( -FLT_MAX, -FLT_MAX );
		return true;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		io.AddFocusEvent( event.type == SDL_EVENT_WINDOW_FOCUS_GAINED );
		return true;
	case SDL_EVENT_WINDOW_RESIZED:
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
	case SDL_EVENT_WINDOW_EXPOSED:
	case SDL_EVENT_WINDOW_RESTORED:
	case SDL_EVENT_WINDOW_MAXIMIZED:
	case SDL_EVENT_WINDOW_SHOWN:
		return true;
	default:
		return false;
	}
}

void PlatformNewFrame()
{
	ImGuiIO &io = ImGui::GetIO();
	int width = 0, height = 0, pixelWidth = 0, pixelHeight = 0;
	SDL_GetWindowSize( platform.window, &width, &height );
	if ( SDL_GetWindowFlags( platform.window ) & SDL_WINDOW_MINIMIZED ) width = height = 0;
	SDL_GetWindowSizeInPixels( platform.window, &pixelWidth, &pixelHeight );
	io.DisplaySize = ImVec2( float( width ), float( height ) );
	if ( width > 0 && height > 0 ) {
		io.DisplayFramebufferScale = ImVec2( float( pixelWidth ) / float( width ), float( pixelHeight ) / float( height ) );
	}

	const Uint64 now = SDL_GetTicksNS();
	const double elapsed = double( now - platform.lastTicks ) / 1.0e9;
	io.DeltaTime = elapsed > 0.0 ? float( elapsed < 0.25 ? elapsed : 0.25 ) : 1.0f / 1000.0f;
	platform.lastTicks = now;

	// Keep a drag that leaves the window, such as a scrollbar's, attached.
	const bool capture = platform.buttonsDown != 0;
	if ( capture != platform.captured ) {
		SDL_CaptureMouse( capture );
		platform.captured = capture;
	}

	if ( !( io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange ) ) {
		const ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
		if ( io.MouseDrawCursor || cursor == ImGuiMouseCursor_None ) {
			if ( !platform.cursorHidden ) {
				SDL_HideCursor();
				platform.cursorHidden = true;
			}
		} else {
			if ( cursor != platform.shownCursor ) {
				SDL_Cursor *shape = platform.cursors[cursor] ? platform.cursors[cursor] : platform.cursors[ImGuiMouseCursor_Arrow];
				if ( shape ) SDL_SetCursor( shape );
				platform.shownCursor = cursor;
			}
			if ( platform.cursorHidden ) {
				SDL_ShowCursor();
				platform.cursorHidden = false;
			}
		}
	}
}

bool RendererInit( SDL_Renderer *target )
{
	ImGuiIO &io = ImGui::GetIO();
	io.BackendRendererName = "fntech3_launcher_sdlrenderer";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
	renderer = Renderer();
	renderer.renderer = target;
	const int maxTexture = int( SDL_GetNumberProperty( SDL_GetRendererProperties( target ),
		SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 2048 ) );
	ImGuiPlatformIO &platformIo = ImGui::GetPlatformIO();
	platformIo.Renderer_TextureMaxWidth = maxTexture;
	platformIo.Renderer_TextureMaxHeight = maxTexture;
	platformIo.DrawCallback_ResetRenderState = ResetRenderState;
	return true;
}

void RendererShutdown()
{
	// Release every texture ImGui still owns, as its backend contract asks.
	for ( ImTextureData *texture : ImGui::GetPlatformIO().Textures ) {
		if ( texture->RefCount == 1 && texture->BackendUserData ) {
			texture->SetStatus( ImTextureStatus_WantDestroy );
			UpdateTexture( texture );
		}
	}
	ImGuiIO &io = ImGui::GetIO();
	io.BackendRendererName = nullptr;
	io.BackendFlags &= ~( ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures );
	renderer = Renderer();
}

void RenderDrawData( ImDrawData *drawData )
{
	if ( !drawData || !renderer.renderer ) return;
	if ( drawData->Textures ) {
		for ( ImTextureData *texture : *drawData->Textures ) {
			if ( texture->Status != ImTextureStatus_OK ) UpdateTexture( texture );
		}
	}
	const ImVec2 scale = drawData->FramebufferScale;
	const float framebufferWidth = drawData->DisplaySize.x * scale.x;
	const float framebufferHeight = drawData->DisplaySize.y * scale.y;
	if ( framebufferWidth <= 0.0f || framebufferHeight <= 0.0f ) return;

	SDL_SetRenderViewport( renderer.renderer, nullptr );
	SDL_SetRenderClipRect( renderer.renderer, nullptr );
	const ImVec2 origin = drawData->DisplayPos;
	for ( const ImDrawList *list : drawData->CmdLists ) {
		const ImDrawVert *vertices = list->VtxBuffer.Data;
		const int vertexCount = list->VtxBuffer.Size;
		// ImGui works in window points; the renderer in pixels. Positions are
		// scaled here so clip rectangles map to exact pixels.
		renderer.positions.resize( std::size_t( vertexCount ) * 2 );
		renderer.colors.resize( std::size_t( vertexCount ) );
		for ( int i = 0; i < vertexCount; ++i ) {
			const ImDrawVert &vertex = vertices[i];
			renderer.positions[std::size_t( i ) * 2] = ( vertex.pos.x - origin.x ) * scale.x;
			renderer.positions[std::size_t( i ) * 2 + 1] = ( vertex.pos.y - origin.y ) * scale.y;
			const ImU32 color = vertex.col;
			SDL_FColor &out = renderer.colors[std::size_t( i )];
			out.r = float( ( color >> IM_COL32_R_SHIFT ) & 0xff ) / 255.0f;
			out.g = float( ( color >> IM_COL32_G_SHIFT ) & 0xff ) / 255.0f;
			out.b = float( ( color >> IM_COL32_B_SHIFT ) & 0xff ) / 255.0f;
			out.a = float( ( color >> IM_COL32_A_SHIFT ) & 0xff ) / 255.0f;
		}
		for ( const ImDrawCmd &command : list->CmdBuffer ) {
			if ( command.UserCallback ) {
				if ( command.UserCallback != ImGui::GetPlatformIO().DrawCallback_ResetRenderState ) {
					command.UserCallback( list, &command );
				}
				SDL_SetRenderViewport( renderer.renderer, nullptr );
				SDL_SetRenderClipRect( renderer.renderer, nullptr );
				continue;
			}
			float left = ( command.ClipRect.x - origin.x ) * scale.x;
			float top = ( command.ClipRect.y - origin.y ) * scale.y;
			float right = ( command.ClipRect.z - origin.x ) * scale.x;
			float bottom = ( command.ClipRect.w - origin.y ) * scale.y;
			if ( left < 0.0f ) left = 0.0f;
			if ( top < 0.0f ) top = 0.0f;
			if ( right > framebufferWidth ) right = framebufferWidth;
			if ( bottom > framebufferHeight ) bottom = framebufferHeight;
			if ( right <= left || bottom <= top ) continue;
			const SDL_Rect clip = { int( std::floor( left ) ), int( std::floor( top ) ),
				int( std::ceil( right ) - std::floor( left ) ), int( std::ceil( bottom ) - std::floor( top ) ) };
			SDL_SetRenderClipRect( renderer.renderer, &clip );
			SDL_Texture *texture = reinterpret_cast<SDL_Texture *>( static_cast<std::uintptr_t>( command.GetTexID() ) );
			const std::size_t offset = command.VtxOffset;
			const float *uv = reinterpret_cast<const float *>(
				reinterpret_cast<const char *>( vertices + offset ) + offsetof( ImDrawVert, uv ) );
			SDL_RenderGeometryRaw( renderer.renderer, texture,
				renderer.positions.data() + offset * 2, int( sizeof( float ) * 2 ),
				renderer.colors.data() + offset, int( sizeof( SDL_FColor ) ),
				uv, int( sizeof( ImDrawVert ) ),
				vertexCount - int( offset ),
				list->IdxBuffer.Data + command.IdxOffset, int( command.ElemCount ), int( sizeof( ImDrawIdx ) ) );
		}
	}
	SDL_SetRenderClipRect( renderer.renderer, nullptr );
}

SDL_Texture *CreateImageTexture( SDL_Renderer *target, const unsigned char *rgba, int width, int height )
{
	if ( !target || !rgba || width <= 0 || height <= 0 ) return nullptr;
	SDL_Texture *texture = SDL_CreateTexture( target, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height );
	if ( !texture ) return nullptr;
	SDL_UpdateTexture( texture, nullptr, rgba, width * 4 );
	SDL_SetTextureBlendMode( texture, SDL_BLENDMODE_BLEND );
	SDL_SetTextureScaleMode( texture, SDL_SCALEMODE_LINEAR );
	return texture;
}

} // namespace fntech3::launcher::gui
