// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#ifndef RT_TEMPORAL_RESPONSE_GLSL
#define RT_TEMPORAL_RESPONSE_GLSL

// Visibility can be sampled sparsely. Retain the fast policy after a moving
// blocker disappears, in seconds rather than a frame-count dependent trail.
float rt_shadow_reactive_time(float previous, float blocked, float elapsed)
{
	return blocked > 0.0 ? 0.15 : max(previous - max(elapsed, 0.0), 0.0);
}

// Response is in seconds, independent of the number of rendered frames.
// A zero elapsed time denotes deliberate accumulation of a frozen scene.
float rt_temporal_alpha(float elapsed, float response, float samples)
{
	float average = 1.0 / max(samples, 1.0);
	return elapsed <= 0.0 ? average :
		max(average, 1.0 - exp(-min(elapsed, 0.25) / response));
}

// Only a reactive receiver needs immediate neighborhood clipping. A quiet
// neighborhood of stochastic samples is not evidence that its old lighting
// disappeared: clipping it to zero erases rare light samples between hits,
// producing dark bias and isolated sparkles. Frozen averages never clip.
float rt_temporal_history(float history, float lower, float upper, float elapsed, bool reactive)
{
	return elapsed <= 0.0 || !reactive ? history : clamp(history, lower, upper);
}

float rt_temporal_blend(float current, float history, float lower, float upper,
	float elapsed, float response, float samples, bool reactive)
{
	float old = rt_temporal_history(history, lower, upper, elapsed, reactive);
	float alpha = rt_temporal_alpha(elapsed, response, samples);
	return old + (current - old) * alpha;
}

float rt_indirect_sample_alpha(float inverseDensity, float elapsed, float samples)
{
	return inverseDensity <= 0.0 ? 0.0 : rt_temporal_alpha(elapsed * inverseDensity, 0.160, samples);
}

float rt_indirect_variance(float spatialVariance, float residual, float innovation, float alpha)
{
	return max(spatialVariance * alpha / max(2.0 - alpha, 1.0),
		max(residual * residual, innovation * innovation * alpha * alpha));
}

#endif
