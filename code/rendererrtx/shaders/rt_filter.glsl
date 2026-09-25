// Light through the surfaces the map compilers filtered it with: surfaceparm
// alphashadow and lightfilter (rt/rt_backend.c keeps them in an instance of
// their own, class RT_CLASS_WORLD_FILTER, which only filter rays see). id's
// q3map lets light through where the light image's alpha is below one half
// and stops it elsewhere, and has no colour filter (light_trace.c
// SetFacetFilter); q3map2 scales it by one minus the alpha (alphashadow) and
// by the image's colour (lightfilter), and counts a surface that leaves no
// light as opaque (TraceTriangle). Both read their light image
// (q3map_lightImage, qer_editorimage or the shader-named image; white and
// opaque when none was found) one texel at a time, at the surface's own
// texture coordinates before any tcMod, wrapped.
//
// Requires: rt_common.glsl, rt_bindings.glsl, rt_material.glsl,
// rt_geometry.glsl.

#ifndef RT_FILTER_GLSL
#define RT_FILTER_GLSL

// A material the map was compiled as filtering, whose script today names no
// alphashadow or lightfilter (so no light image): its first image that is
// not a lightmap stands in.
uint rt_filter_stand_in(RtMaterial m)
{
	for (uint i = 0u; i < m.stageCount && i < 8u; i++) {
		RtStage s = u_stages.stages[m.firstStage + i];
		if (((s.state.y >> 24u) & 0xFu) != RT_ROLE_LIGHTMAP) {
			return rt_stage_image(s, 0u);
		}
	}
	return RT_NO_IMAGE;
}

// The fraction of light a filtering surface passes at a hit.
vec3 rt_filter_transmission(RtTriangle tri, vec2 bary)
{
	RtMaterial m = rt_triangle_material(tri);
	vec4 texel = vec4(1.0);
	if ((m.lightImage.y & RT_LIGHT_IMAGE_MISSING) == 0u) {
		uint image = m.lightImage.x < RT_MAX_TEXTURES ? m.lightImage.x : rt_filter_stand_in(m);
		if (image >= RT_MAX_TEXTURES) {
			// no image at all: q3map takes the surface for solid
			return vec3(0.0);
		}
		float b1 = bary.x;
		float b2 = bary.y;
		vec2 uv = tri.v0.texCoord * (1.0 - b1 - b2) + tri.v1.texCoord * b1 + tri.v2.texCoord * b2;
		ivec2 size = textureSize(u_textures[nonuniformEXT(image)], 0);
		ivec2 at = clamp(ivec2(fract(uv) * vec2(size)), ivec2(0), size - 1);
		texel = texelFetch(u_textures[nonuniformEXT(image)], at, 0);
		if ((m.lightImage.y & RT_LIGHT_IMAGE_SRGB) != 0u) {
			// the compilers read the stored colour, not its linear value
			texel.rgb = linear_to_srgb(texel.rgb);
		}
	}
	vec3 t = vec3(1.0);
	if ((u_frame.flags.x & RT_FRAME_Q3MAP2) != 0u) {
		if ((m.flags & RT_MF_LIGHTFILTER) != 0u) {
			t *= texel.rgb;
		}
		if ((m.flags & RT_MF_ALPHASHADOW) != 0u) {
			t *= 1.0 - texel.a;
		}
	} else if ((m.flags & RT_MF_ALPHASHADOW) != 0u) {
		t = texel.a < 0.5 ? vec3(1.0) : vec3(0.0);
	}
	return t;
}

#endif
