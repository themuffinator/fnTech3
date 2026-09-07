#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

const float PI = 3.14159265359;
const uint RTX_RT_COLORSPACE_SRGB = 1u;
const uint RTX_RT_MAX_SCENE_TEXTURES = 2048u;
const uint RTX_RT_INVALID_TEXTURE_INDEX = 0xFFFFFFFFu;
const uint RTX_RT_LIGHTING_VERTEX_LIGHTMAP = 1u << 0;
const uint RTX_RT_MATFLAG_MASKED = 1u << 0;
const uint RTX_RT_MATFLAG_EMISSIVE = 1u << 1;
const uint RTX_RT_MATFLAG_TRANSLUCENT = 1u << 2;
const uint RTX_RT_MATFLAG_PARTICLE = 1u << 6;
const uint RTX_RT_MATFLAG_EFFECT = 1u << 7;
const uint RTX_RT_MODE_FLAG_PARTICLE_VOLUME = 1u << 3;
const uint RTX_RT_MODE_FLAG_RASTER_REFERENCE = 1u << 4;
const uint RTX_RT_PAYLOAD_FLAG_HIT = 1u << 0;
const uint RTX_RT_PAYLOAD_FLAG_DYNAMIC = 1u << 1;
const uint RTX_RT_LIGHT_FLAG_CASTS_SHADOWS = 1u << 0;
const uint RTX_RT_LIGHT_FLAG_SHADOW_ONLY = 1u << 1;

struct RtRadiancePayload {
	vec3 color;
	float hitT;
	vec3 normal;
	uint flags;
	float shadowFactor;
	float shadowResponse;
	vec3 shadowLoss;
	vec3 nextOrigin;
	vec3 nextDirection;
	vec3 nextWeight;
	uint pathDepth;
};

struct RtPackedVertex {
	vec4 xyz;
	vec4 normal;
	vec4 tangent;
	vec2 texCoord;
	uint color;
	uint materialIndex;
	vec4 lightmapData;
};

struct RtGpuMaterial {
	vec4 baseColor;
	vec4 emissiveColorScale;
	vec4 pbrParams;
	uvec4 metadata;
	uvec4 textureInfo;
};

struct RtLight {
	vec4 positionRadius;
	vec4 colorType;
	vec4 directionSoftness;
	uvec4 metadata;
};

layout(location = 0) rayPayloadInEXT RtRadiancePayload payloadRadiance;
/* The shared any-hit group requires the same incoming payload ABI for every
 * ray type, even though shadow rays only consume the visibility scalar. */
layout(location = 1) rayPayloadEXT RtRadiancePayload payloadShadow;
hitAttributeEXT vec2 barycentrics;

layout(set = 0, binding = 0) uniform accelerationStructureEXT u_tlas;
layout(set = 0, binding = 2, std430) readonly buffer RtWorldVertexBuffer {
	RtPackedVertex vertices[];
} u_worldVertices;
layout(set = 0, binding = 3, std430) readonly buffer RtWorldIndexBuffer {
	uint indices[];
} u_worldIndices;
layout(set = 0, binding = 4, std430) readonly buffer RtWorldMaterialBuffer {
	RtGpuMaterial materials[];
} u_worldMaterials;
layout(set = 0, binding = 5, std430) readonly buffer RtDynamicVertexBuffer {
	RtPackedVertex vertices[];
} u_dynamicVertices;
layout(set = 0, binding = 6, std430) readonly buffer RtDynamicIndexBuffer {
	uint indices[];
} u_dynamicIndices;
layout(set = 0, binding = 7, std430) readonly buffer RtDynamicMaterialBuffer {
	RtGpuMaterial materials[];
} u_dynamicMaterials;
layout(set = 0, binding = 8, std430) readonly buffer RtLightBuffer {
	RtLight lights[];
} u_lights;
layout(set = 0, binding = 11, std430) readonly buffer RtTemporalParamsBuffer {
	vec4 prevCameraOriginTanHalfFovX;
	vec4 prevCameraForwardTanHalfFovY;
	vec4 prevCameraRightWidth;
	vec4 prevCameraUpHeight;
	vec4 dynamicMotionAndExposure;
	vec4 temporalParams0;
	vec4 denoiseParams;
	vec4 exposureParams;
	vec4 legacyColorParams;
	vec4 readabilityParams;
	uvec4 modes;
} u_temporal;
layout(set = 0, binding = 13) uniform sampler2D u_sceneTextures[RTX_RT_MAX_SCENE_TEXTURES];

layout(push_constant) uniform RtPushConstants {
	vec4 cameraOriginTanHalfFovX;
	vec4 cameraForwardTanHalfFovY;
	vec4 cameraRightWidth;
	vec4 cameraUpHeight;
	vec4 sunDirection;
	vec4 sunColorIntensity;
	uint debugMode;
	uint frameIndex;
	uint activeInstances;
	uint lightCount;
	uint worldMaterialCount;
	uint dynamicMaterialCount;
	uint shadowMode;
	float shadowSoftness;
	float indirectStrength;
	float reflectionStrength;
	float skyIntensity;
	float refractionIor;
	float sunIntensity;
	uint refractiveMode;
	uint indirectBounce;
} pc;

uint hash_u32(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

uint hash_world_cell(vec3 p)
{
	ivec3 cell = ivec3(floor(p * 0.25));
	uint h = hash_u32(uint(cell.x) * 73856093u);
	h = hash_u32(h ^ (uint(cell.y) * 19349663u));
	h = hash_u32(h ^ (uint(cell.z) * 83492791u));
	return h;
}

float rand01(inout uint state)
{
	state = hash_u32(state + 0x9e3779b9u);
	return float(state & 0x00FFFFFFu) / float(0x01000000u);
}

vec3 hash_color(uint value)
{
	uint x = value * 1664525u + 1013904223u;
	return vec3(
		float((x >> 0) & 255u),
		float((x >> 8) & 255u),
		float((x >> 16) & 255u)) / 255.0;
}

vec3 heatmap(float t)
{
	t = clamp(t, 0.0, 1.0);
	return vec3(
		smoothstep(0.45, 1.0, t),
		smoothstep(0.0, 0.65, t) * (1.0 - smoothstep(0.65, 1.0, t)),
		1.0 - smoothstep(0.0, 0.55, t));
}

float luminance(vec3 color)
{
	return max(dot(color, vec3(0.2126, 0.7152, 0.0722)), 0.0);
}

vec3 safe_normalize(vec3 value, vec3 fallback)
{
	float lengthSquared = dot(value, value);
	if (!(lengthSquared > 1e-12) || any(isnan(value)) || any(isinf(value))) {
		return fallback;
	}
	return value * inversesqrt(lengthSquared);
}

void basis_from_dir(vec3 n, out vec3 tangent, out vec3 bitangent)
{
	vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
	tangent = safe_normalize(cross(up, n), vec3(1.0, 0.0, 0.0));
	bitangent = cross(n, tangent);
}

vec3 sample_cone(vec3 dir, float coneAngle, inout uint rngState)
{
	if (coneAngle <= 1e-4) {
		return safe_normalize(dir, vec3(0.0, 0.0, 1.0));
	}

	float u = rand01(rngState);
	float v = rand01(rngState);
	float cosTheta = mix(cos(coneAngle), 1.0, u);
	float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
	float phi = 2.0 * PI * v;
	vec3 t;
	vec3 b;
	dir = safe_normalize(dir, vec3(0.0, 0.0, 1.0));
	basis_from_dir(dir, t, b);
	return safe_normalize(t * cos(phi) * sinTheta + b * sin(phi) * sinTheta + dir * cosTheta, dir);
}

vec3 sample_cosine_hemisphere(vec3 normal, inout uint rngState)
{
	float u1 = rand01(rngState);
	float u2 = rand01(rngState);
	float r = sqrt(u1);
	float theta = 2.0 * PI * u2;
	float x = r * cos(theta);
	float y = r * sin(theta);
	float z = sqrt(max(0.0, 1.0 - u1));
	vec3 t;
	vec3 b;
	basis_from_dir(normal, t, b);
	return safe_normalize(t * x + b * y + normal * z, normal);
}

vec3 evaluate_environment(vec3 rayDir)
{
	float up = clamp(rayDir.z * 0.5 + 0.5, 0.0, 1.0);
	vec3 sunColor = max(pc.sunColorIntensity.rgb * pc.sunIntensity, vec3(0.0));
	float sunLuma = max(dot(sunColor, vec3(0.2126, 0.7152, 0.0722)), 1e-4);
	vec3 sunTint = sunColor / sunLuma;
	vec3 skyBottom = mix(vec3(0.040, 0.050, 0.070), sunTint * 0.070, 0.55);
	vec3 skyTop = mix(vec3(0.24, 0.33, 0.52), sunTint * 0.300, 0.75);
	vec3 skyColor = mix(skyBottom, skyTop, up) * pc.skyIntensity;
	vec3 sunDir = safe_normalize(pc.sunDirection.xyz, vec3(0.0, 0.0, 1.0));
	float sunDisk = pow(max(dot(rayDir, sunDir), 0.0), 320.0);
	return skyColor + sunColor * sunDisk;
}

uint fetch_index(uint geometryClass, uint index)
{
	return geometryClass == 0u ? u_worldIndices.indices[index] : u_dynamicIndices.indices[index];
}

RtPackedVertex fetch_vertex(uint geometryClass, uint index)
{
	return geometryClass == 0u ? u_worldVertices.vertices[index] : u_dynamicVertices.vertices[index];
}

RtGpuMaterial fetch_material(uint geometryClass, uint index)
{
	uint maxCount = geometryClass == 0u ? max(pc.worldMaterialCount, 1u) : max(pc.dynamicMaterialCount, 1u);
	uint clamped = min(index, maxCount - 1u);
	return geometryClass == 0u ? u_worldMaterials.materials[clamped] : u_dynamicMaterials.materials[clamped];
}

float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
	return a2 / max(PI * denom * denom, 1e-5);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float k = (roughness + 1.0);
	k = (k * k) / 8.0;
	return NdotV / max(NdotV * (1.0 - k) + k, 1e-5);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float ggx1 = GeometrySchlickGGX(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

/* Standard GGX equations, independently expressed for the native integrator. */
float native_ggx_distribution(float NdotH, float roughness)
{
	float alphaSquared = pow(roughness, 4.0);
	float cosineSquared = NdotH * NdotH;
	float denominator = (1.0 - cosineSquared) + alphaSquared * cosineSquared;
	return alphaSquared / max(PI * denominator * denominator, 1e-12);
}

float native_ggx_masking(float cosine, float roughness)
{
	float alphaSquared = pow(roughness, 4.0);
	return 2.0 * cosine / max(cosine +
		sqrt(alphaSquared + (1.0 - alphaSquared) * cosine * cosine), 1e-6);
}

vec3 native_brdf(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 F0,
	float roughness, float metallic, float diffuseScale, float specularScale)
{
	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);
	if (NdotL <= 0.0 || NdotV <= 0.0) {
		return vec3(0.0);
	}
	vec3 H = safe_normalize(V + L, N);
	vec3 F = FresnelSchlick(max(dot(V, H), 0.0), F0);
	float D = native_ggx_distribution(max(dot(N, H), 0.0), roughness);
	float G = native_ggx_masking(NdotV, roughness) * native_ggx_masking(NdotL, roughness);
	return (vec3(1.0) - F) * (1.0 - metallic) * albedo * (diffuseScale / PI) +
		F * (specularScale * D * G / max(4.0 * NdotL * NdotV, 1e-7));
}

vec3 sample_ggx_reflection(vec3 N, vec3 V, float roughness, inout uint seed)
{
	float u = rand01(seed);
	float phi = 2.0 * PI * rand01(seed);
	float alphaSquared = pow(roughness, 4.0);
	float cosTheta = sqrt((1.0 - u) / max(1.0 + (alphaSquared - 1.0) * u, 1e-7));
	float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
	vec3 tangent;
	vec3 bitangent;
	basis_from_dir(N, tangent, bitangent);
	vec3 H = safe_normalize(tangent * (cos(phi) * sinTheta) +
		bitangent * (sin(phi) * sinTheta) + N * cosTheta, N);
	return reflect(-V, H);
}

float dielectric_fresnel(float cosine, float eta)
{
	float sinTransmittedSquared = eta * eta * max(1.0 - cosine * cosine, 0.0);
	if (sinTransmittedSquared >= 1.0) {
		return 1.0;
	}
	float cosTransmitted = sqrt(max(1.0 - sinTransmittedSquared, 0.0));
	float parallel = (cosine - eta * cosTransmitted) /
		max(cosine + eta * cosTransmitted, 1e-6);
	float perpendicular = (eta * cosine - cosTransmitted) /
		max(eta * cosine + cosTransmitted, 1e-6);
	return clamp(0.5 * (parallel * parallel + perpendicular * perpendicular), 0.0, 1.0);
}

vec3 offset_ray_origin(vec3 position, vec3 geometricNormal, vec3 direction)
{
	/* Scale beyond several float ULPs in large Q3 worlds, on the outgoing side. */
	float magnitude = max(max(abs(position.x), abs(position.y)), abs(position.z));
	float epsilon = max(0.002, magnitude * 9.5367431640625e-7);
	return position + geometricNormal * (dot(geometricNormal, direction) >= 0.0 ? epsilon : -epsilon);
}

void sample_native_continuation(vec3 position, vec3 geometricNormal, vec3 N, vec3 V,
	vec3 albedo, vec3 F0, float roughness, float metallic, float materialAlpha,
	uint materialFlags, inout uint seed)
{
	float diffuseScale = pc.indirectBounce > payloadRadiance.pathDepth ? max(pc.indirectStrength, 0.0) : 0.0;
	float specularScale = max(pc.reflectionStrength, 0.0);
	vec3 transmissionWeight = vec3(0.0);
	vec3 transmissionDirection = -V;
	bool translucent = (materialFlags & RTX_RT_MATFLAG_TRANSLUCENT) != 0u && pc.refractiveMode > 0u;
	if (translucent) {
		float ior = max(pc.refractionIor, 1.0);
		float eta = gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT ? 1.0 / ior : ior;
		float fresnel = dielectric_fresnel(max(dot(N, V), 0.0), eta);
		float etaScale = 1.0;
		if (pc.refractiveMode > 1u) {
			transmissionDirection = refract(-V, N, eta);
			etaScale = eta * eta;
		} else {
			/* Thin alpha glass traverses the original direction and medium. */
			fresnel = dielectric_fresnel(max(dot(N, V), 0.0), 1.0 / ior);
		}
		if (dot(transmissionDirection, transmissionDirection) < 1e-8) {
			/* Total internal reflection must stay in the incident medium. */
			payloadRadiance.nextDirection = reflect(-V, N);
			payloadRadiance.nextOrigin = offset_ray_origin(position, geometricNormal, payloadRadiance.nextDirection);
			payloadRadiance.nextWeight = vec3(1.0 - materialAlpha);
			return;
		}
		transmissionWeight = (1.0 - materialAlpha) * (1.0 - fresnel) * albedo * etaScale;
		diffuseScale *= materialAlpha;
		float dielectricF0 = (ior - 1.0) / (ior + 1.0);
		F0 = mix(vec3(dielectricF0 * dielectricF0), albedo, metallic);
	}

	vec3 viewFresnel = FresnelSchlick(max(dot(N, V), 0.0), F0);
	float diffuseImportance = luminance((vec3(1.0) - viewFresnel) * albedo) * (1.0 - metallic) * diffuseScale;
	float specularImportance = luminance(viewFresnel) * specularScale;
	float reflectionImportance = diffuseImportance + specularImportance;
	float transmissionImportance = luminance(transmissionWeight);
	float totalImportance = reflectionImportance + transmissionImportance;
	if (totalImportance <= 1e-6) {
		return;
	}
	float transmissionProbability = transmissionImportance / totalImportance;
	vec3 direction;
	vec3 weight;
	if (transmissionImportance > 0.0 && rand01(seed) < transmissionProbability) {
		direction = safe_normalize(transmissionDirection, -V);
		weight = transmissionWeight / max(transmissionProbability, 1e-7);
	} else {
		float specularProbability = specularImportance / max(reflectionImportance, 1e-7);
		if (diffuseImportance > 0.0 && specularImportance > 0.0) {
			specularProbability = clamp(specularProbability, 0.05, 0.95);
		}
		direction = rand01(seed) < specularProbability ?
			sample_ggx_reflection(N, V, roughness, seed) : sample_cosine_hemisphere(N, seed);
		float NdotL = max(dot(N, direction), 0.0);
		if (NdotL <= 0.0 || dot(geometricNormal, direction) <= 0.0) {
			return;
		}
		vec3 H = safe_normalize(V + direction, N);
		float NdotH = max(dot(N, H), 0.0);
		float specularPdf = native_ggx_distribution(NdotH, roughness) * NdotH /
			max(4.0 * abs(dot(V, H)), 1e-7);
		float pdf = mix(NdotL / PI, specularPdf, specularProbability) * (1.0 - transmissionProbability);
		weight = native_brdf(N, V, direction, albedo, F0, roughness, metallic,
			diffuseScale, specularScale) * (NdotL / max(pdf, 1e-7));
	}
	if (any(isnan(weight)) || any(isinf(weight))) {
		return;
	}
	payloadRadiance.nextDirection = direction;
	payloadRadiance.nextOrigin = offset_ray_origin(position, geometricNormal, direction);
	payloadRadiance.nextWeight = max(weight, vec3(0.0));
}

vec3 decode_albedo_sample(vec3 sampledColor, uint sampledColorSpace)
{
	/*
	 * sampledColorSpace describes the value returned by the bound image view.
	 * Hardware-sRGB views are therefore tagged LINEAR by the CPU; SRGB here
	 * is reserved for UNORM descriptors that still need the fallback decode.
	 */
	sampledColor = max(sampledColor, vec3(0.0));
	if (sampledColorSpace == RTX_RT_COLORSPACE_SRGB) {
		return pow(sampledColor, vec3(2.2));
	}
	return sampledColor;
}

float estimate_albedo_lod(
	uint textureIndex,
	vec3 p0,
	vec3 p1,
	vec3 p2,
	vec2 uv0,
	vec2 uv1,
	vec2 uv2,
	vec3 shadingNormal)
{
	vec3 edge1 = p1 - p0;
	vec3 edge2 = p2 - p0;
	vec2 uvEdge1 = uv1 - uv0;
	vec2 uvEdge2 = uv2 - uv0;
	float worldArea = length(cross(edge1, edge2));
	float uvArea = abs(
		uvEdge1.x * uvEdge2.y -
		uvEdge1.y * uvEdge2.x);
	ivec2 extent =
		textureSize(
			u_sceneTextures[nonuniformEXT(textureIndex)],
			0);
	float texelArea =
		uvArea *
		float(max(extent.x, 1)) *
		float(max(extent.y, 1));
	float texelsPerWorldUnit =
		sqrt(texelArea / max(worldArea, 1e-6));
	float pixelWorldFootprint =
		2.0 *
		max(gl_HitTEXT, 0.0) *
		max(pc.cameraForwardTanHalfFovY.w, 1e-4) /
		max(pc.cameraUpHeight.w, 1.0);
	float viewCosine = max(
		abs(dot(
			normalize(shadingNormal),
			-normalize(gl_WorldRayDirectionEXT))),
		0.2);
	float footprintTexels =
		texelsPerWorldUnit *
		pixelWorldFootprint /
		viewCosine;
	float maxLod = float(max(
		textureQueryLevels(
			u_sceneTextures[nonuniformEXT(textureIndex)]) - 1,
		0));

	return clamp(
		/*
		 * The area estimate is deliberately conservative and lacks true
		 * anisotropic ray differentials. Bias toward the sharper mip so
		 * Quake's deliberately high-frequency authored materials retain
		 * detail without falling back to the aliasing of forced LOD zero.
		 */
		log2(max(footprintTexels, 1.0)) - 2.0,
		0.0,
		maxLod);
}

float trace_shadow_visibility(vec3 origin, vec3 direction, float maxT)
{
	if (maxT <= 0.01) {
		return 1.0;
	}
	payloadShadow.shadowFactor = 0.0;
	payloadShadow.pathDepth = 0u;
	traceRayEXT(
		u_tlas,
		gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
		0xFF,
		0,
		0,
		1,
		origin,
		0.01,
		direction,
		maxT,
		1);
	return payloadShadow.shadowFactor;
}

void main()
{
	uint geometryClass = gl_InstanceCustomIndexEXT == 1u ? 1u : 0u;
	uint triBase = gl_PrimitiveID * 3u;
	uint i0 = fetch_index(geometryClass, triBase + 0u);
	uint i1 = fetch_index(geometryClass, triBase + 1u);
	uint i2 = fetch_index(geometryClass, triBase + 2u);
	RtPackedVertex v0 = fetch_vertex(geometryClass, i0);
	RtPackedVertex v1 = fetch_vertex(geometryClass, i1);
	RtPackedVertex v2 = fetch_vertex(geometryClass, i2);
	float b1 = barycentrics.x;
	float b2 = barycentrics.y;
	float b0 = max(0.0, 1.0 - b1 - b2);
	vec3 worldPos = v0.xyz.xyz * b0 + v1.xyz.xyz * b1 + v2.xyz.xyz * b2;
	vec3 normal = safe_normalize(v0.normal.xyz * b0 + v1.normal.xyz * b1 + v2.normal.xyz * b2,
		vec3(0.0, 0.0, 1.0));
	vec2 uv = v0.texCoord * b0 + v1.texCoord * b1 + v2.texCoord * b2;
	vec3 vertexColor =
		unpackUnorm4x8(v0.color).rgb * b0 +
		unpackUnorm4x8(v1.color).rgb * b1 +
		unpackUnorm4x8(v2.color).rgb * b2;
	RtGpuMaterial material = fetch_material(geometryClass, v0.materialIndex);
	vec4 albedoSample = vec4(1.0);
	uint albedoTextureIndex = material.textureInfo.x;
	bool hasAlbedoTexture = albedoTextureIndex != RTX_RT_INVALID_TEXTURE_INDEX && albedoTextureIndex < RTX_RT_MAX_SCENE_TEXTURES;
	if (hasAlbedoTexture) {
		float albedoLod = estimate_albedo_lod(
			albedoTextureIndex,
			v0.xyz.xyz,
			v1.xyz.xyz,
			v2.xyz.xyz,
			v0.texCoord,
			v1.texCoord,
			v2.texCoord,
			normal);
		albedoSample = textureLod(
			u_sceneTextures[nonuniformEXT(albedoTextureIndex)],
			uv,
			albedoLod);
		albedoSample.rgb = decode_albedo_sample(albedoSample.rgb, material.metadata.y);
	}
	vec3 albedo = material.baseColor.rgb * albedoSample.rgb;
	float materialAlpha = clamp(material.baseColor.a * albedoSample.a, 0.0, 1.0);
	float roughness = clamp(material.pbrParams.x, 0.04, 1.0);
	float metallic = clamp(material.pbrParams.y, 0.0, 1.0);
	uint materialFlags = material.metadata.x;
	vec3 emissive = material.emissiveColorScale.rgb * material.emissiveColorScale.w;
	if ((materialFlags & RTX_RT_MATFLAG_EFFECT) != 0u) {
		vec3 tint = max(vertexColor, vec3(0.05));
		albedo *= tint;
		emissive *= tint;
	}
	albedo = clamp(albedo, 0.0, 1.0);
	vec3 V = safe_normalize(-gl_WorldRayDirectionEXT, vec3(0.0, 0.0, 1.0));
	vec3 N = safe_normalize(normal, vec3(0.0, 0.0, 1.0));
	vec3 geometricNormal = safe_normalize(cross(v1.xyz.xyz - v0.xyz.xyz, v2.xyz.xyz - v0.xyz.xyz), N);
	float NdotV;
	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 Lo = vec3(0.0);
	float environmentDiffuseVisibility = 1.0;
	float shadowWeightTotal = 0.0;
	float visibleShadowWeight = 0.0;
	uint contributingLights = 0u;
	bool rasterReferenceCompat =
		(u_temporal.modes.w & RTX_RT_MODE_FLAG_RASTER_REFERENCE) != 0u;
	uint stableGeometrySeed =
		gl_PrimitiveID * 7477u + gl_InstanceCustomIndexEXT * 341u;
	uint temporalSeed =
		rasterReferenceCompat ? 0u : pc.frameIndex * 1597u;
	uint seed = hash_u32(stableGeometrySeed + temporalSeed + payloadRadiance.pathDepth * 0x9e3779b9u);
	seed = hash_u32(seed ^ hash_world_cell(worldPos) ^ floatBitsToUint(gl_HitTEXT));

	if (dot(N, V) < 0.0) {
		N = -N;
	}
	if (dot(geometricNormal, V) < 0.0) {
		geometricNormal = -geometricNormal;
	}
	/* Interpolated normals must remain on the visible geometric hemisphere. */
	if (dot(N, geometricNormal) <= 0.0) {
		N = geometricNormal;
	}
	NdotV = max(dot(N, V), 0.0);
	vec3 shadowOrigin = rasterReferenceCompat ? worldPos + N * 0.01 :
		offset_ray_origin(worldPos, geometricNormal, N);

	payloadRadiance.hitT = gl_HitTEXT;
	payloadRadiance.normal = N;
	payloadRadiance.flags = RTX_RT_PAYLOAD_FLAG_HIT | (geometryClass == 1u ? RTX_RT_PAYLOAD_FLAG_DYNAMIC : 0u);
	payloadRadiance.shadowFactor = 1.0;
	payloadRadiance.shadowLoss = vec3(0.0);
	/*
	 * The legacy hybrid keeps authored raster stages authoritative. Do not
	 * let RT visibility dim a material already identified as self-emissive;
	 * ordinary surfaces still receive the bounded compatibility shadow.
	 */
	payloadRadiance.shadowResponse =
		(materialFlags & RTX_RT_MATFLAG_EMISSIVE) != 0u ? 0.0 : 1.0;

	for (uint lightIndex = 0u; lightIndex < pc.lightCount; lightIndex++) {
		RtLight light = u_lights.lights[lightIndex];
		vec3 lightColor = max(light.colorType.rgb, vec3(0.0));
		vec3 L;
		float radianceScale = 1.0;
		float maxT = 100000.0;
		float NdotL;
		float visibility = 1.0;
		bool castsShadows =
			(light.metadata.x & RTX_RT_LIGHT_FLAG_CASTS_SHADOWS) != 0u;
		bool shadowOnly =
			(light.metadata.x & RTX_RT_LIGHT_FLAG_SHADOW_ONLY) != 0u;

		if (light.colorType.w > 0.5 && light.colorType.w < 1.5) {
			/* tr.sunDirection points from the world toward the visible sun. */
			L = safe_normalize(light.directionSoftness.xyz, vec3(0.0, 0.0, 1.0));
		} else {
			vec3 lightPosition = light.positionRadius.xyz;
			if (light.colorType.w >= 1.5) {
				vec3 segment = light.directionSoftness.xyz - light.positionRadius.xyz;
				float segmentLengthSquared = dot(segment, segment);
				if (segmentLengthSquared > 1e-6) {
					float segmentT = clamp(
						dot(worldPos - light.positionRadius.xyz, segment) / segmentLengthSquared,
						0.0,
						1.0);
					lightPosition += segment * segmentT;
				}
			}
			vec3 toLight = lightPosition - worldPos;
			float dist = length(toLight);
			float radius = max(light.positionRadius.w, 0.001);
			float invRadius = 1.0 / radius;
			if (dist >= radius || dist <= 1e-4) {
				continue;
			}
			L = toLight / dist;
			{
				float edge = max(1.0 - dist * invRadius, 0.0);
				float falloff = edge * edge;
				float distAtten = 1.0 / (1.0 + (dist * dist) * (invRadius * invRadius) * 2.0);
				radianceScale = falloff * distAtten;
			}
			maxT = dist - 0.02;
		}

		NdotL = max(dot(N, L), 0.0);
		if (NdotL <= 0.0) {
			continue;
		}

		if (castsShadows && !shadowOnly) {
			float shadowWeight = luminance(lightColor) * radianceScale * NdotL;
			shadowWeightTotal += shadowWeight;
		}

		if (castsShadows && pc.shadowMode > 0u) {
			if (pc.shadowMode > 1u) {
				float coneAngle = clamp(light.directionSoftness.w, 0.0, 1.0);
				uint shadowSeed = rasterReferenceCompat ?
					hash_u32(stableGeometrySeed ^
						hash_u32((lightIndex + 1u) * 0x9e3779b9u)) :
					seed;
				vec3 shadowDir0;
				vec3 shadowDir1;
				float vis0;
				float vis1;
				coneAngle *= (light.colorType.w > 0.5 && light.colorType.w < 1.5) ? 0.02 : 0.18;
				shadowDir0 = sample_cone(L, coneAngle, shadowSeed);
				shadowDir1 = sample_cone(L, coneAngle, shadowSeed);
				if (!rasterReferenceCompat) {
					seed = shadowSeed;
				}
				vis0 = trace_shadow_visibility(shadowOrigin, shadowDir0, maxT);
				vis1 = trace_shadow_visibility(shadowOrigin, shadowDir1, maxT);
				visibility = 0.5 * (vis0 + vis1);
			} else {
				visibility = trace_shadow_visibility(shadowOrigin, L, maxT);
			}
		}
		if (castsShadows && !shadowOnly) {
			visibleShadowWeight +=
				luminance(lightColor) * radianceScale * NdotL * visibility;
		}

		vec3 H = safe_normalize(V + L, N);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float D = DistributionGGX(NdotH, roughness);
		float G = GeometrySmith(NdotV, NdotL, roughness);
		vec3 F = FresnelSchlick(VdotH, F0);
		vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-5);
		vec3 kS = F;
		vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
		vec3 radiance = lightColor * radianceScale;
		vec3 directContribution =
			(kD * albedo / PI + specular) * radiance * NdotL;
		if (!rasterReferenceCompat) {
			directContribution = native_brdf(N, V, L, albedo, F0, roughness, metallic, 1.0, 1.0) * radiance * NdotL;
		}

		if (shadowOnly) {
			/*
			 * PMLIGHT already added this light to the sampled raster base.
			 * Report only the estimated blocked portion so raygen can remove
			 * that light without globally dimming authored lightmaps.
			 */
			payloadRadiance.shadowLoss +=
				directContribution * (1.0 - visibility);
			continue;
		}
		if (visibility <= 0.001) {
			continue;
		}

		Lo += directContribution * visibility;
		contributingLights++;
	}

	if (rasterReferenceCompat) {
		{
			vec3 reflectDir = reflect(-V, N);
			float coneAngle = roughness * roughness * 0.6;
			vec3 glossyDir = sample_cone(reflectDir, coneAngle, seed);
			if (dot(glossyDir, N) > 0.001 && pc.reflectionStrength > 0.0) {
				float environmentSpecularVisibility =
					trace_shadow_visibility(
						worldPos + N * 0.01,
						glossyDir,
						100000.0);
				vec3 envSpec = evaluate_environment(glossyDir);
				vec3 F = FresnelSchlick(NdotV, F0);
				/*
				 * The analytic sky is only a valid reflection source when the
				 * glossy ray can actually escape the scene. This remains a
				 * bounded environment reflection rather than pretending a
				 * blocked ray saw scene radiance.
				 */
				Lo += envSpec *
					F *
					pc.reflectionStrength *
					environmentSpecularVisibility;
			}
		}

		if (pc.indirectBounce != 0u) {
			vec3 diffuseDir = sample_cosine_hemisphere(N, seed);
			if (pc.shadowMode > 0u) {
				float sampledVisibility =
					trace_shadow_visibility(worldPos + N * 0.01, diffuseDir, 100000.0);
				/*
				 * One stochastic visibility sample is an ambient-occlusion
				 * refinement, not enough evidence to erase all diffuse energy in
				 * enclosed Quake spaces. Retain a bounded floor so authored
				 * albedo remains legible while temporal accumulation resolves the
				 * ray-tested occlusion.
				 */
				environmentDiffuseVisibility = mix(0.25, 1.0, sampledVisibility);
			}
		}
		/*
		 * Direct lights and their shadow rays remain authoritative, but Quake maps
		 * do not retain enough source-light transport to illuminate every
		 * interior texel from direct emitters alone. A bounded analytic
		 * environment-diffuse term preserves the legacy hybrid's appearance.
		 * The optional hemisphere ray above only occludes this compatibility
		 * baseline; native transport below instead follows scene radiance.
		 */
		Lo += evaluate_environment(N) *
			(albedo * (1.0 - metallic)) *
			pc.indirectStrength *
			environmentDiffuseVisibility;
	} else {
		/*
		 * Q3's retained lightmap is authored illumination, not an analytic sky
		 * substitute. It remains an explicit baked diffuse source at a path
		 * hit; the continuation below gathers actual visible scene radiance.
		 */
		uint lightmapIndex = material.textureInfo.y;
		float lightmapScale = uintBitsToFloat(material.textureInfo.w);
		if (!isnan(lightmapScale) && !isinf(lightmapScale) && lightmapScale > 0.0) {
			vec3 bakedDiffuse = vec3(0.0);
			if ((material.textureInfo.z & RTX_RT_LIGHTING_VERTEX_LIGHTMAP) != 0u) {
				/* r_vertexLight's collapsed BSP colors are baked irradiance,
				 * never a second tint on the material's albedo/BRDF. */
				bakedDiffuse = vertexColor;
			} else if (lightmapIndex != RTX_RT_INVALID_TEXTURE_INDEX && lightmapIndex < RTX_RT_MAX_SCENE_TEXTURES) {
				vec2 lightmapUv = v0.lightmapData.xy * b0 + v1.lightmapData.xy * b1 + v2.lightmapData.xy * b2;
				bakedDiffuse = textureLod(u_sceneTextures[nonuniformEXT(lightmapIndex)], lightmapUv, 0.0).rgb;
			}
			Lo += albedo * (1.0 - metallic) * max(bakedDiffuse, vec3(0.0)) * lightmapScale;
		}
		if ((materialFlags & RTX_RT_MATFLAG_TRANSLUCENT) != 0u && pc.refractiveMode > 0u) {
			Lo *= materialAlpha;
		}
	}

	Lo += emissive;

	if ((materialFlags & RTX_RT_MATFLAG_PARTICLE) != 0u &&
		(u_temporal.modes.w & RTX_RT_MODE_FLAG_PARTICLE_VOLUME) != 0u) {
		float viewFacing = clamp(1.0 - abs(dot(N, V)), 0.0, 1.0);
		float density = clamp(material.baseColor.a, 0.15, 0.9);
		vec3 phaseTint = mix(vec3(0.35), albedo, 0.75);
		vec3 envScatter = evaluate_environment(-V) * (0.35 + 0.65 * viewFacing);
		vec3 scatter = (envScatter + Lo * 0.35) * phaseTint * (0.5 + 0.5 * density);
		Lo = mix(Lo, scatter + emissive, 0.65);
	}

	if (rasterReferenceCompat && (materialFlags & RTX_RT_MATFLAG_TRANSLUCENT) != 0u && pc.refractiveMode > 0u) {
		float alpha = clamp(material.baseColor.a, 0.05, 0.95);
		vec3 transmissionDir = -V;
		if (pc.refractiveMode > 1u) {
			vec3 refracted = refract(-V, N, 1.0 / max(pc.refractionIor, 1.0));
			if (length(refracted) > 1e-4) {
				transmissionDir = refracted;
			}
		}
		{
			vec3 transmission = evaluate_environment(normalize(transmissionDir)) * albedo;
			vec3 F = FresnelSchlick(NdotV, vec3(0.04));
			float blend = clamp((F.r * 0.7) + alpha * 0.3, 0.05, 0.95);
			Lo = mix(transmission, Lo, blend);
		}
	}

	if (shadowWeightTotal > 1e-5) {
		payloadRadiance.shadowFactor = clamp(visibleShadowWeight / shadowWeightTotal, 0.0, 1.0);
	}

	if (pc.debugMode == 1u) {
		payloadRadiance.color = hash_color(gl_InstanceCustomIndexEXT + 1u);
		return;
	}
	if (pc.debugMode == 2u) {
		float b2 = max(0.0, 1.0 - barycentrics.x - barycentrics.y);
		payloadRadiance.color = vec3(barycentrics.x, barycentrics.y, b2);
		return;
	}
	if (pc.debugMode == 3u) {
		payloadRadiance.color = N * 0.5 + 0.5;
		return;
	}
	if (pc.debugMode == 4u) {
		payloadRadiance.color = albedo;
		return;
	}
	if (pc.debugMode == 5u) {
		payloadRadiance.color = vec3(roughness);
		return;
	}
	if (pc.debugMode == 6u) {
		payloadRadiance.color = emissive;
		return;
	}
	if (pc.debugMode == 7u) {
		float t = float(contributingLights) / max(float(pc.lightCount), 1.0);
		payloadRadiance.color = heatmap(t);
		return;
	}
	if (pc.debugMode == 8u) {
		payloadRadiance.color = vec3(metallic);
		return;
	}

	payloadRadiance.color = max(Lo, vec3(0.0));
	payloadRadiance.normal = N;
	if (!rasterReferenceCompat) {
		sample_native_continuation(worldPos, geometricNormal, N, V, albedo, F0,
			roughness, metallic, materialAlpha, materialFlags, seed);
	}
}
