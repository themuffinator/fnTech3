/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Unlinking a native module's structured-exception registrations before an
 * engine error abandons its frames. See native_seh.h for why. */

#include "native_seh.h"

#include <stddef.h>

/*
==================
NativeSEH_UnlinkOwned

The list is only trusted as far as it looks like a list the OS itself would
walk: a record outside the stack, misaligned, or not above its predecessor
ends the walk, which also bounds it on a corrupted or cyclic chain. Nothing is
read through a handler, only compared.
==================
*/
int NativeSEH_UnlinkOwned( nativeSehRecord_t **head, uintptr_t stackLow, uintptr_t stackHigh,
	nativeSehOwner_t owned, void *context ) {
	nativeSehRecord_t **link;
	nativeSehRecord_t *record;
	uintptr_t previous = 0;
	int unlinked = 0;

	if ( !head || !owned || stackHigh <= stackLow ) {
		return 0;
	}

	link = head;
	for ( record = *link; record != NATIVE_SEH_CHAIN_END; record = *link ) {
		const uintptr_t at = (uintptr_t)record;

		if ( at < stackLow || at >= stackHigh || stackHigh - at < sizeof( *record )
			|| ( at & 3 ) != 0 || at <= previous ) {
			break;
		}
		previous = at;

		if ( owned( record->handler, context ) ) {
			*link = record->next;
			unlinked++;
		} else {
			link = &record->next;
		}
	}

	return unlinked;
}

#if defined( _WIN32 ) && ( defined( _M_IX86 ) || defined( __i386__ ) )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
	void *const *modules;
	int count;
} nativeSehModules_t;

/* An image's sections all share its allocation base, which is the module
 * handle LoadLibrary returned. */
static int NativeSEH_HandlerInModule( const void *handler, void *context ) {
	const nativeSehModules_t *set = (const nativeSehModules_t *)context;
	MEMORY_BASIC_INFORMATION region;
	int i;

	if ( !VirtualQuery( handler, &region, sizeof( region ) ) || region.Type != MEM_IMAGE ) {
		return 0;
	}
	for ( i = 0; i < set->count; i++ ) {
		if ( set->modules[i] && region.AllocationBase == set->modules[i] ) {
			return 1;
		}
	}
	return 0;
}

/* NT_TIB sits at the start of the thread's TEB, which fs addresses, and its
 * Self field (fs:[0x18]) is the TEB's linear address. GCC reads it directly:
 * mingw-w64's NtCurrentTeb draws a false -Warray-bounds there. */
static NT_TIB *NativeSEH_ThreadInformation( void ) {
#if defined( __GNUC__ ) && !defined( _MSC_VER )
	NT_TIB *tib;

	__asm__ __volatile__( "movl %%fs:0x18, %0" : "=r" ( tib ) );
	return tib;
#else
	return (NT_TIB *)NtCurrentTeb();
#endif
}

/*
==================
Sys_UnlinkModuleExceptionRecords

The list head written here is the one fs:[0] reads. StackLimit is the
committed low end of the stack; no live record can sit below it.
==================
*/
int Sys_UnlinkModuleExceptionRecords( void *const *modules, int count ) {
	NT_TIB *tib;
	nativeSehModules_t set;

	if ( !modules || count <= 0 ) {
		return 0;
	}

	tib = NativeSEH_ThreadInformation();
	set.modules = modules;
	set.count = count;
	return NativeSEH_UnlinkOwned( (nativeSehRecord_t **)&tib->ExceptionList,
		(uintptr_t)tib->StackLimit, (uintptr_t)tib->StackBase,
		NativeSEH_HandlerInModule, &set );
}

#else

int Sys_UnlinkModuleExceptionRecords( void *const *modules, int count ) {
	(void)modules;
	(void)count;
	return 0;
}

#endif
