#version 450

layout(set = 0, binding = 0) uniform UBO {
	vec4 eyePos;
	vec4 lightPos;
	vec4 lightColor;
	vec4 lightVector;
	vec4 fogDistanceVector;
	vec4 fogDepthVector;
	vec4 fogEyeT;
	vec4 fogColor;
	vec4 texFactors;
	vec4 depthFadeInfo;
	vec4 depthFadeScale;
	vec4 depthFadeBias;
	vec4 dlightFactors;
	vec4 csmModelX;
	vec4 csmModelY;
	vec4 csmModelZ;
	vec4 csmAxisX;
	vec4 csmAxisY;
	vec4 csmAxisZ;
	vec4 csmInvExtents;
	vec4 csmSplitAtlas;
	vec4 csmShadowColor;
	vec4 csmView;
};

// texFactors: x the height of a row of cascades over the atlas's, y 1 where
// the receiver takes every caster's shadow, zw the filter's inner and outer
// taps. A baked receiver (renderercommon/tr_csm_receivers.h) gets the sun in
// lightmap units in lightColor and the way to the sun, in its own frame, in
// lightVector.

layout(set = 1, binding = 0) uniform sampler2D csm_shadow_texture;
layout(set = 2, binding = 0) uniform sampler2D baked_light_texture;

layout(location = 0) in vec3 world_pos;
layout(location = 1) in float view_dist;
layout(location = 2) in vec3 baked_color;
layout(location = 3) in vec2 lightmap_st;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 out_color;

float shadow_occluded(float stored_depth, float receiver_depth) {
	float forward = 1.0 - step(receiver_depth, stored_depth);
	float reversed = step(receiver_depth, stored_depth);
	return mix(forward, reversed, csmShadowColor.w);
}

void main() {
	if (view_dist < csmSplitAtlas.x || view_dist > csmSplitAtlas.y) {
		discard;
	}

	vec3 light_coord;
	light_coord.x = (dot(world_pos, csmAxisX.xyz) - csmAxisX.w) * csmInvExtents.x;
	light_coord.y = (dot(world_pos, csmAxisY.xyz) - csmAxisY.w) * csmInvExtents.y;
	light_coord.z = (dot(world_pos, csmAxisZ.xyz) - csmAxisZ.w) * csmInvExtents.z;

	if (any(lessThan(light_coord, vec3(0.0))) || any(greaterThan(light_coord, vec3(1.0)))) {
		discard;
	}

	float forward_depth = clamp(light_coord.x - csmInvExtents.w, 0.0, 1.0);
	float reversed_depth = clamp(1.0 - light_coord.x + csmInvExtents.w, 0.0, 1.0);
	float receiver_depth = mix(forward_depth, reversed_depth, csmShadowColor.w);
	vec2 texel = 1.0 / vec2(textureSize(csm_shadow_texture, 0));
	float atlas_x = csmSplitAtlas.z + light_coord.y * csmSplitAtlas.w;
	float atlas_x_min = csmSplitAtlas.z + texel.x;
	float atlas_x_max = csmSplitAtlas.z + csmSplitAtlas.w - texel.x;
	// the world's row of cascades is the image's lowest; each tap stays inside
	// it, and the models' row is one row height above it
	float row_height = texFactors.x;
	float row_min = 1.0 - row_height + 0.5 * texel.y;
	float row_max = 1.0 - 0.5 * texel.y;
	vec2 atlas_uv = vec2(
		clamp(atlas_x, atlas_x_min, atlas_x_max),
		clamp(1.0 - light_coord.z * row_height, row_min, row_max));
	vec2 filter_a = vec2(texFactors.z);
	vec2 filter_b = vec2(texFactors.w);
	vec2 taps[4] = vec2[4](
		vec2(-filter_b.y, -filter_a.x),
		vec2( filter_a.x, -filter_b.y),
		vec2(-filter_a.x,  filter_b.y),
		vec2( filter_b.y,  filter_a.x));
	float every_caster = 0.0;
	float models_only = 0.0;
	for (int i = 0; i < 4; i++) {
		vec2 tap = atlas_uv + texel * taps[i];
		tap.y = clamp(tap.y, row_min, row_max);
		float world = shadow_occluded(texture(csm_shadow_texture, tap).r, receiver_depth);
		float models = shadow_occluded(texture(csm_shadow_texture, tap - vec2(0.0, row_height)).r, receiver_depth);
		every_caster += max(world, models);
		// a baked receiver: the models' shadow only where the world's leaves it in the sun
		models_only += models * (1.0 - world);
	}
	float occlusion = 0.25 * mix(models_only, every_caster, texFactors.y);

	// the sun's share of the baked light, S max(0, N.toSun) / L, held to 0..1;
	// every caster takes the sun colour's share as it always did
	vec3 baked = texture(baked_light_texture, lightmap_st).rgb * baked_color;
	float normal_length = length(normal);
	float facing = normal_length > 0.0 ? max(dot(normal, lightVector.xyz) / normal_length, 0.0) : 0.0;
	vec3 share = clamp(lightColor.rgb * facing / max(baked, vec3(1.0 / 255.0)), 0.0, 1.0);
	share = max(share, vec3(texFactors.y));

	out_color = vec4(vec3(1.0) - occlusion * csmShadowColor.rgb * share, 1.0);
}
