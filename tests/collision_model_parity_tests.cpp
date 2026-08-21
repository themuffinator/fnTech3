#include "../code/qcommon/cm_model_handles.h"
#include "../code/qcommon/cm_trace_contract.h"
#include "../code/server/sv_collision_model.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

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
}

void TestRetailEntityShapeSelection()
{
	using fnql::server::collision::UseCapsuleEntityModel;

	// Ordinary point/box traces keep the target box.
	CHECK( !UseCapsuleEntityModel( false, true ) );
	CHECK( !UseCapsuleEntityModel( false, false ) );
	// Capsule-vs-capsule math is selected only when both sides request it.
	CHECK( UseCapsuleEntityModel( true, true ) );
	CHECK( !UseCapsuleEntityModel( true, false ) );
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
	TestCapsuleTraceProfile();
	TestTracePlaneContract();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
