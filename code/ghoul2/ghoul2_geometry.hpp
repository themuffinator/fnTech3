/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GHOUL2_GEOMETRY_HPP
#define FNTECH3_GHOUL2_GEOMETRY_HPP

/* Independently authored owned GLM geometry and weighted deformation.
 * Record/weight facts: OpenJK 1a6a643427aa347553e9073dac5570b33337c4d9,
 * codemp/rd-common/mdx_format.h; palette use and normal/UV semantics:
 * codemp/rd-vanilla/G2_misc.cpp::R_TransformEachSurface. Observation only.
 * The pose palette already includes the model's fixed root basis. Apply it
 * once, without an additional base-pose inverse or bolt-axis correction.
 * These results are model-space data for Fn rendering and collision. Entity
 * placement, surface overrides, material registration and drawing belong to
 * their consumers. Normals retain the weighted 3x3 result, without translation
 * or normalization; the rendering pipeline owns its final normal transform. */
#include "ghoul2_pose.hpp"
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace fntech3::ghoul2::geometry {
namespace model = fntech3::jka::model;
using Vector = std::array<float, 3>;
using TexCoord = std::array<float, 2>;
using Triangle = std::array<std::uint32_t, 3>;

/* Engine allocation limits, not claims about a retail format maximum. */
inline constexpr std::size_t MaxFileBytes = 32u * 1024u * 1024u;
inline constexpr std::size_t MaxSurfaces = 4096;
inline constexpr int MaxLODs = 16;
inline constexpr std::size_t MaxVertices = 1024u * 1024u;
inline constexpr std::size_t MaxTriangles = 1024u * 1024u;

enum class Error { None, Format, Bounds, Hierarchy, Weight, Numeric, Bone, Index };
inline const char *ErrorName(Error error) {
	switch (error) {
	case Error::None: return "none";
	case Error::Format: return "invalid GLM format";
	case Error::Bounds: return "GLM geometry extent or allocation limit";
	case Error::Hierarchy: return "invalid surface hierarchy";
	case Error::Weight: return "invalid vertex weights";
	case Error::Numeric: return "nonfinite vertex or deformation";
	case Error::Bone: return "deformation bone count out of range";
	case Error::Index: return "triangle vertex index out of range";
	}
	return "unknown geometry error";
}
struct Metadata {
	int index = -1;
	std::string name, shader;
	std::uint32_t flags = 0;
	int parent = -1;
	std::vector<int> children;
	std::size_t sourceOffset = 0, sourceBytes = 0;
};
struct Vertex {
	Vector position{}, normal{};
	TexCoord texCoord{};
};
struct WeightedVertex : Vertex {
	int count = 0;
	std::array<int, model::MaxWeightsPerVertex> bones{};
	std::array<float, model::MaxWeightsPerVertex> weights{};
};
struct Surface {
	Metadata metadata;
	std::vector<WeightedVertex> vertices;
	std::vector<Triangle> triangles;
};
struct Geometry {
	std::string name, animationName;
	int version = 0, lod = 0, numBones = 0;
	std::size_t sourceBytes = 0;
	std::vector<int> roots;
	std::vector<Surface> surfaces;
};
struct DeformedSurface {
	Metadata metadata;
	std::vector<Vertex> vertices;
	std::vector<Triangle> triangles;
};
struct DeformedGeometry {
	std::string name, animationName;
	int version = 0, lod = 0, numBones = 0;
	std::size_t sourceBytes = 0;
	std::vector<int> roots;
	std::vector<DeformedSurface> surfaces;
};

template<std::size_t Count>
inline bool Finite(const std::array<float, Count> &values) {
	for (float value : values) if (!pose::Finite(value)) return false;
	return true;
}

/* Parent-child agreement, uniqueness and acyclicity are required before any
 * consumer can safely traverse the surface tree. A format may have a forest. */
inline bool Hierarchy(const Geometry &geometry) {
	const auto count = geometry.surfaces.size();
	if (!count || count > MaxSurfaces || geometry.roots.empty()) return false;
	std::vector<unsigned char> edges(count, 0), roots(count, 0);
	for (int root : geometry.roots) {
		if (root < 0 || std::size_t(root) >= count || roots[std::size_t(root)]++) return false;
	}
	for (std::size_t i = 0; i < count; ++i) {
		const auto &metadata = geometry.surfaces[i].metadata;
		if (metadata.index != int(i) || metadata.parent < -1
			|| metadata.parent >= int(count) || metadata.parent == int(i)
			|| metadata.children.size() > count) return false;
		if ((metadata.parent == -1) != bool(roots[i])) return false;
		for (int child : metadata.children) {
			if (child < 0 || std::size_t(child) >= count || child == int(i)
				|| edges[std::size_t(child)]++
				|| geometry.surfaces[std::size_t(child)].metadata.parent != int(i)) return false;
		}
	}
	for (std::size_t i = 0; i < count; ++i) {
		if (edges[i] != (roots[i] ? 0 : 1)) return false;
		int cursor = int(i);
		std::size_t steps = 0;
		while (cursor >= 0) {
			if (++steps > count) return false;
			cursor = geometry.surfaces[std::size_t(cursor)].metadata.parent;
		}
	}
	return true;
}

inline bool ValidWeights(const WeightedVertex &vertex, int bones) {
	if (vertex.count < 1 || vertex.count > model::MaxWeightsPerVertex) return false;
	float total = 0;
	for (int i = 0; i < vertex.count; ++i) {
		const auto weight = vertex.weights[std::size_t(i)];
		if (vertex.bones[std::size_t(i)] < 0 || vertex.bones[std::size_t(i)] >= bones
			|| !pose::Finite(weight) || weight < -.000001f || weight > 1.000001f) return false;
		total += weight;
	}
	return pose::Finite(total) && std::fabs(total - 1.0f) <= .00001f;
}

/* Load one explicitly selected LOD. All data is copied; no asset pointer or
 * module memory survives the call. A failed load leaves the old result intact. */
inline Error Load(const void *data, std::size_t bytes, int skeletonBones,
	int level, Geometry &out)
{
	if (!data || bytes < model::MeshHeaderBytes || bytes > MaxFileBytes
		|| skeletonBones <= 0 || skeletonBones > pose::MaxBones) return Error::Bounds;
	model::Reader supplied(data, bytes);
	const int end = supplied.Int(160), lodCount = supplied.Int(144);
	const int surfaceCount = supplied.Int(152), boneCount = supplied.Int(140);
	if (end < int(model::MeshHeaderBytes) || std::size_t(end) > bytes
		|| lodCount <= 0 || lodCount > MaxLODs || level < 0 || level >= lodCount
		|| surfaceCount <= 0 || std::size_t(surfaceCount) > MaxSurfaces
		|| boneCount <= 0 || boneCount > skeletonBones) return Error::Bounds;
	/* Respect the declared file end as well as the actual allocation. */
	model::Mesh parsed;
	if (model::ParseMesh(data, std::size_t(end), skeletonBones, parsed) != model::Error::None) return Error::Format;
	model::Reader reader(data, std::size_t(end));
	Geometry staged;
	staged.name = static_cast<const char *>(data) + 8;
	staged.animationName = static_cast<const char *>(data) + 72;
	staged.version = parsed.version; staged.lod = level;
	staged.numBones = skeletonBones; staged.sourceBytes = std::size_t(end);
	staged.surfaces.resize(std::size_t(surfaceCount));
	std::size_t record = model::MeshHeaderBytes + std::size_t(surfaceCount) * 4;
	std::size_t childTotal = 0;
	for (int i = 0; i < surfaceCount; ++i) {
		auto &metadata = staged.surfaces[std::size_t(i)].metadata;
		metadata.index = i;
		metadata.name = static_cast<const char *>(data) + record;
		metadata.shader = static_cast<const char *>(data) + record + 68;
		metadata.flags = reader.Unsigned(record + 64);
		metadata.parent = reader.Int(record + 136);
		const int children = reader.Int(record + 140);
		if (std::size_t(children) > std::size_t(surfaceCount) - childTotal) return Error::Hierarchy;
		childTotal += std::size_t(children);
		if (metadata.parent == -1) staged.roots.push_back(i);
		for (int child = 0; child < children; ++child)
			metadata.children.push_back(reader.Int(record + model::SurfHierarchyBytes + std::size_t(child) * 4));
		record += model::SurfHierarchyBytes + std::size_t(children) * 4;
	}
	if (!Hierarchy(staged)) return Error::Hierarchy;
	std::size_t lod = parsed.lodOffset;
	for (int i = 0; i < level; ++i) lod += std::size_t(reader.Int(lod));
	const auto lodEnd = lod + std::size_t(reader.Int(lod));
	const auto table = lod + model::LodHeaderBytes;
	std::size_t at = table + std::size_t(surfaceCount) * 4;
	std::size_t vertexTotal = 0, triangleTotal = 0;
	for (int i = 0; i < surfaceCount; ++i) {
		const auto relative = reader.Int(table + std::size_t(i) * 4);
		if (relative < 0 || std::size_t(relative) != at - table) return Error::Bounds;
		const auto surfaceBytes = std::size_t(reader.Int(at + 36));
		if (at > lodEnd || surfaceBytes > lodEnd - at) return Error::Bounds;
		model::MeshSurface surface;
		if (!model::SurfaceAt(data, std::size_t(end), parsed, level, i, surface)) return Error::Bounds;
		const auto fitsSurface = [at, surfaceBytes](std::size_t offset, std::size_t stride, std::size_t count) {
			return offset >= at + model::SurfaceBytes && offset - at <= surfaceBytes
				&& (stride == 0 || count <= (surfaceBytes - (offset - at)) / stride);
		};
		if (!fitsSurface(surface.vertexOffset, model::VertexBytes + model::TexCoordBytes, std::size_t(surface.numVerts))
			|| !fitsSurface(surface.triangleOffset, model::TriangleBytes, std::size_t(surface.numTriangles))
			|| !fitsSurface(surface.boneReferenceOffset, 4, std::size_t(surface.numBoneReferences))) return Error::Bounds;
		if (std::size_t(surface.numVerts) > MaxVertices - vertexTotal
			|| std::size_t(surface.numTriangles) > MaxTriangles - triangleTotal) return Error::Bounds;
		vertexTotal += std::size_t(surface.numVerts); triangleTotal += std::size_t(surface.numTriangles);
		at += surfaceBytes;
	}
	/* Budget all allocations before copying any selected geometry. */
	for (int i = 0; i < surfaceCount; ++i) {
		model::MeshSurface raw;
		if (!model::SurfaceAt(data, std::size_t(end), parsed, level, i, raw)) return Error::Bounds;
		auto &surface = staged.surfaces[std::size_t(i)];
		surface.metadata.sourceOffset = raw.offset;
		surface.metadata.sourceBytes = std::size_t(reader.Int(raw.offset + 36));
		surface.vertices.resize(std::size_t(raw.numVerts));
		surface.triangles.resize(std::size_t(raw.numTriangles));
		for (int v = 0; v < raw.numVerts; ++v) {
			const auto offset = raw.vertexOffset + std::size_t(v) * model::VertexBytes;
			auto &vertex = surface.vertices[std::size_t(v)];
			model::SkinnedVertex weighted;
			if (!model::VertexAt(data, std::size_t(end), raw, v, weighted)) return Error::Weight;
			vertex.count = weighted.count;
			const auto packed = reader.Unsigned(offset + 24);
			unsigned int encodedTotal = 0;
			for (int w = 0; w < vertex.count; ++w) {
				vertex.bones[std::size_t(w)] = weighted.bone[w];
				vertex.weights[std::size_t(w)] = weighted.weight[w];
				if (w + 1 < vertex.count)
					encodedTotal += reader.Byte(offset + 28 + std::size_t(w))
						| ((packed >> (model::WeightTopBitsShift + w * 2)) & model::WeightTopBitsMask);
			}
			if (encodedTotal > 1023 || !ValidWeights(vertex, skeletonBones)) return Error::Weight;
			for (int axis = 0; axis < 3; ++axis) {
				vertex.position[std::size_t(axis)] = weighted.position[axis];
				vertex.normal[std::size_t(axis)] = reader.Float(offset + std::size_t(axis) * 4);
			}
			for (int axis = 0; axis < 2; ++axis)
				vertex.texCoord[std::size_t(axis)] = reader.Float(raw.texCoordOffset + std::size_t(v) * 8 + std::size_t(axis) * 4);
			if (!Finite(vertex.position) || !Finite(vertex.normal) || !Finite(vertex.texCoord)) return Error::Numeric;
		}
		for (int t = 0; t < raw.numTriangles; ++t) {
			int indices[3];
			if (!model::TriangleCorners(data, std::size_t(end), raw, t, indices)) return Error::Index;
			for (int corner = 0; corner < 3; ++corner)
				surface.triangles[std::size_t(t)][std::size_t(corner)] = std::uint32_t(indices[corner]);
		}
	}
	out = std::move(staged);
	return Error::None;
}

inline Error Deform(const Geometry &geometry, const std::vector<pose::Matrix> &palette,
	DeformedGeometry &out)
{
	if (geometry.numBones <= 0 || geometry.numBones > pose::MaxBones
		|| palette.size() < std::size_t(geometry.numBones) || palette.size() > std::size_t(pose::MaxBones)) return Error::Bone;
	if (!Hierarchy(geometry)) return Error::Hierarchy;
	for (const auto &matrix : palette) if (!pose::Finite(matrix)) return Error::Numeric;
	std::size_t vertices = 0, triangles = 0;
	for (const auto &surface : geometry.surfaces) {
		if (surface.vertices.size() > MaxVertices - vertices || surface.triangles.size() > MaxTriangles - triangles) return Error::Bounds;
		vertices += surface.vertices.size(); triangles += surface.triangles.size();
		for (const auto &triangle : surface.triangles) for (auto index : triangle)
			if (index >= surface.vertices.size()) return Error::Index;
	}
	DeformedGeometry staged;
	staged.name = geometry.name; staged.animationName = geometry.animationName;
	staged.version = geometry.version; staged.lod = geometry.lod;
	staged.numBones = geometry.numBones; staged.sourceBytes = geometry.sourceBytes;
	staged.roots = geometry.roots; staged.surfaces.resize(geometry.surfaces.size());
	for (std::size_t i = 0; i < geometry.surfaces.size(); ++i) {
		const auto &source = geometry.surfaces[i]; auto &surface = staged.surfaces[i];
		surface.metadata = source.metadata; surface.triangles = source.triangles;
		surface.vertices.resize(source.vertices.size());
		for (std::size_t v = 0; v < source.vertices.size(); ++v) {
			const auto &vertex = source.vertices[v]; auto &result = surface.vertices[v];
			if (!Finite(vertex.position) || !Finite(vertex.normal) || !Finite(vertex.texCoord)) return Error::Numeric;
			if (!ValidWeights(vertex, geometry.numBones)) return Error::Weight;
			result.texCoord = vertex.texCoord;
			for (int w = 0; w < vertex.count; ++w) {
				const auto &matrix = palette[std::size_t(vertex.bones[std::size_t(w)])];
				const auto weight = vertex.weights[std::size_t(w)];
				for (std::size_t axis = 0; axis < 3; ++axis) {
					float position = matrix[axis * 4 + 3], normal = 0;
					for (std::size_t component = 0; component < 3; ++component) {
						position += matrix[axis * 4 + component] * vertex.position[component];
						normal += matrix[axis * 4 + component] * vertex.normal[component];
					}
					result.position[axis] += weight * position;
					result.normal[axis] += weight * normal;
				}
			}
			if (!Finite(result.position) || !Finite(result.normal)) return Error::Numeric;
		}
	}
	out = std::move(staged);
	return Error::None;
}
} // namespace fntech3::ghoul2::geometry
#endif
