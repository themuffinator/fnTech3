// Tracing through the ray-tracing pipeline (rt_main.rgen): the TLAS, the
// closest-hit shader's payload, the shadow miss shader, and the filter rays'
// hit group and miss shader (rt_filter.rahit, rt_filter.rmiss).
//
// Requires: rt_common.glsl, rt_bindings.glsl.

#ifndef RT_TRACE_HW_GLSL
#define RT_TRACE_HW_GLSL

layout(location = 0) rayPayloadEXT RtHitPayload payload;
layout(location = 1) rayPayloadEXT float shadowVisible;
layout(location = 2) rayPayloadEXT vec3 filterTransmission;

// The nearest hit along the ray, in payload.
bool trace_radiance(vec3 origin, vec3 dir, float tMin, float tMax, uint mask)
{
	payload.hitT = -1.0;
	traceRayEXT(u_tlas, gl_RayFlagsNoneEXT, mask, 0, 0, 0, origin, tMin, dir, tMax, 0);
	return payload.hitT >= 0.0;
}

// The nearest surface along the ray, in payload, with the world solid:
// neither alpha tests nor face culling let the ray through it, as the map
// compilers' light rays saw it (rt_lighting.glsl). The models, movers,
// weapon and body they never saw keep their alpha tests, in a second ray
// that reaches no further than the world's hit.
bool trace_solid(vec3 origin, vec3 dir, float tMin, float tMax, uint mask)
{
	payload.hitT = -1.0;
	uint still = mask & ~RT_MASK_MOVING;
	if (still != 0u) {
		traceRayEXT(u_tlas, gl_RayFlagsOpaqueEXT, still, 0, 0, 0, origin, tMin, dir, tMax, 0);
	}
	if ((mask & RT_MASK_MOVING) != 0u) {
		RtHitPayload world = payload;
		float reach = world.hitT >= 0.0 ? world.hitT : tMax;
		traceRayEXT(u_tlas, gl_RayFlagsNoneEXT, mask & RT_MASK_MOVING, 0, 0, 0, origin, tMin, dir, reach, 0);
		if (payload.hitT < 0.0) {
			payload = world;
		}
	}
	return payload.hitT >= 0.0;
}

// 1 when nothing in the mask's instances blocks the segment, else 0; a solid
// ray is blocked by any surface, alpha-tested or not.
// The last shadow ray's blocker, which this backend does not report (hitT -1)
RtHitPayload g_shadowBlocker;

float rt_trace_shadow_mask(vec3 origin, vec3 dir, float tMax, uint mask, bool solid)
{
	g_shadowBlocker.hitT = -1.0;
	if (tMax <= 0.02) {
		return 1.0;
	}
	shadowVisible = 0.0;
	if (!solid) {
		traceRayEXT(u_tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
			mask, 0, 0, 1, origin, 0.01, dir, tMax, 1);
		return shadowVisible;
	}
	// a solid ray: the world opaque, then what the compilers never saw
	// through its alpha tests
	uint still = mask & ~RT_MASK_MOVING;
	shadowVisible = 1.0;
	if (still != 0u) {
		shadowVisible = 0.0;
		traceRayEXT(u_tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT,
			still, 0, 0, 1, origin, 0.01, dir, tMax, 1);
	}
	if (shadowVisible > 0.0 && (mask & RT_MASK_MOVING) != 0u) {
		shadowVisible = 0.0;
		traceRayEXT(u_tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
			mask & RT_MASK_MOVING, 0, 0, 1, origin, 0.01, dir, tMax, 1);
	}
	return shadowVisible;
}

// The light the map's alphashadow and lightfilter surfaces let through along
// the segment (rt_filter.glsl): the filter instance's any-hit (hit group 1,
// miss 2) scales it at every one.
vec3 rt_trace_filter(vec3 origin, vec3 dir, float tMin, float tMax)
{
	if ((u_frame.flags.x & RT_FRAME_FILTERS) == 0u || !(tMax > tMin)) {
		return vec3(1.0);
	}
	filterTransmission = vec3(1.0);
	traceRayEXT(u_tlas, gl_RayFlagsSkipClosestHitShaderEXT, RT_MASK_FILTER, 1, 0, 2, origin, tMin, dir, tMax, 2);
	return filterTransmission;
}

#endif
