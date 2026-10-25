// Direct lighting for the path tracer: q3map's light models evaluated at a
// shading point, with one light chosen per sample by resampled importance
// sampling over the light-grid cell's candidates (Talbot et al. 2005,
// "Importance Resampling for Global Illumination"). Irradiance is in lightmap
// units (see code/rendererrtx/rt/rt_lights.c).
//
// Requires: rt_common.glsl, rt_bindings.glsl, rt_geometry.glsl, a tracing
// backend's trace_radiance, rt_trace_shadow_mask and rt_trace_filter
// (rt_trace_hw.glsl or rt_trace_sw.glsl) and a `g_shadowMask` variable (the
// instance mask shadow rays test).

#ifndef RT_LIGHTING_GLSL
#define RT_LIGHTING_GLSL

#include "rt_light_proposal.glsl"

struct RtLightSample {
	vec3 L;			// toward the light
	float dist;		// shadow ray length
	vec3 E;			// unshadowed irradiance through this sample (lightmap units)
};

// Evaluate one light at P with shading normal N, unless it skips this kind
// of receiver (exclude: RT_LIGHT_NO_SURFACES or RT_LIGHT_NO_MODELS) or lacks
// a bit exclude requires above RT_LIGHT_REQUIRE_SHIFT. For a triangle the
// sample point is drawn uniformly over its area; E is then that sample's
// estimate.
bool rt_light_eval(uint index, vec3 P, vec3 N, uint exclude, inout uint seed, out RtLightSample s)
{
	RtLight light = u_lights.lights[index];
	uint word = uint(light.color.w);
	uint type = word & RT_LIGHT_TYPE_MASK;
	s.L = vec3(0.0, 0.0, 1.0);
	s.dist = 0.0;
	s.E = vec3(0.0);
	if (((word & exclude) | (~word & (exclude >> RT_LIGHT_REQUIRE_SHIFT))) != 0u) {
		return false;
	}

	if (type == RT_LIGHT_TRIANGLE) {
		float u = rand01(seed);
		float v = rand01(seed);
		if (u + v > 1.0) {
			u = 1.0 - u;
			v = 1.0 - v;
		}
		vec3 p = light.position.xyz + u * light.dir.xyz + v * light.extra.xyz;
		vec3 n = safe_normalize(cross(light.dir.xyz, light.extra.xyz), vec3(0.0, 0.0, 1.0));
		vec3 d = p - P;
		float d2 = dot(d, d);
		float dist = sqrt(d2);
		if (dist < 1e-3) {
			return false;
		}
		s.L = d / dist;
		float cosR = dot(N, s.L);
		float cosL = -dot(n, s.L);
		if (light.extra.w > 0.5) {
			cosL = abs(cosL);
		}
		if (cosR <= 0.0 || cosL <= 0.0) {
			return false;
		}
		// radiance * G / pdf, with pdf = 1 / area; d^2 kept above 16 so a
		// point beside an emitter does not collect an unbounded sample
		s.E = light.color.rgb * (cosR * cosL * light.dir.w / max(d2, 16.0));
		s.dist = dist;
		return true;
	}

	vec3 d = light.position.xyz - P;
	float dist = length(d);
	if (dist < 1e-3 || dist > light.position.w) {
		return false;
	}
	s.L = d / dist;
	s.dist = dist;
	float cosR = dot(N, s.L);
	if (cosR <= 0.0) {
		return false;
	}
	// q3map2 and the Wolfenstein compilers light some lights without
	// Lambert's cosine (extra.w); a surface facing away still gets none
	float angle = (light.extra.w > 0.5 && type != RT_LIGHT_SPOT && type != RT_LIGHT_DLIGHT) ? 1.0 : cosR;

	if (type == RT_LIGHT_POINT_LINEAR) {
		// q3map: add = angle * photons * linearScale - dist * fade
		float add = angle * light.extra.y - dist * light.extra.z;
		if (add <= 0.0) {
			return false;
		}
		s.E = light.color.rgb * add;
		return true;
	}
	if (type == RT_LIGHT_DLIGHT) {
		// the raster light pass: colour * (1 - d^2 / r^2) * cos, its falloff
		// eased toward smoothstep by r_dlightFalloff (light_frag.tmpl)
		float r = light.position.w;
		float d2 = dist * dist;
		float cone = 1.0;
		if (light.dir.w > 0.0) {
			// a static map spot (rt_backend.c RT_SpotDlight): raster's
			// distance to its segment, lit from its origin and only inside
			// its cone (renderercommon/tr_spot_cone.h)
			vec3 toPoint = -d;
			float along = dot(toPoint, light.dir.xyz);
			vec3 off = toPoint - light.dir.xyz * clamp(along, 0.0, light.dir.w);
			r = light.extra.z;
			d2 = dot(off, off);
			cone = along / dist * light.extra.x + light.extra.y;
			if (cone <= 0.0) {
				return false;
			}
			cone = min(cone, 1.0);
			cone = cone * cone * (3.0 - 2.0 * cone);
		}
		float falloff = 1.0 - d2 / (r * r);
		if (falloff <= 0.0) {
			return false;
		}
		falloff = mix(falloff, falloff * falloff * (3.0 - 2.0 * falloff), clamp(u_frame.dlightShape.x, 0.0, 1.0));
		falloff *= cone;
		s.E = light.color.rgb * (falloff * cosR);
		// Its shadow is cast from a point on the light's body, a sphere a
		// twentieth of its reach (2 to 12 units: a muzzle flash, a torch's
		// flame, a fireball), so it has the penumbra a light that size casts;
		// its brightness stays raster's, from the centre
		float body = clamp(0.05 * r, 2.0, 12.0);
		vec3 q = light.position.xyz + body * rand_sphere(seed) - P;
		float qd = length(q);
		if (qd > body && dot(N, q) > 0.0) {
			s.L = q / qd;
			s.dist = qd;
		}
		return true;
	}

	float dc = max(dist, light.extra.x);
	s.E = light.color.rgb * (angle / (dc * dc));
	if (type == RT_LIGHT_SPOT) {
		// q3map's cone: radius (radius + 16) / targetDist at each distance,
		// fading over its outer 32 units
		vec3 toPoint = P - light.position.xyz;
		float along = dot(toPoint, light.dir.xyz);
		if (along <= 0.0) {
			return false;
		}
		float radiusAtDist = light.dir.w * along;
		float sampleRadius = length(toPoint - light.dir.xyz * along);
		if (sampleRadius >= radiusAtDist) {
			return false;
		}
		s.E *= clamp((radiusAtDist - sampleRadius) / 32.0, 0.0, 1.0);
	}
	// fade the last fifth of a light's reach instead of cutting it off
	float edge = clamp((light.position.w - dist) / (0.2 * light.position.w), 0.0, 1.0);
	s.E *= edge;
	return true;
}

uvec2 rt_light_cell(vec3 P)
{
	uvec3 dims = u_frame.lightGridDims.xyz;
	if (dims.x == 0u) {
		return uvec2(0u);
	}
	ivec3 c = ivec3(floor((P - u_frame.lightGridOrigin.xyz) * u_frame.lightGridOrigin.w));
	c = clamp(c, ivec3(0), ivec3(dims) - 1);
	uint index = (uint(c.z) * dims.y + uint(c.y)) * dims.x + uint(c.x);
	return u_lightGrid.cells[index];
}

// What a light sample chose, for weighing it against a glossy reflection ray
// (rt_path.glsl): the light (~0u for none), its sample point's distance and
// target, and what rebuilds the resampling: the cell and receivers it chose
// among, how many candidates it drew, their weight sum and random streams.
struct RtLightChoice {
	uint index;
	float dist;			// to the chosen sample point
	float target;		// the chosen sample's
	float weightSum;	// of every candidate
	uvec2 cell;
	uint exclude;
	uint candidates;	// the cell is enumerated when it holds no more
	uint streams;		// rt_light_stream
	uint reuseKey;		// compatible primary receiver, 0 disables proposal replay
	float globalShare;	// mixture weight of the global proposal
	bool enumerate;
};

// Candidate k's random numbers: its pick and its point on the light draw on
// nothing else, so a reflection ray can draw the candidate again.
uint rt_light_stream(uint streams, uint k)
{
	return hash_u32(streams ^ (k * 0x85ebca6bu));
}

#include "rt_light_reuse.glsl"

uint rt_choice_stream(RtLightChoice c, uint k)
{
	return c.reuseKey != 0u && k < 4u ?
		rt_reused_light_stream(c.reuseKey, uint(u_frame.time.y), k) : rt_light_stream(c.streams, k);
}

// Candidate k of a resampling over the cell's lights and the dynamic lights
// after them (total): the light it picks, its own place when they are
// enumerated, and its sample. Its weight is its target, the luminance of its
// irradiance, times the lights it was picked from unless they were
// enumerated. False when it adds nothing.
bool rt_light_candidate(vec3 P, vec3 N, uvec2 cell, uint total, bool enumerate, float globalShare, uint exclude, uint stream,
	uint k, out uint index, out RtLightSample s, out float target, out float weight)
{
	bool global = !enumerate && globalShare > 0.0 && rand01(stream) < globalShare;
	if (global) {
		index = rt_global_light_pick(rand01(stream));
	} else {
		uint pick = enumerate ? k : min(uint(rand01(stream) * float(total)), total - 1u);
		index = pick < cell.y ? u_lightIndices.indices[cell.x + pick] : u_frame.counts.w + (pick - cell.y);
	}
	target = 0.0;
	weight = 0.0;
	if (!rt_light_eval(index, P, N, exclude, stream, s)) {
		return false;
	}
	float p = luminance(s.E);
	if (!(p > 0.0)) {
		return false;
	}
	// Out-of-range, backfacing and excluded lights have zero target. Their
	// proposal never contributes, so avoid its table reads and membership
	// search. Keep the same draws, mixture PDF and reservoir weights.
	// A locally drawn index is already known to be in the list.
	float proposal = enumerate ? 1.0 : (global ? rt_light_proposal_pdf(cell, total, globalShare, index) :
		globalShare * rt_global_light_pdf(index) + (1.0-globalShare) / float(total));
	target = p;
	weight = p / proposal;
	return true;
}

// A resampling's normalizer for a weight sum: the sum itself when the lights
// were enumerated, its average over the candidates otherwise. A light's
// chance of being chosen is its target over the normalizer, exactly when
// enumerated and nearly otherwise.
float rt_light_normalizer(RtLightChoice c, float weightSum)
{
	return c.enumerate ? weightSum : weightSum / float(max(c.candidates, 1u));
}

// A triangle light's target at its point along L at distance dist from a
// receiver with normal N, as rt_light_eval gives it, and the factor carrying
// a density over the light's area to solid angle.
float rt_triangle_target(RtLight light, vec3 N, vec3 L, float dist, out float toSolidAngle)
{
	toSolidAngle = 0.0;
	vec3 n = safe_normalize(cross(light.dir.xyz, light.extra.xyz), vec3(0.0, 0.0, 1.0));
	float cosR = dot(N, L);
	float cosL = -dot(n, L);
	if (light.extra.w > 0.5) {
		cosL = abs(cosL);
	}
	float area = light.dir.w;
	if (cosR <= 0.0 || cosL <= 0.0 || !(area > 0.0)) {
		return 0.0;
	}
	float d2 = dist * dist;
	toSolidAngle = d2 / (cosL * area);
	return luminance(light.color.rgb * (cosR * cosL * area / max(d2, 16.0)));
}

// The light sampler's density, in solid angle, of the triangle light sample
// it chose, along L: the light's chance times one over its area.
float rt_chosen_light_pdf(RtLightChoice c, vec3 N, vec3 L)
{
	float toSolidAngle;
	rt_triangle_target(u_lights.lights[c.index], N, L, c.dist, toSolidAngle);
	float normalizer = rt_light_normalizer(c, c.weightSum);
	return normalizer > 0.0 ? (c.target / normalizer) * toSolidAngle : 0.0;
}

// The same density at a point of triangle light `index` along L at distance
// dist from P, the resampling taken with that point in place of the
// candidate that could have drawn it (the light's own when the lights were
// enumerated, the first otherwise) and the rest as drawn. A light sample's
// share and a reflection ray's then sum to one on every path (the balance
// heuristic; Veach 1997), though the normalizer is itself random. Zero where
// the sampler never reaches: a light the cell does not list or the receiver
// skips.
float rt_light_pdf_at(RtLightChoice c, uint index, vec3 P, vec3 N, vec3 L, float dist)
{
	uint total = c.cell.y + u_frame.flags.w;
	if (c.candidates == 0u) {
		return 0.0;
	}
	RtLight light = u_lights.lights[index];
	if ((uint(light.color.w) & c.exclude) != 0u) {
		return 0.0;
	}
	float toSolidAngle;
	float target = rt_triangle_target(light, N, L, dist, toSolidAngle);
	if (!(target > 0.0)) {
		return 0.0;
	}
	bool enumerate = c.enumerate;
	float proposal = rt_light_proposal_pdf(c.cell, total, c.globalShare, index);
	uint slot = enumerate ? rt_local_light_slot(c.cell, index) : 0u;
	if (slot == ~0u || !(proposal > 0.0)) {
		return 0.0;
	}
	uint picked;
	RtLightSample drawn;
	float drawnTarget, drawnWeight;
	rt_light_candidate(P, N, c.cell, total, enumerate, c.globalShare, c.exclude, rt_choice_stream(c, slot), slot, picked,
		drawn, drawnTarget, drawnWeight);
	float weightSum = max(c.weightSum - drawnWeight, 0.0) + (enumerate ? target : target / proposal);
	float normalizer = rt_light_normalizer(c, weightSum);
	return normalizer > 0.0 ? (target / normalizer) * toSolidAngle : 0.0;
}

// One resampled light sample, with its shadow ray. Returns the irradiance
// estimate (lightmap units) and the chosen direction for specular shading.
// exclude names the receivers' kind the lights skip (rt_light_eval);
// dynamicOnly leaves the map's own lights out (a lightmap holds them), and
// the choice records an empty cell, so none is weighed against a reflection.
vec3 rt_sample_lights(vec3 P, vec3 N, vec3 Ng, uint maxCandidates, uint exclude, bool dynamicOnly, inout uint seed,
	out vec3 Lsel, out bool found, out RtLightChoice choice, out float reactive)
{
	// A wall often lies on a cell boundary, and the cell behind it lists the
	// lights of the space behind it (the grid culls by PVS). Look one unit
	// out on the lit side, where the grid samples its cells too.
	uvec2 cell = dynamicOnly ? uvec2(0u) : rt_light_cell(P + Ng);
	uint dlights = u_frame.flags.w;
	uint total = cell.y + dlights;
	float globalShare = !dynamicOnly && u_frame.counts.w > 0u ? (total > 0u ? 0.25 : 1.0) : 0.0;
	uint M = globalShare > 0.0 ? maxCandidates : min(total, maxCandidates);
	bool enumerate = globalShare == 0.0 && M == total;
	reactive = 0.0;
	found = false;
	Lsel = N;
	choice.index = ~0u;
	choice.dist = 0.0;
	choice.target = 0.0;
	choice.weightSum = 0.0;
	choice.cell = cell;
	choice.exclude = exclude;
	choice.candidates = M;
	choice.streams = hash_u32(seed ^ 0xa511e9b3u);
	choice.reuseKey = 0u;
	if (g_lightReuseMaterial != 0u && M > 4u && !enumerate &&
		(u_frame.flags.x & RT_FRAME_LIGHT_REUSE) != 0u) {
		// Material, geometric neighbourhood, normal and proposal-table identity
		// must agree to share streams. No data from a disappeared instance or
		// previous light table is read. Small cells limit spatial correlation.
		uvec3 pKey = uvec3(ivec3(floor(P * 0.25)));
		uvec3 nKey = uvec3(ivec3(floor(N * 16.0)));
		choice.reuseKey = hash_u32(pKey.x ^ hash_u32(pKey.y) ^ hash_u32(pKey.z + 19u) ^
			hash_u32(nKey.x + nKey.y * 37u + nKey.z * 1369u) ^
			hash_u32(g_lightReuseMaterial ^ exclude) ^ hash_u32(cell.x + total * 257u) ^
			hash_u32(u_frame.counts.w + u_frame.flags.w * 65537u)) | 1u;
	}
	choice.globalShare = globalShare;
	choice.enumerate = enumerate;
	if (M == 0u) {
		return vec3(0.0);
	}

	float wSum = 0.0;
	float pSel = 0.0;
	uint chosenIndex = 0u;
	RtLightSample chosen;
	chosen.E = vec3(0.0);
	chosen.L = N;
	chosen.dist = 0.0;

	for (uint k = 0u; k < M; k++) {
		uint index;
		RtLightSample s;
		float p, w;
		if (!rt_light_candidate(P, N, cell, total, enumerate, globalShare, exclude, rt_choice_stream(choice, k), k, index, s,
			p, w)) {
			continue;
		}
		wSum += w;
		if (rand01(seed) * wSum < w) {
			chosen = s;
			pSel = p;
			chosenIndex = index;
		}
	}
	// rtx_rt_debug_light: that light alone, whatever the candidates were
	uint debugLight = dynamicOnly ? 0u : u_frame.materialLights.z;
	if (debugLight != 0u) {
		RtLightSample s;
		wSum = 0.0;
		pSel = 0.0;
		if (debugLight - 1u < u_frame.materialLights.x && rt_light_eval(debugLight - 1u, P, N, exclude, seed, s)) {
			chosen = s;
			chosenIndex = debugLight - 1u;
			wSum = 1.0;
			pSel = 1.0;
			enumerate = true;
		}
	}
	choice.weightSum = wSum;
	choice.enumerate = enumerate;
	if (!(wSum > 0.0) || !(pSel > 0.0)) {
		return vec3(0.0);
	}

	float normalizer = enumerate ? wSum : wSum / float(M);
	vec3 estimate = chosen.E * (normalizer / pSel);
	choice.index = chosenIndex;
	choice.dist = chosen.dist;
	choice.target = pSel;
	vec3 origin = P + Ng * max(0.05, 1e-6 * length(P));
	// the map's lights pass what q3map let them pass and nothing more: its
	// light rays stop at an alpha-tested surface, holes and all, unless it
	// filters light (id's light_trace.c SetFacetFilter, q3map2's
	// TraceTriangle). A dynamic light (after the static ones) is blocked by
	// every surface, through its alpha test.
	bool dynamicLight = chosenIndex >= u_frame.counts.w;
	uint mask = dynamicLight ? (g_shadowMask | RT_MASK_NOSHADOW) : g_shadowMask;
	float visibility = rt_trace_shadow_mask(origin, chosen.L, chosen.dist - 0.1, mask, !dynamicLight, reactive);
	// Moving lamps and flashes change illumination even without a moving
	// blocker. Carry their response flag into the denoiser and its short
	// departure window, so retaining quiet history cannot smear a flash.
	if (dynamicLight) reactive = 1.0;
	vec3 transmission = vec3(visibility);
	// ... and through its alphashadow and lightfilter surfaces, filtered
	vec3 filtered = vec3(1.0);
	if ((visibility > 0.0 || debugLight != 0u) && !dynamicLight) {
		filtered = rt_trace_filter(origin, chosen.L, 0.01, chosen.dist - 0.1);
		transmission *= filtered;
	}
	found = visibility > 0.0;
	Lsel = chosen.L;
	if (debugLight != 0u) {
		// its light unshadowed (mode 1), whether its shadow ray got through
		// (2: white; blocked, the blocker's instance class: world red, sky and
		// portals green, models blue, weapon yellow, the viewer's body
		// magenta, black when the backend does not say), what the light
		// filters passed (3), or as lit (0)
		uint mode = u_frame.materialLights.w;
		uint cls = instance_class(g_shadowBlocker.customIndex);
		vec3 blocked = g_shadowBlocker.hitT < 0.0 ? vec3(0.0) :
			(cls == RT_CLASS_WORLD ? vec3(4.0, 0.0, 0.0) : cls == RT_CLASS_WORLD_SPECIAL ? vec3(0.0, 4.0, 0.0) :
			cls == RT_CLASS_DYNAMIC ? vec3(0.0, 0.0, 4.0) : cls == RT_CLASS_WEAPON ? vec3(4.0, 4.0, 0.0) :
			cls == RT_CLASS_BODY ? vec3(4.0, 0.0, 4.0) : vec3(0.0, 4.0, 4.0));
		return mode == 1u ? chosen.E : (mode == 2u ? (visibility > 0.0 ? vec3(4.0) : blocked) :
			(mode == 3u ? filtered : estimate * transmission));
	}
	return estimate * transmission;
}

// How much of the sun reaches origin along L. q3map lights a point with the
// sun only where the ray toward it stops first in a sky brush (SunToPoint):
// the sky ends the ray, so nothing beyond it (another area, a sky portal's
// scene) shadows the point, and a ray that meets no sky gets no sun. The
// nearest of the shadow casters, solid as the compiler saw them, and the sky
// surfaces decides; the alphashadow and lightfilter surfaces before the sky
// filter what is left.
vec3 rt_sun_visibility(vec3 origin, vec3 L, out float reactive)
{
	reactive = 0.0;
	if (!trace_solid(origin, L, 0.01, 1e5, g_shadowMask | RT_MASK_SPECIAL)) {
		return vec3(0.0);
	}
	float skyT = payload.hitT;
	RtTriangle tri = rt_fetch_triangle(payload.customIndex, payload.geometry & 0xFFFFu, payload.primitive);
	if ((rt_triangle_material(tri).flags & RT_MF_SKY) == 0u) {
		if (tri.dynamic) reactive = 1.0;
		return vec3(0.0);
	}
	return rt_trace_filter(origin, L, 0.01, skyT);
}

// q3map2's sky light (q3map_skyLight value iterations): its value shared by a
// dome of equal suns (light.c CreateSkyLights) - rows of 4 (iterations - 1)
// at elevations 90 / iterations degrees apart from half that, each row turned
// a sun's step / (iterations - 1) from the one below, and one overhead - each
// lighting a point as the sun does. One sun per sample, chosen uniformly.
//
// Or the sky itself: a direction drawn with Lambert's cosine around N, and
// the sky's colour there (rt_sky.glsl) where the ray reaches the sky. With
// that density the estimate of the diffuse light (irradiance / pi, the units
// every light here is kept in) is the radiance found.
vec3 rt_sample_sky_light(vec3 P, vec3 N, vec3 Ng, inout uint seed, out vec3 Lsky)
{
	Lsky = vec3(0.0, 0.0, 1.0);
	bool skyRadiance = (u_frame.flags.x & RT_FRAME_SKY_RADIANCE) != 0u;
	vec3 L = vec3(0.0, 0.0, 1.0);
	vec3 light;
	if (skyRadiance) {
		float u = rand01(seed);
		float phi = 2.0 * PI * rand01(seed);
		vec3 t, b;
		basis_from_normal(N, t, b);
		float r = sqrt(u);
		L = normalize(t * (r * cos(phi)) + b * (r * sin(phi)) + N * sqrt(max(1.0 - u, 0.0)));
		if (dot(L, Ng) <= 0.0) {
			return vec3(0.0);
		}
		light = vec3(1.0);
	} else {
		float iterations = u_frame.skyLight.w;
		if (iterations < 2.0) {
			return vec3(0.0);
		}
		uint rows = uint(iterations) - 1u;
		uint perRow = rows * 4u;
		uint suns = perRow * rows + 1u;
		uint k = min(uint(rand01(seed) * float(suns)), suns - 1u);
		if (k < suns - 1u) {
			uint row = k / perRow;
			uint column = k - row * perRow;
			float elevation = (0.5 * PI / iterations) * (float(row) + 0.5);
			float angle = (2.0 * PI / float(perRow)) * (float(column) + float(row) / float(rows));
			L = vec3(cos(angle) * cos(elevation), sin(angle) * cos(elevation), sin(elevation));
		}
		float cosR = dot(N, L);
		if (cosR <= 0.0) {
			return vec3(0.0);
		}
		light = u_frame.skyLight.rgb * cosR;
	}
	Lsky = L;
	// Visibility is shared with the sun sampler in shade_direct. Keeping
	// the expensive traversal at one call site matters to 32-bit drivers.
	if (skyRadiance) {
		light = u_frame.skyLight.w > 0.5 ? u_frame.skyLight.rgb * u_frame.params0.w :
			rt_sky_radiance(L) * u_frame.skyLight.rgb;
	}
	return light;
}

// The sun: q3map_sun's light where the path toward it reaches the sky.
vec3 rt_sample_sun(vec3 P, vec3 N, vec3 Ng, inout uint seed, out vec3 Lsun)
{
	Lsun = u_frame.sunDirection.xyz;
	if ((u_frame.flags.x & RT_FRAME_SUN) == 0u) {
		return vec3(0.0);
	}
	float u = rand01(seed);
	float v = rand01(seed);
	vec3 L;
	float deviance = u_frame.sunShape.x;
	if (deviance > 0.0) {
		// q3map2's q3map_sunExt: one of its suns keeps the sun's direction,
		// the others spread uniformly over a disc of the deviance in azimuth
		// and elevation (CreateSunLight)
		L = u_frame.sunDirection.xyz;
		if (rand01(seed) >= u_frame.sunShape.y) {
			float r = deviance * sqrt(u);
			float horizontal = length(L.xy);
			float azimuth = (horizontal > 1e-6 ? atan(L.y, L.x) : 0.0) + r * cos(2.0 * PI * v);
			float elevation = atan(L.z, horizontal) + r * sin(2.0 * PI * v);
			L = vec3(cos(azimuth) * cos(elevation), sin(azimuth) * cos(elevation), sin(elevation));
		}
	} else {
		// sample a small cone for a soft penumbra
		float cosMax = u_frame.sunDirection.w;
		float cosT = mix(cosMax, 1.0, u);
		float sinT = sqrt(max(1.0 - cosT * cosT, 0.0));
		vec3 t, b;
		basis_from_normal(u_frame.sunDirection.xyz, t, b);
		L = normalize(t * (cos(2.0 * PI * v) * sinT) + b * (sin(2.0 * PI * v) * sinT) +
			u_frame.sunDirection.xyz * cosT);
	}
	float cosR = dot(N, L);
	if (cosR <= 0.0) {
		return vec3(0.0);
	}
	Lsun = L;
	return u_frame.sunColor.rgb * cosR;
}

// The $dlight image RTCW's light materials project (tr_image.c
// R_CreateDlightImage): texel (x, y) of 16 across holds 4000 over its squared
// distance in texels from the centre, over 255, full within about four texels
// and nothing past about seven. Clamped to its edge, which is dark.
float rt_dlight_texel(ivec2 t)
{
	vec2 d = vec2(7.5) - vec2(clamp(t, ivec2(0), ivec2(15)));
	float b = floor(4000.0 / dot(d, d));
	return b > 255.0 ? 1.0 : (b < 75.0 ? 0.0 : b / 255.0);
}

// Its bilinear sample at st.
float rt_dlight_image(vec2 st)
{
	vec2 p = clamp(st, vec2(-1.0), vec2(2.0)) * 16.0 - 0.5;
	vec2 i = floor(p);
	vec2 f = p - i;
	ivec2 t = ivec2(i);
	return mix(mix(rt_dlight_texel(t), rt_dlight_texel(t + ivec2(1, 0)), f.x),
		mix(rt_dlight_texel(t + ivec2(0, 1)), rt_dlight_texel(t + ivec2(1, 1)), f.x), f.y);
}

// What RTCW's light materials make of a world or brush-model surface at P
// (tr_rtcw_light_backend.h): each projects the $dlight image along world z,
// across twice its radius, fading it past half its radius above or below
// (RTCW_ProjectLightVertex); each of its stages then multiplies what the
// surface drew by one minus that (negdlightshader: GL_ZERO
// GL_ONE_MINUS_SRC_COLOR) or one plus it (flamedlightshader: GL_DST_COLOR
// GL_ONE, twice). extra.x and extra.y count those stages.
vec3 rt_material_light_factor(vec3 P)
{
	vec3 factor = vec3(1.0);
	uint first = u_frame.materialLights.x;
	uint count = min(u_frame.materialLights.y, 32u);
	for (uint i = 0u; i < count; i++) {
		RtLight light = u_lights.lights[first + i];
		float r = light.position.w;
		vec3 d = light.position.xyz - P;
		float height = abs(d.z);
		if (!(r > 0.0) || height > r) {
			continue;
		}
		float strength = height < 0.5 * r ? 1.0 : 2.0 * (r - height) / r;
		vec3 src = clamp(light.color.rgb * strength, 0.0, 1.0) * rt_dlight_image(vec2(0.5) + d.xy / r);
		for (uint k = 0u; k < min(uint(light.extra.x), 8u); k++) {
			factor *= vec3(1.0) + src;
		}
		for (uint k = 0u; k < min(uint(light.extra.y), 8u); k++) {
			factor *= vec3(1.0) - src;
		}
	}
	return factor;
}

#endif
