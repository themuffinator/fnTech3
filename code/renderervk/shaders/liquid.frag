#version 450

layout(set = 0, binding = 0) uniform LiquidUniforms {
	vec4 eye_pos;
	vec4 liquid_params;              // wrapped time, warp pixels, pass strength, inverse target width
	vec4 liquid_info;                // type scale, impulse count, inverse target height, refraction pass
	vec4 liquid_impulse[8];          // model-space xyz, expanding ring radius
	vec4 liquid_amplitude[2];        // eight packed ripple pixel amplitudes
	vec4 liquid_reflect;             // material sheen color, reflection weight
	vec4 liquid_depth;               // x: depth reject enabled
	mat4 liquid_mvp;                 // model space -> clip space for reflection reprojection
};

layout(set = 1, binding = 0) uniform sampler2D scene_color;
layout(set = 2, binding = 0) uniform sampler2D scene_depth;

layout(location = 0) in vec4 frag_screen;
layout(location = 1) in vec3 frag_position;
layout(location = 2) in vec3 frag_normal;
layout(location = 3) in vec3 frag_view;

layout(location = 0) out vec4 out_color;

float ImpulseAmplitude(int index)
{
	return index < 4 ? liquid_amplitude[0][index] : liquid_amplitude[1][index - 4];
}

/* Shared ambient wave gradient; the octave constants must stay identical to
 * renderercommon/tr_liquid.h and the GLx GLSL / ARB implementations. */
vec2 LiquidWaveGradient(vec3 position, float time)
{
	vec2 gradient = vec2(0.0);
	gradient += vec2(0.66, 0.75) * (0.42 * cos(dot(position, vec3(0.0096, 0.0109, 0.0051)) + time * 0.85));
	gradient += vec2(-0.81, 0.59) * (0.33 * cos(dot(position, vec3(-0.0269, 0.0196, 0.0116)) + time * 1.40));
	gradient += vec2(0.42, -0.91) * (0.25 * cos(dot(position, vec3(0.0320, -0.0693, 0.0266)) + time * 2.30));
	return gradient;
}

void main()
{
	/* A degenerate normal or an eye exactly on the surface must not turn the
	 * whole fragment into NaN; the GLx GLSL and the per-vertex fallback
	 * substitute the same values. */
	float normal_length = length(frag_normal);
	vec3 normal = normal_length > 0.000001 ? frag_normal / normal_length : vec3(0.0, 0.0, 1.0);
	float view_length = length(frag_view);
	vec3 view_dir = view_length > 0.000001 ? frag_view / view_length : normal;
	vec2 wave_gradient = LiquidWaveGradient(frag_position, liquid_params.x);

	if (liquid_info.w > 0.5) {
		vec2 inverse_view = vec2(liquid_params.w, liquid_info.z);
		vec2 uv = frag_screen.xy / frag_screen.w;
		float distance_atten = clamp(320.0 / max(frag_screen.w, 1.0), 0.30, 1.0);
		/* grazing fade: the compressed wave field near the horizon would alias */
		distance_atten *= clamp(abs(dot(normal, view_dir)) * 4.0, 0.0, 1.0);
		vec2 ambient_pixels = wave_gradient * (liquid_params.y * distance_atten);
		vec2 ripple_pixels = vec2(0.0);

		int impulse_count = clamp(int(liquid_info.y + 0.5), 0, 8);
		for (int i = 0; i < impulse_count; ++i) {
			vec3 delta = frag_position - liquid_impulse[i].xyz;
			float height = dot(delta, normal);
			vec3 tangent_delta = delta - normal * height;
			float distance_to_center = length(tangent_delta);
			float radius = liquid_impulse[i].w;
			float width = 20.0 + radius * 0.12;
			float ring_signed = clamp((distance_to_center - radius) / width, -1.0, 1.0);
			float ring = (1.0 - abs(ring_signed)) * sin(ring_signed * 3.14159265);
			float height_fade = 1.0 - clamp(abs(height) / max(48.0, width * 3.0), 0.0, 1.0);
			vec2 screen_gradient = vec2(dFdx(distance_to_center), dFdy(distance_to_center));
			float gradient_length = length(screen_gradient);
			vec2 direction = gradient_length > 0.0001 ? screen_gradient / gradient_length : vec2(0.0);
			ripple_pixels += direction * ring * height_fade * ImpulseAmplitude(i);
		}

		vec2 edge = smoothstep(vec2(0.0), vec2(0.06), uv)
			* smoothstep(vec2(0.0), vec2(0.06), vec2(1.0) - uv);
		float edge_fade = min(edge.x, edge.y);
		vec2 sample_uv = clamp(uv + (ambient_pixels + ripple_pixels) * inverse_view * edge_fade,
			vec2(0.002), vec2(0.998));
		/* samples nearer than this fragment are foreground: keep the waterline
		 * crisp by falling back to the unwarped coordinate there. Depth is
		 * reversed on this backend, so nearer means a larger stored value. */
		if (liquid_depth.x > 0.5
			&& texture(scene_depth, sample_uv).r > gl_FragCoord.z + 0.00003)
			sample_uv = uv;
		vec3 scene = texture(scene_color, sample_uv).rgb;
		/* The underlay replaces the background at the liquid type's own
		 * opacity rather than blending a warped copy over the unwarped one:
		 * a partial blend of two different taps is a smear, not a refraction.
		 * r_liquidRefraction scales the displacement instead, and reaches the
		 * shader already folded into the warp pixels and ripple amplitudes. */
		float alpha = liquid_info.x;
		out_color = vec4(scene, alpha);
	} else {
		/* Bounded single-tap screen-space reflection of the immutable pre-
		 * transparency snapshot; the material sheen color is the fallback
		 * wherever the mirrored sample is invalid or leaves the screen. */
		vec3 tangent_ref = abs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
		vec3 tangent = normalize(cross(tangent_ref, normal));
		vec3 bitangent = cross(normal, tangent);
		vec3 wave_normal = normalize(normal
			+ (tangent * wave_gradient.x + bitangent * wave_gradient.y) * 0.12);
		float fresnel = 1.0 - abs(dot(wave_normal, view_dir));
		float fresnel2 = fresnel * fresnel;
		fresnel = fresnel2 * fresnel2 * fresnel;
		vec3 reflected = reflect(-view_dir, wave_normal);
		vec4 reflected_clip = liquid_mvp
			* vec4(frag_position + reflected * (384.0 + 0.75 * view_length), 1.0);
		vec2 reflected_uv = reflected_clip.xy / max(reflected_clip.w, 0.001) * 0.5 + 0.5;
		vec2 reflected_edge = smoothstep(vec2(0.0), vec2(0.08), reflected_uv)
			* smoothstep(vec2(0.0), vec2(0.08), vec2(1.0) - reflected_uv);
		float reflection_weight = step(16.0, reflected_clip.w)
			* min(reflected_edge.x, reflected_edge.y) * liquid_reflect.w;
		vec3 reflection_color = texture(scene_color,
			clamp(reflected_uv, vec2(0.002), vec2(0.998))).rgb;
		vec3 sheen_color = mix(liquid_reflect.rgb, reflection_color, reflection_weight);
		float alpha = liquid_info.x * clamp(liquid_params.z, 0.0, 1.0)
			* clamp(0.04 + fresnel * 0.56, 0.0, 0.60);
		out_color = vec4(sheen_color, alpha);
	}
}
