/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 fnTech3 contributors
 */
/* Unlinking a native module's exception registrations before Com_Error
 * abandons its frames (code/qcommon/native_seh.c).
 *
 * The list walk is checked on synthetic chains everywhere. On 32-bit x86
 * Windows the thread's real list is also used: a frame registers a record the
 * way a module's C++ frame does, an engine callback below it registers a real
 * __try/__finally (where the compiler has one), and the CRT's longjmp then has
 * to land cleanly while still running the engine's __finally. If the module's
 * record were left linked,
 * ntdll!RtlUnwind would reject its handler and kill the process with
 * STATUS_BAD_STACK, which is the failure the engine hit. */

#include "../code/qcommon/native_seh.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;

#define CHECK( condition ) do { \
	if ( !( condition ) ) { \
		fprintf( stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition ); \
		failures++; \
	} \
} while ( 0 )

/* ---- synthetic chains ---------------------------------------------------- */

#define OWNED_LOW  ( (uintptr_t)0x30000000u )
#define OWNED_HIGH ( (uintptr_t)0x30466000u )

static void *const MODULE_HANDLER = (void *)(uintptr_t)0x3004c7b8u;
static void *const ENGINE_HANDLER = (void *)(uintptr_t)0x00a203a5u;

static int OwnedByFakeModule( const void *handler, void *context ) {
	int *calls = (int *)context;
	const uintptr_t at = (uintptr_t)handler;

	if ( calls ) {
		( *calls )++;
	}
	return at >= OWNED_LOW && at < OWNED_HIGH;
}

typedef struct {
	nativeSehRecord_t records[8];
} fakeStack_t;

/* Thread records 0..count-1 in address order, innermost first, with the given
 * handlers, and return the head. */
static nativeSehRecord_t *Build( fakeStack_t *stack, void *const *handlers, int count ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		stack->records[i].handler = handlers[i];
		stack->records[i].next = i + 1 < count ? &stack->records[i + 1] : NATIVE_SEH_CHAIN_END;
	}
	return count ? &stack->records[0] : NATIVE_SEH_CHAIN_END;
}

static uintptr_t Low( const fakeStack_t *stack ) {
	return (uintptr_t)&stack->records[0];
}

static uintptr_t High( const fakeStack_t *stack ) {
	return (uintptr_t)( &stack->records[0] + 8 );
}

static void TestRejectsMissingArguments( void ) {
	fakeStack_t stack;
	void *handlers[] = { MODULE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 1 );

	CHECK( NativeSEH_UnlinkOwned( NULL, Low( &stack ), High( &stack ), OwnedByFakeModule, NULL ) == 0 );
	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), NULL, NULL ) == 0 );
	CHECK( NativeSEH_UnlinkOwned( &head, High( &stack ), Low( &stack ), OwnedByFakeModule, NULL ) == 0 );
	CHECK( head == &stack.records[0] );
	CHECK( Sys_UnlinkModuleExceptionRecords( NULL, 0 ) == 0 );
	CHECK( Sys_UnlinkModuleExceptionRecords( NULL, 3 ) == 0 );
}

/* An error that crosses no native module changes nothing. */
static void TestLeavesEngineOnlyChain( void ) {
	fakeStack_t stack;
	void *handlers[] = { ENGINE_HANDLER, ENGINE_HANDLER, ENGINE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 3 );
	int calls = 0;

	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), OwnedByFakeModule, &calls ) == 0 );
	CHECK( calls == 3 );
	CHECK( head == &stack.records[0] );
	CHECK( stack.records[0].next == &stack.records[1] );
	CHECK( stack.records[1].next == &stack.records[2] );
	CHECK( stack.records[2].next == NATIVE_SEH_CHAIN_END );
}

/* Retail Alice's client game at its import 37: two of its own records, then
 * the CRT's. The head moves past both. */
static void TestUnlinksModuleRecordsAtTheHead( void ) {
	fakeStack_t stack;
	void *handlers[] = { MODULE_HANDLER, MODULE_HANDLER, ENGINE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 3 );

	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), OwnedByFakeModule, NULL ) == 2 );
	CHECK( head == &stack.records[2] );
	CHECK( stack.records[2].next == NATIVE_SEH_CHAIN_END );
}

/* Engine callbacks the module was running keep their registrations, in
 * order; only the module's go. */
static void TestSplicesAroundEngineRecords( void ) {
	fakeStack_t stack;
	void *handlers[] = { ENGINE_HANDLER, MODULE_HANDLER, ENGINE_HANDLER, MODULE_HANDLER, MODULE_HANDLER, ENGINE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 6 );

	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), OwnedByFakeModule, NULL ) == 3 );
	CHECK( head == &stack.records[0] );
	CHECK( stack.records[0].next == &stack.records[2] );
	CHECK( stack.records[2].next == &stack.records[5] );
	CHECK( stack.records[5].next == NATIVE_SEH_CHAIN_END );
}

static void TestUnlinksWholeChain( void ) {
	fakeStack_t stack;
	void *handlers[] = { MODULE_HANDLER, MODULE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 2 );

	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), OwnedByFakeModule, NULL ) == 2 );
	CHECK( head == NATIVE_SEH_CHAIN_END );
}

static void TestEmptyChain( void ) {
	fakeStack_t stack;
	nativeSehRecord_t *head = NATIVE_SEH_CHAIN_END;

	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), OwnedByFakeModule, NULL ) == 0 );
	CHECK( head == NATIVE_SEH_CHAIN_END );
}

/* A record outside the stack ends the walk: what was already found is
 * unlinked, and the damaged tail is not touched or even read. */
static void TestStopsAtRecordOutsideStack( void ) {
	fakeStack_t stack;
	nativeSehRecord_t outside = { NATIVE_SEH_CHAIN_END, MODULE_HANDLER };
	void *handlers[] = { MODULE_HANDLER, ENGINE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 2 );
	int calls = 0;

	stack.records[1].next = &outside;
	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), (uintptr_t)&stack.records[2], OwnedByFakeModule, &calls ) == 1 );
	CHECK( calls == 2 );
	CHECK( head == &stack.records[1] );
	CHECK( stack.records[1].next == &outside );
	CHECK( outside.next == NATIVE_SEH_CHAIN_END );
}

/* A record whose last word would cross the top of the stack is not a record. */
static void TestStopsAtRecordCrossingStackTop( void ) {
	fakeStack_t stack;
	void *handlers[] = { MODULE_HANDLER, MODULE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 2 );
	int calls = 0;

	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), (uintptr_t)&stack.records[1] + 4,
		OwnedByFakeModule, &calls ) == 1 );
	CHECK( calls == 1 );
	CHECK( head == &stack.records[1] );
}

static void TestStopsAtMisalignedRecord( void ) {
	fakeStack_t stack;
	void *handlers[] = { ENGINE_HANDLER, MODULE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 2 );
	int calls = 0;

	stack.records[0].next = (nativeSehRecord_t *)( (char *)&stack.records[1] + 2 );
	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), OwnedByFakeModule, &calls ) == 0 );
	CHECK( calls == 1 );
	CHECK( head == &stack.records[0] );
}

/* Records must climb the stack; a loop or a step back ends the walk, so a
 * corrupted chain cannot hang the error path. */
static void TestStopsWhenChainTurnsBack( void ) {
	fakeStack_t stack;
	void *handlers[] = { ENGINE_HANDLER, MODULE_HANDLER, ENGINE_HANDLER };
	nativeSehRecord_t *head = Build( &stack, handlers, 3 );
	int calls = 0;

	stack.records[2].next = &stack.records[0];
	CHECK( NativeSEH_UnlinkOwned( &head, Low( &stack ), High( &stack ), OwnedByFakeModule, &calls ) == 1 );
	CHECK( calls == 3 );
	CHECK( head == &stack.records[0] );
	CHECK( stack.records[0].next == &stack.records[2] );
	CHECK( stack.records[2].next == &stack.records[0] );
}

/* ---- the thread's own list ---------------------------------------------- */

#if defined( _WIN32 ) && ( defined( _M_IX86 ) || defined( __i386__ ) )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setjmp.h>

#if defined( _MSC_VER )
#define TEST_NOINLINE __declspec( noinline )
#else
#define TEST_NOINLINE __attribute__(( noinline ))
#endif

static jmp_buf landing;
static void *fakeModule;
static void *fakeHandler;
static volatile int liveUnlinked = -1;
static volatile int finallyRan;

static NT_TIB *ThreadInformation( void ) {
#if defined( __GNUC__ ) && !defined( _MSC_VER )
	NT_TIB *tib;

	__asm__ __volatile__( "movl %%fs:0x18, %0" : "=r" ( tib ) );
	return tib;
#else
	return (NT_TIB *)NtCurrentTeb();
#endif
}

static nativeSehRecord_t *ChainHead( void ) {
	return (nativeSehRecord_t *)ThreadInformation()->ExceptionList;
}

static void SetChainHead( nativeSehRecord_t *head ) {
	ThreadInformation()->ExceptionList = (struct _EXCEPTION_REGISTRATION_RECORD *)head;
}

/* Com_Error: unlink, then leave by the CRT's longjmp, which unwinds through
 * whatever registrations are still linked (mingw-w64's setjmp is the same
 * msvcrt _setjmp3 pair). */
static TEST_NOINLINE void AbandonFrames( void ) {
	liveUnlinked = Sys_UnlinkModuleExceptionRecords( &fakeModule, 1 );
	longjmp( landing, 1 );
}

/* An engine import the module called, with a real registration of its own
 * where the compiler has __try. */
static TEST_NOINLINE void EngineCallback( void ) {
#if defined( _MSC_VER )
	__try {
		AbandonFrames();
	} __finally {
		finallyRan = 1;
	}
#else
	finallyRan = 1;
	AbandonFrames();
#endif
}

/* A module frame's registration, whose handler is inside the module image.
 * The handler address is never valid code; if the record stayed linked,
 * RtlUnwind would refuse it and raise STATUS_BAD_STACK. */
static TEST_NOINLINE void ModuleFrame( void ) {
	volatile nativeSehRecord_t record;

	record.next = ChainHead();
	record.handler = fakeHandler;
	SetChainHead( (nativeSehRecord_t *)&record );
	EngineCallback();
	SetChainHead( record.next );
}

static void TestLiveLongjmpPastUnlinkedModuleRecord( void ) {
	nativeSehRecord_t *const before = ChainHead();

	/* A loaded image stands in for the module; the handler points into its
	 * headers, which are image memory and never executable. */
	fakeModule = (void *)GetModuleHandleA( "kernel32.dll" );
	CHECK( fakeModule != NULL );
	fakeHandler = (char *)fakeModule + 0x10;
	finallyRan = 0;
	liveUnlinked = -1;

	if ( setjmp( landing ) == 0 ) {
		ModuleFrame();
		CHECK( !"the abandoned frames returned" );
	}

	CHECK( liveUnlinked == 1 );
	CHECK( finallyRan == 1 );
	CHECK( ChainHead() == before );
}

/* A record whose handler is in some other image is not the module's. The
 * stand-in module must own nothing else on this thread's list, which rules out
 * the executable (the CRT's record) and ntdll (the thread's outermost ones). */
static TEST_NOINLINE void TestLiveKeepsForeignRecord( void ) {
	volatile nativeSehRecord_t record;
	nativeSehRecord_t *const before = ChainHead();
	void *otherModule = (void *)GetModuleHandleA( "kernelbase.dll" );

	CHECK( otherModule != NULL );
	record.next = before;
	record.handler = (char *)GetModuleHandleA( "kernel32.dll" ) + 0x10;
	SetChainHead( (nativeSehRecord_t *)&record );
	CHECK( Sys_UnlinkModuleExceptionRecords( &otherModule, 1 ) == 0 );
	CHECK( ChainHead() == (nativeSehRecord_t *)&record );
	SetChainHead( before );
}

static void RunLiveTests( void ) {
	TestLiveLongjmpPastUnlinkedModuleRecord();
	TestLiveKeepsForeignRecord();
	printf( "native_seh: live x86 chain checks ran\n" );
}

#else

static void RunLiveTests( void ) {
	printf( "native_seh: no x86 SEH list on this platform; live checks skipped\n" );
}

#endif

int main( void ) {
	TestRejectsMissingArguments();
	TestLeavesEngineOnlyChain();
	TestUnlinksModuleRecordsAtTheHead();
	TestSplicesAroundEngineRecords();
	TestUnlinksWholeChain();
	TestEmptyChain();
	TestStopsAtRecordOutsideStack();
	TestStopsAtRecordCrossingStackTop();
	TestStopsAtMisalignedRecord();
	TestStopsWhenChainTurnsBack();
	RunLiveTests();

	if ( failures ) {
		fprintf( stderr, "native_seh: %d check(s) failed\n", failures );
		return EXIT_FAILURE;
	}
	printf( "native_seh: all checks passed\n" );
	return EXIT_SUCCESS;
}
