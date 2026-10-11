// Tracing without VK_KHR_ray_tracing_pipeline: the compute backend
// (rt_trace.comp). Each instance's triangles sit in a binary BVH built on the
// CPU (code/rendererrtx/rt/rt_bvh.c), traversed nearest child first. Hits
// follow the ray-tracing pipeline's rules: opaque geometry counts from either
// side, masked geometry asks rt_hit_accepted, radiance rays keep the nearest
// hit and shadow rays stop at the first.
//
// Requires: rt_common.glsl, rt_bindings.glsl (with RT_SOFTWARE_TRACE),
// rt_material.glsl, rt_geometry.glsl, rt_filter.glsl.

#ifndef RT_TRACE_SW_GLSL
#define RT_TRACE_SW_GLSL

// the builder keeps BVHs shallower than this (RT_BVH_MAX_DEPTH)
const uint RT_SW_STACK = 64u;

// barycentric slack: neighbouring triangles overlap along a shared edge, so a
// ray cannot slip between them through rounding
const float RT_SW_EDGE_EPSILON = 1e-6;

RtHitPayload payload;

RtBvhNode rt_sw_node(bool dynamic, uint index)
{
	return dynamic ? u_dynamicBvh.nodes[index] : u_worldBvh.nodes[index];
}

RtBvhTri rt_sw_tri(bool dynamic, uint index)
{
	return dynamic ? u_dynamicTris.tris[index] : u_worldTris.tris[index];
}

// The slab test: the distance at which the ray enters the box within
// [tMin, tMax], or -1.
float rt_sw_box(vec3 origin, vec3 invDir, RtBvhNode node, float tMin, float tMax)
{
	vec3 t0 = (node.bmin.xyz - origin) * invDir;
	vec3 t1 = (node.bmax.xyz - origin) * invDir;
	vec3 lo = min(t0, t1);
	vec3 hi = max(t0, t1);
	float enter = max(max(lo.x, lo.y), max(lo.z, tMin));
	float leave = min(min(hi.x, hi.y), min(hi.z, tMax));
	return enter <= leave ? enter : -1.0;
}

// Moller-Trumbore. bary holds the weights of v1 and v2, as the hardware's
// hit attribute does.
bool rt_sw_triangle(vec3 origin, vec3 dir, vec3 v0, vec3 v1, vec3 v2, float tMin, float tMax,
	out float t, out vec2 bary, out bool front)
{
	vec3 e1 = v1 - v0;
	vec3 e2 = v2 - v0;
	vec3 p = cross(dir, e2);
	float det = dot(e1, p);
	t = 0.0;
	bary = vec2(0.0);
	front = false;
	if (det == 0.0) {
		return false;
	}
	float inv = 1.0 / det;
	vec3 s = origin - v0;
	float u = dot(s, p) * inv;
	if (u < -RT_SW_EDGE_EPSILON || u > 1.0 + RT_SW_EDGE_EPSILON) {
		return false;
	}
	vec3 q = cross(s, e1);
	float v = dot(dir, q) * inv;
	if (v < -RT_SW_EDGE_EPSILON || u + v > 1.0 + RT_SW_EDGE_EPSILON) {
		return false;
	}
	t = dot(e2, q) * inv;
	if (!(t > tMin && t < tMax)) {
		return false;
	}
	bary = clamp(vec2(u, v), vec2(0.0), vec2(1.0));
	// det = -dot(dir, e1 x e2); Quake's front faces wind clockwise seen from
	// in front, so e1 x e2 points away from a ray that meets the front
	front = det < 0.0;
	return true;
}

// Trace against the instances in mask. A radiance ray records its nearest
// hit in hit; a shadow ray (firstHit) returns at its first. A solid ray takes
// the world's masked geometry as opaque, as the map compilers' light rays
// took it, and alpha-tests the models, movers, weapon and body they never saw
// (rt_trace_hw.glsl does the same in two rays).
bool rt_sw_trace(vec3 origin, vec3 dir, float tMin, float tMax, uint mask, bool firstHit, bool solid,
	inout RtHitPayload hit)
{
	vec3 safeDir = vec3(
		abs(dir.x) > 1e-12 ? dir.x : (dir.x < 0.0 ? -1e-12 : 1e-12),
		abs(dir.y) > 1e-12 ? dir.y : (dir.y < 0.0 ? -1e-12 : 1e-12),
		abs(dir.z) > 1e-12 ? dir.z : (dir.z < 0.0 ? -1e-12 : 1e-12));
	vec3 invDir = 1.0 / safeDir;
	float best = tMax;
	bool found = false;
	uint instanceCount = min(u_frame.swInfo.x, RT_SW_MAX_INSTANCES);
	uint stack[RT_SW_STACK];

	for (uint i = 0u; i < instanceCount; i++) {
		uvec4 a = u_frame.swInstances[i * 2u];			// nodes, triangles, class, mask
		uvec4 b = u_frame.swInstances[i * 2u + 1u];	// first opaque, first masked, dynamic
		if ((a.w & mask) == 0u) {
			continue;
		}
		bool dynamic = b.z != 0u;
		RtBvhNode node = rt_sw_node(dynamic, a.x);
		if (rt_sw_box(origin, invDir, node, tMin, best) < 0.0) {
			continue;
		}
		uint sp = 0u;
		for (;;) {
			uint count = floatBitsToUint(node.bmax.w);
			uint first = floatBitsToUint(node.bmin.w);
			if (count == 0u) {
				// interior: descend into the nearer child, keep the farther
				RtBvhNode left = rt_sw_node(dynamic, a.x + first);
				RtBvhNode right = rt_sw_node(dynamic, a.x + first + 1u);
				float tl = rt_sw_box(origin, invDir, left, tMin, best);
				float tr = rt_sw_box(origin, invDir, right, tMin, best);
				if (tl >= 0.0 && tr >= 0.0) {
					bool leftFirst = tl <= tr;
					if (sp < RT_SW_STACK) {
						stack[sp++] = leftFirst ? first + 1u : first;
					}
					node = leftFirst ? left : right;
					continue;
				}
				if (tl >= 0.0) {
					node = left;
					continue;
				}
				if (tr >= 0.0) {
					node = right;
					continue;
				}
			} else {
				for (uint k = 0u; k < count; k++) {
					RtBvhTri tri = rt_sw_tri(dynamic, a.y + first + k);
					float t;
					vec2 bary;
					bool front;
					if (!rt_sw_triangle(origin, dir, tri.v0.xyz, tri.v1.xyz, tri.v2.xyz, tMin, best, t, bary, front)) {
						continue;
					}
					uint prim = floatBitsToUint(tri.v0.w);
					bool masked = prim >= b.y;
					uint geometry = masked ? 1u : 0u;
					uint local = prim - (masked ? b.y : b.x);
					if (masked && (!solid || dynamic)) {
						RtTriangle shaded = rt_fetch_triangle(a.z << 20u, geometry, local);
						if ((firstHit || solid) && !rt_casts_shadow(shaded)) continue;
						if (!rt_hit_accepted(shaded, bary, front, dir)) {
							continue;
						}
					}
					best = t;
					found = true;
					hit.hitT = t;
					hit.customIndex = a.z << 20u;
					hit.primitive = local;
					hit.geometry = geometry | (front ? RT_HIT_FRONT_FACING : 0u);
					hit.bary = bary;
					if (firstHit) {
						return true;
					}
				}
			}
			// the next kept node the ray still reaches before its best hit
			bool more = false;
			while (sp > 0u) {
				node = rt_sw_node(dynamic, a.x + stack[--sp]);
				if (rt_sw_box(origin, invDir, node, tMin, best) >= 0.0) {
					more = true;
					break;
				}
			}
			if (!more) {
				break;
			}
		}
	}
	return found;
}

// The nearest hit along the ray, in payload.
bool trace_radiance(vec3 origin, vec3 dir, float tMin, float tMax, uint mask)
{
	payload.hitT = -1.0;
	return rt_sw_trace(origin, dir, tMin, tMax, mask, false, false, payload);
}

// The nearest surface along the ray, in payload, with every surface solid.
bool trace_solid(vec3 origin, vec3 dir, float tMin, float tMax, uint mask)
{
	payload.hitT = -1.0;
	return rt_sw_trace(origin, dir, tMin, tMax, mask, false, true, payload);
}

// 1 when nothing in the mask's instances blocks the segment, else 0; a solid
// ray is blocked by any surface, alpha-tested or not.
// The last shadow ray's blocker (hitT -1 for none), for rtx_rt_debug_light
RtHitPayload g_shadowBlocker;

float rt_trace_shadow_mask(vec3 origin, vec3 dir, float tMax, uint mask, bool solid, out float reactive)
{
	reactive = 0.0;
	g_shadowBlocker.hitT = -1.0;
	if (tMax <= 0.02) {
		return 1.0;
	}
	bool blocked = rt_sw_trace(origin, dir, 0.01, tMax, mask, true, solid, g_shadowBlocker);
	if (blocked) {
		uint cls = instance_class(g_shadowBlocker.customIndex);
		if (cls == RT_CLASS_DYNAMIC || cls == RT_CLASS_BODY || cls == RT_CLASS_WEAPON) reactive = 1.0;
	}
	return blocked ? 0.0 : 1.0;
}

// The light the map's alphashadow and lightfilter surfaces let through along
// the segment (rt_filter.glsl), scaled at every one the filter instance holds.
vec3 rt_trace_filter(vec3 origin, vec3 dir, float tMin, float tMax)
{
	if ((u_frame.flags.x & RT_FRAME_FILTERS) == 0u || !(tMax > tMin)) {
		return vec3(1.0);
	}
	vec3 safeDir = vec3(
		abs(dir.x) > 1e-12 ? dir.x : (dir.x < 0.0 ? -1e-12 : 1e-12),
		abs(dir.y) > 1e-12 ? dir.y : (dir.y < 0.0 ? -1e-12 : 1e-12),
		abs(dir.z) > 1e-12 ? dir.z : (dir.z < 0.0 ? -1e-12 : 1e-12));
	vec3 invDir = 1.0 / safeDir;
	vec3 transmission = vec3(1.0);
	uint instanceCount = min(u_frame.swInfo.x, RT_SW_MAX_INSTANCES);
	uint stack[RT_SW_STACK];

	for (uint i = 0u; i < instanceCount; i++) {
		uvec4 a = u_frame.swInstances[i * 2u];
		uvec4 b = u_frame.swInstances[i * 2u + 1u];
		if ((a.w & RT_MASK_FILTER) == 0u) {
			continue;
		}
		bool dynamic = b.z != 0u;
		RtBvhNode node = rt_sw_node(dynamic, a.x);
		if (rt_sw_box(origin, invDir, node, tMin, tMax) < 0.0) {
			continue;
		}
		uint sp = 0u;
		for (;;) {
			uint count = floatBitsToUint(node.bmax.w);
			uint first = floatBitsToUint(node.bmin.w);
			if (count == 0u) {
				// every hit counts, in any order: visit both children
				RtBvhNode left = rt_sw_node(dynamic, a.x + first);
				RtBvhNode right = rt_sw_node(dynamic, a.x + first + 1u);
				bool reachLeft = rt_sw_box(origin, invDir, left, tMin, tMax) >= 0.0;
				bool reachRight = rt_sw_box(origin, invDir, right, tMin, tMax) >= 0.0;
				if (reachLeft && reachRight) {
					if (sp < RT_SW_STACK) {
						stack[sp++] = first + 1u;
					}
					node = left;
					continue;
				}
				if (reachLeft) {
					node = left;
					continue;
				}
				if (reachRight) {
					node = right;
					continue;
				}
			} else {
				for (uint k = 0u; k < count; k++) {
					RtBvhTri tri = rt_sw_tri(dynamic, a.y + first + k);
					float t;
					vec2 bary;
					bool front;
					if (!rt_sw_triangle(origin, dir, tri.v0.xyz, tri.v1.xyz, tri.v2.xyz, tMin, tMax, t, bary, front)) {
						continue;
					}
					uint prim = floatBitsToUint(tri.v0.w);
					RtTriangle shaded = rt_fetch_triangle(a.z << 20u, 0u, prim - b.x);
					transmission *= rt_filter_transmission(shaded, bary);
					if (all(lessThanEqual(transmission, vec3(0.001)))) {
						return vec3(0.0);
					}
				}
			}
			if (sp == 0u) {
				break;
			}
			node = rt_sw_node(dynamic, a.x + stack[--sp]);
		}
	}
	return transmission;
}

#endif
