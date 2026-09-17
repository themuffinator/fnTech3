/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RTCW_CAMERA_INPUT_HPP
#define FNTECH3_CLIENT_RTCW_CAMERA_INPUT_HPP
// Logical event policy only. This neither polls devices nor generates input.
enum class rtcwCameraKeyAction { Pass, Consume, Interrupt };
inline rtcwCameraKeyAction CL_RTCWCameraKeyAction(bool sp, bool primaryCamera,
	int catcher, int key, bool down) {
	if (!sp || !primaryCamera) return rtcwCameraKeyAction::Pass;
	if (!(catcher & (KEYCATCH_UI | KEYCATCH_CONSOLE)) && down)
		return key == K_ESCAPE || key == K_SPACE || key == K_ENTER
			? rtcwCameraKeyAction::Interrupt : rtcwCameraKeyAction::Consume;
	if ((catcher & KEYCATCH_CONSOLE) && key == K_ESCAPE) return rtcwCameraKeyAction::Consume;
	return rtcwCameraKeyAction::Pass;
}
#endif
