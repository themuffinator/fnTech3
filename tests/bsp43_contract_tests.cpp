// Independently authored fixtures for the IHV BSP boundary. No game data.
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/bsp_v43.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <iterator>
#include <vector>

void Q_strncpyz(char *out, const char *in, int size) {
	if (size <= 0) return;
	std::strncpy(out, in, size - 1); out[size - 1] = 0;
}

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
using Bytes = std::vector<byte>;
static_assert(sizeof(dheader43_t) == 120 && sizeof(dplane43_t) == 20, "IHV directory/planes");
static_assert(sizeof(dmodel43_t) == 48 && sizeof(dsurface43_t) == 156, "IHV models/surfaces");
static_assert(sizeof(dbrushside43_t) == 8 && sizeof(dfog43_t) == 68, "IHV sides/fogs");

template<class T> void Lump(Bytes &file, int index, const std::vector<T> &records) {
	// Deliberately unaligned disk offsets: the translator must normalize them.
	file.push_back(0);
	lump_t lump{LittleLong(int(file.size())), LittleLong(int(records.size() * sizeof(T)))};
	std::memcpy(file.data() + 8 + index * sizeof(lump), &lump, sizeof(lump));
	const auto *bytes = reinterpret_cast<const byte *>(records.data());
	if (!records.empty()) file.insert(file.end(), bytes, bytes + records.size() * sizeof(T));
}

template<class T, class F> void Edit(Bytes &file, int lump, int record, F update) {
	lump_t directory;
	std::memcpy(&directory, file.data() + 8 + lump * sizeof(directory), sizeof(directory));
	byte *ptr = file.data() + LittleLong(directory.fileofs) + record * sizeof(T);
	T value; std::memcpy(&value, ptr, sizeof(value)); update(value); std::memcpy(ptr, &value, sizeof(value));
}

Bytes Fixture() {
	dheader43_t header{}; header.ident = LittleLong(BSP_IDENT); header.version = LittleLong(43);
	Bytes file(sizeof(header)); std::memcpy(file.data(), &header, sizeof(header));
	Lump<char>(file, LUMP43_ENTITIES, {'{','}', '\n', 0});
	std::vector<dplane43_t> planes(6);
	for (int i = 0; i < 6; ++i) {
		planes[i].normal[i / 2] = LittleFloat(i % 2 ? 1.0f : -1.0f);
		planes[i].dist = LittleFloat(32.0f); planes[i].type = LittleLong(i / 2);
	}
	Lump(file, LUMP43_PLANES, planes);
	dnode_t node{}; node.children[0] = LittleLong(-1); node.children[1] = LittleLong(-2);
	Lump<dnode_t>(file, LUMP43_NODES, {node});
	dleaf_t solid{}, leaf{}; solid.cluster = solid.area = LittleLong(-1);
	leaf.numLeafBrushes = LittleLong(2); leaf.numLeafSurfaces = LittleLong(2);
	Lump<dleaf_t>(file, LUMP43_LEAFS, {solid, leaf});
	Lump<int>(file, LUMP43_LEAFSURFACES, {LittleLong(0), LittleLong(1)});
	Lump<int>(file, LUMP43_LEAFBRUSHES, {LittleLong(2), LittleLong(0)});
	dmodel43_t world{}, model{};
	world.numFaces = LittleLong(1); model.firstFace = LittleLong(1); model.numFaces = LittleLong(1);
	model.headnode = LittleLong(-2);
	for (int i = 0; i < 3; ++i) { world.mins[i] = model.mins[i] = LittleFloat(-32); world.maxs[i] = model.maxs[i] = LittleFloat(32); }
	Lump<dmodel43_t>(file, LUMP43_MODELS, {world, model});
	std::vector<dbrush43_t> brushes(3);
	for (auto &brush : brushes) { brush.numSides = LittleLong(6); brush.contentFlags = LittleLong(CONTENTS_SOLID); }
	Lump(file, LUMP43_BRUSHES, brushes);
	std::vector<dbrushside43_t> sides(6);
	for (int i = 0; i < 6; ++i) { sides[i].planeNum = LittleLong(i); sides[i].surfaceFlags = LittleLong(SURF_NODRAW); }
	Lump(file, LUMP43_BRUSHSIDES, sides);
	Lump<byte>(file, LUMP43_LIGHTMAPS, {});
	Lump<int>(file, LUMP43_VISIBILITY, {LittleLong(1), LittleLong(4), LittleLong(1)});
	std::vector<drawVert_t> vertices(13);
	for (int i = 0; i < 13; ++i) {
		vertices[i].xyz[0] = LittleFloat(float(i + 1)); vertices[i].xyz[1] = LittleFloat(float(i % 3));
		vertices[i].normal[2] = LittleFloat(1); std::memset(&vertices[i].color, 255, 4);
	}
	Lump(file, LUMP43_DRAWVERTS, vertices);
	dsurface43_t face{}, patch{};
	Q_strncpyz(face.shader, "fixture/face", sizeof(face.shader));
	Q_strncpyz(patch.shader, "fixture/patch", sizeof(patch.shader));
	face.fogNum = LittleLong(0); patch.fogNum = LittleLong(-1);
	face.lightmapNum = patch.lightmapNum = LittleLong(-1);
	face.brushSideNum = LittleLong(1); patch.brushSideNum = LittleLong(2);
	face.numVerts = LittleLong(4); patch.firstVert = LittleLong(4); patch.numVerts = LittleLong(9);
	patch.patchWidth = patch.patchHeight = LittleLong(3);
	face.lightmapVecs[2][2] = LittleFloat(1);
	Lump<dsurface43_t>(file, LUMP43_SURFACES, {face, patch});
	dfog43_t fog{}; Q_strncpyz(fog.shader, "fixture/fog", sizeof(fog.shader));
	Lump<dfog43_t>(file, LUMP43_FOGS, {fog});
	return file;
}

static int allocations;
static bool failAllocation;
void *Allocate(int bytes) { ++allocations; return failAllocation ? nullptr : std::malloc(bytes); }
void Free(void *ptr) { std::free(ptr); }

Bytes Translate(const Bytes &file) {
	bsp43_translation_t translated{}; char error[128];
	const auto ok = BSP43_TranslateToV46(file.data(), int(file.size()), Allocate, Free, &translated, error, sizeof(error));
	if (!ok) std::cerr << error << '\n';
	CHECK(ok);
	CHECK(translated.data && translated.length && !error[0]);
	const auto *ptr = static_cast<const byte *>(translated.data);
	Bytes result(ptr, ptr + translated.length); Free(translated.data); return result;
}

template<class T> const T *Records(const Bytes &file, int index) {
	return reinterpret_cast<const T *>(file.data() + LittleLong(reinterpret_cast<const dheader_t *>(file.data())->lumps[index].fileofs));
}

void Reject(const Bytes &file) {
	bsp43_translation_t translated{reinterpret_cast<void *>(1), 99}; char error[128]{};
	int before = allocations;
	CHECK(!BSP43_TranslateToV46(file.data(), int(file.size()), Allocate, Free, &translated, error, sizeof(error)));
	CHECK(!translated.data && !translated.length && error[0]);
	CHECK(allocations == before); // Reject malformed data before allocating published output.
}

void Tests() {
	auto input = Fixture(); const auto original = input;
	auto output = Translate(input); CHECK(input == original);
	CHECK(LittleLong(reinterpret_cast<const dheader_t *>(output.data())->version) == 46);
	const auto *surfaces = Records<dsurface_t>(output, LUMP_SURFACES);
	CHECK(LittleLong(surfaces[0].surfaceType) == MST_PLANAR && LittleLong(surfaces[0].numIndexes) == 6);
	CHECK(LittleLong(surfaces[1].surfaceType) == MST_PATCH && LittleLong(surfaces[1].numIndexes) == 0);
	const auto *indices = Records<int>(output, LUMP_DRAWINDEXES);
	const int expected[] = {0, 1, 2, 0, 2, 3};
	for (int i = 0; i < 6; ++i) CHECK(LittleLong(indices[i]) == expected[i]);
	CHECK(LittleFloat(surfaces[1].lightmapVecs[0][0]) == 0 && LittleFloat(surfaces[1].lightmapVecs[1][0]) == 13);
	const auto *models = Records<dmodel_t>(output, LUMP_MODELS);
	CHECK(LittleLong(models[0].numSurfaces) == 1);
	CHECK(LittleLong(models[1].firstSurface) == 1 && LittleLong(models[1].numSurfaces) == 1);
	CHECK(LittleLong(models[1].firstBrush) == 3 && LittleLong(models[1].numBrushes) == 2);
	const auto *brushes = Records<dbrush_t>(output, LUMP_BRUSHES);
	CHECK(LittleLong(brushes[3].shaderNum) == 0 && LittleLong(brushes[4].shaderNum) == 2);
	const auto *shaders = Records<dshader_t>(output, LUMP_SHADERS);
	CHECK(!(LittleLong(shaders[LittleLong(surfaces[1].shaderNum)].surfaceFlags) & SURF_NODRAW));
	CHECK(LittleLong(shaders[LittleLong(surfaces[1].shaderNum)].contentFlags) == CONTENTS_SOLID);
	CHECK(LittleLong(Records<dfog_t>(output, LUMP_FOGS)->visibleSide) == 5);
	// The IHV surface's offset-68 identifier is not a collision-side index.
	// Valid shipped maps contain values outside that table. It must neither
	// reject the map nor borrow flags from an unrelated in-range side.
	for (int identifier : {-1, INT_MIN, 0, 5, 6, INT_MAX}) {
		input = original;
		Edit<dsurface43_t>(input, LUMP43_SURFACES, 1, [identifier](auto &s) { s.brushSideNum = LittleLong(identifier); });
		CHECK(Translate(input) == output);
	}
	input = original;

	// The same geometry must survive an inline model rooted at an internal node.
	Edit<dmodel43_t>(input, LUMP43_MODELS, 1, [](auto &m) { m.headnode = 0; });
	CHECK(Translate(input) == output);
	// Duplicate leaf references do not duplicate a model's brush membership.
	input = original; Edit<int>(input, LUMP43_LEAFBRUSHES, 1, [](auto &v) { v = LittleLong(2); });
	auto duplicate = Translate(input); models = Records<dmodel_t>(duplicate, LUMP_MODELS);
	CHECK(LittleLong(models[1].firstBrush) == 2 && LittleLong(models[1].numBrushes) == 1);

	// Every byte prefix is incomplete. Includes prefixes of all record kinds.
	for (size_t n = 0; n < original.size(); ++n) Reject(Bytes(original.begin(), original.begin() + n));
	input = original; reinterpret_cast<dheader43_t *>(input.data())->lumps[0].fileofs = LittleLong(INT_MAX); Reject(input);
	input = original; reinterpret_cast<dheader43_t *>(input.data())->lumps[0].filelen = LittleLong(INT_MAX); Reject(input);
	input = original; reinterpret_cast<dheader43_t *>(input.data())->lumps[0].fileofs = 0; Reject(input);
	input = original; Edit<dnode_t>(input, LUMP43_NODES, 0, [](auto &n) { n.children[0] = 0; }); Reject(input);
	input = original; Edit<dnode_t>(input, LUMP43_NODES, 0, [](auto &n) { n.children[0] = LittleLong(INT_MIN); }); Reject(input);
	for (bool reverse : {false, true}) {
		std::vector<dnode_t> chain(2049);
		for (int i = 0; i < int(chain.size()); ++i) {
			const int child = reverse ? i - 1 : i + 1;
			chain[i].children[0] = LittleLong(child < 0 || child == int(chain.size()) ? -1 : child);
			chain[i].children[1] = LittleLong(-2);
		}
		input = original; Lump(input, LUMP43_NODES, chain); Reject(input);
	}
	input = original; Edit<dleaf_t>(input, LUMP43_LEAFS, 1, [](auto &l) { l.numLeafBrushes = LittleLong(INT_MAX); }); Reject(input);
	input = original; Edit<dleaf_t>(input, LUMP43_LEAFS, 1, [](auto &l) { l.cluster = LittleLong(1); }); Reject(input);
	input = original; Edit<int>(input, LUMP43_VISIBILITY, 1, [](auto &n) { n = 0; }); Reject(input);
	input = original; Edit<dbrush43_t>(input, LUMP43_BRUSHES, 0, [](auto &b) { b.numSides = LittleLong(5); }); Reject(input);
	input = original; Edit<dsurface43_t>(input, LUMP43_SURFACES, 0, [](auto &s) { std::memset(s.shader, 'a', sizeof(s.shader)); }); Reject(input);
	input = original; Edit<dsurface43_t>(input, LUMP43_SURFACES, 0, [](auto &s) { s.firstVert = LittleLong(INT_MAX); }); Reject(input);
	for (int width : {-1, 0, 2, 4, 33, INT_MAX}) {
		input = original; Edit<dsurface43_t>(input, LUMP43_SURFACES, 1, [width](auto &s) { s.patchWidth = LittleLong(width); }); Reject(input);
	}
	input = original; Edit<dfog43_t>(input, LUMP43_FOGS, 0, [](auto &f) { f.brushNum = LittleLong(INT_MAX); }); Reject(input);
	for (uint32_t bits : {0x7f800000u, 0xff800000u, 0x7fc00000u}) {
		input = original; Edit<drawVert_t>(input, LUMP43_DRAWVERTS, 0, [bits](auto &v) { uint32_t le = LittleLong(bits); std::memcpy(v.xyz, &le, 4); }); Reject(input);
	}
	// Also support an unaligned caller-owned buffer, and recover after failure.
	input = original; input.insert(input.begin(), 0);
	bsp43_translation_t translated{}; char error[128];
	CHECK(BSP43_TranslateToV46(input.data() + 1, int(input.size() - 1), Allocate, Free, &translated, error, sizeof(error)));
	CHECK(translated.length == int(output.size()) && !std::memcmp(translated.data, output.data(), output.size())); Free(translated.data);
	failAllocation = true;
	CHECK(!BSP43_TranslateToV46(original.data(), int(original.size()), Allocate, Free, &translated, error, sizeof(error)));
	CHECK(!translated.data && !translated.length && error[0]); failAllocation = false;
	CHECK(Translate(original) == output);
	CHECK(!BSP43_TranslateToV46(nullptr, INT_MAX, Allocate, Free, &translated, error, sizeof(error)));
}

int main(int argc, char **argv) {
	Tests();
	for (int i = 1; i < argc; ++i) {
		std::ifstream stream(argv[i], std::ios::binary); CHECK(stream.good());
		Bytes input((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		auto output = Translate(input);
		std::cout << argv[i] << ": " << input.size() << " -> " << output.size() << " bytes\n";
	}
	std::cout << "IHV BSP contract tests passed\n";
}
