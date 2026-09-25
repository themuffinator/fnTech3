// Quake III shader stages evaluated at a ray hit.
//
// A material is the parsed stage list of one shader (rt/rt_material.c keeps
// it before the raster renderer folds stages into multitexture passes). The
// stages are blended in order, in the colour space the raster renderer draws
// in. The lighting term L - a $lightmap stage, the baked vertex light of a
// vertex-lit world surface, a model's lightingDiffuse - enters Quake III's
// blends linearly, so the drawn colour is C(L) = A + B * L: A is the light the
// surface gives off (unlit stages, glows, fullbright maps) and B its
// reflectance. Both come from blending the stages twice, with L = 1 and with
// L = 0. The caller multiplies B by traced (or baked) irradiance in lightmap
// units and adds A, which reproduces the raster result wherever the traced
// light equals the baked light.
//
// Requires: rt_common.glsl, and the bindings u_materials, u_stages,
// u_textures and u_frame declared by the including shader.

#ifndef RT_MATERIAL_GLSL
#define RT_MATERIAL_GLSL

struct RtHitAttribs {
	vec2 uv;				// base texture coordinates
	vec2 lightmapUv;
	vec3 position;			// world position
	vec3 normal;			// shading normal, facing the ray
	vec3 viewDir;			// from the hit toward the ray origin
	vec4 color;				// vertex colour (world) or entity colour (dynamic)
	// a model's light as the raster pass draws it (its entity's grid light by
	// the vertex normals); w: 0 none, 1 without the dynamic lights, 2 with them
	vec4 entityLight;
	vec2 entityTexCoord;	// shaderTexCoord for tcMod entityTranslate
	float entityTime;		// the entity's shaderTime
	float coneWidth;		// ray footprint at the hit, world units
	float uvDensity;		// texture-space length per world unit of the base uv
	bool world;
	// A primary hit's pixel footprint on the surface (ray differentials) and
	// the base uv across it: textures sample with gradients, so the
	// samplers' anisotropic filtering applies as it does in raster. Other
	// hits (gradients false) use the ray cone's isotropic width.
	bool gradients;
	vec3 dPdx;
	vec3 dPdy;
	vec2 dUVdx;
	vec2 dUVdy;
};

struct RtSurface {
	vec3 reflectance;		// B: the part of the colour the lighting term scales
	float alpha;
	vec3 emission;			// A: the part it does not
	vec3 baked;				// baked irradiance (lightmap or vertex light), lightmap units
	// a model's baked light (its raster light) stands in baked: 0 no, 1
	// without the dynamic lights, 2 with them
	uint bakedEntity;
	bool lit;				// false: fullbright, emission only
	bool covered;			// false: an alpha-test stage rejected the hit
};

// The renderer's waveform tables (tr_init.c, R_Init) at the entry WAVEVALUE
// reads for phase x: (int64_t)(x * FUNCTABLE_SIZE) & FUNCTABLE_MASK, which
// truncates toward zero. The tables avoid exact zeros: the sine is offset by
// 0.0001 radians, and the sawtooth and triangle hold 1e-6 at entry 0.
float rt_wave_table(uint func, float x)
{
	float n = trunc(x * 1024.0);
	float i = n - 1024.0 * floor(n / 1024.0);
	if (func == GF_SIN) {
		return sin(i * (2.0 * PI / 1024.0) + 0.0001);
	}
	if (func == GF_SQUARE) {
		return i < 512.0 ? 1.0 : -1.0;
	}
	if (func == GF_TRIANGLE) {
		// each quarter mirrors the rising first one
		float q = i - 256.0 * floor(i / 256.0);
		float rise = q == 0.0 ? 1e-6 : q / 256.0;
		if (i < 256.0) return rise;
		if (i < 512.0) return 1.0 - rise;
		if (i < 768.0) return -rise;
		return rise - 1.0;
	}
	float saw = i == 0.0 ? 1e-6 : i / 1024.0;
	if (func == GF_SAWTOOTH) {
		return saw;
	}
	if (func == GF_INVERSE_SAWTOOTH) {
		return 1.0 - saw;
	}
	return 0.0;
}

// Smooth value noise in [-1, 1], standing in for R_NoiseGet4f.
float rt_noise1(float t)
{
	float i = floor(t);
	float f = t - i;
	float a = float(hash_u32(uint(int(i)) * 0x9e3779b9u) >> 8) * (2.0 / 16777216.0) - 1.0;
	float b = float(hash_u32(uint(int(i) + 1) * 0x9e3779b9u) >> 8) * (2.0 / 16777216.0) - 1.0;
	f = f * f * (3.0 - 2.0 * f);
	return mix(a, b, f);
}

// EvalWaveForm: base + table[(phase + time * frequency)] * amplitude.
float rt_wave(uint func, vec4 wave, float time)
{
	if (func == GF_NOISE) {
		return wave.x + rt_noise1((time + wave.z) * wave.w) * wave.y;
	}
	return wave.x + rt_wave_table(func, wave.z + time * wave.w) * wave.y;
}

vec4 rt_unpack_color(uint c)
{
	return unpackUnorm4x8(c);
}

uint rt_stage_image(RtStage s, uint frame)
{
	uint word = s.images[frame >> 1u];
	return (frame & 1u) != 0u ? (word >> 16u) : (word & 0xFFFFu);
}

uint rt_stage_frame(RtStage s, float time)
{
	uint count = s.state.z & 0xFu;
	if (count <= 1u) {
		return 0u;
	}
	// RB_BindAnimatedImage: (int)(time * speed) modulo the frame count
	int index = int(floor(time * s.animTc0.x));
	if (index < 0) {
		index = 0;
	}
	return uint(index) % count;
}

vec2 rt_stage_texcoords(RtStage s, RtHitAttribs h, float time)
{
	uint tcGen = s.state.y & 0xFFu;
	vec2 st;

	if (tcGen == TCGEN_LIGHTMAP) {
		st = h.lightmapUv;
	} else if (tcGen == TCGEN_VECTOR) {
		st = vec2(dot(h.position, s.animTc0.yzw), dot(h.position, s.tc1.xyz));
	} else if (tcGen == TCGEN_ENVIRONMENT_MAPPED || tcGen == TCGEN_ENVIRONMENT_MAPPED_FP) {
		// RB_CalcEnvironmentTexCoords
		float d = dot(h.normal, h.viewDir);
		vec3 reflected = h.normal * 2.0 * d - h.viewDir;
		st = vec2(0.5 + reflected.y * 0.5, 0.5 - reflected.z * 0.5);
	} else if (tcGen == TCGEN_IDENTITY) {
		st = vec2(0.0);
	} else {
		st = h.uv;
	}

	uint numTexMods = (s.state.z >> 4u) & 0xFu;
	for (uint i = 0u; i < numTexMods && i < RT_MAX_TEXMODS; i++) {
		vec4 a = s.texMod[i * 2u];
		vec4 b = s.texMod[i * 2u + 1u];
		uint type = uint(a.x);
		if (type == TMOD_TRANSFORM) {
			// s' = s * m00 + t * m10 + t0; t' = s * m01 + t * m11 + t1
			st = vec2(st.x * a.y + st.y * a.w + b.y, st.x * a.z + st.y * b.x + b.z);
		} else if (type == TMOD_TURBULENT) {
			// RB_CalcTurbulentTexCoords
			float now = a.w + time * b.x;
			st.x += rt_wave_table(GF_SIN, (h.position.x + h.position.z) * (1.0 / 128.0 * 0.125) + now) * a.z;
			st.y += rt_wave_table(GF_SIN, h.position.y * (1.0 / 128.0 * 0.125) + now) * a.z;
		} else if (type == TMOD_SCROLL) {
			st += fract(a.yz * time);
		} else if (type == TMOD_ENTITY_TRANSLATE) {
			st += fract(h.entityTexCoord * time);
		} else if (type == TMOD_SCALE) {
			st *= a.yz;
		} else if (type == TMOD_OFFSET) {
			st += b.xy;
		} else if (type == TMOD_SCALE_OFFSET) {
			st = st * a.yz + b.xy;
		} else if (type == TMOD_OFFSET_SCALE) {
			st = (st + b.xy) * a.yz;
		} else if (type == TMOD_STRETCH) {
			float p = 1.0 / rt_wave(uint(b.w), vec4(a.y, a.z, a.w, b.x), time);
			st = st * p + (0.5 - 0.5 * p);
		} else if (type == TMOD_ROTATE) {
			// RB_CalcRotateTexCoords: sine-table entries a quarter apart
			float index = trunc(-a.y * time * (1024.0 / 360.0));
			float sn = rt_wave_table(GF_SIN, index / 1024.0);
			float cs = rt_wave_table(GF_SIN, (index + 256.0) / 1024.0);
			st = vec2(st.x * cs - st.y * sn + (0.5 - 0.5 * cs + 0.5 * sn),
				st.x * sn + st.y * cs + (0.5 - 0.5 * sn - 0.5 * cs));
		}
	}
	return st;
}

float rt_stage_lod(RtStage s, RtHitAttribs h, uint image, float extraBias)
{
	uint tcGen = s.state.y & 0xFFu;
	if (tcGen == TCGEN_LIGHTMAP || tcGen == TCGEN_ENVIRONMENT_MAPPED ||
		tcGen == TCGEN_ENVIRONMENT_MAPPED_FP) {
		return 0.0;
	}
	vec2 size = vec2(textureSize(u_textures[nonuniformEXT(image)], 0));
	float density = tcGen == TCGEN_VECTOR ? length(s.animTc0.yzw) : h.uvDensity;
	float footprint = h.coneWidth * density * sqrt(size.x * size.y);
	return max(log2(max(footprint, 1e-6)) + extraBias, 0.0);
}

// RB_CalcSpecularAlpha: a highlight from a light fixed at (-960, 1980, 96).
// (The raster renderer evaluates it per vertex in the drawn geometry's own
// space; here it is per pixel in world space.)
float rt_specular_alpha(RtHitAttribs h)
{
	vec3 lightDir = safe_normalize(vec3(-960.0, 1980.0, 96.0) - h.position, h.normal);
	float d = dot(h.normal, lightDir);
	vec3 reflected = h.normal * (2.0 * d) - lightDir;
	vec3 viewer = u_frame.camOrigin.xyz - h.position;
	float l = dot(reflected, viewer) * inversesqrt(max(dot(viewer, viewer), 1e-8));
	if (l < 0.0) {
		return 0.0;
	}
	l = l * l;
	l = l * l;
	return min(floor(l * 255.0), 255.0) / 255.0;
}

// The stage's texture sample; white for a stage without an image.
vec4 rt_stage_texel(RtStage s, RtHitAttribs h, float time, float lodBias)
{
	uint image = rt_stage_image(s, rt_stage_frame(s, time));
	if (image != RT_NO_IMAGE && image < RT_MAX_TEXTURES) {
		vec2 st = rt_stage_texcoords(s, h, time);
		uint tcGen = s.state.y & 0xFFu;
		if (h.gradients && tcGen != TCGEN_LIGHTMAP && tcGen != TCGEN_ENVIRONMENT_MAPPED &&
			tcGen != TCGEN_ENVIRONMENT_MAPPED_FP) {
			// the footprint's corners through the stage's own coordinate
			// generator and texture modifiers
			RtHitAttribs hx = h;
			RtHitAttribs hy = h;
			hx.position += h.dPdx;
			hx.uv += h.dUVdx;
			hy.position += h.dPdy;
			hy.uv += h.dUVdy;
			float scale = exp2(lodBias);
			vec2 gx = (rt_stage_texcoords(s, hx, time) - st) * scale;
			vec2 gy = (rt_stage_texcoords(s, hy, time) - st) * scale;
			return textureGrad(u_textures[nonuniformEXT(image)], st, gx, gy);
		}
		return textureLod(u_textures[nonuniformEXT(image)], st, rt_stage_lod(s, h, image, lodBias));
	}
	return vec4(1.0);
}

// R_ComputeColors: the stage's colour and alpha generators, with the lighting
// term (a vertex-lit surface's vertex light, a model's lightingDiffuse)
// replaced by `light`, or as drawn when light < 0.
vec4 rt_stage_generator(RtStage s, RtHitAttribs h, float time, float light)
{
	uint rgbGen = (s.state.y >> 8u) & 0xFFu;
	uint alphaGen = (s.state.y >> 16u) & 0xFFu;
	uint flags = s.state.z;
	vec4 constColor = rt_unpack_color(s.state.w);
	float identityLight = u_frame.params1.x;
	vec3 rgb;
	float alpha;

	// The world draws as tr.worldEntity, whose shaderRGBA is never set: zero.
	vec4 entityColor = h.world ? vec4(0.0) : h.color;

	if (rgbGen == CGEN_IDENTITY) {
		rgb = vec3(1.0);
	} else if (rgbGen == CGEN_ENTITY) {
		rgb = entityColor.rgb;
	} else if (rgbGen == CGEN_ONE_MINUS_ENTITY) {
		rgb = vec3(1.0) - entityColor.rgb;
	} else if (rgbGen == CGEN_EXACT_VERTEX || rgbGen == CGEN_VERTEX) {
		if (light >= 0.0 && (flags & RT_SF_VERTEX_IS_LIGHT) != 0u) {
			rgb = vec3(light);
		} else {
			rgb = h.color.rgb * (rgbGen == CGEN_VERTEX ? identityLight : 1.0);
		}
	} else if (rgbGen == CGEN_ONE_MINUS_VERTEX) {
		rgb = (vec3(1.0) - h.color.rgb) * identityLight;
	} else if (rgbGen == CGEN_WAVEFORM) {
		uint func = (flags >> 8u) & 0xFu;
		float glow = rt_wave(func, s.rgbWave, time);
		if (func != GF_NOISE) {
			glow *= identityLight;
		}
		rgb = vec3(clamp(glow, 0.0, 1.0));
	} else if (rgbGen == CGEN_LIGHTING_DIFFUSE) {
		// a model's light-grid shading: the path tracer supplies the light
		rgb = vec3(light >= 0.0 ? light : 1.0);
	} else if (rgbGen == CGEN_CONST) {
		rgb = constColor.rgb;
	} else {
		// identityLighting, and the default for anything unrecognised
		rgb = vec3(identityLight);
	}

	if (alphaGen == AGEN_SKIP) {
		// the shader compiler folded an alphaGen matching the rgbGen away:
		// the colour generator's alpha stands (identity, const or vertex)
		alpha = rgbGen == CGEN_CONST ? constColor.a :
			(rgbGen == CGEN_VERTEX || rgbGen == CGEN_EXACT_VERTEX) ? h.color.a : 1.0;
	} else if (alphaGen == AGEN_ENTITY) {
		alpha = entityColor.a;
	} else if (alphaGen == AGEN_ONE_MINUS_ENTITY) {
		alpha = 1.0 - entityColor.a;
	} else if (alphaGen == AGEN_IDENTITY && rgbGen == CGEN_VERTEX && identityLight == 1.0) {
		// the vertex colours were copied whole, alpha included
		alpha = h.color.a;
	} else if (alphaGen == AGEN_LIGHTING_SPECULAR) {
		// Quake III's fixed-light highlight; traced specular replaces it
		// unless the frame keeps baked lighting
		alpha = (u_frame.flags.x & RT_FRAME_BAKED_LIGHTING) != 0u ? rt_specular_alpha(h) : 0.0;
	} else if (alphaGen == AGEN_VERTEX) {
		alpha = h.color.a;
	} else if (alphaGen == AGEN_ONE_MINUS_VERTEX) {
		alpha = 1.0 - h.color.a;
	} else if (alphaGen == AGEN_WAVEFORM) {
		alpha = clamp(rt_wave((flags >> 12u) & 0xFu, s.alphaWave, time), 0.0, 1.0);
	} else if (alphaGen == AGEN_CONST) {
		alpha = constColor.a;
	} else {
		alpha = 1.0;
	}
	return vec4(rgb, alpha);
}

// The stage's colour before blending, in raster framebuffer units, with the
// lighting term replaced by `light` (a $lightmap stage's texels are that
// term), or as drawn when light < 0.
vec4 rt_stage_color(RtStage s, RtHitAttribs h, float time, float lodBias, float light)
{
	uint role = (s.state.y >> 24u) & 0xFu;
	vec4 texel = rt_stage_texel(s, h, time, lodBias);
	if (role == RT_ROLE_LIGHTMAP && light >= 0.0) {
		return vec4(vec3(light), 1.0);
	}
	return texel * rt_stage_generator(s, h, time, light);
}

// glBlendFunc source and destination factors, GLS encoding.
vec4 rt_blend_src(uint f, vec4 src, vec4 dst)
{
	if (f == 1u) return vec4(0.0);
	if (f == 3u) return dst;
	if (f == 4u) return vec4(1.0) - dst;
	if (f == 5u) return vec4(src.a);
	if (f == 6u) return vec4(1.0 - src.a);
	if (f == 7u) return vec4(dst.a);
	if (f == 8u) return vec4(1.0 - dst.a);
	if (f == 9u) { float k = min(src.a, 1.0 - dst.a); return vec4(k, k, k, 1.0); }
	return vec4(1.0);
}

vec4 rt_blend_dst(uint f, vec4 src, vec4 dst)
{
	if (f == 2u) return vec4(1.0);
	if (f == 3u) return src;
	if (f == 4u) return vec4(1.0) - src;
	if (f == 5u) return vec4(src.a);
	if (f == 6u) return vec4(1.0 - src.a);
	if (f == 7u) return vec4(dst.a);
	if (f == 8u) return vec4(1.0 - dst.a);
	return vec4(0.0);
}

bool rt_alpha_test(uint stateBits, float alpha)
{
	uint test = stateBits & GLS_ATEST_BITS;
	if (test == GLS_ATEST_GT_0) return alpha > 0.0;
	if (test == GLS_ATEST_LT_80) return alpha < 0.5;
	if (test == GLS_ATEST_GE_80) return alpha >= 0.5;
	return true;
}

float rt_material_time(RtMaterial m, RtHitAttribs h)
{
	// tess.shaderTime: refdef time less the entity's shaderTime and the
	// shader's timeOffset, clamped to clampTime when one is set
	float time = u_frame.time.x - h.entityTime - m.timing.x;
	if (m.timing.y > 0.0 && time >= m.timing.y) {
		time = m.timing.y;
	}
	return time;
}

// One glBlendFunc step, with the ordinary no-blend replace.
vec4 rt_blend(uint srcFactor, uint dstFactor, vec4 src, vec4 dst)
{
	if (srcFactor == 0u && dstFactor == 0u) {
		return src;
	}
	return src * rt_blend_src(srcFactor, src, dst) + dst * rt_blend_dst(dstFactor, src, dst);
}

RtSurface rt_evaluate_material(RtMaterial m, RtHitAttribs h, bool bakedLighting, float lodBias)
{
	RtSurface surf;
	surf.reflectance = vec3(0.0);
	surf.alpha = 1.0;
	surf.emission = vec3(0.0);
	surf.baked = vec3(0.0);
	surf.bakedEntity = 0u;
	surf.lit = (m.flags & (RT_MF_LIGHTMAPPED | RT_MF_VERTEX_LIT | RT_MF_ENTITY_LIT)) != 0u;
	surf.covered = true;

	float time = rt_material_time(m, h);
	// the stages blended with the lighting term white (lit) and black (dark)
	vec4 lit = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 dark = vec4(0.0, 0.0, 0.0, 1.0);
	bool first = true;

	for (uint i = 0u; i < m.stageCount && i < 8u; i++) {
		RtStage s = u_stages.stages[m.firstStage + i];
		uint role = (s.state.y >> 24u) & 0xFu;
		uint stateBits = s.state.x;
		uint srcFactor = stateBits & GLS_SRCBLEND_BITS;
		uint dstFactor = (stateBits & GLS_DSTBLEND_BITS) >> 4u;

		if (role == RT_ROLE_SKIP) {
			continue;
		}
		if (role == RT_ROLE_ENVIRONMENT && !bakedLighting) {
			// traced reflections replace an authored environment map
			continue;
		}

		vec4 texel = rt_stage_texel(s, h, time, lodBias);
		vec4 srcLit, srcDark;
		if (role == RT_ROLE_LIGHTMAP) {
			// its texels are the light itself; baked lighting keeps them
			surf.baked = (texel * rt_stage_generator(s, h, time, -1.0)).rgb;
			srcLit = vec4(1.0);
			srcDark = vec4(0.0, 0.0, 0.0, 1.0);
		} else {
			srcLit = texel * rt_stage_generator(s, h, time, 1.0);
			srcDark = texel * rt_stage_generator(s, h, time, 0.0);
		}
		if ((s.state.z & RT_SF_VERTEX_IS_LIGHT) != 0u) {
			uint rgbGen = (s.state.y >> 8u) & 0xFFu;
			surf.baked = h.color.rgb * (rgbGen == CGEN_VERTEX ? u_frame.params1.x : 1.0);
		}
		if (((s.state.y >> 8u) & 0xFFu) == CGEN_LIGHTING_DIFFUSE && h.entityLight.w > 0.5) {
			// a model's lighting term is its raster light (RB_CalcDiffuseColor)
			surf.baked = h.entityLight.rgb;
			surf.bakedEntity = h.entityLight.w > 1.5 ? 2u : 1u;
		}

		if ((s.state.z & RT_SF_ALPHA_TEST) != 0u && !rt_alpha_test(stateBits, srcLit.a)) {
			surf.covered = false;
		}

		if (first) {
			first = false;
			if (srcFactor == 3u || srcFactor == 4u) {
				// a leading filter multiplies whatever is behind; take white
				lit = vec4(1.0);
				dark = vec4(1.0);
			}
		}
		lit = rt_blend(srcFactor, dstFactor, srcLit, lit);
		dark = rt_blend(srcFactor, dstFactor, srcDark, dark);
	}

	surf.alpha = clamp(lit.a, 0.0, 1.0);
	surf.emission = max(dark.rgb, vec3(0.0));
	surf.reflectance = max(lit.rgb - dark.rgb, vec3(0.0));
	if (!surf.lit) {
		vec3 c = max(lit.rgb, vec3(0.0));
		if ((m.flags & RT_MF_FULLBRIGHT_LIT) != 0u) {
			// As Q2RTX lights Quake II: the scene lights a surface its shader
			// drew fullbright, and only its bright texels glow (the emissive
			// mask Q2RTX synthesizes from a texture's bright pixels)
			float glow = rt_bright_texel(c);
			surf.emission = c * glow;
			surf.reflectance = c * (1.0 - glow);
			surf.lit = true;
		} else {
			// a fullbright surface shows its stages as authored
			surf.emission = c;
			surf.reflectance = vec3(0.0);
		}
	}
	if ((u_frame.flags.x & RT_FRAME_LINEAR_LIGHT) != 0u) {
		// what the stages show is a colour on the display; light is traced
		// in linear units, as Q2RTX decodes its textures
		surf.reflectance = rt_linear(surf.reflectance);
		surf.emission = rt_linear(surf.emission);
	}
	return surf;
}

// Coverage only, for any-hit: the alpha-test stage decides.
bool rt_material_covered(RtMaterial m, RtHitAttribs h)
{
	float time = rt_material_time(m, h);
	for (uint i = 0u; i < m.stageCount && i < 8u; i++) {
		RtStage s = u_stages.stages[m.firstStage + i];
		if ((s.state.z & RT_SF_ALPHA_TEST) == 0u) {
			continue;
		}
		vec4 src = rt_stage_color(s, h, time, 0.0, 1.0);
		return rt_alpha_test(s.state.x, src.a);
	}
	return true;
}

#endif
