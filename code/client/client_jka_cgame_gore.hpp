/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_GORE_HPP
#define FNTECH3_CLIENT_JKA_CGAME_GORE_HPP
#include "../ghoul2/ghoul2_geometry.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

/* Jedi Academy's skin gore: the marks hits leave on a Ghoul2 model, as retail
 * jamp.exe 1.01 keeps them for client-game imports 313 (count), 314 (add) and
 * 315 (clear). Independently authored from the retail binary; pinned OpenJK
 * named the routines (reference observation, no text incorporated).
 *
 * Two engine-wide tables: gore records (0x12a5030), each one surface's
 * projected splotch per LOD, found by tag; and gore sets (0x12a5048), each a
 * byte reference count (+4) and a surface-keyed multimap of marks (+8, size
 * +0x10). A model points at its set through a word at +0x4c of its 0xc8-byte
 * record. These are data for the module: the marks are counted, cleared and
 * expired as retail does, but Fn's renderer does not draw them yet. */
namespace fntech3::jka::cgame::gore {
namespace geometry = fntech3::ghoul2::geometry;
namespace pose = fntech3::ghoul2::pose;

/* SSkinGoreData as the client game fills it for import 314 (0x3004c580
 * zeroes 0x90 bytes; the engine reads angles +0, position +0xc, currentTime
 * +0x18, rayDirection +0x20, hitLocation +0x2c, scale +0x38, sizes +0x44/
 * +0x48, theta +0x4c, growth +0x50/+0x54, faces +0x58/+0x5c, lifeTime +0x64,
 * fadeOutTime +0x68, shader +0x84 and fadeRGB +0x8c). */
struct Request {
    float angles[3], position[3];
    std::int32_t currentTime, entNum;
    float rayDirection[3], hitLocation[3], scale[3];
    float sSize, tSize, theta;
    std::int32_t growDuration;
    float startFraction;
    std::int32_t frontFaces, backFaces, baseModelOnly, lifeTime, fadeOutTime, shrinkOutTime;
    float alphaModulate, tint[3], impactStrength;
    std::int32_t shader, myIndex, fadeRGB;
};
static_assert(sizeof(Request) == 0x90, "Academy SSkinGoreData is 0x90 bytes");

inline constexpr std::size_t MaxRecords = 500;   // 0x4e908d: cmp eax, 0x1f4
inline constexpr int TagGroup = 0x100;           // 0x4e91de and the 0xffffff00 mask
inline constexpr int MaxLods = 8;

/* One LOD of one surface's splotch: which of the surface's vertices it uses,
 * their splotch coordinates, and its triangles over those vertices. */
struct Lod {
    bool filled = false;
    std::vector<int> vertices;
    std::vector<std::array<float, 2>> texCoords;
    std::vector<std::array<int, 3>> triangles;
};
struct Record { std::array<Lod, MaxLods> lods; };
/* One mark in a set (0x4ead93..0x4eaeed), keyed by surface index. */
struct Mark {
    int shader = 0, tag = 0, deleteTime = 0, fadeTime = 0;
    bool fadeRGB = false;
    int growStart = 0, growEnd = 0;
    float growFactor = 0, growOffset = 0;
};
struct Set {
    std::uint8_t references = 1;   // NewGoreSet 0x4e9250 starts at one
    std::multimap<int, Mark> marks;
};

struct State {
    std::map<int, Record> records;
    int currentTag = 0x101, currentUpper = 0x100;   // 0x56ce60, 0x56ce64
    std::map<int, Set> sets;
    int nextSet = 1;
    // Per call: (model index, surface) to the record it filled (0x12a5040).
    std::map<std::pair<int, int>, int> pending;
    // Each model's gore-set word (+0x4c), by instance identity and place.
    std::map<std::pair<std::uint32_t, int>, int> models;

    /* 0x4e9070: past 500 records, the oldest call's records go first, all
     * those sharing its tag's upper bits, then a new tag is handed out. */
    int AllocRecord() {
        while (records.size() > MaxRecords) {
            const int group = records.begin()->first & ~(TagGroup - 1);
            records.erase(records.begin());
            while (!records.empty() && (records.begin()->first & ~(TagGroup - 1)) == group)
                records.erase(records.begin());
        }
        const int tag = currentTag++;
        records[tag] = Record{};
        return tag;
    }
    /* 0x4e91a0, at the start of every import 314. */
    void ResetPending() {
        pending.clear();
        currentTag = currentUpper;
        currentUpper += TagGroup;
    }
    Set *FindSet(int tag) {
        const auto found = sets.find(tag);
        return found == sets.end() ? nullptr : &found->second;
    }
    int NewSet() { const int tag = nextSet++; sets[tag] = Set{}; return tag; }
    /* 0x4e9310: one reference dropped, or the set and its records deleted. */
    void DeleteSet(int tag) {
        const auto found = sets.find(tag);
        if (found == sets.end()) return;
        auto &set = found->second;
        if (set.references && set.references - 1) { --set.references; return; }
        for (const auto &mark : set.marks) records.erase(mark.second.tag);
        sets.erase(found);
    }
    int Word(std::uint32_t identity, int index) const {
        const auto found = models.find({identity, index});
        return found == models.end() ? 0 : found->second;
    }
    /* Import 313's core 0x4e07a0: the set's mark count, or 0. */
    int Count(std::uint32_t identity, int index) {
        const int word = Word(identity, index);
        const Set *set = word ? FindSet(word) : nullptr;
        return set ? static_cast<int>(set->marks.size()) : 0;
    }
    /* 0x4e07f0 (import 315, and first in clean 0x4de520): every place's set. */
    void Clear(std::uint32_t identity) {
        for (auto at = models.lower_bound({identity, INT_MIN}); at != models.end() && at->first.first == identity;) {
            if (at->second) DeleteSet(at->second);
            at = models.erase(at);
        }
    }
    /* 0x4dea40 copies the record's word (+0x4c) with no new reference. */
    void CopyModel(std::uint32_t from, int fromIndex, std::uint32_t to, int toIndex) {
        const int word = Word(from, fromIndex);
        if (word) models[{to, toIndex}] = word;
        else models.erase({to, toIndex});
    }
    /* 0x4e03b0 copies every record and adds a reference to each set found. */
    void Duplicate(std::uint32_t from, std::uint32_t to) {
        for (auto at = models.lower_bound({from, INT_MIN}); at != models.end() && at->first.first == from; ++at) {
            models[{to, at->first.second}] = at->second;
            if (Set *set = FindSet(at->second)) ++set->references;
        }
    }
};

using Vector = std::array<float, 3>;
inline Vector Cross(const Vector &a, const Vector &b) {
    return {a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]};
}
inline float Dot(const Vector &a, const Vector &b) { return a[2]*b[2] + a[1]*b[1] + a[0]*b[0]; }
inline Vector Normalized(Vector v) {
    const float length = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (length) { const float inverse = 1.0f / length; for (float &c : v) c *= inverse; }
    return v;
}

/* What one call traces with, in the model's own space. */
struct Trace {
    Vector start{}, direction{};
    float sSize = 0, tSize = 0, theta = 0;
    int shader = 0, lifeTime = 0, fadeTime = 0, growDuration = 0;
    float startFraction = 0;
    bool frontFaces = true, backFaces = true, fadeRGB = false;
    std::uint32_t identity = 0;
    int model = 0, lod = 0, now = 0;
};

/* 0x4ea460, one surface: splotch coordinates for every vertex, the triangles
 * at least partly on the splotch and facing as asked, and on the first such
 * surface of a call a mark in the model's set (later LODs share its record). */
inline void Polys(State &state, const geometry::DeformedSurface &surface, int surfaceIndex, const Trace &trace) {
    Vector up{0, 0, 1};
    Vector first = Cross(trace.direction, up);
    if (Dot(first, first) < 0.1f) { up = {0, 1, 0}; first = Cross(trace.direction, up); }
    Vector second = Cross(trace.direction, first);
    first = Normalized(first); second = Normalized(second);
    const float c = static_cast<float>(std::cos(double(trace.theta)));
    const float s = static_cast<float>(std::sin(double(trace.theta)));
    const float halfC = 0.5f * c;
    Vector tAxis, sAxis;
    for (int i = 0; i < 3; ++i) {
        tAxis[i] = first[i] * (halfC / trace.tSize) + second[i] * (0.5f * s / trace.tSize);
        sAxis[i] = first[i] * (-0.5f * s / trace.sSize) + second[i] * (halfC / trace.sSize);
    }
    const auto count = surface.vertices.size();
    std::vector<unsigned> off(count);
    std::vector<std::array<float, 2>> texCoords(count);
    unsigned outside = 15;
    for (std::size_t v = 0; v < count; ++v) {
        const auto &p = surface.vertices[v].position;
        const Vector delta{p[0] - trace.start[0], p[1] - trace.start[1], p[2] - trace.start[2]};
        const float sValue = Dot(delta, sAxis) + 0.5f, tValue = Dot(delta, tAxis) + 0.5f;
        unsigned inside = 0;
        if (sValue > 0.0f) inside |= 1;
        if (sValue < 1.0f) inside |= 2;
        if (tValue > 0.0f) inside |= 4;
        if (tValue < 1.0f) inside |= 8;
        off[v] = ~inside;
        outside &= off[v];
        texCoords[v] = {sValue, tValue};
    }
    if (outside & 15) return;   // every vertex beyond one edge of the splotch
    Lod lod;
    lod.filled = true;
    std::vector<int> remap(count, -1);
    for (const auto &triangle : surface.triangles) {
        if (15 & off[triangle[0]] & off[triangle[1]] & off[triangle[2]]) continue;
        if (!trace.frontFaces || !trace.backFaces) {
            const auto &a = surface.vertices[triangle[0]].position;
            const auto &b = surface.vertices[triangle[1]].position;
            const auto &d = surface.vertices[triangle[2]].position;
            const Vector normal = Cross({b[0]-a[0], b[1]-a[1], b[2]-a[2]}, {d[0]-a[0], d[1]-a[1], d[2]-a[2]});
            if (!(Dot(trace.direction, normal) > 0.0f ? trace.frontFaces : trace.backFaces)) continue;
        }
        std::array<int, 3> mapped;
        for (int k = 0; k < 3; ++k) {
            auto &slot = remap[triangle[std::size_t(k)]];
            if (slot < 0) {
                slot = static_cast<int>(lod.vertices.size());
                lod.vertices.push_back(static_cast<int>(triangle[std::size_t(k)]));
                lod.texCoords.push_back(texCoords[triangle[std::size_t(k)]]);
            }
            mapped[std::size_t(k)] = slot;
        }
        lod.triangles.push_back(mapped);
    }
    if (lod.vertices.empty()) return;
    int tag;
    const auto key = std::make_pair(trace.model, surfaceIndex);
    const auto found = state.pending.find(key);
    if (found == state.pending.end()) {
        tag = state.AllocRecord();
        int &word = state.models[{trace.identity, trace.model}];
        if (!word || !state.FindSet(word)) word = state.NewSet();
        Mark mark;
        mark.shader = trace.shader;
        mark.deleteTime = trace.lifeTime ? trace.now + trace.lifeTime : 0;
        mark.fadeTime = trace.fadeTime;
        mark.fadeRGB = trace.fadeRGB;
        mark.tag = tag;
        mark.growStart = trace.now;
        mark.growEnd = trace.growDuration == -1 ? -1 : trace.now + trace.growDuration;
        mark.growFactor = (1.0f - trace.startFraction) / static_cast<float>(trace.growDuration);
        mark.growOffset = trace.startFraction;
        state.FindSet(word)->marks.emplace(surfaceIndex, mark);
        state.pending[key] = tag;
    } else tag = found->second;
    const auto record = state.records.find(tag);
    if (record != state.records.end() && trace.lod >= 0 && trace.lod < MaxLods)
        record->second.lods[std::size_t(trace.lod)] = std::move(lod);
}

/* 0x4ec0a0's gore branch: from the root surface down, every surface that is
 * not switched off (its override, or its file flags), and no further below a
 * surface marked NODESCENDANTS (0x100). */
template<class Flags>
inline void Walk(State &state, const geometry::DeformedGeometry &geometry, int surface,
    const Flags &flagsOf, const Trace &trace, int depth = 0) {
    if (surface < 0 || std::size_t(surface) >= geometry.surfaces.size()
        || depth > static_cast<int>(geometry.surfaces.size())) return;
    const auto &record = geometry.surfaces[std::size_t(surface)];
    const std::uint32_t flags = flagsOf(surface, record.metadata.flags);
    if (!flags) Polys(state, record, surface, trace);
    if (flags & 0x100) return;
    for (int child : record.metadata.children) Walk(state, geometry, child, flagsOf, trace, depth + 1);
}

/* The renderer's half (0x498740..0x4989ba): when a surface of a model is
 * drawn, a non-personal model's marks there whose record is gone, or whose
 * delete time has come on the Ghoul2 clock, leave the set. An expired mark's
 * record keeps its table entry with its data freed (0x4966a0): it still
 * counts toward the 500 until evicted. */
inline void Expire(State &state, std::uint32_t identity, int index, const std::vector<int> &drawn, int now) {
    const int word = state.Word(identity, index);
    Set *set = word ? state.FindSet(word) : nullptr;
    if (!set) return;
    for (int surface : drawn) {
        auto range = set->marks.equal_range(surface);
        for (auto at = range.first; at != range.second;) {
            const auto record = state.records.find(at->second.tag);
            if (record == state.records.end()) { at = set->marks.erase(at); continue; }
            if (at->second.deleteTime && now >= at->second.deleteTime) {
                record->second = Record{};
                at = set->marks.erase(at);
            } else ++at;
        }
    }
}

/* G2_GenerateWorldMatrix's inverse (0x4ec580, 0x4ec620): transposed
 * rotation, then the translation taken back through it. */
inline pose::Matrix InverseWorld(const float angles[3], const float position[3]) {
    const pose::Matrix world = pose::Rotation(angles);
    pose::Matrix inverse{};
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) inverse[std::size_t(i*4+j)] = world[std::size_t(j*4+i)];
    for (int i = 0; i < 3; ++i) {
        float value = -(inverse[std::size_t(i*4)] * position[0]);
        value -= inverse[std::size_t(i*4+1)] * position[1];
        value -= inverse[std::size_t(i*4+2)] * position[2];
        inverse[std::size_t(i*4+3)] = value;
    }
    return inverse;
}
inline Vector Through(const pose::Matrix &m, const float v[3], bool translate) {
    Vector out;
    for (int i = 0; i < 3; ++i)
        out[std::size_t(i)] = m[std::size_t(i*4)]*v[0] + m[std::size_t(i*4+1)]*v[1] + m[std::size_t(i*4+2)]*v[2]
            + (translate ? m[std::size_t(i*4+3)] : 0.0f);
    return out;
}

} // namespace fntech3::jka::cgame::gore
#endif
