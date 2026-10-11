// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#ifndef RT_LIGHT_REUSE_GLSL
#define RT_LIGHT_REUSE_GLSL
// Replay a small proposal pool on compatible nearby receivers. Four streams
// rotate one at a time, so an entire lighting estimate never waits for a batch
// refresh. Only random proposals are reused: target, PDF, selected sample and
// visibility are evaluated at the current point against the current scene.
// This is common-random-number reuse, not a cached visibility or RIS reservoir.
uint rt_reused_light_stream(uint receiverKey, uint frame, uint k)
{
	return hash_u32(receiverKey ^ (((frame + k) / 4u) * 0x9e3779b9u) ^ (k * 0x85ebca6bu));
}
#endif
