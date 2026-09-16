/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_PROCESS_RELAUNCH_H
#define FNTECH3_PROCESS_RELAUNCH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Main-thread, two-phase handoff to this executable. Preparation validates and
 * owns a copy of the engine command line; it never starts a child or shuts down
 * the current game. A failed preparation preserves any existing request.
 * Errors/shutdown failures must cancel. Launch consumes the request and must
 * run only after modules, renderer, filesystem, network and terminal cleanup.
 * Windows starts the same image without inherited handles; POSIX replaces this
 * process without inherited non-standard descriptors. No shell or PATH search.
 * Launch can still fail after shutdown (for example if the image was removed);
 * the caller must report the diagnostic and exit unsuccessfully in that case.
 */
enum { SYS_RELAUNCH_COMMAND_BYTES = 1024 };
int Sys_PrepareRelaunch( const char *commandLine, char *error, unsigned errorCapacity );
int Sys_HasPendingRelaunch( void );
void Sys_CancelRelaunch( void );
int Sys_LaunchRelaunch( char *error, unsigned errorCapacity );

#ifdef _WIN32
/* Preserve raw engine quoting while removing only Windows' executable token.
 * Read GetCommandLineW instead of WinMain's lossy ANSI argument. Transactional:
 * invalid Unicode or insufficient space leaves the destination untouched.
 */
int Sys_DecodeStartupCommandLine( const wchar_t *commandLine,
	char *destination, unsigned capacity );
#endif

#ifdef __cplusplus
}
#endif
#endif
