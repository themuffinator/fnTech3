/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_DEMO_AUTO_RECORD_HPP
#define FNTECH3_CLIENT_DEMO_AUTO_RECORD_HPP

/* cl_autoRecordDemo runs `record` on every frame of play that is not being
 * recorded (CL_Frame). Three profiles' `record` refuses outright while their
 * synchronous-clients cvar is 0 (CL_Record_f): RTCW SP and Elite Force
 * Holomatch with g_synchronousClients, Alice with g_syncronousClients. Left
 * alone, the automatic attempt repeats that refusal every frame.
 *
 * The rule here makes the automatic attempt that will be refused once per
 * latch: the caller clears the latch whenever the level or the connection
 * changes (CL_ClearState, which every gamestate and every disconnect run), and
 * an attempt that will not be refused clears it too, so setting the cvar
 * mid-level starts the recording on the next frame. The refusal itself stays
 * CL_Record_f's own message, and a profile whose `record` never refuses
 * passes refused = false and keeps trying every frame exactly as before.
 *
 * Nothing here is a wire or module contract; it includes nothing of the
 * engine's so that tests/demo_auto_record_tests.py can compile it alone. */

namespace fntech3::client::demo {

/* Whether this frame's automatic attempt should run `record`. `refused` says
 * that the profile's record would refuse now; `latched` is the caller's
 * per-level, per-connection memory of an automatic attempt already refused. */
inline bool AutoRecordAttempt( bool refused, bool &latched ) {
	if ( !refused ) {
		latched = false;
		return true;
	}
	if ( latched ) {
		return false;
	}
	latched = true;
	return true;
}

} // namespace fntech3::client::demo

#endif
