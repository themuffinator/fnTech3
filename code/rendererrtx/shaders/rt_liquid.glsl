// Optical interfaces for both tracing backends. The BSP remains the boundary;
// FnQL's wave field and interaction rings perturb its optical normal. Small
// authored vertex waves are replaced by this field, never frozen in a BLAS.
#ifndef RT_LIQUID_GLSL
#define RT_LIQUID_GLSL

// Exact unpolarised dielectric Fresnel, including the critical angle when
// leaving water. eta is incident IOR / transmitted IOR; cosI faces the ray.
float rt_liquid_fresnel(float cosI, float eta)
{
	cosI = clamp(cosI, 0.0, 1.0);
	if (eta == 1.0) return 0.0;
	float sinT2 = eta * eta * max(0.0, 1.0 - cosI * cosI);
	if (sinT2 >= 1.0) return 1.0;
	float cosT = sqrt(max(0.0, 1.0 - sinT2));
	float rs = (eta * cosI - cosT) / max(eta * cosI + cosT, 1e-6);
	float rp = (cosI - eta * cosT) / max(cosI + eta * cosT, 1e-6);
	return clamp(0.5 * (rs * rs + rp * rp), 0.0, 1.0);
}

vec3 rt_liquid_normal(vec3 P, vec3 outward)
{
	float time = u_frame.liquidParams.x;
	vec2 gradient = vec2(0.66, 0.75) *
		(0.42 * cos(dot(P, vec3(0.0096, 0.0109, 0.0051)) + time * 0.85));
	gradient += vec2(-0.81, 0.59) *
		(0.33 * cos(dot(P, vec3(-0.0269, 0.0196, 0.0116)) + time * 1.40));
	gradient += vec2(0.42, -0.91) *
		(0.25 * cos(dot(P, vec3(0.0320, -0.0693, 0.0266)) + time * 2.30));
	vec3 reference = abs(outward.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
	vec3 tangent = normalize(cross(reference, outward));
	vec3 bitangent = cross(outward, tangent);
	vec3 slope = (tangent * gradient.x + bitangent * gradient.y) * u_frame.liquidParams.y;
	for (int i = 0; i < min(int(u_frame.liquidParams.z), 8); i++) {
		vec3 delta = P - u_frame.liquidImpulses[i].xyz;
		float height = dot(delta, outward);
		vec3 radial = delta - height * outward;
		float distance = length(radial);
		float radius = u_frame.liquidImpulses[i].w;
		float width = 20.0 + radius * 0.12;
		float x = clamp((distance - radius) / width, -1.0, 1.0);
		float ring = (1.0 - abs(x)) * sin(x * PI);
		float fade = 1.0 - clamp(abs(height) / max(48.0, width * 3.0), 0.0, 1.0);
		slope += radial / max(distance, 0.001) * ring * fade * u_frame.liquidAmplitudes[i / 4][i & 3];
	}
	return normalize(outward + slope * 0.12);
}

struct RtLiquidInterface {
	vec3 normal;
	vec3 reflected;
	vec3 transmitted;
	vec3 extinction;
	float fresnel;
	bool entering;
};

RtLiquidInterface rt_liquid_interface(RtMaterial m, RtTriangle tri, RtHitAttribs h,
	vec3 P, vec3 geometric, vec3 dir, vec3 tint)
{
	RtLiquidInterface liq;
	// Q3 winds triangles clockwise; vertex normals retain the authored
	// outward side even after orient_normals faces the shading hit to the eye.
	vec3 outward = safe_normalize(tri.v0.normal.xyz + tri.v1.normal.xyz + tri.v2.normal.xyz, -geometric);
	liq.entering = dot(dir, outward) < 0.0;
	liq.normal = rt_liquid_normal(P, outward) * (liq.entering ? 1.0 : -1.0);
	float eta = liq.entering ? 1.0 / m.params.w : m.params.w;
	liq.reflected = reflect(dir, liq.normal);
	liq.transmitted = refract(dir, liq.normal, eta);
	// A tilted optical normal must not send a ray through the wrong side
	// of the actual interface at grazing angles (or immediately hit itself).
	if (dot(liq.normal, -dir) <= 0.0 || dot(liq.reflected, geometric) <= 0.0 ||
		(dot(liq.transmitted, liq.transmitted) > 0.0 && dot(liq.transmitted, geometric) >= 0.0)) {
		liq.normal = geometric;
		liq.reflected = reflect(dir, geometric);
		liq.transmitted = refract(dir, geometric, eta);
	}
	liq.fresnel = rt_liquid_fresnel(dot(-dir, liq.normal), eta);
	// Clear water preferentially absorbs red. The authored, animated colour
	// adds a small tint without repainting the transmitted scene. Coefficients
	// are presentation defaults per map unit, not gameplay/content constants.
	tint = max(tint, vec3(0.0));
	tint /= max(max(max(tint.r, tint.g), tint.b), 1.0);
	liq.extinction = vec3(0.0035, 0.0015, 0.0008) + (1.0 - tint) * 0.001;
	return liq;
}

vec3 rt_liquid_transmittance(vec3 extinction, float distance)
{
	return exp(-max(extinction, vec3(0.0)) * clamp(distance, 0.0, 65536.0));
}

// A specular interface preserves a pixel's narrow footprint. Carry it to
// the next hit so textureGrad can retain anisotropy through water instead
// of choosing the coarse, isotropic mip of a diffuse bounce.
struct RtLiquidFootprint {
	vec3 originDx;
	vec3 originDy;
	vec3 directionDx;
	vec3 directionDy;
	bool valid;
};

RtLiquidFootprint rt_liquid_footprint(RtHitAttribs h, vec3 dir, vec3 normal,
	float eta, bool reflected, vec3 directionDx, vec3 directionDy)
{
	RtLiquidFootprint f;
	f.originDx = h.dPdx;
	f.originDy = h.dPdy;
	vec3 dx = normalize(dir + directionDx), dy = normalize(dir + directionDy);
	vec3 L = reflected ? reflect(dir, normal) : refract(dir, normal, eta);
	vec3 Lx = reflected ? reflect(dx, normal) : refract(dx, normal, eta);
	vec3 Ly = reflected ? reflect(dy, normal) : refract(dy, normal, eta);
	f.directionDx = Lx - L;
	f.directionDy = Ly - L;
	f.valid = h.gradients && dot(Lx, Lx) > 0.5 && dot(Ly, Ly) > 0.5;
	return f;
}

#endif
