// The path tracer's per-pixel work, shared by the ray-tracing pipeline
// (rt_main.rgen) and the compute backend (rt_trace.comp): primary
// visibility, material evaluation, direct light from the map's own lights,
// one bounce of indirect light, and the signals the denoiser and composite
// need.
//
// Shading happens in the raster renderer's framebuffer units: a material's
// stages give its reflectance with the lighting term white, irradiance is in
// lightmap units, and reflectance * irradiance is the colour the raster
// renderer would draw with that irradiance in its lightmap.
//
// Requires: rt_common.glsl, rt_bindings.glsl, rt_material.glsl,
// rt_geometry.glsl and a tracing backend (rt_trace_hw.glsl or
// rt_trace_sw.glsl) providing payload, trace_radiance and
// rt_trace_shadow_mask.

#ifndef RT_PATH_GLSL
#define RT_PATH_GLSL

layout(set = 0, binding = 12, rgba16f) uniform image2D u_outColor;		// diffuse irradiance
layout(set = 0, binding = 13, rgba16f) uniform image2D u_outSpecular;	// specular radiance
layout(set = 0, binding = 14, rgba16f) uniform image2D u_outReflectance;
layout(set = 0, binding = 15, rgba32f) uniform image2D u_outNormalDepth;
layout(set = 0, binding = 16, rgba16f) uniform image2D u_outEmission;	// alpha: the view fog's visibility
layout(set = 0, binding = 17, rgba32f) uniform image2D u_outMotion;
layout(set = 0, binding = 18, r32ui) uniform uimage2D u_outInfo;

// Shadow rays from the first-person weapon ignore the viewer's own body,
// which the weapon sits inside.
uint g_shadowMask = RT_MASK_SHADOW;

// the sky's light samples (rt_lighting.glsl) take the sky's own colour
#include "rt_sky.glsl"
#include "rt_lighting.glsl"

// A glossy reflection that meets a q3map surface light is weighed against the
// light sampler, which reaches the same light (multiple importance sampling,
// balance heuristic; Veach 1997): the primary hit's light sample and the
// point, normals, view direction and GGX alpha its reflection leaves from.
bool g_misLights = false;	// the primary hit sampled lights for a reflecting point
bool g_mis = false;			// and its reflection ray is being traced
RtLightChoice g_misChoice;
vec3 g_misP;
vec3 g_misN;
vec3 g_misNg;
vec3 g_misV;
float g_misAlpha;
// the raster pass's fog equation (RTCW's and ET's global fog, Alice's far plane)
#include "../../renderercommon/rtcw_fog.glsl"

vec3 camera_ray(vec2 pixel, vec2 size)
{
	vec2 ndc = (pixel / size) * 2.0 - 1.0;
	ndc.y = -ndc.y;
	vec3 dir = u_frame.camForward.xyz +
		ndc.x * u_frame.camOrigin.w * u_frame.camRight.xyz +
		ndc.y * u_frame.camForward.w * u_frame.camUp.xyz;
	return normalize(dir);
}

// camera_ray's change per pixel along x and y, for its normalised result dir
void camera_ray_differentials(vec3 dir, vec2 size, out vec3 dDdx, out vec3 dDdy)
{
	vec3 ddx = u_frame.camRight.xyz * (2.0 * u_frame.camOrigin.w / size.x);
	vec3 ddy = -u_frame.camUp.xyz * (2.0 * u_frame.camForward.w / size.y);
	// the unnormalised direction's length is 1 / cos(angle to forward)
	float invLength = max(dot(dir, u_frame.camForward.xyz), 1e-3);
	dDdx = (ddx - dir * dot(dir, ddx)) * invLength;
	dDdy = (ddy - dir * dot(dir, ddy)) * invLength;
}

bool project_previous(vec3 worldPos, out vec2 prevUv)
{
	vec3 d = worldPos - u_frame.prevCamOrigin.xyz;
	float z = dot(d, u_frame.prevCamForward.xyz);
	if (z <= 1e-3) {
		prevUv = vec2(-1.0);
		return false;
	}
	float x = dot(d, u_frame.prevCamRight.xyz) / (z * max(u_frame.prevCamOrigin.w, 1e-4));
	float y = dot(d, u_frame.prevCamUp.xyz) / (z * max(u_frame.prevCamForward.w, 1e-4));
	prevUv = vec2(x * 0.5 + 0.5, 0.5 - y * 0.5);
	return true;
}

// Standard GGX / Smith terms (Walter et al. 2007).
float ggx_d(float NdotH, float alpha)
{
	float a2 = alpha * alpha;
	float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / max(PI * d * d, 1e-8);
}

float smith_g1(float NdotX, float alpha)
{
	float a2 = alpha * alpha;
	return 2.0 * NdotX / max(NdotX + sqrt(a2 + (1.0 - a2) * NdotX * NdotX), 1e-6);
}

// Specular reflectance for a light direction, in the same units as the
// diffuse term (which folds Lambert's 1 / pi into the light).
vec3 specular_weight(vec3 N, vec3 V, vec3 L, float roughness, vec3 F0)
{
	float NdotL = dot(N, L);
	float NdotV = dot(N, V);
	if (NdotL <= 0.0 || NdotV <= 0.0) {
		return vec3(0.0);
	}
	vec3 H = normalize(V + L);
	float alpha = max(roughness * roughness, 0.002);
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(V, H), 0.0), 5.0);
	float D = ggx_d(max(dot(N, H), 0.0), alpha);
	float G = smith_g1(NdotL, alpha) * smith_g1(NdotV, alpha);
	return PI * F * (D * G / (4.0 * NdotL * NdotV));
}

// The glossy reflection sampler's density, in solid angle, of direction L:
// half-vectors drawn with density D(H) (N.H), reflected into L. None below
// the shading or geometric horizon, where no reflection ray is traced.
float ggx_reflection_pdf(vec3 N, vec3 Ng, vec3 V, vec3 L, float alpha)
{
	if (dot(N, L) <= 0.0 || dot(Ng, L) <= 0.0) {
		return 0.0;
	}
	vec3 H = normalize(V + L);
	float VdotH = dot(V, H);
	float NdotH = dot(N, H);
	if (VdotH <= 0.0 || NdotH <= 0.0) {
		return 0.0;
	}
	return ggx_d(NdotH, alpha) * NdotH / (4.0 * VdotH);
}

// The light sample's share of its specular light where a glossy reflection is
// traced too. Against a q3map surface light the reflection can meet, the
// balance heuristic; a sky's emitters are left to the reflection alone, which
// sees the sky itself.
float rt_light_weight(RtLightChoice choice, vec3 N, vec3 Ng, vec3 V, vec3 L, float alpha)
{
	if (choice.index == ~0u) {
		return 1.0;
	}
	uint word = uint(u_lights.lights[choice.index].color.w);
	if ((word & RT_LIGHT_TYPE_MASK) != RT_LIGHT_TRIANGLE) {
		return 1.0;
	}
	if ((word & RT_LIGHT_SKY) != 0u) {
		return 0.0;
	}
	if ((word & RT_LIGHT_TRACED) == 0u) {
		return 1.0;
	}
	float pLight = rt_chosen_light_pdf(choice, N, L);
	float pReflection = ggx_reflection_pdf(N, Ng, V, L, alpha);
	return pLight + pReflection > 0.0 ? pLight / (pLight + pReflection) : 1.0;
}

vec3 sample_cosine(vec3 N, inout uint seed)
{
	float u = rand01(seed);
	float v = rand01(seed);
	float r = sqrt(u);
	float phi = 2.0 * PI * v;
	vec3 t, b;
	basis_from_normal(N, t, b);
	return normalize(t * (r * cos(phi)) + b * (r * sin(phi)) + N * sqrt(max(1.0 - u, 0.0)));
}

struct ShadeResult {
	vec3 irradiance;	// diffuse, lightmap units
	vec3 specular;		// radiance
};

// The reflectance model at a hit: the stages' colour and the material's
// defaults, or a texture pack's companion maps where it has them
// (rt_material.c).
struct RtShading {
	vec3 N;				// shading normal, facing the ray
	vec3 diffuse;		// the reflectance the lighting term scales
	vec3 F0;			// normal-incidence specular reflectance
	float roughness;	// perceptual; the GGX alpha is its square
};

// The companion maps sample through the stage whose texture they belong to,
// so they follow its coordinates and modifiers. A normal map's red channel
// leans the normal toward growing s and its green toward growing t, its z
// the rest (OpenJK rend2's CalcNormal); the specular colour of a specGloss
// map sets F0 and its gloss 1 - roughness; a metal (rmo and orm maps) takes
// its colour as F0 and keeps no diffuse reflectance.
RtShading rt_shading(RtMaterial material, RtTriangle tri, RtHitAttribs h, RtSurface surf, vec3 geometric,
	float lodBias)
{
	RtShading sh;
	sh.N = h.normal;
	sh.diffuse = surf.reflectance;
	sh.F0 = vec3(material.params.z);
	sh.roughness = material.params.x;
	if ((material.flags & RT_MF_MATERIAL_MAPS) == 0u || !surf.lit || material.maps.w >= material.stageCount) {
		return sh;
	}
	RtStage s = u_stages.stages[material.firstStage + material.maps.w];
	s.state.z = (s.state.z & ~0xFu) | 1u;	// one image
	float time = rt_material_time(material, h);

	if (material.maps.x < RT_MAX_TEXTURES) {
		// where along the triangle the stage's s and t grow, through its own
		// coordinate generator and modifiers (Lengyel's tangent frame)
		vec3 e1 = tri.v1.xyz.xyz - tri.v0.xyz.xyz;
		vec3 e2 = tri.v2.xyz.xyz - tri.v0.xyz.xyz;
		RtHitAttribs h1 = h;
		RtHitAttribs h2 = h;
		h1.position += e1;
		h1.uv += tri.v1.texCoord - tri.v0.texCoord;
		h2.position += e2;
		h2.uv += tri.v2.texCoord - tri.v0.texCoord;
		vec2 st = rt_stage_texcoords(s, h, time);
		vec2 d1 = rt_stage_texcoords(s, h1, time) - st;
		vec2 d2 = rt_stage_texcoords(s, h2, time) - st;
		float det = d1.x * d2.y - d2.x * d1.y;
		if (abs(det) > 1e-12) {
			vec3 sdir = (e1 * d2.y - e2 * d1.y) / det;
			vec3 tdir = (e2 * d1.x - e1 * d2.x) / det;
			vec3 T = sdir - sh.N * dot(sh.N, sdir);
			float lengthSq = dot(T, T);
			if (lengthSq > 1e-20) {
				T *= inversesqrt(lengthSq);
				vec3 B = cross(sh.N, T);
				if (dot(B, tdir) < 0.0) {
					B = -B;
				}
				s.images.x = material.maps.x | (RT_NO_IMAGE << 16u);
				vec2 xy = rt_stage_texel(s, h, time, lodBias).rg * 2.0 - 1.0;
				float z = sqrt(max(1.0 - dot(xy, xy), 0.0));
				vec3 N = safe_normalize(T * xy.x + B * xy.y + sh.N * z, sh.N);
				// no further than just above the surface's own horizon
				float below = 0.05 - dot(N, geometric);
				if (below > 0.0) {
					N = safe_normalize(N + geometric * below, sh.N);
				}
				sh.N = N;
			}
		}
	}

	if (material.maps.y < RT_MAX_TEXTURES) {
		s.images.x = material.maps.y | (RT_NO_IMAGE << 16u);
		vec4 texel = rt_stage_texel(s, h, time, lodBias);
		if (material.maps.z == RT_MATERIAL_MAP_SPECGLOSS) {
			sh.F0 = texel.rgb;
			sh.roughness = 1.0 - texel.a;
		} else {
			bool rmo = material.maps.z == RT_MATERIAL_MAP_RMO;
			float metallic = clamp(rmo ? texel.g : texel.b, 0.0, 1.0);
			sh.roughness = rmo ? texel.r : texel.g;
			sh.F0 = mix(vec3(0.04), min(surf.reflectance, vec3(1.0)), metallic);
			sh.diffuse = surf.reflectance * (1.0 - metallic);
		}
	}
	sh.roughness = clamp(sh.roughness, 0.0, 1.0);
	return sh;
}

// The surface's light already sits in its lightmap or vertex colours, or in
// a model's light as the raster pass drew it.
bool baked_light(RtMaterial material, RtSurface surf, bool bakedLighting)
{
	return bakedLighting && ((material.flags & (RT_MF_LIGHTMAPPED | RT_MF_VERTEX_LIT)) != 0u || surf.bakedEntity != 0u);
}

// q3map2's _minlight family raises each channel of the light a surface
// receives: lightmap luxels, vertex colours and the models' light grid.
vec3 apply_min_light(RtMaterial material, vec3 irradiance)
{
	if ((material.flags & RT_MF_LIGHTMAPPED) != 0u) {
		return max(irradiance, u_frame.minLight[0].rgb);
	}
	if ((material.flags & RT_MF_VERTEX_LIT) != 0u) {
		return max(irradiance, u_frame.minLight[1].rgb);
	}
	if ((material.flags & RT_MF_ENTITY_LIT) != 0u) {
		return max(irradiance, u_frame.minLight[2].rgb);
	}
	return irradiance;
}

// Direct light at a surface point, split into diffuse irradiance and
// specular radiance.
// reflections: a glossy reflection ray is traced from P as well, so the
// light sample shares its specular light with it (rt_light_weight).
ShadeResult shade_direct(RtMaterial material, RtSurface surf, RtShading sh, vec3 P, vec3 Ng, vec3 V,
	uint candidates, bool bakedLighting, bool reflections, bool oneSample, inout uint seed)
{
	ShadeResult r;
	r.irradiance = vec3(0.0);
	r.specular = vec3(0.0);
	if (!surf.lit) {
		return r;
	}

	vec3 N = sh.N;
	vec3 F0 = sh.F0;
	float roughness = max(sh.roughness, u_frame.params0.z);
	// models take the compilers' grid lights, world surfaces their lightmap
	// lights; a surface the raster light pass skips takes no dynamic light
	uint exclude = (material.flags & RT_MF_ENTITY_LIT) != 0u ? RT_LIGHT_NO_MODELS : RT_LIGHT_NO_SURFACES;
	if ((material.flags & RT_MF_NODLIGHT) != 0u) {
		exclude |= RT_LIGHT_DYNAMIC;
	}

	if (baked_light(material, surf, bakedLighting)) {
		// the lightmap already holds this surface's static light; the dynamic
		// lights are traced on top of it, unless a model's raster light holds
		// them too (the legacy dynamic-light modes)
		r.irradiance = surf.baked;
		if (surf.bakedEntity == 2u) {
			exclude |= RT_LIGHT_DYNAMIC;
		}
		if (u_frame.flags.w > 0u && (exclude & RT_LIGHT_DYNAMIC) == 0u) {
			vec3 L;
			bool found;
			RtLightChoice choice;
			vec3 E = rt_sample_lights(P, N, Ng, candidates, exclude, true, seed, L, found, choice);
			r.irradiance += E;
			if (found && (u_frame.flags.x & RT_FRAME_SPECULAR) != 0u) {
				r.specular += E * specular_weight(N, V, L, roughness, F0);
			}
		}
	} else {
		// One sample, where oneSample asks (a bounce's point, as Q2RTX samples
		// one light there): the emitters, the sun or the sky, drawn evenly
		// among those that can light it, the one drawn counted for all; each
		// sampler keeps its own shadow ray. Otherwise all three.
		uint pick = 7u;
		float share = 1.0;
		if (oneSample) {
			bool emitters = u_frame.counts.z > 0u;
			bool sun = (u_frame.flags.x & RT_FRAME_SUN) != 0u && dot(N, u_frame.sunDirection.xyz) > 0.0;
			bool sky = (u_frame.flags.x & RT_FRAME_SKY_RADIANCE) != 0u || u_frame.skyLight.w >= 2.0;
			uint count = (emitters ? 1u : 0u) + (sun ? 1u : 0u) + (sky ? 1u : 0u);
			if (count == 0u) {
				r.irradiance += u_frame.ambient.rgb;
				return r;
			}
			uint k = min(uint(rand01(seed) * float(count)), count - 1u);
			uint options = (emitters ? 1u : 0u) | (sun ? 2u : 0u) | (sky ? 4u : 0u);
			for (pick = options & (~options + 1u); k > 0u; k--) {
				options &= ~pick;
				pick = options & (~options + 1u);
			}
			share = float(count);
		}
		vec3 L = vec3(0.0, 0.0, 1.0);
		bool found = false;
		RtLightChoice choice;
		choice.index = ~0u;
		vec3 E = (pick & 1u) != 0u ?
			rt_sample_lights(P, N, Ng, candidates, exclude, false, seed, L, found, choice) * share : vec3(0.0);
		r.irradiance += E;
		if (found && (u_frame.flags.x & RT_FRAME_SPECULAR) != 0u) {
			float share = reflections ? rt_light_weight(choice, N, Ng, V, L, roughness * roughness) : 1.0;
			r.specular += E * specular_weight(N, V, L, roughness, F0) * share;
		}
		if (reflections) {
			g_misLights = true;
			g_misChoice = choice;
			g_misP = P;
			g_misN = N;
			g_misNg = Ng;
			g_misV = V;
			g_misAlpha = roughness * roughness;
		}
		vec3 Lsun = u_frame.sunDirection.xyz;
		vec3 Esun = (pick & 2u) != 0u ? rt_sample_sun(P, N, Ng, seed, Lsun) * share : vec3(0.0);
		r.irradiance += Esun;
		if ((u_frame.flags.x & RT_FRAME_SPECULAR) != 0u) {
			r.specular += Esun * specular_weight(N, V, Lsun, roughness, F0);
		}
		vec3 Lsky = vec3(0.0, 0.0, 1.0);
		vec3 Esky = (pick & 4u) != 0u ? rt_sample_sky_light(P, N, Ng, seed, Lsky) * share : vec3(0.0);
		r.irradiance += Esky;
		// like a sky's emitters, left to a traced reflection
		if ((u_frame.flags.x & RT_FRAME_SPECULAR) != 0u && !reflections) {
			r.specular += Esky * specular_weight(N, V, Lsky, roughness, F0);
		}
		r.irradiance += u_frame.ambient.rgb;
	}
	return r;
}

void write_empty(ivec2 pixel, uint info)
{
	imageStore(u_outColor, pixel, vec4(0.0));
	imageStore(u_outSpecular, pixel, vec4(0.0));
	imageStore(u_outReflectance, pixel, vec4(0.0));
	imageStore(u_outNormalDepth, pixel, vec4(0.0, 0.0, 1e30, 1.0));
	imageStore(u_outEmission, pixel, vec4(0.0));
	imageStore(u_outMotion, pixel, vec4(0.0));
	imageStore(u_outInfo, pixel, uvec4(info, 0u, 0u, 0u));
}

void orient_normals(RtTriangle tri, vec3 dir, inout RtHitAttribs h, out vec3 geometric)
{
	geometric = rt_triangle_geometric_normal(tri);
	if (dot(geometric, dir) > 0.0) {
		geometric = -geometric;
	}
	if (dot(h.normal, geometric) < 0.0) {
		h.normal = -h.normal;
	}
}

// A q3map surface light a glossy reflection ray from origin meets: its
// radiance (pi times the light's, which folds Lambert's 1 / pi into the
// light), weighed against the light sampler's chance of the same path. Both
// shares are taken along the path from the reflecting point.
vec3 rt_reflected_light(RtHitPayload hit, vec3 origin, vec3 dir)
{
	uint index = u_primitiveLights.lights[rt_global_primitive(hit.customIndex, hit.geometry & 0xFFFFu, hit.primitive)];
	if (index == ~0u) {
		return vec3(0.0);
	}
	RtLight light = u_lights.lights[index];
	vec3 n = safe_normalize(cross(light.dir.xyz, light.extra.xyz), vec3(0.0, 0.0, 1.0));
	float cosL = -dot(n, dir);
	if (light.extra.w > 0.5) {
		cosL = abs(cosL);
	}
	if (cosL <= 0.0) {
		return vec3(0.0);
	}
	vec3 toLight = origin + dir * hit.hitT - g_misP;
	float dist = length(toLight);
	vec3 L = toLight / max(dist, 1e-6);
	float pReflection = ggx_reflection_pdf(g_misN, g_misNg, g_misV, L, g_misAlpha);
	float pLight = rt_light_pdf_at(g_misChoice, index, g_misP, g_misN, L, dist);
	float share = pReflection + pLight > 0.0 ? pReflection / (pReflection + pLight) : 1.0;
	return PI * light.color.rgb * share;
}

// Radiance arriving along a secondary ray. At each of up to `depth` hits the
// surface's emission and its reflectance lit by one light sample, and past the
// first hit a diffuse bounce from the one before (cosine-sampled: the radiance
// arriving is the irradiance estimate in lightmap units), weighted by the
// reflectances on the way; the sky where a ray escapes. A diffuse bounce
// leaves out a sky that q3map already lights the scene with (its surface-light
// triangles and sky light are sampled directly), and a baked lightmap already
// holds the bounces its compiler traced.
vec3 bounce_radiance(vec3 origin, vec3 dir, float coneWidthAtOrigin, bool bakedLighting, bool reflection,
	uint depth, inout uint seed)
{
	bool skyIsLight = (u_frame.skyImages[1].w & RT_SKY_LIGHTS) != 0u;
	vec3 radiance = vec3(0.0);
	vec3 throughput = vec3(1.0);
	float coneWidth = coneWidthAtOrigin;
	for (uint d = 0u; d < depth; d++) {
		// the reflection's own hit; every later one is a diffuse bounce
		bool seen = reflection && d == 0u;
		if (!trace_radiance(origin, dir, 0.01, 1e5, RT_MASK_SECONDARY)) {
			radiance += throughput * ((seen || !skyIsLight) ? rt_sky_radiance(dir) : vec3(0.0));
			break;
		}
		RtHitPayload hit = payload;
		RtTriangle tri = rt_fetch_triangle(hit.customIndex, hit.geometry & 0xFFFFu, hit.primitive);
		RtMaterial material = rt_triangle_material(tri);
		if ((material.flags & RT_MF_SKY) != 0u) {
			radiance += throughput * ((seen || !skyIsLight) ? rt_sky_radiance(dir) : vec3(0.0));
			break;
		}
		if ((material.flags & RT_MF_PORTAL) != 0u) {
			break;
		}
		RtHitAttribs h = rt_hit_attribs(tri, hit.bary, dir);
		vec3 geometric;
		orient_normals(tri, dir, h, geometric);
		// a diffuse bounce spreads widely: sample a coarse mip
		coneWidth += hit.hitT * 0.5;
		h.coneWidth = coneWidth;
		RtSurface surf = rt_evaluate_material(material, h, bakedLighting, 1.0);
		RtShading sh = rt_shading(material, tri, h, surf, geometric, 1.0);
		vec3 P = origin + dir * hit.hitT;
		ShadeResult direct = shade_direct(material, surf, sh, P, geometric, -dir, 4u, bakedLighting, false, true, seed);
		bool baked = baked_light(material, surf, bakedLighting);
		vec3 irradiance = baked ? direct.irradiance : apply_min_light(material, direct.irradiance);
		// q3map's surface lights reach the scene through light sampling; their
		// glow stages would count them twice. A glossy reflection sees the
		// light itself, shared with the light sample (rt_reflected_light).
		vec3 emission = (material.flags & RT_MF_SURFACE_LIGHT) != 0u ? vec3(0.0) : surf.emission;
		vec3 lightSeen = vec3(0.0);
		if (seen && g_mis && (material.flags & RT_MF_SURFACE_LIGHT) != 0u && !tri.dynamic) {
			lightSeen = rt_reflected_light(hit, origin, dir);
		}
		radiance += throughput * (sh.diffuse * irradiance + emission * u_frame.params0.y + lightSeen);
		if (baked || !surf.lit || d + 1u >= depth) {
			break;
		}
		vec3 bdir = sample_cosine(sh.N, seed);
		if (dot(bdir, geometric) <= 0.0) {
			break;
		}
		throughput *= sh.diffuse;
		origin = P + geometric * max(0.05, 1e-6 * length(P));
		dir = bdir;
	}
	return radiance;
}

// One pixel of the trace: its signals for the denoiser and composite.
void rt_path_main(ivec2 pixel, vec2 size)
{
	vec2 uv = (vec2(pixel) + 0.5) / size;
	vec3 origin = u_frame.camOrigin.xyz;
	vec3 dir = camera_ray(vec2(pixel) + 0.5 + u_frame.jitter.xy, size);
	uint seed = hash_u32(uint(pixel.x) * 1973u + uint(pixel.y) * 9277u + uint(u_frame.time.y) * 26699u);
	bool bakedLighting = (u_frame.flags.x & RT_FRAME_BAKED_LIGHTING) != 0u;
	uint info = 0u;
	bool hit = false;

	// The first-person weapon draws over the world regardless of depth
	// (RF_DEPTHHACK), so it is traced alone first.
	if ((u_frame.flags.x & RT_FRAME_WEAPON) != 0u) {
		hit = trace_radiance(origin, dir, 0.0, 256.0, RT_MASK_WEAPON);
		if (hit) {
			info |= RT_PIXEL_WEAPON;
		}
	}
	if (!hit) {
		hit = trace_radiance(origin, dir, 0.0, 1e6, RT_MASK_PRIMARY);
	}
	if (!hit) {
		write_empty(pixel, RT_PIXEL_SKY);
		return;
	}

	RtHitPayload primary = payload;
	RtTriangle tri = rt_fetch_triangle(primary.customIndex, primary.geometry & 0xFFFFu, primary.primitive);
	RtMaterial material = rt_triangle_material(tri);

	if ((material.flags & RT_MF_SKY) != 0u) {
		write_empty(pixel, RT_PIXEL_SKY);
		return;
	}
	if ((material.flags & RT_MF_PORTAL) != 0u) {
		write_empty(pixel, RT_PIXEL_PORTAL);
		return;
	}

	if ((info & RT_PIXEL_WEAPON) != 0u) {
		g_shadowMask = RT_MASK_WORLD | RT_MASK_DYNAMIC;
	}
	RtHitAttribs h = rt_hit_attribs(tri, primary.bary, dir);
	vec3 geometric;
	orient_normals(tri, dir, h, geometric);
	float pixelAngle = u_frame.params1.w;
	h.coneWidth = primary.hitT * pixelAngle / max(abs(dot(geometric, dir)), 0.2);
	{
		vec3 dDdx, dDdy;
		camera_ray_differentials(dir, size, dDdx, dDdy);
		rt_hit_differentials(tri, dir, primary.hitT, dDdx, dDdy, h);
	}

	RtSurface surf = rt_evaluate_material(material, h, bakedLighting, 0.0);
	RtShading sh = rt_shading(material, tri, h, surf, geometric, 0.0);
	vec3 P = origin + dir * primary.hitT;
	vec3 V = -dir;
	vec3 N = sh.N;
	// a glossy surface also traces a reflection ray (below), which shares
	// the specular light of the lights it can meet with the light sample
	float roughness = max(sh.roughness, u_frame.params0.z);
	bool reflections = surf.lit && (u_frame.flags.x & RT_FRAME_SPECULAR) != 0u && roughness < 0.65;
	bool mis = reflections && (u_frame.flags.x & RT_FRAME_MIS) != 0u;

	ShadeResult direct = shade_direct(material, surf, sh, P, geometric, V, 16u, bakedLighting, mis, false, seed);
	vec3 irradiance = direct.irradiance;
	vec3 specular = direct.specular;

	// one bounce of diffuse light; a baked lightmap already holds its own.
	// As Q2RTX checkerboards its indirect light, a checkerboard of 8 x 8
	// tiles - the pixels a compute workgroup shades together, so a tile's
	// warps all skip the work and free its slot - bounces on alternate frames
	// and counts twice; the temporal and spatial filters fill in the rest.
	uint bounces = u_frame.flags.y;
	bool baked = baked_light(material, surf, bakedLighting);
	float bounceWeight = 1.0;
	if ((u_frame.flags.x & RT_FRAME_CHECKERBOARD) != 0u) {
		uvec2 tile = uvec2(pixel) / uvec2(8u);
		bounceWeight = ((tile.x + tile.y + uint(u_frame.time.y)) & 1u) == 0u ? 2.0 : 0.0;
	}
	if (surf.lit && !baked && bounces > 0u && u_frame.params0.x > 0.0 && bounceWeight > 0.0) {
		vec3 bdir = sample_cosine(N, seed);
		if (dot(bdir, geometric) > 0.0) {
			vec3 borigin = P + geometric * max(0.05, 1e-6 * length(P));
			// cosine sampling: the estimate of irradiance is pi * L; the
			// lightmap-unit convention folds that pi into the light
			irradiance += bounce_radiance(borigin, bdir, h.coneWidth, bakedLighting, false, bounces, seed) *
				(u_frame.params0.x * bounceWeight);
		}
	}

	// glossy reflection of the scene: one GGX-sampled ray (Walter et al.
	// 2007), weighted F * G * (V.H) / ((N.V)(N.H))
	if (reflections) {
		float alpha = roughness * roughness;
		float u1 = rand01(seed);
		float u2 = rand01(seed);
		float cosTheta = sqrt((1.0 - u1) / (1.0 + (alpha * alpha - 1.0) * u1));
		float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
		float phi = 2.0 * PI * u2;
		vec3 t, b;
		basis_from_normal(N, t, b);
		vec3 H = normalize(t * (sinTheta * cos(phi)) + b * (sinTheta * sin(phi)) + N * cosTheta);
		vec3 L = reflect(-V, H);
		float NdotL = dot(N, L);
		float NdotV = max(dot(N, V), 1e-4);
		if (NdotL > 0.0 && dot(L, geometric) > 0.0) {
			float VdotH = max(dot(V, H), 0.0);
			vec3 F = sh.F0 + (1.0 - sh.F0) * pow(1.0 - VdotH, 5.0);
			float G = smith_g1(NdotL, alpha) * smith_g1(NdotV, alpha);
			vec3 weight = F * (G * VdotH / (NdotV * max(cosTheta, 1e-4)));
			vec3 rorigin = P + geometric * max(0.05, 1e-6 * length(P));
			g_mis = g_misLights;
			specular += weight * bounce_radiance(rorigin, L, h.coneWidth, bakedLighting, true, max(bounces, 1u), seed);
			g_mis = false;
		}
	}

	if (!baked) {
		irradiance = apply_min_light(material, irradiance);
	}

	vec3 emission = surf.emission * u_frame.params0.y;
	// RTCW's light materials multiply all a world surface or brush model
	// drew; as in raster, they leave models alone
	if (u_frame.materialLights.y > 0u && (material.flags & RT_MF_NODLIGHT) == 0u &&
		(!tri.dynamic || tri.v0.lightmapData.w > 0.5)) {
		vec3 factor = rt_material_light_factor(P);
		irradiance *= factor;
		specular *= factor;
		emission *= factor;
	}

	// a single NaN or infinity would persist in the denoiser's history
	emission = finite_or_zero(emission);
	vec3 reflectance = finite_or_zero(sh.diffuse);
	float clampValue = u_frame.params1.z;
	irradiance = clamp(finite_or_zero(irradiance), vec3(0.0), vec3(clampValue));
	specular = clamp(finite_or_zero(specular), vec3(0.0), vec3(clampValue));

	// where the surface point was last frame: a moving surface's own motion
	// (its entity's matched vertices) on top of the camera's; a surface new
	// this frame has no history
	vec3 Pprev = P;
	bool history = true;
	if (tri.dynamic) {
		history = rt_previous_position(primary.customIndex, primary.geometry & 0xFFFFu, primary.primitive,
			primary.bary, Pprev);
		if (!history) {
			Pprev = P;
		}
	}
	vec2 prevUv;
	vec2 motion = vec2(0.0);
	if (project_previous(Pprev, prevUv)) {
		motion = uv - prevUv;
	}
	float viewZ = primary.hitT * dot(dir, u_frame.camForward.xyz);
	float prevViewZ = dot(Pprev - u_frame.prevCamOrigin.xyz, u_frame.prevCamForward.xyz);

	// how much of the surface the view's global fog leaves, as the raster
	// pass fogs it by its distance from the eye; the composite blends
	float fogVisibility = 1.0;
	if (u_frame.fogColor.w != 0.0 && (material.flags & RT_MF_NOFOG) == 0u) {
		fogVisibility = rtcwFogVisibility(u_frame.fogColor, u_frame.fogParams,
			u_frame.fogParams.w != 0.0 ? primary.hitT : abs(viewZ));
	}

	info |= RT_PIXEL_OWNED;
	if (tri.dynamic) {
		info |= RT_PIXEL_DYNAMIC;
	}
	imageStore(u_outColor, pixel, vec4(irradiance, 1.0));
	imageStore(u_outSpecular, pixel, vec4(specular, 1.0));
	imageStore(u_outReflectance, pixel, vec4(reflectance, 1.0));
	imageStore(u_outNormalDepth, pixel, vec4(oct_encode(N), viewZ, sh.roughness));
	imageStore(u_outEmission, pixel, vec4(emission, fogVisibility));
	imageStore(u_outMotion, pixel, vec4(motion, prevViewZ, history ? 0.0 : 1.0));
	imageStore(u_outInfo, pixel, uvec4(info | (tri.v0.materialIndex << 8u), 0u, 0u, 0u));
}

#endif
