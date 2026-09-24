/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_QCOMMON_NATIVE_SEH_H
#define FNTECH3_QCOMMON_NATIVE_SEH_H

/* Structured-exception registrations a native module leaves behind when an
 * engine error abandons its frames.
 *
 * 32-bit Windows threads its exception handlers through the stack: the
 * thread's NT_TIB.ExceptionList (fs:[0]) names the innermost registration
 * record, each record names the next outer one, and -1 ends the list. A frame
 * that uses C++ exceptions or __try pushes a record on entry and pops it on
 * exit, and the record's handler is code inside the image that owns the frame.
 *
 * Com_Error leaves a native module's frames by longjmp, and the MSVC CRT's x86
 * longjmp does not simply reset fs:[0]: it hands every record between the
 * error and the setjmp to ntdll!RtlUnwind, which validates each handler and
 * calls it. Com_Error shuts the modules down and unloads them first, so a
 * record whose handler was inside a module now names unmapped memory, and
 * RtlUnwind raises STATUS_BAD_STACK (0xC0000028), a noncontinuable exception
 * that the same dead chain cannot dispatch. Retail Alice's client game has two
 * such records live when it calls an import.
 *
 * These entry points unlink those records before the teardown, so the unwind
 * never visits them. Only records whose handler lies inside a native module's
 * image are removed; records the engine or the system own, including engine
 * callbacks the module was calling when the error came, stay linked and unwind
 * exactly as before. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The two words of an x86 EXCEPTION_REGISTRATION_RECORD. Records are bigger
 * than this (the rest is the owning frame's own), but nothing past the handler
 * is ever read. */
typedef struct nativeSehRecord_s {
	struct nativeSehRecord_s *next;
	void *handler;
} nativeSehRecord_t;

#define NATIVE_SEH_CHAIN_END ( (nativeSehRecord_t *)(intptr_t)-1 )

/* Whether handler is code in an image whose frames are being abandoned. */
typedef int ( *nativeSehOwner_t )( const void *handler, void *context );

/* Walk the list that *head starts, outward, and unlink every record whose
 * handler owned() claims, keeping the others in order. The walk stops at the
 * end marker or at the first record that is not a well-formed stack record:
 * inside [stackLow, stackHigh), four-byte aligned, and above the record before
 * it. A damaged tail is left exactly as it was. Returns the number of records
 * unlinked. */
int NativeSEH_UnlinkOwned( nativeSehRecord_t **head, uintptr_t stackLow, uintptr_t stackHigh,
	nativeSehOwner_t owned, void *context );

/* The current thread's list, with owned() meaning "inside one of these loaded
 * images" (library handles as Sys_LoadLibrary returns them). Call it while the
 * images are still loaded. Returns 0 and does nothing except on 32-bit x86
 * Windows, where no other platform keeps such a list. */
int Sys_UnlinkModuleExceptionRecords( void *const *modules, int count );

/* Every loaded native VM module's records on the current thread, for
 * Com_Error. Returns the number unlinked. */
int VM_UnlinkNativeExceptionRecords( void );

#ifdef __cplusplus
}
#endif

#endif
