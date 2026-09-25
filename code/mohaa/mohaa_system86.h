/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_MOHAA_SYSTEM86_H
#define FNTECH3_MOHAA_SYSTEM86_H

/* Medal of Honor: Allied Assault's module memory.
 *
 * Both retail libraries import MemoryMalloc and MemoryFree by name from
 * system86.dll, the allocator that lies beside the retail executable in the
 * installation's root. 2015's engine serves the modules' own Malloc and Free
 * imports, and their file buffers, from the same heap: the game frees blocks
 * it took from MemoryMalloc through its Free import, and file buffers
 * through Free as well (gamex86 0x3103d449). So every block the engine hands
 * either module comes from the installation's own allocator here too, and
 * any of them may be freed through any of the three.
 *
 * The user's copy is loaded from the installation, never shipped. Where it
 * cannot be loaded, neither can the modules that import it; the functions
 * then fall back to the C heap so the engine still runs to the error. */

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load the installation's system86.dll once (fs_basepath). */
qboolean MOHAA_LoadSystem86( void );
void *MOHAA_ModuleMalloc( int size );
void MOHAA_ModuleFree( void *block );
/* A file read into module memory, with a terminating zero past its length,
 * or -1 and a null buffer. */
int MOHAA_ModuleReadFile( const char *path, void **buffer );

#ifdef __cplusplus
}
#endif

#endif
