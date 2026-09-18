#include <cmath>
#include <iostream>
#include <limits>

#include "../code/client/client_cvar_compat.hpp"

namespace {

int failures;

void Check( bool condition, const char* expression, int line )
{
	if ( condition ) {
		return;
	}
	std::cerr << "line " << line << ": check failed: " << expression << '\n';
	++failures;
}

#define CHECK(expression) Check( ( expression ), #expression, __LINE__ )

void TestRetailTimeNudge()
{
	using fnql::client::cvars::SelectTimeNudge;
	using fnql::client::cvars::TimeNudgeInputs;

	int previous = 0;
	TimeNudgeInputs inputs;
	inputs.manual = -9;
	CHECK( SelectTimeNudge( inputs, previous ) == -9 );
	inputs.manual = -30;
	CHECK( SelectTimeNudge( inputs, previous ) == -20 );
	inputs.manual = 12;
	CHECK( SelectTimeNudge( inputs, previous ) == 0 );

	inputs = {};
	inputs.retailAutomatic = true;
	inputs.snapshotPing = 18;
	previous = 0;
	CHECK( SelectTimeNudge( inputs, previous ) == -9 );
	inputs.snapshotPing = 30;
	CHECK( SelectTimeNudge( inputs, previous ) == -9 );
	inputs.spectating = true;
	CHECK( SelectTimeNudge( inputs, previous ) == 0 );
	CHECK( previous == 0 );

	inputs = {};
	inputs.manual = -10;
	inputs.localServer = true;
	CHECK( SelectTimeNudge( inputs, previous ) == 0 );

	inputs = {};
	inputs.fnqlAutomaticFactor = 0.5f;
	inputs.fnqlAveragePing = 100.0f;
	CHECK( SelectTimeNudge( inputs, previous ) == -50 );
	CHECK( previous == 0 );
}

void TestRetailAvidemo()
{
	using fnql::client::cvars::AvidemoInputs;
	using fnql::client::cvars::PlanAvidemoFrame;

	AvidemoInputs inputs;
	inputs.latchedFramesPerSecond = 50;
	inputs.minimumServerTime = 1000;
	inputs.serverTime = 1000;
	inputs.frameMilliseconds = 16;
	auto plan = PlanAvidemoFrame( inputs );
	CHECK( !plan.clearLatch );
	CHECK( !plan.timingActive );

	inputs.serverTime = 1001;
	inputs.canCapture = true;
	plan = PlanAvidemoFrame( inputs );
	CHECK( plan.clearLatch );
	CHECK( plan.activeFramesPerSecond == 50 );
	CHECK( plan.captureScreenshot );
	CHECK( plan.frameMilliseconds == 20 );

	inputs = {};
	inputs.activeFramesPerSecond = 25;
	inputs.minimumServerTime = 1000;
	inputs.maximumServerTime = 2000;
	inputs.serverTime = 1000;
	inputs.frameMilliseconds = 16;
	inputs.timeScale = 0.5f;
	inputs.canCapture = true;
	plan = PlanAvidemoFrame( inputs );
	CHECK( plan.captureScreenshot );
	CHECK( !plan.disconnect );
	CHECK( plan.frameMilliseconds == 20 );

	inputs.serverTime = 2001;
	plan = PlanAvidemoFrame( inputs );
	CHECK( plan.disconnect );
	CHECK( !plan.captureScreenshot );

	inputs.maximumServerTime = 0;
	inputs.activeFramesPerSecond = 2000;
	inputs.timeScale = std::numeric_limits<float>::infinity();
	plan = PlanAvidemoFrame( inputs );
	CHECK( plan.frameMilliseconds == 1 );

	inputs.frameMilliseconds = 0;
	plan = PlanAvidemoFrame( inputs );
	CHECK( !plan.timingActive );
	CHECK( !plan.captureScreenshot );
}

void TestRetailVideoConfigOwnership()
{
	using fnql::client::cvars::RetailVideoConfigInputs;
	using fnql::client::cvars::UsesRetailVideoConfig;

	// Retail Q3, RTCW and ET configs archive r_stereo and r_ext_gamma_control.
	// Neither they, a marker left by an earlier build, nor a later change to
	// r_windowedMode makes those profiles use QL's video config.
	RetailVideoConfigInputs inputs;
	inputs.hadRetailVideoCvars = true;
	CHECK( !UsesRetailVideoConfig( inputs ) );
	inputs.hadOwnershipMarker = true;
	inputs.ownershipMarked = true;
	CHECK( !UsesRetailVideoConfig( inputs ) );
	inputs.initialized = true;
	inputs.retailVideoCvarsChanged = true;
	CHECK( !UsesRetailVideoConfig( inputs ) );

	// Quake Live adopts a config retail QL wrote, but not an unmarked one of
	// its own, and adopts later changes to the retail cvars.
	inputs = {};
	inputs.quakeLive = true;
	inputs.hadRetailVideoCvars = true;
	CHECK( UsesRetailVideoConfig( inputs ) );
	inputs.hadOwnershipMarker = true;
	CHECK( !UsesRetailVideoConfig( inputs ) );
	inputs.initialized = true;
	inputs.retailVideoCvarsChanged = true;
	CHECK( UsesRetailVideoConfig( inputs ) );

	// For Quake Live, every input keeps the decision made before the bridge
	// was limited to that profile.
	for ( unsigned bits = 0; bits < 32; ++bits ) {
		inputs = {};
		inputs.quakeLive = true;
		inputs.initialized = ( bits & 1 ) != 0;
		inputs.hadOwnershipMarker = ( bits & 2 ) != 0;
		inputs.hadRetailVideoCvars = ( bits & 4 ) != 0;
		inputs.ownershipMarked = ( bits & 8 ) != 0;
		inputs.retailVideoCvarsChanged = ( bits & 16 ) != 0;
		const bool before = ( !inputs.initialized && !inputs.hadOwnershipMarker
				&& inputs.hadRetailVideoCvars )
			|| inputs.ownershipMarked
			|| ( inputs.initialized && inputs.retailVideoCvarsChanged );
		CHECK( UsesRetailVideoConfig( inputs ) == before );
		inputs.quakeLive = false;
		CHECK( !UsesRetailVideoConfig( inputs ) );
	}
}

} // namespace

int main()
{
	TestRetailTimeNudge();
	TestRetailAvidemo();
	TestRetailVideoConfigOwnership();
	return failures == 0 ? 0 : 1;
}
