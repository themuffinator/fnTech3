/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GHOUL2_MDR_HPP
#define FNTECH3_GHOUL2_MDR_HPP

/* Engine-owned one-frame MDR2 bridge. The canonical MDR reader/renderer own
 * the output contract; this independently authored builder emits its bounded
 * uncompressed records. GLM visibility, surface overrides, entity placement,
 * model registration and drawing are caller responsibilities. Each weight's
 * offset is the original bind vertex, carried once by the supplied deformation
 * palette. Bone IDs are global, and normals/UVs/winding are preserved. */
#include "ghoul2_geometry.hpp"
#include "../renderercommon/tr_mdr_format.h"
#include <limits>

namespace fntech3::ghoul2::mdr {
inline constexpr std::size_t MaxBytes = 32u * 1024u * 1024u;
/* code/qcommon/qfiles.h:38-39. These are current engine tessellation limits. */
inline constexpr std::size_t MaxVertices = 2000;
inline constexpr std::size_t MaxIndices = 12000;
#ifdef SHADER_MAX_VERTEXES
static_assert(MaxVertices == SHADER_MAX_VERTEXES, "MDR bridge vertex limit must match engine tessellation");
static_assert(MaxIndices == SHADER_MAX_INDEXES, "MDR bridge index limit must match engine tessellation");
#endif
enum class Error { None, Empty, Geometry, Surface, Bone, Bounds, Name, Weight, Numeric, Index };
inline const char *ErrorName(Error error) {
	switch(error) {
	case Error::None: return "none";
	case Error::Empty: return "no visible triangles";
	case Error::Geometry: return "invalid owned GLM geometry";
	case Error::Surface: return "invalid visible surface selection";
	case Error::Bone: return "MDR deformation bone limit";
	case Error::Bounds: return "MDR geometry extent or allocation limit";
	case Error::Name: return "MDR name or shader field limit";
	case Error::Weight: return "invalid MDR vertex weights";
	case Error::Numeric: return "nonfinite MDR deformation or bounds";
	case Error::Index: return "MDR triangle vertex index out of range";
	}
	return "unknown MDR bridge error";
}

namespace detail {
inline bool Name(const std::string &value) {
	return value.size() < 64 && value.find('\0') == std::string::npos;
}
inline bool Add(std::size_t &size, std::size_t count, std::size_t stride) {
	if (!stride || size > MaxBytes || count > (MaxBytes - size) / stride) return false;
	size += count * stride; return true;
}
inline void U32(std::vector<unsigned char> &bytes, std::size_t at, std::uint32_t value) {
	for(std::size_t i=0;i<4;++i) bytes[at+i] = static_cast<unsigned char>(value >> (i*8));
}
inline void Float(std::vector<unsigned char> &bytes, std::size_t at, float value) {
	std::uint32_t bits; std::memcpy(&bits,&value,4); U32(bytes,at,bits);
}
inline void Name(std::vector<unsigned char> &bytes, std::size_t at, const std::string &value) {
	std::memcpy(bytes.data()+at,value.data(),value.size());
}
inline bool Skin(const geometry::WeightedVertex &vertex, const std::vector<pose::Matrix> &palette,
	geometry::Vector &position, geometry::Vector &normal)
{
	position = {}; normal = {};
	for(int w=0;w<vertex.count;++w) {
		const auto &matrix = palette[std::size_t(vertex.bones[std::size_t(w)])];
		const float weight = vertex.weights[std::size_t(w)];
		for(std::size_t axis=0;axis<3;++axis) {
			float p = matrix[axis*4+3], n = 0;
			for(std::size_t component=0;component<3;++component) {
				p += matrix[axis*4+component]*vertex.position[component];
				n += matrix[axis*4+component]*vertex.normal[component];
			}
			position[axis] += weight*p; normal[axis] += weight*n;
		}
	}
	return geometry::Finite(position) && geometry::Finite(normal);
}
struct SurfacePlan {
	std::size_t bytes = FN_MDR_SURFACE_BYTES, vertexBytes = 0;
	std::vector<std::uint32_t> references;
};
} // namespace detail

namespace detail {
inline Error Inspect(const geometry::Geometry &geometry, const std::vector<int> &visible,
	const std::vector<pose::Matrix> &deformation, std::vector<SurfacePlan> &plans,
	std::size_t &bytes, geometry::Vector &minimum, geometry::Vector &maximum,
	geometry::Vector &origin, float &radius)
{
	if (visible.empty()) return Error::Empty;
	if (!geometry::Hierarchy(geometry) || geometry.version != geometry::model::MeshVersion
		|| geometry.sourceBytes > geometry::MaxFileBytes || geometry.lod < 0
		|| geometry.lod >= geometry::MaxLODs) return Error::Geometry;
	if (geometry.numBones <= 0 || geometry.numBones > FN_MDR_MAX_BONES
		|| deformation.size() < std::size_t(geometry.numBones)
		|| deformation.size() > FN_MDR_MAX_BONES) return Error::Bone;
	for (const auto &matrix : deformation) if (!pose::Finite(matrix)) return Error::Numeric;
	if (!detail::Name(geometry.name)) return Error::Name;
	if (visible.size() > geometry.surfaces.size()) return Error::Surface;
	std::vector<unsigned char> selected(geometry.surfaces.size(),0);
	plans.resize(visible.size());
	bytes = FN_MDR_HEADER_BYTES + 56 + deformation.size()*48 + FN_MDR_LOD_BYTES;
	bool haveVertex = false, haveTriangle = false;
	for(std::size_t s=0;s<visible.size();++s) {
		const int index = visible[s];
		if (index < 0 || std::size_t(index) >= geometry.surfaces.size()
			|| selected[std::size_t(index)]++) return Error::Surface;
		const auto &surface = geometry.surfaces[std::size_t(index)]; auto &plan = plans[s];
		if (!detail::Name(surface.metadata.name) || !detail::Name(surface.metadata.shader)) return Error::Name;
		if (surface.vertices.size() >= MaxVertices || surface.triangles.size() > (MaxIndices-1)/3) return Error::Bounds;
		std::array<unsigned char,FN_MDR_MAX_BONES> references{};
		for(const auto &triangle : surface.triangles) for(auto vertex : triangle)
			if (vertex >= surface.vertices.size()) return Error::Index;
		haveTriangle = haveTriangle || !surface.triangles.empty();
		for(const auto &vertex : surface.vertices) {
			if (!geometry::Finite(vertex.position) || !geometry::Finite(vertex.normal)
				|| !geometry::Finite(vertex.texCoord)) return Error::Numeric;
			if (!geometry::ValidWeights(vertex,geometry.numBones)) return Error::Weight;
			if (!detail::Add(plan.vertexBytes,1,FN_MDR_VERTEX_BYTES)
				|| !detail::Add(plan.vertexBytes,std::size_t(vertex.count),FN_MDR_WEIGHT_BYTES)) return Error::Bounds;
			for(int w=0;w<vertex.count;++w) {
				const auto bone = std::size_t(vertex.bones[std::size_t(w)]);
				if (!references[bone]) { references[bone]=1; plan.references.push_back(std::uint32_t(bone)); }
			}
			geometry::Vector position, normal;
			if (!detail::Skin(vertex,deformation,position,normal)) return Error::Numeric;
			if (!haveVertex) { minimum=position; maximum=position; haveVertex=true; }
			else for(std::size_t axis=0;axis<3;++axis) {
				if (position[axis] < minimum[axis]) minimum[axis]=position[axis];
				if (position[axis] > maximum[axis]) maximum[axis]=position[axis];
			}
		}
		if (plan.vertexBytes && !detail::Add(plan.bytes,1,plan.vertexBytes)) return Error::Bounds;
		if (!detail::Add(plan.bytes,surface.triangles.size(),12)
			|| !detail::Add(plan.bytes,plan.references.size(),4)
			|| !detail::Add(bytes,1,plan.bytes)) return Error::Bounds;
	}
	if (!haveVertex || !haveTriangle) return Error::Empty;
	double radiusSquared=0;
	for(std::size_t axis=0;axis<3;++axis) {
		origin[axis] = float((double(minimum[axis])+maximum[axis])*.5);
		if (!pose::Finite(origin[axis])) return Error::Numeric;
		const double low = double(origin[axis])-minimum[axis], high = double(maximum[axis])-origin[axis];
		const double extent = low > high ? low : high;
		radiusSquared += extent*extent;
	}
	const double maximumRadius=std::numeric_limits<float>::max();
	if (radiusSquared>maximumRadius*maximumRadius) return Error::Numeric;
	radius = float(std::sqrt(radiusSquared));
	if (!pose::Finite(radius)) return Error::Numeric;
	if (double(radius)*radius < radiusSquared) radius=std::nextafter(radius,std::numeric_limits<float>::max());
	if (!pose::Finite(radius)) return Error::Numeric;
	return Error::None;
}
} // namespace detail

/* Empty is an explicit no-model/no-draw result. Like every failure it leaves
 * the previous output untouched; callers must examine the result before using
 * output. Bounds avoids producing or copying serialized geometry per pose. */
inline Error Bounds(const geometry::Geometry &geometry, const std::vector<int> &visible,
	const std::vector<pose::Matrix> &deformation, geometry::Vector &mins, geometry::Vector &maxs)
{
	std::vector<detail::SurfacePlan> plans;
	std::size_t bytes=0; geometry::Vector minimum{},maximum{},origin{}; float radius=0;
	const auto error=detail::Inspect(geometry,visible,deformation,plans,bytes,minimum,maximum,origin,radius);
	if (error!=Error::None) return error;
	mins=minimum; maxs=maximum; return Error::None;
}

/* None publishes an entirely owned, little-endian MDR2 byte buffer. */
inline Error Build(const geometry::Geometry &geometry, const std::vector<int> &visible,
	const std::vector<pose::Matrix> &deformation, std::vector<unsigned char> &out)
{
	std::vector<detail::SurfacePlan> plans;
	std::size_t bytes=0; geometry::Vector minimum{},maximum{},origin{}; float radius=0;
	const auto error=detail::Inspect(geometry,visible,deformation,plans,bytes,minimum,maximum,origin,radius);
	if (error!=Error::None) return error;
	std::vector<unsigned char> staged(bytes,0);
	detail::U32(staged,0,0x354d4452u); detail::U32(staged,4,2); detail::Name(staged,8,geometry.name);
	detail::U32(staged,72,1); detail::U32(staged,76,std::uint32_t(deformation.size()));
	detail::U32(staged,80,FN_MDR_HEADER_BYTES); detail::U32(staged,84,1);
	const auto lod = std::size_t(FN_MDR_HEADER_BYTES)+56+deformation.size()*48;
	detail::U32(staged,88,std::uint32_t(lod)); detail::U32(staged,96,std::uint32_t(bytes));
	detail::U32(staged,100,std::uint32_t(bytes));
	for(std::size_t axis=0;axis<3;++axis) {
		detail::Float(staged,104+axis*4,minimum[axis]); detail::Float(staged,116+axis*4,maximum[axis]);
		detail::Float(staged,128+axis*4,origin[axis]);
	}
	detail::Float(staged,140,radius); detail::Name(staged,144,"ghoul2_pose");
	for(std::size_t bone=0;bone<deformation.size();++bone) for(std::size_t element=0;element<12;++element)
		detail::Float(staged,160+bone*48+element*4,deformation[bone][element]);
	detail::U32(staged,lod,std::uint32_t(visible.size())); detail::U32(staged,lod+4,FN_MDR_LOD_BYTES);
	detail::U32(staged,lod+8,std::uint32_t(bytes-lod));
	std::size_t at = lod+FN_MDR_LOD_BYTES;
	for(std::size_t s=0;s<visible.size();++s) {
		const auto &surface=geometry.surfaces[std::size_t(visible[s])]; const auto &plan=plans[s];
		detail::Name(staged,at+4,surface.metadata.name); detail::Name(staged,at+68,surface.metadata.shader);
		detail::U32(staged,at+136,0u-std::uint32_t(at));
		detail::U32(staged,at+140,std::uint32_t(surface.vertices.size())); detail::U32(staged,at+144,FN_MDR_SURFACE_BYTES);
		detail::U32(staged,at+148,std::uint32_t(surface.triangles.size()));
		detail::U32(staged,at+152,std::uint32_t(FN_MDR_SURFACE_BYTES+plan.vertexBytes));
		detail::U32(staged,at+156,std::uint32_t(plan.references.size()));
		detail::U32(staged,at+160,std::uint32_t(FN_MDR_SURFACE_BYTES+plan.vertexBytes+surface.triangles.size()*12));
		detail::U32(staged,at+164,std::uint32_t(plan.bytes));
		auto vertexAt=at+FN_MDR_SURFACE_BYTES;
		for(const auto &vertex : surface.vertices) {
			for(std::size_t axis=0;axis<3;++axis) detail::Float(staged,vertexAt+axis*4,vertex.normal[axis]);
			for(std::size_t axis=0;axis<2;++axis) detail::Float(staged,vertexAt+12+axis*4,vertex.texCoord[axis]);
			detail::U32(staged,vertexAt+20,std::uint32_t(vertex.count));
			for(int w=0;w<vertex.count;++w) {
				const auto weightAt=vertexAt+FN_MDR_VERTEX_BYTES+std::size_t(w)*FN_MDR_WEIGHT_BYTES;
				detail::U32(staged,weightAt,std::uint32_t(vertex.bones[std::size_t(w)]));
				detail::Float(staged,weightAt+4,vertex.weights[std::size_t(w)]);
				for(std::size_t axis=0;axis<3;++axis) detail::Float(staged,weightAt+8+axis*4,vertex.position[axis]);
			}
			vertexAt += FN_MDR_VERTEX_BYTES+std::size_t(vertex.count)*FN_MDR_WEIGHT_BYTES;
		}
		for(const auto &triangle : surface.triangles) for(auto index : triangle) {
			detail::U32(staged,vertexAt,index); vertexAt+=4;
		}
		for(auto bone : plan.references) { detail::U32(staged,vertexAt,bone); vertexAt+=4; }
		at += plan.bytes;
	}
	out.swap(staged);
	return Error::None;
}
} // namespace fntech3::ghoul2::mdr
#endif
