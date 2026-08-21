#version 450

/* Submerged-view compositor.  The wave field, the dispersion split, the edge
 * falloff, the absorption curve, and every constant below are shared with the
 * ARB assembly copy in renderer/tr_arb.c; read renderercommon/tr_underwater.h
 * before changing any number here, and note that
 * tests/underwater_view_source_tests.py enforces the parity.
 *
 * The pass renders into a private scratch attachment while sampling the main
 * scene color, so it never reads the attachment it is writing.  A second draw
 * with a zeroed warp, edge, and fog copies the result back over the main target.
 */

layout(set = 0, binding = 0) uniform sampler2D scene_color;
layout(set = 1, binding = 0) uniform sampler2D scene_depth;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform UnderwaterPushConstants {
	vec4 wave_x;     // aspect-scaled wave vector x per octave
	vec4 wave_y;     // wave vector y per octave
	vec4 wave_phase; // speed * wrapped scene time per octave
	vec4 dir_x;      // octave displacement x, w: red dispersion scale
	vec4 dir_y;      // octave displacement y, w: blue dispersion scale
	vec4 fog;        // medium color rgb, w: ramped maximum opacity
	vec4 depth;      // density, zNear*zFar, zNear, zFar-zNear
	vec4 tune;       // warp uv x, warp uv y, ramped edge, ramped fog
} pc;

void main() {
	vec2 uv = frag_tex_coord;
	/* Three octave phases at once.  The x coefficients arrive aspect-scaled, so
	 * the field stays isotropic without a separate aspect multiply here. */
	vec3 wave = sin(pc.wave_x.xyz * uv.x + pc.wave_y.xyz * uv.y + pc.wave_phase.xyz);
	/* Unit directions times amplitudes that sum to one bound the field to the
	 * unit disc, so the displacement never exceeds the authored pixel count. */
	vec2 offset = vec2(dot(pc.dir_x.xyz, wave), dot(pc.dir_y.xyz, wave)) * pc.tune.xy;

	/* Short wavelengths refract further: red is displaced least, blue most.
	 * Bounding the sample to the texel-centre interval stretches the border
	 * rather than wrapping it, which is the least distracting full-screen warp
	 * boundary.  The bounds widen to the fragment's own coordinate inside that
	 * margin, so a zero displacement is preserved exactly and the copy draw
	 * stays a true passthrough instead of duplicating the outermost texels. */
	vec2 lo = min(uv, vec2(0.002));
	vec2 hi = max(uv, vec2(0.998));
	vec2 uv_g = clamp(uv + offset, lo, hi);
	vec2 uv_r = clamp(uv + offset * pc.dir_x.w, lo, hi);
	vec2 uv_b = clamp(uv + offset * pc.dir_y.w, lo, hi);
	vec3 color = vec3(texture(scene_color, uv_r).r,
		texture(scene_color, uv_g).g,
		texture(scene_color, uv_b).b);

	/* Edge falloff over the screen ellipse, in unscaled device coordinates. */
	float radius = length(uv * 2.0 - 1.0);
	float edge = clamp((radius - 0.55) / (1.30 - 0.55), 0.0, 1.0);
	color *= 1.0 - pc.tune.z * (edge * edge * (3.0 - 2.0 * edge));

	/* Beer-Lambert absorption against the opaque scene depth, sampled at the
	 * same refracted coordinate so the medium follows the warped view.  Depth is
	 * reversed on this backend, so the stored value grows toward the near plane
	 * and the denominator counts up from zNear instead of down from zFar. */
	float stored = texture(scene_depth, uv_g).r;
	float scene_distance = pc.depth.y /
		max(pc.depth.z + stored * pc.depth.w, 0.0001);
	float amount = 1.0 - exp(-pc.depth.x * scene_distance);
	float alpha = min(amount * pc.tune.w, pc.fog.w);
	out_color = vec4(mix(color, pc.fog.rgb, alpha), 1.0);
}
