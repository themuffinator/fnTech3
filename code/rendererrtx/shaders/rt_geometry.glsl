// Triangle lookup for a ray hit. Requires rt_common.glsl and rt_bindings.glsl.

#ifndef RT_GEOMETRY_GLSL
#define RT_GEOMETRY_GLSL

struct RtTriangle {
	RtVertex v0;
	RtVertex v1;
	RtVertex v2;
	bool dynamic;
};

// A class's BLAS has an opaque geometry (index 0) and a masked one (index 1);
// their primitives start at the offsets the frame block records.
uint rt_global_primitive(uint customIndex, uint geometryIndex, uint primitive)
{
	uint cls = min(instance_class(customIndex), RT_CLASS_COUNT - 1u);
	uvec4 bases = u_frame.classPrims[cls];
	return (geometryIndex == 0u ? bases.x : bases.y) + primitive;
}

bool rt_class_dynamic(uint customIndex)
{
	uint cls = instance_class(customIndex);
	return cls == RT_CLASS_DYNAMIC || cls == RT_CLASS_WEAPON || cls == RT_CLASS_BODY;
}

// This lane is stamped from RF_NOSHADOW after pose extraction, independently
// of material alpha, first-person visibility and reflection visibility.
bool rt_casts_shadow(RtTriangle tri)
{
	return !tri.dynamic || tri.v0.normal.w >= 0.0;
}

RtTriangle rt_fetch_triangle(uint customIndex, uint geometryIndex, uint primitive)
{
	RtTriangle tri;
	uint base = rt_global_primitive(customIndex, geometryIndex, primitive) * 3u;
	tri.dynamic = rt_class_dynamic(customIndex);
	if (tri.dynamic) {
		tri.v0 = u_dynamicVertices.vertices[u_dynamicIndices.indices[base + 0u]];
		tri.v1 = u_dynamicVertices.vertices[u_dynamicIndices.indices[base + 1u]];
		tri.v2 = u_dynamicVertices.vertices[u_dynamicIndices.indices[base + 2u]];
	} else {
		tri.v0 = u_worldVertices.vertices[u_worldIndices.indices[base + 0u]];
		tri.v1 = u_worldVertices.vertices[u_worldIndices.indices[base + 1u]];
		tri.v2 = u_worldVertices.vertices[u_worldIndices.indices[base + 2u]];
	}
	return tri;
}

// Where a dynamic hit's surface point was last frame, from its vertices'
// previous positions; false when its entity is new this frame.
bool rt_previous_position(uint customIndex, uint geometryIndex, uint primitive, vec2 bary, out vec3 previous)
{
	uint base = rt_global_primitive(customIndex, geometryIndex, primitive) * 3u;
	vec4 p0 = u_dynamicPrevious.positions[u_dynamicIndices.indices[base + 0u]];
	vec4 p1 = u_dynamicPrevious.positions[u_dynamicIndices.indices[base + 1u]];
	vec4 p2 = u_dynamicPrevious.positions[u_dynamicIndices.indices[base + 2u]];
	previous = p0.xyz * (1.0 - bary.x - bary.y) + p1.xyz * bary.x + p2.xyz * bary.y;
	return p0.w > 0.5 && p1.w > 0.5 && p2.w > 0.5;
}

RtMaterial rt_triangle_material(RtTriangle tri)
{
	uint count = max(u_frame.counts.x, 1u);
	return u_materials.materials[min(tri.v0.materialIndex, count - 1u)];
}

// Texture-space length per world unit of the base coordinates.
float rt_triangle_uv_density(RtTriangle tri)
{
	vec3 e1 = tri.v1.xyz.xyz - tri.v0.xyz.xyz;
	vec3 e2 = tri.v2.xyz.xyz - tri.v0.xyz.xyz;
	vec2 t1 = tri.v1.texCoord - tri.v0.texCoord;
	vec2 t2 = tri.v2.texCoord - tri.v0.texCoord;
	float worldArea = length(cross(e1, e2));
	float uvArea = abs(t1.x * t2.y - t1.y * t2.x);
	return sqrt(uvArea / max(worldArea, 1e-8));
}

RtHitAttribs rt_hit_attribs(RtTriangle tri, vec2 bary, vec3 rayDir)
{
	RtHitAttribs h;
	float b1 = bary.x;
	float b2 = bary.y;
	float b0 = 1.0 - b1 - b2;
	vec4 c0 = unpackUnorm4x8(tri.v0.color);
	vec4 c1 = unpackUnorm4x8(tri.v1.color);
	vec4 c2 = unpackUnorm4x8(tri.v2.color);
	vec3 geometric = cross(tri.v1.xyz.xyz - tri.v0.xyz.xyz, tri.v2.xyz.xyz - tri.v0.xyz.xyz);

	h.uv = tri.v0.texCoord * b0 + tri.v1.texCoord * b1 + tri.v2.texCoord * b2;
	h.lightmapUv = tri.v0.lightmapData.xy * b0 + tri.v1.lightmapData.xy * b1 + tri.v2.lightmapData.xy * b2;
	h.position = tri.v0.xyz.xyz * b0 + tri.v1.xyz.xyz * b1 + tri.v2.xyz.xyz * b2;
	h.normal = safe_normalize(tri.v0.normal.xyz * b0 + tri.v1.normal.xyz * b1 + tri.v2.normal.xyz * b2,
		safe_normalize(geometric, vec3(0.0, 0.0, 1.0)));
	h.viewDir = -rayDir;
	h.color = c0 * b0 + c1 * b1 + c2 * b2;
	h.entityLight = vec4(0.0);
	h.world = !tri.dynamic;
	if (tri.dynamic) {
		// a model carries its entity's shaderTexCoord and shaderTime; a brush
		// model (w = 1) keeps its BSP lightmap coordinates in place of the first
		bool brush = tri.v0.lightmapData.w > 0.5;
		h.entityTexCoord = brush ? vec2(0.0) : tri.v0.lightmapData.xy;
		h.entityTime = tri.v0.lightmapData.z;
		if (!brush) {
			h.lightmapUv = vec2(0.0);
			// and the raster pass's light for each vertex, when the host
			// stamped it (RT_StampEntityLight), interpolated as raster's is
			uint w0 = floatBitsToUint(tri.v0.xyz.w);
			uint marker = w0 >> 24u;
			if (marker == RT_ENTITY_LIGHT_GRID || marker == RT_ENTITY_LIGHT_ALL) {
				h.entityLight.rgb = unpackUnorm4x8(w0).rgb * b0 +
					unpackUnorm4x8(floatBitsToUint(tri.v1.xyz.w)).rgb * b1 +
					unpackUnorm4x8(floatBitsToUint(tri.v2.xyz.w)).rgb * b2;
				h.entityLight.w = marker == RT_ENTITY_LIGHT_ALL ? 2.0 : 1.0;
			}
		}
	} else {
		h.entityTexCoord = vec2(0.0);
		h.entityTime = 0.0;
	}
	h.coneWidth = 0.0;
	h.uvDensity = rt_triangle_uv_density(tri);
	h.gradients = false;
	h.dPdx = vec3(0.0);
	h.dPdy = vec3(0.0);
	h.dUVdx = vec2(0.0);
	h.dUVdy = vec2(0.0);
	return h;
}

// Carry a ray's differentials (dD per pixel along x and y) to its hit at
// distance t on the triangle's plane, and the base uv across that footprint
// (Igehy, "Tracing Ray Differentials", 1999). A ray nearly in the plane keeps
// the ray cone's isotropic width instead.
void rt_hit_differentials(RtTriangle tri, vec3 dir, float t, vec3 dDdx, vec3 dDdy, inout RtHitAttribs h)
{
	vec3 e1 = tri.v1.xyz.xyz - tri.v0.xyz.xyz;
	vec3 e2 = tri.v2.xyz.xyz - tri.v0.xyz.xyz;
	vec3 n = cross(e1, e2);
	float nn = dot(n, n);
	float dn = dot(dir, n);
	if (!(nn > 1e-12) || !(abs(dn) > 1e-4 * sqrt(nn))) {
		return;
	}
	// dP = t dD + dt D, with dt keeping P on the plane
	vec3 dPdx = t * dDdx - dir * (t * dot(dDdx, n) / dn);
	vec3 dPdy = t * dDdy - dir * (t * dot(dDdy, n) / dn);
	// barycentric change: dP = db1 e1 + db2 e2
	vec2 bx = vec2(dot(cross(dPdx, e2), n), dot(cross(e1, dPdx), n)) / nn;
	vec2 by = vec2(dot(cross(dPdy, e2), n), dot(cross(e1, dPdy), n)) / nn;
	vec2 t1 = tri.v1.texCoord - tri.v0.texCoord;
	vec2 t2 = tri.v2.texCoord - tri.v0.texCoord;
	h.dPdx = dPdx;
	h.dPdy = dPdy;
	h.dUVdx = t1 * bx.x + t2 * bx.y;
	h.dUVdy = t1 * by.x + t2 * by.y;
	h.gradients = all(lessThan(abs(vec4(h.dUVdx, h.dUVdy)), vec4(1e6)));
}

vec3 rt_triangle_geometric_normal(RtTriangle tri)
{
	return safe_normalize(cross(tri.v1.xyz.xyz - tri.v0.xyz.xyz, tri.v2.xyz.xyz - tri.v0.xyz.xyz),
		vec3(0.0, 0.0, 1.0));
}

// Whether a candidate hit on masked geometry counts: Quake III's face culling
// (front is the face Quake draws) and the shader's alpha test. The any-hit
// shader and the compute backend's traversal both ask it.
bool rt_hit_accepted(RtTriangle tri, vec2 bary, bool front, vec3 dir)
{
	RtMaterial m = rt_triangle_material(tri);
	// Opaque geometry normally bypasses any-hit altogether. An opaque
	// RF_NOSHADOW entity is routed here only to enforce its shadow policy;
	// preserve its original primary/reflection coverage in that case.
	if ((m.flags & RT_MF_MASKED) == 0u) return true;
	if ((m.flags & RT_MF_TWO_SIDED) == 0u) {
		bool cullFront = (m.flags & RT_MF_CULL_FRONT) != 0u;
		if (cullFront ? front : !front) {
			return false;
		}
	}
	if ((m.flags & RT_MF_MASKED) != 0u) {
		RtHitAttribs h = rt_hit_attribs(tri, bary, dir);
		if (!rt_material_covered(m, h)) {
			return false;
		}
	}
	return true;
}

#endif
