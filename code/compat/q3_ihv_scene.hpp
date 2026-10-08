/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_SCENE_HPP
#define FNTECH3_Q3_IHV_SCENE_HPP

#include "q3_ihv_render_abi.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace fntech3::q3ihv::render {

static_assert(sizeof(fntech3_q3_ihv_render_entity_t) == 100);
static_assert(offsetof(fntech3_q3_ihv_render_entity_t, origin) == 40);
static_assert(offsetof(fntech3_q3_ihv_render_entity_t, flags) == 88);
static_assert(offsetof(fntech3_q3_ihv_render_entity_t, rgba) == 96);
static_assert(sizeof(fntech3_q3_ihv_render_light_t) == 28);
static_assert(sizeof(fntech3_q3_ihv_particle_t) == 16);
static_assert(sizeof(fntech3_q3_ihv_poly_vertex_t) == 24);
static_assert(sizeof(fntech3_q3_ihv_poly_t) == 16);
static_assert(sizeof(fntech3_q3_ihv_render_scene_t) == 152);
static_assert(offsetof(fntech3_q3_ihv_render_scene_t, overlayGeometry) == 76);
static_assert(offsetof(fntech3_q3_ihv_render_scene_t, time) == 108);
static_assert(offsetof(fntech3_q3_ihv_render_scene_t, visibleAreas) == 116);
static_assert(offsetof(fntech3_q3_ihv_render_scene_t, entities) == 124);
static_assert(offsetof(fntech3_q3_ihv_render_scene_t, polys) == 148);

enum class ResourceKind { Model, Skin, Shader };
class Memory {
public:
	virtual ~Memory() = default;
	// Return false without reading outside a currently readable module/host
	// range. The caller pins that memory for the synchronous Capture call.
	virtual bool Read(std::uint32_t address, void *output, std::size_t bytes) const = 0;
};
class Resources {
public:
	virtual ~Resources() = default;
	// Resolve only registrations owned by the current renderer lifetime and
	// of the requested kind. A skin must retain IHV's surface-ordinal semantics;
	// registering it as a retail named-surface skin is not equivalent.
	virtual bool Resolve(ResourceKind kind, std::uint32_t token, std::int32_t &handle) const = 0;
};

struct View {
	std::uint32_t reserved = 0, flags = 0;
	std::int32_t x = 0, y = 0, width = 0, height = 0, time = 0;
	float fovX = 0, fovY = 0, origin[3]{}, axis[3][3]{};
	float overlayGeometry[3]{}, overlayColor[4]{};
	std::int32_t overlayShader = 0;
	std::array<std::uint8_t, 32> visibleAreas{};
	// Original 0x441ad7/0x441bf3: absent mask means all areas visible, and
	// a set bit admits an area. Retail refdef_t uses the opposite convention.
	std::array<std::uint8_t, 32> BlockedAreas() const {
		auto result = visibleAreas;
		for (auto &value : result) value = static_cast<std::uint8_t>(~value);
		return result;
	}
	/* The overlay R_BlendBlobs (0x442700) draws after the scene while its
	 * colour has alpha: a square of its shader's first image, its corners at
	 * ten times (centre +- half-size) of the geometry words at eye depth 10,
	 * the modelview reset (0x442779) under the scene's own projection, its
	 * first texture corner top left. This is the screen rectangle that square
	 * covers inside the view, false when nothing would be drawn. A square
	 * with no positive size is not drawn. */
	bool OverlayRect(float &left, float &top, float &w, float &h) const {
		if (!(overlayColor[3] != 0.0f) || width <= 0 || height <= 0 || !(overlayGeometry[2] > 0.0f)) return false;
		const double halfX = std::tan(static_cast<double>(fovX) * 3.14159265358979323846 / 360.0);
		const double halfY = std::tan(static_cast<double>(fovY) * 3.14159265358979323846 / 360.0);
		if (!(halfX > 0.0) || !(halfY > 0.0) || !std::isfinite(halfX) || !std::isfinite(halfY)) return false;
		const double cx = overlayGeometry[0], cy = overlayGeometry[1], r = overlayGeometry[2];
		const double ndcLeft = (cx - r) / halfX, ndcRight = (cx + r) / halfX;
		const double ndcTop = (cy + r) / halfY, ndcBottom = (cy - r) / halfY;
		const double l = x + (ndcLeft + 1.0) * 0.5 * width, t = y + (1.0 - ndcTop) * 0.5 * height;
		const double rw = (ndcRight - ndcLeft) * 0.5 * width, rh = (ndcTop - ndcBottom) * 0.5 * height;
		if (!std::isfinite(l) || !std::isfinite(t) || !std::isfinite(rw) || !std::isfinite(rh)) return false;
		left = static_cast<float>(l); top = static_cast<float>(t);
		w = static_cast<float>(rw); h = static_cast<float>(rh);
		return true;
	}
};
struct Entity {
	std::int32_t model = 0, skin = 0, shader = 0;
	float axis[3][3]{}, origin[3]{}, oldorigin[3]{}, backlerp = 0;
	std::int32_t frame = 0, oldframe = 0, skinNum = 0;
	std::uint32_t flags = 0;
	bool nonNormalizedAxes = false;
	std::array<std::uint8_t, 4> rgba{};
};
struct Light { float origin[3]{}, color[3]{}, radius = 0; };
struct Particle { float origin[3]{}; std::array<std::uint8_t, 4> rgba{}; };
struct Vertex { float xyz[3]{}, st[2]{}; std::array<std::uint8_t, 4> rgba{}; };
struct Polygon { std::int32_t shader = 0; std::vector<Vertex> vertices; };
struct Scene {
	View view;
	std::vector<Entity> entities;
	std::vector<Light> lights;
	std::vector<Particle> particles;
	std::vector<Polygon> polygons;
};

/* Bounds for one owned submission, not claims about original engine maxima.
 * No supplied pointer or grouping link survives into Scene. Native effect
 * flags/overlay geometry remain IHV data until an exact backend adapter is
 * implemented; this reader does not reinterpret them as retail flags. */
struct Limits {
	std::uint32_t entities = 2048, lights = 32, particles = 4096, polygons = 256;
	std::uint32_t verticesPerPolygon = 1024, totalVertices = 65536;
};
enum class Result { Ok, Memory, Limit, View, Numeric, Resource, Polygon };

namespace detail {
inline std::uint32_t Word(const std::uint8_t *p) {
	return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
		(std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
inline std::int32_t Signed(const std::uint8_t *p) {
	const auto bits = Word(p); std::int32_t value; std::memcpy(&value, &bits, 4); return value;
}
inline bool Floats(const std::uint8_t *p, float *out, std::size_t count) {
	for (std::size_t i = 0; i < count; ++i) {
		const auto bits = Word(p + i * 4);
		// Stored-bit check remains valid with the engine's fast-math flags.
		if ((bits & 0x7f800000u) == 0x7f800000u) return false;
		std::memcpy(out + i, &bits, 4);
	}
	return true;
}
inline bool Read(const Memory &memory, std::uint32_t address, void *output, std::size_t bytes) {
	if (!bytes) return true;
	return address && std::uint64_t(bytes) <= 0x100000000ull - address && memory.Read(address, output, bytes);
}
inline Result Array(const Memory &memory, const std::uint8_t *pair, std::uint32_t limit,
	std::size_t stride, std::vector<std::uint8_t> &output) {
	const auto count = Word(pair), address = Word(pair + 4);
	if (count > 0x7fffffffu || count > limit || count > (std::numeric_limits<std::size_t>::max)() / stride)
		return Result::Limit;
	const auto bytes = std::size_t(count) * stride;
	if (bytes && (!address || std::uint64_t(bytes) > 0x100000000ull - address)) return Result::Memory;
	output.resize(bytes);
	return Read(memory, address, output.data(), bytes) ? Result::Ok : Result::Memory;
}
inline bool Resource(const Resources &resources, ResourceKind kind, std::uint32_t token,
	bool required, std::int32_t &handle) {
	if (!token) { handle = 0; return !required; }
	return resources.Resolve(kind, token, handle) && handle >= 0;
}
} // namespace detail

/* Stage the complete submission before publishing. Read/resolve failure or a
 * callback exception leaves output intact; no render callbacks run here. A
 * failed capture must be handled by the caller, never submitted as stale data. */
inline Result Capture(std::uint32_t address, const Memory &memory, const Resources &resources,
	Scene &output, const Limits &limits = {}) {
	using namespace detail;
	std::array<std::uint8_t, 152> bytes{};
	if (!Read(memory, address, bytes.data(), bytes.size())) return Result::Memory;
	const auto *p = bytes.data(); Scene next; auto &view = next.view;
	view.reserved = Word(p); view.x = Signed(p + 4); view.y = Signed(p + 8);
	view.width = Signed(p + 12); view.height = Signed(p + 16);
	view.time = Signed(p + 108); view.flags = Word(p + 112);
	if (!Floats(p + 20, &view.fovX, 1) || !Floats(p + 24, &view.fovY, 1) ||
		!Floats(p + 28, view.origin, 3) || !Floats(p + 76, view.overlayGeometry, 3) ||
		!Floats(p + 88, view.overlayColor, 4)) return Result::Numeric;
	for (unsigned i = 0; i < 3; ++i) if (!Floats(p + 40 + i * 12, view.axis[i], 3)) return Result::Numeric;
	if (view.width <= 0 || view.height <= 0 || view.width > 32768 || view.height > 32768 ||
		std::int64_t(view.x) + view.width > 0x7fffffff || std::int64_t(view.y) + view.height > 0x7fffffff ||
		view.fovX <= 0 || view.fovX >= 180 || view.fovY <= 0 || view.fovY >= 180) return Result::View;
	if (!Resource(resources, ResourceKind::Shader, Word(p + 104), view.overlayColor[3] != 0, view.overlayShader))
		return Result::Resource;
	view.visibleAreas.fill(255);
	if (Word(p + 116) && !Read(memory, Word(p + 116), view.visibleAreas.data(), view.visibleAreas.size())) return Result::Memory;
	std::vector<std::uint8_t> entities, lights, particles, polygons;
	for (auto spec : {std::make_pair(120u, std::make_pair(limits.entities, 100u)),
		std::make_pair(128u, std::make_pair(limits.lights, 28u)),
		std::make_pair(136u, std::make_pair(limits.particles, 16u)),
		std::make_pair(144u, std::make_pair(limits.polygons, 16u))}) {
		auto &array = spec.first == 120 ? entities : spec.first == 128 ? lights : spec.first == 136 ? particles : polygons;
		const auto result = Array(memory, p + spec.first, spec.second.first, spec.second.second, array);
		if (result != Result::Ok) return result;
	}
	next.entities.resize(entities.size() / 100);
	for (std::size_t i = 0; i < next.entities.size(); ++i) {
		const auto *record = entities.data() + i * 100; auto &entity = next.entities[i];
		entity.flags = Word(record + 88);
		// A null model is the original missing-model axes. Unknown nonzero
		// tokens still fail resolution; portal markers also legitimately use null.
		if (!Resource(resources, ResourceKind::Model, Word(record), false, entity.model) ||
			!Resource(resources, ResourceKind::Skin, Word(record + 80), false, entity.skin) ||
			!Resource(resources, ResourceKind::Shader, Word(record + 84), false, entity.shader)) return Result::Resource;
		for (unsigned axis = 0; axis < 3; ++axis) if (!Floats(record + 4 + axis * 12, entity.axis[axis], 3)) return Result::Numeric;
		if (!Floats(record + 40, entity.origin, 3) || !Floats(record + 56, entity.oldorigin, 3) ||
			!Floats(record + 72, &entity.backlerp, 1)) return Result::Numeric;
		entity.frame = Signed(record + 52); entity.oldframe = Signed(record + 68); entity.skinNum = Signed(record + 76);
		entity.nonNormalizedAxes = Word(record + 92) != 0;
		std::memcpy(entity.rgba.data(), record + 96, 4);
	}
	next.lights.resize(lights.size() / 28);
	for (std::size_t i = 0; i < next.lights.size(); ++i) {
		const auto *record = lights.data() + i * 28; auto &light = next.lights[i];
		if (!Floats(record, light.origin, 3) || !Floats(record + 12, light.color, 3) ||
			!Floats(record + 24, &light.radius, 1) || light.radius < 0) return Result::Numeric;
	}
	next.particles.resize(particles.size() / 16);
	for (std::size_t i = 0; i < next.particles.size(); ++i) {
		const auto *record = particles.data() + i * 16; auto &particle = next.particles[i];
		if (!Floats(record, particle.origin, 3)) return Result::Numeric;
		std::memcpy(particle.rgba.data(), record + 12, 4);
	}
	next.polygons.resize(polygons.size() / 16);
	std::uint32_t vertexCount = 0;
	for (std::size_t i = 0; i < next.polygons.size(); ++i) {
		const auto *record = polygons.data() + i * 16; auto &polygon = next.polygons[i];
		const auto count = Word(record + 4);
		if (count < 3) return Result::Polygon;
		if (count > limits.totalVertices - vertexCount) return Result::Limit;
		std::vector<std::uint8_t> vertices;
		const auto result = Array(memory, record + 4, limits.verticesPerPolygon, 24, vertices);
		if (result != Result::Ok) return result;
		vertexCount += count;
		if (!Resource(resources, ResourceKind::Shader, Word(record), true, polygon.shader)) return Result::Resource;
		polygon.vertices.resize(count);
		for (std::size_t v = 0; v < count; ++v) {
			const auto *data = vertices.data() + v * 24; auto &vertex = polygon.vertices[v];
			if (!Floats(data, vertex.xyz, 3) || !Floats(data + 12, vertex.st, 2)) return Result::Numeric;
			std::memcpy(vertex.rgba.data(), data + 20, 4);
		}
		// Do not follow record.next: the original renderer overwrites that word.
	}
	output = std::move(next); return Result::Ok;
}

} // namespace fntech3::q3ihv::render
#endif
