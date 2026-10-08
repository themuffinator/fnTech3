/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/q3_ihv_scene.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>

namespace r = fntech3::q3ihv::render;
static void Check(bool value, const char *why) {
	if (!value) { std::fprintf(stderr, "IHV scene: %s\n", why); std::abort(); }
}
static void Word(std::vector<std::uint8_t> &data, unsigned at, std::uint32_t word) {
	for (unsigned i = 0; i < 4; ++i) data.at(at + i) = static_cast<std::uint8_t>(word >> (i * 8));
}
static void Float(std::vector<std::uint8_t> &data, unsigned at, float value) {
	std::uint32_t word; std::memcpy(&word, &value, 4); Word(data, at, word);
}
struct Failure {};
struct Arena final : r::Memory {
	std::map<std::uint32_t, std::vector<std::uint8_t>> segments;
	mutable unsigned reads = 0;
	unsigned fail = 0;
	bool raises = false;
	bool Read(std::uint32_t address, void *output, std::size_t bytes) const override {
		if (++reads == fail) { if (raises) throw Failure{}; return false; }
		for (const auto &segment : segments) if (address >= segment.first) {
			const auto offset = std::size_t(address - segment.first);
			if (offset <= segment.second.size() && bytes <= segment.second.size() - offset) {
				std::memcpy(output, segment.second.data() + offset, bytes); return true;
			}
		}
		return false;
	}
};
struct Registry final : r::Resources {
	std::map<std::uint32_t, std::pair<r::ResourceKind, std::int32_t>> entries{
		{10, {r::ResourceKind::Model, 101}}, {20, {r::ResourceKind::Skin, 202}}, {30, {r::ResourceKind::Shader, 303}}};
	mutable unsigned queries = 0;
	unsigned fail = 0;
	bool raises = false;
	bool Resolve(r::ResourceKind kind, std::uint32_t token, std::int32_t &handle) const override {
		if (++queries == fail) { if (raises) throw Failure{}; return false; }
		const auto found = entries.find(token);
		if (found == entries.end() || found->second.first != kind) return false;
		handle = found->second.second; return true;
	}
};
constexpr std::uint32_t sceneAddress = 0x1003, entityAddress = 0x2001, lightAddress = 0x3002;
constexpr std::uint32_t particleAddress = 0x4001, polygonAddress = 0x5003, vertexAddress = 0x6001, maskAddress = 0x7001;
static Arena Fixture() {
	Arena result;
	auto &scene = result.segments[sceneAddress]; scene.resize(152);
	Word(scene, 0, 0x12345678); Word(scene, 4, 10); Word(scene, 8, 20); Word(scene, 12, 640); Word(scene, 16, 480);
	Float(scene, 20, 90); Float(scene, 24, 75);
	for (unsigned i = 0; i < 3; ++i) { Float(scene, 28 + i * 4, float(i + 1)); Float(scene, 40 + i * 16, 1); }
	for (unsigned i = 0; i < 7; ++i) Float(scene, 76 + i * 4, float(i + 1) / 8);
	Word(scene, 104, 30); Word(scene, 108, 0xffffffffu); Word(scene, 112, 0x87654321); Word(scene, 116, maskAddress);
	const std::uint32_t addresses[] = {entityAddress, lightAddress, particleAddress, polygonAddress};
	for (unsigned i = 0; i < 4; ++i) { Word(scene, 120 + i * 8, 1); Word(scene, 124 + i * 8, addresses[i]); }
	auto &entity = result.segments[entityAddress]; entity.resize(100);
	Word(entity, 0, 10);
	for (unsigned i = 0; i < 3; ++i) {
		Float(entity, 4 + i * 16, 1); Float(entity, 40 + i * 4, float(i + 8)); Float(entity, 56 + i * 4, float(i + 12));
	}
	Word(entity, 52, 9); Word(entity, 68, 7); Float(entity, 72, .25f); Word(entity, 76, 3);
	Word(entity, 80, 20); Word(entity, 84, 30); Word(entity, 88, 0x405); Word(entity, 92, 0xffffffffu);
	Word(entity, 96, 0xfedcba98);
	auto &light = result.segments[lightAddress]; light.resize(28);
	for (unsigned i = 0; i < 7; ++i) Float(light, i * 4, float(i + 1));
	auto &particle = result.segments[particleAddress]; particle.resize(16);
	for (unsigned i = 0; i < 3; ++i) Float(particle, i * 4, float(i + 15));
	Word(particle, 12, 0x76543210);
	auto &poly = result.segments[polygonAddress]; poly.resize(16);
	Word(poly, 0, 30); Word(poly, 4, 3); Word(poly, 8, vertexAddress); Word(poly, 12, 0xdeadbeef);
	auto &vertices = result.segments[vertexAddress]; vertices.resize(72);
	for (unsigned i = 0; i < 3; ++i) {
		for (unsigned j = 0; j < 5; ++j) Float(vertices, i * 24 + j * 4, float(i * 5 + j));
		Word(vertices, i * 24 + 20, 0x12345678 + i);
	}
	auto &mask = result.segments[maskAddress]; mask.resize(32);
	for (unsigned i = 0; i < 32; ++i) mask[i] = static_cast<std::uint8_t>(i * 7);
	return result;
}
static void Unchanged(const r::Scene &scene) {
	Check(scene.view.reserved == 99 && scene.entities.size() == 1 && scene.entities[0].model == 456 &&
		scene.polygons.size() == 1 && scene.polygons[0].vertices.size() == 2 && scene.polygons[0].vertices[0].xyz[1] == 17,
		"failed capture publishes no partial scene");
}
static r::Scene Sentinel() {
	r::Scene scene; scene.view.reserved = 99; scene.entities.resize(1); scene.entities[0].model = 456;
	scene.polygons.resize(1); scene.polygons[0].vertices.resize(2); scene.polygons[0].vertices[0].xyz[1] = 17;
	return scene;
}
static void Reject(const Arena &memory, const Registry &registry, r::Result expected, const r::Limits &limits = {}) {
	auto out = Sentinel(); Check(r::Capture(sceneAddress, memory, registry, out, limits) == expected, "rejection reason"); Unchanged(out);
}
int main() {
	Registry registry; auto memory = Fixture(); r::Scene scene;
	Check(r::Capture(sceneAddress, memory, registry, scene) == r::Result::Ok, "complete capture");
	const auto readCount = memory.reads, resolveCount = registry.queries;
	Check(scene.view.reserved == 0x12345678 && scene.view.flags == 0x87654321 && scene.view.time == -1,
		"preserve reserved word, native flags and signed time");
	Check(scene.view.x == 10 && scene.view.y == 20 && scene.view.width == 640 && scene.view.height == 480 &&
		scene.view.fovX == 90 && scene.view.fovY == 75 && scene.view.origin[2] == 3 && scene.view.axis[2][2] == 1 &&
		scene.view.overlayGeometry[2] == .375f && scene.view.overlayColor[3] == .875f && scene.view.overlayShader == 303, "view fields");
	const auto &e = scene.entities[0];
	Check(e.model == 101 && e.skin == 202 && e.shader == 303 && e.axis[1][1] == 1 && e.origin[0] == 8 &&
		e.oldorigin[2] == 14 && e.frame == 9 && e.oldframe == 7 && e.backlerp == .25f && e.skinNum == 3 &&
		e.flags == 0x405 && e.nonNormalizedAxes && e.rgba[0] == 0x98 && e.rgba[3] == 0xfe, "entity and resource fields");
	Check(scene.lights[0].origin[2] == 3 && scene.lights[0].color[2] == 6 && scene.lights[0].radius == 7 &&
		scene.particles[0].origin[0] == 15 && scene.particles[0].rgba[3] == 0x76, "light and particle fields");
	Check(scene.polygons[0].shader == 303 && scene.polygons[0].vertices.size() == 3 &&
		scene.polygons[0].vertices[2].st[1] == 14 && scene.polygons[0].vertices[1].rgba[0] == 0x79, "polygon fields");
	for (unsigned i = 0; i < 32; ++i) Check(scene.view.BlockedAreas()[i] == static_cast<std::uint8_t>(~(i * 7)), "area mask inversion");
	memory.segments.clear(); Check(scene.entities[0].origin[2] == 10 && scene.polygons[0].vertices[2].xyz[0] == 10,
		"arrays owned after source lifetime ends");
	for (unsigned index = 1; index <= readCount; ++index) for (bool raises : {false, true}) {
		auto m = Fixture(); m.fail = index; m.raises = raises; auto out = Sentinel(); bool caught = false;
		try { Check(r::Capture(sceneAddress, m, registry, out) == r::Result::Memory, "failed read"); }
		catch (const Failure &) { caught = true; }
		Check(caught == raises, "read exception propagated"); Unchanged(out);
	}
	for (unsigned index = 1; index <= resolveCount; ++index) for (bool raises : {false, true}) {
		auto m = Fixture(); Registry lookup; lookup.fail = index; lookup.raises = raises; auto out = Sentinel(); bool caught = false;
		try { Check(r::Capture(sceneAddress, m, lookup, out) == r::Result::Resource, "failed resolution"); }
		catch (const Failure &) { caught = true; }
		Check(caught == raises, "resolver exception propagated"); Unchanged(out);
	}
	// Every short record/array fails before any publication, including unaligned native addresses.
	for (const auto &segment : Fixture().segments) for (std::size_t size = 0; size < segment.second.size(); ++size) {
		auto m = Fixture(); m.segments[segment.first].resize(size); Reject(m, registry, r::Result::Memory);
	}
	for (unsigned at : {120u, 128u, 136u, 144u}) {
		for (auto count : {0x7fffffffu, 0x80000000u, 0xffffffffu}) {
			auto m = Fixture(); Word(m.segments[sceneAddress], at, count); Reject(m, registry, r::Result::Limit);
		}
		for (auto pointer : {0u, 0xfffffff0u, 0x76543210u}) {
			auto m = Fixture(); Word(m.segments[sceneAddress], at + 4, pointer); Reject(m, registry, r::Result::Memory);
		}
		auto m = Fixture(); Word(m.segments[sceneAddress], at, 0); Word(m.segments[sceneAddress], at + 4, 0xffffffffu);
		Check(r::Capture(sceneAddress, m, registry, scene) == r::Result::Ok, "empty array ignores pointer");
	}
	for (auto pointer : {0u, 0xffffff80u, 0xffffffffu}) {
		auto out = Sentinel(); Check(r::Capture(pointer, Fixture(), registry, out) == r::Result::Memory, "scene pointer bound"); Unchanged(out);
	}
	// Last byte of the x86 address space is valid when the complete range fits.
	{
		auto m = Fixture(); m.segments[0xffffffe0u] = m.segments[maskAddress]; m.segments.erase(maskAddress);
		Word(m.segments[sceneAddress], 116, 0xffffffe0u);
		Check(r::Capture(sceneAddress, m, registry, scene) == r::Result::Ok, "address range ending at 2^32");
		Word(m.segments[sceneAddress], 116, 0xffffffe1u); Reject(m, registry, r::Result::Memory);
	}
	std::vector<std::pair<std::uint32_t, unsigned>> floats;
	for (unsigned at = 20; at <= 100; at += 4) floats.emplace_back(sceneAddress, at);
	for (unsigned at = 4; at <= 48; at += 4) floats.emplace_back(entityAddress, at);
	for (unsigned at : {56u, 60u, 64u, 72u}) floats.emplace_back(entityAddress, at);
	for (unsigned at = 0; at < 28; at += 4) floats.emplace_back(lightAddress, at);
	for (unsigned at = 0; at < 12; at += 4) floats.emplace_back(particleAddress, at);
	for (unsigned v = 0; v < 3; ++v) for (unsigned at = 0; at < 20; at += 4) floats.emplace_back(vertexAddress, v * 24 + at);
	for (const auto &field : floats) for (auto bits : {0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f800001u}) {
		auto m = Fixture(); Word(m.segments[field.first], field.second, bits); Reject(m, registry, r::Result::Numeric);
	}
	for (auto at : {12u, 16u}) for (auto value : {0u, 32769u, 0xffffffffu}) {
		auto m = Fixture(); Word(m.segments[sceneAddress], at, value); Reject(m, registry, r::Result::View);
	}
	for (auto at : {4u, 8u}) {
		auto m = Fixture(); Word(m.segments[sceneAddress], at, 0x7fffffffu); Reject(m, registry, r::Result::View);
	}
	for (auto at : {20u, 24u}) for (auto value : {0.0f, -1.0f, 180.0f, 181.0f}) {
		auto m = Fixture(); Float(m.segments[sceneAddress], at, value); Reject(m, registry, r::Result::View);
	}
	{
		auto m = Fixture(); Word(m.segments[sceneAddress], 116, 0);
		Check(r::Capture(sceneAddress, m, registry, scene) == r::Result::Ok, "null mask");
		for (auto value : scene.view.BlockedAreas()) Check(value == 0, "null mask admits every area");
		Float(m.segments[lightAddress], 24, -1); Reject(m, registry, r::Result::Numeric);
	}
	for (auto at : {0u, 80u, 84u}) {
		auto m = Fixture(); Word(m.segments[entityAddress], at, 999); Reject(m, registry, r::Result::Resource);
		Word(m.segments[entityAddress], at, at == 0 ? 20 : 10); Reject(m, registry, r::Result::Resource);
	}
	for (auto handle : {-1, -123}) {
		Registry lookup; lookup.entries[10].second = handle; Reject(Fixture(), lookup, r::Result::Resource);
	}
	{
		auto m = Fixture(); Word(m.segments[entityAddress], 0, 0);
		Check(r::Capture(sceneAddress, m, registry, scene) == r::Result::Ok && scene.entities[0].model == 0,
			"visible missing-model axes");
		Word(m.segments[entityAddress], 88, 0x800); Word(m.segments[entityAddress], 80, 0); Word(m.segments[entityAddress], 84, 0);
		Check(r::Capture(sceneAddress, m, registry, scene) == r::Result::Ok && scene.entities[0].model == 0, "portal without model");
		Word(m.segments[sceneAddress], 104, 0); Reject(m, registry, r::Result::Resource);
		Float(m.segments[sceneAddress], 100, 0); Check(r::Capture(sceneAddress, m, registry, scene) == r::Result::Ok, "inactive overlay");
		Word(m.segments[polygonAddress], 0, 0); Reject(m, registry, r::Result::Resource);
	}
	for (auto count : {0u, 1u, 2u}) {
		auto m = Fixture(); Word(m.segments[polygonAddress], 4, count); Reject(m, registry, r::Result::Polygon);
	}
	for (auto count : {1025u, 0x80000000u, 0xffffffffu}) {
		auto m = Fixture(); Word(m.segments[polygonAddress], 4, count); Reject(m, registry, r::Result::Limit);
	}
	for (auto pointer : {0u, 0xfffffff0u, 0x76543210u}) {
		auto m = Fixture(); Word(m.segments[polygonAddress], 8, pointer); Reject(m, registry, r::Result::Memory);
	}
	for (unsigned i = 0; i < 6; ++i) {
		r::Limits limits;
		if (i == 0) limits.entities = 0; if (i == 1) limits.lights = 0;
		if (i == 2) limits.particles = 0; if (i == 3) limits.polygons = 0;
		if (i == 4) limits.verticesPerPolygon = 2; if (i == 5) limits.totalVertices = 2;
		Reject(Fixture(), registry, r::Result::Limit, limits);
	}
	{
		auto m = Fixture(); auto poly = m.segments[polygonAddress];
		m.segments[polygonAddress].insert(m.segments[polygonAddress].end(), poly.begin(), poly.end());
		Word(m.segments[sceneAddress], 144, 2); r::Limits limits; limits.totalVertices = 5;
		Reject(m, registry, r::Result::Limit, limits); limits.totalVertices = 6;
		Check(r::Capture(sceneAddress, m, registry, scene, limits) == r::Result::Ok, "cumulative vertex bound");
		m.segments[vertexAddress][0] ^= 127;
		Check(scene.polygons[0].vertices[0].xyz[0] == 0 && scene.polygons[1].vertices[0].xyz[0] == 0, "shared source vertex arrays copied");
	}
	{
		// R_BlendBlobs 0x442700: the square at eye depth 10 under the scene's
		// projection. A 90-degree view of 640x480 maps the half-size 1 at the
		// centre to the whole width, and its height by the vertical fov.
		r::View view;
		view.x = 10; view.y = 20; view.width = 640; view.height = 480; view.fovX = 90; view.fovY = 90;
		view.overlayColor[3] = 1; view.overlayGeometry[2] = 1;
		float l = 0, t = 0, w = 0, h = 0;
		Check(view.OverlayRect(l, t, w, h) && std::fabs(l - 10) < 1e-3f && std::fabs(t - 20) < 1e-3f &&
			std::fabs(w - 640) < 1e-3f && std::fabs(h - 480) < 1e-3f, "overlay square fills a 90-degree view");
		view.overlayGeometry[0] = .5f; view.overlayGeometry[1] = .5f; view.overlayGeometry[2] = .25f;
		Check(view.OverlayRect(l, t, w, h) && std::fabs(l - (10 + 400)) < 1e-3f && std::fabs(t - (20 + 60)) < 1e-3f &&
			std::fabs(w - 160) < 1e-3f && std::fabs(h - 120) < 1e-3f, "overlay square offset up and right");
		view.overlayColor[3] = 0;
		Check(!view.OverlayRect(l, t, w, h), "no alpha, no overlay");
		view.overlayColor[3] = 1; view.overlayGeometry[2] = 0;
		Check(!view.OverlayRect(l, t, w, h), "no size, no overlay");
		view.overlayGeometry[2] = 1; view.fovX = 0;
		Check(!view.OverlayRect(l, t, w, h), "degenerate projection, no overlay");
		view.fovX = 90; view.width = 0;
		Check(!view.OverlayRect(l, t, w, h), "empty view, no overlay");
	}
	std::printf("IHV scene tests passed: %zu nonfinite fields, %u read and %u resource failure positions; bounds, lifetime, masks and atomic publication\n",
		floats.size(), readCount, resolveCount);
}
