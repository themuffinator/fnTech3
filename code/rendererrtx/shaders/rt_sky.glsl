// Sky radiance for secondary rays: the map's skybox sampled by direction the
// way Quake III projects it (tr_sky.c: vec_to_st, sky_texorder, and
// MakeSkyVec's texture coordinates), or the average of its cloud layer when
// it has no farbox. Raster draws the box at tr.identityLight, so the result
// is in the same framebuffer units as every other traced colour.
//
// Requires: rt_common.glsl and rt_bindings.glsl.

#ifndef RT_SKY_GLSL
#define RT_SKY_GLSL

const uint RT_SKY_BOX = 1u << 0;		// farbox faces are present
const uint RT_SKY_CLOUDS = 1u << 1;		// a cloud layer image is present
const uint RT_SKY_LIGHTS = 1u << 2;		// the sky is a q3map surface light

float rt_sky_pick(vec3 v, int j)
{
	return j > 0 ? v[j - 1] : -v[-j - 1];
}

uint rt_sky_image(uint face)
{
	uvec4 words = u_frame.skyImages[face >> 2u];
	return words[face & 3u];
}

// A sky image's texel as light: decoded where light is traced linear
vec3 rt_sky_color(vec3 c)
{
	return (u_frame.flags.x & RT_FRAME_LINEAR_LIGHT) != 0u ? rt_linear(c) : c;
}

vec3 rt_sky_radiance(vec3 d)
{
	uint flags = u_frame.skyImages[1].w;
	float scale = u_frame.params1.x * u_frame.params0.w;

	if ((flags & RT_SKY_BOX) != 0u) {
		const ivec3 vecToSt[6] = ivec3[6](
			ivec3(-2, 3, 1), ivec3(2, 3, -1),
			ivec3(1, 3, 2), ivec3(-1, 3, -2),
			ivec3(-2, -1, 3), ivec3(-2, 1, -3));
		const uint texOrder[6] = uint[6](0u, 2u, 1u, 3u, 4u, 5u);
		vec3 a = abs(d);
		uint axis;
		if (a.x > a.y && a.x > a.z) {
			axis = d.x < 0.0 ? 1u : 0u;
		} else if (a.y > a.z && a.y > a.x) {
			axis = d.y < 0.0 ? 3u : 2u;
		} else {
			axis = d.z < 0.0 ? 5u : 4u;
		}
		ivec3 m = vecToSt[axis];
		float dv = max(rt_sky_pick(d, m.z), 1e-4);
		float s = rt_sky_pick(d, m.x) / dv;
		float t = rt_sky_pick(d, m.y) / dv;
		vec2 uv = vec2(clamp((s + 1.0) * 0.5, 0.0, 1.0), 1.0 - clamp((t + 1.0) * 0.5, 0.0, 1.0));
		uint image = rt_sky_image(texOrder[axis]);
		if (image < RT_MAX_TEXTURES) {
			return rt_sky_color(textureLod(u_textures[nonuniformEXT(image)], uv, 0.0).rgb) * scale;
		}
	}
	if ((flags & RT_SKY_CLOUDS) != 0u) {
		uint image = u_frame.skyImages[1].z;
		if (image < RT_MAX_TEXTURES) {
			// the coarsest mip is the layer's average colour
			return rt_sky_color(textureLod(u_textures[nonuniformEXT(image)], vec2(0.5), 16.0).rgb) * scale;
		}
	}
	return vec3(0.0);
}

#endif
