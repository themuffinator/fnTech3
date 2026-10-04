/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_AAS_LOADER_HPP
#define FNTECH3_RTCW_AAS_LOADER_HPP
#include "../compat/rtcw_aas_format.hpp"
#include "../compat/rtcw_routecache_format.hpp"
#include <cerrno>
#include <cstdlib>

// This file is included by be_aas_file.cpp. Disk records are decoded field by
// field; none of these reads depend on the host ABI, alignment or byte order.
namespace fntech3::rtcw::navigation {
using aas::I32;
using aas::F32;
inline void Vector(float *out, const unsigned char *in) {
	for (unsigned i = 0; i < 3; ++i) out[i] = F32(in + i * 4);
}
inline void Decode(aas_bbox_t &out, const unsigned char *in) {
	out.presencetype = I32(in); out.flags = I32(in + 4);
	Vector(out.mins, in + 8); Vector(out.maxs, in + 20);
}
inline void Decode(aas_vertex_t &out, const unsigned char *in) { Vector(out, in); }
inline void Decode(aas_plane_t &out, const unsigned char *in) {
	Vector(out.normal, in); out.dist = F32(in + 12); out.type = I32(in + 16);
}
inline void Decode(aas_edge_t &out, const unsigned char *in) {
	out.v[0] = I32(in); out.v[1] = I32(in + 4);
}
inline void Decode(int &out, const unsigned char *in) { out = I32(in); }
inline void Decode(aas_face_t &out, const unsigned char *in) {
	out.planenum = I32(in); out.faceflags = I32(in + 4);
	out.numedges = I32(in + 8); out.firstedge = I32(in + 12);
	out.frontarea = I32(in + 16); out.backarea = I32(in + 20);
}
inline void Decode(aas_area_t &out, const unsigned char *in) {
	out.areanum = I32(in); out.numfaces = I32(in + 4); out.firstface = I32(in + 8);
	Vector(out.mins, in + 12); Vector(out.maxs, in + 24); Vector(out.center, in + 36);
}
inline void Decode(aas_areasettings_t &out, const unsigned char *in) {
	out.contents = I32(in); out.areaflags = I32(in + 4); out.presencetype = I32(in + 8);
	out.cluster = I32(in + 12); out.clusterareanum = I32(in + 16);
	out.numreachableareas = I32(in + 20); out.firstreachablearea = I32(in + 24);
}
inline void Decode(aas_reachability_t &out, const unsigned char *in) {
	out.areanum = I32(in); out.facenum = I32(in + 4); out.edgenum = I32(in + 8);
	Vector(out.start, in + 12); Vector(out.end, in + 24); out.traveltype = I32(in + 36);
	out.traveltime = static_cast<unsigned short>(in[40] | (unsigned(in[41]) << 8));
}
inline void Decode(aas_node_t &out, const unsigned char *in) {
	out.planenum = I32(in); out.children[0] = I32(in + 4); out.children[1] = I32(in + 8);
}
inline void Decode(aas_portal_t &out, const unsigned char *in) {
	out.areanum = I32(in); out.frontcluster = I32(in + 4); out.backcluster = I32(in + 8);
	out.clusterareanum[0] = I32(in + 12); out.clusterareanum[1] = I32(in + 16);
}
inline void Decode(aas_cluster_t &out, const unsigned char *in) {
	out.numareas = I32(in); out.numreachabilityareas = I32(in + 4);
	out.numportals = I32(in + 8); out.firstportal = I32(in + 12);
}
template<class T>
void Lump(const aas::View &view, aas::Lump kind, int &count, T *&out) {
	count = static_cast<int>(view.Count(kind));
	if (!count) return;
	// Heap allocations can actually be reclaimed on a failed load or reload.
	// Publish ownership before the next allocation can raise an engine error.
	out = botlib::ClearedAlloc<T>(static_cast<std::size_t>(count));
	for (int i = 0; i < count; ++i) Decode(out[i], view.Record(kind, i));
}
inline bool Checksum(const char *text, std::uint32_t &out) {
	if (!text || !*text || (*text != '-' && (*text < '0' || *text > '9'))) return false;
	char *end;
	errno = 0;
	const auto value = std::strtoll(text, &end, 10);
	if (errno == ERANGE || *end || value < INT32_MIN || value > UINT32_MAX) return false;
	out = static_cast<std::uint32_t>(value);
	return true;
}
} // namespace fntech3::rtcw::navigation

static int AAS_LoadRTCWAASFile(const char *filename, bool wolfet = false) {
	using namespace fntech3::rtcw;
	const char *profileName = wolfet ? "ET" : "RTCW";
	AAS_DumpAASData();
	botimport.Print(PRT_MESSAGE, "trying to load %s\n", filename);
	const int bytes = botimport.FS_FOpenFile(filename, &aasworld.rtcwLoadFile, FS_READ);
	if (!aasworld.rtcwLoadFile) return BLERR_CANNOTOPENAASFILE;
	// Bound both input storage and every converted lump before allocating.
	// No compatible record expands beyond its disk stride (plus the sidecar).
	constexpr int MaxFileBytes = 256 * 1024 * 1024;
	if (bytes < static_cast<int>(aas::HeaderBytes) || bytes > MaxFileBytes) {
		AAS_DumpAASData();
		botimport.Print(PRT_ERROR, "Invalid %s AAS file size: %s\n", profileName, filename);
		return BLERR_CANNOTREADAASLUMP;
	}
	aasworld.rtcwLoadBytes = botlib::Alloc<byte>(bytes);
	const int read = botimport.FS_Read(aasworld.rtcwLoadBytes, bytes, aasworld.rtcwLoadFile);
	botimport.FS_FCloseFile(aasworld.rtcwLoadFile);
	aasworld.rtcwLoadFile = 0;
	std::uint32_t checksum = 0;
	aas::View view;
	const auto error = read != bytes ? aas::Error::Extent
		: !navigation::Checksum(LibVarGetString("sv_mapChecksum"), checksum) ? aas::Error::Checksum
		: aas::Parse(aasworld.rtcwLoadBytes, bytes, checksum, view, wolfet);
	if (error != aas::Error::None) {
		AAS_DumpAASData();
		botimport.Print(PRT_ERROR, "Invalid %s AAS %s: %s\n", profileName, filename, aas::ErrorName(error));
		return error == aas::Error::Identity ? BLERR_WRONGAASFILEID
			: error == aas::Error::Version || error == aas::Error::Checksum ? BLERR_WRONGAASFILEVERSION
			: BLERR_CANNOTREADAASLUMP;
	}
	navigation::Lump(view, aas::Boxes, aasworld.numbboxes, aasworld.bboxes);
	navigation::Lump(view, aas::Vertices, aasworld.numvertexes, aasworld.vertexes);
	navigation::Lump(view, aas::Planes, aasworld.numplanes, aasworld.planes);
	navigation::Lump(view, aas::Edges, aasworld.numedges, aasworld.edges);
	navigation::Lump(view, aas::EdgeIndices, aasworld.edgeindexsize, aasworld.edgeindex);
	navigation::Lump(view, aas::Faces, aasworld.numfaces, aasworld.faces);
	navigation::Lump(view, aas::FaceIndices, aasworld.faceindexsize, aasworld.faceindex);
	navigation::Lump(view, aas::Areas, aasworld.numareas, aasworld.areas);
	navigation::Lump(view, aas::Settings, aasworld.numareasettings, aasworld.areasettings);
	aasworld.rtcwGroundSteepness = botlib::Alloc<float>(aasworld.numareasettings);
	for (int i = 0; i < aasworld.numareasettings; ++i)
		aasworld.rtcwGroundSteepness[i] = aas::F32(view.Record(aas::Settings, i) + 28);
	navigation::Lump(view, aas::Reachability, aasworld.reachabilitysize, aasworld.reachability);
	navigation::Lump(view, aas::Nodes, aasworld.numnodes, aasworld.nodes);
	navigation::Lump(view, aas::Portals, aasworld.numportals, aasworld.portals);
	navigation::Lump(view, aas::PortalIndices, aasworld.portalindexsize, aasworld.portalindex);
	navigation::Lump(view, aas::Clusters, aasworld.numclusters, aasworld.clusters);
	const auto signature = routecache::MapSignature(view);
	for (unsigned i = 0; i < 3; ++i) aasworld.rtcwNavigationCrc[i] = signature.crc[i];
	std::memcpy(&aasworld.bspchecksum, &checksum, sizeof(checksum));
	FreeMemory(aasworld.rtcwLoadBytes);
	aasworld.rtcwLoadBytes = nullptr;
	// The reader checked each record; these are the numbers the shared
	// botlib follows from one record to another, dummies included.
	char fault[128];
	if (!AAS_IndexesInside(false, fault, sizeof(fault))) {
		AAS_DumpAASData();
		botimport.Print(PRT_ERROR, "Invalid %s AAS %s: %s\n", profileName, filename, fault);
		return BLERR_CANNOTREADAASLUMP;
	}
	aasworld.loaded = qtrue;
	return BLERR_NOERROR;
}
#endif
