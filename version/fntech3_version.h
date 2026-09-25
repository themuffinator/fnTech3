/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.
===========================================================================
*/

#ifndef FNTECH3_VERSION_H
#define FNTECH3_VERSION_H

#define FNTECH3_PROJECT_NAME "fnTech3"
#define FNTECH3_PROJECT_NAME_SHORT "fnTech3"
#define FNTECH3_DISPLAY_NAME "fnTech3"
#define FNTECH3_COMPATIBILITY_TARGET "Quake III Arena, Quake Live, Return to Castle Wolfenstein (SP/MP), Wolfenstein: Enemy Territory, Star Trek Voyager: Elite Force (SP/Holomatch), Star Wars Jedi Knight: Jedi Academy (SP/MP), Star Wars Jedi Knight II: Jedi Outcast (SP/MP), American McGee's Alice, Heavy Metal: F.A.K.K.2, and Medal of Honor: Allied Assault"

/* Canonical release version. Use scripts/release_workflow.py prepare to update
 * every representation together. Published versions use MAJOR.MINOR.PATCH;
 * TWEAK stays zero and CI run/commit identity lives in the release manifest. */
#define FNTECH3_VERSION_MAJOR 0
#define FNTECH3_VERSION_MINOR 1
#define FNTECH3_VERSION_PATCH 0
#define FNTECH3_VERSION_TWEAK 0

#define FNTECH3_VERSION_STRING "0.1.0"

#define FNTECH3_TAG_PREFIX "v"

/* Renderer, provider and content sidecars keep their compatibility names. */
#define FNTECH3_ARTIFACT_PREFIX "fntech3"
#define FNTECH3_EXECUTABLE_BASENAME "fntech3"

#define FNTECH3_WINDOWS_FILE_VERSION 0,1,0,0
#define FNTECH3_WINDOWS_PRODUCT_VERSION 0,1,0,0

/* Source compatibility for the initial profile scaffold. */
#define FNTECH3_VERSION_BUILD FNTECH3_VERSION_TWEAK

#endif
