/* Keep setjmp in C so the test models Com_Error without crossing any C++
 * object lifetime. Codec decoder frames and callback captures are trivial. */
#include <setjmp.h>

static jmp_buf messageDropTarget;
static int expectMessageDrop;

int Test_CatchMessageDrop( void (*callback)( void * ), void *context ) {
	int dropped;
	expectMessageDrop = 1;
	dropped = setjmp( messageDropTarget );
	if ( !dropped ) {
		callback( context );
	}
	expectMessageDrop = 0;
	return dropped != 0;
}

void Test_RaiseExpectedMessageDrop( void ) {
	if ( expectMessageDrop ) {
		longjmp( messageDropTarget, 1 );
	}
}
