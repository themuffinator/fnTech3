#include "../code/qcommon/cm_model_handles.h"
#include "../code/qcommon/cm_trace_contract.h"
#include "../code/server/sv_collision_model.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <initializer_list>

static comGameProfile_t selectedProfile{};
static const comGameProfile_t *activeProfile;
const comGameProfile_t *Com_GameProfile( void ) { return activeProfile; }

namespace {

int failures;

void Check( bool condition, const char *expression, int line )
{
	if ( condition ) return;
	std::cerr << "line " << line << ": check failed: " << expression << '\n';
	++failures;
}

#define CHECK(expression) Check( ( expression ), #expression, __LINE__ )

void TestReservedHandleContract()
{
	static_assert( MAX_SUBMODELS == 256 );
	static_assert( CAPSULE_MODEL_HANDLE == 254 );
	static_assert( BOX_MODEL_HANDLE == 255 );

	CHECK( CM_ModelHandleLimit() == 0 );
	CHECK( CM_TemporaryModelHandle( qfalse ) == -1 );
	CHECK( CM_TemporaryModelHandle( qtrue ) == -1 );
	CHECK( !CM_IsTemporaryModelHandle( -1 ) );
	activeProfile = &selectedProfile;
	selectedProfile.collisionModelHandleCount = 256;
	CHECK( CM_TemporaryModelHandle( qfalse ) == BOX_MODEL_HANDLE );
	CHECK( CM_TemporaryModelHandle( qtrue ) == CAPSULE_MODEL_HANDLE );
	CHECK( CM_IsTemporaryModelHandle( BOX_MODEL_HANDLE ) == qtrue );
	CHECK( CM_IsTemporaryModelHandle( CAPSULE_MODEL_HANDLE ) == qtrue );
	CHECK( CM_IsTemporaryModelHandle( 0 ) == qfalse );
	CHECK( CM_IsTemporaryModelHandle( 13 ) == qfalse );
	CHECK( CM_IsTemporaryModelHandle( 14 ) == qfalse );
	CHECK( CM_IsTemporaryModelHandle( CAPSULE_MODEL_HANDLE - 1 ) == qfalse );
	CHECK( CM_IsTemporaryModelHandle( MAX_SUBMODELS ) == qfalse );
	CHECK( CM_IsTemporaryModelHandle( -1 ) == qfalse );
	selectedProfile.collisionModelHandleCount = 512;
	CHECK( CM_ModelHandleLimit() == 512 );
	CHECK( CM_TemporaryModelHandle( qfalse ) == 511 );
	CHECK( CM_TemporaryModelHandle( qtrue ) == 510 );
	CHECK( CM_IsTemporaryModelHandle( 510 ) && CM_IsTemporaryModelHandle( 511 ) );
	CHECK( !CM_IsTemporaryModelHandle( 254 ) && !CM_IsTemporaryModelHandle( 255 ) );
	CHECK( !CM_IsTemporaryModelHandle( 509 ) && !CM_IsTemporaryModelHandle( 512 ) );
}

void TestRetailEntityShapeSelection()
{
	using fnql::server::collision::UseCapsuleEntityModel;

	// Ordinary point/box traces keep the target box.
	CHECK( !UseCapsuleEntityModel( COM_ENTITY_HULL_TRACE_SHAPE, false, true ) );
	CHECK( !UseCapsuleEntityModel( COM_ENTITY_HULL_TRACE_SHAPE, false, false ) );
	// Capsule-vs-capsule math is selected only when both sides request it.
	CHECK( UseCapsuleEntityModel( COM_ENTITY_HULL_TRACE_SHAPE, true, true ) );
	CHECK( !UseCapsuleEntityModel( COM_ENTITY_HULL_TRACE_SHAPE, true, false ) );
	// RTCW's target shape belongs to the entity, independent of the sweep.
	for ( bool sweep : { false, true } ) {
		CHECK( UseCapsuleEntityModel( COM_ENTITY_HULL_ENTITY_FLAG, sweep, true ) );
		CHECK( !UseCapsuleEntityModel( COM_ENTITY_HULL_ENTITY_FLAG, sweep, false ) );
	}
}

void TestRetailEntityBoxContents()
{
	using fnql::server::collision::UseEntityBoxContents;

	// Quake III's box brush is CONTENTS_BODY whatever the entity is.
	CHECK( !UseEntityBoxContents( COM_ENTITY_BOX_CONTENTS_BODY, true ) );
	CHECK( !UseEntityBoxContents( COM_ENTITY_BOX_CONTENTS_BODY, false ) );
	// RTCW MP and ET give it the entity's contents, but only when the entity's
	// handle is the box: capsules and inline models are left alone.
	CHECK( UseEntityBoxContents( COM_ENTITY_BOX_CONTENTS_ENTITY, true ) );
	CHECK( !UseEntityBoxContents( COM_ENTITY_BOX_CONTENTS_ENTITY, false ) );
	// A zeroed profile record keeps Quake III's box.
	static_assert( COM_ENTITY_BOX_CONTENTS_BODY == 0 );
}

void TestCapsuleTraceProfile()
{
	// Retail QL player hull swept by an identically sized capsule.
	const vec3_t mins = { -15.0f, -15.0f, -24.0f };
	const vec3_t maxs = { 15.0f, 15.0f, 32.0f };
	cm_capsuleProfile_t profile{};

	CM_BuildCapsuleTraceProfile( mins, maxs, 15.0f, 28.0f, &profile );

	CHECK( profile.center[0] == 0.0f );
	CHECK( profile.center[1] == 0.0f );
	CHECK( profile.center[2] == 4.0f );
	// The target radius carries the moving capsule's radius.
	CHECK( profile.radius == 30.0f );
	// A cap sphere sits one radius inside each end of the target capsule.
	CHECK( profile.topSphere[2] == 17.0f );
	CHECK( profile.bottomSphere[2] == -9.0f );
	CHECK( profile.topSphere[0] == profile.center[0] );
	CHECK( profile.bottomSphere[1] == profile.center[1] );
	// Both capsule heights, minus what the cap spheres already cover.
	CHECK( profile.cylinderHalfheight == 26.0f );

	// A wide enough sweeping capsule leaves no cylinder section at all, and
	// the trace path must skip the cylinder rather than sweep a negative one.
	const vec3_t cubeMins = { -16.0f, -16.0f, -16.0f };
	const vec3_t cubeMaxs = { 16.0f, 16.0f, 16.0f };

	CM_BuildCapsuleTraceProfile( cubeMins, cubeMaxs, 24.0f, 24.0f, &profile );

	CHECK( profile.radius == 40.0f );
	CHECK( profile.topSphere[2] == profile.center[2] );
	CHECK( profile.bottomSphere[2] == profile.center[2] );
	CHECK( !( profile.cylinderHalfheight > 0.0f ) );
}

void TestTracePlaneContract()
{
	trace_t trace{};

	trace.fraction = 1.0f;
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	// A capsule overlap at the starting point returns no plane at all.
	trace.fraction = 0.0f;
	trace.startsolid = qtrue;
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	// The one-unit analytic epsilon can also produce a sub-unit start plane.
	trace.startsolid = qfalse;
	trace.plane.normal[0] = 0.95f;
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	// A fractional impact must carry a plane on every path.
	trace.fraction = 0.5f;
	VectorClear( trace.plane.normal );
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	VectorSet( trace.plane.normal, 1.0f, 0.0f, 0.0f );
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	// Stored BSP planes are unit; the analytic capsule plane drifts off unit
	// length with distance, so only the stored path may demand one.
	trace.plane.normal[0] = 1.1f;
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );
	trace.plane.normal[0] = 0.89f;
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	// Corrupt fractions and non-finite normals stay defects on both paths.
	VectorSet( trace.plane.normal, 1.0f, 0.0f, 0.0f );
	trace.fraction = NAN;
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );
	trace.fraction = 1.01f;
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );
	trace.fraction = -0.01f;
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	trace.fraction = 0.5f;
	trace.plane.normal[1] = NAN;
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_STORED ) );
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );
	trace.plane.normal[1] = HUGE_VALF;
	CHECK( !CM_TraceResultHasValidPlaneContract( &trace, CM_TRACE_PLANE_ANALYTIC ) );

	CHECK( !CM_TraceResultHasValidPlaneContract( nullptr, CM_TRACE_PLANE_STORED ) );
	CHECK( !CM_TraceResultHasValidPlaneContract( nullptr, CM_TRACE_PLANE_ANALYTIC ) );
}

} // namespace

int main()
{
	TestReservedHandleContract();
	TestRetailEntityShapeSelection();
	TestRetailEntityBoxContents();
	TestCapsuleTraceProfile();
	TestTracePlaneContract();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
