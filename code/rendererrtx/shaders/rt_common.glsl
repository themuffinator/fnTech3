// Shared definitions for the RTX renderer's path tracer.
//
// Every structure here has a C mirror in code/rendererrtx/rt/rt_gpu.h with
// size and offset checks; change both together.

#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

const float PI = 3.14159265358979;
const uint RT_INVALID = 0xFFFFFFFFu;
const uint RT_MAX_TEXTURES = 2048u;
const uint RT_NO_IMAGE = 0xFFFFu;
const uint RT_MAX_TEXMODS = 5u;

// Instance classes, stored in bits 20..23 of instanceCustomIndex.
const uint RT_CLASS_WORLD = 0u;			// static world, opaque and masked
const uint RT_CLASS_WORLD_SPECIAL = 1u;	// sky and portal surfaces
const uint RT_CLASS_DYNAMIC = 2u;		// entities and effects this frame
const uint RT_CLASS_WEAPON = 3u;		// first-person weapon and hands
const uint RT_CLASS_BODY = 4u;			// the viewer's own body (shadows and reflections)
const uint RT_CLASS_WORLD_NOSHADOW = 5u;	// world surfaces the map's lights pass through
const uint RT_CLASS_WORLD_FILTER = 6u;	// world surfaces the map's lights pass filtered (shadow rays only)
const uint RT_CLASS_COUNT = 7u;

// TLAS instance masks.
const uint RT_MASK_WORLD = 0x01u;
const uint RT_MASK_DYNAMIC = 0x02u;
const uint RT_MASK_WEAPON = 0x04u;
const uint RT_MASK_BODY = 0x08u;
const uint RT_MASK_SPECIAL = 0x10u;
const uint RT_MASK_NOSHADOW = 0x20u;
const uint RT_MASK_FILTER = 0x40u;
const uint RT_MASK_PRIMARY = RT_MASK_WORLD | RT_MASK_DYNAMIC | RT_MASK_SPECIAL | RT_MASK_NOSHADOW;
const uint RT_MASK_SECONDARY = RT_MASK_WORLD | RT_MASK_DYNAMIC | RT_MASK_BODY | RT_MASK_SPECIAL | RT_MASK_NOSHADOW;
// shadows from the map's own lights and sun, which (as in q3map) surfaceparm
// trans surfaces, and patches without -patchshadows, do not cast; dynamic
// lights add RT_MASK_NOSHADOW
const uint RT_MASK_SHADOW = RT_MASK_WORLD | RT_MASK_DYNAMIC | RT_MASK_BODY;
// the instances a map compiler never saw (models, movers, the weapon and the
// viewer's body): a solid ray still takes their alpha tests
const uint RT_MASK_MOVING = RT_MASK_DYNAMIC | RT_MASK_WEAPON | RT_MASK_BODY;

// Instances the compute backend traverses (rt_trace_sw.glsl).
const uint RT_SW_MAX_INSTANCES = 8u;

// Material flags (RtMaterial.flags).
const uint RT_MF_MASKED = 1u << 0;			// alpha tested; any-hit decides coverage
const uint RT_MF_SKY = 1u << 1;
const uint RT_MF_PORTAL = 1u << 2;
const uint RT_MF_TWO_SIDED = 1u << 3;
const uint RT_MF_CULL_FRONT = 1u << 4;		// back-sided in Quake terms
const uint RT_MF_LIGHTMAPPED = 1u << 5;	// has a $lightmap stage
const uint RT_MF_VERTEX_LIT = 1u << 6;		// world vertex colours carry baked light
const uint RT_MF_SURFACE_LIGHT = 1u << 7;	// an emitter the light sampler reaches
const uint RT_MF_ENV_MAPPED = 1u << 8;		// authored environment reflection stage
const uint RT_MF_NOLIGHT = 1u << 9;		// fullbright: no lightmap or vertex light, not a model
const uint RT_MF_ENTITY_LIT = 1u << 10;	// rgbGen lightingDiffuse (model shading)
const uint RT_MF_EMISSIVE_ONLY = 1u << 11;	// additive or otherwise light-only surface
const uint RT_MF_ALPHASHADOW = 1u << 12;	// compiled with surfaceparm alphashadow
const uint RT_MF_LIGHTFILTER = 1u << 13;	// compiled with surfaceparm lightfilter
const uint RT_MF_MATERIAL_MAPS = 1u << 14;	// companion normal or material map (RtMaterial.maps)
const uint RT_MF_NODLIGHT = 1u << 15;		// the raster light pass skips it: no dynamic light
const uint RT_MF_NOFOG = 1u << 16;			// RTCW nofog: the view's global fog leaves it
const uint RT_MF_FULLBRIGHT_LIT = 1u << 17;	// drawn fullbright, lit all the same; bright texels glow
const uint RT_MF_LIQUID = 1u << 18;
const uint RT_MF_TRANSMISSIVE = 1u << 19;

// a model vertex's xyz.w: the raster pass's light for it, RGB8 under this top
// byte, without or with the dynamic lights (rt/rt_backend.c RT_StampEntityLight)
const uint RT_ENTITY_LIGHT_GRID = 0x40u;
const uint RT_ENTITY_LIGHT_ALL = 0x41u;

// RtMaterial.maps.z: the material map's channels (tr_local.h RT_MATERIAL_MAP_*)
const uint RT_MATERIAL_MAP_SPECGLOSS = 1u;	// specular colour, gloss in alpha
const uint RT_MATERIAL_MAP_RMO = 2u;		// roughness, metalness, occlusion
const uint RT_MATERIAL_MAP_ORM = 3u;		// occlusion, roughness, metalness

// RtMaterial.lightImage.y
const uint RT_LIGHT_IMAGE_MISSING = 1u << 0;	// none found: a white, opaque image
const uint RT_LIGHT_IMAGE_SRGB = 1u << 1;		// its view decodes sRGB colour

// Stage roles (bits 24..27 of RtStage.state.y).
const uint RT_ROLE_BASE = 0u;			// an ordinary stage
const uint RT_ROLE_LIGHTMAP = 1u;		// $lightmap: the lighting term
const uint RT_ROLE_ENVIRONMENT = 2u;	// tcGen environment fake reflection
const uint RT_ROLE_SKIP = 3u;

// Stage flags (bits 16..31 of RtStage.state.z).
const uint RT_SF_LINEAR_SAMPLE = 1u << 16;	// image view already decodes sRGB
const uint RT_SF_VERTEX_IS_LIGHT = 1u << 17;	// rgbGen vertex on a vertex-lit world surface
const uint RT_SF_ALPHA_TEST = 1u << 18;		// this stage's alpha test decides coverage
const uint RT_SF_DETAIL = 1u << 19;

// Quake III's tcGen, rgbGen and alphaGen values (tr_local.h).
const uint TCGEN_IDENTITY = 1u;
const uint TCGEN_LIGHTMAP = 2u;
const uint TCGEN_TEXTURE = 3u;
const uint TCGEN_ENVIRONMENT_MAPPED = 4u;
const uint TCGEN_ENVIRONMENT_MAPPED_FP = 5u;
const uint TCGEN_FOG = 6u;
const uint TCGEN_VECTOR = 7u;

const uint CGEN_IDENTITY_LIGHTING = 1u;
const uint CGEN_IDENTITY = 2u;
const uint CGEN_ENTITY = 3u;
const uint CGEN_ONE_MINUS_ENTITY = 4u;
const uint CGEN_EXACT_VERTEX = 5u;
const uint CGEN_VERTEX = 6u;
const uint CGEN_ONE_MINUS_VERTEX = 7u;
const uint CGEN_WAVEFORM = 8u;
const uint CGEN_LIGHTING_DIFFUSE = 9u;
const uint CGEN_FOG = 10u;
const uint CGEN_CONST = 11u;

const uint AGEN_IDENTITY = 0u;
const uint AGEN_SKIP = 1u;
const uint AGEN_ENTITY = 2u;
const uint AGEN_ONE_MINUS_ENTITY = 3u;
const uint AGEN_VERTEX = 4u;
const uint AGEN_ONE_MINUS_VERTEX = 5u;
const uint AGEN_LIGHTING_SPECULAR = 6u;
const uint AGEN_WAVEFORM = 7u;
const uint AGEN_PORTAL = 8u;
const uint AGEN_CONST = 9u;

const uint GF_NONE = 0u;
const uint GF_SIN = 1u;
const uint GF_SQUARE = 2u;
const uint GF_TRIANGLE = 3u;
const uint GF_SAWTOOTH = 4u;
const uint GF_INVERSE_SAWTOOTH = 5u;
const uint GF_NOISE = 6u;

const uint TMOD_NONE = 0u;
const uint TMOD_TRANSFORM = 1u;
const uint TMOD_TURBULENT = 2u;
const uint TMOD_SCROLL = 3u;
const uint TMOD_SCALE = 4u;
const uint TMOD_STRETCH = 5u;
const uint TMOD_ROTATE = 6u;
const uint TMOD_ENTITY_TRANSLATE = 7u;
const uint TMOD_OFFSET = 8u;
const uint TMOD_SCALE_OFFSET = 9u;
const uint TMOD_OFFSET_SCALE = 10u;

// GLS blend and alpha-test bits (tr_local.h).
const uint GLS_SRCBLEND_BITS = 0x0000000Fu;
const uint GLS_DSTBLEND_BITS = 0x000000F0u;
const uint GLS_ATEST_BITS = 0x00003000u;
const uint GLS_ATEST_GT_0 = 0x00001000u;
const uint GLS_ATEST_LT_80 = 0x00002000u;
const uint GLS_ATEST_GE_80 = 0x00003000u;

// Light types.
const uint RT_LIGHT_POINT = 0u;
const uint RT_LIGHT_POINT_LINEAR = 1u;
const uint RT_LIGHT_SPOT = 2u;
const uint RT_LIGHT_TRIANGLE = 3u;
const uint RT_LIGHT_DLIGHT = 4u;
const uint RT_LIGHT_MATERIAL = 5u;			// an RTCW light material: multiplies, never sampled
const uint RT_LIGHT_TYPE_MASK = 0xFu;
const uint RT_LIGHT_NO_SURFACES = 1u << 4;	// a lightJunior: models only
const uint RT_LIGHT_NO_MODELS = 1u << 5;	// nogrid: world surfaces only
const uint RT_LIGHT_TRACED = 1u << 6;		// a triangle light a glossy reflection can meet
const uint RT_LIGHT_SKY = 1u << 7;			// a sky surface's emitter
const uint RT_LIGHT_DYNAMIC = 1u << 8;		// a dynamic light, which an RT_MF_NODLIGHT surface skips

// Frame flags (RtFrame.flags.x).
const uint RT_FRAME_BAKED_LIGHTING = 1u << 0;	// lightmaps instead of traced direct light
const uint RT_FRAME_SUN = 1u << 1;
const uint RT_FRAME_SCENE_LINEAR = 1u << 2;	// scene colour is linear (r_hdr)
const uint RT_FRAME_WEAPON = 1u << 3;			// a first-person instance exists
const uint RT_FRAME_SPECULAR = 1u << 4;
const uint RT_FRAME_FILTERS = 1u << 5;		// the world has alphashadow/lightfilter surfaces
const uint RT_FRAME_Q3MAP2 = 1u << 6;		// q3map2 compiled the map (its filter rules)
const uint RT_FRAME_MIS = 1u << 7;			// reflections share surface lights with light sampling
const uint RT_FRAME_ACCUMULATE = 1u << 8;	// average still frames
const uint RT_FRAME_SKY_RADIANCE = 1u << 9;	// the sky lights with its own colour
const uint RT_FRAME_CHECKERBOARD = 1u << 10;	// half the tiles bounce each frame
const uint RT_FRAME_LINEAR_LIGHT = 1u << 11;	// colours decoded, light traced linear
const uint RT_FRAME_LIGHT_REUSE = 1u << 12; // replay compatible primary light proposals

// Pixel information flags (u_info bits 0..7).
const uint RT_PIXEL_OWNED = 1u << 0;		// the path tracer owns this pixel's colour
const uint RT_PIXEL_SKY = 1u << 1;
const uint RT_PIXEL_PORTAL = 1u << 2;
const uint RT_PIXEL_DYNAMIC = 1u << 3;
const uint RT_PIXEL_WEAPON = 1u << 4;
const uint RT_PIXEL_LIQUID = 1u << 5;

struct RtVertex {
	vec4 xyz;
	vec4 normal;
	vec4 tangent;
	vec2 texCoord;
	uint color;				// RGBA8: world vertex colour, or the entity's shaderRGBA
	uint materialIndex;
	vec4 lightmapData;		// world: lightmap st; dynamic: shaderTexCoord st, shaderTime
};

struct RtMaterial {
	uint flags;
	uint firstStage;
	uint stageCount;
	uint lightmapImage;
	vec4 emit;				// rgb: q3map surface-light colour, w: q3map_surfacelight value
	vec4 params;			// roughness, metallic, specular reflectance, liquid IOR
	vec4 timing;			// shader timeOffset, clampTime, unused, unused
	uvec4 maps;				// normal map, material map, material map type, stage they follow
	uvec4 lightImage;		// the compilers' light image, RT_LIGHT_IMAGE_* flags, unused, unused
};

struct RtStage {
	uvec4 images;			// eight 16-bit image indices
	uvec4 state;			// x GLS bits, y tcGen|rgbGen<<8|alphaGen<<16|role<<24,
							// z numImages|numTexMods<<4|rgbWave func<<8|alphaWave func<<12|flags,
							// w constant colour RGBA8
	vec4 animTc0;			// animMap frames per second, tcGen vector 0
	vec4 tc1;				// tcGen vector 1, unused
	vec4 rgbWave;			// base, amplitude, phase, frequency
	vec4 alphaWave;
	vec4 texMod[RT_MAX_TEXMODS * 2u];
};

struct RtLight {
	vec4 position;			// point/spot: origin; triangle: vertex 0; w: influence radius
	vec4 color;				// rgb: display-referred intensity; w: type
	vec4 dir;				// spot: axis, radius/distance; triangle: edge 1, area
	vec4 extra;				// triangle: edge 2, two-sided; point: distance clamp
};

// Frame-constant parameters. Mirrors rtFrameUniforms_t.
struct RtFrameData {
	vec4 camOrigin;			// xyz, tan(fov x / 2)
	vec4 camForward;		// xyz, tan(fov y / 2)
	vec4 camRight;			// xyz, render width
	vec4 camUp;				// xyz, render height
	vec4 prevCamOrigin;
	vec4 prevCamForward;
	vec4 prevCamRight;
	vec4 prevCamUp;
	vec4 sunDirection;		// toward the sun, cos of its angular radius
	vec4 sunColor;			// display-referred irradiance at normal incidence
	vec4 time;				// shader time (refdef floatTime), frame index, unused, unused
	vec4 lightGridOrigin;	// xyz, 1 / cell size
	uvec4 lightGridDims;	// cells per axis, static light count
	vec4 params0;			// indirect strength, emission scale, roughness floor, sky scale
	vec4 params1;			// tr.identityLight, indirect divisor, radiance clamp, pixel cone angle
	uvec4 flags;			// frame flags, bounce limit, debug mode, dlight count
	uvec4 counts;			// materials, stages, total lights, first dlight
	uvec4 classPrims[RT_CLASS_COUNT];	// per class: first opaque prim, first masked prim, unused, unused
	vec4 ambient;			// q3map worldspawn ambient, lightmap units
	vec4 jitter;			// pixel jitter, frames accumulated, unused
	uvec4 skyImages[2];		// farbox faces rt bk lf ft up dn, cloud layer, sky flags
	vec4 minLight[3];		// q3map2 floors (lightmap units): lightmapped, vertex-lit, models
	uvec4 swInfo;			// compute backend: instance count
	// per instance: node offset, primitive offset, class, mask; first opaque
	// primitive, first masked primitive (~0 for none), dynamic, unused
	uvec4 swInstances[RT_SW_MAX_INSTANCES * 2u];
	vec4 sunShape;			// q3map_sunExt: deviance (radians), the exact direction's chance, unused, unused
	vec4 skyLight;			// q3map_skyLight: colour * value (lightmap units), iterations (0 for none)
	vec4 dlightShape;		// the raster light pass's falloff: its smoothstep share (r_dlightFalloff), unused...
	uvec4 materialLights;	// RTCW light materials: first light, count; rtx_rt_debug_light + 1 (0: off), its mode
	vec4 fogColor;			// the view's global fog: colour, mode (0 none, 1 linear, 2 exp)
	vec4 fogParams;			// start, end, density, 1 radial or 0 eye-plane distance
	vec4 liquidParams;		// wave time, warp scale, ring count, unused
	vec4 liquidImpulses[8];	// world origin, expanding radius
	vec4 liquidAmplitudes[2];
};

// Radiance rays record the hit; ray generation shades it.
struct RtHitPayload {
	float hitT;				// negative on a miss
	uint customIndex;
	uint primitive;
	uint geometry;			// geometry index, bit 31 set for a front-facing hit
	vec2 bary;
};

const uint RT_HIT_FRONT_FACING = 0x80000000u;

vec3 safe_normalize(vec3 v, vec3 fallback)
{
	float lengthSquared = dot(v, v);
	if (!(lengthSquared > 1e-20) || any(isnan(v)) || any(isinf(v))) {
		return fallback;
	}
	return v * inversesqrt(lengthSquared);
}

// Zero for NaN or infinite components.
vec3 finite_or_zero(vec3 v)
{
	bvec3 bad = bvec3(uvec3(isnan(v)) | uvec3(isinf(v)));
	return mix(v, vec3(0.0), bad);
}

float luminance(vec3 c)
{
	return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// How much of a texel a shader draws fullbright glows (RT_MF_FULLBRIGHT_LIT):
// none where its brightest channel is below 0.55, all above 0.85. Stages
// drawn at tr.identityLight (rgbGen identityLighting) never reach it; ones
// at full intensity - how Quake III draws a lamp brighter than a wall - do
// where their texture is bright (rt/rt_material.c RT_BrightTexel)
float rt_bright_texel(vec3 c)
{
	return smoothstep(0.55, 0.85, max(c.r, max(c.g, c.b)));
}

// A colour as the display shows it, in linear light (gamma 2.2, as the
// composite encodes it again; rt/rt_lights.c RT_LinearColor)
vec3 rt_linear(vec3 c)
{
	return pow(max(c, vec3(0.0)), vec3(2.2));
}

uint hash_u32(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float rand01(inout uint state)
{
	state = hash_u32(state + 0x9e3779b9u);
	return float(state >> 8) * (1.0 / 16777216.0);
}

// A direction drawn uniformly over the unit sphere
vec3 rand_sphere(inout uint state)
{
	float z = 2.0 * rand01(state) - 1.0;
	float phi = 6.28318530718 * rand01(state);
	float r = sqrt(max(0.0, 1.0 - z * z));
	return vec3(r * cos(phi), r * sin(phi), z);
}

vec3 srgb_to_linear(vec3 c)
{
	c = max(c, vec3(0.0));
	vec3 lo = c / 12.92;
	vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
	return mix(lo, hi, step(vec3(0.04045), c));
}

vec3 linear_to_srgb(vec3 c)
{
	c = max(c, vec3(0.0));
	vec3 lo = c * 12.92;
	vec3 hi = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
	return mix(lo, hi, step(vec3(0.0031308), c));
}

vec2 oct_wrap(vec2 v)
{
	return (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

vec2 oct_encode(vec3 n)
{
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	vec2 e = n.z >= 0.0 ? n.xy : oct_wrap(n.xy);
	return e;
}

vec3 oct_decode(vec2 e)
{
	vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (n.z < 0.0) {
		n.xy = oct_wrap(n.xy);
	}
	return normalize(n);
}

void basis_from_normal(vec3 n, out vec3 t, out vec3 b)
{
	// Duff et al. 2017, "Building an Orthonormal Basis, Revisited".
	float s = n.z >= 0.0 ? 1.0 : -1.0;
	float a = -1.0 / (s + n.z);
	float c = n.x * n.y * a;
	t = vec3(1.0 + s * n.x * n.x * a, s * c, -s * n.x);
	b = vec3(c, s + n.y * n.y * a, -n.y);
}

uint instance_class(uint customIndex)
{
	return (customIndex >> 20) & 0xFu;
}

#endif
