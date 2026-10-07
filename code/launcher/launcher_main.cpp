/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// fntech3-launcher: finds the player's id Tech 3 era games and starts fnTech3
// for the one they choose. It never changes a game's files, and the engine
// still makes every compatibility decision for the profile it is given.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "imgui.h"

#include "launcher_app.hpp"
#include "launcher_fonts.hpp"
#include "launcher_fs.hpp"
#include "launcher_imgui_sdl.hpp"

#include "../platform/application_icon_data.h"
#include "../../version/fntech3_version.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

using namespace fntech3::launcher;

struct CommandLine {
	std::string settings;
	std::string screenshot;
	int width = 0;
	int height = 0;
	AppOptions app;
	bool version = false;
	std::string error;
};

bool ParseSize( const char *text, int &width, int &height )
{
	return text && std::sscanf( text, "%dx%d", &width, &height ) == 2 && width >= 320 && height >= 240
		&& width <= 16384 && height <= 16384;
}

CommandLine Parse( int argc, char **argv )
{
	CommandLine line;
	for ( int i = 1; i < argc; ++i ) {
		const std::string option = argv[i];
		const char *value = i + 1 < argc ? argv[i + 1] : nullptr;
		auto take = [&]() {
			++i;
			return value;
		};
		if ( option == "--version" ) {
			line.version = true;
		} else if ( option == "--settings" && value ) {
			line.settings = take();
		} else if ( option == "--screenshot" && value ) {
			line.screenshot = take();
		} else if ( option == "--size" && value ) {
			if ( !ParseSize( take(), line.width, line.height ) ) line.error = "--size expects WIDTHxHEIGHT";
		} else if ( option == "--scale" && value ) {
			line.app.scale = std::strtof( take(), nullptr );
		} else if ( option == "--theme" && value ) {
			const std::string theme = take();
			line.app.theme = theme == "light" ? Theme::Light : theme == "dark" ? Theme::Dark : Theme::System;
		} else if ( option == "--select" && value ) {
			line.app.selectGroup = take();
		} else if ( option == "--preview" && value ) {
			line.app.preview = take();
		} else if ( option == "--search-folder" && value ) {
			line.app.extraSearchFolders.push_back( fs::FromUtf8( take() ) );
		} else if ( option == "--no-system-scan" ) {
			line.app.scanStores = false;
			line.app.scanCommonFolders = false;
#ifdef __APPLE__
		} else if ( option.rfind( "-psn", 0 ) == 0 ) {
			// Finder's process serial number; nothing to read.
#endif
		} else {
			line.error = "Unknown option " + option;
		}
	}
	return line;
}

void ShowError( const std::string &message )
{
	std::fprintf( stderr, "fntech3-launcher: %s\n", message.c_str() );
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "fnTech3 Launcher", message.c_str(), nullptr );
}

// A remembered position is used only while it still lies on a display.
bool OnSomeDisplay( int x, int y, int width )
{
	int count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays( &count );
	bool visible = false;
	for ( int i = 0; displays && i < count; ++i ) {
		SDL_Rect bounds;
		if ( !SDL_GetDisplayUsableBounds( displays[i], &bounds ) ) continue;
		const int left = std::max( x, bounds.x ), right = std::min( x + width, bounds.x + bounds.w );
		if ( right - left >= 120 && y >= bounds.y - 8 && y < bounds.y + bounds.h - 40 ) visible = true;
	}
	SDL_free( displays );
	return visible;
}

void Draw( App &app, SDL_Renderer *renderer, float fixedDelta )
{
	app.BeforeFrame();
	gui::PlatformNewFrame();
	if ( fixedDelta > 0.0f ) ImGui::GetIO().DeltaTime = fixedDelta;
	ImGui::NewFrame();
	app.Frame();
	ImGui::Render();
	SDL_SetRenderDrawColor( renderer, 13, 15, 19, 255 );
	SDL_RenderClear( renderer );
	gui::RenderDrawData( ImGui::GetDrawData() );
}

int Screenshot( App &app, SDL_Window *window, SDL_Renderer *renderer, const std::string &path )
{
	int width = 0, height = 0;
	SDL_GetWindowSizeInPixels( window, &width, &height );
	SDL_Texture *target = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height );
	if ( !target ) {
		ShowError( std::string( "Cannot create the screenshot target: " ) + SDL_GetError() );
		return 1;
	}
	// Ninety frames at a fixed rate let every animation settle.
	for ( int frame = 0; frame < 90; ++frame ) {
		SDL_SetRenderTarget( renderer, target );
		Draw( app, renderer, 1.0f / 60.0f );
	}
	SDL_Surface *surface = SDL_RenderReadPixels( renderer, nullptr );
	SDL_SetRenderTarget( renderer, nullptr );
	SDL_DestroyTexture( target );
	if ( !surface ) {
		ShowError( std::string( "Cannot read the screenshot: " ) + SDL_GetError() );
		return 1;
	}
	bool saved;
#if SDL_VERSION_ATLEAST( 3, 4, 0 )
	const std::string lower = fs::Lower( path );
	saved = lower.size() > 4 && lower.compare( lower.size() - 4, 4, ".bmp" ) == 0 ? SDL_SaveBMP( surface, path.c_str() )
		: SDL_SavePNG( surface, path.c_str() );
#else
	saved = SDL_SaveBMP( surface, path.c_str() );
#endif
	SDL_DestroySurface( surface );
	if ( !saved ) {
		ShowError( "Cannot write " + path + ": " + SDL_GetError() );
		return 1;
	}
	return 0;
}

} // namespace

int main( int argc, char **argv )
{
	CommandLine line = Parse( argc, argv );
	if ( line.version ) {
		std::printf( "fntech3-launcher %s\n", FNTECH3_VERSION_STRING );
		return 0;
	}
	SDL_SetHint( SDL_HINT_APP_ID, "fntech3-launcher" );
	SDL_SetAppMetadata( "fnTech3 Launcher", FNTECH3_VERSION_STRING, "fntech3-launcher" );
	if ( !SDL_Init( SDL_INIT_VIDEO ) ) {
		ShowError( std::string( "The launcher could not open a window: " ) + SDL_GetError() );
		return 1;
	}
	if ( !line.error.empty() ) {
		ShowError( line.error );
		SDL_Quit();
		return 2;
	}
	const bool screenshot = !line.screenshot.empty();

	const std::filesystem::path launcherDirectory = fs::ExecutableDirectory();
	std::filesystem::path preferences;
	if ( char *folder = SDL_GetPrefPath( "fnTech3", "Launcher" ) ) {
		preferences = fs::FromUtf8( folder );
		SDL_free( folder );
	}
	AppOptions options = line.app;
	options.launcherDirectory = launcherDirectory;
	options.settingsFile = line.settings.empty() ? SettingsFile( preferences, launcherDirectory ) : fs::FromUtf8( line.settings );
	if ( screenshot ) {
		options.synchronousScan = true;
		options.readOnlySettings = true;
	}
	Settings settings;
	std::string settingsError;
	const bool settingsLoaded = LoadSettings( options.settingsFile, settings, &settingsError );

	// The window opens at a size that suits the display, or where it was left.
	const float contentScale = std::max( SDL_GetDisplayContentScale( SDL_GetPrimaryDisplay() ), 1.0f );
	int width = line.width ? line.width : settings.window.width;
	int height = line.height ? line.height : settings.window.height;
	if ( width <= 0 || height <= 0 ) {
		width = int( 1060.0f * contentScale );
		height = int( 700.0f * contentScale );
		SDL_Rect usable;
		if ( SDL_GetDisplayUsableBounds( SDL_GetPrimaryDisplay(), &usable ) ) {
			width = std::min( width, usable.w * 9 / 10 );
			height = std::min( height, usable.h * 9 / 10 );
		}
	}
	SDL_Window *window = SDL_CreateWindow( "fnTech3 Launcher", width, height,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN );
	if ( !window ) {
		ShowError( std::string( "The launcher could not open a window: " ) + SDL_GetError() );
		SDL_Quit();
		return 1;
	}
	if ( !screenshot ) SDL_SetWindowMinimumSize( window, int( 600.0f * contentScale ), int( 420.0f * contentScale ) );
	if ( !screenshot && settings.window.x != INT_MIN && settings.window.y != INT_MIN
		&& OnSomeDisplay( settings.window.x, settings.window.y, width ) ) {
		SDL_SetWindowPosition( window, settings.window.x, settings.window.y );
	}
	if ( SDL_Surface *icon = SDL_CreateSurfaceFrom( int( FNTECH3_ICON_SIZE ), int( FNTECH3_ICON_SIZE ), SDL_PIXELFORMAT_RGBA32,
			const_cast<unsigned char *>( FNTECH3_ICON_RGBA ), int( FNTECH3_ICON_SIZE ) * 4 ) ) {
		SDL_SetWindowIcon( window, icon );
		SDL_DestroySurface( icon );
	}
	SDL_Renderer *renderer = SDL_CreateRenderer( window, nullptr );
	if ( !renderer ) {
		ShowError( std::string( "The launcher could not start drawing: " ) + SDL_GetError() );
		SDL_DestroyWindow( window );
		SDL_Quit();
		return 1;
	}
	SDL_SetRenderVSync( renderer, 1 );

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	gui::PlatformInit( window );
	gui::RendererInit( renderer );
	gui::Fonts fonts = gui::LoadFonts();

	int result = 0;
	{
		const bool maximized = settings.window.maximized;
		App app( window, renderer, options, std::move( settings ) );
		app.Initialize( &fonts );
		if ( !settingsLoaded && !settingsError.empty() ) {
			app.Notify( "Your launcher settings could not be read (" + settingsError + "). A copy was kept beside them as .bad.", 2 );
		}
		if ( screenshot ) {
			result = Screenshot( app, window, renderer, line.screenshot );
		} else {
			int pending = 3;
			bool shown = false;
			auto dispatch = [&]( const SDL_Event &event ) {
				gui::PlatformProcessEvent( event );
				app.HandleEvent( event );
			};
			while ( !app.QuitRequested() ) {
				SDL_Event event;
				bool received = false;
				const bool minimized = ( SDL_GetWindowFlags( window ) & SDL_WINDOW_MINIMIZED ) != 0;
				if ( shown && ( minimized || ( pending <= 0 && !app.Busy() ) ) ) {
					if ( SDL_WaitEventTimeout( &event, minimized ? 500 : app.IdleWaitMilliseconds() ) ) {
						dispatch( event );
						received = true;
					}
					pending = std::max( pending, 1 );
				}
				while ( SDL_PollEvent( &event ) ) {
					dispatch( event );
					received = true;
				}
				// ImGui settles hover and focus over a few frames after input.
				if ( received ) pending = std::max( pending, 3 );
				if ( app.QuitRequested() ) break;
				if ( minimized ) {
					app.BeforeFrame();
					continue;
				}
				Draw( app, renderer, 0.0f );
				SDL_RenderPresent( renderer );
				if ( !shown ) {
					SDL_ShowWindow( window );
					if ( maximized ) SDL_MaximizeWindow( window );
					shown = true;
				}
				--pending;
			}
			app.Shutdown();
		}
	}
	gui::RendererShutdown();
	gui::PlatformShutdown();
	ImGui::DestroyContext();
	SDL_DestroyRenderer( renderer );
	SDL_DestroyWindow( window );
	SDL_Quit();
	return result;
}
