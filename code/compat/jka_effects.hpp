/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_EFFECTS_HPP
#define FNTECH3_JKA_EFFECTS_HPP
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* Independent, engine-owned EFX declaration reader. It preserves ordered pairs,
 * lists and curve groups; it does not implement Raven's presentation pipeline.
 * Retail MP 0x4064d0/0x4069d0 supplies path/cache/primitive facts. The pinned
 * OpenJK format reference supplies the line-oriented group/list grammar. */
namespace fntech3::jka::effects {
constexpr std::size_t MaxTextBytes = 65534;
constexpr std::size_t NameBytes = 64;
constexpr std::size_t MaxEffects = 256;
constexpr std::size_t MaxPrimitives = 24;
constexpr std::size_t MaxTokenBytes = 1023;
constexpr std::size_t MaxNodes = 4096;
constexpr std::size_t MaxDepth = 8;
constexpr std::size_t MaxListValues = 128;
struct Failure { std::size_t line{1}; const char *reason{"invalid effect declaration"}; };
inline std::string Lower(std::string_view value) {
    std::string result(value);
    for (char &c : result) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return result;
}
inline bool SafePath(std::string_view path) {
    if (path.empty() || path.size() >= NameBytes || path.front() == '/') return false;
    std::size_t begin = 0;
    for (std::size_t n = 0; n <= path.size(); ++n) {
        if (n != path.size()) {
            const auto c = static_cast<unsigned char>(path[n]);
            if (c <= 32 || c >= 127 || c == ':' || c == '\\' || c == '"' || c == '[' || c == ']' || c == '{' || c == '}') return false;
            if (c != '/') continue;
        }
        const auto part = path.substr(begin, n - begin);
        if (part.empty() || part == "." || part == "..") return false;
        begin = n + 1;
    }
    return true;
}
struct Name { std::string key, file; };
inline bool ResolveName(std::string_view input, Name &output) {
    if (!SafePath(input)) return false;
    Name candidate;
    candidate.key = Lower(input.substr(0, input.find('.')));
    if (candidate.key.empty()) return false;
    candidate.file = std::string(input);
    if (input.find('.') == std::string_view::npos) candidate.file += ".efx";
    // The retail loader tests the literal first seven characters, then prefixes.
    if (input.substr(0, 7) != "effects") candidate.file.insert(0, "effects/");
    if (!SafePath(candidate.file)) return false;
    output = std::move(candidate);
    return true;
}
struct Pair {
    std::string name;
    std::vector<std::string> values;
    bool list{};
    std::size_t line{};
};
struct Group {
    std::string name;
    std::vector<Pair> pairs;
    std::vector<Group> groups;
    std::size_t line{};
};
enum class Kind { Particle, Line, Tail, Sound, Cylinder, Electricity, Emitter,
    Decal, OrientedParticle, Runner, Light, CameraShake, Flash };
enum class MediaKind { Shader, Model, Sound, Impact, Death, Emit, Play };
struct Media { MediaKind kind{}; std::string name; int handle{}; };
struct Numeric { std::string name; std::vector<double> values; };
struct Primitive {
    Kind kind{};
    Group declaration;
    std::vector<Media> media;
    std::vector<Numeric> numbers;
};
struct Definition {
    int repeatDelay{300};
    std::vector<Primitive> primitives;
    std::vector<std::string> ignoredGroups;
};

class Reader {
    std::string_view text;
    std::size_t at{}, line{1}, nodes{};
    Failure &failure;
    bool bad{};
    bool Fail(const char *reason) {
        if (!bad) failure = {line, reason};
        bad = true;
        return false;
    }
    bool Skip() {
        for (;;) {
            while (at < text.size() && static_cast<unsigned char>(text[at]) <= 32) {
                if (text[at++] == '\n') ++line;
            }
            if (at + 1 >= text.size() || text[at] != '/') return !bad;
            if (text[at + 1] == '/') {
                while (at < text.size() && text[at] != '\n') ++at;
            } else if (text[at + 1] == '*') {
                at += 2;
                while (at + 1 < text.size() && text.substr(at, 2) != "*/")
                    if (text[at++] == '\n') ++line;
                if (at + 1 >= text.size()) return Fail("unterminated effect comment");
                at += 2;
            } else return true;
        }
    }
    bool Quoted(std::string &value) {
        ++at;
        const auto begin = at;
        while (at < text.size() && text[at] != '"') {
            if (text[at] == '\n' || text[at] == '\r') return Fail("multiline effect string");
            ++at;
        }
        if (at == text.size()) return Fail("unterminated effect string");
        value = std::string(text.substr(begin, at - begin));
        ++at;
        return value.size() <= MaxTokenBytes || Fail("effect token exceeds the engine bound");
    }
    bool Word(std::string &value) {
        if (!Skip() || at == text.size()) return false;
        if (text[at] == '"') return Quoted(value);
        const auto begin = at;
        while (at < text.size() && static_cast<unsigned char>(text[at]) > 32) ++at;
        value = std::string(text.substr(begin, at - begin));
        return value.size() <= MaxTokenBytes || Fail("effect token exceeds the engine bound");
    }
    bool Value(std::string &value) {
        if (!Skip() || at == text.size()) return Fail("missing effect value");
        if (text[at] == '"') {
            if (!Quoted(value)) return false;
            while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) ++at;
            if (at < text.size() && text[at] != '\n' && text[at] != '\r' &&
                text.substr(at, 2) != "//" && text.substr(at, 2) != "/*")
                return Fail("trailing text after quoted effect value");
            return true;
        }
        const auto begin = at;
        while (at < text.size() && text[at] != '\n' && text[at] != '\r' &&
            text.substr(at, 2) != "//" && text.substr(at, 2) != "/*") ++at;
        auto end = at;
        while (end > begin && static_cast<unsigned char>(text[end-1]) <= 32) --end;
        value = std::string(text.substr(begin, end - begin));
        return value.size() <= MaxTokenBytes || Fail("effect value exceeds the engine bound");
    }
    bool Tree(Group &group, std::size_t depth) {
        for (;;) {
            std::string key;
            if (!Word(key)) return bad ? false : depth == 0 || Fail("unclosed effect group");
            const auto keyLine = line;
            if (key == "}") return depth != 0 || Fail("unexpected effect group close");
            if (key.empty() || key == "{" || key == "[" || key == "]") return Fail("invalid effect key");
            if (++nodes > MaxNodes) return Fail("too many effect declarations");
            std::string value;
            if (!Value(value)) return false;
            if (value == "{") {
                if (depth >= MaxDepth) return Fail("effect nesting exceeds the engine bound");
                Group child; child.name = std::move(key); child.line = keyLine;
                if (!Tree(child, depth + 1)) return false;
                group.groups.push_back(std::move(child));
            } else {
                Pair pair; pair.name = std::move(key); pair.line = keyLine;
                pair.list = value == "[";
                if (pair.list) {
                    for (;;) {
                        if (!Value(value)) return false;
                        if (value == "]") break;
                        if (value.empty() || value == "[" || value == "{" || value == "}") return Fail("invalid effect list value");
                        if (pair.values.size() >= MaxListValues) return Fail("effect list exceeds the engine bound");
                        pair.values.push_back(std::move(value));
                    }
                } else {
                    if (value.empty() || value == "}" || value == "]") return Fail("missing effect pair value");
                    pair.values.push_back(std::move(value));
                }
                group.pairs.push_back(std::move(pair));
            }
        }
    }
public:
    Reader(std::string_view input, Failure &error):text(input),failure(error){}
    bool Read(Group &output) {
        if (text.empty() || text.size() > MaxTextBytes) return Fail("effect file outside the engine bound");
        for (unsigned char c : text) if (!c || (c < 32 && c != '\t' && c != '\n' && c != '\r')) return Fail("invalid effect text byte");
        Group candidate;
        if (!Tree(candidate, 0)) return false;
        output = std::move(candidate);
        return true;
    }
};

inline bool Numbers(const Pair &pair, std::size_t a, std::size_t b, Numeric &output, Failure &failure) {
    output = {}; output.name = Lower(pair.name);
    if (pair.values.empty()) { failure={pair.line,"missing numeric effect value"}; return false; }
    std::istringstream input(pair.values.front()); input.imbue(std::locale::classic());
    double value;
    while (input >> value) {
        std::uint64_t bits;
        static_assert(sizeof(bits)==sizeof(value),"EFX numeric reader requires the engine's IEEE double contract");
        std::memcpy(&bits,&value,sizeof(bits));
        // Engine builds may use fast-math; do not let it erase a finiteness test.
        if ((bits&UINT64_C(0x7ff0000000000000))==UINT64_C(0x7ff0000000000000) ||
            std::abs(value) > (std::numeric_limits<float>::max)() || output.values.size() == 6) {
            failure={pair.line,"invalid numeric effect value"}; return false;
        }
        output.values.push_back(value);
    }
    if (!input.eof() || (output.values.size() != a && output.values.size() != b)) {
        failure={pair.line,"invalid numeric effect arity"}; return false;
    }
    return true;
}
inline bool PrimitiveKind(std::string_view name, Kind &kind) {
    constexpr const char *names[]{"particle","line","tail","sound","cylinder","electricity",
        "emitter","decal","orientedparticle","fxrunner","light","camerashake","flash"};
    const auto key = Lower(name);
    for (std::size_t n=0;n<13;++n) if (key == names[n]) {kind=static_cast<Kind>(n);return true;}
    return false;
}
inline bool ResourceKind(std::string_view name, MediaKind &kind) {
    if (name=="shader" || name=="shaders") kind=MediaKind::Shader;
    else if (name=="model" || name=="models") kind=MediaKind::Model;
    else if (name=="sound" || name=="sounds") kind=MediaKind::Sound;
    else if (name=="impactfx") kind=MediaKind::Impact;
    else if (name=="deathfx") kind=MediaKind::Death;
    else if (name=="emitfx") kind=MediaKind::Emit;
    else if (name=="playfx") kind=MediaKind::Play;
    else return false;
    return true;
}
// Retail's group parser (jamp.exe 0x40ee34..0x40ef13) reads "width" as size,
// "width2" as size2 and a "height" group as length; each group reads "parms"
// as parm (0x40e1be, 0x40e35b, 0x40e4fb, 0x40e69b, 0x40e83b).
inline std::string CurveGroup(std::string_view name) {
    auto key = Lower(name);
    if (key == "width") key = "size";
    else if (key == "width2") key = "size2";
    else if (key == "height") key = "length";
    return key;
}
inline std::string CurveField(std::string_view name) {
    auto field = Lower(name);
    return field == "parms" ? "parm" : field;
}
inline bool ReadPrimitive(Primitive &primitive, Failure &failure) {
    for (const auto &pair : primitive.declaration.pairs) {
        const auto key = Lower(pair.name);
        MediaKind kind;
        if (ResourceKind(key, kind)) {
            for (const auto &value : pair.values) {
                Name nested;
                if (!SafePath(value) || (kind>=MediaKind::Impact && !ResolveName(value,nested))) {
                    failure={pair.line,"unsafe effect resource path"}; return false;
                }
                primitive.media.push_back({kind,value,0});
            }
            continue;
        }
        const bool scalar = key=="count" || key=="life" || key=="delay" || key=="bounce" || key=="intensity" ||
            key=="gravity" || key=="density" || key=="variance" || key=="radius" || key=="height" || key=="wind" ||
            key=="rotation" || key=="rotationdelta";
        const bool vector = key=="angle" || key=="angles" || key=="angledelta" || key=="velocity" || key=="vel" ||
            key=="acceleration" || key=="accel" || key=="origin" || key=="origin2";
        const bool bounds = key=="min" || key=="max";
        if (scalar || vector || bounds) {
            Numeric number;
            if (!Numbers(pair,scalar?1:3,bounds?3:scalar?2:6,number,failure)) return false;
            primitive.numbers.push_back(std::move(number));
        }
    }
    for (const auto &curve : primitive.declaration.groups) {
        const auto key = CurveGroup(curve.name);
        const bool color = key=="rgb";
        if (!color && key!="alpha" && key!="size" && key!="size2" && key!="length") continue;
        for (const auto &pair : curve.pairs) {
            const auto field = CurveField(pair.name);
            if (field!="start" && field!="end" && field!="parm") continue;
            Numeric number;
            const bool vector = color && field!="parm";
            if (!Numbers(pair,vector?3:1,vector?6:2,number,failure)) return false;
            number.name = key+"/"+field;
            primitive.numbers.push_back(std::move(number));
        }
    }
    return true;
}
inline bool Parse(std::string_view text, Definition &output, Failure &failure) {
    Group root;
    if (!Reader(text,failure).Read(root)) return false;
    Definition candidate;
    if (!root.pairs.empty() && Lower(root.pairs.front().name)=="repeatdelay") {
        const auto &pair=root.pairs.front(); Numeric numeric;
        if (!Numbers(pair,1,1,numeric,failure)) return false;
        const auto value=numeric.values.front();
        if (value < (std::numeric_limits<int>::min)() || value > (std::numeric_limits<int>::max)() || value!=std::trunc(value)) {
            failure={pair.line,"invalid effect repeat delay"};return false;
        }
        candidate.repeatDelay=static_cast<int>(value);
    }
    for (auto &group : root.groups) {
        Kind kind;
        if (!PrimitiveKind(group.name,kind)) {candidate.ignoredGroups.push_back(group.name);continue;}
        if (candidate.primitives.size()==MaxPrimitives) {failure={group.line,"too many effect primitives"};return false;}
        Primitive primitive; primitive.kind=kind; primitive.declaration=std::move(group);
        if (!ReadPrimitive(primitive,failure)) return false;
        candidate.primitives.push_back(std::move(primitive));
    }
    output=std::move(candidate);
    return true;
}
} // namespace fntech3::jka::effects
#endif
