// Independently authored fixtures for the Q3Test IBSP 44/45 boundary. No game
// data: shipped maps are checked only when their paths are passed as
// arguments (docs/fntech3/Q3TEST.md records that run).
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/bsp_q3test.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)

using Bytes = std::vector<unsigned char>;

static int liveBlocks = 0;
static void *Alloc(int size) { ++liveBlocks; return std::malloc(size > 0 ? size_t(size) : 1); }
static void Free(void *block) { if (block) { --liveBlocks; std::free(block); } }

static int32_t Int(const unsigned char *p) { return Q3TBSP_ReadInt(p); }
static float Float(const unsigned char *p) { return Q3TBSP_ReadFloat(p); }

struct Writer {
	Bytes file;
	int lumps;
	Writer(int version, int count) : file(size_t(8 + count * 8)), lumps(count) {
		Q3TBSP_WriteInt(file.data(), BSP_IDENT);
		Q3TBSP_WriteInt(file.data() + 4, version);
	}
	// Deliberately unaligned offsets: the translator must realign.
	void Lump(int index, const Bytes &data) {
		file.push_back(0x5a);
		Q3TBSP_WriteInt(file.data() + 8 + index * 8, int32_t(file.size()));
		Q3TBSP_WriteInt(file.data() + 12 + index * 8, int32_t(data.size()));
		file.insert(file.end(), data.begin(), data.end());
	}
};

struct Record {
	Bytes bytes;
	Record &I(int32_t v) { size_t n = bytes.size(); bytes.resize(n + 4); Q3TBSP_WriteInt(bytes.data() + n, v); return *this; }
	Record &F(float v) { size_t n = bytes.size(); bytes.resize(n + 4); Q3TBSP_WriteFloat(bytes.data() + n, v); return *this; }
	Record &S(const char *name, size_t size) {
		size_t n = bytes.size(); bytes.resize(n + size, 0);
		std::memcpy(bytes.data() + n, name, std::strlen(name) < size ? std::strlen(name) : size);
		return *this;
	}
	Record &B(const Bytes &more) { bytes.insert(bytes.end(), more.begin(), more.end()); return *this; }
};

static Record Plane(int axis, float sign, float dist, int type) {
	Record r; float n[3] = {0, 0, 0}; n[axis] = sign;
	return r.F(n[0]).F(n[1]).F(n[2]).F(dist).I(type);
}

// Six axial planes of a 64-unit cube about the origin, in brush order
// -x, +x, -y, +y, -z, +z; then a split plane for the one node.
static Bytes Planes() {
	Record r;
	for (int axis = 0; axis < 3; ++axis) { r.B(Plane(axis, -1, 32, axis).bytes); r.B(Plane(axis, 1, 32, axis).bytes); }
	r.B(Plane(0, 1, 0, 0).bytes);
	return r.bytes;
}

static Record Vertex(float x, float y, float z) {
	Record r; r.F(x).F(y).F(z).F(0).F(0).F(0).F(0).F(0).F(0).F(1);
	r.bytes.push_back(255); r.bytes.push_back(255); r.bytes.push_back(255); r.bytes.push_back(255);
	return r;
}

static Bytes Leaf(int cluster, int firstSurface, int numSurfaces, int firstBrush, int numBrushes) {
	Record r; r.I(cluster).I(0).I(-32).I(-32).I(-32).I(32).I(32).I(32);
	return r.I(firstSurface).I(numSurfaces).I(firstBrush).I(numBrushes).bytes;
}

static Bytes Visibility() { Record r; r.I(1).I(1); r.bytes.push_back(0xff); return r.bytes; }

static Bytes LightmapPage() {
	Bytes page(LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3);
	for (size_t i = 0; i < page.size(); ++i) page[i] = (unsigned char)(i * 7);
	return page;
}

// Input surfaces 0 world face, 1 submodel face, 2 world patch, 3 world soup,
// 4 world patch drawn with the one non-colliding shader. Brush 0 is the
// submodel's, 1 the world's, 2 the world's fog volume.
static Bytes Surface44(const char *shader, int fog, int firstVert, int numVerts, int firstIndex, int numIndexes,
	int width, int height, int lightmap) {
	Record r; r.S(shader, MAX_QPATH).I(fog).I(7).I(firstVert).I(numVerts).I(firstIndex).I(numIndexes);
	r.I(width).I(height).I(lightmap).I(3).I(5).I(16).I(8);
	r.F(1).F(2).F(3);
	for (int i = 0; i < 9; ++i) r.F(i == 8 ? 1.0f : 0.0f);
	return r.bytes;
}

static Bytes Fixture44() {
	Writer w(44, 15);
	w.Lump(Q3T44_ENTITIES, Bytes{'{', '}', 0});
	w.Lump(Q3T44_PLANES, Planes());
	w.Lump(Q3T44_NODES, Record().I(6).I(-1).I(-2).I(-32).I(-32).I(-32).I(32).I(32).I(32).bytes);
	Record leafs; leafs.B(Leaf(0, 0, 4, 0, 2)).B(Leaf(-1, 0, 0, 0, 0)).B(Leaf(0, 4, 1, 2, 1));
	w.Lump(Q3T44_LEAFS, leafs.bytes);
	w.Lump(Q3T44_LEAFSURFACES, Record().I(0).I(2).I(3).I(4).I(1).bytes);
	w.Lump(Q3T44_LEAFBRUSHES, Record().I(1).I(2).I(0).bytes);
	Record models;
	models.F(-32).F(-32).F(-32).F(32).F(32).F(32).F(0).F(0).F(0).I(0).I(0).I(5);
	// A stale surface range, as 1.05's q3test1 has: never read.
	models.F(-8).F(-8).F(-8).F(8).F(8).F(8).F(0).F(0).F(0).I(-3).I(6534).I(9);
	w.Lump(Q3T44_MODELS, models.bytes);
	w.Lump(Q3T44_BRUSHES, Record().I(0).I(6).I(1).I(6).I(6).I(1).I(12).I(6).I(0x40).bytes);
	Record sides;
	for (int i = 0; i < 18; ++i) sides.I(i % 6).I(i < 6 ? 0x1000 : i < 12 ? 0 : 0x80);
	w.Lump(Q3T44_BRUSHSIDES, sides.bytes);
	w.Lump(Q3T44_LIGHTMAPS, LightmapPage());
	w.Lump(Q3T44_VISIBILITY, Visibility());
	Record verts;
	for (int i = 0; i < 4; ++i) verts.B(Vertex(float(i & 1) * 16, float(i >> 1) * 16, 0).bytes);        // face 0..3
	for (int i = 0; i < 4; ++i) verts.B(Vertex(float(i & 1) * 4, float(i >> 1) * 4, 2).bytes);          // door 4..7
	for (int i = 0; i < 9; ++i) verts.B(Vertex(float(i % 3) * 8 - 4, float(i / 3) * 8, float(i)).bytes); // patch 8..16
	for (int i = 0; i < 3; ++i) verts.B(Vertex(float(i), 0, -4).bytes);                                  // soup 17..19
	for (int i = 0; i < 9; ++i) verts.B(Vertex(float(i % 3), float(i / 3), 30).bytes);                   // patch 20..28
	w.Lump(Q3T44_DRAWVERTS, verts.bytes);
	Record surfaces;
	surfaces.B(Surface44("fixture/face", 0, 0, 4, 0, 0, 0, 0, 0));
	surfaces.B(Surface44("fixture/door", -1, 4, 4, 0, 0, 0, 0, -1));
	surfaces.B(Surface44("fixture/curve", -1, 8, 9, 0, 0, 3, 3, 0));
	surfaces.B(Surface44("fixture/soup", -1, 17, 3, 0, 3, 0, 0, -1));
	surfaces.B(Surface44("textures/sfx/powerupshit", -1, 20, 9, 0, 0, 3, 3, 0));
	w.Lump(Q3T44_SURFACES, surfaces.bytes);
	w.Lump(Q3T44_FOGS, Record().S("fixture/fog", MAX_QPATH).I(2).bytes);
	w.Lump(Q3T44_DRAWINDEXES, Record().I(0).I(2).I(1).bytes);
	return w.file;
}

struct Lumps {
	const unsigned char *base; int length;
	const unsigned char *Data(int lump) const { return base + Int(base + 8 + lump * 8); }
	int Length(int lump) const { return Int(base + 12 + lump * 8); }
	int Count(int lump, int size) const { return Length(lump) / size; }
};

// Every IBSP 46 invariant the shared loaders rely on.
static void CheckV46(const Lumps &m, bool contiguousModels) {
	CHECK(Int(m.base) == BSP_IDENT && Int(m.base + 4) == BSP_VERSION);
	for (int i = 0; i < HEADER_LUMPS; ++i) {
		const int offset = Int(m.base + 8 + i * 8), length = Int(m.base + 12 + i * 8);
		CHECK(offset % 4 == 0 && offset >= int(sizeof(dheader_t)) && length >= 0 && offset + length <= m.length);
	}
	static const int sizes[HEADER_LUMPS] = {1, 72, 16, 36, 48, 4, 4, 40, 12, 8, 44, 4, 72, 104,
		LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3, 8, 1};
	for (int i = 0; i < HEADER_LUMPS; ++i) CHECK(m.Length(i) % sizes[i] == 0);
	const int shaders = m.Count(LUMP_SHADERS, 72), planes = m.Count(LUMP_PLANES, 16), nodes = m.Count(LUMP_NODES, 36);
	const int leafs = m.Count(LUMP_LEAFS, 48), leafSurfaces = m.Count(LUMP_LEAFSURFACES, 4);
	const int leafBrushes = m.Count(LUMP_LEAFBRUSHES, 4), models = m.Count(LUMP_MODELS, 40);
	const int brushes = m.Count(LUMP_BRUSHES, 12), sides = m.Count(LUMP_BRUSHSIDES, 8), verts = m.Count(LUMP_DRAWVERTS, 44);
	const int indexes = m.Count(LUMP_DRAWINDEXES, 4), fogs = m.Count(LUMP_FOGS, 72), surfaces = m.Count(LUMP_SURFACES, 104);
	for (int i = 0; i < shaders; ++i) CHECK(std::memchr(m.Data(LUMP_SHADERS) + i * 72, 0, MAX_QPATH));
	for (int i = 0; i < nodes; ++i) {
		const unsigned char *n = m.Data(LUMP_NODES) + i * 36;
		CHECK(Int(n) >= 0 && Int(n) < planes);
		for (int c = 0; c < 2; ++c) { const int child = Int(n + 4 + c * 4); CHECK(child >= 0 ? child < nodes : -1 - child < leafs); }
	}
	for (int i = 0; i < leafs; ++i) {
		const unsigned char *l = m.Data(LUMP_LEAFS) + i * 48;
		CHECK(Q3TBSP_Range(Int(l + 32), Int(l + 36), leafSurfaces) && Q3TBSP_Range(Int(l + 40), Int(l + 44), leafBrushes));
	}
	for (int i = 0; i < leafSurfaces; ++i) { const int s = Int(m.Data(LUMP_LEAFSURFACES) + i * 4); CHECK(s >= 0 && s < surfaces); }
	for (int i = 0; i < leafBrushes; ++i) { const int b = Int(m.Data(LUMP_LEAFBRUSHES) + i * 4); CHECK(b >= 0 && b < brushes); }
	int nextSurface = 0, nextBrush = 0;
	for (int i = 0; i < models; ++i) {
		const unsigned char *d = m.Data(LUMP_MODELS) + i * 40;
		CHECK(Q3TBSP_Range(Int(d + 24), Int(d + 28), surfaces) && Q3TBSP_Range(Int(d + 32), Int(d + 36), brushes));
		if (contiguousModels) {
			CHECK(Int(d + 24) == nextSurface && Int(d + 32) == nextBrush);
			nextSurface += Int(d + 28); nextBrush += Int(d + 36);
		}
	}
	if (contiguousModels) CHECK(nextSurface == surfaces && nextBrush == brushes);
	for (int i = 0; i < brushes; ++i) {
		const unsigned char *b = m.Data(LUMP_BRUSHES) + i * 12;
		CHECK(Q3TBSP_Range(Int(b), Int(b + 4), sides) && Int(b + 8) >= 0 && Int(b + 8) < shaders);
	}
	for (int i = 0; i < sides; ++i) {
		const unsigned char *s = m.Data(LUMP_BRUSHSIDES) + i * 8;
		CHECK(Int(s) >= 0 && Int(s) < planes && Int(s + 4) >= 0 && Int(s + 4) < shaders);
	}
	for (int i = 0; i < fogs; ++i) {
		const unsigned char *f = m.Data(LUMP_FOGS) + i * 72;
		CHECK(Int(f + 64) >= 0 && Int(f + 64) < brushes);
		const int numSides = Int(m.Data(LUMP_BRUSHES) + Int(f + 64) * 12 + 4);
		CHECK(Int(f + 68) >= -1 && Int(f + 68) < numSides);
	}
	for (int i = 0; i < surfaces; ++i) {
		const unsigned char *s = m.Data(LUMP_SURFACES) + i * 104;
		const int type = Int(s + 8), firstVert = Int(s + 12), numVerts = Int(s + 16);
		const int firstIndex = Int(s + 20), numIndexes = Int(s + 24);
		CHECK(Int(s) >= 0 && Int(s) < shaders && Int(s + 4) >= -1 && Int(s + 4) < fogs);
		CHECK(type == MST_PLANAR || type == MST_PATCH || type == MST_TRIANGLE_SOUP);
		CHECK(Q3TBSP_Range(firstVert, numVerts, verts));
		if (type == MST_PATCH) {
			CHECK(Int(s + 96) * Int(s + 100) == numVerts);
			for (int v = 0; v < numVerts; ++v) {
				for (int k = 0; k < 3; ++k) {
					const float x = Float(m.Data(LUMP_DRAWVERTS) + (firstVert + v) * 44 + k * 4);
					CHECK(x >= Float(s + 60 + k * 4) && x <= Float(s + 72 + k * 4));
				}
			}
		} else {
			CHECK(Q3TBSP_Range(firstIndex, numIndexes, indexes) && numIndexes % 3 == 0);
			for (int j = 0; j < numIndexes; ++j) {
				const int index = Int(m.Data(LUMP_DRAWINDEXES) + (firstIndex + j) * 4);
				CHECK(index >= 0 && index < numVerts);
			}
		}
	}
}

static int CountLive() { return liveBlocks; }

static void FixtureTests44() {
	const Bytes input = Fixture44();
	q3tBspTranslation_t out{};
	char error[128] = "";
	CHECK(Q3TestBSP_TranslateToV46(input.data(), int(input.size()), Alloc, Free, &out, error, sizeof(error)));
	CHECK(CountLive() == 1 && out.data && out.length > 0);
	const Lumps m{static_cast<const unsigned char *>(out.data), out.length};
	CheckV46(m, true);

	// Models: the world keeps its four surfaces in order, the submodel gets
	// its own from its head node's leaf, not from its stale range.
	const unsigned char *models = m.Data(LUMP_MODELS);
	CHECK(m.Count(LUMP_MODELS, 40) == 2);
	CHECK(Int(models + 24) == 0 && Int(models + 28) == 4 && Int(models + 32) == 0 && Int(models + 36) == 2);
	CHECK(Int(models + 64) == 4 && Int(models + 68) == 1 && Int(models + 72) == 2 && Int(models + 76) == 1);
	CHECK(Float(models + 40) == -8.0f && Float(models + 52) == 8.0f);
	const int leafSurfaces[] = {0, 1, 2, 3, 4}, leafBrushes[] = {0, 1, 2};
	for (int i = 0; i < 5; ++i) CHECK(Int(m.Data(LUMP_LEAFSURFACES) + i * 4) == leafSurfaces[i]);
	for (int i = 0; i < 3; ++i) CHECK(Int(m.Data(LUMP_LEAFBRUSHES) + i * 4) == leafBrushes[i]);

	// Planes lose only their type.
	CHECK(m.Length(LUMP_PLANES) == 7 * 16);
	CHECK(Float(m.Data(LUMP_PLANES) + 16) == 1.0f && Float(m.Data(LUMP_PLANES) + 28) == 32.0f);

	// Shaders: two brush contents, three side flag sets, five surface names;
	// the one non-colliding patch shader has no contents, any other patch's
	// is solid, and a face or a soup has none.
	const unsigned char *shaders = m.Data(LUMP_SHADERS);
	CHECK(m.Count(LUMP_SHADERS, 72) == 10);
	auto shader = [&](int n) { return shaders + n * 72; };
	const unsigned char *brushes = m.Data(LUMP_BRUSHES), *sides = m.Data(LUMP_BRUSHSIDES);
	CHECK(Int(brushes) == 6 && Int(brushes + 4) == 6 && Int(shader(Int(brushes + 8)) + 68) == CONTENTS_SOLID);
	CHECK(Int(brushes + 12) == 12 && Int(shader(Int(brushes + 20)) + 68) == 0x40);
	CHECK(Int(brushes + 24) == 0 && Int(shader(Int(brushes + 32)) + 68) == CONTENTS_SOLID);
	CHECK(Int(shader(Int(sides + 4)) + 64) == 0x1000 && Int(shader(Int(sides + 12 * 8 + 4)) + 64) == 0x80);
	CHECK(std::strcmp(reinterpret_cast<const char *>(shader(Int(brushes + 8))), "noshader") == 0);
	for (int i = 0; i < 18; ++i) CHECK(Int(sides + i * 8) == i % 6);

	// Surfaces, in model order: face, patch, soup, patch, door.
	const unsigned char *s = m.Data(LUMP_SURFACES);
	auto surface = [&](int n) { return s + n * 104; };
	auto name = [&](int n) { return std::string(reinterpret_cast<const char *>(shader(Int(surface(n))))); };
	CHECK(name(0) == "fixture/face" && name(1) == "fixture/curve" && name(2) == "fixture/soup");
	CHECK(name(3) == "textures/sfx/powerupshit" && name(4) == "fixture/door");
	CHECK(Int(surface(0) + 8) == MST_PLANAR && Int(surface(1) + 8) == MST_PATCH && Int(surface(2) + 8) == MST_TRIANGLE_SOUP);
	CHECK(Int(surface(3) + 8) == MST_PATCH && Int(surface(4) + 8) == MST_PLANAR);
	CHECK(Int(shader(Int(surface(1))) + 68) == CONTENTS_SOLID && Int(shader(Int(surface(3))) + 68) == 0);
	CHECK(Int(shader(Int(surface(0))) + 68) == 0 && Int(shader(Int(surface(2))) + 68) == 0);
	CHECK(Int(surface(0) + 4) == 0 && Int(surface(4) + 4) == -1);
	// Faces are fanned after the soups' own indexes, in input order.
	CHECK(Int(surface(2) + 20) == 0 && Int(surface(2) + 24) == 3);
	CHECK(Int(surface(0) + 20) == 3 && Int(surface(0) + 24) == 6);
	CHECK(Int(surface(4) + 20) == 9 && Int(surface(4) + 24) == 6);
	const int expectedIndexes[] = {0, 2, 1, 0, 1, 2, 0, 2, 3, 0, 1, 2, 0, 2, 3};
	CHECK(m.Count(LUMP_DRAWINDEXES, 4) == 15);
	for (int i = 0; i < 15; ++i) CHECK(Int(m.Data(LUMP_DRAWINDEXES) + i * 4) == expectedIndexes[i]);
	// Lightmap fields move; a patch gains its control points' bounds.
	CHECK(Int(surface(0) + 28) == 0 && Int(surface(0) + 32) == 3 && Int(surface(0) + 36) == 5);
	CHECK(Int(surface(0) + 40) == 16 && Int(surface(0) + 44) == 8 && Float(surface(0) + 48) == 1.0f);
	CHECK(Float(surface(0) + 92) == 1.0f && Int(surface(0) + 96) == 0 && Int(surface(0) + 100) == 0);
	CHECK(Int(surface(1) + 96) == 3 && Int(surface(1) + 100) == 3 && Int(surface(1) + 20) == 0 && Int(surface(1) + 24) == 0);
	CHECK(Float(surface(1) + 60) == -4.0f && Float(surface(1) + 64) == 0.0f && Float(surface(1) + 68) == 0.0f);
	CHECK(Float(surface(1) + 72) == 12.0f && Float(surface(1) + 76) == 16.0f && Float(surface(1) + 80) == 8.0f);

	// The fog follows its brush and takes that brush's top side.
	CHECK(m.Count(LUMP_FOGS, 72) == 1 && Int(m.Data(LUMP_FOGS) + 64) == 1 && Int(m.Data(LUMP_FOGS) + 68) == 5);

	// Byte-identical lumps, and no light grid.
	const Bytes page = LightmapPage();
	CHECK(m.Length(LUMP_LIGHTMAPS) == int(page.size()) && !std::memcmp(m.Data(LUMP_LIGHTMAPS), page.data(), page.size()));
	CHECK(m.Length(LUMP_VISIBILITY) == 9 && m.Data(LUMP_VISIBILITY)[8] == 0xff);
	CHECK(m.Length(LUMP_LIGHTGRID) == 0 && m.Length(LUMP_ENTITIES) == 3);
	Free(out.data);
	CHECK(CountLive() == 0);
}

// Offsets into Fixture44's lumps, for corruption.
static unsigned char *LumpAt(Bytes &file, int lump) { return file.data() + Int(file.data() + 8 + lump * 8); }

static void ExpectFailure(const Bytes &input, const char *expected) {
	q3tBspTranslation_t out{};
	char error[128] = "";
	CHECK(!Q3TestBSP_TranslateToV46(input.data(), int(input.size()), Alloc, Free, &out, error, sizeof(error)));
	if (!std::strstr(error, expected)) { std::cerr << "expected '" << expected << "', got '" << error << "'\n"; std::exit(1); }
	CHECK(!out.data && out.length == 0 && CountLive() == 0);
}

static void MalformedTests44() {
	Bytes bad = Fixture44();
	ExpectFailure(Bytes(bad.begin(), bad.begin() + 100), "truncated IBSP 44 header");
	bad = Fixture44(); Q3TBSP_WriteInt(bad.data() + 8 + Q3T44_SURFACES * 8, int32_t(bad.size()));
	ExpectFailure(bad, "out of range or not whole records");
	bad = Fixture44(); Q3TBSP_WriteInt(bad.data() + 12 + Q3T44_SURFACES * 8, 5 * 164 - 1);
	ExpectFailure(bad, "out of range or not whole records");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_LEAFSURFACES) + 4, 5);
	ExpectFailure(bad, "leaf surface is out of range");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_LEAFBRUSHES), 3);
	ExpectFailure(bad, "leaf brush is out of range");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_FOGS) + MAX_QPATH, 3);
	ExpectFailure(bad, "fog names an invalid brush");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_MODELS) + 48 + 36, 1);
	ExpectFailure(bad, "submodel tree is malformed");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_MODELS) + 48 + 36, -4);
	ExpectFailure(bad, "submodel tree is malformed");
	// A submodel whose tree cycles back on itself.
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_MODELS) + 48 + 36, 0); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_NODES) + 4, 0);
	ExpectFailure(bad, "submodel tree is malformed");
	// Two submodels claiming one surface.
	{
		Bytes twice = Fixture44();
		Bytes models(LumpAt(twice, Q3T44_MODELS), LumpAt(twice, Q3T44_MODELS) + 96);
		models.insert(models.end(), models.begin() + 48, models.begin() + 96);
		Writer w(44, 15);
		for (int i = 0; i < 15; ++i) {
			const unsigned char *d = LumpAt(twice, i);
			w.Lump(i, i == Q3T44_MODELS ? models : Bytes(d, d + Int(twice.data() + 12 + i * 8)));
		}
		ExpectFailure(w.file, "submodel tree is malformed or shared");
	}
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_SURFACES) + 2 * 164 + 88, 2);
	ExpectFailure(bad, "patch size is invalid");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_DRAWINDEXES) + 4, 3);
	ExpectFailure(bad, "triangle index is out of range");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_SURFACES) + 3 * 164 + 84, 6);
	ExpectFailure(bad, "triangle indexes are out of range");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_SURFACES) + 164 + 76, 30);
	ExpectFailure(bad, "surface is malformed");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_SURFACES) + 64, 1);
	ExpectFailure(bad, "surface is malformed");
	bad = Fixture44(); std::memset(LumpAt(bad, Q3T44_SURFACES), 'x', MAX_QPATH);
	ExpectFailure(bad, "surface is malformed");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_BRUSHES) + 4, 19);
	ExpectFailure(bad, "brush sides are out of range");
	bad = Fixture44(); Q3TBSP_WriteInt(LumpAt(bad, Q3T44_BRUSHSIDES), 7);
	ExpectFailure(bad, "plane is out of range");
	bad = Fixture44(); Q3TBSP_WriteInt(bad.data() + 12 + Q3T44_MODELS * 8, 0);
	ExpectFailure(bad, "has no world model");
	bad = Fixture44(); Q3TBSP_WriteInt(bad.data() + 4, 46);
	ExpectFailure(bad, "not a Q3Test map version");
	bad = Fixture44(); Q3TBSP_WriteInt(bad.data() + 4, 43);
	ExpectFailure(bad, "not a Q3Test map version");
	bad = Fixture44(); bad[0] = 'R';
	ExpectFailure(bad, "not an IBSP map");
	ExpectFailure(Bytes(4, 0), "missing map data");
}

static Bytes Fixture45(int fogSides) {
	Writer w(45, 17);
	w.Lump(0, Bytes{'{', '}', 0});
	Record shaders; shaders.S("fixture/solid", MAX_QPATH).I(0).I(1).S("fixture/face", MAX_QPATH).I(0x10).I(0);
	w.Lump(1, shaders.bytes);
	w.Lump(2, Planes());
	w.Lump(3, Record().I(6).I(-1).I(-2).I(-32).I(-32).I(-32).I(32).I(32).I(32).bytes);
	Record leafs; leafs.B(Leaf(0, 0, 1, 0, 1)).B(Leaf(-1, 0, 0, 0, 0));
	w.Lump(4, leafs.bytes);
	w.Lump(5, Record().I(0).bytes);
	w.Lump(6, Record().I(0).bytes);
	w.Lump(7, Record().F(-32).F(-32).F(-32).F(32).F(32).F(32).F(9).F(9).F(9).I(0).I(0).I(1).I(0).I(1).bytes);
	w.Lump(8, Record().I(0).I(fogSides).I(0).bytes);
	Record sides; for (int i = 0; i < 6; ++i) sides.I(i).I(1);
	w.Lump(9, sides.bytes);
	Record verts; for (int i = 0; i < 4; ++i) verts.B(Vertex(float(i & 1), float(i >> 1), 0).bytes);
	w.Lump(10, verts.bytes);
	w.Lump(11, Record().I(0).I(1).I(2).I(1).I(3).I(2).bytes);
	w.Lump(12, Record().S("fixture/fog", MAX_QPATH).I(0).bytes);
	Record surface; surface.I(1).I(0).I(MST_PLANAR).I(0).I(4).I(0).I(6).I(-1).I(0).I(0).I(0).I(0);
	for (int i = 0; i < 12; ++i) surface.F(float(i));
	surface.I(0).I(0);
	w.Lump(13, surface.bytes);
	w.Lump(14, Bytes());
	w.Lump(15, Bytes(8 * 3, 0x33));
	w.Lump(16, Visibility());
	return w.file;
}

static void FixtureTests45() {
	for (int fogSides : {6, 5}) {
		const Bytes input = Fixture45(fogSides);
		q3tBspTranslation_t out{};
		char error[128] = "";
		CHECK(Q3TestBSP_TranslateToV46(input.data(), int(input.size()), Alloc, Free, &out, error, sizeof(error)));
		CHECK(CountLive() == 1);
		const Lumps m{static_cast<const unsigned char *>(out.data), out.length};
		CheckV46(m, true);
		CHECK(m.Length(LUMP_PLANES) == 7 * 16 && Float(m.Data(LUMP_PLANES) + 16 + 12) == 32.0f);
		const unsigned char *model = m.Data(LUMP_MODELS);
		CHECK(m.Length(LUMP_MODELS) == 40 && Float(model) == -32.0f && Float(model + 20) == 32.0f);
		CHECK(Int(model + 24) == 0 && Int(model + 28) == 1 && Int(model + 32) == 0 && Int(model + 36) == 1);
		CHECK(m.Length(LUMP_FOGS) == 72 && Int(m.Data(LUMP_FOGS) + 64) == 0);
		CHECK(Int(m.Data(LUMP_FOGS) + 68) == (fogSides >= 6 ? 5 : -1));
		CHECK(m.Length(LUMP_SHADERS) == 144 && m.Length(LUMP_SURFACES) == 104 && m.Length(LUMP_DRAWINDEXES) == 24);
		CHECK(m.Length(LUMP_LIGHTGRID) == 24 && m.Data(LUMP_LIGHTGRID)[23] == 0x33);
		CHECK(!std::memcmp(m.Data(LUMP_SURFACES), LumpAt(const_cast<Bytes &>(input), 13), 104));
		Free(out.data);
		CHECK(CountLive() == 0);
	}
	Bytes bad = Fixture45(6);
	ExpectFailure(Bytes(bad.begin(), bad.begin() + 140), "truncated IBSP 45 header");
	bad = Fixture45(6); Q3TBSP_WriteInt(LumpAt(bad, 7) + 44, 2);
	ExpectFailure(bad, "model range is out of bounds");
	bad = Fixture45(6); Q3TBSP_WriteInt(LumpAt(bad, 7) + 48, 1);
	ExpectFailure(bad, "model range is out of bounds");
	bad = Fixture45(6); Q3TBSP_WriteInt(LumpAt(bad, 12) + 64, 1);
	ExpectFailure(bad, "fog names an invalid brush");
	bad = Fixture45(6); Q3TBSP_WriteInt(bad.data() + 12 + 2 * 8, 7 * 20 + 4);
	ExpectFailure(bad, "out of range or not whole records");
}

// A shipped map: the translation must satisfy every IBSP 46 invariant, keep
// every record, and give each submodel exactly the surfaces and brushes its
// head node reaches.
static void CheckShippedMap(const char *path) {
	std::ifstream file(path, std::ios::binary);
	CHECK(file.good());
	const Bytes input((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	const int version = Int(input.data() + 4);
	q3tBspTranslation_t out{};
	char error[128] = "";
	if (!Q3TestBSP_TranslateToV46(input.data(), int(input.size()), Alloc, Free, &out, error, sizeof(error))) {
		std::cerr << path << ": " << error << "\n"; std::exit(1);
	}
	const Lumps m{static_cast<const unsigned char *>(out.data), out.length};
	CheckV46(m, version == 44);
	const int inModels = version == 44 ? 6 : 7, modelSize = version == 44 ? 48 : 56;
	const int inSurfaces = version == 44 ? 12 : 13, surfaceSize = version == 44 ? 164 : 104;
	const int inBrushes = version == 44 ? 7 : 8, inNodes = version == 44 ? 2 : 3, inLeafs = version == 44 ? 3 : 4;
	const Lumps in{input.data(), int(input.size())};
	CHECK(m.Count(LUMP_SURFACES, 104) == in.Count(inSurfaces, surfaceSize));
	CHECK(m.Count(LUMP_BRUSHES, 12) == in.Count(inBrushes, 12));
	CHECK(m.Count(LUMP_MODELS, 40) == in.Count(inModels, modelSize));
	CHECK(m.Length(LUMP_NODES) == in.Length(inNodes) && !std::memcmp(m.Data(LUMP_NODES), in.Data(inNodes), size_t(in.Length(inNodes))));
	CHECK(m.Length(LUMP_LEAFS) == in.Length(inLeafs) && !std::memcmp(m.Data(LUMP_LEAFS), in.Data(inLeafs), size_t(in.Length(inLeafs))));
	int kinds[4] = {0, 0, 0, 0};
	for (int i = 0; i < m.Count(LUMP_SURFACES, 104); ++i) ++kinds[Int(m.Data(LUMP_SURFACES) + i * 104 + 8)];
	for (int model = 1; model < m.Count(LUMP_MODELS, 40); ++model) {
		std::set<int> surfaces, brushes;
		std::vector<int> stack{Int(in.Data(inModels) + model * modelSize + 36)};
		while (!stack.empty()) {
			const int node = stack.back(); stack.pop_back();
			if (node >= 0) {
				stack.push_back(Int(m.Data(LUMP_NODES) + node * 36 + 4));
				stack.push_back(Int(m.Data(LUMP_NODES) + node * 36 + 8));
				continue;
			}
			const unsigned char *leaf = m.Data(LUMP_LEAFS) + (-1 - node) * 48;
			for (int j = 0; j < Int(leaf + 36); ++j) surfaces.insert(Int(m.Data(LUMP_LEAFSURFACES) + (Int(leaf + 32) + j) * 4));
			for (int j = 0; j < Int(leaf + 44); ++j) brushes.insert(Int(m.Data(LUMP_LEAFBRUSHES) + (Int(leaf + 40) + j) * 4));
		}
		const unsigned char *d = m.Data(LUMP_MODELS) + model * 40;
		std::set<int> surfaceRange, brushRange;
		for (int j = 0; j < Int(d + 28); ++j) surfaceRange.insert(Int(d + 24) + j);
		for (int j = 0; j < Int(d + 36); ++j) brushRange.insert(Int(d + 32) + j);
		CHECK(surfaces == surfaceRange && brushes == brushRange);
	}
	std::printf("%s: IBSP %d -> 46, %d bytes, %d shaders, %d/%d/%d planar/patch/soup, %d models\n", path, version,
		out.length, m.Count(LUMP_SHADERS, 72), kinds[MST_PLANAR], kinds[MST_PATCH], kinds[MST_TRIANGLE_SOUP],
		m.Count(LUMP_MODELS, 40));
	Free(out.data);
	CHECK(CountLive() == 0);
}

int main(int argc, char **argv) {
	static_assert(sizeof(dshader_t) == 72 && sizeof(dplane_t) == 16 && sizeof(dmodel_t) == 40, "IBSP 46 records");
	static_assert(sizeof(dbrush_t) == 12 && sizeof(dbrushside_t) == 8 && sizeof(dfog_t) == 72, "IBSP 46 records");
	static_assert(sizeof(dsurface_t) == 104 && sizeof(drawVert_t) == 44 && sizeof(dnode_t) == 36, "IBSP 46 records");
	static_assert(sizeof(dleaf_t) == 48, "IBSP 46 records");
	FixtureTests44();
	MalformedTests44();
	FixtureTests45();
	for (int i = 1; i < argc; ++i) CheckShippedMap(argv[i]);
	std::puts("q3test bsp tests passed");
	return 0;
}
